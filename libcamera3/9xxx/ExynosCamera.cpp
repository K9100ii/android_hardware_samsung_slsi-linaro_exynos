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
#include "ExynosCameraProperty.h"

namespace android {

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif

ExynosCamera::ExynosCamera(int mainCameraId, __unused int subCameraId,  int scenario, camera_metadata_t **info):
    m_requestMgr(NULL),
    m_streamManager(NULL)
{
    BUILD_DATE();

    /* Camera Scenario */
    m_scenario = scenario;
    /* Main Camera */
    m_cameraId = m_cameraIds[0] = mainCameraId;

    switch (m_cameraId) {
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
        CLOGE("Invalid camera ID(%d)", m_cameraId);
        break;
    }

#ifdef USE_DUAL_CAMERA
    /* Sub Camera */
    if (subCameraId >= 0) {
        m_cameraIds[1] = subCameraId;
        CLOGI("Dual cameara enabled! CameraId (%d & %d) m_scenario(%d)",
               m_cameraIds[0], m_cameraIds[1], m_scenario);
    } else {
        m_cameraIds[1] = 0;
        CLOGI("Single cameara enabled! CameraId (%d) m_scenario(%d)", m_cameraIds[0], m_scenario);
    }
#else
    CLOGI("Single cameara enabled! CameraId (%d) m_scenario(%d)", m_cameraIds[0], m_scenario);
#endif

    /* Initialize pointer variables */
    m_ionAllocator = NULL;

    m_bufferSupplier = NULL;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_parameters[i] = NULL;
        m_captureSelector[i] = NULL;
        m_activityControl[i] = NULL;

        m_currentPreviewShot[i] = NULL;
        m_currentInternalShot[i] = NULL;
        m_currentCaptureShot[i] = NULL;
        m_currentVisionShot[i] = NULL;
     }
    m_captureZslSelector = NULL;

    m_vendorSpecificPreConstructor(m_cameraId, m_scenario);

    m_configurations = new ExynosCameraConfigurations(m_cameraIds[0], m_scenario);

#ifdef USE_DEBUG_PROPERTY
    ExynosCameraProperty property;
    bool obteEnable = false;
    status_t ret = property.get(ExynosCameraProperty::TUNING_OBTE_ENABLE, LOG_TAG, obteEnable);
    if (ret == NO_ERROR) {
        m_configurations->setMode(CONFIGURATION_OBTE_MODE, obteEnable);
    }
#endif

    /* Create related classes */
    switch (m_scenario) {
    case SCENARIO_SECURE:
        m_parameters[m_cameraId] = new ExynosCameraParameters(m_cameraId, m_scenario, m_configurations);
        m_activityControl[m_cameraId] = m_parameters[m_cameraId]->getActivityControl();

        m_currentPreviewShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentPreviewShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));

        m_currentInternalShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentInternalShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));

        m_currentVisionShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentVisionShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));
        break;
#ifdef USE_DUAL_CAMERA
    case SCENARIO_DUAL_REAR_ZOOM:
    case SCENARIO_DUAL_REAR_PORTRAIT:
        m_parameters[m_cameraIds[1]] = new ExynosCameraParameters(m_cameraIds[1], m_scenario, m_configurations);
        m_activityControl[m_cameraIds[1]] = m_parameters[m_cameraIds[1]]->getActivityControl();

        m_currentPreviewShot[m_cameraIds[1]] = new struct camera2_shot_ext;
        memset(m_currentPreviewShot[m_cameraIds[1]], 0x00, sizeof(struct camera2_shot_ext));

        m_currentInternalShot[m_cameraIds[1]] = new struct camera2_shot_ext;
        memset(m_currentInternalShot[m_cameraIds[1]], 0x00, sizeof(struct camera2_shot_ext));

        m_currentCaptureShot[m_cameraIds[1]] = new struct camera2_shot_ext;
        memset(m_currentCaptureShot[m_cameraIds[1]], 0x00, sizeof(struct camera2_shot_ext));
        /* No break: m_parameters[0] is same with normal */
#endif
    case SCENARIO_NORMAL:
    default:
        m_parameters[m_cameraId] = new ExynosCameraParameters(m_cameraId, m_scenario, m_configurations);
        m_activityControl[m_cameraId] = m_parameters[m_cameraId]->getActivityControl();

        m_currentPreviewShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentPreviewShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));

        m_currentInternalShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentInternalShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));

        m_currentCaptureShot[m_cameraId] = new struct camera2_shot_ext;
        memset(m_currentCaptureShot[m_cameraId], 0x00, sizeof(struct camera2_shot_ext));
        break;
    }

#ifdef USE_DUAL_CAMERA
    m_metadataConverter = new ExynosCameraMetadataConverter(m_cameraId, m_configurations, m_parameters[m_cameraId],
                                                            m_parameters[m_cameraIds[1]]);
#else
    m_metadataConverter = new ExynosCameraMetadataConverter(m_cameraId, m_configurations, m_parameters[m_cameraId]);
#endif
    m_requestMgr = new ExynosCameraRequestManager(m_cameraId, m_configurations);
    m_requestMgr->setMetaDataConverter(m_metadataConverter);

    /* Create managers */
    m_createManagers();

    /* Create queue for preview path. If you want to control pipeDone in ExynosCamera, try to create frame queue here */
    m_shotDoneQ = new ExynosCameraList<ExynosCameraFrameSP_sptr_t>();
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

    /* Create threads */
    m_createThreads();

    if (m_configurations->getMode(CONFIGURATION_GMV_MODE) == true) {
        m_pipeFrameDoneQ[PIPE_GMV] = new frame_queue_t(m_previewStreamGMVThread);
    }
    m_pipeFrameDoneQ[PIPE_VRA] = new frame_queue_t(m_previewStreamVRAThread);
#ifdef SUPPORT_HFD
    if (m_parameters[m_cameraId]->getHfdMode() == true) {
        m_pipeFrameDoneQ[PIPE_HFD] = new frame_queue_t(m_previewStreamHFDThread);
    }
#endif

#ifdef TIME_LOGGER_LAUNCH_ENABLE
    TIME_LOGGER_INIT(m_cameraId);
#endif

#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        m_slaveShotDoneQ = new ExynosCameraList<ExynosCameraFrameSP_sptr_t>();
        m_dualStandbyTriggerQ = new ExynosCameraList<dual_standby_trigger_t>();
        m_pipeFrameDoneQ[PIPE_SYNC] = new frame_queue_t;
        m_pipeFrameDoneQ[PIPE_FUSION] = new frame_queue_t;
        m_dualTransitionCount = 0;
        m_dualCaptureLockCount = 0;
        m_dualMultiCaptureLockflag = false;
        m_earlyTriggerRequestKey = 0;
        m_flagFinishDualFrameFactoryStartThread = false;
    } else {
        m_slaveShotDoneQ = NULL;
        m_dualStandbyTriggerQ = NULL;
        m_pipeFrameDoneQ[PIPE_SYNC] = NULL;
        m_pipeFrameDoneQ[PIPE_FUSION] = NULL;
        m_dualTransitionCount = 0;
        m_dualCaptureLockCount = 0;
        m_dualMultiCaptureLockflag = false;
        m_earlyTriggerRequestKey = 0;
        m_flagFinishDualFrameFactoryStartThread = false;
    }
#endif

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    /* Create queue for capture path */
    m_yuvCaptureDoneQ = new frame_queue_t;
    m_yuvCaptureDoneQ->setWaitTime(2000000000);
    m_reprocessingDoneQ = new frame_queue_t;
    m_reprocessingDoneQ->setWaitTime(2000000000);
    m_bayerStreamQ = new frame_queue_t;
    m_bayerStreamQ->setWaitTime(2000000000);

#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        m_slaveShotDoneQ->setWaitTime(4000000000);
        m_selectDualSlaveBayerQ = new frame_queue_t();
    } else {
        m_selectDualSlaveBayerQ = NULL;
    }
#endif
    m_shotDoneQ->setWaitTime(4000000000);

    m_frameFactoryQ = new framefactory3_queue_t;
    m_selectBayerQ = new frame_queue_t();
    m_captureQ = new frame_queue_t(m_captureThread);
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    m_reverseProcessingBayerQ = new frame_queue_t(m_reverseProcessingBayerThread);
#endif
#ifdef SUPPORT_HW_GDC
    m_gdcQ = new frame_queue_t(m_gdcThread);
#endif

    /* construct static meta data information */
    if (*info == NULL) {
    if (ExynosCameraMetadataConverter::constructStaticInfo(-1, m_cameraId, m_scenario, info, NULL))
            CLOGE("Create static meta data failed!!");
    }

    m_metadataConverter->setStaticInfo(m_cameraId, *info);

    m_streamManager->setYuvStreamMaxCount(m_parameters[m_cameraId]->getYuvStreamMaxNum());

    m_setFrameManager();

    m_setConfigInform();

    /* init infomation of fd orientation*/
    m_configurations->setModeValue(CONFIGURATION_DEVICE_ORIENTATION, 0);
    m_configurations->setModeValue(CONFIGURATION_FD_ORIENTATION, 0);
    ExynosCameraActivityUCTL *uctlMgr = m_activityControl[m_cameraId]->getUCTLMgr();
    if (uctlMgr != NULL) {
        uctlMgr->setDeviceRotation(m_configurations->getModeValue(CONFIGURATION_FD_ORIENTATION));
    }

    m_state = EXYNOS_CAMERA_STATE_OPEN;

    m_visionFps = 0;

    m_flushLockWait = false;
    m_captureStreamExist = false;
    m_rawStreamExist = false;
    m_videoStreamExist = false;

    m_recordingEnabled = false;
    m_prevStreamBit = 0;

    m_firstRequestFrameNumber = 0;
    m_internalFrameCount = 1;
    m_isNeedRequestFrame = false;
#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif
    m_lastFrametime = 0;
    m_prepareFrameCount = 0;
#ifdef BUFFER_DUMP
    m_dumpBufferQ = new buffer_dump_info_queue_t(m_dumpThread);
#endif

    m_ionClient = exynos_ion_open();
    if (m_ionClient < 0) {
        ALOGE("ERR(%s):m_ionClient ion_open() fail", __func__);
        m_ionClient = -1;
    }

    m_framefactoryCreateResult = NO_ERROR;
    m_flagFirstPreviewTimerOn = false;
    m_previewCallbackPP = NULL;

    /* Construct vendor */
    m_vendorSpecificConstructor();
}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    /* Internal buffers */
#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = (getCameraId() == CAMERA_ID_FRONT) ? FRONT_NUM_SENSOR_BUFFERS : DUAL_NUM_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = (getCameraId() == CAMERA_ID_FRONT) ? FRONT_NUM_3AA_BUFFERS : DUAL_NUM_3AA_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = DUAL_NUM_HW_DIS_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.dual_num_fusion_buffers = DUAL_NUM_SYNC_FUSION_BUFFERS;
    } else
#endif
    {
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = (getCameraId() == CAMERA_ID_FRONT) ? FRONT_NUM_SENSOR_BUFFERS : NUM_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = (getCameraId() == CAMERA_ID_FRONT) ? FRONT_NUM_3AA_BUFFERS : NUM_3AA_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    }
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_preview_buffers = DUAL_NUM_REQUEST_PREVIEW_BUFFER;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_callback_buffers = DUAL_NUM_REQUEST_CALLBACK_BUFFER;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_video_buffers = DUAL_NUM_REQUEST_VIDEO_BUFFER;
    } else
#endif
    {
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_callback_buffers = NUM_REQUEST_CALLBACK_BUFFER;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_video_buffers = NUM_REQUEST_VIDEO_BUFFER;
    }
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_raw_buffers = NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_bayer_buffers = NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_burst_capture_buffers = NUM_REQUEST_BURST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_request_capture_buffers = NUM_REQUEST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_batch_buffers = 1;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_PREPARE_COUNT;

    /* Config HIGH_SPEED 60 buffer & pipe info */
    /* Internal buffers */
#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers = DUAL_NUM_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = DUAL_NUM_3AA_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = DUAL_NUM_HW_DIS_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.dual_num_fusion_buffers = DUAL_NUM_SYNC_FUSION_BUFFERS;
    } else
#endif
    {
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers = NUM_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = NUM_3AA_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    }
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
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
#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_preview_buffers = DUAL_NUM_REQUEST_PREVIEW_BUFFER;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_callback_buffers = DUAL_NUM_REQUEST_CALLBACK_BUFFER;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_video_buffers = DUAL_NUM_REQUEST_VIDEO_BUFFER;
    } else
#endif
    {
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_preview_buffers = NUM_REQUEST_PREVIEW_BUFFER;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_callback_buffers = NUM_REQUEST_CALLBACK_BUFFER;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_video_buffers = NUM_REQUEST_VIDEO_BUFFER;
    }
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_raw_buffers = NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_bayer_buffers = NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_burst_capture_buffers = NUM_REQUEST_BURST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_request_capture_buffers = NUM_REQUEST_CAPTURE_BUFFER;
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
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_bayer_buffers = FPS120_NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_preview_buffers = FPS120_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_callback_buffers = FPS120_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_video_buffers = FPS120_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_burst_capture_buffers = FPS120_NUM_REQUEST_BURST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_request_capture_buffers = FPS120_NUM_REQUEST_CAPTURE_BUFFER;
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
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_bayer_buffers = FPS240_NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_preview_buffers = FPS240_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_callback_buffers = FPS240_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_video_buffers = FPS240_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_burst_capture_buffers = FPS240_NUM_REQUEST_BURST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_request_capture_buffers = FPS240_NUM_REQUEST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_batch_buffers = 4;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_PREPARE_COUNT;

    /* Config HIGH_SPEED 480 buffer & pipe info */
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_sensor_buffers = FPS480_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_3aa_buffers = FPS480_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_hwdis_buffers = FPS480_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_raw_buffers = FPS480_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_bayer_buffers = NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_preview_buffers = FPS480_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_callback_buffers = FPS480_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_video_buffers = FPS480_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_request_jpeg_buffers = FPS480_NUM_REQUEST_JPEG_BUFFER;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].bufInfo.num_batch_buffers = 8;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].pipeInfo.prepare[PIPE_FLITE] = FPS480_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_480].pipeInfo.prepare[PIPE_3AA] = FPS480_PIPE_3AA_PREPARE_COUNT;
#endif

#ifdef SAMSUNG_SSM
    /* Internal buffers */
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_fastaestable_buffer = NUM_FASTAESTABLE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_sensor_buffers = FPS240_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_3aa_buffers = FPS240_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_hwdis_buffers = FPS240_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_nv21_picture_buffers = NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
    /* v4l2_reqBuf() values for stream buffers */
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_bayer_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_preview_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_preview_cb_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_recording_buffers = VIDEO_MAX_FRAME;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_picture_buffers = VIDEO_MAX_FRAME;
    /* Required stream buffers for HAL */
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_raw_buffers = FPS240_NUM_REQUEST_RAW_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_bayer_buffers = FPS240_NUM_REQUEST_BAYER_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_preview_buffers = FPS240_NUM_REQUEST_PREVIEW_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_callback_buffers = FPS240_NUM_REQUEST_CALLBACK_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_video_buffers = FPS240_NUM_REQUEST_VIDEO_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_burst_capture_buffers = FPS240_NUM_REQUEST_BURST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_request_capture_buffers = FPS240_NUM_REQUEST_CAPTURE_BUFFER;
    exynosConfig.info[CONFIG_MODE::SSM_240].bufInfo.num_batch_buffers = 4;
    /* Prepare buffers */
    exynosConfig.info[CONFIG_MODE::SSM_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::SSM_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_PREPARE_COUNT;
#endif

    m_configurations->setConfig(&exynosConfig);
    m_exynosconfig = m_configurations->getConfig();

    return NO_ERROR;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainPreviewThread = new mainCameraThread(this, &ExynosCamera::m_mainPreviewThreadFunc, "m_mainPreviewThreadFunc");
    CLOGD("Main Preview thread created");

    m_mainCaptureThread = new mainCameraThread(this, &ExynosCamera::m_mainCaptureThreadFunc, "m_mainCaptureThreadFunc");
    CLOGD("Main Capture thread created");

    /* m_previewStreamXXXThread is for seperated frameDone each own handler */
    m_previewStreamBayerThread = new ExynosCameraStreamThread(this, "PreviewBayerThread", PIPE_FLITE);
    m_previewStreamBayerThread->setFrameDoneQ(m_pipeFrameDoneQ[PIPE_FLITE]);
    CLOGD("Bayer Preview stream thread created");

    m_previewStream3AAThread = new ExynosCameraStreamThread(this, "Preview3AAThread", PIPE_3AA);
    m_previewStream3AAThread->setFrameDoneQ(m_pipeFrameDoneQ[PIPE_3AA]);
    CLOGD("3AA Preview stream thread created!!!");

#ifdef SUPPORT_GMV
    if (m_configurations->getMode(CONFIGURATION_GMV_MODE) == true) {
        m_previewStreamGMVThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamGMVPipeThreadFunc, "PreviewGMVThread");
        CLOGD("GMV Preview stream thread created");
    }
