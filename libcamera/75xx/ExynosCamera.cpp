/*
**
** Copyright 2014, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCamera"
#include <cutils/log.h>

#include "ExynosCamera.h"

#ifdef BURST_CAPTURE
#include <sys/resource.h>
#include <private/android_filesystem_config.h>

#include <ctype.h>
#include <dirent.h>
#endif

#ifdef TOUCH_AE
#define AE_RESULT 0xF351
#endif

namespace android {

/* vision */
#ifdef VISION_DUMP
int dumpIndex = 0;
#endif

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif

ExynosCamera::ExynosCamera(int cameraId, camera_device_t *dev)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;

    BUILD_DATE();

    checkAndroidVersion();

    m_cameraId = cameraId;
    memset(m_name, 0x00, sizeof(m_name));

    m_exynosCameraParameters = new ExynosCameraParameters(m_cameraId);
    CLOGD("DEBUG(%s):Parameters(Id=%d) created", __FUNCTION__, m_cameraId);

    m_exynosCameraParameters->setHalVersion(IS_HAL_VER_1_0);

    m_exynosCameraActivityControl = m_exynosCameraParameters->getActivityControl();

    m_previewFrameFactory      = NULL;
    m_reprocessingFrameFactory = NULL;
    /* vision */
    m_visionFrameFactory= NULL;

    m_ionAllocator = NULL;
    m_grAllocator  = NULL;
    m_mhbAllocator = NULL;

    m_frameMgr = NULL;

    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
    m_createInternalBufferManager(&m_3aaBufferMgr, "3A1_BUF");
    m_createInternalBufferManager(&m_ispBufferMgr, "ISP_BUF");
    m_createInternalBufferManager(&m_hwDisBufferMgr, "HW_DIS_BUF");

    /* reprocessing Buffer */
    m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "SCC_RE_BUF");

    m_createInternalBufferManager(&m_sccBufferMgr, "SCC_BUF");
    m_createInternalBufferManager(&m_gscBufferMgr, "GSC_BUF");
    m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF");

    /* preview Buffer */
    m_scpBufferMgr = NULL;
    m_createInternalBufferManager(&m_previewCallbackBufferMgr, "PREVIEW_CB_BUF");
    m_createInternalBufferManager(&m_highResolutionCallbackBufferMgr, "HIGH_RESOLUTION_CB_BUF");

    /* recording Buffer */
    m_recordingCallbackHeap = NULL;
    m_createInternalBufferManager(&m_recordingBufferMgr, "REC_BUF");

    m_createThreads();

    m_pipeFrameDoneQ     = new frame_queue_t;
    dstIspReprocessingQ  = new frame_queue_t;
    dstSccReprocessingQ  = new frame_queue_t;
    dstGscReprocessingQ  = new frame_queue_t;
    m_zoomPreviwWithCscQ = new frame_queue_t;

    dstJpegReprocessingQ = new frame_queue_t;
    /* vision */
    m_pipeFrameVisionDoneQ     = new frame_queue_t;

    m_frameFactoryQ = new framefactory_queue_t;
    m_facedetectQ = new frame_queue_t;
    m_facedetectQ->setWaitTime(500000000);

    m_previewQ     = new frame_queue_t;
    m_previewQ->setWaitTime(500000000);
    m_previewCallbackGscFrameDoneQ = new frame_queue_t;
    m_recordingQ   = new frame_queue_t;
    m_recordingQ->setWaitTime(500000000);
    m_postPictureQ = new frame_queue_t(m_postPictureThread);
    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        m_mainSetupQ[i] = new frame_queue_t;
        m_mainSetupQ[i]->setWaitTime(500000000);
    }
    m_jpegCallbackQ = new jpeg_callback_queue_t;

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum] = new jpeg_callback_queue_t;
        m_jpegSaveQ[threadNum]->setWaitTime(2000000000);
        m_burst[threadNum] = false;
        m_running[threadNum] = false;
    }

    dstIspReprocessingQ->setWaitTime(20000000);
    dstSccReprocessingQ->setWaitTime(50000000);
    dstGscReprocessingQ->setWaitTime(500000000);
    dstJpegReprocessingQ->setWaitTime(500000000);
    /* vision */
    m_pipeFrameVisionDoneQ->setWaitTime(2000000000);

    m_jpegCallbackQ->setWaitTime(1000000000);

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));
    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

    m_exitAutoFocusThread = false;
    m_autoFocusRunning    = false;
    m_previewEnabled   = false;
    m_pictureEnabled   = false;
    m_recordingEnabled = false;
    m_zslPictureEnabled   = false;
    m_flagStartFaceDetection = false;
    m_flagLLSStart = false;
    m_flagLightCondition = false;
    m_fdThreshold = 0;
    m_captureSelector = NULL;
    m_sccCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_doCscRecording = true;
    m_recordingBufferCount = NUM_RECORDING_BUFFERS;
    m_frameSkipCount = 0;
#ifdef BURST_CAPTURE
    m_burstCaptureCallbackCount = 0;
#endif
    m_skipCount = 0;

    m_lensmoveCount = 0;
    m_AFstatus = ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_NONE;
    m_currentSetStart = false;
    m_flagMetaDataSet = false;

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    m_stopBurstShot = false;
    m_disablePreviewCB = false;

    m_callbackState = 0;
    m_callbackStateOld = 0;
    m_callbackMonitorCount = 0;

    m_highResolutionCallbackRunning = false;
    m_highResolutionCallbackQ = new frame_queue_t(m_highResolutionCallbackThread);
    m_highResolutionCallbackQ->setWaitTime(500000000);
    m_skipReprocessing = false;
    m_isFirstStart = true;
    m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);

    m_exynosconfig = NULL;
    m_setConfigInform();

    m_setFrameManager();


    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_dynamicSccCount = 0;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

#ifdef FIRST_PREVIEW_TIME_CHECK
    m_flagFirstPreviewTimerOn = false;
#endif

    /* init infomation of fd orientation*/
    m_exynosCameraParameters->setDeviceOrientation(0);
    uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
    if (uctlMgr != NULL)
        uctlMgr->setDeviceRotation(m_exynosCameraParameters->getFdOrientation());
#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif

    m_initFrameFactory();

}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? NUM_BAYER_BUFFERS : FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? NUM_PICTURE_BUFFERS : FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_PREVIEW_BUFFERS_MARGIN;

    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_FLITE_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = (getCameraId() == CAMERA_ID_BACK) ? PIPE_SCP_PREPARE_COUNT : PIPE_SCP_FRONT_PREPARE_COUNT;

    /* reprocessing */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
    /* Config HIGH_SPEED 60 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS60_NUM_NUM_BAYER_BUFFERS : FPS60_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS60_NUM_PICTURE_BUFFERS : FPS60_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_REPROCESSING_PREPARE_COUNT;

    /* Config HIGH_SPEED 120 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS120_NUM_NUM_BAYER_BUFFERS : FPS120_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_3aa_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hwdis_buffers = FPS120_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = FPS120_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = (getCameraId() == CAMERA_ID_BACK) ? FPS120_NUM_PICTURE_BUFFERS : FPS120_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = FPS120_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = FPS120_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = FPS120_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = FPS120_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.preview_buffer_margin = FPS120_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = FPS120_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AC] = FPS120_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA_ISP] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP] = FPS120_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS120_PIPE_SCP_REPROCESSING_PREPARE_COUNT;
#endif

    m_exynosCameraParameters->setConfig(&exynosConfig);

    m_exynosconfig = m_exynosCameraParameters->getConfig();

    return NO_ERROR;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainThread = new mainCameraThread(this, &ExynosCamera::m_mainThreadFunc, "ExynosCameraThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("DEBUG(%s):mainThread created", __FUNCTION__);

    m_previewThread = new mainCameraThread(this, &ExynosCamera::m_previewThreadFunc, "previewThread", PRIORITY_DISPLAY);
    CLOGD("DEBUG(%s):previewThread created", __FUNCTION__);

    /*
     * In here, we cannot know single, dual scenario.
     * So, make all threads.
     */
    /* if (m_exynosCameraParameters->isFlite3aaOtf() == true) { */
    if (1) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupFLITE, "mainThreadQSetupFLITE", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupFLITEThread created", __FUNCTION__);

/* Change 3AA_ISP, 3AC, SCP to ISP */
/*
        m_mainSetupQThread[INDEX(PIPE_3AC)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AC, "mainThreadQSetup3AC", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3ACThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_3AA_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA_ISP, "mainThreadQSetup3AA_ISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3AA_ISPThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupISP, "mainThreadQSetupISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupISPThread created", __FUNCTION__);

        m_mainSetupQThread[INDEX(PIPE_SCP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupSCP, "mainThreadQSetupSCP", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetupSCPThread created", __FUNCTION__);
*/

        m_mainSetupQThread[INDEX(PIPE_3AA)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA, "mainThreadQSetup3AA", PRIORITY_URGENT_DISPLAY);
        CLOGD("DEBUG(%s):mainThreadQSetup3AAThread created", __FUNCTION__);

    }
    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGD("DEBUG(%s):setBuffersThread created", __FUNCTION__);

    m_startPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_startPictureInternalThreadFunc, "startPictureInternalThread");
    CLOGD("DEBUG(%s):startPictureInternalThread created", __FUNCTION__);

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGD("DEBUG(%s):startPictureBufferThread created", __FUNCTION__);

    m_prePictureThread = new mainCameraThread(this, &ExynosCamera::m_prePictureThreadFunc, "prePictureThread");
    CLOGD("DEBUG(%s):prePictureThread created", __FUNCTION__);

    m_pictureThread = new mainCameraThread(this, &ExynosCamera::m_pictureThreadFunc, "PictureThread");
    CLOGD("DEBUG(%s):pictureThread created", __FUNCTION__);

    m_postPictureThread = new mainCameraThread(this, &ExynosCamera::m_postPictureThreadFunc, "postPictureThread");
    CLOGD("DEBUG(%s):postPictureThread created", __FUNCTION__);

    m_recordingThread = new mainCameraThread(this, &ExynosCamera::m_recordingThreadFunc, "recordingThread");
    CLOGD("DEBUG(%s):recordingThread created", __FUNCTION__);

    m_autoFocusThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusThreadFunc, "AutoFocusThread");
    CLOGD("DEBUG(%s):autoFocusThread created", __FUNCTION__);

    m_autoFocusContinousThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusContinousThreadFunc, "AutoFocusContinousThread");
    CLOGD("DEBUG(%s):autoFocusContinousThread created", __FUNCTION__);

    m_facedetectThread = new mainCameraThread(this, &ExynosCamera::m_facedetectThreadFunc, "FaceDetectThread");
    CLOGD("DEBUG(%s):FaceDetectThread created", __FUNCTION__);

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "monitorThread");
    CLOGD("DEBUG(%s):monitorThread created", __FUNCTION__);

    m_framefactoryThread = new mainCameraThread(this, &ExynosCamera::m_frameFactoryInitThreadFunc, "FrameFactoryInitThread");
    CLOGD("DEBUG(%s):FrameFactoryInitThread created", __FUNCTION__);

    m_jpegCallbackThread = new mainCameraThread(this, &ExynosCamera::m_jpegCallbackThreadFunc, "jpegCallbackThread");
    CLOGD("DEBUG(%s):jpegCallbackThread created", __FUNCTION__);

    /* saveThread */
    char threadName[20];
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        snprintf(threadName, sizeof(threadName), "jpegSaveThread%d", threadNum);
        m_jpegSaveThread[threadNum] = new mainCameraThread(this, &ExynosCamera::m_jpegSaveThreadFunc, threadName);
        CLOGD("DEBUG(%s):%s created", __FUNCTION__, threadName);
    }

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    CLOGD("DEBUG(%s):highResolutionCallbackThread created", __FUNCTION__);

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("DEBUG(%s):visionThread created", __FUNCTION__);

    /* Shutter callback */
    m_shutterCallbackThread = new mainCameraThread(this, &ExynosCamera::m_shutterCallbackThreadFunc, "shutterCallbackThread");
    CLOGD("DEBUG(%s):shutterCallbackThread created", __FUNCTION__);

}

status_t ExynosCamera::m_setupFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_exynosCameraParameters->getVisionMode() == true) {
        /* about vision */
        if( m_frameFactory[FRAME_FACTORY_TYPE_VISION] == NULL) {
            m_frameFactory[FRAME_FACTORY_TYPE_VISION] = new ExynosCameraFrameFactoryVision(m_cameraId, m_exynosCameraParameters);
            m_frameFactory[FRAME_FACTORY_TYPE_VISION]->setFrameManager(m_frameMgr);
        }
        m_visionFrameFactory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];

        if (m_frameFactory[FRAME_FACTORY_TYPE_VISION] != NULL && m_frameFactory[FRAME_FACTORY_TYPE_VISION]->isCreated() == false) {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, FRAME_FACTORY_TYPE_VISION);
            m_frameFactoryQ->pushProcessQ(&m_frameFactory[FRAME_FACTORY_TYPE_VISION]);
        }
    } else {
        /* about preview */
        if (m_exynosCameraParameters->getDualMode() == true) {
            m_previewFrameFactory      = m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW];
        } else if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            if (m_exynosCameraParameters->is3aaIspOtf() == true)
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF_TPU];
            else
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M_TPU];
        } else {
            if (m_exynosCameraParameters->is3aaIspOtf() == true)
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF];
            else
                m_previewFrameFactory  = m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M];
        }

        /* find previewFrameFactory and push */
        for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
            if (m_previewFrameFactory == m_frameFactory[i]) {
                if (m_frameFactory[i] != NULL && m_frameFactory[i]->isCreated() == false) {
                    CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, i);
                    m_frameFactoryQ->pushProcessQ(&m_frameFactory[i]);
                }
                break;
            }
        }

        /* about reprocessing */
        if (m_exynosCameraParameters->isReprocessing() == true) {
            m_reprocessingFrameFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->isCreated() == false) {
                CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, FRAME_FACTORY_TYPE_REPROCESSING);
                m_frameFactoryQ->pushProcessQ(&m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]);
            }
        }
    }

    /*
     * disable until multi-instace is possible.
     */
    /*
    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL && m_frameFactory[i]->isCreated() == false) {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory pushProcessQ(%d)", __FUNCTION__, __LINE__, i);
            m_frameFactoryQ->pushProcessQ(&m_frameFactory[i]);
        } else {
            CLOGD("DEBUG(%s[%d]):setupFrameFactory no Push(%d)", __FUNCTION__, __LINE__, i);
        }
    }
    */

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_initFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;

    m_previewFrameFactory = NULL;
    m_pictureFrameFactory = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory = NULL;

    for(int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++)
        m_frameFactory[i] = NULL;

    /*
     * new all FrameFactories.
     * because this called on open(). so we don't know current scenario
     */

    factory = new ExynosCameraFrameFactory3aaIspM2M(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M] = factory;
    /* hack : for dual */
    if (getCameraId() == CAMERA_ID_FRONT) {
        factory = new ExynosCameraFrameFactoryFront(m_cameraId, m_exynosCameraParameters);
        m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;
    } else {
        factory = new ExynosCameraFrameFactory3aaIspM2M(m_cameraId, m_exynosCameraParameters);
        m_frameFactory[FRAME_FACTORY_TYPE_DUAL_PREVIEW] = factory;
    }

    factory = new ExynosCameraFrameFactory3aaIspM2MTpu(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_M2M_TPU] = factory;

    factory = new ExynosCameraFrameFactory3aaIspOtf(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF] = factory;

    factory = new ExynosCameraFrameFactory3aaIspOtfTpu(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_3AA_ISP_OTF_TPU] = factory;

    factory = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_exynosCameraParameters);
    m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] = factory;

    for (int i = 0 ; i < FRAME_FACTORY_TYPE_MAX ; i++) {
        factory = m_frameFactory[i];
        if( factory != NULL )
            factory->setFrameManager(m_frameMgr);
    }

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_deinitFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;

    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("DEBUG(%s[%d]): m_frameFactory index(%d) and index(%d) are same instance, set index(%d) = NULL",
                       __FUNCTION__, __LINE__, i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            if (m_frameFactory[i]->isCreated() == true) {
                ret = m_frameFactory[i]->destroy();
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):m_frameFactory[%d] destroy fail", __FUNCTION__, __LINE__, i);
            }

            SAFE_DELETE(m_frameFactory[i]);

            CLOGD("DEBUG(%s[%d]):m_frameFactory[%d] destroyed", __FUNCTION__, __LINE__, i);
        }
    }

    m_previewFrameFactory = NULL;
    m_pictureFrameFactory = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory = NULL;

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return ret;
}

status_t ExynosCamera::m_setFrameManager()
{
    sp<FrameWorker> worker;
    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT, 50, 100);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 40);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new DeleteWorker("DELETE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::DELETE, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}


ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_frameMgr != NULL) {
        m_frameMgr->stop();
    }

    /* release all framefactory */
    m_deinitFrameFactory();

    if (m_exynosCameraParameters != NULL) {
        delete m_exynosCameraParameters;
        m_exynosCameraParameters = NULL;
        CLOGD("DEBUG(%s):Parameters(Id=%d) destroyed", __FUNCTION__, m_cameraId);
    }

    /* free all buffers */
    m_releaseBuffers();

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_grAllocator != NULL) {
        delete m_grAllocator;
        m_grAllocator = NULL;
    }

    if (m_mhbAllocator != NULL) {
        delete m_mhbAllocator;
        m_mhbAllocator = NULL;
    }

    if (m_pipeFrameDoneQ != NULL) {
        delete m_pipeFrameDoneQ;
        m_pipeFrameDoneQ = NULL;
    }

    if (m_zoomPreviwWithCscQ != NULL) {
        delete m_zoomPreviwWithCscQ;
        m_zoomPreviwWithCscQ = NULL;
    }

    /* vision */
    if (m_pipeFrameVisionDoneQ != NULL) {
        delete m_pipeFrameVisionDoneQ;
        m_pipeFrameVisionDoneQ = NULL;
    }

    if (dstIspReprocessingQ != NULL) {
        delete dstIspReprocessingQ;
        dstIspReprocessingQ = NULL;
    }

    if (dstSccReprocessingQ != NULL) {
        delete dstSccReprocessingQ;
        dstSccReprocessingQ = NULL;
    }

    if (dstGscReprocessingQ != NULL) {
        delete dstGscReprocessingQ;
        dstGscReprocessingQ = NULL;
    }

    if (dstJpegReprocessingQ != NULL) {
        delete dstJpegReprocessingQ;
        dstJpegReprocessingQ = NULL;
    }

    if (m_postPictureQ != NULL) {
        delete m_postPictureQ;
        m_postPictureQ = NULL;
    }

    if (m_jpegCallbackQ != NULL) {
        delete m_jpegCallbackQ;
        m_jpegCallbackQ = NULL;
    }

    if (m_facedetectQ != NULL) {
        delete m_facedetectQ;
        m_facedetectQ = NULL;
    }

    if (m_previewQ != NULL) {
        delete m_previewQ;
        m_previewQ = NULL;
    }

    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        if (m_mainSetupQ[i] != NULL) {
            delete m_mainSetupQ[i];
            m_mainSetupQ[i] = NULL;
        }
    }

    if (m_previewCallbackGscFrameDoneQ != NULL) {
        delete m_previewCallbackGscFrameDoneQ;
        m_previewCallbackGscFrameDoneQ = NULL;
    }

    if (m_recordingQ != NULL) {
        delete m_recordingQ;
        m_recordingQ = NULL;
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_jpegSaveQ[threadNum] != NULL) {
            delete m_jpegSaveQ[threadNum];
            m_jpegSaveQ[threadNum] = NULL;
        }
    }

    if (m_highResolutionCallbackQ != NULL) {
        delete m_highResolutionCallbackQ;
        m_highResolutionCallbackQ = NULL;
    }

    if (m_frameFactoryQ != NULL) {
        delete m_frameFactoryQ;
        m_frameFactoryQ = NULL;
        CLOGD("DEBUG(%s):FrameFactoryQ destroyed", __FUNCTION__);
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(bayerBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(3aaBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispBufferMgr != NULL) {
        delete m_ispBufferMgr;
        m_ispBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(ispBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_hwDisBufferMgr != NULL) {
        delete m_hwDisBufferMgr;
        m_hwDisBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_hwDisBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_scpBufferMgr != NULL) {
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(scpBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(ispReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccReprocessingBufferMgr != NULL) {
        delete m_sccReprocessingBufferMgr;
        m_sccReprocessingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(sccReprocessingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_sccBufferMgr != NULL) {
        delete m_sccBufferMgr;
        m_sccBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(sccBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_gscBufferMgr != NULL) {
        delete m_gscBufferMgr;
        m_gscBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(gscBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_jpegBufferMgr != NULL) {
        delete m_jpegBufferMgr;
        m_jpegBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(jpegBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_previewCallbackBufferMgr != NULL) {
        delete m_previewCallbackBufferMgr;
        m_previewCallbackBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(previewCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        delete m_highResolutionCallbackBufferMgr;
        m_highResolutionCallbackBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_highResolutionCallbackBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_recordingBufferMgr != NULL) {
        delete m_recordingBufferMgr;
        m_recordingBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(recordingBufferMgr) destroyed", __FUNCTION__);
    }

    if (m_captureSelector != NULL) {
        delete m_captureSelector;
        m_captureSelector = NULL;
    }

    if (m_sccCaptureSelector != NULL) {
        delete m_sccCaptureSelector;
        m_sccCaptureSelector = NULL;
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        delete m_recordingCallbackHeap;
        m_recordingCallbackHeap = NULL;
        CLOGD("DEBUG(%s):BufferManager(recordingCallbackHeap) destroyed", __FUNCTION__);
    }

    m_isFirstStart = true;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
}

int ExynosCamera::getCameraId() const
{
    return m_cameraId;
}

int ExynosCamera::getShotBufferIdex() const
{
    return NUM_PLANES(V4L2_PIX_2_HAL_PIXEL_FORMAT(SCC_OUTPUT_COLOR_FMT));
}

status_t ExynosCamera::setPreviewWindow(preview_stream_ops *w)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    int width, height;
    int halPreviewFmt = 0;
    bool flagRestart = false;
    buffer_manager_type bufferType = BUFFER_MANAGER_ION_TYPE;

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

            return NO_ERROR;
        }
    } else {
        CLOGW("(%s):m_exynosCameraParameters is NULL. Skipped", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (previewEnabled() == true) {
        CLOGW("WRN(%s[%d]): preview is started, we forcely re-start preview", __FUNCTION__, __LINE__);
        flagRestart = true;
        m_disablePreviewCB = true;
        stopPreview();
    }

    m_previewWindow = w;

    if (m_scpBufferMgr != NULL) {
        CLOGD("DEBUG(%s[%d]): scp buffer manager need recreate", __FUNCTION__, __LINE__);
        m_scpBufferMgr->deinit();

        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
    }

    if (w == NULL) {
        bufferType = BUFFER_MANAGER_ION_TYPE;
        CLOGW("WARN(%s[%d]):window NULL, create internal buffer for preview", __FUNCTION__, __LINE__);
    } else {
        halPreviewFmt = m_exynosCameraParameters->getHalPixelFormat();
        bufferType = BUFFER_MANAGER_GRALLOC_TYPE;
        m_exynosCameraParameters->getHwPreviewSize(&width, &height);
        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator();

#ifdef RESERVED_MEMORY_FOR_GRALLOC_ENABLE
        if (!(((m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) && (getCameraId() == CAMERA_ID_BACK))
            || m_exynosCameraParameters->getRecordingHint() == true)) {
            ret = m_grAllocator->init(m_previewWindow, m_exynosconfig->current->bufInfo.num_preview_buffers,
                                    m_exynosconfig->current->bufInfo.preview_buffer_margin, (GRALLOC_SET_USAGE_FOR_CAMERA | GRALLOC_USAGE_CAMERA_RESERVED));
        } else
#endif
        {
            ret = m_grAllocator->init(m_previewWindow, m_exynosconfig->current->bufInfo.num_preview_buffers, m_exynosconfig->current->bufInfo.preview_buffer_margin);
        }

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):gralloc init fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto func_exit;
        }

        ret = m_grAllocator->setBuffersGeometry(width, height, halPreviewFmt);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):gralloc setBufferGeomety fail, size(%dx%d), fmt(%d), ret(%d)",
                __FUNCTION__, __LINE__, width, height, halPreviewFmt, ret);
            goto func_exit;
        }
    }

    m_createBufferManager(&m_scpBufferMgr, "SCP_BUF", bufferType);

    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE)
        m_scpBufferMgr->setAllocator(m_grAllocator);

    if (flagRestart == true) {
        startPreview();
    }

func_exit:
    m_disablePreviewCB = false;

    return ret;
}

void ExynosCamera::setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    int ret = 0;

    m_notifyCb        = notify_cb;
    m_dataCb          = data_cb;
    m_dataCbTimestamp = data_cb_timestamp;
    m_getMemoryCb     = get_memory;
    m_callbackCookie  = user;

    if (m_mhbAllocator == NULL)
        m_mhbAllocator = new ExynosCameraMHBAllocator();

    ret = m_mhbAllocator->init(get_memory);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:m_mhbAllocator init failed", __FUNCTION__, __LINE__);
    }
}

void ExynosCamera::enableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): enable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->enableMsgType(msgType);
    }
}

void ExynosCamera::disableMsgType(int32_t msgType)
{
    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): disable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_exynosCameraParameters->disableMsgType(msgType);
    }
}

bool ExynosCamera::msgTypeEnabled(int32_t msgType)
{
    bool IsEnabled = false;

    if (m_exynosCameraParameters) {
        CLOGV("INFO(%s[%d]): Msg type enabled (%x)", __FUNCTION__, __LINE__, msgType);
        IsEnabled = m_exynosCameraParameters->msgTypeEnabled(msgType);
    }

    return IsEnabled;
}

status_t ExynosCamera::startPreview()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    unsigned int fdCallbackSize = 0;

    m_hdrSkipedFcount = 0;
    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_curMinFps = 0;
    m_isNeedAllocPictureBuffer = false;
    m_flagThreadStop= false;
    m_frameSkipCount = 0;

#ifdef FIRST_PREVIEW_TIME_CHECK
    if (m_flagFirstPreviewTimerOn == false) {
        m_firstPreviewTimer.start();
        m_flagFirstPreviewTimerOn = true;

        CLOGD("DEBUG(%s[%d]):m_firstPreviewTimer start", __FUNCTION__, __LINE__);
    }
#endif

    if (m_previewEnabled == true) {
        return INVALID_OPERATION;
    }

    /* frame manager start */
    m_frameMgr->start();

    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;

    m_fdCallbackHeap = m_getMemoryCb(-1, fdCallbackSize, 1, m_callbackCookie);
    if (!m_fdCallbackHeap || m_fdCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, fdCallbackSize);
        goto err;
    }

    /*
     * This is for updating parameter value at once.
     * This must be just before making factory
     */
    m_exynosCameraParameters->updateTpuParameters();

    /* setup frameFactory with scenario */
    m_setupFrameFactory();

    /* vision */
    CLOGI("INFO(%s[%d]): getVisionMode(%d)", __FUNCTION__, __LINE__, m_exynosCameraParameters->getVisionMode());
    if (m_exynosCameraParameters->getVisionMode() == true) {
        ret = m_setVisionBuffers();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setVisionCallbackBuffer() fail", __FUNCTION__, __LINE__);
            return ret;
        }

        ret = m_setVisionCallbackBuffer();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setVisionCallbackBuffer() fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        if (m_visionFrameFactory->isCreated() == false) {
            ret = m_visionFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_visionFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
            CLOGD("DEBUG(%s):FrameFactory(VisionFrameFactory) created", __FUNCTION__);
        }

        m_exynosCameraParameters->setFrameSkipCount(INITIAL_SKIP_FRAME);

        ret = m_startVisionInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startVisionInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        m_visionThread->run(PRIORITY_DEFAULT);
        return NO_ERROR;
    } else {
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_exynosCameraParameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }

        if ((m_exynosCameraParameters->getRestartPreview() == true) ||
            m_previewBufferCount != m_exynosCameraParameters->getPreviewBufferCount()) {
            ret = setPreviewWindow(m_previewWindow);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            m_previewBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
        }

        CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
        m_setBuffersThread->run(PRIORITY_DEFAULT);

        if (m_captureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;
#ifdef DEBUG_RAWDUMP
            bufMgr = m_bayerBufferMgr;
#else
            if (m_exynosCameraParameters->isReprocessing() == true)
                bufMgr = m_bayerBufferMgr;
#endif
            m_captureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr, m_frameMgr);

            if (m_exynosCameraParameters->isReprocessing() == true) {
                ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]): setFrameHoldCount(%d) is fail", __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_sccCaptureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            if (m_exynosCameraParameters->isSccCapture() == true
                || m_exynosCameraParameters->isUsing3acForIspc() == true) {
                /* TODO: Dynamic select buffer manager for capture */
                bufMgr = m_sccBufferMgr;
            }

            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_exynosCameraParameters, bufMgr, m_frameMgr);
        }

        if (m_captureSelector != NULL)
            m_captureSelector->release();

        if (m_sccCaptureSelector != NULL)
            m_sccCaptureSelector->release();

        if (m_previewFrameFactory->isCreated() == false) {
            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
            CLOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
        }

        if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
            m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK &&
            m_exynosCameraParameters->getDualMode() == false &&
            m_exynosCameraParameters->getRecordingHint() == false &&
            m_isFirstStart == true) {

            ret = m_fastenAeStable();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
                ret = INVALID_OPERATION;
                goto err;
            } else {
                skipFrameCount = 0;
                m_exynosCameraParameters->setUseFastenAeStable(false);
            }
        } else if(m_exynosCameraParameters->getDualMode() == true) {
            skipFrameCount = INITIAL_SKIP_FRAME + 2;
        }

#ifdef SET_FPS_SCENE /* This codes for 5260, Do not need other project */
        struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
        if (initMetaData != NULL) {
            m_exynosCameraParameters->duplicateCtrlMetadata(initMetaData);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[0], PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[1], PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_SCENE_MODE, initMetaData->shot.ctl.aa.sceneMode, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            delete initMetaData;
            initMetaData = NULL;
        } else {
            CLOGE("ERR(%s[%d]):initMetaData is NULL", __FUNCTION__, __LINE__);
        }
#elif SET_FPS_FRONTCAM
        if (m_exynosCameraParameters->getCameraId() == CAMERA_ID_FRONT) {
            struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
            if (initMetaData != NULL) {
                m_exynosCameraParameters->duplicateCtrlMetadata(initMetaData);
                CLOGD("(%s:[%d]) : setControl for Frame Range.", __FUNCTION__, __LINE__);

                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[0], PIPE_FLITE_FRONT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, initMetaData->shot.ctl.aa.aeTargetFpsRange[1], PIPE_FLITE_FRONT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

                delete initMetaData;
                initMetaData = NULL;
            } else {
                CLOGE("ERR(%s[%d]):initMetaData is NULL", __FUNCTION__, __LINE__);
            }
        }
