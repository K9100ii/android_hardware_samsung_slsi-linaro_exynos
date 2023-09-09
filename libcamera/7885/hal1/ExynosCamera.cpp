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
#ifdef TIME_LOGGER_ENABLE
#include "ExynosCameraTimeLogger.h"
#endif
#include "SecCameraUtil.h"

#include "ExynosCameraFrameReprocessingFactoryRemosaic.h"

namespace android {

#ifdef MONITOR_LOG_SYNC
uint32_t ExynosCamera::cameraSyncLogId = 0;
#endif
#ifdef TIME_LOGGER_LAUNCH_ENABLE
ExynosCameraTimeLogger g_timeLoggerLaunch;
int g_timeLoggerLaunchIndex;
#endif

#ifdef USE_DUAL_CAMERA
InternalExynosCameraBufferManager ExynosCamera::m_sharedBufferManager[SHARED_BUFFER_MANAGER_MAX];

ExynosCamera::ExynosCamera(int cameraId, camera_device_t *dev, bool flagDualCameraOpened)
#else
ExynosCamera::ExynosCamera(int cameraId, camera_device_t *dev)
#endif
{
    BUILD_DATE();

    checkAndroidVersion();

    m_cameraOriginId = cameraId;
    if (m_cameraOriginId == CAMERA_ID_SECURE) {
        m_cameraId = CAMERA_ID_FRONT;
        m_scenario = SCENARIO_SECURE;
    } else {
        m_cameraId = m_cameraOriginId;
        m_scenario = SCENARIO_NORMAL;
    }
    m_dev = dev;

#ifdef USE_DUAL_CAMERA
    m_flagDualCameraOpened = flagDualCameraOpened;
#endif

    initialize();
}

void ExynosCamera::initialize()
{
    CLOGI(" -IN-");

    ExynosCameraActivityUCTL *uctlMgr = NULL;
    memset(m_name, 0x00, sizeof(m_name));
    m_use_companion = isCompanion(m_cameraOriginId);
    m_cameraSensorId = getSensorId(m_cameraOriginId);

#ifdef DEBUG_CLASS_INFO
    m_dumpClassInfo();
#endif

    if (m_scenario == SCENARIO_SECURE) {
        initializeVision();
        CLOGI(" -OUT-");
        return;
    }

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
    m_vraBufferMgr = NULL;

    /* Frame manager */
    m_frameMgr = NULL;

    /* Main Buffer */
    /* Bayer buffer: Output buffer of Flite or 3AC */
    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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
#ifdef USE_VRA_GROUP
    /* VRA buffer */
    m_createInternalBufferManager(&m_vraBufferMgr, "DS_OUT_BUF");
#endif

    /* Reprocessing Buffer */
    /* ISP-Re buffer: Hand-shaking buffer of reprocessing 3AA-ISP */
    m_createInternalBufferManager(&m_ispReprocessingBufferMgr, "ISP_RE_BUF");
    /* SCC-Re buffer: Output buffer of reprocessing ISP(MCSC) */
#ifdef USE_DUAL_CAMERA
    /* singleton buffer manager */
    if (m_flagDualCameraOpened)
        m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "MCSC_RE_BUF", m_flagDualCameraOpened);
    else
#endif
    m_createInternalBufferManager(&m_sccReprocessingBufferMgr, "MCSC_RE_BUF");
    /* Thumbnail buffer: Output buffer of reprocessing ISP(MCSC) */
    m_createInternalBufferManager(&m_thumbnailBufferMgr, "THUMBNAIL_BUF");

    /* Capture Buffer */
    /* SCC buffer: Output buffer of preview ISP(MCSC) */
    m_createInternalBufferManager(&m_sccBufferMgr, "SCC_BUF");
    /* GSC buffer: Output buffer of external scaler */
    m_createInternalBufferManager(&m_gscBufferMgr, "GSC_BUF");
    /* GSC buffer: Output buffer of JPEG */
#ifdef USE_DUAL_CAMERA
    /* singleton buffer manager */
    if (m_flagDualCameraOpened)
        m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF", m_flagDualCameraOpened);
    else
#endif
    m_createInternalBufferManager(&m_jpegBufferMgr, "JPEG_BUF");

    /* Magic shot Buffer */
#ifdef USE_DUAL_CAMERA
    /* singleton buffer manager */
    if (m_flagDualCameraOpened)
        m_createInternalBufferManager(&m_postPictureBufferMgr, "POSTPICTURE_GSC_BUF", m_flagDualCameraOpened);
    else
#endif
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

#ifdef USE_DUAL_CAMERA
    /* fusion preview buffer */
    m_createInternalBufferManager(&m_fusionBufferMgr, "FUSION_BUF");
    /* fusion reprocessing buffer */
    m_createInternalBufferManager(&m_fusionReprocessingBufferMgr, "FUSION_REPROCESSING_BUF");
#endif

    /* Thumbnail scaling Buffer */
#ifdef USE_DUAL_CAMERA
    /* singleton buffer manager */
    if (m_flagDualCameraOpened)
        m_createInternalBufferManager(&m_thumbnailGscBufferMgr, "THUMBNAIL_GSC_BUF", m_flagDualCameraOpened);
    else
#endif
    m_createInternalBufferManager(&m_thumbnailGscBufferMgr, "THUMBNAIL_GSC_BUF");
#ifdef USE_MSC_CAPTURE
    m_createInternalBufferManager(&m_mscBufferMgr, "MSC_BUF");
#endif

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

    m_previewCallbackQ = new frame_queue_t(m_previewCallbackThread);
    m_previewCallbackQ->setWaitTime(1000000000);

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

#ifdef USE_DUAL_CAMERA
    m_syncReprocessingQ = new frame_queue_t;
    m_syncReprocessingQ->setWaitTime(1000000000);

    m_fusionReprocessingQ = new frame_queue_t;
    m_fusionReprocessingQ->setWaitTime(1000000000);
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
    m_sccCaptureSelector = NULL;
    m_autoFocusType = 0;
    m_hdrEnabled = false;
    m_hdrSkipedFcount = 0;
    m_nv21CaptureEnabled = false;
    m_doCscRecording = true;
    m_recordingBufferCount = NUM_RECORDING_BUFFERS;
    m_frameSkipCount = 0;
    m_isZSLCaptureOn = false;
    m_isSuccessedBufferAllocation = false;
    m_fdFrameSkipCount = 0;

    m_displayPreviewToggle = 0;
    m_previewCallbackToggle = 0;

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

#ifdef ERROR_ESD_STATE_COUNT
    m_esdErrorCount = 0;
#endif

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
    m_preLongExposureTime = 0;
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

#ifdef USE_LIB_ION_LEGACY
    m_ionClient = ion_open();
#else
    m_ionClient = exynos_ion_open();
#endif
    if(m_ionClient < 0) {
        CLOGE("m_ionClient ion_open() fail");
        m_ionClient = -1;
    }
    m_parameters->setIonClient(m_ionClient);

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

    if (m_scenario == SCENARIO_SECURE) {
        releaseVision();
        CLOGI("-OUT-");
        return;
    }

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastenAeThread != NULL) {
        CLOGI("wait m_fastenAeThread");
        m_fastenAeThread->requestExitAndWait();
        CLOGI("wait m_fastenAeThread end");
    } else {
        CLOGI("m_fastenAeThread is NULL");
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
        CLOGD("Parameters(Id=%d) destroyed", m_cameraId);
    }

    /* free all buffers */
    m_releaseBuffers();

    if (m_ionClient >= 0) {
#ifdef USE_LIB_ION_LEGACY
        ion_close(m_ionClient);
#else
        exynos_ion_close(m_ionClient);
#endif
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

    if (m_previewCallbackQ != NULL) {
        delete m_previewCallbackQ;
        m_previewCallbackQ = NULL;
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

#ifdef USE_DUAL_CAMERA
    if (m_syncReprocessingQ != NULL) {
        delete m_syncReprocessingQ;
        m_syncReprocessingQ = NULL;
    }

    if (m_fusionReprocessingQ != NULL) {
        delete m_fusionReprocessingQ;
        m_fusionReprocessingQ = NULL;
    }
#endif

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("BufferManager(bayerBufferMgr) destroyed");
    }

#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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

    if (m_vraBufferMgr != NULL) {
        delete m_vraBufferMgr;
        m_vraBufferMgr = NULL;
        CLOGD("BufferManager(m_vraBufferMgr) destroyed");
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

#ifdef USE_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        delete m_fusionBufferMgr;
        m_fusionBufferMgr = NULL;
        CLOGD("BufferManager(m_fusionBufferMgr) destroyed");
    }

    if (m_fusionReprocessingBufferMgr != NULL) {
        delete m_fusionReprocessingBufferMgr;
        m_fusionReprocessingBufferMgr = NULL;
        CLOGD("BufferManager(m_fusionReprocessingBufferMgr) destroyed");
    }
#endif

    if (m_thumbnailGscBufferMgr != NULL) {
#ifdef USE_DUAL_CAMERA
        if (!m_thumbnailGscBufferMgr->isShared())
#endif
        delete m_thumbnailGscBufferMgr;
        m_thumbnailGscBufferMgr = NULL;
        CLOGD("BufferManager(m_thumbnailGscBufferMgr) destroyed");
    }
#ifdef USE_MSC_CAPTURE
    if (m_mscBufferMgr != NULL) {
        delete m_mscBufferMgr;
        m_mscBufferMgr = NULL;
        CLOGD("BufferManager(m_mscBufferMgr) destroyed");
    }
#endif

    if (m_ispReprocessingBufferMgr != NULL) {
        delete m_ispReprocessingBufferMgr;
        m_ispReprocessingBufferMgr = NULL;
        CLOGD("BufferManager(ispReprocessingBufferMgr) destroyed");
    }

    if (m_sccReprocessingBufferMgr != NULL) {
#ifdef USE_DUAL_CAMERA
        if (!m_sccReprocessingBufferMgr->isShared())
#endif
        delete m_sccReprocessingBufferMgr;
        m_sccReprocessingBufferMgr = NULL;
        CLOGD("BufferManager(sccReprocessingBufferMgr) destroyed");
    }

    if (m_thumbnailBufferMgr != NULL) {
        delete m_thumbnailBufferMgr;
        m_thumbnailBufferMgr = NULL;
        CLOGD("BufferManager(sccReprocessingBufferMgr) destroyed");
    }

    if (m_sccBufferMgr != NULL) {
        delete m_sccBufferMgr;
        m_sccBufferMgr = NULL;
        CLOGD("BufferManager(sccBufferMgr) destroyed");
    }

    if (m_gscBufferMgr != NULL) {
        delete m_gscBufferMgr;
        m_gscBufferMgr = NULL;
        CLOGD("BufferManager(gscBufferMgr) destroyed");
    }

    if (m_jpegBufferMgr != NULL) {
#ifdef USE_DUAL_CAMERA
        if (!m_jpegBufferMgr->isShared())
#endif
        delete m_jpegBufferMgr;
        m_jpegBufferMgr = NULL;
        CLOGD("BufferManager(jpegBufferMgr) destroyed");
    }

    if (m_postPictureBufferMgr != NULL) {
#ifdef USE_DUAL_CAMERA
        if (!m_postPictureBufferMgr->isShared())
#endif
        delete m_postPictureBufferMgr;
        m_postPictureBufferMgr = NULL;
        CLOGD("BufferManager(m_postPictureBufferMgr) destroyed");
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
        m_mhbAllocator = new ExynosCameraMHBAllocator(m_cameraId);

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
        if (ret == TIMED_OUT) {
#ifdef USE_DUAL_CAMERA
        if (!(m_parameters->getDualCameraMode() == true &&
             m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR))
#endif
            CLOGW("Wait timeout");
        } else
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

    if (m_scenario == SCENARIO_SECURE || m_parameters->getVisionMode() == true)
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

    if (m_scenario == SCENARIO_SECURE || m_parameters->getVisionMode() == true)
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
        if (ret == TIMED_OUT) {
#ifdef USE_DUAL_CAMERA
        if (!(m_parameters->getDualCameraMode() == true &&
             m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR))
#endif
            CLOGW("Wait timeout");
        }
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
#ifdef USE_DUAL_CAMERA
        if (!(m_parameters->getDualCameraMode() == true &&
                    m_parameters->getFlagForceSwitchingOnly() == true))
#endif
        m_pipeFrameDoneQ->pushProcessQ(&frame);
    }

#ifdef USE_DUAL_CAMERA
    /* skip 3AA thread in sensor standby */
    if ((m_parameters->getDualCameraMode() == true &&
                m_parameters->getDualStandbyMode() ==
                ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR)) {
        CLOGW("sensor standby generate frame will be skipped");
        goto func_exit;
    }
#endif

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
    int previewH, previewW;
    int bufIndex = -1;
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
        scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);

        srcBufMgr = m_sccBufferMgr;

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
            dstBufMgr = m_fusionBufferMgr;
        else
