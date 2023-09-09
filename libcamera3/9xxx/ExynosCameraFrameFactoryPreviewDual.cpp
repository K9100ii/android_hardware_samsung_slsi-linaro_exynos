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

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::stopPipes(void)
{
    Mutex::Autolock lock(m_sensorStandbyLock);

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

#ifdef SUPPORT_GMV
    if (m_pipes[PIPE_GMV]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GMV]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GMV stopThread fail, ret(%d)", ret);
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[PIPE_ISP]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_ISP]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
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

#ifdef SUPPORT_HFD
    if (m_parameters->getHfdMode() == true
        && m_pipes[PIPE_HFD]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_HFD]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_HFD stopThread fail, ret(%d)", ret);
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

    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
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

#ifdef SAMSUNG_TN_FEATURE
    if (m_pipes[PIPE_PP_UNI]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_PP_UNI]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_PP_UNI stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[PIPE_PP_UNI2]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_PP_UNI2]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_PP_UNI2 stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }


    if (m_pipes[PIPE_PP_UNI3]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_PP_UNI3]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_PP_UNI3 stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[PIPE_GSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_sensorStandby == true) {
        CLOGI("already sensor standby(%d)", m_sensorStandby);
    } else {
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
    }

    /* default settting */
    m_sensorStandby = true;
    m_needSensorStreamOn = true;

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

#ifdef SUPPORT_GMV
    if (m_parameters->getGmvMode() == true) {
        ret = m_pipes[PIPE_GMV]->stop();
        if (ret != NO_ERROR) {
            CLOGE("GMV stop fail, ret(%d)", ret);
            funcRet |= ret;
        }
    }
#endif

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

#ifdef SUPPORT_HFD
    if (m_parameters->getHfdMode() == true) {
        ret = m_pipes[PIPE_HFD]->stop();
        if (ret != NO_ERROR) {
            CLOGE("HFD stop fail, ret(%d)", ret);
            funcRet |= ret;
        }
    }
#endif

#if defined(SUPPORT_HW_GDC)
    ret = stopThreadAndWait(PIPE_GDC);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GDC stopThreadAndWait fail, ret(%d)", ret);
        funcRet |= ret;
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

    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        if (m_pipes[PIPE_FUSION] != NULL) {
            ret = m_pipes[PIPE_FUSION]->stop();
            if (ret < 0) {
                CLOGE("m_pipes[PIPE_FUSION]->stop() fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    ret = m_pipes[PIPE_PP_UNI]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_PP_UNI stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    ret = m_pipes[PIPE_PP_UNI2]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_PP_UNI2 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }


    ret = m_pipes[PIPE_PP_UNI3]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_PP_UNI3 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }
#endif

    ret = m_pipes[PIPE_GSC]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        funcRet |= ret;
    }

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo *cameraInfo = m_parameters->getSsrmCameraInfo();

    cameraInfo->operation = SSRM_CAMERA_INFO_CLEAR;
    cameraInfo->cameraId = m_cameraId;
    storeSsrmCameraInfo(cameraInfo);
#endif

    CLOGI("Stopping  Success!");

    return funcRet;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameFactoryPreviewDual::createNewFrame(uint32_t frameCount, __unused bool useJpegFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrameSP_sptr_t frame = NULL;

    int requestEntityCount = 0;
    int pipeId = -1;

    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }

    frame = m_frameMgr->createFrame(m_configurations, frameCount, FRAME_TYPE_PREVIEW);

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return NULL;
    }

    m_setBaseInfoToFrame(frame);

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
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
    requestEntityCount++;

#ifdef SUPPORT_GMV
    if (m_request[PIPE_GMV] == true) {
        pipeId = PIPE_GMV;
        newEntity[PIPE_GMV] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }
#endif

    /* set ISP pipe to linkageList */
    pipeId = PIPE_ISP;
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M
        && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set MCSC pipe to linkageList */
    pipeId = PIPE_MCSC;
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
            && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set Sync pipe to linkageList */
    pipeId = PIPE_SYNC;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set Fusion pipe to linkageList */
    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        pipeId = PIPE_FUSION;
        if (m_request[pipeId] == true) {
            newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
            frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
            requestEntityCount++;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    /* set PostProcessing pipe to linkageList */
    pipeId = PIPE_PP_UNI;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    pipeId = PIPE_PP_UNI2;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    pipeId = PIPE_PP_UNI3;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }
#endif

    /* set GDC pipe to linkageList */
    if (m_configurations->isSupportedFunction(SUPPORTED_FUNCTION_GDC) == true) {
        pipeId = PIPE_GDC;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set VRA pipe to linkageList */
    if (((m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)
        && m_request[PIPE_MCSC5] == true && m_request[PIPE_VRA] == true) ||
        ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M)
        && m_request[PIPE_3AF] == true && m_request[PIPE_VRA] == true)) {
        pipeId = PIPE_VRA;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;

#ifdef SUPPORT_HFD
        if (m_parameters->getHfdMode() == true
            && m_request[PIPE_HFD] == true) {
            pipeId = PIPE_HFD;
            newEntity[PIPE_HFD] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
#endif
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
    frame->setParameters(m_parameters);
    frame->setCameraId(m_cameraId);

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
    m_useBDSOff = m_parameters->isUse3aaBDSOff() && m_parameters->isVideoStreamExist();
	m_flag3aaVraOTF = m_parameters->getHwConnectionMode(PIPE_3AA, PIPE_VRA);
	if (m_flag3aaVraOTF == HW_CONNECTION_MODE_NONE)
    	m_flagMcscVraOTF = m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA);

    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();
    CLOGD("m_useBDSOff: %d", m_useBDSOff);

    m_request[PIPE_3AP] = (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M);
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        m_request[PIPE_ISPC] = true;
    }

    m_request[PIPE_3AF] = false;
    m_request[PIPE_3AG] = false;

    /* FLITE ~ MCSC */
    ret = m_setDeviceInfo();
    if (ret != NO_ERROR) {
        CLOGE("m_setDeviceInfo() fail, ret(%d)", ret);
        return ret;
    }