#endif

    m_previewStreamISPThread = new ExynosCameraStreamThread(this, "PreviewISPThread", PIPE_ISP);
    m_previewStreamISPThread->setFrameDoneQ(m_pipeFrameDoneQ[PIPE_ISP]);
    CLOGD("ISP Preview stream thread created");

    m_previewStreamMCSCThread = new ExynosCameraStreamThread(this, "PreviewMCSCThread", PIPE_MCSC);
    m_previewStreamMCSCThread->setFrameDoneQ(m_pipeFrameDoneQ[PIPE_MCSC]);
    CLOGD("MCSC Preview stream thread created");

#ifdef SUPPORT_HW_GDC
    m_previewStreamGDCThread = new ExynosCameraStreamThread(this, "PreviewGDCThread", PIPE_GDC);
    m_previewStreamGDCThread->setFrameDoneQ(m_pipeFrameDoneQ[PIPE_GDC]);

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
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        m_slaveMainThread = new mainCameraThread(this, &ExynosCamera::m_slaveMainThreadFunc, "m_slaveMainThreadFunc");
        CLOGD("Dual Slave Main thread created");

        m_dualStandbyThread = new mainCameraThread(this, &ExynosCamera::m_dualStandbyThreadFunc, "m_setStandbyThreadFunc");
        CLOGD("Dual Standby thread created");

        m_previewStreamSyncThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamSyncPipeThreadFunc, "PreviewSyncThread");
        CLOGD("Sync Preview stream thread created");

        m_previewStreamFusionThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamFusionPipeThreadFunc, "PreviewFusionThread");
        CLOGD("Fusion Preview stream thread created");

        m_dualFrameFactoryStartThread = new mainCameraThread(this, &ExynosCamera::m_dualFrameFactoryStartThreadFunc, "DualFrameFactoryStartThread");
        CLOGD("DualFrameFactoryStartThread created");

        m_selectDualSlaveBayerThread = new mainCameraThread(this, &ExynosCamera::m_selectDualSlaveBayerThreadFunc, "SelectDualSlaveBayerThreadFunc");
        CLOGD("SelectDualSlaveBayerThread created");
    }
#endif

    m_selectBayerThread = new mainCameraThread(this, &ExynosCamera::m_selectBayerThreadFunc, "SelectBayerThreadFunc");
    CLOGD("SelectBayerThread created");

    m_bayerStreamThread = new mainCameraThread(this, &ExynosCamera::m_bayerStreamThreadFunc, "BayerStreamThread");
    CLOGD("Bayer stream thread created");

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

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    m_reverseProcessingBayerThread = new mainCameraThread(this, &ExynosCamera::m_reverseProcessingBayerThreadFunc, "reverseProcessingBayerThread");
    CLOGD("reverseProcessingBayerThread created");
#endif

#ifdef SUPPORT_HW_GDC
    m_gdcThread = new mainCameraThread(this, &ExynosCamera::m_gdcThreadFunc, "GDCThread");
    CLOGD("GDCThread created");
#endif

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "MonitorThread");
    CLOGD("MonitorThread created");

#ifdef BUFFER_DUMP
    m_dumpThread = new mainCameraThread(this, &ExynosCamera::m_dumpThreadFunc, "m_dumpThreadFunc");
    CLOGD("DumpThread created");
#endif

    m_vendorCreateThreads();
}

ExynosCamera::~ExynosCamera()
{
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, DESTRUCTOR_START, 0);
    this->release();

#ifdef TIME_LOGGER_CLOSE_ENABLE
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, DESTRUCTOR_END, 0);
    TIME_LOGGER_SAVE(m_cameraId);
#endif
}

void ExynosCamera::release()
{
    CLOGI("-IN-");

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        ExynosCameraTuningInterface::stop();
    }

    m_vendorSpecificPreDestructor();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, PRE_DESTRUCTOR_END, 0);

    if (m_shotDoneQ != NULL) {
        delete m_shotDoneQ;
        m_shotDoneQ = NULL;
    }

#ifdef USE_DUAL_CAMERA
    if (m_slaveShotDoneQ != NULL) {
        delete m_slaveShotDoneQ;
        m_slaveShotDoneQ = NULL;
    }

    if (m_dualStandbyTriggerQ != NULL) {
        delete m_dualStandbyTriggerQ;
        m_dualStandbyTriggerQ = NULL;
    }

    if (m_selectDualSlaveBayerQ != NULL) {
        delete m_selectDualSlaveBayerQ;
        m_selectDualSlaveBayerQ = NULL;
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

    if (m_bayerStreamQ != NULL) {
        delete m_bayerStreamQ;
        m_bayerStreamQ = NULL;
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
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    if (m_reverseProcessingBayerQ != NULL) {
        delete m_reverseProcessingBayerQ;
        m_reverseProcessingBayerQ = NULL;
    }
#endif
#ifdef SUPPORT_HW_GDC
    if (m_gdcQ != NULL) {
        delete m_gdcQ;
        m_gdcQ = NULL;
    }
#endif
#ifdef BUFFER_DUMP
    if (m_dumpBufferQ != NULL) {
        delete m_dumpBufferQ;
        m_dumpBufferQ = NULL;
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

        if (m_currentPreviewShot[i] != NULL) {
            delete m_currentPreviewShot[i];
            m_currentPreviewShot[i] = NULL;
        }

        if (m_currentInternalShot[i] != NULL) {
            delete m_currentInternalShot[i];
            m_currentInternalShot[i] = NULL;
        }

        if (m_currentCaptureShot[i] != NULL) {
            delete m_currentCaptureShot[i];
            m_currentCaptureShot[i] = NULL;
        }

        if (m_currentVisionShot[i] != NULL) {
            delete m_currentVisionShot[i];
            m_currentVisionShot[i] = NULL;
        }
    }

    if (m_configurations != NULL) {
        delete m_configurations;
        m_configurations = NULL;
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

    if (m_ionClient >= 0) {
        exynos_ion_close(m_ionClient);
        m_ionClient = -1;
    }

    if (m_previewCallbackPP != NULL) {
        if (m_previewCallbackPP->flagCreated() == true) {
            status_t ret = NO_ERROR;
            ret = m_previewCallbackPP->destroy();
            if (ret != NO_ERROR) {
                CLOGE("m_previewCallbackPP->destroy() fail");
            }
        }

        SAFE_DELETE(m_previewCallbackPP);
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_SUPPLIER_DEINIT_JOIN_START, 0);
    m_deinitBufferSupplierThread->requestExitAndWait();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_SUPPLIER_DEINIT_JOIN_END, 0);

    // TODO: clean up
    // m_resultBufferVectorSet
    // m_processList
    // m_postProcessList
    // m_pipeFrameDoneQ

    m_vendorSpecificDestructor();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, POST_DESTRUCTOR_END, 0);

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
            m_captureSelector[m_cameraId] = new ExynosCameraFrameSelector(
                        m_cameraId,
                        m_configurations,
                        m_parameters[m_cameraId],
                        m_bufferSupplier,
                        m_frameMgr);
            ret = m_captureSelector[m_cameraId]->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureSelector setFrameHoldCount(%d) is fail",
                     REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
            && m_captureSelector[m_cameraIds[1]] == NULL) {
            m_captureSelector[m_cameraIds[1]] = new ExynosCameraFrameSelector(
                        m_cameraIds[1],
                        m_configurations,
                        m_parameters[m_cameraIds[1]],
                        m_bufferSupplier,
                        m_frameMgr);
            ret = m_captureSelector[m_cameraIds[1]]->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0) {
                CLOGE("m_captureSelector[%d] setFrameHoldCount(%d) is fail",
                     m_cameraIds[1], REPROCESSING_BAYER_HOLD_COUNT);
            }
        }
#endif

        if (m_captureZslSelector == NULL) {
            m_captureZslSelector = new ExynosCameraFrameSelector(
                        m_cameraId,
                        m_configurations,
                        m_parameters[m_cameraId],
                        m_bufferSupplier,
                        m_frameMgr);
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

status_t ExynosCamera::m_reInit(void)
{
    m_vendorReInit();

    return NO_ERROR;
}

status_t ExynosCamera::construct_default_request_settings(camera_metadata_t **request, int type)
{
    CLOGD("Type %d", type);
    if ((type < 0) || (type >= CAMERA3_TEMPLATE_COUNT)) {
        CLOGE("Unknown request settings template: %d", type);
        return NO_INIT;
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

#ifdef USE_DUAL_CAMERA
int ExynosCamera::getSubCameraId() const
{
    return m_cameraIds[1];
}
#endif

status_t ExynosCamera::setPIPMode(bool enabled)
{
    m_configurations->setMode(CONFIGURATION_PIP_MODE, enabled);
    return NO_ERROR;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::setDualMode(bool enabled)
{
    m_configurations->setMode(CONFIGURATION_DUAL_MODE, enabled);
    return NO_ERROR;
}

bool ExynosCamera::getDualMode(void)
{
    return m_configurations->getMode(CONFIGURATION_DUAL_MODE);
}
#endif

bool ExynosCamera::m_mainPreviewThreadFunc(void)
{
    status_t ret = NO_ERROR;

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory. state(%d)", m_getState());

        if (m_getState() == EXYNOS_CAMERA_STATE_ERROR) {
            return false;
        }

        usleep(1000);
        return true;
    }

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == false) {
        ret = m_createPreviewFrameFunc(REQ_SYNC_WITH_3AA, true /* flagFinishFactoryStart */);
    } else {
        ret = m_createVisionFrameFunc(REQ_SYNC_WITH_3AA, true /* flagFinishFactoryStart */);
    }
    if (ret != NO_ERROR) {
        CLOGE("Failed to createPreviewFrameFunc. Shotdone");
    }

    return true;
}

bool ExynosCamera::m_mainCaptureThreadFunc(void)
{
    status_t ret = NO_ERROR;

    m_reprocessingFrameFactoryStartThread->join();

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory. state(%d)", m_getState());

        if (m_getState() == EXYNOS_CAMERA_STATE_ERROR) {
            return false;
        }

        usleep(10000);
        return true;
    }

    if (m_getSizeFromRequestList(&m_requestCaptureWaitingList, &m_requestCaptureWaitingLock) > 0) {
        ret = m_createCaptureFrameFunc();
        if (ret != NO_ERROR) {
            CLOGE("Failed to createCaptureFrameFunc. Shotdone");
        }

        return true;
    } else {
        return false;
    }
}

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_slaveMainThreadFunc(void)
{
    ExynosCameraFrameSP_sptr_t frame;
    bool loop = false;
    status_t ret = NO_ERROR;
    enum DUAL_OPERATION_MODE dualOperationMode = DUAL_OPERATION_MODE_NONE;
    enum DUAL_OPERATION_MODE earlyDualOperationMode = DUAL_OPERATION_MODE_NONE;

    frame_type_t frameType;
    ExynosCameraFrameFactory *factory;
    factory_handler_t frameCreateHandler;
    ExynosCameraRequestSP_sprt_t request;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGI("Wait to run FrameFactory. state(%d), m_earlyDualOperationMode(%d)",
            m_getState(), m_earlyDualOperationMode);

        if (m_getState() == EXYNOS_CAMERA_STATE_ERROR) {
            return false;
        }

        usleep(1000);
        return true;
    }

    /* 1. Wait the shot done with the latest frame */
    ret = m_slaveShotDoneQ->waitAndPopProcessQ(&frame);
    if (ret < 0) {
        if (ret == TIMED_OUT)
            CLOGW("wait timeout");
        else
            CLOGE("wait and pop fail, ret(%d)", ret);
        loop = true;
        goto p_exit;
    } else {
        Mutex::Autolock lock(m_dualOperationModeLock);

        earlyDualOperationMode = m_earlyDualOperationMode;

        switch (frame->getFrameType()) {
        case FRAME_TYPE_TRANSITION_SLAVE:
            /*
             * The dualOperationMode might have not been changed by my standby.
             * So I want to refer to m_earlyDualOperationMode in advance.
             */
            dualOperationMode = m_earlyDualOperationMode;
            break;
        case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
            /*
             * Refer to current dualOperationMode
             */
            dualOperationMode = m_configurations->getDualOperationMode();
            break;
        case FRAME_TYPE_PREVIEW_SLAVE:
        case FRAME_TYPE_INTERNAL_SLAVE:
            /*
             * Refer to current dualOperationMode
             * But, in this generation of frame,
             * DUAL_SLAVE frame may be created in m_createPreviewFrameFunc()
             */
            dualOperationMode = m_configurations->getDualOperationMode();
            break;
        default:
            CLOG_ASSERT("invalid frameType(%d)", frame->getFrameType());
            break;
        }
    }


    CLOGV("[F%d, T%d] dualOperationMode: %d / %d",
            frame->getFrameCount(), frame->getFrameType(),
            m_earlyDualOperationMode, m_configurations->getDualOperationMode());


    switch (dualOperationMode) {
    case DUAL_OPERATION_MODE_MASTER:
        if (earlyDualOperationMode == DUAL_OPERATION_MODE_SYNC &&
                m_parameters[m_cameraIds[1]]->getStandbyState() == DUAL_STANDBY_STATE_OFF) {
            /* create slave frame for sync mode */
            frameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            frameCreateHandler = factory->getFrameCreateHandler();

            /* get the latest request */
            m_latestRequestListLock.lock();
            if (m_essentialRequestList.size() > 0) {
                r = m_essentialRequestList.begin();
                request = *r;
                m_essentialRequestList.erase(r);
            } else {
                r = m_latestRequestList.begin();
                request = *r;
            }
            m_latestRequestListLock.unlock();

            if (request != NULL) {
                if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    (this->*frameCreateHandler)(request, factory, frameType);
                } else {
                    m_createInternalFrameFunc(NULL, true, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
                }
            } else {
                m_createInternalFrameFunc(NULL, true, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
            }
        }
        loop = true;
        goto p_exit;
    case DUAL_OPERATION_MODE_SLAVE:
        ret = m_createPreviewFrameFunc(REQ_SYNC_NONE, true /* flagFinishFactoryStart */);
        loop = true;
        goto p_exit;
    case DUAL_OPERATION_MODE_SYNC:
        /* create slave frame for sync mode */
        frameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
        factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        frameCreateHandler = factory->getFrameCreateHandler();

        /* get the latest request */
        m_latestRequestListLock.lock();
        if (m_essentialRequestList.size() > 0) {
            r = m_essentialRequestList.begin();
            request = *r;
            m_essentialRequestList.erase(r);
        } else {
            r = m_latestRequestList.begin();
            request = *r;
        }
        m_latestRequestListLock.unlock();

        if (request != NULL) {
            if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                (this->*frameCreateHandler)(request, factory, frameType);
            } else {
                m_createInternalFrameFunc(NULL, true, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
            }
        } else {
            m_createInternalFrameFunc(NULL, true, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
        }

        loop = true;
        goto p_exit;
    default:
        goto p_exit;
    }

p_exit:

    return loop;
}
#endif

status_t ExynosCamera::m_createPreviewFrameFunc(enum Request_Sync_Type syncType, __unused bool flagFinishFactoryStart)
{
    status_t ret = NO_ERROR;
    status_t waitRet = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    FrameFactoryList previewFactoryAddrList;
    ExynosCameraFrameFactory *factory = NULL;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;
    frame_type_t frameType = FRAME_TYPE_PREVIEW;
    uint32_t waitingListSize;
    uint32_t requiredRequestCount = -1;
    ExynosCameraFrameSP_sptr_t reqSyncFrame = NULL;
#ifdef USE_DUAL_CAMERA
    ExynosCameraFrameFactory *subFactory = NULL;
    frame_type_t subFrameType = FRAME_TYPE_PREVIEW;
    enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();
    int32_t dualOperationModeLockCount = 0;
    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
#endif

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::[R%d] generate request frame", request->getKey());
#endif

    waitRet = m_waitShotDone(syncType, reqSyncFrame);

#ifdef USE_DUAL_CAMERA
    /* delay this signal after updating dualOperationMode */
    if (isDualMode == false)
        m_captureResultDoneCondition.signal();
#else
    m_captureResultDoneCondition.signal();
#endif

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

            /* 2. Update the entire shot_ext structure */
            service_shot_ext = request->getServiceShot();
            if (service_shot_ext == NULL) {
                CLOGE("[R%d] Get service shot is failed", request->getKey());
            } else {
                m_latestRequestListLock.lock();
                m_latestRequestList.clear();
                m_latestRequestList.push_front(request);
                m_latestRequestListLock.unlock();
            }

            /* 3. Initial (SENSOR_REQUEST_DELAY + 1) frames must be internal frame to
                * secure frame margin for sensor control.
                * If sensor control delay is 0, every initial frames must be request frame.
                */
            if (m_parameters[m_cameraId]->getSensorControlDelay() > 0
                && m_internalFrameCount < (m_parameters[m_cameraId]->getSensorControlDelay() + 2)) {
                m_isNeedRequestFrame = false;
                request = NULL;
                CLOGD("[F%d]Create Initial Frame", m_internalFrameCount);
            } else {
#ifdef USE_DUAL_CAMERA
                if (isDualMode == true) {
                    ret = m_checkDualOperationMode(request, true, false, flagFinishFactoryStart);
                    if (ret != NO_ERROR) {
                        CLOGE("m_checkDualOperationMode fail! flagFinishFactoryStart(%d) ret(%d)", flagFinishFactoryStart, ret);
                    }

                    dualOperationMode = m_configurations->getDualOperationMode();
                    dualOperationModeLockCount = m_configurations->getDualOperationModeLockCount();

                    switch (dualOperationMode) {
                    case DUAL_OPERATION_MODE_SLAVE:
                        /* source from master */
                        if (syncType == REQ_SYNC_WITH_3AA) {
                            if (reqSyncFrame != NULL) {
                                /*
                                 * Early SensorStandby-off case in MASTER only.
                                 * The request to trigger Early SensorStandby-off has not been yet arrived here.
                                 * So we generate Transition frame to keep the count of queued buffers.
                                 * We don't care Early SensorStandby-off in SLAVE cause of independent
                                 * slaveMainThread.
                                 */
                                if (m_earlyTriggerRequestKey > request->getKey() ||
                                        dualOperationModeLockCount > 0) {
                                    CLOGI("generate transition frame more due to early sensorStandby off [%d, %d] lockCnt(%d)",
                                            m_earlyTriggerRequestKey,
                                            request->getKey(),
                                            dualOperationModeLockCount);
                                    ret = m_createInternalFrameFunc(NULL, flagFinishFactoryStart, REQ_SYNC_NONE, FRAME_TYPE_TRANSITION);
                                    if (ret != NO_ERROR) {
                                        CLOGE("Failed to createInternalFrameFunc(%d)", ret);
                                    }
                                } else {
                                    CLOGW("[R%d F%d T%d]skip this request for slave",
                                            request->getKey(),
                                            reqSyncFrame->getFrameCount(),
                                            reqSyncFrame->getFrameType());
                                }
                            } else {
                                CLOGW("skip this request for slave");
                            }
                            return ret;
                        }
                        break;
                    case DUAL_OPERATION_MODE_SYNC:
                    case DUAL_OPERATION_MODE_MASTER:
                        /* source from slave */
                        if (syncType == REQ_SYNC_NONE && flagFinishFactoryStart == true) {
                            if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                                /* create slave frame for sync mode */
                                frameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
                                factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                                frameCreateHandler = factory->getFrameCreateHandler();

                                (this->*frameCreateHandler)(request, factory, frameType);
                            } else {
                                m_createInternalFrameFunc(NULL, flagFinishFactoryStart, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
                            }

                            return ret;
                        }
                        break;
                    default:
                        break;
                    }
                }
#endif

#ifdef USE_DUAL_CAMERA
                /* save the latest request */
                m_latestRequestListLock.lock();
                if (isDualMode == true
                    && m_isRequestEssential(request) == true) {
                    if (m_essentialRequestList.size() > 10) {
                        CLOGW("Essential request list(%d)", m_essentialRequestList.size());
                    }
                    m_essentialRequestList.push_back(request);
                }
                m_latestRequestListLock.unlock();
#endif
                m_requestPreviewWaitingList.erase(r);
            }
        }
    }

#ifdef USE_DUAL_CAMERA
    if (isDualMode == true)
        m_captureResultDoneCondition.signal();
#endif

    CLOGV("Create New Frame %d needRequestFrame %d waitingSize %d",
            m_internalFrameCount, m_isNeedRequestFrame, waitingListSize);

    /* 4. Select the frame creation logic between request frame and internal frame */
    if (m_isNeedRequestFrame == true) {
        previewFactoryAddrList.clear();
        request->getFactoryAddrList(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, &previewFactoryAddrList);
        m_configurations->updateMetaParameter(request->getMetaParameters());

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
                    case FRAME_FACTORY_TYPE_JPEG_REPROCESSING:
                        frameType = FRAME_TYPE_JPEG_REPROCESSING;
                        break;
                    case FRAME_FACTORY_TYPE_VISION:
                        frameType = FRAME_TYPE_VISION;
                        break;
                    default:
                        CLOGE("[R%d] Factory type is not available", request->getKey());
                        break;
                }

#ifdef USE_DUAL_CAMERA
                if (isDualMode == true) {
                    switch (factory->getFactoryType()) {
                    case FRAME_FACTORY_TYPE_CAPTURE_PREVIEW:
                        if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
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
                            frameType = FRAME_TYPE_REPROCESSING_SLAVE;
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
            frame_type_t internalFrameType = FRAME_TYPE_INTERNAL;
#ifdef USE_DUAL_CAMERA
            if (isDualMode == true && dualOperationMode == DUAL_OPERATION_MODE_SLAVE &&
                    syncType == REQ_SYNC_NONE) {
                internalFrameType = FRAME_TYPE_INTERNAL_SLAVE;
            }
#endif
            m_createInternalFrameFunc(request, flagFinishFactoryStart, syncType, internalFrameType);
        }
    } else {
        frame_type_t internalFrameType = FRAME_TYPE_INTERNAL;
#ifdef USE_DUAL_CAMERA
        if (isDualMode == true && dualOperationMode == DUAL_OPERATION_MODE_SLAVE &&
                syncType == REQ_SYNC_NONE) {
            internalFrameType = FRAME_TYPE_INTERNAL_SLAVE;
        }
#endif
        m_createInternalFrameFunc(NULL, flagFinishFactoryStart, syncType, internalFrameType);
    }

    previewFactoryAddrList.clear();

    return ret;
}

