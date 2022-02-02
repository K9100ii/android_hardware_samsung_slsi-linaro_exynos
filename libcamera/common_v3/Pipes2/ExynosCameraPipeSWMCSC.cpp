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
#define LOG_TAG "ExynosCameraPipeSWMCSC"
#include <cutils/log.h>

#include "ExynosCameraPipeSWMCSC.h"

namespace android {

ExynosCameraPipeSWMCSC::~ExynosCameraPipeSWMCSC()
{
    this->destroy();
}

status_t ExynosCameraPipeSWMCSC::create(__unused int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    ret= ExynosCameraSWPipe::create(sensorIds);

    int32_t nodeNum[4][1];

    for (int i = 0; i < 3; i++) {
        nodeNum[i][OUTPUT_NODE] = PREVIEW_GSC_NODE_NUM;
        m_gscPipes[i] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, true, nodeNum[i]);
        m_gscPipes[i]->setPipeId(m_gscPipeId[i]);
    }
    nodeNum[3][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    m_gscPipes[3] = (ExynosCameraPipe*)new ExynosCameraPipePP(m_cameraId, m_parameters, true, nodeNum[3]);
    m_gscPipes[3]->setPipeId(PIPE_GSC_PICTURE);

    m_gscPipes[0]->setPipeName("PIPE_GSC_IN_MCSC0");
    m_gscPipes[1]->setPipeName("PIPE_GSC_IN_MCSC1");
    m_gscPipes[2]->setPipeName("PIPE_GSC_IN_MCSC2");
    m_gscPipes[3]->setPipeName("PIPE_GSC_IN_MCSC3");

    for (int i = 0; i < 4; i++) {
        ret = m_gscPipes[i]->create(sensorIds);
        if (ret != NO_ERROR) {
            CLOGE("Internal GSC Pipe(%d) creation fail!", i);
            return ret;
        }

        m_handleGscDoneQ = new frame_queue_t(m_gscDoneThread);
        m_gscFrameDoneQ[i] = new frame_queue_t(m_gscDoneThread);
    }

    m_gscDoneThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeSWMCSC::m_gscDoneThreadFunc, "gscDoneThread");

    return NO_ERROR;
}

status_t ExynosCameraPipeSWMCSC::destroy(void)
{
    status_t ret = NO_ERROR;

    for (int i = 0; i < 4; i++) {
        ret = m_gscPipes[i]->destroy();
        if (ret != NO_ERROR)
            CLOGE("Internal GSC Pipe(%d) detstroy fail!", i);
    }

    ExynosCameraSWPipe::release();

    return NO_ERROR;
}

