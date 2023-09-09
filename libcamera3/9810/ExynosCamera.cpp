/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCamera"
#include <log/log.h>
#include <ui/Fence.h>

#include "ExynosCamera.h"

namespace android {

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif

ExynosCamera::ExynosCamera(int cameraId,
        camera_metadata_t **info,
#ifndef USE_DUAL_CAMERA
        __unused
#endif
        uint32_t numOfSensors):
    m_requestMgr(NULL),
    m_streamManager(NULL),
    m_activityControl(NULL)
{
    //BUILD_DATE();

    m_cameraOriginId = cameraId;
    if (m_cameraOriginId == CAMERA_ID_SECURE) {
        m_cameraIds[0] = CAMERA_ID_FRONT;
        m_scenario = SCENARIO_SECURE;
    } else {
        m_cameraIds[0] = m_cameraOriginId;
        m_scenario = SCENARIO_NORMAL;
    }

    m_cameraId = m_cameraIds[0];

#ifdef USE_DUAL_CAMERA
    if (numOfSensors >= 2) {
        switch (m_cameraOriginId) {
        case CAMERA_ID_BACK_0:
            m_cameraIds[1] = CAMERA_ID_BACK_1;
            m_scenario = SCENARIO_DUAL_CAMERA;
            break;
        case CAMERA_ID_FRONT_0:
            m_cameraIds[1] = CAMERA_ID_FRONT_1;
            m_scenario = SCENARIO_DUAL_CAMERA;
            break;
        default:
            break;
        }
        CLOGI("Dual cameara enabled! CameraId (%d & %d)", m_cameraIds[0], m_cameraIds[1]);
    }
#endif

    switch (cameraId) {
    case CAMERA_ID_BACK:
        strncpy(m_name, "Back", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_FRONT:
        strncpy(m_name, "Front", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_SECURE:
        strncpy(m_name, "Secure", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        strncpy(m_name, "BackSlave", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
    case CAMERA_ID_FRONT_1:
        strncpy(m_name, "FrontSlave", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        break;
#endif
    default:
        memset(m_name, 0x00, sizeof(m_name));
        CLOGE("Invalid camera ID(%d)", cameraId);
        break;
    }

    /* Initialize pointer variables */
    m_ionAllocator = NULL;

    m_bufferSupplier = NULL;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_parameters[i] = NULL;
        m_captureSelector[i] = NULL;
    }
    m_captureZslSelector = NULL;

    m_vendorSpecificPreConstructor(m_cameraId);

    /* Create related classes */
    switch (m_scenario) {
    case SCENARIO_SECURE:
        m_parameters[m_cameraId] = new ExynosCameraParameters(m_cameraOriginId);
        break;
#ifdef USE_DUAL_CAMERA
    case SCENARIO_DUAL_CAMERA:
        m_parameters[m_cameraIds[1]] = new ExynosCameraParameters(m_cameraIds[1]);
        /* No break: m_parameters[0] is same with normal */
#endif
    case SCENARIO_NORMAL:
    default:
        m_parameters[m_cameraId] = new ExynosCameraParameters(m_cameraId);
        break;
    }

    m_activityControl = m_parameters[m_cameraId]->getActivityControl();

    m_metadataConverter = new ExynosCameraMetadataConverter(m_cameraOriginId, m_parameters[m_cameraId]);
    m_requestMgr = new ExynosCameraRequestManager(m_cameraOriginId, m_parameters[m_cameraId]);
    m_requestMgr->setMetaDataConverter(m_metadataConverter);

    /* Create managers */
    m_createManagers();

    /* Create threads */
    m_createThreads();

    /* Create queue for preview path. If you want to control pipeDone in ExynosCamera, try to create frame queue here */
    m_shotDoneQ = new ExynosCameraList<uint32_t>();
    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        switch (i) {
        case PIPE_FLITE:
        case PIPE_3AA:
        case PIPE_ISP:
        case PIPE_MCSC:
#ifdef SUPPORT_HW_GDC
        case PIPE_GDC:
#endif
            m_pipeFrameDoneQ[i] = new frame_queue_t;
            break;
        default:
            m_pipeFrameDoneQ[i] = NULL;
            break;
        }
    }

    m_pipeFrameDoneQ[PIPE_VRA] = new frame_queue_t(m_previewStreamVRAThread);
#ifdef SUPPORT_HFD
    if (m_parameters[m_cameraId]->getHfdMode() == true) {
        m_pipeFrameDoneQ[PIPE_HFD] = new frame_queue_t(m_previewStreamHFDThread);
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_CAMERA) {
        m_dualShotDoneQ = new ExynosCameraList<uint32_t>();
        m_pipeFrameDoneQ[PIPE_DCP] = new frame_queue_t;
        m_pipeFrameDoneQ[PIPE_SYNC] = new frame_queue_t;
        m_pipeFrameDoneQ[PIPE_FUSION] = new frame_queue_t;
        m_dualTransitionCount = DUAL_TRANSITION_FRAME_COUNT;
    } else {
        m_dualShotDoneQ = NULL;
        m_pipeFrameDoneQ[PIPE_SYNC] = NULL;
        m_pipeFrameDoneQ[PIPE_FUSION] = NULL;
        m_dualTransitionCount = 0;
    }
#endif

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    /* Create queue for capture path */
    m_yuvCaptureDoneQ = new frame_queue_t;
    m_yuvCaptureDoneQ->setWaitTime(2000000000);
    m_reprocessingDoneQ = new frame_queue_t;
    m_reprocessingDoneQ->setWaitTime(2000000000);

    m_shotDoneQ->setWaitTime(4000000000);

    m_frameFactoryQ = new framefactory3_queue_t;
    m_selectBayerQ = new frame_queue_t();
    m_captureQ = new frame_queue_t(m_captureThread);
#ifdef SUPPORT_HW_GDC
    m_gdcQ = new frame_queue_t(m_gdcThread);
#endif

    /* construct static meta data information */
    if (ExynosCameraMetadataConverter::constructStaticInfo(m_cameraOriginId, info))
        CLOGE("Create static meta data failed!!");

    m_metadataConverter->setStaticInfo(m_cameraOriginId, *info);

    m_streamManager->setYuvStreamMaxCount(m_parameters[m_cameraId]->getYuvStreamMaxNum());

    m_setFrameManager();

    m_setConfigInform();

    /* init infomation of fd orientation*/
    m_parameters[m_cameraId]->setDeviceOrientation(0);
    ExynosCameraActivityUCTL *uctlMgr = m_activityControl->getUCTLMgr();
    if (uctlMgr != NULL) {
        uctlMgr->setDeviceRotation(m_parameters[m_cameraId]->getFdOrientation());
    }

    m_state = EXYNOS_CAMERA_STATE_OPEN;

    m_visionFps = 0;

    m_flushLockWait = false;
    m_captureStreamExist = false;
    m_rawStreamExist = false;
    m_videoStreamExist = false;

    m_recordingEnabled = false;
    m_firstRequestFrameNumber = 0;
    m_internalFrameCount = 1;
    m_isNeedRequestFrame = false;
    m_currentPreviewShot = new struct camera2_shot_ext;
    memset(m_currentPreviewShot, 0x00, sizeof(struct camera2_shot_ext));
    m_currentCaptureShot = new struct camera2_shot_ext;
    memset(m_currentCaptureShot, 0x00, sizeof(struct camera2_shot_ext));
#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif
    m_lastFrametime = 0;
#ifdef YUV_DUMP
    m_dumpFrameQ = new frame_queue_t(m_dumpThread);
#endif

    m_ionClient = exynos_ion_open();
    if (m_ionClient < 0) {
        ALOGE("ERR(%s):m_ionClient ion_open() fail", __func__);
        m_ionClient = -1;
    }

    m_needDynamicBayerCount = 0;
    m_flagFirstPreviewTimerOn = false;

    /* Construct vendor */
    m_vendorSpecificConstructor();
}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
#ifdef USE_DUAL_CAMERA
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME - NUM_FUSION_BUFFERS;
#else
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
#endif
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_raw_buffers = NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_callback_buffers = NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_video_buffers = NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_jpeg_buffers = NUM_REQUEST_JPEG_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_batch_buffers = 1;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_PREPARE_COUNT;

    /* Config HIGH_SPEED 60 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers = NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffer for HAL */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_raw_buffers = NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_callback_buffers = NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_video_buffers = NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_jpeg_buffers = NUM_REQUEST_JPEG_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_batch_buffers = 1;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
    /* Config HIGH_SPEED 120 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_sensor_buffers = FPS120_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_3aa_buffers = FPS120_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hwdis_buffers = FPS120_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_raw_buffers = FPS120_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_preview_buffers = FPS120_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_callback_buffers = FPS120_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_video_buffers = FPS120_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_jpeg_buffers = FPS120_NUM_REQUEST_JPEG_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_batch_buffers = 2;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = FPS120_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA] = FPS120_PIPE_3AA_PREPARE_COUNT;

    /* Config HIGH_SPEED 240 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_sensor_buffers = FPS240_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_3aa_buffers = FPS240_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_hwdis_buffers = FPS240_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_raw_buffers = FPS240_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_preview_buffers = FPS240_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_callback_buffers = FPS240_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_video_buffers = FPS240_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_jpeg_buffers = FPS240_NUM_REQUEST_JPEG_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_batch_buffers = 4;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_PREPARE_COUNT;
#endif

    m_parameters[m_cameraId]->setConfig(&exynosConfig);
#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_CAMERA
        && m_parameters[m_cameraIds[1]] != NULL) {
        m_parameters[m_cameraIds[1]]->setConfig(&exynosConfig);
    }
#endif

    m_exynosconfig = m_parameters[m_cameraId]->getConfig();

    return NO_ERROR;
}

status_t ExynosCamera::m_checkStreamInfo(void)
{
    status_t ret = NO_ERROR;
    /* Check PIP Mode Limitation
     * Max number of YUV stream for each device: 1
     * Max size of YUV stream: FHD(1080p)
     * Each limitation is defined at config.h
     */
    if (m_parameters[m_cameraId]->getPIPMode() == true) {
        int yuvStreamCount = m_streamManager->getYuvStreamCount();
        int maxYuvW = 0, maxYuvH = 0;
        m_parameters[m_cameraId]->getMaxYuvSize(&maxYuvW, &maxYuvH);

        if (yuvStreamCount > PIP_CAMERA_MAX_YUV_STREAM
            || maxYuvH > PIP_CAMERA_MAX_YUV_HEIGHT
            || (maxYuvW * maxYuvH) > PIP_CAMERA_MAX_YUV_SIZE) {

            CLOGW("PIP mode NOT support this configuration. " \
                    "YUVStreamCount %d MaxYUVSize %dx%d",
                    yuvStreamCount, maxYuvW, maxYuvH);

            ret = BAD_VALUE;
        }
    }

    return ret;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainPreviewThread = new mainCameraThread(this, &ExynosCamera::m_mainPreviewThreadFunc, "m_mainPreviewThreadFunc");
    CLOGD("Main Preview thread created");

    m_mainCaptureThread = new mainCameraThread(this, &ExynosCamera::m_mainCaptureThreadFunc, "m_mainCaptureThreadFunc");
    CLOGD("Main Capture thread created");

    /* m_previewStreamXXXThread is for seperated frameDone each own handler */
    m_previewStreamBayerThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamBayerPipeThreadFunc, "PreviewBayerThread");
    CLOGD("Bayer Preview stream thread created");

    m_previewStream3AAThread = new mainCameraThread(this, &ExynosCamera::m_previewStream3AAPipeThreadFunc, "Preview3AAThread");
    CLOGD("3AA Preview stream thread created");

    m_previewStreamISPThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamISPPipeThreadFunc, "PreviewISPThread");
    CLOGD("ISP Preview stream thread created");

    m_previewStreamMCSCThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamMCSCPipeThreadFunc, "PreviewMCSCThread");
    CLOGD("MCSC Preview stream thread created");

#ifdef SUPPORT_HW_GDC
    m_previewStreamGDCThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamGDCPipeThreadFunc, "PreviewGDCThread");
    CLOGD("GDC Preview stream thread created");
#endif

    m_previewStreamVRAThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamVRAPipeThreadFunc, "PreviewVRAThread");
    CLOGD("VRA Preview stream thread created");

#ifdef SUPPORT_HFD
    if (m_parameters[m_cameraId]->getHfdMode() == true) {
        m_previewStreamHFDThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamHFDPipeThreadFunc, "PreviewHFDThread");
        CLOGD("HFD Preview stream thread created");
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_CAMERA) {
        m_previewStreamDCPThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamDCPPipeThreadFunc, "PreviewDCPThread");
        CLOGD("DCP Preview stream thread created");

        m_dualMainThread = new mainCameraThread(this, &ExynosCamera::m_dualMainThreadFunc, "m_dualMainThreadFunc");
        CLOGD("Dual Main thread created");

        m_previewStreamSyncThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamSyncPipeThreadFunc, "PreviewSyncThread");
        CLOGD("Sync Preview stream thread created");

        m_previewStreamFusionThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamFusionPipeThreadFunc, "PreviewFusionThread");
        CLOGD("Fusion Preview stream thread created");
    }
#endif

    m_selectBayerThread = new mainCameraThread(this, &ExynosCamera::m_selectBayerThreadFunc, "SelectBayerThreadFunc");
    CLOGD("SelectBayerThread created");

    m_captureThread = new mainCameraThread(this, &ExynosCamera::m_captureThreadFunc, "CaptureThreadFunc");
    CLOGD("CaptureThread created");

    m_captureStreamThread = new mainCameraThread(this, &ExynosCamera::m_captureStreamThreadFunc, "CaptureThread");
    CLOGD("Capture stream thread created");

    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGD("Buffer allocation thread created");

    m_framefactoryCreateThread = new mainCameraThread(this, &ExynosCamera::m_frameFactoryCreateThreadFunc, "FrameFactoryCreateThread");
    CLOGD("FrameFactoryCreateThread created");

    m_reprocessingFrameFactoryStartThread = new mainCameraThread(this, &ExynosCamera::m_reprocessingFrameFactoryStartThreadFunc, "m_reprocessingFrameFactoryStartThread");
    CLOGD("m_reprocessingFrameFactoryStartThread created");

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGD("startPictureBufferThread created");

    m_frameFactoryStartThread = new mainCameraThread(this, &ExynosCamera::m_frameFactoryStartThreadFunc, "FrameFactoryStartThread");
    CLOGD("FrameFactoryStartThread created");