status_t ExynosCamera::m_createVisionFrameFunc(enum Request_Sync_Type syncType, __unused bool flagFinishFactoryStart)
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
    ExynosCameraFrameSP_sptr_t reqSyncFrame = NULL;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::[R%d] generate request frame", request->getKey());
#endif

    m_waitShotDone(syncType, reqSyncFrame);
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
                m_latestRequestListLock.lock();
                m_latestRequestList.clear();
                m_latestRequestList.push_front(request);
                m_latestRequestListLock.unlock();
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
                    case FRAME_FACTORY_TYPE_JPEG_REPROCESSING:
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
            m_createInternalFrameFunc(request, flagFinishFactoryStart, syncType);
        }
    } else {
        m_createInternalFrameFunc(NULL, flagFinishFactoryStart, syncType);
    }

    visionFactoryAddrList.clear();

    return ret;
}

status_t ExynosCamera::m_waitShotDone(enum Request_Sync_Type syncType, ExynosCameraFrameSP_dptr_t reqSyncFrame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    if (syncType == REQ_SYNC_WITH_3AA && m_shotDoneQ != NULL) {
        /* 1. Wait the shot done with the latest framecount */
        ret = m_shotDoneQ->waitAndPopProcessQ(&frame);
        if (ret < 0) {
            if (ret == TIMED_OUT)
                CLOGW("wait timeout");
            else
                CLOGE("wait and pop fail, ret(%d)", ret);
            return ret;
        }
    }

    reqSyncFrame = frame;

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
    request->setRequestUnlock();

    /* CameraMetadata will be released by RequestManager
     * to keep the order of request
     */
    requestResult->frame_number = frameNumber;
    requestResult->result = NULL;
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

    CLOGV("[R%d] SHUTTER frame t(%ju)", requestKey, timeStamp);

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
    } else if (list->size() > MAX_BUFFERS) {
        CLOGW("There are too many frames(%zu) in this list. frame(F%d, T%d)",
                list->size(), frame->getFrameCount(), frame->getFrameType());
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
            if (curFrame->getFrameType() == FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
                || curFrame->getFrameType() == FRAME_TYPE_INTERNAL_SLAVE
#endif
                ) {
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
    if (frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        && frame->getFrameType() != FRAME_TYPE_INTERNAL_SLAVE
#endif
        ) {
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
            CLOGV("remove request key(%d), fcount(%d)", curRequest->getKey(), curRequest->getFrameCount());
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
            /* Temp code */
            if (m_configurations->getSamsungCamera() != true) {
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

    CLOGV("[R%d F%d S%d] buffer status(%d)",
            request->getKey(), request->getFrameCount(), streamId, *status);

    return ret;
}

void ExynosCamera::m_checkUpdateResult(ExynosCameraFrameSP_sptr_t frame, uint32_t streamConfigBit)
{
    enum FRAME_TYPE frameType;
    bool previewFlag = false;
    bool captureFlag= false;
    bool zslInputFlag = false;
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
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_PREVIEW_VIDEO);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_ZSL_OUTPUT);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_DEPTHMAP);
            previewFlag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;

            if (previewFlag == true) {
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
            }

            targetBit = 0;
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_RAW);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_ZSL_INPUT);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_JPEG);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_CALLBACK_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_DEPTHMAP_STALL);
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_THUMBNAIL_CALLBACK);
            captureFlag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;

            if (captureFlag == false) {
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
            }

            break;
        case FRAME_TYPE_REPROCESSING:
        case FRAME_TYPE_JPEG_REPROCESSING:
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_REPROCESSING_SLAVE:
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
            zslInputFlag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, SET_BIT(HAL_STREAM_ID_ZSL_INPUT)) > 0) ? true : false;
            captureFlag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;

            /*
             * Partial update will be done by internal frame or preview frame
             * However, if the ZSL input is present, the meta value will not be known in the preview stream.
             * So partial update will be performed after the capture stream is finished.
             */

            if (zslInputFlag == true) {
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
            }

            if (captureFlag == true) {
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
            }

            break;
        case FRAME_TYPE_VISION:
            targetBit = 0;
            SET_STREAM_CONFIG_BIT(targetBit, HAL_STREAM_ID_VISION);
            previewFlag = (COMPARE_STREAM_CONFIG_BIT(streamConfigBit, targetBit) > 0) ? true : false;

            if (previewFlag == true) {
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
            }

            break;
        default:
            CLOGE("[F%d]Unsupported frame type %d", frame->getFrameCount(), frameType);
            break;
    }

    CLOGV("[F%d T%d]flags %d/%d streamConfig %x",
            frame->getFrameCount(),
            frame->getFrameType(),
            previewFlag, captureFlag,
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
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    bool loop = false;
    status_t ret = NO_ERROR;

    ExynosCameraFrameFactory *framefactory = NULL;


    m_framefactoryCreateResult = NO_ERROR;
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
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_START, 0);
        ret = framefactory->create();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_END, 0);
        if (ret != NO_ERROR) {
            CLOGD("failed to create framefactory (%d)", ret);
            m_framefactoryCreateResult = NO_INIT;
            return false;
        }
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

    if (factory == NULL) {
        CLOGE("frameFactory is NULL");
        return BAD_VALUE;
    }

    /* prepare pipes */
    ret = factory->preparePipes();
    if (ret != NO_ERROR) {
        CLOGW("Failed to prepare FLITE");
    }

    /* stream on pipes */
    ret = factory->startPipes();
    if (ret != NO_ERROR) {
        CLOGE("startPipe fail");
        return ret;
    }

    /* start all thread */
    ret = factory->startInitialThreads();
    if (ret != NO_ERROR) {
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
                TIME_LOGGER_UPDATE(m_cameraId, i, 0, CUMULATIVE_CNT, FACTORY_DESTROY_START, 0);
                ret = m_frameFactory[i]->destroy();
                TIME_LOGGER_UPDATE(m_cameraId, i, 0, CUMULATIVE_CNT, FACTORY_DESTROY_END, 0);
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

status_t ExynosCamera::m_setupPipeline(ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;

    if (factory == NULL) {
        CLOGE("Frame factory is NULL!!");
        return BAD_VALUE;
    }

    int pipeId = MAX_PIPE_NUM;
    int nodePipeId = MAX_PIPE_NUM;
    int32_t cameraId = factory->getCameraId();
    int32_t reprocessingBayerMode = m_parameters[cameraId]->getReprocessingBayerMode();
    bool flagFlite3aaM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
    bool flag3aaVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_VRA) == HW_CONNECTION_MODE_M2M);
    bool flagMcscVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
    enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
#endif
    int flipHorizontal = 0;
    enum NODE_TYPE nodeType;

    if (flag3aaVraM2M)
        flagMcscVraM2M = false;

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

    if (flag3aaVraM2M)
        factory->setRequest(PIPE_3AF, true);

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

        if (m_configurations->getMode(CONFIGURATION_GMV_MODE) == true) {
            pipeId = PIPE_GMV;
            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);
        }

        pipeId = PIPE_ISP;
    }

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
        flipHorizontal = m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL);

        nodeType = factory->getNodeType(PIPE_MCSC0);
        factory->setControl(V4L2_CID_HFLIP, 0, pipeId, nodeType);

        nodeType = factory->getNodeType(PIPE_MCSC1);
        factory->setControl(V4L2_CID_HFLIP, 0, pipeId, nodeType);

        nodeType = factory->getNodeType(PIPE_MCSC2);
        factory->setControl(V4L2_CID_HFLIP, 0, pipeId, nodeType);

#ifdef SAMSUNG_SW_VDIS
        if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
            nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
        } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
        if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true && m_flagVideoStreamPriority == true) {
            nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
        } else
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true && m_flagVideoStreamPriority == true) {
            nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
        } else
#endif
        {
            nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
        }

        if (nodePipeId != -1) {
            nodePipeId = nodePipeId + PIPE_MCSC0;
            nodeType = factory->getNodeType(nodePipeId);
            ret = factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);
            if (ret < 0)
                CLOGE("V4L2_CID_HFLIP fail, ret(%d)", ret);

            CLOGD("set FlipHorizontal, flipHorizontal(%d) Recording pipeId(%d)",
                    flipHorizontal, nodePipeId);
        }
    }

    if (flagMcscVraM2M || flag3aaVraM2M) {
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
    if (cameraId == m_cameraId && isDualMode == true) {
        pipeId = PIPE_SYNC;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
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

status_t ExynosCamera::m_updateLatestInfoToShot(struct camera2_shot_ext *shot_ext, frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    bool isReprocessing = false;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return BAD_VALUE;
    }

    switch (frameType) {
    case FRAME_TYPE_VISION:
        isReprocessing = false;
        break;
    case FRAME_TYPE_PREVIEW:
    case FRAME_TYPE_PREVIEW_FRONT:
    case FRAME_TYPE_INTERNAL:
#ifdef USE_DUAL_CAMERA
    case FRAME_TYPE_PREVIEW_SLAVE:
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_INTERNAL_SLAVE:
    case FRAME_TYPE_TRANSITION:
    case FRAME_TYPE_TRANSITION_SLAVE:
#endif
        isReprocessing = false;
#ifdef SAMSUNG_TN_FEATURE
        m_updateTemperatureInfo(shot_ext);
#endif
        m_updateSetfile(shot_ext, isReprocessing);
#ifdef SAMSUNG_RTHDR
        m_updateWdrMode(shot_ext, isReprocessing);
#endif
        break;
    case FRAME_TYPE_REPROCESSING:
#ifdef USE_DUAL_CAMERA
    case FRAME_TYPE_REPROCESSING_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
#endif
        isReprocessing = true;
        m_updateJpegControlInfo(shot_ext);
        m_updateSetfile(shot_ext, isReprocessing);
#ifdef SAMSUNG_RTHDR
        m_updateWdrMode(shot_ext, isReprocessing);
#endif
        break;
    default:
        CLOGE("Invalid frame type(%d)", frameType);
        break;
    }

    /* This section is common function */
    m_updateCropRegion(shot_ext);
    m_updateMasterCam(shot_ext);
    m_updateEdgeNoiseMode(shot_ext, isReprocessing);

    return ret;
}

void ExynosCamera::m_updateCropRegion(struct camera2_shot_ext *shot_ext)
{
    int sensorMaxW = 0, sensorMaxH = 0;
    int cropRegionMinW = 0, cropRegionMinH = 0;
    int maxZoom = 0;
    float activeZoomRatio = 1.0f;
    ExynosRect targetActiveZoomRect = {0, };
#if defined(USE_DUAL_CAMERA)
    int newCropRegion[4] = {0, };
    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    ExynosRect subActiveZoomRect = {0, };
    float zoomRatio = 1.0f;
    float prevZoomRatio = 1.0f;
    int mainSolutionMargin = 0, subSolutionMargin = 0;
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
    int cropMinWidth = 0, cropMinHeight = 0;
    int cropWidth = 0, cropHeight = 0;
    int cropWideWidth = 0, cropWideHeight = 0;
    int cropTeleWidth = 0, cropTeleHeight = 0;
#endif
#endif /* SAMSUNGDUAL_SOLUTION */
#endif /* USE_DUAL_CAMERA */

    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&sensorMaxW, (uint32_t *)&sensorMaxH);

    shot_ext->shot.ctl.scaler.cropRegion[0] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[0], 2);
    shot_ext->shot.ctl.scaler.cropRegion[1] = ALIGN_DOWN(shot_ext->shot.ctl.scaler.cropRegion[1], 2);
    shot_ext->shot.ctl.scaler.cropRegion[2] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[2], 2);
    shot_ext->shot.ctl.scaler.cropRegion[3] = ALIGN_UP(shot_ext->shot.ctl.scaler.cropRegion[3], 2);

    targetActiveZoomRect.x = shot_ext->shot.ctl.scaler.cropRegion[0];
    targetActiveZoomRect.y = shot_ext->shot.ctl.scaler.cropRegion[1];
    targetActiveZoomRect.w = shot_ext->shot.ctl.scaler.cropRegion[2];
    targetActiveZoomRect.h = shot_ext->shot.ctl.scaler.cropRegion[3];

#if defined(USE_DUAL_CAMERA) && defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
    if (isDualMode == true && m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
        m_updateCropRegion_vendor(shot_ext, sensorMaxW, sensorMaxH, targetActiveZoomRect, subActiveZoomRect, subSolutionMargin);
    } else
