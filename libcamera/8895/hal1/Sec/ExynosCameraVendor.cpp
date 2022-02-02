/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraSec"
#include <cutils/log.h>

#include "ExynosCamera.h"

namespace android {

#define IS_OUTPUT_NODE(_FACTORY,_PIPE) \
    (((_FACTORY->getNodeType(_PIPE) >= OUTPUT_NODE) \
        && (_FACTORY->getNodeType(_PIPE) < MAX_OUTPUT_NODE)) ? true:false)

void ExynosCamera::initializeVision()
{
    CLOGI(" -IN-");

    m_mainSetupQThread[INDEX(PIPE_FLITE)] = new mainCameraThread(this, &ExynosCamera::m_mainThreadQSetupFLITE,
                                                                "mainThreadQSetupFLITE", PRIORITY_URGENT_DISPLAY);
    CLOGV("mainThreadQSetupFLITEThread created");

    m_parameters = new ExynosCamera1Parameters(m_cameraOriginId, m_use_companion);
    CLOGD("Parameters(Id=%d) created", m_cameraOriginId);

    /* Frame factories */
    m_visionFrameFactory = NULL;

    /* Memory allocator */
    m_ionAllocator = NULL;
    m_mhbAllocator = NULL;

    /* Frame manager */
    m_frameMgr = NULL;

    /* Group leader buffer */
    m_createInternalBufferManager(&m_3aaBufferMgr, "3AS_IN_BUF");
    /* Preview CB buffer: Output buffer of MCSC1(Preview CB) */
    m_createInternalBufferManager(&m_previewCallbackBufferMgr, "PREVIEW_CB_BUF");
    /* Bayer buffer: Output buffer of Flite or 3AC */
    m_createInternalBufferManager(&m_bayerBufferMgr, "BAYER_BUF");

    m_pipeFrameVisionDoneQ = new frame_queue_t;
    m_pipeFrameVisionDoneQ->setWaitTime(2000000000);

    /* Create Internal Queue */
    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        m_mainSetupQ[i] = new frame_queue_t;
        m_mainSetupQ[i]->setWaitTime(550000000);
    }

    m_ionClient = ion_open();
    if (m_ionClient < 0) {
        CLOGE("m_ionClient ion_open() fail");
        m_ionClient = -1;
    }

    memset(&m_getMemoryCb, 0, sizeof(camera_request_memory));

    m_previewWindow = NULL;
    m_previewEnabled = false;

    m_displayPreviewToggle = 0;

    m_visionFps = 0;
    m_visionAe = 0;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    /* ExynosConfig setting */
    m_setConfigInform();

    /* Frame manager setting */
    m_setFrameManager();

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("visionThread created");

    CLOGI(" -OUT-");
}

void ExynosCamera::releaseVision()
{
    CLOGI(" -IN-");
    int ret = 0;

    if (m_frameMgr != NULL) {
        m_frameMgr->stop();
    }

    if (m_visionFrameFactory != NULL
        && m_visionFrameFactory->isCreated() == true) {
        ret = m_visionFrameFactory->destroy();
        if (ret != NO_ERROR)
            CLOGE("m_visionFrameFactory destroy fail");
    }

    SAFE_DELETE(m_visionFrameFactory);
    CLOGD("m_visionFrameFactory destroyed");

    if (m_parameters != NULL) {
        delete m_parameters;
        m_parameters = NULL;
        CLOGD("Parameters(Id=%d) destroyed", m_cameraId);
    }

    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->deinit();
    }

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }

    if (m_ionClient >= 0) {
        ion_close(m_ionClient);
        m_ionClient = -1;
    }

    if (m_ionAllocator != NULL) {
        delete m_ionAllocator;
        m_ionAllocator = NULL;
    }

    if (m_mhbAllocator != NULL) {
        delete m_mhbAllocator;
        m_mhbAllocator = NULL;
    }

    /* vision */
    if (m_pipeFrameVisionDoneQ != NULL) {
        delete m_pipeFrameVisionDoneQ;
        m_pipeFrameVisionDoneQ = NULL;
    }

    for(int i = 0 ; i < MAX_NUM_PIPES ; i++ ) {
        if (m_mainSetupQ[i] != NULL) {
            delete m_mainSetupQ[i];
            m_mainSetupQ[i] = NULL;
        }
    }

    if (m_3aaBufferMgr != NULL) {
        delete m_3aaBufferMgr;
        m_3aaBufferMgr = NULL;
        CLOGD("BufferManager(m_3aaBufferMgr) destroyed");
    }

    if (m_bayerBufferMgr != NULL) {
        delete m_bayerBufferMgr;
        m_bayerBufferMgr = NULL;
        CLOGD("BufferManager(bayerBufferMgr) destroyed");
    }

    if (m_previewCallbackBufferMgr != NULL) {
        delete m_previewCallbackBufferMgr;
        m_previewCallbackBufferMgr = NULL;
        CLOGD("BufferManager(previewCallbackBufferMgr) destroyed");
    }

    if (m_frameMgr != NULL) {
        delete m_frameMgr;
        m_frameMgr = NULL;
    }

    CLOGI(" -OUT-");
}

status_t ExynosCamera::setPreviewWindow(preview_stream_ops *w)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    int width, height;
    int halPreviewFmt = 0;
    bool flagRestart = false;
    buffer_manager_type bufferType = BUFFER_MANAGER_ION_TYPE;

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" Vision mode does not support");
            return NO_ERROR;
        }
    } else {
        CLOGW("m_parameters is NULL. Skipped");
        return INVALID_OPERATION;
    }

    if (previewEnabled() == true) {
        CLOGW("Preview is started, we forcely re-start preview");
        flagRestart = true;
        m_disablePreviewCB = true;
        stopPreview();
    }

    if (m_scpBufferMgr != NULL) {
        CLOGD(" scp buffer manager need recreate");
        m_scpBufferMgr->deinit();
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
    }

    m_previewWindow = w;

    if (m_previewWindow == NULL) {
        bufferType = BUFFER_MANAGER_ION_TYPE;
        CLOGW("Preview window is NULL, create internal buffer for preview");
    } else {
        halPreviewFmt = m_parameters->getHalPixelFormat();
        bufferType = BUFFER_MANAGER_GRALLOC_TYPE;
        m_parameters->getHwPreviewSize(&width, &height);

        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator();

#ifdef RESERVED_MEMORY_FOR_GRALLOC_ENABLE
        if (m_parameters->getRecordingHint() == false
            && !(m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE && getCameraIdInternal() == CAMERA_ID_BACK)) {
            ret = m_grAllocator->init(m_previewWindow,
                                      m_exynosconfig->current->bufInfo.num_preview_buffers,
                                      m_exynosconfig->current->bufInfo.preview_buffer_margin,
                                      (GRALLOC_SET_USAGE_FOR_CAMERA | GRALLOC_USAGE_CAMERA_RESERVED));
        } else
#endif
        {
            ret = m_grAllocator->init(m_previewWindow,
                                      m_exynosconfig->current->bufInfo.num_preview_buffers,
                                      m_exynosconfig->current->bufInfo.preview_buffer_margin);
        }

        if (ret != NO_ERROR) {
            CLOGE("Gralloc init fail, ret(%d)", ret);
            goto func_exit;
        }

        ret = m_grAllocator->setBuffersGeometry(width, height, halPreviewFmt);
        if (ret != NO_ERROR) {
            CLOGE("Gralloc setBufferGeomety fail, size(%dx%d), fmt(%d), ret(%d)",
                 width, height, halPreviewFmt, ret);
            goto func_exit;
        }
    }

    m_createBufferManager(&m_scpBufferMgr, "PREVIEW_BUF", bufferType);
    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE) {
        m_scpBufferMgr->setAllocator(m_grAllocator);
    }

    if (flagRestart == true)
        startPreview();

func_exit:
    m_disablePreviewCB = false;

    return ret;
}

status_t ExynosCamera::startPreview()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("");

    if ((m_parameters == NULL) && (m_frameMgr == NULL)) {
        CLOGE(" initalize HAL");
        this->initialize();
    }

    property_set("persist.sys.camera.preview","2");

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    int32_t skipFdFrameCount = 0;
    unsigned int fdCallbackSize = 0;
    /* setup frame thread */
    int pipeId = PIPE_3AA;

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    m_flagAFDone = false;
    m_hdrSkipedFcount = 0;
    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_curMinFps = 0;
    m_isNeedAllocPictureBuffer = false;
    m_flagThreadStop= false;
    m_frameSkipCount = 0;
    m_parameters->setIsThumbnailCallbackOn(false);
    m_stopLongExposure = false;

    if ((m_parameters == NULL) || (m_frameMgr == NULL)) {
        CLOGE("create parameter frameMgr failed");
        return INVALID_OPERATION;
    }

    int controlPipeId = m_getBayerPipeId();

#ifdef FIRST_PREVIEW_TIME_CHECK
    if (m_flagFirstPreviewTimerOn == false) {
        m_firstPreviewTimer.start();
        m_flagFirstPreviewTimerOn = true;

        CLOGD("m_firstPreviewTimer start");
    }
#endif

    if (m_previewEnabled == true) {
        return INVALID_OPERATION;
    }

    /* vision */
    CLOGI(" getVisionMode(%d)", m_parameters->getVisionMode());
    if (m_parameters->getVisionMode() == true) {
        ret = startPreviewVision();
        return ret;
    }

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /* HACK
     * stopPreview() close companion
     * so. start here again
     * The reason why we did't start compaion on stopPreview() is..
     * release() can be come just after stopPreview().
     * If it is, release can be delay by companion open and. then app make exception
     */
    if (m_startCompanion() != NO_ERROR) {
        CLOGE("m_startCompanion fail");
    }
#endif

    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;
    {
        if (m_getMemoryCb != NULL) {
            m_fdCallbackHeap = m_getMemoryCb(-1, fdCallbackSize, 1, m_callbackCookie);
            if (!m_fdCallbackHeap || m_fdCallbackHeap->data == MAP_FAILED) {
                CLOGE("m_getMemoryCb(%d) fail", fdCallbackSize);
                m_fdCallbackHeap = NULL;
                goto err;
            }
        }
    }

    {
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }
    }

    {
        /* update frameMgr parameter */
        m_setFrameManagerInfo();

        /* frame manager start */
        m_frameMgr->start();

        /*
         * This is for updating parameter value at once.
         * This must be just before making factory
         */
        m_parameters->updateTpuParameters();
    }

    if ((m_parameters->getRestartPreview() == true) ||
        m_previewBufferCount != m_parameters->getPreviewBufferCount()) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            CLOGE("setPreviewWindow fail");
            return INVALID_OPERATION;
        }

        m_previewBufferCount = m_parameters->getPreviewBufferCount();
    }

    {
        CLOGI("setBuffersThread is run");
        {
            m_setBuffersThread->run(PRIORITY_DEFAULT);
        }
    }

    if (m_bayerCaptureSelector == NULL) {
        ExynosCameraBufferManager *bufMgr = NULL;

#ifdef DEBUG_RAWDUMP
        if (reprocessingMode == PROCESSING_MODE_NON_REPROCESSING_YUV)
            bufMgr = m_fliteBufferMgr;
        else
#endif
            bufMgr = m_bayerBufferMgr;

        {
            m_bayerCaptureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr
#ifdef SUPPORT_DEPTH_MAP
                    , m_depthCallbackQ, m_depthMapBufferMgr
#endif
                    );
        }

        if (m_parameters->isReprocessingCapture() == true) {
            ret = m_bayerCaptureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0)
                CLOGE(" setFrameHoldCount(%d) is fail",
                        REPROCESSING_BAYER_HOLD_COUNT);
        }
    }

    if (m_yuvCaptureSelector == NULL) {
        ExynosCameraBufferManager *bufMgr = NULL;

        /* TODO: Dynamic select buffer manager for capture */
        bufMgr = m_yuvBufferMgr;

        {
            m_yuvCaptureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr);
        }
    }

    if (m_bayerCaptureSelector != NULL)
        m_bayerCaptureSelector->release();

    if (m_yuvCaptureSelector != NULL)
        m_yuvCaptureSelector->release();

    switch (reprocessingMode) {
        case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
            m_captureSelector = m_bayerCaptureSelector;
            break;
        case PROCESSING_MODE_REPROCESSING_YUV:
        case PROCESSING_MODE_NON_REPROCESSING_YUV:
            m_captureSelector = m_yuvCaptureSelector;
            break;
        default:
            CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
            break;
    }

    if (m_previewFrameFactory->isCreated() == false
       ) {
        ret = m_previewFrameFactory->create();
        if (ret < 0) {
            CLOGE("m_previewFrameFactory->create() failed");
            goto err;
        }

        CLOGD("FrameFactory(previewFrameFactory) created");
    }

    {
        if (m_parameters->getUseFastenAeStable() == true &&
                m_parameters->getDualMode() == false &&
                m_isFirstStart == true) {

            ret = m_fastenAeStable();
            if (ret < 0) {
                CLOGE("m_fastenAeStable() failed");
                ret = INVALID_OPERATION;
                goto err;
            } else {
                skipFrameCount = 0;
                skipFdFrameCount = 0;
                m_parameters->setUseFastenAeStable(false);
            }
        } else if (m_parameters->getDualMode() == true) {
            skipFrameCount = INITIAL_SKIP_FRAME + 2;
            skipFdFrameCount = 0;
        } else if (m_isFirstStart == false) {
            {
                skipFrameCount = 0;
                skipFdFrameCount = INITIAL_SKIP_FD_FRAME;
            }
        }
    }

#ifdef SET_FPS_SCENE /* This codes for 5260, Do not need other project */
    struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
    if (initMetaData != NULL) {
        m_parameters->duplicateCtrlMetadata(initMetaData);

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS,
                                                initMetaData->shot.ctl.aa.aeTargetFpsRange[0],
                                                controlPipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS,
                                                initMetaData->shot.ctl.aa.aeTargetFpsRange[1],
                                                controlPipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_SCENE_MODE,
                                                initMetaData->shot.ctl.aa.sceneMode,
                                                controlPipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);
        delete initMetaData;
        initMetaData = NULL;
    } else {
        CLOGE("initMetaData is NULL");
    }
#elif SET_FPS_FRONTCAM
    if (m_parameters->getCameraId() == CAMERA_ID_FRONT ||
        m_parameters->getCameraId() == CAMERA_ID_BACK_1) {
        struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
        if (initMetaData != NULL) {
            m_parameters->duplicateCtrlMetadata(initMetaData);
            CLOGD(" setControl for Frame Range. (%d - %d)",
                   initMetaData->shot.ctl.aa.aeTargetFpsRange[0], initMetaData->shot.ctl.aa.aeTargetFpsRange[1]);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MIN_TARGET_FPS,
                                                    initMetaData->shot.ctl.aa.aeTargetFpsRange[0],
                                                    controlPipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_MAX_TARGET_FPS,
                                                    initMetaData->shot.ctl.aa.aeTargetFpsRange[1],
                                                    controlPipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            delete initMetaData;
            initMetaData = NULL;
        } else {
            CLOGE("initMetaData is NULL");
        }
    }
#endif

    {
        m_parameters->setFrameSkipCount(skipFrameCount);
        m_fdFrameSkipCount = skipFdFrameCount;
    }

    {
        m_setBuffersThread->join();
    }

    if (m_isSuccessedBufferAllocation == false) {
        {
            CLOGE("m_setBuffersThread() failed");
            ret = INVALID_OPERATION;
            goto err;
        }
    }

    /* FD-AE is always on */
#ifdef USE_FD_AE
    m_enableFaceDetection(m_parameters->getFaceDetectionMode());
#endif

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("m_startPreviewInternal() failed");
        goto err;
    }

    if (m_parameters->isReprocessingCapture() == true
        && m_parameters->checkEnablePicture() == true) {
#ifdef START_PICTURE_THREAD
        if (m_parameters->getDualMode() != true) {
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);
        } else
#endif
        {
            m_startPictureInternal();
        }
    } else {
        m_pictureFrameFactory = m_previewFrameFactory;
        CLOGD("FrameFactory(pictureFrameFactory) created");
    }

    if (m_parameters->checkEnablePicture() == true) {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);
    }

    if (m_previewWindow != NULL)
        m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

    /* setup frame thread */
    if (m_parameters->isFlite3aaOtf() == false) {
        pipeId = PIPE_FLITE;
    }

    CLOGD("setupThread Thread start pipeId(%d)", pipeId);
    m_mainSetupQThread[INDEX(pipeId)]->run(PRIORITY_URGENT_DISPLAY);

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    m_previewThread->run(PRIORITY_DISPLAY);

    m_mainThread->run(PRIORITY_DEFAULT);
    if(m_parameters->getCameraId() == CAMERA_ID_BACK) {
        m_autoFocusContinousThread->run(PRIORITY_DEFAULT);
    }
    m_monitorThread->run(PRIORITY_DEFAULT);

    if (m_parameters->getGDCEnabledMode() == true) {
        CLOGD("GDC Thread start");
        m_gdcThread->run(PRIORITY_DEFAULT);
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == false)) {
        CLOGD("High resolution preview callback start");
        if (skipFrameCount > 0)
            m_skipReprocessing = true;
        m_highResolutionCallbackRunning = true;
        m_parameters->setOutPutFormatNV21Enable(true);
        if (m_parameters->isReprocessingCapture() == true) {
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);
            m_startPictureInternalThread->join();
        }
        m_prePictureThread->run(PRIORITY_DEFAULT);
    }

#if 0 /* Dead Code */
    if (m_parameters->getUseFastenAeStable() == true &&
        m_parameters->getCameraId() == CAMERA_ID_BACK &&
        m_parameters->getDualMode() == false &&
        m_parameters->getRecordingHint() == false &&
        m_isFirstStart == true) {
        /* AF mode is setted as INFINITY in fastenAE, and we should update that mode */
        m_exynosCameraActivityControl->setAutoFocusMode(FOCUS_MODE_INFINITY);

        m_parameters->setUseFastenAeStable(false);
        m_exynosCameraActivityControl->setAutoFocusMode(m_parameters->getFocusMode());
        m_isFirstStart = false;
        m_parameters->setIsFirstStartFlag(m_isFirstStart);
    }
#endif
    m_isFirstStart = false;
    m_parameters->setIsFirstStartFlag(m_isFirstStart);

#ifdef BURST_CAPTURE
    m_burstInitFirst = true;
#endif

    return NO_ERROR;

err:
/* Warning !!!!!!   Don't change "ret" value in error case */
/* Need to return the ret value as it was and process error-handling */
#ifdef USE_SENSOR_RETENTION
    if (m_previewFrameFactory->setControl(V4L2_CID_IS_PREVIEW_STATE, 1, PIPE_3AA) != NO_ERROR) {
        CLOGE(" PIPE_%d V4L2_CID_IS_PREVIEW_STATE fail",
             PIPE_3AA);
    }
#endif

    /* frame manager stop */
    m_frameMgr->stop();

    m_setBuffersThread->join();

    m_releaseBuffers();

    return ret;
}

status_t ExynosCamera::startPreviewVision()
{
    CLOGI(" ");
    int ret = 0;
    int pipeId = PIPE_FLITE;

    /* update frameMgr parameter */
    m_setFrameManagerInfo();

    /* frame manager start */
    m_frameMgr->start();

    /*
     * This is for updating parameter value at once.
     * This must be just before making factory
     */
    m_parameters->updateTpuParameters();

    ret = m_setVisionBuffers();
    if (ret < 0) {
        CLOGE("m_setVisionBuffers() fail");
        return ret;
    }

    {
        ret = m_setVisionCallbackBuffer();
        if (ret < 0) {
            CLOGE("m_setVisionCallbackBuffer() fail");
            return INVALID_OPERATION;
        }
    }

    if (m_visionFrameFactory == NULL) {
        m_visionFrameFactory = (ExynosCameraFrameFactory *)new ExynosCameraFrameFactoryVision(m_cameraOriginId, m_parameters);
        m_visionFrameFactory->setFrameManager(m_frameMgr);

        ret = m_visionFrameFactory->create();
        if (ret < 0) {
            CLOGE("m_visionFrameFactory->create() failed");
            return ret;
        }
        CLOGD("FrameFactory(VisionFrameFactory) created");
    }

    m_parameters->setFrameSkipCount(0);

    ret = m_startVisionInternal();
    if (ret < 0) {
        CLOGE("m_startVisionInternal() failed");
        return ret;
    }

    CLOGD("VisionThread Thread start");
    m_visionThread->run(PRIORITY_DEFAULT);

    CLOGD("setupThread Thread start pipeId(%d)", pipeId);
    m_mainSetupQThread[INDEX(pipeId)]->run(PRIORITY_URGENT_DISPLAY);

    return NO_ERROR;
}

void ExynosCamera::stopPreview()
{
    CLOGI("");
    int ret = 0;

    if (m_parameters->getVisionMode() == true) {
        stopPreviewVision();
        return;
    }

    char previewMode[100] = {0};

    property_get("persist.sys.camera.preview", previewMode, "");

    if (!strstr(previewMode, "0"))
        CLOGI(" persist.sys.camera.preview(%s)", previewMode);

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrameSP_sptr_t frame = NULL;

    m_flagLLSStart = false;
    m_flagFirstPreviewTimerOn = false;

    if (m_previewEnabled == false) {
        CLOGD(" preview is not enabled");
        property_set("persist.sys.camera.preview","0");
        return;
    }

    m_stopLongExposure = true;

    if (m_pictureEnabled == true) {
        CLOGW("m_pictureEnabled == true (picture is not finished)");
        int retry = 0;
        do {
            usleep(WAITING_TIME);
            retry++;
        } while(m_pictureEnabled == true && retry < (TOTAL_WAITING_TIME/WAITING_TIME));
        CLOGW("wait (%d)msec (because, picture is not finished)",
                WAITING_TIME * retry / 1000);
    }

    m_parameters->setIsThumbnailCallbackOn(false);

    m_startPictureInternalThread->join();

    m_startPictureBufferThread->join();

    m_autoFocusRunning = false;
    m_exynosCameraActivityControl->cancelAutoFocus();

    CLOGD(" (%d, %d)", m_flashMgr->getNeedCaptureFlash(), m_pictureEnabled);
    if (m_flashMgr->getNeedCaptureFlash() == true && m_pictureEnabled == true) {
        CLOGD(" force flash off");
        m_exynosCameraActivityControl->cancelFlash();
        autoFocusMgr->stopAutofocus();
        m_isTryStopFlash = true;
        /* m_exitAutoFocusThread = true; */
    }

    /* Wait the end of the autoFocus Thread in order to the autofocus and the pre-flash is completed.*/
    m_autoFocusLock.lock();
    m_exitAutoFocusThread = true;
    m_autoFocusLock.unlock();

    int flashMode = AA_FLASHMODE_OFF;
    int waitingTime = FLASH_OFF_MAX_WATING_TIME / TOTAL_FLASH_WATING_COUNT; /* Max waiting time: 500ms, Count:10, Waiting time: 50ms */

    flashMode = m_flashMgr->getFlashStatus();
    if ((flashMode == AA_FLASHMODE_ON_ALWAYS) || (m_flashMgr->getNeedFlashOffDelay() == true)) {
        int i = 0;
        CLOGD(" flash torch was enabled");

        m_parameters->setFrameSkipCount(100);
        do {
            if (m_flashMgr->checkFlashOff() == false) {
                usleep(waitingTime);
            } else {
                CLOGD("turn off the flash torch.(%d)", i);

                flashMode = m_flashMgr->getFlashStatus();
                if (flashMode == AA_FLASHMODE_OFF || flashMode == AA_FLASHMODE_CANCEL)
                {
                    m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
                }
                usleep(waitingTime);
                break;
            }
        } while(++i < TOTAL_FLASH_WATING_COUNT);

        if (i >= TOTAL_FLASH_WATING_COUNT) {
            CLOGD("timeOut-flashMode(%d),checkFlashOff(%d)",
                flashMode, m_flashMgr->checkFlashOff());
        }
    } else if (m_isTryStopFlash == true) {
        usleep(waitingTime*3);  /* 150ms */
        m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
    }

    m_flashMgr->setNeedFlashOffDelay(false);

    m_previewFrameFactory->setStopFlag();
    if (m_parameters->isReprocessingCapture() == true
        && m_reprocessingFrameFactory->isCreated() == true)
        m_reprocessingFrameFactory->setStopFlag();
    m_flagThreadStop = true;

    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
    m_yuvcallbackCounter.clearCount();
    if (m_captureSelector != NULL) {
        m_captureSelector->cancelPicture();
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        m_skipReprocessing = false;
        m_highResolutionCallbackRunning = false;

        CLOGD("High resolution preview callback stop");
        if(getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1) {
            m_captureSelector->cancelPicture();
            m_captureSelector->wakeupQ();
            CLOGD("High resolution m_captureSelector cancel");
        }

        m_prePictureThread->requestExitAndWait();
        m_parameters->setOutPutFormatNV21Enable(false);
        m_highResolutionCallbackQ->release();
    }

    ret = m_stopPictureInternal();
    if (ret < 0)
        CLOGE("m_stopPictureInternal fail");

    m_exynosCameraActivityControl->stopAutoFocus();
    m_autoFocusThread->requestExitAndWait();

    if (m_previewQ != NULL) {
        m_previewQ->sendCmd(WAKE_UP);
    } else {
        CLOGI(" m_previewQ is NULL");
    }

    m_gdcThread->stop();
    m_gdcQ->sendCmd(WAKE_UP);
    m_gdcThread->requestExitAndWait();

    m_zoomPreviwWithCscThread->stop();
    if (m_zoomPreviwWithCscQ != NULL) {
        m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
    }
    m_zoomPreviwWithCscThread->requestExitAndWait();

    m_pipeFrameDoneQ->sendCmd(WAKE_UP);
    m_mainThread->requestExitAndWait();
    m_monitorThread->requestExitAndWait();

    m_shutterCallbackThread->requestExitAndWait();

    m_previewThread->stop();
    if (m_previewQ != NULL) {
        m_previewQ->sendCmd(WAKE_UP);
    }
    m_previewThread->requestExitAndWait();

    if (m_parameters->isFlite3aaOtf() == true) {
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
    } else {
        if (m_mainSetupQThread[INDEX(PIPE_FLITE)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
            m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();
        }
    }

    m_autoFocusContinousThread->stop();
    m_autoFocusContinousQ.sendCmd(WAKE_UP);
    m_autoFocusContinousThread->requestExitAndWait();
    m_autoFocusContinousQ.release();

    m_facedetectThread->stop();
    m_facedetectQ->sendCmd(WAKE_UP);
    m_facedetectThread->requestExitAndWait();
    m_facedetectQ->release();

#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthCallbackBuffer;

    m_previewFrameFactory->setRequest(PIPE_VC1, false);
    m_parameters->setDepthCallbackOnCapture(false);
    m_parameters->setDepthCallbackOnPreview(false);
    //m_parameters->setDisparityMode(COMPANION_DISPARITY_OFF); /* temp*/
    CLOGI("[Depth] Disable DepthMap");

    while (m_depthCallbackQ->getSizeOfProcessQ() > 0) {
        m_depthCallbackQ->popProcessQ(&depthCallbackBuffer);

        CLOGD("put remaining depthCallbackBuffer buffer(index: %d)",
                depthCallbackBuffer.index);
        m_putBuffers(m_depthMapBufferMgr, depthCallbackBuffer.index);
    }
#endif

    if (m_previewQ != NULL) {
         m_clearList(m_previewQ);
    }

    if (m_gdcQ != NULL) {
         m_gdcQ->release();
    }

    if (m_zoomPreviwWithCscQ != NULL) {
         m_zoomPreviwWithCscQ->release();
    }

    if (m_previewCallbackGscFrameDoneQ != NULL) {
        m_clearList(m_previewCallbackGscFrameDoneQ);
    }

    ret = m_stopPreviewInternal();
    if (ret < 0)
        CLOGE("m_stopPreviewInternal fail");

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        m_depthMapBufferMgr->deinit();
    }
#endif
#if defined(DEBUG_RAWDUMP)
    if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        if (m_fliteBufferMgr != NULL) {
            m_fliteBufferMgr->resetBuffers();
        }
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        m_ispBufferMgr->deinit();
#else
        m_ispBufferMgr->resetBuffers();
#endif
    }
    if (m_hwDisBufferMgr != NULL) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        m_hwDisBufferMgr->deinit();
#else
        m_hwDisBufferMgr->resetBuffers();
#endif
    }

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_yuvReprocessingBufferMgr != NULL) {
        m_yuvReprocessingBufferMgr->deinit();
    }
    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }

    if (m_yuvBufferMgr != NULL) {
        m_yuvBufferMgr->resetBuffers();
    }
    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }

    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->resetBuffers();
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
    if (m_bayerCaptureSelector != NULL) {
        m_bayerCaptureSelector->release();
    }
    if (m_yuvCaptureSelector != NULL) {
        m_yuvCaptureSelector->release();
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
    m_dynamicBayerCount = 0;

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    m_isTryStopFlash= false;
    m_exitAutoFocusThread = false;
    m_isNeedAllocPictureBuffer = false;

    m_burstInitFirst = false;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /*
     * HACK
     * This is force-reset frameFactory adn companion
     */
    m_deinitFrameFactory();

    /*
     * close companion : other node all closed in m_deinitFrameFactory()
     * so, we close also companion
     */
    if (m_stopCompanion() != NO_ERROR)
        CLOGE("m_stopCompanion() fail");

    m_initFrameFactory();
#endif

    property_set("persist.sys.camera.preview","0");
}

void ExynosCamera::stopPreviewVision()
{
    CLOGI(" ");
    char previewMode[100] = {0};
    int ret = 0;
    int pipeId = PIPE_FLITE;

    property_get("persist.sys.camera.preview", previewMode, "");

    if (!strstr(previewMode, "0"))
        CLOGI(" persist.sys.camera.preview(%s)", previewMode);

    if (m_previewEnabled == false) {
        CLOGD(" preview is not enabled");
        property_set("persist.sys.camera.preview","0");
        return;
    }

    m_mainSetupQThread[INDEX(pipeId)]->stop();
    m_mainSetupQ[INDEX(pipeId)]->sendCmd(WAKE_UP);
    m_mainSetupQThread[INDEX(pipeId)]->requestExitAndWait();

    m_visionFrameFactory->setStopFlag();
    m_pipeFrameVisionDoneQ->sendCmd(WAKE_UP);
    m_visionThread->requestExitAndWait();

    ret = m_stopVisionInternal();
    if (ret < 0)
        CLOGE("m_stopVisionInternal fail");

    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }

    /* frame manager stop */
    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();

    /* HACK Reset Preview Flag*/
    m_resetPreview = false;

    property_set("persist.sys.camera.preview","0");
}

status_t ExynosCamera::startRecording()
{
    CLOGI("");

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" / does not support");
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    flashMgr->setCaptureStatus(false);

    if (m_getRecordingEnabled() == true) {
        CLOGW("m_recordingEnabled equals true");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

#ifdef USE_FD_AE
    if (m_parameters != NULL) {
        if (m_parameters->getFaceDetectionMode() == false) {
            m_enableFaceDetection(false);
        } else {
            /* stay current fd mode */
        }
    } else {
        CLOGW("m_parameters is NULL");
        return INVALID_OPERATION;
    }
#endif

    /* Do start recording process */
    ret = m_startRecordingInternal();
    if (ret < 0) {
        CLOGE("");
        return ret;
    }

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_setRecordingEnabled(true);
    m_parameters->setRecordingRunning(true);

    autoFocusMgr->setRecordingHint(true);
    flashMgr->setRecordingHint(true);
    if (m_parameters->doCscRecording() == true) {
        bool useMCSC2Pipe = true;

        if (m_parameters->getNumOfMcscOutputPorts() < 3) {
            useMCSC2Pipe = false;
        }

        if (useMCSC2Pipe == true)
            m_previewFrameFactory->setRequest(PIPE_MCSC2, true);
    }

func_exit:
    /* wait for initial preview skip */
    if (m_parameters != NULL) {
        int retry = 0;
        while (m_parameters->getFrameSkipCount() > 0 && retry < 3) {
            retry++;
            usleep(33000);
            CLOGI(" -OUT- (frameSkipCount:%d) (retry:%d)", m_frameSkipCount, retry);
        }
    }

    return NO_ERROR;
}

