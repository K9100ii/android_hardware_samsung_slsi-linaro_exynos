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
#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraVendorTags.h"
#endif
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

    for (int i = 0; i < 2; i++) {
        m_previewDurationTime[i] = 0;
    }
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
#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastenAeThread = new mainCameraThread(this, &ExynosCamera::m_fastenAeThreadFunc,
            "fastenAeThread", PRIORITY_URGENT_DISPLAY);
    CLOGD("m_fastenAeThread created");
#endif

    /* m_ThumbnailCallback Thread */
    m_thumbnailCbThread = new mainCameraThread(this, &ExynosCamera::m_thumbnailCbThreadFunc, "m_thumbnailCbThread");
    CLOGD("m_thumbnailCbThread created");

#ifdef SAMSUNG_TN_FEATURE
    m_previewStreamPPPipeThread = new mainCameraThread(this, &ExynosCamera::m_previewStreamPPPipeThreadFunc, "PreviewPPPipeThread");
    CLOGD("PP Pipe Preview stream thread created");

    m_previewStreamPPPipe2Thread = new mainCameraThread(this, &ExynosCamera::m_previewStreamPPPipe2ThreadFunc, "PreviewPPPipe2Thread");
    CLOGD("PP Pipe2 Preview stream thread created");

    m_previewStreamPPPipe3Thread = new mainCameraThread(this, &ExynosCamera::m_previewStreamPPPipe3ThreadFunc, "PreviewPPPipe3Thread");
    CLOGD("PP Pipe3 Preview stream thread created");

    m_dscaledYuvStallPostProcessingThread = new mainCameraThread(
            this, &ExynosCamera::m_dscaledYuvStallPostProcessingThreadFunc, "m_dscaledYuvStallPostProcessingThread");
    CLOGD("m_dscaledYuvStallPostProcessingThread created");

    m_gscPreviewCbThread = new mainCameraThread(this, &ExynosCamera::m_gscPreviewCbThreadFunc, "m_gscPreviewCbThread");
    CLOGD("m_gscPreviewCbThread created");
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    m_postVC0Thread = new mainCameraThread(this, &ExynosCamera::m_postVC0ThreadFunc, "m_postVC0Thread");
    CLOGD("m_postVC0Thread created");
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    m_sensorListenerThread = new mainCameraThread(this, &ExynosCamera::m_sensorListenerThreadFunc, "sensorListenerThread");
    CLOGD("m_sensorListenerThread created");

    m_sensorListenerUnloadThread = new mainCameraThread(this, &ExynosCamera::m_sensorListenerUnloadThreadFunc, "m_sensorListenerUnloadThread");
    CLOGD("m_sensorListenerUnloadThread created");
#endif

    return;
}

status_t ExynosCamera::m_vendorReInit(void)
{
    m_captureResultToggle = 0;
    m_displayPreviewToggle = 0;
#ifdef SAMSUNG_EVENT_DRIVEN
    m_eventDrivenToggle = 0;
#endif
#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif
    m_flagUseInternalyuvStall = false;
    m_flagVideoStreamPriority = false;
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    m_dramTestQCount = 0;
    m_dramTestDoneCount = 0;
#endif
#ifdef SAMSUNG_SSM
    m_SSMCommand = -1;
    m_SSMMode = -1;
    m_SSMState = SSM_STATE_NONE;
    m_SSMRecordingtime = 0;
    m_SSMOrgRecordingtime = 0;
    m_SSMSkipToggle = 0;
    m_SSMRecordingToggle = 0;
    m_SSMFirstRecordingRequest = 0;
    m_SSMDetectDurationTime = 0;
    m_checkRegister = false;

    if (m_SSMAutoDelay > 0) {
        m_SSMUseAutoDelayQ = true;
    } else {
        m_SSMUseAutoDelayQ = false;
    }
#endif
#ifdef SAMSUNG_HYPERLAPSE_DEBUGLOG
    m_recordingCallbackCount = 0;
#endif

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

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }
#endif

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
#ifdef CAMERA_FAST_ENTRANCE_V1
            && m_fastEntrance == false
#endif
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
    m_latestRequestListLock.lock();
    m_latestRequestList.clear();
    m_latestRequestListLock.unlock();
#ifdef USE_DUAL_CAMERA
    if (isDualMode == true) {
        if (m_slaveShotDoneQ != NULL) {
            m_slaveShotDoneQ->release();
        }

        m_latestRequestListLock.lock();
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

#ifdef SAMSUNG_FOCUS_PEAKING
    if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PRO) {
#ifdef SUPPORT_DEPTH_MAP
        if (parameters->isDepthMapSupported() == true) {
            ret = m_setDepthInternalBuffer();
            if (ret != NO_ERROR) {
                CLOGE("Failed to m_setDepthInternalBuffer. ret %d", ret);
                return false;
            }
        }
#endif
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

#ifdef SAMSUNG_TN_FEATURE
    m_initUniPP();
#endif

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

#ifdef SAMSUNG_FACTORY_SSM_TEST
    if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_SSM_TEST) {
        int ret = 0;
        enum NODE_TYPE nodeType = INVALID_NODE;

        nodeType = factory->getNodeType(PIPE_FLITE);

        if (factory == NULL) {
            CLOGE("FrameFactory is NULL!!");
        } else {
            ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, FRS_SSM_MODE_FACTORY_TEST, PIPE_3AA, nodeType);
            if (ret) {
                CLOGE("setcontrol() failed!!");
            } else {
                CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL: FRS_SSM_MODE_FACTORY_TEST");
            }
        }
    }
#endif

    CLOGI("Threads run start");
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
                    if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        (this->*frameCreateHandler)(request, factory, FRAME_TYPE_PREVIEW_DUAL_SLAVE);
                    } else {
                        m_createInternalFrameFunc(NULL, false, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
                    }
                } else {
                    m_createInternalFrameFunc(NULL, false, REQ_SYNC_NONE, FRAME_TYPE_INTERNAL_SLAVE);
                }
            }
        } else {
            /* in case of postStandby on */
            for (uint32_t i = 0; i < prepare; i++) {
                ret = m_createInternalFrameFunc(NULL, false, REQ_SYNC_NONE,
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

void ExynosCamera::m_checkRequestStreamChanged(char *handlerName, uint32_t currentStreamBit)
{
    uint32_t changedStreamBit = currentStreamBit;
    bool hasStream[HAL_STREAM_ID_MAX] = {false,};
    bool isSWVdis = 0;
    bool isHIFIVideo = 0;

    changedStreamBit ^= (m_prevStreamBit);

    if (changedStreamBit == 0
#ifdef SAMSUNG_TN_FEATURE
        || (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_PANORAMA)
        || (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SLOW_MOTION)
#endif
        ) {
        return;
    }

    for (int id = 0; id < HAL_STREAM_ID_MAX; id++) {
        hasStream[id] = (currentStreamBit >> id) & 1;
    }

    if (!strcmp(handlerName, "Preview")) {
        CLOGD("[%s Stream] PREVIEW(%d), VIDEO(%d), CALLBACK(%d), ZSL_OUTPUT(%d), DEPTHMAP(%d)",
                                     handlerName,
                                     hasStream[HAL_STREAM_ID_PREVIEW],
                                     hasStream[HAL_STREAM_ID_VIDEO],
                                     hasStream[HAL_STREAM_ID_CALLBACK],
                                     hasStream[HAL_STREAM_ID_ZSL_OUTPUT],
                                     hasStream[HAL_STREAM_ID_DEPTHMAP]);

#ifdef SAMSUNG_SW_VDIS
        isSWVdis = m_configurations->getMode(CONFIGURATION_SWVDIS_MODE);
#endif

#ifdef SAMSUNG_HIFI_VIDEO
        isHIFIVideo = m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE);
#endif

        if (hasStream[HAL_STREAM_ID_VIDEO]) {
            CLOGD("[%s Mode] SWVDIS mode(%d), HIFI_VIDEO Mode(%d)",
                                     handlerName, (int32_t)isSWVdis, (int32_t)isHIFIVideo);
        }
    } else if (!strcmp(handlerName, "Capture")) {
        CLOGD("[%s Stream] RAW(%d), ZSL_INPUT(%d), JPEG(%d), CALLBACK_STALL(%d), " \
                                     "DEPTHMAP_STALL(%d), THUMBNAIL_CALLBACK(%d)",
                                     handlerName,
                                     hasStream[HAL_STREAM_ID_RAW],
                                     hasStream[HAL_STREAM_ID_ZSL_INPUT],
                                     hasStream[HAL_STREAM_ID_JPEG],
                                     hasStream[HAL_STREAM_ID_CALLBACK_STALL],
                                     hasStream[HAL_STREAM_ID_DEPTHMAP_STALL],
                                     hasStream[HAL_STREAM_ID_THUMBNAIL_CALLBACK]);
    } else if (!strcmp(handlerName, "Vision")) {
        CLOGD("[%s Stream] VISION(%d)", handlerName, hasStream[HAL_STREAM_ID_VISION]);
    } else {
        CLOGD("Invalid Handler name");
    }

    m_prevStreamBit = currentStreamBit;
    return;
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
#ifdef SAMSUNG_SSM
    case CONFIG_MODE::SSM_240:
        {
            uint32_t currentSetfile = ISS_SUB_SCENARIO_FHD_240FPS;
            uint32_t currentScenario = FIMC_IS_SCENARIO_HIGH_SPEED_DUALFPS;

            setfile = currentSetfile | (currentScenario << 16);
            setfileReprocessing = ISS_SUB_SCENARIO_VIDEO_HIGH_SPEED;
            yuvRange = YUV_LIMITED_RANGE;
        }
        break;
#endif
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
    //int count = 0;

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

    ret = flush();
    if (ret != NO_ERROR) {
        CLOGE("Failed to flush. ret %d", ret);
        return ret;
    }

#if 0
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
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->join();
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    }
#endif

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

#ifdef SAMSUNG_TN_FEATURE
    m_deinitUniPP();
#endif

    /* Wait for finishing pre-processing threads */
    if (m_shotDoneQ != NULL) {
        m_shotDoneQ->release();
    }
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
#ifdef SAMSUNG_TN_FEATURE
    stopThreadAndInputQ(m_dscaledYuvStallPostProcessingThread, 2, m_dscaledYuvStallPPCbQ, m_dscaledYuvStallPPPostCbQ);
    stopThreadAndInputQ(m_gscPreviewCbThread, 1, m_pipeFrameDoneQ[PIPE_GSC]);
#endif
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    stopThreadAndInputQ(m_postVC0Thread, 1, m_postVC0Q);
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

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    if (m_hifiVideoBufferQ != NULL) {
        m_hifiVideoBufferQ->release();
        m_hifiVideoBufferCount = 0;
    }
#endif

#ifdef SAMSUNG_SSM
    if (m_SSMAutoDelayQ != NULL) {
        m_SSMAutoDelayQ->release();
    }

    if (m_SSMSaveBufferQ != NULL) {
        m_SSMSaveBufferQ->release();
    }
#endif

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

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->join();
    }
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_fastEntrance == true) {
        m_waitFastenAeThreadEnd();
    } else
#endif
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

#ifdef SAMSUNG_TN_FEATURE
    m_deinitUniPP();
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_fusionZoomPreviewWrapper != NULL) {
        if (m_fusionZoomPreviewWrapper->m_getIsInit() == true) {
            m_fusionZoomPreviewWrapper->m_deinitDualSolution(getCameraId());
        }
    }

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_fusionZoomCaptureWrapper != NULL) {
        if (m_fusionZoomCaptureWrapper->m_getIsInit() == true) {
            m_fusionZoomCaptureWrapper->m_deinitDualSolution(getCameraId());
        }
    }
#endif
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    if (m_bokehPreviewWrapper != NULL) {
        if (m_bokehPreviewWrapper->m_getIsInit() == true) {
            m_bokehPreviewWrapper->m_deinitDualSolution(getCameraId());
        }
    }

    if (m_bokehCaptureWrapper != NULL) {
        if (m_bokehCaptureWrapper->m_getIsInit() == true) {
            m_bokehCaptureWrapper->m_deinitDualSolution(getCameraId());
        }
    }
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, LIBRARY_DEINIT_END, 0);

    /* Wait for finishing post-processing thread */
    stopThreadAndInputQ(m_previewStreamBayerThread, 1, m_pipeFrameDoneQ[PIPE_FLITE]);
    stopThreadAndInputQ(m_previewStream3AAThread, 1, m_pipeFrameDoneQ[PIPE_3AA]);
#ifdef SUPPORT_GMV
    stopThreadAndInputQ(m_previewStreamGMVThread, 1, m_pipeFrameDoneQ[PIPE_GMV]);
#endif
    stopThreadAndInputQ(m_previewStreamISPThread, 1, m_pipeFrameDoneQ[PIPE_ISP]);
    stopThreadAndInputQ(m_previewStreamMCSCThread, 1, m_pipeFrameDoneQ[PIPE_MCSC]);
#ifdef SAMSUNG_TN_FEATURE
    stopThreadAndInputQ(m_previewStreamPPPipeThread, 1, m_pipeFrameDoneQ[PIPE_PP_UNI]);
    stopThreadAndInputQ(m_previewStreamPPPipe2Thread, 1, m_pipeFrameDoneQ[PIPE_PP_UNI2]);
    stopThreadAndInputQ(m_previewStreamPPPipe3Thread, 1, m_pipeFrameDoneQ[PIPE_PP_UNI3]);
#endif
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
#ifdef SAMSUNG_TN_FEATURE
    stopThreadAndInputQ(m_dscaledYuvStallPostProcessingThread, 2, m_dscaledYuvStallPPCbQ, m_dscaledYuvStallPPPostCbQ);
    stopThreadAndInputQ(m_gscPreviewCbThread, 1, m_pipeFrameDoneQ[PIPE_GSC]);
#endif
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    stopThreadAndInputQ(m_postVC0Thread, 1, m_postVC0Q);
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

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    if (m_hifiVideoBufferQ != NULL) {
        m_hifiVideoBufferQ->release();
        m_hifiVideoBufferCount = 0;
    }
#endif

#ifdef SAMSUNG_SSM
    if (m_SSMAutoDelayQ != NULL) {
        m_SSMAutoDelayQ->release();
    }

    if (m_SSMSaveBufferQ != NULL) {
        m_SSMSaveBufferQ->release();
    }
#endif

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

#ifdef SAMSUNG_RTHDR
    int bufferCount = (m_parameters[cameraId]->getRTHdr() == CAMERA_WDR_ON) ?
        m_exynosconfig->current->bufInfo.num_fastaestable_buffer :
        m_exynosconfig->current->bufInfo.num_fastaestable_buffer - 4;
#else
    int bufferCount = m_exynosconfig->current->bufInfo.num_fastaestable_buffer - 4;
#endif

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
#ifdef USE_DUAL_CAMERA
        if (newFrame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE
            || newFrame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE
            || newFrame->getFrameType() == FRAME_TYPE_INTERNAL_SLAVE
            || newFrame->getFrameType() == FRAME_TYPE_TRANSITION_SLAVE) {
            m_previewDurationTimer[1].stop();
            m_previewDurationTime[1] = (int)m_previewDurationTimer[1].durationMsecs();
            CLOGV("frame duration time(%d)", m_previewDurationTime[1]);
            m_previewDurationTimer[1].start();
        } else
#endif
        {
            m_previewDurationTimer[0].stop();
            m_previewDurationTime[0] = (int)m_previewDurationTimer[0].durationMsecs();
            CLOGV("frame duration time(%d)", m_previewDurationTime[0]);
            m_previewDurationTimer[0].start();
        }
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
#ifdef SAMSUNG_TN_FEATURE
                    || pipeId == PIPE_PP_UNI || pipeId == PIPE_PP_UNI2
#endif
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

#ifdef SAMSUNG_TN_FEATURE
    CameraMetadata *meta = request->getServiceMeta();
    if (meta->exists(SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT)) {
		m_checkMultiCaptureMode_vendor_captureHint(request, meta);
    } else
#endif /* SAMSUNG_TN_FEATURE */
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
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM)
            m_configurations->checkFusionCaptureMode();
#endif
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
                                                 bool flagFinishFactoryStart,
                                                 __unused enum Request_Sync_Type syncType,
                                                 __unused frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    bool isNeedBayer = (request != NULL) ? true : false;
    bool isNeedDynamicBayer = false;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraRequestSP_sprt_t newRequest = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    int currentCameraId = 0;
    int pipeId = -1;
    int dstPos = 0;
    uint32_t needDynamicBayerCount = m_configurations->getModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT);
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
    currentCameraId = components.currentCameraId;

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
    factory->setRequest(PIPE_3AF, false);
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
#ifdef SAMSUNG_TN_FEATURE
    factory->setRequest(PIPE_PP_UNI, false);
    factory->setRequest(PIPE_PP_UNI2, false);
    factory->setRequest(PIPE_PP_UNI3, false);
#endif

    if (needDynamicBayerCount > 0) {
        CLOGD("needDynamicBayerCount %d", needDynamicBayerCount);
        m_configurations->setModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT, needDynamicBayerCount - 1);
        isNeedBayer = true;
        isNeedDynamicBayer = isNeedBayer;
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
            isNeedDynamicBayer = isNeedBayer;
            factory->setRequest(PIPE_VC0, isNeedBayer);
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
            isNeedDynamicBayer = isNeedBayer;
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

    if (request == NULL) {
        m_latestRequestListLock.lock();
        List<ExynosCameraRequestSP_sprt_t>::iterator r;
        if (m_latestRequestList.size() <= 0) {
            CLOGE("Request is NULL");
        } else {
            r = m_latestRequestList.begin();
            newRequest = *r;
        }
        m_latestRequestListLock.unlock();
    } else {
        newRequest = request;
    }

    if (newRequest != NULL) {
        service_shot_ext = newRequest->getServiceShot();
        if (service_shot_ext == NULL) {
            CLOGE("Get service shot fail, requestKey(%d)", newRequest->getKey());
            m_metadataConverter->initShotData(m_currentInternalShot[currentCameraId]);
        } else {
            *m_currentInternalShot[currentCameraId] = *service_shot_ext;
        }
    } else {
        m_metadataConverter->initShotData(m_currentInternalShot[currentCameraId]);
    }

    m_updateShotInfoLock.lock();
    m_updateLatestInfoToShot(m_currentInternalShot[currentCameraId], frameType);

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
        ret = m_generateTransitionFrame(factory, &m_processList, &m_processLock, newFrame, frameType);
#endif

    } else {
        ret = m_generateInternalFrame(factory, &m_processList, &m_processLock, newFrame, internalframeType);
    }

    m_updateShotInfoLock.unlock();

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

    if (newFrame->getStreamRequested(STREAM_TYPE_CAPTURE) || newFrame->getStreamRequested(STREAM_TYPE_RAW))
        m_updateExposureTime(m_currentInternalShot[currentCameraId]);

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, USER_DATA, CREATE_INTERNAL_FRAME, newFrame->getFrameCount());

    /* BcropRegion is calculated in m_generateFrame    */
    /* so that updatePreviewStatRoi should be set here.*/
    components.parameters->getHwBayerCropRegion(
                    &bayerCropRegion.w,
                    &bayerCropRegion.h,
                    &bayerCropRegion.x,
                    &bayerCropRegion.y);
    components.parameters->updatePreviewStatRoi(m_currentInternalShot[currentCameraId],
                                                    &bayerCropRegion);

    if (flagFinishFactoryStart == false) {
    /* Single flash capture has 2 frame control delays,
        * and the flash mode of the first request is set in the all prepare frame meta as shown below.
        * Single Flash mode
        * Prepare frame                 Request frame
        * N - 3 (single flash start)   N (want frame with flash applied!!)
        * N - 2
        * N - 1 (flash applied to frame)
        *
        * This causes a single flash to be applied to N-1 of the prepare frame,
        * not to the first request frame.
        * To prevent this, we did not copy the flash mode value to the prepare buffer
        * and single flash meta copy to N-2 frame  in driver.
        */
        m_currentInternalShot[currentCameraId]->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        CLOGV("flashMode is NONE");
    }

    ret = newFrame->setMetaData(m_currentInternalShot[currentCameraId]);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaData with m_currentInternalShot. framecount %d ret %d",
                newFrame->getFrameCount(), ret);
        return ret;
    }

    newFrame->setNeedDynamicBayer(isNeedDynamicBayer);

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

#ifdef SAMSUNG_SENSOR_LISTENER
    m_getSensorListenerData(&components);
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
    uint32_t needDynamicBayerCount = m_configurations->getModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT);
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
#ifdef SAMSUNG_FACTORY_DRAM_TEST
                || m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST
#endif
                ) {
                pipeId = flite3aaM2M ? PIPE_FLITE : PIPE_3AA;
                dstPos = factory->getNodeType(PIPE_VC0);
            } else {
                dstPos = factory->getNodeType(PIPE_3AC);
            }
            bufferDirection = DST_BUFFER_DIRECTION;

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR || buffer.index < 0) {
                if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
                    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
                        if (frame->getRequest(PIPE_3AC) == true) {
                            dstPos = factory->getNodeType(PIPE_3AC);
                            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                            if (ret != NO_ERROR || buffer.index < 0) {
                                CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                                        frame->getFrameCount(), buffer.index, dstPos, ret);
                            }

                            ret = m_bufferSupplier->putBuffer(buffer);
                            if (ret !=  NO_ERROR) {
                                CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                                        frame->getFrameCount(), buffer.index,
                                        bufferDirection, pipeId, ret);
                            }
                        }
                    } else {
                        if (frame->getRequest(PIPE_VC0) == true) {
                            pipeId = flite3aaM2M ? PIPE_FLITE : PIPE_3AA;
                            dstPos = factory->getNodeType(PIPE_VC0);
                            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                            if (ret != NO_ERROR || buffer.index < 0) {
                                CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                                        frame->getFrameCount(), buffer.index, dstPos, ret);
                            }

                            ret = m_bufferSupplier->putBuffer(buffer);
                            if (ret !=  NO_ERROR) {
                                CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                                        frame->getFrameCount(), buffer.index,
                                        bufferDirection, pipeId, ret);
                            }
                        }
                    }
                } else {
                    CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                            frame->getFrameCount(), buffer.index, dstPos, ret);
                    goto SKIP_PUTBUFFER;
                }
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

        if (frame->getNeedDynamicBayer()) {
            m_configurations->setModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT, ++needDynamicBayerCount);
            CLOGW("[F%d B%d]Dynamic reprocessing mode. needDynamicBayerCount(%d), Skip ERROR_REQUEST callback. pipeId %d",
                    frame->getFrameCount(), buffer.index, needDynamicBayerCount, pipeId);
        } else if (request == NULL) {
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
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    } else if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST) {
        camera2_stream *shot_stream = (struct camera2_stream *)(buffer.addr[buffer.getMetaPlaneIndex()]);

        if (shot_stream != NULL) {
            if (m_dramTestQCount < components.parameters->getFactoryDramTestCount()) {
                if (m_postVC0Thread->isRunning() == false) {
                    m_postVC0Thread->run();
                }
                m_dramTestQCount++;

                CLOGD("[FRS] push VC0Q F:%d Q:%d D:%d",
                    shot_stream->fcount, m_dramTestQCount, m_dramTestDoneCount);

                m_postVC0Q->pushProcessQ(&buffer);
                goto SKIP_PUTBUFFER;
            }

            CLOGV("[FRS] skip push VC0Q %d", shot_stream->fcount);
        }

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret !=  NO_ERROR) {
            CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index,
                    bufferDirection, pipeId, ret);
        }
#endif
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
                if (frame->getStreamRequested(STREAM_TYPE_CAPTURE)
                    || frame->getStreamRequested(STREAM_TYPE_RAW)
                    || frame->getNeedDynamicBayer() == true
                    || flagForceHoldDynamicBayer) {
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
                        frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
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

#ifdef SAMSUNG_FOCUS_PEAKING
    if (bufferState != ENTITY_BUFFER_STATE_ERROR
        && frame->hasPPScenario(PP_SCENARIO_FOCUS_PEAKING) == true) {
        frame->setReleaseDepthBuffer(false);
    }
#endif

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

#ifdef DEBUG_STREAM_CONFIGURATIONS
    CLOGD("DEBUG_STREAM_CONFIGURATIONS:Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#else
    CLOGV("Capture request(%d) #out(%d)",
         request->frame_number, request->num_output_buffers);
#endif

#ifdef DEBUG_IRIS_LEAK
    if (m_cameraId == CAMERA_ID_SECURE) {
        CLOGD("[IRIS_LEAK] R(%d)", request->frame_number);
    }
#endif

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
    if (request->input_buffer != NULL) {
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

#ifdef SAMSUNG_SSM
    if (m_SSMState == SSM_STATE_POSTPROCESSING && m_SSMSaveBufferQ->getSizeOfProcessQ() > 0) {
        int ratio = SSM_MIPI_SPEED_HD / SSM_PREVIEW_FPS;
        m_SSMRecordingToggle = (m_SSMRecordingToggle + 1) % ratio;
        if (m_SSMRecordingToggle != 0) {
            ret = m_pushServiceRequest(request, req, true);
            ret = m_pushRunningRequest(req);
            ret = m_sendForceSSMResult(req);
            goto req_err;
        }
    }
#endif

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

        CLOGD("first frame: [R%d] Start FrameFactory. state %d ", request->frame_number, m_getState());
        setPreviewProperty(true);

#ifdef SAMSUNG_UNIPLUGIN
        if (m_uniPluginThread != NULL) {
            m_uniPluginThread->join();
        }
#endif

        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
#ifdef USE_DUAL_CAMERA
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
        ) {
            if (processCaptureRequest_vendor_initDualSolutionZoom(request, ret) != NO_ERROR)
                goto req_err;

        } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
            if (processCaptureRequest_vendor_initDualSolutionPortrait(request, ret) != NO_ERROR)
                goto req_err;
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

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            ret = m_waitFastenAeThreadEnd();
            if (ret != NO_ERROR) {
                CLOGE("fastenAeThread exit with error");
                m_transitState(EXYNOS_CAMERA_STATE_ERROR);
                ret = NO_INIT;
                goto req_err;
            }
        } else
#endif
        {
            /* for FAST AE Stable */
#ifdef USE_DUAL_CAMERA
            if (m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                /* for FAST AE Stable */
                if (m_parameters[m_cameraIds[1]]->checkFastenAeStableEnable() == true) {
#ifdef SAMSUNG_READ_ROM
                    m_waitReadRomThreadEnd();
#endif

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
#ifdef SAMSUNG_READ_ROM
                    m_waitReadRomThreadEnd();
#endif

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

#ifdef SAMSUNG_SENSOR_LISTENER
        if (m_configurations->getMode(CONFIGURATION_VISION_MODE) == false) {
            if(m_sensorListenerThread != NULL) {
                m_sensorListenerThread->run();
            }
        }
#endif
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

#ifdef SAMSUNG_SSM
    if (m_SSMState == SSM_STATE_PREVIEW_ONLY
        || m_SSMState == SSM_STATE_PROCESSING
        || m_SSMState == SSM_STATE_POSTPROCESSING) {
        requiredRequestCount = 4;
    }
#endif

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
    int previewDurationTime = 0;

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_REPROCESSING_SLAVE) {
        previewDurationTime = m_previewDurationTime[1];
    } else
#endif
    {
        previewDurationTime = m_previewDurationTime[0];
    }

    if (previewDurationTime != 0) {
        runtime_fps = (int)(1000 / previewDurationTime);
    }

    int burstshotTargetFps = m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET);

#ifdef SAMSUNG_TN_FEATURE
    if (burstshotTargetFps == BURSTSHOT_MAX_FPS && BURSTSHOT_MAX_FPS == 20) {
        m_isSkipBurstCaptureBuffer_vendor(frameType, isSkipBuffer, ratio, runtime_fps, burstshotTargetFps);
    } else
#endif /* SAMSUNG_TN_FEATURE */
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

        if (m_captureResultToggle != 0) {
            isSkipBuffer = true;
        }
    }

    CLOGV("m_captureResultToggle(%d) previewDurationTime(%d) ratio(%d), targetFps(%d)",
            m_captureResultToggle, previewDurationTime, ratio, burstshotTargetFps);

    CLOGV("[%d][%d][%d][%d] ratio(%d)",
            m_burstFps_history[0], m_burstFps_history[1],
            m_burstFps_history[2], m_burstFps_history[3], ratio);

#ifdef OIS_CAPTURE
    frame_handle_components_t components;
    getFrameHandleComponents(frameType, &components);

    ExynosCameraActivitySpecialCapture *sCaptureMgr = components.activityControl->getSpecialCaptureMgr();
    if (m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true
        && sCaptureMgr->getOISCaptureFcount() == 0) {
        isSkipBuffer = true;
    }
#endif

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
    int currentCameraId = 0;
    uint32_t requestKey = 0;
    bool captureFlag = false;
    bool rawStreamFlag = false;
    bool zslFlag = false;
    bool yuvCbStallFlag = false;
    bool thumbnailCbFlag = false;
    bool isNeedThumbnail = false;
    bool isNeedHWFCJpeg = false;
    bool depthStallStreamFlag = false;
    int pipeId = -1;
    int yuvStallPortUsage = YUV_STALL_USAGE_DSCALED;
    frame_handle_components_t components;
    uint32_t streamConfigBit = 0;
    uint32_t needDynamicBayerCount = FLASH_MAIN_TIMEOUT_COUNT;
    ExynosCameraActivitySpecialCapture *sCaptureMgr = NULL;
    ExynosCameraActivityFlash *flashMgr = NULL;
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
    currentCameraId = components.currentCameraId;

    flag3aaVraM2M = (components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING,
                                    PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M);

    sCaptureMgr = components.activityControl->getSpecialCaptureMgr();
    flashMgr = components.activityControl->getFlashMgr();

    if (targetfactory == NULL) {
        CLOGE("targetfactory is NULL");
        return INVALID_OPERATION;
    }

    CLOGD("Capture request. requestKey %d frameCount %d frameType %d",
            request->getKey(),
            request->getFrameCount(),
            frameType);

    if (flashMgr->getNeedCaptureFlash() == true
        && flashMgr->getNeedFlashMainStart() == true) {
        flashMgr->setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_MAIN_START);
        flashMgr->setNeedFlashMainStart(false);
    }

    m_startPictureBufferThread->join();

    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_3AC_REPROCESSING, false);
    targetfactory->setRequest(PIPE_3AG_REPROCESSING, false);
    targetfactory->setRequest(PIPE_ISPC_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC0_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC1_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC2_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC_JPEG_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC_THUMB_REPROCESSING, false);
    targetfactory->setRequest(PIPE_MCSC5_REPROCESSING, false);
#ifdef SAMSUNG_TN_FEATURE
    targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING, false);
    targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING2, false);
#endif
    targetfactory->setRequest(PIPE_VRA_REPROCESSING, false);

    if (components.parameters->isReprocessing() == true) {
        if (components.parameters->isUseHWFC()) {
            targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
            targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
        }
    }

    m_captureFrameHandler_vendor_updateConfigMode(request, targetfactory, frameType);

#ifdef SAMSUNG_TN_FEATURE
    if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_NONE
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        && (m_scenario != SCENARIO_DUAL_REAR_PORTRAIT && m_scenario != SCENARIO_DUAL_FRONT_PORTRAIT)
#endif
    ) {
        m_captureFrameHandler_vendor_updateCaptureMode(request, components, sCaptureMgr);
    }
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
        CLOGD("[BokehCapture] DualCaptureFlag(%d)", m_configurations->getDualCaptureFlag());

        if (m_configurations->getMode(CONFIGURATION_FACTORY_TEST_MODE) == false &&
            m_configurations->getDualCaptureFlag() == 1) {
            m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, MULTI_SHOT_MODE_MULTI2);
            m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, LD_CAPTURE_COUNT_MULTI2);

            CLOGD("[BokehCapture] LLS(%d), getLDCaptureMode(%d), getLDCaptureCount(%d), OIS_CAPTURE_MODE(%d)",
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE),
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE),
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT),
                m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE));
        }
    }
#endif

    if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) != MULTI_CAPTURE_MODE_NONE
        || m_currentMultiCaptureMode != MULTI_CAPTURE_MODE_NONE) {
        if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_NONE
            && m_currentMultiCaptureMode != m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE)) {
#ifdef OIS_CAPTURE
            if (m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true) {
                int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI;

                sCaptureMgr->setMultiCaptureMode(true);

                if (components.previewFactory == NULL) {
                    CLOGE("FrameFactory is NULL!!");
                } else {
                    ret = components.previewFactory->setControl(V4L2_CID_IS_INTENT, captureIntent, PIPE_3AA);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_IS_INTENT:(%d)", captureIntent);
                    }
                }

                sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
                components.activityControl->setOISCaptureMode(true);
                CLOGD("OISMODE(%d), captureIntent(%d)",
                        m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE), captureIntent);
            }
#endif
            m_configurations->setModeValue(CONFIGURATION_MULTI_CAPTURE_MODE, m_currentMultiCaptureMode);
            CLOGD("Multi Capture mode(%d) start. requestKey %d frameCount %d",
                    m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE),
                    request->getKey(), request->getFrameCount());
        }

        if ((m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_BURST
                || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_AGIF)
            && m_isSkipBurstCaptureBuffer(frameType)) {
            m_sendForceStallStreamResult(request);
            m_lastMultiCaptureSkipRequest = requestKey;
            return ret;
        }

        m_lastMultiCaptureNormalRequest = requestKey;
    }

#ifdef SAMSUNG_DOF
    int shotMode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);
    m_configurations->setMode(CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE, false);
    if (shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS) {
        if (m_isFocusSet && m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == false) {
            sCaptureMgr->resetDynamicPickCaptureFcount();
            m_configurations->setMode(CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE, true);
            sCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_DYNAMIC_PICK,
                ExynosCameraActivitySpecialCapture::DYNAMIC_PICK_OUTFOCUS);
        }
    }
#endif

    if (m_configurations->getCaptureExposureTime() > CAMERA_SENSOR_EXPOSURE_TIME_MAX
        && m_configurations->getSamsungCamera() == true) {
        m_longExposureRemainCount = m_configurations->getLongExposureShotCount();
        m_preLongExposureTime = m_configurations->getLongExposureTime();
        CLOGD("m_longExposureRemainCount(%d) m_preLongExposureTime(%lld)",
            m_longExposureRemainCount, m_preLongExposureTime);
    } else
#endif /* SAMSUNG_TN_FEATURE */
    {
        m_longExposureRemainCount = 0;
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        targetfactory->setRequest(PIPE_SYNC_REPROCESSING, false);
        targetfactory->setRequest(PIPE_FUSION_REPROCESSING, false);

        if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
            if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE)
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
                || (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
#endif
                )
#else
            if (m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC)
#endif
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
#ifdef SAMSUNG_TN_FEATURE
        yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT);
        yuvStallPortUsage = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE);
#endif
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_JPEG:
#ifdef SAMSUNG_TN_FEATURE
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_JPEG), YuvStallPortUsage()(%d)",
                    request->getKey(), yuvStallPortUsage);
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
#endif
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
                    isNeedHWFCJpeg = true;
                    targetfactory->setRequest(PIPE_MCSC_THUMB_REPROCESSING, isNeedThumbnail);
                    targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, isNeedHWFCJpeg);
                    targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, isNeedThumbnail);
                }
            }

            captureFlag = true;
            break;
        case HAL_STREAM_ID_RAW:
            CLOGV("requestKey %d buffer-StreamType(HAL_STREAM_ID_RAW)",
                     request->getKey());
            if (components.parameters->isUse3aaDNG()) {
                targetfactory->setRequest(PIPE_3AG_REPROCESSING, true);
            } else {
                targetfactory->setRequest(PIPE_3AC_REPROCESSING, true);
            }
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
#ifdef SAMSUNG_TN_FEATURE
            m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
#endif
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
    if (frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE)
#endif
    {
        m_checkRequestStreamChanged((char *)"Capture", streamConfigBit);
    }

    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        return INVALID_OPERATION;
    }

    *m_currentCaptureShot[currentCameraId] = *service_shot_ext;
    m_updateLatestInfoToShot(m_currentCaptureShot[currentCameraId], frameType);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentCaptureShot[currentCameraId],
                        service_shot_ext->shot.ctl.stats.faceDetectMode, dsInputPortId, true, flag3aaVraM2M);
    }
    components.parameters->setDsInputPortId(
        m_currentCaptureShot[currentCameraId]->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS],
        true);

#ifdef SAMSUNG_TN_FEATURE
    yuvStallPortUsage = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE);
#endif

    m_captureFrameHandler_vendor_updateDualROI(request, components, frameType);

    if (m_currentCaptureShot[currentCameraId]->fd_bypass == false) {
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
        && (m_currentCaptureShot[currentCameraId]->shot.ctl.flash.flashMode == CAM2_FLASH_MODE_SINGLE
            || flashMgr->getNeedCaptureFlash() == true)) {
        m_configurations->setModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT, needDynamicBayerCount);
        CLOGD("needDynamicBayerCount(%d) NeedFlash(%d) flashMode(%d)", needDynamicBayerCount,
            flashMgr->getNeedCaptureFlash(),
            m_currentCaptureShot[currentCameraId]->shot.ctl.flash.flashMode);
    }

    m_frameCountLock.lock();

#ifdef USE_LLS_REPROCESSING
    if (m_parameters[m_cameraId]->getLLSCaptureOn() == true) {
        int llsCaptureCount = m_parameters[m_cameraId]->getLLSCaptureCount();
        for (int i = 1; i < llsCaptureCount; i++) {
            ExynosCameraFrameSP_sptr_t newLLSFrame = NULL;

            ret = m_generateInternalFrame(targetfactory, &m_captureProcessList,
                    &m_captureProcessLock, newLLSFrame);
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
            newLLSFrame->setFrameIndex(i);
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

            m_currentCaptureShot[currentCameraId]->shot.ctl.aa.captureIntent = (enum aa_capture_intent)captureIntent;
            m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, true);
            sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
            components.activityControl->setOISCaptureMode(true);
        }
#endif
    }

#ifdef SAMSUNG_TN_FEATURE
    int connectedPipeNum = m_connectCaptureUniPP(targetfactory);
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE) {
        int ldCaptureCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
        int scenario = PP_SCENARIO_LLS_DEBLUR;

#ifdef SAMSUNG_MFHDR_CAPTURE
        if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) == MULTI_SHOT_MODE_MF_HDR) {
            scenario = PP_SCENARIO_MF_HDR;
        } else
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) == MULTI_SHOT_MODE_LL_HDR) {
            scenario = PP_SCENARIO_LL_HDR;
        } else
#endif
        {
#ifdef SAMSUNG_HIFI_CAPTURE
            scenario = PP_SCENARIO_HIFI_LLS;
#else
            scenario = PP_SCENARIO_LLS_DEBLUR;
#endif
        }

        for (int i = 0 ; i < ldCaptureCount; i++) {
            ExynosCameraFrameSP_sptr_t newLDCaptureFrame = NULL;
            frame_type_t LDframeType = frameType;

            if (i < ldCaptureCount - 1) {
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                    if (i != 0 || frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
                        targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                        targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                        targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                        targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                    } else {
                        targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, true);
                        targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, isNeedThumbnail);
                        targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, isNeedHWFCJpeg);
                        targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, isNeedThumbnail);
                    }
                } else
#endif
                {
                    LDframeType = internalframeType;
                }

#ifdef FAST_SHUTTER_NOTIFY
                ret = m_generateInternalFrame(targetfactory, &m_captureProcessList,
                                                &m_captureProcessLock, newLDCaptureFrame, internalframeType,
                                                request);
#else
                ret = m_generateInternalFrame(targetfactory, &m_captureProcessList,
                                                &m_captureProcessLock, newLDCaptureFrame, internalframeType);
#endif
                if (ret != NO_ERROR) {
                    CLOGE("m_generateInternalFrame fail");
                    return ret;
                } else if (newLDCaptureFrame == NULL) {
                    CLOGE("new faame is NULL");
                    return INVALID_OPERATION;
                }
                newLDCaptureFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, false);
                newLDCaptureFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, false);
            } else {
#ifdef SAMSUNG_STR_CAPTURE
                if (m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE)) {
                    targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING2, true);
                }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                    targetfactory->setRequest(PIPE_MCSC3_REPROCESSING, false);
                    targetfactory->setRequest(PIPE_MCSC4_REPROCESSING, false);
                    targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
                    targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
                }
#endif

                if (yuvCbStallFlag) {
                    ret = m_generateFrame(targetfactory, &m_captureProcessList, &m_captureProcessLock,
                                            newLDCaptureFrame, request);
                } else {
                    ret = m_generateFrame(targetfactory, &m_captureProcessList, &m_captureProcessLock,
                                            newLDCaptureFrame, request, true);
                }
                if (ret != NO_ERROR) {
                    CLOGE("m_generateFrame fail");
                    return ret;
                } else if (newLDCaptureFrame == NULL) {
                    CLOGE("new frame is NULL");
                    return INVALID_OPERATION;
                }
                newLDCaptureFrame->setStreamRequested(STREAM_TYPE_YUVCB_STALL, yuvCbStallFlag);
                newLDCaptureFrame->setStreamRequested(STREAM_TYPE_THUMBNAIL_CB, thumbnailCbFlag);
#ifdef SAMSUNG_STR_CAPTURE
                if (m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE)) {
                    newLDCaptureFrame->setPPScenario(PIPE_PP_UNI_REPROCESSING2 - PIPE_PP_UNI_REPROCESSING,
                                                    PP_SCENARIO_STR_CAPTURE);
                }
#endif
                m_checkUpdateResult(newLDCaptureFrame, streamConfigBit);
            }

            CLOGD("generate request framecount(%d) requestKey(%d) m_internalFrameCount(%d) count(%d)",
                    newLDCaptureFrame->getFrameCount(), request->getKey(), m_internalFrameCount, i);

            TIME_LOGGER_UPDATE(m_cameraId, request->getKey(), 0, USER_DATA, CREATE_CAPTURE_FRAME, newLDCaptureFrame->getFrameCount());

            if (m_getState() == EXYNOS_CAMERA_STATE_FLUSH) {
                CLOGD("[R%d F%d]Flush is in progress.",
                        request->getKey(), newLDCaptureFrame->getFrameCount());
                /* Generated frame is going to be deleted at flush() */
                return ret;
            }

            ret = newLDCaptureFrame->setMetaData(m_currentCaptureShot[currentCameraId]);
            if (ret != NO_ERROR) {
                CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                        newLDCaptureFrame->getFrameCount(), ret);
            }

            newLDCaptureFrame->setFrameType(LDframeType);
            newLDCaptureFrame->setStreamRequested(STREAM_TYPE_RAW, rawStreamFlag);
            newLDCaptureFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
            newLDCaptureFrame->setStreamRequested(STREAM_TYPE_ZSL_INPUT, zslFlag);
            newLDCaptureFrame->setFrameSpecialCaptureStep(SCAPTURE_STEP_COUNT_1+i);
            newLDCaptureFrame->setFrameYuvStallPortUsage(yuvStallPortUsage);
            newLDCaptureFrame->setPPScenario(PIPE_PP_UNI_REPROCESSING - PIPE_PP_UNI_REPROCESSING, scenario);

            ret = m_setupCaptureFactoryBuffers(request, newLDCaptureFrame);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to setupCaptureStreamBuffer. ret %d",
                        request->getKey(), newLDCaptureFrame->getFrameCount(), ret);
            }

            selectBayerQ->pushProcessQ(&newLDCaptureFrame);
        }
    } else
#endif
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

                ret = newLongExposureCaptureFrame->setMetaData(m_currentCaptureShot[currentCameraId]);
                if (ret != NO_ERROR) {
                    CLOGE("Set metadata to frame fail, Frame count(%d), ret(%d)",
                            newLongExposureCaptureFrame->getFrameCount(), ret);
                }

                selectBayerQ->pushProcessQ(&newLongExposureCaptureFrame);
            }
        } else {
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

            ret = newFrame->setMetaData(m_currentCaptureShot[currentCameraId]);
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

#ifdef SAMSUNG_TN_FEATURE
            for (int i = 0; i < connectedPipeNum; i++) {
                int pipePPScenario = targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + i);

                if (pipePPScenario > 0) {
                    newFrame->setPPScenario(i, pipePPScenario);
                }
            }
#endif /* SAMSUNG_TN_FEATURE */

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

#ifdef OIS_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true) {
        retryCount = 9;
    }

    if (m_configurations->getCaptureExposureTime() > PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX) {
        selector->setWaitTimeOISCapture(400000000);
    } else {
        selector->setWaitTimeOISCapture(130000000);
    }
#endif

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
    CameraMetadata *serviceMeta = request->getServiceMeta();
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
            && ((components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) ||
            (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_VRA) == HW_CONNECTION_MODE_M2M))) {
            /* When VRA works as M2M mode, FD metadata will be updated with the latest one in Parameters */
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            if (m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE) == UNI_PLUGIN_CAMERA_TYPE_TELE
                && frameType == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
                m_parameters[m_cameraIds[1]]->getFaceDetectMeta(dst_ext);
            }
            else
#endif
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

#ifdef SAMSUNG_TN_FEATURE
        /* vendorSpecific[64] : Exposure time's denominator */
        if ((shutter_speed <= 7) ||
#ifdef USE_BV_FOR_VARIABLE_BURST_FPS
                ((Bv <= LLS_BV) && (m_cameraId == CAMERA_ID_FRONT)) ||
#endif
                (m_configurations->getLightCondition() == SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW)
           ) {
            burst_fps = BURSTSHOT_OFF_FPS;
        } else if (shutter_speed <= 14) {
            burst_fps = BURSTSHOT_MIN_FPS;
        } else {
            burst_fps = BURSTSHOT_MAX_FPS;
        }
#endif /* SAMSUNG_TN_FEATURE */

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

#ifdef SAMSUNG_TN_FEATURE
    if (m_configurations->getModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT) > 0) {
        CLOGD("get (%d)frame meta for multi frame capture",
                m_configurations->getModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT));
        m_configurations->getExifMetaData(&dst_ext->shot);
    }
#endif

    m_parameters[m_cameraId]->setExifChangedAttribute(NULL, NULL, NULL, &dst_ext->shot);
    ret = m_metadataConverter->updateDynamicMeta(request, PARTIAL_JPEG);

    CLOGV("[F%d(%d)]Set result.",
            request->getFrameCount(), dst_ext->shot.dm.request.frameCount);

    return ret;
}

status_t ExynosCamera::m_setupFrameFactoryToRequest()
{
#ifdef CAMERA_FAST_ENTRANCE_V1
    Mutex::Autolock l(m_previewFactoryLock);
#endif
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
    m_configurations->setMode(CONFIGURATION_HDR_RECORDING_MODE, false);

    for (size_t i = 0; i < streamList->num_streams; i++) {
        camera3_stream_t *newStream = streamList->streams[i];
        int option = 0;

        if (newStream == NULL) {
            CLOGE("Stream index %zu was NULL", i);
            return BAD_VALUE;
        }

#ifdef SAMSUNG_TN_FEATURE
        if (m_configurations->getSamsungCamera() == false) {
            /* Clear option field for 3rd party apps */
            newStream->option = 0;
        }
        option = newStream->option;
#endif

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
#ifdef SAMSUNG_TN_FEATURE
                m_configurations->setMode(CONFIGURATION_PRO_MODE, true);
#endif
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
#ifdef USE_ALWAYS_FD_ON
        m_configurations->setMode(CONFIGURATION_ALWAYS_FD_ON_MODE, false);
#endif
    } else {
#ifdef SAMSUNG_SSM
        if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION
#ifdef SAMSUNG_FACTORY_SSM_TEST
            || m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_SSM_TEST
#endif
        ) {
            CLOGI("super slow mode is configured. StreamCount %d. m_videoStreamExist(%d) recordingFps(%d)",
                    streamList->num_streams, m_videoStreamExist, recordingFps);
            highSpeedMode = CONFIG_MODE::SSM_240;
        } else
#endif
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
    camera_pixel_size pixelSize = CAMERA_PIXEL_SIZE_8BIT;
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

#ifdef SAMSUNG_TN_FEATURE
    if (m_configurations->getSamsungCamera() == true)
        option = stream->option;
#endif

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
                planeCount = 2;

                if (m_videoStreamExist == true) {
                    /* remove yuv full range bit. */
                    stream->usage &= ~GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL;

                    if (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                        stream->usage |= (GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA)
                                         | (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
                        planeCount = 3;
                    }
                }

#ifdef SAMSUNG_HDR10_RECORDING
                if (m_videoStreamExist == true
                    && m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                    //stream->data_space = HAL_DATASPACE_STANDARD_BT2020| HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_RANGE_LIMITED;
                    stream->data_space = HAL_DATASPACE_HDR10_REC;
                } else {
                    //stream->data_space = HAL_DATASPACE_STANDARD_BT709 | HAL_DATASPACE_RANGE_LIMITED;
                    stream->data_space = HAL_DATASPACE_SDR_REC;
                }
#endif

                /* Cached for PP Pipe */
                stream->usage |= (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);

                /* set Preview Size */
                m_configurations->setSize(CONFIGURATION_PREVIEW_SIZE, stream->width, stream->height);

                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_preview_buffers;
            } else if(stream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER format(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                    stream->format, stream->usage, stream->stream_type, option);
                id = HAL_STREAM_ID_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */

                planeCount = 2;

                if ((m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true) ||
                        (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true)) {
                    stream->usage |= (GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA)
                                     | (GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN);
                    planeCount = 3;

                    if (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                        actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M;
                        pixelSize = CAMERA_PIXEL_SIZE_10BIT;

                        /* overrideFormat format  */
                        stream->format = HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M;
                        CLOGD("CONFIGURATION_HDR_RECORDING_MODE is true pixelSize(%d)", pixelSize);
                    }
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

#ifdef SAMSUNG_HDR10_RECORDING
                if (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                    //stream->data_space = HAL_DATASPACE_STANDARD_BT2020 | HAL_DATASPACE_TRANSFER_ST2084 | HAL_DATASPACE_RANGE_LIMITED;
                    stream->data_space = HAL_DATASPACE_HDR10_REC;
                } else {
                    //stream->data_space = HAL_DATASPACE_STANDARD_BT709 | HAL_DATASPACE_RANGE_LIMITED;
                    stream->data_space = HAL_DATASPACE_SDR_REC;
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

#ifdef SAMSUNG_TN_FEATURE
                /* set Thumbnail Size */
                m_configurations->setSize(CONFIGURATION_THUMBNAIL_CB_SIZE, stream->width, stream->height);
#endif
            } else {
                CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888 format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);

                id = HAL_STREAM_ID_CALLBACK;
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_callback_buffers;

#ifdef SAMSUNG_TN_FEATURE
                if (m_cameraId == CAMERA_ID_FRONT
                    && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE)
                        == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_INTELLIGENT_SCAN) {
                    CLOGD("Use contiguous buffer for multi-biometric");
                    stream->usage |= GRALLOC1_PRODUCER_USAGE_CAMERA_RESERVED;
                }
#endif
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
#if !defined(USE_NONE_SECURED_CAMERA)
            if (m_scenario == SCENARIO_SECURE) {
                stream->usage |= GRALLOC1_PRODUCER_USAGE_SECURE_CAMERA_RESERVED;
            }
#endif
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
    newStream->setFormat(actualFormat, pixelSize);
    newStream->setPlaneCount(planeCount);
    newStream->setOutputPortId(outputPortId);
    newStream->setRequestBuffer(requestBuffer);

func_err:
    CLOGD("Out");

    return ret;

}

status_t ExynosCamera::m_destroyPreviewFrameFactory() {
    status_t ret = NO_ERROR;

    if (m_getState() <= EXYNOS_CAMERA_STATE_INITIALIZE) {
        return ret;
    }

    if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] != NULL) {
        if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->isCreated() == true) {
            ret = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->destroy();
            if (ret < 0)
                CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_CAPTURE_PREVIEW);
        }
        SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]);

        CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_CAPTURE_PREVIEW);
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL] != NULL) {
        if (m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]->isCreated() == true) {
            ret = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]->destroy();
            if (ret < 0)
                CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_PREVIEW_DUAL);
        }
        SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]);

        CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_PREVIEW_DUAL);
    }
#endif

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
    camera_pixel_size pixelSize = CAMERA_PIXEL_SIZE_8BIT;
    int outputPortId = 0;
    bool isUseDynamicBayer = true;
    bool needMmap = false;
    bool existVideoMeta = false;
    bool videoStreamExistPrev = false;

    m_flushLockWait = false;

    CLOGD("In");
    m_flagFirstPreviewTimerOn = false;

#ifdef FPS_CHECK
    for (int i = 0; i < sizeof(m_debugFpsCount); i++)
        m_debugFpsCount[i] = 0;
#endif

#if !defined(USE_NONE_SECURED_CAMERA)
    if (m_cameraId == CAMERA_ID_SECURE) {
        int ionFd = 0;
        ionFd = exynos_ion_alloc(m_ionClient,
                           SECURE_CAMERA_WIDTH * SECURE_CAMERA_HEIGHT * RESERVED_NUM_SECURE_BUFFERS,
                           EXYNOS_CAMERA_BUFFER_ION_MASK_SECURE,
                           EXYNOS_CAMERA_BUFFER_ION_FLAG_SECURE);

        if (ionFd < 0) {
            CLOGD("[IRIS_LEAK] Check Secure Memory Leak Failed!");
            return NO_MEMORY;
        } else {
            CLOGD("[IRIS_LEAK] Check Secure Memory Leak Successful!");
            exynos_ion_close(ionFd);
        }
    }
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
        m_prevStreamBit = 0;

#ifdef CAMERA_FAST_ENTRANCE_V1
        if (m_fastEntrance == true) {
            /* The frame factory should not stopped for fast entrance operation when first configure stream */
            ret = m_waitFastenAeThreadEnd();
            if (ret != NO_ERROR) {
                CLOGE("fastenAeThread exit with error");
                return ret;
            }
        }
#endif

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

    videoStreamExistPrev = m_videoStreamExist;
    ret = m_setStreamInfo(stream_list);
    if (ret) {
        CLOGE("setStreams() failed!!");
        return ret;
    }

    /* The setting is effective if USE_BDS_OFF is enabled */
    if (m_parameters[m_cameraId]->isUse3aaBDSOff()) {
        m_parameters[m_cameraId]->setVideoStreamExistStatus(m_videoStreamExist);
        CLOGV("videoStreamExist[%d %d]", videoStreamExistPrev, m_videoStreamExist);
        if (videoStreamExistPrev != m_videoStreamExist) {
            this->m_destroyPreviewFrameFactory();
        }
    }
    videoStreamExistPrev = m_videoStreamExist;

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
#ifdef SAMSUNG_VIDEO_BEAUTY
    if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true
#ifdef SAMSUNG_SW_VDIS
        && m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == false
#endif
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

#ifdef SAMSUNG_HIFI_VIDEO
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        if (m_videoStreamExist == true) {
            bool isValidSize = false;
            int checkW = 0, checkH = 0;
            int checkFps = 0;
            bool flagVideoStreamPriority = false;
            int previewW = 0, previewH = 0;
            int recordingW = 0, recordingH = 0;
            m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&recordingW, (uint32_t *)&recordingH);
            m_configurations->getSize(CONFIGURATION_PREVIEW_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH);
#ifdef SAMSUNG_SW_VDIS
            if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
                checkW = recordingW;
                checkH = recordingH;
            } else
#endif
            {
                if (recordingW > previewW || recordingH > previewH) {
                    checkW = recordingW;
                    checkH = recordingH;
                    flagVideoStreamPriority = true;
                } else {
                    checkW = previewW;
                    checkH = previewH;
                }
            }
            checkFps = m_configurations->getModeValue(CONFIGURATION_RECORDING_FPS);
            isValidSize = m_parameters[m_cameraId]->isValidHiFiVideoSize(checkW, checkH, checkFps);
            CLOGD("[HIFIVIDEO] isValidSize %d (%dx%d, %d)", isValidSize, checkW, checkH, checkFps);
            if (isValidSize == true) {
                if (flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
                    && m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == false
#endif
                    ) {
                    m_flagVideoStreamPriority = true;
                }
            } else {
                m_configurations->setMode(CONFIGURATION_HIFIVIDEO_MODE, false);
            }
        } else {
            m_configurations->setMode(CONFIGURATION_HIFIVIDEO_MODE, false);
        }
    }
#endif

    /* get SessionKeys Meta */
    m_metadataConverter->setSessionParams(stream_list->session_parameters);

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
        privStreamInfo->getFormat(&streamPixelFormat, &pixelSize);

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

                if (m_parameters[m_cameraId]->isUse3aaDNG() == true) {
                    bufTag.pipeId[0] = PIPE_3AG_REPROCESSING;
                    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
                } else {
                    bufTag.pipeId[0] = PIPE_3AC_REPROCESSING;
                }
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

                if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
                    ret = ExynosCameraTuningInterface::setRequest(outputPortId + PIPE_MCSC0, true);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to OBTE setRequest for PREVIEW stream. outputPortId %d",
                                outputPortId);
                        return ret;
                    }
                }

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                            hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

#ifdef SAMSUNG_SW_VDIS
                if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
                    int width = 0;
                    int height = 0;

                    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&width, (uint32_t *)&height);
                    if (width == 3840 && height == 2160) {
                        maxBufferCount = NUM_VDIS_UHD_INTERNAL_BUFFERS;
                    } else {
                        maxBufferCount = NUM_VDIS_INTERNAL_BUFFERS;
                    }
                } else
#endif
#ifdef SAMSUNG_HYPERLAPSE
                if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
                    maxBufferCount = NUM_HYPERLAPSE_INTERNAL_BUFFERS;
                } else
#endif
#if defined(SAMSUNG_HIFI_VIDEO) && !defined(HIFIVIDEO_INPUTCOPY_DISABLE)
                if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                    maxBufferCount = NUM_HIFIVIDEO_INTERNAL_BUFFERS;
                } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
                if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
                    maxBufferCount = NUM_VIDEO_BEAUTY_INTERNAL_BUFFERS;
                } else
#endif
                {
                    maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
                    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                        maxBufferCount = 0;
                    }
#endif
                }

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
                if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                    if (maxBufferCount) {
                        maxBufferCount += NUM_HIFIVIDEO_EXTRA_INTERNAL_BUFFERS;
                    } else {
                        maxBufferCount = NUM_HIFIVIDEO_INTERNAL_BUFFERS;
                    }
                    maxBufferCount = MIN(maxBufferCount, VIDEO_MAX_FRAME);
                }
#endif

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

                if (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                    existVideoMeta = true;
                }

#ifdef SAMSUNG_SW_VDIS
                if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
                    bufTag.pipeId[0] = PIPE_GSC;
                } else
#endif
#ifdef SAMSUNG_HYPERLAPSE
                if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
                    bufTag.pipeId[0] = PIPE_GSC;
                } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
                if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
                    if (m_flagVideoStreamPriority == true) {
                        bufTag.pipeId[0] = PIPE_GSC;
                    } else {
                        bufTag.pipeId[0] = PIPE_PP_UNI;
                        bufTag.pipeId[1] = PIPE_PP_UNI2;
                        bufTag.pipeId[2] = PIPE_PP_UNI3;
                    }
                } else
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                    if (m_flagVideoStreamPriority == true) {
                        bufTag.pipeId[0] = PIPE_GSC;
                    } else {
                        bufTag.pipeId[0] = PIPE_PP_UNI;
                        bufTag.pipeId[1] = PIPE_PP_UNI2;
                        bufTag.pipeId[2] = PIPE_PP_UNI3;
                    }
                } else
#endif
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

                /* disable Cached */
                if (bufTag.pipeId[0] == PIPE_GSC) {
                    newStream->usage &= ~GRALLOC_USAGE_SW_READ_OFTEN;
                }

                CLOGD("Create buffer manager(PREVIEW)");
                ret = m_bufferSupplier->createBufferManager("PREVIEW_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create PREVIEW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                getYuvPlaneSize(HAL_PIXEL_FORMAT_2_V4L2_PIX(streamPixelFormat), bufConfig.size, width, height, pixelSize);
                if (existVideoMeta == true) {
                    bufConfig.size[streamPlaneCount - 1] = EXYNOS_CAMERA_VIDEO_META_PLANE_SIZE;
                }
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.needMmap = needMmap;
                bufConfig.reservedMemoryCount = 0;

                CLOGD("planeCount = %d+1, planeSize[0] = %d, planeSize[1] = %d, planeSize[2] = %d,\
                        bytesPerLine[0] = %d, outputPortId = %d",
                        streamPlaneCount,
                        bufConfig.size[0], bufConfig.size[1], bufConfig.size[2],
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

#ifdef USES_SW_VDIS
                if (m_configurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE) == true) {
                    if (width == 1920 && height == 1080) {
                        hwWidth = VIDEO_MARGIN_FHD_W;
                        hwHeight = VIDEO_MARGIN_FHD_H;
                    } else if (width == 1280 && height == 720) {
                        hwWidth = VIDEO_MARGIN_HD_W;
                        hwHeight = VIDEO_MARGIN_HD_H;
                    }
                    CLOGD("SW VDIS video size %d x %d", hwWidth, hwHeight);
                } else
#endif
                {
                    hwWidth = width;
                    hwHeight = height;
                }

                ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setHwYuvSize for VIDEO stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, outputPortId);
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

                if ((m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true)
                        || (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true)) {
                    existVideoMeta = true;
                }

                if (m_configurations->isSupportedFunction(SUPPORTED_FUNCTION_GDC) == true) {
                    bufTag.pipeId[0] = PIPE_GDC;
                } else {
#ifdef SAMSUNG_SW_VDIS
                    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
                        bufTag.pipeId[0] = PIPE_PP_UNI;
                        bufTag.pipeId[1] = PIPE_PP_UNI2;
                        bufTag.pipeId[2] = PIPE_PP_UNI3;
                    } else
#endif
#ifdef SAMSUNG_HYPERLAPSE
                    if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
                        bufTag.pipeId[0] = PIPE_PP_UNI;
                        bufTag.pipeId[1] = PIPE_PP_UNI2;
                        bufTag.pipeId[2] = PIPE_PP_UNI3;
                    } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
                    if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
                        if (m_flagVideoStreamPriority == true) {
                            bufTag.pipeId[0] = PIPE_PP_UNI;
                            bufTag.pipeId[1] = PIPE_PP_UNI2;
                            bufTag.pipeId[2] = PIPE_PP_UNI3;
                        } else {
                            bufTag.pipeId[0] = PIPE_GSC;
                        }
                    } else
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                        if (m_flagVideoStreamPriority == true) {
                            bufTag.pipeId[0] = PIPE_PP_UNI;
                            bufTag.pipeId[1] = PIPE_PP_UNI2;
                            bufTag.pipeId[2] = PIPE_PP_UNI3;
                        } else {
                            bufTag.pipeId[0] = PIPE_GSC;
                        }
                    } else
#endif
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
                        {
                            bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                               + PIPE_MCSC0;
                        }
                    }
                }
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                /* disable Cached */
                if (bufTag.pipeId[0] == PIPE_GSC) {
                    newStream->usage &= ~GRALLOC_USAGE_SW_READ_OFTEN;
                }

                CLOGD("Create buffer manager(VIDEO)");
                ret =  m_bufferSupplier->createBufferManager("VIDEO_STREAM_BUF", m_ionAllocator, bufTag, newStream, streamPixelFormat);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                getYuvPlaneSize(HAL_PIXEL_FORMAT_2_V4L2_PIX(streamPixelFormat), bufConfig.size, width, height, pixelSize);

                if (existVideoMeta == true) {
                    bufConfig.size[streamPlaneCount - 1] = EXYNOS_CAMERA_VIDEO_META_PLANE_SIZE;
                 }

                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;

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

                ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, outputPortId);
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

                if (m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true) {
                    existVideoMeta = true;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                getYuvPlaneSize(HAL_PIXEL_FORMAT_2_V4L2_PIX(streamPixelFormat), bufConfig.size, width, height, pixelSize);

                if (existVideoMeta == true) {
                    bufConfig.size[streamPlaneCount - 1] = EXYNOS_CAMERA_VIDEO_META_PLANE_SIZE;
                }

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
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
                if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT
                    || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                    bufConfig.size[0] = width * height * 3 * 1.5;
                } else
#endif
                {
                    bufConfig.size[0] = width * height * 2;
                }

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
                if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT
                    || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                    if (m_configurations->getMode(CONFIGURATION_FACTORY_TEST_MODE) == true) {
                        bufConfig.reqBufCount = 3;
                        bufConfig.allowedMaxBufCount = 3;
                    } else {
                        bufConfig.reqBufCount = 1;
                        bufConfig.allowedMaxBufCount = 1;
                    }
                } else
#endif
                {
                    bufConfig.reqBufCount = maxBufferCount;
                    bufConfig.allowedMaxBufCount = maxBufferCount;
                }
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[1]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.reservedMemoryCount = 0;
#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
                if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                    bufConfig.needMmap = true;
                }
#endif

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

                ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, outputPortId);
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

                    ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, stallOutputPortId);
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

                ret = m_configurations->checkYuvFormat(streamPixelFormat, pixelSize, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK_STALL stream. format %x outputPortId %d",
                            streamPixelFormat, outputPortId);
                    return ret;
                }

#ifdef SAMSUNG_LLS_DEBLUR
                /* set startIndex as the next internal buffer index */
                startIndex = m_exynosconfig->current->bufInfo.num_nv21_picture_buffers;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers - startIndex;
#else
                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;
#endif
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
#ifdef SAMSUNG_TN_FEATURE
                bufTag.pipeId[1] = PIPE_PP_UNI_REPROCESSING;
#endif
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

#ifdef SAMSUNG_SW_VDIS
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true
        && m_videoStreamExist == true) {
		configureStreams_vendor_updateVDIS();
    } else
#endif
    {

#ifdef SAMSUNG_VIDEO_BEAUTY
        if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true
            && m_flagVideoStreamPriority == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            int videoW = 0, videoH = 0;

            m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);

            ret = m_parameters[m_cameraId]->checkHwYuvSize(videoW, videoH, portId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setHwYuvSize for PREVIEW stream(Beauty). size %dx%d outputPortId %d",
                         videoW, videoH, portId);
                return ret;
            }
        }
#endif

#ifdef SAMSUNG_HIFI_VIDEO
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true
            && m_flagVideoStreamPriority == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            int videoW = 0, videoH = 0;

            m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);

            ret = m_parameters[m_cameraId]->checkHwYuvSize(videoW, videoH, portId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to checkHwYuvSize for PREVIEW stream(HiFiVideo). size %dx%d outputPortId %d",
                        videoW, videoH, portId);
                return ret;
            }
        }
#endif

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

#ifdef SAMSUNG_HYPERLAPSE
    if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true
        && m_videoStreamExist == true) {
        int portId = m_parameters[m_cameraId]->getPreviewPortId();
        int fps = m_configurations->getModeValue(CONFIGURATION_RECORDING_FPS);
        int videoW = 0, videoH = 0;
        int hwWidth = 0, hwHeight = 0;

        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);
        m_parameters[m_cameraId]->getHyperlapseYuvSize(videoW, videoH, fps, &hwWidth, &hwHeight);

        if (hwWidth == 0 || hwHeight == 0) {
            CLOGE("Not supported HYPERLAPSE size %dx%d fps %d", videoW, videoH, fps);
            return BAD_VALUE;
        }

        ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, portId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setHwYuvSize for PREVIEW stream(Hyperlapse). size %dx%d outputPortId %d",
                     hwWidth, hwHeight, portId);
            return ret;
        }
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT,
                                   m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL)
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
#ifdef SAMSUNG_TN_FEATURE
        yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + ExynosCameraParameters::YUV_MAX;
#endif

        m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, yuvStallPort);

        ret = m_parameters[m_cameraId]->checkPictureSize(width, height);
        if (ret != NO_ERROR) {
            CLOGE("Failed to checkPictureSize for JPEG stream. size %dx%d",
                     width, height);
            return ret;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    width = YUVSTALL_DSCALED_SIZE_16_9_W;
    height = YUVSTALL_DSCALED_SIZE_16_9_H;
    if (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == true
        || m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true) {
        m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&width, (uint32_t *)&height);
    } else if (m_streamManager->findStream(HAL_STREAM_ID_PREVIEW)) {
        m_configurations->getSize(CONFIGURATION_PREVIEW_SIZE, (uint32_t *)&width, (uint32_t *)&height);
    }
    m_configurations->setSize(CONFIGURATION_DS_YUV_STALL_SIZE, width, height);
#endif

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
#ifdef USE_DUAL_CAMERA
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
        ) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        int previewW = 0, previewH = 0;
        int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);

        m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, previewOutputPortId);

        /* Main */
        ret = m_parameters[m_cameraId]->adjustDualSolutionSize(previewW, previewH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDualSolutionRefSize. size %dx%d outputPortId %d",
                previewW, previewH, previewOutputPortId);
            return ret;
        }

        m_parameters[m_cameraIds[1]]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, previewOutputPortId);

        /* Sub */
        ret = m_parameters[m_cameraIds[1]]->adjustDualSolutionSize(previewW, previewH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDualSolutionRefSize. size %dx%d outputPortId %d",
                previewW, previewH, previewOutputPortId);
            return ret;
        }
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
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

#ifdef SAMSUNG_SW_VDIS
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true
        && m_configurations->getDynamicMode(DYNAMIC_UHD_RECORDING_MODE) == true) {
        m_setBuffersThreadFunc();
    } else
#endif
    {
        m_setBuffersThread->run(PRIORITY_DEFAULT);
    }

    /* set Use Dynamic Bayer Flag
     * If the samsungCamera is enabled and the jpeg stream is only existed,
     * then the bayer mode is set always on.
     * Or, the bayer mode is set dynamic.
     */
    if (m_configurations->getSamsungCamera() == true
        && (m_streamManager->findStream(HAL_STREAM_ID_VIDEO) == false)
        && ((m_streamManager->findStream(HAL_STREAM_ID_JPEG) == true)
             || (m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true))) {
        isUseDynamicBayer = false;
    } else {
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

    m_configurations->setMode(CONFIGURATION_DYNAMIC_BAYER_MODE, isUseDynamicBayer);

    ret = m_transitState(EXYNOS_CAMERA_STATE_CONFIGURED);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into CONFIGURED. ret %d", ret);
    }

    CLOGD("Out");
    return ret;
}

status_t ExynosCamera::m_updateTimestamp(ExynosCameraRequestSP_sprt_t request,
                                            ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *timestampBuffer,
                                            frame_handle_components_t *components)
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
        uint64_t correctTime = 0;
        exposureTime = (uint64_t)shot_ext->shot.dm.sensor.exposureTime;

        oldTimeStamp = shot_ext->shot.udm.sensor.timeStampBoot;

        shot_ext->shot.udm.sensor.timeStampBoot -= (exposureTime);
        if (components != NULL) {
            correctTime = components->parameters->getCorrectTimeForSensorFusion();
            shot_ext->shot.udm.sensor.timeStampBoot += correctTime;
        }

        CLOGV("[F%d] exp.time(%ju) correctTime(%ju) timeStamp(%ju -> %ju)",
            frame->getFrameCount(), exposureTime, correctTime,
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
    int currentCameraId = 0;
    uint32_t requestKey = 0;
    uint32_t needDynamicBayerCount = m_configurations->getModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT);
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
#ifdef SAMSUNG_TN_FEATURE
    int connectedPipeNum = 0;
#endif
    requestKey = request->getKey();
    ExynosRect zoomRect = {0, };
    ExynosRect activeZoomRect = {0, };

    getFrameHandleComponents(frameType, &components);
    currentCameraId = components.currentCameraId;

    requestVC0 = (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    request3AP = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    requestGMV = m_configurations->getMode(CONFIGURATION_GMV_MODE);
    requestMCSC = (components.parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
    flag3aaVraM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_VRA) == HW_CONNECTION_MODE_M2M);
    reprocessingBayerMode = components.parameters->getReprocessingBayerMode();
    controlPipeId = (enum pipeline) components.parameters->getPerFrameControlPipe();

#ifdef SAMSUNG_SSM
    if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION
        && m_configurations->getModeValue(CONFIGURATION_SSM_STATE) == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_READY
        ) {
        m_setupSSMMode(request, components);
    }
#endif

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
#ifdef SAMSUNG_TN_FEATURE
    targetfactory->setRequest(PIPE_PP_UNI, false);
    targetfactory->setRequest(PIPE_PP_UNI2, false);
    targetfactory->setRequest(PIPE_PP_UNI3, false);
#endif
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
       // enum DUAL_OPERATION_MODE dualOperationMode = m_configurations->getDualOperationMode();

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

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        for(int i = 0; i < MAX_NUM_PIPES; i++) {
            if(ExynosCameraTuningInterface::getRequest(i) == true) {
                targetfactory->setRequest(i, true);
                break;
            }
        }
    }

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
#ifdef SAMSUNG_SW_VDIS
            if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
                break;
            } else
#endif
#ifdef SAMSUNG_HYPERLAPSE
            if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
                break;
            } else
#endif
#ifdef SAMSUNG_HIFI_VIDEO
            if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                break;
            } else
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
            if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
                break;
            } else
#endif
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

#ifdef USE_DUAL_CAMERA
    if (frameType != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
    {
        m_checkRequestStreamChanged((char *)"Preview", streamConfigBit);
    }

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        goto GENERATE_FRAME;
    }
#endif

    if (needDynamicBayerCount > 0) {
        CLOGD("request %d needDynamicBayerCount %d",
                request->getKey(), needDynamicBayerCount);

        m_configurations->setModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT, needDynamicBayerCount - 1);
        needDynamicBayer = true;

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

#ifdef SAMSUNG_TN_FEATURE
    /* Frames can be pushed and processed in the preview framefactory only */
    /* And all uniPP scenarios should be managed by the preview framefactory only */
    connectedPipeNum = m_connectPreviewUniPP(request, targetfactory);
#endif

#ifdef USE_DUAL_CAMERA
GENERATE_FRAME:
#endif
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        ret = INVALID_OPERATION;
        return ret;
    }

    *m_currentPreviewShot[currentCameraId] = *service_shot_ext;

#ifdef SAMSUNG_TN_FEATURE
    m_setTransientActionInfo(&components);

    if (m_configurations->getMode(CONFIGURATION_FACTORY_TEST_MODE) == true) {
        m_setApertureControl(&components, m_currentPreviewShot[currentCameraId]);
    }
#endif

    if ((m_recordingEnabled == true) && (ysumPortId < 0) && (request->hasStream(HAL_STREAM_ID_PREVIEW))) {
        /* The buffer from the preview port is used as the recording buffer */
        components.parameters->setYsumPordId(components.parameters->getPreviewPortId(),
                                                m_currentPreviewShot[currentCameraId]);
    } else {
        components.parameters->setYsumPordId(ysumPortId, m_currentPreviewShot[currentCameraId]);
    }


    m_updateShotInfoLock.lock();
    m_updateLatestInfoToShot(m_currentPreviewShot[currentCameraId], frameType);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentPreviewShot[currentCameraId],
                    service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), false, flag3aaVraM2M);
    }
    components.parameters->setDsInputPortId(
        m_currentPreviewShot[currentCameraId]->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS],
        false);

    if (captureFlag == true || rawStreamFlag == true) {
        m_updateExposureTime(m_currentPreviewShot[currentCameraId]);
    }

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        ExynosCameraTuningInterface::updateShot(m_currentPreviewShot[currentCameraId]);
    }

    if (m_currentPreviewShot[currentCameraId]->fd_bypass == false) {
        if (flag3aaVraM2M) {
            targetfactory->setRequest(PIPE_3AF, true);
            targetfactory->setRequest(PIPE_VRA, true);
            /* TODO: need to check HFD */
            } else if (components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA)
                                            == HW_CONNECTION_MODE_M2M) {
                targetfactory->setRequest(PIPE_MCSC5, true);
                targetfactory->setRequest(PIPE_VRA, true);
#ifdef SUPPORT_HFD
        if (m_currentPreviewShot[currentCameraId]->hfd.hfd_enable == true) {
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
    m_updateShotInfoLock.unlock();

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
    components.parameters->updatePreviewStatRoi(m_currentPreviewShot[currentCameraId], &bayerCropRegion);

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentPreviewShot[currentCameraId]);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Set metadata to frame fail. ret %d",
                 request->getKey(), newFrame->getFrameCount(), ret);
    }

    newFrame->setFrameType(frameType);
    newFrame->setNeedDynamicBayer(needDynamicBayer);

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
#ifdef SAMSUNG_TN_FEATURE
        for (int i = 0; i < connectedPipeNum; i++) {
            int pipePPScenario = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->getPPScenario(PIPE_PP_UNI + i);

            if (pipePPScenario > 0) {
                newFrame->setPPScenario(i, pipePPScenario);
            }
        }
#endif /* SAMSUNG_TN_FEATURE */

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
#ifdef SAMSUNG_SENSOR_LISTENER
    m_getSensorListenerData(&components);
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
    int currentCameraId = 0;
    uint32_t requestKey = 0;
    int pipeId = m_getBayerPipeId();
    uint32_t streamConfigBit = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    frame_handle_components_t components;
    getFrameHandleComponents(frameType, &components);
    currentCameraId = components.currentCameraId;

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
#ifdef SAMSUNG_TN_FEATURE
    targetfactory->setRequest(PIPE_PP_UNI, false);
    targetfactory->setRequest(PIPE_PP_UNI2, false);
    targetfactory->setRequest(PIPE_PP_UNI3, false);
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

    m_checkRequestStreamChanged((char *)"Vision", streamConfigBit);

    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        ret = INVALID_OPERATION;
        return ret;
    }

    *m_currentVisionShot[currentCameraId] = *service_shot_ext;
    m_updateLatestInfoToShot(m_currentVisionShot[currentCameraId], frameType);
    if (service_shot_ext != NULL) {
        m_updateFD(m_currentVisionShot[currentCameraId],
                        service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(),
                        false);
    }
    components.parameters->setDsInputPortId(
        m_currentVisionShot[currentCameraId]->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS],
        false);
    m_updateExposureTime(m_currentVisionShot[currentCameraId]);

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

    if (m_scenario == SCENARIO_SECURE) {
        shutterSpeed = (int32_t) (m_configurations->getExposureTime() / 100000);
        if (m_shutterSpeed != shutterSpeed) {
            ret = targetfactory->setControl(V4L2_CID_SENSOR_SET_SHUTTER, shutterSpeed, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("shutterSpeed is changed (%d -> %d)", m_shutterSpeed, shutterSpeed);
            m_shutterSpeed = shutterSpeed;
        }

        gain = m_configurations->getGain();
        if (m_gain != gain) {
            ret = targetfactory->setControl(V4L2_CID_SENSOR_SET_GAIN, gain, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("gain is changed (%d -> %d)", m_gain, gain);
            m_gain = gain;
        }

        irLedWidth = (int32_t) (m_configurations->getLedPulseWidth() / 100000);
        if (m_irLedWidth != irLedWidth) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_WIDTH, irLedWidth, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedWidth is changed (%d -> %d)", m_irLedWidth, irLedWidth);
            m_irLedWidth = irLedWidth;
        }

        irLedDelay = (int32_t) (m_configurations->getLedPulseDelay() / 100000);
        if (m_irLedDelay != irLedDelay) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_DELAY, irLedDelay, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedDelay is changed (%d -> %d)", m_irLedDelay, irLedDelay);
            m_irLedDelay = irLedDelay;
        }

        irLedCurrent = m_configurations->getLedCurrent();
        if (m_irLedCurrent != irLedCurrent) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_CURRENT, irLedCurrent, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedCurrent is changed (%d -> %d)", m_irLedCurrent, irLedCurrent);
            m_irLedCurrent = irLedCurrent;
        }

        irLedOnTime = (int32_t) (m_configurations->getLedMaxTime() / 1000);
        if (m_irLedOnTime != irLedOnTime) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_ONTIME, irLedOnTime, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedOnTime is changed (%d -> %d)", m_irLedOnTime, irLedOnTime);
            m_irLedOnTime = irLedOnTime;
        }
    }

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentVisionShot[currentCameraId]);
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
#ifdef LLS_CAPTURE
        components.parameters->setLLSValue(shot_ext);
#endif

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW)
        /* for UHD Recording */
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
            m_updateBeforeForceSwitchSolution(frame, pipeId);
        }
#endif

        if (frame->getHasRequest() == true) {
            ret = m_updateTimestamp(request, frame, &buffer, &components);
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
#ifdef LLS_CAPTURE
    struct camera2_shot_ext *shot_ext = NULL;
#endif
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
#ifdef SAMSUNG_TN_FEATURE
    int pp_scenario = 0;
    int pp_port = MAX_PIPE_NUM;
#endif

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

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        ExynosCameraTuningInterface::updateImage(frame, pipeId, factory);
    }

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
#ifdef LLS_CAPTURE
        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        components.parameters->setLLSValue(shot_ext);
#endif

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW)
        /* for UHD Recording */
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
            m_updateBeforeForceSwitchSolution(frame, pipeId);
        }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
                && m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE) == UNI_PLUGIN_CAMERA_TYPE_TELE) {
                struct camera2_shot_ext src_shot_ext;

                frame->getMetaData(&src_shot_ext);
                /* update slave's aeState to Master */
                if (m_parameters[m_cameraIds[1]]->getAeState() >= AE_STATE_INACTIVE) {
                    src_shot_ext.shot.dm.aa.aeState = (enum ae_state)(m_parameters[m_cameraIds[1]]->getAeState());
                }
                /* update slave's sceneDetectionInfo to Master */
                if (m_parameters[m_cameraIds[1]]->getSceneDetectIndex() >= SCENE_INDEX_INVALID) {
                    src_shot_ext.shot.udm.scene_index = (enum camera2_scene_index)m_parameters[m_cameraIds[1]]->getSceneDetectIndex();
                }
                frame->setMetaData(&src_shot_ext);
            } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE
                || frame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE) {
                /* AeState */
                components.parameters->storeAeState((int)(shot_ext->shot.dm.aa.aeState));
                /* SceneDetectionInfo - scene_index */
                components.parameters->storeSceneDetectIndex((int)(shot_ext->shot.udm.scene_index));
            }
        }
#endif

        ret = m_updateTimestamp(request, frame, &buffer, &components);
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

        if (frame->getRequest(PIPE_ISPC) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_ISPC));
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
            } else if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            } else {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for ISP. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
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
                    if (frame->getRequest(PIPE_FUSION) == true) {
                        ret = m_setupEntity(PIPE_FUSION, frame, &buffer, NULL);
                        if (ret != NO_ERROR) {
                            CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                    PIPE_FUSION, ret);
                        }
                    } else {
                        ret = frame->getSrcBufferState(PIPE_FUSION, &bufferState);
                        if (ret < 0) {
                            CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", PIPE_FUSION, ret);
                            return ret;
                        }

                        if (bufferState == ENTITY_BUFFER_STATE_REQUESTED) {
                            ret = m_setSrcBuffer(PIPE_FUSION, frame, &buffer);
                            if (ret < 0) {
                                CLOGE("m_setSrcBuffer fail, pipeId(%d), ret(%d)", PIPE_FUSION, ret);
                                return ret;
                            }
                        }

                        frame->setFrameState(FRAME_STATE_SKIPPED);
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

#ifdef SAMSUNG_TN_FEATURE
        pp_scenario = frame->getPPScenario(PIPE_PP_UNI);

        if (request != NULL) {
            pp_port = m_getPortPreviewUniPP(request, pp_scenario);
        }

        if (frame->getRequest(PIPE_PP_UNI) == true && frame->getRequest(pp_port) == false) {
            frame->setFrameState(FRAME_STATE_SKIPPED);
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

                if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
                    CLOGD("start tuning component");
                    ExynosCameraTuningInterface::start(
                            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW],
                            m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]);
                    ExynosCameraTuningInterface::setConfigurations(m_configurations);
                    ExynosCameraTuningInterface::setParameters(m_parameters[m_cameraId]);
                }
            }

#ifdef SAMSUNG_SW_VDIS_USE_OIS
            {
                int pp_scenario2 = frame->getPPScenario(PIPE_PP_UNI2);
                int pp_scenario3 = frame->getPPScenario(PIPE_PP_UNI3);
                if (pp_scenario != PP_SCENARIO_SW_VDIS
                    && pp_scenario2 != PP_SCENARIO_SW_VDIS
                    && pp_scenario3 != PP_SCENARIO_SW_VDIS
                    && buffer.index >= 0) {
                    int32_t exposureTime;
                    shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
                    exposureTime = (int32_t)shot_ext->shot.dm.sensor.exposureTime;
                    components.parameters->setSWVdisPreviewFrameExposureTime(exposureTime);
                }
            }
#endif

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
                        ret = m_updateYsumValue(frame, request, pipeId, factory->getNodeType(capturePipeId));
                        if (ret != NO_ERROR) {
                            CLOGE("failed to m_updateYsumValue, ret(%d)", ret);
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

/* For UHD Recording */
#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
                        if (dualPreviewMode == DUAL_PREVIEW_MODE_SW
                            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
                            m_updateAfterForceSwitchSolution(frame);
                        }
#endif

#ifdef DEBUG_IQ_OSD
                        if (bufferState != ENTITY_BUFFER_STATE_ERROR && (streamId == HAL_STREAM_ID_PREVIEW
                            || (getCameraId() == CAMERA_ID_FRONT && streamId == HAL_STREAM_ID_CALLBACK))) {
                            char *y = buffer.addr[0], *uv;
                            int yuvSizeW = 0, yuvSizeH = 0;

                            int portId = m_streamManager->getOutputPortId(streamId);
                            m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&yuvSizeW, (uint32_t *)&yuvSizeH, portId);
                            if (streamId == HAL_STREAM_ID_CALLBACK) {
                                uv = buffer.addr[0] + (yuvSizeW * yuvSizeH);
                            } else {
                                uv = buffer.addr[1];
                            }

                            shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
                            printOSD(y, uv, yuvSizeW, yuvSizeH, shot_ext);
                        }
#endif
                        if (request != NULL) {
                            request->setStreamBufferStatus(streamId, streamBufferState);
                        }

#ifdef SAMSUNG_TN_FEATURE
                        if ((frame->getRequest(PIPE_PP_UNI) == false) || (pp_port != capturePipeId))
#endif
                        {
                            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                                streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                                CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                        buffer.index, frame->getFrameCount(), pipeId);
                            }

                            if (request != NULL) {
                                request->setStreamBufferStatus(streamId, streamBufferState);

#ifdef SAMSUNG_SSM
                                if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) == \
                                        SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION) {
                                    ret = m_SSMProcessing(request, frame, pipeId, &buffer, streamId);
                                    if (ret != NO_ERROR) {
                                        CLOGE("Failed to resultCallback."
                                                " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                                capturePipeId, buffer.index, frame->getFrameCount(), streamId, ret);
                                        return ret;
                                    }
                                } else
#endif
                                {
                                    ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                                frame->getStreamTimestamp(), frame->getParameters());
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

#ifdef SAMSUNG_TN_FEATURE
            if (frame->getRequest(PIPE_PP_UNI) == true
#ifdef USE_DUAL_CAMERA
                && (!(m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false))
#endif
            ) {
                int pipeId_next = PIPE_PP_UNI;

                if (frame->getRequest(pp_port) == true) {
                    ExynosCameraFrameFactory *ppFactory = factory;
                    ret = m_setupPreviewUniPP(frame, request, pipeId, pp_port, pipeId_next);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupPreviewUniPP(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                pipeId, frame->getFrameCount(), ret);
                        return ret;
                    }

                    if (m_previewStreamPPPipeThread->isRunning() == false) {
                        m_previewStreamPPPipeThread->run();
                    }

#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == true) {
                        ppFactory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                    }
#endif

                    ppFactory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId_next], pipeId_next);
                    ppFactory->pushFrameToPipe(frame, pipeId_next);
                    if (ppFactory->checkPipeThreadRunning(pipeId_next) == false) {
                        ppFactory->startThread(pipeId_next);
                    }
                }
            }
#endif

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

#ifdef SAMSUNG_TN_FEATURE
    case PIPE_PP_UNI:
        if (pipeId == PIPE_PP_UNI) {
            int dstPipeId = PIPE_PP_UNI;
            int pp_port = MAX_PIPE_NUM;
            int pp_scenario = frame->getPPScenario(PIPE_PP_UNI);

            pp_port = m_getPortPreviewUniPP(request, pp_scenario);

            ret = frame->getDstBuffer(dstPipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                        pp_port, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = frame->getDstBufferState(dstPipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", dstPipeId, ret);
                return ret;
            }

#if defined(SAMSUNG_STR_PREVIEW) || defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY)
            /* pp_scenario == PP_SCENARIO_HIFI_VIDEO : Sync is done in solution library */
            if (pp_scenario == PP_SCENARIO_STR_PREVIEW
                || pp_scenario == PP_SCENARIO_SW_VDIS
                || pp_scenario == PP_SCENARIO_HYPERLAPSE
                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                ExynosRect rect;
                int planeCount = 0;

                frame->getDstRect(dstPipeId, &rect);
                planeCount = getYuvPlaneCount(rect.colorFormat);

                for (int plane = 0; plane < planeCount; plane++) {
                    if (m_ionClient >= 0)
                        exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
                }
            }
#endif

            if (frame->getRequest(PIPE_PP_UNI2) == false) {
                bool usgGSC = false;
                int leaderPipeId = 0;
                int subPipesId = 0;

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    if (request->hasStream(HAL_STREAM_ID_VIDEO) == true
                        || m_flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
                        || pp_scenario == PP_SCENARIO_SW_VDIS
#endif
#ifdef SAMSUNG_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                        || pp_scenario == PP_SCENARIO_HIFI_VIDEO
#endif
                        ) {
                        usgGSC = true;
                        if (pp_scenario == PP_SCENARIO_HIFI_VIDEO) {
                            if (request->hasStream(HAL_STREAM_ID_VIDEO) == false
                                && m_flagVideoStreamPriority == false) {
                                usgGSC = false;
                            } else {
                                leaderPipeId = PIPE_PP_UNI;
                                subPipesId = PIPE_PP_UNI;
                            }
                        } else {
                            leaderPipeId = PIPE_FUSION;
                            subPipesId = PIPE_FUSION;
                        }
                    }
                } else
#endif
                {
#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_HIFI_VIDEO)
                    if (pp_scenario == PP_SCENARIO_SW_VDIS
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                        || pp_scenario == PP_SCENARIO_HIFI_VIDEO) {
                        usgGSC = true;
                        if (pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                            || pp_scenario == PP_SCENARIO_HIFI_VIDEO) {
                            if (request->hasStream(HAL_STREAM_ID_VIDEO) == false
                                && m_flagVideoStreamPriority == false) {
                                usgGSC = false;
                            } else {
                                leaderPipeId = PIPE_PP_UNI;
                                subPipesId = PIPE_PP_UNI;
                            }
                        } else {
                            leaderPipeId = PIPE_ISP;
                            subPipesId = pp_port;
                        }
                        flipHorizontal = m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL);

                        if (flipHorizontal &&
                            (pp_scenario == PP_SCENARIO_SW_VDIS
                                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                                || pp_scenario == PP_SCENARIO_HIFI_VIDEO)) {
                            frame->setFlipHorizontal(PIPE_GSC, flipHorizontal);
                        }
                    }
#endif
                }

                if (usgGSC) {
                    /* Use GSC for preview while run */
                    ret = m_setupPreviewGSC(frame, request, leaderPipeId, subPipesId, pp_scenario);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupPreviewGSC(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                pipeId, frame->getFrameCount(), ret);
                        return ret;
                    }

#ifdef SAMSUNG_TN_FEATURE
                    if (m_gscPreviewCbThread->isRunning() == false) {
                        m_gscPreviewCbThread->run();
                    }
#endif

                    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[PIPE_GSC], PIPE_GSC);
                    factory->pushFrameToPipe(frame, PIPE_GSC);
                    if (factory->checkPipeThreadRunning(PIPE_GSC) == false) {
                        factory->startThread(PIPE_GSC);
                    }
                } else {
                    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                        streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }
                    }
#endif

#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_HIFI_VIDEO)
                    if (pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
#ifndef HIFIVIDEO_INPUTCOPY_DISABLE
                        || pp_scenario == PP_SCENARIO_HIFI_VIDEO
#endif
                        ) {
                        ExynosCameraBuffer srcbuffer;
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }
                        ret = frame->getSrcBuffer(dstPipeId, &srcbuffer);
                        if (ret != NO_ERROR || srcbuffer.index < 0) {
                            CLOGE("Failed to get Src buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                    pp_port, srcbuffer.index, frame->getFrameCount(), ret);
                        } else {
                            ret = m_bufferSupplier->putBuffer(srcbuffer);
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to putBuffer. ret %d", ret);
                            }
                        }
                    }
#endif

                    streamId = m_streamManager->getYuvStreamId(pp_port - PIPE_MCSC0);
                    if (request != NULL) {
                        request->setStreamBufferStatus(streamId, streamBufferState);

                        ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                    frame->getStreamTimestamp(), frame->getParameters());
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to resultCallback."
                                    " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                    pp_port, buffer.index, frame->getFrameCount(), streamId, ret);
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

#ifdef SAMSUNG_STR_BV_OFFSET
                if (request != NULL && frame->getBvOffset() > 0) {
                    request->setBvOffset(frame->getBvOffset());
                }
#endif

#ifdef SAMSUNG_FOCUS_PEAKING
                if (frame->getRequest(PIPE_VC1) == true
                    && pp_scenario == PP_SCENARIO_FOCUS_PEAKING) {
                    int pipeId = PIPE_FLITE;
                    ExynosCameraBuffer depthMapBuffer;

                    if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
                        pipeId = PIPE_3AA;
                    }

                    depthMapBuffer.index = -2;
                    ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to get DepthMap buffer");
                    }

                    if (depthMapBuffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                                    frame->getFrameCount(), depthMapBuffer.index, ret);
                        }
                    }
                }
#endif
            } else {
                ExynosCameraBuffer dstBuffer;
                int pipeId_next = PIPE_PP_UNI2;
                ExynosRect srcRect;
                ExynosRect dstRect;

                ret = m_setupPreviewUniPP(frame, request, pipeId, pipeId, pipeId_next);
                if (ret != NO_ERROR) {
                    CLOGE("m_setupPreviewUniPP(Pipe:%d) failed, Fcount(%d), ret(%d)",
                        pipeId, frame->getFrameCount(), ret);
                    return ret;
                }

                if (m_previewStreamPPPipe2Thread->isRunning() == false) {
                    m_previewStreamPPPipe2Thread->run();
                }

                factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId_next], pipeId_next);
                factory->pushFrameToPipe(frame, pipeId_next);
                if (factory->checkPipeThreadRunning(pipeId_next) == false) {
                    factory->startThread(pipeId_next);
                }
            }
        }
        break;

    case PIPE_PP_UNI2:
        if (pipeId == PIPE_PP_UNI2) {
            int dstPipeId = PIPE_PP_UNI2;
            int pp_port = MAX_PIPE_NUM;
            int pp_scenario = frame->getPPScenario(PIPE_PP_UNI2);

            pp_port = m_getPortPreviewUniPP(request, pp_scenario);

            ret = frame->getDstBuffer(dstPipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                    pp_port, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = frame->getDstBufferState(dstPipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", dstPipeId, ret);
                return ret;
            }

            /* pp_scenario == PP_SCENARIO_HIFI_VIDEO : Sync is done in solution library */
            if (pp_scenario == PP_SCENARIO_STR_PREVIEW
                || pp_scenario == PP_SCENARIO_SW_VDIS
                || pp_scenario == PP_SCENARIO_HYPERLAPSE
                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                ExynosRect rect;
                int planeCount = 0;

                frame->getDstRect(dstPipeId, &rect);
                planeCount = getYuvPlaneCount(rect.colorFormat);

                for (int plane = 0; plane < planeCount; plane++) {
                    if (m_ionClient >= 0)
                        exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
                }
            }

            if (frame->getRequest(PIPE_PP_UNI3) == false) {
                bool usgGSC = false;
                int leaderPipeId = 0;
                int subPipesId = 0;

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    if (request->hasStream(HAL_STREAM_ID_VIDEO) == true
                        || m_flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
                        || pp_scenario == PP_SCENARIO_SW_VDIS
#endif
#ifdef SAMSUNG_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
#endif
                        ) {
                        usgGSC = true;
                        if (pp_scenario == PP_SCENARIO_SW_VDIS
                            && frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                            leaderPipeId = PIPE_PP_UNI;
                            subPipesId = PIPE_PP_UNI;
                        } else {
                            leaderPipeId = PIPE_FUSION;
                            subPipesId = PIPE_FUSION;
                        }
                    }
                } else
#endif
                {
#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_HIFI_VIDEO)
                    if (pp_scenario == PP_SCENARIO_SW_VDIS
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                        usgGSC = true;
                        if (pp_scenario == PP_SCENARIO_SW_VDIS
                            && frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true) {
                            leaderPipeId = PIPE_PP_UNI;
                            subPipesId = PIPE_PP_UNI;
                        } else if (pp_scenario == PP_SCENARIO_SW_VDIS
                            && frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                            leaderPipeId = PIPE_PP_UNI;
                            subPipesId = PIPE_PP_UNI;
                        } else if (pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                            && frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                            if (request->hasStream(HAL_STREAM_ID_VIDEO) == false
                                && m_flagVideoStreamPriority == false) {
                                usgGSC = false;
                            } else {
                                leaderPipeId = PIPE_PP_UNI2;
                                subPipesId = PIPE_PP_UNI2;
                            }
                        } else {
                            leaderPipeId = PIPE_ISP;
                            subPipesId = pp_port;
                        }
                        flipHorizontal = m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL);

                        if (flipHorizontal &&
                            (pp_scenario == PP_SCENARIO_SW_VDIS
                                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                                || pp_scenario == PP_SCENARIO_HIFI_VIDEO)) {
                            frame->setFlipHorizontal(PIPE_GSC, flipHorizontal);
                        }
                    }
#endif
                }

                if (usgGSC) {
                    /* Use GSC for preview while run */
                    ret = m_setupPreviewGSC(frame, request, leaderPipeId, subPipesId, pp_scenario);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupPreviewGSC(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                pipeId, frame->getFrameCount(), ret);
                        return ret;
                    }

                    if (m_gscPreviewCbThread->isRunning() == false) {
                        m_gscPreviewCbThread->run();
                    }

                    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[PIPE_GSC], PIPE_GSC);
                    factory->pushFrameToPipe(frame, PIPE_GSC);
                    if (factory->checkPipeThreadRunning(PIPE_GSC) == false) {
                        factory->startThread(PIPE_GSC);
                    }
                } else {
                    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                        streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }
                    }
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
                    if (pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                        ExynosCameraBuffer srcbuffer;
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }
                        ret = frame->getSrcBuffer(dstPipeId, &srcbuffer);
                        if (ret != NO_ERROR || srcbuffer.index < 0) {
                            CLOGE("Failed to get Src buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                    pp_port, srcbuffer.index, frame->getFrameCount(), ret);
                        } else {
                            ret = m_bufferSupplier->putBuffer(srcbuffer);
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to putBuffer. ret %d", ret);
                            }
                        }
                    }
#endif
                    streamId = m_streamManager->getYuvStreamId(pp_port - PIPE_MCSC0);
                    if (request != NULL) {
                        request->setStreamBufferStatus(streamId, streamBufferState);

                        ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                    frame->getStreamTimestamp(), frame->getParameters());
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to resultCallback."
                                    " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                    pp_port, buffer.index, frame->getFrameCount(), streamId, ret);
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

#ifdef SAMSUNG_STR_BV_OFFSET
                if (request != NULL && frame->getBvOffset() > 0) {
                    request->setBvOffset(frame->getBvOffset());
                }
#endif

#ifdef SAMSUNG_FOCUS_PEAKING
                if (frame->getRequest(PIPE_VC1) == true
                    && pp_scenario == PP_SCENARIO_FOCUS_PEAKING) {
                    int pipeId = PIPE_FLITE;
                    ExynosCameraBuffer depthMapBuffer;

                    if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
                        pipeId = PIPE_3AA;
                    }

                    depthMapBuffer.index = -2;
                    ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to get DepthMap buffer");
                    }

                    if (depthMapBuffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                                frame->getFrameCount(), depthMapBuffer.index, ret);
                        }
                    }
                }
#endif
            } else {
                ExynosCameraBuffer dstBuffer;
                int pipeId_next = PIPE_PP_UNI3;
                ExynosRect srcRect;
                ExynosRect dstRect;

                ret = m_setupPreviewUniPP(frame, request, pipeId, pipeId, pipeId_next);
                if (ret != NO_ERROR) {
                    CLOGE("m_setupPreviewUniPP(Pipe:%d) failed, Fcount(%d), ret(%d)",
                        pipeId, frame->getFrameCount(), ret);
                    return ret;
                }

                if (m_previewStreamPPPipe3Thread->isRunning() == false) {
                    m_previewStreamPPPipe3Thread->run();
                }

                factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId_next], pipeId_next);
                factory->pushFrameToPipe(frame, pipeId_next);
                if (factory->checkPipeThreadRunning(pipeId_next) == false) {
                    factory->startThread(pipeId_next);
                }
            }
        }
        break;

    case PIPE_PP_UNI3:
        if (pipeId == PIPE_PP_UNI3) {
            int dstPipeId = PIPE_PP_UNI3;
            int pp_port = MAX_PIPE_NUM;
            int pp_scenario = frame->getPPScenario(PIPE_PP_UNI3);

            pp_port = m_getPortPreviewUniPP(request, pp_scenario);

            ret = frame->getDstBuffer(dstPipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                    pp_port, buffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            ret = frame->getDstBufferState(dstPipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", dstPipeId, ret);
                return ret;
            }

            /* pp_scenario == PP_SCENARIO_HIFI_VIDEO : Sync is done in solution library */
            if (pp_scenario == PP_SCENARIO_STR_PREVIEW
                || pp_scenario == PP_SCENARIO_SW_VDIS
                || pp_scenario == PP_SCENARIO_HYPERLAPSE
                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                ExynosRect rect;
                int planeCount = 0;

                frame->getDstRect(dstPipeId, &rect);
                planeCount = getYuvPlaneCount(rect.colorFormat);

                for (int plane = 0; plane < planeCount; plane++) {
                    if (m_ionClient >= 0)
                        exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
                }
            }

            {
                bool usgGSC = false;
                int leaderPipeId = 0;
                int subPipesId = 0;

#ifdef USE_DUAL_CAMERA
                if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    if (request->hasStream(HAL_STREAM_ID_VIDEO) == true
                        || m_flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
                        || pp_scenario == PP_SCENARIO_SW_VDIS
#endif
#ifdef SAMSUNG_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
#endif
                        ) {
                        usgGSC = true;
                        leaderPipeId = PIPE_FUSION;
                        subPipesId = PIPE_FUSION;
                    }
                } else
#endif
                {
#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_HIFI_VIDEO)
                    if (pp_scenario == PP_SCENARIO_SW_VDIS
                        || pp_scenario == PP_SCENARIO_HYPERLAPSE
                        || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
                        usgGSC = true;
                        if (pp_scenario == PP_SCENARIO_SW_VDIS
                            && frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true
                            && frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true) {
                            leaderPipeId = PIPE_PP_UNI2;
                            subPipesId = PIPE_PP_UNI2;
                        } else {
                            leaderPipeId = PIPE_ISP;
                            subPipesId = pp_port;
                        }
                        flipHorizontal = m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL);

                        if (flipHorizontal &&
                            (pp_scenario == PP_SCENARIO_SW_VDIS
                                || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                                || pp_scenario == PP_SCENARIO_HIFI_VIDEO)) {
                            frame->setFlipHorizontal(PIPE_GSC, flipHorizontal);
                        }
                    }
#endif
                }

                if (usgGSC) {
                    /* Use GSC for preview while run */
                    ret = m_setupPreviewGSC(frame, request, leaderPipeId, subPipesId, pp_scenario);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupPreviewGSC(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                pipeId, frame->getFrameCount(), ret);
                        return ret;
                    }

#ifdef SAMSUNG_TN_FEATURE
                    if (m_gscPreviewCbThread->isRunning() == false) {
                        m_gscPreviewCbThread->run();
                    }
#endif

                    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[PIPE_GSC], PIPE_GSC);
                    factory->pushFrameToPipe(frame, PIPE_GSC);
                    if (factory->checkPipeThreadRunning(PIPE_GSC) == false) {
                        factory->startThread(PIPE_GSC);
                    }
                } else {
                    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                        streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

#ifdef USE_DUAL_CAMERA
                    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }
                    }
#endif

                    streamId = m_streamManager->getYuvStreamId(pp_port - PIPE_MCSC0);
                    if (request != NULL) {
                        request->setStreamBufferStatus(streamId, streamBufferState);

                        ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                    frame->getStreamTimestamp(), frame->getParameters());
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to resultCallback."
                                    " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                    pp_port, buffer.index, frame->getFrameCount(), streamId, ret);
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
            }

#ifdef SAMSUNG_STR_BV_OFFSET
            if (request != NULL && frame->getBvOffset() > 0) {
                request->setBvOffset(frame->getBvOffset());
            }
#endif

#ifdef SAMSUNG_FOCUS_PEAKING
            if (frame->getRequest(PIPE_VC1) == true
                && pp_scenario == PP_SCENARIO_FOCUS_PEAKING) {
                int pipeId = PIPE_FLITE;
                ExynosCameraBuffer depthMapBuffer;

                if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
                    pipeId = PIPE_3AA;
                }

                depthMapBuffer.index = -2;
                ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                if (depthMapBuffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), depthMapBuffer.index, ret);
                    }
                }
            }
#endif
        }
        break;
#endif /* SAMSUNG_TN_FEATURE */

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

                ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                            frame->getStreamTimestamp(), frame->getParameters());
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
#if defined (SAMSUNG_SW_VDIS) || defined (SAMSUNG_HYPERLAPSE) || defined (SAMSUNG_HIFI_VIDEO)
                    {
                        int pp_scenario = frame->getPPScenario(PIPE_PP_UNI);

                        if ((m_flagVideoStreamPriority == true && request->hasStream(HAL_STREAM_ID_VIDEO) == false)
#ifdef SAMSUNG_SW_VDIS
                            || pp_scenario == PP_SCENARIO_SW_VDIS
#endif
#ifdef SAMSUNG_HYPERLAPSE
                            || pp_scenario == PP_SCENARIO_HYPERLAPSE
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                            || pp_scenario == PP_SCENARIO_HIFI_VIDEO
#endif
                            ) {
                            ExynosCameraBuffer dstBuffer;
                            int nodePipeId = PIPE_FUSION;

                            dstBuffer.index = -2;

                            ret = frame->getDstBuffer(nodePipeId, &dstBuffer, factory->getNodeType(nodePipeId));
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to getDst buffer, pipeId(%d), ret(%d)", pipeId, ret);
                            } else {
                                if (dstBuffer.index >= 0) {
                                    ret = m_bufferSupplier->putBuffer(dstBuffer);
                                    if (ret != NO_ERROR) {
                                        CLOGE("Failed to putBuffer. pipeId(%d), ret(%d)", pipeId, ret);
                                    }
                                }
                            }
                        }
                    }
#endif

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

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
                /* Check Condition for Dual Camera*/
                if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                    m_updateBeforeDualSolution(frame, pipeId_next);
                }
#endif
            }

            if (pipeId_next >= 0) {
                factory->pushFrameToPipe(frame, pipeId_next);
            }
        }
        break;

    case PIPE_FUSION:
        if (pipeId == PIPE_FUSION) {
            if ((m_configurations->getMode(CONFIGURATION_YSUM_RECORDING_MODE) == true)
                    && (request != NULL)) {
                /* Update YSUM value for YSUM recording */
                ret = m_updateYsumValue(frame, request, pipeId);
                if (ret != NO_ERROR) {
                    CLOGE("failed to m_updateYsumValue, ret(%d)", ret);
                    return ret;
                }
            }

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

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
            /* Check Condition for Dual Camera*/
            if (dualPreviewMode == DUAL_PREVIEW_MODE_SW) {
                m_updateAfterDualSolution(frame);
            }
#else
            /* set N-1 zoomRatio in Frame */
            if (m_configurations->getAppliedZoomRatio() < 1.0f) {
                frame->setAppliedZoomRatio(frame->getZoomRatio());
            } else {
                frame->setAppliedZoomRatio(m_configurations->getAppliedZoomRatio());
            }

            m_configurations->setAppliedZoomRatio(frame->getZoomRatio());
#endif
#if 0
            CLOGV("Return FUSION Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
#endif
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
#ifdef DEBUG_IQ_OSD
                if (bufferState != ENTITY_BUFFER_STATE_ERROR) {
                    int yuvSizeW = 0, yuvSizeH = 0;

                    int portId = m_streamManager->getOutputPortId(streamId);
                    m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&yuvSizeW, (uint32_t *)&yuvSizeH, portId);
                    shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
                    printOSD(buffer.addr[0], buffer.addr[1], yuvSizeW, yuvSizeH, shot_ext);
                }
#endif
#ifdef SAMSUNG_TN_FEATURE
                if (frame->getRequest(PIPE_PP_UNI) == true) {
                    int pipeId_next = PIPE_PP_UNI;

                    ret = m_setupPreviewUniPP(frame, request, pipeId, pipeId, pipeId_next);
                    if (ret != NO_ERROR) {
                        CLOGE("m_setupPreviewUniPP(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                pipeId, frame->getFrameCount(), ret);
                        return ret;
                    }

                    if (m_previewStreamPPPipeThread->isRunning() == false) {
                        m_previewStreamPPPipeThread->run();
                    }

                    factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[pipeId_next], pipeId_next);
                    factory->pushFrameToPipe(frame, pipeId_next);
                    if (factory->checkPipeThreadRunning(pipeId_next) == false) {
                        factory->startThread(pipeId_next);
                    }
                } else
#endif
                {
                    if (request->hasStream(HAL_STREAM_ID_VIDEO) == false
                        && m_flagVideoStreamPriority == false) {
                        if (request != NULL && request->hasStream(HAL_STREAM_ID_CALLBACK) == true) {
                            request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                            m_copyPreviewCbThreadFunc(request, frame, &buffer);
                        }

                        request->setStreamBufferStatus(streamId, streamBufferState);
                        ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                    frame->getStreamTimestamp(), frame->getParameters());
                        if (ret != NO_ERROR) {
                            CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                                    request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                                    ret);
                            return ret;
                        }
                    } else {
#ifdef USE_DUAL_CAMERA
#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY)
                        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                            ret = m_setupPreviewGSC(frame, request, PIPE_FUSION, PIPE_FUSION);

                            if (ret != NO_ERROR) {
                                CLOGE("m_setupPreviewGSC(Pipe:%d) failed, Fcount(%d), ret(%d)",
                                    pipeId, frame->getFrameCount(), ret);
                                return ret;
                            }

                            if (m_gscPreviewCbThread->isRunning() == false) {
                                m_gscPreviewCbThread->run();
                            }

                            factory->setOutputFrameQToPipe(m_pipeFrameDoneQ[PIPE_GSC], PIPE_GSC);
                            factory->pushFrameToPipe(frame, PIPE_GSC);
                            if (factory->checkPipeThreadRunning(PIPE_GSC) == false) {
                                factory->startThread(PIPE_GSC);
                            }
                        } else
#endif
#endif
                        {
                            ret = m_sendYuvStreamResult(request, &buffer, streamId, false,
                                                        frame->getStreamTimestamp(), frame->getParameters());
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

        ret = m_updateTimestamp(request, frame, &buffer, &components);
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
#ifdef SAMSUNG_FEATURE_SHUTTER_NOTIFICATION
    if (type == EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_SHUTTER) {
        resultMeta = request->getShutterMeta();
        CLOGD("Notify Shutter Sound");
    } else
#endif
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
        || m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true
        || m_configurations->getSamsungCamera() == false) {    /* To fix ITS issue */
        m_checkFirstFrameLux = false;
        return;
    }

    data = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[399];

#ifdef SAMSUNG_TN_FEATURE
    if (data <= ENTRANCE_LOW_LUX) {
        CLOGD("(%s[%d]): need skip frame for ae/awb stable(%d).", __FUNCTION__, __LINE__, data);
        m_configurations->setFrameSkipCount(2);
    }
#endif

    m_checkFirstFrameLux = false;
}

status_t ExynosCamera::m_sendYuvStreamResult(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraBuffer *buffer, int streamId, bool skipBuffer,
                                             __unused uint64_t streamTimeStamp, __unused ExynosCameraParameters *params
#ifdef SAMSUNG_SSM
                                             , bool ssmframeFlag
#endif
                                             )
{
    status_t ret = NO_ERROR;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_capture_result_t *requestResult = NULL;
    ResultRequest resultRequest = NULL;
    bool OISCapture_activated = false;
#ifdef OIS_CAPTURE
    ExynosCameraActivityControl *activityControl = params->getActivityControl();
    ExynosCameraActivitySpecialCapture *sCaptureMgr = activityControl->getSpecialCaptureMgr();
    unsigned int OISFcount = 0;
    unsigned int fliteFcount = 0;
    unsigned int skipFcountStart = 0;
    unsigned int skipFcountEnd = 0;
    struct camera2_shot_ext *shot_ext = NULL;
#ifdef SAMSUNG_LLS_DEBLUR
    int ldCaptureCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
#endif
    unsigned int longExposureRemainCount = m_configurations->getLongExposureShotCount();
#endif
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

#ifdef OIS_CAPTURE
        shot_ext = (struct camera2_shot_ext *) buffer->addr[buffer->getMetaPlaneIndex()];

        fliteFcount = shot_ext->shot.dm.request.frameCount;
        OISFcount = sCaptureMgr->getOISCaptureFcount();

        if (OISFcount) {
#ifdef SAMSUNG_LLS_DEBLUR
            if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE) {
                skipFcountStart = OISFcount;
                skipFcountEnd = OISFcount + ldCaptureCount;
            } else
#endif
            {
                if (m_configurations->getCaptureExposureTime() > CAMERA_SENSOR_EXPOSURE_TIME_MAX) {
                    skipFcountStart = OISFcount;
                    skipFcountEnd = OISFcount + longExposureRemainCount;
                } else {
                    skipFcountStart = OISFcount;
                    skipFcountEnd = OISFcount + 1;
                }
            }

            if (fliteFcount >= skipFcountStart && fliteFcount < skipFcountEnd) {
                OISCapture_activated = true;
                CLOGD("Skip lls frame. fliteFcount(%d), start(%d) end(%d)",
                        fliteFcount, skipFcountStart, skipFcountEnd);
            }
        }
#endif

#ifdef SAMSUNG_TN_FEATURE
        m_checkEntranceLux(shot_ext);
#endif
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

#ifdef SAMSUNG_EVENT_DRIVEN
        if ((m_configurations->getModeValue(CONFIGURATION_SHOT_MODE)
                == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SLOW_MOTION)
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_CALLBACK)) {
            int eventDrivenFps = EEVENT_DRIVEN_FPS_REFERENCE;
            ratio = (int)((dispFps * 10 / eventDrivenFps) / buffer->batchSize / 10);
            m_eventDrivenToggle = (m_eventDrivenToggle + 1) % ratio;
            skipStream = (m_eventDrivenToggle == 0) ? false : true;
        }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        if (m_configurations->getBokehProcessResult() < 0) {
            CLOGE("[R%d F%d S%d B%d]Failed to BokehPreview Process.",
                    request->getKey(), request->getFrameCount(), streamId, buffer->index);
            skipStream = true;
        }
#endif

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

#ifdef SAMSUNG_HDR10_RECORDING
        if ((m_configurations->getMode(CONFIGURATION_HDR_RECORDING_MODE) == true)
            && ((streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_PREVIEW)
                || (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO))
            && (request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_OK)) {
            ret = updateHDRBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("updateHDRBuffer fail, bufferIndex(%d), ret(%d)", buffer->index, ret);
                return ret;
            }
        }
#endif  /* SAMSUNG_HDR10_RECORDING */

#ifdef SAMSUNG_HYPERLAPSE
        if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO)
            && request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_OK) {
            resultRequest->setStreamTimeStamp(streamTimeStamp);
        }
#endif
#ifdef SAMSUNG_SW_VDIS
        if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO)
            && request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_OK) {
            resultRequest->setStreamTimeStamp(streamTimeStamp);
        }
#endif

#ifdef SAMSUNG_SSM
        if (m_SSMOrgRecordingtime > 0
            && streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO
            && streamBuffer->status != CAMERA3_BUFFER_STATUS_ERROR) {
            int ssmDuration = SSM_RECORDING_960_DURATION;

#ifdef SAMSUNG_SSM_FRC
            if (m_configurations->getModeValue(CONFIGURATION_SSM_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_SHOT_MODE_MULTI_FRC
                || m_configurations->getModeValue(CONFIGURATION_SSM_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_SHOT_MODE_SINGLE_FRC) {
                ssmDuration = SSM_RECORDING_480_DURATION; /* 480fps */
            } else
#endif
            {
                ssmDuration = SSM_RECORDING_960_DURATION; /* 1000fps */
            }

            if (ssmframeFlag == true) {
                if (m_SSMRecordingtime == m_SSMOrgRecordingtime) { /* ssm first frame */
                    m_SSMRecordingtime -= 33333333;
                    m_SSMRecordingtime += ssmDuration;
                }
            } else {
                if (m_SSMRecordingtime != m_SSMOrgRecordingtime) { /* ssm last frame */
                    m_SSMRecordingtime = m_SSMOrgRecordingtime;
                }
            }

            resultRequest->setStreamTimeStamp(m_SSMRecordingtime);
            CLOGV("[R%d F%d S%d] m_SSMRecordingtime(%lld) m_SSMRecordingtime(%lld) getStreamTimeStamp(%lld) #out(%d) ssmframeFlag(%d)",
                    request->getKey(), request->getFrameCount(), streamId,
                    m_SSMRecordingtime, m_SSMOrgRecordingtime, resultRequest->getStreamTimeStamp(),
                    requestResult->num_output_buffers, ssmframeFlag);

            m_SSMOrgRecordingtime = m_SSMOrgRecordingtime + 33333333; /*30fps*/

            if (ssmframeFlag == true) {
                m_SSMRecordingtime += ssmDuration;
            } else {
                m_SSMRecordingtime = m_SSMOrgRecordingtime;
            }
        }
#endif

#ifdef SAMSUNG_HYPERLAPSE_DEBUGLOG
        if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO)
            && request->getStreamBufferStatus(streamId) == CAMERA3_BUFFER_STATUS_OK) {
            bool printLog = false;
            m_recordingCallbackCount++;
            if (m_recordingCallbackCount < 5
                || m_recordingCallbackCount % 100 == 1) {
                printLog = true;
            }

            if (printLog) {
                CLOGD("[R%d F%d S%d shotmode%d]  timestamp(%lld) getStreamTimeStamp(%lld) #out(%d)",
                        request->getKey(), request->getFrameCount(), streamId,
                        m_configurations->getModeValue(CONFIGURATION_SHOT_MODE),
                        request->getSensorTimestamp(), resultRequest->getStreamTimeStamp(),
                        requestResult->num_output_buffers);
            }
        }
#endif

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

#ifdef SAMSUNG_TN_FEATURE
        if (m_configurations->getSamsungCamera() == true)
            option = streamBuffer->stream->option;
#endif

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

    if (m_scenario == SCENARIO_SECURE) {
        ret = factory->setControl(V4L2_CID_SENSOR_SET_SHUTTER, m_shutterSpeed, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_shutterSpeed(%d)", m_shutterSpeed);
        }

        ret = factory->setControl(V4L2_CID_SENSOR_SET_GAIN, m_gain, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_gain(%d)", m_gain);
        }

        ret = factory->setControl(V4L2_CID_IRLED_SET_WIDTH, m_irLedWidth, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedWidth(%d)", m_irLedWidth);
        }

        ret = factory->setControl(V4L2_CID_IRLED_SET_DELAY, m_irLedDelay, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedDelay(%d)", m_irLedDelay);
        }

        ret = factory->setControl(V4L2_CID_IRLED_SET_CURRENT, m_irLedCurrent, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedCurrent(%d)", m_irLedCurrent);
        }

        ret = factory->setControl(V4L2_CID_IRLED_SET_ONTIME, m_irLedOnTime, pipeId);
        if (ret < 0) {
            CLOGE("FLITE setControl fail, ret(%d)", ret);
            CLOGD("m_irLedOnTime(%d)", m_irLedOnTime);
        }
    }

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

#ifdef SAMSUNG_HIFI_CAPTURE
    m_setupReprocessingPipeline_HifiCapture(factory);
#endif

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

        if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
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
    status_t notifyErrorFlag = INVALID_OPERATION;
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
    ExynosCameraActivityFlash *flashMgr = NULL;
#ifdef SAMSUNG_DOF
    int shotMode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);
#endif
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
#endif
    bool retry = false;
    bayerBuffer.index = -2;

    captureExposureTime = m_configurations->getCaptureExposureTime();
    frameType = (enum FRAME_TYPE) frame->getFrameType();

#ifdef SAMSUNG_TN_FEATURE
BAYER_RETRY:
#endif

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
    flashMgr = components.activityControl->getFlashMgr();

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
            camera2_shot_ext frameShot;
            frame->getMetaData(&frameShot);

            if (frameShot.shot.ctl.flash.flashMode == CAM2_FLASH_MODE_SINGLE
                && flashMgr->getNeedFlash() == false) {
                CLOGW("[F%d B%d]Invalid flash buffer. Retry.",
                        frame->getFrameCount(), bayerBuffer.index);
                retry = true;
                usleep(3000);
            } else {
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
    if (ret != NO_ERROR || bayerBuffer.index < 0) {
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

#ifdef SAMSUNG_DOF
    m_selectBayer_dof(shotMode, autoFocusMgr, components);
#endif

    // shot_ext->shot.udm.ae.vendorSpecific[398], 1 : main-flash fired
    // When captureExposureTime is greater than CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT(100msec)
    // and is less than PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX(300msec)
    // and flash is on,
    // RTA can't save captureExposureTime to shot_ext->shot.dm.sensor.exposureTime.
    // So HAL will skip the checking the exposureTime.
    if (!frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
#ifdef SAMSUNG_TN_FEATURE
        if (m_selectBayer_tn(frame, captureExposureTime, shot_ext, bayerBuffer, depthMapBuffer)) {
            goto BAYER_RETRY;
        } else
#endif /* SAMSUNG_TN_FEATURE */
        if (m_configurations->getCaptureExposureTime() != 0
            && m_longExposureRemainCount > 0
            && m_configurations->getLongExposureShotCount() > 1) {
            ExynosRect srcRect, dstRect;
            ExynosCameraBuffer *srcBuffer = NULL;
            ExynosCameraBuffer *dstBuffer = NULL;
            bool isPacked = false;

            CLOGD("m_longExposureRemainCount(%d) getLongExposureShotCount()(%d)",
                m_longExposureRemainCount, m_configurations->getLongExposureShotCount());

            if (m_longExposureRemainCount == (unsigned int)m_configurations->getLongExposureShotCount()) {
                /* First Bayer Buffer */
                m_newLongExposureCaptureBuffer = bayerBuffer;
                m_longExposureRemainCount--;
                notifyErrorFlag = NO_ERROR;
                goto CLEAN_FRAME;
            }

#ifdef USE_3AA_CROP_AFTER_BDS
            if (components.parameters->getUsePureBayerReprocessing() == true) {
                components.parameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&srcRect.w, (uint32_t *)&srcRect.h);
                dstRect = srcRect;
            } else
#endif
            {
                components.parameters->getPictureBayerCropSize(&srcRect, &dstRect);
            }

            if (components.parameters->getBayerFormat(m_getBayerPipeId()) == V4L2_PIX_FMT_SBGGR16) {
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
                notifyErrorFlag = NO_ERROR;
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
            ret = m_updateTimestamp(request, frame, &bayerBuffer, &components);
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

#ifdef SAMSUNG_FEATURE_SHUTTER_NOTIFICATION
        if (m_configurations->getSamsungCamera() == true) {
            m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_SHUTTER);
        }
#endif
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

#ifdef SAMSUNG_TN_FEATURE
    if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_BURST
        || m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_AGIF) {
        m_doneMultiCaptureRequest = request->getKey();
    }
#endif

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

        if (notifyErrorFlag != NO_ERROR && request != NULL) {
            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[F%d] sendNotifyError fail. ret %d",
                        frame->getFrameCount(), ret);
            }
        }

        frame = NULL;
    }

    return false;
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
#ifdef SAMSUNG_LLS_DEBLUR
        && frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_NONE
#endif
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

#ifdef SAMSUNG_UTC_TS
        m_configurations->setUTCInfo();
#endif

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

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
    if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
        && m_configurations->getMode(CONFIGURATION_FACTORY_TEST_MODE) == false
        && (frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_NONE || frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_JPEG)) {
        ExynosCameraDurationTimer sefProcessTimer;
        sefProcessTimer.start();
        m_sendSefStreamResult(request, frame, &buffer);
        sefProcessTimer.stop();
        CLOGD("[BokehCapture] SEF ProcessTime = %d usec ", (int)sefProcessTimer.durationUsecs());
    } else
#endif
    {
        m_sendJpegStreamResult(request, &buffer, jpegOutputSize);
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE) {
        frame->setFrameSpecialCaptureStep(SCAPTURE_STEP_DONE);
    }
#endif

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
#ifdef SAMSUNG_LLS_DEBLUR
    int currentLDCaptureStep = SCAPTURE_STEP_NONE;
    int currentLDCaptureMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE);
    int LDCaptureLastStep  = SCAPTURE_STEP_COUNT_1 + m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT) - 1;
#endif

#ifdef SAMSUNG_TN_FEATURE
    int yuvStallPipeId = -1;
#endif
    bool needNV21CallbackHandle = false;

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

#ifdef SAMSUNG_LLS_DEBLUR
    currentLDCaptureStep = (int)frame->getFrameSpecialCaptureStep();
#endif

    CLOGD("[F%d T%d]YUV capture done. entityID %d, FrameState(%d)",
            frame->getFrameCount(), frame->getFrameType(), pipeId, frame->getFrameState());

    switch (pipeId) {
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

        ret = frame->getSrcBuffer(pipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            goto FUNC_EXIT;
        }

        ret = frame->getSrcBufferState(pipeId, &bufferState);
        if (ret < 0) {
            CLOGE("[F%d B%d]getSrcBufferState fail. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
            goto FUNC_EXIT;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error. pipeId %d",
                    frame->getFrameCount(), buffer.index, pipeId);

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

            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):reprocessing fail!!! pipeId %d",
                    __FUNCTION__, __LINE__, pipeId);
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
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
            ret = frame->getDstBuffer(pipeId, &buffer, components.reprocessingFactory->getNodeType(dstPipeId));
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
                        frame->getFrameCount(), buffer.index, pipeId);
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
                        frame->getFrameCount(), buffer.index, pipeId);
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
                ) && pipeId == PIPE_3AA_REPROCESSING
                && ((frame->getRequest(PIPE_3AC_REPROCESSING) == true)
                || (frame->getRequest(PIPE_3AG_REPROCESSING) == true))) {
#if defined(USE_RAW_REVERSE_PROCESSING) && defined(USE_SW_RAW_REVERSE_PROCESSING)
                if (components.parameters->isUseRawReverseReprocessing() == true) {
                    /* reverse the raw buffer */
                    m_reverseProcessingBayerQ->pushProcessQ(&frame);
                } else
#endif
                {
                    if (components.parameters->isUse3aaDNG()) {
                        dstPos = components.reprocessingFactory->getNodeType(PIPE_3AG_REPROCESSING);
                    } else {
                        dstPos = components.reprocessingFactory->getNodeType(PIPE_3AC_REPROCESSING);
                    }

                    ret = frame->getDstBuffer(pipeId, &serviceBuffer, dstPos);
                    if (ret != NO_ERROR || serviceBuffer.index < 0) {
                        CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                                frame->getFrameCount(), serviceBuffer.index, dstPos, ret);
                        goto FUNC_EXIT;
                    }

                    ret = frame->getDstBufferState(pipeId, &bufferState, dstPos);
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

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
        m_captureStreamThreadFunc_dualPortraitSef(frame, components, dstPipeId);
#endif

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            break;
        } else if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
                /* This is master NV21 src buffer */
                int yuvStallPort = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT);

                dstPipeId = yuvStallPort + PIPE_MCSC0_REPROCESSING;

                ret = frame->getDstBuffer(pipeId, &buffer, components.reprocessingFactory->getNodeType(dstPipeId));
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
                            frame->getFrameCount(), buffer.index, pipeId);
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

        if (pipeId == PIPE_3AA_REPROCESSING
            && components.parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            break;
        } else if (pipeId == PIPE_ISP_REPROCESSING
                   && components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            dstPipeId = PIPE_ISPC_REPROCESSING;
            if (frame->getRequest(dstPipeId) == true) {
                dstPos = components.reprocessingFactory->getNodeType(dstPipeId);
                ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), pipeId);

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
#ifdef SAMSUNG_TN_FEATURE
        yuvStallPipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
#endif
        if (pipeId == PIPE_MCSC_REPROCESSING) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                        frame->getFrameCount(), buffer.index, pipeId, ret);
                goto FUNC_EXIT;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
                goto FUNC_EXIT;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("ERR(%s[%d]):Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), pipeId);
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

#ifdef SAMSUNG_TN_FEATURE
        int ret_val;
        ret_val = m_captureStreamThreadFunc_tn(frame, components, entity, yuvStallPipeId);
        if (ret_val < 0)
            goto FUNC_EXIT;
        else if (ret == 1)
#endif
        {
            needNV21CallbackHandle = true;
        }

        /* Handle yuv capture buffer */
        if (frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleYuvCaptureFrame. pipeId %d ret %d", pipeId, ret);
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

                ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
                if (ret < 0) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                }

                if (buffer.index < 0) {
                    CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                            buffer.index, frame->getFrameCount(), pipeId);

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

                components.reprocessingFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
                components.reprocessingFactory->pushFrameToPipe(frame, PIPE_VRA_REPROCESSING);
                break;
            }
        }

    case PIPE_VRA_REPROCESSING:
        if (pipeId == PIPE_VRA_REPROCESSING) {
            ret = frame->getSrcBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to getSrcBuffer. pipeId %d ret %d", pipeId, ret);
                return ret;
            }

            ret = frame->getSrcBufferState(pipeId, &bufferState);
            if (ret < 0) {
                CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId, ret);
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
        }

        if ((frame->getFrameYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) ||
            needNV21CallbackHandle) {
            /* Handle NV21 capture callback buffer */
            if (frame->getRequest(PIPE_MCSC0_REPROCESSING) == true
                || frame->getRequest(PIPE_MCSC1_REPROCESSING) == true
                || frame->getRequest(PIPE_MCSC2_REPROCESSING) == true) {
                ret = m_handleNV21CaptureFrame(frame, pipeId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d", pipeId, ret);
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
        if (pipeId == PIPE_JPEG_REPROCESSING
            || frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
            ret = m_handleJpegFrame(frame, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleJpegFrame. pipeId %d ret %d", pipeId, ret);
                goto FUNC_EXIT;
            }
        }
        break;

#ifdef SAMSUNG_TN_FEATURE
    case PIPE_PP_UNI_REPROCESSING:
        if (m_captureStreamThreadFunc_caseUniProcessing(frame, entity, currentLDCaptureStep, LDCaptureLastStep) != NO_ERROR) {
            goto FUNC_EXIT;
        }
        break;

    case PIPE_PP_UNI_REPROCESSING2:
        if (m_captureStreamThreadFunc_caseUniProcessing2(frame, entity) != NO_ERROR) {
            goto FUNC_EXIT;
        }
        break;
#endif /* SAMSUNG_TN_FEATURE */

#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_REPROCESSING:
#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
        if (m_captureStreamThreadFunc_caseSyncReprocessingDualPortraitSef(frame, entity, components, request) != NO_ERROR)
            goto FUNC_EXIT;
#endif

        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
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

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                        if (currentLDCaptureStep == SCAPTURE_STEP_COUNT_1) {
                            // Backup First Slave(Wide) Jpeg
                            ExynosCameraBuffer slaveJpegBuffer;
                            slaveJpegBuffer.index = -2;

                            m_liveFocusFirstSlaveJpegBuf.index = -2;

                            ret = frame->getDstBuffer(PIPE_SYNC_REPROCESSING, &slaveJpegBuffer, CAPTURE_NODE_2);

                            if (slaveJpegBuffer.index < 0) {
                                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                        slaveJpegBuffer.index, frame->getFrameCount(), PIPE_ISP_REPROCESSING);
                            } else {
                                CLOGD("slaveJpegBufferAddr(%p), slaveJpegBufferSize(%d)",
                                    slaveJpegBuffer.addr[0], slaveJpegBuffer.size[0]);
                                m_liveFocusFirstSlaveJpegBuf = slaveJpegBuffer;
                            }
                        }
                    }
#endif
                } else if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                    ret = frame->getDstBuffer(pipeId, &buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
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
                    CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
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
        if (pipeId == PIPE_FUSION_REPROCESSING) {
            if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
                if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
                    CLOGD("PIPE_FUSION_REPROCESSING");

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
                        if (currentLDCaptureStep < LDCaptureLastStep) {
                            // backup Tele(master) NV21 buffer
                            CLOGD("[F%d T%d] Buffer Backup for LiveFocus LLS. currentLDCaptureStep(%d), LDCaptureLastStep(%d)",
                                frame->getFrameCount(), frame->getFrameType(),
                                currentLDCaptureStep, LDCaptureLastStep);
                            buffer.index = -2;
                            ret = frame->getSrcBuffer(pipeId, &buffer);
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to getSrcBuffer, pipeId %d, ret %d", pipeId, ret);
                                goto FUNC_EXIT;
                            } else {
                                if (buffer.index < 0) {
                                    CLOGE("Failed to getSrcBuffer, buffer.index %d", buffer.index);
                                    goto FUNC_EXIT;
                                } else {
                                    m_liveFocusLLSBuf[CAMERA_ID_BACK_1][currentLDCaptureStep] = buffer;
                                }
                            }

                            // backup Wide(slave) NV21 buffer
                            buffer.index = -2;
                            ret = frame->getSrcBuffer(pipeId, &buffer, OUTPUT_NODE_2);
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to getSrcBuffer, pipeId %d, ret %d", pipeId, ret);
                                goto FUNC_EXIT;
                            } else {
                                if (buffer.index < 0) {
                                    CLOGE("Failed to getSrcBuffer, buffer.index %d", buffer.index);
                                    goto FUNC_EXIT;
                                } else {
                                    m_liveFocusLLSBuf[CAMERA_ID_BACK][currentLDCaptureStep] = buffer;
                                }
                            }
                        } else {
                            for (int i = SCAPTURE_STEP_COUNT_1; i < LDCaptureLastStep; i++) {
                                /* put Tele(master) buffer */
                                ret = m_bufferSupplier->putBuffer(m_liveFocusLLSBuf[CAMERA_ID_BACK_1][i]);
                                if (ret != NO_ERROR) {
                                    CLOGE("[B%d]Failed to putBuffer for pipe(%d). ret(%d)",
                                            m_liveFocusLLSBuf[CAMERA_ID_BACK_1][i].index, pipeId, ret);
                                }

                                /* put Wide(slave) buffer */
                                ret = m_bufferSupplier->putBuffer(m_liveFocusLLSBuf[CAMERA_ID_BACK][i]);
                                if (ret != NO_ERROR) {
                                    CLOGE("[B%d]Failed to putBuffer for pipe(%d). ret(%d)",
                                            m_liveFocusLLSBuf[CAMERA_ID_BACK_1][i].index, pipeId, ret);
                                }
                            }

                            frame->setFrameSpecialCaptureStep(SCAPTURE_STEP_JPEG);

                            ret = m_handleNV21CaptureFrame(frame, pipeId);
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d", pipeId, ret);
                                goto FUNC_EXIT;
                            }

                            buffer.index = -2;
                            ret = frame->getSrcBuffer(pipeId, &buffer, OUTPUT_NODE_2);
                            if (ret != NO_ERROR) {
                                CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                            }

                            if (buffer.index >= 0) {
                                ret = m_bufferSupplier->putBuffer(buffer);
                                if (ret != NO_ERROR) {
                                    CLOGE("[F%d T%d B%d]Failed to putBuffer for PIPE_FUSION_REPROCESSING. ret %d",
                                            frame->getFrameCount(), frame->getFrameType(), buffer.index, ret);
                                }
                            }
                        }
                    } else
#endif
                    {
                        buffer.index = -2;
                        ret = frame->getSrcBuffer(pipeId, &buffer, OUTPUT_NODE_2);
                        if (ret != NO_ERROR) {
                            CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId, ret);
                        }

                        if (buffer.index >= 0) {
                            ret = m_bufferSupplier->putBuffer(buffer);
                            if (ret != NO_ERROR) {
                                CLOGE("[F%d T%d B%d]Failed to putBuffer for PIPE_FUSION_REPROCESSING. ret %d",
                                        frame->getFrameCount(), frame->getFrameType(), buffer.index, ret);
                            }
                        }

                        ret = m_handleNV21CaptureFrame(frame, pipeId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d", pipeId, ret);
                            goto FUNC_EXIT;
                        }
                    }
                }
            }
        }
        break;
#endif //USE_DUAL_CAMERA

    default:
        CLOGE("Invalid pipeId %d", pipeId);
        break;
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if (currentLDCaptureMode != MULTI_SHOT_MODE_NONE
        && frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_DONE /* Step changed */) {
        CLOGD("Reset LDCaptureMode, OISCaptureMode");
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_MODE, MULTI_SHOT_MODE_NONE);
        m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
        m_configurations->setMode(CONFIGURATION_LLS_CAPTURE_MODE, false);
#ifdef FAST_SHUTTER_NOTIFY
        m_configurations->setMode(CONFIGURATION_FAST_SHUTTER_MODE, false);
#endif
        m_configurations->setModeValue(CONFIGURATION_LD_CAPTURE_COUNT, 0);
    }
#endif

    ret = frame->setEntityState(pipeId, ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                pipeId, ENTITY_STATE_COMPLETE, ret);
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
            m_configurations->setModeValue(CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT, 0);

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
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            m_configurations->setModeValue(CONFIGURATION_DUAL_HOLD_FALLBACK_STATE, 0);
#endif
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

#ifdef SAMSUNG_LLS_DEBLUR
    int currentLDCaptureStep = (int)frame->getFrameSpecialCaptureStep();
    int currentLDCaptureMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE);

    CLOGD("[F%d]getFrameSpecialCaptureStep(%d) pipeId(%d)",
            frame->getFrameCount(), frame->getFrameSpecialCaptureStep(), leaderPipeId);
#endif

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    if (leaderPipeId == PIPE_VRA_REPROCESSING) {
        leaderPipeId = m_getMcscLeaderPipeId(components.reprocessingFactory);
    }

#ifdef SAMSUNG_TN_FEATURE
    if (leaderPipeId == PIPE_PP_UNI_REPROCESSING || leaderPipeId == PIPE_PP_UNI_REPROCESSING2) {
        nodePipeId = leaderPipeId;
    } else
#endif
    if (leaderPipeId == PIPE_FUSION_REPROCESSING) {
        nodePipeId = leaderPipeId;
    } else
    {
        nodePipeId = m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL) % ExynosCameraParameters::YUV_MAX + PIPE_MCSC0_REPROCESSING;
#ifdef SAMSUNG_TN_FEATURE
        nodePipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
#endif
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

#ifdef SAMSUNG_TN_FEATURE
            m_handleNV21CaptureFrame_tn(frame, leaderPipeId, nodePipeId);
#endif /* SAMSUNG_TN_FEATURE */

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

#ifdef SAMSUNG_TN_FEATURE
                if (m_handleNV21CaptureFrame_useInternalYuvStall(frame, leaderPipeId, request, dstBuffer, nodePipeId, streamId, ret)) {
                    if (ret != NO_ERROR)
                        return ret;
                } else
#endif
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

#ifdef SAMSUNG_LLS_DEBLUR
        if (currentLDCaptureMode != MULTI_SHOT_MODE_NONE) {
            frame->setFrameSpecialCaptureStep(SCAPTURE_STEP_DONE);
        }
#endif
    } else {
        if (m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
            int pipeId_next = -1;

#ifdef SAMSUNG_TN_FEATURE
            ret = m_handleNV21CaptureFrame_unipipe(frame, leaderPipeId, pipeId_next, nodePipeId);
            if (ret != NO_ERROR)
                return ret;
#endif /* SAMSUNG_TN_FEATURE */

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

#ifdef SAMSUNG_TN_FEATURE
    if (leaderPipeId == PIPE_PP_UNI_REPROCESSING
        || leaderPipeId == PIPE_PP_UNI_REPROCESSING2) {
        frame->getDstRect(leaderPipeId, &srcRect);
    } else
#endif
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
#ifdef SAMSUNG_TN_FEATURE
    flagUsePostVC0 = BAYER_NEED_MAP_TYPE;
#endif

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
    } else {
        m_parameters[m_cameraId]->getSize(HW_INFO_MAX_PREVIEW_SIZE, (uint32_t *)&maxPreviewW, (uint32_t *)&maxPreviewH);
        CLOGI("PreviewSize(Max) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    }

    dsWidth = MAX_VRA_INPUT_WIDTH;
    dsHeight = MAX_VRA_INPUT_HEIGHT;
    CLOGI("DS width x height = %dx%d", dsWidth, dsHeight);

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST)
        flagUsePostVC0 = true;
#endif

    /* FLITE */
    bufTag = initBufTag;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        bufTag.pipeId[0] = PIPE_VC0;
        bufTag.pipeId[1] = PIPE_3AC;
        bufTag.pipeId[2] = PIPE_ISPC;
    } else {
        if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true
#ifdef SAMSUNG_FACTORY_DRAM_TEST
                || m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST
#endif
                || flagFlite3aaM2M == true) {
            pipeId = PIPE_VC0;
        } else {
            pipeId = PIPE_3AC;
        }
        bufTag.pipeId[0] = pipeId;
        if (flagFlite3aaM2M == true) {
            bufTag.pipeId[1] = PIPE_3AA;
        }
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

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    /* increase temporarily for dram test */
    if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST) {
        maxBufferCount += 4;
    }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
        maxBufferCount += 1;
    }
#endif

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
    /*
    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        bufConfig.size[0] = bufConfig.size[0] * 2;
        //bufConfig.size[0] = ALIGN_UP(bufConfig.size[0], 4096);
    }
    */
#endif
    bufConfig.reqBufCount = maxBufferCount;
    bufConfig.allowedMaxBufCount = maxBufferCount;
    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    bufConfig.createMetaPlane = true;
#ifdef DEBUG_RAWDUMP
    bufConfig.needMmap = true;
#else
    if (flagUsePostVC0) {
        bufConfig.needMmap = true;
    } else if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        bufConfig.needMmap = true;
    } else {
        bufConfig.needMmap = false;
    }
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
    /*
    if (flagFlite3aaM2M == true) {
        bufTag.pipeId[0] = PIPE_FLITE;
    } else {
        bufTag.pipeId[0] = PIPE_3AA;
    }
    */
    bufTag.pipeId[0] = PIPE_FLITE;
    bufTag.pipeId[1] = PIPE_3AA;
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
            || (m_parameters[m_cameraId]->getLLSOn() == true) ){
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

#if 0
    /* ISPC */
    bufTag = initBufTag;
    bufTag.pipeId[0] = PIPE_ISPC;
    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

    ret = m_bufferSupplier->createBufferManager("ISPC_BUF", m_ionAllocator, bufTag);
    if (ret != NO_ERROR) {
        CLOGE("Failed to create ISPC_BUF. ret %d", ret);
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
    bufConfig.needMmap = true;
    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;

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
#ifdef SAMSUNG_SW_VDIS
        && m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == false
#endif
#ifdef SAMSUNG_HYPERLAPSE
        && m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == false
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        && m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == false
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
        && m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == false
#endif
        ) {
        ExynosRect videoRect;
        int videoFormat = m_configurations->getYuvFormat(videoOutputPortId);
        camera_pixel_size videoPixelSize = m_configurations->getYuvPixelSize(videoOutputPortId);
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
        getYuvPlaneSize(videoFormat, bufConfig.size, videoRect.w, videoRect.h, videoPixelSize);
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

    if ((m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) ||
        (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_VRA) == HW_CONNECTION_MODE_M2M)) {
        /* VRA */
        bufTag = initBufTag;

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            bufTag.pipeId[0] = PIPE_MCSC5;
            maxBufferCount = m_exynosconfig->current->bufInfo.num_vra_buffers;
        } else {
            //maxBufferCount; /* same as 3AA buf count */
            bufTag.pipeId[0] = PIPE_3AF;
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            bufTag.pipeId[1] = PIPE_MCSC5_REPROCESSING;
        } else {
            bufTag.pipeId[1] = PIPE_3AF_REPROCESSING;
        }

        bufTag.pipeId[2] = PIPE_VRA;
        bufTag.pipeId[3] = PIPE_VRA_REPROCESSING;

        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("VRA_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create VRA_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        switch (m_parameters[m_cameraId]->getHwVraInputFormat()) {
            case V4L2_PIX_FMT_NV16M:
            case V4L2_PIX_FMT_NV61M:
                bufConfig.planeCount = 3;
                bufConfig.bytesPerLine[0] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN);
                bufConfig.bytesPerLine[1] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN);
                bufConfig.size[0] = bufConfig.bytesPerLine[0] * dsHeight;
                bufConfig.size[1] = bufConfig.bytesPerLine[1] * dsHeight;

                break;
            case V4L2_PIX_FMT_NV16:
            case V4L2_PIX_FMT_NV61:
                bufConfig.planeCount = 2;
                bufConfig.bytesPerLine[0] = ALIGN_UP(dsWidth, CAMERA_16PX_ALIGN);
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
                camera_pixel_size pixelSize = m_configurations->getYuvPixelSize(previewOutputPortId);
                m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewRect.w, (uint32_t *)&previewRect.h, previewOutputPortId);

#if defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
                m_setBuffers_dualZoomPreview(previewRect);
#endif

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
                getYuvPlaneSize(previewFormat, bufConfig.size, previewRect.w, previewRect.h, pixelSize);
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

#ifdef SAMSUNG_SW_VDIS
            if (m_setupBatchFactoryBuffers_swVdis(frame, factory, streamId, leaderPipeId, bufTag) != NO_ERROR)
                continue;
#endif

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
#ifdef SAMSUNG_SW_VDIS
                if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
                    if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_VIDEO) {
                        int index = frame->getPPScenarioIndex(PP_SCENARIO_SW_VDIS);
                        leaderPipeId = PIPE_PP_UNI + index;
                        bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    } else {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    }
                    break;
                }
#endif
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
#ifdef SAMSUNG_SW_VDIS
            || frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
#endif
#ifdef SAMSUNG_HYPERLAPSE
            || frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true
#endif
#ifdef SAMSUNG_HIFI_VIDEO
            || frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
            || frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true
#endif
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

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
                if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
                    while (m_hifiVideoBufferCount < NUM_HIFIVIDEO_EXTRA_INTERNAL_BUFFERS) {
                        ret = m_bufferSupplier->getBuffer(bufTag, &internalBuffer);
                        if (ret != NO_ERROR || internalBuffer.index < 0) {
                            CLOGE("Get Internal Buffers: Failed to getBuffer(%d). ret %d", internalBuffer.index, ret);
                            continue;
                        }
                        m_hifiVideoBufferQ->pushProcessQ(&internalBuffer);
                        m_hifiVideoBufferCount++;
                    }
                }
#endif

                ret = m_bufferSupplier->getBuffer(bufTag, &internalBuffer);
                if (ret != NO_ERROR || internalBuffer.index < 0) {
                    CLOGE("[R%d F%d T%d]Set Internal Buffers: Failed to getBuffer(%d). ret %d",
                            request->getKey(), frame->getFrameCount(), frame->getFrameType(), internalBuffer.index, ret);
#ifdef USE_DUAL_CAMERA
                    if (leaderPipeId == PIPE_FUSION) {
                        frame->setRequest(PIPE_FUSION, false);
                    }
#endif
                } else {
                    nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);
                    ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
                    if (ret != NO_ERROR) {
                        CLOGE("[R%d F%d T%d] Set Internal Buffers: Failed to setDstBufferState. ret %d",
                                request->getKey(), frame->getFrameCount(), frame->getFrameType(), ret);
                    }

                    ret = frame->setDstBuffer(leaderPipeId, internalBuffer, nodeType);
                    if (ret != NO_ERROR) {
                        CLOGE("[R%d F%d T%d] Set Internal Buffers: Failed to setDstBuffer. ret %d",
                                request->getKey(), frame->getFrameCount(), frame->getFrameType(), ret);
                    }
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
#ifdef SAMSUNG_SW_VDIS
                if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_SW_VDIS);
                    leaderPipeId = PIPE_PP_UNI + index;
                    bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    break;
                }
#endif
#ifdef SAMSUNG_HYPERLAPSE
                if (frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_HYPERLAPSE);
                    leaderPipeId = PIPE_PP_UNI + index;
                    bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    break;
                }
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
                if (frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_VIDEO_BEAUTY);
                    if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                        || m_flagVideoStreamPriority == true) {
                        leaderPipeId = PIPE_PP_UNI + index;
                        bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    } else {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    }
                    break;
                }
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_HIFI_VIDEO);
                    if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                        || m_flagVideoStreamPriority == true) {
                        leaderPipeId = PIPE_PP_UNI + index;
                        bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    } else {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    }
                    break;
                }
#endif

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
                // pass through

            case HAL_STREAM_ID_PREVIEW:
#ifdef SAMSUNG_SW_VDIS
                if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
                    leaderPipeId = PIPE_GSC;
                    bufTag.pipeId[0] = PIPE_GSC;
                    break;
                }
#endif
#ifdef SAMSUNG_HYPERLAPSE
                if (frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true) {
                    leaderPipeId = PIPE_GSC;
                    bufTag.pipeId[0] = PIPE_GSC;
                    break;
                }
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY
                if (frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_VIDEO_BEAUTY);
                    if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                        || m_flagVideoStreamPriority == true) {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    } else {
                        leaderPipeId = PIPE_PP_UNI + index;
                        bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    }
                    break;
                }
#endif
#ifdef SAMSUNG_HIFI_VIDEO
                if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                    int index = frame->getPPScenarioIndex(PP_SCENARIO_HIFI_VIDEO);
                    if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                        || m_flagVideoStreamPriority == true) {
                        leaderPipeId = PIPE_GSC;
                        bufTag.pipeId[0] = PIPE_GSC;
                    } else {
                        leaderPipeId = PIPE_PP_UNI + index;
                        bufTag.pipeId[0] = PIPE_PP_UNI + index;
                    }
                    break;
                }
#endif

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

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    int currentLDCaptureStep = (int)frame->getFrameSpecialCaptureStep();
    int ldCaptureCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
    int ldCaptureMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE);
    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
        CLOGD("currentLDCaptureStep(%d), ldCaptureCount(%d), ldCaptureMode(%d)",
            currentLDCaptureStep, ldCaptureCount, ldCaptureMode);
    }
#endif

    /* Set Internal Buffers */
    if ((frame->getRequest(yuvStallPipeId) == true)
        && (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) == false
#ifdef SAMSUNG_HIFI_CAPTURE
        || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
        || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_LL_HDR
#endif
#endif
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
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                            if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
                                && (ldCaptureMode != MULTI_SHOT_MODE_NONE)) {
                                leaderPipeId = -1;
                                bufTag.pipeId[0] = -1;
                            } else
#endif
                            {
                                leaderPipeId = PIPE_JPEG_REPROCESSING;
                                bufTag.pipeId[0] = PIPE_JPEG_REPROCESSING;
                            }
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
#ifdef SAMSUNG_HIFI_CAPTURE
                    if ((frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
                        || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                        || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_LL_HDR
#endif
                        ) && frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) == true) {
                        leaderPipeId = PIPE_PP_UNI_REPROCESSING;
                        bufTag.pipeId[0] = leaderPipeId;
                    } else
#endif
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
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                        if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
                            && (currentLDCaptureStep != ldCaptureCount)) {
                            leaderPipeId = -1;
                            bufTag.pipeId[0] = -1;
                        } else
#endif
                        {
                            leaderPipeId = PIPE_GSC_REPROCESSING2;
                            bufTag.pipeId[0] = PIPE_GSC_REPROCESSING2;
                        }
                    }
                    break;
                case HAL_STREAM_ID_RAW:
                    flagRawOutput = true;
                    leaderPipeId = PIPE_3AA_REPROCESSING;
                    if (components.parameters->isUse3aaDNG())
                        bufTag.pipeId[0] = PIPE_3AG_REPROCESSING;
                    else
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
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
        if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
            minBufferCount = 3;
        } else
#endif
        {
            minBufferCount = 2;
        }
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
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
        if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
            minBufferCount = 3;
        } else
#endif
        {
            minBufferCount = 2;
        }
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

#ifdef SAMSUNG_TN_FEATURE
    ret = m_setReprocessingBuffer_tn(maxPictureW, maxPictureH);
    if (ret != NO_ERROR)
        return ret;
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

#ifdef SAMSUNG_TN_FEATURE
    m_configurations->getSize(CONFIGURATION_DS_YUV_STALL_SIZE, (uint32_t *)&inputSizeW, (uint32_t *)&inputSizeH);
    m_configurations->getSize(CONFIGURATION_THUMBNAIL_CB_SIZE, (uint32_t *)&thumbnailW, (uint32_t *)&thumbnailH);
#endif
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
                                    skipFlag, frame->getStreamTimestamp(),
                                    frame->getParameters());
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

status_t ExynosCamera::m_updateYsumValue(ExynosCameraFrameSP_sptr_t frame,
                                         ExynosCameraRequestSP_sprt_t request,
                                         int srcPipeId, NODE_TYPE dstNodeType)
{
    int ret = NO_ERROR;
    struct camera2_shot_ext *shotExt = NULL;
    struct camera2_stream *stream = NULL;
    ExynosCameraBuffer buffer;
    entity_buffer_state_t bufState;
    uint32_t yuvSizeW = 0, yuvSizeH = 0, yuvSizeWxH = 0;
    uint32_t videoSizeW = 0, videoSizeH = 0, videoSizeWxH = 0;
    uint64_t ysumValue = 0, scaledYsumValue = 0;

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

    if (request == NULL) {
        CLOGE("Request is NULL");
        return BAD_VALUE;
    }

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    if (srcPipeId == PIPE_FUSION) {
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW
            || frame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE) {
            dstNodeType = OUTPUT_NODE_1;
        } else {
            if (m_configurations->getDualSelectedCam() == UNI_PLUGIN_CAMERA_TYPE_WIDE) {
                dstNodeType = OUTPUT_NODE_1;
            } else {
                dstNodeType = OUTPUT_NODE_2;
            }
        }

        ret = frame->getSrcBuffer(srcPipeId, &buffer, dstNodeType);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, srcPipeId, ret);
            return ret;
        }
    } else
#endif
    {
        ret = frame->getDstBuffer(srcPipeId, &buffer, dstNodeType);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getDstBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, srcPipeId, ret);
            return ret;
        }
    }

    ret = frame->getDstBufferState(srcPipeId, &bufState, dstNodeType);
    if (bufState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("[F%d]buffer state is ERROR", frame->getFrameCount());
        goto skip;
    }

    stream = (struct camera2_stream *)buffer.addr[buffer.getMetaPlaneIndex()];
    shotExt = (struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];

    yuvSizeW = stream->output_crop_region[2];
    yuvSizeH = stream->output_crop_region[3];
    yuvSizeWxH = yuvSizeW * yuvSizeH;

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, &videoSizeW, &videoSizeH);
    videoSizeWxH = videoSizeW * videoSizeH;

    ysumValue = (((uint64_t)shotExt->shot.udm.scaler.ysumdata.higher_ysum_value << 32) & 0xFFFFFFFF00000000)
                | ((uint64_t)shotExt->shot.udm.scaler.ysumdata.lower_ysum_value);
    scaledYsumValue = (double)ysumValue / ((double)videoSizeWxH / (double)yuvSizeWxH);

    CLOGV("[F%d R%d]scaledYSUM value: %jd -> %jd",
            frame->getFrameCount(), request->getKey(),
            ysumValue, scaledYsumValue);

    shotExt->shot.udm.scaler.ysumdata.higher_ysum_value = ((scaledYsumValue >> 32) & 0x00000000FFFFFFFF);
    shotExt->shot.udm.scaler.ysumdata.lower_ysum_value = (scaledYsumValue & 0x00000000FFFFFFFF);

    CLOGV("[F%d R%d]srcPipeId(%d), yuvSizeW(%d), yuvSizeH(%d), videoSizeW(%d), videoSizeH(%d), scaleRatio(%lf)",
            frame->getFrameCount(), request->getKey(),
            srcPipeId, yuvSizeW, yuvSizeH, videoSizeW, videoSizeH, ((double)videoSizeWxH / (double)yuvSizeWxH));

    request->setYsumValue(&shotExt->shot.udm.scaler.ysumdata);

skip:
    return ret;
}

/********************************************************************************/
/**                          VENDOR                                            **/
/********************************************************************************/
void ExynosCamera::m_vendorSpecificPreConstructorInitalize(__unused int cameraId, __unused int scenario)
{
#if defined(SAMSUNG_READ_ROM)
    m_readRomThread = NULL;
    if (isEEprom(getCameraId()) == true) {
        m_readRomThread = new mainCameraThread(this, &ExynosCamera::m_readRomThreadFunc,
                                              "camerareadromThread", PRIORITY_URGENT_DISPLAY);
        if (m_readRomThread != NULL) {
            m_readRomThread->run();
            CLOGD("m_readRomThread starts");
        } else {
            CLOGE("failed the m_readRomThread");
        }
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_fusionZoomPreviewWrapper = NULL;
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    m_fusionZoomCaptureWrapper = NULL;
#endif
    m_previewSolutionHandle = NULL;
    m_captureSolutionHandle = NULL;

    if (scenario == SCENARIO_DUAL_REAR_ZOOM) {
        m_fusionZoomPreviewWrapper = ExynosCameraSingleton<ExynosCameraFusionZoomPreviewWrapper>::getInstance();
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        m_fusionZoomCaptureWrapper = ExynosCameraSingleton<ExynosCameraFusionZoomCaptureWrapper>::getInstance();
#endif
    }
#endif  /* SAMSUNG_DUAL_ZOOM_PREVIEW */

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_bokehPreviewWrapper = NULL;
    m_bokehCaptureWrapper = NULL;
    m_bokehPreviewSolutionHandle = NULL;
    m_bokehCaptureSolutionHandle = NULL;

    if (scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        m_bokehPreviewWrapper = ExynosCameraSingleton<ExynosCameraBokehPreviewWrapper>::getInstance();
        m_bokehCaptureWrapper = ExynosCameraSingleton<ExynosCameraBokehCaptureWrapper>::getInstance();
    }
#endif  /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#ifdef SAMSUNG_TN_FEATURE
    for (int i = 0; i < PP_SCENARIO_MAX; i++) {
        m_uniPlugInHandle[i] = NULL;
    }
    m_uniPluginThread = NULL;
#endif
}

void ExynosCamera::m_vendorSpecificConstructorInitalize(void)
{

#ifdef SAMSUNG_UNI_API
    uni_api_init();
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
#ifdef SAMSUNG_HRM
    m_uv_rayHandle = NULL;
    m_configurations->setMode(CONFIGURATION_HRM_MODE, false);
#endif
#ifdef SAMSUNG_LIGHT_IR
    m_light_irHandle = NULL;
    m_configurations->setMode(CONFIGURATION_LIGHT_IR_MODE, false);
#endif
#ifdef SAMSUNG_GYRO
    m_gyroHandle = NULL;
    m_configurations->setMode(CONFIGURATION_GYRO_MODE, false);
#endif
#ifdef SAMSUNG_ACCELEROMETER
    m_accelerometerHandle = NULL;
    m_configurations->setMode(CONFIGURATION_ACCELEROMETER_MODE, false);
#endif
#ifdef SAMSUNG_ROTATION
    m_rotationHandle = NULL;
    m_configurations->setMode(CONFIGURATION_ROTATION_MODE, false);
#endif
#ifdef SAMSUNG_PROX_FLICKER
    m_proximityHandle = NULL;
    m_configurations->setMode(CONFIGURATION_PROX_FLICKER_MODE, false);
#endif
#endif

#ifdef OIS_CAPTURE
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_activityControl[m_cameraId]->getSpecialCaptureMgr();
    sCaptureMgr->resetOISCaptureFcount();

#ifdef USE_DUAL_CAMERA
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        || m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        ExynosCameraActivitySpecialCapture *sCaptureMgr = m_activityControl[m_cameraIds[1]]->getSpecialCaptureMgr();
        sCaptureMgr->resetOISCaptureFcount();
    }
#endif
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    m_slaveJpegDoneQ = new frame_queue_t;
    m_slaveJpegDoneQ->setWaitTime(1000000000);
#endif

#ifdef SAMSUNG_TN_FEATURE
    m_dscaledYuvStallPPCbQ = new frame_queue_t;
    m_dscaledYuvStallPPCbQ->setWaitTime(1000000000);

    m_dscaledYuvStallPPPostCbQ = new frame_queue_t;
    m_dscaledYuvStallPPPostCbQ->setWaitTime(1000000000);
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    m_postVC0Q = new vcbuffer_queue_t;
    m_postVC0Q->setWaitTime(1000000000);
#endif

#ifdef SAMSUNG_TN_FEATURE
    for (int i = PIPE_PP_VENDOR; i < MAX_PIPE_NUM; i++) {
        m_pipeFrameDoneQ[i] = new frame_queue_t;
    }

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        m_pipePPReprocessingStart[i] = false;
        m_pipePPPreviewStart[i] = false;
    }

    m_pipeFrameDoneQ[PIPE_GSC] = new frame_queue_t;
#endif

#ifdef SAMSUNG_EVENT_DRIVEN
    m_eventDrivenToggle = 0;
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
    m_fastEntrance = false;
    m_isFirstParametersSet = false;
    m_fastenAeThreadResult = 0;
#endif

#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif

#ifdef SAMSUNG_DOF
    m_isFocusSet = false;
#endif

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    m_hifiVideoBufferQ = new buffer_queue_t;
    m_hifiVideoBufferQ->setWaitTime(1000000000);
    m_hifiVideoBufferCount = 0;
#endif

#ifdef DEBUG_IQ_OSD
    m_isOSDMode = -1;
    if (access("/data/camera/debug_iq_osd.txt", F_OK) == 0) {
        CLOGD(" debug_iq_osd.txt access successful");
        m_isOSDMode = 1;
    }
#endif

#ifdef SAMSUNG_SSM
    m_SSMState = SSM_STATE_NONE;
    m_SSMRecordingtime = 0;
    m_SSMOrgRecordingtime = 0;
    m_SSMSkipToggle = 0;
    m_SSMRecordingToggle = 0;
    m_SSMMode = -1;
    m_SSMCommand = -1;
    m_SSMFirstRecordingRequest = 0;
    m_SSMDetectDurationTime = 0;
    m_checkRegister = false;

    m_SSMSaveBufferQ = new buffer_queue_t;
    m_SSMSaveBufferQ->setWaitTime(1000000000);

    if (AUTO_QUEUE_DETECT_COUNT_HD > 0) {
        m_SSMAutoDelay = (int)(AUTO_QUEUE_DETECT_COUNT_HD / (960 / SSM_PREVIEW_FPS));
        m_SSMAutoDelayQ = new frame_queue_t;
        m_SSMAutoDelayQ->setWaitTime(1000000000);
        m_SSMUseAutoDelayQ = true;
    } else {
        m_SSMAutoDelay = 0;
        m_SSMAutoDelayQ = NULL;
        m_SSMUseAutoDelayQ = false;
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_prevRecomCamType = 0;
    m_2x_btn = false;
    m_holdZoomRatio = 1.0f;
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
    m_hifiLLSPowerOn = false;
#endif

}

void ExynosCamera::m_vendorSpecificPreDestructorDeinitalize(void)
{
    __unused status_t ret = 0;

#if defined(SAMSUNG_READ_ROM)
    if (isEEprom(getCameraId()) == true) {
        if (m_readRomThread != NULL) {
            CLOGI("wait m_readRomThread");
            m_readRomThread->requestExitAndWait();
        } else {
            CLOGI("m_readRomThread is NULL");
        }
    }
#endif

#ifdef SAMSUNG_UNIPLUGIN
    if (m_uniPluginThread != NULL) {
        m_uniPluginThread->requestExitAndWait();
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_fusionZoomPreviewWrapper != NULL) {
        if (m_fusionZoomPreviewWrapper->m_getIsInit() == true) {
            m_fusionZoomPreviewWrapper->m_deinitDualSolution(getCameraId());
        }
        m_fusionZoomPreviewWrapper->m_setSolutionHandle(NULL, NULL);
        m_fusionZoomPreviewWrapper = NULL;
    }
    if (m_previewSolutionHandle != NULL) {
        ret = uni_plugin_unload(&m_previewSolutionHandle);
        if (ret < 0) {
            CLOGE("[Fusion]DUAL_PREVIEW plugin unload failed!!");
        }
        m_previewSolutionHandle = NULL;
    }

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_fusionZoomCaptureWrapper != NULL) {
        if (m_fusionZoomCaptureWrapper->m_getIsInit() == true) {
            m_fusionZoomCaptureWrapper->m_deinitDualSolution(getCameraId());
        }
        m_fusionZoomCaptureWrapper->m_setSolutionHandle(NULL, NULL);
        m_fusionZoomCaptureWrapper = NULL;
    }
    if (m_captureSolutionHandle != NULL) {
        ret = uni_plugin_unload(&m_captureSolutionHandle);
        if (ret < 0) {
            CLOGE("[Fusion]DUAL_CAPTURE plugin unload failed!!");
        }
        m_captureSolutionHandle = NULL;
    }
#endif /* SAMSUNG_DUAL_ZOOM_CAPTURE */
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    if (m_bokehPreviewWrapper != NULL) {
        if (m_bokehPreviewWrapper->m_getIsInit() == true) {
            m_bokehPreviewWrapper->m_deinitDualSolution(getCameraId());
        }
        m_bokehPreviewWrapper->m_setSolutionHandle(NULL, NULL);
        m_bokehPreviewWrapper = NULL;
    }
    if (m_bokehPreviewSolutionHandle != NULL) {
        ret = uni_plugin_unload(&m_bokehPreviewSolutionHandle);
        if (ret < 0) {
            CLOGE("[Bokeh] PREVIEW plugin unload failed!!");
        }
        m_bokehPreviewSolutionHandle = NULL;
    }

    if (m_bokehCaptureWrapper != NULL) {
        if (m_bokehCaptureWrapper->m_getIsInit() == true) {
            m_bokehCaptureWrapper->m_deinitDualSolution(getCameraId());
        }
        m_bokehCaptureWrapper->m_setSolutionHandle(NULL, NULL);
        m_bokehCaptureWrapper = NULL;
    }
    if (m_bokehCaptureSolutionHandle != NULL) {
        ret = uni_plugin_unload(&m_bokehCaptureSolutionHandle);
        if (ret < 0) {
            CLOGE("[Bokeh] Capture plugin unload failed!!");
        }
        m_bokehCaptureSolutionHandle = NULL;
    }
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerThread != NULL) {
        m_sensorListenerThread->join();
    }

    if (m_sensorListenerUnloadThread != NULL) {
        m_sensorListenerUnloadThread->run();
    }
#endif

}

void ExynosCamera::m_vendorUpdateExposureTime(__unused struct camera2_shot_ext *shot_ext)
{
#ifdef SAMSUNG_TN_FEATURE
        if (m_configurations->getSamsungCamera() == true
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
            && shot_ext->shot.ctl.aa.vendor_captureExposureTime <= (uint32_t)(PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX)
#endif
            && shot_ext->shot.ctl.aa.vendor_captureExposureTime > (uint32_t)(CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT)) {
            shot_ext->shot.ctl.sensor.exposureTime = (uint64_t)(shot_ext->shot.ctl.aa.vendor_captureExposureTime) * 1000;

            CLOGV("change sensor.exposureTime(%lld) for EXPOSURE_DYNAMIC_SHOT",
                    shot_ext->shot.ctl.sensor.exposureTime);
        }
#endif
}

status_t ExynosCamera::m_checkStreamInfo_vendor(status_t &ret)
{

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST) {
        if (m_cameraId != CAMERA_ID_BACK) {
            CLOGE("FACTORY DRAM TEST support only back camera");
            ret = BAD_VALUE;
        } else if (m_streamManager->getYuvStreamCount() != 1
                    || m_streamManager->getYuvStallStreamCount() != 0) {
            CLOGE("FACTORY DRAM TEST uses only 1 preview stream");
            ret = BAD_VALUE;
        } else if (m_parameters[m_cameraId]->getFactoryDramTestCount() == 0) {
            CLOGE("DRAM TEST Count should be higher than 0");
            ret = BAD_VALUE;
        }
    }
#endif

    return ret;

}

status_t ExynosCamera::setParameters_vendor(__unused const CameraParameters& params)
{
    status_t ret = NO_ERROR;

#ifdef CAMERA_FAST_ENTRANCE_V1
    if (m_isFirstParametersSet == false) {
        CLOGD("1st setParameters");
        m_isFirstParametersSet = true;

        if (m_parameters[m_cameraId]->checkFastenAeStableEnable()
#ifdef USE_DUAL_CAMERA
            && m_scenario != SCENARIO_DUAL_REAR_PORTRAIT
#endif
            && (m_configurations->getSamsungCamera() == true || m_configurations->getFastEntrance() == true)) {
#ifdef SAMSUNG_READ_ROM
            m_waitReadRomThreadEnd();
#endif
            m_fastEntrance = true;
            m_previewFactoryLock.lock();
            m_fastenAeThread->run();
        }
    }
#endif

#ifdef SAMSUNG_UNIPLUGIN
    if (m_configurations->getFastEntrance() == false) {
        int shotmode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);
        if (m_scenario != SCENARIO_SECURE
            && shotmode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS
            && shotmode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_FACE_LOCK
            && shotmode != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_INTELLIGENT_SCAN) {
            if (m_uniPluginThread == NULL) {
                m_uniPluginThread = new mainCameraThread(this, &ExynosCamera::m_uniPluginThreadFunc,
                                                        "uniPluginThread");
                if (m_uniPluginThread != NULL) {
                    m_uniPluginThread->run();
                    CLOGD("m_uniPluginThread starts");
                } else {
                    CLOGE("failed the m_uniPluginThread");
                }
            }
        }
    }
#endif
    return ret;
}

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_checkMultiCaptureMode_vendor_captureHint(ExynosCameraRequestSP_sprt_t request, CameraMetadata *meta)
{
    camera_metadata_entry_t entry;
    entry = meta->find(SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT);
    switch (entry.data.i32[0]) {
    case SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_BURST:
        m_currentMultiCaptureMode = MULTI_CAPTURE_MODE_BURST;
        m_lastMultiCaptureServiceRequest = request->getKey();
        if (m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET) == BURSTSHOT_OFF_FPS) {
            entry = meta->find(SAMSUNG_ANDROID_CONTROL_BURST_SHOT_FPS);
            m_configurations->setModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET, entry.data.i32[0]);
            CLOGD("BurstCapture start. key %d, getBurstShotTargetFps %d getMultiCaptureMode (%d)",
                    request->getKey(), m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET),
                    m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE));
        }
        break;
    case SAMSUNG_ANDROID_CONTROL_CAPTURE_HINT_AGIF:
        m_currentMultiCaptureMode = MULTI_CAPTURE_MODE_AGIF;
        m_lastMultiCaptureServiceRequest = request->getKey();
        if (m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET) == BURSTSHOT_OFF_FPS) {
            entry = meta->find(SAMSUNG_ANDROID_CONTROL_BURST_SHOT_FPS);
            m_configurations->setModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET, entry.data.i32[0]);
            CLOGD("AGIFCapture start. key %d, getBurstShotTargetFps %d getMultiCaptureMode (%d)",
                    request->getKey(), m_configurations->getModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET),
                    m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE));
        }
        break;
    default:
        m_currentMultiCaptureMode = MULTI_CAPTURE_MODE_NONE;
        break;
    }

    return NO_ERROR;
}
#endif

status_t ExynosCamera::m_checkMultiCaptureMode_vendor_update(__unused ExynosCameraRequestSP_sprt_t request)
{

#ifdef SAMSUNG_TN_FEATURE
    if (m_currentMultiCaptureMode != m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE)
        && m_doneMultiCaptureRequest >= 0) {
        bool stopFlag = false;
        if (m_lastMultiCaptureServiceRequest == m_lastMultiCaptureSkipRequest) {
            if (m_doneMultiCaptureRequest >= m_lastMultiCaptureNormalRequest) {
                stopFlag = true;
            }
        } else {
            if (m_doneMultiCaptureRequest >= m_lastMultiCaptureServiceRequest) {
                stopFlag = true;
            }
        }

        if (stopFlag) {
            switch (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE)) {
#ifdef SAMSUNG_TN_FEATURE
            case MULTI_CAPTURE_MODE_BURST:
            case MULTI_CAPTURE_MODE_AGIF:
#ifdef OIS_CAPTURE
                if (m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE) == true) {
                    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_activityControl[m_cameraId]->getSpecialCaptureMgr();
                    /* setMultiCaptureMode flag will be disabled in SCAPTURE_STEP_END in case of OIS capture */
#ifdef USE_DUAL_CAMERA
                    enum DUAL_OPERATION_MODE operationMode = DUAL_OPERATION_MODE_NONE;
                    operationMode = m_configurations->getDualOperationModeReprocessing();
                    if (operationMode == DUAL_OPERATION_MODE_SLAVE) {
                        sCaptureMgr = m_activityControl[m_cameraIds[1]]->getSpecialCaptureMgr();
                    }
#endif
                    sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_WAIT_CAPTURE);
                    m_configurations->setMode(CONFIGURATION_OIS_CAPTURE_MODE, false);
                }
#endif
                m_configurations->setModeValue(CONFIGURATION_BURSTSHOT_FPS_TARGET, BURSTSHOT_OFF_FPS);

                for (int i = 0; i < 4; i++)
                    m_burstFps_history[i] = -1;
                break;
#endif /* SAMSUNG_TN_FEATURE */
            default:
                break;
            }

            CLOGD("[R%d] MultiCapture Mode(%d) request stop. lastRequest(%d), doneRequest(%d) lastskipRequest(%d) lastCaptureRequest(%d)",
                    request->getKey(), m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE),
                    m_lastMultiCaptureServiceRequest, m_doneMultiCaptureRequest,
                    m_lastMultiCaptureSkipRequest, m_lastMultiCaptureNormalRequest);

            m_configurations->setModeValue(CONFIGURATION_MULTI_CAPTURE_MODE, m_currentMultiCaptureMode);
            m_lastMultiCaptureServiceRequest = -1;
            m_lastMultiCaptureSkipRequest = -1;
            m_lastMultiCaptureNormalRequest = -1;
            m_doneMultiCaptureRequest = -1;
#ifdef USE_DUAL_CAMERA
            m_dualMultiCaptureLockflag = false;
#endif
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::processCaptureRequest_vendor_initDualSolutionZoom(__unused camera3_capture_request *request, status_t& ret)
{
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int maxSensorW, maxSensorH;
    int sensorW, sensorH;
    int previewW, previewH;
    int captureW, captureH;
    ExynosRect fusionSrcRect;
    ExynosRect fusionDstRect;
    int *zoomList;
    int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);

    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&maxSensorW, (uint32_t *)&maxSensorH);
    m_parameters[m_cameraId]->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&sensorW, (uint32_t *)&sensorH);
    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&captureW, (uint32_t *)&captureH);
    m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, previewOutputPortId);
    m_parameters[m_cameraId]->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);

    ret = m_readDualCalData(m_fusionZoomPreviewWrapper);
    if (ret != NO_ERROR) {
        CLOGE("[Fusion] DUAL_PREVIEW m_readDualCalData failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    ret = m_fusionZoomPreviewWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH,
                                        sensorW, sensorH, fusionDstRect.w, fusionDstRect.h);
    if (ret != NO_ERROR) {
        CLOGE("[Fusion] DUAL_PREVEIW m_initDualSolution() failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    m_fusionZoomPreviewWrapper->m_setScenario(m_scenario);

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    ret = m_readDualCalData(m_fusionZoomCaptureWrapper);
    if (ret != NO_ERROR) {
        CLOGE("[Fusion] DUAL_CAPTURE m_readDualCalData failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    ret = m_fusionZoomCaptureWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH,
                                                        sensorW, sensorH, captureW, captureH);
    if (ret != NO_ERROR) {
        CLOGE("[Fusion] DUAL_CAPTURE m_initDualSolution() failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    m_fusionZoomCaptureWrapper->m_setScenario(m_scenario);
#endif /* SAMSUNG_DUAL_ZOOM_CAPTURE */
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
    return ret;
}

status_t ExynosCamera::processCaptureRequest_vendor_initDualSolutionPortrait(__unused camera3_capture_request *request, status_t& ret)
{
#if defined(SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    int maxSensorW, maxSensorH;
    int sensorW, sensorH;
    int previewW, previewH;
    int captureW, captureH;
    ExynosRect fusionSrcRect;
    ExynosRect fusionDstRect;
    int *zoomList;
    int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);

    m_parameters[m_cameraId]->getSize(HW_INFO_MAX_SENSOR_SIZE, (uint32_t *)&maxSensorW, (uint32_t *)&maxSensorH);
    m_parameters[m_cameraId]->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&sensorW, (uint32_t *)&sensorH);
    m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, previewOutputPortId);
    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&captureW, (uint32_t *)&captureH);

    ret = m_readDualCalData(m_bokehPreviewWrapper);
    if (ret != NO_ERROR) {
        CLOGE("[Bokeh] PREVEIW m_readDualCalData failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    ret = m_bokehPreviewWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH,
                                        sensorW, sensorH, fusionDstRect.w, fusionDstRect.h);
    if (ret != NO_ERROR) {
        CLOGE("[Bokeh] PREVEIW m_initDualSolution() failed!! m_cameraId(%d)", m_cameraId);
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    m_bokehPreviewWrapper->m_setScenario(m_scenario);

    ret = m_readDualCalData(m_bokehCaptureWrapper);
    if (ret != NO_ERROR) {
        CLOGE("[Bokeh] CAPTURE m_readDualCalData failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    ret = m_bokehCaptureWrapper->m_initDualSolution(m_cameraId, maxSensorW, maxSensorH,
                                        sensorW, sensorH, captureW, captureH);
    if (ret != NO_ERROR) {
        CLOGE("[Bokeh] CAPTURE m_initDualSolution() failed!!");
        m_transitState(EXYNOS_CAMERA_STATE_ERROR);
        ret = NO_INIT;
        return ret;
    }

    m_bokehCaptureWrapper->m_setScenario(m_scenario);
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

    return ret;
}

#ifdef SAMSUNG_TN_FEATURE
bool ExynosCamera::m_isSkipBurstCaptureBuffer_vendor(frame_type_t frameType, int& isSkipBuffer, int& ratio, int& runtime_fps, int& burstshotTargetFps)
{
    if (runtime_fps <= 22) {
        ratio = 0;
    } else if (runtime_fps >= 27) {
        ratio = 3;
    } else {
        ratio = 5;
    }

    m_burstFps_history[3] = m_burstFps_history[2];
    m_burstFps_history[2] = m_burstFps_history[1];
    m_burstFps_history[1] = ratio;

    if ((m_burstFps_history[0] == -1)
        || (m_burstFps_history[1] == m_burstFps_history[2] && m_burstFps_history[1] == m_burstFps_history[3])) {
        m_burstFps_history[0] = m_burstFps_history[1];
    }

    if (ratio == 0 || m_burstFps_history[0] == 0) {
        m_captureResultToggle = 1;
    } else {
        ratio = m_burstFps_history[0];
        m_captureResultToggle = (m_captureResultToggle + 1) % ratio;
    }

    if (m_captureResultToggle == 0) {
        isSkipBuffer = true;
    }

    return true;
}
#endif

status_t ExynosCamera::m_captureFrameHandler_vendor_updateConfigMode(__unused ExynosCameraRequestSP_sprt_t request, __unused ExynosCameraFrameFactory *targetfactory, __unused frame_type_t& frameType)
{
#ifdef SAMSUNG_TN_FEATURE
    m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_DSCALED);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE)) {
        m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        CLOGD("[Fusion] frameType(%d) getFusionCaptureEnable(%d)",
               frameType, (int)m_configurations->getMode(CONFIGURATION_FUSION_CAPTURE_MODE));
    }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
        && m_configurations->getDualOperationModeReprocessing() == DUAL_OPERATION_MODE_SYNC) {
        m_configurations->setModeValue(CONFIGURATION_YUV_STALL_PORT_USAGE, YUV_STALL_USAGE_PICTURE);
        m_configurations->setModeValue(CONFIGURATION_CURRENT_BOKEH_PREVIEW_RESULT,
                                        m_bokehPreviewWrapper->m_getBokehPreviewResultValue());
        CLOGD("[BokehCapture] frameType(%d) set YuvStallPortUsage to USAGE_PICTURE", frameType);
    }
#endif

return NO_ERROR;
}

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_captureFrameHandler_vendor_updateCaptureMode(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components, ExynosCameraActivitySpecialCapture *sCaptureMgr)
{
#ifdef OIS_CAPTURE
    if (getCameraId() == CAMERA_ID_BACK) {
        sCaptureMgr->resetOISCaptureFcount();
        components.parameters->checkOISCaptureMode(m_currentMultiCaptureMode);
        CLOGD("LDC:getOISCaptureModeOn(%d)", m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE));
    }
#endif
    if (m_currentMultiCaptureMode == MULTI_CAPTURE_MODE_NONE) {
#ifdef SAMSUNG_LLS_DEBLUR
        if (isLDCapture(getCameraId()) == true) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            components.parameters->checkLDCaptureMode(m_2x_btn);
#else
            components.parameters->checkLDCaptureMode();
#endif
            CLOGD("LDC: LLS(%d), getLDCaptureMode(%d), getLDCaptureCount(%d)",
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE),
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE),
                m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT));
        }
#endif
        bool hasCaptureSream = false;
        if (request->hasStream(HAL_STREAM_ID_JPEG)
            || request->hasStream(HAL_STREAM_ID_CALLBACK_STALL)) {
            hasCaptureSream = true;
        }
#ifdef SAMSUNG_STR_CAPTURE
        components.parameters->checkSTRCaptureMode(hasCaptureSream);
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
        components.parameters->checkBeautyCaptureMode(hasCaptureSream);
#endif
    }
    return NO_ERROR;
}
#endif

status_t ExynosCamera::m_captureFrameHandler_vendor_updateDualROI(__unused ExynosCameraRequestSP_sprt_t request, __unused frame_handle_components_t& components, __unused frame_type_t frameType)
{
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        ExynosRect mainRoiRect = {0, }, subRoiRect = {0, };
        ExynosRect activeZoomRect = {0, }, zoomRect = {0, };
        UNI_PLUGIN_CAMERA_TYPE uniCamType = \
                        getUniCameraType(m_scenario, components.parameters->getCameraId());
        UTrect cropROI = {0, };

        m_configurations->getSize(CONFIGURATION_PICTURE_SIZE ,(uint32_t *)&mainRoiRect.w, (uint32_t *)&mainRoiRect.h);
        components.parameters->getSize(HW_INFO_HW_PICTURE_SIZE ,(uint32_t *)&subRoiRect.w, (uint32_t *)&subRoiRect.h);
        components.parameters->getActiveZoomRect(&activeZoomRect);
        m_configurations->getZoomRect(&zoomRect);

        m_fusionZoomCaptureWrapper->m_setCameraParam((UniPluginDualCameraParam_t*)m_configurations->getFusionParam());
        m_fusionZoomCaptureWrapper->m_setCurZoomRect(components.parameters->getCameraId(), activeZoomRect);
        m_fusionZoomCaptureWrapper->m_setCurViewRect(components.parameters->getCameraId(), zoomRect);
        m_fusionZoomCaptureWrapper->m_setFrameType(frameType);

        if (components.parameters->useCaptureExtCropROI()) {
            m_fusionZoomCaptureWrapper->m_calRoiRect(components.parameters->getCameraId(), mainRoiRect, subRoiRect);

            CLOGV("[ID%d, FT%d] mainRoiRect(%d x %d), subRoiRect(%d x %d), ActiveZoomRect(%d,%d,%d,%d), ZoomRect(%d,%d,%d,%d)",
                    components.parameters->getCameraId(), frameType,
                    mainRoiRect.w, mainRoiRect.h,
                    subRoiRect.w, subRoiRect.h,
                    activeZoomRect.x, activeZoomRect.y, activeZoomRect.w, activeZoomRect.h,
                    zoomRect.x, zoomRect.y, zoomRect.w, zoomRect.h);

            m_fusionZoomCaptureWrapper->m_getCropROI(uniCamType, &cropROI);
            components.parameters->setCaptureExtCropROI(cropROI);
        }
    }
#endif

    return NO_ERROR;
}

#ifdef OIS_CAPTURE
status_t ExynosCamera::m_captureFrameHandler_vendor_updateIntent(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t& components)
{
    status_t ret = 0;
    ExynosCameraActivitySpecialCapture *sCaptureMgr = NULL;
    sCaptureMgr = components.activityControl->getSpecialCaptureMgr();
    int currentCameraId = components.currentCameraId;

    if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_NONE) {
        unsigned int captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT;
        int value;

#ifdef SAMSUNG_LLS_DEBLUR
        if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE) {
            unsigned int captureCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);
            int ldCaptureLlsValue = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE);
            unsigned int mask = 0;

            if ((ldCaptureLlsValue >= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2
                && ldCaptureLlsValue  <= LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5)
#ifdef SUPPORT_ZSL_MULTIFRAME
                || (ldCaptureLlsValue   >= LLS_LEVEL_MULTI_MERGE_LOW_2
                    && ldCaptureLlsValue    <= LLS_LEVEL_MULTI_MERGE_LOW_5)
#endif
            ) {
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_DYNAMIC_SHOT;
            }
#ifdef SAMSUNG_MFHDR_CAPTURE
                else if (ldCaptureLlsValue >= LLS_LEVEL_MULTI_MFHDR_2
                        && ldCaptureLlsValue <= LLS_LEVEL_MULTI_MFHDR_5) {
                    captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_MFHDR_DYNAMIC_SHOT;
            }
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                else if (ldCaptureLlsValue >= LLS_LEVEL_MULTI_LLHDR_LOW_2
                        && ldCaptureLlsValue <= LLS_LEVEL_MULTI_LLHDR_5) {
                    captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_LLHDR_DYNAMIC_SHOT;
                }
#endif
            else {
                captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_DEBLUR_DYNAMIC_SHOT;
            }
            CLOGD("start set multi mode captureIntent(%d) capturecount(%d)",
                    captureIntent, captureCount);

            mask = (((captureIntent << 16) & 0xFFFF0000) | ((captureCount << 0) & 0x0000FFFF));
            value = mask;
        } else
#endif
        {
            captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_SINGLE;
            value = captureIntent;
        }

        if (components.previewFactory == NULL) {
            CLOGE("FrameFactory is NULL!!");
        } else {
            ret = components.previewFactory->setControl(V4L2_CID_IS_INTENT, value, PIPE_3AA);
            if (ret) {
                CLOGE("setcontrol() failed!!");
            } else {
                CLOGD("setcontrol() V4L2_CID_IS_INTENT:(%d)", value);
            }
        }

        m_currentCaptureShot[currentCameraId]->shot.ctl.aa.captureIntent = (enum aa_capture_intent)captureIntent;
        sCaptureMgr->setCaptureStep(ExynosCameraActivitySpecialCapture::SCAPTURE_STEP_START);
        components.activityControl->setOISCaptureMode(true);
        CLOGD("OISMODE(%d), captureIntent(%d)",
                m_configurations->getMode(CONFIGURATION_OIS_CAPTURE_MODE), value);
    }

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_SW_VDIS
status_t ExynosCamera::configureStreams_vendor_updateVDIS()
{
    status_t ret = 0;
    int portId = m_parameters[m_cameraId]->getPreviewPortId();
    int fps = m_configurations->getModeValue(CONFIGURATION_RECORDING_FPS);
    int videoW = 0, videoH = 0;
    int hwWidth = 0, hwHeight = 0;

    m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);
    m_parameters[m_cameraId]->getSWVdisYuvSize(videoW, videoH, fps, &hwWidth, &hwHeight);

    if (hwWidth == 0 || hwHeight == 0) {
        CLOGE("Not supported VDIS size %dx%d fps %d", videoW, videoH, fps);
        return BAD_VALUE;
    }

    ret = m_parameters[m_cameraId]->checkHwYuvSize(hwWidth, hwHeight, portId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setHwYuvSize for PREVIEW stream(VDIS). size %dx%d outputPortId %d",
                 hwWidth, hwHeight, portId);
        return ret;
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_parameters[m_cameraIds[1]] != NULL) {
        portId = m_parameters[m_cameraIds[1]]->getPreviewPortId();
        ret = m_parameters[m_cameraIds[1]]->checkHwYuvSize(hwWidth, hwHeight, portId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setHwYuvSize for PREVIEW stream(VDIS). size %dx%d outputPortId %d",
                     hwWidth, hwHeight, portId);
            return ret;
        }
    }
#endif
    return NO_ERROR;
}
#endif

status_t ExynosCamera::m_previewFrameHandler_vendor_updateRequest(__unused ExynosCameraFrameFactory *targetfactory)
{
#ifdef SAMSUNG_SW_VDIS
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        targetfactory->setRequest(PIPE_GSC, true);
    }
#endif

#ifdef SAMSUNG_HYPERLAPSE
    if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
        targetfactory->setRequest(PIPE_GSC, true);
    }
#endif

#ifdef SAMSUNG_HIFI_VIDEO
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        targetfactory->setRequest(PIPE_GSC, true);
    }
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY
    if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
        targetfactory->setRequest(PIPE_GSC, true);
    }
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    if (m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST) {
        targetfactory->setRequest(PIPE_VC0, true);
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera::m_handlePreviewFrame_vendor_handleBuffer(__unused ExynosCameraFrameSP_sptr_t frame,
                                                                __unused int pipeId,
                                                                __unused ExynosCameraFrameFactory *factory,
                                                                __unused frame_handle_components_t& components,
                                                                __unused status_t& ret)
{

#if defined (SAMSUNG_SW_VDIS) || defined (SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY) || defined (SAMSUNG_HIFI_VIDEO)
#ifdef USE_DUAL_CAMERA
    if (frame->getRequest(PIPE_SYNC) == false)
#endif
    {
        int pp_scenario = frame->getPPScenario(PIPE_PP_UNI);

        if (pp_scenario == PP_SCENARIO_SW_VDIS
            || pp_scenario == PP_SCENARIO_HYPERLAPSE
            || pp_scenario == PP_SCENARIO_HIFI_VIDEO
            || pp_scenario == PP_SCENARIO_VIDEO_BEAUTY) {
            ExynosCameraBuffer srcBuffer;
            int nodePipeId;
            nodePipeId = PIPE_MCSC0 + components.parameters->getPreviewPortId();

            srcBuffer.index = -2;

            ret = frame->getDstBuffer(pipeId, &srcBuffer, factory->getNodeType(nodePipeId));
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDst buffer, pipeId(%d), ret(%d)", pipeId, ret);
            } else {
                if (srcBuffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(srcBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to putBuffer. pipeId(%d), ret(%d)", pipeId, ret);
                    }
                }
            }
        }
    }
#endif

#ifdef SAMSUNG_FOCUS_PEAKING
    if (frame->getRequest(PIPE_VC1) == true) {
        int pp_scenario = frame->getPPScenario(PIPE_PP_UNI);
        int pp_scenario2 = frame->getPPScenario(PIPE_PP_UNI2);

        if (pp_scenario == PP_SCENARIO_FOCUS_PEAKING || pp_scenario2 == PP_SCENARIO_FOCUS_PEAKING) {
            int pipeId = PIPE_FLITE;
            ExynosCameraBuffer depthMapBuffer;

            if (components.parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
                pipeId = PIPE_3AA;
            }

            depthMapBuffer.index = -2;
            ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
            if (ret != NO_ERROR) {
                CLOGE("Failed to get DepthMap buffer");
            }

            if (depthMapBuffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), depthMapBuffer.index, ret);
                }
            }
        }
    }
#endif
    return ret;
}

void ExynosCamera::m_vendorSpecificDestructorDeinitalize(void)
{

#ifdef SAMSUNG_UNI_API
    uni_api_deinit();
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    m_postVC0Thread->requestExitAndWait();

    if (m_postVC0Q != NULL) {
        delete m_postVC0Q;
        m_postVC0Q = NULL;
    }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    if (m_slaveJpegDoneQ != NULL) {
        delete m_slaveJpegDoneQ;
        m_slaveJpegDoneQ = NULL;
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    m_dscaledYuvStallPostProcessingThread->requestExitAndWait();
    m_gscPreviewCbThread->requestExitAndWait();

    if (m_dscaledYuvStallPPCbQ != NULL) {
        delete m_dscaledYuvStallPPCbQ;
        m_dscaledYuvStallPPCbQ = NULL;
    }

    if (m_dscaledYuvStallPPPostCbQ != NULL) {
        delete m_dscaledYuvStallPPPostCbQ;
        m_dscaledYuvStallPPPostCbQ = NULL;
    }

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    if (m_hifiVideoBufferQ != NULL) {
        delete m_hifiVideoBufferQ;
        m_hifiVideoBufferQ = NULL;
    }
#endif

#ifdef SAMSUNG_SSM
    if (m_SSMAutoDelayQ != NULL) {
        delete m_SSMAutoDelayQ;
        m_SSMAutoDelayQ = NULL;
    }

    if (m_SSMSaveBufferQ != NULL) {
        delete m_SSMSaveBufferQ;
        m_SSMSaveBufferQ = NULL;
    }
#endif
#endif

#ifdef SAMSUNG_LENS_DC
    if (getCameraId() == CAMERA_ID_BACK) {
        if (m_DCpluginHandle != NULL) {
            ret = uni_plugin_unload(&m_DCpluginHandle);
            if (ret < 0) {
                CLOGE("[DC]DC plugin unload failed!!");
            }
        }
    }
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
    if (m_sensorListenerUnloadThread != NULL) {
        m_sensorListenerUnloadThread->join();
    }
#endif

}

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_sensorListenerUnloadThreadFunc(void)
{
    CLOGI("-IN-");

#ifdef SAMSUNG_HRM
    if (m_uv_rayHandle != NULL) {
        if (sensor_listener_disable_sensor(m_uv_rayHandle, ST_UV_RAY) < 0) {
            CLOGE("[HRM]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_uv_rayHandle);
            m_uv_rayHandle = NULL;
        }
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if (m_light_irHandle != NULL) {
        if (sensor_listener_disable_sensor(m_light_irHandle, ST_LIGHT_IR) < 0) {
            CLOGE("[LIGHT_IR]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_light_irHandle);
            m_light_irHandle = NULL;
        }
    }
#endif

#ifdef SAMSUNG_GYRO
    if (m_gyroHandle != NULL) {
        if (sensor_listener_disable_sensor(m_gyroHandle, ST_GYROSCOPE) < 0) {
            CLOGE("[GYRO]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_gyroHandle);
            m_gyroHandle = NULL;
        }
    }
#endif

#ifdef SAMSUNG_ACCELEROMETER
    if (m_accelerometerHandle != NULL) {
        if (sensor_listener_disable_sensor(m_accelerometerHandle, ST_ACCELEROMETER) < 0) {
            CLOGE("[ACCELEROMETER]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_accelerometerHandle);
            m_accelerometerHandle = NULL;
        }
    }
#endif

#ifdef SAMSUNG_ROTATION
    if (m_rotationHandle != NULL) {
        if (sensor_listener_disable_sensor(m_rotationHandle, ST_ROTATION) < 0) {
            CLOGE("[ROTATION]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_rotationHandle);
            m_rotationHandle = NULL;
        }
    }
#endif

#ifdef SAMSUNG_PROX_FLICKER
    if (m_proximityHandle != NULL) {
        if (sensor_listener_disable_sensor(m_proximityHandle, ST_PROXIMITY_FLICKER) < 0) {
            CLOGE("[PROX_FLICKER]sensor_listener_disable_sensor failed!!");
        } else {
            sensor_listener_unload(&m_proximityHandle);
            m_proximityHandle = NULL;
        }
    }
#endif

    CLOGI("-OUT-");

    return false;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
void ExynosCamera::m_updateTemperatureInfo(struct camera2_shot_ext *shot_ext)
{
    uint32_t minFps = 0, maxFps = 0;
    int temperature = 0;
    static int end_count = 0;
    static int current_count = 0;

    if (m_configurations->getTemperature() == 0xFFFF || current_count >= end_count) {
        temperature = readTemperature();
        m_configurations->setTemperature(temperature);
        m_configurations->getPreviewFpsRange(&minFps, &maxFps);
        if (maxFps != 0) {
            end_count = 4000 / (1000/maxFps);
        } else {
            CLOGE("maxFps is 0. Set temperature read count to 100.");
            end_count = 100;
        }
        current_count = 0;
    } else {
        current_count++;
    }

    shot_ext->shot.uctl.aaUd.temperatureInfo.usb_thermal = m_configurations->getTemperature();
}
#endif

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
status_t ExynosCamera::m_updateBeforeForceSwitchSolution(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    const struct camera2_shot_ext *shot_ext = frame->getConstMeta();
    int dispFallbackState = 0;
    int targetFallbackState = 0;
    status_t ret = NO_ERROR;

    if (shot_ext != NULL) {
        int dualOperationMode = m_configurations->getDualOperationMode();
        int targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);
        int selectedCamType = m_configurations->getDualSelectedCam();
        int frameType = frame->getFrameType();
        float viewZoomRatio = m_configurations->getZoomRatio();

        dispFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT);
        targetFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT);

        /* Initialize targetCamType about First Frame */
        if (viewZoomRatio >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
            if (dispFallbackState == true)
                targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            else
                targetCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
        } else {
            targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }

        if (m_configurations->getDynamicMode(DYNAMIC_DUAL_CAMERA_DISABLE) == true) {
            targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }

        /* Initialize dispCamType about First Frame */
        if (dispCamType != UNI_PLUGIN_CAMERA_TYPE_WIDE
            && dispCamType != UNI_PLUGIN_CAMERA_TYPE_TELE
            && dispCamType != UNI_PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE) {
            if (viewZoomRatio >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
                if (dispFallbackState == true)
                    dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                else
                    dispCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            } else {
                dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            }
        }

        if (frameType == FRAME_TYPE_INTERNAL_SLAVE) {
            /* Target Cam */
            m_configurations->checkFallbackCondition(shot_ext, true);

            return NO_ERROR;
        } else if (frameType == FRAME_TYPE_PREVIEW
            || frameType == FRAME_TYPE_PREVIEW_SLAVE
            || frameType == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
            /* Display Cam */
            m_configurations->checkFallbackCondition(shot_ext, false);

            /* reset condition of target in sync and tele */
            if (frameType == FRAME_TYPE_PREVIEW &&
                viewZoomRatio < DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
                m_configurations->setStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT, 0);
                m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
            }
        } else {
            return NO_ERROR;
        }

        if (dispFallbackState == 1) {
            if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                selectedCamType = dispCamType;
            } else {
                if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                    if (targetFallbackState) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    }
                }

                selectedCamType = targetCamType;
            }
        } else {
            if (frameType == FRAME_TYPE_PREVIEW_DUAL_MASTER
                && dispCamType != targetCamType) {
                if (targetFallbackState) {
                    if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    }
                    selectedCamType = dispCamType;
                } else {
                    if (m_configurations->getStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK)) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
                    }
                    selectedCamType = targetCamType;
                }
            } else {
                if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                    if (targetFallbackState) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    } else {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
                    }
                }

                selectedCamType = targetCamType;
            }
        }

        CLOGV("[FT%d]dualOpMode(%d),recommCam(%d),DispCam(%d),selectedCam(%d),dispFall(%d),targetFall(%d),opModeFall(%d)",
                frameType,
                dualOperationMode,
                targetCamType,
                dispCamType,
                selectedCamType,
                m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT),
                m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT),
                m_configurations->getStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK));

        m_configurations->setDualSelectedCam(selectedCamType);
    }

    return ret;
}

status_t ExynosCamera::m_updateAfterForceSwitchSolution(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    int dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    int targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
    int frameType = frame->getFrameType();
    float viewZoomRatio = m_configurations->getZoomRatio();

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
        if (frameType == FRAME_TYPE_PREVIEW_SLAVE) {
            dispCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
        } else {
            dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }
        m_configurations->setModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE, dispCamType);

        if (viewZoomRatio >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
            if (m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT)) {
                targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            } else {
                targetCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            }
        } else {
            targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }
        m_configurations->setModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE, targetCamType);

    }

    return ret;
}

status_t ExynosCamera::m_updateBeforeDualSolution(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
        const struct camera2_shot_ext *shot_ext[2] = {NULL, NULL};
        int selectedCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        int ret = NO_ERROR;
        int curDispNode = OUTPUT_NODE_1;
        int targetNode = OUTPUT_NODE_1;
        int targetCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE);
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);
        int dualOperationMode = m_configurations->getDualOperationMode();
        int recordingMode = m_configurations->getMode(CONFIGURATION_RECORDING_MODE);
        int holdFallbackState = m_configurations->getModeValue(CONFIGURATION_DUAL_HOLD_FALLBACK_STATE);
        float viewZoomRatio = m_configurations->getZoomRatio();
        int dispFallbackState = 0;
        int targetFallbackState = 0;

        /* Initialize targetCamType about First Frame */
        if (targetCamType != UNI_PLUGIN_CAMERA_TYPE_WIDE
            && targetCamType != UNI_PLUGIN_CAMERA_TYPE_TELE
            && targetCamType != UNI_PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE) {
            if (m_configurations->getZoomRatio() >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
                if (m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT))
                    targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                else
                    targetCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            } else {
                targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            }
        }

        if (m_configurations->getDynamicMode(DYNAMIC_DUAL_CAMERA_DISABLE) == true) {
            targetCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }

        /* Initialize dispCamType about First Frame */
        if (dispCamType != UNI_PLUGIN_CAMERA_TYPE_WIDE
            && dispCamType != UNI_PLUGIN_CAMERA_TYPE_TELE
            && dispCamType != UNI_PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE) {
            if (m_configurations->getZoomRatio() >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
                if (m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT))
                    dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
                else
                    dispCamType = UNI_PLUGIN_CAMERA_TYPE_TELE;
            } else {
                dispCamType = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            }
        }

        for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
            if ((frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_MASTER &&
                 frame->getFrameType() != FRAME_TYPE_REPROCESSING_DUAL_MASTER)
                 && (i == OUTPUT_NODE_2))
                break;
            shot_ext[i] = frame->getConstMeta(i);
        }

        CLOGV("FrameType(%d), dispCamType(%d)", frame->getFrameType() , dispCamType);
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
            && dispCamType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            curDispNode = OUTPUT_NODE_2;
            targetNode = OUTPUT_NODE_1;
        } else {
            curDispNode = OUTPUT_NODE_1;
            targetNode = OUTPUT_NODE_2;
        }

        /* Display Cam */
        m_configurations->checkFallbackCondition(shot_ext[curDispNode], false);
        dispFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT);

        /* Target Cam */
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
            m_configurations->checkFallbackCondition(shot_ext[targetNode], true);
            targetFallbackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT);
        } else {
            /* reset target fallback result */
            m_configurations->setStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT, 0);
        }

        if (holdFallbackState) {
            /* For Capture */
            selectedCamType = dispCamType;
        } else if (dispFallbackState == 1) {
            if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                selectedCamType = dispCamType;
            } else {
                if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                    if (targetFallbackState) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    }
                }

                selectedCamType = targetCamType;
            }
        } else {
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
                && dispCamType != targetCamType) {
                if (targetFallbackState) {
                    if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    }
                    selectedCamType = dispCamType;
                } else {
                    if (m_configurations->getStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK)) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
                    }
                    selectedCamType = targetCamType;
                }
            } else {
                if (viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
                    if (targetFallbackState) {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 1);
                    } else {
                        m_configurations->setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
                    }
                }

                selectedCamType = targetCamType;
            }
        }

        CLOGV("dualOpMode(%d),recommCam(%d),DispCam(%d),selectedCam(%d),dispFall(%d),targetFall(%d),opModeFall(%d)",
                dualOperationMode,
                targetCamType,
                dispCamType,
                selectedCamType,
                m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT),
                m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT),
                m_configurations->getStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK));

        m_configurations->setDualSelectedCam(selectedCamType);
#else
        if (m_configurations->getDualOperationMode() == DUAL_OPERATION_MODE_MASTER) {
            m_configurations->setDualSelectedCam(UNI_PLUGIN_CAMERA_TYPE_WIDE);
        } else {
            /* need to check fallBack condition */
            m_configurations->setDualSelectedCam(UNI_PLUGIN_CAMERA_TYPE_TELE);
        }
#endif
    } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            /* update faceDetection */
            struct camera2_shot_ext shot_ext;

            /* master */
            frame->getMetaData(&shot_ext, OUTPUT_NODE_1);
            if (shot_ext.shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
            m_parameters[m_cameraId]->getFaceDetectMeta(&shot_ext);
            frame->storeDynamicMeta(&shot_ext, OUTPUT_NODE_1);
            }

            /* slave */
            frame->getMetaData(&shot_ext, OUTPUT_NODE_2);
            if (shot_ext.shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF) {
            m_parameters[m_cameraIds[1]]->getFaceDetectMeta(&shot_ext);
            frame->storeDynamicMeta(&shot_ext, OUTPUT_NODE_2);
            }
        }
#endif
    }

    return NO_ERROR;
}

status_t ExynosCamera::m_updateAfterDualSolution(ExynosCameraFrameSP_sptr_t frame)
{
    struct camera2_shot_ext shot_ext_1;
    struct camera2_shot_ext shot_ext_2;
    status_t ret = NO_ERROR;

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
        UniPluginDualCameraParam_t *fusionParam = m_fusionZoomPreviewWrapper->m_getCameraParam();
        int fusionIndex = fusionParam->baseCameraType;
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

        /* update Fusion param for every frame. */
        m_configurations->setFusionParam(fusionParam);

        /* set N-1 zoomRatio in Frame*/
        if (m_configurations->getAppliedZoomRatio() < 1.0f) {
            frame->setAppliedZoomRatio(frame->getZoomRatio());
        } else {
            frame->setAppliedZoomRatio(m_configurations->getAppliedZoomRatio());
        }

        m_configurations->setAppliedZoomRatio(fusionParam->mappedUserRatio);

        if (dispCamType == UNI_PLUGIN_CAMERA_TYPE_TELE) {
            frame->setParameters(m_parameters[m_cameraIds[1]]);
        }

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        if (m_configurations->getDualOperationMode()  == DUAL_OPERATION_MODE_SYNC
            && frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {

            frame->getMetaData(&shot_ext_1, OUTPUT_NODE_1);
            frame->getMetaData(&shot_ext_2, OUTPUT_NODE_2);

            m_configurations->checkFusionCaptureCondition(&shot_ext_1, &shot_ext_2);
        }
#endif
    }
    return ret;
}

status_t ExynosCamera::m_readDualCalData(ExynosCameraFusionWrapper *m_fusionWrapper)
{
    unsigned char *buf;
    ExynosCameraFrameFactory *factory;
    struct v4l2_ext_controls extCtrls;
    struct v4l2_ext_control extCtrl;
    FILE *fp = NULL;
    char filePath[80];
    status_t ret = NO_ERROR;
    size_t result = 0;

    memset(filePath, 0, sizeof(filePath));
    sprintf(filePath, "/data/camera/dual_cal.bin");
    m_fusionWrapper->m_getCalBuf(&buf);

    fp = fopen(filePath, "r");
    if (fp == NULL) {
        CLOGD("dual cal file in external path is not exist.");

        factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        memset(&extCtrls, 0x00, sizeof(extCtrls));
        memset(&extCtrl, 0x00, sizeof(extCtrl));
        extCtrls.controls = &extCtrl;

        extCtrl.ptr = buf;
        extCtrl.id = V4L2_CID_IS_GET_DUAL_CAL;
        extCtrls.count = 1;
        extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;

        ret = factory->setExtControl(&extCtrls, PIPE_3AA);
        if (ret) {
            memset(filePath, 0, sizeof(filePath));
            sprintf(filePath, "/vendor/firmware/dual_cal.bin");
            fp = fopen(filePath, "r");
            if (fp == NULL) {
                CLOGD("dual cal fail in internal path is not exist.");
                goto exit;
            } else {
                ret = NO_ERROR;
            }
        } else {
            CLOGD("V4L2_CID_IS_GET_DUAL_CAL success");
            goto exit;
        }
    }

    result = fread(buf, 1, DUAL_CAL_DATA_SIZE, fp);
    if(result < DUAL_CAL_DATA_SIZE)
        ret = BAD_VALUE;

     fclose(fp);

exit:
    return ret;
}

bool ExynosCamera::m_check2XButtonPressed(float curZoomRatio, float prevZoomRatio)
{
    bool ret = false;
    int transientAction = m_configurations->getModeValue(CONFIGURATION_TRANSIENT_ACTION_MODE);
    int recommendCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE);

    if (curZoomRatio - prevZoomRatio >= 1.0f
        && (curZoomRatio < 2.1f)
        && (m_configurations->getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT) == false) /* for blinking issue */
        && (recommendCamType == UNI_PLUGIN_CAMERA_TYPE_WIDE) /* for 2x recording issue */
        && (transientAction == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE)){
        ret = true;
    }

    return ret;
}
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW || SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#if defined(USE_DUAL_CAMERA) && defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
void ExynosCamera::m_updateCropRegion_vendor(struct camera2_shot_ext *shot_ext,
                                                int &sensorMaxW, int &sensorMaxH,
                                                ExynosRect &targetActiveZoomRect,
                                                ExynosRect &subActiveZoomRect,
                                                int &subSolutionMargin)
{
    ExynosRect mainActiveZoomRect = {0, };
    float zoomRatio = 1.0f;
    float prevZoomRatio = 1.0f;
    int mainSolutionMargin = 0;

    if (m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
        int recommendCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE);
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

        zoomRatio = m_configurations->getZoomRatio();
        prevZoomRatio = m_configurations->getPrevZoomRatio();

        if (m_2x_btn) {
            int targetFallBackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT);

            if ((m_prevRecomCamType != recommendCamType && recommendCamType == dispCamType)
                    || targetFallBackState) {
                m_2x_btn = false;
                m_configurations->setModeValue(CONFIGURATION_DUAL_2X_BUTTON, false);
                CLOGD("complete to processing 2x Button. camType(%d/%d/%d), targetFallbackState(%d)",
                        m_prevRecomCamType, recommendCamType, dispCamType, targetFallBackState);
            }
        } else {
            if (m_check2XButtonPressed(zoomRatio, prevZoomRatio) == true) {
                m_2x_btn = true;
                m_configurations->setModeValue(CONFIGURATION_DUAL_2X_BUTTON, true);
                m_holdZoomRatio = prevZoomRatio;
                m_prevRecomCamType = recommendCamType;
                m_configurations->resetStaticFallbackState();   /* for blinking issue */

                CLOGD("detect 2x Button. zoomRatio(%f/%f)", prevZoomRatio, zoomRatio);
            }
        }

        m_fusionZoomPreviewWrapper->m_getActiveZoomInfo(targetActiveZoomRect,
                &mainActiveZoomRect,
                &subActiveZoomRect,
                &mainSolutionMargin,
                &subSolutionMargin);

        CLOGV("[MAIN]zoomRatio(%f->%f),targetActiveZoomRect(%d,%d,%d,%d(%d) -> %d,%d,%d,%d(%d)), margin(%d)",
                m_configurations->getZoomRatio(),
                (float)sensorMaxW/(float)mainActiveZoomRect.w,
                targetActiveZoomRect.x,
                targetActiveZoomRect.y,
                targetActiveZoomRect.w,
                targetActiveZoomRect.h,
                SIZE_RATIO(targetActiveZoomRect.w, targetActiveZoomRect.h),
                mainActiveZoomRect.x,
                mainActiveZoomRect.y,
                mainActiveZoomRect.w,
                mainActiveZoomRect.h,
                SIZE_RATIO(mainActiveZoomRect.w, mainActiveZoomRect.h),
                mainSolutionMargin);
        CLOGV("[SUB]zoomRatio(%f->%f),targetActiveZoomRect(%d,%d,%d,%d(%d) -> %d,%d,%d,%d(%d)), margin(%d)",
                m_configurations->getZoomRatio(),
                (float)sensorMaxW/(float)subActiveZoomRect.w,
                targetActiveZoomRect.x,
                targetActiveZoomRect.y,
                targetActiveZoomRect.w,
                targetActiveZoomRect.h,
                SIZE_RATIO(targetActiveZoomRect.w, targetActiveZoomRect.h),
                subActiveZoomRect.x,
                subActiveZoomRect.y,
                subActiveZoomRect.w,
                subActiveZoomRect.h,
                SIZE_RATIO(subActiveZoomRect.w, subActiveZoomRect.h),
                subSolutionMargin);

        if (mainActiveZoomRect.x < 0 || mainActiveZoomRect.y < 0
                || mainActiveZoomRect.w > sensorMaxW|| mainActiveZoomRect.h > sensorMaxH) {
            CLOGE("Invalid main zoom rect(%d,%d,%d,%d",
                    mainActiveZoomRect.x,
                    mainActiveZoomRect.y,
                    mainActiveZoomRect.w,
                    mainActiveZoomRect.h);
            m_parameters[m_cameraId]->setActiveZoomMargin(0);
        }  else {
            if (m_2x_btn) {
                /* Hold CropRegion for 2X BTN*/
                m_parameters[m_cameraId]->setActiveZoomMargin(0);

                targetActiveZoomRect.w = ceil((float)sensorMaxW / m_holdZoomRatio);
                targetActiveZoomRect.h = ceil((float)sensorMaxH / m_holdZoomRatio);
                targetActiveZoomRect.x = (sensorMaxW - targetActiveZoomRect.w)/2;
                targetActiveZoomRect.y = (sensorMaxH - targetActiveZoomRect.h)/2;
            } else {
                /* set Solution Zoom Margin */
                m_parameters[m_cameraId]->setActiveZoomMargin(mainSolutionMargin);

                targetActiveZoomRect.x = mainActiveZoomRect.x;
                targetActiveZoomRect.y = mainActiveZoomRect.y;
                targetActiveZoomRect.w = mainActiveZoomRect.w;
                targetActiveZoomRect.h = mainActiveZoomRect.h;
            }
        }

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
        // Temp for DualPreview Solution
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true && zoomRatio != 1.0f) {
            m_parameters[m_cameraId]->getDualSolutionSize(&cropWidth, &cropHeight,
                    &cropWideWidth, &cropWideHeight,
                    &cropTeleWidth, &cropTeleHeight,
                    mainSolutionMargin);
            CLOGV("[HIFIVIDEO] sensorMax (%dx%d), crop (%dx%d), cropWide (%dx%d), cropTele (%dx%d), mainSolutionMargin %d",
                    sensorMaxW,
                    sensorMaxH,
                    cropWidth,
                    cropHeight,
                    cropWideWidth,
                    cropWideHeight,
                    cropTeleWidth,
                    cropTeleHeight,
                    mainSolutionMargin);
            CLOGV("[HIFIVIDEO] Org targetActiveZoomRect x %d, y %d, w %d, h %d",
                    targetActiveZoomRect.x,
                    targetActiveZoomRect.y,
                    targetActiveZoomRect.w,
                    targetActiveZoomRect.h);

            if (mainSolutionMargin == DUAL_SOLUTION_MARGIN_VALUE_30 ||
                    mainSolutionMargin == DUAL_SOLUTION_MARGIN_VALUE_20) {
                cropMinWidth = cropWideWidth;
                cropMinHeight = cropWideHeight;
            } else {
                cropMinWidth = cropWidth;
                cropMinHeight = cropHeight;
            }
            if (targetActiveZoomRect.w < cropMinWidth) {
                targetActiveZoomRect.w = cropMinWidth;
                targetActiveZoomRect.x = ALIGN_DOWN((sensorMaxW - cropMinWidth) / 2, 2);
            }
            if (targetActiveZoomRect.h < cropMinHeight) {
                targetActiveZoomRect.h = cropMinHeight;
                targetActiveZoomRect.y = ALIGN_DOWN((sensorMaxH - cropMinHeight) / 2, 2);
            }
            CLOGV("[HIFIVIDEO] New targetActiveZoomRect x %d, y %d, w %d, h %d",
                    targetActiveZoomRect.x,
                    targetActiveZoomRect.y,
                    targetActiveZoomRect.w,
                    targetActiveZoomRect.h);
        }
#endif
    } else {
        int dualOpMode = m_configurations->getDualOperationMode();
        int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

        zoomRatio = m_configurations->getZoomRatio();
        prevZoomRatio = m_configurations->getPrevZoomRatio();

        if (m_2x_btn) {
            int targetFallBackState = m_configurations->getStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT);

            if (m_prevRecomCamType != dispCamType || targetFallBackState) {
                m_2x_btn = false;

                CLOGD("ForceSwitching:complete to processing 2x Button. camType(%d/%d)",
                        m_prevRecomCamType, dispCamType);
            }
        } else {
            if (m_check2XButtonPressed(zoomRatio, prevZoomRatio) == true) {
                m_2x_btn = true;
                m_holdZoomRatio = prevZoomRatio;
                m_prevRecomCamType = dispCamType;
                /* clear target fallback */
                m_configurations->setStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT, 0);

                CLOGD("ForceSwitching:detect 2x Button. zoomRatio(%f->%f)", prevZoomRatio, zoomRatio);
            }
        }

        if (m_2x_btn) {
            /* Hold CropRegion for 2X BTN*/
            targetActiveZoomRect.w = ceil((float)sensorMaxW / m_holdZoomRatio);
            targetActiveZoomRect.h = ceil((float)sensorMaxH / m_holdZoomRatio);
            targetActiveZoomRect.x = (sensorMaxW - targetActiveZoomRect.w)/2;
            targetActiveZoomRect.y = (sensorMaxH - targetActiveZoomRect.h)/2;
        }

        m_parameters[m_cameraId]->setActiveZoomMargin(0);
    }

    return;
}
#endif

#ifdef SAMSUNG_HIFI_CAPTURE
void ExynosCamera::m_setupReprocessingPipeline_HifiCapture(ExynosCameraFrameFactory *factory)
{
    uint32_t cameraId = factory->getCameraId();

    if (m_configurations->getMode(CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE) == true
        && m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true) {
        int hwMaxWidth = 0, hwMaxHeight = 0;
        int width = 0, height = 0;
        int yuvPortId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + ExynosCameraParameters::YUV_MAX;
        m_configurations->getSize(CONFIGURATION_YUV_SIZE,(uint32_t *)&width, (uint32_t *)&height, yuvPortId);
        m_parameters[cameraId]->getSize(HW_INFO_HW_PICTURE_SIZE, (uint32_t *)&hwMaxWidth, (uint32_t *)&hwMaxHeight);
        if (hwMaxWidth != width || hwMaxHeight != height) {
            m_flagUseInternalyuvStall = true;
            CLOGD("set m_flagUseInternalyuvStall (%d)", m_flagUseInternalyuvStall);
        }
    }
}
#endif

#ifdef SAMSUNG_DOF
void ExynosCamera::m_selectBayer_dof(int shotMode, ExynosCameraActivityAutofocus *autoFocusMgr, frame_handle_components_t components)
{
    if (shotMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELECTIVE_FOCUS) {
        int focusLensPos = 0;

        focusLensPos = m_configurations->getModeValue(CONFIGURATION_FOCUS_LENS_POS);

        autoFocusMgr->setStartLensMove(false);
        m_configurations->setMode(CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE, false);
        m_configurations->setModeValue(CONFIGURATION_FOCUS_LENS_POS, 0);
        m_isFocusSet = false;

        if (focusLensPos > 0) {
            m_isFocusSet = true;
            autoFocusMgr->setStartLensMove(true);
            components.parameters->m_setLensPos(focusLensPos);
            m_configurations->setModeValue(CONFIGURATION_FOCUS_LENS_POS, 0);
        }
    }

    return;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
bool ExynosCamera::m_selectBayer_tn(ExynosCameraFrameSP_sptr_t frame, uint64_t captureExposureTime, camera2_shot_ext *shot_ext, ExynosCameraBuffer &bayerBuffer, ExynosCameraBuffer &depthMapBuffer)
{
    status_t ret = 0;

    if (captureExposureTime > CAMERA_PREVIEW_EXPOSURE_TIME_LIMIT
#ifdef USE_EXPOSURE_DYNAMIC_SHOT
            && captureExposureTime <= PERFRAME_CONTROL_CAMERA_EXPOSURE_TIME_MAX
#endif
            && (shot_ext->shot.udm.ae.vendorSpecific[398] != 1
                && captureExposureTime != shot_ext->shot.dm.sensor.exposureTime / 1000)) {
        CLOGD("captureExposureTime(%lld), sensorExposureTime(%lld), vS[64](%lld), vS[398](%d)",
                captureExposureTime,
                shot_ext->shot.dm.sensor.exposureTime,
                (int64_t)(1000000000.0/shot_ext->shot.udm.ae.vendorSpecific[64]),
                shot_ext->shot.udm.ae.vendorSpecific[398]);

        /* Return buffer */
        ret = m_bufferSupplier->putBuffer(bayerBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for Pipe(%d)",
                    frame->getFrameCount(), bayerBuffer.index, m_getBayerPipeId()) ;
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
        return true;
    }

    return false;
}
#endif


#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
void ExynosCamera::m_captureStreamThreadFunc_dualPortraitSef(ExynosCameraFrameSP_sptr_t frame,
                                                            frame_handle_components_t &components,
                                                            int &dstPipeId)
{
    status_t ret = 0;

    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
        dstPipeId = PIPE_HWFC_JPEG_DST_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            ExynosCameraBuffer jpegbuffer;
            ExynosRect pictureRect;
            ExynosRect thumbnailRect;
            exif_attribute_t exifInfo;
            struct camera2_shot_ext *jpeg_meta_shot;

            jpegbuffer.index = -2;
            ret = frame->getDstBuffer(PIPE_ISP_REPROCESSING, &jpegbuffer, components.reprocessingFactory->getNodeType(dstPipeId));
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_ISP, ret);
            } else if (jpegbuffer.index >= 0) {
                components.parameters->getFixedExifInfo(&exifInfo);

                pictureRect.colorFormat = components.parameters->getHwPictureFormat();
                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureRect.w, (uint32_t *)&pictureRect.h);
                m_configurations->getSize(CONFIGURATION_THUMBNAIL_SIZE, (uint32_t *)&thumbnailRect.w, (uint32_t *)&thumbnailRect.h);

                /* get dynamic meters for make exif info */
                jpeg_meta_shot = new struct camera2_shot_ext;
                memset(jpeg_meta_shot, 0x00, sizeof(struct camera2_shot_ext));
                frame->getMetaData(jpeg_meta_shot);
                components.parameters->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, &jpeg_meta_shot->shot, true);

                /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
                if (jpegbuffer.size[0] != 0) {
                    /* APP1 Marker - EXIF */
                    UpdateExif(jpegbuffer.addr[0], jpegbuffer.size[0], &exifInfo);
                    /* APP4 Marker - DebugInfo */
                    UpdateDebugData(jpegbuffer.addr[0], jpegbuffer.size[0], components.parameters->getDebug2Attribute());
                }

                delete jpeg_meta_shot;
            }
        }
    }

    return;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
int ExynosCamera::m_captureStreamThreadFunc_tn(ExynosCameraFrameSP_sptr_t frame,
                                                            frame_handle_components_t &components,
                                                            ExynosCameraFrameEntity *entity,
                                                            int yuvStallPipeId)
{
    status_t ret = 0;

    if (frame->getRequest(yuvStallPipeId) == true
            && frame->getFrameYuvStallPortUsage() == YUV_STALL_USAGE_DSCALED) {
        if (frame->getStreamRequested(STREAM_TYPE_THUMBNAIL_CB)) {
            if (m_thumbnailCbThread->isRunning()) {
                m_thumbnailCbThread->join();
                CLOGD("m_thumbnailCbThread join");
            }

            m_thumbnailCbThread->run();
            m_thumbnailCbQ->pushProcessQ(&frame);
        }

#ifdef SAMSUNG_BD
        if (frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_BLUR_DETECTION) {
            if (m_dscaledYuvStallPostProcessingThread->isRunning()) {
                m_dscaledYuvStallPostProcessingThread->join();
                CLOGD("m_dscaledYuvStallPostProcessingThread join");
            }

            m_dscaledYuvStallPostProcessingThread->run();
            m_dscaledYuvStallPPCbQ->pushProcessQ(&frame);
        }
#endif

        if (frame->getStreamRequested(STREAM_TYPE_THUMBNAIL_CB)) {
            m_thumbnailCbThread->join();
        }

#ifdef SAMSUNG_BD
        if (frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_BLUR_DETECTION) {
            m_dscaledYuvStallPostProcessingThread->join();
        }
#endif

        ExynosCameraBuffer srcBuffer;
        ret = frame->getDstBuffer(entity->getPipeId(), &srcBuffer, components.reprocessingFactory->getNodeType(yuvStallPipeId));
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]getDstBuffer fail. pipeId (%d) ret(%d)",
                    frame->getFrameCount(), srcBuffer.index, yuvStallPipeId, ret);
            return -1;
        }

        ret = m_bufferSupplier->putBuffer(srcBuffer);
        if (ret < 0) {
            CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                    frame->getFrameCount(), srcBuffer.index, ret);
        }
    }else {
        /* NV21 capture callback buffers will be handled from the caller in this case */
        return  1;
    }

    return ret;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_captureStreamThreadFunc_caseUniProcessing(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity,
                                                            int currentLDCaptureStep,
                                                            int LDCaptureLastStep)
{
    status_t ret = 0;
    ExynosCameraBuffer buffer;
	int pipeId = entity->getPipeId();

#ifdef SAMSUNG_LLS_DEBLUR
        if (frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
            || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
            || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_LL_HDR
#endif
            || frame->getPPScenario(PIPE_PP_UNI_REPROCESSING) == PP_SCENARIO_LLS_DEBLUR) {
            if (currentLDCaptureStep < LDCaptureLastStep) {
                ret = frame->getSrcBuffer(pipeId, &buffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getSrcBuffer, pipeId %d, ret %d", pipeId, ret);
                    return ret;
                }

                m_LDBuf[currentLDCaptureStep] = buffer;
                return ret;
            } else {
                for (int i = SCAPTURE_STEP_COUNT_1; i < LDCaptureLastStep; i++) {
                    /* put GSC 1st buffer */
                    ret = m_bufferSupplier->putBuffer(m_LDBuf[i]);
                    if (ret != NO_ERROR) {
                        CLOGE("[B%d]Failed to putBuffer for pipe(%d). ret(%d)",
                                m_LDBuf[i].index, pipeId, ret);
                    }
                }

                if (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL)) {
                    frame->setFrameSpecialCaptureStep(SCAPTURE_STEP_NV21);
                } else {
                    frame->setFrameSpecialCaptureStep(SCAPTURE_STEP_JPEG);
                }
            }
        }
#endif

#ifdef SAMSUNG_STR_CAPTURE
        if (frame->getPPScenario(pipeId) == PP_SCENARIO_STR_CAPTURE) {
            ExynosRect rect;
            int planeCount = 0;

            ret = frame->getDstBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), pipeId, ret);
                return ret;
            }

            frame->getDstRect(pipeId, &rect);
            planeCount = getYuvPlaneCount(rect.colorFormat);
            CLOGV("[F%d] exynos_ion_sync_fd() for STR_CAPTURE. pipeId %d, colorFormat %d planeCount %d",
                    frame->getFrameCount(), pipeId,
                    rect.colorFormat, planeCount);

            for (int plane = 0; plane < planeCount; plane++) {
                if (m_ionClient >= 0)
                    exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
            }
        }
#endif

#ifdef SAMSUNG_STR_BV_OFFSET
        if (request != NULL && frame->getBvOffset() > 0) {
            request->setBvOffset(frame->getBvOffset());
        }
#endif

        ret = m_handleNV21CaptureFrame(frame, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d", pipeId, ret);
            if (ret < 0) {
                CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                        frame->getFrameCount(), srcBuffer.index, ret);
            	return ret;
            }
        }
    return ret;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_captureStreamThreadFunc_caseUniProcessing2(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity)
{
    status_t ret = 0;
    ExynosCameraBuffer buffer;
	int pipeId = entity->getPipeId();

#ifdef SAMSUNG_STR_CAPTURE
        if (frame->getPPScenario(pipeId) == PP_SCENARIO_STR_CAPTURE) {
            ExynosRect rect;
            int planeCount = 0;

            ret = frame->getDstBuffer(pipeId, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), pipeId, ret);
                return ret;
            }

            frame->getDstRect(pipeId, &rect);
            planeCount = getYuvPlaneCount(rect.colorFormat);
            CLOGV("[F%d] exynos_ion_sync_fd() for STR_CAPTURE. pipeId %d, colorFormat %d planeCount %d",
                    frame->getFrameCount(), pipeId, rect.colorFormat, planeCount);

            for (int plane = 0; plane < planeCount; plane++) {
                if (m_ionClient >= 0)
                    exynos_ion_sync_fd(m_ionClient, buffer.fd[plane]);
            }
        }
#endif

#ifdef SAMSUNG_STR_BV_OFFSET
        if (request != NULL && frame->getBvOffset() > 0) {
            request->setBvOffset(frame->getBvOffset());
        }
#endif

        ret = m_handleNV21CaptureFrame(frame, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_handleNV21CaptureFrame. pipeId %d ret %d", pipeId, ret);
            return ret;
        }

        return ret;
}
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
status_t ExynosCamera::m_captureStreamThreadFunc_caseSyncReprocessingDualPortraitSef(ExynosCameraFrameSP_sptr_t frame,
                                                            ExynosCameraFrameEntity *entity,
                                                            frame_handle_components_t &components,
                                                            ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = 0;
    ExynosCameraBuffer buffer;

    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT) {
        ExynosCameraBuffer dstBuffer;
        int pipeId_src = -1;

        if (components.parameters->isReprocessing() == true) {
            pipeId_src = m_getMcscLeaderPipeId(components.reprocessingFactory);
        }

        CLOGD("[F%d T%d] pipeId_src(%d) FrameState(%d)",
                frame->getFrameCount(), frame->getFrameType(), pipeId_src, frame->getFrameState());

        if (components.parameters->isUseHWFC() == true) {
            if (frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
                ret = frame->getDstBuffer(pipeId_src, &dstBuffer, components.reprocessingFactory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                            pipeId_src, PIPE_MCSC_JPEG_REPROCESSING, ret);
                    return ret;
                }

                ret = m_bufferSupplier->putBuffer(dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC_JPEG_REP. ret %d",
                            frame->getFrameCount(), dstBuffer.index, ret);
                    return ret;
                }
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
                    return ret;
                }

                ret = m_bufferSupplier->putBuffer(dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC_THUMB_REP. ret %d",
                            frame->getFrameCount(), dstBuffer.index, ret);
                    return ret;
                }
            }
        }
    }

    return ret;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
void ExynosCamera::m_handleNV21CaptureFrame_tn(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId, int &nodePipeId)
{
    status_t ret = 0;

    if (leaderPipeId == PIPE_PP_UNI_REPROCESSING
            || leaderPipeId == PIPE_PP_UNI_REPROCESSING2) {
#ifdef SAMSUNG_HIFI_CAPTURE
        if ((frame->getPPScenario(leaderPipeId) == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
                    || frame->getPPScenario(leaderPipeId) == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                    || frame->getPPScenario(leaderPipeId) == PP_SCENARIO_LL_HDR
#endif
            ) && leaderPipeId == PIPE_PP_UNI_REPROCESSING) {
            /* In case Src buffer and Dst buffer is different. */
            ExynosCameraBuffer srcBuffer;

            ret = frame->getSrcBuffer(leaderPipeId, &srcBuffer);
            if (ret < 0) {
                CLOGD("[F%d B%d]Failed to getSrcBuffer. pipeId(%d), ret(%d)",
                        frame->getFrameCount(),
                        srcBuffer.index,
                        leaderPipeId, ret);
            } else {
                ret = m_bufferSupplier->putBuffer(srcBuffer);
                if (ret < 0) {
                    CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), srcBuffer.index, ret);
                }
            }
        }
#endif
        nodePipeId = leaderPipeId;
    }
}
#endif

#ifdef SAMSUNG_TN_FEATURE
bool ExynosCamera::m_handleNV21CaptureFrame_useInternalYuvStall(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            ExynosCameraRequestSP_sprt_t request,
                                                            ExynosCameraBuffer &dstBuffer,
                                                            int nodePipeId,
                                                            int streamId,
                                                            status_t &error)
{
    status_t ret = 0;

    if (m_flagUseInternalyuvStall == true
            && leaderPipeId != PIPE_PP_UNI_REPROCESSING
            && leaderPipeId != PIPE_PP_UNI_REPROCESSING2) {
        buffer_manager_tag_t bufTag;
        ExynosCameraBuffer serviceBuffer;
        const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
        int32_t index = -1;
        ExynosCameraDurationTimer timer;

        serviceBuffer.index = -2;
        index = request->getBufferIndex(streamId);

        if (index >= 0) {
            const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
            buffer_handle_t *handle = streamBuffer->buffer;

            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
            bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId) % ExynosCameraParameters::YUV_MAX)
                + PIPE_MCSC0_REPROCESSING;
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
                CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frame->getFrameCount(), serviceBuffer.index, streamId, ret);
            }

            if (dstBuffer.index >= 0) {
                timer.start();
                memcpy(serviceBuffer.addr[0], dstBuffer.addr[0], serviceBuffer.size[0]);
                if (m_ionClient >= 0)
                    exynos_ion_sync_fd(m_ionClient, serviceBuffer.fd[0]);
                timer.stop();
                CLOGV("memcpy time:(%5d msec)", (int)timer.durationMsecs());

                ret = m_bufferSupplier->putBuffer(dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), dstBuffer.index, ret);
                }
            } else {
                serviceBuffer.index = -2;
            }

            ret = m_sendYuvStreamStallResult(request, &serviceBuffer, streamId);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to sendYuvStreamStallResult."
                        " pipeId %d streamId %d ret %d",
                        frame->getFrameCount(),
                        dstBuffer.index,
                        nodePipeId, streamId, ret);
                error = ret;
            }
        }
        return true;
    }

    return false;
}
#endif

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_handleNV21CaptureFrame_unipipe(ExynosCameraFrameSP_sptr_t frame,
                                                            int leaderPipeId,
                                                            int &pipeId_next,
                                                            int &nodePipeId)
{
    status_t ret = 0;
    ExynosCameraBuffer dstBuffer;
    frame_handle_components_t components;

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    switch(leaderPipeId) {
    case PIPE_3AA_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        pipeId_next = PIPE_PP_UNI_REPROCESSING;
        m_setupCaptureUniPP(frame, leaderPipeId, nodePipeId, pipeId_next);
        break;

    case PIPE_PP_UNI_REPROCESSING:
        if (frame->getRequest(PIPE_PP_UNI_REPROCESSING2) == true) {
#ifdef SAMSUNG_HIFI_CAPTURE
            if (frame->getPPScenario(leaderPipeId) == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
                    || frame->getPPScenario(leaderPipeId) == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                        || frame->getPPScenario(leaderPipeId) == PP_SCENARIO_LL_HDR
#endif
               ) {
                /* In case Src buffer and Dst buffer is different. */
                ExynosCameraBuffer srcBuffer;

                ret = frame->getSrcBuffer(leaderPipeId, &srcBuffer);
                if (ret < 0) {
                    CLOGD("[F%d B%d]Failed to getSrcBuffer. pipeId(%d), ret(%d)",
                            frame->getFrameCount(),
                            srcBuffer.index,
                            leaderPipeId, ret);
                } else {
                    ret = m_bufferSupplier->putBuffer(srcBuffer);
                    if (ret < 0) {
                        CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                                frame->getFrameCount(), srcBuffer.index, ret);
                    }
                }
            }
#endif
            pipeId_next = PIPE_PP_UNI_REPROCESSING2;
            m_setupCaptureUniPP(frame, leaderPipeId, nodePipeId, pipeId_next);
        } else {
            pipeId_next = PIPE_JPEG_REPROCESSING;

            ret = frame->getDstBuffer(leaderPipeId, &dstBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), leaderPipeId, ret);
                return ret;
            }

            ret = frame->setSrcBuffer(pipeId_next, dstBuffer);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                return ret;
            }
        }
        break;

    case PIPE_PP_UNI_REPROCESSING2:
        ret = frame->getDstBuffer(leaderPipeId, &dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), nodePipeId, ret);
            return ret;
        }

        pipeId_next = PIPE_JPEG_REPROCESSING;

        ret = frame->setSrcBuffer(pipeId_next, dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("setSrcBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            return ret;
        }
        break;
    case PIPE_FUSION_REPROCESSING:
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        if (m_scenario == SCENARIO_DUAL_REAR_ZOOM && m_fusionZoomCaptureWrapper != NULL) {
            UTstr fusionDebugData;
            fusionDebugData.data = new unsigned char[LLS_EXIF_SIZE];
            m_fusionZoomCaptureWrapper->m_getDebugInfo(m_cameraId, &fusionDebugData);
            if (ret < 0) {
                CLOGE("[Fusion] UNI_PLUGIN_INDEX_DEBUG_INFO failed!!");
            }
            if (fusionDebugData.data != NULL) {
                components.parameters->setFusionCapturedebugInfo(fusionDebugData.data, LLS_EXIF_SIZE);
                delete []fusionDebugData.data;
            } else {
                CLOGE("[Fusion] debug data is null!!");
            }
        }
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)
                    && (frame->getFrameSpecialCaptureStep() == SCAPTURE_STEP_JPEG
                        && m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT) > 0)) {
                    CLOGD("[BokehCapture] convert Tele NV21 frame from LiveFocusLLS to Jpeg");
                    ExynosCameraBuffer masterJpegSrcBuffer;
                    ExynosCameraBuffer masterJpegDstBuffer;
                    buffer_manager_tag_t bufTag;
                    int index = -1;
                    ExynosCameraRequestSP_sprt_t request = NULL;

                    masterJpegSrcBuffer.index = -2;

                    // PIPE_FUSION_REPROCESSING(OUTPUT_NODE_2) has Tele NV21 frame from LiveFocusLLS.
                    ret = frame->getSrcBuffer(leaderPipeId, &masterJpegSrcBuffer, OUTPUT_NODE_2);
                    if (ret != NO_ERROR || masterJpegSrcBuffer.index < 0) {
                        CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d), wideSrcBuffer.index(%d)",
                                leaderPipeId, ret, masterJpegSrcBuffer.index);
                        return ret;
                    }

                    pipeId_next = PIPE_JPEG_REPROCESSING;
                    ret = frame->setSrcBuffer(pipeId_next, masterJpegSrcBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                        return ret;
                    }

                    bufTag.pipeId[0] = pipeId_next;
                    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;
                    masterJpegDstBuffer.index = -2;

                    ret = m_bufferSupplier->getBuffer(bufTag, &masterJpegDstBuffer);
                    if (ret != NO_ERROR || masterJpegDstBuffer.index < 0) {
                        CLOGE("[F%d B%d]Failed to get InternalNV21Buffer. ret %d",
                                frame->getFrameCount(), masterJpegDstBuffer.index, ret);
                        return ret;
                    }

                    ret = frame->setDstBuffer(pipeId_next, masterJpegDstBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("setDstBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                        return ret;
                    }

                    components.reprocessingFactory->setOutputFrameQToPipe(m_slaveJpegDoneQ, pipeId_next);
                    components.reprocessingFactory->pushFrameToPipe(frame, pipeId_next);
                    if (components.reprocessingFactory->checkPipeThreadRunning(pipeId_next) == false) {
                        components.reprocessingFactory->startThread(pipeId_next);
                    }

                    CLOGD("waiting m_slaveJpegDoneQ");
                    int waitCount = 0;
                    ExynosCameraBuffer jpegBuffer;
                    jpegBuffer.index = -2;
                    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
                    do {
                        ret = m_slaveJpegDoneQ->waitAndPopProcessQ(&frame);
                        waitCount++;
                    } while (ret == TIMED_OUT && waitCount < 100);

                    if (ret < 0) {
                        CLOGW("Failed to waitAndPopProcessQ. ret %d waitCount %d", ret, waitCount);
                    }
                    if (frame == NULL) {
                        CLOGE("frame is NULL");
                        return ret;
                    }
                    CLOGD("complete jpeg convert");

                    ret = frame->getDstBuffer(pipeId_next, &jpegBuffer);
                    if (ret < 0 || jpegBuffer.index < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), jpegBuffer.index(%d), ret(%d)",
                            pipeId_next, jpegBuffer.index, ret);
                        return ret;
                    }
                    jpegBuffer.size[0] = frame->getJpegSize();

                    ret = frame->getDstBufferState(pipeId_next, &bufferState);
                    if (ret != NO_ERROR) {
                        CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d",
                                __FUNCTION__, __LINE__,
                                frame->getFrameCount(),
                                pipeId_next);
                        return ret;
                    } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                        CLOGE("ERR(%s[%d]):Invalid thumnail stream buffer state. frameCount %d bufferState %d",
                                __FUNCTION__, __LINE__,
                                frame->getFrameCount(),
                                bufferState);
                        return ret;
                    }

                    // In LiveFocusCapture,
                    // DstBuffer of PIPE_ISP_RE(PIPE_HWFC_JPEG_DST_RE) is used to contain the TeleJpeg.
                    ret = frame->setDstBufferState(PIPE_ISP_REPROCESSING,
                                                    ENTITY_BUFFER_STATE_REQUESTED,
                                                    components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                    ret = frame->setDstBuffer(PIPE_ISP_REPROCESSING, jpegBuffer,
                                                components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), Node(%d), ret(%d)",
                            PIPE_ISP_REPROCESSING,
                            components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING), ret);
                        return ret;
                    }

                    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
                    if (request == NULL) {
                        CLOGE("[F%d]Failed to get request.", frame->getFrameCount());
                        return ret;
                    }

                    // get Jpeg Service Buffer, set DstBuffer of PIPE_JPEG_REPROCESSING
                    index = request->getBufferIndex(HAL_STREAM_ID_JPEG);
                    if (index >= 0) {
                        ExynosCameraBuffer serviceJpegDstBuffer;
                        const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
                        const buffer_manager_tag_t initBufTag;
                        const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
                        buffer_handle_t *handle = streamBuffer->buffer;

                        streamId = request->getStreamIdwithBufferIdx(index);

                        bufTag = initBufTag;
                        bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
                        bufTag.pipeId[0] = pipeId_next;

                        request->setParentStreamPipeId(streamId, pipeId_next);
                        request->setStreamPipeId(streamId, bufTag.pipeId[0]);

                        serviceJpegDstBuffer.handle[0] = handle;
                        serviceJpegDstBuffer.acquireFence[0] = streamBuffer->acquire_fence;
                        serviceJpegDstBuffer.releaseFence[0] = streamBuffer->release_fence;
                        serviceJpegDstBuffer.index = -2;

                        ret = m_bufferSupplier->getBuffer(bufTag, &serviceJpegDstBuffer);
                        if (ret != NO_ERROR || serviceJpegDstBuffer.index < 0) {
                            CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                                    request->getKey(), frame->getFrameCount(), serviceJpegDstBuffer.index, streamId, ret);
                            return ret;
                        }

                        ret = request->setAcquireFenceDone(handle, (serviceJpegDstBuffer.acquireFence[0] == -1) ? true : false);
                        if (ret != NO_ERROR || serviceJpegDstBuffer.index < 0) {
                            CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                                    request->getKey(), frame->getFrameCount(), serviceJpegDstBuffer.index, streamId, ret);
                            return ret;
                        }

                        if (serviceJpegDstBuffer.index >= 0) {
                            frame->setDstBufferState(pipeId_next, ENTITY_BUFFER_STATE_REQUESTED);
                            ret = frame->setDstBuffer(pipeId_next, serviceJpegDstBuffer);
                            if (ret != NO_ERROR) {
                                CLOGE("setDstBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                                return ret;
                            }
                        } else {
                            CLOGE("setDstBuffer() fail, pipeId(%d), ret(%d), buffer.index(%d)",
                                pipeId_next, ret, serviceJpegDstBuffer.index);
                            return ret;
                        }
                        frame->setSrcBufferState(pipeId_next, ENTITY_BUFFER_STATE_REQUESTED);
                    }
                }
#endif

        ret = frame->getDstBuffer(leaderPipeId, &dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                    frame->getFrameCount(), nodePipeId, ret);
            return ret;
        }

        pipeId_next = PIPE_JPEG_REPROCESSING;

        ret = frame->setSrcBuffer(pipeId_next, dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("setSrcBuffer() fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            return ret;
        }
        break;
    }

    return ret;
}
#endif

#if defined(SAMSUNG_DUAL_ZOOM_PREVIEW)
void ExynosCamera::m_setBuffers_dualZoomPreview(ExynosRect &previewRect)
{
    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;

        m_parameters[m_cameraId]->getFusionSize(previewRect.w, previewRect.h,
                &fusionSrcRect, &fusionDstRect,
                DUAL_SOLUTION_MARGIN_VALUE_30);
        previewRect.w = fusionSrcRect.w;
        previewRect.h = fusionSrcRect.h;
    }
}
#endif

#ifdef SAMSUNG_SW_VDIS
status_t ExynosCamera::m_setupBatchFactoryBuffers_swVdis(ExynosCameraFrameSP_sptr_t frame,
                                                  ExynosCameraFrameFactory *factory,
                                                  int streamId,
                                                  uint32_t &leaderPipeId,
                                                  buffer_manager_tag_t &bufTag)
{
    status_t ret = 0;
    uint32_t nodeType = 0;
    frame_handle_components_t components;
    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);

    bool flag3aaIspM2M = (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);

    /* Set Internal Buffer */
    if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
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
                if (flag3aaIspM2M == true) {
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
                return ret;
            }

            nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);
            ret = frame->setDstBufferState(leaderPipeId, ENTITY_BUFFER_STATE_REQUESTED, nodeType);
            if (ret != NO_ERROR) {
                CLOGE("Set Internal Buffers: Failed to setDstBufferState. ret %d", ret);
                return ret;
            }

            ret = frame->setDstBuffer(leaderPipeId, internalBuffer, nodeType);
            if (ret != NO_ERROR) {
                CLOGE("Set Internal Buffers: Failed to setDstBuffer. ret %d", ret);
                return ret;
            }
        }
    }

    return ret;
}
#endif


#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCamera::m_setReprocessingBuffer_tn(int maxPictureW, int maxPictureH)
{
    status_t ret = 0;
    int DScaledYuvStallSizeW = 0, DScaledYuvStallSizeH = 0;
    int minBufferCount = 0;
    int maxBufferCount = 0;
    buffer_manager_tag_t bufTag;
    buffer_manager_configuration_t bufConfig;
    const buffer_manager_tag_t initBufTag;
    const buffer_manager_configuration_t initBufConfig;

    if (m_configurations->getSamsungCamera() == true) {
        /* DScaleYuvStall Internal buffer */
        m_configurations->getSize(CONFIGURATION_DS_YUV_STALL_SIZE, (uint32_t *)&DScaledYuvStallSizeW, (uint32_t *)&DScaledYuvStallSizeH);
        maxBufferCount = NUM_DSCALED_BUFFERS;

        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_GSC_REPROCESSING3;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("DSCALEYUVSTALL_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create DSCALEYUVSTALL_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = 2;
        bufConfig.size[0] = (DScaledYuvStallSizeW * DScaledYuvStallSizeH * 3) / 2;
        bufConfig.reqBufCount = maxBufferCount;
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = true;
        bufConfig.reservedMemoryCount = 0;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_DSCALEYUVSTALL, BUF_TYPE_REPROCESSING);
        if (ret != NO_ERROR) {
            CLOGE("Failed to add PRE_THUMBNAIL_BUF. ret %d", ret);
            return ret;
        }
#else
        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc PRE_THUMBNAIL_BUF. ret %d", ret);
            return ret;
        }
#endif

        CLOGI("m_allocBuffers(DSCALEYUVSTALL_BUF Buffer) %d x %d,\
                planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                DScaledYuvStallSizeW, DScaledYuvStallSizeH,
                bufConfig.size[0], bufConfig.planeCount, maxBufferCount);

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
        if ((m_scenario == SCENARIO_DUAL_REAR_PORTRAIT || m_scenario == SCENARIO_DUAL_FRONT_PORTRAIT)) {
            /* JPEG Internal buffer for PORTRAIT */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_JPEG_REPROCESSING;
            bufTag.pipeId[1] = PIPE_HWFC_JPEG_DST_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("JPEG_INTERNAL_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create JPEG_INTERNAL_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 2;
            bufConfig.size[0] = ALIGN_UP(maxPictureW, GSCALER_IMG_ALIGN) * ALIGN_UP(maxPictureH, GSCALER_IMG_ALIGN) * 2;
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
            bufConfig.reqBufCount = 3;
            bufConfig.allowedMaxBufCount = 3;
#else
            bufConfig.reqBufCount = 2;
            bufConfig.allowedMaxBufCount = 2;
#endif
            bufConfig.startBufIndex = 0;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = true;
            bufConfig.reservedMemoryCount = 0;
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

#ifdef ADAPTIVE_RESERVED_MEMORY
            ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_JPEG_INTERNAL, BUF_TYPE_REPROCESSING);
            if (ret != NO_ERROR) {
                CLOGE("Failed to add JPEG_INTERNAL_BUF. ret %d", ret);
                return ret;
            }
#else
            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc JPEG_INTERNAL_BUF. ret %d", ret);
                return ret;
            }
#endif

            CLOGI("m_allocBuffers(JPEG_INTERNAL_BUF Buffer) %d x %d,\
                    planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                    maxPictureW, maxPictureH,
                    bufConfig.size[0], bufConfig.planeCount, maxBufferCount);
        }
#endif

#ifdef USE_LLS_REPROCESSING
        if (m_parameters[m_cameraId]->getLLSOn() == true) {
            /* LLS_RE */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_LLS_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("LLS_RE_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create MCSC_BUF. ret %d", ret);
            }

            minBufferCount = 1;
            maxBufferCount = 1;

            bufConfig = initBufConfig;
            bufConfig.planeCount = 2;
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
            if (m_parameters[m_cameraId]->checkBayerDumpEnable()) {
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
            bufConfig.needMmap = true;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc LLS_REP_BUF. ret %d", ret);
                return ret;
            }
        }
#endif
    }

    return ret;
}
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF
status_t ExynosCamera::m_sendSefStreamResult(ExynosCameraRequestSP_sprt_t request,
                                            ExynosCameraFrameSP_sptr_t frame,
                                            ExynosCameraBuffer *bokehOuputBuffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer inputBuffer[2];  //0: Tele, 1: Wide
    char tagName[256];
    frame_handle_components_t components;
    ExynosCameraBokehCaptureWrapper *bokehCaptureWrapper;
    UniPluginBufferData_t depthMapInfo;
    UTstr bokehDOFSMetaData;
    UTstr bokehArcExtraData;
    int captureW, captureH;
    int numImages = sizeof(inputBuffer) / sizeof(inputBuffer[0]);
    void *handle = NULL;
    ExynosCameraStream *stream = NULL;
    camera3_stream_buffer_t *streamBuffer = NULL;
    camera3_stream_buffer_t *output_buffers;
    ResultRequest resultRequest = NULL;
    camera3_capture_result_t *requestResult = NULL;
    buffer_manager_tag_t bufTag;
    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
    ExynosCameraDurationTimer timer;
    CameraMetadata *setting = NULL;
    int32_t jpegsize = 0;
    int32_t jpegBufferSize = 0;

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    int currentLDCaptureStep = (int)frame->getFrameSpecialCaptureStep();
    int currentLDCaptureMode = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE);
#endif

    m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&captureW, (uint32_t *)&captureH);

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    bokehCaptureWrapper = (ExynosCameraBokehCaptureWrapper *)(m_bokehCaptureWrapper);

    // get Master(Tele) JPEG
    inputBuffer[0].index = -2;
    ret = frame->getDstBuffer(PIPE_ISP_REPROCESSING, &inputBuffer[0],
        components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
    if (ret != NO_ERROR || inputBuffer[0].index < 0) {
        CLOGE("getDstBuffer fail, pipeId(%d), Node(%d), ret(%d)",
            PIPE_ISP_REPROCESSING,
            components.reprocessingFactory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING), ret);
        return INVALID_OPERATION;
    }

    // get Slave(Wide) JPEG
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    if (currentLDCaptureMode != MULTI_SHOT_MODE_NONE) {
        if (m_liveFocusFirstSlaveJpegBuf.index < 0) {
            CLOGE("Invalid buffer index(%d), framecount(%d)",
                    m_liveFocusFirstSlaveJpegBuf.index, frame->getFrameCount());
            return INVALID_OPERATION;
        } else {
            CLOGD("Change SlaveJpeg to FirstSlaveJpeg");
            inputBuffer[1] = m_liveFocusFirstSlaveJpegBuf;
        }
    } else
#endif
    {
        inputBuffer[1].index = -2;
        ret = frame->getDstBuffer(PIPE_SYNC_REPROCESSING, &inputBuffer[1], CAPTURE_NODE_2);
        if (ret != NO_ERROR || inputBuffer[1].index < 0) {
            CLOGE("getDstBuffer fail, pipeId(%d), Node(%d), ret(%d)",
                PIPE_SYNC_REPROCESSING, CAPTURE_NODE_2, ret);
            return INVALID_OPERATION;
        }
    }

    printSEFVersion();

    CLOGD("[R%d F%d B%d]. ServiceBufferAddr: %p ServiceBufferSize : %d",
            request->getKey(), frame->getFrameCount(), bokehOuputBuffer->index,
            bokehOuputBuffer->addr[0], bokehOuputBuffer->size[0]);

    handle = initSEFAllocatedBuffer(bokehOuputBuffer->addr[0] + frame->getJpegSize(),
                                    bokehOuputBuffer->size[0] - frame->getJpegSize());

    CLOGD("make SEF data. BokehCaptureReusult(%d), ZoomInOutPhoto(%d)",
        m_bokehCaptureWrapper->m_getBokehCaptureResultValue(), m_configurations->getZoomInOutPhoto());

    /* BokehCapture Success */
    if (m_bokehCaptureWrapper->m_getBokehCaptureResultValue() == CONTROL_BOKEH_STATE_SUCCESS) {
        // Get DOFS Metadata
        ret = bokehCaptureWrapper->m_getDOFSMetaData(m_cameraId, &bokehDOFSMetaData);

        if (ret == NO_ERROR && bokehDOFSMetaData.data != NULL && bokehDOFSMetaData.size != 0) {
            if (m_configurations->getZoomInOutPhoto() == true) {
                // Add DOFS Metadata to SEF
                memset(tagName, 0, sizeof(tagName));
                sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_ZOOMINOUT);
                ret = saveSEFDataToBuffer(handle,
                                        tagName,
                                        strlen(tagName),
                                        NULL,
                                        0,
                                        (unsigned char*)bokehDOFSMetaData.data,
                                        bokehDOFSMetaData.size,
                                        SEF_DUAL_SHOT_ZOOMINOUT_INFO,
                                        QURAM_SEF_SAVE_WITH_UPDATE);
            } else {
                // Add DOFS Metadat to SEF
                memset(tagName, 0, sizeof(tagName));
                sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_INFO);
                ret = saveSEFDataToBuffer(handle,
                                         tagName,
                                         strlen(tagName),
                                         NULL,
                                         0,
                                         (unsigned char*)bokehDOFSMetaData.data,
                                         bokehDOFSMetaData.size,
                                         SEF_DUAL_SHOT_INFO,
                                         QURAM_SEF_SAVE_WITH_UPDATE);

            }
        }

        // Add Tele/Wide Input Jpeg to SEF
        for (int index = 0 ; index < numImages ; index++) {
            memset(tagName, 0, sizeof(tagName));
            sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_JPEG_TEMPLATE, index + 1);
            if (inputBuffer[index].addr[0] != NULL && inputBuffer[index].size[0] != 0) {
                if (index == 0 || m_configurations->getZoomInOutPhoto() == true) {
                    ret = saveSEFDataToBuffer(handle,
                                         tagName,
                                         strlen(tagName),
                                         NULL,
                                         0,
                                         (unsigned char*)inputBuffer[index].addr[0],
                                         inputBuffer[index].size[0],
                                         SEF_IMAGE_JPEG,
                                         QURAM_SEF_SAVE_WITH_UPDATE);
                }
            }
        }

        // get Depth Map info
        depthMapInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_DEPTHMAP;
        ret = bokehCaptureWrapper->m_getDepthMap(m_cameraId, &depthMapInfo);

        // Add Depth Map info to SET
        if (ret == NO_ERROR && depthMapInfo.outBuffY != NULL && (depthMapInfo.outWidth * depthMapInfo.outHeight) != 0) {
            memset(tagName, 0, sizeof(tagName));
            sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_DEPTHMAP, 1);
            ret = saveSEFDataToBuffer(handle,
                                     tagName,
                                     strlen(tagName),
                                     NULL,
                                     0,
                                     (unsigned char*)depthMapInfo.outBuffY,
                                     depthMapInfo.outWidth * depthMapInfo.outHeight,
                                     SEF_DUAL_SHOT_DEPTHMAP,
                                     QURAM_SEF_SAVE_WITH_UPDATE);
        }

        // get Arc soft Extra Info
        ret = bokehCaptureWrapper->m_getArcExtraData(m_cameraId, &bokehArcExtraData);

        if (ret == NO_ERROR && bokehArcExtraData.data!= NULL && bokehArcExtraData.size != 0) {
            // Add Arc soft Extra Info to SEF
            memset(tagName, 0, sizeof(tagName));
            sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_EXTRA_INFO);
            ret = saveSEFDataToBuffer(handle,
                                        tagName,
                                        strlen(tagName),
                                        NULL,
                                        0,
                                        (unsigned char*)bokehArcExtraData.data,
                                        bokehArcExtraData.size,
                                        SEF_DUAL_SHOT_EXTRA_INFO,
                                        QURAM_SEF_SAVE_WITH_UPDATE);
        }
    } else {
        if (m_configurations->getZoomInOutPhoto() == true) {
            memset(tagName, 0, sizeof(tagName));
            sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_ONLY);
            ret = saveSEFDataToBuffer(handle,
                                        tagName,
                                        strlen(tagName),
                                        NULL,
                                        0,
                                        (unsigned char*)SEF_KEYNAME_DUAL_SHOT_ONLY,
                                        strlen(SEF_KEYNAME_DUAL_SHOT_ONLY),
                                        SEF_DUAL_SHOT_ONLY,
                                        QURAM_SEF_SAVE_WITH_UPDATE);

            if (inputBuffer[1].addr[0] != NULL && inputBuffer[1].size[0] != 0) {
                //Add Wide Input jpeg to SEF
                memset(tagName, 0, sizeof(tagName));
                sprintf(tagName, SEF_KEYNAME_DUAL_SHOT_JPEG_TEMPLATE, 2);
                ret = saveSEFDataToBuffer(handle,
                                            tagName,
                                            strlen(tagName),
                                            NULL,
                                            0,
                                            (unsigned char*)inputBuffer[1].addr[0],
                                            inputBuffer[1].size[0],
                                            SEF_IMAGE_JPEG,
                                            QURAM_SEF_SAVE_WITH_UPDATE);
            } else {
                CLOGE("Wide Jpeg buffer is invalid!!");
            }
        }
    }

#ifdef SAMSUNG_DUAL_PORTRAIT_SEF_DUMP
    {
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/camera/BokehTele_%d.jpeg", frame->getFrameCount());

        bRet = dumpToFile((char *)filePath, (char *)inputBuffer[0].addr[0], (unsigned int)inputBuffer[0].size[0]);
        if (bRet != true)
                CLOGE("couldn't make a tele original Jpeg");

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/camera/BokehWide_%d.jpeg", frame->getFrameCount());

        bRet = dumpToFile((char *)filePath, (char *)inputBuffer[1].addr[0], (unsigned int)inputBuffer[1].size[0]);
        if (bRet != true)
                CLOGE("couldn't make a wide original Jpeg");

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/camera/BokehSEF_%d.sef", frame->getFrameCount());

        bRet = dumpToFile((char *)filePath, (char *)getSEFBuffer(handle), (unsigned int)getSEFWriteSize(handle));
        if (bRet != true)
                CLOGE("couldn't make a nv21 file");


        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/camera/BokehSEFService0_%d.sef", frame->getFrameCount());

        bRet = dumpToFile((char *)filePath, bokehOuputBuffer->addr[0], bokehOuputBuffer->size[0]);
        if (bRet != true)
                CLOGE("couldn't make a nv21 file");


        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/camera/BokehSEFService10_%d.sef", frame->getFrameCount());

        bRet = dumpToFile((char *)filePath, bokehOuputBuffer->addr[0], frame->getJpegSize() + 1 + getSEFWriteSize(handle));
        if (bRet != true)
                CLOGE("couldn't make a nv21 file");
    }
#endif

    // Send Sef
    ret = m_streamManager->getStream(HAL_STREAM_ID_JPEG, &stream);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from StreamMgr. streamId HAL_STREAM_ID_JPEG");
        goto func_exit;
    }

    if (stream == NULL) {
        CLOGE("stream is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    resultRequest = m_requestMgr->createResultRequest(request->getKey(), request->getFrameCount(),
                                        EXYNOS_REQUEST_RESULT::CALLBACK_BUFFER_ONLY);
    if (resultRequest == NULL) {
        CLOGE("[R%d F%d] createResultRequest fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    requestResult = resultRequest->getCaptureResult();
    if (requestResult == NULL) {
        CLOGE("[R%d F%d] getCaptureResult fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    streamBuffer = resultRequest->getStreamBuffer();
    if (streamBuffer == NULL) {
        CLOGE("[R%d F%d] getStreamBuffer fail. streamId HAL_STREAM_ID_JPEG",
                request->getKey(), request->getFrameCount());
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    ret = stream->getStream(&(streamBuffer->stream));
    if (ret != NO_ERROR) {
        CLOGE("Failed to getStream from ExynosCameraStream. streamId HAL_STREAM_ID_JPEG");
        goto func_exit;
    }

    streamBuffer->buffer = bokehOuputBuffer->handle[0];

    ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
            request->getKey(), request->getFrameCount(),
            HAL_STREAM_ID_JPEG, bokehOuputBuffer->index);
        goto func_exit;
    }

    streamBuffer->acquire_fence = -1;
    streamBuffer->release_fence = -1;

    camera3_jpeg_blob_t sef_blob;
    sef_blob.jpeg_blob_id = CAMERA3_JPEG_BLOB_ID;
    sef_blob.jpeg_size = frame->getJpegSize() + getSEFWriteSize(handle);
    jpegBufferSize = ((private_handle_t *)(*(streamBuffer->buffer)))->width;
    memcpy(bokehOuputBuffer->addr[0] + jpegBufferSize - sizeof(camera3_jpeg_blob_t), &sef_blob, sizeof(camera3_jpeg_blob_t));

    /* update jpeg size */
    request->setRequestLock();
    setting = request->getServiceMeta();
    jpegsize = frame->getJpegSize() + getSEFWriteSize(handle);
    ret = setting->update(ANDROID_JPEG_SIZE, &jpegsize, 1);
    if (ret < 0) {
        CLOGE("ANDROID_JPEG_SIZE update failed(%d)", ret);
    }
    request->setRequestUnlock();

    /* construct result for service */
    requestResult->frame_number = request->getKey();
    requestResult->result = NULL;
    requestResult->num_output_buffers = 1;
    requestResult->output_buffers = streamBuffer;
    requestResult->input_buffer = request->getInputBuffer();
    requestResult->partial_result = 0;

    CLOGD("frame number(%d), #out(%d) sef size(%d)",
            requestResult->frame_number, requestResult->num_output_buffers, getSEFWriteSize(handle));

    m_requestMgr->pushResultRequest(resultRequest);

    ret = m_bufferSupplier->putBuffer(*bokehOuputBuffer);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                request->getKey(), request->getFrameCount(), bokehOuputBuffer->index, ret);
    }

    for (int index = 0; index < numImages; index++) {
        ret = m_bufferSupplier->putBuffer(inputBuffer[index]);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to putBuffer. ret %d",
                    request->getKey(), request->getFrameCount(), inputBuffer[index].index, ret);
        }
    }

func_exit:
    if (handle != NULL)
        destroySEFBuffer(handle);

    return ret;
}
#endif

#ifdef DEBUG_IQ_OSD
void ExynosCamera::printOSD(char *Y, char *UV, int yuvSizeW, int yuvSizeH, struct camera2_shot_ext *meta_shot_ext)
{
#ifdef SAMSUNG_UNI_API
    if (m_isOSDMode == 1 && m_configurations->getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_NONE) {
        uni_toast_init(200, 200, yuvSizeW, yuvSizeH, 15);

        short *ae_meta_short = (short *)meta_shot_ext->shot.udm.ae.vendorSpecific;
        unsigned short *awb_meta_short = (unsigned short *)meta_shot_ext->shot.udm.awb.vendorSpecific;
        double sv = (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[3] / 256.0;
        double short_d_gain_preview = meta_shot_ext->shot.udm.ae.vendorSpecific[387] / 256.0;
        double long_short_a_gain_preview = meta_shot_ext->shot.udm.ae.vendorSpecific[386] / 256.0;
        double short_d_gain_capture = meta_shot_ext->shot.udm.ae.vendorSpecific[99] / 256.0;
        double long_d_gain_capture = meta_shot_ext->shot.udm.ae.vendorSpecific[97] / 256.0;
        double long_short_a_gain_capture = meta_shot_ext->shot.udm.ae.vendorSpecific[96] / 256.0;
        unsigned short ai_scene_mode = awb_meta_short[34];
        char llsFlagString[8] = {0, };
        snprintf(llsFlagString, 7, "0x%X", ae_meta_short[281]);

        uni_toastE("Preview Gain : %f", long_short_a_gain_preview * short_d_gain_preview);
        uni_toastE("Capture Gain : %f", long_short_a_gain_capture * short_d_gain_capture);
        uni_toastE("Long/Short Gain : %f / %f", long_d_gain_capture, short_d_gain_capture);
        uni_toastE("EV : %f", (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[4] / 256.0);
        uni_toastE("BV : %f", (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[5] / 256.0);
        uni_toastE("SV : %f", sv);
        uni_toastE("FD count : %d", ae_meta_short[291]);
        uni_toastE("LLS flag : %s", llsFlagString);
        uni_toastE("Zoom Ratio : %f", m_configurations->getZoomRatio());
        uni_toastE("DRC str ratio : %f", pow(2, (int32_t)meta_shot_ext->shot.udm.ae.vendorSpecific[94] / 256.0));
        uni_toastE("Preview NI : %f", 1024 * (1 / (short_d_gain_preview * (1 + (short_d_gain_preview - 1) * 0.199219) * long_short_a_gain_preview)));
        uni_toastE("Capture NI : %f", 1024 * (1 / (short_d_gain_capture * (1 + (short_d_gain_capture - 1) * 0.199219) * long_short_a_gain_capture)));
        if (ai_scene_mode >= 0)
            uni_toastE("AI Scene mode : %d", ai_scene_mode);

        if (meta_shot_ext->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
            uni_toastE("Preview SS : 1/%d", meta_shot_ext->shot.udm.ae.vendorSpecific[385]);
        else
            uni_toastE("Preview SS : 1/%d", meta_shot_ext->shot.udm.internal.vendorSpecific[0]);

        if (meta_shot_ext->shot.udm.ae.vendorSpecific[0] == 0xAEAEAEAE)
            uni_toastE("Capture SS : 1/%d", meta_shot_ext->shot.udm.ae.vendorSpecific[64]);
        else
            uni_toastE("Capture SS : 1/%d", meta_shot_ext->shot.udm.internal.vendorSpecific[0]);

        if (Y != NULL && UV != NULL) {
            unsigned char *hist;

            uni_toast_print(Y, UV, getCameraId());

            hist = (unsigned char *)(meta_shot_ext->shot.udm.rta.vendorSpecific);
            for (int i = 0; i < 120; i++){
                uni_toast_rect(220 + 2 * i, 800 + (128 - *(hist + 179 + i)), 2, *(hist + 179 + i), 2, Y, UV);
            }
            uni_toast_rect(200, 800, 280, 128, 2, Y, UV);
        }
    }
#endif
}
#endif

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_sensorListenerThreadFunc(void)
{
    CLOGI("");
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    m_timer.start();

    if (m_configurations->getModeValue(CONFIGURATION_VT_MODE) == 0) {
        if (m_configurations->getSamsungCamera() == true) {
#ifdef SAMSUNG_ACCELEROMETER /* SamsungCamera only. vendor specific operation */
            if (getCameraId() == CAMERA_ID_BACK
#ifdef USE_DUAL_CAMERA
                || getCameraId() == CAMERA_ID_BACK_1
#endif
            ) {
                if (m_accelerometerHandle == NULL) {
                    m_accelerometerHandle = sensor_listener_load();
                    if (m_accelerometerHandle != NULL) {
                        if (sensor_listener_enable_sensor(m_accelerometerHandle,ST_ACCELEROMETER, 100 * 1000) < 0) {
                            sensor_listener_unload(&m_accelerometerHandle);
                        } else {
                            m_configurations->setMode(CONFIGURATION_ACCELEROMETER_MODE, true);
                        }
                    }
                } else {
                    m_configurations->setMode(CONFIGURATION_ACCELEROMETER_MODE, true);
                }
            }
#endif

            if (getCameraId() == CAMERA_ID_FRONT
#ifdef SAMSUNG_COLOR_IRIS
                && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_COLOR_IRIS
#endif
                && m_configurations->getModeValue(CONFIGURATION_SHOT_MODE) != SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_INTELLIGENT_SCAN) {
#ifdef SAMSUNG_LIGHT_IR /* SamsungCamera only. vendor specific operation */
                if (m_light_irHandle == NULL) {
                    m_light_irHandle = sensor_listener_load();
                    if (m_light_irHandle != NULL) {
                        if (sensor_listener_enable_sensor(m_light_irHandle, ST_LIGHT_IR, 120 * 1000) < 0) {
                            sensor_listener_unload(&m_light_irHandle);
                        } else {
                            m_configurations->setMode(CONFIGURATION_LIGHT_IR_MODE, true);
                        }
                    }
                } else {
                    m_configurations->setMode(CONFIGURATION_LIGHT_IR_MODE, true);
                }
#endif

#ifdef SAMSUNG_PROX_FLICKER /* SamsungCamera only. vendor specific operation */
                if (m_proximityHandle == NULL) {
                    m_proximityHandle = sensor_listener_load();
                    if (m_proximityHandle != NULL) {
                        if (sensor_listener_enable_sensor(m_proximityHandle, ST_PROXIMITY_FLICKER, 120 * 1000) < 0) {
                            sensor_listener_unload(&m_proximityHandle);
                        } else {
                            m_configurations->setMode(CONFIGURATION_PROX_FLICKER_MODE, true);
                        }
                    }
                } else {
                    m_configurations->setMode(CONFIGURATION_PROX_FLICKER_MODE, true);
                }
#endif
            }

#ifdef SAMSUNG_GYRO /* SamsungCamera only. vendor specific operation */
            if (getCameraId() == CAMERA_ID_BACK
#ifdef USE_DUAL_CAMERA
                || getCameraId() == CAMERA_ID_BACK_1
#endif
#ifdef SAMSUNG_GYRO_FRONT
                || getCameraId() == CAMERA_ID_FRONT
#endif
            ) {
                if (m_gyroHandle == NULL) {
                    m_gyroHandle = sensor_listener_load();
                    if (m_gyroHandle != NULL) {
                        if (sensor_listener_enable_sensor(m_gyroHandle,ST_GYROSCOPE, 100 * 1000) < 0) {
                            sensor_listener_unload(&m_gyroHandle);
                        } else {
                            m_configurations->setMode(CONFIGURATION_GYRO_MODE, true);
                        }
                    }
                } else {
                    m_configurations->setMode(CONFIGURATION_GYRO_MODE, true);
                }
            }
#endif
        }

#ifdef SAMSUNG_HRM
        if (getCameraId() == CAMERA_ID_BACK
#ifdef USE_DUAL_CAMERA
            || getCameraId() == CAMERA_ID_BACK_1
#endif
            ) {
            if (m_uv_rayHandle == NULL) {
                m_uv_rayHandle = sensor_listener_load();
                if (m_uv_rayHandle != NULL) {
                    if (sensor_listener_enable_sensor(m_uv_rayHandle, ST_UV_RAY, 10 * 1000) < 0) {
                        sensor_listener_unload(&m_uv_rayHandle);
                    } else {
                        m_configurations->setMode(CONFIGURATION_HRM_MODE, true);
                    }
                }
            } else {
                m_configurations->setMode(CONFIGURATION_HRM_MODE, true);
            }
        }
#endif

#ifdef SAMSUNG_ROTATION
        if (m_rotationHandle == NULL) {
            m_rotationHandle = sensor_listener_load();
            if (m_rotationHandle != NULL) {
                if (sensor_listener_enable_sensor(m_rotationHandle, ST_ROTATION, 100 * 1000) < 0) {
                    sensor_listener_unload(&m_rotationHandle);
                } else {
                    m_configurations->setMode(CONFIGURATION_ROTATION_MODE, true);
                }
            }
        } else {
            m_configurations->setMode(CONFIGURATION_ROTATION_MODE, true);
        }
#endif
    }

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

    return false;
}
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SAMSUNG_TN_FEATURE
bool ExynosCamera::m_previewStreamPPPipeThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_PP_UNI]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        if (m_pipePPPreviewStart[0] == false) {
            return true;
        }

        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_PP_UNI);
}

bool ExynosCamera::m_previewStreamPPPipe2ThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_PP_UNI2]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        if (m_pipePPPreviewStart[1] == false) {
            return true;
        }

        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_PP_UNI2);
}

bool ExynosCamera::m_previewStreamPPPipe3ThreadFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;

    ret = m_pipeFrameDoneQ[PIPE_PP_UNI3]->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        if (m_pipePPPreviewStart[2] == false) {
            return true;
        }

        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return true;
    }
    return m_previewStreamFunc(newFrame, PIPE_PP_UNI3);
}
#endif

#ifdef CAMERA_FAST_ENTRANCE_V1
bool ExynosCamera::m_fastenAeThreadFunc(void)
{
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FASTEN_AE_THREAD_START, 0);
    CLOGI("");
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    ExynosCameraFrameFactory *previewFrameFactory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    status_t ret = NO_ERROR;
    int32_t skipFrameCount = INITIAL_SKIP_FRAME;
    m_fastenAeThreadResult = 0;

    if (previewFrameFactory == NULL) {
        /* Preview Frame Factory */
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
            previewFrameFactory = new ExynosCameraFrameFactoryPreviewDual(m_cameraId, m_configurations, m_parameters[m_cameraId]);
        } else
#endif
        if (m_cameraId == CAMERA_ID_FRONT
            && m_configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
            previewFrameFactory = new ExynosCameraFrameFactoryPreviewFrontPIP(m_cameraId, m_configurations, m_parameters[m_cameraId]);
        } else {
            previewFrameFactory = new ExynosCameraFrameFactoryPreview(m_cameraId, m_configurations, m_parameters[m_cameraId]);
        }
        previewFrameFactory->setFrameCreateHandler(&ExynosCamera::m_previewFrameHandler);
        previewFrameFactory->setFrameManager(m_frameMgr);
        previewFrameFactory->setFactoryType(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW);
        m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] = previewFrameFactory;
    }

    m_previewFactoryLock.unlock();

    if (previewFrameFactory->isCreated() == false) {
        TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_CREATE_START, 0);
        ret = previewFrameFactory->create();
        TIME_LOGGER_UPDATE(m_cameraId, FRAME_FACTORY_TYPE_CAPTURE_PREVIEW, 0, CUMULATIVE_CNT, FACTORY_CREATE_END, 0);
        if (ret != NO_ERROR) {
            CLOGE("m_previewFrameFactory->create() failed");
            goto err;
        }
        CLOGD("FrameFactory(previewFrameFactory) created");
    }

    ret = m_fastenAeStable(previewFrameFactory);
    if (ret != NO_ERROR) {
        CLOGE("m_fastenAeStable() failed");
        goto err;
    }
    m_configurations->setUseFastenAeStable(false);

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FASTEN_AE_THREAD_END, 0);

    /* one shot */
    return false;

err:
    m_fastenAeThreadResult = ret;

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FASTEN_AE_THREAD_END, 0);

    return false;
}

status_t ExynosCamera::m_waitFastenAeThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FASTEN_AE_THREAD_JOIN_START, 0);

    timer.start();

    if (m_fastenAeThread != NULL) {
        m_fastenAeThread->join();
    } else {
        CLOGW("m_fastenAeThread is NULL");
    }
    m_fastEntrance = false;

    timer.stop();

    CLOGD("fastenAeThread waiting time : duration time(%5d msec)", (int)timer.durationMsecs());
    CLOGD("fastenAeThread join");

    if (m_fastenAeThreadResult < 0) {
        CLOGE("fastenAeThread join with error");
        return BAD_VALUE;
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FASTEN_AE_THREAD_JOIN_END, 0);

    return NO_ERROR;
}
#endif

#if defined(SAMSUNG_READ_ROM)
status_t ExynosCamera::m_waitReadRomThreadEnd(void)
{
    ExynosCameraDurationTimer timer;

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, READ_ROM_THREAD_JOIN_START, 0);

    CLOGI("");
    timer.start();

    if (isEEprom(getCameraId()) == true) {
        if (m_readRomThread != NULL) {
            m_readRomThread->join();
        } else {
            CLOGD("DEBUG(%s): m_readRomThread is NULL.", __FUNCTION__);
        }
    }

    timer.stop();
    CLOGI("m_readRomThread waiting time : duration time(%5d msec)", (int)timer.durationMsecs());

    CLOGI("m_readRomThread join");

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, READ_ROM_THREAD_JOIN_END, 0);

    return NO_ERROR;
}

bool ExynosCamera::m_readRomThreadFunc(void)
{
    CLOGI("");
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, READ_ROM_THREAD_START, 0);

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
    CLOGI("readrom complete. Sensor FW ver: %s", sensorFW);

err:
    if (fp != NULL)
        fclose(fp);

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, READ_ROM_THREAD_END, 0);

    /* one shot */
    return false;
}
#endif

#ifdef SAMSUNG_UNIPLUGIN
bool ExynosCamera::m_uniPluginThreadFunc(void)
{
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    int shotmode = m_configurations->getModeValue(CONFIGURATION_SHOT_MODE);

    m_timer.start();

#ifdef SAMSUNG_HIFI_CAPTURE
    if (m_configurations->getSamsungCamera() == true
        && m_configurations->getModeValue(CONFIGURATION_VT_MODE) == 0) {
        m_uniPlugInHandle[PP_SCENARIO_HIFI_LLS] = uni_plugin_load(HIFI_LLS_PLUGIN_NAME);
        if (m_uniPlugInHandle[PP_SCENARIO_HIFI_LLS] == NULL) {
            CLOGE("[LLS_MBR] LLS_MBR load failed!!");
        } else {
            CLOGD("[LLS_MBR] LLS_MBR load success!!");
        }
    }
#endif

#ifdef SAMSUNG_STR_PREVIEW
    if (m_cameraId == CAMERA_ID_FRONT && m_scenario == SCENARIO_NORMAL) {
        m_uniPlugInHandle[PP_SCENARIO_STR_PREVIEW] = uni_plugin_load(STR_PREVIEW_PLUGIN_NAME);
        if (m_uniPlugInHandle[PP_SCENARIO_STR_PREVIEW] == NULL) {
            CLOGE("[STR_PREVIEW] STR_PREVIEW load failed!!");
        } else {
            CLOGD("[STR_PREVIEW] STR_PREVIEW load success!!");
        }
    }
#endif

#ifdef SAMSUNG_IDDQD
    if (m_configurations->getSamsungCamera() == true
        && (m_scenario == SCENARIO_NORMAL || m_scenario == SCENARIO_DUAL_REAR_ZOOM)
        && (shotmode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
            || shotmode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY)) {
        m_uniPlugInHandle[PP_SCENARIO_IDDQD] = uni_plugin_load(IDDQD_PLUGIN_NAME);
        if (m_uniPlugInHandle[PP_SCENARIO_IDDQD] == NULL) {
            CLOGE("[IDDQD] IDDQD load failed!!");
        } else {
            CLOGD("[IDDQD] IDDQD load success!!");
        }
    }
#endif

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        if (m_previewSolutionHandle == NULL) {
            m_previewSolutionHandle = uni_plugin_load(DUAL_PREVIEW_PLUGIN_NAME);
            if (m_previewSolutionHandle == NULL) {
                CLOGE("[FUSION] DUAL_PREVIEW load failed!!");
            }
        }
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        if (m_captureSolutionHandle == NULL) {
            m_captureSolutionHandle = uni_plugin_load(DUAL_CAPTURE_PLUGIN_NAME);
            if (m_captureSolutionHandle == NULL) {
                CLOGE("[FUSION] DUAL_CAPTURE load failed!!");
            }
        }
#endif

        m_fusionZoomPreviewWrapper->m_setSolutionHandle(m_previewSolutionHandle, m_captureSolutionHandle);
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
        m_fusionZoomCaptureWrapper->m_setSolutionHandle(m_previewSolutionHandle, m_captureSolutionHandle);
#endif
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
    } else if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        if (m_bokehPreviewSolutionHandle == NULL) {
            m_bokehPreviewSolutionHandle = uni_plugin_load(LIVE_FOCUS_PREVIEW_PLUGIN_NAME);
            if (m_bokehPreviewSolutionHandle == NULL) {
                CLOGE("[BOKEH] PREVIEW load failed!!");
            } else {
                CLOGD("[BOKEH] PREVIEW load success!!");
            }
        }
        if (m_bokehCaptureSolutionHandle == NULL) {
            m_bokehCaptureSolutionHandle = uni_plugin_load(LIVE_FOCUS_CAPTURE_PLUGIN_NAME);
            if (m_bokehCaptureSolutionHandle == NULL) {
                CLOGE("[BOKEH] CAPTURE load failed!!");
            } else {
                CLOGD("[BOKEH] CAPTURE load success!!");
            }
        }

        m_bokehPreviewWrapper->m_setSolutionHandle(m_bokehPreviewSolutionHandle, m_bokehCaptureSolutionHandle);
        m_bokehCaptureWrapper->m_setSolutionHandle(m_bokehPreviewSolutionHandle, m_bokehCaptureSolutionHandle);
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
    }

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("duration time(%5d msec)", (int)durationTime);

    return false;
}
#endif /* SAMSUNG_UNIPLUGIN */

#ifdef SAMSUNG_SENSOR_LISTENER
bool ExynosCamera::m_getSensorListenerData(frame_handle_components_t *components)
{
    int ret = 0;
    ExynosCameraParameters *parameters = components->parameters;

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
#ifdef SAMSUNG_PROX_FLICKER
    static uint8_t proximity_error_cnt = 0;
#endif

#ifdef SAMSUNG_HRM
    if (m_uv_rayHandle != NULL) {
        ret = sensor_listener_get_data(m_uv_rayHandle, ST_UV_RAY, &m_uv_rayListenerData, false);
        if (ret < 0) {
            if ((hrm_error_cnt % 30) == 0) {
                CLOGW("skip the HRM data. ret %d", ret);
                hrm_error_cnt = 1;
            } else {
                hrm_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_HRM_MODE)) {
                parameters->m_setHRM(m_uv_rayListenerData.uv_ray.ir_data,
                        m_uv_rayListenerData.uv_ray.flicker_data,
                        m_uv_rayListenerData.uv_ray.status);
            }
        }
    }
#endif

#ifdef SAMSUNG_LIGHT_IR
    if (m_light_irHandle != NULL) {
        ret = sensor_listener_get_data(m_light_irHandle, ST_LIGHT_IR, &m_light_irListenerData, false);
        if (ret < 0) {
            if ((lightir_error_cnt % 30) == 0) {
                CLOGW("skip the Light IR data. ret %d", ret);
                lightir_error_cnt = 1;
            } else {
                lightir_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_LIGHT_IR_MODE)) {
                parameters->m_setLight_IR(m_light_irListenerData);
            }
        }
    }
#endif

#ifdef SAMSUNG_GYRO
    if (m_gyroHandle != NULL) {
        ret = sensor_listener_get_data(m_gyroHandle, ST_GYROSCOPE, &m_gyroListenerData, false);
        if (ret < 0) {
            if ((gyro_error_cnt % 30) == 0) {
                CLOGW("skip the Gyro data. ret %d", ret);
                gyro_error_cnt = 1;
            } else {
                gyro_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_GYRO_MODE)) {
                parameters->m_setGyro(m_gyroListenerData);
            }
        }
    }
#endif

#ifdef SAMSUNG_ACCELEROMETER
    if (m_accelerometerHandle != NULL) {
        ret = sensor_listener_get_data(m_accelerometerHandle, ST_ACCELEROMETER, &m_accelerometerListenerData, false);
        if (ret < 0) {
            if ((accelerometer_error_cnt % 30) == 0) {
                CLOGW("skip the Acceleration data. ret %d", ret);
                accelerometer_error_cnt = 1;
            } else {
                accelerometer_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_ACCELEROMETER_MODE)) {
                parameters->m_setAccelerometer(m_accelerometerListenerData);
            }
        }
    }
#endif

#ifdef SAMSUNG_ROTATION
    if (m_rotationHandle != NULL) {
        ret = sensor_listener_get_data(m_rotationHandle, ST_ROTATION, &m_rotationListenerData, false);
        if (ret < 0) {
            if ((rotation_error_cnt % 30) == 0) {
                CLOGW("skip the Rotation data. ret %d", ret);
                rotation_error_cnt = 1;
            } else {
                rotation_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_ROTATION_MODE)) {
                /* rotationListenerData : counterclockwise,index / DeviceOrientation : clockwise,degree  */
                int fdOrientation = 0;
                int currentCameraId = parameters->getCameraId();
                ExynosCameraActivityUCTL *uctlMgr = NULL;
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
                        CLOGW("unknown degree index. orientation %d", m_rotationListenerData.rotation.orientation);
                        orientation_Degrees = m_configurations->getModeValue(CONFIGURATION_DEVICE_ORIENTATION);
                        break;
                }

                /* Gets the FD orientation angle in degrees. Calibrate FRONT FD orientation */
                if (currentCameraId == CAMERA_ID_FRONT || currentCameraId == CAMERA_ID_SECURE) {
                    fdOrientation = (orientation_Degrees + FRONT_ROTATION + 180) % 360;
                } else {
                    fdOrientation = (orientation_Degrees + BACK_ROTATION) % 360;
                }

                uctlMgr = components->activityControl->getUCTLMgr();
                if (uctlMgr != NULL) {
                    if (uctlMgr->getDeviceRotation() != fdOrientation
                            && m_configurations->getMode(CONFIGURATION_ROTATION_MODE)) {
                        m_configurations->setModeValue(CONFIGURATION_DEVICE_ORIENTATION, orientation_Degrees);
                        m_configurations->setModeValue(CONFIGURATION_FD_ORIENTATION, orientation_Degrees);

                        uctlMgr->setDeviceRotation(m_configurations->getModeValue(CONFIGURATION_FD_ORIENTATION));
                    }
                }
            }
        }
    }
#endif

#ifdef SAMSUNG_PROX_FLICKER
    if (m_proximityHandle != NULL) {
        ret = sensor_listener_get_data(m_proximityHandle, ST_PROXIMITY_FLICKER, &m_proximityListenerData, false);
        if (ret < 0) {
            if ((proximity_error_cnt % 30) == 0) {
                CLOGW("skip the proximity data. ret %d", ret);
                proximity_error_cnt = 1;
            } else {
                proximity_error_cnt++;
            }
        } else {
            if (m_configurations->getMode(CONFIGURATION_PROX_FLICKER_MODE)) {
                parameters->setProxFlicker(m_proximityListenerData);
            }
        }
    }
#endif

    return true;
}
#endif /* SAMSUNG_SENSOR_LISTENER */

#ifdef SAMSUNG_TN_FEATURE
int ExynosCamera::m_getPortPreviewUniPP(ExynosCameraRequestSP_sprt_t request, int pp_scenario)
{
    int pp_port = -1;
    int ppPipe = MAX_PIPE_NUM;
    int streamId = -1;
    int streamIdx = -1;

    if (request == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):request is null!",
                __FUNCTION__, __LINE__);
        return pp_port;
    }

    CLOGV("scenario(%d)", pp_scenario);

    switch (pp_scenario) {
#ifdef SAMSUNG_OT
    case PP_SCENARIO_OBJECT_TRACKING:
        streamIdx = HAL_STREAM_ID_PREVIEW;
        break;
#endif
#ifdef SAMSUNG_HLV
    case PP_SCENARIO_HLV:
        streamIdx = HAL_STREAM_ID_VIDEO;
        break;
#endif
#ifdef SAMSUNG_STR_PREVIEW
    case PP_SCENARIO_STR_PREVIEW:
    {
        if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
            streamIdx = HAL_STREAM_ID_PREVIEW;
        } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
            streamIdx = HAL_STREAM_ID_CALLBACK;
        }
        break;
    }
#endif
#ifdef SAMSUNG_FOCUS_PEAKING
    case PP_SCENARIO_FOCUS_PEAKING:
        streamIdx = HAL_STREAM_ID_PREVIEW;
        break;
#endif
#ifdef SAMSUNG_IDDQD
    case PP_SCENARIO_IDDQD:
        if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
            streamIdx = HAL_STREAM_ID_PREVIEW;
        } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
            streamIdx = HAL_STREAM_ID_CALLBACK;
        }
        break;
#endif
#ifdef SAMSUNG_SW_VDIS
    case PP_SCENARIO_SW_VDIS:
        streamIdx = HAL_STREAM_ID_PREVIEW;
        break;
#endif
#ifdef SAMSUNG_HYPERLAPSE
    case PP_SCENARIO_HYPERLAPSE:
        streamIdx = HAL_STREAM_ID_PREVIEW;
        break;
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    case PP_SCENARIO_HIFI_VIDEO:
        streamIdx = HAL_STREAM_ID_PREVIEW;
        break;
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
    case PP_SCENARIO_VIDEO_BEAUTY:
        if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
            streamIdx = HAL_STREAM_ID_PREVIEW;
        } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
            streamIdx = HAL_STREAM_ID_CALLBACK;
        }
        break;
#endif
    default:
        break;
    }

    if (streamIdx >= 0) {
        streamId = request->getStreamIdwithStreamIdx(streamIdx);
        pp_port = m_streamManager->getOutputPortId(streamId);
        if (pp_port != -1) {
            pp_port += PIPE_MCSC0;
        }
    }

    return pp_port;
}

status_t ExynosCamera::m_setupPreviewUniPP(ExynosCameraFrameSP_sptr_t frame,
                                           ExynosCameraRequestSP_sprt_t request,
                                           int pipeId, int subPipeId, int pipeId_next)
{
    status_t ret = NO_ERROR;
    int pp_scenario = PP_SCENARIO_NONE;
    struct camera2_stream *shot_stream = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraBuffer srcBuffer;
    ExynosRect srcRect;
    ExynosRect dstRect;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int colorFormat = V4L2_PIX_FMT_NV21M;

    if (request == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):[F%d T%d]request is null!",
                __FUNCTION__, __LINE__, frame->getFrameCount(), frame->getFrameType());

        return INVALID_OPERATION;
    }

    pp_scenario = frame->getPPScenario(pipeId_next);

    CLOGV("scenario(%d)", pp_scenario);

    ret = frame->getDstBuffer(pipeId, &dstBuffer, factory->getNodeType(subPipeId));
    if (ret != NO_ERROR || dstBuffer.index < 0) {
        CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                subPipeId, dstBuffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    shot_stream = (struct camera2_stream *)(dstBuffer.addr[dstBuffer.getMetaPlaneIndex()]);
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

    switch (pp_scenario) {
#ifdef SAMSUNG_OT
    case PP_SCENARIO_OBJECT_TRACKING:
        srcRect.x       = shot_stream->output_crop_region[0];
        srcRect.y       = shot_stream->output_crop_region[1];
        srcRect.w       = shot_stream->output_crop_region[2];
        srcRect.h       = shot_stream->output_crop_region[3];
        srcRect.fullW   = shot_stream->output_crop_region[2];
        srcRect.fullH   = shot_stream->output_crop_region[3];
        srcRect.colorFormat = colorFormat;

        ret = frame->setSrcRect(pipeId_next, srcRect);
        if (ret != NO_ERROR) {
            CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                    pipeId_next, frame->getFrameCount(), ret);
        }

        srcBuffer = dstBuffer;
        ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
        }

        dstRect.x = shot_stream->output_crop_region[0];
        dstRect.y = shot_stream->output_crop_region[1];
        dstRect.w = shot_stream->output_crop_region[2];
        dstRect.h = shot_stream->output_crop_region[3];
        dstRect.fullW = shot_stream->output_crop_region[2];
        dstRect.fullH = shot_stream->output_crop_region[3];
        dstRect.colorFormat = colorFormat;

        shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
        break;
#endif
#ifdef SAMSUNG_HLV
    case PP_SCENARIO_HLV:
        srcRect.x       = shot_stream->output_crop_region[0];
        srcRect.y       = shot_stream->output_crop_region[1];
        srcRect.w       = shot_stream->output_crop_region[2];
        srcRect.h       = shot_stream->output_crop_region[3];
        srcRect.fullW   = shot_stream->output_crop_region[2];
        srcRect.fullH   = shot_stream->output_crop_region[3];
        srcRect.colorFormat = colorFormat;

        ret = frame->setSrcRect(pipeId_next, srcRect);
        if (ret != NO_ERROR) {
            CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                    pipeId_next, frame->getFrameCount(), ret);
        }

        srcBuffer = dstBuffer;
        ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
        }

        dstRect.x = shot_stream->output_crop_region[0];
        dstRect.y = shot_stream->output_crop_region[1];
        dstRect.w = shot_stream->output_crop_region[2];
        dstRect.h = shot_stream->output_crop_region[3];
        dstRect.fullW = shot_stream->output_crop_region[2];
        dstRect.fullH = shot_stream->output_crop_region[3];
        dstRect.colorFormat = colorFormat;

        shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
        break;
#endif
#ifdef SAMSUNG_STR_PREVIEW
    case PP_SCENARIO_STR_PREVIEW:
        if (request != NULL) {
            if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
                colorFormat = V4L2_PIX_FMT_NV21M;
            } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
                colorFormat = V4L2_PIX_FMT_NV21;
            }
        }

        srcRect.x       = shot_stream->output_crop_region[0];
        srcRect.y       = shot_stream->output_crop_region[1];
        srcRect.w       = shot_stream->output_crop_region[2];
        srcRect.h       = shot_stream->output_crop_region[3];
        srcRect.fullW   = shot_stream->output_crop_region[2];
        srcRect.fullH   = shot_stream->output_crop_region[3];
        srcRect.colorFormat = colorFormat;

        ret = frame->setSrcRect(pipeId_next, srcRect);
        if (ret != NO_ERROR) {
            CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                    pipeId_next, frame->getFrameCount(), ret);
        }

        srcBuffer = dstBuffer;
        ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
        }

        dstRect.x = shot_stream->output_crop_region[0];
        dstRect.y = shot_stream->output_crop_region[1];
        dstRect.w = shot_stream->output_crop_region[2];
        dstRect.h = shot_stream->output_crop_region[3];
        dstRect.fullW = shot_stream->output_crop_region[2];
        dstRect.fullH = shot_stream->output_crop_region[3];
        dstRect.colorFormat = colorFormat;

        shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
        break;
#endif
#ifdef SAMSUNG_FOCUS_PEAKING
    case PP_SCENARIO_FOCUS_PEAKING:
        {
            int depthW = 0, depthH = 0;
            int pipeDepthId = m_getBayerPipeId();
            m_parameters[m_cameraId]->getDepthMapSize(&depthW, &depthH);

            srcRect.x       = 0;
            srcRect.y       = 0;
            srcRect.w       = depthW;
            srcRect.h       = depthH;
            srcRect.fullW   = depthW;
            srcRect.fullH   = depthH;
            srcRect.colorFormat = DEPTH_MAP_FORMAT;

            ret = frame->setSrcRect(pipeId_next, srcRect);
            if (ret != NO_ERROR) {
                CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                        pipeId_next, frame->getFrameCount(), ret);
            }

            ret = frame->getDstBuffer(pipeDepthId, &srcBuffer, factory->getNodeType(PIPE_VC1));
            if (request != NULL) {
                if (ret != NO_ERROR) {
                    CLOGE("[R%d F%d]Failed to get DepthMap buffer. ret %d",
                            request->getKey(), frame->getFrameCount(), ret);
                }
            }

            ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            }

            dstRect.x = shot_stream->output_crop_region[0];
            dstRect.y = shot_stream->output_crop_region[1];
            dstRect.w = shot_stream->output_crop_region[2];
            dstRect.h = shot_stream->output_crop_region[3];
            dstRect.fullW = shot_stream->output_crop_region[2];
            dstRect.fullH = shot_stream->output_crop_region[3];
            dstRect.colorFormat = colorFormat;

            shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
            break;
        }
#endif
#ifdef SAMSUNG_IDDQD
    case PP_SCENARIO_IDDQD:
        if (request != NULL) {
            if (request->hasStream(HAL_STREAM_ID_PREVIEW)) {
                colorFormat = V4L2_PIX_FMT_NV21M;
            } else if (request->hasStream(HAL_STREAM_ID_CALLBACK)) {
                colorFormat = V4L2_PIX_FMT_NV21;
            }
        }

        srcRect.x       = shot_stream->output_crop_region[0];
        srcRect.y       = shot_stream->output_crop_region[1];
        srcRect.w       = shot_stream->output_crop_region[2];
        srcRect.h       = shot_stream->output_crop_region[3];
        srcRect.fullW   = shot_stream->output_crop_region[2];
        srcRect.fullH   = shot_stream->output_crop_region[3];
        srcRect.colorFormat = colorFormat;

        ret = frame->setSrcRect(pipeId_next, srcRect);
        if (ret != NO_ERROR) {
            CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                pipeId_next, frame->getFrameCount(), ret);
        }

        srcBuffer = dstBuffer;
        ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
        if (ret < 0) {
            CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
        }

        dstRect.x = shot_stream->output_crop_region[0];
        dstRect.y = shot_stream->output_crop_region[1];
        dstRect.w = shot_stream->output_crop_region[2];
        dstRect.h = shot_stream->output_crop_region[3];
        dstRect.fullW = shot_stream->output_crop_region[2];
        dstRect.fullH = shot_stream->output_crop_region[3];
        dstRect.colorFormat = colorFormat;

        shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
        break;
#endif
#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY)
    case PP_SCENARIO_SW_VDIS:
    case PP_SCENARIO_HYPERLAPSE:
    case PP_SCENARIO_VIDEO_BEAUTY:
        {
            srcRect.x       = shot_stream->output_crop_region[0];
            srcRect.y       = shot_stream->output_crop_region[1];
            srcRect.w       = shot_stream->output_crop_region[2];
            srcRect.h       = shot_stream->output_crop_region[3];
            srcRect.fullW   = shot_stream->output_crop_region[2];
            srcRect.fullH   = shot_stream->output_crop_region[3];
            srcRect.colorFormat = colorFormat;

            ret = frame->setSrcRect(pipeId_next, srcRect);
            if (ret != NO_ERROR) {
                CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                        pipeId_next, frame->getFrameCount(), ret);
            }
            srcBuffer = dstBuffer;

            if (request->hasStream(HAL_STREAM_ID_VIDEO) == false) {
#if defined(SAMSUNG_VIDEO_BEAUTY)
                if (pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                    && frame->isLastPPScenarioPipe(pipeId_next) == true
                    && m_flagVideoStreamPriority == false) {
                    ret = m_setSrcBuffer(pipeId_next, frame, &srcBuffer);
                    if (ret < 0) {
                        CLOGE("m_setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                    }
                } else
#endif
                {
                    ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
                    if (ret < 0) {
                        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                    }
                }

                shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
            } else
#if defined(SAMSUNG_VIDEO_BEAUTY) && defined(SAMSUNG_SW_VDIS)
            if (pipeId_next == PIPE_PP_UNI
                && pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                && frame->getRequest(PIPE_PP_UNI2) == true
                && frame->getPPScenario(PIPE_PP_UNI2) == PP_SCENARIO_SW_VDIS) {
                CLOGV("[BEAUTY_DBG] reuse MCSC buffer R%d F%d", request->getKey(), frame->getFrameCount());
                ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }

                shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
            } else
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY) && defined(SAMSUNG_HIFI_VIDEO)
            if (pipeId_next == PIPE_PP_UNI2
                && pp_scenario == PP_SCENARIO_VIDEO_BEAUTY
                && frame->getRequest(PIPE_PP_UNI3) == true
                && frame->getPPScenario(PIPE_PP_UNI3) == PP_SCENARIO_SW_VDIS) {
                CLOGV("[BEAUTY_DBG] reuse MCSC buffer R%d F%d", request->getKey(), frame->getFrameCount());
                ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }

                shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
            } else
#endif
            {
                ExynosCameraBuffer outBuffer;
                ret = frame->getDstBuffer(pipeId_next, &outBuffer, factory->getNodeType(pipeId_next));
                if (ret != NO_ERROR) {
                    CLOGE("m_setupPreviewUniPP: Failed to get Output Buffer. ret %d", ret);
                    m_bufferSupplier->putBuffer(srcBuffer);
                    return ret;
                }

                ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &outBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }

                shot_ext = (struct camera2_shot_ext *) outBuffer.addr[outBuffer.getMetaPlaneIndex()];

                CLOGV("VDIS DEBUG: frameCount %d,  src %d %d, out %d %d)", frame->getFrameCount(),
                    srcBuffer.index, srcBuffer.fd[0], outBuffer.index, outBuffer.fd[0]);
            }

            int yuvSizeW = shot_stream->output_crop_region[2];
            int yuvSizeH = shot_stream->output_crop_region[3];
#ifdef SAMSUNG_SW_VDIS
            if (pp_scenario == PP_SCENARIO_SW_VDIS) {
                uint32_t minFps, maxFps;
                m_configurations->getPreviewFpsRange(&minFps, &maxFps);
                m_parameters[m_cameraId]->getSWVdisAdjustYuvSize(&yuvSizeW, &yuvSizeH, (int)maxFps);
            }
#endif
#ifdef SAMSUNG_HYPERLAPSE
            if (pp_scenario == PP_SCENARIO_HYPERLAPSE) {
                uint32_t minFps, maxFps;
                m_configurations->getPreviewFpsRange(&minFps, &maxFps);
                m_parameters[m_cameraId]->getHyperlapseAdjustYuvSize(&yuvSizeW, &yuvSizeH, (int)maxFps);
            }
#endif

            dstRect.x = shot_stream->output_crop_region[0];
            dstRect.y = shot_stream->output_crop_region[1];
            dstRect.w = yuvSizeW;
            dstRect.h = yuvSizeH;
            dstRect.fullW = yuvSizeW;
            dstRect.fullH = yuvSizeH;
            dstRect.colorFormat = colorFormat;
            break;
        }
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    case PP_SCENARIO_HIFI_VIDEO:
        {
            srcRect.x = shot_stream->output_crop_region[0];
            srcRect.y = shot_stream->output_crop_region[1];
            srcRect.w = shot_stream->output_crop_region[2];
            srcRect.h = shot_stream->output_crop_region[3];
#ifdef HIFIVIDEO_ZOOM_SUPPORTED
            m_parameters[m_cameraId]->getSize(HW_INFO_HW_PREVIEW_SIZE, (uint32_t *)&srcRect.fullW, (uint32_t *)&srcRect.fullH);
#else
            srcRect.fullW   = shot_stream->output_crop_region[2];
            srcRect.fullH   = shot_stream->output_crop_region[3];
#endif
            srcRect.colorFormat = colorFormat;
#ifdef HIFIVIDEO_ZOOM_SUPPORTED
            /* Update based on Zoom Operation in HiFiVideo */
            shot_stream->output_crop_region[0] = 0;
            shot_stream->output_crop_region[1] = 0;
            shot_stream->output_crop_region[2] = srcRect.fullW;
            shot_stream->output_crop_region[3] = srcRect.fullH;
#endif
            ret = frame->setSrcRect(pipeId_next, srcRect);
            if (ret != NO_ERROR) {
                CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                        pipeId_next, frame->getFrameCount(), ret);
            }
            srcBuffer = dstBuffer;

            struct camera2_shot_ext *shot_ext_src = (struct camera2_shot_ext *) srcBuffer.addr[srcBuffer.getMetaPlaneIndex()];
            /* Get perFrame zoomRatio & BayerCrop Info */
            if (shot_ext_src != NULL) {
                frame->getNodeGroupInfo(&shot_ext_src->node_group, PERFRAME_INFO_FLITE, &shot_ext_src->shot.udm.zoomRatio);
            }

            /*
             * m_flagVideoStreamPriority : false / true
             * HAL_STREAM_ID_PREVIEW     : PIPE_PP_UNI / PIPE_GSC
             * HAL_STREAM_ID_VIDEO       : PIPE_GSC / PIPE_PP_UNI
             */
#ifdef HIFIVIDEO_INPUTCOPY_DISABLE
            ExynosCameraBuffer outBuffer;
            if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                || frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true
                || (m_flagVideoStreamPriority == true && request->hasStream(HAL_STREAM_ID_VIDEO) == false)) {
                    m_hifiVideoBufferQ->popProcessQ(&outBuffer);
            } else {
                ret = frame->getDstBuffer(pipeId_next, &outBuffer, factory->getNodeType(pipeId_next));
                if (ret != NO_ERROR) {
                    CLOGE("m_setupPreviewUniPP: Failed to get Output Buffer. ret %d", ret);
                    m_bufferSupplier->putBuffer(srcBuffer);
                    return ret;
                }

                ExynosCameraBuffer tmpBuffer;
                m_hifiVideoBufferQ->popProcessQ(&tmpBuffer);
                if (tmpBuffer.index >= 0) {
                    m_bufferSupplier->putBuffer(tmpBuffer);
                }
            }

            m_hifiVideoBufferQ->pushProcessQ(&srcBuffer);

            ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &outBuffer);
            if (ret < 0) {
                CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            }

            shot_ext = (struct camera2_shot_ext *) outBuffer.addr[outBuffer.getMetaPlaneIndex()];

            CLOGV("[HIFIVIDEO] frameCount %d srcBuffer %d %d outBuffer %d %d", frame->getFrameCount(),
                srcBuffer.index, srcBuffer.fd[0], outBuffer.index, outBuffer.fd[0]);
#else
            if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                || frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true
                || (m_flagVideoStreamPriority == true && request->hasStream(HAL_STREAM_ID_VIDEO) == false)) {
                ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &dstBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }

                shot_ext = (struct camera2_shot_ext *) dstBuffer.addr[dstBuffer.getMetaPlaneIndex()];
            } else {
                ExynosCameraBuffer outBuffer;
                ret = frame->getDstBuffer(pipeId_next, &outBuffer, factory->getNodeType(pipeId_next));
                if (ret != NO_ERROR) {
                    CLOGE("m_setupPreviewUniPP: Failed to get Output Buffer. ret %d", ret);
                    m_bufferSupplier->putBuffer(srcBuffer);
                    return ret;
                }

                ret = m_setupEntity(pipeId_next, frame, &srcBuffer, &outBuffer);
                if (ret < 0) {
                    CLOGE("setupEntity fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                }

                shot_ext = (struct camera2_shot_ext *) outBuffer.addr[outBuffer.getMetaPlaneIndex()];

                CLOGV("[HIFIVIDEO] frameCount %d srcBuffer %d %d outBuffer %d %d", frame->getFrameCount(),
                    srcBuffer.index, srcBuffer.fd[0], outBuffer.index, outBuffer.fd[0]);
            }
#endif

            int yuvSizeW = srcRect.fullW;
            int yuvSizeH = srcRect.fullH;
            if (m_configurations->getModeValue(CONFIGURATION_HIFIVIDEO_OPMODE) != HIFIVIDEO_OPMODE_HIFIONLY_PREVIEW) {
                uint32_t minFps, maxFps;
                m_configurations->getPreviewFpsRange(&minFps, &maxFps);
                m_parameters[m_cameraId]->getHiFiVideoAdjustYuvSize(&yuvSizeW, &yuvSizeH, (int)maxFps);
            }

            dstRect.x = 0;
            dstRect.y = 0;
            dstRect.w = yuvSizeW;
            dstRect.h = yuvSizeH;
            dstRect.fullW = yuvSizeW;
            dstRect.fullH = yuvSizeH;
            dstRect.colorFormat = colorFormat;
            break;
        }
#endif
    default:
        break;
    }

    if (request != NULL && shot_ext != NULL) {
        shot_ext->shot.udm.sensor.timeStampBoot = request->getSensorTimestamp();
    }

    if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF
        && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
        /* When VRA works as M2M mode, FD metadata will be updated with the latest one in Parameters */
        m_parameters[m_cameraId]->getFaceDetectMeta(shot_ext);
    }

    ret = frame->setDstRect(pipeId_next, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("setDstRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                pipeId_next, frame->getFrameCount(), ret);
        return ret;
    }

    return ret;
}

status_t ExynosCamera::m_setupCaptureUniPP(ExynosCameraFrameSP_sptr_t frame,
                                           int pipeId, int subPipeId, int pipeId_next)
{
    status_t ret = NO_ERROR;
    int pp_scenario = PP_SCENARIO_NONE;
    struct camera2_stream *shot_stream = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    ExynosCameraBuffer buffer;
    ExynosRect srcRect;
    ExynosRect dstRect;
    frame_handle_components_t components;
    int sizeW = 0, sizeH = 0;

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    pp_scenario = frame->getPPScenario(pipeId_next);

    CLOGV("scenario(%d)", pp_scenario);
    CLOGV("STREAM_TYPE_YUVCB_STALL(%d), SCENARIO_TYPE_HIFI_LLS(%d)",
        frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL),
        frame->hasPPScenario(PP_SCENARIO_HIFI_LLS)
    );

    ret = frame->getDstBuffer(pipeId, &buffer, components.reprocessingFactory->getNodeType(subPipeId));
    if (ret != NO_ERROR || buffer.index < 0) {
        CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                subPipeId, buffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    shot_stream = (struct camera2_stream *)(buffer.addr[buffer.getMetaPlaneIndex()]);
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

    switch (pp_scenario) {
#ifdef SAMSUNG_HIFI_CAPTURE
        case PP_SCENARIO_HIFI_LLS:
            {
                int pictureW = 0, pictureH = 0;

                srcRect.x       = shot_stream->output_crop_region[0];
                srcRect.y       = shot_stream->output_crop_region[1];
                srcRect.w       = shot_stream->output_crop_region[2];
                srcRect.h       = shot_stream->output_crop_region[3];
                srcRect.fullW   = shot_stream->output_crop_region[2];
                srcRect.fullH   = shot_stream->output_crop_region[3];
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);
                dstRect.x       = 0;
                dstRect.y       = 0;
                dstRect.w       = pictureW;
                dstRect.h       = pictureH;
                dstRect.fullW   = pictureW;
                dstRect.fullH   = pictureH;
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            }
            break;
#endif
#ifdef SAMSUNG_STR_CAPTURE
        case PP_SCENARIO_STR_CAPTURE:
            if (pipeId_next == PIPE_PP_UNI_REPROCESSING2) {
                int pictureW = 0, pictureH = 0;

                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);

                srcRect.x       = 0;
                srcRect.y       = 0;
                srcRect.w       = pictureW;
                srcRect.h       = pictureH;
                srcRect.fullW   = pictureW;
                srcRect.fullH   = pictureH;
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                dstRect.x       = 0;
                dstRect.y       = 0;
                dstRect.w       = pictureW;
                dstRect.h       = pictureH;
                dstRect.fullW   = pictureW;
                dstRect.fullH   = pictureH;
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            } else {
                srcRect.x       = shot_stream->output_crop_region[0];
                srcRect.y       = shot_stream->output_crop_region[1];
                srcRect.w       = shot_stream->output_crop_region[2];
                srcRect.h       = shot_stream->output_crop_region[3];
                srcRect.fullW   = shot_stream->output_crop_region[2];
                srcRect.fullH   = shot_stream->output_crop_region[3];
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                dstRect.x       = shot_stream->output_crop_region[0];
                dstRect.y       = shot_stream->output_crop_region[1];
                dstRect.w       = shot_stream->output_crop_region[2];
                dstRect.h       = shot_stream->output_crop_region[3];
                dstRect.fullW   = shot_stream->output_crop_region[2];
                dstRect.fullH   = shot_stream->output_crop_region[3];
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            }
            break;

#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
        case PP_SCENARIO_VIDEO_BEAUTY_SNAPSHOT:
            if (pipeId_next == PIPE_PP_UNI_REPROCESSING2) {
                int pictureW = 0, pictureH = 0;

                m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);

                srcRect.x       = 0;
                srcRect.y       = 0;
                srcRect.w       = pictureW;
                srcRect.h       = pictureH;
                srcRect.fullW   = pictureW;
                srcRect.fullH   = pictureH;
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                dstRect.x       = 0;
                dstRect.y       = 0;
                dstRect.w       = pictureW;
                dstRect.h       = pictureH;
                dstRect.fullW   = pictureW;
                dstRect.fullH   = pictureH;
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            } else {
                srcRect.x       = shot_stream->output_crop_region[0];
                srcRect.y       = shot_stream->output_crop_region[1];
                srcRect.w       = shot_stream->output_crop_region[2];
                srcRect.h       = shot_stream->output_crop_region[3];
                srcRect.fullW   = shot_stream->output_crop_region[2];
                srcRect.fullH   = shot_stream->output_crop_region[3];
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                dstRect.x       = shot_stream->output_crop_region[0];
                dstRect.y       = shot_stream->output_crop_region[1];
                dstRect.w       = shot_stream->output_crop_region[2];
                dstRect.h       = shot_stream->output_crop_region[3];
                dstRect.fullW   = shot_stream->output_crop_region[2];
                dstRect.fullH   = shot_stream->output_crop_region[3];
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            }
            break;

#endif
#ifdef SAMSUNG_LLS_DEBLUR
        case PP_SCENARIO_LLS_DEBLUR:
#endif
#ifdef SAMSUNG_MFHDR_CAPTURE
        case PP_SCENARIO_MF_HDR:
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        case PP_SCENARIO_LL_HDR:
#endif
        default:
            {
                srcRect.x       = shot_stream->output_crop_region[0];
                srcRect.y       = shot_stream->output_crop_region[1];
                srcRect.w       = shot_stream->output_crop_region[2];
                srcRect.h       = shot_stream->output_crop_region[3];
                srcRect.fullW   = shot_stream->output_crop_region[2];
                srcRect.fullH   = shot_stream->output_crop_region[3];
                srcRect.colorFormat = V4L2_PIX_FMT_NV21;

                dstRect.x       = shot_stream->output_crop_region[0];
                dstRect.y       = shot_stream->output_crop_region[1];
                dstRect.w       = shot_stream->output_crop_region[2];
                dstRect.h       = shot_stream->output_crop_region[3];
                dstRect.fullW   = shot_stream->output_crop_region[2];
                dstRect.fullH   = shot_stream->output_crop_region[3];
                dstRect.colorFormat = V4L2_PIX_FMT_NV21;
            }
            break;
    }

    shot_ext = (struct camera2_shot_ext *)(buffer.addr[buffer.getMetaPlaneIndex()]);

    ret = frame->getMetaData(shot_ext);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
    }

#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
    if (shot_ext->shot.ctl.stats.faceDetectMode > FACEDETECT_MODE_OFF
        && components.parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
        /* When VRA works as M2M mode, FD metadata will be updated with the latest one in Parameters */
        components.parameters->getFaceDetectMeta(shot_ext);
    }
#endif

    CLOGV("src size(%dx%d), dst size(%dx%d)", srcRect.w, srcRect.h, dstRect.w, dstRect.h);

    ret = frame->setSrcRect(pipeId_next, srcRect);
    if (ret != NO_ERROR) {
        CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
            pipeId_next, frame->getFrameCount(), ret);
        return ret;
    }

    ret = frame->setDstRect(pipeId_next, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("setDstRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                pipeId_next, frame->getFrameCount(), ret);
        return ret;
    }

#ifdef SAMSUNG_HIFI_CAPTURE
    /* The last Frame on HIFI and NV21 scenario can have STREAM_TYP_YUVCB_STALL only requested. */
    if (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) == true
        && (pp_scenario == PP_SCENARIO_HIFI_LLS
#ifdef SAMSUNG_MFHDR_CAPTURE
            || pp_scenario == PP_SCENARIO_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
            || pp_scenario == PP_SCENARIO_LL_HDR
#endif
        )) {
        entity_buffer_state_t entityBufferState;

        ret = frame->getSrcBufferState(pipeId_next, &entityBufferState);
        if (ret < 0) {
            CLOGE("getSrcBufferState fail, pipeId(%d), ret(%d)", pipeId_next, ret);
            return ret;
        }

        if (entityBufferState == ENTITY_BUFFER_STATE_REQUESTED) {
            ret = m_setSrcBuffer(pipeId_next, frame, &buffer);
            if (ret < 0) {
                CLOGE("m_setSrcBuffer fail, pipeId(%d), ret(%d)", pipeId_next, ret);
                return ret;
            }
        }

        ret = frame->setEntityState(pipeId_next, ENTITY_STATE_PROCESSING);
        if (ret < 0) {
            CLOGE("setEntityState(ENTITY_STATE_PROCESSING) fail, pipeId(%d), ret(%d)",
                 pipeId_next, ret);
            return ret;
        }

        if (frame->getRequest(PIPE_PP_UNI_REPROCESSING2) == true) {
            ExynosCameraBuffer dstBuffer;
            ret = frame->getDstBuffer(pipeId_next, &dstBuffer, components.reprocessingFactory->getNodeType(pipeId_next));
            memcpy(dstBuffer.addr[dstBuffer.getMetaPlaneIndex()],
                    buffer.addr[buffer.getMetaPlaneIndex()], sizeof(struct camera2_shot_ext));
        }
    } else
#endif
    {
        ret = m_setupEntity(pipeId_next, frame, &buffer, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("setupEntity failed, pipeId(%d), ret(%d)", pipeId_next, ret);
            return ret;
        }
    }

    return ret;
}

int ExynosCamera::m_connectPreviewUniPP(ExynosCameraRequestSP_sprt_t request,
                                                    ExynosCameraFrameFactory *targetfactory)
{
    int pipePPScenario[MAX_PIPE_UNI_NUM];
    int pipeUniNum = 0;
    int pp_port = -1;
    PPConnectionInfo_t PPStage[PP_STAGE_MAX];
    ExynosCameraFrameFactory *controlfactory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        pipePPScenario[i] = PP_SCENARIO_NONE;
    }

    for (int i = 0; i < PP_STAGE_MAX; i++) {
        PPStage[i].scenario = PP_SCENARIO_NONE;
        PPStage[i].type = PP_CONNECTION_NORMAL;
    }

/* Check 1st stage PP scenario */
#ifdef SAMSUNG_IDDQD
    if (m_configurations->getDynamicMode(DYNAMIC_IDDQD_MODE)) {
        PPStage[PP_STAGE_0].scenario = PP_SCENARIO_IDDQD;
    }
#endif

/* Check 2nd stage PP scenario */
#ifdef SAMSUNG_OT
    if (m_configurations->getMode(CONFIGURATION_OBJECT_TRACKING_MODE) == true) {
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_OBJECT_TRACKING;
    }
#endif

#ifdef SAMSUNG_HLV
    if (m_configurations->getHLVEnable(m_recordingEnabled)) {
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_HLV;
    }
#endif

#ifdef SAMSUNG_FOCUS_PEAKING
    if (m_configurations->getModeValue(CONFIGURATION_TRANSIENT_ACTION_MODE) == TRANSIENT_ACTION_MANUAL_FOCUSING
#ifdef SUPPORT_DEPTH_MAP
        && m_parameters[m_cameraId]->isDepthMapSupported() == true
#endif
        ) {
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_FOCUS_PEAKING;
    }
#endif

#ifdef SAMSUNG_STR_PREVIEW
    pp_port = m_getPortPreviewUniPP(request, PP_SCENARIO_STR_PREVIEW);
    if (pp_port > 0
        && targetfactory->getRequest(pp_port)
        && m_parameters[m_cameraId]->getSTRPreviewEnable()) {
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_STR_PREVIEW;
    }
#endif

#if defined(SAMSUNG_HIFI_VIDEO) && defined(SAMSUNG_VIDEO_BEAUTY) && defined(SAMSUNG_SW_VDIS)
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true
        && m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true
        && m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
            PPStage[PP_STAGE_0].scenario = PP_SCENARIO_HIFI_VIDEO;
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_VIDEO_BEAUTY;
            PPStage[PP_STAGE_2].scenario = PP_SCENARIO_SW_VDIS;
    } else if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true
        && m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
            PPStage[PP_STAGE_0].scenario = PP_SCENARIO_HIFI_VIDEO;
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_SW_VDIS;
    } else if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true
        && m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
            PPStage[PP_STAGE_0].scenario = PP_SCENARIO_HIFI_VIDEO;
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_VIDEO_BEAUTY;
    } else
#endif
#if defined(SAMSUNG_SW_VDIS) && defined(SAMSUNG_VIDEO_BEAUTY)
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true
        && m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
        PPStage[PP_STAGE_0].scenario = PP_SCENARIO_VIDEO_BEAUTY;
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_SW_VDIS;
    } else
#endif
    {
#ifdef SAMSUNG_SW_VDIS
        if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_SW_VDIS;
        }
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_HIFI_VIDEO;
        }
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
        if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
            PPStage[PP_STAGE_1].scenario = PP_SCENARIO_VIDEO_BEAUTY;
        }
#endif
    }

#ifdef SAMSUNG_HYPERLAPSE
    if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
        PPStage[PP_STAGE_1].scenario = PP_SCENARIO_HYPERLAPSE;

        if (m_recordingEnabled == false) {
            PPStage[PP_STAGE_1].type = PP_CONNECTION_ONLY;
        } else {
            PPStage[PP_STAGE_1].type = PP_CONNECTION_NORMAL;
        }
    }
#endif

    for (int i = 0; i < PP_STAGE_MAX; i++) {
        if (PPStage[i].scenario != PP_SCENARIO_NONE) {
            if (m_pipePPPreviewStart[pipeUniNum] == true) {
                int currentPPScenario = controlfactory->getPPScenario(PIPE_PP_UNI + pipeUniNum);

                if (currentPPScenario != PPStage[i].scenario || PPStage[i].type == PP_CONNECTION_ONLY) {
                    bool supend_stop = false;

                    for (int j = i; j < PP_STAGE_MAX; j++) {
                        supend_stop |= (currentPPScenario == PPStage[j].scenario);
                    }
                    CLOGD("PIPE_PP_UNI(%d) stop!", controlfactory->getPPScenario(PIPE_PP_UNI + pipeUniNum));
                    m_pipePPPreviewStart[pipeUniNum] = false;
                    controlfactory->stopPPScenario(PIPE_PP_UNI + pipeUniNum, supend_stop);
                }
            }

            controlfactory->connectPPScenario(PIPE_PP_UNI + pipeUniNum, PPStage[i].scenario);
            targetfactory->setRequest(PIPE_PP_UNI + pipeUniNum, true);

            if (m_pipePPPreviewStart[pipeUniNum] == false && PPStage[i].type != PP_CONNECTION_ONLY) {
#ifdef SAMSUNG_SW_VDIS_USE_OIS
                if (PPStage[i].scenario == PP_SCENARIO_SW_VDIS) {
                    struct swVdisExtCtrlInfo vdisExtInfo;
                    vdisExtInfo.pParams = (void *)m_parameters[m_cameraId];
#ifdef USE_DUAL_CAMERA
                    vdisExtInfo.pParams1 = (void *)m_parameters[m_cameraIds[1]];
#endif
                    controlfactory->extControl(PIPE_PP_UNI, PP_SCENARIO_SW_VDIS, PP_EXT_CONTROL_VDIS, &vdisExtInfo);
                }
#endif
                CLOGD("PIPE_PP(%d) start!", PPStage[i].scenario);
                m_pipePPPreviewStart[pipeUniNum] = true;
                controlfactory->setThreadOneShotMode(PIPE_PP_UNI + pipeUniNum, false);
                controlfactory->startPPScenario(PIPE_PP_UNI + pipeUniNum);
            }

            pipePPScenario[pipeUniNum] = PPStage[i].scenario;
            pipeUniNum++;
        }
    }

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        if (pipePPScenario[i] == PP_SCENARIO_NONE
            && m_pipePPPreviewStart[i] == true) {
            CLOGD("PIPE_PP(%d) stop!", controlfactory->getPPScenario(PIPE_PP_UNI + i));
            m_pipePPPreviewStart[i] = false;
            controlfactory->setThreadOneShotMode(PIPE_PP_UNI + i, true);
            controlfactory->stopPPScenario(PIPE_PP_UNI + i);
        }
    }

    return pipeUniNum;
}

int ExynosCamera::m_connectCaptureUniPP(ExynosCameraFrameFactory *targetfactory)
{
    int pipePPScenario[MAX_PIPE_UNI_NUM];
    int pipeUniNum = 0;

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        pipePPScenario[i] = PP_SCENARIO_NONE;
    }

#ifdef SAMSUNG_LLS_DEBLUR
    if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
        && (m_scenario != SCENARIO_DUAL_REAR_PORTRAIT && m_scenario != SCENARIO_DUAL_FRONT_PORTRAIT)
#endif
    ) {
#ifdef SAMSUNG_MFHDR_CAPTURE
        if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) == MULTI_SHOT_MODE_MF_HDR) {
            pipePPScenario[pipeUniNum] = PP_SCENARIO_MF_HDR;
        } else
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
        if (m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_MODE) == MULTI_SHOT_MODE_LL_HDR) {
            pipePPScenario[pipeUniNum] = PP_SCENARIO_LL_HDR;
        } else
#endif
        {
#ifdef SAMSUNG_HIFI_CAPTURE
            pipePPScenario[pipeUniNum] = PP_SCENARIO_HIFI_LLS;
#else
            pipePPScenario[pipeUniNum] = PP_SCENARIO_LLS_DEBLUR;
#endif
        }
    }
#endif

#ifdef SAMSUNG_BD
    if (m_configurations->getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_BURST) {
        pipePPScenario[pipeUniNum] = PP_SCENARIO_BLUR_DETECTION;
    }
#endif

    if (pipePPScenario[pipeUniNum] != PP_SCENARIO_NONE) {
        if (m_pipePPReprocessingStart[pipeUniNum] == true
            && targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum) != pipePPScenario[pipeUniNum]) {
            CLOGD("PIPE_PP_UNI_REPROCESSING pipeId(%d), scenario(%d) stop!",
                    PIPE_PP_UNI_REPROCESSING + pipeUniNum,
                    targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum));
            m_pipePPReprocessingStart[pipeUniNum] = false;
            targetfactory->stopPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum);
        }

        targetfactory->connectPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum, pipePPScenario[pipeUniNum]);
        targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING + pipeUniNum, true);

#ifdef SAMSUNG_BD
        if (pipePPScenario[pipeUniNum] == PP_SCENARIO_BLUR_DETECTION) {
            int pipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
            targetfactory->setRequest(pipeId, true);
        }
#endif

        if (m_pipePPReprocessingStart[pipeUniNum] == false) {
            CLOGD("PIPE_PP_UNI_REPROCESSING pipeId(%d), scenario(%d) start!",
                PIPE_PP_UNI_REPROCESSING + pipeUniNum,
                pipePPScenario[pipeUniNum]);
            m_pipePPReprocessingStart[pipeUniNum]  = true;
            targetfactory->startPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum);
        }
        pipeUniNum++;
    }

#ifdef SAMSUNG_STR_CAPTURE
    if (m_configurations->getMode(CONFIGURATION_STR_CAPTURE_MODE) == true) {
        pipePPScenario[pipeUniNum] = PP_SCENARIO_STR_CAPTURE;
    }
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
    if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE) == true) {
        pipePPScenario[pipeUniNum] = PP_SCENARIO_VIDEO_BEAUTY_SNAPSHOT;
    }
#endif

    if (pipePPScenario[pipeUniNum] != PP_SCENARIO_NONE) {
        if (m_pipePPReprocessingStart[pipeUniNum] == true
            && targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum) != pipePPScenario[pipeUniNum]) {
            CLOGD("PIPE_PP_UNI_REPROCESSING pipeId(%d), scenario(%d) stop!",
                    PIPE_PP_UNI_REPROCESSING + pipeUniNum,
                    targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum));
            m_pipePPReprocessingStart[pipeUniNum] = false;
            targetfactory->stopPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum);
        }

        targetfactory->connectPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum, pipePPScenario[pipeUniNum]);

        if (pipeUniNum == 0) {
            targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING + pipeUniNum, true);
        }

        if (m_pipePPReprocessingStart[pipeUniNum]  == false) {
            CLOGD("PIPE_PP_UNI_REPROCESSING pipeId(%d), scenario(%d) start!",
                PIPE_PP_UNI_REPROCESSING + pipeUniNum,
                pipePPScenario[pipeUniNum]);
            m_pipePPReprocessingStart[pipeUniNum]  = true;
            targetfactory->startPPScenario(PIPE_PP_UNI_REPROCESSING + pipeUniNum);
        }
        pipeUniNum++;
    }

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        if (pipePPScenario[i] == PP_SCENARIO_NONE
            && m_pipePPReprocessingStart[i] == true) {
            CLOGD("PIPE_PP_UNI_REPROCESSING pipeId(%d), scenario(%d) stop!",
                   PIPE_PP_UNI_REPROCESSING + i, targetfactory->getPPScenario(PIPE_PP_UNI_REPROCESSING + i));
            m_pipePPReprocessingStart[i]  = false;
            targetfactory->stopPPScenario(PIPE_PP_UNI_REPROCESSING + i);
            targetfactory->setRequest(PIPE_PP_UNI_REPROCESSING + i, false);
        }
    }

    return pipeUniNum;
}

status_t ExynosCamera::m_initUniPP(void)
{
    CLOGI("IN");

    if (m_uniPluginThread != NULL) {
        m_uniPluginThread->join();
    }

    if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] != NULL) {
#ifdef SAMSUNG_IDDQD
        if (m_uniPlugInHandle[PP_SCENARIO_IDDQD] != NULL) {
            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->extControl(PIPE_PP_UNI,
                PP_SCENARIO_IDDQD, PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE,
                m_uniPlugInHandle[PP_SCENARIO_IDDQD]);
        }
#endif

#ifdef SAMSUNG_STR_PREVIEW
        if (m_uniPlugInHandle[PP_SCENARIO_STR_PREVIEW] != NULL) {
            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->extControl(PIPE_PP_UNI,
                PP_SCENARIO_STR_PREVIEW, PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE,
                m_uniPlugInHandle[PP_SCENARIO_STR_PREVIEW]);
        }
#endif
    }

    if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL
        && m_videoStreamExist == false) {
#ifdef SAMSUNG_HIFI_CAPTURE
        if (m_uniPlugInHandle[PP_SCENARIO_HIFI_LLS] != NULL) {
            m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->extControl(PIPE_PP_UNI_REPROCESSING,
                PP_SCENARIO_HIFI_LLS, PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE,
                m_uniPlugInHandle[PP_SCENARIO_HIFI_LLS]);

            int powerOn = 1;
            m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->extControl(PIPE_PP_UNI_REPROCESSING,
                PP_SCENARIO_HIFI_LLS, PP_EXT_CONTROL_POWER_CONTROL, &powerOn);

            m_hifiLLSPowerOn = true;
        }
#endif
    }

    CLOGI("OUT");

    return NO_ERROR;
}

status_t ExynosCamera::m_deinitUniPP(void)
{
    if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL) {
#ifdef SAMSUNG_HIFI_CAPTURE
        if (m_uniPlugInHandle[PP_SCENARIO_HIFI_LLS] != NULL
            && m_hifiLLSPowerOn == true) {
            int powerOff = 0;
            m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->extControl(PIPE_PP_UNI_REPROCESSING,
                PP_SCENARIO_HIFI_LLS, PP_EXT_CONTROL_POWER_CONTROL, &powerOff);

            m_hifiLLSPowerOn = false;
        }
#endif
    }

    for (int i = 0; i < MAX_PIPE_UNI_NUM; i++) {
        CLOGD("PIPE_UNI_NUM(%d) need stop(%d)", i, m_pipePPPreviewStart[i]);
        if (m_pipePPPreviewStart[i] == true) {
            if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] != NULL) {
                /* UniPP scenario should be cleaned up in PreviewFrameFactory */
                int pp_scenario = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->getPPScenario(PIPE_PP_UNI + i);
                if (pp_scenario != PP_SCENARIO_OBJECT_TRACKING) {
                    CLOGD("PIPE_PP(%d) stop!", pp_scenario);
                    m_pipePPPreviewStart[i] = false;
                    m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->setThreadOneShotMode(PIPE_PP_UNI + i, true);
                    m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->stopPPScenario(PIPE_PP_UNI + i);
                }
            }
        }

        if (m_pipePPReprocessingStart[i] == true) {
            m_pipePPReprocessingStart[i]  = false;
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] != NULL) {
                m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING]->stopPPScenario(PIPE_PP_UNI_REPROCESSING + i);
            }

#ifdef USE_DUAL_CAMERA
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] != NULL) {
                m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]->stopPPScenario(PIPE_PP_UNI_REPROCESSING + i);
            }
#endif
        }
    }

    return NO_ERROR;
}
#endif /* SAMSUNG_TN_FEATURE */

#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE) || defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_HIFI_VIDEO)
status_t ExynosCamera::m_setupPreviewGSC(ExynosCameraFrameSP_sptr_t frame,
                                            ExynosCameraRequestSP_sprt_t request,
                                            int pipeId, int subPipeId,
                                            int pp_scenario)
{
    status_t ret = NO_ERROR;
    int gscPipeId = PIPE_GSC;

    struct camera2_stream *shot_stream = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraBuffer srcBuffer;
    ExynosRect srcRect;
    ExynosRect dstRect;
    frame_handle_components_t components;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        /* TODO: doing exception handling */
        return BAD_VALUE;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    ExynosCameraFrameFactory *factory = components.previewFactory;

    int colorFormat = V4L2_PIX_FMT_NV21M;
    int portId = components.parameters->getPreviewPortId();

    int inYuvSizeW = 0, inYuvSizeH = 0;
    int outYuvSizeW = 0, outYuvSizeH = 0;
    int dstSizeW = 0, dstSizeH = 0;
    int start_x = 0, start_y = 0;
    int offset_left = 0, offset_top = 0;

    if (m_flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
        || pp_scenario == PP_SCENARIO_SW_VDIS
#endif
#ifdef SAMSUNG_HYPERLAPSE
        || pp_scenario == PP_SCENARIO_HYPERLAPSE
#endif
    ) {
        m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&dstSizeW, (uint32_t *)&dstSizeH, portId);
    } else {
        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&dstSizeW, (uint32_t *)&dstSizeH);
    }

    //set srcBuffer
    ret = frame->getDstBuffer(pipeId, &srcBuffer, factory->getNodeType(subPipeId));
    if (ret != NO_ERROR || srcBuffer.index < 0) {
        CLOGE("Failed to get SRC buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                subPipeId, srcBuffer.index, frame->getFrameCount(), ret);
        return ret;
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

    inYuvSizeW = outYuvSizeW = shot_stream->output_crop_region[2];
    inYuvSizeH = outYuvSizeH = shot_stream->output_crop_region[3];

#if defined(SAMSUNG_SW_VDIS) || defined(SAMSUNG_HYPERLAPSE)
    if (pp_scenario == PP_SCENARIO_SW_VDIS || pp_scenario == PP_SCENARIO_HYPERLAPSE) {
        uint32_t minFps, maxFps;
        m_configurations->getPreviewFpsRange(&minFps, &maxFps);
        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&outYuvSizeW, (uint32_t *)&outYuvSizeH);
#ifdef SAMSUNG_SW_VDIS
        components.parameters->getSWVdisYuvSize(outYuvSizeW, outYuvSizeH, (int)maxFps, &inYuvSizeW, &inYuvSizeH);
#endif
        start_x = ((inYuvSizeW - outYuvSizeW) / 2) & (~0x1);
        start_y = ((inYuvSizeH - outYuvSizeH) / 2) & (~0x1);
    }

#ifdef SAMSUNG_SW_VDIS
    if (components.parameters->isSWVdisOnPreview() == true) {
        m_configurations->getSWVdisPreviewOffset(&offset_left, &offset_top);
        start_x += offset_left;
        start_y += offset_top;
    }
#endif
#endif

    srcRect.x = start_x;
    srcRect.y = start_y;
    srcRect.w = outYuvSizeW;
    srcRect.h = outYuvSizeH;
    srcRect.fullW = inYuvSizeW;
    srcRect.fullH = inYuvSizeH;
    srcRect.colorFormat = colorFormat;

    ret = frame->setSrcRect(gscPipeId, srcRect);
    if (ret != NO_ERROR) {
        CLOGE("setSrcRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                gscPipeId, frame->getFrameCount(), ret);
    }

    //set dstBuffer
    ret = frame->getDstBuffer(PIPE_GSC, &dstBuffer);
    if (ret != NO_ERROR || dstBuffer.index < 0) {
        CLOGE("GSC Failed to get Output Buffer. ret %d", ret);
        return ret;
    }

    dstRect.x = 0;
    dstRect.y = 0;
    dstRect.w = dstSizeW;
    dstRect.h = dstSizeH;
    dstRect.fullW = dstSizeW;
    dstRect.fullH = dstSizeH;
    dstRect.colorFormat = colorFormat;

    ret = frame->setDstRect(gscPipeId, dstRect);
    if (ret != NO_ERROR) {
        CLOGE("setDstRect(Pipe:%d) failed, Fcount(%d), ret(%d)",
                gscPipeId, frame->getFrameCount(), ret);
    }

    CLOGV("GSC input  buffer idx %d, addrY 0x%08x fdY %d\n", srcBuffer.index, srcBuffer.addr[0], srcBuffer.fd[0]);
    CLOGV("GSC output buffer idx %d, addrY 0x%08x fdY %d\n", dstBuffer.index, dstBuffer.addr[0], dstBuffer.fd[0]);

    ret = m_setupEntity(gscPipeId, frame, &srcBuffer, &dstBuffer);
    if (ret < 0) {
        CLOGE("setupEntity fail, pipeId(%d), ret(%d)", gscPipeId, ret);
    }

    shot_ext = (struct camera2_shot_ext *) srcBuffer.addr[srcBuffer.getMetaPlaneIndex()];
    if (request != NULL) {
        shot_ext->shot.udm.sensor.timeStampBoot = request->getSensorTimestamp();
    }

    return ret;
}

bool ExynosCamera::m_gscPreviewCbThreadFunc(void)
{
    bool loop = true;

    int dstPipeId = -1;
    int mainStreamId = 0;
    int subStreamId = 0;

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraBuffer buffer;

    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraRequestSP_sprt_t request = NULL;
    entity_buffer_state_t bufferState;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    frame_handle_components_t components;

    buffer.index = -2;

    ret = m_pipeFrameDoneQ[PIPE_GSC]->waitAndPopProcessQ(&frame);
    if (ret == TIMED_OUT && m_pipeFrameDoneQ[PIPE_GSC]->getSizeOfProcessQ() > 0) {
        return true;
    } else if (ret < 0 || ret == TIMED_OUT) {
        CLOGW("GSC Failed to waitAndPopProcessQ. ret %d", ret);
        return false;
    }

    if (frame == NULL) {
        CLOGE("GSC frame is NULL");
        return false;
    }

    getFrameHandleComponents((enum FRAME_TYPE)frame->getFrameType(), &components);
    factory = components.previewFactory;

    if (m_flagVideoStreamPriority == true
#ifdef SAMSUNG_SW_VDIS
        || frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
#endif
#ifdef SAMSUNG_HYPERLAPSE
        || frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true
#endif
    ) {
        mainStreamId = HAL_STREAM_ID_VIDEO;
        subStreamId = HAL_STREAM_ID_PREVIEW;
    } else {
        mainStreamId = HAL_STREAM_ID_PREVIEW;
        subStreamId = HAL_STREAM_ID_VIDEO;
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("GSC getRequest failed ");
        goto CLEAN;
    }

    if (request->hasStream(subStreamId) == true) {
        dstPipeId = PIPE_GSC;

        ret = frame->getDstBuffer(dstPipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("Failed to get GSC buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                dstPipeId, buffer.index, frame->getFrameCount(), ret);
            goto CLEAN;
        }

        ret = frame->getDstBufferState(dstPipeId, &bufferState);
        if (ret < 0) {
            CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", dstPipeId, ret);
            goto CLEAN;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
            CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), dstPipeId);
        }

        if (request != NULL && request->hasStream(subStreamId) == true) {
            if (subStreamId == HAL_STREAM_ID_PREVIEW
                && request->hasStream(HAL_STREAM_ID_CALLBACK) == true
#ifdef USE_DUAL_CAMERA
                && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
                ) {
                request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                m_copyPreviewCbThreadFunc(request, frame, &buffer);
            }

            request->setStreamBufferStatus(subStreamId, streamBufferState);

            ret = m_sendYuvStreamResult(request, &buffer, subStreamId, false,
                                        frame->getStreamTimestamp(), frame->getParameters());
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback."
                        " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                        dstPipeId, buffer.index, frame->getFrameCount(), subStreamId, ret);
                goto CLEAN;
            }
        } else {
            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                            frame->getFrameCount(), buffer.index, dstPipeId, ret);
                }
            }
        }
    }

    if (request->hasStream(mainStreamId) == true) {
#ifdef SAMSUNG_VIDEO_BEAUTY
        if (frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true) {
#ifdef SAMSUNG_HIFI_VIDEO
            if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true
                && frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
                dstPipeId = PIPE_PP_UNI3;
            } else if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                dstPipeId = PIPE_PP_UNI2;
            } else
#endif
#ifdef SAMSUNG_SW_VDIS
            if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
                dstPipeId = PIPE_PP_UNI2;
            } else
#endif
            {
                dstPipeId = PIPE_PP_UNI;
            }
        } else
#endif
#ifdef SAMSUNG_SW_VDIS
        if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
#ifdef SAMSUNG_HIFI_VIDEO
            if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
                dstPipeId = PIPE_PP_UNI2;
            } else
#endif
            {
                dstPipeId = PIPE_PP_UNI;
            }
        } else
#endif
#ifdef SAMSUNG_HYPERLAPSE
        if (frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true) {
            dstPipeId = PIPE_PP_UNI;
        } else
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
            dstPipeId = PIPE_PP_UNI;
        } else
#endif
        {
#ifdef USE_DUAL_CAMERA
            if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                dstPipeId = PIPE_FUSION;
            } else
#endif
            {
                dstPipeId = PIPE_PP_UNI;
            }
        }

        streamBufferState = CAMERA3_BUFFER_STATUS_OK;

        buffer.index = -2;
        ret = frame->getDstBuffer(dstPipeId, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                dstPipeId, buffer.index, frame->getFrameCount(), ret);
            goto CLEAN;
        }

        ret = frame->getDstBufferState(dstPipeId, &bufferState);
        if (ret < 0) {
            CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", dstPipeId, ret);
            goto CLEAN;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
        }

        if (request != NULL && request->hasStream(mainStreamId) == true) {
            if (mainStreamId == HAL_STREAM_ID_PREVIEW
                && request->hasStream(HAL_STREAM_ID_CALLBACK) == true
#ifdef USE_DUAL_CAMERA
                && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
#endif
                ) {
                request->setStreamBufferStatus(HAL_STREAM_ID_CALLBACK, streamBufferState);
                m_copyPreviewCbThreadFunc(request, frame, &buffer);
            }

            request->setStreamBufferStatus(mainStreamId, streamBufferState);

            ret = m_sendYuvStreamResult(request, &buffer, mainStreamId, false,
                                        frame->getStreamTimestamp(), frame->getParameters());
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback."
                        " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                        dstPipeId, buffer.index, frame->getFrameCount(), mainStreamId, ret);
                goto CLEAN;
            }
        } else {
            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                            frame->getFrameCount(), buffer.index, dstPipeId, ret);
                }
            }
        }
    }

CLEAN:
    if ((m_flagVideoStreamPriority == true && request->hasStream(HAL_STREAM_ID_VIDEO) == false)
#ifdef SAMSUNG_SW_VDIS
        || frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
#endif
#ifdef SAMSUNG_HYPERLAPSE
        || frame->hasPPScenario(PP_SCENARIO_HYPERLAPSE) == true
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
        || frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        || frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true
#endif
    ) {

        /* GSC input buffer */
        ExynosRect rect;
        int planeCount = 0;
        int pipeId;
        int nodePipeId = PIPE_MCSC0 + components.parameters->getPreviewPortId();
        ExynosCameraBuffer inBuffer;

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
        if (frame->hasPPScenario(PP_SCENARIO_HIFI_VIDEO) == true) {
            if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true
                || frame->hasPPScenario(PP_SCENARIO_VIDEO_BEAUTY) == true
                || (m_flagVideoStreamPriority == true && request->hasStream(HAL_STREAM_ID_VIDEO) == false)) {
                pipeId = PIPE_PP_UNI;
                nodePipeId = PIPE_PP_UNI;
            } else {
                return true;
            }
        } else
#endif
#ifdef USE_DUAL_CAMERA
        if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            pipeId = PIPE_FUSION;
            nodePipeId = PIPE_FUSION;
        } else
#endif
        {
            if (components.parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
                pipeId = PIPE_ISP;
            } else {
                pipeId = PIPE_3AA;
            }
        }

        inBuffer.index = -2;
        ret = frame->getDstBuffer(pipeId, &inBuffer, factory->getNodeType(nodePipeId));
        if (ret != NO_ERROR) {
            CLOGE("Failed to get GSC input buffer");
        }

#ifdef SAMSUNG_SW_VDIS
        if (frame->hasPPScenario(PP_SCENARIO_SW_VDIS) == true) {
            if (frame->getBufferDondeIndex() >= 0) {
                inBuffer.index = frame->getBufferDondeIndex();
            } else {
                inBuffer.index = -2;
            }
        }
#endif

        if (inBuffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(inBuffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to putBuffer. ret %d", ret);
            }
        }
    }

    if (frame != NULL) {
        CLOGV("GSC frame delete. framecount %d", frame->getFrameCount());
        frame = NULL;
    }

    return loop;
}
#endif /* SAMSUNG_SW_VDIS || SAMSUNG_HYPERLAPSE || SAMSUNG_VIDEO_BEAUTY || SAMSUNG_HIFI_VIDEO */

#ifdef SAMSUNG_FACTORY_DRAM_TEST
bool ExynosCamera::m_postVC0ThreadFunc(void)
{
    char *pBuf;
    int i, bufSize;
    int loop = false;
    bool result;
    status_t ret = NO_ERROR;
    ExynosCameraBuffer vcbuffer;
    camera2_stream *shot_stream = NULL;

    CLOGD("[FRS] wait VC0 buffer");
    ret = m_postVC0Q->waitAndPopProcessQ(&vcbuffer);
    if (ret < 0) {
        CLOGW("wait and pop fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto CLEAN;
    }

    CLOGD("[FRS] get VC0 buffer %llx %d", (uint64_t)vcbuffer.addr[0], (uint32_t)vcbuffer.size[0]);

    if (m_ionClient >= 0)
        exynos_ion_sync_fd(m_ionClient, vcbuffer.fd[0]);

    shot_stream = (struct camera2_stream *)(vcbuffer.addr[vcbuffer.getMetaPlaneIndex()]);

    if (m_configurations->getModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE) == FACTORY_DRAM_TEST_CEHCKING) {
        result = true;

        pBuf = vcbuffer.addr[0];
        bufSize = vcbuffer.size[0];

        if ((bufSize % 3) == 0) {
            for ( i = 0 ; i < bufSize ; i += 3 ) {
                /* compare temporarily */
                if (pBuf[i] != 0xAA || pBuf[i+1] != 0xA2 || pBuf[i+2] != 0x2A ) {
                    CLOGD("[FRS] compare fail F%d D%d - %x %x %x",
                        shot_stream->fcount, m_dramTestDoneCount, pBuf[i], pBuf[i+1], pBuf[i+2]);

                    result = false;
#if defined(DEBUG_FACTORY_DRAM_DUMP)
                    {
                        bool bRet;
                        char filePath[70];

                        memset(filePath, 0, sizeof(filePath));
                        snprintf(filePath, sizeof(filePath), "/data/media/0/DRAM_%d.raw", vcbuffer.index);

                        bRet = dumpToFile((char *)filePath, pBuf, bufSize);
                        if (bRet != true)
                            CLOGE("couldn't make a raw file");
                    }
#endif
                    break;
                }
            }
        }

        CLOGD("[FRS] compare done F%d D%d R%d", shot_stream->fcount, m_dramTestDoneCount, result);

        if (result == true) {
            m_dramTestDoneCount ++;
            if (m_dramTestDoneCount == m_parameters[m_cameraId]->getFactoryDramTestCount()) {
                CLOGD("[FRS] DRAM test is successful %d", m_dramTestDoneCount);
                m_configurations->setModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE, FACTORY_DRAM_TEST_DONE_SUCCESS);
            }
        } else {
            CLOGD("[FRS] DRAM test is failed");
            m_configurations->setModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE, FACTORY_DRAM_TEST_DONE_FAIL);

            /* prevent to Q remained buffer */
            m_dramTestQCount = m_parameters[m_cameraId]->getFactoryDramTestCount();
            m_dramTestDoneCount = m_parameters[m_cameraId]->getFactoryDramTestCount();
        }
    } else {
        CLOGD("[FRS] Drop frame, status %d", m_configurations->getModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE));
    }

    memset(vcbuffer.addr[0], 0, vcbuffer.size[0]);

    ret = m_bufferSupplier->putBuffer(vcbuffer);
    if (ret !=  NO_ERROR) {
        CLOGE("[B%d]PutBuffers failed. ret %d", vcbuffer.index, ret);
    }

    if (m_postVC0Q->getSizeOfProcessQ() > 0) {
        loop = true;
        CLOGD("[FRS] getSizeOfProcessQ = %d", m_postVC0Q->getSizeOfProcessQ());
    }
CLEAN:
    return loop;
}
#endif

#ifdef SAMSUNG_SSM
void ExynosCamera::m_setupSSMMode(ExynosCameraRequestSP_sprt_t request, frame_handle_components_t components)
{
    if (request->hasStream(HAL_STREAM_ID_VIDEO) == true) {
        ExynosCameraFrameFactory *factory = components.previewFactory;
        enum NODE_TYPE nodeType = factory->getNodeType(PIPE_FLITE);
        int ret = 0;

        if (m_configurations->getModeValue(CONFIGURATION_SSM_MODE_VALUE)
            == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_MODE_MANUAL) {
            if (m_configurations->getModeValue(CONFIGURATION_SSM_TRIGGER) == true) {
                m_configurations->setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_OPERATING);
                m_configurations->setModeValue(CONFIGURATION_SSM_TRIGGER, (int)false);
                m_SSMFirstRecordingRequest = 0;

#ifdef SSM_BIG_CORE_CONTROL
                ret = factory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, SSM_BIG_CORE_MIN_LOCK, PIPE_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);
                } else {
                    CLOGD("V4L2_CID_IS_DVFS_CLUSTER1 setControl cid(%d) value(%d)",
                        V4L2_CID_IS_DVFS_CLUSTER1, SSM_BIG_CORE_MIN_LOCK);
                }
#endif

                if (m_SSMCommand == FRS_SSM_START) {
                    m_SSMUseAutoDelayQ = true;

                    m_SSMCommand = FRS_SSM_MANUAL_CUE_ENABLE;
                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_MANUAL_CUE_ENABLE", m_SSMCommand);
                    }
                } else {
                    m_SSMUseAutoDelayQ = false;

                    if (m_configurations->getModeValue(CONFIGURATION_SSM_MODE_VALUE) != m_SSMMode) {
                        m_SSMCommand = FRS_SSM_MODE_ONLY_MANUAL_CUE;
                        ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                        if (ret) {
                            CLOGE("setcontrol() failed!!");
                        } else {
                            CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_MODE_ONLY_MANUAL_CUE", m_SSMCommand);
                        }

                        m_SSMMode = m_configurations->getModeValue(CONFIGURATION_SSM_MODE_VALUE);
                    }

                    m_SSMCommand = FRS_SSM_MANUAL_CUE_ENABLE;
                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_MANUAL_CUE_ENABLE", m_SSMCommand);
                    }

                    m_SSMCommand = FRS_SSM_START;
                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_START_MANUAL", m_SSMCommand);
                    }
                }
            } else {
                memset(&m_SSMRegion, 0, sizeof(ExynosRect2));

                if (m_SSMCommand == FRS_SSM_START) {
                    m_SSMCommand = FRS_SSM_STOP;
                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_STOP", m_SSMCommand);
                    }
                }
            }
        } else {
            ExynosRect2 region;
            m_configurations->getSSMRegion(&region);
            if (region != m_SSMRegion) {
                int ret = 0;
                struct v4l2_ext_controls extCtrls;
                struct v4l2_ext_control extCtrl;
                struct v4l2_rect ssmRoiParam;

                m_SSMRegion = region;
                m_SSMUseAutoDelayQ = true;
                m_SSMFirstRecordingRequest = 0;

                if (m_SSMCommand == FRS_SSM_START) {
                    m_SSMCommand = FRS_SSM_STOP;

                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_STOP", m_SSMCommand);
                    }
                }

                m_SSMDetectDurationTime = 0;
                m_checkRegister = false;

                m_SSMDetectDurationTimer.start();
                if (m_configurations->getModeValue(CONFIGURATION_SSM_MODE_VALUE) != m_SSMMode) {
                    m_SSMCommand = FRS_SSM_MODE_AUTO_MANUAL_CUE;
                    ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                    if (ret) {
                        CLOGE("setcontrol() failed!!");
                    } else {
                        CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_MODE_AUTO_MANUAL_CUE", m_SSMCommand);
                    }

                    m_SSMMode = m_configurations->getModeValue(CONFIGURATION_SSM_MODE_VALUE);
                }

                memset(&extCtrls, 0x00, sizeof(extCtrls));
                memset(&extCtrl, 0x00, sizeof(extCtrl));
                memset(&ssmRoiParam, 0x00, sizeof(ssmRoiParam));

                extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
                extCtrls.count = 1;
                extCtrls.controls = &extCtrl;

                extCtrl.id = V4L2_CID_SENSOR_SET_SSM_ROI;
                extCtrl.ptr = &ssmRoiParam;

                ssmRoiParam.left = m_SSMRegion.x1; //m_ssm_roi.roi_start_x;
                ssmRoiParam.width = m_SSMRegion.x2 - m_SSMRegion.x1; //m_ssm_roi.roi_width;
                ssmRoiParam.top = m_SSMRegion.y1; //m_ssm_roi.roi_start_y;
                ssmRoiParam.height = m_SSMRegion.y2 - m_SSMRegion.y1; //m_ssm_roi.roi_height;

                ret = factory->setExtControl(&extCtrls, PIPE_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("setExtControl() fail, ret(%d)", ret);
                } else {
                    CLOGD("setExtControl() V4L2_CID_SENSOR_SET_SSM_ROI:(%d,%d,%d,%d) ",
                        ssmRoiParam.left, ssmRoiParam.width, ssmRoiParam.top, ssmRoiParam.height);
                }

                m_SSMCommand = FRS_SSM_START;
                ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                if (ret) {
                    CLOGE("setcontrol() failed!!");
                } else {
                    CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_START_AUTO", m_SSMCommand);
                }
            }
#ifdef SSM_CONTROL_THRESHOLD
            else {
                if (m_checkRegister == false) {
                    int duration = 0;

                    m_SSMDetectDurationTimer.stop();
                    duration = (int)m_SSMDetectDurationTimer.durationMsecs();
                    m_SSMDetectDurationTime += duration;

                    if (m_SSMDetectDurationTime >= 2000) {
                        int a_gain = 0, d_gain = 0, sensor_gain = 0;
                        int org_threshold = 0, new_threshold = 0;
                        float coefficient = 1.0f;
                        int flicker_data = m_configurations->getModeValue(CONFIGURATION_FLICKER_DATA);

                        ret = factory->getControl(V4L2_CID_SENSOR_GET_SSM_THRESHOLD, &org_threshold, PIPE_3AA, nodeType);
                        if (ret) {
                            CLOGE("setcontrol() failed!!");
                        } else {
                            CLOGD("setcontrol() V4L2_CID_SENSOR_GET_SSM_THRESHOLD org_threshold (%d)", org_threshold);
                        }

                        ret = factory->getControl(V4L2_CID_SENSOR_GET_ANALOG_GAIN, &a_gain, PIPE_3AA, nodeType);
                        if (ret) {
                            CLOGE("setcontrol() failed!!");
                        } else {
                            CLOGD("setcontrol() V4L2_CID_SENSOR_GET_ANALOG_GAIN a_gain(%d)", a_gain);
                        }

                        ret = factory->getControl(V4L2_CID_SENSOR_GET_DIGITAL_GAIN, &d_gain, PIPE_3AA, nodeType);
                        if (ret) {
                            CLOGE("setcontrol() failed!!");
                        } else {
                            CLOGD("setcontrol() V4L2_CID_SENSOR_GET_DIGITAL_GAIN d_gain(%d)", d_gain);
                        }

                        a_gain = a_gain / 1000;
                        d_gain = d_gain / 1000;
                        sensor_gain = a_gain * d_gain;

                        /* flicker 100: 50Hz, 120: 60Hz */
                        if (flicker_data != 100 && flicker_data != 120) {
                            if (sensor_gain <= 24 && org_threshold > 55) {
                                coefficient = 0.7f;
                            }
                        }

                        new_threshold = org_threshold * coefficient;

                        CLOGD("SSM_REPEATED a_gain(%d) d_gain(%d) sensor_gain(%d) org_threshold(%d), new_threshold(%d) coefficient(%f) flicker_data(%d)",
                            a_gain, d_gain, sensor_gain, org_threshold, new_threshold, coefficient, flicker_data);

                        if (new_threshold != org_threshold) {
                            ret = factory->setControl(V4L2_CID_SENSOR_SET_SSM_THRESHOLD, new_threshold, PIPE_3AA, nodeType);
                            if (ret) {
                                CLOGE("setcontrol() failed!!");
                            } else {
                                CLOGD("setcontrol() V4L2_CID_SENSOR_SET_SSM_THRESHOLD: new_threshold(%d)", new_threshold);
                            }
                        }

                        m_checkRegister = true;
                        m_SSMDetectDurationTime = 0;
                    } else {
                        m_SSMDetectDurationTimer.start();
                    }
                }
            }
#endif
        }
    }
}

void ExynosCamera::m_checkSSMState(ExynosCameraRequestSP_sprt_t request, struct camera2_shot_ext *shot_ext)
{
    int frameId = shot_ext->shot.udm.frame_id;
    int framecount = shot_ext->shot.dm.request.frameCount;
    int preSSMState = m_SSMState;

    switch (m_SSMState) {
    case SSM_STATE_NONE:
        if (request->hasStream(HAL_STREAM_ID_VIDEO) == true) {
            m_SSMState = SSM_STATE_NORMAL;
            m_SSMRecordingtime = m_SSMOrgRecordingtime = shot_ext->shot.udm.sensor.timeStampBoot;
        }
        break;
    case SSM_STATE_NORMAL:
        if (request->hasStream(HAL_STREAM_ID_VIDEO) == false) {
            m_SSMState = SSM_STATE_NONE;
            m_SSMRecordingtime = 0;
            m_SSMOrgRecordingtime = 0;
            m_SSMSkipToggle = 0;
            m_SSMRecordingToggle = 0;
            m_SSMFirstRecordingRequest = 0;
            m_configurations->setModeValue(CONFIGURATION_SSM_TRIGGER, (int)false);
        } else if (frameId == SSM_FRAME_PREVIEW_ONLY) {
            m_SSMState = SSM_STATE_PREVIEW_ONLY;
            if (m_configurations->getModeValue(CONFIGURATION_SSM_STATE)
                != SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_OPERATING) {
                m_configurations->setModeValue(CONFIGURATION_SSM_TRIGGER, (int)false);
                m_configurations->setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_OPERATING);
#ifdef SSM_BIG_CORE_CONTROL
                {
                    ExynosCameraFrameFactory *factory = NULL;
                    int ret = 0;

                    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                    if (factory == NULL) {
                        CLOGE("FrameFactory is NULL!!");
                    } else {
                        ret = factory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, SSM_BIG_CORE_MIN_LOCK, PIPE_3AA);
                        if (ret != NO_ERROR) {
                            CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);
                        } else {
                            CLOGD("V4L2_CID_IS_DVFS_CLUSTER1 setControl cid(%d) value(%d)",
                                V4L2_CID_IS_DVFS_CLUSTER1, SSM_BIG_CORE_MIN_LOCK);
                        }
                    }
                }
#endif
            }
        }
        break;
    case SSM_STATE_PREVIEW_ONLY:
        if (frameId == SSM_FRAME_RECORDING_FIRST || frameId == SSM_FRAME_RECORDING || frameId == SSM_FRAME_PREVIEW) {
            m_SSMState = SSM_STATE_PROCESSING;
            m_SSMFirstRecordingRequest = request->getKey();
        }
        break;
    case SSM_STATE_PROCESSING:
        if (frameId == SSM_FRAME_NORMAL_RECORDING) {
            if (request->getKey() > m_SSMFirstRecordingRequest + 20) {
                m_SSMState = SSM_STATE_POSTPROCESSING;
            } else {
                CLOGD("[R%d F%d S%d] wrong frame Id(%d) !!!",
                    request->getKey(), request->getFrameCount(), m_SSMState, frameId);
            }
        }
        break;
    case SSM_STATE_POSTPROCESSING:
        if (m_SSMSaveBufferQ->getSizeOfProcessQ() == 0) {
            ExynosCameraFrameFactory *factory = NULL;
            int ret = 0;
            enum NODE_TYPE nodeType;

            m_SSMFirstRecordingRequest = 0;
            m_SSMState = SSM_STATE_NORMAL;

            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            if (factory == NULL) {
                CLOGE("FrameFactory is NULL!!");
            } else {
                nodeType = factory->getNodeType(PIPE_FLITE);
                m_SSMCommand = FRS_SSM_STOP;

                ret = factory->setControl(V4L2_CID_SENSOR_SET_FRS_CONTROL, m_SSMCommand, PIPE_3AA, nodeType);
                if (ret) {
                    CLOGE("setcontrol() failed!!");
                } else {
                    CLOGD("setcontrol() V4L2_CID_SENSOR_SET_FRS_CONTROL:(%d) FRS_SSM_STOP", m_SSMCommand);
                }

#ifdef SSM_BIG_CORE_CONTROL
                ret = factory->setControl(V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK, PIPE_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("V4L2_CID_IS_DVFS_CLUSTER1 setControl fail, ret(%d)", ret);
                } else {
                    CLOGD("V4L2_CID_IS_DVFS_CLUSTER1 setControl cid(%d) value(%d)",
                        V4L2_CID_IS_DVFS_CLUSTER1, BIG_CORE_MAX_LOCK);
                }
#endif
            }

            m_configurations->setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_READY);
        }
        break;
    default:
        break;
    }

    if (preSSMState != m_SSMState) {
        CLOGD("[R%d F%d] m_SSMState%d, m_SSMRecordingtime %lld, frameId%d framecount%d m_SSMFirstRecordingRequest%d",
            request->getKey(),request->getFrameCount(), m_SSMState,
            m_SSMRecordingtime, frameId, framecount, m_SSMFirstRecordingRequest);
    }
}

status_t ExynosCamera::m_SSMProcessing(ExynosCameraRequestSP_sprt_t request,
                                                ExynosCameraFrameSP_sptr_t frame, int pipeId,
                                                ExynosCameraBuffer *buffer, int streamId)
{
    status_t ret = NO_ERROR;
    bool skipBuffer = false;
    struct camera2_shot_ext *shot_ext = (struct camera2_shot_ext *) buffer->addr[buffer->getMetaPlaneIndex()];
    int frameId = shot_ext->shot.udm.frame_id;
    int ratio = SSM_PREVIEW_FPS / SSM_RECORDING_FPS;

    if ((streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_PREVIEW) {
        m_checkSSMState(request, shot_ext);

        switch (m_SSMState) {
        case SSM_STATE_NONE:
            if (m_SSMAutoDelay > 0) {
                while (m_SSMAutoDelayQ->getSizeOfProcessQ() > 0) {
                    ExynosCameraFrameSP_sptr_t delayFrame = NULL;
                    ExynosCameraBuffer delayBuffer;
                    ExynosCameraRequestSP_sprt_t delayRequest = NULL;
                    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                    int portId = PIPE_MCSC0 + m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
                    m_SSMAutoDelayQ->popProcessQ(&delayFrame);
                    if (delayFrame != NULL) {
                        ret = delayFrame->getDstBuffer(pipeId, &delayBuffer, factory->getNodeType(portId));
                        if (ret != NO_ERROR || delayBuffer.index < 0) {
                            CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                    portId, delayBuffer.index, delayFrame->getFrameCount(), ret);
                        }

                        delayRequest = m_requestMgr->getRunningRequest(delayFrame->getFrameCount());
                        if (delayRequest != NULL) {
                            if (delayBuffer.index < 0) {
                                skipBuffer = true;
                            } else {
                                m_SSMSkipToggle = (m_SSMSkipToggle + 1) % ratio;
                                skipBuffer = (m_SSMSkipToggle == 0) ? true : false;
                            }

                            ret = m_sendYuvStreamResult(delayRequest, &delayBuffer, HAL_STREAM_ID_VIDEO,
                                                        skipBuffer, frame->getStreamTimestamp(),
                                                        frame->getParameters());
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                                        delayBuffer.index, delayRequest->getFrameCount(), HAL_STREAM_ID_VIDEO, ret);
                            }
                        } else {
                            if (delayBuffer.index >= 0) {
                                CLOGE("[F%d T%d]request is null! entityId %d",
                                    delayFrame->getFrameCount(), delayFrame->getFrameType(), pipeId);

                                ret = m_bufferSupplier->putBuffer(delayBuffer);
                                if (ret !=  NO_ERROR) {
                                    CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                            frame->getFrameCount(), delayBuffer.index, pipeId, ret);
                                }
                            }
                        }
                    }
                }
            }
            break;
        case SSM_STATE_PROCESSING:
        case SSM_STATE_POSTPROCESSING:
            if ((m_SSMState == SSM_STATE_PROCESSING && frameId != SSM_FRAME_PREVIEW)
                || (m_SSMState == SSM_STATE_POSTPROCESSING && frameId != SSM_FRAME_NORMAL_RECORDING)) {
                skipBuffer = true;
                if ((m_SSMState == SSM_STATE_PROCESSING && frameId != SSM_FRAME_RECORDING_FIRST && frameId != SSM_FRAME_RECORDING)
                    || (m_SSMState == SSM_STATE_POSTPROCESSING && frameId != SSM_FRAME_NORMAL_RECORDING)) {
                    CLOGD("[R%d F%d S%d] wrong frame Id(%d) preview frame skip!!!",
                        request->getKey(), request->getFrameCount(), m_SSMState, frameId);
                }
            } else {
                m_SSMSkipToggle = (m_SSMSkipToggle + 1) % ratio;

                if (m_SSMSkipToggle != 0) {
                    buffer_manager_tag_t bufTag;
                    ExynosCameraBuffer saveBuffer;
                    int portId = PIPE_MCSC0 + m_streamManager->getOutputPortId(streamId);
                    bufTag.pipeId[0] = portId;
                    bufTag.managerType = BUFFER_MANAGER_ONLY_HAL_USE_ION_TYPE;
                    ExynosCameraDurationTimer timer;

                    ret = m_bufferSupplier->getBuffer(bufTag, &saveBuffer);
                    if (ret != NO_ERROR || saveBuffer.index < 0) {
                        CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                                request->getKey(), frame->getFrameCount(), saveBuffer.index, ret);
                    }

                    if (saveBuffer.index >= 0) {
                        timer.start();
                        for (int i = 0; i < saveBuffer.planeCount - 1; i++) {
                            memcpy(saveBuffer.addr[i], buffer->addr[i], buffer->size[i]);
                            if (m_ionClient >= 0)
                                exynos_ion_sync_fd(m_ionClient, saveBuffer.fd[i]);
                        }
                        timer.stop();

                        if ((int)timer.durationMsecs() > 10) {
                            CLOGD("[R%d F%d B%d] memcpy time:(%5d msec)",
                                request->getKey(), frame->getFrameCount(), saveBuffer.index, (int)timer.durationMsecs());
                        }

                        m_SSMSaveBufferQ->pushProcessQ(&saveBuffer);
                    }
                }
            }
            break;
        case SSM_STATE_NORMAL:
        case SSM_STATE_PREVIEW_ONLY:
        default:
            break;
        }

        ret = m_sendYuvStreamResult(request, buffer, streamId, skipBuffer,
                                    frame->getStreamTimestamp(), frame->getParameters());
        if (ret != NO_ERROR) {
            CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                    buffer->index, request->getFrameCount(), streamId, ret);
        }
    } else if ((streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_VIDEO) {
        switch (m_SSMState) {
        case SSM_STATE_NORMAL:
            if (m_SSMAutoDelay == 0) {
                m_SSMSkipToggle = (m_SSMSkipToggle + 1) % ratio;
                skipBuffer = (m_SSMSkipToggle == 0) ? true : false;

                ret = m_sendYuvStreamResult(request, buffer, streamId, skipBuffer,
                                            frame->getStreamTimestamp(), frame->getParameters());
                if (ret != NO_ERROR) {
                    CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                            buffer->index, request->getFrameCount(), streamId, ret);
                }
            } else {
                if (m_SSMAutoDelayQ->getSizeOfProcessQ() >= m_SSMAutoDelay) {
                    ExynosCameraFrameSP_sptr_t delayFrame = NULL;
                    ExynosCameraBuffer delayBuffer;
                    ExynosCameraRequestSP_sprt_t delayRequest = NULL;
                    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                    int portId = PIPE_MCSC0 + m_streamManager->getOutputPortId(streamId);
                    delayBuffer.index = -2;

                    m_SSMAutoDelayQ->popProcessQ(&delayFrame);
                    if (delayFrame != NULL) {
                        ret = delayFrame->getDstBuffer(pipeId, &delayBuffer, factory->getNodeType(portId));
                        if (ret != NO_ERROR || delayBuffer.index < 0) {
                            CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                    portId, delayBuffer.index, delayFrame->getFrameCount(), ret);
                        }

                        delayRequest = m_requestMgr->getRunningRequest(delayFrame->getFrameCount());
                        if (delayRequest != NULL) {
                            if (delayBuffer.index >= 0) {
                                m_SSMSkipToggle = (m_SSMSkipToggle + 1) % ratio;
                                skipBuffer = (m_SSMSkipToggle == 0) ? true : false;
                            } else {
                                skipBuffer = false;
                            }

                            ret = m_sendYuvStreamResult(delayRequest, &delayBuffer, streamId, skipBuffer,
                                                        frame->getStreamTimestamp(), frame->getParameters());
                            if (ret != NO_ERROR) {
                                CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                                        delayBuffer.index, delayRequest->getFrameCount(), streamId, ret);
                            }
                        } else {
                            if (delayBuffer.index >= 0) {
                                CLOGE("[F%d T%d]request is null! entityId %d",
                                    delayFrame->getFrameCount(), delayFrame->getFrameType(), pipeId);

                                ret = m_bufferSupplier->putBuffer(delayBuffer);
                                if (ret !=  NO_ERROR) {
                                    CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                            frame->getFrameCount(), delayBuffer.index, pipeId, ret);
                                }
                            }
                        }
                    }
                }

                m_SSMAutoDelayQ->pushProcessQ(&frame);
            }
            break;
        case SSM_STATE_PROCESSING:
            {
                bool ssmfalg = true;
                if (frameId != SSM_FRAME_RECORDING_FIRST && frameId != SSM_FRAME_RECORDING) {
                    skipBuffer = true;
                    ssmfalg = false;
                    if (frameId != SSM_FRAME_PREVIEW) {
                        CLOGD("[R%d F%d S%d] wrong frame Id(%d) recording frame skip!!!",
                            request->getKey(), request->getFrameCount(), m_SSMState, frameId);
                    }
                }

                ret = m_sendYuvStreamResult(request, buffer, streamId, skipBuffer,
                                            frame->getStreamTimestamp(), frame->getParameters(), ssmfalg);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                            buffer->index, request->getFrameCount(), streamId, ret);
                }
            }
            break;
        case SSM_STATE_PREVIEW_ONLY:
            if (m_SSMAutoDelay > 0) {
                if (m_SSMUseAutoDelayQ == false) {
                    if (m_SSMAutoDelayQ->getSizeOfProcessQ() > 0) {
                        ExynosCameraFrameSP_sptr_t delayFrame = NULL;
                        ExynosCameraBuffer delayBuffer;
                        ExynosCameraRequestSP_sprt_t delayRequest = NULL;
                        ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                        int portId = PIPE_MCSC0 + m_streamManager->getOutputPortId(streamId);
                        m_SSMAutoDelayQ->popProcessQ(&delayFrame);
                        if (delayFrame != NULL) {
                            ret = delayFrame->getDstBuffer(pipeId, &delayBuffer, factory->getNodeType(portId));
                            if (ret != NO_ERROR || delayBuffer.index < 0) {
                                CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        portId, delayBuffer.index, delayFrame->getFrameCount(), ret);
                            }

                            delayRequest = m_requestMgr->getRunningRequest(delayFrame->getFrameCount());
                            if (delayRequest != NULL) {
                                ret = m_sendYuvStreamResult(delayRequest, &delayBuffer, streamId,
                                                            false, frame->getStreamTimestamp(), frame->getParameters());
                                if (ret != NO_ERROR) {
                                    CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                                            delayBuffer.index, delayRequest->getFrameCount(), streamId, ret);
                                }
                            } else {
                                if (delayBuffer.index >= 0) {
                                    CLOGE("[F%d T%d]request is null! entityId %d",
                                        delayFrame->getFrameCount(), delayFrame->getFrameType(), pipeId);

                                    ret = m_bufferSupplier->putBuffer(delayBuffer);
                                    if (ret !=  NO_ERROR) {
                                        CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                                frame->getFrameCount(), delayBuffer.index, pipeId, ret);
                                    }
                                }
                            }
                        }
                    }
                } else {
                    while (m_SSMAutoDelayQ->getSizeOfProcessQ() > 0) {
                        ExynosCameraFrameSP_sptr_t delayFrame = NULL;
                        ExynosCameraBuffer delayBuffer;
                        ExynosCameraRequestSP_sprt_t delayRequest = NULL;
                        ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                        int portId = PIPE_MCSC0 + m_streamManager->getOutputPortId(streamId);
                        m_SSMAutoDelayQ->popProcessQ(&delayFrame);
                        if (delayFrame != NULL) {
                            ret = delayFrame->getDstBuffer(pipeId, &delayBuffer, factory->getNodeType(portId));
                            if (ret != NO_ERROR || delayBuffer.index < 0) {
                                CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        portId, delayBuffer.index, delayFrame->getFrameCount(), ret);
                            }

                            delayRequest = m_requestMgr->getRunningRequest(delayFrame->getFrameCount());
                            if (delayRequest != NULL) {
                                ret = m_sendYuvStreamResult(delayRequest, &delayBuffer, streamId, true,
                                                            frame->getStreamTimestamp(), frame->getParameters());
                                if (ret != NO_ERROR) {
                                    CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                                            delayBuffer.index, delayRequest->getFrameCount(), streamId, ret);
                                }
                            } else {
                                if (delayBuffer.index >= 0) {
                                    CLOGE("[F%d T%d]request is null! entityId %d",
                                        delayFrame->getFrameCount(), delayFrame->getFrameType(), pipeId);

                                    ret = m_bufferSupplier->putBuffer(delayBuffer);
                                    if (ret !=  NO_ERROR) {
                                        CLOGE("[F%d B%d]PutBuffers failed. pipeId %d ret %d",
                                                frame->getFrameCount(), delayBuffer.index, pipeId, ret);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        case SSM_STATE_NONE:
        case SSM_STATE_POSTPROCESSING:
            ret = m_sendYuvStreamResult(request, buffer, streamId, true, frame->getStreamTimestamp(),
                                        frame->getParameters());
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                        buffer->index, request->getFrameCount(), streamId, ret);
            }
            break;
        default:
            break;
        }
    }

    return ret;
}

status_t ExynosCamera::m_sendForceSSMResult(ExynosCameraRequestSP_sprt_t request)
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

    bufferCnt = request->getNumOfOutputBuffer();
    outputBuffer = request->getOutputBuffers();
    request->setSensorTimestamp(m_lastFrametime);

    m_frameCountLock.lock();
    m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());
    m_frameCountLock.unlock();

    m_sendNotifyShutter(request);

    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_RESULT);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Failed to sendNotifyError. ret %d",
                request->getKey(), request->getFrameCount(), ret);
    }

    m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);

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

        if (HAL_STREAM_ID_VIDEO == (streamId % HAL_STREAM_ID_MAX)) {
            buffer_handle_t *handle = NULL;
            buffer_manager_tag_t bufTag;
            ExynosCameraBuffer serviceBuffer;
            ExynosCameraBuffer saveBuffer;
            const camera3_stream_buffer_t *streamBuffer = &(outputBuffer[i]);
            bool skipBuffer = false;
            ExynosCameraDurationTimer timer;

            handle = streamBuffer->buffer;

            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
            bufTag.pipeId[0] = PIPE_MCSC0 + m_streamManager->getOutputPortId(streamId);
            serviceBuffer.handle[0] = handle;
            serviceBuffer.acquireFence[0] = streamBuffer->acquire_fence;
            serviceBuffer.releaseFence[0] = streamBuffer->release_fence;

            ret = m_bufferSupplier->getBuffer(bufTag, &serviceBuffer);
            if (ret != NO_ERROR || serviceBuffer.index < 0) {
                CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                    request->getKey(), frameCount, serviceBuffer.index, ret);
            }

            ret = request->setAcquireFenceDone(handle, (serviceBuffer.acquireFence[0] == -1) ? true : false);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frameCount, serviceBuffer.index, streamId, ret);
            }

            if (m_SSMSaveBufferQ->getSizeOfProcessQ() > 0 && serviceBuffer.index >= 0) {
                m_SSMSaveBufferQ->popProcessQ(&saveBuffer);
                if (saveBuffer.index >= 0) {
                    timer.start();
                    for (int i = 0; i < saveBuffer.planeCount - 1; i++) {
                        memcpy(serviceBuffer.addr[i], saveBuffer.addr[i], saveBuffer.size[i]);
                        if (m_ionClient >= 0)
                            exynos_ion_sync_fd(m_ionClient, serviceBuffer.fd[i]);
                    }
                    timer.stop();

                    if ((int)timer.durationMsecs() > 10) {
                        CLOGD("[R%d F%d B%d] memcpy time:(%5d msec) planeCount(%d)",
                            request->getKey(), request->getFrameCount(), saveBuffer.index, (int)timer.durationMsecs(), saveBuffer.planeCount);
                    }

                    skipBuffer = false;
                    ret = m_bufferSupplier->putBuffer(saveBuffer);
                    if (ret !=  NO_ERROR) {
                        CLOGE("[R%d B%d]PutBuffers failed. pipeId %d ret %d",
                                request->getKey(), saveBuffer.index, saveBuffer.tag.pipeId[0], ret);
                    }
                } else {
                    skipBuffer = true;
                }
            } else {
                skipBuffer = true;
            }

            ret = m_sendYuvStreamResult(request, &serviceBuffer, streamId, skipBuffer, 0, m_parameters[m_cameraId]);
            if (ret != NO_ERROR) {
                CLOGE("Failed to resultCallback bufferIndex %d frameCount %d streamId %d ret %d",
                        serviceBuffer.index, request->getFrameCount(), streamId, ret);
            }
        } else {
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
            int err = sync_wait(outputBuffer[i].acquire_fence, 1000 /* msec */);
            if (err >= 0) {
                streamBuffer->release_fence = outputBuffer[i].acquire_fence;
            } else {
                streamBuffer->release_fence = -1;
            }

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
            CLOGV("[R%d F%d S%d] preview", request->getKey(), frameCount, streamId);
        }
    }

    ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
    if (ret != NO_ERROR) {
        CLOGE("[R%d]Failed to sendMeta. ret %d", request->getKey(), ret);
    }

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_RTHDR
void ExynosCamera::m_updateWdrMode(struct camera2_shot_ext *shot_ext, bool isCaptureFrame)
{
    int wdr_mode = -1;

    if (m_cameraId == CAMERA_ID_FRONT) {
        int vtMode = m_configurations->getModeValue(CONFIGURATION_VT_MODE);
        if (vtMode > 0) {
            if (vtMode == 1 && isCaptureFrame == true) {
                wdr_mode = CAMERA_WDR_OFF;
            } else {
                wdr_mode = CAMERA_WDR_AUTO;
            }
        }
    }

    if (wdr_mode >= CAMERA_WDR_OFF) {
        shot_ext->shot.uctl.isModeUd.wdr_mode = (enum camera2_wdr_mode)wdr_mode;
    }
}
#endif

#ifdef SAMSUNG_TN_FEATURE
void ExynosCamera::m_setTransientActionInfo(frame_handle_components_t *components)
{
    int ret = 0;
    int prevTransientAction = m_configurations->getModeValue(CONFIGURATION_PREV_TRANSIENT_ACTION_MODE);
    int transientAction = m_configurations->getModeValue(CONFIGURATION_TRANSIENT_ACTION_MODE);

    if (transientAction != prevTransientAction && (m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_BACK_1)) {
        if (transientAction == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_ZOOMING
            && prevTransientAction == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE)
        {
            ret = components->previewFactory->setControl(V4L2_CID_IS_TRANSIENT_ACTION, ACTION_ZOOMING, PIPE_3AA);
            if (ret < 0) {
                CLOGE("PIPE_3AA setControl fail, transientAction(%d), ret(%d)", ACTION_ZOOMING, ret);
            }
        } else if (transientAction == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE
                        && prevTransientAction == SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_ZOOMING)
        {
            ret = components->previewFactory->setControl(V4L2_CID_IS_TRANSIENT_ACTION, ACTION_NONE, PIPE_3AA);
            if (ret < 0) {
                CLOGE("PIPE_3AA setControl fail, transientAction(%d), ret(%d)", ACTION_NONE, ret);
            }
        }
        m_configurations->setModeValue(CONFIGURATION_PREV_TRANSIENT_ACTION_MODE, transientAction);
    }
}
#endif

#ifdef SAMSUNG_TN_FEATURE
bool ExynosCamera::m_dscaledYuvStallPostProcessingThreadFunc(void)
{
    int loop = false;
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    int srcPipeId = m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) + PIPE_MCSC0_REPROCESSING;
    int pipeId_next = -1;
    frame_handle_components_t components;
    int waitCount = 0;
    int leaderPipeId = -1;
    bool flag3aaIspM2M = false;
    bool flagIspMcscM2M = false;
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = false;
    bool flagDcpMcscM2M = false;
#endif

    CLOGV("wait m_dscaledYuvStallPPCbQ");
    ret = m_dscaledYuvStallPPCbQ->waitAndPopProcessQ(&frame);
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
#ifdef USE_DUAL_CAMERA
    flagIspDcpM2M = (components.parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    flagDcpMcscM2M = (components.parameters->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_MCSC_REPROCESSING) == true) {
            leaderPipeId = PIPE_MCSC_REPROCESSING;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_DCP_REPROCESSING) == true) {
            leaderPipeId = PIPE_DCP_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_ISP_REPROCESSING) == true) {
            leaderPipeId = PIPE_ISP_REPROCESSING;
        } else {
            leaderPipeId = PIPE_3AA_REPROCESSING;
        }
    } else
#endif
    {
        if (flagIspMcscM2M == true
            && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_MCSC_REPROCESSING) == true) {
            leaderPipeId = PIPE_MCSC_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(components.reprocessingFactory, PIPE_ISP_REPROCESSING) == true) {
            leaderPipeId = PIPE_ISP_REPROCESSING;
        } else {
            leaderPipeId = PIPE_3AA_REPROCESSING;
        }
    }

    pipeId_next = PIPE_PP_UNI_REPROCESSING;
    if (frame->getFrameYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) {
        m_setupCaptureUniPP(frame, PIPE_GSC_REPROCESSING3, PIPE_GSC_REPROCESSING3, pipeId_next);
    } else {
        m_setupCaptureUniPP(frame, leaderPipeId, srcPipeId, pipeId_next);
    }

    components.reprocessingFactory->setOutputFrameQToPipe(m_dscaledYuvStallPPPostCbQ, pipeId_next);
    components.reprocessingFactory->pushFrameToPipe(frame, pipeId_next);
    if (components.reprocessingFactory->checkPipeThreadRunning(pipeId_next) == false) {
        components.reprocessingFactory->startThread(pipeId_next);
    }

    /* wait PIPE_PP_UNI_REPROCESSING done */
    CLOGV("INFO(%s[%d]):wait PIPE_PP_UNI_REPROCESSING output", __FUNCTION__, __LINE__);
    waitCount = 0;
    frame = NULL;
    do {
        ret = m_dscaledYuvStallPPPostCbQ->waitAndPopProcessQ(&frame);
        waitCount++;
    } while (ret == TIMED_OUT && waitCount < 100);

    if (ret < 0) {
        CLOGW("WARN(%s[%d]):Failed to waitAndPopProcessQ. ret %d waitCount %d",
                __FUNCTION__, __LINE__, ret, waitCount);
    }
    if (frame == NULL) {
        CLOGE("ERR(%s[%d]):frame is NULL", __FUNCTION__, __LINE__);
        goto CLEAN;
    }

    ret = frame->setEntityState(PIPE_PP_UNI_REPROCESSING, ENTITY_STATE_COMPLETE);

CLEAN:
    if (frame != NULL) {
        CLOGV("frame delete. framecount %d", frame->getFrameCount());
        frame = NULL;
    }

    CLOGV("--OUT--");

    return loop;
}
#endif

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
#ifdef SAMSUNG_SW_VDIS
    if (m_configurations->getMode(CONFIGURATION_SWVDIS_MODE) == true) {
        int portId = m_parameters[m_cameraId]->getPreviewPortId();
        if (width == 3840 && height == 2160) {
            buffer_count = NUM_VDIS_UHD_INTERNAL_BUFFERS;
        } else {
            buffer_count = NUM_VDIS_INTERNAL_BUFFERS;
        }
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

        planeCount = 3;
        needMmap = true;
    } else
#endif
    {
#ifdef SAMSUNG_VIDEO_BEAUTY
        if (m_configurations->getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

            planeCount = 3;
            buffer_count = NUM_VIDEO_BEAUTY_INTERNAL_BUFFERS;
            needMmap = true;
        }
#endif
#if defined(SAMSUNG_HIFI_VIDEO) && !defined(HIFIVIDEO_INPUTCOPY_DISABLE)
        if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
            int portId = m_parameters[m_cameraId]->getPreviewPortId();
            m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

            planeCount = 3;
            buffer_count = NUM_HIFIVIDEO_INTERNAL_BUFFERS;
            needMmap = true;
        }
#endif
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

#ifdef SAMSUNG_HYPERLAPSE
    if (m_configurations->getMode(CONFIGURATION_HYPERLAPSE_MODE) == true) {
        int portId = m_parameters[m_cameraId]->getPreviewPortId();
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

        planeCount = 3;
        buffer_count = NUM_HYPERLAPSE_INTERNAL_BUFFERS;
        needMmap = true;
    }
#endif

#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_INPUTCOPY_DISABLE)
    if (m_configurations->getMode(CONFIGURATION_HIFIVIDEO_MODE) == true) {
        int portId = m_parameters[m_cameraId]->getPreviewPortId();
        m_parameters[m_cameraId]->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height, portId);

        planeCount = 3;
        if (buffer_count) {
            buffer_count += NUM_HIFIVIDEO_EXTRA_INTERNAL_BUFFERS;
        } else {
            buffer_count = NUM_HIFIVIDEO_INTERNAL_BUFFERS;
        }
        buffer_count = MIN(buffer_count, VIDEO_MAX_FRAME);
        needMmap = true;
    }
#endif

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

#ifdef SAMSUNG_SSM
    if (m_configurations->getModeValue(CONFIGURATION_SHOT_MODE)
        == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION) {
        const buffer_manager_tag_t initBufTag;
        const buffer_manager_configuration_t initBufConfig;
        buffer_manager_configuration_t bufConfig;
        buffer_manager_tag_t bufTag;
        exynos_camera_buffer_type_t buffer_type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

        buffer_count = SSM_MAX_BUFFER_COUNT;
        planeCount = 3;
        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&width, (uint32_t *)&height);

        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_MCSC0;
        bufTag.pipeId[1] = PIPE_MCSC1;
        bufTag.pipeId[2] = PIPE_MCSC2;
        bufTag.managerType = BUFFER_MANAGER_ONLY_HAL_USE_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("SSM_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create SSM_BUF. ret %d", ret);
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
        bufConfig.needMmap = true;
        bufConfig.reservedMemoryCount = 0;

#ifdef ADAPTIVE_RESERVED_MEMORY
        ret = m_addAdaptiveBufferInfos(bufTag, bufConfig, BUF_PRIORITY_SSM, BUF_TYPE_VENDOR);
        if (ret != NO_ERROR) {
            CLOGE("Failed to add SSM_BUF. ret %d", ret);
            return ret;
        }
#else
        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc SSM_BUF. ret %d", ret);
            return ret;
        }
#endif

        CLOGI("m_allocBuffers(SSM_BUF) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            width, height,
            bufConfig.size[0], bufConfig.planeCount, buffer_count);
    }
#endif

    return ret;
}

void ExynosCamera::m_updateMasterCam(struct camera2_shot_ext *shot_ext)
{
    enum aa_sensorPlace masterCamera = AA_SENSORPLACE_REAR;
    enum aa_cameraMode cameraMode;

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int dualOperationMode = m_configurations->getDualOperationMode();
    int dispCamType = m_configurations->getModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE);

    if (m_scenario == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        switch(dispCamType) {
        case UNI_PLUGIN_CAMERA_TYPE_WIDE:
            cameraMode = AA_CAMERAMODE_DUAL_SYNC;
            masterCamera = AA_SENSORPLACE_REAR;
            break;
        case UNI_PLUGIN_CAMERA_TYPE_TELE:
            cameraMode = AA_CAMERAMODE_DUAL_SYNC;
            masterCamera = AA_SENSORPLACE_REAR_SECOND;
            break;
        default:
            cameraMode = AA_CAMERAMODE_SINGLE;
             switch(m_cameraId) {
             case CAMERA_ID_BACK:
                 masterCamera = AA_SENSORPLACE_REAR;
                 break;
             case CAMERA_ID_BACK_1:
                 masterCamera = AA_SENSORPLACE_REAR_SECOND;
                 break;
             default:
                 CLOGE("Invalid camera Id(%d)", m_cameraId);
                 break;
             }
        }
    } else
#endif
    {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
        if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT
            && m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            cameraMode = AA_CAMERAMODE_DUAL_SYNC;
            masterCamera = AA_SENSORPLACE_REAR_SECOND;
        } else
#endif
        {
            cameraMode = AA_CAMERAMODE_SINGLE;
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
        }
    }

    shot_ext->shot.uctl.masterCamera = masterCamera;
    shot_ext->shot.uctl.cameraMode = cameraMode;

}

}; /* namespace android */