#ifdef SUPPORT_HW_GDC
    m_gdcThread = new mainCameraThread(this, &ExynosCamera::m_gdcThreadFunc, "GDCThread");
    CLOGD("GDCThread created");
#endif

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "MonitorThread");
    CLOGD("MonitorThread created");

#ifdef YUV_DUMP
    m_dumpThread = new mainCameraThread(this, &ExynosCamera::m_dumpThreadFunc, "m_dumpThreadFunc");
    CLOGD("DumpThread created");
#endif

    m_vendorCreateThreads();
}

ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    CLOGI("-IN-");

    m_vendorSpecificPreDestructor();

    if (m_bufferSupplier != NULL) {
        delete m_bufferSupplier;
        m_bufferSupplier = NULL;
    }

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_shotDoneQ != NULL) {
        delete m_shotDoneQ;
        m_shotDoneQ = NULL;
    }

#ifdef USE_DUAL_CAMERA
    if (m_dualShotDoneQ != NULL) {
        delete m_dualShotDoneQ;
        m_dualShotDoneQ = NULL;
    }
#endif

    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            delete m_pipeFrameDoneQ[i];
            m_pipeFrameDoneQ[i] = NULL;
        }
    }

    if (m_yuvCaptureDoneQ != NULL) {
        delete m_yuvCaptureDoneQ;
        m_yuvCaptureDoneQ = NULL;
    }

    if (m_reprocessingDoneQ != NULL) {
        delete m_reprocessingDoneQ;
        m_reprocessingDoneQ = NULL;
    }

    if (m_frameFactoryQ != NULL) {
        delete m_frameFactoryQ;
        m_frameFactoryQ = NULL;
    }

    if (m_selectBayerQ != NULL) {
        delete m_selectBayerQ;
        m_selectBayerQ = NULL;
    }

    if (m_captureQ != NULL) {
        delete m_captureQ;
        m_captureQ = NULL;
    }
#ifdef SUPPORT_HW_GDC
    if (m_gdcQ != NULL) {
        delete m_gdcQ;
        m_gdcQ = NULL;
    }
#endif
#ifdef YUV_DUMP
    if (m_dumpFrameQ != NULL) {
        delete m_dumpFrameQ;
        m_dumpFrameQ = NULL;
    }
#endif

    m_deinitFrameFactory();

    if (m_streamManager != NULL) {
        delete m_streamManager;
        m_streamManager = NULL;
    }

    if (m_requestMgr!= NULL) {
        delete m_requestMgr;
        m_requestMgr = NULL;
    }

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_parameters[i] != NULL) {
            delete m_parameters[i];
            m_parameters[i] = NULL;
        }
    }

    if (m_metadataConverter != NULL) {
        delete m_metadataConverter;
        m_metadataConverter = NULL;
    }

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_captureSelector[i] != NULL) {
            delete m_captureSelector[i];
            m_captureSelector[i] = NULL;
        }
    }

    if (m_captureZslSelector != NULL) {
        delete m_captureZslSelector;
        m_captureZslSelector = NULL;
    }

    if (m_currentPreviewShot != NULL) {
        delete m_currentPreviewShot;
        m_currentPreviewShot = NULL;
    }

    if (m_currentCaptureShot != NULL) {
        delete m_currentCaptureShot;
        m_currentCaptureShot = NULL;
    }

    if (m_ionClient >= 0) {
        exynos_ion_close(m_ionClient);
        m_ionClient = -1;
    }

    // TODO: clean up
    // m_resultBufferVectorSet
    // m_processList
    // m_postProcessList
    // m_pipeFrameDoneQ

    m_vendorSpecificDestructor();

    /* Frame manager must be deleted in the last.
     * Some frame lists can call frame destructor
     * which will try to access frameQueue from frame manager.
     */
    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    CLOGI("-OUT-");
}

status_t ExynosCamera::initializeDevice(const camera3_callback_ops *callbackOps)
{
    status_t ret = NO_ERROR;
    CLOGD("");

    /* set callback ops */
    m_requestMgr->setCallbackOps(callbackOps);

    if (m_parameters[m_cameraId]->isReprocessing() == true) {
        if (m_captureSelector[m_cameraId] == NULL) {
            m_captureSelector[m_cameraId] = new ExynosCameraFrameSelector(m_cameraId, m_parameters[m_cameraId], m_bufferSupplier, m_frameMgr);
            ret = m_captureSelector[m_cameraId]->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureSelector[%d] setFrameHoldCount(%d) is fail",
                     m_cameraId, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true
            && m_captureSelector[m_cameraIds[1]] == NULL) {
            m_captureSelector[m_cameraIds[1]] = new ExynosCameraFrameSelector(m_cameraIds[1], m_parameters[m_cameraIds[1]], m_bufferSupplier, m_frameMgr);
            ret = m_captureSelector[m_cameraIds[1]]->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureSelector[%d] setFrameHoldCount(%d) is fail",
                     m_cameraIds[1], REPROCESSING_BAYER_HOLD_COUNT);
            }
        }
#endif

        if (m_captureZslSelector == NULL) {
            m_captureZslSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters[m_cameraId], m_bufferSupplier, m_frameMgr);
            ret = m_captureZslSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureZslSelector setFrameHoldCount(%d) is fail",
                     REPROCESSING_BAYER_HOLD_COUNT);
            }
        }
    }

    m_frameMgr->start();

    ret = m_transitState(EXYNOS_CAMERA_STATE_INITIALIZE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into INITIALIZE. ret %d", ret);
    }

    return ret;
}

status_t ExynosCamera::construct_default_request_settings(camera_metadata_t **request, int type)
{
    CLOGD("Type %d", type);
    if ((type < 0) || (type >= CAMERA3_TEMPLATE_COUNT)) {
        CLOGE("Unknown request settings template: %d", type);
        return -ENODEV;
    }

    m_requestMgr->constructDefaultRequestSettings(type, request);

    CLOGI("out");
    return NO_ERROR;
}

void ExynosCamera::get_metadata_vendor_tag_ops(const camera3_device_t *, vendor_tag_query_ops_t *ops)
{
    if (ops == NULL) {
        CLOGE("ops is NULL");
        return;
    }
}

void ExynosCamera::dump()
{
    ExynosCameraFrameFactory *factory = NULL;

    CLOGI("");

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    if (factory == NULL) {
        CLOGE("frameFactory is NULL");
        return;
    }
    m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->dump();
    m_bufferSupplier->dump();
}

int ExynosCamera::getCameraId() const
{
    return m_cameraId;
}

int ExynosCamera::getCameraIdOrigin() const
{
    return m_cameraOriginId;
}

status_t ExynosCamera::setPIPMode(bool enabled)
{
    if (m_parameters[m_cameraId] == NULL) {
        CLOGE("m_parameters[m_cameraId] is NULL");
        return INVALID_OPERATION;
    }

    m_parameters[m_cameraId]->setPIPMode(enabled);

    return NO_ERROR;
}

bool ExynosCamera::getAvailablePIPMode(void)
{
    exynos_camera_state_t state = m_getState();

    if (state == EXYNOS_CAMERA_STATE_CONFIGURED
        || state == EXYNOS_CAMERA_STATE_START
        || state == EXYNOS_CAMERA_STATE_RUN) {
        return false;
    } else {
        return true;
    }
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::setDualMode(bool enabled)
{
    if (m_parameters[m_cameraId] == NULL) {
        CLOGE("m_parameters[m_cameraId] is NULL");
        return INVALID_OPERATION;
    }

    m_parameters[m_cameraId]->setDualMode(enabled);
    if (m_parameters[m_cameraIds[1]] != NULL) {
        m_parameters[m_cameraIds[1]]->setDualMode(enabled);
    }

    return NO_ERROR;
}

bool ExynosCamera::getDualMode(void)
{
    if (m_parameters[m_cameraId] == NULL) {
        CLOGE("m_parameters[m_cameraId] is NULL");
        return false;
    }

    return m_parameters[m_cameraId]->getDualMode();
}
#endif

bool ExynosCamera::m_mainPreviewThreadFunc(void)
{
    status_t ret = NO_ERROR;

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory");
        usleep(1);

        return true;
    }

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        ret = m_createPreviewFrameFunc(REQ_SYNC_WITH_3AA);
    } else {
        ret = m_createVisionFrameFunc(REQ_SYNC_WITH_3AA);
    }
    if (ret != NO_ERROR) {
        CLOGE("Failed to createPreviewFrameFunc. Shotdone");
    }

    return true;
}

bool ExynosCamera::m_mainCaptureThreadFunc(void)
{
    status_t ret = NO_ERROR;

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory");
        usleep(1);

        return true;
    }

    ret = m_createCaptureFrameFunc();
    if (ret != NO_ERROR) {
        CLOGE("Failed to createCaptureFrameFunc. Shotdone");
    }

    if (m_getSizeFromRequestList(&m_requestCaptureWaitingList, &m_requestCaptureWaitingLock) > 0) {
        return true;
    } else {
        return false;
    }
}

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_dualMainThreadFunc(void)
{
    uint32_t frameCount = 0;
    status_t ret = NO_ERROR;

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory");
        usleep(1);

        return true;
    }

    /* 1. Wait the shot done with the latest framecount */
    ret = m_dualShotDoneQ->waitAndPopProcessQ(&frameCount);
    if (ret < 0) {
        if (ret == TIMED_OUT)
            CLOGW("wait timeout");
        else
            CLOGE("wait and pop fail, ret(%d)", ret);
        return true;
    }

    switch (m_parameters[m_cameraId]->getDualOperationMode()) {
    case DUAL_OPERATION_MODE_MASTER:
        if (m_parameters[m_cameraId]->isSupportSlaveSensorStandby() == false) {
            break;
        }
        return false;
    case DUAL_OPERATION_MODE_SLAVE:
        if (m_parameters[m_cameraId]->isSupportMasterSensorStandby() == false) {
            break;
        }
        return false;
    case DUAL_OPERATION_MODE_SYNC:
        return false;
    }

    ret = m_createInternalFrameFunc(NULL, REQ_SYNC_NONE, FRAME_TYPE_TRANSITION);
    if (ret != NO_ERROR)
        CLOGE("Failed to createFrameFunc. Shotdone");

    return true;
}
#endif

status_t ExynosCamera::m_createPreviewFrameFunc(enum Request_Sync_Type syncType)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    FrameFactoryList previewFactoryAddrList;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;
    ExynosCameraFrameFactory *factory = NULL;
    frame_type_t frameType = FRAME_TYPE_PREVIEW;
#ifdef USE_DUAL_CAMERA
    ExynosCameraFrameFactory *subFactory = NULL;
    frame_type_t subFrameType = FRAME_TYPE_PREVIEW;
#endif
    uint32_t waitingListSize;
    uint32_t requiredRequestCount = -1;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::[R%d] generate request frame", request->getKey());
#endif

    m_waitShotDone(syncType);
    m_captureResultDoneCondition.signal();

    /* 1. Update the current shot */
    {
        Mutex::Autolock l(m_requestPreviewWaitingLock);

        enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
        waitingListSize = m_requestPreviewWaitingList.size();
        if (m_parameters[m_cameraId]->useServiceBatchMode() == true) {
            requiredRequestCount = 1;
        } else {
            requiredRequestCount = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
        }

        if (waitingListSize < requiredRequestCount) {
            m_isNeedRequestFrame = false;
        } else {
            m_isNeedRequestFrame = true;

            r = m_requestPreviewWaitingList.begin();
            request = *r;

            /* Update the entire shot_ext structure */
            service_shot_ext = request->getServiceShot();
            if (service_shot_ext == NULL) {
                CLOGE("[R%d] Get service shot is failed", request->getKey());
            } else {
                memcpy(m_currentPreviewShot, service_shot_ext, sizeof(struct camera2_shot_ext));
            }

    /* 2. Initial (SENSOR_CONTROL_DELAY + 1) frames must be internal frame to
     * secure frame margin for sensor control.
     * If sensor control delay is 0, every initial frames must be request frame.
     */
            if (m_parameters[m_cameraId]->getSensorControlDelay() > 0
                && m_internalFrameCount < (m_parameters[m_cameraId]->getSensorControlDelay() + 2)) {
                m_isNeedRequestFrame = false;
                request = NULL;
            } else {
                m_requestPreviewWaitingList.erase(r);
            }
        }
    }

    CLOGV("Create New Frame %d needRequestFrame %d waitingSize %d",
            m_internalFrameCount, m_isNeedRequestFrame, waitingListSize);

    /* 3. Select the frame creation logic between request frame and internal frame */
    if (m_isNeedRequestFrame == true) {
#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true) {
            m_checkDualOperationMode(request);
            if (ret != NO_ERROR) {
                CLOGE("m_checkDualOperationMode fail! ret(%d)", ret);
            }
        }
#endif

        previewFactoryAddrList.clear();
        request->getFactoryAddrList(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, &previewFactoryAddrList);
        m_parameters[m_cameraId]->updateMetaParameter(request->getMetaParameters());

        /* Call the frame create handler for each frame factory */
        /* Frame creation handler calling sequence is ordered in accordance with
           its frame factory type, because there are dependencies between frame
           processing routine.
         */

        if (previewFactoryAddrList.empty() == false) {
            ExynosCameraRequestSP_sprt_t serviceRequest = NULL;
            m_popServiceRequest(serviceRequest);
            serviceRequest = NULL;

            for (factorylistIter = previewFactoryAddrList.begin(); factorylistIter != previewFactoryAddrList.end(); ) {
                factory = *factorylistIter;
                CLOGV("frameFactory (%p)", factory);

                switch(factory->getFactoryType()) {
                    case FRAME_FACTORY_TYPE_CAPTURE_PREVIEW:
                        frameType = FRAME_TYPE_PREVIEW;
                        break;
                    case FRAME_FACTORY_TYPE_REPROCESSING:
                        frameType = FRAME_TYPE_REPROCESSING;
                        break;
                    case FRAME_FACTORY_TYPE_VISION:
                        frameType = FRAME_TYPE_VISION;
                        break;
                    default:
                        CLOGE("[R%d] Factory type is not available", request->getKey());
                        break;
                }
#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true) {
                    enum DUAL_OPERATION_MODE dualOperationMode = m_parameters[m_cameraId]->getDualOperationMode();
                    switch(factory->getFactoryType()) {
                    case FRAME_FACTORY_TYPE_CAPTURE_PREVIEW:
                        if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                            subFrameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
                            subFactory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                            frameCreateHandler = subFactory->getFrameCreateHandler();
                            (this->*frameCreateHandler)(request, subFactory, subFrameType);

                            frameType = FRAME_TYPE_PREVIEW_DUAL_MASTER;
                        } else if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                            frameType = FRAME_TYPE_PREVIEW_SLAVE;
                        }
                        break;
                    case FRAME_FACTORY_TYPE_REPROCESSING:
                        if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                            subFrameType = FRAME_TYPE_REPROCESSING_DUAL_SLAVE;
                            subFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
                            frameCreateHandler = subFactory->getFrameCreateHandler();
                            (this->*frameCreateHandler)(request, subFactory, subFrameType);

                            frameType = FRAME_TYPE_REPROCESSING_DUAL_MASTER;
                        } else if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                            factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
                        }
                        break;
                    default:
                        CLOGE("[R%d] Factory type is not available", request->getKey());
                        break;
                    }
                }