void ExynosCamera::stopRecording()
{
    CLOGI("");

    if (m_parameters == NULL) {
        CLOGE("m_parameters is NULL!");
        return;
    }

    if (m_parameters->getVisionMode() == true) {
        CLOGW(" Vision mode does not support");
        android_printAssert(NULL, LOG_TAG, "Cannot support this operation");
        return;
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();

    if (m_getRecordingEnabled() == false) {
        return;
    }

    m_setRecordingEnabled(false);
    m_parameters->setRecordingRunning(false);

    if (m_parameters->doCscRecording() == true)
        m_previewFrameFactory->setRequest(PIPE_MCSC2, false);

    /* Do stop recording process */
    ret = m_stopRecordingInternal();
    if (ret < 0)
        CLOGE("m_stopRecordingInternal fail");

#ifdef USE_FD_AE
    if (m_parameters != NULL) {
        m_enableFaceDetection(m_parameters->getFaceDetectionMode(m_parameters->getRecordingHint()));
    }
#endif

    autoFocusMgr->setRecordingHint(false);
    flashMgr->setRecordingHint(false);
    flashMgr->setNeedFlashOffDelay(false);
}

status_t ExynosCamera::takePicture()
{
    int ret = 0;
    int takePictureCount = m_takePictureCounter.getCount();
    int seriesShotCount = 0;
    int currentSeriesShotMode = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    enum PROCESSING_MODE reprocessingMode = PROCESSING_MODE_BASE;
    int retryCount = 0;
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

    if (m_previewEnabled == false) {
        CLOGE("preview is stopped, return error");
        return INVALID_OPERATION;
    }

    if (m_parameters == NULL) {
        CLOGE("m_parameters is NULL");
        return INVALID_OPERATION;
    }

    while (m_longExposureCaptureState != LONG_EXPOSURE_PREVIEW) {
        m_parameters->setPreviewExposureTime();

        uint64_t delay = 0;
        if (m_parameters->getCaptureExposureTime() <= 33333)
            delay = 30000;
        else if (m_parameters->getCaptureExposureTime() > CAMERA_EXPOSURE_TIME_MAX)
            delay = CAMERA_EXPOSURE_TIME_MAX;
        else
            delay = m_parameters->getCaptureExposureTime();

        usleep(delay);

        if (++retryCount > 7) {
            CLOGE("HAL can't set preview exposure time.");
            return INVALID_OPERATION;
        }
    }

    retryCount = 0;

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

    seriesShotCount = m_parameters->getSeriesShotCount();
    currentSeriesShotMode = m_parameters->getSeriesShotMode();
    reprocessingMode = m_parameters->getReprocessingMode();
    if (m_parameters->getVisionMode() == true) {
        CLOGW(" Vision mode does not support");
        android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

        return INVALID_OPERATION;
    }

    if (m_parameters->getShotMode() == SHOT_MODE_RICH_TONE ||
        m_parameters->getHdrMode() == true) {
        m_hdrEnabled = true;
        m_parameters->setOutPutFormatNV21Enable(true);
    } else {
        m_hdrEnabled = false;
    }

    flashMgr->setCaptureStatus(true);

    m_parameters->setMarkingOfExifFlash(0);

    /* HACK Reset Preview Flag*/
    while ((m_resetPreview == true) && (retryCount < 10)) {
        usleep(200000);
        retryCount ++;
        CLOGI(" retryCount(%d) m_resetPreview(%d)", retryCount, m_resetPreview);
    }

    if (takePictureCount < 0) {
        CLOGE(" takePicture is called too much. takePictureCount(%d) / seriesShotCount(%d) . so, fail",
             takePictureCount, seriesShotCount);
        return INVALID_OPERATION;
    } else if (takePictureCount == 0) {
        if (seriesShotCount == 0) {
            m_captureLock.lock();
            if (m_pictureEnabled == true) {
                CLOGE(" take picture is inprogressing");
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

    CLOGI("takePicture start m_takePictureCounter(%d), currentSeriesShotMode(%d) seriesShotCount(%d) reprocessingMode(%d)",
         m_takePictureCounter.getCount(), currentSeriesShotMode, seriesShotCount,
         reprocessingMode);

    m_printExynosCameraInfo(__FUNCTION__);

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (m_parameters->isDynamicCapture() == true) {
        int pipeId = m_getBayerPipeId();
        int bufPipeId = -1;
        enum NODE_TYPE dstNodeType = INVALID_NODE;

        switch (reprocessingMode) {
        case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
            bufPipeId = PIPE_VC0;
            break;
        case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
            bufPipeId = PIPE_3AC;
            break;
        case PROCESSING_MODE_NON_REPROCESSING_YUV:
            if (m_parameters->is3aaIspOtf() == true)
                pipeId = PIPE_3AA;
            else
                pipeId = PIPE_ISP;

            bufPipeId = PIPE_MCSC1;
            break;
        default:
            CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
            break;
        }

        dstNodeType = m_previewFrameFactory->getNodeType(bufPipeId);
        m_captureSelector->clearList(pipeId, false, dstNodeType);

        m_previewFrameFactory->setRequest(bufPipeId, true);
    }

#ifdef DEBUG_RAWDUMP
    if (reprocessingMode != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        int buffercount = 1;
        m_dynamicBayerCount += buffercount;

        m_previewFrameFactory->setRequest(PIPE_VC0, true);
    }
#endif

    if (m_takePictureCounter.getCount() == seriesShotCount) {
        m_stopBurstShot = false;
#ifdef BURST_CAPTURE
        m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif

        m_captureSelector->setIsFirstFrame(true);

        if (m_parameters->getScalableSensorMode()) {
            m_parameters->setScalableSensorMode(false);
            stopPreview();
            setPreviewWindow(m_previewWindow);
            startPreview();
            m_parameters->setScalableSensorMode(true);
        }

        CLOGI(" takePicture enabled, takePictureCount(%d)",
                 m_takePictureCounter.getCount());
        m_pictureEnabled = true;
        m_takePictureCounter.decCount();
        m_isNeedAllocPictureBuffer = true;

        m_startPictureBufferThread->join();

        if (m_parameters->isReprocessingCapture() == true) {
            m_startPictureInternalThread->join();

#ifdef BURST_CAPTURE
            if (seriesShotCount > 1)
            {
                int allocCount = 0;
                int addCount = 0;
                CLOGD(" realloc buffer for burst shot");
                m_burstRealloc = false;

                if (m_parameters->isUseHWFC() == false) {
                    allocCount = m_yuvReprocessingBufferMgr->getAllocatedBufferCount();
                    addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                    if( addCount > 0 ){
                        m_yuvReprocessingBufferMgr->increase(addCount);
                    }
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

        CLOGD(" currentSeriesShotMode(%d), flashMgr->getNeedCaptureFlash(%d)",
             currentSeriesShotMode, flashMgr->getNeedCaptureFlash());

#ifdef USE_ODC_CAPTURE
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
            || (m_parameters->getSeriesShotCount() > 1)
        ) {
            m_parameters->setODCCaptureEnable(false);
        } else {
            m_parameters->setODCCaptureEnable(m_parameters->getODCCaptureMode());
        }
#endif

        if (m_parameters->getCaptureExposureTime() != 0) {
            m_longExposureRemainCount = m_parameters->getLongExposureShotCount();
            CLOGD(" m_longExposureRemainCount(%d)",
                m_longExposureRemainCount);
        }

        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            sCaptureMgr->resetHdrStartFcount();
            m_parameters->setFrameSkipCount(13);
        } else if (flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            if (m_parameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                flashMgr->setManualExposureTime(m_parameters->getLongExposureTime() * 1000);
            }

            m_parameters->setMarkingOfExifFlash(1);

            {
                if (flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                    CLOGD(" checkPreFlash(false), Start auto focus internally");
                    m_autoFocusType = AUTO_FOCUS_HAL;
                    flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);
                    flashMgr->setFlashWaitCancel(false);

                    /* execute autoFocus for preFlash */
                    m_autoFocusThread->requestExitAndWait();
                    m_autoFocusThread->run(PRIORITY_DEFAULT);
                }
            }
        }
        else {
            {
                if(m_parameters->getCaptureExposureTime() > 0) {
                    m_stopLongExposure = false;
                    m_parameters->setExposureTime();
                }
            }
        }

        m_parameters->setSetfileYuvRange();

        if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21) {
            m_parameters->setOutPutFormatNV21Enable(true);
        }

        {
            {
                m_reprocessingCounter.setCount(seriesShotCount);
            }
        }

        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                CLOGE("couldn't run pre-picture thread");
                return INVALID_OPERATION;
            }
        }

        {
            m_jpegCounter.setCount(seriesShotCount);
            m_pictureCounter.setCount(seriesShotCount);
            m_yuvcallbackCounter.setCount(seriesShotCount);
        }

        if (m_parameters->getCaptureExposureTime() <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
            if (m_pictureThread->isRunning() == false)
                ret = m_pictureThread->run();
            if (ret < 0) {
                CLOGE("couldn't run picture thread, ret(%d)", ret);
                return INVALID_OPERATION;
            }
        }

        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21 ||
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            m_yuvCallbackThread->join();
            m_yuvCallbackThread->run();
        } else
        {
            m_jpegCallbackThread->join();
            if (m_parameters->getCaptureExposureTime() <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                ret = m_jpegCallbackThread->run();
                if (ret < 0) {
                    CLOGE("couldn't run jpeg callback thread, ret(%d)", ret);
                    return INVALID_OPERATION;
                }
            }
        }
    } else {
        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        /* One second burst capture launching jpegCallbackThread automatically */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21 ||
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            m_yuvCallbackThread->join();
            m_yuvCallbackThread->run();
        } else
        {
            /* series shot : push buffer to callback thread. */
            m_jpegCallbackThread->join();
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("couldn't run jpeg callback thread, ret(%d)", ret);
                return INVALID_OPERATION;
            }
        }

        CLOGI(" series shot takePicture, takePictureCount(%d)",
                 m_takePictureCounter.getCount());
        m_takePictureCounter.decCount();

        /* TODO : in case of no reprocesssing, we make zsl scheme or increse buf */
        if (m_parameters->isReprocessingCapture() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera::cancelPicture()
{
    CLOGI("");

    if (m_parameters == NULL) {
        CLOGE("m_parameters is NULL");
        return NO_ERROR;
    }

    if (m_parameters->getVisionMode() == true) {
        CLOGW(" Vision mode does not support");
        /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

        return NO_ERROR;
    }

    if (m_parameters->getLongExposureShotCount() > 0) {
        CLOGD("emergency stop for manual exposure shot");
        m_cancelPicture = true;
        m_parameters->setPreviewExposureTime();
        m_pictureEnabled = false;
        m_stopLongExposure = true;
        m_reprocessingCounter.clearCount();
        m_captureSelector->cancelPicture();
        m_terminatePictureThreads(false);
        m_cancelPicture = false;
        m_captureSelector->cancelPicture(false);
    }

/*
    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
*/

    return NO_ERROR;
}

status_t ExynosCamera::cancelAutoFocus()
{
    CLOGI("");

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" Vision mode does not support");
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#if 0 // not used.
    if (m_parameters != NULL) {
        if (m_parameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI(" ae awb unlock");
            m_parameters->m_setAutoExposureLock(false);
            m_parameters->m_setAutoWhiteBalanceLock(false);
        }
    }
#endif

    if (m_exynosCameraActivityControl->cancelAutoFocus() == false) {
        CLOGE("Fail on m_secCamera->cancelAutoFocus()");
        return UNKNOWN_ERROR;
    }

    /* if autofocusThread is running, we should be wait to receive the AF reseult. */
    m_autoFocusLock.lock();
    m_autoFocusLock.unlock();

    return NO_ERROR;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;
	bool flagRestart = false;

    CLOGI("");

    if(m_parameters == NULL)
        return INVALID_OPERATION;

#ifdef SCALABLE_ON
    m_parameters->setScalableSensorMode(true);
#else
    m_parameters->setScalableSensorMode(false);
#endif

    flagRestart = m_parameters->checkRestartPreview(params);

    /* HACK Reset Preview Flag*/
    if (flagRestart == true && m_previewEnabled == true) {
        m_resetPreview = true;
        if (m_resetPreview == true
        ) {
            ret = m_restartPreviewInternal(true, (CameraParameters*)&params);
        }

        m_resetPreview = false;
        CLOGI(" m_resetPreview(%d)", m_resetPreview);

        if (ret < 0)
            CLOGE(" restart preview internal fail");
    } else {
        ret = m_parameters->setParameters(params);
    }

    return ret;
}

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, __unused int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;
    CLOGV(" ");

    if (m_parameters != NULL) {
    } else {
        CLOGE("m_parameters is NULL");
        return INVALID_OPERATION;
    }

    /* TO DO : implemented based on the command */
    switch(command) {
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        if (m_parameters->getMaxNumDetectedFaces() == 0) {
            CLOGE("getMaxNumDetectedFaces == 0");
            return BAD_VALUE;
        }

        if (arg1 == CAMERA_FACE_DETECTION_SW) {
            CLOGD("only support HW face dectection");
            return BAD_VALUE;
        }

        if (command == CAMERA_CMD_START_FACE_DETECTION) {
            CLOGD("CAMERA_CMD_START_FACE_DETECTION is called!");
            if (m_flagStartFaceDetection == false
                && m_startFaceDetection() == false) {
                CLOGE("startFaceDetection() fail");
                return BAD_VALUE;
            }
        } else {
            CLOGD("CAMERA_CMD_STOP_FACE_DETECTION is called!");
            if ( m_flagStartFaceDetection == true
                && m_stopFaceDetection() == false) {
                CLOGE("stopFaceDetection() fail");
                return BAD_VALUE;
            }
        }
        break;
#ifdef BURST_CAPTURE
    case 1571: /* HAL_CMD_RUN_BURST_TAKE */
        CLOGD("HAL_CMD_RUN_BURST_TAKE is called!");

        if( m_burstInitFirst ) {
            m_burstRealloc = true;
            m_burstInitFirst = false;
        }
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_BURST);
        m_parameters->setSeriesShotSaveLocation(arg1);
        m_takePictureCounter.setCount(0);

        m_burstCaptureCallbackCount = 0;

        {
            // Check jpeg save path
            char default_path[20];
            int SeriesSaveLocation = m_parameters->getSeriesShotSaveLocation();

            memset(default_path, 0, sizeof(default_path));

            if (SeriesSaveLocation == BURST_SAVE_PHONE)
                strncpy(default_path, CAMERA_SAVE_PATH_PHONE, sizeof(default_path)-1);
            else if (SeriesSaveLocation == BURST_SAVE_SDCARD)
                strncpy(default_path, CAMERA_SAVE_PATH_EXT, sizeof(default_path)-1);

            m_checkCameraSavingPath(default_path,
                    m_parameters->getSeriesShotFilePath(),
                    m_burstSavePath, sizeof(m_burstSavePath));
        }

#ifdef USE_DVFS_LOCK
        m_parameters->setDvfsLock(true);
#endif
        break;
    case 1572: /* HAL_CMD_STOP_BURST_TAKE */
        CLOGD("HAL_CMD_STOP_BURST_TAKE is called!");
        m_takePictureCounter.setCount(0);

        if (m_parameters->getSeriesShotCount() == MAX_SERIES_SHOT_COUNT) {
             m_reprocessingCounter.clearCount();
             m_pictureCounter.clearCount();
             m_jpegCounter.clearCount();
        }

        m_stopBurstShot = true;

        m_captureSelector->cancelPicture();
        m_terminatePictureThreads(false);
        m_captureSelector->cancelPicture(false);

        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

#ifdef USE_DVFS_LOCK
        if (m_parameters->getDvfsLock() == true)
            m_parameters->setDvfsLock(false);
#endif
        break;
    case 1573: /* HAL_CMD_DELETE_BURST_TAKE */
        CLOGD("HAL_CMD_DELETE_BURST_TAKE is called!");
        m_takePictureCounter.setCount(0);
        break;
#ifdef VARIABLE_BURST_FPS
    case 1575: /* HAL_CMD_SET_BURST_FPS */
        CLOGD("HAL_CMD_SET_BURST_FPS is called! / FPS(%d)", arg1);
        m_parameters->setBurstShotDuration(arg1);
        break;
#endif /* VARIABLE_BURST_FPS */
#endif
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        CLOGD("CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!%d", arg1);
        if(arg1) {
            if( m_flagLLSStart != true) {
                m_flagLLSStart = true;
            }
        } else {
            m_flagLLSStart = false;
        }
        break;
    case 1801: /* HAL_ENABLE_LIGHT_CONDITION*/
        CLOGD("HAL_ENABLE_LIGHT_CONDITION is called!%d", arg1);
        if(arg1) {
            m_flagLightCondition = true;
        } else {
            m_flagLightCondition = false;
        }
        break;
    case 1431: /* CAMERA_CMD_START_MOTION_PHOTO */
        CLOGD("CAMERA_CMD_START_MOTION_PHOTO is called!");
        m_parameters->setMotionPhotoOn(true);
        break;
    case 1432: /* CAMERA_CMD_STOP_MOTION_PHOTO */
        CLOGD("CAMERA_CMD_STOP_MOTION_PHOTO is called!)");
        m_parameters->setMotionPhotoOn(false);
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        CLOGD("CAMERA_CMD_SET_FLIP is called!%d", arg1);
        m_parameters->setFlipHorizontal(arg1);
        break;
    /*1641: CAMERA_CMD_ADVANCED_MACRO_FOCUS*/
    case 1641:
        CLOGD("CAMERA_CMD_ADVANCED_MACRO_FOCUS is called!%d", arg1);
        m_parameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER);
        break;
    /*1642: CAMERA_CMD_FOCUS_LOCATION*/
    case 1642:
        CLOGD("CAMERA_CMD_FOCUS_LOCATION is called!%d", arg1);
        m_parameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER_UP);
        break;
    /*1643: HAL_ENABLE_CURRENT_SET */
    case 1643:
        CLOGD("HAL_ENABLE_CURRENT_SET is called!%d", arg1);
        if(arg1)
            m_startCurrentSet();
        else
            m_stopCurrentSet();
        break;
    /*1661: CAMERA_CMD_START_ZOOM */
    case 1661:
        CLOGD("CAMERA_CMD_START_ZOOM is called!");
        m_parameters->setZoomActiveOn(true);
        m_parameters->setFocusModeLock(true);
        break;
    /*1662: CAMERA_CMD_STOP_ZOOM */
    case 1662:
        CLOGD("CAMERA_CMD_STOP_ZOOM is called!");
        m_parameters->setZoomActiveOn(false);
        m_parameters->setFocusModeLock(false);
        break;
    case 1821: /* SAMSUNG NATIVE APP Based on SecCamera*/
        CLOGD(" SAMSUNG NATIVE APP ON!! (%d/%d)", arg1, arg2);
        break;
    case 1806: /* HAL_ENABLE_BRIGHTNESS_VALUE_CALLBACK*/
        CLOGD(" HAL_ENABLE_BRIGHTNESS_VALUE_CALLBACK is called!(%d)", arg1);
        if(arg1) {
            m_flagBrightnessValue = true;
        } else {
            m_flagBrightnessValue = false;
        }
        break;
    default:
        CLOGV("unexpectect command(%d)", command);
        break;
    }

    return NO_ERROR;
}

void ExynosCamera::m_handleFaceDetectionFrame(ExynosCameraFrameSP_sptr_t previewFrame, ExynosCameraBuffer *previewBuffer)
{
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    bool skipFdMetaCallback = false;
    ExynosCameraFrameSP_sptr_t fdFrame = NULL;

    /* Face detection */
    if (!(m_parameters->getHighSpeedRecording() == true && m_getRecordingEnabled() == true)
            && (previewFrame->getFrameState() != FRAME_STATE_SKIPPED)) {

        if (m_parameters->getFrameSkipCount() <= 0
                && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
            previewFrame->getFpsRange(&minFps, &maxFps);

            /* limit maximum FD callback fps to 60fps */
            if (maxFps > 60) {
                ratio = (int)((maxFps * 10 / dispFps) / previewBuffer->batchSize / 10);
                m_fdCallbackToggle = (m_fdCallbackToggle + 1) % ratio;
                skipFdMetaCallback = (m_fdCallbackToggle == 0) ? false : true;
            }

            if (!skipFdMetaCallback) {
#ifdef SUPPORT_DEPTH_MAP
                int pipe = PIPE_VC1;
                fdFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipe, previewFrame->getFrameCount());
#else
                fdFrame = m_frameMgr->createFrame(m_parameters, previewFrame->getFrameCount());
#endif
                m_copyMetaFrameToFrame(previewFrame, fdFrame, true, true);
                camera2_node_group node_group_info;
                previewFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_FLITE);
                fdFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_FLITE);

                m_facedetectQ->pushProcessQ(&fdFrame);
            }
        }
    }
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraBuffer buffer;
    ExynosCameraBuffer previewcallbackbuffer;
    ExynosCameraBuffer t3acBuffer;
    int pipeId = -1;
    int bufPipeId = -1;
    enum NODE_TYPE bufPipeNodeType = INVALID_NODE;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();
    int ispDstBufferIndex = -1;

    uint32_t frameCnt = 0;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("current entity is NULL, HAL-frameCount(%d)",
                 frame->getFrameCount());
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    pipeId = entity->getPipeId();

    m_debugFpsCheck(pipeId);

    /* TODO: remove hard coding */
    switch (pipeId) {
    case PIPE_FLITE:
        bufPipeId = PIPE_VC0;
        bufPipeNodeType = m_previewFrameFactory->getNodeType(bufPipeId);
        ret = frame->getDstBuffer(pipeId, &buffer, bufPipeNodeType);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
            return ret;
        }

        if (m_parameters->isFlite3aaOtf() == false) {
            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR)
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         PIPE_3AA, ret);

            m_previewFrameFactory->pushFrameToPipe(frame, PIPE_3AA);
        }

#ifdef DEBUG_RAWDUMP
        if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            m_previewFrameFactory->stopThread(pipeId);

            if (buffer.index >= 0) {
                if (m_parameters->checkBayerDumpEnable()) {
                    int sensorMaxW, sensorMaxH;
                    int sensorMarginW, sensorMarginH;
                    bool bRet;
                    char filePath[70];

                    memset(filePath, 0, sizeof(filePath));
                    snprintf(filePath, sizeof(filePath), "/data/dump/PureRawCapture%d_%d.raw", m_cameraId, frame->getFrameCount());
                    m_parameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);

                    CLOGE(" Sensor (%d x %d) frame(%d)", \
                             sensorMaxW, sensorMaxH, frame->getFrameCount());

                    bRet = dumpToFile((char *)filePath,
                        buffer.addr[0],
                        buffer.size[0]);
                    if (bRet != true)
                        CLOGE("couldn't make a raw file");
                }

                ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("put Buffer fail");
                }
            }
        }
#endif

        CLOGV("FLITE driver-frameCount(%d) HAL-frameCount(%d)",
                getMetaDmRequestFrameCount((camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                frame->getFrameCount());
        break;

    case PIPE_3AA:
        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        /* Trigger CAF thread */
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
            frameCnt = frame->getFrameCount();
            m_autoFocusContinousQ.pushProcessQ(&frameCnt);
        }

        frame->setMetaDataEnable(true);

        bufPipeId = PIPE_VC0;
        bufPipeNodeType = m_previewFrameFactory->getNodeType(bufPipeId);

        /* Return src buffer to buffer manager */
        if (m_parameters->isFlite3aaOtf() == false) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
                    && entity->getSrcBufState() == ENTITY_BUFFER_STATE_COMPLETE) {
                    /*
                     * In case of CSIS(FLITE) -> 3AA M2M,
                     * it should use pure bayer processed by 3AA for capture
                     */
                    {
                        ret = m_captureSelector->manageFrameHoldList(frame, PIPE_FLITE, false, bufPipeNodeType);
                    }
                    if (ret != NO_ERROR)
                        CLOGE("manageFrameHoldList fail");
                } else {
                    /*
                     * In case of CSIS(FLITE) -> 3AA M2M,
                     * Return Src buffer here.
                     * But in case of CSIS(FLITE) -> 3AA OTF,
                     * Return Src buffer in Setup3AA function.
                     */
                    ExynosCameraBufferManager *bufferMgr = NULL;
                    ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
                    if (ret != NO_ERROR) {
                        CLOGE("m_getBufferManager(pipeId(%d), SRC_BUFFER_DIRECTION) fail", pipeId);
                    } else {
                        CLOGW("m_putBuffer the bayer buffer(%d), fCount(%d)",
                                buffer.index, frame->getFrameCount());

                        ret = m_putBuffers(bufferMgr, buffer.index);
                        if (ret != NO_ERROR)
                            CLOGE("m_putBuffers(index:%d) fail", buffer.index);
                    }
                }
            } else {
                CLOGE("3AA Src buffer Drop HAL-frameCount(%d)'s buffer.index(%d) < 0, mode(%d)",
                        frame->getFrameCount(), buffer.index, reprocessingMode);
            }
        } else {
            if (frame->getRequest(bufPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, bufPipeNodeType);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
                        && entity->getDstBufState(bufPipeNodeType) == ENTITY_BUFFER_STATE_COMPLETE) {
                        {
                            ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false, bufPipeNodeType);
                        }
                        if (ret != NO_ERROR) {
                            CLOGE("manageFrameHoldList fail");
                            return ret;
                        }

                        CLOGV("FLITE driver-frameCount(%d) HAL-frameCount(%d)",
                                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                                frame->getFrameCount());
                    } else
                    {
                        /* Return buffer when buffer is invalid or not pure bayer reprocessing mode */
                        ExynosCameraBufferManager *bufferMgr = NULL;
                        ret = m_getBufferManager(bufPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                        if (ret != NO_ERROR) {
                            CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", bufPipeId);
                        } else {
                            CLOGW("m_putBuffer the bayer buffer(%d), fCount(%d)",
                                    buffer.index, frame->getFrameCount());

                            ret = m_putBuffers(bufferMgr, buffer.index);
                            if (ret != NO_ERROR)
                                CLOGE("m_putBuffers(index:%d) fail", buffer.index);
                        }

                        CLOGW("Skip manageFrameHoldList(fCount:%d), pipeId(%d)'s PIPE_VC0",
                                frame->getFrameCount(), pipeId);
                    }
                } else {
                    CLOGW("FLITE Drop HAL-frameCount(%d)'s buffer.index(%d) < 0, mode(%d)",
                            frame->getFrameCount(), buffer.index, reprocessingMode);
                }
            } else {
                CLOGV("FLITE Drop HAL-frameCount(%d), mode(%d)",
                        frame->getFrameCount(), reprocessingMode);

            }
        }

        CLOGV("3AA driver-frameCount(%d) HAL-frameCount(%d)",
                getMetaDmRequestFrameCount((camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                frame->getFrameCount());

        bufPipeId = PIPE_3AC;
        bufPipeNodeType = m_previewFrameFactory->getNodeType(bufPipeId);

        if (frame->getRequest(bufPipeId) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, bufPipeNodeType);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
            }

            if (buffer.index >= 0) {
                if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
                    && entity->getDstBufState(bufPipeNodeType) == ENTITY_BUFFER_STATE_COMPLETE) {
                    {
                        ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false, bufPipeNodeType);
                    }
                    if (ret != NO_ERROR) {
                        CLOGE("manageFrameHoldList fail");
                        return ret;
                    }
                } else {
                    ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("m_putBuffers(m_bayerBufferMgr, index:%d) fail", buffer.index);
                        break;
                    }
                }
            }
        }

        /* check lux for smooth enterance */
        memset(m_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
        frame->getDynamicMeta(m_meta_shot);
        frame->getUserDynamicMeta(m_meta_shot);
        m_checkEntranceLux(m_meta_shot);

        if (m_parameters->is3aaIspOtf() == false)
            break;
    case PIPE_ISP:
        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        if (pipeId == PIPE_ISP) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("put Buffer fail");
                    break;
                }
            }
        }

        CLOGV("ISP driver-frameCount(%d) HAL-frameCount(%d)",
                getMetaDmRequestFrameCount((camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                frame->getFrameCount());

        if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false) {
            break;
        }
    case PIPE_TPU:
        if (pipeId == PIPE_TPU) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                     pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("m_putBuffers(m_hwDisBufferMgr, %d) fail", buffer.index);
                }
            }
        }

        CLOGV("TPU driver-frameCount(%d) HAL-frameCount(%d)",
                getMetaDmRequestFrameCount((camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                frame->getFrameCount());

        /*
         * dis capture is scp.
         * so, skip break;
         */
    case PIPE_SCP:
    case PIPE_MCSC:
        if (pipeId == PIPE_MCSC) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                     pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("m_putBuffers(m_hwDisBufferMgr, %d) fail", buffer.index);
                }
            }
        }

        /* Return capture node buffers & complete(error) next entities */
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("Src buffer state error, pipeId(%d)", pipeId);

            /* Preview : PIPE_MCSC0 */
            if (frame->getRequest(PIPE_MCSC0) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC0));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret != NO_ERROR)
                        CLOGE("SCP buffer return fail");
                }
            }

            /* Preview Callback : PIPE_MCSC1 */
            if (frame->getRequest(PIPE_MCSC1) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC1));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    /* Return buffer when buffer is invalid or not YUV mode */
                    ExynosCameraBufferManager *bufferMgr = NULL;
                    ret = m_getBufferManager(PIPE_MCSC1, &bufferMgr, DST_BUFFER_DIRECTION);
                    if (ret != NO_ERROR) {
                        CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_MCSC1);
                    } else {
                        ret = m_putBuffers(bufferMgr, buffer.index);
                        if (ret != NO_ERROR)
                            CLOGE("m_putBuffers(index:%d) fail", buffer.index);
                    }
                }
            }

            /* Recording : PIPE_MCSC2 */
            if (frame->getRequest(PIPE_MCSC2) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC2));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Recording buffer return fail");
                    }
                }
            }

            /* Set GDC complete */
            if (m_parameters->getGDCEnabledMode() == true) {
                ret = frame->setDstBufferState(PIPE_GDC, ENTITY_BUFFER_STATE_ERROR);
                if (ret != NO_ERROR) {
                    CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
                    return ret;
                }

                ret = frame->setEntityState(PIPE_GDC, ENTITY_STATE_COMPLETE);
                if (ret != NO_ERROR) {
                    CLOGE("set entity state fail, ret(%d)", ret);
                    return ret;
                }
            }

            /* Set GSC complete */
            ret = frame->setDstBufferState(PIPE_GSC, ENTITY_BUFFER_STATE_ERROR);
            if (ret != NO_ERROR) {
                CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
                return ret;
            }

            ret = frame->setEntityState(PIPE_GSC, ENTITY_STATE_COMPLETE);
            if (ret != NO_ERROR) {
                CLOGE("set entity state fail, ret(%d)", ret);
                return ret;
            }

            goto entity_state_complete;
        } else {
            int previewCallbackPipeId = PIPE_MCSC1;
            int recordingPipeId = PIPE_MCSC2;
            nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
            previewcallbackbuffer.index = -2;

            /* Preview callback : PIPE_MCSC1 */
            /* Capture YUV (Katmai) : PIPE_MCSC1 */
            bufPipeNodeType = m_previewFrameFactory->getNodeType(previewCallbackPipeId);

            if (frame->getRequest(previewCallbackPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, bufPipeNodeType);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            pipeId, ret);
                }

                if (buffer.index >= 0) {
                    bufferState = entity->getDstBufState(bufPipeNodeType);
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                    if (bufferState != ENTITY_BUFFER_STATE_COMPLETE) {
                        CLOGW("Preview callback handling droped \
                                - preview callback buffer is not ready HAL-frameCount(%d)",
                                frame->getFrameCount());
#else
                    if (reprocessingMode == PROCESSING_MODE_NON_REPROCESSING_YUV
                        && entity->getDstBufState(bufPipeNodeType) == ENTITY_BUFFER_STATE_COMPLETE) {
                        ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false, bufPipeNodeType);
                        if (ret != NO_ERROR) {
                            CLOGE("manageFrameHoldList fail");
                            return ret;
                        }
                    } else {
                        CLOGW("Skip manageFrameHoldList(fCount:%d), pipeId(%d)'s PIPE_MCSC1",
                                frame->getFrameCount(), pipeId);
#endif
                        /* Return buffer when buffer is invalid or not YUV mode */
                        ExynosCameraBufferManager *bufferMgr = NULL;
                        ret = m_getBufferManager(previewCallbackPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                        if (ret != NO_ERROR) {
                            CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", previewCallbackPipeId);
                        } else {
                            CLOGW("m_putBuffer the MCSC1 buffer(%d), fCount(%d)",
                                    buffer.index, frame->getFrameCount());

                            ret = m_putBuffers(bufferMgr, buffer.index);
                            if (ret != NO_ERROR)
                                CLOGE("m_putBuffers(index:%d) fail", buffer.index);
                        }
                    }
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                    else {
                        int newPreviewCbPipeId = -1;

                        if (m_isZoomPreviewWithCscEnabled(pipeId, frame) == true) {
                            newPreviewCbPipeId = PIPE_GSC;
                        } else if (m_parameters->getGDCEnabledMode() == true) {
                            newPreviewCbPipeId = PIPE_GDC;
                        } else {
                            newPreviewCbPipeId = pipeId;
                        }

                        ret = frame->setDstBufferState(newPreviewCbPipeId, ENTITY_BUFFER_STATE_REQUESTED, bufPipeNodeType);
                        if (ret != NO_ERROR) {
                            CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
                            return INVALID_OPERATION;
                        }

                        ret = frame->setDstBuffer(newPreviewCbPipeId, buffer, bufPipeNodeType);
                        if (ret != NO_ERROR) {
                            CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
                            return INVALID_OPERATION;
                        }

                        ret = frame->setDstBufferState(newPreviewCbPipeId, ENTITY_BUFFER_STATE_COMPLETE, bufPipeNodeType);
                        if (ret != NO_ERROR) {
                            CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
                            return INVALID_OPERATION;
                        }
                    }
#endif
                }
            }

            /* Recording : PIPE_MCSC2 */
            bufPipeNodeType = m_previewFrameFactory->getNodeType(recordingPipeId);

            if (frame->getRequest(recordingPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, bufPipeNodeType);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(bufPipeNodeType);
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    if (m_frameSkipCount > 0) {
                        CLOGD("Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                 m_frameSkipCount, buffer.index);

                        if (buffer.index >= 0) {
                            ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("Recording buffer return fail");
                            }
                        }
                    } else {
                        if (m_getRecordingEnabled() == true
                            && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME) == true) {
                            if (timeStamp <= 0L) {
                                CLOGE("timeStamp(%lld) Skip", (long long)timeStamp);
                            } else {
                                ExynosCameraFrameSP_sptr_t recordingFrame = NULL;
                                recordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());

                                m_recordingTimeStamp[buffer.index] = timeStamp;
                                recordingFrame->setDstBuffer(recordingPipeId, buffer);

                                ret = m_insertFrameToList(&m_recordingProcessList, recordingFrame, &m_recordingProcessListLock);
                                if (ret != NO_ERROR) {
                                    CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                            recordingFrame->getFrameCount());
                                }

                                m_recordingQ->pushProcessQ(&recordingFrame);
                            }
                        } else {
                            if (buffer.index >= 0) {
                                ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                                if (ret != NO_ERROR) {
                                    CLOGE("Recording buffer return fail");
                                }
                            }
                        }
                    }

                    CLOGV("Recording handling done HAL-frameCount(%d)", frame->getFrameCount());
                } else {
                    CLOGD("Recording handling droped - Recording buffer is not ready HAL-frameCount(%d)",
                            frame->getFrameCount());

                    if (buffer.index >= 0) {
                        ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("Recording buffer return fail");
                        }
                    }

                    /* For debug */
                    /* m_previewFrameFactory->dump(); */
                }
            }
        }

        if (m_parameters->getGDCEnabledMode() == true) {
            // TODO : what about isSccCapture() == true ?
            CLOGV("gdc preview pipeId(%d) frame(%d)", entity->getPipeId(), frame->getFrameCount());

            m_gdcQ->pushProcessQ(&frame);
            break;
        }
        // no break;
    case PIPE_GDC:
        if (pipeId == PIPE_GDC) {
            ret = m_syncPreviewWithGDC(frame);
            if (ret != NO_ERROR) {
                CLOGW("m_syncPreviewWithGDC failed frame(%d)", frame->getFrameCount());

                /* Set GSC complete */
                ret = frame->setDstBufferState(PIPE_GSC, ENTITY_BUFFER_STATE_ERROR);
                if (ret != NO_ERROR) {
                    CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
                    return ret;
                }

                ret = frame->setEntityState(PIPE_GSC, ENTITY_STATE_COMPLETE);
                if (ret != NO_ERROR) {
                    CLOGE("set entity state fail, ret(%d)", ret);
                    return ret;
                }
                break;
            }

            entity = frame->searchEntityByPipeId(pipeId);
            if (entity == NULL) {
                CLOGE("searchEntityByPipeId failed pipe(%d) frame(%d)", pipeId, frame->getFrameCount());
            }
        }

        if (m_isZoomPreviewWithCscEnabled(pipeId, frame) == true) {
            CLOGV("zoom preview with csc pipeId(%d) frame(%d)", entity->getPipeId(), frame->getFrameCount());

            m_zoomPreviwWithCscQ->pushProcessQ(&frame);
            if (m_zoomPreviwWithCscThread->isRunning() == false) {
                m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
            }
            break;
        } else {
            CLOGV("zoom preview without csc pipeId(%d) frame(%d)", entity->getPipeId(), frame->getFrameCount());

            /* Set GSC complete */
            ret = frame->setDstBufferState(PIPE_GSC, ENTITY_BUFFER_STATE_ERROR);
            if (ret != NO_ERROR) {
                CLOGE("setdst Buffer failed(%d) frame(%d)", ret, frame->getFrameCount());
                return ret;
            }

            ret = frame->setEntityState(PIPE_GSC, ENTITY_STATE_COMPLETE);
            if (ret != NO_ERROR) {
                CLOGE("set entity state fail, ret(%d)", ret);
                return ret;
            }
        }
        // no break;
    case PIPE_GSC:
        if (pipeId == PIPE_GSC) {
            ret = m_syncPrviewWithCSC(pipeId, PIPE_GSC, frame);
            if (ret < 0) {
                CLOGW("m_syncPrviewWithCSC failed frame(%d)", frame->getFrameCount());
            }

            entity = frame->searchEntityByPipeId(pipeId);
            if (entity == NULL) {
                CLOGE("searchEntityByPipeId failed pipe(%d) frame(%d)", pipeId, frame->getFrameCount());
            }
        }

        {
            int previewPipeId = PIPE_MCSC0;
            int previewCallbackPipeId = PIPE_MCSC1;
            int recordingPipeId = PIPE_MCSC2;
            nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
            previewcallbackbuffer.index = -2;

            m_parameters->getFrameSkipCount(&m_frameSkipCount);

            if (m_fdFrameSkipCount > 0) {
                m_fdFrameSkipCount--;
            }

            /* fd detection */
            m_handleFaceDetectionFrame(frame, &buffer);

            ret = getYuvFormatInfo(m_parameters->getHwPreviewFormat(), &bpp, &planeCount);
            if (ret != NO_ERROR) {
                CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                        m_parameters->getHwPreviewFormat());
                return ret;
            }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
            /* Preview : PIPE_MCSC1 */
            if (frame->getRequest(previewCallbackPipeId) == true) {
                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    ret = frame->getDstBuffer(pipeId, &previewcallbackbuffer, m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                                entity->getPipeId(), ret);
                        return ret;
                    }
                }
            }
