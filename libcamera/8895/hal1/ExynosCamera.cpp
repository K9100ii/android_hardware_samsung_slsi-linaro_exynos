/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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

namespace android {

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif

ExynosCamera::ExynosCamera(int cameraId, __unused camera_device_t *dev)
{
    BUILD_DATE();

    checkAndroidVersion();

    m_cameraOriginId = cameraId;
    m_cameraId = m_cameraOriginId;
    m_scenario = SCENARIO_NORMAL;

    m_dev = dev;

    initialize();
}

void ExynosCamera::initialize()
{
    CLOGI(" -IN-");
    int ret = 0;

    ExynosCameraActivityUCTL *uctlMgr = NULL;
    memset(m_name, 0x00, sizeof(m_name));
    m_use_companion = false;
    m_cameraSensorId = getSensorId(m_cameraOriginId);

    m_vendorSpecificPreConstructor(m_cameraId, m_dev);

    /* Create Main Threads */
    m_createThreads();

    m_parameters = new ExynosCamera1Parameters(m_cameraId, m_use_companion);
    CLOGD("Parameters(Id=%d) created", m_cameraId);

    m_exynosCameraActivityControl = m_parameters->getActivityControl();

    /* Frame factories */
    m_previewFrameFactory      = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory= NULL;

    /* Memory allocator */
    m_ionAllocator = NULL;
    m_grAllocator  = NULL;
    m_mhbAllocator = NULL;

    /* BufferManager */
#ifdef DEBUG_RAWDUMP
    m_fliteBufferMgr = NULL;
#endif

    /* Frame manager */
    m_frameMgr = NULL;

    /* Main Buffer */
    /* Bayer buffer: Output buffer of Flite or 3AC */
    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
#if defined(DEBUG_RAWDUMP)
    /* Flite buffer: Output buffer of Flite for DEBUG_RAWDUMP on Single chain */
    if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER)
        m_createInternalBufferManager(&m_fliteBufferMgr, "FLITE_BUF");
#endif

    /* 3AA buffer: Input buffer of 3AS */
    m_createInternalBufferManager(&m_3aaBufferMgr, "3AS_IN_BUF");
    /* ISP buffer: Hand-shaking buffer of 3AA-ISP */
    m_createInternalBufferManager(&m_ispBufferMgr, "3AP_OUT_BUF");
    /* HWDIS buffer: Hand-shaking buffer of ISP-TPU or ISP-MCSC */
    m_createInternalBufferManager(&m_hwDisBufferMgr, "ISP_OUT_BUF");
#ifdef SUPPORT_DEPTH_MAP
    m_createInternalBufferManager(&m_depthMapBufferMgr, "DEPTH_MAP_BUF");
#endif

    /* Reprocessing Buffer */
    /* ISP-Re buffer: Hand-shaking buffer of reprocessing 3AA-ISP */
    m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    /* SCC-Re buffer: Output buffer of reprocessing ISP(MCSC) */
    m_createInternalBufferManager(&m_yuvReprocessingBufferMgr, "MCSC_RE_BUF");
    /* Thumbnail buffer: Output buffer of reprocessing ISP(MCSC) */
    m_createInternalBufferManager(&m_thumbnailBufferMgr, "THUMBNAIL_BUF");

    /* Capture Buffer */
    /* SCC buffer: Output buffer of preview ISP(MCSC) */
    m_createInternalBufferManager(&m_yuvBufferMgr, "YUV_BUF");
    /* GSC buffer: Output buffer of external scaler */
    m_createInternalBufferManager(&m_gscBufferMgr, "GSC_BUF");
    /* GSC buffer: Output buffer of JPEG */
    m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF");

    /* Magic shot Buffer */
    m_createInternalBufferManager(&m_postPictureBufferMgr, "POSTPICTURE_GSC_BUF");

    /* Preview Buffer */
    /* SCP buffer: Output buffer of MCSC0(Preview) */
    m_scpBufferMgr = NULL;
    /* Preview CB buffer: Output buffer of MCSC1(Preview CB) */
    m_createInternalBufferManager(&m_previewCallbackBufferMgr, "PREVIEW_CB_BUF");
    /* High resolution CB buffer: Output buffer of Reprocessing CB */
    m_createInternalBufferManager(&m_highResolutionCallbackBufferMgr, "HIGH_RESOLUTION_CB_BUF");
    m_fdCallbackHeap = NULL;

    /* Recording Buffer */
    m_recordingCallbackHeap = NULL;
    /* Recording buffer: Output buffer of MCSC2(Preview) */
    m_createInternalBufferManager(&m_recordingBufferMgr, "RECORDING_BUF");

    /* Highspeed dzoom Buffer */
    m_createInternalBufferManager(&m_zoomScalerBufferMgr, "DZOOM_SCALER_BUF");

    /* Thumbnail scaling Buffer */
    m_createInternalBufferManager(&m_thumbnailGscBufferMgr, "THUMBNAIL_GSC_BUF");

    /* Create Internal Queue */
    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        m_mainSetupQ[i] = new frame_queue_t;
        m_mainSetupQ[i]->setWaitTime(550000000);
    }

    m_pipeFrameDoneQ     = new frame_queue_t;
    m_pipeFrameDoneQ->setWaitTime(550000000);

    m_dstIspReprocessingQ  = new frame_queue_t;
    m_dstIspReprocessingQ->setWaitTime(20000000);

    m_pictureThreadInputQ  = new frame_queue_t;
    m_pictureThreadInputQ->setWaitTime(50000000);

    m_dstGscReprocessingQ  = new frame_queue_t;
    m_dstGscReprocessingQ->setWaitTime(500000000);

    m_gdcQ               = new frame_queue_t;

    m_zoomPreviwWithCscQ = new frame_queue_t;

    m_dstPostPictureGscQ = new frame_queue_t;
    m_dstPostPictureGscQ->setWaitTime(2000000000);

    m_dstJpegReprocessingQ = new frame_queue_t;
    m_dstJpegReprocessingQ->setWaitTime(500000000);

    m_pipeFrameVisionDoneQ = new frame_queue_t;
    m_pipeFrameVisionDoneQ->setWaitTime(2000000000);

    m_facedetectQ = new frame_queue_t;
    m_facedetectQ->setWaitTime(550000000);

    m_autoFocusContinousQ.setWaitTime(550000000);

    m_previewQ = new frame_queue_t;
    m_previewQ->setWaitTime(550000000);

    m_previewCallbackGscFrameDoneQ = new frame_queue_t;

    m_recordingQ = new frame_queue_t;
    m_recordingQ->setWaitTime(550000000);

    m_postPictureThreadInputQ = new frame_queue_t(m_postPictureThread);

    m_highResolutionCallbackQ = new frame_queue_t(m_highResolutionCallbackThread);
    m_highResolutionCallbackQ->setWaitTime(550000000);

    m_jpegCallbackQ = new frame_queue_t;
    m_jpegCallbackQ->setWaitTime(1000000000);

    m_postviewCallbackQ = new postview_callback_queue_t;
    m_postviewCallbackQ->setWaitTime(2000000000);

    m_thumbnailCallbackQ = new thumbnail_callback_queue_t;
    m_thumbnailCallbackQ->setWaitTime(1000000000);

    m_ThumbnailPostCallbackQ = new frame_queue_t;
    m_ThumbnailPostCallbackQ->setWaitTime(2000000000);

#ifdef SUPPORT_DEPTH_MAP
    m_depthCallbackQ = new depth_callback_queue_t;
    m_depthCallbackQ->setWaitTime(50000000);
#endif

#ifdef USE_ODC_CAPTURE
    m_ODCProcessingQ = new frame_queue_t;
    m_ODCProcessingQ->setWaitTime(200000000);

    m_postODCProcessingQ = new frame_queue_t;
    m_postODCProcessingQ->setWaitTime(200000000);
#endif


    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum] = new frame_queue_t;
        m_jpegSaveQ[threadNum]->setWaitTime(2000000000);
        m_burst[threadNum] = false;
        m_running[threadNum] = false;
    }

    /* Initialize menber variable */
    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));
    memset(m_faces, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
    memset(&m_notifyCb, 0, sizeof(camera_notify_callback));
    memset(&m_dataCb, 0, sizeof(camera_data_callback));;
    memset(&m_dataCbTimestamp, 0, sizeof(camera_data_timestamp_callback));
    memset(&m_getMemoryCb, 0, sizeof(camera_request_memory));

    m_previewWindow = NULL;
    m_exitAutoFocusThread = false;
    m_autoFocusRunning    = false;
    m_previewEnabled   = false;
    m_pictureEnabled   = false;
    m_recordingEnabled = false;
    m_zslPictureEnabled   = false;
    m_flagHoldFaceDetection = false;
    m_flagStartFaceDetection = false;
    m_flagLLSStart = false;
    m_flagLightCondition = false;
    m_flagBrightnessValue = false;
    m_fdThreshold = 0;
    m_captureSelector = NULL;
    m_bayerCaptureSelector = NULL;
    m_yuvCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_hdrSkipedFcount = 0;
    m_doCscRecording = true;
    m_recordingBufferCount = NUM_RECORDING_BUFFERS;
    m_frameSkipCount = 0;
    m_isZSLCaptureOn = false;
    m_isSuccessedBufferAllocation = false;
    m_fdFrameSkipCount = 0;

    m_displayPreviewToggle = 0;

    m_isTryStopFlash = false;

    m_curMinFps = 0;

    m_visionFps = 0;
    m_visionAe = 0;

    m_hackForAlignment = 0;

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    m_stopBurstShot = false;
    m_disablePreviewCB = false;
    m_checkFirstFrameLux = false;

    m_callbackCookie = NULL;
    m_callbackState = 0;
    m_callbackStateOld = 0;
    m_callbackMonitorCount = 0;

    m_highResolutionCallbackRunning = false;
    m_skipReprocessing = false;
    m_isFirstStart = true;
    m_parameters->setIsFirstStartFlag(m_isFirstStart);

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_exynosconfig = NULL;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_dynamicSccCount = 0;
    m_dynamicBayerCount = 0;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    m_faceDetected = false;
    m_flagThreadStop= false;
    m_isNeedAllocPictureBuffer = false;

    m_longExposureRemainCount = 0;
    m_stopLongExposure = false;
    m_cancelPicture = false;
    m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;

#ifdef BURST_CAPTURE
    m_isCancelBurstCapture = false;
    m_burstCaptureCallbackCount = 0;
    m_burstSaveTimerTime = 0;
    m_burstDuration = 0;
    m_burstInitFirst = false;
    m_burstRealloc = false;
    m_burstShutterLocation = 0;
#endif

#ifdef FIRST_PREVIEW_TIME_CHECK
    m_flagFirstPreviewTimerOn = false;
#endif

    /* init infomation of fd orientation*/
    m_parameters->setDeviceOrientation(0);
    uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
    if (uctlMgr != NULL)
        uctlMgr->setDeviceRotation(m_parameters->getFdOrientation());

#ifdef MONITOR_LOG_SYNC
    m_syncLogDuration = 0;
#endif

    m_tempshot = new struct camera2_shot_ext;
    m_fdmeta_shot = new struct camera2_shot_ext;
    m_meta_shot  = new struct camera2_shot_ext;
    m_picture_meta_shot = new struct camera2_shot;

    m_ionClient = ion_open();
    if (m_ionClient < 0) {
        CLOGE("m_ionClient ion_open() fail");
        m_ionClient = -1;
    }

    /* ExynosConfig setting */
    m_setConfigInform();

    /* Frame manager setting */
    m_setFrameManager();

    /* Initialize Frame factory */
    m_initFrameFactory();

    /* Construct vendor */
    m_vendorSpecificConstructor(m_cameraId, m_dev);

    CLOGI(" -OUT-");
}

ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    CLOGI("-IN-");
    int ret = 0;

    m_stopCompanion();

    if (m_frameMgr != NULL) {
        m_frameMgr->stop();
    }

    /* release all framefactory */
    m_deinitFrameFactory();

    if (m_parameters != NULL) {
        delete m_parameters;
        m_parameters = NULL;
        CLOGD("Parameters(Id=%d) destroyed", m_cameraId);
    }

    /* free all buffers */
    m_releaseBuffers();

    if (m_ionClient >= 0) {
        ion_close(m_ionClient);
        m_ionClient = -1;
    }

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

    if (m_gdcQ != NULL) {
        delete m_gdcQ;
        m_gdcQ = NULL;
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

    if (m_dstIspReprocessingQ != NULL) {
        delete m_dstIspReprocessingQ;
        m_dstIspReprocessingQ = NULL;
    }

    if (m_pictureThreadInputQ != NULL) {
        delete m_pictureThreadInputQ;
        m_pictureThreadInputQ = NULL;
    }

    if (m_dstGscReprocessingQ != NULL) {
        delete m_dstGscReprocessingQ;
        m_dstGscReprocessingQ = NULL;
    }

    if (m_dstPostPictureGscQ != NULL) {
        delete m_dstPostPictureGscQ;
        m_dstPostPictureGscQ = NULL;
    }

    if (m_dstJpegReprocessingQ != NULL) {
        delete m_dstJpegReprocessingQ;
        m_dstJpegReprocessingQ = NULL;
    }

    if (m_postPictureThreadInputQ != NULL) {
        delete m_postPictureThreadInputQ;
        m_postPictureThreadInputQ = NULL;
    }

    if (m_jpegCallbackQ != NULL) {
        delete m_jpegCallbackQ;
        m_jpegCallbackQ = NULL;
    }

    if (m_yuvCallbackQ != NULL) {
        delete m_yuvCallbackQ;
        m_yuvCallbackQ = NULL;
    }

    if (m_postviewCallbackQ != NULL) {
        delete m_postviewCallbackQ;
        m_postviewCallbackQ = NULL;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthCallbackQ != NULL) {
        delete m_depthCallbackQ;
        m_depthCallbackQ = NULL;
    }
#endif

    if (m_thumbnailCallbackQ != NULL) {
        delete m_thumbnailCallbackQ;
        m_thumbnailCallbackQ = NULL;
    }

    if (m_ThumbnailPostCallbackQ != NULL) {
        delete m_ThumbnailPostCallbackQ;
        m_ThumbnailPostCallbackQ = NULL;
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

#ifdef USE_ODC_CAPTURE
    if (m_ODCProcessingQ != NULL) {
        delete m_ODCProcessingQ;
        m_ODCProcessingQ = NULL;
    }

    if (m_postODCProcessingQ != NULL) {
        delete m_postODCProcessingQ;
        m_postODCProcessingQ = NULL;
    }
#endif

    if (m_highResolutionCallbackQ != NULL) {
        delete m_highResolutionCallbackQ;
        m_highResolutionCallbackQ = NULL;
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("BufferManager(bayerBufferMgr) destroyed");
    }

#if defined(DEBUG_RAWDUMP)
    if (m_fliteBufferMgr != NULL) {
        delete m_fliteBufferMgr;
        m_fliteBufferMgr = NULL;
        CLOGD("BufferManager(fliteBufferMgr) destroyed");
    }
#endif

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        delete m_depthMapBufferMgr;
        m_depthMapBufferMgr = NULL;
        CLOGD("BufferManager(m_depthMapBufferMgr) destroyed");
    }
#endif

    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
        CLOGD("BufferManager(3aaBufferMgr) destroyed");
    }

    if (m_ispBufferMgr != NULL) {
        delete m_ispBufferMgr;
        m_ispBufferMgr = NULL;
        CLOGD("BufferManager(ispBufferMgr) destroyed");
    }

    if (m_hwDisBufferMgr != NULL) {
        delete m_hwDisBufferMgr;
        m_hwDisBufferMgr = NULL;
        CLOGD("BufferManager(m_hwDisBufferMgr) destroyed");
    }

    if (m_scpBufferMgr != NULL) {
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
        CLOGD("BufferManager(scpBufferMgr) destroyed");
    }

    if (m_zoomScalerBufferMgr != NULL) {
        delete m_zoomScalerBufferMgr;
        m_zoomScalerBufferMgr = NULL;
        CLOGD("BufferManager(m_zoomScalerBufferMgr) destroyed");
    }

    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
        CLOGD("BufferManager(ispReprocessingBufferMgr) destroyed");
    }

    if (m_yuvReprocessingBufferMgr != NULL) {
        delete m_yuvReprocessingBufferMgr;
        m_yuvReprocessingBufferMgr = NULL;
        CLOGD("BufferManager(yuvReprocessingBufferMgr) destroyed");
    }

    if (m_thumbnailBufferMgr != NULL) {
        delete m_thumbnailBufferMgr;
        m_thumbnailBufferMgr = NULL;
        CLOGD("BufferManager(thumbnailBufferMgr) destroyed");
    }

    if (m_yuvBufferMgr != NULL) {
        delete m_yuvBufferMgr;
        m_yuvBufferMgr = NULL;
        CLOGD("BufferManager(yuvBufferMgr) destroyed");
    }

    if (m_gscBufferMgr != NULL) {
        delete m_gscBufferMgr;
        m_gscBufferMgr = NULL;
        CLOGD("BufferManager(gscBufferMgr) destroyed");
    }

    if (m_jpegBufferMgr != NULL) {
        delete m_jpegBufferMgr;
        m_jpegBufferMgr = NULL;
        CLOGD("BufferManager(jpegBufferMgr) destroyed");
    }

    if (m_previewCallbackBufferMgr != NULL) {
        delete m_previewCallbackBufferMgr;
        m_previewCallbackBufferMgr = NULL;
        CLOGD("BufferManager(previewCallbackBufferMgr) destroyed");
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        delete m_highResolutionCallbackBufferMgr;
        m_highResolutionCallbackBufferMgr = NULL;
        CLOGD("BufferManager(m_highResolutionCallbackBufferMgr) destroyed");
    }

    if (m_recordingBufferMgr != NULL) {
        delete m_recordingBufferMgr;
        m_recordingBufferMgr = NULL;
        CLOGD("BufferManager(recordingBufferMgr) destroyed");
    }

    if (m_bayerCaptureSelector != NULL) {
        delete m_bayerCaptureSelector;
        m_bayerCaptureSelector = NULL;
    }

    if (m_yuvCaptureSelector != NULL) {
        delete m_yuvCaptureSelector;
        m_yuvCaptureSelector = NULL;
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        delete m_recordingCallbackHeap;
        m_recordingCallbackHeap = NULL;
        CLOGD("BufferManager(recordingCallbackHeap) destroyed");
    }

    m_isFirstStart = true;
    m_previewBufferCount = NUM_PREVIEW_BUFFERS;

    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    if (m_tempshot != NULL) {
        delete m_tempshot;
        m_tempshot = NULL;
    }

    if (m_fdmeta_shot != NULL) {
        delete m_fdmeta_shot;
        m_fdmeta_shot = NULL;
    }

    if (m_meta_shot != NULL) {
        delete m_meta_shot;
        m_meta_shot = NULL;
    }

    if (m_picture_meta_shot != NULL) {
        delete m_picture_meta_shot;
        m_picture_meta_shot = NULL;
    }

    m_vendorSpecificDestructor();

    CLOGI(" -OUT-");
}

