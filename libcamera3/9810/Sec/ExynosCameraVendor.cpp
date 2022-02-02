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
#include <cutils/log.h>

#include "ExynosCamera.h"

namespace android {

void ExynosCamera::m_vendorSpecificPreConstructor(int cameraId)
{
    CLOGI("-IN-");
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

    m_previewDurationTime = 0;
    m_displayPreviewToggle = 0;

    m_longExposureRemainCount = 0;

#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif

    memset(&m_stats, 0x00, sizeof(struct camera2_stats_dm));

    m_shutterSpeed = 0;
    m_gain = 0;
    m_irLedWidth = 0;
    m_irLedDelay = 0;
    m_irLedCurrent = 0;
    m_irLedOnTime = 0;

    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificPreDestructor(void)
{
    CLOGI("-IN-");
    CLOGI("-OUT-");

    return;
}

void ExynosCamera::m_vendorSpecificDestructor(void)
{
    CLOGI("-IN-");

    status_t ret = NO_ERROR;

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

status_t ExynosCamera::releaseDevice(void)
{
    status_t ret = NO_ERROR;
    CLOGD("");

    m_setBuffersThread->requestExitAndWait();
    m_startPictureBufferThread->requestExitAndWait();
    m_framefactoryCreateThread->requestExitAndWait();
    m_monitorThread->requestExitAndWait();

    if (m_getState() > EXYNOS_CAMERA_STATE_CONFIGURED) {
        flush();
    }

    m_frameMgr->stop();
    m_frameMgr->deleteAllFrame();

    return ret;
}

status_t ExynosCamera::m_constructFrameFactory(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("-IN-");

    ExynosCameraFrameFactory *factory = NULL;

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        /* Preview Frame Factory */
        if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] == NULL) {
#ifdef USE_DUAL_CAMERA
            if (m_parameters[m_cameraId]->getDualMode() == true) {
                factory = new ExynosCameraFrameFactoryPreviewDual(m_cameraId, m_parameters[m_cameraId]);
            } else
#endif
            if (m_cameraId == CAMERA_ID_FRONT && m_parameters[m_cameraId]->getPIPMode() == true) {
                factory = new ExynosCameraFrameFactoryPreviewFrontPIP(m_cameraId, m_parameters[m_cameraId]);
            } else {
                factory = new ExynosCameraFrameFactoryPreview(m_cameraId, m_parameters[m_cameraId]);
            }
            factory->setFrameCreateHandler(&ExynosCamera::m_previewFrameHandler);
            factory->setFrameManager(m_frameMgr);
            factory->setFactoryType(FRAME_FACTORY_TYPE_CAPTURE_PREVIEW);
            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW] = factory;
            m_frameFactoryQ->pushProcessQ(&factory);
        }

#ifdef USE_DUAL_CAMERA
        /* Dual Frame Factory */
        if (m_parameters[m_cameraId]->getDualMode() == true
            && m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL] == NULL) {
            factory = new ExynosCameraFrameFactoryPreview(m_cameraIds[1], m_parameters[m_cameraIds[1]]);
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
                if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5 && m_parameters[m_cameraId]->getPIPMode() == true) {
                    CLOGW("skip create framefactory(%d) mcscOutputPort(%d) PidMode(%d)", FRAME_FACTORY_TYPE_REPROCESSING , m_parameters[m_cameraId]->getNumOfMcscOutputPorts(), m_parameters[m_cameraId]->getPIPMode());
                } else {
#ifdef USE_DUAL_CAMERA
                    if (m_parameters[m_cameraId]->getDualMode() == true) {
                        factory = new ExynosCameraFrameReprocessingFactoryDual(m_cameraId, m_parameters[m_cameraId]);
                    } else
#endif
                    {
                        factory = new ExynosCameraFrameReprocessingFactory(m_cameraId, m_parameters[m_cameraId]);
                    }
                    factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                    factory->setFrameManager(m_frameMgr);
                    factory->setFactoryType(FRAME_FACTORY_TYPE_REPROCESSING);
                    m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING] = factory;
                    m_frameFactoryQ->pushProcessQ(&factory);
                }
            }

            if (m_parameters[m_cameraId]->getNumOfMcscOutputPorts() < 5 &&
                    m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] == NULL) {
                factory = new ExynosCameraFrameJpegReprocessingFactory(m_cameraId, m_parameters[m_cameraId]);
                factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                factory->setFrameManager(m_frameMgr);
                factory->setFactoryType(FRAME_FACTORY_TYPE_JPEG_REPROCESSING);
                m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING] = factory;
                m_frameFactoryQ->pushProcessQ(&factory);
            }

#ifdef USE_DUAL_CAMERA
            if (m_parameters[m_cameraId]->getDualMode() == true
                && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] == NULL) {
                factory = new ExynosCameraFrameReprocessingFactory(m_cameraIds[1], m_parameters[m_cameraIds[1]]);
                factory->setFrameCreateHandler(&ExynosCamera::m_captureFrameHandler);
                factory->setFrameManager(m_frameMgr);
                factory->setFactoryType(FRAME_FACTORY_TYPE_REPROCESSING_DUAL);
                m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] = factory;
                m_frameFactoryQ->pushProcessQ(&factory);
            }
#endif
        }
    } else {
        if (m_frameFactory[FRAME_FACTORY_TYPE_VISION] == NULL) {
            /* Vision Frame Factory */
            factory = new ExynosCameraFrameFactoryVision(m_cameraOriginId, m_parameters[m_cameraId]);
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
#ifdef DEBUG
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
#endif
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;
    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();
    uint32_t prepare = m_exynosconfig->current->pipeInfo.prepare[PIPE_3AA];
    enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
    bool startBayerThreads = false;
#ifdef USE_DUAL_CAMERA
    enum DUAL_OPERATION_MODE dualOperationMode = m_parameters[m_cameraId]->getDualOperationMode();
#endif

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_START_THREAD_START, 0);

    if (m_requestMgr->getServiceRequestCount() < 1) {
        CLOGE("There is NO available request!!! "
            "\"processCaptureRequest()\" must be called, first!!!");
        return false;
    }

    /* To decide to run FLITE threads  */
    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
        startBayerThreads = true;
        break;
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        startBayerThreads = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
        break;
    default :
        break;
    }

    m_internalFrameCount = 1;

    if (m_shotDoneQ != NULL) {
        m_shotDoneQ->release();
    }
#ifdef USE_DUAL_CAMERA
    if (m_dualShotDoneQ != NULL) {
        m_dualShotDoneQ->release();
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

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true
            && dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        } else
#endif
        {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        }
    } else {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
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

    /* Set default request flag & buffer manager */
    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        ret = m_setupPipeline(factory);
    } else {
        ret = m_setupVisionPipeline();
    }

    if (ret != NO_ERROR) {
        CLOGE("Failed to setupPipeline. ret %d", ret);
        return false;
    }

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_START, 0);
    ret = factory->initPipes();
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_INIT_PIPES_END, 0);
    if (ret != NO_ERROR) {
        CLOGE("Failed to initPipes. ret %d", ret);
        return false;
    }

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        ret = factory->mapBuffers();
        if (ret != NO_ERROR) {
            CLOGE("m_previewFrameFactory->mapBuffers() failed");
            return ret;
        }
    }

    /* Adjust Prepare Frame Count
     * If current sensor control delay is 0,
     * Prepare frame count must be reduced by #sensor control delay.
     */
    prepare = prepare + m_parameters[m_cameraId]->getSensorControlDelay() + 1;
    CLOGD("prepare %d", prepare);

    for (uint32_t i = 0; i < prepare; i++) {
        if (m_parameters[m_cameraId]->getVisionMode() == false) {
            ret = m_createPreviewFrameFunc(REQ_SYNC_NONE);
        } else {
            ret = m_createVisionFrameFunc(REQ_SYNC_NONE);
        }

        if (ret != NO_ERROR) {
            CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                     i, prepare);
        }
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        bool sensorStandby = false;
        if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            sensorStandby = m_parameters[m_cameraId]->isSupportMasterSensorStandby();
        } else {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            sensorStandby = m_parameters[m_cameraId]->isSupportSlaveSensorStandby();
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

        /* Set default request flag & buffer manager */
        ret = m_setupPipeline(factory);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setupPipeline. ret %d", ret);
            return false;
        }

        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SLAVE_FACTORY_INIT_PIPES_START, 0);
        ret = factory->initPipes();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SLAVE_FACTORY_INIT_PIPES_END, 0);
        if (ret != NO_ERROR) {
            CLOGE("Failed to initPipes. ret %d", ret);
            return false;
        }

        ret = factory->mapBuffers();
        if (ret != NO_ERROR) {
            CLOGE("m_previewFrameFactory->mapBuffers() failed");
            return ret;
        }

        /* Prepare buffers for post standby instance */
        if (sensorStandby == false
            && dualOperationMode != DUAL_OPERATION_MODE_SYNC) {
            for (uint32_t i = 0; i < prepare; i++) {
                ret = m_createInternalFrameFunc(NULL, REQ_SYNC_NONE, FRAME_TYPE_TRANSITION);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to createFrameFunc for preparing frame. prepareCount %d/%d",
                            i, prepare);
                }
            }
        }

        /* - call preparePipes();
         * - call startPipes()
         * - call startInitialThreads()
         */
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SLAVE_FACTORY_START_START, 0);
#if 0
        if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            m_startFrameFactory(factory);
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        } else {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            m_startFrameFactory(factory);
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        }
#else // ToDo: must start frame factory at the same time
        factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        m_startFrameFactory(factory);
        factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
#endif
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SLAVE_FACTORY_START_END, 0);
    }
#endif
    /* - call preparePipes();
     * - call startPipes()
     * - call startInitialThreads()
     */
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_START_START, 0);
    m_startFrameFactory(factory);
    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_START_END, 0);

    if (m_parameters[m_cameraId]->isReprocessing() == true && m_captureStreamExist == true) {
        m_reprocessingFrameFactoryStartThread->run();
    }

    m_mainPreviewThread->run(PRIORITY_URGENT_DISPLAY);

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
        m_previewStream3AAThread->run(PRIORITY_URGENT_DISPLAY);

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
            m_previewStreamISPThread->run(PRIORITY_URGENT_DISPLAY);
        }

#ifdef USE_DUAL_CAMERA
        if ((m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M)
            && (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW)) {
            m_previewStreamDCPThread->run(PRIORITY_URGENT_DISPLAY);
        }
#endif

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M
#ifdef USE_DUAL_CAMERA
            || m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M
#endif
                ) {
            m_previewStreamMCSCThread->run(PRIORITY_URGENT_DISPLAY);
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            m_previewStreamVRAThread->run(PRIORITY_URGENT_DISPLAY);
        }

#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true) {
            if (m_parameters[m_cameraId]->isSupportMasterSensorStandby() == false
                || m_parameters[m_cameraId]->isSupportSlaveSensorStandby() == false) {
                m_dualMainThread->run(PRIORITY_URGENT_DISPLAY);
            }
            m_previewStreamSyncThread->run(PRIORITY_URGENT_DISPLAY);

            if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                m_previewStreamFusionThread->run(PRIORITY_URGENT_DISPLAY);
            }
        }
#endif
    }

    if (startBayerThreads == true) {
        m_previewStreamBayerThread->run(PRIORITY_URGENT_DISPLAY);

        if (factory->checkPipeThreadRunning(m_getBayerPipeId()) == false)
            factory->startThread(m_getBayerPipeId());
    }

    m_monitorThread->run(PRIORITY_DEFAULT);

    ret = m_transitState(EXYNOS_CAMERA_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into RUN. ret %d", ret);
    }

    return false;
}

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

status_t ExynosCamera::m_restartStreamInternal()
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;
    int count = 0;

    CLOGI("IN");

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->join();
    m_reprocessingFrameFactoryStartThread->join();

    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
    m_setBuffersThread->join();
    m_startPictureBufferThread->join();
#else
    if (m_getState() != EXYNOS_CAMERA_STATE_RUN) {
        CLOGD("No need to m_restartStreamInternal");
        goto func_exit;
    }
#endif

    /* Wait for finishing pre-processing threads*/
    m_mainPreviewThread->requestExitAndWait();
    m_mainCaptureThread->requestExitAndWait();
#ifdef USE_DUAL_CAMERA
    m_dualMainThread->requestExitAndWait();
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

   /* Wait for finishing pre-processing threads*/
    m_shotDoneQ->release();
#ifdef USE_DUAL_CAMERA
    m_dualShotDoneQ->release();
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
#ifdef SUPPORT_HW_GDC
    stopThreadAndInputQ(m_previewStreamGDCThread, 1, m_pipeFrameDoneQ[PIPE_GDC]);
    stopThreadAndInputQ(m_gdcThread, 1, m_gdcQ);
#endif
#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        stopThreadAndInputQ(m_previewStreamSyncThread, 1, m_pipeFrameDoneQ[PIPE_SYNC]);

        if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW
            && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M) {
            stopThreadAndInputQ(m_previewStreamDCPThread, 1, m_pipeFrameDoneQ[PIPE_DCP]);
        } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            stopThreadAndInputQ(m_previewStreamFusionThread, 1, m_pipeFrameDoneQ[PIPE_FUSION]);
        }
    }
#endif
#ifdef YUV_DUMP
    stopThreadAndInputQ(m_dumpThread, 1, m_dumpFrameQ);
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

#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
    {
        buffer_manager_type_t bufferType = BUFFER_MANAGER_ION_TYPE;
        buffer_manager_tag_list_t tagList;
        m_bufferSupplier->getBufferTags(bufferType, &tagList);
        m_bufferSupplier->deinit(&tagList);
    }
#endif

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
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *frameFactory = NULL;
    List<ExynosCameraFrameSP_sptr_t> *list = NULL;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    List<ExynosCameraFrameSP_sptr_t>::iterator r;

    /*
     * This flag should be set before stoping all pipes,
     * because other func(like processCaptureRequest) must know call state about flush() entry level
     */
    m_captureResultDoneCondition.signal();

    CLOGD("IN+++");

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

    /* Wait for finishing to start pipeline */
    m_frameFactoryStartThread->requestExitAndWait();
    m_reprocessingFrameFactoryStartThread->requestExitAndWait();

    /* Wait for finishing pre-processing threads and Release pre-processing frame queues */
    stopThreadAndInputQ(m_mainPreviewThread, 1, m_shotDoneQ);
    m_mainCaptureThread->requestExitAndWait();
#ifdef USE_DUAL_CAMERA
    stopThreadAndInputQ(m_dualMainThread, 1, m_dualShotDoneQ);
#endif
    stopThreadAndInputQ(m_selectBayerThread, 1, m_selectBayerQ);

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

    /* Wait for finishing post-processing thread */
    stopThreadAndInputQ(m_previewStreamBayerThread, 1, m_pipeFrameDoneQ[PIPE_FLITE]);
    stopThreadAndInputQ(m_previewStream3AAThread, 1, m_pipeFrameDoneQ[PIPE_3AA]);
    stopThreadAndInputQ(m_previewStreamISPThread, 1, m_pipeFrameDoneQ[PIPE_ISP]);
    stopThreadAndInputQ(m_previewStreamMCSCThread, 1, m_pipeFrameDoneQ[PIPE_MCSC]);
    stopThreadAndInputQ(m_previewStreamVRAThread, 1, m_pipeFrameDoneQ[PIPE_MCSC]);
    stopThreadAndInputQ(m_previewStreamHFDThread, 1, m_pipeFrameDoneQ[PIPE_HFD]);
    stopThreadAndInputQ(m_bayerStreamThread, 1, m_bayerStreamQ);
    stopThreadAndInputQ(m_captureThread, 2, m_captureQ, m_reprocessingDoneQ);
    stopThreadAndInputQ(m_thumbnailCbThread, 2, m_thumbnailCbQ, m_thumbnailPostCbQ);
    stopThreadAndInputQ(m_captureStreamThread, 1, m_yuvCaptureDoneQ);
#ifdef SUPPORT_HW_GDC
    stopThreadAndInputQ(m_previewStreamGDCThread, 1, m_pipeFrameDoneQ[PIPE_GDC]);
    stopThreadAndInputQ(m_gdcThread, 1, m_gdcQ);
#endif
#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        stopThreadAndInputQ(m_previewStreamSyncThread, 1, m_pipeFrameDoneQ[PIPE_SYNC]);

        if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW
            && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M) {
            stopThreadAndInputQ(m_previewStreamDCPThread, 1, m_pipeFrameDoneQ[PIPE_DCP]);
        } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            stopThreadAndInputQ(m_previewStreamFusionThread, 1, m_pipeFrameDoneQ[PIPE_FUSION]);
        }
    }
#endif
#ifdef YUV_DUMP
    stopThreadAndInputQ(m_dumpThread, 1, m_dumpFrameQ);
#endif

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_captureSelector[i] != NULL) {
            m_captureSelector[i]->release();
        }
    }

    m_requestMgr->flush();

    ret = m_clearRequestList(&m_requestPreviewWaitingList, &m_requestPreviewWaitingLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }
    ret = m_clearRequestList(&m_requestCaptureWaitingList, &m_requestCaptureWaitingLock);
    if (ret < 0) {
        CLOGE("m_clearList(m_processList) failed [%d]", ret);
    }

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
    CLOGD(" : OUT---");
    return ret;
}

bool ExynosCamera::m_previewStreamFunc(ExynosCameraFrameSP_sptr_t newFrame, int pipeId)
{
    status_t ret = 0;

    if (newFrame == NULL) {
        CLOGE("frame is NULL");
        return true;
    }

    TIME_LOGGER_UPDATE(m_cameraId, newFrame->getFrameCount(), pipeId, CUMULATIVE_CNT, PREVIEW_STREAM_THREAD, 0);

    if (pipeId == PIPE_3AA) {
        m_previewDurationTimer.stop();
        m_previewDurationTime = (int)m_previewDurationTimer.durationMsecs();
        CLOGV("frame duration time(%d)", m_previewDurationTime);
        m_previewDurationTimer.start();
    }

    if (newFrame->getFrameType() == FRAME_TYPE_INTERNAL
#ifdef USE_DUAL_CAMERA
        || newFrame->getFrameType() == FRAME_TYPE_TRANSITION
#endif
        ) {
        /* Handle the internal frame for each pipe */
        ret = m_handleInternalFrame(newFrame, pipeId);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    } else if (newFrame->getFrameType() == FRAME_TYPE_VISION) {
        ret = m_handleVisionFrame(newFrame, pipeId);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    } else {
        /* TODO: M2M path is also handled by this */
        ret = m_handlePreviewFrame(newFrame, pipeId);
        if (ret < 0) {
            CLOGE("handle preview frame fail");
            return ret;
        }
    }

    return true;
}

status_t ExynosCamera::m_createCaptureFrameFunc(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *service_shot_ext = NULL;
    FrameFactoryList previewFactoryAddrList;
    FrameFactoryList captureFactoryAddrList;
    ExynosCameraFrameFactory *factory = NULL, *subFactory = NULL;
    FrameFactoryListIterator factorylistIter;
    factory_handler_t frameCreateHandler;
    List<ExynosCameraRequestSP_sprt_t>::iterator r;
    frame_type_t frameType = FRAME_TYPE_PREVIEW, subFrameType = FRAME_TYPE_PREVIEW;
    bool jpegRequest = false;
    bool stallRequest = false;

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
        if (m_parameters[m_cameraId]->getDualMode() == true) {
            switch(factory->getFactoryType()) {
            case FRAME_FACTORY_TYPE_CAPTURE_PREVIEW:
                if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                    subFrameType = FRAME_TYPE_PREVIEW_DUAL_SLAVE;
                    subFactory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                    frameCreateHandler = subFactory->getFrameCreateHandler();
                    (this->*frameCreateHandler)(request, subFactory, subFrameType);

                    frameType = FRAME_TYPE_PREVIEW_DUAL_MASTER;
                } else if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                    factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                    frameType = FRAME_TYPE_PREVIEW_SLAVE;
                }
                break;
            case FRAME_FACTORY_TYPE_REPROCESSING:
                if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                    subFrameType = FRAME_TYPE_REPROCESSING_DUAL_SLAVE;
                    subFactory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
                    frameCreateHandler = subFactory->getFrameCreateHandler();
                    (this->*frameCreateHandler)(request, subFactory, subFrameType);

                    frameType = FRAME_TYPE_REPROCESSING_DUAL_MASTER;
                } else if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                    factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL];
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

status_t ExynosCamera::m_createInternalFrameFunc(ExynosCameraRequestSP_sprt_t request,
                                                 enum Request_Sync_Type syncType,
                                                 frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    bool isNeedBayer = (request != NULL) ? true : false;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    int pipeId = -1;
    int dstPos = 0;
    bool requestVC0 = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;

    if (m_parameters[m_cameraId]->getVisionMode() == false) {
#ifdef USE_DUAL_CAMERA
        enum DUAL_OPERATION_MODE dualOperationMode = m_parameters[m_cameraId]->getDualOperationMode();
        if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
            /* This case is slave internal frame on SYNC mode */
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            m_internalFrameCount--;
        } else if (frameType == FRAME_TYPE_INTERNAL
                 && dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
            /* This case is slave internal frame on SLAVE mode */
            factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
        } else if (frameType == FRAME_TYPE_TRANSITION) {
            if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                if (m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->isSensorStandby() == true) {
                    factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
                } else if (m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL]->isSensorStandby() == true) {
                    factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
                }
            } else if (dualOperationMode == DUAL_OPERATION_MODE_MASTER) {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
            } else if (dualOperationMode == DUAL_OPERATION_MODE_SLAVE) {
                factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
            }
        } else