#endif

            /* Preview : PIPE_MCSC0 */
            if (frame->getRequest(previewPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(previewPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewPipeId));
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    /* drop preview frame, for preview stabilization */
                    if (0 < m_frameSkipCount) {
                        skipPreview = true;

                        CLOGD("Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                    m_frameSkipCount, buffer.index);
                    }

                    /* drop preview frame if lcd supported frame rate < scp frame rate */
                    frame->getFpsRange(&minFps, &maxFps);
                    if (skipPreview == false &&
                        dispFps < maxFps) {
                        ratio = (int)((maxFps * 10 / dispFps) / buffer.batchSize / 10);

#if 0
                        if (maxFps > 60)
                            ratio = ratio * 2;
#endif
                        m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                        skipPreview = (m_displayPreviewToggle == 0) ? false : true;
#ifdef DEBUG
                        CLOGE("preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, \
                                maxFps=%d, dispFps=%d, batchSize=%d, ratio=%d, skipPreview=%d)",
                                 frame->getFrameCount(), m_displayPreviewToggle,
                                maxFps, dispFps, buffer.batchSize, ratio, (int)skipPreview);
#endif
                    }

                    memset(m_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
                    frame->getDynamicMeta(m_meta_shot);
                    frame->getUserDynamicMeta(m_meta_shot);

                    m_checkEntranceLux(m_meta_shot);

                    /*
                     * [Code-flow 0]
                     *
                     * if (skipPreview == true &&  on multi-mcsc)
                     *   cancelBuffer;
                     * else
                     *   [Code-flow 1];
                     */
                    if (skipPreview == true &&
                        3 <= m_parameters->getNumOfMcscOutputPorts()) {

                        ExynosCameraBufferManager* bufferMgr = NULL;
                        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                        if (ret != NO_ERROR) {
                            CLOGE("Fail to get buffer manager from pipeId(%d)", pipeId);
                        }
                        ret = bufferMgr->cancelBuffer(buffer.index, true);
                        if (ret != NO_ERROR) {
                            CLOGE("cancelBuffer(buffer index(%d), pipeId(%d))",
                                buffer.index, pipeId);
                        }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                        if (previewcallbackbuffer.index >= 0) {
                            ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("Preview callback buffer return fail");
                            }
                        }
#endif
                    } else {
                        if (m_skipReprocessing == true)
                            m_skipReprocessing = false;

                        ExynosCameraFrameSP_sptr_t previewFrame = NULL;
                        previewFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(previewPipeId, frame->getFrameCount());
                        m_copyMetaFrameToFrame(frame, previewFrame, true, true);
                        previewFrame->setDstBuffer(previewPipeId, buffer);

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                        if (previewcallbackbuffer.index >= 0) {
                            previewFrame->setSrcBuffer(previewPipeId, previewcallbackbuffer);
                            previewFrame->setRequest(previewCallbackPipeId, true);
                        }
#endif

                        int previewBufferCount = m_parameters->getPreviewBufferCount();
                        int sizeOfPreviewQ = m_previewQ->getSizeOfProcessQ();
                        if (m_previewThread->isRunning() == true
                            && ((previewBufferCount == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS && sizeOfPreviewQ >= 2)
                                || (previewBufferCount == NUM_PREVIEW_BUFFERS && sizeOfPreviewQ >= 1))) {
                            if ((m_getRecordingEnabled() == true) && (m_parameters->doCscRecording() == false)) {
                                CLOGW("push frame to previewQ. PreviewQ(%d), PreviewBufferCount(%d)",
                                         m_previewQ->getSizeOfProcessQ(),
                                        m_parameters->getPreviewBufferCount());
                                        m_previewQ->pushProcessQ(&previewFrame);
                            } else {
                                CLOGW("Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                                        m_previewQ->getSizeOfProcessQ(),
                                        m_parameters->getPreviewBufferCount());

                                if (buffer.index >= 0) {
                                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                    if (ret != NO_ERROR)
                                        CLOGE("Preview buffer return fail");
                                }

                                if (previewcallbackbuffer.index >= 0) {
                                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("Preview callback buffer return fail");
                                    }
                                }
                                previewFrame = NULL;
                            }
                        } else {
                            /*
                             * [Code-flow 1]
                             *
                             * if (getRequest(recordingPipeId) == false)
                             *   [Code-flow 1-0] Recording;
                             *
                             * if (skipPreview == true)
                             *   [Code-flow 1-1] cancelBuffer;
                             * else if (fastFps on multi-mcsc)
                             *   [Code-flow 1-2] rendering(w/o any additional operation);
                             * else
                             *   [Code-flow 1-3] rendering;
                             */

                            {
                                /*
                                 * [Code-flow 1-0] Recording
                                 *
                                 * Recording frame (MCSC2 output is false)
                                 */
                                if (m_getRecordingEnabled() == true
                                    && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                    && frame->getRequest(recordingPipeId) == false) {
                                    if (m_parameters->doCscRecording() == false) {
                                        /* This case is Adaptive CSC */
                                        if (timeStamp <= 0L) {
                                            CLOGW("timeStamp(%lld) Skip", (long long)timeStamp);
                                        } else {
                                            ExynosCameraFrameSP_sptr_t adaptiveCscRecordingFrame = NULL;

                                            adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());

                                            m_recordingTimeStamp[buffer.index] = timeStamp;
                                            adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, buffer);

                                            ret = m_insertFrameToList(&m_recordingProcessList, adaptiveCscRecordingFrame, &m_recordingProcessListLock);
                                            if (ret != NO_ERROR) {
                                                CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                                        adaptiveCscRecordingFrame->getFrameCount());
                                            }

                                            m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                        }
                                    } else {
                                        /* Always do Csc Recording in case of dual camera */
                                        int bufIndex = -2;
                                        ExynosCameraBuffer recordingBuffer;

                                        ret = m_recordingBufferMgr->getBuffer(&bufIndex,
                                                EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                                        CLOGV("recordingBufferMgr->getBuffer(index %d) preview index(%d) timestamp(%lld)",
                                                bufIndex, buffer.index, timeStamp);
                                        if (ret < 0 || bufIndex < 0) {
                                            CLOGE(" Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                                    m_recordingFrameSkipCount, bufIndex);
                                            m_recordingBufferMgr->printBufferQState();
                                        } else {
                                            if (m_recordingFrameSkipCount != 0) {
                                                CLOGE(" Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                                        m_recordingFrameSkipCount, bufIndex,
                                                        m_recordingQ->getSizeOfProcessQ());
                                                m_recordingFrameSkipCount = 0;
                                                m_recordingBufferMgr->printBufferQState();
                                            }

                                            ret = m_doPreviewToRecordingFunc(PIPE_GSC_VIDEO, buffer, recordingBuffer, timeStamp);
                                            if (ret < 0 ) {
                                                CLOGW("recordingCallback Skip");
                                            }

                                            m_recordingTimeStamp[bufIndex] = timeStamp;
                                        }
                                    }
                                }
                            }

                            /*
                             * [Code-flow 1-1] cancelBuffer;
                             *
                             * when preview port & recording port is same,
                             * preview frame will skip after m_recordingQ->pushProcessQ.
                             */
                            if (skipPreview == true) {
                                if (buffer.index >= 0) {
                                    ExynosCameraBufferManager *bufferMgr = NULL;
                                    m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                                    if (bufferMgr == NULL) {
                                        CLOGE("m_getBufferManager(pipeId : %d, &bufferMgr, DST_BUFFER_DIRECTION) fail. so, can not cancelBuffer");
                                    } else {
                                        ret = bufferMgr->cancelBuffer(buffer.index, true);
                                        if (ret != NO_ERROR) {
                                            CLOGE("Preview buffer return fail, pipeId(%d), bufferMgr(%s), index(%d)",
                                                    pipeId, bufferMgr->getName(), buffer.index);
                                        } else {
                                            CLOGV("bufferMgr(%s)->cancelBuffer(%d) done", bufferMgr->getName(), buffer.index);
                                        }
                                    }
                                }


#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                                if (previewcallbackbuffer.index >= 0) {
                                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("Preview callback buffer return fail");
                                    }
                                }
#endif
                            } else if ((m_parameters->getFastFpsMode() > 1) &&
                                       (m_parameters->getRecordingHint() == 1) &&
                                        (3 <= m_parameters->getNumOfMcscOutputPorts())) {
                                /*
                                 * [Code-flow 1-2] rendering(w/o any additional operation);
                                 *
                                 * If it has many mcsc output port,
                                 * it will handle on m_recordingThreadFunc() with m_recordingQ
                                 */
                                CLOGV("push frame to previewQ");
                                m_previewQ->pushProcessQ(&previewFrame);
                            } else {
                                /* [Code-flow 1-3] rendering; */
                                if (m_getRecordingEnabled() == true) {
                                    CLOGV("push frame to previewQ");
                                    m_previewQ->pushProcessQ(&previewFrame);
                                    m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                } else if (m_parameters->getCaptureExposureTime() > 0
                                           && (m_meta_shot->shot.dm.sensor.exposureTime / 1000) > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                                    CLOGD("manual exposure capture mode. (%lld), fcount(%d)",
                                             (long long)m_meta_shot->shot.dm.sensor.exposureTime,
                                            m_meta_shot->shot.dm.request.frameCount);

                                    if (buffer.index >= 0) {
                                        /* only apply in the Full OTF of Exynos74xx. */
                                        ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                        if (ret != NO_ERROR)
                                            CLOGE("SCP buffer return fail");
                                    }

                                    if (previewcallbackbuffer.index >= 0) {
                                        ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                        if (ret != NO_ERROR) {
                                            CLOGE("Preview callback buffer return fail");
                                        }
                                    }

                                    previewFrame = NULL;
                                    m_longExposureCaptureState = LONG_EXPOSURE_CAPTURING;
                                } else {
                                    {
                                        CLOGV("push frame to previewQ");
                                        m_previewQ->pushProcessQ(&previewFrame);
                                    }

                                    if (m_stopLongExposure && m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        m_longExposureCaptureState = LONG_EXPOSURE_CANCEL_NOTI;
                                    } else {
                                        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                    }
                                }
                            }
                        }
                    }

                    CLOGV("Preview handling done HAL-frameCount(%d)", frame->getFrameCount());
                } else {
                    CLOGD("Preview handling droped(%d) - SCP buffer(%d) is not ready HAL-frameCount(%d)", frame->getFrameCount(), entity->getPipeId(), bufferState);

                    if (buffer.index >= 0) {
                        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("Preview buffer return fail");
                        }
                    }

                    if (previewcallbackbuffer.index >= 0) {
                        ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("Preview callback buffer return fail");
                        }
                    }

                    /* For debug */
                    /* m_previewFrameFactory->dump(); */
                }
            }
        }
        break;
    default:
        break;
    }

entity_state_complete:

    ret = frame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            pipeId, ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame, &m_processListLock);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }
    }

    return NO_ERROR;
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
    int previewCallbackPipeId = 0;
    int  maxbuffers   = 0;
    ExynosCameraBuffer scpBuffer;
    ExynosCameraBuffer previewCbBuffer;
    bool copybuffer = false;

    ExynosCameraFrameSP_sptr_t frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    frame_queue_t *previewQ;

    pipeId    = PIPE_SCP;
#if defined USE_GSC_FOR_PREVIEW_CALLBACK
    pipeIdCsc = PIPE_GSC_CALLBACK;
#else
    pipeIdCsc = PIPE_GSC;
#endif
    previewCallbackPipeId = PIPE_MCSC1;
    previewCbBuffer.index = -2;

    previewQ = m_previewQ;

    if (m_longExposureCaptureState == LONG_EXPOSURE_CANCEL_NOTI) {
        m_notifyCb(COMMON_SHOT_CANCEL_PICTURE_COMPLETED, 0, 0, m_callbackCookie);
        CLOGD(" COMMON_SHOT_CANCEL_PICTURE_COMPLETED callback(%d)",
                 m_fdmeta_shot->shot.dm.aa.aeState);
        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
    }

    CLOGV("wait previewQ");
    ret = previewQ->waitAndPopProcessQ(&frame);
    if (m_flagThreadStop == true) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);

        frame = NULL;

        return false;
    }
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    CLOGV("get frame from previewQ");
    timeStamp = (nsecs_t)frame->getTimeStamp();
    frameCount = frame->getFrameCount();
    ret = frame->getDstBuffer(pipeId, &scpBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    if (frame->getRequest(previewCallbackPipeId) == true) {
        ret = frame->getSrcBuffer(pipeId, &previewCbBuffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
    }
#endif

    /* ------------- frome here "frame" cannot use ------------- */
    CLOGV("push frame to previewReturnQ");
    if(m_parameters->increaseMaxBufferOfPreview()) {
        maxbuffers = m_parameters->getPreviewBufferCount();
    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (scpBuffer.index < 0 || scpBuffer.index >= maxbuffers ) {
        CLOGE("Out of Index! (Max: %d, Index: %d)", maxbuffers, scpBuffer.index);
        goto func_exit;
    }

    CLOGV("m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)",
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) || (m_parameters->getShotMode() == SHOT_MODE_KIDS)
        ) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("skip the preview callback and the preview display while compressed callback.");
            if (previewCbBuffer.index >= 0) {
                ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Preview callback buffer return fail");
                }
            }
            ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);
            goto func_exit;
        }
    }

    if ((m_previewWindow == NULL) ||
        (m_getRecordingEnabled() == true) ||
        (m_parameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 4 &&
        previewQ->getSizeOfProcessQ() < 2) ||
        (m_parameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1)) {

        if ((m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false)) {
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
            m_previewFrameFactory->setRequest(PIPE_MCSC1, true);
#endif

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                CLOGE("m_setPreviewCallback Buffer fail");
                return ret;
            }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
            /* Exynos8890 has MC scaler, so it need not make callback frame */
            int bufIndex = -2;
            if (previewCbBuffer.index < 0) {
                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                copybuffer = false;
            } else {
                copybuffer = true;
            }
#else
            int bufIndex = -2;
            m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
            copybuffer = false;
#endif

            ExynosCameraFrameSP_sptr_t newFrame = NULL;

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeIdCsc);
            if (newFrame == NULL) {
                CLOGE("newFrame is NULL");
                return UNKNOWN_ERROR;
            }

            m_copyMetaFrameToFrame(frame, newFrame, true, true);

            ret = m_doPreviewToCallbackFunc(pipeIdCsc, newFrame, scpBuffer, previewCbBuffer, copybuffer);
            if (ret < 0) {
                CLOGE("m_doPreviewToCallbackFunc fail");
                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Preview callback buffer return fail");
                    }
                }
                m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                goto func_exit;
            } else {
                if (m_parameters->getCallbackNeedCopy2Rendering() == true) {
                    ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, scpBuffer);
                    if (ret < 0) {
                        CLOGE("m_doCallbackToPreviewFunc fail");
                        if (previewCbBuffer.index >= 0) {
                            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("Preview callback buffer return fail");
                            }
                        }
                        m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                        goto func_exit;
                    }
                }
            }


            if (newFrame != NULL) {
                newFrame = NULL;
            }
        }
#ifdef USE_GSC_FOR_PREVIEW_CALLBACk
        else if (!m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)
                && m_parameters->getCallbackNeedCSC()
                && m_previewFrameFactory->checkPipeThreadRunning(pipeIdCsc)) {
                m_previewFrameFactory->stopThread(pipeIdCsc);
        }
#endif

        if (m_previewWindow != NULL) {
            if (timeStamp > 0L) {
                m_previewWindow->set_timestamp(m_previewWindow, (int64_t)timeStamp);
            } else {
                uint32_t fcount = 0;
                getStreamFrameCount((struct camera2_stream *)scpBuffer.addr[scpBuffer.getMetaPlaneIndex()], &fcount);
                CLOGW(" frameCount(%d)(%d), Invalid timeStamp(%lld)",
                        frameCount,
                        fcount,
                        (long long)timeStamp);
            }
        }

#ifdef FIRST_PREVIEW_TIME_CHECK
        if (m_flagFirstPreviewTimerOn == true) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            m_firstPreviewTimer.stop();
            m_flagFirstPreviewTimerOn = false;

            CLOGD("m_firstPreviewTimer stop");

            CLOGD("============= First Preview time ==================");
            CLOGD("= startPreview ~ first frame  : %d msec", (int)m_firstPreviewTimer.durationMsecs());
            CLOGD("===================================================");
            autoFocusMgr->displayAFInfo();
        }
#endif

        /* Prevent displaying unprocessed beauty images in beauty shot. */
        if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) || (m_parameters->getShotMode() == SHOT_MODE_KIDS)
            ) {
            if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
                checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
                CLOGV("skip the preview callback and the preview display while compressed callback.");
                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Preview callback buffer return fail");
                    }
                }
                ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                goto func_exit;
            }
        }

        /* display the frame */
        ret = m_putBuffers(m_scpBufferMgr, scpBuffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("put Buffer fail");
        }

        if (previewCbBuffer.index >= 0) {
            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Preview callback buffer return fail");
            }
        }
    } else {
        CLOGW("Preview frame buffer is canceled."
                "PreviewThread is blocked or too many buffers are in Service."
                "PreviewBufferCount(%d), ScpBufferMgr(%d), PreviewQ(%d)",
                m_parameters->getPreviewBufferCount(),
                m_scpBufferMgr->getNumOfAvailableAndNoneBuffer(),
                previewQ->getSizeOfProcessQ());
        if (previewCbBuffer.index >= 0) {
            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Preview callback buffer return fail");
            }
        }
        ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);
    }

func_exit:

    if (frame != NULL) {
        frame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_recordingThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    status_t ret = NO_ERROR;
    int pipeId   = PIPE_MCSC2;

    if (m_doCscRecording == true) {
        bool useMCSC2Pipe = true;

        /* This case uses MCSC0 to GSC_VIDEO */
        if (m_parameters->getNumOfMcscOutputPorts() < 3) {
            useMCSC2Pipe = false;
        }

        if (useMCSC2Pipe == true) {
            pipeId = PIPE_MCSC2;
        } else {
            pipeId = PIPE_GSC_VIDEO;
        }
    }

    nsecs_t timeStamp = 0;
    nsecs_t frameDuration = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    CLOGV("wait gsc done output");
    ret = m_recordingQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (m_getRecordingEnabled() == false) {
        CLOGI("recording stopped");
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto func_exit;
    }
    CLOGV("gsc done for recording callback");

    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    if (buffer.index < 0
        || buffer.index >= (int)m_recordingBufferCount) {
        CLOGE("Out of Index! (Max: %d, Index: %d)", m_recordingBufferCount, buffer.index);
        goto func_exit;
    }

    timeStamp = m_recordingTimeStamp[buffer.index];
    frameDuration = (900000000 / MULTI_BUFFER_BASE_FPS) / buffer.batchSize;

    if (m_recordingStartTimeStamp == 0) {
        m_recordingStartTimeStamp = timeStamp;
        CLOGI("m_recordingStartTimeStamp=%lld frameDuration %lld batchSize %d",
                m_recordingStartTimeStamp,
                (long long)frameDuration,
                buffer.batchSize);
    }

#if 0
{
    char filePath[70];
    int width, height = 0;

    m_parameters->getVideoSize(&width, &height);

    if (buffer.index == 3 && kkk < 1) {

        CLOGE("getVideoSize(%d x %d)", width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/video_%d0.yuv", buffer.index);
        dumpToFile2plane((char *)filePath, buffer.addr[0], buffer.addr[1], width * height, width * height / 2);

        kkk ++;
    }
}
#endif

    if (timeStamp > 0L
        && timeStamp > m_lastRecordingTimeStamp
        && timeStamp >= m_recordingStartTimeStamp) {
        int heapIndex = -1;
        native_handle* handle = NULL;
        for (int batchIndex = 0; batchIndex < buffer.batchSize; batchIndex++) {
            int planeIndex = batchIndex * (buffer.planeCount - 1);
            int recordingFrameIndex = -1;
            if (buffer.batchSize > 1 && m_doCscRecording == true) {
                ret = m_recordingBufferMgr->getIndexByFd(buffer.fd[planeIndex], &recordingFrameIndex);
                if (ret != NO_ERROR || recordingFrameIndex < 0) {
                    CLOGE("ERR(%s[%d]):[B%d]Failed to getIndexByFd %d. recordingFrameIndex %d",
                            __FUNCTION__, __LINE__,
                            buffer.index,
                            buffer.fd[planeIndex],
                            recordingFrameIndex);

                    timeStamp += frameDuration;
                    continue;
                }
            } else {
                recordingFrameIndex = buffer.index;
            }
#ifdef CHECK_MONOTONIC_TIMESTAMP
            CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::buffer.index=%d, recordingFrameIndex=%d,"\
                    "batchIndex=%d, recordingTimeStamp=%lld",
                    __FUNCTION__, __LINE__,
                    buffer.index, recordingFrameIndex, batchIndex, timeStamp);
#endif
#ifdef DEBUG
            CLOGD("- lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                m_lastRecordingTimeStamp,
                systemTime(SYSTEM_TIME_MONOTONIC),
                m_recordingStartTimeStamp);
#endif

            heapIndex = -1;
            handle = NULL;

            if (m_recordingCallbackHeap != NULL) {
                struct VideoNativeHandleMetadata *recordAddrs = NULL;

                ret = m_getAvailableRecordingCallbackHeapIndex(&heapIndex);
                if (ret != NO_ERROR || heapIndex < 0 || heapIndex >= m_recordingBufferCount) {
                    CLOGE("Failed to getAvailableRecordingCallbackHeapIndex %d", heapIndex);
                    goto func_exit;
                }

                recordAddrs = (struct VideoNativeHandleMetadata *) m_recordingCallbackHeap->data;
                recordAddrs[heapIndex].eType = kMetadataBufferTypeNativeHandleSource;

                handle = native_handle_create(2, 1);
                if (handle == NULL) {
                    CLOGE("native hande create fail, skip frame");
                    goto func_exit;
                }

                handle->data[0] = (int32_t) buffer.fd[planeIndex];
                handle->data[1] = (int32_t) buffer.fd[planeIndex+1];
                handle->data[2] = (int32_t) recordingFrameIndex;

                recordAddrs[heapIndex].pHandle = handle;

                m_recordingBufAvailable[recordingFrameIndex] = false;
                m_lastRecordingTimeStamp = timeStamp;

                // mfc cannot encode NV21 linear.
                // so, mempcy from preview buffer(NV21) to recording buffer(NV21M).
                if (m_parameters->doCscRecording() == false &&
                    m_parameters->getHwPreviewFormat() == V4L2_PIX_FMT_NV21) {
                    int recordingBufIndex = -1;
                    ExynosCameraBuffer recordingBuf;

                    ret = m_recordingBufferMgr->getBuffer(&recordingBufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuf);
                    if (ret != NO_ERROR) {
                        CLOGE("m_recordingBufferMgr->getBuffer() fail, ret(%d)", ret);
                    }

                    int videoWidth, videoHeight = 0;
                    m_parameters->getVideoSize(&videoWidth, &videoHeight);

                    ret = m_copyNV21toNV21M(buffer, recordingBuf, videoWidth, videoHeight, true, true);
                    if (ret != NO_ERROR) {
                        CLOGE("m_copyNV21toNV21M(%d, %d) fail, ret(%d)",
                            videoWidth, videoHeight, ret);
                    } else {
                        handle->data[0] = (int32_t)recordingBuf.fd[0];
                        handle->data[1] = (int32_t)recordingBuf.fd[1];
                        handle->data[2] = (int32_t)recordingBuf.index;
                    }
                }
            } else {
                CLOGD("m_recordingCallbackHeap is NULL.");
            }

            if (m_getRecordingEnabled() == true
                && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {

                m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    heapIndex,
                    m_callbackCookie);
                }

                if (handle != NULL) {
                    native_handle_delete(handle);
                }

                timeStamp += frameDuration;
        }
    } else {
        CLOGW("buffer.index=%d, timeStamp(%lld) invalid -"
            " lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
             buffer.index, (long long)timeStamp,
            (long long)m_lastRecordingTimeStamp,
            (long long)systemTime(SYSTEM_TIME_MONOTONIC),
            (long long)m_recordingStartTimeStamp);
        m_releaseRecordingBuffer(buffer.index);
    }

func_exit:
    if (frame != NULL) {
        ret = m_removeFrameFromList(&m_recordingProcessList, frame, &m_recordingProcessListLock);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }
        frame = NULL;
    }

    return m_recordingEnabled;
}

void ExynosCamera::m_vendorSpecificPreConstructor(int cameraId, __unused camera_device_t *dev)
{
    CLOGI(" -IN-");

    CLOGI(" -OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificConstructor(__unused int cameraId, __unused camera_device_t *dev)
{
    CLOGI(" -IN-");

    m_postPictureCallbackThread = new mainCameraThread(this, &ExynosCamera::m_postPictureCallbackThreadFunc, "postPictureCallbackThread");
    CLOGD(" postPictureCallbackThread created");

    /* m_ThumbnailCallback Thread */
    m_ThumbnailCallbackThread = new mainCameraThread(this, &ExynosCamera::m_ThumbnailCallbackThreadFunc, "m_ThumbnailCallbackThread");
    CLOGD("m_ThumbnailCallbackThread created");

    m_yuvCallbackThread = new mainCameraThread(this, &ExynosCamera::m_yuvCallbackThreadFunc, "yuvCallbackThread");
    CLOGD("yuvCallbackThread created");

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("visionThread created");

    m_yuvCallbackQ = new frame_queue_t(m_yuvCallbackThread);

#ifdef BURST_CAPTURE
    m_burstCaptureCallbackCount = 0;
    m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif

    m_currentSetStart = false;
    m_flagMetaDataSet = false;

    m_fdCallbackToggle = 0;

    m_flagAFDone = false;

    CLOGI(" -OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI(" -IN-");

    CLOGI(" -OUT-");

    return;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraDurationTimer m_fdcallbackTimer;
    long long m_fdcallbackTimerTime;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    bool autoFocusChanged = false;
    uint32_t shutterSpeed = 0;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
    int ret = 0;
    uint32_t pipeId = PIPE_VC1;
    camera_memory_t *depthCallbackHeap = NULL;

    memset(&m_frameMetadata.depth_data, 0x00, sizeof(camera_depth_data_t));
#endif

    if (m_fdmeta_shot == NULL) {
        CLOGE(" meta_shot_ext is null");
        return INVALID_OPERATION;
    }

    memset(m_fdmeta_shot, 0x00, sizeof(struct camera2_shot_ext));
    if (frame->getDynamicMeta(m_fdmeta_shot) != NO_ERROR) {
        CLOGE(" meta_shot_ext is null");
        return INVALID_OPERATION;
    }

    frame->getUserDynamicMeta(m_fdmeta_shot);

    if (m_flagAFDone) {
        m_flagAFDone = false;
        autoFocusChanged = true;
        CLOGD(" AF Done");
    }

    if (m_flagStartFaceDetection == true && m_fdFrameSkipCount == 0) {
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

        m_parameters->getHwPreviewSize(&previewW, &previewH);

        camera2_node_group node_group_info;
        frame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_FLITE);

        dm = &(m_fdmeta_shot->shot.dm);

        CLOGV(" faceDetectMode(%d)", dm->stats.faceDetectMode);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][0], dm->stats.faceRectangles[0][1]);
        CLOGV("[%d %d]", dm->stats.faceRectangles[0][2], dm->stats.faceRectangles[0][3]);

       num = NUM_OF_DETECTED_FACES;
        if (m_parameters->getMaxNumDetectedFaces() < num)
            num = m_parameters->getMaxNumDetectedFaces();

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
                case FACEDETECT_MODE_FULL:
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

                /* Go through */
                case FACEDETECT_MODE_SIMPLE:
                    id[i] = dm->stats.faceIds[i];
                    score[i] = dm->stats.faceScores[i];

                    if (m_parameters->isMcscVraOtf() == false) {
                        int vraWidth = 0, vraHeight = 0;
                        m_parameters->getHwVraInputSize(&vraWidth, &vraHeight);

                        detectedFace[i].x1 = m_calibratePosition(vraWidth, previewW, dm->stats.faceRectangles[i][0]);
                        detectedFace[i].y1 = m_calibratePosition(vraHeight, previewH, dm->stats.faceRectangles[i][1]);
                        detectedFace[i].x2 = m_calibratePosition(vraWidth, previewW, dm->stats.faceRectangles[i][2]);
                        detectedFace[i].y2 = m_calibratePosition(vraHeight, previewH, dm->stats.faceRectangles[i][3]);
                    } else if ((int)(node_group_info.leader.output.cropRegion[2]) < previewW
                               || (int)(node_group_info.leader.output.cropRegion[3]) < previewH) {
                        detectedFace[i].x1 = m_calibratePosition(node_group_info.leader.output.cropRegion[2], previewW, dm->stats.faceRectangles[i][0]);
                        detectedFace[i].y1 = m_calibratePosition(node_group_info.leader.output.cropRegion[3], previewH, dm->stats.faceRectangles[i][1]);
                        detectedFace[i].x2 = m_calibratePosition(node_group_info.leader.output.cropRegion[2], previewW, dm->stats.faceRectangles[i][2]);
                        detectedFace[i].y2 = m_calibratePosition(node_group_info.leader.output.cropRegion[3], previewH, dm->stats.faceRectangles[i][3]);
                    } else {
                        detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                        detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                        detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                        detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                    }

                    numOfDetectedFaces++;

                /* Go through */
                case FACEDETECT_MODE_OFF:
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

                CLOGV(" left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
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

    if (m_currentSetStart == true)
        m_flagMetaDataSet = true;

    if (m_fdmeta_shot->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
        shutterSpeed = m_fdmeta_shot->shot.udm.ae.vendorSpecific[64];
    else
        shutterSpeed = m_fdmeta_shot->shot.udm.internal.vendorSpecific2[100];

#if defined(BURST_CAPTURE) && defined(VARIABLE_BURST_FPS)
    int Bv = m_fdmeta_shot->shot.udm.internal.vendorSpecific2[102];

    /* vendorSpecific[64] : Exposure time's denominator */
    if ((shutterSpeed <= 7)
#ifdef USE_BV_FOR_VARIABLE_BURST_FPS
#ifdef LLS_BV_REAR
        || ((Bv <= LLS_BV_REAR) && (m_cameraId == CAMERA_ID_BACK))
#endif
        || ((Bv <= LLS_BV) && (m_cameraId == CAMERA_ID_FRONT))
#endif
        ) {
        m_frameMetadata.burstshot_fps = BURSTSHOT_OFF_FPS;
    } else if (shutterSpeed <= 14) {
        m_frameMetadata.burstshot_fps = BURSTSHOT_MIN_FPS;
    } else {
        m_frameMetadata.burstshot_fps = BURSTSHOT_MAX_FPS;
    }

    CLOGV("(%s):m_frameMetadata.burstshot_fps(%d) / Bv(%d)",
        __FUNCTION__, m_frameMetadata.burstshot_fps, Bv);
#endif

#ifdef TOUCH_AE
    if (m_parameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH
        && m_parameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH
        && m_fdmeta_shot->shot.dm.aa.vendor_touchAeDone == 1 /* Touch AE status flag - 0:searching 1:done */
        ) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGD(" AE_RESULT(%d)", m_fdmeta_shot->shot.dm.aa.aeState);
    }

#endif

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true) {
        ret = frame->getDstBuffer(pipeId, &depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get DepthMap buffer");
        }

        if (depthMapBuffer.index != -2 && autoFocusChanged) {
            int depthW = 0, depthH = 0;
            m_parameters->getDepthMapSize(&depthW, &depthH);
            depthCallbackHeap = m_getMemoryCb(depthMapBuffer.fd[0], depthMapBuffer.size[0], 1, m_callbackCookie);

            if (!depthCallbackHeap || depthCallbackHeap->data == MAP_FAILED) {
                CLOGE("m_getMemoryCb(%d) fail", depthMapBuffer.size[0]);
                depthCallbackHeap = NULL;
            } else {
                m_frameMetadata.depth_data.width = depthW;
                m_frameMetadata.depth_data.height = depthH;
                m_frameMetadata.depth_data.format = HAL_PIXEL_FORMAT_RAW10;
                m_frameMetadata.depth_data.depth_data = (void *)depthCallbackHeap;
            }
        }
#if 0
        char filePath[70];
        int bRet = 0;
        int depthMapW = 0, depthMapH = 0;
        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);

        if(frame->getFrameCount() <= 15) {
            memset(filePath, 0, sizeof(filePath));
            snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",
                m_parameters->getCameraId(), frame->getFrameCount());
            /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */

            CLOGD("Raw Dump start (%s)", filePath);

            bRet = dumpToFile((char *)filePath,
                 (char *)depthCallbackHeap->data,
                depthMapW * depthMapH * 2);
            if (bRet != true)
                CLOGE("couldn't make a raw file");
        }
#endif
    }

    if (m_parameters->getDepthCallbackOnPreview() && autoFocusChanged && !depthCallbackHeap) {
        m_flagAFDone = true;
    }
#endif

    m_fdcallbackTimer.start();

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
            (m_flagStartFaceDetection || m_flagLLSStart || m_flagLightCondition || m_flagMetaDataSet
            || m_flagBrightnessValue
#ifdef SUPPORT_DEPTH_MAP
            || m_parameters->getDepthCallbackOnPreview()
#endif
    ))
    {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
        m_dataCb(CAMERA_MSG_PREVIEW_METADATA, m_fdCallbackHeap, 0, &m_frameMetadata, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_META, false);
    }
    m_fdcallbackTimer.stop();
    m_fdcallbackTimerTime = m_fdcallbackTimer.durationUsecs();

#ifdef SUPPORT_DEPTH_MAP
    if (depthMapBuffer.index != -2) {
        if (depthCallbackHeap) {
            depthCallbackHeap->release(depthCallbackHeap);
        }

        ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("Failed to put DepthMap buffer to bufferMgr");
        }
    }
#endif

    m_flagMetaDataSet = false;

    if((int)m_fdcallbackTimerTime / 1000 > 50) {
        CLOGD(" FD callback duration time : (%d)mec", (int)m_fdcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToRecordingFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer recordingBuf,
        nsecs_t timeStamp)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    CLOGV("--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
         previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    struct camera2_node_output node;
#ifdef PERFRAME_CONTROL_FOR_FLIP
    int flipHorizontal = m_parameters->getFlipHorizontal();
    int flipVertical = m_parameters->getFlipVertical();
#endif

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return UNKNOWN_ERROR;
    }

#ifdef PERFRAME_CONTROL_FOR_FLIP
    newFrame->setFlipHorizontal(pipeId, flipHorizontal);
    newFrame->setFlipVertical(pipeId, flipVertical);
#endif

    /* TODO: HACK: Will be removed, this is driver's job */
    m_convertingStreamToShotExt(&previewBuf, &node);
    setMetaDmSensorTimeStamp((camera2_shot_ext *)previewBuf.addr[previewBuf.getMetaPlaneIndex()], timeStamp);

    /* csc and scaling */
    ret = m_calcRecordingGSCRect(&srcRect, &dstRect);
    ret = newFrame->setSrcRect(pipeId, srcRect);
    ret = newFrame->setDstRect(pipeId, dstRect);

    ret = m_setupEntity(pipeId, newFrame, &previewBuf, &recordingBuf);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
             pipeId, ret);
        ret = INVALID_OPERATION;
        if (newFrame != NULL) {
            newFrame = NULL;
        }
        goto func_exit;
    }

    ret = m_insertFrameToList(&m_recordingProcessList, newFrame, &m_recordingProcessListLock);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                newFrame->getFrameCount());
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

    m_recordingStopLock.lock();
    if (m_getRecordingEnabled() == false) {
        m_recordingStopLock.unlock();
        CLOGD(" m_getRecordingEnabled is false, skip frame(%d) previewBuf(%d) recordingBuf(%d)",
                newFrame->getFrameCount(), previewBuf.index, recordingBuf.index);
        if (newFrame != NULL) {
            ret = m_removeFrameFromList(&m_recordingProcessList, newFrame, &m_recordingProcessListLock);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }
            newFrame = NULL;
        }

        if (recordingBuf.index >= 0){
            m_putBuffers(m_recordingBufferMgr, recordingBuf.index);
        }
        goto func_exit;
    }

    m_previewFrameFactory->pushFrameToPipe(newFrame, pipeId);
    m_recordingStopLock.unlock();

func_exit:

    CLOGV("--OUT--");
    return ret;
}

status_t ExynosCamera::m_reprocessingYuvCallbackFunc(ExynosCameraBuffer yuvBuffer)
{
    camera_memory_t *yuvCallbackHeap = NULL;

    if (m_hdrEnabled == false && m_parameters->getSeriesShotCount() <= 0
       ) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY) == true) {
            CLOGD("RAW_IMAGE_NOTIFY callback");

            m_notifyCb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, m_callbackCookie);
        }

        if (yuvBuffer.index < 0) {
            CLOGE("Invalid YUV buffer index %d. Skip to callback%s%s",
                     yuvBuffer.index,
                    (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE))? " RAW" : "",
                    (m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))? " POSTVIEW" : "");

            return BAD_VALUE;
        }

        if (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            CLOGD("RAW callback");

            yuvCallbackHeap = m_getMemoryCb(yuvBuffer.fd[0], yuvBuffer.size[0], 1, m_callbackCookie);
            if (!yuvCallbackHeap || yuvCallbackHeap->data == MAP_FAILED) {
                CLOGE("Failed to getMemoryCb(%d)",
                         yuvBuffer.size[0]);

                return INVALID_OPERATION;
            }

            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, yuvCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            yuvCallbackHeap->release(yuvCallbackHeap);
        }

        if ((m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))
                && !m_parameters->getIsThumbnailCallbackOn()
           ) {
            CLOGD("POSTVIEW callback");

            yuvCallbackHeap = m_getMemoryCb(yuvBuffer.fd[0], yuvBuffer.size[0], 1, m_callbackCookie);
            if (!yuvCallbackHeap || yuvCallbackHeap->data == MAP_FAILED) {
                CLOGE("Failed to getMemoryCb(%d)",
                         yuvBuffer.size[0]);

                return INVALID_OPERATION;
            }

            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, yuvCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            yuvCallbackHeap->release(yuvCallbackHeap);
        }
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
    /* Fixed 1 batchSize */
    int batchSize = 1;
    int bufferCount = NUM_FASTAESTABLE_BUFFER;

    if (skipBufferMgr == NULL) {
        CLOGE("createBufferManager failed");
        goto func_exit;
    }

    ret = m_allocBuffers(skipBufferMgr, planeCount, planeSize, bytesPerLine,
                         bufferCount, batchSize, true, false);
    if (ret < 0) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
             bufferCount);
        return ret;
    }

    for (int i = 0; i < bufferCount; i++) {
        int index = i;
        ret = skipBufferMgr->getBuffer(&index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &fastenAeBuffer[i]);
        if (ret < 0) {
            CLOGE("getBuffer fail");
            goto done;
        }
    }

    m_parameters->setFastenAeStableOn(true);

    ret = m_previewFrameFactory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret < 0) {
        ret = INVALID_OPERATION;
        // doing some error handling
    }

    m_parameters->setFastenAeStableOn(false);

    m_checkFirstFrameLux = true;
done:
    skipBufferMgr->deinit();
    delete skipBufferMgr;
    skipBufferMgr = NULL;

func_exit:
    return ret;
}


status_t ExynosCamera::m_startRecordingInternal(void)
{
    int ret = 0;

    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int videoW = 0, videoH = 0;
    int videoFormat  = m_parameters->getVideoFormat();
    int planeCount  = getYuvPlaneCount(videoFormat);
    planeCount++;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_SILENT;
    int heapFd = 0;

    m_parameters->getHwVideoSize(&videoW, &videoH);
    CLOGD("videoSize = %d x %d",   videoW, videoH);

    m_doCscRecording = true;
    if (m_parameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("do Recording CSC !!! m_recordingBufferCount(%d)", m_recordingBufferCount);
    } else {
        m_doCscRecording = false;
        {
            m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        }
        CLOGI("skip Recording CSC !!! m_recordingBufferCount(%d->%d)",
            m_exynosconfig->current->bufInfo.num_recording_buffers, m_recordingBufferCount);
    }

    /* clear previous recording frame */
    CLOGD("Recording m_recordingProcessList(%d) IN",
            (int)m_recordingProcessList.size());
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
    }
    CLOGD("Recording m_recordingProcessList(%d) OUT",
            (int)m_recordingProcessList.size());

    for (int32_t i = 0; i < m_recordingBufferCount; i++) {
        m_recordingTimeStamp[i] = 0L;
        m_recordingBufAvailable[i] = true;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct VideoNativeHandleMetadata), m_recordingBufferCount, &heapFd);
    if (!m_recordingCallbackHeap || m_recordingCallbackHeap->data == MAP_FAILED) {
        CLOGE("m_getMemoryCb(%zu) fail", sizeof(struct addrs));
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* Initialize recording callback heap available flags */
    memset(m_recordingCallbackHeapAvailable, 0x00, sizeof(m_recordingCallbackHeapAvailable));
    for (int i = 0; i < m_recordingBufferCount; i++) {
        m_recordingCallbackHeapAvailable[i] = true;
    }

    if ((m_doCscRecording == true)
        && m_recordingBufferMgr->isAllocated() == false) {
        /* alloc recording Image buffer */
        planeSize[0] = ROUND_UP(videoW, MFC_ALIGN) * ROUND_UP(videoH, MFC_ALIGN) + MFC_7X_BUFFER_OFFSET;
	/*Chroma buffer size should to be optimized for MFC V11.20 (LS,KM) */
        planeSize[1] = ROUND_UP(videoW, MFC_ALIGN) * ROUND_UP(videoH / 2, MFC_ALIGN*2) + MFC_7X_BUFFER_OFFSET;
        int batchSize = m_parameters->getBatchSize(PIPE_MCSC2);
        if( m_parameters->getHighSpeedRecording() == true)
            minBufferCount = m_recordingBufferCount;
        else
            minBufferCount = 1;

        maxBufferCount = m_recordingBufferCount;

        ret = m_allocBuffers(m_recordingBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize,type, allocMode,
                             true, true);
        if (ret < 0) {
            CLOGE("m_recordingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        }

    }

    if (m_recordingQ->getSizeOfProcessQ() > 0) {
        CLOGE("m_startRecordingInternal recordingQ(%d)", m_recordingQ->getSizeOfProcessQ());
        m_clearList(m_recordingQ);
    }

    m_recordingThread->run();

func_exit:

    return ret;
}

status_t ExynosCamera::m_stopRecordingInternal(void)
{
    status_t ret = NO_ERROR;

    if (m_doCscRecording == true) {
        bool useGSCPipe = false;

        /* This case uses MCSC0 to GSC_VIDEO */
        if (m_parameters->getNumOfMcscOutputPorts() < 3) {
            useGSCPipe = true;
        }

        if (useGSCPipe == true) {
            int recPipeId = PIPE_GSC_VIDEO;
            {
                Mutex::Autolock lock(m_recordingStopLock);
                m_previewFrameFactory->stopPipe(recPipeId);
            }
        }
    }

    m_recordingQ->sendCmd(WAKE_UP);
    m_recordingThread->requestExitAndWait();
    m_recordingQ->release();
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret != NO_ERROR) {
        CLOGE("m_clearList fail");
        return ret;
    }

    /* Checking all frame(buffer) released from Media recorder */
    for (int bufferIndex = 0; bufferIndex < m_recordingBufferCount; bufferIndex++) {
        if (m_recordingBufAvailable[bufferIndex] == false) {
            if (m_doCscRecording == true) {
                m_releaseRecordingBuffer(bufferIndex);
            } else {
                m_recordingTimeStamp[bufferIndex] = 0L;
                m_recordingBufAvailable[bufferIndex] = true;
            }
            CLOGW("frame(%d) wasn't release, and forced release at here",
                     bufferIndex);
        }
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_shutterCallbackThreadFunc(void)
{
    CLOGI("");
    int loop = false;

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        CLOGI(" CAMERA_MSG_SHUTTER callback S");
#ifdef BURST_CAPTURE
        m_notifyCb(CAMERA_MSG_SHUTTER, m_parameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
        CLOGI(" CAMERA_MSG_SHUTTER callback E");
    }

    /* one shot */
    return loop;
}

bool ExynosCamera::m_releasebuffersForRealloc()
{
    status_t ret = NO_ERROR;
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGD("m_setBuffers free all buffers");
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
#if defined(DEBUG_RAWDUMP)
    if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        if (m_fliteBufferMgr != NULL) {
            m_fliteBufferMgr->deinit();
        }
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

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_yuvBufferMgr != NULL) {
        m_yuvBufferMgr->deinit();
    }

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }
    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    m_parameters->setReallocBuffer(false);

    if (m_parameters->getRestartPreview() == true) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            CLOGE("setPreviewWindow fail");
            return INVALID_OPERATION;
        }
    }

   return true;
}

