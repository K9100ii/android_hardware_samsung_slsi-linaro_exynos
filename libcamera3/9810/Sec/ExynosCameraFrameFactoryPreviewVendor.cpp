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
#define LOG_TAG "ExynosCameraFrameFactoryPreviewSec"
#include <log/log.h>

#include "ExynosCameraFrameFactoryPreview.h"

namespace android {

ExynosCameraFrameSP_sptr_t ExynosCameraFrameFactoryPreview::createNewFrame(uint32_t frameCount, __unused bool useJpegFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrameSP_sptr_t frame = NULL;

    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = -1;

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
    parentPipeId = pipeId;

    /* set ISP pipe to linkageList */
    pipeId = PIPE_ISP;
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M
        && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
        parentPipeId = pipeId;
    }

    /* set MCSC pipe to linkageList */
    pipeId = PIPE_MCSC;
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
        && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

#ifdef SAMSUNG_TN_FEATURE
    /* set PostProcessing pipe to linkageList */
    pipeId = PIPE_PP_UNI;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    pipeId = PIPE_PP_UNI2;
    if (m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }
#endif
	
    /* set GDC pipe to linkageList */
    if (m_parameters->getGDCEnabledMode() == true && m_request[PIPE_GDC] == true) {
        pipeId = PIPE_GDC;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }

    /* set VRA pipe to linkageList */
    if (m_flagMcscVraOTF == false
        && m_request[PIPE_MCSC5] == true
        && m_request[PIPE_VRA] == true) {
        pipeId = PIPE_VRA;
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;

        if (m_request[PIPE_HFD] == true) {
            pipeId = PIPE_HFD;
            newEntity[PIPE_HFD] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[pipeId]);
            requestEntityCount++;
        }
    }

#ifdef USE_DUAL_CAMERA
    /* set Sync pipe to linkageList */
    pipeId = PIPE_SYNC;
    if (m_parameters->getDualMode() == true
        && m_request[pipeId] == true) {
        newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[pipeId]);
        requestEntityCount++;
    }
#endif

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

status_t ExynosCameraFrameFactoryPreview::m_setupConfig()
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_flagFlite3aaOTF = m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
    m_flag3aaIspOTF = m_parameters->getHwConnectionMode(PIPE_3AA, PIPE_ISP);
    m_flagIspMcscOTF = m_parameters->getHwConnectionMode(PIPE_ISP, PIPE_MCSC);
    m_flagMcscVraOTF = m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA);

    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request[PIPE_3AP] = (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M);
    m_request[PIPE_ISPC] = (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M);

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
#endif
	
#if defined(SUPPORT_HW_GDC)
    /* GDC */
    m_nodeNums[INDEX(PIPE_GDC)][OUTPUT_NODE] = FIMC_IS_VIDEO_GDC_NUM;
#endif

#ifdef SUPPORT_HFD
    /* HFD */
    m_nodeNums[INDEX(PIPE_HFD)][OUTPUT_NODE] = -1;
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

