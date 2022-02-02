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
#define LOG_TAG "ExynosCameraSolutionVDIS"
#include <log/log.h>

#include "ExynosCameraFrame.h"
#include "ExynosCameraParameters.h"
#include "ExynosCameraConfigurations.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraFrameFactory.h"

#include "ExynosCameraSolutionSWVdis.h"

ExynosCameraSolutionSWVdis::ExynosCameraSolutionSWVdis(int cameraId, int pipeId,
                                       ExynosCameraParameters *parameters,
                                       ExynosCameraConfigurations *configurations)
{
    CLOGD("+ IN + ");

    m_cameraId = cameraId;
    strcpy(m_name, "ExynosCameraSolutionSWVdis");

    m_pipeId = pipeId;
    m_pParameters = parameters;
    m_pConfigurations = configurations;
    m_capturePipeId = -1;
    m_pBufferSupplier = NULL;
    m_pFrameFactory = NULL;
}

ExynosCameraSolutionSWVdis::~ExynosCameraSolutionSWVdis()
{
    CLOGD("+ IN + ");
}

status_t ExynosCameraSolutionSWVdis::configureStream(void)
{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;

    ret = m_adjustSize();

    return ret;
}

status_t ExynosCameraSolutionSWVdis::flush(void)
{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;
    bool bStart = false;
    frame_queue_t *frameQ = NULL;

    if(m_pFrameFactory == NULL) {
        CLOGW("frameFactory is NULL!!");
        return INVALID_OPERATION;
    }

    m_pFrameFactory->getInputFrameQToPipe(&frameQ, m_pipeId);
    while(frameQ->getSizeOfProcessQ() > 0) {
        CLOGW("VDIS pipe inputQ(%d)", frameQ->getSizeOfProcessQ());
        frameQ->sendCmd(WAKE_UP);
        usleep(DM_WAITING_TIME);
    }

    m_pFrameFactory->stopThread(m_pipeId);
    m_pFrameFactory->stopThreadAndWait(m_pipeId);

    frameQ->release();

    m_pFrameFactory->setParameter(m_pipeId, PLUGIN_PARAMETER_KEY_STOP, (void*)&bStart);

    return ret;
}

status_t ExynosCameraSolutionSWVdis::setBuffer(ExynosCameraBufferSupplier* bufferSupplier)
{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;
    m_pBufferSupplier = bufferSupplier;
    return ret;
}

int ExynosCameraSolutionSWVdis::getPipeId(void)
{
    CLOGD("+ IN + ");

    return m_pipeId;
}

int ExynosCameraSolutionSWVdis::getCapturePipeId(void)
{
    CLOGD("+ IN + ");
    return m_capturePipeId;
}

void ExynosCameraSolutionSWVdis::checkMode(void)
{
    CLOGD("+ IN + ");

    int videoW = 0, videoH = 0;
    m_pConfigurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);

    int fps;
    fps = m_pConfigurations->getModeValue(CONFIGURATION_RECORDING_FPS);

    if (m_pConfigurations->getMode(CONFIGURATION_RECORDING_MODE) != true) {
        CLOGE("This mode is NOT recording mode");
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, false);
        return;
    }

    if (videoW == 1920 && videoH == 1080) {
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, true);
    } else if (videoW == 1280 && videoH == 720){
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, true);
    } else {
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, false);
    }

    if (fps > 30) {
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, false);
    }

#ifdef USE_DUAL_CAMERA
    if (m_pConfigurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        m_pConfigurations->setMode(CONFIGURATION_VIDEO_STABILIZATION_MODE, false);
    }
#endif

EXIT_FUNC:
    CLOGD("set VDIS mode(%d), size(%dx%d) fps(%d)",
        m_pConfigurations->getMode(CONFIGURATION_VIDEO_STABILIZATION_MODE), videoW, videoH, fps);
}

