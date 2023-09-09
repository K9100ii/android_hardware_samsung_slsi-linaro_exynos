/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraFrameFactory3aaIspTpu"
#include <cutils/log.h>

#include "ExynosCameraFrameFactory3aaIspTpu.h"

namespace android {

ExynosCameraFrameFactory3aaIspTpu::~ExynosCameraFrameFactory3aaIspTpu()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);
}


enum NODE_TYPE ExynosCameraFrameFactory3aaIspTpu::getNodeType(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;
    if (m_flagIspTpuOTF == false) {
        nodeType = m_getNodeTypeTpu(pipeId);
    } else {
        nodeType = m_getNodeType(pipeId);
    }

    return nodeType;
}

enum NODE_TYPE ExynosCameraFrameFactory3aaIspTpu::m_getNodeType(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;

    switch (pipeId) {
    case PIPE_FLITE:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AA:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_3AC:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AP:
        nodeType = CAPTURE_NODE_2;
        break;
    case PIPE_ISP:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_ISPP:
        nodeType = CAPTURE_NODE_3;
        break;
    case PIPE_DIS:
        nodeType = OTF_NODE_1;
        break;
    case PIPE_ISPC:
    case PIPE_SCC:
    case PIPE_JPEG:
        nodeType = CAPTURE_NODE_4;
        break;
    case PIPE_SCP:
        nodeType = CAPTURE_NODE_5;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
            __FUNCTION__, __LINE__, pipeId);
        break;
    }

    return nodeType;
}


enum NODE_TYPE ExynosCameraFrameFactory3aaIspTpu::m_getNodeTypeTpu(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;

    switch (pipeId) {
    case PIPE_FLITE:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AA:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_3AC:
        nodeType = CAPTURE_NODE_1;
        break;
    case PIPE_3AP:
        nodeType = CAPTURE_NODE_2;
        break;
    case PIPE_ISP:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_ISPP:
        nodeType = CAPTURE_NODE_3;
        break;
    case PIPE_DIS:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_ISPC:
    case PIPE_SCC:
    case PIPE_JPEG:
        nodeType = CAPTURE_NODE_4;
        break;
    case PIPE_SCP:
        nodeType = CAPTURE_NODE_5;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
            __FUNCTION__, __LINE__, pipeId);
        break;
    }

    return nodeType;
}

status_t ExynosCameraFrameFactory3aaIspTpu::m_setDeviceInfo(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    bool flagDirtyBayer = false;

    m_flagIspTpuOTF = m_parameters->isIspTpuOtf();

    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false)
        flagDirtyBayer = true;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;

    int t3aaNums[MAX_NODE];
    int ispNums[MAX_NODE];

#ifdef RAWDUMP_CAPTURE
    t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_31S_NUM;
    t3aaNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_31C_NUM;
    t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_31P_NUM;
#else
    if (m_parameters->getDualMode() == true) {
        t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_31S_NUM;
        t3aaNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_31C_NUM;
        t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_31P_NUM;
    } else {
        t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_30S_NUM;
        t3aaNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_30C_NUM;
        t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_30P_NUM;
    }
