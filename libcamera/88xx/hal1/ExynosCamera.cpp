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

    m_cameraId = cameraId;
    m_dev = dev;

    initialize();
}

void ExynosCamera::initialize()
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    int ret = 0;

    ExynosCameraActivityUCTL *uctlMgr = NULL;
    memset(m_name, 0x00, sizeof(m_name));
    m_use_companion = isCompanion(m_cameraId);
    m_cameraSensorId = getSensorId(m_cameraId);

    if (m_cameraId != CAMERA_ID_SECURE)
        m_vendorSpecificPreConstructor(m_cameraId, m_dev);

    /* Create Main Threads */
    m_createThreads();

    m_parameters = new ExynosCamera1Parameters(m_cameraId, m_use_companion);
    CLOGD("DEBUG(%s):Parameters(Id=%d) created", __FUNCTION__, m_cameraId);

    if (m_cameraId == CAMERA_ID_SECURE) {
        m_cameraHiddenId = m_cameraId;
        m_cameraId = CAMERA_ID_FRONT;
    } else {
        m_cameraHiddenId = -1;
    }

    m_exynosCameraActivityControl = m_parameters->getActivityControl();

    /* Frame factories */
    m_previewFrameFactory      = NULL;
    m_reprocessingFrameFactory = NULL;
    m_visionFrameFactory= NULL;

    /* Memory allocator */
    m_ionAllocator = NULL;
    m_grAllocator  = NULL;
    m_mhbAllocator = NULL;

    /* Frame manager */
    m_frameMgr = NULL;

    /* Main Buffer */
    /* Bayer buffer: Output buffer of Flite or 3AC */
    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
#ifdef DEBUG_RAWDUMP
    /* Flite buffer: Output buffer of Flite for DEBUG_RAWDUMP on Single chain */
    if (m_parameters->getUsePureBayerReprocessing() == false)
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
    m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "MCSC_RE_BUF");
    /* Thumbnail buffer: Output buffer of reprocessing ISP(MCSC) */
    m_createInternalBufferManager(&m_thumbnailBufferMgr, "THUMBNAIL_BUF");

    /* Capture Buffer */
    /* SCC buffer: Output buffer of preview ISP(MCSC) */
    m_createInternalBufferManager(&m_sccBufferMgr, "SCC_BUF");
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    /* fusion preview buffer */
    m_createInternalBufferManager(&m_fusionBufferMgr, "FUSION_BUF");
#endif

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

    m_dstSccReprocessingQ  = new frame_queue_t;
    m_dstSccReprocessingQ->setWaitTime(50000000);

    m_dstGscReprocessingQ  = new frame_queue_t;
    m_dstGscReprocessingQ->setWaitTime(500000000);

    m_zoomPreviwWithCscQ = new frame_queue_t;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_fusionQ            = new frame_queue_t;
#endif

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

    m_postPictureQ = new frame_queue_t(m_postPictureThread);

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
    m_flagStartFaceDetection = false;
    m_flagLLSStart = false;
    m_flagLightCondition = false;
    m_fdThreshold = 0;
    m_captureSelector = NULL;
    m_sccCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_hdrSkipedFcount = 0;
    m_doCscRecording = true;
    m_recordingBufferCount = NUM_RECORDING_BUFFERS;
    m_frameSkipCount = 0;
    m_isZSLCaptureOn = false;
    m_isSuccessedBufferAllocation = false;
    m_fdFrameSkipCount = 0;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;

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

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
    m_isFirstParametersSet = false;
#endif

    m_tempshot = new struct camera2_shot_ext;
    m_fdmeta_shot = new struct camera2_shot_ext;
    m_meta_shot  = new struct camera2_shot_ext;
    m_picture_meta_shot = new struct camera2_shot;

    m_ionClient = ion_open();
    if (m_ionClient < 0) {
        ALOGE("ERR(%s):m_ionClient ion_open() fail", __func__);
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

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
}

ExynosCamera::~ExynosCamera()
{
    this->release();
}

void ExynosCamera::release()
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    int ret = 0;

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastenAeThread != NULL) {
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread", __FUNCTION__, __LINE__);
        m_fastenAeThread->requestExitAndWait();
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread end", __FUNCTION__, __LINE__);
    } else {
        CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
    }
#endif

    m_stopCompanion();

    if (m_frameMgr != NULL) {
        m_frameMgr->stop();
    }

    /* release all framefactory */
    m_deinitFrameFactory();

    if (m_parameters != NULL) {
        delete m_parameters;
        m_parameters = NULL;
        CLOGD("DEBUG(%s):Parameters(Id=%d) destroyed", __FUNCTION__, m_cameraId);
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

    if (m_zoomPreviwWithCscQ != NULL) {
        delete m_zoomPreviwWithCscQ;
        m_zoomPreviwWithCscQ = NULL;
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionQ != NULL) {
        delete m_fusionQ;
        m_fusionQ = NULL;
    }
#endif

    /* vision */
    if (m_pipeFrameVisionDoneQ != NULL) {
        delete m_pipeFrameVisionDoneQ;
        m_pipeFrameVisionDoneQ = NULL;
    }

    if (m_dstIspReprocessingQ != NULL) {
        delete m_dstIspReprocessingQ;
        m_dstIspReprocessingQ = NULL;
    }

    if (m_dstSccReprocessingQ != NULL) {
        delete m_dstSccReprocessingQ;
        m_dstSccReprocessingQ = NULL;
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

    if (m_postPictureQ != NULL) {
        delete m_postPictureQ;
        m_postPictureQ = NULL;
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

    if (m_highResolutionCallbackQ != NULL) {
        delete m_highResolutionCallbackQ;
        m_highResolutionCallbackQ = NULL;
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(bayerBufferMgr) destroyed", __FUNCTION__);
    }

#ifdef DEBUG_RAWDUMP
    if (m_fliteBufferMgr != NULL) {
        delete m_fliteBufferMgr;
        m_fliteBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(fliteBufferMgr) destroyed", __FUNCTION__);
    }
#endif

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        delete m_depthMapBufferMgr;
        m_depthMapBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_depthMapBufferMgr) destroyed", __FUNCTION__);
    }
#endif
#ifdef DEBUG_RAWDUMP
    if (m_fliteBufferMgr != NULL) {
        delete m_fliteBufferMgr;
        m_fliteBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(fliteBufferMgr) destroyed", __FUNCTION__);
    }
#endif

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

    if (m_zoomScalerBufferMgr != NULL) {
        delete m_zoomScalerBufferMgr;
        m_zoomScalerBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_zoomScalerBufferMgr) destroyed", __FUNCTION__);
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        delete m_fusionBufferMgr;
        m_fusionBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(m_fusionBufferMgr) destroyed", __FUNCTION__);
    }
#endif

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

    if (m_thumbnailBufferMgr != NULL) {
        delete m_thumbnailBufferMgr;
        m_thumbnailBufferMgr = NULL;
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

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
}

int ExynosCamera::getCameraId() const
{
    if (m_cameraHiddenId != -1)
        return m_cameraHiddenId;
    else
        return m_cameraId;
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

    CLOGI("INFO(%s[%d]):m_previewEnabled=%d",
        __FUNCTION__, __LINE__, (int)ret);

    return ret;
}

bool ExynosCamera::recordingEnabled()
{
    bool ret = m_getRecordingEnabled();
    CLOGI("INFO(%s[%d]):m_recordingEnabled=%d",
        __FUNCTION__, __LINE__, (int)ret);

    return ret;
}

status_t ExynosCamera::autoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    if (m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_autoFocusRunning == false) {
        m_autoFocusType = AUTO_FOCUS_SERVICE;
        m_autoFocusThread->requestExitAndWait();
        m_autoFocusThread->run(PRIORITY_DEFAULT);
    } else {
        CLOGW("WRN(%s[%d]): auto focus is inprogressing", __FUNCTION__, __LINE__);
    }

#if 0 // not used.
    if (m_parameters != NULL) {
        if (m_parameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb lock", __FUNCTION__, __LINE__);
            m_parameters->m_setAutoExposureLock(true);
            m_parameters->m_setAutoWhiteBalanceLock(true);
        }
    }
#endif

    return NO_ERROR;
}

CameraParameters ExynosCamera::getParameters() const
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return m_parameters->getParameters();
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

status_t ExynosCamera::setDualMode(bool enabled)
{
    if (m_parameters == NULL) {
        CLOGE("ERR(%s[%d]):m_parameters is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    m_parameters->setDualMode(enabled);

    return NO_ERROR;
}

void ExynosCamera::enableMsgType(int32_t msgType)
{
    if (m_parameters) {
        CLOGV("INFO(%s[%d]): enable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_parameters->enableMsgType(msgType);
    }
}

void ExynosCamera::disableMsgType(int32_t msgType)
{
    if (m_parameters) {
        CLOGV("INFO(%s[%d]): disable Msg (%x)", __FUNCTION__, __LINE__, msgType);
        m_parameters->disableMsgType(msgType);
    }
}

bool ExynosCamera::msgTypeEnabled(int32_t msgType)
{
    bool isEnabled = false;

    if (m_parameters) {
        CLOGV("INFO(%s[%d]): Msg type enabled (%x)", __FUNCTION__, __LINE__, msgType);
        isEnabled = m_parameters->msgTypeEnabled(msgType);
    }

    return isEnabled;
}

status_t ExynosCamera::dump(__unused int fd)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    m_dump();

    return NO_ERROR;
}

status_t ExynosCamera::storeMetaDataInBuffers(__unused bool enable)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return OK;
}

bool ExynosCamera::m_mainThreadFunc(void)
{
    status_t ret = NO_ERROR;
    int index = 0;
    ExynosCameraFrame *newFrame = NULL;

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s[%d]):Preview is stopped, thread stop", __FUNCTION__, __LINE__);
        return false;
    }

    ret = m_pipeFrameDoneQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT)
            CLOGW("WARN(%s[%d]):Wait timeout", __FUNCTION__, __LINE__);
        else
            CLOGE("ERR(%s[%d]):Wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        return true;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        return true;
    }

/* HACK Reset Preview Flag*/
#if 0
    if (m_parameters->getRestartPreview() == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal();
        m_resetPreview = false;
        CLOGE("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);

        return true;
    }
#endif

    ret = m_handlePreviewFrame(newFrame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):handle preview frame fail, ret(%d)",
                __FUNCTION__, __LINE__, ret);
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
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;

    CLOGV("INFO(%s[%d]):Wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }

    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("WARN(%s[%d]):Wait timeout", __FUNCTION__, __LINE__);
        else
            CLOGE("ERR(%s[%d]):Wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
        ret = m_generateFrame(m_fliteFrameCount, &newframe);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        if (newframe == NULL) {
            CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
            goto func_exit;
        }

        ret = m_setupEntity(pipeId, newframe);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
        }

        ret = newframe->getDstBuffer(pipeId, &buffer);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
        m_fliteFrameCount++;
    }

