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
#ifdef SAMSUNG_LENS_DC
#include "LDC_Map.h"
#endif

namespace android {

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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true) {
            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;

            if (m_parameters->isFusionEnabled() == true) {
                ret = m_parameters->getFusionSize(width, height, &fusionSrcRect, &fusionDstRect);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):getFusionSize(%d, %d)) fail", __FUNCTION__, __LINE__, width, height);
                    return ret;
                }
                width  = fusionDstRect.w;
                height = fusionDstRect.h;
            }
        }
#endif

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
    if (bufferType == BUFFER_MANAGER_GRALLOC_TYPE) {
        m_scpBufferMgr->setAllocator(m_grAllocator);
#ifdef  SAMSUNG_QUICK_SWITCH
        if (m_isQuickSwitchState == QUICK_SWITCH_STBY
            && m_isSuccessedBufferAllocation == false) {
            m_reallocVendorBuffers();
        }
#endif
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

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if ((m_parameters == NULL) && (m_frameMgr == NULL)) {
        CLOGE("INFO(%s[%d]) initalize HAL", __FUNCTION__, __LINE__);
        this->initialize();
    }

    property_set("persist.sys.camera.preview","2");

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    int32_t skipFdFrameCount = 0;
    unsigned int fdCallbackSize = 0;
#ifdef SR_CAPTURE
    unsigned int srCallbackSize = 0;
#endif
#ifdef CAMERA_FAST_ENTRANCE_V1
    int wait_cnt = 0;
#endif
#if defined(OIS_CAPTURE) || defined(RAWDUMP_CAPTURE)
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
#endif

    m_flagAFDone = false;
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

#ifdef SAMSUNG_SENSOR_LISTENER
    if(m_sensorListenerThread != NULL)
        m_sensorListenerThread->run();
#endif

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

#ifdef SAMSUNG_DOF
    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES +
            sizeof(camera2_pdaf_multi_result)*m_frameMetadata.dof_row*m_frameMetadata.dof_column;
#else
    fdCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
    {
        if (m_getMemoryCb != NULL) {
            m_fdCallbackHeap = m_getMemoryCb(-1, fdCallbackSize, 1, m_callbackCookie);
            if (!m_fdCallbackHeap || m_fdCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, fdCallbackSize);
                m_fdCallbackHeap = NULL;
                goto err;
            }
        }
    }

#ifdef SR_CAPTURE
    srCallbackSize = sizeof(camera_frame_metadata_t) * NUM_OF_DETECTED_FACES;
#ifdef SAMSUNG_QUICK_SWITCH
    if (m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
    {
        if (m_getMemoryCb != NULL) {
            m_srCallbackHeap = m_getMemoryCb(-1, srCallbackSize, 1, m_callbackCookie);
            if (!m_srCallbackHeap || m_srCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, srCallbackSize);
                m_srCallbackHeap = NULL;
                goto err;
            }
        }
    }
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_parameters->getVisionMode() == false &&
        m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
    {
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }

#ifdef SUPPORT_SW_VDIS
        m_swVDIS_Mode = false;
        if(m_parameters->isSWVdisMode()) {
            m_swVDIS_InW = m_swVDIS_InH = 0;
            m_parameters->getHwPreviewSize(&m_swVDIS_InW, &m_swVDIS_InH);
            m_parameters->getPreviewFpsRange(&m_swVDIS_MinFps, &m_swVDIS_MaxFps);
            m_swVDIS_CameraID = getCameraIdInternal();
            m_swVDIS_SensorType = m_cameraSensorId;
            m_swVDIS_APType = UNI_PLUGIN_AP_TYPE_SLSI;
#ifdef SAMSUNG_OIS_VDIS
            m_swVDIS_OISMode = UNI_PLUGIN_OIS_MODE_VDIS;
            m_OISvdisMode = UNI_PLUGIN_OIS_MODE_END;
#endif
            m_swVDIS_init();

            m_parameters->setPreviewBufferCount(NUM_VDIS_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_VDIS_BUFFERS;
            VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_VDIS_BUFFERS);
        }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            m_hyperMotion_InW = m_hyperMotion_InH = 0;
            m_parameters->getHwPreviewSize(&m_hyperMotion_InW, &m_hyperMotion_InH);
            m_hyperMotion_CameraID = getCameraIdInternal();
            m_hyperMotion_SensorType = m_cameraSensorId;
            m_hyperMotion_APType = UNI_PLUGIN_AP_TYPE_SLSI;

            m_parameters->setPreviewBufferCount(NUM_HYPERMOTION_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_HYPERMOTION_BUFFERS;
            CLOGD("HyperMotion_HAL: Preview Buffer Count Change to %d", NUM_HYPERMOTION_BUFFERS);
        }
#endif
    }

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_isQuickSwitchState == QUICK_SWITCH_STBY) {
        CLOGD("DEBUG(%s[%d]): start quick switching", __FUNCTION__, __LINE__);
        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        goto quick_switching_in_startpreview;
    }
#endif

#ifdef SAMSUNG_LBP
    if(getCameraIdInternal() == CAMERA_ID_FRONT)
        m_parameters->resetNormalBestFrameCount();
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        /* In case m_previewFrameFactory is not created, wait for 1 sec */
        while (m_previewFrameFactory == NULL) {
            if (wait_cnt > 200) {
                CLOGE("ERR(%s[%d]): m_previewFrameFactory is not created for 1 SEC", __FUNCTION__, __LINE__);
                break;
            }
            usleep(5000);
            wait_cnt++;

            if (wait_cnt % 20 == 0) {
                CLOGW("WARN(%s[%d]): Waiting until m_previewFrameFactory create (wait_cnt %d)",
                        __FUNCTION__, __LINE__, wait_cnt);
            }
        }
    }
    else
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

        if (m_visionFrameFactory == NULL) {
            m_visionFrameFactory = (ExynosCameraFrameFactory *)new ExynosCameraFrameFactoryVision(m_cameraHiddenId, m_parameters);
            m_visionFrameFactory->setFrameManager(m_frameMgr);

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
        if ((m_parameters->getRestartPreview() == true) ||
            m_previewBufferCount != m_parameters->getPreviewBufferCount()) {
            ret = setPreviewWindow(m_previewWindow);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }

            m_previewBufferCount = m_parameters->getPreviewBufferCount();
        }

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            ret = m_setBuffers();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setBuffers failed, releaseBuffer", __FUNCTION__, __LINE__);
                m_isSuccessedBufferAllocation = false;
                goto err;
            }

            m_isSuccessedBufferAllocation = true;
        } else
#endif
        {
            CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
#ifdef SAMSUNG_QUICK_SWITCH
            if (m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
            {
                m_setBuffersThread->run(PRIORITY_DEFAULT);
            }
        }

        if (m_captureSelector == NULL) {
            ExynosCameraBufferManager *bufMgr = NULL;

            if (m_parameters->isReprocessing() == true)
                bufMgr = m_bayerBufferMgr;

            m_captureSelector = new ExynosCameraFrameSelector(m_parameters, bufMgr, m_frameMgr
#ifdef SUPPORT_DEPTH_MAP
                                                            , m_depthCallbackQ, m_depthMapBufferMgr
#endif
                                                            );

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
            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_parameters, bufMgr, m_frameMgr);
        }

        if (m_captureSelector != NULL)
            m_captureSelector->release();

        if (m_sccCaptureSelector != NULL)
            m_sccCaptureSelector->release();

#ifdef OIS_CAPTURE
        sCaptureMgr->resetOISCaptureFcount();
#endif

#ifdef RAWDUMP_CAPTURE
        sCaptureMgr->resetRawCaptureFcount();
#endif

        if (m_previewFrameFactory->isCreated() == false
#ifdef CAMERA_FAST_ENTRANCE_V1
            && m_fastEntrance == false
#endif
           ) {
#if defined(SAMSUNG_EEPROM)
            if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
                if (m_eepromThread != NULL) {
                    CLOGD("DEBUG(%s): eepromThread join.....", __FUNCTION__);
                    m_eepromThread->join();
                } else {
                    CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
                }
                m_parameters->setRomReadThreadDone(true);
                CLOGD("DEBUG(%s): eepromThread joined", __FUNCTION__);
            }
#endif /* SAMSUNG_EEPROM */

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

#ifdef SAMSUNG_UNIPLUGIN
        if (m_uniPluginThread != NULL)
            m_uniPluginThread->join();
#endif

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
                    m_isFirstStart == true) {

                ret = m_fastenAeStable();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
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
#ifdef SAMSUNG_COMPANION
                if (m_parameters->getFlagChangedRtHdr() == false)
#endif
                {
                    skipFrameCount = 0;
                    skipFdFrameCount = INITIAL_SKIP_FD_FRAME;
                }
            }
        }

#ifdef SAMSUNG_COMPANION
        /* clear flag */
        m_parameters->setFlagChangedRtHdr(false);
#endif

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

#ifdef USE_FADE_IN_ENTRANCE
        if (m_parameters->getFirstEntrance() == true) {
            /* Fade In/Out */
            m_parameters->setFrameSkipCount(0);
            ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 1, PIPE_FLITE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        } else {
            /* Skip Frame */
            m_parameters->setFrameSkipCount(skipFrameCount);
            ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 0, PIPE_FLITE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)", __FUNCTION__, __LINE__, ret);
            }
        }
#else
#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif
        {
            m_parameters->setFrameSkipCount(skipFrameCount);
            m_fdFrameSkipCount = skipFdFrameCount;
        }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == false)
#endif
        {
            m_setBuffersThread->join();
        }

        if (m_isSuccessedBufferAllocation == false) {
#ifdef SAMSUNG_QUICK_SWITCH
            if (m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
            {
                CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
                goto err;
            }
        }

#ifdef SAMSUNG_QUICK_SWITCH
quick_switching_in_startpreview:
        if (m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_IDLE_TO_STBY) {
            m_isQuickSwitchState = QUICK_SWITCH_STBY;
            m_flagFirstPreviewTimerOn = false;
            CLOGD("DEBUG(%s[%d]): set stby state for quick switch", __FUNCTION__, __LINE__);
            return NO_ERROR;
        } else if (m_isQuickSwitchState == QUICK_SWITCH_STBY) {
            CLOGD("DEBUG(%s[%d]): set main state for quick switch", __FUNCTION__, __LINE__);
            m_isQuickSwitchState = QUICK_SWITCH_MAIN;
            if (m_startCompanion() != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_startCompanion fail", __FUNCTION__, __LINE__);
            }

            if (m_use_companion == true) {
                m_waitCompanionThreadEnd();
            }

            if (m_parameters->getUseFastenAeStable() == true
                && m_parameters->getDualMode() == false
                && m_isFirstStart == true) {
                ret = m_fastenAeStable();
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):m_fastenAeStable() failed", __FUNCTION__, __LINE__);
                    ret = INVALID_OPERATION;
                    goto err;
                } else {
                    m_parameters->setUseFastenAeStable(false);
                    m_parameters->setFrameSkipCount(0);
                    m_fdFrameSkipCount = 0;
                }
            }

            m_frameMgr->start();
        } else {
            m_isQuickSwitchState = QUICK_SWITCH_MAIN;
        }

#ifdef START_PICTURE_THREAD
        if (m_parameters->isReprocessing() == true
            && m_parameters->checkEnablePicture() == true
            && m_parameters->getDualMode() != true
        ) {
            m_preStartPictureInternalThread->run();
            CLOGD("DEBUG(%s): m_preStartPictureInternalThread starts", __FUNCTION__);
        }
#endif
#endif
        /* FD-AE is always on */
#ifdef USE_FD_AE
        m_enableFaceDetection(m_parameters->getFaceDetectionMode());
#endif

        ret = m_startPreviewInternal();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):m_startPreviewInternal() failed", __FUNCTION__, __LINE__);
            goto err;
        }

        if (m_parameters->isReprocessing() == true && m_parameters->checkEnablePicture() == true) {
#ifdef START_PICTURE_THREAD
            if (m_parameters->getDualMode() != true) {
#ifdef SAMSUNG_QUICK_SWITCH
                m_preStartPictureInternalThread->join();
#endif
                m_startPictureInternalThread->run(PRIORITY_DEFAULT);
            } else
#endif
            {
                m_startPictureInternal();
            }
        } else {
            m_pictureFrameFactory = m_previewFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }

        if (m_parameters->checkEnablePicture() == true) {
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }

        if (m_previewWindow != NULL)
            m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

#ifdef RAWDUMP_CAPTURE
        CLOGD("DEBUG(%s[%d]):setupThread Thread start pipeId(%d)", __FUNCTION__, __LINE__, PIPE_3AA);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
#else
        /* setup frame thread */
        if (m_parameters->getDualMode() == true && (getCameraIdInternal() == CAMERA_ID_FRONT ||getCameraIdInternal() == CAMERA_ID_BACK_1)) {
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
#endif

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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true) {
            CLOGD("DEBUG(%s[%d]):m_fusionThread start", __FUNCTION__, __LINE__);
            m_fusionThread->run(PRIORITY_DEFAULT);
        }
#endif

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == false)) {
            CLOGD("DEBUG(%s[%d]):High resolution preview callback start", __FUNCTION__, __LINE__);
            if (skipFrameCount > 0)
                m_skipReprocessing = true;
            m_highResolutionCallbackRunning = true;
            m_parameters->setOutPutFormatNV21Enable(true);
            if (m_parameters->isReprocessing() == true) {
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
    }

#ifdef BURST_CAPTURE
    m_burstInitFirst = true;
#endif

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCMode()) {
        m_LensDCIndex = -1;
        m_setLensDCMap();
    }
#endif

#ifdef SAMSUNG_JQ
    {
        int HwPreviewW = 0, HwPreviewH = 0;
        UniPluginBufferData_t pluginData;

        m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        pluginData.InWidth = HwPreviewW;
        pluginData.InHeight = HwPreviewH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;

        if(m_JQpluginHandle != NULL) {
            ret = uni_plugin_set(m_JQpluginHandle,
                    JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if(ret < 0) {
                CLOGE("[JQ](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
            }
            ret = uni_plugin_init(m_JQpluginHandle);
            if(ret < 0) {
                CLOGE("[JQ](%s[%d]): Plugin init failed!!", __FUNCTION__, __LINE__);
            }
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif

    return NO_ERROR;

err:
/* Warning !!!!!!   Don't change "ret" value in error case */
/* Need to return the ret value as it was and process error-handling */
#ifdef USE_SENSOR_RETENTION
    if (m_previewFrameFactory->setControl(V4L2_CID_IS_PREVIEW_STATE, 1, PIPE_3AA) != NO_ERROR) {
        CLOGE("ERR(%s[%d]): PIPE_%d V4L2_CID_IS_PREVIEW_STATE fail",
            __FUNCTION__, __LINE__, PIPE_3AA);
    }
#endif

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        m_waitCompanionThreadEnd();
    }
#endif
#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        if (m_eepromThread != NULL) {
            m_eepromThread->join();
        } else {
            CLOGD("DEBUG(%s): eepromThread is NULL.", __FUNCTION__);
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }

    m_fastEntrance = false;
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->requestExitAndWait();
    }

#ifdef SAMSUNG_HRM
    if (m_uv_rayHandle != NULL) {
        m_parameters->m_setHRM_Hint(false);
        sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY);
        sensor_listener_unload(&m_uv_rayHandle);
        m_uv_rayHandle = NULL;
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if (m_light_irHandle != NULL) {
        m_parameters->m_setLight_IR_Hint(false);
        sensor_listener_disable_sensor(m_light_irHandle, ST_LIGHT_IR);
        sensor_listener_unload(&m_light_irHandle);
        m_light_irHandle = NULL;
    }
#endif

#ifdef SAMSUNG_GYRO
    if (m_gyroHandle != NULL) {
        m_parameters->m_setGyroHint(false);
        sensor_listener_disable_sensor(m_gyroHandle, ST_GYROSCOPE);
        sensor_listener_unload(&m_gyroHandle);
        m_gyroHandle = NULL;
    }
#endif

#ifdef SAMSUNG_ACCELEROMETER
    if (m_accelerometerHandle != NULL) {
        m_parameters->m_setAccelerometerHint(false);
        sensor_listener_disable_sensor(m_accelerometerHandle, ST_ACCELEROMETER);
        sensor_listener_unload(&m_accelerometerHandle);
        m_accelerometerHandle = NULL;
}
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SAMSUNG_UNIPLUGIN
    if (m_uniPluginThread != NULL)
        m_uniPluginThread->join();
#endif

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

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_isQuickSwitchState == QUICK_SWITCH_STBY
        && m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_IDLE_TO_STBY) {
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_DONE);
        m_flagFirstPreviewTimerOn = false;
        m_parameters->setUseFastenAeStable(true);
        m_isFirstStart = true;
        CLOGD("DEBUG(%s[%d]): quick switching standby ", __FUNCTION__, __LINE__);
        return;
    }
#endif

    char previewMode[100] = {0};

    property_get("persist.sys.camera.preview", previewMode, "");

    if (!strstr(previewMode, "0"))
        CLOGI("INFO(%s[%d]) persist.sys.camera.preview(%s)", __FUNCTION__, __LINE__, previewMode);

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrame *frame = NULL;

#ifdef ONE_SECOND_BURST_CAPTURE
    m_clearOneSecondBurst(false);
#endif

#ifdef LLS_CAPTURE
    m_parameters->setLLSOn(false);
#endif

    m_flagLLSStart = false;
    m_flagFirstPreviewTimerOn = false;

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastenAeThread != NULL) {
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread", __FUNCTION__, __LINE__);
        m_fastenAeThread->requestExitAndWait();
        CLOGI("INFO(%s[%d]): wait m_fastenAeThread end", __FUNCTION__, __LINE__);
    } else {
        CLOGI("INFO(%s[%d]):m_fastenAeThread is NULL", __FUNCTION__, __LINE__);
    }
#endif

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
            m_parameters->setOutPutFormatNV21Enable(false);

            CLOGD("DEBUG(%s[%d]):High resolution preview callback stop", __FUNCTION__, __LINE__);
            if(getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1) {
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        m_fusionThread->stop();
        m_fusionQ->sendCmd(WAKE_UP);
        m_fusionThread->requestExitAndWait();
#endif

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
        while (m_facedetectQ->getSizeOfProcessQ()) {
            m_facedetectQ->popProcessQ(&frame);
            frame->decRef();
            m_frameMgr->deleteFrame(frame);
            frame = NULL;
        }

#ifdef SUPPORT_DEPTH_MAP
        ExynosCameraBuffer depthCallbackBuffer;

        m_previewFrameFactory->setRequest(PIPE_VC1, false);
        m_parameters->setDepthCallbackOnCapture(false);
        m_parameters->setDepthCallbackOnPreview(false);
        //m_parameters->setDisparityMode(COMPANION_DISPARITY_OFF); /* temp*/
        CLOGI("INFO(%s[%d])[Depth] Disable DepthMap", __FUNCTION__, __LINE__);

        while (m_depthCallbackQ->getSizeOfProcessQ() > 0) {
            m_depthCallbackQ->popProcessQ(&depthCallbackBuffer);

            CLOGD("DEBUG(%s[%d]):put remaining depthCallbackBuffer buffer(index: %d)",
                    __FUNCTION__, __LINE__, depthCallbackBuffer.index);
            m_putBuffers(m_depthMapBufferMgr, depthCallbackBuffer.index);
        }
#endif

        if (m_previewQ != NULL) {
             m_clearList(m_previewQ);
        }

        if (m_zoomPreviwWithCscQ != NULL) {
             m_zoomPreviwWithCscQ->release();
        }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_fusionQ != NULL) {
             m_fusionQ->release();
        }
#endif

        if (m_previewCallbackGscFrameDoneQ != NULL) {
            m_clearList(m_previewCallbackGscFrameDoneQ);
        }

        ret = m_stopPreviewInternal();
        if (ret < 0)
            CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);

#ifdef SUPPORT_SW_VDIS
        if(m_swVDIS_Mode) {
            m_swVDIS_deinit();
            for (int bufIndex = 0; bufIndex < m_sw_VDIS_OutBufferNum; bufIndex++) {
                m_putBuffers(m_swVDIS_BufferMgr, bufIndex);
            }

            if (m_swVDIS_BufferMgr != NULL) {
                m_swVDIS_BufferMgr->deinit();
            }

            if (m_previewWindow != NULL)
                m_previewWindow->set_crop(m_previewWindow, 0, 0, 0, 0);

            if (m_parameters->increaseMaxBufferOfPreview()) {
                m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
            } else {
                m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
            }
            VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d",
                        m_exynosconfig->current->bufInfo.num_preview_buffers);

            if (m_previewDelayQ != NULL) {
                m_clearList(m_previewDelayQ);
            }
        }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            for (int bufIndex = 0; bufIndex < NUM_HYPERMOTION_BUFFERS; bufIndex++) {
                m_putBuffers(m_hyperMotion_BufferMgr, bufIndex);
            }

            if (m_hyperMotion_BufferMgr != NULL) {
                m_hyperMotion_BufferMgr->deinit();
            }

            if (m_previewWindow != NULL)
                m_previewWindow->set_crop(m_previewWindow, 0, 0, 0, 0);

            if (m_parameters->increaseMaxBufferOfPreview()) {
                m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
            } else {
                m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
            }
            CLOGD("HyperMotion_HAL: Preview Buffer Count Change to %d",
                        m_exynosconfig->current->bufInfo.num_preview_buffers);
        }
#endif /*SAMSUNG_HYPER_MOTION*/
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
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        m_depthMapBufferMgr->deinit();
    }
#endif
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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }
#endif
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

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->deinit();
    }
#endif

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

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

    m_burstInitFirst = false;

    if (m_fdCallbackHeap != NULL) {
        m_fdCallbackHeap->release(m_fdCallbackHeap);
        m_fdCallbackHeap = NULL;
    }

#ifdef SR_CAPTURE
    if (m_srCallbackHeap != NULL) {
        m_srCallbackHeap->release(m_srCallbackHeap);
        m_srCallbackHeap = NULL;
    }
#endif

#ifdef SAMSUNG_DNG
    if (m_dngBayerHeap != NULL) {
        m_dngBayerHeap->release(m_dngBayerHeap);
        m_dngBayerHeap = NULL;
    }
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_isQuickSwitchState == QUICK_SWITCH_MAIN
        && m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_MAIN_TO_STBY) {
        m_deallocVendorBuffers();

        if (m_stopCompanion() != NO_ERROR)
            CLOGE("ERR(%s[%d]):m_stopCompanion() fail", __FUNCTION__, __LINE__);

        CLOGD("DEBUG(%s[%d]): change state (main -> stby)", __FUNCTION__, __LINE__);
        m_isQuickSwitchState = QUICK_SWITCH_STBY;

        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_DONE);

        m_use_companion = false;
        m_parameters->setUseCompanion(false);

#ifdef SAMSUNG_HRM
        if (m_uv_rayHandle != NULL) {
            m_parameters->m_setHRM_Hint(false);
            sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY);
            sensor_listener_unload(&m_uv_rayHandle);
            m_uv_rayHandle = NULL;
        }
#endif
    }
#endif

#ifdef SAMSUNG_LENS_DC
    if (m_DCpluginHandle != NULL) {
        m_skipLensDC = true;
        m_LensDCIndex = -1;
        ret = uni_plugin_deinit(m_DCpluginHandle);
        if(ret < 0) {
            CLOGE("[DC](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_deinit(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF][%s][%d] Out-focus shot parameter is not reset. Reset here forcely!!: %d",
                __FUNCTION__, __LINE__, m_lensmoveCount);
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setStartLensMove(false);
        m_lensmoveCount = 0;
        m_parameters->setMoveLensCount(m_lensmoveCount);
        m_parameters->setMoveLensTotal(m_lensmoveCount);
    }
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL)
        m_sensorListenerThread->requestExitAndWait();

#ifdef SAMSUNG_HRM
    if(m_uv_rayHandle != NULL)
        m_parameters->m_setHRM_Hint(false);
#endif
#ifdef SAMSUNG_LIGHT_IR
    if(m_light_irHandle != NULL)
        m_parameters->m_setLight_IR_Hint(false);
#endif
#ifdef SAMSUNG_GYRO
    if(m_gyroHandle != NULL)
        m_parameters->m_setGyroHint(false);
#endif
#ifdef SAMSUNG_ACCELEROMETER
    if (m_accelerometerHandle != NULL)
        m_parameters->m_setAccelerometerHint(false);
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

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

status_t ExynosCamera::startRecording()
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_HLV
    int curVideoW = 0, curVideoH = 0;
    uint32_t curMinFps = 0, curMaxFps = 0;
#endif

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

#ifdef SAMSUNG_HYPER_MOTION
    if (m_parameters->getHyperMotionMode() == true) {
        UNI_PLUGIN_OPERATION_MODE motionSpeed;

        switch (m_parameters->getHyperMotionSpeed()) {
            case 1:
                motionSpeed = UNI_PLUGIN_OP_4X_RECORDING;
                break;
            case 2:
                motionSpeed = UNI_PLUGIN_OP_8X_RECORDING;
                break;
            case 3:
                motionSpeed = UNI_PLUGIN_OP_16X_RECORDING;
                break;
            case 4:
                motionSpeed = UNI_PLUGIN_OP_32X_RECORDING;
                break;
            default:
                motionSpeed = UNI_PLUGIN_OP_AUTO_RECORDING;
                break;
        }

        m_hyperMotionPlaySpeedSet(motionSpeed);
        m_hyperMotion_init();
        m_hyperMotionModeSet(true);
        m_hyperMotionStep = HYPER_MOTION_START;
    }
#endif

    /* Do start recording process */
    ret = m_startRecordingInternal();
    if (ret < 0) {
        CLOGE("ERR");
        return ret;
    }

#ifdef SAMSUNG_HLV
    m_parameters->getVideoSize(&curVideoW, &curVideoH);
    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_HLV == NULL
        && curVideoW <= 1920 && curVideoH <= 1080
        && curMinFps <= 60 && curMaxFps <= 60
#ifdef USE_LIVE_BROADCAST
        && m_parameters->getPLBMode() == false
#endif
#ifdef SAMSUNG_HYPER_MOTION
        && m_parameters->getHyperMotionMode() == false
#endif
        && m_parameters->isSWVdisMode() == false) {
        ALOGD("HLV : uni_plugin_load !");
        m_HLV = uni_plugin_load(HIGHLIGHT_VIDEO_PLUGIN_NAME);
        if (m_HLV != NULL) {
            ALOGD("HLV : uni_plugin_load success!");

            ret = uni_plugin_init(m_HLV);
            if (ret < 0) {
                ALOGE("HLV : uni_plugin_init fail . Unload %s", HIGHLIGHT_VIDEO_PLUGIN_NAME);

                ret = uni_plugin_unload(&m_HLV);
                if (ret)
                    ALOGE("HLV : uni_plug_unload failed !!");

                m_HLV = NULL;
            }
            else
            {
                ALOGD("HLV : uni_plugin_init success!");
                UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START;
                ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START");

                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
        }
        else {
            ALOGD("HLV : uni_plugin_load fail!");
        }
    }
    else {
        ALOGD("HLV : Not Supported for %dx%d(%d fps) video record !",
            curVideoW, curVideoH, curMinFps);
    }
#endif

    m_lastRecordingTimeStamp  = 0;
    m_recordingStartTimeStamp = 0;
    m_recordingFrameSkipCount = 0;

    m_setRecordingEnabled(true);
    m_parameters->setRecordingRunning(true);

    autoFocusMgr->setRecordingHint(true);
    flashMgr->setRecordingHint(true);
    if (m_parameters->doCscRecording() == true) {
#ifdef SUPPORT_SW_VDIS
        /* VDIS uses MCSC0 only */
        if (m_swVDIS_Mode == false)
#endif
        {
            m_previewFrameFactory->setRequest(PIPE_MCSC2, true);
        }
    }

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

#ifdef SAMSUNG_HLV
    if (m_HLV != NULL)
    {
        UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP;
        ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP");

        uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                &value);
    }
#endif

    /* Do stop recording process */
    ret = m_stopRecordingInternal();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):m_stopRecordingInternal fail", __FUNCTION__, __LINE__);

#ifdef USE_FD_AE
    if (m_parameters != NULL) {
        m_enableFaceDetection(m_parameters->getFaceDetectionMode(false));
    }
#endif

#ifdef SAMSUNG_HYPER_MOTION
    if (m_hyperMotionModeGet()== true) {
        m_hyperMotionStep = HYPER_MOTION_STOP;
    }
#endif /*SAMSUNG_HYPER_MOTION*/

#ifdef SAMSUNG_HLV
    m_HLVprocessStep = HLV_PROCESS_STOP;
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
            CLOGE("DEBUG(%s[%d]):HAL can't set preview exposure time.", __FUNCTION__, __LINE__);
            return INVALID_OPERATION;
        }
    }

    retryCount = 0;

#ifdef SAMSUNG_DNG
    m_parameters->setIsUsefulDngInfo(false);
#endif

    /* wait autoFocus is over for turning on preFlash */
    m_autoFocusThread->join();

#ifdef ONE_SECOND_BURST_CAPTURE
    /* For Sync with jpegCallbackThread */
    m_captureLock.lock();
#endif

    seriesShotCount = m_parameters->getSeriesShotCount();
    currentSeriesShotMode = m_parameters->getSeriesShotMode();
    reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    if (m_parameters->getVisionMode() == true) {
        CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
        android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

#ifdef ONE_SECOND_BURST_CAPTURE
        m_captureLock.unlock();
#endif
        return INVALID_OPERATION;
    }

    if (m_parameters->getShotMode() == SHOT_MODE_RICH_TONE) {
        m_hdrEnabled = true;
    } else {
        m_hdrEnabled = false;
    }

    flashMgr->setCaptureStatus(true);

#ifdef ONE_SECOND_BURST_CAPTURE
    TakepictureDurationTimer.stop();
    if (m_parameters->getSamsungCamera()
        && getCameraIdInternal() == CAMERA_ID_BACK
        && m_parameters->getDualMode() == false
        && m_parameters->getRecordingHint() == false
        && m_getRecordingEnabled() == false
        && m_parameters->getEffectHint() == false
#ifdef SAMSUNG_DNG
        && m_parameters->getDNGCaptureModeOn() == false
#endif
        && m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21
        && (currentSeriesShotMode == SERIES_SHOT_MODE_NONE || currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST)) {
        uint64_t delay;
        uint64_t sum;
        delay = TakepictureDurationTimer.durationUsecs();

        /* set smallest delay */
        if (ONE_SECOND_BURST_CAPTURE_TAKEPICTURE_COUNT == 1
            && m_one_second_burst_first_after_open == true)
            delay = 1;

        /* Check other shot(OIS, HDR without companion, FLASH) launching */
#ifdef OIS_CAPTURE
        if(getCameraIdInternal() == CAMERA_ID_BACK && currentSeriesShotMode != SERIES_SHOT_MODE_BURST) {
            sCaptureMgr->resetOISCaptureFcount();
            m_parameters->checkOISCaptureMode();
#ifdef SAMSUNG_LLS_DEBLUR
            m_parameters->checkLDCaptureMode();
#endif
        }
#endif
        /* SR, Burst, LLS is already disable on sendCommand() */
        if (m_hdrEnabled == true
            || m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS
            || m_parameters->getFlashMode() == FLASH_MODE_ON
            || (getCameraIdInternal() == CAMERA_ID_BACK && m_parameters->getOISCaptureModeOn() == true)
            || (flashMgr->getNeedCaptureFlash() == true && currentSeriesShotMode == SERIES_SHOT_MODE_NONE)
            || m_parameters->getCaptureExposureTime() != 0
#ifdef SAMSUNG_LLS_DEBLUR
            || (m_parameters->getLDCaptureMode() > 0)
#endif
            || m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) { // HAL test (Ext_SecCameraActivityTest_testTakePicture_P01)
            /* Check other shot only. reset Capturemode */
            m_parameters->setOISCaptureModeOn(false);
#ifdef SAMSUNG_LLS_DEBLUR
            m_parameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
            m_parameters->setLDCaptureCount(0);
            m_parameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
#endif
            /* For jpegCallbackThreadFunc() exit */
            m_captureLock.unlock();
            m_clearOneSecondBurst(false);
            m_captureLock.lock();
            /* Reset value */
            currentSeriesShotMode = m_parameters->getSeriesShotMode();
            seriesShotCount = m_parameters->getSeriesShotCount();
            takePictureCount = m_takePictureCounter.getCount();
            /* clear delay */
            delay = 0;
        }

        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            if (delay <= ONE_SECOND_BURST_CAPTURE_CHECK_TIME * ONE_MSECOND) {
                sum = 0;
                for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++) {
                    if (TakepictureDurationTime[i] != 0)
                        sum += TakepictureDurationTime[i];

                    if (TakepictureDurationTime[i] == 0) {
                        TakepictureDurationTime[i] = delay;
                        sum += TakepictureDurationTime[i];
                        break;
                    }
                }
                if (sum >= ONE_SECOND_BURST_CAPTURE_CHECK_TIME * ONE_MSECOND) {
                    /* Remove unnecessary time */
                    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1; i++) {
                        TakepictureDurationTime[i] = TakepictureDurationTime[i + 1];
                    }
                    TakepictureDurationTime[ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1] = 0;
                } else {
                    if (TakepictureDurationTime[ONE_SECOND_BURST_CAPTURE_CHECK_COUNT - 1] != 0) {
                        /* do one second burst */
                        CLOGD("DEBUG(%s[%d]): One second burst start - TimeCheck", __FUNCTION__, __LINE__);
                        m_one_second_burst_first_after_open = true;
                        for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++) {
                            TakepictureDurationTime[i] = 0;
                        }
                        /* Set one second burst parameters */
                        m_one_second_burst_capture = true;

                        if (m_burstInitFirst) {
                            m_burstRealloc = true;
                            m_burstInitFirst = false;
                        }
                        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_ONE_SECOND_BURST);
                        /* Set value */
                        currentSeriesShotMode = m_parameters->getSeriesShotMode();
                        seriesShotCount = m_parameters->getSeriesShotCount();
                        m_takePictureCounter.setCount(0);
                        takePictureCount = m_takePictureCounter.getCount();

                        m_burstCaptureCallbackCount = 0;
                        m_parameters->setSeriesShotSaveLocation(BURST_SAVE_CALLBACK);
                        m_jpegCallbackCounter.setCount(seriesShotCount);
#ifdef USE_DVFS_LOCK
                        m_parameters->setDvfsLock(true);
#endif
                        /* Clear saved CallbackHeap */
                        if (m_one_second_jpegCallbackHeap != NULL) {
                            m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                            m_one_second_jpegCallbackHeap = NULL;
                        }
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                            m_one_second_postviewCallbackHeap = NULL;
                        }
                    }
                }
            } else {
                /* Init TakepictureDurationTime. Delay is too big */
                for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
                    TakepictureDurationTime[i] = 0;
            }
        } else if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
            /* More one second burst */
            /* We add 1 to seriesShotCount because we can't know thread state.
	       We will check m_jpegCallbackCounter is smaller than 1 */
            CLOGD("DEBUG(%s[%d]): Continue One second burst - TimeCheck", __FUNCTION__, __LINE__);
            m_reprocessingCounter.setCount(seriesShotCount + 1);
            m_jpegCounter.setCount(seriesShotCount + 1);
            m_pictureCounter.setCount(seriesShotCount + 1);
            m_jpegCallbackCounter.setCount(seriesShotCount);
            /* Make difference m_takePictureCounter value with seriesShotCount */
            m_takePictureCounter.setCount(seriesShotCount - 1);
            takePictureCount = m_takePictureCounter.getCount();
            m_burstCaptureCallbackCount = 0;
            m_one_second_burst_capture = true;

            if (m_prePictureThread->isRunning() == false) {
                if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                    m_captureLock.unlock();
                    return INVALID_OPERATION;
                }
            }
            if (m_pictureThread->isRunning() == false) {
                if (m_pictureThread->run() != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):couldn't run picture thread, ret(%d)", __FUNCTION__, __LINE__, ret);
                    m_captureLock.unlock();
                    return INVALID_OPERATION;
                }
            }
        }
    }
    TakepictureDurationTimer.start();
#endif

#ifdef ONE_SECOND_BURST_CAPTURE
    m_captureLock.unlock();
#endif

    m_parameters->setMarkingOfExifFlash(0);

#ifdef SAMSUNG_LBP
    if (getCameraIdInternal() == CAMERA_ID_FRONT)
        m_parameters->resetNormalBestFrameCount();
#endif

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
#ifdef BURST_CAPTURE
        m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif

        if (m_parameters->isReprocessing() == true)
            m_captureSelector->setIsFirstFrame(true);
        else
            m_sccCaptureSelector->setIsFirstFrame(true);

#if 0
        if (m_parameters->isReprocessing() == false || m_parameters->getSeriesShotCount() > 0 ||
                m_hdrEnabled == true) {
            m_pictureFrameFactory = m_previewFrameFactory;
            if (m_parameters->getUseDynamicScc() == true) {
                if (m_parameters->isOwnScc(getCameraIdInternal()) == true)
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

#ifdef BURST_CAPTURE
#ifdef CAMERA_GED_FEATURE
            if (seriesShotCount > 1)
#else
#ifdef ONE_SECOND_BURST_CAPTURE
            if ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
                 || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST)
                 && m_burstRealloc == true)
#else
            if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST && m_burstRealloc == true)
#endif
#endif
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
#endif
        }

        CLOGD("DEBUG(%s[%d]): currentSeriesShotMode(%d), flashMgr->getNeedCaptureFlash(%d)",
            __FUNCTION__, __LINE__, currentSeriesShotMode, flashMgr->getNeedCaptureFlash());

#ifdef RAWDUMP_CAPTURE
        if(m_use_companion == true) {
            CLOGD("DEBUG(%s[%d]): start set Raw Capture mode", __FUNCTION__, __LINE__);
            sCaptureMgr->resetRawCaptureFcount();
            sCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_RAW);

            m_parameters->setRawCaptureModeOn(true);

            enum aa_capture_intent captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_COMP_BYPASS;

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setControl(STILL_CAPTURE_RAW) fail. ret(%d) intent(%d)",
                __FUNCTION__, __LINE__, ret, captureIntent);
            }
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
        }
