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
#define LOG_TAG "ExynosCameraFrameFactory"
#include <log/log.h>

#include "ExynosCameraFrameFactory.h"

namespace android {

enum NODE_TYPE ExynosCameraFrameFactory::getNodeType(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;

    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_FLITE_REPROCESSING:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_VC0:
    case PIPE_VC0_REPROCESSING:
        nodeType = CAPTURE_NODE_1;
        break;
#ifdef SUPPORT_DEPTH_MAP
    case PIPE_VC1:
        nodeType = CAPTURE_NODE_2;
        break;
#endif
    case PIPE_3AA:
        if (m_flagFlite3aaOTF == HW_CONNECTION_MODE_M2M) {
            nodeType = OUTPUT_NODE;
        } else {
            nodeType = OTF_NODE_7;
        }
        break;
#ifdef USE_PAF
    case PIPE_3PAF_REPROCESSING:
        if (m_flagPaf3aaOTF == HW_CONNECTION_MODE_OTF) {
            nodeType = OUTPUT_NODE;
        } else {
            nodeType = OTF_NODE_4;
        }
        break;
#endif
    case PIPE_3AA_REPROCESSING:
        if (m_flagPaf3aaOTF == HW_CONNECTION_MODE_OTF) {
            nodeType = OTF_NODE_5;
        } else {
            nodeType = OUTPUT_NODE;
        }
        break;
    case PIPE_3AC:
    case PIPE_3AC_REPROCESSING:
        nodeType = CAPTURE_NODE_3;
        break;
    case PIPE_3AP:
    case PIPE_3AP_REPROCESSING:
        if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
            nodeType = CAPTURE_NODE_4;
        } else {
            nodeType = OTF_NODE_1;
        }
        break;
	case PIPE_3AF:
	case PIPE_3AF_REPROCESSING:
		nodeType = CAPTURE_NODE_24;
        break;

    case PIPE_3AG:
    case PIPE_3AG_REPROCESSING:
        nodeType = CAPTURE_NODE_25;
        break;

    case PIPE_ISP:
    case PIPE_ISP_REPROCESSING:
        if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
            nodeType = OUTPUT_NODE;
        } else {
            nodeType = OTF_NODE_2;
        }
        break;
    case PIPE_ISPC:
    case PIPE_ISPC_REPROCESSING:
        nodeType = CAPTURE_NODE_5;
        break;
    case PIPE_ISPP:
    case PIPE_ISPP_REPROCESSING:
        if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
#ifdef USE_DUAL_CAMERA
            || m_flagIspDcpOTF == HW_CONNECTION_MODE_M2M
#endif
                ) {
            nodeType = CAPTURE_NODE_6;
        } else {
            nodeType = OTF_NODE_3;
        }
        break;
    case PIPE_HWFC_JPEG_DST_REPROCESSING:
        nodeType = CAPTURE_NODE_7;
        break;
    case PIPE_HWFC_JPEG_SRC_REPROCESSING:
        nodeType = CAPTURE_NODE_8;
        break;
    case PIPE_HWFC_THUMB_SRC_REPROCESSING:
        nodeType = CAPTURE_NODE_9;
        break;
    case PIPE_HWFC_THUMB_DST_REPROCESSING:
        nodeType = CAPTURE_NODE_10;
        break;
    case PIPE_MCSC:
    case PIPE_MCSC_REPROCESSING:
        if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M
#ifdef USE_DUAL_CAMERA
            || m_flagDcpMcscOTF == HW_CONNECTION_MODE_M2M
#endif
           ) {
            nodeType = OUTPUT_NODE;
        } else {
            nodeType = OTF_NODE_6;
        }
        break;
    case PIPE_MCSC0:
    case PIPE_MCSC0_REPROCESSING:
        nodeType = CAPTURE_NODE_18;
        break;
    case PIPE_MCSC1:
    case PIPE_MCSC1_REPROCESSING:
        nodeType = CAPTURE_NODE_19;
        break;
    case PIPE_MCSC2:
    case PIPE_MCSC2_REPROCESSING:
        nodeType = CAPTURE_NODE_20;
        break;
    case PIPE_MCSC3_REPROCESSING:
        nodeType = CAPTURE_NODE_21;
        break;
    case PIPE_MCSC4_REPROCESSING:
        nodeType = CAPTURE_NODE_22;
        break;
    case PIPE_MCSC5:
    case PIPE_MCSC5_REPROCESSING:
        nodeType = CAPTURE_NODE_23;
        break;
    case PIPE_VRA:
    case PIPE_VRA_REPROCESSING:
    case PIPE_GDC:
    case PIPE_GSC_REPROCESSING3:
    case PIPE_GSC_REPROCESSING2:
    case PIPE_JPEG_REPROCESSING:
    case PIPE_JPEG:
#ifdef USE_DUAL_CAMERA
    case PIPE_FUSION:
    case PIPE_FUSION_REPROCESSING:
#endif
#ifdef SAMSUNG_TN_FEATURE
    case PIPE_PP_UNI_REPROCESSING:
    case PIPE_PP_UNI_REPROCESSING2:
    case PIPE_PP_UNI:
    case PIPE_PP_UNI2:
    case PIPE_PP_UNI3:
#endif
    case PIPE_GSC:
        nodeType = OUTPUT_NODE;
        break;
    case PIPE_ME:
        nodeType = CAPTURE_NODE_11;
        break;
    default:
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
             __FUNCTION__, __LINE__, pipeId);
        break;
    }

    return nodeType;
}

void ExynosCameraFrameFactory::connectPPScenario(int pipeId, int scenario)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Function is NOT implemented! pipeId %d scenario %d",
            __FUNCTION__, __LINE__, pipeId, scenario);
}