#endif
        {
            factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
        }
    } else {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    }

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
    factory->setRequest(PIPE_DCPS0, false);
    factory->setRequest(PIPE_DCPS1, false);
    factory->setRequest(PIPE_DCPC0, false);
    factory->setRequest(PIPE_DCPC1, false);
    factory->setRequest(PIPE_DCPC2, false);
    factory->setRequest(PIPE_DCPC3, false);
    factory->setRequest(PIPE_DCPC4, false);
    factory->setRequest(PIPE_SYNC, false);
    factory->setRequest(PIPE_FUSION, false);

    if (frameType != FRAME_TYPE_TRANSITION)
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
            break;
        default:
            break;
        }
    }

    if (m_parameters[m_cameraId]->getVisionMode() == true){
        factory->setRequest(PIPE_VC0, false);
    }

    /* Generate the internal frame */
    if (request != NULL) {
        /* Set framecount into request */
        if (request->getFrameCount() == 0)
            m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

        ret = m_generateInternalFrame(factory, &m_processList, &m_processLock, newFrame, request);
#ifdef USE_DUAL_CAMERA
    } else if (frameType == FRAME_TYPE_TRANSITION) {
        ret = m_generateTransitionFrame(factory, &m_processList, &m_processLock, newFrame);
#endif

    } else {
        ret = m_generateInternalFrame(factory, &m_processList, &m_processLock, newFrame);
    }

    if (ret != NO_ERROR) {
        CLOGE("m_generateFrame failed");
        return ret;
    } else if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return INVALID_OPERATION;
    }

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        newFrame->setFrameType(FRAME_TYPE_TRANSITION);
    }
#endif

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

    /* Update the metadata with m_currentPreviewShot into frame */
    if (m_currentPreviewShot == NULL) {
        CLOGE("m_currentPreviewShot is NULL");
        return INVALID_OPERATION;
    }

    ret = newFrame->setMetaData(m_currentPreviewShot);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setMetaData with m_currehtShot. framecount %d ret %d",
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

status_t ExynosCamera::m_handleBayerBuffer(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;
    uint32_t bufferDirection = INVALID_BUFFER_DIRECTION;
    ExynosCameraBuffer buffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_COMPLETE;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int dstPos = 0;
    bool flite3aaM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);

    if (frame == NULL) {
        CLOGE("Frame is NULL");
        return BAD_VALUE;
    }

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
            if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
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
                return ret;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                        frame->getFrameCount(), buffer.index, dstPos, ret);
                return ret;
            }
            break;
        default:
            CLOGE("[F%d]Invalid bayer handling PipeId %d", frame->getFrameCount(), pipeId);
            return INVALID_OPERATION;
    }

    assert(buffer != NULL);

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        ExynosCameraRequestSP_sprt_t request = NULL;

        CLOGE("[F%d B%d]Invalid bayer buffer state. pipeId %d",
                frame->getFrameCount(), buffer.index, pipeId);

        request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("[F%d] request is NULL.", frame->getFrameCount());
            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret !=  NO_ERROR) {
                    CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                            frame->getFrameCount(), buffer.index,
                            bufferDirection, pipeId, ret);
                    }
                }

            goto SKIP_PUTBUFFER;
        }

        ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d B%d]Failed to sendNotifyError. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, ret);
            return ret;
        }
    } else if (buffer.index < 0) {
        CLOGE("[F%d B%d]Invalid bayer buffer. pipeId %d",
                frame->getFrameCount(), buffer.index, pipeId);
        return INVALID_OPERATION;
    }

    if (frame->getStreamRequested(STREAM_TYPE_ZSL_OUTPUT)) {
        ExynosCameraRequestSP_sprt_t request = m_requestMgr->getRunningRequest(frame->getFrameCount());

        CLOGV("[F%d B%d]Handle ZSL buffer. FLITE-3AA_%s bayerPipeId %d",
                frame->getFrameCount(), buffer.index,
                flite3aaM2M ? "M2M" : "OTF",
                pipeId);

        if (request == NULL) {
            CLOGE("[F%d]request is NULL.", frame->getFrameCount());
            return INVALID_OPERATION;
        }

        ret = m_sendZslStreamResult(request, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to sendZslCaptureResult. bayerPipeId %d ret %d",
                    frame->getFrameCount(), buffer.index, pipeId, ret);
        }

        goto SKIP_PUTBUFFER;
    } else if (m_parameters[m_cameraId]->isReprocessing() == true) {
        switch (m_parameters[m_cameraId]->getReprocessingBayerMode()) {
            case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
            case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
                CLOGV("[F%d B%d]Hold internal bayer buffer for reprocessing in Always On."
                        " FLITE-3AA_%s bayerPipeId %d",
                        frame->getFrameCount(), buffer.index,
                        flite3aaM2M ? "M2M" : "OTF", pipeId);
#ifdef USE_DUAL_CAMERA
                if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                    frame->addSelectorTag(m_captureSelector[m_cameraIds[1]]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                    ret = m_captureSelector[m_cameraIds[1]]->manageFrameHoldListHAL3(frame);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to manageFrameHoldListHAL3. bufDirection %d bayerPipeId %d ret %d",
                                frame->getFrameCount(), buffer.index,
                                bufferDirection, pipeId, ret);
                    }
                } else
#endif
                {
                    frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                    ret = m_captureSelector[m_cameraId]->manageFrameHoldListHAL3(frame);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to manageFrameHoldListHAL3. bufDirection %d bayerPipeId %d ret %d",
                                frame->getFrameCount(), buffer.index,
                                bufferDirection, pipeId, ret);
                    }
                }

                break;
            case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
            case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
                if (frame->getStreamRequested(STREAM_TYPE_CAPTURE) || frame->getStreamRequested(STREAM_TYPE_RAW)
                    || m_needDynamicBayerCount > 0) {
                    CLOGV("[F%d B%d]Hold internal bayer buffer for reprocessing in Dynamic."
                            " FLITE-3AA_%s bayerPipeId %d",
                            frame->getFrameCount(), buffer.index,
                            flite3aaM2M ? "M2M" : "OTF", pipeId);

#ifdef USE_DUAL_CAMERA
                    if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                        frame->addSelectorTag(m_captureSelector[m_cameraIds[1]]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                        ret = m_captureSelector[m_cameraIds[1]]->manageFrameHoldListForDynamicBayer(frame);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to manageFrameHoldListForDynamicBayer."
                                    " bufDirection %d bayerPipeId %d ret %d",
                                    frame->getFrameCount(), buffer.index,
                                    bufferDirection, pipeId, ret);
                        }
                    } else
#endif
                    {
                        frame->addSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, (bufferDirection == SRC_BUFFER_DIRECTION));
                        ret = m_captureSelector[m_cameraId]->manageFrameHoldListForDynamicBayer(frame);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to manageFrameHoldListForDynamicBayer."
                                    " bufDirection %d bayerPipeId %d ret %d",
                                    frame->getFrameCount(), buffer.index,
                                    bufferDirection, pipeId, ret);
                        }
                    }
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
                        m_parameters[m_cameraId]->getReprocessingBayerMode());

                ret = INVALID_OPERATION;
                break;
        }

        goto CHECK_RET_PUTBUFFER;
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

    return ret;

CHECK_RET_PUTBUFFER:
    /* Put the bayer buffer if there was an error during processing */
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Handling bayer buffer failed. Bayer buffer will be put."
                " isServiceBayer %d, bufDirection %d pipeId %d ret %d",
                frame->getFrameCount(), buffer.index,
                frame->getStreamRequested(STREAM_TYPE_RAW),
                bufferDirection, pipeId, ret);

        if (m_bufferSupplier->putBuffer(buffer) !=  NO_ERROR) {
            // Do not taint 'ret' to print appropirate error log on next block.
            CLOGE("[F%d B%d]PutBuffers failed. bufDirection %d pipeId %d ret %d",
                    frame->getFrameCount(), buffer.index,
                    bufferDirection, bufferDirection, ret);
        }
    }

SKIP_PUTBUFFER:
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
    depthBuffer.index = -2;

    ret = frame->getDstBuffer(pipeDepthId, &depthBuffer, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Failed to get DepthMap buffer. ret %d",
                request->getKey(), frame->getFrameCount(), ret);
    }

    ret = frame->getDstBufferState(pipeDepthId, &bufferState, dstPos);
    if (ret != NO_ERROR) {
        CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                frame->getFrameCount(), depthBuffer.index, dstPos, ret);
        return ret;
    }

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        streamBufferStae = CAMERA3_BUFFER_STATUS_ERROR;
        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                depthBuffer.index, frame->getFrameCount(), pipeDepthId);
    }

    request->setStreamBufferStatus(streamId, streamBufferStae);

    ret = m_sendDepthStreamResult(request, &depthBuffer, streamId);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d B%d]Failed to m_sendDepthStreamResult. ret %d",
                request->getKey(), frame->getFrameCount(), depthBuffer.index, ret);
    }

    frame->setRequest(PIPE_VC1, false);

    return ret;
}
#endif

status_t ExynosCamera::processCaptureRequest(camera3_capture_request *request)
{
    status_t ret = NO_ERROR;
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

#if 0
    // For Fence FD debugging
    if (request->input_buffer != NULL){
        const camera3_stream_buffer* pStreamBuf = request->input_buffer;
        CLOGD("[Input] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
             request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
    }
    for(int i = 0; i < request->num_output_buffers; i++) {
        const camera3_stream_buffer* pStreamBuf = &(request->output_buffers[i]);
        CLOGD("[Output] Index[%d/%d] acqureFence[%d], releaseFence[%d], Status[%d], Buffer[%p]",
             i, request->num_output_buffers,
            pStreamBuf->acquire_fence, pStreamBuf->release_fence, pStreamBuf->status, pStreamBuf->buffer);
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

    /* 7. Get FactoryAddr */
    ret = m_setFactoryAddr(req);
    captureFactoryAddrList.clear();
    req->getFactoryAddrList(FRAME_FACTORY_TYPE_REPROCESSING, &captureFactoryAddrList);

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
    m_parameters[m_cameraId]->getPreviewFpsRange(&minFps, &maxFps);
    timeOutNs = (1000 / ((minFps == 0) ? 15 : minFps)) * 1000000;

    /* 10. Process initial requests for preparing the stream */
    if (m_getState() == EXYNOS_CAMERA_STATE_CONFIGURED) {
        ret = m_transitState(EXYNOS_CAMERA_STATE_START);
        if (ret != NO_ERROR) {
            CLOGE("[R%d]Failed to transitState into START. ret %d", request->frame_number, ret);
            goto req_err;
        }

        CLOGD("[R%d]Start FrameFactory. state %d ", request->frame_number, m_getState());

#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true) {
            m_checkDualOperationMode(req);
            if (ret != NO_ERROR) {
                CLOGE("m_checkDualOperationMode fail! ret(%d)", ret);
            }
        }
#endif

        m_firstRequestFrameNumber = request->frame_number;

        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SET_BUFFER_THREAD_JOIN_START, 0);
        m_setBuffersThread->join();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, SET_BUFFER_THREAD_JOIN_END, 0);

        m_framefactoryCreateThread->join();
        m_frameFactoryStartThread->run();
    }

    /* Adjust Initial Request Count
     * #sensor control delay > 0
     * #sensor control delay frames will be generated as internal frame.
     * => Initially required request count == Prepare frame count - #sensor control delay
     *
     * #sensor control delay == 0
     * Originally required prepare frame count + 1(HAL request margin) is required.
     * => Initially requred request count == Prepare frame count - SENSOR_CONTROL_DELAY + 1
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

    while ((request->frame_number - m_firstRequestFrameNumber) > initialRequestCount
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

    captureFactoryAddrList.clear();

req_err:
    return ret;
}

status_t ExynosCamera::m_checkStreamBuffer(ExynosCameraFrameSP_sptr_t frame, ExynosCameraStream *stream,
                                            ExynosCameraBuffer *buffer, ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameFactory *factory = NULL;
    entity_buffer_state_t bufferState;

    int bufferIndex = -2;
    int streamId = -1;
    bool usePureBayer = false;

    int pipeId = -1;
    int parentPipeId = -1;

    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif

    enum FRAME_TYPE frameType = (enum FRAME_TYPE)frame->getFrameType();

    if (frameType == FRAME_TYPE_REPROCESSING
#ifdef USE_DUAL_CAMERA
        || frameType == FRAME_TYPE_REPROCESSING_DUAL_MASTER
#endif
        ) {
        bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
        bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
        bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
        bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif
    }

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

    switch (reprocessingBayerMode) {
    case REPROCESSING_BAYER_MODE_NONE :
        break;
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC :
        usePureBayer = true;
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON :
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC :
        usePureBayer = false;
        break;
    default :
        break;
    }

    stream->getID(&streamId);

    switch (streamId % HAL_STREAM_ID_MAX) {
    case HAL_STREAM_ID_RAW:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_RAW)",
                request->getKey(), frame->getFrameCount());
        parentPipeId = PIPE_3AA;
        pipeId = PIPE_VC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_ZSL_OUTPUT:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_ZSL)",
            request->getKey(), frame->getFrameCount());
        parentPipeId = PIPE_3AA;
        pipeId = (usePureBayer == true) ? PIPE_VC0 : PIPE_3AC;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_PREVIEW:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_PREVIEW)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_CALLBACK:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_CALLBACK_STALL:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_CALLBACK_STALL)",
            request->getKey(), frame->getFrameCount());
        pipeId = (m_streamManager->getOutputPortId(streamId) % ExynosCameraParameters::YUV_MAX)
                + PIPE_MCSC0_REPROCESSING;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_VIDEO:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_VIDEO)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_PREVIEW_VIDEO:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_PREVIEW_VIDEO)",
            request->getKey(), frame->getFrameCount());
        pipeId = m_streamManager->getOutputPortId(streamId) + PIPE_MCSC0;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_JPEG:
        CLOGD("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_JPEG)",
            request->getKey(), frame->getFrameCount());
        pipeId = (m_parameters[m_cameraId]->isUseHWFC() == true) ? PIPE_HWFC_JPEG_DST_REPROCESSING : PIPE_JPEG_REPROCESSING;
        factory = request->getFrameFactory(streamId);
        break;
    case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_THUMBNAIL_CALLBACK)",
            request->getKey(), frame->getFrameCount());
        pipeId = PIPE_GSC_REPROCESSING2;
        factory = request->getFrameFactory(streamId);
        break;
#ifdef SUPPORT_DEPTH_MAP
    case HAL_STREAM_ID_DEPTHMAP:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_DEPTHMAP)",
            request->getKey(), frame->getFrameCount());
        parentPipeId = PIPE_3AA;
        pipeId = PIPE_VC1;
        factory = request->getFrameFactory(streamId);
        break;
#endif
    case HAL_STREAM_ID_VISION:
        CLOGV("[R%d F%d] buffer-StreamType(HAL_STREAM_ID_VISION)",
            request->getKey(), frame->getFrameCount());
        parentPipeId = PIPE_FLITE;
        pipeId = PIPE_VC0;
        factory = request->getFrameFactory(streamId);
        break;
    default:
        CLOGE("Invalid stream ID %d", streamId);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    if (parentPipeId < 0) {
        switch (frameType) {
        case FRAME_TYPE_PREVIEW:
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_PREVIEW_SLAVE:
#endif
            if (flagIspMcscM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                parentPipeId = PIPE_MCSC;
            } else if (flag3aaIspM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                parentPipeId = PIPE_ISP;
            } else {
                parentPipeId = PIPE_3AA;
            }
            break;
        case FRAME_TYPE_REPROCESSING:
            if (flagIspMcscM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                parentPipeId = PIPE_MCSC_REPROCESSING;
            } else if (flag3aaIspM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                parentPipeId = PIPE_ISP_REPROCESSING;
            } else {
                parentPipeId = PIPE_3AA_REPROCESSING;
            }
            break;
#ifdef USE_DUAL_CAMERA
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
            if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                if (flagDcpMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                    parentPipeId = PIPE_MCSC;
                } else if (flagIspDcpM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
                    parentPipeId = PIPE_DCP;
                } else if (flag3aaIspM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                    parentPipeId = PIPE_ISP;
                } else {
                    parentPipeId = PIPE_3AA;
                }
            } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                if (flagIspMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                    parentPipeId = PIPE_MCSC;
                } else if (flag3aaIspM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                    parentPipeId = PIPE_ISP;
                } else {
                    parentPipeId = PIPE_3AA;
                }
            }
            break;
        case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
            if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
                if (flagDcpMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                    parentPipeId = PIPE_MCSC_REPROCESSING;
                } else if (flagIspDcpM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
                    parentPipeId = PIPE_DCP_REPROCESSING;
                } else if (flag3aaIspM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                    parentPipeId = PIPE_ISP_REPROCESSING;
                } else {
                    parentPipeId = PIPE_3AA_REPROCESSING;
                }
            } else if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
                if (flagIspMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                    parentPipeId = PIPE_MCSC_REPROCESSING;
                } else if (flag3aaIspM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                    parentPipeId = PIPE_ISP_REPROCESSING;
                } else {
                    parentPipeId = PIPE_3AA_REPROCESSING;
                }
            }
            break;
#endif
        default:
            break;
        }
    }

    if (pipeId > 0) {
        if (pipeId == PIPE_FLITE || pipeId == PIPE_JPEG_REPROCESSING || pipeId == PIPE_GSC_REPROCESSING2) {
            ret = frame->getDstBuffer(pipeId, buffer);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d] getDstBuffer fail. pipeId (%d) ret(%d)",
                    request->getKey(), frame->getFrameCount(), pipeId, ret);
                goto func_exit;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d] getDstBufferState fail. pipeId (%d) ret(%d)",
                    request->getKey(), frame->getFrameCount(), pipeId, ret);
                goto func_exit;
            }
        } else {
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
    }

    if (((buffer->index) < 0) || (bufferState == ENTITY_BUFFER_STATE_ERROR)) {
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
    int captureIntent;
    ExynosCameraFrameFactory *factory = NULL;
    ExynosCameraActivitySpecialCapture *sCaptureMgr = m_activityControl->getSpecialCaptureMgr();
    uint32_t streamConfigBit = 0;
    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();
    ExynosCameraActivityFlash *m_flashMgr = m_activityControl->getFlashMgr();
    camera3_stream_buffer_t* buffer = request->getInputBuffer();

    if (targetfactory == NULL) {
        CLOGE("targetfactory is NULL");
        return INVALID_OPERATION;
    }

    CLOGD("Capture request. requestKey %d frameCount %d frameType(%d)",
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

    if (m_parameters[m_cameraId]->isReprocessing() == true) {
        if (m_parameters[m_cameraId]->isUseHWFC()) {
            targetfactory->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, false);
            targetfactory->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, false);
        }
    }

    m_parameters[m_cameraId]->setYuvStallPortUsage(YUV_STALL_USAGE_DSCALED);

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        targetfactory->setRequest(PIPE_DCPS0_REPROCESSING, false);
        targetfactory->setRequest(PIPE_DCPC0_REPROCESSING, false);
        targetfactory->setRequest(PIPE_DCPS1_REPROCESSING, false);
        targetfactory->setRequest(PIPE_DCPC1_REPROCESSING, false);
        targetfactory->setRequest(PIPE_DCPC3_REPROCESSING, false);
        targetfactory->setRequest(PIPE_DCPC4_REPROCESSING, false);
        targetfactory->setRequest(PIPE_SYNC_REPROCESSING, false);
        targetfactory->setRequest(PIPE_FUSION_REPROCESSING, false);

        if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
            if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                switch (frameType) {
                case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
                    targetfactory->setRequest(PIPE_ISPC_REPROCESSING, true);
                    goto GENERATE_FRAME;
                    break;
                case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                    targetfactory->setRequest(PIPE_SYNC_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_ISPC_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPS0_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPS1_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPC0_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPC1_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPC3_REPROCESSING, true);
                    targetfactory->setRequest(PIPE_DCPC4_REPROCESSING, true);
                    break;
                case FRAME_TYPE_REPROCESSING:
                    break;
                default:
                    CLOGE("Unsupported frame type(%d)", (int)frameType);
                    break;
                }
            }
        } else if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
            if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                switch(frameType) {
                case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
                    targetfactory->setRequest(PIPE_MCSC_JPEG_REPROCESSING, true);
                    goto GENERATE_FRAME;
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
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

        switch (id % HAL_STREAM_ID_MAX) {
        case HAL_STREAM_ID_JPEG:
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_JPEG), YuvStallPortUsage()(%d)",
                    request->getKey(), m_parameters[m_cameraId]->getYuvStallPortUsage());
            if (m_parameters[m_cameraId]->getYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) {
                pipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;
                targetfactory->setRequest(pipeId, true);
            } else {
                targetfactory->setRequest(PIPE_MCSC_JPEG_REPROCESSING, true);

                shot_ext = request->getServiceShot();
                if (shot_ext == NULL) {
                    CLOGE("Get service shot fail. requestKey(%d)", request->getKey());

                    continue;
                }

                if (m_parameters[m_cameraId]->isReprocessing() == true && m_parameters[m_cameraId]->isUseHWFC() == true) {
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
            captureFlag = true;
            yuvCbStallFlag = true;
            m_parameters[m_cameraId]->setYuvStallPortUsage(YUV_STALL_USAGE_PICTURE);
            break;
        case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
            CLOGD("requestKey %d buffer-StreamType(HAL_STREAM_ID_THUMBNAIL_CALLBACK), YuvStallPortUsage()(%d)",
                    request->getKey(), m_parameters[m_cameraId]->getYuvStallPortUsage());
            thumbnailCbFlag = true;
            pipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;
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

GENERATE_FRAME:
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        return INVALID_OPERATION;
    }

    m_updateCropRegion(m_currentCaptureShot);
    m_updateJpegControlInfo(m_currentCaptureShot);
    m_updateFD(m_currentCaptureShot, service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), true);
    m_parameters[m_cameraId]->setDsInputPortId(m_currentCaptureShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], true);
    m_updateEdgeNoiseMode(m_currentCaptureShot, true);
    m_updateSetfile(m_currentCaptureShot, true);

    yuvStallPortUsage = m_parameters[m_cameraId]->getYuvStallPortUsage();

    if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M
#ifdef USE_DUAL_CAMERA
        && frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE/* HACK: temp for dual capture */