func_exit:
    if (frame != NULL) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);;
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
    ExynosCameraFrame  *frame = NULL;
    ExynosCameraFrame  *newframe = NULL;

    CLOGV("INFO(%s[%d]):Wait previewCancelQ", __FUNCTION__, __LINE__);
    ret = m_mainSetupQ[INDEX(pipeId)]->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        goto func_exit;
    }

    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            CLOGW("WARN(%s[%d]):Wait timeout", __FUNCTION__, __LINE__);
        else
            CLOGE("ERR(%s[%d]):Wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = frame->getSrcBuffer(pipeId, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
        goto func_exit;
    }

    if (buffer.index >= 0) {
        ret = m_putBuffers(m_3aaBufferMgr, buffer.index);

        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }
    }

    if (m_parameters->isUseEarlyFrameReturn() == true) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
    } else {
        m_pipeFrameDoneQ->pushProcessQ(&frame);
    }

    ret = m_generateFrame(m_3aa_ispFrameCount, &newframe);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (newframe == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    ret = m_setupEntity(pipeId, newframe);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);

    m_previewFrameFactory->pushFrameToPipe(&newframe, pipeId);
    m_3aa_ispFrameCount++;

    /* Push frame to pure bayer pipe when pure always mode */
    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
        && m_parameters->getIntelligentMode() != 1) {
        int minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;
        int min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

        /*
         * Generate frame and sync FLITE, 3AA frame count.
         * We must considering FLITE, 3AA initialized buffer count gap.
         * (min3AAFrame count - minBayerFrameNum) is calculation of initialized buffer count gap.
         * FLITE count >  3AA count : Skip frame generation.
         * FLITE count <= 3AA count : Generate frame while FLITE count == 3AA count.
         */
        while ((int)m_3aa_ispFrameCount - (int)m_fliteFrameCount > min3AAFrameNum - minBayerFrameNum) {
            ret = m_generateFrame(m_fliteFrameCount, &newframe);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                goto func_exit;
            }

            if (newframe == NULL) {
                CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
                goto func_exit;
            }

            /* Wait tha bayer buffer to be returned to avoid that
               bayer buffer is skipped to queue. */
            int retryCount = 10; /* 1 msec x 10 = 10 msec */
            while (m_bayerBufferMgr->getNumOfAvailableBuffer() <= 0
                    && retryCount-- > 0) {
                usleep(1000);
            }

            if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(bayerPipeId, newframe);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                }

#ifdef SUPPORT_DEPTH_MAP
                if (newframe->getRequest(PIPE_VC1) == true) {
                    if (m_depthMapBufferMgr != NULL
                            && m_depthMapBufferMgr->getNumOfAvailableBuffer() > 0) {
                        int bufIndex = -1;

                        ret = m_depthMapBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &buffer);
                        if (ret == NO_ERROR && bufIndex >= 0) {
                            ret = newframe->setDstBufferState(PIPE_FLITE, ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_2);
                            if (ret == NO_ERROR) {
                                ret = newframe->setDstBuffer(PIPE_FLITE, buffer, CAPTURE_NODE_2);
                                if (ret != NO_ERROR) {
                                    CLOGE("ERR(%s[%d]):Failed to setDstBuffer. PIPE_FLITE, CAPTURE_NODE_2",
                                            __FUNCTION__, __LINE__);
                                    newframe->setRequest(PIPE_VC1, false);
                                }
                            } else {
                                CLOGE("ERR(%s[%d]):Failed to setDstBufferState with REQUESTED. PIPE_FLITE, CAPTURE_NODE_2",
                                        __FUNCTION__, __LINE__);
                                newframe->setRequest(PIPE_VC1, false);
                            }
                        } else {
                            CLOGW("WARN(%s[%d]):Failed to get DepthMap buffer. ret %d bufferIndex %d",
                                    __FUNCTION__, __LINE__, ret, bufIndex);
                            newframe->setRequest(PIPE_VC1, false);
                        }
                    } else {
                        CLOGW("WARN(%s[%d]):Failed to get DepthyMap buffer. m_depthMapBufferMgr %p availableBuffercount %d",
                                __FUNCTION__, __LINE__,
                                m_depthMapBufferMgr,
                                m_depthMapBufferMgr->getNumOfAvailableBuffer());
                        newframe->setRequest(PIPE_VC1, false);
                    }
                }
#endif

                m_previewFrameFactory->pushFrameToPipe(&newframe, bayerPipeId);
            } else {
                int numRequestPipe = (int)(newframe->getNumRequestPipe());
                numRequestPipe--;
                if (numRequestPipe < 0)
                    numRequestPipe = 0;
                newframe->setNumRequestPipe(numRequestPipe);
            }
            m_fliteFrameCount++;
        }
    }

func_exit:
    return loop;
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
    ExynosCameraFrame  *frame = NULL;
    int32_t status = 0;
    frame_queue_t *pipeFrameDoneQ = NULL;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    CLOGV("INFO(%s[%d]):wait zoomPreviwWithCscQThreadFunc", __FUNCTION__, __LINE__);
    status = m_zoomPreviwWithCscQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (status < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (status == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL, retry", __FUNCTION__, __LINE__);
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
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);

        srcBufMgr = m_sccBufferMgr;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            dstBufMgr = m_fusionBufferMgr;
        else
#endif
            dstBufMgr = m_scpBufferMgr;
    } else {
        pipeId = PIPE_ISP;
#ifdef USE_GSC_FOR_PREVIEW
        gscPipe = PIPE_GSC;
#else
        gscPipe = PIPE_GSC_VIDEO;
#endif
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcFmt = m_parameters->getHwPreviewFormat();
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        srcBufMgr = m_zoomScalerBufferMgr;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            dstBufMgr = m_fusionBufferMgr;
        else
#endif
            dstBufMgr = m_scpBufferMgr;
    }

    ret = frame->getDstBufferState(pipeId, &state, srcNodeIndex);
    if (ret < 0 || state != ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), state(%d) frame(%d)",
                __FUNCTION__, __LINE__, pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* get scaler source buffer */
    srcBuf.index = -1;

    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.planeCount-1];

    if (meta == NULL) {
        CLOGE("ERR(%s[%d]):meta is NULL, pipeId(%d), ret(%d) frame(%d)",
                __FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    output = meta->output_crop_region;

    dstBuf.index = -2;
    bufIndex = -1;
    ret = dstBufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Buffer manager getBuffer fail, frame(%d), ret(%d)",
                __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
        goto func_exit;
    }

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

    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        /* dual front scenario use 3aa bayer crop */
    } else {
        m_parameters->calcPreviewDzoomCropSize(&srcRect, &dstRect);
    }


    ret = frame->setSrcRect(gscPipe, srcRect);
    ret = frame->setDstRect(gscPipe, dstRect);

    CLOGV("DEBUG(%s[%d]):do zoomPreviw with CSC : srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> \
            (%d, %d, %d, %d) format(%d)  actual(%x)", __FUNCTION__, __LINE__,
            srcBuf.index, dstBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcFmt,
            V4L2_PIX_2_HAL_PIXEL_FORMAT(srcFmt), dstRect.x, dstRect.y, dstRect.w, dstRect.h,
            previewFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = m_setupEntity(gscPipe, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d) frame(%d)",
                __FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(&frame, gscPipe);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return loop;

func_exit:

    CLOGE("ERR(%s[%d]):do zoomPreviw with CSC failed frame(%d) ret(%d) src(%d) dst(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    dstBuf.index = -1;

    ret = frame->getDstBuffer(gscPipe, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                __FUNCTION__, __LINE__, gscPipe, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index);

    /* msc with ndone frame, in order to frame order*/
    CLOGE("ERR(%s[%d]): zoom with scaler getbuffer failed , use MSC for NDONE frame(%d)",
            __FUNCTION__, __LINE__, frame->getFrameCount());

    frame->setSrcBufferState(gscPipe, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(&frame, gscPipe);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
bool ExynosCamera::m_fusionThreadFunc(void)
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
    int previewH, previewW;
    int bufIndex = -1;
    int waitCount = 0;
    int srcNodeIndex = -1;
    int srcPipeId = -1;
    int dstPipeId = PIPE_FUSION;
    ExynosCameraFrame  *frame = NULL;
    int32_t status = 0;
    frame_queue_t *pipeFrameDoneQ = NULL;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    ExynosRect fusionSrcRect;
    ExynosRect fusionDstRect;

    CLOGV("INFO(%s[%d]):wait %s", __FUNCTION__, __LINE__, __FUNCTION__);
    status = m_fusionQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        return false;
    }
    if (status < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (status == TIMED_OUT) {
            CLOGW("WARN(%s):wait timeout", __FUNCTION__);
        } else {
            CLOGE("ERR(%s):wait and pop fail, ret(%d)", __FUNCTION__, ret);
            /* TODO: doing exception handling */
        }
        return true;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL, retry", __FUNCTION__, __LINE__);
        return true;
    }

    pipeFrameDoneQ = m_pipeFrameDoneQ;

    previewFormat = m_parameters->getHwPreviewFormat();
    m_parameters->getHwPreviewSize(&previewW, &previewH);

    ///////////////////////////////////////////
    // src buffer
    if (m_parameters->getZoomPreviewWIthScaler()== true) {
        srcPipeId = PIPE_GSC;
        srcNodeIndex = 0;
    } else {
        srcPipeId = PIPE_3AA;

        if (m_parameters->is3aaIspOtf() == false)
            srcPipeId = PIPE_ISP;

        if (m_parameters->getTpuEnabledMode() == true && m_parameters->isIspTpuOtf() == false)
            srcPipeId = PIPE_TPU;

        if (m_parameters->isIspMcscOtf() == false && m_parameters->isTpuMcscOtf() == false)
            srcPipeId = PIPE_MCSC;

        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
    }

    srcBufMgr = m_fusionBufferMgr;

    /* gscaler give always ENTITY_BUFFER_STATE_READY */
    /*
    ret = frame->getDstBufferState(srcPipeId, &state, srcNodeIndex);
    if (ret < 0 || state != ENTITY_BUFFER_STATE_COMPLETE) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, srcPipeId(%d), state(%d) frame(%d)",__FUNCTION__, __LINE__, srcPipeId, state, frame->getFrameCount());
        goto func_exit;
    }
    */

    if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
        CLOGE("ERR(%s[%d]):frame->getFrameState() == FRAME_STATE_SKIPPED. so, skip fusion. srcPipeId(%d), frame(%d)",
            __FUNCTION__, __LINE__, srcPipeId, frame->getFrameCount());
        goto func_exit;
    }

    srcBuf.index = -1;

    ret = frame->getDstBuffer(srcPipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, srcPipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, srcPipeId, ret, frame->getFrameCount());
        goto func_exit;
    }

    ///////////////////////////////////////////
    // dst buffer
    dstPipeId = PIPE_FUSION;
    dstBufMgr = m_scpBufferMgr;

    dstBuf.index = -2;
    bufIndex = -1;
    ret = dstBufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Buffer manager getBuffer fail, frame(%d), ret(%d)",
                __FUNCTION__, __LINE__, frame->getFrameCount(), ret);
        goto func_exit;
    }

    /* getMetadata to get buffer size */
    meta = (struct camera2_stream*)srcBuf.addr[srcBuf.planeCount - 1];
    if (meta == NULL) {
        CLOGE("ERR(%s[%d]):meta is NULL, skip fuision. srcPipeId(%d), ret(%d) frame(%d)", __FUNCTION__, __LINE__, srcPipeId, ret, frame->getFrameCount());
        goto func_exit;
    }

    output = meta->output_crop_region;

    /* csc and scaling */
    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = output[2];
    srcRect.h = output[3];
    srcRect.fullW = output[2];
    srcRect.fullH = output[3];
    srcRect.colorFormat = previewFormat;

    ret = frame->setSrcRect(dstPipeId, srcRect);

    if (m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getFusionSize(%d, %d) fail", __FUNCTION__, __LINE__, previewW, previewH);
        return loop;
    }

    dstRect = fusionDstRect;

    ret = frame->setDstRect(dstPipeId, dstRect);

    CLOGV("DEBUG(%s[%d]):do fusion : srcBuf(%d)(%d, %d, %d, %d) format(%d) actual(%x) -> dstBuf(%d)(%d, %d, %d, %d) format(%d)  actual(%x)",
        __FUNCTION__, __LINE__,
        srcBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        dstBuf.index, dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

    ret = m_setupEntity(dstPipeId, frame, &srcBuf, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setupEntity fail, srcPipeId(%d), ret(%d) frame(%d)", __FUNCTION__, __LINE__, dstPipeId, ret, frame->getFrameCount());
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, dstPipeId);
    m_previewFrameFactory->pushFrameToPipe(&frame, dstPipeId);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return loop;

func_exit:
    CLOGE("ERR(%s[%d]):do fusion fail frame(%d) ret(%d) src(%d) dst(%d)", __FUNCTION__, __LINE__, frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    ret = m_putFrameBuffer(frame, srcPipeId, SRC_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_putFrameBuffer(frame(%d), pipeId(%d), SRC_BUFFER_DIRECTION) fail",
            __FUNCTION__, __LINE__, frame->getFrameCount(), srcPipeId);
    }

    ret = m_putFrameBuffer(frame, srcPipeId, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_putFrameBuffer(frame(%d), pipeId(%d), DST_BUFFER_DIRECTION) fail",
            __FUNCTION__, __LINE__, frame->getFrameCount(), srcPipeId);
    }

    /* NDONE frame */
    frame->setEntityState(dstPipeId, ENTITY_STATE_FRAME_SKIP);
    frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, dstPipeId);
    m_previewFrameFactory->pushFrameToPipe(&frame, dstPipeId);

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

    return loop;
}
#endif

