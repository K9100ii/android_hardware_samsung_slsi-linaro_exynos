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
#define LOG_TAG "ExynosCameraGed"
#include <cutils/log.h>

#include "ExynosCamera.h"

namespace android {

void ExynosCamera::initializeVision()
{
    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return;
}

void ExynosCamera::releaseVision()
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

status_t ExynosCamera::setPreviewWindow(preview_stream_ops *w)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;
    int width, height;
    int halPreviewFmt = 0;
    bool flagRestart = false;
    buffer_manager_type bufferType = BUFFER_MANAGER_ION_TYPE;

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            return NO_ERROR;
        }
    } else {
        CLOGW("(%s[%d]):m_parameters is NULL. Skipped", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (previewEnabled() == true) {
        CLOGW("WRN(%s[%d]):Preview is started, we forcely re-start preview", __FUNCTION__, __LINE__);
        flagRestart = true;
        m_disablePreviewCB = true;
        stopPreview();
    }

    if (m_scpBufferMgr != NULL) {
        CLOGD("DEBUG(%s[%d]): scp buffer manager need recreate", __FUNCTION__, __LINE__);
        m_scpBufferMgr->deinit();
        delete m_scpBufferMgr;
        m_scpBufferMgr = NULL;
    }

    m_previewWindow = w;

    if (m_previewWindow == NULL) {
        bufferType = BUFFER_MANAGER_ION_TYPE;
        CLOGW("WARN(%s[%d]):Preview window is NULL, create internal buffer for preview", __FUNCTION__, __LINE__);
    } else {
        halPreviewFmt = m_parameters->getHalPixelFormat();
        bufferType = BUFFER_MANAGER_GRALLOC_TYPE;
        m_parameters->getHwPreviewSize(&width, &height);

        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator();

#ifdef RESERVED_MEMORY_FOR_GRALLOC_ENABLE
        if (m_parameters->getRecordingHint() == false
            && !(m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE && getCameraId() == CAMERA_ID_BACK)) {
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
            CLOGE("ERR(%s[%d]):Gralloc init fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto func_exit;
        }

        ret = m_grAllocator->setBuffersGeometry(width, height, halPreviewFmt);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Gralloc setBufferGeomety fail, size(%dx%d), fmt(%d), ret(%d)",
                __FUNCTION__, __LINE__, width, height, halPreviewFmt, ret);
            goto func_exit;
        }
    }

    m_createBufferManager(&m_scpBufferMgr, "PREVIEW_BUF", bufferType);

    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE)
        m_scpBufferMgr->setAllocator(m_grAllocator);

    if (flagRestart == true)
        startPreview();

func_exit:
    m_disablePreviewCB = false;

    return ret;
}

status_t ExynosCamera::startPreview()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    property_set("persist.sys.camera.preview","2");

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
#ifndef CAMERA_FAST_ENTRANCE_V1
    m_checkFirstFrameLux = false;
#endif
    m_parameters->setIsThumbnailCallbackOn(false);
    m_stopLongExposure = false;

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

#ifdef FORCE_RESET_MULTI_FRAME_FACTORY
    /* HACK
     * stopPreview() close companion
     * so. start here again
     * The reason why we did't start compaion on stopPreview() is..
     * release() can be come just after stopPreview().
     * If it is, release can be delay by companion open and. then app make exception
     */
    if (m_startCompanion() != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_startCompanion fail", __FUNCTION__, __LINE__);
    }
#endif

    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;

    if (m_getMemoryCb != NULL) {
        m_fdCallbackHeap = m_getMemoryCb(-1, fdCallbackSize, 1, m_callbackCookie);
        if (!m_fdCallbackHeap || m_fdCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, fdCallbackSize);
            m_fdCallbackHeap = NULL;
            goto err;
        }
    }

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == false)
#endif
    {
        /* frame manager start */
        m_frameMgr->start();

        /*
         * This is for updating parameter value at once.
         * This must be just before making factory
         */
        m_parameters->updateTpuParameters();
    }

    /* vision */
    CLOGI("INFO(%s[%d]): getVisionMode(%d)", __FUNCTION__, __LINE__, m_parameters->getVisionMode());
    if (m_parameters->getVisionMode() == true) {
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

        m_parameters->setFrameSkipCount(INITIAL_SKIP_FRAME);

        ret = m_startVisionInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startVisionInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        m_visionThread->run(PRIORITY_DEFAULT);
        return NO_ERROR;
    } else {
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }

        if ((m_parameters->getRestartPreview() == true) ||
            m_previewBufferCount != m_parameters->getPreviewBufferCount()) {
            ret = setPreviewWindow(m_previewWindow);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            m_previewBufferCount = m_parameters->getPreviewBufferCount();
        }

        CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
        m_setBuffersThread->run(PRIORITY_DEFAULT);

        if (m_captureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            if (m_parameters->isReprocessing() == true)
                bufMgr = m_bayerBufferMgr;

            m_captureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr);

            if (m_parameters->isReprocessing() == true) {
                ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
                if (ret < 0)
                    CLOGE("ERR(%s[%d]): setFrameHoldCount(%d) is fail", __FUNCTION__, __LINE__, REPROCESSING_BAYER_HOLD_COUNT);
            }
        }

        if (m_sccCaptureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;
            if (m_parameters->isSccCapture() == true) {
                /* TODO: Dynamic select buffer manager for capture */
                bufMgr = m_sccBufferMgr;
            }
            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr);
        }

        if (m_captureSelector != NULL)
            m_captureSelector->release();

        if (m_sccCaptureSelector != NULL)
            m_sccCaptureSelector->release();

        if (m_previewFrameFactory->isCreated() == false
#ifdef CAMERA_FAST_ENTRANCE_V1
            && m_fastEntrance == false
#endif
           ) {
#ifdef SAMSUNG_COMPANION
            if(m_use_companion == true) {
                ret = m_previewFrameFactory->precreate();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->precreate() failed", __FUNCTION__, __LINE__);
                    goto err;
                }

                m_waitCompanionThreadEnd();

                ret = m_previewFrameFactory->postcreate();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->postcreate() failed", __FUNCTION__, __LINE__);
                    goto err;
                }
            } else {
                ret = m_previewFrameFactory->create();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                    goto err;
                }
            }
#else
            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->create() failed", __FUNCTION__, __LINE__);
                goto err;
            }
#endif
            CLOGD("DEBUG(%s):FrameFactory(previewFrameFactory) created", __FUNCTION__);
        }

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            m_waitFastenAeThreadEnd();
            if (m_fastenAeThreadResult < 0) {
                CLOGE("ERR(%s[%d]):fastenAeThread exit with error", __FUNCTION__, __LINE__);
                ret = m_fastenAeThreadResult;
                goto err;
            }
        } else
#endif
        {
#ifdef USE_QOS_SETTING
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
#endif

            if (m_parameters->getUseFastenAeStable() == true &&
                    m_parameters->getDualMode() == false &&
                    m_parameters->getRecordingHint() == false &&
                    m_isFirstStart == true) {

                ret = m_fastenAeStable();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
                    ret = INVALID_OPERATION;
                    goto err;
                } else {
                    skipFrameCount = 0;
                    m_parameters->setUseFastenAeStable(false);
                }
            } else if(m_parameters->getDualMode() == true) {
                skipFrameCount = INITIAL_SKIP_FRAME + 2;
            }
        }

#ifdef SET_FPS_SCENE /* This codes for 5260, Do not need other project */
        struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
        if (initMetaData != NULL) {
            m_parameters->duplicateCtrlMetadata(initMetaData);

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
        if (m_parameters->getCameraId() == CAMERA_ID_FRONT) {
            struct camera2_shot_ext *initMetaData = new struct camera2_shot_ext;
            if (initMetaData != NULL) {
                m_parameters->duplicateCtrlMetadata(initMetaData);
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

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif
            m_parameters->setFrameSkipCount(skipFrameCount);

        m_setBuffersThread->join();

        if (m_isSuccessedBufferAllocation == false) {
            CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
            goto err;
        }

        ret = m_startPreviewInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startPreviewInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        if (m_parameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
            if (!m_parameters->getEffectRecordingHint() &&
                !m_parameters->getDualRecordingHint() &&
                !m_parameters->getUHDRecordingMode())
#endif
            {
                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
            }
#endif
        } else {
            m_pictureFrameFactory = m_previewFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }

#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
        if (!m_parameters->getEffectRecordingHint() &&
            !m_parameters->getDualRecordingHint() &&
            !m_parameters->getUHDRecordingMode())
#endif
        {
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }

        if (m_previewWindow != NULL)
            m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

        /* setup frame thread */
        if (m_parameters->getDualMode() == true && (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
        } else {
            /* Comment out : SetupQThread[FLITE] is not used, but when we reduce FLITE buffer it is useful */
            /*
            switch (m_parameters->getReprocessingBayerMode()) {
            case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
                m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(NULL);
                CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
                break;
            case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
                CLOGD("DEBUG(%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
                break;
            default:
                CLOGI("INFO(%s[%d]):setupThread not started pipeID(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
                break;
            }
            */
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_3AA);
            m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
        }

        if (m_facedetectThread->isRunning() == false)
            m_facedetectThread->run();

        m_previewThread->run(PRIORITY_DISPLAY);
        m_mainThread->run(PRIORITY_DEFAULT);
        if(m_parameters->getCameraId() == CAMERA_ID_BACK) {
            m_autoFocusContinousThread->run(PRIORITY_DEFAULT);
        }
        m_monitorThread->run(PRIORITY_DEFAULT);

        if (m_parameters->getZoomPreviewWIthScaler() == true) {
            CLOGD("DEBUG(%s[%d]):ZoomPreview with Scaler Thread start", __FUNCTION__, __LINE__);
            m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == false)) {
            CLOGD("DEBUG(%s[%d]):High resolution preview callback start", __FUNCTION__, __LINE__);
            if (skipFrameCount > 0)
                m_skipReprocessing = true;
            m_highResolutionCallbackRunning = true;
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                if (m_parameters->isUseHWFC() == true) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                }

                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
                m_startPictureInternalThread->join();
            }
            m_prePictureThread->run(PRIORITY_DEFAULT);
        }

        /* FD-AE is always on */
#ifdef USE_FD_AE
        m_enableFaceDetection(m_parameters->getFaceDetectionMode());
#endif

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
    }

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif

    return NO_ERROR;

err:

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        m_waitCompanionThreadEnd();
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }

    m_fastEntrance = false;
#endif

    /* frame manager stop */
    m_frameMgr->stop();

    m_setBuffersThread->join();

    m_releaseBuffers();

    return ret;
}

status_t ExynosCamera::startPreviewVision()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::stopPreview()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

    char previewMode[100] = {0};

    property_get("persist.sys.camera.preview", previewMode, "");

    if (!strstr(previewMode, "0"))
        CLOGI("INFO(%s[%d]) persist.sys.camera.preview(%s)", __FUNCTION__, __LINE__, previewMode);

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrame *frame = NULL;

    m_flagLLSStart = false;

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_companionThread", __FUNCTION__, __LINE__);
            m_companionThread->requestExitAndWait();
            CLOGI("INFO(%s[%d]): wait m_companionThread end", __FUNCTION__, __LINE__);
        } else {
            CLOGI("INFO(%s[%d]): m_companionThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }

    m_fastEntrance = false;
#endif

    if (m_previewEnabled == false) {
        CLOGD("DEBUG(%s[%d]): preview is not enabled", __FUNCTION__, __LINE__);
        property_set("persist.sys.camera.preview","0");
        return;
    }

    m_stopLongExposure = true;

    if (m_pictureEnabled == true) {
        CLOGW("WARN(%s[%d]):m_pictureEnabled == true (picture is not finished)", __FUNCTION__, __LINE__);
        int retry = 0;
        do {
            usleep(WAITING_TIME);
            retry++;
        } while(m_pictureEnabled == true && retry < (TOTAL_WAITING_TIME/WAITING_TIME));
        CLOGW("WARN(%s[%d]):wait (%d)msec (because, picture is not finished)", __FUNCTION__, __LINE__, WAITING_TIME * retry / 1000);
    }

    m_parameters->setIsThumbnailCallbackOn(false);

    if (m_parameters->getVisionMode() == true) {
        m_visionThread->requestExitAndWait();
        ret = m_stopVisionInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopVisionInternal fail", __FUNCTION__, __LINE__);
    } else {
#ifdef USE_QOS_SETTING
        if (m_previewFrameFactory == NULL) {
            CLOGW("WARN(%s[%d]):m_previewFrameFactory is NULL. so, skip setControl(V4L2_CID_IS_DVFS_CLUSTER1)", __FUNCTION__, __LINE__);
        } else {
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, 0, PIPE_3AA);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        }
#endif
        m_startPictureInternalThread->join();

        m_startPictureBufferThread->join();

        m_autoFocusRunning = false;
        m_exynosCameraActivityControl->cancelAutoFocus();

        CLOGD("DEBUG(%s[%d]): (%d, %d)", __FUNCTION__, __LINE__, m_flashMgr->getNeedCaptureFlash(), m_pictureEnabled);
        if (m_flashMgr->getNeedCaptureFlash() == true && m_pictureEnabled == true) {
            CLOGD("DEBUG(%s[%d]): force flash off", __FUNCTION__, __LINE__);
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
            CLOGD("DEBUG(%s[%d]): flash torch was enabled", __FUNCTION__, __LINE__);

            m_parameters->setFrameSkipCount(100);
            do {
                if (m_flashMgr->checkFlashOff() == false) {
                    usleep(waitingTime);
                } else {
                    CLOGD("DEBUG(%s[%d]):turn off the flash torch.(%d)", __FUNCTION__, __LINE__, i);

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
                CLOGD("DEBUG(%s[%d]):timeOut-flashMode(%d),checkFlashOff(%d)",
                    __FUNCTION__, __LINE__, flashMode, m_flashMgr->checkFlashOff());
            }
        } else if (m_isTryStopFlash == true) {
            usleep(waitingTime*3);  /* 150ms */
            m_flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
        }

        m_flashMgr->setNeedFlashOffDelay(false);

        m_previewFrameFactory->setStopFlag();
        if (m_parameters->isReprocessing() == true && m_reprocessingFrameFactory->isCreated() == true)
            m_reprocessingFrameFactory->setStopFlag();
        m_flagThreadStop = true;

        m_takePictureCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_pictureCounter.clearCount();
        m_jpegCounter.clearCount();
        m_yuvcallbackCounter.clearCount();
        m_captureSelector->cancelPicture();

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
            m_skipReprocessing = false;
            m_highResolutionCallbackRunning = false;
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
                if (m_parameters->isUseHWFC() == true) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                }
            }

            CLOGD("DEBUG(%s[%d]):High resolution preview callback stop", __FUNCTION__, __LINE__);
            if(getCameraId() == CAMERA_ID_FRONT ||
               getCameraId() == CAMERA_ID_BACK_1) {
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

        m_zoomPreviwWithCscThread->stop();
        m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
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

        if (m_previewCallbackThread->isRunning()) {
            m_previewCallbackThread->stop();
            if (m_previewCallbackQ != NULL) {
                m_previewCallbackQ->sendCmd(WAKE_UP);
            }
            m_previewCallbackThread->requestExitAndWait();
        }

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
        while (m_facedetectQ->getSizeOfProcessQ()) {
            m_facedetectQ->popProcessQ(&frame);
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

        if (m_previewQ != NULL) {
             m_clearList(m_previewQ);
        }

        if (m_previewCallbackQ != NULL) {
            m_clearList(m_previewCallbackQ);
        }

        if (m_zoomPreviwWithCscQ != NULL) {
             m_zoomPreviwWithCscQ->release();
        }

        if (m_previewCallbackGscFrameDoneQ != NULL) {
            m_clearList(m_previewCallbackGscFrameDoneQ);
        }

        ret = m_stopPreviewInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
    }

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
#ifdef DEBUG_RAWDUMP
    if (m_parameters->getUsePureBayerReprocessing() == false) {
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
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }
    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->resetBuffers();
    }

    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->resetBuffers();
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

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif

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

    /*
     * close companion : other node all closed in m_deinitFrameFactory()
     * so, we close also companion
     */
    if (m_stopCompanion() != NO_ERROR)
        CLOGE("ERR(%s[%d]):m_stopCompanion() fail", __FUNCTION__, __LINE__);

    m_initFrameFactory();
#endif

    property_set("persist.sys.camera.preview","0");
}

void ExynosCamera::stopPreviewVision()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    property_set("persist.sys.camera.preview","0");
}

status_t ExynosCamera::startRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    int ret = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    flashMgr->setCaptureStatus(false);

    if (m_getRecordingEnabled() == true) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals true", __FUNCTION__, __LINE__);
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
        CLOGW("(%s[%d]):m_parameters is NULL", __FUNCTION__, __LINE__);
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
    m_parameters->setRecordingRunning(true);

    autoFocusMgr->setRecordingHint(true);
    flashMgr->setRecordingHint(true);
    if (m_parameters->doCscRecording() == true)
        m_previewFrameFactory->setRequest(PIPE_MCSC2, true);