#endif
            dstBufMgr = m_scpBufferMgr;
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

    CLOGV("do zoomPreviw with CSC : srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> \
            (%d, %d, %d, %d) format(%d)  actual(%x)",
            srcBuf.index, dstBuf.index, srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcFmt,
            V4L2_PIX_2_HAL_PIXEL_FORMAT(srcFmt), dstRect.x, dstRect.y, dstRect.w, dstRect.h,
            previewFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(previewFormat));

    ret = m_setupEntity(gscPipe, frame, &srcBuf, &dstBuf);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d) frame(%d)",
                gscPipe, ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(frame, gscPipe);

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
        dstBufMgr->cancelBuffer(dstBuf.index);

    /* msc with ndone frame, in order to frame order*/
    CLOGE(" zoom with scaler getbuffer failed , use MSC for NDONE frame(%d)",
            frame->getFrameCount());

    frame->setSrcBufferState(gscPipe, ENTITY_BUFFER_STATE_ERROR);
    m_previewFrameFactory->setOutputFrameQToPipe(pipeFrameDoneQ, gscPipe);
    m_previewFrameFactory->pushFrameToPipe(frame, gscPipe);

    CLOGV("--OUT--");

    return INVALID_OPERATION;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCamera::m_sensorStream(bool on)
{
    status_t ret = NO_ERROR;

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("Sensor start(%d)", on);

    if (on) {
        if (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR) {
            uint32_t minBayerFrameNum = 0;
            uint32_t min3AAFrameNum = 0;
            ExynosCameraFrameSP_sptr_t newFrame = NULL;

            /* streamon */
            minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;
            min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

            minBayerFrameNum = minBayerFrameNum / m_parameters->getBatchSize(PIPE_FLITE);
            min3AAFrameNum = min3AAFrameNum / m_parameters->getBatchSize(PIPE_3AA);

            uint32_t loopFrameNum = minBayerFrameNum > min3AAFrameNum ? minBayerFrameNum : min3AAFrameNum;

            CLOGD("loopFrameNum(%d), minBayerFrameNum(%d), min3AAFrameNum(%d)", loopFrameNum, minBayerFrameNum, min3AAFrameNum);

            for (uint32_t i = 0; i < loopFrameNum; i++) {
                ret = m_generateFrame(-1, newFrame);
                if (ret < 0) {
                    CLOGE("generateFrame fail");
                    return ret;
                }

                if (newFrame == NULL) {
                    CLOGE("new faame is NULL");
                    return ret;
                }

                if (m_parameters->isFlite3aaOtf() == false) {
                    ret = m_setupEntity(PIPE_FLITE, newFrame);
                    if (ret != NO_ERROR)
                        CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                                PIPE_FLITE, ret);

                    m_previewFrameFactory->pushFrameToPipe(newFrame, PIPE_FLITE);
                } else {
                    ret = m_setupEntity(PIPE_3AA, newFrame);
                    if (ret != NO_ERROR)
                        CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                                PIPE_FLITE, ret);

                    m_previewFrameFactory->pushFrameToPipe(newFrame, PIPE_3AA);
                }
            }

            /* Sensor Stream On */
            ret = m_previewFrameFactory->sensorStandby(false);
            if (ret != NO_ERROR)
                CLOGE("sensorStandby(false) failed!!, ret(%d)", ret);
        }
    } else {
        if (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_INACTIVE) {
            /* Start Thread */
            ret = m_previewFrameFactory->sensorStandby(true);
            if (ret != NO_ERROR)
                CLOGE("sensorStandby(true) failed!!, ret(%d)", ret);
        }
    }

    return ret;
}

status_t ExynosCamera::m_dualNotifyFunc(enum dual_camera_notify_type notifyType,
                                        int32_t arg1,
                                        int32_t arg2,
                                        void* arg3)
{
    Mutex::Autolock lock(m_dualNotifyLock);

    status_t ret = NO_ERROR;

    switch (notifyType) {
    case DUAL_CAMERA_NOTIFY_WAKE_UP:
        if (m_parameters->getDualStandbyMode() ==
                ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR) {
            /* streamon setting by wakeup value(true/false) */
            bool on = (arg2 > 0) ? true : false;

            /* setting for smoothTransitionCount in case of wakeup from standby */
            if (on == true && m_parameters->getDualSmoothTransitionCount() <= 0)
                m_parameters->setDualSmoothTransitionCount(DUAL_SMOOTH_TRANSITION_COUNT);

            ret = m_sensorStream(on);
            if (ret != NO_ERROR)
                CLOGE("m_sensorStream(%d, %d, %d) failed! standby mode(%d)",
                        notifyType, arg1, arg2,
                        m_parameters->getDualStandbyMode());
        } else if (m_parameters->getDualStandbyMode() ==
                ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST) {
            /* setting for smoothTransitionCount in case of wakeup from standby */
            if (m_parameters->getDualSmoothTransitionCount() <= 0)
                m_parameters->setDualSmoothTransitionCount(DUAL_SMOOTH_TRANSITION_COUNT);

            m_parameters->setDualStandbyMode(ExynosCameraParameters::DUAL_STANDBY_MODE_READY);
#ifdef DUAL_ENABLE_ASYNC_FPS
            if (DUAL_POSTSTANDBY_FPS > 0) {
                uint32_t minFps;
                uint32_t maxFps;
                m_parameters->getDualBackupPostStandbyFps(&minFps, &maxFps);

                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS, maxFps, PIPE_3AA);
                if (ret != NO_ERROR)
                    CLOGE("setControl(0x%X, %d, %d) failed! standby mode(%d)",
                            V4L2_CID_IS_MAX_TARGET_FPS, maxFps, PIPE_3AA,
                            m_parameters->getDualStandbyMode());
            }
#endif
        } else {
            CLOGW("already wakeup..(%d, %d, %d)",
                    notifyType, arg1, arg2,
                    m_parameters->getDualStandbyMode());
            m_parameters->setDualWakeupFinishCount(1);
        }
        break;
    case DUAL_CAMERA_NOTIFY_WAKE_UP_FINISH:
        CLOGI("notify wake_up_finish(switching:%d, transition:%d, smoothTransition:%d)",
                m_parameters->getDualTransitionCount(),
                m_parameters->getDualTransitionCount(),
                m_parameters->getDualSmoothTransitionCount());
        if (m_parameters->getFlagForceSwitchingOnly() == false)
            m_parameters->setDualTransitionCount(0);
        if (m_parameters->getDualSmoothTransitionCount() > 0)
            m_parameters->setDualSmoothTransitionCount(DUAL_SMOOTH_TRANSITION_COUNT);
        break;
    case DUAL_CAMERA_NOTIFY_SET_OBJECT_TRACKING_FOCUS_AREA:
        if (arg3) {
            memcpy(&m_OTpredictedData, (UniPluginFocusData_t*)arg3, sizeof(UniPluginFocusData_t));
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            ret = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
        }
        break;
    default:
        CLOGE("invalid notifyType(%d, %d, %d)", notifyType, arg1, arg2);
        break;
    }

    return ret;
}

/*
 * Prepare Function "Before" pushing sync pipe.
 *  - 1. move dst buffer to src buffer position(OUTPUT_NODE_1)
 *  - 2. push the frame to sync-pipe
 *      * slave camera's frame will be removed from processList by sync-pipe
 */
status_t ExynosCamera::m_prepareBeforeSync(int srcPipeId, int dstPipeId,
                                           ExynosCameraFrameSP_sptr_t frame,
                                           int dstPos,
                                           ExynosCameraFrameFactory *factory,
                                           frame_queue_t *outputQ,
                                           bool isReprocessing,
                                           bool forceSwitchingOnly)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;
    ExynosCameraBuffer srcBuf;
    int masterCameraId, slaveCameraId;

    if (frame == NULL) {
        CLOGE("([%d] srcPipeId:%d, dstPipeId:%d) frame is NULL",
                isReprocessing, srcPipeId, dstPipeId);
        return INVALID_OPERATION;
    }

    /* control for internal frame */
    if (frame->getFrameType() == FRAME_TYPE_INTERNAL) {
        CLOGV("[%d][INTERNAL_FRAME] srcPipeId:%d, dstPipeId:%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                frame->getFrameCount());
        goto func_internal_frame;
    }

    srcBuf.index = -1;

    /* 1. setting dst buffer to src buffer position */
    if (frame->getFrameState() == FRAME_STATE_SKIPPED ||
            frame->getFrameState() == FRAME_STATE_INVALID) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) frame->getFrameState() == (%d) so, skip fusion. frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                frame->getFrameState(),
                frame->getFrameCount());

        /* complete the frame */
        frame->setFrameState(FRAME_STATE_COMPLETE);

        ret = frame->getDstBuffer(srcPipeId, &srcBuf, dstPos);
        if (ret != NO_ERROR || srcBuf.index < 0) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) getDstBuffer fail, ret(%d) frame(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    ret, frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto func_exit;
        } else {
            /* put the own buffer to bufMgr */
            ExynosCameraBufferManager *bufMgr = (ExynosCameraBufferManager *)srcBuf.manager;
            ret = m_putBuffers(bufMgr, srcBuf.index);
            if (ret < 0)
                CLOGE("bufferMgr->putBuffers(%d) fail, ret(%d)", srcBuf.index, ret);

            ret = INVALID_OPERATION;
            goto func_internal_frame;
        }
    }

    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_cameraId == slaveCameraId && forceSwitchingOnly == false) {
        frame->setFrameState(FRAME_STATE_COMPLETE);
    }

    if (isReprocessing == false &&
            (m_parameters->getDualStandbyMode() ==
             ExynosCameraParameters::DUAL_STANDBY_MODE_INACTIVE ||
             m_parameters->getDualStandbyMode() ==
             ExynosCameraParameters::DUAL_STANDBY_MODE_READY) &&
            m_parameters->getDualWakeupFinishCount() > 0) {
        /* if adjustSync was set, forcely reset the WakeupFinishCount */
        if (frame->getAdjustSync() > 0)
            m_parameters->setDualWakeupFinishCount(DUAL_ADJUST_SYNC_COUNT);

        /* notify to other camera */
        if (m_parameters->decreaseDualWakeupFinishCount() == 0) {
            /* update finishing startPreview state */
#ifdef DUAL_SMOOTH_TRANSITION_LAUNCH
            m_parameters->finishStartPreview(m_cameraId);
#endif
            m_parameters->setDualStableFromStandby(true);
            m_parameters->dualNotify(DUAL_CAMERA_NOTIFY_WAKE_UP_FINISH, false, /* useThread */
                    m_cameraId == masterCameraId ? slaveCameraId : masterCameraId, /* targetCameraId */
                    true, NULL);
        }
    }

    ret = frame->getDstBuffer(srcPipeId, &srcBuf, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) getDstBuffer fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBufferState(ENTITY_BUFFER_STATE_NOREQ) fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = m_setSrcBuffer(dstPipeId, frame, &srcBuf);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) m_setSrcBuffer fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