int ExynosCamera::getCameraId() const
{
    return m_cameraOriginId;
}

int ExynosCamera::getCameraIdInternal() const
{
    return m_cameraId;
}

bool ExynosCamera::previewEnabled()
{
    bool ret = false;

    /* in scalable mode, we should controll out state */
    if (m_parameters != NULL &&
        (m_parameters->getScalableSensorMode() == true) &&
        (m_scalableSensorMgr.getMode() == EXYNOS_CAMERA_SCALABLE_CHANGING))
        ret = true;
    else
        ret = m_previewEnabled;

    CLOGI("m_previewEnabled=%d",
        (int)ret);

    return ret;
}

bool ExynosCamera::recordingEnabled()
{
    bool ret = m_getRecordingEnabled();
    CLOGI("m_recordingEnabled=%d",
        (int)ret);

    return ret;
}

status_t ExynosCamera::autoFocus()
{
    CLOGI("");

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" Vision mode does not support");
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    if (m_previewEnabled == false) {
        CLOGE(" preview is not enabled");
        return INVALID_OPERATION;
    }

    if (m_autoFocusRunning == false) {
        m_autoFocusType = AUTO_FOCUS_SERVICE;
        m_autoFocusThread->requestExitAndWait();
        m_autoFocusThread->run(PRIORITY_DEFAULT);
    } else {
        CLOGW(" auto focus is inprogressing");
    }

#if 0 // not used.
    if (m_parameters != NULL) {
        if (m_parameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI(" ae awb lock");
            m_parameters->m_setAutoExposureLock(true);
            m_parameters->m_setAutoWhiteBalanceLock(true);
        }
    }
#endif

    return NO_ERROR;
}

CameraParameters ExynosCamera::getParameters() const
{
    CLOGV("");

    return m_parameters->getParameters();
}

void ExynosCamera::setCallbacks(
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void *user)
{
    CLOGI(" -IN-");

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
        CLOGE("m_mhbAllocator init failed");
    }
}

status_t ExynosCamera::setDualMode(bool enabled)
{
    if (m_parameters == NULL) {
        CLOGE("m_parameters is NULL");
        return INVALID_OPERATION;
    }

    m_parameters->setDualMode(enabled);

    return NO_ERROR;
}

void ExynosCamera::enableMsgType(int32_t msgType)
{
    if (m_parameters) {
        CLOGV(" enable Msg (%x)", msgType);
        m_parameters->enableMsgType(msgType);
    }
}

void ExynosCamera::disableMsgType(int32_t msgType)
{
    if (m_parameters) {
        CLOGV(" disable Msg (%x)", msgType);
        m_parameters->disableMsgType(msgType);
    }
}

bool ExynosCamera::msgTypeEnabled(int32_t msgType)
{
    bool isEnabled = false;

    if (m_parameters) {
        CLOGV(" Msg type enabled (%x)", msgType);
        isEnabled = m_parameters->msgTypeEnabled(msgType);
    }

    return isEnabled;
}

status_t ExynosCamera::dump(__unused int fd)
{
    CLOGI("");

    m_dump();

    return NO_ERROR;
}

status_t ExynosCamera::storeMetaDataInBuffers(__unused bool enable)
{
    CLOGI("");

    return OK;
}

bool ExynosCamera::m_mainThreadFunc(void)
{
    status_t ret = NO_ERROR;
    int index = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    if (m_previewEnabled == false) {
        CLOGD("Preview is stopped, thread stop");
        return false;
    }

    ret = m_pipeFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        return false;
    }
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT)
            CLOGW("Wait timeout");
        else
            CLOGE("Wait and pop fail, ret(%d)", ret);

        return true;
    }

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return true;
    }

/* HACK Reset Preview Flag*/
#if 0
    if (m_parameters->getRestartPreview() == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGE("m_resetPreview(%d)", m_resetPreview);
        if (ret < 0)
            CLOGE("restart preview internal fail");

        return true;
    }
#endif

    ret = m_handlePreviewFrame(newFrame);
    if (ret != NO_ERROR) {
        CLOGE("handle preview frame fail, ret(%d)",
                ret);
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
        ret = m_removeFrameFromList(&m_processList, newFrame, &m_processListLock);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
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

    return true;
}

bool ExynosCamera::m_mainThreadQSetupFLITE(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;
    bool loop = true;
    int pipeId = PIPE_FLITE;

    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameSP_sptr_t newframe = NULL;

    CLOGV("Wait previewCancelQ");
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        goto func_exit;
    }

    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("Wait timeout");
        else
            CLOGE("Wait and pop fail, ret(%d)", ret);

        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }

    /* handle src Buffer */
    ret = frame->getSrcBuffer(pipeId, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
    }

    if (buffer.index >= 0) {
        ExynosCameraBufferManager *bufferMgr = NULL;
        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("m_getBufferManager(pipeId : %d) fail", pipeId);
        } else {
            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("m_putBuffers(%d) fail", buffer.index);
            }
        }
    }

    if (m_bayerBufferMgr->getNumOfAvailableBuffer() <= 0) {
        CLOGW("bayerBufferMgr->getNumOfAvailableBuffer() => %d",
                m_bayerBufferMgr->getNumOfAvailableBuffer());
    }

    if (m_parameters->getVisionMode() == true)
        ret = m_generateFrameVision(-1, newframe);
    else
        ret = m_generateFrame(-1, newframe);

    if (ret != NO_ERROR) {
        CLOGE("generateFrame fail");
        goto func_exit;
    }

    if (newframe == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }

    ret = m_setupEntity(pipeId, newframe);
    if (ret != NO_ERROR) {
        CLOGE("setupEntity fail");
    }

    if (m_parameters->getVisionMode() == true)
        m_visionFrameFactory->pushFrameToPipe(newframe, pipeId);
    else
        m_previewFrameFactory->pushFrameToPipe(newframe, pipeId);

func_exit:
    if (frame != NULL) {
        frame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_mainThreadQSetup3AA(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;
    bool loop = true;
    int bayerPipeId = PIPE_FLITE;
    int pipeId = PIPE_3AA;

    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameSP_sptr_t newframe = NULL;

    CLOGV("Wait previewCancelQ");
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        goto func_exit;
    }

    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("Wait timeout");
        else
            CLOGE("Wait and pop fail, ret(%d)", ret);

        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }

    ret = frame->getSrcBuffer(pipeId, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                pipeId, ret);
        goto func_exit;
    }

    if (buffer.index >= 0) {
        ret = m_putBuffers(m_3aaBufferMgr, buffer.index);

        if (ret != NO_ERROR) {
            CLOGE("put Buffer fail");
        }
    }

    if (m_parameters->isUseEarlyFrameReturn() == true) {
        frame = NULL;
    } else {
        m_pipeFrameDoneQ->pushProcessQ(&frame);
    }

    ret = m_generateFrame(-1, newframe);
    if (ret != NO_ERROR) {
        CLOGE("generateFrame fail");
        goto func_exit;
    }

    if (newframe == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }

    ret = m_setupEntity(pipeId, newframe);
    if (ret != NO_ERROR)
        CLOGE("setupEntity fail");

    m_previewFrameFactory->pushFrameToPipe(newframe, pipeId);

func_exit:
    return loop;
}

bool ExynosCamera::m_gdcThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;
    bool loop = true;

    ExynosRect srcRect;
    ExynosRect dstRect;

    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;

    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;

    int srcPipeId = -1;
    if (m_parameters->is3aaIspOtf() == true)
        srcPipeId = PIPE_3AA;
    else
        srcPipeId = PIPE_ISP;

    int srcFmt = m_parameters->getHwPreviewFormat();
    int srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

    int dstPipeId = PIPE_GDC;
#if defined(GDC_COLOR_FMT)
    int dstFmt = GDC_COLOR_FMT;
#else
    int dstFmt = V4L2_PIX_FMT_NV12M;