func_exit:
    /* wait for initial preview skip */
    if (m_parameters != NULL) {
        int retry = 0;
        while (m_parameters->getFrameSkipCount() > 0 && retry < 3) {
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

    if (m_parameters == NULL) {
        CLOGE("ERR(%s[%d]):m_parameters is NULL!", __FUNCTION__, __LINE__);
        return;
    }

    if (m_parameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
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
        CLOGE("ERR(%s[%d]):m_stopRecordingInternal fail", __FUNCTION__, __LINE__);

#ifdef USE_FD_AE
    if (m_parameters != NULL) {
        m_enableFaceDetection(m_parameters->getFaceDetectionMode(false));
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
    ExynosCameraFrame *newFrame = NULL;
    int32_t reprocessingBayerMode = 0;
    int retryCount = 0;
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

    if (m_previewEnabled == false) {
        CLOGE("DEBUG(%s[%d]):preview is stopped, return error", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    if (m_parameters == NULL) {
        CLOGE("ERR(%s[%d]):m_parameters is NULL", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    while (m_longExposurePreview == true) {
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
            CLOGE("DEBUG(%s[%d]):HAL can't set preview exposure time.", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    retryCount = 0;

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

    seriesShotCount = m_parameters->getSeriesShotCount();
    currentSeriesShotMode = m_parameters->getSeriesShotMode();
    reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    if (m_parameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
        android_printAssert(NULL, LOG_TAG, "Cannot support this operation");
        return INVALID_OPERATION;
    }

    if (m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
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

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        int buffercount = 1;
        int pipeId = m_getBayerPipeId();
        ExynosCameraBufferManager *fliteBufferMgr = m_bayerBufferMgr;

        m_captureSelector->clearList(pipeId, false);

        for (int i = 0; i < buffercount; i++) {
            if (fliteBufferMgr->getNumOfAvailableBuffer() > 0) {
                m_previewFrameFactory->setRequestFLITE(true);

                ret = m_generateFrame(-1, &newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_previewFrameFactory->setRequestFLITE(false);

                ret = m_setupEntity(pipeId, newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
            }
        }

        m_previewFrameFactory->startThread(pipeId);
    } else if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Comment out, because it included 3AA, it always running */
        /*
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequest3AC(true);
            ret = m_generateFrame(-1, &newFrame);
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

            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
        */
        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0)
            m_previewFrameFactory->setRequest3AC(true);
    }

#ifdef DEBUG_RAWDUMP
    if (m_parameters->getUsePureBayerReprocessing() == false) {
        int buffercount = 1;
        int pipeId = PIPE_FLITE;
        ExynosCameraBufferManager *fliteBufferMgr = m_fliteBufferMgr;

        for (int i = 0; i < buffercount; i++) {
            if (fliteBufferMgr->getNumOfAvailableBuffer() > 0) {
                m_previewFrameFactory->setRequestFLITE(true);

                ret = m_generateFrame(-1, &newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_previewFrameFactory->setRequestFLITE(false);

                ret = m_setupEntity(pipeId, newFrame);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
                    return ret;
                }

                m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            }
        }

        m_previewFrameFactory->startThread(pipeId);
    }
#endif

    if (m_takePictureCounter.getCount() == seriesShotCount) {
        m_stopBurstShot = false;

        if (m_parameters->isReprocessing() == true)
            m_captureSelector->setIsFirstFrame(true);
        else
            m_sccCaptureSelector->setIsFirstFrame(true);

#if 0
        if (m_parameters->isReprocessing() == false || m_parameters->getSeriesShotCount() > 0 ||
                m_hdrEnabled == true) {
            m_pictureFrameFactory = m_previewFrameFactory;
            if (m_parameters->getUseDynamicScc() == true) {
                if (m_parameters->isOwnScc(getCameraId()) == true)
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

        if (m_parameters->getScalableSensorMode()) {
            m_parameters->setScalableSensorMode(false);
            stopPreview();
            setPreviewWindow(m_previewWindow);
            startPreview();
            m_parameters->setScalableSensorMode(true);
        }

        CLOGI("INFO(%s[%d]): takePicture enabled, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_pictureEnabled = true;
        m_takePictureCounter.decCount();
        m_isNeedAllocPictureBuffer = true;

        m_startPictureBufferThread->join();

        if (m_parameters->isReprocessing() == true) {
            m_startPictureInternalThread->join();

            if (seriesShotCount > 1)
            {
                int allocCount = 0;
                int addCount = 0;
                CLOGD("DEBUG(%s[%d]): realloc buffer for burst shot", __FUNCTION__, __LINE__);
                m_burstRealloc = false;

                if (m_parameters->isUseHWFC() == false) {
                    allocCount = m_sccReprocessingBufferMgr->getAllocatedBufferCount();
                    addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                    if( addCount > 0 ){
                        m_sccReprocessingBufferMgr->increase(addCount);
                    }
                }

                allocCount = m_jpegBufferMgr->getAllocatedBufferCount();
                addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                if( addCount > 0 ){
                    m_jpegBufferMgr->increase(addCount);
                }
                m_isNeedAllocPictureBuffer = true;
            }
        }

        CLOGD("DEBUG(%s[%d]): currentSeriesShotMode(%d), flashMgr->getNeedCaptureFlash(%d)",
            __FUNCTION__, __LINE__, currentSeriesShotMode, flashMgr->getNeedCaptureFlash());

        if (m_parameters->getCaptureExposureTime() != 0)
            m_longExposureRemainCount = m_parameters->getLongExposureShotCount();

        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            sCaptureMgr->resetHdrStartFcount();
            m_parameters->setFrameSkipCount(13);
        } else if ((flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE)
#ifdef FLASHED_LLS_CAPTURE
                || (flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_LLS
#endif
        ) {
            if (m_parameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                flashMgr->setManualExposureTime(m_parameters->getLongExposureTime() * 1000);
            }

            m_parameters->setMarkingOfExifFlash(1);

            if (flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                CLOGD("DEBUG(%s[%d]): checkPreFlash(false), Start auto focus internally", __FUNCTION__, __LINE__);
                m_autoFocusType = AUTO_FOCUS_HAL;
                flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);
                flashMgr->setFlashWaitCancel(false);

                /* execute autoFocus for preFlash */
                m_autoFocusThread->requestExitAndWait();
                m_autoFocusThread->run(PRIORITY_DEFAULT);
            }
        } else {
            if (m_parameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                m_parameters->setExposureTime();
            }
        }

        m_parameters->setSetfileYuvRange();

        m_reprocessingCounter.setCount(seriesShotCount);
        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
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
                CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
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
        } else {
            m_jpegCallbackThread->join();
            if (m_parameters->getCaptureExposureTime() <= CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                ret = m_jpegCallbackThread->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
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
        } else {
#ifdef ONE_SECOND_BURST_CAPTURE
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
            {
                /* series shot : push buffer to callback thread. */
                m_jpegCallbackThread->join();
                ret = m_jpegCallbackThread->run();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                    return INVALID_OPERATION;
                }
            }
        }

        CLOGI("INFO(%s[%d]): series shot takePicture, takePictureCount(%d)",
                __FUNCTION__, __LINE__, m_takePictureCounter.getCount());
        m_takePictureCounter.decCount();

        /* TODO : in case of no reprocesssing, we make zsl scheme or increse buf */
        if (m_parameters->isReprocessing() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
}

status_t ExynosCamera::cancelAutoFocus()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#if 0 // not used.
    if (m_parameters != NULL) {
        if (m_parameters->getFocusMode() == FOCUS_MODE_AUTO) {
            CLOGI("INFO(%s[%d]) ae awb unlock", __FUNCTION__, __LINE__);
            m_parameters->m_setAutoExposureLock(false);
            m_parameters->m_setAutoWhiteBalanceLock(false);
        }
    }
#endif

    if (m_exynosCameraActivityControl->cancelAutoFocus() == false) {
        CLOGE("ERR(%s):Fail on m_secCamera->cancelAutoFocus()", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* if autofocusThread is running, we should be wait to receive the AF reseult. */
    m_autoFocusLock.lock();
    m_autoFocusLock.unlock();

    return NO_ERROR;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    status_t ret = NO_ERROR;
    bool flagRestart = false;

    if(m_parameters == NULL)
        return INVALID_OPERATION;

#ifdef SCALABLE_ON
    m_parameters->setScalableSensorMode(true);
#else
    m_parameters->setScalableSensorMode(false);
#endif

    flagRestart = m_parameters->checkRestartPreview(params);

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_isFirstParametersSet == false
        && m_parameters->getDualMode() == false
        && m_parameters->getVisionMode() == false) {
        CLOGD("DEBUG(%s[%d]):1st setParameters", __FUNCTION__, __LINE__);
        m_fastEntrance = true;
        m_isFirstParametersSet = true;
        m_fastenAeThread->run();
    }
#endif

#if 1
    /* HACK Reset Preview Flag*/
    if (flagRestart == true && m_previewEnabled == true) {
        m_resetPreview = true;
        ret = m_restartPreviewInternal(true, (CameraParameters*)&params);
        m_resetPreview = false;
        CLOGI("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);

        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
    } else {
        ret = m_parameters->setParameters(params);
    }
#endif
    return ret;
}

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, __unused int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;

    CLOGV("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    } else {
        CLOGE("ERR(%s):m_parameters is NULL", __FUNCTION__);
        return INVALID_OPERATION;
    }

    /* TO DO : implemented based on the command */
    switch(command) {
    case CAMERA_CMD_START_FACE_DETECTION:
    case CAMERA_CMD_STOP_FACE_DETECTION:
        if (m_parameters->getMaxNumDetectedFaces() == 0) {
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
                && m_startFaceDetection() == false) {
                CLOGE("ERR(%s):startFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        } else {
            CLOGD("DEBUG(%s):CAMERA_CMD_STOP_FACE_DETECTION is called!", __FUNCTION__);
            if ( m_flagStartFaceDetection == true
                && m_stopFaceDetection() == false) {
                CLOGE("ERR(%s):stopFaceDetection() fail", __FUNCTION__);
                return BAD_VALUE;
            }
        }
        break;
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
    /* 1508: HAL_SET_SAMSUNG_CAMERA */
    case 1508 :
        CLOGD("DEBUG(%s):HAL_SET_SAMSUNG_CAMERA is called!%d", __FUNCTION__, arg1);
        m_parameters->setSamsungCamera(true);
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        CLOGD("DEBUG(%s):CAMERA_CMD_SET_FLIP is called!%d", __FUNCTION__, arg1);
        m_parameters->setFlipHorizontal(arg1);
        break;
    /* 1521: CAMERA_CMD_DEVICE_ORIENTATION */
    case 1521:
        CLOGD("DEBUG(%s):CAMERA_CMD_DEVICE_ORIENTATION is called!%d", __FUNCTION__, arg1);
        m_parameters->setDeviceOrientation(arg1);
        uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
        if (uctlMgr != NULL)
            uctlMgr->setDeviceRotation(m_parameters->getFdOrientation());
        break;
    /*1641: CAMERA_CMD_ADVANCED_MACRO_FOCUS*/
    case 1641:
        CLOGD("DEBUG(%s):CAMERA_CMD_ADVANCED_MACRO_FOCUS is called!%d", __FUNCTION__, arg1);
        m_parameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER);
        break;
    /*1642: CAMERA_CMD_FOCUS_LOCATION*/
    case 1642:
        CLOGD("DEBUG(%s):CAMERA_CMD_FOCUS_LOCATION is called!%d", __FUNCTION__, arg1);
        m_parameters->setAutoFocusMacroPosition(ExynosCameraActivityAutofocus::AUTOFOCUS_MACRO_POSITION_CENTER_UP);
        break;
    /*1661: CAMERA_CMD_START_ZOOM */
    case 1661:
        CLOGD("DEBUG(%s):CAMERA_CMD_START_ZOOM is called!", __FUNCTION__);
        m_parameters->setZoomActiveOn(true);
        m_parameters->setFocusModeLock(true);
        break;
    /*1662: CAMERA_CMD_STOP_ZOOM */
    case 1662:
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_ZOOM is called!", __FUNCTION__);
        m_parameters->setZoomActiveOn(false);
        m_parameters->setFocusModeLock(false);
        break;
    default:
        CLOGV("DEBUG(%s):unexpectect command(%d)", __FUNCTION__, command);
        break;
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrame *frame)
{
    int ret = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *fdFrame = NULL;

    ExynosCameraBuffer buffer;
    ExynosCameraBuffer previewcallbackbuffer;
    ExynosCameraBuffer t3acBuffer;
    int pipeId = -1;
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t skipCount = 0;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer resultBuffer;
    camera2_node_group node_group_info_isp;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    int ispDstBufferIndex = -1;
    uint32_t frameCnt = 0;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;

    entity = frame->getFrameDoneFirstEntity();
    if (entity == NULL) {
        CLOGE("ERR(%s[%d]):current entity is NULL, HAL-frameCount(%d)",
                __FUNCTION__, __LINE__, frame->getFrameCount());
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    pipeId = entity->getPipeId();

    m_debugFpsCheck(pipeId);

    /* TODO: remove hard coding */
    switch (pipeId) {
    case PIPE_FLITE:
        if (m_parameters->isReprocessing() == true
            && m_parameters->getUsePureBayerReprocessing() == true) {
        /* Do not use this function: We will modify */
#if 0
            if (m_pictureEnabled == true) {
                m_captureSelector->setFrameHoldCount(0);

                ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                    return ret;
                }
                m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            } else
#endif
            {
                ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                    return ret;
                }
            }
        }

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            return ret;
        }

        /* TODO: Dynamic bayer capture, currently support only single shot */
        if (m_parameters->isFlite3aaOtf() == true
            && reprocessingBayerMode != REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
            && m_previewFrameFactory->getRunningFrameCount(pipeId) == 0)
            m_previewFrameFactory->stopThread(pipeId);

        /*
         * Comment out : Push frame to FLITE pipe is setup3AA thread's works,
         *               but when we reduce FLITE buffer, it is useful.
         */
        /*
        if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON) {
            newFrame = m_frameMgr->createFrame(m_parameters, 0);
            m_mainSetupQ[pipeId]->pushProcessQ(&newFrame);
        }
        */

        if (m_parameters->isFlite3aaOtf() == false) {
            ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, PIPE_3AA, ret);

            m_previewFrameFactory->pushFrameToPipe(&frame, PIPE_3AA);
        }

#ifdef DEBUG_RAWDUMP
        if (m_parameters->getUsePureBayerReprocessing() == false) {
            if (buffer.index >= 0) {
                if (m_parameters->checkBayerDumpEnable()) {
                    int sensorMaxW, sensorMaxH;
                    int sensorMarginW, sensorMarginH;
                    bool bRet;
                    char filePath[70];

                    memset(filePath, 0, sizeof(filePath));
                    snprintf(filePath, sizeof(filePath), "/data/dump/PureRawCapture%d_%d.raw", m_cameraId, frame->getFrameCount());
                    if (m_parameters->getUsePureBayerReprocessing() == true) {
                        /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
                        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
                    } else {
                        m_parameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);
                    }

                    CLOGE("INFO(%s[%d]): Sensor (%d x %d) frame(%d)", \
                            __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH, frame->getFrameCount());

                    bRet = dumpToFile((char *)filePath,
                        buffer.addr[0],
                        buffer.size[0]);
                    if (bRet != true)
                        CLOGE("couldn't make a raw file");
                }

                ret = m_putBuffers(m_fliteBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                }
            }
        }
#endif

        CLOGV("DEBUG(%s[%d]):FLITE driver-frameCount(%d) HAL-frameCount(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

	break;
    case PIPE_3AA:
        /*
        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR)
            m_previewFrameFactory->dump();
        */

        /* Trigger CAF thread */
        if (getCameraId() == CAMERA_ID_BACK) {
            frameCnt = frame->getFrameCount();
            m_autoFocusContinousQ.pushProcessQ(&frameCnt);
        }

        /* Return src buffer to buffer manager */
        if (m_parameters->isFlite3aaOtf() == false) {
            if (m_parameters->isReprocessing() == false) {
                ret = frame->getSrcBuffer(pipeId, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId, ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                    }
                }
            }

            /* Create new frame after return buffer */
            newFrame = m_frameMgr->createFrame(m_parameters, 0);
            m_mainSetupQ[PIPE_FLITE]->pushProcessQ(&newFrame);
        }

        CLOGV("DEBUG(%s[%d]):3AA driver-frameCount(%d) HAL-frameCount(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        if (frame->getRequest(PIPE_3AC) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId, ret);
            }

            if (buffer.index >= 0) {
                if (m_parameters->getHighSpeedRecording() == true) {
                    ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                        break;
                    }
                } else {
                    if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)) != ENTITY_BUFFER_STATE_ERROR) {
                        ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                            return ret;
                        }
                    } else {
                        ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):m_putBuffers(m_bayerBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                            break;
                        }
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
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_ispBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                    break;
                }
            }
        }

        CLOGV("DEBUG(%s[%d]):ISP driver-frameCount(%d) HAL-frameCount(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
                frame->getFrameCount());

        if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false)
            break;
    case PIPE_TPU:
        if (pipeId == PIPE_TPU) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(m_hwDisBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                }
            }
        }

        CLOGV("DEBUG(%s[%d]):TPU driver-frameCount(%d) HAL-frameCount(%d)",
                __FUNCTION__, __LINE__,
                getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[1]),
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
                CLOGE("ERR(%s[%d]):getSrcBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_putBuffers(m_hwDisBufferMgr, buffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_putBuffers(m_hwDisBufferMgr, %d) fail", __FUNCTION__, __LINE__, buffer.index);
                }
            }
        }

        ret = getYuvFormatInfo(m_parameters->getHwPreviewFormat(), &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                __FUNCTION__, __LINE__, m_parameters->getHwPreviewFormat());
            return ret;
        }

        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("ERR(%s[%d]):Src buffer state error, pipeId(%d)",
                    __FUNCTION__, __LINE__, pipeId);

            /* Preview : PIPE_MCSC0 */
            if (frame->getRequest(PIPE_MCSC0) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC0));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                }
            }

            /* Preview Callback : PIPE_MCSC1 */
            if (frame->getRequest(PIPE_MCSC1) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC1));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, buffer.index);
                    if (ret < 0)
                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                }
            }

            /* Recording : PIPE_MCSC2 */
            if (frame->getRequest(PIPE_MCSC2) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC2));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Recording buffer return fail", __FUNCTION__, __LINE__);
                    }
                }
            }

            goto entity_state_complete;
        } else {
            int previewPipeId = PIPE_MCSC0;
            int previewCallbackPipeId = PIPE_MCSC1;
            int recordingPipeId = PIPE_MCSC2;
            nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
            previewcallbackbuffer.index = -2;

            m_parameters->getFrameSkipCount(&m_frameSkipCount);

            /* Face detection */
            if (m_parameters->getHighSpeedRecording() == false
                && frame->getFrameState() != FRAME_STATE_SKIPPED) {
                if (m_parameters->getFrameSkipCount() <= 0
                    && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
                    fdFrame = m_frameMgr->createFrame(m_parameters, frame->getFrameCount());
                    m_copyMetaFrameToFrame(frame, fdFrame, true, true);
                    m_facedetectQ->pushProcessQ(&fdFrame);
                }
            }

            /* Preview callback : PIPE_MCSC1 */
            if (frame->getRequest(previewCallbackPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &previewcallbackbuffer,
                                            m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                if (bufferState != ENTITY_BUFFER_STATE_COMPLETE) {
                    CLOGD("DEBUG(%s[%d]):Preview callback handling droped \
                            - preview callback buffer is not ready HAL-frameCount(%d)",
                            __FUNCTION__, __LINE__, frame->getFrameCount());

                    if (previewcallbackbuffer.index >= 0) {
                        ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):Recording buffer return fail", __FUNCTION__, __LINE__);
                        }
                        previewcallbackbuffer.index = -2;
                    }

                    /* For debug */
                    /* m_previewFrameFactory->dump(); */
                }
            }

            /* Preview : PIPE_MCSC0 */
            if (frame->getRequest(previewPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(previewPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewPipeId));
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    /* drop preview frame if lcd supported frame rate < scp frame rate */
                    frame->getFpsRange(&minFps, &maxFps);
                    if (dispFps < maxFps) {
                        ratio = (int)((maxFps * 10 / dispFps) / 10);

#if 0
                        if (maxFps > 60)
                            ratio = ratio * 2;
#endif
                        m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
                        skipPreview = (m_displayPreviewToggle == 0) ? false : true;
#ifdef DEBUG
                        CLOGE("DEBUG(%s[%d]):preview frame skip! frameCount(%d) (m_displayPreviewToggle=%d, \
                                maxFps=%d, dispFps=%d, ratio=%d, skipPreview=%d)",
                                __FUNCTION__, __LINE__, frame->getFrameCount(), m_displayPreviewToggle,
                                maxFps, dispFps, ratio, (int)skipPreview);
#endif
                    }

                    memset(m_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
                    frame->getDynamicMeta(m_meta_shot);
                    frame->getUserDynamicMeta(m_meta_shot);

                    m_checkEntranceLux(m_meta_shot);




                    if ((m_frameSkipCount > 0) || (skipPreview == true)) {
                        CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);

                        if (buffer.index >= 0) {
                            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                            if (ret != NO_ERROR)
                                CLOGE("ERR(%s[%d]):Preview buffer return fail", __FUNCTION__, __LINE__);
                        }

                        if (previewcallbackbuffer.index >= 0) {
                            ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                            }
                        }
                    } else {
                        if (m_skipReprocessing == true)
                            m_skipReprocessing = false;

                        ExynosCameraFrame *previewFrame = NULL;
                        struct camera2_shot_ext *shot_ext = new struct camera2_shot_ext;

                        frame->getMetaData(shot_ext);
                        previewFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(previewPipeId, frame->getFrameCount());
                        previewFrame->storeDynamicMeta(shot_ext);
                        previewFrame->setDstBuffer(previewPipeId, buffer);
                        if (previewcallbackbuffer.index >= 0) {
                            previewFrame->setSrcBuffer(previewPipeId, previewcallbackbuffer);
                            previewFrame->setRequest(previewCallbackPipeId, true);
                        }

                        int previewBufferCount = m_parameters->getPreviewBufferCount();
                        int sizeOfPreviewQ = m_previewQ->getSizeOfProcessQ();
                        if (m_previewThread->isRunning() == true
                            && ((previewBufferCount == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS && sizeOfPreviewQ >= 2)
                                || (previewBufferCount == NUM_PREVIEW_BUFFERS && sizeOfPreviewQ >= 1))) {
                            if ((m_getRecordingEnabled() == true) && (m_parameters->doCscRecording() == false)) {
                                CLOGW("WARN(%s[%d]):push frame to previewQ. PreviewQ(%d), PreviewBufferCount(%d)",
                                        __FUNCTION__, __LINE__, m_previewQ->getSizeOfProcessQ(),
                                        m_parameters->getPreviewBufferCount());
                                        m_previewQ->pushProcessQ(&previewFrame);
                            } else {
                                CLOGW("WARN(%s[%d]):Frames are stacked in previewQ. Skip frame. PreviewQ(%d), PreviewBufferCount(%d)",
                                        __FUNCTION__, __LINE__,
                                        m_previewQ->getSizeOfProcessQ(),
                                        m_parameters->getPreviewBufferCount());

                                if (buffer.index >= 0) {
                                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                    if (ret != NO_ERROR)
                                        CLOGE("ERR(%s[%d]):Preview buffer return fail", __FUNCTION__, __LINE__);
                                }

                                if (previewcallbackbuffer.index >= 0) {
                                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                                    }
                                }

                                previewFrame->decRef();
                                m_frameMgr->deleteFrame(previewFrame);
                                previewFrame = NULL;
                            }
                        } else if((m_parameters->getFastFpsMode() > 1) && (m_parameters->getRecordingHint() == 1)) {
                            short skipInterval = (m_parameters->getFastFpsMode() - 1) * 2;
                            m_skipCount++;

                            if((m_parameters->getShotMode() == SHOT_MODE_SEQUENCE && (m_skipCount % 4 != 0))
                                || (m_parameters->getShotMode() != SHOT_MODE_SEQUENCE && (m_skipCount % skipInterval) > 0)) {
                                CLOGV("INFO(%s[%d]):fast fps mode skip frame(%d) ", __FUNCTION__, __LINE__, m_skipCount);
                                if (buffer.index >= 0) {
                                    /* only apply in the Full OTF of Exynos74xx. */
                                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                    if (ret < 0)
                                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                                }

                                if (previewcallbackbuffer.index >= 0) {
                                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                                    }
                                }

                                previewFrame->decRef();
                                m_frameMgr->deleteFrame(previewFrame);
                                previewFrame = NULL;
                            } else {
                                CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                m_previewQ->pushProcessQ(&previewFrame);
                            }
                        } else {
                            if (m_getRecordingEnabled() == true) {
                                CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                m_previewQ->pushProcessQ(&previewFrame);
                                m_longExposurePreview = false;
                            } else if (m_parameters->getCaptureExposureTime() > 0
                                       && (m_meta_shot->shot.dm.sensor.exposureTime / 1000) > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                                CLOGD("INFO(%s[%d]):manual exposure capture mode. %lld",
                                        __FUNCTION__, __LINE__, m_meta_shot->shot.dm.sensor.exposureTime);

                                if (buffer.index >= 0) {
                                    /* only apply in the Full OTF of Exynos74xx. */
                                    ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                    if (ret != NO_ERROR)
                                        CLOGE("ERR(%s[%d]):SCP buffer return fail", __FUNCTION__, __LINE__);
                                }

                                if (previewcallbackbuffer.index >= 0) {
                                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                                    }
                                }

                                previewFrame->decRef();
                                m_frameMgr->deleteFrame(previewFrame);
                                previewFrame = NULL;

                                m_longExposurePreview = true;
                            } else {
                                CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                m_previewQ->pushProcessQ(&previewFrame);
                                m_longExposurePreview = false;
                            }

                            /* Recording : case of adaptive CSC recording */
                            if (m_getRecordingEnabled() == true
                                && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                && m_parameters->doCscRecording() == false
                                && frame->getRequest(recordingPipeId) == false) {
                                if (timeStamp <= 0L) {
                                    CLOGW("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                                } else {
                                    ExynosCameraFrame *adaptiveCscRecordingFrame = NULL;
                                    adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());


                                {
                                    m_recordingTimeStamp[buffer.index] = timeStamp;
                                    adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, buffer);
                                }

                                    ret = m_insertFrameToList(&m_recordingProcessList, adaptiveCscRecordingFrame, &m_recordingProcessListLock);
                                    if (ret != NO_ERROR) {
                                        CLOGE("ERR(%s[%d]):[F%d]Failed to insert frame into m_recordingProcessList",
                                                __FUNCTION__, __LINE__, adaptiveCscRecordingFrame->getFrameCount());
                                    }
                                    m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                }
                            }
                        }
                        delete shot_ext;
                        shot_ext = NULL;
                    }

                    CLOGV("DEBUG(%s[%d]):Preview handling done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
                } else {
                    CLOGD("DEBUG(%s[%d]):Preview handling droped - SCP buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

                    if (buffer.index >= 0) {
                        ret = m_scpBufferMgr->cancelBuffer(buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):Preview buffer return fail", __FUNCTION__, __LINE__);
                        }
                    }

                    if (previewcallbackbuffer.index >= 0) {
                        ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                        }
                    }

                    /* For debug */
                    /* m_previewFrameFactory->dump(); */
                }
            }

            /* Recording : PIPE_MCSC2 */
            if (frame->getRequest(recordingPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(recordingPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(recordingPipeId));
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    if (m_frameSkipCount > 0) {
                        CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);

                        if (buffer.index >= 0) {
                            ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("ERR(%s[%d]):Recording buffer return fail", __FUNCTION__, __LINE__);
                            }
                        }
                    } else {
                        if (m_getRecordingEnabled() == true
                                && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME) == true) {
                            if (timeStamp <= 0L) {
                                CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                            } else {
                                ExynosCameraFrame *recordingFrame = NULL;
                                recordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());
                                m_recordingTimeStamp[buffer.index] = timeStamp;
                                recordingFrame->setDstBuffer(recordingPipeId, buffer);

                                ret = m_insertFrameToList(&m_recordingProcessList, recordingFrame, &m_recordingProcessListLock);
                                if (ret != NO_ERROR) {
                                    CLOGE("ERR(%s[%d]):[F%d]Failed to insert frame into m_recordingProcessList",
                                            __FUNCTION__, __LINE__, recordingFrame->getFrameCount());
                                }

                                m_recordingQ->pushProcessQ(&recordingFrame);
                            }
                        } else {
                            if (buffer.index >= 0) {
                                ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                                if (ret != NO_ERROR) {
                                    CLOGE("ERR(%s[%d]):Recording buffer return fail", __FUNCTION__, __LINE__);
                                }
                            }
                        }
                    }

                    CLOGV("DEBUG(%s[%d]):Recording handling done HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());
                } else {
                    CLOGD("DEBUG(%s[%d]):Recording handling droped - Recording buffer is not ready HAL-frameCount(%d)", __FUNCTION__, __LINE__, frame->getFrameCount());

                    if (buffer.index >= 0) {
                        ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("ERR(%s[%d]):Recording buffer return fail", __FUNCTION__, __LINE__);
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
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            __FUNCTION__, __LINE__, pipeId, ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, frame, &m_processListLock);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        frame->decRef();
        m_frameMgr->deleteFrame(frame);
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
#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
#endif
    ExynosCameraBuffer scpBuffer;
    ExynosCameraBuffer previewCbBuffer;
    bool copybuffer = false;

    ExynosCameraFrame *frame = NULL;
    nsecs_t timeStamp = 0;
    int frameCount = -1;
    frame_queue_t *previewQ;

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    /* Check the Slow/Fast Motion Scenario - sensor : 120fps, preview : 60fps */
    if(((curMinFps == 120) && (curMaxFps == 120))
         || ((curMinFps == 240) && (curMaxFps == 240))) {
        CLOGV("(%s[%d]) : Change PreviewDuration from (%d,%d) to (60000, 60000)", __FUNCTION__, __LINE__, curMinFps, curMaxFps);
        curMinFps = 60;
        curMaxFps = 60;
    }
#endif

    pipeId    = PIPE_SCP;
    pipeIdCsc = PIPE_GSC;
    previewCallbackPipeId = PIPE_MCSC1;
    previewCbBuffer.index = -2;
    scpBuffer.index = -2;

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
    ret = frame->getDstBuffer(pipeId, &scpBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        scpBuffer.index = -2;
        goto func_exit;
    }

    if (frame->getRequest(previewCallbackPipeId) == true) {
        ret = frame->getSrcBuffer(pipeId, &previewCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
        }
    }

    /* ------------- frome here "frame" cannot use ------------- */
    CLOGV("INFO(%s[%d]):push frame to previewReturnQ", __FUNCTION__, __LINE__);
    if(m_parameters->increaseMaxBufferOfPreview()) {
        maxbuffers = m_parameters->getPreviewBufferCount();
    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (scpBuffer.index < 0 || scpBuffer.index >= maxbuffers ) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, maxbuffers, scpBuffer.index);
        scpBuffer.index = -2;
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)", __FUNCTION__, __LINE__,
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
        ) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                    __FUNCTION__, __LINE__);
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
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false) {

            m_previewFrameFactory->setRequest(PIPE_MCSC1, true);

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
                return ret;
            }

            /* Exynos8890 has MC scaler, so it need not make callback frame */
            int bufIndex = -2;
            m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
            ret = m_doPreviewToCallbackFunc(pipeIdCsc, scpBuffer, previewCbBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_doPreviewToCallbackFunc fail", __FUNCTION__, __LINE__);
                goto func_exit;
            }

            ret = frame->setSrcBuffer(pipeId, previewCbBuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setSrcBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                goto func_exit;
            }

            if (m_parameters->getCallbackNeedCopy2Rendering() == true) {
                ret = m_previewCallbackFunc(frame);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_previewCallbackFunc fail", __FUNCTION__, __LINE__);
                    goto func_exit;
                }

                ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, scpBuffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_doCallbackToPreviewFunc fail", __FUNCTION__, __LINE__);
                    goto func_exit;
                }

                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                    }
                    previewCbBuffer.index  = -2;
                }
            } else {
                frame->incRef();
                m_previewCallbackQ->pushProcessQ(&frame);
                previewCbBuffer.index = -2;
            }

        }

        if (m_previewWindow != NULL) {
            if (timeStamp > 0L) {
                m_previewWindow->set_timestamp(m_previewWindow, (int64_t)timeStamp);
            } else {
                uint32_t fcount = 0;
                getStreamFrameCount((struct camera2_stream *)scpBuffer.addr[2], &fcount);
                CLOGW("WRN(%s[%d]): frameCount(%d)(%d), Invalid timeStamp(%lld)",
                        __FUNCTION__, __LINE__,
                        frameCount,
                        fcount,
                        timeStamp);
            }
        }