#endif

    ispNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_I0S_NUM;
    ispNums[CAPTURE_NODE_1] = FIMC_IS_VIDEO_I0C_NUM;
    ispNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_I0P_NUM;

    m_initDeviceInfo(INDEX(PIPE_3AA));
    m_initDeviceInfo(INDEX(PIPE_ISP));
    m_initDeviceInfo(INDEX(PIPE_DIS));

    /*******
     * 3AA
     ******/
    pipeId = INDEX(PIPE_3AA);

    // 3AS
    nodeType = getNodeType(PIPE_3AA);
    m_deviceInfo[pipeId].nodeNum[nodeType] = t3aaNums[OUTPUT_NODE];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_nodeNums[INDEX(PIPE_FLITE)][getNodeType(PIPE_FLITE)], m_flagFlite3aaOTF, true, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA;

    // 3AC
    nodeType = getNodeType(PIPE_3AC);
    if (flagDirtyBayer == true) {
        m_deviceInfo[pipeId].nodeNum[nodeType] = t3aaNums[CAPTURE_NODE_1];
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], false, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    } else {
        m_deviceInfo[pipeId].secondaryNodeNum[nodeType] = t3aaNums[CAPTURE_NODE_1];
        strncpy(m_deviceInfo[pipeId].secondaryNodeName[nodeType], "3AA_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], true, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC;
    }

    // 3AP
    nodeType = getNodeType(PIPE_3AP);
    m_deviceInfo[pipeId].nodeNum[nodeType] = t3aaNums[CAPTURE_NODE_2];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA)], m_flag3aaIspOTF, false, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP;

    // set nodeNum
    for (int i = 0; i < MAX_NODE; i++)
        m_nodeNums[pipeId][i] = m_deviceInfo[pipeId].nodeNum[i];

    if (m_checkNodeSetting(pipeId) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_checkNodeSetting(%d) fail", __FUNCTION__, __LINE__, pipeId);
        return INVALID_OPERATION;
    }

    /*******
     * ISP
     ******/
    pipeId = INDEX(PIPE_ISP);

    // ISPS
    nodeType = getNodeType(PIPE_ISP);
    m_deviceInfo[pipeId].nodeNum[nodeType] = ispNums[OUTPUT_NODE];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[getNodeType(PIPE_3AP)], m_flag3aaIspOTF, false, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_ISP;

    if (m_flagIspTpuOTF == true) {
        // ISPP
        nodeType = getNodeType(PIPE_ISPP);
        m_deviceInfo[pipeId].secondaryNodeNum[nodeType] = ispNums[CAPTURE_NODE_2];
        strncpy(m_deviceInfo[pipeId].secondaryNodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], true, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;

        // SCP
        nodeType = getNodeType(PIPE_SCP);
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_SCP_NUM;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "SCP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[INDEX(PIPE_ISP)].secondaryNodeNum[getNodeType(PIPE_ISPP)], true, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_SCP;
    } else {
        // ISPP
        nodeType = getNodeType(PIPE_ISPP);
        m_deviceInfo[pipeId].nodeNum[nodeType] = ispNums[CAPTURE_NODE_2];
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP)], false, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP;
    }

    // set nodeNum
    for (int i = 0; i < MAX_NODE; i++)
        m_nodeNums[pipeId][i] = m_deviceInfo[pipeId].nodeNum[i];

    if (m_checkNodeSetting(pipeId) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_checkNodeSetting(%d) fail", __FUNCTION__, __LINE__, pipeId);
        return INVALID_OPERATION;
    }

    /*******
     * DIS
     ******/
    pipeId = INDEX(PIPE_DIS);

    if (m_flagIspTpuOTF == true) {
        // DIS
        nodeType = getNodeType(PIPE_DIS);
        m_deviceInfo[pipeId].secondaryNodeNum[nodeType] = FIMC_IS_VIDEO_TPU_NUM;
        strncpy(m_deviceInfo[pipeId].secondaryNodeName[nodeType], "DIS_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_secondarySensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[INDEX(PIPE_ISP)].secondaryNodeNum[getNodeType(PIPE_ISPP)], true, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_DIS;
    } else {
        // DIS
        nodeType = getNodeType(PIPE_DIS);
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_TPU_NUM;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "DIS_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[getNodeType(PIPE_ISPP)], false, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_DIS;

        // SCP
        nodeType = getNodeType(PIPE_SCP);
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_SCP_NUM;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "SCP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_DIS)], true, false, m_flagReprocessing);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_SCP;
    }

    // set nodeNum
    for (int i = 0; i < MAX_NODE; i++)
        m_nodeNums[pipeId][i] = m_deviceInfo[pipeId].nodeNum[i];

    if (m_checkNodeSetting(pipeId) != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_checkNodeSetting(%d) fail", __FUNCTION__, __LINE__, pipeId);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory3aaIspTpu::m_initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

#ifdef SUPPORT_GROUP_MIGRATION
    m_updateNodeInfo();
#endif

    int ret = 0;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int maxSensorW = 0, maxSensorH = 0, hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bdsSize;
    int perFramePos = 0;
    int stride = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);

    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);

    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    m_parameters->getPreviewBdsSize(&bdsSize);

    /* When high speed recording mode, hw sensor size is fixed.
     * So, maxPreview size cannot exceed hw sensor size
     */
    if (m_parameters->getHighSpeedRecording()) {
        maxPreviewW = hwSensorW;
        maxPreviewH = hwSensorH;
    }

    CLOGI("INFO(%s[%d]): MaxSensorSize(%dx%d), HWSensorSize(%dx%d)", __FUNCTION__, __LINE__, maxSensorW, maxSensorH, hwSensorW, hwSensorH);
    CLOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI("INFO(%s[%d]): HWPictureSize(%dx%d)", __FUNCTION__, __LINE__, hwPictureW, hwPictureH);

    /* 3AS */
    enum NODE_TYPE t3asNodeType = getNodeType(PIPE_3AA);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_3AA].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_3AA].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_3AA)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_3AA)][i];
    }

    /* 3AS */
    if (m_flagFlite3aaOTF == true) {
        tempRect.fullW = 32;
        tempRect.fullH = 64;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3asNodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
    } else {
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3asNodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 10) * 2;
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 10) * 8 / 5;
        }