#endif
                frameCreateHandler = factory->getFrameCreateHandler();
                (this->*frameCreateHandler)(request, factory, frameType);

                factorylistIter++;
            }
        } else {
            m_createInternalFrameFunc(request, syncType);
#ifdef USE_DUAL_CAMERA
            if (m_parameters[m_cameraId]->getDualMode() == true
                && m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                m_createInternalFrameFunc(request, syncType, FRAME_TYPE_PREVIEW_DUAL_SLAVE);
            }
#endif
        }
    } else {
        m_createInternalFrameFunc(NULL, syncType);
#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true
            && m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
            m_createInternalFrameFunc(NULL, syncType, FRAME_TYPE_PREVIEW_DUAL_SLAVE);
        }
#endif
    }

    previewFactoryAddrList.clear();

    return ret;
}

status_t ExynosCamera::m_createVisionFrameFunc(enum Request_Sync_Type syncType)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    FrameFactoryList visionFactoryAddrList;
    ExynosCameraFrameFactory *factory = NULL;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;
    frame_type_t frameType = FRAME_TYPE_VISION;
    uint32_t watingListSize = 0;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::[R%d] generate request frame", request->getKey());
#endif

    m_waitShotDone(syncType);
    m_captureResultDoneCondition.signal();

    /* 1. Update the current shot */
    {
        Mutex::Autolock l(m_requestPreviewWaitingLock);

        watingListSize = m_requestPreviewWaitingList.size();
        if (m_requestPreviewWaitingList.empty() == true) {
            m_isNeedRequestFrame = false;
        } else {
            m_isNeedRequestFrame = true;

            r = m_requestPreviewWaitingList.begin();
            request = *r;

            /* Update the entire shot_ext structure */
            service_shot_ext = request->getServiceShot();
            if (service_shot_ext == NULL) {
                CLOGE("[R%d] Get service shot is failed", request->getKey());
            } else {
                memcpy(m_currentPreviewShot, service_shot_ext, sizeof(struct camera2_shot_ext));
            }

            /* 2. Initial (SENSOR_CONTROL_DELAY + 1) frames must be internal frame to
             * secure frame margin for sensor control.
             * If sensor control delay is 0, every initial frames must be request frame.
             */
            if (m_parameters[m_cameraId]->getSensorControlDelay() > 0
                && m_internalFrameCount < (m_parameters[m_cameraId]->getSensorControlDelay() + 2)) {
                m_isNeedRequestFrame = false;
                request = NULL;
            } else {
                m_requestPreviewWaitingList.erase(r);
            }
        }
    }

    CLOGV("Create New Frame %d needRequestFrame %d waitingSize %d",
            m_internalFrameCount, m_isNeedRequestFrame, watingListSize);

    /* 3. Select the frame creation logic between request frame and internal frame */
    if (m_isNeedRequestFrame == true) {
        visionFactoryAddrList.clear();
        request->getFactoryAddrList(FRAME_FACTORY_TYPE_VISION, &visionFactoryAddrList);

        /* Call the frame create handler fro each frame factory */
        /* Frame createion handler calling sequence is ordered in accordance with
           its frame factory type, because there are dependencies between frame
           processing routine.
         */

        if (visionFactoryAddrList.empty() == false) {
            ExynosCameraRequestSP_sprt_t serviceRequest = NULL;
            m_popServiceRequest(serviceRequest);
            serviceRequest = NULL;

            for (factorylistIter = visionFactoryAddrList.begin(); factorylistIter != visionFactoryAddrList.end(); ) {
                factory = *factorylistIter;
                CLOGV("frameFactory (%p)", factory);

                switch(factory->getFactoryType()) {
                    case FRAME_FACTORY_TYPE_CAPTURE_PREVIEW:
                        frameType = FRAME_TYPE_PREVIEW;
                        break;
                    case FRAME_FACTORY_TYPE_REPROCESSING:
                        frameType = FRAME_TYPE_REPROCESSING;
                        break;
                    case FRAME_FACTORY_TYPE_VISION:
                        frameType = FRAME_TYPE_VISION;
                        break;
                    default:
                        CLOGE("[R%d] Factory type is not available", request->getKey());
                        break;
                }
                frameCreateHandler = factory->getFrameCreateHandler();
                (this->*frameCreateHandler)(request, factory, frameType);

                factorylistIter++;
            }
        } else {
            m_createInternalFrameFunc(request, syncType);
        }
    } else {
        m_createInternalFrameFunc(NULL, syncType);
    }

    visionFactoryAddrList.clear();

    return ret;
}

status_t ExynosCamera::m_waitShotDone(enum Request_Sync_Type syncType)
{
    status_t ret = NO_ERROR;
    uint32_t frameCount = 0;

    if (syncType == REQ_SYNC_WITH_3AA && m_shotDoneQ != NULL) {
        /* 1. Wait the shot done with the latest framecount */
        ret = m_shotDoneQ->waitAndPopProcessQ(&frameCount);
        if (ret < 0) {
            if (ret == TIMED_OUT)
                CLOGW("wait timeout");
            else
                CLOGE("wait and pop fail, ret(%d)", ret);
            return ret;
        }
    }

    return ret;
}

status_t ExynosCamera::m_sendVisionStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;

    /* 1. Get stream object for VISION */
    ret = m_streamManager->getStream(HAL_STREAM_ID_VISION, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_VISION");
        return ret;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
                                        EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId HAL_STREAM_ID_VISION",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId HAL_STREAM_ID_VISION",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId HAL_STREAM_ID_VISION",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&(streamBuffer->stream));
    if (ret < 0) {
        CLOGE("getStream is failed, from ExynosCameraStream. Id error:HAL_STREAM_ID_VISION");
        return ret;
    }

    /* 3. Get the bayer buffer from frame */
    streamBuffer->buffer = buffer->handle[0];

    /* 4. Get the service buffer handle from buffer manager */
    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d]Failed to checkStreamBufferStatus.",
            request->getKey(), request->getFrameCount(),
            HAL_STREAM_ID_VISION);
        return ret;
    }

    /* 5. Update the remained buffer info */
    streamBuffer->acquire_fence = -1;
    streamBuffer->release_fence = -1;

    /* construct result for service */
    requestResult->frame_number = request->getKey();
    requestResult->result = NULL;
    requestResult->num_output_buffers = 1;
    requestResult->output_buffers = streamBuffer;
    requestResult->input_buffer = request->getInputBuffer();
    requestResult->partial_result = 0;

    CLOGV("frame number(%d), #out(%d)",
            requestResult->frame_number, requestResult->num_output_buffers);

    /* 6. Request to callback the result to request manager */
    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    CLOGV("request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            "request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(), request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(), request->getFrameCount());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer->stream, streamBuffer->buffer);

    return ret;
}

status_t ExynosCamera::m_sendJpegStreamResult(ExynosCameraRequestSP_sprt_t request,
                                               ExynosCameraBuffer *buffer, int size)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;

    ret = m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from StreamMgr. streamId HAL_STREAM_ID_JPEG");
        return ret;
    }

    if (stream == NULL) {
        CLOGE("stream is NULL");
        return INVALID_OPERATION;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
                                        EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    ret = stream->getStream(&(streamBuffer->stream));
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from ExynosCameraStream. streamId HAL_STREAM_ID_JPEG");
        return ret;
    }

    streamBuffer->buffer = buffer->handle[0];

    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
            request->getKey(), request->getFrameCount(),
            HAL_STREAM_ID_JPEG, buffer->index);
        return ret;
    }

    streamBuffer->acquire_fence = -1;
    streamBuffer->release_fence = -1;

    camera3_jpeg_blob_t jpeg_bolb;
    jpeg_bolb.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    jpeg_bolb.jpeg_size = size;
    int32_t jpegBufferSize = ((private_handle_t *)(*(streamBuffer->buffer)))->width;
    memcpy(buffer->addr[0] + jpegBufferSize - sizeof(camera3_jpeg_blob_t), &jpeg_bolb, sizeof(camera3_jpeg_blob_t));

    /* update jpeg size */
    request->setRequestLock();
    CameraMetadata *setting = request->getServiceMeta();
    int32_t jpegsize = size;
    ret = setting->update(ANDROID_JPEG_SIZE, &jpegsize, 1);
    if (ret < 0) {
        CLOGE("ANDROID_JPEG_SIZE update failed(%d)", ret);
    }
    request->setRequestUnlock();

    CLOGD("Set JPEG result Done. frameCount %d", request->getFrameCount());

    /* construct result for service */
    requestResult->frame_number = request->getKey();
    requestResult->result = NULL;
    requestResult->num_output_buffers = 1;
    requestResult->output_buffers = streamBuffer;
    requestResult->input_buffer = request->getInputBuffer();
    requestResult->partial_result = 0;

    CLOGV("frame number(%d), #out(%d)",
            requestResult->frame_number, requestResult->num_output_buffers);

    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    return ret;
}

status_t ExynosCamera::m_sendRawStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;

    /* 1. Get stream object for RAW */
    ret = m_streamManager->getStream(HAL_STREAM_ID_RAW, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_RAW");
        return ret;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
            EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId HAL_STREAM_ID_RAW",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId HAL_STREAM_ID_RAW",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId HAL_STREAM_ID_RAW",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&(streamBuffer->stream));
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_RAW");
        return ret;
    }

    /* 3. Get the service buffer handle */
    streamBuffer->buffer = buffer->handle[0];

    /* 4. Update the remained buffer info */
    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
            request->getKey(), request->getFrameCount(),
            HAL_STREAM_ID_RAW, buffer->index);
        return ret;
    }

    streamBuffer->acquire_fence = -1;
    streamBuffer->release_fence = -1;

    /* 5. Create new result for RAW buffer */
    requestResult->frame_number = request->getKey();
    requestResult->result = NULL;
    requestResult->num_output_buffers = 1;
    requestResult->output_buffers = streamBuffer;
    requestResult->input_buffer = NULL;
    requestResult->partial_result = 0;

    CLOGV("frame number(%d), #out(%d)", requestResult->frame_number, requestResult->num_output_buffers);

    /* 6. Request to callback the result to request manager */
    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    CLOGV("request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            " request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            request->getFrameCount());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer->stream, streamBuffer->buffer);

    return ret;
}

status_t ExynosCamera::m_sendZslStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;
    List<int> *outputStreamList = NULL;

    /* 1. Get stream object for ZSL */
    /* Refer to the ZSL stream ID requested by the service request */
    request->getAllRequestOutputStreams(&outputStreamList);
    for (List<int>::iterator iter = outputStreamList->begin(); iter != outputStreamList->end(); iter++) {
        if ((*iter % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_ZSL_OUTPUT) {
            CLOGV("[R%d F%d] request ZSL Stream ID %d",
                    request->getKey(), request->getFrameCount(), *iter);
            ret = m_streamManager->getStream(*iter, &stream);
            if (ret < 0) {
                CLOGE("getStream is failed, from streammanager. Id error:HAL_STREAM_ID_ZSL");
                return ret;
            }
            break;
        }
    }

    if (stream == NULL) {
        CLOGE("can not find the ZSL stream ID in the current request");
        return NAME_NOT_FOUND;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
            EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId HAL_STREAM_ID_ZSL",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId HAL_STREAM_ID_ZSL",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId HAL_STREAM_ID_ZSL",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&(streamBuffer->stream));
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:HAL_STREAM_ID_ZSL");
        return ret;
    }

    /* 3. Get zsl buffer */
    streamBuffer->buffer = buffer->handle[0];

    /* 4. Update the remained buffer info */
    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d]Failed to checkStreamBufferStatus.",
            request->getKey(), request->getFrameCount(),
            HAL_STREAM_ID_ZSL_OUTPUT);
        return ret;
    }

    streamBuffer->acquire_fence = -1;
    streamBuffer->release_fence = -1;

    /* construct result for service */
    requestResult->frame_number = request->getKey();
    requestResult->result = NULL;
    requestResult->num_output_buffers = 1;
    requestResult->output_buffers = streamBuffer;
    requestResult->input_buffer = request->getInputBuffer();
    requestResult->partial_result = 0;

    CLOGV("frame number(%d), #out(%d)",
            requestResult->frame_number, requestResult->num_output_buffers);

    /* 6. Request to callback the result to request manager */
    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    CLOGV("request->frame_number(%d), request->getNumOfOutputBuffer(%d)"
            "request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(), request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(), request->getFrameCount());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer->stream, streamBuffer->buffer);

    return ret;

}

status_t ExynosCamera::m_sendMeta(ExynosCameraRequestSP_sprt_t request, EXYNOS_REQUEST_RESULT::TYPE type)
{
    ResultRequest resultRequest = NULL;
    uint32_t frameNumber = 0;
    camera3_capture_result_t *requestResult = NULL;
    CameraMetadata resultMeta;
    status_t ret = OK;

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(), type);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. ALL_META",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. ALL_META",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    frameNumber = request->getKey();
    request->setRequestLock();
    request->updatePipelineDepth();
    resultMeta = *(request->getServiceMeta());
    request->setRequestUnlock();

    requestResult->frame_number = frameNumber;
    requestResult->result = resultMeta.release();
    requestResult->num_output_buffers = 0;
    requestResult->output_buffers = NULL;
    requestResult->input_buffer = NULL;
    requestResult->partial_result = 2;

    CLOGV("framecount %d request %d", request->getFrameCount(), request->getKey());

    m_requestMgr->pushResultRequest(resultRequest);

    return ret;
}