#endif
        && m_currentCaptureShot->fd_bypass == false) {
        targetfactory->setRequest(PIPE_MCSC5_REPROCESSING, true);
        targetfactory->setRequest(PIPE_VRA_REPROCESSING, true);
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

#ifdef USE_DUAL_CAMERA
    if (frameType != FRAME_TYPE_REPROCESSING_DUAL_SLAVE)
#endif
    {
        ret = m_setupCaptureFactoryBuffers(request, newFrame);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d]Failed to setupCaptureStreamBuffer. ret %d",
                    request->getKey(), newFrame->getFrameCount(), ret);
        }
    }

    m_checkUpdateResult(newFrame, streamConfigBit);

#ifdef USE_LLS_REPROCESSING
    if (m_parameters[m_cameraId]->getLLSCaptureOn() == true)
        newFrame->setFrameIndex(m_parameters[m_cameraId]->getLLSCaptureCount());
#endif

    m_selectBayerQ->pushProcessQ(&newFrame);

    if (m_selectBayerThread != NULL && m_selectBayerThread->isRunning() == false) {
        m_selectBayerThread->run();
        CLOGI("Initiate selectBayerThread (%d)", m_selectBayerThread->getTid());
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
                                         ExynosCameraFrameSP_dptr_t frame,
                                         ExynosCameraFrameSelector *selector
#ifdef SUPPORT_DEPTH_MAP
                                         , ExynosCameraBuffer *depthMapBuffer
#endif
        )
{
    status_t ret = NO_ERROR;
    int dstPos = 0;
    int retryCount = 30; /* 200ms x 30 */
    int retrySelectCount = 3;
    camera2_shot_ext *shot_ext = NULL;
    camera2_stream backup_shot_stream;
    ExynosCameraFrameSP_sptr_t inListFrame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();

    if (m_parameters[m_cameraId]->isReprocessing() == false || selector == NULL) {
        CLOGE("[F%d]INVALID_OPERATION, isReprocessing(%s) or selector is NULL",
                frameCount, m_parameters[m_cameraId]->isReprocessing() ? "True" : "False");
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    selector->setWaitTime(200000000);

    switch (m_parameters[m_cameraId]->getReprocessingBayerMode()) {
    case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
    case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        dstPos = factory->getNodeType(PIPE_VC0);
        break;
    case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
    case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
        dstPos = factory->getNodeType(PIPE_3AC);
        break;
    default:
        break;
    }

RETRY_TO_SELECT_FRAME:
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
        dstPos = factory->getNodeType(PIPE_VC1);

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

#ifdef SUPPORT_ME
    /* Wait to be enabled ME Data */
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

    shot_ext = (struct camera2_shot_ext *)buffer->addr[buffer->getMetaPlaneIndex()];
    /* 1. should be backup the camera2_stream */
    memcpy(&backup_shot_stream, &shot_ext->reserved_stream, sizeof(struct camera2_stream));

    /* 2. get preview's all meta data */
    ret = bayerFrame->getMetaData(shot_ext);
    if (ret != NO_ERROR) {
        CLOGE("[F%d(%d) B%d]Failed to getMetaData from frame.",
                bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount, buffer->index);
        goto CLEAN;
    }

    /* 3. restore the camera2_stream */
    memcpy(&shot_ext->reserved_stream, &backup_shot_stream, sizeof(struct camera2_stream));

    CLOGD("[F%d(%d) B%d]Frame is selected",
            bayerFrame->getFrameCount(), shot_ext->shot.dm.request.frameCount, buffer->index);

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

status_t ExynosCamera::m_updateResultShot(ExynosCameraRequestSP_sprt_t request,
                                           struct camera2_shot_ext *src_ext,
                                           enum metadata_type metaType)
{
    // TODO: More efficient implementation is required.
    Mutex::Autolock lock(m_updateMetaLock);

    status_t ret = NO_ERROR;
    struct camera2_shot_ext *dst_ext = NULL;
    uint8_t currentPipelineDepth = 0;

    dst_ext = request->getServiceShot();
    if (dst_ext == NULL) {
        CLOGE("getServiceShot failed.");
        return INVALID_OPERATION;
    }

    currentPipelineDepth = dst_ext->shot.dm.request.pipelineDepth;
    memcpy(&dst_ext->shot.dm, &src_ext->shot.dm, sizeof(struct camera2_dm));
    memcpy(&dst_ext->shot.udm, &src_ext->shot.udm, sizeof(struct camera2_udm));
    dst_ext->shot.dm.request.pipelineDepth = currentPipelineDepth;

    if (request->getSensorTimestamp() == 0) {
        request->setSensorTimestamp(src_ext->shot.udm.sensor.timeStampBoot);
    }

    dst_ext->shot.udm.sensor.timeStampBoot = request->getSensorTimestamp();

    ret = m_metadataConverter->updateDynamicMeta(request, metaType);

    CLOGV("[R%d F%d f%d M%d] Set result.",
            request->getKey(), request->getFrameCount(),
            dst_ext->shot.dm.request.frameCount, metaType);

    return ret;
}
status_t ExynosCamera::m_updateJpegPartialResultShot(ExynosCameraRequestSP_sprt_t request)
{
    status_t ret = NO_ERROR;
    struct camera2_shot_ext *dst_ext = NULL;

    dst_ext = request->getServiceShot();
    if (dst_ext == NULL) {
        CLOGE("getServiceShot failed.");
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

    if (factory != NULL && (factoryReprocessing != NULL || factoryJpegReprocessing != NULL)) {
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

    for (size_t i = 0; i < streamList->num_streams; i++) {
        camera3_stream_t *newStream = streamList->streams[i];
        if (newStream == NULL) {
            CLOGE("Stream index %zu was NULL", i);
            return BAD_VALUE;
        }
        // for debug
        CLOGD("Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d)",
            newStream, i, newStream->stream_type, newStream->usage,
            newStream->format, newStream->width, newStream->height);

        if ((int)(newStream->width) <= 0
            || (int)(newStream->height) <= 0
            || newStream->format < 0
            || newStream->rotation < 0) {
            CLOGE("Invalid Stream(%p), ID(%zu), type(%d), usage(%#x) format(%#x) w(%d),h(%d)",
                    newStream, i, newStream->stream_type, newStream->usage,
                    newStream->format, (int)(newStream->width), (int)(newStream->height));
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
                || (newStream->format == HAL_PIXEL_FORMAT_YCbCr_420_888)) {
                m_captureStreamExist = true;
            }
        }

        if ((newStream->format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
            && (newStream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER)) {
            CLOGI("recording stream checked");
            m_videoStreamExist = true;
        }

        // TODO: format validation
    }

    recordingFps = m_parameters[m_cameraId]->getRecordingFps();

    if (streamList->operation_mode == CAMERA3_STREAM_CONFIGURATION_CONSTRAINED_HIGH_SPEED_MODE) {
        CLOGI("High speed mode is configured. StreamCount %d. m_videoStreamExist(%d) recordingFps(%d)",
                streamList->num_streams, m_videoStreamExist, recordingFps);

        if (m_videoStreamExist == true && recordingFps == 120) {
            m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_120);
        } else if (m_videoStreamExist == true && recordingFps == 240) {
            m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_240);
        } else if (m_videoStreamExist == true && recordingFps == 480) {
            m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_480);
        } else {
            if (m_parameters[m_cameraId]->getMaxHighSpeedFps() == 480) {
                m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_480);
            } else if (m_parameters[m_cameraId]->getMaxHighSpeedFps() == 240) {
                m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_240);
            } else {
                m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_120);
            }
        }
    } else {
        if (m_videoStreamExist == true && recordingFps == 60) {
            m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::HIGHSPEED_60);
        } else {
            m_parameters[m_cameraId]->setHighSpeedMode(CONFIG_MODE::NORMAL);
        }
    }
    m_exynosconfig = m_parameters[m_cameraId]->getConfig();

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
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_jpeg_buffers;
            break;
        case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            if ((stream->usage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE || stream->usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER)
                && (stream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER)) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE|HWCOMPOSER|VIDEO_ENCODER foramt(%#x) usage(%#x) stream_type(%#x), stream_option(%#x)",
                    stream->format, stream->usage, stream->stream_type, stream->option);
                enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
                int batchSize = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
                id = HAL_STREAM_ID_PREVIEW_VIDEO;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */
                planeCount = 2;
                stream->usage &= ~GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL;
#ifdef USE_SERVICE_BATCH_MODE
                if (batchSize > 1 && m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                    CLOGD("Use service batch mode. Add HFR_MODE usage bit. batchSize %d", batchSize);
                    stream->usage |= GRALLOC1_PRODUCER_USAGE_HFR_MODE;
                }
#endif
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if (stream->usage & GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE
                || stream->usage & GRALLOC1_CONSUMER_USAGE_HWCOMPOSER) {
                CLOGD("GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE|HWCOMPOSER foramt(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_PREVIEW;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M;
#if 0           /* To enable this, Camera framework bug must be fixed about keeping buffer IDs */
                if (m_videoStreamExist == true) {
                    /* remove yuv full range bit. */
                    stream->usage &= ~GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL;
                }
#endif

                /* Cached for PP Pipe */
                stream->usage |= (GRALLOC1_PRODUCER_USAGE_CPU_READ_OFTEN
                                  | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN);
                planeCount = 2;
                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_preview_buffers;
            } else if(stream->usage & GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER) {
                enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
                int batchSize = m_parameters[m_cameraId]->getBatchSize(controlPipeId);
                id = HAL_STREAM_ID_VIDEO;
#ifdef USE_YSUM_RECORDING
                stream->usage |= GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA;
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_PRIV;
                planeCount = 3;
#else
                actualFormat = HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M; /* NV12M */
                planeCount = 2;
#endif
#ifdef USE_SERVICE_BATCH_MODE
                if (batchSize > 1 && m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                    CLOGD("Use service batch mode. Add HFR_MODE usage bit. batchSize %d", batchSize);
                    stream->usage |= GRALLOC1_PRODUCER_USAGE_HFR_MODE;
                }
#endif

                outputPortId = m_streamManager->getTotalYuvStreamCount();
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_video_buffers;
            } else if((stream->usage & GRALLOC1_CONSUMER_USAGE_CAMERA) && (stream->usage & GRALLOC1_PRODUCER_USAGE_CAMERA)) {
                CLOGD("GRALLOC1_USAGE_CAMERA(ZSL) format(%#x) usage(%#x) stream_type(%#x)",
                      stream->format, stream->usage, stream->stream_type);
                id = HAL_STREAM_ID_ZSL_OUTPUT;
                actualFormat = HAL_PIXEL_FORMAT_RAW16;
                planeCount = 1;
                outputPortId = 0;
                requestBuffer = m_exynosconfig->current->bufInfo.num_request_bayer_buffers;
            } else {
                CLOGE("HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED unknown usage(%#x)"
                    " format(%#x) stream_type(%#x)",
                    stream->usage, stream->format, stream->stream_type);
                ret = BAD_VALUE;
                goto func_err;
            }
            break;
        case HAL_PIXEL_FORMAT_YCbCr_420_888:
            CLOGD("HAL_PIXEL_FORMAT_YCbCr_420_888 format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);

            id = HAL_STREAM_ID_CALLBACK;
            outputPortId = m_streamManager->getTotalYuvStreamCount();
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_callback_buffers;

            actualFormat = HAL_PIXEL_FORMAT_YCrCb_420_SP;
            planeCount = 1;
            break;
        /* case HAL_PIXEL_FORMAT_RAW_SENSOR: */
        case HAL_PIXEL_FORMAT_RAW16:
            CLOGD("HAL_PIXEL_FORMAT_RAW_XXX format(%#x) usage(%#x) stream_type(%#x)",
                stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_RAW;
            actualFormat = HAL_PIXEL_FORMAT_RAW16;
            stream->usage |= GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN;   /* Cached & NeedSync */

            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
#if 0
#ifdef SUPPORT_DEPTH_MAP
        case HAL_PIXEL_FORMAT_Y16:
            CLOGD("HAL_PIXEL_FORMAT_Y16 format(%#x) usage(%#x) stream_type(%#x)",
                    stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_DEPTHMAP;
            actualFormat = HAL_PIXEL_FORMAT_Y16;

            planeCount = 1;
            outputPortId = 0;
            requestBuffer = m_exynosconfig->current->bufInfo.num_request_raw_buffers;
            break;
#endif
#endif
        case HAL_PIXEL_FORMAT_Y8:
            CLOGD("HAL_PIXEL_FORMAT_Y8 format(%#x) usage(%#x) stream_type(%#x)",
                stream->format, stream->usage, stream->stream_type);
            id = HAL_STREAM_ID_VISION;
            actualFormat = HAL_PIXEL_FORMAT_Y8;
            planeCount = 1;
            outputPortId = 0;
            requestBuffer = RESERVED_NUM_SECURE_BUFFERS;
            if (m_scenario == SCENARIO_SECURE) {
                stream->usage |= GRALLOC1_PRODUCER_USAGE_SECURE_CAMERA_RESERVED;
            }
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
        CLOGE("Invalid stream_type %d",   stream->stream_type);
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
    exynos_camera_buffer_type_t type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
    unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES]    = {0};
    int width = 0, height = 0;
    int hwWidth = 0, hwHeight = 0;
    int planeCount = 0;
    int streamPlaneCount = 0;
    int streamPixelFormat = 0;
    int outputPortId = 0;
    int currentConfigMode = m_parameters[m_cameraId]->getConfigMode();
#ifdef SUPPORT_DEPTH_MAP
    int depthInternalBufferCount = 0;
#endif

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
        CLOGI("restart frame factory m_captureStreamExist(%d) m_rawStreamExist(%d),"
            " m_videoStreamExist(%d), stream_list->num_streams(%d)",
            m_captureStreamExist, m_rawStreamExist,
            m_videoStreamExist, stream_list->num_streams);

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
        if (m_parameters[m_cameraId]->getDualMode() == true
            && m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL] != NULL) {
            if (m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]->isCreated() == true) {
                ret = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]->destroy();
                if (ret < 0)
                    CLOGE("m_frameFactory[%d] destroy fail", FRAME_FACTORY_TYPE_REPROCESSING_DUAL);
            }
            SAFE_DELETE(m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING_DUAL]);

            CLOGD("m_frameFactory[%d] destroyed", FRAME_FACTORY_TYPE_REPROCESSING_DUAL);
        }
#endif

        /* restart frame manager */
        m_frameMgr->stop();
        m_frameMgr->deleteAllFrame();
        m_frameMgr->start();

        m_startPictureBufferThread->join();
        m_setBuffersThread->join();

        m_bufferSupplier->deinit();
    }

    m_parameters[m_cameraId]->setRecordingHint(false);

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

    ret = m_parameters[m_cameraId]->init();
    if (ret != NO_ERROR) {
        CLOGE("initMetadata() failed!! status(%d)", ret);
        return BAD_VALUE;
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraIds[1]] != NULL) {
        ret = m_parameters[m_cameraIds[1]]->init();
        if (ret != NO_ERROR) {
            CLOGE("initMetadata() failed!! status(%d)", ret);
            return BAD_VALUE;
        }
    }
#endif

    m_displayPreviewToggle = 0;
#ifdef SUPPORT_DEPTH_MAP
    m_flagUseInternalDepthMap = false;
#endif

    TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, STREAM_BUFFER_ALLOC_START, 0);

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
        bufConfig.needMmap = (newStream->usage & (GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN | GRALLOC1_PRODUCER_USAGE_CPU_WRITE_OFTEN));

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
                ret = m_bufferSupplier->createBufferManager("RAW_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                ret = m_bufferSupplier->createBufferManager("ZSL_OUT_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                ret =  m_bufferSupplier->createBufferManager("ZSL_INPUT_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                    CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
                ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for PREVIEW stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true) {
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
                            CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                     hwWidth, hwHeight, outputPortId);
                            return ret;
                        }

                        ret = m_parameters[m_cameraIds[1]]->checkYuvFormat(streamPixelFormat, outputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to checkYuvFormat for PREVIEW stream. format %x outputPortId %d",
                                    streamPixelFormat, outputPortId);
                            return ret;
                        }

                        maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_buffers;
                        ret = m_parameters[m_cameraIds[1]]->setYuvBufferCount(maxBufferCount, outputPortId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to setYuvBufferCount for PREVIEW stream. maxBufferCount %d outputPortId %d",
                                    maxBufferCount, outputPortId);
                            return ret;
                        }
                    }

                    if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                        bufTag.pipeId[1] = PIPE_FUSION;
                    }
                }
#endif

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(PREVIEW)");
                ret = m_bufferSupplier->createBufferManager("PREVIEW_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
                    bufConfig.needMmap = true;
                }
#endif
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
                m_parameters[m_cameraId]->setVideoSize(width, height);
                m_parameters[m_cameraId]->setRecordingHint(true);
                CLOGD("setRecordingHint");

                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for VIDEO stream");
                    return ret;
                }

                m_parameters[m_cameraId]->setRecordingPortId(outputPortId);

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
                    CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for VIDEO stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for VIDEO stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true
                    && m_parameters[m_cameraIds[1]] != NULL) {
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
                        CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, outputPortId);
                        return ret;
                    }

                    ret = m_parameters[m_cameraIds[1]]->checkYuvFormat(streamPixelFormat, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvFormat for VIDEO stream. format %x outputPortId %d",
                                streamPixelFormat, outputPortId);
                        return ret;
                    }

                    maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                    ret = m_parameters[m_cameraIds[1]]->setYuvBufferCount(maxBufferCount, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferCount for VIDEO stream. maxBufferCount %d outputPortId %d",
                                maxBufferCount, outputPortId);
                        return ret;
                    }
                }