#endif /* GDC_COLOR_FMT */
    int dstBufIndex = -1;

    ExynosRect srcBcropRect;
    int        srcBcropNodeIndex = 1;

    uint32_t *output = NULL;
    struct camera2_stream *meta = NULL;

    int hwPreviewW, hwPreviewH;
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    int sensorW = 0;
    int sensorH = 0;
    camera2_node_group node_group_info_bcrop;

    ExynosCameraFrameSP_sptr_t frame = NULL;
    int32_t status = 0;
    frame_queue_t *pipeFrameDoneQ = m_pipeFrameDoneQ;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    CLOGV("wait gdcThreadFunc");
    status = m_gdcQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        return false;
    }
    if (status < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (status == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL, retry");
        return true;
    }


    ///////////////////////////
    // about src
    m_getBufferManager(srcPipeId, &srcBufMgr, DST_BUFFER_DIRECTION);
    if (srcBufMgr == NULL){
        CLOGE("m_getBufferManager(srcPipeId : %d, &srcBufMgr, DST_BUFFER_DIRECTION) fail. frame(%d)",
            srcPipeId, frame->getFrameCount());
        goto func_exit;
    }

    ret = frame->getDstBufferState(srcPipeId, &state, srcNodeIndex);
    if (ret < 0 || state != ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGE("getDstBufferState fail, srcPipeId(%d), state(%d) frame(%d)",
                srcPipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;

    ret = frame->getDstBuffer(srcPipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, srcPipeId(%d), ret(%d) frame(%d)",
                srcPipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.getMetaPlaneIndex()];

    if (meta == NULL) {
        CLOGE("meta is NULL, srcPipeId(%d), ret(%d) frame(%d)",
                dstPipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    output = meta->output_crop_region;

    ///////////////////////////
    // about dst
    m_getBufferManager(dstPipeId, &dstBufMgr, DST_BUFFER_DIRECTION);
    if (dstBufMgr == NULL){
        CLOGE("m_getBufferManager(dstPipeId : %d, &dstBufMgr, DST_BUFFER_DIRECTION) fail. frame(%d)",
            dstPipeId, frame->getFrameCount());
        goto func_exit;
    }

    dstBuf.index = -2;

    ret = dstBufMgr->getBuffer(&dstBufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("Buffer manager(%s) getBuffer fail, frame(%d), ret(%d)",
            dstBufMgr->getName(), frame->getFrameCount(), ret);
        goto func_exit;
    }

    /* Set SRC Rect */
    srcRect.x     = output[0];
    srcRect.y     = output[1];
    srcRect.w     = output[2];
    srcRect.h     = output[3];
#if GRALLOC_CAMERA_64BYTE_ALIGN
    srcRect.fullW = ALIGN_UP(output[2], CAMERA_64PX_ALIGN);
#else
    srcRect.fullW = output[2];
#endif
    srcRect.fullH = output[3];
    srcRect.colorFormat = srcFmt;

    // Set GDC's srcBcropRect bcrop size in hw sensor size
    m_parameters->getMaxSensorSize(&sensorW, &sensorH);

    if (m_parameters->isFlite3aaOtf() == true)
        frame->getNodeGroupInfo(&node_group_info_bcrop, PERFRAME_INFO_FLITE);
    else
        frame->getNodeGroupInfo(&node_group_info_bcrop, PERFRAME_INFO_3AA);

    srcBcropRect.x     = node_group_info_bcrop.leader.input.cropRegion[0];
    srcBcropRect.y     = node_group_info_bcrop.leader.input.cropRegion[1];
    srcBcropRect.w     = node_group_info_bcrop.leader.input.cropRegion[2];
    srcBcropRect.h     = node_group_info_bcrop.leader.input.cropRegion[3];
    srcBcropRect.fullW = sensorW;
    srcBcropRect.fullH = sensorH;
    srcBcropRect.colorFormat = srcFmt;

    /* Set DST Rect */
    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = srcRect.w;
    dstRect.h = srcRect.h;
    dstRect.fullW = srcRect.fullW;
    dstRect.fullH = srcRect.fullH;
    dstRect.colorFormat = dstFmt;

    CLOGV("ODC for preview : [Bcrop] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        frame->getFrameCount(),
        srcBuf.index,
        srcBcropRect.x, srcBcropRect.y, srcBcropRect.w, srcBcropRect.h, srcBcropRect.fullW, srcBcropRect.fullH,
        v4l2Format2Char(srcBcropRect.colorFormat, 0),
        v4l2Format2Char(srcBcropRect.colorFormat, 1),
        v4l2Format2Char(srcBcropRect.colorFormat, 2),
        v4l2Format2Char(srcBcropRect.colorFormat, 3));

    CLOGV("ODC for preview : [SRC] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        frame->getFrameCount(),
        srcBuf.index,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH,
        v4l2Format2Char(srcRect.colorFormat, 0),
        v4l2Format2Char(srcRect.colorFormat, 1),
        v4l2Format2Char(srcRect.colorFormat, 2),
        v4l2Format2Char(srcRect.colorFormat, 3));

    CLOGV("ODC for preview : [DST] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        frame->getFrameCount(),
        dstBuf.index,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH,
        v4l2Format2Char(dstRect.colorFormat, 0),
        v4l2Format2Char(dstRect.colorFormat, 1),
        v4l2Format2Char(dstRect.colorFormat, 2),
        v4l2Format2Char(dstRect.colorFormat, 3));

    if (srcRect.w != dstRect.w ||
        srcRect.h != dstRect.h) {
        CLOGW("ODC for preview : size is invalid : [SRC] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            frame->getFrameCount(),
            srcBuf.index,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);

        CLOGW("ODC for preview : size is invalid : [DST] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            frame->getFrameCount(),
            dstBuf.index,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);
    }

    ret = frame->setSrcRect(dstPipeId, srcRect);
    if (ret != NO_ERROR) {
        CLOGE("frame->setSrcRect(dstPipeId : %d) fail. frame(%d)", dstPipeId, frame->getFrameCount());
        goto func_exit;
    }

    ret = frame->setSrcRect(dstPipeId, srcBcropRect, srcBcropNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("frame->setSrcRect(dstPipeId : %d, srcBcropNodeIndex : %d) fail. frame(%d)",
            dstPipeId, srcBcropNodeIndex, frame->getFrameCount());
        goto func_exit;
    }

    ret = frame->setDstRect(dstPipeId, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("frame->setDstRect(dstPipeId : %d) fail. frame(%d)", dstPipeId, frame->getFrameCount());
        goto func_exit;
    }



    ret = m_setupEntity(dstPipeId, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("setupEntity fail, dstPipeId(%d), ret(%d) frame(%d)",
                dstPipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, dstPipeId);
    m_previewFrameFactory->pushFrameToPipe(frame, dstPipeId);

    CLOGV("--OUT--");

    return loop;

func_exit:

    CLOGE("do GDC failed frame(%d) src(%d) dst(%d)",
            frame->getFrameCount(), srcBuf.index, dstBuf.index);

    dstBuf.index = -1;

    ret = frame->getDstBuffer(dstPipeId, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, dstPipeId(%d), ret(%d) frame(%d)",
                dstPipeId, ret, frame->getFrameCount());
    }

    if (0 <= dstBuf.index) {
        if (dstBufMgr == NULL) {
            CLOGE("dstBufMgr == NULL. so, skip m_putBuffer(%d). frame(%d)",
                dstBuf.index, frame->getFrameCount());
        } else {
            ret = dstBufMgr->cancelBuffer(dstBuf.index, true);
            if (ret != NO_ERROR) {
                CLOGE("m_putBuffers(dstBufMgr(%s), dstBuf.index : %d) fail, frame(%d)",
                    dstBufMgr->getName(), dstBuf.index, frame->getFrameCount());
            }
        }
    }

    /* GDC with ndone frame, in order to frame order*/
    frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, dstPipeId);
    m_previewFrameFactory->pushFrameToPipe(frame, dstPipeId);

    CLOGV("--OUT--");

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_syncPreviewWithGDC(ExynosCameraFrameSP_sptr_t frame)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;

    int pipeId = PIPE_GDC;
    int dstNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

    ExynosRect srcRect;
    ExynosRect dstRect;

    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;

    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;

    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    /* Get Src/Dst buffer manager */
    m_getBufferManager(pipeId, &srcBufMgr, SRC_BUFFER_DIRECTION);
    m_getBufferManager(pipeId, &dstBufMgr, DST_BUFFER_DIRECTION);

    if (srcBufMgr == NULL) {
        CLOGE("m_getBufferManager(pipeId, &srcBufMgr, SRC_BUFFER_DIRECTION) fail. frame(%d)",
            pipeId, frame->getFrameCount());
        goto func_exit;
    }

    if (dstBufMgr == NULL) {
        CLOGE("m_getBufferManager(pipeId, &dstBufMgr, DST_BUFFER_DIRECTION) fail. frame(%d)",
            pipeId, frame->getFrameCount());
        goto func_exit;
    }

    ///////////////////////////
    // about src
    state = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getSrcBufferState(pipeId, &state);
    if (ret != NO_ERROR || state == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("getSrcBufferState fail, pipeId(%d), entityState(%d) frame(%d)", pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;
    ret = frame->getSrcBuffer(pipeId, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (srcBuf.index < 0) {
        CLOGE("Invalid Src buffer index(%d), frame(%d)", srcBuf.index, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ///////////////////////////
    // about dst
    state = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getDstBufferState(pipeId, &state);
    if (ret != NO_ERROR || state == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("getDstBufferState fail, pipeId(%d), entityState(%d) frame(%d)", pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    dstBuf.index = -1;
    ret = frame->getDstBuffer(pipeId, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (dstBuf.index < 0) {
        CLOGE("Invalid Dst buffer index(%d), frame(%d)", srcBuf.index, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* copy metadata src to dst */
    memcpy(dstBuf.addr[dstBuf.getMetaPlaneIndex()],srcBuf.addr[srcBuf.getMetaPlaneIndex()], sizeof(struct camera2_stream));

    /* Return src Buffer */
    if (srcBuf.index >= 0) {
        ret = srcBufMgr->cancelBuffer(srcBuf.index, true);
        if (ret != NO_ERROR) {
            CLOGE("m_putBuffers(srcBufMgr(%s), srcBuf.index : %d) fail, frame(%d)",
                srcBufMgr->getName(), srcBuf.index, frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    /* Sync dst Node index with MCSC0 */
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBuffer(pipeId, dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    CLOGV("m_syncPreviewWithGDC done(%d)", ret);

    return ret;

func_exit:
    srcBuf.index = -1;
    dstBuf.index = -1;

    /* 1. return buffer pipe done. */
    ret = frame->getSrcBuffer(pipeId, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
    }

    /* 2. do not return buffer dual front case, frameselector ownershipt for the buffer */
    if (srcBuf.index >= 0) {
        if (srcBufMgr == NULL) {
            CLOGE("srcBufMgr == NULL. so, skip m_putBuffer(%d). frame(%d)",
                srcBuf.index, frame->getFrameCount());
        } else {
            ret = srcBufMgr->cancelBuffer(srcBuf.index, true);
            if (ret != NO_ERROR) {
                CLOGE("m_putBuffers(srcBufMgr(%s), srcBuf.index : %d) fail, frame(%d)",\
                    srcBufMgr->getName(), srcBuf.index, frame->getFrameCount());
            }
        }
    }

    /* 3. if the gsc dst buffer available, return the buffer. */
    ret = frame->getDstBuffer(pipeId, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0) {
        if (dstBufMgr == NULL) {
            CLOGE("dstBufMgr == NULL. so, skip m_putBuffer(%d). frame(%d)",
                dstBuf.index, frame->getFrameCount());
        } else {
            ret = dstBufMgr->cancelBuffer(dstBuf.index, true);
            if (ret != NO_ERROR) {
                CLOGE("m_putBuffers(dstBufMgr, dstBuf.index : %d) fail, frame(%d)",
                    dstBufMgr->getName(), dstBuf.index, frame->getFrameCount());
            }
        }
    }

    /* 4. change buffer state error for error handlering */
    /*  1)dst buffer state : 0( putbuffer ndone for mcpipe ) */
    /*  2)dst buffer state : scp node index ( getbuffer ndone for mcpipe ) */
    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR, dstNodeIndex);
    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR);

    CLOGE("sync with GDC failed frame(%d) ret(%d) src(%d) dst(%d)", frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    return INVALID_OPERATION;
}

bool ExynosCamera::m_isZoomPreviewWithCscEnabled(int32_t pipeId, ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    int srcW = 0, srcH = 0;
    int previewW = 0, previewH = 0;
    int srcFormat = 0, previewFormat = 0;
    ExynosCameraBuffer srcBuffer;

    /* get source buffer */
    int nodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
    srcBuffer.index = -1;
    ret = frame->getDstBuffer(pipeId, &srcBuffer, nodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return false;
    }

    if (srcBuffer.index < 0) {
        CLOGE("Invalid buffer index(%d), pipeId(%d)", srcBuffer.index, pipeId, ret);
        return false;
    }

    /* getMetadata to get buffer size */
    struct camera2_stream *metadata = (struct camera2_stream*)srcBuffer.addr[srcBuffer.getMetaPlaneIndex()];
    if (metadata == NULL) {
        CLOGE("srcBuffer.addr is NULL, srcBuffer.addr(0x%x)",
                srcBuffer.addr[srcBuffer.getMetaPlaneIndex()]);
        return false;
    }

    /* get source image size & format */
    uint32_t *outputSize = metadata->output_crop_region;

    srcW = outputSize[2];
    srcH = outputSize[3];
    if (pipeId == PIPE_GDC) {
#if defined(GDC_COLOR_FMT)
        srcFormat = GDC_COLOR_FMT;
#else
        srcFormat = V4L2_PIX_FMT_NV12M;
#endif /* GDC_COLOR_FMT */
    } else {
        srcFormat = m_parameters->getHwPreviewFormat();
    }

    /* get destination imge size & format */
    m_parameters->getHwPreviewSize(&previewW, &previewH);
    previewFormat = m_parameters->getHwPreviewFormat();

    CLOGV("Src size(%dx%d) fmt(%d), preview size(%dx%d) fmt(%d)",
            srcW, srcH, srcFormat, previewW, previewH, previewFormat);

    /* check scaler conditions( compare the crop size & format ) */
    if (srcW == previewW
        && srcH == previewH
        && srcFormat == previewFormat) {
        return false;
    }

    return true;
}

bool ExynosCamera::m_zoomPreviwWithCscThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int  ret  = 0;
    bool loop = true;
    int32_t previewFormat = 0;
    ExynosRect srcRect, dstRect;
    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;
    uint32_t *output = NULL;
    struct camera2_stream *meta = NULL;
    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int scpNodeIndex = -1;
    int srcNodeIndex = -1;
    int srcFmt = -1;
    int pipeId = -1;
    int gscPipe = -1;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    int32_t status = 0;
    frame_queue_t *pipeFrameDoneQ = NULL;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    CLOGV("wait zoomPreviwWithCscQThreadFunc");
    status = m_zoomPreviwWithCscQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        return false;
    }
    if (status < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (status == TIMED_OUT) {
            CLOGW("wait timeout");
            return false;
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL, retry");
        return true;
    }

    pipeFrameDoneQ = m_pipeFrameDoneQ;

    previewFormat = m_parameters->getHwPreviewFormat();
    m_parameters->getHwPreviewSize(&previewW, &previewH);

    /* get Scaler src Buffer Node Index*/
    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        pipeId = PIPE_3AA;
#ifdef USE_GSC_FOR_PREVIEW
        gscPipe = PIPE_GSC;
#else
        gscPipe = PIPE_GSC_VIDEO;
#endif
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        srcFmt = m_parameters->getHWVdisFormat();
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

        dstBufMgr = m_scpBufferMgr;
    } else {
        if (m_parameters->getGDCEnabledMode() == true)
            pipeId = PIPE_GDC;
        else if (m_parameters->is3aaIspOtf() == true)
            pipeId = PIPE_3AA;
        else
            pipeId = PIPE_ISP;

#ifdef USE_GSC_FOR_PREVIEW
        gscPipe = PIPE_GSC;
#else
        gscPipe = PIPE_GSC_VIDEO;
#endif

        if (m_parameters->getGDCEnabledMode() == true) {
            srcNodeIndex = 0;
        } else {
            srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
        }

        if (pipeId == PIPE_GDC) {
#if defined(GDC_COLOR_FMT)
            srcFmt = GDC_COLOR_FMT;
#else
            srcFmt = V4L2_PIX_FMT_NV12M;
#endif /* GDC_COLOR_FMT */
        } else {
            srcFmt = m_parameters->getHwPreviewFormat();
        }
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

        dstBufMgr = m_scpBufferMgr;
    }

    ret = m_getBufferManager(pipeId, &srcBufMgr, SRC_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                pipeId, ret);
        return ret;
    }

    ret = frame->getDstBufferState(pipeId, &state, srcNodeIndex);
    if (ret < 0 || state != ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGE("getDstBufferState fail, pipeId(%d), state(%d) frame(%d)",
                pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* get scaler source buffer */
    srcBuf.index = -1;

    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.getMetaPlaneIndex()];

    if (meta == NULL) {
        CLOGE("meta is NULL, pipeId(%d), ret(%d) frame(%d)",
                gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    output = meta->output_crop_region;

    dstBuf.index = -2;
    bufIndex = -1;
    ret = dstBufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("Buffer manager getBuffer fail, frame(%d), ret(%d)",
                frame->getFrameCount(), ret);
        goto func_exit;
    }

    /* csc and scaling */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = output[2];
    srcRect.h = output[3];
    switch (srcFmt) {
    case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_YVYU:
    case V4L2_PIX_FMT_UYVY:
    case V4L2_PIX_FMT_VYUY:
        srcRect.fullW = ALIGN_UP(output[2], CAMERA_16PX_ALIGN / 2);
        break;
    default:
#if GRALLOC_CAMERA_64BYTE_ALIGN
        srcRect.fullW = ALIGN_UP(output[2], CAMERA_64PX_ALIGN);
#else
        srcRect.fullW = ALIGN_UP(output[2], CAMERA_16PX_ALIGN);
#endif
        break;
    }
    srcRect.fullH = output[3];
    srcRect.colorFormat = srcFmt;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = previewW;
    dstRect.h = previewH;
#if GRALLOC_CAMERA_64BYTE_ALIGN
    dstRect.fullW = ALIGN_UP(previewW, CAMERA_64PX_ALIGN);
#else
    dstRect.fullW = previewW;
#endif
    dstRect.fullH = previewH;
    dstRect.colorFormat = previewFormat;

    m_parameters->calcPreviewDzoomCropSize(&srcRect, &dstRect);

    ret = frame->setSrcRect(gscPipe, srcRect);
    ret = frame->setDstRect(gscPipe, dstRect);

    CLOGV("do zoomPreviw with CSC : srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%c%c%c%c) actual(%x) -> \
            (%d, %d, %d, %d) format(%c%c%c%c)  actual(%x)",
            srcBuf.index, dstBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h,
            v4l2Format2Char(srcFmt, 0),
            v4l2Format2Char(srcFmt, 1),
            v4l2Format2Char(srcFmt, 2),
            v4l2Format2Char(srcFmt, 3),
            V4L2_PIX_2_HAL_PIXEL_FORMAT(srcFmt),
            dstRect.x, dstRect.y, dstRect.w, dstRect.h,
            v4l2Format2Char(previewFormat, 0),
            v4l2Format2Char(previewFormat, 1),
            v4l2Format2Char(previewFormat, 2),
            v4l2Format2Char(previewFormat, 3),
            V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = m_setupEntity(gscPipe, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d) frame(%d)",
                gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(frame, gscPipe);

    if (m_previewFrameFactory->checkPipeThreadRunning(gscPipe) == false) {
        ret = m_previewFrameFactory->startThread(gscPipe);
        if (ret != NO_ERROR) {
            CLOGE("Start thread fail!(ret:%d) Pipe(%d)", ret, gscPipe);
        }
    }

    CLOGV("--OUT--");

    return loop;

func_exit:

    CLOGE("do zoomPreviw with CSC failed frame(%d) ret(%d) src(%d) dst(%d)",
            frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    dstBuf.index = -1;

    ret = frame->getDstBuffer(gscPipe, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                gscPipe, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index, true);

    /* msc with ndone frame, in order to frame order*/
    CLOGE(" zoom with scaler getbuffer failed , use MSC for NDONE frame(%d)",
            frame->getFrameCount());

    frame->setSrcBufferState(gscPipe, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(frame, gscPipe);

    CLOGV("--OUT--");

    return INVALID_OPERATION;
}

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret = 0;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("m_setBuffers failed, releaseBuffer");

        /* TODO: Need release buffers and error exit */

        m_releaseBuffers();
        m_isSuccessedBufferAllocation = false;
        return false;
    }

    m_isSuccessedBufferAllocation = true;
    return false;
}

bool ExynosCamera::m_startPictureInternalThreadFunc(void)
{
    int ret = 0;

    ret = m_startPictureInternal();
    if (ret < 0) {
        CLOGE("m_startPictureInternal failed");

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

bool ExynosCamera::m_startPictureBufferThreadFunc(void)
{
    int ret = 0;

    ret = m_setPictureBuffer();
    if (ret < 0) {
        CLOGE("m_setPictureBuffer failed");

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

    uint64_t seriesShotDuration = m_parameters->getSeriesShotDuration();

    m_burstPrePictureTimer.start();

    if (m_parameters->isReprocessingCapture() == true)
        loop = m_reprocessingPrePictureInternal();
    else
        loop = m_prePictureInternal(&isProcessed);

    m_burstPrePictureTimer.stop();
    m_burstPrePictureTimerTime = m_burstPrePictureTimer.durationUsecs();

    if(isProcessed && loop && seriesShotDuration > 0 && m_burstPrePictureTimerTime < seriesShotDuration) {
        CLOGD(" The time between shots is too short (%lld)us. Extended to (%lld)us"
            , (long long)m_burstPrePictureTimerTime, (long long)seriesShotDuration);

        burstWaitingTime = seriesShotDuration - m_burstPrePictureTimerTime;
        usleep(burstWaitingTime);
    }

    return loop;
}

#if 0
bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int loop = false;
    int retryCountGSC = 4;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    camera2_stream *shot_stream = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBuffer highResolutionCbBuffer;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_parameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_parameters->getPreviewFormat();

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

    pipeId_scc = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
    pipeId_gsc = PIPE_GSC_REPROCESSING;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_parameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        CLOGD(" High Resolution Callback Stop");
        goto CLEAN;
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, cbPreviewW, cbPreviewH);
    if (ret < 0) {
        CLOGE(" BAD value, format(%x), size(%dx%d)",
            previewFormat, cbPreviewW, cbPreviewH);
        return ret;
    }

    /* wait SCC */
    CLOGV("wait SCC output");
    ret = m_highResolutionCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        goto CLEAN;
    }
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", pipeId_scc, ret);
        return ret;
    }
    CLOGV("SCC output done");

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        /* get GSC src buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                    pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[buffer_idx]);
        if (shot_stream == NULL) {
            CLOGE("shot_stream is NULL. buffer(%d)",
                    sccReprocessingBuffer.index);
            goto CLEAN;
        }

        /* alloc GSC buffer */
        if (m_highResolutionCallbackBufferMgr->isAllocated() == false) {
            int batchSize = m_parameters->getBatchSize(PIPE_MCSC2);
            ret = m_allocBuffers(m_highResolutionCallbackBufferMgr, planeCount, planeSize, bytesPerLine,
                                 minBufferCount, maxBufferCount, batchSize, type, allocMode,
                                 false, false);
            if (ret < 0) {
                CLOGE("m_highResolutionCallbackBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    minBufferCount, maxBufferCount);
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
            CLOGE("m_getMemoryCb(%d) fail", highResolutionCbBuffer.size[0]);
            goto CLEAN;
        }

        ret = m_setCallbackBufferInfo(&highResolutionCbBuffer, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            CLOGE(" setCallbackBufferInfo fail, ret(%d)", ret);
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

        CLOGV("srcRect x : %d, y : %d, w : %d, h : %d", srcRect.x, srcRect.y, srcRect.w, srcRect.h);
        CLOGV("dstRect x : %d, y : %d, w : %d, h : %d", dstRect.x, dstRect.y, dstRect.w, dstRect.h);

        ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, &highResolutionCbBuffer);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                pipeId_gsc, ret);
            goto CLEAN;
        }

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(m_dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);

        /* wait GSC for high resolution preview callback */
        CLOGI("wait GSC output");
        while (retryCountGSC > 0) {
            ret = m_dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("wait and pop timeout, ret(%d)", ret);
                m_pictureFrameFactory->startThread(pipeId_gsc);
            } else if (ret < 0) {
                CLOGE("wait and pop fail, ret(%d)", ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else {
                break;
            }
            retryCountGSC--;
        }

        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            goto CLEAN;
        }
        CLOGI("GSC output done");

        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_yuvReprocessingBufferMgr, sccReprocessingBuffer.index);

        CLOGV("high resolution preview callback");
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        /* put high resolution callback buffer */
        ret = m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
            goto CLEAN;
        }

        if (newFrame != NULL) {
            newFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret < 0) {
                CLOGE("remove frame from processList fail, ret(%d)", ret);
            }

            CLOGD(" Reprocessing frame delete(%d)", newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    } else {
        CLOGD(" Preview callback message disabled, skip callback");
        /* put SCC buffer */
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_yuvReprocessingBufferMgr, sccReprocessingBuffer.index);
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("high resolution callback thread complete, loop(%d)", loop);

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2)
        ret = m_putBuffers(m_yuvReprocessingBufferMgr, sccReprocessingBuffer.index);
    if (highResolutionCbBuffer.index != -2)
        m_putBuffers(m_highResolutionCallbackBufferMgr, highResolutionCbBuffer.index);

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        ret = m_deleteFrame(&newFrame);
        if (ret < 0) {
            CLOGE("m_deleteFrame fail, ret(%d)", ret);
        }
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
                m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("high resolution callback thread fail, loop(%d)", loop);

    /* one shot */
    return loop;
}
#endif

bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int loop = false;
    int retryCountGSC = 4;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    camera2_stream *shot_stream = NULL;

    ExynosCameraBuffer reprocessingDstBuffer;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_parameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_parameters->getPreviewFormat();

    int pipeReprocessingSrc = 0;
    int pipeReprocessingDst = 0;

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int planeCount = getYuvPlaneCount(previewFormat);
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int buffer_idx = m_getShotBufferIdex();
    ExynosCameraBufferManager *bufferMgr = NULL;

    reprocessingDstBuffer.index = -2;

    pipeReprocessingSrc = m_parameters->isReprocessing3aaIspOTF() == true ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
    pipeReprocessingDst = PIPE_MCSC1_REPROCESSING;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_parameters->getHighResolutionCallbackMode() == false &&
        m_highResolutionCallbackRunning == false) {
        CLOGD("High Resolution Callback Stop");
        goto CLEAN;
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, cbPreviewW, cbPreviewH);
    if (ret < 0) {
        CLOGE("BAD value, format(%x), size(%dx%d)",
            previewFormat, cbPreviewW, cbPreviewH);
        return ret;
    }

    ret = m_getBufferManager(pipeReprocessingDst, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                pipeReprocessingDst, ret);
        return ret;
    }

    /* wait MCSC1 */
    CLOGW("wait MCSC1 output");
    ret = m_highResolutionCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        goto CLEAN;
    }
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    ret = newFrame->setEntityState(pipeReprocessingSrc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                pipeReprocessingSrc, ret);
        return ret;
    }
    CLOGW("MCSC1 output done");

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        ret = newFrame->getDstBuffer(pipeReprocessingSrc, &reprocessingDstBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeReprocessingDst));
        if (ret < 0 || reprocessingDstBuffer.index < 0) {
            CLOGE("getDstBuffer fail, pipeReprocessingDst(%d), ret(%d)",
                    pipeReprocessingDst, ret);
            goto CLEAN;
        }

        /* get preview callback heap */
        camera_memory_t *previewCallbackHeap = NULL;
        previewCallbackHeap = m_getMemoryCb(reprocessingDstBuffer.fd[0], reprocessingDstBuffer.size[0], 1, m_callbackCookie);
        if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%d) fail", reprocessingDstBuffer.size[0]);
            goto CLEAN;
        }

        CLOGV("high resolution preview callback");
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    pipeReprocessingDst, ret);
            goto CLEAN;
        }
    } else {
        CLOGD(" Preview callback message disabled, skip callback");
        ret = newFrame->getDstBuffer(pipeReprocessingSrc, &reprocessingDstBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeReprocessingDst));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeReprocessingDst(%d), ret(%d)",
                    pipeReprocessingDst, ret);
            goto CLEAN;
        }

        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    pipeReprocessingDst, ret);
            goto CLEAN;
        }
    }

    if (newFrame != NULL) {
        newFrame->frameUnlock();

        CLOGD(" Reprocessing frame delete(%d)", newFrame->getFrameCount());
        newFrame = NULL;
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("high resolution callback thread complete, loop(%d)", loop);

    /* one shot */
    return loop;

CLEAN:

    if (reprocessingDstBuffer.index != -2) {
        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    pipeReprocessingDst, ret);
            goto CLEAN;
        }
    }

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();

        ret = m_deleteFrame(newFrame);
        if (ret < 0) {
            CLOGE("m_deleteFrame fail, ret(%d)", ret);
        }
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
                m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("high resolution callback thread fail, loop(%d)", loop);

    /* one shot */
    return loop;
}

