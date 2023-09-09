/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraPipe"
#include <cutils/log.h>

#include "ExynosCameraPipe.h"

namespace android {

ExynosCameraPipe::~ExynosCameraPipe()
{
    /* don't call virtual function */
}

status_t ExynosCameraPipe::create(int32_t *sensorIds)
{
    CLOGD("");
    int ret = 0;

    for (int i = 0; i < MAX_NODE; i++) {
        if (sensorIds) {
            CLOGD("set new sensorIds[%d] : %d -> %d", i, m_sensorIds[i], sensorIds[i]);

            m_sensorIds[i] = sensorIds[i];
        } else {
            m_sensorIds[i] = -1;
        }
    }

    if (m_flagValidInt(m_nodeNum[OUTPUT_NODE]) == true) {
        m_node[OUTPUT_NODE] = new ExynosCameraNode();
        ret = m_node[OUTPUT_NODE]->create(m_name, m_cameraId);
        if (ret < 0) {
            CLOGE("OUTPUT_NODE create fail, ret(%d)", ret);
            return ret;
        }

        ret = m_node[OUTPUT_NODE]->open(m_nodeNum[OUTPUT_NODE]);
        if (ret < 0) {
            CLOGE("OUTPUT_NODE open fail, ret(%d)", ret);
            return ret;
        }
        CLOGD("Node(%d) opened", m_nodeNum[OUTPUT_NODE]);
    }

    /* mainNode is OUTPUT_NODE */
    m_mainNodeNum = OUTPUT_NODE;
    m_mainNode = m_node[m_mainNodeNum];

    /* setInput for 54xx */
    ret = m_setInput(m_node, m_nodeNum, m_sensorIds);
    if (ret < 0) {
        CLOGE(" m_setInput fail, ret(%d)", ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe::m_mainThreadFunc, "mainThread");

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = 0;
#ifdef USE_CAMERA2_API_SUPPORT
    m_timeLogCount = TIME_LOG_COUNT;
#endif
    CLOGI("create() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe::precreate(int32_t *sensorIds)
{
    CLOGD("");
    int ret = 0;

    for (int i = 0; i < MAX_NODE; i++) {
        if (sensorIds) {
            CLOGD("set new sensorIds[%d] : %d", i, sensorIds[i]);
            m_sensorIds[i] = sensorIds[i];
        } else {
            m_sensorIds[i] = -1;
        }
    }

    if (m_flagValidInt(m_nodeNum[OUTPUT_NODE]) == true) {
        m_node[OUTPUT_NODE] = new ExynosCameraNode();
        ret = m_node[OUTPUT_NODE]->create("main", m_cameraId);
        if (ret < 0) {
            CLOGE(" OUTPUT_NODE create fail, ret(%d)", ret);
            return ret;
        }

        ret = m_node[OUTPUT_NODE]->open(FIMC_IS_VIDEO_SS0_NUM);
        if (ret < 0) {
            CLOGE(" OUTPUT_NODE open fail, ret(%d)", ret);
            return ret;
        }
        CLOGD("Node(%d) opened", FIMC_IS_VIDEO_SS0_NUM);
    }

    /* mainNode is OUTPUT_NODE */
    m_mainNodeNum = OUTPUT_NODE;
    m_mainNode = m_node[m_mainNodeNum];

    CLOGI("precreate() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe::postcreate(int32_t *sensorIds)
{
    CLOGD("");
    int ret = 0;

    for (int i = 0; i < MAX_NODE; i++) {
        if (sensorIds) {
            CLOGD("set new sensorIds[%d] : %d", i, sensorIds[i]);
            m_sensorIds[i] = sensorIds[i];
        } else {
            m_sensorIds[i] = -1;
        }
    }

    ret = m_setInput(m_node, m_nodeNum, m_sensorIds);
    if (ret < 0) {
        CLOGE(" m_setInput fail, ret(%d)", ret);
        return ret;
    }

    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipe::m_mainThreadFunc, "mainThread");

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = 0;
    CLOGI("postcreate() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipe::destroy(void)
{
    CLOGD("");

    for (int i = MAX_NODE - 1; 0 <= i; i--) {
        if (m_node[i] != NULL) {
            if (m_node[i]->close() != NO_ERROR) {
                CLOGE(" close(%d) fail", i);
                return INVALID_OPERATION;
            }
            delete m_node[i];
            m_node[i] = NULL;
            CLOGD("Node(%d, m_nodeNum : %d, m_sensorIds : %d) closed",
                 i, m_nodeNum[i], m_sensorIds[i]);
        }
    }

    m_mainNodeNum = -1;
    m_mainNode = NULL;

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipe::setupPipe(camera_pipe_info_t *pipeInfos, int32_t *sensorIds)
{
    CLOGD("");
    status_t ret = NO_ERROR;

    /* TODO: check node state */

    /* set new sensorId to m_sensorIds */
    if (sensorIds) {
        CLOGD("set new sensorIds");

        for (int i = 0; i < MAX_NODE; i++)
            m_sensorIds[i] = sensorIds[i];
    }

    ret = m_setInput(m_node, m_nodeNum, m_sensorIds);
    if (ret < 0) {
        CLOGE(" m_setInput fail, ret(%d)", ret);
        return ret;
    }

    if (pipeInfos) {
        ret = m_setPipeInfo(pipeInfos);
        if (ret < 0) {
            CLOGE(" m_setPipeInfo fail, ret(%d)", ret);
            return ret;
        }
    }

    for(int i = 0; i < MAX_NODE; i++) {
        for (uint32_t j = 0; j < m_numBuffers; j++) {
            m_runningFrameList[j] = NULL;
            m_nodeRunningFrameList[i][j] = NULL;
        }
    }

    m_numOfRunningFrame = 0;

    m_prepareBufferCount = 0;
    CLOGI("setupPipe() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);
    return NO_ERROR;
}

status_t ExynosCameraPipe::setupPipe(__unused camera_pipe_info_t *pipeInfos, __unused int32_t *sensorIds, __unused int32_t *ispSensorIds)
{
    CLOGE(" unexpected api call. so, fail");
    return INVALID_OPERATION;
}

status_t ExynosCameraPipe::prepare(void)
{
    CLOGD("");
    status_t ret = NO_ERROR;

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (m_node[OUTPUT_NODE] != NULL &&
        m_isOtf(m_sensorIds[OUTPUT_NODE]) == false) {
        CLOGW(" prepare on m2m src is logically weird. so, skip.");
        return NO_ERROR;
    }

    for (uint32_t i = 0; i < m_prepareBufferCount; i++) {
        ret = m_putBuffer();
        if (ret < 0) {
            CLOGE(" m_putBuffer fail, ret(%d)", ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::prepare(uint32_t prepareCnt)
{
    CLOGD("prepareCnt:%d)", prepareCnt);
    status_t ret = NO_ERROR;

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (m_node[OUTPUT_NODE] != NULL &&
        m_isOtf(m_sensorIds[OUTPUT_NODE]) == false) {
        CLOGW(" prepare on m2m src is logically weird. so, skip.");
        return NO_ERROR;
    }

    for (uint32_t i = 0; i < prepareCnt; i++) {
        ret = m_putBuffer();
        if (ret < 0) {
            CLOGE(" m_putBuffer fail, ret(%d)", ret);
            return ret;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::start(void)
{
    CLOGV("");
    /* TODO: check state ready for start */

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->start();
    if (ret != NO_ERROR) {
        CLOGE(" Starting Node Error!");
        return ret;
    }

    m_threadState = 0;
    m_threadRenew = 0;
    m_threadCommand = 0;
    m_timeInterval = 0;

    m_flagStartPipe = true;
    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipe::stop(void)
{
    CLOGV("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    m_flagStartPipe = false;

    ret = m_mainNode->stop();
    if (ret < 0) {
        CLOGE(" node stop fail, ret(%d)", ret);
        return ret;
    }

    m_mainThread->requestExitAndWait();

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        CLOGE(" node clrBuffers fail, ret(%d)", ret);
        return ret;
    }

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    m_mainNode->removeItemBufferQ();

    for(int i = 0; i < MAX_NODE; i++) {
        for (uint32_t j = 0; j < m_numBuffers; j++) {
            m_runningFrameList[j] = NULL;
            m_nodeRunningFrameList[i][j] = NULL;
        }
    }

    m_numOfRunningFrame = 0;

    m_threadState = 0;
    m_threadRenew = 0;
    m_threadCommand = 0;
    m_timeInterval = 0;
    m_flagTryStop= false;

    return NO_ERROR;
}

bool ExynosCameraPipe::flagStart(void)
{
    return m_flagStartPipe;
}

status_t ExynosCameraPipe::startThread(void)
{
    CLOGI("IN");
    if (m_outputFrameQ == NULL) {
        CLOGE(" outputFrameQ is NULL, cannot start");
        return INVALID_OPERATION;
    }

    m_timer.start();
    if (m_mainThread->isRunning() == false) {
        m_mainThread->run(m_name);
        CLOGI("startThread is succeed Pipe(%d)", getPipeId());
    } else {
        CLOGW("startThread is already running (%d)", getPipeId());
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::stopThread(void)
{
    m_mainThread->requestExit();
    m_inputFrameQ->sendCmd(WAKE_UP);

    m_dumpRunningFrameList();

    return NO_ERROR;
}

bool ExynosCameraPipe::flagStartThread(void)
{
    return m_mainThread->isRunning();
}

status_t ExynosCameraPipe::stopThreadAndWait(int sleep, int times)
{
    CLOGD(" IN");
    status_t status = NO_ERROR;
    int i = 0;

    for (i = 0; i < times ; i++) {
        if (m_mainThread->isRunning() == false) {
            break;
        }
        usleep(sleep * 1000);
    }

    if (i >= times) {
        status = TIMED_OUT;
        CLOGE(" stopThreadAndWait failed, waitTime(%d)ms", sleep*times);
    }

    CLOGI(" OUT");
    return status;
}

status_t ExynosCameraPipe::sensorStream(bool on)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;
    int value = on ? IS_ENABLE_STREAM: IS_DISABLE_STREAM;

    ret = m_mainNode->setControl(V4L2_CID_IS_S_STREAM, value);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->sensorStream failed");

    return ret;
}

status_t ExynosCameraPipe::forceDone(unsigned int cid, int value)
{
    status_t ret = NO_ERROR;
    CLOGV("");
    if (m_mainNode == NULL) {
        CLOGE("m_mainNode is NULL");
        return INVALID_OPERATION;
    }

    ret = m_forceDone(m_mainNode, cid, value);
    if (ret != NO_ERROR)
        CLOGE("m_forceDone is failed");

    return ret;
}

status_t ExynosCameraPipe::setControl(int cid, int value)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->setControl(cid, value);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}

status_t ExynosCameraPipe::setControl(int cid, int value, __unused enum NODE_TYPE nodeType)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->setControl(cid, value);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}

status_t ExynosCameraPipe::getControl(int cid, int *value)
{
    CLOGV("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->getControl(cid, value);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->getControl failed");

    return ret;
}

status_t ExynosCameraPipe::setExtControl(struct v4l2_ext_controls *ctrl)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE("m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->setExtControl(ctrl);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}

status_t ExynosCameraPipe::setParam(struct v4l2_streamparm streamParam)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->setParam(&streamParam);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}

status_t ExynosCameraPipe::pushFrame(ExynosCameraFrameSP_dptr_t newFrame)
{
    Mutex::Autolock lock(m_pipeframeLock);
    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return BAD_VALUE;
    }

    m_inputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOn(__unused int32_t numFrames)
{
    CLOGD("");

    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer newBuffer;

    ret = m_mainNode->start();
    if (ret < 0) {
        CLOGE(" mainNode instantOn fail");
        return ret;
    }

    return ret;
}

status_t ExynosCameraPipe::instantOnQbuf(ExynosCameraFrameSP_dptr_t frame, BUFFER_POS::POS pos)
{
    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;
    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
            m_mainNode->dumpState();
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return INVALID_OPERATION;
    }

    if(pos == BUFFER_POS::DST)
        ret = newFrame->getDstBuffer(getPipeId(), &newBuffer);
    else if(pos == BUFFER_POS::SRC)
        ret = newFrame->getSrcBuffer(getPipeId(), &newBuffer);

    if (ret < 0) {
        CLOGE("frame get buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_runningFrameList[newBuffer.index] != NULL) {
        CLOGE("new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
             newBuffer.index, newFrame->getFrameCount());
        return BAD_VALUE;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[newBuffer.planeCount - 1]);

    if (shot_ext != NULL) {
        newFrame->getMetaData(shot_ext);
        m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&newBuffer);

        /* set metadata for instant on */
        shot_ext->shot.ctl.scaler.cropRegion[0] = 0;
        shot_ext->shot.ctl.scaler.cropRegion[1] = 0;
#if defined(FASTEN_AE_WIDTH) && defined(FASTEN_AE_HEIGHT)
        shot_ext->shot.ctl.scaler.cropRegion[2] = FASTEN_AE_WIDTH;
        shot_ext->shot.ctl.scaler.cropRegion[3] = FASTEN_AE_HEIGHT;
#else
        int bcropW = 0;
        int bcropH = 0;

        ret = m_parameters->getFastenAeStableBcropSize(&bcropW, &bcropH);
        if (ret != NO_ERROR) {
            CLOGE("m_parameters->getFastenAeStableBcropSize() fail on fastAE");
            return INVALID_OPERATION;
        }

        shot_ext->shot.ctl.scaler.cropRegion[2] = bcropW;
        shot_ext->shot.ctl.scaler.cropRegion[3] = bcropH;
#endif

        setMetaCtlAeTargetFpsRange(shot_ext, FASTEN_AE_FPS, FASTEN_AE_FPS);
        setMetaCtlSensorFrameDuration(shot_ext, (uint64_t)((1000 * 1000 * 1000) / (uint64_t)FASTEN_AE_FPS));

        /* set afMode into INFINITY */
        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
        shot_ext->shot.ctl.aa.vendor_afmode_option &= (0 << AA_AFMODE_OPTION_BIT_MACRO);

        if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            camera2_node_group node_group_info;
            memset(&shot_ext->node_group, 0x0, sizeof(camera2_node_group));
            newFrame->getNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex);

            /* Per - Leader */
            if (node_group_info.leader.request == 1) {

                if (m_checkNodeGroupInfo(m_mainNode->getName(), &m_curNodeGroupInfo.leader, &node_group_info.leader) != NO_ERROR)
                    CLOGW(" m_checkNodeGroupInfo(%s) fail", m_mainNode->getName());

                setMetaNodeLeaderInputSize(shot_ext,
                    node_group_info.leader.input.cropRegion[0],
                    node_group_info.leader.input.cropRegion[1],
                    node_group_info.leader.input.cropRegion[2],
                    node_group_info.leader.input.cropRegion[3]);
                setMetaNodeLeaderOutputSize(shot_ext,
                    node_group_info.leader.output.cropRegion[0],
                    node_group_info.leader.output.cropRegion[1],
                    node_group_info.leader.output.cropRegion[2],
                    node_group_info.leader.output.cropRegion[3]);
                setMetaNodeLeaderRequest(shot_ext,
                    node_group_info.leader.request);
                setMetaNodeLeaderVideoID(shot_ext,
                    m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID);
            }

            /* Per - Captures */
            for (int i = 0; i < m_perframeMainNodeGroupInfo.perframeSupportNodeNum - 1; i ++) {
                if (node_group_info.capture[i].request == 1) {

                    if (m_checkNodeGroupInfo(i, &m_curNodeGroupInfo.capture[i], &node_group_info.capture[i]) != NO_ERROR)
                        CLOGW(" m_checkNodeGroupInfo(%d) fail", i);

                    setMetaNodeCaptureInputSize(shot_ext, i,
                        node_group_info.capture[i].input.cropRegion[0],
                        node_group_info.capture[i].input.cropRegion[1],
                        node_group_info.capture[i].input.cropRegion[2],
                        node_group_info.capture[i].input.cropRegion[3]);
                    setMetaNodeCaptureOutputSize(shot_ext, i,
                        node_group_info.capture[i].output.cropRegion[0],
                        node_group_info.capture[i].output.cropRegion[1],
                        node_group_info.capture[i].output.cropRegion[2],
                        node_group_info.capture[i].output.cropRegion[3]);
                    setMetaNodeCaptureRequest(shot_ext,  i, node_group_info.capture[i].request);
                    setMetaNodeCaptureVideoID(shot_ext, i, m_perframeMainNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                }
            }
        }
    }
    ret = m_mainNode->putBuffer(&newBuffer);
    if (ret < 0) {
        CLOGE("putBuffer fail");
        return ret;
        /* TODO: doing exception handling */
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE(" setDstBuffer state fail");
        return ret;
    }

    m_runningFrameList[newBuffer.index] = newFrame;

    m_numOfRunningFrame++;

    frame = newFrame;

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOnDQbuf(ExynosCameraFrameSP_dptr_t frame, __unused BUFFER_POS::POS pos)
{
    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;

    if (m_numOfRunningFrame <= 0 ) {
        CLOGD(" skip getBuffer, numOfRunningFrame = %d", m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_mainNode->getBuffer(&curBuffer, &index);
    if (ret < 0) {
        CLOGE("getBuffer fail");
        /* TODO: doing exception handling */
        return ret;
    }

    if (index < 0) {
        CLOGE("Invalid index(%d) fail", index);
        return INVALID_OPERATION;
    }

    m_activityControl->activityAfterExecFunc(getPipeId(), (void *)&curBuffer);

    ret = m_updateMetadataToFrame(curBuffer.addr[curBuffer.planeCount - 1], curBuffer.index);
    if (ret < 0)
        CLOGE(" updateMetadataToFrame fail, ret(%d)", ret);


    if (curBuffer.index < 0) {
        CLOGE("index(%d) is invalid", curBuffer.index);
        return BAD_VALUE;
    }

    curFrame = m_runningFrameList[curBuffer.index];

    if (curFrame == NULL) {
        CLOGE("Unknown buffer, frame is NULL");
        dump();
        return BAD_VALUE;
    }

    frame = curFrame;

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOff(void)
{
    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int ret = 0;

    ret = m_mainNode->stop();

    ret = m_mainNode->clrBuffers();
    if (ret < 0) {
        CLOGE("3AA output node clrBuffers fail, ret(%d)", ret);
        return ret;
    }

    for(int i = 0; i < MAX_NODE; i++) {
        for (int j = 0; j < MAX_BUFFERS; j++) {
            m_runningFrameList[j] = NULL;
            m_nodeRunningFrameList[i][j] = NULL;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::instantOnPushFrameQ(BUFFERQ_TYPE::TYPE type, ExynosCameraFrameSP_dptr_t frame)
{
    if( type == BUFFERQ_TYPE::OUTPUT ) {
        m_outputFrameQ->pushProcessQ(&frame);
    } else {
        m_inputFrameQ->pushProcessQ(&frame);
    }
    return NO_ERROR;
}

status_t ExynosCameraPipe::getPipeInfo(int *fullW, int *fullH, int *colorFormat, __unused int pipePosition)
{
    if (m_mainNode == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int planeCount = 0;
    int ret = NO_ERROR;

    ret = m_mainNode->getSize(fullW, fullH);
    if (ret < 0) {
        CLOGE("getSize fail");
        return ret;
    }

    ret = m_mainNode->getColorFormat(colorFormat, &planeCount);
    if (ret < 0) {
        CLOGE("getColorFormat fail");
        return ret;
    }

    return ret;
}

int ExynosCameraPipe::getCameraId(void)
{
    return this->m_cameraId;
}

status_t ExynosCameraPipe::setPipeId(uint32_t id)
{
    this->m_pipeId = id;

    return NO_ERROR;
}

uint32_t ExynosCameraPipe::getPipeId(void)
{
    return this->m_pipeId;
}

int ExynosCameraPipe::getPipeId(__unused enum NODE_TYPE nodeType)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API. use MCPipe, assert!!!!", __FUNCTION__, __LINE__);

    return -1;
}

status_t ExynosCameraPipe::setPipeId(__unused enum NODE_TYPE nodeType, __unused uint32_t id)
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Not supported API. use MCPipe, assert!!!!", __FUNCTION__, __LINE__);

    return INVALID_OPERATION;
}

status_t ExynosCameraPipe::setPipeName(const char *pipeName)
{
    strncpy(m_name,  pipeName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    return NO_ERROR;
}

status_t ExynosCameraPipe::setBufferManager(ExynosCameraBufferManager **bufferManager)
{
    for (int i = 0; i < MAX_NODE; i++)
        m_bufferManager[i] = bufferManager[i];

    return NO_ERROR;
}

char *ExynosCameraPipe::getPipeName(void)
{
    return m_name;
}

status_t ExynosCameraPipe::clearInputFrameQ(void)
{
    if (m_inputFrameQ != NULL)
        m_inputFrameQ->release();

    return NO_ERROR;
}

status_t ExynosCameraPipe::getInputFrameQ(frame_queue_t **inputFrameQ)
{
    *inputFrameQ = m_inputFrameQ;

    if (*inputFrameQ == NULL)
        CLOGE("inputFrameQ is NULL");

    return NO_ERROR;
}

status_t ExynosCameraPipe::setOutputFrameQ(frame_queue_t *outputFrameQ)
{
    m_outputFrameQ = outputFrameQ;
    return NO_ERROR;
}

status_t ExynosCameraPipe::getOutputFrameQ(frame_queue_t **outputFrameQ)
{
    *outputFrameQ = m_outputFrameQ;

    if (*outputFrameQ == NULL)
        CLOGE("outputFrameQ is NULL");

    return NO_ERROR;
}

status_t ExynosCameraPipe::getFrameDoneQ(frame_queue_t **frameDoneQ)
{
    *frameDoneQ = m_frameDoneQ;

    if (*frameDoneQ == NULL)
        CLOGE("frameDoneQ is NULL");

    return NO_ERROR;
}

status_t ExynosCameraPipe::setFrameDoneQ(frame_queue_t *frameDoneQ)
{
    m_frameDoneQ = frameDoneQ;
    m_flagFrameDoneQ = true;

    return NO_ERROR;
}

status_t ExynosCameraPipe::setNodeInfos(__unused camera_node_objects_t *nodeObjects, __unused bool flagReset)
{
    CLOGD("do not support SUPPORT_GROUP_MIGRATION");
    return NO_ERROR;
}

status_t ExynosCameraPipe::getNodeInfos(__unused camera_node_objects_t *nodeObjects)
{
    CLOGD("do not support SUPPORT_GROUP_MIGRATION");
    return NO_ERROR;
}

status_t ExynosCameraPipe::setMapBuffer(__unused ExynosCameraBuffer *srcBuf, __unused ExynosCameraBuffer *dstBuf)
{
    return NO_ERROR;
}

status_t ExynosCameraPipe::setBoosting(bool isBoosting)
{
    m_isBoosting = isBoosting;

    return NO_ERROR;
}

bool ExynosCameraPipe::isThreadRunning()
{
    return m_mainThread->isRunning();
}

status_t ExynosCameraPipe::getThreadState(int **threadState)
{
    *threadState = &m_threadState;

    return NO_ERROR;
}

status_t ExynosCameraPipe::getThreadInterval(uint64_t **timeInterval)
{
    *timeInterval = &m_timeInterval;

    return NO_ERROR;
}

status_t ExynosCameraPipe::getThreadRenew(int **timeRenew)
{
    *timeRenew = &m_threadRenew;

    return NO_ERROR;
}

status_t ExynosCameraPipe::incThreadRenew(void)
{
    m_threadRenew ++;

    return NO_ERROR;
}

status_t ExynosCameraPipe::setStopFlag(void)
{
    CLOGD("");

    m_flagTryStop = true;

    return NO_ERROR;
}

int ExynosCameraPipe::getRunningFrameCount(void)
{
    int runningFrameCount = 0;

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        if (m_runningFrameList[i] != NULL) {
            runningFrameCount++;
        }
    }

    return runningFrameCount;
}

void ExynosCameraPipe::setOneShotMode(bool enable)
{
    CLOGI("%s %s OneShot mode",
             m_name,
            (enable == true)? "Enable" : "Disable");

    m_oneShotMode = enable;
}

#ifdef USE_MCPIPE_SERIALIZATION_MODE
void ExynosCameraPipe::needSerialization(__unused bool enable)
{
    CLOGD("do not support %s()", __FUNCTION__);
}
#endif

void ExynosCameraPipe::dump(void)
{
    CLOGI("");

    m_dumpRunningFrameList();

    if (m_mainNode != NULL)
        m_mainNode->dump();

    return;
}

status_t ExynosCameraPipe::dumpFimcIsInfo(bool bugOn)
{
    CLOGD("");
    int ret = 0;

    ret = m_mainNode->setControl(V4L2_CID_IS_DEBUG_DUMP, bugOn);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}

//#ifdef MONITOR_LOG_SYNC
status_t ExynosCameraPipe::syncLog(uint32_t syncId)
{
    CLOGD("");
    int ret = 0;

    ret = m_mainNode->setControl(V4L2_CID_IS_DEBUG_SYNC_LOG, syncId);
    if (ret != NO_ERROR)
        CLOGE("m_mainNode->setControl failed");

    return ret;
}
//#endif

bool ExynosCameraPipe::m_mainThreadFunc(void)
{
    int ret = 0;

    /* TODO: check exit condition */
    /*       running list != empty */

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_getBuffer();
    if (ret < 0) {
        CLOGE(" m_getBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return false;
    }

    ret = m_putBuffer();
    if (ret < 0) {
        if (ret == TIMED_OUT)
            return true;

        CLOGE(" m_putBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return false;
    }

    return true;
}

status_t ExynosCameraPipe::m_putBuffer(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    int ret = 0;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (m_flagTryStop == true) {
        CLOGD("m_flagTryStop(%d)", m_flagTryStop);
        return NO_ERROR;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
#ifdef USE_CAMERA2_API_SUPPORT
            /*
                      * TIMEOUT log print
                      * condition 1 : it is not reprocessing
                      * condition 2 : if it is reprocessing, but m_timeLogCount is equals or lower than 0
                      */
            if (!(m_parameters->isReprocessing() == true && m_timeLogCount <= 0)) {
                m_timeLogCount--;
#endif
            CLOGW("wait timeout");
            m_mainNode->dumpState();
#ifdef USE_CAMERA2_API_SUPPORT
            }
#endif
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("newFrame is NULL");
        return INVALID_OPERATION;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &newBuffer);
    if (ret < 0) {
        CLOGE("frame get buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    if (m_runningFrameList[newBuffer.index] != NULL) {
        CLOGE("new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
             newBuffer.index, newFrame->getFrameCount());
        m_dumpRunningFrameList();

        return BAD_VALUE;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(newBuffer.addr[newBuffer.planeCount - 1]);
    if (shot_ext != NULL) {
        newFrame->getMetaData(shot_ext);
        if (m_parameters->getHalVersion() != IS_HAL_VER_3_2)
            m_parameters->duplicateCtrlMetadata((void *)shot_ext);
        m_activityControl->activityBeforeExecFunc(getPipeId(), (void *)&newBuffer);

        if (m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType == PERFRAME_NODE_TYPE_LEADER) {
            camera2_node_group node_group_info;
            memset(&shot_ext->node_group, 0x0, sizeof(camera2_node_group));
            newFrame->getNodeGroupInfo(&node_group_info, m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex);

            /* Per - Leader */
            if (node_group_info.leader.request == 1) {

                if (m_checkNodeGroupInfo(m_mainNode->getName(), &m_curNodeGroupInfo.leader, &node_group_info.leader) != NO_ERROR)
                    CLOGW(" m_checkNodeGroupInfo(leader) fail");

                setMetaNodeLeaderInputSize(shot_ext,
                    node_group_info.leader.input.cropRegion[0],
                    node_group_info.leader.input.cropRegion[1],
                    node_group_info.leader.input.cropRegion[2],
                    node_group_info.leader.input.cropRegion[3]);
                setMetaNodeLeaderOutputSize(shot_ext,
                    node_group_info.leader.output.cropRegion[0],
                    node_group_info.leader.output.cropRegion[1],
                    node_group_info.leader.output.cropRegion[2],
                    node_group_info.leader.output.cropRegion[3]);
                setMetaNodeLeaderRequest(shot_ext,
                    node_group_info.leader.request);
                setMetaNodeLeaderVideoID(shot_ext,
                    m_perframeMainNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID);
            }

            /* Per - Captures */
            for (int i = 0; i < m_perframeMainNodeGroupInfo.perframeSupportNodeNum - 1; i ++) {
                if (node_group_info.capture[i].request == 1) {

                    if (m_checkNodeGroupInfo(i, &m_curNodeGroupInfo.capture[i], &node_group_info.capture[i]) != NO_ERROR)
                        CLOGW(" m_checkNodeGroupInfo(%d) fail", i);

                    setMetaNodeCaptureInputSize(shot_ext, i,
                        node_group_info.capture[i].input.cropRegion[0],
                        node_group_info.capture[i].input.cropRegion[1],
                        node_group_info.capture[i].input.cropRegion[2],
                        node_group_info.capture[i].input.cropRegion[3]);
                    setMetaNodeCaptureOutputSize(shot_ext, i,
                        node_group_info.capture[i].output.cropRegion[0],
                        node_group_info.capture[i].output.cropRegion[1],
                        node_group_info.capture[i].output.cropRegion[2],
                        node_group_info.capture[i].output.cropRegion[3]);
                    setMetaNodeCaptureRequest(shot_ext,  i, node_group_info.capture[i].request);
                    setMetaNodeCaptureVideoID(shot_ext, i, m_perframeMainNodeGroupInfo.perFrameCaptureInfo[i].perFrameVideoID);
                }
            }
        }
    }

    ret = m_mainNode->putBuffer(&newBuffer);
    if (ret < 0) {
        CLOGE("putBuffer fail");
        return ret;
        /* TODO: doing exception handling */
    }

    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING);
    if (ret < 0) {
        CLOGE(" setDstBuffer state fail");
        return ret;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (getPipeId() == PIPE_FLITE && newFrame->getRequest(PIPE_VC1) == true) {
        ExynosCameraBuffer depthMapBuffer;
        ret = newFrame->getDstBuffer(getPipeId(), &depthMapBuffer, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get CAPTURE_NODE_2 buffer. ret %d",
                     ret);
            return OK;
        }

        ret = m_node[CAPTURE_NODE_2]->putBuffer(&depthMapBuffer);
        if (ret < 0) {
            CLOGE("putBuffer fail");
            return ret;
            /* TODO: doing exception handling */
        }

        ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_PROCESSING, CAPTURE_NODE_2);
        if (ret < 0) {
            CLOGE(" setDstBuffer state fail");
            return ret;
        }
    }
#endif

    m_runningFrameList[newBuffer.index] = newFrame;
    m_numOfRunningFrame++;

#ifdef USE_CAMERA2_API_SUPPORT
        m_timeLogCount = TIME_LOG_COUNT;
#endif

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_getBuffer(void)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        CLOGD(" skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d",
                 m_flagStartPipe, m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_mainNode->getBuffer(&curBuffer, &index);
    if (ret < 0) {
        CLOGE("getBuffer fail");
        /* TODO: doing exception handling */
        return ret;
    }

    if (index < 0) {
        CLOGE("Invalid index(%d) fail", index);
        return INVALID_OPERATION;
    }

    m_activityControl->activityAfterExecFunc(getPipeId(), (void *)&curBuffer);

    ret = m_updateMetadataToFrame(curBuffer.addr[curBuffer.planeCount - 1], curBuffer.index);
    if (ret < 0)
        CLOGE(" updateMetadataToFrame fail, ret(%d)", ret);

    /* complete frame */
    ret = m_completeFrame(curFrame, curBuffer);
    if (ret < 0) {
        CLOGE("m_comleteFrame fail");
        /* TODO: doing exception handling */
    }

    if (curFrame == NULL) {
        CLOGE("curFrame is fail");
    }

    m_outputFrameQ->pushProcessQ(&curFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_updateMetadataToFrame(void *metadata, int index)
{
    int ret = 0;
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    camera2_shot_ext *shot_ext;
    shot_ext = (struct camera2_shot_ext *)metadata;
    if (shot_ext == NULL) {
        CLOGE(" metabuffer is null");
        return BAD_VALUE;
    }
    if (index < 0) {
        CLOGE(" Invalid index(%d)", index);
        return BAD_VALUE;
    }

    if (m_metadataTypeShot == false) {
        CLOGV(" stream type do not need update metadata");
        return NO_ERROR;
    }

    ret = m_getFrameByIndex(curFrame, index);
    if (ret < 0) {
        CLOGE(" m_getFrameByIndex fail, index(%d), ret(%d)", index, ret);
        return ret;
    }

    ret = curFrame->storeDynamicMeta(shot_ext);
    if (ret < 0) {
        CLOGE(" storeDynamicMeta fail ret(%d)", ret);
        return ret;
    }

    ret = curFrame->storeUserDynamicMeta(shot_ext);
    if (ret < 0) {
        CLOGE(" storeUserDynamicMeta fail ret(%d)", ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_getFrameByIndex(ExynosCameraFrameSP_dptr_t frame, int index)
{
    frame = m_runningFrameList[index];
    if (frame == NULL) {
        CLOGE("Unknown buffer, index %d frame is NULL", index);
        dump();
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_completeFrame(
        ExynosCameraFrameSP_dptr_t frame,
        ExynosCameraBuffer buffer,
        bool isValid)
{
    int ret = 0;

    if (buffer.index < 0) {
        CLOGE("index(%d) is invalid", buffer.index);
        return BAD_VALUE;
    }

    frame = m_runningFrameList[buffer.index];

    if (frame == NULL) {
        CLOGE("Unknown buffer, frame is NULL");
        dump();
        return BAD_VALUE;
    }

    if (isValid == false) {
        CLOGD("NOT DONE frameCount %d, buffer index(%d)",
                frame->getFrameCount(), buffer.index);
    }

    ret = frame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    CLOGV("entity pipeId(%d), buffer index(%d), frameCount(%d)",
            getPipeId(), buffer.index,
            m_runningFrameList[buffer.index]->getFrameCount());

    m_runningFrameList[buffer.index] = NULL;
    m_numOfRunningFrame--;

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_setInput(ExynosCameraNode *nodes[], int32_t *nodeNums, int32_t *sensorIds)
{
    status_t ret = NO_ERROR;
    int currentSensorId[MAX_NODE] = {0, };

    if (nodeNums == NULL || sensorIds == NULL) {
        CLOGE(" nodes == %p || nodeNum == %p || sensorId == %p",
                 nodes, nodeNums, sensorIds);
        return INVALID_OPERATION;
    }

    for (int i = 0; i < MAX_NODE; i++) {
        if (m_flagValidInt(nodeNums[i]) == false)
            continue;

        if (m_flagValidInt(sensorIds[i]) == false)
            continue;

        if (nodes[i] == NULL)
            continue;

        currentSensorId[i] = nodes[i]->getInput();

        if (m_flagValidInt(currentSensorId[i]) == false ||
            currentSensorId[i] != sensorIds[i]) {

#ifdef FIMC_IS_VIDEO_BAS_NUM
            CLOGD(" setInput(sensorIds : %d) [src nodeNum : %d][nodeNums : %d]\
                [otf : %d][leader : %d][reprocessing : %d][unique sensorId : %d]",
                 sensorIds[i],
                ((sensorIds[i] & INPUT_VINDEX_MASK) >>  INPUT_VINDEX_SHIFT) + FIMC_IS_VIDEO_BAS_NUM,
                nodeNums[i],
                ((sensorIds[i] & INPUT_MEMORY_MASK) >>  INPUT_MEMORY_SHIFT),
                ((sensorIds[i] & INPUT_LEADER_MASK) >>  INPUT_LEADER_SHIFT),
                ((sensorIds[i] & INPUT_STREAM_MASK) >>  INPUT_STREAM_SHIFT),
                ((sensorIds[i] & INPUT_MODULE_MASK) >>  INPUT_MODULE_SHIFT));
#else
            CLOGD(" setInput(sensorIds : %d)",
                 sensorIds[i]);
#endif
#ifdef SAMSUNG_QUICK_SWITCH
            if (nodeNums[1] == FIMC_IS_VIDEO_SS4_NUM || nodeNums[1] == FIMC_IS_VIDEO_SS5_NUM) {
                if (m_parameters->getQuickSwitchCmd() == QUICK_SWITCH_CMD_IDLE_TO_STBY) {
                    ret = nodes[i]->setInput(sensorIds[i] | (SENSOR_SCENARIO_STANDBY << SCENARIO_SHIFT));
                } else {
                    /* Skip the setInput for this node in the switching case */
                }
            } else {
                ret = nodes[i]->setInput(sensorIds[i]);
            }
#else
            ret = nodes[i]->setInput(sensorIds[i]);
#endif
            if (ret < 0) {
                CLOGE(" nodeNums[%d] : %d, setInput(sensorIds : %d fail, ret(%d)",
                         i, nodeNums[i], sensorIds[i],
                        ret);

                return ret;
            }
        }
    }

    return ret;
}

status_t ExynosCameraPipe::m_setPipeInfo(camera_pipe_info_t *pipeInfos)
{
    CLOGD("");

    status_t ret = NO_ERROR;
    uint32_t planeCount = 2;
    uint32_t bytePerPlane = 0;

    if (pipeInfos == NULL) {
        CLOGE(" pipeInfos == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int colorFormat = pipeInfos[0].rectInfo.colorFormat;

    getYuvFormatInfo(colorFormat, &bytePerPlane, &planeCount);
    /* Add medadata plane count */
    planeCount++;

    ret = m_setNodeInfo(m_mainNode, &pipeInfos[0],
                        planeCount, YUV_FULL_RANGE);

    if (ret != NO_ERROR) {
        CLOGE(" m_setNodeInfo(%d, %d, %d) fail",
             pipeInfos[0].rectInfo.fullW, pipeInfos[0].rectInfo.fullH, pipeInfos[0].bufInfo.count);
        return INVALID_OPERATION;
    }

    m_perframeMainNodeGroupInfo = pipeInfos[0].perFrameNodeGroupInfo;
    m_numBuffers = pipeInfos[0].bufInfo.count;

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_setNodeInfo(ExynosCameraNode *node, camera_pipe_info_t *pipeInfos,
                                         int planeCount, enum YUV_RANGE yuvRange,
                                         __unused bool flagBayer)
{
    CLOGD("");

    if (node == NULL) {
        CLOGE(" node == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (pipeInfos == NULL) {
        CLOGE(" pipeInfos == NULL. so, fail");
        return INVALID_OPERATION;
    }

    bool flagSetRequest = false;
    unsigned int requestBufCount = 0;
    int batchSize = 1;

    int currentW = 0;
    int currentH = 0;

    int currentV4l2Colorformat = 0;
    int currentPlanesCount = 0;
    enum YUV_RANGE currentYuvRange = YUV_FULL_RANGE;

    int  currentBufferCount = 0;
    enum v4l2_buf_type currentBufType;
    enum v4l2_memory currentMemType;

    requestBufCount = node->reqBuffersCount();

    /* If it already set */
    if (0 < requestBufCount) {
        node->getSize(&currentW, &currentH);
        node->getColorFormat(&currentV4l2Colorformat, &currentPlanesCount, &currentYuvRange);
        node->getBufferType(&currentBufferCount, &currentBufType, &currentMemType);

        if (/* setSize */
            currentW               != pipeInfos->rectInfo.fullW ||
            currentH               != pipeInfos->rectInfo.fullH ||
            /* setColorFormat */
            currentV4l2Colorformat != pipeInfos->rectInfo.colorFormat ||
            currentPlanesCount     != planeCount ||
            currentYuvRange        != yuvRange ||
            /* setBufferType */
            currentBufferCount     != (int)pipeInfos->bufInfo.count ||
            currentBufType         != (enum v4l2_buf_type)pipeInfos->bufInfo.type ||
            currentMemType         != (enum v4l2_memory)pipeInfos->bufInfo.memory) {

            flagSetRequest = true;

            CLOGW(" node is already requested. so, call clrBuffers()");

            CLOGW(" w (%d -> %d), h (%d -> %d)",
                currentW, pipeInfos->rectInfo.fullW,
                currentH, pipeInfos->rectInfo.fullH);

            CLOGW(" colorFormat (%d -> %d), planeCount (%d -> %d), yuvRange (%d -> %d)",
                currentV4l2Colorformat, pipeInfos->rectInfo.colorFormat,
                currentPlanesCount,     planeCount,
                currentYuvRange,        yuvRange);

            CLOGW(" bufferCount (%d -> %d), bufType (%d -> %d), memType (%d -> %d)",
                currentBufferCount, pipeInfos->bufInfo.count,
                currentBufType,     pipeInfos->bufInfo.type,
                currentMemType,     pipeInfos->bufInfo.memory);

            if (node->clrBuffers() != NO_ERROR) {
                CLOGE(" node->clrBuffers() fail");
                return INVALID_OPERATION;
            }
        }
    } else {
        flagSetRequest = true;
    }

    if (flagSetRequest == true) {
        CLOGD(" set pipeInfos on %s : setFormat(%d, %d) and reqBuffers(%d).",
             node->getName(), pipeInfos->rectInfo.fullW,
            pipeInfos->rectInfo.fullH, pipeInfos->bufInfo.count);

        bool flagValidSetFormatInfo = true;

        if (pipeInfos->rectInfo.fullW == 0 || pipeInfos->rectInfo.fullH == 0) {
            CLOGW(" invalid size (%d x %d). So, skip setSize()",
             pipeInfos->rectInfo.fullW, pipeInfos->rectInfo.fullH);
            flagValidSetFormatInfo = false;
        }
        node->setSize(pipeInfos->rectInfo.fullW, pipeInfos->rectInfo.fullH);

        if (pipeInfos->rectInfo.colorFormat == 0 || planeCount == 0) {
            CLOGW(" invalid colorFormat(%d), planeCount(%d). So, skip setColorFormat()",
                 pipeInfos->rectInfo.colorFormat, planeCount);
            flagValidSetFormatInfo = false;
        }
        batchSize = m_parameters->getBatchSize((enum pipeline)getPipeId());
        node->setColorFormat(pipeInfos->rectInfo.colorFormat, planeCount, batchSize, yuvRange);

        if ((int)pipeInfos->bufInfo.type == 0 || pipeInfos->bufInfo.memory == 0) {
            CLOGW(" invalid bufInfo.type(%d), bufInfo.memory(%d). So, skip setBufferType()",
                 (int)pipeInfos->bufInfo.type, (int)pipeInfos->bufInfo.memory);
            flagValidSetFormatInfo = false;
        }
        node->setBufferType(pipeInfos->bufInfo.count, (enum v4l2_buf_type)pipeInfos->bufInfo.type,
                            (enum v4l2_memory)pipeInfos->bufInfo.memory);

        if (flagValidSetFormatInfo == true) {
#ifdef SAMSUNG_DNG
            if (m_parameters->getDNGCaptureModeOn() && getPipeId() == PIPE_FLITE) {
                CLOGV(" DNG flite node->setFormat() getPipeId()(%d)",getPipeId());
                if (node->setFormat() != NO_ERROR) {
                    CLOGE(" node->setFormat() fail");
                    return INVALID_OPERATION;
                }
            } else
#elif defined(DEBUG_RAWDUMP)
            if (m_parameters->checkBayerDumpEnable() && flagBayer == true) {
                //bytesPerLine[0] = (maxW + 16) * 2;
                if (node->setFormat() != NO_ERROR) {
                    CLOGE(" node->setFormat() fail");
                    return INVALID_OPERATION;
                }
            } else
#endif
            {
                if (node->setFormat(pipeInfos->bytesPerPlane) != NO_ERROR) {
                    CLOGE(" node->setFormat() fail");
                    return INVALID_OPERATION;
                }
            }
        }

        node->getBufferType(&currentBufferCount, &currentBufType, &currentMemType);

    } else {
        CLOGD(" SKIP set pipeInfos setFormat(%d, %d) and reqBuffers(%d).",
             pipeInfos->rectInfo.fullW, pipeInfos->rectInfo.fullH, pipeInfos->bufInfo.count);
    }

    if (currentBufferCount <= 0) {
        CLOGW(" invalid currentBufferCount(%d). So, skip reqBuffers()",
                 currentBufferCount);
    } else {
        if (node->reqBuffers() != NO_ERROR) {
            CLOGE(" node->reqBuffers() fail");
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_forceDone(ExynosCameraNode *node, unsigned int cid, int value)
{
    CLOGD("");
    int ret = NO_ERROR;

    if (node == NULL) {
        CLOGE(" m_mainNode == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (cid != V4L2_CID_IS_FORCE_DONE) {
        CLOGE(" cid != V4L2_CID_IS_FORCE_DONE so, fail");
        return INVALID_OPERATION;
    }

    /* "value" is not meaningful */
    ret = node ->setControl(cid, value);
    if (ret != NO_ERROR) {
        CLOGE("node V4L2_CID_IS_FORCE_DONE failed");
        node->dump();
        return ret;
    }

    return ret;
}

bool ExynosCameraPipe::m_checkLeaderNode(int sensorId)
{
    bool ret = false;

#ifdef INPUT_LEADER_MASK
    if ((sensorId & INPUT_LEADER_MASK) >> INPUT_LEADER_SHIFT)
        ret = true;
#endif

    return ret;
}

status_t ExynosCameraPipe::m_startNode(void)
{
    status_t ret = NO_ERROR;
    const char* nodeNames[MAX_NODE] = {"MAIN_OUTPUT", "MAIN_CAPTURE", "MAIN_SUB"};

    for (int i = MAX_NODE - 1; 0 <= i; i--) {
        /* only M2M mode need stream on/off */
        /* TODO : flite has different sensorId bit */
        if (m_node[i] != NULL &&
            m_isOtf(m_sensorIds[i]) == false) {
            ret = m_node[i]->start();
            if (ret != NO_ERROR) {
                CLOGE(" m_node[%d](%s)->start fail!, ret(%d)", i, nodeNames[i], ret);
                return ret;
            }
        }
    }

    return ret;
}

status_t ExynosCameraPipe::m_stopNode(void)
{
    status_t ret = NO_ERROR;
    const char* nodeNames[MAX_NODE] = {"MAIN_OUTPUT", "MAIN_CAPTURE", "MAIN_SUB"};

    for (int i = 0 ; i < MAX_NODE; i++) {
        /* only M2M mode need stream on/off */
        /* TODO : flite has different sensorId bit */
        if (m_node[i] != NULL &&
            m_isOtf(m_sensorIds[i]) == false) {
            ret = m_node[i]->stop();
            if (ret != NO_ERROR) {
                CLOGE(" m_node[%d](%s)->stop fail!, ret(%d)", i, nodeNames[i], ret);
            }
        }
    }

    return ret;
}

status_t ExynosCameraPipe::m_clearNode(void)
{
    status_t ret = NO_ERROR;
    const char* nodeNames[MAX_NODE] = {"MAIN_OUTPUT", "MAIN_CAPTURE", "MAIN_SUB"};

    for (int i = 0 ; i < MAX_NODE; i++) {
        if (m_node[i]) {
            ret = m_node[i]->clrBuffers();
            if (ret != NO_ERROR) {
                CLOGE(" m_node[%d](%s)->clrBuffers fail!, ret(%d)",
                         i, nodeNames[i], ret);
                return ret;
            }
        }
    }

    return ret;
}

status_t ExynosCameraPipe::m_checkNodeGroupInfo(char *name, camera2_node *oldNode, camera2_node *newNode)
{
    if (oldNode == NULL || newNode == NULL) {
        CLOGE(" oldNode(%p) == NULL || newNode(%p) == NULL", oldNode, newNode);
        return INVALID_OPERATION;
    }

    bool flagCropRegionChanged = false;

    for (int i = 0; i < 4; i++) {
        if (oldNode->input.cropRegion[i] != newNode->input.cropRegion[i] ||
            oldNode->output.cropRegion[i] != newNode->output.cropRegion[i]) {

            CLOGD(" name %s : PerFrame oldCropSize (%d, %d, %d, %d / %d, %d, %d, %d) \
                -> newCropSize (%d, %d, %d, %d / %d, %d, %d, %d)",
                name,
                oldNode->input. cropRegion[0], oldNode->input. cropRegion[1],
                oldNode->input. cropRegion[2], oldNode->input. cropRegion[3],
                oldNode->output.cropRegion[0], oldNode->output.cropRegion[1],
                oldNode->output.cropRegion[2], oldNode->output.cropRegion[3],
                newNode->input. cropRegion[0], newNode->input. cropRegion[1],
                newNode->input. cropRegion[2], newNode->input. cropRegion[3],
                newNode->output.cropRegion[0], newNode->output.cropRegion[1],
                newNode->output.cropRegion[2], newNode->output.cropRegion[3]);

            break;
        }
    }

    for (int i = 0; i < 4; i++) {
        oldNode->input. cropRegion[i] = newNode->input. cropRegion[i];
        oldNode->output.cropRegion[i] = newNode->output.cropRegion[i];
    }

    return NO_ERROR;
}

status_t ExynosCameraPipe::m_checkNodeGroupInfo(int index, camera2_node *oldNode, camera2_node *newNode)
{
    if (oldNode == NULL || newNode == NULL) {
        CLOGE(" oldNode(%p) == NULL || newNode(%p) == NULL", oldNode, newNode);
        return INVALID_OPERATION;
    }

    bool flagCropRegionChanged = false;

    for (int i = 0; i < 4; i++) {
        if (oldNode->input.cropRegion[i] != newNode->input.cropRegion[i] ||
            oldNode->output.cropRegion[i] != newNode->output.cropRegion[i]) {

            CLOGD(" index %d : PerFrame oldCropSize (%d, %d, %d, %d / %d, %d, %d, %d) \
                -> newCropSize (%d, %d, %d, %d / %d, %d, %d, %d)",
                index,
                oldNode->input. cropRegion[0], oldNode->input. cropRegion[1],
                oldNode->input. cropRegion[2], oldNode->input. cropRegion[3],
                oldNode->output.cropRegion[0], oldNode->output.cropRegion[1],
                oldNode->output.cropRegion[2], oldNode->output.cropRegion[3],
                newNode->input. cropRegion[0], newNode->input. cropRegion[1],
                newNode->input. cropRegion[2], newNode->input. cropRegion[3],
                newNode->output.cropRegion[0], newNode->output.cropRegion[1],
                newNode->output.cropRegion[2], newNode->output.cropRegion[3]);

            break;
        }
    }

    for (int i = 0; i < 4; i++) {
        oldNode->input. cropRegion[i] = newNode->input. cropRegion[i];
        oldNode->output.cropRegion[i] = newNode->output.cropRegion[i];
    }

    return NO_ERROR;
}

void ExynosCameraPipe::m_dumpRunningFrameList(void)
{
    CLOGI("*********runningFrameList dump***********");
    CLOGI("m_numBuffers : %d", m_numBuffers);
    for (uint32_t i = 0; i < m_numBuffers; i++) {
        if (m_runningFrameList[i] == NULL) {
            CLOGV("DEBUG:runningFrameList[%d] is NULL", i);
        } else {
            CLOGD("DEBUG:runningFrameList[%d]: fcount = %d",
                    i, m_runningFrameList[i]->getFrameCount());
        }
    }
}

void ExynosCameraPipe::m_dumpPerframeNodeGroupInfo(const char *name, camera_pipe_perframe_node_group_info_t nodeInfo)
{
    if (name != NULL)
        CLOGI("(%s) ++++++++++++++++++++", name);

    CLOGI("\t\t perframeSupportNodeNum : %d", nodeInfo.perframeSupportNodeNum);
    CLOGI("\t\t perFrameLeaderInfo.perframeInfoIndex : %d", nodeInfo.perFrameLeaderInfo.perframeInfoIndex);
    CLOGI("\t\t perFrameLeaderInfo.perFrameVideoID : %d", nodeInfo.perFrameLeaderInfo.perFrameVideoID);
    CLOGI("\t\t perFrameCaptureInfo[0].perFrameVideoID : %d", nodeInfo.perFrameCaptureInfo[0].perFrameVideoID);
    CLOGI("\t\t perFrameCaptureInfo[1].perFrameVideoID : %d", nodeInfo.perFrameCaptureInfo[1].perFrameVideoID);

    if (name != NULL)
        CLOGI("(%s) ------------------------------", name);
}

void ExynosCameraPipe::m_configDvfs(void) {
    bool newDvfs = m_parameters->getDvfsLock();
    int ret = 0;

    if (newDvfs != m_dvfsLocked) {
        ret = setControl(V4L2_CID_IS_DVFS_LOCK, 533000);
        if (ret != NO_ERROR)
            CLOGE("setControl failed");
        m_dvfsLocked = newDvfs;
    }
}

bool ExynosCameraPipe::m_flagValidInt(int num)
{
    bool ret = false;

    if (num == -1 || num == 0)
        ret = false;
    else
        ret = true;

    return ret;
}

bool ExynosCameraPipe::m_isOtf(int sensorId)
{
    if (m_flagValidInt(sensorId) == false) {
        CLOGW("m_flagValidInt(%d) == false", sensorId);
        /* to protect q, dq */
        return true;
    }

#ifdef INPUT_MEMORY_MASK
    return ((sensorId & INPUT_MEMORY_MASK) >>  INPUT_MEMORY_SHIFT);
#else
    return false;
#endif
}

bool ExynosCameraPipe::m_checkValidFrameCount(struct camera2_shot_ext *shot_ext)
{
    bool ret = true;

    if (shot_ext == NULL) {
        CLOGE("shot_ext == NULL. so fail");
        return false;
    }

    int frameCount = getMetaDmRequestFrameCount(shot_ext);

    if (frameCount < 0 ||
        frameCount < m_lastSrcFrameCount) {
        CLOGE("invalid frameCount(%d) < m_lastSrcFrameCount(%d). so fail",
             frameCount, m_lastSrcFrameCount);

        ret = false;
    }

    m_lastSrcFrameCount = frameCount;

    return ret;
}

bool ExynosCameraPipe::m_checkValidFrameCount(struct camera2_stream *stream)
{
    bool ret = true;

    if (stream == NULL) {
        CLOGE("shot_ext == NULL. so fail");
        return false;
    }

    /*
    if (stream->fcount <= 0 ||
        m_lastDstFrameCount + 1 != frameCount) {
        CLOGE("invalid m_lastDstFrameCount(%d) + 1 != stream->fcount(%d). so fail",
             m_lastDstFrameCount, stream->fcount);

        ret = false;
    }
    */

    if (stream->fcount <= 0) {
        CLOGE("invalid stream->fcount(%d). so fail",
             stream->fcount);

        ret = false;
    }

    if (stream->fvalid == 0) {
        CLOGE("invalid fvalid(%d). so fail",
             stream->fvalid);

        ret = false;
    }

    if (0 < stream->fcount)
        m_lastDstFrameCount = stream->fcount;

    return ret;
}

status_t ExynosCameraPipe::m_handleInvalidFrame(int index, ExynosCameraFrameSP_sptr_t newFrame, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;

    if (newFrame == NULL) {
        CLOGE("newFrame == NULL. so fail");
        return false;
    }

    if (buffer == NULL) {
        CLOGE("buffer == NULL. so fail");
        return false;
    }

    /* to complete frame */
    ExynosCameraFrameSP_sptr_t curFrame = NULL;

    m_runningFrameList[index] = newFrame;

    /* m_completeFrame will do m_completeFrame--. so, I put ++ to make 0 */
    m_numOfRunningFrame++;

    /* complete frame */
    ret = m_completeFrame(curFrame, buffer[OUTPUT_NODE], false);
    if (ret < 0) {
        CLOGE("complete frame fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return ret;
    }

    if (curFrame == NULL) {
        CLOGE("curFrame is fail");
        return ret;
    }

    if (m_outputFrameQ != NULL)
        m_outputFrameQ->pushProcessQ(&curFrame);

    return ret;
}

bool ExynosCameraPipe::m_isReprocessing(void)
{
    return m_reprocessing == 1 ? true : false;
}

bool ExynosCameraPipe::m_checkThreadLoop(void)
{
    Mutex::Autolock lock(m_pipeframeLock);
    bool loop = false;

    if (m_isReprocessing() == false)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() > 0)
        loop = true;

    if (m_flagTryStop == true)
        loop = false;

    return loop;
}

void ExynosCameraPipe::m_init(void)
{
    m_mainNodeNum = -1;
    m_mainNode = NULL;

    for (int i = 0; i < MAX_NODE; i++) {
        m_nodeNum[i] = -1;
        m_node[i] = NULL;
        m_sensorIds[i] = -1;
        m_bufferManager[i] = NULL;
    }

    m_exynosconfig = NULL;

    m_pipeId = 0;
    m_cameraId = -1;
    m_reprocessing = 0;
    m_oneShotMode = false;

    m_parameters = NULL;
    m_prepareBufferCount = 0;
    m_numBuffers = 0;
    m_numCaptureBuf = 0;

    m_activityControl = NULL;

    m_inputFrameQ = NULL;
    m_outputFrameQ = NULL;
    m_frameDoneQ= NULL;

    for(int i = 0; i < MAX_NODE; i++) {
        for (int j = 0; j < MAX_BUFFERS; j++) {
            m_runningFrameList[j] = NULL;
            m_nodeRunningFrameList[i][j] = NULL;
        }
    }

    m_numOfRunningFrame = 0;

    m_flagStartPipe = false;
    m_flagTryStop = false;

    m_flagFrameDoneQ = false;

    m_threadCommand = 0;
    m_timeInterval = 0;
    m_threadState = 0;
    m_threadRenew = 0;
    memset(m_name, 0x00, sizeof(m_name));

    m_metadataTypeShot = true;
    memset(&m_perframeMainNodeGroupInfo, 0x00, sizeof(camera_pipe_perframe_node_group_info_t));
    memset(&m_curNodeGroupInfo, 0x00, sizeof(camera2_node_group));

    m_dvfsLocked = false;
    m_isBoosting = false;
    m_lastSrcFrameCount = 0;
    m_lastDstFrameCount = 0;

    m_setfile = 0x0;

#ifdef USE_CAMERA2_API_SUPPORT
    m_timeLogCount = TIME_LOG_COUNT;
#endif
}

}; /* namespace android */