#endif

                if (m_parameters[m_cameraId]->getGDCEnabledMode() == true) {
                    bufTag.pipeId[0] = PIPE_GDC;
                } else {
                    bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                       + PIPE_MCSC0;
                }
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(VIDEO)");
                ret =  m_bufferSupplier->createBufferManager("VIDEO_STREAM_BUF", m_ionAllocator, bufTag, newStream);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create VIDEO_STREAM_BUF. ret %d", ret);
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
                    CLOGE("Failed to alloc VIDEO_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_PREVIEW_VIDEO:
                m_parameters[m_cameraId]->setVideoSize(width, height);
                m_parameters[m_cameraId]->setRecordingHint(true);
                CLOGD("setRecordingHint");

                ret = privStreamInfo->getOutputPortId(&outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to getOutputPortId for PREVIEW_VIDEO stream");
                    return ret;
                }

                m_parameters[m_cameraId]->setRecordingPortId(outputPortId);

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

                ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for PREVIEW_VIDEO stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_recording_buffers;
                ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, outputPortId);
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

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                bufTag.pipeId[0] = PIPE_JPEG;
                bufTag.pipeId[1] = PIPE_JPEG_REPROCESSING;
                bufTag.pipeId[2] = PIPE_HWFC_JPEG_DST_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(JPEG)");
                ret = m_bufferSupplier->createBufferManager("JPEG_STREAM_BUF", m_ionAllocator, bufTag, newStream);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create JPEG_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                bufConfig.planeCount = streamPlaneCount + 1;
                bufConfig.size[0] = width * height * 2;
                bufConfig.reqBufCount = maxBufferCount;
                bufConfig.allowedMaxBufCount = maxBufferCount;
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
                    CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                             streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_cb_buffers;
                ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true
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
                        CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, outputPortId);
                        return ret;
                    }

                    ret = m_parameters[m_cameraIds[1]]->checkYuvFormat(streamPixelFormat, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                                streamPixelFormat, outputPortId);
                        return ret;
                    }

                    maxBufferCount = m_exynosconfig->current->bufInfo.num_preview_cb_buffers;
                    ret = m_parameters[m_cameraIds[1]]->setYuvBufferCount(maxBufferCount, outputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferCount for CALLBACK stream. maxBufferCount %d outputPortId %d",
                                maxBufferCount, outputPortId);
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
                        CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                                 hwWidth, hwHeight, stallOutputPortId);
                        return ret;
                    }

                    ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to checkYuvFormat for CALLBACK_ZSL stream. format %x outputPortId %d",
                                streamPixelFormat, stallOutputPortId);
                        return ret;
                    }

                    ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, stallOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferCount for CALLBACK_ZSL stream. maxBufferCount %d outputPortId %d",
                                maxBufferCount, stallOutputPortId);
                        return ret;
                    }
                }

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0;
                bufTag.pipeId[1] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(PREVIEW_CB)");
                ret = m_bufferSupplier->createBufferManager("PREVIEW_CB_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                    CLOGE("Failed to checkHwYuvSize for PREVIEW stream. size %dx%d outputPortId %d",
                             hwWidth, hwHeight, outputPortId);
                    return ret;
                }

                ret = m_parameters[m_cameraId]->checkYuvFormat(streamPixelFormat, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to checkYuvFormat for CALLBACK stream. format %x outputPortId %d",
                            streamPixelFormat, outputPortId);
                    return ret;
                }

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                ret = m_parameters[m_cameraId]->setYuvBufferCount(maxBufferCount, outputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferCount for CALLBACK_STALL stream. maxBufferCount %d outputPortId %d",
                             maxBufferCount, outputPortId);
                    return ret;
                }

                bufTag.pipeId[0] = (outputPortId % ExynosCameraParameters::YUV_MAX)
                                   + PIPE_MCSC0_REPROCESSING;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(CAPTURE_CB_STALL)");
                ret = m_bufferSupplier->createBufferManager("CAPTURE_CB_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                    CLOGE("Failed to alloc CAPTURE_CB_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                privStreamInfo->setRegisterBuffer(EXYNOS_STREAM::HAL_STREAM_STS_REGISTERED);
                break;
            case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
                /* Share CALLBACK_STALL port */
                m_parameters[m_cameraId]->setThumbnailCbSize(width, height);

                maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

                bufTag.pipeId[0] = PIPE_GSC_REPROCESSING2;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(THUMBNAIL)");
                ret = m_bufferSupplier->createBufferManager("THUMBNAIL_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                m_parameters[m_cameraId]->setUseDepthMap(true);

                maxBufferCount = m_exynosconfig->current->bufInfo.num_request_raw_buffers;

                bufTag.pipeId[0] = PIPE_VC1;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(DEPTH)");
                ret = m_bufferSupplier->createBufferManager("DEPTH_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                depthInternalBufferCount = m_exynosconfig->current->bufInfo.num_sensor_buffers;
                m_flagUseInternalDepthMap = true;

                m_parameters[m_cameraId]->setDepthMapSize(width, height);
                m_parameters[m_cameraId]->setUseDepthMap(true);

                if (m_parameters[m_cameraId]->getSensorControlDelay() == 0) {
                    depthInternalBufferCount -= SENSOR_REQUEST_DELAY;
                }

                /* Set startIndex as the next internal buffer index */
                startIndex = depthInternalBufferCount;
                maxBufferCount = m_exynosconfig->current->bufInfo.num_bayer_buffers - startIndex;

                bufTag.pipeId[0] = PIPE_VC1;
                bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

                CLOGD("Create buffer manager(DEPTH_STALL)");
                ret = m_bufferSupplier->createBufferManager("DEPTH_STALL_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                    CLOGE("Failed to alloc RAW_STREAM_BUF. ret %d", ret);
                    return ret;
                }

                {
                    /* Depth map Internal buffer */
                    int bayerFormat;
                    const buffer_manager_tag_t initBufTag;
                    const buffer_manager_configuration_t initBufConfig;

                    bufTag = initBufTag;
                    bufTag.pipeId[0] = PIPE_VC1;
                    bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

                    ret = m_bufferSupplier->createBufferManager("DEPTH_MAP_BUF", m_ionAllocator, bufTag);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to create DEPTH_MAP_BUF. ret %d", ret);
                    }

                    bayerFormat = DEPTH_MAP_FORMAT;

                    bufConfig = initBufConfig;
                    bufConfig.planeCount = 2;
                    bufConfig.bytesPerLine[0] = getBayerLineSize(width, bayerFormat);
                    bufConfig.size[0] = getBayerPlaneSize(width, height, bayerFormat);
                    bufConfig.reqBufCount = depthInternalBufferCount;
                    bufConfig.allowedMaxBufCount = depthInternalBufferCount;
                    bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                    bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                    bufConfig.createMetaPlane = true;
                    bufConfig.reservedMemoryCount = 0;
                    bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;

                    ret = m_allocBuffers(bufTag, bufConfig);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to alloc DEPTH_MAP_BUF. ret %d", ret);
                        return ret;
                    }

                    CLOGI("m_allocBuffers(DepthMap Buffer) %d x %d,\
                        planeSize(%d), planeCount(%d), maxBufferCount(%d)",
                        width, height,
                        bufConfig.size[0], bufConfig.planeCount, maxBufferCount);
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
                ret = m_bufferSupplier->createBufferManager("VISION_STREAM_BUF", m_ionAllocator, bufTag, newStream);
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
                    CLOGE("Failed to alloc ZSL_OUT_STREAM_BUF. ret %d", ret);
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

    m_parameters[m_cameraId]->setYuvStallPort(m_streamManager->getOutputPortId(HAL_STREAM_ID_CALLBACK_STALL)
                                              % ExynosCameraParameters::YUV_MAX);
    /*
     * NOTICE: Join is to avoid PIP scanario's problem.
     * The problem is that back camera's EOS was not finished, but front camera opened.
     * Two instance was actually different but driver accepts same instance.
     */
    if (m_framefactoryCreateThread->isRunning() == true) {
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_THREAD_JOIN_START, 0);
        m_framefactoryCreateThread->join();
        TIME_LOGGER_UPDATE(m_cameraId, 0, 0, CUMULATIVE_CNT, FACTORY_CREATE_THREAD_JOIN_END, 0);
    }

    m_parameters[m_cameraId]->updateBinningScaleRatio();

    /* assume that Stall stream and jpeg stream have the same size */
    if (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == false
        && m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true) {
        int yuvStallPort = m_parameters[m_cameraId]->getYuvStallPort() + ExynosCameraParameters::YUV_MAX;

        m_parameters[m_cameraId]->getYuvSize(&width, &height, yuvStallPort);

        ret = m_parameters[m_cameraId]->checkPictureSize(width, height);
        if (ret != NO_ERROR) {
            CLOGE("Failed to checkPictureSize for JPEG stream. size %dx%d",
                     width, height);
            return ret;
        }
    }

    /* Check the validation of stream configuration */
    ret = m_checkStreamInfo();
    if (ret != NO_ERROR) {
        CLOGE("checkStremaInfo() failed!!");
        return ret;
    }

    m_setBuffersThread->run(PRIORITY_DEFAULT);
    if (m_captureStreamExist) {
        m_startPictureBufferThread->run(PRIORITY_DEFAULT);
    }

    /* HACK: Support samsung camera */
#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_streamManager->findStream(HAL_STREAM_ID_VIDEO) == false
        && (m_streamManager->findStream(HAL_STREAM_ID_JPEG) == true
            || m_streamManager->findStream(HAL_STREAM_ID_CALLBACK_STALL) == true)) {
        m_parameters[m_cameraId]->setUseDynamicBayer(false);
    } else
#endif
    {
#ifdef USE_LLS_REPROCESSING
        if (m_parameters[m_cameraId]->getLLSOn() == true)
            m_parameters[m_cameraId]->setUseDynamicBayer(false);
        else
#endif
            m_parameters[m_cameraId]->setUseDynamicBayer(true);
    }

    ret = m_transitState(EXYNOS_CAMERA_STATE_CONFIGURED);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState into CONFIGURED. ret %d", ret);
    }

    CLOGD("Out");
    return ret;
}

status_t ExynosCamera::m_updateTimestamp(ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *timestampBuffer)
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
    if (frame->getAdjustedTimestampFlag() == true) {
        CLOGV("frame %d is already adjust timestamp", frame->getFrameCount());
    } else {
        uint64_t exposureTime = 0;
        exposureTime = (uint64_t)shot_ext->shot.dm.sensor.exposureTime;

        shot_ext->shot.udm.sensor.timeStampBoot -= (exposureTime);
#ifdef CORRECTION_SENSORFUSION
        shot_ext->shot.udm.sensor.timeStampBoot += CORRECTION_SENSORFUSION;
#endif

        frame->setAdjustedTimestampFlag(true);
    }
#endif

    /* HACK: W/A for timeStamp reversion */
    if (shot_ext->shot.udm.sensor.timeStampBoot < m_lastFrametime) {
        CLOGW("Timestamp is lower than lastFrametime!"
            "frameCount(%d) shot_ext.timestamp(%lld) m_lastFrametime(%lld)",
            frame->getFrameCount(), shot_ext->shot.udm.sensor.timeStampBoot, m_lastFrametime);
        //shot_ext->shot.udm.sensor.timeStampBoot = m_lastFrametime + 15000000;
    }
    m_lastFrametime = shot_ext->shot.udm.sensor.timeStampBoot;

    return ret;
}

status_t ExynosCamera::m_storePreviewStatsInfo(struct camera2_shot_ext *src_ext)
{
    status_t ret = NO_ERROR;

    memcpy(&m_stats, &src_ext->shot.dm.stats, sizeof(struct camera2_stats_dm));

    return ret;
}

status_t ExynosCamera::m_getPreviewFDInfo(struct camera2_shot_ext *dst_ext)
{
    status_t ret = NO_ERROR;

    dst_ext->shot.dm.stats.faceDetectMode = m_stats.faceDetectMode;
    for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
        dst_ext->shot.dm.stats.faceIds[i] = m_stats.faceIds[i];
        memcpy(dst_ext->shot.dm.stats.faceLandmarks[i], m_stats.faceLandmarks[i], sizeof(m_stats.faceLandmarks[i]));
        memcpy(dst_ext->shot.dm.stats.faceRectangles[i], m_stats.faceRectangles[i], sizeof(m_stats.faceRectangles[i]));
        dst_ext->shot.dm.stats.faceScores[i] = m_stats.faceScores[i];
        if (i < (CAMERA2_MAX_FACES - 2)) {
            dst_ext->shot.dm.stats.faces[i] = m_stats.faces[i];
        }
    }

    return ret;
}

status_t ExynosCamera::m_previewFrameHandler(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraFrameFactory *targetfactory,
                                             frame_type_t frameType)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    struct camera2_shot_ext *service_shot_ext = NULL;
    const camera3_stream_buffer_t *outputBuffer;
    uint32_t requestKey = 0;
    int32_t bufIndex = -1;
    bool captureFlag = false;
    bool zslInputFlag = false;
    bool rawStreamFlag = false;
    bool zslStreamFlag = false;
    bool depthStreamFlag = false;
    bool depthStallStreamFlag = false;
    bool needDynamicBayer = false;
    bool usePureBayer = false;
    bool requestVC0 = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);
    bool request3AP = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool requestMCSC = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();
    enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
    int pipeId = -1;
    int dstPos = 0;
    uint32_t streamConfigBit = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;
    camera3_stream_buffer_t *inputBuffer = request->getInputBuffer();
    requestKey = request->getKey();

    /* Initialize the request flags in framefactory */
    targetfactory->setRequest(PIPE_VC0, requestVC0);
    targetfactory->setRequest(PIPE_3AA, requestVC0);
    targetfactory->setRequest(PIPE_3AC, false);
    targetfactory->setRequest(PIPE_3AP, request3AP);
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
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        targetfactory->setRequest(PIPE_DCPS0, false);
        targetfactory->setRequest(PIPE_DCPS1, false);
        targetfactory->setRequest(PIPE_DCPC0, false);
        targetfactory->setRequest(PIPE_DCPC1, false);
        targetfactory->setRequest(PIPE_DCPC2, false);
        targetfactory->setRequest(PIPE_DCPC3, false);
        targetfactory->setRequest(PIPE_DCPC4, false);
        targetfactory->setRequest(PIPE_SYNC, false);
        targetfactory->setRequest(PIPE_FUSION, false);

        if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
            if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                switch(frameType) {
                case FRAME_TYPE_PREVIEW_DUAL_MASTER:
                    targetfactory->setRequest(PIPE_SYNC, true);
                    targetfactory->setRequest(PIPE_ISPC, true);
                    targetfactory->setRequest(PIPE_DCPS0, true);
                    targetfactory->setRequest(PIPE_DCPS1, true);
                    targetfactory->setRequest(PIPE_DCPC0, true);
                    targetfactory->setRequest(PIPE_DCPC1, true);
                    targetfactory->setRequest(PIPE_DCPC2, false);
                    targetfactory->setRequest(PIPE_DCPC3, true);
                    targetfactory->setRequest(PIPE_DCPC4, true);
                    requestMCSC = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
                    targetfactory->setRequest(PIPE_MCSC, requestMCSC);
                    break;
                case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
                    targetfactory->setRequest(PIPE_SYNC, true);
                    targetfactory->setRequest(PIPE_ISPC, true);
                    break;
                case FRAME_TYPE_PREVIEW:
                case FRAME_TYPE_PREVIEW_SLAVE:
                    break;
                default:
                    CLOGE("Unsupported frame type(%d)", (int)frameType);
                    break;
                }
            } else if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_MASTER) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M
                    && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
                    requestMCSC = true;
                }

                targetfactory->setRequest(PIPE_ISPC, requestMCSC);
                targetfactory->setRequest(PIPE_MCSC, requestMCSC);
                // ToDo..
            } else if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SLAVE) {
                // ToDo..
            } else {
                // ToDo..
            }
        } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            if (m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                int slaveOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
                slaveOutputPortId = (slaveOutputPortId % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;
                switch(frameType) {
                case FRAME_TYPE_PREVIEW_DUAL_MASTER:
                    targetfactory->setRequest(PIPE_SYNC, true);
                    targetfactory->setRequest(PIPE_FUSION, true);
                    break;
                case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
                    targetfactory->setRequest(slaveOutputPortId, true);
                    break;
                case FRAME_TYPE_PREVIEW:
                case FRAME_TYPE_PREVIEW_SLAVE:
                    break;
                default:
                    CLOGE("Unsupported frame type(%d)", (int)frameType);
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

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        goto GENERATE_FRAME;
    }
#endif

    /* Check ZSL_INPUT stream */
    if(inputBuffer != NULL) {
        int inputStreamId = 0;
        ExynosCameraStream *stream = static_cast<ExynosCameraStream *>(inputBuffer->stream->priv);
        if(stream != NULL) {
            stream->getID(&inputStreamId);
            SET_STREAM_CONFIG_BIT(streamConfigBit, inputStreamId);

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
        } else {
            CLOGE("Stream is null (%d)", request->getKey());
        }
    }

    /* Setting DMA-out request flag based on stream configuration */
    for (size_t i = 0; i < request->getNumOfOutputBuffer(); i++) {
        int id = request->getStreamIdwithBufferIdx(i);
        SET_STREAM_CONFIG_BIT(streamConfigBit, id);

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

            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_CALLBACK)",
                     request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
            break;
        case HAL_STREAM_ID_VIDEO:
            CLOGV("request %d outputBuffer-Index %zu buffer-StreamType(HAL_STREAM_ID_VIDEO)",
                    request->getKey(), i);
            pipeId = (m_streamManager->getOutputPortId(id) % ExynosCameraParameters::YUV_MAX)
                    + PIPE_MCSC0;
            targetfactory->setRequest(pipeId, true);
#ifdef SUPPORT_HW_GDC
            if (m_parameters[m_cameraId]->getGDCEnabledMode() == true) {
                targetfactory->setRequest(PIPE_GDC, true);
                m_previewStreamGDCThread->run(PRIORITY_URGENT_DISPLAY);
            }
#endif
            m_recordingEnabled = true;
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
                if(m_parameters[m_cameraId]->getUsePureBayerReprocessing()) {
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
                if(m_parameters[m_cameraId]->getUsePureBayerReprocessing()) {
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

    if (m_needDynamicBayerCount > 0) {
        m_needDynamicBayerCount--;
        if(m_parameters[m_cameraId]->getUsePureBayerReprocessing()) {
            targetfactory->setRequest(PIPE_VC0, true);
        } else {
            targetfactory->setRequest(PIPE_3AC, true);
        }
    }

GENERATE_FRAME:
    service_shot_ext = request->getServiceShot();
    if (service_shot_ext == NULL) {
        CLOGE("Get service shot fail, requestKey(%d)", request->getKey());
        ret = INVALID_OPERATION;
        goto CLEAN;
    }

    m_updateCropRegion(m_currentPreviewShot);
    m_updateFD(m_currentPreviewShot, service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), false);
    m_parameters[m_cameraId]->setDsInputPortId(m_currentPreviewShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], false);
    m_parameters[m_cameraId]->updateYsumPordId(m_currentPreviewShot);
    m_updateEdgeNoiseMode(m_currentPreviewShot, false);
    m_updateSetfile(m_currentPreviewShot, false);

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == false
        || (frameType != FRAME_TYPE_PREVIEW_SLAVE
            && frameType != FRAME_TYPE_PREVIEW_DUAL_SLAVE))
#endif
    {
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M
            && m_currentPreviewShot->fd_bypass == false) {
            targetfactory->setRequest(PIPE_MCSC5, true);
            targetfactory->setRequest(PIPE_VRA, true);
            if (m_parameters[m_cameraId]->getHfdMode() == true
                && m_currentPreviewShot->hfd.hfd_enable == true) {
                targetfactory->setRequest(PIPE_HFD, true);
            }
        }
    }

    /* Set framecount into request */
    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

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

    /* Set control metadata to frame */
    ret = newFrame->setMetaData(m_currentPreviewShot);
    if (ret != NO_ERROR) {
        CLOGE("[R%d F%d]Set metadata to frame fail. ret %d",
                 request->getKey(), newFrame->getFrameCount(), ret);
    }

    newFrame->setFrameType(frameType);

    newFrame->setStreamRequested(STREAM_TYPE_RAW, rawStreamFlag);
    newFrame->setStreamRequested(STREAM_TYPE_ZSL_OUTPUT, zslStreamFlag);
    newFrame->setStreamRequested(STREAM_TYPE_DEPTH, depthStreamFlag);

    if (captureFlag == true) {
        CLOGI("[F%d]setFrameCapture true.", newFrame->getFrameCount());
        newFrame->setStreamRequested(STREAM_TYPE_CAPTURE, captureFlag);
    }

#ifdef USE_DUAL_CAMERA
    if (frameType == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
    } else
#endif
    {
        /* Set service stream buffers to frame */
        if (m_parameters[m_cameraId]->getBatchSize(controlPipeId) > 1
            && m_parameters[m_cameraId]->useServiceBatchMode() == false) {
            ret = m_setupBatchFactoryBuffers(request, newFrame);
        } else {
            ret = m_setupPreviewFactoryBuffers(request, newFrame);
        }
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d]Failed to setupPreviewStreamBuffer. ret %d",
                    request->getKey(), newFrame->getFrameCount(), ret);
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
    const camera3_stream_buffer_t *outputBuffer;
    uint32_t requestKey = 0;
    int32_t bufIndex = -1;
    int32_t reprocessingBayerMode = m_parameters[m_cameraId]->getReprocessingBayerMode();
    int pipeId = m_getBayerPipeId();
    int dstPos = 0;
    uint32_t streamConfigBit = 0;
    const buffer_manager_tag_t initBufTag;
    buffer_manager_tag_t bufTag;

    int fps = 0;
    int ae = 0;
    int shutterSpeed = 0;
    int gain = 0;
    int irLedWidth = 0;
    int irLedDelay = 0;
    int irLedCurrent = 0;
    int irLedOnTime = 0;
    int internalValue = 0;

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
        goto CLEAN;
    }

    m_updateCropRegion(m_currentPreviewShot);
    m_updateFD(m_currentPreviewShot, service_shot_ext->shot.ctl.stats.faceDetectMode, request->getDsInputPortId(), false);
    m_parameters[m_cameraId]->setDsInputPortId(m_currentPreviewShot->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS], false);
    m_updateEdgeNoiseMode(m_currentPreviewShot, false);

    /* Set framecount into request */
    if (request->getFrameCount() == 0)
        m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

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
        shutterSpeed = (int32_t) (m_parameters[m_cameraId]->getExposureTime() / 100000);
        if (m_shutterSpeed != shutterSpeed) {
            ret = targetfactory->setControl(V4L2_CID_SENSOR_SET_SHUTTER, shutterSpeed, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("shutterSpeed is changed (%d -> %d)", m_shutterSpeed, shutterSpeed);
            m_shutterSpeed = shutterSpeed;
        }

        gain = m_parameters[m_cameraId]->getGain();
        if (m_gain != gain) {
            ret = targetfactory->setControl(V4L2_CID_SENSOR_SET_GAIN, gain, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("gain is changed (%d -> %d)", m_gain, gain);
            m_gain = gain;
        }

        irLedWidth = (int32_t) (m_parameters[m_cameraId]->getLedPulseWidth() / 100000);
        if (m_irLedWidth != irLedWidth) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_WIDTH, irLedWidth, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedWidth is changed (%d -> %d)", m_irLedWidth, irLedWidth);
            m_irLedWidth = irLedWidth;
        }

        irLedDelay = (int32_t) (m_parameters[m_cameraId]->getLedPulseDelay() / 100000);
        if (m_irLedDelay != irLedDelay) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_DELAY, irLedDelay, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedDelay is changed (%d -> %d)", m_irLedDelay, irLedDelay);
            m_irLedDelay = irLedDelay;
        }

        irLedCurrent = m_parameters[m_cameraId]->getLedCurrent();
        if (m_irLedCurrent != irLedCurrent) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_CURRENT, irLedCurrent, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedCurrent is changed (%d -> %d)", m_irLedCurrent, irLedCurrent);
            m_irLedCurrent = irLedCurrent;
        }

        irLedOnTime = (int32_t) (m_parameters[m_cameraId]->getLedMaxTime() / 1000);
        if (m_irLedOnTime != irLedOnTime) {
            ret = targetfactory->setControl(V4L2_CID_IRLED_SET_ONTIME, irLedOnTime, pipeId);
            if (ret < 0)
                CLOGE("FLITE setControl fail, ret(%d)", ret);

            CLOGD("irLedOnTime is changed (%d -> %d)", m_irLedOnTime, irLedOnTime);
            m_irLedOnTime = irLedOnTime;
        }
    }

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

