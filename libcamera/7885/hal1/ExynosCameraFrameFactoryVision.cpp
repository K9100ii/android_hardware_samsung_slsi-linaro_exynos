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
#define LOG_TAG "ExynosCameraFrameFactoryVision"
#include <cutils/log.h>

#include "ExynosCameraFrameFactoryVision.h"

namespace android {

ExynosCameraFrameFactoryVision::~ExynosCameraFrameFactoryVision()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        CLOGE("destroy fail");
}

status_t ExynosCameraFrameFactoryVision::create(__unused bool active)
{
    Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());
    CLOGI("");
    int pipeId = -1;

    m_setupConfig();

    int ret = 0;

    pipeId = PIPE_FLITE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, false, &m_deviceInfo[pipeId]);
    m_pipes[pipeId]->setPipeName("PIPE_FLITE");
    m_pipes[pipeId]->setPipeId(pipeId);

    /* flite pipe initialize */
    ret = m_pipes[INDEX(PIPE_FLITE)]->create(m_sensorIds[pipeId]);
    if (ret < 0) {
        CLOGE("FLITE create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    CLOGD("Pipe(%d) created", pipeId);

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::destroy(void)
{
    CLOGI("");
    status_t ret = NO_ERROR;

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            ret = m_pipes[i]->destroy();
            if (ret != NO_ERROR) {
                CLOGE("m_pipes[%d]->destroy() fail", i);
                return ret;
            }

            SAFE_DELETE(m_pipes[i]);

            CLOGD("Pipe(%d) destroyed", i);
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::m_setupRequestFlags(void)
{
    /* Do nothing */
    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::m_fillNodeGroupInfo(__unused ExynosCameraFrameSP_sptr_t frame)
{
    /* Do nothing */
    size_control_info_t sizeControlInfo;
    camera2_node_group node_group_info_flite;
    camera2_node_group *node_group_info_temp;
    ExynosRect sensorSize;

    int zoom = m_parameters->getZoomLevel();
    int pipeId = -1;
    uint32_t perframePosition = 0;

    memset(&node_group_info_flite, 0x0, sizeof(camera2_node_group));

     /* FLITE */
    pipeId = PIPE_FLITE; // this is hack. (not PIPE_FLITE)
    perframePosition = 0;

    node_group_info_temp = &node_group_info_flite;
    node_group_info_temp->leader.request = 1;

    node_group_info_temp->capture[perframePosition].request = m_requestVC0;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_VC0)] - FIMC_IS_VIDEO_BAS_NUM;

    frame->setZoom(zoom);

    sensorSize.x = 0;
    sensorSize.y = 0;
    if (m_cameraId == CAMERA_ID_SECURE) {
        sensorSize.w = SECURE_CAMERA_WIDTH;
        sensorSize.h = SECURE_CAMERA_HEIGHT;
    } else {
        sensorSize.w = VISION_WIDTH;
        sensorSize.h = VISION_HEIGHT;
    }


    setLeaderSizeToNodeGroupInfo(&node_group_info_flite, sensorSize.x, sensorSize.y, sensorSize.w, sensorSize.h);
    setCaptureSizeToNodeGroupInfo(&node_group_info_flite, perframePosition, sensorSize.w, sensorSize.h);

    frame->storeNodeGroupInfo(&node_group_info_flite, PERFRAME_INFO_FLITE);

    return NO_ERROR;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameFactoryVision::createNewFrame(__unused ExynosCameraFrameSP_sptr_t refFrame)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES];

    ExynosCameraFrameSP_sptr_t frame = m_frameMgr->createFrame(m_parameters, m_frameCount);
    if (frame == NULL) {
        CLOGE("[F%d]Failed to createFrame. frameType %d", m_frameCount, FRAME_TYPE_PREVIEW);
        return NULL;
    }

    int requestEntityCount = 0;

    CLOGV("");

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        CLOGE("frame(%d) metadata initialize fail", m_frameCount);

    /* set flite pipe to linkageList */
    newEntity[INDEX(PIPE_FLITE)] = new ExynosCameraFrameEntity(PIPE_FLITE, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FLITE)]);
    requestEntityCount++;

    ret = m_initPipelines(frame);
    if (ret < 0) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    /* add perframe info for vision. */
    m_fillNodeGroupInfo(frame);

    /* TODO: make it dynamic */
    frame->setNumRequestPipe(requestEntityCount);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameFactoryVision::initPipes(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;
    int pipeId = -1;

    ExynosRect tempRect;
    int hwSensorW = 0, hwSensorH = 0;
    int bayerFormat = 0;
    int perFramePos = 0;
    uint32_t frameRate = 15;
    int bufferCount = 0;

    pipeId = PIPE_FLITE;

    if (m_cameraId == CAMERA_ID_SECURE) {
        hwSensorW = SECURE_CAMERA_WIDTH;
        hwSensorH = SECURE_CAMERA_HEIGHT;
        bayerFormat = V4L2_PIX_FMT_SBGGR8;
        bufferCount = RESERVED_NUM_SECURE_BUFFERS;
    } else {
        hwSensorW = VISION_WIDTH;
        hwSensorH = VISION_HEIGHT;
        bayerFormat = V4L2_PIX_FMT_SGRBG8;
        bufferCount = FRONT_NUM_BAYER_BUFFERS;
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
    pipeInfo[nodeType].bufInfo.count = bufferCount;

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
    pipeInfo[nodeType].bufInfo.count = bufferCount;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
    if (ret != NO_ERROR) {
        CLOGE("Flite setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_frameCount = 0;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::preparePipes(void)
{
    CLOGI("");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::startPipes(void)
{
    int ret = 0;

    CLOGI("");

    ret = m_pipes[INDEX(PIPE_FLITE)]->start();
    if (ret < 0) {
        CLOGE("FLITE start fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->prepare();
    if (ret < 0) {
        CLOGE("FLITE prepare fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(true);
    if (ret < 0) {
        CLOGE("FLITE sensorStream on fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    CLOGI("Starting Front [FLITE] Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::startSensor3AAPipe(void)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API",
            __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

status_t ExynosCameraFrameFactoryVision::startInitialThreads(void)
{
    int ret = 0;

    CLOGI("start pre-ordered initial pipe thread");

    ret = startThread(PIPE_FLITE);
    if (ret < 0)
        return ret;

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::setStopFlag(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    status_t ret = 0;

    ret = m_pipes[PIPE_FLITE]->setStopFlag();

    return NO_ERROR;
}

status_t ExynosCameraFrameFactoryVision::stopPipes(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    CLOGI("");

    if (m_pipes[PIPE_FLITE]->isThreadRunning() == true) {
        ret = m_pipes[PIPE_FLITE]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_FLITE stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->sensorStream(false);
    if (ret < 0) {
        CLOGE("FLITE sensorStream fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    /* PIPE_FLITE force done */
    ret = m_pipes[INDEX(PIPE_FLITE)]->forceDone(V4L2_CID_IS_FORCE_DONE, 0x1000);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_FLITE force done fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    ret = m_pipes[INDEX(PIPE_FLITE)]->stop();
    if (ret < 0) {
        CLOGE("FLITE stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    CLOGI("Stopping Front [FLITE] Success!");

    return funcRet;
}

void ExynosCameraFrameFactoryVision::setRequest3AC(__unused bool enable)
{
    /* Do nothing */

    return;
}

void ExynosCameraFrameFactoryVision::m_init(void)
{
    memset(m_nodeNums, -1, sizeof(m_nodeNums));
    memset(m_sensorIds, -1, sizeof(m_sensorIds));

    /* This seems all need to set 0 */
    m_requestVC0 = 1;
    m_request3AP = 0;
    m_request3AC = 0;
    m_requestISP = 0;
    m_requestSCC = 0;
    m_requestDIS = 0;
    m_requestSCP = 0;

    m_supportReprocessing = false;
    m_flagFlite3aaOTF = false;
    m_supportSCC = false;
    m_supportPureBayerReprocessing = false;
    m_flagReprocessing = false;

}

status_t ExynosCameraFrameFactoryVision::m_setupConfig(void)
{
    CLOGI("");

    bool enableSecure = false;
    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = true;
    bool flagReprocessing = false;
    int sensorScenario = 0;

    m_initDeviceInfo(PIPE_FLITE);

    if (m_cameraId == CAMERA_ID_SECURE)
        enableSecure = true;
    
    pipeId = PIPE_FLITE;
    nodeType = getNodeType(PIPE_FLITE);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_FLITE;
    if (m_cameraId == CAMERA_ID_SECURE) {
        m_deviceInfo[pipeId].nodeNum[nodeType] = SECURE_CAMERA_FLITE_NUM;
    } else {
        m_deviceInfo[pipeId].nodeNum[nodeType] = VISION_CAMERA_FLITE_NUM;
    }
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "FLITE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);

     if (enableSecure == true)
        sensorScenario = SENSOR_SCENARIO_SECURE;
     else
        sensorScenario = SENSOR_SCENARIO_VISION;

    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[nodeType], false, flagStreamLeader, flagReprocessing, sensorScenario);

    flagStreamLeader = false;

    /* VC0 for bayer */
    nodeType = getNodeType(PIPE_VC0);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VC0;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    m_deviceInfo[pipeId].nodeNum[nodeType] = getFliteCaptureNodenum(m_cameraId, m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)]);
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "BAYER", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_FLITE)], false, flagStreamLeader, flagReprocessing, sensorScenario);

    return NO_ERROR;
}

}; /* namespace android */