status_t ExynosCameraSolutionSWVdis::handleFrame(SOLUTION_PROCESS_TYPE type,
                                                    sp<ExynosCameraFrame> frame,
                                                    int prevPipeId,
                                                    int capturePipeId,
                                                    ExynosCameraFrameFactory *factory)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case SOLUTION_PROCESS_PRE:
        m_capturePipeId = capturePipeId;
        m_pFrameFactory = factory;

        ret = m_handleFramePreProcess(frame, prevPipeId, capturePipeId, factory);
        break;
    case SOLUTION_PROCESS_POST:
        ret = m_handleFramePostProcess(frame, prevPipeId, capturePipeId, factory);
        break;
    default:
        CLOGW("Unknown SOLUTION_PROCESS_TYPE(%d)", type);
        break;
    }

    return ret;
}

status_t ExynosCameraSolutionSWVdis::m_handleFramePreProcess(sp<ExynosCameraFrame> frame,
                                                                int prevPipeId,
                                                                int capturePipeId,
                                                                ExynosCameraFrameFactory *factory)

{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;

    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer meBuffer;
    entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_INVALID;

    int pipeId = getPipeId();


    /* 1. Getting VDIS input src buffer from dst buffer of previous pipe */
    ret = frame->getDstBuffer(prevPipeId, &srcBuffer, factory->getNodeType(capturePipeId));
    if (ret != NO_ERROR || srcBuffer.index < 0) {
        CLOGE("Failed to get DST buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        capturePipeId, srcBuffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    ret = frame->getDstBufferState(prevPipeId, &bufferState, factory->getNodeType(capturePipeId));
    if (ret < 0) {
        CLOGE("getDstBufferState fail, pipeId(%d), ret(%d)", prevPipeId, ret);
        return ret;
    }

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("Dst buffer state is error index(%d), framecount(%d), pipeId(%d)",
                            srcBuffer.index, frame->getFrameCount(), prevPipeId);
        return INVALID_OPERATION;
    }

    CLOGD("[OSC] Idx(%d) addr : %p", srcBuffer.index, srcBuffer.addr[0]);
    /* 2. Setting VDIS input src buffer to entity PIPE_VDIS of frame */
    ret = frame->setSrcBuffer(pipeId, srcBuffer, OUTPUT_NODE_1);
    if (ret != NO_ERROR || srcBuffer.index < 0) {
        CLOGE("Failed to set SRC buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        pipeId, srcBuffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    /* 3. Getting ME buffer */
    int mePipeId = PIPE_ME;
    int meLeaderPipe = m_pParameters->getLeaderPipeOfMe();
    if (meLeaderPipe < 0) return INVALID_OPERATION;

    ret = frame->getDstBuffer(meLeaderPipe, &meBuffer, factory->getNodeType(mePipeId));
    if (ret != NO_ERROR || meBuffer.index < 0) {
        CLOGE("Failed to get ME buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        meLeaderPipe, meBuffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    ret = frame->getDstBufferState(meLeaderPipe, &bufferState, factory->getNodeType(mePipeId));
    if (ret < 0) {
        CLOGE("Failed to get ME bufferState, pipeId(%d), ret(%d)", meLeaderPipe, ret);
        return ret;
    }

    if (bufferState == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("ME buffer state is error index(%d), framecount(%d), pipeId(%d)",
                            meBuffer.index, frame->getFrameCount(), meLeaderPipe);
        return INVALID_OPERATION;
    }

    /* 4. Setting ME buffer to entity PIPE_VDIS of frame */
    ret = frame->setSrcBuffer(pipeId, meBuffer, OUTPUT_NODE_2);
    if (ret != NO_ERROR || meBuffer.index < 0) {
        CLOGE("Failed to srt SRC buffer. pipeId %d bufferIndex %d frameCount %d ret %d",
                                        pipeId, meBuffer.index, frame->getFrameCount(), ret);
        return ret;
    }

    //for testing
    //ret = -1;
    return ret;
}

status_t ExynosCameraSolutionSWVdis::m_handleFramePostProcess(sp<ExynosCameraFrame> frame,
                                                                int prevPipeId,
                                                                int capturePipeId,
                                                                ExynosCameraFrameFactory *factory)

{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;

    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer meBuffer;
    entity_buffer_state_t srcBufferState = ENTITY_BUFFER_STATE_NOREQ;

    /* Getting VDIS input src buffer to release */
    ret = frame->getSrcBufferState(prevPipeId, &srcBufferState);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get src buffer state");
    } else {

        switch(srcBufferState) {
        case ENTITY_BUFFER_STATE_COMPLETE:
            /* Release src buffer */
            ret = frame->getSrcBuffer(prevPipeId, &srcBuffer);
            if (ret != NO_ERROR || srcBuffer.index < 0) {
                CLOGE("Failed to get SRC_BUFFER. pipeId(%d) bufferIndex(%d) frameCount(%d) ret(%d)",
                        prevPipeId, srcBuffer.index, frame->getFrameCount(), ret);
                return ret;
            }

            if(m_pBufferSupplier != NULL) {
                m_pBufferSupplier->putBuffer(srcBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("Failed to release ME buffer, pipeId(%d), ret(%d), frame(%d) buffer.index(%d)",
                            prevPipeId, ret, frame->getFrameCount(), meBuffer.index);
                    return ret;
                }
                CLOGD("Success to release src buffer(%d)", srcBuffer.index);
            } else {
                CLOGE("bufferSupplier is NULL!!");
            }

            break;
        case ENTITY_BUFFER_STATE_PROCESSING:
            /* Skip to release src buffer */
            CLOGD("bufferState(ENTITY_BUFFER_STATE_PROCESSING)");
            break;
        case ENTITY_BUFFER_STATE_ERROR:
            CLOGD("bufferState(ENTITY_BUFFER_STATE_ERROR)");
            break;
        default:
            break;
        }

    }

#ifdef SUPPORT_ME
    /* Relase ME buffer */
    ret = frame->getSrcBuffer(prevPipeId, &meBuffer, OUTPUT_NODE_2);
    if (ret != NO_ERROR) {
        CLOGE("Failed to get ME buffer, pipeId(%d), ret(%d), frame(%d)",
                prevPipeId, ret, frame->getFrameCount());
        return ret;
    }

    if(meBuffer.index < 0) {
        CLOGE("ME buffer index(%d) is invalid", meBuffer.index);
        return ret;
    }

    ret = m_pBufferSupplier->putBuffer(meBuffer);
    if (ret != NO_ERROR) {
        CLOGE("Failed to release ME buffer, pipeId(%d), ret(%d), frame(%d) buffer.index(%d)",
                prevPipeId, ret, frame->getFrameCount(), meBuffer.index);
        return ret;
    }

    CLOGD("Success to release me buffer(%d)", meBuffer.index);
#endif

    return ret;
}


status_t ExynosCameraSolutionSWVdis::m_adjustSize(void)
{
    CLOGD("+ IN + ");

    status_t ret = NO_ERROR;
    int portId = m_pParameters->getRecordingPortId();
    int fps = m_pConfigurations->getModeValue(CONFIGURATION_RECORDING_FPS);
    int videoW = 0, videoH = 0;
    int hwWidth = 0, hwHeight = 0;

    m_pConfigurations->getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&videoW, (uint32_t *)&videoH);

    if (videoW == 1920 && videoH == 1080) {
        hwWidth = VIDEO_MARGIN_FHD_W;
        hwHeight = VIDEO_MARGIN_FHD_H;
    } else if (videoW == 1280 && videoH == 720){
        hwWidth = VIDEO_MARGIN_HD_W;
        hwHeight = VIDEO_MARGIN_HD_H;
    } else {
        hwWidth = videoW;
        hwHeight = videoH;
    }

    if (hwWidth == 0 || hwHeight == 0) {
        CLOGE("Not supported VDIS size %dx%d fps %d", videoW, videoH, fps);
        ret = BAD_VALUE;
    }

    if (ret != NO_ERROR) {
        hwWidth = videoW;
        hwHeight = videoH;
    }

    /* Update HwYuv size */
    ret = m_pParameters->checkHwYuvSize(hwWidth, hwHeight, portId);
    if (ret != NO_ERROR) {
        CLOGE("Failed to setHwYuvSize for PREVIEW stream(VDIS). size %dx%d outputPortId %d",
                hwWidth, hwHeight, portId);
    }

    return ret;
}