status_t ExynosCamera::m_handleInternalFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    ExynosCameraStream *stream = NULL;
    camera3_capture_result_t captureResult;
    camera3_notify_msg_t notityMsg;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ResultRequest resultRequest = NULL;

    struct camera2_shot_ext *shot_ext = NULL;
    uint32_t framecount = 0;
    int dstPos = 0;

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        CLOGE("current entity is NULL");
        /* TODO: doing exception handling */
        return true;
    }
    CLOGV("handle internal frame(type:%d) : previewStream frameCnt(%d) entityID(%d) getHasRequest(%d)",
            frame->getFrameType(), frame->getFrameCount(), entity->getPipeId(), frame->getHasRequest());

    switch(pipeId) {
    case PIPE_3AA:
        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_OTF) {
#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() == FRAME_TYPE_TRANSITION) {
                if (m_parameters[m_cameraId]->getDualOperationMode() != DUAL_OPERATION_MODE_SYNC) {
                    m_dualShotDoneQ->pushProcessQ(&framecount);
                }
            } else
#endif
            {
                m_shotDoneQ->pushProcessQ(&framecount);
            }
        }

        if (frame->getHasRequest() == true) {
            request = m_requestMgr->getRunningRequest(framecount);
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

        if (request != NULL) {
            ret = m_updateTimestamp(frame, &buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to update timestamp.",
                        frame->getFrameCount(),
                        buffer.index);
                return ret;
            }
            shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
            ret = m_updateResultShot(request, shot_ext, PARTIAL_3AA);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d) B%d]Failed to m_updateResultShot. ret %d",
                        frame->getFrameCount(),
                        shot_ext->shot.dm.request.frameCount,
                        buffer.index, ret);
                return ret;
            }
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
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
            ret = m_handleBayerBuffer(frame, pipeId);
            if (ret < NO_ERROR) {
                CLOGE("Handle bayer buffer failed, framecount(%d), pipeId(%d), ret(%d)",
                         frame->getFrameCount(), entity->getPipeId(), ret);
                return ret;
            }
        }
#ifdef SUPPORT_DEPTH_MAP
        else {
            if (frame->getRequest(PIPE_VC1) == true) {
                int pipeId = PIPE_FLITE;
                ExynosCameraBuffer depthMapBuffer;

                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M)
                    pipeId = PIPE_3AA;

                depthMapBuffer.index = -2;

                ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), depthMapBuffer.index, ret);
                }
            }
        }
#endif

        frame->setMetaDataEnable(true);

        if (request != NULL
#ifdef USE_DUAL_CAMERA
            && frame->getFrameType() != FRAME_TYPE_TRANSITION
#endif
            && request->getNumOfInputBuffer() < 1
            && request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
            m_sendNotifyShutter(request);
            m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
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
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, __FUNCTION__);
        } else {
            /* Notify ShotDone to mainThread */
            framecount = frame->getFrameCount();
#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() == FRAME_TYPE_TRANSITION) {
                if (m_parameters[m_cameraId]->getDualOperationMode() != DUAL_OPERATION_MODE_SYNC) {
                    m_dualShotDoneQ->pushProcessQ(&framecount);
                }
            } else
#endif
            {
                m_shotDoneQ->pushProcessQ(&framecount);
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
#ifdef USE_DUAL_CAMERA
    case PIPE_DCP:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getSrcBuffer. pipeId %d ret %d",
                    entity->getPipeId(), ret);
            return ret;
        }

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for DCP. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }
        break;
#endif
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

status_t ExynosCamera::m_handlePreviewFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    uint32_t framecount = 0;
    int capturePipeId = -1;
    int streamId = -1;
    int dstPos = 0;
    bool isFrameComplete = false;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraRequestSP_sprt_t curRequest = NULL;
    entity_buffer_state_t bufferState;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();

#ifdef USE_DUAL_CAMERA
    if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE
        || frame->getFrameType() == FRAME_TYPE_PREVIEW_SLAVE) {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_PREVIEW_DUAL];
    } else
#endif
    {
        factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    }

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("[F%d]request is NULL.", frame->getFrameCount());
#ifdef USE_DUAL_CAMERA
        /* HACK: If master <-> slave frame count not sync */
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE)
            goto SWITCH;
#endif
        return INVALID_OPERATION;
    }

    CLOGV("[R%d F%d T%d]handle preview frame. entityId %d",
            request->getKey(), frame->getFrameCount(), frame->getFrameType(), pipeId);

#ifdef FPS_CHECK
    m_debugFpsCheck((enum pipeline)pipeId);
#endif

#ifdef USE_DUAL_CAMERA
    /* HACK: If master <-> slave frame count not sync */
SWITCH:
#endif
    switch (pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):[F%d]PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), __FUNCTION__);
        }

        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
        {
            m_shotDoneQ->pushProcessQ(&framecount);
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

#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
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

#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
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

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_OTF) {
#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                m_shotDoneQ->pushProcessQ(&framecount);
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
#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
                }
            }
        }

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        frame->setMetaDataEnable(true);

        ret = m_updateTimestamp(frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to update timestamp. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        for (int batchIndex = 0; batchIndex < buffer.batchSize; batchIndex++) {
            if (batchIndex > 0) {
                if (m_parameters[m_cameraId]->useServiceBatchMode() == true) {
                    break;
                }
                curRequest = m_requestMgr->getRunningRequest(frame->getFrameCount() + batchIndex);
                if (curRequest == NULL) {
                    CLOGE("[F%d]Request is NULL", frame->getFrameCount() + batchIndex);
                    continue;
                }
                shot_ext->shot.udm.sensor.timeStampBoot += shot_ext->shot.dm.sensor.frameDuration;
            } else {
                curRequest = request;
            }

#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            {
                ret = m_updateResultShot(curRequest, shot_ext, PARTIAL_3AA);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d(%d) B%d]Failed to m_updateResultShot. ret %d",
                            frame->getFrameCount(),
                            shot_ext->shot.dm.request.frameCount,
                            buffer.index, ret);
                    return ret;
                }

                if (curRequest->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
                    m_sendNotifyShutter(curRequest);
                    m_sendPartialMeta(curRequest, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
                }
            }
        }

        /* Return dummy-shot buffer */
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            CLOGV("[F%d(%d) B%d]Return 3AS Buffer.",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    buffer.index);

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for 3AS. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

#ifdef SUPPORT_DEPTH_MAP
#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
        {
            if (frame->getRequest(PIPE_VC1) == true && frame->getStreamRequested(STREAM_TYPE_DEPTH)) {
                ret = m_handleDepthBuffer(frame, request);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d]Handle deptg buffer failed. pipeId %d ret %d",
                             frame->getFrameCount(), pipeId, ret);
                }
            }
        }
#endif

        /* Handle bayer buffer */
        if (frame->getRequest(PIPE_VC0) == true || frame->getRequest(PIPE_3AC) == true) {
            ret = m_handleBayerBuffer(frame, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Handle bayer buffer failed. pipeId %d(%s) ret %d",
                         frame->getFrameCount(), pipeId,
                         (frame->getRequest(PIPE_VC0) == true) ? "PIPE_VC0" : "PIPE_3AC",
                         ret);
                return ret;
            }
        }
#ifdef SUPPORT_DEPTH_MAP
        else {
            if (frame->getRequest(PIPE_VC1) == true) {
                int pipeId = PIPE_FLITE;
                ExynosCameraBuffer depthMapBuffer;

                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M)
                    pipeId = PIPE_3AA;

                depthMapBuffer.index = -2;

                ret = frame->getDstBuffer(pipeId, &depthMapBuffer, factory->getNodeType(PIPE_VC1));
                if (ret != NO_ERROR) {
                    CLOGE("Failed to get DepthMap buffer");
                }

                ret = m_bufferSupplier->putBuffer(depthMapBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                            frame->getFrameCount(), depthMapBuffer.index, ret);
                }
            }
        }
#endif

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M) {
            /* Send the bayer buffer to 3AA Pipe */
            dstPos = factory->getNodeType(PIPE_3AP);

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret < 0) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                         pipeId, ret);
            }
            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            ret = m_setupEntity(PIPE_ISP, frame, &buffer, NULL);
            if (ret != NO_ERROR) {
                CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                         PIPE_ISP, ret);
            }

#ifdef USE_DUAL_CAMERA
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
                && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
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

#ifdef USE_DUAL_CAMERA
                if (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
                {
                    ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        return ret;
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

#ifdef SUPPORT_ME
        if (frame->getRequest(PIPE_ME) == true) {
            bool isError = false;

            buffer.index = -1;
            dstPos = factory->getNodeType(PIPE_ME);

            ret = frame->getDstBuffer(pipeId, &buffer, dstPos);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getDstBuffer for PIPE_ME. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                isError = true;
            }

            ret = frame->getDstBufferState(pipeId, &bufferState, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("[F%d P%d]Failed to getDstBufferState for PIPE_ME",
                        frame->getFrameCount(), pipeId);
                isError = true;
            }

            if (buffer.index >= 0 && bufferState == ENTITY_BUFFER_STATE_COMPLETE) {
                /* lock for selectorTag */
                frame->lockSelectorTagList();
                /* In this case only, the me tag can be appendedd to the frame */
                if (frame->getRawStateInSelector() == FRAME_STATE_IN_SELECTOR_PUSHED) {
                    ret = frame->addRawSelectorTag(m_captureSelector[m_cameraId]->getId(), pipeId, dstPos, false);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to addSelectorTag. pipeId %d, dstPos %d, ret %d",
                                frame->getFrameCount(), buffer.index, pipeId, dstPos, ret);
                        isError = true;
                    }
                } else {
                    isError = true;
                }
                frame->unlockSelectorTagList();
            } else {
                CLOGE("[F%d B%d]Invalid buffer. pipeId %d",
                        frame->getFrameCount(), buffer.index, pipeId);
                isError = true;
            }

            /* error case */
            if (isError) {
                ret = frame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d P%d]Failed to setDstBufferState for PIPE_ME",
                            frame->getFrameCount(), pipeId);
                }

                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for ME. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                    }
                }
            }
        }
#endif

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
            m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->pushFrameToPipe(frame, PIPE_SYNC);
            break;
        } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
            if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M) {
                    ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_ISPC));
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                                 pipeId, ret);
                    }

                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

                    ret = m_setupEntity(PIPE_DCPS0, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                 PIPE_DCPS0, ret);
                    }

                    factory->pushFrameToPipe(frame, PIPE_DCP);
                    break;
                }
            }
        } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW) {
            if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M
                    && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
                    ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_ISPC));
                    if (ret < 0) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                                pipeId, ret);
                    }

                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

                    ret = m_setupEntity(PIPE_MCSC, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                PIPE_DCPS0, ret);
                    }

                    factory->pushFrameToPipe(frame, PIPE_MCSC);
                }
            }
        }
#endif
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
            break;
        }

#ifdef USE_DUAL_CAMERA
    case PIPE_DCP:
        if (pipeId == PIPE_DCP) {
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
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), pipeId);

                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
                }
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        } else if (frame->getRequest(PIPE_DCPS0) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPS0));
            if (ret != NO_ERROR) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPC0) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPC0));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index < 0) {
                CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                        buffer.index, frame->getFrameCount(), pipeId);
            }

            if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
                ret = m_setupEntity(PIPE_MCSC, frame, &buffer, NULL);
                if (ret != NO_ERROR) {
                    CLOGE("m_setupEntity failed, pipeId(%d), ret(%d)",
                            PIPE_ISP, ret);
                }
            } else {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPS1) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPS1));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPC1) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPC1));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPC2) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPC2));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPC2. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPC3) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPC3));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getRequest(PIPE_DCPC4) == true) {
            ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_DCPC4));
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)",
                        pipeId, ret);
                return ret;
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
            && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M) {
            factory->pushFrameToPipe(frame, PIPE_MCSC);
            break;
        }
#endif
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
        if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
        } else
#endif
        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            CLOGW("Skipped frame. Force callback result. frameCount %d", frame->getFrameCount());

            ret = m_sendForceYuvStreamResult(request, frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to forceResultCallback. frameCount %d", frame->getFrameCount());
                return ret;
            }
        } else {
            if (m_flagFirstPreviewTimerOn == true) {
                m_firstPreviewTimer.stop();
                m_flagFirstPreviewTimerOn = false;

                CLOGD("m_firstPreviewTimer stop");

                CLOGD("============= First Preview time ==================");
                CLOGD("= startPreview ~ first frame  : %d msec", (int)m_firstPreviewTimer.durationMsecs());
                CLOGD("===================================================");
                autoFocusMgr->displayAFInfo();
            }

            int frameCountPreviewStatsInfo = 0;

            /* PIPE_MCSC 0, 1, 2 */
            for (int i = 0; i < m_streamManager->getTotalYuvStreamCount(); i++) {
                capturePipeId = PIPE_MCSC0 + i;

                CLOGV("[R%d F%d] CapturePipeId(%d) Request(%d)",
                        request->getKey(), frame->getFrameCount(),
                        capturePipeId, frame->getRequest(capturePipeId));

                if (frame->getRequest(capturePipeId) == true) {
                    streamId = m_streamManager->getYuvStreamId(i);
#ifdef USE_DUAL_CAMERA
                    if (m_parameters[m_cameraId]->getDualMode() == true
                        && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                        && frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER
                        && (streamId % HAL_STREAM_ID_MAX) == HAL_STREAM_ID_PREVIEW) {
                    } else
#endif
#ifdef SUPPORT_HW_GDC
                    if (m_parameters[m_cameraId]->getGDCEnabledMode() == true
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

                        request->setStreamBufferStatus(streamId, streamBufferState);

                        if ((m_parameters[m_cameraId]->getYsumRecordingHint() == true)
                                && (m_parameters[m_cameraId]->isRecordingPortId(i) == true)) {
                            struct camera2_udm udm;
                            ret = frame->getUserDynamicMeta(&udm);
                            if (ret < 0) {
                                CLOGE("getUserDynamicMeta fail, pipeId(%d), ret(%d)", pipeId, ret);
                                return ret;
                            }

                            ret = m_parameters[m_cameraId]->updateYsumBuffer(&udm.scaler.ysumdata, &buffer);
                            if (ret != NO_ERROR) {
                                CLOGE("updateYsumBuffer fail, bufferIndex(%d), ret(%d)", buffer.index, ret);
                                return ret;
                            }
                        }

#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
                        if (bufferState != ENTITY_BUFFER_STATE_ERROR && frameCountPreviewStatsInfo == 0) {
                            frameCountPreviewStatsInfo = frame->getFrameCount();
                            shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
                            m_storePreviewStatsInfo(shot_ext);
                        }
#endif

                        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
                            CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                                    buffer.index, frame->getFrameCount(), pipeId);
                        }

                        request->setStreamBufferStatus(streamId, streamBufferState);

                        ret = m_sendYuvStreamResult(request, &buffer, streamId);
                        if (ret != NO_ERROR) {
                            CLOGE("Failed to resultCallback."
                                    " pipeId %d bufferIndex %d frameCount %d streamId %d ret %d",
                                    capturePipeId, buffer.index, frame->getFrameCount(), streamId, ret);
                            return ret;
                        }
                    }
                }
            }	
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
            if (frame->getRequest(PIPE_VRA) == true) {
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

                factory->pushFrameToPipe(frame, PIPE_VRA);
            }
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

            request->setStreamBufferStatus(streamId, streamBufferState);

            ret = m_sendYuvStreamResult(request, &buffer, streamId);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                        ret);
                return ret;
            }
        }

        break;
    case PIPE_VRA:
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

            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

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
        } else if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for VRA. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }
        break;
    case PIPE_HFD:
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

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("[F%d B%d]Src buffer state is error. pipeId %d",
                    frame->getFrameCount(), buffer.index, pipeId);

            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        if (buffer.index >= 0) {
            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for HFD. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                break;
            }
        }
        break;

#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC:
        if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
            if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_MASTER) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_VIRTUAL_OTF) {
                    if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                        ret = m_setVOTFBuffer(PIPE_ISP, frame, PIPE_ISPC, PIPE_DCPS0);
                        if (ret != NO_ERROR) {
                            CLOGE("Set VOTF Buffer fail, pipeId(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                                    PIPE_ISP, PIPE_ISPC, PIPE_DCPS0, ret);
                        }
                    }
                }

                factory->pushFrameToPipe(frame, PIPE_ISP);
            } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                    frame->getDstBuffer(PIPE_ISP, &buffer, factory->getNodeType(PIPE_ISPC));
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
            }
        } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            factory->pushFrameToPipe(frame, PIPE_FUSION);
        }
        break;

    case PIPE_FUSION:
        if (pipeId == PIPE_FUSION) {
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

            CLOGV("Return FUSION Buffer. driver->framecount %d hal->framecount %d",
                    getMetaDmRequestFrameCount((struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()]),
                    frame->getFrameCount());
        }

        if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
            streamBufferState = CAMERA3_BUFFER_STATUS_ERROR;
        }

        if (frame->getRequest(pipeId) == true) {
            streamId = HAL_STREAM_ID_PREVIEW;

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

            request->setStreamBufferStatus(streamId, streamBufferState);

            ret = m_sendYuvStreamResult(request, &buffer, streamId);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d B%d S%d]Failed to sendYuvStreamResult. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId,
                        ret);
                return ret;
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

    if (isFrameComplete == true) {
        if (((frame->getUpdateResult() == true) || (request != NULL && request->getSkipCaptureResult() == true))
#ifdef USE_DUAL_CAMERA
            && (frame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_SLAVE)
#endif
            ) {
            struct camera2_shot_ext resultShot;

            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
            }

            for (int batchIndex = 0; batchIndex < m_parameters[m_cameraId]->getBatchSize((enum pipeline)pipeId); batchIndex++) {
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

#ifdef USE_DUAL_CAMERA
                if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                } else
#endif
                {
                    ret = m_updateResultShot(curRequest, &resultShot);
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
            }
        }

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

status_t ExynosCamera::m_handleVisionFrame(ExynosCameraFrameSP_sptr_t frame, int pipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    struct camera2_shot_ext *shot_ext = NULL;
    uint32_t framecount = 0;
    int capturePipeId = -1;
    int streamId = HAL_STREAM_ID_VISION;
    int dstPos = 0;
    ExynosCameraRequestSP_sprt_t request = NULL;
    entity_buffer_state_t bufferState;
    camera3_buffer_status_t streamBufferState = CAMERA3_BUFFER_STATUS_OK;
    ExynosCameraActivityAutofocus *autoFocusMgr = m_activityControl->getAutoFocusMgr();

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("[F%d]request is NULL.", frame->getFrameCount());
        return INVALID_OPERATION;
    }

    entity = frame->getFrameDoneFirstEntity(pipeId);
    if (entity == NULL) {
        CLOGE("[R%d F%d]current entity is NULL. pipeId %d",
                request->getKey(), frame->getFrameCount(), pipeId);
        return INVALID_OPERATION;
    }

    CLOGV("[R%d F%d]handle vision frame. entityId %d",
             request->getKey(), frame->getFrameCount(), entity->getPipeId());

    switch(pipeId) {
    case PIPE_FLITE:
        /* 1. Handle bayer buffer */
        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):[F%d]PIPE_FLITE cannot come to %s when Flite3aaOtf. so, assert!!!!",
                    __FUNCTION__, __LINE__, frame->getFrameCount(), __FUNCTION__);
        }

        /* Notify ShotDone to mainThread */
        framecount = frame->getFrameCount();
        m_shotDoneQ->pushProcessQ(&framecount);

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

            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to sendNotifyError. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
                return ret;
            }
        }

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for FLITE. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
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

        request->setStreamBufferStatus(streamId, streamBufferState);

        shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
        frame->setMetaDataEnable(true);

        ret = m_updateTimestamp(frame, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to update timestamp. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            return ret;
        }

        ret = m_updateResultShot(request, shot_ext, PARTIAL_3AA);
        if (ret != NO_ERROR) {
            CLOGE("[F%d(%d) B%d]Failed to m_updateResultShot. ret %d",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    buffer.index, ret);
            return ret;
        }

        if (request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
            m_sendNotifyShutter(request);
            m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
        }

        ret = m_sendVisionStreamResult(request, &buffer);
        if (ret != NO_ERROR) {
            CLOGE("Failed to resultCallback."
                    " PIPE_VC0 bufferIndex %d frameCount %d streamId %d ret %d",
                    buffer.index, frame->getFrameCount(), streamId, ret);
            return ret;
        }

        break;
    }

    ret = frame->setEntityState(entity->getPipeId(), ENTITY_STATE_COMPLETE);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):setEntityState fail, pipeId(%d), state(%d), ret(%d)",
                __FUNCTION__, __LINE__, entity->getPipeId(), ENTITY_STATE_COMPLETE, ret);
        return ret;
    }

    if (frame->isComplete() == true) {
        if ((frame->getUpdateResult() == true)
            || (request != NULL && request->getSkipCaptureResult() == true)) {
            struct camera2_shot_ext resultShot;

            ret = frame->getMetaData(&resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getMetaData. ret %d", frame->getFrameCount(), ret);
            }

            ret = m_updateResultShot(request, &resultShot);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d)]Failed to m_updateResultShot. ret %d",
                        frame->getFrameCount(), resultShot.shot.dm.request.frameCount, ret);
                return ret;
            }

            ret = m_sendMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_ALL_RESULT);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to sendMeta. ret %d",
                        frame->getFrameCount(), ret);
            }
        }

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
    resultMeta = request->get3AAResultMeta();
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