#endif

        m_exynosCameraParameters->setFrameSkipCount(skipFrameCount);

        m_setBuffersThread->join();

        ret = m_startPreviewInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startPreviewInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        if (m_exynosCameraParameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
            if (!m_exynosCameraParameters->getDualRecordingHint() && !m_exynosCameraParameters->getUHDRecordingMode())
#endif
            {
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);
            }
#endif
        } else {
            m_pictureFrameFactory = m_previewFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);

            /*
             * Make remained frameFactory here.
             * in case of SCC capture, make here.
             */
            m_framefactoryThread->run();
        }

#if defined(USE_UHD_RECORDING) && !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
        if (!m_exynosCameraParameters->getDualRecordingHint() && !m_exynosCameraParameters->getUHDRecordingMode())
#endif
        {
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }

        if (m_previewWindow != NULL)
            m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

        /* setup frame thread */
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
        } else {
#ifdef DEBUG_RAWDUMP
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
#endif
            m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
        }

        if (m_facedetectThread->isRunning() == false)
            m_facedetectThread->run();

        m_previewThread->run(PRIORITY_DISPLAY);
        m_mainThread->run(PRIORITY_DEFAULT);
        m_autoFocusContinousThread->run(PRIORITY_DEFAULT);
        m_monitorThread->run(PRIORITY_DEFAULT);

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == false)) {
            CLOGD("DEBUG(%s[%d]):High resolution preview callback start", __FUNCTION__, __LINE__);
            if (skipFrameCount > 0)
                m_skipReprocessing = true;
            m_highResolutionCallbackRunning = true;
            if (m_exynosCameraParameters->isReprocessing() == true) {
                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
                m_startPictureInternalThread->join();
            }
            m_prePictureThread->run(PRIORITY_DEFAULT);
        }

        /* FD-AE is always on */
#ifdef USE_FD_AE
        if ((m_exynosCameraParameters->getDualMode() == true && m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK) ||
            (m_exynosCameraParameters->getVtMode() != 0) ||
            (m_exynosCameraParameters->getRecordingHint() == true)) {
            m_startFaceDetection(false);
        } else {
            m_startFaceDetection(true);
        }
#endif

        if (m_exynosCameraParameters->getUseFastenAeStable() == true &&
            m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK &&
            m_exynosCameraParameters->getDualMode() == false &&
            m_exynosCameraParameters->getRecordingHint() == false &&
            m_isFirstStart == true) {
            /* AF mode is setted as INFINITY in fastenAE, and we should update that mode */
            m_exynosCameraActivityControl->setAutoFocusMode(FOCUS_MODE_INFINITY);

            m_exynosCameraParameters->setUseFastenAeStable(false);
            m_exynosCameraActivityControl->setAutoFocusMode(m_exynosCameraParameters->getFocusMode());
            m_isFirstStart = false;
            m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);
        }
    }

#ifdef BURST_CAPTURE
    m_burstInitFirst = true;
#endif
    return NO_ERROR;

err:

    /* frame manager stop */
    m_frameMgr->stop();

    m_setBuffersThread->join();

    m_releaseBuffers();

    return ret;
}

void ExynosCamera::stopPreview()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrame *frame = NULL;

    /* release about frameFactory */
    m_framefactoryThread->requestExitAndWait();
    m_frameFactoryQ->release();

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        return;
    }

    if (m_exynosCameraParameters->getVisionMode() == true) {
        m_visionThread->requestExitAndWait();
        ret = m_stopVisionInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopVisionInternal fail", __FUNCTION__, __LINE__);
    } else {
        m_startPictureInternalThread->join();
        m_startPictureBufferThread->join();

        m_exynosCameraActivityControl->cancelAutoFocus();

        CLOGD("DEBUG(%s[%d]): (%d, %d)", __FUNCTION__, __LINE__, m_flashMgr->getNeedCaptureFlash(), m_pictureEnabled);
        if (m_flashMgr->getNeedCaptureFlash() == true && m_pictureEnabled == true) {
            CLOGD("DEBUG(%s[%d]): force flash off", __FUNCTION__, __LINE__);
            m_exynosCameraActivityControl->cancelFlash();
            autoFocusMgr->stopAutofocus();
            m_isTryStopFlash = true;
            m_exitAutoFocusThread = true;
        }

        int flashMode = AA_FLASHMODE_OFF;
        int waitingTime = FLASH_OFF_MAX_WATING_TIME / TOTAL_FLASH_WATING_COUNT; /* Max waiting time: 500ms, Count:10, Waiting time: 50ms */

        flashMode = m_flashMgr->getFlashStatus();
        if ((flashMode == AA_FLASHMODE_ON_ALWAYS) || (m_flashMgr->getNeedFlashOffDelay() == true)) {
            CLOGD("DEBUG(%s[%d]): flash torch was enabled", __FUNCTION__, __LINE__);

            m_exynosCameraParameters->setFrameSkipCount(100);
            usleep(FLASH_OFF_MAX_WATING_TIME);

            for (int i = 0; i < TOTAL_FLASH_WATING_COUNT; ++i) {
                flashMode = m_flashMgr->getFlashStatus();
                if (flashMode == AA_FLASHMODE_OFF || flashMode == AA_FLASHMODE_CANCEL) {
                    m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
                    break;
                }
                usleep(waitingTime);
            }
        } else if (m_isTryStopFlash == true) {
            usleep(150000);
            m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
        }

        m_flashMgr->setNeedFlashOffDelay(false);

        m_previewFrameFactory->setStopFlag();
        if (m_exynosCameraParameters->isReprocessing() == true && m_reprocessingFrameFactory->isCreated() == true)
            m_reprocessingFrameFactory->setStopFlag();
        m_flagThreadStop = true;

        m_takePictureCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_pictureCounter.clearCount();
        m_jpegCounter.clearCount();
        m_captureSelector->cancelPicture();

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
            m_skipReprocessing = false;
            m_highResolutionCallbackRunning = false;
            CLOGD("DEBUG(%s[%d]):High resolution preview callback stop", __FUNCTION__, __LINE__);
            if(getCameraId() == CAMERA_ID_FRONT) {
                m_sccCaptureSelector->cancelPicture();
                m_sccCaptureSelector->wakeupQ();
                CLOGD("DEBUG(%s[%d]):High resolution m_sccCaptureSelector cancel", __FUNCTION__, __LINE__);
            }

            m_prePictureThread->requestExitAndWait();
            m_highResolutionCallbackQ->release();
        }

        ret = m_stopPictureInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

        m_exynosCameraActivityControl->stopAutoFocus();
        m_autoFocusThread->requestExitAndWait();

        if (m_previewQ != NULL) {
            m_previewQ->sendCmd(WAKE_UP);
        } else {
            CLOGI("INFO(%s[%d]): m_previewQ is NULL", __FUNCTION__, __LINE__);
        }

        m_pipeFrameDoneQ->sendCmd(WAKE_UP);
        m_mainThread->requestExitAndWait();
        m_monitorThread->requestExitAndWait();

        m_shutterCallbackThread->requestExitAndWait();

        m_previewThread->stop();
        if (m_previewQ != NULL) {
            m_previewQ->sendCmd(WAKE_UP);
        }
        m_previewThread->requestExitAndWait();

        if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
            m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();

            if (m_mainSetupQThread[INDEX(PIPE_3AC)] != NULL) {
                m_mainSetupQThread[INDEX(PIPE_3AC)]->stop();
                m_mainSetupQ[INDEX(PIPE_3AC)]->sendCmd(WAKE_UP);
                m_mainSetupQThread[INDEX(PIPE_3AC)]->requestExitAndWait();
            }

            m_mainSetupQThread[INDEX(PIPE_3AA)]->stop();
            m_mainSetupQ[INDEX(PIPE_3AA)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_3AA)]->requestExitAndWait();

            if (m_mainSetupQThread[INDEX(PIPE_ISP)] != NULL) {
                m_mainSetupQThread[INDEX(PIPE_ISP)]->stop();
                m_mainSetupQ[INDEX(PIPE_ISP)]->sendCmd(WAKE_UP);
                m_mainSetupQThread[INDEX(PIPE_ISP)]->requestExitAndWait();
            }

            /* Comment out, because it included ISP */
            /* m_mainSetupQThread[INDEX(PIPE_SCP)]->requestExitAndWait(); */
        }

        m_autoFocusContinousThread->requestExitAndWait();
        m_autoFocusContinousQ.release();

        m_facedetectThread->stop();
        m_facedetectQ->sendCmd(WAKE_UP);
        m_facedetectThread->requestExitAndWait();
        while (m_facedetectQ->getSizeOfProcessQ()) {
            m_facedetectQ->popProcessQ(&frame);
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

        if (m_previewQ != NULL) {
             m_clearList(m_previewQ);
        }

        if (m_zoomPreviwWithCscQ != NULL) {
             m_zoomPreviwWithCscQ->release();
        }

        if (m_previewCallbackGscFrameDoneQ != NULL) {
            m_clearList(m_previewCallbackGscFrameDoneQ);
        }

        for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
            m_clearList(m_mainSetupQ[i]);
        }

        ret = m_stopPreviewInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
    }

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->resetBuffers();
    }

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
        m_sccBufferMgr->setBufferCount(0);
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }

    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }

    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }
    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

#if 0
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGE(" m_setBuffers free all buffers");
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }
#endif
    /* frame manager stop */
    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();


    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();

    m_hdrSkipedFcount = 0;
    m_dynamicSccCount = 0;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_isNeedAllocPictureBuffer = false;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

    m_burstInitFirst = false;

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /*
     * HACK
     * This is force-reset frameFactory adn companion
     */
    m_deinitFrameFactory();
    m_initFrameFactory();
#endif
}

bool ExynosCamera::previewEnabled()
{
    CLOGI("INFO(%s[%d]):m_previewEnabled=%d",
        __FUNCTION__, __LINE__, (int)m_previewEnabled);

    /* in scalable mode, we should controll out state */
    if (m_exynosCameraParameters != NULL &&
        (m_exynosCameraParameters->getScalableSensorMode() == true) &&
        (m_scalableSensorMgr.getMode() == EXYNOS_CAMERA_SCALABLE_CHANGING))
        return true;
    else
        return m_previewEnabled;
}

status_t ExynosCamera::storeMetaDataInBuffers(bool enable)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return OK;
}

status_t ExynosCamera::startRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();

    if (m_getRecordingEnabled() == true) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals true", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

#ifdef USE_FD_AE
    if (m_exynosCameraParameters != NULL) {
        m_startFaceDetection(false);
    } else {
        CLOGW("(%s[%d]):m_exynosCameraParameters is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }
#endif


    /* Do start recording process */
    ret = m_startRecordingInternal();
    if (ret < 0) {
        CLOGE("ERR");
        return ret;
    }

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_setRecordingEnabled(true);

    autoFocusMgr->setRecordingHint(true);
    flashMgr->setRecordingHint(true);

func_exit:
    /* wait for initial preview skip */
    if (m_exynosCameraParameters != NULL) {
        int retry = 0;
        while (m_exynosCameraParameters->getFrameSkipCount() > 0 && retry < 3) {
            retry++;
            usleep(33000);
            CLOGI("INFO(%s[%d]): -OUT- (frameSkipCount:%d) (retry:%d)", __FUNCTION__, __LINE__, m_frameSkipCount, retry);
        }
    }

    return NO_ERROR;
}

void ExynosCamera::stopRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    m_skipCount = 0;

    if (m_getRecordingEnabled() == false) {
        return;
    }
    m_setRecordingEnabled(false);

    /* Do stop recording process */

    ret = m_stopRecordingInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopRecordingInternal fail", __FUNCTION__, __LINE__);

#ifdef USE_FD_AE
    if (m_exynosCameraParameters != NULL) {
        if ((m_exynosCameraParameters->getDualMode() == true && m_exynosCameraParameters->getCameraId() == CAMERA_ID_BACK) ||
            (m_exynosCameraParameters->getVtMode() != 0)) {
            m_startFaceDetection(false);
        } else {
            m_startFaceDetection(true);
        }
    }
#endif

    autoFocusMgr->setRecordingHint(false);
    flashMgr->setRecordingHint(false);
    flashMgr->setNeedFlashOffDelay(false);
}

bool ExynosCamera::recordingEnabled()
{
    bool ret = m_getRecordingEnabled();
    CLOGI("INFO(%s[%d]):m_recordingEnabled=%d",
        __FUNCTION__, __LINE__, (int)ret);

    return ret;
}

void ExynosCamera::releaseRecordingFrame(const void *opaque)
{
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    if (m_getRecordingEnabled() == false) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals false", __FUNCTION__, __LINE__);
        return;
    }

    if (m_recordingCallbackHeap == NULL) {
        CLOGW("WARN(%s[%d]):recordingCallbackHeap equals NULL", __FUNCTION__, __LINE__);
        return;
    }

    bool found = false;
    struct addrs *recordAddrs  = (struct addrs *)m_recordingCallbackHeap->data;
    struct addrs *releaseFrame = (struct addrs *)opaque;

    if (recordAddrs != NULL) {
        for (int32_t i = 0; i < m_recordingBufferCount; i++) {
            if ((char *)(&(recordAddrs[i])) == (char *)opaque) {
                found = true;
                CLOGV("DEBUG(%s[%d]):releaseFrame->bufIndex=%d, fdY=%d",
                    __FUNCTION__, __LINE__, releaseFrame->bufIndex, releaseFrame->fdPlaneY);

                if (m_doCscRecording == true) {
                    m_releaseRecordingBuffer(releaseFrame->bufIndex);
                } else {
                    m_recordingTimeStamp[releaseFrame->bufIndex] = 0L;
                    m_recordingBufAvailable[releaseFrame->bufIndex] = true;
                }

                break;
            }
            m_isFirstStart = false;
            if (m_exynosCameraParameters != NULL) {
                m_exynosCameraParameters->setIsFirstStartFlag(m_isFirstStart);
            }
        }
    }

    if (found == false)
        CLOGW("WARN(%s[%d]):**** releaseFrame not founded ****", __FUNCTION__, __LINE__);

}

status_t ExynosCamera::autoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    if (m_autoFocusRunning == false) {
        m_autoFocusType = AUTO_FOCUS_SERVICE;
        m_autoFocusThread->requestExitAndWait();
        m_autoFocusThread->run(PRIORITY_DEFAULT);
    } else {
        CLOGW("WRN(%s[%d]): auto focus is inprogressing", __FUNCTION__, __LINE__);
    }

#if 0 // not used.
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb lock", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->m_setAutoExposureLock(true);
            m_exynosCameraParameters->m_setAutoWhiteBalanceLock(true);
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::cancelAutoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#if 0 // not used.
    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb unlock", __FUNCTION__, __LINE__);
            m_exynosCameraParameters->m_setAutoExposureLock(false);
            m_exynosCameraParameters->m_setAutoWhiteBalanceLock(false);
        }
    }
#endif

    if (m_exynosCameraActivityControl->cancelAutoFocus() == false) {
        CLOGE("ERR(%s):Fail on m_secCamera->cancelAutoFocus()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::takePicture()
{
    int ret = 0;
    int takePictureCount = m_takePictureCounter.getCount();
    int seriesShotCount = 0;
    int currentSeriesShotMode = 0;
    ExynosCameraFrame *newFrame = NULL;
    int32_t reprocessingBayerMode = 0;

    int retryCount = 0;

    if (m_exynosCameraParameters != NULL) {
        seriesShotCount = m_exynosCameraParameters->getSeriesShotCount();
        currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();
        reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    } else {
        CLOGE("ERR(%s):m_exynosCameraParameters is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

    m_exynosCameraParameters->setMarkingOfExifFlash(0);

    /* HACK Reset Preview Flag*/
    while ((m_resetPreview == true) && (retryCount < 10)) {
        usleep(200000);
        retryCount ++;
        CLOGI("INFO(%s[%d]) retryCount(%d) m_resetPreview(%d)", __FUNCTION__, __LINE__, retryCount, m_resetPreview);
    }

    if (takePictureCount < 0) {
        CLOGE("ERR(%s[%d]): takePicture is called too much. takePictureCount(%d) / seriesShotCount(%d) . so, fail",
            __FUNCTION__, __LINE__, takePictureCount, seriesShotCount);
        return INVALID_OPERATION;
    } else if (takePictureCount == 0) {
        if (seriesShotCount == 0) {
            m_captureLock.lock();
            if (m_pictureEnabled == true) {
                CLOGE("ERR(%s[%d]): take picture is inprogressing", __FUNCTION__, __LINE__);
                /* return NO_ERROR; */
                m_captureLock.unlock();
                return INVALID_OPERATION;
            }
            m_captureLock.unlock();

            /* general shot */
            seriesShotCount = 1;
        }
        m_takePictureCounter.setCount(seriesShotCount);
    }

    CLOGI("INFO(%s[%d]): takePicture start m_takePictureCounter(%d), currentSeriesShotMode(%d) seriesShotCount(%d)",
        __FUNCTION__, __LINE__, m_takePictureCounter.getCount(), currentSeriesShotMode, seriesShotCount);

    m_printExynosCameraInfo(__FUNCTION__);

    if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        m_hdrEnabled = true;
    } else {
        m_hdrEnabled = false;
    }

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequestFLITE(true);
            ret = generateFrame(-1, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }
            m_previewFrameFactory->setRequestFLITE(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
    } else if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Comment out, because it included 3AA, it always running */
        /*
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequest3AC(true);
            ret = generateFrame(-1, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                return ret;
            }
            m_previewFrameFactory->setRequest3AC(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
        */
        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0)
            m_previewFrameFactory->setRequest3AC(true);
    } else if (m_exynosCameraParameters->getRecordingHint() == true
               && m_exynosCameraParameters->isUsing3acForIspc() == true) {
        if (m_sccBufferMgr->getNumOfAvailableBuffer() > 0)
            m_previewFrameFactory->setRequest3AC(true);
    }


    if (m_takePictureCounter.getCount() == seriesShotCount) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();

        m_stopBurstShot = false;

        if (m_exynosCameraParameters->isReprocessing() == true)
            m_captureSelector->setIsFirstFrame(true);
        else
            m_sccCaptureSelector->setIsFirstFrame(true);

#if 0
        if (m_exynosCameraParameters->isReprocessing() == false || m_exynosCameraParameters->getSeriesShotCount() > 0 ||
                m_hdrEnabled == true) {
            m_pictureFrameFactory = m_previewFrameFactory;
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                if (isOwnScc(getCameraId()) == true)
                    m_previewFrameFactory->setRequestSCC(true);
                else
                    m_previewFrameFactory->setRequestISPC(true);

                /* boosting dynamic SCC */
                if (m_hdrEnabled == false &&
                    currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
                    ret = m_boostDynamicCapture();
                    if (ret < 0)
                        CLOGW("WRN(%s[%d]): fail to boosting dynamic capture", __FUNCTION__, __LINE__);
                }

            }
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;
        }
#endif

        if (m_exynosCameraParameters->getScalableSensorMode()) {
            m_exynosCameraParameters->setScalableSensorMode(false);
            stopPreview();
            setPreviewWindow(m_previewWindow);
            startPreview();
            m_exynosCameraParameters->setScalableSensorMode(true);
        }

        CLOGI("INFO(%s[%d]): takePicture enabled, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_pictureEnabled = true;
        m_takePictureCounter.decCount();
        m_isNeedAllocPictureBuffer = true;

        m_startPictureBufferThread->join();

        if (m_exynosCameraParameters->isReprocessing() == true) {
            m_startPictureInternalThread->join();

#ifdef BURST_CAPTURE
            if(seriesShotCount > 1) {
                int allocCount = 0;
                int addCount = 0;
                CLOGD("DEBUG(%s[%d]): realloc buffer for burst shot", __FUNCTION__, __LINE__);
                m_burstRealloc = false;

                allocCount = m_sccReprocessingBufferMgr->getAllocatedBufferCount();
                addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                if( addCount > 0 ){
                    m_sccReprocessingBufferMgr->increase(addCount);
                }

                allocCount = m_jpegBufferMgr->getAllocatedBufferCount();
                addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                if( addCount > 0 ){
                    m_jpegBufferMgr->increase(addCount);
                }
                m_isNeedAllocPictureBuffer = true;
            }
#endif
        }

        CLOGD("DEBUG(%s[%d]): currentSeriesShotMode(%d), m_flashMgr->getNeedCaptureFlash(%d)",
            __FUNCTION__, __LINE__, currentSeriesShotMode, m_flashMgr->getNeedCaptureFlash());

        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            m_sCaptureMgr->resetHdrStartFcount();
            m_exynosCameraParameters->setFrameSkipCount(13);
        } else if ((m_flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE)) {
            m_exynosCameraParameters->setMarkingOfExifFlash(1);

            if (m_flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                m_flashMgr->setCaptureStatus(true);
                CLOGD("DEBUG(%s[%d]): checkPreFlash(false), Start auto focus internally", __FUNCTION__, __LINE__);
                m_autoFocusType = AUTO_FOCUS_HAL;
                m_flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);
                m_flashMgr->setFlashWaitCancel(false);

                /* execute autoFocus for preFlash */
                m_autoFocusThread->requestExitAndWait();
                m_autoFocusThread->run(PRIORITY_DEFAULT);
            }
        }

        m_exynosCameraParameters->setSetfileYuvRange();

        m_reprocessingCounter.setCount(seriesShotCount);
        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }
        }

        m_jpegCounter.setCount(seriesShotCount);
        m_pictureCounter.setCount(seriesShotCount);
        if (m_pictureThread->isRunning() == false)
            ret = m_pictureThread->run();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
            return INVALID_OPERATION;
        }

        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
    } else {
        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
            /* series shot : push buffer to callback thread. */
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
        CLOGI("INFO(%s[%d]): series shot takePicture, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_takePictureCounter.decCount();

        /* TODO : in case of no reprocesssing, we make zsl scheme or increse buf */
        if (m_exynosCameraParameters->isReprocessing() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera::cancelPicture()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

            return NO_ERROR;
        }
    }

/*
    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
*/

    return NO_ERROR;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

#ifdef SCALABLE_ON
    m_exynosCameraParameters->setScalableSensorMode(true);
#else
    m_exynosCameraParameters->setScalableSensorMode(false);
#endif

    ret = m_exynosCameraParameters->setParameters(params);
#if 1
    /* HACK Reset Preview Flag*/
    if (m_exynosCameraParameters->getRestartPreview() == true && m_previewEnabled == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGI("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);

        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
    }
#endif
    return ret;

}

CameraParameters ExynosCamera::getParameters() const
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return m_exynosCameraParameters->getParameters();
}

int ExynosCamera::getMaxNumDetectedFaces(void)
{
    return m_exynosCameraParameters->getMaxNumDetectedFaces();
}

bool ExynosCamera::m_startFaceDetection(bool toggle)
{
    CLOGD("DEBUG(%s[%d]) toggle : %d", __FUNCTION__, __LINE__, toggle);

    if (toggle == true) {
        m_exynosCameraParameters->setFdEnable(true);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_FULL);
    } else {
        m_exynosCameraParameters->setFdEnable(false);
        m_exynosCameraParameters->setFdMode(FACEDETECT_MODE_OFF);
    }

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));

    return true;
}

bool ExynosCamera::startFaceDetection(void)
{
    if (m_flagStartFaceDetection == true) {
        CLOGD("DEBUG(%s):Face detection already started..", __FUNCTION__);
        return true;
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(true);
#endif

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        CLOGE("ERR(%s[%d]):startFaceDetection recordingQ(%d)", __FUNCTION__, __LINE__, m_facedetectQ->getSizeOfProcessQ());
        m_clearList(m_facedetectQ);
    }

    m_flagStartFaceDetection = true;

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    return true;
}

bool ExynosCamera::stopFaceDetection(void)
{
    if (m_flagStartFaceDetection == false) {
        CLOGD("DEBUG(%s [%d]):Face detection already stopped..", __FUNCTION__, __LINE__);
        return true;
    }

    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (autoFocusMgr->setFaceDetection(false) == false) {
        CLOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, false);
    } else {
        /* restart CAF when FD mode changed */
        switch (autoFocusMgr->getAutofocusMode()) {
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE:
        case ExynosCameraActivityAutofocus::AUTOFOCUS_MODE_CONTINUOUS_PICTURE_MACRO:
            if (autoFocusMgr->flagAutofocusStart() == true &&
                autoFocusMgr->flagLockAutofocus() == false) {
                autoFocusMgr->stopAutofocus();
                autoFocusMgr->startAutofocus();
            }
            break;
        default:
            break;
        }
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
#else
    m_startFaceDetection(false);
#endif
    m_flagStartFaceDetection = false;

    return true;
}

bool ExynosCamera::startCurrentSet(void)
{
    m_currentSetStart = true;

    return true;
}

bool ExynosCamera::stopCurrentSet(void)
{
    m_currentSetStart = false;

    return true;
}

bool ExynosCamera::m_getRecordingEnabled(void)
{
    Mutex::Autolock lock(m_recordingStateLock);
    return m_recordingEnabled;
}

void ExynosCamera::m_setRecordingEnabled(bool enable)
{
    Mutex::Autolock lock(m_recordingStateLock);
    m_recordingEnabled = enable;
    return;
}

int ExynosCamera::m_calibratePosition(int w, int new_w, int pos)
{
    return (float)(pos * new_w) / (float)w;
}

