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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPipeFlite"

#include "ExynosCameraPipeFlite.h"

namespace android {

/* global variable for multi-singleton */
Mutex             ExynosCameraPipeFlite::g_nodeInstanceMutex;
ExynosCameraNode *ExynosCameraPipeFlite::g_node[FLITE_CNTS] = {0};
int               ExynosCameraPipeFlite::g_nodeRefCount[FLITE_CNTS] = {0};
#ifdef SUPPORT_DEPTH_MAP
Mutex             ExynosCameraPipeFlite::g_vcNodeInstanceMutex;
ExynosCameraNode *ExynosCameraPipeFlite::g_vcNode[VC_CNTS] = {0};
int               ExynosCameraPipeFlite::g_vcNodeRefCount[VC_CNTS] = {0};
#endif

ExynosCameraPipeFlite::~ExynosCameraPipeFlite()
{
    this->destroy();
}

status_t ExynosCameraPipeFlite::create(int32_t *sensorIds)
{
    CLOGD("");
    int ret = 0;

    for (int i = 0; i < MAX_NODE; i++) {
        if (sensorIds) {
            CLOGV("set new sensorIds[%d] : %d", i, sensorIds[i]);
            m_sensorIds[i] = sensorIds[i];
        } else {
            m_sensorIds[i] = -1;
        }
    }

    /*
     * Flite must open once. so we will take cover as global variable.
     * we will use only m_createNode and m_destroyNode.
     */
    /*
    m_node[CAPTURE_NODE] = new ExynosCameraNode();
    ret = m_node[CAPTURE_NODE]->create("FLITE", m_cameraId);
    if (ret < 0) {
        CLOGE(" CAPTURE_NODE create fail, ret(%d)", ret);
        return ret;
    }

    ret = m_node[CAPTURE_NODE]->open(m_nodeNum[CAPTURE_NODE]);
    if (ret < 0) {
        CLOGE(" CAPTURE_NODE open fail, ret(%d)", ret);
        return ret;
    }
    CLOGD("Node(%d) opened", m_nodeNum[CAPTURE_NODE]);
    */
    m_node[CAPTURE_NODE] = m_createNode(m_cameraId, m_nodeNum[CAPTURE_NODE]);
#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->isDepthMapSupported()) {
        m_node[CAPTURE_NODE_2] = m_createVcNode(m_cameraId, m_nodeNum[CAPTURE_NODE_2]);
    }
#endif

    /* mainNode is CAPTURE_NODE */
    m_mainNodeNum = CAPTURE_NODE;
    m_mainNode = m_node[m_mainNodeNum];

    /* setInput for 54xx */
    ret = m_setInput(m_node, m_nodeNum, m_sensorIds);
    if (ret < 0) {
        CLOGE(" m_setInput fail, ret(%d)", ret);
        return ret;
    }

    m_mainThread = new ExynosCameraThread<ExynosCameraPipeFlite>(this, &ExynosCameraPipeFlite::m_mainThreadFunc, "fliteThread", PRIORITY_URGENT_DISPLAY);