func_internal_frame:
    /* set the entity state to done */
    ret = frame->setEntityState(srcPipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("frame(%d)->setEntityState(pipeId(%d), ENTITY_STATE_FRAME_DONE), ret(%d)",
                frame->getFrameCount(), srcPipeId, ret);
    }

    /* 2. push the frame to sync pipe */
    factory->setOutputFrameQToPipe(outputQ, dstPipeId);
    factory->pushFrameToPipe(frame, dstPipeId);

#ifdef USE_DUAL_CAMERA_LOG_TRACE
    CLOGI("[%d] srcPipeId:%d, dstPipeId:%d) Cam%d, frame(%d) meta(%d) timeStamp(%lld) sync(%d) type(%d) processList(%d, %d)",
            isReprocessing, srcPipeId, dstPipeId,
            frame->getCameraId(),
            frame->getFrameCount(),
            frame->getMetaFrameCount(),
            frame->getTimeStamp(),
            frame->getSyncType(),
            frame->getFrameType(),
            m_processList.size(),
            m_postProcessList.size());
#endif

    return ret;

func_exit:
    /* error handling */
    frame->setFrameState(FRAME_STATE_SKIPPED);

    /* return the buffer to bufMgr */
    ret = m_putFrameBuffer(frame, srcPipeId, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("m_putFrameBuffer(frame(%d), pipeId(%d), DST_BUFFER_DIRECTION) fail",
            frame->getFrameCount(), srcPipeId);
    }

    factory->setOutputFrameQToPipe(outputQ, dstPipeId);
    factory->pushFrameToPipe(frame, dstPipeId);

    CLOGV("--OUT--");

    return ret;
}

/*
 * Prepare Function "After" pushing sync pipe.
 *
 *  (All pushed frames are Master Camera's frame)
 *  1. set the src1 buffer to frame by dstPipeId(PIPE_FUSION)
 *  2. set the dst buffer to frame by dstPipeId(PIPE_FUSION)
 *    (captureNodeIndex is 0)
 *  3. set the src2 buffer to frame by dstPipeId(PIPE_FUSION)
 *  4. calc the src/dst rect
 *
 *  In case of "BYPASS", "SWITCH"
 *  - set FRAME_DONE for fusion-pipe and push the frame to frameDoneQ
 *    (will be memcpy from the src buffer to dst buffer(GRALLOC) in next pipe id)
 *
 *  In case of "SYNC"
 *  - push the frame to fusion-pipe
 */
status_t ExynosCamera::m_prepareAfterSync(int srcPipeId, int dstPipeId,
                                          ExynosCameraFrameSP_dptr_t frame,
                                          int dstPos,
                                          ExynosCameraFrameFactory *factory,
                                          frame_queue_t *outputQ,
                                          bool isReprocessing)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;
    int bufIndex = -2;
    ExynosCameraBuffer dstBuf;
    ExynosCameraBuffer srcBuf1;
    ExynosCameraBuffer srcBuf2;
    ExynosCameraBufferManager *dstBufMgr = NULL;
    ExynosCameraDualFrameSelector *dualSelector = NULL;
    int masterCameraId, slaveCameraId;

    int width, height;
    int32_t format = 0;
    ExynosRect srcRect1;
    ExynosRect srcRect2;
    ExynosRect dstRect;
    ExynosRect fusionSrcRect;
    struct camera2_stream *stream = NULL;
    uint32_t *output = NULL;
    struct camera2_node_group node_group;

    if (frame == NULL) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) inputFrame is NULL",
                isReprocessing, srcPipeId, dstPipeId);
        return INVALID_OPERATION;
    }

    getDualCameraId(&masterCameraId, &slaveCameraId);

    /* push the frame to remove(All buffers already were released) */
    if (m_cameraId == slaveCameraId ||
            frame->getFrameState() == FRAME_STATE_INVALID ||
            frame->getFrameState() == FRAME_STATE_SKIPPED) {
        CLOGV("[%d] srcPipeId:%d, dstPipeId:%d) frame skipped(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                frame->getFrameCount());
        frame->setFrameState(FRAME_STATE_COMPLETE);
        return NO_ERROR;
    }

    /* forcely set the MCSC0 request to true in case of internal frame */
    if (isReprocessing == false) {
        frame->setRequest(PIPE_MCSC0, true);
    } else {
        frame->setRequest(PIPE_MCSC3_REPROCESSING, true);
    }

    /* get the master, slave camera id and dualSelector to return the buffer */
    if (isReprocessing == false) {
        dualSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
    } else {
        dualSelector = ExynosCameraSingleton<ExynosCameraDualCaptureFrameSelector>::getInstance();
    }

    /* init buffer index */
    dstBuf.index = -2;
    srcBuf1.index = -2;
    srcBuf2.index = -2;

    /* 1. get/set the src1 buffer (master or slave) */
    ret = frame->getSrcBuffer(srcPipeId, &srcBuf1);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) getSrcBuffer(OUTPUT_NODE_1) fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* create master frame for slave frame */
    if (frame->getSyncType() == SYNC_TYPE_SWITCH &&
            m_cameraId == masterCameraId) {
        ExynosCameraFrameSP_sptr_t newFrame = NULL;

        if (isReprocessing == false) {
            Mutex::Autolock lock(m_frameLock);
            newFrame = m_previewFrameFactory->createNewFrame(frame);
            if (newFrame == NULL) {
                CLOGE("generateFrame from frame(%d) fail", frame->getFrameCount());
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        } else {
            ret = m_generateFrameReprocessing(newFrame, frame);
            if (ret < 0) {
                CLOGE("generateFrameReprocessing fail, ret(%d)", ret);
                ret = INVALID_OPERATION;
                goto func_exit;
            }
        }

        frame = newFrame;
        CLOGV("[%d] create new frame(F%d, S%d, T%d) for slave", isReprocessing,
                frame->getFrameCount(),
                frame->getSyncType(),
                frame->getFrameType());
    }

    ret = frame->setSrcBuffer(dstPipeId, srcBuf1);
    if (ret < 0) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setSrcBuffer(OUTPUT_NODE_1) fail, index(%d) frame(%d) ret(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                srcBuf1.index, frame->getFrameCount(), ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setSrcBufferState1(ENTITY_BUFFER_STATE_REQUESTED) fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* 2. get/set the dst buffer(GRALLOC or FUSION) */
    if (isReprocessing == false ) {
        dstBufMgr = m_scpBufferMgr;
    } else {
        dstBufMgr = m_fusionReprocessingBufferMgr;
    }

    /* get the dst buffer in case of "not reprocessing" or "reprocessing and sync mode" */
    if (isReprocessing == false ||
            (isReprocessing == true && frame->getSyncType() == SYNC_TYPE_SYNC)) {
        ret = dstBufMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
        if (ret != NO_ERROR || bufIndex < 0) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) Buffer manager getBuffer fail, frame(%d), ret(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    frame->getFrameCount(), ret);
            goto func_exit;
        }

        ret = frame->setDstBuffer(dstPipeId, dstBuf);
        if (ret < 0) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBuffer fail, index(%d) frame(%d) ret(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    dstBuf.index, frame->getFrameCount(), ret);
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        ret = frame->setDstBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED);
        if (ret != NO_ERROR) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBufferState(ENTITY_BUFFER_STATE_REQUESTED) fail, ret(%d) frame(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    ret, frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    /* 3. get/set the src2 buffer (slave) */
    if (frame->getSyncType() == SYNC_TYPE_SYNC) {
        ret = frame->getSrcBuffer(srcPipeId, &srcBuf2, OUTPUT_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) getDstBuffer(OUTPUT_NODE_2) fail, ret(%d) frame(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    ret, frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        ret = frame->setSrcBuffer(dstPipeId, srcBuf2, OUTPUT_NODE_2);
        if (ret < 0) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setSrcBuffer(OUTPUT_NODE_2) fail, index(%d) frame(%d) ret(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    srcBuf2.index, frame->getFrameCount(), ret);
            ret = INVALID_OPERATION;
            goto func_exit;
        }

        ret = frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED, OUTPUT_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBufferState2(ENTITY_BUFFER_STATE_REQUESTED) fail, ret(%d) frame(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    ret, frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto func_exit;
        }
    }

    /* 4. calc src/dst rect */
    if (isReprocessing == false) {
        format = m_parameters->getHwPreviewFormat();
        m_parameters->getPreviewSize(&width, &height);
    } else {
        format = m_parameters->getHwPictureFormat();
        m_parameters->getPictureSize(&width, &height);
    }

    /* src1 */
    stream = (struct camera2_stream *)srcBuf1.addr[srcBuf1.getMetaPlaneIndex()];
    if (stream == NULL) {
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) stream1 is NULL, skip fuision. ret(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                ret, frame->getFrameCount());
        goto func_exit;
    }

    output = stream->output_crop_region;
    srcRect1.x = 0;
    srcRect1.y = 0;
    srcRect1.w = output[2];
    srcRect1.h = output[3];
    srcRect1.fullW = output[2];
    srcRect1.fullH = output[3];
    srcRect1.colorFormat = format;
    frame->setSrcRect(dstPipeId, srcRect1);

#ifdef USE_MSC_CAPTURE
    if (srcPipeId == PIPE_SYNC_REPROCESSING) {
        m_MscCaptureQ->pushProcessQ(&frame);
        m_MscCaptureThread->join();
    }
#endif

    /* src2 */
    if (frame->getSyncType() == SYNC_TYPE_SYNC) {
        stream = (struct camera2_stream *)srcBuf2.addr[srcBuf2.getMetaPlaneIndex()];
        if (stream == NULL) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) stream2 is NULL, skip fuision. ret(%d) frame(%d)",
                    isReprocessing, srcPipeId, dstPipeId,
                    ret, frame->getFrameCount());
            goto func_exit;
        }

        output = stream->output_crop_region;
        srcRect2.x = 0;
        srcRect2.y = 0;
        srcRect2.w = output[2];
        srcRect2.h = output[3];
        srcRect2.fullW = output[2];
        srcRect2.fullH = output[3];
        srcRect2.colorFormat = format;
        frame->setSrcRect(dstPipeId, srcRect2, OUTPUT_NODE_2);
    }

    /* dst */
    if (isReprocessing == false) {
        if (m_parameters->getFusionSize(width, height, &fusionSrcRect, &dstRect) != NO_ERROR) {
            CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) getFusionSize(%d, %d) frame(%d) fail",
                    isReprocessing, srcPipeId, dstPipeId,
                    width, height, frame->getFrameCount());
            goto func_exit;
        }

        frame->setDstRect(dstPipeId, dstRect);
    } else {
        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = width;
        dstRect.h = height;
        dstRect.fullW = width;
        dstRect.fullH = height;
        dstRect.colorFormat = format;
        frame->setDstRect(dstPipeId, dstRect);
    }