status_t ExynosCameraPipeSWMCSC::stop(void)
{
    status_t ret = NO_ERROR;

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();
    m_gscDoneThread->requestExitAndWait();

    m_inputFrameQ->release();
    m_handleGscDoneQ->release();

    for (int i = 0; i < 4; i++) {
        ret = m_gscPipes[i]->stop();
        if (ret != NO_ERROR)
            CLOGE("Internal GSC Pipe stop fail!");

        m_gscFrameDoneQ[i]->release();
    }

    m_flagTryStop = false;

    CLOGI("stop() is succeed, Pipe(%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeSWMCSC::stopThread(void)
{
    status_t ret = NO_ERROR;

    m_gscDoneThread->requestExit();
    m_handleGscDoneQ->sendCmd(WAKE_UP);
    for (int i = 0; i < 4; i++) {
        m_gscFrameDoneQ[i]->sendCmd(WAKE_UP);
    }

    ret = ExynosCameraPipe::stopThread();
    if (ret != NO_ERROR)
        CLOGE("Internal GSC Pipe stopThread fail!");

    CLOGI("stopThread is succeed, Pipe(%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeSWMCSC::m_run(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer srcBuffer;

    struct camera2_stream *streamMeta = NULL;
    uint32_t *mcscOutputCrop = NULL;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return BAD_VALUE;
    }

    if (newFrame->getFrameState() == FRAME_STATE_SKIPPED
        || newFrame->getFrameState() == FRAME_STATE_INVALID) {
        if (newFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
            CLOGE("New frame is INVALID, frameCount(%d)",
                     newFrame->getFrameCount());
        }
        goto FUNC_EXIT;
    }

    /* Get scaler source buffer */
    ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    entity_buffer_state_t srcBufferState;
    ret = newFrame->getSrcBufferState(getPipeId(), &srcBufferState);
    if (srcBuffer.index < 0
        || srcBufferState == ENTITY_BUFFER_STATE_ERROR) {
        if (newFrame->getFrameType() != FRAME_TYPE_INTERNAL) {
            CLOGE("Invalid SrcBuffer(index:%d, state:%d)",
                    srcBuffer.index, srcBufferState);
        }
        goto FUNC_EXIT;
    }

    /* Get size from metadata */
    streamMeta = (struct camera2_stream*)srcBuffer.addr[srcBuffer.getMetaPlaneIndex()];
    if (streamMeta == NULL) {
        CLOGE("srcBuffer.addr is NULL, srcBuf.addr(0x%x)",
                srcBuffer.addr[srcBuffer.getMetaPlaneIndex()]);
        goto FUNC_EXIT;
    }

    mcscOutputCrop = streamMeta->output_crop_region;

    /* Run scaler based source size */
    /*
    if (mcscOutputCrop[2] * mcscOutputCrop[3] > FHD_PIXEL_SIZE) {
        ret = m_runScalerSerial(newFrame);
    } else {
    */
        ret = m_runScalerParallel(newFrame);
    //}

    if (ret != NO_ERROR) {
        CLOGE("m_runScaler() fail!, ret(%d)", ret);
        /* TODO: doing exception handling */
        goto FUNC_EXIT;
    }

    return NO_ERROR;

FUNC_EXIT:
    if (newFrame != NULL) {
        ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("set entity state fail, ret(%d)", ret);
            /* TODO: doing exception handling */
            return INVALID_OPERATION;
        }

        m_outputFrameQ->pushProcessQ(&newFrame);
    }

    return NO_ERROR;
}

status_t ExynosCameraPipeSWMCSC::m_runScalerSerial(ExynosCameraFrameSP_sptr_t frame)
{
    return NO_ERROR;
}

status_t ExynosCameraPipeSWMCSC::m_runScalerParallel(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosRect srcRect, dstRect;
    camera2_node_group node_group_info;
    int yuvStreamCount = m_parameters->getYuvStreamMaxNum();
    int mcscPipeId = -1;
    int bufferIndex = -1;

    /* Get scaler source buffer */
    ret = frame->getSrcBuffer(getPipeId(), &srcBuffer);
    if (ret != NO_ERROR) {
        CLOGE("getSrcBuffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return INVALID_OPERATION;
    }

    frame->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_SW_MCSC);

    for (int i = 0; i < yuvStreamCount; i++) {
        status_t funcRet = NO_ERROR;
        mcscPipeId = PIPE_MCSC0 + i;

        /* Get scaler destination buffer */
        if (frame->getRequest(mcscPipeId) == false) {
            continue;
        }

        dstBuffer.index = -1;
        ret = frame->getDstBuffer(getPipeId(), &dstBuffer, m_nodeType[i]);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail. pipeId(%d), frameCount(%d), ret(%d)",
                    mcscPipeId, frame->getFrameCount(), ret);
            continue;
        }

        if (dstBuffer.index < 0) {
            if (m_bufferManager[m_nodeType[i]] != NULL) {
                bufferIndex = -1;
                ret = m_bufferManager[m_nodeType[i]]->getBuffer(&bufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Buffer manager getBuffer fail, manager(%d)(%s), frameCount(%d), ret(%d)",
                            m_nodeType[i], m_bufferManager[m_nodeType[i]]->getName(),
                            frame->getFrameCount(), ret);

                    frame->setRequest(mcscPipeId, false);
                    continue;
                }
            } else {
                CLOGE("Buffer manager is NULL, index(%d), piepId(%d), frameCount(%d)",
                        m_nodeType[i], mcscPipeId, frame->getFrameCount());
                frame->setRequest(mcscPipeId, false);
                continue;
            }
        }

        /* Set source buffer to Scaler */
        funcRet = frame->setSrcBuffer(m_gscPipeId[i], srcBuffer);
        if (funcRet != NO_ERROR) {
            CLOGE("setSrcBuffer fail, pipeId(%d), ret(%d)", m_gscPipeId[i], ret);
            ret |= funcRet;
        }

        /* Set destination buffer to Scaler */
        funcRet = frame->setDstBuffer(m_gscPipeId[i], dstBuffer);
        if (funcRet != NO_ERROR) {
            CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)", m_gscPipeId[i], ret);
            ret |= funcRet;
        }

        /* Set scaler source size */
        srcRect.colorFormat = m_parameters->getCalculatedYuvColorFormat();
        srcRect.x = node_group_info.capture[i].input.cropRegion[0];
        srcRect.y = node_group_info.capture[i].input.cropRegion[1];
        srcRect.w = node_group_info.capture[i].input.cropRegion[2];
        srcRect.h = node_group_info.capture[i].input.cropRegion[3];
        srcRect.fullW = ALIGN_UP(node_group_info.leader.input.cropRegion[2], CAMERA_16PX_ALIGN);