#endif
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        float orgZoomRatio = 0.0f;
        float hifiVideoZoomRatio = 0.0f;
        int hifiVideoMargin = 0;
        int hifiVideoCropRegion[4] = {0};

        orgZoomRatio = m_configurations->getZoomRatio();
        m_parameters[m_cameraId]->getHiFiVideoZoomRatio(orgZoomRatio, &hifiVideoZoomRatio, &hifiVideoMargin);
        if (orgZoomRatio != hifiVideoZoomRatio) {
            CLOGV("[HIFIVIDEO] zoomRatio (%f -> %f), margin(%d)", orgZoomRatio, hifiVideoZoomRatio, hifiVideoMargin);

            hifiVideoCropRegion[2] = ALIGN_UP((int)ceil((float)sensorMaxW / hifiVideoZoomRatio), 2);
            hifiVideoCropRegion[3] = ALIGN_UP((int)ceil((float)sensorMaxH / hifiVideoZoomRatio), 2);
            hifiVideoCropRegion[0] = ALIGN_DOWN((sensorMaxW - hifiVideoCropRegion[2]) / 2, 2);
            hifiVideoCropRegion[1] = ALIGN_DOWN((sensorMaxH - hifiVideoCropRegion[3]) / 2, 2);

            CLOGV("[HIFIVIDEO] Change CropRegion.(%d, %d, %d, %d -> %d, %d, %d, %d)",
                shot_ext->shot.ctl.scaler.cropRegion[0],
                shot_ext->shot.ctl.scaler.cropRegion[1],
                shot_ext->shot.ctl.scaler.cropRegion[2],
                shot_ext->shot.ctl.scaler.cropRegion[3],
                hifiVideoCropRegion[0],
                hifiVideoCropRegion[1],
                hifiVideoCropRegion[2],
                hifiVideoCropRegion[3]
            );

            targetActiveZoomRect.x = hifiVideoCropRegion[0];
            targetActiveZoomRect.y = hifiVideoCropRegion[1];
            targetActiveZoomRect.w = hifiVideoCropRegion[2];
            targetActiveZoomRect.h = hifiVideoCropRegion[3];
        }

        m_parameters[m_cameraId]->setActiveZoomMargin(hifiVideoMargin);
    } else
#endif
    {
        m_parameters[m_cameraId]->setActiveZoomMargin(0);
    }

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

    if (cropRegionMinW > (int) targetActiveZoomRect.w
        || cropRegionMinH > (int) targetActiveZoomRect.h){
        CLOGE("Invalid Crop Size(%d, %d),Change to cropRegionMin(%d, %d)",
                targetActiveZoomRect.w,
                targetActiveZoomRect.h,
                cropRegionMinW, cropRegionMinH);

        targetActiveZoomRect.w = cropRegionMinW;
        targetActiveZoomRect.h = cropRegionMinH;
    }

    /* 1. Check the validation of the crop size(width x height).
     * The crop size must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) targetActiveZoomRect.w
        || sensorMaxH < (int)targetActiveZoomRect.h) {
        CLOGE("Invalid Crop Size(%d, %d), sensorMax(%d, %d)",
                targetActiveZoomRect.w,
                targetActiveZoomRect.h,
                sensorMaxW, sensorMaxH);
        targetActiveZoomRect.w = sensorMaxW;
        targetActiveZoomRect.h = sensorMaxH;
    }

    /* 2. Check the validation of the crop offset.
     * Offset coordinate + width or height must be smaller than sensor max size.
     */
    if (sensorMaxW < (int) targetActiveZoomRect.x
            + (int) targetActiveZoomRect.w) {
        CLOGE("Invalid Crop Region, offsetX(%d), width(%d) sensorMaxW(%d)",
                targetActiveZoomRect.x,
                targetActiveZoomRect.w,
                sensorMaxW);
        targetActiveZoomRect.x = sensorMaxW - targetActiveZoomRect.w;
    }

    if (sensorMaxH < (int) targetActiveZoomRect.y
            + (int) targetActiveZoomRect.h) {
        CLOGE("Invalid Crop Region, offsetY(%d), height(%d) sensorMaxH(%d)",
                targetActiveZoomRect.y,
                targetActiveZoomRect.h,
                sensorMaxH);
        targetActiveZoomRect.y = sensorMaxH - targetActiveZoomRect.h;
    }

    m_parameters[m_cameraId]->setCropRegion(targetActiveZoomRect.x,
                                            targetActiveZoomRect.y,
                                            targetActiveZoomRect.w,
                                            targetActiveZoomRect.h);

    /* set main active zoom rect */
    m_parameters[m_cameraId]->setActiveZoomRect(targetActiveZoomRect);

    /* set main active zoom ratio */
    activeZoomRatio = (float)(sensorMaxW) / (float)(targetActiveZoomRect.w);
    m_parameters[m_cameraId]->setActiveZoomRatio(activeZoomRatio);

#ifdef USE_DUAL_CAMERA
    if (isDualMode == true) {
        ExynosRect slaveRect;
        m_parameters[m_cameraIds[1]]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&sensorMaxW, (uint32_t *)&sensorMaxH);

        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                if (subActiveZoomRect.x < 0 || subActiveZoomRect.y < 0
                    || subActiveZoomRect.w > sensorMaxW||subActiveZoomRect.h > sensorMaxH) {
                    CLOGE("Invalid sub zoom rect(%d,%d,%d,%d",
                            subActiveZoomRect.x, subActiveZoomRect.y,
                            subActiveZoomRect.w, subActiveZoomRect.h);
                    /* set Default Zoom Rect */
                    m_parameters[m_cameraIds[1]]->setActiveZoomMargin(0);

                    newCropRegion[2] = targetActiveZoomRect.w * DUAL_CAMERA_TELE_RATIO;
                    newCropRegion[3] = targetActiveZoomRect.h * DUAL_CAMERA_TELE_RATIO;
                    newCropRegion[0] = (sensorMaxW - ALIGN_UP(newCropRegion[2], 2)) / 2;
                    newCropRegion[1] = (sensorMaxH - ALIGN_UP(newCropRegion[3], 2)) / 2;
                } else {
                    m_parameters[m_cameraIds[1]]->setActiveZoomMargin(subSolutionMargin);

                    newCropRegion[0] = subActiveZoomRect.x;
                    newCropRegion[1] = subActiveZoomRect.y;
                    newCropRegion[2] = subActiveZoomRect.w;
                    newCropRegion[3] = subActiveZoomRect.h;
                }
            } else {
                m_parameters[m_cameraIds[1]]->setActiveZoomMargin(0);

                newCropRegion[2] = targetActiveZoomRect.w * DUAL_CAMERA_TELE_RATIO;
                newCropRegion[3] = targetActiveZoomRect.h * DUAL_CAMERA_TELE_RATIO;
                newCropRegion[0] = (sensorMaxW - ALIGN_UP(newCropRegion[2], 2)) / 2;
                newCropRegion[1] = (sensorMaxH - ALIGN_UP(newCropRegion[3], 2)) / 2;
            }
#else
            m_parameters[m_cameraIds[1]]->setActiveZoomMargin(0);

            newCropRegion[2] = targetActiveZoomRect.w * DUAL_CAMERA_TELE_RATIO;
            newCropRegion[3] = targetActiveZoomRect.h * DUAL_CAMERA_TELE_RATIO;
            newCropRegion[0] = (sensorMaxW - ALIGN_UP(newCropRegion[2], 2)) / 2;
            newCropRegion[1] = (sensorMaxH - ALIGN_UP(newCropRegion[3], 2)) / 2;

#endif
        } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
            m_parameters[m_cameraIds[1]]->setActiveZoomMargin(0);

            newCropRegion[0] = targetActiveZoomRect.x;
            newCropRegion[1] = targetActiveZoomRect.y;
            newCropRegion[2] = targetActiveZoomRect.w;
            newCropRegion[3] = targetActiveZoomRect.h;
        }

        slaveRect.x = ALIGN_DOWN(newCropRegion[0], 2);
        slaveRect.y = ALIGN_DOWN(newCropRegion[1], 2);
        slaveRect.w = ALIGN_UP(newCropRegion[2], 2);
        slaveRect.h = ALIGN_UP(newCropRegion[3], 2);

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
        // Temp for DualPreview Solution
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true && zoomRatio != 1.0f) {
            m_parameters[m_cameraIds[1]]->getDualSolutionSize(&cropWidth, &cropHeight,
                    &cropWideWidth, &cropWideHeight,
                    &cropTeleWidth, &cropTeleHeight,
                    subSolutionMargin);
            CLOGV("[HIFIVIDEO] sensorMax (%dx%d), crop (%dx%d), cropWide (%dx%d), cropTele (%dx%d), subSolutionMargin %d",
                    sensorMaxW,
                    sensorMaxH,
                    cropWidth,
                    cropHeight,
                    cropWideWidth,
                    cropWideHeight,
                    cropTeleWidth,
                    cropTeleHeight,
                    subSolutionMargin);
            CLOGV("[HIFIVIDEO] Org newCropRegion x %d, y %d, w %d, h %d",
                    newCropRegion[0],
                    newCropRegion[1],
                    newCropRegion[2],
                    newCropRegion[3]);

            if (mainSolutionMargin == DUAL_SOLUTION_MARGIN_VALUE_30 ||
                mainSolutionMargin == DUAL_SOLUTION_MARGIN_VALUE_20) {
                cropMinWidth = cropTeleWidth;
                cropMinHeight = cropTeleHeight;
            } else {
                cropMinWidth = cropWidth;
                cropMinHeight = cropHeight;
            }
            if (newCropRegion[2] < cropMinWidth) {
                slaveRect.w = cropMinWidth;
                slaveRect.x = ALIGN_DOWN((sensorMaxW - cropMinWidth) / 2, 2);
            }
            if (newCropRegion[3] < cropMinHeight) {
                slaveRect.h = cropMinHeight;
                slaveRect.y = ALIGN_DOWN((sensorMaxH - cropMinHeight) / 2, 2);
            }
            CLOGV("[HIFIVIDEO] New slaveRect x %d, y %d, w %d, h %d",
                    slaveRect.x,
                    slaveRect.y,
                    slaveRect.w,
                    slaveRect.h);
        }
#endif

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
            slaveRect.x = ALIGN_DOWN((sensorMaxW - slaveRect.w) / 2, 2);
        }

        if (sensorMaxH < slaveRect.y + slaveRect.h) {
            slaveRect.y = ALIGN_DOWN((sensorMaxH - slaveRect.h) / 2, 2);
        }

        m_parameters[m_cameraIds[1]]->setCropRegion(
                slaveRect.x, slaveRect.y, slaveRect.w, slaveRect.h);

        /* set sub active zoom rect */
        m_parameters[m_cameraIds[1]]->setActiveZoomRect(slaveRect);

        /* set main active zoom ratio */
        activeZoomRatio = (float)(sensorMaxW) / (float)(slaveRect.w);
        m_parameters[m_cameraIds[1]]->setActiveZoomRatio(activeZoomRatio);
    }
#endif
}

void ExynosCamera::m_updateFD(struct camera2_shot_ext *shot_ext, enum facedetect_mode fdMode,
    int dsInputPortId, bool isReprocessing, bool isEarlyFd)
{
    if (fdMode <= FACEDETECT_MODE_OFF
#ifdef USE_ALWAYS_FD_ON
        && m_configurations->getMode(CONFIGURATION_ALWAYS_FD_ON_MODE) == false
#endif
        ) {
        shot_ext->fd_bypass = 1;
        shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
        shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
    } else {
        if (isReprocessing == true) {
            shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
            shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            if (isEarlyFd) {
                // TODO: need to check it.
                shot_ext->fd_bypass = 1;
            } else if (dsInputPortId < MCSC_PORT_MAX) {
                shot_ext->fd_bypass = 1;
            } else {
#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
                shot_ext->fd_bypass = 1;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
#else
                shot_ext->fd_bypass = 0;
                shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) (dsInputPortId % MCSC_PORT_MAX);
#endif
                shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
            }
        } else {
            shot_ext->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
            shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = MCSC_PORT_NONE;
            if (m_configurations->getMode(CONFIGURATION_PIP_MODE) == true && getCameraId() == CAMERA_ID_BACK) {
                shot_ext->fd_bypass = 1;
            } else {
                if (isEarlyFd) {
                    shot_ext->fd_bypass = 0;
                    shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                } else if (dsInputPortId < MCSC_PORT_MAX) {
                    shot_ext->fd_bypass = 0;
                    shot_ext->shot.ctl.stats.faceDetectMode = fdMode;
                    shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) dsInputPortId ;
                } else {
                    shot_ext->fd_bypass = 1;
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

    ret = m_configurations->setModeValue(CONFIGURATION_JPEG_QUALITY, shot_ext->shot.ctl.jpeg.quality);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setJpegQuality. quality %d",
                 shot_ext->shot.ctl.jpeg.quality);
    }

    ret = m_parameters[m_cameraId]->checkThumbnailSize(
            shot_ext->shot.ctl.jpeg.thumbnailSize[0],
            shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
    if (ret != NO_ERROR) {
        CLOGE("Failed to checkThumbnailSize. size %dx%d",
                shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        ret = m_parameters[m_cameraIds[1]]->checkThumbnailSize(
                shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
        if (ret != NO_ERROR) {
            CLOGE("Failed to checkThumbnailSize. size %dx%d",
                    shot_ext->shot.ctl.jpeg.thumbnailSize[0],
                    shot_ext->shot.ctl.jpeg.thumbnailSize[1]);
        }
    }
#endif

    ret = m_configurations->setModeValue(CONFIGURATION_THUMBNAIL_QUALITY, shot_ext->shot.ctl.jpeg.thumbnailQuality);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setThumbnailQuality. quality %d",
                shot_ext->shot.ctl.jpeg.thumbnailQuality);
    }
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
#if 0 /* Use the same frame count in dual camera, lls capture scenario */
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        CLOG_PERFORMANCE(FPS, factory->getCameraId(),
            factory->getFactoryType(), DURATION,
            FRAME_CREATE, 0, request->getKey(), nullptr);

        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }

        /* debug */
        if (request != nullptr)
            newFrame->setRequestKey(request->getKey());

        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    newFrame->setHasRequest(true);

    CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "");

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
#if 0 /* Use the same frame count in dual camera, lls capture scenario */
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        CLOG_PERFORMANCE(FPS, factory->getCameraId(),
            factory->getFactoryType(), DURATION,
            FRAME_CREATE, 0, request->getKey(), nullptr);

        newFrame = factory->createNewFrame(frameCount, useJpegFlag);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }

        /* debug */
        if (request != nullptr)
            newFrame->setRequestKey(request->getKey());

        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    newFrame->setHasRequest(true);

    CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "");

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
    ExynosCameraFrameEntity *entity = NULL;
    uint32_t pipeId = 0;
    int retryCount = 1;
    frame_handle_components_t components;

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

    entity = frame->getFirstEntityNotComplete();
    pipeId = (entity == NULL) ? -1 : entity->getPipeId();

    CLOGI("[F%d T%d P%d] frame frameCount",
            frame->getFrameCount(), frame->getFrameType(), pipeId);

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

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
        if (components.reprocessingFactory == NULL) {
            CLOGE("frame factory is NULL");
            goto FUNC_EXIT;
        }

        while (components.reprocessingFactory->isRunning() == false) {
            CLOGD("[F%d]Wait to start reprocessing stream", frame->getFrameCount());

            if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGD("[F%d]Flush is in progress.", frame->getFrameCount());
                goto FUNC_EXIT;
            }

            usleep(5000);
        }

#ifdef USE_HW_RAW_REVERSE_PROCESSING
        /* To let the 3AA reprocessing pipe twice */
        if (components.parameters->isUseRawReverseReprocessing() == true &&
                frame->getStreamRequested(STREAM_TYPE_RAW) == true) {
            frame->backupRequest();

            if (frame->getRequest(PIPE_3AC_REPROCESSING) == true) {
                bool twiceRun;
                /* Raw Capture will be processed last time */
                twiceRun = frame->reverseExceptForSpecificReq(PIPE_3AC_REPROCESSING, false);
                if (!twiceRun) {
                    /* just only one loop for raw */
                    frame->reverseExceptForSpecificReq(PIPE_3AC_REPROCESSING, true);
                    /* for raw capture setfile index */
                    frame->setSetfile(ISS_SUB_SCENARIO_STILL_CAPTURE_DNG_REPROCESSING);
                }

                CLOGD("[F%d] This frame will be processed %d th time", frame->getFrameCount(), twiceRun ? 2 : 1);
            }
        }
#endif

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW
            && frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            components.reprocessingFactory->pushFrameToPipe(frame, PIPE_SYNC_REPROCESSING);
        } else
#endif
        {
            components.reprocessingFactory->pushFrameToPipe(frame, pipeId);
        }

        /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
        if (components.parameters->isReprocessing() == true)
            components.reprocessingFactory->startThread(pipeId);

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
    if (m_captureQ->getSizeOfProcessQ() > 0 || m_captureProcessList.size() > 0) {
        return true;
    } else {
        return false;
    }
}