status_t ExynosCamera::m_setReprocessingBuffer(void)
{
    status_t ret = NO_ERROR;
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    int maxPictureW = 0, maxPictureH = 0;
    int maxThumbnailW = 0, maxThumbnailH = 0;
    int planeCount = 0;
    int batchSize = 1;
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AP_REPROCESSING);
    int pictureFormat = m_parameters->getHwPictureFormat();
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    bool needMmap = false;

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&maxPictureW, &maxPictureH);
        CLOGI("HW Picture(HighSpeed) width x height = %dx%d", maxPictureW, maxPictureH);
    } else {
        m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
        CLOGI("HW Picture width x height = %dx%d", maxPictureW, maxPictureH);
    }
    m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);

    /* Reprocessing 3AA to ISP Buffer */
    if (m_parameters->getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER
        && m_parameters->isReprocessing3aaIspOTF() == false) {
        bayerFormat     = m_parameters->getBayerFormat(PIPE_3AP_REPROCESSING);
        bytesPerLine[0] = getBayerLineSize(maxPictureW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(maxPictureW, maxPictureH, bayerFormat);
        planeCount      = 2;
        batchSize       = m_parameters->getBatchSize(PIPE_3AP_REPROCESSING);

        /* ISP Reprocessing Buffer realloc for high resolution callback */
        if (m_parameters->getHighResolutionCallbackMode() == true) {
            minBufferCount = 2;
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        } else {
            minBufferCount = 1;
            maxBufferCount = m_exynosconfig->current->bufInfo.num_reprocessing_buffers;
        }
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        {
            needMmap = false;
        }
        ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize, type, allocMode,
                             true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
                     minBufferCount, maxBufferCount);
            return ret;
        }

        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);

        CLOGI("m_allocBuffers(ISP Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                 maxPictureW, maxPictureH, planeCount, maxBufferCount);
    }

    if (m_parameters->isUseHWFC() == true
        && m_parameters->getHighResolutionCallbackMode() == false)
        allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_parameters->getPictureSize(&maxPictureW, &maxPictureH);
        CLOGI("HW Picture(HighResolutionCB) width x height = %dx%d", maxPictureW, maxPictureH);
    }

    /* Reprocessing YUV Buffer */
    switch (pictureFormat) {
    case V4L2_PIX_FMT_NV21M:
        planeCount      = 3;
        planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN);
        planeSize[1]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) / 2;
        break;
    case V4L2_PIX_FMT_NV21:
        planeCount      = 2;
        planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
        break;
    default:
        planeCount      = 2;
        planeSize[0]    = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 2;
        break;
    }

    batchSize = m_parameters->getBatchSize(PIPE_MCSC3_REPROCESSING);
    /* SCC Reprocessing Buffer realloc for high resolution callback */
    if (m_parameters->getHighResolutionCallbackMode() == true)
        minBufferCount = 2;
    else
        minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

    {
        needMmap = false;
    }

    ret = m_allocBuffers(m_yuvReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                         minBufferCount, maxBufferCount, batchSize, type, allocMode,
                         true, needMmap);
    if (ret != NO_ERROR) {
        CLOGE("m_yuvReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(YUV Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
             maxPictureW, maxPictureH, planeCount, maxBufferCount);

    /* Reprocessing Thumbnail Buffer */
    if (pictureFormat == V4L2_PIX_FMT_NV21M) {
        planeCount      = 3;
        planeSize[0]    = maxThumbnailW * maxThumbnailH;
        planeSize[1]    = maxThumbnailW * maxThumbnailH / 2;
    } else {
        planeCount      = 2;
        planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), maxThumbnailW, maxThumbnailH);
    }
    batchSize = m_parameters->getBatchSize(PIPE_MCSC4_REPROCESSING);
    minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    {
        needMmap = false;
    }
    ret = m_allocBuffers(m_thumbnailBufferMgr, planeCount, planeSize, bytesPerLine,
                         minBufferCount, maxBufferCount, batchSize, type, allocMode,
                         true, needMmap);
    if (ret != NO_ERROR) {
        CLOGE("m_thumbnailBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(Thumb Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d) needMmap(%d)",
             maxThumbnailW, maxThumbnailH, planeCount, maxBufferCount, needMmap);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVendorBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW = 0, hwPreviewH = 0;
    int hwVideoW = 0, hwVideoH = 0;
    int sensorMaxW = 0, sensorMaxH = 0;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);

    int planeCount  = 1;
    int batchSize = 1;
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    bool needMmap = false;

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getHwVideoSize(&hwVideoW, &hwVideoH);

    CLOGI("HW Preview width x height = %dx%d",
             hwPreviewW, hwPreviewH);
    CLOGI("HW Video width x height = %dx%d",
             hwVideoW, hwVideoH);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("HW Sensor(HighSpeed) MAX width x height = %dx%d",
                 sensorMaxW, sensorMaxH);
    } else {
        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("Sensor MAX width x height = %dx%d",
                 sensorMaxW, sensorMaxH);
    }

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    /* Bayer(FLITE, 3AC) Buffer */
    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        bayerFormat = m_parameters->getBayerFormat(PIPE_VC0);
        break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);
        break;
    default:
        CLOGV("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    bytesPerLine[0] = getBayerLineSize(sensorMaxW, bayerFormat);
    planeSize[0]    = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat);
    planeCount      = 2;
    batchSize       = m_parameters->getBatchSize(PIPE_FLITE);

    maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
#if defined(CAMERA_ADD_BAYER_ENABLE)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#endif
        m_bayerBufferMgr->setContigBufCount(RESERVED_NUM_BAYER_BUFFERS);
    } else if (m_parameters->getDualMode() == false) {
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
        m_bayerBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_BAYER_BUFFERS);
    } else
#endif
    {
#if defined(CAMERA_ADD_BAYER_ENABLE)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
#endif
    }

    if (m_parameters->getIntelligentMode() != 1 &&
        reprocessingMode != PROCESSING_MODE_NON_REPROCESSING_YUV) {
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, maxBufferCount, batchSize,
                             type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                     maxBufferCount);
            return ret;
        }
    }

#ifdef CAMERA_PACKED_BAYER_ENABLE
    memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

    CLOGI("m_allocBuffers(Bayer Buffer) planeSize(%d), planeCount(%d), maxBufferCount(%d)",
             planeSize[0], planeCount, maxBufferCount);

#if defined(DEBUG_RAWDUMP)
    if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        /* Bayer Dump Buffer */
        bayerFormat = m_parameters->getBayerFormat(PIPE_VC0);
        batchSize = m_parameters->getBatchSize(PIPE_VC0);

        bytesPerLine[0] = getBayerLineSize(sensorMaxW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat);
        planeCount      = 2;

        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, \
                               bytesPerLine, maxBufferCount, maxBufferCount, batchSize, type, true, true);
        if (ret != NO_ERROR) {
            CLOGE("fliteBuffer m_allocBuffers(bufferCount=%d) fail",
                     maxBufferCount);
            return ret;
        }

        CLOGI("m_allocBuffers(Flite Buffer) planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                 planeSize[0], planeCount, maxBufferCount);
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_setVendorPictureBuffer(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int sensorMaxW = 0, sensorMaxH = 0, sensorMaxThumbW = 0, sensorMaxThumbH = 0;
    int planeCount;
    int batchSize = 1;
    int minBufferCount;
    int maxBufferCount;
    exynos_camera_buffer_type_t type;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
    CLOGI("Sensor MAX width x height = %dx%d", sensorMaxW, sensorMaxH);
    m_parameters->getMaxThumbnailSize(&sensorMaxThumbW, &sensorMaxThumbH);
    CLOGI("Sensor MAX Thumbnail width x height = %dx%d", sensorMaxThumbW, sensorMaxThumbH);

    /* Allocate post picture buffers */
    int f_pictureW = 0, f_pictureH = 0;
    int f_previewW = 0, f_previewH = 0;
    int inputW = 0, inputH = 0;
    {
        m_parameters->getPictureSize(&f_pictureW, &f_pictureH);
    }
    m_parameters->getPreviewSize(&f_previewW, &f_previewH);

    if (f_pictureW * f_pictureH >= f_previewW * f_previewH) {
        inputW = f_pictureW;
        inputH = f_pictureH;
    } else {
        inputW = f_previewW;
        inputH = f_previewH;
    }

    planeSize[0] = inputW * inputH * 1.5;
    planeCount = 2;
    batchSize = m_parameters->getBatchSize(PIPE_MCSC2_REPROCESSING);
    minBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraIdInternal() == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        m_postPictureBufferMgr->setContigBufCount(RESERVED_NUM_POST_PIC_BUFFERS);
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
    } else {
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    }
#else
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#endif /* RESERVED_MEMORY_ENABLE */
    ret = m_allocBuffers(m_postPictureBufferMgr, planeCount, planeSize, bytesPerLine,
                         minBufferCount, maxBufferCount, batchSize,
                         type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(m_postPictureBuffer) %d x %d, planeCount(%d), planeSize(%d)",
             inputW, inputH, planeCount, planeSize[0]);

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseVendorBuffers(void)
{
    CLOGI("release buffer");

    CLOGI("free buffer done");

    return NO_ERROR;
}

void ExynosCamera::releaseRecordingFrame(const void *opaque)
{
    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" Vision mode does not support");
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    if (m_getRecordingEnabled() == false) {
        CLOGW("m_recordingEnabled equals false");
        /* m_stopRecordingInternal() will wait for recording frame release */
        /* return; */
    }

    if (m_recordingCallbackHeap == NULL) {
        CLOGW("recordingCallbackHeap equals NULL");
        /* The received native_handle must be closed */
        /* return; */
    }

    struct VideoNativeHandleMetadata *releaseMetadata = (struct VideoNativeHandleMetadata *) opaque;

    if (releaseMetadata == NULL) {
        CLOGW("releaseMetadata is NULL");
        return;
    }

    native_handle_t *releaseHandle = NULL;
    int releaseBufferIndex = -1;

    if (releaseMetadata->eType != kMetadataBufferTypeNativeHandleSource) {
        CLOGW("Inavlid VideoNativeHandleMetadata Type %d",
                releaseMetadata->eType);
        return;
    }

    /*
     * Support only NV21M,
     * data[0]: Y
     * data[1]: CbCr
     * data[2]: bufferIndex
     */
    releaseHandle = releaseMetadata->pHandle;
    if (releaseHandle == NULL) {
        CLOGE("Invalid releaseHandle");
        goto CLEAN;
    }

    releaseBufferIndex = releaseHandle->data[2];
    if (releaseBufferIndex >= m_recordingBufferCount) {
        CLOGW("Invalid VideoBufferIndex %d",
                releaseBufferIndex);

        goto CLEAN;
    } else if (m_recordingBufAvailable[releaseBufferIndex] == true) {
        CLOGW("Already released VideoBufferIndex %d",
                releaseBufferIndex);

        goto CLEAN;
    }

    if (m_doCscRecording == true) {
        m_releaseRecordingBuffer(releaseBufferIndex);
    } else {
    }

    m_recordingTimeStamp[releaseBufferIndex] = 0L;
    m_recordingBufAvailable[releaseBufferIndex] = true;

    m_isFirstStart = false;
    if (m_parameters != NULL) {
        m_parameters->setIsFirstStartFlag(m_isFirstStart);
    }

CLEAN:
    if (releaseHandle != NULL) {
        native_handle_close(releaseHandle);
        native_handle_delete(releaseHandle);
    }

    if (releaseMetadata != NULL) {
        m_releaseRecordingCallbackHeap(releaseMetadata);
    }

    return;
}

bool ExynosCamera::m_monitorThreadFunc(void)
{
    CLOGV("");

    int *threadState;
    struct timeval dqTime;
    uint64_t *timeInterval;
    int *countRenew;
    int camId = getCameraIdInternal();
    int ret = NO_ERROR;
    int loopCount = 0;

    int dtpStatus = 0;
    int pipeIdFlite = 0;
    int pipeIdErrorCheck = 0;

    for (loopCount = 0; loopCount < MONITOR_THREAD_INTERVAL; loopCount += (MONITOR_THREAD_INTERVAL/20)) {
        if (m_flagThreadStop == true) {
            CLOGI(" m_flagThreadStop(%d)", m_flagThreadStop);

            return false;
        }

        usleep(MONITOR_THREAD_INTERVAL/20);
    }

    if (m_previewFrameFactory == NULL) {
        CLOGW(" m_previewFrameFactory is NULL. Skip monitoring.");

        return false;
    }

    pipeIdFlite = m_getBayerPipeId();
    enum NODE_TYPE dstPos = m_previewFrameFactory->getNodeType(PIPE_VC0);

    if (IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_MCSC)) {
        pipeIdErrorCheck = PIPE_MCSC;
    } else {
        if (m_parameters->isTpuMcscOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_TPU))
            pipeIdErrorCheck = PIPE_TPU;
        else if (m_parameters->isIspTpuOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_ISP))
            pipeIdErrorCheck = PIPE_ISP;
        else if (m_parameters->isIspMcscOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_ISP))
            pipeIdErrorCheck = PIPE_ISP;
        else if (m_parameters->is3aaIspOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_3AA))
            pipeIdErrorCheck = PIPE_3AA;
        else
            pipeIdErrorCheck = pipeIdFlite;
    }

#ifdef MONITOR_LOG_SYNC
    /* define pipe for isp node cause of sync log sctrl */
    uint32_t pipeIdLogSync = PIPE_3AA;

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (m_previewFrameFactory->checkPipeThreadRunning(pipeIdLogSync) &&
        !(getCameraIdInternal() == CAMERA_ID_FRONT && m_parameters->getDualMode())){
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI(" @FIMC_IS_SYNC %d", syncLogId);
            m_previewFrameFactory->syncLog(pipeIdLogSync, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif

    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DTP Detected. dtpStatus(%d)", dtpStatus);
        m_dump();

        /* in GED */
        m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);

        return false;
    }

#ifdef SENSOR_OVERFLOW_CHECK
    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DTP Detected. dtpStatus(%d)", dtpStatus);
        m_dump();

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

#if 0
    m_checkThreadState(threadState, countRenew)?:ret = false;
    m_checkThreadInterval(PIPE_SCP, WARNING_SCP_THREAD_INTERVAL, threadState)?:ret = false;

    enum pipeline pipe;

    /* check PIPE_3AA thread state & interval */
    if (m_parameters->isFlite3aaOtf() == true) {
        pipe = PIPE_3AA_ISP;

        m_previewFrameFactory->getThreadRenew(&countRenew, pipe);
        m_checkThreadState(threadState, countRenew)?:ret = false;

        if (ret == false) {
            m_dump();

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
            m_dump();

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
            CLOGD("callback state is updated (0x%x)", m_callbackStateOld);
        } else {
            if ((m_callbackStateOld & m_callbackState) != 0)
                CLOGE("callback is blocked (0x%x), Duration:%d msec", m_callbackState, m_callbackMonitorCount*(MONITOR_THREAD_INTERVAL/1000));
        }
    }
#endif

    gettimeofday(&dqTime, NULL);
    m_previewFrameFactory->getThreadInterval(&timeInterval, pipeIdErrorCheck);

    CLOGV("Thread IntervalTime [%lld]", (long long)*timeInterval);
    CLOGV("Thread Renew Count [%d]", *countRenew);

    {
        m_previewFrameFactory->incThreadRenew(pipeIdErrorCheck);
    }

    return true;
}

bool ExynosCamera::m_autoFocusResetNotify(int focusMode)
{
    /* show restart */
    CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)", FOCUS_RESULT_RESTART, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_RESTART, 0, m_callbackCookie);

    /* show focusing */
    CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)", FOCUS_RESULT_FOCUSING, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_FOCUSING, 0, m_callbackCookie);

    return true;
}

bool ExynosCamera::m_autoFocusThreadFunc(void)
{
    CLOGI(" -IN-");

    bool afResult = false;
    int focusMode = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */
/* HACK : For front touch AF
    if (getCameraIdInternal() == CAMERA_ID_FRONT) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {
            if (m_notifyCb != NULL) {
                CLOGD("Do not support autoFocus in front camera.");
                m_notifyCb(CAMERA_MSG_FOCUS, true, 0, m_callbackCookie);
            } else {
                CLOGD("m_notifyCb is NULL!");
            }
        } else {
            CLOGD("autoFocus msg disabled !!");
        }
        return false;
    } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
*/
        if (getCameraIdInternal() == CAMERA_ID_BACK_1) {

        /*
         * enable AF for dual sensor camera.
         * when infinity, fixed : use old af blocking code.
         * other setting        : adjust af.
         */
        switch (m_parameters->getFocusMode()) {
        case FOCUS_MODE_INFINITY:
        case FOCUS_MODE_FIXED:
            if (m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {
                if (m_notifyCb != NULL) {
                    CLOGD("Do not support autoFocus in front camera.");
                    m_notifyCb(CAMERA_MSG_FOCUS, true, 0, m_callbackCookie);
                } else {
                    CLOGD("m_notifyCb is NULL!");
                }
            } else {
                CLOGD("autoFocus msg disabled !!");
            }
            return false;
        default:
            break;
        }
    }

    if (m_autoFocusType == AUTO_FOCUS_SERVICE) {
        focusMode = m_parameters->getFocusMode();
    } else if (m_autoFocusType == AUTO_FOCUS_HAL) {
        focusMode = FOCUS_MODE_AUTO;

        if (m_notifyCb != NULL) {
            m_autoFocusResetNotify(focusMode);
        }
    }

    m_autoFocusLock.lock();
    /* check early exit request */
    if (m_exitAutoFocusThread == true) {
        CLOGD("exiting on request");
        goto done;
    }

    m_autoFocusRunning = true;

    if (m_autoFocusRunning == true) {
        afResult = m_exynosCameraActivityControl->autoFocus(focusMode,
                                                            m_autoFocusType,
                                                            m_flagStartFaceDetection,
                                                            m_frameMetadata.number_of_faces);
        if (afResult == true)
            CLOGV("autoFocus Success!!");
        else
            CLOGV("autoFocus Fail !!");
    } else {
        CLOGV("autoFocus canceled !!");
    }

    /*
     * CAMERA_MSG_FOCUS only takes a bool.  true for
     * finished and false for failure.
     * If cancelAutofocus() called, no callback.
     */
    if ((m_autoFocusRunning == true) &&
        m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {

        if (m_notifyCb != NULL) {
            int afFinalResult = (int)afResult;

            CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)", afFinalResult, focusMode);
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
            m_autoFocusLock.lock();
            m_flagAFDone = true;
        } else {
            CLOGD("m_notifyCb is NULL mode(%d)", focusMode);
        }
    } else {
        CLOGV("autoFocus canceled, no callback !!");
    }

    autoFocusMgr->displayAFStatus();

    m_autoFocusRunning = false;

    CLOGV("exiting with no error");

done:
    m_autoFocusLock.unlock();

    CLOGI("end");

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
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        return false;
    }
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

    count = m_autoFocusContinousQ.getSizeOfProcessQ();
    if( count >= MAX_FOCUSCONTINUS_THREADQ_SIZE ) {
        for( uint32_t i = 0 ; i < count ; i++) {
            m_autoFocusContinousQ.popProcessQ(&frameCnt);
        }
        CLOGD("m_autoFocusContinousQ  skipped QSize(%d) frame(%d)",  count, frameCnt);
    }

    if(m_autoFocusRunning) {
        return true;
    }

    return true;
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
        if (m_parameters->isFlite3aaOtf() == true)
            bufMgrList[0] = NULL;
        else
            bufMgrList[0] = &m_3aaBufferMgr;

#ifdef DEBUG_RAWDUMP
        if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER)
            bufMgrList[1] = &m_fliteBufferMgr;
        else
#endif
            bufMgrList[1] = &m_bayerBufferMgr;
        break;
    case PIPE_VC0:
        bufMgrList[0] = NULL;
#if defined(DEBUG_RAWDUMP)
        if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER)
            bufMgrList[1] = &m_fliteBufferMgr;
        else
#endif
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
        if (m_parameters->isFlite3aaOtf() == false) {
            bufMgrList[0] = &m_bayerBufferMgr;
        } else {
            bufMgrList[0] = &m_3aaBufferMgr;
        }

        if (m_parameters->is3aaIspOtf() == true) {
            bufMgrList[1] = &m_scpBufferMgr;
        } else {
            bufMgrList[1] = &m_ispBufferMgr;
        }
        break;
    case PIPE_ISP:
        bufMgrList[0] = &m_ispBufferMgr;

        if (m_parameters->getTpuEnabledMode() == true
            && m_parameters->isIspTpuOtf() == false) {
            bufMgrList[1] = &m_hwDisBufferMgr;
        } else {
            bufMgrList[1] = &m_scpBufferMgr;
        }
        break;
    case PIPE_DIS:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;

        break;
    case PIPE_ISPC:
#if 0
    /* deperated these codes, there's no more scc, scp */
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_yuvBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
#endif
    case PIPE_MCSC1:
        bufMgrList[0] = NULL;
        if (m_parameters->getReprocessingMode() == PROCESSING_MODE_NON_REPROCESSING_YUV)
            bufMgrList[1] = &m_yuvBufferMgr;
        else
            bufMgrList[1] = &m_previewCallbackBufferMgr;
        break;
    case PIPE_GDC:
    case PIPE_GSC:
        bufMgrList[0] = &m_scpBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_TPU1:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_yuvReprocessingBufferMgr;
        break;
    case PIPE_GSC_PICTURE:
        bufMgrList[0] = &m_yuvBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_3AA_REPROCESSING:
        bufMgrList[0] = &m_bayerBufferMgr;
        if (m_parameters->isReprocessing3aaIspOTF() == false)
            bufMgrList[1] = &m_ispReprocessingBufferMgr;
        else
            bufMgrList[1] = &m_yuvReprocessingBufferMgr;
        break;
    case PIPE_ISP_REPROCESSING:
        if (m_parameters->getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER)
            bufMgrList[0] = &m_ispReprocessingBufferMgr;
        else
            bufMgrList[0] = &m_bayerBufferMgr;
        bufMgrList[1] = &m_yuvReprocessingBufferMgr;
        break;
    case PIPE_ISPC_REPROCESSING:
    case PIPE_SCC_REPROCESSING:
    case PIPE_MCSC3_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_yuvReprocessingBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING:
        bufMgrList[0] = &m_yuvReprocessingBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING2:
    case PIPE_MCSC1_REPROCESSING:
        bufMgrList[0] = &m_yuvReprocessingBufferMgr;
        bufMgrList[1] = &m_postPictureBufferMgr;
        break;
    case PIPE_GDC_PICTURE:
        bufMgrList[0] = &m_gscBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING3:
        bufMgrList[0] = &m_postPictureBufferMgr;
        bufMgrList[1] = &m_thumbnailGscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING4:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_yuvReprocessingBufferMgr;
        break;
    default:
        CLOGE(" Unknown pipeId(%d)", pipeId);
        bufMgrList[0] = NULL;
        bufMgrList[1] = NULL;
        ret = BAD_VALUE;
        break;
    }

    if (bufMgrList[direction] != NULL)
        *bufMgr = *bufMgrList[direction];

    return ret;
}

ExynosCameraFrameSelector::result_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer,
                                            ExynosCameraFrameSP_dptr_t frame,
                                            camera2_shot_ext *updateDmShot
#ifdef SUPPORT_DEPTH_MAP
                                            , ExynosCameraBuffer *depthMapbuffer
#endif
                                            )
{
    status_t ret = NO_ERROR;
    bool isSrc = false;

    int dstPos = 0;

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        dstPos = m_previewFrameFactory->getNodeType(PIPE_VC0);
        break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        dstPos = m_previewFrameFactory->getNodeType(PIPE_3AC);
        break;
    default:
        CLOGV("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    int funcRetryCount = 0;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraFrameSelector::result_t result;
    ExynosCameraBuffer *saveBuffer = NULL;
    ExynosCameraBuffer tempBuffer;
    ExynosRect srcRect, dstRect;
    entity_buffer_state_t buffer_state = ENTITY_BUFFER_STATE_ERROR;

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

BAYER_RETRY:
    if (m_stopLongExposure == true
        && m_parameters->getCaptureExposureTime() != 0) {
        CLOGD("Emergency stop");
        m_reprocessingCounter.clearCount();
        m_pictureEnabled = false;
        result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
        goto CLEAN;
    }

    m_captureSelector->setWaitTime(200000000);
    {
        result = m_captureSelector->selectFrames(bayerFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, dstPos);
    }

    switch (result) {
    case ExynosCameraFrameSelector::RESULT_NO_FRAME:
        CLOGE("bayerFrame is NULL");
        return result;
        break;
    case ExynosCameraFrameSelector::RESULT_SKIP_FRAME:
        CLOGW("skip bayerFrame");
        return result;
        break;
    case ExynosCameraFrameSelector::RESULT_HAS_FRAME:
        if (bayerFrame == NULL) {
            CLOGE("bayerFrame is NULL!!");
            goto CLEAN;
        }
        /* set the frame */
        frame = bayerFrame;
        break;
    default:
        CLOG_ASSERT("invalid select result!! result:%d", result);
        break;
    }

    ret = bayerFrame->getDstBuffer(pipeId, buffer, dstPos);
    if (ret < 0) {
        CLOGE(" getDstBuffer(pipeId(%d) dstPos(%d)) fail, ret(%d)", pipeId, dstPos, ret);
        result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
        goto CLEAN;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getDepthCallbackOnCapture() || m_parameters->getDepthCallbackOnPreview()) {
        ret = bayerFrame->getDstBuffer(pipeId, depthMapbuffer, m_previewFrameFactory->getNodeType(PIPE_VC1));
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDstBuffer. PIPE_%d, CAPTURE_NODE_%d", pipeId, m_previewFrameFactory->getNodeType(PIPE_VC1));
        }

        if (!(m_parameters->getDepthCallbackOnCapture() && m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21)
            && m_parameters->getDepthCallbackOnPreview()
            && depthMapbuffer->index != -2 && m_depthMapBufferMgr != NULL) {
            CLOGD("put depthMapbuffer->index(%d)", depthMapbuffer->index);
            m_putBuffers(m_depthMapBufferMgr, depthMapbuffer->index);
            depthMapbuffer->index = -2;
        }
    }
#endif

    if (updateDmShot == NULL) {
        CLOGE("updateDmShot is NULL");
        ret = BAD_VALUE;
        result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
        goto CLEAN;
    }

    retryCount = 12; /* 30ms * 12 */
    while (retryCount > 0) {
        if (bayerFrame->getMetaDataEnable() == false)
            CLOGW("Waiting for update jpeg metadata, retryCount(%d)", retryCount);
        else
            break;

        retryCount--;
        usleep(DM_WAITING_TIME);
    }

    ret = bayerFrame->getSrcBufferState(PIPE_3AA, &buffer_state);
    if (ret != NO_ERROR)
        CLOGE("get 3AA Src buffer state fail, ret(%d)", ret);

    if (buffer_state == ENTITY_BUFFER_STATE_ERROR
        || bayerFrame->getMetaDataEnable() == false) {
        /* Return buffer & delete frame */
        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("searchFrameFromList fail");
            } else {
                CLOGD("Selected frame(%d) is not available! buffer_state(%d)",
                         bayerFrame->getFrameCount(), buffer_state);
                bayerFrame = NULL;
                result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
            }
        }
        goto BAYER_RETRY;
    }

    /* update metadata */
    bayerFrame->getUserDynamicMeta(updateDmShot);
    bayerFrame->getDynamicMeta(updateDmShot);
    shot_ext = updateDmShot;

    CLOGD("Selected frame count(hal : %d / driver : %d)",
            bayerFrame->getFrameCount(), updateDmShot->shot.dm.request.frameCount);

    if (m_parameters->getCaptureExposureTime() != 0
        && m_parameters->getLongExposureTime() != shot_ext->shot.dm.sensor.exposureTime / 1000) {
        funcRetryCount++;

        if (m_longExposureRemainCount < 3
            && funcRetryCount - (int)m_longExposureRemainCount >= 3)
            m_parameters->setPreviewExposureTime();

        /* Return buffer & delete frame */
        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

#ifdef SUPPORT_DEPTH_MAP
        if (depthMapbuffer->index != -2 && m_depthMapBufferMgr != NULL)
            m_putBuffers(m_depthMapBufferMgr, depthMapbuffer->index);
#endif

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("searchFrameFromList fail");
            } else {
                CLOGD(" Selected frame(%d, %d msec) is not same with set from user.",
                         bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
                bayerFrame = NULL;
                result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
            }
        }
        goto BAYER_RETRY;
    }

    if (m_parameters->getCaptureExposureTime() != 0
        && m_longExposureRemainCount > 0) {
        if (saveBuffer == NULL) {
            saveBuffer = buffer;
            buffer = &tempBuffer;
            tempBuffer.index = -2;

            if (bayerFrame != NULL) {
                bayerFrame->frameUnlock();
                ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), inListFrame, &m_processListLock);
                if (ret < 0) {
                    CLOGE("searchFrameFromList fail");
                } else {
                    bayerFrame = NULL;
                    result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
                }
            }
            goto BAYER_RETRY;
        }


#ifdef CAMERA_PACKED_BAYER_ENABLE
        ret = addBayerBuffer(buffer, saveBuffer, &dstRect, true);
#else
        ret = addBayerBuffer(buffer, saveBuffer, &dstRect);
#endif
        if (ret < 0) {
            CLOGE("addBayerBufferPacked() fail");
        }

        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("searchFrameFromList fail");
            } else {
                CLOGD("Selected frame(%d, %d msec) delete.",
                         bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
            }
        }

        m_longExposureRemainCount--;
        if (m_longExposureRemainCount > 0) {
            if (m_longExposureRemainCount < 3) {
                m_parameters->setPreviewExposureTime();
            }
            goto BAYER_RETRY;
        }

        buffer = saveBuffer;
        saveBuffer = NULL;

#ifdef CAMERA_ADD_BAYER_ENABLE
        if (m_ionClient >= 0)
            ion_sync_fd(m_ionClient, buffer->fd[0]);
#endif
    }

    if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        if (m_pictureThread->isRunning() == false) {
            ret = m_pictureThread->run();
            if (ret < 0) {
                CLOGE("couldn't run picture thread, ret(%d)", ret);
                return ExynosCameraFrameSelector::RESULT_NO_FRAME;
            }
        }

        if (m_jpegCallbackThread->isRunning() == false) {
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("couldn't run jpeg callback thread, ret(%d)", ret);
                return ExynosCameraFrameSelector::RESULT_NO_FRAME;
            }
        }
    }

    m_parameters->setPreviewExposureTime();

CLEAN:
    if (saveBuffer != NULL
        && saveBuffer->index != -2
        && m_bayerBufferMgr != NULL) {
        m_putBuffers(m_bayerBufferMgr, saveBuffer->index);
        saveBuffer->index = -2;
    }

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        int funcRet = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), inListFrame, &m_processListLock);
        if (funcRet < 0) {
            CLOGE("searchFrameFromList fail");
        } else {
            CLOGD(" Selected frame(%d) complete, Delete", bayerFrame->getFrameCount());
            bayerFrame = NULL;
        }
    }

    return result;
}

/* vision */
status_t ExynosCamera::m_startVisionInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("IN");

    uint32_t minFrameNum = 0;
    int ret = 0;
    int pipeId = PIPE_FLITE;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    ExynosCameraBufferManager *fliteBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        fliteBufferManager[i] = NULL;
    }

    m_displayPreviewToggle = 1;
    m_fdCallbackToggle = 0;

    minFrameNum = m_exynosconfig->current->pipeInfo.prepare[pipeId];

    ret = m_visionFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("m_visionFrameFactory->initPipes() failed");
        return ret;
    }

    {
        m_visionFps = 10;
        m_visionAe = 0x2A;

        ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, m_visionFps, pipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);

        CLOGD("m_visionFps(%d)", m_visionFps);

        ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_AE_TARGET, m_visionAe, pipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);

        CLOGD("m_visionAe(%d)", m_visionAe);
    }

    tempBufferManager = fliteBufferManager;

    tempBufferManager[m_visionFrameFactory->getNodeType(PIPE_FLITE)] = m_3aaBufferMgr;
    tempBufferManager[m_visionFrameFactory->getNodeType(PIPE_VC0)] = m_bayerBufferMgr;

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (fliteBufferManager[i] != NULL) {
            ret = m_visionFrameFactory->setBufferManagerToPipe(fliteBufferManager, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("m_visionFrameFactory->setBufferManagerToPipe(fliteBufferManager, %d) failed", pipeId);
                return ret;
            }
            break;
        }
    }

    for (uint32_t i = 0; i < minFrameNum; i++) {
        ret = m_generateFrameVision(-1, newFrame);
        if (ret < 0) {
            CLOGE("generateFrame fail");
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("new faame is NULL");
            return ret;
        }

        m_setupEntity(pipeId, newFrame);
        m_visionFrameFactory->setFrameDoneQToPipe(m_mainSetupQ[INDEX(pipeId)], pipeId);
        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, pipeId);
        m_visionFrameFactory->pushFrameToPipe(newFrame, pipeId);
    }

    /* prepare pipes */
    ret = m_visionFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("preparePipe fail");
        return ret;
    }

    /* stream on pipes */
    ret = m_visionFrameFactory->startPipes();
    if (ret < 0) {
        CLOGE("startPipe fail");
        return ret;
    }

    /* start all thread */
    ret = m_visionFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("startInitialThreads fail");
        return ret;
    }

    m_previewEnabled = true;
    m_parameters->setPreviewRunning(m_previewEnabled);

    CLOGI("OUT");

    return NO_ERROR;
}

status_t ExynosCamera::m_stopVisionInternal(void)
{
    int ret = 0;
    int pipeId = PIPE_FLITE;

    CLOGI("IN");

    ret = m_visionFrameFactory->stopPipes();
    if (ret < 0) {
        CLOGE("stopPipe fail");
        return ret;
    }

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        m_mainSetupQ[i]->release();
    }

    CLOGD("clear process Frame list");
    ret = m_clearList(&m_processList, &m_processListLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
        return ret;
    }

    m_clearList(m_pipeFrameVisionDoneQ);

    m_previewEnabled = false;
    m_parameters->setPreviewRunning(m_previewEnabled);

    CLOGI("OUT");

    return NO_ERROR;
}