#else
#ifdef OIS_CAPTURE
        if (getCameraIdInternal() == CAMERA_ID_BACK
            && m_getRecordingEnabled() == false
#ifdef ONE_SECOND_BURST_CAPTURE
            && currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST
#endif
        ) {
            sCaptureMgr->resetOISCaptureFcount();
            m_parameters->checkOISCaptureMode();
#ifdef SAMSUNG_LLS_DEBLUR
            if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
                m_parameters->checkLDCaptureMode();
                CLOGD("LLS_MBR(%s[%d]): currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                        __FUNCTION__, __LINE__, currentSeriesShotMode, m_parameters->getLDCaptureMode());
            }
#endif

#ifdef SAMSUNG_LENS_DC
            if (m_parameters->getLensDCMode()) {
                m_setLensDCMap();
            }

            m_parameters->setLensDCEnable(false);
            if (m_parameters->getLensDCMode()
                && !m_skipLensDC
                && m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21
                && m_parameters->getSeriesShotCount() == 0
                && m_parameters->getZoomLevel() == ZOOM_LEVEL_0
#ifdef SAMSUNG_DNG
                && !m_parameters->getDNGCaptureModeOn()
#endif
            ) {
                m_parameters->setOutPutFormatNV21Enable(true);
                m_parameters->setLensDCEnable(true);
                CLOGD("[DC](%s[%d]): Set Lens Distortion Correction, getLensDCEnable(%d)",
                        __FUNCTION__, __LINE__, m_parameters->getLensDCEnable());
            }
#endif
        }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
        if (getCameraIdInternal() == CAMERA_ID_FRONT
            && currentSeriesShotMode == SERIES_SHOT_MODE_NONE
            && m_getRecordingEnabled() == false
            && isLDCapture(getCameraIdInternal()) == true) {
            m_parameters->checkLDCaptureMode();
            CLOGD("LLS_MBR(%s[%d]): currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                    __FUNCTION__, __LINE__, currentSeriesShotMode, m_parameters->getLDCaptureMode());
        }
#endif

        if (m_parameters->getCaptureExposureTime() != 0) {
            m_longExposureRemainCount = m_parameters->getLongExposureShotCount();
            CLOGD("(%s[%d]): m_longExposureRemainCount(%d)",
                __FUNCTION__, __LINE__, m_longExposureRemainCount);
        }

#ifdef SAMSUNG_LBP
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            uint32_t refBestPicCount = 0;
            if(getCameraIdInternal() == CAMERA_ID_FRONT) {
                if (m_parameters->getSCPFrameCount() >= m_parameters->getBayerFrameCount()) {
                    refBestPicCount = m_parameters->getSCPFrameCount() + LBP_CAPTURE_DELAY;
                } else {
                    refBestPicCount = m_parameters->getBayerFrameCount() + LBP_CAPTURE_DELAY;
                }
            }

            if(getCameraIdInternal() == CAMERA_ID_FRONT) {
                if(m_isLBPlux == true) {
                    m_parameters->setNormalBestFrameCount(refBestPicCount);
                    m_captureSelector->setFrameIndex(refBestPicCount);

                    ret = m_captureSelector->setFrameHoldCount(m_parameters->getHoldFrameCount());
                    m_isLBPon = true;
                } else {
                    m_isLBPon = false;
                }
            } else {
#ifndef SAMSUNG_LLS_DEBLUR
                if (m_isLBPlux == true && m_parameters->getOISCaptureModeOn() == true
#ifdef SAMSUNG_DNG
                    && m_parameters->getDNGCaptureModeOn() == false
#endif
                    && m_getRecordingEnabled() == false) {
                    ret = m_captureSelector->setFrameHoldCount(m_parameters->getHoldFrameCount());
                    m_isLBPon = true;
                } else
#endif
                {
                    m_isLBPon = false;
                }
            }
        }
#endif
#ifdef SAMSUNG_JQ
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE) {
            CLOGD("[JQ](%s[%d]): ON!!", __FUNCTION__, __LINE__);
            m_isJpegQtableOn = true;
            m_parameters->setJpegQtableOn(true);
        }
        else {
            CLOGD("[JQ](%s[%d]): OFF!!", __FUNCTION__, __LINE__);
            m_isJpegQtableOn = false;
            m_parameters->setJpegQtableOn(false);
        }
#endif

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

#ifdef SAMSUNG_FRONT_LCD_FLASH
            if (getCameraIdInternal() == CAMERA_ID_FRONT) {
                enum ExynosCameraActivityFlash::FLASH_STEP curFlashStep;
                flashMgr->getFlashStep(&curFlashStep);

                if (curFlashStep == ExynosCameraActivityFlash::FLASH_STEP_PRE_LCD_ON) {
                    flashMgr->resetShotFcount();
                    flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_LCD_ON);
                }
            }
            else
#endif
            {
                if (flashMgr->checkPreFlash() == false && m_isTryStopFlash == false) {
                    CLOGD("DEBUG(%s[%d]): checkPreFlash(false), Start auto focus internally", __FUNCTION__, __LINE__);
                    m_autoFocusType = AUTO_FOCUS_HAL;
                    flashMgr->setFlashTrigerPath(ExynosCameraActivityFlash::FLASH_TRIGGER_SHORT_BUTTON);
                    flashMgr->setFlashWaitCancel(false);

                    /* execute autoFocus for preFlash */
                    m_autoFocusThread->requestExitAndWait();
                    m_autoFocusThread->run(PRIORITY_DEFAULT);
                }
            }
        }
#ifdef OIS_CAPTURE
        else if (m_parameters->getOISCaptureModeOn() == true
                 && getCameraIdInternal() == CAMERA_ID_BACK) {
            CLOGD("DEBUG(%s[%d]): start set zsl-like mode", __FUNCTION__, __LINE__);

            unsigned int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;
            unsigned int captureCount = 0;
            int value;
#ifdef SAMSUNG_LBP
            sCaptureMgr->setBestMultiCaptureMode(false);
#endif

            if (m_parameters->getSeriesShotCount() > 0
#ifdef LLS_REPROCESSING
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS
#endif
            ) {
                /* BURST */
                sCaptureMgr->setMultiCaptureMode(true);
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI;
                value = captureIntent;
            }
#ifdef SAMSUNG_LBP
            else if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_isLBPon == true
#ifdef LLS_REPROCESSING
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS)
#endif
            {
                /* BEST PICK */
                sCaptureMgr->setMultiCaptureMode(true);
                sCaptureMgr->setBestMultiCaptureMode(true);
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_BEST;
                value = captureIntent;
            }
#endif
            else {
                /* SINGLE */
#ifdef SAMSUNG_LLS_DEBLUR
                if (m_parameters->getLDCaptureMode() > 0) {
                    captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT;
                    captureCount = m_parameters->getLDCaptureCount();
                    unsigned int mask = 0;

                    if (m_parameters->getLDCaptureLLSValue() >= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2
                        && m_parameters->getLDCaptureLLSValue() <= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4) {
                        captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT;
                    } else {
                        captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT;
                    }

                    mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
                    value = mask;

                    CLOGD("DEBUG(%s[%d]): start set multi mode captureIntent(%d)", __FUNCTION__, __LINE__,captureIntent);
                } else
#endif
                {
                    captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;
                    value = captureIntent;
                }
            }

#ifdef USE_ULTRA_FAST_SHOT
            if (!sCaptureMgr->getMultiCaptureMode()) {
                int ret = 0;
                struct v4l2_ext_controls extCtrls;
                struct v4l2_ext_control extCtrl;
                struct fast_ctl_capture captureCtrl;
                memset(&extCtrls, 0x00, sizeof(extCtrls));
                memset(&extCtrl, 0x00, sizeof(extCtrl));
                memset(&captureCtrl, 0x00, sizeof(captureCtrl));

                captureCtrl.capture_intent = (uint32_t)captureIntent;
                captureCtrl.capture_count = (uint32_t)captureCount;
                captureCtrl.capture_exposureTime = (uint32_t)0;
                captureCtrl.ready = (uint32_t)0;

                extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
                extCtrls.count = 1;

                extCtrl.id = V4L2_CID_IS_FAST_CAPTURE_CONTROL;
                extCtrl.ptr = &captureCtrl;
                extCtrls.controls = &extCtrl;

                ALOGD("(%s[%d])setExtControl intent:%d, count:%d, exposuretime:%d", __FUNCTION__, __LINE__,
                        captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);
                
                ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
                if (ret != NO_ERROR) {
                    ALOGE("ERR(%s[%d]):Failed to setExtControl to 3AA",
                            __FUNCTION__, __LINE__);
                }
            } else
#endif
            {
                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setControl(V4L2_CID_IS_INTENT) fail. ret(%d) intent(%d)",
                            __FUNCTION__, __LINE__, ret, value);
                }
            }

            if(m_parameters->getSeriesShotCount() == 0) {
                m_OISCaptureShutterEnabled = true;
            }

            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            m_exynosCameraActivityControl->setOISCaptureMode(true);
        }
#endif
        else {
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
            if (m_parameters->getCaptureExposureTime() > PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX) {
                unsigned int captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT;
                unsigned int captureCount = 1 + m_parameters->getLongExposureShotCount();
                unsigned int mask = 0;
                int captureExposureTime = m_parameters->getCaptureExposureTime();
                int value;

#ifdef USE_ULTRA_FAST_SHOT
                int ret = 0;
                struct v4l2_ext_controls extCtrls;
                struct v4l2_ext_control extCtrl;
                struct fast_ctl_capture captureCtrl;
                memset(&extCtrls, 0x00, sizeof(extCtrls));
                memset(&extCtrl, 0x00, sizeof(extCtrl));
                memset(&captureCtrl, 0x00, sizeof(captureCtrl));

                captureCtrl.capture_intent = (uint32_t)captureIntent;
                captureCtrl.capture_count = (uint32_t)captureCount;
                captureCtrl.capture_exposureTime = (uint32_t)captureExposureTime;
                captureCtrl.ready = (uint32_t)0;

                extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
                extCtrls.count = 1;

                extCtrl.id = V4L2_CID_IS_FAST_CAPTURE_CONTROL;
                extCtrl.ptr = &captureCtrl;
                extCtrls.controls = &extCtrl;

                ALOGD("(%s[%d])setExtControl intent:%d, count:%d, exposuretime:%d", __FUNCTION__, __LINE__,
                    captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);

                ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
                if (ret != NO_ERROR) {
                    ALOGE("ERR(%s[%d]):Failed to setExtControl to 3AA",
                            __FUNCTION__, __LINE__);
                }
#else
                mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
                value = mask;

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_CAPTURE_EXPOSURETIME, captureExposureTime, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setControl(V4L2_CID_IS_CAPTURE_EXPOSURETIME) fail. ret(%d) intent(%d)",
                            __FUNCTION__, __LINE__, ret, captureExposureTime);
                }

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setControl(V4L2_CID_IS_INTENT) fail. ret(%d) intent(%d)",
                            __FUNCTION__, __LINE__, ret, value);
                }
#endif

                m_stopLongExposure = false;

                m_parameters->setOISCaptureModeOn(true);
                sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
                m_exynosCameraActivityControl->setOISCaptureMode(true);
            } else
#endif
            {
                if(m_parameters->getCaptureExposureTime() > 0) {
                    m_stopLongExposure = false;
                    m_parameters->setExposureTime();
                }
            }
        }

#ifdef SET_LLS_CAPTURE_SETFILE
        if(getCameraIdInternal() == CAMERA_ID_BACK && currentSeriesShotMode == SERIES_SHOT_MODE_LLS
#ifdef SR_CAPTURE
            && m_parameters->getSROn() == false
#endif
        ) {
            CLOGD("DEBUG(%s[%d]): set LLS Capture mode on", __FUNCTION__, __LINE__);
            m_parameters->setLLSCaptureOn(true);
        }
#endif
#endif /* RAWDUMP_CAPTURE */

#ifdef SAMSUNG_LBP
        if(currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
            int TotalBufferNum = (int)m_parameters->getHoldFrameCount();
            ret = uni_plugin_set(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &TotalBufferNum);
            if(ret < 0) {
                CLOGE("[LBP](%s[%d]):Bestphoto plugin set failed!!", __FUNCTION__, __LINE__);
            }
            ret = uni_plugin_init(m_LBPpluginHandle);
            if(ret < 0) {
                CLOGE("[LBP](%s[%d]):Bestphoto plugin init failed!!", __FUNCTION__, __LINE__);
            }

            m_LBPThread->run(PRIORITY_DEFAULT);
        }
#endif

#ifndef RAWDUMP_CAPTURE
        if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE
#ifdef OIS_CAPTURE
            && m_parameters->getOISCaptureModeOn() == false
#endif
#ifdef SAMSUNG_LBP
            && !m_isLBPon
#endif
            && flashMgr->getNeedCaptureFlash() == false
            && m_parameters->getCaptureExposureTime() == 0) {
            m_isZSLCaptureOn = true;
        }
#endif

#ifndef RAWDUMP_CAPTURE
        if (m_parameters->getSamsungCamera()
#ifdef SAMSUNG_LLS_DEBLUR
            && !m_parameters->getLDCaptureMode()
#endif
#ifdef SAMSUNG_LENS_DC
            && !m_parameters->getLensDCEnable()
#endif
           ) {
            int thumbnailW = 0, thumbnailH = 0;
            m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);

            if ((thumbnailW > 0 && thumbnailH > 0)
                && !m_parameters->getRecordingHint()
                && !m_parameters->getDualMode()
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS
#ifdef SAMSUNG_MAGICSHOT
                && m_parameters->getShotMode() != SHOT_MODE_MAGIC
#endif
                && m_parameters->getShotMode() != SHOT_MODE_OUTFOCUS
                && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                m_parameters->setIsThumbnailCallbackOn(true);
                CLOGI("INFO(%s[%d]):m_isThumbnailCallbackOn(%d)", __FUNCTION__, __LINE__, m_parameters->getIsThumbnailCallbackOn());
            }
        }
#endif

        m_parameters->setSetfileYuvRange();

        if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21) {
            m_parameters->setOutPutFormatNV21Enable(true);
        }

#ifdef SAMSUNG_LLS_DEBLUR
        int pictureCount = m_parameters->getLDCaptureCount();

        if (m_parameters->getLDCaptureMode())
            m_reprocessingCounter.setCount(pictureCount);
        else
#endif
            m_reprocessingCounter.setCount(seriesShotCount);

        if (m_prePictureThread->isRunning() == false) {
            if (m_prePictureThread->run(PRIORITY_DEFAULT) != NO_ERROR) {
                CLOGE("ERR(%s[%d]):couldn't run pre-picture thread", __FUNCTION__, __LINE__);
                return INVALID_OPERATION;
            }
        }

#ifdef SAMSUNG_LLS_DEBLUR
        if (m_parameters->getLDCaptureMode()) {
            m_jpegCounter.setCount(seriesShotCount);
            m_pictureCounter.setCount(pictureCount);
            m_yuvcallbackCounter.setCount(seriesShotCount);

            CLOGD("LLS_MBR(%s[%d]): pictureCount(%d), getLDCaptureMode(%d)",
                    __FUNCTION__, __LINE__, pictureCount, m_parameters->getLDCaptureMode());
        } else
#endif
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
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21 ||
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
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
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21 ||
#ifdef ONE_SECOND_BURST_CAPTURE
                currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
#endif
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
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
        if (m_parameters->isReprocessing() == false)
            m_pictureEnabled = true;
    }

    return NO_ERROR;
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
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
#ifdef OIS_CAPTURE
        ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        if (m_parameters->getOISCaptureModeOn()) {
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_END);
            m_exynosCameraActivityControl->setOISCaptureMode(false);
            m_parameters->setOISCaptureModeOn(false);
        }
#endif

#ifdef USE_ULTRA_FAST_SHOT
        int ret = 0;
        struct v4l2_ext_controls extCtrls;
        struct v4l2_ext_control extCtrl;
        struct fast_ctl_capture captureCtrl;
        memset(&extCtrls, 0x00, sizeof(extCtrls));
        memset(&extCtrl, 0x00, sizeof(extCtrl));
        memset(&captureCtrl, 0x00, sizeof(captureCtrl));

        captureCtrl.capture_intent = (uint32_t)AA_CAPTURE_INTENT_STILL_CAPTURE_CANCEL;
        captureCtrl.capture_count = (uint32_t)0;
        captureCtrl.capture_exposureTime = (uint32_t)0;
        captureCtrl.ready = (uint32_t)0;

        extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
        extCtrls.count = 1;

        extCtrl.id = V4L2_CID_IS_FAST_CAPTURE_CONTROL;
        extCtrl.ptr = &captureCtrl;
        extCtrls.controls = &extCtrl;

        ALOGD("(%s[%d])setExtControl intent:%d, count:%d, exposuretime:%d", __FUNCTION__, __LINE__,
                captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);

        ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
        if (ret != NO_ERROR) {
            ALOGE("ERR(%s[%d]):Failed to setExtControl to 3AA",
                __FUNCTION__, __LINE__);
        }
#endif

#else
        m_parameters->setPreviewExposureTime();
#endif
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
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW("WRN(%s[%d]): Vision mode does not support", __FUNCTION__, __LINE__);
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF][%s][%d] Out-focus shot parameter is not reset. Reset here forcely!!: %d",
                __FUNCTION__, __LINE__, m_lensmoveCount);
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setStartLensMove(false);
        m_lensmoveCount = 0;
        m_parameters->setMoveLensCount(m_lensmoveCount);
        m_parameters->setMoveLensTotal(m_lensmoveCount);
    }
#endif

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
    status_t ret = NO_ERROR;
	bool flagRestart = false;

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if(m_parameters == NULL)
        return INVALID_OPERATION;

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_isQuickSwitchState == QUICK_SWITCH_STBY) {
        m_use_companion = true;
        m_parameters->setUseCompanion(true);
    }
#endif

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
#ifdef SUPPORT_SW_VDIS
        && (!m_swVDIS_Mode)
#endif /*SUPPORT_SW_VDIS*/
#ifdef SAMSUNG_HYPER_MOTION
        && (m_parameters->getHyperMotionMode() == false)
#endif /*SAMSUNG_HYPER_MOTION*/
        ) {
            ret = m_restartPreviewInternal(true, (CameraParameters*)&params);
        }

        m_resetPreview = false;
        CLOGI("INFO(%s[%d]) m_resetPreview(%d)", __FUNCTION__, __LINE__, m_resetPreview);

        if (ret < 0)
            CLOGE("(%s[%d]): restart preview internal fail", __FUNCTION__, __LINE__);
    } else {
        ret = m_parameters->setParameters(params);
    }

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_isFirstParametersSet == false
        && m_parameters->getDualMode() == false
        && m_parameters->getSamsungCamera() == true
        && m_parameters->getVisionMode() == false
#ifdef SAMSUNG_QUICK_SWITCH
        && (m_isQuickSwitchState == QUICK_SWITCH_IDLE
        && m_use_companion == true)
#endif
        ) {
        CLOGD("DEBUG(%s[%d]):1st setParameters", __FUNCTION__, __LINE__);
        m_fastEntrance = true;
        m_isFirstParametersSet = true;
        m_fastenAeThread->run();
    }
#endif

    return ret;
}

status_t ExynosCamera::sendCommand(int32_t command, int32_t arg1, __unused int32_t arg2)
{
    ExynosCameraActivityUCTL *uctlMgr = NULL;
#if defined(SAMSUNG_DOF) || defined(SAMSUNG_OT)
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
#endif
#ifdef SAMSUNG_OIS
    int zoomLevel;
    int zoomRatio;
#endif

    CLOGV("INFO(%s[%d])", __FUNCTION__, __LINE__);

    if (m_parameters != NULL) {
#ifdef SAMSUNG_OIS
        zoomLevel = m_parameters->getZoomLevel();
        zoomRatio = m_parameters->getZoomRatio(zoomLevel);
#endif
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
#ifdef SAMSUNG_OT
    case 1504: /* HAL_OBJECT_TRACKING_STARTSTOP */
        if (m_OTpluginHandle == NULL) {
            CLOGE("[OBTR](%s[%d]):HAL_OBJECT_TRACKING_STARTSTOP is called but handle is null !!", __FUNCTION__, __LINE__);
            break;
        }
        CLOGD("DEBUG(%s):HAL_OBJECT_TRACKING_STARTSTOP(%d) is called!", __FUNCTION__, arg1);
        if (arg1) {
            /* Start case */
            int ret = 0;
            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

            ret = uni_plugin_init(m_OTpluginHandle);
            if (ret < 0) {
                CLOGE("[OBTR](%s[%d]):Object Tracking plugin init failed!!", __FUNCTION__, __LINE__);
            }
            ret = uni_plugin_set(m_OTpluginHandle,
                          OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[OBTR](%s[%d]): Object Tracking Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!", __FUNCTION__, __LINE__);
            }
            autoFocusMgr->setObjectTrackingStart(true);
            m_OTstart = OBJECT_TRACKING_INIT;
        }
        else {
            /* Stop case */
            autoFocusMgr->setObjectTrackingStart(false);
            m_OTstart = OBJECT_TRACKING_DEINIT;
            m_OTstatus = UNI_PLUGIN_OBJECT_TRACKING_IDLE;
            m_OTisTouchAreaGet = false;
            m_parameters->setObjectTrackingGet(false);
            m_clearOTmeta();
            /* Waiting here in the recording mode can make preview thread stop */
#if 0
            if(m_OTstart != OBJECT_TRACKING_IDLE) {
                m_OT_mutex.lock();
                m_OTisWait = true;
                m_OT_condition.waitRelative(m_OT_mutex, 1000000000); /* 1sec */
                m_OTisWait = false;
                m_OT_mutex.unlock();
            }
            CLOGD("DEBUG(%s):HAL_OBJECT_TRACKING_STARTSTOP(%d) Out is called!", __FUNCTION__, arg1);
#endif
        }
        break;
#endif
#ifdef BURST_CAPTURE
    case 1571: /* HAL_CMD_RUN_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_RUN_BURST_TAKE is called!", __FUNCTION__);
#ifdef ONE_SECOND_BURST_CAPTURE
        m_clearOneSecondBurst(false);
#endif

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
#ifdef SAMSUNG_BD
        m_BDbufferIndex = 0;
        if (m_BDpluginHandle != NULL && m_BDstatus != BLUR_DETECTION_INIT) {
            int ret = uni_plugin_init(m_BDpluginHandle);
            if (ret < 0)
                CLOGE("[BD](%s[%d]): Plugin init failed!!", __FUNCTION__, __LINE__);

            for (int i = 0; i < MAX_BD_BUFF_NUM; i++) {
                m_BDbuffer[i].data = new unsigned char[BD_EXIF_SIZE];
            }
        }
        m_BDstatus = BLUR_DETECTION_INIT;
#endif

#ifdef USE_DVFS_LOCK
        m_parameters->setDvfsLock(true);
#endif
        break;
    case 1572: /* HAL_CMD_STOP_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_STOP_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);

#ifdef OIS_CAPTURE
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            if (m_parameters->getOISCaptureModeOn()) {
                /* setMultiCaptureMode flag will be disabled in SCAPTURE_STEP_END in case of OIS capture */
                m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
                m_parameters->setOISCaptureModeOn(false);
            } else {
                m_sCaptureMgr->setMultiCaptureMode(false);
            }
        }
#endif

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

#ifdef SAMSUNG_BD
        m_BDbufferQ->release();

        if(m_BDpluginHandle != NULL && m_BDstatus == BLUR_DETECTION_INIT) {
            int ret = uni_plugin_deinit(m_BDpluginHandle);
            if (ret < 0)
                CLOGE("[BD](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);

            for (int i = 0; i < MAX_BD_BUFF_NUM; i++) {
                if (m_BDbuffer[i].data != NULL)
                    delete []m_BDbuffer[i].data;
            }
        }
        m_BDstatus = BLUR_DETECTION_DEINIT;
#endif

#ifdef USE_DVFS_LOCK
        if (m_parameters->getDvfsLock() == true)
            m_parameters->setDvfsLock(false);
#endif
        break;
    case 1573: /* HAL_CMD_DELETE_BURST_TAKE */
        CLOGD("DEBUG(%s):HAL_CMD_DELETE_BURST_TAKE is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
#ifdef VARIABLE_BURST_FPS
    case 1575: /* HAL_CMD_SET_BURST_FPS */
        CLOGD("DEBUG(%s):HAL_CMD_SET_BURST_FPS is called! / FPS(%d)", __FUNCTION__, arg1);
        m_parameters->setBurstShotDuration(arg1);
        break;
#endif /* VARIABLE_BURST_FPS */
#endif
#ifdef LLS_CAPTURE
    case 1600: /* CAMERA_CMD_START_BURST_PANORAMA */
        CLOGD("DEBUG(%s):CAMERA_CMD_START_BURST_PANORAMA is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
    case 1601: /*CAMERA_CMD_STOP_BURST_PANORAMA */
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_BURST_PANORAMA is called!", __FUNCTION__);
        m_takePictureCounter.setCount(0);
        break;
    case 1516: /*CAMERA_CMD_START_SERIES_CAPTURE */
        CLOGD("DEBUG(%s):CAMERA_CMD_START_SERIES_CAPTURE is called!%d", __FUNCTION__, arg1);
#ifdef ONE_SECOND_BURST_CAPTURE
        m_clearOneSecondBurst(false);
#endif
        m_parameters->setOutPutFormatNV21Enable(true);
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_LLS, arg1);
        m_takePictureCounter.setCount(0);
        m_yuvcallbackCounter.setCount(0);
        break;
    case 1517: /* CAMERA_CMD_STOP_SERIES_CAPTURE */
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_SERIES_CAPTURE is called!", __FUNCTION__);
        m_parameters->setOutPutFormatNV21Enable(false);
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        m_takePictureCounter.setCount(0);
#ifdef SET_LLS_CAPTURE_SETFILE
        m_parameters->setLLSCaptureOn(false);
#endif
#ifdef OIS_CAPTURE
        if (getCameraIdInternal() == CAMERA_ID_BACK
            && m_parameters->getOISCaptureModeOn() == false) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setMultiCaptureMode(false);
        }
#endif
        break;
#endif
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        CLOGD("DEBUG(%s):CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            if( m_flagLLSStart != true) {
                m_flagLLSStart = true;
#ifdef LLS_CAPTURE
                m_parameters->setLLSOn(m_flagLLSStart);
                m_parameters->m_setLLSShotMode();
#endif
            }
        } else {
            m_flagLLSStart = false;
#ifdef LLS_CAPTURE
            m_parameters->setLLSOn(m_flagLLSStart);
            m_parameters->m_setShotMode(m_parameters->getShotMode());
#endif
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
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    case 1802: /* HAL_ENABLE_FLASH_AUTO_CALLBACK*/
        CLOGD("DEBUG(%s):HAL_ENABLE_FLASH_AUTO_CALLBACK is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagFlashCallback = true;
        } else {
            m_flagFlashCallback = false;
        }
        break;
    case 1803: /* HAL_ENABLE_HDR_AUTO_CALLBACK*/
        CLOGD("DEBUG(%s):HAL_ENABLE_HDR_AUTO_CALLBACK is called!%d", __FUNCTION__, arg1);
        if(arg1) {
            m_flagHdrCallback = true;
        } else {
            m_flagHdrCallback = false;
        }
        break;
#endif
#ifdef SAMSUNG_FRONT_LCD_FLASH
    case 1805: /* LCD_FLASH */
        {
            ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();

            CLOGD("DEBUG(%s):LCD_FLASH is called!%d", __FUNCTION__, arg1);
            if (arg1) {
                setHighBrightnessModeOfLCD(1, m_prevHBM, m_prevAutoHBM);
                flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_PRE_LCD_ON);
            } else {
                flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_LCD_OFF);
                setHighBrightnessModeOfLCD(0, m_prevHBM, m_prevAutoHBM);
            }
        }
        break;
#endif
    case 1431: /* CAMERA_CMD_START_MOTION_PHOTO */
        CLOGD("DEBUG(%s):CAMERA_CMD_START_MOTION_PHOTO is called!", __FUNCTION__);
        m_parameters->setMotionPhotoOn(true);
        break;
    case 1432: /* CAMERA_CMD_STOP_MOTION_PHOTO */
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_MOTION_PHOTO is called!)", __FUNCTION__);
        m_parameters->setMotionPhotoOn(false);
        break;
#ifdef SR_CAPTURE
    case 1519: /* CAMERA_CMD_SUPER_RESOLUTION_MODE */
        CLOGD("DEBUG(%s):CAMERA_CMD_SUPER_RESOLUTION_MODE is called!%d", __FUNCTION__, arg1);
        m_parameters->setSROn(arg1);
        if (arg1) {
            m_sr_cropped_width = 0;
            m_sr_cropped_height = 0;
            m_isCopySrMdeta = true;
            memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
            memset(&m_sr_frameMetadata, 0, sizeof(camera_frame_metadata_t));
        }
        break;
#endif
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
    /*1643: HAL_ENABLE_CURRENT_SET */
    case 1643:
        CLOGD("DEBUG(%s):HAL_ENABLE_CURRENT_SET is called!%d", __FUNCTION__, arg1);
        if(arg1)
            m_startCurrentSet();
        else
            m_stopCurrentSet();
        break;
#ifdef SAMSUNG_DOF
    /*1644: HAL_ENABLE_PAF_RESULT */
    case 1644:
        CLOGD("DEBUG(%s):HAL_ENABLE_PAF_RESULT is called!%d", __FUNCTION__, arg1);
        break;
    /*1650: CAMERA_CMD_START_PAF_CALLBACK */
    case 1650:
        CLOGD("DEBUG(%s):CAMERA_CMD_START_PAF_CALLBACK is called!%d", __FUNCTION__, arg1);
        autoFocusMgr->setPDAF(true);
        break;
    /*1651: CAMERA_CMD_STOP_PAF_CALLBACK */
    case 1651:
        CLOGD("DEBUG(%s):CAMERA_CMD_STOP_PAF_CALLBACK is called!%d", __FUNCTION__, arg1);
        autoFocusMgr->setPDAF(false);
        break;
#endif
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
#ifdef SAMSUNG_OIS
        m_parameters->setOISMode();
#endif
        break;
#ifdef SAMSUNG_HLV
    case 1721:  /* HAL_RECORDING_RESUME */
        {
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME;
            ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME");

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                ALOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;

    case 1722: /* HAL_RECORDING_PAUSE */
        {
            ALOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE");
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE;

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                ALOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;
#endif
#ifdef USE_LIVE_BROADCAST
    case 1730: /* SET PLB mode */
        CLOGD("DEBUG(%s):SET PLB mode is called!%d", __FUNCTION__, arg1);
        m_parameters->setPLBMode((bool)arg1);
        break;
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    case 1811: /* HAL_PREPARE_PREVIEW */
        CLOGD("DEBUG(%s): PREPARE PREVIEW (%d/%d)", __FUNCTION__, arg1, arg2);
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_IDLE_TO_STBY);
        break;
    case 1812: /* HAL_STANDBY_PREVIEW */
        CLOGD("DEBUG(%s): STANDBY PREVIEW (%d/%d)", __FUNCTION__, arg1, arg2);
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_MAIN_TO_STBY);
        break;
#endif
    case 1821: /* SAMSUNG NATIVE APP Based on SecCamera*/
        CLOGD("DEBUG(%s): SAMSUNG NATIVE APP ON!! (%d/%d)", __FUNCTION__, arg1, arg2);
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
    bool skipPreview = false, skipFdMetaCallback = false;
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
#ifdef SAMSUNG_LBP
    unsigned int LBPframeCount = 0;
#endif

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;
#ifdef SUPPORT_SW_VDIS
    int swVDIS_BufIndex = -1;
#endif
#ifdef SAMSUNG_HYPER_MOTION
    int hyperMotion_BufIndex = -1;
#endif

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
            ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):manageFrameHoldList fail", __FUNCTION__, __LINE__);
                return ret;
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
            && m_previewFrameFactory->getRunningFrameCount(pipeId) == 0) {
            m_previewFrameFactory->stopThread(pipeId);
        }

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
            m_previewFrameFactory->stopThread(pipeId);

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
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        /* no break */

        if (m_parameters->isFusionEnabled() == true) {
            CLOGV("INFO(%s[%d]): run fusion pipeId(%d) frame(%d)", __FUNCTION__, __LINE__, entity->getPipeId(), frame->getFrameCount());
            m_fusionQ->pushProcessQ(&frame);
            break;
        }

    case PIPE_FUSION:
        if (m_parameters->isFusionEnabled() == true) {
            ret = m_syncPreviewWithFusion(pipeId, frame);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_syncPreviewWithFusion() fail, pipeId(%d), ret(%d) frame(%d)",
                    __FUNCTION__, __LINE__, pipeId, ret, frame->getFrameCount());
            }
        }
