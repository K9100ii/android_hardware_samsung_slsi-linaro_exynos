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
#include "ExynosCameraTimeLogger.h"
#ifdef SAMSUNG_LENS_DC
#include "LDC_Map.h"
#endif

#include "ExynosCameraFrameReprocessingFactoryRemosaic.h"

namespace android {

#define IS_OUTPUT_NODE(_FACTORY,_PIPE) \
    (((_FACTORY->getNodeType(_PIPE) >= OUTPUT_NODE) \
        && (_FACTORY->getNodeType(_PIPE) < MAX_OUTPUT_NODE)) ? true:false)

#ifdef TIME_LOGGER_LAUNCH_ENABLE
extern ExynosCameraTimeLogger g_timeLoggerLaunch;
extern int g_timeLoggerLaunchIndex;
#endif
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

#ifdef USE_LIB_ION_LEGACY
    m_ionClient = ion_open();
#else
    m_ionClient = exynos_ion_open();
#endif
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            int previewW = 0, previewH = 0;
            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;

            m_parameters->getPreviewSize(&previewW, &previewH);

            ret = m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);
            if (ret != NO_ERROR) {
                CLOGE("getFusionSize(%d, %d)) fail", previewW, previewH);
                return ret;
            }
            width  = fusionDstRect.w;
            height = fusionDstRect.h;
        }
#endif

        if (m_grAllocator == NULL)
            m_grAllocator = new ExynosCameraGrallocAllocator(m_cameraId);

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

    CLOGI("");

    if ((m_parameters == NULL) && (m_frameMgr == NULL)) {
        CLOGE(" initalize HAL");
        this->initialize();
    }

    property_set("persist.sys.camera.preview","2");

    int ret = 0;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    int32_t skipFdFrameCount = 0;
    size_t fdCallbackSize = 0;
#ifdef SR_CAPTURE
    unsigned int srCallbackSize = 0;
#endif
#ifdef CAMERA_FAST_ENTRANCE_V1
    int wait_cnt = 0;
#endif
    /* setup frame thread */
    int pipeId = PIPE_3AA;

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
#else
    m_parameters->setMasterCamera(m_cameraId);
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
    m_preLongExposureTime = 0;
    m_previewCallbackToggle = 0;
    m_disablePreviewCB = false;

    int controlPipeId = m_getBayerPipeId();

#ifdef SAMSUNG_DUAL_SOLUTION
    if (m_parameters->getCameraId() == CAMERA_ID_BACK && m_parameters->getDualCameraMode() == true) {
        int maxSensorW, maxSensorH;
        m_parameters->getHwSensorSize(&maxSensorW, &maxSensorH);
        int previewW, previewH;
        int captureW, captureH;
        m_parameters->getPreviewSize(&previewW, &previewH);
        m_parameters->getPictureSize(&captureW, &captureH);
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;
        m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);

        int *zoomList;
        int maxZoomLevel;
        m_parameters->getZoomList(&zoomList, &maxZoomLevel);
        m_fusionPreviewWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH,
                                            fusionDstRect.w, fusionDstRect.h, zoomList, maxZoomLevel);
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        m_fusionCaptureWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH, captureW, captureH,
                                            zoomList, maxZoomLevel);
#endif
    }
#endif /* SAMSUNG_DUAL_SOLUTION */

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

#ifdef SAMSUNG_FIXED_FACE_FOCUS
    if (m_parameters->getFocusMode() ==  FOCUS_MODE_FIXED_FACE) {
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        if (autoFocusMgr->flagAutofocusStart() == true &&
            autoFocusMgr->flagLockAutofocus() == false) {
            autoFocusMgr->stopAutofocus();
            autoFocusMgr->startAutofocus();
        }
    }
#endif

#if defined(OIS_CAPTURE) || defined(RAWDUMP_CAPTURE)
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
#endif

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
        CLOGE("m_startCompanion fail");
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
                CLOGE("m_getMemoryCb(%zu) fail", fdCallbackSize);
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
                CLOGE("m_getMemoryCb(%d) fail", srCallbackSize);
                m_srCallbackHeap = NULL;
                goto err;
            }
        }
    }
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY)
#endif
    {
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);

        if(m_parameters->increaseMaxBufferOfPreview()) {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS);
        } else {
            m_parameters->setPreviewBufferCount(NUM_PREVIEW_BUFFERS);
        }

#ifdef SUPPORT_SW_VDIS
        //Initalize vdis mode/vdis dual mode (dual mode true & wide case or dual false case)

        if ((m_parameters->getDualCameraMode() == false ||
            (m_parameters->getDualCameraMode() == true && getCameraIdInternal() != CAMERA_ID_BACK_1))) {
            m_swVDIS_Mode = false;
            m_swVDIS_ModeDual = false;
            CLOGD("VDIS_HAL: m_swVDIS_Mode init");
        }

        if (m_parameters->getDualCameraMode()) {
            m_swVDIS_ModeDual = true;
        }

        if (m_parameters->isSWVdisMode() && (m_swVDIS_ModeDual == false ||
            (m_swVDIS_ModeDual == true && getCameraIdInternal() != CAMERA_ID_BACK_1))) {
            m_swVDIS_InW = m_swVDIS_InH = 0;
            m_parameters->getHwPreviewSize(&m_swVDIS_InW, &m_swVDIS_InH);
#ifdef SAMSUNG_DUAL_SOLUTION
            if (m_parameters->getDualCameraMode() == true) {
                ExynosRect fusionSrcRect;
                ExynosRect fusionDstRect;
                int previewW = 0, previewH = 0;
                m_parameters->getPreviewSize(&previewW, &previewH);
                m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);

                m_swVDIS_InW = fusionDstRect.w;
                m_swVDIS_InH = fusionDstRect.h;
            }
#endif
            m_parameters->getPreviewFpsRange(&m_swVDIS_MinFps, &m_swVDIS_MaxFps);
            m_swVDIS_DeviceOrientation = m_parameters->getDeviceOrientation();
            m_swVDIS_CameraID = getCameraIdInternal();
            m_swVDIS_SensorType = m_cameraSensorId;
            m_swVDIS_APType = UNI_PLUGIN_AP_TYPE_SLSI;
#ifdef SAMSUNG_OIS_VDIS
            m_swVDIS_OISMode = UNI_PLUGIN_OIS_MODE_VDIS;
            m_swVDIS_OISGain = m_parameters->getOISGain();
            m_OISvdisMode = UNI_PLUGIN_OIS_MODE_END;
#endif
            m_swVDIS_init();

            if (m_swVDIS_UHD) {
                m_parameters->setPreviewBufferCount(NUM_VDIS_UHD_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_VDIS_UHD_BUFFERS;
                VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_VDIS_UHD_BUFFERS);
            } else {
                m_parameters->setPreviewBufferCount(NUM_VDIS_BUFFERS);
                m_exynosconfig->current->bufInfo.num_preview_buffers = NUM_VDIS_BUFFERS;
                VDIS_LOG("VDIS_HAL: Preview Buffer Count Change to %d", NUM_VDIS_BUFFERS);
            }
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
        CLOGD(" start quick switching");
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == false ||
                (m_parameters->getDualCameraMode() == true &&
                 m_cameraId == masterCameraId))
#endif
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
        if (ret < 0)
            CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);
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
                CLOGE(" m_previewFrameFactory is not created for 1 SEC");
                break;
            }
            usleep(5000);
            wait_cnt++;

            if (wait_cnt % 20 == 0) {
                CLOGW(" Waiting until m_previewFrameFactory create (wait_cnt %d)",
                         wait_cnt);
            }
        }
    }
    else
#endif
    {
        /* update frameMgr parameter */
        m_setFrameManagerInfo();

        /* frame manager start */
        m_frameMgr->start();

        /*
         * This is for updating parameter value at once.
         * This must be just before making factory
         */
        m_parameters->updateParameters();
    }



#ifdef USE_DUAL_CAMERA
    m_parameters->updateDualCameraFusionMode();
#endif

    if ((m_parameters->getRestartPreview() == true) ||
        m_previewBufferCount != m_parameters->getPreviewBufferCount()) {
        ret = setPreviewWindow(m_previewWindow);
        if (ret < 0) {
            CLOGE("setPreviewWindow fail");
            return INVALID_OPERATION;
        }

        m_previewBufferCount = m_parameters->getPreviewBufferCount();
    }

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
        if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
        {
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_START_PREVIEW_START, false);
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_SET_BUFFER_START, true);
        }
#endif
        ret = m_setBuffers();
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
        if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
        {
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_SET_BUFFER_START, false);
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_SET_BUFFER_END, true);
        }
#endif
        if (ret < 0) {
            CLOGE("m_setBuffers failed, releaseBuffer");
            m_isSuccessedBufferAllocation = false;
            goto err;
        }

        m_isSuccessedBufferAllocation = true;
    } else
#endif
    {
        CLOGI("setBuffersThread is run");
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            m_captureSelector = new ExynosCameraSyncFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr
#ifdef SUPPORT_DEPTH_MAP
                    , m_depthCallbackQ, m_depthMapBufferMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                    , m_fliteBufferMgr
#endif
                    );
        } else
#endif /* USE_DUAL_CAMERA */
        {
            m_captureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr
#ifdef SUPPORT_DEPTH_MAP
                    , m_depthCallbackQ, m_depthMapBufferMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                    , m_fliteBufferMgr
#endif
                    );
        }

        if (m_parameters->isReprocessing() == true) {
            ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
            if (ret < 0)
                CLOGE(" setFrameHoldCount(%d) is fail",
                        REPROCESSING_BAYER_HOLD_COUNT);
        }
    }

    if (m_sccCaptureSelector == NULL) {
        ExynosCameraBufferManager *bufMgr = NULL;
        if (m_parameters->isSccCapture() == true) {
            /* TODO: Dynamic select buffer manager for capture */
            bufMgr = m_sccBufferMgr;
        }
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            m_sccCaptureSelector = new ExynosCameraSyncFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr);
        } else
#endif /* USE_DUAL_CAMERA */
        {
            m_sccCaptureSelector = new ExynosCameraFrameSelector(m_cameraId, m_parameters, bufMgr, m_frameMgr);
        }
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
                CLOGD(" eepromThread join.....");
                m_eepromThread->join();
            } else {
                CLOGD(" eepromThread is NULL.");
            }
            m_parameters->setRomReadThreadDone(true);
            CLOGD(" eepromThread joined");
        }
#endif /* SAMSUNG_EEPROM */

#ifdef SAMSUNG_COMPANION
        if(m_use_companion == true) {
            /*
             * for critical section from open ~ V4L2_CID_IS_END_OF_STREAM.
             * This lock is available on local variable scope only from { to }.
             */
            Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());

            /*
             * companion thread(front eeprom) and fastAE thread(front AF) use same i2c port.
             * so, companion thread should be finished before fastAE thread in DICO system.
             */
            m_waitCompanionThreadEnd();

            ret = m_previewFrameFactory->precreate();
            if (ret < 0) {
                CLOGE("m_previewFrameFactory->precreate() failed");
                goto err;
            }

            ret = m_previewFrameFactory->postcreate();
            if (ret < 0) {
                CLOGE("m_previewFrameFactory->postcreate() failed");
                goto err;
            }
        } else {
            ret = m_previewFrameFactory->create();
            if (ret < 0) {
                CLOGE("m_previewFrameFactory->create() failed");
                goto err;
            }
        }
#else
        ret = m_previewFrameFactory->create();
        if (ret < 0) {
            CLOGE("m_previewFrameFactory->create() failed");
            goto err;
        }
#endif
        CLOGD("FrameFactory(previewFrameFactory) created");
    }

#ifdef SAMSUNG_UNIPLUGIN
    if (m_uniPluginThread != NULL)
        m_uniPluginThread->join();
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
        if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
        {
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_SET_BUFFER_END, false);
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FASTAE_THREAD_WAIT_START, true);
        }
#endif
        m_waitFastenAeThreadEnd();
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
        if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
        {
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FASTAE_THREAD_WAIT_START, false);
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FASTAE_THREAD_WAIT_END, true);
        }
#endif
        if (m_fastenAeThreadResult < 0) {
            CLOGE("fastenAeThread exit with error");
            ret = m_fastenAeThreadResult;
            goto err;
        }
    } else
#endif
    {
#ifdef USE_QOS_SETTING
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == false ||
                (m_parameters->getDualCameraMode() == true &&
                 m_cameraId == masterCameraId)) {
#endif
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
            if (ret < 0)
                CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER0, LITTLE_CORE_MIN_LOCK, PIPE_3AA);
            if (ret < 0)
                CLOGE("V4L2_CID_IS_DVFS_CLUSTER0 setControl fail, ret(%d)", ret);
#ifdef USE_DUAL_CAMERA
        }
#endif
#endif

        if (m_parameters->getUseFastenAeStable() == true &&
#ifdef USE_DUAL_CAMERA
            m_parameters->isFastenAeStableEnable() &&
#else
            m_parameters->getDualMode() == false &&
#endif
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
        } else if (m_parameters->getDualMode() == true
#ifdef USE_DUAL_CAMERA
                   && m_parameters->getDualCameraMode() == false
#endif
        ) {
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

#ifdef USE_FADE_IN_ENTRANCE
    if (m_parameters->getFirstEntrance() == true) {
        /* Fade In/Out */
        m_parameters->setFrameSkipCount(0);
        ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 1, controlPipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)",
                    ret);
        }
    } else {
        /* Skip Frame */
        m_parameters->setFrameSkipCount(skipFrameCount);
        ret = m_previewFrameFactory->setControl(V4L2_CID_CAMERA_FADE_IN, 0, controlPipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail(V4L2_CID_CAMERA_FADE_IN), ret(%d)",
                    ret);
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
            CLOGE("m_setBuffersThread() failed");
            ret = INVALID_OPERATION;
            goto err;
        }
    }

#ifdef SAMSUNG_QUICK_SWITCH
quick_switching_in_startpreview:
    if (m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_IDLE_TO_STBY) {
        m_isQuickSwitchState = QUICK_SWITCH_STBY;
        m_flagFirstPreviewTimerOn = false;
        CLOGD(" set stby state for quick switch");
        return NO_ERROR;
    } else if (m_isQuickSwitchState == QUICK_SWITCH_STBY) {
        CLOGD(" set main state for quick switch");
        m_isQuickSwitchState = QUICK_SWITCH_MAIN;
        if (m_startCompanion() != NO_ERROR) {
            CLOGE("m_startCompanion fail");
        }

        if (m_use_companion == true) {
            m_waitCompanionThreadEnd();
        }

        if (m_parameters->getUseFastenAeStable() == true
#ifdef USE_DUAL_CAMERA
            && m_parameters->isFastenAeStableEnable()
#else
            && m_parameters->getDualMode() == false
#endif
            && m_isFirstStart == true) {
            ret = m_fastenAeStable();
            if (ret < 0) {
                CLOGE("m_fastenAeStable() failed");
                ret = INVALID_OPERATION;
                goto err;
            } else {
                m_parameters->setUseFastenAeStable(false);
                m_parameters->setFrameSkipCount(0);
                m_fdFrameSkipCount = 0;
            }
        } else {
            m_parameters->setFrameSkipCount(0);
            m_fdFrameSkipCount = INITIAL_SKIP_FD_FRAME;
        }

        /* update frameMgr parameter */
        m_setFrameManagerInfo();

        m_frameMgr->start();
    } else {
        m_isQuickSwitchState = QUICK_SWITCH_MAIN;
    }

#ifdef START_PICTURE_THREAD
    if (m_parameters->isReprocessing() == true
        && m_parameters->checkEnablePicture() == true
#ifdef USE_DUAL_CAMERA
        && !(m_parameters->getDualCameraMode() == false &&
            && m_parameters->getDualMode() == true)
#else
        && m_parameters->getDualMode() != true
#endif
       ) {
        m_preStartPictureInternalThread->run();
        CLOGD(" m_preStartPictureInternalThread starts");
    }
#endif
#endif
    /* FD-AE is always on */
#ifdef USE_FD_AE
    m_enableFaceDetection(m_parameters->getFaceDetectionMode(false));
#endif

    ret = m_startPreviewInternal();
    if (ret < 0) {
        CLOGE("m_startPreviewInternal() failed");
        goto err;
    }

    if (m_parameters->isReprocessing() == true && m_parameters->checkEnablePicture() == true) {
#ifdef START_PICTURE_THREAD
#ifdef USE_DUAL_CAMERA
        if (!(m_parameters->getDualCameraMode() == false &&
            m_parameters->getDualMode() == true))
#else
        if (m_parameters->getDualMode() != true)
#endif
        {
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
        CLOGD("FrameFactory(pictureFrameFactory) created");
    }

    if (m_parameters->checkEnablePicture() == true) {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);
    }

    if (m_previewWindow != NULL)
        m_previewWindow->set_timestamp(m_previewWindow, systemTime(SYSTEM_TIME_MONOTONIC));

#ifdef RAWDUMP_CAPTURE
    CLOGD("[RAWDUMP_CAP] setupThread Thread start pipeId(%d)", PIPE_3AA);
    m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
#else
    /* setup frame thread */
    if (m_parameters->isFlite3aaOtf() == false) {
        pipeId = PIPE_FLITE;
    }

    CLOGD("setupThread Thread start pipeId(%d)", pipeId);
    m_mainSetupQThread[INDEX(pipeId)]->run(PRIORITY_URGENT_DISPLAY);
#endif

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == false ||
           (m_parameters->getDualCameraMode() == true &&
            m_cameraId == masterCameraId)) {
        /* don't start previewThread in slave camera for dual camera */
#endif
    if (m_facedetectThread->isRunning() == false)
        m_facedetectThread->run();

    m_previewThread->run(PRIORITY_DISPLAY);
#ifdef USE_DUAL_CAMERA
    }
#endif

    m_mainThread->run(PRIORITY_DEFAULT);

#ifndef SUPPORT_FRONT_AF
    if (m_parameters->getCameraId() == CAMERA_ID_BACK)
#endif
    {
        m_autoFocusContinousThread->run(PRIORITY_DEFAULT);
    }
    m_monitorThread->run(PRIORITY_DEFAULT);

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        CLOGD("ZoomPreview with Scaler Thread start");
        m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
    }

    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
        (m_highResolutionCallbackRunning == false)) {
        CLOGD("High resolution preview callback start");
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
                CLOGE("[JQ] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
            }
            ret = uni_plugin_init(m_JQpluginHandle);
            if(ret < 0) {
                CLOGE("[JQ] Plugin init failed!!");
            }
        }
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (getCameraIdInternal() == CAMERA_ID_FRONT && m_STRCapturePluginHandle != NULL) {
        ret = uni_plugin_init(m_STRCapturePluginHandle);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] STR_CAPTURE Plugin init failed!!");
        }
    }
#endif

#ifdef SAMSUNG_STR_PREVIEW
    if (getCameraIdInternal() == CAMERA_ID_FRONT && m_STRPreviewPluginHandle != NULL) {
        ret = uni_plugin_init(m_STRPreviewPluginHandle);
        if (ret < 0) {
            CLOGE("[STR_PREVIEW] STR_PREVIEW Plugin init failed!!");
        }
    }
#endif

#ifdef SAMSUNG_HIFI_LLS
    if (m_parameters->checkHiFiLLSEnable() == true && m_LLSpluginHandle != NULL) {
        int powerCtrl = 1;
        ret = uni_plugin_set(m_LLSpluginHandle,
                HIFI_LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_POWER_CONTROL failed!! ret(%d)", ret);
        }
        m_hifiLLSEnabled = true;
    }
#endif

#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
    if (getCameraIdInternal() == CAMERA_ID_BACK
        && m_parameters->getRecordingHint() == false
        && m_FocusPeakingPluginHandle != NULL) {
        ret = uni_plugin_init(m_FocusPeakingPluginHandle);
        if (ret < 0) {
            CLOGE("[FocusPeaking] FocusPeaking Plugin init failed!!");
        }
        m_flagFocusPeakingStarted = false;
    }
#endif
#endif

#ifdef SAMSUNG_IDDQD
    if (m_parameters->getRecordingHint() == false
        && m_IDDQDPluginHandle != NULL) {
        ret = uni_plugin_init(m_IDDQDPluginHandle);
        if (ret < 0) {
            CLOGE("[IDDQD] IDDQD Plugin init failed!!");
        } else {
            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            ret = uni_plugin_set(m_IDDQDPluginHandle, IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[IDDQD] IDDQD Plugin set camera info failed!! (%d, %d)",
                    cameraInfo.CameraType, cameraInfo.SensorType);
            }
        }
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
#endif
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_START_END, false);
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_START_PREVIEW_END, true);
        if (m_previewWindow == NULL) {
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_START_PREVIEW_END, false);
            TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_TOTAL_TIME, false);
            if (g_timeLoggerLaunchIndex % TIME_LOGGER_LAUNCH_COUNT == 0)
                TIME_LOGGER_SAVE_BASE(&g_timeLoggerLaunch, m_cameraId);
        }
    }
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
            CLOGD(" eepromThread is NULL.");
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

#ifdef SAMSUNG_ROTATION
    if (m_rotationHandle != NULL) {
        sensor_listener_disable_sensor(m_rotationHandle, ST_ROTATION);
        sensor_listener_unload(&m_rotationHandle);
        m_rotationHandle = NULL;
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
    m_parameters->updateParameters();

    ret = m_setVisionBuffers();
    if (ret < 0) {
        CLOGE("m_setVisionBuffers() fail");
        return ret;
    }

#ifndef SUPPORT_IRIS_PREVIEW_CALLBACK
    if (m_scenario != SCENARIO_SECURE)
#endif
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

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    if (m_isQuickSwitchState == QUICK_SWITCH_STBY
        && m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_IDLE_TO_STBY) {
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_DONE);
        m_flagFirstPreviewTimerOn = false;
        m_parameters->setUseFastenAeStable(true);
        m_isFirstStart = true;
        CLOGD(" quick switching standby ");
        return;
    }
#endif

    char previewMode[100] = {0};

    property_get("persist.sys.camera.preview", previewMode, "");

    if (!strstr(previewMode, "0"))
        CLOGI(" persist.sys.camera.preview(%s)", previewMode);

    ExynosCameraActivityFlash *m_flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
    ExynosCameraFrameSP_sptr_t frame = NULL;

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
        CLOGI(" wait m_fastenAeThread");
        m_fastenAeThread->requestExitAndWait();
        CLOGI(" wait m_fastenAeThread end");
    } else {
        CLOGI("m_fastenAeThread is NULL");
    }
#endif

#ifdef SAMSUNG_COMPANION
    if(m_use_companion == true) {
        m_waitCompanionThreadEnd();
    }
#endif

    if (m_previewEnabled == false) {
        CLOGD(" preview is not enabled");
        property_set("persist.sys.camera.preview","0");
        return;
    }

    m_stopLongExposure = true;
    m_preLongExposureTime = 0;

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

#ifdef USE_QOS_SETTING
    if (m_previewFrameFactory == NULL) {
        CLOGW("m_previewFrameFactory is NULL. so, skip setControl(V4L2_CID_IS_DVFS_CLUSTER1)");
    } else {
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == false ||
                (m_parameters->getDualCameraMode() == true &&
                 m_cameraId == masterCameraId)) {
#endif
            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, 0, PIPE_3AA);
            if (ret < 0) {
                CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);
            }

            ret = m_previewFrameFactory->setControl(V4L2_CID_IS_DVFS_CLUSTER0, 0, PIPE_3AA);
            if (ret < 0) {
                CLOGE("V4L2_CID_IS_DVFS_CLUSTER0 setControl fail, ret(%d)", ret);
            }
#ifdef USE_DUAL_CAMERA
        }
#endif
    }
#endif
    storeSsrmCameraInfo(m_parameters->getCameraId(), 0, 0, 0, 0);

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

    if (m_previewFrameFactory != NULL) {
        m_previewFrameFactory->setStopFlag();
    }

    for (int i = 0; i < m_getNumOfReprocessingInstance(); i++) {
        if (m_parameters->isReprocessing() == true && m_pReprocessingFrameFactories[i]->isCreated() == true)
            m_pReprocessingFrameFactories[i]->setStopFlag();
    }

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

        CLOGD("High resolution preview callback stop");
        if(getCameraIdInternal() == CAMERA_ID_FRONT || getCameraIdInternal() == CAMERA_ID_BACK_1) {
            m_sccCaptureSelector->cancelPicture();
            m_sccCaptureSelector->wakeupQ();
            CLOGD("High resolution m_sccCaptureSelector cancel");
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
    m_facedetectQ->release();

#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthCallbackBuffer;

    if (m_previewFrameFactory != NULL) {
        m_previewFrameFactory->setRequest(PIPE_VC1, false);
    }
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
        CLOGE("m_stopPreviewInternal fail");

#ifdef SUPPORT_SW_VDIS
    int cameraId = getCameraIdInternal();
    if (m_swVDIS_Mode && (m_swVDIS_ModeDual == false || (m_swVDIS_ModeDual == true && cameraId != CAMERA_ID_BACK_1))) {
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

    /* skip to free and reallocate buffers : flite / 3aa / isp / ispReprocessing */
    if (m_bayerBufferMgr != NULL) {
        m_bayerBufferMgr->resetBuffers();
    }
#ifdef SUPPORT_DEPTH_MAP
    if (m_depthMapBufferMgr != NULL) {
        m_depthMapBufferMgr->deinit();
    }
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    if (m_parameters->getUsePureBayerReprocessing() == false) {
        if (m_fliteBufferMgr != NULL) {
#if defined(SAMSUNG_DNG_DIRTY_BAYER)
            m_fliteBufferMgr->deinit();
#else
            m_fliteBufferMgr->resetBuffers();
#endif
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

    if (m_vraBufferMgr != NULL) {
        m_vraBufferMgr->deinit();
    }

    /* realloc reprocessing buffer for change burst panorama <-> normal mode */
    if (m_ispReprocessingBufferMgr != NULL) {
        m_ispReprocessingBufferMgr->resetBuffers();
    }
    if (m_sccReprocessingBufferMgr != NULL) {
        m_sccReprocessingBufferMgr->deinit(m_cameraId);
    }
    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit();
    }
#ifdef USE_MSC_CAPTURE
    if (m_mscBufferMgr != NULL) {
        m_mscBufferMgr->deinit();
    }
#endif
    /* realloc callback buffers */
    if (m_scpBufferMgr != NULL) {
        m_scpBufferMgr->deinit();
        m_scpBufferMgr->setBufferCount(0);
    }
    if (m_zoomScalerBufferMgr != NULL) {
        m_zoomScalerBufferMgr->deinit();
    }
#ifdef USE_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }

    if (m_fusionReprocessingBufferMgr != NULL) {
        m_fusionReprocessingBufferMgr->deinit();
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
        m_postPictureBufferMgr->deinit(m_cameraId);
    }

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->deinit();
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (m_strCaptureBufferMgr != NULL) {
        m_strCaptureBufferMgr->deinit();
    }
#endif

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit(m_cameraId);
    }

#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        m_lbpBufferMgr->deinit();
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
    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

#ifdef USE_DUAL_CAMERA
    /* free all buffers cause of not using dual camera */
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
#else
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
    m_previewCallbackToggle = 0;

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
            CLOGE("m_stopCompanion() fail");

        CLOGD(" change state (main -> stby)");
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

#ifdef SAMSUNG_DUAL_SOLUTION
    if (m_parameters->getCameraId() == CAMERA_ID_BACK) {
        if (m_fusionPreviewWrapper->m_getIsInit() == true)
            m_fusionPreviewWrapper->m_deinitDualSolution(getCameraId());
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (m_fusionCaptureWrapper->m_getIsInit() == true)
            m_fusionCaptureWrapper->m_deinitDualSolution(getCameraId());
#endif
    }
#endif /* SAMSUNG_DUAL_SOLUTION */

#ifdef SAMSUNG_LENS_DC
    if (m_DCpluginHandle != NULL && m_parameters->getLensDCMode()) {
        m_skipLensDC = true;
        m_LensDCIndex = -1;
        ret = uni_plugin_deinit(m_DCpluginHandle);
        if(ret < 0) {
            CLOGE("[DC] Plugin deinit failed!!");
        }
    }
#endif

#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_deinit(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ] Plugin deinit failed!!");
        }
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (getCameraIdInternal() == CAMERA_ID_FRONT && m_STRCapturePluginHandle != NULL) {
        ret = uni_plugin_deinit(m_STRCapturePluginHandle);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] STR_CAPTURE Plugin deinit failed!!");
        }
    }
#endif

#ifdef SAMSUNG_STR_PREVIEW
    if (getCameraIdInternal() == CAMERA_ID_FRONT && m_STRPreviewPluginHandle != NULL) {
        ret = uni_plugin_deinit(m_STRPreviewPluginHandle);
        if (ret < 0) {
            CLOGE("[STR_PRVIEW] STR_PREVIEW Plugin deinit failed!!");
        }
    }
#endif

#ifdef SAMSUNG_HIFI_LLS
    if (m_hifiLLSEnabled == true && m_LLSpluginHandle != NULL) {
        int powerCtrl = 0;
        ret = uni_plugin_set(m_LLSpluginHandle,
                HIFI_LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_POWER_CONTROL failed!! ret(%d)", ret);
        }
        m_hifiLLSEnabled = false;
    }
#endif

#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
    if (getCameraIdInternal() == CAMERA_ID_BACK
        && m_parameters->getRecordingHint() == false
        && m_FocusPeakingPluginHandle != NULL) {
        ret = uni_plugin_deinit(m_FocusPeakingPluginHandle);
        if (ret < 0) {
            CLOGE("[FocusPeaking] FocusPeaking Plugin deinit failed!!");
        }
        m_flagFocusPeakingStarted = false;
    }
#endif
#endif

#ifdef SAMSUNG_IDDQD
    if (m_IDDQDPluginHandle != NULL) {
        ret = uni_plugin_deinit(m_IDDQDPluginHandle);
        if (ret < 0) {
            CLOGE("[IDDQD] IDDQD Plugin deinit failed!!");
        }
    }
#endif

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF]Out-focus shot parameter is not reset. Reset here forcely!!: %d",
                 m_lensmoveCount);
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

#ifdef SAMSUNG_HLV
    int curVideoW = 0, curVideoH = 0;
    uint32_t curMinFps = 0, curMaxFps = 0;
#endif

    TIME_LOGGER_INIT(m_cameraId);

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
        if (m_parameters->getFaceDetectionMode(false) == false) {
            m_enableFaceDetection(false);
        } else {
            /* stay current fd mode */
        }
    } else {
        CLOGW("m_parameters is NULL");
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
        CLOGE("");
        return ret;
    }

#ifdef SAMSUNG_HLV
    m_parameters->getVideoSize(&curVideoW, &curVideoH);
    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (m_HLV == NULL
        && curVideoW <= 1920 && curVideoH <= 1080
        && curMinFps <= 60 && curMaxFps <= 60
        && m_parameters->getSamsungCamera()
#ifdef USE_LIVE_BROADCAST
        && m_parameters->getPLBMode() == false
#endif
#ifdef SAMSUNG_HYPER_MOTION
        && m_parameters->getHyperMotionMode() == false
#endif
        && m_parameters->isSWVdisMode() == false) {
        CLOGD("HLV : uni_plugin_load !");
        m_HLV = uni_plugin_load(HIGHLIGHT_VIDEO_PLUGIN_NAME);
        if (m_HLV != NULL) {
            CLOGD("HLV : uni_plugin_load success!");

            ret = uni_plugin_init(m_HLV);
            if (ret < 0) {
                CLOGE("HLV : uni_plugin_init fail . Unload %s", HIGHLIGHT_VIDEO_PLUGIN_NAME);

                ret = uni_plugin_unload(&m_HLV);
                if (ret)
                    CLOGE("HLV : uni_plug_unload failed !!");

                m_HLV = NULL;
            }
            else
            {
                CLOGD("HLV : uni_plugin_init success!");
                UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START;
                CLOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START");

                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
        }
        else {
            CLOGD("HLV : uni_plugin_load fail!");
        }
    }
    else {
        CLOGD("HLV : Not Supported for %dx%d(%d fps) video record !",
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
        bool useMCSC2Pipe = true;

#ifdef SUPPORT_SW_VDIS
        /* VDIS uses MCSC0 only */
        if (m_swVDIS_Mode == true)
            useMCSC2Pipe = false;
#endif
#ifdef USE_DUAL_CAMERA
        /* If dual camera, use libcsc only */
        if (m_parameters->getDualCameraMode() == true)
            useMCSC2Pipe = false;
#endif
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

    TIME_LOGGER_SAVE(m_cameraId);

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

#ifdef SAMSUNG_HLV
    if (m_HLV != NULL)
    {
        UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP;
        CLOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP");

        uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                &value);
    }
#endif

    /* Do stop recording process */
    ret = m_stopRecordingInternal();
    if (ret < 0)
        CLOGE("m_stopRecordingInternal fail");

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
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    int32_t reprocessingBayerMode = 0;
    int retryCount = 0;
    ExynosCameraActivityFlash *flashMgr = m_exynosCameraActivityControl->getFlashMgr();
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_parameters->getDualCameraMode() == true) {
        /* lock syncType during takePicture */
        m_parameters->lockDualCameraSyncType(true);

#ifdef SAMSUNG_DUAL_SOLUTION
        CLOGI("takePicture standbyMode(%d), syncType(%d) forceWide(%d), fusionCapture(%d)",
                m_parameters->getDualStandbyMode(),
                m_parameters->getDualCameraReprocessingSyncType(),
                m_parameters->getForceWide(),
                m_parameters->getFusionCaptureMode(m_cameraId, true));
#else
        CLOGI("takePicture standbyMode(%d), syncType(%d)",
                m_parameters->getDualStandbyMode(),
                m_parameters->getDualCameraReprocessingSyncType());
#endif

        switch (m_parameters->getDualCameraReprocessingSyncType()) {
        case SYNC_TYPE_BYPASS:
            if (m_cameraId == slaveCameraId) {
                CLOGI("takePicture will be skipped(%d)(syncType:%d)",
                        m_parameters->getDualStandbyMode(),
                        m_parameters->getDualCameraReprocessingSyncType());
                return NO_ERROR;
            }
            break;
        case SYNC_TYPE_SWITCH:
            if (m_cameraId == masterCameraId) {
                CLOGI("takePicture will be skipped(%d)(syncType:%d)",
                        m_parameters->getDualStandbyMode(),
                        m_parameters->getDualCameraReprocessingSyncType());
                return NO_ERROR;
            }
            break;
        case SYNC_TYPE_SYNC:
            /* takePicture will be running in only master for burst mode / flash */
            if ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) ||
                (m_parameters->getFlashMode() == FLASH_MODE_ON) ||
                (flashMgr->getNeedCaptureFlash() == true && m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE)
#ifdef SAMSUNG_DUAL_SOLUTION
                || (m_parameters->getForceWide() == true)
                || (m_parameters->getFusionCaptureMode(m_cameraId, true) == false)
#endif
            ) {
                if (m_cameraId == slaveCameraId) {
                    CLOGI("takePicture will be skipped(%d)(syncType:%d)",
                            m_parameters->getDualStandbyMode(),
                            m_parameters->getDualCameraReprocessingSyncType());
                    return NO_ERROR;
                }
            }
            break;
        default:
            break;
        }
    }
#endif

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

        if(m_preLongExposureTime == 0)
            break;

        usleep(m_preLongExposureTime);

        if (++retryCount > 7) {
            CLOGE("DEBUG(%s[%d]):HAL can't set preview exposure time.", __FUNCTION__, __LINE__);
            m_preLongExposureTime = 0;
            return INVALID_OPERATION;
        }
    }

    retryCount = 0;
    m_preLongExposureTime = 0;

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
        CLOGW(" Vision mode does not support");
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
        if((getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1) &&
            currentSeriesShotMode != SERIES_SHOT_MODE_BURST) {
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
            || ((getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1)
                    && m_parameters->getOISCaptureModeOn() == true)
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
                        CLOGD(" One second burst start - TimeCheck");
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
            CLOGD(" Continue One second burst - TimeCheck");
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
                    CLOGE("couldn't run pre-picture thread");
                    m_captureLock.unlock();
                    return INVALID_OPERATION;
                }
            }
            if (m_pictureThread->isRunning() == false) {
                if (m_pictureThread->run() != NO_ERROR) {
                    CLOGE("couldn't run picture thread, ret(%d)", ret);
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

    CLOGI(" takePicture start m_takePictureCounter(%d), currentSeriesShotMode(%d) seriesShotCount(%d) reprocessingMode(%d)",
         m_takePictureCounter.getCount(), currentSeriesShotMode, seriesShotCount,
         reprocessingBayerMode);

    m_printExynosCameraInfo(__FUNCTION__);

    int pipeId = m_getBayerPipeId();

    /* TODO: Dynamic bayer capture, currently support only single shot */
    if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC) {
        m_captureSelector->clearList(pipeId, false, m_previewFrameFactory->getNodeType(PIPE_VC0));

        m_previewFrameFactory->setRequestBayer(true);

    } else if (reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        /* Comment out, because it included 3AA, it always running */
        /*
        int pipeId = m_getBayerPipeId();

        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0) {
            m_previewFrameFactory->setRequest3AC(true);
            ret = m_generateFrame(-1, &newFrame);
            if (ret < 0) {
                CLOGE("generateFrame fail");
                return ret;
            }
            m_previewFrameFactory->setRequest3AC(false);

            ret = m_setupEntity(pipeId, newFrame);
            if (ret < 0) {
                CLOGE("setupEntity fail");
                return ret;
            }

            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
            m_previewFrameFactory->pushFrameToPipe(&newFrame, pipeId);
        }

        m_previewFrameFactory->startThread(pipeId);
        */

        /*
         * Always captureSelector's frame must be cleared.
         * Because too old bayer can be selected and it cause the DDK assert
         */
        m_captureSelector->clearList(pipeId, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
        if (m_bayerBufferMgr->getNumOfAvailableBuffer() > 0)
            m_previewFrameFactory->setRequest3AC(true);
    }

#ifdef DEBUG_RAWDUMP
    CLOGD("[RAWDUMP] increase m_dynamicBayerCount. pureBayerReproc = %d", m_parameters->getUsePureBayerReprocessing());
    if (m_parameters->getUsePureBayerReprocessing() == false) {
        int buffercount = 1;
        m_dynamicBayerCount += buffercount;
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
                        CLOGW(" fail to boosting dynamic capture");
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

        CLOGI(" takePicture enabled, takePictureCount(%d)",
                 m_takePictureCounter.getCount());
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
                CLOGD(" realloc buffer for burst shot");
                m_burstRealloc = false;

                if (m_parameters->isUseHWFC() == false) {
                    allocCount = m_sccReprocessingBufferMgr->getAllocatedBufferCount();
                    addCount = (allocCount <= NUM_BURST_GSC_JPEG_INIT_BUFFER)?(NUM_BURST_GSC_JPEG_INIT_BUFFER-allocCount):0;
                    if( addCount > 0 ){
                        m_sccReprocessingBufferMgr->increase(addCount);
#ifdef USE_DUAL_CAMERA
                        if (m_parameters->getDualCameraMode() == true) {
                            m_fusionReprocessingBufferMgr->increase(addCount);
                        }
#endif
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

#ifdef RAWDUMP_CAPTURE
        CLOGD("[RAWDUMP_CAP] set RAW capture mode, Bypass companion. m_use_companion = %d", m_use_companion);
        sCaptureMgr->resetRawCaptureFcount();
        sCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_RAW);
        m_parameters->setRawCaptureModeOn(true);
        enum aa_capture_intent captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_RAWDUMP;
        if (m_use_companion == true) {
            CLOGD(" start set Raw Capture mode");
            captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_COMP_BYPASS;
        } else {
            CLOGD(" start set Raw Capture mode(NON-C3)");
        }
        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
        if (ret < 0) {
            CLOGE("setControl(STILL_CAPTURE_RAW) fail. ret(%d) intent(%d)",
             ret, captureIntent);
        }
        sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
#else
#ifdef OIS_CAPTURE
        if ((getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1)
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
                CLOGD("LLS_MBR currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                        currentSeriesShotMode, m_parameters->getLDCaptureMode());
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
                CLOGD("[DC] Set Lens Distortion Correction, getLensDCEnable(%d)",
                         m_parameters->getLensDCEnable());
            }
#endif
        }
#endif

#ifdef SAMSUNG_STR_CAPTURE
        if (m_parameters->checkSTRCaptureMode() && m_STRCapturePluginHandle != NULL) {
            m_parameters->setOutPutFormatNV21Enable(true);
            m_parameters->setSTRCaptureEnable(true);
            CLOGD("[STR_CAPTURE] Set STR Capture, getSTRCaptureEnable(%d)",
                     m_parameters->getSTRCaptureEnable());
        } else {
            m_parameters->setSTRCaptureEnable(false);
        }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
        if (getCameraIdInternal() == CAMERA_ID_FRONT
            && currentSeriesShotMode == SERIES_SHOT_MODE_NONE
            && m_getRecordingEnabled() == false
            && isLDCapture(getCameraIdInternal()) == true) {
            m_parameters->checkLDCaptureMode();
            CLOGD("LLS_MBR currentSeriesShotMode(%d), getLDCaptureMode(%d)",
                    currentSeriesShotMode, m_parameters->getLDCaptureMode());
        }
#endif

#ifdef LLS_STUNR
        CLOGE("STUNR before checking stunr mode");
        m_parameters->checkLLSStunrMode();
#endif

#ifdef USE_ODC_CAPTURE
        if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#ifdef CAMERA_GED_FEATURE
            || (m_parameters->getSeriesShotCount() > 1)
#endif
#ifdef SAMSUNG_LENS_DC
            || m_parameters->getLensDCEnable()
#endif
        ) {
            m_parameters->setODCCaptureEnable(false);
        } else {
            m_parameters->setODCCaptureEnable(m_parameters->getODCCaptureMode());
        }
#endif

        if (m_parameters->getCaptureExposureTime() != 0) {
            m_longExposureRemainCount = m_parameters->getLongExposureShotCount();
            m_preLongExposureTime = m_parameters->getLongExposureTime();
            CLOGD(" m_longExposureRemainCount(%d)",
                m_longExposureRemainCount);
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
            CLOGD("[JQ] ON!!");
            m_isJpegQtableOn = true;
            m_parameters->setJpegQtableOn(true);
        }
        else {
            CLOGD("[JQ] OFF!!");
            m_isJpegQtableOn = false;
            m_parameters->setJpegQtableOn(false);
        }
#endif

#ifdef SAMSUNG_FRONT_LCD_FLASH
        enum ExynosCameraActivityFlash::FLASH_STEP curFlashStep = ExynosCameraActivityFlash::FLASH_STEP_OFF;
        if (getCameraIdInternal() == CAMERA_ID_FRONT) {
            flashMgr->getFlashStep(&curFlashStep);
        }
#endif

        if (m_hdrEnabled == true) {
            seriesShotCount = HDR_REPROCESSING_COUNT;
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            sCaptureMgr->resetHdrStartFcount();
            m_parameters->setFrameSkipCount(13);
        } else if (currentSeriesShotMode == SERIES_SHOT_MODE_NONE
                    && (flashMgr->getNeedCaptureFlash() == true
#ifdef SAMSUNG_FRONT_LCD_FLASH
                        || curFlashStep == ExynosCameraActivityFlash::FLASH_STEP_PRE_LCD_ON
#endif
            )) {
            if (m_parameters->getCaptureExposureTime() != 0) {
                m_stopLongExposure = false;
                flashMgr->setManualExposureTime(m_parameters->getLongExposureTime() * 1000);
            }

            m_parameters->setMarkingOfExifFlash(1);

#ifdef SAMSUNG_FRONT_LCD_FLASH
            if (getCameraIdInternal() == CAMERA_ID_FRONT) {
                if (curFlashStep == ExynosCameraActivityFlash::FLASH_STEP_PRE_LCD_ON) {
                    CLOGD("DEBUG(%s[%d]) : FLASH_STEP_LCD_ON", __FUNCTION__, __LINE__);
                    flashMgr->resetShotFcount();
                    flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_LCD_ON);
                }
            }
            else
#endif
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
#ifdef OIS_CAPTURE
        else if (m_parameters->getOISCaptureModeOn() == true
                 && (getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1)) {
            CLOGD(" start set zsl-like mode");

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

                    switch (m_parameters->getLDCaptureLLSValue()) {
                        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2:
                        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3:
                        case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4:
#ifdef SUPPORT_ZSL_MULTIFRAME
                        case LLS_LEVEL_MULTI_MERGE_LOW_2:
                        case LLS_LEVEL_MULTI_MERGE_LOW_3:
                        case LLS_LEVEL_MULTI_MERGE_LOW_4:
#endif
                            captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT;
                            break;
                        default:
                            captureIntent = AA_CAPTRUE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT;
                            break;
                    }

                    mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
                    value = mask;

                    CLOGD(" start set multi mode captureIntent(%d)",captureIntent);
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

                CLOGD("setExtControl intent:%d, count:%d, exposuretime:%d",
                        captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);

                ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setExtControl to 3AA");
                }
            } else
#endif
            {
                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("setControl(V4L2_CID_IS_INTENT) fail. ret(%d) intent(%d)",
                            ret, value);
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

                CLOGD("setExtControl intent:%d, count:%d, exposuretime:%d",
                    captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);

                ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setExtControl to 3AA");
                }
#else
                mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
                value = mask;

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_CAPTURE_EXPOSURETIME, captureExposureTime, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("setControl(V4L2_CID_IS_CAPTURE_EXPOSURETIME) fail. ret(%d) intent(%d)",
                            ret, captureExposureTime);
                }

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("setControl(V4L2_CID_IS_INTENT) fail. ret(%d) intent(%d)",
                            ret, value);
                }
#endif

                m_stopLongExposure = false;

#ifdef OIS_CAPTURE
                m_parameters->setOISCaptureModeOn(true);
#endif
                sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
#ifdef OIS_CAPTURE
                m_exynosCameraActivityControl->setOISCaptureMode(true);
#endif
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
        if(currentSeriesShotMode == SERIES_SHOT_MODE_LLS
#ifdef SR_CAPTURE
            && m_parameters->getSROn() == false
#endif
        ) {
            CLOGD(" set LLS Capture mode on");
            m_parameters->setLLSCaptureOn(true);
        }
#endif
#endif /* RAWDUMP_CAPTURE */

#ifdef SAMSUNG_LBP
        if(currentSeriesShotMode == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
            int TotalBufferNum = (int)m_parameters->getHoldFrameCount();
            ret = uni_plugin_set(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &TotalBufferNum);
            if(ret < 0) {
                CLOGE("[LBP]Bestphoto plugin set failed!!");
            }
            ret = uni_plugin_init(m_LBPpluginHandle);
            if(ret < 0) {
                CLOGE("[LBP]Bestphoto plugin init failed!!");
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
#ifdef SAMSUNG_STR_CAPTURE
            && !m_parameters->getSTRCaptureEnable()
#endif
           ) {
            int thumbnailW = 0, thumbnailH = 0;
            m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);

            if ((thumbnailW > 0 && thumbnailH > 0)
                && !m_parameters->getRecordingHint()
#ifdef USE_DUAL_CAMERA
                && !(m_parameters->getDualCameraMode() == false &&
                    m_parameters->getDualMode())
#else
                && !m_parameters->getDualMode()
#endif
                && currentSeriesShotMode != SERIES_SHOT_MODE_LLS
#ifdef SAMSUNG_MAGICSHOT
                && m_parameters->getShotMode() != SHOT_MODE_MAGIC
#endif
                && m_parameters->getShotMode() != SHOT_MODE_OUTFOCUS
                && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                m_parameters->setIsThumbnailCallbackOn(true);
                CLOGI("m_isThumbnailCallbackOn(%d)", m_parameters->getIsThumbnailCallbackOn());
            }
        }
#endif

        m_parameters->setSetfileYuvRange();

        if (m_parameters->getPictureFormat() == V4L2_PIX_FMT_NV21) {
            m_parameters->setOutPutFormatNV21Enable(true);
            m_nv21CaptureEnabled = true;
        }

#ifdef SAMSUNG_LLS_DEBLUR
        int pictureCount = m_parameters->getLDCaptureCount();

        if (m_parameters->getLDCaptureMode())
            m_reprocessingCounter.setCount(pictureCount);
        else
#endif
        {
#ifdef LLS_STUNR
            if (m_parameters->getLLSStunrMode()) {
                m_captureSelector->setFrameHoldCount(m_parameters->getLLSStunrCount());
                m_reprocessingCounter.setCount(m_parameters->getLLSStunrCount());
                CLOGD("STUNR : reprocessingCount(%d)", m_reprocessingCounter.getCount());
            } else
#endif
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

#ifdef SAMSUNG_LLS_DEBLUR
        if (m_parameters->getLDCaptureMode()) {
            m_jpegCounter.setCount(seriesShotCount);
            m_pictureCounter.setCount(pictureCount);
            m_yuvcallbackCounter.setCount(seriesShotCount);

            CLOGD("LLS_MBR pictureCount(%d), getLDCaptureMode(%d)",
                    pictureCount, m_parameters->getLDCaptureMode());
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
                CLOGE("couldn't run picture thread, ret(%d)", ret);
                return INVALID_OPERATION;
            }
        }

        /* HDR, LLS, SIS should make YUV callback data. so don't use jpeg thread */
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_nv21CaptureEnabled ||
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
            m_yuvcallbackCounter.setCount(0);
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
        if (!(m_hdrEnabled == true ||
                currentSeriesShotMode == SERIES_SHOT_MODE_LLS ||
                currentSeriesShotMode == SERIES_SHOT_MODE_SIS ||
                m_nv21CaptureEnabled ||
#ifdef ONE_SECOND_BURST_CAPTURE
                currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
#endif
                m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA)) {
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
        if (m_parameters->isReprocessing() == false)
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

        CLOGD("setExtControl intent:%d, count:%d, exposuretime:%d",
                captureCtrl.capture_intent, captureCtrl.capture_count, captureCtrl.capture_exposureTime);

        ret = m_previewFrameFactory->setExtControl(&extCtrls, PIPE_3AA);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setExtControl to 3AA");
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
    CLOGI("");

    if (m_parameters != NULL) {
        if (m_parameters->getVisionMode() == true) {
            CLOGW(" Vision mode does not support");
            android_printAssert(NULL, LOG_TAG, "Cannot support this operation");

            return INVALID_OPERATION;
        }
    }

    m_autoFocusRunning = false;

#ifdef SAMSUNG_DOF
    if(m_lensmoveCount) {
        CLOGW("[DOF] Out-focus shot parameter is not reset. Reset here forcely!!: %d",
                 m_lensmoveCount);
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setStartLensMove(false);
        m_lensmoveCount = 0;
        if (m_parameters != NULL) {
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->setMoveLensTotal(m_lensmoveCount);
    }
    }
#endif

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
        CLOGI(" m_resetPreview(%d)", m_resetPreview);

        if (ret < 0)
            CLOGE(" restart preview internal fail");
    } else {
        ret = m_parameters->setParameters(params);
    }

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (m_parameters->getUseDNGCapture() == true) {
        if(m_parameters->getDNGCaptureModeOn() == true)
            m_previewFrameFactory->setRequestBayer(true);
        else
            m_previewFrameFactory->setRequestBayer(false);
    }
#endif
#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    m_previewFrameFactory->setRequestBayer(true);
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_isFirstParametersSet == false
#ifdef USE_DUAL_CAMERA
        && m_parameters->isFastenAeStableEnable()
#else
        && m_parameters->getDualMode() == false
#endif
#ifndef CAMERA_GED_FEATURE
        && m_parameters->getSamsungCamera() == true
#endif
        && m_parameters->getVisionMode() == false
#ifdef SAMSUNG_QUICK_SWITCH
        && (m_isQuickSwitchState == QUICK_SWITCH_IDLE)
#endif
        && m_previewEnabled == false
        ) {
        CLOGD("1st setParameters");
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (m_parameters->getDualCameraMode()) {
            debug_attribute_t *debuginfo;
            debuginfo = m_parameters->getDebugAttribute();
            debuginfo->debugSize[APP_MARKER_4] += (sizeof(struct camera2_udm) + sizeof(struct ois_exif_data));
            if (debuginfo->debugData[APP_MARKER_4]) {
                delete [] debuginfo->debugData[APP_MARKER_4];
                debuginfo->debugData[APP_MARKER_4] = new char[debuginfo->debugSize[APP_MARKER_4]];
                memset((void *)debuginfo->debugData[APP_MARKER_4], 0, debuginfo->debugSize[APP_MARKER_4]);
            }

            debuginfo = m_parameters->getDebug2Attribute();
            debuginfo->debugSize[APP_MARKER_4] += (sizeof(struct camera2_udm) + sizeof(struct ois_exif_data));
            if (debuginfo->debugData[APP_MARKER_4]) {
                delete [] debuginfo->debugData[APP_MARKER_4];
                debuginfo->debugData[APP_MARKER_4] = new char[debuginfo->debugSize[APP_MARKER_4]];
                memset((void *)debuginfo->debugData[APP_MARKER_4], 0, debuginfo->debugSize[APP_MARKER_4]);
            }
        }
#endif
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
    ExynosCameraActivityAutofocus *autoFocusMgr = NULL;
#endif
#ifdef SAMSUNG_OIS
    int zoomLevel;
    int zoomRatio;
#endif

    CLOGV(" ");

    if (m_parameters != NULL) {
#ifdef SAMSUNG_OIS
        zoomLevel = m_parameters->getZoomLevel();
        zoomRatio = m_parameters->getZoomRatio(zoomLevel);
#endif
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
#ifdef SAMSUNG_OT
    case 1504: /* HAL_OBJECT_TRACKING_STARTSTOP */
        autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        if (m_OTpluginHandle == NULL) {
            CLOGE("[OBTR] HAL_OBJECT_TRACKING_STARTSTOP is called but handle is null !!");
#ifdef SAMSUNG_DUAL_SOLUTION
            if (m_parameters->getDualCameraMode() == true) {
                if (arg1) {
                    autoFocusMgr->setObjectTrackingStart(true);
                } else {
                    autoFocusMgr->setObjectTrackingStart(false);
                }
            }
#endif
            break;
        }
        CLOGD("HAL_OBJECT_TRACKING_STARTSTOP(%d) is called!", arg1);
        if (arg1) {
            /* Start case */
            int ret = 0;
            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

            ret = uni_plugin_init(m_OTpluginHandle);
            if (ret < 0) {
                CLOGE("[OBTR] Object Tracking plugin init failed!!");
            }
            ret = uni_plugin_set(m_OTpluginHandle,
                          OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[OBTR] Object Tracking Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
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
            CLOGD("HAL_OBJECT_TRACKING_STARTSTOP(%d) Out is called!", arg1);
#endif
        }
        break;
#endif
#ifdef BURST_CAPTURE
    case 1571: /* HAL_CMD_RUN_BURST_TAKE */
        CLOGD("HAL_CMD_RUN_BURST_TAKE is called!");
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
                CLOGE("[BD] Plugin init failed!!");

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
        CLOGD("HAL_CMD_STOP_BURST_TAKE is called!");
        m_takePictureCounter.setCount(0);

#ifdef OIS_CAPTURE
        if (getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1) {
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
                CLOGE("[BD] Plugin deinit failed!!");

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
#ifdef LLS_CAPTURE
    case 1600: /* CAMERA_CMD_START_BURST_PANORAMA */
        CLOGD("CAMERA_CMD_START_BURST_PANORAMA is called!");
        m_takePictureCounter.setCount(0);
        break;
    case 1601: /*CAMERA_CMD_STOP_BURST_PANORAMA */
        CLOGD("CAMERA_CMD_STOP_BURST_PANORAMA is called!");
        m_takePictureCounter.setCount(0);
        break;
    case 1516: /*CAMERA_CMD_START_SERIES_CAPTURE */
        CLOGD("CAMERA_CMD_START_SERIES_CAPTURE is called!%d", arg1);
#ifdef ONE_SECOND_BURST_CAPTURE
        m_clearOneSecondBurst(false);
#endif
        m_parameters->setOutPutFormatNV21Enable(true);
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_LLS, arg1);
        m_takePictureCounter.setCount(0);
        m_yuvcallbackCounter.setCount(0);
        break;
    case 1517: /* CAMERA_CMD_STOP_SERIES_CAPTURE */
        CLOGD("CAMERA_CMD_STOP_SERIES_CAPTURE is called!");
        m_parameters->setOutPutFormatNV21Enable(false);
        m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        m_takePictureCounter.setCount(0);
#ifdef SET_LLS_CAPTURE_SETFILE
        m_parameters->setLLSCaptureOn(false);
#endif
#ifdef OIS_CAPTURE
        if ((getCameraIdInternal() == CAMERA_ID_BACK || getCameraIdInternal() == CAMERA_ID_BACK_1)
            && m_parameters->getOISCaptureModeOn() == false) {
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setMultiCaptureMode(false);
        }
#endif
        break;
#endif
    case 1351: /*CAMERA_CMD_AUTO_LOW_LIGHT_SET */
        CLOGD("CAMERA_CMD_AUTO_LOW_LIGHT_SET is called!%d", arg1);
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
        CLOGD("HAL_ENABLE_LIGHT_CONDITION is called!%d", arg1);
        if(arg1) {
            m_flagLightCondition = true;
        } else {
            m_flagLightCondition = false;
        }
        break;
#ifdef SAMSUNG_CAMERA_EXTRA_INFO
    case 1802: /* HAL_ENABLE_FLASH_AUTO_CALLBACK*/
        CLOGD("HAL_ENABLE_FLASH_AUTO_CALLBACK is called!%d", arg1);
        if(arg1) {
            m_flagFlashCallback = true;
        } else {
            m_flagFlashCallback = false;
        }
        break;
    case 1803: /* HAL_ENABLE_HDR_AUTO_CALLBACK*/
        CLOGD("HAL_ENABLE_HDR_AUTO_CALLBACK is called!%d", arg1);
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

            CLOGD("LCD_FLASH is called!%d", arg1);
            if (arg1) {
                /* setHighBrightnessModeOfLCD(1, m_prevHBM, m_prevAutoHBM); */
                flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_PRE_LCD_ON);
            } else {
                flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_LCD_OFF);
                /* setHighBrightnessModeOfLCD(0, m_prevHBM, m_prevAutoHBM); */
            }
        }
        break;
#endif
    case 1431: /* CAMERA_CMD_START_MOTION_PHOTO */
        CLOGD("CAMERA_CMD_START_MOTION_PHOTO is called!");
        m_parameters->setMotionPhotoOn(true);
        break;
    case 1432: /* CAMERA_CMD_STOP_MOTION_PHOTO */
        CLOGD("CAMERA_CMD_STOP_MOTION_PHOTO is called!)");
        m_parameters->setMotionPhotoOn(false);
        break;
#ifdef SR_CAPTURE
    case 1519: /* CAMERA_CMD_SUPER_RESOLUTION_MODE */
        CLOGD("CAMERA_CMD_SUPER_RESOLUTION_MODE is called!%d", arg1);
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
        CLOGD("HAL_SET_SAMSUNG_CAMERA is called!%d", arg1);
        m_parameters->setSamsungCamera(true);
        break;
    /* 1510: CAMERA_CMD_SET_FLIP */
    case 1510 :
        CLOGD("CAMERA_CMD_SET_FLIP is called!%d", arg1);
        m_parameters->setFlipHorizontal(arg1);
        break;
#ifndef SAMSUNG_ROTATION
    /* 1521: CAMERA_CMD_DEVICE_ORIENTATION */
    case 1521:
        CLOGD("CAMERA_CMD_DEVICE_ORIENTATION is called!%d", arg1);
        m_parameters->setDeviceOrientation(arg1);
        uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
        if (uctlMgr != NULL)
            uctlMgr->setDeviceRotation(m_parameters->getFdOrientation());
        break;
#endif
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
#ifdef SAMSUNG_DOF
    /*1644: HAL_ENABLE_PAF_RESULT */
    case 1644:
        CLOGD("HAL_ENABLE_PAF_RESULT is called!%d", arg1);
        break;
    /*1650: CAMERA_CMD_START_PAF_CALLBACK */
    case 1650:
        CLOGD("CAMERA_CMD_START_PAF_CALLBACK is called!%d", arg1);
        autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setPDAF(true);
        break;
    /*1651: CAMERA_CMD_STOP_PAF_CALLBACK */
    case 1651:
        CLOGD("CAMERA_CMD_STOP_PAF_CALLBACK is called!%d", arg1);
        autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
        autoFocusMgr->setPDAF(false);
        break;
#endif
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
#ifdef SAMSUNG_OIS
        m_parameters->setOISMode();
#endif
        break;
#ifdef SAMSUNG_HLV
    case 1721:  /* HAL_RECORDING_RESUME */
        {
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME;
            CLOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_RESUME");

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                CLOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;

    case 1722: /* HAL_RECORDING_PAUSE */
        {
            CLOGD("HLV: set UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE");
            UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_PAUSE;

            if (m_HLV) {
                uni_plugin_set(m_HLV,
                        HIGHLIGHT_VIDEO_PLUGIN_NAME,
                        UNI_PLUGIN_INDEX_CAPTURE_STATUS,
                        &value);
            }
            else {
                CLOGE("HLV: %s not yet loaded", HIGHLIGHT_VIDEO_PLUGIN_NAME);
            }
        }
        break;
#endif
#ifdef USE_LIVE_BROADCAST
    case 1730: /* SET PLB mode */
        CLOGD("SET PLB mode is called!%d", arg1);
        m_parameters->setPLBMode((bool)arg1);
        break;
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    case 1811: /* HAL_PREPARE_PREVIEW */
        CLOGD(" PREPARE PREVIEW (%d/%d)", arg1, arg2);
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_IDLE_TO_STBY);
        break;
    case 1812: /* HAL_STANDBY_PREVIEW */
        CLOGD(" STANDBY PREVIEW (%d/%d)", arg1, arg2);
        m_parameters->setQuickSwitchCmd(QUICK_SWITCH_CMD_MAIN_TO_STBY);
        break;
#endif
        /* For CTS ISSUE-testPreviewPictureSizesCombination */
    case 1814: /*  START PREVIEW NOTIFY */
        CLOGD("START PREVIEW NOTIFY");
        m_disablePreviewCB = false;
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
#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
    case 1841: /* HAL_SET_FOCUS_PEAKING */
        CLOGD("[FocusPeaking] HAL_SET_FOCUS_PEAKING is called!(%d)", arg1);
        if (arg1) {
            m_parameters->m_setFocusPeakingMode(true);
            m_enableDepthMap();
        } else {
            m_parameters->m_setFocusPeakingMode(false);
            m_disableDepthMap();
        }
        break;
#endif
#endif
    default:
        CLOGV("unexpectect command(%d)", command);
        break;
    }

    return NO_ERROR;
}

void ExynosCamera::m_handleFaceDetectionFrame(ExynosCameraFrameSP_sptr_t previewFrame, __unused ExynosCameraBuffer *previewBuffer)
{
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    bool skipFdMetaCallback = false;
    ExynosCameraFrameSP_sptr_t fdFrame = NULL;

    /* Face detection */
    if (!(m_parameters->getFastFpsMode() > 1 && m_getRecordingEnabled() == true)
            && (previewFrame->getFrameState() != FRAME_STATE_SKIPPED)) {
#if defined(SR_CAPTURE)
        if(m_isCopySrMdeta) {
            previewFrame->getDynamicMeta(&m_srShotMeta);
            m_isCopySrMdeta = false;
            CLOGI(" copy SR FdMeta");
        }
#endif

        if (m_parameters->getFrameSkipCount() <= 0
                && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
            previewFrame->getFpsRange(&minFps, &maxFps);

            /* limit maximum FD callback fps to 15fps due to performance issue*/
            if (maxFps >= 60) {
                if (m_getRecordingEnabled() == true) {
                    ratio = (int)((maxFps * 40 / 60) / 10);
                } else {
                    ratio = (int)((maxFps * 10 / 60) / 10);
                }
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
                if (fdFrame == NULL) {
                    CLOGE("[F%d]Failed to createFrame.", previewFrame->getFrameCount());
                    return;
                }

                m_copyMetaFrameToFrame(previewFrame, fdFrame, true, true);
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
    /* to handle the high speed frame rate */
    bool skipPreview = false;
    int ratio = 1;
    uint32_t minFps = 0, maxFps = 0;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    ExynosCameraBuffer resultBuffer;
    int32_t reprocessingBayerMode = m_parameters->getReprocessingBayerMode();
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;

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

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
#endif

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
        ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                     entity->getPipeId(), ret);
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
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         PIPE_3AA, ret);

            m_previewFrameFactory->pushFrameToPipe(frame, PIPE_3AA);
        }

#ifdef DEBUG_RAWDUMP
        if (m_parameters->getUsePureBayerReprocessing() == false) {
            m_previewFrameFactory->stopThread(pipeId);

            if (buffer.index >= 0) {
                CLOGD("[RAWDUMP] BayerDumpEnabled = %d.", m_parameters->checkBayerDumpEnable());
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

                    CLOGD("[RAWDUMP] Sensor (%d x %d) frame(%d)", \
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
#ifndef SUPPORT_FRONT_AF
        if (getCameraIdInternal() == CAMERA_ID_BACK)
#endif
        {
            frameCnt = frame->getFrameCount();
            m_autoFocusContinousQ.pushProcessQ(&frameCnt);
        }

        frame->setMetaDataEnable(true);

        /* Return src buffer to buffer manager */
        if (m_parameters->isFlite3aaOtf() == false) {
            if (m_parameters->isReprocessing() == false) {
                ret = frame->getSrcBuffer(pipeId, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                             pipeId, ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("put Buffer fail");
                    }
                }
            } else {
                // when vc0's NDONE. it will put the buffer.
                if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
                    CLOGW(" pipeId(%d)'s entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR", pipeId);

                    ret = frame->getSrcBuffer(pipeId, &buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                    } else {
                        ExynosCameraBufferManager *bufferMgr = NULL;
                        ret = m_getBufferManager(pipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
                        if (ret != NO_ERROR) {
                            CLOGE("m_getBufferManager(pipeId(%d), SRC_BUFFER_DIRECTION) fail", pipeId);
                        } else {
                            CLOGW("m_putBuffer the bayer buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());

                            ret = m_putBuffers(bufferMgr, buffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                            }
                        }
                    }
                } else {
                    if (m_parameters->getUsePureBayerReprocessing() == true) {
                        /* In case of csis(flite) -> 3AA M2M, it should use pure bayer processed by 3AA for capture */
#ifdef USE_DUAL_CAMERA
                        if (m_parameters->getDualCameraMode() == true) {
                            ret = m_captureSelector->manageSyncFrameHoldList(frame, PIPE_FLITE, false, m_previewFrameFactory->getNodeType(PIPE_VC0));
                        } else
#endif
                        {
                            ret = m_captureSelector->manageFrameHoldList(frame, PIPE_FLITE, false, m_previewFrameFactory->getNodeType(PIPE_VC0));
                        }
                        if (ret != NO_ERROR) {
                            CLOGE("manageFrameHoldList fail");
                        }
                    }
                }
            }
        } else {
            if (m_parameters->isReprocessing() == true) {
#ifdef SAMSUNG_DNG_DIRTY_BAYER
                if (m_parameters->getUseDNGCapture() == true) {
                    if (m_parameters->getDNGCaptureModeOn()) {
                        if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_VC0)) == ENTITY_BUFFER_STATE_COMPLETE
                            && entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)) == ENTITY_BUFFER_STATE_COMPLETE) {
#ifdef USE_DUAL_CAMERA
                            if (m_parameters->getDualCameraMode() == true) {
                                ret = m_captureSelector->manageSyncFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            } else
#endif
                            {
                                ret = m_captureSelector->manageFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            }
                            if (ret != NO_ERROR) {
                                CLOGE("manageFrameHoldList fail");
                                return ret;
                            }
                        } else {

                            CLOGW("Skip manageFrameHoldList(frameCount : %d) VC0(%d), 3AC(%d)", frame->getFrameCount()
                                , entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_VC0))
                                , entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)));

                            ExynosCameraBufferManager *bufferMgr = NULL;
                            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
                            if (ret != NO_ERROR) {
                                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                                return ret;
                            }

                            if (0 <= buffer.index) {
                                ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
                                if (ret != NO_ERROR) {
                                    CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_VC0);
                                } else {
                                    CLOGW("m_putBuffer the flite buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                                    ret = m_putBuffers(bufferMgr, buffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                                    }
                                }
                            }

                            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            if (ret != NO_ERROR) {
                                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                                return ret;
                            }

                            if (0 <= buffer.index) {
                                ret = m_getBufferManager(PIPE_3AC, &bufferMgr, DST_BUFFER_DIRECTION);
                                if (ret != NO_ERROR) {
                                    CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_3AC);
                                } else {
                                    CLOGW("m_putBuffer the bayer buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                                    ret = m_putBuffers(bufferMgr, buffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                                    }
                                }
                            }
                        }
                    } else {  // PRO MODE + DNG Enable + Burst Shot.
                        if (frame->getRequest(PIPE_VC0) == true) {
                            ExynosCameraBufferManager *bufferMgr = NULL;
                            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
                            if (ret != NO_ERROR) {
                                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                                return ret;
                            }

                            if (0 <= buffer.index) {
                                ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
                                if (ret != NO_ERROR) {
                                    CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_VC0);
                                } else {
                                    CLOGW("m_putBuffer the flite buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                                    ret = m_putBuffers(bufferMgr, buffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                                    }
                                }
                            }
                            frame->setRequest(PIPE_VC0, false);
                        }

                        if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)) == ENTITY_BUFFER_STATE_COMPLETE) {
#ifdef USE_DUAL_CAMERA
                            if (m_parameters->getDualCameraMode() == true) {
                                ret = m_captureSelector->manageSyncFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            } else
#endif
                            {
                                ret = m_captureSelector->manageFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            }
                            if (ret != NO_ERROR) {
                                CLOGE("manageFrameHoldList fail");
                                return ret;
                            }
                        } else {
                            ExynosCameraBufferManager *bufferMgr = NULL;
                            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            if (ret != NO_ERROR) {
                                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                                return ret;
                            }

                            if (0 <= buffer.index) {
                                ret = m_getBufferManager(PIPE_3AC, &bufferMgr, DST_BUFFER_DIRECTION);
                                if (ret != NO_ERROR) {
                                    CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_3AC);
                                } else {
                                    CLOGW("m_putBuffer the bayer buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                                    ret = m_putBuffers(bufferMgr, buffer.index);
                                    if (ret != NO_ERROR) {
                                        CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                                    }
                                }
                            }
                        }
                    }
                } else
#endif
                if (frame->getRequest(PIPE_VC0) == true && m_parameters->getUsePureBayerReprocessing() == true) {
                    ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                        return ret;
                    }

                    if (0 <= buffer.index) {
                        if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_VC0)) == ENTITY_BUFFER_STATE_COMPLETE) {
#ifdef USE_DUAL_CAMERA
                            if (m_parameters->getDualCameraMode() == true) {
                                ret = m_captureSelector->manageSyncFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_VC0));
                            } else
#endif
                            {
                                ret = m_captureSelector->manageFrameHoldList(frame, PIPE_3AA, false, m_previewFrameFactory->getNodeType(PIPE_VC0));
                            }
                            if (ret != NO_ERROR) {
                                CLOGE("manageFrameHoldList fail");
                                return ret;
                            }

                            CLOGV("FLITE driver-frameCount(%d) HAL-frameCount(%d)",
                                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                                    frame->getFrameCount());
                        } else {
                            ExynosCameraBufferManager *bufferMgr = NULL;
                            ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
                            if (ret != NO_ERROR) {
                                CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_VC0);
                            } else {
                                CLOGW("m_putBuffer the bayer buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                                ret = m_putBuffers(bufferMgr, buffer.index);
                                if (ret != NO_ERROR) {
                                    CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                                }
                            }

#ifdef SUPPORT_DEPTH_MAP
                            if (frame->getRequest(PIPE_VC1) == true) {
                                ExynosCameraBuffer depth_buffer;
                                ret = frame->getDstBuffer(pipeId, &depth_buffer, m_previewFrameFactory->getNodeType(PIPE_VC1));
                                if (ret != NO_ERROR) {
                                    CLOGE("depth_buffer getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                                } else {
                                    if (depth_buffer.index >= 0) {
                                        CLOGW("m_putBuffer the depth buffer(%d)(frameCount : %d)",
                                            depth_buffer.index, frame->getFrameCount());
                                        ret = m_putBuffers(m_depthMapBufferMgr, depth_buffer.index);
                                        if (ret != NO_ERROR) {
                                            CLOGE("m_putBuffers(m_depthMapBufferMgr, %d) fail", depth_buffer.index);
                                        }
                                    }
                                }
                            }
#endif
                            CLOGW("Skip manageFrameHoldList(frameCount : %d), on pipeId(%d)'s PIPE_VC0", frame->getFrameCount(), pipeId);
                        }
                    } else {
                        CLOGW("FLITE Drop HAL-frameCount(%d)'s buffer.index(%d) < 0",
                                frame->getFrameCount(), buffer.index);
                    }
                } else {
                    int reprocessingBayerMode = m_parameters->getReprocessingBayerMode();

                    switch (reprocessingBayerMode) {
                    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
                    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
                        CLOGV("FLITE Drop HAL-frameCount(%d), mode(%d)", frame->getFrameCount(),
                                reprocessingBayerMode);
                        break;
                    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
                    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
                        CLOGV("FLITE Drop HAL-frameCount(%d)", frame->getFrameCount());
                        break;
                    default:
                        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid reprocessingBayerMode(%d), assert!!!!",
                            __FUNCTION__, __LINE__, reprocessingBayerMode);
                        break;
                    }
                }
            }
        }

        if (0 <= buffer.index) {
            CLOGV("3AA driver-frameCount(%d) HAL-frameCount(%d)",
                    getMetaDmRequestFrameCount((camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
        }

        if (frame->getRequest(PIPE_3AC) == true
#ifdef SAMSUNG_DNG_DIRTY_BAYER
            && m_parameters->getUseDNGCapture() == false
#endif
            ) {
            if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)) != ENTITY_BUFFER_STATE_COMPLETE
#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                    || entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_VC0)) != ENTITY_BUFFER_STATE_COMPLETE
#endif
               ) {
                CLOGW("Skip manageFrameHoldList(frameCount : %d) VC0(%d), 3AC(%d)", frame->getFrameCount()
                        , entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_VC0))
                        , entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)));

                ExynosCameraBufferManager *bufferMgr = NULL;
#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                    return ret;
                }

                if (0 <= buffer.index) {
                    ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
                    if (ret != NO_ERROR) {
                        CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_VC0);
                    } else {
                        CLOGW("m_putBuffer the flite buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                        ret = m_putBuffers(bufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                        }
                    }
                }
#endif
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                    return ret;
                }

                if (0 <= buffer.index) {
                    ret = m_getBufferManager(PIPE_3AC, &bufferMgr, DST_BUFFER_DIRECTION);
                    if (ret != NO_ERROR) {
                        CLOGE("m_getBufferManager(pipeId(%d), DST_BUFFER_DIRECTION) fail", PIPE_3AC);
                    } else {
                        CLOGW("m_putBuffer the bayer buffer(%d)(frameCount : %d)", buffer.index, frame->getFrameCount());
                        ret = m_putBuffers(bufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("m_putBuffers(bufferMgr, %d) fail", buffer.index);
                        }
                    }
                }
            } else {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                            pipeId, ret);
                }

                if (buffer.index >= 0) {
                    if (m_parameters->getHighSpeedRecording() == true) {
                        ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("m_putBuffers(m_bayerBufferMgr, %d) fail", buffer.index);
                            break;
                        }
                    } else {
                        if (entity->getDstBufState(m_previewFrameFactory->getNodeType(PIPE_3AC)) != ENTITY_BUFFER_STATE_ERROR) {
#ifdef USE_DUAL_CAMERA
                            if (m_parameters->getDualCameraMode() == true) {
                                ret = m_captureSelector->manageSyncFrameHoldList(frame, pipeId, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            } else
#endif
                            {
                                ret = m_captureSelector->manageFrameHoldList(frame, pipeId, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                            }
                            if (ret != NO_ERROR) {
                                CLOGE("manageFrameHoldList fail");
                                return ret;
                            }
                        } else {
                            ret = m_putBuffers(m_bayerBufferMgr, buffer.index);
                            if (ret != NO_ERROR) {
                                CLOGE("m_putBuffers(m_bayerBufferMgr, %d) fail", buffer.index);
                                break;
                            }
                        }
                    }
                }
            }
        }
#ifdef USE_DUAL_CAMERA
        else {
            /* in case of internal frame */
            if (m_parameters->getDualCameraMode() == true &&
                    frame->getFrameType() == FRAME_TYPE_INTERNAL) {
                ret = m_captureSelector->manageSyncFrameHoldList(frame, pipeId, false, m_previewFrameFactory->getNodeType(PIPE_3AC));
                if (ret != NO_ERROR) {
                    CLOGE("manageFrameHoldList fail");
                    return ret;
                }
            }
        }
#endif

        memset(m_meta_shot, 0x00, sizeof(struct camera2_shot_ext));

        /* check lux for smooth enterance */
        frame->getDynamicMeta(m_meta_shot);
        frame->getUserDynamicMeta(m_meta_shot);

        m_checkEntranceLux(m_meta_shot);

#ifdef USE_DUAL_CAMERA
        /* no break */
        if (m_parameters->getDualCameraMode() == true &&
                m_parameters->getFlagForceSwitchingOnly() == true) {
            ret = m_prepareBeforeSync(pipeId, PIPE_SYNC_SWITCHING, frame,
                    m_previewFrameFactory->getNodeType(PIPE_3AP),
                    m_previewFrameFactory, m_pipeFrameDoneQ, false, true);
            if (ret != NO_ERROR) {
                CLOGE("m_prepareBeforeSync(pipeId(%d/%d), frameCount(%d), dstPos(%d), isReprocessing(%d) fail",
                        pipeId, PIPE_SYNC_SWITCHING, frame->getFrameCount(),
                        m_previewFrameFactory->getNodeType(PIPE_MCSC0), false);
            }
        }
#endif
        if (m_parameters->is3aaIspOtf() == false)
            break;
#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_SWITCHING:
        if (m_parameters->getDualCameraMode() == true) {
            if (frame->getFrameState() == FRAME_STATE_INVALID ||
                    frame->getFrameState() == FRAME_STATE_SKIPPED) {
                CLOGV("pipeId:%d) frame(%d) skipped(%d)",
                        pipeId, frame->getFrameCount(), frame->getFrameState());
                frame->setFrameState(FRAME_STATE_COMPLETE);
            } else {
                /* if the frame is sync_type_sync, do release slave camera's buffer */
                if (frame->getSyncType() == SYNC_TYPE_SYNC) {
                    ExynosCameraBuffer srcBuf2;
                    ret = frame->getSrcBuffer(PIPE_SYNC_SWITCHING, &srcBuf2, OUTPUT_NODE_2);
                    if (ret != NO_ERROR || srcBuf2.index < 0) {
                        CLOGE("getSrcBuffer(frame(%d, %d), index:%d) fail, ret(%d)",
                                frame->getFrameCount(), frame->getFrameType(),
                                srcBuf2.index, ret);
                    } else {
                        /* put the own buffer to bufMgr */
                        ExynosCameraBufferManager *bufMgr = (ExynosCameraBufferManager *)srcBuf2.manager;
                        ret = m_putBuffers(bufMgr, srcBuf2.index);
                        if (ret < 0)
                            CLOGE("bufferMgr->putBuffers(%d) fail, ret(%d)", srcBuf2.index, ret);
                    }

                    /* forcely change the bypass */
                    frame->setSyncType(SYNC_TYPE_BYPASS);
                }

                /* push isp pipe */
                ret = frame->getDstBuffer(PIPE_3AA, &buffer,
                        m_previewFrameFactory->getNodeType(PIPE_3AP));
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("getDstBuffer(pipeId(%d), frame(%d), buf(%d), node(%d)) failed!! ret(%d)",
                            pipeId, frame->getFrameCount(), buffer.index,
                            m_previewFrameFactory->getNodeType(PIPE_3AP), ret);
                }

                int dstPipeId = PIPE_ISP;
                ret = frame->setSrcBufferState(dstPipeId, ENTITY_BUFFER_STATE_REQUESTED);
                if (ret != NO_ERROR) {
                    CLOGE("pipeId(%d), frame(%d) setDstBufferState(ENTITY_BUFFER_STATE_NOREQ) failed!! ret(%d)",
                            pipeId, frame->getFrameCount(), ret);
                }

                ret = m_setSrcBuffer(dstPipeId, frame, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("m_setDstBuffer(pipeId(%d), frame(%d), buf(%d), node(%d)) failed!! ret(%d)",
                            dstPipeId, frame->getFrameCount(), buffer.index,
                            m_previewFrameFactory->getNodeType(PIPE_3AP), ret);
                }

                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, dstPipeId);
                m_previewFrameFactory->pushFrameToPipe(frame, dstPipeId);
            }
        }
        break;
#endif
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

        if (m_parameters->isIspTpuOtf() == false && m_parameters->isIspMcscOtf() == false)
            break;
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

#ifdef USE_DUAL_CAMERA
        /* no break */
        if (m_parameters->getDualCameraMode() == true) {
            ret = m_prepareBeforeSync(pipeId, PIPE_SYNC, frame,
                    m_previewFrameFactory->getNodeType(PIPE_MCSC0),
                    m_previewFrameFactory, m_pipeFrameDoneQ, false);
            if (ret != NO_ERROR) {
                CLOGE("m_prepareBeforeSync(pipeId(%d/%d), frameCount(%d), dstPos(%d), isReprocessing(%d) fail",
                        pipeId, PIPE_SYNC, frame->getFrameCount(),
                        m_previewFrameFactory->getNodeType(PIPE_MCSC0), false);
            } else {
                /* fd detection for slave camera */
                if (m_cameraId == slaveCameraId) {
                    m_flagHoldFaceDetection = true;
                }
            }
            break;
        }
    case PIPE_SYNC:
        if (m_parameters->getDualCameraMode() == true) {
            ret = m_prepareAfterSync(pipeId, PIPE_FUSION, frame,
                    m_previewFrameFactory->getNodeType(PIPE_MCSC0),
                    m_previewFrameFactory, m_pipeFrameDoneQ, false);
            if (ret != NO_ERROR) {
                CLOGE("m_prepareAfterSync(pipeId(%d/%d), frameCount(%d), isReprocessing(%d) fail",
                        pipeId, PIPE_FUSION, frame->getFrameCount(), false);
            }
            break;
        }
    case PIPE_FUSION:
        if (m_parameters->getDualCameraMode() == true) {
            ret = m_prepareAfterFusion(pipeId, frame,
                    m_previewFrameFactory->getNodeType(PIPE_MCSC0), false);
            if (ret != NO_ERROR) {
                CLOGE("m_prepareAfterFusion(pipeId(%d), frameCount(%d), isReprocessing(%d) fail",
                        pipeId, frame->getFrameCount(), false);
            }
        }
#endif

        ret = getYuvFormatInfo(m_parameters->getHwPreviewFormat(), &bpp, &planeCount);
        if (ret < 0) {
            CLOGE("getYuvFormatInfo(hwPreviewFormat(%x)) fail",
                 m_parameters->getHwPreviewFormat());
            return ret;
        }

        if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("Src buffer state error, pipeId(%d)",
                     pipeId);

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
                    if (ret < 0)
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
                    ret = m_putBuffers(m_previewCallbackBufferMgr, buffer.index);
                    if (ret < 0)
                        CLOGE("Preview callback buffer return fail");
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

#ifdef USE_VRA_GROUP
            /* MCSC DS : PIPE_MCSC_DS */
            if (frame->getRequest(PIPE_MCSC_DS) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC_DS));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_vraBufferMgr, buffer.index);
                    if (ret < 0)
                        CLOGE("Preview callback buffer return fail");
                }
            }

            if (frame->getRequest(PIPE_VRA) == true) {
				frame->setRequest(PIPE_VRA, false);
                ret = frame->setEntityState(PIPE_VRA, ENTITY_STATE_COMPLETE);
                if (ret < 0) {
                    CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                        PIPE_VRA, ENTITY_STATE_COMPLETE, ret);
                }
            }
#endif
            goto entity_state_complete;
        } else {
            int previewPipeId = PIPE_MCSC0;
            int previewCallbackPipeId = PIPE_MCSC1;
            int recordingPipeId = PIPE_MCSC2;
            nsecs_t timeStamp = (nsecs_t)frame->getTimeStamp();
            previewcallbackbuffer.index = -2;

            m_parameters->getFrameSkipCount(&m_frameSkipCount);

            if (m_fdFrameSkipCount > 0) {
                m_fdFrameSkipCount--;
            }

#ifdef USE_VRA_GROUP
#else
            /* fd detection */
            m_handleFaceDetectionFrame(frame, &buffer);
#endif

            /* Preview callback : PIPE_MCSC1 */
            if (frame->getRequest(previewCallbackPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &previewcallbackbuffer,
                                            m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewCallbackPipeId));
                if (bufferState != ENTITY_BUFFER_STATE_COMPLETE) {
                    CLOGD("Preview callback handling droped \
                            - preview callback buffer is not ready HAL-frameCount(%d)",
                             frame->getFrameCount());

                    if (previewcallbackbuffer.index >= 0) {
                        ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                        if (ret != NO_ERROR) {
                            CLOGE("Recording buffer return fail");
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
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(previewPipeId));
                if (bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                    /* drop preview frame if lcd supported frame rate < scp frame rate */
                    frame->getFpsRange(&minFps, &maxFps);
                    if (dispFps < maxFps) {
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
#ifdef SAMSUNG_DUAL_SOLUTION
                    if (m_parameters->getDualCameraMode() == true) {
                        UniPlugin_DCVOZ_CAMERAPARAM_t *fusionParam = m_fusionPreviewWrapper->m_getCameraParam();
                        int fusionIndex = fusionParam->CameraIndex;
                        int outNode;
                        bool flagValidFrame = true;

                        /* update current Fusion Index */
                        m_parameters->setMasterCamera(fusionIndex);

                        /* update Fusion param for every frame */
                        m_parameters->setFusionParam(fusionParam);

                        /* --FORCEWIDE-- : update tele meta for force wide */
                        if (frame->getSyncType() == SYNC_TYPE_SYNC)
                            outNode = OUTPUT_NODE_2;
                        else
                            outNode = OUTPUT_NODE_1;

                        frame->getDynamicMeta(m_meta_shot, outNode);
                        frame->getUserDynamicMeta(m_meta_shot, outNode);

                        if (frame->getFrameState() == FRAME_STATE_INVALID ||
                                frame->getFrameState() == FRAME_STATE_SKIPPED ||
                                frame->getFrameType() == FRAME_TYPE_INTERNAL) {
                            flagValidFrame = false;
                        }

                        if (frame->getSyncType() == SYNC_TYPE_SWITCH ||
                            (frame->getSyncType() == SYNC_TYPE_SYNC && m_parameters->getForceWide())) {
                            /* update force wide condition only for tele cam */
                            m_parameters->setForceWideCond(flagValidFrame, m_meta_shot);
                        }
                        /* --FORCEWIDE-- */

                        /* Update the fusion capture condition */
                        if (frame->getSyncType() == SYNC_TYPE_SYNC) {
                            /* Get wide camera meta */
                            memset(m_tempshot, 0x00, sizeof(struct camera2_shot_ext));
                            frame->getUserDynamicMeta(m_tempshot, OUTPUT_NODE_1);
                            m_parameters->checkFusionCaptureMode(getCameraIdInternal(), m_tempshot, m_meta_shot);
                        }

                        if (frame->getSyncType() != SYNC_TYPE_SYNC)
                            outNode = OUTPUT_NODE_1;
                        else {
                            /* Decide meta according to displayed preview */
                            if (fusionIndex == UNI_PLUGIN_CAMERA_TYPE_WIDE)
                                outNode = OUTPUT_NODE_1;
                            else
                                outNode = OUTPUT_NODE_2;

                            frame->getDynamicMeta(m_meta_shot, outNode);
                            frame->getUserDynamicMeta(m_meta_shot, outNode);
                        }
                    } else
#endif
                    {
                        /* update current Fusion Index */
                        m_parameters->setMasterCamera(m_cameraId);

                        /* check lux for smooth enterance */
                        frame->getDynamicMeta(m_meta_shot);
                        frame->getUserDynamicMeta(m_meta_shot);

                        m_checkEntranceLux(m_meta_shot);
                    }
#ifdef SAMSUNG_LBP
                    if (m_parameters->getUsePureBayerReprocessing() == true) {
                        camera2_shot_ext *shot_ext = NULL;
                        shot_ext = buffer.addr[buffer.getMetaPlaneIndex()];

                        if (shot_ext != NULL)
                            LBPframeCount = shot_ext->shot.dm.request.frameCount;
                        else
                            CLOGE("Buffer is null");
                    } else {
                        camera2_stream *lbp_shot_stream = NULL;
                        lbp_shot_stream = (struct camera2_stream *)buffer.addr[buffer.getMetaPlaneIndex()];
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
                            CLOGE("[JQ] m_processJpegQtable() failed!!");
                        }
                    }
#endif

#ifdef DEBUG_IQ_OSD
                    printOSD(buffer.addr[0], buffer.addr[1], m_meta_shot);
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
                                CLOGE("[OBTR]:Object Tracking plugin set focus info failed!!");
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
                        int PreviewW = 0, PreviewH = 0;
                        m_parameters->getPreviewSize(&PreviewW, &PreviewH);

                        UniPluginBufferData_t bufferData;
                        memset(&bufferData, 0, sizeof(UniPluginBufferData_t));

                        bufferData.InBuffY = buffer.addr[0];
                        bufferData.InBuffU = buffer.addr[1];

                        bufferData.InWidth = PreviewW;
                        bufferData.InHeight = PreviewH;
                        bufferData.InFormat = UNI_PLUGIN_FORMAT_NV21;
                        ret = uni_plugin_set(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
                        if(ret < 0) {
                            CLOGE("[OBTR]:Object Tracking plugin set buffer info failed!!");
                        }

                        ret = uni_plugin_process(m_OTpluginHandle);
                        if(ret < 0) {
                            CLOGE("[OBTR]:Object Tracking plugin process failed!!");
                        }

                        uni_plugin_get(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_INFO, &m_OTfocusData);
                        CLOGV("[OBTR]Focus state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                                 m_OTfocusData.FocusState,
                                m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
                                m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
                        CLOGV("[OBTR]Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                                 m_OTfocusData.W_Movement, m_OTfocusData.H_Movement,
                                m_OTfocusData.W_Velocity, m_OTfocusData.H_Velocity);

                        if (m_parameters->getObjectTrackingEnable() == true && m_OTstart == OBJECT_TRACKING_INIT) {
                            m_parameters->setObjectTrackingGet(true);
                        }

                        m_OTstatus = m_OTfocusData.FocusState;

                        uni_plugin_get(m_OTpluginHandle,
                                OBJECT_TRACKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_FOCUS_PREDICTED, &m_OTpredictedData);
                        CLOGV("[OBTR]Predicted state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
                                 m_OTpredictedData.FocusState,
                                m_OTpredictedData.FocusROILeft, m_OTpredictedData.FocusROIRight,
                                m_OTpredictedData.FocusROITop, m_OTpredictedData.FocusROIBottom, m_OTpredictedData.FocusWeight);
                        CLOGV("[OBTR]Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
                                 m_OTpredictedData.W_Movement, m_OTpredictedData.H_Movement,
                                m_OTpredictedData.W_Velocity, m_OTpredictedData.H_Velocity);

                        ExynosRect cropRegionRect;
                        ExynosRect2 oldrect, newRect;
                        oldrect.x1 = m_OTpredictedData.FocusROILeft;
                        oldrect.x2 = m_OTpredictedData.FocusROIRight;
                        oldrect.y1 = m_OTpredictedData.FocusROITop;
                        oldrect.y2 = m_OTpredictedData.FocusROIBottom;

#ifdef SAMSUNG_DUAL_SOLUTION
                        if (m_parameters->getDualCameraMode() == true) {
                            UniPlugin_DCVOZ_CAMERAPARAM_t *fusionParam;
                            fusionParam = m_parameters->getFusionParam();
                            if (fusionParam) {
                                oldrect.x1 -= fusionParam->PreviewImageShiftX;
                                oldrect.y1 -= fusionParam->PreviewImageShiftY;
                                oldrect.x2 -= fusionParam->PreviewImageShiftX;
                                oldrect.y2 -= fusionParam->PreviewImageShiftY;
                            }
                        }
#endif

                        m_parameters->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
                        newRect = convertingAndroidArea2HWAreaBcropOut(&oldrect, &cropRegionRect);
                        m_OTpredictedData.FocusROILeft = newRect.x1;
                        m_OTpredictedData.FocusROIRight = newRect.x2;
                        m_OTpredictedData.FocusROITop = newRect.y1;
                        m_OTpredictedData.FocusROIBottom = newRect.y2;

                        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
                        ret = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
#ifdef SAMSUNG_DUAL_SOLUTION
                        if (m_parameters->getDualCameraMode() == true &&
                            (frame->getSyncType() == SYNC_TYPE_SYNC || frame->getSyncType() == SYNC_TYPE_SWITCH)) {
                            m_parameters->dualNotify(DUAL_CAMERA_NOTIFY_SET_OBJECT_TRACKING_FOCUS_AREA,
                                                    false, slaveCameraId, 0, (void*)&m_OTpredictedData);
                        }
#endif
                        if (ret == false) {
                            CLOGE("[OBTR] setObjectTrackingAreas failed!!");
                        }
                    }
                    if (m_OTpluginHandle != NULL && m_OTstart == OBJECT_TRACKING_DEINIT) {
                        int ret = uni_plugin_deinit(m_OTpluginHandle);
                        if (ret < 0) {
                            CLOGE("[OBTR] Object Tracking plugin deinit failed!!");
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
                        CLOGD("[LBP]:Start BestPic(%d/%d)", LBPframeCount, m_LBPindex);
                        char *srcAddr = NULL;
                        char *dstAddr = NULL;
                        int hwPreviewFormat = m_parameters->getHwPreviewFormat();
                        int planeCount = getYuvPlaneCount(hwPreviewFormat);
                        if (planeCount <= 0) {
                            CLOGE("[LBP]:getYuvPlaneCount(%d) fail", hwPreviewFormat);
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
                            CLOGD("HLV : uni_plugin_deinit !!!");
                            ret = uni_plugin_deinit(m_HLV);
                            if (ret)
                                CLOGE("HLV : uni_plugin_deinit failed !! Possible Memory Leak !!!");

                            ret = uni_plugin_unload(&m_HLV);
                            if (ret)
                                CLOGE("HLV : uni_plugin_unload failed !! Possible Memory Leak !!!");

                            m_HLV = NULL;
                            m_HLVprocessStep = HLV_PROCESS_DONE;
                        }
                    }
#endif
#ifdef SAMSUNG_HYPER_MOTION
                    if (m_hyperMotionModeGet() == true && m_hyperMotionStep == HYPER_MOTION_STOP) {
                        CLOGD("HYPERMOTION : uni_plugin_deinit !!!");
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
                            CLOGE(" meta_shot_ext is null");
                        }

                        swVDIS_BufIndex = -1;
                        int swVDIS_IndexCount = m_swVDIS_NextOutBufIdx;
                        ret = m_swVDIS_BufferMgr->getBuffer(&swVDIS_BufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
                                &m_swVDIS_OutBuf[swVDIS_IndexCount]);

                        if (ret < 0) {
                            CLOGE("m_swVDIS_BufferMgr->getBuffer fail, ret(%d)", ret);
                        } else {
                            m_swVDIS_OutBufIndex[swVDIS_BufIndex] = swVDIS_IndexCount;
                        }

                        nsecs_t swVDIS_timeStamp = (nsecs_t)frame->getTimeStamp();
                        nsecs_t swVDIS_timeStampBoot = (nsecs_t)frame->getTimeStampBoot();
                        int swVDIS_Lux = m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256;
                        int swVDIS_ZoomLevel = m_parameters->getZoomLevel();
                        float swVDIS_ExposureValue = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[4] / 256.0;
                        int swVDIS_ExposureTime = (int32_t)m_meta_shot->shot.dm.sensor.exposureTime;

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

#ifdef SAMSUNG_OIS_VDIS
                        m_parameters->setSWVdisPreviewFrameExposureTime(swVDIS_ExposureTime);
                        uint32_t oisGain = m_parameters->getOISGain();
                        m_parameters->setOISCoef(oisGain);
#endif
                        m_swVDIS_process(&buffer, swVDIS_BufIndex, swVDIS_timeStamp, swVDIS_timeStampBoot,
                                    swVDIS_Lux, swVDIS_ZoomLevel, swVDIS_ExposureValue, oisGain, m_swVDIS_FaceData, swVDIS_ExposureTime);
                        m_swVDIS_GetPreviewVDISInfo();
                    }
#ifdef SAMSUNG_OIS_VDIS
                    else {
                        int swVDIS_ExposureTime = (int32_t)m_meta_shot->shot.dm.sensor.exposureTime;
                        m_parameters->setSWVdisPreviewFrameExposureTime(swVDIS_ExposureTime);
                    }
#endif
#endif

#ifdef SAMSUNG_HYPER_MOTION
                    if (m_hyperMotionModeGet() == true) {
                        hyperMotion_BufIndex = -1;
                        int hyperMotion_IndexCount = (m_hyperMotion_FrameNum % NUM_HYPERMOTION_BUFFERS);
                        if (m_hyperMotion_BufferMgr!= NULL) {
                            ret = m_hyperMotion_BufferMgr->getBuffer(&hyperMotion_BufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL,
                                                                    &m_hyperMotion_OutBuf[hyperMotion_IndexCount]);
                        }

                        if (ret < 0) {
                            CLOGE("m_hyperMotion_BufferMgr->getBuffer fail, ret(%d)", ret);
                        } else {
                            nsecs_t hyperMotion_timeStamp = (nsecs_t)frame->getTimeStamp();
                            nsecs_t hyperMotion_timeStampBoot = (nsecs_t)frame->getTimeStampBoot();
                            int hyperMotion_Lux = m_meta_shot->shot.udm.ae.vendorSpecific[5] / 256;
                            int hyperMotion_ZoomLevel = m_parameters->getZoomLevel();
                            float hyperMotion_ExposureValue = (int32_t)m_meta_shot->shot.udm.ae.vendorSpecific[4] / 256.0;
                            int hyperMotion_ExposureTime = (int32_t)m_meta_shot->shot.dm.sensor.exposureTime;
                            int hyperMotion_Ori = m_parameters->getDeviceOrientation();
                            int hm_faceCnt = 0;

                            memset(m_swHM_fd_dm, 0x00, sizeof(struct camera2_dm));
                            if (frame->getDynamicMeta(m_swHM_fd_dm) != NO_ERROR) {
                                CLOGE("ERR(%s[%d]) meta_shot_ext is null", __FUNCTION__, __LINE__);
                            }
                            memset(m_swHM_FaceData, 0x00, sizeof(swHM_FaceData_t));

                            for (int i = 0; i < 10; i++) {//face Detection is able to 10.
                                if (m_swHM_fd_dm->stats.faceIds[i]) {
                                    m_swHM_FaceData->FaceRect[i].left = m_swHM_fd_dm->stats.faceRectangles[i][0];
                                    m_swHM_FaceData->FaceRect[i].top = m_swHM_fd_dm->stats.faceRectangles[i][1];
                                    m_swHM_FaceData->FaceRect[i].right = m_swHM_fd_dm->stats.faceRectangles[i][2];
                                    m_swHM_FaceData->FaceRect[i].bottom = m_swHM_fd_dm->stats.faceRectangles[i][3];
                                    m_swHM_FaceData->FaceNum++;
                                    hm_faceCnt++;
                                }
                            }

                            m_hyperMotion_process(&buffer, hyperMotion_IndexCount, hyperMotion_timeStamp, hyperMotion_timeStampBoot,
                                        hyperMotion_Lux, hyperMotion_ZoomLevel, hyperMotion_ExposureValue, hyperMotion_ExposureTime,
                                        m_swHM_FaceData, m_parameters->getDeviceOrientation());
                        }
                    }
#endif

#ifdef SAMSUNG_STR_PREVIEW
                    if (m_parameters->checkSTRPreviewMode()) {
                        ret = m_processSTRPreview(buffer.addr[0], buffer.addr[1], m_meta_shot);
                        if (ret == NO_ERROR) {
                            int hwPreviewFormat = m_parameters->getHwPreviewFormat();
                            int planeCount = getYuvPlaneCount(hwPreviewFormat);
                            /* TODO : have to consider all fmt(planes) and stride */
                            for (int plane = 0; plane < planeCount; plane++) {
                                if (m_ionClient >= 0)
#ifdef USE_LIB_ION_LEGACY
                                    ion_sync_fd(m_ionClient, buffer.fd[plane]);
#else
                                    exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
#endif
                            }
                        }
                    }
#endif

#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
                    if (m_parameters->m_getFocusPeakingMode()
                        && m_parameters->getFocusMode() == FOCUS_MODE_MANUAL) {
                        m_processFocusPeaking(buffer.addr[0], buffer.addr[1], frame);
                    }
#endif
#endif

#ifdef SAMSUNG_IDDQD
                    if (m_parameters->checkIDDQDMode()) {
                        m_processIDDQD(buffer.addr[0], buffer.addr[1], buffer.size[0], m_meta_shot);
                    }
#endif

                    if ((m_frameSkipCount > 0) || (skipPreview == true)) {
                        if (skipPreview != true) {
                            CLOGD("Skip frame for frameSkipCount(%d) buffer.index(%d)",
                                     m_frameSkipCount, buffer.index);
                        }
#ifdef SUPPORT_SW_VDIS
                        if (m_swVDIS_Mode) {
                            int VDISOutBufIndex = m_swVDIS_GetVDISOutInfo(&timeStamp);
                            if (VDISOutBufIndex >= 0) {
                                ret = m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                if (ret != NO_ERROR) {
                                    CLOGE("ERR(%s[%d]):[F%d B%d]Failed to return SW_VDIS buffer. ret %d",
                                                                     __FUNCTION__, __LINE__,
                                                                      frame->getFrameCount(),
                                                                      VDISOutBufIndex,
                                                                      ret);
                                }
                            }
                            if (m_frameSkipCount == 1) {
                                for (int i = 0; i < m_sw_VDIS_OutBufferNum; i++) {
                                    ret = m_swVDIS_BufferMgr->cancelBuffer(i);
                                    if (ret < 0) {
                                        CLOGE("swVDIS buffer return fail");
                                    }
                                }
                            }
                        }
#endif

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
                    } else {
                        if (m_skipReprocessing == true)
                            m_skipReprocessing = false;

                        ExynosCameraFrameSP_sptr_t previewFrame = NULL;
                        previewFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(previewPipeId, frame->getFrameCount());
                        if (previewFrame == NULL) {
                            CLOGE("[F%d]Failed to createFrame previewFrame", frame->getFrameCount());
                            if (buffer.index >= 0) {
                                ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                if (ret != NO_ERROR)
                                    CLOGE("Preview buffer return fail");
                            }
                            goto entity_state_complete;
                        }
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

#ifdef TIME_LOGGER_PREVIEW_ENABLE
                        TIME_LOGGER_UPDATE(m_cameraId, previewFrame->getFrameCount(), PIPE_MCSC0, INTERVAL, PREVIEW_TRIGGER, true);
#endif
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

#ifdef TIME_LOGGER_PREVIEW_ENABLE
                                TIME_LOGGER_UPDATE(m_cameraId, previewFrame->getFrameCount(), PIPE_MCSC0, CUMULATIVE_CNT, PREVIEW_TRIGGER, true);
#endif
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
                        } else if((m_parameters->getFastFpsMode() > 1) && (m_parameters->getRecordingHint() == 1)) {
                                CLOGV("push frame to previewQ");
                                m_previewQ->pushProcessQ(&previewFrame);
                        } else {
#ifdef SUPPORT_SW_VDIS
                            if (m_swVDIS_Mode == true) {
                                if (m_parameters->isSWVdisOnPreview() == true) {
                                    int sizeOfPreviewDelayQ = m_previewDelayQ->getSizeOfProcessQ();
                                    if (sizeOfPreviewDelayQ >= NUM_VDIS_PREVIEW_DELAY) {
                                        m_previewDelayQ->pushProcessQ(&previewFrame);
                                        m_previewDelayQ->popProcessQ(&previewFrame);
#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
                                        previewcallbackbuffer.index = -2;

                                        if (previewFrame->getReqeust(previewCallbackId) == true) {
                                            ret = frame->getSrcBuffer(previewPipeId, &previewcallbackbuffer);
                                            if (ret < 0) {
                                                CLOGE(" : getSrcBuffer fail, ret(%d)", ret);
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
                                    CLOGV("push frame to previewQ");
                                    m_previewQ->pushProcessQ(&previewFrame);
                                    m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                    m_preLongExposureTime = 0;
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
#ifdef OIS_CAPTURE
                                    ExynosCameraActivitySpecialCapture *m_sCaptureMgr = NULL;
                                    unsigned int OISFcount = 0;
                                    unsigned int fliteFcount = 0;
                                    bool OISCapture_activated = false;

                                    m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
                                    OISFcount = m_sCaptureMgr->getOISCaptureFcount();

                                    if (m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        CLOGD("manual exposure capture mode end. (%lld), fcount(%d)",
                                             (long long)m_meta_shot->shot.dm.sensor.exposureTime,
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
                                        CLOGD("OIS capture mode. Skip frame. FliteFrameCount(%d) ",fliteFcount);

                                        if (buffer.index >= 0) {
                                            /* only apply in the Full OTF of Exynos74xx. */
                                            ret = m_scpBufferMgr->cancelBuffer(buffer.index, true);
                                            if (ret < 0)
                                                CLOGE("SCP buffer return fail");
                                        }

                                        if (previewcallbackbuffer.index >= 0) {
                                            ret = m_putBuffers(m_previewCallbackBufferMgr, previewcallbackbuffer.index);
                                            if (ret != NO_ERROR) {
                                                CLOGE("Preview callback buffer return fail");
                                            }
                                        }
                                        previewFrame = NULL;
                                    } else
#endif
                                    {
                                        CLOGV("push frame to previewQ");
                                        m_previewQ->pushProcessQ(&previewFrame);
                                    }

                                    if (m_stopLongExposure && m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        m_longExposureCaptureState = LONG_EXPOSURE_CANCEL_NOTI;
                                        m_preLongExposureTime = 0;
                                    } else if (m_longExposureCaptureState == LONG_EXPOSURE_CAPTURING) {
                                        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                        m_preLongExposureTime = 0;
                                    } else {
                                        m_longExposureCaptureState = LONG_EXPOSURE_PREVIEW;
                                    }
                                }
                            }

                            /* Recording : case of adaptive CSC recording */
#ifdef SUPPORT_SW_VDIS
                            if (m_swVDIS_Mode == true) {
                                nsecs_t timeStamp;
                                int VDISOutBufIndex = m_swVDIS_GetVDISOutInfo(&timeStamp);
                                int VDIS_OutVDISQIndex = -1;
                                if (VDISOutBufIndex >= 0) {
                                    if (m_getRecordingEnabled() == true
                                        && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
                                        && m_swVDIS_FrameNum > m_swVDIS_Delay) {
                                        if (timeStamp <= 0L) {
                                            CLOGE("timeStamp(%lld) Skip", (long long)timeStamp);
                                            if (VDISOutBufIndex >= 0) {
                                                m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                            }
                                        } else {
                                            if (m_parameters->doCscRecording() == true) {
                                                /* FHD to HD down scale */
                                                int bufIndex = -2;
                                                VDIS_OutVDISQIndex = m_swVDIS_OutBufIndex[VDISOutBufIndex];
                                                ExynosCameraBuffer recordingBuffer;
                                                ret = m_recordingBufferMgr->getBuffer(&bufIndex,
                                                        EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &recordingBuffer);
                                                if (ret < 0 || bufIndex < 0) {
                                                    CLOGE(" Recording Frames are Skipping(%d frames) (bufIndex=%d)",
                                                         m_recordingFrameSkipCount, bufIndex);
                                                    m_recordingBufferMgr->printBufferQState();
                                                    if (VDISOutBufIndex >= 0) {
                                                        m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                                    }
                                                } else {
                                                    if (m_recordingFrameSkipCount != 0) {
                                                        CLOGE(" Recording Frames are Skipped(%d frames) (bufIndex=%d) (recordingQ=%d)",
                                                             m_recordingFrameSkipCount, bufIndex,
                                                            m_recordingQ->getSizeOfProcessQ());
                                                        m_recordingFrameSkipCount = 0;
                                                        m_recordingBufferMgr->printBufferQState();
                                                    }

                                                    ret = m_doPreviewToRecordingFunc(PIPE_GSC_VIDEO, m_swVDIS_OutBuf[VDIS_OutVDISQIndex], recordingBuffer, timeStamp);
                                                    if (ret < 0 ) {
                                                        CLOGW("recordingCallback Skip");
                                                    }

                                                    if (VDISOutBufIndex >= 0) {
                                                        m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                                    }
                                                    m_recordingTimeStamp[recordingBuffer.index] = timeStamp;
                                                }
                                            } else {
                                                ExynosCameraFrameSP_sptr_t adaptiveCscRecordingFrame = NULL;
                                                adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());
                                                if (adaptiveCscRecordingFrame != NULL) {
                                                    VDIS_OutVDISQIndex = m_swVDIS_OutBufIndex[VDISOutBufIndex];

                                                    m_recordingTimeStamp[m_swVDIS_OutBuf[VDIS_OutVDISQIndex].index] = timeStamp;
                                                    adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, m_swVDIS_OutBuf[VDIS_OutVDISQIndex]);

                                                    ret = m_insertFrameToList(&m_recordingProcessList, adaptiveCscRecordingFrame, &m_recordingProcessListLock);
                                                    if (ret != NO_ERROR) {
                                                        CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                                                adaptiveCscRecordingFrame->getFrameCount());
                                                    }

                                                    m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                                } else {
                                                    CLOGW("WRR(%s[%d]):[F%d]Failed to createNewFrame for adaptiveCscRecording. Skip.",
                                                            __FUNCTION__, __LINE__,
                                                            frame->getFrameCount());

                                                    if (VDISOutBufIndex >= 0) {
                                                        ret = m_putBuffers(m_swVDIS_BufferMgr, VDISOutBufIndex);
                                                        if (ret != NO_ERROR) {
                                                            CLOGE("ERR(%s[%d]):[F%d B%d]Failed to return SW_VDIS buffer. ret %d",
                                                                    __FUNCTION__, __LINE__,
                                                                    frame->getFrameCount(),
                                                                    VDISOutBufIndex,
                                                                    ret);
                                                        }
                                                    }
                                                }
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
                                            CLOGE("timeStamp(%lld) Skip", (long long)timeStamp);
                                            if (hyperMotionOutBufIndex >= 0) {
                                                m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                            }
                                        } else {
                                            if (m_hyperMotionCanSendFrame() == true) {
                                                ExynosCameraFrameSP_sptr_t adaptiveCscRecordingFrame = NULL;
                                                adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());
                                                if (adaptiveCscRecordingFrame != NULL) {
                                                    m_recordingTimeStamp[hyperMotionOutBufIndex] = timeStamp;
                                                    adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, m_hyperMotion_OutBuf[m_hyperMotion_OutQIndex]);

                                                    ret = m_insertFrameToList(&m_recordingProcessList, adaptiveCscRecordingFrame, &m_recordingProcessListLock);
                                                    if (ret != NO_ERROR) {
                                                        CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                                                adaptiveCscRecordingFrame->getFrameCount());
                                                    }

                                                    m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                                } else {
                                                    CLOGW("WARN(%s[%d]):[F%d]Failed to createNewFrame for adaptiveCscRecording. Skip.",
                                                            __FUNCTION__, __LINE__,
                                                            frame->getFrameCount());

                                                    if (hyperMotionOutBufIndex >= 0) {
                                                        ret = m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                                        if (ret != NO_ERROR) {
                                                            CLOGE("ERR(%s[%d]):[F%d B%d]Failed to return hyperMotionOutBuffer. ret %d",
                                                                    __FUNCTION__, __LINE__,
                                                                    frame->getFrameCount(),
                                                                    hyperMotionOutBufIndex,
                                                                    ret);
                                                        }
                                                    }
                                                }
                                            } else {
                                                ret = m_putBuffers(m_hyperMotion_BufferMgr, hyperMotionOutBufIndex);
                                                if (ret < 0) {
                                                    CLOGE("put Buffer fail");
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
                                        CLOGW("timeStamp(%lld) Skip", (long long)timeStamp);
                                    } else {
                                        ExynosCameraFrameSP_sptr_t adaptiveCscRecordingFrame = NULL;

                                        adaptiveCscRecordingFrame = m_previewFrameFactory->createNewFrameOnlyOnePipe(recordingPipeId, frame->getFrameCount());
                                        if (adaptiveCscRecordingFrame != NULL) {
                                            m_recordingTimeStamp[buffer.index] = timeStamp;
                                            adaptiveCscRecordingFrame->setDstBuffer(recordingPipeId, buffer);

                                            ret = m_insertFrameToList(&m_recordingProcessList, adaptiveCscRecordingFrame, &m_recordingProcessListLock);
                                            if (ret != NO_ERROR) {
                                                CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                                        adaptiveCscRecordingFrame->getFrameCount());
                                            }

                                            m_recordingQ->pushProcessQ(&adaptiveCscRecordingFrame);
                                        } else {
                                            CLOGW("WARN(%s[%d]):[F%d]Failed to createNewFrame for adaptiveCscRecording. Skip",
                                                    __FUNCTION__, __LINE__,
                                                    frame->getFrameCount());
                                        }
                                    }
                                }
#ifdef USE_DUAL_CAMERA
                                if (m_getRecordingEnabled() == true &&
                                        m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME) &&
                                        m_parameters->getDualCameraMode() == true &&
                                        m_parameters->doCscRecording() == true) {

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
#endif
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

            /* Recording : PIPE_MCSC2 */
            if (frame->getRequest(recordingPipeId) == true) {
                ret = frame->getDstBuffer(pipeId, &buffer, m_previewFrameFactory->getNodeType(recordingPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                bufferState = entity->getDstBufState(m_previewFrameFactory->getNodeType(recordingPipeId));
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
                                if (recordingFrame != NULL) {
                                    m_recordingTimeStamp[buffer.index] = timeStamp;
                                    recordingFrame->setDstBuffer(recordingPipeId, buffer);

                                    ret = m_insertFrameToList(&m_recordingProcessList, recordingFrame, &m_recordingProcessListLock);
                                    if (ret != NO_ERROR) {
                                        CLOGE("[F%d]Failed to insert frame into m_recordingProcessList",
                                                recordingFrame->getFrameCount());
                                    }

                                    m_recordingQ->pushProcessQ(&recordingFrame);
                                } else {
                                    CLOGW("WARN(%s[%d]):[F%d]Failed to createNewFrame for recording. Skip recording.",
                                            __FUNCTION__, __LINE__,
                                            frame->getFrameCount());

                                    if (buffer.index >= 0) {
                                        ret = m_putBuffers(m_recordingBufferMgr, buffer.index);
                                        if (ret != NO_ERROR) {
                                            CLOGE("ERR(%s[%d]):[F%d B%d]Recording buffer return fail",
                                                    __FUNCTION__, __LINE__,
                                                    frame->getFrameCount(), buffer.index);
                                        }
                                    }
                                }
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
                    CLOGD("Recording handling droped - Recording buffer is not ready HAL-frameCount(%d)", frame->getFrameCount());

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

#ifdef USE_VRA_GROUP
            if (m_parameters->isMcscVraOtf() == false) {
                if (frame->getRequest(PIPE_MCSC_DS) == true) {
                    /* Send the Yuv buffer to VRA Pipe */
                    ret = frame->getDstBuffer(entity->getPipeId(), &buffer, m_previewFrameFactory->getNodeType(PIPE_MCSC_DS));
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, framecount(%d) pipeId(%d), ret(%d)",
                                frame->getFrameCount(), entity->getPipeId(), ret);
                    }

                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), entity->getPipeId());
                    }

                    ret = m_setupEntity(PIPE_VRA, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupEntity failed, framecount(%d) pipeId(%d), ret(%d)", frame->getFrameCount(), PIPE_VRA, ret);
                    }
                    m_previewFrameFactory->pushFrameToPipe(frame, PIPE_VRA);
                } else {
                    if (frame->getRequest(PIPE_VRA) == true) {
                        frame->setRequest(PIPE_VRA, false);
                        ret = frame->setEntityState(PIPE_VRA, ENTITY_STATE_COMPLETE);
                        if (ret < 0) {
                            CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                                PIPE_VRA, ENTITY_STATE_COMPLETE, ret);
                        }
                    }
                }
            }
        }
        break;
    case PIPE_VRA:
        if (entity->getPipeId() == PIPE_VRA) {
            if (frame->getRequest(PIPE_VRA)) {
                ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                            entity->getPipeId(), ret);
                }

                ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
                if (ret < 0) {
                    CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                    return ret;
                }

                if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                    CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                }

                if (buffer.index >= 0) {
                    ret = m_putBuffers(m_vraBufferMgr, buffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Recording buffer return fail");
                    }
                }
            }
            m_handleFaceDetectionFrame(frame, NULL);
#endif
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
#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;
#endif
    ExynosCameraBuffer scpBuffer;
    ExynosCameraBuffer previewCbBuffer;

    ExynosCameraFrameSP_sptr_t frame = NULL;
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
#ifdef SAMSUNG_ROTATION
    static uint8_t rotation_error_cnt = 0;
#endif
#endif

#ifdef USE_CAMERA_PREVIEW_FRAME_SCHEDULER
    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);

    /* Check the Slow/Fast Motion Scenario - sensor : 120fps, preview : 60fps */
    if(((curMinFps == 120) && (curMaxFps == 120))
         || ((curMinFps == 240) && (curMaxFps == 240))) {
        CLOGV(" Change PreviewDuration from (%d,%d) to (60000, 60000)", curMinFps, curMaxFps);
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
        scpBuffer.index = -2;
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
        scpBuffer.index = -2;
        goto func_exit;
    }

    CLOGV("m_previewQ->getSizeOfProcessQ(%d) m_scpBufferMgr->getNumOfAvailableBuffer(%d)",
        previewQ->getSizeOfProcessQ(), m_scpBufferMgr->getNumOfAvailableBuffer());

    /* Prevent displaying unprocessed beauty images in beauty shot. */
    if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) || (m_parameters->getShotMode() == SHOT_MODE_KIDS)
#ifdef LLS_CAPTURE
        || (m_parameters->getLLSOn() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
#endif
        ) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
            CLOGV("skip the preview callback and the preview display while compressed callback.");
            goto func_exit;
        }
    }

    if ((m_previewWindow == NULL) ||
        (m_getRecordingEnabled() == true) ||
        (m_parameters->getPreviewBufferCount() == NUM_PREVIEW_BUFFERS + NUM_PREVIEW_SPARE_BUFFERS &&
        m_scpBufferMgr->getNumOfAvailableAndNoneBuffer() > 4 &&
        previewQ->getSizeOfProcessQ() < 2) ||
#ifdef SUPPORT_SW_VDIS
        ((m_parameters->getPreviewBufferCount() == NUM_VDIS_BUFFERS ||
        m_parameters->getPreviewBufferCount() == NUM_VDIS_UHD_BUFFERS) &&
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

        if ((m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
            m_highResolutionCallbackRunning == false &&
            m_disablePreviewCB == false &&
            !m_previewCallbackToggle)
#ifdef SAMSUNG_COLOR_IRIS
            || ((getCameraIdInternal() == CAMERA_ID_FRONT) &&
            m_parameters->msgTypeEnabled(CAMERA_MSG_IRIS_PREVIEW_DATA))
#endif
            ) {
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
            if (previewCbBuffer.index < 0)
#endif
            {
                int bufIndex = -2;
                m_previewCallbackBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &previewCbBuffer);
                if (bufIndex < 0) {
                    CLOGE("getBuffer fail");
                    goto func_exit;
                }
                ret = m_doPreviewToCallbackFunc(pipeIdCsc, scpBuffer, previewCbBuffer);
                if (ret < 0) {
                    CLOGE("m_doPreviewToCallbackFunc fail");
                    goto func_exit;
                }

                ret = frame->setSrcBuffer(pipeId, previewCbBuffer);
                if (ret < 0) {
                    CLOGE("setSrcBuffer fail, ret(%d)", ret);
                    goto func_exit;
                }
            }

            if (m_parameters->getCallbackNeedCopy2Rendering() == true) {
                ret = m_previewCallbackFunc(frame);
                if (ret < 0) {
                    CLOGE("m_previewCallbackFunc fail");
                    goto func_exit;
                }

                ret = m_doCallbackToPreviewFunc(pipeIdCsc, frame, previewCbBuffer, scpBuffer);
                if (ret < 0) {
                    CLOGE("m_doCallbackToPreviewFunc fail");
                    goto func_exit;
                }

                if (previewCbBuffer.index >= 0) {
                    ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
                    if (ret != NO_ERROR) {
                        CLOGE("Preview callback buffer return fail");
                    }
                    previewCbBuffer.index  = -2;
                }
            } else {
                m_previewCallbackQ->pushProcessQ(&frame);
                previewCbBuffer.index = -2;
            }
        }
#ifdef USE_GSC_FOR_PREVIEW
        else if (!m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME)
                && m_parameters->getCallbackNeedCSC()
                && m_previewFrameFactory->checkPipeThreadRunning(pipeIdCsc)) {
                m_previewFrameFactory->stopThread(pipeIdCsc);
        }
#endif
        /* In case of 240 fps recording */
        if (m_parameters->getFastFpsMode() == 3) {
            m_previewCallbackToggle = (m_previewCallbackToggle + 1) % 6;
        }

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

#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
            if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
            {
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_START_PREVIEW_END, false);
                TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_TOTAL_TIME, false);
                if (g_timeLoggerLaunchIndex % TIME_LOGGER_LAUNCH_COUNT == 0)
                    TIME_LOGGER_SAVE_BASE(&g_timeLoggerLaunch, m_cameraId);
            }
#endif
            CLOGD("m_firstPreviewTimer stop");

            CLOGD("============= First Preview time ==================");
            CLOGD("= startPreview ~ first frame  : %d msec", (int)m_firstPreviewTimer.durationMsecs());
            CLOGD("===================================================");
            autoFocusMgr->displayAFInfo();
        }
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_HRM
        if (m_uv_rayHandle != NULL) {
            ret = sensor_listener_get_data(m_uv_rayHandle, ST_UV_RAY, &m_uv_rayListenerData, false);
            if (ret < 0) {
                if ((hrm_error_cnt % 30) == 0) {
                    CLOGW("%d:skip the HRM data", ret);
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
                    CLOGW("%d:skip the Light IR data", ret);
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
                    CLOGW("%d:skip the Gyro data", ret);
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
                    CLOGW("%d:skip the Acceleration data", ret);
                    accelerometer_error_cnt = 1;
                } else {
                    accelerometer_error_cnt++;
                }
            } else {
                m_parameters->m_setAccelerometer(m_accelerometerListenerData);
            }
        }
#endif

#ifdef SAMSUNG_ROTATION
        if (m_rotationHandle != NULL) {
            ret = sensor_listener_get_data(m_rotationHandle, ST_ROTATION, &m_rotationListenerData, false);
            if (ret < 0) {
                if ((rotation_error_cnt % 30) == 0) {
                    CLOGW("%d:skip the Rotation data", ret);
                    rotation_error_cnt = 1;
                } else {
                    rotation_error_cnt++;
                }
            } else {
                /* rotationListenerData : counterclockwise,index / DeviceOrientation : clockwise,degree  */
                int orientation_Degrees = 0;
                switch (m_rotationListenerData.rotation.orientation) {
                    case SCREEN_DEGREES_0:
                        orientation_Degrees = 0;
                        break;
                    case SCREEN_DEGREES_270:
                        orientation_Degrees = 270;
                        break;
                    case SCREEN_DEGREES_180:
                        orientation_Degrees = 180;
                        break;
                    case SCREEN_DEGREES_90:
                        orientation_Degrees = 90;
                        break;
                    default:
                        CLOGW("%d:skip the Rotation data(unknown index)", m_rotationListenerData.rotation.orientation);
                        orientation_Degrees = m_parameters->getDeviceOrientation();
                        break;
                }
                if (m_parameters->getDeviceOrientation() != orientation_Degrees) {
                    m_parameters->setDeviceOrientation(orientation_Degrees);
                    ExynosCameraActivityUCTL *uctlMgr = NULL;
                    uctlMgr = m_exynosCameraActivityControl->getUCTLMgr();
                    if (uctlMgr != NULL)
                        uctlMgr->setDeviceRotation(m_parameters->getFdOrientation());
                }
            }
        }
#endif /* SAMSUNG_ROTATION */
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SUPPORT_SW_VDIS
        if (m_swVDIS_Mode == true) {
            int swVDIS_InW, swVDIS_InH, swVDIS_OutW, swVDIS_OutH;
            int swVDIS_StartX, swVDIS_StartY;

            m_parameters->getHwPreviewSize(&swVDIS_InW, &swVDIS_InH);
#ifdef SAMSUNG_DUAL_SOLUTION
            if (m_parameters->getDualCameraMode() == true) {
                ExynosRect fusionSrcRect;
                ExynosRect fusionDstRect;
                int previewW, previewH;
                m_parameters->getPreviewSize(&previewW, &previewH);
                m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);

                swVDIS_InW = fusionDstRect.w;
                swVDIS_InH = fusionDstRect.h;
            }
#endif
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
        if ((m_parameters->getShotMode() == SHOT_MODE_BEAUTY_FACE) || (m_parameters->getShotMode() == SHOT_MODE_KIDS)
#ifdef LLS_CAPTURE
            || (m_parameters->getLLSOn() == true && getCameraIdInternal() == CAMERA_ID_FRONT)
#endif
            ) {
            if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME) &&
                checkBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE)) {
                CLOGV("skip the preview callback and the preview display while compressed callback.");
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
#ifdef TIME_LOGGER_PREVIEW_ENABLE
        TIME_LOGGER_UPDATE(m_cameraId, frameCount, PIPE_MCSC0, INTERVAL, PREVIEW_SERVICE_ENQUEUE, true);
#endif
        ret = m_putBuffers(m_scpBufferMgr, scpBuffer.index);
        if (ret < 0) {
            /* TODO: error handling */
            CLOGE("put Buffer fail");
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
	if (previewCbBuffer.index >= 0) {
        ret = m_putBuffers(m_previewCallbackBufferMgr, previewCbBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("Preview callback buffer return fail");
        }
    }

    ret = m_scpBufferMgr->cancelBuffer(scpBuffer.index);

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

    int  ret  = 0;
    int  pipeId = PIPE_MCSC2;

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_Mode == true && m_doCscRecording == true) {
        pipeId = PIPE_GSC_VIDEO;
    }
#endif
#ifdef USE_DUAL_CAMERA
    if (m_doCscRecording == true && m_parameters->getDualCameraMode() == true)
        pipeId = PIPE_GSC_VIDEO;
#endif
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
                (long long)m_recordingStartTimeStamp,
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

#ifdef SAMSUNG_HLV
                if (m_HLV) {
                    /* Ignore the ERROR .. HLV solution is smart */
                    ret = m_ProgramAndProcessHLV(&buffer);
                }
#endif
#ifdef TIME_LOGGER_RECORDING_ENABLE
                TIME_LOGGER_UPDATE(m_cameraId, buffer.index, PIPE_MCSC2, INTERVAL, RECORDING_CALLBACK, true);
                TIME_LOGGER_UPDATE(m_cameraId, buffer.index, PIPE_MCSC2, DURATION, RECORDING_CALLBACK, true);
#endif
                m_dataCbTimestamp(
                    timeStamp,
                    CAMERA_MSG_VIDEO_FRAME,
                    m_recordingCallbackHeap,
                    heapIndex,
                    m_callbackCookie);
                }
#ifdef TIME_LOGGER_RECORDING_ENABLE
                TIME_LOGGER_UPDATE(m_cameraId, buffer.index, PIPE_MCSC2, DURATION, RECORDING_CALLBACK, false);
#endif
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

#ifdef SAMSUNG_DUAL_SOLUTION
    m_fusionPreviewWrapper = ExynosCameraSingleton<ExynosCameraFusionPreviewWrapper>::getInstance();
    m_fusionCaptureWrapper = ExynosCameraSingleton<ExynosCameraFusionCaptureWrapper>::getInstance();
    m_previewSolutionHandle = NULL;
    m_captureSolutionHandle = NULL;
#endif

#ifdef SAMSUNG_COMPANION
    m_companionNode = NULL;
    m_companionThread = NULL;
    CLOGI("cameraId(%d), sensorId(%d), use_companion(%d)",
        cameraId, m_cameraSensorId, m_use_companion);

    if(m_use_companion == true) {
        m_companionThread = new mainCameraThread(this, &ExynosCamera::m_companionThreadFunc,
                                                  "companionshotThread", PRIORITY_URGENT_DISPLAY);
        if (m_companionThread != NULL) {
            m_companionThread->run();
            CLOGD(" companionThread starts");
        } else {
            CLOGE("failed the m_companionThread.");
        }
    }
#else
    CLOGI("cameraId(%d), sensorId(%d)", cameraId, m_cameraSensorId);
#endif

#if defined(SAMSUNG_EEPROM)
    m_eepromThread = NULL;
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD(" eepromThread starts");
        } else {
            CLOGE("failed the m_eepromThread");
        }
    }
#endif

#ifdef SAMSUNG_UNIPLUGIN
    m_uniPluginThread = new mainCameraThread(this, &ExynosCamera::m_uniPluginThreadFunc, "uniPluginThread");
    if(m_uniPluginThread != NULL) {
        m_uniPluginThread->run();
        CLOGD(" uniPluginThread starts");
    } else {
        CLOGE("failed the m_uniPluginThread");
    }
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    m_preStartPictureInternalThread = new mainCameraThread(this, &ExynosCamera::m_preStartPictureInternalThreadFunc,
                                                  "m_preStartPictureInternalThread");
    CLOGD(" m_preStartPictureInternalThread created");
#endif

    CLOGI(" -OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificConstructor(__unused int cameraId, __unused camera_device_t *dev)
{
    CLOGI(" -IN-");

#ifdef SAMSUNG_UNI_API
    uni_api_init();
#endif

#ifdef DEBUG_IQ_OSD
    isOSDMode = -1;
    if (access("/data/media/0/debug_iq_osd.txt", F_OK) == 0) {
        CLOGD(" debug_iq_osd.txt access successful");
        isOSDMode = 1;
    }
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
#ifdef SAMSUNG_STR_CAPTURE
    m_createInternalBufferManager(&m_strCaptureBufferMgr, "STRCAPTURE_BUF");
#endif

#ifdef SR_CAPTURE
    m_isCopySrMdeta = false;
    m_srCallbackHeap = NULL;
    memset(m_faces_sr, 0, sizeof(camera_face_t) * NUM_OF_DETECTED_FACES);
#endif

    m_postPictureCallbackThread = new mainCameraThread(this, &ExynosCamera::m_postPictureCallbackThreadFunc, "postPictureCallbackThread");
    CLOGD(" postPictureCallbackThread created");
#ifdef SAMSUNG_LBP
    m_LBPThread = new mainCameraThread(this, &ExynosCamera::m_LBPThreadFunc, "LBPThread");
    CLOGD(" LBPThread created");
#endif

    /* m_ThumbnailCallback Thread */
    m_ThumbnailCallbackThread = new mainCameraThread(this, &ExynosCamera::m_ThumbnailCallbackThreadFunc, "m_ThumbnailCallbackThread");
    CLOGD("m_ThumbnailCallbackThread created");

#ifdef RAWDUMP_CAPTURE
    /* RawCaptureDump Thread */
    m_RawCaptureDumpThread = new mainCameraThread(this, &ExynosCamera::m_RawCaptureDumpThreadFunc, "m_RawCaptureDumpThread");
    CLOGD("[RAWDUMP_CAP] RawCaptureDumpThread created");
#endif

#ifdef SAMSUNG_DNG
    /* m_DNGCaptureThread */
    m_DNGCaptureThread = new mainCameraThread(this, &ExynosCamera::m_DNGCaptureThreadFunc, "m_DNGCaptureThread");
    CLOGD("m_DNGCaptureThread created");
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    /* m_LDCapture Thread */
    m_LDCaptureThread = new mainCameraThread(this, &ExynosCamera::m_LDCaptureThreadFunc, "m_LDCaptureThread");
    CLOGD("m_LDCaptureThread created");
#endif
#ifdef USE_MSC_CAPTURE
    m_MscCaptureThread = new mainCameraThread(this, &ExynosCamera::m_MscCaptureThreadFunc, "m_MscCaptureThread");
    CLOGD("m_MscCaptureThread created");
#endif
#ifdef SAMSUNG_SENSOR_LISTENER
    m_sensorListenerThread = new mainCameraThread(this, &ExynosCamera::m_sensorListenerThreadFunc, "sensorListenerThread");
    CLOGD("m_sensorListenerThread created");
#endif

    m_yuvCallbackThread = new mainCameraThread(this, &ExynosCamera::m_yuvCallbackThreadFunc, "yuvCallbackThread");
    CLOGD("yuvCallbackThread created");

    /* vision */
    m_visionThread = new mainCameraThread(this, &ExynosCamera::m_visionThreadFunc, "VisionThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("visionThread created");

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
    m_LDCaptureQ->setWaitTime(1000000000);
#endif
#ifdef USE_MSC_CAPTURE
    m_MscCaptureQ = new frame_queue_t(m_MscCaptureThread);
    m_MscCaptureQ->setWaitTime(1000000000);
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
#ifdef SAMSUNG_ROTATION
    m_rotationHandle = NULL;
#endif

#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_fd_dm == NULL) {
        m_swVDIS_fd_dm = new struct camera2_dm;
    }
    if (m_swVDIS_FaceData == NULL) {
        m_swVDIS_FaceData = new struct swVDIS_FaceData;
    }
#endif /*SUPPORT_SW_VDIS*/

#ifdef SAMSUNG_HYPER_MOTION
    if (m_swHM_fd_dm == NULL) {
        m_swHM_fd_dm = new struct camera2_dm;
    }
    if (m_swHM_FaceData == NULL) {
        m_swHM_FaceData = new struct swHM_FaceData;
    }
#endif /*SUPPORT_SW_HM*/

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureCount = 0;

    for (int i = 0; i < MAX_LD_CAPTURE_COUNT; i++) {
        m_LDBufIndex[i] = 0;
    }
#endif

#ifdef SAMSUNG_HIFI_LLS
    m_hifiLLSEnabled = false;
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

#ifdef SAMSUNG_INF_BURST
    memset(&m_preBurstTimeinfo, 0, sizeof(m_preBurstTimeinfo));
#endif

#ifdef SAMSUNG_IDDQD
    m_lens_dirty_detected = 0;
#endif

    CLOGI(" -OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI(" -IN-");

#ifdef SAMSUNG_TN_FEATURE
    status_t ret = NO_ERROR;
#endif

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
#ifdef USE_MSC_CAPTURE
    if (m_MscCaptureQ != NULL) {
        delete m_MscCaptureQ;
        m_MscCaptureQ = NULL;
    }
#endif
#ifdef SAMSUNG_LBP
    if (m_lbpBufferMgr != NULL) {
        delete m_lbpBufferMgr;
        m_lbpBufferMgr = NULL;
        CLOGD("BufferManager(lbpBufferMgr) destroyed");
    }
#endif
#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        delete m_lensDCBufferMgr;
        m_lensDCBufferMgr = NULL;
        CLOGD("BufferManager(m_lensDCBufferMgr) destroyed");
    }
#endif
#ifdef SAMSUNG_STR_CAPTURE
    if (m_strCaptureBufferMgr != NULL) {
        delete m_strCaptureBufferMgr;
        m_strCaptureBufferMgr = NULL;
        CLOGD("BufferManager(m_strCaptureBufferMgr) destroyed");
    }
#endif
#ifdef SUPPORT_SW_VDIS
    if (m_swVDIS_BufferMgr != NULL) {
        delete m_swVDIS_BufferMgr;
        m_swVDIS_BufferMgr = NULL;
        VDIS_LOG("VDIS_HAL BufferManager(m_swVDIS_BufferMgr) destroyed");
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
        CLOGD("HyperMotion_HAL BufferManager(m_hyperMotion_BufferMgr) destroyed");
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

#ifdef SAMSUNG_DUAL_SOLUTION
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        if (m_fusionPreviewWrapper->m_getIsInit() == true)
            m_fusionPreviewWrapper->m_deinitDualSolution(getCameraId());

        if (m_previewSolutionHandle != NULL) {
            ret = uni_plugin_unload(&m_previewSolutionHandle);
            if (ret < 0) {
                CLOGE("[Fusion]DUAL_PREVIEW plugin unload failed!!");
            }
        }

#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (m_fusionCaptureWrapper->m_getIsInit() == true)
            m_fusionCaptureWrapper->m_deinitDualSolution(getCameraId());

        if (m_captureSolutionHandle != NULL) {
            ret = uni_plugin_unload(&m_captureSolutionHandle);
            if (ret < 0) {
                CLOGE("[Fusion]DUAL_CAPTURE plugin unload failed!!");
            }
        }
#endif
        m_previewSolutionHandle = NULL;
        m_captureSolutionHandle = NULL;
        m_fusionCaptureWrapper->m_setSolutionHandle(NULL, NULL);
        m_fusionPreviewWrapper->m_setSolutionHandle(NULL, NULL);
    }
#endif

#ifdef SAMSUNG_HIFI_LLS
    if (m_hifiLLSEnabled == true && m_LLSpluginHandle != NULL) {
        int powerCtrl = 0;
        ret = uni_plugin_set(m_LLSpluginHandle,
                HIFI_LLS_PLUGIN_NAME, UNI_PLUGIN_INDEX_POWER_CONTROL, &powerCtrl);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_POWER_CONTROL failed!! ret(%d)", ret);
        }
        m_hifiLLSEnabled = false;
    }
#endif

#ifdef SAMSUNG_LENS_DC
    if(m_DCpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_DCpluginHandle);
        if(ret < 0) {
            CLOGE("[DC]DC plugin unload failed!!");
        }
    }
#endif

#ifdef SAMSUNG_LLV
    if(m_LLVpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLVpluginHandle);
        if(ret < 0) {
            CLOGE("[LLV]LLV plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_OT
    if(m_OTpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_OTpluginHandle);
        if(ret < 0) {
            CLOGE("[OBTR]:Object Tracking plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_LBP
    if(m_LBPpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LBPpluginHandle);
        if(ret < 0) {
            CLOGE("[LBP]:Bestphoto plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_JQ
    if(m_JQpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ]:JPEG Qtable plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_BD
    if(m_BDpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_BDpluginHandle);
        if(ret < 0) {
            CLOGE("[BD]:Blur Detection plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    if(m_LLSpluginHandle != NULL) {
        ret = uni_plugin_unload(&m_LLSpluginHandle);
        if(ret < 0) {
            CLOGE("[LLS_MBR] LLS plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_STR_CAPTURE
    if (m_STRCapturePluginHandle != NULL) {
        ret = uni_plugin_unload(&m_STRCapturePluginHandle);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE] STR_CAPTURE plugin unload failed!!");
        }
    }
#endif
#ifdef SAMSUNG_STR_PREVIEW
    if (m_STRPreviewPluginHandle != NULL) {
        ret = uni_plugin_unload(&m_STRPreviewPluginHandle);
        if (ret < 0) {
            CLOGE("[STR_PRVIEW] STR_PRVIEW plugin unload failed!!");
        }
    }
#endif
#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
    if (m_FocusPeakingPluginHandle != NULL) {
        ret = uni_plugin_unload(&m_FocusPeakingPluginHandle);
        if (ret < 0) {
            CLOGE("[FocusPeaking] FocusPeaking plugin unload failed!!");
        }
    }
#endif
#endif

#ifdef SAMSUNG_IDDQD
    if (m_IDDQDPluginHandle != NULL) {
        ret = uni_plugin_unload(&m_IDDQDPluginHandle);
        if (ret < 0) {
            CLOGE("[IDDQD] IDDQD plugin unload failed!!");
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
#ifdef SAMSUNG_ROTATION
    if (m_rotationHandle != NULL) {
        sensor_listener_disable_sensor(m_rotationHandle, ST_ROTATION);
        sensor_listener_unload(&m_rotationHandle);
        m_rotationHandle = NULL;
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

#ifdef SAMSUNG_HYPER_MOTION
    if (m_swHM_fd_dm != NULL) {
        delete m_swHM_fd_dm;
        m_swHM_fd_dm = NULL;
    }

    if (m_swHM_FaceData != NULL) {
        delete m_swHM_FaceData;
        m_swHM_FaceData = NULL;
    }
#endif

    CLOGI(" -OUT-");

    return;
}

status_t ExynosCamera::m_doFdCallbackFunc(ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraDurationTimer m_fdcallbackTimer;
    long long m_fdcallbackTimerTime;
#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF) ||(SAMSUNG_LIVE_OUTFOCUS)
    ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();
#endif
    bool autoFocusChanged = false;
    uint32_t shutterSpeed = 0;
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
        CLOGE(" meta_shot_ext is null");
        return INVALID_OPERATION;
    }

    memset(m_fdmeta_shot, 0x00, sizeof(struct camera2_shot_ext));

#ifdef SAMSUNG_DUAL_SOLUTION
    if (m_parameters->getDualCameraMode() == true &&
            frame->getSyncType() == SYNC_TYPE_SYNC) {
        int fusionIndex = m_parameters->getFusionParamIndex();
        int outNode = (fusionIndex == UNI_PLUGIN_CAMERA_TYPE_TELE ? OUTPUT_NODE_2 : OUTPUT_NODE_1);

        CLOGV("fusionIndex = %d, outNode = %d", fusionIndex, outNode);

        if (frame->getDynamicMeta(m_fdmeta_shot, outNode) != NO_ERROR) {
            CLOGE(" meta_shot_ext is null");
            return INVALID_OPERATION;
        }

        frame->getUserDynamicMeta(m_fdmeta_shot, outNode);
    } else {
        if (frame->getDynamicMeta(m_fdmeta_shot) != NO_ERROR) {
            CLOGE(" meta_shot_ext is null");
            return INVALID_OPERATION;
        }

        frame->getUserDynamicMeta(m_fdmeta_shot);
    }
#else
    if (frame->getDynamicMeta(m_fdmeta_shot) != NO_ERROR) {
        CLOGE(" meta_shot_ext is null");
        return INVALID_OPERATION;
    }

    frame->getUserDynamicMeta(m_fdmeta_shot);
#endif

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

#ifdef SAMSUNG_DUAL_SOLUTION
        if (m_parameters->getDualCameraMode() == true) {
            if (frame->getSyncType() == SYNC_TYPE_SWITCH) {
                if (m_fusionPreviewWrapper->m_getNeedMargin(CAMERA_ID_BACK_1, frame->getZoom()) == 0) {
                    m_parameters->getPreviewSize(&previewW, &previewH);
                } else {
                    m_parameters->getHwPreviewSize(&previewW, &previewH);
                }
            } else {
                if (m_fusionPreviewWrapper->m_getNeedMargin(CAMERA_ID_BACK_0, frame->getZoom()) == 0) {
                    m_parameters->getPreviewSize(&previewW, &previewH);
                } else {
                    m_parameters->getHwPreviewSize(&previewW, &previewH);
                }
            }
        } else {
            m_parameters->getHwPreviewSize(&previewW, &previewH);
        }
#else
        m_parameters->getHwPreviewSize(&previewW, &previewH);
#endif

        camera2_node_group node_group_info;
        frame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_3AA);

        dm = &(m_fdmeta_shot->shot.dm);
        if (dm == NULL) {
            CLOGE(" dm data is null");
            return INVALID_OPERATION;
        }

        CLOGV("faceDetectMode(%d)", dm->stats.faceDetectMode);
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
                        vraWidth = dm->stats.faceSrcImageSize[0];
                        vraHeight = dm->stats.faceSrcImageSize[1];

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

#ifdef SAMSUNG_DUAL_SOLUTION
                    /* Fix faceRect according to ShiftX/ShiftY diff */
                    if (m_parameters->getDualCameraMode() == true) {
                        UniPlugin_DCVOZ_CAMERAPARAM_t *fusionParam;
                        fusionParam = m_parameters->getFusionParam();
                        detectedFace[i].x1 += fusionParam->PreviewImageShiftX;
                        detectedFace[i].y1 += fusionParam->PreviewImageShiftY;
                        detectedFace[i].x2 += fusionParam->PreviewImageShiftX;
                        detectedFace[i].y2 += fusionParam->PreviewImageShiftY;
                        CLOGV("ShiftX = %d, ShiftY = %d, Result = (%d, %d, %d, %d)",
                                fusionParam->PreviewImageShiftX,
                                fusionParam->PreviewImageShiftY,
                                detectedFace[i].x1, detectedFace[i].y1,
                                detectedFace[i].x2, detectedFace[i].y2);
                    }
#endif

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
#ifdef LLS_CAPTURE
        || (m_frameMetadata.light_condition == LLS_LEVEL_LLS_INDICATOR_ONLY)
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

#ifdef LLS_CAPTURE
    if (m_flagBrightnessValue) {
        m_frameMetadata.brightness_value = m_fdmeta_shot->shot.udm.internal.vendorSpecific2[102];
    } else {
        m_frameMetadata.brightness_value = 0;
    }
    CLOGV("brightness_value(%d)", m_frameMetadata.brightness_value);
#endif

#ifdef TOUCH_AE
    if (m_parameters->getMeteringMode() >= METERING_MODE_CENTER_TOUCH
        && m_parameters->getMeteringMode() <= METERING_MODE_AVERAGE_TOUCH
        && m_fdmeta_shot->shot.dm.aa.vendor_touchAeDone == 1 /* Touch AE status flag - 0:searching 1:done */
        ) {
        m_notifyCb(AE_RESULT, 0, 0, m_callbackCookie);
        CLOGD(" AE_RESULT(%d)", m_fdmeta_shot->shot.dm.aa.aeState);
    }

#ifdef LLS_CAPTURE
    m_frameMetadata.ae_bv_changed = m_fdmeta_shot->shot.dm.aa.vendor_touchBvChange;
#endif
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
        m_frameMetadata.current_set_data.exposure_time = (uint32_t)(1000000000.0/shutterSpeed);
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
        CLOGV("[OBTR]focusState: %d, focusROI[0]: %d, focusROI[1]: %d, focusROI[2]: %d, focusROI[3]: %d",
             m_OTfocusData.FocusState,
            m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROITop,
            m_OTfocusData.FocusROIRight, m_OTfocusData.FocusROIBottom);
    }
#endif
#ifdef SAMSUNG_FACE_BEAUTY_SCENE
    if (m_cameraId == CAMERA_ID_FRONT)
        m_frameMetadata.scene_index = m_fdmeta_shot->shot.udm.ae.vendorSpecific[395];
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

#ifdef SAMSUNG_LIVE_OUTFOCUS
    if (m_parameters->getShotMode() == SHOT_MODE_LIVE_OUTFOCUS
        && m_parameters->getCameraId() == CAMERA_ID_BACK_1) {
        ExynosRect bayerCropRegionRect, cropRegionRect;
        ExynosRect2 focusedRect, convertingRect;
        int curAFstatus = 0;

        m_parameters->getHwBayerCropRegion(&bayerCropRegionRect.w, &bayerCropRegionRect.h,
                                           &bayerCropRegionRect.x, &bayerCropRegionRect.y);

        if (m_parameters->isUseIspInputCrop() == true
            || m_parameters->isUseMcscInputCrop() == true) {
            int zoomLevel = 0;
            float zoomRatio = m_parameters->getZoomRatio(0) / 1000;

            zoomLevel = m_parameters->getZoomLevel();
            zoomRatio = m_parameters->getZoomRatio(zoomLevel) / 1000;
            ret = getCropRectAlign(bayerCropRegionRect.w, bayerCropRegionRect.h,
                    bayerCropRegionRect.w, bayerCropRegionRect.h,
                    &cropRegionRect.x, &cropRegionRect.y,
                    &cropRegionRect.w, &cropRegionRect.h,
                    2, 2,
                    zoomLevel, zoomRatio);
        } else {
            cropRegionRect = bayerCropRegionRect;
        }

        focusedRect.x1 = m_fdmeta_shot->shot.dm.aa.afRegions[0];
        focusedRect.y1 = m_fdmeta_shot->shot.dm.aa.afRegions[1];
        focusedRect.x2 = m_fdmeta_shot->shot.dm.aa.afRegions[2];
        focusedRect.y2 = m_fdmeta_shot->shot.dm.aa.afRegions[3];

        curAFstatus = autoFocusMgr->getCurrentState();

        if (curAFstatus == ExynosCameraActivityAutofocus::AUTOFOCUS_STATE_SUCCEESS) {
            m_frameMetadata.focus_area.focusState =  1;
            convertingRect = convertingHWArea2AndroidArea(&focusedRect, &cropRegionRect);
            m_frameMetadata.focus_area.focusROI[0] = (int16_t)convertingRect.x1;
            m_frameMetadata.focus_area.focusROI[1] = (int16_t)convertingRect.y1;
            m_frameMetadata.focus_area.focusROI[2] = (int16_t)convertingRect.x2;
            m_frameMetadata.focus_area.focusROI[3] = (int16_t)convertingRect.y2;
        } else {
            m_frameMetadata.focus_area.focusState =  0;
        }

        CLOGV("current_set_data.focus_area.focusState(%d)", m_frameMetadata.focus_area.focusState);
        CLOGV("current_set_data.focus_area.focusROI(%d, %d, %d, %d)",
                m_frameMetadata.focus_area.focusROI[0], m_frameMetadata.focus_area.focusROI[1],
                m_frameMetadata.focus_area.focusROI[2], m_frameMetadata.focus_area.focusROI[3]);
    }
#endif

#ifdef SAMSUNG_IDDQD
        CLOGV("[IDDQD](%s[%d]): DIRT=%d", __FUNCTION__, __LINE__, m_lens_dirty_detected); 
        m_frameMetadata.lens_dirty_detected = m_lens_dirty_detected;
#endif

    m_fdcallbackTimer.start();

    if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) &&
            (m_flagStartFaceDetection || m_flagLLSStart || m_flagLightCondition || m_flagMetaDataSet
            || m_flagBrightnessValue
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
            CLOGE("Failed to put DepthMap buffer to bufferMgr");
        }
    }
#endif

#if defined(SAMSUNG_DOF) || defined(SUPPORT_MULTI_AF)
    m_clearDOFmeta();
#endif
#ifdef SAMSUNG_TN_FEATURE
    memset(&m_frameMetadata.current_set_data, 0, sizeof(camera_current_set_t));
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
        goto func_exit;
    }

    m_previewFrameFactory->setOutputFrameQToPipe(m_recordingQ, pipeId);

    m_recordingStopLock.lock();
    if (m_getRecordingEnabled() == false) {
        m_recordingStopLock.unlock();
        CLOGD("DEBUG(%s[%d]): m_getRecordingEnabled is false, skip frame(%d) previewBuf(%d) recordingBuf(%d)",
            __FUNCTION__, __LINE__, newFrame->getFrameCount(), previewBuf.index, recordingBuf.index);

        if (recordingBuf.index >= 0) {
            m_putBuffers(m_recordingBufferMgr, recordingBuf.index);
        }
        goto func_exit;
    }

    ret = m_insertFrameToList(&m_recordingProcessList, newFrame, &m_recordingProcessListLock);
    if (ret != NO_ERROR) {
        m_recordingStopLock.unlock();
        CLOGE("ERR(%s[%d]):[F%d]Failed to insert frame into m_recordingProcessList",
                __FUNCTION__, __LINE__, newFrame->getFrameCount());

        if (recordingBuf.index >= 0) {
            m_putBuffers(m_recordingBufferMgr, recordingBuf.index);
        }
        goto func_exit;
    }

    m_previewFrameFactory->pushFrameToPipe(newFrame, pipeId);
    m_recordingStopLock.unlock();

    CLOGV("DEBUG(%s[%d]):--OUT--", __FUNCTION__, __LINE__);
    return ret;

func_exit:
    if (newFrame != NULL) {
        newFrame = NULL;
    }

    CLOGV("--OUT--");
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
#ifdef SAMSUNG_MAGICSHOT
                && (m_parameters->getShotMode() != SHOT_MODE_MAGIC)
#endif
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
#ifdef SAMSUNG_COMPANION
    int bufferCount = (m_parameters->getRTHdr() == COMPANION_WDR_ON) ?
                        NUM_FASTAESTABLE_BUFFER : NUM_FASTAESTABLE_BUFFER - 4;
#else
    int bufferCount = NUM_FASTAESTABLE_BUFFER;
#endif

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
    CLOGD("videoSize = %d x %d",   videoW, videoH);

    m_doCscRecording = true;
    if (m_parameters->doCscRecording() == true) {
        m_recordingBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
        CLOGI("do Recording CSC !!! m_recordingBufferCount(%d)", m_recordingBufferCount);
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

    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }

    /* alloc recording Callback Heap */
    m_recordingCallbackHeap = m_getMemoryCb(-1, sizeof(struct VideoNativeHandleMetadata), m_recordingBufferCount, m_callbackCookie);
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

    if ((m_doCscRecording == true
#ifdef USE_DUAL_CAMERA
         || m_parameters->getDualCameraMode() == true
#endif
         )
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
    int ret = 0;
    bool useGSCPipe = false;

#ifdef SUPPORT_SW_VDIS
    CLOGD("m_doCscRecording == %d, m_swVDIS_Mode == %d",
             m_doCscRecording, m_swVDIS_Mode);

    if (m_doCscRecording == true && m_swVDIS_Mode == true)
        useGSCPipe = true;
#endif
#ifdef USE_DUAL_CAMERA
    /* If dual camera, use libcsc only */
    if (m_doCscRecording == true && m_parameters->getDualCameraMode() == true)
        useGSCPipe = true;
#endif
    if (m_doCscRecording == true && useGSCPipe == true) {
        int recPipeId = PIPE_GSC_VIDEO;
        {
            Mutex::Autolock lock(m_recordingStopLock);
            m_previewFrameFactory->stopPipe(recPipeId);
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
            CLOGW("frame(%d) wasn't release, and forced release at here",
                     bufferIndex);
        }
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
    CLOGI("");
    int loop = false;

#ifdef OIS_CAPTURE
    if(m_OISCaptureShutterEnabled) {
        CLOGD("shutter callback wait start");
        ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
        m_sCaptureMgr->waitShutterCallback();
        CLOGD("fast shutter callback wait end(%d)", m_sCaptureMgr->getOISCaptureFcount());
    }
#endif

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
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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
    if (m_vraBufferMgr != NULL) {
        m_vraBufferMgr->deinit();
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
    if (m_parameters->getUsePureBayerReprocessing() == true
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
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            needMmap = true;
        } else
#endif
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
#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        needMmap = true;
    } else
#endif
    {
        needMmap = false;
    }
    if (m_parameters->getHighResolutionCallbackMode() == false) {
        ret = m_allocBuffers(m_sccReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize, type, allocMode,
                             true, needMmap);
        if (ret != NO_ERROR) {
            CLOGE("m_sccReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                     minBufferCount, maxBufferCount);
            return ret;
        }
        CLOGI("m_allocBuffers(YUV Re Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                 maxPictureW, maxPictureH, planeCount, maxBufferCount);
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraCaptureFusionMode() == true) {
        ret = m_allocBuffers(m_fusionReprocessingBufferMgr, planeCount, planeSize, bytesPerLine,
                minBufferCount, maxBufferCount, batchSize, type, allocMode,
                true, true);
        if (ret != NO_ERROR) {
            CLOGE("m_fusionReprocessingBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                    minBufferCount, maxBufferCount);
            return ret;
        }

        CLOGI("m_allocBuffers(Fusion Reprocessing Buffer) %d x %d, planeCount(%d), maxBufferCount(%d)",
                maxPictureW, maxPictureH, planeCount, maxBufferCount);
    }
#endif

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

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true) {
        needMmap = true;
    } else
#endif
    {
        needMmap = false;
    }
    if (m_parameters->getHighResolutionCallbackMode() == false) {
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
    }

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
        m_sccReprocessingBufferMgr->deinit(m_cameraId);
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

#ifdef SAMSUNG_STR_CAPTURE
    if (m_strCaptureBufferMgr != NULL) {
        m_strCaptureBufferMgr->deinit();
    }
#endif

    if (m_thumbnailGscBufferMgr != NULL) {
        m_thumbnailGscBufferMgr->deinit();
    }

#ifdef USE_MSC_CAPTURE
    if (m_mscBufferMgr != NULL) {
        m_mscBufferMgr->deinit();
    }
#endif

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

    /* Bayer(FLITE, 3AC) Buffer */
    if (m_parameters->getUsePureBayerReprocessing() == true)
        bayerFormat = m_parameters->getBayerFormat(PIPE_VC0);
    else
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);

#ifdef USE_REMOSAIC_CAPTURE
    if (m_parameters->isUseRemosaicCapture() == true) {
        bayerFormat = m_parameters->getBayerFormat(PIPE_VC0);
    }
#endif

    bytesPerLine[0] = getBayerLineSize(sensorMaxW, bayerFormat);
    planeSize[0]    = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat);
    planeCount      = 2;
    batchSize       = m_parameters->getBatchSize(PIPE_FLITE);

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
        m_bayerBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER]);

#ifdef USE_DUAL_CAMERA
    } else if (getCameraIdInternal() == CAMERA_ID_BACK_1) {
#if defined(CAMERA_ADD_BAYER_ENABLE) || defined(SAMSUNG_DNG)
            needMmap = true;
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#else
            needMmap = false;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
#endif
#endif
    } else if (m_parameters->getDualMode() == false) {
#if defined(SAMSUNG_DNG)
        needMmap = true;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#else
        needMmap = false;
        type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
#endif
        m_bayerBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER]);
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

    if (m_parameters->getIntelligentMode() != 1
		|| (CONFIG_MODE::HIGHSPEED_60 >= m_parameters->getFastFpsMode())) {
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

#if defined(SAMSUNG_DNG_DIRTY_BAYER)
#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    if (m_parameters->getUsePureBayerReprocessing() == false)
#else
    if (m_parameters->getUsePureBayerReprocessing() == false && m_parameters->getDNGCaptureModeOn() == true)
#endif
    {
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

#ifdef SAMSUNG_LBP
    planeCount      = 3;
    batchSize       = 1;
    planeSize[0]    = hwPreviewW * hwPreviewH;
    planeSize[1]    = hwPreviewW * hwPreviewH / 2;
    maxBufferCount  = m_parameters->getHoldFrameCount();
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(m_lbpBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, maxBufferCount, batchSize,
                         type, true, true);
    if (ret < 0) {
        CLOGE("[LBP]m_lbpBufferMgr m_allocBuffers(bufferCount=%d) fail",
             maxBufferCount);
        return ret;
    }
#endif

#ifdef SUPPORT_SW_VDIS
    int cameraId = getCameraIdInternal();
    if (m_swVDIS_Mode && (m_swVDIS_ModeDual == false || (m_swVDIS_ModeDual == true && cameraId != CAMERA_ID_BACK_1))) {
        int hwPreviewW = 0, hwPreviewH = 0;
        m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
        m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
#ifdef SAMSUNG_DUAL_SOLUTION
        if (m_parameters->getDualCameraMode() == true) {
            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;
            int previewW = 0, previewH = 0;
            m_parameters->getPreviewSize(&previewW, &previewH);
            m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);

            hwPreviewW = fusionDstRect.w;
            hwPreviewH = fusionDstRect.h;
            m_swVDIS_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);
        }
#endif

        planeCount = 3;
        batchSize = 1;
        planeSize[0] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                            * ROUND_UP(hwPreviewH, CAMERA_16PX_ALIGN)
                            + MFC_7X_BUFFER_OFFSET;
        planeSize[1] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                            * ROUND_UP(hwPreviewH / 2, CAMERA_16PX_ALIGN)
                            + MFC_7X_BUFFER_OFFSET;
        maxBufferCount = m_sw_VDIS_OutBufferNum;
        type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(m_swVDIS_BufferMgr, planeCount, planeSize, bytesPerLine,
                             maxBufferCount, maxBufferCount, batchSize, type,
                             true, true);
        if (ret != NO_ERROR) {
            CLOGE("m_swVDIS_BufferMgr m_allocBuffers(bufferCount=%d) fail",
                 maxBufferCount);
            return ret;
        }

        CLOGD("VDIS_HAL: m_allocBuffers(m_swVDIS_BufferMgr): %d x %d", hwPreviewW, hwPreviewH);
    }
#endif

#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters->getHyperMotionMode() == true) {
            int hwPreviewW = 0, hwPreviewH = 0;
            m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
            m_hyperMotion_AdjustPreviewSize(&hwPreviewW, &hwPreviewH);

            planeCount = 3;
            batchSize = 1;
            planeSize[0] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                                * ROUND_UP(hwPreviewH, CAMERA_16PX_ALIGN)
                                + MFC_7X_BUFFER_OFFSET;
            planeSize[1] = ROUND_UP(hwPreviewW, CAMERA_16PX_ALIGN)
                                * ROUND_UP(hwPreviewH / 2, CAMERA_16PX_ALIGN)
                                + MFC_7X_BUFFER_OFFSET;
            maxBufferCount = NUM_HYPERMOTION_BUFFERS;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

            ret = m_allocBuffers(m_hyperMotion_BufferMgr, planeCount, planeSize, bytesPerLine,
                                 maxBufferCount, maxBufferCount, batchSize,
                                 type, true, true);
            if (ret != NO_ERROR) {
                CLOGE("m_hyperMotion_BufferMgr m_allocBuffers(bufferCount=%d) fail",
                     maxBufferCount);
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
    int batchSize = 1;
    int minBufferCount;
    int maxBufferCount;
    exynos_camera_buffer_type_t type;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
    CLOGI("Sensor MAX width x height = %dx%d", sensorMaxW, sensorMaxH);
    m_parameters->getMaxThumbnailSize(&sensorMaxThumbW, &sensorMaxThumbH);
    CLOGI("Sensor MAX Thumbnail width x height = %dx%d", sensorMaxThumbW, sensorMaxThumbH);

#ifdef SAMSUNG_DNG
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("DNG capture on");
        dngBayerBufferSize = getBayerPlaneSize(sensorMaxW, sensorMaxH, bayerFormat)
                            + DNG_HEADER_LIMIT_SIZE
                            + (sensorMaxThumbW * sensorMaxThumbH * 3);
        m_dngBayerHeap = m_getMemoryCb(-1, dngBayerBufferSize, 1, m_callbackCookie);
        if (!m_dngBayerHeap || (m_dngBayerHeap->data == MAP_FAILED)) {
            CLOGE("DNG m_getMemoryCb(%d bytes) fail", dngBayerBufferSize);
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
#ifdef SAMSUNG_STR_CAPTURE
    if (!m_parameters->getSamsungCamera() && getCameraIdInternal() == CAMERA_ID_FRONT) {
        m_parameters->getMaxPictureSize(&f_pictureW, &f_pictureH);
    } else
#endif
#ifdef SAMSUNG_HIFI_LLS
    if (m_parameters->checkHiFiLLSEnable()) {
        m_parameters->getHwPictureSize(&f_pictureW, &f_pictureH);
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
    batchSize = m_parameters->getBatchSize(PIPE_MCSC2_REPROCESSING);
    minBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

#if defined (SAMSUNG_BD) || defined(SAMSUNG_LLS_DEBLUR)
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraIdInternal() == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        m_postPictureBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC]);
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#ifdef USE_DUAL_CAMERA
    } else if (getCameraIdInternal() == CAMERA_ID_BACK_1 || m_parameters->getDualMode() == false) {
        m_postPictureBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC]);
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#endif
    } else {
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    }
#else
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
#endif /* RESERVED_MEMORY_ENABLE */
    ret = m_allocBuffers(m_postPictureBufferMgr, planeCount, planeSize, bytesPerLine,
                         minBufferCount, maxBufferCount, batchSize,
                         type, allocMode, true, true);
    if (ret < 0) {
        CLOGE("m_gscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                 minBufferCount, maxBufferCount);
        return ret;
    }
#else
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraIdInternal() == CAMERA_ID_BACK || m_parameters->getDualMode() == false) {
        m_postPictureBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC]);
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#ifdef USE_DUAL_CAMERA
    } else if (getCameraIdInternal() == CAMERA_ID_BACK_1 || m_parameters->getDualMode() == false) {
            m_postPictureBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::POST_PIC]);
            type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
#endif
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
#endif

    CLOGI("m_allocBuffers(m_postPictureBuffer) %d x %d, planeCount(%d), planeSize(%d)",
             inputW, inputH, planeCount, planeSize[0]);

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCMode()) {
        minBufferCount = 1;
        maxBufferCount = 1;
        batchSize = 1;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_lensDCBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize,
                             type, allocMode, false, true);

        CLOGI("m_allocBuffers(m_lensDCBufferMgr) %d x %d, planeCount(%d), planeSize(%d)",
                 inputW, inputH, planeCount, planeSize[0]);
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (getCameraIdInternal() == CAMERA_ID_FRONT && m_STRCapturePluginHandle != NULL) {
        minBufferCount = 1;
        maxBufferCount = 1;
        batchSize = 1;
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        ret = m_allocBuffers(m_strCaptureBufferMgr, planeCount, planeSize, bytesPerLine,
                             minBufferCount, maxBufferCount, batchSize,
                             type, allocMode, false, true);

        CLOGI("[STR_CAPTURE] m_allocBuffers(m_strCaptureBufferMgr) %d x %d, planeCount(%d), planeSize(%d)",
                 inputW, inputH, planeCount, planeSize[0]);
    }
#endif

#ifdef USE_MSC_CAPTURE
    planeCount  = 1;
    batchSize = m_parameters->getBatchSize(PIPE_GSC_REPROCESSING3);
    maxBufferCount = 1;
    type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(m_mscBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, maxBufferCount, batchSize,
                         type, allocMode, false, true);
    if (ret < 0) {
        CLOGE("m_mscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
             maxBufferCount, maxBufferCount);
        return ret;
    }

    CLOGI("m_allocBuffers(m_mscBufferMgr) %d x %d, planeCount(%d), planeSize(%d)",
             inputW, inputH, planeCount, planeSize[0]);
#endif

    if (m_parameters->getSamsungCamera() == true) {
        int thumbnailW = 0, thumbnailH = 0;
        m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);
        if (thumbnailW > 0 && thumbnailH > 0) {
            planeSize[0] = FRAME_SIZE(HAL_PIXEL_FORMAT_RGBA_8888, thumbnailW, thumbnailH);
            planeCount  = 1;
            batchSize = m_parameters->getBatchSize(PIPE_GSC_REPROCESSING3);
            maxBufferCount = NUM_THUMBNAIL_POSTVIEW_BUFFERS;
            type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

            ret = m_allocBuffers(m_thumbnailGscBufferMgr, planeCount, planeSize, bytesPerLine,
                                 maxBufferCount, maxBufferCount, batchSize,
                                 type, allocMode, false, true);
            if (ret < 0) {
                CLOGE("m_thumbnailGscBufferMgr m_allocBuffers(minBufferCount=%d, maxBufferCount=%d) fail",
                     maxBufferCount, maxBufferCount);
                return ret;
            }

            CLOGI("m_allocBuffers(m_thumbnailGscBuffer) %d x %d, planeCount(%d), planeSize(%d)",
                     thumbnailW, thumbnailH, planeCount, planeSize[0]);

        }
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_releaseVendorBuffers(void)
{
    CLOGI("release buffer");

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

#ifdef SAMSUNG_STR_CAPTURE
    if (m_strCaptureBufferMgr != NULL) {
        m_strCaptureBufferMgr->deinit();
    }
#endif

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
    releaseBufferIndex = releaseHandle->data[2];
#ifdef TIME_LOGGER_RECORDING_ENABLE
    TIME_LOGGER_UPDATE(m_cameraId, releaseBufferIndex, PIPE_MCSC2, INTERVAL, RECORDING_RELEASE_FRAME, true);
    TIME_LOGGER_UPDATE(m_cameraId, releaseBufferIndex, PIPE_MCSC2, DURATION, RECORDING_RELEASE_FRAME, true);
#endif
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
#ifdef SAMSUNG_HYPER_MOTION
        if (m_parameters != NULL) {
            if (m_parameters->getHyperMotionMode() == true) {
                m_putBuffers(m_hyperMotion_BufferMgr, releaseBufferIndex);
            }
        }
#endif
#ifdef SUPPORT_SW_VDIS
        int cameraId = getCameraIdInternal();
		if (m_swVDIS_Mode && (m_swVDIS_ModeDual == false || (m_swVDIS_ModeDual == true && cameraId != CAMERA_ID_BACK_1))) {
            m_putBuffers(m_swVDIS_BufferMgr, releaseBufferIndex);
        }
#endif
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            m_releaseRecordingBuffer(releaseBufferIndex);
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

#ifdef TIME_LOGGER_RECORDING_ENABLE
    TIME_LOGGER_UPDATE(m_cameraId, releaseBufferIndex, PIPE_MCSC2, DURATION, RECORDING_RELEASE_FRAME, false);
#endif
    return;
}

bool ExynosCamera::m_monitorThreadFunc(void)
{
    CLOGV("");

    int *threadState;
    struct timeval dqTime;
    uint64_t *timeInterval;
    int *countRenew;
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

    if (m_parameters->getSamsungCamera() && ((*threadState == ERROR_POLLING_DETECTED) || (*countRenew > ERROR_DQ_BLOCKED_COUNT))
#ifdef USE_DUAL_CAMERA
            && !(m_parameters->getDualCameraMode() == true &&
                (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST ||
                m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR ||
                m_parameters->getFlagForceSwitchingOnly() == true))
#endif
       ) {
#ifdef ERROR_ESD_STATE_COUNT
        m_esdErrorCount++;
        CLOGD("ESD Detected. threadState(%d) *countRenew(%d) esdErrorCount(%d)", *threadState, *countRenew, m_esdErrorCount);
#else
        CLOGD("ESD Detected. threadState(%d) *countRenew(%d)", *threadState, *countRenew);
#endif

#ifdef ERROR_ESD_STATE_COUNT
        if (m_esdErrorCount == 1) {
#endif
        m_dump();

#ifdef SAMSUNG_TN_FEATURE
        if (m_recordingEnabled == true
          && m_parameters->msgTypeEnabled(CAMERA_MSG_VIDEO_FRAME)
          && m_recordingCallbackHeap != NULL
          && m_callbackCookie != NULL) {

          CLOGD("Timestamp callback with CAMERA_MSG_ERROR start");

          m_dataCbTimestamp(
              0,
              CAMERA_MSG_ERROR | CAMERA_MSG_VIDEO_FRAME,
              m_recordingCallbackHeap,
              0,
              m_callbackCookie);

          CLOGD("Timestamp callback with CAMERA_MSG_ERROR end");
        }
#endif

        ret = m_previewFrameFactory->setControl(V4L2_CID_IS_CAMERA_TYPE, IS_COLD_RESET, PIPE_3AA);
        if (ret < 0) {
            CLOGE("PIPE_%d V4L2_CID_IS_CAMERA_TYPE fail, ret(%d)", PIPE_3AA, ret);
        }

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

#ifdef ERROR_ESD_STATE_COUNT
        } else {
            if (*countRenew > ERROR_DQ_BLOCKED_COUNT * 5) {
                android_printAssert(NULL, LOG_TAG, "assert, An ESD error cocurred consecutively");
                return false;
            }
        }
#else
        return false;
#endif
    } else {
#ifdef ERROR_ESD_STATE_COUNT
        m_esdErrorCount = 0;
#endif
        CLOGV("(%d)", *threadState);
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

#ifdef USE_DUAL_CAMERA
    if (!(m_parameters->getDualCameraMode() == true &&
             (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST ||
             m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR)))
#endif
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

#ifdef USE_DUAL_CAMERA
    /* skip af control in specific sync mode per camera */
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_parameters->getDualCameraMode() == true &&
            /* master cam / switch mode */
            ((m_cameraId == masterCameraId &&
              m_parameters->getDualCameraSyncType() == SYNC_TYPE_SWITCH) ||
            /* slave cam / not switch mode */
             (m_cameraId == slaveCameraId &&
              m_parameters->getDualCameraSyncType() == SYNC_TYPE_BYPASS)))
            return false;
#endif

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
#ifdef SAMSUNG_TN_FEATURE
            /* if inactive detected, tell it */
            if (m_parameters->getSamsungCamera() == true && focusMode == FOCUS_MODE_CONTINUOUS_PICTURE) {
                if (m_exynosCameraActivityControl->getCAFResult() == FOCUS_RESULT_CANCEL) {
                    afFinalResult = FOCUS_RESULT_CANCEL;
                }
            }
#endif
#ifdef SAMSUNG_DUAL_SOLUTION
            if (m_parameters->getDualCameraMode() == true &&
                m_parameters->getDualCameraSyncType() == SYNC_TYPE_SYNC) {
                CLOGD("[Fusion] AF done!!(%d)", afFinalResult);
                int state =  m_parameters->checkDualAfState(getCameraIdInternal(), afFinalResult);
                if (state != -1) {
                    CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)", state, focusMode);
                    m_autoFocusLock.unlock();
                    m_notifyCb(CAMERA_MSG_FOCUS, state, 0, m_callbackCookie);
                    m_autoFocusLock.lock();
                    m_flagAFDone = true;
                }
            }else
#endif
            {
                CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)", afFinalResult, focusMode);
                m_autoFocusLock.unlock();
                m_notifyCb(CAMERA_MSG_FOCUS, afFinalResult, 0, m_callbackCookie);
                m_autoFocusLock.lock();
                m_flagAFDone = true;
            }
        } else {
            CLOGD("m_notifyCb is NULL mode(%d)", focusMode);
        }
    } else {
        CLOGV("autoFocus canceled, no callback !!");
    }

    autoFocusMgr->displayAFStatus();

    m_autoFocusRunning = false;

    CLOGV("exiting with no error");

#ifdef SAMSUNG_TN_FEATURE
    if (focusMode == FOCUS_MODE_CONTINUOUS_PICTURE || focusMode == FOCUS_MODE_CONTINUOUS_VIDEO) {
        bool prev_afInMotion = autoFocusMgr->getAfInMotionResult();
        bool afInMotion = false;

        autoFocusMgr->setAfInMotionResult(afInMotion);

        if (m_notifyCb != NULL && m_parameters->msgTypeEnabled(CAMERA_MSG_FOCUS_MOVE)
            && (prev_afInMotion != afInMotion)) {
            CLOGD("CAMERA_MSG_FOCUS_MOVE(%d) mode(%d)",
                afInMotion, m_parameters->getFocusMode());
            m_autoFocusLock.unlock();
            m_notifyCb(CAMERA_MSG_FOCUS_MOVE, afInMotion, 0, m_callbackCookie);
            m_autoFocusLock.lock();
        }
    }
#endif

done:
    m_autoFocusLock.unlock();

    CLOGI("end");

    return false;
}

bool ExynosCamera::m_autoFocusContinousThreadFunc(void)
{
    int ret = 0;
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
#ifdef USE_DUAL_CAMERA
        if (!(m_parameters->getDualCameraMode() == true &&
             m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR))
#endif
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

#ifdef USE_DUAL_CAMERA
    /* skip af control in specific sync mode per camera */
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_parameters->getDualCameraMode() == true &&
            /* master cam / switch mode */
            ((m_cameraId == masterCameraId &&
              m_parameters->getDualCameraSyncType() == SYNC_TYPE_SWITCH) ||
            /* slave cam / not switch mode */
             (m_cameraId == slaveCameraId &&
              m_parameters->getDualCameraSyncType() == SYNC_TYPE_BYPASS)))
            return true;
#endif

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
#ifdef SAMSUNG_DUAL_SOLUTION
                if (!(m_parameters->getDualCameraMode() == true &&
                    m_parameters->getDualCameraSyncType() == SYNC_TYPE_SYNC &&
                    m_cameraId == slaveCameraId))
#endif
                {
                    CLOGD("CAMERA_MSG_FOCUS(%d) mode(%d)",
                            afResult, m_parameters->getFocusMode());
                    m_notifyCb(CAMERA_MSG_FOCUS, afResult, 0, m_callbackCookie);
                }
                if (afstatus == FOCUS_RESULT_FAIL || afstatus == FOCUS_RESULT_SUCCESS)
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
        if (m_parameters->isFlite3aaOtf() == true)
            bufMgrList[0] = NULL;
        else
            bufMgrList[0] = &m_3aaBufferMgr;

#ifdef DEBUG_RAWDUMP
        bufMgrList[1] = (m_parameters->getUsePureBayerReprocessing() == true) ? &m_bayerBufferMgr : &m_fliteBufferMgr;
#else
        bufMgrList[1] = &m_bayerBufferMgr;
#endif
        break;
    case PIPE_VC0:
        bufMgrList[0] = NULL;
        if (m_parameters->getUsePureBayerReprocessing() == false)
            bufMgrList[1] = &m_fliteBufferMgr;
        else
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
#ifdef USE_DUAL_CAMERA
            if (m_parameters->getDualCameraMode() == true)
                bufMgrList[1] = &m_fusionBufferMgr;
            else
#endif
                bufMgrList[1] = &m_scpBufferMgr;
        }
        break;
    case PIPE_DIS:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;

#ifdef USE_DUAL_CAMERA
       if (m_parameters->getDualCameraMode() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif
        break;
    case PIPE_ISPC:
#if 0
    /* deperated these codes, there's no more scc, scp */
    case PIPE_SCC:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccBufferMgr;
        break;
    case PIPE_SCP:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif
        break;
#endif
    case PIPE_MCSC1:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_previewCallbackBufferMgr;
        break;
    case PIPE_VRA:
        bufMgrList[0] = &m_vraBufferMgr;
        bufMgrList[1] = NULL;
        break;
    case PIPE_GSC:
        bufMgrList[0] = &m_scpBufferMgr;
        bufMgrList[1] = &m_scpBufferMgr;

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true)
            bufMgrList[1] = &m_fusionBufferMgr;
#endif

        break;
#ifdef SAMSUNG_MAGICSHOT
    case PIPE_GSC_VIDEO:
        bufMgrList[0] = &m_sccBufferMgr;
        bufMgrList[1] = &m_postPictureBufferMgr;
        break;
#endif
#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_SWITCHING:
        bufMgrList[0] = &m_ispBufferMgr;
        bufMgrList[1] = NULL;
        break;
    case PIPE_SYNC:
        bufMgrList[0] = &m_fusionBufferMgr;
        bufMgrList[1] = NULL;
        break;
    case PIPE_FUSION:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_scpBufferMgr;
        break;
#endif
    case PIPE_TPU1:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
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
        if (m_parameters->getUsePureBayerReprocessing() == true)
            bufMgrList[0] = &m_ispReprocessingBufferMgr;
        else
            bufMgrList[0] = &m_bayerBufferMgr;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
    case PIPE_ISPC_REPROCESSING:
    case PIPE_SCC_REPROCESSING:
    case PIPE_MCSC3_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_REPROCESSING:
        bufMgrList[0] = &m_sccReprocessingBufferMgr;
        bufMgrList[1] = NULL;
        break;
    case PIPE_FUSION_REPROCESSING:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_fusionReprocessingBufferMgr;
        break;
#endif
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
    case PIPE_GSC_REPROCESSING4:
        bufMgrList[0] = NULL;
        bufMgrList[1] = &m_sccReprocessingBufferMgr;
        break;
#ifdef USE_REMOSAIC_CAPTURE
    case PIPE_REMOSAIC_REPROCESSING:
        bufMgrList[0] = &m_bayerBufferMgr;
        bufMgrList[1] = &m_bayerBufferMgr;
        break;
#endif
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

    CLOGD("[DNG]-- IN --");

    bayerBuffer.index = -2;

    /* wait flite */
    CLOGV("wait bayer output");
    ret = m_dngCaptureQ->waitAndPopProcessQ(&bayerBuffer);
    CLOGD("[DNG]bayer output done pipe");
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    shot_ext = (struct camera2_shot_ext *)bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()];
    if (shot_ext != NULL)
        fliteFcount = shot_ext->shot.dm.request.frameCount;
    else
        CLOGE("fliteReprocessingBuffer is null");

    if (m_dngBayerHeap == NULL) {
        CLOGE("DNG bayer heap not allocated");
        goto CLEAN;
    }

    memcpy(m_dngBayerHeap->data, bayerBuffer.addr[0], bayerBuffer.size[0]);
    CLOGD("[DNG] bayer copy complete! (%d bytes)", bayerBuffer.size[0]);

    m_dngCaptureDoneQ->pushProcessQ(&bayerBuffer);

    CLOGD("[DNG]frame count (%d)", fliteFcount);

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

    CLOGD("[DNG]Raw frame(%dx%d) count (%d)",
            sensorMaxW, sensorMaxH, fliteFcount);

    CLOGD("[DNG]Raw Dump start (%s)", filePath);

    bRet = dumpToFile((char *)filePath,
            (char *)m_dngBayerHeap->data,
            sensorMaxW * sensorMaxH * 2);
    if (bRet != true)
        CLOGE("couldn't make a raw file");
    CLOGD("[DNG]Raw Dump end (%s)", filePath);
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

        CLOGD("DNG: RAW callback");
        // Make Callback Buffer
        camera_memory_t *rawCallbackHeap = NULL;
        rawCallbackHeap = m_getDngCallbackHeap((char *)m_dngBayerHeap->data, dngOutSize);
        if (!rawCallbackHeap || rawCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%d) fail", dngOutSize);
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

    CLOGI("");

    if (dngSaveLocation == BURST_SAVE_CALLBACK) {
        CLOGD("burst callback : size (%d)", dngBuf->size[0]);

        dngCallbackHeap = m_getMemoryCb(dngBuf->fd[0], dngBuf->size[0], 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%d) fail", dngBuf->size[0]);
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
        CLOGD("burst callback : size (%d), filePath(%s)",
                 dngBuf->size[0], filePath);

        if(m_FileSaveFunc(filePath, dngBuf) == false) {
            CLOGE(" FILE save ERROR");
            goto done;
        }

        dngCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%s) fail", filePath);
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

    CLOGD("making callback buffer done");

    return dngCallbackHeap;
}

camera_memory_t *ExynosCamera::m_getDngCallbackHeap(char *dngBuf, unsigned int bufSize)
{
    camera_memory_t *dngCallbackHeap = NULL;
    int dngSaveLocation = m_parameters->getDngSaveLocation();

    CLOGI("");

    if (dngSaveLocation == BURST_SAVE_CALLBACK) {
        CLOGD("burst callback : size (%d)", bufSize);

        dngCallbackHeap = m_getMemoryCb(-1, bufSize, 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%d) fail", bufSize);
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
        CLOGD("burst callback : size (%d), filePath(%s)",
                 bufSize, filePath);

        if(m_FileSaveFunc(filePath, dngBuf, bufSize) == false) {
            CLOGE(" FILE save ERROR");
            goto done;
        }

        dngCallbackHeap = m_getMemoryCb(-1, sizeof(filePath), 1, m_callbackCookie);
        if (!dngCallbackHeap || dngCallbackHeap->data == MAP_FAILED) {
            CLOGE("m_getMemoryCb(%s) fail", filePath);
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

    CLOGD("making callback buffer done");

    return dngCallbackHeap;
}
#endif /* SAMSUNG_DNG */

#ifdef RAWDUMP_CAPTURE
bool ExynosCamera::m_RawCaptureDumpThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("[RAWDUMP_CAP]");

    int ret = 0;
    int sensorMaxW, sensorMaxH;
    int sensorMarginW, sensorMarginH;
    bool bRet;
    char filePath[70];
    ExynosCameraBufferManager *bufferMgr = NULL;
    int bayerPipeId = 0;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    unsigned int fliteFcount = 0;
    int dstPos = 0;
    time_t rawtime;
    struct tm *timeinfo;

    ret = m_RawCaptureDumpQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        // TODO: doing exception handling
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

    bayerPipeId = newFrame->getFirstEntity()->getPipeId();

#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    dstPos = m_previewFrameFactory->getNodeType(PIPE_3AC);
    ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer, dstPos);
	if (ret != NO_ERROR) {
		CLOGE("getDstBuffer failed, frame(%d) pipeId(%d) pos(%d)", newFrame->getFrameCount(), bayerPipeId, dstPos);
	}
	ret = m_getBufferManager(PIPE_3AC, &bufferMgr, DST_BUFFER_DIRECTION);

    ret = bufferMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL);
    if (ret < 0) {
        CLOGE("putIndex is %d", bayerBuffer.index);
        bufferMgr->printBufferState();
        bufferMgr->printBufferQState();
    }
#endif

#if defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    dstPos = m_previewFrameFactory->getNodeType(PIPE_VC0);
#else
    if (m_parameters->getUsePureBayerReprocessing() == true)
        dstPos = m_previewFrameFactory->getNodeType(PIPE_VC0);
    else
        dstPos = m_previewFrameFactory->getNodeType(PIPE_3AC);
#endif

    ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer, dstPos);

    if (bayerPipeId == PIPE_3AA &&
        m_parameters->isFlite3aaOtf() == true) {
        ret = m_getBufferManager(PIPE_VC0, &bufferMgr, DST_BUFFER_DIRECTION);
    } else {
        // update for capture meta data.
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            struct camera2_shot_ext temp_shot_ext;
            newFrame->getMetaData(&temp_shot_ext);
            if (m_parameters->setFusionInfo(&temp_shot_ext) != NO_ERROR) {
                CLOGE("DEBUG(%s[%d]):m_parameters->setFusionInfo() fail", __FUNCTION__, __LINE__);
            }
        }
#endif
        {
            ret = m_getBufferManager(bayerPipeId, &bufferMgr, DST_BUFFER_DIRECTION);
        }
    }

    /* Rawdump capture is available in pure bayer only */
    CLOGD("[RAWDUMP_CAP] get frameCount. pureBayerReproc = %d", m_parameters->getUsePureBayerReprocessing());
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        camera2_shot_ext *shot_ext = NULL;
        shot_ext = (camera2_shot_ext *)bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()];
        if (shot_ext != NULL)
            fliteFcount = shot_ext->shot.dm.request.frameCount;
        else
            CLOGE("fliteReprocessingBuffer is null");
    } else {
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()];
        if (shot_stream != NULL)
            fliteFcount = shot_stream->fcount;
        else
            CLOGE("fliteReprocessingBuffer is null");
    }

    memset(filePath, 0, sizeof(filePath));
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    snprintf(filePath, sizeof(filePath), "/data/media/0/Raw%d_%02d%02d%02d_%02d%02d%02d.raw",
        m_cameraId, timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

    /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
    m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);

    CLOGD("[RAWDUMP_CAP]Raw Dump start (%s)", filePath);

    bRet = dumpToFile((char *)filePath,
        bayerBuffer.addr[0],
        sensorMaxW * sensorMaxH * 2);
    if (bRet != true)
        CLOGE("couldn't make a raw file");

    ret = bufferMgr->putBuffer(bayerBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL);
    if (ret < 0) {
        CLOGE("putIndex is %d", bayerBuffer.index);
        bufferMgr->printBufferState();
        bufferMgr->printBufferQState();
    }

CLEAN:

    if (newFrame != NULL) {
        newFrame->frameUnlock();

        ret = m_searchFrameFromList(&m_processList, newFrame->getFrameCount(), inListFrame, &m_processListLock);
        if (ret < 0) {
            CLOGE("searchFrameFromList fail");
        } else {
            if (inListFrame == NULL) {
                CLOGD(" Selected frame(%d) complete, Delete",
                     newFrame->getFrameCount());
            }
            newFrame = NULL;
        }
    }

    return ret;
}
#endif

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

    if (m_parameters->getUsePureBayerReprocessing() == true)
        dstPos = m_previewFrameFactory->getNodeType(PIPE_VC0);
    else
        dstPos = m_previewFrameFactory->getNodeType(PIPE_3AC);

    int funcRetryCount = 0;
    int retryCount = 30; /* 200ms x 30 */
    camera2_shot_ext *shot_ext = NULL;
#ifdef LLS_REPROCESSING
    camera2_stream *shot_stream = NULL;
#endif
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraFrameSelector::result_t result;
    ExynosCameraBuffer *saveBuffer = NULL;
    ExynosCameraBuffer tempBuffer;
#ifdef SAMSUNG_DNG_DIRTY_BAYER
    ExynosCameraBuffer *pureSaveBuffer = NULL;
    ExynosCameraBuffer *purebuffer = NULL;
    ExynosCameraBuffer pureTempBuffer;
    ExynosCameraBuffer pureBayerBuffer;
#endif
    ExynosRect srcRect, dstRect;
    entity_buffer_state_t buffer_state = ENTITY_BUFFER_STATE_ERROR;
#ifdef LLS_REPROCESSING
    unsigned int fliteFcount = 0;
#endif

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    pureBayerBuffer.index = -2;
    purebuffer = &pureBayerBuffer;
#endif

    m_parameters->getPictureBayerCropSize(&srcRect, &dstRect);

BAYER_RETRY:
#ifdef ONE_SECOND_BURST_CAPTURE
    if (m_parameters->getCaptureExposureTime() != 0
        && m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("manual exposure mode. Stop one second burst.");
        m_stopLongExposure = true;
        m_jpegCallbackCounter.clearCount();
        m_stopBurstShot = true;
    }
#endif

    if (m_stopLongExposure == true
        && m_parameters->getCaptureExposureTime() != 0) {
        CLOGD("Emergency stop");
        m_reprocessingCounter.clearCount();
        m_pictureEnabled = false;
        result = ExynosCameraFrameSelector::RESULT_NO_FRAME;
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
    CLOGD("[RAWDUMP_CAP] selectFrames. Push bayerFrame to RawDumpQ and mode off.");
    for (int i = 0; i < 2; i++) {
        result = m_captureSelector->selectFrames(bayerFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, m_previewFrameFactory->getNodeType(PIPE_VC0));

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
            result = m_captureSelector->selectFrames(bayerFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, dstPos);
        }
    } else
#endif
    {
#ifdef FLASHED_LLS_CAPTURE
        if (m_parameters->getLDCaptureMode() == MULTI_SHOT_MODE_FLASHED_LLS
            && m_reprocessingCounter.getCount() == m_parameters->getLDCaptureCount()) {
            CLOGD(" wait preFlash");
            m_exynosCameraActivityControl->waitFlashMainReady();

            if (m_parameters->getOISCaptureModeOn()) {
                ExynosCameraActivitySpecialCapture *sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
                int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;

                CLOGD(" start set captureIntent(%d)",captureIntent);

                /* HACK: For the fast OIS-Capture Response time */
                ret = m_previewFrameFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
                if (ret < 0) {
                    CLOGE("setControl(STILL_CAPTURE_OIS) fail. ret(%d) intent(%d)",
                             ret, captureIntent);
                }

                sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
                m_exynosCameraActivityControl->setOISCaptureMode(true);
            }
        }
#endif
        result = m_captureSelector->selectFrames(bayerFrame, m_reprocessingCounter.getCount(), pipeId, isSrc, retryCount, dstPos);
    }
#ifdef SAMSUNG_QUICKSHOT
    m_quickShotStart = false;
#endif
#endif

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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true &&
            frame->getFrameType() == FRAME_TYPE_INTERNAL) {
        frame = NULL;
        return ExynosCameraFrameSelector::RESULT_SKIP_FRAME;
    }
#endif

#ifdef LLS_REPROCESSING
    m_parameters->setSetfileYuvRange();
#endif

#if !defined(DEBUG_RAWDUMP)
#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (bayerFrame->getRequest(PIPE_VC0) == true) {
        ret = bayerFrame->getDstBuffer(pipeId, purebuffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
        if (ret < 0) {
            CLOGE(" getDstBuffer(pipeId(%d) dstPos(%d)) fail, ret(%d)", pipeId, m_previewFrameFactory->getNodeType(PIPE_VC0), ret);
            goto CLEAN;
        }

        if (m_parameters->getDNGCaptureModeOn() == false) {
            if (purebuffer->index != -2 && m_fliteBufferMgr != NULL) {
                m_putBuffers(m_fliteBufferMgr, purebuffer->index);
                CLOGD("[DNG] m_putBuffers for m_fliteBufferMgr");
            }
        }
    }
#endif
#endif

#if defined(RAWDUMP_CAPTURE) && defined(DEBUG_RAWDUMP_DIRTY_BAYER)
    if (bayerFrame->getRequest(PIPE_VC0) == true) {
        ret = bayerFrame->getDstBuffer(pipeId, purebuffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
        if (ret < 0) {
            CLOGE(" getDstBuffer(pipeId(%d) dstPos(%d)) fail, ret(%d)", pipeId, m_previewFrameFactory->getNodeType(PIPE_VC0), ret);
            goto CLEAN;
        }

        if (purebuffer->index != -2 && m_fliteBufferMgr != NULL) {
            m_putBuffers(m_fliteBufferMgr, purebuffer->index);
            CLOGD("[RAW] m_putBuffers for m_fliteBufferMgr");
        }
    }
#endif

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

        if (!(m_parameters->getDepthCallbackOnCapture() && m_nv21CaptureEnabled)
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

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (bayerFrame->getRequest(PIPE_VC0) == true && purebuffer->index != -2 && m_fliteBufferMgr != NULL) {
            m_putBuffers(m_fliteBufferMgr, purebuffer->index);
            CLOGD("[DNG] m_putBuffers for m_fliteBufferMgr");
        }
#endif

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

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (bayerFrame->getRequest(PIPE_VC0) == true && purebuffer->index != -2 && m_fliteBufferMgr != NULL) {
            m_putBuffers(m_fliteBufferMgr, purebuffer->index);
            CLOGD("[DNG] m_putBuffers for m_fliteBufferMgr");
        }
#endif

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

#ifdef SAMSUNG_DNG_DIRTY_BAYER
            if (m_parameters->getDNGCaptureModeOn() == true) {
                pureSaveBuffer = purebuffer;
                purebuffer = &pureTempBuffer;
                pureTempBuffer.index = -2;
            }
#endif
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
#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (m_parameters->getDNGCaptureModeOn() == true)
            ret = addBayerBuffer(purebuffer, pureSaveBuffer, &dstRect);
#endif
        if (ret < 0) {
            CLOGE("addBayerBufferPacked() fail");
        }

        if (buffer->index != -2 && m_bayerBufferMgr != NULL)
            m_putBuffers(m_bayerBufferMgr, buffer->index);

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (m_parameters->getDNGCaptureModeOn() == true && purebuffer->index != -2 && m_fliteBufferMgr != NULL) {
            m_putBuffers(m_fliteBufferMgr, purebuffer->index);
            CLOGD("[DNG] m_putBuffers for m_fliteBufferMgr %d %d", purebuffer->index, m_longExposureRemainCount);
        }
#endif

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
#ifdef USE_LIB_ION_LEGACY
            ion_sync_fd(m_ionClient, buffer->fd[0]);
#else
            exynos_ion_sync_fd(m_ionClient, buffer->fd[0]);
#endif
#endif

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (m_parameters->getDNGCaptureModeOn() == true) {
            purebuffer = pureSaveBuffer;
            pureSaveBuffer = NULL;

#ifdef CAMERA_ADD_BAYER_ENABLE
            if (m_ionClient >= 0)
#ifdef USE_LIB_ION_LEGACY
                ion_sync_fd(m_ionClient, purebuffer->fd[0]);
#else
                exynos_ion_sync_fd(m_ionClient, purebuffer->fd[0]);
#endif
#endif
        }
#endif
    }

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("[DNG] Push pure bayer to DNG Thread %d", purebuffer->index);
        m_dngFrameNumber = getMetaDmRequestFrameCount(updateDmShot);
        m_DNGCaptureThread->run();
        m_dngCaptureQ->pushProcessQ(purebuffer);
    }
#if 0
    {
        char filePath[70];
        int width, height = 0;
        m_parameters->getHwSensorSize(&width, &height);
        CLOGD("[DNG] getHwSensorSize(%d x %d), /data/media/0/bayer_%d.raw", width, height, buffer->index);
        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/bayer_%d.raw", buffer->index);
        dumpToFile((char *)filePath, buffer->addr[0], width * height * 2);
    }
#endif
#endif

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

#ifdef LLS_REPROCESSING
    if(m_captureSelector->getIsLastFrame() == false) {
        if (m_captureSelector->getIsConvertingMeta() == false) {
            camera2_shot_ext *shot_ext = NULL;
            shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->getMetaPlaneIndex()];
            if (shot_ext != NULL)
                fliteFcount = shot_ext->shot.dm.request.frameCount;
        } else {
            camera2_stream *shot_stream = NULL;
            shot_stream = (camera2_stream *)buffer->addr[buffer->getMetaPlaneIndex()];
            if (shot_stream != NULL)
                fliteFcount = shot_stream->fcount;
        }

        CLOGD(" LLS Capture ...ing(hal : %d / driver : %d)",
            bayerFrame->getFrameCount(), fliteFcount);
        return result;
    }
#endif

CLEAN:
    if (saveBuffer != NULL
        && saveBuffer->index != -2
        && m_bayerBufferMgr != NULL) {
        m_putBuffers(m_bayerBufferMgr, saveBuffer->index);
        saveBuffer->index = -2;
    }
#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (m_parameters->getDNGCaptureModeOn() == true) {
        if (pureSaveBuffer != NULL
            && pureSaveBuffer->index != -2
            && m_fliteBufferMgr != NULL) {
            CLOGD("[DNG] [PUTBUFFER]");
            m_putBuffers(m_fliteBufferMgr, pureSaveBuffer->index);
            pureSaveBuffer->index = -2;
        }
    }
#endif

#ifdef LLS_REPROCESSING
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS
        && m_captureSelector->getIsLastFrame() == true) {
        m_captureSelector->resetFrameCount();
        m_parameters->setLLSCaptureCount(0);
    }
#endif

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

    if (m_scenario != SCENARIO_SECURE) {
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
    } else {
        /* set initial value for Secure Camera*/
        m_shutterSpeed = m_parameters->getShutterSpeed();
        m_gain = m_parameters->getGain();
        m_irLedWidth = m_parameters->getIrLedWidth();
        m_irLedDelay = m_parameters->getIrLedDelay();
        m_irLedCurrent = m_parameters->getIrLedCurrent();
        m_irLedOnTime = m_parameters->getIrLedOnTime();
        m_visionFps = 30;

        ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, m_visionFps, pipeId);
        if (ret < 0)
            CLOGE("FLITE setControl fail, ret(%d)", ret);

        ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_SHUTTER, m_shutterSpeed, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_shutterSpeed(%d)", m_shutterSpeed);
        }

        ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_GAIN, m_gain, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_gain(%d)", m_gain);
        }

        ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_WIDTH, m_irLedWidth, pipeId);
          if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedWidth(%d)", m_irLedWidth);
        }

        ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_DELAY, m_irLedDelay, pipeId);
         if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedDelay(%d)", m_irLedDelay);
        }

        ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_CURRENT, m_irLedCurrent, pipeId);
          if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedCurrent(%d)", m_irLedCurrent);
        }

        ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_ONTIME, m_irLedOnTime, pipeId);
           if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedOnTime(%d)", m_irLedOnTime);
        }
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

    CLOGI("IN");

    ret = m_visionFrameFactory->stopPipes();
    if (ret < 0) {
        CLOGE("stopPipe fail");
        /* stopPipe fail but all node closed */
        /* return ret; */
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
    int batchSize = 1;

    int planeCount  = 2;
    int maxBufferCount = 1;

    exynos_camera_buffer_type_t type;

    planeCount      = 2;
    planeSize[0]    = 32 * 64 * 2;
    batchSize       = 1;
    if (m_scenario == SCENARIO_SECURE) {
        maxBufferCount = m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE];
    } else {
        maxBufferCount = m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER];
    }
    type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

    ret = m_allocBuffers(m_3aaBufferMgr, planeCount, planeSize, bytesPerLine,
                         maxBufferCount, batchSize, true, false);
    if (ret != NO_ERROR) {
        CLOGE("m_3aaBufferMgr m_allocBuffers(bufferCount=%d) fail",
            maxBufferCount);
        return ret;
    }

    if (m_scenario == SCENARIO_SECURE) {
        maxBufferCount = m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE];

        m_bayerBufferMgr->setContigBufCount(m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::SECURE]);
        type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_SECURE_TYPE;

        planeSize[0] = SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT;

#ifdef SUPPORT_IRIS_PREVIEW_CALLBACK
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                                maxBufferCount, maxBufferCount, 1 /* BathSize */, type, true, true);
#else
        ret = m_allocBuffers(m_bayerBufferMgr, planeCount, planeSize, bytesPerLine,
                                maxBufferCount, maxBufferCount, 1 /* BathSize */, type, true, false);
#endif
        if (ret < 0) {
            CLOGE("bayerBuffer m_allocBuffers(bufferCount=%d) fail",
                maxBufferCount);
            return ret;
        }
    } else {
        maxBufferCount = m_exynosconfig->current->reservedBufInfo.num_buffers[CONFIG_RESERVED::BAYER];
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

    if (m_scenario == SCENARIO_SECURE) {
        bufferCount = RESERVED_NUM_SECURE_BUFFERS;
        planeSize[0] = SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT;
    } else {
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

    int frameSkipCount = 0;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t handleFrame = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    ExynosCameraBuffer buffer;
    int pipeID = 0;
    /* to handle the high speed frame rate */
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
                    if (m_scenario == SCENARIO_SECURE) {
                        dumpToFile(filePath, (char *)bayerBuffer.addr[0], SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT);
                    } else {
                        dumpToFile(filePath, (char *)bayerBuffer.addr[0], VISION_WIDTH * VISION_HEIGHT);
                    }
                    dumpIndex ++;
                    if (dumpIndex > 4)
                        dumpIndex = 0;
#endif
                    if (frameSkipCount > 0) {
                        CLOGD("frameSkipCount(%d)", frameSkipCount);
                    } else {
                        /* callback */
#ifdef SUPPORT_IRIS_PREVIEW_CALLBACK
                        if (m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME))
#else
                        if (m_scenario != SCENARIO_SECURE && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_FRAME))
#endif
                        {
                            if (m_displayPreviewToggle) {
                                ExynosCameraBuffer previewCbBuffer;
                                camera_memory_t *previewCallbackHeap = NULL;
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
#ifdef SAMSUNG_TN_FEATURE
#ifndef SUPPORT_IRIS_PREVIEW_CALLBACK
                        if (m_scenario == SCENARIO_SECURE && m_parameters->msgTypeEnabled(CAMERA_MSG_IRIS_DATA)) {
                            if (m_displayPreviewToggle) {
                                camera_memory_t *fdCallbackData = NULL;
#if 0
                                char filePath[50];
                                if (dumpIndex > 5 && dumpIndex <10) {
                                    snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Camera/iris%02d.raw", dumpIndex);
                                    CLOGE("iris dump %s", filePath);

                                    dumpToFile(filePath, (char *)bayerBuffer.addr[0], SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT);
                    	        }
                                dumpIndex ++;
#endif
                                fdCallbackData = m_getMemoryCb(-1, sizeof(CameraBufferFD), 1, m_callbackCookie);
                                if (fdCallbackData == NULL) {
                                    CLOGE("m_getMemoryCb fail");
                                    return INVALID_OPERATION;
                                }

                                ((CameraBufferFD *)fdCallbackData->data)->magic = 0x00FD42FD;
                                ((CameraBufferFD *)fdCallbackData->data)->fd = bayerBuffer.fd[0];

                                setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
                                m_dataCb(CAMERA_MSG_IRIS_DATA, fdCallbackData, 0, NULL, m_callbackCookie);
                                clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);

                                fdCallbackData->release(fdCallbackData);
                            }
                       }
#endif
#endif
                        if (m_scenario == SCENARIO_SECURE) {
                            if (m_visionFps == 30) {
                                m_displayPreviewToggle = (m_displayPreviewToggle + 1) % 2;
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

    if (m_scenario != SCENARIO_SECURE) {
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
    } else {
        shutterSpeed = m_parameters->getShutterSpeed();
        if (m_shutterSpeed != shutterSpeed) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_SHUTTER, shutterSpeed, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_shutterSpeed = shutterSpeed;
            CLOGD("(%d)(%d)", m_shutterSpeed, shutterSpeed);
        }

        gain = m_parameters->getGain();
        if (m_gain != gain) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_SENSOR_SET_GAIN, gain, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_gain = gain;
            CLOGD("(%d)(%d)", m_gain, gain);
        }

        irLedWidth  = m_parameters->getIrLedWidth();
        if (m_irLedWidth != irLedWidth) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_WIDTH, irLedWidth, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_irLedWidth = irLedWidth;
            CLOGD("(%d)(%d)", m_irLedWidth, irLedWidth);
        }

        irLedDelay = m_parameters->getIrLedDelay();
        if (m_irLedDelay != irLedDelay) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_DELAY, irLedDelay, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_irLedDelay = irLedDelay;
            CLOGD("(%d)(%d)", m_irLedDelay, irLedDelay);
        }

        irLedCurrent = m_parameters->getIrLedCurrent();
        if (m_irLedCurrent != irLedCurrent) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_CURRENT, irLedCurrent, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_irLedCurrent = irLedCurrent;
            CLOGD("(%d)(%d)", m_irLedCurrent, irLedCurrent);
        }

        irLedOnTime = m_parameters->getIrLedOnTime();
        if (m_irLedOnTime != irLedOnTime) {
            ret = m_visionFrameFactory->setControl(V4L2_CID_IRLED_SET_ONTIME, irLedOnTime, PIPE_FLITE);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            m_irLedOnTime = irLedOnTime;
            CLOGD("(%d)(%d)", m_irLedOnTime, irLedOnTime);
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
                CLOGD(" companionThread starts");
            } else {
                CLOGE("failed the m_companionThread.");
            }
        } else {
            CLOGW("m_companionNode != NULL. so, it already running");
        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        m_eepromThread = new mainCameraThread(this, &ExynosCamera::m_eepromThreadFunc,
                                                  "cameraeepromThread", PRIORITY_URGENT_DISPLAY);
        if (m_eepromThread != NULL) {
            m_eepromThread->run();
            CLOGD(" eepromThread starts");
        } else {
            CLOGE("failed the m_eepromThread");
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
            CLOGI(" wait m_companionThread");
            m_companionThread->requestExitAndWait();
            CLOGI(" wait m_companionThread end");
        } else {
            CLOGI(" m_companionThread is NULL");
        }

        if (m_companionNode != NULL) {
            ExynosCameraDurationTimer timer;

            timer.start();

            if (m_companionNode->close() != NO_ERROR) {
                CLOGE("close fail");
            }
            delete m_companionNode;
            m_companionNode = NULL;

            CLOGD("Companion Node(%d) closed", MAIN_CAMERA_COMPANION_NUM);

            timer.stop();
            CLOGD("duration time(%5d msec)", (int)timer.durationMsecs());

        }
    }
#endif

#if defined(SAMSUNG_EEPROM)
    if ((m_use_companion == false) && (isEEprom(getCameraIdInternal()) == true)) {
        if (m_eepromThread != NULL) {
            CLOGI(" wait m_eepromThread");
            m_eepromThread->requestExitAndWait();
        } else {
            CLOGI(" m_eepromThread is NULL");
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
#ifdef TIME_LOGGER_LAUNCH_ENABLE
    TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_COMPANION_THREAD, true);
#endif
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int loop = false;
    int ret = 0;

    m_timer.start();

    m_companionNode = new ExynosCameraNode();

    ret = m_companionNode->create("companion", m_cameraId);
    if (ret < 0) {
        CLOGE(" Companion Node create fail, ret(%d)", ret);
    }

    ret = m_companionNode->open(MAIN_CAMERA_COMPANION_NUM);
    if (ret < 0) {
        CLOGE(" Companion Node open fail, ret(%d)", ret);
    }
    CLOGD("Companion Node(%d) opened running)", MAIN_CAMERA_COMPANION_NUM);

    ret = m_companionNode->setInput(m_getSensorId(m_cameraId));
    if (ret < 0) {
        CLOGE(" Companion Node s_input fail, ret(%d)", ret);
    }
#ifdef TIME_LOGGER_LAUNCH_ENABLE
    TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 1, DURATION, LAUNCH_SUB_COMPANION_THREAD, false);
#endif
    CLOGD("Companion Node(%d) s_input)", MAIN_CAMERA_COMPANION_NUM);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

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
            CLOGI("m_companionThread is NULL");
        }
    }
#endif

    timer.stop();
    CLOGD("companionThread joined, waiting time : duration time(%5d msec)", (int)timer.durationMsecs());

    return NO_ERROR;
}

#ifdef SAMSUNG_QUICK_SWITCH
bool ExynosCamera::m_preStartPictureInternalThreadFunc(void)
{
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int ret = 0;

    m_timer.start();
    if (m_parameters->isReprocessing() == true) {

        for (int i = 0; i < m_getNumOfReprocessingInstance(); i++) {
            if (m_pReprocessingFrameFactories[i]->isCreated() == false) {
                ret = m_pReprocessingFrameFactories[i]->create();
                if (ret != NO_ERROR) {
                    m_timer.stop();
                    durationTime = m_timer.durationMsecs();
                    CLOGE("m_reprocessingFrameFactory->create() failed");
                    return false;
                }
            }
        }

        m_pictureFrameFactory = m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PROCESSED];
        CLOGD("FrameFactory(pictureFrameFactory) created");
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD(" duration time(%5d msec)", (int)m_timer.durationMsecs());

    return ret;
}
#endif

#ifdef SAMSUNG_UNIPLUGIN
bool ExynosCamera::m_uniPluginThreadFunc(void)
{
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

#ifdef SAMSUNG_DUAL_SOLUTION
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        if (m_previewSolutionHandle == NULL) {
            m_previewSolutionHandle = uni_plugin_load(DUAL_PREVIEW_PLUGIN_NAME);
            if (m_previewSolutionHandle == NULL) {
                CLOGE("[DUAL_PREVIEW] : DUAL_PREVIEW plugin load failed!!");
            }
        }
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (m_captureSolutionHandle == NULL) {
            m_captureSolutionHandle = uni_plugin_load(DUAL_CAPTURE_PLUGIN_NAME);
            if (m_captureSolutionHandle == NULL) {
                CLOGE("[DUAL_CAPTURE] : DUAL_CAPTURE plugin load failed!!");
            }
        }
#endif
        m_fusionPreviewWrapper->m_setSolutionHandle(m_previewSolutionHandle, m_captureSolutionHandle);
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        m_fusionCaptureWrapper->m_setSolutionHandle(m_previewSolutionHandle, m_captureSolutionHandle);
#endif
    }
#endif

#ifdef SAMSUNG_LLV
#ifdef SAMSUNG_LLV_TUNING
    m_LLVpowerLevel = UNI_PLUGIN_POWER_CONTROL_OFF;
    m_checkLLVtuning();
#endif
    m_LLVstatus = LLV_UNINIT;
    m_LLVpluginHandle = uni_plugin_load(LLV_PLUGIN_NAME);
    if (m_LLVpluginHandle == NULL) {
        CLOGE("[LLV]: LLV plugin load failed!!");
    }
#endif

#ifdef SAMSUNG_LENS_DC
    m_DCpluginHandle = NULL;
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        m_DCpluginHandle = uni_plugin_load(LDC_PLUGIN_NAME);
        if (m_DCpluginHandle == NULL) {
            CLOGE("[DC]: DC plugin load failed!!");
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
            CLOGE("[OBTR]  Object Tracking plugin load failed!!");
        }
    } else {
        CLOGD("[OBTR]  Object Tracking plugin no load for front camera");
    }
#endif
#ifdef SAMSUNG_LBP
    m_LBPindex = 0;
    m_LBPCount = 0;
    m_isLBPlux = false;
    m_isLBPon = false;
    m_LBPpluginHandle = uni_plugin_load(BESTPHOTO_PLUGIN_NAME);
    if (m_LBPpluginHandle == NULL) {
        CLOGE("[LBP]:Bestphoto plugin load failed!!");
    }
#endif
#ifdef SAMSUNG_JQ
    m_isJpegQtableOn = false;
    m_JQpluginHandle = uni_plugin_load(JPEG_QTABLE_PLUGIN_NAME);
    if (m_JQpluginHandle == NULL) {
        CLOGE("[JQ]:JpegQtable plugin load failed!!");
    }
#endif
#ifdef SAMSUNG_BD
    m_BDpluginHandle = uni_plugin_load(BLUR_DETECTION_PLUGIN_NAME);
    if (m_BDpluginHandle == NULL) {
        CLOGE("[BD]:BlurDetection plugin load failed!!");
    }
    m_BDstatus = BLUR_DETECTION_IDLE;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
#if defined(SAMSUNG_HIFI_LLS) || defined(FLASHED_LLS_CAPTURE)
#ifdef SAMSUNG_HIFI_LLS
    m_LLSpluginHandle = uni_plugin_load(HIFI_LLS_PLUGIN_NAME);
    if (m_LLSpluginHandle == NULL) {
        CLOGE("[LLS_MBR] HIFI LLS plugin load failed!!");
    }
#else /* FLASHED_LLS_CAPTURE */
    m_LLSpluginHandle = uni_plugin_load(FLASHED_LLS_PLUGIN_NAME);
    if (m_LLSpluginHandle == NULL) {
        CLOGE("[LLS_MBR] FLASHED LLS plugin load failed!!");
    }
#endif
#else
    m_LLSpluginHandle = uni_plugin_load(LLS_PLUGIN_NAME);
    if (m_LLSpluginHandle == NULL) {
        CLOGE("[LLS_MBR] LLS plugin load failed!!");
    }
#endif
#endif
#ifdef SAMSUNG_STR_CAPTURE
    m_STRCapturePluginHandle = NULL;
    if (getCameraIdInternal() == CAMERA_ID_FRONT) {
        m_STRCapturePluginHandle = uni_plugin_load(STR_CAPTURE_PLUGIN_NAME);
        if (m_STRCapturePluginHandle == NULL) {
            CLOGE("[STR_CAPTURE]: STR_CAPTURE plugin load failed!!");
        }
    }
#endif
#ifdef SAMSUNG_STR_PREVIEW
    m_STRPreviewPluginHandle = NULL;
    if (getCameraIdInternal() == CAMERA_ID_FRONT) {
        m_STRPreviewPluginHandle = uni_plugin_load(STR_PREVIEW_PLUGIN_NAME);
        if (m_STRPreviewPluginHandle == NULL) {
            CLOGE("[STR_PRVIEW]: STR_PRVIEW plugin load failed!!");
        }
    }
#endif
#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
    m_FocusPeakingPluginHandle = NULL;
    if (getCameraIdInternal() == CAMERA_ID_BACK) {
        m_FocusPeakingPluginHandle = uni_plugin_load(FOCUS_PEAKING_PLUGIN_NAME);
        if (m_FocusPeakingPluginHandle == NULL) {
            CLOGE("[FocusPeaking]: FocusPeaking plugin load failed!!");
        }
    }
#endif
#endif
#ifdef SAMSUNG_IDDQD
	m_IDDQDPluginHandle = NULL;
	m_IDDQDPluginHandle = uni_plugin_load(IDDQD_PLUGIN_NAME);
	if (m_IDDQDPluginHandle == NULL) {
	    CLOGE("[IDDQD]: IDDQD plugin load failed!!");
	}
#endif
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

    return false;
}
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_sensorListenerThreadFunc(void)
{
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    uint32_t curMinFps = 0;
    uint32_t curMaxFps = 0;

#if 0
    /* skip the sensor listener data in slow motion mode */
    m_parameters->getPreviewFpsRange(&curMinFps, &curMaxFps);
    if(curMinFps > 60 && curMaxFps > 60)
        goto skip_sensor_listener;
#endif

    if (m_parameters->getVtMode() == 0
#ifdef SAMSUNG_QUICK_SWITCH
        && m_parameters->getQuickSwitchCmd() != QUICK_SWITCH_CMD_IDLE_TO_STBY
#endif
       ) {
        if (m_parameters->getSamsungCamera() == true) {
            if (getCameraIdInternal() == CAMERA_ID_BACK
#ifdef SAMSUNG_DUAL_SOLUTION
                || (getCameraIdInternal() == CAMERA_ID_BACK_1 &&
                    m_parameters->getDualCameraMode() == false)
#endif
            ) {
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

            if (getCameraIdInternal() == CAMERA_ID_FRONT
#ifdef SAMSUNG_COLOR_IRIS
                && m_parameters->getShotMode() != SHOT_MODE_COLOR_IRIS
#endif
               ) {
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

#ifdef SAMSUNG_GYRO
            if ((getCameraIdInternal() == CAMERA_ID_BACK)
#ifdef SAMSUNG_DUAL_SOLUTION
                || (getCameraIdInternal() == CAMERA_ID_BACK_1 &&
                    m_parameters->getDualCameraMode() == false)
#endif
#ifdef SAMSUNG_GYRO_FRONT
                || (getCameraIdInternal() == CAMERA_ID_FRONT)
#endif
            ) {
                if (m_gyroHandle == NULL) {
                    m_gyroHandle = sensor_listener_load();
                    if (m_gyroHandle != NULL) {
                        if (sensor_listener_enable_sensor(m_gyroHandle,ST_GYROSCOPE, 100 * 1000) < 0) {
                            sensor_listener_unload(&m_gyroHandle);
                        } else {
                            m_parameters->m_setGyroHint(true);
                        }
                    }
                } else {
                    m_parameters->m_setGyroHint(true);
                }
            }
#endif
        }

#ifdef SAMSUNG_ROTATION
        if (m_rotationHandle == NULL) {
            m_rotationHandle = sensor_listener_load();
            if (m_rotationHandle != NULL) {
                if (sensor_listener_enable_sensor(m_rotationHandle, ST_ROTATION, 100 * 1000) < 0) {
                    sensor_listener_unload(&m_rotationHandle);
                }
            }
        }
#endif
    }

skip_sensor_listener:
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

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
            CLOGE("failed to open LLVKEY_%d", i);
        }
        else {
            CLOGD(":succeeded to open LLVKEY_%d", i);
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
        CLOGE("HLV:  FrameBuffer is NULL - Exit");
        return -1;
    }

    int maxNumFocusAreas = m_parameters->getMaxNumFocusAreas();
    if (maxNumFocusAreas <= 0) {
        CLOGE("HLV:  maxNumFocusAreas is <= 0 - Exit");
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

    CLOGV("HLV: uni_plugin_set BUFFER_INFO");
    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_BUFFER_INFO,
                &BufferInfo);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_set fail!");


    CLOGV("HLV: uni_plugin_set FOCUS_INFO");

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
            CLOGW("HLV: Valid Focus Area > 1");

        delete[] objectTrackingArea;
        delete[] objectTrackingWeight;
    } else {
        int FocusAreaWeight = 0;
        ExynosRect2 CurrentFocusArea;
        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        memset(&CurrentFocusArea, 0, sizeof(ExynosRect2));
        autoFocusMgr->getFocusAreas(&CurrentFocusArea, &FocusAreaWeight);

#ifdef SAMSUNG_DUAL_SOLUTION
        if (m_parameters->getDualCameraMode() == true) {
            UniPlugin_DCVOZ_CAMERAPARAM_t *fusionParam;
            fusionParam = m_parameters->getFusionParam();
            if (fusionParam) {
                CurrentFocusArea.x1 += fusionParam->PreviewImageShiftX;
                CurrentFocusArea.y1 += fusionParam->PreviewImageShiftY;
                CurrentFocusArea.x2 += fusionParam->PreviewImageShiftX;
                CurrentFocusArea.y2 += fusionParam->PreviewImageShiftY;
            }
        }
#endif

        focusData[0].FocusROILeft = CurrentFocusArea.x1;
        focusData[0].FocusROIRight = CurrentFocusArea.x2;
        focusData[0].FocusROITop = CurrentFocusArea.y1;
        focusData[0].FocusROIBottom = CurrentFocusArea.y2;
        focusData[0].FocusWeight = FocusAreaWeight;

        CLOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
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

    CLOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
        CurrentFocusArea.x1, CurrentFocusArea.x2,
        CurrentFocusArea.y1, CurrentFocusArea.y2);
#endif

    ret = uni_plugin_set(m_HLV,
                HIGHLIGHT_VIDEO_PLUGIN_NAME,
                UNI_PLUGIN_INDEX_FOCUS_INFO,
                focusData);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_set fail!");

    ret = uni_plugin_process(m_HLV);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_process FAIL");

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
        CLOGD(" need skip frame for ae/awb stable(%d).", data);
        m_parameters->setFrameSkipCount(2);
    }
    m_checkFirstFrameLux = false;
}

status_t ExynosCamera::m_copyMetaFrameToFrame(ExynosCameraFrameSP_sptr_t srcframe, ExynosCameraFrameSP_sptr_t dstframe, bool useDm, bool useUdm)
{
    Mutex::Autolock lock(m_metaCopyLock);
    status_t ret = NO_ERROR;

#ifdef USE_DUAL_CAMERA
    dstframe->setSyncType(srcframe->getSyncType());
#endif

    memset(m_tempshot, 0x00, sizeof(struct camera2_shot_ext));
    if (useDm) {
        ret = srcframe->getDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("getDynamicMeta fail, ret(%d)", ret);

        ret = dstframe->storeDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("storeDynamicMeta fail, ret(%d)", ret);

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && srcframe->getSyncType() == SYNC_TYPE_SYNC) {
            ret = srcframe->getDynamicMeta(m_tempshot, OUTPUT_NODE_2);
            if (ret != NO_ERROR)
                CLOGE("getDynamicMeta fail, ret(%d)", ret);

            ret = dstframe->storeDynamicMeta(m_tempshot, OUTPUT_NODE_2);
            if (ret != NO_ERROR)
                CLOGE("storeDynamicMeta fail, ret(%d)", ret);
        }
#endif
    }

    if (useUdm) {
        ret = srcframe->getUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("getUserDynamicMeta fail, ret(%d)", ret);

        ret = dstframe->storeUserDynamicMeta(m_tempshot);
        if (ret != NO_ERROR)
            CLOGE("storeUserDynamicMeta fail, ret(%d)", ret);

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && srcframe->getSyncType() == SYNC_TYPE_SYNC) {
            ret = srcframe->getUserDynamicMeta(m_tempshot, OUTPUT_NODE_2);
            if (ret != NO_ERROR)
                CLOGE("getDynamicMeta fail, ret(%d)", ret);

            ret = dstframe->storeUserDynamicMeta(m_tempshot, OUTPUT_NODE_2);
            if (ret != NO_ERROR)
                CLOGE("storeDynamicMeta fail, ret(%d)", ret);
        }
#endif
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
    int32_t reprocessingBayerMode = m_parameters->updateReprocessingBayerMode();
    bool zoomWithScaler = false;
    int pipeId = PIPE_FLITE;
    bool flagSensorStandby = false;
    m_displayPreviewToggle = 0;
    m_fdCallbackToggle = 0;

    zoomWithScaler = m_parameters->getSupportedZoomPreviewWIthScaler();

    minBayerFrameNum = m_exynosconfig->current->bufInfo.init_bayer_buffers;

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

    ExynosCameraBufferManager *fliteBufferManager[MAX_NODE];
    ExynosCameraBufferManager *taaBufferManager[MAX_NODE];
    ExynosCameraBufferManager *ispBufferManager[MAX_NODE];
    ExynosCameraBufferManager *disBufferManager[MAX_NODE];
    ExynosCameraBufferManager *mcscBufferManager[MAX_NODE];
#ifdef USE_DUAL_CAMERA
    ExynosCameraBufferManager *syncBufferManager[MAX_NODE];
    ExynosCameraBufferManager *fusionBufferManager[MAX_NODE];
#endif
    ExynosCameraBufferManager *vraBufferManager[MAX_NODE];
    ExynosCameraBufferManager **tempBufferManager;

    for (int i = 0; i < MAX_NODE; i++) {
        fliteBufferManager[i] = NULL;
        taaBufferManager[i] = NULL;
        ispBufferManager[i] = NULL;
        disBufferManager[i] = NULL;
        mcscBufferManager[i] = NULL;
#ifdef USE_DUAL_CAMERA
        syncBufferManager[i] = NULL;
        fusionBufferManager[i] = NULL;
#endif
        vraBufferManager[i] = NULL;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        m_parameters->clearDualCameraInformation();

        /* UHD recording */
        int videoW = 0, videoH = 0;
        m_parameters->getVideoSize(&videoW, &videoH);
        if ((videoW == 3840 && videoH == 2160) &&
                m_parameters->getRecordingHint() == true) {
            CLOGD("DUAL UHD Recording(Switching Mode)");
            m_parameters->setForceSwitchingOnly(true);
        }
    }
#endif
    m_parameters->setZoomPreviewWIthScaler(zoomWithScaler);

#ifdef FPS_CHECK
    for (int i = 0; i < DEBUG_MAX_PIPE_NUM; i++)
        m_debugFpsCount[i] = 0;
#endif

    switch (reprocessingBayerMode) {
        case REPROCESSING_BAYER_MODE_NONE : /* Not using reprocessing */
            CLOGD(" Use REPROCESSING_BAYER_MODE_NONE");
            m_previewFrameFactory->setRequestBayer(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            CLOGD(" Use REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON");
            if (m_parameters->getIntelligentMode() == 1)
                m_previewFrameFactory->setRequestBayer(false);
            else
                m_previewFrameFactory->setRequestBayer(true);

            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
            CLOGD(" Use REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON");
            m_previewFrameFactory->setRequestBayer(false);

            if (m_parameters->getIntelligentMode() == 1)
                m_previewFrameFactory->setRequest3AC(false);
            else
                m_previewFrameFactory->setRequest3AC(true);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            CLOGD(" Use REPROCESSING_BAYER_MODE_PURE_DYNAMIC");
            m_previewFrameFactory->setRequestBayer(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            CLOGD(" Use REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC");
            m_previewFrameFactory->setRequestBayer(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
        default :
            CLOGE(" Unknown dynamic bayer mode");
            m_previewFrameFactory->setRequestBayer(false);
            m_previewFrameFactory->setRequest3AC(false);
            break;
    }

    if (m_parameters->isFlite3aaOtf() == false) {
        m_previewFrameFactory->setRequestBayer(true);
    }

#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("[DNG] Request pure bayer for DNG capture");
        m_previewFrameFactory->setRequestBayer(true);
        m_parameters->setUseDNGCapture(true);
    } else {
        m_parameters->setUseDNGCapture(false);
    }
#endif

#ifdef DEBUG_RAWDUMP
    m_previewFrameFactory->setRequestBayer(true);
#endif

    int width, height;
    uint32_t minfps, maxfps;
    m_parameters->getPreviewSize(&width, &height);
    m_parameters->getPreviewFpsRange(&minfps, &maxfps);
    storeSsrmCameraInfo(m_parameters->getCameraId(), width, height, (int)minfps, (int)maxfps);

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
            CLOGI("[Depth] Enable depth callback on capture");
        }
#ifdef SAMSUNG_FOOD_MODE
        else if (m_parameters->getSceneMode() == SCENE_MODE_FOOD
                    && m_parameters->msgTypeEnabled(CAMERA_MSG_PREVIEW_METADATA) == true) {
            m_previewFrameFactory->setRequest(PIPE_VC1, true);
            m_parameters->setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER);
            m_parameters->setDepthCallbackOnPreview(true);
            CLOGI("[Depth] Enable depth callback on preview ");
        }
#endif
    }
#ifdef SAMSUNG_FOCUS_PEAKING
    else if (m_parameters->getUseDepthMap()
            && m_parameters->getShotMode() == SHOT_MODE_PRO_MODE) {
        m_parameters->setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER);
        CLOGD("[FocusPeaking] Enable setDisparityMode(COMPANION_DISPARITY_CENSUS_CENTER) ");
    }
#endif
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

#ifdef USE_VRA_GROUP
    m_parameters->setDsInputPortId(MCSC_PORT_0);
#endif

#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FASTAE_THREAD_WAIT_END, false);
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_INIT_PIPES_START, true);
    }
#endif
    ret = m_previewFrameFactory->initPipes();
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_INIT_PIPES_START, false);
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_INIT_PIPES_END, true);
    }
#endif
    if (ret < 0) {
        CLOGE("m_previewFrameFactory->initPipes() failed");
        return ret;
    }

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        scpBufferMgr = m_zoomScalerBufferMgr;
#ifdef USE_DUAL_CAMERA
    } else if (m_parameters->getDualCameraMode() == true) {
        scpBufferMgr = m_fusionBufferMgr;
#endif
    } else {
        scpBufferMgr = m_scpBufferMgr;
    }

    if (m_parameters->isFlite3aaOtf() == true)  {
        tempBufferManager = taaBufferManager;

        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_FLITE)] = m_3aaBufferMgr;
    } else {
        tempBufferManager = fliteBufferManager;

        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_FLITE)] = m_3aaBufferMgr;
    }
#ifdef SAMSUNG_DNG_DIRTY_BAYER
    if (m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("[DNG] Set tempBufferManager (m_fliteBufferMgr) for DNG capture");
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;
    } else {
        if (m_parameters->getUsePureBayerReprocessing() == false)
            tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;
        else
            tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_bayerBufferMgr;
    }
#else
    if (m_parameters->getUsePureBayerReprocessing() == false) {
#ifdef USE_REMOSAIC_CAPTURE
        if (m_parameters->isUseRemosaicCapture() == true) {
            tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_bayerBufferMgr;
        }
#else
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_fliteBufferMgr;
#endif
    } else
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VC0)] = m_bayerBufferMgr;
#endif
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
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = m_scpBufferMgr;
#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraPreviewFusionMode() == true)
        tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC0)] = m_fusionBufferMgr;
#endif
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC1)] = m_previewCallbackBufferMgr;
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC2)] = m_recordingBufferMgr;
#ifdef USE_VRA_GROUP
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_MCSC_DS)] = m_vraBufferMgr;
#endif
    if (m_parameters->isMcscVraOtf() == false) {
        tempBufferManager = vraBufferManager;
    }

#ifdef USE_VRA_GROUP
    tempBufferManager[m_previewFrameFactory->getNodeType(PIPE_VRA)] = m_vraBufferMgr;
#endif

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

#ifdef USE_VRA_GROUP
    for (int i = 0; i < MAX_NODE; i++) {
        /* If even one buffer slot is valid. call setBufferManagerToPipe() */
        if (vraBufferManager[i] != NULL) {
            ret = m_previewFrameFactory->setBufferManagerToPipe(vraBufferManager, PIPE_VRA);
            if (ret != NO_ERROR) {
                CLOGE("m_previewFrameFactory->setBufferManagerToPipe(vraBufferManager, %d) failed",
                     PIPE_VRA);
                return ret;
            }
            break;
        }
    }
#endif

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
        else if (m_parameters->is3aaIspOtf() && IS_OUTPUT_NODE(m_previewFrameFactory, PIPE_3AA))
            pipeForFlip = PIPE_3AA;
        else
            pipeForFlip = PIPE_FLITE;
    }

    enum NODE_TYPE nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC2);
    m_previewFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeForFlip, nodeType);

#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    if (m_parameters->getCameraDirection() == CAMERA_FACING_DIRECTION_BACK) {
        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC2);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, pipeForFlip, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, pipeForFlip, nodeType);

#ifdef USE_MCSC1_FOR_PREVIEWCALLBACK
        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC1);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, 1, pipeForFlip, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, pipeForFlip, nodeType);
#endif

        nodeType = m_previewFrameFactory->getNodeType(PIPE_MCSC0);
        m_previewFrameFactory->setControl(V4L2_CID_HFLIP, 1, pipeForFlip, nodeType);
        m_previewFrameFactory->setControl(V4L2_CID_VFLIP, 1, pipeForFlip, nodeType);
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        int pipeId = PIPE_SYNC;
        /* default outputQ setting */
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

        ret = m_getBufferManager(pipeId, &syncBufferManager[OUTPUT_NODE], SRC_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        ret = m_getBufferManager(pipeId, &syncBufferManager[CAPTURE_NODE], DST_BUFFER_DIRECTION);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
            return ret;
        }

        for (int i = 0; i < MAX_NODE; i++) {
            /* If even one buffer slot is valid. call setBufferManagerToPipe() */
            if (syncBufferManager[i] != NULL) {
                ret = m_previewFrameFactory->setBufferManagerToPipe(syncBufferManager, pipeId);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(syncBufferManager, %d) failed",
                        __FUNCTION__, __LINE__, pipeId);
                    return ret;
                }
                break;
            }
        }

        if (m_parameters->getFlagForceSwitchingOnly() == true) {
            pipeId = PIPE_SYNC_SWITCHING;
            /* default outputQ setting */
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

            ret = m_getBufferManager(pipeId, &syncBufferManager[OUTPUT_NODE], SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            ret = m_getBufferManager(pipeId, &syncBufferManager[CAPTURE_NODE], DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            for (int i = 0; i < MAX_NODE; i++) {
                /* If even one buffer slot is valid. call setBufferManagerToPipe() */
                if (syncBufferManager[i] != NULL) {
                    ret = m_previewFrameFactory->setBufferManagerToPipe(syncBufferManager, pipeId);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):m_previewFrameFactory->setBufferManagerToPipe(syncBufferManager, %d) failed",
                                __FUNCTION__, __LINE__, pipeId);
                        return ret;
                    }
                    break;
                }
            }
        }

        pipeId = PIPE_FUSION;

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

    ret = m_previewFrameFactory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("m_previewFrameFactory->mapBuffers() failed");
        return ret;
    }

    m_printExynosCameraInfo(__FUNCTION__);

    pipeId = PIPE_FLITE;

    if (m_parameters->isFlite3aaOtf() == false
        || (reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON
            && m_parameters->getIntelligentMode() != 1)) {
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true &&
                m_parameters->getFlagForceSwitchingOnly() == true)
            m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
#endif
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

    if (m_parameters->isMcscVraOtf() == false) {
        pipeId = PIPE_VRA;
        m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
    }

    minBayerFrameNum = minBayerFrameNum / m_parameters->getBatchSize(PIPE_FLITE);
    min3AAFrameNum = min3AAFrameNum / m_parameters->getBatchSize(PIPE_3AA);

    uint32_t loopFrameNum = minBayerFrameNum > min3AAFrameNum ? minBayerFrameNum : min3AAFrameNum;

    CLOGD("loopFrameNum(%d), minBayerFrameNum(%d), min3AAFrameNum(%d)", loopFrameNum, minBayerFrameNum, min3AAFrameNum);

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        int masterCameraId, slaveCameraId;
        getDualCameraId(&masterCameraId, &slaveCameraId);

        m_parameters->registerDualNotifyCallback(this, &ExynosCamera::m_dualNotifyFunc);

        switch (m_parameters->getDualCameraSyncType()) {
        case SYNC_TYPE_BYPASS:
            if (m_cameraId == slaveCameraId) {
#ifdef DUAL_STANDBY_MODE_SLAVE
                m_parameters->setDualStandbyMode(ExynosCameraParameters::DUAL_STANDBY_MODE_SLAVE);
#else
                m_parameters->setDualStandbyMode(ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST);
#endif
            }
            break;
        case SYNC_TYPE_SWITCH:
            if (m_cameraId == masterCameraId) {
#ifdef DUAL_STANDBY_MODE_MASTER
                m_parameters->setDualStandbyMode(ExynosCameraParameters::DUAL_STANDBY_MODE_MASTER);
#else
                m_parameters->setDualStandbyMode(ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST);
#endif
            }
            break;
        default:
            break;
        }

#ifdef DUAL_SMOOTH_TRANSITION_LAUNCH
        if (m_isFirstStart == true) {
            if (masterCameraId == m_cameraId) {
                m_parameters->finishStartPreview(m_cameraId);
                if (m_parameters->getDualCameraSyncType() == SYNC_TYPE_SWITCH) {
#ifdef SAMSUNG_DUAL_SOLUTION
                    m_parameters->setForceWide(true);
#else
                    m_parameters->setDualSmoothTransitionCount(DUAL_SMOOTH_TRANSITION_COUNT);
#endif
                    CLOGI("setting smoothTransitionCount in master");
                }
            } else {
                /* in case of slave camera and not standby, forcely setting smoothTransitionCount */
                if (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_READY ||
                        m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_INACTIVE) {
#ifdef SAMSUNG_DUAL_SOLUTION
                    m_parameters->setForceWide(true);
#else
                    m_parameters->setDualSmoothTransitionCount(DUAL_SMOOTH_TRANSITION_COUNT);
#endif
                    CLOGI("setting smoothTransitionCount in slave");
                }
            }
        } else {
            m_parameters->finishStartPreview(m_cameraId);
        }
#endif
        m_parameters->setDualWakeupFinishCount(DUAL_WAKEUP_FINISH_COUNT);
        CLOGD("standby mode(%d) syncType(%d) switchingOnly(%d)", m_parameters->getDualStandbyMode(),
                m_parameters->getDualCameraSyncType(),
                m_parameters->getFlagForceSwitchingOnly());

        if (m_parameters->getDualStandbyMode() ==
                ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR) {
            flagSensorStandby = true;

            /* default outputQ setting for pipe->startThread() success */
            pipeId = PIPE_3AA;
            if (m_parameters->isFlite3aaOtf() == true)
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

            pipeId = PIPE_ISP;
            if (m_parameters->is3aaIspOtf() == false)
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

            pipeId = PIPE_TPU;
            if (m_parameters->isIspTpuOtf() == false &&
                    m_parameters->getTpuEnabledMode() == true)
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);

            pipeId = PIPE_MCSC;
            if (m_parameters->isIspTpuOtf() == false &&
                    m_parameters->isIspMcscOtf() == false)
                m_previewFrameFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ, pipeId);
        }
    }
#endif

    /* skip qbuf in sensor standby mode */
    for (uint32_t i = 0; i < loopFrameNum && !flagSensorStandby; i++) {
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
        if(m_parameters->isSccCapture() == true) {

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

#ifdef RAWDUMP_CAPTURE
#ifndef DEBUG_RAWDUMP_DIRTY_BAYER
    /* s_ctrl HAL version for selecting dvfs table */
    ret = m_previewFrameFactory->setControl(V4L2_CID_IS_HAL_VERSION, IS_HAL_VER_3_2, PIPE_3AA);
    CLOGD("V4L2_CID_IS_HAL_VERSION_%d pipe(%d)", IS_HAL_VER_3_2, PIPE_3AA);
    if (ret < 0)
        CLOGW(" V4L2_CID_IS_HAL_VERSION is fail");
#endif
#endif

#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_INIT_PIPES_END, false);
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_START_START, true);
    }
#endif
    /* stream on pipes */
    ret = m_previewFrameFactory->startPipes();
#ifdef TIME_LOGGER_LAUNCH_ENABLE
#ifdef USE_DUAL_CAMERA
    if (TIME_LOGGER_LAUNCH_COND(m_parameters, m_cameraId))
#endif
    {
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_SENSOR_STREAM_ON, false);
        TIME_LOGGER_UPDATE_BASE(&g_timeLoggerLaunch, m_cameraId, g_timeLoggerLaunchIndex, 0, DURATION, LAUNCH_FACTORY_START_END, true);
    }
#endif
    if (ret < 0) {
        m_previewFrameFactory->stopPipes();
        CLOGE("startPipe fail");
        /* stopPipe fail but all node closed */
        /* return ret; */
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

#ifdef SAMSUNG_OIS
    if (m_parameters->getOISModeSetting() == true) {
        CLOGD("set OIS Mode");
        m_parameters->setOISMode();
        m_parameters->setOISModeSetting(false);
    }
#endif

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
            /* stopPipe fail but all node closed */
            /* return ret; */
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


    CLOGD("Recording m_recordingCallbackHeap free IN");
    if (m_recordingCallbackHeap != NULL) {
        m_recordingCallbackHeap->release(m_recordingCallbackHeap);
        m_recordingCallbackHeap = NULL;
    }
    CLOGD("Recording m_recordingCallbackHeap free OUT");

    m_pipeFrameDoneQ->release();

    m_isZSLCaptureOn = false;

    m_previewEnabled = false;
    m_parameters->setPreviewRunning(m_previewEnabled);

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        m_parameters->unregisterDualNotifyCallback();
    }
#endif

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

    if (m_parameters->getUsePureBayerReprocessing() == true)
        reprocessingHeadPipeId = PIPE_3AA_REPROCESSING;
    else
        /* TODO : isUseYuvReprocessing() case */
        reprocessingHeadPipeId = PIPE_ISP_REPROCESSING;

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
        CLOGE("m_stopPreviewInternal fail");
        err = ret;
    }

    if (m_previewQ != NULL) {
        m_clearList(m_previewQ);
    }

    if (m_previewCallbackQ != NULL) {
        m_clearList(m_previewCallbackQ);
    }

#ifdef SUPPORT_SW_VDIS
    int cameraId = getCameraIdInternal();
    if (m_swVDIS_Mode && (m_swVDIS_ModeDual == false || (m_swVDIS_ModeDual == true && cameraId != CAMERA_ID_BACK_1))) {
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
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
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
    if (m_vraBufferMgr != NULL) {
        m_vraBufferMgr->deinit();
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
        m_sccReprocessingBufferMgr->deinit(m_cameraId);
    }

    if (m_postPictureBufferMgr != NULL) {
        if (m_postPictureBufferMgr->getContigBufCount() != 0) {
            m_postPictureBufferMgr->deinit(m_cameraId);
        } else {
            m_postPictureBufferMgr->resetBuffers(m_cameraId);
        }
    }

#ifdef SAMSUNG_LENS_DC
    if (m_lensDCBufferMgr != NULL) {
        m_lensDCBufferMgr->resetBuffers();
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (m_strCaptureBufferMgr != NULL) {
        m_strCaptureBufferMgr->resetBuffers();
    }
#endif

    if (m_thumbnailBufferMgr != NULL) {
        m_thumbnailBufferMgr->deinit(m_cameraId);
    }

    if (m_gscBufferMgr != NULL) {
        m_gscBufferMgr->resetBuffers();
    }
    if (m_jpegBufferMgr != NULL) {
        m_jpegBufferMgr->resetBuffers(m_cameraId);

        if (reservedMemoryCount > 0) {
            m_jpegBufferMgr->deinit(m_cameraId);
        }
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
#ifdef USE_DUAL_CAMERA
    if (m_fusionBufferMgr != NULL) {
        m_fusionBufferMgr->deinit();
    }

    if (m_fusionReprocessingBufferMgr != NULL) {
        m_fusionReprocessingBufferMgr->deinit();
    }
#endif

    if (m_captureSelector != NULL) {
        m_captureSelector->release();
    }
    if (m_sccCaptureSelector != NULL) {
        m_sccCaptureSelector->release();
    }

    if( m_parameters->getHighSpeedRecording() && m_parameters->getReallocBuffer() ) {
        CLOGD(" realloc buffer all buffer deinit ");
        if (m_bayerBufferMgr != NULL) {
            m_bayerBufferMgr->deinit();
        }
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
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

        if (m_vraBufferMgr != NULL) {
            m_vraBufferMgr->deinit();
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
    if (m_use_companion == true) {
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
        /* Comment out : SetupQThread[FLITE] is not used, but when we reduce FLITE buffer, it is useful */
        /*
        switch (m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(NULL);
            CLOGD("setupThread Thread start pipeId(%d)", PIPE_FLITE);
            m_mainSetupQThread[INDEX(PIPE_FLITE)]->run(PRIORITY_URGENT_DISPLAY);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
            CLOGD("setupThread with List pipeId(%d)", PIPE_FLITE);
            m_mainSetupQ[INDEX(PIPE_FLITE)]->setup(m_mainSetupQThread[INDEX(PIPE_FLITE)]);
            break;
        default:
            CLOGI("setupThread not started pipeID(%d)", PIPE_FLITE);
            break;
        }
        */
        CLOGD("setupThread Thread start pipeId(%d)", PIPE_3AA);
        m_mainSetupQThread[INDEX(PIPE_3AA)]->run(PRIORITY_URGENT_DISPLAY);
    }

    if (m_monitorThread->isRunning() == false)
        m_monitorThread->run(PRIORITY_DEFAULT);

    m_disablePreviewCB = false;

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_parameters->getDualCameraMode() == false ||
           (m_parameters->getDualCameraMode() == true &&
            m_cameraId == masterCameraId)) {
        /* don't start previewThread in slave camera for dual camera */
#endif
        if (m_facedetectThread->isRunning() == false)
            m_facedetectThread->run();

        m_previewThread->run(PRIORITY_DISPLAY);
#ifdef USE_DUAL_CAMERA
    }
#endif

    m_mainThread->run(PRIORITY_DEFAULT);
    m_startPictureInternalThread->join();

    if (m_parameters->getZoomPreviewWIthScaler() == true) {
        CLOGD("ZoomPreview with Scaler Thread start");
        m_zoomPreviwWithCscThread->run(PRIORITY_DEFAULT);
    }

    /* On High-resolution callback scenario, the reprocessing works with specific fps.
       To avoid the thread creation performance issue, threads in reprocessing pipes
       must be run with run&wait mode */
    bool needOneShotMode = (m_parameters->getHighResolutionCallbackMode() == false);
    for (int i = 0; i < m_getNumOfReprocessingInstance(); i++) {
        m_pReprocessingFrameFactories[i]->setThreadOneShotMode(reprocessingHeadPipeId, needOneShotMode);
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
    int reprocessingHeadPipeId = -1;
    int mcscContainedPipeId = -1;

    /* get head of reprocessing pipe */
    if (m_parameters->getUsePureBayerReprocessing() == true)
        reprocessingHeadPipeId = PIPE_3AA_REPROCESSING;
    else
        /* TODO : isUseYuvReprocessing() case */
        reprocessingHeadPipeId = PIPE_ISP_REPROCESSING;

    /* get the pipeId to have mcsc */
    if (m_parameters->isReprocessingIspMcscOtf() == false)
        mcscContainedPipeId = PIPE_MCSC_REPROCESSING;
    else if (m_parameters->isReprocessing3aaIspOTF() == false)
        mcscContainedPipeId = PIPE_ISP_REPROCESSING;
    else
        mcscContainedPipeId = PIPE_3AA_REPROCESSING;

#ifdef USE_DUAL_CAMERA
    ExynosCameraBufferManager *syncBufferManager[MAX_NODE];
    ExynosCameraBufferManager *fusionBufferManager[MAX_NODE];
#endif

    if (m_zslPictureEnabled == true) {
        CLOGD(" zsl picture is already initialized");
        return NO_ERROR;
    }

    if (m_parameters->isReprocessing() == true) {
        int flipHorizontal = m_parameters->getFlipHorizontal();
        CLOGD("set FlipHorizontal on capture, FlipHorizontal(%d)", flipHorizontal);

        m_parameters->getThumbnailSize(&thumbnailW, &thumbnailH);

        ret = m_setReprocessingBuffer();
        if (ret != NO_ERROR) {
            CLOGE("m_setReprocessing Buffer fail");
            return ret;
        }

        bool bPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();
        bool bReprocessing3aaIspOTF = m_parameters->isReprocessing3aaIspOTF();

        for (int i = 0; i < m_getNumOfReprocessingInstance(); i++) {

             for (int i = 0; i < MAX_NODE; i++) {
                 taaBufferManager[i] = NULL;
                 ispBufferManager[i] = NULL;
#ifdef USE_DUAL_CAMERA
                 syncBufferManager[i] = NULL;
                 fusionBufferManager[i] = NULL;
#endif
             }


            //Second instance is for pure bayer reprocessing
            if (i == REPROCESSING_FRAME_FACTORY_PURE_REMOSAIC) {
                bPureBayerReprocessing = true;
                bReprocessing3aaIspOTF = true;

                reprocessingHeadPipeId = PIPE_3AA_REPROCESSING;
            }

            m_reprocessingFrameFactory = m_pReprocessingFrameFactories[i];

            if (m_reprocessingFrameFactory == NULL) {
                CLOGE("reprocessingFrameFactory[%d] is NULL", i);
                continue;
            }

            if (thumbnailW > 0 && thumbnailH > 0) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, true);
            } else {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
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

            enum NODE_TYPE nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

            nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

            nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
            m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, flipHorizontal, mcscContainedPipeId, nodeType);

#ifdef USE_MULTI_FACING_SINGLE_CAMERA
            if (m_parameters->getCameraDirection() == CAMERA_FACING_DIRECTION_BACK) {
                nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC1_REPROCESSING);
                m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, mcscContainedPipeId, nodeType);
                m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, mcscContainedPipeId, nodeType);

                nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING);
                m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, mcscContainedPipeId, nodeType);
                m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, mcscContainedPipeId, nodeType);

                nodeType = m_reprocessingFrameFactory->getNodeType(PIPE_MCSC4_REPROCESSING);
                m_reprocessingFrameFactory->setControl(V4L2_CID_HFLIP, !flipHorizontal, mcscContainedPipeId, nodeType);
                m_reprocessingFrameFactory->setControl(V4L2_CID_VFLIP, 1, mcscContainedPipeId, nodeType);
             }
#endif

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

            if (bPureBayerReprocessing == true) {
                tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AA_REPROCESSING)] = m_bayerBufferMgr;
                tempBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_3AP_REPROCESSING)] = m_ispReprocessingBufferMgr;
            }

            if (bPureBayerReprocessing == false
                || bReprocessing3aaIspOTF == false)
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

            if (bPureBayerReprocessing == true) {
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            int pipeId = PIPE_SYNC_REPROCESSING;

            /* default outputQ setting */
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_syncReprocessingQ, pipeId);

            ret = m_getBufferManager(pipeId, &syncBufferManager[OUTPUT_NODE], SRC_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getBufferManager(SRC) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            ret = m_getBufferManager(pipeId, &syncBufferManager[CAPTURE_NODE], DST_BUFFER_DIRECTION);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):getBufferManager(DST) fail, pipeId(%d), ret(%d)", __FUNCTION__, __LINE__, pipeId, ret);
                return ret;
            }

            for (int i = 0; i < MAX_NODE; i++) {
                /* If even one buffer slot is valid. call setBufferManagerToPipe() */
                if (syncBufferManager[i] != NULL) {
                    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(syncBufferManager, pipeId);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->setBufferManagerToPipe(syncBufferManager, %d) failed",
                            __FUNCTION__, __LINE__, pipeId);
                        return ret;
                    }
                    break;
                }
            }

            pipeId = PIPE_FUSION_REPROCESSING;

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
                    ret = m_reprocessingFrameFactory->setBufferManagerToPipe(fusionBufferManager, pipeId);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):m_reprocessingFrameFactory->setBufferManagerToPipe(fusionBufferManager, %d) failed",
                            __FUNCTION__, __LINE__, pipeId);
                        return ret;
                    }
                    break;
                }
            }
        }
#endif

#ifdef USE_ODC_CAPTURE
            if (m_parameters->getODCCaptureMode()) {
                ExynosCameraBufferManager *tpuBufferManager[MAX_NODE];
                tpuBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_TPU1)] = m_sccReprocessingBufferMgr;
                tpuBufferManager[m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C)] = m_sccReprocessingBufferMgr;
                ret = m_reprocessingFrameFactory->setBufferManagerToPipe(tpuBufferManager, PIPE_TPU1);
                if (ret != NO_ERROR) {
                    CLOGE("m_reprocessingFrameFactory->setBufferManagerToPipe(ispBufferManager, %d) failed",
                            PIPE_3AA_REPROCESSING);
                    return ret;
                }
            }
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
            m_reprocessingFrameFactory->setThreadOneShotMode(reprocessingHeadPipeId, needOneShotMode);
        }

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
    int mcscContainedPipeId = PIPE_3AA_REPROCESSING;

    if (m_parameters->getUsePureBayerReprocessing() == false) {
        mcscContainedPipeId = PIPE_ISP_REPROCESSING;
    }

    m_prePictureThread->join();
    m_pictureThread->join();

#ifdef SAMSUNG_LLS_DEBLUR
    m_LDCaptureThread->join();
#endif

#ifdef USE_MSC_CAPTURE
    m_MscCaptureThread->join();
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
        for (int i = 0; i < m_getNumOfReprocessingInstance(); i++) {
            ret = m_pReprocessingFrameFactories[i]->stopPipes();
            if (ret < 0) {
                CLOGE("m_reprocessingFrameFactory0>stopPipe() fail");
            }
        }
    }

    if (m_parameters->getHighResolutionCallbackMode() == true) {
        m_highResolutionCallbackThread->stop();
        if (m_highResolutionCallbackQ != NULL)
            m_highResolutionCallbackQ->sendCmd(WAKE_UP);
        m_highResolutionCallbackThread->requestExitAndWait();
    }

    /* Clear frames & buffers which remain in capture processingQ */
    m_clearFrameQ(m_dstSccReprocessingQ, mcscContainedPipeId, SRC_BUFFER_DIRECTION);
    m_clearFrameQ(m_postPictureQ, PIPE_SCC, DST_BUFFER_DIRECTION);
    m_clearFrameQ(m_dstJpegReprocessingQ, PIPE_JPEG, SRC_BUFFER_DIRECTION);

    m_dstIspReprocessingQ->release();
    m_dstGscReprocessingQ->release();

#ifdef USE_DUAL_CAMERA
    m_syncReprocessingQ->release();
    m_fusionReprocessingQ->release();
#endif

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
            CLOGE("[BD]Plugin deinit failed!!");

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
        CLOGD("m_highResolutionCallbackQ->getSizeOfProcessQ(%d). release the highResolutionCallbackQ.",
             m_highResolutionCallbackQ->getSizeOfProcessQ());
        m_highResolutionCallbackQ->release();
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
    m_nv21CaptureEnabled = false;

    /* TODO: need timeout */
    return NO_ERROR;
}

status_t ExynosCamera::m_doPreviewToCallbackFunc(
        int32_t pipeId,
        ExynosCameraBuffer previewBuf,
        ExynosCameraBuffer callbackBuf)
{
    CLOGV(" converting preview to callback buffer");

    int ret = 0;
    status_t statusRet = NO_ERROR;

    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    if (m_flagThreadStop == true || m_previewEnabled == false) {
        CLOGE(" preview was stopped!");
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
            CLOGE("newFrame is NULL");
            goto done;
        }

        m_calcPreviewGSCRect(&srcRect, &dstRect);
        newFrame->setSrcRect(pipeId, &srcRect);
        newFrame->setDstRect(pipeId, &dstRect);

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

    dstAddr = callbackBuf.addr[0];
    /* TODO : have to consider all fmt(planes) and stride */
    for (int plane = 0; plane < planeCount; plane++) {
        srcAddr = previewBuf.addr[plane];
        memcpy(dstAddr, srcAddr, previewBuf.size[plane]);
        dstAddr = dstAddr + previewBuf.size[plane];

            if (m_ionClient >= 0)
#ifdef USE_LIB_ION_LEGACY
                ion_sync_fd(m_ionClient, callbackBuf.fd[plane]);
#else
                exynos_ion_sync_fd(m_ionClient, callbackBuf.fd[plane]);
#endif
        }
    }
done:

    if (newFrame != NULL) {
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
    camera_frame_metadata_t m_Metadata;
    ExynosCameraBuffer callbackBuf;
    int pipeId = PIPE_MCSC0;

    camera_memory_t *previewCallbackHeap = NULL;

    callbackBuf.index = -2;

    ret = newFrame->getSrcBuffer(pipeId, &callbackBuf);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, ret(%d)", ret);
    }

    previewCallbackHeap = m_getMemoryCb(callbackBuf.fd[0], callbackBuf.size[0], 1, m_callbackCookie);
    if (!previewCallbackHeap || previewCallbackHeap->data == MAP_FAILED) {
        CLOGE("m_getMemoryCb(%d) fail", callbackBuf.size[0]);
        statusRet = INVALID_OPERATION;
        goto done;
    }

    memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));
#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_Metadata.timestamp = newFrame->getTimeStampBoot();
#else
    m_Metadata.timestamp = newFrame->getTimeStamp();
#endif
    CLOGV("timestamp:%jd ms!", m_Metadata.timestamp);

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
        }
#ifdef SAMSUNG_LIVE_OUTFOCUS
        else if (m_parameters->getShotMode() == SHOT_MODE_LIVE_OUTFOCUS) {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, &m_Metadata, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }
#endif
        else {
            setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
            m_dataCb(CAMERA_MSG_PREVIEW_FRAME, previewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        }
    }

#ifdef SAMSUNG_COLOR_IRIS
    if ((getCameraIdInternal() == CAMERA_ID_FRONT) && (m_parameters->msgTypeEnabled(CAMERA_MSG_IRIS_PREVIEW_DATA))) {
        camera_memory_t *fdCallbackData = NULL;

        fdCallbackData = m_getMemoryCb(-1, sizeof(CameraBufferFD), 1, m_callbackCookie);
        if (fdCallbackData == NULL) {
            CLOGE("m_getMemoryCb fail");
            goto done;
        }

#if 0
        char filePath[50];
        if (dumpIndex >5 && dumpIndex <15) {
            snprintf(filePath, sizeof(filePath), "/data/media/0/DCIM/Camera/colorIRIS%02d.raw", dumpIndex);
            CLOGE("iris dump %s", filePath);

            dumpToFile(filePath,  (char *)callbackBuf.addr[0], 752 * 1328 * 1.5);
        }
        dumpIndex ++;
#endif

        ((CameraBufferFD *)fdCallbackData->data)->magic = 0x00FD42FD;
        ((CameraBufferFD *)fdCallbackData->data)->fd = callbackBuf.fd[0];
        CLOGV("FRONT_CAMERA CAMERA_MSG_IRIS_PREVIEW_DATA Callback");

        setBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);
        m_dataCb(CAMERA_MSG_IRIS_PREVIEW_DATA, fdCallbackData, 0, NULL, m_callbackCookie);
        clearBit(&m_callbackState, CALLBACK_STATE_PREVIEW_FRAME, false);

        fdCallbackData->release(fdCallbackData);
   }
#endif

    probeTimer.stop();
    probeTimeMSEC = (int)probeTimer.durationMsecs();

    if (probeTimeMSEC > 33 && probeTimeMSEC <= 66)
        CLOGV("duration time(%5d msec)", (int)probeTimer.durationMsecs());
    else if(probeTimeMSEC > 66)
        CLOGD("duration time(%5d msec)", (int)probeTimer.durationMsecs());
    else
        CLOGV("duration time(%5d msec)", (int)probeTimer.durationMsecs());

done:
    if (previewCallbackHeap != NULL) {
        previewCallbackHeap->release(previewCallbackHeap);
    }

    if (needBufferRelease && callbackBuf.index >= 0) {
        ret = m_putBuffers(m_previewCallbackBufferMgr, callbackBuf.index);
        if (ret != NO_ERROR) {
            CLOGE("Preview callback buffer return fail");
        }
    }

    return statusRet;
}

bool ExynosCamera::m_previewCallbackThreadFunc(void)
{
    bool loop = true;
    int ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    CLOGV("wait m_previewCallbackQ output");
    ret = m_previewCallbackQ->waitAndPopProcessQ(&newFrame);
    if (m_flagThreadStop == true || ret == TIMED_OUT) {
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);

        if (newFrame != NULL) {
            newFrame = NULL;
        }

        return false;
    }
    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        goto done;
    }

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        goto done;
    }

    ret = m_previewCallbackFunc(newFrame, true);
    if (ret < 0) {
        CLOGE("m_previewCallbackFunc fail");
    }

done:

    if (newFrame != NULL) {
        newFrame = NULL;
    }

    return loop;
}

status_t ExynosCamera::m_doCallbackToPreviewFunc(
        __unused int32_t pipeId,
        __unused ExynosCameraFrameSP_sptr_t newFrame,
        ExynosCameraBuffer callbackBuf,
        ExynosCameraBuffer previewBuf)
{
    CLOGV(" converting callback to preview buffer");

    status_t statusRet = NO_ERROR;

    int hwPreviewW = 0, hwPreviewH = 0;
    int hwPreviewFormat = m_parameters->getHwPreviewFormat();
    bool useCSC = m_parameters->getCallbackNeedCSC();

    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

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
        srcAddr = callbackBuf.addr[0];
        for (int plane = 0; plane < planeCount; plane++) {
            dstAddr = previewBuf.addr[plane];
            memcpy(dstAddr, srcAddr, previewBuf.size[plane]);
            srcAddr = srcAddr + previewBuf.size[plane];

            if (m_ionClient >= 0)
#ifdef USE_LIB_ION_LEGACY
                ion_sync_fd(m_ionClient, previewBuf.fd[plane]);
#else
                exynos_ion_sync_fd(m_ionClient, previewBuf.fd[plane]);
#endif
        }
    }

done:

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
    camera2_shot_ext *shot_ext = NULL;
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
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    ExynosRect mainRoiRect, subRoiRect;
    int zoomLevel = 0;
#endif

    camera2_shot_ext *updateDmShot = new struct camera2_shot_ext;
    memset(updateDmShot, 0x0, sizeof(struct camera2_shot_ext));

    bayerBuffer.index = -2;
    ispReprocessingBuffer.index = -2;
#ifdef SUPPORT_DEPTH_MAP
    depthMapBuffer.index = -2;
#endif

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
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

#ifdef USE_REMOSAIC_CAPTURE
    m_sensorModeSwitchThread->join();

    m_parameters->setRemosaicCaptureMode(ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_NONE);
    if (m_parameters->getAvailableRemosaicCapture() == true) {
        prePictureDonePipeId = PIPE_3AA_REPROCESSING;

        //m_parameters->setUsePureBayerReprocessing(true);
        //m_parameters->updateReprocessingBayerMode();

        m_parameters->setRemosaicCaptureMode(ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_PURE_BAYER);
        m_reprocessingFrameFactory = m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PURE_REMOSAIC];

        CLOGD("Run sensorModeSwitchThread");
        m_sensorModeSwitchThread->run();
    } else
#endif
    {
        m_reprocessingFrameFactory = m_pReprocessingFrameFactories[REPROCESSING_FRAME_FACTORY_PROCESSED];
    }

    if (m_reprocessingFrameFactory == NULL) {
        CLOGE("m_reprocessingFrameFactory is NULL!!!!");
    }

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


#if defined(SAMSUNG_JQ) && defined(ONE_SECOND_BURST_CAPTURE)
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD("[JQ] ON!!");
        m_isJpegQtableOn = true;
        m_parameters->setJpegQtableOn(true);
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true &&
                m_cameraId == slaveCameraId &&
                m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC) {
        CLOGD("don't call fast shutter callback!!");
    } else {
#endif
    if (m_isZSLCaptureOn
#ifdef OIS_CAPTURE
        || m_OISCaptureShutterEnabled
#endif
    ) {
        CLOGD("fast shutter callback!!");
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
    }
#ifdef USE_DUAL_CAMERA
    }
#endif

#ifdef USE_REMOSAIC_CAPTURE
    CLOGD("Join sensorModeSwitchThread");
    m_sensorModeSwitchThread->join();
#endif

    /* Check available buffer */
    ret = m_getBufferManager(prePictureDonePipeId, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("Failed to getBufferManager. pipeId %d direction %d ret %d",
                prePictureDonePipeId, DST_BUFFER_DIRECTION, ret);
        goto CLEAN;
    }

    if (bufferMgr != NULL) {
        if (m_parameters->getHighResolutionCallbackMode() == false) {
            ret = m_checkBufferAvailable(prePictureDonePipeId, bufferMgr);
            if (ret < 0) {
                CLOGE("Failed to wait available buffer. pipeId %d ret %d",
                        prePictureDonePipeId, ret);
                goto CLEAN;
            }
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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (bayerFrame == NULL || result == ExynosCameraFrameSelector::RESULT_SKIP_FRAME) {
            CLOGW("skip takePicture in this camera for dual");
            m_captureLock.lock();
            m_pictureEnabled = false;
            m_captureLock.unlock();
            m_reprocessingCounter.clearCount();
            m_pictureCounter.clearCount();
            m_jpegCounter.clearCount();
            m_yuvcallbackCounter.clearCount();
            m_terminatePictureThreads(false);
            goto CLEAN_FRAME;
        }
    }
#endif

#ifdef SAMSUNG_LBP
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE && m_isLBPon == true) {
        ret = m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
        m_LBPThread->requestExitAndWait();
        m_lbpBufferMgr->resetBuffers();
        if(m_LBPpluginHandle != NULL) {
            ret = uni_plugin_deinit(m_LBPpluginHandle);
            if(ret < 0)
                CLOGE("[LBP]Bestphoto plugin deinit failed!!");
        }
        m_LBPindex = 0;
        m_isLBPon = false;
    }
#endif

#ifdef USE_DUAL_CAMERA
    /* takePicture will be running in only master for burst mode / flash */
    if (m_cameraId == masterCameraId &&
            m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC &&
            ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) ||
             (m_parameters->getFlashMode() == FLASH_MODE_ON) ||
             (m_exynosCameraActivityControl->getFlashMgr()->getNeedCaptureFlash() == true &&
                     m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_NONE)
#ifdef SAMSUNG_DUAL_SOLUTION
                || (m_parameters->getForceWide() == true)
                || (m_parameters->getFusionCaptureMode(m_cameraId, false) == false)
#endif
    )) {
        bayerFrame->setReprocessingSyncType(SYNC_TYPE_BYPASS);
    } else {
        bayerFrame->setReprocessingSyncType(m_parameters->getDualCameraReprocessingSyncType());
    }

    CLOGD("bayer frame(fcount:%d, sync:%d, reprocessingSync:%d), currentSync(%d/%d)",
            bayerFrame->getFrameCount(),
            bayerFrame->getSyncType(),
            bayerFrame->getReprocessingSyncType(),
            m_parameters->getDualCameraSyncType(),
            m_parameters->getDualCameraReprocessingSyncType());

    if (m_parameters->getDualCameraMode() == true &&
            bayerFrame->getReprocessingSyncType() == SYNC_TYPE_SYNC) {
        CLOGD("this frame is sync frame(%d)", bayerFrame->getFrameCount());
        if (m_parameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
            m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
            if (m_parameters->isUseHWFC()) {
                m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                CLOGD("disable hwFC request");
            }
        }
    } else {
#endif
#ifdef LLS_STUNR
    if (m_parameters->getLLSStunrMode() && m_reprocessingCounter.getCount() > 1) {
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
        CLOGD("STUNR : (%d)frame diable MCSC DMA out and HWFC!!", (LLS_STUNR_CAPTURE_COUNT - m_reprocessingCounter.getCount() + 1));
    } else
#endif
    {
#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
        m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);

        CLOGD("ODC capture: HWFC enable but, JPEG disable");
    } else {
#endif
    if (m_parameters->getOutPutFormatNV21Enable()) {
       if ((m_parameters->getSeriesShotCount() == m_reprocessingCounter.getCount() && m_parameters->getSeriesShotMode() > 0)
#ifdef SAMSUNG_LLS_DEBLUR
            || (m_parameters->getLDCaptureCount() == m_reprocessingCounter.getCount() && m_parameters->getLDCaptureMode() > 0)
#endif
#ifdef SAMSUNG_LENS_DC
            || m_parameters->getLensDCEnable()
#endif
#ifdef SAMSUNG_STR_CAPTURE
            || m_parameters->getSTRCaptureEnable()
#endif
            || m_parameters->getHighResolutionCallbackMode()
            || m_nv21CaptureEnabled
        ) {
            if (m_parameters->isReprocessing() == true) {
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC1_REPROCESSING, true);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                m_reprocessingFrameFactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                if (m_parameters->isUseHWFC()) {
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                    m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                    CLOGD("disable hwFC request");
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
                        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, true);
                        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, true);
                        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                        m_reprocessingFrameFactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, true);
                        CLOGD("enable hwFC request");
                    }

                    if (m_parameters->getIsThumbnailCallbackOn()
#ifdef SAMSUNG_MAGICSHOT
                        || m_parameters->getShotMode() == SHOT_MODE_MAGIC
#endif
#ifdef SAMSUNG_BD
                        || m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST
#endif
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
    }
#ifdef USE_ODC_CAPTURE
    }
#endif
    }
#ifdef USE_DUAL_CAMERA
    } /* USE_DUAL_CAMERA */
#endif

    CLOGD("bayerBuffer index %d", bayerBuffer.index);

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
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true &&
                m_cameraId == slaveCameraId &&
                m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC) {
            CLOGD("don't call fast shutter callback!!");
        } else {
#endif
        m_shutterCallbackThread->join();
        m_shutterCallbackThread->run();
#ifdef USE_DUAL_CAMERA
        }
#endif
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
            CLOGD("LLS last capture frame");
#ifndef LLS_REPROCESSING
            ExynosCameraActivitySpecialCapture *m_sCaptureMgr = m_exynosCameraActivityControl->getSpecialCaptureMgr();
            m_sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
#else
            m_parameters->setOISCaptureModeOn(false);
#endif
    }
#endif

    /* Generate reprocessing Frame */
    ret = m_generateFrameReprocessing(newFrame, bayerFrame);
    if (ret < 0) {
        CLOGE("generateFrameReprocessing fail, ret(%d)", ret);
        goto CLEAN_FRAME;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (result == ExynosCameraFrameSelector::RESULT_SKIP_FRAME) {
            newFrame->setFrameType(FRAME_TYPE_INTERNAL);
            newFrame->setFrameState(FRAME_STATE_SKIPPED);
        }

        ret = m_reprocessingFrameFactory->startThread(PIPE_SYNC_REPROCESSING);
        if (ret < 0) {
            CLOGE("startThread(PIPE_SYNC_REPROCESSING fail");
        }

        if (newFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
            CLOGI("it's internal frame(frameSelResult:%d, reprocessing:%d, bayer:%d)",
                    result,
                    newFrame->getFrameCount(),
                    (bayerFrame != NULL) ? bayerFrame->getFrameCount() : -1);

            ret = m_insertFrameToList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to insert frame into m_postProcessList",
                        newFrame->getFrameCount());
            }

            m_dstSccReprocessingQ->pushProcessQ(&newFrame);

            m_reprocessingCounter.decCount();

            CLOGI("reprocessing complete, remaining count(%d)",
                    m_reprocessingCounter.getCount());

            goto reprocessing_internal_frame;
        }
     }
#endif

#ifndef RAWDUMP_CAPTURE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable() && bayerFrame->getRequest(PIPE_VC0) == true) {
        int sensorMaxW, sensorMaxH;
        int sensorMarginW, sensorMarginH;
        bool bRet;
        char filePath[70];
        time_t rawtime;
        struct tm* timeinfo;
        ExynosCameraBuffer pureBayerBuffer;

        if (m_parameters->getUsePureBayerReprocessing() == false) {
            ret = bayerFrame->getDstBuffer(m_getBayerPipeId(), &pureBayerBuffer, m_previewFrameFactory->getNodeType(PIPE_VC0));
            if (ret < 0) {
                CLOGE(" getDstBuffer(pipeId(%d) dstPos(%d)) fail, ret(%d)", m_getBayerPipeId() , m_previewFrameFactory->getNodeType(PIPE_VC0), ret);
                goto CLEAN;
            }
        } else {
            pureBayerBuffer = bayerBuffer;
        }

        memset(filePath, 0, sizeof(filePath));
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(filePath, sizeof(filePath), "/data/media/0/Raw%d_%d_%02d%02d%02d_%02d%02d%02d.raw",
            m_cameraId, bayerFrame->getFrameCount(), timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
            timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        if (m_parameters->getUsePureBayerReprocessing() == true) {
            /* Pure Bayer Buffer Size == MaxPictureSize + Sensor Margin == Max Sensor Size */
            m_parameters->getMaxSensorSize(&sensorMaxW, &sensorMaxH);
        } else {
            m_parameters->getMaxPictureSize(&sensorMaxW, &sensorMaxH);
        }

        CLOGD("[RAWDUMP] Dump Sensor (%d x %d) : %s", sensorMaxW, sensorMaxH, filePath);

        bRet = dumpToFile((char *)filePath,
                pureBayerBuffer.addr[0],
                sensorMaxW * sensorMaxH * 2);
        if (bRet != true)
            CLOGE("couldn't make a raw file");

        /* release buffer after dump */
        if (m_parameters->getUsePureBayerReprocessing() == false)
            m_putBuffers(m_fliteBufferMgr, pureBayerBuffer.index);
    }
#endif /* DEBUG_RAWDUMP */
#endif

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
    if (m_parameters->getUsePureBayerReprocessing() == false
#ifdef LLS_REPROCESSING
            && m_captureSelector->getIsConvertingMeta() == true
#endif
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

#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
        if (m_parameters->getDualCameraMode()) {
            m_fusionCaptureWrapper->m_setSyncType(m_parameters->getDualCameraReprocessingSyncType());
            m_fusionCaptureWrapper->m_setCameraParam((UniPlugin_DCVOZ_CAMERAPARAM_t*)m_parameters->getFusionParam());
            mainRoiRect.w = output_crop_info.cropRegion[2];
            mainRoiRect.h = output_crop_info.cropRegion[3];
            m_parameters->getHwPictureSize(&subRoiRect.w, &subRoiRect.h);

            if (m_parameters->getDualCameraReprocessingSyncType() != SYNC_TYPE_SWITCH && getCameraId() == CAMERA_ID_BACK_1)
                zoomLevel = bayerFrame->getZoom(OUTPUT_NODE_2);
            else
                zoomLevel = bayerFrame->getZoom(OUTPUT_NODE_1);
            m_fusionCaptureWrapper->m_setCurZoomRatio(getCameraId(), m_parameters->getZoomRatio(getCameraId(), zoomLevel));
            m_fusionCaptureWrapper->m_setCurViewRatio(getCameraId(), m_parameters->getZoomOrgRatio(zoomLevel));
            if (m_parameters->isUseNewCropRect()) {
                 m_fusionCaptureWrapper->m_calRoiRect(getCameraId(), mainRoiRect, subRoiRect);
            }
        }
#endif

#if defined(SAMSUNG_HIFI_LLS) || defined(SR_CAPTURE)
            bool isLlsSrEnabled = false;
#ifdef SAMSUNG_HIFI_LLS
            if (m_parameters->getLDCaptureMode())
                isLlsSrEnabled = true;
#endif
#ifdef SR_CAPTURE
            if (m_parameters->getSROn())
                isLlsSrEnabled = true;
#endif
            if (isLlsSrEnabled &&
                 (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
                  m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
            ) {
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
            if (m_parameters->isUseNewCropRect()) {
                ExynosRect DualRect;
                m_parameters->getDualCropRect(&DualRect);
                sizeControlInfo.pictureSize.w = ALIGN_UP(DualRect.w, 16);
                sizeControlInfo.pictureSize.h = ALIGN_UP(DualRect.h, 16);
                m_sr_cropped_width = sizeControlInfo.pictureSize.w;
                m_sr_cropped_height = sizeControlInfo.pictureSize.h;
            } else
#endif
            {
            sizeControlInfo.pictureSize.w = ALIGN_UP(sizeControlInfo.bdsSize.w, 16);
            sizeControlInfo.pictureSize.h = ALIGN_UP(sizeControlInfo.bdsSize.h, 16);
            m_sr_cropped_width = sizeControlInfo.pictureSize.w;
            m_sr_cropped_height = sizeControlInfo.pictureSize.h;
            }
        }
#endif
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
#ifdef LLS_STUNR
    if (m_parameters->getLLSStunrMode()) {
        updateDmShot->shot.uctl.hwlls_mode.hwLlsMode = CAMERA_HW_LLS_ON;
        updateDmShot->shot.uctl.hwlls_mode.hwLlsProgress = (camera2_is_hw_lls_progress)(LLS_STUNR_CAPTURE_COUNT - m_reprocessingCounter.getCount());
        newFrame->setMetaData(updateDmShot);
    }
#endif
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
            m_reprocessingFrameFactory->setOutputFrameQToPipe(m_dstSccReprocessingQ, pipe);
        }

        /* push frame to SCC pipe */
        m_reprocessingFrameFactory->pushFrameToPipe(newFrame, pipe);
    } else {
#ifdef USE_REMOSAIC_CAPTURE
        if (m_parameters->getRemosaicCaptureMode() == ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_PURE_BAYER)
            pipe = PIPE_3AA_REPROCESSING;
        else
#endif
        if (m_parameters->isReprocessing3aaIspOTF() == false) {
            pipe = PIPE_ISP_REPROCESSING;

            m_getBufferManager(pipe, &bufferMgr, DST_BUFFER_DIRECTION);

            if (m_parameters->getHighResolutionCallbackMode() == false) {
                ret = m_checkBufferAvailable(pipe, bufferMgr);
                if (ret < 0) {
                    CLOGE(" Waiting buffer timeout, pipeId(%d), ret(%d)",
                                pipe, ret);
                    goto CLEAN_FRAME;
                }
            }
        } else {
            pipe = PIPE_3AA_REPROCESSING;
        }

        if ((m_parameters->getHighResolutionCallbackMode() == true) &&
                (m_highResolutionCallbackRunning == true)) {
            m_reprocessingFrameFactory->setFrameDoneQToPipe(NULL, pipe);
        } else {
#ifdef LLS_STUNR
            if (m_parameters->getLLSStunrMode()) {
                if (m_reprocessingCounter.getCount() == 1 ) {
                    m_parameters->setLLSStunrMode(false);
                    m_captureSelector->setFrameHoldCount(REPROCESSING_BAYER_HOLD_COUNT);
                    CLOGD("STUNR : (%d)th frame!!!!", (LLS_STUNR_CAPTURE_COUNT - m_reprocessingCounter.getCount() + 1));
                    m_reprocessingFrameFactory->setFrameDoneQToPipe(m_dstSccReprocessingQ, pipe);
                } else {
                    m_reprocessingFrameFactory->setFrameDoneQToPipe(NULL, pipe);
                }
            } else
#endif
            {
                m_reprocessingFrameFactory->setFrameDoneQToPipe(m_dstSccReprocessingQ, pipe);
            }
        }
    }

#ifdef SAMSUNG_DNG
    if (m_parameters->getUsePureBayerReprocessing() == true && m_parameters->getDNGCaptureModeOn() == true) {
        CLOGD("[DNG]Push frame to dngCaptureQ (fcount %d)",
                 getMetaDmRequestFrameCount(updateDmShot));
        m_dngFrameNumber = getMetaDmRequestFrameCount(updateDmShot);
        m_DNGCaptureThread->run();
        m_dngCaptureQ->pushProcessQ(&bayerBuffer);
    }
#endif

    /* Add frame to post processing list */
    newFrame->frameLock();
    if ((m_parameters->getHighResolutionCallbackMode() == true) &&
            (m_highResolutionCallbackRunning == true)) {
        CLOGV("does not use postPicList in highResolutionCB (%d)",
            newFrame->getFrameCount());
    } else {
#ifdef LLS_STUNR
        if (m_parameters->getLLSStunrMode()) {
            if (m_reprocessingCounter.getCount() == 1) {
                CLOGD(" push frame(%d) to postPictureList", newFrame->getFrameCount());

                ret = m_insertFrameToList(&m_postProcessList, newFrame, &m_postProcessListLock);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Failed to insert frame into m_postProcessList",
                        newFrame->getFrameCount());
                    goto CLEAN_FRAME;
                }
            }
        } else
#endif
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

#ifdef SAMSUNG_JQ
    if (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST
        && m_parameters->getJpegQtableStatus() != JPEG_QTABLE_UPDATED) {
        CLOGV("[JQ] WAIT JPEG_QTABLE_UPDATED");
        m_JQ_mutex.lock();
        m_JQisWait = true;
        m_JQ_condition.waitRelative(m_JQ_mutex, 200000000); /* 0.2sec */
        m_JQisWait = false;
        m_JQ_mutex.unlock();
    }
#endif

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

    /* When enabled SCC capture or pureBayerReprocessing, we need to start bayer pipe thread */
    if (m_parameters->getUsePureBayerReprocessing() == true ||
        m_parameters->isSccCapture() == true)
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

        if (!(m_parameters->getUsePureBayerReprocessing() == true
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

#ifdef LLS_CAPTURE
        if (shot_ext != NULL) {
            if ((m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_LLS ||
                 m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_SIS) &&
                 m_reprocessingCounter.getCount() == m_parameters->getSeriesShotCount()) {
                CLOGD("LLS: lls_tuning_set_index(%d)",
                        shot_ext->shot.dm.stats.vendor_lls_tuning_set_index);
                CLOGD("LLS: lls_brightness_index(%d)",
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

                CLOGD("wait for DNG bayer buffer copy complete");
                do {
                    dngBayerBuffer.index = -2;
                    ret = m_dngCaptureDoneQ->waitAndPopProcessQ(&dngBayerBuffer);
                    retry++;
                    if (retry % 3 == 0) {
                        CLOGW(" bayer copy for DNG not still complete, ret(%d), retry(%d)",
                                 ret, retry);
                    }
                } while (ret == TIMED_OUT && retry < 200);

                if (ret < 0) {
                    CLOGE("failed to wait for dng bayer copy done, ret(%d), retry(%d)",
                             ret, retry);
                }

#ifdef SAMSUNG_DNG_DIRTY_BAYER
                /* put pure bayer buffer */
                ret = m_putBuffers(m_fliteBufferMgr, dngBayerBuffer.index);
                if (ret < 0) {
                    CLOGE(" VC0 src putBuffer fail, index(%d), ret(%d)",
                        dngBayerBuffer.index, ret);
                    goto CLEAN;
                }
#endif
            }
#endif
            /* put bayer buffer */
            ret = m_putBuffers(m_bayerBufferMgr, bayerBuffer.index);
            if (ret < 0) {
                CLOGE(" 3AA src putBuffer fail, index(%d), ret(%d)",
                         bayerBuffer.index, ret);
                goto CLEAN;
            }
#ifdef LLS_REPROCESSING
            m_captureSelector->setIsConvertingMeta(true);
#endif
        }

        /* put isp buffer */
        if (m_parameters->getUsePureBayerReprocessing() == true
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
#ifdef USE_ODC_CAPTURE
            if (m_parameters->getODCCaptureEnable()) {
                /* Release buffer in m_processODC() */
            } else
#endif
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

                ret = m_putBuffers(m_sccReprocessingBufferMgr, yuvBuffer.index);
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

#ifdef SAMSUNG_DNG
            if (m_parameters->getDNGCaptureModeOn()
                && m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_BURST) {
                dng_thumbnail_t *dngThumbnailBuf = NULL;
                unsigned int thumbBufSize = 0;
                int maxThumbnailW = 0, maxThumbnailH = 0;
                struct camera2_dm *dm = new struct camera2_dm;

                m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);
                newFrame->getDynamicMeta(dm);

                dngThumbnailBuf = m_parameters->createDngThumbnailBuffer(maxThumbnailW * maxThumbnailH * 2);
                if (dngThumbnailBuf->buf && yuvBuffer.addr[0]) {
                    memcpy(dngThumbnailBuf->buf, yuvBuffer.addr[0], yuvBuffer.size[0]);
                    dngThumbnailBuf->size = yuvBuffer.size[0];

                    if (m_parameters->getCaptureExposureTime() > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT) {
                        dngThumbnailBuf->frameCount = 0;
                    } else {
                        dngThumbnailBuf->frameCount = dm->request.frameCount;
                    }

                    m_parameters->putDngThumbnailBuffer(dngThumbnailBuf);

                    CLOGD("[DNG] Thumbnail size:%d, fcount %d",
                            dngThumbnailBuf->size, dngThumbnailBuf->frameCount);
                } else {
                    CLOGE("[DNG] buffer is NULL(src:%p, dst:%p)",
                             yuvBuffer.addr[0], dngThumbnailBuf->buf);
                }
                delete dm;
            }
#endif
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
            CLOGD(" Reprocessing frame delete(%d)",
                     newFrame->getFrameCount());
            newFrame = NULL;
        }
    } while ((ret == TIMED_OUT || isOldFrame == true) && retryIsp < 200 && m_flagThreadStop != true);

#ifdef USE_DUAL_CAMERA
reprocessing_internal_frame:
#endif

#ifdef USE_REMOSAIC_CAPTURE
    if (m_parameters->getSwitchSensor() == true) {
        CLOGD("Run sensorModeSwitchThread");
        m_sensorModeSwitchThread->run();
    }
#endif //USE_REMOSAIC_CAPTURE

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

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (m_parameters->getDNGCaptureModeOn() == true) {
            ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer,
                m_pictureFrameFactory->getNodeType(PIPE_VC0));

            if (bayerBuffer.index != -2 && m_fliteBufferMgr != NULL) {
                ret = m_putBuffers(m_fliteBufferMgr, bayerBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to put DepthMap buffer to bufferMgr");
                }
            }
        }
#endif

        newFrame = NULL;
    }

CLEAN:
#ifdef USE_REMOSAIC_CAPTURE
    m_sensorModeSwitchThread->join();
    if (m_parameters->getSwitchSensor() == true) {
        CLOGD("Run sensorModeSwitchThread at CLEAN");
        m_sensorModeSwitchThread->run();
    }
#endif

    if (newFrame != NULL) {
        bayerBuffer.index = -2;
        ret = m_getBufferManager(bayerPipeId, &bufferMgr, SRC_BUFFER_DIRECTION);
        if (ret < 0)
            CLOGE("getBufferManager fail, ret(%d)", ret);

#ifdef SAMSUNG_DNG
        if (m_DNGCaptureThread->isRunning() == true) {
            CLOGI(" wait m_DNGCaptureThread");
            m_DNGCaptureThread->requestExitAndWait();
        }

#ifdef SAMSUNG_DNG_DIRTY_BAYER
        if (m_parameters->getDNGCaptureModeOn() == true) {
            ret = newFrame->getDstBuffer(bayerPipeId, &bayerBuffer,
                m_pictureFrameFactory->getNodeType(PIPE_VC0));

            if (bayerBuffer.index != -2 && m_fliteBufferMgr != NULL) {
                ret = m_putBuffers(m_fliteBufferMgr, bayerBuffer.index);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to put DepthMap buffer to bufferMgr");
                }
            }
        }
#endif
#endif
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

        if (m_parameters->getUsePureBayerReprocessing() == true) {
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
#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
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

#ifdef OIS_CAPTURE
    if (m_parameters->getOISCaptureModeOn() == true && m_reprocessingCounter.getCount() == 0) {
        m_parameters->setOISCaptureModeOn(false);
    }
#endif

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
    int retryCountGSC = 4;
#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    int retryCountSync = 4;
    int retryCountFusion = 4;
#endif

    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ExynosCameraBuffer sccReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    struct camera2_stream *shot_stream = NULL;
    ExynosRect srcRect, dstRect;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    float zoomRatio = m_parameters->getZoomRatio(0) / 1000;

    sccReprocessingBuffer.index = -2;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;
    int bayerPipeId = 0;
    bool isSrc = false;

    /* wait SCC */
    CLOGI("wait SCC output");
    int retry = 0;
    do {
        ret = m_dstSccReprocessingQ->waitAndPopProcessQ(&newFrame);
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
    bayerPipeId = newFrame->getFirstEntity()->getPipeId();

    if (m_parameters->getUsePureBayerReprocessing() == true
        && m_parameters->isReprocessing3aaIspOTF() == true
        && m_parameters->isUseHWFC() == true
        && m_parameters->getHWFCEnable() == true) {
        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            goto CLEAN;
        }

        ret = newFrame->setEntityState(bayerPipeId, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)", bayerPipeId, ret);

            return ret;
        }

        newFrame->setMetaDataEnable(true);
    }

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
                CLOGE("Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                    getCameraIdInternal(), m_parameters->isReprocessing());
                break;
        }
    }

    /*
     * When Non-reprocessing scenario does not setEntityState,
     * because Non-reprocessing scenario share preview and capture frames
     */
    if (m_parameters->isReprocessing() == true) {
        ret = newFrame->setEntityState(pipeId_scc, ENTITY_STATE_COMPLETE);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_COMPLETE) fail, pipeId(%d), ret(%d)", pipeId_scc, ret);
            return ret;
        }
    }

    CLOGI("SCC output done, frame Count(%d)", newFrame->getFrameCount());

#ifdef USE_DUAL_CAMERA
    /* in case of sync mode, do processing sync->fusion */
    if (m_parameters->getDualCameraMode() == true &&
        newFrame->getSyncType() == SYNC_TYPE_SYNC) {
        int picturePipeId = PIPE_MCSC3_REPROCESSING;
        getDualCameraId(&masterCameraId, &slaveCameraId);

        /* define picture buffer pipeID (mcsc1 or mcsc3) */
        if (newFrame->getRequest(PIPE_MCSC3_REPROCESSING) == true) {
            picturePipeId = PIPE_MCSC3_REPROCESSING;
        } else if (newFrame->getRequest(PIPE_MCSC1_REPROCESSING) == true) {
            picturePipeId = PIPE_MCSC1_REPROCESSING;
        } else {
            CLOGE("invalid frame(%d) request!!", newFrame->getFrameCount());
            goto CLEAN;
        }

        CLOGI("frame(%d), pipe(%d), nodeIndex(%d) request!!", newFrame->getFrameCount(),
                picturePipeId, m_reprocessingFrameFactory->getNodeType(picturePipeId));

        /* 1. push the frame to sync pipe [prepareReprocessingBeforeSyncPipe] */
        ret = m_prepareBeforeSync(pipeId_scc, PIPE_SYNC_REPROCESSING, newFrame,
                m_reprocessingFrameFactory->getNodeType(picturePipeId),
                m_reprocessingFrameFactory, m_syncReprocessingQ, true);
        if (ret != NO_ERROR) {
            CLOGE("m_prepareBeforeSync(pipeId(%d/%d), frameCount(%d), dstPos(%d), isReprocessing(%d) fail",
                    pipeId_scc, PIPE_SYNC_REPROCESSING, newFrame->getFrameCount(),
                    m_reprocessingFrameFactory->getNodeType(picturePipeId), true);
            goto CLEAN;
        }

        /* 2. in slave camera, all job for reprocesing is complete */
        if (m_cameraId == slaveCameraId) {
            m_captureLock.lock();
            m_pictureEnabled = false;
            m_captureLock.unlock();

            m_yuvcallbackCounter.decCount();
            m_pictureCounter.decCount();
            m_jpegCounter.decCount();
            CLOGD(" Picture frame delete(%d)", newFrame->getFrameCount());

            newFrame->frameUnlock();

            ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret != NO_ERROR) {
                CLOGE("Failed to removeFrameFromList. framecount %d ret %d",
                        newFrame->getFrameCount(), ret);
            }

            newFrame = NULL;
            goto CLEAN;
        } else if (m_cameraId == masterCameraId &&
                newFrame->getFrameType() == FRAME_TYPE_INTERNAL) {
            newFrame->frameUnlock();

            ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret < 0) {
                CLOGE("remove frame from processList fail, ret(%d)", ret);
            }
        }

        /* 3. in master camera, wait for sync Frame */
        CLOGI("wait Sync output");
        while (retryCountSync > 0) {
            retryCountSync--;

            ret = m_syncReprocessingQ->waitAndPopProcessQ(&newFrame);
            if (ret == TIMED_OUT) {
                CLOGW("m_syncReprocessingQ(frame(%d)) wait and pop timeout, ret(%d)",
                        newFrame->getFrameCount(), ret);
                m_reprocessingFrameFactory->startThread(PIPE_SYNC_REPROCESSING);
                if (retryCountSync <= 0) {
                    CLOGE("m_syncReprocessingQ(frame(%d)) retry count was over, ret(%d)",
                            newFrame->getFrameCount(), ret);
                    goto CLEAN;
                }
            } else if (ret < 0) {
                CLOGE("m_syncReprocessingQ(frame(%d)) wait and pop fail, ret(%d)",
                        newFrame->getFrameCount(), ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            } else if (newFrame != NULL &&
                    m_cameraId == masterCameraId &&
                    newFrame->getCameraId() == masterCameraId &&
                    newFrame->getFrameState() == FRAME_STATE_SKIPPED) {
                newFrame->frameUnlock();

                ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
                if (ret < 0) {
                    CLOGE("remove frame from processList fail, ret(%d)", ret);
                }

                continue;
            } else {
                break;
            }
        }

        if (newFrame == NULL) {
            CLOGE("newFrame is NULL");
            goto CLEAN;
        }

        /* 3. prepare the frame for fusion */
        ret = m_prepareAfterSync(PIPE_SYNC_REPROCESSING, PIPE_FUSION_REPROCESSING, newFrame,
                m_reprocessingFrameFactory->getNodeType(picturePipeId),
                m_reprocessingFrameFactory, m_fusionReprocessingQ, true);
        if (ret != NO_ERROR) {
            CLOGE("m_prepareAfterSync(pipeId(%d/%d), frameCount(%d), isReprocessing(%d) fail",
                    PIPE_SYNC_REPROCESSING, PIPE_FUSION_REPROCESSING, newFrame->getFrameCount(), true);
            goto CLEAN;
        }

        if (newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            /* 4. in master camera, wait for fusion Frame */
            CLOGI("wait Fusion output");
            while (retryCountFusion > 0) {
                retryCountFusion--;

                ret = m_fusionReprocessingQ->waitAndPopProcessQ(&newFrame);
                if (ret == TIMED_OUT) {
                    CLOGW("m_fusionReprocessingQ(frame(%d)) wait and pop timeout, retry(%d), ret(%d)",
                            newFrame->getFrameCount(), retryCountFusion, ret);
                    m_reprocessingFrameFactory->startThread(PIPE_FUSION_REPROCESSING);
                    if (retryCountFusion <= 0) {
                        CLOGE("m_fusionReprocessingQ(frame(%d)) retry count was over, ret(%d)",
                                newFrame->getFrameCount(), ret);
                        goto CLEAN;
                    }
                } else if (ret < 0) {
                    CLOGE("m_fusionReprocessingQ(frame(%d)) wait and pop fail, ret(%d)",
                            newFrame->getFrameCount(), ret);
                    /* TODO: doing exception handling */
                    goto CLEAN;
                } else {
                    break;
                }
            }

            /* 5. release source buffer and move the buffer to original dst position */
            ret = m_prepareAfterFusion(PIPE_FUSION_REPROCESSING, newFrame,
                    m_reprocessingFrameFactory->getNodeType(picturePipeId), true);
            if (ret != NO_ERROR) {
                CLOGE("m_prepareAfterFusion(pipeId(%d), frameCount(%d), isReprocessing(%d) fail",
                        PIPE_FUSION_REPROCESSING, newFrame->getFrameCount(), true);
                goto CLEAN;
            }
        }
    }
#endif

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
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
    }
#endif

    /* Shutter Callback */
    if (m_parameters->isReprocessing() == false) {
        if (m_parameters->msgTypeEnabled(CAMERA_MSG_SHUTTER)) {
            CLOGI(" CAMERA_MSG_SHUTTER callback S");
#ifdef BURST_CAPTURE
            m_notifyCb(CAMERA_MSG_SHUTTER, m_parameters->getSeriesShotDuration(), 0, m_callbackCookie);
#else
            m_notifyCb(CAMERA_MSG_SHUTTER, 0, 0, m_callbackCookie);
#endif
            CLOGI(" CAMERA_MSG_SHUTTER callback E");
        }
    }

#ifdef SAMSUNG_DOF
    if (m_parameters->getShotMode() == SHOT_MODE_OUTFOCUS &&
            m_parameters->getMoveLensTotal() > 0) {
        CLOGD("[DOF] Lens moved count: %d",
                m_lensmoveCount);
        CLOGD("[DOF] Total count : %d",
                m_parameters->getMoveLensTotal());

        ExynosCameraActivityAutofocus *autoFocusMgr = m_exynosCameraActivityControl->getAutoFocusMgr();

        if(m_lensmoveCount < m_parameters->getMoveLensTotal()) {
            CLOGD("[DOF] callback done!!: %d",
                m_lensmoveCount);
            autoFocusMgr->setStartLensMove(true);
            m_lensmoveCount++;
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->m_setLensPosition(m_lensmoveCount);
        }
        else if(m_lensmoveCount == m_parameters->getMoveLensTotal() ){
            CLOGD("[DOF] Last callback done!!: %d",
                m_lensmoveCount);
            autoFocusMgr->setStartLensMove(false);

            m_lensmoveCount = 0;
            m_parameters->setMoveLensCount(m_lensmoveCount);
            m_parameters->setMoveLensTotal(m_lensmoveCount);
        }else {
            CLOGE("[DOF] unexpected error!!: %d",
                m_lensmoveCount);
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
            CLOGD(" m_ThumbnailCallbackThread join");
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
        int postPicturePipeId = PIPE_MCSC1_REPROCESSING;
        if (newFrame->getRequest(postPicturePipeId) == true) {
            m_dstPostPictureGscQ->pushProcessQ(&newFrame);

            ret = m_postPictureCallbackThread->run();
            if (ret < 0) {
                CLOGE("couldn't run magicshot thread, ret(%d)", ret);
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
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                     pipeId_scc, ret);
            goto CLEAN;
        }

        shot_stream = (struct camera2_stream *)(sccReprocessingBuffer.addr[sccReprocessingBuffer.getMetaPlaneIndex()]);
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
                ret = m_setupEntity(pipeId_gsc, newFrame, &sccReprocessingBuffer, NULL);
            } else {
                /* wait available SCC buffer */
                usleep(WAITING_TIME);
            }

            if (retry % 10 == 0) {
                CLOGW("retry setupEntity for GSC postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                        m_postPictureQ->getSizeOfProcessQ(),
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
#if defined(SR_CAPTURE)
        if (m_parameters->getSROn() &&
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
            CLOGD("[SR] size (%d, %d)", m_sr_cropped_width, m_sr_cropped_height);
        }
#endif

        /* put SCC buffer */
        if (m_parameters->isReprocessing() == true)
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_reprocessingFrameFactory->getNodeType(PIPE_ISPC_REPROCESSING));
        else
            ret = newFrame->getDstBuffer(pipeId_scc, &sccReprocessingBuffer, m_previewFrameFactory->getNodeType(PIPE_ISPC));

        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_scc, ret);
            goto CLEAN;
        }

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            ret = m_putFusionReprocessingBuffers(newFrame, &sccReprocessingBuffer);
            if (ret < 0) {
                CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                        sccReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            }
        } else {
#endif
        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        ret = m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("m_putBuffers fail, ret(%d)", ret);
            /* TODO: doing exception handling */
            goto CLEAN;
        }
#ifdef USE_DUAL_CAMERA
        }
#endif
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_parameters->getLDCaptureMode() > 0) {
        CLOGD(" m_LDCaptureQ push mode(%d)",
                 m_parameters->getLDCaptureMode());
        m_LDCaptureQ->pushProcessQ(&newFrame);
    } else
#endif
    {
        /* push postProcess */
        m_postPictureQ->pushProcessQ(&newFrame);
    }

    m_pictureCounter.decCount();

    CLOGI("picture thread complete, remaining count(%d)", m_pictureCounter.getCount());

    if (m_pictureCounter.getCount() > 0) {
        loop = true;
    } else {
        if (m_parameters->isReprocessing() == true) {
            CLOGD("Reprocessing ");
            if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_PURE_DYNAMIC)
                m_previewFrameFactory->setRequest(PIPE_VC0, false);
            else if (m_parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
                m_previewFrameFactory->setRequest(PIPE_3AC, false);
        } else {
            if (m_parameters->getUseDynamicScc() == true) {
                CLOGD(" Use dynamic bayer");

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
        CLOGD(" putBuffer sccReprocessingBuffer(index:%d) in error state",
                 sccReprocessingBuffer.index);
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            ret = m_putFusionReprocessingBuffers(newFrame, &sccReprocessingBuffer);
            if (ret < 0) {
                CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                        sccReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
            }
        } else {
#endif
        m_getBufferManager(pipeId_scc, &bufferMgr, DST_BUFFER_DIRECTION);
        m_putBuffers(bufferMgr, sccReprocessingBuffer.index);
#ifdef USE_DUAL_CAMERA
        }
#endif
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

            memset(filePath, 0, sizeof(filePath));

#if defined(SAMSUNG_INF_BURST)
            snprintf(filePath, sizeof(filePath), "%s%s_%03d.jpg",
                    m_burstSavePath, m_burstTime, seriesShotNumber);
#else
            snprintf(filePath, sizeof(filePath), "%sBurst%02d.jpg", m_burstSavePath, seriesShotNumber);
#endif
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
    int retryCountJPEG = 4;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSP_sptr_t yuvFrame = NULL;
    ExynosCameraFrameSP_sptr_t jpegFrame = NULL;

    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBuffer jpegReprocessingBuffer;

    gscReprocessingBuffer.index = -2;
    jpegReprocessingBuffer.index = -2;

    int pipeId_gsc = 0;
    int pipeId_jpeg = 0;
    int pipeId = 0;

    int currentSeriesShotMode = 0;
    bool IsThumbnailCallback = false;
#if 0
    bool needIonSync = false;
#endif

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
#ifdef USE_REMOSAIC_CAPTURE
            } else if (m_parameters->getRemosaicCaptureMode() == ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_PURE_BAYER) {
                pipeId_gsc = PIPE_3AA_REPROCESSING;
#endif
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

    CLOGI("wait postPictureQ output");
    ret = m_postPictureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC)
        pipeId_gsc = PIPE_FUSION_REPROCESSING;
#endif

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD(" Picture canceled");
        goto CLEAN;
    }

    CLOGI("postPictureQ output done");

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
        CLOGD(" MULTI_FRAME_SHOT_BV_INFO BV value %u",
                 m_picture_meta_shot->udm.awb.vendorSpecific[11]);
        m_notifyCb(MULTI_FRAME_SHOT_BV_INFO, m_picture_meta_shot->udm.awb.vendorSpecific[11], 0, m_callbackCookie);
    }
#endif /* LLS_REPROCESSING */

#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCEnable()) {
        int src_PipeId = PIPE_MCSC1_REPROCESSING;
        ExynosCameraBuffer srcReprocessingBuffer;
        ExynosCameraBufferManager *srcBufferMgr = NULL;
        int dstbufferIndex = -2;
        int retry = 0;
        ExynosCameraDurationTimer m_timer;
        long long durationTime = 0;

        bufferMgr = m_lensDCBufferMgr;

        CLOGI("[DC]getLensDCEnable(%d)", m_parameters->getLensDCEnable());
        ret = m_getBufferManager(src_PipeId, &srcBufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                     src_PipeId, ret);
            goto CLEAN;
        }

        ret = newFrame->getDstBuffer(pipeId_gsc, &srcReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(src_PipeId));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                     pipeId_gsc, ret);
            goto CLEAN;
        }

        do {
            dstbufferIndex = -2;
            retry++;

            if (m_lensDCBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_lensDCBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &gscReprocessingBuffer);
                if (ret != NO_ERROR)
                    CLOGE("getBuffer fail, ret(%d)", ret);
            }

            if (dstbufferIndex < 0) {
                usleep(WAITING_TIME);
                if (retry % 20 == 0) {
                    CLOGW("retry m_lensDCBuffer getBuffer)");
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

        CLOGD("[DC]:-- Start Library --");
        m_timer.start();
        if (m_DCpluginHandle != NULL) {
            UniPluginExtraBufferInfo_t extraData;
            memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));

            extraData.brightnessValue.snum = (int)m_picture_meta_shot->udm.ae.vendorSpecific[5];
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
                CLOGE("[DC] DC Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!");
            }

            ret = uni_plugin_set(m_DCpluginHandle,
                    LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
            if(ret < 0) {
                CLOGE("[DC] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
            }

            ret = uni_plugin_process(m_DCpluginHandle);
            if (ret < 0) {
                CLOGE("[DC] LLS Plugin process failed!!");
            }

            UTstr debugData;
            debugData.data = new unsigned char[LDC_EXIF_SIZE];

            if (debugData.data != NULL) {
                ret = uni_plugin_get(m_DCpluginHandle,
                        LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
                if (ret < 0) {
                    CLOGE("[DC] DC Plugin get UNI_PLUGIN_INDEX_DEBUG_INFO failed!!");
                }
                CLOGD("[DC] Debug buffer size: %d", debugData.size);
                m_parameters->setLensDCdebugInfo(debugData.data, debugData.size);

                delete []debugData.data;
            }
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[DC]duration time(%5d msec)", (int)durationTime);
        CLOGD("[DC]-- end Library --");

        ret = m_putBuffers(srcBufferMgr, srcReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("srcbufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                     src_PipeId, ret);
            goto CLEAN;
        }
    } else
#endif
#ifdef SAMSUNG_STR_CAPTURE
    if (m_parameters->getSTRCaptureEnable()) {
        ExynosCameraBuffer srcReprocessingBuffer;
        ExynosCameraBufferManager *srcBufferMgr = NULL;
        int dstbufferIndex = -2;
        int retry = 0;

        pipeId = PIPE_MCSC1_REPROCESSING;
        bufferMgr = m_strCaptureBufferMgr;

        CLOGI("[STR_CAPTURE]getSTRCaptureEnable(%d)", m_parameters->getSTRCaptureEnable());
        ret = m_getBufferManager(pipeId, &srcBufferMgr, DST_BUFFER_DIRECTION);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE]getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                     pipeId, ret);
            goto CLEAN;
        }

        ret = newFrame->getDstBuffer(pipeId_gsc, &srcReprocessingBuffer,
                                        m_reprocessingFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("[STR_CAPTURE]getDstBuffer fail, pipeId(%d), ret(%d)",
                     pipeId_gsc, ret);
            goto CLEAN;
        }

        do {
            dstbufferIndex = -2;
            retry++;

            if (m_strCaptureBufferMgr->getNumOfAvailableBuffer() > 0) {
                ret = m_strCaptureBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &gscReprocessingBuffer);
                if (ret != NO_ERROR)
                    CLOGE("[STR_CAPTURE]getBuffer fail, ret(%d)", ret);
            }

            if (dstbufferIndex < 0) {
                usleep(WAITING_TIME);
                if (retry % 20 == 0) {
                    CLOGW("[STR_CAPTURE]retry m_strCaptureBuffer getBuffer)");
                    m_strCaptureBufferMgr->dump();
                }
            }
        } while (dstbufferIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_flagThreadStop != true && m_stopBurstShot == false);

        CLOGD("[STR_CAPTURE]:-- Start Library --");
        m_processSTRCapture(srcReprocessingBuffer.addr[0], srcReprocessingBuffer.addr[0]);
        memcpy(gscReprocessingBuffer.addr[0], srcReprocessingBuffer.addr[0], srcReprocessingBuffer.size[0]);
        CLOGD("[STR_CAPTURE]-- end Library --");

        ret = m_putBuffers(srcBufferMgr, srcReprocessingBuffer.index);
        if (ret < 0) {
            CLOGE("[STR_CAPTURE]srcbufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                     pipeId, ret);
            goto CLEAN;
        }
    } else
#endif
    {
        if (m_parameters->getOutPutFormatNV21Enable()) {
            pipeId = PIPE_MCSC1_REPROCESSING;
            ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                goto CLEAN;
            }
        }
#ifdef USE_DUAL_CAMERA
        else if (m_parameters->getDualCameraMode() == true &&
                newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            pipeId = PIPE_MCSC1_REPROCESSING;
        }
#endif
        else if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
            pipeId = PIPE_MCSC3_REPROCESSING;
            ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                         pipeId_gsc, ret);
                goto CLEAN;
            }
        }
#ifdef USE_ODC_CAPTURE
        else if (m_parameters->getODCCaptureEnable()) {
            pipeId = PIPE_MCSC3_REPROCESSING;
            ret = m_getBufferManager(pipeId, &bufferMgr, DST_BUFFER_DIRECTION);
            if (ret < 0) {
                CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                goto CLEAN;
            }
        }
#endif
        ret = newFrame->getDstBuffer(pipeId_gsc, &gscReprocessingBuffer,
                m_reprocessingFrameFactory->getNodeType(pipeId));
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                     pipeId_gsc, ret);
            goto CLEAN;
        }
    }

#if defined(SAMSUNG_LLS_DEBLUR) || defined(SAMSUNG_LENS_DC) || defined(SAMSUNG_STR_CAPTURE)
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
#ifdef SAMSUNG_STR_CAPTURE
        if (m_parameters->getSTRCaptureEnable()) {
            IsThumbnailCallback = true;
        }
#endif
    }
#endif

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getSamsungCamera()
        && m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
        if (m_parameters->getODCCaptureEnable() == true) {
            IsThumbnailCallback = true;
        }
    }
#endif

#if 0
    /* Temp code for cache flush issue (M2M case) */
#ifdef SAMSUNG_LLS_DEBLUR
    if (m_parameters->getLDCaptureMode()) {
        needIonSync = true;
    }
#endif
#ifdef SAMSUNG_LENS_DC
    if (m_parameters->getLensDCEnable()) {
        needIonSync = true;
    }
#endif
#ifdef SAMSUNG_STR_CAPTURE
    if (m_parameters->getSTRCaptureEnable()) {
        needIonSync = true;
    }
#endif

    if (needIonSync) {
        int hwPreviewFormat = m_parameters->getHwPreviewFormat();
        int planeCount = getYuvPlaneCount(hwPreviewFormat);
        /* TODO : have to consider all fmt(planes) and stride */
        for (int plane = 0; plane < planeCount; plane++) {
            if (m_ionClient >= 0)
#ifdef USE_LIB_ION_LEGACY
                ion_sync_fd(m_ionClient, gscReprocessingBuffer.fd[plane]);
#else
                exynos_ion_sync_fd(m_ionClient, gscReprocessingBuffer.fd[plane]);
#endif
        }
    }
#endif

    if (IsThumbnailCallback) {
        m_parameters->setIsThumbnailCallbackOn(true);
        m_ThumbnailCallbackThread->run();
        m_thumbnailCallbackQ->pushProcessQ(&gscReprocessingBuffer);
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
        snprintf(filePath, sizeof(filePath), "/data/media/0/pic_%d.yuv", gscReprocessingBuffer.index);

        dumpToFile((char *)filePath, gscReprocessingBuffer.addr[0], width * height);
    }
#endif

    /* callback */
    if (m_parameters->isUseHWFC() == false || m_parameters->getHWFCEnable() == false) {
        ret = m_reprocessingYuvCallbackFunc(gscReprocessingBuffer);
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
        if (m_hdrEnabled == true
            || currentSeriesShotMode == SERIES_SHOT_MODE_LLS
            || currentSeriesShotMode == SERIES_SHOT_MODE_SIS
            || m_nv21CaptureEnabled
            || m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) {
            CLOGD(" HDR callback");

#ifdef USE_DUAL_CAMERA
            if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
                pipeId = PIPE_FUSION_REPROCESSING;
                yuvFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId, newFrame->getFrameCount(), 0, newFrame);
            } else
#endif
            {
                yuvFrame = m_reprocessingFrameFactory->createNewFrameOnlyOnePipe(pipeId, newFrame->getFrameCount());
            }
            if (yuvFrame == NULL) {
                CLOGE("frame is NULL(yuvFrame)");
                goto CLEAN;
            }

            ret = yuvFrame->setDstBuffer(pipeId, gscReprocessingBuffer);
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

                ret = yuvFrame->setSrcBuffer(pipeId, depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                yuvFrame->setRequest(PIPE_VC1, true);
            }
#endif
            m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, m_picture_meta_shot);

            m_yuvCallbackQ->pushProcessQ(&yuvFrame);
            m_jpegCounter.decCount();
        } else {
            CLOGI("pipeId(%d), pipeId_gsc(%d) hwfc(%d, %d)",
                    pipeId, pipeId_gsc, m_parameters->isUseHWFC(), m_parameters->getHWFCEnable());

            if (m_parameters->isUseHWFC() == true && m_parameters->getHWFCEnable() == true
#ifdef USE_DUAL_CAMERA
                && !(m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC)
#endif
                    ) {
                /* get gsc dst buffers */
                entity_buffer_state_t jpegMainBufferState = ENTITY_BUFFER_STATE_NOREQ;
                ret = newFrame->getDstBufferState(pipeId_gsc, &jpegMainBufferState,
                                                    m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                if (ret < 0) {
                    CLOGE("getDstBufferState fail, pipeId(%d), nodeType(%d), ret(%d)",
                            pipeId_gsc,
                            m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING),
                            ret);
                    goto CLEAN;
                }

                if (jpegMainBufferState == ENTITY_BUFFER_STATE_ERROR) {
                    ret = newFrame->getDstBuffer(pipeId_gsc, &jpegReprocessingBuffer,
                                                    m_reprocessingFrameFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
                        goto CLEAN;
                    }

                    /* put JPEG callback buffer */
                    if (m_jpegBufferMgr->putBuffer(jpegReprocessingBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE) != NO_ERROR)
                        CLOGE("putBuffer(%d) fail", jpegReprocessingBuffer.index);

                    CLOGE("Failed to get the encoded JPEG buffer");
                    goto CLEAN;
                }

#ifdef SAMSUNG_BD
                if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_BURST) {
                    UTstr bdInfo = {NULL, 0};
                    ret = m_BDbufferQ->waitAndPopProcessQ(&bdInfo);
                    if(ret == TIMED_OUT || ret < 0) {
                        CLOGE("[BD]wait and pop fail, ret(%d)", ret);
                    }
                    else {
                        CLOGD("[BD]: Pop the buffer(size: %d)", bdInfo.size);
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
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
                    goto CLEAN;
                }

                /* in case OTF until JPEG, we should be call below function to this position
                                in order to update debugData from frame. */
                m_parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, m_picture_meta_shot, true);

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
                                    m_parameters->getDebug2Attribute());
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
                        CLOGI("m_pictureEnable is false");
                        goto CLEAN;
                    }
                    if (m_jpegBufferMgr->getNumOfAvailableBuffer() > 0) {
                        ret = m_jpegBufferMgr->getBuffer(&bufIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &jpegReprocessingBuffer);
                        if (ret != NO_ERROR)
                            CLOGE("getBuffer fail, ret(%d)", ret);
                    }

                    if (bufIndex < 0) {
                        usleep(WAITING_TIME);

                        if (retry % 20 == 0) {
                            CLOGW("retry JPEG getBuffer(%d) postPictureQ(%d), saveQ0(%d), saveQ1(%d), saveQ2(%d)",
                                     bufIndex,
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
#ifdef USE_DUAL_CAMERA
                    if (m_parameters->getDualCameraMode() == true &&
                            pipeId_gsc == PIPE_FUSION_REPROCESSING) {
                        ret = m_putFusionReprocessingBuffers(newFrame, &gscReprocessingBuffer);
                        if (ret < 0) {
                            CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                                    gscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                            /* TODO: doing exception handling */
                            goto CLEAN;
                        }
                    } else {
#endif
                    ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
                    goto CLEAN;
#ifdef USE_DUAL_CAMERA
                    }
#endif
                }

                /* 2. setup Frame Entity */
                ret = m_setupEntity(pipeId_jpeg, newFrame, &gscReprocessingBuffer, &jpegReprocessingBuffer);
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
                if (m_parameters->isReprocessing() == true) {
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
#ifdef USE_DUAL_CAMERA
                if (m_parameters->getDualCameraMode() == true &&
                        pipeId_gsc == PIPE_FUSION_REPROCESSING) {
                    ret = m_putFusionReprocessingBuffers(newFrame, &gscReprocessingBuffer);
                    if (ret < 0) {
                        CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                                gscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                        /* TODO: doing exception handling */
                        goto CLEAN;
                    }
                } else {
#endif
                ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
                if (ret < 0) {
                    CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                             pipeId_gsc, ret);
                    goto CLEAN;
                }
#ifdef USE_DUAL_CAMERA
                }
#endif
            }

            int jpegOutputSize = newFrame->getJpegSize();
            if (jpegOutputSize <= 0) {
                jpegOutputSize = jpegReprocessingBuffer.size[0];
                if (jpegOutputSize <= 0)
                    CLOGW(" jpegOutput size(%d) is invalid", jpegOutputSize);
            }

            CLOGI("Jpeg output done, jpeg size(%d)", jpegOutputSize);

            jpegReprocessingBuffer.size[0] = jpegOutputSize;

#ifdef BURST_CAPTURE
            m_burstCaptureCallbackCount++;
            CLOGI(" burstCaptureCallbackCount(%d)", m_burstCaptureCallbackCount);
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
                CLOGE("frame is NULL");
                goto CLEAN;
            }

            ret = jpegFrame->setDstBuffer(pipeId_jpeg, jpegReprocessingBuffer);
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
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true &&
                pipeId_gsc == PIPE_FUSION_REPROCESSING) {
            ret = m_putFusionReprocessingBuffers(newFrame, &gscReprocessingBuffer);
            if (ret < 0) {
                CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                        gscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                /* TODO: doing exception handling */
                goto CLEAN;
            }
        } else {
#endif
        if (gscReprocessingBuffer.index >= 0) {
            ret = m_putBuffers(bufferMgr, gscReprocessingBuffer.index);
            if (ret < 0) {
                CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                         pipeId_gsc, ret);
                goto CLEAN;
            }
        }
#ifdef USE_DUAL_CAMERA
        }
#endif

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

#ifdef LLS_REPROCESSING
        if (m_captureSelector->getIsLastFrame() == true)
#endif
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

#ifdef ONE_SECOND_BURST_CAPTURE
        if (!(currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST ||
            currentSeriesShotMode == SERIES_SHOT_MODE_BURST))
#else
        if (currentSeriesShotMode != SERIES_SHOT_MODE_BURST)
#endif
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
    while (m_postPictureQ->getSizeOfProcessQ() == 0 && 0 < waitCount && m_jpegCounter.getCount() != 0) {
        usleep(10000);
        waitCount--;
    }

    if (m_postPictureQ->getSizeOfProcessQ() > 0
        || m_jpegCounter.getCount() > 0) {
        CLOGD("postPicture thread will run again. ShotMode(%d), postPictureQ(%d), Remaining(%d)",
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
    CLOGI("");

    int ret = 0;
    int curThreadNum = -1;
    char burstFilePath[CAMERA_FILE_PATH_SIZE];

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
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
#if defined(SAMSUNG_INF_BURST)
            {
                time_t rawtime;
                struct tm *timeinfo;

                time(&rawtime);
                timeinfo = localtime(&rawtime);
                if(seriesShotNumber == 1) {
                    if(m_preBurstTimeinfo.tm_hour == timeinfo->tm_hour
                        && m_preBurstTimeinfo.tm_min == timeinfo->tm_min
                        && m_preBurstTimeinfo.tm_sec == timeinfo->tm_sec) {
                        timeinfo->tm_sec = timeinfo->tm_sec + 1;
                    }

                    m_preBurstTimeinfo.tm_hour = timeinfo->tm_hour;
                    m_preBurstTimeinfo.tm_min = timeinfo->tm_min;
                    m_preBurstTimeinfo.tm_sec = timeinfo->tm_sec;
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

    pipeId = PIPE_MCSC1_REPROCESSING;

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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC)
        pipeId = PIPE_FUSION_REPROCESSING;
#endif

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
#ifdef SAMSUNG_LLS_DEBLUR
        && m_parameters->getLDCaptureMode() == 0
#endif
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
#ifdef SAMSUNG_TN_FEATURE
    m_MetadataExt.ext_data.usage = EXT_DATA_EXIF_DEBUG_INFO;
    m_MetadataExt.ext_data.size = debug->debugSize[APP_MARKER_4];
    m_MetadataExt.ext_data.data = (void *)debug->debugData[APP_MARKER_4];
#endif

#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_MetadataExt.timestamp = newFrame->getTimeStampBoot();
#else
    m_MetadataExt.timestamp = newFrame->getTimeStamp();
#endif
    CLOGV(" timestamp:%lldms!", m_MetadataExt.timestamp);
#endif

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
#ifdef SAMSUNG_LLS_DEBLUR
        && m_parameters->getLDCaptureMode() == 0
#endif
        ) {
        m_ThumbnailCallbackThread->join();
        CLOGD(" m_ThumbnailCallbackThread join");
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
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            ret = m_putFusionReprocessingBuffers(newFrame, &gscReprocessingBuffer);
            if (ret < 0) {
                CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                        gscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
            }
        } else
#endif
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

    int waitCount = 5;

    if (m_yuvcallbackCounter.getCount() <= 0) {
#ifdef USE_DUAL_CAMERA
        int masterCameraId, slaveCameraId;
        getDualCameraId(&masterCameraId, &slaveCameraId);
        if (m_parameters->getDualCameraMode() == true &&
                m_cameraId == slaveCameraId &&
                m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC) {
            loop = false;
        } else
#endif
        {
        if (m_nv21CaptureEnabled) {
            m_parameters->setOutPutFormatNV21Enable(false);
            m_nv21CaptureEnabled = false;
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
        waitCount = 0;
        m_terminatePictureThreads(true);
        m_captureLock.unlock();
        }
    } else {
        loop = true;
        m_captureLock.unlock();
    }

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

#ifdef USE_DUAL_CAMERA
    int masterCameraId, slaveCameraId;
    getDualCameraId(&masterCameraId, &slaveCameraId);
    if (m_parameters->getDualCameraMode() == true &&
            m_cameraId == slaveCameraId &&
            m_parameters->getDualCameraReprocessingSyncType() != SYNC_TYPE_SWITCH) {
        m_parameters->setIsThumbnailCallbackOn(false);
        return loop;
    }
#endif

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
            CLOGD("Burst Shutter callback start");
            m_shutterCallbackThreadFunc();
            CLOGD("Burst Shutter callback end");
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
#ifdef SAMSUNG_STR_CAPTURE
            || m_parameters->getSTRCaptureEnable()
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

#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true &&
             (m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_POST ||
             m_parameters->getDualStandbyMode() == ExynosCameraParameters::DUAL_STANDBY_MODE_ACTIVE_IN_SENSOR)) {
            loop = false;
            goto CLEAN;
        }
#endif

        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("postviewCallbackQ pop fail, retry(%d)", retry);
            }
        } while (ret < 0
                 && retry < maxRetry
                 && m_flagThreadStop == false
                 && m_stopBurstShot == false
                 && m_cancelPicture == false);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0], 1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE("get postviewCallbackHeap fail");
            loop = true;
            goto CLEAN;
        }

        if (m_cancelPicture == false) {
            CLOGD("thumbnail POSTVIEW callback start");
            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
            CLOGD("thumbnail POSTVIEW callback end");
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
                CLOGD("ONE SECOND CAPTURE coming.");
		        ret = 0;
                break;
            }
            ret = m_jpegCallbackQ->popProcessQ(&newFrame);
            if (ret < 0) {
                /* If there's no saved jpegCallbackHeap, ret value will smaller than 0
                   This will launch "goto CLEAN" */
                retry++;
                CLOGW("jpegCallbackQ pop fail, retry(%d)", retry);
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

#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_TIMESTAMP_BOOT
    m_Metadata.timestamp = newFrame->getTimeStampBoot();
#else
    m_Metadata.timestamp = newFrame->getTimeStamp();
#endif
    CLOGV(" timestamp:%lldms!", m_Metadata.timestamp);
#endif

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST
        && m_parameters->getIsThumbnailCallbackOn()
        && jpegCallbackBuffer.index != -2) {
        do {
            ret = m_postviewCallbackQ->waitAndPopProcessQ(&postviewCallbackBuffer);
            if (ret < 0) {
                retry++;
                CLOGW("postviewCallbackQ pop fail, retry(%d)", retry);
            }
        } while (ret < 0 && retry < maxRetry && m_flagThreadStop != true && m_stopBurstShot == false);

        postviewCallbackHeap = m_getMemoryCb(postviewCallbackBuffer.fd[0], postviewCallbackBuffer.size[0], 1, m_callbackCookie);

        if (!postviewCallbackHeap || postviewCallbackHeap->data == MAP_FAILED) {
            CLOGE(" get postviewCallbackHeap fail");
            loop = true;
            goto CLEAN;
        }
    }
#endif

    m_captureLock.lock();

    if (ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
        loop = true;
        goto CLEAN;
    }

    seriesShotNumber = newFrame->getFrameCount();

    CLOGD("jpeg calllback is start");

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
                CLOGE("wait and pop fail, ret(%d)", ret);
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
                        CLOGD("One Second Shutter callback start");
                        m_shutterCallbackThreadFunc();
                        CLOGD("One Second Shutter callback end");
                    } else if (m_burstShutterLocation == BURST_SHUTTER_PREPICTURE_DONE) {
                        m_burstShutterLocation = BURST_SHUTTER_JPEGCB;
                    }
                    if (m_parameters->getIsThumbnailCallbackOn()) {
                        if (m_parameters->msgTypeEnabled(CAMERA_MSG_POSTVIEW_FRAME)) {
                            CLOGD("thumbnail POSTVIEW callback start");
                            setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                            clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                            CLOGD("thumbnail POSTVIEW callback end");
                        }
                        if (m_one_second_postviewCallbackHeap != NULL) {
                            /* release saved buffer when COMPRESSED m_dataCb() called. */
                            m_one_second_postviewCallbackHeap->release(m_one_second_postviewCallbackHeap);
                            m_one_second_postviewCallbackHeap = NULL;
                        }
                    }
                    CLOGD(" before compressed - TimeCheck");
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
                    CLOGD(" CAMERA_MSG_COMPRESSED_IMAGE callback (%d)",
                             m_burstCaptureCallbackCount);
                }

#ifdef ONE_SECOND_BURST_CAPTURE
                if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
                    CLOGD(" After compressed - TimeCheck");
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
                    CLOGE("putBuffer(%d) fail", jpegCallbackBuffer.index);
            }

#ifdef ONE_SECOND_BURST_CAPTURE
            /* One second burst operation will release m_one_second_jpegCallbackHeap */
            if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
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

#ifdef ONE_SECOND_BURST_CAPTURE
    if (currentSeriesShotMode == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        m_jpegCallbackCounter.decCount();
        CLOGD("ONE SECOND BURST remaining count(%d)", m_jpegCallbackCounter.getCount());
        if (m_jpegCallbackCounter.getCount() <= 1) {
            if (m_one_second_burst_capture) {
                loop = true;
                m_jpegCallbackCounter.setCount(2);
                m_reprocessingCounter.setCount(2);
                m_jpegCounter.setCount(2);
                m_pictureCounter.setCount(2);
                CLOGD("Waiting once more...");
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
#ifdef USE_DUAL_CAMERA
        int masterCameraId, slaveCameraId;
        getDualCameraId(&masterCameraId, &slaveCameraId);
        if (m_parameters->getDualCameraMode() == true &&
                m_cameraId == slaveCameraId &&
                m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC) {
            loop = false;
        } else
#endif
        {
        m_pictureEnabled = false;
        loop = false;

#ifdef ONE_SECOND_BURST_CAPTURE
        /* m_terminatePictureThreads() already called in m_clearOneSecondBurst() */
        if (currentSeriesShotMode != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
        m_terminatePictureThreads(true);
        m_captureLock.unlock();
        }
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

    if (m_nv21CaptureEnabled
#ifdef SAMSUNG_LLS_DEBLUR
        || m_parameters->getLDCaptureMode() > 0
#endif
#ifdef SAMSUNG_LENS_DC
        || m_parameters->getLensDCEnable()
#endif
#ifdef SAMSUNG_STR_CAPTURE
        || m_parameters->getSTRCaptureEnable()
#endif
#ifdef USE_DUAL_CAMERA
        || (m_parameters->getDualCameraMode() == true &&
           m_parameters->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC)
#endif
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
#ifdef SAMSUNG_LLS_DEBLUR
       && !m_parameters->getLDCaptureMode()
#endif
#ifdef SAMSUNG_LENS_DC
       && !m_parameters->getLensDCEnable()
#endif
#ifdef SAMSUNG_STR_CAPTURE
       && !m_parameters->getSTRCaptureEnable()
#endif
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

    if (m_parameters->getUseDynamicScc() == true) {
        CLOGD(" Use dynamic bayer");
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
        CLOGD("[DC] Set Lens Distortion Correction, getLensDCEnable(%d)",
                 m_parameters->getLensDCEnable());
    }
#endif

#ifdef SAMSUNG_STR_CAPTURE
    if (m_parameters->getSTRCaptureEnable()) {
        m_parameters->setSTRCaptureEnable(false);
        m_parameters->setOutPutFormatNV21Enable(false);
        CLOGD("[STR_CAPTURE] Set STR Capture, getSTRCaptureEnable(%d)",
                 m_parameters->getSTRCaptureEnable());
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
#ifdef USE_MSC_CAPTURE
    m_MscCaptureThread->requestExit();
#endif

    if (m_ThumbnailCallbackThread != NULL)
        m_ThumbnailCallbackThread->requestExit();

#ifdef SAMSUNG_DNG
    m_DNGCaptureThread->requestExit();
#endif

    m_postPictureThread->requestExit();
    m_jpegCallbackThread->requestExit();
    m_yuvCallbackThread->requestExit();

    if (!callFromJpeg) {
        m_postviewCallbackQ->sendCmd(WAKE_UP);
        m_jpegCallbackQ->sendCmd(WAKE_UP);
        CLOGI("wakeup");
    }

    CLOGI(" wait m_prePictureThrad");
    m_prePictureThread->requestExitAndWait();
    CLOGI(" wait m_pictureThrad");
    m_pictureThread->requestExitAndWait();

    CLOGI(" wait m_postPictureCallbackThread");
    m_postPictureCallbackThread->requestExitAndWait();
#ifdef SAMSUNG_LLS_DEBLUR
    CLOGI(" wait m_LDCaptureThread");
    m_LDCaptureThread->requestExitAndWait();
#endif
#ifdef USE_MSC_CAPTURE
    CLOGI(" wait m_MscCaptureThread");
    m_MscCaptureThread->requestExitAndWait();
#endif

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

    if (m_parameters->isReprocessing() == true) {
        enum pipeline pipe = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;

        m_reprocessingFrameFactory->stopThread(pipe);
    }

#ifdef SAMSUNG_MAGICSHOT
    if (m_parameters->getShotMode() == SHOT_MODE_MAGIC) {
        if(m_parameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING2);
            CLOGI("rear gsc , pipe stop(%d)", PIPE_GSC_REPROCESSING2);
        } else {
            enum pipeline pipe = PIPE_GSC_VIDEO;

            m_previewFrameFactory->stopThread(pipe);
            CLOGI("front gsc , pipe stop(%d)", pipe);
        }
    }
#endif

    if(m_parameters->isReprocessing() == true) {
            m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING3);
            CLOGI("rear gsc , pipe stop(%d)", PIPE_GSC_REPROCESSING3);
    }

#ifdef USE_ODC_CAPTURE
    if (m_parameters->getODCCaptureEnable()) {
        m_reprocessingFrameFactory->stopThread(PIPE_TPU1);
        CLOGI("rear tpu1 , pipe stop(%d)", PIPE_TPU1);
        m_reprocessingFrameFactory->stopThread(PIPE_GSC_REPROCESSING4);
        CLOGI("rear gsc , pipe stop(%d)", PIPE_GSC_REPROCESSING4);
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
#ifdef ONE_SECOND_BURST_CAPTURE
            if (m_parameters->getSeriesShotMode() != SERIES_SHOT_MODE_ONE_SECOND_BURST)
#endif
            {
                char filepath[CAMERA_FILE_PATH_SIZE];
                int ret = -1;

                memset(filepath, 0, sizeof(filepath));

#ifdef SAMSUNG_INF_BURST
                snprintf(filepath, sizeof(filepath), "%s%s_%03d.jpg", m_burstSavePath, m_burstTime, newFrame->getFrameCount());
#else
                snprintf(filepath, sizeof(filepath), "%sBurst%02d.jpg", m_burstSavePath, newFrame->getFrameCount());
#endif
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
        odcBufferMgr = m_sccReprocessingBufferMgr;

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

    if (m_parameters->isReprocessing() == true) {
        enum pipeline pipe = PIPE_3AA_REPROCESSING;

        if (m_parameters->isOwnScc(getCameraIdInternal()) == true)
            pipe = PIPE_SCC_REPROCESSING;
        else if (m_parameters->getUsePureBayerReprocessing() == false)
            pipe = PIPE_ISP_REPROCESSING;

#ifdef USE_REMOSAIC_CAPTURE
        if (m_parameters->getRemosaicCaptureMode() == ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_PURE_BAYER) {
            pipe = PIPE_3AA_REPROCESSING;
            m_parameters->setRemosaicCaptureMode(ExynosCamera1Parameters::REMOSAIC_CAPTURE_MODE_NONE);
        }
#endif

        CLOGD(" Wait thread exit Pipe(%d) ", pipe);
        ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
        if (ret != NO_ERROR)
            CLOGE("3AA reprocessing stopThread fail, pipe(%d) ret(%d)", pipe, ret);

        ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
        if (ret != NO_ERROR)
            CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);

        if (m_parameters->needGSCForCapture(m_cameraId) == true) {
            pipe = PIPE_GSC_REPROCESSING;
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("GSC stopThread fail, pipe(%d) ret(%d)", pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);
        }
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            pipe = PIPE_SYNC_REPROCESSING;
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("3AA reprocessing stopThread fail, pipe(%d) ret(%d)", pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);

            pipe = PIPE_FUSION_REPROCESSING;
            ret = m_reprocessingFrameFactory->stopThread(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("3AA reprocessing stopThread fail, pipe(%d) ret(%d)", pipe, ret);

            ret = m_reprocessingFrameFactory->stopThreadAndWait(INDEX(pipe));
            if (ret != NO_ERROR)
                CLOGE("stopThreadAndWait fail, pipeId(%d) ret(%d)", pipe, ret);

            int masterCameraId, slaveCameraId;
            getDualCameraId(&masterCameraId, &slaveCameraId);

            /* unlock syncType during takePicture */
            switch (m_parameters->getDualCameraReprocessingSyncType()) {
            case SYNC_TYPE_BYPASS:
            case SYNC_TYPE_SYNC:
                if (m_cameraId == masterCameraId)
                    m_parameters->lockDualCameraSyncType(false);
                break;
            case SYNC_TYPE_SWITCH:
                if (m_cameraId == slaveCameraId)
                    m_parameters->lockDualCameraSyncType(false);
                break;
            default:
                break;
            }
        }
#endif
    }

    CLOGD(" clear postProcessList");
    if (m_clearList(&m_postProcessList, &m_postProcessListLock) < 0) {
        CLOGE("m_clearList fail");
    }

#if 1
    CLOGD(" clear m_dstPostPictureGscQ");
    m_dstPostPictureGscQ->release();

    CLOGD(" clear postPictureQ");
    m_postPictureQ->release();

    CLOGD(" clear m_yuvCallbackQ");
    m_yuvCallbackQ->release();

    CLOGD(" clear m_dstSccReprocessingQ");
    m_dstSccReprocessingQ->release();

    CLOGD(" clear m_dstJpegReprocessingQ");
    m_dstJpegReprocessingQ->release();
#else
    ExynosCameraFrameSP_sptr_t frame = NULL;

    CLOGD(" clear postPictureQ");
    while(m_postPictureQ->getSizeOfProcessQ()) {
        m_postPictureQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }

    CLOGD(" clear m_dstSccReprocessingQ");
    while(m_dstSccReprocessingQ->getSizeOfProcessQ()) {
        m_dstSccReprocessingQ->popProcessQ(&frame);
        if (frame != NULL) {
            delete frame;
            frame = NULL;
        }
    }
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    CLOGD(" clear m_LDCaptureQ");
    m_LDCaptureQ->release();
#endif
#ifdef USE_MSC_CAPTURE
    CLOGD(" clear m_MscCaptureQ");
    m_MscCaptureQ->release();
#endif

#ifdef USE_DUAL_CAMERA
    CLOGD(" clear m_syncReprocessingQ");
    m_syncReprocessingQ->release();

    CLOGD(" clear m_fusionReprocessingQ");
    m_fusionReprocessingQ->release();
#endif

#ifdef SAMSUNG_LENS_DC
    CLOGD(" reset m_lensDCBufferMgr");
    m_lensDCBufferMgr->resetBuffers();
#endif

#ifdef SAMSUNG_STR_CAPTURE
    CLOGD(" reset m_strCaptureBufferMgr");
    m_strCaptureBufferMgr->resetBuffers();
#endif

    CLOGD(" reset thumbnail gsc buffers");
    m_thumbnailGscBufferMgr->resetBuffers(m_cameraId);

#ifdef USE_MSC_CAPTURE
    CLOGD(" reset msc buffers");
    m_mscBufferMgr->resetBuffers();
#endif

    CLOGD(" reset buffer gsc buffers");
    m_gscBufferMgr->resetBuffers();
    CLOGD(" reset buffer jpeg buffers");
    m_jpegBufferMgr->resetBuffers(m_cameraId);
    CLOGD(" reset buffer sccReprocessing buffers");
    m_sccReprocessingBufferMgr->resetBuffers(m_cameraId);

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        CLOGD(" reset buffer fusionReprocessing buffers");
        m_fusionReprocessingBufferMgr->resetBuffers();
    }
#endif

    CLOGD(" reset buffer m_postPicture buffers");
    m_postPictureBufferMgr->resetBuffers(m_cameraId);
    CLOGD(" reset buffer thumbnail buffers");
    m_thumbnailBufferMgr->resetBuffers(m_cameraId);

    m_nv21CaptureEnabled = false;
#ifdef SAMSUNG_DNG
    CLOGI(" wait m_DNGCaptureThread");
    m_DNGCaptureThread->requestExitAndWait();
    if (m_dngCaptureQ->getSizeOfProcessQ() > 0) {
        CLOGE("remaining DNG buffer in captureQ");
    }
    if (m_dngCaptureDoneQ->getSizeOfProcessQ() > 0) {
        CLOGE("remaining DNG buffer in capturDoneQ");
    }

    if (m_parameters->getDNGCaptureModeOn() == true) {
        m_parameters->cleanDngThumbnailBuffer();
    }
#endif

#ifdef USE_ODC_CAPTURE
    m_ODCProcessingQ->release();
    m_postODCProcessingQ->release();
    m_parameters->setODCCaptureEnable(false);
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

    CLOGI(" -IN-");

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

    CLOGD(" faceDetectMode(%d)", m_srShotMeta.stats.faceDetectMode);
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

            CLOGV(" left eye(%d,%d), right eye(%d,%d), mouth(%dx%d), num of facese(%d)",
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
#ifdef SAMSUNG_DUAL_SOLUTION
        m_sr_frameMetadata.crop_info.fov_left = 0;
        m_sr_frameMetadata.crop_info.fov_top = 0;
        m_sr_frameMetadata.crop_info.fov_width = m_sr_cropped_width;
        m_sr_frameMetadata.crop_info.fov_height = m_sr_cropped_height;
#endif
    } else {
        m_sr_frameMetadata.crop_info.cropped_width = pictureW;
        m_sr_frameMetadata.crop_info.cropped_height = pictureH;
    }

    CLOGI("sieze (%d,%d,%d,%d)"
     , pictureW, pictureH, m_sr_frameMetadata.crop_info.cropped_width, m_sr_frameMetadata.crop_info.cropped_height);

    m_srcallbackTimer.start();

    setBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);
    m_dataCb(MULTI_FRAME_SHOT_SR_EXTRA_INFO, m_srCallbackHeap, 0, &m_sr_frameMetadata, m_callbackCookie);
    clearBit(&m_callbackState, MULTI_FRAME_SHOT_SR_EXTRA_INFO, false);

    m_srcallbackTimer.stop();
    m_srcallbackTimerTime = m_srcallbackTimer.durationUsecs();

    if((int)m_srcallbackTimerTime / 1000 > 50) {
        CLOGD(" SR callback duration time : (%d)mec", (int)m_srcallbackTimerTime / 1000);
    }

    return NO_ERROR;
}
#endif

#if defined(SAMSUNG_EEPROM)
bool ExynosCamera::m_eepromThreadFunc(void)
{
    CLOGI("");
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
        CLOGE("failed to open sysfs. camera id = %d", m_cameraId);
        goto err;
    }

    if (fgets(sensorFW, sizeof(sensorFW), fp) == NULL) {
        CLOGE("failed to read sysfs entry");
	    goto err;
    }

    /* int numread = strlen(sensorFW); */
    CLOGI(" eeprom read complete. Sensor FW ver: %s", sensorFW);

err:
    if (fp != NULL)
        fclose(fp);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

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
        CLOGI("m_flagThreadStop(%d)", m_flagThreadStop);
        m_LBPCount = 0;
        return false;
    }

    if(ret == TIMED_OUT || ret < 0) {
        CLOGE("wait and pop fail, ret(%d)", ret);
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
            CLOGE("[LBP]:Bestphoto plugin set failed!!");
        ret = uni_plugin_process(m_LBPpluginHandle);
        if(ret < 0)
            CLOGE("[LBP]:Bestphoto plugin process failed!!");
        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[LBP]:duration time(%5d msec)", (int)durationTime);
    }

    if(m_LBPCount == m_parameters->getHoldFrameCount()) {
        int bestPhotoIndex;
        ret = uni_plugin_get(m_LBPpluginHandle, BESTPHOTO_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INDEX, &bestPhotoIndex);
        if(ret < 0)
            CLOGE("[LBP]:Bestphoto plugin get failed!!");

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
                CLOGE("[JQ]: m_processJpegQtable() failed!!");
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
            CLOGE("[JQ]: Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
        }
        ret = uni_plugin_process(m_JQpluginHandle);
        if(ret < 0) {
            CLOGE("[JQ]: Plugin process failed!!");
        }
        ret = uni_plugin_get(m_JQpluginHandle,
                JPEG_QTABLE_PLUGIN_NAME, UNI_PLUGIN_INDEX_JPEG_QTABLE, m_qtable);
        if(ret < 0) {
            CLOGE("[JQ]: Plugin get(UNI_PLUGIN_INDEX_JPEG_QTABLE) failed!!");
        }

        CLOGD("[JQ]: Qtable Set Done!!");
        m_parameters->setJpegQtable(m_qtable);
        m_parameters->setJpegQtableStatus(JPEG_QTABLE_UPDATED);
    }

    m_JQ_mutex.lock();
    if (m_JQisWait == true)
        m_JQ_condition.signal();
    m_JQ_mutex.unlock();

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[JQ]:duration time(%5d msec)", (int)durationTime);

    return ret;
}
#endif
#ifdef ONE_SECOND_BURST_CAPTURE
void ExynosCamera::m_clearOneSecondBurst(bool isJpegCallbackThread)
{
    if (m_parameters->getSeriesShotMode() == SERIES_SHOT_MODE_ONE_SECOND_BURST) {
        CLOGD(" DISABLE ONE SECOND BURST %d - TimeCheck", isJpegCallbackThread);

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
            CLOGI(" wait m_jpegCallbackThread");
            m_jpegCallbackThread->requestExit();
            m_jpegCallbackThread->requestExitAndWait();
            m_parameters->setSeriesShotMode(SERIES_SHOT_MODE_NONE);
        }

        CLOGD(" DISABLE ONE SECOND BURST DONE %d - TimeCheck", isJpegCallbackThread);
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
    CLOGI("");

    int loop = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    int ret = 0;
    int pipeId = PIPE_3AA_REPROCESSING;
    int pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;
    ExynosCameraBuffer postgscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
#ifdef SAMSUNG_MAGICSHOT
    camera_memory_t *postviewCallbackHeap = NULL;
#endif
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
        pipeId_gsc_magic = PIPE_MCSC1_REPROCESSING;
        pipeId = (m_parameters->isReprocessing3aaIspOTF() == true) ? PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
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

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true &&
            newFrame->getSyncType() == SYNC_TYPE_SYNC)
        pipeId = PIPE_FUSION_REPROCESSING;
#endif

    if (m_parameters->isReprocessing() == true) {
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
                    CLOGE("m_getMemoryCb(%d) fail", postgscReprocessingBuffer.size[0]);
                    goto CLEAN;
                }

                setBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                m_dataCb(CAMERA_MSG_POSTVIEW_FRAME, postviewCallbackHeap, 0, NULL, m_callbackCookie);
                clearBit(&m_callbackState, CALLBACK_STATE_POSTVIEW_FRAME, true);
                postviewCallbackHeap->release(postviewCallbackHeap);
                CLOGD("magic shot POSTVIEW callback end. pipe(%d) index(%d)count(%d),max(%d)",
                        pipeId_gsc_magic,postgscReprocessingBuffer.index,
                        m_burstCaptureCallbackCount,m_magicshotMaxCount);
            }

#ifdef USE_DUAL_CAMERA
            if (m_parameters->getDualCameraMode() == true) {
                ret = m_putFusionReprocessingBuffers(newFrame, &postgscReprocessingBuffer);
                if (ret < 0) {
                    CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                            postgscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                }
            } else
#endif
            {
                ret = m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
            }
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
                CLOGE("[BD]: Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
            }

            ret = uni_plugin_process(m_BDpluginHandle);
            if (ret < 0) {
                CLOGE("[BD]: Plugin process failed!!");
            }

            ret = uni_plugin_get(m_BDpluginHandle,
                    BLUR_DETECTION_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &m_BDbuffer[m_BDbufferIndex]);
            if (ret < 0) {
                CLOGE("[BD]: Plugin get failed!!");
            }

            CLOGD("[BD]: Push the buffer(Qsize:%d, Index: %d)",
                     m_BDbufferQ->getSizeOfProcessQ(), m_BDbufferIndex);

            if (m_BDbufferQ->getSizeOfProcessQ() == MAX_BD_BUFF_NUM)
                CLOGE("[BD]: All buffers are queued!! Skip the buffer!!");
            else
                m_BDbufferQ->pushProcessQ(&m_BDbuffer[m_BDbufferIndex]);

            if (++m_BDbufferIndex == MAX_BD_BUFF_NUM)
                m_BDbufferIndex = 0;
        }

        m_timer.stop();
        durationTime = m_timer.durationMsecs();
        CLOGD("[BD]:Plugin duration time(%5d msec)", (int)durationTime);
    }

    if (isThumbnailCallbackOn == false) {
        if (postgscReprocessingBuffer.index != -2) {
#ifdef USE_DUAL_CAMERA
            if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
                ret = m_putFusionReprocessingBuffers(newFrame, &postgscReprocessingBuffer);
                if (ret < 0) {
                    CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                            postgscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
                }
            } else
#endif
            {
                m_putBuffers(bufferMgr, postgscReprocessingBuffer.index);
            }
        }
    }
#endif

    if (newFrame != NULL) {
       /* newFrame->printEntity(); */
        CLOGD(" m_postPictureCallbackThreadFunc frame delete(%d)",
                 newFrame->getFrameCount());
        newFrame = NULL;
    }

    return loop;

CLEAN:
    if (postgscReprocessingBuffer.index != -2) {
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            ret = m_putFusionReprocessingBuffers(newFrame, &postgscReprocessingBuffer);
            if (ret < 0) {
                CLOGE("m_putFusionReprocessingBuffer(%d, %d, %d) fail, ret(%d)",
                        postgscReprocessingBuffer.index, newFrame->getSyncType(), newFrame->getFrameCount(), ret);
            }
        } else
#endif
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

#ifdef USE_MSC_CAPTURE
bool ExynosCamera::m_MscCaptureThreadFunc(void)
{
    int loop = false;
    int pipeId = PIPE_ISP_REPROCESSING;
    int pipeId_gsc = PIPE_MCSC1_REPROCESSING;
    int mscPipe = PIPE_GSC_REPROCESSING3;

    int waitCount = 0;
    bool isTimeOut = false;
    int framecount = 0;
    int dstbufferIndex = -1;
    int retry = 0;
    int ret = 0;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBufferManager *srcbufferMgr = NULL;
    ExynosCameraBufferManager *mscBufferMgr = NULL;
    ExynosCameraBuffer postmscReprocessingSrcBuffer;
    ExynosCameraBuffer postmscReprocessingDstBuffer;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    ExynosRect mscRect;
    ExynosRect srcRect;

    int32_t SrcFormat = HAL_PIXEL_FORMAT_2_V4L2_PIX(HAL_PIXEL_FORMAT_YCrCb_420_SP);
    int inputSizeW, inputSizeH;
    int bayerCropX, bayerCropY, bayerCropW, bayerCropH;

    m_timer.start();
    m_parameters->getPictureSize(&inputSizeW, &inputSizeH);
    m_parameters->getHwBayerCropRegion(&bayerCropW, &bayerCropH, &bayerCropX, &bayerCropY);

    srcRect.w = ALIGN_UP(504, 16); // get rect info from dual lib
    srcRect.h = ALIGN_UP(378, 16); // get rect info from dual lib
    srcRect.x = (bayerCropW - srcRect.w) / 2; // get rect info from dual lib
    srcRect.y = (bayerCropH - srcRect.h) / 2; // get rect info from dual lib
    srcRect.fullW = ALIGN_UP(bayerCropW, 16);
    srcRect.fullH = ALIGN_UP(bayerCropH, 16);
    srcRect.colorFormat = SrcFormat;

    mscRect.x = 0;
    mscRect.y = 0;
    mscRect.w = inputSizeW;
    mscRect.h = inputSizeH;
    mscRect.fullW = inputSizeW;
    mscRect.fullH = inputSizeH;
    mscRect.colorFormat = SrcFormat;

    postmscReprocessingSrcBuffer.index = -2;
    postmscReprocessingDstBuffer.index = -2;

    ret = m_MscCaptureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        isTimeOut = true;
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId, &postmscReprocessingSrcBuffer,
                                    m_reprocessingFrameFactory->getNodeType(pipeId_gsc));

    m_getBufferManager(mscPipe, &srcbufferMgr, SRC_BUFFER_DIRECTION);

    mscBufferMgr = m_mscBufferMgr;
    newFrame = m_pictureFrameFactory->createNewFrameOnlyOnePipe(mscPipe, framecount);

    do {
        dstbufferIndex = -2;
        retry++;

        if (mscBufferMgr->getNumOfAvailableBuffer() > 0) {
            ret = mscBufferMgr->getBuffer(&dstbufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &postmscReprocessingDstBuffer);
                CLOGE("getBuffer fail, ret(%d)", ret);
        }

        if (dstbufferIndex < 0) {
            usleep(WAITING_TIME);
            if (retry % 20 == 0) {
                CLOGW("retry Post View Thumbnail getBuffer)");
                mscBufferMgr->dump();
            }
        }
    } while (dstbufferIndex < 0 && retry < (TOTAL_WAITING_TIME / WAITING_TIME) && m_flagThreadStop != true && m_stopBurstShot == false);

    newFrame->setSrcRect(mscPipe, srcRect);
    newFrame->setDstRect(mscPipe, mscRect);

    ret = m_setupEntity(mscPipe, newFrame, &postmscReprocessingSrcBuffer, &postmscReprocessingDstBuffer);

    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", mscPipe, ret);
    }

    m_pictureFrameFactory->setOutputFrameQToPipe(m_MscCaptureQ, mscPipe);
    m_pictureFrameFactory->pushFrameToPipe(newFrame, mscPipe);

    /* wait GSC done */
    CLOGV("wait GSC output");
    newFrame = NULL;
    do {
        ret = m_MscCaptureQ->waitAndPopProcessQ(&newFrame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100 && m_flagThreadStop != true);

    if (ret < 0) {
        CLOGW("Failed to waitAndPopProcessQ. ret %d waitCount %d flagThreadStop %d",
                 ret, waitCount, m_flagThreadStop);
    }

    ret = newFrame->getDstBuffer(mscPipe, &postmscReprocessingDstBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", mscPipe, ret);
        goto CLEAN;
    }

    if (postmscReprocessingSrcBuffer.addr[0] != NULL && postmscReprocessingDstBuffer.addr[0] != NULL
        && (postmscReprocessingSrcBuffer.size[0] == postmscReprocessingDstBuffer.size[0])) {
        memcpy(postmscReprocessingSrcBuffer.addr[0], postmscReprocessingDstBuffer.addr[0], postmscReprocessingSrcBuffer.size[0]);
    }

CLEAN:
    if (postmscReprocessingDstBuffer.index != -2 && ret < 0)
        m_putBuffers(mscBufferMgr, postmscReprocessingDstBuffer.index);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[MSC Capture]duration time(%5d msec)", (int)durationTime);

    return loop;
}
#endif

#ifdef SAMSUNG_LLS_DEBLUR
bool ExynosCamera::m_LDCaptureThreadFunc(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    CLOGI("[LLS_MBR] ");

    int ret = 0;
    int loop = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer gscReprocessingBuffer;
    ExynosCameraBufferManager *bufferMgr = NULL;
    int pipeId_gsc = PIPE_MCSC1_REPROCESSING;
    int pipeId = PIPE_3AA_REPROCESSING;
    int previewW = 0, previewH = 0, previewFormat = 0;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
#ifdef SAMSUNG_HIFI_LLS
    int hwPictureW = 0, hwPictureH = 0;
#endif
    ExynosRect srcRect_magic, dstRect_magic;
    float zoomRatio = m_parameters->getZoomRatio(0) / 1000;
    int retry1 = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int totalCount = 0;
    bool isTimeOut = false;
    camera_frame_metadata_t m_Metadata;

#if defined(SAMSUNG_HIFI_LLS) || defined(FLASHED_LLS_CAPTURE)
#ifdef SAMSUNG_HIFI_LLS
    const char *llsPluginName = HIFI_LLS_PLUGIN_NAME;
#else /* FLASHED_LLS_CAPTURE */
    const char *llsPluginName = FLASHED_LLS_PLUGIN_NAME;
#endif
#else
    const char *llsPluginName = LLS_PLUGIN_NAME;
#endif

    gscReprocessingBuffer.index = -2;

    if (m_parameters->getUsePureBayerReprocessing() == true)
        pipeId = PIPE_3AA_REPROCESSING;
    else
        pipeId = PIPE_ISP_REPROCESSING;

    ret = m_getBufferManager(pipeId_gsc, &bufferMgr, DST_BUFFER_DIRECTION);
    if (ret < 0) {
        CLOGE("getBufferManager(SRC) fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
        return ret;
    }

    CLOGI("wait m_deblurCaptureQ output");
    ret = m_LDCaptureQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        isTimeOut = true;
        goto CLEAN;
    }

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        goto CLEAN;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC)
        pipeId = PIPE_FUSION_REPROCESSING;
#endif

    if (m_jpegCounter.getCount() <= 0) {
        CLOGD(" Picture canceled");
        goto CLEAN;
    }

    ret = newFrame->getDstBuffer(pipeId, &gscReprocessingBuffer,
                                    m_reprocessingFrameFactory->getNodeType(pipeId_gsc));
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_gsc, ret);
        goto CLEAN;
    }

    m_LDBufIndex[m_LDCaptureCount] = gscReprocessingBuffer.index;
    m_LDCaptureCount++;

    CLOGI("[LLS_MBR] m_deblurCaptureQ output done m_LDCaptureCount(%d) index(%d)",
             m_LDCaptureCount, gscReprocessingBuffer.index);

    /* Library */
    CLOGD("[LLS_MBR]-- Start Library --");
    m_timer.start();
    if (m_LLSpluginHandle != NULL) {
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
#ifdef SUPPORT_ZSL_MULTIFRAME
            case LLS_LEVEL_MULTI_MERGE_LOW_2:
            case LLS_LEVEL_MULTI_MERGE_ZSL_2:
#endif
                totalCount = 2;
                opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
                break;
            case LLS_LEVEL_MULTI_MERGE_3:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_3:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3:
#ifdef SUPPORT_ZSL_MULTIFRAME
            case LLS_LEVEL_MULTI_MERGE_LOW_3:
            case LLS_LEVEL_MULTI_MERGE_ZSL_3:
#endif
                totalCount = 3;
                opMode = UNI_PLUGIN_OP_COMPOSE_IMAGE;
                break;
            case LLS_LEVEL_MULTI_MERGE_4:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_4:
            case LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4:
#ifdef SUPPORT_ZSL_MULTIFRAME
            case LLS_LEVEL_MULTI_MERGE_LOW_4:
            case LLS_LEVEL_MULTI_MERGE_ZSL_4:
#endif
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
                CLOGE("[LLS_MBR] Wrong LLS mode has been get!! %d", llsMode);
                goto CLEAN;
        }

        /* First shot */
        if (m_LDCaptureCount == 1) {
            ret = uni_plugin_init(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin init failed!!");
            }

            CLOGD("[LLS_MBR] Set capture num: %d", totalCount);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM, &totalCount);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_TOTAL_BUFFER_NUM failed!!");
            }

            CLOGD("[LLS_MBR] Set capture mode: %d", opMode);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_OPERATION_MODE, &opMode);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_OPERATION_MODE failed!!");
            }

#if defined(SAMSUNG_DUAL_SOLUTION) && defined(SAMSUNG_HIFI_LLS)
            UniPluginDualInfo_t dualInfo;
            memset(&dualInfo, 0, sizeof(UniPluginDualInfo_t));

            m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
            dualInfo.fullWidth = hwPictureW;
            dualInfo.fullHeight = hwPictureH;

            if (m_parameters->getDualCameraMode() == true) {
                int masterCameraId, slaveCameraId;
                getDualCameraId(&masterCameraId, &slaveCameraId);

                dualInfo.isDual = m_parameters->getDualCameraMode();
                dualInfo.mainSensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(masterCameraId);
                dualInfo.auxSensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(slaveCameraId);
            }

            CLOGD("[LLS_MBR] UNI_PLUGIN_INDEX_DUAL_INFO(isDual:%d, full size:%dx%d, Sensor id:%d,%d)",
                    dualInfo.isDual, dualInfo.fullWidth, dualInfo.fullHeight,
                    (int)dualInfo.mainSensorType, (int)dualInfo.auxSensorType);

            ret = uni_plugin_set(m_LLSpluginHandle,
                llsPluginName, UNI_PLUGIN_INDEX_DUAL_INFO, &dualInfo);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_DUAL_INFO failed!! ret(%d)", ret);
            }
#endif
            UniPluginCameraInfo_t cameraInfo;
            cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
            cameraInfo.SensorType = (UNI_PLUGIN_SENSOR_TYPE)m_cameraSensorId;
            CLOGD("[LLS_MBR] Set camera info: %d:%d",
                    cameraInfo.CameraType, cameraInfo.SensorType);
            ret = uni_plugin_set(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
            }

            struct camera2_udm *udm = new struct camera2_udm;

            if (udm != NULL) {
                UniPluginExtraBufferInfo_t extraData;
                memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
                memset(udm, 0x00, sizeof(struct camera2_udm));
                newFrame->getUserDynamicMeta(udm);

                extraData.brightnessValue.snum = (int)udm->ae.vendorSpecific[5];
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

#ifdef SAMSUNG_HIFI_LLS
                /* float zoomRatio */
                extraData.zoomRatio = m_parameters->getZoomRatio();
                CLOGD("[LLS_MBR] zoomRatio(%f)", extraData.zoomRatio);
#endif
                ret = uni_plugin_set(m_LLSpluginHandle,
                                     llsPluginName,
                                     UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO,
                                     &extraData);
                if(ret < 0)
                    CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO failed!!");

                delete udm;
            }

#ifdef SAMSUNG_HIFI_LLS
            ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];
            memset(detectedFace, 0x00, sizeof(detectedFace));
            memset(m_llsFaceInfo, 0x00, sizeof(m_llsFaceInfo));
            m_llsFaceNum = 0;
            m_parameters->getHwPreviewSize(&previewW, &previewH);

            if (m_fdmeta_shot->shot.dm.stats.faceDetectMode != FACEDETECT_MODE_OFF) {
                m_llsFaceNum = m_frameMetadata.number_of_faces;

                if (getCameraIdInternal() == CAMERA_ID_FRONT
                    && m_parameters->getFlipHorizontal() == true) {
                    /* convert left, right values when flip horizontal(front camera) */
                    for (int i = 0; i < m_llsFaceNum; i++) {
                        if (m_fdmeta_shot->shot.dm.stats.faceIds[i]) {
                            detectedFace[i].x1 = previewW - m_fdmeta_shot->shot.dm.stats.faceRectangles[i][2];
                            detectedFace[i].y1 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][1];
                            detectedFace[i].x2 = previewW - m_fdmeta_shot->shot.dm.stats.faceRectangles[i][0];
                            detectedFace[i].y2 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][3];
                        }
                    }
                } else {
                    for (int i = 0; i < m_llsFaceNum; i++) {
                        if (m_fdmeta_shot->shot.dm.stats.faceIds[i]) {
                            detectedFace[i].x1 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][0];
                            detectedFace[i].y1 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][1];
                            detectedFace[i].x2 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][2];
                            detectedFace[i].y2 = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][3];
                        }
                    }
                }
            }

            for (int i = 0; i < m_llsFaceNum; i++) {
                m_llsFaceInfo[i].FaceROI.left   = m_calibratePosition(previewW, 2000, detectedFace[i].x1) - 1000;
                m_llsFaceInfo[i].FaceROI.top    = m_calibratePosition(previewH, 2000, detectedFace[i].y1) - 1000;
                m_llsFaceInfo[i].FaceROI.right  = m_calibratePosition(previewW, 2000, detectedFace[i].x2) - 1000;
                m_llsFaceInfo[i].FaceROI.bottom = m_calibratePosition(previewH, 2000, detectedFace[i].y2) - 1000;
            }
#endif
        }

#ifdef SAMSUNG_HIFI_LLS
        ret = uni_plugin_set(m_LLSpluginHandle,
                llsPluginName, UNI_PLUGIN_INDEX_FACE_NUM, &m_llsFaceNum);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_FACE_NUM failed!! ret(%d)", ret);
            if (m_LDCaptureCount == totalCount) {
                ret = uni_plugin_deinit(m_LLSpluginHandle);
                if (ret < 0) {
                    CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
                }
            }
            goto POST_PROC;
        }

        ret = uni_plugin_set(m_LLSpluginHandle,
                llsPluginName, UNI_PLUGIN_INDEX_FACE_INFO, &m_llsFaceInfo);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_FACE_INFO failed!! ret(%d)", ret);
            if (m_LDCaptureCount == totalCount) {
                ret = uni_plugin_deinit(m_LLSpluginHandle);
                if (ret < 0) {
                    CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
                }
            }
            goto POST_PROC;
        }
#endif

        UniPluginBufferData_t pluginData;
        memset(&pluginData, 0, sizeof(UniPluginBufferData_t));
#ifdef SAMSUNG_HIFI_LLS
        m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);
#endif
        m_parameters->getPictureSize(&pictureW, &pictureH);

#ifdef SAMSUNG_HIFI_LLS
        camera2_stream *shot_stream = NULL;
        shot_stream = (camera2_stream *)gscReprocessingBuffer.addr[gscReprocessingBuffer.getMetaPlaneIndex()];
        ExynosRect cropPicture;
        cropPicture.w = shot_stream->output_crop_region[2];
        cropPicture.h = shot_stream->output_crop_region[3];
        CLOGD("[LLS_MBR] Input width: %d, height: %d", cropPicture.w, cropPicture.h);

        pluginData.InBuffY = gscReprocessingBuffer.addr[0];
        pluginData.InBuffU = gscReprocessingBuffer.addr[0] + (cropPicture.w * cropPicture.h);
        pluginData.InWidth = cropPicture.w;
        pluginData.InHeight = cropPicture.h;
#ifdef SAMSUNG_DUAL_SOLUTION
        pluginData.FOV.left = 0;
        pluginData.FOV.top = 0;
        pluginData.FOV.right = cropPicture.w;
        pluginData.FOV.bottom = cropPicture.h;
#endif
        pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_Y] = gscReprocessingBuffer.fd[0];
        pluginData.InBuffFd[UNI_PLUGIN_BUFFER_PLANE_U] = -1;
        pluginData.OutBuffY = gscReprocessingBuffer.addr[0];
        pluginData.OutBuffU = gscReprocessingBuffer.addr[0] + (pictureW * pictureH);
        pluginData.OutWidth = pictureW;
        pluginData.OutHeight = pictureH;
#else
        pluginData.InBuffY = gscReprocessingBuffer.addr[0];
        pluginData.InBuffU = gscReprocessingBuffer.addr[0] + (pictureW * pictureH);
        pluginData.InWidth = pictureW;
        pluginData.InHeight = pictureH;
#endif
        pluginData.InFormat = UNI_PLUGIN_FORMAT_NV21;
#if 0
        char filePath[100] = {'\0',};
        sprintf(filePath, "/data/media/0/LLS_input_%d.yuv", m_LDCaptureCount);
        dumpToFile((char *)filePath,
                gscReprocessingBuffer.addr[0],
                (pluginData.InWidth * pluginData.InHeight * 3) / 2);
#endif
        CLOGD("[LLS_MBR] Set input  buffer info, Width: %d, Height: %d",
                pluginData.InWidth, pluginData.InHeight);
        CLOGD("[LLS_MBR] Set output buffer info, Width: %d, Height: %d",
                pluginData.OutWidth, pluginData.OutHeight);
        ret = uni_plugin_set(m_LLSpluginHandle,
                llsPluginName, UNI_PLUGIN_INDEX_BUFFER_INFO, &pluginData);
        if (ret < 0) {
            CLOGE("[LLS_MBR] LLS Plugin set UNI_PLUGIN_INDEX_BUFFER_INFO failed!! ret(%d)", ret);
            if (m_LDCaptureCount == totalCount) {
                ret = uni_plugin_deinit(m_LLSpluginHandle);
                if (ret < 0) {
                    CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
                }
            }
            goto POST_PROC;
        }

        /* Last shot */
        if (m_LDCaptureCount == totalCount) {
            if (m_parameters->getMotionPhotoOn() &&
                m_parameters->msgTypeEnabled(CAMERA_MSG_COMPRESSED_IMAGE_NOTIFY)) {
                camera_memory_t *dummyCallbackHeap = m_getMemoryCb(-1, 0, 1, m_callbackCookie);
                if (dummyCallbackHeap == NULL) {
                    CLOGE("m_getMemoryCb fail");
                } else {
                    memset(&m_Metadata, 0, sizeof(camera_frame_metadata_t));
#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_TIMESTAMP_BOOT
                    m_Metadata.timestamp = newFrame->getTimeStampBoot();
#else
                    m_Metadata.timestamp = newFrame->getTimeStamp();
#endif
                    CLOGV(" timestamp:%lldms!", (long long)m_Metadata.timestamp);
#endif
                    setBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    m_dataCb(CAMERA_MSG_COMPRESSED_IMAGE_NOTIFY, dummyCallbackHeap, 0, &m_Metadata, m_callbackCookie);
                    clearBit(&m_callbackState, CALLBACK_STATE_COMPRESSED_IMAGE, true);
                    dummyCallbackHeap->release(dummyCallbackHeap);
                }
            }
            ret = uni_plugin_process(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin process failed!!");
            }

            UTstr debugData;
            debugData.data = new unsigned char[LLS_EXIF_SIZE];

            ret = uni_plugin_get(m_LLSpluginHandle,
                    llsPluginName, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
            CLOGD("[LLS_MBR] Debug buffer size: %d", debugData.size);
            m_parameters->setLLSdebugInfo(debugData.data, debugData.size);

            if (debugData.data != NULL)
                delete []debugData.data;

            ret = uni_plugin_deinit(m_LLSpluginHandle);
            if (ret < 0) {
                CLOGE("[LLS_MBR] LLS Plugin deinit failed!!");
            }
        }
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[LLS_MBR]duration time(%5d msec) %d/%d", (int)durationTime, m_LDCaptureCount, totalCount);
    CLOGD("[LLS_MBR]-- end Library --");

POST_PROC:
    if (m_LDCaptureCount < m_parameters->getLDCaptureCount()) {
        if (newFrame != NULL) {
            newFrame->printEntity();
            newFrame->frameUnlock();
            ret = m_removeFrameFromList(&m_postProcessList, newFrame, &m_postProcessListLock);
            if (ret < 0) {
                CLOGE("remove frame from processList fail, ret(%d)", ret);
            }

            CLOGD(" Picture frame delete(%d)", newFrame->getFrameCount());
            newFrame = NULL;
        }
        goto CLEAN;
    }

    m_postPictureQ->pushProcessQ(&newFrame);

    for (int i = 0; i < m_parameters->getLDCaptureCount() - 1; i++) {
        /* put GSC 1st buffer */
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true) {
            /* HACK */
            bufferMgr = (ExynosCameraBufferManager *)gscReprocessingBuffer.manager;
        }
#endif
        ret = m_putBuffers(bufferMgr, m_LDBufIndex[i]);
        if (ret < 0) {
            CLOGE("bufferMgr->putBuffers() fail, pipeId(%d), ret(%d)",
                    pipeId_gsc, ret);
        }
    }

CLEAN:
    if (!isTimeOut && m_parameters->getLDCaptureCount() > m_LDCaptureCount) {
        CLOGD("[LLS_MBR]m_LDCaptureThread thread will run again. "\
                "m_LDCaptureQ size(%d), totalCount(%d), curCount(%d)",
                m_LDCaptureQ->getSizeOfProcessQ(), m_parameters->getLDCaptureCount(), m_LDCaptureCount);

        loop = true;
    }

    return loop;
}

#endif

#ifdef DEBUG_IQ_OSD
void ExynosCamera::printOSD(char *Y, char *UV, struct camera2_shot_ext *meta_shot_ext)
{
#ifdef SAMSUNG_LIVE_OUTFOCUS
    if (m_parameters->getShotMode() == SHOT_MODE_LIVE_OUTFOCUS && getCameraId() == CAMERA_ID_BACK_0) {
        return;
    }
#endif
#ifdef SAMSUNG_UNI_API
    if (isOSDMode == 1 && m_parameters->getIntelligentMode() != 1) {
        int previewW = 0, previewH = 0;
#ifdef SAMSUNG_DUAL_SOLUTION
        m_parameters->getPreviewSize(&previewW, &previewH);
#ifdef SUPPORT_SW_VDIS
        if (m_parameters->isSWVdisMode() && (m_swVDIS_ModeDual || getCameraId() == CAMERA_ID_FRONT)) {
            previewW = m_swVDIS_InW;
            previewH = m_swVDIS_InH;
        }
#endif
#else
        m_parameters->getHwPreviewSize(&previewW, &previewH);
#endif
        uni_toast_init(200, 200, previewW, previewH, 13);
        short *ae_meta_short = (short *)meta_shot_ext->shot.udm.ae.vendorSpecific;
        double sv = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[3] / 256.0;
        double short_d_gain = meta_shot_ext->shot.udm.ae.vendorSpecific[99] / 256.0;
        double long_d_gain = meta_shot_ext->shot.udm.ae.vendorSpecific[97] / 256.0;
        double long_short_a_gain = meta_shot_ext->shot.udm.ae.vendorSpecific[96] / 256.0;

        uni_toastE("Preview Gain : %f", pow(2, sv - 4.0));
        uni_toastE("Capture Gain : %f", long_short_a_gain * short_d_gain);
        uni_toastE("Long gain : %f", long_d_gain);
        uni_toastE("Short gain : %f", short_d_gain);
        uni_toastE("EV : %f", (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[4] / 256.0);
        uni_toastE("BV : %f", (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[5] / 256.0);
        uni_toastE("SV : %f", sv);
        uni_toastE("FD count : %d", ae_meta_short[281+10]);
        uni_toastE("LLS flag : %d", ae_meta_short[281]);
        uni_toastE("Zoom step : %d", m_parameters->getZoomLevel());
        uni_toastE("DRC str ratio : %f", pow(2, (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[94] / 256.0));
        uni_toastE("NI : %f", 1024 * (1 / (short_d_gain * (1 + (short_d_gain - 1) * 0.199219) * long_short_a_gain)));

        if (m_meta_shot->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
            uni_toastE("Shutter speed : 1/%d", meta_shot_ext->shot.udm.ae.vendorSpecific[64]);
        else
            uni_toastE("Shutter speed : 1/%d", meta_shot_ext->shot.udm.internal.vendorSpecific2[100]);

        if (Y != NULL && UV != NULL) {
            uni_toast_print(Y, UV, getCameraId());
        }
    }
#endif
}
#endif

#ifdef SAMSUNG_STR_CAPTURE
status_t ExynosCamera::m_processSTRCapture(char *buffer, char *dst_buffer)
{
    status_t ret = NO_ERROR;

    if (m_STRCapturePluginHandle == NULL) {
        CLOGD("[STR_CAPTURE](%s[%d]): handle is NULL!!", __FUNCTION__, __LINE__);
        return INVALID_OPERATION;
    }

    UniPluginBufferData_t bufferInfo;
    UniPluginExtraBufferInfo_t extraData;
    UniPluginCameraInfo_t cameraInfo;
    UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
    uint32_t faceNum = m_frameMetadata.number_of_faces;
    uint32_t hdrData = 0;
    uint32_t flashData = 0;
    int picture_w = 0 , picture_h = 0;
    int preview_w = 0 , preview_h = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    if (faceNum == 0)
        return INVALID_OPERATION;

    m_parameters->getPictureSize(&picture_w, &picture_h);
    m_parameters->getPreviewSize(&preview_w, &preview_h);

    /* Set face Info */
    memset(faceInfo, 0x00, sizeof(faceInfo));
    if (m_fdmeta_shot->shot.dm.stats.faceDetectMode != FACEDETECT_MODE_OFF) {
        if (m_parameters->getFlipHorizontal() == false) {
            for (int i = 0; i < faceNum; i++) {
                if (m_fdmeta_shot->shot.dm.stats.faceIds[i]) {
                    faceInfo[i].FaceROI.left    = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][0];
                    faceInfo[i].FaceROI.top     = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][1];
                    faceInfo[i].FaceROI.right   = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][2];
                    faceInfo[i].FaceROI.bottom  = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][3];
                }
            }
        } else {
            /* convert left, right values when flip horizontal */
            for (int i = 0; i < faceNum; i++) {
                if (m_fdmeta_shot->shot.dm.stats.faceIds[i]) {
                    faceInfo[i].FaceROI.left    = preview_w - m_fdmeta_shot->shot.dm.stats.faceRectangles[i][2];
                    faceInfo[i].FaceROI.top     = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][1];
                    faceInfo[i].FaceROI.right   = preview_w - m_fdmeta_shot->shot.dm.stats.faceRectangles[i][0];
                    faceInfo[i].FaceROI.bottom  = m_fdmeta_shot->shot.dm.stats.faceRectangles[i][3];
                 }
            }
        }
    }

    /* Set buffer info */
    memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
    bufferInfo.InBuffY = buffer;
    bufferInfo.InBuffU = buffer + picture_w * picture_h;
    bufferInfo.InWidth = picture_w;
    bufferInfo.InHeight = picture_h;
    bufferInfo.InFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.OutBuffY = dst_buffer;
    bufferInfo.OutBuffU = dst_buffer + picture_w * picture_h;
    bufferInfo.OutWidth = preview_w;  //STR Solution need preview info
    bufferInfo.OutHeight = preview_h;

    /* Set extraData Info */
    memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraData.orientation = m_getUniOrientationMode();

    /* convert orientation value when flip horizontal */
    if (m_parameters->getFlipHorizontal()) {
        switch (m_getUniOrientationMode()) {
            case UNI_PLUGIN_ORIENTATION_90:
                extraData.orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case UNI_PLUGIN_ORIENTATION_270:
                extraData.orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            default:
                break;
        }
    }

    /* Set camera Info */
    memset(&cameraInfo, 0, sizeof(UniPluginCameraInfo_t));
    cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraId();

    hdrData = m_getUniHDRMode();
    flashData = m_getUniFlashMode();

    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FACE_NUM) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FACE_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_HDR_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRCapturePluginHandle,
            STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_FLASH_INFO, &flashData);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FLASH_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Start processing */
    m_timer.start();
    ret = uni_plugin_process(m_STRCapturePluginHandle);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): STR Capture plugin process failed!!", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }

    UTstr debugData;
    debugData.data = new unsigned char[STR_EXIF_SIZE];
    memset(debugData.data, 0, sizeof(debugData));
    ret = uni_plugin_get(m_STRCapturePluginHandle,
        STR_CAPTURE_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
    if (ret < 0) {
        CLOGE("[STR_CAPTURE](%s[%d]): get debug info failed!!", __FUNCTION__, __LINE__);
    }

    CLOGD("[STR_CAPTURE](%s[%d]): Debug buffer size: %d", __FUNCTION__, __LINE__, debugData.size);
    if (debugData.size <= STR_EXIF_SIZE) {
        CLOGD("[STR_CAPTURE] debugData.data = %s", debugData.data);
        m_parameters->setSTRdebugInfo(debugData.data, debugData.size);
    }

    if (debugData.data != NULL)
        delete []debugData.data;

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[STR_CAPTURE]duration time(%5d msec)", (int)durationTime);

    return ret;
}
#endif

#ifdef SAMSUNG_STR_PREVIEW
status_t ExynosCamera::m_processSTRPreview(char *Y, char *UV, struct camera2_shot_ext *meta_shot_ext)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginExtraBufferInfo_t extraData;
    UniPluginCameraInfo_t cameraInfo;
    UniPluginFaceInfo_t faceInfo[NUM_OF_DETECTED_FACES];
    uint32_t faceNum = 0;
    uint32_t hdrData = 0;
    uint32_t flashData = 0;
    int preview_w = 0 , preview_h = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    /* Set face Info */
    if (meta_shot_ext->shot.dm.stats.faceDetectMode != FACEDETECT_MODE_OFF) {
        for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
            if (meta_shot_ext->shot.dm.stats.faceIds[i]) {
                faceInfo[i].FaceROI.left    = meta_shot_ext->shot.dm.stats.faceRectangles[i][0];
                faceInfo[i].FaceROI.top     = meta_shot_ext->shot.dm.stats.faceRectangles[i][1];
                faceInfo[i].FaceROI.right   = meta_shot_ext->shot.dm.stats.faceRectangles[i][2];
                faceInfo[i].FaceROI.bottom  = meta_shot_ext->shot.dm.stats.faceRectangles[i][3];
                faceNum++;
            }
        }
    }

    if (faceNum == 0)
        return INVALID_OPERATION;

    /* Set buffer Info */
    m_parameters->getPreviewSize(&preview_w, &preview_h);
    bufferInfo.InBuffY = Y;
    bufferInfo.InBuffU = UV;
    bufferInfo.InWidth = preview_w;
    bufferInfo.InHeight= preview_h;
    bufferInfo.InFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.OutBuffY = Y;
    bufferInfo.OutBuffU = UV;
    bufferInfo.OutWidth = preview_w;
    bufferInfo.OutHeight= preview_h;

    /* Set extraData Info */
    memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraData.orientation = m_getUniOrientationMode();

    /* Set camera Info */
    memset(&cameraInfo, 0, sizeof(UniPluginCameraInfo_t));
    cameraInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraId();

    hdrData = m_getUniHDRMode();
    flashData = m_getUniFlashMode();

    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_CAMERA_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_NUM, &faceNum);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FACE_NUM) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FACE_INFO, &faceInfo);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FACE_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_HDR_INFO, &hdrData);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_HDR_INFO) failed!!", __FUNCTION__, __LINE__);
    }
    ret = uni_plugin_set(m_STRPreviewPluginHandle,
            STR_PREVIEW_PLUGIN_NAME, UNI_PLUGIN_INDEX_FLASH_INFO, &flashData);
    if (ret < 0) {
        CLOGE("[STR_PREVIEW](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FLASH_INFO) failed!!", __FUNCTION__, __LINE__);
    }
/*
    CLOGD("[STR_PREVIEW] w(%d) h(%d)", preview_w, preview_h);
    CLOGD("[STR_PREVIEW] Y(%p) UV(%p)", Y, UV);
    CLOGD("[STR_PREVIEW] rotaition(%d)", extraData.orientation);
    CLOGD("[STR_PREVIEW] camera id(%d)", getCameraId());
    CLOGD("[STR_PREVIEW] face num(%d)", faceNum);
    CLOGD("[STR_PREVIEW] hdr mode(%d)", hdrData);
    CLOGD("[STR_PREVIEW] flash mode(%d)", flashData);
*/
    /* Start processing */
    m_timer.start();
    ret = uni_plugin_process(m_STRPreviewPluginHandle);
    if (ret < 0) {
        ALOGE("[STR_PREVIEW](%s[%d]): STR Preview plugin process failed!!", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    /* CLOGD("[STR_PREVIEW]duration time(%5d msec)", (int)durationTime); */

    return ret;
}
#endif

#ifdef SUPPORT_DEPTH_MAP
#ifdef SAMSUNG_FOCUS_PEAKING
void ExynosCamera::m_enableDepthMap(void)
{
    m_previewFrameFactory->setRequest(PIPE_VC1, true);
    m_parameters->setDepthCallbackOnPreview(true);
    m_flagFocusPeakingStarted = false;
    CLOGD("[FocusPeaking] Enable depth callback on preview ");
}
void ExynosCamera::m_disableDepthMap(void)
{
    m_previewFrameFactory->setRequest(PIPE_VC1, false);
    m_parameters->setDepthCallbackOnPreview(false);
    m_flagFocusPeakingStarted = false;
    CLOGD("[FocusPeaking] disable depth callback on preview ");
}
void ExynosCamera::m_processFocusPeaking(char *Y, char *UV, ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginBufferData_t depthMapInfo;
    UniPluginExtraBufferInfo_t extraInfo;
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
    int preview_w = 0 , preview_h = 0;
    int depthW = 0, depthH = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    if (m_depthCallbackQ->getSizeOfProcessQ() > 0) {
        ret = m_depthCallbackQ->waitAndPopProcessQ(&depthMapBuffer);
        CLOGD("[FocusPeaking] pop depthBuffer (%d)", depthMapBuffer.index);

        if (ret != NO_ERROR) {
            CLOGE("[FocusPeaking] Failed to get DepthMap buffer");
            if (ret == TIMED_OUT) {
                /* Face Detection time out is not meaningful */
                CLOGE("m_depthCallbackQ time out, status(%d)", ret);
            }
        } else {
            if (m_flagFocusPeakingStarted == false) {
                /* the first frame with a valid depthmap info becomes the starting point of focus peaking. */
                if (depthMapBuffer.index >= 0 && depthMapBuffer.addr[0] != NULL)
                    m_flagFocusPeakingStarted = true;
            }
        }
    }

    if (m_flagFocusPeakingStarted == false) {
        CLOGD("[FocusPeaking] m_flagFocusPeakingStarted false (%d)", depthMapBuffer.index);
        return;
    }

    /* set depthMap Info : If depthMap info is invaild, do not set it. */
    if (depthMapBuffer.index >= 0 && depthMapBuffer.addr[0] != NULL) {
        m_parameters->getDepthMapSize(&depthW, &depthH);

        memset(&depthMapInfo, 0, sizeof(UniPluginBufferData_t));
        depthMapInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_DEPTHMAP;
        depthMapInfo.InBuffY = depthMapBuffer.addr[0];
        depthMapInfo.InWidth = depthW;
        depthMapInfo.InHeight= depthH;
        depthMapInfo.InFormat = UNI_PLUGIN_FORMAT_RAW10;

        ret = uni_plugin_set(m_FocusPeakingPluginHandle,
                FOCUS_PEAKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &depthMapInfo);
        if (ret < 0) {
            CLOGE("[FocusPeaking](%s[%d]): Plugin set DepthMap (UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
        }
    } else {
        CLOGD("[FocusPeaking] not use depthMap(%d)", depthMapBuffer.index);
    }

    /* set buffer Info */
    memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
    bufferInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
    m_parameters->getPreviewSize(&preview_w, &preview_h);
    bufferInfo.InBuffY = Y;
    bufferInfo.InBuffU = UV;
    bufferInfo.InWidth = preview_w;
    bufferInfo.InHeight= preview_h;
    bufferInfo.InFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.OutBuffY = Y;
    bufferInfo.OutBuffU = UV;
    bufferInfo.OutWidth = preview_w;
    bufferInfo.OutHeight= preview_h;

    ret = uni_plugin_set(m_FocusPeakingPluginHandle,
            FOCUS_PEAKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[FocusPeaking](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* set zoom Info */
    memset(&extraInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraInfo.zoomRatio = m_parameters->getZoomRatio();

    ret = uni_plugin_set(m_FocusPeakingPluginHandle,
            FOCUS_PEAKING_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraInfo);
    if (ret < 0) {
        CLOGE("[FocusPeaking](%s[%d]): Plugin set zoomInfo (UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Start processing */
    m_timer.start();
    ret = uni_plugin_process(m_FocusPeakingPluginHandle);
    if (ret < 0) {
        ALOGE("[FocusPeaking](%s[%d]): FocusPeaking plugin process failed!!", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }
    m_timer.stop();
    durationTime = m_timer.durationMsecs();

    CLOGD("[FocusPeaking] duration time(%5d msec) (%d)", (int)durationTime, depthMapBuffer.index);

CLEAN:
    if (depthMapBuffer.index >= 0) {
        ret = m_putBuffers(m_depthMapBufferMgr, depthMapBuffer.index);
        if (ret != NO_ERROR) {
            CLOGE("Failed to put DepthMap buffer to bufferMgr");
        }
    }
}
#endif
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
    } else if (pictureW == 4032 && pictureH == 1960) {
        skipLensDC = false;
        lensDCIndex = 16;
        map[0] = map7_x;
        map[1] = map7_y;
    } else {
        skipLensDC = true;
        lensDCIndex = 0;
        map[0] = NULL;
        map[1] = NULL;
    }

    if ((m_LensDCIndex == -1 || (m_LensDCIndex != -1 && m_LensDCIndex != lensDCIndex))
            && !skipLensDC) {
        CLOGD("[DC] Change Picture size. map change (%d)->(%d)",
                 m_LensDCIndex, lensDCIndex);

        if (m_DCpluginHandle != NULL) {
            if (m_LensDCIndex > 0) {
                    ret = uni_plugin_deinit(m_DCpluginHandle);
                    if (ret < 0) {
                        CLOGE("[DC] Plugin deinit failed!!");
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
                CLOGE("[DC] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
            }

            ret = uni_plugin_set(m_DCpluginHandle,
                                    LDC_PLUGIN_NAME, UNI_PLUGIN_INDEX_PRIV_INFO, &pluginPrivInfo);
            if (ret < 0) {
                CLOGE("[DC] Plugin set(UNI_PLUGIN_INDEX_DISTORTION_INFO) failed!!");
            }

            ret = uni_plugin_init(m_DCpluginHandle);
            if (ret < 0) {
                CLOGE("[DC] Plugin init failed!!");
            }
        }
    }

    m_skipLensDC = skipLensDC;
    m_LensDCIndex = lensDCIndex;
}
#endif

#ifdef SAMSUNG_IDDQD
void ExynosCamera::m_processIDDQD(char *Y, char *UV, int size, struct camera2_shot_ext *meta_shot_ext)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginExtraBufferInfo_t extraData;
    UniPluginPrivInfo_t privInfo;
    gyroEvent_t gyroInfo;
    int preview_w = 0 , preview_h = 0;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    /* set buffer Info */
    memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
    bufferInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
    m_parameters->getPreviewSize(&preview_w, &preview_h);
    bufferInfo.InBuffY = Y;
    bufferInfo.InBuffU = UV;
    bufferInfo.InWidth = preview_w;
    bufferInfo.InHeight= preview_h;
    bufferInfo.InFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.Size = size;
    bufferInfo.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
    CLOGV("[IDDQD] prieviewSize %d x %d, size %d, CameraType %d",
        bufferInfo.InWidth, bufferInfo.InHeight, bufferInfo.Size, bufferInfo.CameraType);
    ret = uni_plugin_set(m_IDDQDPluginHandle,
            IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    	return;
    }

    /* Set extraData Info */
    memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraData.zoomLevel = (UNI_PLUGIN_ZOOMLEVEL)m_parameters->getZoomLevel();
    extraData.brightnessValue.snum = meta_shot_ext->shot.udm.ae.vendorSpecific[5];
    extraData.brightnessValue.den = 256;
    extraData.iso[0] = meta_shot_ext->shot.udm.ae.vendorSpecific[391]; // short ISO
    extraData.iso[1] = meta_shot_ext->shot.udm.ae.vendorSpecific[393]; // Long ISO
    extraData.CameraType = (UNI_PLUGIN_CAMERA_TYPE)getCameraIdInternal();
    CLOGV("[IDDQD] zoomLevel %d, BV snum %d, BV den %d, CameraType %d",
        extraData.zoomLevel, extraData.brightnessValue.snum, extraData.brightnessValue.den, extraData.CameraType);    
    ret = uni_plugin_set(m_IDDQDPluginHandle,
            IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Set priv Info */
    memset(&privInfo, 0, sizeof(UniPluginPrivInfo_t));
    privInfo.priv[0] = meta_shot_ext->shot.udm.ae.vendorSpecific;
    ret = uni_plugin_set(m_IDDQDPluginHandle,
            IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_PRIV_INFO, &privInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_PRIV_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Set gyro Info */
    gyroInfo = m_gyroListenerData.gyro;
    CLOGV("[IDDQD] gyro %f, %f, %f ", gyroInfo.x, gyroInfo.y, gyroInfo.z);
    ret = uni_plugin_set(m_IDDQDPluginHandle,
            IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_SENSOR_DATA, &gyroInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_SENSOR_DATA) failed!!", __FUNCTION__, __LINE__);
    }

    /* Start processing */
    m_timer.start();
    ret = uni_plugin_process(m_IDDQDPluginHandle);
    if (ret < 0) {
        ALOGE("[IDDQD](%s[%d]): IDDQD plugin process failed!!", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }

    UTstr debugData; 
    unsigned char IDDQDbuf[10]; 
    debugData.data = IDDQDbuf; 
    debugData.size = 10; 
    ret = uni_plugin_get(m_IDDQDPluginHandle,
            IDDQD_PLUGIN_NAME, UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): get debug info failed!!", __FUNCTION__, __LINE__);
    }

    if (debugData.data[0] != '0') 
        CLOGV("[IDDQD](%s[%d]): DIRT=%c", __FUNCTION__, __LINE__, debugData.data[0]); 

    m_lens_dirty_detected = debugData.data[0] - '0';

    m_timer.stop();
    durationTime = m_timer.durationMsecs();

    CLOGD("[IDDQD] duration time(%5d msec)", (int)durationTime);
}
#endif

#ifdef USE_ODC_CAPTURE
status_t ExynosCamera::m_processODC(ExynosCameraFrameSP_sptr_t newFrame)
{
    status_t ret = NO_ERROR;
    int nRetry = 10;
    bool bExitLoop = false;

    const int ERR_NDONE = -1;
    const int ERR_TIMEOUT = -2;
    int errState = 0;

    ExynosCameraBuffer odcReprocessingBuffer;
    ExynosCameraBuffer odcReprocessedBuffer;
    ExynosCameraBufferManager* odcReprocessingBufferMgr = NULL;

    int pipeId_scc = 0;
    int pipeId_gsc = 0;

    int processedBufferPos = PIPE_MCSC3_REPROCESSING;

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->isReprocessing3aaIspOTF() == false)
            pipeId_scc = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;

        pipeId_gsc = PIPE_GSC_REPROCESSING4;
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
                CLOGE("Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                        getCameraIdInternal(), m_parameters->isReprocessing());
                break;
        }
    }

    /* Get MCSC3 Buffer */
    ret = newFrame->getDstBuffer(pipeId_scc, &odcReprocessingBuffer,
                                    m_reprocessingFrameFactory->getNodeType(PIPE_MCSC3_REPROCESSING));
    if (ret < NO_ERROR) {
        CLOGE("Failed to getDstBuffer for MCSC1. pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

    /* Set dst buffer(MCSC3) to src buffer of(TPU1) */
    ret = newFrame->setSrcBuffer(PIPE_TPU1, odcReprocessingBuffer);
    if (ret < NO_ERROR) {
        CLOGE("Failed to setSrcBuffer for TPU1. pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

    newFrame->setRequest(PIPE_TPU1, true);
    newFrame->setRequest(PIPE_TPU1C, true);

    /* Push frame to TPU1 pipe */
    CLOGD("Push frame to PIPE_TPU1");
    m_reprocessingFrameFactory->setOutputFrameQToPipe(m_ODCProcessingQ, PIPE_TPU1);
    m_reprocessingFrameFactory->pushFrameToPipe(newFrame, PIPE_TPU1);

    /* Waiting ODC processing done */
    do {
        if (errState == ERR_NDONE)
            m_reprocessingFrameFactory->pushFrameToPipe(newFrame, PIPE_TPU1);

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
            ret = newFrame->getDstBufferState(PIPE_TPU1, &tpu1BufferState,
                                            m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C));
            if (ret < NO_ERROR) {
                CLOGE("getDstBufferState fail, pipeId(%d), nodeType(%d), ret(%d)",
                            PIPE_TPU1,
                            m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C),
                            ret);
            }

            if (tpu1BufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                bExitLoop = true;
            } else {
                /* Get ODC processed image buffer from TPU1C */
                ret = newFrame->getDstBuffer(PIPE_TPU1, &odcReprocessedBuffer,
                                            m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C));
                if (ret < NO_ERROR) {
                    CLOGE("Failed to getDstBuffer for TPU1(ODC). buffer state(%d), pipeId(%d), ret(%d)", tpu1BufferState, PIPE_TPU1, ret);
                } else {
                    /* Release src buffer from TPU1C */
                    odcReprocessingBufferMgr = NULL;
                    ret = m_getBufferManager(PIPE_TPU1, &odcReprocessingBufferMgr, DST_BUFFER_DIRECTION);
                    if (ret < NO_ERROR) {
                        CLOGE("Failed to get bufferManager for %s, %s", "PIPE_TPU1", "DST_BUFFER_DIRECTION");
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
    ret = newFrame->getDstBuffer(PIPE_TPU1, &odcReprocessedBuffer,
                                    m_reprocessingFrameFactory->getNodeType(PIPE_TPU1C));
    if (ret < NO_ERROR) {
        CLOGE("Failed to getDstBuffer for TPU1(ODC). pipeId %d ret %d",
                pipeId_scc, ret);
        return ret;
    }

    /* replace MCSC1 buffer by ODC processed buffer */
    newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_REQUESTED,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to getDstBuffer for MCSC3. pipeId %d ret %d", pipeId_scc, ret);
        return ret;
    }

    newFrame->setDstBuffer(pipeId_scc, odcReprocessedBuffer,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to set processed buffer to MCSC3. pipeId %d ret %d", pipeId_scc, ret);
        return ret;
    }

    newFrame->setDstBufferState(pipeId_scc, ENTITY_BUFFER_STATE_COMPLETE,
                                    m_reprocessingFrameFactory->getNodeType(processedBufferPos));
    if (ret < NO_ERROR) {
        CLOGE("Failed to setDstBufferState to MCSC3. pipeId %d ret %d", pipeId_scc, ret);
        return ret;
    }

    /* Release src buffer from MCSC3 */
    ret = m_getBufferManager(PIPE_MCSC3_REPROCESSING, &odcReprocessingBufferMgr, DST_BUFFER_DIRECTION);
    if (ret < NO_ERROR) {
        CLOGE("Failed to get bufferManager for %s, %s", "PIPE_MCSC3_REPROCESSING", "DST_BUFFER_DIRECTION");
    }

    ret = m_putBuffers(odcReprocessingBufferMgr, odcReprocessingBuffer.index);
    if (ret < NO_ERROR) {
        CLOGE("Failed to putBuffer index(%d)", odcReprocessingBuffer.index);
    }

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

    if (m_parameters->isReprocessing() == true) {
        if (m_parameters->isReprocessing3aaIspOTF() == false)
            pipeId_scc = (m_parameters->isOwnScc(getCameraIdInternal()) == true) ? PIPE_SCC_REPROCESSING : PIPE_ISP_REPROCESSING;
        else
            pipeId_scc = PIPE_3AA_REPROCESSING;
#ifdef USE_DUAL_CAMERA
        if (m_parameters->getDualCameraMode() == true && newFrame->getSyncType() == SYNC_TYPE_SYNC) {
            pipeId_scc = PIPE_FUSION_REPROCESSING;
        }
#endif
        pipeId_gsc = PIPE_GSC_REPROCESSING4;
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
                CLOGE("Current picture mode is not yet supported, CameraId(%d), reprocessing(%d)",
                        getCameraIdInternal(), m_parameters->isReprocessing());
                break;
        }
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
        gscReprocessingBufferMgr = m_sccReprocessingBufferMgr;
    } else {
        processedBufferPos = PIPE_MCSC3_REPROCESSING;
        gscReprocessingBufferMgr = m_sccReprocessingBufferMgr;
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

#ifdef SAMSUNG_UNIPLUGIN
UNI_PLUGIN_HDR_MODE ExynosCamera::m_getUniHDRMode(void)
{
    UNI_PLUGIN_HDR_MODE hdrData = UNI_PLUGIN_HDR_MODE_OFF;

    switch (m_parameters->getRTHdr()) {
        case COMPANION_WDR_OFF:
            hdrData = UNI_PLUGIN_HDR_MODE_OFF;
            break;
        case COMPANION_WDR_AUTO:
            hdrData = UNI_PLUGIN_HDR_MODE_AUTO;
            break;
        case COMPANION_WDR_ON:
            hdrData = UNI_PLUGIN_HDR_MODE_ON;
            break;
    }
    return hdrData;
}

UNI_PLUGIN_FLASH_MODE ExynosCamera::m_getUniFlashMode(void)
{
    UNI_PLUGIN_FLASH_MODE flashData = UNI_PLUGIN_FLASH_MODE_OFF;

    switch (m_parameters->getFlashMode()) {
        case FLASH_MODE_OFF:
            flashData = UNI_PLUGIN_FLASH_MODE_OFF;
            break;
        case FLASH_MODE_AUTO:
            flashData = UNI_PLUGIN_FLASH_MODE_AUTO;
            break;
        case FLASH_MODE_ON:
            flashData = UNI_PLUGIN_FLASH_MODE_ON;
            break;
    }
    return flashData;
}

UNI_PLUGIN_DEVICE_ORIENTATION ExynosCamera::m_getUniOrientationMode(void)
{
    /* translate Orientation enum(match to exif data) for uniPlugin */
    UNI_PLUGIN_DEVICE_ORIENTATION orientation = UNI_PLUGIN_ORIENTATION_0;

    if (getCameraIdInternal() == CAMERA_ID_FRONT) {
        /* please check when flip horizontal is on */
        switch (m_parameters->getDeviceOrientation()) {
            case 0:
                orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case 90:
                orientation = UNI_PLUGIN_ORIENTATION_180;
                break;
            case 180:
                orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            case 270:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
            default:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
        }
    } else {
        switch (m_parameters->getDeviceOrientation()) {
            case 0:
                orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case 90:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
            case 180:
                orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            case 270:
                orientation = UNI_PLUGIN_ORIENTATION_180;
                break;
            default:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
        }
    }
    return orientation;
}
#endif

void ExynosCamera::m_printVendorCameraInfo(void)
{
#ifdef USE_REMOSAIC_CAPTURE
    CLOGD("getAvailableRemosaicCapture	   : %d", m_parameters->getAvailableRemosaicCapture());
#endif
}
}; /* namespace android */