status_t ExynosCamera::m_generateFrameVision(int32_t frameCount, ExynosCameraFrameSP_dptr_t newFrame)
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
        newFrame = m_visionFrameFactory->createNewFrame();

        if (newFrame == NULL) {
            CLOGE(" newFrame is NULL");
            return UNKNOWN_ERROR;
        }

        ret = m_insertFrameToList(&m_processList, newFrame, &m_processListLock);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to insert frame to m_processList",
                    newFrame->getFrameCount());
            return ret;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setVisionBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("alloc buffer - scenario: %d", m_scenario);
    int ret = 0;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int hwPreviewW, hwPreviewH;
    int hwSensorW, hwSensorH;
    int hwPictureW, hwPictureH;
    int batchSize = 1;

    int previewMaxW, previewMaxH;
    int sensorMaxW, sensorMaxH;

    int planeCount  = 2;
    int minBufferCount = 1;
    int maxBufferCount = 1;

    exynos_camera_buffer_type_t type;

    planeCount      = 2;
    planeSize[0]    = 32 * 64 * 2;
    batchSize       = 1;

    maxBufferCount = FRONT_NUM_BAYER_BUFFERS;

    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, batchSize, true, false);
    if (ret != NO_ERROR) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            maxBufferCount);
        return ret;
    }

    {
        maxBufferCount = FRONT_NUM_BAYER_BUFFERS;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
        planeSize[0] = VISION_WIDTH * VISION_HEIGHT;

        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                                maxBufferCount, maxBufferCount, 1 /* BathSize */, type, true, true);
        if (ret < 0) {
            CLOGE("bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    }

    CLOGI("alloc buffer done - scenario: %d", m_scenario);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVisionCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    int planeCount = 1;
    int batchSize   = 1;
    int bufferCount = FRONT_NUM_BAYER_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    m_parameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_parameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    {
        bufferCount = FRONT_NUM_BAYER_BUFFERS;
        planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    }

    ret = m_allocBuffers(m_previewCallbackBufferMgr, planeCount, planeSize, bytesPerLine,
                            bufferCount, bufferCount, batchSize, type, false, true);
    if (ret < 0) {
        CLOGE("m_previewCallbackBufferMgr m_allocBuffers(bufferCount=%d) fail",
             bufferCount);
        return ret;
    }
    CLOGI("alloc preview callback buffer done - scenario: %d",
        m_scenario);

    return NO_ERROR;
}

bool ExynosCamera::m_visionThreadFunc(void)
{
    int ret = 0;
    int index = 0;

    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t handleFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer buffer;
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
    int shutterSpeed = 0;
    int gain = 0;
    int irLedWidth = 0;
    int irLedDelay = 0;
    int irLedCurrent = 0;
    int irLedOnTime = 0;
    int internalValue = 0;

    if (m_previewEnabled == false) {
        CLOGD("preview is stopped, thread stop");
        return false;
    }

    ret = m_pipeFrameVisionDoneQ->waitAndPopProcessQ(&handleFrame);
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

    if (handleFrame == NULL) {
        CLOGE("handleFrame is NULL");
        return true;
    }

    /* handle vision frame */
    entity = handleFrame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("current entity is NULL");
        /* TODO: doing exception handling */
        return true;
    }

    pipeID = entity->getPipeId();

    switch(entity->getPipeId()) {
    case PIPE_FLITE:
        m_parameters->getFrameSkipCount(&frameSkipCount);

        if (handleFrame->getRequest(PIPE_VC0) == true) {
            ret = handleFrame->getDstBuffer(entity->getPipeId(), &bayerBuffer, m_visionFrameFactory->getNodeType(PIPE_VC0));
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                return ret;
            }

            if (0 <= bayerBuffer.index) {
                if (entity->getDstBufState(m_visionFrameFactory->getNodeType(PIPE_VC0)) == ENTITY_BUFFER_STATE_COMPLETE) {
#ifdef VISION_DUMP
                    char filePath[50];
                    snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Camera/vision%02d.raw", dumpIndex);
                    CLOGD("vision dump %s", filePath);
                    dumpToFile(filePath, (char *)bayerBuffer.addr[0], VISION_WIDTH * VISION_HEIGHT);

                    dumpIndex ++;
                    if (dumpIndex > 4)
                        dumpIndex = 0;
#endif
                    if (frameSkipCount > 0) {
                        CLOGD("frameSkipCount(%d)", frameSkipCount);
                    } else {
                        /* callback */
                        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME))
                        {
                            if (m_displayPreviewToggle) {
                                ExynosCameraBuffer previewCbBuffer;
                                camera_memory_t *previewCallbackHeap = NULL;
                                char *srcAddr = NULL;
                                char *dstAddr = NULL;
                                int bufIndex = -2;

                                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                                previewCallbackHeap = m_getMemoryCb(previewCbBuffer.fd[0], previewCbBuffer.size[0], 1, m_callbackCookie);
                                if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
                                    CLOGE("m_getMemoryCb(fd:%d, size:%d) fail",
                                    previewCbBuffer.fd[0], previewCbBuffer.size[0]);

                                    return INVALID_OPERATION;
                                }
                                memcpy(previewCallbackHeap->data, bayerBuffer.addr[0], previewCbBuffer.size[0]);

                                setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);
                                m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
                                clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);

                                previewCallbackHeap->release(previewCallbackHeap);

                                m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                            }
                        }
                    }
                }

                ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("m_putBuffers(bufferMgr, %d) fail", bayerBuffer.index);
                }
            } else {
                CLOGW("buffer is empty frame(%d) pipe(%d)", handleFrame->getFrameCount(), pipeID);
            }
        } else {
            CLOGE("reqeust failed frame(%d) pipe(%d) request(%d)", handleFrame->getFrameCount(), pipeID, PIPE_VC0);
        }
        break;
    default:
        break;
    }

    ret = handleFrame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    CLOGD("frame complete, count(%d)", handleFrame->getFrameCount());
    ret = m_removeFrameFromList(&m_processList, handleFrame, &m_processListLock);
    if (ret < 0) {
        CLOGE("remove frame from processList fail, ret(%d)", ret);
    }
    handleFrame = NULL;

    {
        fps = m_parameters->getVisionModeFps();
        if (m_visionFps != fps) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, fps, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_visionFps = fps;
            CLOGD("(%d)(%d)", m_visionFps, fps);
        }

        ae = m_parameters->getVisionModeAeTarget();
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
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_visionAe = ae;
            CLOGD("(%d)(%d)", m_visionAe, internalValue);
        }
    }
    return true;
}

status_t ExynosCamera::m_startCompanion(void)
{
    return NO_ERROR;
}

status_t ExynosCamera::m_stopCompanion(void)
{
    return NO_ERROR;
}

status_t ExynosCamera::m_waitCompanionThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

    timer.stop();
    CLOGD("companionThread joined, waiting time : duration time(%5d msec)", (int)timer.durationMsecs());

    return NO_ERROR;
}

void ExynosCamera::m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext) {
    uint32_t data = 0;

    if (m_checkFirstFrameLux == false || m_parameters->getDualMode() == true ||
        m_parameters->getRecordingHint() == true) {
        m_checkFirstFrameLux = false;
        return;
    }

    data = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[399];

    if (data <= ENTRANCE_LOW_LUX) {
        CLOGD(" need skip frame for ae/awb stable(%d).", data);
        m_parameters->setFrameSkipCount(2);
    }
    m_checkFirstFrameLux = false;
}

status_t ExynosCamera::m_copyMetaFrameToFrame(ExynosCameraFrameSP_sptr_t srcframe, ExynosCameraFrameSP_sptr_t dstframe, bool useDm, bool useUdm)
{
    Mutex::Autolock lock(m_metaCopyLock);
    status_t ret = NO_ERROR;

    memset(m_tempshot, 0x00, sizeof(struct camera2_shot_ext));
    if(useDm) {
        ret = srcframe->getDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("getDynamicMeta fail, ret(%d)", ret);

        ret = dstframe->storeDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("storeDynamicMeta fail, ret(%d)", ret);
    }

    if(useUdm) {
        ret = srcframe->getUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("getUserDynamicMeta fail, ret(%d)", ret);

        ret = dstframe->storeUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("storeUserDynamicMeta fail, ret(%d)", ret);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_startPreviewInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("IN");

    uint32_t minBayerFrameNum = 0;
    uint32_t min3AAFrameNum = 0;
    int ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    ExynosCameraBufferManager *scpBufferMgr = NULL;
    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();
    enum pipeline pipe;
    int pipeId = PIPE_FLITE;

    m_displayPreviewToggle = 0;
    m_fdCallbackToggle = 0;

    minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;

    /*
     * with MCPipe, we need to putBuffer 3 buf.
     */
    /*
    if (reprocessingMode == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON)
        min3AAFrameNum = minBayerFrameNum;
    else
        min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    */

    min3AAFrameNum = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];

    ExynosCameraBufferManager *fliteBufferManager[MAX_NODE];
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *disBufferManager[MAX_NODE];
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        fliteBufferManager[i] = NULL;
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
    }

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    /* Set capture src image buffer request */
    m_previewFrameFactory->setRequest(PIPE_VC0, false);
    m_previewFrameFactory->setRequest(PIPE_3AC, false);

    if (m_parameters->isDynamicCapture() == false) {
        switch (reprocessingMode) {
        case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
            m_previewFrameFactory->setRequest(PIPE_VC0, true);
            break;
        case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
            m_previewFrameFactory->setRequest(PIPE_3AC, true);
            break;
        case PROCESSING_MODE_NON_REPROCESSING_YUV:
            m_previewFrameFactory->setRequest(PIPE_MCSC1, true);
            break;
        default:
            CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
            break;
        }
    }

    if (m_parameters->isFlite3aaOtf() == false) {
        m_previewFrameFactory->setRequestBayer(true);
    }

#ifdef SUPPORT_DEPTH_MAP
    m_previewFrameFactory->setRequest(PIPE_VC1, false);
    m_parameters->setDisparityMode(COMPANION_DISPARITY_OFF);

    if (m_parameters->getUseDepthMap()
        && m_parameters->getPaf() == COMPANION_PAF_ON
        && m_parameters->getRecordingHint() == false
        && (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS
            )) {
        if (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS) {
            m_previewFrameFactory->setRequest(PIPE_VC1, true);
            m_parameters->setDepthCallbackOnCapture(true);
            m_parameters->setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER);
            CLOGI("[Depth] Enable depth callback on capture");
        }
    }
#endif

    if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false) {
        if (m_parameters->getTpuEnabledMode() == true) {
            m_previewFrameFactory->setRequestISPC(true);
            m_previewFrameFactory->setRequestISPP(false);
            //m_previewFrameFactory->setRequestDIS(true);
        } else {
            m_previewFrameFactory->setRequestISPC(false);
            m_previewFrameFactory->setRequestISPP(false);
            //m_previewFrameFactory->setRequestDIS(false);
        }
    } else {
        m_previewFrameFactory->setRequestISPC(false);
        m_previewFrameFactory->setRequestISPP(false);
    }

    /* In Case of Preview of Dual Mode,
           RearCamera : PIEP_MCSC0 -> mcsc0 node / PIPE_MCSC1 -> mcsc1 node
           FrontCamera : PIEP_MCSC0 -> mcsc2 node / PIPE_MCSC1 -> mcsc4 node

           In Case of Capture of Dual Mode,
           Reprocessing : mcsc3 node for Main Image / mcsc4 node for Thumbnail Image

           Enabling DMA Out of PIPE_MCSC1 can cause conflict between capture scenario and preview scenario.
           so, we can't used it on dual mode.
    */
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    if ((m_parameters->getHighResolutionCallbackMode() == true)
        || (m_parameters->getDualMode() == true)) {
        m_previewFrameFactory->setRequest(PIPE_MCSC1, false);
    } else {
        m_previewFrameFactory->setRequest(PIPE_MCSC1, true);
    }
#endif

    ret = m_previewFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("m_previewFrameFactory->initPipes() failed");
        return ret;
    }

    {
        scpBufferMgr = m_scpBufferMgr;
    }

    if (m_parameters->isFlite3aaOtf() == true)  {
        tempBufferManager = taaBufferManager;

        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_FLITE)] = m_3aaBufferMgr;
    } else {
        tempBufferManager = fliteBufferManager;

        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_FLITE)] = m_3aaBufferMgr;
    }

#ifdef DEBUG_RAWDUMP
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;
#else
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_bayerBufferMgr;
#endif // DEBUG_RAWDUMP

#ifdef SUPPORT_DEPTH_MAP
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC1)] = m_depthMapBufferMgr;
#endif // SUPPORT_DEPTH_MAP

    tempBufferManager = taaBufferManager;

    if (m_parameters->isFlite3aaOtf() == false)
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_bayerBufferMgr;
    else
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;

    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AC)] = m_bayerBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AP)] = m_ispBufferMgr;

    if (m_parameters->is3aaIspOtf() == false) {
        tempBufferManager = ispBufferManager;
    }

    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISP)] = m_ispBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPC)] = m_hwDisBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_ISPP)] = m_hwDisBufferMgr;

    if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false) {
        if (m_parameters->getTpuEnabledMode() == true) {
            tempBufferManager = disBufferManager;
            tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_TPU)] = m_hwDisBufferMgr;
        } else {
            tempBufferManager = mcscBufferManager;
        }
    }

    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC)] = m_hwDisBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = scpBufferMgr;

    if (m_parameters->getReprocessingMode() == PROCESSING_MODE_NON_REPROCESSING_YUV)
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC1)] = m_yuvBufferMgr;
    else
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC1)] = m_previewCallbackBufferMgr;

    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC2)] = m_recordingBufferMgr;

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (fliteBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(fliteBufferManager, PIPE_FLITE);
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                    PIPE_FLITE);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (taaBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA);
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                     PIPE_3AA);
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
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                     PIPE_ISP);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (disBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, PIPE_DIS);
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, %d) failed",
                     PIPE_DIS);
                return ret;
            }
            break;
        }
    }

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (mcscBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(mcscBufferManager, PIPE_MCSC);
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, %d) failed",
                     PIPE_DIS);
                return ret;
            }
            break;
        }
    }

    int flipHorizontal = m_parameters->getFlipHorizontal();

    CLOGD("set FlipHorizontal on preview, FlipHorizontal(%d)", flipHorizontal);

    int pipeForFlip = PIPE_ISP;
    if (IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_MCSC)) {
        pipeForFlip = PIPE_MCSC;
    } else {
        if (m_parameters->isTpuMcscOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_TPU))
            pipeForFlip = PIPE_TPU;
        else if (m_parameters->isIspTpuOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_ISP))
            pipeForFlip = PIPE_ISP;
        else if (m_parameters->isIspMcscOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_ISP))
            pipeForFlip = PIPE_ISP;
        else
            pipeForFlip = PIPE_3AA;
    }

    if (m_parameters->getNumOfMcscOutputPorts() > 2) {
        enum NODE_TYPE nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC2);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeForFlip, nodeType);
    }

    ret = m_previewFrameFactory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("m_previewFrameFactory->mapBuffers() failed");
        return ret;
    }

    m_printExynosCameraInfo(__FUNCTION__);

    pipeId = PIPE_FLITE;

    if (m_parameters->isFlite3aaOtf() == false) {
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        m_previewFrameFactory->setFrameDoneQToPipe(m_mainSetupQ[INDEX(pipeId)], pipeId);
    }

    pipeId = PIPE_3AA;

    if (m_parameters->is3aaIspOtf() == false
        || (m_parameters->isIspTpuOtf() == false
            && m_parameters->isIspMcscOtf() == false)) {
        if (m_parameters->isFlite3aaOtf() == true)
            m_previewFrameFactory->setFrameDoneQToPipe(m_mainSetupQ[INDEX(pipeId)], pipeId);
        else
            m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId);
    }

    if (m_parameters->is3aaIspOtf() == false)
        pipeId = PIPE_ISP;

    if (m_parameters->isIspTpuOtf() == false
        && m_parameters->isIspMcscOtf() == false) {
        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId);

        if (m_parameters->getTpuEnabledMode() == true)
            pipeId = PIPE_TPU;
        else
            pipeId = PIPE_MCSC;
    }

    if (m_parameters->getTpuEnabledMode() == true
        && m_parameters->isTpuMcscOtf() == false) {
        m_previewFrameFactory->setFrameDoneQToPipe(m_pipeFrameDoneQ, pipeId);
        pipeId = PIPE_MCSC;
    }

    if (pipeId == PIPE_3AA
        && m_parameters->isFlite3aaOtf() == true) {
        if (m_parameters->isUseEarlyFrameReturn() == true) {
            m_previewFrameFactory->setFrameDoneQToPipe(m_mainSetupQ[INDEX(pipeId)], pipeId);
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        } else {
            m_previewFrameFactory->setOutputFrameQToPipe(m_mainSetupQ[INDEX(pipeId)], pipeId);
        }
    } else {
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
    }

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
        }

        if (m_parameters->isFlite3aaOtf() == true) {
            ret = m_setupEntity(PIPE_3AA, newFrame);
            if (ret != NO_ERROR)
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                        PIPE_FLITE, ret);

            m_previewFrameFactory->pushFrameToPipe(newFrame, PIPE_3AA);
        }

#if 0
        /* SCC */
        if(m_parameters->isReprocessingCapture() == false) {
            if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
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
        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
*/
    }

/* Comment out, because it included ISP */
/*
    for (uint32_t i = minFrameNum; i < INIT_SCP_BUFFERS; i++) {
        ret = m_generateFrame(-1, &newFrame);
        if (ret < 0) {
            CLOGE("generateFrame fail");
            return ret;
        }
        if (newFrame == NULL) {
            CLOGE("new faame is NULL");
            return ret;
        }

        m_setupEntity(PIPE_SCP, newFrame);
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, PIPE_SCP);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_SCP);
    }
*/

    /* prepare pipes */
    ret = m_previewFrameFactory->preparePipes();
    if (ret < 0) {
        CLOGE("preparePipe fail");
        return ret;
    }

    /* stream on pipes */
    ret = m_previewFrameFactory->startPipes();
    if (ret < 0) {
        m_previewFrameFactory->stopPipes();
        CLOGE("startPipe fail");
        return ret;
    }

    /* start all thread */
    ret = m_previewFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("startInitialThreads fail");
        return ret;
    }

    m_previewEnabled = true;
    m_parameters->setPreviewRunning(m_previewEnabled);

    if (m_parameters->getFocusModeSetting() == true) {
        int focusmode = m_parameters->getFocusMode();
        m_exynosCameraActivityControl->setAutoFocusMode(focusmode);
        m_parameters->setFocusModeSetting(false);
    }

    CLOGI("OUT");

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPreviewInternal(void)
{
    int ret = 0;

    CLOGI("IN");

    if (m_previewFrameFactory != NULL) {
        ret = m_previewFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("stopPipe fail");
            return ret;
        }
    }

    /* MCPipe send frame to mainSetupQ directly, must use it */
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        m_mainSetupQ[i]->release();
    }

    /* clear previous preview frame */
    CLOGD("clear process Frame list");
    ret = m_clearList(&m_processList, &m_processListLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
        return ret;
    }

    /* clear previous recording frame */
    CLOGD("Recording m_recordingProcessList(%d) IN",
            (int)m_recordingProcessList.size());
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
    }
    CLOGD("Recording m_recordingProcessList(%d) OUT",
            (int)m_recordingProcessList.size());

    m_pipeFrameDoneQ->release();

    m_isZSLCaptureOn = false;

    m_previewEnabled = false;
    m_parameters->setPreviewRunning(m_previewEnabled);

    CLOGI("OUT");

    return NO_ERROR;
}

status_t ExynosCamera::m_restartPreviewInternal(bool flagUpdateParam, CameraParameters *params)
{
    CLOGI(" Internal restart preview");
    int ret = 0;
    int err = 0;
    int reservedMemoryCount = 0;
    int reprocessingHeadPipeId = -1;

    m_flagThreadStop = true;
    m_disablePreviewCB = true;

    m_startPictureInternalThread->join();

    m_startPictureBufferThread->join();

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->setStopFlag();

    m_mainThread->requestExitAndWait();

    ret = m_stopPictureInternal();
    if (ret < 0)
        CLOGE("m_stopPictureInternal fail");

    m_gdcThread->stop();
    m_gdcQ->sendCmd(WAKE_UP);
    m_gdcThread->requestExitAndWait();

    m_zoomPreviwWithCscThread->stop();
    m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
    m_zoomPreviwWithCscThread->requestExitAndWait();

    m_previewThread->stop();
    if(m_previewQ != NULL)
        m_previewQ->sendCmd(WAKE_UP);
    m_previewThread->requestExitAndWait();

    if (m_parameters->isFlite3aaOtf() == true) {
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
    } else {
        if (m_mainSetupQThread[INDEX(PIPE_FLITE)] != NULL) {
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->stop();
            m_mainSetupQ[INDEX(PIPE_FLITE)]->sendCmd(WAKE_UP);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->requestExitAndWait();
            m_clearList(m_mainSetupQ[INDEX(PIPE_FLITE)]);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->release();
        }
    }

    ret = m_stopPreviewInternal();
    if (ret < 0) {
        CLOGE("m_stopPreviewInternal fail");
        err = ret;
    }

    if (m_previewQ != NULL) {
        m_clearList(m_previewQ);
    }

    /* check reserved memory count */
    if (m_bayerBufferMgr != NULL && m_bayerBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    if (m_ispBufferMgr != NULL && m_ispBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    if (m_jpegBufferMgr != NULL && m_jpegBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    /* skip to free and reallocate buffers */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (m_bayerBufferMgr != NULL && reservedMemoryCount > 0) {
        m_bayerBufferMgr->deinit();
    }
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        m_depthMapBufferMgr->deinit();
    }
#endif
    if (m_3aaBufferMgr != NULL) {
        m_3aaBufferMgr->resetBuffers();
    }
    if (m_ispBufferMgr != NULL) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        m_ispBufferMgr->deinit();
#else
        m_ispBufferMgr->resetBuffers();
        if (reservedMemoryCount > 0) {
            m_ispBufferMgr->deinit();
        }
#endif
    }
    if (m_hwDisBufferMgr != NULL) {
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
        m_hwDisBufferMgr->deinit();
#else
        m_hwDisBufferMgr->resetBuffers();
#endif
    }
    if (m_yuvBufferMgr != NULL) {
        m_yuvBufferMgr->resetBuffers();
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->resetBuffers();
    }

    /* skip to free and reallocate buffers */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_yuvReprocessingBufferMgr != NULL) {
        m_yuvReprocessingBufferMgr->deinit();
    }

    if (m_postPictureBufferMgr != NULL) {
        if (m_postPictureBufferMgr->getContigBufCount() != 0) {
            m_postPictureBufferMgr->deinit();
        } else {
            m_postPictureBufferMgr->resetBuffers();
        }
    }

    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL && reservedMemoryCount > 0) {
        m_jpegBufferMgr->deinit();
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

    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }

    if (m_bayerCaptureSelector != NULL) {
        m_bayerCaptureSelector->release();
    }

    if (m_yuvCaptureSelector != NULL) {
        m_yuvCaptureSelector->release();
    }

    if( m_parameters->getHighSpeedRecording() && m_parameters->getReallocBuffer() ) {
        CLOGD(" realloc buffer all buffer deinit ");
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
#if defined(DEBUG_RAWDUMP)
        if (m_parameters->getReprocessingMode() != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            if (m_fliteBufferMgr != NULL) {
                m_fliteBufferMgr->deinit();
            }
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

        if (m_yuvBufferMgr != NULL) {
            m_yuvBufferMgr->deinit();
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

    m_flagThreadStop = false;

    if (flagUpdateParam == true && params != NULL)
        m_parameters->setParameters(*params);

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        CLOGE("setPreviewWindow fail");
        err = ret;
    }

    CLOGI("setBuffersThread is run");
    m_setBuffersThread->run(PRIORITY_DEFAULT);

    if (m_parameters->checkEnablePicture() == true) {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);

    }

    m_setBuffersThread->join();

    if (m_isSuccessedBufferAllocation == false) {
        CLOGE("m_setBuffersThread() failed");
        err = INVALID_OPERATION;
    }

    m_startPictureBufferThread->join();

    if (m_parameters->isReprocessingCapture() == true
        && m_parameters->checkEnablePicture() == true) {
#ifdef START_PICTURE_THREAD
        if (m_parameters->getDualMode() != true) {
            m_startPictureInternalThread->run(PRIORITY_DEFAULT);
        } else
#endif
        {
            m_startPictureInternal();
        }
    } else {
        m_pictureFrameFactory = m_previewFrameFactory;
        CLOGD("FrameFactory(pictureFrameFactory) created");
    }

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("m_startPreviewInternal fail");
        err = ret;
    }

    /* setup frame thread */
    if (m_parameters->isFlite3aaOtf() == false) {
        CLOGD("setupThread Thread start pipeId(%d)", PIPE_FLITE);
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
    } else {
        CLOGD("setupThread Thread start pipeId(%d)", PIPE_3AA);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
    }

    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    if (m_monitorThread->isRunning() == false)
        m_monitorThread->run(PRIORITY_DEFAULT);

    m_disablePreviewCB = false;

    m_previewThread->run(PRIORITY_DISPLAY);

    m_mainThread->run(PRIORITY_DEFAULT);
    m_startPictureInternalThread->join();

    if (m_parameters->getGDCEnabledMode() == true) {
        CLOGD("GDC Thread start");
        m_gdcThread->run(PRIORITY_DEFAULT);
    }

    /* On High-resolution callback scenario, the reprocessing works with specific fps.
       To avoid the thread creation performance issue, threads in reprocessing pipes
       must be run with run&wait mode */
    if (m_parameters->isReprocessingCapture() == true) {
        bool needOneShotMode = (m_parameters->getHighResolutionCallbackMode() == false);
        enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_3AA_REPROCESSING, needOneShotMode);
        }

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
            || m_parameters->isReprocessing3aaIspOTF() == false) {
            m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_ISP_REPROCESSING, needOneShotMode);
        }
    }

    return err;
}

status_t ExynosCamera::m_startPictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    int thumbnailW, thumbnailH;
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;
    int mcscContainedPipeId = -1;

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    /* get the pipeId to have mcsc */
    if (m_parameters->isReprocessingIspMcscOtf() == false)
        mcscContainedPipeId = PIPE_MCSC_REPROCESSING;
    else if (m_parameters->isReprocessing3aaIspOTF() == false)
        mcscContainedPipeId = PIPE_ISP_REPROCESSING;
    else
        mcscContainedPipeId = PIPE_3AA_REPROCESSING;

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
    }

    if (m_zslPictureEnabled == true) {
        CLOGD(" zsl picture is already initialized");
        return NO_ERROR;
    }

    if (m_parameters->isReprocessingCapture() == true) {
        m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);
        if (thumbnailW > 0 && thumbnailH > 0) {
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
        } else {
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
        }

        ret = m_setReprocessingBuffer();
        if (ret != NO_ERROR) {
            CLOGE("m_setReprocessing Buffer fail");
            return ret;
        }

        if (m_reprocessingFrameFactory->isCreated() == false) {
            ret = m_reprocessingFrameFactory->create();
            if (ret != NO_ERROR) {
                CLOGE("m_reprocessingFrameFactory->create() failed");
                return ret;
            }

            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("FrameFactory(pictureFrameFactory) created");
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("FrameFactory(pictureFrameFactory) created");
        }

        int flipHorizontal = m_parameters->getFlipHorizontal();

        CLOGD("set FlipHorizontal on capture, FlipHorizontal(%d)", flipHorizontal);

        enum NODE_TYPE nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

        /* If we want set buffer namanger from m_getBufferManager, use this */
#if 0
        ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)], SRC_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager() fail, pipeId(%d), ret(%d)", PIPE_3AA_REPROCESSING, ret);
            return ret;
        }

        ret = m_getBufferManager(PIPE_3AA_REPROCESSING, bufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)], DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager() fail, pipeId(%d), ret(%d)", PIPE_3AP_REPROCESSING, ret);
            return ret;
        }
#else
        tempBufferManager = taaBufferManager;

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
            tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;
        }

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
            || m_parameters->isReprocessing3aaIspOTF() == false)
            tempBufferManager = ispBufferManager;

        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_yuvReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING)] = m_postPictureBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING)] = m_yuvReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING)] = m_thumbnailBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = m_yuvReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = m_thumbnailBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)] = m_jpegBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)] = m_thumbnailBufferMgr;
#endif

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            for (int i = 0; i < MAX_NODE; i++) {
                /* If even one buffer slot is valid. call setBufferManagerToPipe() */
                if (taaBufferManager[i] != NULL) {
                    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA_REPROCESSING);
                    if (ret != NO_ERROR) {
                        CLOGE("m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                                PIPE_3AA_REPROCESSING);
                        return ret;
                    }
                    break;
                }
            }
        }

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
            || m_parameters->isReprocessing3aaIspOTF() == false) {
            for (int i = 0; i < MAX_NODE; i++) {
                /* If even one buffer slot is valid. call setBufferManagerToPipe() */
                if (ispBufferManager[i] != NULL) {
                    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP_REPROCESSING);
                    if (ret != NO_ERROR) {
                        CLOGE("m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                                PIPE_ISP_REPROCESSING);
                        return ret;
                    }
                    break;
                }
            }
        }


#ifdef USE_ODC_CAPTURE
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
        // nop
#else
        if (m_parameters->getODCCaptureMode()) {
            ExynosCameraBufferManager *tpuBufferManager[MAX_NODE];
            tpuBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_TPU1)] = m_yuvReprocessingBufferMgr;
            tpuBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C)] = m_yuvReprocessingBufferMgr;
            ret = m_reprocessingFrameFactory->setBufferManagerToPipe(tpuBufferManager, PIPE_TPU1);
            if (ret != NO_ERROR) {
                CLOGE("m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                        PIPE_3AA_REPROCESSING);
                return ret;
            }
        }
#endif // CAMERA_HAS_OWN_GDC
#endif

        ret = m_reprocessingFrameFactory->initPipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory->initPipes() failed");
            return ret;
        }

        ret = m_reprocessingFrameFactory->preparePipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory preparePipe fail");
            return ret;
        }

        /* stream on pipes */
        ret = m_reprocessingFrameFactory->startPipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory startPipe fail");
            return ret;
        }

        /* On High-resolution callback scenario, the reprocessing works with specific fps.
           To avoid the thread creation performance issue, threads in reprocessing pipes
           must be run with run&wait mode */
        bool needOneShotMode = (m_parameters->getHighResolutionCallbackMode() == false);
        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER)
            m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_3AA_REPROCESSING, needOneShotMode);

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
            || m_parameters->isReprocessing3aaIspOTF() == false)
            m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_ISP_REPROCESSING, needOneShotMode);
    }

    m_zslPictureEnabled = true;

    /*
     * Make remained frameFactory here.
     * in case of reprocessing capture, make here.
     */

    return NO_ERROR;
}

status_t ExynosCamera::m_stopPictureInternal(void)
{
    CLOGI("");
    int ret = 0;
    int mcscContainedPipeId = -1;

    /* get the pipeId to have mcsc */
    if (m_parameters->isReprocessingIspMcscOtf() == false)
        mcscContainedPipeId = PIPE_MCSC_REPROCESSING;
    else if (m_parameters->isReprocessing3aaIspOTF() == false)
        mcscContainedPipeId = PIPE_ISP_REPROCESSING;
    else
        mcscContainedPipeId = PIPE_3AA_REPROCESSING;

    m_prePictureThread->join();
    m_pictureThread->join();

    m_ThumbnailCallbackThread->join();
    m_postPictureThread->join();

    m_jpegCallbackThread->join();
    m_yuvCallbackThread->join();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveThread[threadNum]->join();
    }

    if (m_zslPictureEnabled == true) {
        ret = m_reprocessingFrameFactory->stopPipes();
        if (ret < 0) {
            CLOGE("m_reprocessingFrameFactory0>stopPipe() fail");
        }
    }

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_highResolutionCallbackThread->stop();
        if (m_highResolutionCallbackQ != NULL)
            m_highResolutionCallbackQ->sendCmd(WAKE_UP);
        m_highResolutionCallbackThread->requestExitAndWait();
    }

    /* Clear frames & buffers which remain in capture processingQ */
    m_clearFrameQ(m_pictureThreadInputQ, mcscContainedPipeId, SRC_BUFFER_DIRECTION);
    m_clearFrameQ(m_postPictureThreadInputQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(m_dstJpegReprocessingQ, PIPE_JPEG, SRC_BUFFER_DIRECTION);

    m_dstIspReprocessingQ->release();
    m_dstGscReprocessingQ->release();

#ifdef UVS
    dstUvsReprocessingQ->release();
#endif

    m_dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();
    m_yuvCallbackQ->release();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum]->release();
    }

    if (m_highResolutionCallbackQ != NULL) {
        if (m_highResolutionCallbackQ->getSizeOfProcessQ() > 0){
            CLOGD("m_highResolutionCallbackQ->getSizeOfProcessQ(%d). release the highResolutionCallbackQ.",
                 m_highResolutionCallbackQ->getSizeOfProcessQ());
            m_highResolutionCallbackQ->release();
        }
    }

#ifdef USE_ODC_CAPTURE
    m_ODCProcessingQ->release();
    m_postODCProcessingQ->release();
#endif

    /*
     * HACK :
     * Just sleep for
     * all picture-related thread(having m_postProcessList) is over.
     *  if not :
     *    m_clearList will delete frames.
     *    and then, the internal mutex of other thead's deleted frame
     *    will sleep forever (pThread's tech report)
     *  to remove this hack :
     *    stopPreview()'s burstPanorama-related sequence.
     *    stop all pipe -> wait all thread. -> clear all frameQ.
     */
    usleep(5000);

    CLOGD("clear postProcess(Picture) Frame list");

    ret = m_clearList(&m_postProcessList, &m_postProcessListLock);
    if (ret < 0) {
        CLOGE("m_clearList fail");
        return ret;
    }

    m_zslPictureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraFrameSP_sptr_t newFrame,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf,
        bool copybuffer)
{
    CLOGV(" converting preview to callback buffer copybuffer(%d)", copybuffer);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();

    ExynosCameraDurationTimer probeTimer;
    int probeTimeMSEC;
    uint32_t fcount = 0;
    camera_frame_metadata_t m_Metadata;

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    ExynosRect srcRect, dstRect;

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("m_getMemoryCb(%d) fail", callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (!copybuffer) {
        ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            CLOGE(" setCallbackBufferInfo fail, ret(%d)", ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        if (m_flagThreadStop == true || m_previewEnabled == false) {
            CLOGE(" preview was stopped!");
            statusRet = INVALID_OPERATION;
            goto done;
        }

        memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));

        ret = m_calcPreviewGSCRect(&srcRect, &dstRect);

        if (useCSC) {
            int pushFrameCnt = 0, doneFrameCnt = 0;
            int retryCnt = 5;
            bool isOldFrame = false;

            ret = newFrame->setSrcRect(pipeId, &srcRect);
            ret = newFrame->setDstRect(pipeId, &dstRect);

            ret = m_setupEntity(pipeId, newFrame, &previewBuf, &callbackBuf);
            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                statusRet = INVALID_OPERATION;
                goto done;
            }
            m_previewFrameFactory->setOutputFrameQToPipe(m_previewCallbackGscFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(newFrame, pipeId);

            pushFrameCnt = newFrame->getFrameCount();

            do {
                isOldFrame = false;
                retryCnt--;

                CLOGV("wait preview callback output");
                ret = m_previewCallbackGscFrameDoneQ->waitAndPopProcessQ(&newFrame);
                if (ret < 0) {
                    CLOGE("wait and pop fail, ret(%d)", ret);
                    if (ret == TIMED_OUT && retryCnt > 0) {
                        continue;
                    } else {
                        /* TODO: doing exception handling */
                        statusRet = INVALID_OPERATION;
                        goto done;
                    }
                }
                if (newFrame == NULL) {
                    CLOGE("newFrame is NULL");
                    statusRet = INVALID_OPERATION;
                    goto done;
                }

                doneFrameCnt = newFrame->getFrameCount();
                if (doneFrameCnt < pushFrameCnt) {
                    isOldFrame = true;
                    CLOGD("Frame Count(%d/%d), retryCnt(%d)",
                         pushFrameCnt, doneFrameCnt, retryCnt);
                }
            } while ((ret == TIMED_OUT || isOldFrame == true) && (retryCnt > 0));
            CLOGV("preview callback done");

#if 0
            int remainedH = m_orgPreviewRect.h - dst_height;

            if (remainedH != 0) {
                char *srcAddr = NULL;
                char *dstAddr = NULL;
                int planeDiver = 1;

                for (int plane = 0; plane < 2; plane++) {
                    planeDiver = (plane + 1) * 2 / 2;

                    srcAddr = previewBuf.virt.extP[plane] + (ALIGN_UP(hwPreviewW, CAMERA_16PX_ALIGN) * dst_crop_height / planeDiver);
                    dstAddr = callbackBuf->virt.extP[plane] + (m_orgPreviewRect.w * dst_crop_height / planeDiver);

                    for (int i = 0; i < remainedH; i++) {
                        memcpy(dstAddr, srcAddr, (m_orgPreviewRect.w / planeDiver));

                        srcAddr += (ALIGN_UP(hwPreviewW, CAMERA_16PX_ALIGN) / planeDiver);
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
                CLOGE("getYuvPlaneCount(%d) fail", hwPreviewFormat);
                statusRet = INVALID_OPERATION;
                goto done;
            }

            /* TODO : have to consider all fmt(planes) and stride */
            for (int plane = 0; plane < planeCount; plane++) {
                srcAddr = previewBuf.addr[plane];
                dstAddr = callbackBuf.addr[plane];
                memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);

                if (m_ionClient >= 0)
                    ion_sync_fd(m_ionClient, callbackBuf.fd[plane]);
            }
        }
    }

    probeTimer.start();
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
        /* !checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE) && */ /* remove it for motion photo */
        m_disablePreviewCB == false) {
        if (m_parameters->getVRMode() == 1) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME|CAMERA_MSG_PREVIEW_METADATA, previewCallbackHeap, 0, &m_Metadata, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        } else if (m_parameters->getMotionPhotoOn()) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, &m_Metadata, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        } else {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }
    }

    probeTimer.stop();
    getStreamFrameCount((struct camera2_stream *)previewBuf.addr[previewBuf.getMetaPlaneIndex()], &fcount);
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        CLOGV("(%d) duration time(%5d msec)", fcount, (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        CLOGD("(%d) duration time(%5d msec)", fcount, (int)probeTimer.durationMsecs());
    else
        CLOGV("(%d) duration time(%5d msec)", fcount, (int)probeTimer.durationMsecs());

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        __unused int32_t pipeId,
        __unused ExynosCameraFrameSP_sptr_t newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    CLOGV(" converting callback to preview buffer");

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    camera_memory_t *previewCallbackHeap = NULL;
    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("m_getMemoryCb(%d) fail", callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
    if (ret < 0) {
        CLOGE(" setCallbackBufferInfo fail, ret(%d)", ret);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE(" preview was stopped!");
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (useCSC) {
#if 0
        if (m_exynosPreviewCSC) {
            csc_set_src_format(m_exynosPreviewCSC,
                    ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_16PX_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_16PX_ALIGN),
                    0, 0, ALIGN_DOWN(m_orgPreviewRect.w, CAMERA_16PX_ALIGN), ALIGN_DOWN(m_orgPreviewRect.h, CAMERA_16PX_ALIGN),
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
                CLOGE("csc_convert() from callback to lcd fail");
        } else {
            CLOGE("m_exynosPreviewCSC == NULL");
        }
#else
        CLOGW(" doCallbackToPreview use CSC is not yet possible");
#endif
    } else { /* neon memcpy */
        char *srcAddr = NULL;
        char *dstAddr = NULL;
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        if (planeCount <= 0) {
            CLOGE("getYuvPlaneCount(%d) fail", hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = callbackBuf.addr[plane];
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, callbackBuf.size[plane]);

            if (m_ionClient >= 0)
                ion_sync_fd(m_ionClient, previewBuf.fd[plane]);
        }
    }

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    return statusRet;
}

bool ExynosCamera::m_reprocessingPrePictureInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    bool loop = false;
    int retry = 0;
    int retryIsp = 0;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSelector::result_t result;
    ExynosCameraFrameEntity *entity = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    uint32_t bayerFrameCount = 0;
    struct camera2_node_output output_crop_info;

    ExynosCameraBufferManager *bufferMgr = NULL;

    int bayerPipeId = 0;
    int prePictureDonePipeId = 0;
    enum pipeline pipe;
    uint32_t reprocessingFrameCount = 0;
    bool isOldFrame = false;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
#endif

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    if (updateDmShot == NULL) {
        CLOGE("alloc updateDmShot failed");
        goto CLEAN_FRAME;
    }

    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;
#ifdef SUPPORT_DEPTH_MAP
    depthMapBuffer.index = -2;
#endif


    /*
     * in case of pureBayer and 3aa_isp OTF, buffer will go isp directly
     */
    if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
        && m_parameters->isReprocessing3aaIspOTF() == true)
            prePictureDonePipeId = PIPE_3AA_REPROCESSING;
    else
        prePictureDonePipeId = PIPE_ISP_REPROCESSING;

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        if (m_highResolutionCallbackRunning == true) {
            /* will be removed */
            while (m_skipReprocessing == true) {
                usleep(WAITING_TIME);
                if (m_skipReprocessing == false) {
                    CLOGD("stop skip frame for high resolution preview callback");
                    break;
                }
            }
        } else if (m_highResolutionCallbackRunning == false) {
            CLOGW(" m_reprocessingThreadfunc stop for high resolution preview callback");
            loop = false;
            goto CLEAN_FRAME;
        }
    }

    if (m_isZSLCaptureOn
    ) {
        CLOGD("fast shutter callback!!");
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
    }

    /* Check available buffer */
    ret = m_getBufferManager(prePictureDonePipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("Failed to getBufferManager. pipeId %d direction %d ret %d",
                prePictureDonePipeId, DST_BUFFER_DIRECTION, ret);
        goto CLEAN;
    }

    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(prePictureDonePipeId, bufferMgr);
        if (ret < 0) {
            CLOGE("Failed to wait available buffer. pipeId %d ret %d",
                    prePictureDonePipeId, ret);
            goto CLEAN;
        }
    }

    if (m_parameters->isUseHWFC() == true && m_parameters->getHWFCEnable() == true) {
        ret = m_checkBufferAvailable(PIPE_HWFC_JPEG_DST_REPROCESSING, m_jpegBufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("Failed to wait available JPEG buffer. ret %d",
                     ret);
            goto CLEAN;
        }

        ret = m_checkBufferAvailable(PIPE_HWFC_THUMB_SRC_REPROCESSING, m_thumbnailBufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("Failed to wait available THUMB buffer. ret %d",
                     ret);
            goto CLEAN;
        }
    }

    /* Get Bayer buffer for reprocessing */
    result = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer, bayerFrame, updateDmShot