bool ExynosCamera::m_facedetectThreadFunc(void)
{
    int32_t status = 0;
    bool ret = true;

    int index = 0;
    int count = 0;

    ExynosCameraFrame *newFrame = NULL;
    uint32_t frameCnt = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        ret = false;
        goto func_exit;
    }

    status = m_facedetectQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d) m_flagStartFaceDetection(%d)", __FUNCTION__, __LINE__, m_flagThreadStop, m_flagStartFaceDetection);
        ret = false;
        goto func_exit;
    }

    if (status < 0) {
        if (status == TIMED_OUT) {
            /* Face Detection time out is not meaningful */
        } else {
            /* TODO: doing exception handling */
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
        }
        goto func_exit;
    }

    count = m_facedetectQ->getSizeOfProcessQ();
    if (count >= MAX_FACEDETECT_THREADQ_SIZE) {
        CLOGE("ERR(%s[%d]):m_facedetectQ  skipped QSize(%d) frame(%d)", __FUNCTION__, __LINE__,  count, newFrame->getFrameCount());
        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
        for (int i = 0 ; i < count-1 ; i++) {
            m_facedetectQ->popProcessQ(&newFrame);
            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }
        }
        m_facedetectQ->popProcessQ(&newFrame);
    }

    status = m_doFdCallbackFunc(newFrame);
    if (status < 0) {
        CLOGE("ERR(%s[%d]) m_doFdCallbackFunc failed(%d).", __FUNCTION__, __LINE__, status);
    }

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        ret = true;
    }

    if (newFrame != NULL) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrame *frame)
{
    ExynosCameraDurationTimer       m_fdcallbackTimer;
    long long                       m_fdcallbackTimerTime;

    struct camera2_shot_ext *meta_shot_ext = NULL;
    meta_shot_ext = new struct camera2_shot_ext;
    if (meta_shot_ext == NULL) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    memset(meta_shot_ext, 0x00, sizeof(struct camera2_shot_ext));
    if (frame->getDynamicMeta(meta_shot_ext) != NO_ERROR) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        if (meta_shot_ext) {
            delete meta_shot_ext;
            meta_shot_ext = NULL;
        }
        return INVALID_OPERATION;
    }

    if (m_flagStartFaceDetection == true) {
        int id[NUM_OF_DETECTED_FACES];
        int score[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedLeftEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedRightEye[NUM_OF_DETECTED_FACES];
        ExynosRect2 detectedMouth[NUM_OF_DETECTED_FACES];
        int numOfDetectedFaces = 0;
        int num = 0;
        struct camera2_dm *dm = NULL;
        int previewW, previewH;

        memset(&id, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);
        memset(&score, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);

        m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);

        dm = &(meta_shot_ext->shot.dm);
        if (dm == NULL) {
            CLOGE("ERR(%s[%d]) dm data is null", __FUNCTION__, __LINE__);
            delete meta_shot_ext;
            meta_shot_ext = NULL;
            return INVALID_OPERATION;
        }

        CLOGV("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, dm->stats.faceDetectMode);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][0], dm->stats.faceRectangles[0][1]);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][2], dm->stats.faceRectangles[0][3]);

       num = NUM_OF_DETECTED_FACES;
        if (getMaxNumDetectedFaces() < num)
            num = getMaxNumDetectedFaces();

        switch (dm->stats.faceDetectMode) {
        case FACEDETECT_MODE_SIMPLE:
        case FACEDETECT_MODE_FULL:
            break;
        case FACEDETECT_MODE_OFF:
        default:
            num = 0;
            break;
        }

        for (int i = 0; i < num; i++) {
            if (dm->stats.faceIds[i]) {
                switch (dm->stats.faceDetectMode) {
                case FACEDETECT_MODE_OFF:
                    break;
                case FACEDETECT_MODE_SIMPLE:
                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                    numOfDetectedFaces++;
                    break;
                case FACEDETECT_MODE_FULL:
                    id[i] = dm->stats.faceIds[i];
                    score[i] = dm->stats.faceScores[i];

                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];

                    detectedLeftEye[i].x1
                        = detectedLeftEye[i].y1
                        = detectedLeftEye[i].x2
                        = detectedLeftEye[i].y2 = -1;

                    detectedRightEye[i].x1
                        = detectedRightEye[i].y1
                        = detectedRightEye[i].x2
                        = detectedRightEye[i].y2 = -1;

                    detectedMouth[i].x1
                        = detectedMouth[i].y1
                        = detectedMouth[i].x2
                        = detectedMouth[i].y2 = -1;

                    numOfDetectedFaces++;
                    break;
                default:
                    break;
                }
            }
        }

        if (0 < numOfDetectedFaces) {
            /*
             * camera.h
             * width   : -1000~1000
             * height  : -1000~1000
             * if eye, mouth is not detectable : -2000, -2000.
             */
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

            int realNumOfDetectedFaces = 0;

            for (int i = 0; i < numOfDetectedFaces; i++) {
                /*
                 * over 50s, we will catch
                 * if (score[i] < 50)
                 *    continue;
                */
                m_faces[realNumOfDetectedFaces].rect[0] = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[1] = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
                m_faces[realNumOfDetectedFaces].rect[2] = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
                m_faces[realNumOfDetectedFaces].rect[3] = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;

                m_faces[realNumOfDetectedFaces].id = id[i];
                m_faces[realNumOfDetectedFaces].score = score[i] > 100 ? 100 : score[i];

                m_faces[realNumOfDetectedFaces].left_eye[0] = (detectedLeftEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedLeftEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].left_eye[1] = (detectedLeftEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedLeftEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].right_eye[0] = (detectedRightEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedRightEye[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].right_eye[1] = (detectedRightEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedRightEye[i].y1) - 1000;

                m_faces[realNumOfDetectedFaces].mouth[0] = (detectedMouth[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedMouth[i].x1) - 1000;
                m_faces[realNumOfDetectedFaces].mouth[1] = (detectedMouth[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedMouth[i].y1) - 1000;

                CLOGV("face posision(cal:%d,%d %dx%d)(org:%d,%d %dx%d), id(%d), score(%d)",
                    m_faces[realNumOfDetectedFaces].rect[0], m_faces[realNumOfDetectedFaces].rect[1],
                    m_faces[realNumOfDetectedFaces].rect[2], m_faces[realNumOfDetectedFaces].rect[3],
                    detectedFace[i].x1, detectedFace[i].y1,
                    detectedFace[i].x2, detectedFace[i].y2,
                    m_faces[realNumOfDetectedFaces].id,
                    m_faces[realNumOfDetectedFaces].score);

                CLOGV("DEBUG(%s[%d]): left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
                        __FUNCTION__, __LINE__,
                        m_faces[realNumOfDetectedFaces].left_eye[0],
                        m_faces[realNumOfDetectedFaces].left_eye[1],
                        m_faces[realNumOfDetectedFaces].right_eye[0],
                        m_faces[realNumOfDetectedFaces].right_eye[1],
                        m_faces[realNumOfDetectedFaces].mouth[0],
                        m_faces[realNumOfDetectedFaces].mouth[1],
                        realNumOfDetectedFaces
                     );

                realNumOfDetectedFaces++;
            }

            m_frameMetadata.number_of_faces = realNumOfDetectedFaces;
            m_frameMetadata.faces = m_faces;

            m_faceDetected = true;
            m_fdThreshold = 0;
        } else {
            if (m_faceDetected == true && m_fdThreshold < NUM_OF_DETECTED_FACES_THRESHOLD) {
                /* waiting the unexpected fail about face detected */
                m_fdThreshold++;
            } else {
                if (0 < m_frameMetadata.number_of_faces)
                    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

                m_frameMetadata.number_of_faces = 0;
                m_frameMetadata.faces = m_faces;
                m_fdThreshold = 0;
                m_faceDetected = false;
            }
        }
    } else {
        if (0 < m_frameMetadata.number_of_faces)
            memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

        m_frameMetadata.number_of_faces = 0;
        m_frameMetadata.faces = m_faces;

        m_fdThreshold = 0;

        m_faceDetected = false;
    }

#ifdef TOUCH_AE
    if(((m_exynosCameraParameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH) && (m_exynosCameraParameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH))
        && ((meta_shot_ext->shot.dm.aa.aeState == AE_STATE_CONVERGED) || (meta_shot_ext->shot.dm.aa.aeState == AE_STATE_FLASH_REQUIRED))) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGV("INFO(%s[%d]): AE_RESULT(%d)", __FUNCTION__, __LINE__, meta_shot_ext->shot.dm.aa.aeState);
    }
#endif

    delete meta_shot_ext;
    meta_shot_ext = NULL;

    m_fdcallbackTimer.start();

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
            (m_flagStartFaceDetection || m_flagLLSStart || m_flagLightCondition))
    {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
        m_dataCb(CAMERA_MSG_PREVIEW_METADATA, m_fdCallbackHeap, 0, &m_frameMetadata, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
    }
    m_fdcallbackTimer.stop();
    m_fdcallbackTimerTime = m_fdcallbackTimer.durationUsecs();

    if((int)m_fdcallbackTimerTime / 1000 > 50) {
        CLOGD("INFO(%s[%d]): FD callback duration time : (%d)mec", __FUNCTION__, __LINE__, (int)m_fdcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;
    CLOGV("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters != NULL) {
        if (m_exynosCameraParameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    } else {
        CLOGE("ERR(%s):m_exynosCameraParameters is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    /* TO DO : implemented based on the command */
    switch(command) {
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        if (getMaxNumDetectedFaces() == 0) {
            CLOGE("ERR(%s):getMaxNumDetectedFaces == 0", __FUNCTION__);
            return BAD_VALUE;
        }

        if (arg1 == CAMERA_FACE_DETECTION_SW) {
            CLOGD("DEBUG(%s):only support HW face dectection", __FUNCTION__);
            return BAD_VALUE;
        }

        if (command == CAMERA_CMD_START_FACE_DETECTION) {
            CLOGD("DEBUG(%s):CAMERA_CMD_START_FACE_DETECTION is called!", __FUNCTION__);
            if (m_flagStartFaceDetection == false
                && startFaceDetection() == false) {
                CLOGE("ERR(%s):startFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        } else {
            CLOGD("DEBUG(%s):CAMERA_CMD_STOP_FACE_DETECTION is called!", __FUNCTION__);
            if ( m_flagStartFaceDetection == true
                && stopFaceDetection() == false) {
                CLOGE("ERR(%s):stopFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        }
        break;
#ifdef BURST_CAPTURE
    case 1571: /* HAL_CMD_RUN_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_RUN_BURST_TAKE is called!", __FUNCTION__);

        if( m_burstInitFirst ) {
            m_burstRealloc = true;
            m_burstInitFirst = false;
        }
        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_BURST);
        m_exynosCameraParameters->setSeriesShotSaveLocation(arg1);
        m_takePictureCounter.setCount(0);

        m_burstCaptureCallbackCount = 0;

        {
            // Check jpeg save path
            char default_path[20];
            int SeriesSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

            memset(default_path, 0, sizeof(default_path));

            if (SeriesSaveLocation == BURST_SAVE_PHONE)
                strncpy(default_path, BURST_SAVE_PATH_PHONE, sizeof(default_path)-1);
            else if (SeriesSaveLocation == BURST_SAVE_SDCARD)
                strncpy(default_path, BURST_SAVE_PATH_EXT, sizeof(default_path)-1);

            m_CheckBurstJpegSavingPath(default_path);
        }

#ifdef USE_DVFS_LOCK
        m_exynosCameraParameters->setDvfsLock(true);
#endif
        break;
    case 1572: /* HAL_CMD_STOP_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_STOP_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);

        if (m_exynosCameraParameters->getSeriesShotCount() == MAX_SERIES_SHOT_COUNT) {
             m_reprocessingCounter.clearCount();
             m_pictureCounter.clearCount();
             m_jpegCounter.clearCount();
        }

        m_stopBurstShot = true;

        m_clearJpegCallbackThread(false);

        m_exynosCameraParameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

#ifdef USE_DVFS_LOCK
        if (m_exynosCameraParameters->getDvfsLock() == true)
            m_exynosCameraParameters->setDvfsLock(false);
#endif
        break;
    case 1573: /* HAL_CMD_DELETE_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_DELETE_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
#endif
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        CLOGD("DEBUG(%s):CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            if( m_flagLLSStart != true) {
                m_flagLLSStart = true;
            }
        } else {
            m_flagLLSStart = false;
        }
        break;
    case 1801: /* HAL_ENABLE_LIGHT_CONDITION*/
        CLOGD("DEBUG(%s):HAL_ENABLE_LIGHT_CONDITION is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagLightCondition = true;
        } else {
            m_flagLightCondition = false;
        }
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        CLOGD("DEBUG(%s):CAMERA_CMD_SET_FLIP is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setFlipHorizontal(arg1);
        break;
    /* 1521: CAMERA_CMD_DEVICE_ORIENTATION */
    case 1521:
        CLOGD("DEBUG(%s):CAMERA_CMD_DEVICE_ORIENTATION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setDeviceOrientation(arg1);
        uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
        if (uctlMgr != NULL)
            uctlMgr->setDeviceRotation(m_exynosCameraParameters->getFdOrientation());
        break;
    /*1641: CAMERA_CMD_ADVANCED_MACRO_FOCUS*/
    case 1641:
        CLOGD("DEBUG(%s):CAMERA_CMD_ADVANCED_MACRO_FOCUS is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER);
        break;
    /*1642: CAMERA_CMD_FOCUS_LOCATION*/
    case 1642:
        CLOGD("DEBUG(%s):CAMERA_CMD_FOCUS_LOCATION is called!%d", __FUNCTION__, arg1);
        m_exynosCameraParameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER_UP);
        break;
    /*1661: CAMERA_CMD_START_ZOOM */
    case 1661:
        CLOGD("DEBUG(%s):CAMERA_CMD_START_ZOOM is called!", __FUNCTION__);
        m_exynosCameraParameters->setZoomActiveOn(true);
        m_exynosCameraParameters->setFocusModeLock(true);
        break;
    /*1662: CAMERA_CMD_STOP_ZOOM */
    case 1662:
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_ZOOM is called!", __FUNCTION__);
        m_exynosCameraParameters->setZoomActiveOn(false);
        m_exynosCameraParameters->setFocusModeLock(false);
        break;
    default:
        CLOGV("DEBUG(%s):unexpectect command(%d)", __FUNCTION__, command);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    *newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = m_previewFrameFactory->createNewFrame();

        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        bool flagRequested = false;

        if (isOwnScc(getCameraId()) == true)
            flagRequested = (*newFrame)->getRequest(PIPE_SCC);
        else
            flagRequested = (*newFrame)->getRequest(PIPE_ISPC);

        if (flagRequested == true) {
            m_dynamicSccCount++;
            CLOGV("DEBUG(%s[%d]):dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, (*newFrame)->getFrameCount());
        }

        m_processList.push_back(*newFrame);
    }

    return ret;
}

status_t ExynosCamera::generateFrameSccScp(uint32_t pipeId, uint32_t *frameCount, ExynosCameraFrame **frame)
{
    int  ret = 0;
    int  regenerateFrame = 0;
    int  count = *frameCount;
    ExynosCameraFrame *newframe = NULL;

    do {
        *frame = NULL;
        ret = generateFrame(count, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail, pipeID: %d", __FUNCTION__, __LINE__, pipeId);
            return ret;
        }

        newframe = *frame;
        if (((PIPE_SCP == pipeId) && newframe->getScpDrop()) ||
            ((m_cameraId == CAMERA_ID_FRONT) && (PIPE_SCC == pipeId) && (newframe->getSccDrop() == true)) ||
            ((m_cameraId == CAMERA_ID_FRONT) && (PIPE_ISPC == pipeId) && (newframe->getIspcDrop() == true))) {
            count++;

            ret = newframe->setEntityState(pipeId, ENTITY_STATE_FRAME_SKIP);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_FRAME_SKIP, ret);
                *frameCount = count;
                return ret;
            }

            /* let m_mainThreadFunc handle the processlist cleaning part */
            m_pipeFrameDoneQ->pushProcessQ(&newframe);

            regenerateFrame = 1;
            continue;
        }
        regenerateFrame = 0;
    } while (regenerateFrame);

    *frameCount = count;
    return NO_ERROR;
}

status_t ExynosCamera::m_setupEntity(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *srcBuf,
        ExynosCameraBuffer *dstBuf)
{
    int ret = 0;
    entity_buffer_state_t entityBufferState;

    /* set SRC buffer */
    ret = newFrame->getSrcBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setSrcBuffer(pipeId, newFrame, srcBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    /* set DST buffer */
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setSrcBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer srcBuf;

    if (buffer == NULL) {
        buffer = &srcBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
            bufferMgr->dump();
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setDstBuffer(
        uint32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer *buffer)
{
    int ret = 0;
    int bufIndex = -1;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBuffer dstBuf;

    if (buffer == NULL) {
        buffer = &dstBuf;

        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("ERR(%s[%d]):buffer manager is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            ExynosCameraFrameEntity *curEntity = newFrame->searchEntityByPipeId(pipeId);
            if (curEntity != NULL) {
                if (curEntity->getBufType() == ENTITY_BUFFER_DELIVERY) {
                    CLOGV("DEBUG(%s[%d]): pipe(%d) buffer is empty for delivery", __FUNCTION__, __LINE__, pipeId);
                    buffer->index = -1;
                } else {
                    CLOGE("ERR(%s[%d]):getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId, newFrame->getFrameCount(), ret);
                    return ret;
                }
            } else {
                CLOGE("ERR(%s[%d]):curEntity is NULL, pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
                return ret;
            }
        }
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::generateFrameReprocessing(ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    struct ExynosCameraBuffer tempBuffer;
    int bufIndex = -1;

     /* 1. Make Frame */
    *newFrame = m_reprocessingFrameFactory->createNewFrame();
    if (*newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_fastenAeStable(void)
{
    int ret = 0;
    ExynosCameraBuffer fastenAeBuffer[NUM_FASTAESTABLE_BUFFER];
    ExynosCameraBufferManager *skipBufferMgr = NULL;
    m_createInternalBufferManager(&skipBufferMgr, "SKIP_BUF");

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    planeSize[0] = 32 * 64 * 2;
    int planeCount  = 2;
    int bufferCount = NUM_FASTAESTABLE_BUFFER;

    if (skipBufferMgr == NULL) {
        CLOGE("ERR(%s[%d]):createBufferManager failed", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBuffer fail", __FUNCTION__, __LINE__);
            goto done;
        }
    }

    ret = m_previewFrameFactory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        ret = INVALID_OPERATION;
        // doing some error handling
    }

done:
    skipBufferMgr->deinit();
    delete skipBufferMgr;
    skipBufferMgr = NULL;

func_exit:
    return ret;
}

status_t ExynosCamera::m_startPreviewInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    uint32_t minBayerFrameNum = 0;
    uint32_t min3AAFrameNum = 0;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    enum pipeline pipe;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    if (m_exynosCameraParameters->isFlite3aaOtf() == true)
        minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;
    else
        minBayerFrameNum = m_exynosconfig->current->bufInfo.num_bayer_buffers
                    - m_exynosconfig->current->bufInfo.reprocessing_bayer_hold_count;

    /*
     * with MCPipe, we need to putBuffer 3 buf.
     */
    /*
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON)
        min3AAFrameNum = minBayerFrameNum;
    else
        min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    */

    min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *disBufferManager[MAX_NODE];

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
    }

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    switch (reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
#ifdef DEBUG_RAWDUMP
            m_previewFrameFactory->setRequestFLITE(true);
#else
            m_previewFrameFactory->setRequestFLITE(false);
#endif
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(true);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(true);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        default :
            CLOGE("ERR(%s[%d]): Unknown dynamic bayer mode", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequest3AC(false);
            break;
    }

    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        m_previewFrameFactory->setRequestISPP(true);
        m_previewFrameFactory->setRequestDIS(true);

        if (m_exynosCameraParameters->is3aaIspOtf() == true) {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPP)] = m_hwDisBufferMgr;

            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_DIS)] = m_hwDisBufferMgr;
            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = m_scpBufferMgr;
        } else {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPP)] = m_hwDisBufferMgr;

            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_DIS)] = m_hwDisBufferMgr;
            disBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = m_scpBufferMgr;
        }
     } else {
        m_previewFrameFactory->setRequestISPP(false);
        m_previewFrameFactory->setRequestDIS(false);

        if (m_exynosCameraParameters->is3aaIspOtf() == true) {
            if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
                m_previewFrameFactory->setRequestFLITE(true);
                m_previewFrameFactory->setRequestISPC(true);

                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_bayerBufferMgr;
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPC)] = m_sccBufferMgr;
            } else {
                if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
                    if (m_exynosCameraParameters->getRecordingHint() == true)
                        m_previewFrameFactory->setRequest3AC(false);
                    else
                        m_previewFrameFactory->setRequest3AC(true);
                }

                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
                if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                    taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_sccBufferMgr;
                else
                    taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
                taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = m_scpBufferMgr;
            }
        } else {
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
            taaBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
            ispBufferManager[m_previewFrameFactory->getNodeType(PIPE_SCP)] = m_scpBufferMgr;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (taaBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_3AA);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (ispBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_ISP);
                return ret;
            }
            break;
        }
    }
    if (m_exynosCameraParameters->getHWVdisMode()) {
        for (int i = 0; i < MAX_NODE; i++) {
            /* If even one buffer slot is valid. call setBufferManagerToPipe() */
            if (disBufferManager[i] != NULL) {
                ret = m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, PIPE_DIS);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, %d) failed",
                        __FUNCTION__, __LINE__, PIPE_DIS);
                    return ret;
                }
                break;
            }
        }
    }

    ret = m_previewFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    m_printExynosCameraInfo(__FUNCTION__);

    for (uint32_t i = 0; i < minBayerFrameNum; i++) {
        ret = generateFrame(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_fliteFrameCount++;

        if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
#ifndef DEBUG_RAWDUMP
            if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON)
#endif
            {
                m_setupEntity(m_getBayerPipeId(), newFrame);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, m_getBayerPipeId());
                m_previewFrameFactory->pushFrameToPipe(&newFrame, m_getBayerPipeId());
            }

            if (i < min3AAFrameNum) {
                m_setupEntity(PIPE_3AA, newFrame);

                if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                    m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA);

                    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_DIS);
                    }
                } else {
                    m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_3AA);

                    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_DIS);
                        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                    } else {
                        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                    }
                }

                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA);
                m_3aa_ispFrameCount++;
            }
        } else {
            m_setupEntity(m_getBayerPipeId(), newFrame);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, m_getBayerPipeId());
            m_previewFrameFactory->pushFrameToPipe(&newFrame, m_getBayerPipeId());

            if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
            } else {
                m_setupEntity(PIPE_3AA, newFrame);
                m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA);
                m_3aa_ispFrameCount++;
            }

        }
#if 0
        /* SCC */
        if(m_exynosCameraParameters->isSccCapture() == true) {
            m_sccFrameCount++;

            if (isOwnScc(getCameraId()) == true) {
                pipe = PIPE_SCC;
            } else {
                pipe = PIPE_ISPC;
            }

            if(newFrame->getRequest(pipe)) {
                m_setupEntity(pipe, newFrame);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);
            }
        }
#endif
        /* SCP */
/* Comment out, because it included ISP */
/*
        m_scpFrameCount++;

        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
*/
    }

/* Comment out, because it included ISP */
/*
    for (uint32_t i = minFrameNum; i < INIT_SCP_BUFFERS; i++) {
        ret = generateFrame(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_scpFrameCount++;

        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
    }
*/

    /* prepare pipes */
    ret = m_previewFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):preparePipe fail", __FUNCTION__);
        return ret;
    }

#ifndef START_PICTURE_THREAD
    if (m_exynosCameraParameters->isReprocessing() == true) {
        m_startPictureInternal();
    }
#endif

    /* stream on pipes */
    ret = m_previewFrameFactory->startPipes();
    if (ret < 0) {
        m_previewFrameFactory->stopPipes();
        CLOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = m_previewFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    m_previewEnabled = true;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    if (m_exynosCameraParameters->getFocusModeSetting() == true) {
        CLOGD("set Focus Mode(%s[%d])", __FUNCTION__, __LINE__);
        int focusmode = m_exynosCameraParameters->getFocusMode();
        m_exynosCameraActivityControl->setAutoFocusMode(focusmode);
        m_exynosCameraParameters->setFocusModeSetting(false);
    }

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPreviewInternal(void)
{
    int ret = 0;

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);
    if (m_previewFrameFactory != NULL) {
        ret = m_previewFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):stopPipe fail", __FUNCTION__);
            return ret;
        }
    }

    CLOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_pipeFrameDoneQ->release();

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;

    m_previewEnabled = false;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_startPictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPictureW, hwPictureH;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
    }

    if (m_zslPictureEnabled == true) {
        CLOGD("DEBUG(%s[%d]): zsl picture is already initialized", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if( m_exynosCameraParameters->getHighSpeedRecording() ) {
            m_exynosCameraParameters->getHwSensorSize(&hwPictureW, &hwPictureH);
            CLOGI("(%s):HW Picture(HighSpeed) width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        } else {
            m_exynosCameraParameters->getMaxPictureSize(&hwPictureW, &hwPictureH);
            CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        }

        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN);
            planeSize[1] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) / 2;
            planeCount  = 3;
        } else if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            planeCount  = 2;
        } else {
        planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
        planeCount  = 2;
        }
        minBufferCount = 1;
        maxBufferCount = NUM_PICTURE_BUFFERS;

        if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            /* SCC Reprocessing Buffer realloc for high resolution callback */
            minBufferCount = 2;
        }

        ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_sccReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }

        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
            ret = m_setReprocessingBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setReprocessing Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }
    }

    if (m_reprocessingFrameFactory->isCreated() == false) {
        ret = m_reprocessingFrameFactory->create();
        if (ret < 0) {
            CLOGE("ERR(%s):m_reprocessingFrameFactory->create() failed", __FUNCTION__);
            return ret;
        }

        m_pictureFrameFactory = m_reprocessingFrameFactory;
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
    }

    /* If we want set buffer namanger from m_getBufferManager, use this */
#if 0
    ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)], SRC_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA_REPROCESSING, ret);
        return ret;
    }

    ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)], DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_3AP_REPROCESSING, ret);
        return ret;
    }
#else
    if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false) {
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    } else {
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
        taaBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_sccReprocessingBufferMgr;

        taaBufferManager[OUTPUT_NODE] = m_bayerBufferMgr;
        taaBufferManager[CAPTURE_NODE] = m_sccReprocessingBufferMgr;
    }
#endif

    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA_REPROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->setBufferManagerToPipe() failed", __FUNCTION__);
        return ret;
    }

    ispBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
    ispBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_sccReprocessingBufferMgr;

    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP_REPROCESSING);
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->setBufferManagerToPipe() failed", __FUNCTION__);
        return ret;
    }

    ret = m_reprocessingFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory->initPipes() failed", __FUNCTION__);
        return ret;
    }

    ret = m_reprocessingFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = m_reprocessingFrameFactory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):m_reprocessingFrameFactory startPipe fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = true;

    /*
     * Make remained frameFactory here.
     * in case of reprocessing capture, make here.
     */
    m_framefactoryThread->run();

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPictureInternal(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    m_prePictureThread->join();
    m_pictureThread->join();
    m_postPictureThread->join();

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        m_highResolutionCallbackThread->stop();
        if (m_highResolutionCallbackQ != NULL)
            m_highResolutionCallbackQ->sendCmd(WAKE_UP);
        m_highResolutionCallbackThread->requestExitAndWait();
    }

    m_jpegCallbackThread->join();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveThread[threadNum]->join();
    }

    if (m_zslPictureEnabled == true) {
        ret = m_reprocessingFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("ERR(%s):m_reprocessingFrameFactory0>stopPipe() fail", __FUNCTION__);
        }
    }

    /* Clear frames & buffers which remain in capture processingQ */
    m_clearFrameQ(dstSccReprocessingQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(m_postPictureQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(dstJpegReprocessingQ, PIPE_JPEG, SRC_BUFFER_DIRECTION);

    dstIspReprocessingQ->release();
    dstGscReprocessingQ->release();
    dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum]->release();
    }

    if (m_highResolutionCallbackQ->getSizeOfProcessQ() != 0){
        CLOGD("DEBUG(%s[%d]):m_highResolutionCallbackQ->getSizeOfProcessQ(%d). release the highResolutionCallbackQ.",
            __FUNCTION__, __LINE__, m_highResolutionCallbackQ->getSizeOfProcessQ());
        m_highResolutionCallbackQ->release();
    }

    CLOGD("DEBUG(%s[%d]):clear postProcess(Picture) Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_postProcessList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_startRecordingInternal(void)
{
    int ret = 0;

    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int videoW = 0, videoH = 0;
    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_SILENT;
    int heapFd = 0;

    m_exynosCameraParameters->getVideoSize(&videoW, &videoH);
    CLOGD("DEBUG(%s[%d]):videoSize = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    m_doCscRecording = true;
    if (m_exynosCameraParameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("INFO(%s[%d]):do Recording CSC !!! m_recordingBufferCount(%d)", __FUNCTION__, __LINE__, m_recordingBufferCount);
    } else {
        m_doCscRecording = false;
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        CLOGI("INFO(%s[%d]):skip Recording CSC !!! m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_exynosconfig->current->bufInfo.num_recording_buffers, m_recordingBufferCount);
    }

    for (int32_t i = 0; i < m_recordingBufferCount; i++) {
        m_recordingTimeStamp[i] = 0L;
        m_recordingBufAvailable[i] = true;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct addrs), m_recordingBufferCount, &heapFd);
    if (!m_recordingCallbackHeap || m_recordingCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, sizeof(struct addrs));
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_doCscRecording == true) {
        /* alloc recording Image buffer */
        planeSize[0] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(videoW, CAMERA_MAGIC_ALIGN) * ROUND_UP(videoH / 2, CAMERA_MAGIC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeCount   = 2;
        if( m_exynosCameraParameters->getHighSpeedRecording() == true)
            minBufferCount = m_recordingBufferCount;
        else
            minBufferCount = 1;

        maxBufferCount = m_recordingBufferCount;

        ret = m_allocBuffers(m_recordingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_recordingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        }
    }

    if (m_doCscRecording == true) {
        int recPipeId = PIPE_GSC_VIDEO;

        m_previewFrameFactory->startThread(recPipeId);

        if (m_recordingQ->getSizeOfProcessQ() > 0) {
            CLOGE("ERR(%s[%d]):m_startRecordingInternal recordingQ(%d)", __FUNCTION__, __LINE__, m_recordingQ->getSizeOfProcessQ());
           m_clearList(m_recordingQ);
        }

        m_recordingThread->run();
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_stopRecordingInternal(void)
{
    int ret = 0;
    if (m_doCscRecording == true) {
        int recPipeId = PIPE_GSC_VIDEO;

        m_previewFrameFactory->stopPipe(recPipeId);
        m_recordingQ->sendCmd(WAKE_UP);

        m_recordingThread->requestExitAndWait();
        m_recordingQ->release();
        m_recordingListLock.lock();
        ret = m_clearList(&m_recordingProcessList);
        if (ret < 0) {
            CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
            return ret;
        }
        m_recordingListLock.unlock();

        m_recordingBufferMgr->deinit();
    } else {
        CLOGI("INFO(%s[%d]):reset m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_recordingBufferCount, m_exynosconfig->current->bufInfo.num_recording_buffers);
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_restartPreviewInternal(void)
{
    CLOGI("INFO(%s[%d]): Internal restart preview", __FUNCTION__, __LINE__);
    int ret = 0;
    int err = 0;

    m_flagThreadStop = true;

    /* release about frameFactory */
    m_framefactoryThread->requestExitAndWait();
    m_frameFactoryQ->release();

    m_startPictureInternalThread->join();
    m_startPictureBufferThread->join();

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->setStopFlag();

    m_mainThread->requestExitAndWait();

    ret = m_stopPictureInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

    m_previewThread->stop();
    if(m_previewQ != NULL)
        m_previewQ->sendCmd(WAKE_UP);
    m_previewThread->requestExitAndWait();

    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
        m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();

        if (m_mainSetupQThread[INDEX(PIPE_3AC)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_3AC)]->stop();
            m_mainSetupQ[INDEX(PIPE_3AC)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_3AC)]->requestExitAndWait();
        }

        m_mainSetupQThread[INDEX(PIPE_3AA)]->stop();
        m_mainSetupQ[INDEX(PIPE_3AA)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->requestExitAndWait();

       if (m_mainSetupQThread[INDEX(PIPE_ISP)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_ISP)]->stop();
            m_mainSetupQ[INDEX(PIPE_ISP)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_ISP)]->requestExitAndWait();
        }

        /* Comment out, because it included ISP */
        /*
        m_mainSetupQThread[INDEX(PIPE_SCP)]->stop();
        m_mainSetupQ[INDEX(PIPE_SCP)]->sendCmd(WAKE_UP);
        m_mainSetupQThread[INDEX(PIPE_SCP)]->requestExitAndWait();
        */

        m_clearList(m_mainSetupQ[INDEX(PIPE_FLITE)]);
        m_clearList(m_mainSetupQ[INDEX(PIPE_3AA)]);
        m_clearList(m_mainSetupQ[INDEX(PIPE_ISP)]);
        /* Comment out, because it included ISP */
        /* m_clearList(m_mainSetupQ[INDEX(PIPE_SCP)]); */

        m_mainSetupQ[INDEX(PIPE_FLITE)]->release();
        m_mainSetupQ[INDEX(PIPE_3AA)]->release();
        m_mainSetupQ[INDEX(PIPE_ISP)]->release();
        /* Comment out, because it included ISP */
        /* m_mainSetupQ[INDEX(PIPE_SCP)]->release(); */
    }

    ret = m_stopPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* skip to free and reallocate buffers */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->resetBuffers();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
        m_sccBufferMgr->setBufferCount(0);
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->resetBuffers();
    }

    /* skip to free and reallocate buffers */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->resetBuffers();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }

    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

    if( m_exynosCameraParameters->getHighSpeedRecording() && m_exynosCameraParameters->getReallocBuffer() ) {
        CLOGD("DEBUG(%s): realloc buffer all buffer deinit ", __FUNCTION__);
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
        if (m_3aaBufferMgr != NULL) {
            m_3aaBufferMgr->deinit();
        }
        if (m_ispBufferMgr != NULL) {
            m_ispBufferMgr->deinit();
        }

        if (m_sccBufferMgr != NULL) {
            m_sccBufferMgr->deinit();
        }
/*
        if (m_highResolutionCallbackBufferMgr != NULL) {
            m_highResolutionCallbackBufferMgr->deinit();
        }
*/
        if (m_gscBufferMgr != NULL) {
            m_gscBufferMgr->deinit();
        }
        if (m_jpegBufferMgr != NULL) {
            m_jpegBufferMgr->deinit();
        }
        if (m_recordingBufferMgr != NULL) {
            m_recordingBufferMgr->deinit();
        }
    }

    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        if (m_hwDisBufferMgr != NULL) {
            m_hwDisBufferMgr->deinit();
        }
    }

    m_flagThreadStop = false;

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
    m_setBuffersThread->run(PRIORITY_DEFAULT);
    m_setBuffersThread->join();

#ifdef START_PICTURE_THREAD
    m_startPictureInternalThread->join();
#endif
    m_startPictureBufferThread->join();

    if (m_exynosCameraParameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
        m_startPictureInternalThread->run(PRIORITY_DEFAULT);
#endif
    } else {
        m_pictureFrameFactory = m_previewFrameFactory;
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);

        /*
         * Make remained frameFactory here.
         * in case of SCC capture, make here.
         */
        m_framefactoryThread->run();
    }

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_startPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* setup frame thread */
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
    } else {
#ifdef DEBUG_RAWDUMP
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
#endif
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
    }

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    m_previewThread->run(PRIORITY_DISPLAY);

    m_mainThread->run(PRIORITY_DEFAULT);
    m_startPictureInternalThread->join();

    return err;
}

