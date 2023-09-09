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
#define LOG_TAG "ExynosCameraPipeVRAGroup"

#include "ExynosCameraPipeVRAGroup.h"
#include "ExynosCameraPipeGSC.h"

namespace android {

ExynosCameraPipeVRAGroup::~ExynosCameraPipeVRAGroup()
{
    destroy();
}

status_t ExynosCameraPipeVRAGroup::create(int32_t *sensorIds)
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

    m_mainThread = new ExynosCameraThread<ExynosCameraPipeVRAGroup>(this, &ExynosCameraPipeVRAGroup::m_mainThreadFunc, "mainThread");

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    m_prepareBufferCount = 0;
#ifdef USE_CAMERA2_API_SUPPORT
    m_timeLogCount = TIME_LOG_COUNT;
#endif
    CLOGI("create() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeVRAGroup::m_putBuffer(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer newBuffer;
    struct camera2_shot_ext *shot_ext;
    struct camera2_shot_ext check_shot_ext;

    while (m_inputFrameQ->getSizeOfProcessQ() > 1) {
        if (m_isReprocessing() == true) {
            break;
        }

        ret = m_inputFrameQ->popProcessQ(&newFrame);
        if (ret != NO_ERROR) {
            CLOGE("wait and pop fail, ret(%d)", ret);
            return ret;
        }

        if (newFrame != NULL) {
            ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
            if (ret != NO_ERROR) {
                CLOGE("set entity state fail, ret(%d)", ret);
                /* TODO: doing exception handling */
                return INVALID_OPERATION;
            }

            memset(&check_shot_ext, 0, sizeof(struct camera2_shot_ext));
            newFrame->getMetaData(&check_shot_ext);
            if (m_parameters->checkFaceDetectMeta(&check_shot_ext) == true) {
                newFrame->setMetaData(&check_shot_ext);
            }

            m_outputFrameQ->pushProcessQ(&newFrame);
        }
    }

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (m_flagTryStop == true) {
        CLOGD("m_flagTryStop(%d)", m_flagTryStop);
        return NO_ERROR;
    }
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            /*
                      * TIMEOUT log print
                      * condition 1 : it is not reprocessing
                      * condition 2 : if it is reprocessing, but m_timeLogCount is equals or lower than 0
                      */
#ifdef USE_CAMERA2_API_SUPPORT
            if (!(m_parameters->isReprocessing() == true && m_timeLogCount <= 0)) {
                m_timeLogCount--;
                CLOGW("wait timeout");
                m_mainNode->dumpState();
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

    /* Get source buffer */
    ret = newFrame->getSrcBuffer(getPipeId(), &newBuffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    entity_buffer_state_t srcBufferState;
    ret = newFrame->getSrcBufferState(getPipeId(), &srcBufferState);
    if (newBuffer.index < 0
        || srcBufferState == ENTITY_BUFFER_STATE_ERROR) {
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    shot_ext = (camera2_shot_ext *)newBuffer.addr[newBuffer.getMetaPlaneIndex()];
    if (shot_ext != NULL && shot_ext->fd_bypass == true) {
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    if (m_runningFrameList[newBuffer.index] != NULL) {
        CLOGE("new buffer is invalid, we already get buffer index(%d), newFrame->frameCount(%d)",
             newBuffer.index, newFrame->getFrameCount());
        m_dumpRunningFrameList();

        return BAD_VALUE;
    }

    if (shot_ext != NULL) {
        newFrame->getMetaData(shot_ext);
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

    m_runningFrameList[newBuffer.index] = newFrame;
    m_numOfRunningFrame++;

#ifdef USE_CAMERA2_API_SUPPORT
    m_timeLogCount = TIME_LOG_COUNT;
#endif

    return NO_ERROR;

FUNC_EXIT:
    if (newFrame != NULL) {
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("set entity state fail, ret(%d)", ret);
            /* TODO: doing exception handling */
            return INVALID_OPERATION;
        }

        memset(&check_shot_ext, 0, sizeof(struct camera2_shot_ext));
        newFrame->getMetaData(&check_shot_ext);
        if (m_parameters->checkFaceDetectMeta(&check_shot_ext) == true) {
            newFrame->setMetaData(&check_shot_ext);
        }

        m_outputFrameQ->pushProcessQ(&newFrame);
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeVRAGroup::m_getBuffer(void)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    struct camera2_shot_ext shot_ext;
    int index = -1;
    int ret = 0;

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        CLOGV(" skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d",
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

    ret = m_updateMetadataToFrame(curBuffer.addr[curBuffer.getMetaPlaneIndex()], curBuffer.index);
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

    if (curFrame != NULL) {
        memset(&shot_ext, 0, sizeof(struct camera2_shot_ext));
        curFrame->getMetaData(&shot_ext);
        if (m_parameters->checkFaceDetectMeta(&shot_ext) == true) {
            curFrame->setMetaData(&shot_ext);
        }
    }

    m_outputFrameQ->pushProcessQ(&curFrame);

    return NO_ERROR;
}

bool ExynosCameraPipeVRAGroup::m_mainThreadFunc(void)
{
    status_t ret = NO_ERROR;

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_putBuffer();
    if (ret == TIMED_OUT) {
        goto retry_thread_loop;
    } else if (ret != NO_ERROR) {
        CLOGE("m_putBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto retry_thread_loop;
    }

    ret = m_getBuffer();
    if (ret != NO_ERROR) {
        if (ret == TIMED_OUT)
            return true;

        CLOGE("m_getBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return false;
    }

    return m_checkThreadLoop();

retry_thread_loop:
    if (m_inputFrameQ->getSizeOfProcessQ() > 0) {
        return true;
    } else {
        return false;
    }
}

bool ExynosCameraPipeVRAGroup::m_checkThreadLoop(void)
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

void ExynosCameraPipeVRAGroup::m_init(void)
{
    /* m_init */
}

}; /* namespace android */
