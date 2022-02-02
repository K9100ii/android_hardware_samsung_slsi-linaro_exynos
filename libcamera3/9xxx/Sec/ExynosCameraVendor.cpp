/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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
#include <log/log.h>

#include "ExynosCamera.h"
#include "fimc-is-metadata.h"

namespace android {

void ExynosCamera::m_vendorSpecificPreConstructor(int cameraId, int scenario)
{
    CLOGI("-IN-");

    m_vendorSpecificPreConstructorInitalize(cameraId, scenario);

    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificConstructor(void)
{
    CLOGI("-IN-");

    m_thumbnailCbQ = new frame_queue_t;
    m_thumbnailCbQ->setWaitTime(1000000000);

    m_thumbnailPostCbQ = new frame_queue_t;
    m_thumbnailPostCbQ->setWaitTime(1000000000);

    m_resizeDoneQ = new frame_queue_t;
    m_resizeDoneQ->setWaitTime(1000000000);

    m_currentMultiCaptureMode = MULTI_CAPTURE_MODE_NONE;
    m_lastMultiCaptureServiceRequest = -1;
    m_lastMultiCaptureSkipRequest = -1;
    m_lastMultiCaptureNormalRequest = -1;
    m_doneMultiCaptureRequest = -1;

    m_previewDurationTime = 0;
    m_captureResultToggle = 0;
    m_displayPreviewToggle = 0;

    m_longExposureRemainCount = 0;
    m_stopLongExposure = false;
    m_preLongExposureTime = 0;

    for (int i = 0; i < 4; i++)
        m_burstFps_history[i] = -1;

    m_flagUseInternalyuvStall = false;
    m_flagVideoStreamPriority = false;

    memset(&m_stats, 0x00, sizeof(struct camera2_stats_dm));

    m_shutterSpeed = 0;
    m_gain = 0;
    m_irLedWidth = 0;
    m_irLedDelay = 0;
    m_irLedCurrent = 0;
    m_irLedOnTime = 0;

    m_vendorSpecificConstructorInitalize();

    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificPreDestructor(void)
{
    CLOGI("-IN-");

    m_vendorSpecificPreDestructorDeinitalize();

    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI("-IN-");

    m_thumbnailCbThread->requestExitAndWait();

    if (m_thumbnailCbQ != NULL) {
        delete m_thumbnailCbQ;
        m_thumbnailCbQ = NULL;
    }

    if (m_thumbnailPostCbQ != NULL) {
        delete m_thumbnailPostCbQ;
        m_thumbnailPostCbQ = NULL;
    }

    if (m_resizeDoneQ != NULL) {
        delete m_resizeDoneQ;
        m_resizeDoneQ = NULL;
    }

    m_vendorSpecificDestructorDeinitalize();

    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorCreateThreads(void)
{
    /* m_ThumbnailCallback Thread */
    m_thumbnailCbThread = new mainCameraThread(this, &ExynosCamera::m_thumbnailCbThreadFunc, "m_thumbnailCbThread");
    CLOGD("m_thumbnailCbThread created");

    return;
}

status_t ExynosCamera::m_vendorReInit(void)
{
    m_captureResultToggle = 0;
    m_displayPreviewToggle = 0;
#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif
    m_flagUseInternalyuvStall = false;
    m_flagVideoStreamPriority = false;

    return NO_ERROR;
}

status_t ExynosCamera::releaseDevice(void)
{
    status_t ret = NO_ERROR;
    CLOGD("");
#ifdef TIME_LOGGER_CLOSE_ENABLE
    TIME_LOGGER_INIT(m_cameraId);
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, RELEASE_DEVICE_START, 0);
#endif
    setPreviewProperty(false);

    m_setBuffersThread->requestExitAndWait();
    m_startPictureBufferThread->requestExitAndWait();
    m_framefactoryCreateThread->requestExitAndWait();
    m_monitorThread->requestExit();

    if (m_getState() > EXYNOS_CAMERA_STATE_CONFIGURED) {
        flush();
    }

    m_deinitBufferSupplierThread = new mainCameraThread(this, &ExynosCamera::m_deinitBufferSupplierThreadFunc, "deinitBufferSupplierThread");
    m_deinitBufferSupplierThread->run();
    CLOGD("Deinit Buffer Supplier Thread created");

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, MONITOR_THREAD_STOP_START, 0);
    m_monitorThread->requestExitAndWait();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, MONITOR_THREAD_STOP_END, 0);

    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, RELEASE_DEVICE_END, 0);
    return ret;
}

status_t ExynosCamera::m_constructFrameFactory(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("-IN-");

    ExynosCameraFrameFactory *factory = NULL;
#ifdef USE_DUAL_CAMERA
    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
#endif

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == false) {
        /* Preview Frame Factory */
        if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] == NULL
            ) {
#ifdef USE_DUAL_CAMERA
            if (isDualMode == true) {
                factory = new ExynosCameraFrameFactoryPreviewDual(m_cameraId, m_configurations, m_parameters[m_cameraId]);
            } else
#endif
            if (m_cameraId == CAMERA_ID_FRONT && m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
                factory = new ExynosCameraFrameFactoryPreviewFrontPIP(m_cameraId, m_configurations, m_parameters[m_cameraId]);
            } else {
                factory = new ExynosCameraFrameFactoryPreview(m_cameraId, m_configurations, m_parameters[m_cameraId]);
            }
            factory->setFrameCreateHandler(&ExynosCamera::m_previewFrameHandler);
            factory->setFrameManager(m_frameMgr);
            factory->setFactoryType(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW);
            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] = factory;
            m_frameFactoryQ->pushProcessQ(&factory);
        }

#ifdef USE_DUAL_CAMERA
        /* Dual Frame Factory */
        if (isDualMode == true
            && m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL] == NULL) {
            factory = new ExynosCameraFrameFactoryPreview(m_cameraIds[1], m_configurations, m_parameters[m_cameraIds[1]]);
            factory->setFrameCreateHandler(&ExynosCamera::m_previewFrameHandler);
            factory->setFrameManager(m_frameMgr);
            factory->setFactoryType(FRAME_FACTORY_TYPE_PREVIEW_DUAL);
            m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL] = factory;
            m_frameFactoryQ->pushProcessQ(&factory);
        }
#endif

        /* Reprocessing Frame Factory */
        if (m_parameters[m_cameraId]->isReprocessing() == true) {
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] == NULL) {
                if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5 && m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
                    CLOGW("skip create framefactory(%d) mcscOutputPort(%d) PidMode(%d)", FRAME_FACTORY_TYPE_REPROCESSING , m_parameters[m_cameraId]->getNumOfMcscOutputPorts(), m_configurations->getMode(CONFIGURATION_PIP_MODE));
                } else {
#ifdef USE_DUAL_CAMERA
                if (isDualMode == true) {
                    factory = new ExynosCameraFrameReprocessingFactoryDual(m_cameraId, m_configurations, m_parameters[m_cameraId]);
                } else
#endif
                {
                    factory = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_configurations, m_parameters[m_cameraId]);
                }
                factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                factory->setFrameManager(m_frameMgr);
                factory->setFactoryType(FRAME_FACTORY_TYPE_REPROCESSING);
                m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] = factory;
                m_frameFactoryQ->pushProcessQ(&factory);
                }

                if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5 &&
#ifdef USE_DUAL_CAMERA
                    isDualMode == false &&
#endif
                        m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] == NULL) {
                    factory = new ExynosCameraFrameJpegReprocessingFactory(m_cameraId, m_configurations, m_parameters[m_cameraId]);
                    factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                    factory->setFrameManager(m_frameMgr);
                    factory->setFactoryType(FRAME_FACTORY_TYPE_JPEG_REPROCESSING);
                    m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] = factory;
                    m_frameFactoryQ->pushProcessQ(&factory);
                }

#ifdef USE_DUAL_CAMERA
                if (isDualMode == true
                        && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] == NULL) {
                    factory = new ExynosCameraFrameReprocessingFactory(m_cameraIds[1], m_configurations, m_parameters[m_cameraIds[1]]);
                    factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                    factory->setFrameManager(m_frameMgr);
                    factory->setFactoryType(FRAME_FACTORY_TYPE_REPROCESSING_DUAL);
                    m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] = factory;
                    m_frameFactoryQ->pushProcessQ(&factory);
                }
#endif
            }
        }
    } else {
        if (m_frameFactory[FRAME_FACTORY_TYPE_VISION] == NULL) {
            /* Vision Frame Factory */
            factory = new ExynosCameraFrameFactoryVision(m_cameraId, m_configurations, m_parameters[m_cameraId]);
            factory->setFrameCreateHandler(&ExynosCamera::m_visionFrameHandler);
            factory->setFrameManager(m_frameMgr);
            factory->setFactoryType(FRAME_FACTORY_TYPE_VISION);
            m_frameFactory[FRAME_FACTORY_TYPE_VISION] = factory;
            m_frameFactoryQ->pushProcessQ(&factory);
        }
    }

    CLOGI("-OUT-");

    return NO_ERROR;
}

bool ExynosCamera::m_frameFactoryStartThreadFunc(void)
{
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    int32_t reprocessingBayerMode;
    uint32_t prepare = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    bool startBayerThreads = false;
    bool isVisionMode = m_configurations->getMode(CONFIGURATION_VISION_MODE);
#ifdef USE_DUAL_CAMERA
    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
    enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();
    enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
    bool needSensorStreamOn = true;
#endif
    ExynosCameraFrameFactory *factory;
    ExynosCameraParameters *parameters;

    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_START_THREAD_START, 0);

    if (m_requestMgr->getServiceRequestCount() < 1) {
        CLOGE("There is NO available request!!! "
            "\"processCaptureRequest()\" must be called, first!!!");
        return false;
    }

    m_internalFrameCount = 1;

    if (m_shotDoneQ != NULL) {
        m_shotDoneQ->release();
    }
#ifdef USE_DUAL_CAMERA
    if (isDualMode == true) {
        if (m_slaveShotDoneQ != NULL) {
            m_slaveShotDoneQ->release();
        }

        m_latestRequestListLock.lock();
        m_latestRequestList.clear();
        m_essentialRequestList.clear();
        m_latestRequestListLock.unlock();


        m_dualTransitionCount = 0;
        m_dualCaptureLockCount = 0;
        m_dualMultiCaptureLockflag = false;
        m_earlyTriggerRequestKey = 0;
    }
#endif

    for (int i = 0; i < MAX_PIPE_NUM; i++) {
        if (m_pipeFrameDoneQ[i] != NULL) {
            m_pipeFrameDoneQ[i]->release();
        }
    }

    m_yuvCaptureDoneQ->release();
    m_reprocessingDoneQ->release();
    m_bayerStreamQ->release();

    if (isVisionMode == false) {
#ifdef USE_DUAL_CAMERA
        if (isDualMode == true && dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            parameters = m_parameters[m_cameraIds[1]];
        } else
#endif
        {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            parameters = m_parameters[m_cameraId];
        }
    } else {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
        parameters = m_parameters[m_cameraId];
    }

    if (factory == NULL) {
        CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!! Prepare(%d), Request(%d)",
                prepare,
                m_requestMgr != NULL ? m_requestMgr->getAllRequestCount(): 0);

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("Preview FrameFactory is NOT created!");
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("Preview FrameFactory is already running.");
        return false;
    }

    /* To decide to run FLITE threads  */
    reprocessingBayerMode = parameters->getReprocessingBayerMode();
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        startBayerThreads = true;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        startBayerThreads = (parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
        break;
    default :
        break;
    }

    /* Adjust Prepare Frame Count
     * If current sensor control delay is 0,
     * Prepare frame count must be reduced by #sensor control delay.
     */
    prepare = prepare + parameters->getSensorControlDelay() + 1;
    CLOGD("prepare %d", prepare);

#ifdef USE_DUAL_CAMERA
    if (isDualMode == true) {
        m_flagFinishDualFrameFactoryStartThread = false;
        m_prepareFrameCount = prepare;

        if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
            /*
             * DualFrameFactoryStartThread Starting Timing
             *  SYNC mode  : (parallel) before starting master's initPipe
             *  other mode : (serial  ) after finishing this frameFactoryStartThread
             */
            m_dualFrameFactoryStartThread->run();
        }
    }
#endif

    /* Set default request flag & buffer manager */
    if (isVisionMode == false) {
        ret = m_setupPipeline(factory);
    } else {
        ret = m_setupVisionPipeline();
    }

    if (ret != NO_ERROR) {
        CLOGE("Failed to setupPipeline. ret %d", ret);
        return false;
    }

    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_START, 0);
    ret = factory->initPipes();
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_END, 0);
    if (ret != NO_ERROR) {
        CLOGE("Failed to initPipes. ret %d", ret);
        return false;
    }

    if (isVisionMode == false) {
        ret = factory->mapBuffers();
        if (ret != NO_ERROR) {
            CLOGE("m_previewFrameFactory->mapBuffers() failed");
            return ret;
        }
    }

    for (uint32_t i = 0; i < prepare; i++) {
        if (isVisionMode == false) {
            ret = m_createPreviewFrameFunc(REQ_SYNC_NONE, false /* flagFinishFactoryStart */);
        } else {
            ret = m_createVisionFrameFunc(REQ_SYNC_NONE, false /* flagFinishFactoryStart */);
        }

        if (ret != NO_ERROR) {
            CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                     i, prepare);
        }
    }

#ifdef USE_DUAL_CAMERA
    if (isDualMode == true) {
        /* init standby state */
        ret = factory->sensorStandby(!needSensorStreamOn);
        if (ret != NO_ERROR)
            CLOGE("sensorStandby(%d) fail! ret(%d)", !needSensorStreamOn, ret);

        factory->setNeedSensorStreamOn(needSensorStreamOn);
        parameters->setStandbyState(needSensorStreamOn ? DUAL_STANDBY_STATE_OFF : DUAL_STANDBY_STATE_ON);

        m_configurations->setDualOperationModeLockCount(0);
        android_atomic_and(0, &m_needSlaveDynamicBayerCount);

        CLOGD("Factory NeedStreamOn(%d), dualOperationMode(%d), switching(%d), UHD(%d), scenario(%d)",
               needSensorStreamOn, dualOperationMode,
               m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING),
               m_configurations->getDynamicMode(DYNAMIC_UHD_RECORDING_MODE),
               m_scenario);
    }
#endif

    /* - call preparePipes();
     * - call startPipes()
     * - call startInitialThreads()
     */
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_STREAM_START_START, 0);
    m_startFrameFactory(factory);
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_STREAM_START_END, 0);

#ifdef USE_DUAL_CAMERA
    if (isDualMode == true && dualOperationMode != DUAL_OPERATION_MODE_SYNC) {
        /*
         * When it's dual mode, we'll delay starting all kinds of reprocessingFactory factory
         * to the time when dualFrameFactoryStartThread finished. Because reprocessing instance
         * should be created after creation of preview instance.
         */
        CLOGI("m_dualFrameFactoryStartThread run E");
        m_dualFrameFactoryStartThread->run();
        CLOGI("m_dualFrameFactoryStartThread run X");
    } else
#endif
    {
        if (parameters->isReprocessing() == true && m_captureStreamExist == true) {
            CLOGI("m_reprocessingFrameFactoryStartThread run E");
            m_reprocessingFrameFactoryStartThread->run();
            CLOGI("m_reprocessingFrameFactoryStartThread run X");
        }
    }

    if (isVisionMode == false) {
        CLOGI("m_previewStream3AAThread run E");
        m_previewStream3AAThread->run(PRIORITY_URGENT_DISPLAY);
        CLOGI("m_previewStream3AAThread run X");

        if (parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
            CLOGI("m_previewStreamISPThread run E");
            m_previewStreamISPThread->run(PRIORITY_URGENT_DISPLAY);
            CLOGI("m_previewStreamISPThread run X");
        }

        if (parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
            CLOGI("m_previewStreamMCSCThread run E");
            m_previewStreamMCSCThread->run(PRIORITY_URGENT_DISPLAY);
            CLOGI("m_previewStreamMCSCThread run X");
        }

        if (parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            CLOGI("m_previewStreamVRAThread run E");
            m_previewStreamVRAThread->run(PRIORITY_URGENT_DISPLAY);
            CLOGI("m_previewStreamVRAThread run X");
        }

#ifdef USE_DUAL_CAMERA
        if (isDualMode == true) {
            CLOGI("m_previewStreamSyncThread run E");
            m_previewStreamSyncThread->run(PRIORITY_URGENT_DISPLAY);
            CLOGI("m_previewStreamSyncThread run X");

            if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                CLOGI("m_previewStreamFusionThread run E");
                m_previewStreamFusionThread->run(PRIORITY_URGENT_DISPLAY);
                CLOGI("m_previewStreamFusionThread run X");
            }
        }
#endif
    }

    if (startBayerThreads == true) {
        CLOGI("m_previewStreamBayerThread run E");
        m_previewStreamBayerThread->run(PRIORITY_URGENT_DISPLAY);
        CLOGI("m_previewStreamBayerThread run X");
        if (factory->checkPipeThreadRunning(m_getBayerPipeId()) == false) {
            CLOGI("factory->startThread run E");
            factory->startThread(m_getBayerPipeId());
            CLOGI("factory->startThread run X");
        }
    }

    CLOGI("m_mainPreviewThread run E");
    m_mainPreviewThread->run(PRIORITY_URGENT_DISPLAY);
    CLOGI("m_mainPreviewThread run X");

    CLOGI("m_monitorThread run E");
    m_monitorThread->run(PRIORITY_DEFAULT);
    CLOGI("m_monitorThread run X");

    ret = m_transitState(EXYNOS_CAMERA_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into RUN. ret %d", ret);
    }

    return false;
}

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_dualFrameFactoryStartThreadFunc(void)
{
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    int32_t reprocessingBayerMode;
    uint32_t prepare = m_prepareFrameCount;
    bool startBayerThreads = false;

    bool isDualMode = m_configurations->getMode(CONFIGURATION_DUAL_MODE);
    enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();

    ExynosCameraFrameFactory *factory;
    ExynosCameraParameters *parameters;

    bool supportSensorStandby = false;
    bool needSensorStreamOn = false;
    bool standby = true;

    if (isDualMode == false)
        return false;

    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_PREVIEW_DUAL, 0, CUMULATIVE_CNT, FACTORY_START_THREAD_START, 0);

    switch (dualOperationMode) {
    case DUAL_OPERATION_MODE_SLAVE:
        /* we'11 start master framefactory */
        factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        parameters = m_parameters[m_cameraId];
        break;
    case DUAL_OPERATION_MODE_SYNC:
        standby = false;
        needSensorStreamOn = true;
        /* don't break : falling down */
    default:
        /* we'll start slave framefactory */
        factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        parameters = m_parameters[m_cameraIds[1]];
        break;
    }

    /* To decide to run FLITE threads  */
    reprocessingBayerMode = parameters->getReprocessingBayerMode();
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        startBayerThreads = true;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        startBayerThreads = (parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
        break;
    default :
        break;
    }

    supportSensorStandby = parameters->isSupportedFunction(SUPPORTED_HW_FUNCTION_SENSOR_STANDBY);
    if (supportSensorStandby == false)
        needSensorStreamOn = true;

    if (factory == NULL) {
        CLOGE("Can't start FrameFactory!!!! FrameFactory is NULL!! Prepare(%d), Request(%d)",
                prepare,
                m_requestMgr != NULL ? m_requestMgr->getAllRequestCount(): 0);

        return false;
    } else if (factory->isCreated() == false) {
        CLOGE("Preview FrameFactory is NOT created!");
        return false;
    } else if (factory->isRunning() == true) {
        CLOGW("Preview FrameFactory is already running.");
        return false;
    }

    /* Set default request flag & buffer manager */
    ret = m_setupPipeline(factory);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setupPipeline. ret %d", ret);
        return false;
    }

    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_PREVIEW_DUAL, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_START, 0);
    ret = factory->initPipes();
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_PREVIEW_DUAL, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_END, 0);
    if (ret != NO_ERROR) {
        CLOGE("Failed to initPipes. ret %d", ret);
        return false;
    }

    ret = factory->mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("m_previewFrameFactory->mapBuffers() failed");
        return ret;
    }

    if (needSensorStreamOn == true) {
        if (standby == false) {
            /* in case of standby off */
            int requestListSize = 0;
            int tryCount = 60; /* max : 5ms * 60 = 300ms */
            ExynosCameraRequestSP_sprt_t request = NULL;
            List<ExynosCameraRequestSP_sprt_t>::iterator r;
            factory_handler_t frameCreateHandler = factory->getFrameCreateHandler();

            /* get the latest request */
            while (requestListSize <= 0 && tryCount > 0) {
                m_latestRequestListLock.lock();
                if (m_essentialRequestList.size() > 0) {
                    r = m_essentialRequestList.begin();
                    request = *r;
                    m_essentialRequestList.erase(r);
                } else {
                    requestListSize = m_latestRequestList.size();
                    if (requestListSize > 0) {
                        r = m_latestRequestList.begin();
                        request = *r;
                    }
                }
                m_latestRequestListLock.unlock();
                tryCount--;
                usleep(5000);
                CLOGW("wait for latest request..tryCount(%d)", tryCount);
            }

            for (uint32_t i = 0; i < prepare; i++) {
                if (request != NULL) {
                    (this->*frameCreateHandler)(request, factory, FRAME_TYPE_PREVIEW_DUAL_SLAVE);
                } else {
                    m_createInternalFrameFunc(NULL, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
                }
            }
        } else {
            /* in case of postStandby on */
            for (uint32_t i = 0; i < prepare; i++) {
                ret = m_createInternalFrameFunc(NULL, REQ_SYNC_NONE,
                        dualOperationMode == DUAL_OPERATION_MODE_SLAVE ? FRAME_TYPE_TRANSITION_SLAVE : FRAME_TYPE_TRANSITION);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                            i, prepare);
                }
            }
        }
    }

    /* init standby state */
    ret = factory->sensorStandby(!needSensorStreamOn);
    if (ret != NO_ERROR)
        CLOGE("sensorStandby(%d) fail! ret(%d)", !needSensorStreamOn, ret);

    factory->setNeedSensorStreamOn(needSensorStreamOn);
    parameters->setStandbyState(needSensorStreamOn ? DUAL_STANDBY_STATE_OFF : DUAL_STANDBY_STATE_ON);

    CLOGD("Factory NeedStreamOn(%d) dualOperationMode(%d)",
            needSensorStreamOn, dualOperationMode);

    m_slaveMainThread->run(PRIORITY_URGENT_DISPLAY);

    /* - call preparePipes();
     * - call startPipes()
     * - call startInitialThreads()
     */
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_PREVIEW_DUAL, 0, CUMULATIVE_CNT, FACTORY_STREAM_START_START, 0);
    m_startFrameFactory(factory);
    TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_PREVIEW_DUAL, 0, CUMULATIVE_CNT, FACTORY_STREAM_START_END, 0);
    if (startBayerThreads == true) {
        if (factory->checkPipeThreadRunning(m_getBayerPipeId()) == false) {
            factory->startThread(m_getBayerPipeId());
        }
    }

    m_frameFactoryStartThread->join();
    {
        /* OK. Im finished.. */
        Mutex::Autolock lock(m_dualOperationModeLock);
        m_flagFinishDualFrameFactoryStartThread = true;
    }

    if (parameters->isReprocessing() == true && m_captureStreamExist == true) {
        m_reprocessingFrameFactoryStartThread->run();
    }

    return false;
}
#endif

void ExynosCamera::m_createManagers(void)
{
    status_t ret = NO_ERROR;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;

    if (!m_streamManager) {
        m_streamManager = new ExynosCameraStreamManager(m_cameraId);
        CLOGD("Stream Manager created");
    }

    if (m_bufferSupplier == NULL) {
        m_bufferSupplier = new ExynosCameraBufferSupplier(m_cameraId);
        CLOGD("Buffer Supplier created");
    }

    if (m_ionAllocator == NULL) {
        ret = m_createIonAllocator(&m_ionAllocator);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create ionAllocator. ret %d", ret);
        } else {
            CLOGD("IonAllocator is created");
        }
    }
}

status_t ExynosCamera::m_checkStreamInfo(void)
{
    status_t ret = NO_ERROR;
    /* Check PIP Mode Limitation
     * Max number of YUV stream for each device: 1
     * Max size of YUV stream: FHD(1080p)
     * Each limitation is defined at config.h
     */
    if (m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
        int yuvStreamCount = m_streamManager->getYuvStreamCount();
        int maxYuvW = 0, maxYuvH = 0;
        m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, (uint32_t *)&maxYuvW, (uint32_t *)&maxYuvH);

        if (yuvStreamCount > PIP_CAMERA_MAX_YUV_STREAM
            || maxYuvH > PIP_CAMERA_MAX_YUV_HEIGHT
            || (maxYuvW * maxYuvH) > PIP_CAMERA_MAX_YUV_SIZE) {

            CLOGW("PIP mode NOT support this configuration. " \
                    "YUVStreamCount %d MaxYUVSize %dx%d",
                    yuvStreamCount, maxYuvW, maxYuvH);

            ret = BAD_VALUE;
        }
    }

    m_checkStreamInfo_vendor(ret);

    return ret;
}

status_t ExynosCamera::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    CLOGI("");

    m_configurations->setParameters(params);

    setParameters_vendor(params);

    return ret;
}

status_t ExynosCamera::m_setSetfile(void) {
    int configMode = m_configurations->getConfigMode();
    int setfile = 0;
    int setfileReprocessing = 0;
    int yuvRange = YUV_FULL_RANGE;
    int yuvRangeReprocessing = YUV_FULL_RANGE;

    switch (configMode) {
    case CONFIG_MODE::NORMAL:
    case CONFIG_MODE::HIGHSPEED_60:
        m_parameters[m_cameraId]->checkSetfileYuvRange();
        /* general */
        m_parameters[m_cameraId]->getSetfileYuvRange(false, &setfile, &yuvRange);
        /* reprocessing */
        m_parameters[m_cameraId]->getSetfileYuvRange(true, &setfileReprocessing, &yuvRangeReprocessing);

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            m_parameters[m_cameraIds[1]]->checkSetfileYuvRange();
        }
#endif
        break;
    case CONFIG_MODE::HIGHSPEED_120:
    case CONFIG_MODE::HIGHSPEED_240:
    case CONFIG_MODE::HIGHSPEED_480:    /* HACK: We want new sub scenario setfile index */
        setfile = ISS_SUB_SCENARIO_FHD_240FPS;
        setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
        yuvRange = YUV_LIMITED_RANGE;
        break;
    default:
        CLOGE("configMode is abnormal(%d)", configMode);
        break;
    }

    CLOGI("configMode(%d), PIPMode(%d)", configMode, m_configurations->getMode(CONFIGURATION_PIP_MODE));

    /* reprocessing */
    m_parameters[m_cameraId]->setSetfileYuvRange(true, setfileReprocessing, yuvRangeReprocessing);
    /* preview */
    m_parameters[m_cameraId]->setSetfileYuvRange(false, setfile, yuvRange);

    return NO_ERROR;
}

status_t ExynosCamera::m_restartStreamInternal()
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;
    int count = 0;
#ifdef USE_DUAL_CAMERA
#endif

    CLOGI("IN");

    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGD("No need to m_restartStreamInternal");
        goto func_exit;
    }

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->join();
    m_reprocessingFrameFactoryStartThread->join();

    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

    /* Wait for finishing pre-processing threads*/
    m_mainPreviewThread->requestExitAndWait();
    m_mainCaptureThread->requestExitAndWait();
#ifdef USE_DUAL_CAMERA
    if (m_dualFrameFactoryStartThread != NULL) {
        m_dualFrameFactoryStartThread->join();
        m_dualFrameFactoryStartThread->requestExitAndWait();
    }

    if (m_slaveMainThread != NULL) {
        m_slaveMainThread->requestExitAndWait();
    }

    if (m_dualStandbyThread != NULL) {
        m_dualStandbyThread->requestExitAndWait();
    }
#endif

    /* 1. wait for request callback done.*/
    count = 0;
    while (true) {
        uint32_t requestCnt = 0;
        count++;

        requestCnt = m_requestMgr->getAllRequestCount();

        if (requestCnt == 0)
            break;

        CLOGD("getAllRequestCount size(%d) retry(%d)", requestCnt, count);
        usleep(50000);

        if (count == 200)
            break;
    }

    /* 2. wait for internal frame done.*/
    count = 0;
    while (true) {
        uint32_t processCount = 0;
        uint32_t captureProcessCount = 0;
        count++;
        processCount = m_getSizeFromFrameList(&m_processList, &m_processLock);
        captureProcessCount = m_getSizeFromFrameList(&m_captureProcessList, &m_captureProcessLock);

        if (processCount == 0 && captureProcessCount == 0)
            break;

        CLOGD("processCount size(%d) captureProcessCount size(%d) retry(%d)",
             processCount, captureProcessCount, count);
        usleep(50000);

        if (count == 200)
            break;
    }

    /* Stop pipeline */
    for (int i = 0; i < FRAME_FACTORY_TYPE_MAX; i++) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i + 1; k < FRAME_FACTORY_TYPE_MAX; k++) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance,"
                        " set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            ret = m_stopFrameFactory(m_frameFactory[i]);
            if (ret < 0)
                CLOGE("m_frameFactory[%d] stopPipes fail", i);

            CLOGD("m_frameFactory[%d] stopPipes", i);
        }
    }

    /* Wait for finishing pre-processing threads */
    m_shotDoneQ->release();
#ifdef USE_DUAL_CAMERA
    if (m_slaveShotDoneQ != NULL) {
        m_slaveShotDoneQ->release();
    }
    stopThreadAndInputQ(m_selectDualSlaveBayerThread, 1, m_selectDualSlaveBayerQ);
#endif
    stopThreadAndInputQ(m_selectBayerThread, 1, m_selectBayerQ);

    /* Wait for finishing post-processing thread */
    stopThreadAndInputQ(m_previewStreamBayerThread, 1, m_pipeFrameDoneQ[PIPE_FLITE]);
    stopThreadAndInputQ(m_previewStream3AAThread, 1, m_pipeFrameDoneQ[PIPE_3AA]);
    stopThreadAndInputQ(m_previewStreamISPThread, 1, m_pipeFrameDoneQ[PIPE_ISP]);
    stopThreadAndInputQ(m_previewStreamMCSCThread, 1, m_pipeFrameDoneQ[PIPE_MCSC]);
    stopThreadAndInputQ(m_bayerStreamThread, 1, m_bayerStreamQ);
    stopThreadAndInputQ(m_captureThread, 2, m_captureQ, m_reprocessingDoneQ);
    stopThreadAndInputQ(m_thumbnailCbThread, 2, m_thumbnailCbQ, m_thumbnailPostCbQ);
    stopThreadAndInputQ(m_captureStreamThread, 1, m_yuvCaptureDoneQ);
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    stopThreadAndInputQ(m_reverseProcessingBayerThread, 1, m_reverseProcessingBayerQ);
#endif
#ifdef SUPPORT_HW_GDC
    stopThreadAndInputQ(m_previewStreamGDCThread, 1, m_pipeFrameDoneQ[PIPE_GDC]);
    stopThreadAndInputQ(m_gdcThread, 1, m_gdcQ);
#endif
#ifdef USES_SW_VDIS
    stopThreadAndInputQ(m_previewStreamVDISThread, 1, m_pipeFrameDoneQ[PIPE_VDIS]);
#endif
#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
        stopThreadAndInputQ(m_previewStreamSyncThread, 1, m_pipeFrameDoneQ[PIPE_SYNC]);

        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
            stopThreadAndInputQ(m_previewStreamFusionThread, 1, m_pipeFrameDoneQ[PIPE_FUSION]);
        }
    }
#endif
#ifdef BUFFER_DUMP
    stopThreadAndInputQ(m_dumpThread, 1, m_dumpBufferQ);
#endif

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_captureSelector[i] != NULL) {
            m_captureSelector[i]->release();
        }
    }

    ret = m_transitState(EXYNOS_CAMERA_STATE_FLUSH);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into FLUSH. ret %d", ret);
        goto func_exit;
    }

    /* reset buffer */
    ret = m_bufferSupplier->resetBuffers();
    if (ret != NO_ERROR) {
        CLOGE("Failed to resetBuffers. ret %d", ret);
    }

    /* Clear frame list, because the frame number is initialized in startFrameFactoryThread. */
    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_captureProcessList) failed [%d]", ret);
    }

    ret = m_transitState(EXYNOS_CAMERA_STATE_CONFIGURED);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into CONFIGURED. ret %d", ret);
        goto func_exit;
    }

func_exit:
    CLOGI("Internal restart stream OUT");
    return ret;
}

status_t ExynosCamera::flush()
{
    int retryCount;

    /* flush lock */
    m_flushLock.lock();

    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    /*
     * This flag should be set before stoping all pipes,
     * because other func(like processCaptureRequest) must know call state about flush() entry level
     */

    CLOGD("IN+++");
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FLUSH_START, 0);

#ifdef TIME_LOGGER_STREAM_PERFORMANCE_ENABLE
    TIME_LOGGER_SAVE(m_cameraId);
#endif

    if (m_getState() == EXYNOS_CAMERA_STATE_CONFIGURED) {
        CLOGD("No need to wait & flush");
        goto func_exit;
    }

    if (m_getState() != EXYNOS_CAMERA_STATE_ERROR) {
        /*
         * In the Error state, flush() can not be called by frame work.
         * But, flush() is called internally when Device close is called.
         * So, flush operation is performed, and exynos_camera_state is not changed.
         */
        ret = m_transitState(EXYNOS_CAMERA_STATE_FLUSH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to transitState into FLUSH. ret %d", ret);
            goto func_exit;
        }
    }

    m_captureResultDoneCondition.signal();

    {
        m_configurations->setUseFastenAeStable(false);
    }

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_captureSelector[i] != NULL) {
            m_captureSelector[i]->wakeupQ();
            m_captureSelector[i]->cancelPicture(true);
        }
    }

    /* Wait for finishing pre-processing threads and Release pre-processing frame queues */
    stopThreadAndInputQ(m_mainPreviewThread, 1, m_shotDoneQ);
    m_mainCaptureThread->requestExitAndWait();
#ifdef USE_DUAL_CAMERA
    if (m_dualFrameFactoryStartThread != NULL)
        m_dualFrameFactoryStartThread->requestExitAndWait();
    stopThreadAndInputQ(m_slaveMainThread, 1, m_slaveShotDoneQ);
    stopThreadAndInputQ(m_dualStandbyThread, 1, m_dualStandbyTriggerQ);
    stopThreadAndInputQ(m_selectDualSlaveBayerThread, 1, m_selectDualSlaveBayerQ);
#endif
    stopThreadAndInputQ(m_selectBayerThread, 1, m_selectBayerQ);
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FRAME_CREATE_THREAD_STOP_END, 0);

    /* Stop pipeline */
    for (int i = FRAME_FACTORY_TYPE_MAX - 1; i >= 0; i--) {
        if (m_frameFactory[i] != NULL) {
            frameFactory = m_frameFactory[i];

            for (int k = i - 1; k >= 0; k--) {
               if (frameFactory == m_frameFactory[k]) {
                   CLOGD("m_frameFactory index(%d) and index(%d) are same instance,"
                        " set index(%d) = NULL", i, k, k);
                   m_frameFactory[k] = NULL;
               }
            }

            TIME_LOGGER_UPDATE(m_cameraId, i, 0, CUMULATIVE_CNT, FACTORY_STREAM_STOP_START, 0);
            ret = m_stopFrameFactory(m_frameFactory[i]);
            TIME_LOGGER_UPDATE(m_cameraId, i, 0, CUMULATIVE_CNT, FACTORY_STREAM_STOP_END, 0);
            if (ret < 0)
                CLOGE("m_frameFactory[%d] stopPipes fail", i);

            CLOGD("m_frameFactory[%d] stopPipes", i);
        }
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, LIBRARY_DEINIT_END, 0);

    /* Wait for finishing post-processing thread */
    stopThreadAndInputQ(m_previewStreamBayerThread, 1, m_pipeFrameDoneQ[PIPE_FLITE]);
    stopThreadAndInputQ(m_previewStream3AAThread, 1, m_pipeFrameDoneQ[PIPE_3AA]);
#ifdef SUPPORT_GMV
    stopThreadAndInputQ(m_previewStreamGMVThread, 1, m_pipeFrameDoneQ[PIPE_GMV]);
#endif
    stopThreadAndInputQ(m_previewStreamISPThread, 1, m_pipeFrameDoneQ[PIPE_ISP]);
    stopThreadAndInputQ(m_previewStreamMCSCThread, 1, m_pipeFrameDoneQ[PIPE_MCSC]);
    stopThreadAndInputQ(m_previewStreamVRAThread, 1, m_pipeFrameDoneQ[PIPE_VRA]);
#ifdef SUPPORT_HFD
    stopThreadAndInputQ(m_previewStreamHFDThread, 1, m_pipeFrameDoneQ[PIPE_HFD]);
#endif
    stopThreadAndInputQ(m_bayerStreamThread, 1, m_bayerStreamQ);
    stopThreadAndInputQ(m_captureThread, 2, m_captureQ, m_reprocessingDoneQ);
    stopThreadAndInputQ(m_thumbnailCbThread, 2, m_thumbnailCbQ, m_thumbnailPostCbQ);
    stopThreadAndInputQ(m_captureStreamThread, 1, m_yuvCaptureDoneQ);
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
    stopThreadAndInputQ(m_reverseProcessingBayerThread, 1, m_reverseProcessingBayerQ);
#endif
#ifdef SUPPORT_HW_GDC
    stopThreadAndInputQ(m_previewStreamGDCThread, 1, m_pipeFrameDoneQ[PIPE_GDC]);
    stopThreadAndInputQ(m_gdcThread, 1, m_gdcQ);
#endif
#ifdef USES_SW_VDIS
    if (m_exCameraSolutionSWVdis != NULL) {
        m_exCameraSolutionSWVdis->flush();
    }
    stopThreadAndInputQ(m_previewStreamVDISThread, 1, m_pipeFrameDoneQ[PIPE_VDIS]);
#endif
#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
        stopThreadAndInputQ(m_previewStreamSyncThread, 1, m_pipeFrameDoneQ[PIPE_SYNC]);

        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
            stopThreadAndInputQ(m_previewStreamFusionThread, 1, m_pipeFrameDoneQ[PIPE_FUSION]);
        }

        m_latestRequestListLock.lock();
        m_latestRequestList.clear();
        m_essentialRequestList.clear();
        m_latestRequestListLock.unlock();
    }
#endif
#ifdef BUFFER_DUMP
    stopThreadAndInputQ(m_dumpThread, 1, m_dumpBufferQ);
#endif

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, STREAM_THREAD_STOP_END, 0);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_captureSelector[i] != NULL) {
            m_captureSelector[i]->release();
        }
    }
    {
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, REQUEST_FLUSH_START, 0);
        m_requestMgr->flush();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, REQUEST_FLUSH_END, 0);
    }
    ret = m_clearRequestList(&m_requestPreviewWaitingList, &m_requestPreviewWaitingLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearRequestList(&m_requestCaptureWaitingList, &m_requestCaptureWaitingLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_RESET_START, 0);
    ret = m_bufferSupplier->resetBuffers();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, BUFFER_RESET_END, 0);
    if (ret != NO_ERROR) {
        CLOGE("Failed to resetBuffers. ret %d", ret);
    }

    /* Clear frame list, because the frame number is initialized in startFrameFactoryThread. */
    ret = m_clearList(&m_processList, &m_processLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_captureProcessList) failed [%d]", ret);
    }

    /*
     * do unlock/lock of flushLock to give a chance to processCaptureRequest()
     * to flush remained requests and wait maximum 33ms for finishing current
     * processCaptureRequest().
     */
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, WAIT_PROCESS_CAPTURE_REQUEST_START, 0);
    m_flushLock.unlock();
    retryCount = 33; /* 33ms */
    while (m_flushLockWait && retryCount > 0) {
        CLOGW("wait for done current processCaptureRequest(%d)!!", retryCount);
        retryCount--;
        usleep(1000);
    }
    m_flushLock.lock();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, WAIT_PROCESS_CAPTURE_REQUEST_END, 0);

    if (m_getState() != EXYNOS_CAMERA_STATE_ERROR) {
        /*
         * In the Error state, flush() can not be called by frame work.
         * But, flush() is called internally when Device close is called.
         * So, flush operation is performed, and exynos_camera_state is not changed.
         */
        ret = m_transitState(EXYNOS_CAMERA_STATE_CONFIGURED);
        if (ret != NO_ERROR) {
            CLOGE("Failed to transitState into CONFIGURED. ret %d", ret);
            goto func_exit;
        }
    }

func_exit:
    setPreviewProperty(false);

    /* flush unlock */
    m_flushLock.unlock();

    CLOGD(" : OUT---");
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FLUSH_END, 0);
    return ret;
}

status_t ExynosCamera::m_fastenAeStable(ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer fastenAeBuffer[NUM_FASTAESTABLE_BUFFERS];
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;

    if (factory == NULL) {
        CLOGE("frame factory is NULL!!");
        return BAD_VALUE;
    }

    int cameraId = factory->getCameraId();

    bufTag.pipeId[0] = PIPE_3AA;
    bufTag.managerType = BUFFER_MANAGER_FASTEN_AE_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("SKIP_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create SKIP_BUF. ret %d", ret);
        return ret;
    }

    int bufferCount = m_exynosconfig->current->bufInfo.num_fastaestable_buffer - 4;

    bufConfig.planeCount = 2;
    bufConfig.size[0] = 32 * 64 * 2;
    bufConfig.reqBufCount = bufferCount;
    bufConfig.allowedMaxBufCount = bufferCount;
    bufConfig.batchSize = 1; /* 1, fixed batch size */
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
    bufConfig.reservedMemoryCount = 0;

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc SKIP_BUF. ret %d.", ret);
        goto done;
    }

    for (int i = 0; i < bufferCount; i++) {
        ret = m_bufferSupplier->getBuffer(bufTag, &fastenAeBuffer[i]);
        if (ret != NO_ERROR) {
            CLOGE("[B%d]Failed to getBuffer", fastenAeBuffer[i].index);
            goto done;
        }
    }

    m_parameters[cameraId]->setFastenAeStableOn(true);

    ret = factory->fastenAeStable(bufferCount, fastenAeBuffer);
    if (ret != NO_ERROR) {
        ret = INVALID_OPERATION;
        CLOGE("fastenAeStable fail(%d)", ret);
    }

    m_parameters[cameraId]->setFastenAeStableOn(false);

    m_checkFirstFrameLux = true;
done:
     m_bufferSupplier->deinit(bufTag);

    return ret;
}

bool ExynosCamera::m_previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame, int pipeId)
{
    status_t ret = 0;
    ExynosCameraFrameFactory *factory = NULL;

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        return true;
    } else if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
        CLOGD("[F%d P%d]Flush is in progress.",
                newFrame->getFrameCount(),
                pipeId);
        return false;
    }

    CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "pipeId(%d)", pipeId);
    TIME_LOGGER_UPDATE(m_cameraId, newFrame->getFrameCount(), pipeId, CUMULATIVE_CNT, PREVIEW_STREAM_THREAD, 0);

    if (pipeId == PIPE_3AA) {
        m_previewDurationTimer.stop();
        m_previewDurationTime = (int)m_previewDurationTimer.durationMsecs();
        CLOGV("frame duration time(%d)", m_previewDurationTime);
        m_previewDurationTimer.start();
    }

    if (newFrame->getFrameType() == FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        || newFrame->getFrameType() == FRAME_TYPE_INTERNAL_SLAVE
        || newFrame->getFrameType() == FRAME_TYPE_TRANSITION
        || newFrame->getFrameType() == FRAME_TYPE_TRANSITION_SLAVE
#endif
        ) {
#ifdef USE_DUAL_CAMERA
        if (newFrame->isSlaveFrame()) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        } else
#endif
        {
            if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == true) {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
            } else {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            }
        }

        /* Handle the internal frame for each pipe */
        ret = m_handleInternalFrame(newFrame, pipeId, factory);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    } else if (newFrame->getFrameType() == FRAME_TYPE_VISION) {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
        ret = m_handleVisionFrame(newFrame, pipeId, factory);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    } else {
        /* TODO: M2M path is also handled by this */
#ifdef USE_DUAL_CAMERA
        if (newFrame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE
            || newFrame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE) {
            if (pipeId == PIPE_FUSION
                    ) {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            } else
            {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            }
        } else
#endif
        {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        }

        ret = m_handlePreviewFrame(newFrame, pipeId, factory);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    }

    return true;
}

status_t ExynosCamera::m_checkMultiCaptureMode(ExynosCameraRequestSP_sprt_t request)
{
#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::generate request frame request(%d)", request->getKey());
#endif

    {
        m_currentMultiCaptureMode = MULTI_CAPTURE_MODE_NONE;
    }

    m_checkMultiCaptureMode_vendor_update(request);

    CLOGV("- OUT - (F:%d)", request->getKey());

    return NO_ERROR;
}

status_t ExynosCamera::m_createCaptureFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    FrameFactoryList previewFactoryAddrList;
    FrameFactoryList captureFactoryAddrList;
    ExynosCameraFrameFactory *factory = NULL;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;
    frame_type_t frameType = FRAME_TYPE_PREVIEW;
    bool jpegRequest = false;
    bool stallRequest = false;

#ifdef USE_DUAL_CAMERA
    ExynosCameraFrameFactory *subFactory = NULL;
    frame_type_t subFrameType = FRAME_TYPE_PREVIEW;
#endif

    if (m_getSizeFromRequestList(&m_requestCaptureWaitingList, &m_requestCaptureWaitingLock) == 0) {
        return ret;
    }

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS::[R%d] generate request frame", request->getKey());
#endif

    m_requestCaptureWaitingLock.lock();
    r = m_requestCaptureWaitingList.begin();
    request = *r;
    m_requestCaptureWaitingList.erase(r);
    m_requestCaptureWaitingLock.unlock();

    previewFactoryAddrList.clear();
    captureFactoryAddrList.clear();
    request->getFactoryAddrList(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, &previewFactoryAddrList);
    request->getFactoryAddrList(FRAME_FACTORY_TYPE_REPROCESSING, &captureFactoryAddrList);

    CLOGD("[R%d F%d] Create CaptureFrame of request", request->getKey(), request->getFrameCount());

#ifdef DEBUG_FUSION_CAPTURE_DUMP
    captureNum++;
#endif

    if (previewFactoryAddrList.empty() == true) {
        ExynosCameraRequestSP_sprt_t serviceRequest = NULL;
        m_popServiceRequest(serviceRequest);
        serviceRequest = NULL;
    }

    /* Update the entire shot_ext structure */
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("[R%d] Get service shot is failed", request->getKey());
    } else {
        memcpy(m_currentCaptureShot, service_shot_ext, sizeof(struct camera2_shot_ext));
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        bool flagFinishFactoryStart = false;
        if (m_getState() == EXYNOS_CAMERA_STATE_RUN)
            flagFinishFactoryStart = true;

        m_checkDualOperationMode(request, false, true, flagFinishFactoryStart);
        if (ret != NO_ERROR) {
            CLOGE("m_checkDualOperationMode fail! ret(%d)", ret);
        }
    }
#endif

    /*
     * HACK: Checked the request combination!!
     * If this device hasn't three mcsc ports, it can't support
     * YUV_STALL and JPEG request concurrently.
     */
    if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5) {
        for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
            int id = request->getStreamIdwithBufferIdx(i);
            switch (id % HAL_STREAM_ID_MAX) {
                case HAL_STREAM_ID_JPEG:
                    jpegRequest = true;
                    break;
                case HAL_STREAM_ID_CALLBACK_STALL:
                case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
                    stallRequest = true;
                    break;
                default:
                    break;
            }
        }

        if (jpegRequest == true && stallRequest == true) {
            CLOG_ASSERT("this device can't support YUV_STALL and JPEG at the same time");
        }
    }

    /* Call the frame create handler fro each frame factory */
    /* Frame createion handler calling sequence is ordered in accordance with
       its frame factory type, because there are dependencies between frame
       processing routine.
     */

    for (factorylistIter = captureFactoryAddrList.begin(); factorylistIter != captureFactoryAddrList.end(); ) {
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

        /* skip case */
        if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5) {
                if (frameType == FRAME_TYPE_REPROCESSING && jpegRequest == true)
                        continue;
                if (frameType == FRAME_TYPE_JPEG_REPROCESSING && stallRequest == true)
                        continue;
        }

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();
            enum DUAL_OPERATION_MODE dualOperationModeReprocessing =
                m_configurations->getDualOperationModeReprocessing();

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
                if (dualOperationModeReprocessing == DUAL_OPERATION_MODE_SYNC) {
                    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
                        frameType = FRAME_TYPE_REPROCESSING_DUAL_MASTER;
                        frameCreateHandler = factory->getFrameCreateHandler();
                        (this->*frameCreateHandler)(request, factory, frameType);

                        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
                        frameType = FRAME_TYPE_REPROCESSING_DUAL_SLAVE;
                    } else {
                        subFrameType = FRAME_TYPE_REPROCESSING_DUAL_SLAVE;
                        subFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
                        frameCreateHandler = subFactory->getFrameCreateHandler();
                        (this->*frameCreateHandler)(request, subFactory, subFrameType);
                        frameType = FRAME_TYPE_REPROCESSING_DUAL_MASTER;
                    }
                } else if (dualOperationModeReprocessing == DUAL_OPERATION_MODE_SLAVE) {
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

    previewFactoryAddrList.clear();
    captureFactoryAddrList.clear();

    return ret;
}

void ExynosCamera::m_updateExposureTime(struct camera2_shot_ext *shot_ext)
{
    m_vendorUpdateExposureTime(shot_ext);
}

status_t ExynosCamera::m_createInternalFrameFunc(ExynosCameraRequestSP_sprt_t request,
                                                 __unused enum Request_Sync_Type syncType,
                                                 __unused frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    bool isNeedBayer = (request != NULL) ? true : false;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeId = -1;
    int dstPos = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    frame_type_t internalframeType = FRAME_TYPE_INTERNAL;
    frame_handle_components_t components;
    ExynosRect bayerCropRegion = {0, };

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        /* This case is slave internal frame on SYNC mode */
        internalframeType = FRAME_TYPE_INTERNAL_SLAVE;
    } else if (frameType == FRAME_TYPE_INTERNAL_SLAVE) {
        internalframeType = FRAME_TYPE_INTERNAL_SLAVE;
    } else if (frameType == FRAME_TYPE_TRANSITION) {
        internalframeType = FRAME_TYPE_INTERNAL;
    } else if (frameType == FRAME_TYPE_TRANSITION_SLAVE) {
        internalframeType = FRAME_TYPE_INTERNAL_SLAVE;
    }
#endif

    getFrameHandleComponents(internalframeType, &components);

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == true) {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    } else {
        factory = components.previewFactory;
    }

    bool requestVC0 = (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);

    /* Initialize the request flags in framefactory */
    factory->setRequest(PIPE_VC0, requestVC0);
    factory->setRequest(PIPE_3AC, false);
    factory->setRequest(PIPE_3AP, false);
    factory->setRequest(PIPE_ISP, false);
    factory->setRequest(PIPE_ISPP, false);
    factory->setRequest(PIPE_ISPC, false);
    factory->setRequest(PIPE_MCSC, false);
    factory->setRequest(PIPE_MCSC0, false);
    factory->setRequest(PIPE_MCSC1, false);
    factory->setRequest(PIPE_MCSC2, false);
    factory->setRequest(PIPE_GDC, false);
    factory->setRequest(PIPE_MCSC5, false);
    factory->setRequest(PIPE_VRA, false);
    factory->setRequest(PIPE_HFD, false);
#ifdef SUPPORT_DEPTH_MAP
    factory->setRequest(PIPE_VC1, m_flagUseInternalDepthMap);
#endif

    if (m_needDynamicBayerCount > 0) {
        isNeedBayer = true;
        m_needDynamicBayerCount--;
    }

#ifdef USE_DUAL_CAMERA
    factory->setRequest(PIPE_SYNC, false);
    factory->setRequest(PIPE_FUSION, false);

    if (!(frameType == FRAME_TYPE_TRANSITION ||
                frameType == FRAME_TYPE_TRANSITION_SLAVE))
#endif
    {
        switch (m_parameters[m_cameraId]->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            factory->setRequest(PIPE_VC0, true);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
            factory->setRequest(PIPE_3AC, true);
            break;
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            isNeedBayer = (isNeedBayer || requestVC0);
            factory->setRequest(PIPE_VC0, isNeedBayer);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            factory->setRequest(PIPE_3AC, isNeedBayer);
#ifdef USE_DUAL_CAMERA
            /* master : increase dynamic bayer count */
            if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) &&
                    (frameType == FRAME_TYPE_INTERNAL) && isNeedBayer) {
                android_atomic_inc(&m_needSlaveDynamicBayerCount);
            }
#endif
            break;
        default:
            break;
        }
    }

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == true){
        factory->setRequest(PIPE_VC0, false);
    }

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()) {
        factory->setRequest(PIPE_VC0, true);
    }
#endif

    /* Generate the internal frame */
    if (request != NULL) {
        /* Set framecount into request */
        m_frameCountLock.lock();
        if (request->getFrameCount() == 0) {
#ifdef USE_DUAL_CAMERA
            if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                m_internalFrameCount--;
            }
#endif

            m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
        }
        m_frameCountLock.unlock();

        ret = m_generateInternalFrame(factory, &m_processList, &m_processLock, newFrame, internalframeType, request);
#ifdef USE_DUAL_CAMERA
    } else if (frameType == FRAME_TYPE_TRANSITION || frameType == FRAME_TYPE_TRANSITION_SLAVE) {
        ret = m_generateTransitionFrame(factory, &m_processList, &m_processLock, newFrame, frameType, request);
#endif

    } else {
        ret = m_generateInternalFrame(factory, &m_processList, &m_processLock, newFrame, internalframeType);
    }

    if (ret != NO_ERROR) {
        CLOGE("m_generateFrame failed");
        return ret;
    } else if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return INVALID_OPERATION;
    }

    if (request != NULL) {
        for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
            int id = request->getStreamIdwithBufferIdx(i);
            switch (id % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
                newFrame->setStreamRequested(STREAM_TYPE_CAPTURE, true);
                break;
            case HAL_STREAM_ID_RAW:
                newFrame->setStreamRequested(STREAM_TYPE_RAW, true);
                break;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_VISION:
            default:
                break;
            }
        }

        if (request->getNumOfInputBuffer() < 1) {
            newFrame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                               ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED);
        }
    }

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:[F%d]generate internal frame.", newFrame->getFrameCount());
#else
    CLOGV("[F%d]generate internal frame.", newFrame->getFrameCount());
#endif

    if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
        CLOGD("[F%d]Flush is in progress.", newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        return ret;
    }

    Mutex::Autolock l(m_currentPreviewShotLock);

    /* Update the metadata with m_currentPreviewShot into frame */
    if (m_currentPreviewShot == NULL) {
        CLOGE("m_currentPreviewShot is NULL");
        return INVALID_OPERATION;
    }

    if (newFrame->getStreamRequested(STREAM_TYPE_CAPTURE) || newFrame->getStreamRequested(STREAM_TYPE_RAW))
        m_updateExposureTime(m_currentPreviewShot);

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, USER_DATA, CREATE_INTERNAL_FRAME, newFrame->getFrameCount());

    /* BcropRegion is calculated in m_generateFrame    */
    /* so that updatePreviewStatRoi should be set here.*/
    components.parameters->getHwBayerCropRegion(
                    &bayerCropRegion.w,
                    &bayerCropRegion.h,
                    &bayerCropRegion.x,
                    &bayerCropRegion.y);
    components.parameters->updatePreviewStatRoi(m_currentPreviewShot, &bayerCropRegion);

    ret = newFrame->setMetaData(m_currentPreviewShot);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaData with m_currentPreviewShot. framecount %d ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

    pipeId = m_getBayerPipeId();

    /* Attach VC0 buffer & push frame to VC0 */
    if (newFrame->getRequest(PIPE_VC0) == true) {
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_VC0;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
        buffer.index = -2;
        dstPos = factory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to get Internal Bayer Buffer. ret %d",
                    newFrame->getFrameCount(), ret);
        }

        if (buffer.index < 0) {
            CLOGW("[F%d B%d]Invalid bayer buffer index. Skip to pushFrame",
                    newFrame->getFrameCount(), buffer.index);
            newFrame->setRequest(bufTag.pipeId[0], false);
        } else {
            ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                        pipeId, bufTag.pipeId[0], dstPos);
                newFrame->setRequest(bufTag.pipeId[0], false);
            } else {
                ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                            pipeId, bufTag.pipeId[0], dstPos);
                    newFrame->setRequest(bufTag.pipeId[0], false);
                }
            }
        }
    }

#ifdef SUPPORT_DEPTH_MAP
    if (newFrame->getRequest(PIPE_VC1) == true) {
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_VC1;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
        buffer.index = -2;
        dstPos = factory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to get Internal Depth Buffer. ret %d",
                    newFrame->getFrameCount(), ret);
        }

        CLOGV("[F%d B%d]Use Internal Depth Buffer",
                newFrame->getFrameCount(), buffer.index);

        if (buffer.index < 0) {
            CLOGW("[F%d B%d]Invalid bayer buffer index. Skip to pushFrame",
                    newFrame->getFrameCount(), buffer.index);
            newFrame->setRequest(bufTag.pipeId[0], false);
        } else {
            ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
            if (ret == NO_ERROR) {
                ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                            pipeId, bufTag.pipeId[0], dstPos);
                    newFrame->setRequest(bufTag.pipeId[0], false);
                }
            } else {
                CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                        pipeId, bufTag.pipeId[0], dstPos);
                newFrame->setRequest(bufTag.pipeId[0], false);
            }
        }
    }
#endif

    /* Attach SrcBuffer */
    ret = m_setupEntity(pipeId, newFrame);
    if (ret != NO_ERROR) {
        CLOGW("[F%d]Failed to setupEntity. pipeId %d", newFrame->getFrameCount(), pipeId);
    } else {
        factory->pushFrameToPipe(newFrame, pipeId);
    }

    return ret;
}

status_t ExynosCamera::m_handleBayerBuffer(ExynosCameraFrameSP_sptr_t frame,
                                                 ExynosCameraRequestSP_sprt_t request,
                                                 int pipeId)
{
    status_t ret = NO_ERROR;
    uint32_t bufferDirection = INVALID_BUFFER_DIRECTION;
    ExynosCameraBuffer buffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_COMPLETE;
    int dstPos = 0;
    frame_handle_components_t components;
#ifdef SUPPORT_DEPTH_MAP
    bool releaseDepthBuffer = true;
#endif

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    ExynosCameraFrameFactory *factory = components.previewFactory;
    bool flite3aaM2M = (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    bool flagForceHoldDynamicBayer = false;

    assert(pipeId >= 0);
    assert(pipeId < MAX_PIPE_NUM);

    switch (pipeId) {
        case PIPE_FLITE:
            dstPos = factory->getNodeType(PIPE_VC0);
            bufferDirection = DST_BUFFER_DIRECTION;

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getDstBuffer for PIPE_VC0. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to getDstBufferState for PIPE_VC0",
                    frame->getFrameCount(), bufferDirection);
            }
            break;

        case PIPE_3AA:
            if (components.parameters->getUsePureBayerReprocessing() == true
                ) {
                pipeId = flite3aaM2M ? PIPE_FLITE : PIPE_3AA;
                dstPos = factory->getNodeType(PIPE_VC0);
            } else {
                dstPos = factory->getNodeType(PIPE_3AC);
            }
            bufferDirection = DST_BUFFER_DIRECTION;

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                        frame->getFrameCount(), buffer.index, dstPos, ret);
                goto SKIP_PUTBUFFER;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                        frame->getFrameCount(), buffer.index, dstPos, ret);
                goto SKIP_PUTBUFFER;
            }
            break;

        default:
            CLOGE("[F%d]Invalid bayer handling PipeId %d", frame->getFrameCount(), pipeId);
            return INVALID_OPERATION;
    }

    assert(buffer != NULL);

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("[F%d B%d]Invalid bayer buffer state. pipeId %d",
                frame->getFrameCount(), buffer.index, pipeId);

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer. pipeId %d",
                    frame->getFrameCount(), buffer.index, pipeId);
            /* No operation */
        }

        if (request == NULL) {
            CLOGE("[F%d] request is NULL.", frame->getFrameCount());
            ret = INVALID_OPERATION;
        } else if ((frame->getFrameType() == FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
                    || frame->getFrameType() == FRAME_TYPE_INTERNAL_SLAVE
#endif
                    ) && m_configurations->getMode(CONFIGURATION_DYNAMIC_BAYER_MODE) == false) {
            CLOGW("[F%d B%d]Always reprocessing mode. Skip ERROR_REQUEST callback. pipeId %d",
                    frame->getFrameCount(), buffer.index, pipeId);
        } else if (frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
            == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED) {
            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d]Failed to sendNotifyError. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, ret);
            }
        }

        goto SKIP_PUTBUFFER;
    } else if (buffer.index < 0) {
        CLOGE("[F%d B%d]Invalid bayer buffer. pipeId %d",
                frame->getFrameCount(), buffer.index, pipeId);
        ret = INVALID_OPERATION;
        goto SKIP_PUTBUFFER;
    }

    if (frame->getStreamRequested(STREAM_TYPE_ZSL_OUTPUT)) {
        CLOGV("[F%d B%d]Handle ZSL buffer. FLITE-3AA_%s bayerPipeId %d",
                frame->getFrameCount(), buffer.index,
                flite3aaM2M ? "M2M" : "OTF",
                pipeId);

        if (request == NULL) {
            CLOGE("[F%d]request is NULL.", frame->getFrameCount());
            ret = INVALID_OPERATION;
            goto CHECK_RET_PUTBUFFER;
        }

        ret = m_sendZslStreamResult(request, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to sendZslCaptureResult. bayerPipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            goto CHECK_RET_PUTBUFFER;
        }
    } else if (components.parameters->isReprocessing() == true) {
        switch (components.parameters->getReprocessingBayerMode()) {
            case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
                CLOGV("[F%d B%d]Hold internal bayer buffer for reprocessing in Always On."
                        " FLITE-3AA_%s bayerPipeId %d",
                        frame->getFrameCount(), buffer.index,
                        flite3aaM2M ? "M2M" : "OTF", pipeId);

                frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                ret = components.captureSelector->manageFrameHoldListHAL3(frame);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to manageFrameHoldListHAL3. bufDirection %d bayerPipeId %d ret %d",
                            frame->getFrameCount(), buffer.index,
                            bufferDirection, pipeId, ret);
                }
#ifdef SUPPORT_DEPTH_MAP
                else {
                    releaseDepthBuffer = false;
                }
#endif
                break;

            case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
#ifdef USE_DUAL_CAMERA
                /* check the dual slave frame for dynamic capture */
                flagForceHoldDynamicBayer = (m_configurations->getMode(CONFIGURATION_DUAL_MODE) &&
                                            (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) &&
                                            (frame->getRequest(PIPE_3AC) == true));
#endif
                if (frame->getStreamRequested(STREAM_TYPE_CAPTURE) || frame->getStreamRequested(STREAM_TYPE_RAW)
                    || (m_needDynamicBayerCount > 0) || flagForceHoldDynamicBayer) {
                    CLOGV("[F%d B%d T%d]Hold internal bayer buffer for reprocessing in Dynamic."
                            " FLITE-3AA_%s bayerPipeId %d",
                            frame->getFrameCount(), buffer.index, frame->getFrameType(),
                            flite3aaM2M ? "M2M" : "OTF", pipeId);
#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE)) {
                        /* use normal holding frame API, because there's no release logic in manageFrameHoldListForDynamicBayer() */
                        frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                        ret = components.captureSelector->manageFrameHoldListHAL3(frame);
                    } else
#endif
                    {
                        ret = components.captureSelector->manageFrameHoldListForDynamicBayer(frame);
                    }
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to manageFrameHoldListForDynamicBayer."
                                " bufDirection %d bayerPipeId %d ret %d",
                                frame->getFrameCount(), buffer.index,
                                bufferDirection, pipeId, ret);
                    }
#ifdef SUPPORT_DEPTH_MAP
                    else {
                        releaseDepthBuffer = false;
                    }
#endif
                } else {
                    CLOGV("[F%d B%d]Return internal bayer buffer. FLITE-3AA_%s bayerPipeId %d",
                            frame->getFrameCount(), buffer.index,
                            flite3aaM2M ? "M2M" : "OTF", pipeId);

                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret !=  NO_ERROR) {
                        CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                                frame->getFrameCount(), buffer.index,
                                bufferDirection, pipeId, ret);
                    }
                }
                break;

            default:
                CLOGE("[F%d B%d]ReprocessingMode %d is not valid.",
                        frame->getFrameCount(), buffer.index,
                        components.parameters->getReprocessingBayerMode());

                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret !=  NO_ERROR) {
                    CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                            frame->getFrameCount(), buffer.index,
                            bufferDirection, pipeId, ret);
                }

                ret = INVALID_OPERATION;
                break;
        }
    } else {
        /* No request for bayer image */
        CLOGV("[F%d B%d]Return internal bayer buffer. FLITE-3AA_%s bayerPipeId %d",
                frame->getFrameCount(), buffer.index,
                flite3aaM2M ? "M2M" : "OTF", pipeId);

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret !=  NO_ERROR) {
            CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index,
                    bufferDirection, pipeId, ret);
        }
    }

    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Handling bayer buffer failed."
                " isServiceBayer %d bufDirection %d pipeId %d ret %d",
                frame->getFrameCount(), buffer.index,
                frame->getStreamRequested(STREAM_TYPE_RAW),
                bufferDirection, pipeId, ret);
    }

#ifdef SUPPORT_DEPTH_MAP
    frame->setReleaseDepthBuffer(releaseDepthBuffer);
#endif

    return ret;

CHECK_RET_PUTBUFFER:
    /* Put the bayer buffer if there was an error during processing */
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Handling bayer buffer failed. Bayer buffer will be put."
                " isServiceBayer %d, bufDirection %d pipeId %d ret %d",
                frame->getFrameCount(), buffer.index,
                frame->getStreamRequested(STREAM_TYPE_RAW),
                bufferDirection, pipeId, ret);

        if (buffer.index >= 0) {
            if (m_bufferSupplier->putBuffer(buffer) !=  NO_ERROR) {
                // Do not taint 'ret' to print appropirate error log on next block.
                CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                        frame->getFrameCount(), buffer.index,
                        bufferDirection, bufferDirection, ret);
            }
        }
    }

SKIP_PUTBUFFER:
#ifdef SUPPORT_DEPTH_MAP
    frame->setReleaseDepthBuffer(releaseDepthBuffer);
#endif

    return ret;
}

#ifdef SUPPORT_DEPTH_MAP
status_t ExynosCamera::m_handleDepthBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer depthBuffer;
    uint32_t pipeDepthId = m_getBayerPipeId();
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_COMPLETE;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int dstPos = factory->getNodeType(PIPE_VC1);
    camera3_buffer_status_t streamBufferStae = CAMERA3_BUFFER_STATUS_OK;
    int streamId = HAL_STREAM_ID_DEPTHMAP;

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

    depthBuffer.index = -2;

    ret = frame->getDstBuffer(pipeDepthId, &depthBuffer, dstPos);
    if (ret != NO_ERROR) {
        if (request == NULL) {
            CLOGE("[F%d]Failed to get DepthMap buffer. ret %d",
                    frame->getFrameCount(), ret);
        } else {
            CLOGE("[R%d F%d]Failed to get DepthMap buffer. ret %d",
                    request->getKey(), frame->getFrameCount(), ret);
        }
    }

    ret = frame->getDstBufferState(pipeDepthId, &bufferState, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                frame->getFrameCount(), depthBuffer.index, dstPos, ret);
    }

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        streamBufferStae = CAMERA3_BUFFER_STATUS_ERROR;
        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                depthBuffer.index, frame->getFrameCount(), pipeDepthId);
    }

    if (request != NULL && frame->getStreamRequested(STREAM_TYPE_DEPTH)) {
        request->setStreamBufferStatus(streamId, streamBufferStae);
        frame->setRequest(PIPE_VC1, false);
        frame->setReleaseDepthBuffer(false);

        ret = m_sendDepthStreamResult(request, &depthBuffer, streamId);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to m_sendDepthStreamResult. ret %d",
                    request->getKey(), frame->getFrameCount(), depthBuffer.index, ret);
        }
    }

    if (depthBuffer.index >= 0 && frame->getReleaseDepthBuffer() == true) {
        ret = m_bufferSupplier->putBuffer(depthBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                    frame->getFrameCount(), depthBuffer.index, ret);
        }

        frame->setRequest(PIPE_VC1, false);
    }

    return ret;
}
#endif

status_t ExynosCamera::processCaptureRequest(camera3_capture_request *request)
{
    status_t ret = NO_ERROR;
    ExynosCamera::exynos_camera_state_t entryState = m_getState();
    ExynosCameraRequestSP_sprt_t req = NULL;
    ExynosCameraStream *streamInfo = NULL;
    uint32_t timeOutNs = 60 * 1000000; /* timeout default value is 60ms based on 15fps */
    uint32_t minFps = 0, maxFps = 0;
    int initialRequestCount = 0;
    uint32_t requiredRequestCount = -1;
    enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
    camera3_stream_t *stream = NULL;
    EXYNOS_STREAM::STATE registerBuffer = EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED;
    FrameFactoryList captureFactoryAddrList;

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#else
    CLOGV("Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#endif

#ifdef DEBUG_IRIS_LEAK
#endif

    /* 0. Check the validation of ExynosCamera */
    if (m_getState() == EXYNOS_CAMERA_STATE_ERROR) {
        CLOGE("ExynosCamera state is ERROR! Exynos Camera must be terminated by frame work.");
        ret = NO_INIT;
        goto req_err;
    }

    /* 1. Check the validation of request */
    if (request == NULL) {
        CLOGE("NULL request!");
        ret = BAD_VALUE;
        goto req_err;
    }

#ifdef USE_DEBUG_PROPERTY
    // For Fence FD debugging
    if (request->input_buffer != NULL) {
        const camera3_stream_buffer* pStreamBuf = request->input_buffer;
        CLOG_PERFRAME(BUF, m_cameraId, m_name, nullptr, (void *)(pStreamBuf),
                request->frame_number, "[INPUT]");
    }

    for(uint32_t i = 0; i < request->num_output_buffers; i++) {
        const camera3_stream_buffer* pStreamBuf = &(request->output_buffers[i]);
        CLOG_PERFRAME(BUF, m_cameraId, m_name, nullptr, (void *)(pStreamBuf),
                request->frame_number, "[OUTPUT%d]", i);
    }
#endif

    /* m_streamManager->dumpCurrentStreamList(); */

    /* 2. Check NULL for service metadata */
    if ((request->settings == NULL) && (m_requestMgr->isPrevRequest())) {
        CLOGE("Request%d: NULL and no prev request!!", request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 3. Check the registeration of input buffer on stream */
    if (request->input_buffer != NULL){
        stream = request->input_buffer->stream;
        streamInfo = static_cast<ExynosCameraStream*>(stream->priv);
        streamInfo->getRegisterBuffer(&registerBuffer);

        if (registerBuffer != EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED) {
            CLOGE("Request %d: Input buffer not from input stream!", request->frame_number);
            CLOGE("Bad Request %p, type %d format %x",
                    request->input_buffer->stream,
                    request->input_buffer->stream->stream_type,
                    request->input_buffer->stream->format);
            ret = BAD_VALUE;
            goto req_err;
        }
    }

    /* 4. Check the output buffer count */
    if ((request->num_output_buffers < 1) || (request->output_buffers == NULL)) {
        CLOGE("Request %d: No output buffers provided!", request->frame_number);
        ret = BAD_VALUE;
        goto req_err;
    }

    /* 5. Store request settings
     * Caution : All information must be copied into internal data structure
     * before receiving another request from service
     */
    ret = m_pushServiceRequest(request, req);

    /* check restart condition */
    ret = m_checkRestartStream(req);
    if (ret != NO_ERROR) {
        CLOGE("m_checkRestartStream failed [%d]", ret);
        goto req_err;
    }

    CLOGV("[R%d F%d] Push request in ServiceList. ServiceListSize %d",
            req->getKey(), req->getFrameCount(), m_requestMgr->getServiceRequestCount());

    /* 6. Push request in RunningList */
    ret = m_pushRunningRequest(req);
    if (ret != NO_ERROR) {
        CLOGE("m_pushRunningRequest failed [%d]", ret);
        ret = INVALID_OPERATION;
        goto req_err;
    } else {
        m_flushLockWait = true;
        {
            /* flush lock */
            Mutex::Autolock lock(m_flushLock);

            /* thie request will be flushed during flush() */
            if (entryState == EXYNOS_CAMERA_STATE_FLUSH ||
                    m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGE("Request key:%d, out:%d can't accepted during flush()!! forcely Flush() again!(%d,%d)",
                        request->frame_number, request->num_output_buffers,
                        entryState, m_getState());
                m_requestMgr->flush();
                m_flushLockWait = false;
                goto req_err;
            }
        }
        m_flushLockWait = false;
    }

    /* 7. Get FactoryAddr */
    ret = m_setFactoryAddr(req);
    captureFactoryAddrList.clear();
    req->getFactoryAddrList(FRAME_FACTORY_TYPE_REPROCESSING, &captureFactoryAddrList);

    m_checkMultiCaptureMode(req);

    /* 8. Push request in m_requestPreviewWaitingList and m_requestCaptureWaitingList */
    m_requestPreviewWaitingLock.lock();
    m_requestPreviewWaitingList.push_back(req);
    m_requestPreviewWaitingLock.unlock();

    if (captureFactoryAddrList.empty() == false) {
        CLOGD("[R%d F%d] Push capture request in m_requestCaptureWaitingList.",
                req->getKey(), req->getFrameCount());
        m_requestCaptureWaitingLock.lock();
        m_requestCaptureWaitingList.push_back(req);
        if (m_mainCaptureThread != NULL && m_mainCaptureThread->isRunning() == false) {
            m_mainCaptureThread->run(PRIORITY_URGENT_DISPLAY);
        }
        m_requestCaptureWaitingLock.unlock();
    }

    /* 9. Calculate the timeout value for processing request based on actual fps setting */
    m_configurations->getPreviewFpsRange(&minFps, &maxFps);
    timeOutNs = (1000 / ((minFps == 0) ? 15 : minFps)) * 1000000;

    /* 10. Process initial requests for preparing the stream */
    if (m_getState() == EXYNOS_CAMERA_STATE_CONFIGURED) {
        ret = m_transitState(EXYNOS_CAMERA_STATE_START);
        if (ret != NO_ERROR) {
            CLOGE("[R%d]Failed to transitState into START. ret %d", request->frame_number, ret);
            goto req_err;
        }

#ifdef TIME_LOGGER_STREAM_PERFORMANCE_ENABLE
        TIME_LOGGER_INIT(m_cameraId);
#endif

        CLOGD("first frame: [R%d] Start FrameFactory. state %d ", request->frame_number, m_getState());
        setPreviewProperty(true);

        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
#ifdef USE_DUAL_CAMERA
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
        ) {
            processCaptureRequest_vendor_initDualSolutionZoom(request, ret);
        } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
            processCaptureRequest_vendor_initDualSolutionPortrait(request, ret);
        }

        m_configurations->updateMetaParameter(req->getMetaParameters());

#ifdef TIME_LOGGER_STREAM_PERFORMANCE_ENABLE
        TIME_LOGGER_INIT(m_cameraId);
#endif
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            m_checkDualOperationMode(req, true, false, false);
            if (ret != NO_ERROR) {
                CLOGE("m_checkDualOperationMode fail! ret(%d)", ret);
            }
        }
#endif

        m_firstRequestFrameNumber = request->frame_number;

        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SET_BUFFER_THREAD_JOIN_START, 0);
        m_setBuffersThread->join();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SET_BUFFER_THREAD_JOIN_END, 0);

        if (m_captureStreamExist) {
            m_startPictureBufferThread->run(PRIORITY_DEFAULT);
        }

        m_framefactoryCreateThread->join();
        if (m_framefactoryCreateResult != NO_ERROR) {
            CLOGE("Failed to create framefactory");
            m_transitState(EXYNOS_CAMERA_STATE_ERROR);
            ret = NO_INIT;
            goto req_err;
        }

        {
            /* for FAST AE Stable */
#ifdef USE_DUAL_CAMERA
            if (m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                /* for FAST AE Stable */
                if (m_parameters[m_cameraIds[1]]->checkFastenAeStableEnable() == true) {
                    ret = m_fastenAeStable(m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]);
                    if (ret != NO_ERROR) {
                        CLOGE("m_fastenAeStable() failed");
                        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
                        ret = NO_INIT;
                        goto req_err;
                    }

                    m_configurations->setUseFastenAeStable(false);
                }
            } else
#endif
            {
                /* for FAST AE Stable */
                if (m_parameters[m_cameraId]->checkFastenAeStableEnable() == true) {
                    ret = m_fastenAeStable(m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]);
                    if (ret != NO_ERROR) {
                        CLOGE("m_fastenAeStable() failed");
                        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
                        ret = NO_INIT;
                        goto req_err;
                    }

                    m_configurations->setUseFastenAeStable(false);
                }
            }
        }

        m_frameFactoryStartThread->run();
    }
#ifdef USE_DUAL_CAMERA
    else if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        bool flagFinishFactoryStart = false;
        if (m_getState() == EXYNOS_CAMERA_STATE_RUN)
            flagFinishFactoryStart = true;

        m_checkDualOperationMode(req, false, false, flagFinishFactoryStart);
        if (ret != NO_ERROR) {
            CLOGE("m_checkDualOperationMode fail! ret(%d)", ret);
        }
    }
#endif

    TIME_LOGGER_UPDATE(m_cameraId, request->frame_number, 0, INTERVAL, PROCESS_CAPTURE_REQUEST, 0);

    /* Adjust Initial Request Count
     * #sensor control delay > 0
     * #sensor control delay frames will be generated as internal frame.
     * => Initially required request count == Prepare frame count - #sensor control delay
     *
     * #sensor control delay == 0
     * Originally required prepare frame count + 1(HAL request margin) is required.
     * => Initially requred request count == Prepare frame count - SENSOR_REQUEST_DELAY + 1
     */
    if (m_parameters[m_cameraId]->getSensorControlDelay() > 0) {
        initialRequestCount = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    } else {
        initialRequestCount = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA] + 1;
    }

    if (m_parameters[m_cameraId]->useServiceBatchMode() == true) {
        requiredRequestCount = 1;
    } else {
        requiredRequestCount = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
        initialRequestCount *= m_parameters[m_cameraId]->getBatchSize(controlPipeId);
    }

    if (initialRequestCount < 1 || requiredRequestCount < 1) {
        CLOGW("Invalid initialRequestCount %d requiredRequestCount %dprepare %d sensorControlDelay %d/%d",
                initialRequestCount, requiredRequestCount,
                m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA],
                m_parameters[m_cameraId]->getSensorControlDelay(),
                SENSOR_REQUEST_DELAY);
    }

    TIME_LOGGER_UPDATE(m_cameraId, request->frame_number, 0, DURATION, BLOCK_PROCESS_CAPTURE_REQUEST, true);

    CLOG_PERFORMANCE(FPS, m_cameraId, 0, INTERVAL, SERVICE_REQUEST, 0, req->getKey(), nullptr);
    CLOG_PERFRAME(PATH, m_cameraId, m_name, nullptr, nullptr, request->frame_number, "out(%d), requests(%d, %d), waiting(%lu/%lu), state(%d)",
            request->num_output_buffers,
            m_requestMgr->getServiceRequestCount(),
            m_requestMgr->getRunningRequestCount(),
            (unsigned long)m_requestPreviewWaitingList.size(),
            (unsigned long)m_requestCaptureWaitingList.size(),
            m_getState());

    while ((int)(request->frame_number - m_firstRequestFrameNumber) > initialRequestCount
            && m_requestMgr->getServiceRequestCount() > requiredRequestCount
            && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        status_t waitRet = NO_ERROR;
        m_captureResultDoneLock.lock();
        waitRet = m_captureResultDoneCondition.waitRelative(m_captureResultDoneLock, timeOutNs);
        if (waitRet == TIMED_OUT)
            CLOGV("Time out (m_processList:%zu / totalRequestCnt:%d)",
                    m_processList.size(), m_requestMgr->getAllRequestCount());
        m_captureResultDoneLock.unlock();
    }

    TIME_LOGGER_UPDATE(m_cameraId, request->frame_number, 0, DURATION, BLOCK_PROCESS_CAPTURE_REQUEST, false);

    captureFactoryAddrList.clear();

req_err:
    return ret;
}

status_t ExynosCamera::m_checkStreamBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraStream *stream,
                                            ExynosCameraBuffer *buffer, ExynosCameraRequestSP_sprt_t request,
                                            ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    entity_buffer_state_t bufferState;
    int streamId = -1;
    int pipeId = -1;
    int parentPipeId = -1;

    buffer->index = -2;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        ret = BAD_VALUE;
        goto func_exit;
    }

    if (stream == NULL) {
        CLOGE("stream is NULL");
        ret = BAD_VALUE;
        goto func_exit;
    }

    stream->getID(&streamId);

    parentPipeId = request->getParentStreamPipeId(streamId);
    pipeId = request->getStreamPipeId(streamId);

    if (parentPipeId != -1 && pipeId != -1) {
        ret = frame->getDstBuffer(parentPipeId, buffer, factory->getNodeType(pipeId));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d] getDstBuffer fail. pipeId (%d) ret(%d)",
                request->getKey(), frame->getFrameCount(), pipeId, ret);
            goto func_exit;
        }

        ret = frame->getDstBufferState(parentPipeId, &bufferState, factory->getNodeType(pipeId));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d] getDstBufferState fail. pipeId (%d) ret(%d)",
                request->getKey(), frame->getFrameCount(), pipeId, ret);
            goto func_exit;
        }
    }

    if ((frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
            == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED)
        && (((buffer->index) < 0) || (bufferState == ENTITY_BUFFER_STATE_ERROR))) {
        CLOGV("[R%d F%d] Pending resuest", request->getKey(), frame->getFrameCount());
        ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                frame->getFrameCount(),
                buffer->index,
                ret);
            goto func_exit;
        }
    }

func_exit:
    return ret;
}

bool ExynosCamera::m_isSkipBurstCaptureBuffer(__unused frame_type_t frameType)
{
    int isSkipBuffer = false;
    int ratio = 0;
    int runtime_fps = 30;

    if (m_previewDurationTime != 0) {
        runtime_fps = (int)(1000 / m_previewDurationTime);
    }

    int burstshotTargetFps = m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET);

    { /* available up to 15fps */
        int calValue = (int)(burstshotTargetFps / 2);

        if (runtime_fps <= burstshotTargetFps + calValue) {
            ratio = 0;
        } else {
            ratio = (int)((runtime_fps + calValue) / burstshotTargetFps);
        }

        m_burstFps_history[3] = m_burstFps_history[2];
        m_burstFps_history[2] = m_burstFps_history[1];
        m_burstFps_history[1] = ratio;

        if ((m_burstFps_history[0] == -1)
            || (m_burstFps_history[1] == m_burstFps_history[2] && m_burstFps_history[1] == m_burstFps_history[3])) {
            m_burstFps_history[0] = m_burstFps_history[1];
        }

        if (ratio == 0 || m_burstFps_history[0] == 0) {
            m_captureResultToggle = 0;
        } else {
            ratio = m_burstFps_history[0];
            m_captureResultToggle = (m_captureResultToggle + 1) % ratio;
        }

        CLOGV("m_captureResultToggle(%d) m_previewDurationTime(%d) ratio(%d), targetFps(%d)",
                m_captureResultToggle, m_previewDurationTime, ratio, burstshotTargetFps);

        CLOGV("[%d][%d][%d][%d] ratio(%d)",
                m_burstFps_history[0], m_burstFps_history[1],
                m_burstFps_history[2], m_burstFps_history[3], ratio);

        if (m_captureResultToggle != 0) {
            isSkipBuffer = true;
        }
    }

    return isSkipBuffer;
}

status_t ExynosCamera::m_captureFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraFrameFactory *targetfactory,
                                             frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    uint32_t requestKey = 0;
    bool captureFlag = false;
    bool rawStreamFlag = false;
    bool zslFlag = false;
    bool yuvCbStallFlag = false;
    bool thumbnailCbFlag = false;
    bool isNeedThumbnail = false;
    bool depthStallStreamFlag = false;
    int pipeId = -1;
    int yuvStallPortUsage = YUV_STALL_USAGE_DSCALED;
    frame_handle_components_t components;
    uint32_t streamConfigBit = 0;
    ExynosCameraActivitySpecialCapture *sCaptureMgr = NULL;
    ExynosCameraActivityFlash *m_flashMgr = NULL;
    camera3_stream_buffer_t* buffer = request->getInputBuffer();
    int dsInputPortId = request->getDsInputPortId();
    frame_type_t internalframeType = FRAME_TYPE_INTERNAL;
    bool flag3aaVraM2M = false;
    frame_queue_t *selectBayerQ;
    sp<mainCameraThread> selectBayerThread;

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_REPROCESSING_SLAVE
        || frameType == FRAME_TYPE_INTERNAL_SLAVE
        || frameType == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
        internalframeType = FRAME_TYPE_INTERNAL_SLAVE;
        selectBayerQ = m_selectDualSlaveBayerQ;
        selectBayerThread = m_selectDualSlaveBayerThread;
    } else
#endif
    {
        selectBayerQ = m_selectBayerQ;
        selectBayerThread = m_selectBayerThread;
    }

    getFrameHandleComponents(frameType, &components);
    int32_t reprocessingBayerMode = components.parameters->getReprocessingBayerMode();
    flag3aaVraM2M = (components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING,
                                    PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    sCaptureMgr = components.activityControl->getSpecialCaptureMgr();
    m_flashMgr = components.activityControl->getFlashMgr();

    if (targetfactory == NULL) {
        CLOGE("targetfactory is NULL");
        return INVALID_OPERATION;
    }

    CLOGD("Capture request. requestKey %d frameCount %d frameType %d",
            request->getKey(),
            request->getFrameCount(),
            frameType);

    m_startPictureBufferThread->join();

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_3AC_REPROCESSING, false);
    targetfactory->setRequest(PIPE_ISPC_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC0_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC2_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC_JPEG_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC_THUMB_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC5_REPROCESSING, false);
    targetfactory->setRequest(PIPE_VRA_REPROCESSING, false);

    if (components.parameters->isReprocessing() == true) {
        if (components.parameters->isUseHWFC()) {
            targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
            targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
        }
    }

    m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_DSCALED);

#ifdef USE_SLSI_PLUGIN
    m_prepareCaptureMode(request, components);
#endif
    m_captureFrameHandler_vendor_updateConfigMode(request, targetfactory, frameType);

    {
        m_longExposureRemainCount = 0;
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        targetfactory->setRequest(PIPE_SYNC_REPROCESSING, false);
        targetfactory->setRequest(PIPE_FUSION_REPROCESSING, false);

        if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
            if (m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC)
            {
                switch(frameType) {
                case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
                    targetfactory->setRequest(PIPE_SYNC_REPROCESSING, true);
                    break;
                case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                    targetfactory->setRequest(PIPE_SYNC_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_FUSION_REPROCESSING, true);
                    break;
                case FRAME_TYPE_REPROCESSING:
                    break;
                default:
                    CLOGE("Unsupported frame type(%d)", (int)frameType);
                    break;
                }
            }
        }
    }
#endif

    /* set input buffers belonged to each stream as available */
    if (buffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(buffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            SET_STREAM_CONFIG_BIT(streamConfigBit, inputStreamId);

            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d]) Buffer[%p], Stream[%p]",
                     request->getKey(), inputStreamId, buffer, stream);

            switch (inputStreamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
                CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_ZSL_INPUT[%d])",
                         request->getKey(), inputStreamId);
                zslFlag = true;
                break;
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                         request->getKey(), inputStreamId);
                break;
            default:
                break;
            }
        } else {
            CLOGE(" Stream is null (%d)", request->getKey());
        }
    }

    /* set output buffers belonged to each stream as available */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamIdwithBufferIdx(i);
        int yuvStallPort = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX);
        yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT);
        yuvStallPortUsage = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE);
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_JPEG:
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_JPEG), YuvStallPortUsage()(%d) yuvStallPort(%d)",
                    request->getKey(), yuvStallPortUsage, yuvStallPort);
            if (yuvStallPortUsage == YUV_STALL_USAGE_PICTURE) {
                pipeId = yuvStallPort + PIPE_MCSC0_REPROCESSING;
                targetfactory->setRequest(pipeId, true);
                dsInputPortId = yuvStallPort + MCSC_PORT_MAX;
            }

            if (yuvStallPortUsage != YUV_STALL_USAGE_PICTURE
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
                || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT
                || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT
#endif
               )
            {
                targetfactory->setRequest(PIPE_MCSC_JPEG_REPROCESSING, true);
                dsInputPortId = MCSC_PORT_3 + MCSC_PORT_MAX;

                shot_ext = request->getServiceShot();
                if (shot_ext == NULL) {
                    CLOGE("Get service shot fail. requestKey(%d)", request->getKey());

                    continue;
                }

                if (components.parameters->isReprocessing() == true && components.parameters->isUseHWFC() == true) {
                    isNeedThumbnail = (shot_ext->shot.ctl.jpeg.thumbnailSize[0] > 0
                            && shot_ext->shot.ctl.jpeg.thumbnailSize[1] > 0)? true : false;
                    targetfactory->setRequest(PIPE_MCSC_THUMB_REPROCESSING, isNeedThumbnail);
                    targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, isNeedThumbnail);
                }
            }

            captureFlag = true;
            break;
        case HAL_STREAM_ID_RAW:
            CLOGV("requestKey %d buffer-StreamType(HAL_STREAM_ID_RAW)",
                     request->getKey());
            targetfactory->setRequest(PIPE_3AC_REPROCESSING, true);
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_VIDEO:
        case HAL_STREAM_ID_VISION:
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
#endif
            break;
        case HAL_STREAM_ID_CALLBACK:
            if (zslFlag == false) {
                /* If there is no ZSL_INPUT stream buffer,
                * It will be processed through preview stream.
                */
                break;
            }
        case HAL_STREAM_ID_CALLBACK_STALL:
            CLOGV("requestKey %d buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
                     request->getKey());
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                    + PIPE_MCSC0_REPROCESSING;
            targetfactory->setRequest(pipeId, true);
            dsInputPortId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX) + MCSC_PORT_MAX;
            captureFlag = true;
            yuvCbStallFlag = true;
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
            break;
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_THUMBNAIL_CALLBACK), YuvStallPortUsage()(%d)",
                    request->getKey(), yuvStallPortUsage);
            thumbnailCbFlag = true;
            pipeId = yuvStallPort + PIPE_MCSC0_REPROCESSING;
            targetfactory->setRequest(pipeId, true);
            break;
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP_STALL:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_DEPTHMAP_STALL)",
                    request->getKey(), i);
            depthStallStreamFlag = true;
            break;
#endif
        default:
            CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                     request->getKey(), id);
            break;
        }
    }

#ifdef USE_DUAL_CAMERA
GENERATE_FRAME:
#endif
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        return INVALID_OPERATION;
    }

    m_updateCropRegion(m_currentCaptureShot);
    m_updateJpegControlInfo(m_currentCaptureShot);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentCaptureShot, service_shot_ext->shot.ctl.stats.faceDetectMode,
                    dsInputPortId, true, flag3aaVraM2M);
    }
    components.parameters->setDsInputPortId(m_currentCaptureShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], true);
    m_updateEdgeNoiseMode(m_currentCaptureShot, true);
    m_updateSetfile(m_currentCaptureShot, true);
    m_updateMasterCam(m_currentCaptureShot);

    m_captureFrameHandler_vendor_updateDualROI(request, components, frameType);

    if (m_currentCaptureShot->fd_bypass == false) {
       if (components.parameters->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M
#ifdef USE_DUAL_CAMERA
        && m_configurations->getDualOperationMode() != DUAL_OPERATION_MODE_SLAVE /* HACK: temp for dual capture */
        && frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE/* HACK: temp for dual capture */
#endif
            ) {
            targetfactory->setRequest(PIPE_MCSC5_REPROCESSING, true);
            targetfactory->setRequest(PIPE_VRA_REPROCESSING, true);
        }
    }

    if ((reprocessingBayerMode == REPROCESSING_BAYER_MODE_PURE_DYNAMIC
            || reprocessingBayerMode == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
        && (m_currentCaptureShot->shot.ctl.flash.flashMode == CAM2_FLASH_MODE_SINGLE
            || m_flashMgr->getNeedCaptureFlash() == true)) {
        m_needDynamicBayerCount = FLASH_MAIN_TIMEOUT_COUNT;
        CLOGD("needDynamicBayerCount(%d) NeedFlash(%d) flashMode(%d)", m_needDynamicBayerCount,
            m_flashMgr->getNeedCaptureFlash(),
            m_currentCaptureShot->shot.ctl.flash.flashMode);
    }

#ifdef OIS_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true) {
        m_captureFrameHandler_vendor_updateIntent(request, components);
    } else
#endif
    {
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
        if (zslFlag == false
            && m_configurations->getCaptureExposureTime() > PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX
            && m_configurations->getSamsungCamera() == true) {
            unsigned int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_EXPOSURE_DYNAMIC_SHOT;
            unsigned int captureCount = m_configurations->getLongExposureShotCount();
            unsigned int mask = 0;
            int captureExposureTime = m_configurations->getCaptureExposureTime();
            int value;

            mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
            value = mask;

            if (components.previewFactory == NULL) {
                CLOGE("FrameFactory is NULL!!");
            } else {
                ret = components.previewFactory->setControl(V4L2_CID_IS_CAPTURE_EXPOSURETIME, captureExposureTime, PIPE_3AA);
                if (ret) {
                    CLOGE("setControl() fail. ret(%d) captureExposureTime(%d)",
                            ret, captureExposureTime);
                } else {
                    CLOGD("setcontrol() V4L2_CID_IS_CAPTURE_EXPOSURETIME:(%d)", captureExposureTime);
                }

                ret = components.previewFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
                if (ret) {
                    CLOGE("setcontrol(V4L2_CID_IS_INTENT) failed!!");
                } else {
                    CLOGD("setcontrol() V4L2_CID_IS_INTENT:(%d)", value);
                }
            }

            m_currentCaptureShot->shot.ctl.aa.captureIntent = (enum aa_capture_intent)captureIntent;
            m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            components.activityControl->setOISCaptureMode(true);
        }
#endif
    }

    list<int> scenarioList;
    int pluginCnt = m_prepareCapturePlugin(targetfactory, &scenarioList);

#ifdef SAMSUNG_TN_FEATURE
    int connectedPipeNum = m_connectCaptureUniPP(targetfactory);
#endif

    m_frameCountLock.lock();

#ifdef USE_SLSI_PLUGIN
    if (m_configurations->getMode(CONFIGURATION_HIFI_LLS_MODE)
#ifdef USE_LLS_REPROCESSING
        || m_parameters[m_cameraId]->getLLSCaptureOn() == true
#endif
        ) {
        int pluginYuvStallPortUsage = false;
		int scenario = 0;
		int pluginPipeId = 0;

#ifdef USES_HIFI_LLS
        if (m_configurations->getMode(CONFIGURATION_HIFI_LLS_MODE)) {
            pluginYuvStallPortUsage = yuvStallPortUsage;
            pluginPipeId = PIPE_PLUGIN_POST1_REPROCESSING;
            scenario = PLUGIN_SCENARIO_HIFILLS_REPROCESSING;
        }
#endif

        int captureCount = m_configurations->getModeValue(CONFIGURATION_CAPTURE_COUNT);

        for (int i = 0; i < (captureCount-1); i++) {
            ExynosCameraFrameSP_sptr_t newLLSFrame = NULL;

            ret = m_generateInternalFrame(targetfactory, &m_captureProcessList,
                    &m_captureProcessLock, newLLSFrame, internalframeType, request);
            if (ret != NO_ERROR) {
                CLOGE("m_generateInternalFrame fail");
                return ret;
            } else if (newLLSFrame == NULL) {
                CLOGE("new faame is NULL");
                return INVALID_OPERATION;
            }
            newLLSFrame->setStreamRequested(STREAM_TYPE_RAW, true);
            newLLSFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, false);
            newLLSFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, false);
            newLLSFrame->setStreamRequested(STREAM_TYPE_CAPTURE, false);
            newLLSFrame->setFrameYuvStallPortUsage(pluginYuvStallPortUsage);
            newLLSFrame->setPPScenario(pluginPipeId, scenario);
            newLLSFrame->setFrameIndex(i);
            newLLSFrame->setMaxFrameIndex(captureCount);


            CLOGD("[%d] LLS generate request framecount(%d) requestKey(%d) m_internalFrameCount(%d)",
                    i, newLLSFrame->getFrameCount(), request->getKey(), m_internalFrameCount);

            if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGD("[R%d F%d]Flush is in progress.",
                        request->getKey(), newLLSFrame->getFrameCount());
                /* Generated frame is going to be deleted at flush() */
                return ret;
            }

            ret = newLLSFrame->setMetaData(m_currentCaptureShot);
            if (ret != NO_ERROR) {
                CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                        newLLSFrame->getFrameCount(), ret);
            }

            ret = m_setupCaptureFactoryBuffers(request, newLLSFrame);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to setupCaptureStreamBuffer. ret %d",
                        request->getKey(), newLLSFrame->getFrameCount(), ret);
            }

            m_selectBayerQ->pushProcessQ(&newLLSFrame);
        }
    }
#endif

    if (request->getFrameCount() == 0) {
        /* Must use the same framecount with internal frame */
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
    }
    m_frameCountLock.unlock();

    {
        if (m_longExposureRemainCount > 0 && zslFlag == false) {
            for (uint32_t i = 0; i < m_longExposureRemainCount; i++) {
                ExynosCameraFrameSP_sptr_t newLongExposureCaptureFrame = NULL;
                frame_type_t LongExposureframeType = frameType;

                if (i < m_longExposureRemainCount - 1) {
                    LongExposureframeType = internalframeType;
                    ret = m_generateInternalFrame(targetfactory, &m_captureProcessList,
                            &m_captureProcessLock, newLongExposureCaptureFrame, internalframeType);
                    if (ret != NO_ERROR) {
                        CLOGE("m_generateFrame fail");
                        return ret;
                    } else if (newLongExposureCaptureFrame == NULL) {
                        CLOGE("new frame is NULL");
                        return INVALID_OPERATION;
                    }

                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_RAW, false);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, yuvCbStallFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_ZSL_INPUT, zslFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, thumbnailCbFlag);
                    newLongExposureCaptureFrame->setFrameYuvStallPortUsage(yuvStallPortUsage);
                } else {
                    ret = m_generateFrame(targetfactory, &m_captureProcessList, &m_captureProcessLock,
                            newLongExposureCaptureFrame, request);
                    if (ret != NO_ERROR) {
                        CLOGE("m_generateFrame fail");
                        return ret;
                    } else if (newLongExposureCaptureFrame == NULL) {
                        CLOGE("new faame is NULL");
                        return INVALID_OPERATION;
                    }

                    newLongExposureCaptureFrame->setFrameType(LongExposureframeType);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_RAW, rawStreamFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, yuvCbStallFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_ZSL_INPUT, zslFlag);
                    newLongExposureCaptureFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, thumbnailCbFlag);
                    newLongExposureCaptureFrame->setFrameYuvStallPortUsage(yuvStallPortUsage);

                    ret = m_setupCaptureFactoryBuffers(request, newLongExposureCaptureFrame);
                    if (ret != NO_ERROR) {
                        CLOGE("[R%d F%d]Failed to setupCaptureStreamBuffer. ret %d",
                                request->getKey(), newLongExposureCaptureFrame->getFrameCount(), ret);
                    }

                    m_checkUpdateResult(newLongExposureCaptureFrame, streamConfigBit);
                }

                CLOGD("generate request framecount(%d) requestKey(%d) m_internalFrameCount(%d)",
                        newLongExposureCaptureFrame->getFrameCount(), request->getKey(), m_internalFrameCount);

                TIME_LOGGER_UPDATE(m_cameraId, request->getKey(), 0, USER_DATA, CREATE_CAPTURE_FRAME,
                                   newLongExposureCaptureFrame->getFrameCount());

                if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                    CLOGD("[R%d F%d]Flush is in progress.",
                            request->getKey(), newLongExposureCaptureFrame->getFrameCount());
                    /* Generated frame is going to be deleted at flush() */
                    return ret;
                }

                ret = newLongExposureCaptureFrame->setMetaData(m_currentCaptureShot);
                if (ret != NO_ERROR) {
                    CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                            newLongExposureCaptureFrame->getFrameCount(), ret);
                }

                selectBayerQ->pushProcessQ(&newLongExposureCaptureFrame);
            }
        } else {
            int frameCount = m_configurations->getModeValue(CONFIGURATION_CAPTURE_COUNT);

            if (yuvCbStallFlag && frameType != FRAME_TYPE_JPEG_REPROCESSING) {
                ret = m_generateFrame(targetfactory, &m_captureProcessList, &m_captureProcessLock,
                                        newFrame, request);
            } else {
                bool useJpegFlag = (yuvStallPortUsage == YUV_STALL_USAGE_PICTURE)? true : false;
                ret = m_generateFrame(targetfactory, &m_captureProcessList, &m_captureProcessLock,
                                        newFrame, request, useJpegFlag);
            }

            if (ret != NO_ERROR) {
                CLOGE("m_generateFrame fail");
                return ret;
            } else if (newFrame == NULL) {
                CLOGE("new faame is NULL");
                return INVALID_OPERATION;
            }

            CLOGV("[R%d F%d]Generate capture frame. streamConfig %x",
                    request->getKey(), newFrame->getFrameCount(), streamConfigBit);

            if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGD("[R%d F%d]Flush is in progress.",
                        request->getKey(), newFrame->getFrameCount());
                /* Generated frame is going to be deleted at flush() */
                return ret;
            }

            TIME_LOGGER_UPDATE(m_cameraId, request->getKey(), 0, USER_DATA, CREATE_CAPTURE_FRAME, newFrame->getFrameCount());

            ret = newFrame->setMetaData(m_currentCaptureShot);
            if (ret != NO_ERROR) {
                CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                        newFrame->getFrameCount(), ret);
            }

            newFrame->setFrameType(frameType);

            newFrame->setStreamRequested(STREAM_TYPE_RAW, rawStreamFlag);
            newFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
            newFrame->setStreamRequested(STREAM_TYPE_ZSL_INPUT, zslFlag);
            newFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, yuvCbStallFlag);
            newFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, thumbnailCbFlag);
            newFrame->setFrameYuvStallPortUsage(yuvStallPortUsage);
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
            newFrame->setAdjustedTimestampFlag(zslFlag);
#endif
#ifdef SUPPORT_DEPTH_MAP
            newFrame->setStreamRequested(STREAM_TYPE_DEPTH_STALL, depthStallStreamFlag);
#endif

            ret = m_setupCaptureFactoryBuffers(request, newFrame);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to setupCaptureStreamBuffer. ret %d",
                        request->getKey(), newFrame->getFrameCount(), ret);
            }

            m_checkUpdateResult(newFrame, streamConfigBit);

#ifdef USE_LLS_REPROCESSING
            if (m_parameters[m_cameraId]->getLLSCaptureOn() == true)
                newFrame->setFrameIndex(m_parameters[m_cameraId]->getLLSCaptureCount());
#endif
            newFrame->setFrameIndex(frameCount-1);
            newFrame->setMaxFrameIndex(frameCount);

            selectBayerQ->pushProcessQ(&newFrame);
        }
    }

    if (selectBayerThread != NULL && selectBayerThread->isRunning() == false) {
        selectBayerThread->run();
        CLOGI("Initiate selectBayerThread (%d)", selectBayerThread->getTid());
    }

    return ret;
}

/* release all buffer to be appened to frame by selectorTag */
status_t ExynosCamera::m_releaseSelectorTagBuffers(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSelectorTag tag;
    ExynosCameraBuffer buffer;

    if (frame == NULL)
        return ret;

    /* lock for selectorTag */
    frame->lockSelectorTagList();

    while (frame->getFirstRawSelectorTag(&tag)) {
        buffer.index = -1;
        if (tag.isSrc)
            ret = frame->getSrcBuffer(tag.pipeId, &buffer, tag.bufPos);
        else
            ret = frame->getDstBuffer(tag.pipeId, &buffer, tag.bufPos);

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for ISP. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
        frame->removeRawSelectorTag(&tag);
    }

    /* unlock for selectorTag */
    frame->unlockSelectorTagList();

    return ret;
}

status_t ExynosCamera::m_getBayerBuffer(uint32_t pipeId,
                                         uint32_t frameCount,
                                         ExynosCameraBuffer *buffer,
                                         ExynosCameraFrameSelector *selector,
                                         frame_type_t frameType,
                                         ExynosCameraFrameSP_dptr_t frame
#ifdef SUPPORT_DEPTH_MAP
                                         , ExynosCameraBuffer *depthMapBuffer
#endif
        )
{
    status_t ret = NO_ERROR;
    int dstPos = 0;
    int retryCount = 30; /* 200ms x 30 */
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    frame_handle_components_t components;

    getFrameHandleComponents(frameType, &components);

    if (components.parameters->isReprocessing() == false || selector == NULL) {
        CLOGE("[F%d]INVALID_OPERATION, isReprocessing(%s) or selector is NULL",
                frameCount, components.parameters->isReprocessing() ? "True" : "False");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    selector->setWaitTime(200000000);

    switch (components.parameters->getReprocessingBayerMode()) {
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        dstPos = components.previewFactory->getNodeType(PIPE_VC0);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        dstPos = components.previewFactory->getNodeType(PIPE_3AC);
        break;
    default:
        break;
    }

#ifdef SUPPORT_ME
#ifdef USE_LLS_REPROCESSING
RETRY_TO_SELECT_FRAME:
#endif
#endif
    bayerFrame = selector->selectCaptureFrames(1, frameCount, retryCount);
    if (bayerFrame == NULL) {
        CLOGE("[F%d]bayerFrame is NULL", frameCount);
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    ret = bayerFrame->getDstBuffer(pipeId, buffer, dstPos);
    if (ret != NO_ERROR || buffer->index < 0) {
        CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d pos %d ret %d",
                bayerFrame->getFrameCount(), buffer->index, pipeId, dstPos, ret);
        goto CLEAN;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (bayerFrame->getRequest(PIPE_VC1)) {
        dstPos = components.previewFactory->getNodeType(PIPE_VC1);

        ret = bayerFrame->getDstBuffer(pipeId, depthMapBuffer, dstPos);
        if (ret != NO_ERROR || depthMapBuffer->index < 0) {
            CLOGE("[F%d B%d]Failed to get DepthMap buffer. ret %d",
                    bayerFrame->getFrameCount(), depthMapBuffer->index, ret);
        }
    }
#endif

    /* Wait to be enabled Metadata */
    retryCount = 12; /* 30ms * 12 */
    while (retryCount > 0) {
        if (bayerFrame->getMetaDataEnable() == false) {
            CLOGD("[F%d B%d]Failed to wait Metadata Enabled. ret %d retryCount %d",
                    bayerFrame->getFrameCount(), buffer->index, ret, retryCount);
        } else {
            CLOGI("[F%d B%d]Frame metadata is updated.",
                    bayerFrame->getFrameCount(), buffer->index);
            break;
        }

        retryCount--;
        usleep(DM_WAITING_TIME);
    }

    CLOGD("[F%d(%d) B%d T%d]Frame is selected timestamp(%jd)",
            bayerFrame->getFrameCount(), bayerFrame->getMetaFrameCount(), buffer->index,
            bayerFrame->getFrameType(), bayerFrame->getTimeStampBoot());

#ifdef SUPPORT_ME
#ifdef USE_LLS_REPROCESSING
    /* Wait to be enabled ME Data */
    int retrySelectCount = 3;
    if (m_parameters[m_cameraId]->getLLSCaptureOn() == true) {
        int meSrcPipeId = PIPE_ISP;
        int dstPos = factory->getNodeType(PIPE_ME);
        entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_ERROR;

        if (bayerFrame->getFrameType() == FRAME_TYPE_INTERNAL &&
                retrySelectCount > 0) {
            CLOGE("[F%d(%d) B%d] This frame is internal frame(S:%d, T:%d).",
                    bayerFrame->getFrameCount(), bayerFrame->getFrameState(), bayerFrame->getFrameType());
            if (buffer->index >= 0) {
                ret = m_releaseSelectorTagBuffers(bayerFrame);
                if (ret < 0)
                    CLOGE("Failed to releaseSelectorTagBuffers. frameCount %d ret %d",
                            bayerFrame->getFrameCount(), ret);
            }

            retrySelectCount--;
            goto RETRY_TO_SELECT_FRAME;
        }

        bayerFrame->getDstBufferState(meSrcPipeId, &bufferState, dstPos);
        if (ret != NO_ERROR) {
            CLOGE("[F%d(%d) B%d]Failed to getDstBufferState from frame(S:%d, T:%d).",
                    bayerFrame->getFrameCount(), bayerFrame->getFrameState(), bayerFrame->getFrameType());
        }

        retryCount = 12; /* 30ms * 12 */

        /* loop until the ME buffer state equals error or complete */
        while (retryCount > 0 &&
                bayerFrame->getRequest(PIPE_ME) == true &&
                !(bufferState == ENTITY_BUFFER_STATE_ERROR ||
                    bufferState == ENTITY_BUFFER_STATE_COMPLETE)) {
            CLOGI("[F%d B%d] Waiting for Frame until ME data(%d) is updated. retry(%d)",
                    bayerFrame->getFrameCount(), buffer->index, bufferState, retryCount);
            retryCount--;
            usleep(DM_WAITING_TIME);
            bayerFrame->getDstBufferState(meSrcPipeId, &bufferState, dstPos);
        }

        /* try to select bayerFrame again */
        if (bufferState != ENTITY_BUFFER_STATE_COMPLETE &&
                (retrySelectCount--) > 0) {
            CLOGI("[F%d B%d]Frame(state:%d, type:%d, %d, %d) ME metadata is not updated..try to select again(%d)",
                    bayerFrame->getFrameCount(),
                    bayerFrame->getRequest(PIPE_ME),
                    bayerFrame->getFrameState(),
                    bayerFrame->getFrameType(),
                    bufferState, retryCount,
                    buffer->index, retrySelectCount);
            ret = m_releaseSelectorTagBuffers(bayerFrame);
            if (ret < 0)
                CLOGE("Failed to releaseSelectorTagBuffers. frameCount %d ret %d",
                        bayerFrame->getFrameCount(), ret);
            goto RETRY_TO_SELECT_FRAME;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Frame ME metadata is not updated..invalid frame",
                    bayerFrame->getFrameCount(), buffer->index);
        }
    }
#endif
#endif

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()
        && m_parameters[m_cameraId]->getUsePureBayerReprocessing() == false
        && bayerFrame->getRequest(PIPE_VC0) == true) {
        ExynosCameraBuffer rawDumpBuffer;
        bool bRet;
        char filePath[70];
        time_t rawtime;
        struct tm *timeinfo;

        ret = bayerFrame->getDstBuffer(pipeId, &rawDumpBuffer, components.previewFactory->getNodeType(PIPE_VC0));
        if (ret != NO_ERROR || rawDumpBuffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getDstBuffer for RAW_DUMP. pipeId %d ret %d",
                    bayerFrame->getFrameCount(), rawDumpBuffer.index, pipeId, ret);
        } else {
            memset(filePath, 0, sizeof(filePath));
            time(&rawtime);
            timeinfo = localtime(&rawtime);
            snprintf(filePath, sizeof(filePath), "/data/camera/Raw%d_%02d%02d%02d_%02d%02d%02d_%d.raw",
                    m_cameraId, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                    timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, bayerFrame->getFrameCount());

            bRet = dumpToFile((char *)filePath,
                    rawDumpBuffer.addr[0],
                    rawDumpBuffer.size[0]);
            if (bRet != true)
                CLOGE("couldn't make a raw file");

            m_bufferSupplier->putBuffer(rawDumpBuffer);
        }
    }
#endif

CLEAN:
    frame = bayerFrame;

    if (bayerFrame != NULL) {
        ret = m_searchFrameFromList(&m_processList, &m_processLock, bayerFrame->getFrameCount(), inListFrame);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]searchFrameFromList fail", bayerFrame->getFrameCount());
        } else {
            CLOGD("[F%d]Selected frame complete, Delete", bayerFrame->getFrameCount());
            bayerFrame = NULL;
        }
    }

    return ret;
}

bool ExynosCamera::m_isRequestEssential(ExynosCameraRequestSP_dptr_t request)
{
    CameraMetadata* serviceMeta = request->getServiceMeta();
    camera_metadata_entry_t entry;
    bool ret = false;

    /* Check AF Trigger */
    entry = serviceMeta->find(ANDROID_CONTROL_AF_TRIGGER);
    if (entry.count > 0) {
        if (entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_START
            || entry.data.u8[0] == ANDROID_CONTROL_AF_TRIGGER_CANCEL) {
            ret = true;
        }
    }

    return ret;
}

status_t ExynosCamera::m_updateResultShot(ExynosCameraRequestSP_sprt_t request,
                                           struct camera2_shot_ext *src_ext,
                                           enum metadata_type metaType,
                                           frame_type_t frameType)
{
    // TODO: More efficient implementation is required.
    Mutex::Autolock lock(m_updateMetaLock);

    status_t ret = NO_ERROR;
    struct camera2_shot_ext *dst_ext = NULL;
    uint8_t currentPipelineDepth = 0;
    uint32_t frameCount = 0;
    int Bv = 0;
    int shutter_speed = 0;
    int pre_burst_fps = 0;
    int burst_fps = 0;

    frame_handle_components_t components;
    getFrameHandleComponents(frameType, &components);

    if (src_ext == NULL) {
        CLOGE("src_ext is null.");
        return INVALID_OPERATION;
    }

    dst_ext = request->getServiceShot();
    if (dst_ext == NULL) {
        CLOGE("getServiceShot failed.");
        return INVALID_OPERATION;
    }

    currentPipelineDepth = dst_ext->shot.dm.request.pipelineDepth;
    frameCount = dst_ext->shot.dm.request.frameCount;
    memcpy(&dst_ext->shot.dm, &src_ext->shot.dm, sizeof(struct camera2_dm));
    memcpy(&dst_ext->shot.udm, &src_ext->shot.udm, sizeof(struct camera2_udm));
    dst_ext->shot.dm.request.pipelineDepth = currentPipelineDepth;

    /* Keep previous sensor framecount */
    if (frameCount > 0) {
        dst_ext->shot.dm.request.frameCount = frameCount;
    }
    if (request->getSensorTimestamp() == 0) {
        request->setSensorTimestamp(src_ext->shot.udm.sensor.timeStampBoot);
    }

    dst_ext->shot.udm.sensor.timeStampBoot = request->getSensorTimestamp();

    switch (metaType) {
    case PARTIAL_NONE:
        if (src_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF
            && components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            /* When VRA works as M2M mode, FD metadata will be updated with the latest one in Parameters */
            {
                components.parameters->getFaceDetectMeta(dst_ext);
            }
        }
        break;
    case PARTIAL_3AA:
#ifdef LLS_CAPTURE
        {
            int LLS_value = 0;
            int pre_LLS_value = 0;

            LLS_value = components.parameters->getLLSValue();

            pre_LLS_value = m_configurations->getModeValue(CONFIGURATION_LLS_VALUE);

            if (LLS_value != pre_LLS_value) {
                m_configurations->setModeValue(CONFIGURATION_LLS_VALUE, LLS_value);
                CLOGD("LLS_value(%d)", LLS_value);
            }
        }
#endif

        Bv = dst_ext->shot.udm.internal.vendorSpecific[2];
        shutter_speed = dst_ext->shot.udm.ae.vendorSpecific[64];
        pre_burst_fps = m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS);

        if (pre_burst_fps != burst_fps) {
            m_configurations->setModeValue(CONFIGURATION_BURSTSHOT_FPS, burst_fps);
            CLOGD("burstshot_fps(%d) / Bv(%d) / Shutter speed(%d)", burst_fps, Bv, shutter_speed);
        }
        break;
    case PARTIAL_JPEG:
    default:
        /* no operation */
        break;
    }

    ret = m_metadataConverter->updateDynamicMeta(request, metaType);

    CLOGV("[R%d F%d f%d M%d] Set result.",
            request->getKey(), request->getFrameCount(),
            dst_ext->shot.dm.request.frameCount, metaType);

    return ret;
}

status_t ExynosCamera::m_updateJpegPartialResultShot(ExynosCameraRequestSP_sprt_t request, struct camera2_shot_ext *dst_ext)
{
    status_t ret = NO_ERROR;

    if (dst_ext == NULL) {
        CLOGE("dst Shot failed.");
        return INVALID_OPERATION;
    }

    m_parameters[m_cameraId]->setExifChangedAttribute(NULL, NULL, NULL, &dst_ext->shot);
    ret = m_metadataConverter->updateDynamicMeta(request, PARTIAL_JPEG);

    CLOGV("[F%d(%d)]Set result.",
            request->getFrameCount(), dst_ext->shot.dm.request.frameCount);

    return ret;
}

status_t ExynosCamera::m_setupFrameFactoryToRequest()
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameFactory *factoryVision = NULL;
    ExynosCameraFrameFactory *factoryReprocessing = NULL;
    ExynosCameraFrameFactory *factoryJpegReprocessing = NULL;

    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    factoryVision = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    factoryReprocessing = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    factoryJpegReprocessing = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];

    if (factory != NULL && factoryReprocessing != NULL) {
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW_VIDEO, factory);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factory);
#ifdef SUPPORT_DEPTH_MAP
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_DEPTHMAP, factory);
#endif
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VISION, factory);

        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factoryReprocessing);
        if (factoryJpegReprocessing != NULL)
                m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryJpegReprocessing);
        else
                m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryReprocessing);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_INPUT, factoryReprocessing);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK_STALL, factoryReprocessing);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_THUMBNAIL_CALLBACK, factoryReprocessing);
#ifdef SUPPORT_DEPTH_MAP
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_DEPTHMAP_STALL, factoryReprocessing);
#endif
    } else if (factoryVision != NULL) {
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_PREVIEW, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VIDEO, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_OUTPUT, factoryVision);
#ifdef SUPPORT_DEPTH_MAP
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_DEPTHMAP, factoryVision);
#endif
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_VISION, factoryVision);

        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_RAW, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_JPEG, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_ZSL_INPUT, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_CALLBACK_STALL, factoryVision);
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_THUMBNAIL_CALLBACK, factoryVision);
#ifdef SUPPORT_DEPTH_MAP
        m_requestMgr->setRequestsInfo(HAL_STREAM_ID_DEPTHMAP_STALL, factoryVision);
#endif
    } else {
        if (factory == NULL) {
            CLOGE("FRAME_FACTORY_TYPE_CAPTURE_PREVIEW factory is NULL!!!!");
        }
        if (factoryReprocessing == NULL) {
            CLOGE("FRAME_FACTORY_TYPE_REPROCESSING factory is NULL!!!!");
        }
        if (factoryVision == NULL) {
            CLOGE("FRAME_FACTORY_TYPE_VISION factory is NULL!!!!");
        }
    }

    return ret;
}

status_t ExynosCamera::m_setStreamInfo(camera3_stream_configuration *streamList)
{
    int ret = OK;
    int id = 0;
    int recordingFps = 0;
    int highSpeedMode = -1;

    CLOGD("In");

    /* sanity check */
    if (streamList == NULL) {
        CLOGE("NULL stream configuration");
        return BAD_VALUE;
    }

    if (streamList->streams == NULL) {
        CLOGE("NULL stream list");
        return BAD_VALUE;
    }

    if (streamList->num_streams < 1) {
        CLOGE(" Bad number of streams requested: %d", streamList->num_streams);
        return BAD_VALUE;
    }

    /* check input stream */
    bool inputStreamExist = false;
    /* initialize variable */
    m_captureStreamExist = false;
    m_videoStreamExist = false;
    m_rawStreamExist = false;
    m_configurations->setMode(CONFIGURATION_YSUM_RECORDING_MODE, false);

    for (size_t i = 0; i < streamList->num_streams; i++) {
        camera3_stream_t *newStream = streamList->streams[i];
        int option = 0;

        if (newStream == NULL) {
            CLOGE("Stream index %zu was NULL", i);
            return BAD_VALUE;
        }

        // for debug
        CLOGD("Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d), option(%#x)",
            newStream, i, newStream->stream_type, newStream->usage,
            newStream->format, (int)(newStream->width), (int)(newStream->height), option);

        if ((int)(newStream->width) <= 0
            || (int)(newStream->height) <= 0
            || newStream->format < 0
            || newStream->rotation < 0) {
            CLOGE("Invalid Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d), option(%#x)",
                newStream, i, newStream->stream_type, newStream->usage,
                newStream->format, (int)(newStream->width), (int)(newStream->height), option);
            return BAD_VALUE;
        }

        if ((newStream->stream_type == CAMERA3_STREAM_INPUT)
            || (newStream->stream_type == CAMERA3_STREAM_BIDIRECTIONAL)) {
            if (inputStreamExist == true) {
                CLOGE("Multiple input streams requested!");
                return BAD_VALUE;
            }
            inputStreamExist = true;
            m_captureStreamExist = true;
        }

        if (newStream->stream_type == CAMERA3_STREAM_OUTPUT) {
            if ((newStream->format == HAL_PIXEL_FORMAT_BLOB)
                || (newStream->format == HAL_PIXEL_FORMAT_RAW16)
                || (newStream->format == HAL_PIXEL_FORMAT_YCbCr_420_888
                    && (option & STREAM_OPTION_STALL_MASK))) {
                m_captureStreamExist = true;
            }

            if (newStream->format == HAL_PIXEL_FORMAT_RAW16
                && !(option & STREAM_OPTION_DEPTH10_MASK)) {
                m_rawStreamExist = true;
            }
        }

        if ((newStream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
            && (newStream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER)) {
            CLOGI("recording stream checked");
            m_videoStreamExist = true;
        }

        // TODO: format validation
    }

    recordingFps = m_configurations->getModeValue(CONFIGURATION_RECORDING_FPS);

    if (streamList->operation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE) {
        CLOGI("High speed mode is configured. StreamCount %d. m_videoStreamExist(%d) recordingFps(%d)",
                streamList->num_streams, m_videoStreamExist, recordingFps);

        if (m_videoStreamExist == true && recordingFps == 120) {
            highSpeedMode = CONFIG_MODE::HIGHSPEED_120;
        } else if (m_videoStreamExist == true && recordingFps == 240) {
            highSpeedMode = CONFIG_MODE::HIGHSPEED_240;
        } else if (m_videoStreamExist == true && recordingFps == 480) {
            highSpeedMode = CONFIG_MODE::HIGHSPEED_480;
        } else {
            if (m_parameters[m_cameraId]->getMaxHighSpeedFps() == 480) {
                highSpeedMode = CONFIG_MODE::HIGHSPEED_480;
            } else if (m_parameters[m_cameraId]->getMaxHighSpeedFps() == 240) {
                highSpeedMode = CONFIG_MODE::HIGHSPEED_240;
            } else {
                highSpeedMode = CONFIG_MODE::HIGHSPEED_120;
            }
        }
    } else {
        {
            if (m_videoStreamExist == true) {
                m_configurations->setMode(CONFIGURATION_YSUM_RECORDING_MODE, true);
                highSpeedMode = (recordingFps == 60) ? CONFIG_MODE::HIGHSPEED_60 : CONFIG_MODE::NORMAL;
            } else {
                highSpeedMode = CONFIG_MODE::NORMAL;
            }
        }
    }

    m_configurations->setModeValue(CONFIGURATION_HIGHSPEED_MODE, highSpeedMode);
    m_exynosconfig = m_configurations->getConfig();

    /* 1. Invalidate all current streams */
    List<int> keylist;
    List<int>::iterator iter;
    ExynosCameraStream *stream = NULL;
    keylist.clear();
    m_streamManager->getStreamKeys(&keylist);
    for (iter = keylist.begin(); iter != keylist.end(); iter++) {
        m_streamManager->getStream(*iter, &stream);
        stream->setRegisterStream(EXYNOS_STREAM::HAL_STREAM_STS_INVALID);
        stream->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED);
    }

    /* 2. Remove dead streams */
    keylist.clear();
    stream = NULL;
    id = 0;
    EXYNOS_STREAM::STATE registerStream = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    m_streamManager->getStreamKeys(&keylist);
    for (iter = keylist.begin(); iter != keylist.end(); iter++) {
        m_streamManager->getStream(*iter, &stream);
        ret = stream->getRegisterStream(&registerStream);
        if (registerStream == EXYNOS_STREAM::HAL_STREAM_STS_INVALID){
            ret = stream->getID(&id);
            if (id < 0) {
                CLOGE("getID failed id(%d)", id);
                continue;
            }
            m_streamManager->deleteStream(id);
        }
    }

    /* 3. Update stream info */
    for (size_t i = 0; i < streamList->num_streams; i++) {
        stream = NULL;
        camera3_stream_t *newStream = streamList->streams[i];
        if (newStream->priv == NULL) {
            /* new stream case */
            ret = m_enumStreamInfo(newStream);
            if (ret) {
                CLOGE("Register stream failed %p", newStream);
                return ret;
            }
        } else {
            /* Existing stream, reuse current stream */
            stream = static_cast<ExynosCameraStream*>(newStream->priv);
            stream->setRegisterStream(EXYNOS_STREAM::HAL_STREAM_STS_VALID);
        }
    }

    CLOGD("out");
    return ret;
}

status_t ExynosCamera::m_enumStreamInfo(camera3_stream_t *stream)
{
    CLOGD("In");
    int ret = OK;
    ExynosCameraStream *newStream = NULL;
    int id = 0;
    int actualFormat = 0;
    int planeCount = 0;
    int requestBuffer = 0;
    int outputPortId = 0;
    int option = 0;
    EXYNOS_STREAM::STATE registerStream;
    EXYNOS_STREAM::STATE registerBuffer;

    registerStream = EXYNOS_STREAM::HAL_STREAM_STS_VALID;
    registerBuffer = EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED;

    if (stream == NULL) {
        CLOGE("stream is NULL.");
        return INVALID_OPERATION;
    }

    /* default surface usage */
    stream->usage |= GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL;

    /* ToDo: need to Consideration about how to support each camera3_stream_rotation_t */
    if (stream->rotation != CAMERA3_STREAM_ROTATION_0) {
        CLOGE("Invalid stream rotation %d", stream->rotation);
        ret = BAD_VALUE;
        goto func_err;
    }

    switch (stream->stream_type) {
    case CAMERA3_STREAM_OUTPUT:
        // TODO: split this routine to function
        switch (stream->format) {
        case HAL_PIXEL_FORMAT_BLOB:
            CLOGD("HAL_PIXEL_FORMAT_BLOB format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_JPEG;
            actualFormat = HAL_PIXEL_FORMAT_BLOB;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_burst_capture_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if ((stream->usage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE || stream->usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER)
                && (stream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER)) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE|HWCOMPOSER|VIDEO_ENCODER foramt(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                        stream->format, stream->usage, stream->stream_type, option);
                id = HAL_STREAM_ID_PREVIEW_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */
                planeCount = 2;
                stream->usage &= ~GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL;
#ifdef USE_SERVICE_BATCH_MODE
                enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
                int batchSize = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
                if (batchSize > 1 && m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                    CLOGD("Use service batch mode. Add HFR_MODE usage bit. batchSize %d", batchSize);
                    stream->usage |= GRALLOC1_PRODUCER_USAGE_HFR_MODE;
                }
#endif
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if (stream->usage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE
                || stream->usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE|HWCOMPOSER foramt(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                    stream->format, stream->usage, stream->stream_type, option);
                id = HAL_STREAM_ID_PREVIEW;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;

                /* Cached for PP Pipe */
                stream->usage |= (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);

                planeCount = 2;

                /* set Preview Size */
                m_configurations->setSize(CONFIGURATION_PREVIEW_SIZE, stream->width, stream->height);

                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_preview_buffers;
            } else if(stream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER format(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                    stream->format, stream->usage, stream->stream_type, option);
                id = HAL_STREAM_ID_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */

                if (m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true) {
                    stream->usage |= (GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA)
                                     | (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
                    planeCount = 3;
                } else {
#ifdef GRALLOC_ALWAYS_ALLOC_VIDEO_META
                    planeCount = 3;
#else
                    planeCount = 2;
#endif
                }

                /* Cached for PP Pipe */
                stream->usage |= (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
#ifdef USE_SERVICE_BATCH_MODE
                enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
                int batchSize = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
                if (batchSize > 1 && m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                    CLOGD("Use service batch mode. Add HFR_MODE usage bit. batchSize %d", batchSize);
                    stream->usage |= GRALLOC1_PRODUCER_USAGE_HFR_MODE;
                }
#endif

                /* set Video Size and Recording Mode(Hint) */
                m_configurations->setSize(CONFIGURATION_VIDEO_SIZE, stream->width, stream->height);
                m_configurations->setMode(CONFIGURATION_RECORDING_MODE, true);

                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if((stream->usage & GRALLOC1_CONSUMER_USAGE_CAMERA) && (stream->usage & GRALLOC1_PRODUCER_USAGE_CAMERA)) {
                CLOGD("GRALLOC1_USAGE_CAMERA(ZSL) format(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                      stream->format, stream->usage, stream->stream_type, option);
                id = HAL_STREAM_ID_ZSL_OUTPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                outputPortId = 0;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_bayer_buffers;
            } else {
                CLOGE("HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x), stream_option(%#x)",
                    stream->usage, stream->format, stream->stream_type, option);
                ret = BAD_VALUE;
                goto func_err;
            }
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            if (option & STREAM_OPTION_STALL_MASK) {
                CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888_STALL format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_CALLBACK_STALL;
                outputPortId = m_streamManager->getTotalYuvStreamCount()
                               + ExynosCameraParameters::YUV_STALL_0;
                if (option & STREAM_OPTION_STITCHING) {
                    requestBuffer = m_exynosconfig->current->bufInfo.num_request_burst_capture_buffers;
                } else {
                    requestBuffer = m_exynosconfig->current->bufInfo.num_request_capture_buffers;
                }
                /* Cached for PP Pipe */
                stream->usage |= (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
            } else if (option & STREAM_OPTION_THUMBNAIL_CB_MASK) {
                CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888 THUMBNAIL_CALLBACK format (%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_THUMBNAIL_CALLBACK;
                outputPortId = 0;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_burst_capture_buffers;
            } else {
                CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888 format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);

                id = HAL_STREAM_ID_CALLBACK;
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_callback_buffers;
            }

            actualFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            planeCount = 1;
            break;
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
            CLOGD("HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x) stream_option(%#x)",
                stream->format, stream->usage, stream->stream_type, option);
#ifdef SUPPORT_DEPTH_MAP
            if (option & STREAM_OPTION_DEPTH10_MASK && option & STREAM_OPTION_STALL_MASK) {
                id = HAL_STREAM_ID_DEPTHMAP_STALL;
                stream->usage |= GRALLOC_USAGE_SW_READ_OFTEN;   /* Cached & NeedSync */
                requestBuffer = NUM_DEPTHMAP_BUFFERS;
            } else if (option & STREAM_OPTION_DEPTH10_MASK) {
                id = HAL_STREAM_ID_DEPTHMAP;
                requestBuffer = NUM_DEPTHMAP_BUFFERS;
            } else
#endif
            {
                id = HAL_STREAM_ID_RAW;
                stream->usage |= GRALLOC_USAGE_SW_READ_OFTEN;   /* Cached & NeedSync */
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            }
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            outputPortId = 0;
            break;
#if 0
#ifdef SUPPORT_DEPTH_MAP
        case HAL_PIXEL_FORMAT_Y16:
            CLOGD("HAL_PIXEL_FORMAT_Y16 format(%#x) usage(%#x) stream_type(%#x) stream_option(%#x)",
                stream->format, stream->usage, stream->stream_type, option);
            if (option & STREAM_OPTION_STALL_MASK) {
                id = HAL_STREAM_ID_DEPTHMAP_STALL;
                stream->usage |= GRALLOC_USAGE_SW_READ_OFTEN;   /* Cached & NeedSync */
            } else {
                id = HAL_STREAM_ID_DEPTHMAP;
            }
            actualFormat = HAL_PIXEL_FORMAT_Y16;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
#endif
#endif
        case HAL_PIXEL_FORMAT_Y8:
            CLOGD("HAL_PIXEL_FORMAT_Y8 format(%#x) usage(%#x) stream_type(%#x) stream_option(%#x)",
                stream->format, stream->usage, stream->stream_type, option);
            id = HAL_STREAM_ID_VISION;
            actualFormat = HAL_PIXEL_FORMAT_Y8;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = RESERVED_NUM_SECURE_BUFFERS;
            break;
        default:
            CLOGE("Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            ret = BAD_VALUE;
            goto func_err;
            break;
        }
        break;
    case CAMERA3_STREAM_INPUT:
    case CAMERA3_STREAM_BIDIRECTIONAL:
        ret = m_streamManager->increaseInputStreamCount(m_parameters[m_cameraId]->getInputStreamMaxNum());
        if (ret != NO_ERROR) {
            CLOGE("can not support more than %d input streams", m_parameters[m_cameraId]->getInputStreamMaxNum());
            goto func_err;
        }

        switch (stream->format) {
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
        case HAL_PIXEL_FORMAT_RAW_OPAQUE:
            CLOGD("HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            planeCount = 1;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            CLOGD("GRALLOC1_USAGE_CAMERA(ZSL) foramt(%#x) usage(%#x) stream_type(%#x)",
                stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_ZSL_INPUT;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;

            /* Add camera usage to identify ZSL stream for Gralloc */
            stream->usage |= (GRALLOC1_CONSUMER_USAGE_CAMERA | GRALLOC1_PRODUCER_USAGE_CAMERA);

            planeCount = 1;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
        default:
            CLOGE("Not supported image format(%#x) usage(%#x) stream_type(%#x)",
                 stream->format, stream->usage, stream->stream_type);
            goto func_err;
        }
        break;
    default:
        CLOGE("Unknown stream_type(%#x) format(%#x) usage(%#x)",
             stream->stream_type, stream->format, stream->usage);
        ret = BAD_VALUE;
        goto func_err;
    }

    /* Update gralloc usage */
    switch (stream->stream_type) {
    case CAMERA3_STREAM_INPUT:
        stream->usage |= GRALLOC1_CONSUMER_USAGE_CAMERA;
        break;
    case CAMERA3_STREAM_OUTPUT:
        stream->usage |= GRALLOC1_PRODUCER_USAGE_CAMERA;
        break;
    case CAMERA3_STREAM_BIDIRECTIONAL:
        stream->usage |= (GRALLOC1_CONSUMER_USAGE_CAMERA
                          | GRALLOC1_PRODUCER_USAGE_CAMERA);
        break;
    default:
        CLOGE("Invalid stream_type %d", stream->stream_type);
        break;
    }

    ret = m_streamManager->createStream(id, stream, &newStream);
    if (ret != NO_ERROR) {
        CLOGE("createStream is NULL id(%d)", id);
        ret = BAD_VALUE;
        goto func_err;
    }

    newStream->setRegisterStream(registerStream);
    newStream->setRegisterBuffer(registerBuffer);
    newStream->setFormat(actualFormat);
    newStream->setPlaneCount(planeCount);
    newStream->setOutputPortId(outputPortId);
    newStream->setRequestBuffer(requestBuffer);

func_err:
    CLOGD("Out");

    return ret;

}

status_t ExynosCamera::configureStreams(camera3_stream_configuration *stream_list)
{
    status_t ret = NO_ERROR;
    EXYNOS_STREAM::STATE registerStreamState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    EXYNOS_STREAM::STATE registerbufferState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
    int width = 0, height = 0;
    int hwWidth = 0, hwHeight = 0;
    int streamPlaneCount = 0;
    int streamPixelFormat = 0;
    int outputPortId = 0;
    bool isUseDynamicBayer = true;
    bool needMmap = false;
    m_flushLockWait = false;

    CLOGD("In");
    m_flagFirstPreviewTimerOn = false;

#ifdef FPS_CHECK
    for (int i = 0; i < sizeof(m_debugFpsCount); i++)
        m_debugFpsCount[i] = 0;
#endif

    if (m_flagFirstPreviewTimerOn == false) {
        m_firstPreviewTimer.start();
        m_flagFirstPreviewTimerOn = true;

        CLOGD("m_firstPreviewTimer start");
    }
    /* sanity check for stream_list */
    if (stream_list == NULL) {
        CLOGE("NULL stream configuration");
        return BAD_VALUE;
    }

    if (stream_list->streams == NULL) {
        CLOGE("NULL stream list");
        return BAD_VALUE;
    }

    if (stream_list->num_streams < 1) {
        CLOGE("Bad number of streams requested: %d",
             stream_list->num_streams);
        return BAD_VALUE;
    }

    /* HACK :: restart frame factory */
    if (m_getState() > EXYNOS_CAMERA_STATE_INITIALIZE) {
        CLOGI("restart frame factory. state(%d)", m_getState());

        /* In case of preview with Recording, enter this block even if not restart */
        m_recordingEnabled = false;

        if (m_getState() != EXYNOS_CAMERA_STATE_CONFIGURED) {
            ret = flush();
            if (ret != NO_ERROR) {
                CLOGE("Failed to flush. ret %d", ret);
                return ret;
            }
        }

        /* clear frame lists */
        m_removeInternalFrames(&m_processList, &m_processLock);
        ret = m_clearList(&m_captureProcessList, &m_captureProcessLock);
        if (ret != NO_ERROR) {
            CLOGE("Failed to clearList(m_captureProcessList). ret %d", ret);
            /* continue */
        }

        if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL) {
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->isCreated() == true) {
                ret = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->destroy();
                if (ret < 0)
                    CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_REPROCESSING);
            }
            SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]);

            CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_REPROCESSING);
        }

        if (m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] != NULL) {
                if (m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING]->isCreated() == true) {
                        ret = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING]->destroy();
                        if (ret < 0)
                                CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_JPEG_REPROCESSING);
                }
                SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING]);

                CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_JPEG_REPROCESSING);
        }

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
            && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] != NULL) {
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]->isCreated() == true) {
                ret = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]->destroy();
                if (ret < 0)
                    CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_REPROCESSING_DUAL);
            }
            SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]);

            CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_REPROCESSING_DUAL);

            m_configurations->setDualOperationMode(DUAL_OPERATION_MODE_NONE);
        }
#endif

        /* restart frame manager */
        m_frameMgr->stop();
        m_frameMgr->deleteAllFrame();
        m_frameMgr->start();

        m_setBuffersThread->join();

        m_bufferSupplier->deinit();
    }

    m_reInit();

    ret = m_configurations->reInit();
    if (ret != NO_ERROR) {
        CLOGE("Failed to reInit Configurations. ret %d", ret);
        return BAD_VALUE;
    }

    ret = m_parameters[m_cameraId]->reInit();
    if (ret != NO_ERROR) {
        CLOGE("initMetadata() failed!! status(%d)", ret);
        return BAD_VALUE;
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_parameters[m_cameraIds[1]] != NULL) {
        ret = m_parameters[m_cameraIds[1]]->reInit();
        if (ret != NO_ERROR) {
            CLOGE("initMetadata() failed!! status(%d)", ret);
            return BAD_VALUE;
        }

#ifdef DUAL_DYNAMIC_HW_SYNC
        switch (m_scenario) {
        case SCENARIO_DUAL_REAR_PORTRAIT:
        case SCENARIO_DUAL_FRONT_PORTRAIT:
            m_configurations->setDualHwSyncOn(false);
            break;
        default:
            m_configurations->setDualHwSyncOn(true);
            break;
        }
        CLOGD("dualHwSyncOn(%d)", m_configurations->getDualHwSyncOn());
#endif
    }

#endif

    ret = m_setStreamInfo(stream_list);
    if (ret) {
        CLOGE("setStreams() failed!!");
        return ret;
    }

    ret = m_constructFrameFactory();
    if (ret != NO_ERROR) {
        CLOGE("Failed to constructFrameFactory. ret %d", ret);
        return ret;
    }

    if (m_frameFactoryQ->getSizeOfProcessQ() > 0) {
        m_framefactoryCreateThread->run();
    }

    m_requestMgr->clearFrameFactory();

    ret = m_setupFrameFactoryToRequest();
    if (ret != NO_ERROR) {
        CLOGE("Failed to setupFrameFactoryToRequest. ret %d", ret);
        return ret;
    }

    /* clear previous settings */
    ret = m_requestMgr->clearPrevRequest();
    if (ret) {
        CLOGE("clearPrevRequest() failed!!");
        return ret;
    }

    m_requestMgr->resetResultRenew();

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, STREAM_BUFFER_ALLOC_START, 0);

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
        && m_videoStreamExist == true) {
        int previewW = 0, previewH = 0;
        int recordingW = 0, recordingH = 0;
        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&recordingW, (uint32_t *)&recordingH);
        m_configurations->getSize(CONFIGURATION_PREVIEW_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH);
        if (recordingW > previewW || recordingH > previewH) {
            m_flagVideoStreamPriority = true;
        }
    }
#endif

#ifdef USES_SW_VDIS
    if (m_exCameraSolutionSWVdis != NULL) {
        m_exCameraSolutionSWVdis->checkMode();
    }
#endif

    /* Create service buffer manager at each stream */
    for (size_t i = 0; i < stream_list->num_streams; i++) {
        buffer_manager_tag_t bufTag;
        buffer_manager_configuration_t bufConfig;
        registerStreamState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
        registerbufferState = EXYNOS_STREAM::HAL_STREAM_STS_INIT;
        int startIndex = 0;
        int maxBufferCount = 0;
        int id = -1;

        camera3_stream_t *newStream = stream_list->streams[i];
        ExynosCameraStream *privStreamInfo = static_cast<ExynosCameraStream*>(newStream->priv);
        privStreamInfo->getID(&id);
        width = newStream->width;
        height = newStream->height;
        bufConfig.needMmap = (newStream->usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN));

        CLOGI("list_index %zu streamId %d", i, id);
        CLOGD("stream_type %d", newStream->stream_type);
        CLOGD("size %dx%d", width, height);
        CLOGD("format %x", newStream->format);
        CLOGD("usage %x", newStream->usage);
        CLOGD("max_buffers %d", newStream->max_buffers);
        CLOGD("needMmap %d", bufConfig.needMmap);

        privStreamInfo->getRegisterStream(&registerStreamState);
        privStreamInfo->getRegisterBuffer(&registerbufferState);
        privStreamInfo->getPlaneCount(&streamPlaneCount);
        privStreamInfo->getFormat(&streamPixelFormat);

        if (registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_INVALID) {
            CLOGE("Invalid stream index %zu id %d", i, id);
            return BAD_VALUE;
        }
        if (registerbufferState != EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED) {
            CLOGE("privStreamInfo->registerBuffer state error!!");
            return BAD_VALUE;
        }

        CLOGD("registerStream %d registerbuffer %d", registerStreamState, registerbufferState);

        if ((registerStreamState == EXYNOS_STREAM::HAL_STREAM_STS_VALID) &&
            (registerbufferState == EXYNOS_STREAM::HAL_STREAM_STS_UNREGISTERED)) {
            switch (id % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
                m_parameters[m_cameraId]->setUseSensorPackedBayer(false);

                /* set startIndex as the next internal buffer index */
                maxBufferCount = m_exynosconfig->current->bufInfo.num_request_raw_buffers;

                bufTag.pipeId[0] = PIPE_3AC_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(RAW)");
                ret = m_bufferSupplier->createBufferManager("RAW_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create RAW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc RAW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
                if (m_parameters[m_cameraId]->isUseRawReverseReprocessing() == true) {
                    bufTag.pipeId[0] = PIPE_3AC_REPROCESSING;
                    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

                    CLOGD("Create buffer manager(RAW_INTERNAL)");
                    ret = m_bufferSupplier->createBufferManager("RAW_INTERNAL_BUF", m_ionAllocator, bufTag);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to create RAW_INTERNAL_STREAM_BUF. ret %d", ret);
                        return ret;
                    }

                    /* same with raw buffer information except for below setting */
                    bufConfig.startBufIndex = 0;
#ifdef CAMERA_PACKED_BAYER_ENABLE
                    bufConfig.bytesPerLine[0] = getBayerLineSize(width, m_parameters[m_cameraId]->getBayerFormat(PIPE_3AC_REPROCESSING));
                    bufConfig.size[0] = bufConfig.bytesPerLine[0] * height;
#endif
                    bufConfig.reqBufCount = 2;
                    bufConfig.allowedMaxBufCount = 2;
                    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;

                    CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                            streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                    ret = m_allocBuffers(bufTag, bufConfig);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to alloc RAW_INTERNAL_BUF. ret %d", ret);
                        return ret;
                    }
                }
#endif

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_ZSL_OUTPUT:
                /* set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers - startIndex;

                if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
                    bufTag.pipeId[0] = PIPE_VC0;
                } else {
                    bufTag.pipeId[0] = PIPE_3AC;
                }

                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(ZSL_OUT)");
                ret = m_bufferSupplier->createBufferManager("ZSL_OUT_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create ZSL_OUT_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc ZSL_OUT_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_ZSL_INPUT:
                /* Check support for ZSL input */
                if(m_parameters[m_cameraId]->isSupportZSLInput() == false) {
                    CLOGE("ZSL input is not supported, but streamID [%d] is specified.", id);
                    return INVALID_OPERATION;
                }

                /* Set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers - startIndex;

                if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
                    bufTag.pipeId[0] = PIPE_3AA_REPROCESSING;
                } else {
                    bufTag.pipeId[0] = PIPE_ISP_REPROCESSING;
                }

                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(ZSL_INPUT)");
                ret =  m_bufferSupplier->createBufferManager("ZSL_INPUT_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create ZSL_INPUT_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc ZSL_INPUT_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_PREVIEW:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for PREVIEW stream");
                    return ret;
                }

                m_parameters[m_cameraId]->setPreviewPortId(outputPortId);

                ret = m_parameters[m_cameraId]->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                hwWidth = width;
                hwHeight = height;

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                {
                    maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
                }

                ret = m_configurations->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for PREVIEW stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
                    if (m_parameters[m_cameraIds[1]] != NULL) {
                        m_parameters[m_cameraIds[1]]->setPreviewPortId(outputPortId);

                        ret = m_parameters[m_cameraIds[1]]->checkYuvSize(width, height, outputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to checkYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                    width, height, outputPortId);
                            return ret;
                        }

                        hwWidth = width;
                        hwHeight = height;

                        ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to setHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                     hwWidth, hwHeight, outputPortId);
                            return ret;
                        }
                    }
                }
#endif

                needMmap = bufConfig.needMmap;

                {
#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        if (m_flagVideoStreamPriority == true) {
                            bufTag.pipeId[0] = PIPE_GSC;
                        } else {
                            bufTag.pipeId[0] = PIPE_FUSION;
                        }
                    } else
#endif
                    {
                        bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                           + PIPE_MCSC0;
                    }
                }
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(PREVIEW)");
                ret = m_bufferSupplier->createBufferManager("PREVIEW_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create PREVIEW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height;
                bufConfig.size[1] = width * height / 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.needMmap = needMmap;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, planeSize[1] = %d, \
                        bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.size[1],
                        bufConfig.bytesPerLine[0], outputPortId);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc PREVIEW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_VIDEO:
                CLOGD("setRecordingHint");

                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for VIDEO stream");
                    return ret;
                }

                m_parameters[m_cameraId]->setRecordingPortId(outputPortId);
                m_parameters[m_cameraId]->setPreviewPortId(outputPortId);

                ret = m_parameters[m_cameraId]->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                hwWidth = width;
                hwHeight = height;

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for VIDEO stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_configurations->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for VIDEO stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                    && m_parameters[m_cameraIds[1]] != NULL) {
                    m_parameters[m_cameraIds[1]]->setRecordingPortId(outputPortId);

                    ret = m_parameters[m_cameraIds[1]]->checkYuvSize(width, height, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                                 width, height, outputPortId);
                        return ret;
                    }

                    hwWidth = width;
                    hwHeight = height;

                    ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setHwYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, outputPortId);
                        return ret;
                    }
                }
#endif

#ifdef USES_SW_VDIS
                if (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true) {
                    if (m_exCameraSolutionSWVdis != NULL) {
                        m_exCameraSolutionSWVdis->configureStream();
                    }
                }
#endif
                if (m_configurations->isSupportedFunction(SUPPORTED_FUNCTION_GDC) == true) {
                    bufTag.pipeId[0] = PIPE_GDC;
                } else {
                    {
#ifdef USE_DUAL_CAMERA
                        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                            if (m_flagVideoStreamPriority == true) {
                                bufTag.pipeId[0] = PIPE_FUSION;
                            } else {
                                bufTag.pipeId[0] = PIPE_GSC;
                            }
                        } else
#endif
#ifdef USES_SW_VDIS
                        if (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true) {
                            if (m_exCameraSolutionSWVdis != NULL) {
                                bufTag.pipeId[0] = m_exCameraSolutionSWVdis->getPipeId();
                            }
                        } else
#endif
                        {
                            bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                               + PIPE_MCSC0;
                        }
                    }
                }
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(VIDEO)");
                ret =  m_bufferSupplier->createBufferManager("VIDEO_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height;
                bufConfig.size[1] = width * height / 2;
                if (m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true) {
                    bufConfig.size[2] = EXYNOS_CAMERA_YSUM_PLANE_SIZE;
                } else {
#ifdef GRALLOC_ALWAYS_ALLOC_VIDEO_META
                    bufConfig.size[2] = EXYNOS_CAMERA_YSUM_PLANE_SIZE;
#else
                    bufConfig.size[2] = 0;
#endif
                }
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;
#ifdef USES_SW_VDIS
                bufConfig.needMmap = needMmap;
#endif

                CLOGD("planeCount = %d+1, planeSize[0] = %d, planeSize[1] = %d, planeSize[2] = %d, \
                        bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.size[1], bufConfig.size[2],
                        bufConfig.bytesPerLine[0], outputPortId);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_PREVIEW_VIDEO:
//                m_parameters[m_cameraId]->setVideoSize(width, height);
//                m_parameters[m_cameraId]->setRecordingHint(true);
                CLOGD("setRecordingHint");

                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for PREVIEW_VIDEO stream");
                    return ret;
                }

                m_parameters[m_cameraId]->setRecordingPortId(outputPortId);
                m_parameters[m_cameraId]->setPreviewPortId(outputPortId);

                ret = m_parameters[m_cameraId]->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for PREVIEW_VIDEO stream. size %dx%d outputPortId %d",
                            width, height, outputPortId);
                    return ret;
                }

                hwWidth = width;
                hwHeight = height;

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkHwYuvSize for PREVIEW_VIDEO stream. size %dx%d outputPortId %d",
                            hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW_VIDEO stream. format %x outputPortId %d",
                            streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_configurations->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for PREVIEW_VIDEO stream. maxBufferCount %d outputPortId %d",
                            maxBufferCount, outputPortId);
                    return ret;
                }

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                    + PIPE_MCSC0;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(PREVIEW_VIDEO)");
                ret =  m_bufferSupplier->createBufferManager("PREVIEW_VIDEO_STREAM_BUF", m_ionAllocator, bufTag, newStream);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create PREVIEW_VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height;
                bufConfig.size[1] = width * height / 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, planeSize[1] = %d, \
                        bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.size[1],
                        bufConfig.bytesPerLine[0], outputPortId);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc PREVIEW_VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);

                break;
            case HAL_STREAM_ID_JPEG:
                ret = m_parameters[m_cameraId]->checkPictureSize(width, height);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkPictureSize for JPEG stream. size %dx%d",
                             width, height);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                    && m_parameters[m_cameraIds[1]] != NULL) {
                    ret = m_parameters[m_cameraIds[1]]->checkPictureSize(width, height);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setHwYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, outputPortId);
                        return ret;
                    }
                }
#endif

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                bufTag.pipeId[0] = PIPE_JPEG;
                bufTag.pipeId[1] = PIPE_JPEG_REPROCESSING;
                bufTag.pipeId[2] = PIPE_HWFC_JPEG_DST_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(JPEG)");
                ret = m_bufferSupplier->createBufferManager("JPEG_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create JPEG_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;

                {
                    bufConfig.size[0] = width * height * 2;
                }

                {
                    bufConfig.reqBufCount = maxBufferCount;
                    bufConfig.allowedMaxBufCount = maxBufferCount;
                }
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[1]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc JPEG_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_CALLBACK:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for CALLBACK stream");
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvSize(width, height, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                             width, height, outputPortId);
                    return ret;
                }

                hwWidth = width;
                hwHeight = height;

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_cb_buffers;
                ret = m_configurations->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                    && m_parameters[m_cameraIds[1]] != NULL) {
                    ret = m_parameters[m_cameraIds[1]]->checkYuvSize(width, height, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                                width, height, outputPortId);
                        return ret;
                    }

                    hwWidth = width;
                    hwHeight = height;

                    ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setHwYuvSize for CALLBACK stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, outputPortId);
                        return ret;
                    }
                }
#endif

                /* Set reprocessing stream YUV size for ZSL_INPUT */
                {
                    int stallOutputPortId = ExynosCameraParameters::YUV_STALL_0 + outputPortId;

                    ret = m_parameters[m_cameraId]->checkYuvSize(width, height, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvSize for CALLBACK_ZSL stream. size %dx%d outputPortId %d",
                                width, height, stallOutputPortId);
                        return ret;
                    }

                    hwWidth = width;
                    hwHeight = height;

                    ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setHwYuvSize for CALLBACK_ZSL stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, stallOutputPortId);
                        return ret;
                    }

                    ret = m_configurations->checkYuvFormat(streamPixelFormat, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvFormat for CALLBACK_ZSL stream. format %x outputPortId %d",
                                streamPixelFormat, stallOutputPortId);
                        return ret;
                    }

                    ret = m_configurations->setYuvBufferCount(maxBufferCount, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferCount for CALLBACK_ZSL stream. maxBufferCount %d outputPortId %d",
                                maxBufferCount, stallOutputPortId);
                        return ret;
                    }

#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                        && m_parameters[m_cameraIds[1]] != NULL) {
                        ret = m_parameters[m_cameraIds[1]]->checkYuvSize(width, height, stallOutputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to checkYuvSize for CALLBACK_ZSL stream. size %dx%d outputPortId %d",
                                    width, height, stallOutputPortId);
                            return ret;
                        }

                        ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, stallOutputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to setHwYuvSize for CALLBACK_ZSL stream. size %dx%d outputPortId %d",
                                    hwWidth, hwHeight, stallOutputPortId);
                            return ret;
                        }
                    }
#endif
                }

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0;
                bufTag.pipeId[1] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(PREVIEW_CB)");
                ret = m_bufferSupplier->createBufferManager("PREVIEW_CB_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create PREVIEW_CB_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = (width * height * 3) / 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.bytesPerLine[0], outputPortId);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc PREVIEW_CB_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_CALLBACK_STALL:
                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for CALLBACK_STALL stream");
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvSize(width, height, outputPortId, true);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkPictureSize for CALLBACK_STALL stream. size %dx%d",
                            width, height);
                    return ret;
                }

                hwWidth = width;
                hwHeight = height;

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for CALLBACK_STALL stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK_STALL stream. format %x outputPortId %d",
                            streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
                ret = m_configurations->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK_STALL stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
                    && m_parameters[m_cameraIds[1]] != NULL) {
                    ret = m_parameters[m_cameraIds[1]]->checkYuvSize(width, height, outputPortId, true);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkPictureSize for CALLBACK_STALL stream. size %dx%d",
                                width, height);
                        return ret;
                    }

                    ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setHwYuvSize for CALLBACK_STALL stream. size %dx%d outputPortId %d",
                                hwWidth, hwHeight, outputPortId);
                        return ret;
                    }
                }
#endif

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(CAPTURE_CB_STALL)");
                ret = m_bufferSupplier->createBufferManager("CAPTURE_CB_STALL_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create CAPTURE_CB_STALL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = (width * height * 3) / 2;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.bytesPerLine[0], outputPortId);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc CAPTURE_CB_STALL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
                /* Share CALLBACK_STALL port */
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                bufTag.pipeId[0] = PIPE_GSC_REPROCESSING2;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(THUMBNAIL)");
                ret = m_bufferSupplier->createBufferManager("THUMBNAIL_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create THUMBNAIL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount;
                bufConfig.size[0] = (width * height * 3) / 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = false;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc THUMBNAIL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
                m_parameters[m_cameraId]->setDepthMapSize(width, height);
                m_configurations->setMode(CONFIGURATION_DEPTH_MAP_MODE, true);

                maxBufferCount = NUM_DEPTHMAP_BUFFERS;

                bufTag.pipeId[0] = PIPE_VC1;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(DEPTH)");
                ret = m_bufferSupplier->createBufferManager("DEPTH_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create DEPTH_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc RAW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_DEPTHMAP_STALL:
                ret = m_setDepthInternalBuffer();
                if (ret != NO_ERROR) {
                    CLOGE("Failed to m_setDepthInternalBuffer. ret %d", ret);
                    return ret;
                }

                /* Set startIndex as the next internal buffer index */
                startIndex = NUM_DEPTHMAP_BUFFERS;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers - startIndex;

                bufTag.pipeId[0] = PIPE_VC1;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(DEPTH_STALL)");
                ret = m_bufferSupplier->createBufferManager("DEPTH_STALL_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create DEPTH_STALL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc DEPTH_STALL_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
#endif
            case HAL_STREAM_ID_VISION:
                /* set startIndex as the next internal buffer index */
                // startIndex = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                maxBufferCount = RESERVED_NUM_SECURE_BUFFERS; //m_exynosconfig->current->bufInfo.num_bayer_buffers - startIndex;

                bufTag.pipeId[0] = PIPE_VC0;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(VISION)");
                ret = m_bufferSupplier->createBufferManager("VISION_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create VISION_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height;
                bufConfig.startBufIndex = startIndex;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, bytesPerLine[0] = %d",
                        streamPlaneCount, bufConfig.size[0], bufConfig.bytesPerLine[0]);

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc VISION_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            default:
                CLOGE("privStreamInfo->id is invalid !! id(%d)", id);
                ret = BAD_VALUE;
                break;
            }
        }
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, STREAM_BUFFER_ALLOC_END, 0);

    {
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
            && m_flagVideoStreamPriority == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            int hwWidth = 0, hwHeight = 0;

            m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&hwWidth, (uint32_t *)&hwHeight);

            ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, portId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setHwYuvSize for PREVIEW stream(VDIS). size %dx%d outputPortId %d",
                         hwWidth, hwHeight, portId);
                return ret;
            }

            if (m_parameters[m_cameraIds[1]] != NULL) {
                portId = m_parameters[m_cameraIds[1]]->getPreviewPortId();
                ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, portId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for PREVIEW stream(VDIS). size %dx%d outputPortId %d",
                             hwWidth, hwHeight, portId);
                    return ret;
                }
            }
        }
#endif
    }

#ifdef USES_HIFI_LLS
    m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT,
                                   m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW)
                                   % ExynosCameraParameters::YUV_MAX);
#endif

    /*
     * NOTICE: Join is to avoid PIP scanario's problem.
     * The problem is that back camera's EOS was not finished, but front camera opened.
     * Two instance was actually different but driver accepts same instance.
     */
    if (m_framefactoryCreateThread->isRunning() == true) {
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_THREAD_JOIN_START, 0);
        m_framefactoryCreateThread->join();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_THREAD_JOIN_END, 0);
        if (m_framefactoryCreateResult != NO_ERROR) {
            CLOGE("Failed to create framefactory");
            m_transitState(EXYNOS_CAMERA_STATE_ERROR);
            return NO_INIT;
        }
    }

    m_parameters[m_cameraId]->updateBinningScaleRatio();

    /* assume that Stall stream and jpeg stream have the same size */
    if (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == false
        && m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true) {
        int yuvStallPort = (m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL) % ExynosCameraParameters::YUV_MAX) + ExynosCameraParameters::YUV_MAX;

        m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, yuvStallPort);

        ret = m_parameters[m_cameraId]->checkPictureSize(width, height);
        if (ret != NO_ERROR) {
            CLOGE("Failed to checkPictureSize for JPEG stream. size %dx%d",
                     width, height);
            return ret;
        }
    }

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
#ifdef USE_DUAL_CAMERA
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
        ) {
    }

    /* Check the validation of stream configuration */
    ret = m_checkStreamInfo();
    if (ret != NO_ERROR) {
        CLOGE("checkStremaInfo() failed!!");
        return ret;
    }

    m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height);
    CLOGD("MaxYuvSize:%dx%d ratio:%d", width, height, m_parameters[m_cameraId]->getYuvSizeRatioId());

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_parameters[m_cameraIds[1]] != NULL) {
        CLOGD("slave MaxYuvSize:%dx%d ratio:%d", width, height, m_parameters[m_cameraIds[1]]->getYuvSizeRatioId());
    }
#endif

#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_setupAdaptiveBuffer();
    if (ret != NO_ERROR) {
        CLOGE("m_setupAdaptiveBuffer() failed!!");
        return ret;
    }
#endif

    m_setBuffersThread->run(PRIORITY_DEFAULT);

    {
        /* HACK: Support samsung camera */
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
            && m_streamManager->findStream(HAL_STREAM_ID_VIDEO) == false
            && (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == true
                || m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true)) {
            isUseDynamicBayer = false;
        } else
#endif
        {
#ifdef USE_LLS_REPROCESSING
            if (m_parameters[m_cameraId]->getLLSOn() == true)
                isUseDynamicBayer = false;
            else
#endif
                isUseDynamicBayer = true;
        }
    }

#ifdef USE_SLSI_PLUGIN
    m_configurePlugInMode(isUseDynamicBayer);
#endif

    m_configurations->setMode(CONFIGURATION_DYNAMIC_BAYER_MODE, isUseDynamicBayer);

    ret = m_transitState(EXYNOS_CAMERA_STATE_CONFIGURED);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into CONFIGURED. ret %d", ret);
    }

    CLOGD("Out");
    return ret;
}

status_t ExynosCamera::m_updateTimestamp(ExynosCameraRequestSP_sprt_t request,
                                            ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *timestampBuffer)
{
    struct camera2_shot_ext *shot_ext = NULL;
    status_t ret = NO_ERROR;

    /* handle meta data */
    shot_ext = (struct camera2_shot_ext *) timestampBuffer->addr[timestampBuffer->getMetaPlaneIndex()];
    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL. framecount %d buffer %d",
                 frame->getFrameCount(), timestampBuffer->index);
        return INVALID_OPERATION;
    }

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    if (frame->getAdjustedTimestampFlag() == true ||
        (request != NULL && request->getSensorTimestamp() != 0)) {
        CLOGV("frame %d is already adjust timestamp", frame->getFrameCount());
    } else {
        uint64_t exposureTime = 0;
        uint64_t oldTimeStamp = 0;
        exposureTime = (uint64_t)shot_ext->shot.dm.sensor.exposureTime;

        oldTimeStamp = shot_ext->shot.udm.sensor.timeStampBoot;

        shot_ext->shot.udm.sensor.timeStampBoot -= (exposureTime);
#ifdef CORRECTION_SENSORFUSION
        shot_ext->shot.udm.sensor.timeStampBoot += CORRECTION_SENSORFUSION;
#endif

        CLOGV("[F%d] exp.time(%ju) timeStamp(%ju -> %ju)",
            frame->getFrameCount(), exposureTime,
            oldTimeStamp, shot_ext->shot.udm.sensor.timeStampBoot);

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE
            && frame->getFrameType() != FRAME_TYPE_REPROCESSING_DUAL_SLAVE)
#endif
        {
            if (request != NULL) {
                request->setSensorTimestamp(shot_ext->shot.udm.sensor.timeStampBoot);
            }
        }

        frame->setAdjustedTimestampFlag(true);
    }
#endif

    /* HACK: W/A for timeStamp reversion */
    if (shot_ext->shot.udm.sensor.timeStampBoot < m_lastFrametime) {
        CLOGV("Timestamp is lower than lastFrametime!"
            "frameCount(%d) shot_ext.timestamp(%ju) m_lastFrametime(%ju)",
            frame->getFrameCount(), shot_ext->shot.udm.sensor.timeStampBoot, m_lastFrametime);
        //shot_ext->shot.udm.sensor.timeStampBoot = m_lastFrametime + 15000000;
    }
    m_lastFrametime = shot_ext->shot.udm.sensor.timeStampBoot;

    return ret;
}

status_t ExynosCamera::m_previewFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraFrameFactory *targetfactory,
                                             frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    frame_handle_components_t components;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext *service_shot_ext = NULL;
    uint32_t requestKey = 0;
    bool captureFlag = false;
    bool zslInputFlag = false;
    bool rawStreamFlag = false;
    bool zslStreamFlag = false;
    bool depthStreamFlag = false;
    bool needDynamicBayer = false;
    bool usePureBayer = false;
    bool requestVC0 = false;
    bool request3AP = false;
    bool requestGMV = false;
    bool requestMCSC = false;
    int32_t reprocessingBayerMode = REPROCESSING_BAYER_MODE_NONE;
    enum pipeline controlPipeId = MAX_PIPE_NUM;
    int pipeId = -1;
    int ysumPortId = -1;
    int dstPos = 0;
    uint32_t streamConfigBit = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    bool flag3aaVraM2M = false;
    camera3_stream_buffer_t *inputBuffer = request->getInputBuffer();
    ExynosRect bayerCropRegion = {0, };
    requestKey = request->getKey();
    ExynosRect zoomRect = {0, };
    ExynosRect activeZoomRect = {0, };

    getFrameHandleComponents(frameType, &components);

    requestVC0 = (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    request3AP = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    requestGMV = m_configurations->getMode(CONFIGURATION_GMV_MODE);
    requestMCSC = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
    flag3aaVraM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_VRA) == HW_CONNECTION_MODE_M2M);
    reprocessingBayerMode = components.parameters->getReprocessingBayerMode();
    controlPipeId = (enum pipeline) components.parameters->getPerFrameControlPipe();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_VC0, requestVC0);
    targetfactory->setRequest(PIPE_3AA, requestVC0);
    targetfactory->setRequest(PIPE_3AC, false);
    targetfactory->setRequest(PIPE_3AP, request3AP);
    targetfactory->setRequest(PIPE_3AF, false);
    targetfactory->setRequest(PIPE_ME, requestGMV);
    targetfactory->setRequest(PIPE_GMV, requestGMV);
    targetfactory->setRequest(PIPE_ISP, request3AP);
    targetfactory->setRequest(PIPE_ISPC, false);
    targetfactory->setRequest(PIPE_MCSC, requestMCSC);
    targetfactory->setRequest(PIPE_MCSC0, false);
    targetfactory->setRequest(PIPE_MCSC1, false);
    targetfactory->setRequest(PIPE_MCSC2, false);
#ifdef SUPPORT_HW_GDC
    targetfactory->setRequest(PIPE_GDC, false);
#endif
    targetfactory->setRequest(PIPE_MCSC5, false);
    targetfactory->setRequest(PIPE_GSC, false);
    targetfactory->setRequest(PIPE_VRA, false);
    targetfactory->setRequest(PIPE_HFD, false);
#ifdef SUPPORT_DEPTH_MAP
    targetfactory->setRequest(PIPE_VC1, m_flagUseInternalDepthMap);
#endif
#ifdef SUPPORT_ME
    if (m_parameters[m_cameraId]->getLLSOn() == true)
        targetfactory->setRequest(PIPE_ME, true);
    else
        targetfactory->setRequest(PIPE_ME, false);
#endif
#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
        enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();

        targetfactory->setRequest(PIPE_SYNC, false);
        targetfactory->setRequest(PIPE_FUSION, false);


        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW
                   && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            targetfactory->setRequest(PIPE_SYNC, true);
            {
                int slaveOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
                slaveOutputPortId = (slaveOutputPortId % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;

                switch(frameType) {
                case FRAME_TYPE_PREVIEW_DUAL_MASTER:
                    targetfactory->setRequest(PIPE_FUSION, true);
                    break;
                case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
                    targetfactory->setRequest(slaveOutputPortId, true);
                    break;
                default:
                    targetfactory->setRequest(PIPE_FUSION, true);
                    break;
                }
            }
        }
    }
#endif

    /* To decide the dynamic bayer request flag for JPEG capture */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
        needDynamicBayer = false;
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
        targetfactory->setRequest(PIPE_VC0, true);
        needDynamicBayer = false;
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        targetfactory->setRequest(PIPE_3AC, true);
        needDynamicBayer = false;
        usePureBayer = false;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        needDynamicBayer = true;
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        needDynamicBayer = true;
        usePureBayer = false;
        break;
    default :
        break;
    }

    /* Check ZSL_INPUT stream */
    if(inputBuffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(inputBuffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            SET_STREAM_CONFIG_BIT(streamConfigBit, inputStreamId);

#ifdef USE_DUAL_CAMERA
            if (frameType != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                switch (inputStreamId % HAL_STREAM_ID_MAX) {
                case HAL_STREAM_ID_ZSL_INPUT:
                    zslInputFlag = true;
                    break;
                case HAL_STREAM_ID_JPEG:
                case HAL_STREAM_ID_PREVIEW:
                case HAL_STREAM_ID_VIDEO:
                case HAL_STREAM_ID_CALLBACK:
                case HAL_STREAM_ID_CALLBACK_STALL:
                case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
                case HAL_STREAM_ID_DEPTHMAP:
                case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                case HAL_STREAM_ID_VISION:
                    CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                            request->getKey(), inputStreamId);
                    break;
                default:
                    break;
                }
            }
        } else {
            CLOGE("Stream is null (%d)", request->getKey());
        }
    }

    /* Setting DMA-out request flag based on stream configuration */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamIdwithBufferIdx(i);
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

#ifdef USE_DUAL_CAMERA
        if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
            continue;
        }
#endif

        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_RAW:
            CLOGV("requestKey %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_RAW)",
                     request->getKey(), i);
            targetfactory->setRequest(PIPE_VC0, true);
            rawStreamFlag = true;
            break;
        case HAL_STREAM_ID_ZSL_OUTPUT:
            CLOGV("request(%d) outputBuffer-Index(%zu) buffer-StreamType(HAL_STREAM_ID_ZSL)",
                     request->getKey(), i);
            if (usePureBayer == true)
                targetfactory->setRequest(PIPE_VC0, true);
            else
                targetfactory->setRequest(PIPE_3AC, true);
            zslStreamFlag = true;
            break;
        case HAL_STREAM_ID_PREVIEW:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_PREVIEW)",
                     request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            break;
        case HAL_STREAM_ID_CALLBACK:
            if (zslInputFlag == true) {
                /* If there is ZSL_INPUT stream buffer,
                * It will be processed through reprocessing stream.
                */
                break;
            }
#if defined(USE_DUAL_CAMERA)
            if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                break;
            } else
#endif
            {
                CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
                         request->getKey(), i);
                pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                        + PIPE_MCSC0;
                targetfactory->setRequest(pipeId, true);
            }
            break;
        case HAL_STREAM_ID_VIDEO:
            m_recordingEnabled = true;
            {
#if defined(USE_DUAL_CAMERA)
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    break;
                } else
#endif
                {
                    CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VIDEO)",
                             request->getKey(), i);
                    pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                            + PIPE_MCSC0;
                    targetfactory->setRequest(pipeId, true);
                    ysumPortId = m_streamManager->getOutputPortId(id);
#ifdef SUPPORT_HW_GDC
                    if (components.parameters->getGDCEnabledMode() == true) {
                        targetfactory->setRequest(PIPE_GDC, true);
                        m_previewStreamGDCThread->run(PRIORITY_URGENT_DISPLAY);
                    }
#endif
                }
            }
            break;
        case HAL_STREAM_ID_PREVIEW_VIDEO:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_PREVIEW_VIDEO)",
                    request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            m_recordingEnabled = true;
            break;
        case HAL_STREAM_ID_JPEG:
            CLOGD("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_JPEG)",
                     request->getKey(), i);
            if (needDynamicBayer == true) {
                if(components.parameters->getUsePureBayerReprocessing()) {
                    targetfactory->setRequest(PIPE_VC0, true);
                } else {
                    targetfactory->setRequest(PIPE_3AC, true);
                }
            }
            captureFlag = true;
            break;
        case HAL_STREAM_ID_CALLBACK_STALL:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
                     request->getKey(), i);
            if (needDynamicBayer == true) {
                if(components.parameters->getUsePureBayerReprocessing()) {
                    targetfactory->setRequest(PIPE_VC0, true);
                } else {
                    targetfactory->setRequest(PIPE_3AC, true);
                }
            }
            captureFlag = true;
            break;
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_THUMBNAIL_CALLBACK)",
                    request->getKey(), i);
            break;
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_DEPTHMAP)",
                    request->getKey(), i);
            targetfactory->setRequest(PIPE_VC1, true);
            depthStreamFlag = true;
            break;
        case HAL_STREAM_ID_DEPTHMAP_STALL:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_DEPTHMAP_STALL)",
                    request->getKey(), i);
            break;
#endif
        case HAL_STREAM_ID_VISION:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VISION)",
                    request->getKey(), i);
            break;
        default:
            CLOGE("Invalid stream ID %d", id);
            break;
        }
    }
#ifdef USES_SW_VDIS
    targetfactory->setRequest(PIPE_ME, m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE));
    targetfactory->setRequest(PIPE_VDIS, m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE));
#endif

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        goto GENERATE_FRAME;
    }
#endif

    if (m_needDynamicBayerCount > 0) {
        m_needDynamicBayerCount--;
        if(components.parameters->getUsePureBayerReprocessing()) {
            targetfactory->setRequest(PIPE_VC0, true);
        } else {
            targetfactory->setRequest(PIPE_3AC, true);
        }
    }

    m_previewFrameHandler_vendor_updateRequest(targetfactory);

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()) {
        targetfactory->setRequest(PIPE_VC0, true);
    }
#endif
    m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT,
                                   m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW)
                                   % ExynosCameraParameters::YUV_MAX);

#ifdef USE_DUAL_CAMERA
GENERATE_FRAME:
#endif
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        ret = INVALID_OPERATION;
        return ret;
    }

    Mutex::Autolock l(m_currentPreviewShotLock);

    if ((m_recordingEnabled == true) && (ysumPortId < 0) && (request->hasStream(HAL_STREAM_ID_PREVIEW))) {
        /* The buffer from the preview port is used as the recording buffer */
        components.parameters->setYsumPordId(components.parameters->getPreviewPortId(), m_currentPreviewShot);
    } else {
        components.parameters->setYsumPordId(ysumPortId, m_currentPreviewShot);
    }

    m_updateCropRegion(m_currentPreviewShot);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentPreviewShot, service_shot_ext->shot.ctl.stats.faceDetectMode,
                        request->getDsInputPortId(), false, flag3aaVraM2M);
    }
    components.parameters->setDsInputPortId(m_currentPreviewShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], false);
    m_updateEdgeNoiseMode(m_currentPreviewShot, false);
    m_updateSetfile(m_currentPreviewShot, false);
    m_updateMasterCam(m_currentPreviewShot);

    if (captureFlag == true || rawStreamFlag == true) {
        m_updateExposureTime(m_currentPreviewShot);
    }

    if (m_currentPreviewShot->fd_bypass == false) {
        if (flag3aaVraM2M) {
            targetfactory->setRequest(PIPE_3AF, true);
            targetfactory->setRequest(PIPE_VRA, true);
            /* TODO: need to check HFD */
            } else if (components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA)
                                            == HW_CONNECTION_MODE_M2M) {
                targetfactory->setRequest(PIPE_MCSC5, true);
                targetfactory->setRequest(PIPE_VRA, true);
#ifdef SUPPORT_HFD
            if (m_currentPreviewShot->hfd.hfd_enable == true) {
                targetfactory->setRequest(PIPE_HFD, true);
            }
#endif
        }
    }

    /* Set framecount into request */
    m_frameCountLock.lock();
    if (request->getFrameCount() == 0) {
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
    }
    m_frameCountLock.unlock();

    /* Generate frame for YUV stream */
    ret = m_generateFrame(targetfactory, &m_processList, &m_processLock, newFrame, request);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to generateRequestFrame.", m_internalFrameCount - 1);
        goto CLEAN;
    } else if (newFrame == NULL) {
        CLOGE("[F%d]newFrame is NULL.", m_internalFrameCount - 1);
        goto CLEAN;
    }

    CLOGV("[R%d F%d]Generate preview frame. streamConfig %x",
            request->getKey(), newFrame->getFrameCount(), streamConfigBit);

    if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
        CLOGD("[R%d F%d]Flush is in progress.", request->getKey(), newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        if (captureFlag == true) {
            CLOGI("[R%d F%d]setFrameCapture true.", request->getKey(), newFrame->getFrameCount());
            newFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
        }
        goto CLEAN;
    }

    TIME_LOGGER_UPDATE(m_cameraId, request->getKey(), 0, USER_DATA, CREATE_PREVIEW_FRAME, newFrame->getFrameCount());

    /* BcropRegion is calculated in m_generateFrame    */
    /* so that updatePreviewStatRoi should be set here.*/
    components.parameters->getHwBayerCropRegion(
                    &bayerCropRegion.w,
                    &bayerCropRegion.h,
                    &bayerCropRegion.x,
                    &bayerCropRegion.y);
    components.parameters->updatePreviewStatRoi(m_currentPreviewShot, &bayerCropRegion);

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentPreviewShot);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Set metadata to frame fail. ret %d",
                 request->getKey(), newFrame->getFrameCount(), ret);
    }

    newFrame->setFrameType(frameType);

#ifdef USE_DUAL_CAMERA
    if (newFrame->isSlaveFrame() == true) {
        /* Slave 3AA input region is different with master's */
        m_adjustSlave3AARegion(&components, newFrame);
    }
#endif

    newFrame->setStreamRequested(STREAM_TYPE_RAW, rawStreamFlag);
    newFrame->setStreamRequested(STREAM_TYPE_ZSL_OUTPUT, zslStreamFlag);
    newFrame->setStreamRequested(STREAM_TYPE_DEPTH, depthStreamFlag);

    newFrame->setZoomRatio(m_configurations->getZoomRatio());
    newFrame->setActiveZoomRatio(components.parameters->getActiveZoomRatio());

    m_configurations->getZoomRect(&zoomRect);
    newFrame->setZoomRect(zoomRect);
    components.parameters->getActiveZoomRect(&activeZoomRect);
    newFrame->setActiveZoomRect(activeZoomRect);

    newFrame->setActiveZoomMargin(components.parameters->getActiveZoomMargin());

    if (captureFlag == true) {
        CLOGI("[F%d]setFrameCapture true.", newFrame->getFrameCount());
        newFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC) {
        switch (frameType) {
        case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
            /* slave : check dynamic bayer count */
            if (android_atomic_or(0, &m_needSlaveDynamicBayerCount) > 0)
                newFrame->setRequest(PIPE_3AC, true);
            break;
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            /* master : increase dynamic bayer count */
            if (newFrame->getRequest(PIPE_3AC))
                android_atomic_inc(&m_needSlaveDynamicBayerCount);
            break;
        default:
            break;
        }
    }

    if (frameType != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
    {
        /* Set service stream buffers to frame */
        if (components.parameters->getBatchSize(controlPipeId) > 1
            && components.parameters->useServiceBatchMode() == false) {
            ret = m_setupBatchFactoryBuffers(request, newFrame, targetfactory);
        } else {
            ret = m_setupPreviewFactoryBuffers(request, newFrame, targetfactory);
        }
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d T%d]Failed to setupPreviewStreamBuffer. ret %d",
                    request->getKey(), newFrame->getFrameCount(), newFrame->getFrameType(), ret);
        }
    }

    m_checkUpdateResult(newFrame, streamConfigBit);
    pipeId = m_getBayerPipeId();

    /* Attach VC0 buffer & push frame to VC0 */
    if (newFrame->getRequest(PIPE_VC0) == true
        && zslStreamFlag == false) {
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_VC0;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
        buffer.index = -2;
        dstPos = targetfactory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to get Internal Bayer Buffer. ret %d",
                    newFrame->getFrameCount(), ret);
        }

        CLOGV("[F%d B%d]Use Internal Bayer Buffer",
                newFrame->getFrameCount(), buffer.index);

        if (buffer.index < 0) {
            CLOGW("[F%d B%d]Invalid bayer buffer index. Skip to pushFrame",
                    newFrame->getFrameCount(), buffer.index);
            newFrame->setRequest(bufTag.pipeId[0], false);
        } else {
            ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                        pipeId, bufTag.pipeId[0], dstPos);
                newFrame->setRequest(bufTag.pipeId[0], false);
            } else {
                ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                            pipeId, bufTag.pipeId[0], dstPos);
                    newFrame->setRequest(bufTag.pipeId[0], false);
                }
            }
        }
    }

#ifdef SUPPORT_DEPTH_MAP
    if (newFrame->getRequest(PIPE_VC1) == true
        && m_flagUseInternalDepthMap == true) {
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_VC1;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
        buffer.index = -2;
        dstPos = targetfactory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to get Internal Depth Buffer. ret %d",
                    newFrame->getFrameCount(), ret);
        }

        CLOGV("[F%d B%d]Use Internal Depth Buffer",
                newFrame->getFrameCount(), buffer.index);

        if (buffer.index < 0) {
            CLOGW("[F%d B%d]Invalid bayer buffer index. Skip to pushFrame",
                    newFrame->getFrameCount(), buffer.index);
            newFrame->setRequest(bufTag.pipeId[0], false);
        } else {
            ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, dstPos);
            if (ret == NO_ERROR) {
                ret = newFrame->setDstBuffer(pipeId, buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setDstBuffer. pipeId %d(%d) pos %d",
                            pipeId, bufTag.pipeId[0], dstPos);
                    newFrame->setRequest(bufTag.pipeId[0], false);
                }
            } else {
                CLOGE("Failed to setDstBufferState. pipeId %d(%d) pos %d",
                        pipeId, bufTag.pipeId[0], dstPos);
                newFrame->setRequest(bufTag.pipeId[0], false);
            }
        }
    }
#endif

    /* Attach SrcBuffer */
    ret = m_setupEntity(pipeId, newFrame);
    if (ret != NO_ERROR) {
        CLOGW("[F%d]Failed to setupEntity. pipeId %d", newFrame->getFrameCount(), pipeId);
    } else {
        targetfactory->pushFrameToPipe(newFrame, pipeId);
    }

CLEAN:
    return ret;
}

status_t ExynosCamera:: m_visionFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraFrameFactory *targetfactory,
                                             frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext *service_shot_ext = NULL;
    uint32_t requestKey = 0;
    int pipeId = m_getBayerPipeId();
    uint32_t streamConfigBit = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    frame_handle_components_t components;
    getFrameHandleComponents(frameType, &components);

    int shutterSpeed = 0;
    int gain = 0;
    int irLedWidth = 0;
    int irLedDelay = 0;
    int irLedCurrent = 0;
    int irLedOnTime = 0;

    bool visionStreamFlag = false;

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_VC0, false);
    targetfactory->setRequest(PIPE_3AC, false);
    targetfactory->setRequest(PIPE_3AP, false);
    targetfactory->setRequest(PIPE_MCSC0, false);
    targetfactory->setRequest(PIPE_MCSC1, false);
    targetfactory->setRequest(PIPE_MCSC2, false);
#ifdef SUPPORT_DEPTH_MAP
    targetfactory->setRequest(PIPE_VC1, false);
#endif

    /* Check ZSL_INPUT stream */
    camera3_stream_buffer_t *inputBuffer = request->getInputBuffer();
    if(inputBuffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(inputBuffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            SET_STREAM_CONFIG_BIT(streamConfigBit, inputStreamId);

            switch (inputStreamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_INPUT:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
            case HAL_STREAM_ID_VISION:
                CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                        request->getKey(), inputStreamId);
                break;
            default:
                break;
            }
        } else {
            CLOGE("Stream is null (%d)", request->getKey());
        }
    }

    /* Setting DMA-out request flag based on stream configuration */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamIdwithBufferIdx(i);
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_VISION:
            CLOGV("requestKey %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VISION)",
                     request->getKey(), i);
            targetfactory->setRequest(PIPE_VC0, true);
            visionStreamFlag = true;
            break;
        case HAL_STREAM_ID_RAW:
        case HAL_STREAM_ID_ZSL_OUTPUT:
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_CALLBACK:
        case HAL_STREAM_ID_VIDEO:
        case HAL_STREAM_ID_PREVIEW_VIDEO:
        case HAL_STREAM_ID_JPEG:
        case HAL_STREAM_ID_CALLBACK_STALL:
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
        case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
            CLOGE("requestKey %d Invalid buffer-StreamType(%d)",
                    request->getKey(), id);
            break;
        default:
            CLOGE("Invalid stream ID %d", id);
            break;
        }
    }

    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        ret = INVALID_OPERATION;
        return ret;
    }

    Mutex::Autolock l(m_currentPreviewShotLock);

    m_updateCropRegion(m_currentPreviewShot);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentPreviewShot, service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), false);
    }
    components.parameters->setDsInputPortId(m_currentPreviewShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], false);
    m_updateEdgeNoiseMode(m_currentPreviewShot, false);
    m_updateExposureTime(m_currentPreviewShot);
    m_updateMasterCam(m_currentPreviewShot);

    /* Set framecount into request */
    m_frameCountLock.lock();
    if (request->getFrameCount() == 0) {
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
    }
    m_frameCountLock.unlock();

    /* Generate frame for YUV stream */
    ret = m_generateFrame(targetfactory, &m_processList, &m_processLock, newFrame, request);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to generateRequestFrame.", m_internalFrameCount - 1);
        goto CLEAN;
    } else if (newFrame == NULL) {
        CLOGE("[F%d]newFrame is NULL.", m_internalFrameCount - 1);
        goto CLEAN;
    }

    CLOGV("[R%d F%d]Generate vision frame. streamConfig %x",
            request->getKey(), newFrame->getFrameCount(), streamConfigBit);

    if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
        CLOGD("[R%d F%d]Flush is in progress.", request->getKey(), newFrame->getFrameCount());
        /* Generated frame is going to be deleted at flush() */
        goto CLEAN;
    }

    newFrame->setStreamRequested(STREAM_TYPE_VISION, visionStreamFlag);

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentPreviewShot);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Set metadata to frame fail. ret %d",
                 request->getKey(), newFrame->getFrameCount(), ret);
    }

    /* Set service stream buffers to frame */
    ret = m_setupVisionFactoryBuffers(request, newFrame);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Failed to setupPreviewStreamBuffer. ret %d",
                request->getKey(), newFrame->getFrameCount(), ret);
    }

    m_checkUpdateResult(newFrame, streamConfigBit);

    /* Attach SrcBuffer */
    ret = m_setupEntity(pipeId, newFrame);
    if (ret != NO_ERROR) {
        CLOGW("[F%d]Failed to setupEntity. pipeId %d", newFrame->getFrameCount(), pipeId);
    } else {
        targetfactory->pushFrameToPipe(newFrame, pipeId);
    }

CLEAN:
    return ret;
}

status_t ExynosCamera::m_handleInternalFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId,
                                                ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraRequestSP_sprt_t request = NULL;

    struct camera2_shot_ext resultShot;
    struct camera2_shot_ext *shot_ext = NULL;
    uint32_t framecount = 0;
    int dstPos = 0;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        CLOGE("current entity is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    if (frame->getHasRequest() == true
        && frame->isCompleteForResultUpdate() == false) {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d]request is NULL. pipeId %d",
                    frame->getFrameCount(), pipeId);
            return INVALID_OPERATION;
        }
    }

    switch(pipeId) {
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();

        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_OTF) {
#ifdef USE_DUAL_CAMERA
            if (frame->isSlaveFrame()) {
                m_slaveShotDoneQ->pushProcessQ(&frame);
            } else
#endif
            {
                m_shotDoneQ->pushProcessQ(&frame);
            }
        }

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                    entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index < 0) {
            CLOGE("[F%d B%d]Invalid buffer index. pipeId(%d)",
                    frame->getFrameCount(),
                    buffer.index,
                    entity->getPipeId());
            return BAD_VALUE;
        }

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        frame->setMetaDataEnable(true);
        components.parameters->updateMetaDataParam(shot_ext);

        if (frame->getHasRequest() == true) {
            ret = m_updateTimestamp(request, frame, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to update timestamp. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            CLOGV("[F%d(%d) B%d]Return 3AS Buffer.",
                    frame->getFrameCount(),
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    buffer.index);

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for 3AS. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        /* Handle bayer buffer */
        if (frame->getRequest(PIPE_VC0) == true || frame->getRequest(PIPE_3AC)) {
            ret = m_handleBayerBuffer(frame, request, pipeId);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(), ret);
            }
        }

#ifdef SUPPORT_DEPTH_MAP
#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
        {
            if (frame->getRequest(PIPE_VC1) == true) {
                ret = m_handleDepthBuffer(frame, request);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Handle deptg buffer failed. pipeId %d ret %d",
                             frame->getFrameCount(), pipeId, ret);
                }
            }
        }
#endif

        if (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
            if (frame->getRequest(PIPE_3AP) == true) {
                /* Send the bayer buffer to 3AA Pipe */
                dstPos = factory->getNodeType(PIPE_3AP);
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            entity->getPipeId(), ret);
                }
                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());
                }

                ret = m_setupEntity(PIPE_ISP, frame, &buffer, NULL);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)", PIPE_ISP, ret);
                }

                factory->pushFrameToPipe(frame, PIPE_ISP);
            }
        }
        break;

    case PIPE_FLITE:
        /* TODO: HACK: Will be removed, this is driver's job */
        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, __FUNCTION__);
        } else {
            /* Notify ShotDone to mainThread */
            framecount = frame->getFrameCount();
#ifdef USE_DUAL_CAMERA
            if (frame->isSlaveFrame()) {
                m_slaveShotDoneQ->pushProcessQ(&frame);
            } else
#endif
            {
                m_shotDoneQ->pushProcessQ(&frame);
            }

            /* handle src Buffer */
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for FLITE. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                }
            }

            if (frame->getRequest(PIPE_VC0) == true) {
                /* Send the bayer buffer to 3AA Pipe */
                dstPos = factory->getNodeType(PIPE_VC0);

                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                             entity->getPipeId(), ret);
                    return ret;
                }

                CLOGV("Deliver Flite Buffer to 3AA. driver->framecount %d hal->framecount %d",
                        getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                        frame->getFrameCount());

                ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                             PIPE_3AA, ret);
                    return ret;
                }

                factory->pushFrameToPipe(frame, PIPE_3AA);
            }
        }
        break;

    case PIPE_ISP:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                     entity->getPipeId(), ret);
        }
        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for ISP. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }
        break;

    case PIPE_MCSC:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                     entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }
        break;

    default:
        CLOGE("Invalid pipe ID");
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
             entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == pipeId
        && frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == true) {
        ret = frame->getMetaData(&resultShot);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d]Failed to getMetaData. ret %d",
                    request->getKey(), frame->getFrameCount(), ret);
        }

        ret = m_updateResultShot(request, &resultShot, PARTIAL_3AA);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d(%d) B%d]Failed to m_updateResultShot. ret %d",
                    request->getKey(),
                    frame->getFrameCount(),
                    resultShot.shot.dm.request.frameCount,
                    buffer.index, ret);
            return ret;
        }

        if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false
            && request->getNumOfInputBuffer() < 1
#ifdef USE_DUAL_CAMERA
            && (!(frame->getFrameType() == FRAME_TYPE_TRANSITION ||
                    frame->getFrameType() == FRAME_TYPE_TRANSITION_SLAVE))
#endif
           ) {
            m_sendNotifyShutter(request);
            m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
        }

        frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                        ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, &m_processLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        CLOGV("frame complete, frameCount %d FrameType %d",
                frame->getFrameCount(), frame->getFrameType());
        frame = NULL;
    }

    return ret;
}

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId,
                                                ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot_ext resultShot;
    uint32_t framecount = 0;
    int capturePipeId = -1;
    int streamId = -1;
    int dstPos = 0;
    bool isFrameComplete = false;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraRequestSP_sprt_t curRequest = NULL;
    entity_buffer_state_t bufferState;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    __unused int flipHorizontal = 0;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    ExynosCameraActivityAutofocus *autoFocusMgr = components.activityControl->getAutoFocusMgr();

#ifdef USE_DUAL_CAMERA
    enum DUAL_PREVIEW_MODE dualPreviewMode = m_configurations->getDualPreviewMode();
#endif

    if (frame->isCompleteForResultUpdate() == false
#ifdef USE_DUAL_CAMERA
            && (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
       ) {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d T%d]request is NULL. pipeId %d, resultState[%d, %d, %d]",
                    frame->getFrameCount(), frame->getFrameType(), pipeId,
                    frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL),
                    frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER),
                    frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL));
        }
    }

#ifdef FPS_CHECK
    m_debugFpsCheck((enum pipeline)pipeId);
#endif

#ifdef SUPPORT_ME
    ret = m_handleMeBuffer(frame, factory->getNodeType(PIPE_ME));
    if (ret != NO_ERROR) {
        CLOGE("F%d] Failed to handle Me buffer, PipeId(%d)", frame->getFrameCount(), pipeId);
    }
#endif

    switch (pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):[F%d]PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), __FUNCTION__);
        }

        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
#ifdef USE_DUAL_CAMERA
        if (frame->isSlaveFrame()) {
            m_slaveShotDoneQ->pushProcessQ(&frame);
        } else
#endif
        {
            m_shotDoneQ->pushProcessQ(&frame);
        }

        /* Return dummy-shot buffer */
        buffer.index = -2;

        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer for PIPE_FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getSrcBufferState for PIPE_FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error for PIPE_FLITE.",
                    frame->getFrameCount(), buffer.index);

            if ((frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                    == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED)
#ifdef USE_DUAL_CAMERA
                && frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE
#endif
                ) {
                if (request != NULL) {
                    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        return ret;
                    }
                }
            }
        }

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        /* Send the bayer buffer to 3AA Pipe */
        dstPos = factory->getNodeType(PIPE_VC0);
        buffer.index = -2;

        ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getDstBuffer for PIPE_VC0. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = frame->getDstBufferState(pipeId, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getDstBufferState for PIPE_VC0. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Dst buffer state is error for PIPE_VC0.",
                    frame->getFrameCount(), buffer.index);

            if ((frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                    == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED)
#ifdef USE_DUAL_CAMERA
                && frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE
#endif
                ) {
                if (request != NULL) {
                    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        return ret;
                    }
                }
            }
        }

        ret = m_setupEntity(PIPE_3AA, frame, &buffer, NULL);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to setupEntity for PIPE_3AA(PIPE_VC0). ret %d",
                     frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        factory->pushFrameToPipe(frame, PIPE_3AA);
        break;

    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();

        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_OTF) {
#ifdef USE_DUAL_CAMERA
            if (frame->isSlaveFrame()) {
                m_slaveShotDoneQ->pushProcessQ(&frame);
            } else
#endif
            {
                m_shotDoneQ->pushProcessQ(&frame);
            }
        }

        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer for PIPE_3AA. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getSrcBufferState for PIPE_3AA. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error for PIPE_3AA. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            if ((frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                    == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED)
#ifdef USE_DUAL_CAMERA
                && frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE
#endif
                ) {
                if (request != NULL) {
                    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                    }
                }
            }
        }

        frame->setMetaDataEnable(true);
        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        components.parameters->updateMetaDataParam(shot_ext);

        ret = m_updateTimestamp(request, frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to update timestamp. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        /* Return dummy-shot buffer */
        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            CLOGV("[F%d(%d) B%d]Return 3AS Buffer.",
                    frame->getFrameCount(),
                    frame->getMetaFrameCount(),
                    buffer.index);

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for 3AS. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        /* Handle bayer buffer */
        if (frame->getRequest(PIPE_VC0) == true || frame->getRequest(PIPE_3AC) == true) {
            ret = m_handleBayerBuffer(frame, request, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Handle bayer buffer failed. pipeId %d(%s) ret %d",
                         frame->getFrameCount(), pipeId,
                         (frame->getRequest(PIPE_VC0) == true) ? "PIPE_VC0" : "PIPE_3AC",
                         ret);
            }
        }

#ifdef SUPPORT_DEPTH_MAP
#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
        {
            if (frame->getRequest(PIPE_VC1) == true) {
                ret = m_handleDepthBuffer(frame, request);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Handle deptg buffer failed. pipeId %d ret %d",
                             frame->getFrameCount(), pipeId, ret);
                }
            }
        }
#endif

        /* Return VRA buffer */
        if ((frame->getRequest(PIPE_VRA) == true) &&
            (frame->getRequest(PIPE_3AF) == true)) {
            /* Send the 3AA downscaled Yuv buffer to VRA Pipe */
            dstPos = factory->getNodeType(PIPE_3AF);
            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDst buffer, pipeId(%d), ret(%d)", PIPE_3AF, ret);
            }

            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), type(%d), pipeId(%d) outport(%d)",
                    buffer.index, frame->getFrameCount(), frame->getFrameType(), pipeId, PIPE_3AF);

                ret = frame->setSrcBufferState(PIPE_VRA, ENTITY_BUFFER_STATE_ERROR);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d",
                            frame->getFrameCount(), PIPE_VRA, ret);
                }
            } else {
                ret = m_setupEntity(PIPE_VRA, frame, &buffer, NULL);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                            PIPE_VRA, ret);
                }
            }

            if (factory->checkPipeThreadRunning(PIPE_VRA) == false) {
                factory->startThread(PIPE_VRA);
            }
            factory->pushFrameToPipe(frame, PIPE_VRA);

        }

        if (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
            /* Send the bayer buffer to 3AA Pipe */
            dstPos = factory->getNodeType(PIPE_3AP);

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
            }
            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), type(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), frame->getFrameType(), pipeId);
            }

            ret = m_setupEntity(PIPE_ISP, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                         PIPE_ISP, ret);
            }

            if (m_flagFirstPreviewTimerOn == true) {
                CLOGI("first frame(F%d(%d) T%d) 3AA DONE",
                        frame->getFrameCount(), frame->getMetaFrameCount(), frame->getFrameType());
            }

            if (frame->getRequest(PIPE_GMV) == true) {
                factory->pushFrameToPipe(frame, PIPE_GMV);
            } else
#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
                && dualPreviewMode == DUAL_PREVIEW_MODE_HW) {
                factory->pushFrameToPipe(frame, PIPE_SYNC);
            } else
#endif
            {
                factory->pushFrameToPipe(frame, PIPE_ISP);
            }
            break;
        }

    case PIPE_ISP:
        if (pipeId == PIPE_ISP) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                        pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), pipeId);


            if ((frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                    == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED)
#ifdef USE_DUAL_CAMERA
                && frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE
#endif
                ) {
                    if (request != NULL) {
                        ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                                    frame->getFrameCount(), buffer.index, ret);
                            return ret;
                        }
                    }
                }
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for ISP. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (m_flagFirstPreviewTimerOn == true) {
            CLOGI("first frame(F%d(%d)), type(%d) ISP DONE",
                    frame->getFrameCount(), frame->getMetaFrameCount(), frame->getFrameType());
        }

        if (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
            break;
        }

    case PIPE_MCSC:
        if (pipeId == PIPE_MCSC) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }

            CLOGV("Return MCSC Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
        }

#ifdef USE_DUAL_CAMERA
        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
            && frame->getRequest(PIPE_SYNC) == true) {
            capturePipeId = PIPE_MCSC0 + m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);

            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(capturePipeId));
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
            }

            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
                frame->setFrameState(FRAME_STATE_SKIPPED);
            } else {
                if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                    ret = m_setupEntity(PIPE_FUSION, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                PIPE_FUSION, ret);
                    }
                } else {
                    ret = frame->setSrcBuffer(PIPE_SYNC, buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                                PIPE_SYNC, ret);
                    }
                }
            }

            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->pushFrameToPipe(frame, PIPE_SYNC);
        }
#endif

        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            CLOGW("[F%d T%d] Skipped frame. Force callback result. frameCount %d",
                frame->getFrameCount(), frame->getFrameType(), frame->getFrameCount());
#ifdef USE_DUAL_CAMERA
            if (frame->getRequest(PIPE_SYNC) == false)
#endif
            {
                if (request != NULL) {
                    ret = m_sendForceYuvStreamResult(request, frame, factory);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to forceResultCallback. frameCount %d", frame->getFrameCount());
                        return ret;
                    }
                } else {
                    CLOGE("[F%d]request is null. All buffers should be putBuffer!!!",
                            frame->getFrameCount());
                }
            }

            /* Return internal buffers */
            m_handlePreviewFrame_vendor_handleBuffer(frame, pipeId, factory, components, ret);

            /* Return VRA buffer */
            if ((frame->getRequest(PIPE_VRA) == true) &&
				(frame->getRequest(PIPE_MCSC5) == true)) {
                ExynosCameraBuffer srcBuffer;

                srcBuffer.index = -2;
                ret = frame->getDstBuffer(pipeId, &srcBuffer, factory->getNodeType(PIPE_MCSC5));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getDst buffer, pipeId(%d), ret(%d)", PIPE_MCSC5, ret);
                } else {
                    if (srcBuffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(srcBuffer);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to putBuffer. pipeId(%d), ret(%d)", PIPE_MCSC5, ret);
                        }
                    }
                }
            }

#ifdef USE_DUAL_CAMERA
            if (frame->getRequest(PIPE_SYNC) == false)
#endif
            {
                frame->setFrameState(FRAME_STATE_COMPLETE);
                if (frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                    == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED) {
                    frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                    ExynosCameraFrame::RESULT_UPDATE_STATUS_READY);
                    pipeId = frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL);
                }
            }
        } else {
            if (m_flagFirstPreviewTimerOn == true) {
                m_firstPreviewTimer.stop();
                m_flagFirstPreviewTimerOn = false;

                CLOGD("m_firstPreviewTimer stop");

                CLOGD("============= First Preview time ==================");
                CLOGD("= configureStream ~ first frame  : %d msec", (int)m_firstPreviewTimer.durationMsecs());
                CLOGD("===================================================");
                autoFocusMgr->displayAFInfo();
            }

#ifdef USE_DUAL_CAMERA
            if (frame->getRequest(PIPE_SYNC) == false)
#endif
            {
                /* set N-1 zoomRatio in Frame */
                if (m_configurations->getAppliedZoomRatio() < 1.0f) {
                    frame->setAppliedZoomRatio(frame->getZoomRatio());
                } else {
                    frame->setAppliedZoomRatio(m_configurations->getAppliedZoomRatio());
                }

                m_configurations->setAppliedZoomRatio(frame->getZoomRatio());
            }

            /* PIPE_MCSC 0, 1, 2 */
            for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
                capturePipeId = PIPE_MCSC0 + i;
                if (frame->getRequest(capturePipeId) == true) {
                    streamId = m_streamManager->getYuvStreamId(i);

                    if ((m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true)
                            && ((m_parameters[m_cameraId]->getYsumPordId() % ExynosCameraParameters::YUV_MAX) == i)
                            && (request != NULL)) {
                        /* Update YSUM value for YSUM recording */
                        ret = m_updateYsumValue(frame, request);
                        if (ret != NO_ERROR) {
                            CLOGE("failed to setYsumValue, ret(%d)", ret);
                            return ret;
                        }
                    }

#ifdef USE_DUAL_CAMERA
                    if (dualPreviewMode == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        /* In case of dual camera, send preview stream after pipe_fusion */
                        continue;
                    } else
#endif
#ifdef SUPPORT_HW_GDC
                    if (components.parameters->getGDCEnabledMode() == true
                        && (streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_VIDEO) {
                        m_gdcQ->pushProcessQ(&frame);
                    } else
#endif
#ifdef USES_SW_VDIS
                    if ( (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true)
                            && ((streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_VIDEO)) {
                        if (m_exCameraSolutionSWVdis != NULL) {
                            ret = m_exCameraSolutionSWVdis->handleFrame(ExynosCameraSolutionSWVdis::SOLUTION_PROCESS_PRE,
                                                                        frame, pipeId, capturePipeId, factory);
                            if (ret == NO_ERROR) {
                                /* Push frame to VDIS pipe */
                                int vdisPipeId = m_exCameraSolutionSWVdis->getPipeId();
                                factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[vdisPipeId], vdisPipeId);
                                factory->pushFrameToPipe(frame, vdisPipeId);

                                if (factory->checkPipeThreadRunning(vdisPipeId) == false) {
                                    factory->startThread(vdisPipeId);
                                }

                                return ret;
                                //continue;
                            }
                        } else {
                            /* NOP : fall through */
                        }
                    }
#endif
                    {
                        ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(capturePipeId));
                        if (ret != NO_ERROR || buffer.index < 0) {
                            CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                    capturePipeId, buffer.index, frame->getFrameCount(), ret);
                            return ret;
                        }

                        ret = frame->getDstBufferState(pipeId, &bufferState, factory->getNodeType(capturePipeId));
                        if (ret < 0) {
                            CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                            return ret;
                        }

                        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                            CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                    buffer.index, frame->getFrameCount(), pipeId);
                        }

                        if (request != NULL) {
                            request->setStreamBufferStatus(streamId, streamBufferState);
                        }

                        {
                            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                                CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                        buffer.index, frame->getFrameCount(), pipeId);
                            }

                            if (request != NULL) {
                                request->setStreamBufferStatus(streamId, streamBufferState);

                                {
                                    ret = m_sendYuvStreamResult(request, &buffer, streamId, false, frame->getStreamTimestamp());
                                    if (ret != NO_ERROR) {
                                        CLOGE("Failed to resultCallback."
                                                " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                                capturePipeId, buffer.index, frame->getFrameCount(), streamId, ret);
                                        return ret;
                                    }
                                }
                            } else {
                                if (buffer.index >= 0) {
                                    ret = m_bufferSupplier->putBuffer(buffer);
                                    if (ret !=  NO_ERROR) {
                                        CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                                frame->getFrameCount(), buffer.index, pipeId, ret);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
                if ((frame->getRequest(PIPE_VRA) == true) &&
						(frame->getRequest(PIPE_MCSC5) == true)) {
                    /* Send the Yuv buffer to VRA Pipe */
                    dstPos = factory->getNodeType(PIPE_MCSC5);

                    ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                                pipeId, ret);
                    }

                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);

                        ret = frame->setSrcBufferState(PIPE_VRA, ENTITY_BUFFER_STATE_ERROR);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d",
                                    frame->getFrameCount(), PIPE_VRA, ret);
                        }
                    } else {
                        ret = m_setupEntity(PIPE_VRA, frame, &buffer, NULL);
                        if (ret != NO_ERROR) {
                            CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                    PIPE_VRA, ret);
                        }
                    }

#if 0 /* test dump code */
                    {
                        bool bRet;
                        char filePath[70];

                        memset(filePath, 0, sizeof(filePath));
                        snprintf(filePath, sizeof(filePath), "/data/camera/VRA_Input_FT%d_F%d.nv21",
                            frame->getFrameType(), frame->getFrameCount());

                        if (frame->getFrameCount()%10 == 0) {
                            bRet = dumpToFile((char *)filePath, buffer.addr[0], buffer.size[0]);
                            if (bRet != true)
                                CLOGE("couldn't make a raw file");
                        }
                    }
#endif
                    if (factory->checkPipeThreadRunning(PIPE_VRA) == false) {
                        factory->startThread(PIPE_VRA);
                    }
                    factory->pushFrameToPipe(frame, PIPE_VRA);
                }
            }
        }
        break;
    case PIPE_GMV:
        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            return ret;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getSrcBufferState. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            /* continue */
        }

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for PIPE_GMV. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            /* continue */
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
            && dualPreviewMode == DUAL_PREVIEW_MODE_HW) {
            factory->pushFrameToPipe(frame, PIPE_SYNC);
        } else
#endif
        {
            factory->pushFrameToPipe(frame, PIPE_ISP);
        }
        break;
    case PIPE_VRA:
        if (pipeId == PIPE_VRA) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                        pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

#ifdef SUPPORT_HFD
            if (frame->getRequest(PIPE_HFD) == true) {
                if (buffer.index < 0 || bufferState == ENTITY_BUFFER_STATE_ERROR) {
                    ret = frame->setSrcBufferState(PIPE_HFD, ENTITY_BUFFER_STATE_ERROR);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d",
                                frame->getFrameCount(), buffer.index, PIPE_HFD, ret);
                    }
                } else {
                    ret = m_setupEntity(PIPE_HFD, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to setSrcBuffer. PIPE_HFD, ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        return ret;
                    }
                }

                factory->pushFrameToPipe(frame, PIPE_HFD);
            } else
#endif
            {
                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for VRA. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                    }
                } else {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), pipeId);
                }
            }

#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                /* already this frame was completed in sync pipe */
                return ret;
            }
#endif
        }
        break;
    case PIPE_GDC:
        if (pipeId == PIPE_GDC) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }

            CLOGV("Return GDC Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
        }

        if (frame->getRequest(pipeId) == true) {
            streamId = HAL_STREAM_ID_VIDEO;

            ret = frame->getDstBuffer(pipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), buffer.index,
                        pipeId, ret);
                return ret;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBufferState. pipeId %d ret %d",
                        frame->getFrameCount(),
                        pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("[F%d B%d]DstBufferState is ERROR. pipeId %d",
                        frame->getFrameCount(), buffer.index,
                        pipeId);
            }

            if (request != NULL) {
                request->setStreamBufferStatus(streamId, streamBufferState);

                ret = m_sendYuvStreamResult(request, &buffer, streamId, false, frame->getStreamTimestamp());
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                            request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                            ret);
                    return ret;
                }
            } else {
                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret !=  NO_ERROR) {
                        CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                frame->getFrameCount(), buffer.index, pipeId, ret);
                    }
                }
            }
        }
        break;

#ifdef SUPPORT_HFD
    case PIPE_HFD:
        entity_state_t entityState;
        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            return ret;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret < 0) {
            CLOGE("[F%d B%d]getSrcBufferState fail. pipeId %d, ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for HFD. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                break;
            }
        }

        ret = frame->getEntityState(pipeId, &entityState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to getEntityState. pipeId %d",
                    frame->getFrameCount(), pipeId);
            break;
        }

        if (entityState == ENTITY_STATE_FRAME_DONE) {
            ret = m_updateFDAEMetadata(frame);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to updateFDAEMetadata. ret %d",
                        frame->getFrameCount(), ret);
                /* continue */
            }
        }

        break;
#endif

#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC:
        {
            int pipeId_next = -1;

            if (frame->getFrameState() == FRAME_STATE_SKIPPED
                && dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                ExynosCameraBuffer dstBuffer;
                int srcpipeId = -1;
                dstBuffer.index = -2;

                if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                    srcpipeId = PIPE_SYNC;
                } else {
                    srcpipeId = PIPE_FUSION;
                }

                ret = frame->getSrcBuffer(srcpipeId, &dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                            srcpipeId, ret);
                }

                if (dstBuffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(dstBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for HFD. ret %d",
                                frame->getFrameCount(), dstBuffer.index, ret);
                    }
                }

                if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                    if (request != NULL) {
                        ret = m_sendForceYuvStreamResult(request, frame, factory);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to forceResultCallback. frameCount %d", frame->getFrameCount());
                            return ret;
                        }
                    } else {
                        CLOGE("[F%d]request is null. All buffers should be putBuffer!!!",
                                frame->getFrameCount());
                    }

                    /* Return internal buffers */
                    frame->setFrameState(FRAME_STATE_COMPLETE);
                    if (frame->getStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL)
                        == ExynosCameraFrame::RESULT_UPDATE_STATUS_REQUIRED) {
                        frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                        ExynosCameraFrame::RESULT_UPDATE_STATUS_READY);
                        pipeId = frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL);
                    }
                } else {
                    /* my life is over anymore in dual slave frame */
                    frame->setFrameState(FRAME_STATE_COMPLETE);
                    frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                            ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                    frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                            ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                    frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                            ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                }
                break;
            }

            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
                if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                    pipeId_next = PIPE_FUSION;

                    if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                        buffer.index = -2;
                        ret = frame->getDstBuffer(pipeId, &buffer);
                        if (buffer.index < 0) {
                            CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                    buffer.index, frame->getFrameCount(), pipeId);
                        } else {
                            ret = frame->setSrcBuffer(pipeId_next, buffer, OUTPUT_NODE_2);
                            if (ret != NO_ERROR) {
                                CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                            }
                        }
                    }
                }
            } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                    if (dualPreviewMode == DUAL_PREVIEW_MODE_HW) {
                        capturePipeId = PIPE_ISPC;
                    } else if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                        capturePipeId = (m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW)
                                         % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;
                    }

                    buffer.index = -2;
                    ret = frame->getDstBuffer(PIPE_ISP, &buffer, factory->getNodeType(capturePipeId));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_ISP, ret);
                    }

                    if (buffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(buffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to putBuffer for PIPE_SYNC. ret %d",
                                    frame->getFrameCount(), buffer.index, ret);
                        }
                    }
                }

                /* my life is over anymore in dual slave frame */
                frame->setFrameState(FRAME_STATE_COMPLETE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                break;
            } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW) {
                pipeId_next = PIPE_FUSION;
            } else if(frame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE) {
                /* main(master) frame factory only have pipe fusion.
                you should be connect main(master) frame factory about all frame type. */
                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                pipeId_next = PIPE_FUSION;
            }

            if (pipeId_next == PIPE_FUSION) {
                ret = m_updateSizeBeforeDualFusion(frame, pipeId);
                if (ret != NO_ERROR) {
                    CLOGE("m_updateSizeBeforeDualFusion(framecount(%d), pipeId(%d)", frame->getFrameCount(), pipeId);
                }
            }

            if (pipeId_next >= 0) {
                factory->pushFrameToPipe(frame, pipeId_next);
            }
        }
        break;

    case PIPE_FUSION:
        if (pipeId == PIPE_FUSION) {
            /* OUTPUT_NODE_1 */
            buffer.index = -2;
            ret = frame->getSrcBuffer(pipeId, &buffer, OUTPUT_NODE_1);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                }
            }

            /* OUTPUT_NODE_2 */
            buffer.index = -2;
            ret = frame->getSrcBuffer(pipeId, &buffer, OUTPUT_NODE_2);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                }
            }

            /* set N-1 zoomRatio in Frame */
            if (m_configurations->getAppliedZoomRatio() < 1.0f) {
                frame->setAppliedZoomRatio(frame->getZoomRatio());
            } else {
                frame->setAppliedZoomRatio(m_configurations->getAppliedZoomRatio());
            }

            m_configurations->setAppliedZoomRatio(frame->getZoomRatio());

            /*
            CLOGV("Return FUSION Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
            */
        }

        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
        }

        if (frame->getRequest(pipeId) == true) {
            int colorFormat = components.parameters->getHwPreviewFormat();
            int planeCount = getYuvPlaneCount(colorFormat);
            streamId = HAL_STREAM_ID_PREVIEW;

            buffer.index = -2;
            ret = frame->getDstBuffer(pipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), buffer.index,
                        pipeId, ret);
                //return ret;
                break;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBufferState. pipeId %d ret %d",
                        frame->getFrameCount(),
                        pipeId, ret);
                return ret;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                CLOGE("[F%d B%d]DstBufferState is ERROR. pipeId %d",
                        frame->getFrameCount(), buffer.index,
                        pipeId);
            }

            /* DEST BUFFER  of PIPE_FUION */
            for (int plane = 0; plane < planeCount; plane++) {
                if (m_ionClient >= 0)
                    exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
            }

            if (request != NULL) {
                {
                    if (request->hasStream(HAL_STREAM_ID_VIDEO) == false
                        && m_flagVideoStreamPriority == false) {
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }

                        request->setStreamBufferStatus(streamId, streamBufferState);
                        ret = m_sendYuvStreamResult(request, &buffer, streamId, false, frame->getStreamTimestamp());
                        if (ret != NO_ERROR) {
                            CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                                    request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                                    ret);
                            return ret;
                        }
                    } else {
#ifdef USE_DUAL_CAMERA
#endif
                        {
                            ret = m_sendYuvStreamResult(request, &buffer, streamId, false, frame->getStreamTimestamp());
                            if (ret != NO_ERROR) {
                                CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                                        request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                                        ret);
                                return ret;
                            }
                        }
                    }
                }
            } else {
                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret !=  NO_ERROR) {
                        CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                frame->getFrameCount(), buffer.index, pipeId, ret);
                    }
                }
            }
        }
        break;
#endif
    case PIPE_VDIS:
#ifdef USES_SW_VDIS
        m_handleVdisFrame(frame, request, pipeId, factory);
#endif //USES_SW_VDIS
        break;
    default:
        CLOGE("Invalid pipe ID(%d)", pipeId);
        break;
    }

    m_frameCompleteLock.lock();
    ret = frame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
            pipeId, ENTITY_STATE_COMPLETE, ret);
        m_frameCompleteLock.unlock();
        return ret;
    }

    isFrameComplete = frame->isComplete();
    m_frameCompleteLock.unlock();

    if (request != NULL) {
#ifdef USE_DUAL_CAMERA
        /* don't update result state for dual_slave frame */
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
        {
            if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == pipeId
                && frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == true) {
                if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA) == false) {
                    ret = frame->getMetaData(&resultShot);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
                    }

                    for (int batchIndex = 0; batchIndex < components.parameters->getBatchSize((enum pipeline)pipeId); batchIndex++) {
                        if (batchIndex > 0) {
                            if (m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                                break;
                            }

                            curRequest = m_requestMgr->getRunningRequest(frame->getFrameCount() + batchIndex);
                            if (curRequest == NULL) {
                                CLOGE("[F%d]Request is NULL", frame->getFrameCount() + batchIndex);
                                continue;
                            }
                            resultShot.shot.udm.sensor.timeStampBoot += resultShot.shot.dm.sensor.frameDuration;
                        } else {
                            curRequest = request;
                        }

                        if (curRequest != NULL) {
                            ret = m_updateResultShot(curRequest, &resultShot, PARTIAL_3AA);
                            if (ret != NO_ERROR) {
                                CLOGE("[F%d(%d) B%d]Failed to m_updateResultShot. ret %d",
                                        frame->getFrameCount(),
                                        resultShot.shot.dm.request.frameCount,
                                        buffer.index, ret);
                                return ret;
                            }

                            if (curRequest->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
                                m_sendNotifyShutter(curRequest);
                                m_sendPartialMeta(curRequest, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
                            }
                        }
                    }
                }

                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
            }

            if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == pipeId
                && (frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == true)) {
                ret = frame->getMetaData(&resultShot);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
                }

                for (int batchIndex = 0; batchIndex < components.parameters->getBatchSize((enum pipeline)pipeId); batchIndex++) {
                    if (batchIndex > 0) {
                        if (m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                            break;
                        }

                        curRequest = m_requestMgr->getRunningRequest(frame->getFrameCount() + batchIndex);
                        if (curRequest == NULL) {
                            CLOGE("[F%d]Request is NULL", frame->getFrameCount() + batchIndex);
                            continue;
                        }
                        resultShot.shot.udm.sensor.timeStampBoot += resultShot.shot.dm.sensor.frameDuration;
                    } else {
                        curRequest = request;
                    }

                    /* update N-1 zoomRatio */
                    resultShot.shot.udm.zoomRatio = frame->getAppliedZoomRatio();

                    ret = m_updateResultShot(curRequest, &resultShot, PARTIAL_NONE, (frame_type_t)frame->getFrameType());
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d(%d)]Failed to m_updateResultShot. ret %d",
                                frame->getFrameCount(), resultShot.shot.dm.request.frameCount, ret);
                        return ret;
                    }

                    ret = m_sendMeta(curRequest, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d]Failed to sendMeta. ret %d",
                                frame->getFrameCount(), ret);
                    }
                }

                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                                ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
            }
        }
    }

    if (isFrameComplete == true) {
        ret = m_removeFrameFromList(&m_processList, &m_processLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        CLOGV("frame complete, frameCount %d FrameType %d",
                frame->getFrameCount(), frame->getFrameType());
        frame = NULL;
    }

    return ret;
}

status_t ExynosCamera::m_handleVisionFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId,
                                                ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot_ext resultShot;
    uint32_t framecount = 0;
    int streamId = HAL_STREAM_ID_VISION;
    int dstPos = 0;
    ExynosCameraRequestSP_sprt_t request = NULL;
    entity_buffer_state_t bufferState;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    if (frame->isCompleteForResultUpdate() == false) {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d]request is NULL. pipeId %d",
                    frame->getFrameCount(), pipeId);
            return INVALID_OPERATION;
        }
    }

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        if (request != NULL) {
            CLOGE("[R%d F%d]current entity is NULL. pipeId %d",
                request->getKey(), frame->getFrameCount(), pipeId);
        }
        return INVALID_OPERATION;
    }

    switch(pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):[F%d]PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), __FUNCTION__);
        }

        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&frame);

        /* Return dummy-shot buffer */
        buffer.index = -2;

        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer for PIPE_FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getSrcBufferState for PIPE_FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error for PIPE_FLITE.",
                    frame->getFrameCount(), buffer.index);

            if (request != NULL) {
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
                }
            }
        }
        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for FLITE. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        dstPos = factory->getNodeType(PIPE_VC0);
        buffer.index = -2;

        ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getDstBuffer for PIPE_VC0. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = frame->getDstBufferState(entity->getPipeId(), &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getDstBufferState for PIPE_VC0. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Dst buffer state is error for PIPE_VC0.",
                    frame->getFrameCount(), buffer.index);

            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
        }

        if (request != NULL) {
            request->setStreamBufferStatus(streamId, streamBufferState);
        }

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        frame->setMetaDataEnable(true);

        ret = m_updateTimestamp(request, frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to update timestamp. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        if (request != NULL) {
            if (m_flagFirstPreviewTimerOn == true) {
                m_firstPreviewTimer.stop();
                m_flagFirstPreviewTimerOn = false;

                CLOGD("m_firstPreviewTimer stop");

                CLOGD("============= First Preview time ==================");
                CLOGD("= configureStream ~ first frame  : %d msec", (int)m_firstPreviewTimer.durationMsecs());
                CLOGD("===================================================");
            }

            ret = m_sendVisionStreamResult(request, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback."
                        " PIPE_VC0 bufferIndex %d frameCount %d streamId %d ret %d",
                        buffer.index, frame->getFrameCount(), streamId, ret);
                return ret;
            }
        } else {
            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for FLITE. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
                }
            }
        }

        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (request != NULL) {
        if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == pipeId
            && frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == true) {
            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to getMetaData. ret %d",
                        request->getKey(), frame->getFrameCount(), ret);
            }

            ret = m_updateResultShot(request, &resultShot, PARTIAL_3AA);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d(%d)]Failed to m_updateResultShot. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        resultShot.shot.dm.request.frameCount,
                        ret);
                return ret;
            }

            if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
                m_sendNotifyShutter(request);
                m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
            }

            frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                            ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
        }

        if (frame->getPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == pipeId
            && (frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == true)) {
            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to getMetaData. ret %d",
                        request->getKey(), frame->getFrameCount(), ret);
            }

            ret = m_updateResultShot(request, &resultShot, PARTIAL_NONE, (frame_type_t)frame->getFrameType());
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d(%d)]Failed to m_updateResultShot. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        resultShot.shot.dm.request.frameCount,
                        ret);
                return ret;
            }

            ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to sendMeta. ret %d",
                        frame->getFrameCount(), ret);
            }

            frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                            ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
        }
    }

    if (frame->isComplete() == true) {
        ret = m_removeFrameFromList(&m_processList, &m_processLock, frame);
        if (ret < 0) {
            CLOGE("remove frame from processList fail, ret(%d)", ret);
        }

        CLOGV("frame complete, frameCount %d FrameType %d",
                frame->getFrameCount(), frame->getFrameType());
        frame = NULL;
    }

    return ret;
}

status_t ExynosCamera::m_sendPartialMeta(ExynosCameraRequestSP_sprt_t request,
                                         EXYNOS_REQUEST_RESULT::TYPE type)
{
    ResultRequest resultRequest = NULL;
    uint32_t frameNumber = 0;
    camera3_capture_result_t *requestResult = NULL;
    CameraMetadata resultMeta;

    status_t ret = OK;

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(), type);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. PARTIAL_META",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. PARTIAL_META",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    frameNumber = request->getKey();
    request->setRequestLock();

    {
        resultMeta = request->get3AAResultMeta();
    }
    request->setRequestUnlock();

    requestResult->frame_number = frameNumber;
    requestResult->result = resultMeta.release();
    requestResult->num_output_buffers = 0;
    requestResult->output_buffers = NULL;
    requestResult->input_buffer = NULL;
    requestResult->partial_result = 1;

    CLOGV("framecount %d request %d", request->getFrameCount(), request->getKey());

    m_requestMgr->pushResultRequest(resultRequest);

    return ret;
}

void ExynosCamera::m_checkEntranceLux(struct camera2_shot_ext *meta_shot_ext) {
    uint32_t data = 0;

    if (m_checkFirstFrameLux == false
        || m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true) {
        m_checkFirstFrameLux = false;
        return;
    }

    data = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[399];

    m_checkFirstFrameLux = false;
}

status_t ExynosCamera::m_sendYuvStreamResult(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraBuffer *buffer, int streamId, bool skipBuffer,
                                             __unused uint64_t streamTimeStamp
                                             )
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ResultRequest resultRequest = NULL;
    bool OISCapture_activated = false;
    uint32_t dispFps = EXYNOS_CAMERA_PREVIEW_FPS_REFERENCE;
    uint32_t maxFps, minFps;
    int ratio = 1;
    int LowLightSkipFcount = 0;
    bool skipStream = false;
    uint32_t frameCount = request->getFrameCount();

    if (buffer == NULL) {
        CLOGE("buffer is NULL");
        return BAD_VALUE;
    }

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from streamMgr", streamId);
        return INVALID_OPERATION;
    }

    m_configurations->getPreviewFpsRange(&minFps, &maxFps);

    for (int batchIndex = 0; batchIndex < buffer->batchSize; batchIndex++) {
        if (batchIndex > 0) {
            /* Get next request */
            request = m_requestMgr->getRunningRequest(frameCount + batchIndex);
            if (request == NULL) {
                CLOGE("[F%d]Request is NULL", frameCount + batchIndex);
                continue;
            }
        }

        resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
                                                          EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
        if (resultRequest == NULL) {
            CLOGE("[R%d F%d S%d] createResultRequest fail.",
                    request->getKey(), request->getFrameCount(), streamId);
            ret = INVALID_OPERATION;
            continue;
        }

        requestResult = resultRequest->getCaptureResult();
        if (requestResult == NULL) {
            CLOGE("[R%d F%d S%d] getCaptureResult fail.",
                    request->getKey(), request->getFrameCount(), streamId);
            ret = INVALID_OPERATION;
            continue;
        }

        streamBuffer = resultRequest->getStreamBuffer();
        if (streamBuffer == NULL) {
            CLOGE("[R%d F%d S%d] getStreamBuffer fail.",
                    request->getKey(), request->getFrameCount(), streamId);
            ret = INVALID_OPERATION;
            continue;
        }

        ret = stream->getStream(&(streamBuffer->stream));
        if (ret != NO_ERROR) {
            CLOGE("Failed to get stream %d from ExynosCameraStream", streamId);
            continue;
        }

        streamBuffer->buffer = buffer->handle[batchIndex];

        m_configurations->getFrameSkipCount(&LowLightSkipFcount);

        if (LowLightSkipFcount > 0)
            skipStream = true;

        if ((maxFps > dispFps)
            && (request->hasStream(HAL_STREAM_ID_VIDEO) == false)
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_PREVIEW)) {
            ratio = (int)((maxFps * 10 / dispFps) / buffer->batchSize / 10);
            m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
            skipStream = (m_displayPreviewToggle == 0) ? false : true;
        }

        ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status,
                OISCapture_activated | skipStream | skipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                    request->getKey(), request->getFrameCount(), streamId, buffer->index);
            continue;
        }

        if ((m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true)
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO)
            && (request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_OK)) {
            struct ysum_data ysumdata;
            request->getYsumValue(&ysumdata);
            ret = updateYsumBuffer(&ysumdata, buffer);
            if (ret != NO_ERROR) {
                CLOGE("updateYsumBuffer fail, bufferIndex(%d), ret(%d)", buffer->index, ret);
                return ret;
            }
        }

#ifdef DEBUG_DISPLAY_BUFFER
        {
            int displayId = -1;
            if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
                displayId = HAL_STREAM_ID_PREVIEW;
            } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
                displayId = HAL_STREAM_ID_CALLBACK;
            }

            if (streamBuffer->status != CAMERA3_BUFFER_STATUS_ERROR
                && (streamId % HAL_STREAM_ID_MAX == displayId)
                && (streamBuffer->stream->usage & (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN))
                && buffer->size[0] > 3000
                && buffer->addr[0][0] == 0 && buffer->addr[0][10] == 0 && buffer->addr[0][100] == 0
                && buffer->addr[0][1000] == 0 && buffer->addr[0][2000] == 0 && buffer->addr[0][3000] == 0) {
                CLOGD("[R%d id:%d shotmode%d]: %d, %d, %d, %d, %d, %d Green image buffer detected!!!",
                    request->getKey(), streamId, m_configurations->getModeValue(CONFIGURATION_SHOT_MODE),
                    buffer->addr[0][1], buffer->addr[0][11], buffer->addr[0][101],
                    buffer->addr[0][1001], buffer->addr[0][2001], buffer->addr[0][2999]);
            }
        }
#endif

        streamBuffer->acquire_fence = -1;
        streamBuffer->release_fence = -1;

        /* construct result for service */
        requestResult->frame_number = request->getKey();
        requestResult->result = NULL;
        requestResult->input_buffer = request->getInputBuffer();
        requestResult->num_output_buffers = 1;
        requestResult->output_buffers = streamBuffer;
        requestResult->partial_result = 0;

        CLOGV("frame number(%d), #out(%d) streamId(%d)",
                requestResult->frame_number, requestResult->num_output_buffers, streamId);

        m_requestMgr->pushResultRequest(resultRequest);
    }

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    return ret;
}

status_t ExynosCamera::m_sendYuvStreamStallResult(ExynosCameraRequestSP_sprt_t request,
                                                   ExynosCameraBuffer *buffer, int streamId)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ResultRequest resultRequest = NULL;

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from streamMgr", streamId);
        return INVALID_OPERATION;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
            EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d S%d] createResultRequest fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d S%d] getCaptureResult fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d S%d] getStreamBuffer fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    ret = stream->getStream(&(streamBuffer->stream));
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from ExynosCameraStream", streamId);
        return INVALID_OPERATION;
    }

    streamBuffer->buffer = buffer->handle[0];

    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                request->getKey(), request->getFrameCount(), streamId, buffer->index);
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

    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    return ret;
}

status_t ExynosCamera::m_sendThumbnailStreamResult(ExynosCameraRequestSP_sprt_t request,
                                                    ExynosCameraBuffer *buffer, int streamId)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ResultRequest resultRequest = NULL;

    if (buffer == NULL) {
        CLOGE("buffer is NULL");
        return BAD_VALUE;
    }

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from streamMgr", streamId);
        return INVALID_OPERATION;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
            EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d S%d] createResultRequest fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d S%d] getCaptureResult fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d S%d] getStreamBuffer fail.",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    ret = stream->getStream(&(streamBuffer->stream));
    if (ret != NO_ERROR) {
        CLOGE("Failed to get stream %d from ExynosCameraStream", streamId);
        return INVALID_OPERATION;
    }

    streamBuffer->buffer = buffer->handle[0];

    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                request->getKey(), request->getFrameCount(), streamId, buffer->index);
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

    CLOGD("frame number(%d), #out(%d)",
            requestResult->frame_number, requestResult->num_output_buffers);

    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*buffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), buffer->index, ret);
    }

    return ret;
}

#ifdef SUPPORT_DEPTH_MAP
status_t ExynosCamera::m_sendDepthStreamResult(ExynosCameraRequestSP_sprt_t request,
                                                ExynosCameraBuffer *buffer, int streamId)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;
    bool bufferSkip = false;

    /* 1. Get stream object for RAW */
    ret = m_streamManager->getStream(streamId, &stream);
    if (ret < 0) {
        CLOGE("getStream is failed, from streammanager. Id error:(%d)", streamId);
        return ret;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
                                        EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId (%d)",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId (%d)",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId (%d)",
                request->getKey(), request->getFrameCount(), streamId);
        ret = INVALID_OPERATION;
        return ret;
    }

    /* 2. Get camera3_stream structure from stream object */
    ret = stream->getStream(&(streamBuffer->stream));
    if (ret < 0) {
        CLOGE("getStream is failed, from exynoscamerastream. Id error:(%d)", streamId);
        return ret;
    }

    /* 3. Get the service buffer handle */
    streamBuffer->buffer = buffer->handle[0];

    if (buffer->index < 0) {
        bufferSkip = true;
    }

    /* 4. Update the remained buffer info */
    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status, bufferSkip);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                request->getKey(), request->getFrameCount(),
                streamId, buffer->index);
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
            " request->getCompleteBufferCount(%d) frame->getFrameCapture(%d)",
            request->getKey(),
            request->getNumOfOutputBuffer(),
            request->getCompleteBufferCount(),
            request->getFrameCount());

    CLOGV("streamBuffer info: stream (%p), handle(%p)",
            streamBuffer->stream, streamBuffer->buffer);

    return ret;
}
#endif

status_t ExynosCamera::m_sendVisionStreamResult(ExynosCameraRequestSP_sprt_t request, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;
    bool skipPreview = false;
    int option = 0;

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

    if ((option & STREAM_OPTION_IRIS_MASK)
        && m_visionFps == 30) {
        m_displayPreviewToggle = (m_displayPreviewToggle + 1) % 2;
        skipPreview = (m_displayPreviewToggle == 0) ? false : true;

        CLOGV("skip iris frame fcount(%d) skip(%d)", request->getFrameCount(), skipPreview);
    }

    /* 4. Get the service buffer handle from buffer manager */
    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status, skipPreview);
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

status_t ExynosCamera::m_setupVisionPipeline(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("++IN++");

    int ret = 0;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    ExynosCameraBuffer dstBuf;
    uint32_t pipeId = PIPE_FLITE;
    uint32_t nodePipeId = PIPE_VC0;
    enum NODE_TYPE nodeType;

    factory->setRequest(PIPE_VC0, true);

    m_setSetfile();

    nodeType = factory->getNodeType(nodePipeId);

    /* set initial value for Secure Camera*/
    m_shutterSpeed = (int32_t) (m_configurations->getExposureTime() / 100000);
    m_gain = m_configurations->getGain();
    m_irLedWidth = (int32_t) (m_configurations->getLedPulseWidth() / 100000);
    m_irLedDelay = (int32_t) (m_configurations->getLedPulseDelay() / 100000);
    m_irLedCurrent = m_configurations->getLedCurrent();
    m_irLedOnTime = (int32_t) (m_configurations->getLedMaxTime() / 1000);
    m_visionFps = 30;

    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRAME_RATE, m_visionFps, pipeId);
    if (ret < 0)
        CLOGE("FLITE setControl fail, ret(%d)", ret);

    ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
        return ret;
    }

    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId], pipeId);

    CLOGI("--OUT--");

    return NO_ERROR;
}

status_t ExynosCamera::m_setupReprocessingPipeline(ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    uint32_t pipeId = MAX_PIPE_NUM;
    int flipHorizontal = 0;
    enum NODE_TYPE nodeType;

    int cameraId = factory->getCameraId();
    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagMcscVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flag3aaVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    if (flag3aaVraM2M)
        flagMcscVraM2M = false;

    flipHorizontal = m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL);

    /* Setting BufferSupplier basd on H/W pipeline */
    pipeId = PIPE_3AA_REPROCESSING;

    if (flag3aaIspM2M == true) {
        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* TODO : Consider the M2M Reprocessing Scenario */
        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

        pipeId = PIPE_ISP_REPROCESSING;
    }

    /* TODO : Consider the M2M Reprocessing Scenario */
    factory->setFrameDoneQToPipe(m_reprocessingDoneQ, pipeId);

    if (flagIspMcscM2M == true) {
        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* TODO : Consider the M2M Reprocessing Scenario */
        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

        pipeId = PIPE_MCSC_REPROCESSING;
    }

    ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
        return ret;
    }

    /* TODO : Consider the M2M Reprocessing Scenario */
    factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

    if (cameraId == m_cameraId) {
        nodeType = factory->getNodeType(PIPE_MCSC0_REPROCESSING);
        factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);

        nodeType = factory->getNodeType(PIPE_MCSC1_REPROCESSING);
        factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);

        nodeType = factory->getNodeType(PIPE_MCSC2_REPROCESSING);
        factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);


        switch (m_parameters[m_cameraId]->getNumOfMcscOutputPorts()) {
        case 5:
            nodeType = factory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING);
            factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);

            nodeType = factory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING);
            factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);
            break;
        case 3:
            CLOGW("You need to set V4L2_CID_HFLIP on jpegReprocessingFrameFactory");
            break;
        default:
            CLOGE("invalid output port(%d)", m_parameters[m_cameraId]->getNumOfMcscOutputPorts());
            break;
        }
    }

    if (flagMcscVraM2M || flag3aaVraM2M) {
        pipeId = PIPE_VRA_REPROCESSING;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* TODO : Consider the M2M Reprocessing Scenario */
        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);
    }

#ifdef USE_DUAL_CAMERA
    if (cameraId == m_cameraId
        && m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        pipeId = PIPE_SYNC_REPROCESSING;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* Setting OutputFrameQ/FrameDoneQ to Pipe */
        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

        if (m_configurations->getDualReprocessingMode() == DUAL_PREVIEW_MODE_SW) {
            pipeId = PIPE_FUSION_REPROCESSING;

            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* Setting OutputFrameQ/FrameDoneQ to Pipe */
            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);
        }
    }
#endif

#ifdef USE_LLS_REPROCESSING
    if (factory->getFactoryType() == FRAME_FACTORY_TYPE_JPEG_REPROCESSING &&
                    m_parameters[m_cameraId]->getLLSOn() == true) {
            pipeId = PIPE_LLS_REPROCESSING;

            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                    CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                    return ret;
            }

            factory->setOutputFrameQToPipe(m_bayerStreamQ, pipeId);
    }
#endif

    return ret;
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

bool ExynosCamera::m_selectBayerThreadFunc()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;
    status_t ret = NO_ERROR;

    ret = m_selectBayerQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGV("Wait timeout");
        } else {
            CLOGE("Failed to waitAndPopProcessQ. ret %d",
                     ret);
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto CLEAN;
    }

    if (m_selectBayer(frame) == false)
        CLOGW("can't select bayer..(%d)", frame->getFrameCount());

CLEAN:
    if (m_getState() != EXYNOS_CAMERA_STATE_FLUSH && m_selectBayerQ->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}

#ifdef USE_DUAL_CAMERA
bool ExynosCamera::m_selectDualSlaveBayerThreadFunc()
{
    ExynosCameraFrameSP_sptr_t frame = NULL;
    status_t ret = NO_ERROR;

    ret = m_selectDualSlaveBayerQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT) {
            CLOGV("Wait timeout");
        } else {
            CLOGE("Failed to waitAndPopProcessQ. ret %d",
                     ret);
        }
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto CLEAN;
    }

    if (m_selectBayer(frame) == false)
        CLOGW("can't select dual slave bayer..(%d)", frame->getFrameCount());

CLEAN:
    if (m_getState() != EXYNOS_CAMERA_STATE_FLUSH && m_selectDualSlaveBayerQ->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}
#endif

bool ExynosCamera::m_selectBayer(ExynosCameraFrameSP_sptr_t frame)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    camera2_shot_ext *shot_ext = NULL;
    enum FRAME_TYPE frameType;
    uint32_t pipeID = 0;
    uint32_t dstPipeId = 0;
    ExynosCameraRequestSP_sprt_t request = NULL;
    uint64_t captureExposureTime = 0L;
    ExynosCameraDurationTimer timer;
    ExynosCameraFrameFactory *factory = NULL;
    frame_handle_components_t components;
    ExynosCameraActivityAutofocus *autoFocusMgr = NULL;
    ExynosCameraActivitySpecialCapture *sCaptureMgr = NULL;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
#endif
    bool retry = false;
    bayerBuffer.index = -2;

    captureExposureTime = m_configurations->getCaptureExposureTime();
    frameType = (enum FRAME_TYPE) frame->getFrameType();

#ifdef USE_DUAL_CAMERA
    if (frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE)
#endif
    {
        if (!frame->getStreamRequested(STREAM_TYPE_CAPTURE) && !frame->getStreamRequested(STREAM_TYPE_RAW)) {
            CLOGW("[F%d]frame is not capture frame", frame->getFrameCount());
            goto CLEAN;
        }
    }

    CLOGD("[F%d T%d U%d]Start to select Bayer.",
            frame->getFrameCount(), frameType, frame->getUniqueKey());

    if (frameType != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        && frameType != FRAME_TYPE_INTERNAL_SLAVE
#endif
#ifdef FAST_SHUTTER_NOTIFY
        || (frame->getHasRequest() == true
            && frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_COUNT_1)
#endif
        ) {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("getRequest failed ");
            goto CLEAN;
        }
    }

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
    } else
#endif
    {
        if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
            if (frame->getFrameType() == FRAME_TYPE_JPEG_REPROCESSING)
                factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
            else
                factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
        } else {
#ifdef USE_LLS_REPROCESSING
            /* HACK: LLS Capture can be supported to jpeg */
            if (m_parameters[m_cameraId]->getLLSCaptureOn() == true)
                factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
#endif
        }
    }

    getFrameHandleComponents(frameType, &components);
    autoFocusMgr = components.activityControl->getAutoFocusMgr();
    sCaptureMgr = components.activityControl->getSpecialCaptureMgr();

    switch (components.parameters->getReprocessingBayerMode()) {
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        CLOGD("[F%d T%d]PURE_ALWAYS_REPROCESISNG%s Start",
                frame->getFrameCount(), frame->getFrameType(),
                (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) ? "_ZSL" : "");
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        CLOGD("[F%d T%d]PURE_DYNAMIC_REPROCESISNG%s Start",
                frame->getFrameCount(), frame->getFrameType(),
                (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) ? "_ZSL" : "");
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        CLOGD("[F%d T%d]PROCESSED_ALWAYS_REPROCESISNG%s Start",
                frame->getFrameCount(), frame->getFrameType(),
                (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) ? "_ZSL" : "");
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        CLOGD("[F%d T%d]PROCESSED_DYNAMIC_REPROCESISNG%s Start",
                frame->getFrameCount(), frame->getFrameType(),
                (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) ? "_ZSL" : "");
        break;
    default:
        break;
    }

    if (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
        ret = m_getBayerServiceBuffer(frame, &bayerBuffer, request);
    } else {
        do {
            retry = false;

            ret = m_getBayerBuffer(m_getBayerPipeId(),
                    frame->getFrameCount(),
                    &bayerBuffer,
                    components.captureSelector,
                    frameType,
                    bayerFrame
#ifdef SUPPORT_DEPTH_MAP
                    , &depthMapBuffer
#endif
                    );
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to m_getBayerBuffer. ret %d",
                        frame->getFrameCount(), ret);
                goto CLEAN;
            }

            bool flagForceRecovery = false;
            if (components.parameters->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
                flagForceRecovery = true;

            if (bayerBuffer.index >= 0
                    && m_checkValidBayerBufferSize((camera2_stream *) bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()],
                                                    frame, flagForceRecovery) == false) {
                CLOGW("[F%d B%d]Invalid bayer buffer size. Retry.",
                        frame->getFrameCount(), bayerBuffer.index);

                ret = m_bufferSupplier->putBuffer(bayerBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for Pipe(%d)",
                            frame->getFrameCount(), bayerBuffer.index, m_getBayerPipeId());
                    /* Continue */
                }

#ifdef SUPPORT_DEPTH_MAP
                if (m_flagUseInternalDepthMap && depthMapBuffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[B%d]Failed to putBuffer. ret %d", depthMapBuffer.index, ret);
                    }
                    depthMapBuffer.index = -2;
                }
#endif

                retry = true;
            }
        } while(retry == true);
    }
    if (ret != NO_ERROR | bayerBuffer.index < 0) {
        CLOGE("[F%d B%d]Failed to getBayerBuffer. ret %d",
                frame->getFrameCount(), bayerBuffer.index, ret);
        goto CLEAN;
    }

    shot_ext = (struct camera2_shot_ext *) bayerBuffer.addr[bayerBuffer.getMetaPlaneIndex()];
    if (shot_ext == NULL) {
        CLOGE("[F%d B%d]shot_ext is NULL", frame->getFrameCount(), bayerBuffer.index);
        goto CLEAN;
    }

    /* Sync-up metadata in frame and buffer */
    if (frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
        ret = frame->getDynamicMeta(&shot_ext->shot.dm);
        if (ret != NO_ERROR) {
            CLOGE("[F%d(%d) B%d]Failed to getDynamicMetadata. ret %d",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    bayerBuffer.index, ret);
            goto CLEAN;
        }

        ret = frame->getUserDynamicMeta(&shot_ext->shot.udm);
        if (ret != NO_ERROR) {
            CLOGE("[F%d(%d) B%d]Failed to getUserDynamicMetadata. ret %d",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    bayerBuffer.index, ret);
            goto CLEAN;
        }
    } else {
        ret = frame->storeDynamicMeta(shot_ext);
        if (ret < 0) {
            CLOGE("[F%d(%d) B%d]Failed to storeDynamicMeta. ret %d",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    bayerBuffer.index, ret);
            goto CLEAN;
        }

        ret = frame->storeUserDynamicMeta(shot_ext);
        if (ret < 0) {
            CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    bayerBuffer.index, ret);
            goto CLEAN;
        }
    }

    // shot_ext->shot.udm.ae.vendorSpecific[398], 1 : main-flash fired
    // When captureExposureTime is greater than CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT(100msec)
    // and is less than PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX(300msec)
    // and flash is on,
    // RTA can't save captureExposureTime to shot_ext->shot.dm.sensor.exposureTime.
    // So HAL will skip the checking the exposureTime.
    if (!frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
        if (m_configurations->getCaptureExposureTime() != 0
            && m_longExposureRemainCount > 0
            && m_configurations->getLongExposureShotCount() > 1) {
            ExynosRect srcRect, dstRect;
            ExynosCameraBuffer *srcBuffer = NULL;
            ExynosCameraBuffer *dstBuffer = NULL;
            bool isPacked = false;
            components.parameters->getPictureBayerCropSize(&srcRect, &dstRect);

            CLOGD("m_longExposureRemainCount(%d) getLongExposureShotCount()(%d)",
                m_longExposureRemainCount, m_configurations->getLongExposureShotCount());

            if (m_longExposureRemainCount == (unsigned int)m_configurations->getLongExposureShotCount()) {
                /* First Bayer Buffer */
                m_newLongExposureCaptureBuffer = bayerBuffer;
                m_longExposureRemainCount--;
                goto CLEAN_FRAME;
            }

            if (components.parameters->getBayerFormat(PIPE_FLITE) == V4L2_PIX_FMT_SBGGR16) {
                if (m_longExposureRemainCount > 1) {
                    srcBuffer = &bayerBuffer;
                    dstBuffer = &m_newLongExposureCaptureBuffer;
                } else {
                    srcBuffer = &m_newLongExposureCaptureBuffer;
                    dstBuffer = &bayerBuffer;
                }
            } else {
                if (m_longExposureRemainCount > 1) {
                    srcBuffer = &bayerBuffer;
                    dstBuffer = &m_newLongExposureCaptureBuffer;
                    isPacked = true;
                } else {
                    srcBuffer = &m_newLongExposureCaptureBuffer;
                    dstBuffer = &bayerBuffer;
                    isPacked = true;
                }
            }

            ret = addBayerBuffer(srcBuffer, dstBuffer, &dstRect, isPacked);
            if (ret < 0) {
                CLOGE("addBayerBufferPacked() fail");
            }

            if (m_ionClient >= 0)
                exynos_ion_sync_fd(m_ionClient, dstBuffer->fd[0]);

            CLOGD("Selected frame(%d, %d msec) m_longExposureRemainCount(%d)",
                    frame->getFrameCount(),
                    (int) shot_ext->shot.dm.sensor.exposureTime, m_longExposureRemainCount);

            if (m_longExposureRemainCount-- > 1) {
                goto CLEAN;
            }

            ret = m_bufferSupplier->putBuffer(m_newLongExposureCaptureBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for Pipe(%d)",
                    frame->getFrameCount(), m_newLongExposureCaptureBuffer.index, m_getBayerPipeId()) ;
            }
        }
    }

#ifdef FAST_SHUTTER_NOTIFY
    if (frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_COUNT_1) {
        if (m_cameraId == CAMERA_ID_BACK) {
            m_configurations->setMode(CONFIGURATION_FAST_SHUTTER_MODE, true);
            CLOGD("set fast shutter mode on");
        }
#ifdef FRONT_FAST_SHUTTER_NOTIFY_BV_VALUE
        else if (m_cameraId == CAMERA_ID_FRONT) {
            int Bv = shot_ext->shot.udm.internal.vendorSpecific[2];
            CLOGD("Bv %d %f", Bv, (double)Bv/256.f);

            if ((double)Bv/256.f > FRONT_FAST_SHUTTER_NOTIFY_BV_VALUE) {
                m_configurations->setMode(CONFIGURATION_FAST_SHUTTER_MODE, true);
                CLOGD("set fast shutter mode on");
            }
        }
#endif
    }
#endif

    if (frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        && frame->getFrameType() != FRAME_TYPE_INTERNAL_SLAVE
#endif
#ifdef FAST_SHUTTER_NOTIFY
        || (m_configurations->getMode(CONFIGURATION_FAST_SHUTTER_MODE) == true
            && frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_COUNT_1)
#endif
        ) {
        if (request->getNumOfInputBuffer() > 0
            && request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
            ret = m_updateTimestamp(request, frame, &bayerBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d(%d) B%d]Failed to updateTimestamp. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        bayerBuffer.index,
                        ret);
                goto CLEAN;
            }

            ret = m_updateResultShot(request, shot_ext, PARTIAL_3AA);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d(%d) B%d]Failed to updateResultShot. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        bayerBuffer.index,
                        ret);
                goto CLEAN;
            }

            m_sendNotifyShutter(request);
        }
    }

#ifdef SUPPORT_DEPTH_MAP
    if (frame->getStreamRequested(STREAM_TYPE_DEPTH_STALL)) {
        buffer_manager_tag_t bufTag;
        ExynosCameraBuffer serviceBuffer;
        const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
        int32_t index = -1;

        serviceBuffer.index = -2;
        index = request->getBufferIndex(HAL_STREAM_ID_DEPTHMAP_STALL);

        if (index >= 0) {
            const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
            buffer_handle_t *handle = streamBuffer->buffer;

            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
            bufTag.pipeId[0] = PIPE_VC1;
            serviceBuffer.handle[0] = handle;
            serviceBuffer.acquireFence[0] = streamBuffer->acquire_fence;
            serviceBuffer.releaseFence[0] = streamBuffer->release_fence;

            ret = m_bufferSupplier->getBuffer(bufTag, &serviceBuffer);
            if (ret != NO_ERROR || serviceBuffer.index < 0) {
                CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), serviceBuffer.index, ret);
            }

            ret = request->setAcquireFenceDone(handle, (serviceBuffer.acquireFence[0] == -1) ? true : false);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frame->getFrameCount(), serviceBuffer.index, ret);
            }

            if (depthMapBuffer.index >= 0) {
                timer.start();
                memcpy(serviceBuffer.addr[0], depthMapBuffer.addr[0], depthMapBuffer.size[0]);
                if (m_ionClient >= 0)
                    exynos_ion_sync_fd(m_ionClient, serviceBuffer.fd[0]);
                timer.stop();
                CLOGV("memcpy time:(%5d msec)", (int)timer.durationMsecs());
            } else {
                serviceBuffer.index = -2;
            }

            ret = m_sendDepthStreamResult(request, &serviceBuffer, HAL_STREAM_ID_DEPTHMAP_STALL);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to sendRawStreamResult. ret %d",
                        frame->getFrameCount(), serviceBuffer.index, ret);
            }
        }
    }

    if (m_flagUseInternalDepthMap && depthMapBuffer.index >= 0) {
        ret = m_bufferSupplier->putBuffer(depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[B%d]Failed to putBuffer. ret %d",
                    depthMapBuffer.index, ret);
        }
        depthMapBuffer.index = -2;
    }
#endif

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()
        && m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
        bool bRet;
        char filePath[70];
        time_t rawtime;
        struct tm *timeinfo;

        memset(filePath, 0, sizeof(filePath));
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(filePath, sizeof(filePath), "/data/camera/Raw%d_%02d%02d%02d_%02d%02d%02d_%d.raw",
          m_cameraId, timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
          timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, frame->getFrameCount());

        bRet = dumpToFile((char *)filePath,
            bayerBuffer.addr[0],
            bayerBuffer.size[0]);
        if (bRet != true)
            CLOGE("couldn't make a raw file");
    }
#endif /* DEBUG_RAWDUMP */

    pipeID = frame->getFirstEntity()->getPipeId();
    CLOGD("[F%d(%d) B%d]Setup bayerBuffer(frame:%d) for %d",
            frame->getFrameCount(),
            getMetaDmRequestFrameCount(shot_ext),
            bayerBuffer.index,
            (bayerFrame != NULL) ? bayerFrame->getFrameCount() : -1,
            pipeID);

    if (frame->getFrameType() == FRAME_TYPE_INTERNAL)
        goto SKIP_INTERNAL;

    if (pipeID == PIPE_3AA_REPROCESSING
        && components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId = PIPE_3AP_REPROCESSING;
    } else if (components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId = PIPE_ISPC_REPROCESSING;
    } else {
        if (components.parameters->isUseHWFC() == true) {
            ret = m_setHWFCBuffer(pipeID, frame, PIPE_MCSC_JPEG_REPROCESSING, PIPE_HWFC_JPEG_SRC_REPROCESSING);
            if (ret != NO_ERROR) {
                CLOGE("Set HWFC Buffer fail, pipeID(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                        pipeID, PIPE_MCSC_JPEG_REPROCESSING, PIPE_HWFC_JPEG_SRC_REPROCESSING, ret);
                goto CLEAN;
            }

            ret = m_setHWFCBuffer(pipeID, frame, PIPE_MCSC_THUMB_REPROCESSING, PIPE_HWFC_THUMB_SRC_REPROCESSING);
            if (ret != NO_ERROR) {
                CLOGE("Set HWFC Buffer fail, pipeID(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                        pipeID, PIPE_MCSC_THUMB_REPROCESSING, PIPE_HWFC_THUMB_SRC_REPROCESSING, ret);
                goto CLEAN;
            }
        }

        dstPipeId = PIPE_MCSC_JPEG_REPROCESSING;
    }

    if (components.parameters->isUseHWFC() == false
        || dstPipeId != PIPE_MCSC_JPEG_REPROCESSING) {
        if (frame->getRequest(dstPipeId) == true) {
            /* Check available buffer */
            ret = m_checkBufferAvailable(dstPipeId);
            if (ret != NO_ERROR) {
                CLOGE("Waiting buffer for Pipe(%d) timeout. ret %d", dstPipeId, ret);
                goto CLEAN;
            }
        }
    }

SKIP_INTERNAL:
    ret = m_selectBayerHandler(pipeID, frame, &bayerBuffer, bayerFrame, factory);
    if (ret < 0) {
        CLOGE("Failed to m_selectBayerHandler. frameCount %d ret %d",
                frame->getFrameCount(), ret);
        goto CLEAN;
    }

    if (m_captureThread->isRunning() == false) {
        m_captureThread->run(PRIORITY_DEFAULT);
    }

    /* Thread can exit only if timeout or error occured from inputQ (at CLEAN: label) */
    return true;

CLEAN:
    if (bayerBuffer.index >= 0 && !frame->getStreamRequested(STREAM_TYPE_RAW)) {
        ret = m_releaseSelectorTagBuffers(bayerFrame);
        if (ret < 0)
            CLOGE("Failed to releaseSelectorTagBuffers. frameCount %d ret %d",
                    bayerFrame->getFrameCount(), ret);
    }

CLEAN_FRAME:
    if (m_flagUseInternalDepthMap && depthMapBuffer.index >= 0) {
        ret = m_bufferSupplier->putBuffer(depthMapBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[B%d]Failed to putBuffer. ret %d",
                    depthMapBuffer.index, ret);
        }
    }

    if (frame != NULL && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret != NO_ERROR)
            CLOGE("Failed to remove frame from m_captureProcessList. frameCount %d ret %d",
                     frame->getFrameCount(), ret);

        frame->printEntity();
        CLOGD("[F%d]Delete frame from m_captureProcessList.", frame->getFrameCount());
        frame = NULL;
    }

    return false;
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

        m_needDynamicBayerCount = 0;

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

status_t ExynosCamera::m_handleYuvCaptureFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    int pipeId_src = -1;
    int pipeId_gsc = -1;
    int pipeId_jpeg = -1;
    ExynosCameraRequestSP_sprt_t request = NULL;
    frame_handle_components_t components;

    float zoomRatio = 0.0F;
    struct camera2_stream *shot_stream = NULL;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect, dstRect;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    if (components.parameters->isReprocessing() == true) {
        pipeId_src = m_getMcscLeaderPipeId(components.reprocessingFactory);
    }

    if (components.parameters->isUseHWFC() == true
        ) {
        ret = frame->getDstBuffer(pipeId_src, &dstBuffer, components.reprocessingFactory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                    pipeId_src, PIPE_MCSC_JPEG_REPROCESSING, ret);
            return INVALID_OPERATION;
        }

        ret = m_bufferSupplier->putBuffer(dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for MCSC_JPEG_REP. ret %d",
                    frame->getFrameCount(), dstBuffer.index, ret);
            return INVALID_OPERATION;
        }

        if (frame->getRequest(PIPE_MCSC_THUMB_REPROCESSING) == true) {
            ret = frame->getDstBuffer(pipeId_src, &dstBuffer, components.reprocessingFactory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING));
            if (ret != NO_ERROR) {
                if (request == NULL) {
                    CLOGE("[F%d]Failed to getDstBuffer. pipeId %d node %d ret %d",
                        frame->getFrameCount(),
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING, ret);
                } else {
                    CLOGE("[R%d F%d]Failed to getDstBuffer. pipeId %d node %d ret %d",
                        request->getKey(), frame->getFrameCount(),
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING, ret);
                }
                return INVALID_OPERATION;
            }

            ret = m_bufferSupplier->putBuffer(dstBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for MCSC_THUMB_REP. ret %d",
                        frame->getFrameCount(), dstBuffer.index, ret);
                return INVALID_OPERATION;
            }
        }
    } else {
        zoomRatio = m_configurations->getZoomRatio();

        if (frame->getRequest(PIPE_MCSC_THUMB_REPROCESSING) == true) {
            // Thumbnail image is currently not used
            ret = frame->getDstBuffer(pipeId_src, &dstBuffer, components.reprocessingFactory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING, ret);
            } else {
                ret = m_bufferSupplier->putBuffer(dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC_THUMB_REP. ret %d",
                            frame->getFrameCount(), dstBuffer.index, ret);
                }
                CLOGI("[F%d B%d]Thumbnail image disposed at pipeId %d node %d",
                        frame->getFrameCount(), dstBuffer.index,
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING);
            }
        }

        if (components.parameters->needGSCForCapture(getCameraId()) == true) {
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer);
            if (ret < 0)
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_src, ret);

            shot_stream = (struct camera2_stream *)(srcBuffer.addr[srcBuffer.getMetaPlaneIndex()]);
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
                return INVALID_OPERATION;
            }

            /* should change size calculation code in pure bayer */
            m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);
            pictureFormat = components.parameters->getHwPictureFormat();

            srcRect.x = shot_stream->output_crop_region[0];
            srcRect.y = shot_stream->output_crop_region[1];
            srcRect.w = shot_stream->output_crop_region[2];
            srcRect.h = shot_stream->output_crop_region[3];
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
                    2, 2, zoomRatio);

            ret = frame->setSrcRect(pipeId_gsc, &srcRect);
            ret = frame->setDstRect(pipeId_gsc, &dstRect);

            CLOGD("size (%d, %d, %d, %d %d %d)",
                    srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.fullW, srcRect.fullH);
            CLOGD("size (%d, %d, %d, %d %d %d)",
                    dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.fullW, dstRect.fullH);

            ret = m_setupEntity(pipeId_gsc, frame, &srcBuffer, NULL);
            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)",
                         pipeId_jpeg, ret);
            }

            components.reprocessingFactory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_gsc);
            components.reprocessingFactory->pushFrameToPipe(frame, pipeId_gsc);
        } else { /* m_parameters[m_cameraId]->needGSCForCapture(getCameraId()) == false */
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer, components.reprocessingFactory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), pipeId_src, ret);
                return ret;
            }

            ret = frame->setSrcBuffer(pipeId_jpeg, srcBuffer);
            if (ret != NO_ERROR) {
                if (request == NULL) {
                    CLOGE("[F%d]Failed to setSrcBuffer. pipeId %d ret %d",
                         frame->getFrameCount(), pipeId_jpeg, ret);
                } else {
                    CLOGE("[R%d F%d]Failed to setSrcBuffer. pipeId %d ret %d",
                         request->getKey(), frame->getFrameCount(), pipeId_jpeg, ret);
                }
            }

            components.reprocessingFactory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_jpeg);
            components.reprocessingFactory->pushFrameToPipe(frame, pipeId_jpeg);
        }
    }

    return ret;
}

status_t ExynosCamera::m_handleJpegFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId)
{
    status_t ret = NO_ERROR;
    int pipeId_jpeg = -1;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraBuffer buffer;
    int jpegOutputSize = -1;
    exif_attribute_t exifInfo;
    ExynosRect pictureRect;
    ExynosRect thumbnailRect;
    struct camera2_shot_ext *jpeg_meta_shot;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    components.parameters->getFixedExifInfo(&exifInfo);
    pictureRect.colorFormat = components.parameters->getHwPictureFormat();
    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureRect.w, (uint32_t *)&pictureRect.h);
    m_configurations->getSize(CONFIGURATION_THUMBNAIL_SIZE, (uint32_t *)&thumbnailRect.w, (uint32_t *)&thumbnailRect.h);

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("[F%d] request is NULL.", frame->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }

    if (leaderPipeId == PIPE_VRA_REPROCESSING) {
        leaderPipeId = m_getMcscLeaderPipeId(components.reprocessingFactory);
    }

    /* We are using only PIPE_ISP_REPROCESSING */
    if (components.parameters->isReprocessing() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    if (components.parameters->isUseHWFC() == true && leaderPipeId != PIPE_JPEG_REPROCESSING) {
        entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = frame->getDstBufferState(leaderPipeId, &bufferState, components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBufferState. frameCount %d pipeId %d node %d",
                    frame->getFrameCount(),
                    leaderPipeId, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            request->setStreamBufferStatus(HAL_STREAM_ID_JPEG, CAMERA3_BUFFER_STATUS_ERROR);
            CLOGE("Invalid JPEG buffer state. frameCount %d bufferState %d",
                    frame->getFrameCount(), bufferState);
        }

        ret = frame->getDstBuffer(leaderPipeId, &buffer, components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. frameCount %d pipeId %d node %d",
                    frame->getFrameCount(),
                    leaderPipeId, PIPE_HWFC_JPEG_DST_REPROCESSING);
            return INVALID_OPERATION;
        }

        /* get dynamic meters for make exif info */
        jpeg_meta_shot = new struct camera2_shot_ext;
        memset(jpeg_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
        frame->getMetaData(jpeg_meta_shot);
        components.parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, &jpeg_meta_shot->shot, true);

        /*In case of HWFC, overwrite buffer's size to compression's size in ExynosCameraNodeJpeg file.*/
        jpegOutputSize = buffer.size[0];

        /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
        if (buffer.size[0] != 0) {
            /* APP1 Marker - EXIF */
            UpdateExif(buffer.addr[0], buffer.size[0], &exifInfo);
            /* APP4 Marker - DebugInfo */
            UpdateDebugData(buffer.addr[0], buffer.size[0], components.parameters->getDebug2Attribute());
        }

        delete jpeg_meta_shot;
    } else {
        ret = frame->setEntityState(pipeId_jpeg, ENTITY_STATE_FRAME_DONE);
        if (ret < 0) {
            CLOGE("Failed to setEntityState, frameCoutn %d ret %d",
                    frame->getFrameCount(), ret);
            return INVALID_OPERATION;
        }

        m_thumbnailCbThread->join();

        ret = frame->getSrcBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", pipeId_jpeg, ret);
        }

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to putBuffer for JPEG_SRC. ret %d",
                request->getKey(), frame->getFrameCount(), buffer.index, ret);
        }

        entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = frame->getDstBufferState(pipeId_jpeg, &bufferState);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(),
                    pipeId_jpeg);
        } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            request->setStreamBufferStatus(HAL_STREAM_ID_JPEG, CAMERA3_BUFFER_STATUS_ERROR);
            CLOGE("ERR(%s[%d]):Invalid JPEG buffer state. frameCount %d bufferState %d",
                    __FUNCTION__, __LINE__,
                    frame->getFrameCount(),
                    bufferState);
        }

        ret = frame->getDstBuffer(pipeId_jpeg, &buffer);
        if (ret < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId_jpeg, ret);
        }

        jpegOutputSize = frame->getJpegSize();
    }

    CLOGI("Jpeg output done, jpeg size(%d)", jpegOutputSize);

    {
        m_sendJpegStreamResult(request, &buffer, jpegOutputSize);
    }

    return ret;
}

bool ExynosCamera::m_captureStreamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraBuffer serviceBuffer;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext resultShot;
    struct camera2_shot_ext *shot_ext = NULL;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
    frame_handle_components_t components;
    int pipeId = -1;
    int dstPipeId = -1;
    int dstPos = -1;
    bool flag3aaVraOTF;
    bool flagMcscVraOTF = false;

    ret = m_yuvCaptureDoneQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("Failed to wait&pop m_yuvCaptureDoneQ, ret(%d)", ret);
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

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    flag3aaVraOTF = (components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_VRA_REPROCESSING)
                    == HW_CONNECTION_MODE_M2M);
    if (!flag3aaVraOTF) {
        flagMcscVraOTF = (components.parameters->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING)
                    == HW_CONNECTION_MODE_M2M);
    }
    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("Current entity is NULL");
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    pipeId = entity->getPipeId();
    CLOG_PERFRAME(PATH, m_cameraId, m_name, frame.get(), nullptr, frame->getRequestKey(), "pipeId(%d)", pipeId);

    if ((frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        && frame->getFrameType() != FRAME_TYPE_INTERNAL_SLAVE
#endif
        ) && frame->isCompleteForResultUpdate() == false) {
        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d]Failed to get request.", frame->getFrameCount());
            goto FUNC_EXIT2;
        }
    }

    CLOGD("[F%d T%d]YUV capture done. entityID %d, FrameState(%d)",
            frame->getFrameCount(), frame->getFrameType(), entity->getPipeId(), frame->getFrameState());

    switch (entity->getPipeId()) {
    case PIPE_3AA_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
#ifdef USE_HW_RAW_REVERSE_PROCESSING
        /* To let the 3AA reprocessing pipe processing the frame twice */
        if (components.parameters->isUseRawReverseReprocessing() == true &&
            frame->getStreamRequested(STREAM_TYPE_RAW) == true &&
            pipeId == PIPE_3AA_REPROCESSING &&
            frame->getBackupRequest(PIPE_3AC_REPROCESSING) == true) {

            /*
             * Maybe just already another processing except for raw have been finished.
             * Try to check if it is true, push the frame to this pipe again for raw
             */
            if (frame->getRequest(PIPE_3AC_REPROCESSING) == false) {
                /* only set the raw request to true */
                frame->reverseExceptForSpecificReq(PIPE_3AC_REPROCESSING, true);
                ret = frame->setEntityState(pipeId, ENTITY_STATE_REWORK);
                if (ret != NO_ERROR) {
                    CLOGE("Set entity state fail, ret(%d)", ret);
                }

                /* for raw capture setfile index */
                frame->setSetfile(ISS_SUB_SCENARIO_STILL_CAPTURE_DNG_REPROCESSING);

                CLOGD("[F%d] This frame will be processed agrin for raw", frame->getFrameCount());

                components.reprocessingFactory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);
                components.reprocessingFactory->pushFrameToPipe(frame, pipeId);

                /* loop this thread again for processing remained request */
                return true;
            } else {
                CLOGD("[F%d] This frame's processing in this pipe was finished",
                        frame->getFrameCount());
            }

            frame->restoreRequest();
        }
#endif

        if (components.parameters->getUsePureBayerReprocessing() == true
            && components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M
            && entity->getPipeId() == PIPE_ISP_REPROCESSING) {
            ret = frame->getSrcBuffer(frame->getFirstEntity()->getPipeId(), &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                        frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }
            m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }

        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                    frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
        if (ret < 0) {
            CLOGE("[F%d B%d]getSrcBufferState fail. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error. pipeId %d",
                    frame->getFrameCount(), buffer.index, entity->getPipeId());

            if (request != NULL) {
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d B%d] sendNotifyError fail. ret %d",
                            request->getKey(),
                            frame->getFrameCount(),
                            buffer.index, ret);
                    goto FUNC_EXIT;
                }
            }
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            if (m_configurations->getDualReprocessingMode() == DUAL_PREVIEW_MODE_HW) {
                dstPipeId = PIPE_ISPC_REPROCESSING;
            } else if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
                int yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT);
                dstPipeId = yuvStallPort + PIPE_MCSC0_REPROCESSING;
            }

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }

            buffer.index = -2;
            ret = frame->getDstBuffer(PIPE_ISP_REPROCESSING, &buffer, components.reprocessingFactory->getNodeType(dstPipeId));
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_ISP, ret);
            }

            if (buffer.index >= 0) {
                ret = frame->setSrcBuffer(PIPE_SYNC_REPROCESSING, buffer);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                            PIPE_SYNC_REPROCESSING, ret);
                }
            }

            shot_ext = (camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
            if (shot_ext == NULL) {
                CLOGE("[F%d B%d]shot_ext is NULL. pipeId %d",
                        frame->getFrameCount(), buffer.index, entity->getPipeId());
                goto FUNC_EXIT;
            }
            components.parameters->getFaceDetectMeta(shot_ext);

#ifdef DEBUG_FUSION_CAPTURE_DUMP
            {
                bool bRet;
                char filePath[70];

                memset(filePath, 0, sizeof(filePath));
                snprintf(filePath, sizeof(filePath), "/data/camera/Tele%d_F%07d.nv21",
                    frame->getFrameType(), captureNum);

                bRet = dumpToFile((char *)filePath, buffer.addr[0], buffer.size[0]);
                if (bRet != true)
                    CLOGE("couldn't make a raw file");
            }
#endif

            ret = frame->storeDynamicMeta(shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        buffer.index,
                        ret);
                goto FUNC_EXIT;
            }

            dstPipeId = PIPE_HWFC_JPEG_DST_REPROCESSING;
            if (frame->getRequest(dstPipeId) == true) {
                buffer.index = -2;
                ret = frame->getDstBuffer(PIPE_ISP_REPROCESSING, &buffer, components.reprocessingFactory->getNodeType(dstPipeId));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_ISP, ret);
                }

                if (buffer.index >= 0) {
                    ret = frame->setSrcBuffer(PIPE_SYNC_REPROCESSING, buffer, OUTPUT_NODE_2);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                                PIPE_SYNC_REPROCESSING, ret);
                    }
                }
            }

            m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->pushFrameToPipe(frame, PIPE_SYNC_REPROCESSING);
        } else
#endif
        {
            shot_ext = (camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
            if (shot_ext == NULL) {
                CLOGE("[F%d B%d]shot_ext is NULL. pipeId %d",
                        frame->getFrameCount(), buffer.index, entity->getPipeId());
                goto FUNC_EXIT;
            }

            ret = frame->storeDynamicMeta(shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        buffer.index,
                        ret);
                goto FUNC_EXIT;
            }

            ret = frame->storeUserDynamicMeta(shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        buffer.index,
                        ret);
                goto FUNC_EXIT;
            }

            if ((frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
                && frame->getFrameType() != FRAME_TYPE_INTERNAL_SLAVE
#endif
                ) && entity->getPipeId() == PIPE_3AA_REPROCESSING
                && frame->getRequest(PIPE_3AC_REPROCESSING) == true) {

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
                if (components.parameters->isUseRawReverseReprocessing() == true) {
                    /* reverse the raw buffer */
                    m_reverseProcessingBayerQ->pushProcessQ(&frame);
                } else
#endif
                {
                    dstPos = components.reprocessingFactory->getNodeType(PIPE_3AC_REPROCESSING);

                    ret = frame->getDstBuffer(entity->getPipeId(), &serviceBuffer, dstPos);
                    if (ret != NO_ERROR || serviceBuffer.index < 0) {
                        CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                                frame->getFrameCount(), serviceBuffer.index, dstPos, ret);
                        goto FUNC_EXIT;
                    }

                    ret = frame->getDstBufferState(entity->getPipeId(), &bufferState, dstPos);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                                frame->getFrameCount(), buffer.index, dstPos, ret);
                        goto FUNC_EXIT;
                    }

                    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                        CLOGE("[R%d F%d B%d]Invalid RAW buffer state. bufferState %d",
                                request->getKey(), frame->getFrameCount(), serviceBuffer.index, bufferState);
                        request->setStreamBufferStatus(HAL_STREAM_ID_RAW, CAMERA3_BUFFER_STATUS_ERROR);
                    }

                    ret = m_sendRawStreamResult(request, &serviceBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to sendRawStreamResult. ret %d",
                                frame->getFrameCount(), serviceBuffer.index, ret);
                    }
                }
            }

            if (!frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
                m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                }
            }

            CLOGV("[F%d(%d) B%d]3AA_REPROCESSING Done.",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    buffer.index);
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            break;
        } else if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
                /* This is master NV21 src buffer */
                ret = frame->getDstBuffer(pipeId, &buffer, components.reprocessingFactory->getNodeType(PIPE_MCSC2_REPROCESSING));
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            pipeId, ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), pipeId);
                }

                shot_ext = (camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
                if (shot_ext == NULL) {
                    CLOGE("[F%d B%d]shot_ext is NULL. pipeId %d",
                            frame->getFrameCount(), buffer.index, entity->getPipeId());
                    goto FUNC_EXIT;
                }
                components.parameters->getFaceDetectMeta(shot_ext);

#ifdef DEBUG_FUSION_CAPTURE_DUMP
                {
                    bool bRet;
                    char filePath[70];

                    memset(filePath, 0, sizeof(filePath));
                    snprintf(filePath, sizeof(filePath), "/data/camera/Wide%d_F%07d.nv21",
                        frame->getFrameType(), captureNum);

                    bRet = dumpToFile((char *)filePath, buffer.addr[0], buffer.size[0]);
                    if (bRet != true)
                        CLOGE("couldn't make a raw file");
                }
#endif

                ret = frame->storeDynamicMeta(shot_ext);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            buffer.index,
                            ret);
                    goto FUNC_EXIT;
                }


                ret = m_setupEntity(PIPE_FUSION_REPROCESSING, frame, &buffer, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                            PIPE_FUSION_REPROCESSING, ret);
                }

                components.reprocessingFactory->pushFrameToPipe(frame, PIPE_SYNC_REPROCESSING);
                break;
            }
        }
#endif

        if (entity->getPipeId() == PIPE_3AA_REPROCESSING
            && components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            break;
        } else if (entity->getPipeId() == PIPE_ISP_REPROCESSING
                   && components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            dstPipeId = PIPE_ISPC_REPROCESSING;
            if (frame->getRequest(dstPipeId) == true) {
                dstPos = components.reprocessingFactory->getNodeType(dstPipeId);
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            entity->getPipeId(), ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());

                    ret = frame->setSrcBufferState(PIPE_MCSC_REPROCESSING, ENTITY_BUFFER_STATE_ERROR);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d",
                                frame->getFrameCount(), PIPE_VRA, ret);
                    }
                } else {
                    ret = m_setupEntity(PIPE_MCSC_REPROCESSING, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                PIPE_MCSC_REPROCESSING, ret);
                    }
                }

                if (components.parameters->isUseHWFC() == true) {
                    ret = m_setHWFCBuffer(PIPE_MCSC_REPROCESSING, frame, PIPE_MCSC_JPEG_REPROCESSING, PIPE_HWFC_JPEG_SRC_REPROCESSING);
                    if (ret != NO_ERROR) {
                        CLOGE("Set HWFC Buffer fail, pipeId(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                                PIPE_MCSC_REPROCESSING, PIPE_MCSC_JPEG_REPROCESSING, PIPE_HWFC_JPEG_SRC_REPROCESSING, ret);
                    }

                    ret = m_setHWFCBuffer(PIPE_MCSC_REPROCESSING, frame, PIPE_MCSC_THUMB_REPROCESSING, PIPE_HWFC_THUMB_SRC_REPROCESSING);
                    if (ret != NO_ERROR) {
                        CLOGE("Set HWFC Buffer fail, pipeId(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                                PIPE_MCSC_REPROCESSING, PIPE_MCSC_THUMB_REPROCESSING, PIPE_HWFC_THUMB_SRC_REPROCESSING, ret);
                    }
                }
            }

            components.reprocessingFactory->pushFrameToPipe(frame, PIPE_MCSC_REPROCESSING);
            break;
        }

    case PIPE_MCSC_REPROCESSING:
        if (entity->getPipeId() == PIPE_MCSC_REPROCESSING) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                        frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("ERR(%s[%d]):Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                if (request != NULL) {
                    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                    if (ret != NO_ERROR) {
                        CLOGE("[R%d F%d B%d] sendNotifyError fail. ret %d",
                                request->getKey(),
                                frame->getFrameCount(),
                                buffer.index, ret);
                        goto FUNC_EXIT;
                    }
                }
            }

            m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }

        /* Handle yuv capture buffer */
        if (frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleYuvCaptureFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }
        }

#ifdef USE_DUAL_CAMERA
        /* HACK: Slave preview has no VRA */
        if (m_configurations->getDualOperationMode() != DUAL_OPERATION_MODE_SLAVE
            && flagMcscVraOTF)
#else
        if (flagMcscVraOTF)
#endif
        {
            if (frame->getRequest(PIPE_VRA_REPROCESSING) == true) {
                /* Send the Yuv buffer to VRA Pipe */
                dstPos = components.reprocessingFactory->getNodeType(PIPE_MCSC5_REPROCESSING);

                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            entity->getPipeId(), ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), entity->getPipeId());

                    ret = frame->setSrcBufferState(PIPE_VRA_REPROCESSING, ENTITY_BUFFER_STATE_ERROR);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d]Failed to setSrcBufferState into ERROR. pipeId %d ret %d",
                                frame->getFrameCount(), PIPE_VRA, ret);
                    }
                } else {
                    ret = m_setupEntity(PIPE_VRA_REPROCESSING, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                PIPE_VRA_REPROCESSING, ret);
                    }
                }

                components.reprocessingFactory->pushFrameToPipe(frame, PIPE_VRA_REPROCESSING);
                break;
            }
        }

    case PIPE_VRA_REPROCESSING:
        if (entity->getPipeId() == PIPE_VRA_REPROCESSING) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                        entity->getPipeId(), ret);
                return ret;
            }

            ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", entity->getPipeId(), ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for VRA. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
            break;
        }

        if (frame->getFrameYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) {
            /* Handle NV21 capture callback buffer */
            if (frame->getRequest(PIPE_MCSC0_REPROCESSING) == true
                || frame->getRequest(PIPE_MCSC1_REPROCESSING) == true
                || frame->getRequest(PIPE_MCSC2_REPROCESSING) == true) {
                ret = m_handleNV21CaptureFrame(frame, entity->getPipeId());
                if (ret != NO_ERROR) {
                    CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d",
                            entity->getPipeId(), ret);
                    goto FUNC_EXIT;
                }
            }
        }

        /* Continue to JPEG processing stage in HWFC mode */
        if (components.parameters->isUseHWFC() == false
            || frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == false) {
            break;
        }

    case PIPE_GSC_REPROCESSING2:
    case PIPE_JPEG_REPROCESSING:
        /* Handle JPEG buffer */
        if (entity->getSrcBufState() != ENTITY_BUFFER_STATE_ERROR
		    && (pipeId == PIPE_JPEG_REPROCESSING || frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true)) {
            ret = m_handleJpegFrame(frame, entity->getPipeId());
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleJpegFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }
        }
        break;

#ifdef USE_SLSI_PLUGIN
    case PIPE_PLUGIN_POST1_REPROCESSING:
        m_handleNV21CaptureFrame(frame, entity->getPipeId());
        break;
#endif
#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_REPROCESSING:
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            if (m_configurations->getDualReprocessingMode() == DUAL_PREVIEW_MODE_SW) {
                if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                    ret = frame->getDstBuffer(pipeId, &buffer);
                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    } else {
                        ret = frame->setSrcBuffer(PIPE_FUSION_REPROCESSING, buffer, OUTPUT_NODE_2);
                        if (ret != NO_ERROR) {
                            CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                                    PIPE_FUSION_REPROCESSING, ret);
                        }
                    }
                } else if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                    ret = frame->getDstBuffer(pipeId, &buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                                pipeId, ret);
                    }

                    if (buffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(buffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d T%d B%d]Failed to putBuffer for PIPE_SYNC_REPROCESSING. ret %d",
                                    frame->getFrameCount(), frame->getFrameType(), buffer.index, ret);
                        }
                    }
                }
                components.reprocessingFactory->pushFrameToPipe(frame, PIPE_FUSION_REPROCESSING);
            }
        } else if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                ret = frame->getSrcBuffer(pipeId, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                            pipeId, ret);
                }

                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d T%d B%d]Failed to putBuffer for PIPE_SYNC_REPROCESSING. ret %d",
                                frame->getFrameCount(), frame->getFrameType(), buffer.index, ret);
                    }
                }
            } else {
                /* my life is over anymore in dual slave frame */
                frame->setFrameState(FRAME_STATE_COMPLETE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
                frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                        ExynosCameraFrame::RESULT_UPDATE_STATUS_NONE);
            }
        }
        break;
    case PIPE_FUSION_REPROCESSING:
        if (entity->getPipeId() == PIPE_FUSION_REPROCESSING) {
            if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
                if (m_configurations->getDualReprocessingMode() == DUAL_PREVIEW_MODE_SW) {
                    CLOGD("PIPE_FUSION_REPROCESSING");
                    ret = frame->getSrcBuffer(entity->getPipeId(), &buffer, OUTPUT_NODE_2);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)",
                                entity->getPipeId(), ret);
                    }

                    if (buffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(buffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d T%d B%d]Failed to putBuffer for PIPE_FUSION_REPROCESSING. ret %d",
                                    frame->getFrameCount(), frame->getFrameType(), buffer.index, ret);
                        }
                    }

                    ret = m_handleNV21CaptureFrame(frame, entity->getPipeId());
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d",
                                entity->getPipeId(), ret);
                        goto FUNC_EXIT;
                    }
                }
            }
        }
        break;
#endif //USE_DUAL_CAMERA

    default:
        CLOGE("Invalid pipeId %d", entity->getPipeId());
        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
             entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

FUNC_EXIT:
    if (frame != NULL && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        if (frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL) == true
            && frame->getFrameType() != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
            && frame->getFrameType() != FRAME_TYPE_INTERNAL_SLAVE
            && frame->getFrameType() != FRAME_TYPE_REPROCESSING_DUAL_SLAVE
#endif
           ) {
            if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA) == false
                && request->getNumOfInputBuffer() > 0) {
                ret = frame->getMetaData(&resultShot);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
                }

                ret = m_updateResultShot(request, &resultShot, PARTIAL_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d(%d)]Failed to pushResult. ret %d",
                            request->getKey(),
                            frame->getFrameCount(),
                            resultShot.shot.dm.request.frameCount,
                            ret);
                }

                m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
            }

            frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL,
                                            ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
        }

        if (frame->isReadyForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL) == true) {
            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
            }

            ret = m_updateResultShot(request, &resultShot, PARTIAL_NONE, (frame_type_t)frame->getFrameType());
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d(%d)]Failed to m_updateResultShot. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        resultShot.shot.dm.request.frameCount,
                        ret);
            }

            ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to sendMeta. ret %d",
                        request->getKey(),
                        frame->getFrameCount(),
                        ret);
            }

            frame->setStatusForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL,
                                            ExynosCameraFrame::RESULT_UPDATE_STATUS_DONE);
        }

        if (frame->isComplete() == true) {
            m_needDynamicBayerCount = 0;

#ifdef USE_DUAL_CAMERA
            if (m_configurations->getMode(CONFIGURATION_DUAL_MODE)) {
                if ((frame->isSlaveFrame() == true) && (android_atomic_or(0, &m_needSlaveDynamicBayerCount) > 0))
                    android_atomic_and(0, &m_needSlaveDynamicBayerCount);
            }
#endif
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
    }

FUNC_EXIT2:
    {
        Mutex::Autolock l(m_captureProcessLock);
        if (m_captureProcessList.size() > 0)
            return true;
        else {
            return false;
        }
    }
}

status_t ExynosCamera::m_handleNV21CaptureFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer dstBuffer;
    frame_handle_components_t components;

    int nodePipeId = -1;
    int streamId = -1;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    if (leaderPipeId == PIPE_VRA_REPROCESSING) {
        leaderPipeId = m_getMcscLeaderPipeId(components.reprocessingFactory);
    }

    if (leaderPipeId == PIPE_FUSION_REPROCESSING) {
        nodePipeId = leaderPipeId;
    } else
    {
        nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL) % ExynosCameraParameters::YUV_MAX + PIPE_MCSC0_REPROCESSING;
    }

    if (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) && frame->getNumRemainPipe() == 1) {
        ExynosCameraRequestSP_sprt_t request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d]request is NULL.", frame->getFrameCount());
        }

        if (frame->getStreamRequested(STREAM_TYPE_THUMBNAIL_CB)) {
            m_resizeToDScaledYuvStall(frame, leaderPipeId, nodePipeId);

            if (m_thumbnailCbThread->isRunning()) {
                m_thumbnailCbThread->join();
                CLOGD("m_thumbnailCbThread join");
            }

            m_thumbnailCbThread->run();
            m_thumbnailCbQ->pushProcessQ(&frame);

            m_thumbnailCbThread->join();

            ExynosCameraBuffer srcBuffer;
            ret = frame->getDstBuffer(PIPE_GSC_REPROCESSING3, &srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]getDstBuffer fail. ret(%d)",
                        frame->getFrameCount(), srcBuffer.index, ret);
            }

            ret = m_bufferSupplier->putBuffer(srcBuffer);
            if (ret < 0) {
                CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                        frame->getFrameCount(), srcBuffer.index, ret);
            }
        }

        /* PIPE_MCSC 0, 1, 2 */
        for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
            streamId = m_streamManager->getYuvStreamId(ExynosCameraParameters::YUV_STALL_0 + i);
            nodePipeId = i + PIPE_MCSC0_REPROCESSING;

            if (frame->getRequest(nodePipeId) == false) {
                continue;
            }

            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
            ret = frame->getDstBufferState(leaderPipeId, &bufferState, components.reprocessingFactory->getNodeType(nodePipeId));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(),
                        nodePipeId);
                return ret;
            } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                if (request != NULL) {
                    request->setStreamBufferStatus(streamId, CAMERA3_BUFFER_STATUS_ERROR);
                    CLOGE("ERR(%s[%d]):Invalid JPEG buffer state. frameCount %d bufferState %d",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(),
                            bufferState);
                }
            }

            ret = frame->getDstBuffer(leaderPipeId, &dstBuffer, components.reprocessingFactory->getNodeType(nodePipeId));
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to getDstBuffer."
                        " pipeId %d streamId %d ret %d",
                        frame->getFrameCount(),
                        dstBuffer.index,
                        nodePipeId, streamId, ret);
                return ret;
            }

            if (request != NULL) {
                struct camera2_shot_ext *shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];

                ret = m_updateJpegPartialResultShot(request, shot_ext);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to m_updateJpegPartialResultShot");
                }

                {
                    ret = m_sendYuvStreamStallResult(request, &dstBuffer, streamId);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to sendYuvStreamStallResult."
                                " pipeId %d streamId %d ret %d",
                                frame->getFrameCount(),
                                dstBuffer.index,
                                nodePipeId, streamId, ret);
                        return ret;
                    }
                }
            } else {
                if (dstBuffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(dstBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                                frame->getFrameCount(), dstBuffer.index, ret);
                        break;
                    }
                }
            }
        }
    } else {
        if (m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
            int pipeId_next = -1;

#ifdef USE_SLSI_PLUGIN
            ret = m_handleCaptureFramePlugin(frame, leaderPipeId, pipeId_next, nodePipeId);
#endif

            if (frame->getStreamRequested(STREAM_TYPE_THUMBNAIL_CB)
                && pipeId_next == PIPE_JPEG_REPROCESSING) {
                m_resizeToDScaledYuvStall(frame, leaderPipeId, nodePipeId);

                if (m_thumbnailCbThread->isRunning()) {
                    m_thumbnailCbThread->join();
                    CLOGD("m_thumbnailCbThread join");
                }

                m_thumbnailCbThread->run();
                m_thumbnailCbQ->pushProcessQ(&frame);

                m_thumbnailCbThread->join();

                ExynosCameraBuffer srcBuffer;
                ret = frame->getDstBuffer(PIPE_GSC_REPROCESSING3, &srcBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]getDstBuffer fail. ret(%d)",
                            frame->getFrameCount(), srcBuffer.index, ret);
                }

                ret = m_bufferSupplier->putBuffer(srcBuffer);
                if (ret < 0) {
                    CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), srcBuffer.index, ret);
                }
            }

            components.reprocessingFactory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_next);
            components.reprocessingFactory->pushFrameToPipe(frame, pipeId_next);
            if (components.reprocessingFactory->checkPipeThreadRunning(pipeId_next) == false) {
                components.reprocessingFactory->startThread(pipeId_next);
            }
        }
    }

    return ret;
}

status_t ExynosCamera::m_resizeToDScaledYuvStall(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId, int nodePipeId)
{
    status_t ret = NO_ERROR;
    int outputSizeW, outputSizeH;
    ExynosRect srcRect, dstRect;
    int gscPipeId = PIPE_GSC_REPROCESSING3;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    int waitCount = 0;
    frame_handle_components_t components;
    struct camera2_stream *shot_stream = NULL;
    ExynosCameraBuffer buffer;
    buffer_manager_tag_t bufTag;

    srcBuffer.index = -2;
    dstBuffer.index = -2;

    CLOGD("[F%d]-- IN --", frame->getFrameCount());

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    m_configurations->getSize(CONFIGURATION_DS_YUV_STALL_SIZE, (uint32_t *)&outputSizeW, (uint32_t *)&outputSizeH);

    ret = frame->getDstBuffer(leaderPipeId, &srcBuffer, components.reprocessingFactory->getNodeType(nodePipeId));
    if (ret != NO_ERROR) {
        CLOGE("[F%d]getDstBuffer fail. pipeId (%d) ret(%d)",
                frame->getFrameCount(), nodePipeId, ret);
        goto CLEAN;
    }

    shot_stream = (struct camera2_stream *)(srcBuffer.addr[srcBuffer.getMetaPlaneIndex()]);
    if (shot_stream != NULL) {
        CLOGV("(%d %d %d %d)",
                shot_stream->fcount,
                shot_stream->rcount,
                shot_stream->findex,
                shot_stream->fvalid);
        CLOGV("(%d %d %d %d)(%d %d %d %d)",
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
        return INVALID_OPERATION;
    }

    bufTag.pipeId[0] = gscPipeId;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->getBuffer(bufTag, &dstBuffer);
    if (ret != NO_ERROR || dstBuffer.index < 0) {
        CLOGE("[F%d B%d]Failed to get InternalNV21Buffer. ret %d",
                frame->getFrameCount(), dstBuffer.index, ret);
    }

    {
        srcRect.x = shot_stream->output_crop_region[0];
        srcRect.y = shot_stream->output_crop_region[1];
        srcRect.w = shot_stream->output_crop_region[2];
        srcRect.h = shot_stream->output_crop_region[3];
        srcRect.fullW = shot_stream->output_crop_region[2];
        srcRect.fullH = shot_stream->output_crop_region[3];
        srcRect.colorFormat = V4L2_PIX_FMT_NV21;
    }

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = outputSizeW;
    dstRect.h = outputSizeH;
    dstRect.fullW = outputSizeW;
    dstRect.fullH = outputSizeH;
    dstRect.colorFormat = V4L2_PIX_FMT_NV21;

    CLOGV("srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)",
            srcBuffer.index, dstBuffer.index,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

    frame->setSrcRect(gscPipeId, srcRect);
    frame->setDstRect(gscPipeId, dstRect);

    ret = m_setupEntity(gscPipeId, frame, &srcBuffer, &dstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", gscPipeId, ret);
    }

    components.reprocessingFactory->setOutputFrameQToPipe(m_resizeDoneQ, gscPipeId);
    components.reprocessingFactory->pushFrameToPipe(frame, gscPipeId);

    /* wait PIPE_GSC_REPROCESSING3 done */
    CLOGV("wait PIPE_GSC_REPROCESSING3 output");
    waitCount = 0;
    frame = NULL;
    dstBuffer.index = -2;
    do {
        ret = m_resizeDoneQ->waitAndPopProcessQ(&frame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100);

    if (ret < 0) {
        CLOGW("Failed to waitAndPopProcessQ. ret %d waitCount %d", ret, waitCount);
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto CLEAN;
    }

CLEAN:
    if (frame != NULL) {
        CLOGD("frame delete. framecount %d", frame->getFrameCount());
        frame = NULL;
    }

    CLOGD("--OUT--");

    return ret;
}

status_t ExynosCamera::m_setBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;
    int maxBufferCount = 1;
    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    int dsWidth  = 0, dsHeight  = 0;
    int pipeId = -1;
    ExynosRect bdsRect;
    bool flagFlite3aaM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    bool flagUsePostVC0 = false;

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);
        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&maxSensorW, (uint32_t *)&maxSensorH);
    CLOGI("HW Sensor MAX width x height = %dx%d",
            maxSensorW, maxSensorH);
    m_parameters[m_cameraId]->getPreviewBdsSize(&bdsRect, false);
    CLOGI("Preview BDS width x height = %dx%d",
            bdsRect.w, bdsRect.h);

    if (m_configurations->getDynamicMode(DYNAMIC_HIGHSPEED_RECORDING_MODE) == true) {
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_PREVIEW_SIZE, (uint32_t *)&maxPreviewW, (uint32_t *)&maxPreviewH);
        CLOGI("PreviewSize(HW - Highspeed) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
        maxSensorW = maxPreviewW;
        maxSensorH = maxPreviewH;
    } else {
        m_parameters[m_cameraId]->getSize(HW_INFO_MAX_PREVIEW_SIZE, (uint32_t *)&maxPreviewW, (uint32_t *)&maxPreviewH);
        CLOGI("PreviewSize(Max) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    }

    dsWidth = MAX_VRA_INPUT_WIDTH;
    dsHeight = MAX_VRA_INPUT_HEIGHT;
    CLOGI("DS width x height = %dx%d", dsWidth, dsHeight);

    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true
        || flagFlite3aaM2M == true) {
        pipeId = PIPE_VC0;
    } else {
        pipeId = PIPE_3AC;
    }

    /* FLITE */
    bufTag = initBufTag;
    bufTag.pipeId[0] = pipeId;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
    if (flagFlite3aaM2M == true) {
        bufTag.pipeId[1] = PIPE_3AA;
    }

    ret = m_bufferSupplier->createBufferManager("FLITE_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create FLITE_BUF. ret %d", ret);
    }

    if (m_configurations->getMode(CONFIGURATION_PIP_MODE) == true && getCameraId() == CAMERA_ID_FRONT) {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    } else {
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_sensor_buffers;
    }

    if (m_parameters[m_cameraId]->getSensorControlDelay() == 0) {
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            maxBufferCount -= (SENSOR_REQUEST_DELAY * 2);
        } else
#endif
        {
            maxBufferCount -= SENSOR_REQUEST_DELAY;
        }
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
#ifdef CAMERA_PACKED_BAYER_ENABLE
    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true
        || flagUsePostVC0 ) {
        bufConfig.bytesPerLine[0] = getBayerLineSize(maxSensorW, m_parameters[m_cameraId]->getBayerFormat(PIPE_VC0));
    } else {
        bufConfig.bytesPerLine[0] = getBayerLineSize(maxSensorW, m_parameters[m_cameraId]->getBayerFormat(PIPE_3AC));
    }
    bufConfig.size[0] = bufConfig.bytesPerLine[0] * maxSensorH;
#else
    bufConfig.bytesPerLine[0] = maxSensorW * 2;
    bufConfig.size[0] = bufConfig.bytesPerLine[0] * maxSensorH;
#endif
    bufConfig.reqBufCount = maxBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
#ifdef DEBUG_RAWDUMP
    bufConfig.needMmap = true;
#else
    if (flagUsePostVC0)
        bufConfig.needMmap = true;
    else
        bufConfig.needMmap = false;
#endif
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_BACK) {
        bufConfig.reservedMemoryCount = RESERVED_NUM_BAYER_BUFFERS;
        if (m_rawStreamExist || flagUsePostVC0)
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
        else
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else if (getCameraId() == CAMERA_ID_FRONT
                && m_configurations->getMode(CONFIGURATION_PIP_MODE) == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_BAYER_BUFFERS;
        if (m_rawStreamExist || flagUsePostVC0)
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
        else
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else
#endif
    {
        if (m_rawStreamExist || flagUsePostVC0)
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        else
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    }

#ifdef USE_LLS_REPROCESSING
    if (m_parameters[m_cameraId]->getLLSOn() == true) {
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        bufConfig.needMmap = true;
    }
#endif

#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_FLITE, BUF_TYPE_NORMAL);
    if (ret != NO_ERROR) {
        CLOGE("Failed to add FLITE_BUF. ret %d", ret);
        return ret;
    }
#else
    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc FLITE_BUF. ret %d", ret);
        return ret;
    }
#endif

#ifdef DEBUG_RAWDUMP
    /* For RAW_DUMP with pure bayer,
     * another buffer manager is required on processed bayer reprocessing mode.
     */
    if (bufTag.pipeId[0] != PIPE_VC0) {
        /* VC0 for RAW_DUMP */
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_VC0;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("RAW_DUMP_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create RAW_DUMP_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = 2;
        bufConfig.bytesPerLine[0] = getBayerLineSize(maxSensorW, m_parameters[m_cameraId]->getBayerFormat(PIPE_VC0));
        bufConfig.size[0] = bufConfig.bytesPerLine[0] * maxSensorH;
        bufConfig.reqBufCount = maxBufferCount;
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = true;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc RAW_DUMP_BUF. ret %d", ret);
            return ret;
        }
    }
#endif

    /* 3AA */
    bufTag = initBufTag;
    if (flagFlite3aaM2M == true) {
        bufTag.pipeId[0] = PIPE_FLITE;
    } else {
        bufTag.pipeId[0] = PIPE_3AA;
    }
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("3AA_IN_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create 3AA_IN_BUF. ret %d", ret);
    }

    maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    if (m_parameters[m_cameraId]->getSensorControlDelay() == 0) {
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            maxBufferCount -= (SENSOR_REQUEST_DELAY * 2);
        } else
#endif
        {
            maxBufferCount -= SENSOR_REQUEST_DELAY;
        }
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
    bufConfig.size[0] = 32 * 64 * 2;
    bufConfig.reqBufCount = maxBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
    bufConfig.reservedMemoryCount = 0;

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc 3AA_BUF. ret %d", ret);
        return ret;
    }

#ifdef SUPPORT_ME
    if ( (m_configurations->getMode(CONFIGURATION_GMV_MODE) == true)
            || (m_parameters[m_cameraId]->getLLSOn() == true)
#ifdef USES_SW_VDIS
            || (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true)
#endif
    ){
        int meWidth = 0, meHeight = 0;

        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_ME;
        bufTag.pipeId[1] = PIPE_GMV;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("ME_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create ME_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = 2;
        m_parameters[m_cameraId]->getMeSize(&meWidth, &meHeight);
        bufConfig.size[0] = meWidth * meHeight * 2;
        bufConfig.reqBufCount = maxBufferCount;
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = true;
        bufConfig.reservedMemoryCount = 0;

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc ME_BUF. ret %d", ret);
            return ret;
        }
    }
#endif

    /* ISP */
    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_3AP;
    bufTag.pipeId[1] = PIPE_ISP;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("ISP_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create ISP_BUF. ret %d", ret);
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
#if defined (USE_ISP_BUFFER_SIZE_TO_BDS)
    bufConfig.bytesPerLine[0] = getBayerLineSize(bdsRect.w, m_parameters[m_cameraId]->getBayerFormat(PIPE_ISP));
    bufConfig.size[0] = bufConfig.bytesPerLine[0] * bdsRect.h;
#else
    bufConfig.bytesPerLine[0] = getBayerLineSize(maxPreviewW, m_parameters[m_cameraId]->getBayerFormat(PIPE_ISP));
    bufConfig.size[0] = bufConfig.bytesPerLine[0] * maxPreviewH;
#endif
    bufConfig.reqBufCount = maxBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
#if defined(RESERVED_MEMORY_ENABLE) && defined(USE_ISP_BUFFER_SIZE_TO_BDS)
    if (getCameraId() == CAMERA_ID_BACK) {
        if (m_configurations->getDynamicMode(DYNAMIC_UHD_RECORDING_MODE) == true) {
            bufConfig.reservedMemoryCount = RESERVED_NUM_ISP_BUFFERS_ON_UHD;
        } else {
            bufConfig.reservedMemoryCount = RESERVED_NUM_ISP_BUFFERS;
        }
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else if (getCameraId() == CAMERA_ID_FRONT
            && m_configurations->getMode(CONFIGURATION_PIP_MODE) == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_ISP_BUFFERS;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else
#endif
    {
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    }


#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_ISP, BUF_TYPE_NORMAL);
    if (ret != NO_ERROR) {
     CLOGE("Failed to add ISP_BUF. ret %d", ret);
     return ret;
    }
#else
    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc ISP_BUF. ret %d", ret);
        return ret;
    }
#endif

#ifdef SUPPORT_HW_GDC
    /* GDC */
    int videoOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
    if (videoOutputPortId > -1
        ) {
        ExynosRect videoRect;
        int videoFormat = m_configurations->getYuvFormat(videoOutputPortId);
        m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&videoRect.w, (uint32_t *)&videoRect.h, videoOutputPortId);

        bufTag = initBufTag;
        bufTag.pipeId[0] = (videoOutputPortId % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;
        bufTag.pipeId[1] = PIPE_GDC;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("GDC_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create GDC_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = getYuvPlaneCount(videoFormat) + 1;
        getYuvPlaneSize(videoFormat, bufConfig.size, videoRect.w, videoRect.h);
        bufConfig.reqBufCount = 5;
        bufConfig.allowedMaxBufCount = 5;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = false;
        bufConfig.reservedMemoryCount = 0;

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc GDC_BUF. ret %d", ret);
            return ret;
        }
    }
#endif

    if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
        /* VRA */
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_MCSC5;
        bufTag.pipeId[1] = PIPE_MCSC5_REPROCESSING;
        bufTag.pipeId[2] = PIPE_VRA;
        bufTag.pipeId[3] = PIPE_VRA_REPROCESSING;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("VRA_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create VRA_BUF. ret %d", ret);
        }

        maxBufferCount = m_exynosconfig->current->bufInfo.num_vra_buffers;

        bufConfig = initBufConfig;
        switch (m_parameters[m_cameraId]->getHwVraInputFormat()) {
            case V4L2_PIX_FMT_NV16M:
            case V4L2_PIX_FMT_NV61M:
                bufConfig.planeCount = 3;
                bufConfig.size[0] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN) * dsHeight;
                bufConfig.size[1] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN) * dsHeight;
                break;
            case V4L2_PIX_FMT_NV16:
            case V4L2_PIX_FMT_NV61:
                bufConfig.planeCount = 2;
                bufConfig.size[0] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN) * dsHeight * 2;
                break;
            default:
                bufConfig.planeCount = 2;
                bufConfig.size[0] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN) * dsHeight * 2;
                break;
        }
        bufConfig.reqBufCount = maxBufferCount - 2;
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = true;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_VRA, BUF_TYPE_NORMAL);
        if (ret != NO_ERROR) {
            CLOGE("Failed to add VRA_BUF. ret %d", ret);
            return ret;
        }
#else
        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc VRA_BUF. ret %d", ret);
            return ret;
        }
#endif
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                  && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
            if (previewOutputPortId > -1) {
                ExynosRect previewRect;
                int previewFormat = m_configurations->getYuvFormat(previewOutputPortId);
                m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewRect.w, (uint32_t *)&previewRect.h, previewOutputPortId);

                bufTag = initBufTag;
                bufTag.pipeId[0] = (previewOutputPortId % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;
                bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

                ret = m_bufferSupplier->createBufferManager("FUSION_BUF", m_ionAllocator, bufTag);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create FUSION_BUF. ret %d", ret);
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.dual_num_fusion_buffers;

                bufConfig = initBufConfig;
                bufConfig.planeCount = getYuvPlaneCount(previewFormat) + 1;
                getYuvPlaneSize(previewFormat, bufConfig.size, previewRect.w, previewRect.h);
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.startBufIndex = 0;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.needMmap = true;
                bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
                ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_FUSION, BUF_TYPE_NORMAL);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to add FUSION_BUF. ret %d", ret);
                    return ret;
                }
#else
                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc FUSION_BUF. ret %d", ret);
                    return ret;
                }
#endif
            }
        }
    }
#endif

    ret = m_setVendorBuffers();
    if (ret != NO_ERROR) {
        CLOGE("Failed to m_setVendorBuffers. ret %d", ret);
        return ret;
    }

    CLOGI("alloc buffer done - camera ID: %d", m_cameraId);
    return ret;
}

status_t ExynosCamera::m_setVisionBuffers(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;
    int maxBufferCount = 1;
    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    ExynosRect bdsRect;

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);
        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&maxSensorW, (uint32_t *)&maxSensorH);
    CLOGI("HW Sensor MAX width x height = %dx%d",
            maxSensorW, maxSensorH);
    m_parameters[m_cameraId]->getPreviewBdsSize(&bdsRect);
    CLOGI("Preview BDS width x height = %dx%d",
            bdsRect.w, bdsRect.h);

    if (m_configurations->getDynamicMode(DYNAMIC_HIGHSPEED_RECORDING_MODE) == true) {
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_PREVIEW_SIZE, (uint32_t *)&maxPreviewW, (uint32_t *)&maxPreviewH);
        CLOGI("PreviewSize(HW - Highspeed) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    } else {
        m_parameters[m_cameraId]->getSize(HW_INFO_MAX_PREVIEW_SIZE, (uint32_t *)&maxPreviewW, (uint32_t *)&maxPreviewH);
        CLOGI("PreviewSize(Max) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    }

    /* 3AA */
    bufTag = initBufTag;
    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true
        || m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M) {
        bufTag.pipeId[0] = PIPE_FLITE;
    } else {
        bufTag.pipeId[0] = PIPE_3AA;
    }
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("3AA_IN_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create 3AA_IN_BUF. ret %d", ret);
    }

    maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    if (m_parameters[m_cameraId]->getSensorControlDelay() == 0) {
        maxBufferCount -= SENSOR_REQUEST_DELAY;
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
    bufConfig.size[0] = 32 * 64 * 2;
    bufConfig.reqBufCount = maxBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
    bufConfig.reservedMemoryCount = 0;

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc 3AA_BUF. ret %d", ret);
        return ret;
    }

    CLOGI("alloc buffer done - camera ID: %d", m_cameraId);

    return NO_ERROR;
}

status_t ExynosCamera::m_setupBatchFactoryBuffers(ExynosCameraRequestSP_sprt_t request,
                                                  ExynosCameraFrameSP_sptr_t frame,
                                                  ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    int streamId = -1;
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    enum pipeline controlPipeId = (enum pipeline) components.parameters->getPerFrameControlPipe();
    int batchSize = components.parameters->getBatchSize(controlPipeId);

    bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);

    for (int batchIndex = 0; batchIndex < batchSize; batchIndex++) {
        if (batchIndex > 0
            && m_getSizeFromRequestList(&m_requestPreviewWaitingList, &m_requestPreviewWaitingLock) > 0) {
            /* Use next request to get stream buffers */
            Mutex::Autolock l(m_requestPreviewWaitingLock);
            List<ExynosCameraRequestSP_sprt_t>::iterator r = m_requestPreviewWaitingList.begin();
            request = *r;
            m_requestPreviewWaitingList.erase(r);

            ExynosCameraRequestSP_sprt_t serviceRequest = NULL;
            m_popServiceRequest(serviceRequest);
            serviceRequest = NULL;
        } else if (batchIndex > 0) {
            request = NULL;
        }

        if (request == NULL) {
            CLOGW("[R%d F%d]Not enough request for batch buffer. batchIndex %d batchSize %d",
                    request->getKey(), frame->getFrameCount(), batchIndex, batchSize);
            /* Skip to set stream buffer for this batch index. This is Frame Drop */
            return INVALID_OPERATION;
        }

        m_frameCountLock.lock();
        if (request->getFrameCount() == 0) {
           m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
        }
        m_frameCountLock.unlock();

        const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
        for (uint32_t bufferIndex = 0; bufferIndex < request->getNumOfOutputBuffer(); bufferIndex++) {
            const camera3_stream_buffer_t *streamBuffer = &(bufferList[bufferIndex]);
            streamId = request->getStreamIdwithBufferIdx(bufferIndex);
            buffer_handle_t *handle = streamBuffer->buffer;
            buffer_manager_tag_t bufTag;

            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

            switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_OUTPUT:
                leaderPipeId = PIPE_3AA;
                bufTag.pipeId[0] = PIPE_VC0;
                break;
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
                leaderPipeId = PIPE_3AA;
                bufTag.pipeId[0] = PIPE_VC1;
                break;
#endif
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
                if (flagIspMcscM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                    leaderPipeId = PIPE_MCSC;
                } else if (flag3aaIspM2M == true
                           && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                    leaderPipeId = PIPE_ISP;
                } else {
                    leaderPipeId = PIPE_3AA;
                }
                bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                                    % ExynosCameraParameters::YUV_MAX)
                                    + PIPE_MCSC0;
                break;
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                /* Do nothing */
                break;
            default:
                CLOGE("Invalid stream ID %d", streamId);
                break;
            }

            if (bufTag.pipeId[0] < 0) {
                CLOGW("[R%d F%d]Invalid pipe ID. batchIndex %d batchSize %d",
                        request->getKey(), frame->getFrameCount(), batchIndex, batchSize);
                /* Skip to set stream buffer for this batch index. This is Frame Drop */
                continue;
            }

            nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

            ret = frame->getDstBuffer(leaderPipeId, &buffer, nodeType);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to getDstBuffer. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            buffer.handle[batchIndex] = handle;
            buffer.acquireFence[batchIndex] = streamBuffer->acquire_fence;
            buffer.releaseFence[batchIndex] = streamBuffer->release_fence;

            ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType, INDEX(bufTag.pipeId[0]));
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            frame->setRequest(bufTag.pipeId[0], true);
        }
    }

    List<int> keylist;
    List<int>::iterator iter;
    ExynosCameraStream *stream = NULL;
    keylist.clear();
    m_streamManager->getStreamKeys(&keylist);
    for (iter = keylist.begin(); iter != keylist.end(); iter++) {
        buffer_manager_tag_t bufTag;
        bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

        m_streamManager->getStream(*iter, &stream);
        stream->getID(&streamId);

        switch (streamId % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_ZSL_OUTPUT:
            leaderPipeId = PIPE_3AA;
            bufTag.pipeId[0] = PIPE_VC0;
            break;
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP:
            leaderPipeId = PIPE_3AA;
            bufTag.pipeId[0] = PIPE_VC1;
            break;
#endif
        case HAL_STREAM_ID_PREVIEW:
        case HAL_STREAM_ID_CALLBACK:
        case HAL_STREAM_ID_VIDEO:
            if (flagIspMcscM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                leaderPipeId = PIPE_MCSC;
            } else if (flag3aaIspM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                leaderPipeId = PIPE_ISP;
            } else {
                leaderPipeId = PIPE_3AA;
            }
            bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                        % ExynosCameraParameters::YUV_MAX)
                                + PIPE_MCSC0;
            break;
        case HAL_STREAM_ID_RAW:
        case HAL_STREAM_ID_JPEG:
        case HAL_STREAM_ID_CALLBACK_STALL:
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
        case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
            /* Do nothing */
            break;
        default:
            CLOGE("Invalid stream ID %d", streamId);
            break;
        }

        if (bufTag.pipeId[0] < 0
            || frame->getRequest(bufTag.pipeId[0]) == false) {
            continue;
        }

        nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

        ret = frame->getDstBuffer(leaderPipeId, &buffer, nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to getDstBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        for (int batchIndex = 0; batchIndex < buffer.batchSize; batchIndex++) {
            ret = request->setAcquireFenceDone(buffer.handle[batchIndex],
                                            (buffer.acquireFence[batchIndex] == -1) ? true : false);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }
        }

        ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setupPreviewFactoryBuffers(const ExynosCameraRequestSP_sprt_t request,
                                                    ExynosCameraFrameSP_sptr_t frame,
                                                    ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    if (factory == NULL) {
        CLOGE("frame Factory is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);

    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();

    for (uint32_t index = 0; index < request->getNumOfOutputBuffer(); index++) {
        const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
        int streamId = request->getStreamIdwithBufferIdx(index);
        buffer_handle_t *handle = streamBuffer->buffer;
        buffer_manager_tag_t bufTag;

        /* Set Internal Buffer */
        if ((m_flagVideoStreamPriority && request->hasStream(HAL_STREAM_ID_VIDEO) == false)
            ) {
            if ((streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_PREVIEW) {
                ExynosCameraBuffer internalBuffer;
                bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    leaderPipeId = PIPE_FUSION;
                    bufTag.pipeId[0] = PIPE_FUSION;
                } else
#endif
                {
                    if (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
                        leaderPipeId = PIPE_ISP;
                    } else {
                        leaderPipeId = PIPE_3AA;
                    }
                    bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                                        % ExynosCameraParameters::YUV_MAX)
                                        + PIPE_MCSC0;
                }

                ret = m_bufferSupplier->getBuffer(bufTag, &internalBuffer);
                if (ret != NO_ERROR || internalBuffer.index < 0) {
                    CLOGE("Set Internal Buffers: Failed to getBuffer(%d). ret %d", internalBuffer.index, ret);
                    continue;
                }

                nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);
                ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
                if (ret != NO_ERROR) {
                    CLOGE("Set Internal Buffers: Failed to setDstBufferState. ret %d", ret);
                    continue;
                }

                ret = frame->setDstBuffer(leaderPipeId, internalBuffer, nodeType);
                if (ret != NO_ERROR) {
                    CLOGE("Set Internal Buffers: Failed to setDstBuffer. ret %d", ret);
                    continue;
                }
            }
        }

        bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_OUTPUT:
                leaderPipeId = PIPE_3AA;
                if (components.parameters->getUsePureBayerReprocessing() == true) {
                    bufTag.pipeId[0] = PIPE_VC0;
                } else {
                    bufTag.pipeId[0] = PIPE_3AC;
                }
                break;
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
                leaderPipeId = PIPE_3AA;
                bufTag.pipeId[0] = PIPE_VC1;
                break;
#endif
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_VIDEO:
                if (m_configurations->isSupportedFunction(SUPPORTED_FUNCTION_GDC) == true) {
                    leaderPipeId = PIPE_GDC;
                    bufTag.pipeId[0] = PIPE_GDC;
                    break;
                }

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    if (m_flagVideoStreamPriority == true) {
                        leaderPipeId = PIPE_FUSION;
                        bufTag.pipeId[0] = PIPE_FUSION;
                    } else {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    }
                    break;
                }
#endif
#ifdef USES_SW_VDIS
                if (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true) {
                    leaderPipeId = m_exCameraSolutionSWVdis->getPipeId();
                    bufTag.pipeId[0] = leaderPipeId;
                    break;
                }
#endif
                // pass through

            case HAL_STREAM_ID_PREVIEW:
#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    if (m_flagVideoStreamPriority) {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    } else {
                        leaderPipeId = PIPE_FUSION;
                        bufTag.pipeId[0] = PIPE_FUSION;
                    }
                    break;
                }
#endif
                // pass through

            case HAL_STREAM_ID_CALLBACK:
#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    continue;
                } else
#endif
                {
                    if (flagIspMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                        leaderPipeId = PIPE_MCSC;
                    } else if (flag3aaIspM2M == true
                            && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                        leaderPipeId = PIPE_ISP;
                    } else {
                        leaderPipeId = PIPE_3AA;
                    }

                    bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                                        % ExynosCameraParameters::YUV_MAX)
                                        + PIPE_MCSC0;
                }
                break;
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                /* Do nothing */
                break;
            case HAL_STREAM_ID_VISION:
                leaderPipeId = PIPE_FLITE;
                bufTag.pipeId[0] = PIPE_VC0;
                break;
            default:
                CLOGE("Invalid stream ID %d", streamId);
                break;
        }

        if (bufTag.pipeId[0] < 0) {
            CLOGV("Invalid pipe ID %d", bufTag.pipeId[0]);
            continue;
        }

        request->setParentStreamPipeId(streamId, leaderPipeId);
        request->setStreamPipeId(streamId, bufTag.pipeId[0]);

        buffer.handle[0] = handle;
        buffer.acquireFence[0] = streamBuffer->acquire_fence;
        buffer.releaseFence[0] = streamBuffer->release_fence;
        nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d T%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, frame->getFrameType(), ret);
#ifdef SUPPORT_DEPTH_MAP
            if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_DEPTHMAP) {
                frame->setRequest(PIPE_VC1, false);
            }
#endif
            continue;
        }

        ret = request->setAcquireFenceDone(handle, (buffer.acquireFence[0] == -1) ? true : false);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d T%d]Failed to setAcquireFenceDone. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, frame->getFrameType(), ret);
#ifdef SUPPORT_DEPTH_MAP
            if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_DEPTHMAP) {
                frame->setRequest(PIPE_VC1, false);
            }
#endif
            continue;
        }

        ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
#ifdef SUPPORT_DEPTH_MAP
            if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_DEPTHMAP) {
                frame->setRequest(PIPE_VC1, false);
            }
#endif
            continue;
        }

        ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType, INDEX(bufTag.pipeId[0]));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
#ifdef SUPPORT_DEPTH_MAP
            if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_DEPTHMAP) {
                frame->setRequest(PIPE_VC1, false);
            }
#endif
            continue;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setupVisionFactoryBuffers(const ExynosCameraRequestSP_sprt_t request,
                                                   ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();

    for (uint32_t index = 0; index < request->getNumOfOutputBuffer(); index++) {
        const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
        int streamId = request->getStreamIdwithBufferIdx(index);
        buffer_handle_t *handle = streamBuffer->buffer;
        buffer_manager_tag_t bufTag;
        bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_ZSL_OUTPUT:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
#endif
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                /* Do nothing */
                break;
            case HAL_STREAM_ID_VISION:
                leaderPipeId = PIPE_FLITE;
                bufTag.pipeId[0] = PIPE_VC0;
                break;
            default:
                CLOGE("Invalid stream ID %d", streamId);
                break;
        }

        if (bufTag.pipeId[0] < 0) {
            CLOGV("Invalid pipe ID %d", bufTag.pipeId[0]);
            continue;
        }

        request->setParentStreamPipeId(streamId, leaderPipeId);
        request->setStreamPipeId(streamId, bufTag.pipeId[0]);

        buffer.handle[0] = handle;
        buffer.acquireFence[0] = streamBuffer->acquire_fence;
        buffer.releaseFence[0] = streamBuffer->release_fence;
        nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        ret = request->setAcquireFenceDone(handle, (buffer.acquireFence[0] == -1) ? true : false);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }

        ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType, INDEX(bufTag.pipeId[0]));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            continue;
        }
    }

    return ret;
}

status_t ExynosCamera::m_setupCaptureFactoryBuffers(const ExynosCameraRequestSP_sprt_t request,
                                                    ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;
    uint32_t frameType = 0;
    buffer_manager_tag_t bufTag;
    const buffer_manager_tag_t initBufTag;
    int yuvStallPipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
    frame_handle_components_t components;

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    bool flagRawOutput = false;
    bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
    frameType = frame->getFrameType();

    /* Set Internal Buffers */
    if ((frame->getRequest(yuvStallPipeId) == true)
        && (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) == false
        || m_flagUseInternalyuvStall == true
        )) {
        leaderPipeId = (components.parameters->getUsePureBayerReprocessing() == true) ?
            PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
        bufTag.pipeId[0] = yuvStallPipeId;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, ret);
            return BAD_VALUE;
        }

        nodeType = (uint32_t) components.reprocessingFactory->getNodeType(bufTag.pipeId[0]);

        ret = frame->setDstBufferState(leaderPipeId,
                                       ENTITY_BUFFER_STATE_REQUESTED,
                                       nodeType);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to setDstBufferState. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, ret);
            return BAD_VALUE;
        }

        ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType, INDEX(bufTag.pipeId[0]));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to setDstBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, ret);
            return BAD_VALUE;
        }
    }

    /* Set Service Buffers */
    if (frameType != FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        && frameType != FRAME_TYPE_INTERNAL_SLAVE
#endif
        ) {
        for (uint32_t index = 0; index < request->getNumOfOutputBuffer(); index++) {
            const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
            int streamId = request->getStreamIdwithBufferIdx(index);
            bufTag = initBufTag;
            buffer_handle_t *handle = streamBuffer->buffer;
            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
            flagRawOutput = false;

            switch (streamId % HAL_STREAM_ID_MAX) {
                case HAL_STREAM_ID_JPEG:
                    if (m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE) == YUV_STALL_USAGE_PICTURE) {
#ifdef USE_DUAL_CAMERA
                        if (frameType == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
                            leaderPipeId = -1;
                            bufTag.pipeId[0] = -1;
                        } else
#endif
                        {
                            leaderPipeId = PIPE_JPEG_REPROCESSING;
                            bufTag.pipeId[0] = PIPE_JPEG_REPROCESSING;
                        }
                    } else if (components.parameters->isReprocessing() == true && components.parameters->isUseHWFC() == true) {
                        if (flagIspMcscM2M == true
                            && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_MCSC_REPROCESSING) == true) {
                            leaderPipeId = PIPE_MCSC_REPROCESSING;
                        } else if (flag3aaIspM2M == true
                                && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_ISP_REPROCESSING) == true) {
                            leaderPipeId = PIPE_ISP_REPROCESSING;
                        } else {
                            if (components.parameters->getUsePureBayerReprocessing() == true) {
                                leaderPipeId = PIPE_3AA_REPROCESSING;
                            } else {
                                leaderPipeId = PIPE_ISP_REPROCESSING;
                            }
                        }
                        bufTag.pipeId[0] = PIPE_HWFC_JPEG_DST_REPROCESSING;
                    } else {
                        leaderPipeId = PIPE_JPEG;
                        bufTag.pipeId[0] = PIPE_JPEG;
                    }
                    break;
                case HAL_STREAM_ID_CALLBACK:
                    if (!frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
                        /* If there is no ZSL_INPUT stream buffer,
                         * It will be processed through preview stream.
                         */
                        break;
                    }
                case HAL_STREAM_ID_CALLBACK_STALL:
#ifdef USE_DUAL_CAMERA
                    if (frameType == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
                        leaderPipeId = PIPE_MCSC_REPROCESSING;
                    } else
#endif
                    {
                        if (m_flagUseInternalyuvStall == true) {
                            continue;
                        }

                        leaderPipeId = (components.parameters->getUsePureBayerReprocessing() == true) ?
                                        PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
                        bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                                % ExynosCameraParameters::YUV_MAX)
                            + PIPE_MCSC0_REPROCESSING;
                    }
                    break;
                case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef USE_DUAL_CAMERA
                    if (frameType == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
                        leaderPipeId = -1;
                        bufTag.pipeId[0] = -1;
                    } else
#endif
                    {
                        leaderPipeId = PIPE_GSC_REPROCESSING2;
                        bufTag.pipeId[0] = PIPE_GSC_REPROCESSING2;
                    }
                    break;
                case HAL_STREAM_ID_RAW:
                    flagRawOutput = true;
                    leaderPipeId = PIPE_3AA_REPROCESSING;
                    bufTag.pipeId[0] = PIPE_3AC_REPROCESSING;
                    break;
                case HAL_STREAM_ID_ZSL_OUTPUT:
                case HAL_STREAM_ID_PREVIEW:
                case HAL_STREAM_ID_VIDEO:
                case HAL_STREAM_ID_PREVIEW_VIDEO:
#ifdef SUPPORT_DEPTH_MAP
                case HAL_STREAM_ID_DEPTHMAP:
                case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                case HAL_STREAM_ID_VISION:
                    /* Do nothing */
                    bufTag.pipeId[0] = -1;
                    break;
                default:
                    bufTag.pipeId[0] = -1;
                    CLOGE("Invalid stream ID %d", streamId);
                    break;
            }

            if (bufTag.pipeId[0] < 0) {
                CLOGV("Invalid pipe ID %d", bufTag.pipeId[0]);
                continue;
            }

            request->setParentStreamPipeId(streamId, leaderPipeId);
            request->setStreamPipeId(streamId, bufTag.pipeId[0]);

            buffer.handle[0] = handle;
            buffer.acquireFence[0] = streamBuffer->acquire_fence;
            buffer.releaseFence[0] = streamBuffer->release_fence;

            ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            ret = request->setAcquireFenceDone(handle, (buffer.acquireFence[0] == -1) ? true : false);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            nodeType = (uint32_t) components.reprocessingFactory->getNodeType(bufTag.pipeId[0]);

            ret = frame->setDstBufferState(leaderPipeId,
                                           ENTITY_BUFFER_STATE_REQUESTED,
                                           nodeType);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
            /* change the 3AC buffer to internal from service */
            if (components.parameters->isUseRawReverseReprocessing() == true && flagRawOutput) {
                ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, OUTPUT_NODE_1);
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d B%d S%d]Failed to setDstBufferState. ret %d",
                            request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                    continue;
                }

                /* HACK: backup the service buffer to the frame by output node index */
                ret = frame->setDstBuffer(leaderPipeId, buffer, OUTPUT_NODE_1);
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                            request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                }

                bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
                ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                            request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                    continue;
                }

                /* ion sync before qbuf to driver */
                for (int plane = 0; plane < buffer.getMetaPlaneIndex(); plane++)
                    if (m_ionClient >= 0)
                        exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
            }
#endif

            ret = frame->setDstBuffer(leaderPipeId, buffer, nodeType);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setDstBuffer. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }
        }
    }

    return ret;
}

status_t ExynosCamera::m_setReprocessingBuffer(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;
    int minBufferCount = 0;
    int maxBufferCount = 0;
    int maxPictureW = 0, maxPictureH = 0;
    int maxThumbnailW = 0, maxThumbnailH = 0;
    int pictureFormat = 0;

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);

        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    if (m_configurations->getDynamicMode(DYNAMIC_HIGHSPEED_RECORDING_MODE) == true) {
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&maxPictureW, (uint32_t *)&maxPictureH);
    } else {
        m_parameters[m_cameraId]->getSize(HW_INFO_MAX_PICTURE_SIZE, (uint32_t *)&maxPictureW, (uint32_t *)&maxPictureH);
    }
    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_THUMBNAIL_SIZE, (uint32_t *)&maxThumbnailW, (uint32_t *)&maxThumbnailH);
    pictureFormat = m_parameters[m_cameraId]->getHwPictureFormat();

    CLOGI("Max Picture %ssize %dx%d format %x",
            (m_configurations->getDynamicMode(DYNAMIC_HIGHSPEED_RECORDING_MODE) == true) ?
            "on HighSpeedRecording ":"",
            maxPictureW, maxPictureH, pictureFormat);
    CLOGI("MAX Thumbnail size %dx%d",
            maxThumbnailW, maxThumbnailH);

    /* Reprocessing buffer from 3AA to ISP */
    if (m_parameters[m_cameraId]->isReprocessing() == true
        && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M
        && m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
        /* ISP_RE */
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_3AP_REPROCESSING;
        bufTag.pipeId[1] = PIPE_ISP_REPROCESSING;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("ISP_RE_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create MCSC_BUF. ret %d", ret);
        }

        minBufferCount = 1;
        maxBufferCount = m_exynosconfig->current->bufInfo.num_reprocessing_buffers;

        bufConfig = initBufConfig;
        bufConfig.planeCount = 2;
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_configurations->checkBayerDumpEnable()) {
            bufConfig.bytesPerLine[0] = maxPictureW* 2;
            bufConfig.size[0] = maxPictureW* maxPictureH* 2;
        } else
#endif /* DEBUG_RAWDUMP */
        {
            bufConfig.bytesPerLine[0] = getBayerLineSize(maxPictureW, m_parameters[m_cameraId]->getBayerFormat(PIPE_ISP_REPROCESSING));
            // Old: bytesPerLine[0] = ROUND_UP((maxPictureW * 3 / 2), 16);
            bufConfig.size[0] = bufConfig.bytesPerLine[0] * maxPictureH;
        }
#else
        bufConfig.size[0] = maxPictureW * maxPictureH * 2;
#endif
        bufConfig.reqBufCount = minBufferCount;
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = false;
        bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_ISP_RE, BUF_TYPE_REPROCESSING);
        if (ret != NO_ERROR) {
            CLOGE("Failed to add ISP_REP_BUF. ret %d", ret);
            return ret;
        }
#else
        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc ISP_REP_BUF. ret %d", ret);
            return ret;
        }
#endif
    }

    /* CAPTURE_CB */
    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_MCSC0_REPROCESSING;
    bufTag.pipeId[1] = PIPE_MCSC1_REPROCESSING;
    bufTag.pipeId[2] = PIPE_MCSC2_REPROCESSING;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("CAPTURE_CB_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create CAPTURE_CB_BUF. ret %d", ret);
    }

    minBufferCount = m_exynosconfig->current->bufInfo.num_nv21_picture_buffers;
    maxBufferCount = m_exynosconfig->current->bufInfo.num_nv21_picture_buffers;

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
    bufConfig.size[0] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
    bufConfig.reqBufCount = minBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = true;
#if defined(RESERVED_MEMORY_ENABLE)
    if (getCameraId() == CAMERA_ID_BACK) {
        bufConfig.reservedMemoryCount = RESERVED_NUM_INTERNAL_NV21_BUFFERS;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
    } else if (m_configurations->getMode(CONFIGURATION_PIP_MODE) == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_INTERNAL_NV21_BUFFERS;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
    } else
#endif
    {
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    }

    CLOGD("Allocate CAPTURE_CB(NV21)_BUF (minBufferCount=%d, maxBufferCount=%d)",
                minBufferCount, maxBufferCount);

#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_CAPTURE_CB, BUF_TYPE_REPROCESSING);
    if (ret != NO_ERROR) {
        CLOGE("Failed to add CAPTURE_CB_BUF. ret %d", ret);
        return ret;
    }
#else
    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret < 0) {
        CLOGE("Failed to alloc CAPTURE_CB_BUF. ret %d", ret);
        return ret;
    }
#endif

    /* Reprocessing YUV buffer */
    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_MCSC_JPEG_REPROCESSING;
    bufTag.pipeId[1] = PIPE_JPEG_REPROCESSING;
    bufTag.pipeId[2] = PIPE_HWFC_JPEG_SRC_REPROCESSING;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("YUV_CAP_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create YUV_CAP_BUF. ret %d", ret);
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        minBufferCount = 2;
    } else
#endif
    {
        minBufferCount = 1;
    }
    maxBufferCount = m_exynosconfig->current->bufInfo.num_reprocessing_buffers;

    bufConfig = initBufConfig;
    switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
            bufConfig.planeCount = 3;
            bufConfig.size[0] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN);
            bufConfig.size[1] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) / 2;
            break;
        case V4L2_PIX_FMT_NV21:
            bufConfig.planeCount = 2;
            bufConfig.size[0] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 3 / 2;
            break;
        default:
            bufConfig.planeCount = 2;
            bufConfig.size[0] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 2;
            break;
    }
    bufConfig.reqBufCount = minBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;
    if (m_parameters[m_cameraId]->isUseHWFC() == true)
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    else
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
    bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_YUV_CAP, BUF_TYPE_REPROCESSING);
    if (ret != NO_ERROR) {
        CLOGE("Failed to add YUV_CAP_BUF. ret %d", ret);
        return ret;
    }
#else
    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc YUV_CAP_BUF. ret %d", ret);
        return ret;
    }
#endif

    /* Reprocessing Thumbanil buffer */
    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_MCSC_THUMB_REPROCESSING;
    bufTag.pipeId[1] = PIPE_HWFC_THUMB_SRC_REPROCESSING;
    bufTag.pipeId[2] = PIPE_HWFC_THUMB_DST_REPROCESSING;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("THUMB_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create THUMB_BUF. ret %d", ret);
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        minBufferCount = 2;
    } else
#endif
    {
        minBufferCount = 1;
    }
    maxBufferCount = m_exynosconfig->current->bufInfo.num_reprocessing_buffers;

    bufConfig = initBufConfig;
    switch (pictureFormat) {
        case V4L2_PIX_FMT_NV21M:
            bufConfig.planeCount = 3;
            bufConfig.size[0] = maxThumbnailW * maxThumbnailH;
            bufConfig.size[1] = maxThumbnailW * maxThumbnailH / 2;
            break;
        case V4L2_PIX_FMT_NV21:
        default:
            bufConfig.planeCount = 2;
            bufConfig.size[0] = FRAME_SIZE(V4L2_PIX_2_HAL_PIXEL_FORMAT(pictureFormat), maxThumbnailW, maxThumbnailH);
            break;
    }

    bufConfig.reqBufCount = minBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    if (m_parameters[m_cameraId]->isUseHWFC() == true)
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    else
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ONDEMAND;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = false;
    bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
    ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_THUMB, BUF_TYPE_REPROCESSING);
    if (ret != NO_ERROR) {
        CLOGE("Failed to add THUMB_BUF. ret %d", ret);
        return ret;
    }
#else
    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc THUMB_BUF. ret %d", ret);
        return ret;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_sendForceStallStreamResult(ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    camera3_stream_buffer_t *streamBuffer = NULL;
    const camera3_stream_buffer_t *outputBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ExynosCameraStream *stream = NULL;
    ResultRequest resultRequest = NULL;
    int streamId = 0;
    int bufferCnt = 0;
    int frameCount = request->getFrameCount();

    request->setSkipCaptureResult(true);
    bufferCnt = request->getNumOfOutputBuffer();
    outputBuffer = request->getOutputBuffers();

    m_frameCountLock.lock();
    if (request->getFrameCount() == 0) {
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
    }
    m_frameCountLock.unlock();

    /* timestamp issue for burst capture */
    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_RESULT);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Failed to sendNotifyError. ret %d",
                request->getKey(), request->getFrameCount(), ret);
    }

    for (int i = 0; i < bufferCnt; i++) {
        streamBuffer = NULL;
        requestResult = NULL;
        stream = NULL;

        stream = static_cast<ExynosCameraStream*>(outputBuffer[i].stream->priv);
        if (stream == NULL) {
            CLOGE("[R%d F%d]Failed to getStream.", request->getKey(), frameCount);
            return INVALID_OPERATION;
        }

        stream->getID(&streamId);

        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                break;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_ZSL_OUTPUT:
                continue;
            default:
                CLOGE("Inavlid stream Id %d", streamId);
                return BAD_VALUE;
        }

        resultRequest = m_requestMgr->createResultRequest(request->getKey(), frameCount,
                EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
        if (resultRequest == NULL) {
            CLOGE("[R%d F%d S%d] createResultRequest fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        requestResult = resultRequest->getCaptureResult();
        if (requestResult == NULL) {
            CLOGE("[R%d F%d S%d] getCaptureResult fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        streamBuffer = resultRequest->getStreamBuffer();
        if (streamBuffer == NULL) {
            CLOGE("[R%d F%d S%d] getStreamBuffer fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        ret = stream->getStream(&(streamBuffer->stream));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getStream. frameCount %d requestKey %d",
                    frameCount, request->getKey());
            continue;
        }

        streamBuffer->buffer = outputBuffer[i].buffer;

        ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status, true);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d]Failed to checkStreamBufferStatus.",
                    request->getKey(), frameCount, streamId);
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

        CLOGV("[R%d F%d S%d]checkStreamBufferStatus.",
                request->getKey(), frameCount, streamId);

        m_requestMgr->pushResultRequest(resultRequest);
    }

    ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
    if (ret != NO_ERROR) {
        CLOGE("[R%d]Failed to sendMeta. ret %d", request->getKey(), ret);
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_sendForceYuvStreamResult(ExynosCameraRequestSP_sprt_t request,
                                                   ExynosCameraFrameSP_sptr_t frame,
                                                   ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraStream *stream = NULL;
    ResultRequest resultRequest = NULL;
    List<int> *outputStreamId;
    int frameCount = 0;
    int streamId = 0;

    if (frame == NULL) {
        CLOGE("frame is NULL.");
        ret = INVALID_OPERATION;
        return ret;
    }

    frameCount = frame->getFrameCount();

    request->getAllRequestOutputStreams(&outputStreamId);
    for (List<int>::iterator iter = outputStreamId->begin(); iter != outputStreamId->end(); iter++) {
        streamBuffer = NULL;
        requestResult = NULL;

        m_streamManager->getStream(*iter, &stream);
        if (stream == NULL) {
            CLOGE("[R%d F%d]Failed to getStream.", request->getKey(), frameCount);
            continue;
        }

        stream->getID(&streamId);
        switch (streamId % HAL_STREAM_ID_MAX) {
            case HAL_STREAM_ID_RAW:
            case HAL_STREAM_ID_ZSL_OUTPUT:
            case HAL_STREAM_ID_JPEG:
            case HAL_STREAM_ID_CALLBACK_STALL:
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP_STALL:
#endif
                continue;
            case HAL_STREAM_ID_PREVIEW:
            case HAL_STREAM_ID_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
#endif
            case HAL_STREAM_ID_PREVIEW_VIDEO:
                break;
            default:
                CLOGE("[R%d F%d]Inavlid stream Id %d",
                        request->getKey(), frameCount, streamId);
                continue;
        }

        resultRequest = m_requestMgr->createResultRequest(request->getKey(), frameCount,
                EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
        if (resultRequest == NULL) {
            CLOGE("[R%d F%d S%d] createResultRequest fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        requestResult = resultRequest->getCaptureResult();
        if (requestResult == NULL) {
            CLOGE("[R%d F%d S%d] getCaptureResult fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        streamBuffer = resultRequest->getStreamBuffer();
        if (streamBuffer == NULL) {
            CLOGE("[R%d F%d S%d] getStreamBuffer fail.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        ret = stream->getStream(&(streamBuffer->stream));
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d]Failed to getStream.",
                    request->getKey(), frameCount, streamId);
            continue;
        }

        ret = m_checkStreamBuffer(frame, stream, &buffer, request, factory);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBuffer.",
                    request->getKey(),
                    frame->getFrameCount(),
                    streamId,
                    buffer.index);
            /* Continue to handle other streams */
            continue;
        }

        streamBuffer->buffer = buffer.handle[0];

        ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status, true);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                    request->getKey(), frame->getFrameCount(), streamId, buffer.index);
            /* Continue to handle other streams */
            continue;
        }

        streamBuffer->acquire_fence = -1;
        streamBuffer->release_fence = -1;

        /* construct result for service */
        requestResult->frame_number = request->getKey();
        requestResult->result = NULL;
        requestResult->input_buffer = request->getInputBuffer();
        requestResult->num_output_buffers = 1;
        requestResult->output_buffers = streamBuffer;
        requestResult->partial_result = 0;

        m_requestMgr->pushResultRequest(resultRequest);

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d S%d B%d]Failed to putBuffer. ret %d",
                        request->getKey(), frameCount, streamId, buffer.index, ret);
            }
        }
    }

    return NO_ERROR;
}

bool ExynosCamera::m_thumbnailCbThreadFunc(void)
{
    int loop = false;
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosRect srcRect, dstRect;
    int srcPipeId = -1;
    int nodePipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
    int gscPipeId = PIPE_GSC_REPROCESSING2;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
    int thumbnailH = 0, thumbnailW = 0;
    int inputSizeH = 0, inputSizeW = 0;
    int waitCount = 0;
    frame_handle_components_t components;
    int streamId = HAL_STREAM_ID_THUMBNAIL_CALLBACK;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraStream *stream = NULL;
    bool flag3aaIspM2M = false;
    bool flagIspMcscM2M = false;

    srcBuffer.index = -2;
    dstBuffer.index = -2;

    CLOGD("-- IN --");

    CLOGV("wait m_postProcessingQ");
    ret = m_thumbnailCbQ->waitAndPopProcessQ(&frame);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    } else if (frame == NULL) {
        CLOGE("frame is NULL!!");
        goto CLEAN;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    flagIspMcscM2M = (components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("getRequest failed ");
        return INVALID_OPERATION;
    }

    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_MCSC_REPROCESSING) == true) {
        srcPipeId = PIPE_MCSC_REPROCESSING;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_ISP_REPROCESSING) == true) {
        srcPipeId = PIPE_ISP_REPROCESSING;
    } else {
        srcPipeId = PIPE_3AA_REPROCESSING;
    }

    ret = m_streamManager->getStream(streamId, &stream);
    if (ret != NO_ERROR)
        CLOGE("Failed to getStream from streamMgr. HAL_STREAM_ID_THUMBNAIL_CALLBACK");

    if (frame->getFrameYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) {
        ret = frame->getDstBuffer(PIPE_GSC_REPROCESSING3, &srcBuffer);
    } else {
        ret = frame->getDstBuffer(srcPipeId, &srcBuffer, components.reprocessingFactory->getNodeType(nodePipeId));
    }
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]getDstBuffer fail. pipeId (%d) ret(%d)",
                request->getKey(), frame->getFrameCount(), nodePipeId, ret);
        goto CLEAN;
    }

    ret = frame->getDstBuffer(gscPipeId, &dstBuffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]getDstBuffer fail. pipeId(%d) ret(%d)",
                request->getKey(), frame->getFrameCount(), gscPipeId, ret);
        goto CLEAN;
    }

    srcRect.x = 0;
    srcRect.y = 0;
    srcRect.w = inputSizeW;
    srcRect.h = inputSizeH;
    srcRect.fullW = inputSizeW;
    srcRect.fullH = inputSizeH;
    srcRect.colorFormat = V4L2_PIX_FMT_NV21;

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = thumbnailW;
    dstRect.h = thumbnailH;
    dstRect.fullW = thumbnailW;
    dstRect.fullH = thumbnailH;
    dstRect.colorFormat = V4L2_PIX_FMT_NV21;

    CLOGD("srcBuf(%d) dstBuf(%d) (%d, %d, %d, %d) format(%d) actual(%x) -> (%d, %d, %d, %d) format(%d)  actual(%x)",
            srcBuffer.index, dstBuffer.index,
            srcRect.x, srcRect.y, srcRect.w, srcRect.h, srcRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
            dstRect.x, dstRect.y, dstRect.w, dstRect.h, dstRect.colorFormat, V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

    ret = frame->setSrcRect(gscPipeId, srcRect);
    ret = frame->setDstRect(gscPipeId, dstRect);

    ret = m_setupEntity(gscPipeId, frame, &srcBuffer, &dstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", gscPipeId, ret);
    }

    components.reprocessingFactory->setOutputFrameQToPipe(m_thumbnailPostCbQ, gscPipeId);
    components.reprocessingFactory->pushFrameToPipe(frame, gscPipeId);

    /* wait GSC done */
    CLOGV("wait GSC output");
    waitCount = 0;
    frame = NULL;
    dstBuffer.index = -2;
    do {
        ret = m_thumbnailPostCbQ->waitAndPopProcessQ(&frame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100);

    if (ret < 0) {
        CLOGW("Failed to waitAndPopProcessQ. ret %d waitCount %d", ret, waitCount);
    }
    if (frame == NULL) {
        CLOGE("frame is NULL");
        goto CLEAN;
    }

    ret = frame->getDstBuffer(gscPipeId, &dstBuffer);
    if (ret < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", gscPipeId, ret);
        goto CLEAN;
    }

    ret = frame->getDstBufferState(gscPipeId, &bufferState);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(),
                gscPipeId);
        goto CLEAN;
    } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        request->setStreamBufferStatus(streamId, CAMERA3_BUFFER_STATUS_ERROR);
        CLOGE("ERR(%s[%d]):Invalid thumnail stream buffer state. frameCount %d bufferState %d",
                __FUNCTION__, __LINE__,
                frame->getFrameCount(),
                bufferState);
    }

    ret = m_sendThumbnailStreamResult(request, &dstBuffer, streamId);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to sendYuvStreamStallResult."
                " pipeId %d streamId %d ret %d",
                frame->getFrameCount(),
                dstBuffer.index,
                gscPipeId, streamId, ret);
        goto CLEAN;
    }

CLEAN:
    if (frame != NULL) {
        CLOGV("frame delete. framecount %d", frame->getFrameCount());
        frame = NULL;
    }

    CLOGD("--OUT--");

    return loop;
}

#ifdef SUPPORT_DEPTH_MAP
status_t ExynosCamera::m_setDepthInternalBuffer()
{
    status_t ret = NO_ERROR;
    int bayerFormat = DEPTH_MAP_FORMAT;
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;
    buffer_manager_configuration_t bufConfig;
    buffer_manager_tag_t bufTag;
    int depthW = 0, depthH = 0;
    int depthInternalBufferCount = NUM_DEPTHMAP_BUFFERS;

    m_flagUseInternalDepthMap = true;
    m_parameters[m_cameraId]->getDepthMapSize(&depthW, &depthH);
    m_configurations->setMode(CONFIGURATION_DEPTH_MAP_MODE, true);

    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_VC1;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("DEPTH_MAP_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create DEPTH_MAP_BUF. ret %d", ret);
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
    bufConfig.bytesPerLine[0] = getBayerLineSize(depthW, bayerFormat);
    bufConfig.size[0] = getBayerPlaneSize(depthW, depthH, bayerFormat);
    bufConfig.reqBufCount = depthInternalBufferCount;
    bufConfig.allowedMaxBufCount = depthInternalBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
    bufConfig.needMmap = true;
    bufConfig.reservedMemoryCount = 0;
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc DEPTH_MAP_BUF. ret %d", ret);
        return ret;
    }

    CLOGI("m_allocBuffers(DepthMap Buffer) %d x %d,\
        planeSize(%d), planeCount(%d), maxBufferCount(%d)",
        depthW, depthH,
        bufConfig.size[0], bufConfig.planeCount, depthInternalBufferCount);

    return ret;
}
#endif

#ifdef ADAPTIVE_RESERVED_MEMORY
status_t ExynosCamera::m_setupAdaptiveBuffer()
{
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;
    status_t ret = NO_ERROR;

    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    for (int i = 0; i < BUF_PRIORITY_MAX; i++) {
        m_adaptiveBufferInfos[i].bufTag = initBufTag;
        m_adaptiveBufferInfos[i].bufConfig = initBufConfig;
        m_adaptiveBufferInfos[i].minReservedNum = 0;
        m_adaptiveBufferInfos[i].totalPlaneSize = 0;
        m_adaptiveBufferInfos[i].bufType = BUF_TYPE_UNDEFINED;
    }

    if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == false) {
        ret = m_setBuffers();
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_setAdaptiveBuffers. ret %d", ret);
            return ret;
        }
    }
    if (m_captureStreamExist && m_parameters[m_cameraId]->isReprocessing() == true) {
        ret = m_setReprocessingBuffer();
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_setAdaptiveBuffers. ret %d", ret);
            return ret;
        }
    }
    ret = m_setAdaptiveReservedBuffers();
    if (ret != NO_ERROR) {
        CLOGE("Failed to m_setAdaptiveBuffers. ret %d", ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_addAdaptiveBufferInfos(const buffer_manager_tag_t bufTag,
                                         const buffer_manager_configuration_t bufConfig,
                                         enum BUF_PRIORITY bufPrior,
                                         enum BUF_TYPE bufType)
{
    status_t ret = NO_ERROR;
    int i;
    int bufferPlaneCount;

    if (bufPrior >= BUF_PRIORITY_MAX) {
        ret = BAD_INDEX;
        return ret;
    }

    bufferPlaneCount = bufConfig.planeCount;

    if (bufConfig.createMetaPlane == true && bufferPlaneCount > 1) {
        bufferPlaneCount--;
    }

    m_adaptiveBufferInfos[bufPrior].totalPlaneSize = 0;
    for (i = 0; i < bufferPlaneCount; i++) {
        m_adaptiveBufferInfos[bufPrior].totalPlaneSize
            += ((ALIGN_UP(bufConfig.size[i], 0x1000)) * bufConfig.batchSize);
    }

    m_adaptiveBufferInfos[bufPrior].bufTag = bufTag;
    m_adaptiveBufferInfos[bufPrior].bufConfig = bufConfig;
    m_adaptiveBufferInfos[bufPrior].bufConfig.reservedMemoryCount = 0;
    m_adaptiveBufferInfos[bufPrior].bufType = bufType;

    if (m_adaptiveBufferInfos[bufPrior].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE)
        m_adaptiveBufferInfos[bufPrior].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    else if (m_adaptiveBufferInfos[bufPrior].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_NONCACHED_RESERVED_TYPE)
        m_adaptiveBufferInfos[bufPrior].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    else if (m_adaptiveBufferInfos[bufPrior].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_RESERVED_TYPE)
        m_adaptiveBufferInfos[bufPrior].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE;

#ifdef MIN_RESERVED_NUM_CAPTURE_CB
    if (bufPrior == BUF_PRIORITY_CAPTURE_CB) {
        m_adaptiveBufferInfos[bufPrior].minReservedNum = MIN_RESERVED_NUM_CAPTURE_CB;
    }
#endif

#ifdef MIN_RESERVED_NUM_YUV_CAP
    if (bufPrior == BUF_PRIORITY_YUV_CAP) {
        m_adaptiveBufferInfos[bufPrior].minReservedNum = MIN_RESERVED_NUM_YUV_CAP;
    }
#endif

#ifdef DEBUG_ADAPTIVE_RESERVED
    CLOGI("bufPrior(%d)", bufPrior);
#endif
    return ret;
}

status_t ExynosCamera::m_allocAdaptiveNormalBuffers()
{
    status_t ret = NO_ERROR;
#ifdef DEBUG_ADAPTIVE_RESERVED
    ExynosCameraDurationTimer m_timer;
    long long durationTime[BUF_PRIORITY_MAX] = {0};
    int totalDurationTime = 0;

    CLOGI("==IN==");
#endif

    for (int i = 0; i < BUF_PRIORITY_MAX; i++) {
        if (m_adaptiveBufferInfos[i].bufType == BUF_TYPE_UNDEFINED
            || m_adaptiveBufferInfos[i].totalPlaneSize == 0) {
            continue;
        }

        if (m_adaptiveBufferInfos[i].bufType == BUF_TYPE_NORMAL
            || m_adaptiveBufferInfos[i].bufType == BUF_TYPE_VENDOR) {
#ifdef DEBUG_ADAPTIVE_RESERVED
            m_timer.start();
#endif
            ret = m_allocBuffers(m_adaptiveBufferInfos[i].bufTag, m_adaptiveBufferInfos[i].bufConfig);
#ifdef DEBUG_ADAPTIVE_RESERVED
            m_timer.stop();
            durationTime[i] = m_timer.durationMsecs();
#endif
            if (ret < 0) {
                CLOGE("Failed to alloc i(%d). ret %d", i, ret);
                /* android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Failed to alloc. so, assert!!!!",
                        __FUNCTION__, __LINE__); */
                return ret;
            }

#ifdef DEBUG_ADAPTIVE_RESERVED
            CLOGI("buf(%d), size(%d KB), reserved(%d), req(%d), reprocessing(%d), time(%5d msec)", i,
                m_adaptiveBufferInfos[i].totalPlaneSize/1024,
                m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount,
                m_adaptiveBufferInfos[i].bufConfig.reqBufCount,
                ((m_adaptiveBufferInfos[i].bufType == BUF_TYPE_REPROCESSING) ? 1 : 0),
                (int)durationTime[i]);
            totalDurationTime += (int)durationTime[i];
#endif
        }
    }

#ifdef DEBUG_ADAPTIVE_RESERVED
    CLOGI("total duration time(%5d msec)", totalDurationTime);
    CLOGI("==OUT==");
#endif
    return ret;
}

status_t ExynosCamera::m_allocAdaptiveReprocessingBuffers()
{
    status_t ret = NO_ERROR;
#ifdef DEBUG_ADAPTIVE_RESERVED
    ExynosCameraDurationTimer m_timer;
    long long durationTime[BUF_PRIORITY_MAX] = {0};
    int totalDurationTime = 0;

    CLOGI("==IN==");
#endif

    for (int i = 0; i < BUF_PRIORITY_MAX; i++) {
        if (m_adaptiveBufferInfos[i].bufType == BUF_TYPE_UNDEFINED
            || m_adaptiveBufferInfos[i].totalPlaneSize == 0) {
            continue;
        }

        if (m_adaptiveBufferInfos[i].bufType == BUF_TYPE_REPROCESSING) {
#ifdef DEBUG_ADAPTIVE_RESERVED
            m_timer.start();
#endif
            ret = m_allocBuffers(m_adaptiveBufferInfos[i].bufTag, m_adaptiveBufferInfos[i].bufConfig);
#ifdef DEBUG_ADAPTIVE_RESERVED
            m_timer.stop();
            durationTime[i] = m_timer.durationMsecs();
#endif
            if (ret < 0) {
                CLOGE("Failed to alloc i(%d). ret %d", i, ret);
                /* android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Failed to alloc. so, assert!!!!",
                        __FUNCTION__, __LINE__); */
                return ret;
            }
#ifdef DEBUG_ADAPTIVE_RESERVED
            CLOGI("buf(%d), size(%d KB), reserved(%d), req(%d), duration time(%5d msec)", i,
                m_adaptiveBufferInfos[i].totalPlaneSize/1024,
                m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount,
                m_adaptiveBufferInfos[i].bufConfig.reqBufCount,
                (int)durationTime[i]);
            totalDurationTime += (int)durationTime[i];
#endif
        }
    }

#ifdef DEBUG_ADAPTIVE_RESERVED
    CLOGI("total duration time(%5d msec)", totalDurationTime);
    CLOGI("==OUT==");
#endif
    return ret;
}

status_t ExynosCamera::m_setAdaptiveReservedBuffers()
{
    status_t ret = NO_ERROR;
    int clearance = TOTAL_RESERVED_BUFFER_SIZE;
#ifdef DEBUG_ADAPTIVE_RESERVED
    ExynosCameraDurationTimer m_timer;
    long long durationTime[BUF_PRIORITY_MAX] = {0};
    int totalAllocReservedBuf = 0;

    CLOGI("==IN==");
#endif

    for (int i = 0; i < BUF_PRIORITY_MAX; i++) {
        int minBuf = m_adaptiveBufferInfos[i].minReservedNum;
        int size = m_adaptiveBufferInfos[i].totalPlaneSize;

        if (m_adaptiveBufferInfos[i].bufConfig.reqBufCount < minBuf) {
            minBuf = m_adaptiveBufferInfos[i].bufConfig.reqBufCount;
            m_adaptiveBufferInfos[i].minReservedNum = minBuf;
        }

        if (minBuf > 0 && size > LOW_LIMIT_RESERVED_BUFFER_SIZE) {
            if (clearance >= size * minBuf) {
                clearance -= (size * minBuf);
                m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount = minBuf;
            }
        }
    }

    for (int i = 0; i < BUF_PRIORITY_MAX; i++) {
        int size = m_adaptiveBufferInfos[i].totalPlaneSize;
        int reqBuf = m_adaptiveBufferInfos[i].bufConfig.reqBufCount;
        int minBuf = m_adaptiveBufferInfos[i].minReservedNum;

        if (size > LOW_LIMIT_RESERVED_BUFFER_SIZE) {
            if (reqBuf >= minBuf) {
                reqBuf -= minBuf;
            }
            if (reqBuf > 0) {
                if (clearance >= size * reqBuf) {
                    clearance -= (size * reqBuf);
                    if (minBuf > 0)
                        m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount += reqBuf;
                    else
                        m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount = reqBuf;
                } else {
                    if (clearance >= size) {
                        int tempReqBuf = (clearance / size);
                        if (tempReqBuf >= minBuf) {
                            tempReqBuf -= minBuf;
                        }
                        clearance -= (size * tempReqBuf);
                        if (minBuf > 0)
                            m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount += tempReqBuf;
                        else
                            m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount = tempReqBuf;
                    }
                }
            }

            if (m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount > 0) {
                if (m_adaptiveBufferInfos[i].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE)
                    m_adaptiveBufferInfos[i].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
                else if (m_adaptiveBufferInfos[i].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE)
                    m_adaptiveBufferInfos[i].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_RESERVED_TYPE;
                else if (m_adaptiveBufferInfos[i].bufConfig.type == EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_TYPE)
                    m_adaptiveBufferInfos[i].bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_SYNC_FORCE_RESERVED_TYPE;
            }
        }

#ifdef DEBUG_ADAPTIVE_RESERVED
        CLOGI("buf(%d) / req(%d) / minBuf(%d) / reserved(%d) / size(%d KB) / allocReservedBuf(%d KB) / totalReservedBuf(%d KB)",
            i,
            m_adaptiveBufferInfos[i].bufConfig.reqBufCount,
            minBuf,
            m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount,
            size/1024,
            m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount * size/1024,
            totalAllocReservedBuf += m_adaptiveBufferInfos[i].bufConfig.reservedMemoryCount * size/1024);
#endif
    }

#ifdef DEBUG_ADAPTIVE_RESERVED
    CLOGI("================================================================================");
    CLOGI("remaining reserved memory (%d KB)", clearance/1024);
    CLOGI("================================================================================");
    CLOGI("==OUT==");
#endif
    return ret;
}
#endif

void ExynosCamera::m_copyPreviewCbThreadFunc(ExynosCameraRequestSP_sprt_t request,
                                                            ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraBuffer *buffer)
{
    buffer_manager_tag_t bufTag;
    ExynosCameraBuffer serviceBuffer;
    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
    int32_t index = -1;
    int ret = 0;
    bool skipFlag = false;

    serviceBuffer.index = -2;
    index = request->getBufferIndex(HAL_STREAM_ID_CALLBACK);

    if (index >= 0) {
        const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
        buffer_handle_t *handle = streamBuffer->buffer;

        bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        bufTag.pipeId[0] = (m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK)
                            % ExynosCameraParameters::YUV_MAX)
                            + PIPE_MCSC0;
        serviceBuffer.handle[0] = handle;
        serviceBuffer.acquireFence[0] = streamBuffer->acquire_fence;
        serviceBuffer.releaseFence[0] = streamBuffer->release_fence;

        ret = m_bufferSupplier->getBuffer(bufTag, &serviceBuffer);
        if (ret != NO_ERROR || serviceBuffer.index < 0) {
            CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                request->getKey(), frame->getFrameCount(), serviceBuffer.index, ret);
        }

        ret = request->setAcquireFenceDone(handle, (serviceBuffer.acquireFence[0] == -1) ? true : false);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to setAcquireFenceDone. ret %d",
                request->getKey(), frame->getFrameCount(), serviceBuffer.index, ret);
        }

        if (buffer->index >= 0) {
            ret = m_copyPreview2Callback(frame, buffer, &serviceBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d]m_copyPreview2Callback->create() fail",
                    request->getKey(), frame->getFrameCount(), serviceBuffer.index);
            }
        } else {
            serviceBuffer.index = -2;
            skipFlag = true;
        }

        ret = m_sendYuvStreamResult(request, &serviceBuffer, HAL_STREAM_ID_CALLBACK,
                                    skipFlag, frame->getStreamTimestamp());
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to sendRawStreamResult. ret %d",
                    frame->getFrameCount(), serviceBuffer.index, ret);
        }
    }
}

status_t ExynosCamera::m_copyPreview2Callback(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *srcBuffer, ExynosCameraBuffer *dstBuffer)
{
    status_t ret = NO_ERROR;

    ExynosCameraDurationTimer timer;

    int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
    int previewFormat = m_configurations->getYuvFormat(previewOutputPortId);
    ExynosRect rect = convertingBufferDst2Rect(srcBuffer, previewFormat);

    int callbbackOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK);
    int callbackFormat = m_configurations->getYuvFormat(callbbackOutputPortId);

    int callbackWidth = 0;
    int callbackHeight = 0;
    m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&callbackWidth, (uint32_t *)&callbackHeight, callbbackOutputPortId);

    bool flagMemcpy = false;

    if (rect.w == callbackWidth &&
        rect.h == callbackHeight) {
        flagMemcpy = true;
    } else {
        flagMemcpy = false;
    }

    timer.start();

    if(flagMemcpy == true) {
        memcpy(dstBuffer->addr[0],                      srcBuffer->addr[0], srcBuffer->size[0]);
        memcpy(dstBuffer->addr[0] + srcBuffer->size[0], srcBuffer->addr[1], srcBuffer->size[1]);

        if(m_ionClient >= 0) {
            exynos_ion_sync_fd(m_ionClient, dstBuffer->fd[0]);
        }
    } else {
        if (m_previewCallbackPP == NULL) {
            m_previewCallbackPP = ExynosCameraPPFactory::newPP(m_cameraId, m_configurations, m_parameters[m_cameraId], PREVIEW_GSC_NODE_NUM);

            ret = m_previewCallbackPP->create();
            if (ret != NO_ERROR) {
                CLOGE("m_previewCallbackPP->create() fail");
                goto done;
            }
        }

        ExynosCameraImage srcImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];
        ExynosCameraImage dstImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];

        srcImage[0].buf  = *srcBuffer;
        srcImage[0].rect = rect;

        dstImage[0].buf  = *dstBuffer;
        dstImage[0].rect.w     = callbackWidth;
        dstImage[0].rect.fullW = callbackWidth;
        dstImage[0].rect.h     = callbackHeight;
        dstImage[0].rect.fullH = callbackHeight;
        dstImage[0].rect.colorFormat = callbackFormat;

        ret = m_previewCallbackPP->draw(srcImage, dstImage, frame->getParameters());
        if (ret != NO_ERROR) {
            CLOGE("m_previewCallbackPP->draw(srcImage(fd(%d, %d) size(%d, %d, %d, %d)), dstImage(fd(%d, %d) size(%d, %d, %d, %d)) fail",
                srcImage[0].buf.fd[0],
                srcImage[0].buf.fd[1],
                srcImage[0].rect.w,
                srcImage[0].rect.h,
                srcImage[0].rect.fullW,
                srcImage[0].rect.fullH,
                dstImage[0].buf.fd[0],
                dstImage[0].buf.fd[1],
                dstImage[0].rect.w,
                dstImage[0].rect.h,
                dstImage[0].rect.fullW,
                dstImage[0].rect.fullH);

            goto done;
        }
    }

done:

    timer.stop();

    if(flagMemcpy == true) {
        CLOGV("memcpy for callback time:(%5d msec)", (int)timer.durationMsecs());
    } else {
        CLOGV("PP for callback time:(%5d msec)", (int)timer.durationMsecs());
    }

    return ret;
}

#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
/*
 * RGB gain Calculation
 * @param RGBgain, inverseRGBGain(from CFG), pedestal 10bit
 * @return void
 * @Constraint precesion is 14bit, The number of gains is 4.
 */
void ExynosCamera::m_reverseProcessingApplyCalcRGBgain(int32_t *inRGBgain, int32_t *invRGBgain, int32_t *pedestal)
{
    const int kPrecision = 14;
    const int kNumOfGains = 4;
    int iValue = 0;
    double dValue = 0.0;

    for (int i = 0; i < kNumOfGains; i++) {
        dValue = (double)16383.0 * (double)1024.0 / (((double)16383.0 - (double)(pedestal[i])) * (double)(inRGBgain[i])); // golden
        iValue = (int)(dValue * (1 << kPrecision) + (double)0.5);
        invRGBgain[i] = iValue;
    }
}

/*
 * Gamma Image Processing
 * @param PixelInput, GammaLutX, GammaLutY
 * @return Pixel as Integer.
 * @Constraint input/output pixel use 14bit
 */
int ExynosCamera::m_reverseProcessingApplyGammaProc(int pixelIn, int32_t *lutX, int32_t *lutY)
{
    const int kFirstIdx = 0;
    const int kLastIdx = 31;
    const int kInImgBitWidth = 14; // 0 ~ 16383
    const int kOutImgBitWidth = 14; // 0 ~ 16383
    int deltaX = 0;
    int deltaY = 0;
    int baseX = 0;
    int baseY = 0;
    int shiftX = 0;
    int step = 0;
    int pixelOut = 0;
    int curIdx = 0;
    bool sign = (pixelIn >= 0) ? false : true;

    if (pixelIn < 0)
        pixelIn = -pixelIn;

    if (pixelIn > ((1 << kInImgBitWidth) - 1))
        pixelIn = ((1 << kInImgBitWidth) - 1); // clip to input gamma max.

    for (curIdx = kFirstIdx; curIdx < kLastIdx; curIdx++)
        if (lutX[curIdx] > pixelIn)
            break;

    if (curIdx == kFirstIdx) {
        deltaX = lutX[0];
        deltaY = lutY[0];
        baseX = 0;
        baseY = 0;
    } else if (curIdx == kLastIdx) {
        // [LAST_IDX] is difference value between last two values [LAST_IDX - 1] and [LAST_IDX]. This is h/w concept.
        deltaX = lutX[kLastIdx];
        deltaY = lutY[kLastIdx];
        baseX = lutX[kLastIdx - 1];
        baseY = lutY[kLastIdx - 1];
    } else {
        deltaX = lutX[curIdx] - lutX[curIdx - 1];
        deltaY = lutY[curIdx] - lutY[curIdx - 1];
        baseX = lutX[curIdx - 1];
        baseY = lutY[curIdx - 1];
    }

    // shift = 0(1), 1(2~3), 2(4~7), 3(8~15), ... , 13(8192 ~ 16383)
    for (shiftX = 0; shiftX < kInImgBitWidth; shiftX++) {
        if (deltaX == (1 << shiftX)) {
            step = (deltaY * (pixelIn - baseX)) >> shiftX;
            pixelOut = baseY + step;
            break;
        }
    }

    // Exceptional case clipping
    if (pixelOut > ((1 << kOutImgBitWidth) - 1))
        pixelOut = pixelOut & ((1 << kOutImgBitWidth) - 1);

    if (sign == true)
        pixelOut = -pixelOut;

    return pixelOut;
}

status_t ExynosCamera::m_reverseProcessingBayer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *inBuf, ExynosCameraBuffer *outBuf)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;

#if 0
    directDumpToFile(inBuf, m_cameraId, frame->getMetaFrameCount());
    for (int i = 0; i < inBuf->getMetaPlaneIndex(); i++) {
        if (inBuf->fd[i] > 0)
            memcpy(outBuf->addr[i], inBuf->addr[i], ALIGN(inBuf->size[i] / 2, 16));
    }
    directDumpToFile(outBuf, m_cameraId, frame->getMetaFrameCount());
#else
    int i;
    char *inImg = inBuf->addr[0];
    char *outImg = outBuf->addr[0];
    struct camera2_shot_ext shot_ext;
    frame->getMetaData(&shot_ext);
    struct camera2_stream *shot_stream = (struct camera2_stream *)(inBuf->addr[inBuf->getMetaPlaneIndex()]);
    int row = shot_stream->output_crop_region[3];
    int col = shot_stream->output_crop_region[2];
    int inPackBitWidth = 16;
    int outPackBitWidth = 16;

    switch (m_parameters[m_cameraId]->getBayerFormat(PIPE_3AC_REPROCESSING)) {
    case V4L2_PIX_FMT_SBGGR10:
    case V4L2_PIX_FMT_SBGGR12:
        inPackBitWidth = 12;
        break;
    default:
        break;
    }

    int inByteSize = (col * inPackBitWidth + 7) >> 3;
    int inStride = ((inByteSize + 15) >> 4) << 4;
    int outByteSize = (col * outPackBitWidth + 7) >> 3;
    int outStride = ((outByteSize + 15) >> 4) << 4;

    /* apply Postgamma/invWB/pedestalRemove/Stretching */
    int mod2_x = 0;
    int mod2_y = 0;
    int gainIdx = 0;
    int pixelIn = 0;
    int postGamma = 0;
    int invWB = 0;
    int clipV = 0;
    int outV = 0;
    int in_line_addr = 0;
    int out_line_addr = 0;
    int x_idx = 0;
    int y_idx = 0;
    int inRemain = 0;
    int outRemain = 0;
    int pixelInPed = 0;
    unsigned int inBuffer = 0;
    unsigned int outBuffer = 0;

    /*
     * inverse RGB gain calculation with pedestal stretching
     * Bayerorder Gr first.
     */
    int32_t invRGBgain[4] = { 0, 0, 0, 0 }; // Gr R Gb B
#if 0
    /* just test */
    int32_t samplePedestal[4] = { 896, 896, 896, 896 };
    int32_t sampleRgbGain[4] = { 1024, 1731, 2394, 1024 };
    int32_t sampleGammaLutX[32] = {0    , 512  , 1024 , 2048 , 2560 ,
        3072 , 3584 , 4096 , 4608 , 5120 ,
        5632 , 6144 , 6656 , 7168 , 7680 ,
        8192 , 8704 , 9216 , 9728 , 10240,
        10752, 11264, 11776, 12288, 12800,
        13312, 13824, 14336, 14848, 15360,
        15488, 1024 };

    int32_t sampleGammaLutY[32] = {0    , 112  , 239  , 533  , 707  ,
        896  , 1109 , 1347 , 1611 , 1902 ,
        2224 , 2567 , 2936 , 3342 , 3785 ,
        4266 , 4788 , 5354 , 5967 , 6625 ,
        7325 , 8066 , 8854 , 9694 , 10592,
        11553, 12575, 13665, 14829, 16069,
        16383, 16384};

    int32_t *pedestal = samplePedestal;
    int32_t *rgbGain = sampleRgbGain;
    int32_t *gammaX = sampleGammaLutX;
    int32_t *gammaY = sampleGammaLutY;
#else
    /* Meta Information */
    int32_t pedestal[4] = {
        /* G, R, B, G */
        (int32_t)shot_ext.shot.udm.dng.pedestal[1], /* G */
        (int32_t)shot_ext.shot.udm.dng.pedestal[0], /* R */
        (int32_t)shot_ext.shot.udm.dng.pedestal[2], /* B */
        (int32_t)shot_ext.shot.udm.dng.pedestal[1]  /* G */
    };

    /* HACK : RTA Team */
    for (i = 0; i < 4; i++) {
        if (pedestal[i] < 896) {
            pedestal[i] = 896;
        }
    }
    int32_t *rgbGain = (int32_t *)shot_ext.shot.udm.dng.wbgain;
    int32_t *gammaX = (int32_t *)shot_ext.shot.udm.dng.gammaXpnt;
    int32_t *gammaY = (int32_t *)shot_ext.shot.udm.dng.gammaYpnt;
#endif

    CLOGI("[F%d M%d] inBuf(%d),outBuf(%d), (%dx%d inPackBit:%d, in(%d, %d), out(%d, %d))",
            frame->getFrameCount(), frame->getMetaFrameCount(), inBuf->index, outBuf->index,
            col, row, inPackBitWidth, outPackBitWidth, inByteSize, inStride, outByteSize, outStride);

    if (col % 2 != 0) {
        CLOGE("Image width should be even! current image(w:%d, h:%d)", col, row);
        ret = INVALID_OPERATION;
        goto p_err;
    }

    if (((inStride % 16) != 0) || ((outStride % 16) != 0)) {
        CLOGE("Stride is not 16times! (inStride:%d, outStride:%d)", inStride, outStride);
        ret = INVALID_OPERATION;
        goto p_err;
    }

#if 0
    for (i = 0; i < 4; i+=4) {
        CLOGD("pedestal[%d][ %d, %d, %d, %d ]", i, pedestal[i], pedestal[i+1], pedestal[i+2], pedestal[i+3]);
        CLOGD("rgbgain[%d][ %d, %d, %d, %d ]", i, rgbGain[i], rgbGain[i+1], rgbGain[i+2], rgbGain[i+3]);
    }

    for (i = 0; i < 32; i+=4)
        CLOGD("gammaX[%d][ %d, %d, %d, %d ]", i, gammaX[i], gammaX[i+1], gammaX[i+2], gammaX[i+3]);

    for (i = 0; i < 32; i+=4)
        CLOGD("gammaY[%d][ %d, %d, %d, %d ]", i, gammaY[i], gammaY[i+1], gammaY[i+2], gammaY[i+3]);
#endif
    m_reverseProcessingApplyCalcRGBgain(rgbGain, invRGBgain, pedestal);

    for (i = 0; i < row * col; i++) {
        x_idx = i % col;
        y_idx = i / col;
        mod2_x = (x_idx % 2);
        mod2_y = (y_idx % 2);
        // for pixel order calc
        gainIdx = 2 * mod2_y + mod2_x;

        //=========================================
        // 0) Unpack Proc.
        // line address & buffer value/size initialize
        if (x_idx == 0) {
            in_line_addr = y_idx * inStride;
            out_line_addr = y_idx * outStride;
            inRemain = 0;
            inBuffer = 0;
            outRemain = 0;
            outBuffer = 0;
        }

        // Unpack core
        while (inRemain < inPackBitWidth) {
            inBuffer = (inImg[in_line_addr] << inRemain) + inBuffer;
            inRemain += 8;
            in_line_addr++;
        }

        pixelIn = inBuffer & ((1 << inPackBitWidth) - 1);
        inBuffer = inBuffer >> inPackBitWidth;
        inRemain -= inPackBitWidth;

        //=========================================
        // 1) input 12bit to 14bit
        pixelIn = pixelIn << 2;
        // input 10bit to 14bit
        if (inPackBitWidth < 12) {
            pixelIn = pixelIn << (12 - inPackBitWidth);
        }

        // 2) Post-gamma proc
        pixelInPed = pixelIn - pedestal[gainIdx];
        postGamma = m_reverseProcessingApplyGammaProc(pixelInPed, gammaX, gammaY);
        clipV = CLIP3(postGamma, 0, 16383);

        // 3) inv WB gain proc & pedestal stretching
        invWB = (invRGBgain[gainIdx] * clipV + (1 << 17)) >> 18; //unpack 10bit output

        // 4) clipping
        outV = MIN(invWB, 1023); //unpack 10bit output

        // 5) Pack core
        // constraint : outPackBitWidth > 8
        outBuffer = (outV << outRemain) + outBuffer;
        outRemain += outPackBitWidth;

        while (outRemain >= 8) {
            outImg[out_line_addr] = outBuffer & ((1 << 8) - 1);
            outBuffer = outBuffer >> 8;
            out_line_addr++;
            outRemain -= 8;
        }
    }
#endif

p_err:

    return ret;
}
#endif

void ExynosCamera::m_setApertureControl(frame_handle_components_t *components, struct camera2_shot_ext *shot_ext)
{
    int ret = 0;
    int aperture_value = shot_ext->shot.ctl.lens.aperture;

    if (components->previewFactory != NULL) {
        ret = components->previewFactory->setControl(V4L2_CID_IS_FACTORY_APERTURE_CONTROL, aperture_value, PIPE_3AA);
        if (ret < 0) {
            CLOGE("setcontrol() failed!!");
        } else {
            CLOGV("setcontrol() V4L2_CID_IS_FACTORY_APERTURE_CONTROL:(%d)", aperture_value);
        }
    }
}

status_t ExynosCamera::m_updateYsumValue(ExynosCameraFrameSP_sptr_t frame, ExynosCameraRequestSP_sprt_t request)
{
    int ret = NO_ERROR;
    struct camera2_udm udm;

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

    if (request == NULL) {
        CLOGE("Request is NULL");
        return BAD_VALUE;
    }

    ret = frame->getUserDynamicMeta(&udm);
    if (ret < 0) {
        CLOGE("getUserDynamicMeta fail, ret(%d)", ret);
        return ret;
    }
#ifdef USE_DUAL_CAMERA
    if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
        /* YSUM recoridng does not necessarily have to match the Y value of the recording buffer.
         * Because it looks at the trend between Y values. */
#endif
    {
        request->setYsumValue(&udm.scaler.ysumdata);
    }

    return ret;
}

/********************************************************************************/
/**                          VENDOR                                            **/
/********************************************************************************/
void ExynosCamera::m_vendorSpecificPreConstructorInitalize(__unused int cameraId, __unused int scenario)
{
}
void ExynosCamera::m_vendorSpecificConstructorInitalize(void)
{
#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif
}
void ExynosCamera::m_vendorSpecificPreDestructorDeinitalize(void)
{
    __unused status_t ret = 0;
}

void ExynosCamera::m_vendorUpdateExposureTime(__unused struct camera2_shot_ext *shot_ext)
{
}

status_t ExynosCamera::m_checkStreamInfo_vendor(status_t &ret)
{
    return ret;

}

status_t ExynosCamera::setParameters_vendor(__unused const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t ExynosCamera::m_checkMultiCaptureMode_vendor_update(__unused ExynosCameraRequestSP_sprt_t request)
{
    return NO_ERROR;
}

status_t ExynosCamera::processCaptureRequest_vendor_initDualSolutionZoom(__unused camera3_capture_request *request, status_t& ret)
{
    return ret;
}

status_t ExynosCamera::processCaptureRequest_vendor_initDualSolutionPortrait(__unused camera3_capture_request *request, status_t& ret)
{
    return ret;
}

status_t ExynosCamera::m_captureFrameHandler_vendor_updateConfigMode(__unused ExynosCameraRequestSP_sprt_t request, __unused ExynosCameraFrameFactory *targetfactory, __unused frame_type_t& frameType)
{
return NO_ERROR;
}

status_t ExynosCamera::m_captureFrameHandler_vendor_updateDualROI(__unused ExynosCameraRequestSP_sprt_t request, __unused frame_handle_components_t& components, __unused frame_type_t frameType)
{
    return NO_ERROR;
}

status_t ExynosCamera::m_previewFrameHandler_vendor_updateRequest(__unused ExynosCameraFrameFactory *targetfactory)
{
    return NO_ERROR;
}

status_t ExynosCamera::m_handlePreviewFrame_vendor_handleBuffer(__unused ExynosCameraFrameSP_sptr_t frame,
                                                                __unused int pipeId,
                                                                __unused ExynosCameraFrameFactory *factory,
                                                                __unused frame_handle_components_t& components,
                                                                __unused status_t& ret)
{
    return ret;
}

void ExynosCamera::m_vendorSpecificDestructorDeinitalize(void)
{
}

status_t ExynosCamera::m_setVendorBuffers()
{
    status_t ret = NO_ERROR;

    int width = 0, height = 0;
    int planeCount = 0;
    int buffer_count = 0;
    bool needMmap = false;
    exynos_camera_buffer_type_t buffer_type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&width, (uint32_t *)&height);

#ifdef USES_SW_VDIS
    if (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true) {
        m_exCameraSolutionSWVdis->setBuffer(m_bufferSupplier);

        int portId = m_parameters[m_cameraId]->getRecordingPortId();
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

        buffer_count = NUM_SW_VDIS_INTERNAL_BUFFERS;
        planeCount = 3;
        needMmap = true;
    } else
#endif
    {

#ifdef USE_DUAL_CAMERA
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
            && m_flagVideoStreamPriority == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

            planeCount = 3;
            buffer_count = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            needMmap = true;
        }
#endif
    }

    if (buffer_count > 0) {
        const buffer_manager_tag_t initBufTag;
        const buffer_manager_configuration_t initBufConfig;
        buffer_manager_configuration_t bufConfig;
        buffer_manager_tag_t bufTag;

        bufTag = initBufTag;
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            bufTag.pipeId[0] = PIPE_FUSION;
        } else
#endif
        {
            bufTag.pipeId[0] = PIPE_MCSC0;
            bufTag.pipeId[1] = PIPE_MCSC1;
            bufTag.pipeId[2] = PIPE_MCSC2;
#ifdef USES_SW_VDIS
            bufTag.pipeId[3] = PIPE_VDIS;
#endif
        }
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("MCSC_OUT_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create MCSC_OUT_BUF. ret %d", ret);
            return ret;
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = planeCount;
        bufConfig.size[0] = width * height;
        bufConfig.size[1] = width * height / 2;
        bufConfig.reqBufCount = buffer_count;
        bufConfig.allowedMaxBufCount = buffer_count;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.type = buffer_type;
        bufConfig.allocMode = allocMode;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = needMmap;
        bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_MCSC_OUT, BUF_TYPE_VENDOR);
        if (ret != NO_ERROR) {
            CLOGE("Failed to add MCSC_OUT_BUF. ret %d", ret);
            return ret;
        }
#else
        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc MCSC_OUT_BUF. ret %d", ret);
            return ret;
        }
#endif

        CLOGI("m_allocBuffers(MCSC_OUT_BUF) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            width, height,
            bufConfig.size[0], bufConfig.planeCount, buffer_count);
    }

    return ret;
}

void ExynosCamera::m_updateMasterCam(struct camera2_shot_ext *shot_ext)
{
    enum aa_sensorPlace masterCamera;
    enum aa_cameraMode cameraMode;

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        cameraMode = AA_CAMERAMODE_DUAL_SYNC;
    } else
#endif
    {
        cameraMode = AA_CAMERAMODE_SINGLE;
    }

    switch(m_cameraId) {
    case CAMERA_ID_BACK_0:
        masterCamera = AA_SENSORPLACE_REAR;
        break;
    case CAMERA_ID_BACK_1:
        masterCamera = AA_SENSORPLACE_REAR_SECOND;
        break;
    case CAMERA_ID_FRONT_0:
        masterCamera = AA_SENSORPLACE_FRONT;
        break;
    case CAMERA_ID_FRONT_1:
        masterCamera = AA_SENSORPLACE_FRONT_SECOND;
        break;
    default:
        CLOGE("Invalid camera Id(%d)", m_cameraId);
        break;
    }

    shot_ext->shot.uctl.masterCamera = masterCamera;
    shot_ext->shot.uctl.cameraMode = cameraMode;

}

#ifdef USES_SW_VDIS
status_t ExynosCamera::m_handleVdisFrame(ExynosCameraFrameSP_sptr_t frame,
                                              ExynosCameraRequestSP_sprt_t request,
                                              int pipeId,
                                              ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    int streamId = HAL_STREAM_ID_VIDEO;

    if (m_exCameraSolutionSWVdis != NULL) {
        ret = m_exCameraSolutionSWVdis->handleFrame(ExynosCameraSolutionSWVdis::SOLUTION_PROCESS_POST,
                                                    frame, pipeId,
                                                    (-1),
                                                    factory);

        //streamId = m_streamManager->getYuvStreamId(m_exCameraSolutionSWVdis->getCapturePipeId() - PIPE_MCSC0);
        streamId = HAL_STREAM_ID_VIDEO;

        ret = frame->getDstBuffer(pipeId, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), buffer.index, pipeId, ret);
            return ret;
        }

        entity_buffer_state_t dstBufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = frame->getDstBufferState(pipeId, &dstBufferState);
        if (ret != NO_ERROR) {
            CLOGE("Failed to set DST_BUFFER_STATE to replace target buffer(%d) to release pipeId(%d), ret(%d), frame(%d)",
                        buffer.index, pipeId, ret, frame->getFrameCount());
            return ret;
        }

        if (dstBufferState != ENTITY_BUFFER_STATE_COMPLETE) {
            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
        }

        request->setStreamBufferStatus(streamId, streamBufferState);

        ret = m_sendYuvStreamResult(request, &buffer, streamId, false, frame->getStreamTimestamp());
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
            return ret;
        }
    }

    return ret;
}
#endif //USES_SW_VDIS


}; /* namespace android */