#ifdef FIRST_PREVIEW_TIME_CHECK
        if (m_flagFirstPreviewTimerOn == true) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            m_firstPreviewTimer.stop();
            m_flagFirstPreviewTimerOn = false;

            CLOGD("DEBUG(%s[%d]):m_firstPreviewTimer stop", __FUNCTION__, __LINE__);

            CLOGD("DEBUG(%s[%d]):============= First Preview time ==================", "m_printExynosCameraInfo", __LINE__);
            CLOGD("DEBUG(%s[%d]):= startPreview ~ first frame  : %d msec", "m_printExynosCameraInfo", __LINE__, (int)m_firstPreviewTimer.durationMsecs());
            CLOGD("DEBUG(%s[%d]):===================================================", "m_printExynosCameraInfo", __LINE__);
            autoFocusMgr->displayAFInfo();
        }
#endif



        /* Prevent displaying unprocessed beauty images in beauty shot. */
        if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
            ) {
            if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
                checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
                CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                        __FUNCTION__, __LINE__);
                goto func_exit;
            }
        }

        /* display the frame */
        ret = m_putBuffers(m_scpBufferMgr, scpBuffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }
    } else {
        ALOGW("WARN(%s[%d]):Preview frame buffer is canceled."
                "PreviewThread is blocked or too many buffers are in Service."
                "PreviewBufferCount(%d), ScpBufferMgr(%d), PreviewQ(%d)",
                __FUNCTION__, __LINE__,
                m_parameters->getPreviewBufferCount(),
                m_scpBufferMgr->getNumOfAvailableAndNoneBuffer(),
                previewQ->getSizeOfProcessQ());
        if (previewCbBuffer.index >= 0) {
            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
            }
        }
        ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);
    }

    if (frame != NULL) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    return loop;

func_exit:

    if (previewCbBuffer.index >= 0) {
        ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
        }
    }

    ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);

    if (frame != NULL) {
        frame->decRef();
        m_frameMgr->deleteFrame(frame);
        frame = NULL;
    }

    return loop;
}

bool ExynosCamera::m_recordingThreadFunc(void)
{
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif

    int  ret  = 0;
    int  pipeId = PIPE_MCSC2;
    nsecs_t timeStamp = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame  *frame = NULL;

    CLOGV("INFO(%s[%d]):wait gsc done output", __FUNCTION__, __LINE__);
    ret = m_recordingQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGW("WARN(%s[%d]):wait timeout", __FUNCTION__, __LINE__);
        } else {
            CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
        }
        goto func_exit;
    }

    if (m_getRecordingEnabled() == false) {
        CLOGI("INFO(%s[%d]):recording stopped", __FUNCTION__, __LINE__);
        goto func_exit;
    }

    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto func_exit;
    }
    CLOGV("INFO(%s[%d]):gsc done for recording callback", __FUNCTION__, __LINE__);

    ret = frame->getDstBuffer(pipeId, &buffer);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto func_exit;
    }

    if (buffer.index < 0
        || buffer.index >= (int)m_recordingBufferCount) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, m_recordingBufferCount, buffer.index);
        goto func_exit;
    }

    timeStamp = m_recordingTimeStamp[buffer.index];

    if (m_recordingStartTimeStamp == 0) {
        m_recordingStartTimeStamp = timeStamp;
        CLOGI("INFO(%s[%d]):m_recordingStartTimeStamp=%lld",
                __FUNCTION__, __LINE__, m_recordingStartTimeStamp);
    }

#if 0
{
    char filePath[70];
    int width, height = 0;

    m_parameters->getVideoSize(&width, &height);

    if (buffer.index == 3 && kkk < 1) {

        CLOGE("INFO(%s[%d]):getVideoSize(%d x %d)", __FUNCTION__, __LINE__, width, height);

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
#ifdef CHECK_MONOTONIC_TIMESTAMP
        CLOGD("DEBUG(%s[%d]):m_dataCbTimestamp::recordingFrameIndex=%d, recordingTimeStamp=%lld",
                __FUNCTION__, __LINE__, buffer.index, timeStamp);
#endif
#ifdef DEBUG
        CLOGD("DEBUG(%s[%d]):- lastTimeStamp(%lld), systemTime(%lld), recordingStart(%lld)",
                __FUNCTION__, __LINE__,
                m_lastRecordingTimeStamp,
                systemTime(SYSTEM_TIME_MONOTONIC),
                m_recordingStartTimeStamp);
#endif
        struct addrs *recordAddrs = NULL;

        if (m_recordingCallbackHeap != NULL) {
            recordAddrs = (struct addrs *)m_recordingCallbackHeap->data;
            recordAddrs[buffer.index].type        = kMetadataBufferTypeCameraSource;
            recordAddrs[buffer.index].fdPlaneY    = (unsigned int)buffer.fd[0];
            recordAddrs[buffer.index].fdPlaneCbcr = (unsigned int)buffer.fd[1];
            recordAddrs[buffer.index].bufIndex    = buffer.index;

            m_recordingBufAvailable[buffer.index] = false;
            m_lastRecordingTimeStamp = timeStamp;
        } else {
            CLOGD("INFO(%s[%d]):m_recordingCallbackHeap is NULL.", __FUNCTION__, __LINE__);
        }

        if (m_getRecordingEnabled() == true
            && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {
            m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    buffer.index,
                    m_callbackCookie);
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
        ret = m_removeFrameFromList(&m_recordingProcessList, frame, &m_recordingProcessListLock);
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

void ExynosCamera::m_vendorSpecificConstructor(int cameraId, __unused camera_device_t *dev)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_COMPANION
    m_companionNode = NULL;
    m_companionThread = NULL;
    CLOGI("INFO(%s):cameraId(%d), use_companion(%d)", __FUNCTION__, cameraId, m_use_companion);

    if(m_use_companion == true) {
        m_companionThread = new mainCameraThread(this, &ExynosCamera::m_companionThreadFunc,
                                                  "companionshotThread", PRIORITY_URGENT_DISPLAY);
        if (m_companionThread != NULL) {
            m_companionThread->run();
            CLOGD("DEBUG(%s): companionThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s):failed the m_companionThread.", __FUNCTION__);
        }
    }
#endif

    m_postPictureCallbackThread = new mainCameraThread(this, &ExynosCamera::m_postPictureCallbackThreadFunc, "postPictureCallbackThread");
    CLOGD("DEBUG(%s): postPictureCallbackThread created", __FUNCTION__);
    /* m_ThumbnailCallback Thread */
    m_ThumbnailCallbackThread = new mainCameraThread(this, &ExynosCamera::m_ThumbnailCallbackThreadFunc, "m_ThumbnailCallbackThread");
    CLOGD("DEBUG(%s):m_ThumbnailCallbackThread created", __FUNCTION__);

    m_yuvCallbackThread = new mainCameraThread(this, &ExynosCamera::m_yuvCallbackThreadFunc, "yuvCallbackThread");
    CLOGD("DEBUG(%s):yuvCallbackThread created", __FUNCTION__);

    m_yuvCallbackQ = new yuv_callback_queue_t;
    m_yuvCallbackQ->setWaitTime(2000000000);

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);
    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrame *frame)
{
    ExynosCameraDurationTimer       m_fdcallbackTimer;
    long long                       m_fdcallbackTimerTime;

    if (m_fdmeta_shot == NULL) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    memset(m_fdmeta_shot, 0x00, sizeof(struct camera2_shot_ext));
    if (frame->getDynamicMeta(m_fdmeta_shot) != NO_ERROR) {
        CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    frame->getUserDynamicMeta(m_fdmeta_shot);

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

        m_parameters->getHwPreviewSize(&previewW, &previewH);

        dm = &(m_fdmeta_shot->shot.dm);
        if (dm == NULL) {
            CLOGE("ERR(%s[%d]) dm data is null", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }

        CLOGV("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, dm->stats.faceDetectMode);
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

                    detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                    detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                    detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                    detectedFace[i].y2 = dm->stats.faceRectangles[i][3];

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
    if (m_parameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH
        && m_parameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH
        && m_fdmeta_shot->shot.udm.ae.vendorSpecific[395] == 1 /* Touch AE status flag - 0:searching 1:done    */
        && m_fdmeta_shot->shot.udm.ae.vendorSpecific[398] == 1 /* Touch AE enable flag - 0:disabled  1:enabled */
        ) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGD("INFO(%s[%d]): AE_RESULT(%d)", __FUNCTION__, __LINE__, m_fdmeta_shot->shot.dm.aa.aeState);
    }

    m_frameMetadata.ae_bv_changed = m_fdmeta_shot->shot.udm.ae.vendorSpecific[396];
#endif

    m_fdcallbackTimer.start();

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
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
#ifdef SAMSUNG_COMPANION
    int bufferCount = (m_parameters->getRTHdr() == COMPANION_WDR_ON) ?
                        NUM_FASTAESTABLE_BUFFER : NUM_FASTAESTABLE_BUFFER - 4;
#else
    int bufferCount = NUM_FASTAESTABLE_BUFFER;
#endif

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

    m_checkFirstFrameLux = true;
done:
    skipBufferMgr->deinit();
    delete skipBufferMgr;
    skipBufferMgr = NULL;

func_exit:
    return ret;
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

    CLOGV("DEBUG(%s[%d]):--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
        __FUNCTION__, __LINE__, previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrame  *newFrame = NULL;
    struct camera2_node_output node;

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

    /* TODO: HACK: Will be removed, this is driver's job */
    m_convertingStreamToShotExt(&previewBuf, &node);
    setMetaDmSensorTimeStamp((struct camera2_shot_ext*)previewBuf.addr[previewBuf.planeCount-1], timeStamp);

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

    ret = m_insertFrameToList(&m_recordingProcessList, newFrame, &m_recordingProcessListLock);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):[F%d]Failed to insert frame into m_recordingProcessList",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
    }

    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);
    m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);

func_exit:

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;
}