#ifdef SUPPORT_DEPTH_MAP
                            , &depthMapBuffer
#endif
                            );

#if 0
    {
        char filePath[70];
        int width, height = 0;

        m_parameters->getHwSensorSize(&width, &height);

        CLOGE("getHwSensorSize(%d x %d)", width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/bayer_%d.raw", bayerBuffer.index);

        dumpToFile((char *)filePath, bayerBuffer.addr[0], bayerBuffer.size[0]);
    }
#endif

    switch (result) {
    case ExynosCameraFrameSelector::RESULT_NO_FRAME:
        CLOGE(" getBayerBuffer fail");
        goto CLEAN_FRAME;
        break;
    case ExynosCameraFrameSelector::RESULT_SKIP_FRAME:
        CLOGW("skip selectedFrame");
        break;
    case ExynosCameraFrameSelector::RESULT_HAS_FRAME:
        if (bayerBuffer.index == -2) {
            CLOGE(" getBayerBuffer fail. buffer index is -2");
            goto CLEAN_FRAME;
        }
        break;
    default:
        CLOG_ASSERT("invalid select result!! result:%d", result);
        break;
    }

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);

        CLOGD("ODC capture: HWFC enable but, JPEG disable");
    } else {
#endif
    if (m_parameters->getOutPutFormatNV21Enable()) {
       if ((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount() && m_parameters->getSeriesShotMode() > 0)
            || m_parameters->getHighResolutionCallbackMode()
            || m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
            || m_hdrEnabled == true
        ) {
            if (m_parameters->isReprocessingCapture() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                if (m_parameters->isUseHWFC() == true) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                    CLOGD("disable hwFC request");
                }
            }
        }
    } else {
        if ((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount()
             && m_parameters->getSeriesShotMode() > 0)
            || (m_parameters->getSeriesShotCount() == 0)) {
            if (m_parameters->isReprocessingCapture() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
                if (m_parameters->isUseHWFC() == true) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                    CLOGD("enable hwFC request");
                }

                if (m_parameters->getIsThumbnailCallbackOn()
                   ) {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
                } else {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
                }
            }
        } else {
            if (m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount()) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
                if (m_parameters->isUseHWFC()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                    CLOGD("enable hwFC request(default)");
                }
            }
        }
#ifdef USE_ODC_CAPTURE
    }
#endif
    }

    CLOGD("bayerBuffer index %d", bayerBuffer.index);

    if (!m_isZSLCaptureOn
    ) {
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();

#ifdef BURST_CAPTURE
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST)
            m_burstShutterLocation = BURST_SHUTTER_PREPICTURE_DONE;
#endif
    }

    m_isZSLCaptureOn = false;

    /* Generate reprocessing Frame */
    ret = m_generateFrameReprocessing(newFrame, bayerFrame);
    if (ret < 0) {
        CLOGE("generateFrameReprocessing fail, ret(%d)", ret);
        goto CLEAN_FRAME;
    }

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()
        && reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
        int sensorMaxW, sensorMaxH;
        int sensorMarginW, sensorMarginH;
        bool bRet;
        char filePath[70];
        time_t rawtime;
        struct tm* timeinfo;

        memset(filePath, 0, sizeof(filePath));
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(filePath, sizeof(filePath), "/data/media/0/Raw%d_%d_%02d%02d%02d_%02d%02d%02d.raw",
            m_cameraId, bayerFrame->getFrameCount(), timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

        CLOGE(" Sensor (%d x %d)", sensorMaxW, sensorMaxH);

        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */

#ifdef SUPPORT_DEPTH_MAP
    if (depthMapBuffer.index != -2 && m_parameters->getDepthCallbackOnCapture()) {
        ret = newFrame->setDstBuffer(PIPE_VC1, depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get DepthMap buffer");
        }
        newFrame->setRequest(PIPE_VC1, true);

        CLOGD("[Depth] reprocessing frame buffer setting getRequest(PIPE_VC1)(%d)",
                 newFrame->getRequest(PIPE_VC1));
    }
#endif

    /*
     * If it is changed dzoom level in per-frame,
     * it might be occure difference crop region between selected bayer and commend.
     * When dirty bayer is used for reprocessing,
     * the bayer in frame selector is applied n level dzoom but take_picture commend is applied n+1 level dzoom.
     * So, we should re-set crop region value from bayer.
     */
    if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER
       ) {
        /* TODO: HACK: Will be removed, this is driver's job */
        ret = m_convertingStreamToShotExt(&bayerBuffer, &output_crop_info);
        if (ret < 0) {
            CLOGE(" shot_stream to shot_ext converting fail, ret(%d)", ret);
            goto CLEAN_FRAME;
        }

        /* All perframe's information should be updated based on dirty bayer's meta info. */
        size_control_info_t sizeControlInfo;
        camera2_node_group node_group_info;

        /* get a dirty bayer's meta info */
        newFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);

        /* update all size based on dirty bayer's meta info */
        sizeControlInfo.bdsSize.x = output_crop_info.cropRegion[0];
        sizeControlInfo.bdsSize.y = output_crop_info.cropRegion[1];
        sizeControlInfo.bdsSize.w = output_crop_info.cropRegion[2];
        sizeControlInfo.bdsSize.h = output_crop_info.cropRegion[3];

        m_parameters->getPreviewBayerCropSize(&sizeControlInfo.bnsSize, &sizeControlInfo.bayerCropSize);
        m_parameters->getPictureYuvCropSize(&sizeControlInfo.yuvCropSize);
        m_parameters->getPreviewSize(&sizeControlInfo.previewSize.w,
                                    &sizeControlInfo.previewSize.h);
        m_parameters->getPictureSize(&sizeControlInfo.pictureSize.w,
                                    &sizeControlInfo.pictureSize.h);
        m_parameters->getThumbnailSize(&sizeControlInfo.thumbnailSize.w,
                                    &sizeControlInfo.thumbnailSize.h);

        updateNodeGroupInfo(PIPE_ISP_REPROCESSING, m_parameters,
                            sizeControlInfo, &node_group_info);

        newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    ret = newFrame->storeDynamicMeta(updateDmShot);
    if (ret < 0) {
        CLOGE(" storeDynamicMeta fail ret(%d)", ret);
        goto CLEAN_FRAME;
    }

    ret = newFrame->storeUserDynamicMeta(updateDmShot);
    if (ret < 0) {
        CLOGE(" storeUserDynamicMeta fail ret(%d)", ret);
        goto CLEAN_FRAME;
    }

    newFrame->getMetaData(updateDmShot);
    m_parameters->duplicateCtrlMetadata((void *)updateDmShot);

    CLOGD("meta_shot_ext->shot.dm.request.frameCount : %d",
             getMetaDmRequestFrameCount(updateDmShot));



    /* SCC */
    if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
        pipe = PIPE_SCC_REPROCESSING;

        m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

        ret = m_checkBufferAvailable(pipe, bufferMgr);
        if (ret < 0) {
            CLOGE(" Waiting buffer timeout, pipeId(%d), ret(%d)",
                     pipe, ret);
            goto CLEAN_FRAME;
        }

        ret = m_setupEntity(pipe, newFrame);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                     pipe, ret);
            goto CLEAN_FRAME;
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_pictureThreadInputQ, pipe);
        }

        /* push frame to SCC pipe */
        m_reprocessingFrameFactory->pushFrameToPipe(newFrame, pipe);
    } else {
        if (m_parameters->isReprocessing3aaIspOTF() == false) {
            pipe = PIPE_ISP_REPROCESSING;

            m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

            ret = m_checkBufferAvailable(pipe, bufferMgr);
            if (ret < 0) {
                CLOGE(" Waiting buffer timeout, pipeId(%d), ret(%d)",
                            pipe, ret);
                goto CLEAN_FRAME;
            }

        } else {
            pipe = PIPE_3AA_REPROCESSING;
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(NULL, pipe);
        } else {
            {
                m_reprocessingFrameFactory->setFrameDoneQToPipe(m_pictureThreadInputQ, pipe);
            }
        }
    }

    /* Add frame to post processing list */
    newFrame->frameLock();
    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
        CLOGV("does not use postPicList in highResolutionCB (%d)",
            newFrame->getFrameCount());
    } else {
        {
            CLOGD(" push frame(%d) to postPictureList",
                newFrame->getFrameCount());

            ret = m_insertFrameToList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to insert frame into m_postProcessList",
                        newFrame->getFrameCount());
                goto CLEAN_FRAME;
            }
        }
    }

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("startInitialThreads fail");
        goto CLEAN;
    }

    if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
        retry = 0;
        do {
            if( m_reprocessingFrameFactory->checkPipeThreadRunning(pipe) == false ) {
                ret = m_reprocessingFrameFactory->startInitialThreads();
                if (ret < 0) {
                    CLOGE("startInitialThreads fail");
                }
            }

            ret = newFrame->ensureDstBufferState(pipe, ENTITY_BUFFER_STATE_PROCESSING);
            if (ret < 0) {
                CLOGE(" ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail(retry), ret(%d)", ret);
                usleep(1000);
                retry++;
            }
        } while (ret < 0 && retry < 100);
        if (ret < 0) {
            CLOGE(" ensure buffer state(ENTITY_BUFFER_STATE_PROCESSING) fail, ret(%d)", ret);
            goto CLEAN;
        }
    }

    /* Get bayerPipeId at first entity */
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    CLOGV(" bayer Pipe ID(%d)", bayerPipeId);

    ret = m_setupEntity(bayerPipeId, newFrame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("setupEntity fail, bayerPipeId(%d), ret(%d)",
                 bayerPipeId, ret);
        goto CLEAN;
    }

    m_reprocessingFrameFactory->setOutputFrameQToPipe(m_dstIspReprocessingQ, prePictureDonePipeId);

    /* push the newFrameReprocessing to pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(newFrame, bayerPipeId);

    /* We need to start bayer pipe thread */
    m_reprocessingFrameFactory->startThread(bayerPipeId);

    /* wait ISP done */
    reprocessingFrameCount = newFrame->getFrameCount();
    do {
        CLOGI("wait ISP output");
        newFrame = NULL;
        isOldFrame = false;
        ret = m_dstIspReprocessingQ->waitAndPopProcessQ(&newFrame);
        retryIsp++;

        if (ret == TIMED_OUT && retryIsp < 200) {
            CLOGW("ISP wait and pop return, ret(%d)",
                     ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            goto CLEAN;
        }

        CLOGI("ISP output done");

        ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]:Failed to getSrcBuffer. pipeId %d",
                    newFrame->getFrameCount(), bayerPipeId);
            continue;
        }

        shot_ext = (camera2_shot_ext *)bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()];

        if (newFrame->getFrameCount() < reprocessingFrameCount) {
            isOldFrame = true;
            CLOGD("[F%d(%d)]Reprocessing done delayed! waitingFrameCount %d",
                    newFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount,
                    reprocessingFrameCount);
        }

        if (!(reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
              && m_parameters->isReprocessing3aaIspOTF() == true
              && m_parameters->isUseHWFC() == true
              && m_parameters->getHWFCEnable() == true)) {
            ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
            if (ret < 0) {
                CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                         bayerPipeId, ret);

                if (updateDmShot != NULL) {
                    delete updateDmShot;
                    updateDmShot = NULL;
                }

                return ret;
            }

            newFrame->setMetaDataEnable(true);
        }

        {
            /* put bayer buffer */
            ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
            if (ret < 0) {
                CLOGE(" 3AA src putBuffer fail, index(%d), ret(%d)",
                         bayerBuffer.index, ret);
                goto CLEAN;
            }
        }

        /* put isp buffer */
        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
            && m_parameters->isReprocessing3aaIspOTF() == false) {
            if (bayerPipeId == PIPE_3AA &&
                m_parameters->isFlite3aaOtf() == true) {
                ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
            } else
            {
                ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            }
            if (ret < 0) {
                CLOGE(" getBufferManager fail, ret(%d)", ret);
                goto CLEAN;
            }
            if (bufferMgr != NULL) {
                ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                             bayerPipeId, ret);
                    goto CLEAN;
                }
                ret = m_putBuffers(m_ispReprocessingBufferMgr, ispReprocessingBuffer.index);
                if (ret < 0) {
                    CLOGE(" ISP src putBuffer fail, index(%d), ret(%d)",
                             bayerBuffer.index, ret);
                    goto CLEAN;
                }
            }
        }

        /* Put YUV buffer */
        if (newFrame->getRequest(PIPE_MCSC3_REPROCESSING) && newFrame->getRequest(PIPE_MCSC4_REPROCESSING)) {
            ExynosCameraBuffer yuvBuffer;
            {
                /* Handle Main Buffer */
                ret = newFrame->getDstBuffer(bayerPipeId, &yuvBuffer,
                                         m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING));
                if (ret < NO_ERROR) {
                    CLOGE("Failed to getDstBuffer for Main. pipeId %d ret %d",
                            bayerPipeId, ret);
                    goto CLEAN;
                }

                ret = m_reprocessingYuvCallbackFunc(yuvBuffer);
                if (ret < NO_ERROR) {
                    CLOGW("[F%d]Failed to callback reprocessing YUV",
                            newFrame->getFrameCount());
                }

                ret = m_putBuffers(m_yuvReprocessingBufferMgr, yuvBuffer.index);
                if (ret < NO_ERROR) {
                    CLOGE("bufferMgr->putBuffers() fail, pipeId %d ret %d",
                            bayerPipeId, ret);
                    goto CLEAN;
                }
            }

            /* Handle Thumbnail Buffer */
            ret = newFrame->getDstBuffer(bayerPipeId, &yuvBuffer,
                                         m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING));
            if (ret < NO_ERROR) {
                CLOGE("Failed to getDstBuffer. pipeId %d ret %d",
                        bayerPipeId, ret);
                goto CLEAN;
            }

            ret = m_putBuffers(m_thumbnailBufferMgr, yuvBuffer.index);
            if (ret < 0) {
                CLOGE("bufferMgr->putBuffers() fail, pipeId %d ret %d",
                        bayerPipeId, ret);
                goto CLEAN;
            }
        }

        m_reprocessingCounter.decCount();

        CLOGI("reprocessing complete, remaining count(%d)",
                m_reprocessingCounter.getCount());

        if (m_hdrEnabled) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

            m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

            if (m_reprocessingCounter.getCount() == 0)
                m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_highResolutionCallbackQ->pushProcessQ(&newFrame);
        }

        if (newFrame != NULL) {
            CLOGD(" Reprocessing frame delete(%d)",
                     newFrame->getFrameCount());
            newFrame = NULL;
        }
    } while ((ret == TIMED_OUT || isOldFrame == true) && retryIsp < 200 && m_flagThreadStop != true);

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        loop = true;
    }

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
        CLOGD(" Reprocessing frame delete(%d)", newFrame->getFrameCount());

        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE(" getBufferManager fail, ret(%d)", ret);

        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0)
                CLOGE("getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                         bayerPipeId, ret);

            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

#ifdef SUPPORT_DEPTH_MAP
        if (depthMapBuffer.index != -2 && m_depthMapBufferMgr != NULL) {
            ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("Failed to put DepthMap buffer to bufferMgr");
            }
        }
#endif

        newFrame = NULL;
    }

CLEAN:
    if (newFrame != NULL) {
        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE("getBufferManager fail, ret(%d)", ret);

        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0)
                CLOGE("getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                         bayerPipeId, ret);

            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

#ifdef SUPPORT_DEPTH_MAP
        if (depthMapBuffer.index != -2 && m_depthMapBufferMgr != NULL) {
            ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
            if (ret != NO_ERROR) {
               CLOGE("Failed to put DepthMap buffer to bufferMgr");
            }
        }
#endif

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            if (bayerPipeId == PIPE_3AA &&
                m_parameters->isFlite3aaOtf() == true) {
                ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
            } else
            {
                ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            }
            if (ret < 0)
                CLOGE("getBufferManager fail, ret(%d)", ret);

            if (bufferMgr != NULL) {
                ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer,
                        m_pictureFrameFactory->getNodeType(PIPE_3AC));
                if (ret < 0)
                    CLOGE("getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                             bayerPipeId, ret);

                if (ispReprocessingBuffer.index >= 0)
                    m_putBuffers(bufferMgr, ispReprocessingBuffer.index);
            }
        }
    }

    /* newFrame is already pushed some pipes, we can not delete newFrame until frame is complete */
    if (newFrame != NULL) {
        newFrame->frameUnlock();

        ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        newFrame->printEntity();
        CLOGD(" Reprocessing frame delete(%d)", newFrame->getFrameCount());
        {
            newFrame = NULL;
        }
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

    if (newFrame != NULL) {
        CLOGD(" Reprocessing frame delete(%d)", newFrame->getFrameCount());
        newFrame = NULL;
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        loop = true;
    }

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI(" reprocessing fail, remaining count(%d)", m_reprocessingCounter.getCount());

    return loop;
}

bool ExynosCamera::m_pictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int retryCountGSC = 4;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraBuffer yuvBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    struct camera2_stream *shot_stream = NULL;
    ExynosRect srcRect, dstRect;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    int hwPictureW = 0, hwPictureH = 0, hwPictureFormat = 0;
    int buffer_idx = m_getShotBufferIdex();
    float zoomRatio = m_parameters->getZoomRatio(0) / 1000;

    yuvBuffer.index = -2;

    int prePictureStartPipeId = 0;
    int prePictureDonePipeId = 0;
    int bufPipeId = 0;
    bool isSrc = false;

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        if (m_parameters->isReprocessing3aaIspOTF() == true)
            prePictureDonePipeId = PIPE_3AA_REPROCESSING;
        else
            prePictureDonePipeId = PIPE_ISP_REPROCESSING;

        bufPipeId = PIPE_ISPC_REPROCESSING;
        isSrc = false;
        break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        prePictureDonePipeId = PIPE_ISP_REPROCESSING;
        bufPipeId = PIPE_ISPC_REPROCESSING;
        isSrc = false;
        break;
    case PROCESSING_MODE_NON_REPROCESSING_YUV:
        if (m_parameters->is3aaIspOtf() == true)
            prePictureDonePipeId = PIPE_3AA;
        else
            prePictureDonePipeId = PIPE_ISP;

        bufPipeId = PIPE_MCSC1;
        break;
    default:
        CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    /* wait YUV */
    CLOGI("wait YUV output");
    int retry = 0;
    do {
        ret = m_pictureThreadInputQ->waitAndPopProcessQ(&newFrame);
        retry++;
    } while (ret == TIMED_OUT && retry < 40 &&
             (m_takePictureCounter.getCount() > 0 || m_parameters->getSeriesShotCount() == 0));

    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d), retry(%d), takePictuerCount(%d), seriesShotCount(%d)",
                 ret, retry, m_takePictureCounter.getCount(), m_parameters->getSeriesShotCount());
        // TODO: doing exception handling
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    if (m_postProcessList.empty()) {
        CLOGE(" postPictureList is empty");
        usleep(5000);
        if(m_postProcessList.empty()) {
            CLOGE("Retry but postPictureList is empty");
            goto CLEAN;
        }
    }

    /* Get bayerPipeId at first entity */
    prePictureStartPipeId = newFrame->getFirstEntity()->getPipeId();

    if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
        && m_parameters->isReprocessing3aaIspOTF() == true
        && m_parameters->isUseHWFC() == true
        && m_parameters->getHWFCEnable() == true) {
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            goto CLEAN;
        }

        ret = newFrame->setEntityState(prePictureStartPipeId, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", prePictureStartPipeId, ret);

            return ret;
        }

        newFrame->setMetaDataEnable(true);
    }

    /*
     * When Non-reprocessing scenario does not setEntityState,
     * because Non-reprocessing scenario share preview and capture frames
     */
    if (m_parameters->isReprocessingCapture() == true) {
        ret = newFrame->setEntityState(prePictureDonePipeId, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", prePictureDonePipeId, ret);
            return ret;
        }
    }

    CLOGI("YUV output done, frame Count(%d)", newFrame->getFrameCount());

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
        // nop
#else
        ret = m_processODC(newFrame);
        if (ret < NO_ERROR) {
           CLOGW("ODC processing fail : Skip ODC processing!!");
        } else {
            if (m_parameters->getOutPutFormatNV21Enable()) {
                ret = m_convertHWPictureFormat(newFrame, V4L2_PIX_FMT_NV21);
                if (ret < NO_ERROR) {
                    CLOGW("Fail to convert YUYV to NV21");
                    return ret;
                }
            } else {
                ret = m_convertHWPictureFormat(newFrame, V4L2_PIX_FMT_YUYV);
                if (ret < NO_ERROR) {
                    CLOGW("Fail to scale up");
                    return ret;
                }
            }
        }
#endif // CAMERA_HAS_OWN_GDC
    }
#endif

    /* Shutter Callback */
    if (m_parameters->isReprocessingCapture() == false) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI("CAMERA_MSG_SHUTTER callback S");
#ifdef BURST_CAPTURE
            m_notifyCb(CAMERA_MSG_SHUTTER, m_parameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
            CLOGI("CAMERA_MSG_SHUTTER callback E");
        }
    }

    if (m_parameters->getIsThumbnailCallbackOn()) {
        if (m_ThumbnailCallbackThread->isRunning()) {
            m_ThumbnailCallbackThread->join();
            CLOGD(" m_ThumbnailCallbackThread join");
        }
    }

    if ((m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21)
         && (m_parameters->getIsThumbnailCallbackOn()
       )) {
        if (m_parameters->isReprocessingCapture() == true) {
            int postPicturePipeId = PIPE_MCSC1_REPROCESSING;
            if (newFrame->getRequest(postPicturePipeId) == true) {
                m_dstPostPictureGscQ->pushProcessQ(&newFrame);

                ret = m_postPictureCallbackThread->run();
                if (ret != NO_ERROR) {
                    CLOGE("couldn't run magicshot thread, ret(%d)", ret);
                    return INVALID_OPERATION;
                }
            }
        } else {
            int pipeId_gsc_magic = PIPE_GSC_VIDEO;
            ExynosCameraBuffer postgscReprocessingBuffer;
            int previewW = 0, previewH = 0, previewFormat = 0;
            ExynosRect srcRect_magic, dstRect_magic;

            postgscReprocessingBuffer.index = -2;
            CLOGD("magic shot front POSTVIEW callback start. Pipe(%d)", pipeId_gsc_magic);

            ret = newFrame->getDstBuffer(prePictureDonePipeId, &yuvBuffer, m_previewFrameFactory->getNodeType(bufPipeId));
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", prePictureDonePipeId, ret);
                goto CLEAN;
            }

            shot_stream = (struct camera2_stream *)(yuvBuffer.addr[buffer_idx]);
            if (shot_stream != NULL) {
                CLOGD("(%d %d %d %d)",
                        shot_stream->fcount,
                        shot_stream->rcount,
                        shot_stream->findex,
                        shot_stream->fvalid);
                CLOGD("(%d %d %d %d)(%d %d %d %d)",
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
            }

            int retry1 = 0;
            do {
                ret = TIMED_OUT;
                retry1++;
                if (m_postPictureBufferMgr->getNumOfAvailableBuffer() > 0) {
                    ret = m_setupEntity(pipeId_gsc_magic, newFrame, &yuvBuffer, NULL);
                } else {
                    /* wait available YUV buffer */
                    usleep(WAITING_TIME);
                }

                if (retry1 % 10 == 0)
                    CLOGW("retry setupEntity for GSC");
            } while(ret != NO_ERROR && retry1 < (TOTAL_WAITING_TIME/WAITING_TIME));

            if (ret != NO_ERROR) {
                CLOGE("setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                        pipeId_gsc_magic, retry1, ret);
                goto CLEAN;
            }

            m_parameters->getPreviewSize(&previewW, &previewH);
            m_parameters->getPictureSize(&pictureW, &pictureH);
            pictureFormat = m_parameters->getHwPictureFormat();
            previewFormat = m_parameters->getPreviewFormat();

            CLOGD("size preview(%d, %d,format:%d)picture(%d, %d,format:%d)",
                    previewW, previewH, previewFormat, pictureW, pictureH, pictureFormat);

            srcRect_magic.x = shot_stream->input_crop_region[0];
            srcRect_magic.y = shot_stream->input_crop_region[1];
            srcRect_magic.w = shot_stream->input_crop_region[2];
            srcRect_magic.h = shot_stream->input_crop_region[3];
            srcRect_magic.fullW = shot_stream->input_crop_region[2];
            srcRect_magic.fullH = shot_stream->input_crop_region[3];
            srcRect_magic.colorFormat = pictureFormat;

            dstRect_magic.x = 0;
            dstRect_magic.y = 0;
            dstRect_magic.w = previewW;
            dstRect_magic.h = previewH;
            dstRect_magic.fullW = previewW;
            dstRect_magic.fullH = previewH;
            dstRect_magic.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

            ret = getCropRectAlign(srcRect_magic.w, srcRect_magic.h,
                                   previewW, previewH,
                                   &srcRect_magic.x, &srcRect_magic.y,
                                   &srcRect_magic.w, &srcRect_magic.h,
                                   2, 2, 0, zoomRatio);

            ret = newFrame->setSrcRect(pipeId_gsc_magic, &srcRect_magic);
            ret = newFrame->setDstRect(pipeId_gsc_magic, &dstRect_magic);

            CLOGD("size (%d, %d) (%dx%d) full(%dx%d)",
                    srcRect_magic.x, srcRect_magic.y,
                    srcRect_magic.w, srcRect_magic.h,
                    srcRect_magic.fullW, srcRect_magic.fullH);
            CLOGD("size (%d, %d) (%dx%d) full(%dx%d)",
                    dstRect_magic.x, dstRect_magic.y,
                    dstRect_magic.w, dstRect_magic.h,
                    dstRect_magic.fullW, dstRect_magic.fullH);

            /* push frame to GSC pipe */
            m_pictureFrameFactory->setOutputFrameQToPipe(m_dstPostPictureGscQ, pipeId_gsc_magic);
            m_pictureFrameFactory->pushFrameToPipe(newFrame, pipeId_gsc_magic);

            ret = m_postPictureCallbackThread->run();
            if (ret < 0) {
                CLOGE("couldn't run magicshot thread, ret(%d)", ret);
                return INVALID_OPERATION;
            }
        }
    }

    if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
        int pipeId_gsc = (m_parameters->isReprocessingCapture() == true) ? PIPE_GSC_REPROCESSING : PIPE_GSC_PICTURE;

        /* set GSC buffer */
        if (m_parameters->isReprocessingCapture() == true)
            ret = newFrame->getDstBuffer(prePictureDonePipeId, &yuvBuffer, m_reprocessingFrameFactory->getNodeType(bufPipeId));
        else
            ret = newFrame->getDstBuffer(prePictureDonePipeId, &yuvBuffer, m_previewFrameFactory->getNodeType(bufPipeId));

        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", prePictureDonePipeId, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(yuvBuffer.addr[yuvBuffer.getMetaPlaneIndex()]);
        if (shot_stream != NULL) {
            CLOGD("(%d %d %d %d)",
                shot_stream->fcount,
                shot_stream->rcount,
                shot_stream->findex,
                shot_stream->fvalid);
            CLOGD("(%d %d %d %d)(%d %d %d %d)",
                shot_stream->input_crop_region[0],
                shot_stream->input_crop_region[1],
                shot_stream->input_crop_region[2],
                shot_stream->input_crop_region[3],
                shot_stream->output_crop_region[0],
                shot_stream->output_crop_region[1],
                shot_stream->output_crop_region[2],
                shot_stream->output_crop_region[3]);
        } else {
            CLOGE("shot_stream is NULL");
            goto CLEAN;
        }

        int retry = 0;
        m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
        do {
            ret = -1;
            retry++;
            if (bufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_setupEntity(pipeId_gsc, newFrame, &yuvBuffer, NULL);
            } else {
                /* wait available YUV buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0) {
                CLOGW("retry setupEntity for GSC postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                        m_postPictureThreadInputQ->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD0]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD1]->getSizeOfProcessQ(),
                        m_jpegSaveQ[JPEG_SAVE_THREAD2]->getSizeOfProcessQ());
            }
        } while(ret < 0 && retry < (TOTAL_WAITING_TIME/WAITING_TIME) && m_stopBurstShot == false);

        if (ret < 0) {
            if (retry >= (TOTAL_WAITING_TIME/WAITING_TIME)) {
                CLOGE("setupEntity fail, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                         pipeId_gsc, retry, ret, m_stopBurstShot);
                /* HACK for debugging P150108-08143 */
                bufferMgr->printBufferState();
                android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]): unexpected error, get GSC buffer failed, assert!!!!", __FUNCTION__, __LINE__);
            } else {
                CLOGD("setupEntity stopped, pipeId(%d), retry(%d), ret(%d), m_stopBurstShot(%d)",
                         pipeId_gsc, retry, ret, m_stopBurstShot);
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
        m_parameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_parameters->getHwPictureFormat();
#if 1   /* HACK in case of 3AA-OTF-ISP input_cropRegion always 0, use output crop region, check the driver */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
#endif
        switch (pictureFormat) {
        case V4L2_PIX_FMT_YUYV:
        case V4L2_PIX_FMT_YVYU:
        case V4L2_PIX_FMT_UYVY:
        case V4L2_PIX_FMT_VYUY:
            srcRect.fullW = ALIGN_UP(shot_stream->output_crop_region[2], CAMERA_16PX_ALIGN / 2);
            break;
        default:
            srcRect.fullW = ALIGN_UP(shot_stream->output_crop_region[2], CAMERA_16PX_ALIGN);
            break;
        }
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = pictureFormat;

        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
            || m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS
            || m_parameters->getShotMode() == SHOT_MODE_RICH_TONE
            || m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA
            || m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
          ) {
            pictureFormat = V4L2_PIX_FMT_NV21;
        } else {
#if defined(USE_ODC_CAPTURE)
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
            if (m_parameters->getODCCaptureEnable() == true) {
#if defined(GDC_COLOR_FMT)
                pictureFormat = GDC_COLOR_FMT;
#else
                pictureFormat = V4L2_PIX_FMT_NV12M;
#endif /* GDC_COLOR_FMT */
            } else
#endif /* CAMERA_HAS_OWN_GDC */
#endif /* USE_ODC_CAPTURE */
            {
                pictureFormat = JPEG_INPUT_COLOR_FMT;
            }
        }

        {
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = pictureW;
            dstRect.h = pictureH;
            dstRect.fullW = pictureW;
            dstRect.fullH = pictureH;
            dstRect.colorFormat = pictureFormat;
        }
        ret = getCropRectAlign(srcRect.w,  srcRect.h,
                               pictureW,   pictureH,
                               &srcRect.x, &srcRect.y,
                               &srcRect.w, &srcRect.h,
                               2, 2, 0, zoomRatio);

        ret = newFrame->setSrcRect(pipeId_gsc, &srcRect);
        ret = newFrame->setDstRect(pipeId_gsc, &dstRect);