status_t ExynosCamera::m_getBayerServiceBuffer(ExynosCameraFrameSP_sptr_t frame,
                                                ExynosCameraBuffer *buffer,
                                                ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    int retryCount = 30;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    ExynosCameraFrameFactory *factory = components.previewFactory;

    int dstPos = factory->getNodeType(PIPE_VC0);
    int pipeId = -1;

    if (components.parameters->getUsePureBayerReprocessing() == true) {
        pipeId = PIPE_3AA_REPROCESSING;
    } else {
        pipeId = PIPE_ISP_REPROCESSING;
    }

    if (frame->getStreamRequested(STREAM_TYPE_RAW)) {
        m_captureZslSelector->setWaitTime(200000000);
        bayerFrame = m_captureZslSelector->selectDynamicFrames(1, retryCount);
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

bool ExynosCamera::m_previewStreamGMVPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_GMV]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
            goto retry_thread_loop;
        } else if (ret != NO_ERROR) {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
            goto retry_thread_loop;
        } else if (newFrame == NULL) {
            CLOGE("Frame is NULL");
            goto retry_thread_loop;
        }
    }

    return m_previewStreamFunc(newFrame, PIPE_GMV);

retry_thread_loop:
    if (m_pipeFrameDoneQ[PIPE_GMV]->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}

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
    if (m_pipeFrameDoneQ[PIPE_HFD]->getSizeOfProcessQ() > 0) {
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
            enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();
            if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                CLOGW("wait timeout");
            }
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
            enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();
            if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                CLOGW("wait timeout");
            }
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
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    int ret;

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SET_BUFFER_THREAD_START, 0);

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == false) {
#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_allocAdaptiveNormalBuffers();
#else
        ret = m_setBuffers();
#endif
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

bool ExynosCamera::m_deinitBufferSupplierThreadFunc(void)
{
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_SUPPLIER_DEINIT_START, 0);

    if (m_bufferSupplier != NULL) {
        delete m_bufferSupplier;
        m_bufferSupplier = NULL;
    }

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_SUPPLIER_DEINIT_END, 0);
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

status_t ExynosCamera::m_pushServiceRequest(camera3_capture_request *request_in,
                                                        ExynosCameraRequestSP_dptr_t req, bool skipRequest)
{
    status_t ret = OK;

    CLOGV("m_pushServiceRequest frameCnt(%d)", request_in->frame_number);

    req = m_requestMgr->registerToServiceList(request_in, skipRequest);
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
    ExynosCameraFrameFactory *factoryList[FRAME_FACTORY_TYPE_MAX] = {NULL, };

    factoryList[FRAME_FACTORY_TYPE_REPROCESSING] = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5)
                factoryList[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
            factory = factoryList[i];
            if (factory == NULL)
                    continue;

            if (factory->isCreated() == false) {
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

#ifdef USE_DUAL_CAMERA
            if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                    && m_dualFrameFactoryStartThread != NULL) {
                /* reprocessing instance must be create after creation of preview instance */
                CLOGI("m_dualFrameFactoryStartThread join E");
                m_dualFrameFactoryStartThread->join();
                CLOGI("m_dualFrameFactoryStartThread join X");
            }
#endif

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

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
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

bool ExynosCamera::m_checkValidBayerBufferSize(struct camera2_stream *stream, ExynosCameraFrameSP_sptr_t frame, bool flagForceRecovery)
{
    status_t ret = NO_ERROR;
    ExynosRect bayerRect;
    ExynosRect reprocessingInputRect;
    struct camera2_node_group nodeGroupInfo;
    frame_handle_components_t components;
    int perframeIndex = -1;

    if (stream == NULL) {
        CLOGE("[F%d]stream is NULL", frame->getFrameCount());
        return false;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return false;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    bool usePureBayerReprocessing = components.parameters->getUsePureBayerReprocessing();

    if (usePureBayerReprocessing == true) {
        CLOGV("[F%d]Skip. Reprocessing mode is PURE bayer", frame->getFrameCount());
        return true;
    } else {
        perframeIndex = PERFRAME_INFO_DIRTY_REPROCESSING_ISP;
    }

    /* Get per-frame size info from stream.
     * Set by driver.
     */
    bayerRect.x = stream->output_crop_region[0];
    bayerRect.y = stream->output_crop_region[1];
    bayerRect.w = stream->output_crop_region[2];
    bayerRect.h = stream->output_crop_region[3];

    /* Get per-frame size info from frame.
     * Set by FrameFactory.
     */
    ret = frame->getNodeGroupInfo(&nodeGroupInfo, perframeIndex);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getNodeGroupInfo. perframeIndex %d ret %d",
                frame->getFrameCount(), perframeIndex, ret);
        return true;
    }

    reprocessingInputRect.x = nodeGroupInfo.leader.input.cropRegion[0];
    reprocessingInputRect.y = nodeGroupInfo.leader.input.cropRegion[1];
    reprocessingInputRect.w = nodeGroupInfo.leader.input.cropRegion[2];
    reprocessingInputRect.h = nodeGroupInfo.leader.input.cropRegion[3];

    /* Compare per-frame size. */
    if (bayerRect.w != reprocessingInputRect.w
        || bayerRect.h != reprocessingInputRect.h) {
        CLOGW("[F%d(%d)]Bayer size mismatch. Bayer %dx%d Control %dx%d, forceRecovery(%d)",
                frame->getFrameCount(),
                stream->fcount,
                bayerRect.w, bayerRect.h,
                reprocessingInputRect.w, reprocessingInputRect.h,
                flagForceRecovery);

        if (flagForceRecovery) {
            /* recovery based on this node's real perframe size */
            nodeGroupInfo.leader.input.cropRegion[0] = bayerRect.x;
            nodeGroupInfo.leader.input.cropRegion[1] = bayerRect.y;
            nodeGroupInfo.leader.input.cropRegion[2] = bayerRect.w;
            nodeGroupInfo.leader.input.cropRegion[3] = bayerRect.h;

            /* also forcely set the input crop */
            for (int i = 0; i < CAPTURE_NODE_MAX; i++) {
                if (nodeGroupInfo.capture[i].vid) {
                    nodeGroupInfo.capture[i].input.cropRegion[0] = 0;
                    nodeGroupInfo.capture[i].input.cropRegion[1] = 0;
                    nodeGroupInfo.capture[i].input.cropRegion[2] = bayerRect.w;
                    nodeGroupInfo.capture[i].input.cropRegion[3] = bayerRect.h;
                }
            }

            ret = frame->storeNodeGroupInfo(&nodeGroupInfo, perframeIndex);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getNodeGroupInfo. perframeIndex %d ret %d",
                        frame->getFrameCount(), perframeIndex, ret);
            }
        } else {
            return false;
        }
    }

    return true;
}

bool ExynosCamera::m_startPictureBufferThreadFunc(void)
{
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    int ret = 0;

    if (m_parameters[m_cameraId]->isReprocessing() == true) {
#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_allocAdaptiveReprocessingBuffers();
#else
        ret = m_setReprocessingBuffer();
#endif
        if (ret < 0) {
            CLOGE("m_setReprocessing Buffer fail");
            m_bufferSupplier->deinit();
            return false;
        }
    }

    return false;
}

status_t ExynosCamera::m_generateInternalFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                                Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame, frame_type_t frameType,
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
        m_frameCountLock.lock();
        frameCount = m_internalFrameCount++;
        m_frameCountLock.unlock();
        hasReques = false;
    }

    CLOGV("(%d)", frameCount);
#if 0 /* Use the same frame count in dual camera, lls capture scenario */
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        CLOG_PERFORMANCE(FPS, factory->getCameraId(),
            factory->getFactoryType(), DURATION,
            FRAME_CREATE, 0, request == nullptr ? 0 : request->getKey(), nullptr);

        newFrame = factory->createNewFrame(frameCount);
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }

        /* debug */
        if (request != nullptr)
            newFrame->setRequestKey(request->getKey());

        listLock->lock();
        list->push_back(newFrame);
        listLock->unlock();
    }

    /* Set frame type into FRAME_TYPE_INTERNAL */
    ret = newFrame->setFrameInfo(m_configurations, factory->getParameters(), frameCount, frameType);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setFrameInfo with INTERNAL. frameCount %d", frameCount);
        return ret;
    }

    newFrame->setHasRequest(hasReques);

    CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "");

    return ret;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::m_generateTransitionFrame(ExynosCameraFrameFactory *factory, List<ExynosCameraFrameSP_sptr_t> *list,
                                                Mutex *listLock, ExynosCameraFrameSP_dptr_t newFrame, frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    newFrame = NULL;
    uint32_t frameCount = m_internalFrameCount;
    bool hasReques = false;

    m_frameCountLock.lock();
    frameCount = m_internalFrameCount++;
    m_frameCountLock.unlock();


    CLOGV("(%d)", frameCount);
#if 0 /* Use the same frame count in dual camera, lls capture scenario */
    ret = m_searchFrameFromList(list, listLock, frameCount, newFrame);
    if (ret < 0) {
        CLOGE("searchFrameFromList fail");
        return INVALID_OPERATION;
    }
#endif

    if (newFrame == NULL) {
        CLOG_PERFORMANCE(FPS, factory->getCameraId(),
            factory->getFactoryType(), DURATION,
            FRAME_CREATE, 0, 0, nullptr);

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
    ret = newFrame->setFrameInfo(m_configurations, factory->getParameters(), frameCount, frameType);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setFrameInfo with INTERNAL. frameCount %d", frameCount);
        return ret;
    }

    newFrame->setHasRequest(hasReques);

    CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "");

    return ret;
}

status_t ExynosCamera::m_checkDualOperationMode(ExynosCameraRequestSP_sprt_t request,
                                                 bool isNeedModeChange, bool isReprocessing, bool flagFinishFactoryStart)
{
    Mutex::Autolock lock(m_dualOperationModeLock);

    status_t ret = NO_ERROR;
    flagFinishFactoryStart &= m_flagFinishDualFrameFactoryStartThread;

    if (request == NULL) {
        CLOGE("request is NULL!!");
        return BAD_VALUE;
    }

    enum DUAL_OPERATION_MODE newOperationMode = DUAL_OPERATION_MODE_NONE;
    enum DUAL_OPERATION_MODE oldOperationMode = m_configurations->getDualOperationMode();
    enum DUAL_OPERATION_MODE newOperationModeReprocessing = DUAL_OPERATION_MODE_NONE;
    enum DUAL_OPERATION_MODE oldOperationModeReprocessing = m_configurations->getDualOperationModeReprocessing();
    float zoomRatio = request->getMetaParameters()->m_zoomRatio; /* zoom ratio by current request */
    int32_t dualOperationModeLockCount = m_configurations->getDualOperationModeLockCount();
#ifdef DEBUG_IQ_CAPTURE_MODE
    int debugCaptureMode = m_configurations->getModeValue(CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE);
#endif
    bool needCheckMasterStandby = false;
    bool needCheckSlaveStandby = false;
    bool isTriggered = false;
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);
    int dispFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT);
    int targetFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT);
#endif
    int dualOperationFallback = -1;
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
    dualOperationFallback = m_configurations->getStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK);
#endif

    if (isReprocessing) {
        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
           /* reprocessingDualMode */
           if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
                if (zoomRatio >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
                    if (dispCamType == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                    } else
#endif
                    {
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_SLAVE;
                    }
                } else {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                }
            } else {
                if (zoomRatio < DUAL_CAPTURE_SYNC_MODE_MIN_ZOOM_RATIO) {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                } else if (zoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_SLAVE;
                } else {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_SYNC;
                }

                m_dualCaptureLockCount = DUAL_CAPTURE_LOCK_COUNT;

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
                int fallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT);

                switch (newOperationModeReprocessing) {
                case DUAL_OPERATION_MODE_SYNC:
                    if (oldOperationMode != DUAL_OPERATION_MODE_SYNC
                        || (m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true)
                        || m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_BURST
                        || m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_AGIF
#ifdef SAMSUNG_TN_FEATURE
                        || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_BURST
                        || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_AGIF
#endif
                        || (fallbackState == true
#ifdef DEBUG_IQ_CAPTURE_MODE
                            && debugCaptureMode == DEBUG_IQ_CAPTURE_MODE_NORMAL
#endif
                    )) {
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                        CLOGD("[Fusion] SYNC, set to MASTER: "
                                "[R%d] oldOperationMode(%d) m_currentMultiCaptureMode(%d) fallbackState(%d)",
                                request->getKey(), oldOperationMode, m_currentMultiCaptureMode, fallbackState);
                    }
                    break;
                case DUAL_OPERATION_MODE_SLAVE:
                    if (oldOperationMode == DUAL_OPERATION_MODE_SYNC
                        && dispCamType != UNI_PLUGIN_CAMERA_TYPE_TELE) {
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                        CLOGD("[Fusion] SLAVE1, set to MASTER: [R%d] oldOperationMode(%d) dispCamType(%d)",
                                request->getKey(), oldOperationMode, dispCamType);
                    } else if (oldOperationMode == DUAL_OPERATION_MODE_MASTER) {
                        CLOGW("Forcely set to MASTER: [R%d] oldOperationMode(%d) dispCamType(%d)",
                                request->getKey(), oldOperationMode, dispCamType);
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                    }
                    break;
                case DUAL_OPERATION_MODE_MASTER:
                    if (oldOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                        CLOGW("Forcely set to SLAVE: [R%d] oldOperationMode(%d) dispCamType(%d)",
                                request->getKey(), oldOperationMode, dispCamType);
                        newOperationModeReprocessing = DUAL_OPERATION_MODE_SLAVE;
                    }
                    break;
                default:
                    break;
                }
#else
                if (newOperationModeReprocessing == DUAL_OPERATION_MODE_SYNC
                     && (m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_BURST
                         || m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_AGIF)) {
                     newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                }
#endif

                if (m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_BURST
                    || m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_AGIF
#ifdef SAMSUNG_TN_FEATURE
                    || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_BURST
                    || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_AGIF
#endif
                    ) {
                    if (m_dualMultiCaptureLockflag == false) {
                        m_dualMultiCaptureLockflag = true;
                    } else {
                        if (oldOperationModeReprocessing != newOperationModeReprocessing) {
                            newOperationModeReprocessing = oldOperationModeReprocessing;
                        }
                    }
                }
            }

            if (dualOperationModeLockCount > 0 &&
                    oldOperationModeReprocessing != DUAL_OPERATION_MODE_NONE &&
                    oldOperationMode != DUAL_OPERATION_MODE_SYNC) {
                /* keep this dualOperationModeReprocessing with current dualOperationMode */
                if (oldOperationMode != newOperationModeReprocessing) {
                    CLOGW("[R%d] lock reprocessing operation mode(%d / %d) for capture, lock(%d)",
                            request->getKey(), newOperationModeReprocessing, oldOperationMode,
                            dualOperationModeLockCount);
                    newOperationModeReprocessing = oldOperationMode;
                }
            }
        } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
#ifdef SAMSUNG_TN_FEATURE
            if (m_configurations->getMode(CONFIGURATION_FACTORY_TEST_MODE) == true) {
                CameraMetadata *meta = request->getServiceMeta();
                int32_t capture_hint = SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_NONE;

                if (meta->exists(SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT)) {
                    camera_metadata_entry_t entry;
                    entry = meta->find(SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT);
                    capture_hint = entry.data.i32[0];
                }

                if (capture_hint == SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_FACTORY_MAIN) {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                } else if (capture_hint == SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_FACTORY_SECONDARY) {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_SLAVE;
                } else {
                    newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
                }
            } else
#endif
            {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
                newOperationModeReprocessing = DUAL_OPERATION_MODE_SYNC;
#else
                newOperationModeReprocessing = DUAL_OPERATION_MODE_MASTER;
#endif
            }
        }

        m_configurations->setDualOperationModeReprocessing(newOperationModeReprocessing);
   } else {
        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
            if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
                if (zoomRatio >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
                    if (dualOperationFallback == 1 && oldOperationMode != DUAL_OPERATION_MODE_SLAVE) {
                        if (m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT)) {
                            newOperationMode = DUAL_OPERATION_MODE_MASTER;
                        } else {
                            newOperationMode = DUAL_OPERATION_MODE_SYNC;
                        }
                    } else
#endif
                    {
                        newOperationMode = DUAL_OPERATION_MODE_SLAVE;
                    }
                } else {
                    newOperationMode = DUAL_OPERATION_MODE_MASTER;
                }
            } else {
                if (zoomRatio < DUAL_PREVIEW_SYNC_MODE_MIN_ZOOM_RATIO) {
                    /* DUAL_OPERATION_MODE_MASTER */
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
                    /* clear dual operation mode fallback flag */
                    if (m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT) == false) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
                    }
#endif
                    newOperationMode = DUAL_OPERATION_MODE_MASTER;
                } else if (zoomRatio > DUAL_PREVIEW_SYNC_MODE_MAX_ZOOM_RATIO) {
                    /* DUAL_OPERATION_MODE_SLAVE */
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
                    if (dualOperationFallback == 1 || dispCamType != UNI_PLUGIN_CAMERA_TYPE_TELE) {
                        newOperationMode = DUAL_OPERATION_MODE_SYNC;
                    } else
#endif
                    {
                        newOperationMode = DUAL_OPERATION_MODE_SLAVE;
                    }
                } else {
                    /* DUAL_OPERATION_MODE_SYNC */
                    newOperationMode = DUAL_OPERATION_MODE_SYNC;
                }
            }

#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
            if (m_configurations->getDynamicMode(DYNAMIC_DUAL_CAMERA_DISABLE) == true) {
                newOperationMode = DUAL_OPERATION_MODE_MASTER;
            }