status_t ExynosCamera::m_reprocessingYuvCallbackFunc(ExynosCameraBuffer yuvBuffer)
{
    camera_memory_t *yuvCallbackHeap = NULL;

    if (m_hdrEnabled == false && m_parameters->getSeriesShotCount() <= 0) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE_NOTIFY) == true) {
            CLOGD("DEBUG(%s[%d]):RAW_IMAGE_NOTIFY callback", __FUNCTION__, __LINE__);

            m_notifyCb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0, 0, m_callbackCookie);
        }

        if (yuvBuffer.index < 0) {
            CLOGE("ERR(%s[%d]):Invalid YUV buffer index %d. Skip to callback%s%s",
                    __FUNCTION__, __LINE__, yuvBuffer.index,
                    (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE))? " RAW" : "",
                    (m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))? " POSTVIEW" : "");

            return BAD_VALUE;
        }

        if (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
            CLOGD("DEBUG(%s[%d]):RAW callback", __FUNCTION__, __LINE__);

            yuvCallbackHeap = m_getMemoryCb(yuvBuffer.fd[0], yuvBuffer.size[0], 1, m_callbackCookie);
            if (!yuvCallbackHeap || yuvCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):Failed to getMemoryCb(%d)",
                        __FUNCTION__, __LINE__, yuvBuffer.size[0]);

                return INVALID_OPERATION;
            }

            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, yuvCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            yuvCallbackHeap->release(yuvCallbackHeap);
        }

        if ((m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME))
                && !m_parameters->getIsThumbnailCallbackOn()) {
            CLOGD("DEBUG(%s[%d]):POSTVIEW callback", __FUNCTION__, __LINE__);

            yuvCallbackHeap = m_getMemoryCb(yuvBuffer.fd[0], yuvBuffer.size[0], 1, m_callbackCookie);
            if (!yuvCallbackHeap || yuvCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):Failed to getMemoryCb(%d)",
                        __FUNCTION__, __LINE__, yuvBuffer.size[0]);

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
    CLOGD("DEBUG(%s[%d]):videoSize = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    m_doCscRecording = true;
    if (m_parameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("INFO(%s[%d]):do Recording CSC !!! m_recordingBufferCount(%d)", __FUNCTION__, __LINE__, m_recordingBufferCount);
    } else {
        m_doCscRecording = false;
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        CLOGI("INFO(%s[%d]):skip Recording CSC !!! m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_exynosconfig->current->bufInfo.num_recording_buffers, m_recordingBufferCount);
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
    }
    m_recordingListLock.unlock();
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) OUT",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());

    for (int32_t i = 0; i < m_recordingBufferCount; i++) {
        m_recordingTimeStamp[i] = 0L;
        m_recordingBufAvailable[i] = true;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct addrs), m_recordingBufferCount, &heapFd);
    if (!m_recordingCallbackHeap || m_recordingCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%zu) fail", __FUNCTION__, __LINE__, sizeof(struct addrs));
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (m_doCscRecording == true && m_recordingBufferMgr->isAllocated() == false) {
        /* alloc recording Image buffer */
        planeSize[0] = ROUND_UP(videoW, MFC_ALIGN) * ROUND_UP(videoH, MFC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(videoW, MFC_ALIGN) * ROUND_UP(videoH / 2, MFC_ALIGN) + MFC_7X_BUFFER_OFFSET;
        if( m_parameters->getHighSpeedRecording() == true)
            minBufferCount = m_recordingBufferCount;
        else
            minBufferCount = 1;

        maxBufferCount = m_recordingBufferCount;

        ret = m_allocBuffers(m_recordingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, true);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_recordingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        }

    }

    if (m_recordingQ->getSizeOfProcessQ() > 0) {
        CLOGE("ERR(%s[%d]):m_startRecordingInternal recordingQ(%d)", __FUNCTION__, __LINE__, m_recordingQ->getSizeOfProcessQ());
        m_clearList(m_recordingQ);
    }

    m_recordingThread->run();

func_exit:

    return ret;
}

status_t ExynosCamera::m_stopRecordingInternal(void)
{
    int ret = 0;

    m_recordingQ->sendCmd(WAKE_UP);
    m_recordingThread->requestExitAndWait();
    m_recordingQ->release();
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
        return ret;
    }
    m_recordingListLock.unlock();

    /* Checking all frame(buffer) released from Media recorder */
    int sleepUsecs = 33300;
    int retryCount = 3;     /* 33.3ms*3 = 100ms */
    bool allBufferReleased = true;

    for (int i = 0; i < retryCount; i++) {
        allBufferReleased = true;

        for (int bufferIndex = 0; bufferIndex < m_recordingBufferCount; bufferIndex++) {
            if (m_recordingBufAvailable[bufferIndex] == false) {
                CLOGW("WRN(%s[%d]):Media recorder doesn't release frame(buffer), index(%d)",
                        __FUNCTION__, __LINE__, bufferIndex);
                allBufferReleased = false;
            }
        }

        if(allBufferReleased == true) {
            break;
        }

        usleep(sleepUsecs);
    }

    if (allBufferReleased == false) {
        CLOGE("ERR(%s[%d]):Media recorder doesn't release frame(buffer) all!!", __FUNCTION__, __LINE__);
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

    return NO_ERROR;
}

bool ExynosCamera::m_shutterCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    int loop = false;


    if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);

        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);

        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
    }

    /* one shot */
    return loop;
}

bool ExynosCamera::m_releasebuffersForRealloc()
{
    status_t ret = NO_ERROR;
    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    CLOGD("DEBUG(%s[%d]):m_setBuffers free all buffers", __FUNCTION__, __LINE__);
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }
#ifdef DEBUG_RAWDUMP
    if (m_parameters->getUsePureBayerReprocessing() == false) {
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

    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }

    if (m_postPictureBufferMgr != NULL) {
        m_postPictureBufferMgr->deinit();
    }

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

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
            CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
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
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AP_REPROCESSING);
    int pictureFormat = m_parameters->getHwPictureFormat();
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&maxPictureW, &maxPictureH);
        CLOGI("(%s):HW Picture(HighSpeed) width x height = %dx%d", __FUNCTION__, maxPictureW, maxPictureH);
    } else {
        m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
        CLOGI("(%s):HW Picture width x height = %dx%d", __FUNCTION__, maxPictureW, maxPictureH);
    }
    m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);

    /* Reprocessing 3AA to ISP Buffer */
    if (m_parameters->getUsePureBayerReprocessing() == true
        && m_parameters->isReprocessing3aaIspOTF() == false) {
        bayerFormat     = m_parameters->getBayerFormat(PIPE_3AP_REPROCESSING);
        bytesPerLine[0] = getBayerLineSize(maxPictureW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(maxPictureW, maxPictureH, bayerFormat);
        planeCount      = 2;

        /* ISP Reprocessing Buffer realloc for high resolution callback */
        if (m_parameters->getHighResolutionCallbackMode() == true) {
            minBufferCount = 2;
            maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
        } else {
            minBufferCount = 1;
            maxBufferCount = m_exynosconfig->current->bufInfo.num_reprocessing_buffers;
        }
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_ispReprocessingBufferMgr m_allocBuffers(minBufferCount=%d/maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
            return ret;
        }

        memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);

        CLOGI("INFO(%s[%d]):m_allocBuffers(ISP Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, maxPictureW, maxPictureH, planeCount, maxBufferCount);
    }

    if (m_parameters->isUseHWFC() == true
        && m_parameters->getHighResolutionCallbackMode() == false)
        allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_parameters->getPictureSize(&maxPictureW, &maxPictureH);
        CLOGI("(%s):HW Picture(HighResolutionCB) width x height = %dx%d", __FUNCTION__, maxPictureW, maxPictureH);
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
    /* SCC Reprocessing Buffer realloc for high resolution callback */
    if (m_parameters->getHighResolutionCallbackMode() == true)
        minBufferCount = 2;
    else
        minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

    ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_sccReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(YUV Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, maxPictureW, maxPictureH, planeCount, maxBufferCount);

    /* Reprocessing Thumbnail Buffer */
    if (pictureFormat == V4L2_PIX_FMT_NV21M) {
        planeCount      = 3;
        planeSize[0]    = maxThumbnailW * maxThumbnailH;
        planeSize[1]    = maxThumbnailW * maxThumbnailH / 2;
    } else {
        planeCount      = 2;
        planeSize[0]    = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), maxThumbnailW, maxThumbnailH);
    }
    minBufferCount = 1;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(m_thumbnailBufferMgr, planeCount, planeSize, bytesPerLine, minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_thumbnailBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(Thumb Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, maxThumbnailW, maxThumbnailH, planeCount, maxBufferCount);

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
    int maxBufferCount = 1;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    bool needMmap = false;

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getHwVideoSize(&hwVideoW, &hwVideoH);

    CLOGI("INFO(%s[%d]):HW Preview width x height = %dx%d",
            __FUNCTION__, __LINE__, hwPreviewW, hwPreviewH);
    CLOGI("INFO(%s[%d]):HW Video width x height = %dx%d",
            __FUNCTION__, __LINE__, hwVideoW, hwVideoH);

    if (m_parameters->getHighSpeedRecording() == true) {
        m_parameters->getHwSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("INFO(%s[%d]):HW Sensor(HighSpeed) MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH);
    } else {
        m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        CLOGI("INFO(%s[%d]):Sensor MAX width x height = %dx%d",
                __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH);
    }

    /* Bayer(FLITE, 3AC) Buffer */
    if (m_parameters->getUsePureBayerReprocessing() == true)
        bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);
    else
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);

    bytesPerLine[0] = getBayerLineSize(sensorMaxW, bayerFormat);
    planeSize[0]    = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat);
    planeCount      = 2;

    if ((m_parameters->getDualMode() == true && (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1))
        || m_parameters->getUsePureBayerReprocessing() == true)
        maxBufferCount = m_exynosconfig->current->bufInfo.num_sensor_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_BACK) {
#if defined(CAMERA_ADD_BAYER_ENABLE) || defined(SAMSUNG_DNG)
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
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    }

    ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, needMmap);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(Bayer Buffer) planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            __FUNCTION__, __LINE__, planeSize[0], planeCount, maxBufferCount);

#ifdef DEBUG_RAWDUMP
    if (m_parameters->getUsePureBayerReprocessing() == false) {
        /* Bayer Dump Buffer */
        bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);

        bytesPerLine[0] = getBayerLineSize(sensorMaxW, bayerFormat);
        planeSize[0]    = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat);
        planeCount      = 2;

        maxBufferCount = m_exynosconfig->current->bufInfo.num_sensor_buffers;

        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, bytesPerLine, \
                               maxBufferCount, maxBufferCount, type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGI("INFO(%s[%d]):m_allocBuffers(Flite Buffer) planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, planeSize[0], planeCount, maxBufferCount);
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
    int minBufferCount;
    int maxBufferCount;
    exynos_camera_buffer_type_t type;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
    CLOGI("(%s):Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
    m_parameters->getMaxThumbnailSize(&sensorMaxThumbW, &sensorMaxThumbH);
    CLOGI("(%s):Sensor MAX Thumbnail width x height = %dx%d", __FUNCTION__, sensorMaxThumbW, sensorMaxThumbH);

    /* Allocate post picture buffers */
    int f_pictureW = 0, f_pictureH = 0;
    m_parameters->getPictureSize(&f_pictureW, &f_pictureH);
    planeSize[0] = f_pictureW * f_pictureH * 1.5;
    planeCount = 2;
    minBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    ret = m_allocBuffers(m_postPictureBufferMgr, planeCount, planeSize, bytesPerLine,
                            minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_postPictureBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(m_postPictureBuffer) %d x %d, planeCount(%d), planeSize(%d)",
            __FUNCTION__, __LINE__, f_pictureW, f_pictureH, planeCount, planeSize[0]);

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseVendorBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    CLOGI("INFO(%s[%d]):free buffer done", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::releaseRecordingFrame(const void *opaque)
{
    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return;
        }
    }

    if (m_getRecordingEnabled() == false) {
        CLOGW("WARN(%s[%d]):m_recordingEnabled equals false", __FUNCTION__, __LINE__);
        /* m_stopRecordingInternal() will wait for recording frame release */
        /* return; */
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
            if (m_parameters != NULL) {
                m_parameters->setIsFirstStartFlag(m_isFirstStart);
            }
        }
    }

    if (found == false)
        CLOGW("WARN(%s[%d]):**** releaseFrame not founded ****", __FUNCTION__, __LINE__);
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

    if (m_parameters->is3aaIspOtf() == true) {
        pipeIdErrorCheck = PIPE_3AA;
    } else {
        pipeIdErrorCheck = PIPE_ISP;
    }

#ifdef MONITOR_LOG_SYNC
    /* define pipe for isp node cause of sync log sctrl */
    uint32_t pipeIdLogSync = PIPE_3AA;

    /* If it is not front camera in dual and sensor pipe is running, do sync log */
    if (m_previewFrameFactory->checkPipeThreadRunning(pipeIdLogSync) &&
        !((getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1) && m_parameters->getDualMode())){
        if (!(m_syncLogDuration % MONITOR_LOG_SYNC_INTERVAL)) {
            uint32_t syncLogId = m_getSyncLogId();
            CLOGI("INFO(%s[%d]): @FIMC_IS_SYNC %d", __FUNCTION__, __LINE__, syncLogId);
            m_previewFrameFactory->syncLog(pipeIdLogSync, syncLogId);
        }
        m_syncLogDuration++;
    }
#endif

    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
        m_dump();

        /* in GED */
        m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);

        return false;
    }

#ifdef SENSOR_OVERFLOW_CHECK
    m_previewFrameFactory->getControl(V4L2_CID_IS_G_DTPSTATUS, &dtpStatus, pipeIdFlite);
    if (dtpStatus == 1) {
        CLOGD("DEBUG(%s[%d]):DTP Detected. dtpStatus(%d)", __FUNCTION__, __LINE__, dtpStatus);
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

    if (m_parameters->getSamsungCamera() && ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT))) {
        CLOGD("DEBUG(%s[%d]):ESD Detected. threadState(%d) *countRenew(%d)", __FUNCTION__, __LINE__, *threadState, *countRenew);
        m_dump();

#ifdef CAMERA_GED_FEATURE
        /* in GED */
        /* skip error callback */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
#else
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 1001, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif
        return false;
    } else {
        CLOGV("[%s] (%d) (%d)", __FUNCTION__, __LINE__, *threadState);
    }

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

status_t ExynosCamera::cancelPicture()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters == NULL) {
        CLOGE("ERR(%s[%d]):m_parameters is NULL", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (m_parameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
        /* android_printAssert(NULL, LOG_TAG, "Cannot support this operation"); */

        return NO_ERROR;
    }

    if (m_parameters->getLongExposureShotCount() > 0) {
        CLOGD("DEBUG(%s[%d]):emergency stop for manual exposure shot", __FUNCTION__, __LINE__);
        m_cancelPicture = true;
        m_parameters->setPreviewExposureTime();
        m_pictureEnabled = false;
        m_stopLongExposure = true;
        m_reprocessingCounter.clearCount();
        m_terminatePictureThreads(false);
        m_cancelPicture = false;
    }

/*
    m_takePictureCounter.clearCount();
    m_reprocessingCounter.clearCount();
    m_pictureCounter.clearCount();
    m_jpegCounter.clearCount();
*/

    return NO_ERROR;
}

bool ExynosCamera::m_autoFocusResetNotify(__unused int focusMode)
{
    return true;
}

bool ExynosCamera::m_autoFocusThreadFunc(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    bool afResult = false;
    int focusMode = 0;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    /* block until we're told to start.  we don't want to use
     * a restartable thread and requestExitAndWait() in cancelAutoFocus()
     * because it would cause deadlock between our callbacks and the
     * caller of cancelAutoFocus() which both want to grab the same lock
     * in CameraServices layer.
     */

    if (getCameraId() == CAMERA_ID_FRONT) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {
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
    } else if (getCameraId() == CAMERA_ID_BACK_1) {
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
                    CLOGD("DEBUG(%s):Do not support autoFocus in front camera.", __FUNCTION__);
                    m_notifyCb(CAMERA_MSG_FOCUS, true, 0, m_callbackCookie);
                } else {
                    CLOGD("DEBUG(%s):m_notifyCb is NULL!", __FUNCTION__);
                }
            } else {
                CLOGD("DEBUG(%s):autoFocus msg disabled !!", __FUNCTION__);
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
        CLOGD("DEBUG(%s):exiting on request", __FUNCTION__);
        goto done;
    }

    m_autoFocusRunning = true;

    if (m_autoFocusRunning == true) {
        afResult = m_exynosCameraActivityControl->autoFocus(focusMode,
                                                            m_autoFocusType,
                                                            m_flagStartFaceDetection,
                                                            m_frameMetadata.number_of_faces);
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
        m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)) {

        if (m_notifyCb != NULL) {
            int afFinalResult = (int)afResult;

            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __FUNCTION__, afFinalResult, focusMode);
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
            m_autoFocusLock.lock();
        } else {
            CLOGD("DEBUG(%s):m_notifyCb is NULL mode(%d)", __FUNCTION__, focusMode);
        }
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled, no callback !!", __FUNCTION__);
    }

    autoFocusMgr->displayAFStatus();

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
        bufMgrList[0] = NULL;
#ifdef DEBUG_RAWDUMP
        bufMgrList[1] = (m_parameters->getUsePureBayerReprocessing() == true) ? &m_bayerBufferMgr : &m_fliteBufferMgr;
#else
        bufMgrList[1] = &m_bayerBufferMgr;