status_t ExynosCameraFrameFactoryPreview::m_constructPipes()
{
    CLOGI("");

    int pipeId = -1;

    /* FLITE */
    pipeId = PIPE_FLITE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_FLITE");

    /* 3AA */
    pipeId = PIPE_3AA;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_3AA");

    /* ISP */
    pipeId = PIPE_ISP;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_ISP");

    /* MCSC */
    pipeId = PIPE_MCSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_MCSC");

#ifdef SAMSUNG_TN_FEATURE	
    /* PostProcessing for Preview */
    pipeId = PIPE_PP_UNI;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_PP_UNI");

    pipeId = PIPE_PP_UNI2;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_PP_UNI2");
#endif

#if defined(SUPPORT_HW_GDC)
    /* GDC */
    pipeId = PIPE_GDC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_GDC");
    m_pipes[pipeId]->setPipeId(PIPE_GDC);
#endif

    /* VRA */
    pipeId = PIPE_VRA;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeVRA(m_cameraId, m_parameters, m_flagReprocessing, m_deviceInfo[pipeId].nodeNum);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_VRA");

#ifdef SUPPORT_HFD
    /* HFD */
    if (m_parameters->getHfdMode() == true) {
        pipeId = PIPE_HFD;
        m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeHFD(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
        m_pipes[pipeId]->setPipeId(pipeId);
        m_pipes[pipeId]->setPipeName("PIPE_HFD");
    }
#endif

    /* GSC */
    pipeId = PIPE_GSC;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC");

    /* GSC for Recording */
    pipeId = PIPE_GSC_VIDEO;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_VIDEO");

    /* GSC for Capture */
    pipeId = PIPE_GSC_PICTURE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_PICTURE");

    /* JPEG */
    pipeId = PIPE_JPEG;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_JPEG");

    CLOGI("pipe ids for preview");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("m_pipes[%d] : PipeId(%d)" , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::m_initFrameMetadata(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;

    if (m_shot_ext == NULL) {
        CLOGE("new struct camera2_shot_ext fail");
        return INVALID_OPERATION;
    }

    memset(m_shot_ext, 0x0, sizeof(struct camera2_shot_ext));

    m_shot_ext->shot.magicNumber = SHOT_MAGIC_NUMBER;

    for (int i = 0; i < INTERFACE_TYPE_MAX; i++) {
        m_shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[i] = MCSC_PORT_NONE;
    }

    frame->setRequest(PIPE_VC0, m_request[PIPE_VC0]);
    frame->setRequest(PIPE_VC1, m_request[PIPE_VC1]);
    frame->setRequest(PIPE_3AC, m_request[PIPE_3AC]);
    frame->setRequest(PIPE_3AP, m_request[PIPE_3AP]);
    frame->setRequest(PIPE_ISPC, m_request[PIPE_ISPC]);
    frame->setRequest(PIPE_ISPP, m_request[PIPE_ISPP]);
    frame->setRequest(PIPE_MCSC0, m_request[PIPE_MCSC0]);
    frame->setRequest(PIPE_MCSC1, m_request[PIPE_MCSC1]);
    frame->setRequest(PIPE_MCSC2, m_request[PIPE_MCSC2]);
    frame->setRequest(PIPE_GSC, m_request[PIPE_GSC]);
#ifdef SAMSUNG_TN_FEATURE	
	frame->setRequest(PIPE_PP_UNI, m_request[PIPE_PP_UNI]);
    frame->setRequest(PIPE_PP_UNI2, m_request[PIPE_PP_UNI2]);
#endif
    frame->setRequest(PIPE_GDC, m_request[PIPE_GDC]);
    frame->setRequest(PIPE_MCSC5, m_request[PIPE_MCSC5]);
    frame->setRequest(PIPE_VRA, m_request[PIPE_VRA]);
    frame->setRequest(PIPE_HFD, m_request[PIPE_HFD]);
#ifdef USE_DUAL_CAMERA
    frame->setRequest(PIPE_DCPS0, m_request[PIPE_DCPS0]);
    frame->setRequest(PIPE_DCPS1, m_request[PIPE_DCPS1]);
    frame->setRequest(PIPE_DCPC0, m_request[PIPE_DCPC0]);
    frame->setRequest(PIPE_DCPC1, m_request[PIPE_DCPC1]);
    frame->setRequest(PIPE_DCPC2, m_request[PIPE_DCPC2]);
    frame->setRequest(PIPE_DCPC3, m_request[PIPE_DCPC3]);
    frame->setRequest(PIPE_DCPC4, m_request[PIPE_DCPC4]);
    frame->setRequest(PIPE_SYNC, m_request[PIPE_SYNC]);
    frame->setRequest(PIPE_FUSION, m_request[PIPE_FUSION]);
#endif

    setMetaBypassDrc(m_shot_ext, m_bypassDRC);
    setMetaBypassDnr(m_shot_ext, m_bypassDNR);
    setMetaBypassDis(m_shot_ext, m_bypassDIS);
    setMetaBypassFd(m_shot_ext, m_bypassFD);

    ret = frame->initMetaData(m_shot_ext);
    if (ret != NO_ERROR)
        CLOGE("initMetaData fail");

    return ret;
}

void ExynosCameraFrameFactoryPreview::connectScenario(int pipeId, int scenario)
{
    CLOGV("pipeId(%d), scenario(%d)", pipeId, scenario);

#ifdef SAMSUNG_TN_FEATURE	
    if (pipeId == PIPE_PP_UNI || pipeId == PIPE_PP_UNI2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->connectScenario(scenario);
    }
#endif
}
status_t ExynosCameraFrameFactoryPreview::startPipes(void)
{
    Mutex::Autolock lock(m_sensorStandbyLock);

    status_t ret = NO_ERROR;

#ifdef SUPPORT_HFD
    if (m_parameters->getHfdMode() == true) {
        ret = m_pipes[PIPE_HFD]->start();
        if (ret != NO_ERROR) {
            CLOGE("HFD start fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }
#endif

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
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

#ifdef SAMSUNG_SSRM
    int width = 0, height = 0;
    uint32_t minfps = 0, maxfps = 0;
    struct ssrmCameraInfo cameraInfo;
    memset(&cameraInfo, 0, sizeof(cameraInfo));

    m_parameters->getMaxYuvSize(&width, &height);
    m_parameters->getPreviewFpsRange(&minfps, &maxfps);

    cameraInfo.operation = SSRM_CAMERA_INFO_SET;
    cameraInfo.cameraId = m_cameraId;
    cameraInfo.width = width;
    cameraInfo.height = height;
    cameraInfo.minFPS = (int)minfps;
    cameraInfo.maxFPS = (int)maxfps;
    cameraInfo.sensorOn = 1;

    storeSsrmCameraInfo(&cameraInfo);
#endif

    CLOGI("Starting Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryPreview::stopPipes(void)
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

    if (m_pipes[PIPE_GSC]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_GSC]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
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

    ret = stopThreadAndWait(PIPE_GSC);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC stopThreadAndWait fail, ret(%d)", ret);
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        funcRet |= ret;
    }

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo cameraInfo;
    memset(&cameraInfo, 0, sizeof(cameraInfo));

    cameraInfo.operation = SSRM_CAMERA_INFO_CLEAR;
    cameraInfo.cameraId = m_cameraId;
    storeSsrmCameraInfo(&cameraInfo);
#endif

    CLOGI("Stopping  Success!");

    return funcRet;
}

void ExynosCameraFrameFactoryPreview::startScenario(int pipeId)
{
    CLOGD("pipeId(%d)", pipeId);

#ifdef SAMSUNG_TN_FEATURE	
    if (pipeId == PIPE_PP_UNI || pipeId == PIPE_PP_UNI2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->startScenario();
    }
#endif
}

void ExynosCameraFrameFactoryPreview::stopScenario(int pipeId, bool suspendFlag)
{
    CLOGD("pipeId(%d), suspendFlag(%d)", pipeId, suspendFlag);

#ifdef SAMSUNG_TN_FEATURE	
    if (pipeId == PIPE_PP_UNI || pipeId == PIPE_PP_UNI2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->stopScenario(suspendFlag);
    }
#endif
}

int ExynosCameraFrameFactoryPreview::getScenario(
#ifndef SAMSUNG_TN_FEATURE	
    __unused
#endif
    int pipeId)
{
    int scenario = -1;

#ifdef SAMSUNG_TN_FEATURE	
    scenario = PP_SCENARIO_NONE;

    if (pipeId == PIPE_PP_UNI || pipeId == PIPE_PP_UNI2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        scenario = pipe->getScenario();

        CLOGV("pipeId(%d), scenario(%d)", pipeId, scenario);
    }
#endif

    return scenario;
}

}; /* namespace android */