bool ExynosCamera::m_facedetectThreadFunc(void)
{
    int32_t status = 0;
    bool loop = true;

    int index = 0;
    int count = 0;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    uint32_t frameCnt = 0;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
    uint32_t pipeId = PIPE_VC1;
    int ret = 0;
#endif

    if (m_previewEnabled == false) {
        CLOGD("preview is stopped, thread stop");
        loop = false;
        goto func_exit;
    }

    status = m_facedetectQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d) m_flagStartFaceDetection(%d)", m_flagThreadStop, m_flagStartFaceDetection);
        loop = false;
        goto func_exit;
    }

    if (status < 0) {
        if (status == TIMED_OUT) {
            if (m_flagHoldFaceDetection == false) {
                /* Face Detection time out is not meaningful */
                CLOGW("m_facedetectQ time out, status(%d)", status);
            }
        } else {
            /* TODO: doing exception handling */
            CLOGE("wait and pop fail, status(%d)", status);
        }
        goto func_exit;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getDepthCallbackOnPreview()) {
        status = m_depthCallbackQ->waitAndPopProcessQ(&depthMapBuffer);
        if (status < 0) {
            if (status == TIMED_OUT) {
                /* Face Detection time out is not meaningful */
                CLOGE("m_depthCallbackQ time out, status(%d)", status);
            } else {
                /* TODO: doing exception handling */
                CLOGE("wait and pop fail, status(%d)", status);
            }
        }

        if (depthMapBuffer.index != -2) {
            newFrame->setRequest(pipeId, true);
            ret = newFrame->setDstBuffer(pipeId, depthMapBuffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get DepthMap buffer");
            }
        }
    }
#endif

    count = m_facedetectQ->getSizeOfProcessQ();
    if (count >= MAX_FACEDETECT_THREADQ_SIZE) {
        if (newFrame != NULL) {
            CLOGE("m_facedetectQ  skipped QSize(%d) frame(%d)",
                     count, newFrame->getFrameCount());

#ifdef SUPPORT_DEPTH_MAP
            if (newFrame->getRequest(pipeId) == true) {
                depthMapBuffer.index = -2;
                ret = newFrame->getDstBuffer(pipeId, &depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to put DepthMap buffer to bufferMgr");
                }
            }
#endif

            newFrame = NULL;
        }
        for (int i = 0 ; i < count-1 ; i++) {
            m_facedetectQ->popProcessQ(&newFrame);
            if (newFrame != NULL) {
#ifdef SUPPORT_DEPTH_MAP
                if (newFrame->getRequest(pipeId) == true) {
                    depthMapBuffer.index = -2;
                    ret = newFrame->getDstBuffer(pipeId, &depthMapBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to get DepthMap buffer");
                    }

                    ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to put DepthMap buffer to bufferMgr");
                    }
                }
#endif

                newFrame = NULL;
            }
        }
        m_facedetectQ->popProcessQ(&newFrame);
    }

    if (newFrame != NULL) {
        status = m_doFdCallbackFunc(newFrame);
        if (status < 0) {
#ifdef SUPPORT_DEPTH_MAP
            if (newFrame->getRequest(pipeId) == true) {
                depthMapBuffer.index = -2;
                ret = newFrame->getDstBuffer(pipeId, &depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to put DepthMap buffer to bufferMgr");
                }
            }
#endif
            CLOGE(" m_doFdCallbackFunc failed(%d).", status);
        }
    }

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        loop = true;
    }

    if (newFrame != NULL) {
        newFrame = NULL;
    }

func_exit:

    return loop;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainThread = new mainCameraThread(this, &ExynosCamera::m_mainThreadFunc, "ExynosCameraThread", PRIORITY_URGENT_DISPLAY);
    CLOGV("mainThread created");

    m_previewThread = new mainCameraThread(this, &ExynosCamera::m_previewThreadFunc, "previewThread", PRIORITY_DISPLAY);
    CLOGV("previewThread created");

    /*
     * In here, we cannot know single, dual scenario.
     * So, make all threads.
     */
    /* if (m_parameters->isFlite3aaOtf() == true) { */
    if (1) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupFLITE, "mainThreadQSetupFLITE", PRIORITY_URGENT_DISPLAY);
        CLOGV("mainThreadQSetupFLITEThread created");

/* Change 3AA_ISP, 3AC, SCP to ISP */
/*
        m_mainSetupQThread[INDEX(PIPE_3AC)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AC, "mainThreadQSetup3AC", PRIORITY_URGENT_DISPLAY);
        CLOGD("mainThreadQSetup3ACThread created");

        m_mainSetupQThread[INDEX(PIPE_3AA_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA_ISP, "mainThreadQSetup3AA_ISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("mainThreadQSetup3AA_ISPThread created");

        m_mainSetupQThread[INDEX(PIPE_ISP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupISP, "mainThreadQSetupISP", PRIORITY_URGENT_DISPLAY);
        CLOGD("mainThreadQSetupISPThread created");

        m_mainSetupQThread[INDEX(PIPE_SCP)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupSCP, "mainThreadQSetupSCP", PRIORITY_URGENT_DISPLAY);
        CLOGD("mainThreadQSetupSCPThread created");
*/

        m_mainSetupQThread[INDEX(PIPE_3AA)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetup3AA, "mainThreadQSetup3AA", PRIORITY_URGENT_DISPLAY);
        CLOGV("mainThreadQSetup3AAThread created");

    }
    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGV("setBuffersThread created");

    m_startPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_startPictureInternalThreadFunc, "startPictureInternalThread");
    CLOGV("startPictureInternalThread created");

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGV("startPictureBufferThread created");

    m_prePictureThread = new mainCameraThread(this, &ExynosCamera::m_prePictureThreadFunc, "prePictureThread");
    CLOGV("prePictureThread created");

    m_pictureThread = new mainCameraThread(this, &ExynosCamera::m_pictureThreadFunc, "PictureThread");
    CLOGV("pictureThread created");

    m_postPictureThread = new mainCameraThread(this, &ExynosCamera::m_postPictureThreadFunc, "postPictureThread");
    CLOGV("postPictureThread created");

    m_recordingThread = new mainCameraThread(this, &ExynosCamera::m_recordingThreadFunc, "recordingThread");
    CLOGV("recordingThread created");

    m_autoFocusThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusThreadFunc, "AutoFocusThread");
    CLOGV("autoFocusThread created");

    m_autoFocusContinousThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusContinousThreadFunc, "AutoFocusContinousThread");
    CLOGV("autoFocusContinousThread created");

    m_facedetectThread = new mainCameraThread(this, &ExynosCamera::m_facedetectThreadFunc, "FaceDetectThread");
    CLOGV("FaceDetectThread created");

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "monitorThread");
    CLOGV("monitorThread created");

    m_jpegCallbackThread = new mainCameraThread(this, &ExynosCamera::m_jpegCallbackThreadFunc, "jpegCallbackThread");
    CLOGV("jpegCallbackThread created");

    m_gdcThread = new mainCameraThread(this, &ExynosCamera::m_gdcThreadFunc, "gdcQThread");
    CLOGV("gdcQThread created");

    m_zoomPreviwWithCscThread = new mainCameraThread(this, &ExynosCamera::m_zoomPreviwWithCscThreadFunc, "zoomPreviwWithCscQThread");
    CLOGV("zoomPreviwWithCscQThread created");

    /* saveThread */
    char threadName[20];
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        snprintf(threadName, sizeof(threadName), "jpegSaveThread%d", threadNum);
        m_jpegSaveThread[threadNum] = new mainCameraThread(this, &ExynosCamera::m_jpegSaveThreadFunc, threadName);
        CLOGV("%s created", threadName);
    }

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    CLOGV("highResolutionCallbackThread created");


    /* Shutter callback */
    m_shutterCallbackThread = new mainCameraThread(this, &ExynosCamera::m_shutterCallbackThreadFunc, "shutterCallbackThread");
    CLOGV("shutterCallbackThread created");
}

status_t  ExynosCamera::m_setConfigInform() {
    struct ExynosConfigInfo exynosConfig;
    memset((void *)&exynosConfig, 0x00, sizeof(exynosConfig));

    exynosConfig.mode = CONFIG_MODE::NORMAL;

    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? NUM_SENSOR_BUFFERS : FRONT_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? NUM_BAYER_BUFFERS : FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? NUM_3AA_BUFFERS : FRONT_NUM_3AA_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? NUM_PICTURE_BUFFERS : FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hiding_mode_buffers = NUM_HIDING_BUFFER_COUNT;

    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = (getCameraIdInternal() == CAMERA_ID_BACK) ? PIPE_FLITE_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = (getCameraIdInternal() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = (getCameraIdInternal() == CAMERA_ID_BACK) ? PIPE_3AA_ISP_PREPARE_COUNT : PIPE_FLITE_FRONT_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = (getCameraIdInternal() == CAMERA_ID_BACK) ? PIPE_SCP_PREPARE_COUNT : PIPE_SCP_FRONT_PREPARE_COUNT;

    /* reprocessing */
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
    /* Config HIGH_SPEED 60 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS60_NUM_NUM_SENSOR_BUFFERS : FPS60_FRONT_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS60_NUM_NUM_BAYER_BUFFERS : FPS60_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS60_NUM_PICTURE_BUFFERS : FPS60_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hiding_mode_buffers = FPS60_NUM_HIDING_BUFFER_COUNT;

    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_REPROCESSING_PREPARE_COUNT;

    /* Config HIGH_SPEED 120 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_sensor_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS120_NUM_NUM_SENSOR_BUFFERS : FPS120_FRONT_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_bayer_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS120_NUM_NUM_BAYER_BUFFERS : FPS120_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = FPS120_INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_3aa_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hwdis_buffers = FPS120_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_preview_buffers = FPS120_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_picture_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS120_NUM_PICTURE_BUFFERS : FPS120_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_reprocessing_buffers = FPS120_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_recording_buffers = FPS120_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_fastaestable_buffer = FPS120_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.reprocessing_bayer_hold_count = FPS120_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.preview_buffer_margin = FPS120_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_hiding_mode_buffers = FPS120_NUM_HIDING_BUFFER_COUNT;

    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_FLITE] = FPS120_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AC] = FPS120_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_3AA_ISP] = FPS120_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP] = FPS120_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS120_PIPE_SCP_REPROCESSING_PREPARE_COUNT;

    /* Config HIGH_SPEED 240 buffer & pipe info */
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_sensor_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS240_NUM_NUM_SENSOR_BUFFERS : FPS240_FRONT_NUM_SENSOR_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_bayer_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS240_NUM_NUM_BAYER_BUFFERS : FPS240_FRONT_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.init_bayer_buffers = FPS240_INIT_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_3aa_buffers = FPS240_NUM_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_hwdis_buffers = FPS240_NUM_HW_DIS_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_preview_buffers = FPS240_NUM_PREVIEW_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_picture_buffers = (getCameraIdInternal() == CAMERA_ID_BACK) ? FPS240_NUM_PICTURE_BUFFERS : FPS240_FRONT_NUM_PICTURE_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_reprocessing_buffers = FPS240_NUM_REPROCESSING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_recording_buffers = FPS240_NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_fastaestable_buffer = FPS240_INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.reprocessing_bayer_hold_count = FPS240_REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.preview_buffer_margin = FPS240_NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_hiding_mode_buffers = FPS240_NUM_HIDING_BUFFER_COUNT;

    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_FLITE] = FPS240_PIPE_FLITE_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AC] = FPS240_PIPE_3AC_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA] = FPS240_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_3AA_ISP] = FPS240_PIPE_3AA_ISP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_SCP] = FPS240_PIPE_SCP_PREPARE_COUNT;
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS240_PIPE_SCP_REPROCESSING_PREPARE_COUNT;
#endif

    m_parameters->setConfig(&exynosConfig);

    m_exynosconfig = m_parameters->getConfig();

    return NO_ERROR;
}

status_t ExynosCamera::m_setFrameManager()
{
    sp<FrameWorker> worker;

    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 200);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new RunWorker("RUNNING FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 100, 300);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::RUNNING, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}

void ExynosCamera::m_setFrameManagerInfo()
{
    sp<FrameWorker> worker;
    worker = m_frameMgr->getWorker(FRAMEMGR_WORKER::CREATE);
    if (worker == NULL) {
        CLOGD("getWorker(%d) fail, skip update FrameManagerInfo", FRAMEMGR_WORKER::CREATE);
        return;
    }

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        worker->setMargin(300, 200);
    } else {
        worker->setMargin(200, 100);
    }
}