bool ExynosCamera::m_setBuffersThreadFunc(void)
{
    int ret = 0;

    ret = m_setBuffers();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_setBuffers failed, releaseBuffer", __FUNCTION__, __LINE__);

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
        CLOGE("ERR(%s[%d]):m_startPictureInternal failed", __FUNCTION__, __LINE__);

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
        CLOGE("ERR(%s[%d]):m_setPictureBuffer failed", __FUNCTION__, __LINE__);

        /* TODO: Need release buffers and error exit */

        return false;
    }

    return false;
}

#ifdef CAMERA_FAST_ENTRANCE_V1
bool ExynosCamera::m_fastenAeThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    m_fastenAeThreadResult = 0;

    /* frame manager start */
    m_frameMgr->start();

    /*
     * This is for updating parameter value at once.
     * This must be just before making factory
     */
    m_parameters->updateTpuParameters();

    if (m_previewFrameFactory->isCreated() == false) {
#if defined(SAMSUNG_EEPROM)
        if (m_use_companion == false
            && isEEprom(getCameraIdInternal()) == true) {
            if (m_eepromThread != NULL) {
                CLOGD("DEBUG(%s[%d]):eepromThread join.....", __FUNCTION__, __LINE__);
                m_eepromThread->join();
            } else {
                CLOGD("DEBUG(%s[%d]): eepromThread is NULL.", __FUNCTION__, __LINE__);
            }
            m_parameters->setRomReadThreadDone(true);
            CLOGD("DEBUG(%s[%d]):eepromThread joined", __FUNCTION__, __LINE__);
        }
#endif /* SAMSUNG_EEPROM */

#ifdef SAMSUNG_COMPANION
        if (m_use_companion == true) {
            ret = m_previewFrameFactory->precreate();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->precreate() failed", __FUNCTION__, __LINE__);
                goto err;
            }

            m_waitCompanionThreadEnd();
#ifdef SAMSUNG_COMPANION
            m_parameters->setRomReadThreadDone(true);
#endif
            ret = m_previewFrameFactory->postcreate();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->postcreate() failed", __FUNCTION__, __LINE__);
                goto err;
            }
        } else {
            ret = m_previewFrameFactory->create();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
        }
#else
        ret = m_previewFrameFactory->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
            goto err;
        }
#endif /* SAMSUNG_COMPANION */
        CLOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
    }

#ifdef USE_QOS_SETTING
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
#endif

    if (m_parameters->getUseFastenAeStable() == true
        && m_parameters->getDualMode() == false
        && m_isFirstStart == true) {

        ret = m_fastenAeStable();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
            goto err;
        } else {
            skipFrameCount = 0;
            m_parameters->setUseFastenAeStable(false);
        }
    }

#ifdef USE_FADE_IN_ENTRANCE
    if (m_parameters->getUseFastenAeStable() == true) {
        CLOGE("ERR(%s[%d]):consistency in skipFrameCount might be broken by FASTEN_AE and FADE_IN", __FUNCTION__, __LINE__);
    }
#endif

    m_parameters->setFrameSkipCount(skipFrameCount);
    m_fdFrameSkipCount = 0;

    /* one shot */
    return false;

err:
    m_fastenAeThreadResult = ret;

#ifdef SAMSUNG_COMPANION
    if (m_use_companion == true)
        m_waitCompanionThreadEnd();
#endif

#if defined(SAMSUNG_EEPROM)
    if (m_use_companion == false
        && isEEprom(getCameraIdInternal()) == true) {
        if (m_eepromThread != NULL)
            m_eepromThread->join();
        else
            CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
    }
#endif

    return false;
}

status_t ExynosCamera::m_waitFastenAeThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

    if(m_fastEntrance == true) {
        if (m_fastenAeThread != NULL) {
            m_fastenAeThread->join();
        } else {
            CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
        }
    }

    timer.stop();

    CLOGD("DEBUG(%s[%d]):fastenAeThread waiting time : duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());
    CLOGD("DEBUG(%s[%d]):fastenAeThread join", __FUNCTION__, __LINE__);

    return NO_ERROR;
}
#endif

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

    if (m_parameters->isReprocessing())
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

#if 0
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

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        /* get GSC src buffer */
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
        m_pictureFrameFactory->setOutputFrameQToPipe(m_dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);

        /* wait GSC for high resolution preview callback */
        CLOGI("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
        while (retryCountGSC > 0) {
            ret = m_dstGscReprocessingQ->waitAndPopProcessQ(&newFrame);
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
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);

        CLOGV("DEBUG(%s[%d]):high resolution preview callback", __FUNCTION__, __LINE__);
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
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
        ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_SCC_REPROCESSING));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
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
                m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread fail, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;
}
#endif

bool ExynosCamera::m_highResolutionCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    int retryCountGSC = 4;

    ExynosCameraFrame *newFrame = NULL;
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
    pipeReprocessingDst = PIPE_MCSC2_REPROCESSING;

    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_parameters->getHighResolutionCallbackMode() == false &&
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

    ret = m_getBufferManager(pipeReprocessingDst, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
        return ret;
    }

    /* wait MCSC2 */
    CLOGW("INFO(%s[%d]):wait MCSC2 output", __FUNCTION__, __LINE__);
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

    ret = newFrame->setEntityState(pipeReprocessingSrc, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeReprocessingSrc, ret);
        return ret;
    }
    CLOGW("INFO(%s[%d]):MCSC2 output done", __FUNCTION__, __LINE__);

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
        ret = newFrame->getDstBuffer(pipeReprocessingSrc, &reprocessingDstBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeReprocessingDst));
        if (ret < 0 || reprocessingDstBuffer.index < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeReprocessingDst(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
            goto CLEAN;
        }

        /* get preview callback heap */
        camera_memory_t *previewCallbackHeap = NULL;
        previewCallbackHeap = m_getMemoryCb(reprocessingDstBuffer.fd[0], reprocessingDstBuffer.size[0], 1, m_callbackCookie);
        if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, reprocessingDstBuffer.size[0]);
            goto CLEAN;
        }

        CLOGV("DEBUG(%s[%d]):high resolution preview callback", __FUNCTION__, __LINE__);
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }

        previewCallbackHeap->release(previewCallbackHeap);

        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
            goto CLEAN;
        }
    } else {
        CLOGD("DEBUG(%s[%d]): Preview callback message disabled, skip callback", __FUNCTION__, __LINE__);
        ret = newFrame->getDstBuffer(pipeReprocessingSrc, &reprocessingDstBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeReprocessingDst));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeReprocessingDst(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
            goto CLEAN;
        }

        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
            goto CLEAN;
        }
    }

    if (newFrame != NULL) {
        newFrame->frameUnlock();

        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
            m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread complete, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;

CLEAN:

    if (reprocessingDstBuffer.index != -2) {
        ret = m_putBuffers(bufferMgr, reprocessingDstBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_putBuffers fail, pipeReprocessingDst(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeReprocessingDst, ret);
            goto CLEAN;
        }
    }

    if (newFrame != NULL) {
        newFrame->printEntity();

        newFrame->frameUnlock();

        ret = m_deleteFrame(&newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_deleteFrame fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }

    if(m_flagThreadStop != true) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0 ||
                m_parameters->getHighResolutionCallbackMode() == true) {
            CLOGD("DEBUG(%s[%d]):highResolutionCallbackQ size(%d), highResolutionCallbackMode(%s), start again",
                    __FUNCTION__, __LINE__,
                    m_highResolutionCallbackQ->getSizeOfProcessQ(),
                    (m_parameters->getHighResolutionCallbackMode() == true)? "TRUE" : "FALSE");
            loop = true;
        }
    }

    CLOGI("INFO(%s[%d]):high resolution callback thread fail, loop(%d)", __FUNCTION__, __LINE__, loop);

    /* one shot */
    return loop;
}