status_t ExynosCamera::m_sendNotifyShutter(ExynosCameraRequestSP_sprt_t request, uint64_t timeStamp)
{
    camera3_notify_msg_t *notify = NULL;
    ResultRequest resultRequest = NULL;
    uint32_t requestKey = 0;

    requestKey = request->getKey();
    if (timeStamp <= 0) {
        timeStamp = request->getSensorTimestamp();
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(), EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. Notify Callback",
                request->getKey(), request->getFrameCount());
        return INVALID_OPERATION;
    }

    notify = resultRequest->getNotifyMsg();
    if (notify == NULL) {
        CLOGE("[R%d F%d] getNotifyResult fail. Notify Callback",
                request->getKey(), request->getFrameCount());
        return INVALID_OPERATION;
    }

    CLOGV("[R%d] SHUTTER frame t(%llu)", requestKey, (unsigned long long)timeStamp);

    notify->type = CAMERA3_MSG_SHUTTER;
    notify->message.shutter.frame_number = requestKey;
    notify->message.shutter.timestamp = timeStamp;

    m_requestMgr->pushResultRequest(resultRequest);

    return OK;
}

status_t ExynosCamera::m_sendNotifyError(ExynosCameraRequestSP_sprt_t request,
                                          EXYNOS_REQUEST_RESULT::ERROR resultError,
                                          camera3_stream_t *stream)
{
    ResultRequest resultRequest = NULL;
    camera3_notify_msg_t *notify = NULL;
    ExynosCameraStream *streamInfo = NULL;
    EXYNOS_REQUEST_RESULT::TYPE cbType = EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ERROR;
    int streamId = 0;
    uint32_t requestKey = 0;

    if (request->getRequestState() == EXYNOS_REQUEST::STATE_ERROR) {
        CLOGV("[F%d] already send notify Error by ERROR_REQUEST", request->getFrameCount());
        return OK;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(), cbType);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. Notify Callback",
                request->getKey(), request->getFrameCount());
        return INVALID_OPERATION;
    }

    requestKey = request->getKey();
    notify = resultRequest->getNotifyMsg();
    if (notify == NULL) {
        CLOGE("[R%d F%d] getNotifyResult fail. Notify Callback",
                requestKey, request->getFrameCount());
        return INVALID_OPERATION;
    }

    notify->type = CAMERA3_MSG_ERROR;
    notify->message.error.frame_number = requestKey;

    switch (resultError) {
    case EXYNOS_REQUEST_RESULT::ERROR_REQUEST:
        CLOGE("[R%d F%d] REQUEST ERROR", requestKey, request->getFrameCount());
        notify->message.error.error_code = CAMERA3_MSG_ERROR_REQUEST;

        /* The error state of the request is set only if the error message is ERROR_REQUEST. */
        request->setRequestState(EXYNOS_REQUEST::STATE_ERROR);
        break;
    case EXYNOS_REQUEST_RESULT::ERROR_RESULT:
        CLOGE("[R%d F%d] RESULT ERROR", requestKey, request->getFrameCount());
        notify->message.error.error_code = CAMERA3_MSG_ERROR_RESULT;
        break;
    case EXYNOS_REQUEST_RESULT::ERROR_BUFFER:
        if (stream == NULL) {
            CLOGE("[R%d F%d] BUFFER ERROR. But stream value is NULL", requestKey, request->getFrameCount());
        } else {
            streamInfo = static_cast<ExynosCameraStream*>(stream->priv);
            streamInfo->getID(&streamId);
            CLOGE("[R%d F%d S%d] BUFFER ERROR",
                    requestKey, request->getFrameCount(), streamId);
        }
        notify->message.error.error_code = CAMERA3_MSG_ERROR_BUFFER;
        notify->message.error.error_stream = stream;
        break;
    case EXYNOS_REQUEST_RESULT::ERROR_UNKNOWN:
        CLOGE("[R%d F%d] UNKNOWN ERROR", requestKey, request->getFrameCount());
        return INVALID_OPERATION;
    }

    m_requestMgr->pushResultRequest(resultRequest);

    return OK;
}

status_t ExynosCamera::m_searchFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, uint32_t frameCount, ExynosCameraFrameSP_dptr_t frame)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGD("list is empty");
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("curFrame is empty");
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            CLOGV("frame count match: expected(%d)", frameCount);
            frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    CLOGV("Cannot find match frame, frameCount(%d)", frameCount);

    return OK;
}

status_t ExynosCamera::m_removeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock, ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    uint32_t frameUniqueKey = 0;
    uint32_t curFrameUniqueKey = 0;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    Mutex::Autolock l(listLock);
    if (list->empty()) {
        CLOGE("list is empty");
        return INVALID_OPERATION;
    }

    frameCount = frame->getFrameCount();
    frameUniqueKey = frame->getUniqueKey();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("curFrame is empty");
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        curFrameUniqueKey = curFrame->getUniqueKey();
        if (frameCount == curFrameCount
            && frameUniqueKey == curFrameUniqueKey) {
            CLOGV("frame count match: expected(%d, %u), current(%d, %u)",
                 frameCount, frameUniqueKey, curFrameCount, curFrameUniqueKey);
            list->erase(r);
            return NO_ERROR;
        }

        if (frameCount != curFrameCount
            && frameUniqueKey != curFrameUniqueKey) {
#ifndef USE_DUAL_CAMERA // HACK: Remove!
            CLOGW("frame count mismatch: expected(%d, %u), current(%d, %u)",
                 frameCount, frameUniqueKey, curFrameCount, curFrameUniqueKey);
            /* curFrame->printEntity(); */
            curFrame->printNotDoneEntity();
#endif
        }
        r++;
    } while (r != list->end());

#ifdef USE_DUAL_CAMERA // HACK: Remove!
    return NO_ERROR;
#else
    CLOGE("[F%d U%d]Cannot find match frame!!!", frameCount, frameUniqueKey);

    return INVALID_OPERATION;
#endif
}

uint32_t ExynosCamera::m_getSizeFromFrameList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    uint32_t count = 0;
    Mutex::Autolock l(listLock);

    count = list->size();

    return count;
}

status_t ExynosCamera::m_clearList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("remaining frame(%zu), we remove them all", list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGV("remove frame count(%d)", curFrame->getFrameCount());
            curFrame = NULL;
        }
        list->erase(r);
    }

    return OK;
}

status_t ExynosCamera::m_removeInternalFrames(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *listLock)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("remaining frame(%zu), we remove internal frames",
             list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
                CLOGV("remove internal frame(%d)",
                         curFrame->getFrameCount());
                m_releaseInternalFrame(curFrame);
            } else {
                CLOGW("frame(%d) is NOT internal frame and will be remained in List",
                         curFrame->getFrameCount());
            }
        }
        list->erase(r);
        curFrame = NULL;
    }

    return OK;
}

status_t ExynosCamera::m_releaseInternalFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }
    if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
        CLOGE("frame(%d) is NOT internal frame",
                 frame->getFrameCount());
        return BAD_VALUE;
    }

    /* Return bayer buffer */
    if (frame->getRequest(PIPE_VC0) == true) {
        int pipeId = m_getBayerPipeId();
        int dstPos = factory->getNodeType(PIPE_VC0);

        ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer(pipeId(%d), dstPos(%d)) fail", pipeId, dstPos);
        } else if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for VC0. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
    }

    /* Return 3AS buffer */
    ret = frame->getSrcBuffer(PIPE_3AA, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer failed. PIPE_3AA, ret(%d)", ret);
    } else if (buffer.index >= 0) {
        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for 3AS. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
        }
    }

    /* Return 3AP buffer */
    if (frame->getRequest(PIPE_3AP) == true) {
        ret = frame->getDstBuffer(PIPE_3AA, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer failed. PIPE_3AA, ret %d",
                     ret);
        } else if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for 3AP. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
    }

    frame = NULL;

    return ret;
}

uint32_t ExynosCamera::m_getSizeFromRequestList(List<ExynosCameraRequestSP_sprt_t> *list, Mutex *listLock)
{
    uint32_t count = 0;
    Mutex::Autolock l(listLock);

    count = list->size();

    return count;
}

status_t ExynosCamera::m_clearRequestList(List<ExynosCameraRequestSP_sprt_t> *list, Mutex *listLock)
{
    ExynosCameraRequestSP_sprt_t curRequest = NULL;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;

    CLOGD("remaining request(%zu), we remove them all", list->size());

    Mutex::Autolock l(listLock);
    while (!list->empty()) {
        r = list->begin()++;
        curRequest = *r;
        if (curRequest != NULL) {
            CLOGV("remove request key(%d), fcount(%d)",
                    curRequest->getKey(), curRequest->getFrameCount());
            curRequest = NULL;
        }
        list->erase(r);
    }

    return OK;
}

status_t ExynosCamera::m_checkStreamBufferStatus(ExynosCameraRequestSP_sprt_t request, ExynosCameraStream *stream,
                                                  int *status, bool bufferSkip)
{
    status_t ret = NO_ERROR;
    int streamId = 0;
    camera3_stream_t *serviceStream = NULL;

    if (stream == NULL) {
        CLOGE("stream is NULL");
        return BAD_VALUE;
    }

    stream->getID(&streamId);

    if (request->getRequestState() == EXYNOS_REQUEST::STATE_ERROR) {
        /* If the request state is error, the buffer status is always error. */
        request->setStreamBufferStatus(streamId, CAMERA3_BUFFER_STATUS_ERROR);
    } else {
        if (request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_ERROR) {
            ret = stream->getStream(&serviceStream);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getStream from ExynosCameraStream. streamId(%d)", streamId);
                return ret;
            }

            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_BUFFER, serviceStream);
            if (ret != NO_ERROR) {
                CLOGE("[F%d R%d S%d] sendNotifyError fail. ret %d",
                        request->getFrameCount(), request->getKey(), streamId, ret);
                return ret;
            }
        } else {
            if (bufferSkip == true) {
                /* The state of the buffer is normal,
                 * but if you want to skip the buffer, set the buffer status to ERROR.
                 * NotifyError is not given. */
                request->setStreamBufferStatus(streamId, CAMERA3_BUFFER_STATUS_ERROR);
            }
        }
    }

    *status = request->getStreamBufferStatus(streamId);

    ALOGV("[R%d F%d S%d] buffer status(%d)",
            request->getKey(), request->getFrameCount(), streamId, *status);

    return ret;
}

void ExynosCamera::m_checkUpdateResult(ExynosCameraFrameSP_sptr_t frame, uint32_t streamConfigBit)
{
    enum FRAME_TYPE frameType;
    bool flag = false;
    uint32_t targetBit = 0;

    frameType = (enum FRAME_TYPE) frame->getFrameType();
    switch (frameType) {
        case FRAME_TYPE_PREVIEW:
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_PREVIEW_SLAVE:
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
        case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
#endif
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_PREVIEW);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_VIDEO);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_CALLBACK);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_ZSL_OUTPUT);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_DEPTHMAP);
            flag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;

            targetBit = 0;
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_RAW);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_ZSL_INPUT);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_JPEG);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_CALLBACK_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_DEPTHMAP_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_THUMBNAIL_CALLBACK);
            flag &= (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) == 0) ? true : false;
            break;
        case FRAME_TYPE_REPROCESSING:
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
        case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
#endif
            targetBit = 0;
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_RAW);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_ZSL_INPUT);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_JPEG);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_CALLBACK_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_DEPTHMAP_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_THUMBNAIL_CALLBACK);
            flag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;
            break;
        case FRAME_TYPE_VISION:
            targetBit = 0;
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_VISION);
            flag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;
            break;
        default:
            CLOGE("[F%d]Unsupported frame type %d", frame->getFrameCount(), frameType);
            break;
    }

    frame->setUpdateResult(flag);

    CLOGV("[F%d T%d]%s Result. streamConfig %x",
            frame->getFrameCount(),
            frame->getFrameType(),
            (flag == true) ? "Update" : "Skip",
            streamConfigBit);

    return;
}

status_t ExynosCamera::m_setFrameManager()
{
    sp<FrameWorker> worker;

    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 300, 200);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new RunWorker("RUNNING FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 100, 300);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::RUNNING, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}

bool ExynosCamera::m_frameFactoryCreateThreadFunc(void)
{

#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    bool loop = false;
    status_t ret = NO_ERROR;

    ExynosCameraFrameFactory *framefactory = NULL;

    ret = m_frameFactoryQ->waitAndPopProcessQ(&framefactory);
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        goto func_exit;
    }

    if (framefactory == NULL) {
        CLOGE("framefactory is NULL");
        goto func_exit;
    }

    if (framefactory->isCreated() == false) {
        CLOGD("framefactory create");
        framefactory->create();
    } else {
        CLOGD("framefactory already create");
    }

func_exit:
    if (0 < m_frameFactoryQ->getSizeOfProcessQ()) {
        loop = true;
    }

    return loop;
}

status_t ExynosCamera::m_startFrameFactory(ExynosCameraFrameFactory *factory)
{
    status_t ret = OK;

    /* prepare pipes */
    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGW("Failed to prepare FLITE");
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("startPipe fail");
        return ret;
    }

    /* start all thread */
    ret = factory->startInitialThreads();
    if (ret < 0) {
        CLOGE("startInitialThreads fail");
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_stopFrameFactory(ExynosCameraFrameFactory *factory)
{
    int ret = 0;

    CLOGD("");
    if (factory != NULL  && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("stopPipe fail");
            return ret;
        }
    }

    return ret;
}

status_t ExynosCamera::m_deinitFrameFactory()
{
    CLOGI("-IN-");

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance, "
                        "set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            if (m_frameFactory[i]->isCreated() == true) {
                ret = m_frameFactory[i]->destroy();
                if (ret < 0)
                    CLOGE("m_frameFactory[%d] destroy fail", i);

                SAFE_DELETE(m_frameFactory[i]);

                CLOGD("m_frameFactory[%d] destroyed", i);
            }
        }
    }

    CLOGI("-OUT-");

    return ret;

}