status_t ExynosCamera::m_sendYuvStreamResult(ExynosCameraRequestSP_sprt_t request,
                                             ExynosCameraBuffer *buffer, int streamId)
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
    bool skipPreview = false;
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

    m_parameters[m_cameraId]->getPreviewFpsRange(&minFps, &maxFps);

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

        if ((maxFps > dispFps)
            && (request->hasStream(HAL_STREAM_ID_VIDEO) == false)
            && (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_PREVIEW)) {
            ratio = (int)((maxFps * 10 / dispFps) / buffer->batchSize / 10);
            m_displayPreviewToggle = (m_displayPreviewToggle + 1) % ratio;
            skipPreview = (m_displayPreviewToggle == 0) ? false : true;
        }

        ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status,
                OISCapture_activated | skipPreview);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d B%d]Failed to checkStreamBufferStatus.",
                    request->getKey(), request->getFrameCount(), streamId, buffer->index);
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

        CLOGV("frame number(%d), #out(%d)",
                requestResult->frame_number, requestResult->num_output_buffers);

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


status_t ExynosCamera::m_setupVisionPipeline(void)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    CLOGI("++IN++");

    int ret = 0;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_VISION];
    ExynosCameraFrame *newFrame = NULL;
    ExynosCameraBuffer dstBuf;
    uint32_t pipeId = PIPE_FLITE;
    uint32_t nodePipeId = PIPE_VC0;
    enum NODE_TYPE nodeType;

    factory->setRequest(PIPE_VC0, true);

    m_setSetfile();

    nodeType = factory->getNodeType(nodePipeId);

    /* set initial value for Secure Camera*/
    m_shutterSpeed = (int32_t) (m_parameters[m_cameraId]->getExposureTime() / 100000);
    m_gain = m_parameters[m_cameraId]->getGain();
    m_irLedWidth = (int32_t) (m_parameters[m_cameraId]->getLedPulseWidth() / 100000);
    m_irLedDelay = (int32_t) (m_parameters[m_cameraId]->getLedPulseDelay() / 100000);
    m_irLedCurrent = m_parameters[m_cameraId]->getLedCurrent();
    m_irLedOnTime = (int32_t) (m_parameters[m_cameraId]->getLedMaxTime() / 1000);
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
    ExynosCameraStream *stream = NULL;
    int flipHorizontal = 0;
    enum NODE_TYPE nodeType;

    uint32_t cameraId = factory->getCameraId();
    bool flag3aaIspM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagMcscVraM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

    flipHorizontal = m_parameters[cameraId]->getFlipHorizontal();

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

#ifdef USE_DUAL_CAMERA
    if (cameraId == m_cameraId
        && m_parameters[cameraId]->getDualMode() == true
        && m_parameters[cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
        if (flagIspDcpM2M == true) {
            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* TODO : Consider the M2M Reprocessing Scenario */
            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

            pipeId = PIPE_DCP_REPROCESSING;
        }

        if (flagDcpMcscM2M == true) {
            ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
            if (ret != NO_ERROR) {
                CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
                return ret;
            }

            /* TODO : Consider the M2M Reprocessing Scenario */
            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);

            pipeId = PIPE_MCSC_REPROCESSING;
        }

        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, PIPE_SYNC_REPROCESSING);
    } else
#endif
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

        nodeType = factory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING);
        factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);

        nodeType = factory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING);
        factory->setControl(V4L2_CID_HFLIP, flipHorizontal, pipeId, nodeType);
    }

    if (flagMcscVraM2M == true) {
        pipeId = PIPE_VRA_REPROCESSING;

        ret = factory->setBufferSupplierToPipe(m_bufferSupplier, pipeId);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setBufferSupplierToPipe into pipeId %d", pipeId);
            return ret;
        }

        /* TODO : Consider the M2M Reprocessing Scenario */
        factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId);
    }

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

status_t ExynosCamera::m_selectBayerHandler(uint32_t pipeID, ExynosCameraFrameSP_sptr_t frame, ExynosCameraBuffer *bayerBuffer,ExynosCameraFrameSP_sptr_t bayerFrame, ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;
    buffer_manager_tag_t bufTag;
    ExynosCameraBuffer meBuffer;
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
            int meSrcPipeId = PIPE_ISP;
            int dstPos = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW]->getNodeType(PIPE_ME);

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
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameSP_sptr_t bayerFrame = NULL;
    ExynosCameraBuffer bayerBuffer;
    camera2_shot_ext *shot_ext = NULL;
    enum FRAME_TYPE frameType;
    uint32_t pipeID = 0;
    uint32_t dstPipeId = 0;
    ExynosCameraRequestSP_sprt_t request = NULL;
    int bufIndex = -1;
    ExynosCameraDurationTimer timer;
    ExynosCameraFrameFactory *factory = NULL;
#ifdef SUPPORT_DEPTH_MAP
    ExynosCameraBuffer depthMapBuffer;
    depthMapBuffer.index = -2;
#endif
    int yuvStallPipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;
    bayerBuffer.index = -2;

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
    frameType = (enum FRAME_TYPE) frame->getFrameType();

BAYER_RETRY:

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

    if (frameType != FRAME_TYPE_INTERNAL) {
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

    switch (m_parameters[m_cameraId]->getReprocessingBayerMode()) {
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
#ifdef USE_DUAL_CAMERA
        if (frameType == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            ret = m_getBayerBuffer(m_getBayerPipeId(),
                    frame->getFrameCount(),
                    &bayerBuffer,
                    bayerFrame,
                    m_captureSelector[m_cameraIds[1]]);
        } else
#endif
        {
            ret = m_getBayerBuffer(m_getBayerPipeId(),
                    frame->getFrameCount(),
                    &bayerBuffer,
                    bayerFrame,
                    m_captureSelector[m_cameraId]
#ifdef SUPPORT_DEPTH_MAP
                    , &depthMapBuffer
#endif
                    );
        }
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

#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
    m_getPreviewFDInfo(shot_ext);
#endif

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

    if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
        if (request->getNumOfInputBuffer() > 0
            && request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_NOTIFY_ONLY) == false) {
            ret = m_updateTimestamp(frame, &bayerBuffer);
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
        uint32_t index = -1;

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
                    ion_sync_fd(m_ionClient, serviceBuffer.fd[0]);
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
        m_bufferSupplier->putBuffer(depthMapBuffer);
    }
#endif

#ifdef DEBUG_RAWDUMP
    if (m_parameters[m_cameraId]->checkBayerDumpEnable()) {
        bool bRet;
        char filePath[70];

        memset(filePath, 0, sizeof(filePath));
        snprintf(filePath, sizeof(filePath), "/data/media/0/RawCapture%d_%d.raw", m_cameraId, frame->getFrameCount());

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

#ifdef USE_DUAL_CAMERA
    if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
        dstPipeId = PIPE_ISPC_REPROCESSING;
    } else
#endif
    if (pipeID == PIPE_3AA_REPROCESSING
        && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId = PIPE_3AP_REPROCESSING;
    } else if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId == PIPE_ISPC_REPROCESSING;
#ifdef USE_DUAL_CAMERA
    } else if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId == PIPE_ISPC_REPROCESSING;
    } else if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
        dstPipeId == PIPE_DCPC0_REPROCESSING;
#endif
    } else {
        if (m_parameters[m_cameraId]->isUseHWFC() == true) {
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

    if (m_parameters[m_cameraId]->isUseHWFC() == false
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
    if (frame != NULL && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        ret = m_removeFrameFromList(&m_captureProcessList, &m_captureProcessLock, frame);
        if (ret != NO_ERROR)
            CLOGE("Failed to remove frame from m_captureProcessList. frameCount %d ret %d",
                     frame->getFrameCount(), ret);

        frame->printEntity();
        CLOGD("[F%d]Delete frame from m_captureProcessList.", frame->getFrameCount());
        frame = NULL;
    }

    if (m_selectBayerQ->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
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
    struct camera2_shot_ext *shot_ext = NULL;
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
    } else {
#ifdef USE_LLS_REPROCESSING
        /* HACK: LLS Capture can be supported to jpeg */
        if (m_parameters[m_cameraId]->getLLSCaptureOn() == true)
            factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
#endif
    }

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("Current entity is NULL");
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    pipeId = entity->getPipeId();

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
                    ion_sync_fd(m_ionClient, buffer.fd[i]);
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

        if (frame->getFrameType() != FRAME_TYPE_INTERNAL &&
                frame->getUpdateResult() == true) {
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
    int bufIndex = -1;
    ExynosCameraFrameFactory *factory = NULL;
    int pipeId_src = -1;
    int pipeId_gsc = -1;
    int pipeId_jpeg = -1;
    ExynosCameraRequestSP_sprt_t request = NULL;

    bool isSrc = false;
    float zoomRatio = 0.0F;
    struct camera2_stream *shot_stream = NULL;
    int pictureW = 0, pictureH = 0, pictureFormat = 0;
    ExynosRect srcRect, dstRect;

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());

    if (frame->getFrameType() == FRAME_TYPE_JPEG_REPROCESSING)
        factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
    else
        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

    if (m_parameters[m_cameraId]->isReprocessing() == true) {
#ifdef USE_DUAL_CAMERA
        if (m_parameters[m_cameraId]->getDualMode() == true
            && m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
            if (flagDcpMcscM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                pipeId_src = PIPE_MCSC_REPROCESSING;
            } else if (flagIspDcpM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
                pipeId_src = PIPE_DCP_REPROCESSING;
            } else if (flag3aaIspM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                pipeId_src = PIPE_ISP_REPROCESSING;
            } else {
                pipeId_src = PIPE_3AA_REPROCESSING;
            }
        } else
#endif
        if (flagIspMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
            pipeId_src = PIPE_MCSC_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
            pipeId_src = PIPE_ISP_REPROCESSING;
        } else {
            pipeId_src = PIPE_3AA_REPROCESSING;
        }
    }

    if (m_parameters[m_cameraId]->isUseHWFC() == true) {
        ret = frame->getDstBuffer(pipeId_src, &dstBuffer, factory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING));
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                    pipeId_src, PIPE_MCSC_JPEG_REPROCESSING, ret);
            return INVALID_OPERATION;
        }

        ret = m_bufferSupplier->putBuffer(dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to putBuffer for MCSC3_REP. ret %d",
                    frame->getFrameCount(), dstBuffer.index, ret);
            return INVALID_OPERATION;
        }

        if (frame->getRequest(PIPE_MCSC_THUMB_REPROCESSING) == true) {
            ret = frame->getDstBuffer(pipeId_src, &dstBuffer, factory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to getDstBuffer. pipeId %d node %d ret %d",
                        request->getKey(), frame->getFrameCount(),
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING, ret);
                return INVALID_OPERATION;
            }

            ret = m_bufferSupplier->putBuffer(dstBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for MCSC4_REP. ret %d",
                        frame->getFrameCount(), dstBuffer.index, ret);
                return INVALID_OPERATION;
            }
        }
    } else {
        zoomRatio = m_parameters[m_cameraId]->getZoomRatio();

        if (frame->getRequest(PIPE_MCSC_THUMB_REPROCESSING) == true) {
            // Thumbnail image is currently not used
            ret = frame->getDstBuffer(pipeId_src, &dstBuffer, factory->getNodeType(PIPE_MCSC_THUMB_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("Failed to getDstBuffer. pipeId %d node %d ret %d",
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING, ret);
            } else {
                ret = m_bufferSupplier->putBuffer(dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for MCSC4_REP. ret %d",
                            frame->getFrameCount(), dstBuffer.index, ret);
                }
                CLOGI("[F%d B%d]Thumbnail image disposed at pipeId %d node %d",
                        frame->getFrameCount(), dstBuffer.index,
                        pipeId_src, PIPE_MCSC_THUMB_REPROCESSING);
            }
        }

        if (m_parameters[m_cameraId]->needGSCForCapture(getCameraId()) == true) {
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
            m_parameters[m_cameraId]->getPictureSize(&pictureW, &pictureH);
            pictureFormat = m_parameters[m_cameraId]->getHwPictureFormat();

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

            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_gsc);
            factory->pushFrameToPipe(frame, pipeId_gsc);

        } else { /* m_parameters[m_cameraId]->needGSCForCapture(getCameraId()) == false */
            ret = frame->getDstBuffer(pipeId_src, &srcBuffer, factory->getNodeType(PIPE_MCSC_JPEG_REPROCESSING));
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to getDstBuffer. pipeId %d ret %d",
                        frame->getFrameCount(), pipeId_src, ret);
                return ret;
            }

            ret = frame->setSrcBuffer(pipeId_jpeg, srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("[R%d F%d]Failed to setSrcBuffer. pipeId %d ret %d",
                         request->getKey(), frame->getFrameCount(), pipeId_jpeg, ret);
            }

            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_jpeg);
            factory->pushFrameToPipe(frame, pipeId_jpeg);
        }
    }

    return ret;
}

status_t ExynosCamera::m_handleJpegFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId)
{
    status_t ret = NO_ERROR;
    int pipeId_jpeg = -1;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraFrameFactory * factory = NULL;
    ExynosCameraBuffer buffer;
    int jpegOutputSize = -1;
    exif_attribute_t exifInfo;
    ExynosRect pictureRect;
    ExynosRect thumbnailRect;
    struct camera2_shot_ext *jpeg_meta_shot;
    camera3_buffer_status_t streamBufferStae = CAMERA3_BUFFER_STATUS_OK;

    m_parameters[m_cameraId]->getFixedExifInfo(&exifInfo);
    pictureRect.colorFormat = m_parameters[m_cameraId]->getHwPictureFormat();
    m_parameters[m_cameraId]->getPictureSize(&pictureRect.w, &pictureRect.h);
    m_parameters[m_cameraId]->getThumbnailSize(&thumbnailRect.w, &thumbnailRect.h);

    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("[F%d] request is NULL.", frame->getFrameCount());
        ret = INVALID_OPERATION;
        return ret;
    }
    factory = request->getFrameFactory(HAL_STREAM_ID_JPEG);

    /* We are using only PIPE_ISP_REPROCESSING */
    if (m_parameters[m_cameraId]->isReprocessing() == true) {
        pipeId_jpeg = PIPE_JPEG_REPROCESSING;
    } else {
        pipeId_jpeg = PIPE_JPEG;
    }

    if (m_parameters[m_cameraId]->isUseHWFC() == true && leaderPipeId != PIPE_JPEG_REPROCESSING) {
        entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = frame->getDstBufferState(leaderPipeId, &bufferState, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
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

        ret = frame->getDstBuffer(leaderPipeId, &buffer, factory->getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING));
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
        m_parameters[m_cameraId]->setExifChangedAttribute(&exifInfo, &pictureRect, &thumbnailRect, &jpeg_meta_shot->shot, true);

        /*In case of HWFC, overwrite buffer's size to compression's size in ExynosCameraNodeJpeg file.*/
        jpegOutputSize = buffer.size[0];

        /* in case OTF until JPEG, we should overwrite debugData info to Jpeg data. */
        if (buffer.size[0] != 0) {
            /* APP1 Marker - EXIF */
            UpdateExif(buffer.addr[0], buffer.size[0], &exifInfo);
            /* APP4 Marker - DebugInfo */
            UpdateDebugData(buffer.addr[0], buffer.size[0], m_parameters[m_cameraId]->getDebug2Attribute());
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

    /* frame->printEntity(); */
    m_sendJpegStreamResult(request, &buffer, jpegOutputSize);

    return ret;
}

bool ExynosCamera::m_captureStreamThreadFunc(void)
{
    status_t ret = 0;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    ExynosCameraBuffer buffer;
    ExynosCameraBuffer serviceBuffer;
    ExynosCameraFrameFactory *factory;
    ExynosCameraRequestSP_sprt_t request = NULL;
    struct camera2_shot_ext *shot_ext = NULL;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;
    int pipeId = -1;
    int dstPipeId = -1;
    int dstPos = -1;
    int yuvStallPipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;

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

    if (frame->getFrameType() == FRAME_TYPE_JPEG_REPROCESSING)
        factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
    else
        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

    entity = frame->getFrameDoneEntity();
    if (entity == NULL) {
        CLOGE("Current entity is NULL");
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    pipeId = entity->getPipeId();

    CLOGD("[F%d T%d]YUV capture done. entityID %d",
            frame->getFrameCount(), frame->getFrameType(), entity->getPipeId());

    switch(entity->getPipeId()) {
    case PIPE_3AA_REPROCESSING:
    case PIPE_ISP_REPROCESSING:
        ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                    frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)",
                    __FUNCTION__, __LINE__, entity->getPipeId(), ret);
            goto FUNC_EXIT;
        }

        if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("ERR(%s[%d]):Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                    __FUNCTION__, __LINE__,
                    buffer.index, frame->getFrameCount(), entity->getPipeId());
            request = m_requestMgr->getRunningRequest(frame->getFrameCount());
            ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):[F%d B%d] sendNotifyError fail. ret %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(),
                        buffer.index, ret);
                goto FUNC_EXIT;
            }
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            factory->pushFrameToPipe(frame, PIPE_SYNC_REPROCESSING);
        } else
#endif
        {
            shot_ext = (struct camera2_shot_ext *) buffer.addr[buffer.getMetaPlaneIndex()];
            if (shot_ext == NULL) {
                CLOGE("[F%d]shot_ext is NULL. pipeId %d",
                        frame->getFrameCount(), entity->getPipeId());
                goto FUNC_EXIT;
            }

            if (frame->getFrameType() != FRAME_TYPE_INTERNAL) {
                request = m_requestMgr->getRunningRequest(frame->getFrameCount());

                if (request != NULL && request->getNumOfInputBuffer() > 0
                        && request->getCallbackDone(EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA) == false) {
                    ret = m_updateResultShot(request, shot_ext, PARTIAL_3AA);
                    if (ret != NO_ERROR) {
                        CLOGE("[R%d F%d(%d) B%d]Failed to pushResult. ret %d",
                                request->getKey(),
                                frame->getFrameCount(),
                                shot_ext->shot.dm.request.frameCount,
                                buffer.index,
                                ret);
                        goto FUNC_EXIT;
                    }

                    m_sendPartialMeta(request, EXYNOS_REQUEST_RESULT::CALLBACK_PARTIAL_3AA);
                }
            }

            ret = frame->storeDynamicMeta(shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("[F%d(%d) B%d]Failed to storeUserDynamicMeta. ret %d",
                        request->getKey(),
                        shot_ext->shot.dm.request.frameCount,
                        buffer.index,
                        ret);
                goto FUNC_EXIT;
            }

            ret = frame->storeUserDynamicMeta(shot_ext);
            if (ret != NO_ERROR) {
                CLOGE("[F%d]Failed to storeUserDynamicMeta. ret %d",
                        request->getKey(), ret);
                goto FUNC_EXIT;
            }

            if (entity->getPipeId() == PIPE_3AA_REPROCESSING
                    && frame->getRequest(PIPE_3AC_REPROCESSING) == true) {
                dstPos = factory->getNodeType(PIPE_3AC_REPROCESSING);

                ret = frame->getDstBuffer(entity->getPipeId(), &serviceBuffer, dstPos);
                if (ret != NO_ERROR || buffer.index < 0) {
                    CLOGE("[F%d B%d]Failed to getDstBuffer. pos %d. ret %d",
                            frame->getFrameCount(), serviceBuffer.index, dstPos, ret);
                    return ret;
                }

                ret = frame->getDstBufferState(entity->getPipeId(), &bufferState, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to getDstBufferState. pos %d. ret %d",
                            frame->getFrameCount(), buffer.index, dstPos, ret);
                    return ret;
                }

                if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                    request->setStreamBufferStatus(HAL_STREAM_ID_RAW, CAMERA3_BUFFER_STATUS_ERROR);
                    CLOGE("ERR(%s[%d]):Invalid RAW buffer state. frameCount %d bufferState %d",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(), bufferState);
                }

                request = m_requestMgr->getRunningRequest(frame->getFrameCount());

                if (request != NULL) {
                    ret = m_sendRawStreamResult(request, &serviceBuffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to sendRawStreamResult. ret %d",
                                frame->getFrameCount(), serviceBuffer.index, ret);
                    }
                }
            }

            CLOGV("[F%d(%d) B%d]3AA_REPROCESSING Done.",
                    frame->getFrameCount(),
                    shot_ext->shot.dm.request.frameCount,
                    buffer.index);
        }

        if (!frame->getStreamRequested(STREAM_TYPE_ZSL_INPUT)) {
            m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }

#ifdef USE_DUAL_CAMERA
        if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE) {
            break;
        } else if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
                    ret = frame->getDstBuffer(pipeId, &buffer, factory->getNodeType(PIPE_ISPC_REPROCESSING));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                                pipeId, ret);
                    }

                    if (buffer.index < 0) {
                        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                                buffer.index, frame->getFrameCount(), pipeId);
                    }

                    ret = m_setupEntity(PIPE_DCPS0_REPROCESSING, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)", PIPE_DCPS0_REPROCESSING, ret);
                    }

                    factory->pushFrameToPipe(frame, PIPE_DCP_REPROCESSING);
                    break;
                }
            }
        }
#endif

        if (entity->getPipeId() == PIPE_3AA_REPROCESSING
            && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            break;
        } else if (entity->getPipeId() == PIPE_ISP_REPROCESSING
                && m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            dstPipeId = PIPE_ISPC_REPROCESSING;
            if (frame->getRequest(dstPipeId) == true) {
                dstPos = factory->getNodeType(dstPipeId);
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

                if (m_parameters[m_cameraId]->isUseHWFC() == true) {
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

            factory->pushFrameToPipe(frame, PIPE_MCSC_REPROCESSING);
            break;
        }
#ifdef USE_DUAL_CAMERA
    case PIPE_DCP_REPROCESSING:
        if (entity->getPipeId() == PIPE_DCP_REPROCESSING) {
            ret = frame->getSrcBuffer(entity->getPipeId(), &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[F%d B%d]Failed to getSrcBuffer. pipeId %d, ret %d",
                        frame->getFrameCount(), buffer.index, entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            ret = frame->getSrcBufferState(entity->getPipeId(), &bufferState);
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("ERR(%s[%d]):Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                request = m_requestMgr->getRunningRequest(frame->getFrameCount());
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):[F%d B%d] sendNotifyError fail. ret %d",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(),
                            buffer.index, ret);
                    goto FUNC_EXIT;
                }
            }

            if (buffer.index >= 0) {
                m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPS0. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                }
            }
        } else {
            dstPipeId = PIPE_DCPS0_REPROCESSING;
            if (frame->getRequest(dstPipeId) == true) {
                dstPos = factory->getNodeType(dstPipeId);
                ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
                if (ret != NO_ERROR) {
                    CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                            entity->getPipeId(), ret);
                }

                if (buffer.index >= 0) {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for DCPS0. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        break;
                    }
                }
            }
        }

        /* Send the Yuv buffer to MCSC Pipe */
        dstPipeId = PIPE_DCPC0_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
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
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
                    ret = m_setupEntity(PIPE_MCSC_REPROCESSING, frame, &buffer, NULL);
                    if (ret != NO_ERROR) {
                        CLOGE("setSrcBuffer failed, pipeId(%d), ret(%d)",
                                PIPE_MCSC_REPROCESSING, ret);
                    }
                } else {
                    ret = m_bufferSupplier->putBuffer(buffer);
                    if (ret != NO_ERROR) {
                        CLOGE("[F%d B%d]Failed to putBuffer for DCPS1. ret %d",
                                frame->getFrameCount(), buffer.index, ret);
                        break;
                    }
                }
            }
        }

        dstPipeId = PIPE_DCPS1_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPS1. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        dstPipeId = PIPE_DCPC1_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPC1. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        dstPipeId = PIPE_DCPC2_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPC2. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        dstPipeId = PIPE_DCPC3_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPC3. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        dstPipeId = PIPE_DCPC4_REPROCESSING;
        if (frame->getRequest(dstPipeId) == true) {
            dstPos = factory->getNodeType(dstPipeId);
            ret = frame->getDstBuffer(entity->getPipeId(), &buffer, dstPos);
            if (ret != NO_ERROR) {
                CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                        entity->getPipeId(), ret);
            }

            if (buffer.index >= 0) {
                ret = m_bufferSupplier->putBuffer(buffer);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d]Failed to putBuffer for DCPC4. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    break;
                }
            }
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            if (m_parameters[m_cameraId]->isUseHWFC() == true) {
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

            factory->pushFrameToPipe(frame, PIPE_MCSC_REPROCESSING);
            break;
        }
#endif
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
                CLOGE("ERR(%s[%d]):getSrcBufferState fail, pipeId(%d), ret(%d)",
                        __FUNCTION__, __LINE__, entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("ERR(%s[%d]):Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                        __FUNCTION__, __LINE__,
                        buffer.index, frame->getFrameCount(), entity->getPipeId());
                request = m_requestMgr->getRunningRequest(frame->getFrameCount());
                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("ERR(%s[%d]):[F%d B%d] sendNotifyError fail. ret %d",
                            __FUNCTION__, __LINE__,
                            frame->getFrameCount(),
                            buffer.index, ret);
                    goto FUNC_EXIT;
                }
            }

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for bayerBuffer. ret %d",
                        frame->getFrameCount(), buffer.index, ret);
            }
        }

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

            if (frame->getStreamRequested(STREAM_TYPE_THUMBNAIL_CB)) {
                m_thumbnailCbThread->join();
            }

            ExynosCameraBuffer srcBuffer;
            ret = frame->getDstBuffer(entity->getPipeId(), &srcBuffer, factory->getNodeType(yuvStallPipeId));
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]getDstBuffer fail. pipeId (%d) ret(%d)",
                        frame->getFrameCount(), srcBuffer.index, yuvStallPipeId, ret);
                goto FUNC_EXIT;
            }

            ret = m_bufferSupplier->putBuffer(srcBuffer);
            if (ret < 0) {
                CLOGE("[F%d B%d]Failed to putBuffer. ret %d",
                        frame->getFrameCount(), srcBuffer.index, ret);
            }
        } else {
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

        /* Handle yuv capture buffer */
        if (frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
            ret = m_handleYuvCaptureFrame(frame);
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleYuvCaptureFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }
        }

        if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING) == HW_CONNECTION_MODE_M2M) {
            if (frame->getRequest(PIPE_VRA_REPROCESSING) == true) {
                /* Send the Yuv buffer to VRA Pipe */
                dstPos = factory->getNodeType(PIPE_MCSC5_REPROCESSING);

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

                factory->pushFrameToPipe(frame, PIPE_VRA_REPROCESSING);
            }
        }

        /* Continue to JPEG processing stage in HWFC mode */
        if (m_parameters[m_cameraId]->isUseHWFC() == false
            || frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == false) {
            break;
        }
    case PIPE_GSC_REPROCESSING2:
    case PIPE_JPEG_REPROCESSING:
        /* Handle JPEG buffer */
        if (entity->getPipeId() == PIPE_JPEG_REPROCESSING
            || frame->getRequest(PIPE_MCSC_JPEG_REPROCESSING) == true) {
            ret = m_handleJpegFrame(frame, entity->getPipeId());
            if (ret != NO_ERROR) {
                CLOGE("Failed to handleJpegFrame. pipeId %d ret %d",
                         entity->getPipeId(), ret);
                goto FUNC_EXIT;
            }
        }
        break;
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

            if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                CLOGE("Src buffer state is error index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, frame->getFrameCount(), entity->getPipeId());

                ret = m_sendNotifyError(request, EXYNOS_REQUEST_RESULT::ERROR_REQUEST);
                if (ret != NO_ERROR) {
                    CLOGE("[F%d B%d] sendNotifyError fail. ret %d",
                            frame->getFrameCount(), buffer.index, ret);
                    return ret;
                }
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

        break;
#ifdef USE_DUAL_CAMERA
    case PIPE_SYNC_REPROCESSING:
        if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_PREVIEW_MODE_HW) {
            if (frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
                if (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_VIRTUAL_OTF) {
                    if (frame->getFrameState() != FRAME_STATE_SKIPPED) {
                        ret = m_setVOTFBuffer(PIPE_ISP_REPROCESSING, frame, PIPE_ISPC_REPROCESSING, PIPE_DCPS0_REPROCESSING);
                        if (ret != NO_ERROR) {
                            CLOGE("Set VOTF Buffer fail, pipeId(%d), srcPipe(%d), dstPipe(%d), ret(%d)",
                                    PIPE_ISP_REPROCESSING, PIPE_ISPC_REPROCESSING, PIPE_DCPS0_REPROCESSING, ret);
                        }
                    }
                }

                factory->pushFrameToPipe(frame, PIPE_ISP_REPROCESSING);
            } else if (frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE) {
                if (frame->getFrameState() == FRAME_STATE_SKIPPED) {
                    frame->getDstBuffer(PIPE_ISP_REPROCESSING, &buffer, factory->getNodeType(PIPE_ISPC_REPROCESSING));
                    if (ret != NO_ERROR) {
                        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", PIPE_ISP, ret);
                    }

                    if (buffer.index >= 0) {
                        ret = m_bufferSupplier->putBuffer(buffer);
                        if (ret != NO_ERROR) {
                            CLOGE("[F%d B%d]Failed to putBuffer for PIPE_SYNC_REPROCESSING. ret %d",
                                    frame->getFrameCount(), buffer.index, ret);
                        }
                    }
                }
            }
        } else if (m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
            factory->pushFrameToPipe(frame, PIPE_FUSION_REPROCESSING);
        }

        break;
#endif
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
    if (frame != NULL && frame->isComplete() == true && m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
        if (frame->getUpdateResult() == true) {
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

status_t ExynosCamera::m_handleNV21CaptureFrame(ExynosCameraFrameSP_sptr_t frame, int leaderPipeId)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

    int nodePipeId = -1;
    int streamId = -1;

    nodePipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;

    if (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) && frame->getNumRemainPipe() == 1) {
        ExynosCameraRequestSP_sprt_t request = m_requestMgr->getRunningRequest(frame->getFrameCount());
        if (request == NULL) {
            CLOGE("getRequest failed ");
            return INVALID_OPERATION;
        }

        ret = m_updateJpegPartialResultShot(request);
        if (ret != NO_ERROR) {
            CLOGE("Failed to m_updateJpegPartialResultShot");
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

            ret = frame->getDstBuffer(leaderPipeId, &dstBuffer, factory->getNodeType(nodePipeId));
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to getDstBuffer."
                        " pipeId %d streamId %d ret %d",
                        frame->getFrameCount(),
                        dstBuffer.index,
                        nodePipeId, streamId, ret);
                return ret;
            }

            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
            ret = frame->getDstBufferState(leaderPipeId, &bufferState, factory->getNodeType(nodePipeId));
            if (ret != NO_ERROR) {
                CLOGE("ERR(%s[%d]):Failed to getDstBufferState. frameCount %d pipeId %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(),
                        nodePipeId);
                return ret;
            } else if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
                request->setStreamBufferStatus(streamId, CAMERA3_BUFFER_STATUS_ERROR);
                CLOGE("ERR(%s[%d]):Invalid JPEG buffer state. frameCount %d bufferState %d",
                        __FUNCTION__, __LINE__,
                        frame->getFrameCount(),
                        bufferState);
            }

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
        if (m_getState() != EXYNOS_CAMERA_STATE_FLUSH) {
            int pipeId_next = -1;

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

            factory->setOutputFrameQToPipe(m_yuvCaptureDoneQ, pipeId_next);
            factory->pushFrameToPipe(frame, pipeId_next);
            if (factory->checkPipeThreadRunning(pipeId_next) == false) {
                factory->startThread(pipeId_next);
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
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    struct camera2_stream *shot_stream = NULL;

    int pipeId = -1;
    ExynosCameraBuffer buffer;
    buffer_manager_tag_t bufTag;

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

    srcBuffer.index = -2;
    dstBuffer.index = -2;

    CLOGD("[F%d]-- IN --", frame->getFrameCount());

    m_parameters[m_cameraId]->getDScaledYuvStallSize(&outputSizeW, &outputSizeH);

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
            pipeId = PIPE_MCSC_REPROCESSING;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
            pipeId = PIPE_DCP_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
            pipeId = PIPE_ISP_REPROCESSING;
        } else {
            pipeId = PIPE_3AA_REPROCESSING;
        }
    } else
#endif
    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
        pipeId = PIPE_MCSC_REPROCESSING;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
        pipeId = PIPE_ISP_REPROCESSING;
    } else {
        pipeId = PIPE_3AA_REPROCESSING;
    }

    ret = frame->getDstBuffer(leaderPipeId, &srcBuffer, factory->getNodeType(nodePipeId));
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

    srcRect.x = shot_stream->output_crop_region[0];
    srcRect.y = shot_stream->output_crop_region[1];
    srcRect.w = shot_stream->output_crop_region[2];
    srcRect.h = shot_stream->output_crop_region[3];
    srcRect.fullW = shot_stream->output_crop_region[2];
    srcRect.fullH = shot_stream->output_crop_region[3];
    srcRect.colorFormat = V4L2_PIX_FMT_NV21;

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

    factory->setOutputFrameQToPipe(m_resizeDoneQ, gscPipeId);
    factory->pushFrameToPipe(frame, gscPipeId);

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
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    int dsWidth  = 0, dsHeight  = 0;
    int pipeId = -1;
    ExynosRect bdsRect;
    bool flagFlite3aaM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) == HW_CONNECTION_MODE_M2M);

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);
        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    m_parameters[m_cameraId]->getMaxSensorSize(&maxSensorW, &maxSensorH);
    CLOGI("HW Sensor MAX width x height = %dx%d",
            maxSensorW, maxSensorH);
    m_parameters[m_cameraId]->getPreviewBdsSize(&bdsRect, false);
    CLOGI("Preview BDS width x height = %dx%d",
            bdsRect.w, bdsRect.h);

    if (m_parameters[m_cameraId]->getHighSpeedRecording() == true
#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
        && m_parameters[m_cameraId]->getConfigMode() != CONFIG_MODE::HIGHSPEED_60
#endif
        ) {

        m_parameters[m_cameraId]->getHwPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("PreviewSize(HW - Highspeed) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    } else {
        m_parameters[m_cameraId]->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
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

    if (m_parameters[m_cameraId]->getPIPMode() == true && getCameraId() == CAMERA_ID_FRONT) {
        maxBufferCount = m_exynosconfig->current->bufInfo.num_3aa_buffers;
    } else {
        maxBufferCount  = m_exynosconfig->current->bufInfo.num_sensor_buffers;
    }

    if (m_parameters[m_cameraId]->getSensorControlDelay() == 0) {
        maxBufferCount -= SENSOR_REQUEST_DELAY;
    }

    bufConfig = initBufConfig;
    bufConfig.planeCount = 2;
#ifdef CAMERA_PACKED_BAYER_ENABLE
    if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
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
    bufConfig.needMmap = false;
#endif
#ifdef RESERVED_MEMORY_ENABLE
    if (getCameraId() == CAMERA_ID_BACK) {
        bufConfig.reservedMemoryCount = RESERVED_NUM_BAYER_BUFFERS;
        if (m_rawStreamExist)
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
        else
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else if (m_parameters[m_cameraId]->getPIPMode() == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_BAYER_BUFFERS;
        if (m_rawStreamExist)
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
        else
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else
#endif
    {
        if (m_rawStreamExist)
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

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc FLITE_BUF. ret %d", ret);
        return ret;
    }

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
        maxBufferCount -= SENSOR_REQUEST_DELAY;
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
        if (m_parameters[m_cameraId]->getUHDRecordingMode() == true) {
            bufConfig.reservedMemoryCount = RESERVED_NUM_ISP_BUFFERS_ON_UHD;
        } else {
            bufConfig.reservedMemoryCount = RESERVED_NUM_ISP_BUFFERS;
        }
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else if (m_parameters[m_cameraId]->getPIPMode() == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_ISP_BUFFERS;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_RESERVED_TYPE;
    } else
#endif
    {
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    }

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc ISP_BUF. ret %d", ret);
        return ret;
    }

    /* GDC */
    int videoOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_VIDEO);
    if (videoOutputPortId > -1) {
        ExynosRect videoRect;
        int videoFormat = m_parameters[m_cameraId]->getYuvFormat(videoOutputPortId);
        m_parameters[m_cameraId]->getYuvSize(&videoRect.w, &videoRect.h, videoOutputPortId);

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

#ifdef SUPPORT_ME
    if (m_parameters[m_cameraId]->getLLSOn() == true) {
        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_ME;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->createBufferManager("ME_BUF", m_ionAllocator, bufTag);
        if (ret != NO_ERROR) {
            CLOGE("Failed to create ME_BUF. ret %d", ret);
        }

        bufConfig = initBufConfig;
        bufConfig.planeCount = 2;
        int width, height;
        m_parameters[m_cameraId]->getMeSize(&width, &height);
        bufConfig.size[0] = width * height * 2;
        bufConfig.reqBufCount = maxBufferCount; /* same as 3AA buf count */
        bufConfig.allowedMaxBufCount = maxBufferCount;
        bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
        bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
        bufConfig.createMetaPlane = true;
        bufConfig.needMmap = true;
        bufConfig.reservedMemoryCount = 0;

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc 3AA_BUF. ret %d", ret);
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

        maxBufferCount  = m_exynosconfig->current->bufInfo.num_vra_buffers;

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

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc VRA_BUF. ret %d", ret);
            return ret;
        }
    }

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true) {
        if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
            /* DCPS0 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_ISPC;
            bufTag.pipeId[1] = PIPE_DCPS0;
            bufTag.pipeId[2] = PIPE_ISPC_REPROCESSING;
            bufTag.pipeId[3] = PIPE_DCPS0_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPS0_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPS_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 2;
            bufConfig.size[0] = ALIGN_UP(maxSensorW * 2 * 10 / 16 * 2, CAMERA_16PX_ALIGN) * ALIGN_UP(maxSensorH, CAMERA_16PX_ALIGN);
            bufConfig.reqBufCount = NUM_HW_DIS_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_HW_DIS_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = false;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc DCPS_BUF. ret %d", ret);
                return ret;
            }

            /* DCPC0 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_DCPC0;
            bufTag.pipeId[1] = PIPE_MCSC;
            bufTag.pipeId[2] = PIPE_DCPC0_REPROCESSING;
            bufTag.pipeId[3] = PIPE_MCSC_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPC0_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPC0_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 3;
            getYuvPlaneSize(V4L2_PIX_FMT_NV16M, bufConfig.size, ALIGN_UP(maxPreviewW + 512, 32), ALIGN_UP(maxPreviewH + 272, 32));

            bufConfig.reqBufCount = NUM_DCP_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_DCP_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = false;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc ISP_BUF. ret %d", ret);
                return ret;
            }

            /* DCPC1 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_DCPC1;
            bufTag.pipeId[1] = PIPE_CIPS1;
            bufTag.pipeId[2] = PIPE_DCPC1_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPC1_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPC1_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 3;
            getYuvPlaneSize(V4L2_PIX_FMT_NV16M, bufConfig.size, ALIGN_UP(maxPreviewW + 512, 32), ALIGN_UP(maxPreviewH + 272, 32));

            bufConfig.reqBufCount = NUM_DCP_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_DCP_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = true;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc DCPC1_BUF. ret %d", ret);
                return ret;
            }

            /* DCPC2 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_DCPC2;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPC2_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPC2_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 2;
            bufConfig.size[0] = DCP_DISPARITY_BUFFER_SIZE_W * DCP_DISPARITY_BUFFER_SIZE_H;
            bufConfig.reqBufCount = NUM_DCP_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_DCP_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = false;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc DCPC2_BUF. ret %d", ret);
                return ret;
            }

            /* DCPC3 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_DCPC3;
            bufTag.pipeId[1] = PIPE_DCPC3_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPC3_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPC2_3_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 3;
            getYuvPlaneSize(V4L2_PIX_FMT_NV12M, bufConfig.size, ALIGN_UP(maxPreviewW + 512, 32), ALIGN_UP(maxPreviewH + 272, 32));
            bufConfig.reqBufCount = NUM_DCP_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_DCP_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = false;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc DCPC1_BUF. ret %d", ret);
                return ret;
            }

            /* DCPC4 */
            bufTag = initBufTag;
            bufTag.pipeId[0] = PIPE_DCPC4;
            bufTag.pipeId[1] = PIPE_DCPC4_REPROCESSING;
            bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

            ret = m_bufferSupplier->createBufferManager("DCPC4_BUF", m_ionAllocator, bufTag);
            if (ret != NO_ERROR) {
                CLOGE("Failed to create DCPC2_3_BUF. ret %d", ret);
            }

            bufConfig = initBufConfig;
            bufConfig.planeCount = 3;
            getYuvPlaneSize(V4L2_PIX_FMT_NV12M, bufConfig.size, ALIGN_UP(maxPreviewW + 512, 32), ALIGN_UP(maxPreviewH + 272, 32));
            bufConfig.reqBufCount = NUM_DCP_BUFFERS;
            bufConfig.allowedMaxBufCount = NUM_DCP_BUFFERS;
            bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
            bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
            bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
            bufConfig.createMetaPlane = true;
            bufConfig.needMmap = false;
            bufConfig.reservedMemoryCount = 0;

            ret = m_allocBuffers(bufTag, bufConfig);
            if (ret != NO_ERROR) {
                CLOGE("Failed to alloc DCPC1_BUF. ret %d", ret);
                return ret;
            }
        } else if (m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
            int previewOutputPortId = m_streamManager->getOutputPortId(HAL_STREAM_ID_PREVIEW);
            if (previewOutputPortId > -1) {
                ExynosRect previewRect;
                int previewFormat = m_parameters[m_cameraId]->getYuvFormat(previewOutputPortId);
                m_parameters[m_cameraId]->getYuvSize(&previewRect.w, &previewRect.h, previewOutputPortId);

                bufTag = initBufTag;
                bufTag.pipeId[0] = (previewOutputPortId % ExynosCameraParameters::YUV_MAX) + PIPE_MCSC0;
                bufTag.pipeId[1] = PIPE_FUSION;
                bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

                ret = m_bufferSupplier->createBufferManager("FUSION_BUF", m_ionAllocator, bufTag);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to create FUSION_BUF. ret %d", ret);
                }

                bufConfig = initBufConfig;
                bufConfig.planeCount = getYuvPlaneCount(previewFormat) + 1;
                getYuvPlaneSize(previewFormat, bufConfig.size, previewRect.w, previewRect.h);
                bufConfig.reqBufCount = 14;
                bufConfig.allowedMaxBufCount = 14;
                bufConfig.startBufIndex = m_exynosconfig->current->bufInfo.num_preview_buffers;
                bufConfig.batchSize = m_parameters[m_cameraId]->getBatchSize((enum pipeline)bufTag.pipeId[0]);
                bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
                bufConfig.allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;
                bufConfig.createMetaPlane = true;
                bufConfig.needMmap = true;
                bufConfig.reservedMemoryCount = 0;

                ret = m_allocBuffers(bufTag, bufConfig);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to alloc FUSION_BUF. ret %d", ret);
                    return ret;
                }

                int totalBufferCount = bufConfig.allowedMaxBufCount + m_parameters[m_cameraId]->getYuvBufferCount(previewOutputPortId);
                ret = m_parameters[m_cameraId]->setYuvBufferCount(totalBufferCount, previewOutputPortId);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to setYuvBufferFUSION_BUF. ret %d", ret);
                    return ret;
                }

                if (m_parameters[m_cameraIds[1]] != NULL) {
                    ret = m_parameters[m_cameraIds[1]]->setYuvBufferCount(totalBufferCount, previewOutputPortId);
                    if (ret != NO_ERROR) {
                        CLOGE("Failed to setYuvBufferFUSION_BUF. ret %d", ret);
                        return ret;
                    }
                }
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
    int minBufferCount = 1;
    int maxBufferCount = 1;
    int maxSensorW  = 0, maxSensorH  = 0;
    int maxPreviewW  = 0, maxPreviewH  = 0;
    int pipeId = -1;
    ExynosRect bdsRect;

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);
        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    m_parameters[m_cameraId]->getMaxSensorSize(&maxSensorW, &maxSensorH);
    CLOGI("HW Sensor MAX width x height = %dx%d",
            maxSensorW, maxSensorH);
    m_parameters[m_cameraId]->getPreviewBdsSize(&bdsRect);
    CLOGI("Preview BDS width x height = %dx%d",
            bdsRect.w, bdsRect.h);

    if (m_parameters[m_cameraId]->getHighSpeedRecording() == true) {
        m_parameters[m_cameraId]->getHwPreviewSize(&maxPreviewW, &maxPreviewH);
        CLOGI("PreviewSize(HW - Highspeed) width x height = %dx%d",
                maxPreviewW, maxPreviewH);
    } else {
        m_parameters[m_cameraId]->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
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
                                                  ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    int streamId = -1;
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;
    enum pipeline controlPipeId = (enum pipeline) m_parameters[m_cameraId]->getPerFrameControlPipe();
    int batchSize = m_parameters[m_cameraId]->getBatchSize(controlPipeId);

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif

    for (int batchIndex = 0; batchIndex < batchSize; batchIndex++) {
        if (batchIndex > 0 && m_requestPreviewWaitingList.empty() == false) {
            /* Use next request to get stream buffers */
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

       if (request->getFrameCount() == 0)
           m_requestMgr->setFrameCount(m_internalFrameCount++, request->getKey());

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
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true
                    && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                    if (flagDcpMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                        leaderPipeId = PIPE_MCSC;
                    } else if (flagIspDcpM2M == true
                               && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
                        leaderPipeId = PIPE_DCP;
                    } else if (flag3aaIspM2M == true
                               && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                        leaderPipeId = PIPE_ISP;
                    } else {
                        leaderPipeId = PIPE_3AA;
                    }
                } else
#endif
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
        case HAL_STREAM_ID_PREVIEW_VIDEO:
#ifdef USE_DUAL_CAMERA
            if (m_parameters[m_cameraId]->getDualMode() == true
                && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                if (flagDcpMcscM2M == true
                    && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                    leaderPipeId = PIPE_MCSC;
                } else if (flagIspDcpM2M == true
                           && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
                    leaderPipeId = PIPE_DCP;
                } else if (flag3aaIspM2M == true
                           && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                    leaderPipeId = PIPE_ISP;
                } else {
                    leaderPipeId = PIPE_3AA;
                }
            } else
#endif
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
                                                    ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_CAPTURE_PREVIEW];
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA, PIPE_ISP) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP, PIPE_DCP) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP, PIPE_MCSC) == HW_CONNECTION_MODE_M2M);
#endif

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
                leaderPipeId = PIPE_3AA;
                if (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) {
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
            case HAL_STREAM_ID_VIDEO:
                if (m_parameters[m_cameraId]->getGDCEnabledMode() == true) {
                    leaderPipeId = PIPE_GDC;
                    bufTag.pipeId[0] = PIPE_GDC;
                    break;
                }
            case HAL_STREAM_ID_PREVIEW:
#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true
                    && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW
                    && m_parameters[m_cameraId]->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
                    leaderPipeId = PIPE_FUSION;
                    bufTag.pipeId[0] = PIPE_FUSION;
                    break;
                }