#if GRALLOC_CAMERA_64BYTE_ALIGN
        if (srcRect.colorFormat == V4L2_PIX_FMT_NV21M)
            srcRect.fullW = ALIGN_UP(node_group_info.leader.input.cropRegion[2], CAMERA_64PX_ALIGN);
#endif
        srcRect.fullH = node_group_info.leader.input.cropRegion[3];

        /* Set scaler destination size */
        dstRect.colorFormat = m_parameters->getYuvFormat(i);
        dstRect.x = 0;
        dstRect.y = 0;
        dstRect.w = node_group_info.capture[i].output.cropRegion[2];
        dstRect.h = node_group_info.capture[i].output.cropRegion[3];
        dstRect.fullW = node_group_info.capture[i].output.cropRegion[2];
#if GRALLOC_CAMERA_64BYTE_ALIGN
        if (dstRect.colorFormat == V4L2_PIX_FMT_NV21M)
            dstRect.fullW = ALIGN_UP(node_group_info.capture[i].output.cropRegion[2], CAMERA_64PX_ALIGN);
#endif
        dstRect.fullH = node_group_info.capture[i].output.cropRegion[3];

        /* Calibrate src size */
        struct camera2_stream *streamMeta = streamMeta = (struct camera2_stream*)srcBuffer.addr[srcBuffer.getMetaPlaneIndex()];
        if (streamMeta == NULL) {
            CLOGE("srcBuffer.addr is NULL, srcBuffer.addr(0x%x)", srcBuffer.addr[srcBuffer.getMetaPlaneIndex()]);
            ret |= BAD_VALUE;
        } else {
            uint32_t *mcscOutputCrop = streamMeta->output_crop_region;

            if (srcRect.fullW != mcscOutputCrop[2] || srcRect.fullH != mcscOutputCrop[3]) {
                CLOGV("Calibrated src size(%d x %d -> %d x %d), fCount(%d)",
                        srcRect.fullW, srcRect.fullH, mcscOutputCrop[2], mcscOutputCrop[3],
                        frame->getFrameCount());

                srcRect.fullW = ALIGN_UP(mcscOutputCrop[2], CAMERA_16PX_ALIGN);
#if GRALLOC_CAMERA_64BYTE_ALIGN
                if (srcRect.colorFormat == V4L2_PIX_FMT_NV21M)
                    srcRect.fullW = ALIGN_UP(mcscOutputCrop[2], CAMERA_64PX_ALIGN);
#endif
                srcRect.fullH = mcscOutputCrop[3];
                ret = getCropRectAlign(
                        mcscOutputCrop[2], mcscOutputCrop[3], dstRect.fullW, dstRect.fullH,
                        &srcRect.x, &srcRect.y, &srcRect.w, &srcRect.h,
                        2, 2, 0, 1.0);
                if (ret != NO_ERROR) {
                    CLOGW("getCropRectAlign failed. in_size %dx%d, out_size %dx%d",
                         srcRect.fullW, srcRect.fullH, dstRect.fullW, dstRect.fullH);

                    srcRect.x = 0;
                    srcRect.y = 0;
                    srcRect.w = srcRect.fullW;
                    srcRect.h = srcRect.fullH;
                }
            }
        }

        ret |= frame->setSrcRect(m_gscPipeId[i], srcRect);
        ret |= frame->setDstRect(m_gscPipeId[i], dstRect);

        CLOGV("Scaling(MCSC%d):srcBuf.index(%d) dstBuf.index(%d) \
                (size:%d, %d, %d x %d) format(%c%c%c%c) actual(%x) -> \
                (size:%d, %d, %d x %d) format(%c%c%c%c)  actual(%x)",
                i, srcBuffer.index, dstBuffer.index,
                srcRect.x, srcRect.y, srcRect.w, srcRect.h,
                v4l2Format2Char(srcRect.colorFormat, 0),
                v4l2Format2Char(srcRect.colorFormat, 1),
                v4l2Format2Char(srcRect.colorFormat, 2),
                v4l2Format2Char(srcRect.colorFormat, 3),
                V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
                dstRect.x, dstRect.y, dstRect.w, dstRect.h,
                v4l2Format2Char(dstRect.colorFormat, 0),
                v4l2Format2Char(dstRect.colorFormat, 1),
                v4l2Format2Char(dstRect.colorFormat, 2),
                v4l2Format2Char(dstRect.colorFormat, 3),
                V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat));

        /* TODO: doing exception handling */
        if (ret != NO_ERROR) {
            ret = m_bufferManager[m_nodeType[i]]->putBuffer(bufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE("Buffer manager putBuffer fail, manager(%d)(%s), frameCount(%d), ret(%d)",
                        m_nodeType[i], m_bufferManager[m_nodeType[i]]->getName(),
                        frame->getFrameCount(), ret);
            }

            ret = frame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_ERROR, m_nodeType[i]);
            if (ret != NO_ERROR) {
                CLOGE("setDstBuffer state fail, pipeID(%d), frameCount(%d), ret(%d)",
                        mcscPipeId, frame->getFrameCount(), ret);
            }

            frame->setRequest(mcscPipeId, false);
            continue;
        }

        /* Copy metadata to destination buffer */
        memcpy(dstBuffer.addr[dstBuffer.getMetaPlaneIndex()], srcBuffer.addr[srcBuffer.getMetaPlaneIndex()], dstBuffer.size[dstBuffer.getMetaPlaneIndex()]);

        /* Push frame to scaler Pipe */
        m_gscPipes[i]->setOutputFrameQ(m_gscFrameDoneQ[i]);
        m_gscPipes[i]->pushFrame(frame);
    }

    m_handleGscDoneQ->pushProcessQ(&frame);
    if (m_gscDoneThread->isRunning() == false) {
        m_gscDoneThread->run(m_name);
    }

    return NO_ERROR;
}