#endif

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

            if (m_fdFrameSkipCount > 0) {
                m_fdFrameSkipCount--;
            }

            /* Face detection */
            if (!(m_parameters->getHighSpeedRecording() == true && m_getRecordingEnabled() == true)
                && (frame->getFrameState() != FRAME_STATE_SKIPPED)) {
#ifdef SR_CAPTURE
                if(m_isCopySrMdeta) {
                    frame->getDynamicMeta(&m_srShotMeta);
                    m_isCopySrMdeta = false;
                    CLOGI("INFO(%s[%d]) copy SR FdMeta", __FUNCTION__, __LINE__);
                }
#endif

                if (m_parameters->getFrameSkipCount() <= 0
                    && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
                    frame->getFpsRange(&minFps, &maxFps);

                    /* limit maximum FD callback fps to 60fps */
                    if (maxFps > 60) {
                        ratio = (int)((maxFps * 10 / 60) / 10);
                        m_fdCallbackToggle = (m_fdCallbackToggle + 1) % ratio;
                        skipFdMetaCallback = (m_fdCallbackToggle == 0) ? false : true;
                    }

                    if (!skipFdMetaCallback) {
#ifdef SUPPORT_DEPTH_MAP
                        int pipe = PIPE_VC1;
                        fdFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipe, frame->getFrameCount());
#else
                        fdFrame = m_frameMgr->createFrame(m_parameters, frame->getFrameCount());
#endif
                        m_copyMetaFrameToFrame(frame, fdFrame, true, true);
                        m_facedetectQ->pushProcessQ(&fdFrame);
                    }
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
#ifdef SAMSUNG_LBP
                    if (m_parameters->getUsePureBayerReprocessing() == true) {
                        camera2_shot_ext *shot_ext = NULL;
                        shot_ext = (camera2_shot_ext *)(buffer.addr[1]);

                        if (shot_ext != NULL)
                            LBPframeCount = shot_ext->shot.dm.request.frameCount;
                        else
                            ALOGE("ERR(%s[%d]):Buffer is null", __FUNCTION__, __LINE__);
                    } else {
                        camera2_stream *lbp_shot_stream = NULL;
                        lbp_shot_stream = (struct camera2_stream *)buffer.addr[planeCount];
                        getStreamFrameCount(lbp_shot_stream, &LBPframeCount);
                    }
                    if(getCameraIdInternal() == CAMERA_ID_FRONT)
                        m_parameters->setSCPFrameCount(LBPframeCount);
#endif
#ifdef SAMSUNG_JQ
                    if(m_isJpegQtableOn == true
#ifdef SAMSUNG_LBP
                       && m_isLBPon == false
#endif
                       ) {
                        m_isJpegQtableOn = false;
                        ret = m_processJpegQtable(&buffer);
                        if (ret < 0) {
                            CLOGE("[JQ](%s[%d]): m_processJpegQtable() failed!!", __FUNCTION__, __LINE__);
                        }
                    }
#endif
#ifdef SAMSUNG_LLV
                    if (m_LLVstatus == LLV_INIT) {
                        int HwPreviewW = 0, HwPreviewH = 0;
                        m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
                        UniPluginBufferData_t pluginData;
                        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
                        int powerCtrl = UNI_PLUGIN_POWER_CONTROL_OFF;
                        float Low_Lux = -5.5;
                        float High_Lux = -3.5;
                        float Low_Lux_Front = -5.2;
                        float High_Lux_Front = -3.2;
                        float LLV_Lux = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256.0;

                        pluginData.InBuffY = buffer.addr[0];
                        pluginData.InBuffU = buffer.addr[1];

                        pluginData.InWidth = HwPreviewW;
                        pluginData.InHeight = HwPreviewH;
                        pluginData.BrightnessLux = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[5];

                        if (getCameraIdInternal() == CAMERA_ID_FRONT) {
                            if ((LLV_Lux > Low_Lux_Front) && (LLV_Lux <= High_Lux_Front))
                                powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
                        }
                        else if (getCameraIdInternal() == CAMERA_ID_BACK){
                            if ((LLV_Lux > Low_Lux) && (LLV_Lux <= High_Lux))
                                powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
                        }

#ifdef SAMSUNG_LLV_TUNING
                        powerCtrl = m_LLVpowerLevel;
                        CLOGD("[LLV_TEST] powerCtrl: %d", powerCtrl);
#endif
                        if(m_LLVpluginHandle != NULL) {
                            uni_plugin_set(m_LLVpluginHandle,
                                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
                            uni_plugin_set(m_LLVpluginHandle,
                                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
                            uni_plugin_process(m_LLVpluginHandle);
                        }
                    }
                    else if (m_LLVstatus == LLV_STOPPED) {
                        if(m_LLVpluginHandle != NULL) {
                            uni_plugin_deinit(m_LLVpluginHandle);
                        }
                        m_LLVstatus = LLV_UNINIT;
                    }
#endif

#ifdef SAMSUNG_OT
                    if (m_parameters->getObjectTrackingChanged() == true
                            && m_OTstatus == UNI_PLUGIN_OBJECT_TRACKING_IDLE && m_OTstart == OBJECT_TRACKING_INIT) {
                        int maxNumFocusAreas = m_parameters->getMaxNumFocusAreas();
                        ExynosRect2* objectTrackingArea = new ExynosRect2[maxNumFocusAreas];
                        int* objectTrackingWeight = new int[maxNumFocusAreas];
                        int validNumber;
                        UniPluginFocusData_t focusData;
                        memset(&focusData, 0, sizeof(UniPluginFocusData_t));

                        m_parameters->getObjectTrackingAreas(&validNumber, objectTrackingArea, objectTrackingWeight);

                        for (int i = 0; i < validNumber; i++) {
                            focusData.FocusROILeft = objectTrackingArea[i].x1;
                            focusData.FocusROIRight = objectTrackingArea[i].x2;
                            focusData.FocusROITop = objectTrackingArea[i].y1;
                            focusData.FocusROIBottom = objectTrackingArea[i].y2;
                            focusData.FocusWeight = objectTrackingWeight[i];
                            ret = uni_plugin_set(m_OTpluginHandle,
                                    OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
                            if(ret < 0) {
                                CLOGE("[OBTR](%s[%d]):Object Tracking plugin set focus info failed!!", __FUNCTION__, __LINE__);
                            }
                        }

                        delete[] objectTrackingArea;
                        delete[] objectTrackingWeight;

                        m_parameters->setObjectTrackingChanged(false);
                        m_OTisTouchAreaGet = true;
                    }

                    if ((m_parameters->getObjectTrackingEnable() == true
                                || m_parameters->getObjectTrackingGet() == true)
                            && m_OTstart == OBJECT_TRACKING_INIT
                            && m_OTisTouchAreaGet == true) {
                        int HwPreviewW = 0, HwPreviewH = 0;
                        m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

                        UniPluginBufferData_t bufferData;
                        memset(&bufferData, 0, sizeof(UniPluginBufferData_t));

                        bufferData.InBuffY = buffer.addr[0];
                        bufferData.InBuffU = buffer.addr[1];

                        bufferData.InWidth = HwPreviewW;
                        bufferData.InHeight = HwPreviewH;
                        bufferData.InFormat = UNI_PLUGIN_FORMAT_NV21;
                        ret = uni_plugin_set(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
                        if(ret < 0) {
                            CLOGE("[OBTR](%s[%d]):Object Tracking plugin set buffer info failed!!", __FUNCTION__, __LINE__);
                        }

                        ret = uni_plugin_process(m_OTpluginHandle);
                        if(ret < 0) {
                            CLOGE("[OBTR](%s[%d]):Object Tracking plugin process failed!!", __FUNCTION__, __LINE__);
                        }

                        uni_plugin_get(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &m_OTfocusData);
                        CLOGV("[OBTR](%s[%d])Focus state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                                __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
                                m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                                m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
                        CLOGV("[OBTR](%s[%d])Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                                __FUNCTION__, __LINE__, m_OTfocusData.W_Movement, m_OTfocusData.H_Movement,
                                m_OTfocusData.W_Velocity, m_OTfocusData.H_Velocity);

                        if (m_parameters->getObjectTrackingEnable() == true && m_OTstart == OBJECT_TRACKING_INIT) {
                            m_parameters->setObjectTrackingGet(true);
                        }

                        m_OTstatus = m_OTfocusData.FocusState;

                        uni_plugin_get(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_PREDICTED, &m_OTpredictedData);
                        CLOGV("[OBTR](%s[%d])Predicted state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                                __FUNCTION__, __LINE__, m_OTpredictedData.FocusState,
                                m_OTpredictedData.FocusROILeft, m_OTpredictedData.FocusROIRight,
                                m_OTpredictedData.FocusROITop, m_OTpredictedData.FocusROIBottom, m_OTpredictedData.FocusWeight);
                        CLOGV("[OBTR](%s[%d])Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                                __FUNCTION__, __LINE__, m_OTpredictedData.W_Movement, m_OTpredictedData.H_Movement,
                                m_OTpredictedData.W_Velocity, m_OTpredictedData.H_Velocity);

                        ExynosRect cropRegionRect;
                        ExynosRect2 oldrect, newRect;
                        oldrect.x1 = m_OTpredictedData.FocusROILeft;
                        oldrect.x2 = m_OTpredictedData.FocusROIRight;
                        oldrect.y1 = m_OTpredictedData.FocusROITop;
                        oldrect.y2 = m_OTpredictedData.FocusROIBottom;
                        m_parameters->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
                        newRect = convertingAndroidArea2HWAreaBcropOut(&oldrect, &cropRegionRect);
                        m_OTpredictedData.FocusROILeft = newRect.x1;
                        m_OTpredictedData.FocusROIRight = newRect.x2;
                        m_OTpredictedData.FocusROITop = newRect.y1;
                        m_OTpredictedData.FocusROIBottom = newRect.y2;

                        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
                        ret = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
                        if (ret == false) {
                            CLOGE("[OBTR](%s[%d]):setObjectTrackingAreas failed!!", __FUNCTION__, __LINE__);
                        }
                    }
                    if (m_OTpluginHandle != NULL && m_OTstart == OBJECT_TRACKING_DEINIT) {
                        int ret = uni_plugin_deinit(m_OTpluginHandle);
                        if (ret < 0) {
                            CLOGE("[OBTR](%s[%d]):Object Tracking plugin deinit failed!!", __FUNCTION__, __LINE__);
                        }
                        m_OTstart = OBJECT_TRACKING_IDLE;
                        /* Waiting in the recording mode can make preview thread stop */
#if 0
                        m_OT_mutex.lock();
                        if(m_OTisWait == true)
                            m_OT_condition.signal();
                        m_OT_mutex.unlock();
#endif
                    }
#endif
#ifdef SAMSUNG_LBP
                    if(((m_captureSelector->getFrameIndex() == LBPframeCount && m_captureSelector->getFrameIndex())
                                || (m_LBPindex > 0 && m_LBPindex < m_parameters->getHoldFrameCount()))
                            && m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE
                            && m_isLBPon == true){
                        CLOGD("[LBP](%s[%d]):Start BestPic(%d/%d)", __FUNCTION__, __LINE__, LBPframeCount, m_LBPindex);
                        char *srcAddr = NULL;
                        char *dstAddr = NULL;
                        int hwPreviewFormat = m_parameters->getHwPreviewFormat();
                        int planeCount = getYuvPlaneCount(hwPreviewFormat);
                        if (planeCount <= 0) {
                            CLOGE("[LBP](%s[%d]):getYuvPlaneCount(%d) fail", __FUNCTION__, __LINE__, hwPreviewFormat);
                        }

                        ret = m_lbpBufferMgr->getBuffer(&m_LBPindex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &m_LBPbuffer[m_LBPindex].buffer);

                        /* TODO : have to consider all fmt(planes) and stride */
                        for (int plane = 0; plane < planeCount; plane++) {
                            srcAddr = buffer.addr[plane];
                            dstAddr = m_LBPbuffer[m_LBPindex].buffer.addr[plane];
                            memcpy(dstAddr, srcAddr, m_LBPbuffer[m_LBPindex].buffer.size[plane]);
                        }
                        m_LBPbuffer[m_LBPindex].frameNumber = LBPframeCount;
                        /* There is a case that LBP thread ends first, we need to increase the index before that. */
                        m_LBPindex++;
                        m_LBPbufferQ->pushProcessQ(&m_LBPbuffer[m_LBPindex-1]);
                    }
#endif
#ifdef SAMSUNG_HLV
                    if (m_HLVprocessStep == HLV_PROCESS_STOP) {
                        if (m_HLV != NULL) {
                            /* De-Init should be called before m_stopRecordingInternal to avoid race-condition in libHighLightVideo */
                            ALOGD("HLV : uni_plugin_deinit !!!");
                            ret = uni_plugin_deinit(m_HLV);
                            if (ret)
                                ALOGE("HLV : uni_plugin_deinit failed !! Possible Memory Leak !!!");

                            ret = uni_plugin_unload(&m_HLV);
                            if (ret)
                                ALOGE("HLV : uni_plugin_unload failed !! Possible Memory Leak !!!");

                            m_HLV = NULL;
                            m_HLVprocessStep = HLV_PROCESS_DONE;
                        }
                    }
#endif
#ifdef SAMSUNG_HYPER_MOTION
                    if (m_hyperMotionModeGet() == true && m_hyperMotionStep == HYPER_MOTION_STOP) {
                        ALOGD("HYPERMOTION : uni_plugin_deinit !!!");
                        m_hyperMotion_deinit();
                        if (m_hyperMotion_BufferMgr!= NULL) {
                            m_hyperMotion_BufferMgr->resetBuffers();
                        }
                    }
#endif

#ifdef SUPPORT_SW_VDIS
                    if (m_swVDIS_Mode == true) {
                        /* VDIS processing */
                        memset(m_swVDIS_fd_dm, 0x00, sizeof(struct camera2_dm));
                        if (frame->getDynamicMeta(m_swVDIS_fd_dm) != NO_ERROR) {
                            CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
                        }

                        swVDIS_BufIndex = -1;
                        int swVDIS_IndexCount = (m_swVDIS_FrameNum % m_sw_VDIS_OutBufferNum);
                        m_swVDIS_TSIndexCount = (m_swVDIS_FrameNum % NUM_VDIS_BUFFERS);
                        ret = m_swVDIS_BufferMgr->getBuffer(&swVDIS_BufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
                                &m_swVDIS_OutBuf[swVDIS_IndexCount]);

                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_swVDIS_BufferMgr->getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                        } else {
                            nsecs_t swVDIS_timeStamp = (nsecs_t)frame->getTimeStamp();
                            nsecs_t swVDIS_timeStampBoot = (nsecs_t)frame->getTimeStampBoot();
                            int swVDIS_Lux = m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256;
                            int swVDIS_ZoomLevel = m_parameters->getZoomLevel();
                            float swVDIS_ExposureValue = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[4] / 256.0;

                            int swVDIS_num = 0;
                            swVDIS_num = NUM_OF_DETECTED_FACES;
                            memset(m_swVDIS_FaceData, 0x00, sizeof(swVDIS_FaceData_t));

                            for (int i = 0; i < swVDIS_num; i++) {
                                if (m_swVDIS_fd_dm->stats.faceIds[i]) {
                                    m_swVDIS_FaceData->FaceRect[i].left = m_swVDIS_fd_dm->stats.faceRectangles[i][0];
                                    m_swVDIS_FaceData->FaceRect[i].top = m_swVDIS_fd_dm->stats.faceRectangles[i][1];
                                    m_swVDIS_FaceData->FaceRect[i].right = m_swVDIS_fd_dm->stats.faceRectangles[i][2];
                                    m_swVDIS_FaceData->FaceRect[i].bottom= m_swVDIS_fd_dm->stats.faceRectangles[i][3];
                                    m_swVDIS_FaceData->FaceNum++;
                                }
                            }

                            m_swVDIS_process(&buffer, swVDIS_IndexCount, swVDIS_timeStamp, swVDIS_timeStampBoot,
                                    swVDIS_Lux, swVDIS_ZoomLevel, swVDIS_ExposureValue, m_swVDIS_FaceData);
                            m_swVDIS_StoreOffset();

#ifdef SAMSUNG_OIS_VDIS
                            if (m_parameters->getOIS() == OPTICAL_STABILIZATION_MODE_VDIS) {
                                UNI_PLUGIN_OIS_MODE mode = m_swVDIS_getOISmode();
                                uint32_t coef = 0;
                                if(mode == UNI_PLUGIN_OIS_MODE_CENTERING)
                                    coef = 0xFFFF;
                                else
                                    coef = 0x1;

                                if (m_OISvdisMode != mode) {
                                    CLOGD("DEBUG(%s[%d]):OIS VDIS coef: %d", __FUNCTION__, __LINE__, coef);
                                    m_OISvdisMode = mode;
                                }
                                m_parameters->setOISCoef(coef);
                            }
#endif
                        }
                    }
#endif

#ifdef SAMSUNG_HYPER_MOTION
                    if (m_hyperMotionModeGet() == true) {
                        hyperMotion_BufIndex = -1;
                        int hyperMotion_IndexCount = (m_hyperMotion_FrameNum % NUM_HYPERMOTION_BUFFERS);
                        ret = m_hyperMotion_BufferMgr->getBuffer(&hyperMotion_BufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
                                                                &m_hyperMotion_OutBuf[hyperMotion_IndexCount]);

                        if (ret < 0) {
                            CLOGE("ERR(%s[%d]):m_hyperMotion_BufferMgr->getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                        } else {
                            nsecs_t hyperMotion_timeStamp = (nsecs_t)frame->getTimeStamp();
                            nsecs_t hyperMotion_timeStampBoot = (nsecs_t)frame->getTimeStampBoot();
                            int hyperMotion_Lux = m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256;
                            int hyperMotion_ZoomLevel = m_parameters->getZoomLevel();
                            float hyperMotion_ExposureValue = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[4] / 256.0;

                            m_hyperMotion_process(&buffer, hyperMotion_IndexCount, hyperMotion_timeStamp, hyperMotion_timeStampBoot,
                                        hyperMotion_Lux, hyperMotion_ZoomLevel, hyperMotion_ExposureValue);
                        }
                    }
#endif

                    if ((m_frameSkipCount > 0) || (skipPreview == true)) {
                        if (skipPreview != true) {
                            CLOGD("INFO(%s[%d]):Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                    __FUNCTION__, __LINE__, m_frameSkipCount, buffer.index);
                        }
#ifdef SUPPORT_SW_VDIS
                        if (m_swVDIS_Mode) {
                            if (m_frameSkipCount == 1) {
                                for (int i = 0; i < m_sw_VDIS_OutBufferNum; i++) {
                                    ret = m_swVDIS_BufferMgr->cancelBuffer(i);
                                    if (ret < 0) {
                                        CLOGE("ERR(%s[%d]):swVDIS buffer return fail", __FUNCTION__, __LINE__);
                                    }
                                }
                            }
                        }
#endif

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
                                CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                m_previewQ->pushProcessQ(&previewFrame);
                        } else {
#ifdef SUPPORT_SW_VDIS
                            if (m_swVDIS_Mode == true) {
                                if (m_parameters->isSWVdisOnPreview() == true) {
                                    int sizeOfPreviewDelayQ = m_previewDelayQ->getSizeOfProcessQ();
                                    if (sizeOfPreviewDelayQ > 0) {
                                        m_previewDelayQ->pushProcessQ(&previewFrame);
                                        m_previewDelayQ->popProcessQ(&previewFrame);
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                                        previewcallbackbuffer.index = -2;

                                        if (previewFrame->getReqeust(previewCallbackId) == true) {
                                            ret = frame->getSrcBuffer(previewPipeId, &previewcallbackbuffer);
                                            if (ret < 0) {
                                                CLOGE("ERR(%s[%d]) : getSrcBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                                            }
                                        }
#endif
                                    } else {
                                        m_previewDelayQ->pushProcessQ(&previewFrame);
                                        previewFrame = NULL;
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                                        previewcallbackbuffer.index = -2;
#endif
                                    }
                                }
                            }

                            if (previewFrame != NULL)
#endif
                            {
                                if (m_getRecordingEnabled() == true) {
                                    CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                    m_previewQ->pushProcessQ(&previewFrame);
                                    m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                } else if (m_parameters->getCaptureExposureTime() > 0
                                           && (m_meta_shot->shot.dm.sensor.exposureTime / 1000) > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                                    CLOGD("INFO(%s[%d]):manual exposure capture mode. (%lld), fcount(%d)",
                                            __FUNCTION__, __LINE__, m_meta_shot->shot.dm.sensor.exposureTime,
                                            m_meta_shot->shot.dm.request.frameCount);
    
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
    
                                    m_longExposureCaptureState = LONG_EXPOSURE_CAPTURING;
                                } else {
#ifdef OIS_CAPTURE
                                    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
                                    unsigned int OISFcount = 0;
                                    unsigned int fliteFcount = 0;
                                    bool OISCapture_activated = false;
    
                                    m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
                                    OISFcount = m_sCaptureMgr->getOISCaptureFcount();
    
                                    if (m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        CLOGD("INFO(%s[%d]):manual exposure capture mode end. (%lld), fcount(%d)",
                                            __FUNCTION__, __LINE__, m_meta_shot->shot.dm.sensor.exposureTime,
                                            m_meta_shot->shot.dm.request.frameCount);
                                        m_sCaptureMgr->resetOISCaptureFcount();
                                    }
    
                                    fliteFcount = m_meta_shot->shot.dm.request.frameCount;
    
                                    if (OISFcount) {
                                        if (OISFcount == fliteFcount
#ifdef SAMSUNG_LLS_DEBLUR
                                            && m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_NONE
#endif
                                           ) {
                                            OISCapture_activated = true;
                                        } else if (m_parameters->getCaptureExposureTime() > PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX
                                                    && fliteFcount >= OISFcount
                                                    && fliteFcount < OISFcount + m_parameters->getLongExposureShotCount() + 1) {
                                            OISCapture_activated = true;
                                        }
#ifdef SAMSUNG_LLS_DEBLUR
                                        else if (m_parameters->getLDCaptureMode() > MULTI_SHOT_MODE_NONE
                                                 && fliteFcount >= OISFcount
                                                 && fliteFcount < OISFcount + m_parameters->getLDCaptureCount()) {
                                            OISCapture_activated = true;
                                        }
#endif
                                    }
    
                                    if((m_parameters->getSeriesShotCount() == 0
#ifdef LLS_REPROCESSING
                                        || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
#endif
                                       ) && OISCapture_activated == true) {
                                        CLOGD("INFO(%s[%d]):OIS capture mode. Skip frame. FliteFrameCount(%d) ", __FUNCTION__, __LINE__,fliteFcount);
    
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
                                    } else
#endif
                                    {
                                        CLOGV("INFO(%s[%d]):push frame to previewQ", __FUNCTION__, __LINE__);
                                        m_previewQ->pushProcessQ(&previewFrame);
                                    }
    
                                    if (m_stopLongExposure && m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        m_longExposureCaptureState = LONG_EXPOSURE_CANCEL_NOTI;
                                    } else {
                                        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                    }
                                }
                            }

                            /* Recording : case of adaptive CSC recording */
#ifdef SUPPORT_SW_VDIS
                            if (m_swVDIS_Mode == true) {
                                if (swVDIS_BufIndex >= 0) {
                                    int VDISOutBufIndex = m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index;
                                    nsecs_t timeStamp = m_swVDIS_timeStamp[m_swVDIS_TSIndex];

                                    if (m_getRecordingEnabled() == true
                                        && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                        && m_swVDIS_FrameNum > m_swVDIS_Delay) {
                                        if (timeStamp <= 0L) {
                                            CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                                            if (VDISOutBufIndex >= 0) {
                                                m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                            }
                                        } else {
                                            if (m_parameters->doCscRecording() == true) {
                                                /* FHD to HD down scale */
                                                int bufIndex = -2;
                                                ExynosCameraBuffer recordingBuffer;
                                                ret = m_recordingBufferMgr->getBuffer(&bufIndex,
                                                        EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                                                if (ret < 0 || bufIndex < 0) {
                                                    CLOGE("ERR(%s[%d]): Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                                        __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex);
                                                    m_recordingBufferMgr->printBufferQState();
                                                    if (VDISOutBufIndex >= 0) {
                                                        m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                                    }
                                                } else {
                                                    if (m_recordingFrameSkipCount != 0) {
                                                        CLOGE("ERR(%s[%d]): Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                                            __FUNCTION__, __LINE__, m_recordingFrameSkipCount, bufIndex,
                                                            m_recordingQ->getSizeOfProcessQ());
                                                        m_recordingFrameSkipCount = 0;
                                                        m_recordingBufferMgr->printBufferQState();
                                                    }

                                                    ret = m_doPreviewToRecordingFunc(PIPE_GSC_VIDEO, m_swVDIS_OutBuf[m_swVDIS_OutQIndex], recordingBuffer, timeStamp);
                                                    if (ret < 0 ) {
                                                        CLOGW("WARN(%s[%d]):recordingCallback Skip", __FUNCTION__, __LINE__);
                                                    }

                                                    if (VDISOutBufIndex >= 0) {
                                                        m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                                    }
                                                    m_recordingTimeStamp[recordingBuffer.index] = timeStamp;
                                                }
                                            } else {
                                                ExynosCameraFrame *adaptiveCscRecordingFrame = NULL;
                                                adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());

                                                m_recordingTimeStamp[m_swVDIS_OutBuf[m_swVDIS_OutQIndex].index] = timeStamp;
                                                adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, m_swVDIS_OutBuf[m_swVDIS_OutQIndex]);

                                                m_recordingListLock.lock();
                                                m_recordingProcessList.push_back(adaptiveCscRecordingFrame);
                                                m_recordingListLock.unlock();
                                                m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                            }
                                        }
                                    } else {
                                        if (VDISOutBufIndex >= 0) {
                                            m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                        }
                                    }
                                }
                            }
                            else
#endif
#ifdef SAMSUNG_HYPER_MOTION
                            if (m_hyperMotionModeGet() == true) {
                                if (hyperMotion_BufIndex >= 0) {
                                    int hyperMotionOutBufIndex = m_hyperMotion_OutBuf[m_hyperMotion_OutQIndex].index;
                                    nsecs_t timeStamp = m_hyperMotionTimeStampGet();

                                    if (m_getRecordingEnabled() == true
                                        && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                        && m_hyperMotion_FrameNum > m_hyperMotion_Delay) {
                                        if (timeStamp <= 0L) {
                                            CLOGE("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                                            if (hyperMotionOutBufIndex >= 0) {
                                                m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                            }
                                        } else {
                                            if (m_hyperMotionCanSendFrame() == true) {
                                                ExynosCameraFrame *adaptiveCscRecordingFrame = NULL;
                                                adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());

                                                m_recordingTimeStamp[hyperMotionOutBufIndex] = timeStamp;
                                                adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, m_hyperMotion_OutBuf[m_hyperMotion_OutQIndex]);

                                                m_recordingListLock.lock();
                                                m_recordingProcessList.push_back(adaptiveCscRecordingFrame);
                                                m_recordingListLock.unlock();
                                                m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                            } else {
                                                ret = m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                                if (ret < 0) {
                                                    CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
                                                }
                                            }
                                        }
                                    } else {
                                        if (hyperMotionOutBufIndex >= 0) {
                                            m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                        }
                                    }
                                }
                            }
                            else
#endif
                            {
                                if (m_getRecordingEnabled() == true
                                    && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                    && m_parameters->doCscRecording() == false
                                    && frame->getRequest(recordingPipeId) == false) {
                                    if (timeStamp <= 0L) {
                                        CLOGW("WARN(%s[%d]):timeStamp(%lld) Skip", __FUNCTION__, __LINE__, timeStamp);
                                    } else {
                                        ExynosCameraFrame *adaptiveCscRecordingFrame = NULL;
                                        adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());

                                        m_recordingTimeStamp[buffer.index] = timeStamp;
                                        adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, buffer);

                                        m_recordingListLock.lock();
                                        m_recordingProcessList.push_back(adaptiveCscRecordingFrame);
                                        m_recordingListLock.unlock();
                                        m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                    }
                                }
                            }
                        }
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

                                m_recordingListLock.lock();
                                m_recordingProcessList.push_back(recordingFrame);
                                m_recordingListLock.unlock();
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
        ret = m_removeFrameFromList(&m_processList, frame);
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

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_HRM
    static uint8_t hrm_error_cnt = 0;
#endif
#ifdef SAMSUNG_LIGHT_IR
    static uint8_t lightir_error_cnt = 0;
#endif
#ifdef SAMSUNG_GYRO
    static uint8_t gyro_error_cnt = 0;
#endif
#ifdef SAMSUNG_ACCELEROMETER
    static uint8_t accelerometer_error_cnt = 0;
#endif
#endif

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

    previewQ = m_previewQ;

    if (m_longExposureCaptureState == LONG_EXPOSURE_CANCEL_NOTI) {
        m_notifyCb(COMMON_SHOT_CANCEL_PICTURE_COMPLETED, 0, 0, m_callbackCookie);
        CLOGD("INFO(%s[%d]): COMMON_SHOT_CANCEL_PICTURE_COMPLETED callback(%d)",
                __FUNCTION__, __LINE__, m_fdmeta_shot->shot.dm.aa.aeState);
        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
    }

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
        goto func_exit;
    }

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
    if (frame->getRequest(previewCallbackPipeId) == true) {
        ret = frame->getSrcBuffer(pipeId, &previewCbBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
        }
    }
#endif

    /* ------------- frome here "frame" cannot use ------------- */
    CLOGV("INFO(%s[%d]):push frame to previewReturnQ", __FUNCTION__, __LINE__);
    if(m_parameters->increaseMaxBufferOfPreview()) {
        maxbuffers = m_parameters->getPreviewBufferCount();
    } else {
        maxbuffers = (int)m_exynosconfig->current->bufInfo.num_preview_buffers;
    }

    if (scpBuffer.index < 0 || scpBuffer.index >= maxbuffers ) {
        CLOGE("ERR(%s[%d]):Out of Index! (Max: %d, Index: %d)", __FUNCTION__, __LINE__, maxbuffers, scpBuffer.index);
        goto func_exit;
    }

    CLOGV("INFO(%s[%d]):m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)", __FUNCTION__, __LINE__,
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
#ifdef LLS_CAPTURE
        || (m_parameters->getLLSOn() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
#endif
        ) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                    __FUNCTION__, __LINE__);
            if (previewCbBuffer.index >= 0) {
                ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
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
#ifdef SUPPORT_SW_VDIS
        (m_parameters->getPreviewBufferCount() == NUM_VDIS_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1) ||
#endif
#ifdef SAMSUNG_HYPER_MOTION
        (m_parameters->getPreviewBufferCount() == NUM_HYPERMOTION_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1) ||
#endif
        (m_parameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 2 &&
        previewQ->getSizeOfProcessQ() < 1)) {

        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false) {
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
            m_previewFrameFactory->setRequest(PIPE_MCSC1, true);
#endif

            ret = m_setPreviewCallbackBuffer();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_setPreviewCallback Buffer fail", __FUNCTION__, __LINE__);
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

            ExynosCameraFrame *newFrame = NULL;

            newFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(pipeIdCsc);
            if (newFrame == NULL) {
                CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
                return UNKNOWN_ERROR;
            }

            ret = m_doPreviewToCallbackFunc(pipeIdCsc, newFrame, scpBuffer, previewCbBuffer, copybuffer);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_doPreviewToCallbackFunc fail", __FUNCTION__, __LINE__);
                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                    }
                }
                m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                goto func_exit;
            } else {
                if (m_parameters->getCallbackNeedCopy2Rendering() == true) {
                    ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, scpBuffer);
                    if (ret < 0) {
                        CLOGE("ERR(%s[%d]):m_doCallbackToPreviewFunc fail", __FUNCTION__, __LINE__);
                        if (previewCbBuffer.index >= 0) {
                            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                            }
                        }
                        m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                        goto func_exit;
                    }
                }
            }

            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }
        }
#ifdef USE_GSC_FOR_PREVIEW
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

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_HRM
        if (m_uv_rayHandle != NULL) {
            ret = sensor_listener_get_data(m_uv_rayHandle, ST_UV_RAY, &m_uv_rayListenerData, false);
            if (ret < 0) {
                if ((hrm_error_cnt % 30) == 0) {
                    CLOGW("WRN(%s[%d]:%d):skip the HRM data", __FUNCTION__, __LINE__, ret);
                    hrm_error_cnt = 1;
                } else {
                    hrm_error_cnt++;
                }
            } else {
                m_parameters->m_setHRM(m_uv_rayListenerData.uv_ray.ir_data,
                                       m_uv_rayListenerData.uv_ray.flicker_data,
                                       m_uv_rayListenerData.uv_ray.status);
            }
        }
#endif

#ifdef SAMSUNG_LIGHT_IR
        if (m_light_irHandle != NULL) {
            ret = sensor_listener_get_data(m_light_irHandle, ST_LIGHT_IR, &m_light_irListenerData, false);
            if (ret < 0) {
                if ((lightir_error_cnt % 30) == 0) {
                    CLOGW("WRN(%s[%d]:%d):skip the Light IR data", __FUNCTION__, __LINE__, ret);
                    lightir_error_cnt = 1;
                } else {
                    lightir_error_cnt++;
                }
            } else {
                m_parameters->m_setLight_IR(m_light_irListenerData);
            }
        }
#endif

#ifdef SAMSUNG_GYRO
        if (m_gyroHandle != NULL) {
            ret = sensor_listener_get_data(m_gyroHandle, ST_GYROSCOPE, &m_gyroListenerData, false);
            if (ret < 0) {
                if ((gyro_error_cnt % 30) == 0) {
                    CLOGW("WRN(%s[%d]:%d):skip the Gyro data", __FUNCTION__, __LINE__, ret);
                    gyro_error_cnt = 1;
                } else {
                    gyro_error_cnt++;
                }
            } else {
                m_parameters->m_setGyro(m_gyroListenerData);
            }
        }
#endif

#ifdef SAMSUNG_ACCELEROMETER
        if (m_accelerometerHandle != NULL) {
            ret = sensor_listener_get_data(m_accelerometerHandle, ST_ACCELEROMETER, &m_accelerometerListenerData, false);
            if (ret < 0) {
                if ((accelerometer_error_cnt % 30) == 0) {
                    CLOGW("WRN(%s[%d]:%d):skip the Acceleration data", __FUNCTION__, __LINE__, ret);
                    accelerometer_error_cnt = 1;
                } else {
                    accelerometer_error_cnt++;
                }
            } else {
                m_parameters->m_setAccelerometer(m_accelerometerListenerData);
            }
        }
#endif
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SUPPORT_SW_VDIS
        if (m_swVDIS_Mode == true) {
            int swVDIS_InW, swVDIS_InH, swVDIS_OutW, swVDIS_OutH;
            int swVDIS_StartX, swVDIS_StartY;

            m_parameters->getHwPreviewSize(&swVDIS_InW, &swVDIS_InH);
            swVDIS_OutW = swVDIS_InW;
            swVDIS_OutH = swVDIS_InH;
            m_swVDIS_AdjustPreviewSize(&swVDIS_OutW, &swVDIS_OutH);

            swVDIS_StartX = (swVDIS_InW-swVDIS_OutW)/2;
            swVDIS_StartY = (swVDIS_InH-swVDIS_OutH)/2;

            /* Preview VDIS (except for FHD 60fps & front cam) */
            if (m_parameters->isSWVdisOnPreview() == true) {
                int offset_x = 0;
                int offset_y = 0;

                m_swVDIS_GetOffset(&offset_x, &offset_y, (long long int)frame->getTimeStampBoot());
                swVDIS_StartX += offset_x;
                swVDIS_StartY += offset_y;
            }

            if (m_previewWindow != NULL) {
                m_previewWindow->set_crop(m_previewWindow, swVDIS_StartX, swVDIS_StartY,
                        swVDIS_OutW + swVDIS_StartX, swVDIS_OutH + swVDIS_StartY);
            }
        }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            int hyperMotion_InW, hyperMotion_InH, hyperMotion_OutW, hyperMotion_OutH;
            int hyperMotion_StartX, hyperMotion_StartY;

            m_parameters->getHwPreviewSize(&hyperMotion_InW, &hyperMotion_InH);
            hyperMotion_OutW = hyperMotion_InW;
            hyperMotion_OutH = hyperMotion_InH;
            m_hyperMotion_AdjustPreviewSize(&hyperMotion_OutW, &hyperMotion_OutH);

            hyperMotion_StartX = (hyperMotion_InW-hyperMotion_OutW)/2;
            hyperMotion_StartY = (hyperMotion_InH-hyperMotion_OutH)/2;

            if (m_previewWindow != NULL) {
                m_previewWindow->set_crop(m_previewWindow, hyperMotion_StartX, hyperMotion_StartY,
                        hyperMotion_OutW + hyperMotion_StartX, hyperMotion_OutH + hyperMotion_StartY);
            }
        }
#endif /*SAMSUNG_HYPER_MOTION*/

        /* Prevent displaying unprocessed beauty images in beauty shot. */
        if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE)
#ifdef LLS_CAPTURE
            || (m_parameters->getLLSOn() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
#endif
            ) {
            if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
                checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
                CLOGV("INFO(%s[%d]):skip the preview callback and the preview display while compressed callback.",
                        __FUNCTION__, __LINE__);
                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
                    }
                }
                ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);
                goto func_exit;
            }
        }

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
        if (((m_getRecordingEnabled() == true || (m_parameters->getFastFpsMode() > 1 && m_parameters->getRecordingHint() == 1))
                && (curMinFps == curMaxFps)
                && (m_parameters->getShotMode() != SHOT_MODE_SEQUENCE))
            || (m_parameters->getVRMode() == 1)) {
            if (previewQ->getSizeOfProcessQ() < 1) {
                m_previewFrameScheduler->m_schedulePreviewFrame(curMaxFps);
            }
        }
#endif

        /* display the frame */
        ret = m_putBuffers(m_scpBufferMgr, scpBuffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("ERR(%s[%d]):put Buffer fail", __FUNCTION__, __LINE__);
        }

        if (previewCbBuffer.index >= 0) {
            ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Preview callback buffer return fail", __FUNCTION__, __LINE__);
            }
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

func_exit:

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

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_Mode == true && m_doCscRecording == true) {
        pipeId = PIPE_GSC_VIDEO;
    }
#endif
    nsecs_t timeStamp = 0;

    ExynosCameraBuffer buffer;
    ExynosCameraFrame *frame = NULL;

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
        int heapIndex = -1;
        native_handle* handle = NULL;
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

        if (m_recordingCallbackHeap != NULL) {
            struct VideoNativeHandleMetadata *recordAddrs = NULL;

            ret = m_getAvailableRecordingCallbackHeapIndex(&heapIndex);
            if (ret != NO_ERROR || heapIndex < 0 || heapIndex >= m_recordingBufferCount) {
                CLOGE("ERR(%s[%d]):Failed to getAvailableRecordingCallbackHeapIndex %d",
                        __FUNCTION__, __LINE__,
                        heapIndex);
                goto func_exit;
            }

            recordAddrs = (struct VideoNativeHandleMetadata *) m_recordingCallbackHeap->data;
            recordAddrs[heapIndex].eType = kMetadataBufferTypeNativeHandleSource;

            handle = native_handle_create(2, 1);
            handle->data[0] = (int32_t) buffer.fd[0];
            handle->data[1] = (int32_t) buffer.fd[1];
            handle->data[2] = (int32_t) buffer.index;

            recordAddrs[heapIndex].pHandle = handle;

            m_recordingBufAvailable[buffer.index] = false;
            m_lastRecordingTimeStamp = timeStamp;
        } else {
            CLOGD("INFO(%s[%d]):m_recordingCallbackHeap is NULL.", __FUNCTION__, __LINE__);
        }

        if (m_getRecordingEnabled() == true
            && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)) {

#ifdef SAMSUNG_HLV
            if (m_HLV) {
                /* Ignore the ERROR .. HLV solution is smart */
                ret = m_ProgramAndProcessHLV(&buffer);
            }
#endif

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

void ExynosCamera::m_vendorSpecificPreConstructor(int cameraId, __unused camera_device_t *dev)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_COMPANION
    m_companionNode = NULL;
    m_companionThread = NULL;
    CLOGI("INFO(%s):cameraId(%d), sensorId(%d), use_companion(%d)",
        __FUNCTION__, cameraId, m_cameraSensorId, m_use_companion);

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
#else
    CLOGI("INFO(%s):cameraId(%d), sensorId(%d)", __FUNCTION__, cameraId, m_cameraSensorId);
#endif

#if defined(SAMSUNG_EEPROM)
    m_eepromThread = NULL;
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD("DEBUG(%s): eepromThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s): failed the m_eepromThread", __FUNCTION__);
        }
    }
#endif

#ifdef SAMSUNG_UNIPLUGIN
    m_uniPluginThread = new mainCameraThread(this, &ExynosCamera::m_uniPluginThreadFunc, "uniPluginThread");
    if(m_uniPluginThread != NULL) {
        m_uniPluginThread->run();
        CLOGD("DEBUG(%s): uniPluginThread starts", __FUNCTION__);
    } else {
        CLOGE("(%s): failed the m_uniPluginThread", __FUNCTION__);
    }
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    m_preStartPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_preStartPictureInternalThreadFunc,
                                                  "m_preStartPictureInternalThread");
    CLOGD("DEBUG(%s): m_preStartPictureInternalThread created", __FUNCTION__);
#endif

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

void ExynosCamera::m_vendorSpecificConstructor(__unused int cameraId, __unused camera_device_t *dev)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

#ifdef SAMSUNG_UNI_API
    uni_api_init();
#endif

#ifdef SUPPORT_SW_VDIS
    m_createInternalBufferManager(&m_swVDIS_BufferMgr, "VDIS_BUF");
    m_previewDelayQ = new frame_queue_t;
    m_previewDelayQ->setWaitTime(550000000);
#endif /*SUPPORT_SW_VDIS*/
#ifdef SAMSUNG_HYPER_MOTION
    m_createInternalBufferManager(&m_hyperMotion_BufferMgr, "HYPERMOTION_BUF");
#endif /*SAMSUNG_HYPER_MOTION*/
#ifdef SAMSUNG_LBP
    m_createInternalBufferManager(&m_lbpBufferMgr, "LBP_BUF");
#endif
#ifdef SAMSUNG_LENS_DC
    m_createInternalBufferManager(&m_lensDCBufferMgr, "LENSDC_BUF");
#endif
#ifdef SR_CAPTURE
    m_srCallbackHeap = NULL;
    memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
#endif

    m_postPictureCallbackThread = new mainCameraThread(this, &ExynosCamera::m_postPictureCallbackThreadFunc, "postPictureCallbackThread");
    CLOGD("DEBUG(%s): postPictureCallbackThread created", __FUNCTION__);
#ifdef SAMSUNG_LBP
    m_LBPThread = new mainCameraThread(this, &ExynosCamera::m_LBPThreadFunc, "LBPThread");
    CLOGD("DEBUG(%s): LBPThread created", __FUNCTION__);
#endif

    /* m_ThumbnailCallback Thread */
    m_ThumbnailCallbackThread = new mainCameraThread(this, &ExynosCamera::m_ThumbnailCallbackThreadFunc, "m_ThumbnailCallbackThread");
    CLOGD("DEBUG(%s):m_ThumbnailCallbackThread created", __FUNCTION__);

#ifdef RAWDUMP_CAPTURE
    /* RawCaptureDump Thread */
    m_RawCaptureDumpThread = new mainCameraThread(this, &ExynosCamera::m_RawCaptureDumpThreadFunc, "m_RawCaptureDumpThread");
    CLOGD("DEBUG(%s):RawCaptureDumpThread created", __FUNCTION__);
#endif

#ifdef SAMSUNG_DNG
    /* m_DNGCaptureThread */
    m_DNGCaptureThread = new mainCameraThread(this, &ExynosCamera::m_DNGCaptureThreadFunc, "m_DNGCaptureThread");
    CLOGD("DEBUG(%s):m_DNGCaptureThread created", __FUNCTION__);
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    /* m_LDCapture Thread */
    m_LDCaptureThread = new mainCameraThread(this, &ExynosCamera::m_LDCaptureThreadFunc, "m_LDCaptureThread");
    CLOGD("DEBUG(%s):m_LDCaptureThread created", __FUNCTION__);
#endif
#ifdef SAMSUNG_SENSOR_LISTENER
    m_sensorListenerThread = new mainCameraThread(this, &ExynosCamera::m_sensorListenerThreadFunc, "sensorListenerThread");
    CLOGD("DEBUG(%s):m_sensorListenerThread created", __FUNCTION__);
#endif

    m_yuvCallbackThread = new mainCameraThread(this, &ExynosCamera::m_yuvCallbackThreadFunc, "yuvCallbackThread");
    CLOGD("DEBUG(%s):yuvCallbackThread created", __FUNCTION__);

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("DEBUG(%s):visionThread created", __FUNCTION__);

    m_yuvCallbackQ = new frame_queue_t(m_yuvCallbackThread);

#ifdef SAMSUNG_LBP
    m_LBPbufferQ = new lbp_queue_t;
    m_LBPbufferQ->setWaitTime(1000000000); //1sec