status_t ExynosCamera::m_initFrameFactory(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    CLOGI(" -IN-");

    status_t ret = NO_ERROR;

    m_previewFrameFactory = NULL;
    m_pictureFrameFactory = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory = NULL;

    /*
     * new all FrameFactories.
     * because this called on open(). so we don't know current scenario
     */

    m_previewFrameFactory = new ExynosCameraFrameFactoryPreview(m_cameraId, m_parameters);
    m_previewFrameFactory->setFrameManager(m_frameMgr);

    m_reprocessingFrameFactory = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_parameters);
    m_reprocessingFrameFactory->setFrameManager(m_frameMgr);

    CLOGI(" -OUT-");
    return ret;
}

status_t ExynosCamera::m_deinitFrameFactory(void)
{
    CLOGI(" -IN-");

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_previewFrameFactory != NULL
        && m_previewFrameFactory->isCreated() == true) {
        ret = m_previewFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("m_previewFrameFactory destroy fail");
    }

    if (m_reprocessingFrameFactory != NULL
        && m_reprocessingFrameFactory->isCreated() == true) {
        ret = m_reprocessingFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("m_reprocessingFrameFactory destroy fail");
    }

    if (m_visionFrameFactory != NULL
        && m_visionFrameFactory->isCreated() == true) {
        ret = m_visionFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("m_visionFrameFactory destroy fail");
    }

    SAFE_DELETE(m_previewFrameFactory);
    CLOGD("m_previewFrameFactory destroyed");
    SAFE_DELETE(m_reprocessingFrameFactory);
    CLOGD("m_reprocessingFrameFactory destroyed");
    /*
       COUTION: m_pictureFrameFactory is only a pointer, just refer preview or reprocessing frame factory's instance.
       So, this pointer shuld be set null when camera HAL is deinitializing.
     */
    m_pictureFrameFactory = NULL;
    CLOGD("m_pictureFrameFactory destroyed");
    SAFE_DELETE(m_visionFrameFactory);
    CLOGD("m_visionFrameFactory destroyed. --OUT--");

    return NO_ERROR;
}

status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("alloc buffer - camera ID: %d",
        m_cameraId);

    status_t ret = NO_ERROR;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW = 0, hwPreviewH = 0, hwPreviewFormat = 0;
    int hwPictureW = 0, hwPictureH = 0;

    int ispBufferW = 0, ispBufferH = 0;
    int tpuBufferW = 0, tpuBufferH = 0;
    int previewMaxW = 0, previewMaxH = 0;
    int pictureMaxW = 0, pictureMaxH = 0;
    int sensorMaxW = 0, sensorMaxH = 0;
    ExynosRect bdsRect;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;
    int batchSize = 1;
    int maxBufferCount = 1;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bool needMmap = false;

    if (m_parameters->getReallocBuffer() == true) {
        /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
        m_releasebuffersForRealloc();
    }

    m_parameters->getPreviewBdsSize(&bdsRect, false);

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    CLOGI("HW Preview width x height = %dx%d",
            hwPreviewW, hwPreviewH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    CLOGI("HW Picture width x height = %dx%d",
            hwPictureW, hwPictureH);
    m_parameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("Picture MAX width x height = %dx%d",
            pictureMaxW, pictureMaxH);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("HW Sensor(HighSpeed) MAX width x height = %dx%d",
                sensorMaxW, sensorMaxH);
        m_parameters->getHwPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("HW Preview(HighSpeed) MAX width x height = %dx%d",
                previewMaxW, previewMaxH);
    } else {
        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("Sensor MAX width x height = %dx%d",
                sensorMaxW, sensorMaxH);
        m_parameters->getMaxPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("Preview MAX width x height = %dx%d",
                previewMaxW, previewMaxH);
    }

    hwPreviewFormat = m_parameters->getHwPreviewFormat();

    int stride = m_scpBufferMgr->getBufStride();
    if (stride != hwPreviewW) {
        CLOGI("hwPreviewW(%d), stride(%d)", hwPreviewW, stride);
        if (stride == 0) {
            /* If the SCP buffer manager is not instance of GrallocExynosCameraBufferManager
               (In case of setPreviewWindow(null) is called), return value of setHwPreviewStride()
               will be zero. If this value is passed as SCP width to firmware, firmware will
               generate PABORT error. */

            int strideAlign = 16;

            switch (hwPreviewFormat) {
            case V4L2_PIX_FMT_NV21M:
#if GRALLOC_CAMERA_64BYTE_ALIGN
                strideAlign = 64;
#else
                strideAlign = 1;
#endif
                break;
            case V4L2_PIX_FMT_NV21:
                strideAlign = 1;
                break;
            case V4L2_PIX_FMT_YVU420M:
                strideAlign = 32;
                break;
            default:
                strideAlign = 16;
                break;
            }
            CLOGW("HACK: Invalid stride(%d). It will be replaced as hwPreviewW(%d) value, by %c%c%c%c strideAlign(%d) ",
                stride, ALIGN(hwPreviewW, strideAlign),
                v4l2Format2Char(hwPreviewFormat, 0),
                v4l2Format2Char(hwPreviewFormat, 1),
                v4l2Format2Char(hwPreviewFormat, 2),
                v4l2Format2Char(hwPreviewFormat, 3),
                strideAlign);

            stride = ALIGN(hwPreviewW, strideAlign);
        }

        if (hwPreviewW < stride) {
            /* stride is 16 align. but hwPrevieW can be differnt.
               if hwPreviewW is smaller than stride(setFmt size), qBuf() fail. (becuase of the lack of buffer size)
               so, alloc buffer as stride size. */
            CLOGW("hwPreviewW(%d) < stride(%d). so, just set hwPreviewW = stride",
                hwPreviewW, stride);
            hwPreviewW = stride;
        }
    }

#if (SUPPORT_BACK_HW_VDIS || SUPPORT_FRONT_HW_VDIS)
    /*
     * we cannot expect TPU on or not, when open() api.
     * so extract memory TPU size
     */
    int w = 0, h = 0;
    m_parameters->calcNormalToTpuSize(previewMaxW, previewMaxH, &w, &h);
    if (ret != NO_ERROR) {
        CLOGE("Hw vdis buffer calulation fail src(%d x %d) dst(%d x %d)", previewMaxW, previewMaxH, w, h);
    }
    previewMaxW = w;
    previewMaxH = h;
    CLOGI("TPU based Preview MAX width x height = %dx%d", previewMaxW, previewMaxH);
#endif

    /* Depth map buffer */
#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getUseDepthMap()) {
        int depthMapW = 0, depthMapH = 0;
        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDepthMapSize.");
            return ret;
        } else {
            CLOGI("Depth Map Size width x height = %dx%d",
                    depthMapW, depthMapH);
        }

        bayerFormat     = DEPTH_MAP_FORMAT;
        bytesPerLine[0] = getBayerLineSize(depthMapW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(depthMapW, depthMapH, bayerFormat);
        planeCount      = 2;
        batchSize       = m_parameters->getBatchSize(PIPE_VC1);
        maxBufferCount  = NUM_DEPTHMAP_BUFFERS;
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_depthMapBufferMgr,
                planeCount, planeSize, bytesPerLine,
                maxBufferCount, maxBufferCount, batchSize,
                type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("Failed to allocate Depth Map Buffers. bufferCount %d",
                    maxBufferCount);
            return ret;
        }

        CLOGI("m_allocBuffers(DepthMap Buffer) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            depthMapW, depthMapH,
            planeSize[0], planeCount, maxBufferCount);
    }
#endif

    /* 3AA source(for shot) Buffer */
    planeCount      = 2;
    planeSize[0]    = 32 * 64 * 2;
    batchSize       = m_parameters->getBatchSize(PIPE_3AA);
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, batchSize, true, false);
    if (ret != NO_ERROR) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            maxBufferCount);
        return ret;
    }

    /* 3AP to ISP Buffer */
    if (m_parameters->is3aaIspOtf() == false
#ifdef USE_3AA_ISP_BUFFER_HIDING_MODE
        || (USE_3AA_ISP_BUFFER_HIDING_MODE == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
#endif
       ) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        ispBufferW = bdsRect.w;
        ispBufferH = bdsRect.h;
#else
        ispBufferW = previewMaxW;
        ispBufferH = previewMaxH;
#endif

        bayerFormat     = m_parameters->getBayerFormat(PIPE_3AP);
        bytesPerLine[0] = getBayerLineSize(ispBufferW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(ispBufferW, ispBufferH, bayerFormat);
        planeCount      = 2;
        batchSize       = m_parameters->getBatchSize(PIPE_3AP);
#ifdef USE_3AA_ISP_BUFFER_HIDING_MODE
        if (USE_3AA_ISP_BUFFER_HIDING_MODE == true
            && m_parameters->is3aaIspOtf() == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            maxBufferCount = m_exynosconfig->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
            maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;

#ifdef RESERVED_MEMORY_ENABLE
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS_ON_UHD);
            else
                m_ispBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS);
        } else if (m_parameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS_ON_UHD);
            else
                m_ispBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
        } else
#endif
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, maxBufferCount, batchSize, type, true, false);
        if (ret != NO_ERROR) {
            CLOGE("m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    maxBufferCount);
            return ret;
        }

        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);

        CLOGI("m_allocBuffers(ISP Buffer) %d x %d, planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                ispBufferW, ispBufferH, planeSize[0], planeCount, maxBufferCount);
    }

    /* ISP to TPU(MCSC) Buffer */
    if ((m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false)
#ifdef USE_ISP_TPU_BUFFER_HIDING_MODE
        || (USE_ISP_TPU_BUFFER_HIDING_MODE == true
            && m_parameters->isIspTpuOtf() == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
#endif
#ifdef USE_ISP_MCSC_BUFFER_HIDING_MODE
        || (USE_ISP_MCSC_BUFFER_HIDING_MODE == true
            && m_parameters->isIspMcscOtf() == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
#endif
       ) {
        int ispDmaOutAlignW = CAMERA_16PX_ALIGN;
        int ispDmaOutAlignH = 1;

#ifdef TPU_INPUT_CHUNK_TYPE
        if (m_parameters->getTpuEnabledMode() == true) {
            ispDmaOutAlignW = CAMERA_TPU_CHUNK_ALIGN_W;
            ispDmaOutAlignH = CAMERA_TPU_CHUNK_ALIGN_H;
        }
#endif

#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        tpuBufferW = bdsRect.w;
        tpuBufferH = bdsRect.h;
#else
        tpuBufferW = previewMaxW;
        tpuBufferH = previewMaxH;
#endif

        planeCount      = 2;
        batchSize       = m_parameters->getBatchSize(PIPE_ISP);
        bytesPerLine[0] = ROUND_UP((tpuBufferW * 2), ispDmaOutAlignW);
        planeSize[0]    = bytesPerLine[0] * ROUND_UP(tpuBufferH, ispDmaOutAlignH);
#ifdef USE_ISP_TPU_BUFFER_HIDING_MODE
        if (USE_ISP_TPU_BUFFER_HIDING_MODE == true
            && m_parameters->isIspTpuOtf() == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            maxBufferCount = m_exynosconfig->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
#ifdef USE_ISP_MCSC_BUFFER_HIDING_MODE
        if (USE_ISP_MCSC_BUFFER_HIDING_MODE == true
            && m_parameters->isIspMcscOtf() == true
            && (m_cameraId == CAMERA_ID_BACK
                || m_parameters->getDualMode() == false))
            maxBufferCount = m_exynosconfig->current->bufInfo.num_hiding_mode_buffers;
        else
#endif
            maxBufferCount  = m_exynosconfig->current->bufInfo.num_hwdis_buffers;

#ifdef RESERVED_MEMORY_ENABLE
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_hwDisBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS_ON_UHD);
            else
                m_hwDisBufferMgr->setContigBufCount(RESERVED_NUM_ISP_BUFFERS);
        } else if (m_parameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_hwDisBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS_ON_UHD);
            else
                m_hwDisBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_ISP_BUFFERS);
        } else