bool ExynosCamera::m_facedetectThreadFunc(void)
{
    int32_t status = 0;
    bool loop = true;

    int index = 0;
    int count = 0;

    ExynosCameraFrame *newFrame = NULL;
    uint32_t frameCnt = 0;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
    uint32_t pipeId = PIPE_VC1;
    int ret = 0;
#endif

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s):preview is stopped, thread stop", __FUNCTION__);
        loop = false;
        goto func_exit;
    }

    status = m_facedetectQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d) m_flagStartFaceDetection(%d)", __FUNCTION__, __LINE__, m_flagThreadStop, m_flagStartFaceDetection);
        loop = false;
        goto func_exit;
    }

    if (status < 0) {
        if (status == TIMED_OUT) {
            /* Face Detection time out is not meaningful */
        } else {
            /* TODO: doing exception handling */
            CLOGE("ERR(%s):wait and pop fail, status(%d)", __FUNCTION__, status);
        }
        goto func_exit;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getDepthCallbackOnPreview()) {
        status = m_depthCallbackQ->waitAndPopProcessQ(&depthMapBuffer);
        if (status < 0) {
            if (status == TIMED_OUT) {
                /* Face Detection time out is not meaningful */
                CLOGE("ERR(%s):m_depthCallbackQ time out, status(%d)", __FUNCTION__, status);
            } else {
                /* TODO: doing exception handling */
                CLOGE("ERR(%s):wait and pop fail, status(%d)", __FUNCTION__, status);
            }
        }

        if (depthMapBuffer.index != -2) {
            newFrame->setRequest(pipeId, true);
            ret = newFrame->setDstBuffer(pipeId, depthMapBuffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
            }
        }
    }
#endif

    count = m_facedetectQ->getSizeOfProcessQ();
    if (count >= MAX_FACEDETECT_THREADQ_SIZE) {
        if (newFrame != NULL) {
            CLOGE("ERR(%s[%d]):m_facedetectQ  skipped QSize(%d) frame(%d)",
                    __FUNCTION__, __LINE__,  count, newFrame->getFrameCount());

#ifdef SUPPORT_DEPTH_MAP
            if (newFrame->getRequest(pipeId) == true) {
                depthMapBuffer.index = -2;
                ret = newFrame->getDstBuffer(pipeId, &depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
                }

                ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                            __FUNCTION__, __LINE__);
                }
            }
#endif
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
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
                        CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
                    }

                    ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                                __FUNCTION__, __LINE__);
                    }
                }
#endif
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
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
                    CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
                }

                ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                            __FUNCTION__, __LINE__);
                }
            }
#endif
            CLOGE("ERR(%s[%d]) m_doFdCallbackFunc failed(%d).", __FUNCTION__, __LINE__, status);
        }
    }

    if (m_facedetectQ->getSizeOfProcessQ() > 0) {
        loop = true;
    }

    if (newFrame != NULL) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

func_exit:

    return loop;
}

void ExynosCamera::m_createThreads(void)
{
    m_mainThread = new mainCameraThread(this, &ExynosCamera::m_mainThreadFunc, "ExynosCameraThread", PRIORITY_URGENT_DISPLAY);
    CLOGV("DEBUG(%s):mainThread created", __FUNCTION__);

    m_previewThread = new mainCameraThread(this, &ExynosCamera::m_previewThreadFunc, "previewThread", PRIORITY_DISPLAY);
    CLOGV("DEBUG(%s):previewThread created", __FUNCTION__);

    /*
     * In here, we cannot know single, dual scenario.
     * So, make all threads.
     */
    /* if (m_parameters->isFlite3aaOtf() == true) { */
    if (1) {
        m_mainSetupQThread[INDEX(PIPE_FLITE)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupFLITE, "mainThreadQSetupFLITE", PRIORITY_URGENT_DISPLAY);
        CLOGV("DEBUG(%s):mainThreadQSetupFLITEThread created", __FUNCTION__);

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
        CLOGV("DEBUG(%s):mainThreadQSetup3AAThread created", __FUNCTION__);

    }
    m_setBuffersThread = new mainCameraThread(this, &ExynosCamera::m_setBuffersThreadFunc, "setBuffersThread");
    CLOGV("DEBUG(%s):setBuffersThread created", __FUNCTION__);

    m_startPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_startPictureInternalThreadFunc, "startPictureInternalThread");
    CLOGV("DEBUG(%s):startPictureInternalThread created", __FUNCTION__);

    m_startPictureBufferThread = new mainCameraThread(this, &ExynosCamera::m_startPictureBufferThreadFunc, "startPictureBufferThread");
    CLOGV("DEBUG(%s):startPictureBufferThread created", __FUNCTION__);

    m_prePictureThread = new mainCameraThread(this, &ExynosCamera::m_prePictureThreadFunc, "prePictureThread");
    CLOGV("DEBUG(%s):prePictureThread created", __FUNCTION__);

    m_pictureThread = new mainCameraThread(this, &ExynosCamera::m_pictureThreadFunc, "PictureThread");
    CLOGV("DEBUG(%s):pictureThread created", __FUNCTION__);

    m_postPictureThread = new mainCameraThread(this, &ExynosCamera::m_postPictureThreadFunc, "postPictureThread");
    CLOGV("DEBUG(%s):postPictureThread created", __FUNCTION__);

    m_recordingThread = new mainCameraThread(this, &ExynosCamera::m_recordingThreadFunc, "recordingThread");
    CLOGV("DEBUG(%s):recordingThread created", __FUNCTION__);

    m_autoFocusThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusThreadFunc, "AutoFocusThread");
    CLOGV("DEBUG(%s):autoFocusThread created", __FUNCTION__);

    m_autoFocusContinousThread = new mainCameraThread(this, &ExynosCamera::m_autoFocusContinousThreadFunc, "AutoFocusContinousThread");
    CLOGV("DEBUG(%s):autoFocusContinousThread created", __FUNCTION__);

    m_facedetectThread = new mainCameraThread(this, &ExynosCamera::m_facedetectThreadFunc, "FaceDetectThread");
    CLOGV("DEBUG(%s):FaceDetectThread created", __FUNCTION__);

    m_monitorThread = new mainCameraThread(this, &ExynosCamera::m_monitorThreadFunc, "monitorThread");
    CLOGV("DEBUG(%s):monitorThread created", __FUNCTION__);

    m_jpegCallbackThread = new mainCameraThread(this, &ExynosCamera::m_jpegCallbackThreadFunc, "jpegCallbackThread");
    CLOGV("DEBUG(%s):jpegCallbackThread created", __FUNCTION__);

    m_zoomPreviwWithCscThread = new mainCameraThread(this, &ExynosCamera::m_zoomPreviwWithCscThreadFunc, "zoomPreviwWithCscQThread");
    CLOGV("DEBUG(%s):zoomPreviwWithCscQThread created", __FUNCTION__);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_fusionThread = new mainCameraThread(this, &ExynosCamera::m_fusionThreadFunc, "fusionThread");
    CLOGD("DEBUG(%s):m_fusionThread created", __FUNCTION__);
#endif

    /* saveThread */
    char threadName[20];
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        snprintf(threadName, sizeof(threadName), "jpegSaveThread%d", threadNum);
        m_jpegSaveThread[threadNum] = new mainCameraThread(this, &ExynosCamera::m_jpegSaveThreadFunc, threadName);
        CLOGV("DEBUG(%s):%s created", __FUNCTION__, threadName);
    }

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    CLOGV("DEBUG(%s):highResolutionCallbackThread created", __FUNCTION__);


    /* Shutter callback */
    m_shutterCallbackThread = new mainCameraThread(this, &ExynosCamera::m_shutterCallbackThreadFunc, "shutterCallbackThread");
    CLOGV("DEBUG(%s):shutterCallbackThread created", __FUNCTION__);

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastenAeThread = new mainCameraThread(this, &ExynosCamera::m_fastenAeThreadFunc, "fastenAeThread", PRIORITY_URGENT_DISPLAY);
    CLOGV("DEBUG(%s):m_fastenAeThread created", __FUNCTION__);
#endif
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
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_NUM_NUM_BAYER_BUFFERS;
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
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.init_bayer_buffers = FPS120_NUM_NUM_BAYER_BUFFERS;
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
    m_frameMgr = new ExynosCameraFrameManager("FRAME MANAGER", m_cameraId, FRAMEMGR_OPER::SLIENT, 100, 150);

    worker = new CreateWorker("CREATE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT, 200);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::CREATE, worker);

    worker = new DeleteWorker("DELETE FRAME WORKER", m_cameraId, FRAMEMGR_OPER::SLIENT);
    m_frameMgr->setWorker(FRAMEMGR_WORKER::DELETE, worker);

    sp<KeyBox> key = new KeyBox("FRAME KEYBOX", m_cameraId);

    m_frameMgr->setKeybox(key);

    return NO_ERROR;
}

status_t ExynosCamera::m_initFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
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

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);
    return ret;
}