#endif
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
        if ((m_parameters->getDualMode() == true) &&
            (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
            bufMgrList[0] = &m_bayerBufferMgr;
            bufMgrList[1] = &m_sccBufferMgr;
        } else {
            bufMgrList[0] = &m_3aaBufferMgr;
            bufMgrList[1] = &m_ispBufferMgr;
        }
        break;
    case PIPE_ISP:
        bufMgrList[0] = &m_ispBufferMgr;

        if (m_parameters->getTpuEnabledMode() == true) {
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
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_MCSC1:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_previewCallbackBufferMgr;
        break;
    case PIPE_GSC:
        if ((m_parameters->getDualMode() == true) &&
            (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
            bufMgrList[0] = &m_sccBufferMgr;
        } else {
            bufMgrList[0] = &m_scpBufferMgr;
        }
        bufMgrList[1] = &m_scpBufferMgr;
        break;
    case PIPE_GSC_PICTURE:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_3AA_REPROCESSING:
        bufMgrList[0] = &m_bayerBufferMgr;
        if (m_parameters->isReprocessing3aaIspOTF() == false)
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
    case PIPE_MCSC3_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING:
        bufMgrList[0] = &m_sccReprocessingBufferMgr;
        bufMgrList[1] = &m_gscBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING2:
    case PIPE_MCSC1_REPROCESSING:
        bufMgrList[0] = &m_sccReprocessingBufferMgr;
        bufMgrList[1] = &m_postPictureBufferMgr;
        break;
    case PIPE_GSC_REPROCESSING3:
        bufMgrList[0] = &m_postPictureBufferMgr;
        bufMgrList[1] = &m_thumbnailGscBufferMgr;
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

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, camera2_shot_ext *updateDmShot)
{
    status_t ret = NO_ERROR;
    bool isSrc = false;
    int funcRetryCount = 0;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream *shot_stream = NULL;
    ExynosCameraFrame *inListFrame = NULL;
    ExynosCameraFrame *bayerFrame = NULL;
    ExynosCameraBuffer *saveBuffer = NULL;
    ExynosCameraBuffer tempBuffer;
    ExynosRect srcRect, dstRect;
    entity_buffer_state_t buffer_state = ENTITY_BUFFER_STATE_ERROR;

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

BAYER_RETRY:
    if (m_stopLongExposure == true
        && m_parameters->getCaptureExposureTime() != 0) {
        CLOGD("DEBUG(%s[%d]):Emergency stop", __FUNCTION__, __LINE__);
        m_reprocessingCounter.clearCount();
        m_pictureEnabled = false;
        goto CLEAN;
    }

    m_captureSelector->setWaitTime(200000000);
    bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    if (pipeId == PIPE_3AA) {
        ret = bayerFrame->getDstBuffer(pipeId, buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
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

    if (updateDmShot == NULL) {
        CLOGE("ERR(%s[%d]):updateDmShot is NULL", __FUNCTION__, __LINE__);
        ret = BAD_VALUE;
        goto CLEAN;
    }

    retryCount = 12; /* 30ms * 12 */
    while (retryCount > 0) {
        if (bayerFrame->getMetaDataEnable() == false)
            CLOGW("WRN(%s[%d]):Waiting for update jpeg metadata, retryCount(%d)", __FUNCTION__, __LINE__, retryCount);
        else
            break;

        retryCount--;
        usleep(DM_WAITING_TIME);
    }

    ret = bayerFrame->getSrcBufferState(PIPE_3AA, &buffer_state);
    if (ret != NO_ERROR)
        CLOGE("ERR(%s[%d]):get 3AA Src buffer state fail, ret(%d)", __FUNCTION__, __LINE__, ret);

    if (buffer_state == ENTITY_BUFFER_STATE_ERROR
        || bayerFrame->getMetaDataEnable() == false) {
        /* Return buffer & delete frame */
        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]):Selected frame(%d) is not available! buffer_state(%d)",
                        __FUNCTION__, __LINE__, bayerFrame->getFrameCount(), buffer_state);
                bayerFrame->decRef();
                m_frameMgr->deleteFrame(bayerFrame);
                bayerFrame = NULL;
            }
        }
        goto BAYER_RETRY;
    }

    /* update metadata */
    bayerFrame->getUserDynamicMeta(updateDmShot);
    bayerFrame->getDynamicMeta(updateDmShot);
    shot_ext = updateDmShot;

    CLOGD("DEBUG(%s[%d]):Selected frame count(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
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

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]): Selected frame(%d, %d msec) is not same with set from user.",
                        __FUNCTION__, __LINE__, bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
                bayerFrame->decRef();
                m_frameMgr->deleteFrame(bayerFrame);
                bayerFrame = NULL;
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
                ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame, &m_processListLock);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
                } else {
                    bayerFrame->decRef();
                    m_frameMgr->deleteFrame(bayerFrame);
                    bayerFrame = NULL;
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
            CLOGE("ERR(%s[%d]):addBayerBufferPacked() fail", __FUNCTION__, __LINE__);
        }

        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

        if (bayerFrame != NULL) {
            bayerFrame->frameUnlock();
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame, &m_processListLock);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            } else {
                CLOGD("DEBUG(%s[%d]):Selected frame(%d, %d msec) delete.",
                        __FUNCTION__, __LINE__, bayerFrame->getFrameCount(),
                        (int) shot_ext->shot.dm.sensor.exposureTime);
                bayerFrame->decRef();
                m_frameMgr->deleteFrame(bayerFrame);
                bayerFrame = NULL;
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
    }

    if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
        if (m_pictureThread->isRunning() == false) {
            ret = m_pictureThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;

            }
        }

        if (m_jpegCallbackThread->isRunning() == false) {
            ret = m_jpegCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run jpeg callback thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
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

        int funcRet = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame, &m_processListLock);
        if (funcRet < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
            ret = INVALID_OPERATION;
        } else {
            CLOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete", __FUNCTION__, __LINE__, bayerFrame->getFrameCount());
            bayerFrame->decRef();
            m_frameMgr->deleteFrame(bayerFrame);
            bayerFrame = NULL;
        }
    }

    return ret;
}

/* vision */
status_t ExynosCamera::m_startVisionInternal(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_stopVisionInternal(void)
{
    int ret = 0;

    CLOGI("DEBUG(%s[%d]):IN", __FUNCTION__, __LINE__);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame)
{
    Mutex::Autolock lock(m_frameLock);

    int ret = 0;

    return ret;
}

status_t ExynosCamera::m_setVisionBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("INFO(%s[%d]):alloc buffer - camera ID: %d",
        __FUNCTION__, __LINE__, m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVisionCallbackBuffer(void)
{
    return NO_ERROR;
}


bool ExynosCamera::m_visionThreadFunc(void)
{

    return false;
}

status_t ExynosCamera::m_startCompanion(void)
{
#if defined(SAMSUNG_COMPANION)
    if(m_use_companion == true) {
        if (m_companionNode == NULL) {
            m_companionThread = new mainCameraThread(this, &ExynosCamera::m_companionThreadFunc,
                                                      "companionshotThread", PRIORITY_URGENT_DISPLAY);
            if (m_companionThread != NULL) {
                m_companionThread->run();
                CLOGD("DEBUG(%s): companionThread starts", __FUNCTION__);
            } else {
                CLOGE("(%s):failed the m_companionThread.", __FUNCTION__);
            }
        } else {
            CLOGW("WRN(%s[%d]):m_companionNode != NULL. so, it already running", __FUNCTION__, __LINE__);
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_stopCompanion(void)
{
#if defined(SAMSUNG_COMPANION)
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_companionThread", __FUNCTION__, __LINE__);
            m_companionThread->requestExitAndWait();
            CLOGI("INFO(%s[%d]): wait m_companionThread end", __FUNCTION__, __LINE__);
        } else {
            CLOGI("INFO(%s[%d]): m_companionThread is NULL", __FUNCTION__, __LINE__);
        }

        if (m_companionNode != NULL) {
            ExynosCameraDurationTimer timer;

            timer.start();

            if (m_companionNode->close() != NO_ERROR) {
                CLOGE("ERR(%s):close fail", __FUNCTION__);
            }
            delete m_companionNode;
            m_companionNode = NULL;

            CLOGD("DEBUG(%s):Companion Node(%d) closed", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

            timer.stop();
            CLOGD("DEBUG(%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());

        }
    }
#endif

    return NO_ERROR;
}

#if defined(SAMSUNG_COMPANION)
int ExynosCamera::m_getSensorId(int m_cameraId)
{
    unsigned int scenario = 0;
    unsigned int scenarioBit = 0;
    unsigned int nodeNumBit = 0;
    unsigned int sensorIdBit = 0;
    unsigned int sensorId = getSensorId(m_cameraId);

    scenarioBit = (scenario << SCENARIO_SHIFT);

    nodeNumBit = ((FIMC_IS_VIDEO_SS0_NUM - FIMC_IS_VIDEO_SS0_NUM) << SSX_VINDEX_SHIFT);

    sensorIdBit = (sensorId << 0);

    return (scenarioBit) | (nodeNumBit) | (sensorIdBit);
}

bool ExynosCamera::m_companionThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int loop = false;
    int ret = 0;

    m_timer.start();

    m_companionNode = new ExynosCameraNode();

    ret = m_companionNode->create("companion", m_cameraId);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    ret = m_companionNode->open(MAIN_CAMERA_COMPANION_NUM);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node open fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    CLOGD("DEBUG(%s):Companion Node(%d) opened running)", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

    ret = m_companionNode->setInput(m_getSensorId(m_cameraId));
    if (ret < 0) {
        CLOGE("ERR(%s[%d]): Companion Node s_input fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    CLOGD("DEBUG(%s):Companion Node(%d) s_input)", __FUNCTION__, MAIN_CAMERA_COMPANION_NUM);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    /* one shot */
    return loop;
}
#endif

status_t ExynosCamera::m_waitCompanionThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    timer.start();

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        if (m_companionThread != NULL) {
            m_companionThread->join();
        } else {
            CLOGI("INFO(%s[%d]):m_companionThread is NULL", __FUNCTION__, __LINE__);
        }
    }
#endif

    timer.stop();
    CLOGD("DEBUG(%s[%d]):companion waiting time : duration time(%5d msec)", __FUNCTION__, __LINE__, (int)timer.durationMsecs());

    CLOGD("DEBUG(%s[%d]):companionThread join", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

void ExynosCamera::m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext) {
    return;
}

status_t ExynosCamera::m_copyMetaFrameToFrame(ExynosCameraFrame *srcframe, ExynosCameraFrame *dstframe, bool useDm, bool useUdm)
{
    Mutex::Autolock lock(m_metaCopyLock);
    status_t ret = NO_ERROR;

    memset(m_tempshot, 0x00, sizeof(struct camera2_shot_ext));
    if(useDm) {
        ret = srcframe->getDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):getDynamicMeta fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        ret = dstframe->storeDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):storeDynamicMeta fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    if(useUdm) {
        ret = srcframe->getUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):getUserDynamicMeta fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        ret = dstframe->storeUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):storeUserDynamicMeta fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    return NO_ERROR;
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
    ExynosCameraBufferManager *scpBufferMgr = NULL;
    int32_t reprocessingBayerMode = m_parameters->updateReprocessingBayerMode();
    enum pipeline pipe;
    bool zoomWithScaler = false;
    int pipeId = PIPE_FLITE;

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_displayPreviewToggle = 0;

    zoomWithScaler = m_parameters->getSupportedZoomPreviewWIthScaler();

    if (m_parameters->isFlite3aaOtf() == true)
        minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;
    else
        minBayerFrameNum = m_exynosconfig->current->bufInfo.num_sensor_buffers;

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
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
    }

    m_parameters->setZoomPreviewWIthScaler(zoomWithScaler);

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    switch (reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_NONE", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
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

    if ((m_parameters->getDualMode() == true) &&
        (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
        m_previewFrameFactory->setRequestFLITE(true);
    }

    if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false) {
        if (m_parameters->getTpuEnabledMode() == true) {
            m_previewFrameFactory->setRequestISPC(false);
            m_previewFrameFactory->setRequestISPP(true);
            m_previewFrameFactory->setRequestDIS(true);
        } else {
            m_previewFrameFactory->setRequestISPC(true);
            m_previewFrameFactory->setRequestISPP(false);
            m_previewFrameFactory->setRequestDIS(false);
        }
    }

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) == true
        && m_parameters->getHighResolutionCallbackMode() == false) {
        m_previewFrameFactory->setRequest(PIPE_MCSC1, true);

        ret = m_setPreviewCallbackBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
            return ret;
        }
    } else {
        m_previewFrameFactory->setRequest(PIPE_MCSC1, false);
    }

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        scpBufferMgr = m_zoomScalerBufferMgr;
    } else {
        scpBufferMgr = m_scpBufferMgr;
    }

    tempBufferManager = taaBufferManager;

    if ((m_parameters->getDualMode() == true) &&
        (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_bayerBufferMgr;
    } else {
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_3AA)] = m_3aaBufferMgr;
    }

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
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = m_scpBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC1)] = m_previewCallbackBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC2)] = m_recordingBufferMgr;

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

    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (mcscBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(mcscBufferManager, PIPE_MCSC);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(disBufferManager, %d) failed",
                    __FUNCTION__, __LINE__, PIPE_DIS);
                return ret;
            }
            break;
        }
    }

    ret = m_previewFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    ret = m_previewFrameFactory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_previewFrameFactory->mapBuffers() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    m_printExynosCameraInfo(__FUNCTION__);

    pipeId = PIPE_FLITE;

    if (m_parameters->isFlite3aaOtf() == false
        || reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON)
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

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

    uint32_t loopFrameNum = minBayerFrameNum > min3AAFrameNum ? minBayerFrameNum : min3AAFrameNum;
    for (uint32_t i = 0; i < loopFrameNum; i++) {
        ret = m_generateFrame(i, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):new faame is NULL", __FUNCTION__, __LINE__);
            return ret;
        }

        if (m_parameters->isFlite3aaOtf() == false
            || reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON) {
            if (i < minBayerFrameNum) {
                ret = m_setupEntity(PIPE_FLITE, newFrame);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, PIPE_FLITE, ret);

                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
                m_fliteFrameCount++;
            }
        }

        if (m_parameters->isFlite3aaOtf() == true) {
            if (i < min3AAFrameNum) {
                ret = m_setupEntity(PIPE_3AA, newFrame);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, PIPE_FLITE, ret);

                m_previewFrameFactory->pushFrameToPipe(&newFrame, PIPE_3AA);
                m_3aa_ispFrameCount++;
            }
        }

#if 0
        /* SCC */
        if(m_parameters->isSccCapture() == true) {
            m_sccFrameCount++;

            if (m_parameters->isOwnScc(getCameraId()) == true) {
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
        ret = m_generateFrame(i, &newFrame);
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
    if (m_parameters->isReprocessing() == true) {
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
    m_parameters->setPreviewRunning(m_previewEnabled);

    if (m_parameters->getFocusModeSetting() == true) {
        CLOGD("set Focus Mode(%s[%d])", __FUNCTION__, __LINE__);
        int focusmode = m_parameters->getFocusMode();
        m_exynosCameraActivityControl->setAutoFocusMode(focusmode);
        m_parameters->setFocusModeSetting(false);
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

    /* MCPipe send frame to mainSetupQ directly, must use it */
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        m_mainSetupQ[i]->release();
    }

    /* clear previous preview frame */
    CLOGD("DEBUG(%s[%d]):clear process Frame list", __FUNCTION__, __LINE__);
    ret = m_clearList(&m_processList, &m_processListLock);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList, &m_recordingProcessListLock);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
    }
    m_recordingListLock.unlock();
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) OUT",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());

    m_pipeFrameDoneQ->release();

    m_fliteFrameCount = 0;
    m_3aa_ispFrameCount = 0;
    m_ispFrameCount = 0;
    m_sccFrameCount = 0;
    m_scpFrameCount = 0;
    m_isZSLCaptureOn = false;

    m_previewEnabled = false;
    m_parameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_restartPreviewInternal(bool flagUpdateParam, CameraParameters *params)
{
    CLOGI("INFO(%s[%d]): Internal restart preview", __FUNCTION__, __LINE__);
    int ret = 0;
    int err = 0;
    int reservedMemoryCount = 0;

    m_flagThreadStop = true;
    m_disablePreviewCB = true;

    m_startPictureInternalThread->join();

    m_startPictureBufferThread->join();

    if (m_previewFrameFactory != NULL)
        m_previewFrameFactory->setStopFlag();

    m_mainThread->requestExitAndWait();

    ret = m_stopPictureInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopPictureInternal fail", __FUNCTION__, __LINE__);

    m_zoomPreviwWithCscThread->stop();
    m_zoomPreviwWithCscQ->sendCmd(WAKE_UP);
    m_zoomPreviwWithCscThread->requestExitAndWait();

    m_previewThread->stop();
    if(m_previewQ != NULL)
        m_previewQ->sendCmd(WAKE_UP);
    m_previewThread->requestExitAndWait();

    if (m_previewCallbackThread->isRunning()) {
        m_previewCallbackThread->stop();
        if (m_previewCallbackQ != NULL) {
            m_previewCallbackQ->sendCmd(WAKE_UP);
        }
        m_previewCallbackThread->requestExitAndWait();
    }

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
        CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    if (m_previewQ != NULL) {
        m_clearList(m_previewQ);
    }

    if (m_previewCallbackQ != NULL) {
        m_clearList(m_previewCallbackQ);
    }

    /* check reserved memory count */
    if (m_bayerBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    if (m_ispBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    if (m_jpegBufferMgr->getContigBufCount() > 0)
        reservedMemoryCount++;

    /* skip to free and reallocate buffers */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
    if (reservedMemoryCount > 0) {
        m_bayerBufferMgr->deinit();
    }

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
    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->resetBuffers();
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->resetBuffers();
    }

    /* skip to free and reallocate buffers */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }

    if (m_postPictureBufferMgr != NULL) {
        m_postPictureBufferMgr->resetBuffers();
    }

    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->resetBuffers();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers();
    }
    if (reservedMemoryCount > 0) {
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

    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

    if( m_parameters->getHighSpeedRecording() && m_parameters->getReallocBuffer() ) {
        CLOGD("DEBUG(%s): realloc buffer all buffer deinit ", __FUNCTION__);
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
#ifdef DEBUG_RAWDUMP
        if (m_parameters->getUsePureBayerReprocessing() == false) {
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

    m_flagThreadStop = false;

    if (flagUpdateParam == true && params != NULL)
        m_parameters->setParameters(*params);

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
    m_setBuffersThread->run(PRIORITY_DEFAULT);
#if !defined(USE_SNAPSHOT_ON_UHD_RECORDING)
    if (m_parameters->getEffectRecordingHint() == false
        && m_parameters->getDualRecordingHint() == false
        && m_parameters->getUHDRecordingMode() == false)
#endif
    {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);
    }

    m_setBuffersThread->join();

    if (m_isSuccessedBufferAllocation == false) {
        CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
        err = INVALID_OPERATION;
    }

#ifdef START_PICTURE_THREAD
    m_startPictureInternalThread->join();
#endif
    m_startPictureBufferThread->join();

    if (m_parameters->isReprocessing() == true) {
#ifdef START_PICTURE_THREAD
        m_startPictureInternalThread->run(PRIORITY_DEFAULT);
#endif
    } else {
        m_pictureFrameFactory = m_previewFrameFactory;
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
    }

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_startPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* setup frame thread */
    if ((m_parameters->getDualMode() == true) &&
        (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_BACK_1)) {
        CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
        m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
    } else {
        /* Comment out : SetupQThread[FLITE] is not used, but when we reduce FLITE buffer, it is useful */
        /*
        switch (m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(NULL);
            CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            CLOGD("DEBUG(%s[%d]):setupThread with List pipeId(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
            break;
        default:
            CLOGI("INFO(%s[%d]):setupThread not started pipeID(%d)", __FUNCTION__, __LINE__, PIPE_FLITE);
            break;
        }
        */
        CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_3AA);
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

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        CLOGD("DEBUG(%s[%d]):ZoomPreview with Scaler Thread start", __FUNCTION__, __LINE__);
        m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
    }

    /* On High-resolution callback scenario, the reprocessing works with specific fps.
       To avoid the thread creation performance issue, threads in reprocessing pipes
       must be run with run&wait mode */
    bool needOneShotMode = (m_parameters->getHighResolutionCallbackMode() == false);
    m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_3AA_REPROCESSING, needOneShotMode);

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

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
    }

    if (m_zslPictureEnabled == true) {
        CLOGD("DEBUG(%s[%d]): zsl picture is already initialized", __FUNCTION__, __LINE__);
        return NO_ERROR;
    }

    if (m_parameters->isReprocessing() == true) {
        m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);
        if (thumbnailW > 0 && thumbnailH > 0) {
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
        } else {
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
        }

        ret = m_setReprocessingBuffer();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_setReprocessing Buffer fail", __FUNCTION__, __LINE__);
            return ret;
        }

        if (m_reprocessingFrameFactory->isCreated() == false) {
            ret = m_reprocessingFrameFactory->create();
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->create() failed", __FUNCTION__, __LINE__);
                return ret;
            }

            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }

        int flipHorizontal = m_parameters->getFlipHorizontal();

        CLOGD("DEBUG(%s [%d]):set FlipHorizontal on capture, FlipHorizontal(%d)", __FUNCTION__, __LINE__, flipHorizontal);

        enum NODE_TYPE nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);
        
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
        tempBufferManager = taaBufferManager;

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
            tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;
        }

        if (m_parameters->getUsePureBayerReprocessing() == false
            || m_parameters->isReprocessing3aaIspOTF() == false)
            tempBufferManager = ispBufferManager;

        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISP_REPROCESSING)] = m_ispReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING)] = m_sccReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING)] = m_postPictureBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING)] = m_sccReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING)] = m_thumbnailBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = m_sccReprocessingBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = m_thumbnailBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)] = m_jpegBufferMgr;
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)] = m_thumbnailBufferMgr;
#endif

        for (int i = 0; i < MAX_NODE; i++) {
            /* If even one buffer slot is valid. call setBufferManagerToPipe() */
            if (taaBufferManager[i] != NULL) {
                ret = m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, PIPE_3AA_REPROCESSING);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->setBufferManagerToPipe(taaBufferManager, %d) failed",
                            __FUNCTION__, __LINE__, PIPE_3AA_REPROCESSING);
                    return ret;
                }
                break;
            }
        }

        for (int i = 0; i < MAX_NODE; i++) {
            /* If even one buffer slot is valid. call setBufferManagerToPipe() */
            if (ispBufferManager[i] != NULL) {
                ret = m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, PIPE_ISP_REPROCESSING);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                            __FUNCTION__, __LINE__, PIPE_3AA_REPROCESSING);
                    return ret;
                }
                break;
            }
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

        /* On High-resolution callback scenario, the reprocessing works with specific fps.
           To avoid the thread creation performance issue, threads in reprocessing pipes
           must be run with run&wait mode */
        bool needOneShotMode = (m_parameters->getHighResolutionCallbackMode() == false);
        m_reprocessingFrameFactory->setThreadOneShotMode(PIPE_3AA_REPROCESSING, needOneShotMode);
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
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);
    int ret = 0;

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
            CLOGE("ERR(%s):m_reprocessingFrameFactory0>stopPipe() fail", __FUNCTION__);
        }
    }

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_highResolutionCallbackThread->stop();
        if (m_highResolutionCallbackQ != NULL)
            m_highResolutionCallbackQ->sendCmd(WAKE_UP);
        m_highResolutionCallbackThread->requestExitAndWait();
    }

    /* Clear frames & buffers which remain in capture processingQ */
    m_clearFrameQ(m_dstSccReprocessingQ, PIPE_3AA_REPROCESSING, SRC_BUFFER_DIRECTION);
    m_clearFrameQ(m_postPictureQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(m_dstJpegReprocessingQ, PIPE_JPEG, SRC_BUFFER_DIRECTION);

    m_dstIspReprocessingQ->release();
    m_dstGscReprocessingQ->release();

    m_dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();
    m_yuvCallbackQ->release();

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        m_jpegSaveQ[threadNum]->release();
    }

    if (m_highResolutionCallbackQ->getSizeOfProcessQ() != 0){
        CLOGD("DEBUG(%s[%d]):m_highResolutionCallbackQ->getSizeOfProcessQ(%d). release the highResolutionCallbackQ.",
            __FUNCTION__, __LINE__, m_highResolutionCallbackQ->getSizeOfProcessQ());
        m_highResolutionCallbackQ->release();
    }

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

    CLOGD("DEBUG(%s[%d]):clear postProcess(Picture) Frame list", __FUNCTION__, __LINE__);

    ret = m_clearList(&m_postProcessList, &m_postProcessListLock);
    if (ret < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
        return ret;
    }

    m_zslPictureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf)
{
    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();

    ExynosCameraFrame *newFrame = NULL;

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview was stopped!", __FUNCTION__, __LINE__);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (useCSC) {
        int pushFrameCnt = 0, doneFrameCnt = 0;
        int retryCnt = 5;
        bool isOldFrame = false;
        ExynosRect srcRect, dstRect;

        newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeId);
        if (newFrame == NULL) {
            CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
            goto done;
        }

        m_calcPreviewGSCRect(&srcRect, &dstRect);
        newFrame->setSrcRect(pipeId, &srcRect);
        newFrame->setDstRect(pipeId, &dstRect);

        ret = m_setupEntity(pipeId, newFrame, &previewBuf, &callbackBuf);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }
        m_previewFrameFactory->setOutputFrameQToPipe(m_previewCallbackGscFrameDoneQ, pipeId);
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        pushFrameCnt = newFrame->getFrameCount();

        do {
            isOldFrame = false;
            retryCnt--;

            CLOGV("INFO(%s[%d]):wait preview callback output", __FUNCTION__, __LINE__);
            ret = m_previewCallbackGscFrameDoneQ->waitAndPopProcessQ(&newFrame);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                if (ret == TIMED_OUT && retryCnt > 0) {
                    continue;
                } else {
                    /* TODO: doing exception handling */
                    statusRet = INVALID_OPERATION;
                    goto done;
                }
            }
            if (newFrame == NULL) {
                CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
                statusRet = INVALID_OPERATION;
                goto done;
            }

            doneFrameCnt = newFrame->getFrameCount();
            if (doneFrameCnt < pushFrameCnt) {
                isOldFrame = true;
                CLOGD("INFO(%s[%d]):Frame Count(%d/%d), retryCnt(%d)",
                    __FUNCTION__, __LINE__, pushFrameCnt, doneFrameCnt, retryCnt);
            }
        } while ((ret == TIMED_OUT || isOldFrame == true) && (retryCnt > 0));
        CLOGV("INFO(%s[%d]):preview callback done", __FUNCTION__, __LINE__);

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
            CLOGE("ERR(%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        dstAddr = callbackBuf.addr[0];
        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            srcAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, previewBuf.size[plane]);
            dstAddr = dstAddr + previewBuf.size[plane];

            if (m_ionClient >= 0)
                ion_sync_fd(m_ionClient, callbackBuf.fd[plane]);
        }

    }