#endif
        } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
            newOperationMode = DUAL_OPERATION_MODE_SYNC;
        }

        if (m_dualTransitionCount > 0) {
            if (oldOperationMode == DUAL_OPERATION_MODE_SYNC) {
                if (isNeedModeChange == true) {
                    CLOGD("[R%d]Return for dual transition, transitionCount(%d)",
                            request->getKey(), m_dualTransitionCount);
                }

                m_dualTransitionCount--;
                return NO_ERROR;
            }

            /* keep previous sync for at least remained m_dualTransitionCount */
            if (newOperationMode != DUAL_OPERATION_MODE_SYNC)
                /* DUAL_OPERATION_MODE_SYNC */
                newOperationMode = DUAL_OPERATION_MODE_SYNC;
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

            if (isNeedModeChange == true) {
                bool lockStandby = false;
                {
                    /* don't off alive sensor cause of not finished capture */
                    Mutex::Autolock l(m_captureProcessLock);
                    lockStandby = (m_dualCaptureLockCount > 0 || m_captureProcessList.size() > 0);
                }

                if (dualOperationModeLockCount > 0)
                    lockStandby = true;

                CLOGD("Change dual mode old(%d), new(%d), fallback(%d), finishFactoryStart(%d), standby(M:%d, S:%d) count(%d, %d) capturelock(%d) lock(%d)",
                        oldOperationMode, newOperationMode, dualOperationFallback, flagFinishFactoryStart,
                        m_parameters[m_cameraId]->getStandbyState(),
                        m_parameters[m_cameraIds[1]]->getStandbyState(),
                        m_dualTransitionCount,
                        m_dualCaptureLockCount,
                        lockStandby,
                        dualOperationModeLockCount);

                switch (newOperationMode) {
                case DUAL_OPERATION_MODE_MASTER:
                    needCheckMasterStandby = true;
                    break;
                case DUAL_OPERATION_MODE_SLAVE:
                    needCheckSlaveStandby = true;
                    break;
                case DUAL_OPERATION_MODE_SYNC:
                    needCheckMasterStandby = true;
                    needCheckSlaveStandby = true;
                    break;
                default:
                    break;
                }

                m_earlyDualOperationMode = newOperationMode;

                if (newOperationMode == DUAL_OPERATION_MODE_SYNC) {
                    if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false)
                        m_dualTransitionCount = DUAL_TRANSITION_FRAME_COUNT;
                    else
                        m_dualTransitionCount = DUAL_SWITCH_TRANSITION_FRAME_COUNT;
                }

                if ((oldOperationMode != newOperationMode) && (lockStandby == false)) {
                    CLOGD("[R%d] stanby mode trigger start, modeChange(%d)",
                            request->getKey(), isNeedModeChange);
                    ret = m_setStandbyModeTrigger(flagFinishFactoryStart, newOperationMode, oldOperationMode, isNeedModeChange, &isTriggered);
                    if (ret != NO_ERROR) {
                        CLOGE("Set Standby mode(false) fail! ret(%d)", ret);
                    }
                }

                if (oldOperationMode != DUAL_OPERATION_MODE_NONE) {
                    if (flagFinishFactoryStart == false) {
                        CLOGW("[R%d] not finish dualFrameFactoryStartThread forcely set to dualOperationMode(%d -> %d)",
                                request->getKey(), newOperationMode, oldOperationMode);
                        newOperationMode = oldOperationMode;
                        needCheckMasterStandby = false;
                        needCheckSlaveStandby = false;
                    }

                    if (needCheckMasterStandby &&
                            m_parameters[m_cameraId]->getStandbyState() != DUAL_STANDBY_STATE_OFF) {
                        CLOGW("[R%d] not ready of cameraId(%d) state(%d) forcely set to dualOperationMode(%d -> %d)",
                                request->getKey(), m_cameraId, m_parameters[m_cameraId]->getStandbyState(),
                                newOperationMode, oldOperationMode);
                        newOperationMode = oldOperationMode;
                    }

                    if (needCheckSlaveStandby &&
                            m_parameters[m_cameraIds[1]]->getStandbyState() != DUAL_STANDBY_STATE_OFF) {
                        CLOGW("[R%d] not ready of cameraId(%d) state(%d) forcely set to dualOperationMode(%d -> %d)",
                                request->getKey(), m_cameraIds[1], m_parameters[m_cameraIds[1]]->getStandbyState(),
                                newOperationMode, oldOperationMode);
                        newOperationMode = oldOperationMode;
                    }

                    if (lockStandby == true) {
                        switch (newOperationMode) {
                        case DUAL_OPERATION_MODE_MASTER:
                            if (oldOperationMode == DUAL_OPERATION_MODE_SYNC) {
                                CLOGW("[R%d] please keep the previous operation mode(%d / %d) for capture. cnt(%d/%d)",
                                        request->getKey(), newOperationMode, oldOperationMode,
                                        m_dualCaptureLockCount, m_captureProcessList.size());
                                newOperationMode = oldOperationMode;
                            }
                            break;
                        case DUAL_OPERATION_MODE_SLAVE:
                            if (oldOperationMode == DUAL_OPERATION_MODE_SYNC) {
                                CLOGW("[R%d] please keep the previous operation mode(%d / %d) for capture, cnt(%d/%d)",
                                        request->getKey(), newOperationMode, oldOperationMode,
                                        m_dualCaptureLockCount, m_captureProcessList.size());
                                newOperationMode = oldOperationMode;
                            }
                            break;
                        default:
                            break;
                        }
                    }

                    if (dualOperationModeLockCount > 0) {
                        CLOGW("[R%d] lock previous operation mode(%d / %d), lockCnt(%d)",
                                request->getKey(), newOperationMode, oldOperationMode,
                                dualOperationModeLockCount);
                        newOperationMode = oldOperationMode;
                    }
                }

                m_configurations->setDualOperationMode(newOperationMode);

            } else if (newOperationMode == DUAL_OPERATION_MODE_SYNC) {
                m_earlyDualOperationMode = newOperationMode;

                CLOGD("[R%d] stanby mode trigger start, modeChange(%d)",
                        request->getKey(), isNeedModeChange);
                ret = m_setStandbyModeTrigger(flagFinishFactoryStart, newOperationMode, oldOperationMode, isNeedModeChange, &isTriggered);
                if (ret != NO_ERROR) {
                    CLOGE("Set Standby mode(false) fail! ret(%d)", ret);
                } else {
                    if (isTriggered) {
                        /*
                         * This variable will be used in case of situation that
                         * "Early sensorStandby off" is finished too fast
                         * before the request to trigger it is used in m_createPreviewFrameFunc().
                         */
                        m_earlyTriggerRequestKey = request->getKey();
                    }
                }

                /* Clearing essentialRequestList */
                m_latestRequestListLock.lock();
                m_essentialRequestList.clear();
                m_latestRequestListLock.unlock();
            }
        }

        if (isNeedModeChange == true) {
            if (m_dualCaptureLockCount > 0) {
                m_dualCaptureLockCount--;
            }

            m_configurations->decreaseDualOperationModeLockCount();
        }
    }
    return ret;
}

status_t ExynosCamera::m_setStandbyModeTrigger(bool flagFinishFactoryStart,
        enum DUAL_OPERATION_MODE newDualOperationMode,
        enum DUAL_OPERATION_MODE oldDualOperationMode,
        bool isNeedModeChange,
        bool *isTriggered)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    bool masterSensorStandby = false; /* target */
    bool slaveSensorStandby = false;  /* target */
    bool supportMasterSensorStandby;
    bool supportSlaveSensorStandby;
    bool isMasterSensorStandby;
    bool isSlaveSensorStandby;
    dual_standby_state_t masterStandbyState;
    dual_standby_state_t slaveStandbyState;
    dual_standby_trigger_t masterDualStandbyTrigger = DUAL_STANDBY_TRIGGER_NONE;
    dual_standby_trigger_t slaveDualStandbyTrigger = DUAL_STANDBY_TRIGGER_NONE;

    if (isTriggered == NULL) {
        CLOGE("isTriggerd is NULL");
        return INVALID_OPERATION;
    }

    *isTriggered = false;

    if (flagFinishFactoryStart == false) {
        CLOGD("Not yet finishing startFactoryThread(dualOperationMode(%d -> %d))",
                newDualOperationMode, oldDualOperationMode);
        return ret;
    }

    /* master */
    supportMasterSensorStandby = m_parameters[m_cameraId]->isSupportedFunction(SUPPORTED_HW_FUNCTION_SENSOR_STANDBY);
    masterStandbyState = m_parameters[m_cameraId]->getStandbyState();
    isMasterSensorStandby = (masterStandbyState >= DUAL_STANDBY_STATE_OFF) ? false : true;

    /* slave */
    supportSlaveSensorStandby = m_parameters[m_cameraIds[1]]->isSupportedFunction(SUPPORTED_HW_FUNCTION_SENSOR_STANDBY);
    slaveStandbyState = m_parameters[m_cameraIds[1]]->getStandbyState();
    isSlaveSensorStandby = (slaveStandbyState >= DUAL_STANDBY_STATE_OFF) ? false : true;

    switch (oldDualOperationMode) {
    case DUAL_OPERATION_MODE_MASTER:
        switch (newDualOperationMode) {
        case DUAL_OPERATION_MODE_SLAVE:
            masterSensorStandby = true; /* off */
            slaveSensorStandby = false; /* on  */
            break;
        case DUAL_OPERATION_MODE_SYNC:
            masterSensorStandby = false; /* on */
            slaveSensorStandby = false;  /* on */
            break;
        default:
            CLOGE("invalid trasition of dualOperationMode");
            break;
        }
        break;
    case DUAL_OPERATION_MODE_SLAVE:
        switch (newDualOperationMode) {
        case DUAL_OPERATION_MODE_MASTER:
            masterSensorStandby = false; /* on  */
            slaveSensorStandby = true;   /* off */
            break;
        case DUAL_OPERATION_MODE_SYNC:
            masterSensorStandby = false; /* on */
            slaveSensorStandby = false;  /* on */
            break;
        default:
            CLOGE("invalid trasition of dualOperationMode");
            break;
        }
        break;
    case DUAL_OPERATION_MODE_SYNC:
        switch (newDualOperationMode) {
        case DUAL_OPERATION_MODE_MASTER:
            masterSensorStandby = false; /* on  */
            slaveSensorStandby = true;   /* off */
            break;
        case DUAL_OPERATION_MODE_SLAVE:
            masterSensorStandby = true; /* off */
            slaveSensorStandby = false; /* on  */
            break;
        default:
            CLOGE("invalid trasition of dualOperationMode");
            break;
        }
        break;
    default:
        CLOG_ASSERT("wrong dual operationMode(%d)", oldDualOperationMode);
        break;
    }

    /* master */
    if (masterSensorStandby != isMasterSensorStandby) {
        if (!supportMasterSensorStandby) {
            /* poststandby case */
            if (masterSensorStandby == true) {
                m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_ON);
            } else {
                m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_OFF);
            }
        } else {
            /* sensorstandby case */
            masterDualStandbyTrigger = masterSensorStandby ? DUAL_STANDBY_TRIGGER_MASTER_ON : DUAL_STANDBY_TRIGGER_MASTER_OFF;
        }
    }

    /* slave */
    if (slaveSensorStandby != isSlaveSensorStandby) {
        if (!supportSlaveSensorStandby) {
            /* poststandby case */
            if (slaveSensorStandby == true) {
                m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_ON);
            } else {
                m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_OFF);
            }
        } else {
            /* sensorstandby case */
            slaveDualStandbyTrigger = slaveSensorStandby ? DUAL_STANDBY_TRIGGER_SLAVE_ON : DUAL_STANDBY_TRIGGER_SLAVE_OFF;

            /* slave shot sync thread */
            if (slaveDualStandbyTrigger == DUAL_STANDBY_TRIGGER_SLAVE_OFF) {
                if (m_slaveMainThread->isRunning() == false)
                    m_slaveMainThread->run(PRIORITY_URGENT_DISPLAY);
            }
        }

    }

    CLOGD("dualOperationMode(%d -> %d) M[%d](%d, %d, %d) S[%d](%d, %d, %d)",
            oldDualOperationMode, newDualOperationMode,
            masterDualStandbyTrigger,
            masterSensorStandby, isMasterSensorStandby, supportMasterSensorStandby,
            slaveDualStandbyTrigger,
            slaveSensorStandby, isSlaveSensorStandby, supportSlaveSensorStandby);

    /*
     * ordering
     * Master standby off -> Slave standby off -> Slave standby on -> Master standby on
     */
    if (slaveDualStandbyTrigger == DUAL_STANDBY_TRIGGER_SLAVE_OFF) {
        m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_OFF_READY);
        m_dualStandbyTriggerQ->pushProcessQ(&slaveDualStandbyTrigger);
        *isTriggered = true;
    }

    if (masterDualStandbyTrigger == DUAL_STANDBY_TRIGGER_MASTER_OFF) {
        m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_OFF_READY);
        m_dualStandbyTriggerQ->pushProcessQ(&masterDualStandbyTrigger);
        *isTriggered = true;
    }

    if (isNeedModeChange && masterDualStandbyTrigger == DUAL_STANDBY_TRIGGER_MASTER_ON) {
        m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_ON_READY);
        m_dualStandbyTriggerQ->pushProcessQ(&masterDualStandbyTrigger);
        *isTriggered = true;
    }

    if (isNeedModeChange && slaveDualStandbyTrigger == DUAL_STANDBY_TRIGGER_SLAVE_ON) {
        m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_ON_READY);
        m_dualStandbyTriggerQ->pushProcessQ(&slaveDualStandbyTrigger);
        *isTriggered = true;
    }

    if ((masterDualStandbyTrigger != DUAL_STANDBY_TRIGGER_NONE ||
            slaveDualStandbyTrigger != DUAL_STANDBY_TRIGGER_NONE)
        || (m_dualStandbyTriggerQ->getSizeOfProcessQ() > 0 &&
            m_dualStandbyThread->isRunning() == false)) {
        m_dualStandbyThread->run(PRIORITY_URGENT_DISPLAY);
    }

    return ret;
}

bool ExynosCamera::m_dualStandbyThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    bool standby;
    bool isNeedPrepare;
    dual_standby_trigger_t dualStandbyTrigger;
    ExynosCameraFrameFactory *factory;
    enum DUAL_OPERATION_MODE dualOperationMode = DUAL_OPERATION_MODE_NONE;
    frame_type_t prepareFrameType;

    ret = m_dualStandbyTriggerQ->waitAndPopProcessQ(&dualStandbyTrigger);
    if (ret == TIMED_OUT) {
        CLOGW("Time-out to wait m_dualStandbyTriggerQ");
        goto THREAD_EXIT;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ for m_dualStandbyTriggerQ");
        goto THREAD_EXIT;
    }

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGE("state is not RUN!!(%d)", m_getState());
        goto THREAD_EXIT;
    }

    /* default stream-off */
    standby = true;
    isNeedPrepare = false;

    CLOGI("dualStandbyTriggerQ(%d) prepare(%d) trigger(%d)",
            m_dualStandbyTriggerQ->getSizeOfProcessQ(), m_prepareFrameCount, dualStandbyTrigger);

    switch (dualStandbyTrigger) {
    case DUAL_STANDBY_TRIGGER_MASTER_OFF:
        isNeedPrepare = true;
        standby = false;
        dualOperationMode = DUAL_OPERATION_MODE_MASTER;
        prepareFrameType = FRAME_TYPE_TRANSITION;
        /* fall through */
    case DUAL_STANDBY_TRIGGER_MASTER_ON:
        factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        break;
    case DUAL_STANDBY_TRIGGER_SLAVE_OFF:
        isNeedPrepare = true;
        standby = false;
        dualOperationMode = DUAL_OPERATION_MODE_SLAVE;
        prepareFrameType = FRAME_TYPE_TRANSITION_SLAVE;
        /* fall through */
    case DUAL_STANDBY_TRIGGER_SLAVE_ON:
        factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        break;
    default:
        CLOG_ASSERT("invalid trigger(%d)", dualStandbyTrigger);
        break;
    }

    if (standby == false) {
        uint32_t pipeId = PIPE_3AA;

        /* wait for real finish */
        enum SENSOR_STANDBY_STATE state = factory->getSensorStandbyState(pipeId);
        int retryCount = 50; /* maximum 100ms(1frame) * 5 */
        while (state != SENSOR_STANDBY_ON && retryCount > 0) {
            usleep(10000); /* 10ms */
            retryCount--;
            CLOGW("waiting for previous standby off(%d) retry(%d)!!", state, retryCount);
            state = factory->getSensorStandbyState(pipeId);
        }
    }

    if (isNeedPrepare == true) {
        for (int i = 0; i < m_prepareFrameCount; i++) {
            ret = m_createInternalFrameFunc(NULL, false, REQ_SYNC_NONE, prepareFrameType);
            if (ret != NO_ERROR) {
                CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                        i, m_prepareFrameCount);
            }
        }
    }

    ret = factory->sensorStandby(standby);
    if (ret != NO_ERROR) {
        CLOGE("sensorStandby(%s) fail! ret(%d)", (standby?"On":"Off"), ret);
    } else {
        switch (dualStandbyTrigger) {
        case DUAL_STANDBY_TRIGGER_MASTER_OFF:
            m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_OFF);
            break;
        case DUAL_STANDBY_TRIGGER_MASTER_ON:
            m_parameters[m_cameraId]->setStandbyState(DUAL_STANDBY_STATE_ON);
            break;
        case DUAL_STANDBY_TRIGGER_SLAVE_OFF:
            m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_OFF);
            break;
        case DUAL_STANDBY_TRIGGER_SLAVE_ON:
            m_parameters[m_cameraIds[1]]->setStandbyState(DUAL_STANDBY_STATE_ON);
            break;
        default:
            break;
        }
        CLOGI("dualStandbyTriggerQ(%d) prepare(%d) trigger(%d) standby(M:%d, S:%d)",
                m_dualStandbyTriggerQ->getSizeOfProcessQ(), m_prepareFrameCount, dualStandbyTrigger,
                m_parameters[m_cameraId]->getStandbyState(),
                m_parameters[m_cameraIds[1]]->getStandbyState());
    }