status_t ExynosCamera::m_deinitFrameFactory(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_previewFrameFactory != NULL
        && m_previewFrameFactory->isCreated() == true) {
        ret = m_previewFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):m_previewFrameFactory destroy fail", __FUNCTION__, __LINE__);
    }

    if (m_reprocessingFrameFactory != NULL
        && m_reprocessingFrameFactory->isCreated() == true) {
        ret = m_reprocessingFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory destroy fail", __FUNCTION__, __LINE__);
    }

    if (m_visionFrameFactory != NULL
        && m_visionFrameFactory->isCreated() == true) {
        ret = m_visionFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):m_visionFrameFactory destroy fail", __FUNCTION__, __LINE__);
    }

    SAFE_DELETE(m_previewFrameFactory);
    CLOGD("DEBUG(%s[%d]):m_previewFrameFactory destroyed", __FUNCTION__, __LINE__);
    SAFE_DELETE(m_reprocessingFrameFactory);
    CLOGD("DEBUG(%s[%d]):m_reprocessingFrameFactory destroyed", __FUNCTION__, __LINE__);
    /*
       COUTION: m_pictureFrameFactory is only a pointer, just refer preview or reprocessing frame factory's instance.
       So, this pointer shuld be set null when camera HAL is deinitializing.
     */
    m_pictureFrameFactory = NULL;
    CLOGD("DEBUG(%s[%d]):m_pictureFrameFactory destroyed", __FUNCTION__, __LINE__);
    SAFE_DELETE(m_visionFrameFactory);
    CLOGD("DEBUG(%s[%d]):m_visionFrameFactory destroyed", __FUNCTION__, __LINE__);

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

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
    CLOGI("INFO(%s[%d]):HW Preview width x height = %dx%d",
            __FUNCTION__, __LINE__, hwPreviewW, hwPreviewH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
    CLOGI("INFO(%s[%d]):HW Picture width x height = %dx%d",
            __FUNCTION__, __LINE__, hwPictureW, hwPictureH);
    m_parameters->getMaxPictureSize(&pictureMaxW, &pictureMaxH);
    CLOGI("INFO(%s[%d]):Picture MAX width x height = %dx%d",
            __FUNCTION__, __LINE__, pictureMaxW, pictureMaxH);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("INFO(%s[%d]):HW Sensor(HighSpeed) MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH);
        m_parameters->getHwPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("INFO(%s[%d]):HW Preview(HighSpeed) MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, previewMaxW, previewMaxH);
    } else {
        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("INFO(%s[%d]):Sensor MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH);
        m_parameters->getMaxPreviewSize(&previewMaxW, &previewMaxH);
        CLOGI("INFO(%s[%d]):Preview MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, previewMaxW, previewMaxH);
    }

    hwPreviewFormat = m_parameters->getHwPreviewFormat();

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    ExynosRect fusionSrcRect;
    ExynosRect fusionDstRect;

    if (m_parameters->isFusionEnabled() == true) {
        ret = m_parameters->getFusionSize(hwPreviewW, hwPreviewH, &fusionSrcRect, &fusionDstRect);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getFusionSize(%d, %d)) fail", __FUNCTION__, __LINE__, hwPreviewW, hwPreviewH);
            return ret;
        }
        CLOGI("(%s):Fusion src width x height = %dx%d", __FUNCTION__, fusionSrcRect.w, fusionDstRect.h);
        CLOGI("(%s):Fusion dst width x height = %dx%d", __FUNCTION__, fusionDstRect.w, fusionDstRect.h);
    }
#endif

#if (SUPPORT_BACK_HW_VDIS || SUPPORT_FRONT_HW_VDIS)
    /*
     * we cannot expect TPU on or not, when open() api.
     * so extract memory TPU size
     */
    int w = 0, h = 0;
    m_parameters->calcNormalToTpuSize(previewMaxW, previewMaxH, &w, &h);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Hw vdis buffer calulation fail src(%d x %d) dst(%d x %d)",__FUNCTION__, __LINE__, previewMaxW, previewMaxH, w, h);
    }
    previewMaxW = w;
    previewMaxH = h;
    CLOGI("(%s): TPU based Preview MAX width x height = %dx%d", __FUNCTION__, previewMaxW, previewMaxH);
#endif

    /* Depth map buffer */
#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getUseDepthMap()) {
        int depthMapW = 0, depthMapH = 0;
        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):Failed to getDepthMapSize.", __FUNCTION__, __LINE__);
            return ret;
        } else {
            CLOGI("INFO(%s[%d]):Depth Map Size width x height = %dx%d",
                    __FUNCTION__, __LINE__, depthMapW, depthMapH);
        }

        bayerFormat     = DEPTH_MAP_FORMAT;
        bytesPerLine[0] = getBayerLineSize(depthMapW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(depthMapW, depthMapH, bayerFormat);
        planeCount      = 2;
        maxBufferCount  = NUM_DEPTHMAP_BUFFERS;
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_depthMapBufferMgr,
                planeCount, planeSize, bytesPerLine,
                maxBufferCount, maxBufferCount,
                type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to allocate Depth Map Buffers. bufferCount %d",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGI("INFO(%s[%d]):m_allocBuffers(DepthMap Buffer) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, depthMapW, depthMapH,
            planeSize[0], planeCount, maxBufferCount);
    }
#endif

    /* 3AA source(for shot) Buffer */
    planeCount      = 2;
    planeSize[0]    = 32 * 64 * 2;
    maxBufferCount  = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
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

        ret = m_allocBuffers(m_ispBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_ispBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);

        CLOGI("INFO(%s[%d]):m_allocBuffers(ISP Buffer) %d x %d, planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, ispBufferW, ispBufferH, planeSize[0], planeCount, maxBufferCount);
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

        if (m_parameters->getTpuEnabledMode() == true) {
            ispDmaOutAlignW = CAMERA_TPU_CHUNK_ALIGN_W;
            ispDmaOutAlignH = CAMERA_TPU_CHUNK_ALIGN_H;
        }

#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        tpuBufferW = bdsRect.w;
        tpuBufferH = bdsRect.h;
#else
        tpuBufferW = previewMaxW;
        tpuBufferH = previewMaxH;
#endif

        planeCount      = 2;
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

        ret = m_allocBuffers(m_hwDisBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, false);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_hwDisBufferMgr m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

#ifdef CAMERA_PACKED_BAYER_ENABLE
        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

        CLOGI("INFO(%s[%d]):m_allocBuffers(hwDis Buffer) %d x %d, planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, tpuBufferW, tpuBufferH, planeSize[0], planeCount, maxBufferCount);
    }

    /* Preview(MCSC0) Buffer */
    memset(planeSize, 0, sizeof(planeSize));

    ret = getYuvFormatInfo(hwPreviewFormat, &bpp, &planeCount);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getYuvFormatInfo(hwPreviewFormat(%x)) fail",
            __FUNCTION__, __LINE__, hwPreviewFormat);
        return INVALID_OPERATION;
    }

    // for meta
    planeCount += 1;

    if(m_parameters->increaseMaxBufferOfPreview() == true)
        maxBufferCount = m_parameters->getPreviewBufferCount();
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;

    if (m_previewWindow == NULL)
        needMmap = true;
    else
        needMmap = false;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {
        // hack for debugging
        needMmap = true;

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, fusionDstRect.w, fusionDstRect.h);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneSize(hwPreviewFormat(%x), fusionDstRect(%d x %d)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat, fusionDstRect.w, fusionDstRect.h);
            return INVALID_OPERATION;
        }

        ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
            fusionDstRect.w, fusionDstRect.h, fusionDstRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_LIMITED_RANGE),
            needMmap);
    } else
#endif
    {
        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, hwPreviewW, hwPreviewH);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneSize(hwPreviewFormat(%x), hwPreview(%d x %d)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat, hwPreviewW, hwPreviewH);
            return INVALID_OPERATION;
        }

        ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
    }

    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(Preview Buffer) %d x %d, planeSize(%d+%d), planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, hwPreviewW, hwPreviewH, planeSize[0], planeSize[1], planeCount, maxBufferCount);

    /* for zoom scaling by external scaler */
    /*
     * only alloc in back case.
     * dual's front doesn't need m_zoomScalerBufferMgr, by src(m_sccBufferMgr), dst(m_scpBufferMgr)
     */
    if ((m_parameters->getSupportedZoomPreviewWIthScaler()) &&
        ! ((m_parameters->getDualMode() == true) &&
           (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1))) {
        int scalerW = hwPreviewW;
        int scalerH = hwPreviewH;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true) {
            scalerW = fusionSrcRect.w;
            scalerH = fusionSrcRect.h;
        }
#endif
        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, scalerW, scalerH);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneSize(hwPreviewFormat(%x), scaler(%d x %d)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat, scalerW, scalerH);
            return INVALID_OPERATION;
        }

        ret = getYuvFormatInfo(hwPreviewFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat);
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true) {
            // hack for debugging
            needMmap = true;

            ret = m_allocBuffers(m_zoomScalerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
                fusionSrcRect.w, fusionSrcRect.h, fusionSrcRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_LIMITED_RANGE),
                needMmap);
        } else
#endif
        {
            ret = m_allocBuffers(m_zoomScalerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, needMmap);
        }
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_scpBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {

        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, fusionSrcRect.w, fusionSrcRect.h);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvPlaneSize(hwPreviewFormat(%x), fusionSrcRect(%d x %d)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat, fusionSrcRect.w, fusionSrcRect.h);
            return INVALID_OPERATION;
        }

        ret = getYuvFormatInfo(hwPreviewFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                __FUNCTION__, __LINE__, hwPreviewFormat);
            return INVALID_OPERATION;
        }

        // for meta
        planeCount += 1;

        if (m_parameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;


        bool needMmap = false;

        // hack for debugging
        needMmap = true;

        ret = m_allocBuffers(m_fusionBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
            fusionSrcRect.w, fusionSrcRect.h, fusionSrcRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_LIMITED_RANGE),
            needMmap);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_fusionBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }
#endif

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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {
        stride = fusionSrcRect.w;
    }
#endif

    m_parameters->setHwPreviewStride(stride);

    /* Do not allocate SCC buffer */
#if 0
    if (m_parameters->isSccCapture() == true) {
        m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
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
        if (m_parameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

        ret = m_allocBuffers(m_sccBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true, false);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_sccBufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }
#endif

    ret = m_setVendorBuffers();
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):m_setVendorBuffers fail, ret(%d)",
                __FUNCTION__, __LINE__, ret);

    ret = m_setPreviewCallbackBuffer();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
        return ret;
    }

    CLOGI("INFO(%s[%d]):alloc buffer done - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setPictureBuffer(void)
{
    status_t ret = NO_ERROR;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int pictureW = 0, pictureH = 0;
    int planeCount = 0;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int pictureFormat = m_parameters->getHwPictureFormat();
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxPictureSize(&pictureW, &pictureH);
    if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
        if (pictureFormat == V4L2_PIX_FMT_NV21M) {
            planeCount      = 2;
            planeSize[0]    = pictureW * pictureH;
            planeSize[1]    = pictureW * pictureH / 2;
        } else {
            planeCount      = 1;
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
        }

        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_parameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
        else
            minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_gscBufferMgr, planeCount, planeSize, bytesPerLine,
                                minBufferCount, maxBufferCount, type, allocMode, false, false);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }

        CLOGI("INFO(%s[%d]):m_allocBuffers(GSC Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, pictureW, pictureH, planeCount, maxBufferCount);

    }

    if (m_hdrEnabled == false) {
        planeCount      = 1;
        planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
        // Pre-allocate certain amount of buffers enough to fed into 3 JPEG save threads.
        if (m_parameters->getSeriesShotCount() > 0)
            minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
        else
            minBufferCount  = 1;
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_picture_buffers;

        CLOGD("DEBUG(%s[%d]): jpegBuffer picture(%dx%d) size(%d)", __FUNCTION__, __LINE__, pictureW, pictureH, planeSize[0]);

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
                                minBufferCount, maxBufferCount, type, allocMode, false, true);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s:%d):jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, NUM_REPROCESSING_BUFFERS);

        CLOGI("INFO(%s[%d]):m_allocBuffers(JPEG Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, pictureW, pictureH, planeCount, maxBufferCount);
    }

    ret = m_setVendorPictureBuffer();
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):m_setVendorBuffers fail, ret(%d)",
                __FUNCTION__, __LINE__, ret);

    return ret;
}