status_t ExynosCamera::m_setSetfile(void) {
    int configMode = m_parameters[m_cameraId]->getConfigMode();
    int setfile = 0;
    int setfileReprocessing = 0;
    int yuvRange = YUV_FULL_RANGE;
    int yuvRangeReprocessing = YUV_FULL_RANGE;
    uint32_t minFps = 0, maxFps = 0;

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
        m_parameters[m_cameraId]->checkSetfileYuvRange();
        /* general */
        m_parameters[m_cameraId]->getSetfileYuvRange(false, &setfile, &yuvRange);
        /* reprocessing */
        m_parameters[m_cameraId]->getSetfileYuvRange(true, &setfileReprocessing, &yuvRangeReprocessing);
        break;
    case CONFIG_MODE::HIGHSPEED_60:
        setfile = ISS_SUB_SCENARIO_FHD_60FPS;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    case CONFIG_MODE::HIGHSPEED_120:
        setfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    case CONFIG_MODE::HIGHSPEED_240:
        m_parameters[m_cameraId]->getPreviewFpsRange(&minFps, &maxFps);
        if (maxFps == 120) {
            setfile = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        } else {
            setfile = ISS_SUB_SCENARIO_FHD_240FPS;
        }
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    default:
        CLOGE("configMode is abnormal(%d)", configMode);
        break;
    }

    CLOGI("configMode(%d), PIPMode(%d)", configMode, m_parameters[m_cameraId]->getPIPMode());

    /* reprocessing */
    m_parameters[m_cameraId]->setSetfileYuvRange(true, setfileReprocessing, yuvRangeReprocessing);
    /* preview */
    m_parameters[m_cameraId]->setSetfileYuvRange(false, setfile, yuvRange);

    return NO_ERROR;
}

status_t ExynosCamera::m_setupPipeline(ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;

    if (factory == NULL) {
        CLOGE("Frame factory is NULL!!");
        return BAD_VALUE;
    }

    int pipeId = MAX_PIPE_NUM;
    int nodePipeId = MAX_PIPE_NUM;
    uint32_t cameraId = factory->getCameraId();
    int32_t reprocessingBayerMode = m_parameters[cameraId]->getReprocessingBayerMode();
    bool flagFlite3aaM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
    bool flagMcscVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif
    int flipHorizontal = 0;
    enum NODE_TYPE nodeType;

    m_setSetfile();

    /* Setting default Bayer DMA-out request flag */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
        CLOGD("Use REPROCESSING_BAYER_MODE_NONE");
        factory->setRequest(PIPE_VC0, false);
        factory->setRequest(PIPE_3AC, false);
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        CLOGD("Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON");
        factory->setRequest(PIPE_VC0, true);
        factory->setRequest(PIPE_3AC, false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        CLOGD("Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON");
        factory->setRequest(PIPE_VC0, false);
        factory->setRequest(PIPE_3AC, true);
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        CLOGD("Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC");
        factory->setRequest(PIPE_VC0, false);
        factory->setRequest(PIPE_3AC, false);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        CLOGD("Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC");
        factory->setRequest(PIPE_VC0, false);
        factory->setRequest(PIPE_3AC, false);
        break;
    default :
        CLOGE("Unknown dynamic bayer mode");
        factory->setRequest(PIPE_3AC, false);
        break;
    }

    if (flagFlite3aaM2M == true) {
        factory->setRequest(PIPE_VC0, true);
    }

    if (flagMcscVraM2M == true) {
        factory->setRequest(PIPE_MCSC5, true);
    }

    /* Setting bufferSupplier based on H/W pipeline */
    if (flagFlite3aaM2M == true) {
        pipeId = PIPE_FLITE;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
    }

    pipeId = PIPE_3AA;

    if (flag3aaIspM2M == true) {
        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

        pipeId = PIPE_ISP;
    }

#ifdef USE_DUAL_CAMERA
    if (cameraId == m_cameraId
        && m_parameters[cameraId]->getDualMode() == true
        && m_parameters[cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        if (flagIspDcpM2M == true) {
            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* Setting OutputFrameQ/FrameDoneQ to Pipe */
            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

            pipeId = PIPE_DCP;
        }

        if (flagDcpMcscM2M == true) {
            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* Setting OutputFrameQ/FrameDoneQ to Pipe */
            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

            pipeId = PIPE_MCSC;
        }
    } else
#endif
    if (flagIspMcscM2M == true) {
        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

        pipeId = PIPE_MCSC;
    }

    ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setBufferSupplierToPipe into pipeId %d ", pipeId);
        return ret;
    }

    /* Setting OutputFrameQ/FrameDoneQ to Pipe */
    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    if (cameraId == m_cameraId) {
        flipHorizontal = m_parameters[cameraId]->getFlipHorizontal();

        nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
        if (nodePipeId != -1) {
            nodePipeId = nodePipeId + PIPE_MCSC0;
            nodeType = factory->getNodeType(nodePipeId);
            factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);
            CLOGD("set FlipHorizontal on preview, FlipHorizontal(%d) Recording pipeId(%d)",
                    flipHorizontal, nodePipeId);
        }
    }

    if (flagMcscVraM2M == true) {
        pipeId = PIPE_VRA;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
#ifdef SUPPORT_HFD
        if (m_parameters[m_cameraId]->getHfdMode() == true) {
            pipeId = PIPE_HFD;
            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        }
#endif
    }

#ifdef USE_DUAL_CAMERA
    if (cameraId == m_cameraId
        && m_parameters[cameraId]->getDualMode() == true) {
        pipeId = PIPE_SYNC;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

        if (m_parameters[cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            pipeId = PIPE_FUSION;

            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* Setting OutputFrameQ/FrameDoneQ to Pipe */
            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        }
    }
#endif

#ifdef SUPPORT_HW_GDC
    pipeId = PIPE_GDC;
    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
#endif

    return ret;
}

void ExynosCamera::m_updateCropRegion(struct camera2_shot_ext *shot_ext)
{
    int sensorMaxW = 0, sensorMaxH = 0;
    int cropRegionMinW = 0, cropRegionMinH = 0;
    int maxZoom = 0;

    m_parameters[m_cameraId]->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    shot_ext->shot.ctl.scaler.cropRegion[0] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[0], 2);
    shot_ext->shot.ctl.scaler.cropRegion[1] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[1], 2);
    shot_ext->shot.ctl.scaler.cropRegion[2] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[2], 2);
    shot_ext->shot.ctl.scaler.cropRegion[3] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[3], 2);

    /*  Check the validation of the crop size(width x height).
     The width and height of the crop region cannot be set to be smaller than
     floor( activeArraySize.width / android.scaler.availableMaxDigitalZoom )
     and floor( activeArraySize.height / android.scaler.availableMaxDigitalZoom )
     */
    maxZoom = (int) (m_parameters[m_cameraId]->getMaxZoomRatio() / 1000);
    cropRegionMinW = (int)ceil((float)sensorMaxW / maxZoom);
    cropRegionMinH = (int)ceil((float)sensorMaxH / maxZoom);

    cropRegionMinW = ALIGN_UP(cropRegionMinW, 2);
    cropRegionMinH = ALIGN_UP(cropRegionMinH, 2);

    if (cropRegionMinW > (int) shot_ext->shot.ctl.scaler.cropRegion[2]
        || cropRegionMinH > (int) shot_ext->shot.ctl.scaler.cropRegion[3]){
        CLOGE("Invalid Crop Size(%d, %d),Change to cropRegionMin(%d, %d)",
                shot_ext->shot.ctl.scaler.cropRegion[2],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                cropRegionMinW, cropRegionMinH);

        shot_ext->shot.ctl.scaler.cropRegion[2] = cropRegionMinW;
        shot_ext->shot.ctl.scaler.cropRegion[3] = cropRegionMinH;
    }


    /* 1. Check the validation of the crop size(width x height).
     * The crop size must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) shot_ext->shot.ctl.scaler.cropRegion[2]
        || sensorMaxH < (int)shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("Invalid Crop Size(%d, %d), sensorMax(%d, %d)",
                shot_ext->shot.ctl.scaler.cropRegion[2],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                sensorMaxW, sensorMaxH);
        shot_ext->shot.ctl.scaler.cropRegion[2] = sensorMaxW;
        shot_ext->shot.ctl.scaler.cropRegion[3] = sensorMaxH;
    }

    /* 2. Check the validation of the crop offset.
     * Offset coordinate + width or height must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) shot_ext->shot.ctl.scaler.cropRegion[0]
            + (int) shot_ext->shot.ctl.scaler.cropRegion[2]) {
        CLOGE("Invalid Crop Region, offsetX(%d), width(%d) sensorMaxW(%d)",
                shot_ext->shot.ctl.scaler.cropRegion[0],
                shot_ext->shot.ctl.scaler.cropRegion[2],
                sensorMaxW);
        shot_ext->shot.ctl.scaler.cropRegion[0] = sensorMaxW - shot_ext->shot.ctl.scaler.cropRegion[2];
    }

    if (sensorMaxH < (int) shot_ext->shot.ctl.scaler.cropRegion[1]
            + (int) shot_ext->shot.ctl.scaler.cropRegion[3]) {
        CLOGE("Invalid Crop Region, offsetY(%d), height(%d) sensorMaxH(%d)",
                shot_ext->shot.ctl.scaler.cropRegion[1],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                sensorMaxH);
        shot_ext->shot.ctl.scaler.cropRegion[1] = sensorMaxH - shot_ext->shot.ctl.scaler.cropRegion[3];
    }

    m_parameters[m_cameraId]->setCropRegion(
            shot_ext->shot.ctl.scaler.cropRegion[0],
            shot_ext->shot.ctl.scaler.cropRegion[1],
            shot_ext->shot.ctl.scaler.cropRegion[2],
            shot_ext->shot.ctl.scaler.cropRegion[3]);

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        ExynosRect slaveRect;

        m_parameters[m_cameraIds[1]]->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

        slaveRect.x = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[0]/DUAL_CAMERA_TELE_RATIO, 2);
        slaveRect.y = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[1]/DUAL_CAMERA_TELE_RATIO, 2);
        slaveRect.w = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[2]*DUAL_CAMERA_TELE_RATIO, 2);
        slaveRect.h = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[3]*DUAL_CAMERA_TELE_RATIO, 2);

        /* 1. Check the validation of the crop size(width x height).
         * The crop size must be smaller than sensor max size.
         */
        if (sensorMaxW < slaveRect.w
                || sensorMaxH < slaveRect.h) {
            slaveRect.w = sensorMaxW;
            slaveRect.h = sensorMaxH;
        }

        /* 2. Check the validation of the crop offset.
         * Offset coordinate + width or height must be smaller than sensor max size.
         */
        if (sensorMaxW < slaveRect.x + slaveRect.w) {
            slaveRect.x = sensorMaxW - slaveRect.w;
        }

        if (sensorMaxH < slaveRect.y + slaveRect.h) {
            slaveRect.y = sensorMaxH - slaveRect.h;
        }

        m_parameters[m_cameraIds[1]]->setCropRegion(
                slaveRect.x, slaveRect.y, slaveRect.w, slaveRect.h);
    }
#endif
}

void ExynosCamera::m_updateFD(struct camera2_shot_ext *shot_ext, enum facedetect_mode fdMode, int dsInputPortId, bool isReprocessing)
{
    if (fdMode <= FACEDETECT_MODE_OFF) {
        shot_ext->fd_bypass = 1;
        shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
        shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
    } else {
        if (isReprocessing == true) {
            if (dsInputPortId < MCSC_PORT_MAX) {
                shot_ext->fd_bypass = 1;
                shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            } else {
                shot_ext->fd_bypass = 0;
                shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) (dsInputPortId % MCSC_PORT_MAX);
            }
        } else {
            //To Do
            if (m_parameters[m_cameraId]->getPIPMode() == true && getCameraId() == CAMERA_ID_BACK) {
                shot_ext->fd_bypass = 1;
                shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            } else {
                if (dsInputPortId < MCSC_PORT_MAX) {
                    shot_ext->fd_bypass = 0;
                    shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) dsInputPortId ;
                } else {
                    shot_ext->fd_bypass = 1;
                    shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
                    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
                }
            }
        }
    }
}

void ExynosCamera::m_updateEdgeNoiseMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame)
{
    enum processing_mode mode = PROCESSING_MODE_FAST;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return;
    }

    if (isCaptureFrame == true) {
        mode = PROCESSING_MODE_OFF;
    } else {
        mode = PROCESSING_MODE_FAST;
    }

    if (shot_ext->shot.ctl.noise.mode
        == (enum processing_mode) FIMC_IS_METADATA(ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG)) {
        shot_ext->shot.ctl.noise.mode = mode;
    }
    if (shot_ext->shot.ctl.edge.mode
        == (enum processing_mode) FIMC_IS_METADATA(ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG)) {
        shot_ext->shot.ctl.edge.mode = mode;
    }
}

status_t ExynosCamera::m_updateJpegControlInfo(const struct camera2_shot_ext *shot_ext)
{
    status_t ret = NO_ERROR;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return BAD_VALUE;
    }

    ret = m_parameters[m_cameraId]->checkJpegQuality(shot_ext->shot.ctl.jpeg.quality);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkJpegQuality. quality %d",
                 shot_ext->shot.ctl.jpeg.quality);

    ret = m_parameters[m_cameraId]->checkThumbnailSize(
            shot_ext->shot.ctl.jpeg.thumbnailSize[0],
            shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkThumbnailSize. size %dx%d",
                shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                shot_ext->shot.ctl.jpeg.thumbnailSize[1]);

    ret = m_parameters[m_cameraId]->checkThumbnailQuality(shot_ext->shot.ctl.jpeg.thumbnailQuality);
    if (ret != NO_ERROR)
        CLOGE("Failed to checkThumbnailQuality. quality %d",
                shot_ext->shot.ctl.jpeg.thumbnailQuality);

    return ret;
}

void ExynosCamera::m_updateSetfile(struct camera2_shot_ext *shot_ext, bool isCaptureFrame)
{
    int yuvRange = 0;
    int setfile = 0;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return;
    }

    if (isCaptureFrame == true) {
        m_parameters[m_cameraId]->setSetfileYuvRange();
    }

    m_parameters[m_cameraId]->getSetfileYuvRange(isCaptureFrame, &setfile, &yuvRange);
    shot_ext->setfile = setfile;
}

status_t ExynosCamera::m_generateFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                        Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame,
                                        ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = OK;
    newFrame = NULL;
    uint32_t frameCount = request->getFrameCount();

    CLOGV("(%d)", frameCount);
#ifndef USE_DUAL_CAMERA
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    newFrame->setHasRequest(true);

    return ret;
}