#ifdef SAMSUNG_TN_FEATURE
    /* PostProcessing for Preview */
    m_nodeNums[INDEX(PIPE_PP_UNI)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
    m_nodeNums[INDEX(PIPE_PP_UNI2)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
    m_nodeNums[INDEX(PIPE_PP_UNI3)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
    CLOGD("UNI(%d), UNI2(%d) UNI3(%d)", m_nodeNums[PIPE_PP_UNI][0], m_nodeNums[PIPE_PP_UNI2][0], m_nodeNums[PIPE_PP_UNI3][0]);
#endif

#if defined(SUPPORT_HW_GDC)
    /* GDC */
    m_nodeNums[INDEX(PIPE_GDC)][OUTPUT_NODE] = FIMC_IS_VIDEO_GDC_NUM;
#endif

#ifdef SUPPORT_HFD
    /* HFD */
    m_nodeNums[INDEX(PIPE_HFD)][OUTPUT_NODE] = -1;
#endif
#ifdef SUPPORT_GMV
    /* GMV */
    m_nodeNums[INDEX(PIPE_GMV)][OUTPUT_NODE] = -1;
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
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeSync(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_SYNC");

    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        /* Fusion Pipe for Dual */
        pipeId = PIPE_FUSION;

#ifdef SAMSUNG_TN_FEATURE
        ExynosCameraPipeFusion *fusionPipe = new ExynosCameraPipeFusion(
                    m_cameraId,
                    m_configurations,
                    m_parameters,
                    m_flagReprocessing,
                    m_nodeNums[pipeId]);
        m_pipes[pipeId] = (ExynosCameraPipe *)fusionPipe;
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_FUSION");

        if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
            ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionZoomPreviewWrapper>::getInstance();
            fusionPipe->setFusionWrapper(fusionWrapper);
#endif
        } else if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
            ExynosCameraFusionWrapper *bokehWrapper = ExynosCameraSingleton<ExynosCameraBokehPreviewWrapper>::getInstance();
            fusionPipe->setBokehWrapper(bokehWrapper);
#endif
        }
#else
        ExynosCameraPipePlugIn *fusionPipe = new ExynosCameraPipePlugIn(
            m_cameraId,
            m_configurations,
            m_parameters,
            m_flagReprocessing,
            m_nodeNums[pipeId]);
        m_pipes[pipeId] = (ExynosCameraPipe *)fusionPipe;
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_FUSION");
#endif
    } else  {
        CLOGE("Unknown Dual preview mode! %d", m_configurations->getDualPreviewMode());
        return INVALID_OPERATION;
    }

    CLOGI("pipe ids for preview dual");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("m_pipes[%d] : PipeId(%d)" , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::startPipes(void)
{
    Mutex::Autolock lock(m_sensorStandbyLock);

    status_t ret = NO_ERROR;
    __unused int width = 0, height = 0;
    __unused uint32_t minfps = 0, maxfps = 0;

#ifdef SUPPORT_HFD
    if (m_parameters->getHfdMode() == true) {
        ret = m_pipes[PIPE_HFD]->start();
        if (ret != NO_ERROR) {
            CLOGE("HFD start fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }
#endif

    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        if (m_pipes[PIPE_FUSION] != NULL) {
            ret = m_pipes[PIPE_FUSION]->start();
            if (ret < 0) {
                CLOGE("PIPE_FUSION start fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }

    if (m_pipes[PIPE_SYNC] != NULL &&
            m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        ret = m_pipes[PIPE_SYNC]->start();
        if (ret != NO_ERROR) {
            CLOGE("SYNC start fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }

    if ((m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) ||
		(m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M)) {
        ret = m_pipes[PIPE_VRA]->start();
        if (ret != NO_ERROR) {
            CLOGE("VRA start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[PIPE_MCSC]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
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

#ifdef SUPPORT_GMV
    if (m_parameters->getGmvMode() == true) {
        ret = m_pipes[PIPE_GMV]->start();
        if (ret != NO_ERROR) {
            CLOGE("GMV start fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }
#endif

    ret = m_pipes[PIPE_3AA]->start();
    if (ret != NO_ERROR) {
        CLOGE("3AA start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    int pipeId = PIPE_3AA;
    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_FLITE;
        ret = m_pipes[PIPE_FLITE]->start();
        if (ret != NO_ERROR) {
            CLOGE("FLITE start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_needSensorStreamOn == true) {
        ret = m_pipes[pipeId]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("pipeId %d prepare fail, ret(%d)", pipeId, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        ret = m_pipes[pipeId]->sensorStream(true);
        if (ret != NO_ERROR) {
            CLOGE("pipeId %d sensorStream on fail, ret(%d)", pipeId, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* change state to streamOn(standby off) */
        m_sensorStandby = false;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d", ret);
        return ret;
    }

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo *cameraInfo = m_parameters->getSsrmCameraInfo();

    if (m_configurations->getMode(CONFIGURATION_RECORDING_MODE) == true) {
        m_configurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&width, (uint32_t *)&height);
    } else {
        m_configurations->getSize(CONFIGURATION_MAX_YUV_SIZE, (uint32_t *)&width, (uint32_t *)&height);
    }
    m_configurations->getPreviewFpsRange(&minfps, &maxfps);

    cameraInfo->operation = SSRM_CAMERA_INFO_SET;
    cameraInfo->cameraId = m_cameraId;
    cameraInfo->width = width;
    cameraInfo->height = height;
    cameraInfo->minFPS = (int)minfps;
    cameraInfo->maxFPS = (int)maxfps;

    storeSsrmCameraInfo(cameraInfo);
#endif

    CLOGI("Starting Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_setDeviceInfo(void)
{
    CLOGI("");

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1, node3af = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1, nodeMcscp2 = -1, nodeMcscpDs = -1;
    int nodeVra = -1;
    int previousPipeId = -1;
    int mcscSrcPipeId = -1;
    enum HW_CONNECTION_MODE mcscInputConnection = HW_CONNECTION_MODE_NONE;
    int vraSrcPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = true;

    m_initDeviceInfo(PIPE_FLITE);
    m_initDeviceInfo(PIPE_3AA);
    m_initDeviceInfo(PIPE_ISP);
    m_initDeviceInfo(PIPE_MCSC);
    m_initDeviceInfo(PIPE_VRA);

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true
        && (getCameraId() == CAMERA_ID_BACK_1 || getCameraId() == CAMERA_ID_FRONT_1)) {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        node3af = FIMC_IS_VIDEO_31F_NUM;
    } else
#endif
    {
#if defined(FRONT_CAMERA_3AA_NUM)
        if(getCameraId() == CAMERA_ID_FRONT_0) {
            if(FRONT_CAMERA_3AA_NUM == FIMC_IS_VIDEO_30S_NUM) {
                node3aa = FIMC_IS_VIDEO_30S_NUM;
                node3ac = FIMC_IS_VIDEO_30C_NUM;
                node3ap = FIMC_IS_VIDEO_30P_NUM;
                node3af = FIMC_IS_VIDEO_30F_NUM;
            } else {
                node3aa = FIMC_IS_VIDEO_31S_NUM;
                node3ac = FIMC_IS_VIDEO_31C_NUM;
                node3ap = FIMC_IS_VIDEO_31P_NUM;
                node3af = FIMC_IS_VIDEO_31F_NUM;
            }
        } else
#endif
        {
            /* default single path */
            node3aa = FIMC_IS_VIDEO_30S_NUM;
            node3ac = FIMC_IS_VIDEO_30C_NUM;
            node3ap = FIMC_IS_VIDEO_30P_NUM;
            node3af = FIMC_IS_VIDEO_30F_NUM;
        }
    }

    if (!m_useBDSOff) {
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
    } else {
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
    }

    nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeVra = FIMC_IS_VIDEO_VRA_NUM;

    switch (m_parameters->getNumOfMcscOutputPorts()) {
    case 5:
        nodeMcscpDs = FIMC_IS_VIDEO_M5P_NUM;
        break;
    case 3:
        nodeMcscpDs = FIMC_IS_VIDEO_M3P_NUM;
        break;
    default:
        CLOGE("invalid output port(%d)", m_parameters->getNumOfMcscOutputPorts());
        break;
    }

    /*
     * FLITE
     */
    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_FLITE;
    } else {
        pipeId = PIPE_3AA;
    }

    /* FLITE */
    nodeType = getNodeType(PIPE_FLITE);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_FLITE;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteNodenum(m_cameraId);
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
        m_deviceInfo[pipeId].nodeNum[nodeType] = getDepthVcNodeNum(m_cameraId);
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
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_VC0)],
                m_flagFlite3aaOTF,
                flagStreamLeader,
                m_flagReprocessing);

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
         /* 3AF */
         nodeType = getNodeType(PIPE_3AF);
         m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AF;
         m_deviceInfo[pipeId].nodeNum[nodeType] = node3af;
         m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
         strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_FD", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
         m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, flagStreamLeader, m_flagReprocessing);

         /*
          * VRA
          */
         previousPipeId = pipeId;
         vraSrcPipeId = PIPE_3AF;

         nodeType = getNodeType(PIPE_VRA);
         m_deviceInfo[PIPE_VRA].pipeId[nodeType]  = PIPE_VRA;
         m_deviceInfo[PIPE_VRA].nodeNum[nodeType] = nodeVra;
         m_deviceInfo[PIPE_VRA].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
         strncpy(m_deviceInfo[PIPE_VRA].nodeName[nodeType], "VRA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
         /* Workaround for Driver limitation : Due to driver limitation, VRA should be in different stream to connect the VRA to 3AA */
         m_sensorIds[PIPE_VRA][nodeType] = m_getSensorId(
                  m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flag3aaVraOTF, true, true);
    }

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
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP)],
                m_flag3aaIspOTF,
                flagStreamLeader,
                m_flagReprocessing);

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    /*
     * MCSC
     */
    previousPipeId = pipeId;
    mcscSrcPipeId = PIPE_ISPC;
    mcscInputConnection = m_flagIspMcscOTF;

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
    }

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[previousPipeId].nodeNum[getNodeType(mcscSrcPipeId)],
                mcscInputConnection,
                flagStreamLeader,
                m_flagReprocessing);

    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_PREVIEW_CALLBACK", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_RECORDING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(
                m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)],
                true,
                flagStreamLeader,
                m_flagReprocessing);

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        /* MCSC5 */
        nodeType = getNodeType(PIPE_MCSC5);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC5;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscpDs;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_DS", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(
                    m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC)],
                    true,
                    flagStreamLeader,
                    m_flagReprocessing);

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
        m_sensorIds[pipeId][nodeType] = m_getSensorId(
                    m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)],
                    m_flagMcscVraOTF,
                    flagStreamLeader,
                    m_flagReprocessing);
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

    uint32_t maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    uint32_t yuvWidth[ExynosCameraParameters::YUV_MAX] = {0};
    uint32_t yuvHeight[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    camera_pixel_size yuvPixelSize[ExynosCameraParameters::YUV_MAX] = {CAMERA_PIXEL_SIZE_8BIT};
    int dsWidth = MAX_VRA_INPUT_WIDTH;
    int dsHeight = MAX_VRA_INPUT_HEIGHT;
    int dsFormat = m_parameters->getHwVraInputFormat();
    int yuvBufferCnt[ExynosCameraParameters::YUV_MAX] = {0};
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA);
    int hwVdisformat = m_parameters->getHWVdisFormat();
    int perFramePos = 0;
    int yuvIndex = -1;
    struct ExynosConfigInfo *config = m_configurations->getConfig();
    ExynosRect bnsSize;
    ExynosRect bcropSize;
    ExynosRect bdsSize;
    ExynosRect tempRect;
    int ispSrcW, ispSrcH;

    /* default settting */
    m_sensorStandby = true;
    m_needSensorStreamOn = true;

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getSize(HW_INFO_MAX_SENSOR_SIZE, &maxSensorW, &maxSensorH);
    m_parameters->getSize(HW_INFO_HW_SENSOR_SIZE, &hwSensorW, &hwSensorH);

    m_parameters->getPreviewBayerCropSize(&bnsSize, &bcropSize, false);
    m_parameters->getPreviewBdsSize(&bdsSize, false);

    CLOGI("MaxSensorSize %dx%d bayerFormat %x",
             maxSensorW, maxSensorH, bayerFormat);
    CLOGI("BnsSize %dx%d BcropSize %dx%d BdsSize %dx%d",
            bnsSize.w, bnsSize.h, bcropSize.w, bcropSize.h, bdsSize.w, bdsSize.h);
    CLOGI("DS Size %dx%d Format %x Buffer count %d",
            dsWidth, dsHeight, dsFormat, config->current->bufInfo.num_vra_buffers);

    for (int i = ExynosCameraParameters::YUV_0; i < ExynosCameraParameters::YUV_MAX; i++) {
        m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&yuvWidth[i], (uint32_t *)&yuvHeight[i], i);
        yuvFormat[i] = m_configurations->getYuvFormat(i);
        yuvPixelSize[i] = m_configurations->getYuvPixelSize(i);
        yuvBufferCnt[i] = m_configurations->getYuvBufferCount(i);

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
            && m_parameters->isPreviewPortId(i) == true
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;

            m_parameters->getFusionSize(yuvWidth[i], yuvHeight[i],
                    &fusionSrcRect, &fusionDstRect,
                    DUAL_SOLUTION_MARGIN_VALUE_30);

            yuvWidth[i] = fusionSrcRect.w;
            yuvHeight[i] = fusionSrcRect.h;
        }
#endif

        CLOGI("YUV[%d] Size %dx%d Format %x PixelSizeNum %d Buffer count %d",
                i, yuvWidth[i], yuvHeight[i], yuvFormat[i], yuvPixelSize[i], yuvBufferCnt[i]);
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

    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
        /* 3AF */
        nodeType = getNodeType(PIPE_3AF);
        perFramePos = 0; //it is dummy info
        // TODO: need to remove perFramePos from the SET_CAPTURE_DEVICE_BASIC_INFO

        /* set v4l2 buffer size */
        tempRect.fullW = MAX_VRA_INPUT_WIDTH;
        tempRect.fullH = MAX_VRA_INPUT_HEIGHT;
        tempRect.colorFormat = m_parameters->getHW3AFdFormat();

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
    }

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

    if (!m_useBDSOff) {
        ispSrcW = bdsSize.w;
        ispSrcH = bdsSize.h;
    } else {
        ispSrcW = bcropSize.w;
        ispSrcH = bcropSize.h;
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
        tempRect.fullW = ispSrcW;
        tempRect.fullH = ispSrcH;
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
    tempRect.fullW = ispSrcW;
    tempRect.fullH = ispSrcH;
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
    tempRect.fullW = ispSrcW;
    tempRect.fullH = ispSrcH;
    tempRect.colorFormat = hwVdisformat;

    pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = NUM_HW_DIS_BUFFERS;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
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

    /*
     * MCSC
     */

    /* MCSC */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
        nodeType = getNodeType(PIPE_MCSC);

        /* set v4l2 buffer size */
        tempRect.fullW = ispSrcW;
        tempRect.fullH = ispSrcH;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_8BIT;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

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

    /* set v4l2 format pixel size */
    pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
        && m_parameters->isPreviewPortId(yuvIndex)) {
        pipeInfo[nodeType].bytesPerPlane[0] = 0;
    } else
#endif
    {
        pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[yuvIndex];
    }
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

    /* set v4l2 format pixel size */
    pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

    /* set v4l2 video node bytes per plane */
#ifdef USE_BUFFER_WITH_STRIDE
    /* to use stride for preview buffer, set the bytesPerPlane */
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
        && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false
        && m_parameters->isPreviewPortId(yuvIndex)) {
        pipeInfo[nodeType].bytesPerPlane[0] = 0;
    } else
#endif
    {
        pipeInfo[nodeType].bytesPerPlane[0] = yuvWidth[yuvIndex];
    }
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

    /* set v4l2 format pixel size */
    pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

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
        CLOGE("MCSC/VRA setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* VRA */
    if ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) &&
        (m_flagMcscVraOTF != HW_CONNECTION_MODE_M2M)) {
        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        pipeId = PIPE_VRA;
        nodeType = getNodeType(PIPE_VRA);

        /* set v4l2 buffer size */
        tempRect.fullW = MAX_VRA_INPUT_WIDTH;
        tempRect.fullH = MAX_VRA_INPUT_HEIGHT;
        tempRect.colorFormat = dsFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_VRA);

        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("VRA setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

#ifdef USE_DUAL_CAMERA
    if (m_configurations->getDualPreviewMode() == DUAL_PREVIEW_MODE_SW) {
        if (m_pipes[PIPE_FUSION] != NULL) {
            Map_t map;

            /* Fusion */
            ret = m_pipes[PIPE_FUSION]->setupPipe(&map);
            if(ret != NO_ERROR) {
                CLOGE("Fusion setupPipe fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreviewDual::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    camera2_node_group node_group_info_flite;
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group node_group_info_vra;
    camera2_node_group *node_group_info_temp;

    int pipeId = -1;
    int nodePipeId = -1;
    uint32_t perframePosition = 0;

    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvIndex = -1;

    for (int i = ExynosCameraParameters::YUV_0; i < ExynosCameraParameters::YUV_MAX; i++) {
        yuvFormat[i] = m_configurations->getYuvFormat(i);
    }

    memset(&node_group_info_flite, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
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
        if (m_configurations->getMode(CONFIGURATION_DEPTH_MAP_MODE) == true) {
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
        if (m_configurations->getMode(CONFIGURATION_DEPTH_MAP_MODE) == true) {
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

    /* VRA */
    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
        nodePipeId = PIPE_3AF;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwVraInputFormat();
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
    node_group_info_temp->capture[perframePosition].vid = \
                m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHWVdisFormat();
    perframePosition++;

    /* MCSC */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC;
        perframePosition = 0;
        node_group_info_temp = &node_group_info_mcsc;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getHWVdisFormat();
    }

    nodePipeId = PIPE_MCSC0;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = \
                m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_0;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    nodePipeId = PIPE_MCSC1;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = \
                m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_1;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    nodePipeId = PIPE_MCSC2;
    node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
    node_group_info_temp->capture[perframePosition].vid = \
                m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
    yuvIndex = ExynosCameraParameters::YUV_2;
    node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
    perframePosition++;

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        nodePipeId = PIPE_MCSC5;
        node_group_info_temp->capture[perframePosition].request = m_request[nodePipeId];
        node_group_info_temp->capture[perframePosition].vid = \
                    m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwVraInputFormat();
        perframePosition++;
    }


    if ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) ||
        (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)) {
        /* 3AA to VRA is a sideway path*/
        node_group_info_vra.leader.request = 1;
        node_group_info_vra.leader.pixelformat = m_parameters->getHwVraInputFormat();
    }

    if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_FLITE,
                frame,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);

        updateNodeGroupInfo(
                PIPE_3AA,
                frame,
                &node_group_info_3aa);
        frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_3AA);
    } else {
        updateNodeGroupInfo(
                PIPE_3AA,
                frame,
                &node_group_info_flite);
        frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);
    }

    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_ISP,
                frame,
                &node_group_info_isp);
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_ISP);
    }

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_MCSC,
                frame,
                &node_group_info_mcsc);
        frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_MCSC);
    }

    if ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) ||
            (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)) {
        updateNodeGroupInfo(
                PIPE_VRA,
                frame,
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