#ifdef USE_DUAL_CAMERA_LOG_TRACE
    CLOGI("[%d] srcPipeId:%d, dstPipeId:%d) frame([%d,%d,%d]-F%d-M%d),timeStamp(%lld)-processList(%d)"
            "Src1[%d,%d,%d,%d,%d,%d,%x] " \
            "Src2[%d,%d,%d,%d,%d,%d,%x] " \
            "Dst[%d,%d,%d,%d,%d,%d,%x]",
            isReprocessing, srcPipeId, dstPipeId,
            frame->getSyncType(),
            frame->getFrameType(),
            frame->getFrameState(),
            frame->getFrameCount(),
            frame->getMetaFrameCount(),
            frame->getTimeStamp(),
            m_processList.size(),
            srcBuf1.index, srcRect1.x, srcRect1.y, srcRect1.w, srcRect1.h,
            srcRect1.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect1.colorFormat),
            srcBuf2.index, srcRect2.x, srcRect2.y, srcRect2.w, srcRect2.h,
            srcRect2.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect2.colorFormat),
            dstBuf.index, dstRect.x, dstRect.y, dstRect.w, dstRect.h,
            dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));
#endif

    switch (frame->getSyncType()) {
    case SYNC_TYPE_BYPASS:
    case SYNC_TYPE_SWITCH:
        if (isReprocessing == false) {
            /* push the frame to fusion pipe to copy the data to gralloc */
            factory->setOutputFrameQToPipe(outputQ, dstPipeId);
            factory->pushFrameToPipe(frame, dstPipeId);
        } else {
            /* in case of reprocessing, dont' push the frame */
            /* move the source buffer position to dst */
            ret = frame->setDstBuffer(dstPipeId, srcBuf1);
            if (ret < 0) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBuffer fail, index(%d) frame(%d) ret(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        dstBuf.index, frame->getFrameCount(), ret);
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            ret = frame->setDstBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBufferState(ENTITY_BUFFER_STATE_REQUESTED) fail, ret(%d) frame(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        ret, frame->getFrameCount());
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            ret = frame->setDstBuffer(dstPipeId, srcBuf1, dstPos);
            if (ret < 0) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBuffer fail, index(%d) frame(%d) ret(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        dstBuf.index, frame->getFrameCount(), ret);
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            ret = frame->setDstBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setDstBufferState(ENTITY_BUFFER_STATE_REQUESTED) fail, ret(%d) frame(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        ret, frame->getFrameCount());
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            /* set the entity state to done */
            ret = frame->setEntityState(dstPipeId, ENTITY_STATE_FRAME_DONE);
            if (ret < 0) {
                CLOGE("frame(%d)->setEntityState(pipeId(%d), ENTITY_STATE_FRAME_DONE), ret(%d)",
                        frame->getFrameCount(), dstPipeId, ret);
                ret = INVALID_OPERATION;
                goto func_exit;
            }

            /* push the frame */
            outputQ->pushProcessQ(&frame);
        }
        break;
    case SYNC_TYPE_SYNC:
        /* push the frame to fusion pipe */
        factory->setOutputFrameQToPipe(outputQ, dstPipeId);
        factory->pushFrameToPipe(frame, dstPipeId);

        /* if it's reprocessing instance, start the fusion pipe thread */
        if (isReprocessing == true) {
            ret = m_reprocessingFrameFactory->startThread(dstPipeId);
            if (ret != NO_ERROR)
                CLOGE("startThread(pipe:%d) fail, ret(%d)", dstPipeId, ret);
        }
        break;
    default:
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) Invalid SyncType(%d) frame(%d)",
                isReprocessing, srcPipeId, dstPipeId,
                frame->getSyncType(), frame->getFrameCount());
        goto func_exit;
        break;
    }

    CLOGV("[%d] srcPipeId:%d, dstPipeId:%d) --OUT--",
            isReprocessing, srcPipeId, dstPipeId);

    return ret;

func_exit:
    /* error handling */
    CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) release sync frame(%d) timeStamp(%lld) processList(%d)",
            isReprocessing, srcPipeId, dstPipeId,
            frame->getFrameCount(),
            frame->getMetaFrameCount(),
            frame->getTimeStamp(),
            m_processList.size());

    if (isReprocessing == false) {
        /* return gralloc buffer */
        if (bufIndex >= 0) {
            /* return the buffer to bufMgr */
            ret = dstBufMgr->cancelBuffer(dstBuf.index);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) cancelBuffer(%d) frame(%d) fail",
                        isReprocessing, srcPipeId, dstPipeId,
                        dstBuf.index, frame->getFrameCount());
            }
        }
    } else {
        /* return fusion reprocessing buffer */
        if (bufIndex >= 0) {
            ret = m_putBuffers(m_fusionReprocessingBufferMgr, dstBuf.index);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) putBuffer(%d) frame(%d) fail",
                        isReprocessing, srcPipeId, dstPipeId,
                        dstBuf.index, frame->getFrameCount());
            }
        }
    }

    /* return source buffers */
    switch (frame->getSyncType()) {
    case SYNC_TYPE_BYPASS:
        if (srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(masterCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        masterCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
    case SYNC_TYPE_SWITCH:
        if (srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(slaveCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        slaveCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
    case SYNC_TYPE_SYNC:
        if (srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(masterCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        masterCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }

        if (srcBuf2.index >= 0) {
            ret = dualSelector->releaseBuffer(slaveCameraId, &srcBuf2);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId, dstPipeId,
                        slaveCameraId, srcBuf2.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
    default:
        break;
    }

    /* push the frame to remove */
    ret = frame->setEntityState(dstPipeId, ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR)
        CLOGE("[%d] srcPipeId:%d, dstPipeId:%d) setEntityState(ENTITY_STATE_FRAME_DONE) fail",
                isReprocessing, srcPipeId, dstPipeId);

    frame->setFrameState(FRAME_STATE_COMPLETE);

    CLOGV("[%d] srcPipeId:%d, dstPipeId:%d) --OUT--",
            isReprocessing, srcPipeId, dstPipeId);

    return ret;
}

/*
 * Prepare Function "After" pushing fusion pipe.
 *
 *  1. release source buffers (from sync pipe)
 *  2. move the position of dst buffer
 */
status_t ExynosCamera::m_prepareAfterFusion(int srcPipeId,
                                          ExynosCameraFrameSP_sptr_t frame,
                                          int dstPos, bool isReprocessing)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;
    int bufIndex = -2;
    ExynosCameraBuffer srcBuf1;
    ExynosCameraBuffer srcBuf2;
    ExynosCameraBuffer dstBuf;
    ExynosCameraDualFrameSelector *dualSelector = NULL;
    int masterCameraId, slaveCameraId;
    entity_buffer_state_t bufferState;

    if (frame == NULL) {
        CLOGE("[%d] srcPipeId:%d) inputFrame is NULL",
                isReprocessing, srcPipeId);
        return INVALID_OPERATION;
    }

#ifdef USE_DUAL_CAMERA_LOG_TRACE
    CLOGI("[%d] srcPipeId:%d, dstPos:%d) frame(%d/%d) syncType(%d) timeStamp(%lld) processList(%d)",
            isReprocessing, srcPipeId,
            dstPos,
            frame->getFrameCount(),
            frame->getMetaFrameCount(),
            frame->getSyncType(),
            frame->getTimeStamp(),
            m_processList.size());
#endif

    /*
     * 1. release source buffers (from sync pipe)
     */

    /* get the master, slave camera id and dualSelector to return the buffer */
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (isReprocessing == false) {
        dualSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
    } else {
        dualSelector = ExynosCameraSingleton<ExynosCameraDualCaptureFrameSelector>::getInstance();
    }

    /* init buffer index */
    srcBuf1.index = -2;
    srcBuf2.index = -2;

    /* get the src1 buffer (master or slave) */
    ret = frame->getSrcBuffer(srcPipeId, &srcBuf1);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d) getSrcBuffer fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, ret, frame->getFrameCount());
    }

    /* get/set the src2 buffer (slave) */
    if (frame->getSyncType() == SYNC_TYPE_SYNC) {
        ret = frame->getSrcBuffer(srcPipeId, &srcBuf2, OUTPUT_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("[%d] srcPipeId:%d) getDstBuffer fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, ret, frame->getFrameCount());
        }
    }

    /* return source buffers */
    switch (frame->getSyncType()) {
    case SYNC_TYPE_BYPASS:
        /* in case of reprocessing, don't release the buffer */
        if (isReprocessing == false && srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(masterCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId,
                        masterCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
        break;
    case SYNC_TYPE_SWITCH:
        /* in case of reprocessing, don't release the buffer */
        if (isReprocessing == false && srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(slaveCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId,
                        slaveCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
        break;
    case SYNC_TYPE_SYNC:
        if (srcBuf1.index >= 0) {
            ret = dualSelector->releaseBuffer(masterCameraId, &srcBuf1);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId,
                        masterCameraId, srcBuf1.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }

        if (srcBuf2.index >= 0) {
            ret = dualSelector->releaseBuffer(slaveCameraId, &srcBuf2);
            if (ret != NO_ERROR) {
                CLOGE("[%d] srcPipeId:%d) dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        isReprocessing, srcPipeId,
                        slaveCameraId, srcBuf2.index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
        break;
    default:
        CLOGE("[%d] srcPipeId:%d) Invalid SyncType(%d) frame(%d)",
                isReprocessing, srcPipeId,
                frame->getSyncType(), frame->getFrameCount());
        break;
    }

    /*
     * 2. move the position of dst buffer
     */
    ret = frame->getDstBuffer(srcPipeId, &dstBuf);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d) getDstBuffer fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId, ret, frame->getFrameCount());
    }

    ret = frame->setDstBuffer(srcPipeId, dstBuf, dstPos);
    if (ret < 0) {
        CLOGE("[%d] srcPipeId:%d) setDstBuffer fail, index(%d) frame(%d) ret(%d)",
                isReprocessing, srcPipeId,
                dstBuf.index, frame->getFrameCount(), ret);
    }

    ret = frame->setDstBufferState(srcPipeId, ENTITY_BUFFER_STATE_COMPLETE, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[%d] srcPipeId:%d) setDstBufferState(ENTITY_BUFFER_STATE_COMPLETE) fail, ret(%d) frame(%d)",
                isReprocessing, srcPipeId,
                ret, frame->getFrameCount());
    }

    CLOGV("[%d] srcPipeId:%d) --OUT--", isReprocessing, srcPipeId);

    return ret;
}
#endif

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

#ifdef CAMERA_FAST_ENTRANCE_V1
bool ExynosCamera::m_fastenAeThreadFunc(void)
{

#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_THREAD_START, true);
    }
#endif
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    m_fastenAeThreadResult = 0;

    /* update frameMgr parameter */
    m_setFrameManagerInfo();

    /* frame manager start */
    m_frameMgr->start();

    /*
     * This is for updating parameter value at once.
     * This must be just before making factory
     */
    m_parameters->updateParameters();

    if (m_previewFrameFactory->isCreated() == false) {
#if defined(SAMSUNG_EEPROM)
        if (m_use_companion == false
            && isEEprom(getCameraIdInternal()) == true) {
            if (m_eepromThread != NULL) {
                CLOGD("eepromThread join.....");
                m_eepromThread->join();
            } else {
                CLOGD(" eepromThread is NULL.");
            }
            m_parameters->setRomReadThreadDone(true);
        }
#endif /* SAMSUNG_EEPROM */

#ifdef SAMSUNG_COMPANION
        if (m_use_companion == true) {
            /*
             * for critical section from open ~ V4L2_CID_IS_END_OF_STREAM.
             * This lock is available on local variable scope only from { to }.
             */
            Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());

            /*
             * companion thread(front eeprom) and fastAE thread(front AF) use same i2c port.
             * so, companion thread should be finished before fastAE thread in DICO system.
             */
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
            if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
            {
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_THREAD_START, false);
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_COMPANION_WAIT, true);
            }
#endif
            m_waitCompanionThreadEnd();
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
            if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
            {
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_COMPANION_WAIT, false);
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_PRE_POST_CREATE_START, true);
            }
#endif
            ret = m_previewFrameFactory->precreate();
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->precreate() failed");
                goto err;
            }