#endif
    }

    pipeInfo[t3asNodeType].rectInfo = tempRect;
    pipeInfo[t3asNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[t3asNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_3AA;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3asNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* 3AC */
    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false) {
        enum NODE_TYPE t3acNodeType = getNodeType(PIPE_3AC);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AC_POS : PERFRAME_FRONT_3AC_POS;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3acNodeType] - FIMC_IS_VIDEO_BAS_NUM);

#ifdef FIXED_SENSOR_SIZE
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
#else
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
#endif
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3acNodeType].rectInfo = tempRect;
        pipeInfo[t3acNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[t3acNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3acNodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
        /* per frame info */
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    /* 3AP */
    enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP);

    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AP_POS : PERFRAME_FRONT_3AP_POS;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = bayerFormat;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, 16);
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 3 / 2, 16);
    }
#endif

    pipeInfo[t3apNodeType].rectInfo = tempRect;
    pipeInfo[t3apNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[t3apNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[t3apNodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    ret = m_pipes[INDEX(PIPE_3AA)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* ISPS */
    enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_ISP)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_ISP].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_ISP].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_ISP)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_ISP)][i];
    }

    tempRect.fullW = bdsSize.w;
    tempRect.fullH = bdsSize.h;
    tempRect.colorFormat = bayerFormat;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[ispsNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 2, 16);
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[ispsNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW * 3 / 2, 16);
    }