#endif

        CLOGD("size (%d, %d, %d, %d %d %d)",
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        CLOGD("size (%d, %d, %d, %d %d %d)",
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(m_dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(newFrame, pipeId_gsc);

        /* wait GSC */
        newFrame = NULL;
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
            } else if (m_stopBurstShot == true) {
                CLOGD("stopburst! ret(%d)", ret);
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

        /*
         * 15/08/21
         * make complete is need..
         * (especially, on Non-reprocessing. it shared the frame between preview and capture)
         * on handlePreviewFrame, no where to set ENTITY_STATE_COMPLETE.
         * so, we need to set ENTITY_STATE_COMPLETE here.
         */
        ret = newFrame->setEntityState(pipeId_gsc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
            goto CLEAN;
        }

#ifdef USE_ODC_CAPTURE
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
        if (m_parameters->getODCCaptureEnable() == true) {
            ret = m_processODC(newFrame);
            if (ret != NO_ERROR) {
                CLOGW("ODC processing fail : Skip ODC processing!!");
                goto CLEAN;
            }
        }
#endif // CAMERA_HAS_OWN_GDC
#endif // USE_ODC_CAPTURE


        if (m_parameters->getIsThumbnailCallbackOn() == true
            || m_parameters->getShotMode() == SHOT_MODE_MAGIC
        ) {
            m_postPictureCallbackThread->join();
        }

        /* put YUV buffer */
        if (m_parameters->isReprocessingCapture() == true)
            ret = newFrame->getDstBuffer(prePictureDonePipeId, &yuvBuffer, m_reprocessingFrameFactory->getNodeType(bufPipeId));
        else
            ret = newFrame->getDstBuffer(prePictureDonePipeId, &yuvBuffer, m_previewFrameFactory->getNodeType(bufPipeId));

        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", prePictureDonePipeId, ret);
            goto CLEAN;
        }

        m_getBufferManager(pipeId_gsc, &bufferMgr, SRC_BUFFER_DIRECTION);
        ret = m_putBuffers(bufferMgr, yuvBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, ret(%d)", ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
    }

    {
        /* push postProcess */
        m_postPictureThreadInputQ->pushProcessQ(&newFrame);
    }

    m_pictureCounter.decCount();

    CLOGI("picture thread complete, remaining count(%d)", m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0) {
        loop = true;
    } else {
        m_pictureThreadInputQ->release();

        /* If dynamic mode enabled, set request to false */
        if (m_parameters->isDynamicCapture() == true) {
            int srcBufPipeId = -1;
            enum NODE_TYPE dstNodeType = INVALID_NODE;

            switch (reprocessingMode) {
                case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
                    srcBufPipeId = PIPE_VC0;
                    break;
                case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
                    srcBufPipeId = PIPE_3AC;
                    break;
                case PROCESSING_MODE_NON_REPROCESSING_YUV:
                    srcBufPipeId = PIPE_MCSC1;
                    break;
                default:
                    CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
                    break;
            }

            m_previewFrameFactory->setRequest(srcBufPipeId, false);
        }

#ifdef DEBUG_RAWDUMP
        if (reprocessingMode != PROCESSING_MODE_REPROCESSING_PURE_BAYER) {
            m_previewFrameFactory->setRequest(PIPE_VC0, false);
        }
#endif
    }

    /*
       Robust code for postPicture thread running.
       When push frame to postPictrueQ, the thread was start.
       But sometimes it was not started(Known issue).
       So, Manually start the thread.
     */
    if (m_postPictureThread->isRunning() == false
        && m_postPictureThreadInputQ->getSizeOfProcessQ() > 0)
        m_postPictureThread->run();

    /* one shot */
    return loop;

CLEAN:
    if (yuvBuffer.index != -2) {
        CLOGD("putBuffer yuvBuffer(index:%d) in error state",
                yuvBuffer.index);
        {
            m_getBufferManager(prePictureDonePipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            m_putBuffers(bufferMgr, yuvBuffer.index);
        }
    }

    if (m_pictureCounter.getCount() > 0)
        loop = true;

    /* one shot */
    return loop;
}

camera_memory_t *ExynosCamera::m_getJpegCallbackHeap(ExynosCameraBuffer jpegBuf, int seriesShotNumber)
{
    CLOGI("");

    camera_memory_t *jpegCallbackHeap = NULL;

#ifdef BURST_CAPTURE
    if (1 < m_parameters->getSeriesShotCount()) {
        int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();

        if (seriesShotNumber < 0 || seriesShotNumber > m_parameters->getSeriesShotCount()) {
             CLOGE(" Invalid shot number (%d)", seriesShotNumber);
             goto done;
        }

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            CLOGD("burst callback : size (%d), count(%d)", jpegBuf.size[0], seriesShotNumber);

            jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("m_getMemoryCb(%d) fail", jpegBuf.size[0]);
                goto done;
            }
            if (jpegBuf.fd[0] < 0)
                memcpy(jpegCallbackHeap->data, jpegBuf.addr[0], jpegBuf.size[0]);
        } else {
            char filePath[CAMERA_FILE_PATH_SIZE];
            int nw, cnt = 0;
            uint32_t written = 0;
            camera_memory_t *tempJpegCallbackHeap = NULL;

            memset(filePath, 0, sizeof(filePath));

            snprintf(filePath, sizeof(filePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
            CLOGD("burst callback : size (%d), filePath(%s)", jpegBuf.size[0], filePath);

            jpegCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
            if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
                CLOGE("m_getMemoryCb(%s) fail", filePath);
                goto done;
            }

            memcpy(jpegCallbackHeap->data, filePath, sizeof(filePath));
        }
    } else
#endif
    {
        CLOGD("general callback : size (%d)", jpegBuf.size[0]);

        jpegCallbackHeap = m_getMemoryCb(jpegBuf.fd[0], jpegBuf.size[0], 1, m_callbackCookie);
        if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%d) fail", jpegBuf.size[0]);
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

    CLOGD("making callback buffer done");

    return jpegCallbackHeap;
}

bool ExynosCamera::m_postPictureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int loop = false;
    int bufIndex = -2;
    int buffer_idx = m_getShotBufferIdex();
    int retryCountJPEG = 4;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSP_sptr_t yuvFrame = NULL;
    ExynosCameraFrameSP_sptr_t jpegFrame = NULL;

    ExynosCameraBuffer yuvBuffer;
    ExynosCameraBuffer jpegBuffer;

    yuvBuffer.index = -2;
    jpegBuffer.index = -2;

    int pictureDonePipeId = 0;
    int pipeId_jpeg = 0;
    int bufPipeId = 0;

    int currentSeriesShotMode = 0;
    bool IsThumbnailCallback = false;

    exif_attribute_t exifInfo;
    ExynosRect pictureRect;
    ExynosRect thumbnailRect;

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    m_parameters->getFixedExifInfo(&exifInfo);
    pictureRect.colorFormat = m_parameters->getHwPictureFormat();
    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);
    m_parameters->getThumbnailSize(&thumbnailRect.w, &thumbnailRect.h);
    memset(m_picture_meta_shot, 0x00, sizeof(struct camera2_shot));

    /* Set internal Pipe ID */
    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        if (m_parameters->isReprocessing3aaIspOTF() == true)
            pictureDonePipeId = PIPE_3AA_REPROCESSING;
        else
            pictureDonePipeId = PIPE_ISP_REPROCESSING;

        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
        break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        pictureDonePipeId = PIPE_ISP_REPROCESSING;
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
        break;
    case PROCESSING_MODE_NON_REPROCESSING_YUV:
        if (m_parameters->is3aaIspOtf() == true)
            pictureDonePipeId = PIPE_3AA;
        else
            pictureDonePipeId = PIPE_ISP;

        pipeId_jpeg = PIPE_JPEG;
        break;
    default:
        CLOGE("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    /* Set special(external) Pipe ID */
    if (m_parameters->isReprocessingCapture() == true) {
        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true)
            pictureDonePipeId = PIPE_GSC_REPROCESSING;
    } else {
        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true)
            pictureDonePipeId = PIPE_GSC_PICTURE;
    }

    ExynosCameraBufferManager *bufferMgr = NULL;

    CLOGI("wait postPictureQ output");
    ret = m_postPictureThreadInputQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD(" Picture canceled");
        goto CLEAN;
    }

    CLOGI("postPictureQ output done");

    newFrame->getUserDynamicMeta(&m_picture_meta_shot->udm);
    newFrame->getDynamicMeta(&m_picture_meta_shot->dm);

    /* Save the iso of capture frame to camera parameters for providing App/framework iso info. */
    {
        if (m_parameters->isReprocessingCapture() == true) {
            if (m_parameters->getOutPutFormatNV21Enable() == true) {
                bufPipeId = PIPE_MCSC1_REPROCESSING;
                ret = m_getBufferManager(bufPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                if (ret != NO_ERROR) {
                    CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                            bufPipeId, ret);
                    goto CLEAN;
                }
            } else if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
                bufPipeId = PIPE_MCSC3_REPROCESSING;
                ret = m_getBufferManager(bufPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                if (ret != NO_ERROR) {
                    CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                            bufPipeId, ret);
                    goto CLEAN;
                }
            }
#ifdef USE_ODC_CAPTURE
            else if (m_parameters->getODCCaptureEnable() == true) {
                bufPipeId = PIPE_MCSC3_REPROCESSING;
                ret = m_getBufferManager(bufPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
                if (ret != NO_ERROR) {
                    CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                            bufPipeId, ret);
                    goto CLEAN;
                }
            }
#endif
        } else {
            if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true)
                bufPipeId = PIPE_GSC_PICTURE;
            else
                bufPipeId = PIPE_MCSC1;

            ret = m_getBufferManager(bufPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                        bufPipeId, ret);
                goto CLEAN;
            }
        }

        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true)
            ret = newFrame->getDstBuffer(pictureDonePipeId, &yuvBuffer);
        else
            ret = newFrame->getDstBuffer(pictureDonePipeId, &yuvBuffer,
                    m_reprocessingFrameFactory->getNodeType(bufPipeId));
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                    pictureDonePipeId, ret);
            goto CLEAN;
        }
    }

    if (IsThumbnailCallback) {
        m_parameters->setIsThumbnailCallbackOn(true);
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&yuvBuffer);
        CLOGD(" m_ThumbnailCallbackThread run");
    }

#if 0
    {
        char filePath[70];
        int width, height = 0;

        newFrame->dumpNodeGroupInfo("");

        m_parameters->getPictureSize(&width, &height);

        CLOGE("getPictureSize(%d x %d)", width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/pic_%d.yuv", yuvBuffer.index);

        dumpToFile((char *)filePath, yuvBuffer.addr[0], width * height);
    }
#endif

    /* callback */
    if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
        ret = m_reprocessingYuvCallbackFunc(yuvBuffer);
        if (ret < NO_ERROR) {
            CLOGW("[F%d]Failed to callback reprocessing YUV",
                    newFrame->getFrameCount());
        }
    }

    currentSeriesShotMode = m_parameters->getSeriesShotMode();

    /* Make compressed image */
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_parameters->getSeriesShotCount() > 0 ||
        m_hdrEnabled == true) {

        /* HDR callback */
        if (m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21 ||
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGD(" HDR callback");

            {
                yuvFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(bufPipeId, newFrame->getFrameCount());
            }
            ret = yuvFrame->setDstBuffer(bufPipeId, yuvBuffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get DepthMap buffer");
            }

            ret = yuvFrame->storeDynamicMeta(&m_picture_meta_shot->dm);
            if (ret < 0) {
                CLOGE(" storeDynamicMeta fail ret(%d)", ret);
            }

            ret = yuvFrame->storeUserDynamicMeta(&m_picture_meta_shot->udm);
            if (ret < 0) {
                CLOGE(" storeUserDynamicMeta fail ret(%d)", ret);
            }

#ifdef SUPPORT_DEPTH_MAP
            if (newFrame->getRequest(PIPE_VC1) == true) {
                ExynosCameraBuffer depthMapBuffer;

                ret = newFrame->getDstBuffer(PIPE_VC1, &depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                    return ret;
                }

                ret = yuvFrame->setSrcBuffer(bufPipeId, depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                yuvFrame->setRequest(PIPE_VC1, true);
            }
#endif
            m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, m_picture_meta_shot);

            m_yuvCallbackQ->pushProcessQ(&yuvFrame);

            if (m_yuvCallbackThread->isRunning() == false && m_yuvCallbackQ->getSizeOfProcessQ() > 0)
                m_yuvCallbackThread->run();

            m_jpegCounter.decCount();
        } else {
            if (m_parameters->isUseHWFC() == true && m_parameters->getHWFCEnable() == true) {
                /* get gsc dst buffers */
                entity_buffer_state_t jpegMainBufferState = ENTITY_BUFFER_STATE_NOREQ;
                ret = newFrame->getDstBufferState(pictureDonePipeId, &jpegMainBufferState,
                                                    m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("getDstBufferState fail, pipeId(%d), nodeType(%d), ret(%d)",
                            pictureDonePipeId,
                            m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING),
                            ret);
                    goto CLEAN;
                }

                if (jpegMainBufferState == ENTITY_BUFFER_STATE_ERROR) {
                    ret = newFrame->getDstBuffer(pictureDonePipeId, &jpegBuffer,
                                                    m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pictureDonePipeId, ret);
                        goto CLEAN;
                    }

                    /* put JPEG callback buffer */
                    if (m_jpegBufferMgr->putBuffer(jpegBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                        CLOGE("putBuffer(%d) fail", jpegBuffer.index);

                    CLOGE("Failed to get the encoded JPEG buffer");
                    goto CLEAN;
                }

                ret = newFrame->getDstBuffer(pictureDonePipeId, &jpegBuffer,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pictureDonePipeId, ret);
                    goto CLEAN;
                }

                /* in case OTF until JPEG, we should be call below function to this position
                                in order to update debugData from frame. */
                m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, m_picture_meta_shot, true);

                /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
                if (jpegBuffer.size[0] != 0) {
                    /* APP1 Marker - EXIF */
                    UpdateExif(jpegBuffer.addr[0],
                               jpegBuffer.size[0],
                               &exifInfo);
                    /* APP4 Marker - DebugInfo */
                    UpdateDebugData(jpegBuffer.addr[0],
                                    jpegBuffer.size[0],
                                    m_parameters->getDebug2Attribute());
                }
            } else {
                /* 1. get wait available JPEG src buffer */
                int retry = 0;
                do {
                    bufIndex = -2;
                    retry++;

                    if (m_pictureEnabled == false) {
                        CLOGI("m_pictureEnable is false");
                        goto CLEAN;
                    }
                    if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0) {
                        ret = m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegBuffer);
                        if (ret != NO_ERROR)
                            CLOGE("getBuffer fail, ret(%d)", ret);
                    }

                    if (bufIndex < 0) {
                        usleep(WAITING_TIME);

                        if (retry % 20 == 0) {
                            CLOGW("retry JPEG getBuffer(%d) postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                                     bufIndex,
                                    m_postPictureThreadInputQ->getSizeOfProcessQ(),
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
                        CLOGE("getBuffer totally fail, retry(%d), m_stopBurstShot(%d)",
                                 retry, m_stopBurstShot);
                        /* HACK for debugging P150108-08143 */
                        if (bufferMgr != NULL) {
                            bufferMgr->printBufferState();
                        }
                        android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]):\
                                            unexpected error, get jpeg buffer failed, assert!!!!", __FUNCTION__, __LINE__);
                    } else {
                        CLOGD("getBuffer stopped, retry(%d), m_stopBurstShot(%d)",
                                 retry, m_stopBurstShot);
                    }

                    {
                        ret = m_putBuffers(bufferMgr, yuvBuffer.index);
                        goto CLEAN;
                    }
                }

                /* 2. setup Frame Entity */
                ret = m_setupEntity(pipeId_jpeg, newFrame, &yuvBuffer, &jpegBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                             pipeId_jpeg, ret);
                    goto CLEAN;
                }

                /* 3. Q Set-up */
                m_pictureFrameFactory->setOutputFrameQToPipe(m_dstJpegReprocessingQ, pipeId_jpeg);

                /* 4. push the newFrame to pipe */
                m_pictureFrameFactory->pushFrameToPipe(newFrame, pipeId_jpeg);

                /* 5. wait outputQ */
                CLOGI("wait Jpeg output");
                newFrame = NULL;
                while (retryCountJPEG > 0) {
                    ret = m_dstJpegReprocessingQ->waitAndPopProcessQ(&newFrame);
                    if (ret == TIMED_OUT) {
                        CLOGW("wait and pop timeout, ret(%d)", ret);
                        m_pictureFrameFactory->startThread(pipeId_jpeg);
                    } else if (ret < 0) {
                        CLOGE("wait and pop fail, ret(%d)", ret);
                        /* TODO: doing exception handling */
                        goto CLEAN;
                    } else {
                        break;
                    }
                    retryCountJPEG--;
                }

                if (newFrame == NULL) {
                    CLOGE("newFrame is NULL");
                    goto CLEAN;
                }
                /*
                 * When Non-reprocessing scenario does not setEntityState,
                 * because Non-reprocessing scenario share preview and capture frames
                 */
                if (m_parameters->isReprocessingCapture() == true) {
                    ret = newFrame->setEntityState(pipeId_jpeg, ENTITY_STATE_COMPLETE);
                    if (ret < 0) {
                        CLOGE("setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)",
                                 pipeId_jpeg, ret);
                        goto CLEAN;
                    }
                }

                if (IsThumbnailCallback) {
                    if (m_ThumbnailCallbackThread->isRunning()) {
                        m_ThumbnailCallbackThread->join();
                        CLOGD(" m_ThumbnailCallbackThread join");
                    }
                }

                /* put GSC buffer */
                {
                    ret = m_putBuffers(bufferMgr, yuvBuffer.index);
                    if (ret < 0) {
                        CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                                pictureDonePipeId, ret);
                        goto CLEAN;
                    }
                }
            }

            int jpegOutputSize = newFrame->getJpegSize();
            if (jpegOutputSize <= 0) {
                jpegOutputSize = jpegBuffer.size[0];
                if (jpegOutputSize <= 0)
                    CLOGW(" jpegOutput size(%d) is invalid", jpegOutputSize);
            }

            CLOGI("Jpeg output done, jpeg size(%d)", jpegOutputSize);

            jpegBuffer.size[0] = jpegOutputSize;

#ifdef BURST_CAPTURE
            m_burstCaptureCallbackCount++;
            CLOGI(" burstCaptureCallbackCount(%d)", m_burstCaptureCallbackCount);
#endif

            /* push postProcess to call CAMERA_MSG_COMPRESSED_IMAGE */
            int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();

            if ((m_parameters->getSeriesShotCount() > 0)
                && (seriesShotSaveLocation != BURST_SAVE_CALLBACK)
            ) {
                jpegFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId_jpeg, m_burstCaptureCallbackCount);
            } else {
                jpegFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId_jpeg, 0);
            }

            if (jpegFrame == NULL) {
                CLOGE("frame is NULL");
                goto CLEAN;
            }

            ret = jpegFrame->setDstBuffer(pipeId_jpeg, jpegBuffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to get DepthMap buffer");
            }

            ret = jpegFrame->storeDynamicMeta(&m_picture_meta_shot->dm);
            if (ret < 0) {
                CLOGE(" storeDynamicMeta fail ret(%d)", ret);
            }

            ret = jpegFrame->storeUserDynamicMeta(&m_picture_meta_shot->udm);
            if (ret < 0) {
                CLOGE(" storeUserDynamicMeta fail ret(%d)", ret);
            }

retry:
            if ((m_parameters->getSeriesShotCount() > 0)
                ) {
                int threadNum = 0;

                if (m_burst[JPEG_SAVE_THREAD0] == false && m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD0;
                } else if (m_burst[JPEG_SAVE_THREAD1] == false && m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD1;
                } else if (m_burst[JPEG_SAVE_THREAD2] == false && m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning() == false) {
                    threadNum = JPEG_SAVE_THREAD2;
                } else {
                    CLOGW(" wait for available save thread, thread running(%d, %d, %d,)",
                            m_jpegSaveThread[JPEG_SAVE_THREAD0]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD1]->isRunning(),
                            m_jpegSaveThread[JPEG_SAVE_THREAD2]->isRunning());
                    usleep(WAITING_TIME * 10);
                    goto retry;
                }

                m_burst[threadNum] = true;
                ret = m_jpegSaveThread[threadNum]->run();
                if (ret < 0) {
                    CLOGE(" m_jpegSaveThread%d run fail, ret(%d)", threadNum, ret);
                    m_burst[threadNum] = false;
                    m_running[threadNum] = false;
                    goto retry;
                }

                m_jpegSaveQ[threadNum]->pushProcessQ(&jpegFrame);
            } else {
                m_jpegCallbackQ->pushProcessQ(&jpegFrame);
            }

            m_jpegCounter.decCount();
        }
    } else {
        CLOGD(" Disabled compressed image");

        if (IsThumbnailCallback) {
            if (m_ThumbnailCallbackThread->isRunning()) {
                m_ThumbnailCallbackThread->join();
                CLOGD(" m_ThumbnailCallbackThread join");
            }
        }

        /* put GSC buffer */
        {
            if (yuvBuffer.index >= 0) {
                ret = m_putBuffers(bufferMgr, yuvBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                            pictureDonePipeId, ret);
                    goto CLEAN;
                }
            }
        }

        m_jpegCounter.decCount();
    }

    if (newFrame != NULL) {
        newFrame->printEntity();
        newFrame->frameUnlock();

        if (newFrame->getSyncType() != SYNC_TYPE_SWITCH) {
            ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret < 0) {
                CLOGE("remove frame from processList fail, ret(%d)", ret);
            }
        }

        {
            CLOGD(" Picture frame delete(%d)", newFrame->getFrameCount());
            newFrame = NULL;
        }
    }

    CLOGI("postPicture thread complete, remaining count(%d)", m_jpegCounter.getCount());

    if (m_jpegCounter.getCount() <= 0) {
        ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
        flashMgr->setCaptureStatus(false);

        if(m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGI(" End of wideselfie capture!");
            m_pictureEnabled = false;
        }

        CLOGV(" free gsc buffers");
        m_gscBufferMgr->resetBuffers();

        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST)
        {
            CLOGD(" clearList postProcessList, series shot mode(%d)", currentSeriesShotMode);
            if (m_clearList(&m_postProcessList, &m_postProcessListLock) < 0) {
                CLOGE("m_clearList fail");
            }
        }
    }

    if (m_parameters->getScalableSensorMode()) {
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_CHANGING);
        ret = m_restartPreviewInternal(false, NULL);
        if (ret < 0)
            CLOGE("  restart preview internal fail");
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_NONE);
    }

CLEAN:
    if (newFrame != NULL) {
        CLOGD(" Picture frame delete(%d)", newFrame->getFrameCount());

        newFrame->frameUnlock();

        ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
        if (ret != NO_ERROR) {
            CLOGE("Failed to removeFrameFromList. framecount %d ret %d",
                    newFrame->getFrameCount(), ret);
        }

        newFrame = NULL;
    }

    /* HACK: Sometimes, m_postPictureThread is finished without waiting the last picture */
    int waitCount = 5;
    while (m_postPictureThreadInputQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_postPictureThreadInputQ->getSizeOfProcessQ() > 0
        || m_jpegCounter.getCount() > 0) {
        CLOGD("postPicture thread will run again. ShotMode(%d), postPictureQ(%d), Remaining(%d)",
            currentSeriesShotMode,
            m_postPictureThreadInputQ->getSizeOfProcessQ(),
            m_jpegCounter.getCount());
        loop = true;
    }

    return loop;
}

bool ExynosCamera::m_jpegSaveThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int loop = false;
    int curThreadNum = -1;
    char burstFilePath[CAMERA_FILE_PATH_SIZE];
#ifdef BURST_CAPTURE
    int fd = -1;
#endif

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    int pipeId_jpeg = 0;
//    camera_memory_t *jpegCallbackHeap = NULL;

    if (m_parameters->isReprocessingCapture() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_burst[threadNum] == true && m_running[threadNum] == false) {
            m_running[threadNum] = true;
            curThreadNum = threadNum;
            if (m_jpegSaveQ[curThreadNum]->waitAndPopProcessQ(&newFrame) < 0) {
                CLOGE("wait and pop fail, ret(%d)", ret);
                goto done;
            }
            break;
        }
    }

    if (curThreadNum < 0 || curThreadNum > JPEG_SAVE_THREAD2) {
        CLOGE(" invalid thrad num (%d)", curThreadNum);
        goto done;
    }

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        goto done;
    }

#ifdef BURST_CAPTURE
    if (m_parameters->getSeriesShotCount() > 0) {

        int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();

        if (seriesShotSaveLocation == BURST_SAVE_CALLBACK) {
            m_jpegCallbackQ->pushProcessQ(&newFrame);
            goto done;
        } else {
            ExynosCameraBuffer jpegSaveBuffer;
            int seriesShotNumber = newFrame->getFrameCount();

            ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegSaveBuffer);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, ret(%d)", ret);
                goto done;
            }

            memset(burstFilePath, 0, sizeof(burstFilePath));

            m_burstCaptureCallbackCountLock.lock();
            snprintf(burstFilePath, sizeof(burstFilePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
            m_burstCaptureCallbackCountLock.unlock();

            CLOGD("%s fd:%d jpegSize : %d",
                    burstFilePath, jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0]);

            m_burstCaptureSaveLock.lock();

            m_burstSaveTimer.start();

            if(m_FileSaveFunc(burstFilePath, &jpegSaveBuffer) == false) {
                m_burstCaptureSaveLock.unlock();
                CLOGE(" FILE save ERROR");
                goto done;
            }

            m_burstCaptureSaveLock.unlock();

            m_burstSaveTimer.stop();
            m_burstSaveTimerTime = m_burstSaveTimer.durationUsecs();
            if (m_burstSaveTimerTime > (m_burstDuration - 33000)) {
                m_burstDuration += (int)((m_burstSaveTimerTime - m_burstDuration + 33000) / 33000) * 33000;
                CLOGD("Increase burst duration = %d", m_burstDuration);
            }

            CLOGD("m_burstSaveTimerTime : %d msec, path(%s)",
                     (int)m_burstSaveTimerTime / 1000, burstFilePath);
        }
        m_jpegCallbackQ->pushProcessQ(&newFrame);
    } else
#endif
    {
        m_jpegCallbackQ->pushProcessQ(&newFrame);
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

    CLOGI("saving jpeg buffer done");

    /* one shot */
    return false;
}

bool ExynosCamera::m_yuvCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int retry = 0, maxRetry = 0;
    ExynosCameraBuffer postviewCallbackBuffer;
    ExynosCameraBuffer gscReprocessingBuffer;
    camera_memory_t *postviewCallbackHeap = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;
    int currentSeriesShotMode = 0;
    int pipeId = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    int loop = false;
    int ret = 0;
    camera_memory_t *jpegCallbackHeap = NULL;

#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    camera_frame_metadata_t m_Metadata;
    camera_memory_t *depthCallbackHeap = NULL;
    camera_memory_t *depthInfoCallbackHeap = NULL;

    depthMapBuffer.index = -2;
    memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));
#endif

    camera_frame_metadata_t m_MetadataExt;
    debug_attribute_t *debug = NULL;

    memset(&m_MetadataExt, 0, sizeof(camera_frame_metadata_t));

    postviewCallbackBuffer.index = -2;
    gscReprocessingBuffer.index = -2;
    currentSeriesShotMode = m_parameters->getSeriesShotMode();

    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

    if (m_parameters->isReprocessingCapture() == true) {
        pipeId = PIPE_MCSC1_REPROCESSING;
    } else {
        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true)
            pipeId = PIPE_GSC_PICTURE;
        else
            pipeId = PIPE_MCSC1;
    }

    do {
        ret = m_yuvCallbackQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            retry++;
            CLOGW("m_yuvCallbackQ pop fail, retry(%d)", retry);
        }
    } while (ret < 0 && retry < maxRetry &&
             m_flagThreadStop != true &&
             m_stopBurstShot == false &&
             !m_cancelPicture);

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    CLOGV("get frame from m_yuvCallbackQ");

    ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                 pipeId, ret);
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId, &gscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    if (m_parameters->getIsThumbnailCallbackOn()
        ) {
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&gscReprocessingBuffer);
        CLOGD(" m_ThumbnailCallbackThread run");
    }

#ifdef SUPPORT_DEPTH_MAP
    if (newFrame->getRequest(PIPE_VC1) == true) {
        depthInfoCallbackHeap = m_getMemoryCb(-1, sizeof(camera_frame_metadata_t), 1, m_callbackCookie);
        if (!depthInfoCallbackHeap || depthInfoCallbackHeap->data == MAP_FAILED) {
            CLOGE("get depthInfoCallbackHeap fail");
            goto CLEAN;
        }

        ret = newFrame->getSrcBuffer(pipeId, &depthMapBuffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }

        if (depthMapBuffer.index != -2) {
            int depthW = 0, depthH = 0;
            m_parameters->getDepthMapSize(&depthW, &depthH);
            depthCallbackHeap = m_getMemoryCb(depthMapBuffer.fd[0], depthMapBuffer.size[0], 1, m_callbackCookie);
            if (!depthCallbackHeap || depthCallbackHeap->data == MAP_FAILED) {
                CLOGE("m_getMemoryCb(%d) fail", depthMapBuffer.size[0]);
                depthCallbackHeap = NULL;
            } else {
                m_Metadata.depth_data.width = depthW;
                m_Metadata.depth_data.height = depthH;
                m_Metadata.depth_data.format = HAL_PIXEL_FORMAT_RAW10;
                m_Metadata.depth_data.depth_data = (void *)depthCallbackHeap;
            }
        }

#if 0
        char filePath[70];
        int bRet = 0;
        int depthMapW = 0, depthMapH= 0;
        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",
            m_parameters->getCameraId(), newFrame->getFrameCount());

        CLOGD("Raw Dump start (%s)", filePath);

        bRet = dumpToFile((char *)filePath,
            depthMapBuffer.addr[0],
            depthMapW * depthMapH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
#endif
    }
#endif

    debug = m_parameters->getDebugAttribute();

    m_captureLock.lock();

#ifdef SUPPORT_DEPTH_MAP
    if (depthInfoCallbackHeap) {
        setBit(&m_callbackState, OUTFOCUS_DEPTH_MAP_INFO, true);
        m_dataCb(OUTFOCUS_DEPTH_MAP_INFO, depthInfoCallbackHeap, 0, &m_Metadata, m_callbackCookie);
        clearBit(&m_callbackState, OUTFOCUS_DEPTH_MAP_INFO, true);
    }
#endif

    CLOGD(" NV21 callback");

    /* send yuv image with jpeg callback */
    jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
    if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
        CLOGE("m_getMemoryCb(%d) fail", gscReprocessingBuffer.size[0]);
        goto CLEAN;
    }

    setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
    m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, &m_MetadataExt, m_callbackCookie);
    clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

    jpegCallbackHeap->release(jpegCallbackHeap);

    CLOGD(" NV21 callback end");

    if (m_ThumbnailCallbackThread->isRunning()
        ) {
        m_ThumbnailCallbackThread->join();
        CLOGD(" m_ThumbnailCallbackThread join");
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    if ((m_parameters->getIsThumbnailCallbackOn()
        )
        && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
        if (m_cancelPicture) {
            goto CLEAN;
        }

        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("postviewCallbackQ pop fail, retry(%d)", retry);
            }
        } while (ret < 0 && retry < maxRetry &&
                 m_flagThreadStop != true &&
                 m_stopBurstShot == false &&
                 !m_cancelPicture);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0],
                                             postviewCallbackBuffer.size[0],
                                             1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("get postviewCallbackHeap fail");
            goto CLEAN;
        }

        if (!m_cancelPicture) {
            CLOGD(" thumbnail POSTVIEW callback start");
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            CLOGD(" thumbnail POSTVIEW callback end");
        }
        postviewCallbackHeap->release(postviewCallbackHeap);
    }

    m_yuvcallbackCounter.decCount();

CLEAN:
    CLOGI("yuv callback thread complete, remaining count(%d)",
         m_yuvcallbackCounter.getCount());

    if (gscReprocessingBuffer.index != -2) {
        /* put GSC buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                     pipeId, ret);
        }

        {
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
            }
        }
    }

    if (postviewCallbackBuffer.index != -2) {
        CLOGD(" put postviewCallbackBuffer(%d)", postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

#ifdef SUPPORT_DEPTH_MAP
    if (depthMapBuffer.index != -2) {
        if (depthCallbackHeap) {
            depthCallbackHeap->release(depthCallbackHeap);
        }

        CLOGD(" put depthMapBuffer(%d)", depthMapBuffer.index);
        m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
    }

    if (depthInfoCallbackHeap) {
        depthInfoCallbackHeap->release(depthInfoCallbackHeap);
    }
#endif

    if (newFrame != NULL) {
        CLOGD(" yuv callback frame delete(%d)",
                newFrame->getFrameCount());
        newFrame = NULL;
    }

    if (m_yuvcallbackCounter.getCount() <= 0) {

        if (m_parameters->getOutPutFormatNV21Enable() == true) {
            m_parameters->setOutPutFormatNV21Enable(false);
        }

        if (m_hdrEnabled == true) {
            CLOGI(" End of HDR capture!");
            m_hdrEnabled = false;
            m_pictureEnabled = false;
        }

        if (currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
            currentSeriesShotMode == SERIES_SHOT_MODE_SIS) {
            CLOGI(" End of LLS/SIS capture!");
            m_pictureEnabled = false;
        }

        loop = false;
        m_terminatePictureThreads(true);
        m_captureLock.unlock();
    } else {
        loop = true;
        m_captureLock.unlock();
    }

    int waitCount = 5;
    while (m_yuvCallbackQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_yuvCallbackQ->getSizeOfProcessQ() > 0
        || currentSeriesShotMode != SERIES_SHOT_MODE_NONE
        || m_yuvcallbackCounter.getCount() > 0) {
        CLOGD("m_yuvCallbackThread thread will run again. ShotMode(%d), postPictureQ(%d), Remaining(%d)",
            currentSeriesShotMode,
            m_yuvCallbackQ->getSizeOfProcessQ(),
            m_yuvcallbackCounter.getCount());
        loop = true;
    }

    return loop;
}

bool ExynosCamera::m_jpegCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("");

    int ret = 0;
    int retry = 0, maxRetry = 0;
    int loop = false;
    int seriesShotNumber = 0;
    int currentSeriesShotMode = 0;
    int pipeId_jpeg = 0;

    ExynosCameraBuffer jpegCallbackBuffer;
    ExynosCameraBuffer postviewCallbackBuffer;
    camera_memory_t *jpegCallbackHeap = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    camera_frame_metadata_t m_Metadata;

    jpegCallbackBuffer.index = -2;

    postviewCallbackBuffer.index = -2;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    currentSeriesShotMode = m_parameters->getSeriesShotMode();

    if (m_parameters->isReprocessingCapture() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

#ifdef BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_BURST) {
        if (m_burstShutterLocation == BURST_SHUTTER_JPEGCB) {
            m_shutterCallbackThread->join();
            CLOGD("Burst Shutter callback start");
            m_shutterCallbackThreadFunc();
            CLOGD("Burst Shutter callback end");
        } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
            m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
        }
    }
#endif

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    {
        if (m_cancelPicture == true) {
            loop = false;
            goto CLEAN;
        }

        do {
            ret = m_jpegCallbackQ->waitAndPopProcessQ(&newFrame);
            if (ret < 0) {
                retry++;
                CLOGW("jpegCallbackQ pop fail, retry(%d)", retry);
            }
        } while (ret < 0
                 && retry < maxRetry
                 && m_jpegCounter.getCount() > 0
                 && m_cancelPicture == false);
    }

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    m_captureLock.lock();

    seriesShotNumber = newFrame->getFrameCount();

    CLOGD("jpeg calllback is start");

    /* Make compressed image */
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_parameters->getSeriesShotCount() > 0) {
            if (jpegCallbackBuffer.index != -2) {
                jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            }

            if (jpegCallbackHeap == NULL) {
                CLOGE("wait and pop fail, ret(%d)", ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

                if (m_cancelPicture == false) {
                    if (m_parameters->getMotionPhotoOn()) {
                        setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                        m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, &m_Metadata, m_callbackCookie);
                        clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    } else {
                        setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                        m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
                        clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    }
                    CLOGD(" CAMERA_MSG_COMPRESSED_IMAGE callback (%d)",
                             m_burstCaptureCallbackCount);
                }

            if (jpegCallbackBuffer.index != -2) {
                /* put JPEG callback buffer */
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                    CLOGE("putBuffer(%d) fail", jpegCallbackBuffer.index);
            }

            jpegCallbackHeap->release(jpegCallbackHeap);
    } else {
        CLOGD(" Disabled compressed image");
    }

CLEAN:
    CLOGI("jpeg callback thread complete, remaining count(%d)", m_takePictureCounter.getCount());

    if (postviewCallbackBuffer.index != -2) {
        CLOGD(" put dst(%d)", postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

    if (newFrame != NULL) {
        CLOGD(" jpeg callback frame delete(%d)",
                newFrame->getFrameCount());
        newFrame = NULL;
    }

    if (m_takePictureCounter.getCount() == 0) {
        m_pictureEnabled = false;
        loop = false;

        m_terminatePictureThreads(true);
        m_captureLock.unlock();
    } else {
        m_captureLock.unlock();
    }

    return loop;
}

bool ExynosCamera::m_ThumbnailCallbackThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    int32_t previewFormat = 0;
    int32_t SrcFormat = 0;
    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    int thumbnailH, thumbnailW;
    int inputSizeH, inputSizeW;
    int waitCount = 0;
    int dstbufferIndex = -1;
    ExynosCameraFrameSP_sptr_t callbackFrame = NULL;
    ExynosCameraBuffer postgscReprocessingSrcBuffer;
    ExynosCameraBuffer postgscReprocessingDstBuffer;
    ExynosCameraBufferManager *srcbufferMgr = NULL;
    ExynosCameraBufferManager *dstbufferMgr = NULL;
    int gscPipe = PIPE_GSC_REPROCESSING3;
    int retry = 0;
    int framecount = 0;
    int loop = false;

#ifdef PERFRAME_CONTROL_FOR_FLIP
    int flipHorizontal = m_parameters->getFlipHorizontal();
    int flipVertical = m_parameters->getFlipVertical();
#endif

    postgscReprocessingSrcBuffer.index = -2;
    postgscReprocessingDstBuffer.index = -2;

    CLOGD("-- IN --");

    /* wait GSC */
    CLOGV("Wait Thumbnail GSC output");
    ret = m_thumbnailCallbackQ->waitAndPopProcessQ(&postgscReprocessingSrcBuffer);
    CLOGD("Thumbnail GSC output done");
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    m_getBufferManager(gscPipe, &srcbufferMgr, SRC_BUFFER_DIRECTION);
    m_getBufferManager(gscPipe, &dstbufferMgr, DST_BUFFER_DIRECTION);

    callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(gscPipe, framecount);

    if (callbackFrame == NULL) {
        CLOGE("frame is NULL");
        goto CLEAN;
    }

#ifdef PERFRAME_CONTROL_FOR_FLIP
    callbackFrame->setFlipHorizontal(gscPipe, flipHorizontal);
    callbackFrame->setFlipVertical(gscPipe, flipVertical);
#endif

    previewFormat = m_parameters->getHwPreviewFormat();
    SrcFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);

    m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);

    if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
    ) {
        m_parameters->getPictureSize(&inputSizeW, &inputSizeH);
    } else {
        m_parameters->getPreviewSize(&inputSizeW, &inputSizeH);
    }

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable() == true) {

        m_parameters->getPictureSize(&inputSizeW, &inputSizeH);

        if (m_parameters->getOutPutFormatNV21Enable() == true) {
            SrcFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
        } else {
            SrcFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCbCr_422_I);
        }
    }
#endif

    do {
        dstbufferIndex = -2;
        retry++;

        if (dstbufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = dstbufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &postgscReprocessingDstBuffer);
            if (ret != NO_ERROR)
                CLOGE("getBuffer fail, ret(%d)", ret);
        }

        if (dstbufferIndex < 0) {
            usleep(WAITING_TIME);
            if (retry % 20 == 0) {
                CLOGW("retry Post View Thumbnail getBuffer)");
                dstbufferMgr->dump();
            }
        }
    } while (dstbufferIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_flagThreadStop != true && m_stopBurstShot == false);

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = inputSizeW;
    srcRect.h = inputSizeH;
    srcRect.fullW = inputSizeW;
    srcRect.fullH = inputSizeH;
    srcRect.colorFormat = SrcFormat;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = thumbnailW;
    dstRect.h = thumbnailH;
    dstRect.fullW = thumbnailW;
    dstRect.fullH = thumbnailH;
    dstRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_RGBA_8888);

    CLOGD("srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)",
         postgscReprocessingSrcBuffer.index, postgscReprocessingDstBuffer.index,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

    ret = callbackFrame->setSrcRect(gscPipe, srcRect);
    ret = callbackFrame->setDstRect(gscPipe, dstRect);

    ret = m_setupEntity(gscPipe, callbackFrame, &postgscReprocessingSrcBuffer, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", gscPipe, ret);
    }

    m_pictureFrameFactory->setOutputFrameQToPipe(m_ThumbnailPostCallbackQ, gscPipe);
    m_pictureFrameFactory->pushFrameToPipe(callbackFrame, gscPipe);

    /* wait GSC done */
    CLOGV("wait GSC output");
    waitCount = 0;
    callbackFrame = NULL;
    do {
        ret = m_ThumbnailPostCallbackQ->waitAndPopProcessQ(&callbackFrame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("Failed to waitAndPopProcessQ. ret %d waitCount %d flagThreadStop %d",
                 ret, waitCount, m_flagThreadStop);
    }
    if (callbackFrame == NULL) {
        CLOGE("frame is NULL");
        goto CLEAN;
    }

    ret = callbackFrame->getDstBuffer(gscPipe, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", gscPipe, ret);
        goto CLEAN;
    }

#if 0 /* dump code */
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.rgb",
            m_parameters->getCameraId(), callbackFrame->getFrameCount());

    ret = dumpToFile((char *)filePath,
        postgscReprocessingDstBuffer.addr[0],
        FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH));