bool ExynosCameraPipeSWMCSC::m_gscDoneThreadFunc(void)
{
    status_t ret = NO_ERROR;

    ret = m_handleGscDoneFrame();
    if (ret != NO_ERROR) {
        if (ret != TIMED_OUT)
            CLOGE("m_putBuffer fail, ret(%d)", ret);

        /* TODO: doing exception handling */
    }

    return m_checkThreadLoop();
}

status_t ExynosCameraPipeSWMCSC::m_handleGscDoneFrame()
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameSP_sptr_t requestFrame = NULL;
    ExynosCameraFrameSP_sptr_t doneFrame = NULL;
    int mcscPipeId = -1;
    int waitCount = 0;
    int yuvStreamCount = m_parameters->getYuvStreamMaxNum();
    ExynosCameraBuffer dstBuffer;
    entity_buffer_state_t dstBufferState = ENTITY_BUFFER_STATE_NOREQ;

    ret = m_handleGscDoneQ->waitAndPopProcessQ(&requestFrame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (requestFrame == NULL) {
        CLOGE("Request Frame is NULL");
        return BAD_VALUE;
    }

    /* Wait and Pop frame from GSC output Q */
    for (int i = 0; i < yuvStreamCount; i++) {
        status_t funcRet = NO_ERROR;
        mcscPipeId = PIPE_MCSC0 + i;

        /* Get scaler destination buffer */
        if (requestFrame->getRequest(mcscPipeId) == false) {
            continue;
        }

        CLOGV("wait GSC(%d) output", i);

        waitCount = 0;
        doneFrame = NULL;
        do {
            ret = m_gscFrameDoneQ[i]->waitAndPopProcessQ(&doneFrame);
            waitCount++;

        } while (ret == TIMED_OUT && waitCount < 10);

        if (ret != NO_ERROR)
            CLOGW("GSC wait and pop error, ret(%d)", ret);

        if (doneFrame == NULL) {
            CLOGE("gscFrame is NULL");
            requestFrame->setRequest(mcscPipeId, false);
            continue;
        }

        if (requestFrame->getFrameCount() != doneFrame->getFrameCount()) {
            CLOGW("FrameCount mismatch, Push(%d) Pop(%d)",
                    requestFrame->getFrameCount(), doneFrame->getFrameCount());
        }

        CLOGV("Get frame from GSC Pipe(%d), frameCount(%d)",
                i, doneFrame->getFrameCount());

        /* Check dstBuffer state */
        dstBufferState = ENTITY_BUFFER_STATE_NOREQ;
        ret = requestFrame->getDstBufferState(m_gscPipeId[i], &dstBufferState);
        if (ret != NO_ERROR || dstBufferState == ENTITY_BUFFER_STATE_ERROR) {
            CLOGE("getDstBufferState fail, pipeId(%d), entityState(%d) frame(%d)",
                    m_gscPipeId[i], dstBufferState, requestFrame->getFrameCount());
            requestFrame->setRequest(mcscPipeId, false);
            continue;
        }

        dstBuffer.index = -1;
        ret = requestFrame->getDstBuffer(m_gscPipeId[i], &dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d) frame(%d)",
                    m_gscPipeId[i], ret, requestFrame->getFrameCount());
            requestFrame->setRequest(mcscPipeId, false);
            continue;
        }

        if (dstBuffer.index < 0) {
            CLOGE("Invalid Dst buffer index(%d), frame(%d)",
                    dstBuffer.index, requestFrame->getFrameCount());
            requestFrame->setRequest(mcscPipeId, false);
            continue;
        }

        /* Sync dst Node index with MCSC0 */
        funcRet = requestFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED, m_nodeType[i]);
        if (funcRet != NO_ERROR) {
            CLOGE("setdst Buffer state failed(%d) frame(%d)",
                    ret, requestFrame->getFrameCount());
            ret |= funcRet;
        }

        funcRet = requestFrame->setDstBuffer(getPipeId(), dstBuffer, m_nodeType[i]);
        if (funcRet != NO_ERROR) {
            CLOGE("setdst Buffer failed(%d) frame(%d)",
                    ret, requestFrame->getFrameCount());
            ret |= funcRet;
        }

        funcRet = requestFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE, m_nodeType[i]);
        if (funcRet != NO_ERROR) {
            CLOGE("setdst Buffer state failed(%d) frame(%d)",
                    ret, requestFrame->getFrameCount());
            ret |= funcRet;
        }

        /* TODO: doing exception handling */
        if (ret != NO_ERROR) {
            ret = m_bufferManager[m_nodeType[i]]->putBuffer(dstBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret != NO_ERROR) {
                CLOGE("Buffer manager putBuffer fail, manager(%d)(%s), frameCount(%d), ret(%d)",
                        m_nodeType[i], m_bufferManager[m_nodeType[i]]->getName(),
                        requestFrame->getFrameCount(), ret);
            }

            ret = requestFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_ERROR, m_nodeType[i]);
            if (ret != NO_ERROR) {
                CLOGE("setDstBuffer state fail, pipeID(%d), frameCount(%d), ret(%d)",
                        mcscPipeId, requestFrame->getFrameCount(), ret);
            }

            requestFrame->setRequest(mcscPipeId, false);
            continue;
        }
    }

    ret = requestFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("Set entity state fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return ret;
    }

    m_outputFrameQ->pushProcessQ(&requestFrame);

    return NO_ERROR;
}

void ExynosCameraPipeSWMCSC::m_init(int32_t *nodeNums)
{
    for (int i = 0; i < 4; i++) {
        m_gscFrameDoneQ[i] = NULL;
        m_gscPipes[i] = NULL;
        m_nodeType[i] = (enum NODE_TYPE)nodeNums[CAPTURE_NODE + i];
    }
    m_handleGscDoneQ = NULL;
}

}; /* namespace android */