#endif

    pipeInfo[ispsNodeType].rectInfo = tempRect;
    pipeInfo[ispsNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[ispsNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[ispsNodeType].bufInfo.count = config->current->bufInfo.num_3aa_buffers;

    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_ISP;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[ispsNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    if (m_flagIspTpuOTF == true) {
        /* SCP */
        int scpNodeType = getNodeType(PIPE_SCP);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;

        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[scpNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        stride = m_parameters->getHwPreviewStride();
        CLOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, stride);
        tempRect.fullW = stride;
        tempRect.fullH = hwPreviewH;
        tempRect.colorFormat = previewFormat;

#ifdef USE_BUFFER_WITH_STRIDE
        /* to use stride for preview buffer, set the bytesPerPlane */
        pipeInfo[scpNodeType].bytesPerPlane[0] = hwPreviewW;
#endif

        pipeInfo[scpNodeType].rectInfo = tempRect;
        pipeInfo[scpNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[scpNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        if (m_parameters->increaseMaxBufferOfPreview() == true) {
            pipeInfo[scpNodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
        } else {
            pipeInfo[scpNodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;
        }

        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }else {
        /* ISPP  */
        enum NODE_TYPE isppNodeType = getNodeType(PIPE_ISPP);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_ISPP_POS : PERFRAME_FRONT_ISPP_POS;

        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[isppNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

#ifdef USE_BUFFER_WITH_STRIDE
            /* to use stride for preview buffer, set the bytesPerPlane */
            pipeInfo[isppNodeType].bytesPerPlane[0] = bdsSize.w;
#endif

        pipeInfo[isppNodeType].rectInfo = tempRect;
        pipeInfo[isppNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[isppNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[isppNodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;;

        /* per frame info */
        pipeInfo[isppNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[isppNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    ret = m_pipes[INDEX(PIPE_ISP)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* DIS */
    enum NODE_TYPE disNodeType = getNodeType(PIPE_DIS);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_DIS)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_DIS].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_DIS].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_DIS)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_DIS)][i];
    }

    if (m_flagIspTpuOTF == true) {
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        pipeInfo[disNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 16);

        pipeInfo[disNodeType].rectInfo = tempRect;
        pipeInfo[disNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[disNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[disNodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[PIPE_DIS].secondaryNodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    } else {
        tempRect.fullW = bdsSize.w;
        tempRect.fullH = bdsSize.h;
        tempRect.colorFormat = hwVdisformat;

        pipeInfo[disNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 16);

        pipeInfo[disNodeType].rectInfo = tempRect;
        pipeInfo[disNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[disNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[disNodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[PIPE_DIS].nodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        /* SCP */
        int scpNodeType = getNodeType(PIPE_SCP);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_DIS)].nodeNum[scpNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        stride = m_parameters->getHwPreviewStride();
        CLOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, stride);
        tempRect.fullW = stride;
        tempRect.fullH = hwPreviewH;
        tempRect.colorFormat = previewFormat;

        pipeInfo[scpNodeType].rectInfo = tempRect;
        pipeInfo[scpNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[scpNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        if (m_parameters->increaseMaxBufferOfPreview() == true) {
            pipeInfo[scpNodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
        } else {
            pipeInfo[scpNodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;
        }

        /* per frame info */
        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef USE_BUFFER_WITH_STRIDE
        /* to use stride for preview buffer, set the bytesPerPlane */
        pipeInfo[scpNodeType].bytesPerPlane[0] = hwPreviewW;
#endif

    }
    ret = m_pipes[INDEX(PIPE_DIS)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):DIS setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory3aaIspTpu::m_initPipesFastenAeStable(int32_t numFrames,
                                                                      int hwSensorW, int hwSensorH,
                                                                      int hwPreviewW, int hwPreviewH)
{
    status_t ret = NO_ERROR;

    /* TODO 1. setup pipes for 120FPS */
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];

    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int hwVdisformat = m_parameters->getHWVdisFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    ExynosRect bdsSize;
    uint32_t frameRate = 0;
    struct v4l2_streamparm streamParam;
    int perFramePos = 0;

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getPreviewBdsSize(&bdsSize);

    /* 3AS */
    enum NODE_TYPE t3asNodeType = getNodeType(PIPE_3AA);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_3AA].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_3AA].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_3AA)][i];
    }

    tempRect.fullW = 32;
    tempRect.fullH = 64;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[t3asNodeType].rectInfo = tempRect;
    pipeInfo[t3asNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[t3asNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[t3asNodeType].bufInfo.count = numFrames;

    /* per frame info */
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_3AA;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3asNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    /* 3AC */
    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false) {
        enum NODE_TYPE t3acNodeType = getNodeType(PIPE_3AC);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AC_POS : PERFRAME_FRONT_3AC_POS;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3acNodeType] - FIMC_IS_VIDEO_BAS_NUM);

#ifdef FIXED_SENSOR_SIZE
        tempRect.fullW = maxSensorW;
        tempRect.fullH = maxSensorH;
#else
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
#endif
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3acNodeType].rectInfo = tempRect;
        pipeInfo[t3acNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[t3acNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3acNodeType].bufInfo.count = numFrames;
        /* per frame info */
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[t3acNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    /* 3AP pipe */
    enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP);
    //perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
    perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_3AP_POS : PERFRAME_FRONT_3AP_POS;

    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA)].nodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[t3apNodeType].rectInfo = tempRect;
    pipeInfo[t3apNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[t3apNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[t3apNodeType].bufInfo.count = numFrames;
    /* per frame info */
    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[t3apNodeType].rectInfo.fullW * 2, 16);
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[t3apNodeType].rectInfo.fullW * 3 / 2, 16);
    }
#endif

    ret = m_pipes[INDEX(PIPE_3AA)]->setupPipe(pipeInfo, sensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* ISP pipe */
    enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_ISP)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_ISP].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_ISP].pipeId[i], ret);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_ISP)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_ISP)][i];
    }

    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[ispsNodeType].rectInfo = tempRect;
    pipeInfo[ispsNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[ispsNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[ispsNodeType].bufInfo.count = numFrames;
    /* per frame info */
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_ISP;

    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[ispsNodeType] - FIMC_IS_VIDEO_BAS_NUM);

    int isppNodeType = 0;
    if (m_flagIspTpuOTF == true) {
        isppNodeType = getNodeType(PIPE_SCP);
        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
    } else {
        isppNodeType = getNodeType(PIPE_ISPP);
        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_ISPP_POS : PERFRAME_FRONT_ISPP_POS;
    }

    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
    pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP)].nodeNum[isppNodeType] - FIMC_IS_VIDEO_BAS_NUM);

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[isppNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[2].rectInfo.fullW * 2, 16);
        }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[isppNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[2].rectInfo.fullW * 3 / 2, 16);
    }