#endif
#ifdef RAWDUMP_CAPTURE
    m_RawCaptureDumpQ = new frame_queue_t(m_RawCaptureDumpThread);
#endif
#ifdef SAMSUNG_DNG
    m_dngCaptureQ = new dng_capture_queue_t;
    m_dngCaptureQ->setWaitTime(1000000000);

    m_dngCaptureDoneQ = new bayer_release_queue_t;
    m_dngCaptureDoneQ->setWaitTime(10000000);
#endif
#ifdef SAMSUNG_BD
    m_BDbufferQ = new bd_queue_t;
    m_BDbufferQ->setWaitTime(1000000000);
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureQ = new frame_queue_t(m_LDCaptureThread);
#endif
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    m_flagFlashCallback = false;
    m_flagHdrCallback = false;
#endif
#ifdef BURST_CAPTURE
    m_burstCaptureCallbackCount = 0;
    m_burstShutterLocation = BURST_SHUTTER_PREPICTURE;
#endif
#ifdef SAMSUNG_HLV
    m_HLV = NULL;
    m_HLVprocessStep = HLV_PROCESS_DONE;
#endif

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    m_lensmoveCount = 0;
#endif
    m_currentSetStart = false;
    m_flagMetaDataSet = false;
#ifdef LLS_CAPTURE
    for (int i = 0; i < LLS_HISTORY_COUNT; i++)
        m_needLLS_history[i] = 0;
#endif
#ifdef SAMSUNG_MAGICSHOT
    if(getCameraIdInternal() == CAMERA_ID_BACK) {
        m_magicshotMaxCount = MAGICSHOT_COUNT_REAR;
    } else {
        m_magicshotMaxCount = MAGICSHOT_COUNT_FRONT;
    }
#endif

#ifdef OIS_CAPTURE
    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
    m_sCaptureMgr->resetOISCaptureFcount();

    m_OISCaptureShutterEnabled = false;
#endif

#ifdef SAMSUNG_LBP
    if(getCameraIdInternal() == CAMERA_ID_FRONT) {
        m_parameters->resetNormalBestFrameCount();
        m_parameters->resetSCPFrameCount();
        m_parameters->resetBayerFrameCount();
    }
#endif
#ifdef SAMSUNG_HRM
    m_uv_rayHandle = NULL;
#endif
#ifdef SAMSUNG_LIGHT_IR
    m_light_irHandle = NULL;
#endif
#ifdef SAMSUNG_GYRO
    m_gyroHandle = NULL;
#endif
#ifdef SAMSUNG_ACCELEROMETER
    m_accelerometerHandle = NULL;
#endif

#ifdef SUPPORT_SW_VDIS
    m_swVDIS_fd_dm = new struct camera2_dm;
    m_swVDIS_FaceData = new struct swVDIS_FaceData;
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureCount = 0;

    for (int i = 0; i < MAX_LD_CAPTURE_COUNT; i++) {
        m_LDBufIndex[i] = 0;
    }
#endif

#ifdef ONE_SECOND_BURST_CAPTURE
    /* init one second burst capture parameters. */
    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
        TakepictureDurationTime[i] = 0;
    m_one_second_burst_capture = false;
    m_one_second_burst_first_after_open = false;
    m_one_second_jpegCallbackHeap = NULL;
    m_one_second_postviewCallbackHeap = NULL;
#endif
#ifdef SAMSUNG_QUICKSHOT
    m_quickShotStart = true;
#endif
#ifdef SAMSUNG_DNG
    m_dngFrameNumber = 0;
    m_dngBayerHeap = NULL;
#endif

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    m_previewFrameScheduler = new SecCameraPreviewFrameScheduler();
#endif

#ifdef SAMSUNG_FRONT_LCD_FLASH
    strncpy(m_prevHBM, "LCDF", sizeof(m_prevHBM));
    strncpy(m_prevAutoHBM, "LCDF", sizeof(m_prevAutoHBM));
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    m_isQuickSwitchState = QUICK_SWITCH_IDLE;
#endif

    m_fdCallbackToggle = 0;

    m_flagAFDone = false;

#ifdef SAMSUNG_LENS_DC
    m_skipLensDC = true;
    m_LensDCIndex = -1;
#endif

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI("INFO(%s[%d]): -IN-", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

#ifdef SAMSUNG_UNI_API
    uni_api_deinit();
#endif

#ifdef SAMSUNG_MAGICSHOT
    if (m_dstPostPictureGscQ != NULL) {
        delete m_dstPostPictureGscQ;
        m_dstPostPictureGscQ = NULL;
    }
#endif
#ifdef SAMSUNG_LBP
    if (m_LBPbufferQ != NULL) {
        delete m_LBPbufferQ;
        m_LBPbufferQ = NULL;
    }
#endif
#ifdef SAMSUNG_DNG
    if (m_dngCaptureQ != NULL) {
        delete m_dngCaptureQ;
        m_dngCaptureQ = NULL;
    }

    if (m_dngCaptureDoneQ != NULL) {
        delete m_dngCaptureDoneQ;
        m_dngCaptureDoneQ = NULL;
    }
#endif
#ifdef SAMSUNG_BD
    if (m_BDbufferQ != NULL) {
        delete m_BDbufferQ;
        m_BDbufferQ = NULL;
    }
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    if (m_LDCaptureQ != NULL) {
        delete m_LDCaptureQ;
        m_LDCaptureQ = NULL;
    }
#endif
#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        delete m_lbpBufferMgr;
        m_lbpBufferMgr = NULL;
        CLOGD("DEBUG(%s):BufferManager(lbpBufferMgr) destroyed", __FUNCTION__);
    }
#endif
#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL) {
        delete m_swVDIS_BufferMgr;
        m_swVDIS_BufferMgr = NULL;
        VDIS_LOG("VDIS_HAL(%s):BufferManager(m_swVDIS_BufferMgr) destroyed", __FUNCTION__);
    }
    if (m_previewDelayQ != NULL) {
        delete m_previewDelayQ;
        m_previewDelayQ = NULL;
    }
#endif /*SUPPORT_SW_VDIS*/
#ifdef SAMSUNG_HYPER_MOTION
    if (m_hyperMotion_BufferMgr != NULL) {
        delete m_hyperMotion_BufferMgr;
        m_hyperMotion_BufferMgr = NULL;
        CLOGD("HyperMotion_HAL(%s):BufferManager(m_hyperMotion_BufferMgr) destroyed", __FUNCTION__);
    }
#endif /*SAMSUNG_HYPER_MOTION*/

#ifdef SAMSUNG_UNIPLUGIN
    if (m_uniPluginThread != NULL)
        m_uniPluginThread->requestExitAndWait();
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_preStartPictureInternalThread != NULL) {
        m_preStartPictureInternalThread->requestExitAndWait();
    }
#endif

#ifdef SAMSUNG_LENS_DC
    if(m_DCpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_DCpluginHandle);
        if(ret < 0) {
            CLOGE("[DC](%s[%d]):DC plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_LLV
    if(m_LLVpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLVpluginHandle);
        if(ret < 0) {
            CLOGE("[LLV](%s[%d]):LLV plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_OT
    if(m_OTpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_OTpluginHandle);
        if(ret < 0) {
            CLOGE("[OBTR](%s[%d]):Object Tracking plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_LBP
    if(m_LBPpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LBPpluginHandle);
        if(ret < 0) {
            CLOGE("[LBP](%s[%d]):Bestphoto plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]):JPEG Qtable plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_BD
    if(m_BDpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_BDpluginHandle);
        if(ret < 0) {
            CLOGE("[BD](%s[%d]):Blur Detection plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    if(m_LLSpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLSpluginHandle);
        if(ret < 0) {
            CLOGE("[LLS_MBR](%s[%d]):LLS plugin unload failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif

#ifdef SAMSUNG_HRM
    if (m_uv_rayHandle != NULL) {
        sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY);
        sensor_listener_unload(&m_uv_rayHandle);
        m_uv_rayHandle = NULL;
    }
#endif
#ifdef SAMSUNG_LIGHT_IR
    if (m_light_irHandle != NULL) {
        sensor_listener_disable_sensor(m_light_irHandle, ST_LIGHT_IR);
        sensor_listener_unload(&m_light_irHandle);
        m_light_irHandle = NULL;
    }
#endif
#ifdef SAMSUNG_GYRO
    if (m_gyroHandle != NULL) {
        sensor_listener_disable_sensor(m_gyroHandle, ST_GYROSCOPE);
        sensor_listener_unload(&m_gyroHandle);
        m_gyroHandle = NULL;
    }
#endif
#ifdef SAMSUNG_ACCELEROMETER
    if (m_accelerometerHandle != NULL) {
        sensor_listener_disable_sensor(m_accelerometerHandle, ST_ACCELEROMETER);
        sensor_listener_unload(&m_accelerometerHandle);
        m_accelerometerHandle = NULL;
}
#endif

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_fd_dm != NULL) {
        delete m_swVDIS_fd_dm;
        m_swVDIS_fd_dm = NULL;
    }

    if (m_swVDIS_FaceData != NULL) {
        delete m_swVDIS_FaceData;
        m_swVDIS_FaceData = NULL;
    }
#endif /*SUPPORT_SW_VDIS*/

    CLOGI("INFO(%s[%d]): -OUT-", __FUNCTION__, __LINE__);

    return;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrame *frame)
{
    ExynosCameraDurationTimer m_fdcallbackTimer;
    long long m_fdcallbackTimerTime;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    bool autoFocusChanged = false;
#ifdef LLS_CAPTURE
    int LLS_value = 0;
#endif
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
    int ret = 0;
    uint32_t pipeId = PIPE_VC1;
    camera_memory_t *depthCallbackHeap = NULL;

    memset(&m_frameMetadata.depth_data, 0x00, sizeof(camera_depth_data_t));
#endif

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

    if (m_flagAFDone) {
        m_flagAFDone = false;
        autoFocusChanged = true;
        CLOGD("DEBUG(%s[%d]) AF Done", __FUNCTION__, __LINE__);
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

#ifdef USE_MULTI_FACING_SINGLE_CAMERA
                if (m_parameters->getCameraDirection() == CAMERA_FACING_DIRECTION_BACK) {
                    int tempInt;
                    tempInt = m_faces[realNumOfDetectedFaces].rect[0];
                    m_faces[realNumOfDetectedFaces].rect[0] = -(m_faces[realNumOfDetectedFaces].rect[2]);
                    m_faces[realNumOfDetectedFaces].rect[2] = -tempInt;
                    tempInt = m_faces[realNumOfDetectedFaces].rect[1];
                    m_faces[realNumOfDetectedFaces].rect[1] = -(m_faces[realNumOfDetectedFaces].rect[3]);
                    m_faces[realNumOfDetectedFaces].rect[3] = -tempInt;
                }
#endif
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

#ifdef LLS_CAPTURE
    m_needLLS_history[3] = m_needLLS_history[2];
    m_needLLS_history[2] = m_needLLS_history[1];
    m_needLLS_history[1] = m_parameters->getLLS(m_fdmeta_shot);

    if (m_needLLS_history[1] == m_needLLS_history[2]
        && m_needLLS_history[1] == m_needLLS_history[3])
        LLS_value = m_needLLS_history[0] = m_needLLS_history[1];
    else
        LLS_value = m_needLLS_history[0];

    m_parameters->setLLSValue(LLS_value);

    if (m_parameters->getShotMode() == SHOT_MODE_PRO_MODE) {
        LLS_value = LLS_LEVEL_ZSL_LIKE; /* disable SR Mode */
    } else if ((LLS_value >= LLS_LEVEL_MULTI_MERGE_INDICATOR_2
                    && LLS_value <= LLS_LEVEL_MULTI_MERGE_INDICATOR_4)
                || (LLS_value >= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2
                    && LLS_value <= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4)) {
        LLS_value = LLS_LEVEL_LLS_INDICATOR_ONLY;
    }

    m_frameMetadata.needLLS = LLS_value;
    m_frameMetadata.light_condition = LLS_value;

    CLOGV("[%d[%d][%d][%d] LLS_value(%d) light_condition(%d)",
            m_needLLS_history[0], m_needLLS_history[1], m_needLLS_history[2], m_needLLS_history[3],
            m_parameters->getLLSValue(), m_frameMetadata.light_condition);
#endif

#if defined(BURST_CAPTURE) && defined(VARIABLE_BURST_FPS)
    /* vendorSpecific[64] : Exposure time's denominator */
    if ((m_fdmeta_shot->shot.udm.ae.vendorSpecific[64] <= 7) ||
#ifdef LLS_CAPTURE
        (m_frameMetadata.light_condition == LLS_LEVEL_LLS_INDICATOR_ONLY)
#endif
        ) {
        m_frameMetadata.burstshot_fps = BURSTSHOT_OFF_FPS;
    } else if (m_fdmeta_shot->shot.udm.ae.vendorSpecific[64] <= 14) {
        m_frameMetadata.burstshot_fps = BURSTSHOT_MIN_FPS;
    } else {
        m_frameMetadata.burstshot_fps = BURSTSHOT_MAX_FPS;
    }
    CLOGV("(%s):m_frameMetadata.burstshot_fps(%d)", __FUNCTION__, m_frameMetadata.burstshot_fps);
#endif

#ifdef SAMSUNG_LBP
    uint32_t shutterSpeed = 0;

    if (m_fdmeta_shot->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
        shutterSpeed = m_fdmeta_shot->shot.udm.ae.vendorSpecific[64];
    else
        shutterSpeed = m_fdmeta_shot->shot.udm.internal.vendorSpecific2[100];

    CLOGV("[LBP]: ShutterSpeed:%d", shutterSpeed);
    if(shutterSpeed <= 24)
        m_isLBPlux = true;
    else
        m_isLBPlux = false;
#endif

#ifdef TOUCH_AE
    if (m_parameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH
        && m_parameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH
        && m_fdmeta_shot->shot.dm.aa.vendor_touchAeDone == 1 /* Touch AE status flag - 0:searching 1:done */
        ) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGD("INFO(%s[%d]): AE_RESULT(%d)", __FUNCTION__, __LINE__, m_fdmeta_shot->shot.dm.aa.aeState);
    }

    m_frameMetadata.ae_bv_changed = m_fdmeta_shot->shot.dm.aa.vendor_touchBvChange;
#endif

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    if (autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING) {
        /* Single Window */
        m_frameMetadata.current_set_data.lens_position_min = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionMacro;
        m_frameMetadata.current_set_data.lens_position_max = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionInfinity;
        m_frameMetadata.current_set_data.lens_position_current = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionCurrent;
        m_frameMetadata.current_set_data.driver_resolution = m_fdmeta_shot->shot.udm.companion.pdaf.lensPosResolution;
        CLOGV("current_set_data.exposure_time = %d", m_frameMetadata.current_set_data.exposure_time);
        CLOGV("current_set_data.iso = %d", m_frameMetadata.current_set_data.iso);
        CLOGV("current_set_data.lens_position_min = %d", m_frameMetadata.current_set_data.lens_position_min);
        CLOGV("current_set_data.lens_position_max = %d", m_frameMetadata.current_set_data.lens_position_max);
        CLOGV("current_set_data.lens_position_current = %d", m_frameMetadata.current_set_data.lens_position_current);
        CLOGV("current_set_data.driver_resolution = %d", m_frameMetadata.current_set_data.driver_resolution);

        m_flagMetaDataSet = true;
    }

    if (autoFocusMgr->getCurrentState() == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING) {
        m_frameMetadata.single_paf_data.mode = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.mode;
        m_frameMetadata.single_paf_data.goal_pos = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.goalPos;
        m_frameMetadata.single_paf_data.reliability = m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.reliability;
        CLOGV("single_paf_data.mode = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.mode);
        CLOGV("single_paf_data.goal_pos = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.goalPos);
        CLOGV("single_paf_data.reliability = %d", m_fdmeta_shot->shot.udm.companion.pdaf.singleResult.reliability);
    }

    if ((m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS) || (m_parameters->getShotMode() == SHOT_MODE_PRO_MODE)) {
        if (m_fdmeta_shot->shot.udm.companion.pdaf.numCol > 0 &&
                    m_fdmeta_shot->shot.udm.companion.pdaf.numRow > 0) {
            if (autoFocusChanged || (m_parameters->getShotMode() == SHOT_MODE_PRO_MODE)) {
                /* Multi window */
                m_frameMetadata.dof_column = m_fdmeta_shot->shot.udm.companion.pdaf.numCol;
                m_frameMetadata.dof_row = m_fdmeta_shot->shot.udm.companion.pdaf.numRow;

                if (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS)
                    m_frameMetadata.dof_usage = PAF_DATA_FOR_OUTFOCUS;
                else
                    m_frameMetadata.dof_usage = PAF_DATA_FOR_MULTI_AF;

                if (m_frameMetadata.dof_data == NULL)
                     m_frameMetadata.dof_data = new st_AF_PafWinResult[m_frameMetadata.dof_column * m_frameMetadata.dof_row];

                for (int i = 0; i < m_frameMetadata.dof_row; i++) {
                    for (int j = 0; j < m_frameMetadata.dof_column; j++) {
                        memcpy(m_frameMetadata.dof_data + (i * m_frameMetadata.dof_column + j), &(m_fdmeta_shot->shot.udm.companion.pdaf.multiResult[i][j]),
                                sizeof(camera2_pdaf_multi_result));
                        CLOGV("dof_data[%d][%d] mode = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_column +j))->mode);
                        CLOGV("dof_data[%d][%d] goalPos = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_column +j))->goal_pos);
                        CLOGV("dof_data[%d][%d] reliability = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_column +j))->reliability);
                        CLOGV("dof_data[%d][%d] focused = %d", i, j, (m_frameMetadata.dof_data+(i*m_frameMetadata.dof_column +j))->focused);
                    }
                }

                m_frameMetadata.current_set_data.lens_position_min = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionMacro;
                m_frameMetadata.current_set_data.lens_position_max = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionInfinity;
                m_frameMetadata.current_set_data.lens_position_current = (uint16_t)m_fdmeta_shot->shot.udm.af.lensPositionCurrent;
                m_frameMetadata.current_set_data.driver_resolution = m_fdmeta_shot->shot.udm.companion.pdaf.lensPosResolution;
                CLOGV("current_set_data.lens_position_min = %d", m_frameMetadata.current_set_data.lens_position_min);
                CLOGV("current_set_data.lens_position_max = %d", m_frameMetadata.current_set_data.lens_position_max);
                CLOGV("current_set_data.lens_position_current = %d", m_frameMetadata.current_set_data.lens_position_current);
                CLOGV("current_set_data.driver_resolution = %d", m_frameMetadata.current_set_data.driver_resolution);

                m_flagMetaDataSet = true;
            }
        }
    }

    /* Set capture exposure time, iso value in pro mode */
    if (m_parameters->getShotMode() == SHOT_MODE_PRO_MODE) {
        m_frameMetadata.current_set_data.exposure_time = (uint32_t)(1000000000.0/m_fdmeta_shot->shot.udm.ae.vendorSpecific[64]);
        m_frameMetadata.current_set_data.iso = (uint16_t)m_fdmeta_shot->shot.udm.internal.vendorSpecific2[101];
    } else {
        m_frameMetadata.current_set_data.exposure_time = (uint32_t)m_fdmeta_shot->shot.dm.sensor.exposureTime;
        m_frameMetadata.current_set_data.iso = (uint16_t)m_fdmeta_shot->shot.dm.sensor.sensitivity;
    }
    m_frameMetadata.current_set_data.exposure_value = (int16_t)m_fdmeta_shot->shot.dm.aa.vendor_exposureValue;
    CLOGV("current_set_data.exposure_time = %d", m_frameMetadata.current_set_data.exposure_time);
    CLOGV("current_set_data.exposure_value = %d", m_frameMetadata.current_set_data.exposure_value);
    CLOGV("current_set_data.iso = %d", m_frameMetadata.current_set_data.iso);
#endif
#ifdef SAMSUNG_OT
    if (m_parameters->getObjectTrackingGet() == true){
        m_frameMetadata.object_data.focusState = m_OTfocusData.FocusState;
        m_frameMetadata.object_data.focusROI[0] = m_OTfocusData.FocusROILeft;
        m_frameMetadata.object_data.focusROI[1] = m_OTfocusData.FocusROITop;
        m_frameMetadata.object_data.focusROI[2] = m_OTfocusData.FocusROIRight;
        m_frameMetadata.object_data.focusROI[3] = m_OTfocusData.FocusROIBottom;
        CLOGV("[OBTR](%s[%d])focusState: %d, focusROI[0]: %d, focusROI[1]: %d, focusROI[2]: %d, focusROI[3]: %d",
            __FUNCTION__, __LINE__, m_OTfocusData.FocusState,
            m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROITop,
            m_OTfocusData.FocusROIRight, m_OTfocusData.FocusROIBottom);
    }
#endif

#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    if (m_parameters->getFlashMode() == FLASH_MODE_AUTO) {
        ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
        m_frameMetadata.flash_enabled = (int32_t) m_flashMgr->getNeedCaptureFlash();
    } else {
        m_frameMetadata.flash_enabled = 0;
    }

#ifdef SAMSUNG_COMPANION
    if (m_parameters->getRTHdr() == COMPANION_WDR_AUTO) {
        if (m_fdmeta_shot->shot.dm.stats.vendor_wdrAutoState == STATE_WDR_AUTO_REQUIRED)
            m_frameMetadata.hdr_enabled = 1;
        else
            m_frameMetadata.hdr_enabled = 0;
    } else
#endif
    {
        m_frameMetadata.hdr_enabled = 0;
    }

    m_frameMetadata.awb_exposure.brightness = m_fdmeta_shot->shot.udm.awb.vendorSpecific[11];
    m_frameMetadata.awb_exposure.colorTemp = m_fdmeta_shot->shot.udm.awb.vendorSpecific[21];
    m_frameMetadata.awb_exposure.awbRgbGain[0] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[22];
    m_frameMetadata.awb_exposure.awbRgbGain[1] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[23];
    m_frameMetadata.awb_exposure.awbRgbGain[2] = m_fdmeta_shot->shot.udm.awb.vendorSpecific[24];
#endif

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getRequest(PIPE_VC1) == true) {
        ret = frame->getDstBuffer(pipeId, &depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
        }

        if (depthMapBuffer.index != -2 && autoFocusChanged) {
            int depthW = 0, depthH = 0;
            m_parameters->getDepthMapSize(&depthW, &depthH);
            depthCallbackHeap = m_getMemoryCb(depthMapBuffer.fd[0], depthMapBuffer.size[0], 1, m_callbackCookie);

            if (!depthCallbackHeap || depthCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, depthMapBuffer.size[0]);
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

            CLOGD("INFO(%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

            bRet = dumpToFile((char *)filePath,
                 (char *)depthCallbackHeap->data,
                depthMapW * depthMapH * 2);
            if (bRet != true)
                ALOGE("couldn't make a raw file", __FUNCTION__, __LINE__);
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
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
            || m_flagFlashCallback || m_flagHdrCallback
#endif
#ifdef SAMSUNG_OT
            || m_parameters->getObjectTrackingGet()
#endif
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
            CLOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                    __FUNCTION__, __LINE__);
        }
    }
#endif

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    m_clearDOFmeta();
#endif
    memset(&m_frameMetadata.current_set_data, 0, sizeof(camera_current_set_t));

    m_flagMetaDataSet = false;

    if((int)m_fdcallbackTimerTime / 1000 > 50) {
        CLOGD("INFO(%s[%d]): FD callback duration time : (%d)mec", __FUNCTION__, __LINE__, (int)m_fdcallbackTimerTime / 1000);
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

    CLOGV("DEBUG(%s[%d]):--IN-- (previewBuf.index=%d, recordingBuf.index=%d)",
        __FUNCTION__, __LINE__, previewBuf.index, recordingBuf.index);

    status_t ret = NO_ERROR;
    ExynosRect srcRect, dstRect;
    ExynosCameraFrame  *newFrame = NULL;
    struct camera2_node_output node;
#ifdef PERFRAME_CONTROL_FOR_FLIP
    int flipHorizontal = m_parameters->getFlipHorizontal();
    int flipVertical = m_parameters->getFlipVertical();
#endif

    newFrame = m_previewFrameFactory->createNewFrameVideoOnly();
    if (newFrame == NULL) {
        CLOGE("ERR(%s):newFrame is NULL", __FUNCTION__);
        return UNKNOWN_ERROR;
    }

#ifdef PERFRAME_CONTROL_FOR_FLIP
    newFrame->setFlipHorizontal(pipeId, flipHorizontal);
    newFrame->setFlipVertical(pipeId, flipVertical);
#endif

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
    m_recordingListLock.lock();
    m_recordingProcessList.push_back(newFrame);
    m_recordingListLock.unlock();
    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

#ifdef SUPPORT_SW_VDIS
    if (m_doCscRecording == true && m_swVDIS_Mode == true) {
        m_recordingStopLock.lock();
        if (m_getRecordingEnabled() == false) {
            m_recordingStopLock.unlock();
            CLOGD("DEBUG(%s[%d]): m_getRecordingEnabled is false, skip frame(%d) previewBuf(%d) recordingBuf(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount(), previewBuf.index, recordingBuf.index);
            if (newFrame != NULL) {
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
                newFrame = NULL;
            }

            if (recordingBuf.index >= 0){
                m_putBuffers(m_recordingBufferMgr, recordingBuf.index);
            }
            goto func_exit;
        }
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        m_recordingStopLock.unlock();
    } else
#endif
    {
        m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
    }

func_exit:

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;
}

status_t ExynosCamera::m_reprocessingYuvCallbackFunc(ExynosCameraBuffer yuvBuffer)
{
    camera_memory_t *yuvCallbackHeap = NULL;

    if (m_hdrEnabled == false && m_parameters->getSeriesShotCount() <= 0
#ifdef SAMSUNG_DNG
            && !m_parameters->getDNGCaptureModeOn()
#endif
       ) {
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
                && !m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
                && (m_parameters->getShotMode() != SHOT_MODE_MAGIC)
#endif
           ) {
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

#ifdef SAMSUNG_LLV
    if (m_parameters->getLLV() == true
#ifdef USE_LIVE_BROADCAST
        && m_parameters->getPLBMode() == false
#endif
       ) {
        if(m_LLVpluginHandle != NULL){
            int HwPreviewW = 0, HwPreviewH = 0;
            m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

            UniPluginBufferData_t pluginData;
            UniPluginCameraInfo_t pluginCameraInfo;
            pluginCameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            pluginCameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
            pluginData.InWidth= HwPreviewW;
            pluginData.InHeight= HwPreviewH;
            if(m_LLVpluginHandle != NULL) {
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            }
            int powerCtrl = UNI_PLUGIN_POWER_CONTROL_4;
            if(m_LLVpluginHandle != NULL) {
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
                uni_plugin_set(m_LLVpluginHandle,
                    LLV_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &pluginCameraInfo);
            }
            uni_plugin_init(m_LLVpluginHandle);

            m_LLVstatus = LLV_INIT;
        }
    }
#endif

    m_parameters->getHwVideoSize(&videoW, &videoH);
    CLOGD("DEBUG(%s[%d]):videoSize = %d x %d",  __FUNCTION__, __LINE__, videoW, videoH);

    m_doCscRecording = true;
    if (m_parameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("INFO(%s[%d]):do Recording CSC !!! m_recordingBufferCount(%d)", __FUNCTION__, __LINE__, m_recordingBufferCount);
    } else {
        m_doCscRecording = false;
#ifdef SUPPORT_SW_VDIS
        if (m_swVDIS_Mode == true) {
            m_recordingBufferCount = m_sw_VDIS_OutBufferNum;
        } else
#endif
        {
            m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
        }
        CLOGI("INFO(%s[%d]):skip Recording CSC !!! m_recordingBufferCount(%d->%d)",
            __FUNCTION__, __LINE__, m_exynosconfig->current->bufInfo.num_recording_buffers, m_recordingBufferCount);
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList);
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
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct VideoNativeHandleMetadata), m_recordingBufferCount, &heapFd);
    if (!m_recordingCallbackHeap || m_recordingCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%zu) fail", __FUNCTION__, __LINE__, sizeof(struct addrs));
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* Initialize recording callback heap available flags */
    memset(m_recordingCallbackHeapAvailable, 0x00, sizeof(m_recordingCallbackHeapAvailable));
    for (int i = 0; i < m_recordingBufferCount; i++) {
        m_recordingCallbackHeapAvailable[i] = true;
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

#ifdef SUPPORT_SW_VDIS
    CLOGD("ERR(%s[%d]):m_doCscRecording == %d, m_swVDIS_Mode == %d",
            __FUNCTION__, __LINE__, m_doCscRecording, m_swVDIS_Mode);

    if (m_doCscRecording == true && m_swVDIS_Mode == true) {
        int recPipeId = PIPE_GSC_VIDEO;
        {
            Mutex::Autolock lock(m_recordingStopLock);
            m_previewFrameFactory->stopPipe(recPipeId);
        }
    }
#endif

    m_recordingQ->sendCmd(WAKE_UP);
    m_recordingThread->requestExitAndWait();
    m_recordingQ->release();
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
        return ret;
    }
    m_recordingListLock.unlock();

    /* Checking all frame(buffer) released from Media recorder */
    for (int bufferIndex = 0; bufferIndex < m_recordingBufferCount; bufferIndex++) {
        if (m_recordingBufAvailable[bufferIndex] == false) {
            if (m_doCscRecording == true) {
                m_releaseRecordingBuffer(bufferIndex);
            } else {
                m_recordingTimeStamp[bufferIndex] = 0L;
                m_recordingBufAvailable[bufferIndex] = true;

#ifdef SAMSUNG_HYPER_MOTION
                if (m_parameters->getHyperMotionMode() == true) {
                    m_putBuffers(m_hyperMotion_BufferMgr, bufferIndex);
                }
#endif

#ifdef SUPPORT_SW_VDIS
                if (m_swVDIS_Mode == true) {
                    m_putBuffers(m_swVDIS_BufferMgr, bufferIndex);
                }
#endif
            }
            CLOGW("WRN(%s[%d]):frame(%d) wasn't release, and forced release at here",
                    __FUNCTION__, __LINE__, bufferIndex);
        }
    }

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

#ifdef SAMSUNG_LLV
    if (m_LLVstatus == LLV_INIT) {
        m_LLVstatus = LLV_STOPPED;
    }
#endif

    return NO_ERROR;
}

bool ExynosCamera::m_shutterCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    int loop = false;

#ifdef OIS_CAPTURE
    if(m_OISCaptureShutterEnabled) {
        CLOGD("INFO(%s[%d]):shutter callback wait start", __FUNCTION__, __LINE__);
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->waitShutterCallback();
        CLOGD("INFO(%s[%d]):fast shutter callback wait end(%d)", __FUNCTION__, __LINE__, m_sCaptureMgr->getOISCaptureFcount());
    }
#endif

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
        CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
        m_notifyCb(CAMERA_MSG_SHUTTER, m_parameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
        m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
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
    bool needMmap = false;

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

        ret = m_allocBuffers(m_ispReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                            minBufferCount, maxBufferCount, type, allocMode, true, false);
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

    ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                        minBufferCount, maxBufferCount, type, allocMode, true, false);
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

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true) {
        needMmap = true;
    } else
#endif
    {
        needMmap = false;
    }
    ret = m_allocBuffers(m_thumbnailBufferMgr, planeCount, planeSize, bytesPerLine,
                        minBufferCount, maxBufferCount, type, allocMode, true, needMmap);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_thumbnailBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("INFO(%s[%d]):m_allocBuffers(Thumb Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d) needMmap(%d)",
            __FUNCTION__, __LINE__, maxThumbnailW, maxThumbnailH, planeCount, maxBufferCount, needMmap);

    return NO_ERROR;
}

#ifdef SAMSUNG_QUICK_SWITCH
status_t ExynosCamera::m_reallocVendorBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    m_setBuffersThread->run(PRIORITY_DEFAULT);
    m_setBuffersThread->join();

    return ret;
}

status_t ExynosCamera::m_deallocVendorBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->deinit();
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        m_depthMapBufferMgr->deinit();
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

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->deinit();
    }

    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit();
    }

    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }

    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }

    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }

    if (m_sccBufferMgr != NULL) {
        m_sccBufferMgr->deinit();
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->deinit();
    }

    if (m_recordingBufferMgr != NULL) {
        m_recordingBufferMgr->deinit();
    }

    if (m_postPictureBufferMgr != NULL) {
        m_postPictureBufferMgr->deinit();
    }

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->deinit();
    }
#endif

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
    }
#endif

    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->deinit();
    }

    if (m_previewCallbackBufferMgr != NULL) {
        m_previewCallbackBufferMgr->deinit();
    }

    if (m_highResolutionCallbackBufferMgr != NULL) {
        m_highResolutionCallbackBufferMgr->deinit();
    }

    m_isSuccessedBufferAllocation = false;

    return ret;
}

bool ExynosCamera::getQuickSwitchFlag()
{
    return m_parameters->getQuickSwitchFlag();
}
#endif

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

    if ((m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
        || m_parameters->getUsePureBayerReprocessing() == true)
        maxBufferCount = m_exynosconfig->current->bufInfo.num_sensor_buffers;
    else
        maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers;

#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
#if defined(CAMERA_ADD_BAYER_ENABLE) || defined(SAMSUNG_DNG)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#endif
        m_bayerBufferMgr->setContigBufCount(RESERVED_NUM_BAYER_BUFFERS);
    } else if (m_parameters->getDualMode() == false) {
#if defined(SAMSUNG_DNG)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#endif
        m_bayerBufferMgr->setContigBufCount(FRONT_RESERVED_NUM_BAYER_BUFFERS);
    } else
#endif
    {
#if defined(SAMSUNG_DNG) || defined(CAMERA_ADD_BAYER_ENABLE)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
#endif
    }

    if (m_parameters->getIntelligentMode() != 1) {
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }
    }

#ifdef CAMERA_PACKED_BAYER_ENABLE
    memset(&bytesPerLine, 0, sizeof(unsigned int) * EXYNOS_CAMERA_BUFFER_MAX_PLANES);
#endif

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

        ret = m_allocBuffers(m_fliteBufferMgr, planeCount, planeSize, \
                               bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):fliteBuffer m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        CLOGI("INFO(%s[%d]):m_allocBuffers(Flite Buffer) planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                __FUNCTION__, __LINE__, planeSize[0], planeCount, maxBufferCount);
    }
#endif

#ifdef SAMSUNG_LBP
    planeCount      = 3;
    planeSize[0]    = hwPreviewW * hwPreviewH;
    planeSize[1]    = hwPreviewW * hwPreviewH / 2;
    maxBufferCount  = m_parameters->getHoldFrameCount();
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(m_lbpBufferMgr, planeCount, planeSize, bytesPerLine, maxBufferCount, maxBufferCount, type, true, true);
    if (ret < 0) {
        CLOGE("[LBP]ERR(%s[%d]):m_lbpBufferMgr m_allocBuffers(bufferCount=%d) fail",
            __FUNCTION__, __LINE__, maxBufferCount);
        return ret;
    }
#endif

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_Mode == true) {
        int hwPreviewW = 0, hwPreviewH = 0;
        m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);

        planeCount = 3;
        planeSize[0] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                            * ROUND_UP(hwPreviewH, CAMERA_16PX_ALIGN)
                            + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                            * ROUND_UP(hwPreviewH / 2, CAMERA_16PX_ALIGN)
                            + MFC_7X_BUFFER_OFFSET;
        maxBufferCount = m_sw_VDIS_OutBufferNum;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_swVDIS_BufferMgr, planeCount, planeSize, bytesPerLine,
                                maxBufferCount, maxBufferCount, type, true, true);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):m_swVDIS_BufferMgr m_allocBuffers(bufferCount=%d) fail",
                __FUNCTION__, __LINE__, maxBufferCount);
            return ret;
        }

        VDIS_LOG("VDIS_HAL: m_allocBuffers(m_swVDIS_BufferMgr): %d x %d", hwPreviewW, hwPreviewH);
    }
#endif

#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            int hwPreviewW = 0, hwPreviewH = 0;
            m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
            m_hyperMotion_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);

            planeCount = 3;
            planeSize[0] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                                * ROUND_UP(hwPreviewH, CAMERA_16PX_ALIGN)
                                + MFC_7X_BUFFER_OFFSET;
            planeSize[1] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                                * ROUND_UP(hwPreviewH / 2, CAMERA_16PX_ALIGN)
                                + MFC_7X_BUFFER_OFFSET;
            maxBufferCount = NUM_HYPERMOTION_BUFFERS;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

            ret = m_allocBuffers(m_hyperMotion_BufferMgr, planeCount, planeSize, bytesPerLine,
                                    maxBufferCount, maxBufferCount, type, true, true);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):m_hyperMotion_BufferMgr m_allocBuffers(bufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount);
                return ret;
            }

            CLOGD("HyperMotion_HAL: m_allocBuffers(m_hyperMotion_BufferMgr): %d x %d", hwPreviewW, hwPreviewH);
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
#ifdef SAMSUNG_DNG
    int bayerFormat = m_parameters->getBayerFormat(m_getBayerPipeId());
    unsigned int dngBayerBufferSize = 0;
#endif
    int planeCount;
    int minBufferCount;
    int maxBufferCount;
    exynos_camera_buffer_type_t type;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
    CLOGI("(%s):Sensor MAX width x height = %dx%d", __FUNCTION__, sensorMaxW, sensorMaxH);
    m_parameters->getMaxThumbnailSize(&sensorMaxThumbW, &sensorMaxThumbH);
    CLOGI("(%s):Sensor MAX Thumbnail width x height = %dx%d", __FUNCTION__, sensorMaxThumbW, sensorMaxThumbH);

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("DEBUG(%s[%d]):DNG capture on", __FUNCTION__, __LINE__);
        dngBayerBufferSize = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat)
                            + DNG_HEADER_LIMIT_SIZE
                            + (sensorMaxThumbW * sensorMaxThumbH * 3);
        m_dngBayerHeap = m_getMemoryCb(-1, dngBayerBufferSize, 1, m_callbackCookie);
        if (!m_dngBayerHeap || (m_dngBayerHeap->data == MAP_FAILED)) {
            CLOGE("ERR(%s[%d]):DNG m_getMemoryCb(%d bytes) fail", __FUNCTION__, __LINE__, dngBayerBufferSize);
            m_dngBayerHeap = NULL;
        }
    }
#endif

    /* Allocate post picture buffers */
    int f_pictureW = 0, f_pictureH = 0;
    int f_previewW = 0, f_previewH = 0;
    int inputW = 0, inputH = 0;
#ifdef SAMSUNG_LENS_DC
    if (!m_parameters->getSamsungCamera() && m_parameters->getLensDCMode()) {
        m_parameters->getMaxPictureSize(&f_pictureW, &f_pictureH);
    } else
