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
#define LOG_TAG "ExynosCameraFrameReprocessingFactorySec"
#include <log/log.h>

#include "ExynosCameraFrameReprocessingFactory.h"

namespace android {

ExynosCameraFrameSP_sptr_t ExynosCameraFrameReprocessingFactory::createNewFrame(uint32_t frameCount, bool useJpegFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }
    ExynosCameraFrameSP_sptr_t frame =  m_frameMgr->createFrame(m_configurations, frameCount, FRAME_TYPE_REPROCESSING);
    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = PIPE_3AA_REPROCESSING;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return NULL;
    }

    m_setBaseInfoToFrame(frame);

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("frame(%d) metadata initialize fail", m_frameCount);

    /* set 3AA pipe to linkageList */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = PIPE_3AA_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        parentPipeId = pipeId;
    }

    /* set ISP pipe to linkageList */
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_ISP_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        if (m_supportPureBayerReprocessing == true)
            frame->addChildEntity(newEntity[INDEX(parentPipeId)], newEntity[INDEX(pipeId)], INDEX(PIPE_3AP_REPROCESSING));
        else
            frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        parentPipeId = pipeId;
    }

    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
    requestEntityCount++;

    /* set MCSC pipe to linkageList */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set VRA pipe to linkageList */
    if (((m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)
        && m_request[INDEX(PIPE_MCSC5_REPROCESSING)] == true && m_request[INDEX(PIPE_VRA_REPROCESSING)] == true) ||
        ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) &&
        m_request[INDEX(PIPE_3AF_REPROCESSING)] == true && m_request[INDEX(PIPE_VRA_REPROCESSING)] == true)) {
        pipeId = PIPE_VRA_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        requestEntityCount++;
    }