#endif

    int stride = m_parameters->getHwPreviewStride();
    CLOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, stride);

    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;

    pipeInfo[isppNodeType].rectInfo = tempRect;
    pipeInfo[isppNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[isppNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    if (m_parameters->increaseMaxBufferOfPreview() == true) {
        pipeInfo[isppNodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
    } else {
        pipeInfo[isppNodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;
    }

     /* per frame info */
    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[isppNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef USE_BUFFER_WITH_STRIDE
        /* to use stride for preview buffer, set the bytesPerPlane */
        pipeInfo[isppNodeType].bytesPerPlane[0] = hwPreviewW;
#endif


    ret = m_pipes[INDEX(PIPE_ISP)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* DIS */
    enum NODE_TYPE disNodeType = getNodeType(PIPE_DIS);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_DIS)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[PIPE_DIS].pipeId[i]);
    if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_DIS].pipeId[i], ret);
            return ret;
    }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_DIS)][i];
        secondarySensorIds[i] = m_secondarySensorIds[INDEX(PIPE_DIS)][i];
    }

    if (m_flagIspTpuOTF == true) {
        /* SRC */
        /* per frame info */
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[PIPE_DIS].secondaryNodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);
    } else {
        /* SRC */
        tempRect.fullW = hwPreviewW;
        tempRect.fullH = hwPreviewH;
        tempRect.colorFormat = hwVdisformat;

        pipeInfo[disNodeType].rectInfo = tempRect;
        pipeInfo[disNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[disNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[disNodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        pipeInfo[disNodeType].bytesPerPlane[0] = ROUND_UP(tempRect.fullW, 16);

        /* per frame info */
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[PIPE_DIS].nodeNum[disNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        /* SCP */
        int scpNodeType = getNodeType(PIPE_SCP);

        perFramePos = (m_cameraId == CAMERA_ID_BACK) ? PERFRAME_BACK_SCP_POS : PERFRAME_FRONT_SCP_POS;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
        pipeInfo[disNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[perFramePos].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_DIS)].nodeNum[scpNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        stride = m_parameters->getHwPreviewStride();
        CLOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, stride);
        tempRect.fullW = hwPreviewW;
        tempRect.fullH = hwPreviewH;
        tempRect.colorFormat = previewFormat;

        pipeInfo[scpNodeType].rectInfo = tempRect;
        pipeInfo[scpNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[scpNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        if (m_parameters->increaseMaxBufferOfPreview() == true) {
            pipeInfo[scpNodeType].bufInfo.count = m_parameters->getPreviewBufferCount();
        } else {
            pipeInfo[scpNodeType].bufInfo.count = config->current->bufInfo.num_preview_buffers;
        }

        /* per frame info */
        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[scpNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    }

    ret = m_pipes[INDEX(PIPE_DIS)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
        if (ret < 0) {
        CLOGE("ERR(%s[%d]):DIS setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

#ifdef SUPPORT_GROUP_MIGRATION
status_t ExynosCameraFrameFactory3aaIspTpu::m_initNodeInfo(void)
{
    CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
    int ispTpuOtf = m_parameters->isIspTpuOtf();

    struct nodeInfos *curNodeInfos = NULL;

    if (ispTpuOtf)
        curNodeInfos = m_nodeInfo;
    else
        curNodeInfos = m_nodeInfoTpu;

    m_curNodeInfo = curNodeInfos;

    m_pipes[INDEX(PIPE_3AA)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);
    m_pipes[INDEX(PIPE_ISP)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);
    m_pipes[INDEX(PIPE_DIS)]->getNodeInfos(&curNodeInfos[INDEX(PIPE_DIS)]);
    m_dumpNodeInfo("m_initNodeInfo PIPE_DIS", &curNodeInfos[INDEX(PIPE_DIS)]);
    curNodeInfos->isInitalize = true;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactory3aaIspTpu::m_updateNodeInfo(void)
{
    CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
    bool flagDirtyBayer = false;
    int ispTpuOtf = m_parameters->isIspTpuOtf();
    bool flagReset = false;
    if (m_supportReprocessing == true && m_supportPureBayerReprocessing == false)
        flagDirtyBayer = true;

    struct nodeInfos *curNodeInfos = NULL;
    struct nodeInfos *srcNodeInfos = NULL;

    if (ispTpuOtf) {
        curNodeInfos = m_nodeInfo;
        srcNodeInfos = m_nodeInfoTpu;
    } else {
        curNodeInfos = m_nodeInfoTpu;
        srcNodeInfos = m_nodeInfo;
    }

    if (m_curNodeInfo != curNodeInfos) {
        if (ispTpuOtf)
            CLOGD("DEBUG(%s[%d]): scenario change TPU ON -> OFF", __FUNCTION__, __LINE__);
        else
            CLOGD("DEBUG(%s[%d]): scenario change TPU OFF -> ON", __FUNCTION__, __LINE__);

        flagReset = true;
        m_curNodeInfo = curNodeInfos;
    }

    if (curNodeInfos->isInitalize == true) {
        CLOGD("DEBUG(%s[%d]): group migration is already done, use preset", __FUNCTION__, __LINE__);
        m_pipes[INDEX(PIPE_3AA)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)], flagReset);
        m_dumpNodeInfo("m_updateVideoNode PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);
        m_pipes[INDEX(PIPE_ISP)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)], flagReset);
        m_dumpNodeInfo("m_updateVideoNode PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);
        m_pipes[INDEX(PIPE_DIS)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_DIS)], flagReset);
        m_dumpNodeInfo("m_updateVideoNode PIPE_DIS", &curNodeInfos[INDEX(PIPE_DIS)]);
        return NO_ERROR;
    }

    if (ispTpuOtf == false) {
        // tpu off -> tpu on
        CLOGD("DEBUG(%s[%d]): TPU group migration OFF->ON", __FUNCTION__, __LINE__);

        // 3AS
        curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AA)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AA)];

        // 3AC
        if (flagDirtyBayer == true) {
            curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AC)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AC)];
        } else {
            curNodeInfos[INDEX(PIPE_3AA)].secondaryNode[m_getNodeTypeTpu(PIPE_3AC)] = srcNodeInfos[INDEX(PIPE_3AA)].secondaryNode[m_getNodeType(PIPE_3AC)];
        }

        // 3AP
        curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AP)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AP)];

        // ISPS
        curNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeTypeTpu(PIPE_ISP)] = srcNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeType(PIPE_ISP)];

        // ISPP
        curNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeTypeTpu(PIPE_ISPP)] = srcNodeInfos[INDEX(PIPE_ISP)].secondaryNode[m_getNodeType(PIPE_ISPP)];

        // SCP
        curNodeInfos[INDEX(PIPE_DIS)].node[m_getNodeTypeTpu(PIPE_SCP)] = srcNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeType(PIPE_SCP)];

        // DIS
        curNodeInfos[INDEX(PIPE_DIS)].node[m_getNodeTypeTpu(PIPE_DIS)] = srcNodeInfos[INDEX(PIPE_DIS)].secondaryNode[m_getNodeType(PIPE_DIS)];
    }  else {
        // tpu on -> tpu off
        CLOGD("DEBUG(%s[%d]): TPU group migration ON->OFF", __FUNCTION__, __LINE__);

        // 3AS
        curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AA)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AA)];

        // 3AC
        if (flagDirtyBayer == true) {
            curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AC)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AC)];
        } else {
            curNodeInfos[INDEX(PIPE_3AA)].secondaryNode[m_getNodeType(PIPE_3AC)] = srcNodeInfos[INDEX(PIPE_3AA)].secondaryNode[m_getNodeTypeTpu(PIPE_3AC)];
        }

        // 3AP
        curNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeType(PIPE_3AP)] = srcNodeInfos[INDEX(PIPE_3AA)].node[m_getNodeTypeTpu(PIPE_3AP)];

        // ISPS
        curNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeType(PIPE_ISP)] = srcNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeTypeTpu(PIPE_ISP)];

        // ISPP
        curNodeInfos[INDEX(PIPE_ISP)].secondaryNode[m_getNodeType(PIPE_ISPP)] = srcNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeTypeTpu(PIPE_ISPP)];

        // SCP
        curNodeInfos[INDEX(PIPE_ISP)].node[m_getNodeType(PIPE_SCP)] = srcNodeInfos[INDEX(PIPE_DIS)].node[m_getNodeTypeTpu(PIPE_SCP)];

        // DIS
        curNodeInfos[INDEX(PIPE_DIS)].secondaryNode[m_getNodeType(PIPE_DIS)] = srcNodeInfos[INDEX(PIPE_DIS)].node[m_getNodeTypeTpu(PIPE_DIS)];
    }

    m_pipes[INDEX(PIPE_3AA)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_3AA)], flagReset);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_3AA", &curNodeInfos[INDEX(PIPE_3AA)]);
    m_pipes[INDEX(PIPE_ISP)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_ISP)], flagReset);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_ISP", &curNodeInfos[INDEX(PIPE_ISP)]);
    m_pipes[INDEX(PIPE_DIS)]->setNodeInfos(&curNodeInfos[INDEX(PIPE_DIS)], flagReset);
    m_dumpNodeInfo("m_updateNodeInfo PIPE_DIS", &curNodeInfos[INDEX(PIPE_DIS)]);

    curNodeInfos->isInitalize = true;

    return NO_ERROR;
}
#endif

