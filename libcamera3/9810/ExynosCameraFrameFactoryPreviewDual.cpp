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
#define LOG_TAG "ExynosCameraFrameFactoryPreviewDual"
#include <log/log.h>

#include "ExynosCameraFrameFactoryPreviewDual.h"

namespace android {
ExynosCameraFrameFactoryPreviewDual::~ExynosCameraFrameFactoryPreviewDual()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCameraFrameFactoryPreviewDual::mapBuffers(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraFrameFactoryPreview::mapBuffers();
    if (ret != NO_ERROR) {
        CLOGE("Construct Pipes fail! ret(%d)", ret);
        return ret;
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        ret = m_pipes[PIPE_DCP]->setMapBuffer();
        if (ret != NO_ERROR) {
            CLOGE("DCP mapBuffer fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("Map buffer Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGI("start pre-ordered initial pipe thread");

    ret = ExynosCameraFrameFactoryPreview::startInitialThreads();
    if (ret != NO_ERROR) {
        CLOGE("Construct Pipes fail! ret(%d)", ret);
        return ret;
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M) {
            ret = startThread(PIPE_DCP);
            if (ret != NO_ERROR) {
                return ret;
            }
        }

#ifdef USE_CIP_M2M_HW
        ret = startThread(PIPE_CIP);
        if (ret != NO_ERROR) {
            return ret;
        }
#endif

        // HACK: ToDo remove
        if (m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
            ret = startThread(PIPE_MCSC);
            if (ret != NO_ERROR) {
                return ret;
            }
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::stopPipes(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (m_pipes[PIPE_3AA]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_3AA]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("3AA stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_ISP]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_ISP]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        if (m_pipes[PIPE_DCP] != NULL
            && m_pipes[PIPE_DCP]->isThreadRunning() == true) {
            ret = m_pipes[PIPE_DCP]->stopThread();
            if (ret != NO_ERROR) {
                CLOGE("PIPE_FUSION stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

    if (m_pipes[PIPE_MCSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_MCSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_request[PIPE_VC0] == true
        && m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_FLITE]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("FLITE stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#if defined(SUPPORT_HW_GDC)
    if (m_pipes[PIPE_GDC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GDC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("GDC stopThread fail. ret(%d)", ret);
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[PIPE_VRA]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_VRA]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_VRA stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_GSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#ifdef USE_CIP_M2M_HW
    if (m_pipes[PIPE_CIP] != NULL
        && m_pipes[PIPE_CIP]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_CIP]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("CIP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[PIPE_SYNC] != NULL
        && m_pipes[PIPE_SYNC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_SYNC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("SYNC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        if (m_pipes[PIPE_FUSION] != NULL
            && m_pipes[PIPE_FUSION]->isThreadRunning() == true) {
            ret = stopThread(PIPE_FUSION);
            if (ret != NO_ERROR) {
                CLOGE("PIPE_FUSION stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_FLITE]->sensorStream(false);
    } else {
        ret = m_pipes[PIPE_3AA]->sensorStream(false);
    }
    if (ret != NO_ERROR) {
        CLOGE("FLITE sensorStream off fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_FLITE]->stop();
        if (ret != NO_ERROR) {
            CLOGE("FLITE stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* 3AA force done */
    ret = m_pipes[PIPE_3AA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_3AA force done fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    /* stream off for 3AA */
    ret = m_pipes[PIPE_3AA]->stop();
    if (ret != NO_ERROR) {
        CLOGE("3AA stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    if (m_pipes[PIPE_ISP]->flagStart() == true) {
        /* ISP force done */
        ret = m_pipes[PIPE_ISP]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_ISP force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        /* stream off for ISP */
        ret = m_pipes[PIPE_ISP]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_DCP]->flagStart() == true) {
        /* ISP force done */
        ret = m_pipes[PIPE_DCP]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_DCP force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        /* stream off for ISP */
        ret = m_pipes[PIPE_DCP]->stop();
        if (ret != NO_ERROR) {
            CLOGE("DCP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_MCSC]->flagStart() == true) {
        /* MCSC force done */
        ret = m_pipes[PIPE_MCSC]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_MCSC force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        ret = m_pipes[PIPE_MCSC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_VRA]->flagStart() == true) {
        /* VRA force done */
        ret = m_pipes[PIPE_VRA]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_VRA force done fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }

        ret = m_pipes[PIPE_VRA]->stop();
        if (ret != NO_ERROR) {
            CLOGE("VRA stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#if defined(SUPPORT_HW_GDC)
    ret = stopThreadAndWait(PIPE_GDC);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GDC stopThreadAndWait fail, ret(%d)", ret);
        funcRet |= ret;
    }
#endif

    ret = stopThreadAndWait(PIPE_GSC);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC stopThreadAndWait fail, ret(%d)", ret);
    }

#ifdef USE_CIP_M2M_HW
    if (m_pipes[PIPE_CIP] != NULL) {
        ret = m_pipes[PIPE_CIP]->stop();
        if (ret != NO_ERROR) {
            CLOGE("CIP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[PIPE_SYNC] != NULL) {
        ret = m_pipes[PIPE_SYNC]->stop();
        if (ret != NO_ERROR) {
            CLOGE("SYNC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        if (m_pipes[PIPE_FUSION] != NULL) {
            ret = m_pipes[PIPE_FUSION]->stop();
            if (ret < 0) {
                CLOGE("m_pipes[PIPE_FUSION]->stop() fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        funcRet |= ret;
    }

    CLOGI("Stopping  Success!");

    return funcRet;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameFactoryPreviewDual::createNewFrame(uint32_t frameCount, bool useJpegFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrameSP_sptr_t frame = NULL;

    int requestEntityCount = 0;
    int pipeId = -1;

    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }

    frame = m_frameMgr->createFrame(m_parameters, frameCount, FRAME_TYPE_PREVIEW);

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return NULL;
    }

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("frame(%d) metadata initialize fail", m_frameCount);

    if (m_request[PIPE_VC0] == true
        && m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        /* set flite pipe to linkageList */
        pipeId = PIPE_FLITE;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set 3AA pipe to linkageList */
    pipeId = PIPE_3AA;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);
    requestEntityCount++;

    /* set ISP pipe to linkageList */
    pipeId = PIPE_ISP;
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M
        && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        /* set DCP pipe to linkageList */
        pipeId = PIPE_DCP;
        if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M
            && m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }

#ifdef USE_CIP_M2M_HW
        /* set CIP pipe to linkageList */
        pipeId = PIPE_CIP;
        if (m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
#endif

        /* set MCSC pipe to linkageList */
        pipeId = PIPE_MCSC;
        if (m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M
            && m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
    } else {
        /* set MCSC pipe to linkageList */
        pipeId = PIPE_MCSC;
        if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
            && m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
    }

    /* set GDC pipe to linkageList */
    if (m_parameters->getGDCEnabledMode() == true) {
        pipeId = PIPE_GDC;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set VRA pipe to linkageList */
    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M
        && m_request[PIPE_MCSC5] == true
        && m_request[PIPE_VRA] == true) {
        pipeId = PIPE_VRA;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set Sync pipe to linkageList */
    pipeId = PIPE_SYNC;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set Sync pipe to linkageList */
    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        pipeId = PIPE_FUSION;
        if (m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
    }

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);

    /* set GSC for Video pipe to linkageList */
    pipeId = PIPE_GSC_VIDEO;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);

    if (m_supportReprocessing == false) {
        /* set GSC for Capture pipe to linkageList */
        pipeId = PIPE_GSC_PICTURE;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);

        /* set JPEG pipe to linkageList */
        pipeId = PIPE_JPEG;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
    }

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_setupConfig()
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_flagFlite3aaOTF = m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
    m_flag3aaIspOTF = m_parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP);
    m_flagIspMcscOTF = m_parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC);
    m_flagIspDcpOTF = m_parameters->getHwConnectionMode(PIPE_ISP, PIPE_DCP);
    m_flagDcpMcscOTF = m_parameters->getHwConnectionMode(PIPE_DCP, PIPE_MCSC);
    m_flagMcscVraOTF = m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA);

    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request[PIPE_3AP] = (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M);
    if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M
        || m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        m_request[PIPE_ISPC] = true;
    }

    /* FLITE ~ MCSC */
    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE("m_setDeviceInfo() fail, ret(%d)", ret);
        return ret;
    }

#if defined(SUPPORT_HW_GDC)
    /* GDC */
    m_nodeNums[INDEX(PIPE_GDC)][OUTPUT_NODE] = FIMC_IS_VIDEO_GDC_NUM;
#endif

    /* GSC */
    m_nodeNums[PIPE_GSC][OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;

    /* GSC for Recording */
    m_nodeNums[PIPE_GSC_VIDEO][OUTPUT_NODE] = VIDEO_GSC_NODE_NUM;

    /* GSC for Capture */
    m_nodeNums[PIPE_GSC_PICTURE][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG */
    m_nodeNums[PIPE_JPEG][OUTPUT_NODE] = -1;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_constructPipes(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    int pipeId = -1;

    ret = ExynosCameraFrameFactoryPreview::m_constructPipes();
    if (ret != NO_ERROR) {
        CLOGE("Construct Pipes fail! ret(%d)", ret);
        return ret;
    }

    /* SYNC Pipe for Dual */
    pipeId = PIPE_SYNC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeSync(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_SYNC");

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        /* DCP */
        pipeId = PIPE_DCP;
        m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_DCP");

#ifdef USE_CIP_M2M_HW
        /* CIP */
        pipeId = PIPE_CIP;
        m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_CIP");
#endif
    } else if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        /* Fusion Pipe for Dual */
        pipeId = PIPE_FUSION;
        m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeFusion(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_FUSION");
    } else  {
        CLOGE("Unknown Dual preview mode! %d", m_parameters->getDualPreviewMode());
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::startPipes(void)
{
    Mutex::Autolock lock(m_sensorStandbyLock);

    status_t ret = NO_ERROR;
    int width = 0, height = 0;
    uint32_t minfps = 0, maxfps = 0;

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_VRA]->start();
        if (ret != NO_ERROR) {
            CLOGE("VRA start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
        || m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

#ifdef USE_CIP_M2M_HW
    ret = m_pipes[PIPE_CIP]->start();
    if (ret != NO_ERROR) {
        CLOGE("CIP start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
#endif

    if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_DCP]->start();
        if (ret != NO_ERROR) {
            CLOGE("DCP start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_ISP]->start();
        if (ret != NO_ERROR) {
            CLOGE("ISP start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_pipes[PIPE_3AA]->start();
    if (ret != NO_ERROR) {
        CLOGE("3AA start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_sensorStandby == false) {
        if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
            ret = m_pipes[PIPE_FLITE]->start();
            if (ret != NO_ERROR) {
                CLOGE("FLITE start fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* Here is doing FLITE prepare(qbuf) */
            ret = m_pipes[PIPE_FLITE]->prepare();
            if (ret != NO_ERROR) {
                CLOGE("FLITE prepare fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            ret = m_pipes[PIPE_FLITE]->sensorStream(true);
        } else {
            /* Here is doing 3AA prepare(qbuf) */
            ret = m_pipes[PIPE_3AA]->prepare();
            if (ret != NO_ERROR) {
                CLOGE("3AA prepare fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            ret = m_pipes[PIPE_3AA]->sensorStream(true);
        }
    }

    if (ret != NO_ERROR) {
        CLOGE("FLITE sensorStream on fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d", ret);
        return ret;
    }

    CLOGI("Starting Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_setDeviceInfo(void)
{
    CLOGI("");

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1, nodeMcscp2 = -1, nodeMcscp5 = -1;
    int nodeVra = -1;
    int previousPipeId = -1;
    int mcscSrcPipeId = -1;
    enum HW_CONNECTION_MODE mcscInputConnection = HW_CONNECTION_MODE_NONE;
    int vraSrcPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = true;
    int nodeDcps0 = -1, nodeDcps1 = -1, nodeDcpc0 = -1, nodeDcpc1 = -1, nodeDcpc2 = -1, nodeDcpc3 = -1, nodeDcpc4 = -1;
#if defined(USE_CIP_HW) || defined(USE_CIP_M2M_HW)
    int nodeCips0 = -1, nodeCips1 = -1;
#endif /* USE_CIP_HW */

    m_initDeviceInfo(PIPE_FLITE);
    m_initDeviceInfo(PIPE_3AA);
    m_initDeviceInfo(PIPE_ISP);
    m_initDeviceInfo(PIPE_DCP);
    m_initDeviceInfo(PIPE_MCSC);
    m_initDeviceInfo(PIPE_VRA);

    node3aa = FIMC_IS_VIDEO_30S_NUM;
    node3ac = FIMC_IS_VIDEO_30C_NUM;
    node3ap = FIMC_IS_VIDEO_30P_NUM;
    nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
    nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
    nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
    nodeDcps0 = FIMC_IS_VIDEO_DCP0S_NUM;
    nodeDcps1 = FIMC_IS_VIDEO_DCP1S_NUM;
    nodeDcpc0 = FIMC_IS_VIDEO_DCP0C_NUM;
    nodeDcpc1 = FIMC_IS_VIDEO_DCP1C_NUM;
    nodeDcpc2 = FIMC_IS_VIDEO_DCP2C_NUM;
    nodeDcpc3 = FIMC_IS_VIDEO_DCP3C_NUM;
    nodeDcpc4 = FIMC_IS_VIDEO_DCP4C_NUM;
#if defined(USE_CIP_HW) || defined(USE_CIP_M2M_HW)
    nodeCips0 = FIMC_IS_VIDEO_CIP0S_NUM;
    nodeCips1 = FIMC_IS_VIDEO_CIP1S_NUM;
#endif /* USE_CIP_HW */
    nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
    nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeMcscp5 = FIMC_IS_VIDEO_M5P_NUM;
    nodeVra = FIMC_IS_VIDEO_VRA_NUM;

    /*
     * FLITE
     */
    bool flagQuickSwitchFlag = false;

#ifdef SAMSUNG_QUICK_SWITCH
    flagQuickSwitchFlag = m_parameters->getQuickSwitchFlag();
#endif

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_FLITE;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteNodenum(m_cameraId, false, flagQuickSwitchFlag);
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "FLITE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[nodeType], false, flagStreamLeader, m_flagReprocessing);

    /* Other nodes is not stream leader */
    flagStreamLeader = false;

    /* VC0 for bayer */
    nodeType = getNodeType(PIPE_VC0);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VC0;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteCaptureNodenum(m_cameraId, m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)]);
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "BAYER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)], false, flagStreamLeader, m_flagReprocessing);

#ifdef SUPPORT_DEPTH_MAP
    /* VC1 for depth */
    if (m_parameters->isDepthMapSupported()) {
        nodeType = getNodeType(PIPE_VC1);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VC1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = getDepthVcNodeNum(m_cameraId, false);
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DEPTH", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)], false, flagStreamLeader, m_flagReprocessing);
    }
#endif // SUPPORT_DEPTH_MAP

    /*
     * 3AA
     */
    previousPipeId = pipeId;
    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_VC0)], m_flagFlite3aaOTF, flagStreamLeader, m_flagReprocessing);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);


    /*
     * ISP
     */
    previousPipeId = pipeId;

    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_ISP;
    }

    /* ISPS */
    nodeType = getNodeType(PIPE_ISP);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_ISP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, flagStreamLeader, m_flagReprocessing);

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        /*
         * DCP 
         */
        previousPipeId = pipeId;

        if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M) {
            pipeId = PIPE_DCP;
        }

        /* DCPS0 */
        nodeType = getNodeType(PIPE_DCPS0);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPS0;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcps0;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPS0_MASTER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPC)], m_flagIspDcpOTF, flagStreamLeader, m_flagReprocessing);

        /* DCPC0 */
        nodeType = getNodeType(PIPE_DCPC0);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPC0;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcpc0;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPC0_MASTER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* DCPS1 */
        nodeType = getNodeType(PIPE_DCPS1);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPS1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcps1;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPS1_SLAVE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* DCPC1 */
        nodeType = getNodeType(PIPE_DCPC1);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPC1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcpc1;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPC1_SLAVE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* DCPC2 */
        nodeType = getNodeType(PIPE_DCPC2);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPC2;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcpc2;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPC2_DEPTH", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* DCPC3 */
        nodeType = getNodeType(PIPE_DCPC3);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPC3;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcpc3;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPC3_MASTER_DS", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* DCPC4 */
        nodeType = getNodeType(PIPE_DCPC4);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_DCPC4;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeDcpc4;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DCPC4_SLAVE_DS", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DCPS0)], true, flagStreamLeader, m_flagReprocessing);

#ifdef USE_CIP_M2M_HW
        previousPipeId = pipeId;
        pipeId = PIPE_CIP;
#endif

#if defined(USE_CIP_HW) || defined(USE_CIP_M2M_HW)
        /* CIPS0 */
        nodeType = getNodeType(PIPE_CIPS0);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_CIPS0;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeCips0;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "CIPS0_MASTER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_CIPS0)], true, flagStreamLeader, m_flagReprocessing);

        /* CIPS1 */
        nodeType = getNodeType(PIPE_CIPS1);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_CIPS1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeCips1;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "CIPS1_SLAVE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_CIPS1)], true, flagStreamLeader, m_flagReprocessing);
#endif /* USE_CIP_HW */
    }

    /*
     * MCSC
     */
#ifndef USE_CIP_M2M_HW
    previousPipeId = pipeId;
#endif

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        mcscSrcPipeId = PIPE_DCPC0;
        mcscInputConnection = m_flagDcpMcscOTF;
    } else {
        mcscSrcPipeId = PIPE_ISPC;
        mcscInputConnection = m_flagIspMcscOTF;
    }

    if (m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M
        || m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
    }

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(mcscSrcPipeId)], mcscInputConnection, flagStreamLeader, m_flagReprocessing);

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW_CALLBACK", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_RECORDING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        /* MCSC5 */
        nodeType = getNodeType(PIPE_MCSC5);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC5;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp5;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_DS", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)], true, flagStreamLeader, m_flagReprocessing);

        /*
         * VRA
         */
        previousPipeId = pipeId;
        vraSrcPipeId = PIPE_MCSC5;
        pipeId = PIPE_VRA;

        nodeType = getNodeType(PIPE_VRA);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VRA;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeVra;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "VRA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flagMcscVraOTF, flagStreamLeader, m_flagReprocessing);
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_initPipes(uint32_t frameRate)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;

    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int yuvWidth[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvHeight[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    int dsWidth = MAX_VRA_INPUT_WIDTH;
    int dsHeight = MAX_VRA_INPUT_HEIGHT;
    int dsFormat = m_parameters->getHwVraInputFormat();
    int yuvBufferCnt[ExynosCameraParameters::YUV_MAX] = {0};
    int stride = 0;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);
    int hwVdisformat = m_parameters->getHWVdisFormat();
    int perFramePos = 0;
    int yuvIndex = -1;
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bnsSize;
    ExynosRect bcropSize;
    ExynosRect bdsSize;
    ExynosRect tempRect;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);

    m_parameters->getPreviewBayerCropSize(&bnsSize, &bcropSize, false);
    m_parameters->getPreviewBdsSize(&bdsSize, false);

    CLOGI("MaxSensorSize %dx%d bayerFormat %x",
             maxSensorW, maxSensorH, bayerFormat);
    CLOGI("BnsSize %dx%d BcropSize %dx%d BdsSize %dx%d",
            bnsSize.w, bnsSize.h, bcropSize.w, bcropSize.h, bdsSize.w, bdsSize.h);
    CLOGI("DS Size %dx%d Format %x Buffer count %d",
            dsWidth, dsHeight, dsFormat, config->current->bufInfo.num_vra_buffers);

    for (int i = ExynosCameraParameters::YUV_0; i < ExynosCameraParameters::YUV_MAX; i++) {
        m_parameters->getYuvSize(&yuvWidth[i], &yuvHeight[i], i);
        yuvFormat[i] = m_parameters->getYuvFormat(i);
        yuvBufferCnt[i] = m_parameters->getYuvBufferCount(i);

        CLOGI("YUV[%d] Size %dx%d Format %x Buffer count %d",
                i, yuvWidth[i], yuvHeight[i], yuvFormat[i], yuvBufferCnt[i]);
    }

    /*
     * FLITE
     */
    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }

    /* setParam for Frame rate : must after setInput on Flite */
    struct v4l2_streamparm streamParam;
    memset(&streamParam, 0x0, sizeof(v4l2_streamparm));

    streamParam.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    streamParam.parm.capture.timeperframe.numerator   = 1;
    streamParam.parm.capture.timeperframe.denominator = frameRate;
    CLOGI("Set framerate (denominator=%d)", frameRate);

    ret = setParam(&streamParam, pipeId);
    if (ret != NO_ERROR) {
        CLOGE("FLITE setParam(frameRate(%d), pipeId(%d)) fail", frameRate, pipeId);
        return INVALID_OPERATION;
    }

    ret = m_setSensorSize(pipeId, hwSensorW, hwSensorH);
    if (ret != NO_ERROR) {
        CLOGE("m_setSensorSize(pipeId(%d), hwSensorW(%d), hwSensorH(%d)) fail", pipeId, hwSensorW, hwSensorH);
        return ret;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);

    /* set v4l2 buffer size */
    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_FLITE);

    /* BAYER */
    nodeType = getNodeType(PIPE_VC0);
    perFramePos = PERFRAME_BACK_VC0_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwSensorW;
    tempRect.fullH = hwSensorH;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->isDepthMapSupported()) {
        /* Depth Map Configuration */
        int depthMapW = 0, depthMapH = 0;
        int depthMapFormat = DEPTH_MAP_FORMAT;

        ret = m_parameters->getDepthMapSize(&depthMapW, &depthMapH);
        if (ret != NO_ERROR) {
            CLOGE("Failed to getDepthMapSize");
            return ret;
        }

        CLOGI("DepthMapSize %dx%d", depthMapW, depthMapH);

        tempRect.fullW = depthMapW;
        tempRect.fullH = depthMapH;
        tempRect.colorFormat = depthMapFormat;

        nodeType = getNodeType(PIPE_VC1);
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, tempRect.colorFormat);
        pipeInfo[nodeType].bufInfo.count = NUM_DEPTHMAP_BUFFERS;

        SET_CAPTURE_DEVICE_BASIC_INFO();
    }
#endif

    /* setup pipe info to FLITE pipe */
    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("FLITE setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /*
     * 3AA
     */
    pipeId = PIPE_3AA;

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA);
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_OTF) {
        /* set v4l2 buffer size */
        tempRect.fullW = 32;
        tempRect.fullH = 64;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
    } else if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        /* set v4l2 buffer size */
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
    }

    /* Set output node default info */
    SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_3AA);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    perFramePos = PERFRAME_BACK_3AC_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AC);

    /* set v4l2 buffer size */
    tempRect.fullW = bcropSize.w;
    tempRect.fullH = bcropSize.h;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    switch(m_parameters->getReprocessingBayerMode()) {
        case REPROCESSING_BAYER_MODE_PURE_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_PURE_DYNAMIC:
        case REPROCESSING_BAYER_MODE_NONE:
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
            break;
        case REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON:
        case REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC:
            if (m_parameters->isSupportZSLInput()) {
                pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
            } else {
                pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_sensor_buffers;
            }
            break;
        default:
            CLOGE("Invalid reprocessing mode(%d)", m_parameters->getReprocessingBayerMode());
    }

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    perFramePos = PERFRAME_BACK_3AP_POS;
    bayerFormat = m_parameters->getBayerFormat(PIPE_3AP);

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = bayerFormat;

    /* set v4l2 video node bytes per plane */
    pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to 3AA pipe */
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("3AA setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /*
     * ISP
     */

    /* ISPS */
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_ISP;
        nodeType = getNodeType(PIPE_ISP);
        bayerFormat = m_parameters->getBayerFormat(PIPE_ISP);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_ISP);
    }

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    perFramePos = PERFRAME_BACK_ISPC_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = hwVdisformat;

    pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = NUM_HW_DIS_BUFFERS;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    perFramePos = PERFRAME_BACK_ISPP_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = hwVdisformat;

    pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = NUM_HW_DIS_BUFFERS;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M
        || m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("ISP setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW) {
        /* DCP M2M */
        if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M) {
            pipeId = PIPE_DCP;
        }

        /* DCPS0 */
        nodeType = getNodeType(PIPE_DCPS0);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_HW_DIS_BUFFERS;

        /* Set capture node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_DCP);

        /* DCPC0 */
        nodeType = getNodeType(PIPE_DCPC0);
        perFramePos = PERFRAME_BACK_DCPC0_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;


        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* DCPS1 */
        nodeType = getNodeType(PIPE_DCPS1);
        perFramePos = PERFRAME_BACK_DCPS1_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_HW_DIS_BUFFERS;

        /* Set capture node default info */
        SET_OUTPUT_DEVICE_CAPTURE_NODE_INFO();

        /* DCPC1 */
        nodeType = getNodeType(PIPE_DCPC1);
        perFramePos = PERFRAME_BACK_DCPC1_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* DCPC2 */
        nodeType = getNodeType(PIPE_DCPC2);
        perFramePos = PERFRAME_BACK_DCPC2_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = 640;
        tempRect.fullH = 480;
        tempRect.colorFormat = V4L2_PIX_FMT_Z16;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* DCPC3 */
        nodeType = getNodeType(PIPE_DCPC3);
        perFramePos = PERFRAME_BACK_DCPC3_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV12M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* DCPC4 */
        nodeType = getNodeType(PIPE_DCPC4);
        perFramePos = PERFRAME_BACK_DCPC4_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV12M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

#ifdef USE_CIP_M2M_HW
        /* setup pipe info to DCP pipe */
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("DCP setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        pipeId = PIPE_CIP;
#endif

#if defined(USE_CIP_HW) || defined(USE_CIP_M2M_HW)
        /* CIPS0 */
        nodeType = getNodeType(PIPE_CIPS0);
        perFramePos = PERFRAME_BACK_CIPS0_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_16PX_ALIGN);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* CIPS1 */
        nodeType = getNodeType(PIPE_CIPS1);
        perFramePos = PERFRAME_BACK_CIPS1_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, CAMERA_TPU_CHUNK_ALIGN_W);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
#endif /* USE_CIP_HW */

        /* setup pipe info to DCP pipe */
        if (m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
            ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
            if (ret != NO_ERROR) {
                CLOGE("DCP setupPipe fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    }

    /*
     * MCSC
     */

    /* MCSC */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
        || m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
        nodeType = getNodeType(PIPE_MCSC);

        /* set v4l2 buffer size */
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = NUM_DCP_BUFFERS;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_MCSC);
    }

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    perFramePos = PERFRAME_BACK_SCP_POS;
    yuvIndex = ExynosCameraParameters::YUV_0;
    m_parameters->setYuvOutPortId(PIPE_MCSC0, yuvIndex);

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[yuvIndex];
    tempRect.fullH = yuvHeight[yuvIndex];
    tempRect.colorFormat = yuvFormat[yuvIndex];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[yuvIndex];
#endif

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = yuvBufferCnt[yuvIndex];

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    perFramePos = PERFRAME_BACK_MCSC1_POS;
    yuvIndex = ExynosCameraParameters::YUV_1;
    m_parameters->setYuvOutPortId(PIPE_MCSC1, yuvIndex);

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[yuvIndex];
    tempRect.fullH = yuvHeight[yuvIndex];
    tempRect.colorFormat = yuvFormat[yuvIndex];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
    pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[yuvIndex];
#endif

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = yuvBufferCnt[yuvIndex];

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2);
    perFramePos = PERFRAME_BACK_MCSC2_POS;
    yuvIndex = ExynosCameraParameters::YUV_2;
    m_parameters->setYuvOutPortId(PIPE_MCSC2, yuvIndex);

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[yuvIndex];
    tempRect.fullH = yuvHeight[yuvIndex];
    tempRect.colorFormat = yuvFormat[yuvIndex];

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = yuvBufferCnt[yuvIndex];

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC5 */
    nodeType = getNodeType(PIPE_MCSC5);
    perFramePos = PERFRAME_BACK_MCSC5_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = dsWidth;
    tempRect.fullH = dsHeight;
    tempRect.colorFormat = dsFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("MCSC setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    /* VRA */
    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_VRA;
        nodeType = getNodeType(PIPE_VRA);

        /* set v4l2 buffer size */
        tempRect.fullW = dsWidth;
        tempRect.fullH = dsHeight;
        tempRect.colorFormat = dsFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_VRA);
    }

    ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
    if (ret != NO_ERROR) {
        CLOGE("VRA setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    camera2_node_group node_group_info_flite;
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_dcp;
#ifdef USE_CIP_M2M_HW
    camera2_node_group node_group_info_cip;
#endif
    camera2_node_group node_group_info_mcsc;
    camera2_node_group node_group_info_vra;
    camera2_node_group *node_group_info_temp;

    float zoomRatio = m_parameters->getZoomRatio();
    int pipeId = -1;
    int nodePipeId = -1;
    uint32_t perframePosition = 0;

    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvIndex = -1;

    for (int i = ExynosCameraParameters::YUV_0; i < ExynosCameraParameters::YUV_MAX; i++) {
        yuvFormat[i] = m_parameters->getYuvFormat(i);
    }

    memset(&node_group_info_flite, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_dcp, 0x0, sizeof(camera2_node_group));
#ifdef USE_CIP_M2M_HW
    memset(&node_group_info_cip, 0x0, sizeof(camera2_node_group));
#endif
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_vra, 0x0, sizeof(camera2_node_group));

    if (m_flagFlite3aaOTF != HW_CONNECTION_MODE_M2M) {
        /* 3AA */
        pipeId = PIPE_3AA;
        perframePosition = 0;

        node_group_info_temp = &node_group_info_flite;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getBayerFormat(pipeId);

        nodePipeId = PIPE_VC0;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;

#ifdef SUPPORT_DEPTH_MAP
        /* VC1 for depth */
        if (m_parameters->getUseDepthMap() == true) {
            nodePipeId = PIPE_VC1;
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = DEPTH_MAP_FORMAT;
            perframePosition++;
        }
#endif // SUPPORT_DEPTH_MAP

        nodePipeId = PIPE_3AC;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;

        nodePipeId = PIPE_3AP;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);

        perframePosition++;
    } else {
        /* FLITE */
        pipeId = PIPE_FLITE;
        perframePosition = 0;

        node_group_info_temp = &node_group_info_flite;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getBayerFormat(pipeId);

        nodePipeId = PIPE_VC0;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;

#ifdef SUPPORT_DEPTH_MAP
        /* VC1 for depth */
        if (m_parameters->getUseDepthMap() == true) {
            nodePipeId = PIPE_VC1;
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
            perframePosition++;
        }
#endif // SUPPORT_DEPTH_MAP

        /* 3AA */
        pipeId = PIPE_3AA;
        perframePosition = 0;

        node_group_info_temp = &node_group_info_3aa;

        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getBayerFormat(pipeId);

        nodePipeId = PIPE_3AC;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;

        nodePipeId = PIPE_3AP;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;
    }

    /* ISP */
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_ISP;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_isp;
        node_group_info_temp->leader.request = 1;
    }

    nodePipeId = PIPE_ISPC;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHWVdisFormat();
    perframePosition++;

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW
        && m_parameters->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC) {
        /* DCP */
        if (m_flagIspDcpOTF != HW_CONNECTION_MODE_OTF) {
            if (m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M) {
                pipeId = PIPE_DCP;
            }
            perframePosition = 0;
            node_group_info_temp = &node_group_info_dcp;
            node_group_info_temp->leader.request = 1;
        } else {
            nodePipeId = PIPE_DCPS0;
            if (m_request[INDEX(nodePipeId)] == true) {
                node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
                node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
                perframePosition++;
            }
        }

        nodePipeId = PIPE_DCPS1;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_DCPC0;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_DCPC1;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_DCPC2;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_DCPC3;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_DCPC4;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

#ifdef USE_CIP_M2M_HW
        pipeId = PIPE_CIP;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_cip;
#endif

#if defined(USE_CIP_HW) || defined(USE_CIP_M2M_HW)
        nodePipeId = PIPE_CIPS0;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        nodePipeId = PIPE_CIPS1;
        if (m_request[nodePipeId] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }
#endif
    }

    /* MCSC */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
        || m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_mcsc;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getHWVdisFormat();
    }

    nodePipeId = PIPE_MCSC0;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_0;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    nodePipeId = PIPE_MCSC1;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_1;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    nodePipeId = PIPE_MCSC2;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_2;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    nodePipeId = PIPE_MCSC5;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwVraInputFormat();
    perframePosition++;

    /* VRA */
    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_VRA;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_vra;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getHwVraInputFormat();
    }

    frame->setZoomRatio(zoomRatio);

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_FLITE,
                m_parameters,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);

        updateNodeGroupInfo(
                PIPE_3AA,
                m_parameters,
                &node_group_info_3aa);
        frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_3AA);
    } else {
        updateNodeGroupInfo(
                PIPE_3AA,
                m_parameters,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);
    }

    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_ISP,
                m_parameters,
                &node_group_info_isp);
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    }

    if (m_parameters->getDualPreviewMode() == DUAL_PREVIEW_MODE_HW
        && m_parameters->getDualOperationMode() == DUAL_OPERATION_MODE_SYNC
        && m_flagIspDcpOTF != HW_CONNECTION_MODE_OTF) {
        updateNodeGroupInfo(
                PIPE_DCP,
                m_parameters,
                &node_group_info_dcp);
        frame->storeNodeGroupInfo(&node_group_info_dcp, PERFRAME_INFO_DCP);
    }

#ifdef USE_CIP_M2M_HW
    updateNodeGroupInfo(
            PIPE_CIP,
            m_parameters,
            &node_group_info_cip);
    frame->storeNodeGroupInfo(&node_group_info_cip, PERFRAME_INFO_CIP);
#endif

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
        || m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_MCSC,
                m_parameters,
                &node_group_info_mcsc);
        frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_MCSC);
    }

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_VRA,
                m_parameters,
                &node_group_info_vra);
        frame->storeNodeGroupInfo(&node_group_info_vra, PERFRAME_INFO_VRA);
    }

    return NO_ERROR;
}

void ExynosCameraFrameFactoryPreviewDual::m_init(void)
{
    m_flagReprocessing = false;
}
}; /* namespace android */