    m_inputFrameQ = new frame_queue_t;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("create() is succeed (%d) prepare (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::destroy(void)
{
    CLOGD("");
    if (m_node[CAPTURE_NODE] != NULL) {
        /*
        if (m_node[CAPTURE_NODE]->close() != NO_ERROR) {
            CLOGE("close fail");
            return INVALID_OPERATION;
        }
        delete m_node[CAPTURE_NODE];
        */
        m_destroyNode(m_cameraId, m_node[CAPTURE_NODE]);

        m_node[CAPTURE_NODE] = NULL;
        CLOGD("Node(CAPTURE_NODE, m_nodeNum : %d, m_sensorIds : %d) closed",
             m_nodeNum[CAPTURE_NODE], m_sensorIds[CAPTURE_NODE]);
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_node[CAPTURE_NODE_2] != NULL) {
        m_destroyVcNode(m_cameraId, m_node[CAPTURE_NODE_2]);

        m_node[CAPTURE_NODE_2] = NULL;
        CLOGD("Node(CAPTURE_NODE_2, m_nodeNum : %d, m_sensorIds : %d) closed",
                 m_nodeNum[CAPTURE_NODE_2], m_sensorIds[CAPTURE_NODE_2]);
    }
#endif

    m_mainNode = NULL;
    m_mainNodeNum = -1;

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

#ifdef SUPPORT_DEPTH_MAP
status_t ExynosCameraPipeFlite::start(void)
{
    status_t ret = 0;

    ret = ExynosCameraPipe::start();

    if (m_parameters->isDepthMapSupported()) {
        ret = m_node[CAPTURE_NODE_2]->start();
        if (ret != NO_ERROR) {
            CLOGE("Failed to start VCI node");
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::stop(void)
{
    status_t ret = 0;

    ret = ExynosCameraPipe::stop();
    if (ret != NO_ERROR) {
        CLOGE("Failed to stopPipe");
        return ret;
    }

    if (m_parameters->isDepthMapSupported()) {
        ret = m_node[CAPTURE_NODE_2]->stop();
        if (ret != NO_ERROR) {
            CLOGE("Failed to stop VCI node");
            return INVALID_OPERATION;
        }

        ret = m_node[CAPTURE_NODE_2]->clrBuffers();
        if (ret != NO_ERROR) {
            CLOGE("Failed to clrBuffers VCI node");
            return INVALID_OPERATION;
        }

        m_node[CAPTURE_NODE_2]->removeItemBufferQ();
    }

    return NO_ERROR;
}
#endif

status_t ExynosCameraPipeFlite::m_getBuffer(void)
{
    ExynosCameraFrameSP_sptr_t curFrame = NULL;
    ExynosCameraBuffer curBuffer;
    int index = -1;
    int ret = 0;
    struct camera2_shot_ext *shot_ext = NULL;

    if (m_numOfRunningFrame <= 0 || m_flagStartPipe == false) {
        if (m_timeLogCount > 0)
            CLOGD(" skip getBuffer, flagStartPipe(%d), numOfRunningFrame = %d",
                 m_flagStartPipe, m_numOfRunningFrame);
        return NO_ERROR;
    }

    ret = m_node[CAPTURE_NODE]->getBuffer(&curBuffer, &index);
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

    shot_ext = (struct camera2_shot_ext *)curBuffer.addr[curBuffer.getMetaPlaneIndex()];

//#ifdef SHOT_RECOVERY
    if (shot_ext != NULL) {
        retryGetBufferCount = shot_ext->complete_cnt;

        if (retryGetBufferCount > 0) {
            CLOGI(" ( %d %d %d %d )",
                shot_ext->free_cnt,
                shot_ext->request_cnt,
                shot_ext->process_cnt,
                shot_ext->complete_cnt);
        }
    }
//#endif

#ifdef SUPPORT_DEPTH_MAP
    curFrame = m_runningFrameList[curBuffer.index];

    if (curFrame->getRequest(PIPE_VC1) == true) {
        ExynosCameraBuffer depthMapBuffer;

        ret = m_node[CAPTURE_NODE_2]->getBuffer(&depthMapBuffer, &index);
        if (ret != NO_ERROR) {
            CLOGE("Failed to get DepthMap buffer");
            return ret;
        }

        if (index < 0) {
            CLOGE("Invalid index %d", index);
            return INVALID_OPERATION;
        }

        ret = curFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDstBufferState into REQUESTED. pipeId %d frameCount %d ret %d",
                     getPipeId(), curFrame->getFrameCount(), ret);
        }

        ret = curFrame->setDstBuffer(getPipeId(), depthMapBuffer, CAPTURE_NODE_2, INDEX(getPipeId()));
        if (ret != NO_ERROR) {
            CLOGE("Failed to set DepthMapBuffer to frame. bufferIndex %d frameCount %d",
                     index, curFrame->getFrameCount());
            return ret;
        }

        ret = curFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE, CAPTURE_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("Failed to setDstBufferState with COMPLETE");
            return ret;
        }
    }
#endif

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

status_t ExynosCameraPipeFlite::m_setPipeInfo(camera_pipe_info_t *pipeInfos)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    if (pipeInfos == NULL) {
        CLOGE(" pipeInfos == NULL. so, fail");
        return INVALID_OPERATION;
    }

    /* initialize node */
    ret = m_setNodeInfo(m_node[CAPTURE_NODE], &pipeInfos[0],
                    2, YUV_FULL_RANGE,
                    true);
    if (ret != NO_ERROR) {
        CLOGE(" m_setNodeInfo(%d, %d, %d) fail",
             pipeInfos[0].rectInfo.fullW, pipeInfos[0].rectInfo.fullH, pipeInfos[0].bufInfo.count);
        return INVALID_OPERATION;
    }

#ifdef SUPPORT_DEPTH_MAP
    if (m_parameters->isDepthMapSupported()) {
        ret = m_setNodeInfo(m_node[CAPTURE_NODE_2], &pipeInfos[CAPTURE_NODE_2],
                2, YUV_FULL_RANGE,
                true);
        if (ret != NO_ERROR) {
            CLOGE("m_setNodeInfo(%d, %d, %d) fail",
                    pipeInfos[CAPTURE_NODE_2].rectInfo.fullW, pipeInfos[CAPTURE_NODE_2].rectInfo.fullH,
                    pipeInfos[CAPTURE_NODE_2].bufInfo.count);
            return INVALID_OPERATION;
        }
    }
#endif

    m_numBuffers = pipeInfos[0].bufInfo.count;

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::setupPipe(camera_pipe_info_t *pipeInfos, int32_t *sensorIds)
{
    CLOGD("");
    status_t ret = NO_ERROR;

#ifdef DEBUG_RAWDUMP
    unsigned int bytesPerLine[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};
#endif

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

    for (uint32_t i = 0; i < m_numBuffers; i++) {
        m_runningFrameList[i] = NULL;
    }
    m_numOfRunningFrame = 0;

    m_prepareBufferCount = m_exynosconfig->current->pipeInfo.prepare[getPipeId()];
    CLOGI("setupPipe() is succeed (%d) setupPipe (%d)", getPipeId(), m_prepareBufferCount);

    return NO_ERROR;
}

status_t ExynosCameraPipeFlite::sensorStream(bool on)
{
    CLOGD("");

    int ret = 0;
    int value = on ? IS_ENABLE_STREAM: IS_DISABLE_STREAM;

    ret = m_node[CAPTURE_NODE]->setControl(V4L2_CID_IS_S_STREAM, value);
    if (ret != NO_ERROR)
        CLOGE("sensor S_STREAM(%d) fail", value);

    return ret;
}

bool ExynosCameraPipeFlite::m_mainThreadFunc(void)
{
    int ret = 0;

    if (m_flagTryStop == true) {
        usleep(5000);
        return true;
    }

    ret = m_getBuffer();
    if (m_flagTryStop == true) {
        CLOGD("m_flagTryStop(%d)", m_flagTryStop);
        return false;
    }
    if (ret < 0) {
        CLOGE("m_getBuffer fail");
        /* TODO: doing exception handling */
    }

//#ifdef SHOT_RECOVERY
    for (int i = 0; i < retryGetBufferCount; i ++) {
        CLOGE(" retryGetBufferCount( %d)", retryGetBufferCount);
        ret = m_getBuffer();
        if (ret < 0) {
            CLOGE("m_getBuffer fail");
            /* TODO: doing exception handling */
        }

        ret = m_putBuffer();
        if (m_flagTryStop == true) {
            CLOGD("m_flagTryStop(%d)", m_flagTryStop);
            return false;
        }
        if (ret < 0) {
            if (ret == TIMED_OUT)
                return true;
            CLOGE("m_putBuffer fail");
            /* TODO: doing exception handling */
        }
    }
    retryGetBufferCount = 0;
//#endif

    if (m_numOfRunningFrame < 2) {
        int cnt = m_inputFrameQ->getSizeOfProcessQ();
        do {
            ret = m_putBuffer();
            if (m_flagTryStop == true) {
                CLOGD("m_flagTryStop(%d)", m_flagTryStop);
                return false;
            }
            if (ret < 0) {
                if (ret == TIMED_OUT)
                    return true;
                CLOGE("m_putBuffer fail");
                /* TODO: doing exception handling */
            }
            cnt--;
        } while (cnt > 0);
    } else {
        ret = m_putBuffer();
        if (m_flagTryStop == true) {
            CLOGD("m_flagTryStop(%d)", m_flagTryStop);
            return false;
        }
        if (ret < 0) {
            if (ret == TIMED_OUT)
                return true;
            CLOGE("m_putBuffer fail");
            /* TODO: doing exception handling */
        }
     }

    return true;
}

void ExynosCameraPipeFlite::m_init(void)
{
//#ifdef SHOT_RECOVERY
    retryGetBufferCount = 0;
//#endif
}

status_t ExynosCameraPipeFlite::m_updateMetadataToFrame(void *metadata, int index)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    camera2_shot_ext *shot_ext = NULL;
    camera2_dm frame_dm;
    camera2_udm frame_udm;

    shot_ext = (struct camera2_shot_ext *) metadata;
    if (shot_ext == NULL) {
        CLOGE("shot_ext is NULL");
        return BAD_VALUE;
    }

    if (index < 0) {
        CLOGE("[B%d]Invalid index.", index);
        return BAD_VALUE;
    }

    if (m_metadataTypeShot == false) {
        CLOGV("Stream type do not need update metadata");
        return NO_ERROR;
    }

    ret = m_getFrameByIndex(frame, index);
    if (ret != NO_ERROR) {
        CLOGE("[B%d]Failed to getFrameByIndex. ret %d",
                 index, ret);
        return ret;
    }

    ret = frame->getDynamicMeta(&frame_dm);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getDynamicMeta. ret %d",
                 frame->getFrameCount(), ret);
        return ret;
    }

    if (frame_dm.request.frameCount == 0 || frame_dm.sensor.timeStamp == 0) {
        frame_dm.request.frameCount = shot_ext->shot.dm.request.frameCount;
        frame_dm.sensor.timeStamp = shot_ext->shot.dm.sensor.timeStamp;

        ret = frame->storeDynamicMeta(&frame_dm);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to storeDynamicMeta. ret %d",
                     frame->getFrameCount(), ret);
            return ret;
        }
    }

    ret = frame->getUserDynamicMeta(&frame_udm);
    if (ret != NO_ERROR) {
        CLOGE("[F%d]Failed to getUserDynamicMeta. ret %d",
                frame->getFrameCount(), ret);
        return ret;
    }

    if (frame_udm.sensor.timeStampBoot == 0) {
        frame_udm.sensor.timeStampBoot = shot_ext->shot.udm.sensor.timeStampBoot;

        ret = frame->storeUserDynamicMeta(&frame_udm);
        if (ret != NO_ERROR) {
            CLOGE("[F%d]Failed to storeUserDynamicMeta. ret %d",
                    frame->getFrameCount(), ret);
            return ret;
        }
    }

    return NO_ERROR;
}

}; /* namespace android */