#ifdef USE_DUAL_CAMERA
    /* set Sync pipe to linkageList */
    pipeId = PIPE_SYNC_REPROCESSING;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        requestEntityCount++;
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    pipeId = PIPE_PP_UNI_REPROCESSING;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    pipeId = PIPE_PP_UNI_REPROCESSING2;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }
#endif

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set JPEG pipe to linkageList */
    pipeId = PIPE_JPEG_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (m_flagHWFCEnabled == false || useJpegFlag == true) {
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING2;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING3;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    frame->setNumRequestPipe(requestEntityCount);
    frame->setParameters(m_parameters);
    frame->setCameraId(m_cameraId);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactory::m_setupConfig(void)
{
    CLOGI("");

    int pipeId = -1;

    int node3Paf = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1, node3af = -1, node3ag = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1;
    int nodeMcscp2 = -1, nodeMcscp3 = -1, nodeMcscp4 = -1;
    int nodeMcscpDs = -1;
    int nodeVra = -1;
    int previousPipeId = -1;
    int vraSrcPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = false;
    bool supportJpeg = false;
    enum pipeline pipeId3Paf = (enum pipeline)-1;

#ifdef USE_PAF
    pipeId3Paf = PIPE_3PAF_REPROCESSING;
#endif

    m_flagFlite3aaOTF = m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
    m_flagPaf3aaOTF = m_parameters->getHwConnectionMode(pipeId3Paf, PIPE_3AA_REPROCESSING);
    m_flag3aaIspOTF = m_parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING);
    m_flagIspMcscOTF = m_parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING);
    m_flag3aaVraOTF = m_parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_VRA_REPROCESSING);
    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_NONE)
        m_flagMcscVraOTF = m_parameters->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING);


    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request[INDEX(PIPE_3AP_REPROCESSING)] = (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M);
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        m_request[INDEX(PIPE_ISPC_REPROCESSING)] = true;
    }

    /* jpeg can be supported in this factory depending on mcsc output port */
    if (m_parameters->getNumOfMcscOutputPorts() > 3) {
        m_request[INDEX(PIPE_MCSC_JPEG_REPROCESSING)] = true;
        m_request[INDEX(PIPE_MCSC_THUMB_REPROCESSING)] = true;
        if (m_flagHWFCEnabled == true) {
            m_request[INDEX(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = true;
            m_request[INDEX(PIPE_HWFC_JPEG_DST_REPROCESSING)] = true;
            m_request[INDEX(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = true;
        }
    }

    if (m_configurations->getMode(CONFIGURATION_OBTE_MODE) == true) {
        switch (m_configurations->getModeValue(CONFIGURATION_OBTE_DATA_PATH)) {
        case 0:
            node3aa = FIMC_IS_VIDEO_31S_NUM;
            node3ac = FIMC_IS_VIDEO_31C_NUM;
            node3ap = FIMC_IS_VIDEO_31P_NUM;
            node3af = FIMC_IS_VIDEO_31F_NUM;
            nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
            nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
            nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
            break;
        case 1:
            node3aa = FIMC_IS_VIDEO_30S_NUM;
            node3ac = FIMC_IS_VIDEO_30C_NUM;
            node3ap = FIMC_IS_VIDEO_30P_NUM;
            node3af = FIMC_IS_VIDEO_30F_NUM;
            nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
            nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
            nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
            break;
        case 2:
            node3aa = FIMC_IS_VIDEO_31S_NUM;
            node3ac = FIMC_IS_VIDEO_31C_NUM;
            node3ap = FIMC_IS_VIDEO_31P_NUM;
            node3af = FIMC_IS_VIDEO_31F_NUM;
            nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
            nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
            nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
            break;
        case 3:
            node3aa = FIMC_IS_VIDEO_30S_NUM;
            node3ac = FIMC_IS_VIDEO_30C_NUM;
            node3ap = FIMC_IS_VIDEO_30P_NUM;
            node3af = FIMC_IS_VIDEO_30F_NUM;
            nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
            nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
            nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
            break;
        default:
            node3aa = FIMC_IS_VIDEO_31S_NUM;
            node3ac = FIMC_IS_VIDEO_31C_NUM;
            node3ap = FIMC_IS_VIDEO_31P_NUM;
            node3af = FIMC_IS_VIDEO_31F_NUM;
            nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
            nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
            nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
            break;
        }
    } else {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        node3af = FIMC_IS_VIDEO_31F_NUM;
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
    }

    if (m_parameters->getHwChainType() == HW_CHAIN_TYPE_SEMI_DUAL_CHAIN) {
#ifdef USE_PAF
        node3Paf = (m_configurations->getCameraId() == CAMERA_ID_BACK)? FIMC_IS_VIDEO_P1S_NUM: FIMC_IS_VIDEO_P0S_NUM;
#endif
#if defined(FRONT_CAMERA_3AA_NUM)
        if (getCameraId() == CAMERA_ID_FRONT_0
            && FRONT_CAMERA_3AA_NUM != FIMC_IS_VIDEO_30S_NUM) {
                node3aa = FIMC_IS_VIDEO_30S_NUM;
                node3ac = FIMC_IS_VIDEO_30C_NUM;
                node3ap = FIMC_IS_VIDEO_30P_NUM;
                node3af = FIMC_IS_VIDEO_31F_NUM;
        }
#endif
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
    }

    if (m_parameters->getNumOfMcscInputPorts() > 1)
#if 0//def USE_DUAL_CAMERA // HACK??
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
#else
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
#endif
    else
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;

    nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeMcscp3 = FIMC_IS_VIDEO_M3P_NUM;
    nodeMcscp4 = FIMC_IS_VIDEO_M4P_NUM;
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

    m_initDeviceInfo(INDEX(PIPE_3AA_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_ISP_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_MCSC_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_VRA_REPROCESSING));

    /*
     * 3AA for Reprocessing
     */
    pipeId = INDEX(PIPE_3AA_REPROCESSING);
    previousPipeId = PIPE_FLITE;

    if (m_flagPaf3aaOTF == HW_CONNECTION_MODE_OTF
		|| m_flagPaf3aaOTF == HW_CONNECTION_MODE_M2M) {

        /*
         * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
         */
        if (m_supportPureBayerReprocessing == false) {
            flagStreamLeader = false;
        } else {
            flagStreamLeader = true;
        }

        /* 3PAF */
        nodeType = getNodeType(pipeId3Paf);
        m_deviceInfo[pipeId].pipeId[nodeType]  = pipeId3Paf;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3Paf;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3PAF_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        int fliteNodeNum = getFliteNodenum(m_cameraId);
        m_sensorIds[pipeId][nodeType]  = m_getSensorId(getFliteCaptureNodenum(m_cameraId, fliteNodeNum), false, flagStreamLeader, m_flagReprocessing);
        previousPipeId = INDEX(pipeId3Paf);
    }

    /*
     * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
     */
    if (m_supportPureBayerReprocessing == false
        || m_flagPaf3aaOTF == HW_CONNECTION_MODE_OTF) {
        flagStreamLeader = false;
    } else {
        flagStreamLeader = true;
    }

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    if (m_flagPaf3aaOTF == HW_CONNECTION_MODE_OTF
        || m_flagPaf3aaOTF == HW_CONNECTION_MODE_M2M) {
        m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(pipeId3Paf)], m_flagPaf3aaOTF, flagStreamLeader, m_flagReprocessing);
    } else {
        int fliteNodeNum = getFliteNodenum(m_cameraId);
        m_sensorIds[pipeId][nodeType]  = m_getSensorId(getFliteCaptureNodenum(m_cameraId, fliteNodeNum), false, flagStreamLeader, m_flagReprocessing);
    }

    // Restore leader flag
    flagStreamLeader = false;

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_OPT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    if (m_parameters->isUse3aaDNG()) {
        /* 3AG */
        node3ag = FIMC_IS_VIDEO_31G_NUM;
        nodeType = getNodeType(PIPE_3AG_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AG_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3ag;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_DNG", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AG_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }

    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
        int vraPipeId = INDEX(PIPE_VRA_REPROCESSING);
        /* 3AF */
        nodeType = getNodeType(PIPE_3AF_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AF_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3af;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_FD", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /*
         * VRA
         */
        previousPipeId = pipeId;

        vraSrcPipeId = INDEX(PIPE_3AF_REPROCESSING);
        nodeType = getNodeType(PIPE_VRA_REPROCESSING);
        m_deviceInfo[vraPipeId].pipeId[nodeType]  = PIPE_VRA;
        m_deviceInfo[vraPipeId].nodeNum[nodeType] = nodeVra;
        m_deviceInfo[vraPipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[vraPipeId].nodeName[nodeType], "VRA_OUTPUT_REPROCESSING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[vraPipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flag3aaVraOTF, flagStreamLeader, m_flagReprocessing);
    }

    /*
     * ISP for Reprocessing
     */
    previousPipeId = pipeId;
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_ISP_REPROCESSING);
    }

    if (m_supportPureBayerReprocessing == false) {
        /* Set leader flag on ISP pipe for Dirty bayer reprocessing */
        flagStreamLeader = true;
    }

    /* ISPS */
    nodeType = getNodeType(PIPE_ISP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

    // Restore leader flag
    flagStreamLeader = false;

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_M2M", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_OTF", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /*
     * MCSC for Reprocessing
     */
    previousPipeId = pipeId;

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_MCSC_REPROCESSING);
    }

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPC_REPROCESSING)], m_flagIspMcscOTF, flagStreamLeader, m_flagReprocessing);

    switch (m_parameters->getNumOfMcscOutputPorts()) {
    case 5:
        supportJpeg = true;

        /* MCSC4 */
        nodeType = getNodeType(PIPE_MCSC_THUMB_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_THUMB_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp4;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_THUMBNAIL", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* MCSC3 */
        nodeType = getNodeType(PIPE_MCSC_JPEG_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_JPEG_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp3;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Not break; */
    case 3:
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_2", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* MCSC1 */
        nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_1", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Not break; */
    case 1:
        /* MCSC0 */
        nodeType = getNodeType(PIPE_MCSC0_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_0", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
        break;
    default:
        CLOG_ASSERT("invalid MCSC output(%d)", m_parameters->getNumOfMcscOutputPorts());
        break;
    }

    if (supportJpeg == true && m_flagHWFCEnabled == true) {
        /* JPEG Src */
        nodeType = getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Src */
        nodeType = getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* JPEG Dst */
        nodeType = getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Dst  */
        nodeType = getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        /* MCSC5 */
        nodeType = getNodeType(PIPE_MCSC5_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC5_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscpDs;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_DS_REPROCESSING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /*
         * VRA
         */
        previousPipeId = pipeId;
        vraSrcPipeId = PIPE_MCSC5_REPROCESSING;
        pipeId = INDEX(PIPE_VRA_REPROCESSING);

        nodeType = getNodeType(PIPE_VRA_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VRA_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeVra;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_VRA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flagMcscVraOTF, flagStreamLeader, m_flagReprocessing);
    }

    /* GSC for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* GSC2 for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING2)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* GSC3 for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING3)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG for Reprocessing */
    m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)][OUTPUT_NODE] = -1;

#ifdef SAMSUNG_TN_FEATURE
    /* PostProcessing for Reprocessing */
    m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
    m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING2)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_constructPipes(void)
{
    CLOGI("");

    int pipeId = -1;

    /* 3AA for Reprocessing */
    pipeId = PIPE_3AA_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_3AA_REPROCESSING");

    /* ISP for Reprocessing */
    pipeId = PIPE_ISP_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_ISP_REPROCESSING");

    /* MCSC for Reprocessing */
    pipeId = PIPE_MCSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_MCSC_REPROCESSING");

#ifdef SAMSUNG_TN_FEATURE
    /* PostProcessing for Reprocessing */
    pipeId = PIPE_PP_UNI_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipePP(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_PP_UNI_REPROCESSING");

    pipeId = PIPE_PP_UNI_REPROCESSING2;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipePP(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_PP_UNI_REPROCESSING2");
#endif

    /* VRA */
    pipeId = PIPE_VRA_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeVRA(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_deviceInfo[INDEX(pipeId)].nodeNum);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_VRA_REPROCESSING");

    /* GSC for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING");

    /* GSC2 for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING2;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING2");

    /* GSC3 for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING3;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(
                m_cameraId,
                m_configurations,
                m_parameters,
                m_flagReprocessing,
                m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING3");

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG for Reprocessing */
        pipeId = PIPE_JPEG_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(
                    m_cameraId,
                    m_configurations,
                    m_parameters,
                    m_flagReprocessing,
                    m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_JPEG_REPROCESSING");
    }

    CLOGI("pipe ids for reprocessing");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("m_pipes[%d] : PipeId(%d)" , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_initFrameMetadata(ExynosCameraFrameSP_sptr_t frame)
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

    frame->setRequest(PIPE_3AC_REPROCESSING, m_request[INDEX(PIPE_3AC_REPROCESSING)]);
    frame->setRequest(PIPE_3AP_REPROCESSING, m_request[INDEX(PIPE_3AP_REPROCESSING)]);
    frame->setRequest(PIPE_3AF_REPROCESSING, m_request[INDEX(PIPE_3AF_REPROCESSING)]);
    frame->setRequest(PIPE_3AG_REPROCESSING, m_request[INDEX(PIPE_3AG_REPROCESSING)]);
    frame->setRequest(PIPE_ISPC_REPROCESSING, m_request[INDEX(PIPE_ISPC_REPROCESSING)]);
    frame->setRequest(PIPE_ISPP_REPROCESSING, m_request[INDEX(PIPE_ISPP_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC0_REPROCESSING, m_request[INDEX(PIPE_MCSC0_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC1_REPROCESSING, m_request[INDEX(PIPE_MCSC1_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC2_REPROCESSING, m_request[INDEX(PIPE_MCSC2_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC_JPEG_REPROCESSING, m_request[INDEX(PIPE_MCSC_JPEG_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC_THUMB_REPROCESSING, m_request[INDEX(PIPE_MCSC_THUMB_REPROCESSING)]);
    frame->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, m_request[INDEX(PIPE_HWFC_JPEG_SRC_REPROCESSING)]);
    frame->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, m_request[INDEX(PIPE_HWFC_JPEG_DST_REPROCESSING)]);
    frame->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, m_request[INDEX(PIPE_HWFC_THUMB_SRC_REPROCESSING)]);
    frame->setRequest(PIPE_MCSC5_REPROCESSING, m_request[INDEX(PIPE_MCSC5_REPROCESSING)]);
#ifdef SAMSUNG_TN_FEATURE
    frame->setRequest(PIPE_PP_UNI_REPROCESSING, m_request[INDEX(PIPE_PP_UNI_REPROCESSING)]);
    frame->setRequest(PIPE_PP_UNI_REPROCESSING2, m_request[INDEX(PIPE_PP_UNI_REPROCESSING2)]);
#endif
    frame->setRequest(PIPE_VRA_REPROCESSING, m_request[INDEX(PIPE_VRA_REPROCESSING)]);
#ifdef USE_DUAL_CAMERA
    frame->setRequest(PIPE_SYNC_REPROCESSING, m_request[INDEX(PIPE_SYNC_REPROCESSING)]);
    frame->setRequest(PIPE_FUSION_REPROCESSING, m_request[INDEX(PIPE_FUSION_REPROCESSING)]);
#endif

    /* Reprocessing is not use this */
    m_bypassDRC = 1;
    m_bypassDNR = 1;
    m_bypassDIS = 1;
    m_bypassFD = 1;

    setMetaBypassDrc(m_shot_ext, m_bypassDRC);
    setMetaBypassDnr(m_shot_ext, m_bypassDNR);
    setMetaBypassDis(m_shot_ext, m_bypassDIS);
    setMetaBypassFd(m_shot_ext, m_bypassFD);

    ret = frame->initMetaData(m_shot_ext);
    if (ret != NO_ERROR)
        CLOGE("initMetaData fail");

    return ret;
}

void ExynosCameraFrameReprocessingFactory::connectPPScenario(int pipeId, int scenario)
{
    CLOGD("pipeId(%d), scenario(%d)", pipeId, scenario);

#ifdef SAMSUNG_TN_FEATURE
    if (pipeId == PIPE_PP_UNI_REPROCESSING || pipeId == PIPE_PP_UNI_REPROCESSING2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->connectPPScenario(scenario);
    }
#endif
}

void ExynosCameraFrameReprocessingFactory::extControl(int pipeId, int scenario, int controlType, __unused void *data)
{
    CLOGD("pipeId(%d), scenario(%d), controlType(%d)", pipeId, scenario, controlType);

#ifdef SAMSUNG_TN_FEATURE
    if (pipeId == PIPE_PP_UNI_REPROCESSING || pipeId == PIPE_PP_UNI_REPROCESSING2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        if (pipe != NULL) {
            pipe->extControl(scenario, controlType, data);
        } else {
            CLOGE("pipe(%d) is Null", pipeId);
        }
    }
#endif
}

void ExynosCameraFrameReprocessingFactory::startPPScenario(int pipeId)
{
    CLOGD("pipeId(%d)", pipeId);

#ifdef SAMSUNG_TN_FEATURE
    if (pipeId == PIPE_PP_UNI_REPROCESSING || pipeId == PIPE_PP_UNI_REPROCESSING2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->startPPScenario();
    }
#endif
}

void ExynosCameraFrameReprocessingFactory::stopPPScenario(int pipeId, bool suspendFlag)
{
    CLOGD("pipeId(%d), suspendFlag(%d)", pipeId, suspendFlag);

#ifdef SAMSUNG_TN_FEATURE
    if (pipeId == PIPE_PP_UNI_REPROCESSING || pipeId == PIPE_PP_UNI_REPROCESSING2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        pipe->stopPPScenario();
    }
#endif
}

int ExynosCameraFrameReprocessingFactory::getPPScenario(__unused int pipeId)
{
    int scenario = 0;

#ifdef SAMSUNG_TN_FEATURE
    if (pipeId == PIPE_PP_UNI_REPROCESSING || pipeId == PIPE_PP_UNI_REPROCESSING2) {
        ExynosCameraPipePP *pipe;
        pipe = (ExynosCameraPipePP *)(m_pipes[INDEX(pipeId)]);
        scenario = pipe->getPPScenario();

        CLOGV("pipeId(%d), scenario(%d)", pipeId, scenario);
    }
#endif

    return scenario;
}

}; /* namespace android */