status_t ExynosCamera::m_generateFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                       Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame,
                                       ExynosCameraRequestSP_sprt_t request, bool useJpegFlag)
{
    status_t ret = OK;
    newFrame = NULL;
    uint32_t frameCount = request->getFrameCount();

    CLOGV("(%d)", frameCount);
#ifndef USE_DUAL_CAMERA
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount, useJpegFlag);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    newFrame->setHasRequest(true);

    return ret;
}

status_t ExynosCamera::m_setupEntity(uint32_t pipeId,
                                      ExynosCameraFrameSP_sptr_t newFrame,
                                      ExynosCameraBuffer *srcBuf,
                                      ExynosCameraBuffer *dstBuf,
                                      int dstNodeIndex,
                                      int dstPipeId)
{
    status_t ret = OK;
    entity_buffer_state_t entityBufferState;

    CLOGV("(%d)", pipeId);
    /* set SRC buffer */
    ret = newFrame->getSrcBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setSrcBuffer(pipeId, newFrame, srcBuf);
        if (ret < 0) {
            CLOGE("m_setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }
    }

    /* set DST buffer */
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState, dstNodeIndex);
    if (ret < 0) {
        CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf, dstNodeIndex, dstPipeId);
        if (ret < 0) {
            CLOGE("m_setDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
             pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_setSrcBuffer(uint32_t pipeId, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer)
{
    status_t ret = OK;
    ExynosCameraBuffer srcBuffer;

    CLOGV("(%d)", pipeId);
    if (buffer == NULL) {
        buffer_manager_tag_t bufTag;
        bufTag.pipeId[0] = pipeId;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        buffer = &srcBuffer;

        ret = m_bufferSupplier->getBuffer(bufTag, buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to getBuffer. pipeId %d ret %d",
                    newFrame->getFrameCount(), pipeId, ret);
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("[F%d B%d]Failed to setSrcBuffer. pipeId %d ret %d",
                newFrame->getFrameCount(), buffer->index, pipeId, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_setDstBuffer(uint32_t pipeId,
                                       ExynosCameraFrameSP_sptr_t newFrame,
                                       ExynosCameraBuffer *buffer,
                                       int nodeIndex,
                                       __unused int dstPipeId)
{
    status_t ret = OK;

    if (buffer == NULL) {
        CLOGE("[F%d]Buffer is NULL", newFrame->getFrameCount());
        return BAD_VALUE;
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer, nodeIndex);
    if (ret < 0) {
        CLOGE("[F%d B%d]Failed to setDstBuffer. pipeId %d nodeIndex %d ret %d",
                newFrame->getFrameCount(), buffer->index, pipeId, nodeIndex, ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_setVOTFBuffer(uint32_t pipeId,
                                       ExynosCameraFrameSP_sptr_t newFrame,
                                       uint32_t srcPipeId,
                                       uint32_t dstPipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    int srcPos = -1;
    int dstPos = -1;
    /*
       COUTION: getNodeType is common function, So, use preview frame factory.
       If, getNodeType is seperated, frame factory must seperate.
     */
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    if (newFrame == NULL) {
        CLOGE("New frame is NULL!!");
        return BAD_VALUE;
    }

    /* Get buffer from buffer supplier */
    ret = m_checkBufferAvailable(srcPipeId);
    if (ret != NO_ERROR) {
        CLOGE("Waiting buffer for Pipe(%d) timeout. ret %d", srcPipeId, ret);
        return INVALID_OPERATION;
    }

    bufTag = initBufTag;
    bufTag.pipeId[0] = srcPipeId;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
    buffer.index = -2;

    ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to get Buffer(Pipeid:%d). ret %d",
                newFrame->getFrameCount(), srcPipeId, ret);
        return INVALID_OPERATION;
    }

    if (buffer.index < 0) {
        CLOGE("[F%d]Invalid Buffer index(%d), Pipeid %d. ret %d",
                newFrame->getFrameCount(), buffer.index, srcPipeId, ret);
        return INVALID_OPERATION;
    }

    /* Set Buffer to srcPosition in Frame */
    if (newFrame->getRequest(srcPipeId) == true) {
        srcPos = factory->getNodeType(srcPipeId);
        ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, srcPos);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                    pipeId, srcPipeId, srcPos);
            newFrame->setRequest(srcPipeId, false);
        } else {
            ret = newFrame->setDstBuffer(pipeId, buffer, srcPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                        pipeId, srcPipeId, srcPos);
                newFrame->setRequest(srcPipeId, false);
            }
        }
    }

    /* Set Buffer to dstPosition in Frame */
    if (newFrame->getRequest(srcPipeId) == true) {
        dstPos = factory->getNodeType(dstPipeId);
        ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                    pipeId, srcPipeId, dstPos);
            newFrame->setRequest(srcPipeId, false);
        } else {
            ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                        pipeId, srcPipeId, dstPos);
                newFrame->setRequest(srcPipeId, false);
            }
        }
    }

    /* If setBuffer failed, return buffer */
    if (newFrame->getRequest(srcPipeId) == false) {
        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to put Buffer(Pipeid:%d). ret %d",
                    newFrame->getFrameCount(), srcPipeId, ret);
            ret = INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setHWFCBuffer(uint32_t pipeId,
                                       ExynosCameraFrameSP_sptr_t newFrame,
                                       uint32_t srcPipeId,
                                       uint32_t dstPipeId)
{
    /* This function is same logic with m_setVOTFBuffer */
    return m_setVOTFBuffer(pipeId, newFrame, srcPipeId, dstPipeId);
}

status_t ExynosCamera::m_createIonAllocator(ExynosCameraIonAllocator **allocator)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    do {
        retry++;
        CLOGI("try(%d) to create IonAllocator", retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0)
            CLOGE("create IonAllocator fail (retryCount=%d)", retry);
        else {
            CLOGD("m_createIonAllocator success (allocator=%p)", *allocator);
            break;
        }
    } while ((ret < 0) && (retry < 3));

    if ((ret < 0) && (retry >=3)) {
        CLOGE("create IonAllocator fail (retryCount=%d)", retry);
        ret = INVALID_OPERATION;
    }

    return ret;
}

bool ExynosCamera::m_captureThreadFunc()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraFrameFactory *factory = NULL;
    uint32_t pipeId = 0;
    int retryCount = 1;

    ret = m_captureQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("Wait timeout");
        } else {
            CLOGE("Failed to wait&pop captureQ, ret %d", ret);
            /* TODO: doing exception handling */
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto FUNC_EXIT;
    }

    CLOGV("[F%d T%d]frame frameCount", frame->getFrameCount(), frame->getFrameType());

    pipeId = frame->getFirstEntity()->getPipeId();
    m_captureStreamThread->run(PRIORITY_DEFAULT);

    if (frame->getStreamRequested(STREAM_TYPE_CAPTURE) == false
#ifdef USE_DUAL_CAMERA
        && frame->getFrameType() != FRAME_TYPE_REPROCESSING_DUAL_SLAVE
#endif
        && frame->getStreamRequested(STREAM_TYPE_RAW) == false) {
        ret = frame->setEntityState(pipeId, ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to setEntityState(FRAME_DONE). pipeId %d ret %d",
                    frame->getFrameCount(), pipeId, ret);
            /* continue */
        }

        m_yuvCaptureDoneQ->pushProcessQ(&frame);
    } else {
#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
        } else
#endif
        {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
        }

        if (factory == NULL) {
            CLOGE("frame factory is NULL");
            goto FUNC_EXIT;
        }

        while (factory->isRunning() == false) {
            CLOGD("[F%d]Wait to start reprocessing stream", frame->getFrameCount());

            if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGD("[F%d]Flush is in progress.", frame->getFrameCount());
                goto FUNC_EXIT;
            }

            usleep(5000);
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            factory->pushFrameToPipe(frame, PIPE_SYNC_REPROCESSING);
        } else
#endif
        {
            factory->pushFrameToPipe(frame, pipeId);
        }

        /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
        if (m_parameters[m_cameraId]->isReprocessing() == true)
            factory->startThread(pipeId);

        /* Wait reprocesisng done */
        do {
            ret = m_reprocessingDoneQ->waitAndPopProcessQ(&frame);
        } while (ret == TIMED_OUT && retryCount-- > 0);

        if (ret != NO_ERROR) {
            CLOGW("[F%d]Failed to waitAndPopProcessQ to reprocessingDoneQ. ret %d",
                    frame->getFrameCount(), ret);
        }
        CLOGI("[F%d]Wait reprocessing done", frame->getFrameCount());
    }

    /* Thread can exit only if timeout or error occured from inputQ (at CLEAN: label) */
    return true;

CLEAN:
    if (frame != NULL && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to removeFrameFromList for captureProcessList. ret %d",
                    frame->getFrameCount(), ret);
        }

        frame->printEntity();
        CLOGD("[F%d]Delete frame from captureProcessList", frame->getFrameCount());
        frame = NULL;
    }

FUNC_EXIT:
    if (m_captureQ->getSizeOfProcessQ() > 0)
        return true;
    else
        return false;
}

status_t ExynosCamera::m_getBayerServiceBuffer(ExynosCameraFrameSP_sptr_t frame,
                                                ExynosCameraBuffer *buffer,
                                                ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    int retryCount = 30;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int dstPos = factory->getNodeType(PIPE_VC0);
    int pipeId = -1;

    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
        pipeId = PIPE_3AA_REPROCESSING;
    } else {
        pipeId = PIPE_ISP_REPROCESSING;
    }

    if (frame->getStreamRequested(STREAM_TYPE_RAW)) {
        m_captureZslSelector->setWaitTime(200000000);
        bayerFrame = m_captureZslSelector->selectDynamicFrames(1, m_getBayerPipeId(), false, retryCount, dstPos);
        if (bayerFrame == NULL) {
            CLOGE("bayerFrame is NULL");
            return INVALID_OPERATION;
        }

        if (bayerFrame->getMetaDataEnable() == true)
            CLOGI("Frame metadata is updated. frameCount %d",
                     bayerFrame->getFrameCount());

        ret = bayerFrame->getDstBuffer(m_getBayerPipeId(), buffer, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d ret %d",
                    m_getBayerPipeId(), ret);
            return ret;
        }
    } else {
        if (request != NULL) {
            camera3_stream_buffer_t *stream_buffer = request->getInputBuffer();
            buffer_handle_t *handle = stream_buffer->buffer;
            buffer_manager_tag_t bufTag;

            CLOGI("[R%d F%d H%p]Getting Bayer from getRunningRequest",
                    request->getKey(), frame->getFrameCount(), handle);

            bufTag.pipeId[0] = pipeId;
            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
            buffer->handle[0] = handle;
            buffer->acquireFence[0] = stream_buffer->acquire_fence;
            buffer->releaseFence[0] = stream_buffer->release_fence;

            ret = m_bufferSupplier->getBuffer(bufTag, buffer);

            ret = request->setAcquireFenceDone(handle, (buffer->acquireFence[0] == -1) ? true : false);
            if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d B%d]Failed to setAcquireFenceDone. ret %d",
                            request->getKey(), frame->getFrameCount(), buffer->index, ret);
                    return ret;
            }

            CLOGI("[R%d F%d B%d H%p]Service bayer selected. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer->index, handle, ret);
        } else {
            CLOGE("request if NULL(fcount:%d)", frame->getFrameCount());
            ret = BAD_VALUE;
        }
    }

    return ret;
}

bool ExynosCamera::m_previewStreamBayerPipeThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_FLITE]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /* CLOGW("wait timeout"); */
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_FLITE);
}

bool ExynosCamera::m_previewStream3AAPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_3AA]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_3AA);
}

bool ExynosCamera::m_previewStreamISPPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_ISP]->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGV("wait timeout");
        else
            CLOGE("Failed to waitAndPopProcessQ. ret %d", ret);
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_ISP);
}

bool ExynosCamera::m_previewStreamMCSCPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_MCSC]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_MCSC);
}

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_previewStreamDCPPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_DCP]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_DCP);
}
#endif

#ifdef SUPPORT_HW_GDC
bool ExynosCamera::m_previewStreamGDCPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_GDC]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_GDC);
}
#endif

bool ExynosCamera::m_previewStreamVRAPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_VRA]->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto retry_thread_loop;
    } else if (newFrame == NULL) {
        CLOGE("Frame is NULL");
        goto retry_thread_loop;
    }

    return m_previewStreamFunc(newFrame, PIPE_VRA);

retry_thread_loop:
    if (m_pipeFrameDoneQ[PIPE_VRA]->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}

#ifdef SUPPORT_HFD
bool ExynosCamera::m_previewStreamHFDPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_HFD]->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto retry_thread_loop;
    } else if (newFrame == NULL) {
        CLOGE("Frame is NULL");
        goto retry_thread_loop;
    }

    return m_previewStreamFunc(newFrame, PIPE_HFD);

retry_thread_loop:
    if (m_pipeFrameDoneQ[PIPE_VRA]->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}
#endif

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_previewStreamSyncPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_SYNC]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_SYNC);
}

bool ExynosCamera::m_previewStreamFusionPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_FUSION]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_FUSION);
}
#endif

status_t ExynosCamera::m_allocBuffers(
        const buffer_manager_tag_t tag,
        const buffer_manager_configuration_t info)
{
    status_t ret = NO_ERROR;

    ret = m_bufferSupplier->setInfo(tag, info);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setInfo. ret %d", ret);

        return ret;
    }

    return m_bufferSupplier->alloc(tag);
}

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret;

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        ret = m_setBuffers();
    } else {
        ret = m_setVisionBuffers();
    }

    if (ret < 0) {
        CLOGE("m_setBuffers failed");
        m_bufferSupplier->deinit();
        return false;
    }

    return false;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }

    return pipeId;
}