#endif
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
    minBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

#if defined (SAMSUNG_BD) || defined(SAMSUNG_LLS_DEBLUR)
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
                            minBufferCount, maxBufferCount, type, allocMode, true, true);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }
#else
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
                            minBufferCount, maxBufferCount, type, allocMode, true, false);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                __FUNCTION__, __LINE__, minBufferCount, maxBufferCount);
        return ret;
    }
#endif

    CLOGI("INFO(%s[%d]):m_allocBuffers(m_postPictureBuffer) %d x %d, planeCount(%d), planeSize(%d)",
            __FUNCTION__, __LINE__, inputW, inputH, planeCount, planeSize[0]);

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCMode()) {
        minBufferCount = 1;
        maxBufferCount = 1;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_lensDCBufferMgr, planeCount, planeSize, bytesPerLine,
                                minBufferCount, maxBufferCount, type, allocMode, false, true);

        CLOGI("INFO(%s[%d]):m_allocBuffers(m_lensDCBufferMgr) %d x %d, planeCount(%d), planeSize(%d)",
                __FUNCTION__, __LINE__, inputW, inputH, planeCount, planeSize[0]);
    }
#endif

    if (m_parameters->getSamsungCamera() == true) {
        int thumbnailW = 0, thumbnailH = 0;
        m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);
        if (thumbnailW > 0 && thumbnailH > 0) {
            planeSize[0] = FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH);
            planeCount  = 1;
            maxBufferCount = NUM_THUMBNAIL_POSTVIEW_BUFFERS;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

            ret = m_allocBuffers(m_thumbnailGscBufferMgr, planeCount, planeSize,
                        bytesPerLine, maxBufferCount, maxBufferCount, type, allocMode, false, true);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):m_thumbnailGscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    __FUNCTION__, __LINE__, maxBufferCount, maxBufferCount);
                return ret;
            }

            CLOGI("INFO(%s[%d]):m_allocBuffers(m_thumbnailGscBuffer) %d x %d, planeCount(%d), planeSize(%d)",
                    __FUNCTION__, __LINE__, thumbnailW, thumbnailH, planeCount, planeSize[0]);

        }
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseVendorBuffers(void)
{
    CLOGI("INFO(%s[%d]):release buffer", __FUNCTION__, __LINE__);
    int ret = 0;

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL)
        m_lbpBufferMgr->deinit();
#endif

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL)
        m_swVDIS_BufferMgr->deinit();
#endif

#ifdef SAMSUNG_HYPER_MOTION
    if (m_hyperMotion_BufferMgr != NULL)
        m_hyperMotion_BufferMgr->deinit();
#endif

#ifdef SAMSUNG_DNG
    if (m_dngBayerHeap != NULL) {
        m_dngBayerHeap->release(m_dngBayerHeap);
        m_dngBayerHeap = NULL;
    }
#endif

#ifdef SR_CAPTURE
    if (m_srCallbackHeap != NULL) {
        m_srCallbackHeap->release(m_srCallbackHeap);
        m_srCallbackHeap = NULL;
    }
#endif

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->deinit();
    }
#endif

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
        /* The received native_handle must be closed */
        /* return; */
    }

    struct VideoNativeHandleMetadata *releaseMetadata = (struct VideoNativeHandleMetadata *) opaque;

    if (releaseMetadata == NULL) {
        CLOGW("WARN(%s[%d]):releaseMetadata is NULL", __FUNCTION__, __LINE__);
        return;
    }

    native_handle_t *releaseHandle = NULL;
    int releaseBufferIndex = -1;

    if (releaseMetadata->eType != kMetadataBufferTypeNativeHandleSource) {
        CLOGW("WARN(%s[%d]):Inavlid VideoNativeHandleMetadata Type %d",
                __FUNCTION__, __LINE__,
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
    releaseBufferIndex = releaseHandle->data[2];
    if (releaseBufferIndex >= m_recordingBufferCount) {
        CLOGW("WARN(%s[%d]):Invalid VideoBufferIndex %d",
                __FUNCTION__, __LINE__,
                releaseBufferIndex);

        goto CLEAN;
    } else if (m_recordingBufAvailable[releaseBufferIndex] == true) {
        CLOGW("WARN(%s[%d]):Already released VideoBufferIndex %d",
                __FUNCTION__, __LINE__,
                releaseBufferIndex);

        goto CLEAN;
    }

    if (m_doCscRecording == true) {
        m_releaseRecordingBuffer(releaseBufferIndex);
    } else {
#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            m_putBuffers(m_hyperMotion_BufferMgr, releaseBufferIndex);
        }
#endif
#ifdef SUPPORT_SW_VDIS
        if (m_swVDIS_Mode == true) {
            m_putBuffers(m_swVDIS_BufferMgr, releaseBufferIndex);
        }
#endif
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
    CLOGV("INFO(%s[%d]):", __FUNCTION__, __LINE__);

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
        !(getCameraIdInternal() == CAMERA_ID_FRONT && m_parameters->getDualMode())){
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

#ifdef CAMERA_GED_FEATURE
        /* in GED */
        m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie);
#else
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 1002, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif

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

#ifdef SAMSUNG_TN_FEATURE
        if (m_recordingEnabled == true
          && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
          && m_recordingCallbackHeap != NULL
          && m_callbackCookie != NULL) {

          CLOGD("DEBUG(%s[%d]):Timestamp callback with CAMERA_MSG_ERROR start", __FUNCTION__, __LINE__);

          m_dataCbTimestamp(
              0,
              CAMERA_MSG_ERROR | CAMERA_MSG_VIDEO_FRAME,
              m_recordingCallbackHeap,
              0,
              m_callbackCookie);

          CLOGD("DEBUG(%s[%d]):Timestamp callback with CAMERA_MSG_ERROR end", __FUNCTION__, __LINE__);
        }
#endif

#ifdef CAMERA_GED_FEATURE
        /* in GED */
        /* skip error callback */
        /* m_notifyCb(CAMERA_MSG_ERROR, 100, 0, m_callbackCookie); */
#else
        /* specifically defined */
        m_notifyCb(CAMERA_MSG_ERROR, 2001, 0, m_callbackCookie);
        /* or */
        /* android_printAssert(NULL, LOG_TAG, "killed by itself"); */
#endif

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", __FUNCTION__, __LINE__, PIPE_3AA, ret);
        }

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

bool ExynosCamera::m_autoFocusResetNotify(int focusMode)
{
    /* show restart */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, FOCUS_RESULT_RESTART, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_RESTART, 0, m_callbackCookie);

    /* show focusing */
    CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __func__, FOCUS_RESULT_FOCUSING, focusMode);
    m_notifyCb(CAMERA_MSG_FOCUS, FOCUS_RESULT_FOCUSING, 0, m_callbackCookie);

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

    if (getCameraIdInternal() == CAMERA_ID_FRONT) {
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
    } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
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
#ifdef SAMSUNG_TN_FEATURE
            /* if inactive detected, tell it */
            if (m_parameters->getSamsungCamera() == true && focusMode == FOCUS_MODE_CONTINUOUS_PICTURE) {
                if (m_exynosCameraActivityControl->getCAFResult() == FOCUS_RESULT_CANCEL) {
                    afFinalResult = FOCUS_RESULT_CANCEL;
                }
            }
#endif

            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)", __FUNCTION__, afFinalResult, focusMode);
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
            m_autoFocusLock.lock();
            m_flagAFDone = true;
        } else {
            CLOGD("DEBUG(%s):m_notifyCb is NULL mode(%d)", __FUNCTION__, focusMode);
        }
    } else {
        CLOGV("DEBUG(%s):autoFocus canceled, no callback !!", __FUNCTION__);
    }

    autoFocusMgr->displayAFStatus();

    m_autoFocusRunning = false;

    CLOGV("DEBUG(%s):exiting with no error", __FUNCTION__);

#ifdef SAMSUNG_TN_FEATURE
    if (focusMode == FOCUS_MODE_CONTINUOUS_PICTURE || focusMode == FOCUS_MODE_CONTINUOUS_VIDEO) {
        bool prev_afInMotion = autoFocusMgr->getAfInMotionResult();
        bool afInMotion = false;

        autoFocusMgr->setAfInMotionResult(afInMotion);

        if (m_notifyCb != NULL && m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE)
            && (prev_afInMotion != afInMotion)) {
            CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS_MOVE(%d) mode(%d)",
                __FUNCTION__, afInMotion, m_parameters->getFocusMode());
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS_MOVE, afInMotion, 0, m_callbackCookie);
            m_autoFocusLock.lock();
        }
    }
#endif

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

#ifdef SAMSUNG_TN_FEATURE
    /* Continuous Auto-focus */
    if(m_parameters->getSamsungCamera() == true) {
        if (m_parameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            int afstatus = FOCUS_RESULT_FAIL;
            static int afResult = FOCUS_RESULT_SUCCESS;
            static int preAFstatus = ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS;
            int curAFstatus = autoFocusMgr->getCurrentState();
            int prev_afstatus = afResult;
            afstatus = m_exynosCameraActivityControl->getCAFResult();
            afResult = afstatus;

            if (afstatus == FOCUS_RESULT_FOCUSING &&
                (prev_afstatus == FOCUS_RESULT_FAIL || prev_afstatus == FOCUS_RESULT_SUCCESS)) {
                afResult = FOCUS_RESULT_RESTART;
            }

            if((preAFstatus == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SCANNING)
                && (curAFstatus == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS
                    || curAFstatus == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_FAIL)) {
                m_flagAFDone = true;
            }
            preAFstatus = curAFstatus;

            if (m_notifyCb != NULL && m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS)
                && (prev_afstatus != afstatus)) {
                CLOGD("DEBUG(%s):CAMERA_MSG_FOCUS(%d) mode(%d)",
                    __FUNCTION__, afResult, m_parameters->getFocusMode());
                m_notifyCb(CAMERA_MSG_FOCUS, afResult, 0, m_callbackCookie);
                autoFocusMgr->displayAFStatus();
            }
        }
    } else {
        if (m_parameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_PICTURE
            || m_parameters->getFocusMode() == FOCUS_MODE_CONTINUOUS_VIDEO) {
            ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
            bool prev_afInMotion = autoFocusMgr->getAfInMotionResult();
            bool afInMotion = false;
            switch (m_exynosCameraActivityControl->getCAFResult()) {
                case FOCUS_RESULT_FAIL: // AA_AFSTATE_PASSIVE_UNFOCUSED
                case FOCUS_RESULT_SUCCESS: // AA_AFSTATE_PASSIVE_FOCUSED
                    afInMotion = false;
                    break;
                case FOCUS_RESULT_FOCUSING: // AA_AFSTATE_PASSIVE_SCAN, AA_AFSTATE_ACTIVE_SCAN
                    afInMotion = true;
                    break;
            }
            autoFocusMgr->setAfInMotionResult(afInMotion);

            if (m_notifyCb != NULL && m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE)
                && (prev_afInMotion != afInMotion)) {
                m_notifyCb(CAMERA_MSG_FOCUS_MOVE, afInMotion, 0, m_callbackCookie);
            }
        }
    }
#endif

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
        if (m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT) {
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
            if (m_parameters->isFusionEnabled() == true)
                bufMgrList[1] = &m_fusionBufferMgr;
#endif
        }

        break;
    case PIPE_DIS:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
       if (m_parameters->isFusionEnabled() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif

        break;
    case PIPE_ISPC:
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif

        break;
    case PIPE_MCSC1:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_previewCallbackBufferMgr;
        break;
    case PIPE_GSC:
        if (m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
            bufMgrList[0] = &m_sccBufferMgr;
        else
            bufMgrList[0] = &m_scpBufferMgr;

        bufMgrList[1] = &m_scpBufferMgr;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        if (m_parameters->isFusionEnabled() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif

        break;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    case PIPE_FUSION:
        bufMgrList[0] = &m_fusionBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
#endif
#ifdef SAMSUNG_MAGICSHOT
    case PIPE_GSC_VIDEO:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_postPictureBufferMgr;
        break;
#endif
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
    case PIPE_MCSC2_REPROCESSING:
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

#ifdef SAMSUNG_DNG
bool ExynosCamera::m_DNGCaptureThreadFunc(void)
{
    ExynosCameraBuffer bayerBuffer;
    int loop = false;
    int ret = 0;
    int count;
    unsigned int fliteFcount = 0;
    unsigned int dngOutSize = 0;
    camera2_shot_ext *shot_ext = NULL;
    ExynosRect srcRect, dstRect;

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

    CLOGD("[DNG](%s[%d]):-- IN --", __FUNCTION__, __LINE__);

    bayerBuffer.index = -2;

    /* wait flite */
    CLOGV("INFO(%s[%d]):wait bayer output", __FUNCTION__, __LINE__);
    ret = m_dngCaptureQ->waitAndPopProcessQ(&bayerBuffer);
    CLOGD("[DNG](%s[%d]):bayer output done pipe", __FUNCTION__, __LINE__);
    if (ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    shot_ext = (camera2_shot_ext *)(bayerBuffer.addr[1]);
    if (shot_ext != NULL)
        fliteFcount = shot_ext->shot.dm.request.frameCount;
    else
        CLOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);

    if (m_dngBayerHeap == NULL) {
        CLOGE("ERR(%s)(%d):DNG bayer heap not allocated", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    memcpy(m_dngBayerHeap->data, bayerBuffer.addr[0], bayerBuffer.size[0]);
    CLOGD("[DNG](%s[%d]): bayer copy complete! (%d bytes)", __FUNCTION__, __LINE__, bayerBuffer.size[0]);

    m_dngCaptureDoneQ->pushProcessQ(&bayerBuffer);

    CLOGD("[DNG](%s[%d]):frame count (%d)", __FUNCTION__, __LINE__, fliteFcount);

#if 0
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    bool bRet;
    char filePath[70];

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/dngbayer%d_%d.raw",
            m_parameters->getCameraId(), fliteFcount);
    /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    CLOGD("[DNG](%s[%d]):Raw frame(%dx%d) count (%d)", __FUNCTION__, __LINE__,
            sensorMaxW, sensorMaxH, fliteFcount);

    CLOGD("[DNG](%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

    bRet = dumpToFile((char *)filePath,
            (char *)m_dngBayerHeap->data,
            sensorMaxW * sensorMaxH * 2);
    if (bRet != true)
        CLOGE("couldn't make a raw file", __FUNCTION__, __LINE__);
    CLOGD("[DNG](%s[%d]):Raw Dump end (%s)", __FUNCTION__, __LINE__, filePath);
#endif

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_RAW_IMAGE)) {
        // Make Dng format
        if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT)
            m_dngCreator.makeDng(m_parameters, 0, (char *)m_dngBayerHeap->data, &dngOutSize);
        else
            m_dngCreator.makeDng(m_parameters, fliteFcount, (char *)m_dngBayerHeap->data, &dngOutSize);

        // Check DNG save path
        char default_path[20];
        memset(default_path, 0, sizeof(default_path));
        strncpy(default_path, CAMERA_SAVE_PATH_PHONE, sizeof(default_path)-1);
        m_checkCameraSavingPath(default_path,
                m_parameters->getDngFilePath(),
                m_dngSavePath, sizeof(m_dngSavePath));

        CLOGD("DNG(%s[%d]): RAW callback", __FUNCTION__, __LINE__);
        // Make Callback Buffer
        camera_memory_t *rawCallbackHeap = NULL;
        rawCallbackHeap = m_getDngCallbackHeap((char *)m_dngBayerHeap->data, dngOutSize);
        if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, dngOutSize);
            goto CLEAN;
        }

        if (m_cancelPicture == false) {
            setBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
            m_dataCb(CAMERA_MSG_RAW_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_RAW_IMAGE, true);
        }
        rawCallbackHeap->release(rawCallbackHeap);
    }

    return loop;

CLEAN:
    m_dngCaptureDoneQ->pushProcessQ(&bayerBuffer);

    return loop;
}

camera_memory_t *ExynosCamera::m_getDngCallbackHeap(ExynosCameraBuffer *dngBuf)
{
    camera_memory_t *dngCallbackHeap = NULL;
    int dngSaveLocation = m_parameters->getDngSaveLocation();

    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    if (dngSaveLocation == BURST_SAVE_CALLBACK) {
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d)", __FUNCTION__, __LINE__, dngBuf->size[0]);

        dngCallbackHeap = m_getMemoryCb(dngBuf->fd[0], dngBuf->size[0], 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, dngBuf->size[0]);
            goto done;
        }
        if (dngBuf->fd[0] < 0)
            memcpy(dngCallbackHeap->data, dngBuf->addr[0], dngBuf->size[0]);
    } else {
        char filePath[CAMERA_FILE_PATH_SIZE];
        time_t rawtime;
        struct tm *timeinfo;

        // Get current local time
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(m_dngTime, 20, "%Y%m%d_%H%M%S", timeinfo);

        // Get Full Path Name (Path + FileName)
        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "%s%s_hal.dng", m_dngSavePath, m_dngTime);
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d), filePath(%s)",
                __FUNCTION__, __LINE__, dngBuf->size[0], filePath);

        if(m_FileSaveFunc(filePath, dngBuf) == false) {
            CLOGE("ERR(%s[%d]): FILE save ERROR", __FUNCTION__, __LINE__);
            goto done;
        }

        dngCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%s) fail", __FUNCTION__, __LINE__, filePath);
            goto done;
        }

        memcpy(dngCallbackHeap->data, filePath, sizeof(filePath));
    }

done:

    if (dngCallbackHeap == NULL || dngCallbackHeap->data == MAP_FAILED) {
        if (dngCallbackHeap) {
            dngCallbackHeap->release(dngCallbackHeap);
            dngCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    CLOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return dngCallbackHeap;
}

camera_memory_t *ExynosCamera::m_getDngCallbackHeap(char *dngBuf, unsigned int bufSize)
{
    camera_memory_t *dngCallbackHeap = NULL;
    int dngSaveLocation = m_parameters->getDngSaveLocation();

    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    if (dngSaveLocation == BURST_SAVE_CALLBACK) {
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d)", __FUNCTION__, __LINE__, bufSize);

        dngCallbackHeap = m_getMemoryCb(-1, bufSize, 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, bufSize);
            goto done;
        }
        memcpy(dngCallbackHeap->data, dngBuf, bufSize);
    } else {
        char filePath[CAMERA_FILE_PATH_SIZE];
        time_t rawtime;
        struct tm *timeinfo;

        // Get current local time
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        strftime(m_dngTime, 20, "%Y%m%d_%H%M%S", timeinfo);

        // Get Full Path Name (Path + FileName)
        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "%s%s_hal.dng", m_dngSavePath, m_dngTime);
        CLOGD("DEBUG(%s[%d]):burst callback : size (%d), filePath(%s)",
                __FUNCTION__, __LINE__, bufSize, filePath);

        if(m_FileSaveFunc(filePath, dngBuf, bufSize) == false) {
            CLOGE("ERR(%s[%d]): FILE save ERROR", __FUNCTION__, __LINE__);
            goto done;
        }

        dngCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):m_getMemoryCb(%s) fail", __FUNCTION__, __LINE__, filePath);
            goto done;
        }

        memcpy(dngCallbackHeap->data, filePath, sizeof(filePath));
    }

done:

    if (dngCallbackHeap == NULL || dngCallbackHeap->data == MAP_FAILED) {
        if (dngCallbackHeap) {
            dngCallbackHeap->release(dngCallbackHeap);
            dngCallbackHeap = NULL;
        }

        m_notifyCb(CAMERA_MSG_ERROR, -1, 0, m_callbackCookie);
    }

    CLOGD("INFO(%s[%d]):making callback buffer done", __FUNCTION__, __LINE__);

    return dngCallbackHeap;
}
#endif /* SAMSUNG_DNG */

#ifdef RAWDUMP_CAPTURE
bool ExynosCamera::m_RawCaptureDumpThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    bool bRet;
    char filePath[70];
    ExynosCameraBufferManager *bufferMgr = NULL;
    int bayerPipeId = 0;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *inListFrame = NULL;
    unsigned int fliteFcount = 0;

    ret = m_RawCaptureDumpQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):newFrame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    bayerPipeId = newFrame->getFirstEntity()->getPipeId();
    ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer);
    ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);

    /* Rawdump capture is available in pure bayer only */
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)(bayerBuffer.addr[1]);
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)(bayerBuffer.addr[1]);
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            ALOGE("ERR(%s[%d]):fliteReprocessingBuffer is null", __FUNCTION__, __LINE__);
    }

    memset(filePath, 0, sizeof(filePath));
    snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",
        m_parameters->getCameraId(), fliteFcount);
    /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    CLOGD("INFO(%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

    bRet = dumpToFile((char *)filePath,
        bayerBuffer.addr[0],
        sensorMaxW * sensorMaxH * 2);
    if (bRet != true)
        ALOGE("couldn't make a raw file", __FUNCTION__, __LINE__);

    ret = bufferMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL);
    if (ret < 0) {
        ALOGE("ERR(%s[%d]):putIndex is %d", __FUNCTION__, __LINE__, bayerBuffer.index);
        bufferMgr->printBufferState();
        bufferMgr->printBufferQState();
    }

CLEAN:

    if (newFrame != NULL) {
        newFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, newFrame->getFrameCount(), &inListFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):searchFrameFromList fail", __FUNCTION__, __LINE__);
        } else {
            if (inListFrame == NULL) {
                CLOGD("DEBUG(%s[%d]): Selected frame(%d) complete, Delete",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());
                newFrame->decRef();
                m_frameMgr->deleteFrame(newFrame);
            }
            newFrame = NULL;
        }
    }

    return ret;
}
#endif

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId, ExynosCameraBuffer *buffer, camera2_shot_ext *updateDmShot
#ifdef SUPPORT_DEPTH_MAP
                                            , ExynosCameraBuffer *depthMapbuffer
#endif
                                            )
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
#ifdef LLS_REPROCESSING
    unsigned int fliteFcount = 0;
#endif

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

BAYER_RETRY:
#ifdef ONE_SECOND_BURST_CAPTURE
    if (m_parameters->getCaptureExposureTime() != 0
        && m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("DEBUG(%s[%d]):manual exposure mode. Stop one second burst.", __FUNCTION__, __LINE__);
        m_stopLongExposure = true;
        m_jpegCallbackCounter.clearCount();
        m_stopBurstShot = true;
    }
#endif

    if (m_stopLongExposure == true
        && m_parameters->getCaptureExposureTime() != 0) {
        CLOGD("DEBUG(%s[%d]):Emergency stop", __FUNCTION__, __LINE__);
        m_reprocessingCounter.clearCount();
        m_pictureEnabled = false;
        goto CLEAN;
    }

#ifdef OIS_CAPTURE
    if (m_parameters->getOISCaptureModeOn() == true) {
        retryCount = 9;
    }

    if (m_parameters->getCaptureExposureTime() > PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX) {
        m_captureSelector->setWaitTimeOISCapture(400000000);
    } else {
        m_captureSelector->setWaitTimeOISCapture(130000000);
    }
#endif

    m_captureSelector->setWaitTime(200000000);
#ifdef RAWDUMP_CAPTURE
    for (int i = 0; i < 2; i++) {
        bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);

        if(i == 0) {
            m_RawCaptureDumpQ->pushProcessQ(&bayerFrame);
        } else if (i == 1) {
            m_parameters->setRawCaptureModeOn(false);
        }
    }
#else
#ifdef SAMSUNG_QUICKSHOT
    if (m_parameters->getQuickShot() && m_quickShotStart) {
        for (int i = 0; i < FRAME_SKIP_COUNT_QUICK_SHOT; i++) {
            bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
        }
    } else
#endif
    {
#ifdef FLASHED_LLS_CAPTURE
        if (m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_FLASHED_LLS
            && m_reprocessingCounter.getCount() == m_parameters->getLDCaptureCount()) {
            CLOGD("DEBUG(%s[%d]): wait preFlash", __FUNCTION__, __LINE__);
            m_exynosCameraActivityControl->waitFlashMainReady();

            if (m_parameters->getOISCaptureModeOn()) {
                ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
                int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;

                CLOGD("DEBUG(%s[%d]): start set captureIntent(%d)", __FUNCTION__, __LINE__,captureIntent);

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):setControl(STILL_CAPTURE_OIS) fail. ret(%d) intent(%d)",
                            __FUNCTION__, __LINE__, ret, captureIntent);
                }

                sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
                m_exynosCameraActivityControl->setOISCaptureMode(true);
            }
        }
#endif
        bayerFrame = m_captureSelector->selectFrames(m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount);
    }
#ifdef SAMSUNG_QUICKSHOT
    m_quickShotStart = false;
#endif
#endif
    if (bayerFrame == NULL) {
        CLOGE("ERR(%s[%d]):bayerFrame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

#ifdef LLS_REPROCESSING
    m_parameters->setSetfileYuvRange();
#endif

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

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->getDepthCallbackOnCapture() || m_parameters->getDepthCallbackOnPreview()) {
        ret = bayerFrame->getDstBuffer(PIPE_FLITE, depthMapbuffer, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to setDstBuffer. PIPE_FLITE, CAPTURE_NODE_2", __FUNCTION__, __LINE__);
        }

        if (!(m_parameters->getDepthCallbackOnCapture() && m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21)
            && m_parameters->getDepthCallbackOnPreview()
            && depthMapbuffer->index != -2 && m_depthMapBufferMgr != NULL) {
            CLOGD("DEBUG(%s[%d]):put depthMapbuffer->index(%d)", __FUNCTION__, __LINE__, depthMapbuffer->index);
            m_putBuffers(m_depthMapBufferMgr, depthMapbuffer->index);
            depthMapbuffer->index = -2;
        }
    }
#endif

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
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
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
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
        && m_parameters->getCaptureExposureTime() <= PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX
#endif
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
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
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
                ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
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
            ret = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
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

#ifdef CAMERA_ADD_BAYER_ENABLE
        if (m_ionClient >= 0)
            ion_sync_fd(m_ionClient, buffer->fd[0]);
#endif
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

#ifdef LLS_REPROCESSING
    if(m_captureSelector->getIsLastFrame() == false) {
        if (m_captureSelector->getIsConvertingMeta() == false) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (camera2_shot_ext *)(buffer->addr[1]);
            if (shot_ext != NULL)
                fliteFcount = shot_ext->shot.dm.request.frameCount;
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)(buffer->addr[1]);
            if (shot_stream != NULL)
                fliteFcount = shot_stream->fcount;
        }

        CLOGD("DEBUG(%s[%d]): LLS Capture ...ing(hal : %d / driver : %d)", __FUNCTION__, __LINE__,
            bayerFrame->getFrameCount(), fliteFcount);
        return ret;
    }
#endif

CLEAN:
    if (saveBuffer != NULL
        && saveBuffer->index != -2
        && m_bayerBufferMgr != NULL) {
        m_putBuffers(m_bayerBufferMgr, saveBuffer->index);
        saveBuffer->index = -2;
    }

#ifdef LLS_REPROCESSING
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
        && m_captureSelector->getIsLastFrame() == true) {
        m_captureSelector->resetFrameCount();
        m_parameters->setLLSCaptureCount(0);
    }
#endif

    if (bayerFrame != NULL) {
        bayerFrame->frameUnlock();

        int funcRet = m_searchFrameFromList(&m_processList, bayerFrame->getFrameCount(), &inListFrame);
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
    m_fdCallbackToggle = 0;

    ret = m_visionFrameFactory->initPipes();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_visionFrameFactory->initPipes() failed", __FUNCTION__, __LINE__);
        return ret;
    }

    if (m_cameraHiddenId != CAMERA_ID_SECURE) {
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
    }

    for (uint32_t i = 0; i < minFrameNum; i++) {
        ret = m_generateFrameVision(i, &newFrame);
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
        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
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
    m_parameters->setPreviewRunning(m_previewEnabled);

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
    m_parameters->setPreviewRunning(m_previewEnabled);

    CLOGI("DEBUG(%s[%d]):OUT", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCamera::m_generateFrameVision(int32_t frameCount, ExynosCameraFrame **newFrame)
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
        __FUNCTION__, __LINE__, m_cameraHiddenId);
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
#ifdef SAMSUNG_TN_FEATURE
    if (m_cameraHiddenId == CAMERA_ID_SECURE) {
        planeSize[0] = SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT;
    } else
#endif
    {
        planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    }
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
        __FUNCTION__, __LINE__, m_cameraHiddenId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setVisionCallbackBuffer(void)
{
    int ret = 0;
    int previewW = 0, previewH = 0;
    int previewFormat = 0;
    m_parameters->getPreviewSize(&previewW, &previewH);
    previewFormat = m_parameters->getPreviewFormat();

    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

    int planeCount  = getYuvPlaneCount(previewFormat);
    int bufferCount = FRONT_NUM_BAYER_BUFFERS;
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#ifdef SAMSUNG_TN_FEATURE
    if (m_cameraHiddenId == CAMERA_ID_SECURE) {
        planeSize[0] = SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT;
    } else
#endif
    {
        planeSize[0] = VISION_WIDTH * VISION_HEIGHT;
    }
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
    if (m_cameraHiddenId == CAMERA_ID_SECURE) {
        dumpToFile(filePath, (char *)bayerBuffer.addr[0], SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT);
    } else {
        dumpToFile(filePath, (char *)bayerBuffer.addr[0], VISION_WIDTH * VISION_HEIGHT);
    }
    dumpIndex ++;
    if (dumpIndex > 4)
        dumpIndex = 0;
#endif

        m_parameters->getFrameSkipCount(&frameSkipCount);

        if (frameSkipCount > 0) {
            CLOGD("INFO(%s[%d]):frameSkipCount(%d)", __FUNCTION__, __LINE__, frameSkipCount);
        } else {
            /* callback */
            if (m_cameraHiddenId != CAMERA_ID_SECURE && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)) {
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

            if (m_cameraHiddenId == CAMERA_ID_SECURE && m_parameters->msgTypeEnabled(CAMERA_MSG_IRIS_DATA)) {
                ExynosCameraBuffer previewCbBuffer;
                camera_memory_t *fdCallbackData = NULL;

                int bufIndex = -2;

                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                fdCallbackData = m_getMemoryCb(-1, sizeof(CameraBufferFD), 1, m_callbackCookie);
                if (fdCallbackData == NULL) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb fail", __FUNCTION__, __LINE__);
                    return INVALID_OPERATION;
                }

                ((CameraBufferFD *)fdCallbackData->data)->magic = 0x00FD42FD;
                ((CameraBufferFD *)fdCallbackData->data)->fd = previewCbBuffer.fd[0];

                setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_IRIS_DATA, fdCallbackData, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, true);

                fdCallbackData->release(fdCallbackData);

                m_previewCallbackBufferMgr->putBuffer(bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            }
        }

        ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);

        ret = m_generateFrameVision(m_fliteFrameCount, &newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):generateFrame fail", __FUNCTION__, __LINE__);
            return ret;
        }

        ret = m_setupEntity(PIPE_FLITE, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setupEntity fail", __FUNCTION__, __LINE__);
            break;
        }

        m_visionFrameFactory->setOutputFrameQToPipe(m_pipeFrameVisionDoneQ, PIPE_FLITE);
        m_visionFrameFactory->pushFrameToPipe(&newFrame, PIPE_FLITE);
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

    if (m_cameraHiddenId != CAMERA_ID_SECURE) {
        fps = m_parameters->getVisionModeFps();
        if (m_visionFps != fps) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, fps, PIPE_FLITE);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionFps = fps;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionFps, fps);
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
                CLOGE("ERR(%s[%d]):FLITE setControl fail, ret(%d)", __FUNCTION__, __LINE__, ret);

            m_visionAe = ae;
            CLOGD("DEBUG(%s[%d]):(%d)(%d)", __FUNCTION__, __LINE__, m_visionAe, internalValue);
        }
    }
    return true;
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

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD("DEBUG(%s): eepromThread starts", __FUNCTION__);
        } else {
            CLOGE("(%s): failed the m_eepromThread", __FUNCTION__);
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

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        if (m_eepromThread != NULL) {
            CLOGI("INFO(%s[%d]): wait m_eepromThread", __FUNCTION__, __LINE__);
            m_eepromThread->requestExitAndWait();
        } else {
            CLOGI("INFO(%s[%d]): m_eepromThread is NULL", __FUNCTION__, __LINE__);
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

#ifdef SAMSUNG_QUICK_SWITCH
bool ExynosCamera::m_preStartPictureInternalThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int ret = 0;

    m_timer.start();
    if (m_parameters->isReprocessing() == true) {
        if (m_reprocessingFrameFactory->isCreated() == false) {
            ret = m_reprocessingFrameFactory->create();
            if (ret != NO_ERROR) {
                m_timer.stop();
                durationTime = m_timer.durationMsecs();
                CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->create() failed", __FUNCTION__, __LINE__);
                return false;
            }
            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s[%d]): duration time(%5d msec)", __FUNCTION__, __LINE__, (int)m_timer.durationMsecs());

    return ret;
}
#endif

#ifdef SAMSUNG_UNIPLUGIN
bool ExynosCamera::m_uniPluginThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();
#ifdef SAMSUNG_LLV
#ifdef SAMSUNG_LLV_TUNING
    m_LLVpowerLevel = UNI_PLUGIN_POWER_CONTROL_OFF;
    m_checkLLVtuning();
#endif
    m_LLVstatus = LLV_UNINIT;
    m_LLVpluginHandle = uni_plugin_load(LLV_PLUGIN_NAME);
    if (m_LLVpluginHandle == NULL) {
        CLOGE("[LLV](%s[%d]): LLV plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_LENS_DC
    m_DCpluginHandle = NULL;
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        m_DCpluginHandle = uni_plugin_load(LDC_PLUGIN_NAME);
        if (m_DCpluginHandle == NULL){
            CLOGE("[DC](%s[%d]): DC plugin load failed!!", __FUNCTION__, __LINE__);
        }
    }
#endif
#ifdef SAMSUNG_OT
    m_OTpluginHandle = NULL;
    m_OTstatus = UNI_PLUGIN_OBJECT_TRACKING_IDLE;
    m_OTstart = OBJECT_TRACKING_IDLE;
    m_OTisTouchAreaGet = false;
    m_OTisWait = false;
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        m_OTpluginHandle = uni_plugin_load(OBJECT_TRACKING_PLUGIN_NAME);
        if (m_OTpluginHandle == NULL) {
            CLOGE("[OBTR](%s[%d]): Object Tracking plugin load failed!!", __FUNCTION__, __LINE__);
        }
    } else {
        CLOGD("[OBTR](%s[%d]): Object Tracking plugin no load for front camera", __FUNCTION__, __LINE__);
    }
#endif
#ifdef SAMSUNG_LBP
    m_LBPindex = 0;
    m_LBPCount = 0;
    m_isLBPlux = false;
    m_isLBPon = false;
    m_LBPpluginHandle = uni_plugin_load(BESTPHOTO_PLUGIN_NAME);
    if (m_LBPpluginHandle == NULL) {
        CLOGE("[LBP](%s[%d]):Bestphoto plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif
#ifdef SAMSUNG_JQ
    m_isJpegQtableOn = false;
    m_JQpluginHandle = uni_plugin_load(JPEG_QTABLE_PLUGIN_NAME);
    if (m_JQpluginHandle == NULL) {
        CLOGE("[JQ](%s[%d]):JpegQtable plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif
#ifdef SAMSUNG_BD
    m_BDpluginHandle = uni_plugin_load(BLUR_DETECTION_PLUGIN_NAME);
    if (m_BDpluginHandle == NULL) {
        CLOGE("[BD](%s[%d]):BlurDetection plugin load failed!!", __FUNCTION__, __LINE__);
    }
    m_BDstatus = BLUR_DETECTION_IDLE;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
#ifdef FLASHED_LLS_CAPTURE
    m_LLSpluginHandle = uni_plugin_load(FLASHED_LLS_PLUGIN_NAME);
    if (m_LLSpluginHandle == NULL) {
        CLOGE("[LLS_MBR](%s[%d]):FLASHED LLS plugin load failed!!", __FUNCTION__, __LINE__);
    }
#else
    m_LLSpluginHandle = uni_plugin_load(LLS_PLUGIN_NAME);
    if (m_LLSpluginHandle == NULL) {
        CLOGE("[LLS_MBR](%s[%d]):LLS plugin load failed!!", __FUNCTION__, __LINE__);
    }
#endif
#endif
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    return false;
}
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_sensorListenerThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;

    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);
    if(curMinFps > 60 && curMaxFps > 60)
        goto skip_sensor_listener;

    if (m_parameters->getSamsungCamera() == true
        && m_parameters->getVtMode() == 0
#ifdef SAMSUNG_QUICK_SWITCH
        && m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY
#endif
       ) {
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
#ifdef SAMSUNG_HRM
            if (m_uv_rayHandle == NULL) {
                m_uv_rayHandle = sensor_listener_load();
                if (m_uv_rayHandle != NULL) {
                    if (sensor_listener_enable_sensor(m_uv_rayHandle, ST_UV_RAY, 10 * 1000) < 0) {
                        sensor_listener_unload(&m_uv_rayHandle);
                    } else {
                        m_parameters->m_setHRM_Hint(true);
                    }
                }
            } else {
                m_parameters->m_setHRM_Hint(true);
            }
#endif

#ifdef SAMSUNG_GYRO
            if (m_gyroHandle == NULL) {
                m_gyroHandle = sensor_listener_load();
                if (m_gyroHandle != NULL) {
                    /* HACK
                     * Below event rate(7700) is set to let gyro timestamp operate
                     * for VDIS performance. Real event rate is 10ms not 7.7ms
                     */
                    if (sensor_listener_enable_sensor(m_gyroHandle,ST_GYROSCOPE, 100 * 1000) < 0) {
                        sensor_listener_unload(&m_gyroHandle);
                    } else {
                        m_parameters->m_setGyroHint(true);
                    }
                }
            } else {
                m_parameters->m_setGyroHint(true);
            }
#endif

#ifdef SAMSUNG_ACCELEROMETER
            if (m_accelerometerHandle == NULL) {
                m_accelerometerHandle = sensor_listener_load();
                if (m_accelerometerHandle != NULL) {
                    if (sensor_listener_enable_sensor(m_accelerometerHandle,ST_ACCELEROMETER, 100 * 1000) < 0) {
                        sensor_listener_unload(&m_accelerometerHandle);
                    } else {
                        m_parameters->m_setAccelerometerHint(true);
                    }
                }
            } else {
                m_parameters->m_setAccelerometerHint(true);
            }
#endif
        }

        if (getCameraIdInternal() == CAMERA_ID_FRONT) {
#ifdef SAMSUNG_LIGHT_IR
            if (m_light_irHandle == NULL) {
                m_light_irHandle = sensor_listener_load();
                if (m_light_irHandle != NULL) {
                    if (sensor_listener_enable_sensor(m_light_irHandle, ST_LIGHT_IR, 120 * 1000) < 0) {
                        sensor_listener_unload(&m_light_irHandle);
                    } else {
                        m_parameters->m_setLight_IR_Hint(true);
                    }
                }
            } else {
                m_parameters->m_setLight_IR_Hint(true);
            }
#endif
        }
    }

skip_sensor_listener:
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    return false;
}
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SAMSUNG_LLV_TUNING
status_t ExynosCamera::m_checkLLVtuning(void)
{
    FILE *fp = NULL;
    int i;
    char filePath[30];
    status_t ret = NO_ERROR;

    for(i = UNI_PLUGIN_POWER_CONTROL_OFF; i <= UNI_PLUGIN_POWER_CONTROL_4; i++) {
        memset(filePath, 0, sizeof(filePath));
        sprintf(filePath, "%s_%d.txt", "/mnt/sdcard/LLVKEY", i);
        fp = fopen(filePath, "r");
        if (fp == NULL) {
            CLOGE("(%s[%d]):failed to open LLVKEY_%d", __FUNCTION__, __LINE__, i);
        }
        else {
            CLOGD("(%s[%d]):succeeded to open LLVKEY_%d", __FUNCTION__, __LINE__, i);
            m_LLVpowerLevel = i;
            fclose(fp);
            break;
        }
    }

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_HLV
status_t ExynosCamera::m_ProgramAndProcessHLV(ExynosCameraBuffer *FrameBuffer)
{
    UniPluginBufferData_t BufferInfo;
    int curVideoW = 0, curVideoH = 0;
    int numValid = 0;
    int ret = 0;
    UniPluginFocusData_t *focusData = NULL;

    if (FrameBuffer == NULL) {
        ALOGE("HLV: [%s-%d]: FrameBuffer is NULL - Exit", __FUNCTION__, __LINE__);
        return -1;
    }

    int maxNumFocusAreas = m_parameters->getMaxNumFocusAreas();
    if (maxNumFocusAreas <= 0) {
        ALOGE("HLV: [%s-%d]: maxNumFocusAreas is <= 0 - Exit", __FUNCTION__, __LINE__);
        return -1;
    }

    focusData = new UniPluginFocusData_t[maxNumFocusAreas];

    memset(&BufferInfo, 0, sizeof(UniPluginBufferData_t));
    memset(focusData, 0, sizeof(UniPluginFocusData_t) *  maxNumFocusAreas);

    m_parameters->getVideoSize(&curVideoW, &curVideoH);

    BufferInfo.InWidth = curVideoW;
    BufferInfo.InHeight = curVideoH;
    BufferInfo.InBuffY = FrameBuffer->addr[0];
    BufferInfo.InBuffU = FrameBuffer->addr[1];
    BufferInfo.ZoomLevel = m_parameters->getZoomLevel();
    BufferInfo.Timestamp = m_lastRecordingTimeStamp / 1000000;  /* in MilliSec */

    ALOGV("HLV: [%s-%d]: uni_plugin_set BUFFER_INFO", __FUNCTION__, __LINE__);
    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_BUFFER_INFO,
                &BufferInfo);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_set fail!");


    ALOGV("HLV: uni_plugin_set FOCUS_INFO");

#ifdef SAMSUNG_OT
    if (m_parameters->getObjectTrackingEnable()) {
        ExynosRect2 *objectTrackingArea = new ExynosRect2[maxNumFocusAreas];
        int* objectTrackingWeight = new int[maxNumFocusAreas];
        int validNumber;

        m_parameters->getObjectTrackingAreas(&validNumber,
            objectTrackingArea, objectTrackingWeight);

        for (int i = 0; i < validNumber; i++) {
            focusData[i].FocusROILeft = objectTrackingArea[i].x1;
            focusData[i].FocusROIRight = objectTrackingArea[i].x2;
            focusData[i].FocusROITop = objectTrackingArea[i].y1;
            focusData[i].FocusROIBottom = objectTrackingArea[i].y2;
            focusData[i].FocusWeight = objectTrackingWeight[i];
        }

        if (validNumber > 1)
            ALOGW("HLV: Valid Focus Area > 1");

        delete[] objectTrackingArea;
        delete[] objectTrackingWeight;
    } else {
        int FocusAreaWeight = 0;
        ExynosRect2 CurrentFocusArea;
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        memset(&CurrentFocusArea, 0, sizeof(ExynosRect2));
        autoFocusMgr->getFocusAreas(&CurrentFocusArea, &FocusAreaWeight);

        focusData[0].FocusROILeft = CurrentFocusArea.x1;
        focusData[0].FocusROIRight = CurrentFocusArea.x2;
        focusData[0].FocusROITop = CurrentFocusArea.y1;
        focusData[0].FocusROIBottom = CurrentFocusArea.y2;
        focusData[0].FocusWeight = FocusAreaWeight;

        ALOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
            CurrentFocusArea.x1, CurrentFocusArea.x2,
            CurrentFocusArea.y1, CurrentFocusArea.y2);
    }
#else
    int FocusAreaWeight = 0;
    ExynosRect2 CurrentFocusArea;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

    memset(&CurrentFocusArea, 0, sizeof(ExynosRect2));
    autoFocusMgr->getFocusAreas(&CurrentFocusArea, &FocusAreaWeight);

    focusData[0].FocusROILeft = CurrentFocusArea.x1;
    focusData[0].FocusROIRight = CurrentFocusArea.x2;
    focusData[0].FocusROITop = CurrentFocusArea.y1;
    focusData[0].FocusROIBottom = CurrentFocusArea.y2;
    focusData[0].FocusWeight = FocusAreaWeight;

    ALOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
        CurrentFocusArea.x1, CurrentFocusArea.x2,
        CurrentFocusArea.y1, CurrentFocusArea.y2);
#endif

    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_FOCUS_INFO,
                focusData);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_set fail!");

    ret = uni_plugin_process(m_HLV);
    if (ret < 0)
        ALOGE("HLV: uni_plugin_process FAIL");

    delete[] focusData;

    return ret;
}
#endif