#endif
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_hwDisBufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, maxBufferCount, batchSize, type, true, false);
        if (ret != NO_ERROR) {
            CLOGE("m_hwDisBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    maxBufferCount);
            return ret;
        }

#ifdef CAMERA_PACKED_BAYER_ENABLE
        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

        CLOGI("m_allocBuffers(hwDis Buffer) %d x %d, planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                tpuBufferW, tpuBufferH, planeSize[0], planeCount, maxBufferCount);
    }

    /* Preview(MCSC0) Buffer */
    memset(planeSize, 0, sizeof(planeSize));

    ret = getYuvFormatInfo(hwPreviewFormat, &bpp, &planeCount);
    if (ret < 0) {
        CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
            hwPreviewFormat);
        return INVALID_OPERATION;
    }

    // for meta
    planeCount += 1;
    batchSize   = m_parameters->getBatchSize(PIPE_MCSC0);

    if(m_parameters->increaseMaxBufferOfPreview() == true)
        maxBufferCount = m_parameters->getPreviewBufferCount();
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;

    if (m_previewWindow == NULL)
        needMmap = true;
    else
        needMmap = false;

    ret = getYuvPlaneSize(hwPreviewFormat, planeSize, hwPreviewW, hwPreviewH);
    if (ret < 0) {
        CLOGE("getYuvPlaneSize(hwPreviewFormat(%x), hwPreview(%d x %d)) fail",
                hwPreviewFormat, hwPreviewW, hwPreviewH);
        return INVALID_OPERATION;
    }

    {
        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, hwPreviewW, hwPreviewH);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(hwPreviewFormat(%x), hwPreview(%d x %d)) fail",
                hwPreviewFormat, hwPreviewW, hwPreviewH);
            return INVALID_OPERATION;
        }
        if (m_previewWindow == NULL) {
            ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, maxBufferCount, batchSize,
                         EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE, true, needMmap);
        } else {
            ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, batchSize, true, needMmap);
        }
    }

    if (ret != NO_ERROR) {
        CLOGE("m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            maxBufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(Preview Buffer) %d x %d, planeSize(%d+%d), planeCount(%d), maxBufferCount(%d)",
            hwPreviewW, hwPreviewH, planeSize[0], planeSize[1], planeCount, maxBufferCount);

    /* for zoom scaling by external scaler */
    /*
     * only alloc in back case.
     * dual's front doesn't need m_zoomScalerBufferMgr, by src(m_yuvBufferMgr), dst(m_scpBufferMgr)
     */
    /* m_zoomScalerBufferMgr integrated into m_scpBufferMgr, to reduce memory */
#if 0
    if ((m_parameters->getSupportedZoomPreviewWIthScaler()) &&
        ! ((m_parameters->getDualMode() == true) &&
           (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1))) {
        int scalerW = hwPreviewW;
        int scalerH = hwPreviewH;

        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, scalerW, scalerH);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(hwPreviewFormat(%x), scaler(%d x %d)) fail",
                hwPreviewFormat, scalerW, scalerH);
            return INVALID_OPERATION;
        }

        ret = getYuvFormatInfo(hwPreviewFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                hwPreviewFormat);
            return INVALID_OPERATION;
        }

        // for meta
        planeCount += 1;

        if(m_parameters->increaseMaxBufferOfPreview()){
            maxBufferCount = m_parameters->getPreviewBufferCount();
        } else {
            maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        }

        bool needMmap = false;
        if (m_previewWindow == NULL)
            needMmap = true;

        {
            ret = m_allocBuffers(m_zoomScalerBufferMgr, planeCount, planeSize, bytesPerLine,
                                 maxBufferCount, batchSize, true, needMmap);
        }
        if (ret != NO_ERROR) {
            CLOGE("m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    }
#endif

    m_parameters->setHwPreviewStride(stride);

    /* Do not allocate SCC buffer */
    if (m_parameters->isReprocessingCapture() == false) {
        m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
        CLOGI("HW Picture width x height = %dx%d", hwPictureW, hwPictureH);
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

        batchSize = m_parameters->getBatchSize(PIPE_SCC);
        /* TO DO : make same num of buffers */
        if (m_parameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

        ret = m_allocBuffers(m_yuvBufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, batchSize, true, false);
        if (ret < 0) {
            CLOGE("m_yuvBufferMgr m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    }

    ret = m_setVendorBuffers();
    if (ret != NO_ERROR)
        CLOGE("m_setVendorBuffers fail, ret(%d)",
                ret);

    ret = m_setPreviewCallbackBuffer();
    if (ret != NO_ERROR) {
        CLOGE("m_setPreviewCallback Buffer fail");
        return ret;
    }

    CLOGI("alloc buffer done - camera ID: %d",
        m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setPictureBuffer(void)
{
    status_t ret = NO_ERROR;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int pictureW = 0, pictureH = 0;
    int planeCount = 0;
    int batchSize = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int pictureFormat = m_parameters->getHwPictureFormat();

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxPictureSize(&pictureW, &pictureH);
    if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
#if defined(USE_ODC_CAPTURE)
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
        if (m_parameters->getODCCaptureEnable() == true) {
#if defined(GDC_COLOR_FMT)
            pictureFormat = GDC_COLOR_FMT;
#else
            pictureFormat = V4L2_PIX_FMT_NV12M;
#endif /* GDC_COLOR_FMT */
        }
#endif /* CAMERA_HAS_OWN_GDC */
#endif /* USE_ODC_CAPTURE */

        switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
        case V4L2_PIX_FMT_NV12M:
            planeCount      = 2;
#if defined(USE_ODC_CAPTURE)
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
            /*
             * Single capture use GDC (YUYV).
             * but, Burst capture doesn't use GDC, it is NV12/21.
             * this scenario set on capture time(after startPreview).
             * so, just alloc Y plane as YUYV.
             */
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(V4L2_PIX_FMT_YUYV), pictureW, pictureH);
#else
            planeSize[0]    = pictureW * pictureH;
#endif // CAMERA_HAS_OWN_GDC
#endif // USE_ODC_CAPTURE
            planeSize[1]    = pictureW * pictureH / 2;
            break;
        default:
            planeCount      = 1;
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
            break;
        }

        batchSize = m_parameters->getBatchSize(PIPE_GSC_PICTURE);

        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_parameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
        else
            minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize, type, allocMode,
                             false, false);
        if (ret != NO_ERROR) {
            CLOGE("m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                minBufferCount, maxBufferCount);
            return ret;
        }

        CLOGI("m_allocBuffers(GSC Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                pictureW, pictureH, planeCount, maxBufferCount);

    }

    if (m_hdrEnabled == false) {
        pictureFormat = m_parameters->getHwPictureFormat();

        planeCount      = 1;
        batchSize       = m_parameters->getBatchSize(PIPE_JPEG);
        planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_parameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
        else
            minBufferCount  = 1;
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_picture_buffers;

        CLOGD(" jpegBuffer picture(%dx%d) size(%d)", pictureW, pictureH, planeSize[0]);

#ifdef RESERVED_MEMORY_ENABLE
        if (getCameraIdInternal() == CAMERA_ID_BACK
#ifdef RESERVED_MEMORY_20M_WORKAROUND
        && m_cameraSensorId != SENSOR_NAME_S5K2T2
#endif
        ) {
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
            if (m_parameters->getUHDRecordingMode() == true) {
                m_jpegBufferMgr->setContigBufCount(RESERVED_NUM_JPEG_BUFFERS_ON_UHD);
            } else {
                m_jpegBufferMgr->setContigBufCount(RESERVED_NUM_JPEG_BUFFERS);

                /* alloc at once */
                minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
            }
        } else
#endif
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

        ret = m_allocBuffers(m_jpegBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize, type, allocMode,
                             false, true);
        if (ret != NO_ERROR)
            CLOGE("jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                    NUM_REPROCESSING_BUFFERS);

        CLOGI("m_allocBuffers(JPEG Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                pictureW, pictureH, planeCount, maxBufferCount);
    }

    ret = m_setVendorPictureBuffer();
    if (ret != NO_ERROR)
        CLOGE("m_setVendorBuffers fail, ret(%d)",
                ret);

    return ret;
}

status_t ExynosCamera::m_releaseBuffers(void)
{
    CLOGI("release buffer");
    int ret = 0;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }

#if defined(DEBUG_RAWDUMP)
    if (m_fliteBufferMgr != NULL) {
        m_fliteBufferMgr->deinit();
    }
#endif

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
    if (m_yuvReprocessingBufferMgr != NULL) {
        m_yuvReprocessingBufferMgr->deinit();
    }
    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }
    if (m_yuvBufferMgr != NULL) {
        m_yuvBufferMgr->deinit();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }

    if (m_postPictureBufferMgr != NULL) {
        m_postPictureBufferMgr->deinit();
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
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

    ret = m_releaseVendorBuffers();
    if (ret != NO_ERROR)
        CLOGE("m_releaseVendorBuffers fail, ret(%d)",
                ret);

    CLOGI("free buffer done");

    return NO_ERROR;
}

bool ExynosCamera::m_startFaceDetection(void)
{
    if (m_flagStartFaceDetection == true) {
        CLOGD("Face detection already started..");
        return true;
    }

    if (m_parameters->getDualMode() == true
        && getCameraIdInternal() == CAMERA_ID_BACK
       ) {
        CLOGW("On dual mode, Face detection disable!");
        m_enableFaceDetection(false);
        m_flagStartFaceDetection = false;
    } else
    {
        /* FD-AE is always on */
#ifdef USE_FD_AE
#else
        m_enableFaceDetection(true);
#endif

        /* Block FD-AF except for special shot modes */
        if(m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE ||
                m_parameters->getShotMode() == SHOT_MODE_SELFIE_ALARM) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

            if (autoFocusMgr->setFaceDetection(true) == false) {
                CLOGE("setFaceDetection(%d)", true);
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
        }

        if (m_facedetectQ->getSizeOfProcessQ() > 0) {
            CLOGE("startFaceDetection recordingQ(%d)", m_facedetectQ->getSizeOfProcessQ());
            m_clearList(m_facedetectQ);
        }

        m_flagStartFaceDetection = true;

        if (m_facedetectThread->isRunning() == false)
            m_facedetectThread->run();
    }

    return true;
}

bool ExynosCamera::m_stopFaceDetection(void)
{
    if (m_flagStartFaceDetection == false) {
        CLOGD("Face detection already stopped..");
        return true;
    }

    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    if (autoFocusMgr->setFaceDetection(false) == false) {
        CLOGE("setFaceDetection(%d)", false);
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
    m_enableFaceDetection(false);
#endif
    m_flagStartFaceDetection = false;

    return true;
}

status_t ExynosCamera::m_setPreviewCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_parameters->getHwPreviewSize(&previewW, &previewH);
    } else {
        m_parameters->getPreviewSize(&previewW, &previewH);
    }

    previewFormat = m_parameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int bufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    int planeCount  = getYuvPlaneCount(previewFormat);
    planeCount++;
    int batchSize   = 1;

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    batchSize   = m_parameters->getBatchSize(PIPE_MCSC1);
    bufferCount = m_parameters->getPreviewBufferCount();
#endif

    if (m_previewCallbackBufferMgr == NULL) {
        CLOGE(" m_previewCallbackBufferMgr is NULL");
        return INVALID_OPERATION;
    }

    if (m_previewCallbackBufferMgr->isAllocated() == true) {
        if (m_parameters->getRestartPreview() == true) {
            CLOGD(" preview size is changed, realloc buffer");
            m_previewCallbackBufferMgr->deinit();
        } else {
            return NO_ERROR;
        }
    }

    ret = getYuvPlaneSize(previewFormat, planeSize, previewW, previewH);
    if (ret < 0) {
        CLOGE(" BAD value, format(%x), size(%dx%d)",
            previewFormat, previewW, previewH);
        return ret;
    }

    ret = m_allocBuffers(m_previewCallbackBufferMgr,
                        planeCount,
                        planeSize,
                        bytesPerLine,
                        bufferCount,
                        bufferCount,
                        batchSize,
                        type,
                        true,
                        false);
    if (ret < 0) {
        CLOGE("m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d/%d) fail",
            bufferCount, bufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(Preview Callback Buffer) %d x %d, planeSize(%d), planeCount(%d), bufferCount(%d)",
            previewW, previewH, planeSize[0], planeCount, bufferCount);

    return NO_ERROR;
}

status_t ExynosCamera::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufManager != NULL) {
        ret = bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret != NO_ERROR)
            CLOGE("Buffer manager putBuffer fail, index(%d), ret(%d)",
                    bufIndex, ret);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        int batchSize,
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
                batchSize,
                EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE,
                BUFFER_MANAGER_ALLOCATION_ATONCE,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("m_allocBuffers(reqBufCount=%d) fail",
            reqBufCount);
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
        int batchSize,
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
                batchSize,
                type,
                BUFFER_MANAGER_ALLOCATION_ONDEMAND,
                createMetaPlane,
                needMmap);
    if (ret < 0) {
        CLOGE("m_allocBuffers(minBufCount=%d, maxBufCount=%d, type=%d) fail",
            minBufCount, maxBufCount, type);
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
        int batchSize,
        exynos_camera_buffer_type_t type,
        buffer_manager_allocation_mode_t allocMode,
        bool createMetaPlane,
        bool needMmap)
{
    int ret = 0;
	int retryCount = 20; /* 2Sec */

	if (bufManager == NULL) {
		CLOGE("ERR(%s[%d]):BufferManager is NULL", __FUNCTION__, __LINE__);
		ret = BAD_VALUE;
		goto func_exit;
	}

retry_alloc:

    CLOGI("setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, batchSize=%d, type=%d, allocMode=%d)",
        planeCount, minBufCount, maxBufCount, batchSize, (int)type, (int)allocMode);

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        0 /* startBufIndex */,
                        minBufCount,
                        maxBufCount,
                        batchSize,
                        type,
                        allocMode,
                        createMetaPlane,
                        needMmap);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        if (retryCount != 0
            && (type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE
                || type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE)) {
            CLOGE("Alloc fail about reserved memory. retry alloc. (%d)", retryCount);
            usleep(100000); /* 100ms */
            retryCount--;
            goto retry_alloc;
        }
        CLOGE("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_allocBuffers(
        ExynosCameraBufferManager *bufManager,
        int  planeCount,
        unsigned int *planeSize,
        unsigned int *bytePerLine,
        int  reqBufCount,
        bool createMetaPlane,
        int width,
        int height,
        int stride,
        int pixelFormat,
        bool needMmap)
{
    int ret = 0;

	if (bufManager == NULL) {
		CLOGE("ERR(%s[%d]):BufferManager is NULL", __FUNCTION__, __LINE__);
		ret = BAD_VALUE;
		goto func_exit;
	}

    CLOGI("setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, width=%d height=%d stride=%d pixelFormat=%d)",
        planeCount, reqBufCount, reqBufCount, width, height, stride, pixelFormat);

    ret = bufManager->setInfo(
                        planeCount,
                        planeSize,
                        bytePerLine,
                        0,
                        reqBufCount,
                        reqBufCount,
                        EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE,
                        BUFFER_MANAGER_ALLOCATION_ATONCE,
                        createMetaPlane,
                        width,
                        height,
                        stride,
                        pixelFormat,
                        needMmap);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc();
    if (ret < 0) {
        CLOGE("alloc fail");
        goto func_exit;
    }

func_exit:

    return ret;
}

int ExynosCamera::m_getShotBufferIdex() const
{
    return NUM_PLANES(V4L2_PIX_2_HAL_PIXEL_FORMAT(SCC_OUTPUT_COLOR_FMT));
}

bool ExynosCamera::m_enableFaceDetection(bool toggle)
{
    CLOGD(" toggle : %d", toggle);

    if (toggle == true) {
        m_parameters->setFdEnable(true);
        m_parameters->setFdMode(FACEDETECT_MODE_FULL);
    } else {
        m_parameters->setFdEnable(false);
        m_parameters->setFdMode(FACEDETECT_MODE_OFF);
    }

    memset(&m_frameMetadata, 0, sizeof(camera_frame_metadata_t));

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

status_t ExynosCamera::m_generateFrame(int32_t frameCount, ExynosCameraFrameSP_dptr_t newFrame, ExynosCameraFrameSP_sptr_t refFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;

    newFrame = NULL;

    if (frameCount >= 0) {
        ret = m_searchFrameFromList(&m_processList, frameCount, newFrame, &m_processListLock);
        if (ret < 0) {
            CLOGE("searchFrameFromList fail");
            return INVALID_OPERATION;
        }
    }

    if (newFrame == NULL) {
        newFrame = m_previewFrameFactory->createNewFrame(refFrame);

        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            return UNKNOWN_ERROR;
        }

        bool flagRequested = false;

        if (m_parameters->isOwnScc(getCameraId()) == true)
            flagRequested = newFrame->getRequest(PIPE_SCC);
        else
            flagRequested = newFrame->getRequest(PIPE_ISPC);

        if (flagRequested == true) {
            m_dynamicSccCount++;
            CLOGV("dynamicSccCount inc(%d) frameCount(%d)", m_dynamicSccCount, newFrame->getFrameCount());
        }

        if (newFrame->getRequest(PIPE_VC0) == true) {
            m_dynamicBayerCount++;
            CLOGV("dynamicBayerCount inc(%d) frameCount(%d)", m_dynamicBayerCount, newFrame->getFrameCount());
        }

        ret = m_insertFrameToList(&m_processList, newFrame, &m_processListLock);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to insert frame into m_processList",
                    newFrame->getFrameCount());
            return ret;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setupEntity(
        uint32_t pipeId,
        ExynosCameraFrameSP_sptr_t newFrame,
        ExynosCameraBuffer *srcBuf,
        ExynosCameraBuffer *dstBuf)
{
    int ret = 0;
    entity_buffer_state_t entityBufferState;

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
    ret = newFrame->getDstBufferState(pipeId, &entityBufferState);
    if (ret < 0) {
        CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
        ret = m_setDstBuffer(pipeId, newFrame, dstBuf);
        if (ret < 0) {
            CLOGE("m_setDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setSrcBuffer(
        uint32_t pipeId,
        ExynosCameraFrameSP_sptr_t newFrame,
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
            CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("buffer manager is NULL, pipeId(%d)", pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            CLOGE("getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                pipeId, newFrame->getFrameCount(), ret);
            bufferMgr->dump();
            return ret;
        }
    }

    /* set buffers */
    ret = newFrame->setSrcBuffer(pipeId, *buffer);
    if (ret < 0) {
        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setDstBuffer(
        uint32_t pipeId,
        ExynosCameraFrameSP_sptr_t newFrame,
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
            CLOGE("getBufferManager(DST) fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }

        if (bufferMgr == NULL) {
            CLOGE("buffer manager is NULL, pipeId(%d)", pipeId);
            return BAD_VALUE;
        }

        /* get buffers */
        ret = bufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, buffer);
        if (ret < 0) {
            ExynosCameraFrameEntity *curEntity = newFrame->searchEntityByPipeId(pipeId);
            if (curEntity != NULL) {
                if (curEntity->getBufType() == ENTITY_BUFFER_DELIVERY) {
                    CLOGV("pipe(%d) buffer is empty for delivery", pipeId);
                    buffer->index = -1;
                } else {
                    CLOGE("getBuffer fail, pipeId(%d), frameCount(%d), ret(%d)",
                            pipeId, newFrame->getFrameCount(), ret);
                    return ret;
                }
            } else {
                CLOGE("curEntity is NULL, pipeId(%d)", pipeId);
                return ret;
            }
        }
    }

    /* set buffers */
    ret = newFrame->setDstBuffer(pipeId, *buffer);

    if (ret < 0) {
        CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_generateFrameReprocessing(ExynosCameraFrameSP_dptr_t newFrame, ExynosCameraFrameSP_sptr_t refFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;
    struct ExynosCameraBuffer tempBuffer;
    int bufIndex = -1;

     /* 1. Make Frame */
    newFrame = m_reprocessingFrameFactory->createNewFrame(NULL);
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return UNKNOWN_ERROR;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr)
{
    status_t ret = NO_ERROR;

    /*
     * If it is not 16-aligend, shrink down it as 16 align. ex) 1080 -> 1072
     * But, memory is set on Android format. so, not aligned area will be black.
     */
    int dst_width = 0, dst_height = 0, dst_crop_width = 0, dst_crop_height = 0;
    int dst_format = m_parameters->getPreviewFormat();

    m_parameters->getPreviewSize(&dst_width, &dst_height);
    dst_crop_width = dst_width;
    dst_crop_height = dst_height;

    if (dst_format == V4L2_PIX_FMT_NV21 ||
        dst_format == V4L2_PIX_FMT_NV21M) {
        if (m_parameters->getCallbackNeedCSC() == true) {
            ret = getYuvPlaneSize(dst_format, callbackBuf->size, dst_crop_width, dst_crop_height);
            if (ret != NO_ERROR) {
                CLOGE("getYuvPlaneSize(dst_format(%x), size(%dx%d)) fail",
                    v4l2Format2Char(dst_format, 0),
                    v4l2Format2Char(dst_format, 1),
                    v4l2Format2Char(dst_format, 2),
                    v4l2Format2Char(dst_format, 3),
                    dst_crop_width, dst_crop_height);
                return ret;
            }
        } else {
            callbackBuf->size[0] = (dst_width * dst_height);
            callbackBuf->size[1] = (dst_width * dst_height) / 2;
        }

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + callbackBuf->size[0];
    } else if (dst_format == V4L2_PIX_FMT_YVU420 ||
               dst_format == V4L2_PIX_FMT_YVU420M) {
        unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

        ret = getYuvPlaneSize(dst_format, callbackBuf->size, dst_crop_width, dst_crop_height);
        if (ret != NO_ERROR) {
            CLOGE("getYuvPlaneSize(dst_format(%x), size(%dx%d)) fail",
                v4l2Format2Char(dst_format, 0),
                v4l2Format2Char(dst_format, 1),
                v4l2Format2Char(dst_format, 2),
                v4l2Format2Char(dst_format, 3),
                dst_crop_width, dst_crop_height);
            return ret;
        }

        callbackBuf->addr[0] = baseAddr;
        callbackBuf->addr[1] = callbackBuf->addr[0] + dst_width * dst_height;
        callbackBuf->addr[2] = callbackBuf->addr[1] + (ALIGN((dst_width >> 1), 16) * (dst_height >> 1));
    }

    CLOGV(" dst_size(%dx%d), dst_crop_size(%dx%d)", dst_width, dst_height, dst_crop_width, dst_crop_height);

    return NO_ERROR;
}

status_t ExynosCamera::m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrameSP_sptr_t frame)
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
    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int srcNodeIndex = -1;
    int dstNodeIndex = -1;
    int scpNodeIndex = -1;
    int srcFmt = -1;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    /* copy metadata src to dst*/
    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        dstNodeIndex = 0;

        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

        m_getBufferManager(gscPipe, &dstBufMgr, DST_BUFFER_DIRECTION);
    } else {
        if (m_parameters->getGDCEnabledMode() == true) {
            srcNodeIndex = 0;
        } else {
            srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
        }

        dstNodeIndex = 0;

        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

        m_getBufferManager(gscPipe, &dstBufMgr, DST_BUFFER_DIRECTION);
    }

    ret = m_getBufferManager(pipeId, &srcBufMgr, SRC_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                pipeId, ret);
        return ret;
    }

    state = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getSrcBufferState(gscPipe, &state);
    if( ret < 0 || state == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("getSrcBufferState fail, pipeId(%d), entityState(%d) frame(%d)", gscPipe, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;
    dstBuf.index = -1;

    ret = frame->getSrcBuffer(gscPipe, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    memcpy(dstBuf.addr[dstBuf.getMetaPlaneIndex()],srcBuf.addr[srcBuf.getMetaPlaneIndex()], sizeof(struct camera2_stream));

    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0)
            srcBufMgr->cancelBuffer(srcBuf.index, true);
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }


    CLOGV("syncPrviewWithCSC done(%d)", ret);

    return ret;

func_exit:

    CLOGE(" sync with csc failed frame(%d) ret(%d) src(%d) dst(%d)", frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    srcBuf.index = -1;
    dstBuf.index = -1;

    /* 1. return buffer pipe done. */
    ret = frame->getSrcBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
    }

    /* 2. do not return buffer dual front case, frameselector ownershipt for the buffer */
    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0)
            srcBufMgr->cancelBuffer(srcBuf.index, true);
    }

    /* 3. if the gsc dst buffer available, return the buffer. */
    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index, true);

    /* 4. change buffer state error for error handlering */
    /*  1)dst buffer state : 0( putbuffer ndone for mcpipe ) */
    /*  2)dst buffer state : scp node index ( getbuffer ndone for mcpipe ) */
    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR, scpNodeIndex);
    frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR);

    return INVALID_OPERATION;
}

/*
 * pIsProcessed : out parameter
 *                true if the frame is properly handled.
 *                false if frame processing is failed or there is no frame to process
 */
bool ExynosCamera::m_prePictureInternal(bool* pIsProcessed)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    status_t ret = NO_ERROR;
    bool loop = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSelector::result_t result;
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

    if (m_parameters->is3aaIspOtf() == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_ISP;
    }

    bufPipeId = PIPE_MCSC1;

    int postProcessQSize = m_postPictureThreadInputQ->getSizeOfProcessQ();
    if (postProcessQSize > 2) {
        CLOGW("post picture is delayed(stacked %d frames), skip", postProcessQSize);
        usleep(WAITING_TIME);
        goto CLEAN;
    }

    result = m_captureSelector->selectFrames(newFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, m_pictureFrameFactory->getNodeType(bufPipeId));
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }
    newFrame->frameLock();

    CLOGI("Selected Frame Count (%d)", newFrame->getFrameCount());

    ret = m_insertFrameToList(&m_postProcessList, newFrame, &m_postProcessListLock);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to insert frame into m_postProcessList",
                newFrame->getFrameCount());
        return ret;
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_highResolutionCallbackQ->pushProcessQ(&newFrame);
    } else {
        m_pictureThreadInputQ->pushProcessQ(&newFrame);
    }

    m_reprocessingCounter.decCount();

    CLOGI("prePicture complete, remaining count(%d)", m_reprocessingCounter.getCount());

    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0) {
        loop = true;
    }

    *pIsProcessed = true;