status_t ExynosCamera::m_pushServiceRequest(camera3_capture_request *request_in, ExynosCameraRequestSP_dptr_t req)
{
    status_t ret = OK;

    CLOGV("m_pushServiceRequest frameCnt(%d)", request_in->frame_number);

    req = m_requestMgr->registerToServiceList(request_in);
    if (req == NULL) {
        CLOGE("registerToServiceList failed, FrameNumber = [%d]", request_in->frame_number);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_popServiceRequest(ExynosCameraRequestSP_dptr_t request)
{
    status_t ret = OK;

    CLOGV("m_popServiceRequest ");

    request = m_requestMgr->eraseFromServiceList();
    if (request == NULL) {
        CLOGE("createRequest failed ");
        ret = INVALID_OPERATION;
    }
    return ret;
}

status_t ExynosCamera::m_pushRunningRequest(ExynosCameraRequestSP_dptr_t request_in)
{
    status_t ret = NO_ERROR;

    CLOGV("[R%d F%d] Push Request in runningRequest",
            request_in->getKey(), request_in->getFrameCount());

    ret = m_requestMgr->registerToRunningList(request_in);
    if (ret != NO_ERROR) {
        CLOGE("[R%d] registerToRunningList is failed", request_in->getKey());
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_setFactoryAddr(ExynosCameraRequestSP_dptr_t request)
{
    status_t ret = NO_ERROR;
    FrameFactoryList factorylist;
    FrameFactoryListIterator factorylistIter;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameFactory *factoryAddr[FRAME_FACTORY_TYPE_MAX] = {NULL, };

    factorylist.clear();
    request->getFrameFactoryList(&factorylist);
    for (factorylistIter = factorylist.begin(); factorylistIter != factorylist.end(); ) {
        factory = *factorylistIter;

        if (factory->getFactoryType() < FRAME_FACTORY_TYPE_MAX && factoryAddr[factory->getFactoryType()] == NULL) {
            factoryAddr[factory->getFactoryType()] = factory;
            CLOGV("list Factory(%p) Type(%d)", factory, factory->getFactoryType());
        }

        factorylistIter++;
    }

    request->setFactoryAddrList(factoryAddr);

    return ret;
}

bool ExynosCamera::m_reprocessingFrameFactoryStartThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameFactory *factory = NULL;

    factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    if (factory == NULL) {
        CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!!");

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("Reprocessing FrameFactory is NOT created!");
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("Reprocessing FrameFactory is already running");
        return false;
    }

    /* Set buffer manager */
    ret = m_setupReprocessingPipeline(factory);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setupReprocessingPipeline. ret %d",
                 ret);
        return false;
    }

    ret = factory->initPipes();
    if (ret < 0) {
        CLOGE("Failed to initPipes. ret %d",
                 ret);
        return false;
    }

    ret = m_startReprocessingFrameFactory(factory);
    if (ret < 0) {
        CLOGE("Failed to startReprocessingFrameFactory");
        /* TODO: Need release buffers and error exit */
        return false;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
        if (factory == NULL) {
            CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!!");
            return false;
        } else if (factory->isCreated() == false) {
            CLOGE("Reprocessing FrameFactory is NOT created!");
            return false;
        } else if (factory->isRunning() == true) {
            CLOGW("Reprocessing FrameFactory is already running");
            return false;
        }

        /* Set buffer manager */
        ret = m_setupReprocessingPipeline(factory);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setupReprocessingPipeline. ret %d",
                    ret);
            return false;
        }

        ret = factory->initPipes();
        if (ret < 0) {
            CLOGE("Failed to initPipes. ret %d",
                    ret);
            return false;
        }

        ret = m_startReprocessingFrameFactory(factory);
        if (ret < 0) {
            CLOGE("Failed to startReprocessingFrameFactory");
            /* TODO: Need release buffers and error exit */
            return false;
        }
    }
#endif

    return false;
}

status_t ExynosCamera::m_startReprocessingFrameFactory(ExynosCameraFrameFactory *factory)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = 0;

    CLOGD("- IN -");

    ret = factory->preparePipes();
    if (ret < 0) {
        CLOGE("m_reprocessingFrameFactory preparePipe fail");
        return ret;
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret < 0) {
        CLOGE("m_reprocessingFrameFactory startPipe fail");
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_stopReprocessingFrameFactory(ExynosCameraFrameFactory *factory)
{
    CLOGI("");
    status_t ret = 0;

    if (factory != NULL && factory->isRunning() == true) {
        ret = factory->stopPipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory->stopPipe() fail");
        }
    }

    CLOGD("clear m_captureProcessList(Picture) Frame list");
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_checkBufferAvailable(uint32_t pipeId)
{
    status_t ret = TIMED_OUT;
    buffer_manager_tag_t bufTag;
    int retry = 0;

    bufTag.pipeId[0] = pipeId;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    do {
        ret = -1;
        retry++;
        if (m_bufferSupplier->getNumOfAvailableBuffer(bufTag) > 0) {
            ret = OK;
        } else {
            /* wait available ISP buffer */
            usleep(WAITING_TIME);
        }
        if (retry % 10 == 0)
            CLOGW("retry(%d) setupEntity for pipeId(%d)", retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

    return ret;
}

bool ExynosCamera::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    if (m_parameters[m_cameraId]->isReprocessing() == true) {
        ret = m_setReprocessingBuffer();
        if (ret < 0) {
            CLOGE("m_setReprocessing Buffer fail");
            m_bufferSupplier->deinit();
            return false;
        }
    }

    return false;
}

status_t ExynosCamera::m_generateInternalFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                                Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame,
                                                ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = OK;
    newFrame = NULL;
    uint32_t frameCount = 0;
    bool hasReques = false;

    if (request != NULL) {
        frameCount = request->getFrameCount();
        hasReques = true;
    } else {
        frameCount = m_internalFrameCount++;
        hasReques = false;
    }

    CLOGV("(%d)", frameCount);
#ifndef USE_DUAL_CAMERA
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    /* Set frame type into FRAME_TYPE_INTERNAL */
    ret = newFrame->setFrameInfo(m_parameters[m_cameraId], frameCount, FRAME_TYPE_INTERNAL);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setFrameInfo with INTERNAL. frameCount %d", frameCount);
        return ret;
    }

    newFrame->setHasRequest(hasReques);

    return ret;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::m_generateTransitionFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                                Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame)
{
    status_t ret = NO_ERROR;
    newFrame = NULL;
    uint32_t frameCount = m_internalFrameCount;
    bool hasReques = false;

    frameCount = m_internalFrameCount;

    CLOGV("(%d)", frameCount);
#ifndef USE_DUAL_CAMERA
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }
        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    /* Set frame type into FRAME_TYPE_TRANSITION */
    ret = newFrame->setFrameInfo(m_parameters[m_cameraId], frameCount, FRAME_TYPE_TRANSITION);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setFrameInfo with INTERNAL. frameCount %d", frameCount);
        return ret;
    }

    newFrame->setHasRequest(hasReques);

    return ret;
}

status_t ExynosCamera::m_checkDualOperationMode(ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;

    if (request == NULL) {
        CLOGE("request is NULL!!");
        return BAD_VALUE;
    }

    enum DUAL_OPERATION_MODE newOperationMode = DUAL_OPERATION_MODE_NONE;
    enum DUAL_OPERATION_MODE oldOperationMode = m_parameters[m_cameraId]->getDualOperationMode();
    struct CameraMetaParameters *metaParameters = request->getMetaParameters();
    float zoomRatio = metaParameters->m_zoomRatio;

    m_parameters[m_cameraIds[1]]->setZoomRatio(zoomRatio);
    if (zoomRatio == 1.0f) {
        newOperationMode = DUAL_OPERATION_MODE_MASTER;
    } else if (zoomRatio > 2.0f) {
        newOperationMode = DUAL_OPERATION_MODE_SLAVE;
    } else {
        newOperationMode = DUAL_OPERATION_MODE_SYNC;
    }

    newOperationMode = DUAL_OPERATION_MODE_SYNC;

    if (oldOperationMode == DUAL_OPERATION_MODE_SYNC
        && m_dualTransitionCount > 0) {
        m_dualTransitionCount--;
        return NO_ERROR;
    }

    if (newOperationMode != oldOperationMode) {
        switch (oldOperationMode) {
        case DUAL_OPERATION_MODE_MASTER:
        case DUAL_OPERATION_MODE_SLAVE:
            if (newOperationMode != DUAL_OPERATION_MODE_SYNC) {
                newOperationMode = DUAL_OPERATION_MODE_SYNC;
            }
            break;
        default:
            break;
        }

        m_parameters[m_cameraId]->setDualOperationMode(newOperationMode);

        if (newOperationMode == DUAL_OPERATION_MODE_SYNC) {
            m_dualTransitionCount = 30;
            ret = m_setStandbyMode(false);
            if (ret != NO_ERROR) {
                CLOGE("Set Standby mode(false) fail! ret(%d)", ret);
            }
        } else {
            ret = m_setStandbyMode(true);
            if (ret != NO_ERROR) {
                CLOGE("Set Standby mode(true) fail! ret(%d)", ret);
            }
        }
    }

    return ret;
}

status_t ExynosCamera::m_setStandbyMode(bool standby)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    enum DUAL_OPERATION_MODE dualOperationMode = m_parameters[m_cameraId]->getDualOperationMode();
    bool sensorStandby = false;
    bool isNeedPrepare = false;
    ExynosCameraFrameFactory *factory = NULL;
    frame_type_t frameType = FRAME_TYPE_BASE;

    switch (dualOperationMode) {
    case DUAL_OPERATION_MODE_MASTER:
        if (standby == true) {
            sensorStandby = m_parameters[m_cameraId]->isSupportSlaveSensorStandby();
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        } else {
            sensorStandby = m_parameters[m_cameraId]->isSupportMasterSensorStandby();
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        }
        frameType = FRAME_TYPE_TRANSITION;
        break;
    case DUAL_OPERATION_MODE_SLAVE:
        if (standby == true) {
            sensorStandby = m_parameters[m_cameraId]->isSupportMasterSensorStandby();
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        } else {
            sensorStandby = m_parameters[m_cameraId]->isSupportSlaveSensorStandby();
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        }
        frameType = FRAME_TYPE_TRANSITION;
        break;
    case DUAL_OPERATION_MODE_SYNC:
        if (standby == true) {
            CLOGE("Invalid standby(true) setting, fail! Dual operation mode(%d)",
                    dualOperationMode);
            return BAD_VALUE;
        }

        if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->isSensorStandby() == true) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            frameType = FRAME_TYPE_INTERNAL;
        } else if (m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]->isSensorStandby() == true) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            frameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
        }
        break;
    default:
        CLOGE("Invalid dual operation mode(%d)", dualOperationMode);
        return INVALID_OPERATION;
        break;
    }

    if (standby == true) {
        if (sensorStandby == false) {
            /* Goto post standby mode, and prepare Frames */
            isNeedPrepare = true;

            m_dualShotDoneQ->release();
            if (m_dualMainThread->isRunning() == false) {
                m_dualMainThread->run(PRIORITY_URGENT_DISPLAY);
            }
        }
    } else {
        if (sensorStandby == true) {
            /* Wakeup for Sync mode, and prepare Frames */
            isNeedPrepare = true;
        }
    }

    if (m_getState() == EXYNOS_CAMERA_STATE_RUN
        && isNeedPrepare == true) {
        uint32_t prepare = m_parameters[m_cameraId]->getSensorStandbyDelay();
        for (uint32_t i = 0; i < prepare; i++) {
            ret = m_createInternalFrameFunc(NULL, REQ_SYNC_NONE, frameType);
            if (ret != NO_ERROR) {
                CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                        i, prepare);
            }
        }
    }

    if (sensorStandby == true) {
        ret = factory->sensorStandby(standby);
        if (ret != NO_ERROR) {
            CLOGE("sensorStandby(%s) fail! ret(%d)", (standby?"On":"Off"), ret);
        }
    }

    return ret;
}
#endif

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

bool ExynosCamera::m_monitorThreadFunc(void)
{
    CLOGV("");

    int *threadState;
    uint64_t *timeInterval;
    int *pipeCountRenew, resultCountRenew;
    int ret = NO_ERROR;
    int loopCount = 0;
    int dtpStatus = 0;
    int pipeIdFlite = m_getBayerPipeId();
    int pipeIdErrorCheck = 0;
    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif
    ExynosCameraFrameFactory *factory = NULL;

    /* 1. MONITOR_THREAD_INTERVAL (0.2s)*/
    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL / 20)) {
        if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
            CLOGI("Flush is in progress");
            return false;
        }
        usleep(MONITOR_THREAD_INTERVAL / 20);
    }

    m_framefactoryCreateThread->join();

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    if (factory == NULL) {
        CLOGE("previewFrameFactory is NULL. Skip monitoring.");
        return false;
    }

    /* 2. Select Error check pipe ID (of last output node) */
#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
            pipeIdErrorCheck = PIPE_MCSC;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
            pipeIdErrorCheck = PIPE_DCP;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
            pipeIdErrorCheck = PIPE_ISP;
        } else {
            pipeIdErrorCheck = PIPE_3AA;
        }
    } else
#endif
    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
        pipeIdErrorCheck = PIPE_MCSC;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
        pipeIdErrorCheck = PIPE_ISP;
    } else {
        pipeIdErrorCheck = PIPE_3AA;
    }

    if (factory->checkPipeThreadRunning(pipeIdErrorCheck) == false) {
        CLOGW("pipe(%d) is not running.. Skip monitoring.", pipeIdErrorCheck);
        return false;
    }

    /* 3. Monitor sync log output */