void ExynosCamera::m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext) {
    uint32_t data = 0;

    if (m_checkFirstFrameLux == false || m_parameters->getDualMode() == true ||
        m_parameters->getRecordingHint() == true) {
        m_checkFirstFrameLux = false;
        return;
    }

    data = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[399];

    if (data <= ENTRANCE_LOW_LUX) {
        ALOGD("(%s[%d]): need skip frame for ae/awb stable(%d).", __FUNCTION__, __LINE__, data);
        m_parameters->setFrameSkipCount(2);
    }
    m_checkFirstFrameLux = false;
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
    m_fdCallbackToggle = 0;

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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    ExynosCameraBufferManager *fusionBufferManager[MAX_NODE];
#endif
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        fusionBufferManager[i] = NULL;
#endif
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
            if (m_parameters->getIntelligentMode() == 1)
                m_previewFrameFactory->setRequestFLITE(false);
            else
                m_previewFrameFactory->setRequestFLITE(true);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
            CLOGD("DEBUG(%s[%d]): Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON", __FUNCTION__, __LINE__);
            m_previewFrameFactory->setRequestFLITE(false);
            if (m_parameters->getIntelligentMode() == 1)
                m_previewFrameFactory->setRequest3AC(false);
            else
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
            m_previewFrameFactory->setRequestFLITE(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
    }

    if (m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
        m_previewFrameFactory->setRequestFLITE(true);

#ifdef SUPPORT_DEPTH_MAP
    m_previewFrameFactory->setRequest(PIPE_VC1, false);
    m_parameters->setDisparityMode(COMPANION_DISPARITY_OFF);

    if (m_parameters->getUseDepthMap()
        && m_parameters->getPaf() == COMPANION_PAF_ON
        && m_parameters->getRecordingHint() == false
        && (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS
#ifdef SAMSUNG_FOOD_MODE
            || m_parameters->getSceneMode() == SCENE_MODE_FOOD
#endif
            )) {
        if (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS) {
            m_previewFrameFactory->setRequest(PIPE_VC1, true);
            m_parameters->setDepthCallbackOnCapture(true);
            m_parameters->setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER);
            CLOGI("INFO(%s[%d])[Depth] Enable depth callback on capture", __FUNCTION__, __LINE__);
        }
#ifdef SAMSUNG_FOOD_MODE
        else if (m_parameters->getSceneMode() == SCENE_MODE_FOOD
                    && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
            m_previewFrameFactory->setRequest(PIPE_VC1, true);
            m_parameters->setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER);
            m_parameters->setDepthCallbackOnPreview(true);
            CLOGI("INFO(%s[%d])[Depth] Enable depth callback on preview ", __FUNCTION__, __LINE__);
        }
#endif
    }
#endif

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

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        scpBufferMgr = m_zoomScalerBufferMgr;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    } else if (m_parameters->isFusionEnabled() == true) {
        scpBufferMgr = m_fusionBufferMgr;
#endif
    } else {
        scpBufferMgr = m_scpBufferMgr;
    }

    tempBufferManager = taaBufferManager;

    if (m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
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
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = m_scpBufferMgr;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true)
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = m_fusionBufferMgr;
#endif
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {
        int pipeId = PIPE_FUSION;

        ret = m_getBufferManager(pipeId, &fusionBufferManager[OUTPUT_NODE], SRC_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        ret = m_getBufferManager(pipeId, &fusionBufferManager[CAPTURE_NODE], DST_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        for (int i = 0; i < MAX_NODE; i++) {
            /* If even one buffer slot is valid. call setBufferManagerToPipe() */
            if (fusionBufferManager[i] != NULL) {
                ret = m_previewFrameFactory->setBufferManagerToPipe(fusionBufferManager, pipeId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(fusionBufferManager, %d) failed",
                        __FUNCTION__, __LINE__, pipeId);
                    return ret;
                }
                break;
            }
        }
    }
#endif

    int flipHorizontal = m_parameters->getFlipHorizontal();

    CLOGD("DEBUG(%s [%d]):set FlipHorizontal on preview, FlipHorizontal(%d)", __FUNCTION__, __LINE__, flipHorizontal);

    enum NODE_TYPE nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC2);
    m_previewFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_ISP, nodeType);

#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    if (m_parameters->getCameraDirection() == CAMERA_FACING_DIRECTION_BACK) {
        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC2);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, PIPE_ISP, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_ISP, nodeType);

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC1);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, 1, PIPE_ISP, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_ISP, nodeType);
#endif

        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, 1, PIPE_ISP, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_ISP, nodeType);
    }
#endif

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
        || (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
            && m_parameters->getIntelligentMode() != 1)) {
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
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
            || (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
                && m_parameters->getIntelligentMode() != 1)) {
            if (i < minBayerFrameNum) {
                ret = m_setupEntity(PIPE_FLITE, newFrame);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):setupEntity fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, PIPE_FLITE, ret);

#ifdef SUPPORT_DEPTH_MAP
                if (newFrame->getRequest(PIPE_VC1) == true) {
                    if (m_depthMapBufferMgr != NULL
                            && m_depthMapBufferMgr->getNumOfAvailableBuffer() > 0) {
                        int bufIndex = -1;

                        ret = m_depthMapBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuf);
                        if (ret == NO_ERROR && bufIndex >= 0) {
                            ret = newFrame->setDstBufferState(PIPE_FLITE, ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_2);
                            if (ret == NO_ERROR) {
                                ret = newFrame->setDstBuffer(PIPE_FLITE, dstBuf, CAPTURE_NODE_2);
                                if (ret != NO_ERROR) {
                                    CLOGE("ERR(%s[%d]):Failed to setDstBuffer. PIPE_FLITE, CAPTURE_NODE_2",
                                            __FUNCTION__, __LINE__);
                                    newFrame->setRequest(PIPE_VC1, false);
                                }
                            } else {
                                CLOGE("ERR(%s[%d]):Failed to setDstBufferState with REQUESTED. PIPE_FLITE, CAPTURE_NODE_2",
                                        __FUNCTION__, __LINE__);
                                newFrame->setRequest(PIPE_VC1, false);
                            }

                        } else {
                            CLOGW("WARN(%s[%d]):Failed to get DepthMap buffer. ret %d bufferIndex %d",
                                    __FUNCTION__, __LINE__, ret, bufIndex);
                            newFrame->setRequest(PIPE_VC1, false);
                        }
                    } else {
                        CLOGW("WARN(%s[%d]):Failed to get DepthMap buffer. DepthMapBufferMgr %p availableBuffercount %d",
                                __FUNCTION__, __LINE__,
                                m_depthMapBufferMgr,
                                m_depthMapBufferMgr->getNumOfAvailableBuffer());
                        newFrame->setRequest(PIPE_VC1, false);
                    }
                }
#endif

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

#ifdef RAWDUMP_CAPTURE
    /* s_ctrl HAL version for selecting dvfs table */
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2, PIPE_3AA);
    ALOGD("WARN(%s): V4L2_CID_IS_HAL_VERSION_%d pipe(%d)", __FUNCTION__, IS_HAL_VER_3_2, PIPE_3AA);
    if (ret < 0)
        ALOGW("WARN(%s): V4L2_CID_IS_HAL_VERSION is fail", __FUNCTION__);
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

#ifdef SAMSUNG_OIS
    if (m_parameters->getOISModeSetting() == true) {
        CLOGD("set OIS Mode(%s[%d])", __FUNCTION__, __LINE__);
        m_parameters->setOISMode();
        m_parameters->setOISModeSetting(false);
    }
#endif

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
    ret = m_clearList(&m_processList);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_clearList fail", __FUNCTION__, __LINE__);
        return ret;
    }

    /* clear previous recording frame */
    CLOGD("DEBUG(%s[%d]):Recording m_recordingProcessList(%d) IN",
            __FUNCTION__, __LINE__, m_recordingProcessList.size());
    m_recordingListLock.lock();
    ret = m_clearList(&m_recordingProcessList);
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_fusionThread->stop();
    m_fusionQ->sendCmd(WAKE_UP);
    m_fusionThread->requestExitAndWait();
#endif

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
        CLOGE("ERR(%s[%d]):m_stopPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    if (m_previewQ != NULL) {
        m_clearList(m_previewQ);
    }

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_Mode) {
        if (m_swVDIS_BufferMgr != NULL)
            m_swVDIS_BufferMgr->deinit();

        if (m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
        }
        VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d",
                    m_exynosconfig->current->bufInfo.num_preview_buffers);

        if (m_previewDelayQ != NULL) {
            m_clearList(m_previewDelayQ);
        }
    }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
    if (m_hyperMotionModeGet() == true) {
        if (m_hyperMotion_BufferMgr != NULL)
            m_hyperMotion_BufferMgr->deinit();

        if (m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS;
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
            m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_PREVIEW_BUFFERS;
        }
        CLOGD("HyperMotion_HAL: Preview Buffer Count Change to %d",
                    m_exynosconfig->current->bufInfo.num_preview_buffers);
    }
#endif /*SAMSUNG_HYPER_MOTION*/

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
    if (reservedMemoryCount > 0) {
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
        if (m_postPictureBufferMgr->getContigBufCount() != 0) {
            m_postPictureBufferMgr->deinit();
        } else {
            m_postPictureBufferMgr->resetBuffers();
        }
    }

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->resetBuffers();
    }
#endif

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
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }
#endif

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

#ifdef SAMSUNG_COMPANION
    if(isCompanion(getCameraIdInternal()) == true) {
        if (m_parameters->getSceneMode() == SCENE_MODE_HDR)
            m_parameters->checkSceneRTHdr(true);
        else
            m_parameters->checkSceneRTHdr(false);
    }
#endif

    if (flagUpdateParam == true && params != NULL)
        m_parameters->setParameters(*params);

    ret = setPreviewWindow(m_previewWindow);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setPreviewWindow fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    CLOGI("INFO(%s[%d]):setBuffersThread is run", __FUNCTION__, __LINE__);
    m_setBuffersThread->run(PRIORITY_DEFAULT);

    if (m_parameters->checkEnablePicture() == true) {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);

    }

    m_setBuffersThread->join();

    if (m_isSuccessedBufferAllocation == false) {
        CLOGE("ERR(%s[%d]):m_setBuffersThread() failed", __FUNCTION__, __LINE__);
        err = INVALID_OPERATION;
    }

    m_startPictureBufferThread->join();

    if (m_parameters->isReprocessing() == true && m_parameters->checkEnablePicture() == true) {
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
        CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
    }

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):m_startPreviewInternal fail", __FUNCTION__, __LINE__);
        err = ret;
    }

    /* setup frame thread */
    if (m_parameters->getDualMode() == true && getCameraIdInternal() == CAMERA_ID_FRONT) {
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->isFusionEnabled() == true) {
        CLOGD("DEBUG(%s[%d]):m_fusionThread start", __FUNCTION__, __LINE__);
        m_fusionThread->run(PRIORITY_DEFAULT);
    }
#endif

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
        } else {
            m_pictureFrameFactory = m_reprocessingFrameFactory;
            CLOGD("DEBUG(%s[%d]):FrameFactory(pictureFrameFactory) created", __FUNCTION__, __LINE__);
        }

        int flipHorizontal = m_parameters->getFlipHorizontal();

        CLOGD("DEBUG(%s [%d]):set FlipHorizontal on capture, FlipHorizontal(%d)", __FUNCTION__, __LINE__, flipHorizontal);

        enum NODE_TYPE nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC2_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);

        nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
        m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);

#ifdef USE_MULTI_FACING_SINGLE_CAMERA
        if (m_parameters->getCameraDirection() == CAMERA_FACING_DIRECTION_BACK) {
            nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC2_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);
            m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_3AA_REPROCESSING, nodeType);

            nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);
            m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_3AA_REPROCESSING, nodeType);

            nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, PIPE_3AA_REPROCESSING, nodeType);
            m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, PIPE_3AA_REPROCESSING, nodeType);
         }
#endif

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
        tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_MCSC2_REPROCESSING)] = m_postPictureBufferMgr;
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

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureThread->join();
#endif

#ifdef SAMSUNG_MAGICSHOT
    if (m_parameters->getShotMode() == SHOT_MODE_MAGIC) {
        m_postPictureCallbackThread->join();
    }
#endif

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

#ifdef SAMSUNG_MAGICSHOT
    m_dstPostPictureGscQ->release();
#endif

#ifdef UVS
    dstUvsReprocessingQ->release();
#endif

    m_dstJpegReprocessingQ->release();

    m_jpegCallbackQ->release();
    m_yuvCallbackQ->release();

#ifdef SAMSUNG_DNG
    m_dngCaptureQ->release();
    m_dngCaptureDoneQ->release();
#endif

#ifdef SAMSUNG_BD
    m_BDbufferQ->release();

    if (m_BDpluginHandle != NULL && m_BDstatus == BLUR_DETECTION_INIT) {
        int ret = uni_plugin_deinit(m_BDpluginHandle);
        if (ret < 0)
            CLOGE("[BD](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);

        for (int i = 0; i < MAX_BD_BUFF_NUM; i++) {
            if (m_BDbuffer[i].data != NULL)
                delete []m_BDbuffer[i].data;
        }
    }
    m_BDstatus = BLUR_DETECTION_DEINIT;
#endif

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

    ret = m_clearList(&m_postProcessList);
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
        ExynosCameraFrame *newFrame,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf,
        bool copybuffer)
{
    CLOGV("DEBUG(%s): converting preview to callback buffer copybuffer(%d)", __FUNCTION__, copybuffer);

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
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    if (!copybuffer) {
        ret = m_setCallbackBufferInfo(&callbackBuf, (char *)previewCallbackHeap->data);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): setCallbackBufferInfo fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        if (m_flagThreadStop == true || m_previewEnabled == false) {
            CLOGE("ERR(%s[%d]): preview was stopped!", __FUNCTION__, __LINE__);
            statusRet = INVALID_OPERATION;
            goto done;
        }

        memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));
#ifdef SAMSUNG_TIMESTAMP_BOOT
        m_Metadata.timestamp = newFrame->getTimeStampBoot();
#else
        m_Metadata.timestamp = newFrame->getTimeStamp();
#endif
        CLOGV("INFO(%s[%d]): timestamp:%lldms!", __FUNCTION__, __LINE__, m_Metadata.timestamp);

        ret = m_calcPreviewGSCRect(&srcRect, &dstRect);

        if (useCSC) {
            int pushFrameCnt = 0, doneFrameCnt = 0;
            int retryCnt = 5;
            bool isOldFrame = false;

            ret = newFrame->setSrcRect(pipeId, &srcRect);
            ret = newFrame->setDstRect(pipeId, &dstRect);

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
        if (m_parameters->getVRMode() == 1 || m_parameters->getMotionPhotoOn()) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME|CAMERA_MSG_PREVIEW_METADATA, previewCallbackHeap, 0, &m_Metadata, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        } else {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }
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
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
#endif

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;
#ifdef SUPPORT_DEPTH_MAP
    depthMapBuffer.index = -2;
#endif

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


#if defined(SAMSUNG_JQ) && defined(ONE_SECOND_BURST_CAPTURE)
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("[JQ](%s[%d]): ON!!", __FUNCTION__, __LINE__);
        m_isJpegQtableOn = true;
        m_parameters->setJpegQtableOn(true);
    }
#endif

    /* Check available buffer */
    ret = m_getBufferManager(prePictureDonePipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getBufferManager. pipeId %d direciton %d ret %d",
                __FUNCTION__, __LINE__,
                prePictureDonePipeId, DST_BUFFER_DIRECTION, ret);
        goto CLEAN;
    }

    if (bufferMgr != NULL) {
        ret = m_checkBufferAvailable(prePictureDonePipeId, bufferMgr);
        if (ret != NO_ERROR) {
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

    if (m_isZSLCaptureOn
#ifdef OIS_CAPTURE
        || m_OISCaptureShutterEnabled
#endif
    ) {
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
    ret = m_getBayerBuffer(m_getBayerPipeId(), &bayerBuffer, updateDmShot
#ifdef SUPPORT_DEPTH_MAP
                            , &depthMapBuffer
#endif
                            );

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

#ifdef SAMSUNG_LBP
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
        ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        m_LBPThread->requestExitAndWait();
        m_lbpBufferMgr->resetBuffers();
        if(m_LBPpluginHandle != NULL) {
            ret = uni_plugin_deinit(m_LBPpluginHandle);
            if(ret < 0)
                CLOGE("[LBP](%s[%d]):Bestphoto plugin deinit failed!!", __FUNCTION__, __LINE__);
        }
        m_LBPindex = 0;
        m_isLBPon = false;
    }
#endif

    if (m_parameters->getOutPutFormatNV21Enable()) {
       if ((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount() && m_parameters->getSeriesShotMode() > 0)
#ifdef SAMSUNG_LLS_DEBLUR
            || (m_parameters->getLDCaptureCount() == m_reprocessingCounter.getCount() && m_parameters->getLDCaptureMode() > 0)
#endif
#ifdef SAMSUNG_LENS_DC
            || m_parameters->getLensDCEnable()
#endif
            || m_parameters->getHighResolutionCallbackMode()
            || m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21
        ) {
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC2_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                if (m_parameters->isUseHWFC()) {
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
                if (m_parameters->isUseHWFC()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                    CLOGD("DEBUG(%s[%d]):enable hwFC request", __FUNCTION__, __LINE__);
                }

                if (m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
                    || m_parameters->getShotMode() == SHOT_MODE_MAGIC
#endif
#ifdef SAMSUNG_BD
                    || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
                ) {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC2_REPROCESSING, true);
                } else {
                    m_reprocessingFrameFactory->setRequest(PIPE_MCSC2_REPROCESSING, false);
                }
            }
        }
    }

    CLOGD("DEBUG(%s[%d]):bayerBuffer index %d", __FUNCTION__, __LINE__, bayerBuffer.index);

    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_captureSelector->clearList(m_getBayerPipeId(), false, m_previewFrameFactory->getNodeType(PIPE_3AC));
    }

    if (!m_isZSLCaptureOn
#ifdef OIS_CAPTURE
        && !m_OISCaptureShutterEnabled
#endif
#ifdef BURST_CAPTURE
        && (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST
            || m_burstShutterLocation == BURST_SHUTTER_PREPICTURE)
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
        && (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST
            || m_burstShutterLocation == BURST_SHUTTER_PREPICTURE)
#endif
#ifdef FLASHED_LLS_CAPTURE /* Last Capture Frame */
        && !(m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_FLASHED_LLS
            && m_reprocessingCounter.getCount() > 1)
#endif
    ) {
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();

#ifdef BURST_CAPTURE
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST)
            m_burstShutterLocation = BURST_SHUTTER_PREPICTURE_DONE;
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST)
            m_burstShutterLocation = BURST_SHUTTER_PREPICTURE_DONE;
#endif
    }

#ifdef OIS_CAPTURE
    m_OISCaptureShutterEnabled = false;
#endif

    m_isZSLCaptureOn = false;

#ifdef OIS_CAPTURE
if ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS ||
        m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_SIS) &&
        m_reprocessingCounter.getCount() == 1) {
            CLOGD("DEBUG(%s[%d]):LLS last capture frame", __FUNCTION__, __LINE__);
#ifndef LLS_REPROCESSING
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
#else
            m_parameters->setOISCaptureModeOn(false);
#endif
    }
#endif

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
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw",m_cameraId, m_fliteFrameCount);
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

#ifdef SUPPORT_DEPTH_MAP
    if (depthMapBuffer.index != -2 && m_parameters->getDepthCallbackOnCapture()) {
        ret = newFrame->setDstBuffer(PIPE_VC1, depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
        }
        newFrame->setRequest(PIPE_VC1, true);

        CLOGD("DEBUG(%s[%d]):[Depth] reprocessing frame buffer setting getRequest(PIPE_VC1)(%d)",
                __FUNCTION__, __LINE__, newFrame->getRequest(PIPE_VC1));
    }
#endif

    /*
     * If it is changed dzoom level in per-frame,
     * it might be occure difference crop region between selected bayer and commend.
     * When dirty bayer is used for reprocessing,
     * the bayer in frame selector is applied n level dzoom but take_picture commend is applied n+1 level dzoom.
     * So, we should re-set crop region value from bayer.
     */
    if (m_parameters->getUsePureBayerReprocessing() == false
#ifdef LLS_REPROCESSING
            && m_captureSelector->getIsConvertingMeta() == true
#endif
       ) {
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
    if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
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

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("[DNG](%s[%d]):Push frame to dngCaptureQ (fcount %d)",
                __FUNCTION__, __LINE__, getMetaDmRequestFrameCount(updateDmShot));
        m_dngFrameNumber = getMetaDmRequestFrameCount(updateDmShot);
        m_DNGCaptureThread->run();
        m_dngCaptureQ->pushProcessQ(&bayerBuffer);
    }
#endif

    /* Add frame to post processing list */
    newFrame->frameLock();
    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
        CLOGV("DEBUG(%s[%d]):does not use postPicList in highResolutionCB",
            __FUNCTION__, __LINE__, newFrame->getFrameCount());
    } else {
        CLOGD("DEBUG(%s[%d]): push frame(%d) to postPictureList",
            __FUNCTION__, __LINE__, newFrame->getFrameCount());
        m_postProcessList.push_back(newFrame);
    }

#ifdef SAMSUNG_JQ
    if (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST
        && m_parameters->getJpegQtableStatus() != JPEG_QTABLE_UPDATED) {
        CLOGV("INFO(%s):[JQ] WAIT JPEG_QTABLE_UPDATED", __FUNCTION__);
        m_JQ_mutex.lock();
        m_JQisWait = true;
        m_JQ_condition.waitRelative(m_JQ_mutex, 200000000); /* 0.2sec */
        m_JQisWait = false;
        m_JQ_mutex.unlock();
    }
#endif

    ret = m_reprocessingFrameFactory->startInitialThreads();
    if (ret < 0) {
        CLOGE("ERR(%s):startInitialThreads fail", __FUNCTION__);
        goto CLEAN;
    }

    if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
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
              && m_parameters->isUseHWFC() == true
              && m_parameters->getHWFCEnable() == true)) {
            ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);

                if (updateDmShot != NULL) {
                    delete updateDmShot;
                    updateDmShot = NULL;
                }

                return ret;
            }

            newFrame->setMetaDataEnable(true);
        }

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

#ifdef LLS_CAPTURE
        if (shot_ext != NULL) {
            if ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS ||
                 m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_SIS) &&
                 m_reprocessingCounter.getCount() == m_parameters->getSeriesShotCount()) {
                CLOGD("LLS(%s[%d]): lls_tuning_set_index(%d)", __FUNCTION__, __LINE__,
                        shot_ext->shot.dm.stats.vendor_lls_tuning_set_index);
                CLOGD("LLS(%s[%d]): lls_brightness_index(%d)", __FUNCTION__, __LINE__,
                        shot_ext->shot.dm.stats.vendor_lls_brightness_index);
                m_notifyCb(MULTI_FRAME_SHOT_PARAMETERS, shot_ext->shot.dm.stats.vendor_lls_tuning_set_index,
                        shot_ext->shot.dm.stats.vendor_lls_brightness_index, m_callbackCookie);
            }
        }
#endif

#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
        {
#ifdef SAMSUNG_DNG
            if (m_parameters->getDNGCaptureModeOn() == true) {
                ExynosCameraBuffer dngBayerBuffer;
                int retry = 0;

                CLOGD("DEBUG(%s[%d]):wait for DNG bayer buffer copy complete", __FUNCTION__, __LINE__);
                do {
                    dngBayerBuffer.index = -2;
                    ret = m_dngCaptureDoneQ->waitAndPopProcessQ(&dngBayerBuffer);
                    retry++;
                    if (retry % 3 == 0) {
                        CLOGW("WARN(%s[%d]): bayer copy for DNG not still complete, ret(%d), retry(%d)",
                                __FUNCTION__, __LINE__, ret, retry);
                    }
                } while (ret == TIMED_OUT && retry < 200);

                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):failed to wait for dng bayer copy done, ret(%d), retry(%d)",
                            __FUNCTION__, __LINE__, ret, retry);
                }
            }
#endif
            /* put bayer buffer */
            ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): 3AA src putBuffer fail, index(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerBuffer.index, ret);
                goto CLEAN;
            }
#ifdef LLS_REPROCESSING
            m_captureSelector->setIsConvertingMeta(true);
#endif
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

        /* Put YUV buffer */
        if (m_parameters->isUseHWFC() == true && m_parameters->getHWFCEnable() == true) {
            ExynosCameraBuffer yuvBuffer;
            /* Handle Main Buffer */
            ret = newFrame->getDstBuffer(bayerPipeId, &yuvBuffer,
                                         m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING));
            if (ret < NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getDstBuffer for Main. pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        bayerPipeId, ret);
                goto CLEAN;
            }

            ret = m_reprocessingYuvCallbackFunc(yuvBuffer);
            if (ret < NO_ERROR) {
                CLOGW("ERR(%s[%d]):[F%d]Failed to callback reprocessing YUV",
                        __FUNCTION__, __LINE__,
                        newFrame->getFrameCount());
            }

            ret = m_putBuffers(m_sccReprocessingBufferMgr, yuvBuffer.index);
            if (ret < NO_ERROR) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        bayerPipeId, ret);
                goto CLEAN;
            }

            /* Handle Thumbnail Buffer */
            ret = newFrame->getDstBuffer(bayerPipeId, &yuvBuffer,
                                         m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING));
            if (ret < NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getDstBuffer. pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        bayerPipeId, ret);
                goto CLEAN;
            }

#ifdef SAMSUNG_DNG
            if (m_parameters->getDNGCaptureModeOn()
                && m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST) {
                dng_thumbnail_t *dngThumbnailBuf = NULL;
                unsigned int thumbBufSize = 0;
                int thumbnailW = 0, thumbnailH = 0;
                struct camera2_dm *dm = new struct camera2_dm;

                m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);
                newFrame->getDynamicMeta(dm);

                dngThumbnailBuf = m_parameters->createDngThumbnailBuffer(thumbnailW * thumbnailH * 2);
                if (dngThumbnailBuf->buf && yuvBuffer.addr[0]) {
                    memcpy(dngThumbnailBuf->buf, yuvBuffer.addr[0], yuvBuffer.size[0]);
                    dngThumbnailBuf->size = yuvBuffer.size[0];

                    if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                        dngThumbnailBuf->frameCount = 0;
                    } else {
                        dngThumbnailBuf->frameCount = dm->request.frameCount;
                    }

                    m_parameters->putDngThumbnailBuffer(dngThumbnailBuf);

                    CLOGD("DEBUG(%s):[DNG] Thumbnail size:%d, fcount %d", __FUNCTION__,
                            dngThumbnailBuf->size, dngThumbnailBuf->frameCount);
                } else {
                    CLOGE("ERR(%s[%d]):[DNG] buffer is NULL(src:%p, dst:%p)",
                            __FUNCTION__, __LINE__, yuvBuffer.addr[0], dngThumbnailBuf->buf);
                }
                delete dm;
            }
#endif
            ret = m_putBuffers(m_thumbnailBufferMgr, yuvBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId %d ret %d",
                        __FUNCTION__, __LINE__,
                        bayerPipeId, ret);
                goto CLEAN;
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

#ifdef OIS_CAPTURE
        if (m_parameters->getOISCaptureModeOn() == true && m_reprocessingCounter.getCount() == 0) {
            m_parameters->setOISCaptureModeOn(false);
        }
#endif
        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_highResolutionCallbackQ->pushProcessQ(&newFrame);
        }

        if (newFrame != NULL) {
            CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount());
            newFrame->decRef();
            m_frameMgr->deleteFrame(newFrame);
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

#ifdef SUPPORT_DEPTH_MAP
        if (depthMapBuffer.index != -2 && m_depthMapBufferMgr != NULL) {
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

CLEAN:
    if (newFrame != NULL) {
        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE("ERR(%s[%d]):getBufferManager fail, ret(%d)", __FUNCTION__, __LINE__, ret);

#ifdef SAMSUNG_DNG
        if (m_DNGCaptureThread->isRunning() == true) {
            CLOGI("INFO(%s[%d]): wait m_DNGCaptureThread", __FUNCTION__, __LINE__);
            m_DNGCaptureThread->requestExitAndWait();
        }
#endif
        if (bufferMgr != NULL) {
            ret = newFrame->getSrcBuffer(bayerPipeId, &bayerBuffer);
            if (ret < 0)
                CLOGE("ERR(%s[%d]):getDstBuffer fail, bayerPipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, bayerPipeId, ret);

            if (bayerBuffer.index >= 0)
                m_putBuffers(bufferMgr, bayerBuffer.index);
        }

#ifdef SUPPORT_DEPTH_MAP
        if (depthMapBuffer.index != -2 && m_depthMapBufferMgr != NULL) {
            ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to put DepthMap buffer to bufferMgr",
                        __FUNCTION__, __LINE__);
            }
        }
#endif

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
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):remove frame from processList fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }

        newFrame->printEntity();
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
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

#ifdef OIS_CAPTURE
    if (m_parameters->getOISCaptureModeOn() == true && m_reprocessingCounter.getCount() == 0) {
        m_parameters->setOISCaptureModeOn(false);
    }