#endif
            case HAL_STREAM_ID_CALLBACK:
            case HAL_STREAM_ID_PREVIEW_VIDEO:
#ifdef USE_DUAL_CAMERA
                if (m_parameters[m_cameraId]->getDualMode() == true
                    && m_parameters[m_cameraId]->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
                    if (flagDcpMcscM2M == true
                        && IS_OUTPUT_NODE(factory, PIPE_MCSC) == true) {
                        leaderPipeId = PIPE_MCSC;
                    } else if (flagIspDcpM2M == true
                            && IS_OUTPUT_NODE(factory, PIPE_DCP) == true) {
                        leaderPipeId = PIPE_DCP;
                    } else if (flag3aaIspM2M == true
                            && IS_OUTPUT_NODE(factory, PIPE_ISP) == true) {
                        leaderPipeId = PIPE_ISP;
                    } else {
                        leaderPipeId = PIPE_3AA;
                    }
                } else
#endif
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

        buffer.handle[0] = handle;
        buffer.acquireFence[0] = streamBuffer->acquire_fence;
        buffer.releaseFence[0] = streamBuffer->release_fence;
        nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
#ifdef SUPPORT_DEPTH_MAP
            if (streamId % HAL_STREAM_ID_MAX == HAL_STREAM_ID_DEPTHMAP) {
                frame->setRequest(PIPE_VC1, false);
            }
#endif
            continue;
        }

        ret = request->setAcquireFenceDone(handle, (buffer.acquireFence[0] == -1) ? true : false);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
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
    ExynosCameraFrameFactory *factory;
    uint32_t leaderPipeId = 0;
    uint32_t nodeType = 0;
    uint32_t frameType = 0;
    buffer_manager_tag_t bufTag;
    const buffer_manager_tag_t initBufTag;
    int yuvStallPipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return BAD_VALUE;
    }

    if (frame->getFrameType() == FRAME_TYPE_JPEG_REPROCESSING)
        factory = m_frameFactory[FRAME_FACTORY_TYPE_JPEG_REPROCESSING];
    else
        factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];

    const camera3_stream_buffer_t *bufferList = request->getOutputBuffers();
    frameType = frame->getFrameType();

    /* Set Internal Buffers */
    if ((frame->getRequest(yuvStallPipeId) == true)
        && (frame->getStreamRequested(STREAM_TYPE_YUVCB_STALL) == false)) {
#ifdef USE_DUAL_CAMERA
        if (frameType == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
            leaderPipeId = PIPE_MCSC_REPROCESSING;
        } else
#endif
        {
            leaderPipeId = (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) ?
                            PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
        }
        bufTag.pipeId[0] = yuvStallPipeId;
        bufTag.managerType = BUFFER_MANAGER_ION_TYPE;

        ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
        if (ret != NO_ERROR || buffer.index < 0) {
            CLOGE("[R%d F%d B%d]Failed to getBuffer. ret %d",
                    request->getKey(), frame->getFrameCount(), buffer.index, ret);
            return BAD_VALUE;
        }

        nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);

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
    if (frameType != FRAME_TYPE_INTERNAL) {
        for (uint32_t index = 0; index < request->getNumOfOutputBuffer(); index++) {
            const camera3_stream_buffer_t *streamBuffer = &(bufferList[index]);
            int streamId = request->getStreamIdwithBufferIdx(index);
            bufTag = initBufTag;
            buffer_handle_t *handle = streamBuffer->buffer;
            bufTag.managerType = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;

            switch (streamId % HAL_STREAM_ID_MAX) {
                case HAL_STREAM_ID_JPEG:
                    if (m_parameters[m_cameraId]->getYuvStallPortUsage() == YUV_STALL_USAGE_PICTURE) {
                        leaderPipeId = PIPE_JPEG_REPROCESSING;
                        bufTag.pipeId[0] = PIPE_JPEG_REPROCESSING;
                    } else if (m_parameters[m_cameraId]->isReprocessing() == true && m_parameters[m_cameraId]->isUseHWFC() == true) {
#ifdef USE_DUAL_CAMERA
                        if (m_parameters[m_cameraId]->getDualMode() == true
                            && m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
                            if (flagDcpMcscM2M == true
                                && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                                leaderPipeId = PIPE_MCSC_REPROCESSING;
                            } else if (flagIspDcpM2M == true
                                    && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
                                leaderPipeId = PIPE_DCP_REPROCESSING;
                            } else if (flag3aaIspM2M == true
                                    && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                                leaderPipeId = PIPE_ISP_REPROCESSING;
                            } else {
                                leaderPipeId = PIPE_3AA_REPROCESSING;
                            }
                        } else
#endif
                        if (flagIspMcscM2M == true
                            && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
                            leaderPipeId = PIPE_MCSC_REPROCESSING;
                        } else if (flag3aaIspM2M == true
                                && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
                            leaderPipeId = PIPE_ISP_REPROCESSING;
                        } else {
                            leaderPipeId = PIPE_3AA_REPROCESSING;
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
                        leaderPipeId = (m_parameters[m_cameraId]->getUsePureBayerReprocessing() == true) ?
                                       PIPE_3AA_REPROCESSING : PIPE_ISP_REPROCESSING;
                        bufTag.pipeId[0] = (m_streamManager->getOutputPortId(streamId)
                                           % ExynosCameraParameters::YUV_MAX)
                                           + PIPE_MCSC0_REPROCESSING;
                    }
                    break;
                case HAL_STREAM_ID_THUMBNAIL_CALLBACK:
                    leaderPipeId = PIPE_GSC_REPROCESSING2;
                    bufTag.pipeId[0] = PIPE_GSC_REPROCESSING2;
                    break;
                case HAL_STREAM_ID_RAW:
                    leaderPipeId = PIPE_3AA_REPROCESSING;
                    bufTag.pipeId[0] = PIPE_3AC_REPROCESSING;
                    break;
                case HAL_STREAM_ID_ZSL_OUTPUT:
                case HAL_STREAM_ID_PREVIEW:
                case HAL_STREAM_ID_VIDEO:
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

            buffer.handle[0] = handle;
            buffer.acquireFence[0] = streamBuffer->acquire_fence;
            buffer.releaseFence[0] = streamBuffer->release_fence;

            ret = m_bufferSupplier->getBuffer(bufTag, &buffer);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[R%d F%d B%d S%d]Failed to getBuffer. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            nodeType = (uint32_t) factory->getNodeType(bufTag.pipeId[0]);
            ret = request->setAcquireFenceDone(handle, (buffer.acquireFence[0] == -1) ? true : false);
            if (ret != NO_ERROR || buffer.index < 0) {
                CLOGE("[R%d F%d B%d S%d]Failed to setAcquireFenceDone. ret %d",
                        request->getKey(), frame->getFrameCount(), buffer.index, streamId, ret);
                continue;
            }

            ret = frame->setDstBufferState(leaderPipeId,
                                           ENTITY_BUFFER_STATE_REQUESTED,
                                           nodeType);
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
    int DScaledYuvStallSizeW = 0, DScaledYuvStallSizeH = 0;
    int pictureFormat = 0;

    if (m_ionAllocator == NULL || m_bufferSupplier == NULL) {
        CLOGE("Allocator %p, BufferSupplier %p is NULL",
                m_ionAllocator, m_bufferSupplier);

        return INVALID_OPERATION;
    }

    CLOGI("alloc buffer - camera ID: %d", m_cameraId);

    if (m_parameters[m_cameraId]->getHighSpeedRecording() == true
#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
        && m_parameters[m_cameraId]->getConfigMode() != CONFIG_MODE::HIGHSPEED_60
#endif
        )
        m_parameters[m_cameraId]->getHwSensorSize(&maxPictureW, &maxPictureH);
    else
        m_parameters[m_cameraId]->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters[m_cameraId]->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);
    pictureFormat = m_parameters[m_cameraId]->getHwPictureFormat();

    CLOGI("Max Picture %ssize %dx%d format %x",
            (m_parameters[m_cameraId]->getHighSpeedRecording() == true)? "on HighSpeedRecording ":"",
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
        bufConfig.needMmap = false;
        bufConfig.reservedMemoryCount = 0;

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc ISP_REP_BUF. ret %d", ret);
            return ret;
        }
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
    } else if (m_parameters[m_cameraId]->getPIPMode() == false) {
        bufConfig.reservedMemoryCount = FRONT_RESERVED_NUM_INTERNAL_NV21_BUFFERS;
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_RESERVED_TYPE;
    } else
#endif
    {
        bufConfig.type = EXYNOS_CAMERA_BUFFER_ION_CACHED_TYPE;
    }

    CLOGD("Allocate CAPTURE_CB(NV21)_BUF (minBufferCount=%d, maxBufferCount=%d)",
                minBufferCount, maxBufferCount);

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret < 0) {
        CLOGE("Failed to alloc CAPTURE_CB_BUF. ret %d", ret);
        return ret;
    }

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

    minBufferCount = 1;
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

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc YUV_CAP_BUF. ret %d", ret);
        return ret;
    }

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

    minBufferCount = 1;
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

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc THUMB_BUF. ret %d", ret);
        return ret;
    }

    /* DScaleYuvStall Internal buffer */
    m_parameters[m_cameraId]->getDScaledYuvStallSize(&DScaledYuvStallSizeW, &DScaledYuvStallSizeH);
    maxBufferCount = m_exynosconfig->current->bufInfo.num_picture_buffers;

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

    ret = m_allocBuffers(bufTag, bufConfig);
    if (ret != NO_ERROR) {
        CLOGE("Failed to alloc PRE_THUMBNAIL_BUF. ret %d", ret);
        return ret;
    }

    CLOGI("m_allocBuffers(DSCALEYUVSTALL_BUF Buffer) %d x %d,\
        planeSize(%d), planeCount(%d), maxBufferCount(%d)",
        DScaledYuvStallSizeW, DScaledYuvStallSizeH,
        bufConfig.size[0], bufConfig.planeCount, maxBufferCount);

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

    return NO_ERROR;
}

status_t ExynosCamera::m_sendForceYuvStreamResult(ExynosCameraRequestSP_sprt_t request,
                                                   ExynosCameraFrameSP_sptr_t frame)
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
            case HAL_STREAM_ID_PREVIEW_VIDEO:
            case HAL_STREAM_ID_CALLBACK:
#ifdef SUPPORT_DEPTH_MAP
            case HAL_STREAM_ID_DEPTHMAP:
#endif
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

        ret = m_checkStreamBuffer(frame, stream, &buffer, request);
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

        ret = m_checkStreamBufferStatus(request, stream, &streamBuffer->status);
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

        ret = m_bufferSupplier->putBuffer(buffer);
        if (ret != NO_ERROR) {
            CLOGE("[R%d F%d S%d B%d]Failed to putBuffer. ret %d",
                    request->getKey(), frameCount, streamId, buffer.index, ret);
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
    int nodePipeId = m_parameters[m_cameraId]->getYuvStallPort() + PIPE_MCSC0_REPROCESSING;
    int gscPipeId = PIPE_GSC_REPROCESSING2;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;
    int thumbnailH, thumbnailW;
    int inputSizeH, inputSizeW;
    int waitCount = 0;
    ExynosCameraFrameFactory *factory = m_frameFactory[FRAME_FACTORY_TYPE_REPROCESSING];
    int streamId = HAL_STREAM_ID_THUMBNAIL_CALLBACK;
    ExynosCameraRequestSP_sprt_t request = NULL;
    ExynosCameraStream *stream = NULL;
    int bufIndex = -2;

    bool flag3aaIspM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagIspMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#ifdef USE_DUAL_CAMERA
    bool flagIspDcpM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_DCP_REPROCESSING) == HW_CONNECTION_MODE_M2M);
    bool flagDcpMcscM2M = (m_parameters[m_cameraId]->getHwConnectionMode(PIPE_DCP_REPROCESSING, PIPE_MCSC_REPROCESSING) == HW_CONNECTION_MODE_M2M);
#endif

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

    m_parameters[m_cameraId]->getDScaledYuvStallSize(&inputSizeW, &inputSizeH);
    m_parameters[m_cameraId]->getThumbnailCbSize(&thumbnailW, &thumbnailH);
    request = m_requestMgr->getRunningRequest(frame->getFrameCount());
    if (request == NULL) {
        CLOGE("getRequest failed ");
        return INVALID_OPERATION;
    }

    factory = request->getFrameFactory(streamId);

#ifdef USE_DUAL_CAMERA
    if (m_parameters[m_cameraId]->getDualMode() == true
        && m_parameters[m_cameraId]->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_HW) {
        if (flagDcpMcscM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
            srcPipeId = PIPE_MCSC_REPROCESSING;
        } else if (flagIspDcpM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_DCP_REPROCESSING) == true) {
            srcPipeId = PIPE_DCP_REPROCESSING;
        } else if (flag3aaIspM2M == true
                && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
            srcPipeId = PIPE_ISP_REPROCESSING;
        } else {
            srcPipeId = PIPE_3AA_REPROCESSING;
        }
    } else
#endif
    if (flagIspMcscM2M == true
        && IS_OUTPUT_NODE(factory, PIPE_MCSC_REPROCESSING) == true) {
        srcPipeId = PIPE_MCSC_REPROCESSING;
    } else if (flag3aaIspM2M == true
            && IS_OUTPUT_NODE(factory, PIPE_ISP_REPROCESSING) == true) {
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
        ret = frame->getDstBuffer(srcPipeId, &srcBuffer, factory->getNodeType(nodePipeId));
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

    factory->setOutputFrameQToPipe(m_thumbnailPostCbQ, gscPipeId);
    factory->pushFrameToPipe(frame, gscPipeId);

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

status_t ExynosCamera::m_setVendorBuffers()
{
    status_t ret = NO_ERROR;

    int width = 0, height = 0;
    int planeCount = 0;
    int buffer_count = 0;
    bool needMmap = false;
    exynos_camera_buffer_type_t buffer_type = EXYNOS_CAMERA_BUFFER_ION_NONCACHED_TYPE;
    buffer_manager_allocation_mode_t allocMode = BUFFER_MANAGER_ALLOCATION_ATONCE;

    m_parameters[m_cameraId]->getVideoSize(&width, &height);

    if (buffer_count > 0) {
        const buffer_manager_tag_t initBufTag;
        const buffer_manager_configuration_t initBufConfig;
        buffer_manager_configuration_t bufConfig;
        buffer_manager_tag_t bufTag;

        bufTag = initBufTag;
        bufTag.pipeId[0] = PIPE_MCSC0;
        bufTag.pipeId[1] = PIPE_MCSC1;
        bufTag.pipeId[2] = PIPE_MCSC2;
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

        ret = m_allocBuffers(bufTag, bufConfig);
        if (ret != NO_ERROR) {
            CLOGE("Failed to alloc MCSC_OUT_BUF. ret %d", ret);
            return ret;
        }

        CLOGI("m_allocBuffers(MCSC_OUT_BUF) %d x %d,\
            planeSize(%d), planeCount(%d), maxBufferCount(%d)",
            width, height,
            bufConfig.size[0], bufConfig.planeCount, buffer_count);
    }

    return ret;
}

}; /* namespace android */