#ifdef SAMSUNG_COMPANION
            m_parameters->setRomReadThreadDone(true);
#endif
            ret = m_previewFrameFactory->postcreate();
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->postcreate() failed");
                goto err;
            }
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
            if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
            {
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_PRE_POST_CREATE_START, false);
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_PRE_POST_CREATE_END, true);
            }
#endif
        } else {
            ret = m_previewFrameFactory->create();
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->create() failed");
                goto err;
            }
        }
#else
        ret = m_previewFrameFactory->create();
        if (ret != NO_ERROR) {
            CLOGE("m_previewFrameFactory->create() failed");
            goto err;
        }
#endif /* SAMSUNG_COMPANION */
        CLOGD("FrameFactory(previewFrameFactory) created");
    }

#ifdef USE_QOS_SETTING
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
    if (ret != NO_ERROR)
        CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);

    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER0, LITTLE_CORE_MIN_LOCK, PIPE_3AA);
    if (ret != NO_ERROR)
        CLOGE("V4L2_CID_IS_DVFS_CLUSTER0 setControl fail, ret(%d)", ret);
#endif


#ifdef CAMERA_GED_FEATURE
    if (isFastenAeStable(m_cameraId, false)== true
#else
    if (m_parameters->getUseFastenAeStable() == true
#endif
#ifdef USE_DUAL_CAMERA
        && m_parameters->isFastenAeStableEnable()
#else
        && m_parameters->getDualMode() == false
#endif
        && m_isFirstStart == true) {

        ret = m_fastenAeStable();
        if (ret != NO_ERROR) {
            CLOGE("m_fastenAeStable() failed");
            goto err;
        } else {
            skipFrameCount = 0;
            m_parameters->setUseFastenAeStable(false);
        }
    }

#ifdef USE_FADE_IN_ENTRANCE
    if (m_parameters->getUseFastenAeStable() == true) {
        CLOGE("consistency in skipFrameCount might be broken by FASTEN_AE and FADE_IN");
    }
#endif

    m_parameters->setFrameSkipCount(skipFrameCount);
    m_fdFrameSkipCount = 0;

#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_FASTAE_THREAD_END, false);
    }
#endif

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
            CLOGD(" eepromThread is NULL.");
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
            CLOGI("m_fastenAeThread is NULL");
        }
    }

    timer.stop();

    CLOGD("fastenAeThread joined, waiting time : duration time(%5d msec)", (int)timer.durationMsecs());

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

    uint64_t seriesShotDuration = m_parameters->getSeriesShotDuration();

    m_burstPrePictureTimer.start();

    if (m_parameters->isReprocessing())
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

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[sccReprocessingBuffer.getMetaPlaneIndex()]);
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
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);

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
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
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
        ret = m_putBuffers(m_sccReprocessingBufferMgr, sccReprocessingBuffer.index);
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

    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraBuffer reprocessingDstBuffer;

    int cbPreviewW = 0, cbPreviewH = 0;
    int previewFormat = 0;
    ExynosRect srcRect, dstRect;
    m_parameters->getPreviewSize(&cbPreviewW, &cbPreviewH);
    previewFormat = m_parameters->getPreviewFormat();

    int pipeReprocessingSrc = 0;
    int pipeReprocessingDst = 0;

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    ExynosCameraBufferManager *bufferMgr = NULL;

    reprocessingDstBuffer.index = -2;

    pipeReprocessingSrc = m_parameters->isReprocessing3aaIspOTF() == true ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
    pipeReprocessingDst = PIPE_MCSC1_REPROCESSING;

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

#ifdef USE_REMOSAIC_CAPTURE
bool ExynosCamera::m_sensorModeSwitchThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    uint32_t prepare3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

    ret = m_previewFrameFactory->switchSensorMode();

    m_mainSetupQThread[INDEX(PIPE_3AA)]->stop();
    m_mainSetupQ[INDEX(PIPE_3AA)]->sendCmd(WAKE_UP);
    m_mainSetupQThread[INDEX(PIPE_3AA)]->requestExitAndWait();
    m_mainSetupQ[INDEX(PIPE_3AA)]->release();

    ret = m_previewFrameFactory->finishSwitchSensorMode();

    /* Remove the frame which did NOT finished 3AA in processList */
    {
        Mutex::Autolock aLock(m_processListLock);
        List<ExynosCameraFrameSP_sptr_t>::iterator r;
        ExynosCameraFrameEntity *entity = NULL;
        ExynosCameraBuffer buffer;

        while (m_processList.empty() == false) {
            r = m_processList.begin()++;
            newFrame = *r;
            if (newFrame == NULL) {
                m_processList.erase(r);
                continue;
            }

            entity = newFrame->getFrameDoneFirstEntity();
            if (entity == NULL || entity->getPipeId() < PIPE_3AA) {
                CLOGD("[F%d]Delete frame. FrameDoneEntityAddr %p",
                        newFrame->getFrameCount(),
                        entity);
            }

            m_processList.erase(r);
        }
    }

    ret = m_captureSelector->clearList(m_getBayerPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_VC0));
    ret = m_captureSelector->clearList(m_getBayerPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_3AC));

    if (ret != NO_ERROR) {
        CLOGE("Failed to clear captureSelector list. ret %d", ret);
        return false;
    }

    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }

    if (m_ispBufferMgr != NULL) {
        m_ispBufferMgr->resetBuffers();
    }

    if (m_parameters->getSwitchSensor() == true) {
        m_previewFrameFactory->setRequest(PIPE_VC0, true);
        m_previewFrameFactory->setRequest(PIPE_3AC, false);
    } else {
        m_previewFrameFactory->setRequest(PIPE_VC0, false);
        m_previewFrameFactory->setRequest(PIPE_3AC, true);
    }

    /* Prepare initial frames */
    for (uint32_t frameCount = 0; frameCount < prepare3AAFrameNum; frameCount++) {
        ret = m_generateFrame(frameCount, newFrame);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to generateFrame", frameCount);
            /* Continue to generate initial frames */
            continue;
        } else if (newFrame == NULL) {
            CLOGE("[F%d]newFrame is NULL", frameCount);
            /* Continue to generate initial frames */
            continue;
        }

        if (m_parameters->isFlite3aaOtf() == true) {
            ret = m_setupEntity(PIPE_3AA, newFrame);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to setupEntity. PIPE_3AA, ret %d", newFrame->getFrameCount(), ret);
            }

            m_previewFrameFactory->pushFrameToPipe(newFrame, PIPE_3AA);
        }
    }

    ret = m_previewFrameFactory->preparePipes();
    if (ret != NO_ERROR) {
        CLOGE("Failed to preparePipes. ret %d", ret);

        return false;
    }

    ret = m_previewFrameFactory->startSensor3AAPipe();
    //ret = m_previewFrameFactory->startPipes();
    if (ret != NO_ERROR) {
        CLOGE("Failed to startSensor3AAPipe. ret %d", ret);

        m_previewFrameFactory->stopPipes();
        return false;
    }

    ret = m_previewFrameFactory->startInitialThreads();
    if (ret != NO_ERROR) {
        CLOGE("Failed to startInitialThreads. ret %d", ret);

        return false;
    }

    m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);

    return false;
}
#endif //USE_REMOSAIC_CAPTURE

bool ExynosCamera::m_facedetectThreadFunc(void)
{
    int32_t status = 0;
    bool loop = true;

    int count = 0;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
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
#ifdef USE_DUAL_CAMERA
                if (!(m_parameters->getDualCameraMode() == true &&
                            m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR))
#endif
                CLOGW("m_facedetectQ time out, status(%d)", status);
            }
        } else {
            /* TODO: doing exception handling */
            CLOGE("wait and pop fail, status(%d)", status);
        }
        goto func_exit;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getDepthCallbackOnPreview()
#ifdef SAMSUNG_FOCUS_PEAKING
        && m_parameters->m_getFocusPeakingMode() == false
#endif
    ) {
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
            if (newFrame->getRequest(pipeId) == true
#ifdef SAMSUNG_FOCUS_PEAKING
                && m_parameters->m_getFocusPeakingMode() == false
#endif
            ) {
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
                if (newFrame->getRequest(pipeId) == true
#ifdef SAMSUNG_FOCUS_PEAKING
                    && m_parameters->m_getFocusPeakingMode() == false
#endif
                ) {
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
            if (newFrame->getRequest(pipeId) == true
#ifdef SAMSUNG_FOCUS_PEAKING
                && m_parameters->m_getFocusPeakingMode() == false
#endif
            ) {
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

    m_previewCallbackThread = new mainCameraThread(this, &ExynosCamera::m_previewCallbackThreadFunc, "previewCallbackThread", PRIORITY_DISPLAY);
    CLOGV("previewCallbackThread created");

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

    m_zoomPreviwWithCscThread = new mainCameraThread(this, &ExynosCamera::m_zoomPreviwWithCscThreadFunc, "zoomPreviwWithCscQThread");
    CLOGV("zoomPreviwWithCscQThread created");

    /* saveThread */
    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        snprintf(m_sjpegThreadName[threadNum], sizeof(m_sjpegThreadName[threadNum]), "jpegSaveThread%d", threadNum);
        m_jpegSaveThread[threadNum] = new mainCameraThread(this, &ExynosCamera::m_jpegSaveThreadFunc, m_sjpegThreadName[threadNum]);
        CLOGV("%s created", m_sjpegThreadName[threadNum]);
    }

    /* high resolution preview callback Thread */
    m_highResolutionCallbackThread = new mainCameraThread(this, &ExynosCamera::m_highResolutionCallbackThreadFunc, "m_highResolutionCallbackThread");
    CLOGV("highResolutionCallbackThread created");


    /* Shutter callback */
    m_shutterCallbackThread = new mainCameraThread(this, &ExynosCamera::m_shutterCallbackThreadFunc, "shutterCallbackThread");
    CLOGV("shutterCallbackThread created");

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastenAeThread = new mainCameraThread(this, &ExynosCamera::m_fastenAeThreadFunc, "fastenAeThread", PRIORITY_URGENT_DISPLAY);
    CLOGV("m_fastenAeThread created");
#endif

#ifdef USE_REMOSAIC_CAPTURE
    m_sensorModeSwitchThread = new mainCameraThread(this, &ExynosCamera::m_sensorModeSwitchThreadFunc, "sensorModeSwitchThread", PRIORITY_URGENT_DISPLAY);
    CLOGV("m_sensorModeSwitchThread created");
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
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_RECORDING_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_PREVIEW_BUFFERS_MARGIN;
    exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hiding_mode_buffers = NUM_HIDING_BUFFER_COUNT;

    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER] = (getCameraIdInternal() == CAMERA_ID_BACK) ? RESERVED_NUM_BAYER_BUFFERS : FRONT_RESERVED_NUM_BAYER_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP] = (getCameraIdInternal() == CAMERA_ID_BACK) ? RESERVED_NUM_ISP_BUFFERS : FRONT_RESERVED_NUM_ISP_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD] = (getCameraIdInternal() == CAMERA_ID_BACK) ? RESERVED_NUM_ISP_BUFFERS_ON_UHD : FRONT_RESERVED_NUM_ISP_BUFFERS_ON_UHD;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG] = (getCameraIdInternal() == CAMERA_ID_BACK) ? RESERVED_NUM_JPEG_BUFFERS : FRONT_RESERVED_NUM_JPEG_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD] = (getCameraIdInternal() == CAMERA_ID_BACK) ? RESERVED_NUM_JPEG_BUFFERS_ON_UHD : FRONT_RESERVED_NUM_JPEG_BUFFERS_ON_UHD;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC] = RESERVED_NUM_POST_PIC_BUFFERS;
    exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE] = RESERVED_NUM_SECURE_BUFFERS;

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
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
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
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_120].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
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
#ifdef USE_VRA_GROUP
    exynosConfig.info[CONFIG_MODE::HIGHSPEED_240].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
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