status_t ExynosCamera::m_releaseBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    int ret = 0;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }

#ifdef DEBUG_RAWDUMP
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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }
#endif
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }
    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
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
        CLOGE("ERR(%s[%d]):m_releaseVendorBuffers fail, ret(%d)",
                __FUNCTION__, __LINE__, ret);

    CLOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

bool ExynosCamera::m_startFaceDetection(void)
{
    if (m_flagStartFaceDetection == true) {
        CLOGD("DEBUG(%s):Face detection already started..", __FUNCTION__);
        return true;
    }

    if (m_parameters->getDualMode() == true
        && getCameraIdInternal() == CAMERA_ID_BACK
       ) {
        CLOGW("WRN(%s[%d]):On dual mode, Face detection disable!", __FUNCTION__, __LINE__);
        m_enableFaceDetection(false);
        m_flagStartFaceDetection = false;
    } else {
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
                CLOGE("ERR(%s[%d]):setFaceDetection(%d)", __FUNCTION__, __LINE__, true);
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
            CLOGE("ERR(%s[%d]):startFaceDetection recordingQ(%d)", __FUNCTION__, __LINE__, m_facedetectQ->getSizeOfProcessQ());
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

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    bufferCount = m_parameters->getPreviewBufferCount();
#endif

    if (m_previewCallbackBufferMgr == NULL) {
        CLOGE("ERR(%s[%d]): m_previewCallbackBufferMgr is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_previewCallbackBufferMgr->isAllocated() == true) {
        if (m_parameters->getRestartPreview() == true) {
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

    ret = m_allocBuffers(m_previewCallbackBufferMgr,
                        planeCount,
                        planeSize,
                        bytesPerLine,
                        bufferCount,
                        bufferCount,
                        type,
                        true,
                        false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d/%d) fail",
            __FUNCTION__, __LINE__, bufferCount, bufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(Preview Callback Buffer) %d x %d, planeSize(%d), planeCount(%d), bufferCount(%d)",
            __FUNCTION__, __LINE__, previewW, previewH, planeSize[0], planeCount, bufferCount);

    return NO_ERROR;
}

status_t ExynosCamera::m_putBuffers(ExynosCameraBufferManager *bufManager, int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufManager != NULL) {
        ret = bufManager->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):Buffer manager putBuffer fail, index(%d), ret(%d)",
                    __FUNCTION__, __LINE__, bufIndex, ret);
    }

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
    int retryCount = 20; /* 2Sec */

retry_alloc:

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
        if (retryCount != 0
            && (type == EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE
                || type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE)) {
            CLOGE("ERR(%s[%d]: Alloc fail about reserved memory. retry alloc. (%d)", __FUNCTION__, __LINE__, retryCount);
            usleep(100000); /* 100ms */
            retryCount--;
            goto retry_alloc;
        }
        CLOGE("ERR(%s[%d]):alloc fail", __FUNCTION__, __LINE__);
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

    CLOGI("INFO(%s[%d]):setInfo(planeCount=%d, minBufCount=%d, maxBufCount=%d, width=%d height=%d stride=%d pixelFormat=%d)",
        __FUNCTION__, __LINE__, planeCount, reqBufCount, reqBufCount, width, height, stride, pixelFormat);

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

int ExynosCamera::m_getShotBufferIdex() const
{
    return NUM_PLANES(V4L2_PIX_2_HAL_PIXEL_FORMAT(SCC_OUTPUT_COLOR_FMT));
}

bool ExynosCamera::m_enableFaceDetection(bool toggle)
{
    CLOGD("DEBUG(%s[%d]) toggle : %d", __FUNCTION__, __LINE__, toggle);

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

status_t ExynosCamera::m_generateFrame(int32_t frameCount, ExynosCameraFrame **newFrame)
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

        if (m_parameters->isOwnScc(getCameraIdInternal()) == true)
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

status_t ExynosCamera::m_generateFrameReprocessing(ExynosCameraFrame **newFrame)
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

status_t ExynosCamera::m_setCallbackBufferInfo(ExynosCameraBuffer *callbackBuf, char *baseAddr)
{
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

status_t ExynosCamera::m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrame *frame)
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            scpNodeIndex = 0;
        else
#endif
            scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);

        srcBufMgr = m_sccBufferMgr;
        m_getBufferManager(gscPipe, &dstBufMgr, DST_BUFFER_DIRECTION);
    } else {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        dstNodeIndex = 0;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            scpNodeIndex = 0;
        else
#endif
            scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);

        srcBufMgr = m_zoomScalerBufferMgr;
        m_getBufferManager(gscPipe, &dstBufMgr, DST_BUFFER_DIRECTION);
    }

    state = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getSrcBufferState(gscPipe, &state);
    if( ret < 0 || state == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), entityState(%d) frame(%d)",__FUNCTION__, __LINE__, gscPipe, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;
    dstBuf.index = -1;

    ret = frame->getSrcBuffer(gscPipe, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    memcpy(dstBuf.addr[dstBuf.planeCount-1],srcBuf.addr[srcBuf.planeCount-1], sizeof(struct camera2_stream));

    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0 )
            srcBufMgr->cancelBuffer(srcBuf.index);
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setdst Buffer state failed(%d) frame(%d)", __FUNCTION__, __LINE__, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setdst Buffer failed(%d) frame(%d)", __FUNCTION__, __LINE__, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setdst Buffer state failed(%d) frame(%d)", __FUNCTION__, __LINE__, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setdst Buffer failed(%d) frame(%d)", __FUNCTION__, __LINE__, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }


    CLOGV("DEBUG(%s[%d]): syncPrviewWithCSC done(%d)", __FUNCTION__, __LINE__, ret);

    return ret;

func_exit:

    CLOGE("ERR(%s[%d]): sync with csc failed frame(%d) ret(%d) src(%d) dst(%d)", __FUNCTION__, __LINE__, frame->getFrameCount(), ret, srcBuf.index, dstBuf.index);

    srcBuf.index = -1;
    dstBuf.index = -1;

    /* 1. return buffer pipe done. */
    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    /* 2. do not return buffer dual front case, frameselector ownershipt for the buffer */
    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        /* dual front scenario use ispc buffer for capture, frameSelector ownership for buffer */
    } else {
        if (srcBuf.index >= 0)
            srcBufMgr->cancelBuffer(srcBuf.index);
    }

    /* 3. if the gsc dst buffer available, return the buffer. */
    ret = frame->getDstBuffer(gscPipe, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    if (dstBuf.index >= 0)
        dstBufMgr->cancelBuffer(dstBuf.index);

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
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    bool loop = false;
    ExynosCameraFrame *newFrame = NULL;
    camera2_shot_ext *shot_ext = NULL;

    ExynosCameraBuffer fliteReprocessingBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    int pipeId = 0;
    int bufPipeId = 0;
    bool isSrc = false;
    int retryCount = 3;

    if (m_hdrEnabled)
        retryCount = 15;

    if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
        bufPipeId = PIPE_SCC;
    } else {
        bufPipeId = PIPE_ISPC;
    }

    if (m_parameters->is3aaIspOtf() == true) {
        pipeId = PIPE_3AA;
    } else {
        pipeId = PIPE_ISP;
    }

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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    // update for capture meta data.
    if (m_parameters->isFusionEnabled() == true) {
        struct camera2_shot_ext temp_shot_ext;
        newFrame->getMetaData(&temp_shot_ext);

        if (m_parameters->setFusionInfo(&temp_shot_ext) != NO_ERROR) {
            CLOGE("DEBUG(%s[%d]):m_parameters->setFusionInfo() fail", __FUNCTION__, __LINE__);
        }
    }