done:
    if (newFrame != NULL) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    return statusRet;
}



status_t ExynosCamera::m_previewCallbackFunc(ExynosCameraFrameSP_sptr_t newFrame, bool needBufferRelease)
{
    status_t statusRet = NO_ERROR;
    ExynosCameraDurationTimer probeTimer;
    int probeTimeMSEC;
    int ret = 0;
    ExynosCameraBuffer callbackBuf;
    int pipeId = PIPE_MCSC0;
    camera_memory_t *previewCallbackHeap = NULL;

    callbackBuf.index = -2;

    ret = newFrame->getSrcBuffer(pipeId, &callbackBuf);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    probeTimer.start();
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
        !checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE) &&
        m_disablePreviewCB == false) {
        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
    }
    probeTimer.stop();
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        CLOGV("(%s[%d]): duration time(%5d msec)", __FUNCTION__, __LINE__, (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        CLOGD("(%s[%d]): duration time(%5d msec)", __FUNCTION__, __LINE__, (int)probeTimer.durationMsecs());
    else
        CLOGV("(%s[%d]): duration time(%5d msec)", __FUNCTION__, __LINE__, (int)probeTimer.durationMsecs());

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    if (needBufferRelease && callbackBuf.index >= 0) {
        ret = m_putBuffers(m_previewCallbackBufferMgr, callbackBuf.index);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
        }
    }

    return statusRet;
}

bool ExynosCamera::m_previewCallbackThreadFunc(void)
{
    bool loop = true;
    int ret = 0;
    ExynosCameraFrame *newFrame = NULL;

    CLOGV("INFO(%s[%d]):wait m_previewCallbackQ output", __FUNCTION__, __LINE__);
    ret = m_previewCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true || ret == TIMED_OUT) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);

        if (newFrame != NULL) {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }

        return false;
    }
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto done;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto done;
    }

    ret = m_previewCallbackFunc(newFrame, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_previewCallbackFunc fail", __FUNCTION__, __LINE__);
    }

done:

    if (newFrame != NULL) {
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    return loop;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        __unused int32_t pipeId,
        __unused ExynosCameraFrame *newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    CLOGV("DEBUG(%s): converting callback to preview buffer", __FUNCTION__);

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE("ERR(%s[%d]): preview was stopped!", __FUNCTION__, __LINE__);
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
        srcAddr = callbackBuf.addr[0];
        for (int plane = 0; plane < planeCount; plane++) {
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, previewBuf.size[plane]);
            srcAddr = srcAddr + previewBuf.size[plane];

            if (m_ionClient >= 0)
                ion_sync_fd(m_ionClient, previewBuf.fd[plane]);
        }
    }

done:

    return statusRet;
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
    uint32_t reprocessingFrameCount = 0;
    bool isOldFrame = false;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer ispReprocessingBuffer;

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;

    /*
     * in case of pureBayer and 3aa_isp OTF, buffer will go isp directly
     */
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        if (m_parameters->isReprocessing3aaIspOTF() == true)
            prePictureDonePipeId = PIPE_3AA_REPROCESSING;
        else
            prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    } else {
        prePictureDonePipeId = PIPE_ISP_REPROCESSING;
    }

    if (m_parameters->getHighResolutionCallbackMode() == true) {
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


    if (m_isZSLCaptureOn) {
        CLOGD("INFO(%s[%d]):fast shutter callback!!", __FUNCTION__, __LINE__);
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
    }

    /* Check available buffer */
    ret = m_getBufferManager(prePictureDonePipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):Failed to getBufferManager. pipeId %d direction %d ret %d",
                __FUNCTION__, __LINE__,
                prePictureDonePipeId, DST_BUFFER_DIRECTION, ret);
        goto CLEAN;
    }

    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(prePictureDonePipeId, bufferMgr);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):Failed to wait available buffer. pipeId %d ret %d",
                    __FUNCTION__, __LINE__,
                    prePictureDonePipeId, ret);
            goto CLEAN;
        }
    }

    if (m_parameters->isUseHWFC() == true && m_parameters->getHWFCEnable() == true) {
        ret = m_checkBufferAvailable(PIPE_HWFC_JPEG_DST_REPROCESSING, m_jpegBufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to wait available JPEG buffer. ret %d",
                    __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }

        ret = m_checkBufferAvailable(PIPE_HWFC_THUMB_SRC_REPROCESSING, m_thumbnailBufferMgr);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to wait available THUMB buffer. ret %d",
                    __FUNCTION__, __LINE__, ret);
            goto CLEAN;
        }
    }

    /* Get Bayer buffer for reprocessing */
    ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer, updateDmShot);

#if 0
    {
        char filePath[70];
        int width, height = 0;

        m_parameters->getHwSensorSize(&width, &height);

        CLOGE("INFO(%s[%d]):getHwSensorSize(%d x %d)", __FUNCTION__, __LINE__, width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/bayer_%d.raw", bayerBuffer.index);

        dumpToFile((char *)filePath, bayerBuffer.addr[0], width * height * 2);
    }
#endif

    if (ret < 0) {
        CLOGE("ERR(%s[%d]): getBayerBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

    if (bayerBuffer.index == -2) {
        CLOGE("ERR(%s[%d]): getBayerBuffer fail. buffer index is -2", __FUNCTION__, __LINE__);
        goto CLEAN_FRAME;
    }

    if (m_parameters->getCaptureExposureTime() != 0
        && m_stopLongExposure == true) {
        CLOGD("DEBUG(%s[%d]): Emergency stop", __FUNCTION__, __LINE__);
        goto CLEAN_FRAME;
    }

    if (m_parameters->getOutPutFormatNV21Enable()) {
       if ((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount() && m_parameters->getSeriesShotMode() > 0)
            || m_parameters->getHighResolutionCallbackMode()
            || m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
            || m_hdrEnabled == true
        ) {
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                if(m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
                    && m_parameters->getIsThumbnailCallbackOn()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
                } else {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                }
                if (m_parameters->isHWFCEnabled()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                    CLOGD("DEBUG(%s[%d]):disable hwFC request", __FUNCTION__, __LINE__);
                }
            }
        }
    } else {
        if((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount() && m_parameters->getSeriesShotMode() > 0)
            || (m_parameters->getSeriesShotCount() == 0)) {
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
                if (m_parameters->isHWFCEnabled()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                    CLOGD("DEBUG(%s[%d]):enable hwFC request", __FUNCTION__, __LINE__);
                }

                if (m_parameters->getIsThumbnailCallbackOn()
                ) {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
                } else {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
                }
            }
        }
    }
    CLOGD("DEBUG(%s[%d]):bayerBuffer index %d", __FUNCTION__, __LINE__, bayerBuffer.index);

    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_captureSelector->clearList(m_getBayerPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_3AC));
    }

    if (!m_isZSLCaptureOn) {
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
    }

    m_isZSLCaptureOn = false;

    /* Generate reprocessing Frame */
    ret = m_generateFrameReprocessing(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):generateFrameReprocessing fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto CLEAN_FRAME;
    }

#ifndef RAWDUMP_CAPTURE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable() && (m_parameters->getUsePureBayerReprocessing() == true)) {
        int sensorMaxW, sensorMaxH;
        int sensorMarginW, sensorMarginH;
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/RawCapture%d_%d.raw",m_cameraId, 0);
        if (m_parameters->getUsePureBayerReprocessing() == true) {
            /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
            m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        } else {
            m_parameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);
        }

        CLOGE("INFO(%s[%d]): Sensor (%d x %d)", __FUNCTION__, __LINE__, sensorMaxW, sensorMaxH);

        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */
#endif

    /*
     * If it is changed dzoom level in per-frame,
     * it might be occure difference crop region between selected bayer and commend.
     * When dirty bayer is used for reprocessing,
     * the bayer in frame selector is applied n level dzoom but take_picture commend is applied n+1 level dzoom.
     * So, we should re-set crop region value from bayer.
     */
    if (m_parameters->getUsePureBayerReprocessing() == false) {
        /* TODO: HACK: Will be removed, this is driver's job */
        ret = m_convertingStreamToShotExt(&bayerBuffer, &output_crop_info);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): shot_stream to shot_ext converting fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            goto CLEAN_FRAME;
        }

        camera2_node_group node_group_info;
        ExynosRect ratioCropSize;
        int pictureW = 0, pictureH = 0;

        memset(&node_group_info, 0x0, sizeof(camera2_node_group));
        m_parameters->getPictureSize(&pictureW, &pictureH);

        newFrame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);

        setLeaderSizeToNodeGroupInfo(&node_group_info,
                                     output_crop_info.cropRegion[0], output_crop_info.cropRegion[1],
                                     output_crop_info.cropRegion[2], output_crop_info.cropRegion[3]);

        ret = getCropRectAlign(
                node_group_info.leader.input.cropRegion[2], node_group_info.leader.input.cropRegion[3], pictureW, pictureH,
                &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                CAMERA_MCSC_ALIGN, 2, 0, 1.0);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):getCropRectAlign failed. MCSC in_crop %dx%d, MCSC(picture) out_size %dx%d", __FUNCTION__, __LINE__,
                node_group_info.leader.input.cropRegion[2], node_group_info.leader.input.cropRegion[3], pictureW, pictureH);

            ratioCropSize.x = 0;
            ratioCropSize.y = 0;
            ratioCropSize.w = node_group_info.leader.input.cropRegion[2];
            ratioCropSize.h = node_group_info.leader.input.cropRegion[3];
        }

        setCaptureCropNScaleSizeToNodeGroupInfo(&node_group_info, PERFRAME_REPROCESSING_MCSC3_POS,
                                                ratioCropSize.x, ratioCropSize.y,
                                                ratioCropSize.w, ratioCropSize.h,
                                                pictureW, pictureH);

        newFrame->storeNodeGroupInfo(&node_group_info, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

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

    newFrame->getMetaData(updateDmShot);
    m_parameters->duplicateCtrlMetadata((void *)updateDmShot);

    CLOGD("DEBUG(%s[%d]):meta_shot_ext->shot.dm.request.frameCount : %d",
            __FUNCTION__, __LINE__, getMetaDmRequestFrameCount(updateDmShot));

    /* SCC */
    if (m_parameters->isOwnScc(getCameraId()) == true) {
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

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_highResolutionCallbackQ, pipe);
        } else {
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_dstSccReprocessingQ, pipe);
        }

        /* push frame to SCC pipe */
        m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, pipe);
    } else {
        if (m_parameters->isReprocessing3aaIspOTF() == false) {
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

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(NULL, pipe);
        } else {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(m_dstSccReprocessingQ, pipe);
        }

    }

    /* Add frame to post processing list */
    newFrame->frameLock();
    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
        CLOGV("DEBUG(%s[%d]):does not use postPicList in highResolutionCB",
            __FUNCTION__, __LINE__, newFrame->getFrameCount());
    } else {
        CLOGD("DEBUG(%s[%d]): push frame(%d) to postPictureList",
            __FUNCTION__, __LINE__, newFrame->getFrameCount());

        ret = m_insertFrameToList(&m_postProcessList, newFrame, &m_postProcessListLock);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):[F%d]Failed to insert frame into m_postProcessList",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());
            goto CLEAN_FRAME;
        }
    }

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        goto CLEAN;
    }

    if (m_parameters->isOwnScc(getCameraId()) == true) {
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

    ret = m_setupEntity(bayerPipeId, newFrame, &bayerBuffer, NULL);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]:setupEntity fail, bayerPipeId(%d), ret(%d)",
            __FUNCTION__, __LINE__, bayerPipeId, ret);
        goto CLEAN;
    }

    m_reprocessingFrameFactory->setOutputFrameQToPipe(m_dstIspReprocessingQ, prePictureDonePipeId);

    newFrame->incRef();

    /* push the newFrameReprocessing to pipe */
    m_reprocessingFrameFactory->pushFrameToPipe(&newFrame, bayerPipeId);

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_parameters->getUsePureBayerReprocessing() == true ||
        m_parameters->isSccCapture() == true)
        m_reprocessingFrameFactory->startThread(bayerPipeId);

    /* wait ISP done */
    reprocessingFrameCount = newFrame->getFrameCount();
    do {
        CLOGI("INFO(%s[%d]):wait ISP output", __FUNCTION__, __LINE__);
        newFrame = NULL;
        isOldFrame = false;
        ret = m_dstIspReprocessingQ->waitAndPopProcessQ(&newFrame);
        retryIsp++;
        if (ret == TIMED_OUT && retryIsp < 200) {
            CLOGW("WARN(%s[%d]):ISP wait and pop return, ret(%d)",
                    __FUNCTION__, __LINE__, ret);
            continue;
        }

        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        CLOGI("INFO(%s[%d]):ISP output done", __FUNCTION__, __LINE__);

        ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):[F%d]:Failed to getSrcBuffer. pipeId %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(), bayerPipeId);
            continue;
        }

        shot_ext = (struct camera2_shot_ext *) bayerBuffer.addr[bayerBuffer.planeCount - 1];

        if (newFrame->getFrameCount() < reprocessingFrameCount) {
            isOldFrame = true;
            CLOGD("DEBUG(%s[%d]):[F%d(%d)]Reprocessing done delayed! waitingFrameCount %d",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount,
                    reprocessingFrameCount);
        }

        if (!(m_parameters->getUsePureBayerReprocessing() == true
              && m_parameters->isReprocessing3aaIspOTF() == true
              && m_parameters->isUseHWFC() == true)) {
            ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, bayerPipeId, ret);

                if (updateDmShot != NULL) {
                    delete updateDmShot;
                    updateDmShot = NULL;
                }

                return ret;
            }

            newFrame->setMetaDataEnable(true);
        }

        /* put bayer buffer */
        ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): 3AA src putBuffer fail, index(%d), ret(%d)",
                    __FUNCTION__, __LINE__, bayerBuffer.index, ret);
            goto CLEAN;
        }

        /* put isp buffer */
        if (m_parameters->getUsePureBayerReprocessing() == true
            && m_parameters->isReprocessing3aaIspOTF() == false) {
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
                    CLOGE("ERR(%s[%d]): ISP src putBuffer fail, index(%d), ret(%d)",
                            __FUNCTION__, __LINE__, bayerBuffer.index, ret);
                    goto CLEAN;
                }
            }
        }

        m_reprocessingCounter.decCount();

        CLOGI("INFO(%s[%d]):reprocessing complete, remaining count(%d)",
                __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

        if (m_hdrEnabled) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr;

            m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

            if (m_reprocessingCounter.getCount() == 0)
                m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_OFF);
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true)
            && (m_highResolutionCallbackRunning == true)) {
            m_highResolutionCallbackQ->pushProcessQ(&newFrame);
        }
    } while ((ret == TIMED_OUT || isOldFrame == true) && retryIsp < 200 && m_flagThreadStop != true);

    if ((m_parameters->getHighResolutionCallbackMode() == true)
        && (m_highResolutionCallbackRunning == true)) {
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
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE("ERR(%s[%d]): getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);

            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

CLEAN:
    if (newFrame != NULL) {
        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);

        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);

            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

        if (m_parameters->getUsePureBayerReprocessing() == true) {
            ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            if (bufferMgr != NULL) {
                ret = newFrame->getDstBuffer(bayerPipeId, &ispReprocessingBuffer,
                        m_pictureFrameFactory->getNodeType(PIPE_3AC));
                if (ret < 0)
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, bayerPipeId, ret);

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
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        {
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
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
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true))
        loop = true;

    if (m_reprocessingCounter.getCount() > 0)
        loop = true;

    CLOGI("INFO(%s[%d]): reprocessing fail, remaining count(%d)", __FUNCTION__, __LINE__, m_reprocessingCounter.getCount());

    return loop;
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
    int buffer_idx = m_getShotBufferIdex();
    float zoomRatio = m_parameters->getZoomRatio(0) / 1000;

    sccReprocessingBuffer.index = -2;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;
    int bayerPipeId = 0;
    bool isSrc = false;

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->isReprocessing3aaIspOTF() == false)
            pipeId_scc = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;

        pipeId_gsc = PIPE_GSC_REPROCESSING;
        isSrc = true;
    } else {
        switch (getCameraId()) {
            case CAMERA_ID_FRONT:
            case CAMERA_ID_BACK_1:
                if (m_parameters->getDualMode() == true) {
                    pipeId_scc = PIPE_3AA;
                } else {
                    pipeId_scc = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC : PIPE_ISPC;
                }
                pipeId_gsc = PIPE_GSC_PICTURE;
                break;
            default:
                CLOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    __FUNCTION__, __LINE__, getCameraId(), m_parameters->isReprocessing());
                break;
        }
    }

    /* wait SCC */
    CLOGI("INFO(%s[%d]):wait SCC output", __FUNCTION__, __LINE__);
    int retry = 0;
    do {
        ret = m_dstSccReprocessingQ->waitAndPopProcessQ(&newFrame);
        retry++;
    } while (ret == TIMED_OUT && retry < 40 &&
             (m_takePictureCounter.getCount() > 0 || m_parameters->getSeriesShotCount() == 0));

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):wait and pop fail, ret(%d), retry(%d), takePictuerCount(%d), seriesShotCount(%d)",
                __FUNCTION__, __LINE__, ret, retry, m_takePictureCounter.getCount(), m_parameters->getSeriesShotCount());
        // TODO: doing exception handling
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    if (m_postProcessList.empty()) {
        CLOGE("ERR(%s[%d]): postPictureList is empty", __FUNCTION__, __LINE__);
        usleep(5000);
        if(m_postProcessList.empty()) {
            CLOGE("ERR(%s[%d]):Retry but postPictureList is empty", __FUNCTION__, __LINE__);
            goto CLEAN;
        }
    }

    /* Get bayerPipeId at first entity */
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();

    if (m_parameters->getUsePureBayerReprocessing() == true
        && m_parameters->isReprocessing3aaIspOTF() == true
        && m_parameters->isHWFCEnabled() == true
        && !(m_parameters->getOutPutFormatNV21Enable())) {
        if (newFrame == NULL) {
            CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, bayerPipeId, ret);

            return ret;
        }

        newFrame->setMetaDataEnable(true);
    }

    /*
     * When Non-reprocessing scenario does not setEntityState,
     * because Non-reprocessing scenario share preview and capture frames
     */
    if (m_parameters->isReprocessing() == true) {
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
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
        }
    }
    if (m_parameters->getIsThumbnailCallbackOn()) {
        if (m_ThumbnailCallbackThread->isRunning()) {
            m_ThumbnailCallbackThread->join();
            CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
        }
    }
    if ((m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21)
         && (m_parameters->getIsThumbnailCallbackOn()
       )) {
        int postPicturePipeId = PIPE_MCSC1_REPROCESSING;
        if (newFrame->getRequest(postPicturePipeId) == true) {
            newFrame->incRef();

            m_dstPostPictureGscQ->pushProcessQ(&newFrame);

            ret = m_postPictureCallbackThread->run();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run magicshot thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
        }
    }

    if (m_parameters->needGSCForCapture(getCameraId()) == true) {
        /* set GSC buffer */
        if (m_parameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_scc, ret);
            goto CLEAN;
        }

        {
            int pipeId_gsc_magic = PIPE_GSC_REPROCESSING2;
            ExynosCameraBuffer postgscReprocessingBuffer;
            int previewW = 0, previewH = 0, previewFormat = 0;
            ExynosRect srcRect_magic, dstRect_magic;

            postgscReprocessingBuffer.index = -2;
            CLOGD("magic shot front POSTVIEW callback start (%s)(%d) pipe(%d)", __FUNCTION__, __LINE__,pipeId_gsc_magic);
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
            }

            int retry1 = 0;
            do {
                ret = -1;
                retry1++;
                if (m_postPictureBufferMgr->getNumOfAvailableBuffer() > 0) {
                    ret = m_setupEntity(pipeId_gsc_magic, newFrame, &sccReprocessingBuffer, NULL);
                } else {
                    /* wait available SCC buffer */
                    usleep(WAITING_TIME);
                }
                if (retry1 % 10 == 0)
                    CLOGW("WRAN(%s[%d]):retry setupEntity for GSC", __FUNCTION__, __LINE__);
            } while(ret < 0 && retry1 < (TOTAL_WAITING_TIME/WAITING_TIME));

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), retry(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc_magic, retry1, ret);
                goto CLEAN;
            }

            m_parameters->getPreviewSize(&previewW, &previewH);
            m_parameters->getPictureSize(&pictureW, &pictureH);
            pictureFormat = m_parameters->getHwPictureFormat();
            previewFormat = m_parameters->getPreviewFormat();

            CLOGD("DEBUG(%s):size preview(%d, %d,format:%d)picture(%d, %d,format:%d)", __FUNCTION__,
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

            ret = getCropRectAlign(srcRect_magic.w,  srcRect_magic.h,
                previewW,   previewH,
                &srcRect_magic.x, &srcRect_magic.y,
                &srcRect_magic.w, &srcRect_magic.h,
                2, 2, 0, zoomRatio);

            ret = newFrame->setSrcRect(pipeId_gsc_magic, &srcRect_magic);
            ret = newFrame->setDstRect(pipeId_gsc_magic, &dstRect_magic);

            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                srcRect_magic.x, srcRect_magic.y, srcRect_magic.w, srcRect_magic.h, srcRect_magic.fullW, srcRect_magic.fullH);
            CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
                dstRect_magic.x, dstRect_magic.y, dstRect_magic.w, dstRect_magic.h, dstRect_magic.fullW, dstRect_magic.fullH);

            /* push frame to GSC pipe */
            m_pictureFrameFactory->setOutputFrameQToPipe(m_dstPostPictureGscQ, pipeId_gsc_magic);
            m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc_magic);

            if (ret < 0) {
                CLOGE("ERR(%s[%d]):couldn't run magicshot thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                return INVALID_OPERATION;
            }
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
        m_parameters->getPictureSize(&pictureW, &pictureH);
        pictureFormat = m_parameters->getHwPictureFormat();
#if 1   /* HACK in case of 3AA-OTF-ISP input_cropRegion always 0, use output crop region, check the driver */
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
#endif
        srcRect.fullW = shot_stream->output_crop_region[2];
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = pictureFormat;

        if(m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
            || m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS) {
            pictureFormat = V4L2_PIX_FMT_NV21;
        } else {
            pictureFormat = JPEG_INPUT_COLOR_FMT;
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

        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
        CLOGD("DEBUG(%s):size (%d, %d, %d, %d %d %d)", __FUNCTION__,
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

        /* push frame to GSC pipe */
        m_pictureFrameFactory->setOutputFrameQToPipe(m_dstGscReprocessingQ, pipeId_gsc);
        m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_gsc);

        /* wait GSC */
        newFrame = NULL;
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
            } else if (m_stopBurstShot == true) {
                CLOGD("DEBUG(%s)(%d):stopburst! ret(%d)", __FUNCTION__, __LINE__, ret);
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

        /*
         * 15/08/21
         * make complete is need..
         * (especially, on Non-reprocessing. it shared the frame between preview and capture)
         * on handlePreviewFrame, no where to set ENTITY_STATE_COMPLETE.
         * so, we need to set ENTITY_STATE_COMPLETE here.
         */
        ret = newFrame->setEntityState(pipeId_gsc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        if (m_parameters->getIsThumbnailCallbackOn() == true
            || m_parameters->getShotMode() == SHOT_MODE_MAGIC
        ) {
            m_postPictureCallbackThread->join();
        }
        /* put SCC buffer */
        if (m_parameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
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
        if (m_parameters->isReprocessing() == true) {
            CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
            if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
                m_previewFrameFactory->setRequest(PIPE_3AC, false);
        } else {
            if (m_parameters->getUseDynamicScc() == true) {
                CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);

                if (m_parameters->isOwnScc(getCameraId()) == true)
                    m_previewFrameFactory->setRequestSCC(false);
                else
                    m_previewFrameFactory->setRequestISPC(false);
            }

            m_sccCaptureSelector->clearList(pipeId_scc, isSrc, m_pictureFrameFactory->getNodeType(PIPE_ISPC));
        }

        m_dstSccReprocessingQ->release();
    }

    /*
       Robust code for postPicture thread running.
       When push frame to postPictrueQ, the thread was start.
       But sometimes it was not started(Known issue).
       So, Manually start the thread.
     */
    if (m_postPictureThread->isRunning() == false
        && m_postPictureQ->getSizeOfProcessQ() > 0)
        m_postPictureThread->run();

    /* one shot */
    return loop;

CLEAN:
    if (sccReprocessingBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): putBuffer sccReprocessingBuffer(index:%d) in error state",
                __FUNCTION__, __LINE__, sccReprocessingBuffer.index);
        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
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
    int buffer_idx = m_getShotBufferIdex();
    int retryCountJPEG = 4;

    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *jpegFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;
    ExynosCameraBuffer thumbnailReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;
    thumbnailReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;
    int pipeId = 0;

    int currentSeriesShotMode = 0;

    struct camera2_udm *udm;
    udm = new struct camera2_udm;
    memset(udm, 0x00, sizeof(struct camera2_udm));

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            if (m_parameters->isOwnScc(getCameraId()) == false) {
                pipeId_gsc = (m_parameters->isReprocessing3aaIspOTF() == true) ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
            } else {
                pipeId_gsc = PIPE_SCC_REPROCESSING;
            }
        }
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        if (m_parameters->needGSCForCapture(getCameraId()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (m_parameters->isOwnScc(getCameraId()) == true) {
                pipeId_gsc = PIPE_SCC;
            } else {
                pipeId_gsc = PIPE_ISPC;
            }
        }

        pipeId_jpeg = PIPE_JPEG;
    }

    ExynosCameraBufferManager *bufferMgr = NULL;

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

    newFrame->getUserDynamicMeta(udm);

    if (m_parameters->getOutPutFormatNV21Enable()) {
        pipeId = PIPE_MCSC1_REPROCESSING;
        ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
            goto CLEAN;
        }
    } else {
        pipeId = PIPE_MCSC3_REPROCESSING;
        ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }
    }

    /* put picture callback buffer */
    /* get gsc dst buffers */
    ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer,
                                    m_reprocessingFrameFactory->getNodeType(pipeId));
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

#if 0
    {
        char filePath[70];
        int width, height = 0;

        newFrame->dumpNodeGroupInfo("");

        m_parameters->getPictureSize(&width, &height);

        CLOGE("INFO(%s[%d]):getPictureSize(%d x %d)", __FUNCTION__, __LINE__, width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/pic_%d.yuv", gscReprocessingBuffer.index);

        dumpToFile((char *)filePath, gscReprocessingBuffer.addr[0], width * height);
    }
#endif

    /* callback */
    ret = m_reprocessingYuvCallbackFunc(gscReprocessingBuffer);
    if (ret < NO_ERROR) {
        CLOGW("ERR(%s[%d]):[F%d]Failed to callback reprocessing YUV",
                __FUNCTION__, __LINE__,
                newFrame->getFrameCount());
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
            CLOGD("DEBUG(%s[%d]): HDR callback", __FUNCTION__, __LINE__);
            if(m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
                && m_parameters->getIsThumbnailCallbackOn()) {
                /* put Thumbnail buffer */
                ret = newFrame->getDstBuffer(pipeId_gsc, &thumbnailReprocessingBuffer,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }

                m_ThumbnailCallbackThread->run();
                m_thumbnailCallbackQ->pushProcessQ(&thumbnailReprocessingBuffer);
                CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread run", __FUNCTION__, __LINE__);
            }

            m_yuvCallbackQ->pushProcessQ(&gscReprocessingBuffer);

            m_jpegCounter.decCount();
        } else {
            if (m_parameters->isHWFCEnabled() == true
                && !(m_parameters->getOutPutFormatNV21Enable())) {
                /* get gsc dst buffers */
                entity_buffer_state_t jpegMainBufferState = ENTITY_BUFFER_STATE_NOREQ;
                ret = newFrame->getDstBufferState(pipeId_gsc, &jpegMainBufferState,
                                                    m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBufferState fail, pipeId(%d), nodeType(%d), ret(%d)",
                            __FUNCTION__, __LINE__,
                            pipeId_gsc,
                            m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING),
                            ret);
                    goto CLEAN;
                }

                if (jpegMainBufferState == ENTITY_BUFFER_STATE_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to get the encoded JPEG buffer", __FUNCTION__, __LINE__);
                    goto CLEAN;
                }

                /* put Thumbnail buffer */
                ret = newFrame->getDstBuffer(pipeId_gsc, &thumbnailReprocessingBuffer,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }

                ret = m_putBuffers(m_thumbnailBufferMgr, thumbnailReprocessingBuffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }

                ret = newFrame->getDstBuffer(pipeId_gsc, &jpegReprocessingBuffer,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }

                /* in case OTF until JPEG, we should be call below function to this position
                                in order to update debugData from frame. */
                m_parameters->setDebugInfoAttributeFromFrame(udm);

                /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
/*
                if (jpegReprocessingBuffer.size[0] != 0) {
                    UpdateDebugData(jpegReprocessingBuffer.addr[0],
                                    jpegReprocessingBuffer.size[0],
                                    m_parameters->getDebugAttribute());
                }
*/
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
                    if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0) {
                        ret = m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegReprocessingBuffer);
                        if (ret != NO_ERROR)
                            CLOGE("ERR(%s[%d]):getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    }

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
                        android_printAssert(NULL, LOG_TAG, "BURST_SHOT_TIME_ASSERT(%s[%d]):\
                                            unexpected error, get jpeg buffer failed, assert!!!!",
                                            __FUNCTION__, __LINE__);
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
                m_pictureFrameFactory->setOutputFrameQToPipe(m_dstJpegReprocessingQ, pipeId_jpeg);

                /* 4. push the newFrame to pipe */
                m_pictureFrameFactory->pushFrameToPipe(&newFrame, pipeId_jpeg);

                /* 5. wait outputQ */
                CLOGI("INFO(%s[%d]):wait Jpeg output", __FUNCTION__, __LINE__);
                newFrame = NULL;
                while (retryCountJPEG > 0) {
                    ret = m_dstJpegReprocessingQ->waitAndPopProcessQ(&newFrame);
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
                if (m_parameters->isReprocessing() == true) {
                    ret = newFrame->setEntityState(pipeId_jpeg, ENTITY_STATE_COMPLETE);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)",
                                __FUNCTION__, __LINE__, pipeId_jpeg, ret);
                        return ret;
                    }
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
            if (jpegOutputSize <= 0) {
                jpegOutputSize = jpegReprocessingBuffer.size[0];
                if (jpegOutputSize <= 0)
                    CLOGW("WRN(%s[%d]): jpegOutput size(%d) is invalid", __FUNCTION__, __LINE__, jpegOutputSize);
            }

            CLOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

            /* push postProcess to call CAMERA_MSG_COMPRESSED_IMAGE */
            int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();

            if ((m_parameters->getSeriesShotCount() > 0)
                && (seriesShotSaveLocation != BURST_SAVE_CALLBACK)
#ifdef ONE_SECOND_BURST_CAPTURE
                && (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
            ) {
                jpegFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId_jpeg, m_burstCaptureCallbackCount);
            } else {
                jpegFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId_jpeg, 0);
            }

            if (jpegFrame == NULL) {
                CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
                goto CLEAN;
            }

            ret = jpegFrame->setDstBuffer(pipeId_jpeg, jpegReprocessingBuffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
            }
 retry:
            if ((m_parameters->getSeriesShotCount() > 0)) {
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

                m_jpegSaveQ[threadNum]->pushProcessQ(&jpegFrame);
            } else {
                m_jpegCallbackQ->pushProcessQ(&jpegFrame);
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
        ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        {
            CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
    }

    CLOGI("INFO(%s[%d]):postPicture thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCounter.getCount());

    if (m_jpegCounter.getCount() <= 0) {
        ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
        flashMgr->setCaptureStatus(false);

        if(m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGI("INFO(%s[%d]): End of wideselfie capture!", __FUNCTION__, __LINE__);
            m_pictureEnabled = false;
        }

        CLOGD("DEBUG(%s[%d]): free gsc buffers", __FUNCTION__, __LINE__);
        m_gscBufferMgr->resetBuffers();

        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST)
        {
            CLOGD("DEBUG(%s[%d]): clearList postProcessList, series shot mode(%d)", __FUNCTION__, __LINE__, currentSeriesShotMode);
            if (m_clearList(&m_postProcessList, &m_postProcessListLock) < 0) {
                CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
            }
        }
    }

    if (m_parameters->getScalableSensorMode()) {
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_CHANGING);
        ret = m_restartPreviewInternal(false, NULL);
        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
        m_scalableSensorMgr.setMode(EXYNOS_CAMERA_SCALABLE_NONE);
    }

CLEAN:
    if (udm != NULL) {
        delete udm;
        udm = NULL;
    }

    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    /* HACK: Sometimes, m_postPictureThread is finished without waiting the last picture */
    int waitCount = 5;
    while (m_postPictureQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_postPictureQ->getSizeOfProcessQ() > 0
        || currentSeriesShotMode != SERIES_SHOT_MODE_NONE
        || m_jpegCounter.getCount() > 0) {
        CLOGD("DEBUG(%s[%d]):postPicture thread will run again. ShotMode(%d), postPictureQ(%d), Remaining(%d)",
            __FUNCTION__, __LINE__,
            currentSeriesShotMode,
            m_postPictureQ->getSizeOfProcessQ(),
            m_jpegCounter.getCount());
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
    char burstFilePath[CAMERA_FILE_PATH_SIZE];
#ifdef BURST_CAPTURE
    int fd = -1;
#endif

    ExynosCameraFrame *newFrame = NULL;
//    camera_memory_t *jpegCallbackHeap = NULL;

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        if (m_burst[threadNum] == true && m_running[threadNum] == false) {
            m_running[threadNum] = true;
            curThreadNum = threadNum;
            if (m_jpegSaveQ[curThreadNum]->waitAndPopProcessQ(&newFrame) < 0) {
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

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto done;
    }

    m_jpegCallbackQ->pushProcessQ(&newFrame);

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

bool ExynosCamera::m_yuvCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int retry = 0, maxRetry = 0;
    ExynosCameraBuffer postviewCallbackBuffer;
    ExynosCameraBuffer gscReprocessingBuffer;
    camera_memory_t *postviewCallbackHeap = NULL;
    ExynosCameraBufferManager *bufferMgr = NULL;
    ExynosCameraBufferManager *postviewBufferMgr = NULL;
    int currentSeriesShotMode = 0;
    int pipeId = 0;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    int loop = false;
    int ret = 0;

    postviewCallbackBuffer.index = -2;
    gscReprocessingBuffer.index = -2;
    currentSeriesShotMode = m_parameters->getSeriesShotMode();

    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

    pipeId = PIPE_MCSC1_REPROCESSING;
    ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
        return ret;
    }

    do {
        ret = m_yuvCallbackQ->waitAndPopProcessQ(&gscReprocessingBuffer);
        if (ret < 0) {
            retry++;
            CLOGW("WARN(%s[%d]):m_yuvCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
        }
    } while (ret < 0 && retry < maxRetry &&
             m_flagThreadStop != true &&
             m_stopBurstShot == false &&
             !m_cancelPicture);

    m_captureLock.lock();


    CLOGD("DEBUG(%s[%d]): NV21 callback", __FUNCTION__, __LINE__);

    /* send yuv image with jpeg callback */
    camera_memory_t *jpegCallbackHeap = NULL;
    jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
    if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
        goto CLEAN;
    }

    setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
    m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
    clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

    jpegCallbackHeap->release(jpegCallbackHeap);

    CLOGD("DEBUG(%s[%d]): NV21 callback end", __FUNCTION__, __LINE__);

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
                CLOGW("WARN(%s[%d]):postviewCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while (ret < 0 && retry < maxRetry &&
                 m_flagThreadStop != true &&
                 m_stopBurstShot == false &&
                 !m_cancelPicture);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0],
                                             postviewCallbackBuffer.size[0],
                                             1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):get postviewCallbackHeap(%d) fail", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        if (!m_cancelPicture) {
            CLOGD("DEBUG(%s[%d]): thumbnail POSTVIEW callback start", __FUNCTION__, __LINE__);
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            CLOGD("DEBUG(%s[%d]): thumbnail POSTVIEW callback end", __FUNCTION__, __LINE__);
        }
        postviewCallbackHeap->release(postviewCallbackHeap);
    }

    m_yuvcallbackCounter.decCount();