#ifdef USE_DUAL_CAMERA
    /* for slave camera(single) */
    if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = NUM_BACK_1_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = NUM_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_BACK_1_3AA_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_BACK_1_HW_DIS_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_BACK_1_PREVIEW_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = NUM_BACK_1_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_BACK_1_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_BACK_1_RECORDING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_BACK_1_PREVIEW_BUFFERS_MARGIN;
        exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hiding_mode_buffers = NUM_HIDING_BUFFER_COUNT;

        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER] = RESERVED_NUM_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP] = RESERVED_NUM_BACK_1_ISP_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD] = RESERVED_NUM_BACK_1_ISP_BUFFERS_ON_UHD;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG] = RESERVED_NUM_BACK_1_JPEG_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD] = RESERVED_NUM_BACK_1_JPEG_BUFFERS_ON_UHD;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC] = RESERVED_NUM_BACK_1_POST_PIC_BUFFERS;
        exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE] = RESERVED_NUM_BACK_1_SECURE_BUFFERS;

        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = PIPE_SCP_BACK_1_PREPARE_COUNT;

        /* reprocessing */
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
        /* Config HIGH_SPEED 60 buffer & pipe info */
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers =  FPS60_NUM_NUM_BACK_1_SENSOR_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers =  FPS60_NUM_NUM_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_INIT_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_BACK_1_BAYER_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_BACK_1_HW_DIS_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_BACK_1_PREVIEW_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers =  FPS60_NUM_BACK_1_PICTURE_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_BACK_1_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_BACK_1_RECORDING_BUFFERS;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_BACK_1_SKIP_FRAME;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_BACK_1_REPROCESSING_BAYER_HOLD_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_BACK_1_PREVIEW_BUFFERS_MARGIN;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hiding_mode_buffers = FPS60_NUM_BACK_1_HIDING_BUFFER_COUNT;

        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_BACK_1_PREPARE_COUNT;
        exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_BACK_1_REPROCESSING_PREPARE_COUNT;
#endif
    }

    if (m_flagDualCameraOpened == true) {
        /* for master camera(dual) */
        if (getCameraIdInternal() == CAMERA_ID_BACK_0) {
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = NUM_DUAL_BACK_0_SENSOR_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = NUM_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_DUAL_BACK_0_3AA_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_DUAL_BACK_0_HW_DIS_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_DUAL_BACK_0_PREVIEW_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = NUM_DUAL_BACK_0_PICTURE_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_DUAL_BACK_0_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_DUAL_BACK_0_RECORDING_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_DUAL_BACK_0_PREVIEW_BUFFERS_MARGIN;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hiding_mode_buffers = NUM_HIDING_BUFFER_COUNT;

            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER] = RESERVED_NUM_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP] = RESERVED_NUM_DUAL_BACK_0_ISP_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD] = RESERVED_NUM_DUAL_BACK_0_ISP_BUFFERS_ON_UHD;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG] = RESERVED_NUM_DUAL_BACK_0_JPEG_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD] = RESERVED_NUM_DUAL_BACK_0_JPEG_BUFFERS_ON_UHD;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC] = RESERVED_NUM_DUAL_BACK_0_POST_PIC_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE] = RESERVED_NUM_DUAL_BACK_0_SECURE_BUFFERS;

            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = PIPE_SCP_DUAL_BACK_0_PREPARE_COUNT;

            /* reprocessing */
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
            /* Config HIGH_SPEED 60 buffer & pipe info */
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers =  FPS60_NUM_NUM_DUAL_BACK_0_SENSOR_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers =  FPS60_NUM_NUM_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_INIT_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_DUAL_BACK_0_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_DUAL_BACK_0_HW_DIS_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_DUAL_BACK_0_PREVIEW_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers =  FPS60_NUM_DUAL_BACK_0_PICTURE_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_DUAL_BACK_0_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_DUAL_BACK_0_RECORDING_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_DUAL_BACK_0_SKIP_FRAME;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_DUAL_BACK_0_REPROCESSING_BAYER_HOLD_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_DUAL_BACK_0_PREVIEW_BUFFERS_MARGIN;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hiding_mode_buffers = FPS60_NUM_DUAL_BACK_0_HIDING_BUFFER_COUNT;

            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_DUAL_BACK_0_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_DUAL_BACK_0_REPROCESSING_PREPARE_COUNT;
#endif
        }

        /* for master camera(dual) */
        if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_sensor_buffers = NUM_DUAL_BACK_1_SENSOR_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_bayer_buffers = NUM_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.init_bayer_buffers = INIT_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_3aa_buffers = NUM_DUAL_BACK_1_3AA_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hwdis_buffers = NUM_DUAL_BACK_1_HW_DIS_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_preview_buffers = NUM_DUAL_BACK_1_PREVIEW_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_picture_buffers = NUM_DUAL_BACK_1_PICTURE_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_reprocessing_buffers = NUM_DUAL_BACK_1_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_recording_buffers = NUM_DUAL_BACK_1_RECORDING_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_fastaestable_buffer = INITIAL_SKIP_FRAME;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.reprocessing_bayer_hold_count = REPROCESSING_BAYER_HOLD_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.preview_buffer_margin = NUM_DUAL_BACK_1_PREVIEW_BUFFERS_MARGIN;
            exynosConfig.info[CONFIG_MODE::NORMAL].bufInfo.num_hiding_mode_buffers = NUM_HIDING_BUFFER_COUNT;

            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER] = RESERVED_NUM_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP] = RESERVED_NUM_DUAL_BACK_1_ISP_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD] = RESERVED_NUM_DUAL_BACK_1_ISP_BUFFERS_ON_UHD;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG] = RESERVED_NUM_DUAL_BACK_1_JPEG_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD] = RESERVED_NUM_DUAL_BACK_1_JPEG_BUFFERS_ON_UHD;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC] = RESERVED_NUM_DUAL_BACK_1_POST_PIC_BUFFERS;
            exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE] = RESERVED_NUM_DUAL_BACK_1_SECURE_BUFFERS;

            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_FLITE] = PIPE_FLITE_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AC] = PIPE_3AC_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA] = PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_3AA_ISP] = PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISP] = PIPE_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC] = PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC] = PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP] = PIPE_SCP_DUAL_BACK_1_PREPARE_COUNT;

            /* reprocessing */
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = PIPE_SCP_REPROCESSING_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_ISPC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::NORMAL].pipeInfo.prepare[PIPE_SCC_REPROCESSING] = PIPE_SCC_REPROCESSING_PREPARE_COUNT;

#if (USE_HIGHSPEED_RECORDING)
            /* Config HIGH_SPEED 60 buffer & pipe info */
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_sensor_buffers =  FPS60_NUM_NUM_DUAL_BACK_1_SENSOR_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_bayer_buffers =  FPS60_NUM_NUM_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.init_bayer_buffers = FPS60_INIT_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_3aa_buffers = FPS60_NUM_NUM_DUAL_BACK_1_BAYER_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hwdis_buffers = FPS60_NUM_DUAL_BACK_1_HW_DIS_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_preview_buffers = FPS60_NUM_DUAL_BACK_1_PREVIEW_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_picture_buffers =  FPS60_NUM_DUAL_BACK_1_PICTURE_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_reprocessing_buffers = FPS60_NUM_DUAL_BACK_1_REPROCESSING_BUFFERS;
#ifdef USE_VRA_GROUP
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_vra_buffers = NUM_VRA_BUFFERS;
#endif
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_recording_buffers = FPS60_NUM_DUAL_BACK_1_RECORDING_BUFFERS;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_fastaestable_buffer = FPS60_INITIAL_DUAL_BACK_1_SKIP_FRAME;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.reprocessing_bayer_hold_count = FPS60_DUAL_BACK_1_REPROCESSING_BAYER_HOLD_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.preview_buffer_margin = FPS60_NUM_DUAL_BACK_1_PREVIEW_BUFFERS_MARGIN;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].bufInfo.num_hiding_mode_buffers = FPS60_NUM_DUAL_BACK_1_HIDING_BUFFER_COUNT;

            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_FLITE] = FPS60_PIPE_FLITE_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AC] = FPS60_PIPE_3AC_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA] = FPS60_PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_3AA_ISP] = FPS60_PIPE_3AA_ISP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP] = FPS60_PIPE_SCP_DUAL_BACK_1_PREPARE_COUNT;
            exynosConfig.info[CONFIG_MODE::HIGHSPEED_60].pipeInfo.prepare[PIPE_SCP_REPROCESSING] = FPS60_PIPE_SCP_DUAL_BACK_1_REPROCESSING_PREPARE_COUNT;
#endif
        }
    }
#endif

    for(int i = CONFIG_MODE::NORMAL+1 ; i < CONFIG_MODE::MAX ; i++) {
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC];
        exynosConfig.info[i].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE] = exynosConfig.info[CONFIG_MODE::NORMAL].reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE];
    }

    m_parameters->setConfig(&exynosConfig);

    m_exynosconfig = m_parameters->getConfig();

    return NO_ERROR;
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

    for (int i = 0; i < REPROCESSING_FRAME_FACTORY_MAX; i++) {
        m_pReprocessingFrameFactories[i] = NULL;
    }

    m_visionFrameFactory = NULL;

    /*
     * new all FrameFactories.
     * because this called on open(). so we don't know current scenario
     */

    m_previewFrameFactory = new ExynosCameraFrameFactoryPreview(m_cameraId, m_parameters);
    m_previewFrameFactory->setFrameManager(m_frameMgr);


    int nReprocessingInstance = 0;
    m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PROCESSED] = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_parameters);
    m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PROCESSED]->setFrameManager(m_frameMgr);
    nReprocessingInstance++;

#ifdef USE_REMOSAIC_CAPTURE
    if (m_parameters->isUseRemosaicCapture() == true) {
        m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PURE_REMOSAIC] = new ExynosCameraFrameReprocessingFactoryRemosaic(m_cameraId, m_parameters);
        m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PURE_REMOSAIC]->setFrameManager(m_frameMgr);
        nReprocessingInstance++;
    }
#endif

    m_setNumOfReprocessingInstance(nReprocessingInstance);

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

    for (int i = 0; i < REPROCESSING_FRAME_FACTORY_MAX; i++) {
        if (m_pReprocessingFrameFactories[i] != NULL) {
            if (m_pReprocessingFrameFactories[i]->isCreated() == true) {
                ret = m_pReprocessingFrameFactories[i]->destroy();
                if (ret != NO_ERROR)
                    CLOGE("m_reprocessingFrameFactory[%d] destroy fail", i);
            }

            SAFE_DELETE(m_pReprocessingFrameFactories[i]);
            CLOGD("m_reprocessingFrameFactory[%d] destroyed", i);
        }
    }

    if (m_visionFrameFactory != NULL
        && m_visionFrameFactory->isCreated() == true) {
        ret = m_visionFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("m_visionFrameFactory destroy fail");
    }

    SAFE_DELETE(m_previewFrameFactory);
    CLOGD("m_previewFrameFactory destroyed");

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