void ExynosCameraFrameFactory3aaIspTpu::m_init(void)
{
    CLOGD("DEBUG(%s[%d]): ", __FUNCTION__, __LINE__);
#ifdef SUPPORT_GROUP_MIGRATION
    memset(m_nodeInfoTpu, 0x00, sizeof(struct nodeInfos) * GROUP_MAX);
    memset(m_nodeInfo, 0x00, sizeof(struct nodeInfos) * GROUP_MAX);
    m_nodeInfoTpu[0].isInitalize = false;
    m_nodeInfo[0].isInitalize = false;
    m_curNodeInfo = NULL;
#endif
}

#ifdef SUPPORT_GROUP_MIGRATION
void ExynosCameraFrameFactory3aaIspTpu::m_dumpNodeInfo(const char* name, struct nodeInfos* nodeInfo)
{
    for(int i = 0 ; i < 20 ; i++) {
        if (nodeInfo->node[i] != NULL) {
            CLOGD("DEBUG(%s[%d]): %s mainNode[%d] = %d %p", __FUNCTION__, __LINE__, name, i, nodeInfo->node[i]->getNodeNum(), nodeInfo->node[i]);
        }

        if (nodeInfo->secondaryNode[i] != NULL) {
            CLOGD("DEBUG(%s[%d]): %s secondaryNode[%d] = %d %p", __FUNCTION__, __LINE__, name, i, nodeInfo->secondaryNode[i]->getNodeNum(), nodeInfo->secondaryNode[i]);
        }

    }
}
#endif

}; /* namespace android */