#endif

    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): Reprocessing frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == true)) {
        loop = true;
    }

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
            pipeId_scc = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;

        pipeId_gsc = PIPE_GSC_REPROCESSING;
        isSrc = true;
    } else {
        switch (getCameraIdInternal()) {
            case CAMERA_ID_FRONT:
                if (m_parameters->getDualMode() == true) {
                    pipeId_scc = PIPE_3AA;
                } else {
                    pipeId_scc = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC : PIPE_ISPC;
                }
                pipeId_gsc = PIPE_GSC_PICTURE;
                break;
            default:
                CLOGE("ERR(%s[%d]):Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    __FUNCTION__, __LINE__, getCameraIdInternal(), m_parameters->isReprocessing());
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
        && m_parameters->isUseHWFC() == true
        && m_parameters->getHWFCEnable() == true) {
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
    if (m_parameters->isReprocessing() == false) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback S", __FUNCTION__, __LINE__);
#ifdef BURST_CAPTURE
            m_notifyCb(CAMERA_MSG_SHUTTER, m_parameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
            CLOGI("INFO(%s[%d]): CAMERA_MSG_SHUTTER callback E", __FUNCTION__, __LINE__);
        }
    }

#ifdef SAMSUNG_DOF
    if (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS &&
            m_parameters->getMoveLensTotal() > 0) {
        CLOGD("[DOF][%s][%d] Lens moved count: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
        CLOGD("[DOF][%s][%d] Total count : %d",
                "m_pictureThreadFunc", __LINE__, m_parameters->getMoveLensTotal());

        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        if(m_lensmoveCount < m_parameters->getMoveLensTotal()) {
            CLOGD("[DOF][%s][%d] callback done!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(true);
            m_lensmoveCount++;
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->m_setLensPosition(m_lensmoveCount);
        }
        else if(m_lensmoveCount == m_parameters->getMoveLensTotal() ){
            CLOGD("[DOF][%s][%d] Last callback done!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(false);

            m_lensmoveCount = 0;
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->setMoveLensTotal(m_lensmoveCount);
        }else {
            CLOGE("[DOF][%s][%d] unexpected error!!: %d",
                "m_pictureThreadFunc", __LINE__, m_lensmoveCount);
            autoFocusMgr->setStartLensMove(false);
            m_lensmoveCount = 0;
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->setMoveLensTotal(m_lensmoveCount);
        }
    }
#endif

#ifdef SAMSUNG_DNG
    struct camera2_dm *dm;
    struct camera2_udm *udm;

    dm = new struct camera2_dm;
    udm = new struct camera2_udm;

    newFrame->getDynamicMeta(dm);
    newFrame->getUserDynamicMeta(udm);

    if(m_parameters->getDNGCaptureModeOn() == true
       && (m_dngFrameNumber == dm->request.frameCount)) {
        m_parameters->setDngChangedAttribute(dm, udm);
        m_parameters->setIsUsefulDngInfo(true);
    }

    delete dm;
    delete udm;
#endif

    if (m_parameters->getIsThumbnailCallbackOn()) {
        if (m_ThumbnailCallbackThread->isRunning()) {
            m_ThumbnailCallbackThread->join();
            CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
        }
    }

    if ((m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21)
         && (m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
                || m_parameters->getShotMode() == SHOT_MODE_MAGIC
#endif
#ifdef SAMSUNG_BD
                || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
       )) {
        int postPicturePipeId = PIPE_MCSC2_REPROCESSING;
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

    if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
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

        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
            || m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS
#ifdef SAMSUNG_LLS_DEBLUR
            || m_parameters->getLDCaptureMode()
#endif
          ) {
            pictureFormat = V4L2_PIX_FMT_NV21;
        } else {
            pictureFormat = JPEG_INPUT_COLOR_FMT;
        }

#ifdef SR_CAPTURE
        if(m_parameters->getSROn() &&
            (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
            m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)) {
            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = shot_stream->output_crop_region[2];
            dstRect.h = shot_stream->output_crop_region[3];
            dstRect.fullW = shot_stream->output_crop_region[2];
            dstRect.fullH = shot_stream->output_crop_region[3];
            dstRect.colorFormat = pictureFormat;
        } else
#endif
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
#ifdef SAMSUNG_BD
            || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
        ) {
            m_postPictureCallbackThread->join();
        }

#ifdef SR_CAPTURE
        if(m_parameters->getSROn()) {
            if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
                m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
                m_sr_cropped_width = shot_stream->output_crop_region[2];
                m_sr_cropped_height = shot_stream->output_crop_region[3];
            } else {
                m_sr_cropped_width = pictureW;
                m_sr_cropped_height = pictureH;
            }
            CLOGD("DEBUG(%s):size (%d, %d)", __FUNCTION__, m_sr_cropped_width, m_sr_cropped_height);
        }
#endif

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

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_parameters->getLDCaptureMode() > 0) {
        CLOGD("DEBUG(%s[%d]): m_LDCaptureQ push mode(%d)",
                __FUNCTION__, __LINE__, m_parameters->getLDCaptureMode());
        m_LDCaptureQ->pushProcessQ(&newFrame);
    } else
#endif
    {
        /* push postProcess */
        m_postPictureQ->pushProcessQ(&newFrame);
    }

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

                if (m_parameters->isOwnScc(getCameraIdInternal()) == true)
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
    if (1 < m_parameters->getSeriesShotCount()) {
        int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();

        if (seriesShotNumber < 0 || seriesShotNumber > m_parameters->getSeriesShotCount()) {
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
            char filePath[CAMERA_FILE_PATH_SIZE];
            int nw, cnt = 0;
            uint32_t written = 0;
            camera_memory_t *tempJpegCallbackHeap = NULL;

            memset(filePath, 0, sizeof(filePath));

#if defined(SAMSUNG_INF_BURST)
            snprintf(filePath, sizeof(filePath), "%s%s_%03d.jpg",
                    m_burstSavePath, m_burstTime, seriesShotNumber);
#else
            snprintf(filePath, sizeof(filePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
#endif
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
    int buffer_idx = m_getShotBufferIdex();
    int retryCountJPEG = 4;

    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraFrame *yuvFrame = NULL;
    ExynosCameraFrame *jpegFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;
    int pipeId = 0;

    int currentSeriesShotMode = 0;
    bool IsThumbnailCallback = false;

    exif_attribute_t exifInfo;
    ExynosRect pictureRect;
    ExynosRect thumbnailRect;

    m_parameters->getFixedExifInfo(&exifInfo);
    pictureRect.colorFormat = m_parameters->getHwPictureFormat();
    m_parameters->getPictureSize(&pictureRect.w, &pictureRect.h);
    m_parameters->getThumbnailSize(&thumbnailRect.w, &thumbnailRect.h);
    memset(m_picture_meta_shot, 0x00, sizeof(struct camera2_shot));

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
            pipeId_gsc = PIPE_GSC_REPROCESSING;
        } else {
            if (m_parameters->isOwnScc(getCameraIdInternal()) == false) {
                pipeId_gsc = (m_parameters->isReprocessing3aaIspOTF() == true) ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
            } else {
                pipeId_gsc = PIPE_SCC_REPROCESSING;
            }
        }
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        if (m_parameters->needGSCForCapture(getCameraIdInternal()) == true) {
            pipeId_gsc = PIPE_GSC_PICTURE;
        } else {
            if (m_parameters->isOwnScc(getCameraIdInternal()) == true) {
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

    newFrame->getUserDynamicMeta(&m_picture_meta_shot->udm);
    newFrame->getDynamicMeta(&m_picture_meta_shot->dm);

    /* Save the iso of capture frame to camera parameters for providing App/framework iso info. */
#ifdef SAMSUNG_TN_FEATURE
#ifdef ONE_SECOND_BURST_CAPTURE
    if (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
    {
        m_parameters->setParamExifInfo(&m_picture_meta_shot->udm);
    }
#endif

#ifdef LLS_REPROCESSING
    if(m_parameters->getSamsungCamera() && m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS) {
        CLOGD("DEBUG(%s[%d]): MULTI_FRAME_SHOT_BV_INFO BV value %u",
                __FUNCTION__, __LINE__, m_picture_meta_shot->udm.awb.vendorSpecific[11]);
        m_notifyCb(MULTI_FRAME_SHOT_BV_INFO, m_picture_meta_shot->udm.awb.vendorSpecific[11], 0, m_callbackCookie);
    }
#endif /* LLS_REPROCESSING */

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCEnable()) {
        int src_PipeId = PIPE_MCSC2_REPROCESSING;
        ExynosCameraBuffer srcReprocessingBuffer;
        ExynosCameraBufferManager *srcBufferMgr = NULL;
        int dstbufferIndex = -2;
        int retry = 0;
        ExynosCameraDurationTimer m_timer;
        long long durationTime = 0;

        bufferMgr = m_lensDCBufferMgr;

        CLOGI("[DC](%s[%d]):getLensDCEnable(%d)", __FUNCTION__, __LINE__, m_parameters->getLensDCEnable());
        ret = m_getBufferManager(src_PipeId, &srcBufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, src_PipeId, ret);
            goto CLEAN;
        }

        ret = newFrame->getDstBuffer(pipeId_gsc, &srcReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(src_PipeId));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }

        do {
            dstbufferIndex = -2;
            retry++;

            if (m_lensDCBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_lensDCBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &gscReprocessingBuffer);
                if (ret != NO_ERROR)
                    CLOGE("ERR(%s[%d]):getBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            if (dstbufferIndex < 0) {
                usleep(WAITING_TIME);
                if (retry % 20 == 0) {
                    CLOGW("WRN(%s[%d]):retry m_lensDCBuffer getBuffer)", __FUNCTION__, __LINE__);
                    m_lensDCBufferMgr->dump();
                }
            }
        } while (dstbufferIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_flagThreadStop != true && m_stopBurstShot == false);

        int pictureW = 0, pictureH = 0;
        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));

        m_parameters->getPictureSize(&pictureW, &pictureH);

        pluginData.InWidth = pictureW;
        pluginData.InHeight = pictureH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
        pluginData.InBuffY = srcReprocessingBuffer.addr[0];
        pluginData.InBuffU = srcReprocessingBuffer.addr[0] + (pictureW * pictureH);
        pluginData.OutBuffY = gscReprocessingBuffer.addr[0];
        pluginData.OutBuffU = gscReprocessingBuffer.addr[0] + (pictureW * pictureH);

        CLOGD("[DC](%s[%d]):-- Start Library --", __FUNCTION__, __LINE__);
        m_timer.start();
        if (m_DCpluginHandle != NULL) {
            UniPluginExtraBufferInfo_t extraData;
            memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));

            extraData.brightnessValue.num = m_picture_meta_shot->udm.ae.vendorSpecific[5];
            extraData.brightnessValue.den = 256;

            /* short ISO */
            extraData.iso[0] = m_picture_meta_shot->udm.ae.vendorSpecific[391];
            /* Long ISO */
            extraData.iso[1] = m_picture_meta_shot->udm.ae.vendorSpecific[393];

            ret = uni_plugin_set(m_DCpluginHandle,
                             LDC_PLUGIN_NAME,
                             UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO,
                             &extraData);
            if (ret < 0) {
                CLOGE("[DC](%s[%d]): DC Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_set(m_DCpluginHandle,
                    LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if(ret < 0) {
                CLOGE("[DC](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_process(m_DCpluginHandle);
            if (ret < 0) {
                CLOGE("[DC](%s[%d]): LLS Plugin process failed!!", __FUNCTION__, __LINE__);
            }

            UTstr debugData;
            debugData.data = new unsigned char[LDC_EXIF_SIZE];

            if (debugData.data != NULL) {
                ret = uni_plugin_get(m_DCpluginHandle,
                        LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
                if (ret < 0) {
                    CLOGE("[DC](%s[%d]): DC Plugin get UNI_PLUGIN_INDEX_DEBUG_INFO failed!!", __FUNCTION__, __LINE__);
                }

                CLOGD("[DC](%s[%d]): Debug buffer size: %d", __FUNCTION__, __LINE__, debugData.size);
                m_parameters->setLensDCdebugInfo(debugData.data, debugData.size);

                delete []debugData.data;
            }
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[DC](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
        CLOGD("[DC](%s[%d]):-- end Library --", __FUNCTION__, __LINE__);

        ret = m_putBuffers(srcBufferMgr, srcReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):srcbufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, src_PipeId, ret);
            goto CLEAN;
        }
    } else
#endif
    {
        if (m_parameters->getOutPutFormatNV21Enable()) {
            pipeId = PIPE_MCSC2_REPROCESSING;
            ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId, ret);
                goto CLEAN;
            }
        } else if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
            pipeId = PIPE_MCSC3_REPROCESSING;
            ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }
        }

        ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer,
                m_reprocessingFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
            goto CLEAN;
        }
    }

#if defined(SAMSUNG_LLS_DEBLUR) || defined(SAMSUNG_LENS_DC)
    if (m_parameters->getSamsungCamera()
        && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
#ifdef SAMSUNG_LLS_DEBLUR
        if (m_parameters->getLDCaptureMode()) {
            IsThumbnailCallback = true;
        }
#endif
#ifdef SAMSUNG_LENS_DC
        if (m_parameters->getLensDCEnable()) {
            IsThumbnailCallback = true;
        }
#endif
    }
#endif

    if (IsThumbnailCallback) {
        m_parameters->setIsThumbnailCallbackOn(true);
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&gscReprocessingBuffer);
        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread run", __FUNCTION__, __LINE__);
    }

#if 0
    {
        char filePath[70];
        int width, height = 0;

        newFrame->dumpNodeGroupInfo("");

        m_parameters->getPictureSize(&width, &height);

        CLOGE("INFO(%s[%d]):getPictureSize(%d x %d)", __FUNCTION__, __LINE__, width, height);

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/pic_%d.yuv", gscReprocessingBuffer.index);

        dumpToFile((char *)filePath, gscReprocessingBuffer.addr[0], width * height);
    }
#endif

    /* callback */
    if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
        ret = m_reprocessingYuvCallbackFunc(gscReprocessingBuffer);
        if (ret < NO_ERROR) {
            CLOGW("ERR(%s[%d]):[F%d]Failed to callback reprocessing YUV",
                    __FUNCTION__, __LINE__,
                    newFrame->getFrameCount());
        }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
        /*
         * In case of YUV capture,
         * HAL will callback YUV by CAMERA_MSG_COMPRESSED_IMAGE/
         */
        if (m_parameters->getUseJpegCallbackForYuv() == true &&
            m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE)) {

            camera_memory_t *rawCallbackHeap = NULL;
            rawCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
            if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
                goto CLEAN;
            }

            CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callback for YUV capture start", __FUNCTION__, __LINE__);

            setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
            m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, rawCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

            CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callback for YUV capture end", __FUNCTION__, __LINE__);

            rawCallbackHeap->release(rawCallbackHeap);
        }
#endif // BOARD_CAMERA_USES_DUAL_CAMERA
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

            yuvFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId, newFrame->getFrameCount());
            ret = yuvFrame->setDstBuffer(pipeId, gscReprocessingBuffer);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
            }

            ret = yuvFrame->storeDynamicMeta(&m_picture_meta_shot->dm);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            ret = yuvFrame->storeUserDynamicMeta(&m_picture_meta_shot->udm);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            }

#ifdef SUPPORT_DEPTH_MAP
            if (newFrame->getRequest(PIPE_VC1) == true) {
                ExynosCameraBuffer depthMapBuffer;

                ret = newFrame->getDstBuffer(PIPE_VC1, &depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
                    return ret;
                }

                ret = yuvFrame->setSrcBuffer(pipeId, depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):Failed to get DepthMap buffer", __FUNCTION__, __LINE__);
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

#ifdef SAMSUNG_BD
                if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
                    UTstr bdInfo = {NULL, 0};
                    ret = m_BDbufferQ->waitAndPopProcessQ(&bdInfo);
                    if(ret == TIMED_OUT || ret < 0) {
                        CLOGE("[BD](%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                    }
                    else {
                        CLOGD("[BD](%s[%d]): Pop the buffer(size: %d)", __FUNCTION__, __LINE__, bdInfo.size);
                        m_parameters->setBlurInfo(bdInfo.data, bdInfo.size);
                    }
                }
#endif
#ifdef SAMSUNG_UTC_TS
                m_parameters->setUTCInfo();
#endif
                ret = newFrame->getDstBuffer(pipeId_gsc, &jpegReprocessingBuffer,
                                                m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }

                /* in case OTF until JPEG, we should be call below function to this position
                                in order to update debugData from frame. */
                m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, m_picture_meta_shot);

#ifdef DEBUG_EXIF_OVERWRITE
                {
                    char filePath[70];
                    memset(filePath, 0, sizeof(filePath));

                    newFrame->getDynamicMeta(&m_picture_meta_shot->dm);
                    snprintf(filePath, sizeof(filePath), "/data/media/0/Before_IMAGE%d_%d.jpeg",
                    m_parameters->getCameraId(), m_picture_meta_shot->dm.request.frameCount);

                    ret = dumpToFile((char *)filePath,
                                    jpegReprocessingBuffer.addr[0],
                                    jpegReprocessingBuffer.size[0]);
                }
#endif

                /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
                if (jpegReprocessingBuffer.size[0] != 0) {
                    /* APP1 Marker - EXIF */
                    UpdateExif(jpegReprocessingBuffer.addr[0],
                               jpegReprocessingBuffer.size[0],
                               &exifInfo);
                    /* APP4 Marker - DebugInfo */
                    UpdateDebugData(jpegReprocessingBuffer.addr[0],
                                    jpegReprocessingBuffer.size[0],
                                    m_parameters->getDebugAttribute());
                }

#ifdef DEBUG_EXIF_OVERWRITE
                {
                    char filePath[70];
                    memset(filePath, 0, sizeof(filePath));

                    newFrame->getDynamicMeta(&m_picture_meta_shot->dm);
                    snprintf(filePath, sizeof(filePath), "/data/media/0/Before_IMAGE%d_%d.jpeg",
                    m_parameters->getCameraId(), m_picture_meta_shot->dm.request.frameCount);

                    ret = dumpToFile((char *)filePath,
                                    jpegReprocessingBuffer.addr[0],
                                    jpegReprocessingBuffer.size[0]);
                }
#endif
            } else {
                /* 1. get wait available JPEG src buffer */
                int retry = 0;
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
                        goto CLEAN;
                    }
                }

                if (IsThumbnailCallback) {
                    if (m_ThumbnailCallbackThread->isRunning()) {
                        m_ThumbnailCallbackThread->join();
                        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
                    }
                } 

                /* put GSC buffer */
                ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
                if (ret < 0) {
                    CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                            __FUNCTION__, __LINE__, pipeId_gsc, ret);
                    goto CLEAN;
                }
            }

            int jpegOutputSize = newFrame->getJpegSize();
            if (jpegOutputSize <= 0) {
                jpegOutputSize = jpegReprocessingBuffer.size[0];
                if (jpegOutputSize <= 0)
                    CLOGW("WRN(%s[%d]): jpegOutput size(%d) is invalid", __FUNCTION__, __LINE__, jpegOutputSize);
            }

            CLOGI("INFO(%s[%d]):Jpeg output done, jpeg size(%d)", __FUNCTION__, __LINE__, jpegOutputSize);

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

#ifdef BURST_CAPTURE
            m_burstCaptureCallbackCount++;
            CLOGI("INFO(%s[%d]): burstCaptureCallbackCount(%d)", __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
#endif

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

            ret = jpegFrame->storeDynamicMeta(&m_picture_meta_shot->dm);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            }

            ret = jpegFrame->storeUserDynamicMeta(&m_picture_meta_shot->udm);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): storeUserDynamicMeta fail ret(%d)", __FUNCTION__, __LINE__, ret);
            }

retry:
            if ((m_parameters->getSeriesShotCount() > 0)
#ifdef SAMSUNG_MAGICSHOT
                && !((m_parameters->getShotMode() == SHOT_MODE_MAGIC) && (m_burstCaptureCallbackCount > m_magicshotMaxCount))
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
                && (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
                ) {
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

        if (IsThumbnailCallback) {
            if (m_ThumbnailCallbackThread->isRunning()) {
                m_ThumbnailCallbackThread->join();
                CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
            }
        } 

        /* put GSC buffer */
        if (gscReprocessingBuffer.index >= 0) {
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, pipeId_gsc, ret);
                goto CLEAN;
            }
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

#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
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

#ifdef ONE_SECOND_BURST_CAPTURE
        if (!(currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
            currentSeriesShotMode == SERIES_SHOT_MODE_BURST))
#else
        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST)
#endif
        {
            CLOGD("DEBUG(%s[%d]): clearList postProcessList, series shot mode(%d)", __FUNCTION__, __LINE__, currentSeriesShotMode);
            if (m_clearList(&m_postProcessList) < 0) {
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
    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): Picture frame delete(%d)", __FUNCTION__, __LINE__, newFrame->getFrameCount());

        newFrame->frameUnlock();
        ret = m_removeFrameFromList(&m_postProcessList, newFrame);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to removeFrameFromList. framecount %d ret %d",
                    __FUNCTION__, __LINE__, newFrame->getFrameCount(), ret);
        }

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
    int pipeId_jpeg = 0;
//    camera_memory_t *jpegCallbackHeap = NULL;

    if (m_parameters->isReprocessing() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

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
                CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                goto done;
            }

            memset(burstFilePath, 0, sizeof(burstFilePath));

            m_burstCaptureCallbackCountLock.lock();
#if defined(SAMSUNG_INF_BURST)
            {
                time_t rawtime;
                struct tm *timeinfo;

                time(&rawtime);
                timeinfo = localtime(&rawtime);
                if(seriesShotNumber == 1) {
                    strftime(m_burstTime, 20, "%Y%m%d_%H%M%S", timeinfo);
                }
                else {
                /* We don't do anything in this time */
                }

                snprintf(burstFilePath, sizeof(burstFilePath), "%s%s_%03d.jpg",
                        m_burstSavePath, m_burstTime, seriesShotNumber);
            }
#else
            snprintf(burstFilePath, sizeof(burstFilePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
#endif
            m_burstCaptureCallbackCountLock.unlock();

            CLOGD("DEBUG(%s[%d]):%s fd:%d jpegSize : %d", __FUNCTION__, __LINE__,
                    burstFilePath, jpegSaveBuffer.fd[0], jpegSaveBuffer.size[0]);

            m_burstCaptureSaveLock.lock();

            m_burstSaveTimer.start();

            if(m_FileSaveFunc(burstFilePath, &jpegSaveBuffer) == false) {
                m_burstCaptureSaveLock.unlock();
                CLOGE("ERR(%s[%d]): FILE save ERROR", __FUNCTION__, __LINE__);
                goto done;
            }

            m_burstCaptureSaveLock.unlock();

            m_burstSaveTimer.stop();
            m_burstSaveTimerTime = m_burstSaveTimer.durationUsecs();
            if (m_burstSaveTimerTime > (m_burstDuration - 33000)) {
                m_burstDuration += (int)((m_burstSaveTimerTime - m_burstDuration + 33000) / 33000) * 33000;
                CLOGD("Increase burst duration = %d", m_burstDuration);
            }

            CLOGD("DEBUG(%s[%d]):m_burstSaveTimerTime : %d msec, path(%s)",
                    __FUNCTION__, __LINE__, (int)m_burstSaveTimerTime / 1000, burstFilePath);
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
    ExynosCameraFrame *newFrame = NULL;

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

    pipeId = PIPE_MCSC2_REPROCESSING;

    do {
        ret = m_yuvCallbackQ->waitAndPopProcessQ(&newFrame);
        if (ret < 0) {
            retry++;
            CLOGW("WARN(%s[%d]):m_yuvCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
        }
    } while (ret < 0 && retry < maxRetry &&
             m_flagThreadStop != true &&
             m_stopBurstShot == false &&
             !m_cancelPicture);

    if (newFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    CLOGV("INFO(%s[%d]):get frame from m_yuvCallbackQ", __FUNCTION__, __LINE__);

    ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                __FUNCTION__, __LINE__, pipeId, ret);
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId, &gscReprocessingBuffer);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    if (m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_LLS_DEBLUR
        && m_parameters->getLDCaptureMode() == 0
#endif
        ) {
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&gscReprocessingBuffer);
        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread run", __FUNCTION__, __LINE__);
    }

#ifdef SUPPORT_DEPTH_MAP
    if (newFrame->getRequest(PIPE_VC1) == true) {
        depthInfoCallbackHeap = m_getMemoryCb(-1, sizeof(camera_frame_metadata_t), 1, m_callbackCookie);
        if (!depthInfoCallbackHeap || depthInfoCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]):get depthInfoCallbackHeap(%d) fail", __FUNCTION__, __LINE__);
            goto CLEAN;
        }

        ret = newFrame->getSrcBuffer(pipeId, &depthMapBuffer);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBuffer fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: doing exception handling */
        }

        if (depthMapBuffer.index != -2) {
            int depthW = 0, depthH = 0;
            m_parameters->getDepthMapSize(&depthW, &depthH);
            depthCallbackHeap = m_getMemoryCb(depthMapBuffer.fd[0], depthMapBuffer.size[0], 1, m_callbackCookie);
            if (!depthCallbackHeap || depthCallbackHeap->data == MAP_FAILED) {
                CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, depthMapBuffer.size[0]);
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

        CLOGD("INFO(%s[%d]):Raw Dump start (%s)", __FUNCTION__, __LINE__, filePath);

        bRet = dumpToFile((char *)filePath,
            depthMapBuffer.addr[0],
            depthMapW * depthMapH * 2);
        if (bRet != true)
            ALOGE("couldn't make a raw file", __FUNCTION__, __LINE__);
#endif
    }
#endif

    debug = m_parameters->getDebugAttribute();
    m_MetadataExt.ext_data.usage = EXT_DATA_EXIF_DEBUG_INFO;
    m_MetadataExt.ext_data.size = debug->debugSize[APP_MARKER_4];
    m_MetadataExt.ext_data.data = (void *)debug->debugData[APP_MARKER_4];

#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_MetadataExt.timestamp = newFrame->getTimeStampBoot();
#else
    m_MetadataExt.timestamp = newFrame->getTimeStamp();
#endif
    CLOGV("INFO(%s[%d]): timestamp:%lldms!", __FUNCTION__, __LINE__, m_MetadataExt.timestamp);

    m_captureLock.lock();

#ifdef SUPPORT_DEPTH_MAP
    if (depthInfoCallbackHeap) {
        setBit(&m_callbackState, OUTFOCUS_DEPTH_MAP_INFO, true);
        m_dataCb(OUTFOCUS_DEPTH_MAP_INFO, depthInfoCallbackHeap, 0, &m_Metadata, m_callbackCookie);
        clearBit(&m_callbackState, OUTFOCUS_DEPTH_MAP_INFO, true);
    }
#endif

#ifdef SR_CAPTURE
    if(m_parameters->getSROn()) {
        m_doSRCallbackFunc();
    }
#endif

    CLOGD("DEBUG(%s[%d]): NV21 callback", __FUNCTION__, __LINE__);

    /* send yuv image with jpeg callback */
    jpegCallbackHeap = m_getMemoryCb(gscReprocessingBuffer.fd[0], gscReprocessingBuffer.size[0], 1, m_callbackCookie);
    if (!jpegCallbackHeap || jpegCallbackHeap->data == MAP_FAILED) {
        CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, gscReprocessingBuffer.size[0]);
        goto CLEAN;
    }

    setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
    m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE, jpegCallbackHeap, 0, &m_MetadataExt, m_callbackCookie);
    clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);

    jpegCallbackHeap->release(jpegCallbackHeap);

    CLOGD("DEBUG(%s[%d]): NV21 callback end", __FUNCTION__, __LINE__);

    if (m_ThumbnailCallbackThread->isRunning()
#ifdef SAMSUNG_LLS_DEBLUR
        && m_parameters->getLDCaptureMode() == 0
#endif
        ) {
        m_ThumbnailCallbackThread->join();
        CLOGD("DEBUG(%s[%d]): m_ThumbnailCallbackThread join", __FUNCTION__, __LINE__);
    }

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    if ((m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_LLS_DEBLUR
        || m_parameters->getLDCaptureMode()
#endif
        )
#ifdef ONE_SECOND_BURST_CAPTURE
        && currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST
#endif
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

#ifdef SUPPORT_DEPTH_MAP
    if (depthMapBuffer.index != -2) {
        if (depthCallbackHeap) {
            depthCallbackHeap->release(depthCallbackHeap);
        }

        CLOGD("DEBUG(%s[%d]): put depthMapBuffer(%d)", __FUNCTION__, __LINE__, depthMapBuffer.index);
        m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
    }

    if (depthInfoCallbackHeap) {
        depthInfoCallbackHeap->release(depthInfoCallbackHeap);
    }
#endif

    if (newFrame != NULL) {
        CLOGD("DEBUG(%s[%d]): yuv callback frame delete(%d)",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());
        newFrame->decRef();
        m_frameMgr->deleteFrame(newFrame);
        newFrame = NULL;
    }

    if (m_yuvcallbackCounter.getCount() <= 0) {

        if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21) {
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

    int waitCount = 5;
    while (m_yuvCallbackQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_yuvCallbackQ->getSizeOfProcessQ() > 0
        || currentSeriesShotMode != SERIES_SHOT_MODE_NONE
        || m_yuvcallbackCounter.getCount() > 0) {
        CLOGD("DEBUG(%s[%d]):m_yuvCallbackThread thread will run again. ShotMode(%d), postPictureQ(%d), Remaining(%d)",
            __FUNCTION__, __LINE__,
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
    camera_frame_metadata_t m_Metadata;

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
#ifdef ONE_SECOND_BURST_CAPTURE
    } else if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        maxRetry = ONE_SECOND_BURST_CAPTURE_MAX_RETRY;
#endif
    } else {
        maxRetry = TOTAL_WAITING_COUNT;
    }

#ifdef BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_BURST) {
        if (m_burstShutterLocation == BURST_SHUTTER_JPEGCB) {
            m_shutterCallbackThread->join();
            CLOGD("Burst Shutter callback start(%s)(%d)", __FUNCTION__, __LINE__);
            m_shutterCallbackThreadFunc();
            CLOGD("Burst Shutter callback end(%s)(%d)", __FUNCTION__, __LINE__);
        } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
            m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
        }
    }
#endif

    m_getBufferManager(PIPE_GSC_REPROCESSING3, &postviewBufferMgr, DST_BUFFER_DIRECTION);
    if (m_parameters->getSamsungCamera()
        && (m_parameters->getIsThumbnailCallbackOn() == true
#ifdef SAMSUNG_LLS_DEBLUR
            || m_parameters->getLDCaptureMode()
#endif
#ifdef SAMSUNG_LENS_DC
            || m_parameters->getLensDCEnable()
#endif
            )
#ifdef ONE_SECOND_BURST_CAPTURE
        && currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST
#endif
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

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        retry = 0;
        /* For launch COMPRESSED m_dataCb() faster*/
        do {
            if (m_one_second_burst_capture && m_one_second_jpegCallbackHeap != NULL) {
                /* If there's saved jpegCallbackHeap */
                CLOGD("DEBUG(%s[%d]):ONE SECOND CAPTURE coming.", __FUNCTION__, __LINE__);
		        ret = 0;
                break;
            }
            ret = m_jpegCallbackQ->popProcessQ(&newFrame);
            if (ret < 0) {
                /* If there's no saved jpegCallbackHeap, ret value will smaller than 0
                   This will launch "goto CLEAN" */
                retry++;
                CLOGW("WARN(%s[%d]):jpegCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
                usleep(ONE_SECOND_BURST_CAPTURE_RETRY_DELAY);
            }
        } while(ret < 0 && retry < maxRetry && m_jpegCounter.getCount() > 0 && m_stopBurstShot == false);
        if (m_stopBurstShot)
            goto CLEAN;
    } else
#endif
    {
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
    }

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

#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_Metadata.timestamp = newFrame->getTimeStampBoot();
#else
    m_Metadata.timestamp = newFrame->getTimeStamp();
#endif
    CLOGV("INFO(%s[%d]): timestamp:%lldms!", __FUNCTION__, __LINE__, m_Metadata.timestamp);

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST
        && m_parameters->getIsThumbnailCallbackOn()
        && jpegCallbackBuffer.index != -2) {
        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("WARN(%s[%d]):postviewCallbackQ pop fail, retry(%d)", __FUNCTION__, __LINE__, retry);
            }
        } while (ret < 0 && retry < maxRetry && m_flagThreadStop != true && m_stopBurstShot == false);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0], 1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("ERR(%s[%d]): get postviewCallbackHeap(%d) fail", __FUNCTION__, __LINE__);
            loop = true;
            goto CLEAN;
        }
    }
#endif

    m_captureLock.lock();

    if (ret < 0) {
        CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        loop = true;
        goto CLEAN;
    }

    seriesShotNumber = newFrame->getFrameCount();

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    /*
     * In case of YUV capture,
     * HAL will callback YUV by CAMERA_MSG_COMPRESSED_IMAGE
     * so, skip this jpeg callback.
     */
    if (m_parameters->getUseJpegCallbackForYuv() == true) {
        CLOGD("DEBUG(%s[%d]):skip jpeg calllback, by YUV capture", __FUNCTION__, __LINE__);
        goto CLEAN;
    }
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    CLOGD("DEBUG(%s[%d]):jpeg calllback is start", __FUNCTION__, __LINE__);

    /* Make compressed image */
    if (m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE) ||
        m_parameters->getSeriesShotCount() > 0) {
            if (jpegCallbackBuffer.index != -2) {
                jpegCallbackHeap = m_getJpegCallbackHeap(jpegCallbackBuffer, seriesShotNumber);
            }
#ifdef ONE_SECOND_BURST_CAPTURE
            if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                if (jpegCallbackHeap != NULL) {
                    if (m_one_second_jpegCallbackHeap != NULL) {
                        /* if last jpegCallbackHeap is remained */
                        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                    }
                    /* Save last jpegCallbackHeap */
                    m_one_second_jpegCallbackHeap = jpegCallbackHeap;
                } else {
                    /* Ready for COMPRESSED m_dataCb() */
                    jpegCallbackHeap = m_one_second_jpegCallbackHeap;
                }
                if (m_parameters->getIsThumbnailCallbackOn()) {
                    if (postviewCallbackHeap != NULL) {
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                        }
                        m_one_second_postviewCallbackHeap = postviewCallbackHeap;
                    } else {
                        postviewCallbackHeap = m_one_second_postviewCallbackHeap;
                    }
                }
            }
#endif

            if (jpegCallbackHeap == NULL) {
                CLOGE("ERR(%s[%d]):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: doing exception handling */
                android_printAssert(NULL, LOG_TAG, "Cannot recoverable, assert!!!!");
            }

#ifdef ONE_SECOND_BURST_CAPTURE
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST ||
                (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST && m_one_second_burst_capture)) {
                if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                    m_one_second_burst_capture = false;
                    if (m_burstShutterLocation == BURST_SHUTTER_JPEGCB) {
                        m_shutterCallbackThread->join();
                        CLOGD("One Second Shutter callback start(%s)(%d)", __FUNCTION__, __LINE__);
                        m_shutterCallbackThreadFunc();
                        CLOGD("One Second Shutter callback end(%s)(%d)", __FUNCTION__, __LINE__);
                    } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
                        m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
                    }
                    if (m_parameters->getIsThumbnailCallbackOn()) {
                        if (m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                            CLOGD("thumbnail POSTVIEW callback start(%s)(%d)", __FUNCTION__, __LINE__);
                            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            CLOGD("thumbnail POSTVIEW callback end(%s)(%d)", __FUNCTION__, __LINE__);
                        }
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            /* release saved buffer when COMPRESSED m_dataCb() called. */
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                            m_one_second_postviewCallbackHeap = NULL;
                        }
                    }
                    CLOGD("DEBUG(%s[%d]): before compressed - TimeCheck", __FUNCTION__, __LINE__);
                }
#endif
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
                    CLOGD("DEBUG(%s[%d]): CAMERA_MSG_COMPRESSED_IMAGE callback (%d)",
                            __FUNCTION__, __LINE__, m_burstCaptureCallbackCount);
                }

#ifdef ONE_SECOND_BURST_CAPTURE
                if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                    CLOGD("DEBUG(%s[%d]): After compressed - TimeCheck", __FUNCTION__, __LINE__);
                    if (m_one_second_jpegCallbackHeap != NULL) {
                        /* release saved buffer when COMPRESSED m_dataCb() called. */
                        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
                        m_one_second_jpegCallbackHeap = NULL;
                    }
                }
            }
#endif

            if (jpegCallbackBuffer.index != -2) {
                /* put JPEG callback buffer */
                if (m_jpegBufferMgr->putBuffer(jpegCallbackBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                    CLOGE("ERR(%s[%d]):putBuffer(%d) fail", __FUNCTION__, __LINE__, jpegCallbackBuffer.index);
            }

#ifdef ONE_SECOND_BURST_CAPTURE
            /* One second burst operation will release m_one_second_jpegCallbackHeap */
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
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

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        m_jpegCallbackCounter.decCount();
        CLOGD("DEBUG(%s[%d]):ONE SECOND BURST remaining count(%d)", __FUNCTION__, __LINE__, m_jpegCallbackCounter.getCount());
        if (m_jpegCallbackCounter.getCount() <= 1) {
            if (m_one_second_burst_capture) {
                loop = true;
                m_jpegCallbackCounter.setCount(2);
                m_reprocessingCounter.setCount(2);
                m_jpegCounter.setCount(2);
                m_pictureCounter.setCount(2);
                CLOGD("DEBUG(%s[%d]):Waiting once more...", __FUNCTION__, __LINE__);
            } else {
                loop = false;
                m_clearOneSecondBurst(true);
            }
        } else {
            loop = true;
        }
    }
#endif

    if (m_takePictureCounter.getCount() == 0) {
        m_pictureEnabled = false;
        loop = false;

#ifdef ONE_SECOND_BURST_CAPTURE
        /* m_terminatePictureThreads() already called in m_clearOneSecondBurst() */
        if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
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
    ExynosCameraFrame *callbackFrame = NULL;
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

    if (callbackFrame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
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
#ifdef SAMSUNG_LLS_DEBLUR
        || m_parameters->getLDCaptureMode() > 0
#endif
#ifdef SAMSUNG_LENS_DC
        || m_parameters->getLensDCEnable()
#endif
    ) {
        m_parameters->getPictureSize(&inputSizeW, &inputSizeH);
    } else {
        m_parameters->getPreviewSize(&inputSizeW, &inputSizeH);
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
    callbackFrame = NULL;
    do {
        ret = m_ThumbnailPostCallbackQ->waitAndPopProcessQ(&callbackFrame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):[F%d]Failed to waitAndPopProcessQ. ret %d waitCount %d flagThreadStop %d",
                __FUNCTION__, __LINE__,
                callbackFrame->getFrameCount(), ret, waitCount, m_flagThreadStop);
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
            m_parameters->getCameraId(), callbackFrame->getFrameCount());

    ret = dumpToFile((char *)filePath,
        postgscReprocessingDstBuffer.addr[0],
        FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH));
#endif

    m_postviewCallbackQ->pushProcessQ(&postgscReprocessingDstBuffer);

    CLOGD("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);

CLEAN:

    if (postgscReprocessingDstBuffer.index != -2 && ret < 0)
        m_putBuffers(dstbufferMgr, postgscReprocessingDstBuffer.index);

    if (m_parameters->getPictureFormat() != V4L2_PIX_FMT_NV21
#ifdef SAMSUNG_LLS_DEBLUR
       && !m_parameters->getLDCaptureMode()
#endif
#ifdef SAMSUNG_LENS_DC
       && !m_parameters->getLensDCEnable()
#endif
    ) {
        if (postgscReprocessingSrcBuffer.index != -2)
            m_putBuffers(srcbufferMgr, postgscReprocessingSrcBuffer.index);
    }

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
        if (m_parameters->isOwnScc(getCameraIdInternal()) == true)
            m_previewFrameFactory->setRequestSCC(false);
        else
            m_previewFrameFactory->setRequestISPC(false);
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if(m_parameters->getLDCaptureMode()) {
        m_parameters->setOutPutFormatNV21Enable(false);

        m_LDCaptureCount = 0;

        for (int i = 0; i < MAX_LD_CAPTURE_COUNT; i++) {
            m_LDBufIndex[i] = 0;
        }

        m_parameters->setLDCaptureMode(MULTI_SHOT_MODE_NONE);
        m_parameters->setLDCaptureCount(0);
        m_parameters->setLDCaptureLLSValue(LLS_LEVEL_ZSL);
#ifdef SET_LLS_CAPTURE_SETFILE
        m_parameters->setLLSCaptureOn(false);
#endif
    }
#endif

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCEnable()) {
        m_parameters->setLensDCEnable(false);
        m_parameters->setOutPutFormatNV21Enable(false);
        CLOGD("[DC](%s[%d]): Set Lens Distortion Correction, getLensDCEnable(%d)",
                __FUNCTION__, __LINE__, m_parameters->getLensDCEnable());
    }
#endif

    if (m_parameters->isReprocessing() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    m_prePictureThread->requestExit();
    m_pictureThread->requestExit();

    m_postPictureCallbackThread->requestExit();
#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureThread->requestExit();
#endif

    if (m_ThumbnailCallbackThread != NULL)
        m_ThumbnailCallbackThread->requestExit();

#ifdef SAMSUNG_DNG
    m_DNGCaptureThread->requestExit();
#endif

    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();
    m_yuvCallbackThread->requestExit();

    CLOGI("INFO(%s[%d]): wait m_prePictureThrad", __FUNCTION__, __LINE__);
    m_prePictureThread->requestExitAndWait();
    CLOGI("INFO(%s[%d]): wait m_pictureThrad", __FUNCTION__, __LINE__);
    m_pictureThread->requestExitAndWait();
 
    CLOGI("INFO(%s[%d]): wait m_postPictureCallbackThread", __FUNCTION__, __LINE__);
    m_postPictureCallbackThread->requestExitAndWait();
#ifdef SAMSUNG_LLS_DEBLUR
    CLOGI("INFO(%s[%d]): wait m_LDCaptureThread", __FUNCTION__, __LINE__);
    m_LDCaptureThread->requestExitAndWait();
#endif

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
        enum pipeline pipe = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;

        m_reprocessingFrameFactory->stopThread(pipe);
    }

#ifdef SAMSUNG_MAGICSHOT
    if (m_parameters->getShotMode() == SHOT_MODE_MAGIC) {
        if(m_parameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING2);
            CLOGI("INFO(%s[%d]):rear gsc , pipe stop(%d)",__FUNCTION__, __LINE__, PIPE_GSC_REPROCESSING2);
        } else {
            enum pipeline pipe = PIPE_GSC_VIDEO;

            m_previewFrameFactory->stopThread(pipe);
            CLOGI("INFO(%s[%d]):front gsc , pipe stop(%d)",__FUNCTION__, __LINE__, pipe);
        }
    }