CLEAN:
    CLOGI("INFO(%s[%d]):yuv callback thread complete, remaining count(%d)",
        __FUNCTION__, __LINE__, m_yuvcallbackCounter.getCount());

    if (gscReprocessingBuffer.index != -2) {
        /* put GSC buffer */
        ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret);
        }
    }

    if (postviewCallbackBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): put postviewCallbackBuffer(%d)", __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

    if (m_yuvcallbackCounter.getCount() <= 0) {

        if (m_parameters->getOutPutFormatNV21Enable() == true) {
            m_parameters->setOutPutFormatNV21Enable(false);
        }

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

        loop = false;
        m_terminatePictureThreads(true);
        m_captureLock.unlock();
    } else {
        loop = true;
        m_captureLock.unlock();
    }

    return loop;
}

bool ExynosCamera::m_jpegCallbackThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

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
    ExynosCameraFrame *newFrame = NULL;

    jpegCallbackBuffer.index = -2;

    postviewCallbackBuffer.index = -2;

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    currentSeriesShotMode = m_parameters->getSeriesShotMode();

    if (m_parameters->isReprocessing() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    if (m_flashMgr->getNeedFlash() == true) {
        maxRetry = TOTAL_FLASH_WATING_COUNT;
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    if ((m_parameters->getIsThumbnailCallbackOn() == true)
         && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
        /* One second burst will get thumbnail RGB after get jpegcallbackheap */
        if (m_cancelPicture == true) {
            loop = false;
            goto CLEAN;
        }

        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("WARN(%s[%d]):postviewCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while (ret < 0
                 && retry < maxRetry
                 && m_flagThreadStop == false
                 && m_stopBurstShot == false
                 && m_cancelPicture == false);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0], 1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):get postviewCallbackHeap fail", __FUNCTION__, __LINE__);
            loop = true;
            goto CLEAN;
        }

        if (m_cancelPicture == false) {
            CLOGD("thumbnail POSTVIEW callback start(%s)(%d)", __FUNCTION__, __LINE__);
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            CLOGD("thumbnail POSTVIEW callback end(%s)(%d)", __FUNCTION__, __LINE__);
        }
        postviewCallbackHeap->release(postviewCallbackHeap);
    }

    if (m_cancelPicture == true) {
        loop = false;
        goto CLEAN;
    }

    do {
        ret = m_jpegCallbackQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            retry++;
            CLOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
        }
    } while (ret < 0
             && retry < maxRetry
             && m_jpegCounter.getCount() > 0
             && m_cancelPicture == false);

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    m_captureLock.lock();

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        loop = true;
        goto CLEAN;
    }

    seriesShotNumber = newFrame->getFrameCount();

    CLOGD("DEBUG(%s[%d]):jpeg calllback is start", __FUNCTION__, __LINE__);

    /* Make compressed image */
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_parameters->getSeriesShotCount() > 0) {
            if (jpegCallbackBuffer.index != -2) {
                jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            }
            if (jpegCallbackHeap == NULL) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

            if (m_cancelPicture == false) {
                setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callback (%d)",
                        __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
            }

            if (jpegCallbackBuffer.index != -2) {
                /* put JPEG callback buffer */
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                    CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
            }

            jpegCallbackHeap->release(jpegCallbackHeap);
    } else {
        CLOGD("DEBUG(%s[%d]): Disabled compressed image", __FUNCTION__, __LINE__);
    }

CLEAN:
    CLOGI("INFO(%s[%d]):jpeg callback thread complete, remaining count(%d)", __FUNCTION__, __LINE__, m_takePictureCounter.getCount());

    if (postviewCallbackBuffer.index != -2) {
        CLOGD("DEBUG(%s[%d]): put dst(%d)", __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): jpeg callback frame delete(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
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
    int32_t PictureFormat = 0;
    int32_t SrcFormat = 0;
    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    int thumbnailH, thumbnailW;
    int inputSizeH, inputSizeW;
    int waitCount = 0;
    int dstbufferIndex = -1;
    ExynosCameraFrame *callbackFrame = NULL;
    ExynosCameraBuffer postgscReprocessingSrcBuffer;
    ExynosCameraBuffer postgscReprocessingDstBuffer;
    ExynosCameraBufferManager *srcbufferMgr = NULL;
    ExynosCameraBufferManager *dstbufferMgr = NULL;
    int gscPipe = PIPE_GSC_REPROCESSING3;
    int retry = 0;
    int framecount = 0;
    int loop = false;

    postgscReprocessingSrcBuffer.index = -2;
    postgscReprocessingDstBuffer.index = -2;

    CLOGD("DEBUG(%s[%d]):-- IN --", __FUNCTION__, __LINE__);

    /* wait GSC */
    CLOGV("INFO(%s[%d]):Wait Thumbnail GSC output", __FUNCTION__, __LINE__);
    ret = m_thumbnailCallbackQ->waitAndPopProcessQ(&postgscReprocessingSrcBuffer);
    CLOGD("INFO(%s[%d]):Thumbnail GSC output done", __FUNCTION__, __LINE__);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
       goto CLEAN;
    }

    m_getBufferManager(gscPipe, &srcbufferMgr, SRC_BUFFER_DIRECTION);
    m_getBufferManager(gscPipe, &dstbufferMgr, DST_BUFFER_DIRECTION);

    callbackFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(gscPipe, framecount);

    previewFormat = m_parameters->getHwPreviewFormat();
    PictureFormat = m_parameters->getHwPictureFormat();
    m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);

    if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21) {
        m_parameters->getThumbnailSize(&inputSizeW, &inputSizeH);
        SrcFormat = PictureFormat;
        srcbufferMgr = m_thumbnailBufferMgr;
    }
    else {
        m_parameters->getPreviewSize(&inputSizeW, &inputSizeH);
        SrcFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
    }

    do {
        dstbufferIndex = -2;
        retry++;

        if (dstbufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = dstbufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &postgscReprocessingDstBuffer);
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        if (dstbufferIndex < 0) {
            usleep(WAITING_TIME);
            if (retry % 20 == 0) {
                CLOGW("WRN(%s[%d]):retry Post View Thumbnail getBuffer)", __FUNCTION__, __LINE__);
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

    CLOGD("DEBUG(%s[%d]):srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)",
        __FUNCTION__, __LINE__, postgscReprocessingSrcBuffer.index, postgscReprocessingDstBuffer.index,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

    ret = callbackFrame->setSrcRect(gscPipe, srcRect);
    ret = callbackFrame->setDstRect(gscPipe, dstRect);

    ret = m_setupEntity(gscPipe, callbackFrame, &postgscReprocessingSrcBuffer, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, gscPipe, ret);
    }

    m_pictureFrameFactory->setOutputFrameQToPipe(m_ThumbnailPostCallbackQ, gscPipe);
    m_pictureFrameFactory->pushFrameToPipe(&callbackFrame, gscPipe);

    /* wait GSC done */
    CLOGV("INFO(%s[%d]):wait GSC output", __FUNCTION__, __LINE__);
    waitCount = 0;

    do {
        ret = m_ThumbnailPostCallbackQ->waitAndPopProcessQ(&callbackFrame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):GSC wait and pop return, ret(%d)", __FUNCTION__, __LINE__, ret);
    }
    if (callbackFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = callbackFrame->getDstBuffer(gscPipe, &postgscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, gscPipe, ret);
        goto CLEAN;
    }

#if 0 /* dump code */
    char filePath[70];
    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.rgb",
            m_parameters->getCameraId(), newFrame->getFrameCount());

    ret = dumpToFile((char *)filePath,
        postgscReprocessingDstBuffer.addr[0],
        FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH));
#endif

    m_postviewCallbackQ->pushProcessQ(&postgscReprocessingDstBuffer);

    CLOGD("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

CLEAN:

    m_pictureFrameFactory->setFlagFlipIgnoreToPipe(gscPipe, false);

    if (postgscReprocessingDstBuffer.index != -2 && ret < 0)
        m_putBuffers(dstbufferMgr, postgscReprocessingDstBuffer.index);

    if (postgscReprocessingSrcBuffer.index != -2)
        m_putBuffers(srcbufferMgr, postgscReprocessingSrcBuffer.index);

    if (callbackFrame != NULL) {
        callbackFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, callbackFrame->getFrameCount());
        callbackFrame->decRef();
        m_frameMgr->deleteFrame(callbackFrame);
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
    ExynosCameraFrame *newFrame = NULL;
    int pipeId_jpeg = 0;

    CLOGI("INFO(%s[%d]): takePicture disabled, takePicture callback done takePictureCounter(%d)",
            __FUNCTION__, __LINE__, m_takePictureCounter.getCount());

    m_pictureEnabled = false;

    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    flashMgr->setCaptureStatus(false);

    if (m_parameters->getUseDynamicScc() == true) {
        CLOGD("DEBUG(%s[%d]): Use dynamic bayer", __FUNCTION__, __LINE__);
        if (m_parameters->isOwnScc(getCameraId()) == true)
            m_previewFrameFactory->setRequestSCC(false);
        else
            m_previewFrameFactory->setRequestISPC(false);
    }

    if (m_parameters->isReprocessing() == true) {
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

    CLOGI("INFO(%s[%d]): wait m_prePictureThrad", __FUNCTION__, __LINE__);
    m_prePictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_pictureThrad", __FUNCTION__, __LINE__);
    m_pictureThread->requestExitAndWait();
 
    CLOGI("INFO(%s[%d]): wait m_postPictureCallbackThread", __FUNCTION__, __LINE__);
    m_postPictureCallbackThread->requestExitAndWait();
    if (m_ThumbnailCallbackThread != NULL) {
        CLOGI("INFO(%s[%d]): wait m_ThumbnailCallbackThread", __FUNCTION__, __LINE__);
        m_ThumbnailCallbackThread->requestExitAndWait();
    }

    CLOGI("INFO(%s[%d]): wait m_postPictureThrad", __FUNCTION__, __LINE__);
    m_postPictureThread->requestExitAndWait();

    m_parameters->setIsThumbnailCallbackOn(false);

    CLOGI("INFO(%s[%d]): wait m_jpegCallbackThrad", __FUNCTION__, __LINE__);
    CLOGI("INFO(%s[%d]): wait m_yuvCallbackThrad", __FUNCTION__, __LINE__);
    if (!callFromJpeg) {
        m_jpegCallbackThread->requestExitAndWait();
        m_yuvCallbackThread->requestExitAndWait();
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
         CLOGI("INFO(%s[%d]): wait m_jpegSaveThrad%d", __FUNCTION__, __LINE__, threadNum);
         m_jpegSaveThread[threadNum]->requestExitAndWait();
    }

    CLOGI("INFO(%s[%d]): All picture threads done", __FUNCTION__, __LINE__);

    if (m_parameters->isReprocessing() == true) {
        enum pipeline pipe = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;

        m_reprocessingFrameFactory->stopThread(pipe);
    }

    if(m_parameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING3);
            CLOGI("INFO(%s[%d]):rear gsc , pipe stop(%d)",__FUNCTION__, __LINE__, PIPE_GSC_REPROCESSING3);
    }

    while (m_jpegCallbackQ->getSizeOfProcessQ() > 0) {
        jpegCallbackBuffer.index = -2;
        newFrame = NULL;
        m_jpegCallbackQ->popProcessQ(&newFrame);
        ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        CLOGD("DEBUG(%s[%d]):put remaining jpeg buffer(index: %d)", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
            CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
        }

        if (newFrame != NULL) {
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

            CLOGD("DEBUG(%s[%d]): remaining frame delete(%d)",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());

            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
            newFrame = NULL;
        }
     }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &thumbnailBufferMgr, SRC_BUFFER_DIRECTION);
    while (m_thumbnailCallbackQ->getSizeOfProcessQ() > 0) {
        m_thumbnailCallbackQ->popProcessQ(&thumbnailCallbackBuffer);

        CLOGD("DEBUG(%s[%d]):put remaining thumbnailCallbackBuffer buffer(index: %d)",
                __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(thumbnailBufferMgr, thumbnailCallbackBuffer.index);
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    while (m_postviewCallbackQ->getSizeOfProcessQ() > 0) {
        m_postviewCallbackQ->popProcessQ(&postviewCallbackBuffer);

        CLOGD("DEBUG(%s[%d]):put remaining postview buffer(index: %d)",
                    __FUNCTION__, __LINE__, postviewCallbackBuffer.index);
        m_putBuffers(postviewBufferMgr, postviewCallbackBuffer.index);
    }

    for (int threadNum = JPEG_SAVE_THREAD0; threadNum < JPEG_SAVE_THREAD_MAX_COUNT; threadNum++) {
        while (m_jpegSaveQ[threadNum]->getSizeOfProcessQ() > 0) {
            jpegCallbackBuffer.index = -2;
            newFrame = NULL;
            m_jpegSaveQ[threadNum]->popProcessQ(&newFrame);
            if (newFrame != NULL) {
                ret = newFrame->getDstBuffer(pipeId_jpeg, &jpegCallbackBuffer);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                }

                CLOGD("DEBUG(%s[%d]):put remaining SaveQ%d jpeg buffer(index: %d)",
                        __FUNCTION__, __LINE__, threadNum, jpegCallbackBuffer.index);
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
                }

                CLOGD("DEBUG(%s[%d]): remaining frame delete(%d)",
                        __FUNCTION__, __LINE__, newFrame->getFrameCount());
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }
        }
        m_burst[threadNum] = false;
    }

    if (m_parameters->isReprocessing() == true) {
        enum pipeline pipe = (m_parameters->isOwnScc(getCameraId()) == true) ? PIPE_SCC_REPROCESSING : PIPE_3AA_REPROCESSING;
        CLOGD("DEBUG(%s[%d]): Wait thread exit Pipe(%d) ", __FUNCTION__, __LINE__, pipe);
        ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):3AA reprocessing stopThread fail, pipe(%d) ret(%d)", __FUNCTION__, __LINE__, pipe, ret);

        ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
        if (ret != NO_ERROR)
            CLOGE("ERR(%s[%d]):stopThreadAndWait fail, pipeId(%d) ret(%d)", __FUNCTION__, __LINE__, pipe, ret);

        if (m_parameters->needGSCForCapture(m_cameraId) == true) {
            pipe = PIPE_GSC_REPROCESSING;
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):GSC stopThread fail, pipe(%d) ret(%d)", __FUNCTION__, __LINE__, pipe,  ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):stopThreadAndWait fail, pipeId(%d) ret(%d)", __FUNCTION__, __LINE__, pipe, ret);
        }
    }

    CLOGD("DEBUG(%s[%d]): clear postProcessList", __FUNCTION__, __LINE__);
    if (m_clearList(&m_postProcessList, &m_postProcessListLock) < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }

#if 1
    CLOGD("DEBUG(%s[%d]): clear m_dstPostPictureGscQ", __FUNCTION__, __LINE__);
    m_dstPostPictureGscQ->release();

    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    m_postPictureQ->release();

    CLOGD("DEBUG(%s[%d]): clear m_dstSccReprocessingQ", __FUNCTION__, __LINE__);
    m_dstSccReprocessingQ->release();

    CLOGD("DEBUG(%s[%d]): clear m_dstJpegReprocessingQ", __FUNCTION__, __LINE__);
    m_dstJpegReprocessingQ->release();
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

    CLOGD("DEBUG(%s[%d]): clear m_dstSccReprocessingQ", __FUNCTION__, __LINE__);
    while(m_dstSccReprocessingQ->getSizeOfProcessQ()) {
        m_dstSccReprocessingQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif


    CLOGD("DEBUG(%s[%d]): reset postPictureBuffer", __FUNCTION__, __LINE__);
    m_postPictureBufferMgr->resetBuffers();

    CLOGD("DEBUG(%s[%d]): reset thumbnail gsc buffers", __FUNCTION__, __LINE__);
    m_thumbnailGscBufferMgr->resetBuffers();

    CLOGD("DEBUG(%s[%d]): reset buffer gsc buffers", __FUNCTION__, __LINE__);
    m_gscBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer jpeg buffers", __FUNCTION__, __LINE__);
    m_jpegBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer sccReprocessing buffers", __FUNCTION__, __LINE__);
    m_sccReprocessingBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer m_postPicture buffers", __FUNCTION__, __LINE__);
    m_postPictureBufferMgr->resetBuffers();
    CLOGD("DEBUG(%s[%d]): reset buffer thumbnail buffers", __FUNCTION__, __LINE__);
    m_thumbnailBufferMgr->resetBuffers();
}

void ExynosCamera::m_dumpVendor(void)
{
    return;
}

bool ExynosCamera::m_postPictureCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    int ret = 0;
    int pipeId = PIPE_3AA_REPROCESSING;
    int pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;

    postgscReprocessingBuffer.index = -2;

    if (m_parameters->isReprocessing() == true) {
        pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;
        pipeId = (m_parameters->isReprocessing3aaIspOTF() == true) ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
    } else {
        pipeId_gsc_magic = PIPE_GSC_VIDEO;
    }

    m_getBufferManager(pipeId_gsc_magic, &bufferMgr, DST_BUFFER_DIRECTION);

    /* wait GSC */
    CLOGI("INFO(%s[%d]):wait GSC output pipe(%d)", __FUNCTION__, __LINE__,pipeId_gsc_magic);
    ret = m_dstPostPictureGscQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    CLOGI("INFO(%s[%d]):GSC output done pipe(%d)", __FUNCTION__, __LINE__,pipeId_gsc_magic);

    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        goto CLEAN;
    }

    if (m_parameters->isReprocessing() == true) {
        /* put GCC buffer */
        ret = newFrame->getDstBuffer(pipeId, &postgscReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeId_gsc_magic));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc_magic, ret);
            goto CLEAN;
        }
    } else {
        ret = newFrame->setEntityState(pipeId_gsc_magic, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]) etEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc_magic, ret);
            return ret;
        }

        /* put GCC buffer */
        ret = newFrame->getDstBuffer(pipeId_gsc_magic, &postgscReprocessingBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc_magic, ret);
            goto CLEAN;
        }
    }

    if (m_parameters->getIsThumbnailCallbackOn()) {
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&postgscReprocessingBuffer);
        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread run", __FUNCTION__, __LINE__);
    }

    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): m_postPictureCallbackThreadFunc frame delete(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    return loop;

CLEAN:
    if (postgscReprocessingBuffer.index != -2)
        m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);

    if (newFrame != NULL) {
        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    /* one shot */
    return loop;
}

void ExynosCamera::m_printVendorCameraInfo(void)
{
    /* Do nothing */
}
}; /* namespace android */