#endif

    m_postProcessList.push_back(newFrame);

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_highResolutionCallbackQ->pushProcessQ(&newFrame);
    } else {
        m_dstSccReprocessingQ->pushProcessQ(&newFrame);
    }

    m_reprocessingCounter.decCount();

    CLOGI("INFO(%s[%d]):prePicture complete, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

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

    CLOGI("INFO(%s[%d]): prePicture fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());
    *pIsProcessed = false;   // Notify failure
    return loop;

}

status_t ExynosCamera::m_getAvailableRecordingCallbackHeapIndex(int *index)
{
    if (m_recordingBufferCount <= 0 || m_recordingBufferCount > MAX_BUFFERS) {
        CLOGE("ERR(%s[%d]):Invalid recordingBufferCount %d",
                __FUNCTION__, __LINE__,
                m_recordingBufferCount);

        return INVALID_OPERATION;
    } else if (m_recordingCallbackHeap == NULL) {
        CLOGE("ERR(%s[%d]):RecordingCallbackHeap is NULL",
                __FUNCTION__, __LINE__);

        return INVALID_OPERATION;
    }

    Mutex::Autolock aLock(m_recordingCallbackHeapAvailableLock);
    for (int i = 0; i < m_recordingBufferCount; i++) {
        if (m_recordingCallbackHeapAvailable[i] == true) {
            CLOGV("DEBUG(%s[%d]):Found recordingCallbackHeapIndex %d",
                    __FUNCTION__, __LINE__, i);

            *index = i;
            m_recordingCallbackHeapAvailable[i] = false;
            return NO_ERROR;
        }
    }

    CLOGW("WARN(%s[%d]):There is no available recordingCallbackHeapIndex",
            __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

status_t ExynosCamera::m_releaseRecordingCallbackHeap(struct VideoNativeHandleMetadata *addr)
{
    struct VideoNativeHandleMetadata *baseAddr = NULL;
    int recordingCallbackHeapIndex = -1;

    if (addr == NULL) {
        CLOGE("ERR(%s[%d]):Addr is NULL",
                __FUNCTION__, __LINE__);

        return BAD_VALUE;
    } else if (m_recordingCallbackHeap == NULL) {
        CLOGE("ERR(%s[%d]):RecordingCallbackHeap is NULL",
                __FUNCTION__, __LINE__);

        return INVALID_OPERATION;
    }

    /* Calculate the recordingCallbackHeap index base on address offest. */
    baseAddr = (struct VideoNativeHandleMetadata *) m_recordingCallbackHeap->data;
    recordingCallbackHeapIndex = (int) (addr - baseAddr);

    Mutex::Autolock aLock(m_recordingCallbackHeapAvailableLock);
    if (recordingCallbackHeapIndex < 0 || recordingCallbackHeapIndex >= m_recordingBufferCount) {
        CLOGE("ERR(%s[%d]):Invalid index %d. base %p addr %p offset %d",
                __FUNCTION__, __LINE__,
                recordingCallbackHeapIndex,
                baseAddr,
                addr,
                (int) (addr - baseAddr));

        return INVALID_OPERATION;
    } else if (m_recordingCallbackHeapAvailable[recordingCallbackHeapIndex] == true) {
        CLOGW("WARN(%s[%d]):Already available index %d. base %p addr %p offset %d",
                __FUNCTION__, __LINE__,
                recordingCallbackHeapIndex,
                baseAddr,
                addr,
                (int) (addr - baseAddr));
    }

    CLOGV("DEBUG(%s[%d]):Release recordingCallbackHeapIndex %d.",
            __FUNCTION__, __LINE__, recordingCallbackHeapIndex);

    m_recordingCallbackHeapAvailable[recordingCallbackHeapIndex] = true;

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseRecordingBuffer(int bufIndex)
{
    status_t ret = NO_ERROR;

    if (bufIndex < 0 || bufIndex >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, m_recordingBufferCount, bufIndex);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = m_putBuffers(m_recordingBufferMgr, bufIndex);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
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
#if 0
        curFrame->printEntity();
#else
        curFrame->printNotDoneEntity();
#endif
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

    CLOGD("DEBUG(%s):remaining frame(%zu), we remove them all", __FUNCTION__, list->size());

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

        if (direction == SRC_BUFFER_DIRECTION) {
            ret = newFrame->getSrcBuffer(pipeId, &deleteSccBuffer);
        } else {
            if(m_previewFrameFactory == NULL) {
                return INVALID_OPERATION;
            }
            ret = newFrame->getDstBuffer(pipeId, &deleteSccBuffer, m_previewFrameFactory->getNodeType(pipeId));
        }
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

    CLOGV("DEBUG(%s):BufferManager(%s) created", __FUNCTION__, name);

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

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::m_getSyncLogId(void)
{
    return ++cameraSyncLogId;
}
#endif

void ExynosCamera::m_dump(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    m_printExynosCameraInfo(__FUNCTION__);

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->dump();

    if (m_bayerBufferMgr != NULL)
        m_bayerBufferMgr->dump();

#ifdef DEBUG_RAWDUMP
    if (m_parameters->getUsePureBayerReprocessing() == false) {
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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL)
        m_fusionBufferMgr->dump();
#endif

    if (m_ispReprocessingBufferMgr != NULL)
        m_ispReprocessingBufferMgr->dump();
    if (m_sccReprocessingBufferMgr != NULL)
        m_sccReprocessingBufferMgr->dump();
    if (m_thumbnailBufferMgr != NULL)
        m_thumbnailBufferMgr->dump();
    if (m_sccBufferMgr != NULL)
        m_sccBufferMgr->dump();
    if (m_gscBufferMgr != NULL)
        m_gscBufferMgr->dump();

    m_dumpVendor();

    return;
}

uint32_t ExynosCamera::m_getBayerPipeId(void)
{
    uint32_t pipeId = 0;

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
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
        CLOGI("INFO(%s[%d]): FPS_CHECK(id:%d), duration %lld / 30 = %lld ms. %lld fps",
            __FUNCTION__, __LINE__, pipeId, durationTime, durationTime / 30, 1000 / (durationTime / 30));
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

    shot_stream = (struct camera2_stream *)buffer->addr[buffer->planeCount-1];
    bayerFrameCount = shot_stream->fcount;
    outputInfo->cropRegion[0] = shot_stream->output_crop_region[0];
    outputInfo->cropRegion[1] = shot_stream->output_crop_region[1];
    outputInfo->cropRegion[2] = shot_stream->output_crop_region[2];
    outputInfo->cropRegion[3] = shot_stream->output_crop_region[3];

    memset(buffer->addr[buffer->planeCount-1], 0x0, sizeof(struct camera2_shot_ext));

    shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->planeCount-1];
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
            CLOGW("WRAN(%s[%d]):retry(%d) setupEntity for pipeId(%d)", __FUNCTION__, __LINE__, retry, pipeId);
    } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

    return ret;
}

status_t ExynosCamera::m_boostDynamicCapture(void)
{
    status_t ret = NO_ERROR;
#if 0 /* TODO: need to implementation for bayer */
    uint32_t pipeId = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC : PIPE_ISPC;
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
        CLOGW("WARN(%s[%d])Parameter srcPath is NULL. Change to Default Path", __FUNCTION__, __LINE__);
        snprintf(dstPath, dstSize, "%s/DCIM/Camera/", dir);
    }

    if (access(dstPath, 0)==0) {
        CLOGW("WARN(%s[%d]) success access dir = %s", __FUNCTION__, __LINE__, dstPath);
        return true;
    }

    CLOGW("WARN(%s[%d]) can't find dir = %s", __FUNCTION__, __LINE__, m_burstSavePath);

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
                            CLOGW("WARN(%s[%d]) change save path = %s", __FUNCTION__, __LINE__, dstPath);
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
    int nw, cnt = 0;
    uint32_t written = 0;
    char *data;

    fd = open(filePath, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        CLOGD("DEBUG(%s[%d]):failed to create file [%s]: %s",
                __FUNCTION__, __LINE__, filePath, strerror(errno));
        goto SAVE_ERR;
    }

    data = SaveBuf->addr[0];

    CLOGD("DEBUG(%s[%d]):(%s)file write start)", __FUNCTION__, __LINE__, filePath);
    while (written < SaveBuf->size[0]) {
        nw = ::write(fd, (const char *)(data) + written, SaveBuf->size[0] - written);

        if (nw < 0) {
            CLOGD("DEBUG(%s[%d]):failed to write file [%s]: %s",
                    __FUNCTION__, __LINE__, filePath, strerror(errno));
            break;
        }

        written += nw;
        cnt++;
    }
    CLOGD("DEBUG(%s[%d]):(%s)file write end)", __FUNCTION__, __LINE__, filePath);

    if (fd > 0)
        ::close(fd);

    if (chmod(filePath, 0664) < 0) {
        CLOGE("failed chmod [%s]", filePath);
    }
    if (chown(filePath, AID_MEDIA, AID_MEDIA_RW) < 0) {
        CLOGE("failed chown [%s] user(%d), group(%d)", filePath, AID_MEDIA, AID_MEDIA_RW);
    }

    return true;

SAVE_ERR:
    return false;
}

bool ExynosCamera::m_FileSaveFunc(char *filePath, char *saveBuf, unsigned int size)
{
    int fd = -1;
    int nw, cnt = 0;
    uint32_t written = 0;

    fd = open(filePath, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        CLOGD("DEBUG(%s[%d]):failed to create file [%s]: %s",
                __FUNCTION__, __LINE__, filePath, strerror(errno));
        goto SAVE_ERR;
    }

    CLOGD("DEBUG(%s[%d]):(%s)file write start)", __FUNCTION__, __LINE__, filePath);
    while (written < size) {
        nw = ::write(fd, (const char *)(saveBuf) + written, size - written);

        if (nw < 0) {
            CLOGD("DEBUG(%s[%d]):failed to write file [%s]: %s",
                    __FUNCTION__, __LINE__, filePath, strerror(errno));
            break;
        }

        written += nw;
        cnt++;
    }
    CLOGD("DEBUG(%s[%d]):(%s)file write end)", __FUNCTION__, __LINE__, filePath);

    if (fd > 0)
        ::close(fd);

    if (chmod(filePath, 0664) < 0) {
        CLOGE("failed chmod [%s]", filePath);
    }
    if (chown(filePath, AID_MEDIA, AID_MEDIA_RW) < 0) {
        CLOGE("failed chown [%s] user(%d), group(%d)", filePath, AID_MEDIA, AID_MEDIA_RW);
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
        CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);

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

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):============= ExynosCameraInfo call by %s", __FUNCTION__, __LINE__, funcName);
    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);

    CLOGD("DEBUG(%s[%d]):============= Scenario ============================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= getCameraId                 : %d", __FUNCTION__, __LINE__, m_parameters->getCameraId());
    CLOGD("DEBUG(%s[%d]):= getDualMode                 : %d", __FUNCTION__, __LINE__, m_parameters->getDualMode());
    CLOGD("DEBUG(%s[%d]):= getScalableSensorMode       : %d", __FUNCTION__, __LINE__, m_parameters->getScalableSensorMode());
    CLOGD("DEBUG(%s[%d]):= getRecordingHint            : %d", __FUNCTION__, __LINE__, m_parameters->getRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getEffectRecordingHint      : %d", __FUNCTION__, __LINE__, m_parameters->getEffectRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getDualRecordingHint        : %d", __FUNCTION__, __LINE__, m_parameters->getDualRecordingHint());
    CLOGD("DEBUG(%s[%d]):= getAdaptiveCSCRecording     : %d", __FUNCTION__, __LINE__, m_parameters->getAdaptiveCSCRecording());
    CLOGD("DEBUG(%s[%d]):= doCscRecording              : %d", __FUNCTION__, __LINE__, m_parameters->doCscRecording());
    CLOGD("DEBUG(%s[%d]):= needGSCForCapture           : %d", __FUNCTION__, __LINE__, m_parameters->needGSCForCapture(getCameraIdInternal()));
    CLOGD("DEBUG(%s[%d]):= getShotMode                 : %d", __FUNCTION__, __LINE__, m_parameters->getShotMode());
    CLOGD("DEBUG(%s[%d]):= getTpuEnabledMode           : %d", __FUNCTION__, __LINE__, m_parameters->getTpuEnabledMode());
    CLOGD("DEBUG(%s[%d]):= getHWVdisMode               : %d", __FUNCTION__, __LINE__, m_parameters->getHWVdisMode());
    CLOGD("DEBUG(%s[%d]):= get3dnrMode                 : %d", __FUNCTION__, __LINE__, m_parameters->get3dnrMode());

    CLOGD("DEBUG(%s[%d]):============= Internal setting ====================", __FUNCTION__, __LINE__);
    CLOGD("DEBUG(%s[%d]):= isFlite3aaOtf               : %d", __FUNCTION__, __LINE__, m_parameters->isFlite3aaOtf());
    CLOGD("DEBUG(%s[%d]):= is3aaIspOtf                 : %d", __FUNCTION__, __LINE__, m_parameters->is3aaIspOtf());
    CLOGD("DEBUG(%s[%d]):= isReprocessing              : %d", __FUNCTION__, __LINE__, m_parameters->isReprocessing());
    CLOGD("DEBUG(%s[%d]):= isReprocessing3aaIspOTF     : %d", __FUNCTION__, __LINE__, m_parameters->isReprocessing3aaIspOTF());
    CLOGD("DEBUG(%s[%d]):= getUsePureBayerReprocessing : %d", __FUNCTION__, __LINE__, m_parameters->getUsePureBayerReprocessing());

    int reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
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

    CLOGD("DEBUG(%s[%d]):= isSccCapture                : %d", __FUNCTION__, __LINE__, m_parameters->isSccCapture());

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    CLOGD("DEBUG(%s[%d]):= getDualCameraMode           : %d", __FUNCTION__, __LINE__, m_parameters->getDualCameraMode());
    CLOGD("DEBUG(%s[%d]):= isFusionEnabled             : %d", __FUNCTION__, __LINE__, m_parameters->isFusionEnabled());
#endif

    CLOGD("DEBUG(%s[%d]):============= size setting =======================", __FUNCTION__, __LINE__);
    m_parameters->getMaxSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getMaxSensorSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_parameters->getHwSensorSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwSensorSize             : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_parameters->getBnsSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getBnsSize                  : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_parameters->getPreviewBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getPreviewBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPreviewBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getHwPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPreviewSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;

        // we need to calculate with set by app.
        m_parameters->getPreviewSize(&w, &h);
        m_parameters->getFusionSize(w, h, &fusionSrcRect, &fusionDstRect);

        CLOGD("DEBUG(%s[%d]):= getFusionSize             : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
            fusionSrcRect.x, fusionSrcRect.y, fusionSrcRect.w, fusionSrcRect.h,
            fusionDstRect.x, fusionDstRect.y, fusionDstRect.w, fusionDstRect.h);
    }