#endif

    m_postviewCallbackQ->pushProcessQ(&postgscReprocessingDstBuffer);

    CLOGD("--OUT--");

CLEAN:

    if (postgscReprocessingDstBuffer.index != -2 && ret < 0)
        m_putBuffers(dstbufferMgr, postgscReprocessingDstBuffer.index);

    if (m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21
    ) {
        if (postgscReprocessingSrcBuffer.index != -2)
            m_putBuffers(srcbufferMgr, postgscReprocessingSrcBuffer.index);
    }

    if (callbackFrame != NULL) {
        callbackFrame->printEntity();
        CLOGD(" Reprocessing frame delete(%d)", callbackFrame->getFrameCount());
        callbackFrame = NULL;
    }

    return loop;
}

void ExynosCamera::m_terminatePictureThreads(bool callFromJpeg)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer postviewCallbackBuffer;
    ExynosCameraBuffer thumbnailCallbackBuffer;
    ExynosCameraBuffer jpegCallbackBuffer;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;
    ExynosCameraBufferManager *thumbnailBufferMgr = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    int pipeId_jpeg = 0;

    CLOGI(" takePicture disabled, takePicture callback done takePictureCounter(%d)",
             m_takePictureCounter.getCount());

    m_pictureEnabled = false;

    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    flashMgr->setCaptureStatus(false);

#ifdef USE_ODC_CAPTURE
   bool bOutputNV21 = m_parameters->getOutPutFormatNV21Enable();
#endif

    if (m_parameters->isReprocessingCapture() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    m_prePictureThread->requestExit();
    m_pictureThread->requestExit();

    m_postPictureCallbackThread->requestExit();

    if (m_ThumbnailCallbackThread != NULL)
        m_ThumbnailCallbackThread->requestExit();

    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();
    m_yuvCallbackThread->requestExit();

    CLOGI(" wait m_prePictureThrad");
    m_prePictureThread->requestExitAndWait();
    CLOGI(" wait m_pictureThrad");
    m_pictureThread->requestExitAndWait();

    CLOGI(" wait m_postPictureCallbackThread");
    m_postPictureCallbackThread->requestExitAndWait();

    if (m_ThumbnailCallbackThread != NULL) {
        CLOGI(" wait m_ThumbnailCallbackThread");
        m_ThumbnailCallbackThread->requestExitAndWait();
    }

    CLOGI(" wait m_postPictureThrad");
    m_postPictureThread->requestExitAndWait();

    m_parameters->setIsThumbnailCallbackOn(false);

    CLOGI(" wait m_jpegCallbackThrad");
    CLOGI(" wait m_yuvCallbackThrad");
    if (!callFromJpeg) {
        m_jpegCallbackThread->requestExitAndWait();
        m_yuvCallbackThread->requestExitAndWait();
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
         CLOGI(" wait m_jpegSaveThrad%d", threadNum);
         m_jpegSaveThread[threadNum]->requestExitAndWait();
    }

    CLOGI(" All picture threads done");

    enum PROCESSING_MODE reprocessingMode = m_parameters->getReprocessingMode();

    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        m_reprocessingFrameFactory->stopThread(PIPE_3AA_REPROCESSING);

        if (m_parameters->isReprocessing3aaIspOTF() == true)
            break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        m_reprocessingFrameFactory->stopThread(PIPE_ISP_REPROCESSING);
        break;
    default:
        CLOGV("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    if(m_parameters->isReprocessingCapture() == true) {
        m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING3);
        CLOGI("rear gsc, pipe stop(%d)", PIPE_GSC_REPROCESSING3);
    } else {
        m_previewFrameFactory->stopThread(PIPE_GSC_PICTURE);
        CLOGI("GSC Picture Pipe stop(%d)", PIPE_GSC_PICTURE);
    }

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
        if (m_parameters->isReprocessingCapture() == true) {
            // TODO, it will be PIPE_GDC_REPROCESSING
        } else {
            m_previewFrameFactory->stopThread(PIPE_GSC_PICTURE);
            CLOGI("rear gsc , pipe stop(%d)", PIPE_GSC_PICTURE);
            m_previewFrameFactory->stopThread(PIPE_GDC_PICTURE);
            CLOGI("rear gdc , pipe stop(%d)", PIPE_GDC_PICTURE);
        }
#else
        m_reprocessingFrameFactory->stopThread(PIPE_TPU1);
        CLOGI("rear tpu1 , pipe stop(%d)", PIPE_TPU1);
        m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING4);
        CLOGI("rear gsc , pipe stop(%d)", PIPE_GSC_REPROCESSING4);
#endif // CAMERA_HAS_OWN_GDC
    }
#endif

    while (m_jpegCallbackQ->getSizeOfProcessQ() > 0) {
        jpegCallbackBuffer.index = -2;
        newFrame = NULL;
        m_jpegCallbackQ->popProcessQ(&newFrame);
        ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
        if (ret < 0) {
            CLOGE("getDstBuffer fail, ret(%d)", ret);
        }

        CLOGD("put remaining jpeg buffer(index: %d)", jpegCallbackBuffer.index);
        if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
            CLOGE("putBuffer(%d) fail", jpegCallbackBuffer.index);
        }

        if (newFrame != NULL) {
            {
                int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();
                char filepath[CAMERA_FILE_PATH_SIZE];
                int ret = -1;

                memset(filepath, 0, sizeof(filepath));

                snprintf(filepath, sizeof(filepath), "%sBurst%02d.jpg", m_burstSavePath, newFrame->getFrameCount());
                CLOGD("DEBUG(%s[%d]):run unlink %s - start", __FUNCTION__, __LINE__, filepath);
                ret = unlink(filepath);
                CLOGD("DEBUG(%s[%d]):run unlink %s - end", __FUNCTION__, __LINE__, filepath);
                if (ret != 0) {
                    CLOGE("ERR(%s):unlink fail. filepath(%s) ret(%d)", __FUNCTION__, filepath, ret);
                }
            }

            CLOGD(" remaining frame delete(%d)",
                     newFrame->getFrameCount());
            newFrame = NULL;
        }
     }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &thumbnailBufferMgr, SRC_BUFFER_DIRECTION);
    while (m_thumbnailCallbackQ->getSizeOfProcessQ() > 0) {
        m_thumbnailCallbackQ->popProcessQ(&thumbnailCallbackBuffer);

        CLOGD("put remaining thumbnailCallbackBuffer buffer(index: %d)",
                 postviewCallbackBuffer.index);
        m_putBuffers(thumbnailBufferMgr, thumbnailCallbackBuffer.index);
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    while (m_postviewCallbackQ->getSizeOfProcessQ() > 0) {
        m_postviewCallbackQ->popProcessQ(&postviewCallbackBuffer);

        CLOGD("put remaining postview buffer(index: %d)",
                     postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

#ifdef USE_ODC_CAPTURE
    ExynosCameraBufferManager * odcBufferMgr = NULL;
    ExynosCameraBuffer odcBuffer;

    m_getBufferManager(PIPE_TPU1, &odcBufferMgr, DST_BUFFER_DIRECTION);
    while (m_ODCProcessingQ->getSizeOfProcessQ() > 0) {
        m_ODCProcessingQ->popProcessQ(&newFrame);

        CLOGD("ODC buffer(index: %d)", odcBuffer.index);

        /* Get TPU1C Buffer */
        ret = newFrame->getDstBuffer(PIPE_TPU1, &odcBuffer,
                                            m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C));
        if (ret < NO_ERROR) {
            CLOGE("Failed to getDstBuffer for MCSC1. pipeId %d ret %d", PIPE_TPU1, ret);
        }

        m_putBuffers(odcBufferMgr, odcBuffer.index);
    }

    odcBufferMgr = NULL;

    if (bOutputNV21)
        odcBufferMgr = m_postPictureBufferMgr;
    else
        odcBufferMgr = m_yuvReprocessingBufferMgr;

    while (m_postODCProcessingQ->getSizeOfProcessQ() > 0) {
        m_postODCProcessingQ->popProcessQ(&newFrame);

        CLOGD("post ODC buffer(index: %d)", odcBuffer.index);

        /* Get GSC4 Buffer */
        ret = newFrame->getDstBuffer(PIPE_GSC_REPROCESSING4, &odcBuffer);
        if (ret < NO_ERROR) {
            CLOGE("Failed to getDstBuffer for MCSC1. pipeId %d ret %d", PIPE_GSC_REPROCESSING4, ret);
        }

        m_putBuffers(odcBufferMgr, odcBuffer.index);
    }
#endif

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        while (m_jpegSaveQ[threadNum]->getSizeOfProcessQ() > 0) {
            jpegCallbackBuffer.index = -2;
            newFrame = NULL;
            m_jpegSaveQ[threadNum]->popProcessQ(&newFrame);
            if (newFrame != NULL) {
                ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, ret(%d)", ret);
                }

                CLOGD("put remaining SaveQ%d jpeg buffer(index: %d)",
                         threadNum, jpegCallbackBuffer.index);
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
                    CLOGE("putBuffer(%d) fail", jpegCallbackBuffer.index);
                }

                CLOGD(" remaining frame delete(%d)",
                         newFrame->getFrameCount());
                newFrame = NULL;
            }
        }
        m_burst[threadNum] = false;
    }

    switch (reprocessingMode) {
    case PROCESSING_MODE_REPROCESSING_PURE_BAYER:
        m_reprocessingFrameFactory->stopThread(PIPE_3AA_REPROCESSING);

        if (m_parameters->isReprocessing3aaIspOTF() == true)
            break;
    case PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER:
        m_reprocessingFrameFactory->stopThread(PIPE_ISP_REPROCESSING);
        break;
    default:
        CLOGV("Unsupported reprocessing mode(%d)", reprocessingMode);
        break;
    }

    if (m_parameters->isReprocessingCapture() == true) {
        enum pipeline pipe = PIPE_3AA_REPROCESSING;

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PURE_BAYER
            && m_parameters->isReprocessing3aaIspOTF() == false) {
            CLOGD("Wait thread exit Pipe(%d)", pipe);
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("reprocessing stopThread fail, pipe(%d) ret(%d)", pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);

            pipe = PIPE_ISP_REPROCESSING;
        }

        if (reprocessingMode == PROCESSING_MODE_REPROCESSING_PROCESSED_BAYER)
            pipe = PIPE_ISP_REPROCESSING;

        CLOGD("Wait thread exit Pipe(%d)", pipe);
        ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
        if (ret != NO_ERROR)
            CLOGE("reprocessing stopThread fail, pipe(%d) ret(%d)", pipe, ret);

        /* wait for reprocessing thread stop */
        ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe), 5, 4000);
        if (ret != NO_ERROR) {
            CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);
            m_reprocessingFrameFactory->dumpFimcIsInfo(INDEX(pipe), false);
        }

        if (m_parameters->needGSCForCapture(m_cameraId) == true) {
            pipe = PIPE_GSC_REPROCESSING;
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("GSC stopThread fail, pipe(%d) ret(%d)", pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe), 5, 4000);
            if (ret != NO_ERROR) {
                CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);
                m_reprocessingFrameFactory->dumpFimcIsInfo(INDEX(pipe), false);
            }
        }
    }

    CLOGD(" clear postProcessList");
    if (m_clearList(&m_postProcessList, &m_postProcessListLock) < 0) {
        CLOGE("m_clearList fail");
    }

#if 1
    CLOGD(" clear m_dstPostPictureGscQ");
    m_dstPostPictureGscQ->release();

    CLOGD(" clear postPictureThreadInputQ");
    m_postPictureThreadInputQ->release();

    CLOGD(" clear m_yuvCallbackQ");
    m_yuvCallbackQ->release();

    CLOGD(" clear m_pictureThreadInputQ");
    m_pictureThreadInputQ->release();

    CLOGD(" clear m_dstJpegReprocessingQ");
    m_dstJpegReprocessingQ->release();
#else
    ExynosCameraFrameSP_sptr_t frame = NULL;

    CLOGD(" clear postPictureThreadInputQ");
    while(m_postPictureThreadInputQ->getSizeOfProcessQ()) {
        m_postPictureThreadInputQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }

    CLOGD("clear m_pictureThreadInputQ");
    while(m_pictureThreadInputQ->getSizeOfProcessQ()) {
        m_pictureThreadInputQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif

    CLOGD(" reset postPictureBuffer");
    m_postPictureBufferMgr->resetBuffers();

    CLOGD(" reset thumbnail gsc buffers");
    m_thumbnailGscBufferMgr->resetBuffers();

    CLOGD(" reset buffer gsc buffers");
    m_gscBufferMgr->resetBuffers();
    CLOGD(" reset buffer jpeg buffers");
    m_jpegBufferMgr->resetBuffers();
    CLOGD(" reset buffer sccReprocessing buffers");
    m_yuvReprocessingBufferMgr->resetBuffers();

    CLOGD(" reset buffer m_postPicture buffers");
    m_postPictureBufferMgr->resetBuffers();
    CLOGD(" reset buffer thumbnail buffers");
    m_thumbnailBufferMgr->resetBuffers();

#ifdef USE_ODC_CAPTURE
    m_ODCProcessingQ->release();
    m_postODCProcessingQ->release();
    if (m_parameters->isReprocessingCapture() == true) {
        m_parameters->setODCCaptureEnable(false);
    } else {
        m_parameters->setODCCaptureEnable(m_parameters->getODCCaptureMode());
    }
#endif

}

void ExynosCamera::m_dumpVendor(void)
{
    return;
}

bool ExynosCamera::m_startCurrentSet(void)
{
    m_currentSetStart = true;

    return true;
}

bool ExynosCamera::m_stopCurrentSet(void)
{
    m_currentSetStart = false;

    return true;
}

bool ExynosCamera::m_postPictureCallbackThreadFunc(void)
{
    CLOGI("");

    int loop = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    int ret = 0;
    int pipeId = PIPE_3AA_REPROCESSING;
    int pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;

    postgscReprocessingBuffer.index = -2;

    if (m_parameters->isReprocessingCapture() == true) {
        pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;

        if (m_parameters->getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER
            && m_parameters->isReprocessing3aaIspOTF() == true) {
            pipeId = PIPE_3AA_REPROCESSING;
        } else {
            pipeId = PIPE_ISP_REPROCESSING;
        }
    } else {
        pipeId_gsc_magic = PIPE_GSC_VIDEO;
    }

    m_getBufferManager(pipeId_gsc_magic, &bufferMgr, DST_BUFFER_DIRECTION);

    /* wait GSC */
    CLOGI("wait output for postPucture pipe(%d)",pipeId_gsc_magic);
    ret = m_dstPostPictureGscQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    CLOGI("output done pipe(%d)",pipeId_gsc_magic);

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    if (m_parameters->isReprocessingCapture() == true) {
        /* put GCC buffer */
        ret = newFrame->getDstBuffer(pipeId, &postgscReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeId_gsc_magic));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_gsc_magic, ret);
            goto CLEAN;
        }
    } else {
        ret = newFrame->setEntityState(pipeId_gsc_magic, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE(" etEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)",
                     pipeId_gsc_magic, ret);
            return ret;
        }

        /* put GCC buffer */
        ret = newFrame->getDstBuffer(pipeId_gsc_magic, &postgscReprocessingBuffer);
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_gsc_magic, ret);
            goto CLEAN;
        }
    }

    if (m_parameters->getIsThumbnailCallbackOn()) {
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&postgscReprocessingBuffer);
        CLOGD(" m_ThumbnailCallbackThread run");
    }
    if (newFrame != NULL) {
       /* newFrame->printEntity(); */
        CLOGD(" m_postPictureCallbackThreadFunc frame delete(%d)",
                 newFrame->getFrameCount());
        newFrame = NULL;
    }

    return loop;

CLEAN:
    if (postgscReprocessingBuffer.index != -2) {
        {
            m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
        }
    }

    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD(" Reprocessing frame delete(%d)",
                 newFrame->getFrameCount());
        newFrame = NULL;
    }

    /* one shot */
    return loop;
}

#ifdef USE_ODC_CAPTURE
status_t ExynosCamera::m_processODC(ExynosCameraFrameSP_sptr_t newFrame)
{
    status_t ret = NO_ERROR;
    int nRetry = 10;
    bool bExitLoop = false;

    const int ERR_NDONE = -1;
    const int ERR_TIMEOUT = -2;
    int errState = 0;

    ExynosCameraFrameFactory  *frameFactory = NULL;

    ExynosCameraBuffer odcReprocessingBuffer;
    ExynosCameraBuffer odcReprocessedBuffer;

    ExynosCameraBufferManager* odcReprocessingBufferMgr = NULL;
    ExynosCameraBufferManager* odcReprocessedBufferMgr  = NULL;

    int pipeId_scc = 0;
    int pipeId_scc_capture = 0;

    int pipeId_odc = 0;
    int pipeId_odc_capture = 0;

    int odcSrcBufferPos = 0;
    int odcDstBufferPos = 0;

    if (m_parameters->isReprocessingCapture() == true) {
        frameFactory = m_reprocessingFrameFactory;
    } else {
        frameFactory = m_previewFrameFactory;
    }

#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
    pipeId_scc         = PIPE_GSC_PICTURE;
    pipeId_scc_capture = PIPE_GSC_PICTURE;

    pipeId_odc         = PIPE_GDC_PICTURE;
    pipeId_odc_capture = PIPE_GDC_PICTURE;

    odcSrcBufferPos = 0;
    odcDstBufferPos = 0;
#else
    if (m_parameters->isReprocessingCapture() == true) {
        if (m_parameters->getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER
            && m_parameters->isReprocessing3aaIspOTF() == true)
            pipeId_scc = PIPE_3AA_REPROCESSING;
        else
            pipeId_scc = PIPE_ISP_REPROCESSING;
    } else {
        if (m_parameters->is3aaIspOtf() == true)
            pipeId_scc = PIPE_3AA;
        else
            pipeId_scc = PIPE_ISP;
    }
    pipeId_scc_capture = PIPE_MCSC3_REPROCESSING;

    pipeId_odc = PIPE_TPU1;
    pipeId_odc_capture = PIPE_TPU1C;

    odcSrcBufferPos = frameFactory->getNodeType(pipeId_scc_capture);
    odcDstBufferPos = frameFactory->getNodeType(pipeId_odc_capture);
#endif

    CLOGD("ODC for capture : start : pipeId_scc(%d), pipeId_scc_capture(%d) -> pipeId_odc(%d), pipeId_odc_capture(%d)",
        pipeId_scc, pipeId_scc_capture, pipeId_odc, pipeId_odc_capture);

    /* Get SRC Buffer */
    ret = newFrame->getDstBuffer(pipeId_scc, &odcReprocessingBuffer, odcSrcBufferPos);
    if (ret < NO_ERROR) {
        CLOGE("Failed to getDstBuffer for MCSC1. pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

#if defined(CAMERA_HAS_OWN_GDC) && (CAMERA_HAS_OWN_GDC == true)
    /* Get DST Buffer */
    ret = m_getBufferManager(pipeId_odc, &odcReprocessedBufferMgr, DST_BUFFER_DIRECTION);
    if (ret < NO_ERROR) {
        CLOGE("Failed to get bufferManager for pipeId_odc(%d), %s", pipeId_odc, "DST_BUFFER_DIRECTION");
        return ret;
    }

    int reqBufIndex = -1;

    odcReprocessedBuffer.index = -2;

    ret = odcReprocessedBufferMgr->getBuffer(&reqBufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &odcReprocessedBuffer);
    if (ret != NO_ERROR) {
        CLOGE("Buffer manager(%s) getBuffer fail, frame(%d), ret(%d)",
            odcReprocessedBufferMgr->getName(), newFrame->getFrameCount(), ret);
        return ret;
    }

    /* Set SRC Rect */
    ExynosRect srcRect;
    newFrame->getDstRect(pipeId_scc_capture, &srcRect, odcSrcBufferPos);

    // Set GDC's srcBcropRect bcrop size in hw sensor size
    ExynosRect srcBcropRect;
    int        srcBcropNodeIndex = 1;

    int sensorW = 0;
    int sensorH = 0;
    camera2_node_group node_group_info_bcrop;

    m_parameters->getMaxSensorSize(&sensorW, &sensorH);

    if (m_parameters->isFlite3aaOtf() == true)
        newFrame->getNodeGroupInfo(&node_group_info_bcrop, PERFRAME_INFO_FLITE);
    else
        newFrame->getNodeGroupInfo(&node_group_info_bcrop, PERFRAME_INFO_3AA);

    srcBcropRect.x     = node_group_info_bcrop.leader.input.cropRegion[0];
    srcBcropRect.y     = node_group_info_bcrop.leader.input.cropRegion[1];
    srcBcropRect.w     = node_group_info_bcrop.leader.input.cropRegion[2];
    srcBcropRect.h     = node_group_info_bcrop.leader.input.cropRegion[3];
    srcBcropRect.fullW = sensorW;
    srcBcropRect.fullH = sensorH;
    srcBcropRect.colorFormat = srcRect.colorFormat;

    /* Set DST Rect */
    ExynosRect dstRect(srcRect);

    CLOGD("ODC for capture : [Bcrop] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        newFrame->getFrameCount(),
        odcReprocessingBuffer.index,
        srcBcropRect.x, srcBcropRect.y, srcBcropRect.w, srcBcropRect.h, srcBcropRect.fullW, srcBcropRect.fullH,
        v4l2Format2Char(srcBcropRect.colorFormat, 0),
        v4l2Format2Char(srcBcropRect.colorFormat, 1),
        v4l2Format2Char(srcBcropRect.colorFormat, 2),
        v4l2Format2Char(srcBcropRect.colorFormat, 3));

    CLOGD("ODC for capture : [SRC] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        newFrame->getFrameCount(),
        odcReprocessingBuffer.index,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH,
        v4l2Format2Char(srcRect.colorFormat, 0),
        v4l2Format2Char(srcRect.colorFormat, 1),
        v4l2Format2Char(srcRect.colorFormat, 2),
        v4l2Format2Char(srcRect.colorFormat, 3));

    CLOGD("ODC for capture : [DST] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d) format(%c%c%c%c)",
        newFrame->getFrameCount(),
        odcReprocessedBuffer.index,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH,
        v4l2Format2Char(dstRect.colorFormat, 0),
        v4l2Format2Char(dstRect.colorFormat, 1),
        v4l2Format2Char(dstRect.colorFormat, 2),
        v4l2Format2Char(dstRect.colorFormat, 3));

    if (srcRect.w != dstRect.w ||
        srcRect.h != dstRect.h) {
        CLOGW("ODC for capture : size is invalid : [SRC] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            newFrame->getFrameCount(),
            odcReprocessingBuffer.index,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);

        CLOGW("ODC for capture : size is invalid : [DST] : frame(%d) index(%d) x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            newFrame->getFrameCount(),
            odcReprocessedBuffer.index,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);
    }

    ret = newFrame->setSrcRect(pipeId_odc, srcRect);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setSrcRect(pipeId_odc : %d) fail. frame(%d)", pipeId_odc, newFrame->getFrameCount());
        return ret;
    }

    ret = newFrame->setSrcRect(pipeId_odc, srcBcropRect, srcBcropNodeIndex);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setSrcRect(pipeId_odc : %d, srcBcropNodeIndex : %d) fail. frame(%d)",
            pipeId_odc, srcBcropNodeIndex, newFrame->getFrameCount());
        return ret;
    }

    ret = newFrame->setDstRect(pipeId_odc, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setDstRect(pipeId_odc : %d) fail. frame(%d)", pipeId_odc, newFrame->getFrameCount());
        return ret;
    }

    ret = m_setupEntity(pipeId_odc, newFrame, &odcReprocessingBuffer, &odcReprocessedBuffer);
    if (ret != NO_ERROR) {
        CLOGE("m_setupEntity(pipeId_odc : %d, frame : %d, odcReprocessingBuffer : %d, &odcReprocessedBuffer : %d) fail",
            pipeId_odc, newFrame->getFrameCount(), odcReprocessingBuffer.index, odcReprocessedBuffer.index);
        return ret;
    }
#else
    /* Set DST buffer to SRC buffer for ODC */
    ret = newFrame->setSrcBuffer(pipeId_odc, odcReprocessingBuffer);
    if (ret < NO_ERROR) {
        CLOGE("Failed to setSrcBuffer for TPU1. pipeId %d ret %d",
                pipeId_odc, ret);
        return ret;
    }

    newFrame->setRequest(pipeId_odc, true);
    newFrame->setRequest(pipeId_odc_capture, true);
#endif // CAMERA_HAS_OWN_GDC

    /* Push frame to TPU1 pipe */
    CLOGD("Push frame to pipeId_odc(%d)", pipeId_odc);
    frameFactory->setOutputFrameQToPipe(m_ODCProcessingQ, pipeId_odc);
    frameFactory->pushFrameToPipe(newFrame, pipeId_odc);

    /* Waiting ODC processing done */
    do {
        if (errState == ERR_NDONE)
            frameFactory->pushFrameToPipe(newFrame, pipeId_odc);

        ret = m_ODCProcessingQ->waitAndPopProcessQ(&newFrame);
        if (ret < NO_ERROR) {
            if (ret == TIMED_OUT) {
                CLOGE("ODC Processing waiting time is expired");
            } else {
                CLOGE("wait and pop fail, ret(%s(%d))", "ERROR", ret);
            }

            CLOGW("ODC processing retry(%d)", nRetry);
            errState = ERR_TIMEOUT;
            nRetry--;
        } else {

            entity_buffer_state_t tpu1BufferState = ENTITY_BUFFER_STATE_NOREQ;
            ret = newFrame->getDstBufferState(pipeId_odc, &tpu1BufferState, odcDstBufferPos);
            if (ret < NO_ERROR) {
                CLOGE("getDstBufferState fail, pipeId(%d), nodeType(%d), ret(%d)",
                            pipeId_odc,
                            odcDstBufferPos,
                            ret);
            }

            if (tpu1BufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                bExitLoop = true;
            } else {
                /* Get ODC processed image buffer from TPU1C */
                ret = newFrame->getDstBuffer(pipeId_odc, &odcReprocessedBuffer, odcDstBufferPos);
                if (ret < NO_ERROR) {
                    CLOGE("Failed to getDstBuffer for TPU1(ODC). buffer state(%d), pipeId(%d), ret(%d)", tpu1BufferState, pipeId_odc, ret);
                } else {
                    /* Release src buffer from TPU1C */
                    odcReprocessingBufferMgr = NULL;
                    ret = m_getBufferManager(pipeId_odc, &odcReprocessingBufferMgr, DST_BUFFER_DIRECTION);
                    if (ret < NO_ERROR) {
                        CLOGE("Failed to get bufferManager for %s, %s", "pipeId_odc", "DST_BUFFER_DIRECTION");
                    } else {
                        ret = m_putBuffers(odcReprocessingBufferMgr, odcReprocessingBuffer.index);
                        if (ret < NO_ERROR) {
                            CLOGE("Failed to putBuffer index(%d)", odcReprocessingBuffer.index);
                        }
                    }
                    CLOGW("Released error buffer, state(%d)", tpu1BufferState);
                }

                CLOGW("ODC processing retry(%d)", nRetry);
                errState = ERR_NDONE;
                nRetry--;
            }
        }
    } while(nRetry > 0 && bExitLoop == false);

    if (nRetry <= 0)
        return ret;

    CLOGD("ODC processing done!!!!!!!");

    /* Get ODC processed image buffer from TPU1C */
    ret = newFrame->getDstBuffer(pipeId_odc, &odcReprocessedBuffer, odcDstBufferPos);
    if (ret < NO_ERROR) {
        CLOGE("Failed to getDstBuffer for TPU1(ODC). pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

    /* replace MCSC1 buffer by ODC processed buffer */
    ret = newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_REQUESTED, odcSrcBufferPos);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getDstBuffer for MCSC3. pipeId %d ret %d", pipeId_scc, ret);
        return ret;
    }

    ret = newFrame->setDstBuffer(pipeId_scc, odcReprocessedBuffer, odcSrcBufferPos);
    if (ret != NO_ERROR) {
        CLOGE("Failed to set processed buffer to MCSC3. pipeId %d ret %d", pipeId_scc, ret);
        return ret;
    }

    ret = newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_COMPLETE, odcSrcBufferPos);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setDstBufferState to pipeId_scc(%d) ret %d", pipeId_scc, ret);
        return ret;
    }

    /* Release src buffer from MCSC3 */
    ret = m_getBufferManager(pipeId_scc_capture, &odcReprocessingBufferMgr, DST_BUFFER_DIRECTION);
    if (ret < NO_ERROR) {
        CLOGE("Failed to get bufferManager for pipeId_scc_capture(%d), %s", pipeId_scc_capture, "DST_BUFFER_DIRECTION");
    }

    ret = m_putBuffers(odcReprocessingBufferMgr, odcReprocessingBuffer.index);
    if (ret < NO_ERROR) {
        CLOGE("Failed to putBuffer index(%d)", odcReprocessingBuffer.index);
    }

    CLOGD("ODC for capture : end");

    return ret;
}

status_t ExynosCamera::m_convertHWPictureFormat(ExynosCameraFrameSP_sptr_t newFrame, int pictureFormat)
{
    status_t ret = NO_ERROR;
    int nRetry = 20;
    bool bExitLoop = false;

    const int ERR_NDONE = -1;
    const int ERR_TIMEOUT = -2;
    int errState = 0;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer gscReprocessedBuffer;
    ExynosCameraBufferManager* gscReprocessingBufferMgr = NULL;

    int gscReprocessingBufferIndex;

    ExynosRect srcBufRect, dstBufRect;
    int pictureW = 0, pictureH = 0;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;

    int processedBufferPos = PIPE_MCSC3_REPROCESSING;

    if (m_parameters->isReprocessingCapture() == true) {
        if (m_parameters->getReprocessingMode() == PROCESSING_MODE_REPROCESSING_PURE_BAYER
            && m_parameters->isReprocessing3aaIspOTF() == true) {
            pipeId_scc = PIPE_3AA_REPROCESSING;
        } else {
            pipeId_scc = PIPE_ISP_REPROCESSING;
        }

        pipeId_gsc = PIPE_GSC_REPROCESSING4;
    } else {
        if (m_parameters->is3aaIspOtf() == true)
            pipeId_scc = PIPE_3AA;
        else
            pipeId_scc = PIPE_ISP;

        pipeId_gsc = PIPE_GSC_PICTURE;
    }

    /* Get MCSC3 Buffer */
    ret = newFrame->getDstBuffer(pipeId_scc, &gscReprocessingBuffer,
                                    m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING));
    if (ret != NO_ERROR) {
        CLOGE("Failed to getDstBuffer for MCSC1. pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

    /* Set buffer pos and Buffer mgr after gsc */
    if (pictureFormat == V4L2_PIX_FMT_NV21) {
        processedBufferPos = PIPE_MCSC1_REPROCESSING;
        gscReprocessingBufferMgr = m_postPictureBufferMgr;
    } else if (pictureFormat == V4L2_PIX_FMT_YUYV) {
        processedBufferPos = PIPE_MCSC3_REPROCESSING;
        gscReprocessingBufferMgr = m_yuvReprocessingBufferMgr;
    } else {
        processedBufferPos = PIPE_MCSC3_REPROCESSING;
        gscReprocessingBufferMgr = m_yuvReprocessingBufferMgr;
    }

    ret = gscReprocessingBufferMgr->getBuffer(&gscReprocessingBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &gscReprocessedBuffer);
    if (ret < 0)
        CLOGE("Failed to get buffer");

    /* set size for gsc */
    m_parameters->getPictureSize(&pictureW, &pictureH);

    srcBufRect.x = 0;
    srcBufRect.y = 0;
    srcBufRect.w = pictureW;
    srcBufRect.h = pictureH;
    srcBufRect.fullW = pictureW;
    srcBufRect.fullH = pictureH;
    srcBufRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCbCr_422_I);

    dstBufRect.x = 0;
    dstBufRect.y = 0;
    dstBufRect.w = pictureW;
    dstBufRect.h = pictureH;
    dstBufRect.fullW = pictureW;
    dstBufRect.fullH = pictureH;

    if (pictureFormat == V4L2_PIX_FMT_NV21)
        dstBufRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
    else if (pictureFormat == V4L2_PIX_FMT_YUYV)
        dstBufRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCbCr_422_I);
    else {
        dstBufRect.colorFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCbCr_422_I);
    }

    CLOGD("srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%c %c %c %c) -> (%d, %d, %d, %d) format(%c %c %c %c)",
        gscReprocessingBuffer.index, gscReprocessedBuffer.index,
        srcBufRect.x, srcBufRect.y, srcBufRect.w, srcBufRect.h,
        (char)((srcBufRect.colorFormat >> 0) & 0xFF),
        (char)((srcBufRect.colorFormat >> 8) & 0xFF),
        (char)((srcBufRect.colorFormat >> 16) & 0xFF),
        (char)((srcBufRect.colorFormat >> 24) & 0xFF),
        dstBufRect.x, dstBufRect.y, dstBufRect.w, dstBufRect.h,
        (char)((dstBufRect.colorFormat >> 0) & 0xFF),
        (char)((dstBufRect.colorFormat >> 8) & 0xFF),
        (char)((dstBufRect.colorFormat >> 16) & 0xFF),
        (char)((dstBufRect.colorFormat >> 24) & 0xFF));

    ret = newFrame->setSrcRect(PIPE_GSC_REPROCESSING4, srcBufRect);
    ret = newFrame->setDstRect(PIPE_GSC_REPROCESSING4, dstBufRect);

    ret = m_setupEntity(PIPE_GSC_REPROCESSING4, newFrame, &gscReprocessingBuffer, &gscReprocessedBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", PIPE_GSC_REPROCESSING4, ret);
    }

    CLOGD("Push frame to PIPE_GSC4");
    m_reprocessingFrameFactory->setOutputFrameQToPipe(m_postODCProcessingQ, PIPE_GSC_REPROCESSING4);
    m_reprocessingFrameFactory->pushFrameToPipe(newFrame, PIPE_GSC_REPROCESSING4);

    do {
        if (errState == ERR_NDONE)
            m_reprocessingFrameFactory->pushFrameToPipe(newFrame, PIPE_GSC_REPROCESSING4);

        ret = m_postODCProcessingQ->waitAndPopProcessQ(&newFrame);
        if (ret < NO_ERROR) {
            if (ret == TIMED_OUT) {
                CLOGE("GSC Processing waiting time is expired");
            } else {
                CLOGE("wait and pop fail, ret(%s(%d))", "ERROR", ret);
            }
            errState = ERR_TIMEOUT;
            nRetry--;
            CLOGW("GSC4 processing retry(%d)", nRetry);
        } else {
            entity_state_t gscEntityState;
            newFrame->getEntityState(PIPE_GSC_REPROCESSING4, &gscEntityState);
            if (gscEntityState != ENTITY_STATE_FRAME_DONE) {
                errState = ERR_NDONE;
                nRetry--;
                CLOGW("GSC4 processing retry(%d)", nRetry);
            } else {
                bExitLoop = true;
            }
        }
    }while( (nRetry > 0) && (bExitLoop == false) );

    if (nRetry <= 0)
        goto CONVERT_FORMAT_DONE;

    CLOGD("GSC4 processing done!!!!!!!");

    /* replace MCSC buffer by GSC processed buffer */
    ret = newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_REQUESTED,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to setDstBufferState to dst_pipeId(%d). pipeId %d ret %d", processedBufferPos, pipeId_scc, ret);
        goto CONVERT_FORMAT_DONE;
    }

    ret = newFrame->setDstBuffer(pipeId_scc, gscReprocessedBuffer,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to set processed buffer to dst_pipeId(%d). pipeId %d ret %d", processedBufferPos, pipeId_scc, ret);
        goto CONVERT_FORMAT_DONE;
    }

    ret = newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_COMPLETE,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to setDstBufferState to dst_pipeId(%d). pipeId %d ret %d", processedBufferPos, pipeId_scc, ret);
    }

    /* Release src buffer from MCSC3 */
    ret = m_getBufferManager(PIPE_MCSC3_REPROCESSING, &gscReprocessingBufferMgr, DST_BUFFER_DIRECTION);
    if (ret < NO_ERROR) {
        CLOGE("Failed to get bufferManager for %s, %s", "PIPE_MCSC3_REPROCESSING", "DST_BUFFER_DIRECTION");
    }

    ret = m_putBuffers(gscReprocessingBufferMgr, gscReprocessingBuffer.index);
    if (ret < NO_ERROR) {
        CLOGE("Failed to putBuffer index(%d)", gscReprocessingBuffer.index);
    }

    return ret;

CONVERT_FORMAT_DONE:

    ret = m_putBuffers(gscReprocessingBufferMgr, gscReprocessedBuffer.index);
    if (ret < NO_ERROR) {
        CLOGE("Failed to putBuffer index(%d)", gscReprocessingBuffer.index);
    }

    return ret;
}
#endif

}; /* namespace android */