THREAD_EXIT:
    if (m_dualStandbyTriggerQ->getSizeOfProcessQ() > 0) {
        CLOGI("run again. dualStandbyTriggerQ size %d", m_dualStandbyTriggerQ->getSizeOfProcessQ());
        return true;
    } else {
        CLOGI("Complete to dualStandbyTriggerQ");
        return false;
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
    uint32_t checkStreamCount = 1;
    frame_type_t frameType = FRAME_TYPE_PREVIEW;
    frame_handle_components_t components;
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

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
        checkStreamCount = 2;
    }
#endif

    for (uint32_t i = 1; i <= checkStreamCount; i++) {
        switch (i) {
#ifdef USE_DUAL_CAMERA
        case 2:
            frameType = FRAME_TYPE_PREVIEW_SLAVE;
            break;
#endif
        case 1:
        default:
#ifdef USE_DUAL_CAMERA
            if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                && m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                frameType = FRAME_TYPE_PREVIEW_SLAVE;
            } else
#endif
            {
                frameType = FRAME_TYPE_PREVIEW;
            }
            break;
        }

        getFrameHandleComponents(frameType, &components);

        factory = components.previewFactory;
        if (factory == NULL) {
            CLOGE("previewFrameFactory is NULL. Skip monitoring.");
            return false;
        }

        if (factory->isCreated() == false) {
            CLOGE("previewFrameFactory is not Created. Skip monitoring.");
            return false;
        }

        bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
        bool flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);

        /* 2. Select Error check pipe ID (of last output node) */
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
        if (factory->checkPipeThreadRunning(pipeIdLogSync)
            && !(getCameraId() == CAMERA_ID_FRONT && m_configurations->getMode(CONFIGURATION_PIP_MODE))) {
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

            if (m_configurations->getSamsungCamera() == true) {
                ret = factory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("PIPE_3AA V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", ret);
                }
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
        CLOGV("Thread IntervalTime [%ju]", *timeInterval);
    }

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

    if (m_configurations->getSamsungCamera() == true) {
        ret = factory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
        if (ret < 0) {
            CLOGE("PIPE_3AA V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", ret);
        }
    }

    return false;
}

#ifdef BUFFER_DUMP
bool ExynosCamera::m_dumpThreadFunc(void)
{
    char filePath[70];
    int imagePlaneCount = -1;
    status_t ret = NO_ERROR;
    buffer_dump_info_t bufDumpInfo;

    ret = m_dumpBufferQ->waitAndPopProcessQ(&bufDumpInfo);
    if (ret == TIMED_OUT) {
        CLOGW("Time-out to wait dumpBufferQ");
        goto THREAD_EXIT;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ for dumpBufferQ");
        goto THREAD_EXIT;
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/%s_CAM%d_F%d_%c%c%c%c_%dx%d.img",
            bufDumpInfo.name,
            m_cameraId,
            bufDumpInfo.frameCount,
            v4l2Format2Char(bufDumpInfo.format, 0),
            v4l2Format2Char(bufDumpInfo.format, 1),
            v4l2Format2Char(bufDumpInfo.format, 2),
            v4l2Format2Char(bufDumpInfo.format, 3),
            bufDumpInfo.width, bufDumpInfo.height);

    imagePlaneCount = bufDumpInfo.buffer.planeCount - 1;

    switch (imagePlaneCount) {
    case 2:
        if (bufDumpInfo.buffer.addr[1] == NULL) {
            android_printAssert(NULL, LOG_TAG, "[CAM_ID(%d)][%s]-ASSERT(%s[%d]):[F%d B%d P1]%s_Buffer do NOT have virtual address",
                    m_cameraId, m_name, __FUNCTION__, __LINE__,
                    bufDumpInfo.frameCount, bufDumpInfo.buffer.index, bufDumpInfo.name);
            goto THREAD_EXIT;
        }
    case 1:
        if (bufDumpInfo.buffer.addr[0] == NULL) {
            android_printAssert(NULL, LOG_TAG, "[CAM_ID(%d)][%s]-ASSERT(%s[%d]):[F%d B%d P0]%s_Buffer do NOT have virtual address",
                    m_cameraId, m_name, __FUNCTION__, __LINE__,
                    bufDumpInfo.frameCount, bufDumpInfo.buffer.index, bufDumpInfo.name);
            goto THREAD_EXIT;
        }
    default:
        break;
    }

    if (imagePlaneCount > 1) {
        ret = dumpToFile2plane((char *)filePath,
                               bufDumpInfo.buffer.addr[0], bufDumpInfo.buffer.addr[1],
                               bufDumpInfo.buffer.size[0], bufDumpInfo.buffer.size[1]);
    } else {
        ret = dumpToFile((char *)filePath, bufDumpInfo.buffer.addr[0], bufDumpInfo.buffer.size[0]);
    }
    if (ret == false) {
        CLOGE("[F%d B%d]Failed to dumpToFile. FilePath %s size %d/%d",
                bufDumpInfo.frameCount, bufDumpInfo.buffer.index, filePath,
                bufDumpInfo.buffer.size[0], bufDumpInfo.buffer.size[1]);
        goto THREAD_EXIT;
    }

THREAD_EXIT:
    if (bufDumpInfo.buffer.index > -1) {
        m_bufferSupplier->putBuffer(bufDumpInfo.buffer);
    }

    if (m_dumpBufferQ->getSizeOfProcessQ() > 0) {
        CLOGI("run again. dumpBufferQ size %d",
                 m_dumpBufferQ->getSizeOfProcessQ());
        return true;
    } else {
        CLOGI("Complete to dumpFrame.");
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
        /* GED Camera appliction calls flush() directly after calling initialize(). */
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

            /* HACK
            * The original retryCount is 10 (1s), : int retryCount = 10;
            * Due to the sensor entry time issues, it has increased to 30 (3s).
            */
            int retryCount = 30;

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
    bool restart = m_configurations->getRestartStream();

    if (restart) {
        CLOGD("Internal restart stream request[R%d]",
             request->getKey());
        ret = m_restartStreamInternal();
        if (ret != NO_ERROR) {
            CLOGE("m_restartStreamInternal failed [%d]", ret);
            return ret;
        }

        m_requestMgr->registerToServiceList(request);
        m_configurations->setRestartStream(false);

        if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
            m_monitorThread->requestExitAndWait();
            ret = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->destroy();
            ret = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->destroy();
            m_configurations->setMode(CONFIGURATION_OBTE_STREAM_CHANGING, false);
            ret = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->create();
            ret = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->create();
        }
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

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
bool ExynosCamera::m_reverseProcessingBayerThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    bool loop = false;
    int dstPos = -1;
    int leaderPipeId = PIPE_3AA_REPROCESSING;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;

    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraBuffer serviceBuffer;
    frame_handle_components_t components;

    CLOGV("Wait reverseProcessingBayerQ");
    ret = m_reverseProcessingBayerQ->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT) {
        CLOGV("Wait timeout to reverseProcessingBayerQ");
        loop = true;
        goto p_err;
    } else if (ret != NO_ERROR) {
        CLOGE("Failed to waitAndPopProcessQ to reverseProcessingBayerQ");
        goto p_err;
    } else if (frame == NULL) {
        CLOGE("Frame is NULL");
        goto p_err;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("[F%d]Failed to get request.", frame->getFrameCount());
        return INVALID_OPERATION;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    dstPos = components.reprocessingFactory->getNodeType(PIPE_3AC_REPROCESSING);

    ret = frame->getDstBuffer(leaderPipeId, &dstBuffer, dstPos);
    if (ret != NO_ERROR || dstBuffer.index < 0) {
        CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                frame->getFrameCount(), dstBuffer.index, dstPos, ret);
        goto p_err;
    }

    ret = frame->getDstBufferState(leaderPipeId, &bufferState, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                frame->getFrameCount(), dstBuffer.index, dstPos, ret);
        goto p_err;
    }

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("[R%d F%d B%d]Invalid RAW buffer state. bufferState %d",
                request->getKey(), frame->getFrameCount(), dstBuffer.index, bufferState);
        request->setStreamBufferStatus(HAL_STREAM_ID_RAW, CAMERA3_BUFFER_STATUS_ERROR);
        goto p_err;
    }

    /* HACK: get service buffer backuped by output node index */
    ret = frame->getDstBuffer(leaderPipeId, &serviceBuffer, OUTPUT_NODE_1);
    if (ret != NO_ERROR || serviceBuffer.index < 0) {
        CLOGE("[F%d B%d]Failed to getDstBuffer for serviceBuffer. pos %d. ret %d",
                frame->getFrameCount(), serviceBuffer.index, OUTPUT_NODE_1, ret);
        return ret;
    }

    /* reverse the raw buffer */
    ret = m_reverseProcessingBayer(frame, &dstBuffer, &serviceBuffer);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed m_reverseProcessingBayer %d",
                frame->getFrameCount(), dstBuffer.index, ret);
    }

    ret = m_bufferSupplier->putBuffer(dstBuffer);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                frame->getFrameCount(), dstBuffer.index, ret);
    }

    ret = m_sendRawStreamResult(request, &serviceBuffer);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to sendRawStreamResult. ret %d",
                frame->getFrameCount(), dstBuffer.index, ret);
    }

p_err:

    return loop;
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
    frame_handle_components_t components;
    struct camera2_stream *meta = NULL;
    camera2_node_group bcropNodeGroupInfo;
    int perframeInfoIndex = -1;

    outputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
    if (outputPortId < 0) {
        CLOGE("There is NO VIDEO stream");
        goto exit_thread_loop;
    }

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
    } else {
        getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

        ExynosCameraFrameFactory *factory = components.previewFactory;

        bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
        bool flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);

        if (flagIspMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
            srcPipeId = PIPE_MCSC;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
            srcPipeId = PIPE_ISP;
        } else {
            srcPipeId = PIPE_3AA;
        }

        srcNodeIndex = factory->getNodeType(PIPE_MCSC0 + outputPortId);
        dstPipeId = PIPE_GDC;
        srcFmt = m_configurations->getYuvFormat(outputPortId);
        dstFmt = m_configurations->getYuvFormat(outputPortId);

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
        perframeInfoIndex = (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M) ? PERFRAME_INFO_3AA : PERFRAME_INFO_FLITE;
        frame->getNodeGroupInfo(&bcropNodeGroupInfo, perframeInfoIndex);
        components.parameters->getMaxSensorSize(&gdcSrcRect[1].fullW, &gdcSrcRect[1].fullH);

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
    }

    return true;

retry_thread_loop:
    if (m_gdcQ->getSizeOfProcessQ() > 0) {
        return true;
    }

exit_thread_loop:
    return false;
}
#endif

int ExynosCamera::m_getMcscLeaderPipeId(ExynosCameraFrameFactory *factory)
{
    uint32_t pipeId = -1;

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return -1;
    }

    uint32_t cameraId = (uint32_t)factory->getCameraId();

    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
        pipeId = PIPE_MCSC_REPROCESSING;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
        pipeId = PIPE_ISP_REPROCESSING;
    } else {
        pipeId = PIPE_3AA_REPROCESSING;
    }

    return pipeId;
}

void ExynosCamera::getFrameHandleComponents(frame_type_t frameType, frame_handle_components_t *components)
{
    switch (frameType) {
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_PREVIEW_SLAVE:
        case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
        case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
        case FRAME_TYPE_INTERNAL_SLAVE:
        case FRAME_TYPE_REPROCESSING_SLAVE:
        case FRAME_TYPE_TRANSITION_SLAVE:
            components->previewFactory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            components->reprocessingFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
            components->parameters = m_parameters[m_cameraIds[1]];
            components->activityControl = m_activityControl[m_cameraIds[1]];
            components->captureSelector = m_captureSelector[m_cameraIds[1]];
            components->currentCameraId = m_cameraIds[1];
            break;
            /* TODO: MUST consider JPEG_REPROCESSING in case of DUAL scenario */
#endif
        case FRAME_TYPE_JPEG_REPROCESSING:
            components->previewFactory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            components->reprocessingFactory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
            components->parameters = m_parameters[m_cameraId];
            components->activityControl = m_activityControl[m_cameraId];
            components->captureSelector = m_captureSelector[m_cameraId];
            components->currentCameraId = m_cameraId;
            break;
        default:
            components->previewFactory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            components->reprocessingFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
            components->parameters = m_parameters[m_cameraId];
            components->activityControl = m_activityControl[m_cameraId];
            components->captureSelector = m_captureSelector[m_cameraId];
            components->currentCameraId = m_cameraId;
            break;
    }
}

status_t ExynosCamera::m_updateFDAEMetadata(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;
    camera2_shot_ext frameShotExt;
    fdae_info_t fdaeInfo;
    struct v4l2_ext_control extCtrl;
    struct v4l2_ext_controls extCtrls;

    ret = frame->getMetaData(&frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getMetaData. ret %d",
                frame->getFrameCount(), ret);
        return ret;
    }

    memset(&fdaeInfo, 0x00, sizeof(fdaeInfo));
    memset(&extCtrl, 0x00, sizeof(extCtrl));
    memset(&extCtrls, 0x00, sizeof(extCtrls));

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    fdaeInfo.frameCount = frameShotExt.shot.dm.request.frameCount;

    for (int index = 0; index < NUM_OF_DETECTED_FACES; index++) {
        if (frameShotExt.hfd.faceIds[index] > 0) {
            fdaeInfo.id[index]          = frameShotExt.hfd.faceIds[index];
            fdaeInfo.score[index]       = frameShotExt.hfd.score[index];
            fdaeInfo.rect[index][X1]    = frameShotExt.hfd.faceRectangles[index][X1];
            fdaeInfo.rect[index][Y1]    = frameShotExt.hfd.faceRectangles[index][Y1];
            fdaeInfo.rect[index][X2]    = frameShotExt.hfd.faceRectangles[index][X2];
            fdaeInfo.rect[index][Y2]    = frameShotExt.hfd.faceRectangles[index][Y2];
            fdaeInfo.isRot[index]       = frameShotExt.hfd.is_rot[index];
            fdaeInfo.rot[index]         = frameShotExt.hfd.rot[index];
            fdaeInfo.faceNum           += 1;

            CLOGV("[F%d(%d)]id %d score %d rect %d,%d %dx%d isRot %d rot %d",
                    frame->getFrameCount(),
                    fdaeInfo.frameCount,
                    fdaeInfo.id[index],
                    fdaeInfo.score[index],
                    fdaeInfo.rect[index][X1],
                    fdaeInfo.rect[index][Y1],
                    fdaeInfo.rect[index][X2],
                    fdaeInfo.rect[index][Y2],
                    fdaeInfo.isRot[index],
                    fdaeInfo.rot[index]);
        }
    }

    CLOGV("[F%d(%d)]setExtControl for FDAE. FaceNum %d",
            frame->getFrameCount(), fdaeInfo.frameCount, fdaeInfo.faceNum);

    extCtrl.id = V4L2_CID_IS_FDAE;
    extCtrl.ptr = &fdaeInfo;

    extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    extCtrls.count = 1;
    extCtrls.controls = &extCtrl;

    ret = factory->setExtControl(&extCtrls, PIPE_VRA);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to setExtControl(V4L2_CID_IS_FDAE). ret %d",
                frame->getFrameCount(), ret);
    }

    return ret;
}