void ExynosCameraFrameFactory::extControl(int pipeId, int scenario, int controlType, __unused void *data)
{
    android_printAssert(NULL, LOG_TAG,
            "ASSERT(%s[%d]):Function is NOT implemented! pipeId %d scenario %d controlType %d",
            __FUNCTION__, __LINE__, pipeId, scenario, controlType);
}

void ExynosCameraFrameFactory::startPPScenario(int pipeId)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Function is NOT implemented! pipeId %d",
            __FUNCTION__, __LINE__, pipeId);
}

void ExynosCameraFrameFactory::stopPPScenario(int pipeId, __unused bool suspendFlag)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Function is NOT implemented! pipeId %d",
            __FUNCTION__, __LINE__, pipeId);
}

int ExynosCameraFrameFactory::getPPScenario(int pipeId)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Function is NOT implemented! pipeId %d",
            __FUNCTION__, __LINE__, pipeId);

    return -1;
}

status_t ExynosCameraFrameFactory::m_initFlitePipe(int sensorW, int sensorH, __unused uint32_t frameRate)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];

    int pipeId = PIPE_FLITE;

    if (m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA) != HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_3AA;
    }

    ExynosRect tempRect;

#ifdef DEBUG_RAWDUMP
    int bayerFormat = m_parameters->getBayerFormat(PIPE_FLITE);
    if (m_configurations->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    CLOGI("SensorSize(%dx%d)", sensorW, sensorH);

    /* set BNS ratio */
    int bnsScaleRatio = 1000;
    ret = m_pipes[pipeId]->setControl(V4L2_CID_IS_S_BNS, bnsScaleRatio);
    if (ret != NO_ERROR) {
        CLOGE("Set BNS(%.1f) fail, ret(%d)", (float)(bnsScaleRatio / 1000), ret);
    } else {
        int bnsSize = 0;

        ret = m_pipes[pipeId]->getControl(V4L2_CID_IS_G_BNS_SIZE, &bnsSize);
        if (ret != NO_ERROR) {
            CLOGE("Get BNS size fail, ret(%d)", ret);
        } else {
            int bnsWidth = bnsSize >> 16;
            int bnsHeight = bnsSize & 0xffff;
            CLOGI("BNS scale down ratio(%.1f), size (%dx%d)",
                     (float)(bnsScaleRatio / 1000), bnsWidth, bnsHeight);

            m_parameters->setSize(HW_INFO_HW_BNS_SIZE, bnsWidth, bnsHeight);
        }
    }

    return NO_ERROR;
}

int ExynosCameraFrameFactory::m_getSensorId(unsigned int nodeNum, unsigned int connectionMode,
                                             bool flagLeader, bool reprocessing, int sensorScenario)
{
    /* sub 100, and make index */
    nodeNum -= FIMC_IS_VIDEO_BAS_NUM;

    unsigned int reprocessingBit = 0;
    unsigned int leaderBit = 0;
    unsigned int sensorId = 0;

    if (reprocessing == true)
        reprocessingBit = 1;

    sensorId = m_cameraId;
    /* HACK
     * Driver detects Secure Camera ID as 3, not 4.
     */
    switch (sensorId) {
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        sensorId = 2;
        break;
#endif
    case CAMERA_ID_SECURE:
        sensorId = 3;
        break;
    default:
        break;
    }

    if (flagLeader == true)
        leaderBit = 1;

    if (sensorScenario < 0 ||SENSOR_SCENARIO_MAX <= sensorScenario) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Invalid sensorScenario(%d). assert!!!!",
            __FUNCTION__, __LINE__, sensorScenario);
    }

    return ((sensorScenario     << INPUT_SENSOR_SHIFT) & INPUT_SENSOR_MASK) |
           ((reprocessingBit    << INPUT_STREAM_SHIFT) & INPUT_STREAM_MASK) |
           ((sensorId           << INPUT_MODULE_SHIFT) & INPUT_MODULE_MASK) |
           ((nodeNum            << INPUT_VINDEX_SHIFT) & INPUT_VINDEX_MASK) |
           ((connectionMode     << INPUT_MEMORY_SHIFT) & INPUT_MEMORY_MASK) |
           ((leaderBit          << INPUT_LEADER_SHIFT) & INPUT_LEADER_MASK);
}

int ExynosCameraFrameFactory::m_getFliteNodenum(void)
{
    int fliteNodeNum = FIMC_IS_VIDEO_SS0_NUM;

    switch (m_cameraId) {
    case CAMERA_ID_BACK:
        fliteNodeNum = MAIN_CAMERA_FLITE_NUM;
        break;
    case CAMERA_ID_FRONT:
        fliteNodeNum = FRONT_CAMERA_FLITE_NUM;
        break;
#ifdef USE_DUAL_CAMERA
    case CAMERA_ID_BACK_1:
        fliteNodeNum = MAIN_1_CAMERA_FLITE_NUM;
        break;
    case CAMERA_ID_FRONT_1:
        fliteNodeNum = FRONT_1_CAMERA_FLITE_NUM;
        break;
#endif
       default:
        break;
    }

    return fliteNodeNum;
}

#ifdef SUPPORT_DEPTH_MAP
int ExynosCameraFrameFactory::m_getDepthVcNodeNum(void)
{
    int depthVcNodeNum = FIMC_IS_VIDEO_SS0VC1_NUM;

    depthVcNodeNum = (m_cameraId == CAMERA_ID_BACK) ? MAIN_CAMERA_DEPTH_VC_NUM : FRONT_CAMERA_DEPTH_VC_NUM;

    return depthVcNodeNum;
}
#endif

void ExynosCameraFrameFactory::m_init(void)
{
}

}; /* namespace android */