#ifdef DEBUG_RAWDUMP
    retryCount = 30; /* 200ms x 30 */
    bayerBuffer.index = -2;

    ret = newFrame->getDstBuffer(pipeId, &pictureBuffer, m_previewFrameFactory->getNodeType(bufPipeId));
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_FLITE, ret);
    } else {
        if (m_parameters->checkBayerDumpEnable() == true) {
            int sensorMaxW, sensorMaxH;
            int sensorMarginW, sensorMarginH;
            char filePath[70];
            int fliteFcount = 0;
            int pictureFcount = 0;
            int picturePipeId = 0;
            int bayerPipeId = PIPE_VC0;

            camera2_shot_ext *shot_ext = NULL;

            // this is for bayer
            ret = newFrame->getDstBuffer(pipeId, &bayerBuffer,
                    m_previewFrameFactory->getNodeType(bayerPipeId));
            if (ret != NO_ERROR)
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);

            shot_ext = (camera2_shot_ext *)(bayerBuffer.addr[1]);
            if (shot_ext != NULL)
                fliteFcount = shot_ext->shot.dm.request.frameCount;
            else
                CLOGE("BayerBuffer is null");

            // this is for picture
            shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(pictureBuffer.addr[1]);
            if (shot_ext != NULL)
                pictureFcount = shot_ext->shot.dm.request.frameCount;
            else
                CLOGE("PictureBuffer is null");

            CLOGD("bayer fcount(%d) picture fcount(%d)", fliteFcount, pictureFcount);

            /* The driver frame count is used to check the match between the 3AA frame and the FLITE frame.
               if the match fails then the bayer buffer does not correspond to the capture output and hence
               not written to the file */
            if (fliteFcount == pictureFcount) {
                memset(filePath, 0, sizeof(filePath));
                snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",m_cameraId, pictureFcount);

                bool bRet = dumpToFile((char *)filePath,
                                       bayerBuffer.addr[0],
                                       bayerBuffer.size[0]);
                if (bRet == false)
                    CLOGE("couldn't make a raw file");
            }
        }
    }
#endif

    return loop;

CLEAN:
    if (m_hdrEnabled) {
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

        m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

        if (m_reprocessingCounter.getCount() == 0)
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI(" prePicture fail, remaining count(%d)", m_reprocessingCounter.getCount());
    *pIsProcessed = false;   // Notify failure
    return loop;

}

status_t ExynosCamera::m_getAvailableRecordingCallbackHeapIndex(int *index)
{
    if (m_recordingBufferCount <= 0 || m_recordingBufferCount > MAX_BUFFERS) {
        CLOGE("Invalid recordingBufferCount %d", m_recordingBufferCount);

        return INVALID_OPERATION;
    } else if (m_recordingCallbackHeap == NULL) {
        CLOGE("RecordingCallbackHeap is NULL");

        return INVALID_OPERATION;
    }

    Mutex::Autolock aLock(m_recordingCallbackHeapAvailableLock);
    for (int i = 0; i < m_recordingBufferCount; i++) {
        if (m_recordingCallbackHeapAvailable[i] == true) {
            CLOGV("Found recordingCallbackHeapIndex %d", i);

            *index = i;
            m_recordingCallbackHeapAvailable[i] = false;
            return NO_ERROR;
        }
    }

    CLOGW("There is no available recordingCallbackHeapIndex");

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_releaseRecordingCallbackHeap(struct VideoNativeHandleMetadata *addr)
{
    struct VideoNativeHandleMetadata *baseAddr = NULL;
    int recordingCallbackHeapIndex = -1;

    if (addr == NULL) {
        CLOGE("Addr is NULL");

        return BAD_VALUE;
    } else if (m_recordingCallbackHeap == NULL) {
        CLOGE("RecordingCallbackHeap is NULL");

        return INVALID_OPERATION;
    }

    /* Calculate the recordingCallbackHeap index base on address offest. */
    baseAddr = (struct VideoNativeHandleMetadata *) m_recordingCallbackHeap->data;
    recordingCallbackHeapIndex = (int) (addr - baseAddr);

    Mutex::Autolock aLock(m_recordingCallbackHeapAvailableLock);
    if (recordingCallbackHeapIndex < 0 || recordingCallbackHeapIndex >= m_recordingBufferCount) {
        CLOGE("Invalid index %d. base %p addr %p offset %d",
                recordingCallbackHeapIndex,
                baseAddr,
                addr,
                (int) (addr - baseAddr));

        return INVALID_OPERATION;
    } else if (m_recordingCallbackHeapAvailable[recordingCallbackHeapIndex] == true) {
        CLOGW("Already available index %d. base %p addr %p offset %d",
                recordingCallbackHeapIndex,
                baseAddr,
                addr,
                (int) (addr - baseAddr));
    }

    CLOGV("Release recordingCallbackHeapIndex %d.", recordingCallbackHeapIndex);

    m_recordingCallbackHeapAvailable[recordingCallbackHeapIndex] = true;

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseRecordingBuffer(int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufIndex < 0 || bufIndex >= (int)m_recordingBufferCount) {
        CLOGE("Out of Index! (Max: %d, Index: %d)", m_recordingBufferCount, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = m_putBuffers(m_recordingBufferMgr, bufIndex);
    if (ret < 0) {
        CLOGE("put Buffer fail");
    }

func_exit:

    return ret;
}

status_t ExynosCamera::m_calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_parameters->calcPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_parameters->calcHighResolutionPreviewGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_parameters->calcRecordingGSCRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_parameters->calcPictureRect(srcRect, dstRect);
}

status_t ExynosCamera::m_calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect)
{
    return m_parameters->calcPictureRect(originW, originH, srcRect, dstRect);
}

status_t ExynosCamera::m_insertFrameToList(List<ExynosCameraFrameSP_sptr_t> *list, ExynosCameraFrameSP_sptr_t frame, Mutex *lock)
{
    Mutex::Autolock aLock(*lock);

    if (list == NULL || frame == NULL) {
        CLOGE("BAD parameters. list %p",
                list);
        return BAD_VALUE;
    }

    list->push_back(frame);
    return NO_ERROR;
}

status_t ExynosCamera::m_searchFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, uint32_t frameCount, ExynosCameraFrameSP_dptr_t frame, Mutex *lock)
{
    Mutex::Autolock aLock(*lock);
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

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

    return NO_ERROR;
}

status_t ExynosCamera::m_removeFrameFromList(List<ExynosCameraFrameSP_sptr_t> *list, ExynosCameraFrameSP_sptr_t frame, Mutex *lock)
{
    Mutex::Autolock aLock(*lock);
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    if (list->empty()) {
        CLOGD("list is empty");
        return NO_ERROR;
    }

    frameCount = frame->getFrameCount();
    r = list->begin()++;

    do {
        curFrame = *r;
        if (curFrame == NULL) {
            CLOGE("curFrame is empty");
            return INVALID_OPERATION;
        }

        curFrameCount = curFrame->getFrameCount();
        if (frameCount == curFrameCount) {
            CLOGV("frame count match: expected(%d), current(%d)", frameCount, curFrameCount);
            list->erase(r);
            return NO_ERROR;
        }
        CLOGW("frame count mismatch: expected(%d), current(%d)", frameCount, curFrameCount);
#if 0
        curFrame->printEntity();
#else
        curFrame->printNotDoneEntity();
#endif
        r++;
    } while (r != list->end());

    CLOGE("Cannot find match frame(%d)!!!", frameCount);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_deleteFrame(ExynosCameraFrameSP_dptr_t frame)
{
    status_t ret = NO_ERROR;

    /* put lock using this frame */
    Mutex::Autolock lock(m_searchframeLock);

    if (frame == NULL) {
        CLOGE("frame == NULL. so, fail");
        return BAD_VALUE;
    }

    frame = NULL;

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *lock)
{
    Mutex::Autolock aLock(*lock);
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("remaining frame(%zu), we remove them all", list->size());

    list->clear();

    CLOGD("EXIT ");

    return NO_ERROR;
}

status_t ExynosCamera::m_clearList(frame_queue_t *queue)
{
    Mutex::Autolock lock(m_searchframeLock);
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;

    CLOGD("remaining frame(%d), we remove them all", queue->getSizeOfProcessQ());

    queue->release();

    CLOGD("EXIT ");

    return NO_ERROR;
}

status_t ExynosCamera::m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction) {
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer deleteSccBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int ret = NO_ERROR;

    if (frameQ == NULL) {
        CLOGE("frameQ is NULL.");
        return INVALID_OPERATION;
    }

    CLOGI(" IN... frameQSize(%d)", frameQ->getSizeOfProcessQ());

    while (0 < frameQ->getSizeOfProcessQ()) {
        ret = frameQ->popProcessQ(&newFrame);
        if (ret < 0) {
            CLOGE("wait and pop fail, ret(%d)", ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            ret = INVALID_OPERATION;
            continue;
        }

        if (direction == SRC_BUFFER_DIRECTION) {
            ret = newFrame->getSrcBuffer(pipeId, &deleteSccBuffer);
        } else {
            if(m_previewFrameFactory == NULL) {
                return INVALID_OPERATION;
            }
            ret = newFrame->getDstBuffer(pipeId, &deleteSccBuffer, m_previewFrameFactory->getNodeType(pipeId));
        }
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            continue;
        }

        ret = m_getBufferManager(pipeId, &bufferMgr, direction);
        if (ret < 0)
            CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)", pipeId, ret);

        /* put SCC buffer */
        CLOGD("m_putBuffer by clearjpegthread(dstSccRe), index(%d)", deleteSccBuffer.index);
        ret = m_putBuffers(bufferMgr, deleteSccBuffer.index);
        if (ret < 0)
            CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)", pipeId, ret);
    }

    return ret;
}

status_t ExynosCamera::m_printFrameList(List<ExynosCameraFrameSP_sptr_t> *list, Mutex *lock)
{
    Mutex::Autolock aLock(*lock);
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    CLOGD("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
    CLOGD("\t remaining frame count(%zu)", list->size());

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
        CLOGI("try(%d) to create IonAllocator", retry);
        *allocator = new ExynosCameraIonAllocator();
        ret = (*allocator)->init(false);
        if (ret < 0) {
            CLOGE("create IonAllocator fail (retryCount=%d)", retry);
            delete (*allocator);
            *allocator = NULL;
        } else {
            CLOGD("m_createIonAllocator success (allocator=%p)", *allocator);
            break;
        }
    } while (ret < 0 && retry < 3);

    if (ret < 0 && retry >=3) {
        CLOGE("create IonAllocator fail (retryCount=%d)", retry);
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
            CLOGE("m_createIonAllocator fail");
        else
            CLOGD("m_createIonAllocator success");
    }

    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    CLOGV("BufferManager(%s) created", name);

    return ret;
}

status_t ExynosCamera::m_checkThreadState(int *threadState, int *countRenew)
{
    int ret = NO_ERROR;

    if ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT)) {
        CLOGW("SCP DQ Timeout! State:[%d], Duration:%d msec", *threadState, (*countRenew)*(MONITOR_THREAD_INTERVAL/1000));
        ret = false;
    } else {
        CLOGV("(%d)", *threadState);
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
        CLOGW("Pipe(%d) Thread Interval [%lld msec], State:[%d]", pipeId, (long long)(*threadInterval)/1000, *threadState);
        ret = false;
    } else {
        CLOGV("Thread IntervalTime [%lld]", (long long)*threadInterval);
        CLOGV("Thread Renew state [%d]", *threadState);
        ret = NO_ERROR;
    }

    return ret;
}

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

void ExynosCamera::m_dump(void)
{
    CLOGI("");

    m_printExynosCameraInfo(__FUNCTION__);

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->dump();

    if (m_bayerBufferMgr != NULL)
        m_bayerBufferMgr->dump();

#if defined(DEBUG_RAWDUMP)
    if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        if (m_fliteBufferMgr != NULL)
            m_fliteBufferMgr->dump();
    }
#endif

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
    if (m_yuvReprocessingBufferMgr != NULL)
        m_yuvReprocessingBufferMgr->dump();
    if (m_thumbnailBufferMgr != NULL)
        m_thumbnailBufferMgr->dump();
    if (m_yuvBufferMgr != NULL)
        m_yuvBufferMgr->dump();
    if (m_gscBufferMgr != NULL)
        m_gscBufferMgr->dump();

    m_dumpVendor();

    return;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_parameters->isFlite3aaOtf() == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_FLITE;
    }

    return pipeId;
}