void ExynosCamera::m_adjustSlave3AARegion(frame_handle_components_t *components, ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    camera2_shot_ext frameShotExt;
    ExynosRect masterHwSensorSize;
    ExynosRect masterHwBcropSize;
    ExynosRect slaveHwSensorSize;
    ExynosRect slaveHwBcropSize;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;
    uint32_t *region = NULL;

    ret = frame->getMetaData(&frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getMetaData. ret %d",
                frame->getFrameCount(), ret);
        return;
    }

    m_parameters[m_cameraId]->getPreviewBayerCropSize(&masterHwSensorSize, &masterHwBcropSize);
    components->parameters->getPreviewBayerCropSize(&slaveHwSensorSize, &slaveHwBcropSize);

    CLOGV("[F%d T%d]MasterBcrop %d,%d %dx%d(%d) SlaveBcrop %d,%d %dx%d(%d)",
            frame->getFrameCount(), frame->getFrameType(),
            masterHwBcropSize.x, masterHwBcropSize.y, masterHwBcropSize.w, masterHwBcropSize.h,
            SIZE_RATIO(masterHwBcropSize.w, masterHwBcropSize.h),
            slaveHwBcropSize.x, slaveHwBcropSize.y, slaveHwBcropSize.w, slaveHwBcropSize.h,
            SIZE_RATIO(slaveHwBcropSize.w, slaveHwBcropSize.h));

    /* Scale up the 3AA region
       to adjust into the 3AA input size of slave sensor stream
     */
    scaleRatioW = (float) slaveHwBcropSize.w / (float) masterHwBcropSize.w;
    scaleRatioH = (float) slaveHwBcropSize.h / (float) masterHwBcropSize.h;

    for (int i = 0; i < 3; i++)  {
        switch (i) {
        case 0:
            region = frameShotExt.shot.ctl.aa.afRegions;
            break;
        case 1:
            region = frameShotExt.shot.ctl.aa.aeRegions;
            break;
        case 2:
            region = frameShotExt.shot.ctl.aa.awbRegions;
            break;
        default:
            continue;
        }

        region[0] = (uint32_t) (((float) region[0]) * scaleRatioW);
        region[1] = (uint32_t) (((float) region[1]) * scaleRatioH);
        region[2] = (uint32_t) (((float) region[2]) * scaleRatioW);
        region[3] = (uint32_t) (((float) region[3]) * scaleRatioH);

        /* Adjust the boundary of HW bcrop region */
        /* Bottom-right */
        if (region[0] > (uint32_t)slaveHwBcropSize.w) region[0] = (uint32_t)slaveHwBcropSize.w;
        if (region[1] > (uint32_t)slaveHwBcropSize.h) region[1] = (uint32_t)slaveHwBcropSize.h;
        if (region[2] > (uint32_t)slaveHwBcropSize.w) region[2] = (uint32_t)slaveHwBcropSize.w;
        if (region[3] > (uint32_t)slaveHwBcropSize.h) region[3] = (uint32_t)slaveHwBcropSize.h;
    }

    ret = frame->setMetaData(&frameShotExt);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to setMetaData. ret %d", frame->getFrameCount(), ret);
        return;
    }
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::m_updateSizeBeforeDualFusion(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;

    frame_handle_components_t components;
    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    int pipeId_next = PIPE_FUSION;
    int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
    int previewFormat = m_configurations->getYuvFormat(previewOutputPortId);

    ExynosCameraBuffer buffer;

    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        ///////////////////////
        // set Src(WIDE) Rect
        /*
        * use pipeId_next, not pipeId.
        * because, pipeId is changed in somewhere.
        */
        buffer.index = -2;
        ret = frame->getSrcBuffer(pipeId_next, &buffer);
        if(buffer.index < 0) {
            CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                buffer.index, frame->getFrameCount(), pipeId_next);
        } else {
            ExynosRect rect = convertingBufferDst2Rect(&buffer, previewFormat);

            ret = frame->setSrcRect(pipeId_next, rect, OUTPUT_NODE_1);
            if(ret != NO_ERROR) {
                CLOGE("setSrcRect fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            }
        }

        ///////////////////////
        // set Src(TELE) Rect
        CLOGV("frame->getFrameType() : %d", frame->getFrameType());

        switch (frame->getFrameType()) {
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
        case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
        case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
        case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
            buffer.index = -2;
            ret = frame->getDstBuffer(pipeId, &buffer);
            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), pipeId);
            } else {
                ExynosRect rect = convertingBufferDst2Rect(&buffer, previewFormat);

                ret = frame->setSrcRect(pipeId_next, rect, OUTPUT_NODE_2);
                if(ret != NO_ERROR) {
                    CLOGE("setSrcRect fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }
            }
            break;
        default:
            break;
        }
    }

    ///////////////////////
    // set Dst Rect
    ExynosRect dstRect;
    int width, height;

    components.parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, previewOutputPortId);

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = width;
    dstRect.h = height;
    dstRect.fullW = width;
    dstRect.fullH = height;
    dstRect.colorFormat = previewFormat;

    ret = frame->setDstRect(pipeId_next, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("setDstRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                pipeId_next, frame->getFrameCount(), ret);
    }

    return ret;
}
#endif

#ifdef SUPPORT_ME
status_t ExynosCamera::m_handleMeBuffer(ExynosCameraFrameSP_sptr_t frame, int mePos, const int meLeaderPipeId)
{
    int leaderPipeId = -1;
    ExynosCameraFrameEntity* entity = NULL;
    bool bError = false;
    ExynosCameraBuffer buffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
    int dstPos = mePos;
    status_t ret = NO_ERROR;

    if (frame->getRequest(PIPE_ME) == false) {
        frame->setRequest(PIPE_GMV, false);
        return NO_ERROR;
    }

    /* 1. verify pipeId including ME */
    if (meLeaderPipeId < 0) {
        entity = frame->getFrameDoneFirstEntity();
        if (entity == NULL) {
            CLOGE("current entity is NULL, HAL-frameCount(%d)",
                    frame->getFrameCount());
            /* TODO: doing exception handling */
            return INVALID_OPERATION;
        }
        leaderPipeId = entity->getPipeId();
    } else {
        leaderPipeId = meLeaderPipeId;
    }

    if (m_parameters[m_cameraId]->getLeaderPipeOfMe() != leaderPipeId) {
        return NO_ERROR;
    }

    /* 2. handling ME buffer*/
    ret = frame->getDstBuffer(leaderPipeId, &buffer, dstPos);
    if (ret != NO_ERROR || buffer.index < 0) {
        CLOGE("[F%d B%d]Failed to getDstBuffer for PIPE_ME. ret %d",
                frame->getFrameCount(), buffer.index, ret);
        bError = true;
    }

    ret = frame->getDstBufferState(leaderPipeId, &bufferState, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[F%d P%d]Failed to getDstBufferState for PIPE_ME",
                frame->getFrameCount(), leaderPipeId);
        bError = true;
    }

    if (buffer.index >= 0 && bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
        /* In this case only, the me tag can be appendedd to the frame */
        if (frame->getRawStateInSelector() == FRAME_STATE_IN_SELECTOR_PUSHED) {
            ret = frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), leaderPipeId, dstPos, false);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to addSelectorTag. pipeId %d, dstPos %d, ret %d",
                        frame->getFrameCount(), buffer.index, leaderPipeId, dstPos, ret);
                bError = true;
            }
        } else {
            /* Don't release me buffer in order to support  use case for ME */
            //bError = true;
        }

        if (frame->getRequest(PIPE_GMV) == true) {
            ret = m_setupEntity(PIPE_GMV, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to setSrcBuffer to PIPE_GMV. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                bError = true;
            }
        }
    } else {
        CLOGE("[F%d B%d]Invalid buffer. pipeId %d",
                frame->getFrameCount(), buffer.index, leaderPipeId);
        bError = true;
    }

    /* 3. error case */
    if (bError == true) {
        ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_ERROR, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("[F%d P%d]Failed to setDstBufferState with ERROR for PIPE_ME",
                    frame->getFrameCount(), leaderPipeId);
        }

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for ME. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
    }

    return ret;
}
#endif

bool ExynosCamera::previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame,int pipeId)
{
    return m_previewStreamFunc(newFrame, pipeId);
}

ExynosCameraStreamThread::ExynosCameraStreamThread(ExynosCamera* cameraObj, const char* name, int pipeId)
    : ExynosCameraThread<ExynosCameraStreamThread>(this, &ExynosCameraStreamThread::m_streamThreadFunc, name)
{
    m_mainCamera = cameraObj;
    m_frameDoneQ = NULL;
    m_pipeId = pipeId;
}

ExynosCameraStreamThread::~ExynosCameraStreamThread()
{
}

frame_queue_t* ExynosCameraStreamThread::getFrameDoneQ()
{
    return m_frameDoneQ;
}

void ExynosCameraStreamThread::setFrameDoneQ(frame_queue_t* frameDoneQ)
{
    m_frameDoneQ = frameDoneQ;
}

bool ExynosCameraStreamThread::m_streamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    if(m_frameDoneQ == NULL) {
        ALOGE("frameDoneQ is NULL");
        return false;
    }

    ret = m_frameDoneQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /* CLOGW("wait timeout"); */
        } else {
            ALOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_mainCamera->previewStreamFunc(newFrame, m_pipeId);
}

bool ExynosCamera::m_bayerStreamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory;
    ExynosCameraRequestSP_sprt_t request = NULL;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
    int pipeId = -1;

    ret = m_bayerStreamQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("Failed to wait&pop m_bayerStreamQ, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto FUNC_EXIT;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto FUNC_EXIT;
    } else if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
        CLOGD("[F%d]Flush is in progress.", frame->getFrameCount());
        goto FUNC_EXIT;
    }

    if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
        if (frame->getFrameType() == FRAME_TYPE_JPEG_REPROCESSING)
            factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
        else
            factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    }

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("Current entity is NULL");
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    pipeId = entity->getPipeId();
    CLOG_PERFRAME(PATH, m_cameraId, m_name, frame.get(), nullptr, frame->getRequestKey(), "pipeId(%d)", pipeId);

    CLOGD("[F%d T%d]Bayer Processing done. entityID %d",
            frame->getFrameCount(), frame->getFrameType(), pipeId);

    switch (pipeId) {
        case PIPE_LLS_REPROCESSING:
            /* get source buffer */
            ret = frame->getSrcBuffer(entity->getPipeId(), &srcBuffer);
            if (ret != NO_ERROR || srcBuffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                        frame->getFrameCount(), srcBuffer.index, entity->getPipeId(), ret);
                ret = INVALID_OPERATION;
                goto FUNC_EXIT;
            }

            ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        srcBuffer.index, frame->getFrameCount(), entity->getPipeId());
                ret = INVALID_OPERATION;
                goto FUNC_EXIT;
            }

            /*
             * if the frame is not internal frame, move the dst buffer
             * to source position for reprocessing
             */
            if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
                /* get dst buffer */
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d, ret %d",
                            frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
                    ret = INVALID_OPERATION;
                    goto FUNC_EXIT;
                }

                ret = frame->getDstBufferState(entity->getPipeId(), &bufferState);
                if (ret < 0) {
                    CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                    goto FUNC_EXIT;
                }

                if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                    CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());
                    ret = INVALID_OPERATION;
                    goto FUNC_EXIT;
                }

                /* memcpy meta data to dst buf from src buf */
                memcpy((struct camera2_shot_ext *)srcBuffer.addr[srcBuffer.getMetaPlaneIndex()],
                        (struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()],
                        sizeof(struct camera2_shot_ext));

                /* cache clean */
                if (m_ionClient >= 0) {
                    for (int i = 0; i < buffer.getMetaPlaneIndex(); i++)
                        exynos_ion_sync_fd(m_ionClient, buffer.fd[i]);
                }

                m_setSrcBuffer(PIPE_ISP_REPROCESSING, frame, &buffer);
                if (ret < 0) {
                    CLOGE("[F%d B%d]Failed to setSrcBuffer. pipeId %d ret %d",
                            frame->getFrameCount(), buffer.index,
                            PIPE_ISP_REPROCESSING, ret);
                    goto FUNC_EXIT;
                }
            }

            /* release source buffer */
            if (srcBuffer.index >= 0) {
                ret = m_releaseSelectorTagBuffers(frame);
                if (ret < 0) {
                    CLOGE("Failed to releaseSelectorTagBuffers. frameCount %d ret %d",
                            frame->getFrameCount(), ret);
                    goto FUNC_EXIT;
                }
            }

            /* frame complete for internal frame */
            if (frame->getFrameType() == FRAME_TYPE_INTERNAL) {
                frame->setFrameState(FRAME_STATE_COMPLETE);
                goto FUNC_EXIT;
            }
            break;
        default:
            CLOGE("Invalid pipeId %d", entity->getPipeId());
            break;
    }

    ret = frame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    m_captureQ->pushProcessQ(&frame);

FUNC_EXIT:
    if (frame != NULL && frame->isComplete() == true && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        /* error case */
        if (frame->getFrameType() != FRAME_TYPE_INTERNAL && ret < 0) {
            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
        if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == pipeId
                && (frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == true)) {
            struct camera2_shot_ext resultShot;
            ExynosCameraRequestSP_sprt_t request = NULL;

            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
            }

            request = m_requestMgr->getRunningRequest(frame->getFrameCount());
            if (request == NULL) {
                CLOGE("getRequest failed ");
                return INVALID_OPERATION;
            }

            ret = m_updateResultShot(request, &resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d)]Failed to m_updateResultShot. ret %d",
                        frame->getFrameCount(), resultShot.shot.dm.request.frameCount, ret);
            }
            ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to sendMeta. ret %d", frame->getFrameCount(), ret);
            }
        }

        List<ExynosCameraFrameSP_sptr_t> *list = NULL;
        Mutex *listLock = NULL;
        list = &m_captureProcessList;
        listLock = &m_captureProcessLock;
        // TODO:decide proper position
        CLOGD("frame complete. framecount %d", frame->getFrameCount());
        ret = m_removeFrameFromList(list, listLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        frame = NULL;
    }
    {
        Mutex::Autolock l(m_captureProcessLock);
        if (m_captureProcessList.size() > 0)
            return true;
        else
            return false;
    }
}

status_t ExynosCamera::m_selectBayerHandler(uint32_t pipeID, ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *bayerBuffer,__unused ExynosCameraFrameSP_sptr_t bayerFrame, ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    buffer_manager_tag_t bufTag;
    ExynosCameraBuffer dstBuffer;
    bool needBayerProcessingThread = false;
    camera2_stream *shot_stream = NULL;
    ExynosRect cropRect;

    /* dstbuffer setting */
    switch (pipeID) {
    case PIPE_LLS_REPROCESSING:
#ifdef SUPPORT_ME
        {
            /* ME buffer setting */
            int meSrcPipeId = m_parameters[m_cameraId]->getLeaderPipeOfMe();
            int dstPos = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->getNodeType(PIPE_ME);
            ExynosCameraBuffer meBuffer;

            bayerFrame->getDstBuffer(meSrcPipeId, &meBuffer, dstPos);
            if (meBuffer.index < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), dstPos(%d) ret(%d)",
                        meSrcPipeId, dstPos, ret);
            } else {
                /* move to buffer position and set */
                ret = frame->setSrcBuffer(pipeID, meBuffer, OUTPUT_NODE_2);
                if (ret < 0) {
                    CLOGE("[F%d B%d]Failed to setSrcBuffer. pipeId %d ret %d",
                            frame->getFrameCount(), meBuffer.index, pipeID, ret);
                }

                frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(),
                        pipeID, OUTPUT_NODE_2, true /* isSrc */);
            }
        }
#endif

        /* bayer buffer setting */
        shot_stream = (camera2_stream *)bayerBuffer->addr[bayerBuffer->getMetaPlaneIndex()];
        cropRect.x = shot_stream->output_crop_region[0];
        cropRect.y = shot_stream->output_crop_region[1];
        cropRect.w = shot_stream->output_crop_region[2];
        cropRect.h = shot_stream->output_crop_region[3];
        cropRect.fullW = shot_stream->output_crop_region[2];
        cropRect.fullH = shot_stream->output_crop_region[3];
        cropRect.colorFormat = m_parameters[m_cameraId]->getBayerFormat(PIPE_ISP_REPROCESSING);

        ret = frame->setSrcRect(pipeID, cropRect);
        if (ret < 0)
            CLOGE("[F%d]Failed to setSrcRect. ret %d", frame->getFrameCount(), ret);

        ret = frame->setDstRect(pipeID, cropRect);
        if (ret < 0)
            CLOGE("[F%d]Failed to setDstRect. ret %d", frame->getFrameCount(), ret);

        /* getting dst buffer in last frame */
        if (frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_LLS_REPROCESSING
                && frame->getFrameIndex() == (m_parameters[m_cameraId]->getLLSCaptureCount())
#endif
           ) {
            bufTag.pipeId[0] = pipeID;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
            ret = m_bufferSupplier->getBuffer(bufTag, &dstBuffer);
            if (ret < 0 || dstBuffer.index < 0) {
                CLOGE("[F%d B%d]Failed to get dstBuffer. ret %d",
                        frame->getFrameCount(), dstBuffer.index, ret);
            }
        } else {
            frame->setDstBufferState(pipeID, ENTITY_BUFFER_STATE_NOREQ);
            if (ret < 0)
                CLOGE("[F%d]Failed to set dstBuffer state. ret %d", frame->getFrameCount(), ret);
        }
        break;
    default:
        break;
    }

    ret = m_setupEntity(pipeID, frame, bayerBuffer, dstBuffer.index < 0 ? NULL : &dstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, bayerPipeId(%d), ret(%d)",
                pipeID, ret);
        goto CLEAN;
    }

    /* add selectorTag to release all buffers after done of these buffers */
    frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(),
            pipeID, OUTPUT_NODE_1, true /* isSrc */);

    /* queing for different path */
    switch (pipeID) {
        case PIPE_LLS_REPROCESSING:
            factory->setOutputFrameQToPipe(m_bayerStreamQ, pipeID);
            factory->pushFrameToPipe(frame, pipeID);
            needBayerProcessingThread = true;
            break;
        default:
            m_captureQ->pushProcessQ(&frame);
            break;
    }

    if (needBayerProcessingThread == true &&
            m_bayerStreamThread != NULL && m_bayerStreamThread->isRunning() == false) {
        m_bayerStreamThread->run();
        CLOGI("Initiate bayerStreamThread (%d)", m_bayerStreamThread->getTid());
    } else {
        CLOGI("bayerStreamThread (%d, %d, run:%d)",
                needBayerProcessingThread, m_bayerStreamThread != NULL,
                m_bayerStreamThread->isRunning());
    }

CLEAN:
    return ret;
}


}; /* namespace android */