#ifdef DEBUG_CLASS_INFO
void ExynosCamera::m_dumpClassInfo()
{
    CLOGD("-IN-");

    char propertyValue[PROPERTY_VALUE_MAX] ={0};
    property_get(CAMERA_DEBUG_PROPERTY_KEY_FUNCTION_CLASSINFO, propertyValue, "0");

    /* This code allows developer who wants to check class information. */
    /* They just set system property, camera.debug.function.classinfo, as "1". */
    /* enable : adb shell setprop camera.debug.function.classinfo 1 */
    /* disable : adb shell setprop camera.debug.function.classinfo 0 */
#ifdef DEBUG_PROPERTY_FUNCTION_CLASSINFO
    if ((strncmp(propertyValue, "1", sizeof(propertyValue)) == 0))
#endif
    {
        typedef struct classInfo {
            char *name;
            size_t size;
        } classInfo_t;
        classInfo_t info[] = {
            /* 1. static component : create one time  */
            /* camera */
            {"ExynosCamera", sizeof(ExynosCamera)},
            /* parameter */
            {"ExynosCamera1Parameters", sizeof(ExynosCamera1Parameters)},
            /* activity */
            {"ExynosCameraActivityControl", sizeof(ExynosCameraActivityControl)},
            {"ExynosCameraActivityAutofocus", sizeof(ExynosCameraActivityAutofocus)},
            {"ExynosCameraActivityFlash", sizeof(ExynosCameraActivityFlash)},
            {"ExynosCameraActivitySpecialCapture", sizeof(ExynosCameraActivitySpecialCapture)},
            {"ExynosCameraActivityUCTL", sizeof(ExynosCameraActivityUCTL)},
            /* framefactory */
            {"ExynosCameraFrameFactoryPreview", sizeof(ExynosCameraFrameFactoryPreview)},
            {"ExynosCameraFrameReprocessingFactory", sizeof(ExynosCameraFrameReprocessingFactory)},
            {"ExynosCameraFrameReprocessingFactoryRemosaic", sizeof(ExynosCameraFrameReprocessingFactoryRemosaic)},
            /* pipe */
            {"ExynosCameraPipe", sizeof(ExynosCameraPipe)},
            {"ExynosCameraPipeFlite", sizeof(ExynosCameraPipeFlite)},
            {"ExynosCameraMCPipe", sizeof(ExynosCameraMCPipe)},
            {"ExynosCameraPipeJpeg", sizeof(ExynosCameraPipeJpeg)},
            {"ExynosCameraPipeGSC", sizeof(ExynosCameraPipeGSC)},
            /* bufferMgr */
            {"ExynosCameraBufferManager", sizeof(ExynosCameraBufferManager)},
            {"InternalExynosCameraBufferManager", sizeof(InternalExynosCameraBufferManager)},
            {"GrallocExynosCameraBufferManager", sizeof(GrallocExynosCameraBufferManager)},
            /* frameMgr */
            {"ExynosCameraFrameManager", sizeof(ExynosCameraFrameManager)},
            /* frameselector */
            {"ExynosCameraFrameSelector", sizeof(ExynosCameraFrameSelector)},
            /* 2. dynamic component : create / delete each scenario */
            /* frame */
            {"ExynosCameraFrame", sizeof(ExynosCameraFrame)}
        };

        for(int i = 0; i < (sizeof(info) / sizeof(classInfo_t)) ; i++) {
            CLOGD("class info name:%s size:%zu", info[i].name , info[i].size);
        }
    }

    CLOGD("-OUT-");
}
#endif

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
#ifdef USE_VRA_GROUP
    int dsWidth = 0, dsHeight = 0;
    int dsFormat = 0;
#endif
    ExynosRect bdsRect;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;
    int batchSize = 1;
    int maxBufferCount = 1;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
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

#ifdef USE_VRA_GROUP
    m_parameters->getHwVraInputSize(&dsWidth, & dsHeight);
    dsFormat = m_parameters->getHwVraInputFormat();
    CLOGI("DS Size %dx%d Format %x", dsWidth, dsHeight, dsFormat);
#endif

    hwPreviewFormat = m_parameters->getHwPreviewFormat();

#ifdef USE_DUAL_CAMERA
    ExynosRect fusionSrcRect;
    ExynosRect fusionDstRect;

    if (m_parameters->getDualCameraMode() == true) {
        int previewW = 0, previewH = 0;

        m_parameters->getPreviewSize(&previewW, &previewH);
        CLOGI("Preview width x height = %dx%d", previewW, previewH);

        ret = m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);
        if (ret != NO_ERROR) {
            CLOGE("getFusionSize(%d, %d)) fail", hwPreviewW, hwPreviewH);
            return ret;
        }
        CLOGI("Fusion src width x height = %dx%d", fusionSrcRect.w, fusionSrcRect.h);
        CLOGI("Fusion dst width x height = %dx%d", fusionDstRect.w, fusionDstRect.h);
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
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);

#ifdef USE_DUAL_CAMERA
        } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);
#endif
        } else if (m_parameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_ispBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);
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
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);
#ifdef USE_DUAL_CAMERA
        } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);
#endif
        } else if (m_parameters->getDualMode() == false) {
            type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
            if(m_parameters->getUHDRecordingMode() == true)
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP_UHD]);
            else
                m_hwDisBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::ISP]);
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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        // hack for debugging
        needMmap = true;

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, fusionDstRect.w, fusionDstRect.h);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(hwPreviewFormat(%x), fusionDstRect(%d x %d)) fail",
                hwPreviewFormat, fusionDstRect.w, fusionDstRect.h);
            return INVALID_OPERATION;
        }
        /* perview buffer use for mastercamera */
        if (getCameraIdInternal() == CAMERA_ID_BACK_0) {
            ret = m_allocBuffers(m_scpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
                fusionDstRect.w, fusionDstRect.h, fusionDstRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_FULL_RANGE),
                needMmap);
        }
    } else
#endif
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
     * dual's front doesn't need m_zoomScalerBufferMgr, by src(m_sccBufferMgr), dst(m_scpBufferMgr)
     */
    if ((m_parameters->getSupportedZoomPreviewWIthScaler()) &&
        ! ((m_parameters->getDualMode() == true) &&
           (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1))) {
        int scalerW = hwPreviewW;
        int scalerH = hwPreviewH;

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            scalerW = fusionSrcRect.w;
            scalerH = fusionSrcRect.h;
        }
#endif

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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            // hack for debugging
            needMmap = true;

            ret = m_allocBuffers(m_zoomScalerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
                fusionSrcRect.w, fusionSrcRect.h, fusionSrcRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_FULL_RANGE),
                needMmap);
        } else
#endif
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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraPreviewFusionMode() == true) {
        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(hwPreviewFormat, planeSize, fusionSrcRect.w, fusionSrcRect.h);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(hwPreviewFormat(%x), fusionSrcRect(%d x %d)) fail",
                hwPreviewFormat, fusionSrcRect.w, fusionSrcRect.h);
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

        if (m_parameters->isFlite3aaOtf() == true)
            maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
        else
            maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;


        bool needMmap = false;

        // hack for debugging
        needMmap = true;

        ret = m_allocBuffers(m_fusionBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, true,
            fusionSrcRect.w, fusionSrcRect.h, fusionSrcRect.w, m_parameters->convertingHalPreviewFormat(hwPreviewFormat, YUV_FULL_RANGE),
            needMmap);
        if (ret < 0) {
            CLOGE("m_fusionBufferMgr m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    }
#endif

#ifdef USE_BUFFER_WITH_STRIDE
    int stride = m_scpBufferMgr->getBufStride();
    if (stride != hwPreviewW) {
        CLOGI("hwPreviewW(%d), stride(%d)", hwPreviewW, stride);
        if (stride == 0) {
            /* If the SCP buffer manager is not instance of GrallocExynosCameraBufferManager
               (In case of setPreviewWindow(null) is called), return value of setHwPreviewStride()
               will be zero. If this value is passed as SCP width to firmware, firmware will
               generate PABORT error. */
            CLOGW("HACK: Invalid stride(%d). It will be replaced as hwPreviewW(%d) value.",
                stride, hwPreviewW);
            stride = hwPreviewW;
        }
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        stride = fusionSrcRect.w;
    }
#endif

    m_parameters->setHwPreviewStride(stride);

    /* Do not allocate SCC buffer */
#if 0
    if (m_parameters->isSccCapture() == true) {
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

        ret = m_allocBuffers(m_sccBufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, batchSize, true, false);
        if (ret < 0) {
            CLOGE("m_sccBufferMgr m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    }
#endif

#ifdef USE_VRA_GROUP
    if (m_parameters->isMcscVraOtf() == false) {
        memset(planeSize, 0, sizeof(planeSize));

        ret = getYuvPlaneSize(dsFormat, planeSize, dsWidth, dsHeight);
        if (ret < 0) {
            CLOGE("getYuvPlaneSize(dsFormat(%x), dsWidth(%d x %d)) fail",
                dsFormat, dsWidth, dsHeight);
            return INVALID_OPERATION;
        }

        ret = getYuvFormatInfo(dsFormat, &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                hwPreviewFormat);
            return INVALID_OPERATION;
        }

        planeCount += 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_vra_buffers;
        needMmap = false;

        ret = m_allocBuffers(m_vraBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, batchSize, true, needMmap);
        if (ret < 0) {
            CLOGE("m_vraBufferMgr m_allocBuffers(bufferCount=%d) fail", maxBufferCount);
            return ret;
        }
    }
#endif

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
        if (pictureFormat == V4L2_PIX_FMT_NV21M) {
            planeCount      = 2;
            planeSize[0]    = pictureW * pictureH;
            planeSize[1]    = pictureW * pictureH / 2;
        } else {
            planeCount      = 1;
            planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), pictureW, pictureH);
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
                m_jpegBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD]);
            } else {
                m_jpegBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG]);

                /* alloc at once */
                minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
            }
#ifdef USE_DUAL_CAMERA
        } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
            if (m_parameters->getUHDRecordingMode() == true) {
                m_jpegBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG_UHD]);
            } else {
                m_jpegBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::JPEG]);

                /* alloc at once */
                minBufferCount = NUM_BURST_GSC_JPEG_INIT_BUFFER;
            }
#endif
        } else
#endif
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

        if (m_parameters->getHighResolutionCallbackMode() == false) {
            ret = m_allocBuffers(m_jpegBufferMgr, planeCount, planeSize, bytesPerLine,
                                 minBufferCount, maxBufferCount, batchSize, type, allocMode,
                                 false, true);
            if (ret != NO_ERROR)
                CLOGE("jpegSrcHeapBuffer m_allocBuffers(bufferCount=%d) fail",
                        NUM_REPROCESSING_BUFFERS);

            CLOGI("m_allocBuffers(JPEG Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                    pictureW, pictureH, planeCount, maxBufferCount);
        }
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

#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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
    if (m_vraBufferMgr != NULL) {
        m_vraBufferMgr->deinit();
    }
    if (m_hwDisBufferMgr != NULL) {
        m_hwDisBufferMgr->deinit();
    }
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
    }
#ifdef USE_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }

    if (m_fusionReprocessingBufferMgr != NULL) {
        m_fusionReprocessingBufferMgr->deinit();
    }
#endif
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit(m_cameraId);
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
        m_postPictureBufferMgr->deinit(m_cameraId);
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit(m_cameraId);
    }
#ifdef USE_MSC_CAPTURE
    if (m_mscBufferMgr != NULL) {
        m_mscBufferMgr->deinit();
    }
#endif
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit(m_cameraId);
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
#if defined(USE_DUAL_CAMERA)
        && m_parameters->getDualCameraMode() == false
#endif
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

    int bufferCount = 2;
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

    /* perview callback buffer use for mastercamera */