#endif

    m_parameters->getPreviewSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPreviewSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBayerCropSize     : (%d, %d, %d, %d) -> (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getPictureBdsSize(&dstRect);
    CLOGD("DEBUG(%s[%d]):= getPictureBdsSize           : (%d, %d, %d, %d)", __FUNCTION__, __LINE__,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h);

    m_parameters->getHwPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getHwPictureSize            : %d x %d", __FUNCTION__, __LINE__, w, h);

    m_parameters->getPictureSize(&w, &h);
    CLOGD("DEBUG(%s[%d]):= getPictureSize              : %d x %d", __FUNCTION__, __LINE__, w, h);

    CLOGD("DEBUG(%s[%d]):===================================================", __FUNCTION__, __LINE__);
}

status_t ExynosCamera::m_putFrameBuffer(ExynosCameraFrame *frame, int pipeId, enum buffer_direction_type bufferDirectionType)
{
    status_t ret = NO_ERROR;

    ExynosCameraBuffer buffer;
    ExynosCameraBufferManager *bufferMgr = NULL;

    switch (bufferDirectionType) {
    case SRC_BUFFER_DIRECTION:
        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
        break;
    case DST_BUFFER_DIRECTION:
        ret = frame->getDstBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }
        break;
    default:
        CLOGE("ERR(%s[%d]):invalid bufferDirectionType(%d). so fail", __FUNCTION__, __LINE__, bufferDirectionType);
        return INVALID_OPERATION;
    }

    if (0 <= buffer.index) {
        ret = m_getBufferManager(pipeId, &bufferMgr, bufferDirectionType);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_getBufferManager(pipeId(%d), SRC_BUFFER_DIRECTION)", __FUNCTION__, __LINE__, pipeId);
            return ret;
        }

        if (bufferMgr == m_scpBufferMgr) {
            ret = bufferMgr->cancelBuffer(buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):cancelBuffer(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                return ret;
            }
        } else {
            ret = m_putBuffers(bufferMgr, buffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_putBuffers(%d) fail", __FUNCTION__, __LINE__, buffer.index);
                return ret;
            }
        }
    }

    return ret;
}

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
status_t ExynosCamera::m_syncPreviewWithFusion(int pipeId, ExynosCameraFrame *frame)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;

    ExynosRect srcRect, dstRect;

    ExynosCameraBuffer srcBuf;
    ExynosCameraBuffer dstBuf;

    ExynosCameraBufferManager *srcBufMgr = NULL;
    ExynosCameraBufferManager *dstBufMgr = NULL;

    int srcNodeIndex = -1;
    int dstNodeIndex = -1;

    int scpNodeIndex = -1;

    entity_state_t        state = ENTITY_STATE_FRAME_SKIP;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;

    // src
    srcNodeIndex = 0;

    ret = m_getBufferManager(pipeId, &srcBufMgr, SRC_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_getBufferManager(pipeId(%d), SRC_BUFFER_DIRECTION)", __FUNCTION__, __LINE__, pipeId);
        goto func_exit;
    }

    // dst
    if ((m_parameters->getDualMode() == true) &&
        (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
        dstNodeIndex = 0;
    } else {
        /*
         * fusion looking dstNodeIndex 0, not PIPE_MCSC0.
         * so, just set dstNodeIndex = 0;
         */
        //dstNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
        dstNodeIndex = 0;
    }

    ret = m_getBufferManager(pipeId, &dstBufMgr, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION)", __FUNCTION__, __LINE__, pipeId);
        goto func_exit;
    }

    // scp
    scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_MCSC0);

    ret = frame->getEntityState(pipeId, &state);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getEntityState(pipeId(%d)) fail, frame(%d)",
            __FUNCTION__, __LINE__, pipeId, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if(state == ENTITY_STATE_FRAME_SKIP) {
        CLOGE("ERR(%s[%d]):state == ENTITY_STATE_FRAME_SKIP. so, pipeId(%d), state(%d) frame(%d)",
            __FUNCTION__, __LINE__, pipeId, state, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    bufferState = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getSrcBufferState(pipeId, &bufferState);
    if( ret < 0 || bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), entityState(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, bufferState, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    bufferState = ENTITY_BUFFER_STATE_NOREQ;
    ret = frame->getDstBufferState(pipeId, &bufferState);
    if( ret < 0 || bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), entityState(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, bufferState, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    srcBuf.index = -1;
    dstBuf.index = -1;

    ret = frame->getSrcBuffer(pipeId, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->getDstBuffer(pipeId, &dstBuf, dstNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",__FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (dstBuf.addr[dstBuf.planeCount - 1] == NULL ||
        srcBuf.addr[srcBuf.planeCount - 1] == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):dstBuf.addr[%d](%p) or srcBuf.addr[%d](%p) weird, assert!!!!",
            __FUNCTION__, __LINE__,
            dstBuf.planeCount - 1, dstBuf.addr[dstBuf.planeCount - 1],
            srcBuf.planeCount - 1, srcBuf.addr[srcBuf.planeCount - 1]);
    }

    /* copy metadata src to dst */
    memcpy(dstBuf.addr[dstBuf.planeCount - 1], srcBuf.addr[srcBuf.planeCount - 1], sizeof(struct camera2_stream));

    ret = m_putFrameBuffer(frame, pipeId, SRC_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_putFrameBuffer(frame(%d), pipeId(%d), SRC_BUFFER_DIRECTION) fail",
            __FUNCTION__, __LINE__, frame->getFrameCount(), pipeId);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /*
     * after fusion lib, frame's dst pos is 0 (because fusion looking srcPos 0, dstPos 0)
     * so, after fusion lib, we will change the frame's dst pos as scpNodeIndex.
     */
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState(pipeId(%d), ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex(%d)) fail frame(%d)",
		__FUNCTION__, __LINE__, pipeId, scpNodeIndex, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBuffer(pipeId(%d), dstBuf(%d), scpNodeIndex(%d)) fail frame(%d)",
		__FUNCTION__, __LINE__, pipeId, dstBuf.index, scpNodeIndex, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
            __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
        __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    CLOGV("DEBUG(%s[%d]): m_syncPreviewWithFusion done. frame(%d) timeStamp(%d), ret(%d) src(%d) dst(%d)",
        __FUNCTION__, __LINE__, frame->getFrameCount(), (int)(ns2ms(frame->getTimeStamp())), ret, srcBuf.index, dstBuf.index);

    return ret;

func_exit:

    CLOGE("ERR(%s[%d]): sync with fusion failed. frame(%d) timeStamp(%d), ret(%d) src(%d) dst(%d)",
        __FUNCTION__, __LINE__, frame->getFrameCount(), (int)(ns2ms(frame->getTimeStamp())), ret, srcBuf.index, dstBuf.index);

    ret = m_putFrameBuffer(frame, pipeId, SRC_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_putFrameBuffer(frame(%d), pipeId(%d), SRC_BUFFER_DIRECTION) fail",
            __FUNCTION__, __LINE__, frame->getFrameCount(), pipeId);
    }

    // if error, we will skip this frame.
    ret = frame->setEntityState(pipeId, ENTITY_STATE_FRAME_SKIP);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState(%d, ENTITY_STATE_FRAME_SKIP) fail", __FUNCTION__, __LINE__, pipeId);
    }

    /*
     * when SKIP, after this function, buffer release is skipped also.
     * so, we must release here.
     */
    ret = m_putFrameBuffer(frame, pipeId, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_putFrameBuffer(frame(%d), pipeId(%d), DST_BUFFER_DIRECTION) fail",
            __FUNCTION__, __LINE__, frame->getFrameCount(), pipeId);
    }

    /*
     * after fusion lib, frame's dst pos is 0 (because fusion looking srcPos 0, dstPos 0)
     * so, after fusion lib, we will change the frame's dst pos as scpNodeIndex.
     */
    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState(pipeId(%d), ENTITY_BUFFER_STATE_REQUESTED, scpNodeIndex(%d)) fail frame(%d)",
            __FUNCTION__, __LINE__, pipeId, scpNodeIndex, frame->getFrameCount());
        ret = INVALID_OPERATION;
    }

    ret = frame->setDstBuffer(pipeId, dstBuf, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBuffer(pipeId(%d), dstBuf(%d), scpNodeIndex(%d)) fail frame(%d)",
            __FUNCTION__, __LINE__, pipeId, dstBuf.index, scpNodeIndex, frame->getFrameCount());
        ret = INVALID_OPERATION;
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR, scpNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
            __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):setDstBufferState fail, pipeId(%d), ret(%d) frame(%d)",
            __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
    }

    return INVALID_OPERATION;
}
#endif

}; /* namespace android */