void ExynosCamera::m_debugFpsCheck(__unused uint32_t pipeId)
{
#ifdef FPS_CHECK
    uint32_t id = pipeId % DEBUG_MAX_PIPE_NUM;

    m_debugFpsCount[id]++;
    if (m_debugFpsCount[id] == 1) {
        m_debugFpsTimer[id].start();
    }
    if (m_debugFpsCount[id] == 31) {
        m_debugFpsTimer[id].stop();
        long long durationTime = m_debugFpsTimer[id].durationMsecs();
        CLOGI(" FPS_CHECK(id:%d), duration %lld / 30 = %lld ms. %lld fps",
            pipeId, durationTime, durationTime / 30, 1000 / (durationTime / 30));
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

    shot_stream = (struct camera2_stream *)buffer->addr[buffer->getMetaPlaneIndex()];
    if (shot_stream == NULL) {
        CLOGE("Failed to getMetadataPlane. addr[0] %p",
                buffer->addr[0]);
        return INVALID_OPERATION;
    }
    bayerFrameCount = shot_stream->fcount;
    outputInfo->cropRegion[0] = shot_stream->output_crop_region[0];
    outputInfo->cropRegion[1] = shot_stream->output_crop_region[1];
    outputInfo->cropRegion[2] = shot_stream->output_crop_region[2];
    outputInfo->cropRegion[3] = shot_stream->output_crop_region[3];

    shot_ext = (camera2_shot_ext *)buffer->addr[buffer->getMetaPlaneIndex()];
    if (shot_ext == NULL) {
        CLOGE("Failed to getMetadataPlane. addr[0] %p",
                buffer->addr[0]);
        return INVALID_OPERATION;
    }

    memset(shot_ext, 0x0, sizeof(struct camera2_shot_ext));
    shot_ext->shot.dm.request.frameCount = bayerFrameCount;

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
            CLOGW("retry(%d) setupEntity for pipeId(%d)", retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

    return ret;
}

status_t ExynosCamera::m_boostDynamicCapture(void)
{
    status_t ret = NO_ERROR;
#if 0 /* TODO: need to implementation for bayer */
    uint32_t pipeId = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC : PIPE_ISPC;
    uint32_t size = m_processList.size();

    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;
    camera2_node_group node_group_info_isp;

    if (m_processList.empty()) {
        CLOGD("m_processList is empty");
        return NO_ERROR;
    }
    CLOGD("m_processList size(%d)", m_processList.size());
    r = m_processList.end();

    for (unsigned int i = 0; i < 3; i++) {
        r--;
        if (r == m_processList.begin())
            break;

    }

    curFrame = *r;
    if (curFrame == NULL) {
        CLOGE("curFrame is empty");
        return INVALID_OPERATION;
    }

    if (curFrame->getRequest(pipeId) == true) {
        CLOGD(" Boosting dynamic capture is not need");
        return NO_ERROR;
    }

    CLOGI(" boosting dynamic capture (frameCount: %d)", curFrame->getFrameCount());
    /* For ISP */
    curFrame->getNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    m_updateBoostDynamicCaptureSize(&node_group_info_isp);
    curFrame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);

    curFrame->setRequest(pipeId, true);
    curFrame->setNumRequestPipe(curFrame->getNumRequestPipe() + 1);

    ret = curFrame->setEntityState(pipeId, ENTITY_STATE_REWORK);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                pipeId, ENTITY_STATE_REWORK, ret);
        return ret;
    }

    m_previewFrameFactory->pushFrameToPipe(&curFrame, pipeId);
    m_dynamicSccCount++;
    CLOGV("dynamicSccCount inc(%d) frameCount(%d)", m_dynamicSccCount, curFrame->getFrameCount());
#endif

    return ret;
}

bool ExynosCamera::m_checkCameraSavingPath(char *dir, char* srcPath, char *dstPath, int dstSize)
{
    int ret = false;
    int funcRet = false;

    struct dirent **items = NULL;
    struct stat fstat;
    int item_count, i;
    char ChangeDirPath[CAMERA_FILE_PATH_SIZE] = {'\0',};

    memset(dstPath, 0, dstSize);

    // Check access path
    if (srcPath && dstSize > (int)strlen(srcPath)) {
        strncpy(dstPath, srcPath, dstSize - 1);
    } else {
        CLOGW("Parameter srcPath is NULL. Change to Default Path");
        snprintf(dstPath, dstSize, "%s/DCIM/Camera/", dir);
    }

    if (access(dstPath, 0)==0) {
        CLOGW(" success access dir = %s", dstPath);
        return true;
    }

    CLOGW(" can't find dir = %s", m_burstSavePath);

    // If directory cant't access, then search "DCIM/Camera" folder in current directory
    int iitems = scandir(dir, &items, NULL, alphasort);
    for (int i = 0; i < iitems; i++) {
        // Search only dcim directory
        funcRet = lstat(items[i]->d_name, &fstat);
        if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
            if (!strcmp(items[i]->d_name, ".") || !strcmp(items[i]->d_name, ".."))
                continue;
            if (strcasecmp(items[i]->d_name, "DCIM")==0) {
                sprintf(ChangeDirPath, "%s/%s", dir, items[i]->d_name);
                int jitems = scandir(ChangeDirPath, &items, NULL, alphasort);
                for (int j = 0; j < jitems; j++) {
                    // Search only camera directory
                    funcRet = lstat(items[j]->d_name, &fstat);
                    if ((fstat.st_mode & S_IFDIR) == S_IFDIR) {
                        if (!strcmp(items[j]->d_name, ".") || !strcmp(items[j]->d_name, ".."))
                            continue;
                        if (strcasecmp(items[j]->d_name, "CAMERA")==0) {
                            sprintf(dstPath, "%s/%s/", ChangeDirPath, items[j]->d_name);
                            CLOGW(" change save path = %s", dstPath);
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

bool ExynosCamera::m_FileSaveFunc(char *filePath, ExynosCameraBuffer *SaveBuf)
{
    int fd = -1;
    int err = NO_ERROR;
    int nw, cnt = 0;
    uint32_t written = 0;
    char *data;

    fd = open(filePath, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        CLOGD("failed to create file [%s]: %s",
                filePath, strerror(errno));
        goto SAVE_ERR;
    }

    data = SaveBuf->addr[0];

    CLOGD("(%s)file write start)", filePath);
    while (written < SaveBuf->size[0]) {
        nw = ::write(fd, (const char *)(data) + written, SaveBuf->size[0] - written);

        if (nw < 0) {
            CLOGD("failed to write file [%s]: %s",
                    filePath, strerror(errno));
            break;
        }

        written += nw;
        cnt++;
    }
    CLOGD("(%s)file write end)", filePath);

    if (fd >= 0)
        ::close(fd);

    err = chmod(filePath, 0664);
    if (err != NO_ERROR) {
        CLOGE("(%s[%d]): failed chmod [%s] /%s(%d)",
            __FUNCTION__, __LINE__, filePath, strerror(errno), err);
    }

    err = chown(filePath, AID_CAMERASERVER, AID_MEDIA_RW);
    if (err  != NO_ERROR) {
        CLOGE("(%s[%d]):failed chown [%s] user(%d), group(%d) /%s(%d)",
                __FUNCTION__, __LINE__,  filePath, AID_CAMERASERVER, AID_MEDIA_RW, strerror(errno), err);
    }

    return true;

SAVE_ERR:
    return false;
}

bool ExynosCamera::m_FileSaveFunc(char *filePath, char *saveBuf, unsigned int size)
{
    int fd = -1;
    int err = NO_ERROR;
    int nw, cnt = 0;
    uint32_t written = 0;

    fd = open(filePath, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        CLOGD("failed to create file [%s]: %s",
                filePath, strerror(errno));
        goto SAVE_ERR;
    }

    CLOGD("(%s)file write start)", filePath);
    while (written < size) {
        nw = ::write(fd, (const char *)(saveBuf) + written, size - written);

        if (nw < 0) {
            CLOGD("failed to write file [%s]: %s",
                    filePath, strerror(errno));
            break;
        }

        written += nw;
        cnt++;
    }
    CLOGD("(%s)file write end)", filePath);

    if (fd > 0)
        ::close(fd);

    err = chmod(filePath, 0664);
    if (err != NO_ERROR) {
        CLOGE("(%s[%d]): failed chmod [%s] /%s(%d)",
            __FUNCTION__, __LINE__, filePath, strerror(errno), err);
    }

    err = chown(filePath, AID_CAMERASERVER, AID_MEDIA_RW);
    if (err != NO_ERROR) {
        CLOGE("(%s[%d]):failed chown [%s] user(%d), group(%d) /%s(%d)",
                __FUNCTION__, __LINE__,  filePath, AID_CAMERASERVER, AID_MEDIA_RW, strerror(errno), err);
    }

    return true;

SAVE_ERR:
    return false;
}

void ExynosCamera::m_updateBoostDynamicCaptureSize(__unused camera2_node_group *node_group_info)
{
#if 0 /* TODO: need to implementation for bayer */
    ExynosRect sensorSize;
    ExynosRect bayerCropSize;

    node_group_info->capture[PERFRAME_BACK_SCC_POS].request = 1;

    m_parameters->getPreviewBayerCropSize(&sensorSize, &bayerCropSize);

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

    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_curMinFps != curMinFps) {
        CLOGD("current MinFps(%d), current MaxFps(%d)", curMinFps, curMaxFps);

        enum pipeline pipe = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC : PIPE_ISPC;

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

void ExynosCamera::m_printExynosCameraInfo(const char *funcName)
{
    int w = 0;
    int h = 0;
    ExynosRect srcRect, dstRect;

    CLOGD("===================================================");
    CLOGD("============= ExynosCameraInfo call by %s", funcName);

    CLOGD("============= Scenario ============================");
    CLOGD("= getCameraId                 : %d", m_parameters->getCameraId());
    CLOGD("= getDualMode                 : %d", m_parameters->getDualMode());
    CLOGD("= getScalableSensorMode       : %d", m_parameters->getScalableSensorMode());
    CLOGD("= getRecordingHint            : %d", m_parameters->getRecordingHint());
    CLOGD("= getEffectRecordingHint      : %d", m_parameters->getEffectRecordingHint());
    CLOGD("= getDualRecordingHint        : %d", m_parameters->getDualRecordingHint());
    CLOGD("= getAdaptiveCSCRecording     : %d", m_parameters->getAdaptiveCSCRecording());
    CLOGD("= doCscRecording              : %d", m_parameters->doCscRecording());
    CLOGD("= needGSCForCapture           : %d", m_parameters->needGSCForCapture(getCameraIdInternal()));
    CLOGD("= getShotMode                 : %d", m_parameters->getShotMode());
    CLOGD("= getTpuEnabledMode           : %d", m_parameters->getTpuEnabledMode());
    CLOGD("= getGDCEnabledMode           : %d", m_parameters->getGDCEnabledMode());
    CLOGD("= getHWVdisMode               : %d", m_parameters->getHWVdisMode());
    CLOGD("= get3dnrMode                 : %d", m_parameters->get3dnrMode());
    CLOGD("= getOdcMode                  : %d", m_parameters->getOdcMode());
#ifdef USE_ODC_CAPTURE
    CLOGD("= getODCCaptureMode           : %d", m_parameters->getODCCaptureMode());
    CLOGD("= getODCCaptureEnable         : %d", m_parameters->getODCCaptureEnable());
#endif /* USE_ODC_CAPTURE */

    CLOGD("============= Internal setting ====================");
    CLOGD("= isFlite3aaOtf               : %d", m_parameters->isFlite3aaOtf());
    CLOGD("= is3aaIspOtf                 : %d", m_parameters->is3aaIspOtf());
    CLOGD("= isIspMcscOtf                : %d", m_parameters->isIspMcscOtf());
    CLOGD("= isIspTpuOtf                 : %d", m_parameters->isIspTpuOtf());
    CLOGD("= isTpuMcscOtf                : %d", m_parameters->isTpuMcscOtf());
    CLOGD("= isMcscGDCOtf                : %d", m_parameters->isMcscGDCOtf());
    CLOGD("= TpuEnabledMode              : %d", m_parameters->getTpuEnabledMode());
    CLOGD("= isReprocessingCapture       : %d", m_parameters->isReprocessingCapture());
    CLOGD("= isDynamicCapture            : %d", m_parameters->isDynamicCapture());
    CLOGD("= isReprocessing3aaIspOTF     : %d", m_parameters->isReprocessing3aaIspOTF());

    int reprocessingMode = m_parameters->getReprocessingMode();
    switch(reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        CLOGD("= getReprocessingMode         : REPROCESSING_PURE_BAYER");
        break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        CLOGD("= getReprocessingMode         : REPROCESSING_PROCESSED_BAYER");
        break;
    case PROCESSING_MODE_REPROCESSING_YUV:
        CLOGD("= getReprocessingMode         : REPROCESSING_YUV");
        break;
    case PROCESSING_MODE_NON_REPROCESSING_YUV:
        CLOGD("= getReprocessingMode         : NON_REPROCESSING_YUV");
        break;
    default:
        CLOGD("= getReprocessingMode         : unexpected mode %d", reprocessingMode);
        break;
    }

    CLOGD("============= size setting =======================");
    m_parameters->getMaxSensorSize(&w, &h);
    CLOGD("= getMaxSensorSize            : %d x %d", w, h);

    m_parameters->getHwSensorSize(&w, &h);
    CLOGD("= getHwSensorSize             : %d x %d", w, h);

    m_parameters->getBnsSize(&w, &h);
    CLOGD("= getBnsSize                  : %d x %d", w, h);

    m_parameters->getPreviewBayerCropSize(&srcRect, &dstRect);
    CLOGD("= getPreviewBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getPreviewBdsSize(&dstRect);
    CLOGD("= getPreviewBdsSize           : (%d, %d, %d, %d)",
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getHwPreviewSize(&w, &h);
    CLOGD("= getHwPreviewSize            : %d x %d", w, h);

    m_parameters->getPreviewSize(&w, &h);
    CLOGD("= getPreviewSize              : %d x %d", w, h);

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);
    CLOGD("= getPictureBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getPictureBdsSize(&dstRect);
    CLOGD("= getPictureBdsSize           : (%d, %d, %d, %d)",
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getHwPictureSize(&w, &h);
    CLOGD("= getHwPictureSize            : %d x %d", w, h);

    m_parameters->getPictureSize(&w, &h);
    CLOGD("= getPictureSize              : %d x %d", w, h);

    CLOGD("===================================================");
}

status_t ExynosCamera::m_putFrameBuffer(ExynosCameraFrameSP_sptr_t frame,
                                        int pipeId,
                                        enum buffer_direction_type bufferDirectionType,
                                        bool isReuse)
{
    status_t ret = NO_ERROR;

    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;

    switch (bufferDirectionType) {
    case SRC_BUFFER_DIRECTION:
        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                pipeId, ret);
            return ret;
        }
        break;
    case DST_BUFFER_DIRECTION:
        ret = frame->getDstBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                pipeId, ret);
            return ret;
        }
        break;
    default:
        CLOGE("invalid bufferDirectionType(%d). so fail", bufferDirectionType);
        return INVALID_OPERATION;
    }

    if (0 <= buffer.index) {
        ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirectionType);
        if (ret != NO_ERROR) {
            CLOGE("m_getBufferManager(pipeId(%d), %s)",
                pipeId,
                bufferDirectionType == SRC_BUFFER_DIRECTION ? "SRC_BUFFER_DIRECTION" : "DST_BUFFER_DIRECTION");
            return ret;
        }

        if (bufferMgr == m_scpBufferMgr) {
            ret = bufferMgr->cancelBuffer(buffer.index, isReuse);
            if (ret != NO_ERROR) {
                CLOGE("cancelBuffer(%d) fail", buffer.index);
                return ret;
            }
        } else {
            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("m_putBuffers(%d) fail", buffer.index);
                return ret;
            }
        }
    } else {
        CLOGE("Invalid bufferIndex(%d), pipeId(%d), %s", buffer.index, pipeId,
            bufferDirectionType == SRC_BUFFER_DIRECTION ? "SRC_BUFFER_DIRECTION" : "DST_BUFFER_DIRECTION");
    }

    return ret;
}

status_t ExynosCamera::m_copyNV21toNV21M(ExynosCameraBuffer srcBuf, ExynosCameraBuffer dstBuf,
                                         int width, int height,
                                         bool flagCopyY, bool flagCopyCbCr)
{
    status_t ret = NO_ERROR;

#if 0
    ExynosCameraDurationTimer copyTimer;
    copyTimer.start();
#endif

    int planeSize = width * height;

    if (flagCopyY == true) {
        if (dstBuf.addr[0] == NULL || srcBuf.addr[0] == NULL) {
            CLOGE("dstBuf.addr[0] == %p ||  srcBuf.addr[0] == %p. so, fail",
                dstBuf.addr[0], srcBuf.addr[0]);
            return BAD_VALUE;
        }

        memcpy(dstBuf.addr[0], srcBuf.addr[0], planeSize);
    }

    if (flagCopyCbCr == true) {
        if (dstBuf.addr[1] == NULL || srcBuf.addr[0] == NULL) {
            CLOGE("dstBuf.addr[1] == %p ||  srcBuf.addr[0] == %p. so, fail",
                dstBuf.addr[1], srcBuf.addr[0]);
            return BAD_VALUE;
        }

        memcpy(dstBuf.addr[1], srcBuf.addr[0] + planeSize, planeSize / 2);
    }

#if 0
    copyTimer.stop();
    int copyTime = (int)copyTimer.durationUsecs();

    if (33333 < copyTime) {
        CLOGW("copyTime(%d  usec) is too more slow than than 33333 usec",
            copyTime);
    } else {
        CLOGD("copyTime(%d  usec)",
            copyTime);
    }
#endif

    return ret;
}

}; /* namespace android */