bool ExynosCamera::m_mainThreadQSetup3AA()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId_3AA    = PIPE_3AA;
    int  pipeId_3AC    = PIPE_3AC;
    int  pipeId_ISP    = PIPE_ISP;
    int  pipeId_DIS    = PIPE_DIS;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId_3AA)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }

    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = generateFrame(m_3aa_ispFrameCount, &newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (newframe == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_setupEntity(pipeId_3AA, newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
    }

    m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId_3AA);

    if (m_exynosCameraParameters->is3aaIspOtf() == true) {
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_3AA);

        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_DIS);
    } else {
        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId_3AA);

        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId_ISP);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_DIS);
        } else {
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId_ISP);
        }
    }

    m_3aa_ispFrameCount++;

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_mainThreadQSetup3AA_ISP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_3AA_ISP;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    while (m_3aaBufferMgr->getNumOfAvailableBuffer() > 0 &&
            m_ispBufferMgr->getNumOfAvailableBuffer() > 0) {
        ret = generateFrame(m_3aa_ispFrameCount, &newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        if (newframe == NULL) {
            CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        ret = m_setupEntity(pipeId, newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }

        m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

        m_3aa_ispFrameCount++;

    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupISP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = false;
    int  pipeId = PIPE_ISP;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() > 0)
        loop = true;

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupFLITE()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_FLITE;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }


    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
        ret = generateFrame(m_fliteFrameCount, &newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        ret = m_setupEntity(pipeId, newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        ret = newframe->getDstBuffer(pipeId, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

        m_fliteFrameCount++;
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadQSetup3AC()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_3AC;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }
    if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
        do {
            ret = generateFrame(m_fliteFrameCount, &newframe);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                goto func_exit;
            }

            ret = m_setupEntity(pipeId, newframe);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            }
            ret = newframe->getDstBuffer(pipeId, &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

            m_fliteFrameCount++;
        //} while (m_bayerBufferMgr->getAllocatedBufferCount() - PIPE_3AC_PREPARE_COUNT
        //         < m_bayerBufferMgr->getNumOfAvailableBuffer());
        } while (0 < m_bayerBufferMgr->getNumOfAvailableBuffer());
    }

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadQSetupSCP()
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = PIPE_SCP;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;

    camera2_node_group node_group_info_isp;
    ExynosCameraBuffer resultBuffer;
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    int frameGen = 1;

    CLOGV("INFO(%s[%d]):wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = generateFrameSccScp(pipeId, &m_scpFrameCount, &newframe);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if( frame->getFrameState() == FRAME_STATE_SKIPPED ) {
        frame->getDstBuffer(pipeId, &resultBuffer);
        m_setupEntity(pipeId, newframe, NULL, &resultBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        newframe->getDstBuffer(pipeId, &resultBuffer);
    } else {
        ret = m_setupEntity(pipeId, newframe);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }
        newframe->getDstBuffer(pipeId, &resultBuffer);
    }

    /*check preview drop...*/
    newframe->getDstBuffer(pipeId, &resultBuffer);
    if (resultBuffer.index < 0) {
        newframe->setRequest(pipeId, false);
        newframe->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
        node_group_info_isp.capture[PERFRAME_BACK_SCP_POS].request = 0;
        newframe->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
        m_previewFrameFactory->dump();

        if (m_getRecordingEnabled() == true
            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
            && m_exynosCameraParameters->getHighSpeedRecording() == false) {
            CLOGW("WARN(%s[%d]):Recording preview drop. Failed to get preview buffer. FrameSkipCount(%d)",
                    __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_RECORDING);
            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_RECORDING);
        } else {
           CLOGW("WARN(%s[%d]):Preview drop. Failed to get preview buffer. FrameSkipCount (%d)",
                   __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_PREVIEW);
            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_PREVIEW);
        }
    }

    m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
    m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

    m_scpFrameCount++;

func_exit:
    if( frame != NULL ) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    /*
    if (m_mainSetupQ[INDEX(pipeId)]->getSizeOfProcessQ() <= 0)
        loop = false;
    */

    return loop;
}

bool ExynosCamera::m_mainThreadFunc(void)
{
    int ret = 0;
    int index = 0;
    ExynosCameraFrame *newFrame = NULL;
    uint32_t frameCnt = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        return false;
    }

    ret = m_pipeFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    frameCnt = newFrame->getFrameCount();

/* HACK Reset Preview Flag*/
#if 0
    if (m_exynosCameraParameters->getRestartPreview() == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);

        return true;
    }
#endif

    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        ret = m_handlePreviewFrame(newFrame);
    } else {
        if (m_exynosCameraParameters->getDualMode())
            ret = m_handlePreviewFrameFrontDual(newFrame);
        else
            ret = m_handlePreviewFrameFront(newFrame);
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):handle preview frame fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* Below code block is moved to m_handlePreviewFrame() and m_handlePreviewFrameFront() functions
     * because we want to delete the frame as soon as the setFrameState(FRAME_STATE_COMPLETE) is called.
     * Otherwise, some other thread might be executed between "setFrameState(FRAME_STATE_COMPLETE)" and
     * "delete frame" statements and might delete the same frame. This would cause the second "delete frame"
     * (trying to delete the same frame) to behave abnormally since that frame is already deleted.
     */
#if 0
    /* Don't use this lock */
    m_frameFliteDeleteBetweenPreviewReprocessing.lock();
    if (newFrame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (newFrame->getFrameLockState() == false)
        {
            delete newFrame;
            newFrame = NULL;
        }
    }
    m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
#endif


    /*
     * HACK
     * By using MCpipe. we don't use seperated pipe_scc.
     * so, we will not meet inputFrameQ's fail issue.
     */
    /* m_checkFpsAndUpdatePipeWaitTime(); */

    m_autoFocusContinousQ.pushProcessQ(&frameCnt);

    return true;
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;

    ExynosCameraBuffer buffer;
#ifdef SUPPORT_SW_VDIS
    ExynosCameraBuffer vs_InputBuffer;
#endif /*SUPPORT_SW_VDIS*/
    ExynosCameraBuffer t3acBuffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    int ispDstBufferIndex = -1;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    /* TODO: remove hard coding */
    switch(INDEX(entity->getPipeId())) {
    case PIPE_3AA_ISP:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }
        ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }

        CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            break;
        }

        frame->setMetaDataEnable(true);

        /* Face detection */
        if(!m_exynosCameraParameters->getHighSpeedRecording()) {
            skipCount = m_exynosCameraParameters->getFrameSkipCount();
            if( m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
                skipCount <= 0 && m_flagStartFaceDetection == true) {
                struct camera2_shot_ext shot;
                frame->getDynamicMeta(&shot);
                fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
                fdFrame->storeDynamicMeta(&shot);
                m_facedetectQ->pushProcessQ(&fdFrame);
            }
        }

        /* ISP capture mode q/dq for vdis */
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
#if 0
            /* case 1 : directly push on isp, tpu. */
            ret = m_pushFrameToPipeIspDIS();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_pushFrameToPipeIspDIS() fail", __FUNCTION__, __LINE__);
            }
#else
            /* case 2 : indirectly push on isp, tpu. */
            newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
            m_mainSetupQ[INDEX(PIPE_ISP)]->pushProcessQ(&newFrame);
#endif
        }

        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
        break;
    case PIPE_3AC:
    case PIPE_FLITE:
        m_debugFpsCheck(entity->getPipeId());

        if (m_exynosCameraParameters->getHighSpeedRecording()) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                break;
            }
        } else {
            if (frame->getSccDrop() == true || frame->getIspcDrop() == true) {
                CLOGE("ERR(%s[%d]):getSccDrop() == %d || getIspcDrop()== %d. so drop this frame(frameCount : %d)",
                    __FUNCTION__, __LINE__, frame->getSccDrop(), frame->getIspcDrop(), frame->getFrameCount());

                ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                    break;
                }
            } else {
                ret = m_captureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                    return ret;
                }
#ifdef DEBUG_RAWDUMP
                newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
#endif
            }
        }

        /* TODO: Dynamic bayer capture, currently support only single shot */
        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC
            || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
            m_previewFrameFactory->stopThread(entity->getPipeId());

        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
            || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
            newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
            m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
        }
        break;
    case PIPE_ISP:
        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_putBuffers(m_ispBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                break;
            }
        }

        /* Face detection */
        if (!m_exynosCameraParameters->getHighSpeedRecording()
            && frame->getFrameState() != FRAME_STATE_SKIPPED) {
            skipCount = m_exynosCameraParameters->getFrameSkipCount();
            if( m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
                skipCount <= 0 && m_flagStartFaceDetection == true) {
                ret = m_doFdCallbackFunc(frame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_doFdCallbackFunc fail, ret(%d)",
                        __FUNCTION__, __LINE__, ret);
                    return ret;
                }
            }
        }

        /* ISP capture mode q/dq for vdis */
        if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
            break;
        }
    case PIPE_3AA:
    case PIPE_DIS:
       /* The following switch allows both  PIPE_3AA and PIPE_ISP to
        * fall through to PIPE_SCP
        */

       switch(INDEX(entity->getPipeId())) {
            case PIPE_3AA:
                m_debugFpsCheck(entity->getPipeId());

                /*
                if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
                    m_previewFrameFactory->dump();
                */

                ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_3aaBufferMgr, buffer.index);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                    }
                }

                frame->setMetaDataEnable(true);

                newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
                m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);

                /* 3AC buffer handling */
                t3acBuffer.index = -1;

                if (frame->getRequest(PIPE_3AC) == true) {
                    ret = frame->getDstBuffer(entity->getPipeId(), &t3acBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    }
                }

                if (0 <= t3acBuffer.index) {
                    if (frame->getRequest(PIPE_3AC) == true) {
                        if (m_exynosCameraParameters->getHighSpeedRecording() == true) {
                            if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                                ret = m_putBuffers(m_sccBufferMgr, t3acBuffer.index);
                            else
                                ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                            if (ret < 0) {
                                CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                                break;
                            }
                        } else {
                            entity_buffer_state_t bufferstate = ENTITY_BUFFER_STATE_NOREQ;
                            ret = frame->getDstBufferState(entity->getPipeId(), &bufferstate, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            if (ret == NO_ERROR && bufferstate != ENTITY_BUFFER_STATE_ERROR) {
                                if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                                    ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                                else
                                    ret = m_captureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
                                if (ret < 0) {
                                    CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                                    return ret;
                                }
                            } else {
                                if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                                    ret = m_putBuffers(m_sccBufferMgr, t3acBuffer.index);
                                else
                                    ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                                if (ret < 0) {
                                    CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                                    break;
                                }
                            }
                        }
                    } else {
                        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON) {
                            CLOGW("WARN(%s[%d]):frame->getRequest(PIPE_3AC) == false. so, just m_putBuffers(t3acBuffer.index(%d)..., pipeId(%d), ret(%d)",
                                __FUNCTION__, __LINE__, t3acBuffer.index, entity->getPipeId(), ret);
                        }

                        if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                            ret = m_putBuffers(m_sccBufferMgr, t3acBuffer.index);
                        else
                            ret = m_putBuffers(m_bayerBufferMgr, t3acBuffer.index);
                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, t3acBuffer.index);
                            break;
                        }
                    }
                }

                /* Face detection for using 3AA-ISP OTF mode. */
                if (m_exynosCameraParameters->is3aaIspOtf() == true) {
                    skipCount = m_exynosCameraParameters->getFrameSkipCount();
                    if (skipCount <= 0) {
                        /* Face detection */
                        struct camera2_shot_ext shot;
                        frame->getDynamicMeta(&shot);
                        fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
                        fdFrame->storeDynamicMeta(&shot);
                        m_facedetectQ->pushProcessQ(&fdFrame);
                    }
                }

                CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                        __FUNCTION__, __LINE__,
                        getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                        frame->getFrameCount());

                break;
            case PIPE_DIS:
                m_debugFpsCheck(pipeID);
                if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
                    ret = frame->getSrcBuffer(PIPE_DIS, &buffer);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, PIPE_ISP, ret);
                        return ret;
                    }

                    if (buffer.index >= 0) {
                        ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(m_hwDisBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                        }
                    }
                }

                CLOGV("DEBUG(%s[%d]):DIS done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

                break;
            default:
                    CLOGE("ERR(%s[%d]):Its impossible to be here. ",
                            __FUNCTION__, __LINE__);
                break;
        }

        if ((INDEX(entity->getPipeId())) == PIPE_3AA &&
            m_exynosCameraParameters->is3aaIspOtf() == true &&
            m_exynosCameraParameters->getTpuEnabledMode() == false) {
            /* Fall through to PIPE_SCP */
        } else if ((INDEX(entity->getPipeId())) == PIPE_DIS) {
            /* Fall through to PIPE_SCP */
        } else {
            /* Break out of the outer switch and reach entity_state_complete: */
            break;
        }

    case PIPE_SCP:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            if (frame->getRequest(PIPE_SCP) == true) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
                /* For debug */
                /* m_previewFrameFactory->dump(); */

                /* Comment out, because it included ISP */
                /*
                   newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                   newFrame->setDstBuffer(PIPE_SCP, buffer);
                   newFrame->setFrameState(FRAME_STATE_SKIPPED);

                   m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                 */
            }
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            /* TO DO : skip frame for HDR */
            shot_stream = (struct camera2_stream *)buffer.addr[2];

            if (shot_stream != NULL) {
                getStreamFrameValid(shot_stream, &fvalid);
                getStreamFrameCount(shot_stream, &fcount);
            } else {
                CLOGE("ERR(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
                fvalid = false;
                fcount = 0;
            }

            /* drop preview frame if lcd supported frame rate < scp frame rate */
            frame->getFpsRange(&minFps, &maxFps);
            if (dispFps < maxFps) {
                ratio = (int)((maxFps * 10 / dispFps) / 10);
                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                skipPreview = (m_displayPreviewToggle == 0) ? true : false;
#ifdef DEBUG
                CLOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), m_displayPreviewToggle, maxFps, dispFps, ratio, (int)skipPreview);
#endif
            }

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
            newFrame->setDstBuffer(PIPE_SCP, buffer);

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                        __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);
                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            } else {
                if (m_skipReprocessing == true)
                    m_skipReprocessing = false;
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                if (m_getRecordingEnabled() == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                    } else {
                        if (m_exynosCameraParameters->doCscRecording() == true) {
                            /* get Recording Image buffer */
                            int bufIndex = -2;
                            ExynosCameraBuffer recordingBuffer;
                            ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                            if (ret < 0 || bufIndex < 0) {
                                if ((++m_recordingFrameSkipCount % 100) == 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                    m_recordingBufferMgr->printBufferQState();
                                }
                            } else {
                                if (m_recordingFrameSkipCount != 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                    m_recordingFrameSkipCount = 0;
                                    m_recordingBufferMgr->printBufferQState();
                                }
                                m_recordingTimeStamp[bufIndex] = timeStamp;

                                ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer);
                                if (ret < 0) {
                                    CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                }
                            }
                        } else {
                            m_recordingTimeStamp[buffer.index] = timeStamp;

                            if (m_recordingStartTimeStamp == 0) {
                                m_recordingStartTimeStamp = timeStamp;
                                CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
                            }

                            if ((0L < timeStamp)
                                && (m_lastRecordingTimeStamp < timeStamp)
                                && (m_recordingStartTimeStamp <= timeStamp)) {
                                if (m_getRecordingEnabled() == true
                                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                    CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld, fd[0]=%d",
                                            __FUNCTION__, __LINE__, buffer.index, timeStamp, buffer.fd[0]);
#endif
#ifdef DEBUG
                                    CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                            __FUNCTION__, __LINE__,
                                            m_lastRecordingTimeStamp,
                                            systemTime(SYSTEM_TIME_MONOTONIC),
                                            m_recordingStartTimeStamp);
#endif

                                    if (m_recordingBufAvailable[buffer.index] == false) {
                                        CLOGW("WARN(%s[%d]):recordingFrameIndex(%d) didn't release yet !!! drop the frame !!! "
                                                " timeStamp(%lld) m_recordingBufAvailable(%d)",
                                                __FUNCTION__, __LINE__, buffer.index, timeStamp, (int)m_recordingBufAvailable[buffer.index]);
                                    } else {
                                        struct addrs *recordAddrs = NULL;

                                        recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
                                        recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
                                        recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
                                        recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
                                        recordAddrs[buffer.index].bufIndex    = buffer.index;

                                        m_recordingBufAvailable[buffer.index] = false;
                                        m_lastRecordingTimeStamp = timeStamp;

                                        if (m_getRecordingEnabled() == true
                                            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                                        m_dataCbTimestamp(
                                                timeStamp,
                                                CAMERA_MSG_VIDEO_FRAME,
                                                m_recordingCallbackHeap,
                                                buffer.index,
                                                m_callbackCookie);
                                        }
                                    }
                                }
                            } else {
                                CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
                                    " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld), m_recordingBufAvailable(%d)",
                                    __FUNCTION__, __LINE__, buffer.index, timeStamp,
                                    m_lastRecordingTimeStamp,
                                    systemTime(SYSTEM_TIME_MONOTONIC),
                                    m_recordingStartTimeStamp,
                                    (int)m_recordingBufAvailable[buffer.index]);
                                m_recordingTimeStamp[buffer.index] = 0L;
                            }
                        }
                    }
                }

                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;
                struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;

                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                frame->getDstBuffer(entity->getPipeId(), &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                frame->getMetaData(shot_ext);
                callbackFrame->storeDynamicMeta(shot_ext);
                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);

                if (((m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 2) ||
                    (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 1)) &&
                    (m_previewThread->isRunning() == true)) {
                        if ((m_getRecordingEnabled() == true) && (m_exynosCameraParameters->doCscRecording() == false)) {
                            CLOGW("WARN(%s[%d]):push frame to previewQ. PreviewQ(%d), PreviewBufferCount(%d)",
                                    __FUNCTION__,
                                    __LINE__,
                                     m_previewQ->getSizeOfProcessQ(),
                                     m_exynosCameraParameters->getPreviewBufferCount());
                            m_previewQ->pushProcessQ(&callbackFrame);
                        } else {
                            CLOGW("WARN(%s[%d]):Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                                __FUNCTION__, __LINE__,
                                m_previewQ->getSizeOfProcessQ(),
                                m_exynosCameraParameters->getPreviewBufferCount());
                            newFrame->setFrameState(FRAME_STATE_SKIPPED);

                            callbackFrame->decRef();
                            m_frameMgr->deleteFrame(callbackFrame);
                            callbackFrame = NULL;
                        }
                } else if((m_exynosCameraParameters->getFastFpsMode() == 2) && (m_exynosCameraParameters->getRecordingHint() == 1)) {
                    m_skipCount++;
                    CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                    m_previewQ->pushProcessQ(&callbackFrame);
                } else {
                    if (m_getRecordingEnabled() == true) {
                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                        m_previewQ->pushProcessQ(&callbackFrame);
                    } else {
                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                        m_previewQ->pushProcessQ(&callbackFrame);
                    }
                }
                delete shot_ext;
                shot_ext = NULL;
            }
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            //m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            }

            /* For debug */
            /* m_previewFrameFactory->dump(); */

            /* Comment out, because it included ISP */
            /*
            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
            newFrame->setDstBuffer(PIPE_SCP, buffer);
            newFrame->setFrameState(FRAME_STATE_SKIPPED);

            m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            */
        }
        break;
    default:
        break;
    }

entity_state_complete:

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }


    return NO_ERROR;
}

status_t ExynosCamera::m_handlePreviewFrameFrontDual(ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;

    ExynosCameraBuffer buffer;
    ExynosCameraBuffer t3acBuffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    int ispDstBufferIndex = -1;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    /* TODO: remove hard coding */
    switch(INDEX(entity->getPipeId())) {
    case PIPE_3AA_ISP:
        break;
    case PIPE_3AC:
    case PIPE_FLITE:
        m_debugFpsCheck(entity->getPipeId());

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_3AA);
        m_previewFrameFactory->pushFrameToPipe(&frame, PIPE_3AA);

        newFrame = m_frameMgr->createFrame(m_exynosCameraParameters, 0);
        m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);

        break;
    case PIPE_3AA:
        m_debugFpsCheck(entity->getPipeId());

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false, CAPTURE_NODE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
            return ret;
        }


        if (buffer.index >= 0) {
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        frame->setMetaDataEnable(true);

        CLOGV("DEBUG(%s[%d]):3AA_ISP frameCount(%d) frame.Count(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

    case PIPE_SCP:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
                /* For debug */
                /* m_previewFrameFactory->dump(); */

                /* Comment out, because it included ISP */
                /*
                   newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                   newFrame->setDstBuffer(PIPE_SCP, buffer);
                   newFrame->setFrameState(FRAME_STATE_SKIPPED);

                   m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
                 */
            }
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            /* TO DO : skip frame for HDR */
            shot_stream = (struct camera2_stream *)buffer.addr[2];

            if (shot_stream != NULL) {
                getStreamFrameValid(shot_stream, &fvalid);
                getStreamFrameCount(shot_stream, &fcount);
            } else {
                CLOGE("ERR(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
                fvalid = false;
                fcount = 0;
            }

            /* drop preview frame if lcd supported frame rate < scp frame rate */
            frame->getFpsRange(&minFps, &maxFps);
            if (dispFps < maxFps) {
                ratio = (int)((maxFps * 10 / dispFps) / 10);
                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                skipPreview = (m_displayPreviewToggle == 0) ? true : false;
#ifdef DEBUG
                CLOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                        __FUNCTION__, __LINE__, frame->getFrameCount(), m_displayPreviewToggle, maxFps, dispFps, ratio, (int)skipPreview);
#endif
            }

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
            newFrame->setDstBuffer(PIPE_SCP, buffer);

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                        __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);
                newFrame->setFrameState(FRAME_STATE_SKIPPED);
                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            } else {
                if (m_skipReprocessing == true)
                    m_skipReprocessing = false;
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                if (m_getRecordingEnabled() == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                    } else {
                        if (m_exynosCameraParameters->doCscRecording() == true) {
                            /* get Recording Image buffer */
                            int bufIndex = -2;
                            ExynosCameraBuffer recordingBuffer;
                            ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                            if (ret < 0 || bufIndex < 0) {
                                if ((++m_recordingFrameSkipCount % 100) == 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                    m_recordingBufferMgr->printBufferQState();
                                }
                            } else {
                                if (m_recordingFrameSkipCount != 0) {
                                    CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex, m_recordingQ->getSizeOfProcessQ());
                                    m_recordingFrameSkipCount = 0;
                                    m_recordingBufferMgr->printBufferQState();
                                }
                                m_recordingTimeStamp[bufIndex] = timeStamp;

                                ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer);
                                if (ret < 0) {
                                    CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                }
                            }
                        } else {
                            m_recordingTimeStamp[buffer.index] = timeStamp;

                            if (m_recordingStartTimeStamp == 0) {
                                m_recordingStartTimeStamp = timeStamp;
                                CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                                        __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
                            }

                            if ((0L < timeStamp)
                                && (m_lastRecordingTimeStamp < timeStamp)
                                && (m_recordingStartTimeStamp <= timeStamp)) {
                                if (m_getRecordingEnabled() == true
                                        && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
                                    CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld, fd[0]=%d",
                                            __FUNCTION__, __LINE__, buffer.index, timeStamp, buffer.fd[0]);
#endif
#ifdef DEBUG
                                    CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                                            __FUNCTION__, __LINE__,
                                            m_lastRecordingTimeStamp,
                                            systemTime(SYSTEM_TIME_MONOTONIC),
                                            m_recordingStartTimeStamp);
#endif

                                    if (m_recordingBufAvailable[buffer.index] == false) {
                                        CLOGW("WARN(%s[%d]):recordingFrameIndex(%d) didn't release yet !!! drop the frame !!! "
                                                " timeStamp(%lld) m_recordingBufAvailable(%d)",
                                                __FUNCTION__, __LINE__, buffer.index, timeStamp, (int)m_recordingBufAvailable[buffer.index]);
                                    } else {
                                        struct addrs *recordAddrs = NULL;

                                        recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
                                        recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
                                        recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
                                        recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
                                        recordAddrs[buffer.index].bufIndex    = buffer.index;

                                        m_recordingBufAvailable[buffer.index] = false;
                                        m_lastRecordingTimeStamp = timeStamp;

                                        m_dataCbTimestamp(
                                                timeStamp,
                                                CAMERA_MSG_VIDEO_FRAME,
                                                m_recordingCallbackHeap,
                                                buffer.index,
                                                m_callbackCookie);
                                    }
                                }
                            } else {
                                CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
                                    " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld), m_recordingBufAvailable(%d)",
                                    __FUNCTION__, __LINE__, buffer.index, timeStamp,
                                    m_lastRecordingTimeStamp,
                                    systemTime(SYSTEM_TIME_MONOTONIC),
                                    m_recordingStartTimeStamp,
                                    (int)m_recordingBufAvailable[buffer.index]);
                                m_recordingTimeStamp[buffer.index] = 0L;
                            }
                        }
                    }
                }
                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;
                struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;
                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP, frame->getFrameCount());
                frame->getDstBuffer(entity->getPipeId(), &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                frame->getMetaData(shot_ext);
                callbackFrame->storeDynamicMeta(shot_ext);
                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);

                if (((m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 2) ||
                    (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
                    m_previewQ->getSizeOfProcessQ() >= 1)) &&
                    (m_previewThread->isRunning() == true)) {
                    CLOGW("WARN(%s[%d]):Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                            __FUNCTION__, __LINE__,
                            m_previewQ->getSizeOfProcessQ(),
                            m_exynosCameraParameters->getPreviewBufferCount());

                    newFrame->setFrameState(FRAME_STATE_SKIPPED);
                    if (buffer.index >= 0) {
                        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                        if (ret < 0)
                            CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                    }

                    callbackFrame->decRef();
                    m_frameMgr->deleteFrame(callbackFrame);
                    callbackFrame = NULL;
                } else if((m_exynosCameraParameters->getFastFpsMode() == 2) && (m_exynosCameraParameters->getRecordingHint() == 1)) {
                    m_skipCount++;
                    CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                    m_previewQ->pushProcessQ(&callbackFrame);
                } else {
                    CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                    m_previewQ->pushProcessQ(&callbackFrame);
                }
                delete shot_ext;
                shot_ext = NULL;
            }
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            //m_mainSetupQ[INDEX(entity->getPipeId())]->pushProcessQ(&newFrame);
            CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            }
        }
        break;
    default:
        break;
    }

entity_state_complete:

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }

    return NO_ERROR;
}

bool ExynosCamera::m_frameFactoryInitThreadFunc(void)
{

#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    bool loop = false;
    status_t ret = NO_ERROR;

    ExynosCameraFrameFactory *framefactory = NULL;

    ret = m_frameFactoryQ->waitAndPopProcessQ(&framefactory);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto func_exit;
    }

    if (framefactory == NULL) {
        CLOGE("ERR(%s[%d]):framefactory is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (framefactory->isCreated() == false) {
        CLOGD("DEBUG(%s[%d]):framefactory create", __FUNCTION__, __LINE__);
        framefactory->create(false);
    } else {
        CLOGD("DEBUG(%s[%d]):framefactory already create", __FUNCTION__, __LINE__);
    }

func_exit:
    if (0 < m_frameFactoryQ->getSizeOfProcessQ()) {
        if (m_previewEnabled == true) {
            loop = true;
        } else {
            CLOGW("WARN(%s[%d]):Sudden stopPreview. so, stop making frameFactory (m_frameFactoryQ->getSizeOfProcessQ() : %d)",
                __FUNCTION__, __LINE__, m_frameFactoryQ->getSizeOfProcessQ());
            loop = false;
        }
    }

    return loop;
}

bool ExynosCamera::m_previewThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    bool loop = true;
    int  pipeId    = 0;
    int  pipeIdCsc = 0;
    int  maxbuffers   = 0;
#ifdef USE_PREVIEW_DURATION_CONTROL
    uint32_t offset = 1000; /* 1ms */
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    uint64_t frameDurationUs;
#endif
    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    frame_queue_t *previewQ;

#ifdef USE_PREVIEW_DURATION_CONTROL
    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    /* Check the Slow/Fast Motion Scenario - sensor : 120fps, preview : 60fps */
    if((curMinFps == 120) && (curMaxFps == 120)) {
        CLOGV("(%s[%d]) : Change PreviewDuration from (%d,%d) to (60000, 60000)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);
        curMinFps = 60;
        curMaxFps = 60;
    }

    frameDurationUs = 1000000/curMaxFps;
    if (frameDurationUs > offset) {
        frameDurationUs = frameDurationUs - offset; /* add the offset value for timing issue */
    }
    PreviewDurationTimer.start();
#endif

    pipeId    = PIPE_SCP;
    pipeIdCsc = PIPE_GSC;

    previewQ = m_previewQ;

    CLOGV("INFO(%s[%d]):wait previewQ", __FUNCTION__, __LINE__);
    ret = previewQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);

        if (frame != NULL) {
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

        return false;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):get frame from previewQ", __FUNCTION__, __LINE__);
    timeStamp = (nsecs_t)frame->getTimeStamp();
    frameCount = frame->getFrameCount();
    ret = frame->getDstBuffer(pipeId, &buffer);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    /* ------------- frome here "frame" cannot use ------------- */
    CLOGV("INFO(%s[%d]):push frame to previewReturnQ", __FUNCTION__, __LINE__);
    if(m_exynosCameraParameters->increaseMaxBufferOfPreview()) {
        maxbuffers = m_exynosCameraParameters->getPreviewBufferCount();
    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (buffer.index < 0 || buffer.index >= maxbuffers ) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, maxbuffers, buffer.index);
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)", __FUNCTION__, __LINE__,
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_exynosCameraParameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                    __FUNCTION__, __LINE__);
            ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            goto func_exit;
        }
    }

    if ((m_previewWindow == NULL) ||
        (m_getRecordingEnabled() == true) ||
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 4 &&
        previewQ->getSizeOfProcessQ() < 2) ||
        (m_exynosCameraParameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1)) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false) {
            ExynosCameraBuffer previewCbBuffer;

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }

            int bufIndex = -2;
            m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);

            ExynosCameraFrame  *newFrame = NULL;

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeIdCsc);
            if (newFrame == NULL) {
                CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
                return UNKNOWN_ERROR;
            }

            ret = m_doPreviewToCallbackFunc(pipeIdCsc, newFrame, buffer, previewCbBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_doPreviewToCallbackFunc fail", __FUNCTION__, __LINE__);
            } else {
                if (m_exynosCameraParameters->getCallbackNeedCopy2Rendering() == true) {
                    ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, buffer);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):m_doCallbackToPreviewFunc fail", __FUNCTION__, __LINE__);
                }
            }

            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }

            m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        }

        if (m_previewWindow != NULL) {
            if (timeStamp > 0L) {
                m_previewWindow->set_timestamp(m_previewWindow, (int64_t)timeStamp);
            } else {
                uint32_t fcount = 0;
                getStreamFrameCount((struct camera2_stream *)buffer.addr[2], &fcount);
                CLOGW("WRN(%s[%d]): frameCount(%d)(%d), Invalid timeStamp(%lld)",
                        __FUNCTION__, __LINE__,
                        frameCount,
                        fcount,
                        timeStamp);
            }
        }

#ifdef FIRST_PREVIEW_TIME_CHECK
        if (m_flagFirstPreviewTimerOn == true) {
            m_firstPreviewTimer.stop();
            m_flagFirstPreviewTimerOn = false;

            CLOGD("DEBUG(%s[%d]):m_firstPreviewTimer stop", __FUNCTION__, __LINE__);

            CLOGD("DEBUG(%s[%d]):============= First Preview time ==================", "m_printExynosCameraInfo", __LINE__);
            CLOGD("DEBUG(%s[%d]):= startPreview ~ first frame  : %d msec", "m_printExynosCameraInfo", __LINE__, (int)m_firstPreviewTimer.durationMsecs());
            CLOGD("DEBUG(%s[%d]):===================================================", "m_printExynosCameraInfo", __LINE__);
        }