#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* perview callback buffer use for mastercamera */
        if (getCameraIdInternal() == CAMERA_ID_BACK_0) {
            ret = m_allocBuffers(m_previewCallbackBufferMgr,
                                planeCount,
                                planeSize,
                                bytesPerLine,
                                bufferCount,
                                bufferCount,
                                batchSize,
                                type,
                                true,
                                true);
        }
    } else
#endif
    {
        ret = m_allocBuffers(m_previewCallbackBufferMgr,
                            planeCount,
                            planeSize,
                            bytesPerLine,
                            bufferCount,
                            bufferCount,
                            batchSize,
                            type,
                            true,
                            true);
    }

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

#ifdef USE_DUAL_CAMERA
/* this function should be used after fusion */
status_t ExynosCamera::m_putFusionReprocessingBuffers(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *buf)
{
    status_t ret = NO_ERROR;
    int masterCameraId, slaveCameraId;
    ExynosCameraDualFrameSelector *dualSelector = NULL;

    /* get the master, slave camera id and dualSelector to return the buffer */
    getDualCameraId(&masterCameraId, &slaveCameraId);
    dualSelector = ExynosCameraSingleton<ExynosCameraDualCaptureFrameSelector>::getInstance();

    if (buf == NULL) {
        CLOGE("Invalid buf(%d) - SyncType(%d) frame(%d)",
                buf->index, frame->getSyncType(), frame->getFrameCount());
        return INVALID_OPERATION;
    }

    switch (frame->getSyncType()) {
    case SYNC_TYPE_BYPASS:
        if (buf->index >= 0) {
            ret = dualSelector->releaseBuffer(masterCameraId, buf);
            if (ret != NO_ERROR) {
                CLOGE("dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        masterCameraId, buf->index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
        break;
    case SYNC_TYPE_SWITCH:
        if (buf->index >= 0) {
            ret = dualSelector->releaseBuffer(slaveCameraId, buf);
            if (ret != NO_ERROR) {
                CLOGE("dualSelector->releaseBuffer(%d, %d) frame:%d(%d) fail(%d)",
                        slaveCameraId, buf->index,
                        frame->getFrameCount(),
                        frame->getSyncType(), ret);
            }
        }
        break;
    case SYNC_TYPE_SYNC:
        /* putBuffer to fusionReprocessingBufferMgr */
        ret = m_putBuffers(m_fusionReprocessingBufferMgr, buf->index);
        if (ret < 0)
            CLOGE("bufferMgr->putBuffers(fusionReprocessingBufMgr, %d) fail, ret(%d)",
                    buf->index, ret);
        break;
    default:
        CLOGE("Invalid SyncType(%d) frame(%d)",
                frame->getSyncType(), frame->getFrameCount());
        break;
    }

    return ret;
}
#endif

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
                        needMmap,
                        m_cameraId);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc(m_cameraId);
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
                        needMmap,
                        m_cameraId);
    if (ret < 0) {
        CLOGE("setInfo fail");
        goto func_exit;
    }

    ret = bufManager->alloc(m_cameraId);
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

    struct ExynosCameraBuffer tempBuffer;

    if (refFrame != NULL) {
        CLOGV("refFrame(%d)", refFrame->getFrameCount());
    }

     /* 1. Make Frame */
#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        newFrame = m_reprocessingFrameFactory->createNewFrame(refFrame);
    } else {
#endif
    newFrame = m_reprocessingFrameFactory->createNewFrame(NULL);
#ifdef USE_DUAL_CAMERA
    }
#endif
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
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

    CLOGV(" dst_size(%dx%d), dst_crop_size(%dx%d)", dst_width, dst_height, dst_crop_width, dst_crop_height);

    return NO_ERROR;
}

status_t ExynosCamera::m_syncPrviewWithCSC(int32_t pipeId, int32_t gscPipe, ExynosCameraFrameSP_sptr_t frame)
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
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_NOREQ;

    /* copy metadata src to dst*/
    if ((m_parameters->getDualMode() == true) &&
        (getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_ISPC);
        dstNodeIndex = 0;

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
            scpNodeIndex = 0;
        else
#endif
            scpNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);

        srcBufMgr = m_sccBufferMgr;
        m_getBufferManager(gscPipe, &dstBufMgr, DST_BUFFER_DIRECTION);
    } else {
        srcNodeIndex = m_previewFrameFactory->getNodeType(PIPE_SCP);
        dstNodeIndex = 0;
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
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
        if (srcBuf.index >= 0 )
            srcBufMgr->cancelBuffer(srcBuf.index);
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
    ret = frame->getDstBuffer(pipeId, &srcBuf, srcNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
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
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)", pipeId, ret, frame->getFrameCount());
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
    CLOGI("");

    int ret = 0;
    bool loop = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSelector::result_t result;

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
        CLOGW("post picture is delayed(stacked %d frames), skip", postProcessQSize);
        usleep(WAITING_TIME);
        goto CLEAN;
    }

    result = m_sccCaptureSelector->selectFrames(newFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, m_pictureFrameFactory->getNodeType(bufPipeId));
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }
    newFrame->frameLock();

    CLOGI("Frame Count (%d)", newFrame->getFrameCount());

#ifdef USE_DUAL_CAMERA
    // update for capture meta data.
    if (m_parameters->getDualCameraMode() == true) {
        struct camera2_shot_ext temp_shot_ext;
        newFrame->getMetaData(&temp_shot_ext);

        if (m_parameters->setFusionInfo(&temp_shot_ext) != NO_ERROR) {
            CLOGE("m_parameters->setFusionInfo() fail");
        }
    }
#endif

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
        m_dstSccReprocessingQ->pushProcessQ(&newFrame);
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
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    int frameCount = 0;
    int curFrameCount = 0;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;
#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
#endif

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    if (list->empty()) {
        CLOGD("list is empty");
        return NO_ERROR;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_cameraId == masterCameraId &&
                frame->getSyncType() == SYNC_TYPE_SWITCH)
            return NO_ERROR;
    }
#endif

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
        CLOGW("frame count mismatch: expected(%d), current(%d, CAM%d, Type:%d, State:%d)", frameCount, curFrameCount,
                curFrame->getCameraId(), curFrame->getFrameType(), curFrame->getFrameState());
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
    ExynosCameraFrameSP_sptr_t curFrame = NULL;

    CLOGD("remaining frame(%d), we remove them all", queue->getSizeOfProcessQ());

    queue->release();

    CLOGD("EXIT ");

    return NO_ERROR;
}

status_t ExynosCamera::m_clearFrameQ(frame_queue_t *frameQ, uint32_t pipeId, uint32_t direction) {
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
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
        if (ret < 0)
            CLOGE("create IonAllocator fail (retryCount=%d)", retry);
        else {
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

status_t ExynosCamera::m_createInternalBufferManager(ExynosCameraBufferManager **bufferManager, const char *name, bool isShared)
{
    return m_createBufferManager(bufferManager, name, BUFFER_MANAGER_ION_TYPE, isShared);
}

status_t ExynosCamera::m_createBufferManager(
        ExynosCameraBufferManager **bufferManager,
        const char *name,
        buffer_manager_type type,
        bool isShared)
{
    status_t ret = NO_ERROR;

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret < 0)
            CLOGE("m_createIonAllocator fail");
        else
            CLOGD("m_createIonAllocator success");
    }

#ifdef USE_DUAL_CAMERA
    if (isShared) {
        if (strncmp(name, "MCSC_RE_BUF", strlen(name)) == 0) {
            *bufferManager = &m_sharedBufferManager[SHARED_BUFFER_MANAGER_SCC_REPROCESSING];
        } else if (strncmp(name, "JPEG_BUF", strlen(name)) == 0) {
            *bufferManager = &m_sharedBufferManager[SHARED_BUFFER_MANAGER_JPEG];
        } else if (strncmp(name, "POSTPICTURE_GSC_BUF", strlen(name)) == 0) {
            *bufferManager = &m_sharedBufferManager[SHARED_BUFFER_MANAGER_POSTPICTURE_GSC];
        } else if (strncmp(name, "THUMBNAIL_GSC_BUF", strlen(name)) == 0) {
            *bufferManager = &m_sharedBufferManager[SHARED_BUFFER_MANAGER_THUMBNAIL_GSC];
        } else {
            CLOGE("can't not support shared buffer manager(%s, %d)", name, type);
            *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
        }
    } else {
        *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    }
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator, isShared);
#else
    *bufferManager = ExynosCameraBufferManager::createBufferManager(type);
    (*bufferManager)->create(name, m_cameraId, m_ionAllocator);

    if (isShared) {
        CLOGV("isShared(%d)", isShared);
    }
#endif

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

#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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
#ifdef USE_DUAL_CAMERA
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

    if (fd > 0)
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

#ifdef USE_VRA_GROUP
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
            if (m_parameters->getDualMode() == true && getCameraId() == CAMERA_ID_BACK) {
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
#endif

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
    CLOGD("= getHWVdisMode               : %d", m_parameters->getHWVdisMode());
    CLOGD("= get3dnrMode                 : %d", m_parameters->get3dnrMode());

    CLOGD("============= Internal setting ====================");
    CLOGD("= isFlite3aaOtf               : %d", m_parameters->isFlite3aaOtf());
    CLOGD("= is3aaIspOtf                 : %d", m_parameters->is3aaIspOtf());
    CLOGD("= isIspMcscOtf                : %d", m_parameters->isIspMcscOtf());
    CLOGD("= isIspTpuOtf                 : %d", m_parameters->isIspTpuOtf());
    CLOGD("= isTpuMcscOtf                : %d", m_parameters->isTpuMcscOtf());
    CLOGD("= TpuEnabledMode              : %d", m_parameters->getTpuEnabledMode());
    CLOGD("= isReprocessing              : %d", m_parameters->isReprocessing());
    CLOGD("= isReprocessing3aaIspOTF     : %d", m_parameters->isReprocessing3aaIspOTF());
    CLOGD("= getUsePureBayerReprocessing : %d", m_parameters->getUsePureBayerReprocessing());

    int reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    switch(reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE:
        CLOGD("= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_NONE");
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        CLOGD("= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON");
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        CLOGD("= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON");
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        CLOGD("= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_PURE_DYNAMIC");
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        CLOGD("= getReprocessingBayerMode    : REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC");
        break;
    default:
        CLOGD("= getReprocessingBayerMode    : unexpected mode %d", reprocessingBayerMode);
        break;
    }

    CLOGD("= isSccCapture                : %d", m_parameters->isSccCapture());

#ifdef USE_DUAL_CAMERA
    CLOGD("= getDualCameraMode           : %d", m_parameters->getDualCameraMode());
    CLOGD("= isFusionEnabled             : %d", m_parameters->isFusionEnabled());
#endif

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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;

        // we need to calculate with set by app.
        m_parameters->getPreviewSize(&w, &h);
        m_parameters->getFusionSize(w, h, &fusionSrcRect, &fusionDstRect);

        CLOGD("= getFusionSize             : (%d, %d, %d, %d) -> (%d, %d, %d, %d)",
            fusionSrcRect.x, fusionSrcRect.y, fusionSrcRect.w, fusionSrcRect.h,
            fusionDstRect.x, fusionDstRect.y, fusionDstRect.w, fusionDstRect.h);
    }
#endif

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

    CLOGD("============= vendor setting ======================");
    m_printVendorCameraInfo();

    CLOGD("===================================================");
}

status_t ExynosCamera::m_putFrameBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId, enum buffer_direction_type bufferDirectionType)
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
            CLOGE("m_getBufferManager(pipeId(%d), SRC_BUFFER_DIRECTION)", pipeId);
            return ret;
        }

        if (bufferMgr == m_scpBufferMgr) {
            ret = bufferMgr->cancelBuffer(buffer.index);
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