#ifdef MONITOR_LOG_SYNC
    uint32_t pipeIdLogSync = PIPE_3AA;

    /* If it is not front camera in PIP and sensor pipe is running, do sync log */
    if (factory->checkPipeThreadRunning(pipeIdLogSync) &&
        !(getCameraId() == CAMERA_ID_FRONT && m_parameters[m_cameraId]->getPIPMode())) {
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("@FIMC_IS_SYNC %d", syncLogId);
            factory->syncLog(pipeIdLogSync, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif

    /* 4. DTP status check */
    factory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);

    if (dtpStatus == 1) {
        CLOGE("DTP Detected. dtpStatus(%d)", dtpStatus);
        dump();

        /* Once this error is returned by some method, or if notify() is called with ERROR_DEVICE,
         * only the close() method can be called successfully. */
        m_requestMgr->notifyDeviceError();
        ret = m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        if (ret != NO_ERROR) {
            CLOGE("Failed to transitState into ERROR. ret %d", ret);
        }

#ifdef SENSOR_OVERFLOW_CHECK
        android_printAssert(NULL, LOG_TAG, "killed by itself");
#endif
        return false;
    }

    /* 5. ESD check */
    if (m_streamManager->findStream(HAL_STREAM_ID_PREVIEW) == true) {
        factory->getThreadState(&threadState, pipeIdErrorCheck);
        factory->getThreadRenew(&pipeCountRenew, pipeIdErrorCheck);

        if ((*threadState == ERROR_POLLING_DETECTED) || (*pipeCountRenew > ERROR_DQ_BLOCKED_COUNT)) {
            CLOGE("ESD Detected. threadState(%d) pipeCountRenew(%d)", *threadState, *pipeCountRenew);
            goto ERROR;
        }

        CLOGV("Thread Renew Count [%d]", *pipeCountRenew);
        factory->incThreadRenew(pipeIdErrorCheck);
    }

    /* 6. Result Callback Error check */
    if (m_getState() == EXYNOS_CAMERA_STATE_RUN) {
        resultCountRenew = m_requestMgr->getResultRenew();

        if (resultCountRenew > ERROR_RESULT_DALAY_COUNT) {
            CLOGE("Device Error Detected. Probably there was not result callback about %.2f seconds. resultCountRenew(%d)",
                    (float)MONITOR_THREAD_INTERVAL / 1000000 * (float)ERROR_RESULT_DALAY_COUNT, resultCountRenew);
            m_requestMgr->dump();
            goto ERROR;
        }

        CLOGV("Result Renew Count [%d]", resultCountRenew);
        m_requestMgr->incResultRenew();
    } else {
        m_requestMgr->resetResultRenew();
    }

    factory->getThreadInterval(&timeInterval, pipeIdErrorCheck);
    CLOGV("Thread IntervalTime [%lld]", (long long)(*timeInterval));

    return true;

ERROR:
    dump();

    /* Once this error is returned by some method, or if notify() is called with ERROR_DEVICE,
     * only the close() method can be called successfully. */
    m_requestMgr->notifyDeviceError();
    ret = m_transitState(EXYNOS_CAMERA_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into ERROR. ret %d", ret);
    }

    if (m_parameters[m_cameraId]->getSamsungCamera() == true) {
        ret = factory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
        if (ret < 0) {
            CLOGE("PIPE_3AA V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", ret);
        }
    }

    return false;
}

#ifdef YUV_DUMP
bool ExynosCamera::m_dumpThreadFunc(void)
{
    char filePath[70];
    int size = -1;
    uint32_t pipeId = -1;
    uint32_t nodeIndex = -1;
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer buffer;
    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

    ret = m_dumpFrameQ->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT) {
        CLOGW("Time-out to wait dumpFrameQ");
        goto THREAD_EXIT;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ for dumpFrameQ");
        goto THREAD_EXIT;
    } else if (frame == NULL) {
        CLOGE("frame is NULL");
        goto THREAD_EXIT;
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/YuvCapture.yuv");

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
            pipeId = PIPE_MCSC_REPROCESSING;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
            pipeId = PIPE_DCP_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
            pipeId = PIPE_ISP_REPROCESSING;
        } else {
            pipeId = PIPE_3AA_REPROCESSING;
        }
    } else
#endif
    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
        pipeId = PIPE_MCSC_REPROCESSING;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
        pipeId = PIPE_ISP_REPROCESSING;
    } else {
        pipeId = PIPE_3AA_REPROCESSING;
    }

    nodeIndex = factory->getNodeType(PIPE_MCSC3_REPROCESSING);

    ret = frame->getDstBuffer(pipeId, &buffer, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getDstBuffer. frameCount %d pipeId %d nodeIndex %d",
                frame->getFrameCount(), pipeId, nodeIndex);
        goto THREAD_EXIT;
    }

    size = buffer.size[0];

    ret = dumpToFile((char *)filePath, buffer.addr[0], size);
    if (ret == false) {
        CLOGE("Failed to dumpToFile. frameCount %d filePath %s size %d",
                frame->getFrameCount(), filePath, size);
        goto THREAD_EXIT;
    }

THREAD_EXIT:
    if (m_dumpFrameQ->getSizeOfProcessQ() > 0) {
        CLOGI("run again. dumpFrameQ size %d",
                 m_dumpFrameQ->getSizeOfProcessQ());
        return true;
    } else {
        CLOGI("Complete to dumpFrame. frameCount %d",
                 frame->getFrameCount());
        return false;
    }
}
#endif

status_t ExynosCamera::m_transitState(ExynosCamera::exynos_camera_state_t state)
{
    int ret = NO_ERROR;

    Mutex::Autolock lock(m_stateLock);

    CLOGV("State transition. curState %d newState %d", m_state, state);

    if (state == EXYNOS_CAMERA_STATE_ERROR) {
        CLOGE("Exynos camera state is ERROR! The device close of framework must be called.");
    }

    switch (m_state) {
    case EXYNOS_CAMERA_STATE_OPEN:
        if (state != EXYNOS_CAMERA_STATE_INITIALIZE
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }
        break;
    case EXYNOS_CAMERA_STATE_INITIALIZE:
        if (state != EXYNOS_CAMERA_STATE_CONFIGURED
            && state != EXYNOS_CAMERA_STATE_FLUSH
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }
        break;
    case EXYNOS_CAMERA_STATE_CONFIGURED:
        if (state != EXYNOS_CAMERA_STATE_CONFIGURED
            && state != EXYNOS_CAMERA_STATE_START
            && state != EXYNOS_CAMERA_STATE_FLUSH
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }
        break;
    case EXYNOS_CAMERA_STATE_START:
        if (state == EXYNOS_CAMERA_STATE_FLUSH) {
            /* When FLUSH state is requested on START state, it must wait state to transit to RUN.
            * On FLUSH state, Frame factory will be stopped.
            * On START state, Frame factory will be run.
            * They must be done sequencially
            */

            /*
             * HACK
             * The original retryCount is 10 (1s),
             * Due to the sensor entry time issues, it has increased to 150 (15s).
             * int retryCount = 10;
             */
            int retryCount = 150;
            while (m_state == EXYNOS_CAMERA_STATE_START && retryCount-- > 0) {
                CLOGI("Wait to RUN state");
                m_stateLock.unlock();
                /* Sleep 100ms */
                usleep(100000);
                m_stateLock.lock();
            }

            if (retryCount < 0 && m_state == EXYNOS_CAMERA_STATE_START) {
                android_printAssert(NULL, LOG_TAG,
                        "ASSERT(%s):Failed to wait EXYNOS_CAMERA_RUN state. curState %d newState %d",
                        __FUNCTION__, m_state, state);
            }
        } else if (state != EXYNOS_CAMERA_STATE_RUN
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }

        break;
    case EXYNOS_CAMERA_STATE_RUN:
        if (state != EXYNOS_CAMERA_STATE_FLUSH
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }
        break;
    case EXYNOS_CAMERA_STATE_FLUSH:
        if (state != EXYNOS_CAMERA_STATE_CONFIGURED
            && state != EXYNOS_CAMERA_STATE_ERROR) {
            ret = INVALID_OPERATION;
            goto ERR_EXIT;
        }
        break;
    case EXYNOS_CAMERA_STATE_ERROR:
        if (state != EXYNOS_CAMERA_STATE_ERROR) {
            /*
             * Camera HAL device ops functions that have a return value will all return -ENODEV / NULL
             * in case of a serious error.
             * This means the device cannot continue operation, and must be closed by the framework.
             */
            ret = NO_INIT;
            goto ERR_EXIT;
        }
        break;
    default:
        CLOGW("Invalid curState %d maxValue %d", state, EXYNOS_CAMERA_STATE_MAX);
        ret = INVALID_OPERATION;
        goto ERR_EXIT;
    }

    m_state = state;
    return NO_ERROR;

ERR_EXIT:
    CLOGE("Invalid state transition. curState %d newState %d", m_state, state);
    return ret;
}

ExynosCamera::exynos_camera_state_t ExynosCamera::m_getState(void)
{
    Mutex::Autolock lock(m_stateLock);

    return m_state;
}

status_t ExynosCamera::m_checkRestartStream(ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    bool restart = m_parameters[m_cameraId]->getRestartStream();

    if (restart) {
        CLOGD("Internal restart stream request[R%d]",
             request->getKey());
        ret = m_restartStreamInternal();
        if (ret != NO_ERROR) {
            CLOGE("m_restartStreamInternal failed [%d]", ret);
            return ret;
        }

        m_requestMgr->registerToServiceList(request);
        m_parameters[m_cameraId]->setRestartStream(false);
    }

    return ret;
}

#ifdef FPS_CHECK
void ExynosCamera::m_debugFpsCheck(enum pipeline pipeId)
{
    uint32_t debugMaxPipeNum = sizeof(m_debugFpsCount);
    uint32_t id = pipeId % debugMaxPipeNum;
    uint32_t debugStartCount = 1, debugEndCount = 30;

    m_debugFpsCount[id]++;

    if (m_debugFpsCount[id] == debugStartCount) {
        m_debugFpsTimer[id].start();
    }

    if (m_debugFpsCount[id] == debugStartCount + debugEndCount) {
        m_debugFpsTimer[id].stop();
        long long durationTime = m_debugFpsTimer[id].durationMsecs();
        CLOGI("FPS_CHECK(id:%d), duration %lld / 30 = %lld ms. %lld fps",
                pipeId, durationTime, durationTime / debugEndCount, 1000 / (durationTime / debugEndCount));
        m_debugFpsCount[id] = 0;
    }
}
#endif

#ifdef SUPPORT_HW_GDC
bool ExynosCamera::m_gdcThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;
    bool loop = true;
    int outputPortId = -1;
    int srcPipeId = -1;
    int srcNodeIndex = -1;
    int dstPipeId = -1;
    int srcFmt = -1;
    int dstFmt = -1;
    ExynosRect gdcSrcRect[2];
    ExynosRect gdcDstRect;
    ExynosCameraBuffer gdcSrcBuf;
    ExynosCameraBuffer gdcDstBuf;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    struct camera2_stream *meta = NULL;
    camera2_node_group bcropNodeGroupInfo;
    int perframeInfoIndex = -1;
    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif

    outputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
    if (outputPortId < 0) {
        CLOGE("There is NO VIDEO stream");
        goto exit_thread_loop;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
            srcPipeId = PIPE_MCSC;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
            srcPipeId = PIPE_DCP;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
            srcPipeId = PIPE_ISP;
        } else {
            srcPipeId = PIPE_3AA;
        }
    } else
#endif
    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
        srcPipeId = PIPE_MCSC;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
        srcPipeId = PIPE_ISP;
    } else {
        srcPipeId = PIPE_3AA;
    }

    srcPipeId = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) ? PIPE_ISP : PIPE_3AA;
    srcNodeIndex = factory->getNodeType(PIPE_MCSC0 + outputPortId);
    dstPipeId = PIPE_GDC;
    srcFmt = m_parameters[m_cameraId]->getYuvFormat(outputPortId);
    dstFmt = m_parameters[m_cameraId]->getYuvFormat(outputPortId);

    CLOGV("Wait gdcQ");
    ret = m_gdcQ->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT) {
        CLOGW("Wait timeout to gdcQ");
        goto retry_thread_loop;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ to gdcQ");
        goto retry_thread_loop;
    } else if (frame == NULL) {
        CLOGE("Frame is NULL");
        goto retry_thread_loop;
    }

    /* Set Source Rect */
    ret = frame->getDstBuffer(srcPipeId, &gdcSrcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getDstBuffer. pipeId %d nodeIndex %d ret %d",
                frame->getFrameCount(),
                srcPipeId, srcNodeIndex, ret);
        goto retry_thread_loop;
    }

    meta = (struct camera2_stream *)gdcSrcBuf.addr[gdcSrcBuf.getMetaPlaneIndex()];
    if (meta == NULL) {
        CLOGE("[F%d B%d]meta is NULL. pipeId %d nodeIndex %d",
                frame->getFrameCount(), gdcSrcBuf.index,
                srcPipeId, srcNodeIndex);
        goto retry_thread_loop;
    }

    gdcSrcRect[0].x             = meta->output_crop_region[0];
    gdcSrcRect[0].y             = meta->output_crop_region[1];
    gdcSrcRect[0].w             = meta->output_crop_region[2];
    gdcSrcRect[0].h             = meta->output_crop_region[3];
    gdcSrcRect[0].fullW         = meta->output_crop_region[2];
    gdcSrcRect[0].fullH         = meta->output_crop_region[3];
    gdcSrcRect[0].colorFormat   = srcFmt;

    /* Set Bcrop Rect */
    perframeInfoIndex = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M) ? PERFRAME_INFO_3AA : PERFRAME_INFO_FLITE;
    frame->getNodeGroupInfo(&bcropNodeGroupInfo, perframeInfoIndex);
    m_parameters[m_cameraId]->getMaxSensorSize(&gdcSrcRect[1].fullW, &gdcSrcRect[1].fullH);

    gdcSrcRect[1].x             = bcropNodeGroupInfo.leader.input.cropRegion[0];
    gdcSrcRect[1].y             = bcropNodeGroupInfo.leader.input.cropRegion[1];
    gdcSrcRect[1].w             = bcropNodeGroupInfo.leader.input.cropRegion[2];
    gdcSrcRect[1].h             = bcropNodeGroupInfo.leader.input.cropRegion[3];
    gdcSrcRect[1].colorFormat   = srcFmt;

    /* Set Destination Rect */
    gdcDstRect.x            = 0;
    gdcDstRect.y            = 0;
    gdcDstRect.w            = gdcSrcRect[0].w;
    gdcDstRect.h            = gdcSrcRect[0].h;
    gdcDstRect.fullW        = gdcSrcRect[0].fullW;
    gdcDstRect.fullH        = gdcSrcRect[0].fullH;
    gdcDstRect.colorFormat  = dstFmt;

    for (int i = 0; i < 2; i++) {
        ret = frame->setSrcRect(dstPipeId, gdcSrcRect[i], i);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to setSrcRect. pipeId %d index %d ret %d",
                    frame->getFrameCount(), dstPipeId, i, ret);
            goto retry_thread_loop;
        }
    }

    ret = frame->setDstRect(dstPipeId, gdcDstRect);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to setDstRect. pipeId %d ret %d",
                frame->getFrameCount(), dstPipeId, ret);
        goto retry_thread_loop;
    }

    ret = m_setSrcBuffer(dstPipeId, frame, &gdcSrcBuf);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to setSrcBuffer. pipeId %d ret %d",
                frame->getFrameCount(), gdcSrcBuf.index,
                dstPipeId, ret);
        goto retry_thread_loop;
    }

    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[dstPipeId], dstPipeId);
    factory->pushFrameToPipe(frame, dstPipeId);

    return true;

retry_thread_loop:
    if (m_gdcQ->getSizeOfProcessQ() > 0) {
        return true;
    }

exit_thread_loop:
    return false;
}
#endif

}; /* namespace android */