#endif

        /* display the frame */
        ret = m_putBuffers(m_scpBufferMgr, buffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }
    } else {
        ALOGW("WARN(%s[%d]):Preview frame buffer is canceled."
                "PreviewThread is blocked or too many buffers are in Service."
                "PreviewBufferCount(%d), ScpBufferMgr(%d), PreviewQ(%d)",
                __FUNCTION__, __LINE__,
                m_exynosCameraParameters->getPreviewBufferCount(),
                m_scpBufferMgr->getNumOfAvailableAndNoneBuffer(),
                previewQ->getSizeOfProcessQ());
        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
    }

func_exit:

    if (frame != NULL) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

#ifdef USE_PREVIEW_DURATION_CONTROL
    PreviewDurationTimer.stop();
    if ((m_getRecordingEnabled() == true) && (curMinFps == curMaxFps) &&
            (m_exynosCameraParameters->getShotMode() != SHOT_MODE_SEQUENCE)) {
        PreviewDurationTime = PreviewDurationTimer.durationUsecs();

        if ( frameDurationUs > PreviewDurationTime ) {
            uint64_t delay = frameDurationUs - PreviewDurationTime;
            CLOGV("(%s):Delay Time(%lld),fpsRange(%d,%d), Duration(%lld)", __FUNCTION__,
                frameDurationUs - PreviewDurationTime,
                curMinFps,
                curMaxFps,
                frameDurationUs);
            usleep(delay);
        }
    }
#endif
    return loop;
}

status_t ExynosCamera::m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr)
{
    /*
     * If it is not 16-aligend, shrink down it as 16 align. ex) 1080 -> 1072
     * But, memory is set on Android format. so, not aligned area will be black.
     */
    int dst_width = 0, dst_height = 0, dst_crop_width = 0, dst_crop_height = 0;
    int dst_format = m_exynosCameraParameters->getPreviewFormat();

    m_exynosCameraParameters->getPreviewSize(&dst_width, &dst_height);
    dst_crop_width = dst_width;
    dst_crop_height = dst_height;

    if (dst_format == V4L2_PIX_FMT_NV21 ||
        dst_format == V4L2_PIX_FMT_NV21M) {

        callbackBuf->size[0] = (dst_width * dst_height);
        callbackBuf->size[1] = (dst_width * dst_height) / 2;

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
    } else if (dst_format == V4L2_PIX_FMT_YVU420 ||
               dst_format == V4L2_PIX_FMT_YVU420M) {
        callbackBuf->size[0] = dst_width * dst_height;
        callbackBuf->size[1] = dst_width / 2 * dst_height / 2;
        callbackBuf->size[2] = callbackBuf->size[1];

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
        callbackBuf->addr[2] = callbackBuf->addr[1] + callbackBuf->size[1];
    }

    CLOGV("DEBUG(%s): dst_size(%dx%d), dst_crop_size(%dx%d)", __FUNCTION__, dst_width, dst_height, dst_crop_width, dst_crop_height);

    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf)
{
    CLOGV("DEBUG(%s): converting preview to callback buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    ExynosCameraDurationTimer probeTimer;
    int probeTimeMSEC;
    uint32_t fcount = 0;

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    ExynosRect srcRect, dstRect;

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_calcPreviewGSCRect(&srcRect, &dstRect);

    if (useCSC) {
        ret = newFrame->setSrcRect(pipeId, &srcRect);
        ret = newFrame->setDstRect(pipeId, &dstRect);

        ret = m_setupEntity(pipeId, newFrame, &previewBuf, &callbackBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        m_previewFrameFactory->setOutputFrameQToPipe(m_previewCallbackGscFrameDoneQ, pipeId);

        CLOGV("INFO(%s[%d]):wait preview callback output", __FUNCTION__, __LINE__);
        ret = m_previewCallbackGscFrameDoneQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            statusRet = INVALID_OPERATION;
            goto done;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        CLOGV("INFO(%s[%d]):preview callback done", __FUNCTION__, __LINE__);

#if 0
        int remainedH = m_orgPreviewRect.h - dst_height;

        if (remainedH != 0) {
            char *srcAddr = NULL;
            char *dstAddr = NULL;
            int planeDiver = 1;

            for (int plane = 0; plane < 2; plane++) {
                planeDiver = (plane + 1) * 2 / 2;

                srcAddr = previewBuf.virt.extP[plane] + (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) * dst_crop_height / planeDiver);
                dstAddr = callbackBuf->virt.extP[plane] + (m_orgPreviewRect.w * dst_crop_height / planeDiver);

                for (int i = 0; i < remainedH; i++) {
                    memcpy(dstAddr, srcAddr, (m_orgPreviewRect.w / planeDiver));

                    srcAddr += (ALIGN_UP(hwPreviewW, CAMERA_ISP_ALIGN) / planeDiver);
                    dstAddr += (m_orgPreviewRect.w                   / planeDiver);
                }
            }
        }
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = previewBuf.addr[plane];
            dstAddr = callbackBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);
        }
    }

    probeTimer.start();
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
        !checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE) &&
        m_disablePreviewCB == false) {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
    }
    probeTimer.stop();
    getStreamFrameCount((struct camera2_stream *)previewBuf.addr[2], &fcount);
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        CLOGV("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        CLOGD("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());
    else
        CLOGV("(%s[%d]):(%d) duration time(%5d msec)", __FUNCTION__, __LINE__, fcount, (int)probeTimer.durationMsecs());

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        int32_t pipeId,
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    CLOGV("DEBUG(%s): converting callback to preview buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    bool useCSC = m_exynosCameraParameters->getCallbackNeedCSC();

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (useCSC) {
#if 0
        if (m_exynosPreviewCSC) {
            csc_set_src_format(m_exynosPreviewCSC,
                    ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    0, 0, ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_MAGIC_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_MAGIC_ALIGN),
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(m_orgPreviewRect.colorFormat),
                    1);

            csc_set_dst_format(m_exynosPreviewCSC,
                    previewW, previewH,
                    0, 0, previewW, previewH,
                    V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat),
                    0);

            csc_set_src_buffer(m_exynosPreviewCSC,
                    (void **)callbackBuf->virt.extP, CSC_MEMORY_USERPTR);

            csc_set_dst_buffer(m_exynosPreviewCSC,
                    (void **)previewBuf.fd.extFd, CSC_MEMORY_TYPE);

            if (csc_convert_with_rotation(m_exynosPreviewCSC, 0, m_flip_horizontal, 0) != 0)
                CLOGE("ERR(%s):csc_convert() from callback to lcd fail", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):m_exynosPreviewCSC == NULL", __FUNCTION__);
        }
#else
        CLOGW("WRN(%s[%d]): doCallbackToPreview use CSC is not yet possible", __FUNCTION__, __LINE__);
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = callbackBuf.addr[plane];
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);
        }
    }

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

status_t ExynosCamera::m_handlePreviewFrameFront(ExynosCameraFrame *frame)
{
    int ret = 0;
    uint32_t skipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeID = 0;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    enum pipeline pipe;

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL, frameCount(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount());
        /* TODO: doing exception handling */
        return true;
    }

    if (entity->getEntityState() == ENTITY_STATE_FRAME_SKIP)
            goto entity_state_complete;

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_ISP:
        m_debugFpsCheck(entity->getPipeId());
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }
        frame->setMetaDataEnable(true);

        ret = m_putBuffers(m_ispBufferMgr, buffer.index);

        ret = frame->getSrcBuffer(PIPE_3AA, &buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        if (m_exynosCameraParameters->isReprocessing() == true) {
            ret = m_captureSelector->manageFrameHoldList(frame, pipeID, true);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        } else {
            /* TODO: This is unusual case, flite buffer and isp buffer */
            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
            }
        }

        skipCount = m_exynosCameraParameters->getFrameSkipCount();
        if (skipCount <= 0 && m_flagStartFaceDetection == true) {
            /* Face detection */
            struct camera2_shot_ext shot;
            frame->getDynamicMeta(&shot);
            fdFrame = m_frameMgr->createFrame(m_exynosCameraParameters, frame->getFrameCount());
            fdFrame->storeDynamicMeta(&shot);
            m_facedetectQ->pushProcessQ(&fdFrame);
        }

        ret = generateFrame(m_3aa_ispFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_FLITE, newFrame);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);

        m_setupEntity(PIPE_3AA, newFrame);
        m_setupEntity(PIPE_ISP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_ISP);

        m_3aa_ispFrameCount++;

        /* HACK: When SCC pipe is stopped, generate frame for SCC */
        if ((m_sccBufferMgr->getNumOfAvailableBuffer() > 2)) {
            CLOGW("WRN(%s[%d]): Too many available SCC buffers, generating frame for SCC", __FUNCTION__, __LINE__);

            pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

            while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = generateFrame(m_sccFrameCount, &newFrame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_setupEntity(pipe, newFrame);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);

                m_sccFrameCount++;
            }
        }

        break;
    case PIPE_ISPC:
    case PIPE_SCC:
        m_debugFpsCheck(entity->getPipeId());
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            ret = m_sccCaptureSelector->manageFrameHoldList(frame, entity->getPipeId(), false);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
            }
        }

        pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

        while (m_sccBufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = generateFrameSccScp(pipe, &m_sccFrameCount, &newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
                return ret;
            }

            m_setupEntity(pipe, newFrame);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipe);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipe);

            m_sccFrameCount++;
        }
        break;
    case PIPE_SCP:
        if (entity->getDstBufState() == ENTITY_BUFFER_STATE_ERROR) {
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }
            ret = m_scpBufferMgr->cancelBuffer(buffer.index);

        } else if (entity->getDstBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
            m_debugFpsCheck(entity->getPipeId());
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                return ret;
            }

            m_exynosCameraParameters->getFrameSkipCount(&m_frameSkipCount);
            if (m_frameSkipCount > 0) {
                CLOGD("INFO(%s[%d]):frameSkipCount=%d", __FUNCTION__, __LINE__, m_frameSkipCount);
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
            } else {
                nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
                if (m_getRecordingEnabled() == true
                    && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
                    if (timeStamp <= 0L) {
                        CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                    } else {
                        /* get Recording Image buffer */
                        int bufIndex = -2;
                        ExynosCameraBuffer recordingBuffer;
                        ret = m_recordingBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                        if (ret < 0 || bufIndex < 0) {
                            if ((++m_recordingFrameSkipCount % 100) == 0) {
                                CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                            }
                        } else {
                            if (m_recordingFrameSkipCount != 0) {
                                CLOGE("ERR(%s[%d]): Recording buffer is not available!! Recording Frames are Skipped(%d frames) (bufIndex=%d)",
                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                m_recordingFrameSkipCount = 0;
                            }
                            m_recordingTimeStamp[bufIndex] = timeStamp;
                            ret = m_doPrviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer);
                            if (ret < 0) {
                                CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                            }
                        }
                    }
                }

                ExynosCameraBuffer callbackBuffer;
                ExynosCameraFrame *callbackFrame = NULL;
                struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;

                callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(PIPE_SCP);
                frame->getDstBuffer(PIPE_SCP, &callbackBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
                frame->getMetaData(shot_ext);
                callbackFrame->storeDynamicMeta(shot_ext);
                callbackFrame->setDstBuffer(PIPE_SCP, callbackBuffer);
                delete shot_ext;

                CLOGV("INFO(%s[%d]):push frame to front previewQ", __FUNCTION__, __LINE__);
                m_previewQ->pushProcessQ(&callbackFrame);

                CLOGV("DEBUG(%s[%d]):SCP done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
            }
        } else {
            CLOGV("DEBUG(%s[%d]):SCP droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
            if (buffer.index >= 0) {
                ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
            }
            m_previewFrameFactory->dump();
        }

        ret = generateFrameSccScp(PIPE_SCP, &m_scpFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrameSccScp fail", __FUNCTION__, __LINE__);
            return ret;
        }

        m_setupEntity(PIPE_SCP, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        /*check preview drop...*/
        newFrame->getDstBuffer(PIPE_SCP, &resultBuffer, m_previewFrameFactory->getNodeType(PIPE_SCP));
        if (resultBuffer.index < 0) {
            newFrame->setRequest(PIPE_SCP, false);
            newFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
            node_group_info_isp.capture[PERFRAME_FRONT_SCP_POS].request = 0;
            newFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

            m_previewFrameFactory->dump();

            /* when preview buffer is not ready, we should drop preview to make preview buffer ready */
            CLOGW("WARN(%s[%d]):Front preview drop. Failed to get preview buffer. FrameSkipcount(%d)",
                    __FUNCTION__, __LINE__, FRAME_SKIP_COUNT_PREVIEW_FRONT);
            m_exynosCameraParameters->setFrameSkipCount(FRAME_SKIP_COUNT_PREVIEW_FRONT);
        }

        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);

        m_scpFrameCount++;
        break;
    default:
        break;
    }

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        return ret;
    }

entity_state_complete:

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_doZoomPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrame *frame)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t previewFormat = 0;
    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;
    uint32_t *output = NULL;
    struct camera2_stream *meta = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int zoom = 0;
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int scpNodeIndex = -1;
    int srcNodeIndex = -1;
    int srcFmt = -1;

    previewFormat = m_exynosCameraParameters->getHwPreviewFormat();
    m_exynosCameraParameters->getHwPreviewSize(&previewW, &previewH);

    /* get Scaler src Buffer Node Index*/
    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        srcFmt = m_exynosCameraParameters->getHWVdisFormat();
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
    } else {
        srcFmt = previewFormat;
        return true;
    }

    /* get scaler source buffer */
    srcBuf.index = -1;
    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",__FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.planeCount-1];

    output = meta->output_crop_region;

    /* check scaler conditions( compare the crop size & format ) */
    if (output[2] == previewW && output[3] == previewH && srcFmt == previewFormat) {
        return ret;
    }

    /* wait unitil get buffers */
    do {
        dstBuf.index = -2;
        waitCount++;

        if (m_scpBufferMgr->getNumOfAvailableBuffer() > 0)
            m_scpBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);

        if (dstBuf.index < 0) {
            usleep(WAITING_TIME);

            if (waitCount % 20 == 0) {
                CLOGW("WRN(%s[%d]):retry JPEG getBuffer(%d) ", __FUNCTION__, __LINE__, bufIndex, m_zoomPreviwWithCscQ->getSizeOfProcessQ());
                m_scpBufferMgr->dump();
            }
        } else {
            break;
        }
        /* this will retry until 300msec */
    } while (waitCount < (TOTAL_WAITING_TIME / WAITING_TIME) && previewEnabled() == false);

    /* csc and scaling */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = output[2];
    srcRect.h = output[3];
    srcRect.fullW = output[2];
    srcRect.fullH = output[3];
    srcRect.colorFormat = srcFmt;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = previewW;
    dstRect.h = previewH;
    dstRect.fullW = previewW;
    dstRect.fullH = previewH;
    dstRect.colorFormat = previewFormat;

    CLOGV("DEBUG(%s[%d]):do zoomPreviw with CSC : srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)", __FUNCTION__, __LINE__, srcBuf.index, dstBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcFmt, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcFmt), dstRect.x, dstRect.y, dstRect.w, dstRect.h, previewFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = frame->setSrcRect(gscPipe, srcRect);
    ret = frame->setDstRect(gscPipe, dstRect);

    ret = m_setupEntity(gscPipe, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, gscPipe, ret);
        ret = INVALID_OPERATION;
    }

    m_previewFrameFactory->pushFrameToPipe(&frame, gscPipe);
    m_previewFrameFactory->setOutputFrameQToPipe(m_zoomPreviwWithCscQ, gscPipe);

    /* wait GSC done */
    CLOGV("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
    waitCount = 0;

    do {
        ret = m_zoomPreviwWithCscQ->waitAndPopProcessQ(&frame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):GSC wait and pop return, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    /* copy metadata src to dst*/
    memcpy(dstBuf.addr[dstBuf.planeCount-1],srcBuf.addr[srcBuf.planeCount-1], sizeof(struct camera2_stream));

    if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        /* in case of dual front path buffer returned frameSelector, do not return buffer. */
    } else {
        ret = m_scpBufferMgr->cancelBuffer(srcBuf.index);
    }

    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setdst Buffer failed(%d)", __FUNCTION__, __LINE__, ret);
    }
    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, scpNodeIndex);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return ret;
    func_exit:


    return ret;
}

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret = 0;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_startPictureInternalThreadFunc(void)
{
    int ret = 0;

    ret = m_startPictureInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_prePictureThreadFunc(void)
{
    bool loop = false;
    bool isProcessed = true;

    ExynosCameraDurationTimer m_burstPrePictureTimer;
    uint64_t m_burstPrePictureTimerTime;
    uint64_t burstWaitingTime;

    status_t ret = NO_ERROR;

    uint64_t seriesShotDuration = m_exynosCameraParameters->getSeriesShotDuration();

    m_burstPrePictureTimer.start();

    if (m_exynosCameraParameters->isReprocessing())
        loop = m_reprocessingPrePictureInternal();
    else
        loop = m_prePictureInternal(&isProcessed);

    m_burstPrePictureTimer.stop();
    m_burstPrePictureTimerTime = m_burstPrePictureTimer.durationUsecs();

    if(isProcessed && loop && seriesShotDuration > 0 && m_burstPrePictureTimerTime < seriesShotDuration) {
        CLOGD("DEBUG(%s[%d]): The time between shots is too short(%lld)us. Extended to (%lld)us"
            , __FUNCTION__, __LINE__, m_burstPrePictureTimerTime, seriesShotDuration);

        burstWaitingTime = seriesShotDuration - m_burstPrePictureTimerTime;
        usleep(burstWaitingTime);
    }

    return loop;
}

/*
 * pIsProcessed : out parameter
 *                true if the frame is properly handled.
 *                false if frame processing is failed or there is no frame to process
 */
bool ExynosCamera::m_prePictureInternal(bool* pIsProcessed)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    ExynosCameraFrame *newFrame = NULL;
    camera2_shot_ext *shot_ext = NULL;

    ExynosCameraBuffer fliteReprocessingBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;
#ifdef DEBUG_RAWDUMP
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer pictureBuffer;
    ExynosCameraFrame *inListFrame = NULL;
    ExynosCameraFrame *bayerFrame = NULL;
#endif
    int pipeId = 0;
    int bufPipeId = 0;
    bool isSrc = false;
    int retryCount = 3;

    if (m_hdrEnabled)
        retryCount = 15;

    if (isOwnScc(getCameraId()) == true)
        bufPipeId = PIPE_SCC;
    else if (m_exynosCameraParameters->isUsing3acForIspc() == true)
        bufPipeId = PIPE_3AC;
    else
        bufPipeId = PIPE_ISPC;

    if (m_exynosCameraParameters->is3aaIspOtf() == true)
        pipeId = PIPE_3AA;
    else
        pipeId = PIPE_ISP;

    int postProcessQSize = m_postPictureQ->getSizeOfProcessQ();
    if (postProcessQSize > 2) {
        CLOGW("DEBUG(%s[%d]): post picture is delayed(stacked %d frames), skip", __FUNCTION__, __LINE__, postProcessQSize);
        usleep(WAITING_TIME);
        goto CLEAN;
    }

    newFrame = m_sccCaptureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, m_pictureFrameFactory->getNodeType(bufPipeId));
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }
    newFrame->frameLock();

    CLOGI("DEBUG(%s[%d]):Frame Count (%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    m_postProcessList.push_back(newFrame);

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_highResolutionCallbackQ->pushProcessQ(&newFrame);
    } else {
    dstSccReprocessingQ->pushProcessQ(&newFrame);
    }

    m_reprocessingCounter.decCount();

    CLOGI("INFO(%s[%d]):prePicture complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0) {
        loop = true;

    }

    *pIsProcessed = true;

#ifdef DEBUG_RAWDUMP
    retryCount = 30; /* 200ms x 30 */
    bayerBuffer.index = -2;

    m_captureSelector->setWaitTime(200000000);
    bayerFrame = m_captureSelector->selectFrames(1, PIPE_FLITE, isSrc, retryCount);
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
    } else {
        ret = bayerFrame->getDstBuffer(PIPE_FLITE, &bayerBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_FLITE, ret);
        } else {
            if (m_exynosCameraParameters->checkBayerDumpEnable()) {
                int sensorMaxW, sensorMaxH;
                int sensorMarginW, sensorMarginH;
                bool bRet;
                char filePath[70];
                int fliteFcount = 0;
                int pictureFcount = 0;

                camera2_shot_ext *shot_ext = NULL;

                ret = newFrame->getDstBuffer(PIPE_3AA, &pictureBuffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA, ret);
                }

                shot_ext = (camera2_shot_ext *)(bayerBuffer.addr[1]);
                if (shot_ext != NULL)
                    fliteFcount = shot_ext->shot.dm.request.frameCount;
                else
                    ALOGE("ERR(%s[%d]):fliteBayerBuffer is null", __FUNCTION__, __LINE__);

                shot_ext->shot.dm.request.frameCount = 0;
                shot_ext = (camera2_shot_ext *)(pictureBuffer.addr[1]);
                if (shot_ext != NULL)
                    pictureFcount = shot_ext->shot.dm.request.frameCount;
                else
                    ALOGE("ERR(%s[%d]):PictureBuffer is null", __FUNCTION__, __LINE__);

                CLOGD("DEBUG(%s[%d]):bayer fcount(%d) picture fcount(%d)", __FUNCTION__, __LINE__, fliteFcount, pictureFcount);
                /* The driver frame count is used to check the match between the 3AA frame and the FLITE frame.
                   if the match fails then the bayer buffer does not correspond to the capture output and hence
                   not written to the file */
                if (fliteFcount == pictureFcount) {
                    memset(filePath, 0, sizeof(filePath));
                    snprintf(filePath, sizeof(filePath), "/data/rawdump/RawCapture%d_%d.raw",m_cameraId, pictureFcount);

                    bRet = dumpToFile((char *)filePath,
                        bayerBuffer.addr[0],
                        bayerBuffer.size[0]);
                    if (bRet != true)
                        CLOGE("couldn't make a raw file");
                }
            }

            if (bayerFrame != NULL) {
                ret = m_bayerBufferMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL);
                if (ret < 0) {
                    ALOGE("ERR(%s[%d]):putBuffer failed Index is %d", __FUNCTION__, __LINE__, bayerBuffer.index);
                    m_bayerBufferMgr->printBufferState();
                    m_bayerBufferMgr->printBufferQState();
                }
                if (m_frameMgr != NULL) {
#ifdef USE_FRAME_REFERENCE_COUNT
                    bayerFrame->decRef();
#endif
                    m_frameMgr->deleteFrame(bayerFrame);
                } else {
                    ALOGE("ERR(%s[%d]):m_frameMgr is NULL (%d)", __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
                }
                bayerFrame = NULL;
            }
        }
    }
#endif
    return loop;

CLEAN:
    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI("INFO(%s[%d]): prePicture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());
    *pIsProcessed = false;   // Notify failure
    return loop;

}

bool ExynosCamera::m_shutterCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    int loop = false;

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
        m_notifyCb(CAMERA_MSG_SHUTTER, m_exynosCameraParameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
    }

    /* one shot */
    return loop;
}

bool ExynosCamera::m_reprocessingPrePictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    int retry = 0;
    int retryIsp = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    uint32_t bayerFrameCount = 0;
    struct camera2_node_output output_crop_info;

    ExynosCameraBufferManager *bufferMgr = NULL;

    int bayerPipeId = 0;
    int prePictureDonePipeId = 0;
    enum pipeline pipe;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;

    /*
     * in case of pureBayer and 3aa_isp OTF, buffer will go isp directly
     */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == true)
            prePictureDonePipeId = PIPE_3AA_REPROCESSING;
        else
            prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    } else {
        prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    }

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        if (m_highResolutionCallbackRunning == true) {
            /* will be removed */
            while (m_skipReprocessing == true) {
                usleep(WAITING_TIME);
                if (m_skipReprocessing == false) {
                    CLOGD("DEBUG(%s[%d]:stop skip frame for high resolution preview callback", __FUNCTION__, __LINE__);
                    break;
                }
            }
        } else if (m_highResolutionCallbackRunning == false) {
            CLOGW("WRN(%s[%d]): m_reprocessingThreadfunc stop for high resolution preview callback", __FUNCTION__, __LINE__);
            loop = false;
            goto CLEAN_FRAME;
        }
    }

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        /* Get Bayer buffer for reprocessing */
        ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer);
    } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Get Bayer buffer for reprocessing */
        ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer, updateDmShot);
    } else {
        CLOGE("ERR(%s[%d]): bayer mode is not valid (%d)", __FUNCTION__, __LINE__,
            m_exynosCameraParameters->getReprocessingBayerMode());
        goto CLEAN_FRAME;
    }

    if (ret < 0) {
        CLOGE("ERR(%s[%d]): getBayerBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

    CLOGD("DEBUG(%s[%d]):bayerBuffer index %d", __FUNCTION__, __LINE__, bayerBuffer.index);

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_frameFliteDeleteBetweenPreviewReprocessing.lock();
        m_captureSelector->clearList(m_getBayerPipeId(), false);
        m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
    }

    m_shutterCallbackThread->join();
    m_shutterCallbackThread->run();

    /* Generate reprocessing Frame */
    ret = generateFrameReprocessing(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrameReprocessing fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        int sensorMaxW, sensorMaxH;
        int sensorMarginW, sensorMarginH;
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",m_cameraId, m_fliteFrameCount);
        if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true)
            /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
        m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        else
            m_exynosCameraParameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);

        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == false) {
        /* TODO: HACK: Will be removed, this is driver's job */
        ret = m_convertingStreamToShotExt(&bayerBuffer, &output_crop_info);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): shot_stream to shot_ext converting fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        camera2_node_group node_group_info;
        ExynosRect srcRect , dstRect;
        int pictureW = 0, pictureH = 0;

        memset(&node_group_info, 0x0, sizeof(camera2_node_group));
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);

        newFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
        node_group_info.leader.input.cropRegion[0] = output_crop_info.cropRegion[0];
        node_group_info.leader.input.cropRegion[1] = output_crop_info.cropRegion[1];
        node_group_info.leader.input.cropRegion[2] = output_crop_info.cropRegion[2];
        node_group_info.leader.input.cropRegion[3] = output_crop_info.cropRegion[3];
        node_group_info.leader.output.cropRegion[0] = 0;
        node_group_info.leader.output.cropRegion[1] = 0;
        node_group_info.leader.output.cropRegion[2] = node_group_info.leader.input.cropRegion[2];
        node_group_info.leader.output.cropRegion[3] = node_group_info.leader.input.cropRegion[3];

        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0] = node_group_info.leader.output.cropRegion[0];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1] = node_group_info.leader.output.cropRegion[1];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0] = 0;
        node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1] = 0;
        if (isOwnScc(getCameraId()) == true) {
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = pictureW;
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = pictureH;
        } else {
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        }

        CLOGV("DEBUG(%s[%d]): isp capture input(%d %d %d %d), output(%d %d %d %d)", __FUNCTION__, __LINE__,
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[0],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[1],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[2],
                                                                                    node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].output.cropRegion[3]);

        if (node_group_info.leader.output.cropRegion[2] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2]) {
            CLOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2],
                node_group_info.leader.output.cropRegion[2]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[2] = node_group_info.leader.output.cropRegion[2];
        }
        if (node_group_info.leader.output.cropRegion[3] < node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3]) {
            CLOGI("INFO(%s[%d]:(%d -> %d))", __FUNCTION__, __LINE__,
                node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3],
                node_group_info.leader.output.cropRegion[3]);

            node_group_info.capture[PERFRAME_REPROCESSING_SCC_POS].input.cropRegion[3] = node_group_info.leader.output.cropRegion[3];
        }

        newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    shot_ext = (struct camera2_shot_ext *)(bayerBuffer.addr[1]);

    /* Meta setting */
    if (shot_ext != NULL) {
        if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
            m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
            ret = newFrame->storeDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }

            ret = newFrame->storeUserDynamicMeta(shot_ext);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }
        } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
            m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
            ret = newFrame->storeDynamicMeta(updateDmShot);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }

            ret = newFrame->storeUserDynamicMeta(updateDmShot);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
                goto CLEAN_FRAME;
            }
        }

        newFrame->getMetaData(shot_ext);
        m_exynosCameraParameters->duplicateCtrlMetadata((void *)shot_ext);

        CLOGD("DEBUG(%s[%d]):meta_shot_ext->shot.dm.request.frameCount : %d",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount(shot_ext));
    } else {
        CLOGE("DEBUG(%s[%d]):shot_ext is NULL", __FUNCTION__, __LINE__);
    }

    /* SCC */
    if (isOwnScc(getCameraId()) == true) {
        pipe = PIPE_SCC_REPROCESSING;

        m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

        ret = m_checkBufferAvailable(pipe, bufferMgr);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): Waiting buffer timeout, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipe, ret);
            goto CLEAN_FRAME;
        }

        ret = m_setupEntity(pipe, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]:setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipe, ret);
            goto CLEAN_FRAME;
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(dstSccReprocessingQ, pipe);
        }

        /* push frame to SCC pipe */
        m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, pipe);
    } else {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false) {
            pipe = PIPE_ISP_REPROCESSING;

            m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

            ret = m_checkBufferAvailable(pipe, bufferMgr);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): Waiting buffer timeout, pipeId(%d), ret(%d)",
                           __FUNCTION__, __LINE__, pipe, ret);
                goto CLEAN_FRAME;
            }

        } else {
            pipe = PIPE_3AA_REPROCESSING;
        }

        if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(dstSccReprocessingQ, pipe);
        }

    }

    /* Add frame to post processing list */
    CLOGD("DEBUG(%s[%d]): postPictureList size(%d), frame(%d)",
        __FUNCTION__, __LINE__, m_postProcessList.size(), newFrame->getFrameCount());
    newFrame->frameLock();
    m_postProcessList.push_back(newFrame);

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        goto CLEAN;
    }

    if (isOwnScc(getCameraId()) == true) {
        retry = 0;
        do {
            if( m_reprocessingFrameFactory->checkPipeThreadRunning(pipe) == false ) {
                ret = m_reprocessingFrameFactory->startInitialThreads();
                if (ret < 0) {
                    CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
                }
            }

            ret = newFrame->ensureDstBufferState(pipe, ENTITY_BUFFER_STATE_PROCESSING);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail(retry), ret(%d)", __FUNCTION__, __LINE__, ret);
                usleep(1000);
                retry++;
            }
        } while (ret < 0 && retry < 100);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
    }

    /* Get bayerPipeId at first entity */
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    CLOGD("DEBUG(%s[%d]): bayer Pipe ID(%d)", __FUNCTION__, __LINE__, bayerPipeId);

    /* Check available buffer */
    ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN;
    }
    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(bayerPipeId, bufferMgr);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): Waiting buffer timeout, bayerPipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, bayerPipeId, ret);
            goto CLEAN;
        }
    }

    ret = m_setupEntity(bayerPipeId, newFrame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:setupEntity fail, bayerPipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, bayerPipeId, ret);
        goto CLEAN;
    }

    m_reprocessingFrameFactory->setOutputFrameQToPipe(dstIspReprocessingQ, prePictureDonePipeId);

    /* push the newFrameReprocessing to pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, bayerPipeId);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true ||
        m_exynosCameraParameters->isSccCapture() == true)
        m_reprocessingFrameFactory->startThread(bayerPipeId);

    /* wait ISP done */
    CLOGI("INFO(%s[%d]):wait ISP output", __FUNCTION__, __LINE__);
    do {
        ret = dstIspReprocessingQ->waitAndPopProcessQ(&newFrame);
        retryIsp++;
    } while (ret == TIMED_OUT && retryIsp < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):ISP wait and pop return, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        /* goto CLEAN; */
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, bayerPipeId, ret);

        if (updateDmShot != NULL) {
            delete updateDmShot;
            updateDmShot = NULL;
        }

        return ret;
    }

    CLOGI("INFO(%s[%d]):ISP output done", __FUNCTION__, __LINE__);

    newFrame->setMetaDataEnable(true);

    /* put bayer buffer */
    ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): 3AA src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
        goto CLEAN;
    }

    /* put isp buffer */
    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
        if (bufferMgr != NULL) {
            ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_FLITE));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): ISP src putBuffer fail, index(%d), ret(%d)", __FUNCTION__, __LINE__, bayerBuffer.index, ret);
                goto CLEAN;
            }
        }
    }

    m_reprocessingCounter.decCount();

    CLOGI("INFO(%s[%d]):reprocessing complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    if (updateDmShot != NULL) {
        delete updateDmShot;
        updateDmShot = NULL;
    }

    /* one shot */
    return loop;

