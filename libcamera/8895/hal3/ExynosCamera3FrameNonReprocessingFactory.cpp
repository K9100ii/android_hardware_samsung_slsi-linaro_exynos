/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCamera3FrameNonReprocessingFactory"
#include <cutils/log.h>

#include "ExynosCamera3FrameNonReprocessingFactory.h"

namespace android {

ExynosCamera3FrameNonReprocessingFactory::~ExynosCamera3FrameNonReprocessingFactory()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCamera3FrameNonReprocessingFactory::create()
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructNonReprocessingPipes();

    /* GSC_PICTURE pipe initialize */
    ret = m_pipes[PIPE_GSC_PICTURE]->create();
    if (ret != NO_ERROR) {
        CLOGE("GSC_PICTURE create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_GSC_PICTURE]->getPipeName(), PIPE_GSC_PICTURE);

    /* JPEG pipe initialize */
    ret = m_pipes[PIPE_JPEG]->create();
    if (ret != NO_ERROR) {
        CLOGE("JPEG create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[PIPE_JPEG]->getPipeName(), PIPE_JPEG);

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::initPipes(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    ret = m_transitState(FRAME_FACTORY_STATE_INIT);

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::preparePipes(void)
{
    status_t ret = NO_ERROR;

    /* NOTE: Prepare for 3AA is moved after ISP stream on */
    /* we must not qbuf before stream on, when sensor group. */
    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::startPipes(void)
{
    status_t ret = NO_ERROR;

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        return ret;
    }

    CLOGI("Starting Success!");

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::stopPipes(void)
{
    status_t ret = NO_ERROR;

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        return ret;
    }

    CLOGI("Stopping  Success!");

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGI("start pre-ordered initial pipe thread");

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::setStopFlag(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCamera3FrameNonReprocessingFactory::createNewFrame(uint32_t frameCount)
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

    frame = m_frameMgr->createFrame(m_parameters, frameCount, FRAME_TYPE_REPROCESSING);

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("frame(%d) metadata initialize fail", frameCount);

    /* set GSC for Capture pipe to linkageList */
    pipeId = PIPE_GSC_PICTURE;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);
    if (m_requestGSC_PICTURE == true) {
        requestEntityCount++;
    }

    /* set JPEG pipe to linkageList */
    pipeId = PIPE_JPEG;
    newEntity[pipeId] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[pipeId]);
    if (m_requestJPEG == true) {
        requestEntityCount++;
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

status_t ExynosCamera3FrameNonReprocessingFactory::m_setupConfig()
{
    CLOGI("");

    status_t ret = NO_ERROR;

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->is3aaIspOtf();
    m_flagIspTpuOTF = m_parameters->isIspTpuOtf();
    m_flagIspMcscOTF = m_parameters->isIspMcscOtf();
    m_flagTpuMcscOTF = m_parameters->isTpuMcscOtf();

    m_supportReprocessing = m_parameters->isReprocessingCapture();

    /* GSC for Capture */
    m_nodeNums[PIPE_GSC_PICTURE][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG */
    m_nodeNums[PIPE_JPEG][OUTPUT_NODE] = -1;

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::m_constructNonReprocessingPipes()
{
    CLOGI("");

    status_t ret = NO_ERROR;
    int pipeId = -1;

    /* GSC for Capture */
    pipeId = PIPE_GSC_PICTURE;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_GSC_PICTURE");

    /* JPEG */
    pipeId = PIPE_JPEG;
    m_pipes[pipeId] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, m_nodeNums[pipeId]);
    m_pipes[pipeId]->setPipeId(pipeId);
    m_pipes[pipeId]->setPipeName("PIPE_JPEG");

    CLOGI("pipe ids for preview");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("-> m_pipes[%d] : PipeId(%d)" , i, m_pipes[i]->getPipeId());
        }
    }

    return ret;
}

status_t ExynosCamera3FrameNonReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    return ret;
}

void ExynosCamera3FrameNonReprocessingFactory::m_init(void)
{
    m_flagReprocessing = false;
}

}; /* namespace android */