#endif

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
#ifdef ONE_SECOND_BURST_CAPTURE
            if (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
            {
                int seriesShotSaveLocation = m_parameters->getSeriesShotSaveLocation();
                char command[CAMERA_FILE_PATH_SIZE];
                memset(command, 0, sizeof(command));

#ifdef SAMSUNG_INF_BURST
                snprintf(command, sizeof(command), "rm %s%s_%03d.jpg", m_burstSavePath, m_burstTime, newFrame->getFrameCount());
#else
                snprintf(command, sizeof(command), "rm %sBurst%02d.jpg", m_burstSavePath, newFrame->getFrameCount());
#endif

                CLOGD("DEBUG(%s[%d]):run %s - start", __FUNCTION__, __LINE__, command);
                system(command);
                CLOGD("DEBUG(%s[%d]):run %s - end", __FUNCTION__, __LINE__, command);
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
        enum pipeline pipe = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_3AA_REPROCESSING;
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
                CLOGE("ERR(%s[%d]):GSC stopThread fail, pipe(%d) ret(%d)", __FUNCTION__, __LINE__, pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("ERR(%s[%d]):stopThreadAndWait fail, pipeId(%d) ret(%d)", __FUNCTION__, __LINE__, pipe, ret);
        }
    }

    CLOGD("DEBUG(%s[%d]): clear postProcessList", __FUNCTION__, __LINE__);
    if (m_clearList(&m_postProcessList) < 0) {
        CLOGE("ERR(%s):m_clearList fail", __FUNCTION__);
    }

#if 1
    CLOGD("DEBUG(%s[%d]): clear m_dstPostPictureGscQ", __FUNCTION__, __LINE__);
    m_dstPostPictureGscQ->release();

    CLOGD("DEBUG(%s[%d]): clear postPictureQ", __FUNCTION__, __LINE__);
    m_postPictureQ->release();

    CLOGD("DEBUG(%s[%d]): clear m_yuvCallbackQ", __FUNCTION__, __LINE__);
    m_yuvCallbackQ->release();

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

#ifdef SAMSUNG_LLS_DEBLUR
    CLOGD("DEBUG(%s[%d]): clear m_LDCaptureQ", __FUNCTION__, __LINE__);
    m_LDCaptureQ->release();
#endif

    CLOGD("DEBUG(%s[%d]): reset postPictureBuffer", __FUNCTION__, __LINE__);
    m_postPictureBufferMgr->resetBuffers();

#ifdef SAMSUNG_LENS_DC
    CLOGD("DEBUG(%s[%d]): reset m_lensDCBufferMgr", __FUNCTION__, __LINE__);
    m_lensDCBufferMgr->resetBuffers();
#endif

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

#ifdef SAMSUNG_DNG
    CLOGI("INFO(%s[%d]): wait m_DNGCaptureThread", __FUNCTION__, __LINE__);
    m_DNGCaptureThread->requestExitAndWait();
    if (m_dngCaptureQ->getSizeOfProcessQ() > 0) {
        CLOGE("ERR(%s[%d]):remaining DNG buffer in captureQ", __FUNCTION__, __LINE__);
    }
    if (m_dngCaptureDoneQ->getSizeOfProcessQ() > 0) {
        CLOGE("ERR(%s[%d]):remaining DNG buffer in capturDoneQ", __FUNCTION__, __LINE__);
    }

    if (m_parameters->getDNGCaptureModeOn() == true) {
        m_parameters->cleanDngThumbnailBuffer();
    }
#endif
}

void ExynosCamera::m_dumpVendor(void)
{
#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL)
        m_swVDIS_BufferMgr->dump();
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
    if (m_hyperMotion_BufferMgr != NULL)
        m_hyperMotion_BufferMgr->dump();
#endif /*SAMSUNG_HYPER_MOTION*/
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

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
void ExynosCamera::m_clearDOFmeta(void)
{
    if(m_frameMetadata.dof_data != NULL) {
        delete []m_frameMetadata.dof_data;
        m_frameMetadata.dof_data = NULL;
    }
    m_frameMetadata.dof_column = 0;
    m_frameMetadata.dof_row = 0;
    m_frameMetadata.dof_usage = 0;

    memset(&m_frameMetadata.single_paf_data, 0, sizeof(st_AF_PafWinResult));
}
#endif
#ifdef SAMSUNG_OT
void ExynosCamera::m_clearOTmeta(void)
{
    memset(&m_frameMetadata.object_data, 0, sizeof(camera_object_t));
}
#endif
#ifdef SR_CAPTURE
status_t ExynosCamera::m_doSRCallbackFunc()
{
    ExynosCameraDurationTimer       m_srcallbackTimer;
    long long                       m_srcallbackTimerTime;
    int pictureW, pictureH;

    CLOGI("INFO(%s[%d]) -IN-", __FUNCTION__, __LINE__);

    int id[NUM_OF_DETECTED_FACES];
    int score[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedLeftEye[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedRightEye[NUM_OF_DETECTED_FACES];
    ExynosRect2 detectedMouth[NUM_OF_DETECTED_FACES];
    int numOfDetectedFaces = 0;
    int num = 0;
    int previewW, previewH;

    memset(&id, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);
    memset(&score, 0x00, sizeof(int) * NUM_OF_DETECTED_FACES);

    m_parameters->getHwPreviewSize(&previewW, &previewH);

    int waitCount = 5;
    while (m_isCopySrMdeta == true
           && waitCount > 0) {
        usleep(10000);
        waitCount--;
    }

    CLOGD("DEBUG(%s[%d]) faceDetectMode(%d)", __FUNCTION__, __LINE__, m_srShotMeta.stats.faceDetectMode);
    CLOGV("[%d %d]", m_srShotMeta.stats.faceRectangles[0][0], m_srShotMeta.stats.faceRectangles[0][1]);
    CLOGV("[%d %d]", m_srShotMeta.stats.faceRectangles[0][2], m_srShotMeta.stats.faceRectangles[0][3]);

    num = NUM_OF_DETECTED_FACES;
    if (m_parameters->getMaxNumDetectedFaces() < num)
        num = m_parameters->getMaxNumDetectedFaces();

    switch (m_srShotMeta.stats.faceDetectMode) {
    case FACEDETECT_MODE_SIMPLE:
    case FACEDETECT_MODE_FULL:
        break;
    case FACEDETECT_MODE_OFF:
    default:
        num = 0;
        break;
    }

    for (int i = 0; i < num; i++) {
        if (m_srShotMeta.stats.faceIds[i]) {
            switch (m_srShotMeta.stats.faceDetectMode) {
            case FACEDETECT_MODE_OFF:
                break;
            case FACEDETECT_MODE_SIMPLE:
                detectedFace[i].x1 = m_srShotMeta.stats.faceRectangles[i][0];
                detectedFace[i].y1 = m_srShotMeta.stats.faceRectangles[i][1];
                detectedFace[i].x2 = m_srShotMeta.stats.faceRectangles[i][2];
                detectedFace[i].y2 = m_srShotMeta.stats.faceRectangles[i][3];
                numOfDetectedFaces++;
                break;
            case FACEDETECT_MODE_FULL:
                id[i] = m_srShotMeta.stats.faceIds[i];
                score[i] = m_srShotMeta.stats.faceScores[i];

                detectedFace[i].x1 = m_srShotMeta.stats.faceRectangles[i][0];
                detectedFace[i].y1 = m_srShotMeta.stats.faceRectangles[i][1];
                detectedFace[i].x2 = m_srShotMeta.stats.faceRectangles[i][2];
                detectedFace[i].y2 = m_srShotMeta.stats.faceRectangles[i][3];

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
        memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

        int realNumOfDetectedFaces = 0;

        for (int i = 0; i < numOfDetectedFaces; i++) {
            /*
             * over 50s, we will catch
             * if (score[i] < 50)
             *    continue;
            */
            m_faces_sr[realNumOfDetectedFaces].rect[0] = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[1] = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[2] = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
            m_faces_sr[realNumOfDetectedFaces].rect[3] = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;

            m_faces_sr[realNumOfDetectedFaces].id = id[i];
            m_faces_sr[realNumOfDetectedFaces].score = score[i] > 100 ? 100 : score[i];

            m_faces_sr[realNumOfDetectedFaces].left_eye[0] = (detectedLeftEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedLeftEye[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].left_eye[1] = (detectedLeftEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedLeftEye[i].y1) - 1000;

            m_faces_sr[realNumOfDetectedFaces].right_eye[0] = (detectedRightEye[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedRightEye[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].right_eye[1] = (detectedRightEye[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedRightEye[i].y1) - 1000;

            m_faces_sr[realNumOfDetectedFaces].mouth[0] = (detectedMouth[i].x1 < 0) ? -2000 : m_calibratePosition(previewW, 2000, detectedMouth[i].x1) - 1000;
            m_faces_sr[realNumOfDetectedFaces].mouth[1] = (detectedMouth[i].y1 < 0) ? -2000 : m_calibratePosition(previewH, 2000, detectedMouth[i].y1) - 1000;

            CLOGV("face posision(cal:%d,%d %dx%d)(org:%d,%d %dx%d), id(%d), score(%d)",
                m_faces_sr[realNumOfDetectedFaces].rect[0], m_faces_sr[realNumOfDetectedFaces].rect[1],
                m_faces_sr[realNumOfDetectedFaces].rect[2], m_faces_sr[realNumOfDetectedFaces].rect[3],
                detectedFace[i].x1, detectedFace[i].y1,
                detectedFace[i].x2, detectedFace[i].y2,
                m_faces_sr[realNumOfDetectedFaces].id,
                m_faces_sr[realNumOfDetectedFaces].score);

            CLOGV("DEBUG(%s[%d]): left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
                    __FUNCTION__, __LINE__,
                    m_faces_sr[realNumOfDetectedFaces].left_eye[0],
                    m_faces_sr[realNumOfDetectedFaces].left_eye[1],
                    m_faces_sr[realNumOfDetectedFaces].right_eye[0],
                    m_faces_sr[realNumOfDetectedFaces].right_eye[1],
                    m_faces_sr[realNumOfDetectedFaces].mouth[0],
                    m_faces_sr[realNumOfDetectedFaces].mouth[1],
                    realNumOfDetectedFaces
                 );

            realNumOfDetectedFaces++;
        }

        m_sr_frameMetadata.number_of_faces = realNumOfDetectedFaces;
        m_sr_frameMetadata.faces = m_faces_sr;

        m_faceDetected = true;
        m_fdThreshold = 0;
    } else {
        if (m_faceDetected == true && m_fdThreshold < NUM_OF_DETECTED_FACES_THRESHOLD) {
            /* waiting the unexpected fail about face detected */
            m_fdThreshold++;
        } else {
            if (0 < m_sr_frameMetadata.number_of_faces)
                memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);

            m_sr_frameMetadata.number_of_faces = 0;
            m_sr_frameMetadata.faces = m_faces_sr;
            m_fdThreshold = 0;
            m_faceDetected = false;
        }
    }

    m_parameters->getPictureSize(&pictureW, &pictureH);

    m_sr_frameMetadata.crop_info.org_width = pictureW;
    m_sr_frameMetadata.crop_info.org_height = pictureH;
    if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
        m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        m_sr_frameMetadata.crop_info.cropped_width = m_sr_cropped_width;
        m_sr_frameMetadata.crop_info.cropped_height = m_sr_cropped_height;
    } else {
        m_sr_frameMetadata.crop_info.cropped_width = pictureW;
        m_sr_frameMetadata.crop_info.cropped_height = pictureH;
    }

    CLOGI("INFO(%s[%d]):sieze (%d,%d,%d,%d)", __FUNCTION__, __LINE__
     , pictureW, pictureH, m_sr_frameMetadata.crop_info.cropped_width, m_sr_frameMetadata.crop_info.cropped_height);

    m_srcallbackTimer.start();

    setBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);
    m_dataCb(MULTI_FRAME_SHOT_SR_EXTRA_INFO, m_srCallbackHeap, 0, &m_sr_frameMetadata, m_callbackCookie);
    clearBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);

    m_srcallbackTimer.stop();
    m_srcallbackTimerTime = m_srcallbackTimer.durationUsecs();

    if((int)m_srcallbackTimerTime / 1000 > 50) {
        CLOGD("INFO(%s[%d]): SR callback duration time : (%d)mec", __FUNCTION__, __LINE__, (int)m_srcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}
#endif

#if defined(SAMSUNG_EEPROM)
bool ExynosCamera::m_eepromThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    char sensorFW[50] = {0,};
    int ret = 0;
    FILE *fp = NULL;

    m_timer.start();

    if(m_cameraId == CAMERA_ID_BACK) {
        fp = fopen(SENSOR_FW_PATH_BACK, "r");
    } else {
        fp = fopen(SENSOR_FW_PATH_FRONT, "r");
    }
    if (fp == NULL) {
        CLOGE("ERR(%s[%d]):failed to open sysfs. camera id = %d", __FUNCTION__, __LINE__, m_cameraId);
        goto err;
    }

    if (fgets(sensorFW, sizeof(sensorFW), fp) == NULL) {
        CLOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    /* int numread = strlen(sensorFW); */
    CLOGI("INFO(%s[%d]): eeprom read complete. Sensor FW ver: %s", __FUNCTION__, __LINE__, sensorFW);

err:
    if (fp != NULL)
        fclose(fp);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("DEBUG(%s):duration time(%5d msec)", __FUNCTION__, (int)durationTime);

    /* one shot */
    return false;
}
#endif

#ifdef SAMSUNG_LBP
bool ExynosCamera::m_LBPThreadFunc(void)
{
    lbp_buffer_t LBPbuff;
    int ret;
    bool loop = true;

    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int HwPreviewW = 0, HwPreviewH = 0;

    ret = m_LBPbufferQ->waitAndPopProcessQ(&LBPbuff);

    if (m_flagThreadStop == true) {
        CLOGI("INFO(%s[%d]):m_flagThreadStop(%d)", __FUNCTION__, __LINE__, m_flagThreadStop);
        m_LBPCount = 0;
        return false;
    }

    if(ret == TIMED_OUT || ret < 0) {
        CLOGE("ERR(%s)(%d):wait and pop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        m_captureSelector->setBestFrameNum(0);
        m_LBPCount = 0;
        return false;
    }

    CLOGD("[LBP]: Buffer pushed(%d):%p, %p!!", LBPbuff.buffer.index, LBPbuff.buffer.addr[0], LBPbuff.buffer.addr[1]);
    m_LBPCount ++;

    if(m_LBPpluginHandle != NULL) {
        m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);
        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        pluginData.InBuffY= LBPbuff.buffer.addr[0];
        pluginData.InBuffU = LBPbuff.buffer.addr[1];
        pluginData.InWidth = HwPreviewW;
        pluginData.InHeight = HwPreviewH;

        m_timer.start();
        ret = uni_plugin_set(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin set failed!!", __FUNCTION__, __LINE__);
        ret = uni_plugin_process(m_LBPpluginHandle);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin process failed!!", __FUNCTION__, __LINE__);
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[LBP](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
    }

    if(m_LBPCount == m_parameters->getHoldFrameCount()) {
        int bestPhotoIndex;
        ret = uni_plugin_get(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INDEX, &bestPhotoIndex);
        if(ret < 0)
            CLOGE("[LBP](%s[%d]):Bestphoto plugin get failed!!", __FUNCTION__, __LINE__);

        CLOGD("[LBP]: Best photo index(%d), frame(%d)", bestPhotoIndex, m_LBPbuffer[bestPhotoIndex].frameNumber);
#if 0
        /* To check the performance */
        bool bRet;
        char filePath[70];
        for(int i = 0; i < m_LBPCount; i++) {
            memset(filePath, 0, sizeof(filePath));
            if(i == bestPhotoIndex)
                snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Bestphoto_%d_Pick.yuv", m_LBPbuffer[i].frameNumber);
            else
                snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Bestphoto_%d.yuv", m_LBPbuffer[i].frameNumber);

            bRet = dumpToFileYUV((char *)filePath,
                m_LBPbuffer[i].buffer.addr[0], m_LBPbuffer[i].buffer.addr[1],
                HwPreviewW, HwPreviewH);
            if (bRet != true)
                CLOGE("couldn't make a YUV file");
        }
#endif
        m_captureSelector->setBestFrameNum(m_LBPbuffer[bestPhotoIndex].frameNumber);
#ifdef SAMSUNG_JQ
        if(m_isJpegQtableOn == true) {
            m_isJpegQtableOn = false;
            ret = m_processJpegQtable(&m_LBPbuffer[bestPhotoIndex].buffer);
            if (ret < 0) {
                CLOGE("[JQ](%s[%d]): m_processJpegQtable() failed!!", __FUNCTION__, __LINE__);
            }
        }
#endif
    }

    if(m_LBPCount == m_parameters->getHoldFrameCount()) {
        m_LBPCount = 0;
        loop = false;
    }
    else
        loop = true;

    return loop;
}
#endif

#ifdef SAMSUNG_JQ
int ExynosCamera::m_processJpegQtable(ExynosCameraBuffer* buffer)
{
    int ret = 0;
    int HwPreviewW = 0, HwPreviewH = 0;
    UniPluginBufferData_t pluginData;

    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    m_parameters->getHwPreviewSize(&HwPreviewW, &HwPreviewH);

    memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
    pluginData.InBuffY = buffer->addr[0];
    pluginData.InBuffU = buffer->addr[1];
    pluginData.InWidth = HwPreviewW;
    pluginData.InHeight = HwPreviewH;
    pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;

    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_set(m_JQpluginHandle,
                JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
        }
        ret = uni_plugin_process(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin process failed!!", __FUNCTION__, __LINE__);
        }
        ret = uni_plugin_get(m_JQpluginHandle,
                JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_JPEG_QTABLE, m_qtable);
        if(ret < 0) {
            CLOGE("[JQ](%s[%d]): Plugin get(UNI_PLUGIN_INDEX_JPEG_QTABLE) failed!!", __FUNCTION__, __LINE__);
        }

        CLOGD("[JQ](%s[%d]): Qtable Set Done!!", __FUNCTION__, __LINE__);
        m_parameters->setJpegQtable(m_qtable);
        m_parameters->setJpegQtableStatus(JPEG_QTABLE_UPDATED);
    }

    m_JQ_mutex.lock();
    if (m_JQisWait == true)
        m_JQ_condition.signal();
    m_JQ_mutex.unlock();

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[JQ](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);

    return ret;
}
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
void ExynosCamera::m_clearOneSecondBurst(bool isJpegCallbackThread)
{
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("DEBUG(%s[%d]): DISABLE ONE SECOND BURST %d - TimeCheck", __FUNCTION__, __LINE__, isJpegCallbackThread);

        m_takePictureCounter.setCount(0);
#ifdef OIS_CAPTURE
        if (getCameraIdInternal() == CAMERA_ID_BACK) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
        }
#endif

        m_pictureEnabled = false;
        m_one_second_burst_capture = false;
        m_jpegCallbackCounter.clearCount();
        m_reprocessingCounter.clearCount();
        m_jpegCounter.clearCount();
        m_pictureCounter.clearCount();
        m_yuvcallbackCounter.clearCount();
        m_stopBurstShot = true;
        if (isJpegCallbackThread) {
            m_terminatePictureThreads(isJpegCallbackThread);
            m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
#ifdef USE_DVFS_LOCK
            if (m_parameters->getDvfsLock() == true)
                m_parameters->setDvfsLock(false);
#endif
        } else {
            /* waiting m_terminatePictureThreads first. jpegCallbackThread will launching m_clearJpegCallbackThread() */
            CLOGI("INFO(%s[%d]): wait m_jpegCallbackThread", __FUNCTION__, __LINE__);
            m_jpegCallbackThread->requestExit();
            m_jpegCallbackThread->requestExitAndWait();
            m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        }

        CLOGD("DEBUG(%s[%d]): DISABLE ONE SECOND BURST DONE %d - TimeCheck", __FUNCTION__, __LINE__, isJpegCallbackThread);
    }
    if (m_one_second_jpegCallbackHeap != NULL) {
        m_one_second_jpegCallbackHeap->release(m_one_second_jpegCallbackHeap);
        m_one_second_jpegCallbackHeap = NULL;
    }
    if (m_one_second_postviewCallbackHeap != NULL) {
        m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
        m_one_second_postviewCallbackHeap = NULL;
    }
    for (int i = 0; i < ONE_SECOND_BURST_CAPTURE_CHECK_COUNT; i++)
        TakepictureDurationTime[i] = 0;
}
#endif

bool ExynosCamera::m_postPictureCallbackThreadFunc(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    int ret = 0;
    int pipeId = PIPE_3AA_REPROCESSING;
    int pipeId_gsc_magic = PIPE_MCSC2_REPROCESSING;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    camera_memory_t *postviewCallbackHeap = NULL;
#ifdef SAMSUNG_BD
    int previewW = 0, previewH = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    bool isThumbnailCallbackOn = false;
#if 0
    char filePath[70] = "/data/media/0/BD_input.yuv";
#endif
#endif

    postgscReprocessingBuffer.index = -2;

    if (m_parameters->isReprocessing() == true) {
        pipeId_gsc_magic = PIPE_MCSC2_REPROCESSING;
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
#ifdef SAMSUNG_BD
        isThumbnailCallbackOn = true;
#endif
    }
#ifdef SAMSUNG_MAGICSHOT
    else if(m_parameters->getShotMode() == SHOT_MODE_MAGIC) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
            if (m_burstCaptureCallbackCount < m_magicshotMaxCount ) {
                postviewCallbackHeap = m_getMemoryCb(postgscReprocessingBuffer.fd[0],
                                                     postgscReprocessingBuffer.size[0], 1, m_callbackCookie);

                if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
                    CLOGE("ERR(%s[%d]):m_getMemoryCb(%d) fail", __FUNCTION__, __LINE__, postgscReprocessingBuffer.size[0]);
                    goto CLEAN;
                }

                setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                postviewCallbackHeap->release(postviewCallbackHeap);
                CLOGD("magic shot POSTVIEW callback end(%s)(%d) pipe(%d) index(%d)count(%d),max(%d)",
                        __FUNCTION__, __LINE__,pipeId_gsc_magic,postgscReprocessingBuffer.index,
                        m_burstCaptureCallbackCount,m_magicshotMaxCount);
            }

            ret = m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
        }

        return loop;
    }
#endif

#ifdef SAMSUNG_BD
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
        m_timer.start();

        UniPluginBufferData_t pluginData;
        m_parameters->getPreviewSize(&previewW, &previewH);
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        pluginData.InWidth = previewW;
        pluginData.InHeight = previewH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
        pluginData.InBuffY = postgscReprocessingBuffer.addr[0];
        pluginData.InBuffU = postgscReprocessingBuffer.addr[1];
#if 0
        dumpToFile((char *)filePath,
                postgscReprocessingBuffer.addr[0],
                (previewW * previewH * 3) / 2);
#endif
        if (m_BDpluginHandle != NULL) {
            ret = uni_plugin_set(m_BDpluginHandle,
                    BLUR_DETECTION_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if (ret < 0) {
                CLOGE("[BD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_process(m_BDpluginHandle);
            if (ret < 0) {
                CLOGE("[BD](%s[%d]): Plugin process failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_get(m_BDpluginHandle,
                    BLUR_DETECTION_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &m_BDbuffer[m_BDbufferIndex]);
            if (ret < 0) {
                CLOGE("[BD](%s[%d]): Plugin get failed!!", __FUNCTION__, __LINE__);
            }

            CLOGD("[BD](%s[%d]): Push the buffer(Qsize:%d, Index: %d)",
                    __FUNCTION__, __LINE__, m_BDbufferQ->getSizeOfProcessQ(), m_BDbufferIndex);

            if (m_BDbufferQ->getSizeOfProcessQ() == MAX_BD_BUFF_NUM)
                CLOGE("[BD](%s[%d]): All buffers are queued!! Skip the buffer!!", __FUNCTION__, __LINE__);
            else
                m_BDbufferQ->pushProcessQ(&m_BDbuffer[m_BDbufferIndex]);

            if (++m_BDbufferIndex == MAX_BD_BUFF_NUM)
                m_BDbufferIndex = 0;
        }

        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[BD](%s[%d]):Plugin duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
    }

    if (isThumbnailCallbackOn == false) {
        if (postgscReprocessingBuffer.index != -2)
            m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
    }
#endif

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

#ifdef SAMSUNG_LLS_DEBLUR
bool ExynosCamera::m_LDCaptureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("[LLS_MBR](%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;
    int loop = false;
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer gscReprocessingBuffer;
    int pipeId_gsc = PIPE_MCSC2_REPROCESSING;
    int pipeId = PIPE_3AA_REPROCESSING;
    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect_magic, dstRect_magic;
    float zoomRatio = m_parameters->getZoomRatio(0) / 1000;
    int retry1 = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
#ifdef FLASHED_LLS_CAPTURE
    const char *llsPluginName = FLASHED_LLS_PLUGIN_NAME;
#else
    const char *llsPluginName = LLS_PLUGIN_NAME;
#endif

    gscReprocessingBuffer.index = -2;

    ExynosCameraBufferManager *bufferMgr = NULL;
    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        return ret;
    }

    CLOGI("INFO(%s[%d]):wait m_deblurCaptureQ output", __FUNCTION__, __LINE__);
    ret = m_LDCaptureQ->waitAndPopProcessQ(&newFrame);
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

    ret = newFrame->getDstBuffer(pipeId, &gscReprocessingBuffer,
                                    m_reprocessingFrameFactory->getNodeType(pipeId_gsc));
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):getDstBuffer fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId_gsc, ret);
        goto CLEAN;
    }

    m_LDBufIndex[m_LDCaptureCount] = gscReprocessingBuffer.index;
    m_LDCaptureCount++;

    CLOGI("[LLS_MBR](%s[%d]):m_deblurCaptureQ output done m_LDCaptureCount(%d) index(%d)",
            __FUNCTION__, __LINE__, m_LDCaptureCount, gscReprocessingBuffer.index);

    /* Library */
    CLOGD("[LLS_MBR](%s[%d]):-- Start Library --", __FUNCTION__, __LINE__);
    m_timer.start();
    if (m_LLSpluginHandle != NULL) {
        int totalCount = 0;
        UNI_PLUGIN_OPERATION_MODE opMode = UNI_PLUGIN_OP_END;

        int llsMode = m_parameters->getLDCaptureLLSValue();
        switch(llsMode) {
#ifdef FLASHED_LLS_CAPTURE
            case LLS_LEVEL_FLASH:
                totalCount = 2;
                opMode = UNI_PLUGIN_OP_FLASHED_LLS_IMAGE;
                break;
#endif
            case LLS_LEVEL_MULTI_MERGE_2:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_2:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2:
                totalCount = 2;
                opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
                break;
            case LLS_LEVEL_MULTI_MERGE_3:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_3:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3:
                totalCount = 3;
                opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
                break;
            case LLS_LEVEL_MULTI_MERGE_4:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_4:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4:
                totalCount = 4;
                opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
                break;
            case LLS_LEVEL_MULTI_PICK_2:
                totalCount = 2;
                opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
                break;
            case LLS_LEVEL_MULTI_PICK_3:
                totalCount = 3;
                opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
                break;
            case LLS_LEVEL_MULTI_PICK_4:
                totalCount = 4;
                opMode = UNI_PLUGIN_OP_SELECT_IMAGE;
                break;
            default:
                CLOGE("[LLS_MBR](%s[%d]): Wrong LLS mode has been get!!", __FUNCTION__, __LINE__);
                goto CLEAN;
        }

        /* First shot */
        if (m_LDCaptureCount == 1) {
            ret = uni_plugin_init(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin init failed!!", __FUNCTION__, __LINE__);
            }

            CLOGD("[LLS_MBR](%s[%d]): Set capture num: %d", __FUNCTION__, __LINE__, totalCount);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &totalCount);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM failed!!", __FUNCTION__, __LINE__);
            }

            CLOGD("[LLS_MBR](%s[%d]): Set capture mode: %d", __FUNCTION__, __LINE__, opMode);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_OPERATION_MODE, &opMode);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_OPERATION_MODE failed!!", __FUNCTION__, __LINE__);
            }

            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            CLOGD("[LLS_MBR](%s[%d]): Set camera info: %d:%d", __FUNCTION__, __LINE__,
                    cameraInfo.CameraType, cameraInfo.SensorType);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!", __FUNCTION__, __LINE__);
            }

            struct camera2_udm *udm = new struct camera2_udm;

            if (udm != NULL) {
                UniPluginExtraBufferInfo_t extraData;
                memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
                memset(udm, 0x00, sizeof(struct camera2_udm));
                newFrame->getUserDynamicMeta(udm);

                extraData.brightnessValue.num = udm->ae.vendorSpecific[5];
                extraData.brightnessValue.den = 256;

                /* short shutter speed(us) */
                extraData.shutterSpeed[0].num = udm->ae.vendorSpecific[390];
                extraData.shutterSpeed[0].den = 1000000;

                /* long shutter speed(us) */
                extraData.shutterSpeed[1].num = udm->ae.vendorSpecific[392];
                extraData.shutterSpeed[1].den = 1000000;

                /* short ISO */
                extraData.iso[0] = udm->ae.vendorSpecific[391];

                /* long ISO */
                extraData.iso[1] = udm->ae.vendorSpecific[393];
                extraData.DRCstrength = udm->ae.vendorSpecific[394];

                ret = uni_plugin_set(m_LLSpluginHandle,
                                     llsPluginName,
                                     UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO,
                                     &extraData);
                if(ret < 0)
                    CLOGE("[LLS_MBR](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!", __FUNCTION__, __LINE__);

                delete udm;
            }
        }

        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
        m_parameters->getPictureSize(&pictureW, &pictureH);
        pluginData.InBuffY = gscReprocessingBuffer.addr[0];
        pluginData.InBuffU = gscReprocessingBuffer.addr[0] + (pictureW * pictureH);
        pluginData.InWidth = pictureW;
        pluginData.InHeight = pictureH;
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
#if 0
        char filePath[100] = {'\0',};
        sprintf(filePath, "/data/media/0/LLS_input_%d.yuv", m_LDCaptureCount);
        dumpToFile((char *)filePath,
                gscReprocessingBuffer.addr[0],
                (pictureW * pictureH * 3) / 2);
#endif
        CLOGD("[LLS_MBR](%s[%d]): Set buffer info, Width: %d, Height: %d", __FUNCTION__, __LINE__,
                pluginData.InWidth, pluginData.InHeight);
        ret = uni_plugin_set(m_LLSpluginHandle,
                llsPluginName, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if (ret < 0) {
            CLOGE("[LLS_MBR](%s[%d]): LLS Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!! ret(%d)", __FUNCTION__, __LINE__, ret);
            if (m_LDCaptureCount == totalCount) {
                ret = uni_plugin_deinit(m_LLSpluginHandle);
                if (ret < 0) {
                    CLOGE("[LLS_MBR](%s[%d]): LLS Plugin deinit failed!!", __FUNCTION__, __LINE__);
                }
            }
            goto POST_PROC;
        }

        /* Last shot */
        if (m_LDCaptureCount == totalCount) {
            ret = uni_plugin_process(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin process failed!!", __FUNCTION__, __LINE__);
            }

            UTstr debugData;
            debugData.data = new unsigned char[LLS_EXIF_SIZE];

            ret = uni_plugin_get(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
            CLOGD("[LLS_MBR](%s[%d]): Debug buffer size: %d", __FUNCTION__, __LINE__, debugData.size);
            m_parameters->setLLSdebugInfo(debugData.data, debugData.size);

            if (debugData.data != NULL)
                delete []debugData.data;

            ret = uni_plugin_deinit(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR](%s[%d]): LLS Plugin deinit failed!!", __FUNCTION__, __LINE__);
            }
        }
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[LLS_MBR](%s[%d]):duration time(%5d msec)", __FUNCTION__, __LINE__, (int)durationTime);
    CLOGD("[LLS_MBR](%s[%d]):-- end Library --", __FUNCTION__, __LINE__);

POST_PROC:
    if (m_LDCaptureCount < m_parameters->getLDCaptureCount()) {
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
        goto CLEAN;
    }

    m_postPictureQ->pushProcessQ(&newFrame);

    for (int i = 0; i < m_parameters->getLDCaptureCount() - 1; i++) {
        /* put GSC 1st buffer */
        ret = m_putBuffers(bufferMgr, m_LDBufIndex[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, pipeId_gsc, ret);
        }
    }

CLEAN:
    int waitCount = 25;
    if (m_LDCaptureCount == m_parameters->getLDCaptureCount()) {
        waitCount = 0;
    }

    while (m_LDCaptureQ->getSizeOfProcessQ() == 0 && 0 < waitCount) {
        usleep(10000);
        waitCount--;
    }

    if (m_LDCaptureQ->getSizeOfProcessQ()) {
        CLOGD("[LLS_MBR](%s[%d]):m_LDCaptureThread thread will run again. m_LDCaptureQ size(%d)",
                __func__, __LINE__, m_LDCaptureQ->getSizeOfProcessQ());
        loop = true;
    }

    return loop;
}

#endif

#ifdef SAMSUNG_LENS_DC
void ExynosCamera::m_setLensDCMap(void)
{
    int pictureW = 0, pictureH = 0;
    UniPluginBufferData_t pluginData;
    UniPluginPrivInfo_t pluginPrivInfo;
    int lensDCIndex = -1;
    int skipLensDC = true;
    int ret = 0;
    void *map[2];

    m_parameters->getPictureSize(&pictureW, &pictureH);

    if (pictureW == 4032 && pictureH == 3024) {
        skipLensDC = false;
        lensDCIndex = 1;
        map[0] = map1_x;
        map[1] = map1_y;
    } else if (pictureW == 2880 && pictureH == 2160) {
        skipLensDC = false;
        lensDCIndex = 2;
        map[0] = map2_x;
        map[1] = map2_y;
    } else if (pictureW == 4032 && pictureH == 2268) {
        skipLensDC = false;
        lensDCIndex = 3;
        map[0] = map3_x;
        map[1] = map3_y;
    } else if (pictureW == 2560 && pictureH == 1440) {
        skipLensDC = false;
        lensDCIndex = 4;
        map[0] = map4_x;
        map[1] = map4_y;
    } else if (pictureW == 3024 && pictureH == 3024) {
        skipLensDC = false;
        lensDCIndex = 5;
        map[0] = map5_x;
        map[1] = map5_y;
    } else if (pictureW == 2160 && pictureH == 2160) {
        skipLensDC = false;
        lensDCIndex = 6;
        map[0] = map6_x;
        map[1] = map6_y;
    } else if (pictureW == 3984 && pictureH == 2988) {
        skipLensDC = false;
        lensDCIndex = 7;
        map[0] = thirdmap1_x;
        map[1] = thirdmap1_y;
    } else if (pictureW == 3264 && pictureH == 2448) {
        skipLensDC = false;
        lensDCIndex = 8;
        map[0] = thirdmap2_x;
        map[1] = thirdmap2_y;
    } else if (pictureW == 3264 && pictureH == 1836) {
        skipLensDC = false;
        lensDCIndex = 9;
        map[0] = thirdmap3_x;
        map[1] = thirdmap3_y;
    } else if (pictureW == 2976 && pictureH == 2976) {
        skipLensDC = false;
        lensDCIndex = 10;
        map[0] = thirdmap4_x;
        map[1] = thirdmap4_y;
    } else if (pictureW == 2048 && pictureH == 1152) {
        skipLensDC = false;
        lensDCIndex = 11;
        map[0] = thirdmap5_x;
        map[1] = thirdmap5_y;
    } else if (pictureW == 1920 && pictureH == 1080) {
        skipLensDC = false;
        lensDCIndex = 12;
        map[0] = thirdmap6_x;
        map[1] = thirdmap6_y;
    } else if (pictureW == 1280 && pictureH == 720) {
        skipLensDC = false;
        lensDCIndex = 13;
        map[0] = thirdmap7_x;
        map[1] = thirdmap7_y;
    } else if (pictureW == 960 && pictureH == 720) {
        skipLensDC = false;
        lensDCIndex = 14;
        map[0] = thirdmap8_x;
        map[1] = thirdmap8_y;
    } else if (pictureW == 640 && pictureH == 480) {
        skipLensDC = false;
        lensDCIndex = 15;
        map[0] = thirdmap9_x;
        map[1] = thirdmap9_y;
    } else {
        skipLensDC = true;
        lensDCIndex = 0;
        map[0] = NULL;
        map[1] = NULL;
    }

    if ((m_LensDCIndex == -1 || (m_LensDCIndex != -1 && m_LensDCIndex != lensDCIndex))
            && !skipLensDC) {
        CLOGD("[DC](%s[%d]): Change Picture size. map change (%d)->(%d)",
                __FUNCTION__, __LINE__, m_LensDCIndex, lensDCIndex);

        if (m_DCpluginHandle != NULL) {
            if (m_LensDCIndex > 0) {
                    ret = uni_plugin_deinit(m_DCpluginHandle);
                    if (ret < 0) {
                        CLOGE("[DC](%s[%d]): Plugin deinit failed!!", __FUNCTION__, __LINE__);
                    }
            }

            memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
            memset(&pluginPrivInfo, 0, sizeof(UniPluginPrivInfo_t));

            pluginData.InWidth = pictureW;
            pluginData.InHeight = pictureH;
            pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
            pluginPrivInfo.priv[0] = map[0];
            pluginPrivInfo.priv[1] = map[1];

            ret = uni_plugin_set(m_DCpluginHandle,
                    LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if (ret < 0) {
                CLOGE("[DC](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_set(m_DCpluginHandle,
                                    LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_PRIV_INFO, &pluginPrivInfo);                
            if (ret < 0) {
                CLOGE("[DC](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_DISTORTION_INFO) failed!!", __FUNCTION__, __LINE__);
            }

            ret = uni_plugin_init(m_DCpluginHandle);
            if (ret < 0) {
                CLOGE("[DC](%s[%d]): Plugin init failed!!", __FUNCTION__, __LINE__);
            }
        }
    }

    m_skipLensDC = skipLensDC;
    m_LensDCIndex = lensDCIndex;
}
#endif

}; /* namespace android */