CLEAN_FRAME:
    /* newFrame is not pushed any pipes, we can delete newFrame */
    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

CLEAN:
    if (bayerBuffer.index != -2 && m_bayerBufferMgr != NULL)
        m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
    if (ispReprocessingBuffer.index != -2 && m_ispReprocessingBufferMgr != NULL)
        m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);

    /* newFrame is already pushed some pipes, we can not delete newFrame until frame is complete */
    if (newFrame != NULL) {
        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if (updateDmShot != NULL) {
        delete updateDmShot;
        updateDmShot = NULL;
    }

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_exynosCameraParameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI("INFO(%s[%d]): reprocessing fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    return loop;
}

bool ExynosCamera::m_CheckBurstJpegSavingPath(char *dir)
{
    int ret = false;

    struct dirent **items;
    struct stat fstat;

    char *burstPath = m_exynosCameraParameters->getSeriesShotFilePath();

    char ChangeDirPath[BURST_CAPTURE_FILEPATH_SIZE] = {'\0',};

    memset(m_burstSavePath, 0, sizeof(m_burstSavePath));

    // Check access path
    if (burstPath && sizeof(m_burstSavePath) >= sizeof(burstPath)) {
        strncpy(m_burstSavePath, burstPath, sizeof(m_burstSavePath)-1);
    } else {
        CLOGW("WARN(%s[%d]) Parameter burstPath is NULL. Change to Default Path", __FUNCTION__, __LINE__);
        snprintf(m_burstSavePath, sizeof(m_burstSavePath), "%s/DCIM/Camera/", dir);
    }

    if (access(m_burstSavePath, 0)==0) {
        CLOGW("WARN(%s[%d]) success access dir = %s", __FUNCTION__, __LINE__, m_burstSavePath);
        return true;
    }

    CLOGW("WARN(%s[%d]) can't find dir = %s", __FUNCTION__, __LINE__, m_burstSavePath);

    // If directory cant't access, then search "DCIM/Camera" folder in current directory
    int iitems = scandir(dir, &items, NULL, alphasort);
    for (int i = 0; i < iitems; i++) {
        // Search only dcim directory
        lstat(items[i]->d_name, &fstat);
        if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
            if (!strcmp(items[i]->d_name, ".") || !strcmp(items[i]->d_name, ".."))
                continue;
            if (strcasecmp(items[i]->d_name, "DCIM")==0) {
                sprintf(ChangeDirPath, "%s/%s", dir, items[i]->d_name);
                int jitems = scandir(ChangeDirPath, &items, NULL, alphasort);
                for (int j = 0; j < jitems; j++) {
                    // Search only camera directory
                    lstat(items[j]->d_name, &fstat);
                    if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
                        if (!strcmp(items[j]->d_name, ".") || !strcmp(items[j]->d_name, ".."))
                            continue;
                        if (strcasecmp(items[j]->d_name, "CAMERA")==0) {
                            sprintf(m_burstSavePath, "%s/%s/", ChangeDirPath, items[j]->d_name);
                            CLOGW("WARN(%s[%d]) change save path = %s", __FUNCTION__, __LINE__, m_burstSavePath);
                            j = jitems;
                            ret = true;
                            break;
                        }
                    }
                }
                i = iitems;
                break;
            }
        }
    }

    if (items != NULL) {
        free(items);
    }

    return ret;
}

bool ExynosCamera::m_pictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int retryCountGSC = 4;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    struct camera2_stream *shot_stream = NULL;
    ExynosRect srcRect, dstRect;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int buffer_idx = getShotBufferIdex();
    float zoomRatio = m_exynosCameraParameters->getZoomRatio(0) / 1000;

    sccReprocessingBuffer.index = -2;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;
    bool isSrc = false;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (m_exynosCameraParameters->isReprocessing3aaIspOTF() == false)
            pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;

        pipeId_gsc = PIPE_GSC_REPROCESSING;
        isSrc = true;
    } else if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
        pipeId_scc = PIPE_3AA;
        pipeId_gsc = PIPE_GSC_PICTURE;
    } else {
        switch (getCameraId()) {
            case CAMERA_ID_FRONT:
                if (m_exynosCameraParameters->getDualMode() == true) {
                    pipeId_scc = PIPE_3AA;
                } else {
                    pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;
                }
                pipeId_gsc = PIPE_GSC_PICTURE;
                break;
            default:
                CLOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    __FUNCTION__, __LINE__, getCameraId(), m_exynosCameraParameters->isReprocessing());
                break;
        }
    }

    /* wait SCC */
    CLOGI("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    int retry = 0;
    do {
        ret = dstSccReprocessingQ->waitAndPopProcessQ(&newFrame);
        retry++;
    } while (ret == TIMED_OUT && retry < 40 &&
             (m_takePictureCounter.getCount() > 0 || m_exynosCameraParameters->getSeriesShotCount() == 0));

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d), retry(%d), takePictuerCount(%d), seriesShotCount(%d)",
                __FUNCTION__, __LINE__, ret, retry, m_takePictureCounter.getCount(), m_exynosCameraParameters->getSeriesShotCount());
        // TODO: doing exception handling
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_postProcessList.size() <= 0) {
        CLOGE("ERR(%s[%d]): postPictureList size(%d)", __FUNCTION__, __LINE__, m_postProcessList.size());
        usleep(5000);
        if(m_postProcessList.size() <= 0) {
            CLOGE("ERR(%s[%d]):Retry postPictureList size(%d)", __FUNCTION__, __LINE__, m_postProcessList.size());
            goto CLEAN;
        }
    }

    /*
     * When Non-reprocessing scenario does not setEntityState,
     * because Non-reprocessing scenario share preview and capture frames
     */
    if (m_exynosCameraParameters->isReprocessing() == true) {
        ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            return ret;
        }
    }

    CLOGI("INFO(%s[%d]):SCC output done, frame Count(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

    /* Shutter Callback */
    if (pipeId_scc != PIPE_SCC_REPROCESSING &&
        pipeId_scc != PIPE_ISP_REPROCESSING) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
            m_notifyCb(CAMERA_MSG_SHUTTER, m_exynosCameraParameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
        }
    }

    if (needGSCForCapture(getCameraId()) == true) {
        /* set GSC buffer */
        if (m_exynosCameraParameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else if (m_exynosCameraParameters->isUsing3acForIspc() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream != NULL) {
            CLOGD("DEBUG(%s[%d]):(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->fcount,
                shot_stream->rcount,
                shot_stream->findex,
                shot_stream->fvalid);
            CLOGD("DEBUG(%s[%d]):(%d %d %d %d)(%d %d %d %d)", __FUNCTION__, __LINE__,
                shot_stream->input_crop_region[0],
                shot_stream->input_crop_region[1],
                shot_stream->input_crop_region[2],
                shot_stream->input_crop_region[3],
                shot_stream->output_crop_region[0],
                shot_stream->output_crop_region[1],
                shot_stream->output_crop_region[2],
                shot_stream->output_crop_region[3]);
        } else {
            CLOGE("DEBUG(%s[%d]):shot_stream is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        int retry = 0;
        m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
        do {
            ret = -1;
            retry++;
            if (bufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0) {
                CLOGW("WRAN(%s[%d]):retry setupEntity for GSC postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                        __FUNCTION__, __LINE__,
                        m_postPictureQ->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD0]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD1]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD2]->getSizeOfProcessQ());
            }
        } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

        if (ret < 0) {
            if (retry >= (TOTAL_WAITING_TIME/WAITING_TIME)) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
		/* HACK for debugging P150108-08143 */
		bufferMgr->printBufferState();
		android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]): unexpected error, get GSC buffer failed, assert!!!!", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]):setupEntity stopped, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, retry, ret, m_stopBurstShot);
            }
            goto CLEAN;
        }
/* should change size calculation code in pure bayer */
#if 0
        if (shot_stream != NULL) {
            ret = m_calcPictureRect(&srcRect, &dstRect);
            ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
            ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
        }
#else
        m_exynosCameraParameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_exynosCameraParameters->getPictureFormat();
#if 1   /* HACK in case of 3AA-OTF-ISP input_cropRegion always 0, use output crop region, check the driver */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
#endif
        srcRect.fullW = shot_stream->output_crop_region[2];
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = pictureFormat;

        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = pictureW;
        dstRect.h = pictureH;
        dstRect.fullW = pictureW;
        dstRect.fullH = pictureH;
        dstRect.colorFormat = JPEG_INPUT_COLOR_FMT;

        ret = getCropRectAlign(srcRect.w,  srcRect.h,
                               pictureW,   pictureH,
                               &srcRect.x, &srcRect.y,
                               &srcRect.w, &srcRect.h,
                               2, 2, 0, zoomRatio);

        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
#endif

        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);

        /* wait GSC */
        newFrame = NULL;
        CLOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        while (retryCountGSC > 0) {
            ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                m_pictureFrameFactory->startThread(pipeId_gsc);
            } else if (ret < 0) {
                CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else {
                break;
            }
            retryCountGSC--;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        CLOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        /* put SCC buffer */
        if (m_exynosCameraParameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else if (m_exynosCameraParameters->isUsing3acForIspc() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        ret = m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s)(%d):m_putBuffers fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
    }

    /* push postProcess */
    m_postPictureQ->pushProcessQ(&newFrame);

    m_pictureCounter.decCount();

    CLOGI("INFO(%s[%d]):picture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0) {
        loop = true;
    } else {
        if (m_exynosCameraParameters->isReprocessing() == true)
            CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
        else {
            if (m_exynosCameraParameters->getUseDynamicScc() == true) {
                CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);

                if (isOwnScc(getCameraId()) == true)
                    m_previewFrameFactory->setRequestSCC(false);
                else
                    m_previewFrameFactory->setRequestISPC(false);
            } else if (m_exynosCameraParameters->getRecordingHint() == true
                       && m_exynosCameraParameters->isUsing3acForIspc() == true) {
                CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
                m_previewFrameFactory->setRequest3AC(false);
            }

            m_frameFliteDeleteBetweenPreviewReprocessing.lock();
            m_sccCaptureSelector->clearList(pipeId_scc, isSrc);
            m_frameFliteDeleteBetweenPreviewReprocessing.unlock();
        }

        dstSccReprocessingQ->release();
    }

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): putBuffer sccReprocessingBuffer(index:%d) in error state",
                __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
        m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
    }

    CLOGI("INFO(%s[%d]):take picture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0)
        loop = true;

    /* one shot */
    return loop;
}

camera_memory_t *ExynosCamera::m_getJpegCallbackHeap(ExynosCameraBuffer jpegBuf, int seriesShotNumber)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    camera_memory_t *jpegCallbackHeap = NULL;

#ifdef BURST_CAPTURE
    if (1 < m_exynosCameraParameters->getSeriesShotCount()) {
        int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

        if (seriesShotNumber < 0 || seriesShotNumber > m_exynosCameraParameters->getSeriesShotCount()) {
             CLOGE("ERR(%s[%d]): Invalid shot number (%d)", __FUNCTION__, __LINE__, seriesShotNumber);
             goto done;
        }

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            CLOGD("DEBUG(%s[%d]):burst callback : size (%d), count(%d)", __FUNCTION__, __LINE__, jpegBuf.size[0], seriesShotNumber);

            jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, jpegBuf.size[0]);
                goto done;
            }
            if (jpegBuf.fd[0] < 0)
                memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
        } else {
            char filePath[100];
            int nw, cnt = 0;
            uint32_t written = 0;
            camera_memory_t *tempJpegCallbackHeap = NULL;

            memset(filePath, 0, sizeof(filePath));

            snprintf(filePath, sizeof(filePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
            CLOGD("DEBUG(%s[%d]):burst callback : size (%d), filePath(%s)", __FUNCTION__, __LINE__, jpegBuf.size[0], filePath);

            jpegCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%s) fail", __FUNCTION__, __LINE__, filePath);
                goto done;
            }

            memcpy(jpegCallbackHeap->data, filePath, sizeof(filePath));
        }
    } else
#endif
    {
        CLOGD("DEBUG(%s[%d]):general callback : size (%d)", __FUNCTION__, __LINE__, jpegBuf.size[0]);

        jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
        if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, jpegBuf.size[0]);
            goto done;
        }

        if (jpegBuf.fd[0] < 0)
            memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
    }

done:
    if (jpegCallbackHeap == NULL ||
        jpegCallbackHeap->data == MAP_FAILED) {

        if (jpegCallbackHeap) {
            jpegCallbackHeap->release(jpegCallbackHeap);
            jpegCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    CLOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return jpegCallbackHeap;
}

bool ExynosCamera::m_postPictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int buffer_idx = getShotBufferIdex();
    int retryCountJPEG = 4;

    ExynosCameraFrame *newFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;

    int currentSeriesShotMode = 0;

    if (m_exynosCameraParameters->isReprocessing() == true) {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            pipeId_gsc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        }
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        if (needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (isOwnScc(getCameraId()) == true) {
                pipeId_gsc = PIPE_SCC;
            } else {
                if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                    pipeId_gsc = PIPE_3AA;
                else
                    pipeId_gsc = PIPE_ISPC;
            }
        }

        pipeId_jpeg = PIPE_JPEG;
    }

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):wait postPictureQ output", __FUNCTION__, __LINE__);
    ret = m_postPictureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD("DEBUG(%s[%d]): Picture canceled", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    CLOGI("INFO(%s[%d]):postPictureQ output done", __FUNCTION__, __LINE__);

    /* put picture callback buffer */
    /* get gsc dst buffers */
    ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

    /* callback */
    if (m_hdrEnabled == false && m_exynosCameraParameters->getSeriesShotCount() <= 0) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            CLOGD("DEBUG(%s[%d]): RAW callabck", __FUNCTION__, __LINE__);
            camera_memory_t *rawCallbackHeap = NULL;
            rawCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            rawCallbackHeap->release(rawCallbackHeap);
        }

        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY)) {
            CLOGD("DEBUG(%s[%d]): RAW_IMAGE_NOTIFY callabck", __FUNCTION__, __LINE__);

            m_notifyCb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, m_callbackCookie);
        }

        if ((m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))) {
            CLOGD("DEBUG(%s[%d]): POSTVIEW callabck", __FUNCTION__, __LINE__);

            camera_memory_t *postviewCallbackHeap = NULL;
            postviewCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            postviewCallbackHeap->release(postviewCallbackHeap);
        }
    }

    currentSeriesShotMode = m_exynosCameraParameters->getSeriesShotMode();

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0 ||
        m_hdrEnabled == true) {

        /* HDR callback */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGD("DEBUG(%s[%d]): HDR callback", __FUNCTION__, __LINE__);

            /* send yuv image with jpeg callback */
            camera_memory_t    *jpegCallbackHeap = NULL;
            jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

            jpegCallbackHeap->release(jpegCallbackHeap);

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            m_jpegCounter.decCount();
        } else {
            int retry = 0;

            /* 1. get wait available JPEG src buffer */
            do {
                bufIndex = -2;
                retry++;

                if (m_pictureEnabled == false) {
                    CLOGI("INFO(%s[%d]):m_pictureEnable is false", __FUNCTION__, __LINE__);
                    goto CLEAN;
                }
                if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0)
                    m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegReprocessingBuffer);

                if (bufIndex < 0) {
                    usleep(WAITING_TIME);

                    if (retry % 20 == 0) {
                        CLOGW("WRN(%s[%d]):retry JPEG getBuffer(%d) postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                                __FUNCTION__, __LINE__, bufIndex,
                                m_postPictureQ->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD0]->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD1]->getSizeOfProcessQ(),
                                m_jpegSaveQ[JPEG_SAVE_THREAD2]->getSizeOfProcessQ());
                        m_jpegBufferMgr->dump();
                    }
                }
                /* this will retry until 300msec */
            } while (bufIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_stopBurstShot == false);

            if (bufIndex < 0) {
                if (retry >= (TOTAL_WAITING_TIME / WAITING_TIME)) {
                    CLOGE("ERR(%s[%d]):getBuffer totally fail, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
		    /* HACK for debugging P150108-08143 */
		    bufferMgr->printBufferState();
		    android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]): unexpected error, get jpeg buffer failed, assert!!!!", __FUNCTION__, __LINE__);
                } else {
                    CLOGD("DEBUG(%s[%d]):getBuffer stopped, retry(%d), m_stopBurstShot(%d)",
                            __FUNCTION__, __LINE__, retry, m_stopBurstShot);
                }
                ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
                goto CLEAN;
            }

            /* 2. setup Frame Entity */
            ret = m_setupEntity(pipeId_jpeg, newFrame, &gscReprocessingBuffer, &jpegReprocessingBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                goto CLEAN;
            }

            /* 3. Q Set-up */
            m_pictureFrameFactory->setOutputFrameQToPipe(dstJpegReprocessingQ, pipeId_jpeg);

            /* 4. push the newFrame to pipe */
            m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_jpeg);

            /* 5. wait outputQ */
            CLOGI("INFO(%s[%d]):wait Jpeg output", __FUNCTION__, __LINE__);
            while (retryCountJPEG > 0) {
                ret = dstJpegReprocessingQ->waitAndPopProcessQ(&newFrame);
                if (ret == TIMED_OUT) {
                    CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                    m_pictureFrameFactory->startThread(pipeId_jpeg);
                } else if (ret < 0) {
                    CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    /* TODO: doing exception handling */
                    goto CLEAN;
                } else {
                    break;
                }
                retryCountJPEG--;
            }

            if (newFrame == NULL) {
                CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
                goto CLEAN;
            }

            /*
             * When Non-reprocessing scenario does not setEntityState,
             * because Non-reprocessing scenario share preview and capture frames
             */
            if (m_exynosCameraParameters->isReprocessing() == true) {
                ret = newFrame->setEntityState(pipeId_jpeg, ENTITY_STATE_COMPLETE);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                    return ret;
                }
            }

            /* put GSC buffer */
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }

            int jpegOutputSize = newFrame->getJpegSize();
            CLOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

            if (jpegOutputSize <= 0) {
                CLOGW("WRN(%s[%d]): jpegOutput size(%d) is invalid", __FUNCTION__, __LINE__, jpegOutputSize);
                jpegOutputSize = jpegReprocessingBuffer.size[0];
            }

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

            /* push postProcess to call CAMERA_MSG_COMPRESSED_IMAGE */
            jpeg_callback_buffer_t jpegCallbackBuf;
            jpegCallbackBuf.buffer = jpegReprocessingBuffer;
#ifdef BURST_CAPTURE
            m_burstCaptureCallbackCount++;
            CLOGI("INFO(%s[%d]): burstCaptureCallbackCount(%d)", __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
#endif
retry:
            if ((m_exynosCameraParameters->getSeriesShotCount() > 0)) {
                int threadNum = 0;

                if (m_burst[JPEG_SAVE_THREAD0] == false && m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD0;
                } else if (m_burst[JPEG_SAVE_THREAD1] == false && m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD1;
                } else if (m_burst[JPEG_SAVE_THREAD2] == false && m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD2;
                } else {
                    CLOGW("WARN(%s[%d]): wait for available save thread, thread running(%d, %d, %d,)",
                            __FUNCTION__, __LINE__,
                            m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning());
                    usleep(WAITING_TIME * 10);
                    goto retry;
                }

                m_burst[threadNum] = true;
                ret = m_jpegSaveThread[threadNum]->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]): m_jpegSaveThread%d run fail, ret(%d)", __FUNCTION__, __LINE__, threadNum, ret);
                    m_burst[threadNum] = false;
                    m_running[threadNum] = false;
                    goto retry;
                }

                jpegCallbackBuf.callbackNumber = m_burstCaptureCallbackCount;
                m_jpegSaveQ[threadNum]->pushProcessQ(&jpegCallbackBuf);
            } else {
                jpegCallbackBuf.callbackNumber = 0;
                m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
            }

            m_jpegCounter.decCount();
        }
    } else {
        CLOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);

        /* put GSC buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        m_jpegCounter.decCount();
    }

    if (newFrame != NULL) {
        newFrame->printEntity();
        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    CLOGI("INFO(%s[%d]):postPicture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCounter.getCount());

    if (m_jpegCounter.getCount() <= 0) {
        if (m_hdrEnabled == true) {
            CLOGI("INFO(%s[%d]): End of HDR capture!", __FUNCTION__, __LINE__);
            m_hdrEnabled = false;
            m_pictureEnabled = false;
        }
        if (currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
            currentSeriesShotMode == SERIES_SHOT_MODE_SIS) {
            CLOGI("INFO(%s[%d]): End of LLS/SIS capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;
        }

        if(m_exynosCameraParameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGI("INFO(%s[%d]): End of wideselfie capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;
        }

        CLOGD("DEBUG(%s[%d]): free gsc buffers", __FUNCTION__, __LINE__);
        m_gscBufferMgr->resetBuffers();

        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST) {
            CLOGD("DEBUG(%s[%d]): clearList postProcessList, series shot mode(%d)", __FUNCTION__, __LINE__, currentSeriesShotMode);
            if (m_clearList(&m_postProcessList) < 0) {
                CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
            }
        }
    }

    if (m_exynosCameraParameters->getScalableSensorMode()) {
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_CHANGING);
        ret = m_restartPreviewInternal();
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_NONE);
    }

CLEAN:
    /* HACK: Sometimes, m_postPictureThread is finished without waiting the last picture */
    int waitCount = 5;
    while (m_postPictureQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_postPictureQ->getSizeOfProcessQ() > 0 ||
            currentSeriesShotMode != SERIES_SHOT_MODE_NONE) {
        CLOGD("DEBUG(%s[%d]):postPicture thread will run again.  currentSeriesShotMode(%d), postPictureQ size(%d)",
            __func__, __LINE__, currentSeriesShotMode, m_postPictureQ->getSizeOfProcessQ());
        loop = true;
    }

    return loop;
}

bool ExynosCamera::m_jpegSaveThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int curThreadNum = -1;
    char burstFilePath[100];
#ifdef BURST_CAPTURE
    int fd = -1;
#endif

    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegSaveBuffer;
    int seriesShotNumber = -1;
//    camera_memory_t *jpegCallbackHeap = NULL;

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_burst[threadNum] == true && m_running[threadNum] == false) {
            m_running[threadNum] = true;
            curThreadNum = threadNum;
            if (m_jpegSaveQ[curThreadNum]->waitAndPopProcessQ(&jpegCallbackBuf) < 0) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                goto done;
            }
            break;
        }
    }

    if (curThreadNum < 0 || curThreadNum > JPEG_SAVE_THREAD2) {
        CLOGE("ERR(%s[%d]): invalid thrad num (%d)", __FUNCTION__, __LINE__, curThreadNum);
        goto done;
    }

    jpegSaveBuffer = jpegCallbackBuf.buffer;
    seriesShotNumber = jpegCallbackBuf.callbackNumber;

#ifdef BURST_CAPTURE
    if (m_exynosCameraParameters->getSeriesShotCount() > 0) {

        int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            jpegCallbackBuf.buffer = jpegSaveBuffer;
            jpegCallbackBuf.callbackNumber = 0;
            m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
            goto done;
        } else {
            int nw, cnt = 0;
            uint32_t written = 0;
            camera_memory_t *tempJpegCallbackHeap = NULL;

            memset(burstFilePath, 0, sizeof(burstFilePath));

            m_burstCaptureCallbackCountLock.lock();
            snprintf(burstFilePath, sizeof(burstFilePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
            m_burstCaptureCallbackCountLock.unlock();

            CLOGD("DEBUG(%s[%d]):%s fd:%d jpegSize : %d", __FUNCTION__, __LINE__, burstFilePath, jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0]);

            m_burstCaptureSaveLock.lock();

            fd = open(burstFilePath, O_RDWR | O_CREAT, 0664);
            if (fd < 0) {
                CLOGD("DEBUG(%s[%d]):failed to create file [%s]: %s",
                    __FUNCTION__, __LINE__, burstFilePath, strerror(errno));
                m_burstCaptureSaveLock.unlock();
                goto done;
            }

            m_burstSaveTimer.start();
            CLOGD("DEBUG(%s[%d]):%s jpegSize : %d", __FUNCTION__, __LINE__, burstFilePath, jpegSaveBuffer.size[0]);

           char *data = NULL;

            if (jpegSaveBuffer.fd[0] < 0) {
                data = jpegSaveBuffer.addr[0];
            } else {
                /* TODO : we need to use jpegBuf's buffer directly */
                tempJpegCallbackHeap = m_getMemoryCb(jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0], 1, m_callbackCookie);
                if (!tempJpegCallbackHeap || tempJpegCallbackHeap->data == MAP_FAILED) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb(fd:%d, size:%d) fail", __FUNCTION__, __LINE__, jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0]);
                    m_burstCaptureSaveLock.unlock();
                    goto done;
                }

                data = (char *)tempJpegCallbackHeap->data;
            }

            CLOGD("DEBUG(%s[%d]):(%s)file write start)", __FUNCTION__, __LINE__, burstFilePath);
            while (written < jpegSaveBuffer.size[0]) {
                nw = ::write(fd, (const char *)(data) + written, jpegSaveBuffer.size[0] - written);

                if (nw < 0) {
                    CLOGD("DEBUG(%s[%d]):failed to write file [%s]: %s",
                        __FUNCTION__, __LINE__, burstFilePath, strerror(errno));
                    break;
                }

                written += nw;
                cnt++;
            }
            CLOGD("DEBUG(%s[%d]):(%s)file write end)", __FUNCTION__, __LINE__, burstFilePath);

            if (fd > 0)
                ::close(fd);

            if (chmod(burstFilePath,0664) < 0) {
                CLOGE("failed chmod [%s]", burstFilePath);
            }
            if (chown(burstFilePath,AID_MEDIA,AID_MEDIA_RW) < 0) {
                CLOGE("failed chown [%s] user(%d), group(%d)", burstFilePath,AID_MEDIA,AID_MEDIA_RW);
            }

            m_burstCaptureSaveLock.unlock();

            if (tempJpegCallbackHeap) {
                tempJpegCallbackHeap->release(tempJpegCallbackHeap);
                tempJpegCallbackHeap = NULL;
            }

            m_burstSaveTimer.stop();
            m_burstSaveTimerTime = m_burstSaveTimer.durationUsecs();
            if (m_burstSaveTimerTime > (m_burstDuration - 33000)) {
                m_burstDuration += (int)((m_burstSaveTimerTime - m_burstDuration + 33000) / 33000) * 33000;
                CLOGD("Increase burst duration = %d", m_burstDuration);
            }

            CLOGD("DEBUG(%s[%d]):m_burstSaveTimerTime : %d msec, path(%s)", __FUNCTION__, __LINE__, (int)m_burstSaveTimerTime / 1000, burstFilePath);
        }
        jpegCallbackBuf.buffer = jpegSaveBuffer;
        jpegCallbackBuf.callbackNumber = seriesShotNumber;
        m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
    } else
#endif
    {
        jpegCallbackBuf.buffer = jpegSaveBuffer;
        jpegCallbackBuf.callbackNumber = 0;
        m_jpegCallbackQ->pushProcessQ(&jpegCallbackBuf);
    }

done:
/*
    if (jpegCallbackHeap == NULL ||
        jpegCallbackHeap->data == MAP_FAILED) {

        if (jpegCallbackHeap) {
            jpegCallbackHeap->release(jpegCallbackHeap);
            jpegCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }
*/
    if (JPEG_SAVE_THREAD0 <= curThreadNum && curThreadNum < JPEG_SAVE_THREAD_MAX_COUNT) {
        m_burst[curThreadNum] = false;
        m_running[curThreadNum] = false;
    }

    CLOGI("INFO(%s[%d]):saving jpeg buffer done", __FUNCTION__, __LINE__);

    /* one shot */
    return false;
}

bool ExynosCamera::m_jpegCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int retry = 0, maxRetry = 0;
    int loop = false;
    int seriesShotNumber = -1;

    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegCallbackBuffer;
    camera_memory_t *jpegCallbackHeap = NULL;

    jpegCallbackBuffer.index = -2;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

    do {
        ret = m_jpegCallbackQ->waitAndPopProcessQ(&jpegCallbackBuf);
        if (ret < 0) {
            retry++;
            CLOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
        }
    } while(ret < 0 && retry < maxRetry && m_jpegCounter.getCount() > 0);

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        loop = true;
        goto CLEAN;
    }

    jpegCallbackBuffer = jpegCallbackBuf.buffer;
    seriesShotNumber = jpegCallbackBuf.callbackNumber;

    CLOGD("DEBUG(%s[%d]):jpeg calllback is start", __FUNCTION__, __LINE__);

    /* Make compressed image */
    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_exynosCameraParameters->getSeriesShotCount() > 0) {
            m_captureLock.lock();
            camera_memory_t *jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            if (jpegCallbackHeap == NULL) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

            setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callabck (%d)", __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);

            /* put JPEG callback buffer */
            if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);

            jpegCallbackHeap->release(jpegCallbackHeap);
    } else {
        CLOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);
    }

CLEAN:
    CLOGI("INFO(%s[%d]):jpeg callback thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
    if (m_takePictureCounter.getCount() == 0) {
        m_pictureEnabled = false;
        loop = false;
        m_clearJpegCallbackThread(true);
        m_captureLock.unlock();
    } else {
        m_captureLock.unlock();
    }

    return loop;
}

void ExynosCamera::m_clearJpegCallbackThread(bool callFromJpeg)
{
    jpeg_callback_buffer_t jpegCallbackBuf;
    ExynosCameraBuffer jpegCallbackBuffer;
    int ret = 0;

    CLOGI("INFO(%s[%d]): takePicture disabled, takePicture callback done takePictureCounter(%d)",
            __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
    m_pictureEnabled = false;

    if (m_exynosCameraParameters->getUseDynamicScc() == true) {
        CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
        if (isOwnScc(getCameraId()) == true)
            m_previewFrameFactory->setRequestSCC(false);
        else
            m_previewFrameFactory->setRequestISPC(false);
    }

    m_prePictureThread->requestExit();
    m_pictureThread->requestExit();
    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();

    CLOGI("INFO(%s[%d]): wait m_prePictureThrad", __FUNCTION__, __LINE__);
    m_prePictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_pictureThrad", __FUNCTION__, __LINE__);
    m_pictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_postPictureThrad", __FUNCTION__, __LINE__);
    m_postPictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_jpegCallbackThrad", __FUNCTION__, __LINE__);
    if (!callFromJpeg)
        m_jpegCallbackThread->requestExitAndWait();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
         CLOGI("INFO(%s[%d]): wait m_jpegSaveThrad%d", __FUNCTION__, __LINE__, threadNum);
         m_jpegSaveThread[threadNum]->requestExitAndWait();
    }

    CLOGI("INFO(%s[%d]): All picture threads done", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters->isReprocessing() == true) {
        enum pipeline pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;

        m_reprocessingFrameFactory->stopThread(pipe);
    }

    while (m_jpegCallbackQ->getSizeOfProcessQ() > 0) {
        m_jpegCallbackQ->popProcessQ(&jpegCallbackBuf);
        jpegCallbackBuffer = jpegCallbackBuf.buffer;

        CLOGD("DEBUG(%s[%d]):put remaining jpeg buffer(index: %d)", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        }

        int seriesShotSaveLocation = m_exynosCameraParameters->getSeriesShotSaveLocation();
        char command[100];
        memset(command, 0, sizeof(command));

        snprintf(command, sizeof(command), "rm %sBurst%02d.jpg", m_burstSavePath, jpegCallbackBuf.callbackNumber);

        system(command);
        CLOGD("run %s", command);
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        while (m_jpegSaveQ[threadNum]->getSizeOfProcessQ() > 0) {
            m_jpegSaveQ[threadNum]->popProcessQ(&jpegCallbackBuf);
            jpegCallbackBuffer = jpegCallbackBuf.buffer;

            CLOGD("DEBUG(%s[%d]):put remaining SaveQ%d jpeg buffer(index: %d)",
                    __FUNCTION__, __LINE__, threadNum, jpegCallbackBuffer.index);
            if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
            }

        }

            m_burst[threadNum] = false;
        }

    CLOGD("DEBUG(%s[%d]): clear postProcessList", __FUNCTION__, __LINE__);
    if (m_clearList(&m_postProcessList) < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }

#if 1
    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    m_postPictureQ->release();

    CLOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    dstSccReprocessingQ->release();

    CLOGD("DEBUG(%s[%d]): clear dstJpegReprocessingQ", __FUNCTION__, __LINE__);
    dstJpegReprocessingQ->release();
#else
    ExynosCameraFrame *frame = NULL;

    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    while(m_postPictureQ->getSizeOfProcessQ()) {
        m_postPictureQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }

    CLOGD("DEBUG(%s[%d]): clear dstSccReprocessingQ", __FUNCTION__, __LINE__);
    while(dstSccReprocessingQ->getSizeOfProcessQ()) {
        dstSccReprocessingQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif

    CLOGD("DEBUG(%s[%d]): reset buffer gsc buffers", __FUNCTION__, __LINE__);
    m_gscBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer jpeg buffers", __FUNCTION__, __LINE__);
    m_jpegBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer sccReprocessing buffers", __FUNCTION__, __LINE__);
    m_sccReprocessingBufferMgr->resetBuffers();
}

bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int retryCountGSC = 4;

    ExynosCameraFrame *newFrame = NULL;
    camera2_stream *shot_stream = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBuffer highResolutionCbBuffer;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_exynosCameraParameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    int pipeId_scc = 0;
    int pipeId_gsc = 0;

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount = getYuvPlaneCount(previewFormat);
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int buffer_idx = getShotBufferIdex();

    sccReprocessingBuffer.index = -2;
    highResolutionCbBuffer.index = -2;

    if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
        pipeId_scc = PIPE_3AA;
        pipeId_gsc = PIPE_GSC_PICTURE;
    } else {
        pipeId_scc = (isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        pipeId_gsc = PIPE_GSC_REPROCESSING;
    }

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        CLOGD("DEBUG(%s[%d]): High Resolution Callback Stop", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, cbPreviewW, cbPreviewH);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, cbPreviewW, cbPreviewH);
        return ret;
    }

    /* wait SCC */
    CLOGV("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    ret = m_highResolutionCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto CLEAN;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
        return ret;
    }
    CLOGV("INFO(%s[%d]):SCC output done", __FUNCTION__, __LINE__);

    if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        /* get GSC src buffer */
        if (m_exynosCameraParameters->isUsing3acForIspc() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream == NULL) {
            CLOGE("ERR(%s[%d]):shot_stream is NULL. buffer(%d)",
                    __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
            goto CLEAN;
        }

        /* alloc GSC buffer */
        if (m_highResolutionCallbackBufferMgr->isAllocated() == false) {
            ret = m_allocBuffers(m_highResolutionCallbackBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_highResolutionCallbackBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
                return ret;
            }
        }

        /* get GSC dst buffer */
        int bufIndex = -2;
        m_highResolutionCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &highResolutionCbBuffer);

        /* get preview callback heap */
        camera_memory_t *previewCallbackHeap = NULL;
        previewCallbackHeap = m_getMemoryCb(highResolutionCbBuffer.fd[0], highResolutionCbBuffer.size[0], 1, m_callbackCookie);
        if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, highResolutionCbBuffer.size[0]);
            goto CLEAN;
        }

        ret = m_setCallbackBufferInfo(&highResolutionCbBuffer, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }

        /* set src/dst rect */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];

        ret = m_calcHighResolutionPreviewGSCRect(&srcRect, &dstRect);
        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);

        CLOGV("DEBUG(%s[%d]):srcRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, srcRect.x, srcRect.y, srcRect.w, srcRect.h);
        CLOGV("DEBUG(%s[%d]):dstRect x : %d, y : %d, w : %d, h : %d", __FUNCTION__, __LINE__, dstRect.x, dstRect.y, dstRect.w, dstRect.h);

        ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, &highResolutionCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        /* push frame to GSC pipe */
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);
        m_pictureFrameFactory->setOutputFrameQToPipe(dstGscReprocessingQ, pipeId_gsc);

        /* wait GSC for high resolution preview callback */
        CLOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        while (retryCountGSC > 0) {
            ret = dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("WRN(%s)(%d):wait and pop timeout, ret(%d)", __FUNCTION__, __LINE__, ret);
                m_pictureFrameFactory->startThread(pipeId_gsc);
            } else if (ret < 0) {
                CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else {
                break;
            }
            retryCountGSC--;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto CLEAN;
        }
        CLOGI("INFO(%s[%d]):GSC output done", __FUNCTION__, __LINE__);

        /* put SCC buffer */
        if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_sccBufferMgr, sccReprocessingBuffer.index);
        } else {
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
        }

        CLOGV("DEBUG(%s[%d]):high resolution preview callback", __FUNCTION__, __LINE__);
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        /* put high resolution callback buffer */
        ret = m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        if (newFrame != NULL) {
            newFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    } else {
        CLOGD("DEBUG(%s[%d]): Preview callback message disabled, skip callback", __FUNCTION__, __LINE__);
        /* put SCC buffer */
        if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_sccBufferMgr, sccReprocessingBuffer.index);
        } else {
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
                goto CLEAN;
            }
            ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
        }
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_exynosCameraParameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread complete, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2)
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
    if (highResolutionCbBuffer.index != -2)
        m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        ret = m_deleteFrame(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_deleteFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
                m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_exynosCameraParameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread fail, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;
}

status_t ExynosCamera::m_doPrviewToRecordingFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer recordingBuf)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    CLOGV("DEBUG(%s[%d]):--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
        __FUNCTION__, __LINE__, previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrame  *newFrame = NULL;

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* csc and scaling */
    ret = m_calcRecordingGSCRect(&srcRect, &dstRect);
    ret = newFrame->setSrcRect(pipeId, srcRect);
    ret = newFrame->setDstRect(pipeId, dstRect);

    ret = m_setupEntity(pipeId, newFrame, &previewBuf, &recordingBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, ret);
        ret = INVALID_OPERATION;
        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
        goto func_exit;
    }
    m_recordingListLock.lock();
    m_recordingProcessList.push_back(newFrame);
    m_recordingListLock.unlock();
    m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

func_exit:

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;

}

bool ExynosCamera::m_recordingThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    int  pipeId = PIPE_GSC_VIDEO;
    nsecs_t timeStamp = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;

    CLOGV("INFO(%s[%d]):wait gsc done output", __FUNCTION__, __LINE__);
    ret = m_recordingQ->waitAndPopProcessQ(&frame);
    if (m_getRecordingEnabled() == false) {
        CLOGI("INFO(%s[%d]):recording stopped", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (ret < 0) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto func_exit;
    }
    CLOGV("INFO(%s[%d]):gsc done for recording callback", __FUNCTION__, __LINE__);

    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    if (buffer.index < 0 || buffer.index >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, m_recordingBufferCount, buffer.index);
        goto func_exit;
    }

    timeStamp = m_recordingTimeStamp[buffer.index];

    if (m_recordingStartTimeStamp == 0) {
        m_recordingStartTimeStamp = timeStamp;
        CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
    }

    if ((0L < timeStamp)
        && (m_lastRecordingTimeStamp < timeStamp)
        && (m_recordingStartTimeStamp <= timeStamp)) {
        if (m_getRecordingEnabled() == true
            && m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
#ifdef CHECK_MONOTONIC_TIMESTAMP
            CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld",
                __FUNCTION__, __LINE__, buffer.index, timeStamp);
#endif
#ifdef DEBUG
            CLOGD("DEBUG(%s[%d]): - lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                __FUNCTION__, __LINE__,
                m_lastRecordingTimeStamp,
                systemTime(SYSTEM_TIME_MONOTONIC),
                m_recordingStartTimeStamp);
#endif
            struct addrs *recordAddrs = NULL;

            recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
            recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
            recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
            recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
            recordAddrs[buffer.index].bufIndex    = buffer.index;

            m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    buffer.index,
                    m_callbackCookie);
            m_lastRecordingTimeStamp = timeStamp;
        }
    } else {
        CLOGW("WARN(%s[%d]):recordingFrameIndex=%d, timeStamp(%lld) invalid -"
            " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
            __FUNCTION__, __LINE__, buffer.index, timeStamp,
            m_lastRecordingTimeStamp,
            systemTime(SYSTEM_TIME_MONOTONIC),
            m_recordingStartTimeStamp);
        m_releaseRecordingBuffer(buffer.index);
    }

func_exit:

    m_recordingListLock.lock();
    if (frame != NULL) {
        ret = m_removeFrameFromList(&m_recordingProcessList, frame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
        frame = NULL;
    }
    m_recordingListLock.unlock();

    return m_recordingEnabled;
}

status_t ExynosCamera::m_releaseRecordingBuffer(int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufIndex < 0 || bufIndex >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, m_recordingBufferCount, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_recordingTimeStamp[bufIndex] = 0L;
    ret = m_putBuffers(m_recordingBufferMgr, bufIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcHighResolutionPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcRecordingGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_exynosCameraParameters->calcPictureRect(originW, originH, srcRect, dstRect);
}

status_t ExynosCamera::m_searchFrameFromList(List<ExynosCameraFrame *> *list, uint32_t frameCount, ExynosCameraFrame **frame)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    if (list->empty()) {
        CLOGD("DEBUG(%s[%d]):list is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        if (frameCount == curFrame->getFrameCount()) {
            CLOGV("DEBUG(%s):frame count match: expected(%d)", __FUNCTION__, frameCount);
            *frame = curFrame;
            return NO_ERROR;
        }
        r++;
    } while (r != list->end());

    CLOGV("DEBUG(%s[%d]):Cannot find match frame, frameCount(%d)", __FUNCTION__, __LINE__, frameCount);

    return NO_ERROR;
}

status_t ExynosCamera::m_removeFrameFromList(List<ExynosCameraFrame *> *list, ExynosCameraFrame *frame)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrame *>::iterator r;

    if (frame == NULL) {
        CLOGE("ERR(%s):frame is NULL", __FUNCTION__);
        return BAD_VALUE;
    }

    if (list->empty()) {
        CLOGD("DEBUG(%s):list is empty", __FUNCTION__);
        return NO_ERROR;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            CLOGV("DEBUG(%s):frame count match: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        CLOGW("WARN(%s):frame count mismatch: expected(%d), current(%d)", __FUNCTION__, frameCount, curFrameCount);
        /* removed message */
        /* curFrame->printEntity(); */
        r++;
    } while (r != list->end());

    CLOGE("ERR(%s):Cannot find match frame!!!", __FUNCTION__);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_deleteFrame(ExynosCameraFrame **frame)
{
    status_t ret = NO_ERROR;

    /* put lock using this frame */
    Mutex::Autolock lock(m_searchframeLock);

    if (*frame == NULL) {
        CLOGE("ERR(%s[%d]):frame == NULL. so, fail", __FUNCTION__, __LINE__);
        return BAD_VALUE;
    }

    if ((*frame)->getFrameLockState() == false) {
        if ((*frame)->isComplete() == true) {
            CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, (*frame)->getFrameCount());

            (*frame)->decRef();
            m_frameMgr->deleteFrame(*frame);
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(List<ExynosCameraFrame *> *list)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, list->size());

    while (!list->empty()) {
        r = list->begin()++;
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
            curFrame = NULL;
        }
        list->erase(r);
    }
    CLOGD("DEBUG(%s):EXIT ", __FUNCTION__);

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(frame_queue_t *queue)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;

    CLOGD("DEBUG(%s):remaining frame(%d), we remove them all", __FUNCTION__, queue->getSizeOfProcessQ());

    while (0 < queue->getSizeOfProcessQ()) {
        queue->popProcessQ(&curFrame);
        if (curFrame != NULL) {
            CLOGV("DEBUG(%s):remove frame count %d", __FUNCTION__, curFrame->getFrameCount() );
            curFrame->decRef();
            m_frameMgr->deleteFrame(curFrame);
            curFrame = NULL;
        }
    }
    CLOGD("DEBUG(%s):EXIT ", __FUNCTION__);

    return NO_ERROR;
}

status_t ExynosCamera::m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction) {
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer deleteSccBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int ret = NO_ERROR;

    if (frameQ == NULL) {
        CLOGE("ERR(%s[%d]):frameQ is NULL.", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    CLOGI("INFO(%s[%d]): IN... frameQSize(%d)", __FUNCTION__, __LINE__, frameQ->getSizeOfProcessQ());

    while (0 < frameQ->getSizeOfProcessQ()) {
        ret = frameQ->popProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            continue;
        }

        if (direction == SRC_BUFFER_DIRECTION)
            ret = newFrame->getSrcBuffer(pipeId, &deleteSccBuffer);
        else
            ret = newFrame->getDstBuffer(pipeId, &deleteSccBuffer, m_previewFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            continue;
        }

        ret = m_getBufferManager(pipeId, &bufferMgr, direction);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);

        /* put SCC buffer */
        CLOGD("DEBUG(%s)(%d):m_putBuffer by clearjpegthread(dstSccRe), index(%d)", __FUNCTION__, __LINE__, deleteSccBuffer.index);
        ret = m_putBuffers(bufferMgr, deleteSccBuffer.index);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
    }

    return ret;
}

status_t ExynosCamera::m_printFrameList(List<ExynosCameraFrame *> *list)
{
    int ret = 0;
    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;

    CLOGD("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    CLOGD("\t remaining frame count(%d)", list->size());

    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame != NULL) {
            CLOGI("\t hal frame count %d", curFrame->getFrameCount() );
            curFrame->printEntity();
        }

        r++;
    } while (r != list->end());
    CLOGD("----------------------------------------------------------------------------");

    return NO_ERROR;
}

status_t ExynosCamera::m_createIonAllocator(ExynosCameraIonAllocator **allocator)
{
    status_t ret = NO_ERROR;
    int retry = 0;
    do {
        retry++;
        CLOGI("INFO(%s[%d]):try(%d) to create IonAllocator", __FUNCTION__, __LINE__, retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        else {
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success (allocator=%p)", __FUNCTION__, __LINE__, *allocator);
            break;
        }
    } while (ret < 0 && retry < 3);

    if (ret < 0 && retry >=3) {
        CLOGE("ERR(%s[%d]):create IonAllocator fail (retryCount=%d)", __FUNCTION__, __LINE__, retry);
        ret = INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCamera::m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_ION_TYPE);
}

status_t ExynosCamera::m_createBufferManager(
        ExynosCameraBufferManager **bufferManager,
        const char *name,
        buffer_manager_type type)
{
    status_t ret = NO_ERROR;

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_createIonAllocator fail", __FUNCTION__, __LINE__);
        else
            CLOGD("DEBUG(%s[%d]):m_createIonAllocator success", __FUNCTION__, __LINE__);
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    CLOGD("DEBUG(%s):BufferManager(%s) created", __FUNCTION__, name);

    return ret;
}

bool ExynosCamera::m_releasebuffersForRealloc()
{
    status_t ret = NO_ERROR;
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGD("DEBUG(%s[%d]):m_setBuffers free all buffers", __FUNCTION__, __LINE__);
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    m_exynosCameraParameters->setReallocBuffer(false);

    if (m_exynosCameraParameters->getRestartPreview() == true) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

   return true;
}


status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwPictureW, hwPictureH;

    int previewMaxW, previewMaxH;
    int pictureMaxW, pictureMaxH;
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    ExynosRect bdsRect;

    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    if( m_exynosCameraParameters->getReallocBuffer() ) {
        /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
        m_releasebuffersForRealloc();
    }

    m_exynosCameraParameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    CLOGI("(%s):HW Preview width x height = %dx%d", __FUNCTION__, hwPreviewW, hwPreviewH);
    m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
    m_exynosCameraParameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("(%s):Picture MAX width x height = %dx%d", __FUNCTION__, pictureMaxW, pictureMaxH);
    if( m_exynosCameraParameters->getHighSpeedRecording() ) {
        m_exynosCameraParameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("(%s):HW Sensor(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getHwPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("(%s):HW Preview(HighSpeed) MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    } else {
        m_exynosCameraParameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("(%s):Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
        m_exynosCameraParameters->getMaxPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("(%s):Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
    }

#if (SUPPORT_BACK_HW_VDIS || SUPPORT_FRONT_HW_VDIS)
    /*
     * we cannot expect TPU on or not, when open() api.
     * so extract memory TPU size
     */
    int w = 0, h = 0;
    m_exynosCameraParameters->calcNormalToTpuSize(previewMaxW, previewMaxH, &w, &h);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Hw vdis buffer calulation fail src(%d x %d) dst(%d x %d)",__FUNCTION__, __LINE__, previewMaxW, previewMaxH, w, h);
    }
    previewMaxW = w;
    previewMaxH = h;
    CLOGI("(%s): TPU based Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
#endif

    m_exynosCameraParameters->getPreviewBdsSize(&bdsRect);

    /* FLITE */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = sensorMaxW * 2;
        planeSize[0] = sensorMaxW * sensorMaxH * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP(sensorMaxW , 10) * 8 / 5;
        planeSize[0]    = bytesPerLine[0] * sensorMaxH;
    }
#else
    planeSize[0] = sensorMaxW * sensorMaxH * 2;
#endif
    planeCount  = 2;

    /* TO DO : make num of buffers samely */
    maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_BACK) {
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
        m_bayerBufferMgr->setContigBufCount(RESERVED_NUM_BAYER_BUFFERS);
    } else {
        if (m_exynosCameraParameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            m_bayerBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_BAYER_BUFFERS);
        }
    }
#endif

#ifndef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->isUsing3acForIspc() == false
        || m_exynosCameraParameters->getDualMode() == true)
#endif
    {
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
    memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

    /* for preview */
    planeSize[0] = 32 * 64 * 2;
    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true)
        maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    if (m_exynosCameraParameters->isReprocessing() == true) {
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_exynosCameraParameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = previewMaxW * 2;
            planeSize[0] = previewMaxW * previewMaxH * 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
                planeSize[0] = previewMaxW * previewMaxH * 2;
            } else {
                bytesPerLine[0] = ROUND_UP((previewMaxW * 3 / 2), 16);
                planeSize[0]    = bytesPerLine[0] * previewMaxH;
            }
        }
#else
        /* planeSize[0] = width * height * 2; */
        planeSize[0] = previewMaxW * previewMaxH * 2;
#endif
    } else {
        /* Picture Max Size == Sensor Max Size - Sensor Margin */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_exynosCameraParameters->checkBayerDumpEnable()) {
            bytesPerLine[0] = pictureMaxW * 2;
            planeSize[0] = pictureMaxW * pictureMaxH * 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            bytesPerLine[0] = ROUND_UP((pictureMaxW * 3 / 2), 16);
            planeSize[0]    = bytesPerLine[0] * pictureMaxH;
        }
#else
        /* planeSize[0] = width * height * 2; */
        planeSize[0] = pictureMaxW * pictureMaxH * 2;
#endif
    }
    planeCount  = 2;
    /* TO DO : make num of buffers samely */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
#ifdef RESERVED_MEMORY_ENABLE
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
        m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS);
#endif
    } else {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;
#ifdef RESERVED_MEMORY_ENABLE
        if (m_exynosCameraParameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
        }
#endif
    }

    if (m_exynosCameraParameters->is3aaIspOtf() == false) {
        ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    /* HW VDIS memory */
    if (m_exynosCameraParameters->getTpuEnabledMode() == true) {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_hwdis_buffers;

        /* DIS MEMORY */
        int disFormat = m_exynosCameraParameters->getHWVdisFormat();
        unsigned int bpp = 0;
        unsigned int disPlanes = 0;

        getYuvFormatInfo(disFormat, &bpp, &disPlanes);

        switch (disFormat) {
        case V4L2_PIX_FMT_YUYV:
            planeSize[0] = bdsRect.w * bdsRect.h * 2;
            break;
        default:
            CLOGE("ERR(%s[%d]):unexpected VDIS format(%d). so, fail", __FUNCTION__, __LINE__, disFormat);
            return INVALID_OPERATION;
            break;
        }

        exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_hwDisBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_hwDisBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGD("DEBUG(%s[%d]):m_allocBuffers(m_hwDisBufferMgr): %d x %d, planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, bdsRect.w, bdsRect.h, planeCount, maxBufferCount);
    }

    /* SW VDIS memory */
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    planeSize[0] = hwPreviewW * hwPreviewH;
    planeSize[1] = hwPreviewW * hwPreviewH / 2;
    planeCount  = 3;
    if(m_exynosCameraParameters->increaseMaxBufferOfPreview()){
        maxBufferCount = m_exynosCameraParameters->getPreviewBufferCount();
    } else {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    bool needMmap = false;
    if (m_previewWindow == NULL)
        needMmap = true;

    ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

#ifdef USE_BUFFER_WITH_STRIDE
    int stride = m_scpBufferMgr->getBufStride();
    if (stride != hwPreviewW) {
        CLOGI("INFO(%s[%d]):hwPreviewW(%d), stride(%d)", __FUNCTION__, __LINE__, hwPreviewW, stride);
        if (stride == 0) {
            /* If the SCP buffer manager is not instance of GrallocExynosCameraBufferManager
               (In case of setPreviewWindow(null) is called), return value of setHwPreviewStride()
               will be zero. If this value is passed as SCP width to firmware, firmware will
               generate PABORT error. */
            CLOGW("WARN(%s[%d]):HACK: Invalid stride(%d). It will be replaced as hwPreviewW(%d) value.",
                __FUNCTION__, __LINE__, stride, hwPreviewW);
            stride = hwPreviewW;
        }
    }
#endif

    m_exynosCameraParameters->setHwPreviewStride(stride);

    if (m_exynosCameraParameters->isSccCapture() == true
        || m_exynosCameraParameters->isUsing3acForIspc() == true) {
        m_exynosCameraParameters->getHwPictureSize(&hwPictureW, &hwPictureH);
        CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, hwPictureW, hwPictureH);
        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN);
            planeSize[1] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) / 2;
            planeCount  = 3;
        } else if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            planeCount  = 2;
        } else {
        planeSize[0] = ALIGN_UP(hwPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(hwPictureH, GSCALER_IMG_ALIGN) * 2;
        planeCount  = 2;
        }
        /* TO DO : make same num of buffers */
        if (m_exynosCameraParameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

        if (m_exynosCameraParameters->isUsing3acForIspc() == true) {
            allocMode = BUFFER_MANAGER_ALLOCATION_SILENT;
            minBufferCount = 1;
        } else {
            allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            minBufferCount = maxBufferCount;
        }
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_sccBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_sccBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setReprocessingBuffer(void)
{
    int ret = 0;
    int pictureMaxW, pictureMaxH;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount  = 0;
    int bufferCount = 0;
    int minBufferCount = NUM_REPROCESSING_BUFFERS;
    int maxBufferCount = NUM_PICTURE_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_exynosCameraParameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("(%s):HW Picture MAX width x height = %dx%d", __FUNCTION__, pictureMaxW, pictureMaxH);

    /* for reprocessing */
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_exynosCameraParameters->checkBayerDumpEnable()) {
        bytesPerLine[0] = pictureMaxW * 2;
        planeSize[0] = pictureMaxW * pictureMaxH * 2;
    } else
#endif /* DEBUG_RAWDUMP */
    {
        bytesPerLine[0] = ROUND_UP((pictureMaxW * 3 / 2), 16);
        planeSize[0] = bytesPerLine[0] * pictureMaxH;
    }
#else
    planeSize[0] = pictureMaxW * pictureMaxH * 2;
#endif
    planeCount  = 2;
    bufferCount = NUM_REPROCESSING_BUFFERS;

    if (m_exynosCameraParameters->getHighResolutionCallbackMode() == true) {
        /* ISP Reprocessing Buffer realloc for high resolution callback */
        minBufferCount = 2;
    }

    ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
            __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setPreviewCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    if (m_previewCallbackBufferMgr == NULL) {
        CLOGE("ERR(%s[%d]): m_previewCallbackBufferMgr is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_previewCallbackBufferMgr->isAllocated() == true) {
        if (m_exynosCameraParameters->getRestartPreview() == true) {
            CLOGD("DEBUG(%s[%d]): preview size is changed, realloc buffer", __FUNCTION__, __LINE__);
            m_previewCallbackBufferMgr->deinit();
        } else {
            return NO_ERROR;
        }
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, previewW, previewH);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): BAD value, format(%x), size(%dx%d)",
            __FUNCTION__, __LINE__, previewFormat, previewW, previewH);
        return ret;
    }

    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, bufferCount, type, false, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    ret = m_setPictureBuffer();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setPictureBuffer failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

status_t ExynosCamera::m_setPictureBuffer(void)
{
    int ret = 0;
    unsigned int planeSize[3] = {0};
    unsigned int bytesPerLine[3] = {0};
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int planeCount = 0;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_exynosCameraParameters->getMaxPictureSize(&pictureW, &pictureH);
    pictureFormat = m_exynosCameraParameters->getPictureFormat();
    if ((needGSCForCapture(getCameraId()) == true)) {
        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = pictureW * pictureH;
            planeSize[1] = pictureW * pictureH / 2;
            planeCount = 2;
        } else if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21) {
            planeSize[0] = pictureW * pictureH * 3 / 2;
            planeCount = 1;
        }else {
            planeSize[0] = pictureW * pictureH * 2;
            planeCount = 1;
        }

        minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }
    }

    if( m_hdrEnabled == false ) {
        if (SCC_OUTPUT_COLOR_FMT == V4L2_PIX_FMT_NV21M) {
            planeSize[0] = pictureW * pictureH * 3 / 2;
        } else {
        planeSize[0] = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
        }
        planeCount = 1;
        minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        CLOGD("DEBUG(%s[%d]): jpegBuffer picture(%dx%d) size(%d)", __FUNCTION__, __LINE__, pictureW, pictureH, planeSize[0]);

        // Same with above GSC buffers
        if (m_exynosCameraParameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;

        ret = m_allocBuffers(m_jpegBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret < 0)
            CLOGE("ERR(%s:%d):jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, NUM_REPROCESSING_BUFFERS);
    }

    return ret;
}

status_t ExynosCamera::m_releaseBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }
    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
    }
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }
    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }
    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    CLOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    if (bufManager != NULL)
        bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);

    return NO_ERROR;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                reqBufCount,
                reqBufCount,
                EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
                BUFFER_MANAGER_ALLOCATION_ATONCE,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_allocBuffers(reqBufCount=%d) fail",
            __FUNCTION__, __LINE__, reqBufCount);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    ret = m_allocBuffers(
                bufManager,
                planeCount,
                planeSize,
                bytePerLine,
                minBufCount,
                maxBufCount,
                type,
                BUFFER_MANAGER_ALLOCATION_ONDEMAND,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
            __FUNCTION__, __LINE__, minBufCount, maxBufCount, type);
    }

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  minBufCount,
        int  maxBufCount,
        exynos_camera_buffer_type_t type,
        buffer_manager_allocation_mode_t allocMode,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;

    CLOGI("INFO(%s[%d]):setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, type=%d, allocMode=%d)",
        __FUNCTION__, __LINE__, planeCount, minBufCount, maxBufCount, (int)type, (int)allocMode);

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        0,
                        minBufCount,
                        maxBufCount,
                        type,
                        allocMode,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setInfo fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):alloc fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_checkThreadState(int *threadState, int *countRenew)
{
    int ret = NO_ERROR;

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        CLOGW("WRN(%s[%d]:SCP DQ Timeout! State:[%d], Duration:%d msec", __FUNCTION__, __LINE__, *threadState, (*countRenew)*(MONITOR_THREAD_INTERVAL/1000));
        ret = false;
    } else {
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

status_t ExynosCamera::m_checkThreadInterval(uint32_t pipeId, uint32_t pipeInterval, int *threadState)
{
    uint64_t *threadInterval;
    int ret = NO_ERROR;

    m_previewFrameFactory->getThreadInterval(&threadInterval, pipeId);
    if (*threadInterval > pipeInterval) {
        CLOGW("WRN(%s[%d]:Pipe(%d) Thread Interval [%lld msec], State:[%d]", __FUNCTION__, __LINE__, pipeId, (*threadInterval)/1000, *threadState);
        ret = false;
    } else {
        CLOGV("Thread IntervalTime [%lld]", *threadInterval);
        CLOGV("Thread Renew state [%d]", *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

bool ExynosCamera::m_monitorThreadFunc(void)
{
    CLOGV("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int *threadState;
    struct timeval dqTime;
    uint64_t *timeInterval;
    int *countRenew;
    int camId = getCameraId();
    int ret = NO_ERROR;
    int loopCount = 0;

    int dtpStatus = 0;
    int pipeIdFlite = 0;
    int pipeIdErrorCheck = 0;

    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL/20)) {
        if (m_flagThreadStop == true) {
            CLOGI("INFO(%s[%d]): m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);

            return false;
        }

        usleep(MONITOR_THREAD_INTERVAL/20);
    }

    if (m_previewFrameFactory == NULL) {
        CLOGW("WARN(%s[%d]): m_previewFrameFactory is NULL. Skip monitoring.", __FUNCTION__, __LINE__);

        return false;
    }

    pipeIdFlite = PIPE_FLITE;

    if (m_exynosCameraParameters->is3aaIspOtf() == true) {
        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            pipeIdErrorCheck = PIPE_DIS;
        else
            pipeIdErrorCheck = PIPE_3AA;
    } else {
        pipeIdErrorCheck = PIPE_ISP;
    }

#ifdef MONITOR_LOG_SYNC
    uint32_t pipeIdIsp = 0;

    /* define pipe for isp node cause of sync log sctrl */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true)
        pipeIdIsp = PIPE_3AA;
    else
        pipeIdIsp = PIPE_3AA;

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (m_previewFrameFactory->checkPipeThreadRunning(pipeIdIsp) &&
        !(getCameraId() == CAMERA_ID_FRONT && m_exynosCameraParameters->getDualMode())){
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("INFO(%s[%d]): @FIMC_IS_SYNC %d", __FUNCTION__, __LINE__, syncLogId);
            m_previewFrameFactory->syncLog(pipeIdIsp, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif

    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
        dump();

        /* in GED */
        m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
        return false;
    }

#ifdef SENSOR_OVERFLOW_CHECK
    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
        dump();

        /* in GED */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
        /* specifically defined */
        /* m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie); */
        /* or */
        android_printAssert(NULL, LOG_TAG, "killed by itself");

        return false;
    }
#endif

    m_previewFrameFactory->getThreadState(&threadState, pipeIdErrorCheck);
    m_previewFrameFactory->getThreadRenew(&countRenew, pipeIdErrorCheck);

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        CLOGD("DEBUG(%s[%d]):ESD Detected. threadState(%d) *countRenew(%d)", __FUNCTION__, __LINE__, *threadState, *countRenew);
        dump();

        /* in GED */
        /* skip error callback */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */

        return false;
    } else {
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
    }

#if 0
    m_checkThreadState(threadState, countRenew)?:ret = false;
    m_checkThreadInterval(PIPE_SCP, WARNING_SCP_THREAD_INTERVAL, threadState)?:ret = false;

    enum pipeline pipe;

    /* check PIPE_3AA thread state & interval */
    if (m_exynosCameraParameters->isFlite3aaOtf() == true) {
        pipe = PIPE_3AA_ISP;

        m_previewFrameFactory->getThreadRenew(&countRenew, pipe);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }
    } else {
        pipe = PIPE_3AA;

        m_previewFrameFactory->getThreadRenew(&countRenew, pipe);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            dump();

            /* in GED */
            m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
            /* specifically defined */
            /* m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie); */
            /* or */
            android_printAssert(NULL, LOG_TAG, "killed by itself");
        }
    }

    m_checkThreadInterval(pipe, WARNING_3AA_THREAD_INTERVAL, threadState)?:ret = false;

    if (m_callbackState == 0) {
        m_callbackStateOld = 0;
        m_callbackState = 0;
        m_callbackMonitorCount = 0;
    } else {
        if (m_callbackStateOld != m_callbackState) {
            m_callbackStateOld = m_callbackState;
            CLOGD("INFO(%s[%d]):callback state is updated (0x%x)", __FUNCTION__, __LINE__, m_callbackStateOld);
        } else {
            if ((m_callbackStateOld & m_callbackState) != 0)
                CLOGE("ERR(%s[%d]):callback is blocked (0x%x), Duration:%d msec", __FUNCTION__, __LINE__, m_callbackState, m_callbackMonitorCount*(MONITOR_THREAD_INTERVAL/1000));
        }
    }
#endif

    gettimeofday(&dqTime, NULL);
    m_previewFrameFactory->getThreadInterval(&timeInterval, pipeIdErrorCheck);

    CLOGV("Thread IntervalTime [%lld]", *timeInterval);
    CLOGV("Thread Renew Count [%d]", *countRenew);

    m_previewFrameFactory->incThreadRenew(pipeIdErrorCheck);

    return true;
}

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

bool ExynosCamera::m_autoFocusResetNotify(int focusMode)
{
    /* show restart */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, 4, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, 4, 0, m_callbackCookie);

    /* show focusing */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, 3, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, 3, 0, m_callbackCookie);

    return true;
}

bool ExynosCamera::m_autoFocusThreadFunc(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    bool afResult = false;
    int focusMode = 0;

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */

    if (getCameraId() == CAMERA_ID_FRONT) {
        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {
            if (m_notifyCb != NULL) {
                CLOGD("DEBUG(%s):Do not support autoFocus in front camera.", __FUNCTION__);
                m_notifyCb(CAMERA_MSG_FOCUS, true, 0, m_callbackCookie);
            } else {
                CLOGD("DEBUG(%s):m_notifyCb is NULL!", __FUNCTION__);
            }
        } else {
            CLOGD("DEBUG(%s):autoFocus msg disabled !!", __FUNCTION__);
        }
        return false;
    }

    if (m_autoFocusType == AUTO_FOCUS_SERVICE) {
        focusMode = m_exynosCameraParameters->getFocusMode();
    } else if (m_autoFocusType == AUTO_FOCUS_HAL) {
        focusMode = FOCUS_MODE_AUTO;

        m_autoFocusResetNotify(focusMode);
    }

    /* check early exit request */
    if (m_exitAutoFocusThread == true) {
        CLOGD("DEBUG(%s):exiting on request", __FUNCTION__);
        goto done;
    }

    m_autoFocusLock.lock();
    m_autoFocusRunning = true;

    if (m_autoFocusRunning == true) {
        afResult = m_exynosCameraActivityControl->autoFocus(focusMode, m_autoFocusType);
        if (afResult == true)
            CLOGV("DEBUG(%s):autoFocus Success!!", __FUNCTION__);
        else
            CLOGV("DEBUG(%s):autoFocus Fail !!", __FUNCTION__);
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled !!", __FUNCTION__);
    }

    /*
     * CAMERA_MSG_FOCUS only takes a bool.  true for
     * finished and false for failure.
     * If cancelAutofocus() called, no callback.
     */
    if ((m_autoFocusRunning == true) &&
        m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {

        if (m_notifyCb != NULL) {
            int afFinalResult = (int)afResult;

            /* if inactive detected, tell it */
            if (focusMode == FOCUS_MODE_CONTINUOUS_PICTURE) {
                if (m_exynosCameraActivityControl->getCAFResult() == 2) {
                    afFinalResult = 2;
                }
            }

            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __FUNCTION__, afFinalResult, focusMode);
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
        }  else {
            CLOGD("DEBUG(%s):m_notifyCb is NULL mode(%d)", __FUNCTION__, focusMode);
        }
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled, no callback !!", __FUNCTION__);
    }

    m_autoFocusRunning = false;

    CLOGV("DEBUG(%s):exiting with no error", __FUNCTION__);

done:
    m_autoFocusLock.unlock();

    CLOGI("DEBUG(%s):end", __FUNCTION__);

    return false;
}

bool ExynosCamera::m_autoFocusContinousThreadFunc(void)
{
    int ret = 0;
    int index = 0;
    uint32_t frameCnt = 0;
    uint32_t count = 0;


    ret = m_autoFocusContinousQ.waitAndPopProcessQ(&frameCnt);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    count = m_autoFocusContinousQ.getSizeOfProcessQ();
    if( count >= MAX_FOCUSCONTINUS_THREADQ_SIZE ) {
        for( uint32_t i = 0 ; i < count ; i++) {
            m_autoFocusContinousQ.popProcessQ(&frameCnt);
        }
        CLOGD("DEBUG(%s[%d]):m_autoFocusContinousQ  skipped QSize(%d) frame(%d)", __FUNCTION__, __LINE__,  count, frameCnt);
    }

    /* Continuous Auto-focus */
    if (m_exynosCameraParameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE) {
        int afstatus = 0;
        static int afResult = 1;
        int prev_afstatus = afResult;
        afstatus = m_exynosCameraActivityControl->getCAFResult();
        afResult = afstatus;

        if (afstatus == 3 && (prev_afstatus == 0 || prev_afstatus == 1)) {
            afResult = 4;
        }

        if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_FOCUS)
            && (prev_afstatus != afstatus)) {
            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)",
                __FUNCTION__, afResult, m_exynosCameraParameters->getFocusMode());
            m_notifyCb(CAMERA_MSG_FOCUS, afResult, 0, m_callbackCookie);
        }
    }

    return true;
}

status_t ExynosCamera::dump(int fd) const
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::dump()
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    m_printExynosCameraInfo(__FUNCTION__);

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->dump();

    if (m_bayerBufferMgr != NULL)
        m_bayerBufferMgr->dump();
    if (m_3aaBufferMgr != NULL)
        m_3aaBufferMgr->dump();
    if (m_ispBufferMgr != NULL)
        m_ispBufferMgr->dump();
    if (m_hwDisBufferMgr != NULL)
        m_hwDisBufferMgr->dump();
    if (m_scpBufferMgr != NULL)
        m_scpBufferMgr->dump();

    if (m_ispReprocessingBufferMgr != NULL)
        m_ispReprocessingBufferMgr->dump();
    if (m_sccReprocessingBufferMgr != NULL)
        m_sccReprocessingBufferMgr->dump();
    if (m_sccBufferMgr != NULL)
        m_sccBufferMgr->dump();
    if (m_gscBufferMgr != NULL)
        m_gscBufferMgr->dump();

    return;
}

status_t ExynosCamera::m_getBufferManager(uint32_t pipeId, ExynosCameraBufferManager **bufMgr, uint32_t direction)
{
    status_t ret = NO_ERROR;
    ExynosCameraBufferManager **bufMgrList[2] = {NULL};
    *bufMgr = NULL;
    int internalPipeId  = pipeId;

    /*
     * front / back is different up to scenario(3AA OTF/M2M, etc)
     * so, we don't need to distinguish front / back camera.
     * but. reprocessing must handle the separated operation
     */
    if (pipeId < PIPE_FLITE_REPROCESSING)
        internalPipeId = INDEX(pipeId);

    switch (internalPipeId) {
    case PIPE_FLITE:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_bayerBufferMgr;
        break;
    case PIPE_3AA_ISP:
        bufMgrList[0] = &m_3aaBufferMgr;
        bufMgrList[1] = &m_ispBufferMgr;
        break;
    case PIPE_3AC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_bayerBufferMgr;
        break;
    case PIPE_3AA:
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT) {
            bufMgrList[0] = &m_bayerBufferMgr;
            bufMgrList[1] = &m_sccBufferMgr;
        } else {
            bufMgrList[0] = &m_3aaBufferMgr;
            if (m_exynosCameraParameters->isUsing3acForIspc() == true)
                bufMgrList[1] = &m_sccBufferMgr;
            else
                bufMgrList[1] = &m_ispBufferMgr;
        }
        break;
    case PIPE_ISP:
        bufMgrList[0] = &m_ispBufferMgr;

        if (m_exynosCameraParameters->getTpuEnabledMode() == true)
            bufMgrList[1] = &m_hwDisBufferMgr;
        else
            bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_DIS:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_ISPC:
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_GSC:
        if (m_exynosCameraParameters->getDualMode() == true && getCameraId() == CAMERA_ID_FRONT)
            bufMgrList[0] = &m_sccBufferMgr;
        else
            bufMgrList[0] = &m_scpBufferMgr;
       bufMgrList[1] = &m_scpBufferMgr;
       break;
    case PIPE_GSC_PICTURE:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_3AA_REPROCESSING:
        bufMgrList[0] = &m_bayerBufferMgr;
        if (m_exynosCameraParameters->getDualMode() == false)
            bufMgrList[1] = &m_ispReprocessingBufferMgr;
        else
            bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_ISP_REPROCESSING:
        bufMgrList[0] = &m_ispReprocessingBufferMgr;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_ISPC_REPROCESSING:
    case PIPE_SCC_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING:
        bufMgrList[0] = &m_sccReprocessingBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    default:
        CLOGE("ERR(%s[%d]): Unknown pipeId(%d)", __FUNCTION__, __LINE__, pipeId);
        bufMgrList[0] = NULL;
        bufMgrList[1] = NULL;
        ret = BAD_VALUE;
        break;
    }

    if (bufMgrList[direction] != NULL)
        *bufMgr = *bufMgrList[direction];

    return ret;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_exynosCameraParameters->getUsePureBayerReprocessing() == true) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }
#ifdef DEBUG_RAWDUMP
        pipeId = PIPE_FLITE;
#endif
    return pipeId;
}

void ExynosCamera::m_debugFpsCheck(uint32_t pipeId)
{
#ifdef FPS_CHECK
    uint32_t id = pipeId % DEBUG_MAX_PIPE_NUM;

    m_debugFpsCount[id]++;
    if (m_debugFpsCount[id] == 1) {
        m_debugFpsTimer[id].start();
    }
    if (m_debugFpsCount[id] == 30) {
        m_debugFpsTimer[id].stop();
        long long durationTime = m_debugFpsTimer[id].durationMsecs();
        CLOGI("DEBUG: FPS_CHECK(id:%d), duration %lld / 30 = %lld ms", pipeId, durationTime, durationTime / 30);
        m_debugFpsCount[id] = 0;
    }
#endif
}

status_t ExynosCamera::m_convertingStreamToShotExt(ExynosCameraBuffer *buffer, struct camera2_node_output *outputInfo)
{
/* TODO: HACK: Will be removed, this is driver's job */
    status_t ret = NO_ERROR;
    int bayerFrameCount = 0;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;

    shot_stream = (struct camera2_stream *)buffer->addr[1];
    bayerFrameCount = shot_stream->fcount;
    outputInfo->cropRegion[0] = shot_stream->output_crop_region[0];
    outputInfo->cropRegion[1] = shot_stream->output_crop_region[1];
    outputInfo->cropRegion[2] = shot_stream->output_crop_region[2];
    outputInfo->cropRegion[3] = shot_stream->output_crop_region[3];

    memset(buffer->addr[1], 0x0, sizeof(struct camera2_shot_ext));

    shot_ext = (struct camera2_shot_ext *)buffer->addr[1];
    shot_ext->shot.dm.request.frameCount = bayerFrameCount;

    return ret;
}

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, camera2_shot_ext *updateDmShot)
{
    status_t ret = NO_ERROR;
    bool isSrc = false;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    ExynosCameraFrame *inListFrame = NULL;

    m_captureSelector->setWaitTime(200000000);
    ExynosCameraFrame *bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    if (pipeId == PIPE_3AA) {
        ret = bayerFrame->getDstBuffer(pipeId, buffer, m_previewFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            goto CLEAN;
        }
    } else if (pipeId == PIPE_FLITE) {
        ret = bayerFrame->getDstBuffer(pipeId, buffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            goto CLEAN;
        }
    }

    if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        shot_ext = (struct camera2_shot_ext *)buffer->addr[1];
        CLOGD("DEBUG(%s[%d]): Selected frame count(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount);
    } else if (m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_exynosCameraParameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        if (updateDmShot == NULL) {
            CLOGE("ERR(%s[%d]): updateDmShot is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        retryCount = 12; /* 80ms * 12 */
        while(retryCount > 0) {
            if(bayerFrame->getMetaDataEnable() == false) {
                CLOGD("DEBUG(%s[%d]): Waiting for update jpeg metadata failed (%d), retryCount(%d)", __FUNCTION__, __LINE__, ret, retryCount);
            } else {
                break;
            }
            retryCount--;
            usleep(DM_WAITING_TIME);
        }

        /* update meta like pure bayer */
        bayerFrame->getUserDynamicMeta(updateDmShot);
        bayerFrame->getDynamicMeta(updateDmShot);

        shot_stream = (struct camera2_stream *)buffer->addr[1];
        CLOGD("DEBUG(%s[%d]): Selected fcount(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), shot_stream->fcount);
    } else {
        CLOGE("ERR(%s[%d]): reprocessing is not valid pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
        goto CLEAN;
    }

CLEAN:

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        } else {
            CLOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete", __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            bayerFrame->decRef();
            m_frameMgr->deleteFrame(bayerFrame);
            bayerFrame = NULL;
        }
    }

    return ret;
}

status_t ExynosCamera::m_checkBufferAvailable(uint32_t pipeId, ExynosCameraBufferManager *bufferMgr)
{
    status_t ret = TIMED_OUT;
    int retry = 0;

    do {
        ret = -1;
        retry++;
        if (bufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = OK;
        } else {
            /* wait available ISP buffer */
            usleep(WAITING_TIME);
        }
        if (retry % 10 == 0)
            CLOGW("WRAN(%s[%d]):retry(%d) setupEntity for pipeId(%d)", __FUNCTION__, __LINE__, retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME));

    return ret;
}

status_t ExynosCamera::m_boostDynamicCapture(void)
{
    status_t ret = NO_ERROR;
#if 0 /* TODO: need to implementation for bayer */
    uint32_t pipeId = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;
    uint32_t size = m_processList.size();

    ExynosCameraFrame *curFrame = NULL;
    List<ExynosCameraFrame *>::iterator r;
    camera2_node_group node_group_info_isp;

    if (m_processList.empty()) {
        CLOGD("DEBUG(%s[%d]):m_processList is empty", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }
    CLOGD("DEBUG(%s[%d]):m_processList size(%d)", __FUNCTION__, __LINE__, m_processList.size());
    r = m_processList.end();

    for (unsigned int i = 0; i < 3; i++) {
        r--;
        if (r == m_processList.begin())
            break;

    }

    curFrame = *r;
    if (curFrame == NULL) {
        CLOGE("ERR(%s):curFrame is empty", __FUNCTION__);
        return INVALID_OPERATION;
    }

    if (curFrame->getRequest(pipeId) == true) {
        CLOGD("DEBUG(%s[%d]): Boosting dynamic capture is not need", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    CLOGI("INFO(%s[%d]): boosting dynamic capture (frameCount: %d)", __FUNCTION__, __LINE__, curFrame->getFrameCount());
    /* For ISP */
    curFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    m_updateBoostDynamicCaptureSize(&node_group_info_isp);
    curFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

    curFrame->setRequest(pipeId, true);
    curFrame->setNumRequestPipe(curFrame->getNumRequestPipe() + 1);

    ret = curFrame->setEntityState(pipeId, ENTITY_STATE_REWORK);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_REWORK, ret);
        return ret;
    }

    m_previewFrameFactory->pushFrameToPipe(&curFrame, pipeId);
    m_dynamicSccCount++;
    CLOGV("DEBUG(%s[%d]): dynamicSccCount inc(%d) frameCount(%d)", __FUNCTION__, __LINE__, m_dynamicSccCount, curFrame->getFrameCount());
#endif

    return ret;
}

void ExynosCamera::m_updateBoostDynamicCaptureSize(camera2_node_group *node_group_info)
{
#if 0 /* TODO: need to implementation for bayer */
    ExynosRect sensorSize;
    ExynosRect bayerCropSize;

    node_group_info->capture[PERFRAME_BACK_SCC_POS].request = 1;

    m_exynosCameraParameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);

    node_group_info->leader.input.cropRegion[0] = bayerCropSize.x;
    node_group_info->leader.input.cropRegion[1] = bayerCropSize.y;
    node_group_info->leader.input.cropRegion[2] = bayerCropSize.w;
    node_group_info->leader.input.cropRegion[3] = bayerCropSize.h;
    node_group_info->leader.output.cropRegion[0] = 0;
    node_group_info->leader.output.cropRegion[1] = 0;
    node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
    node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];

    /* Capture 0 : SCC - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[0] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[1] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCC_POS].output.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_SCC_POS].input.cropRegion[3];

    /* Capture 1 : SCP - [scaling] */
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[0] = node_group_info->leader.output.cropRegion[0];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[1] = node_group_info->leader.output.cropRegion[1];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
    node_group_info->capture[PERFRAME_BACK_SCP_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

#endif
    return;
}

void ExynosCamera::m_checkFpsAndUpdatePipeWaitTime(void)
{
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
    frame_queue_t *inputFrameQ = NULL;

    m_exynosCameraParameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_curMinFps != curMinFps) {
        CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);

        enum pipeline pipe = (isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;

        m_previewFrameFactory->getInputFrameQToPipe(&inputFrameQ, pipe);

        /* 100ms * (30 / 15 fps) = 200ms */
        /* 100ms * (30 / 30 fps) = 100ms */
        /* 100ms * (30 / 10 fps) = 300ms */
        if (inputFrameQ != NULL && curMinFps != 0)
            inputFrameQ->setWaitTime(((100000000 / curMinFps) * 30));
    }

    m_curMinFps = curMinFps;

    return;
}

/* vision */
status_t ExynosCamera::m_startVisionInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    uint32_t minFrameNum = FRONT_NUM_BAYER_BUFFERS;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    ret = m_visionFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_visionFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    m_visionFps = 10;
    m_visionAe = 0x2A;

    ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, m_visionFps, PIPE_FLITE);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    CLOGD("DEBUG(%s[%d]):m_visionFps(%d)", __FUNCTION__, __LINE__, m_visionFps);

    ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_AE_TARGET, m_visionAe, PIPE_FLITE);
    if (ret < 0)
        CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    CLOGD("DEBUG(%s[%d]):m_visionAe(%d)", __FUNCTION__, __LINE__, m_visionAe);

    for (uint32_t i = 0; i < minFrameNum; i++) {
        ret = generateFrameVision(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        m_fliteFrameCount++;

        m_setupEntity(PIPE_FLITE, newFrame);
        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
    }

    /* prepare pipes */
    ret = m_visionFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("ERR(%s):preparePipe fail", __FUNCTION__);
        return ret;
    }

    /* stream on pipes */
    ret = m_visionFrameFactory->startPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):startPipe fail", __FUNCTION__);
        return ret;
    }

    /* start all thread */
    ret = m_visionFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        return ret;
    }

    m_previewEnabled = true;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopVisionInternal(void)
{
    int ret = 0;

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    ret = m_visionFrameFactory->stopPipes();
    if (ret < 0) {
        CLOGE("ERR(%s):stopPipe fail", __FUNCTION__);
        return ret;
    }

    CLOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_clearList(m_pipeFrameVisionDoneQ);

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;

    m_previewEnabled = false;
    m_exynosCameraParameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    *newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    if (*newFrame == NULL) {
        *newFrame = m_visionFrameFactory->createNewFrame();

        if (*newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            return UNKNOWN_ERROR;
        }

        m_processList.push_back(*newFrame);
    }

    return ret;
}

status_t ExynosCamera::m_setVisionBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwSensorW, hwSensorH;
    int hwPictureW, hwPictureH;

    int previewMaxW, previewMaxH;
    int sensorMaxW, sensorMaxH;

    int planeCount  = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;

    maxBufferCount = FRONT_NUM_BAYER_BUFFERS;
    planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    planeCount  = 2;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    /* ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false); */
    ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVisionCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_exynosCameraParameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_exynosCameraParameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = FRONT_NUM_BAYER_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    planeCount = 1;
    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine, bufferCount, bufferCount, type, false, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, bufferCount);
        return ret;
    }

    return NO_ERROR;
}


bool ExynosCamera::m_visionThreadFunc(void)
{
    int ret = 0;
    int index = 0;

    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *handleFrame = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t fvalid = 0;
    uint32_t fcount = 0;
    struct camera2_stream *shot_stream = NULL;
    size_t callbackBufSize;
    status_t statusRet = NO_ERROR;
    int fps = 0;
    int ae = 0;
    int internalValue = 0;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        return false;
    }

    ret = m_pipeFrameVisionDoneQ->waitAndPopProcessQ(&handleFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (handleFrame == NULL) {
        CLOGE("ERR(%s[%d]):handleFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

    /* handle vision frame */
    entity = handleFrame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL", __FUNCTION__, __LINE__);
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_FLITE:
        ret = handleFrame->getDstBuffer(entity->getPipeId(), &bayerBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

#ifdef VISION_DUMP
    char filePath[50];
    snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Camera/vision%02d.raw", dumpIndex);
    CLOGD("vision dump %s", filePath);
    dumpToFile(filePath, (char *)bayerBuffer.addr[0], VISION_WIDTH * VISION_HEIGHT);

    dumpIndex ++;
    if (dumpIndex > 4)
        dumpIndex = 0;
#endif

        m_exynosCameraParameters->getFrameSkipCount(&frameSkipCount);

        if (frameSkipCount > 0) {
            CLOGD("INFO(%s[%d]):frameSkipCount(%d)", __FUNCTION__, __LINE__, frameSkipCount);
        } else {
            /* callback */
            if (m_exynosCameraParameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
                ExynosCameraBuffer previewCbBuffer;
                camera_memory_t *previewCallbackHeap = NULL;
                char *srcAddr = NULL;
                char *dstAddr = NULL;
                int bufIndex = -2;

                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                previewCallbackHeap = m_getMemoryCb(previewCbBuffer.fd[0], previewCbBuffer.size[0], 1, m_callbackCookie);
                if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb(fd:%d, size:%d) fail", __FUNCTION__, __LINE__, previewCbBuffer.fd[0], previewCbBuffer.size[0]);

                    return INVALID_OPERATION;
                }

                for (int plane = 0; plane < 1; plane++) {
                    srcAddr = bayerBuffer.addr[plane];
                    dstAddr = (char *)previewCallbackHeap->data;
                    memcpy(dstAddr, srcAddr, previewCbBuffer.size[plane]);
                }
#if 0
                // Getting the buffer size for current preview size.
                callbackBufSize = m_getCurrentPreviewSizeBytes();
                if (callbackBufSize <= 0) {
                    CLOGE("ERR(%s[%d]): m_getCurrentPreviewSizeBytes fail, ret(%d)", __FUNCTION__, __LINE__, callbackBufSize);
                    statusRet = INVALID_OPERATION;
//                    goto done;
                }

                // Comparing newely updated preview buffer size against previewCallbackHeap.
                // If size is different, it means that the preview size has changed
                // during the memory transfer or GSC operations. So we have to drop this
                // preview callback buffer to prevent malfunctioning of user application
                if (previewCallbackHeap->size != callbackBufSize) {
                    CLOGW("WARN(%s[%d]): Preview size changed during operation. " \
                          "Initial=[%d], Current=[%d]. Current preview frame[%d] will be dropped."
                          , __FUNCTION__, __LINE__, previewCallbackHeap->size, callbackBufSize, handleFrame->getFrameCount());
//                    goto done;
                }
#endif
                setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);

                previewCallbackHeap->release(previewCallbackHeap);

                m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            }
        }

        ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);

        ret = generateFrameVision(m_fliteFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        ret = m_setupEntity(PIPE_FLITE, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
        m_fliteFrameCount++;

        break;
    default:
        break;
    }

    ret = handleFrame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    CLOGV("DEBUG(%s[%d]):frame complete, count(%d)", __FUNCTION__, __LINE__, handleFrame->getFrameCount());
    ret = m_removeFrameFromList(&m_processList, handleFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    handleFrame->decRef();
    m_frameMgr->deleteFrame(handleFrame);
    handleFrame = NULL;

#if 1
        fps = m_exynosCameraParameters->getVisionModeFps();
        if (m_visionFps != fps) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, fps, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionFps = fps;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionFps, fps);
        }
#if 0
        if (0 < m_visionAeTarget) {
            if (m_visionAeTarget != m_exynosVision->getAeTarget()) {
                if (m_exynosVision->setAeTarget(m_visionAeTarget) == false) {
                    CLOGE("ERR(%s): Fail to setAeTarget(%d)", __func__, m_visionAeTarget);

                    goto err;
                }
            }
        }
#endif

        ae = m_exynosCameraParameters->getVisionModeAeTarget();
        if (ae != m_visionAe) {
            switch(ae) {
            case 1:
                internalValue = 0x2A;
                break;
            case 2:
                internalValue = 0x8A;
                break;
            default:
                internalValue = 0x2A;
                break;
            }

            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_AE_TARGET, internalValue, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionAe = ae;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionAe, internalValue);
        }
#endif



    return true;
}

void ExynosCamera::m_printExynosCameraInfo(const char *funcName)
{
    int w = 0;
    int h = 0;
    ExynosRect srcRect, dstRect;

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):============= ExynosCameraInfo call by %s", __FUNCTION__, __LINE__, funcName);
    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);

    CLOGD("DEBUG(%s[%d]):============= Scenario ============================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= getCameraId                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getCameraId());
    CLOGD("DEBUG(%s[%d]):= getDualMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getDualMode());
    CLOGD("DEBUG(%s[%d]):= getScalableSensorMode       : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getScalableSensorMode());
    CLOGD("DEBUG(%s[%d]):= getRecordingHint            : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getDualRecordingHint        : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getDualRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getAdaptiveCSCRecording     : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getAdaptiveCSCRecording());
    CLOGD("DEBUG(%s[%d]):= doCscRecording              : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->doCscRecording());
    CLOGD("DEBUG(%s[%d]):= needGSCForCapture           : %d", __FUNCTION__, __LINE__, needGSCForCapture(getCameraId()));
    CLOGD("DEBUG(%s[%d]):= getShotMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getShotMode());
    CLOGD("DEBUG(%s[%d]):= getTpuEnabledMode           : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getTpuEnabledMode());
    CLOGD("DEBUG(%s[%d]):= getHWVdisMode               : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getHWVdisMode());
    CLOGD("DEBUG(%s[%d]):= get3dnrMode                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->get3dnrMode());

    CLOGD("DEBUG(%s[%d]):============= Internal setting ====================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= isFlite3aaOtf               : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isFlite3aaOtf());
    CLOGD("DEBUG(%s[%d]):= is3aaIspOtf                 : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->is3aaIspOtf());
    CLOGD("DEBUG(%s[%d]):= isReprocessing              : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isReprocessing());
    CLOGD("DEBUG(%s[%d]):= isReprocessing3aaIspOTF     : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isReprocessing3aaIspOTF());
    CLOGD("DEBUG(%s[%d]):= getUsePureBayerReprocessing : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->getUsePureBayerReprocessing());

    int reprocessingBayerMode = m_exynosCameraParameters->getReprocessingBayerMode();
    switch(reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_DYNAMIC", __FUNCTION__, __LINE__);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC", __FUNCTION__, __LINE__);
        break;
    default:
        CLOGD("DEBUG(%s[%d]):= getReprocessingBayerMode    : unexpected mode %d", __FUNCTION__, __LINE__, reprocessingBayerMode);
        break;
    }

    CLOGD("DEBUG(%s[%d]):= isSccCapture                : %d", __FUNCTION__, __LINE__, m_exynosCameraParameters->isSccCapture());

    CLOGD("DEBUG(%s[%d]):============= size setting =======================", __FUNCTION__, __LINE__);
    m_exynosCameraParameters->getMaxSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getMaxSensorSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getHwSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwSensorSize             : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getBnsSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getBnsSize                  : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPreviewBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getPreviewBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getHwPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPreviewSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPreviewSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPictureBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getPictureBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_exynosCameraParameters->getHwPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPictureSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_exynosCameraParameters->getPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPictureSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
}

}; /* namespace android */
