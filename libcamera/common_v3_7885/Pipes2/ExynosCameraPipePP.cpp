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
#define LOG_TAG "ExynosCameraPipePP"
#include <cutils/log.h>

#include "ExynosCameraPipePP.h"

namespace android {

ExynosCameraPipePP::~ExynosCameraPipePP()
{
    this->destroy();
}

status_t ExynosCameraPipePP::create(__unused int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    m_pp = ExynosCameraPPFactory::newPP(m_cameraId, m_nodeNum);
    if (m_pp == NULL) {
        CLOGE("ExynosCameraPPFactory::newPP(m_cameraId : %d, m_nodeNum : %d) fail", m_cameraId, m_nodeNum);
        return INVALID_OPERATION;
    }

    if (m_pp->flagCreated() == false) {
        ret = m_pp->create();
        if (ret != NO_ERROR) {
            CLOGE("m_pp->create() fail");
            return INVALID_OPERATION;
        }
    }

    ExynosCameraSWPipe::create(sensorIds);

    return NO_ERROR;
}

status_t ExynosCameraPipePP::destroy(void)
{
    status_t ret = NO_ERROR;

    if (m_pp != NULL) {
        if (m_pp->flagCreated() == true) {
            ret = m_pp->destroy();
            if (ret != NO_ERROR) {
                CLOGE("m_pp->destroy() fail");
            }
        }

        SAFE_DELETE(m_pp);
    }

    ExynosCameraSWPipe::release();

    return NO_ERROR;
}

status_t ExynosCameraPipePP::start(void)
{
    CLOGD("");

    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipePP::stop(void)
{
    CLOGD("");

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipePP::m_run(void)
{
    status_t ret = NO_ERROR;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    int pipeId = getPipeId();

    ExynosCameraImageCapacity srcImageCapacity = m_pp->getSrcImageCapacity();
    ExynosCameraImageCapacity dstImageCapacity = m_pp->getDstImageCapacity();

    int numOfSrcImage = srcImageCapacity.getNumOfImage();
    int numOfDstImage = dstImageCapacity.getNumOfImage();

    ExynosCameraImage srcImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];
    ExynosCameraImage dstImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];
    int rotation = 0;
    int flipHorizontal = 0;
    int flipVertical = 0;

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
        return NO_ERROR;
    }

    entity = newFrame->searchEntityByPipeId(getPipeId());
    if (entity == NULL || entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("frame(%d) entityState(ENTITY_BUFFER_STATE_ERROR), skip msc", newFrame->getFrameCount());
        goto func_exit;
    }

    rotation = newFrame->getRotation(getPipeId());
    CLOGV(" getPipeId(%d), rotation(%d)", getPipeId(), rotation);

#ifdef PERFRAME_CONTROL_FOR_FLIP
    flipHorizontal = newFrame->getFlipHorizontal(getPipeId());
    flipVertical = newFrame->getFlipVertical(getPipeId());
#else
    flipHorizontal = m_parameters->getFlipHorizontal();
    flipVertical = m_parameters->getFlipVertical();
#endif

    //////////////////////////////
    //// This is core drawing part
    if (m_pp == NULL) {
        CLOGE("(%s) m_pp == NULL. so, fail, frameCount(%d)",
            this->getPipeName(), newFrame->getFrameCount());

        goto func_exit;
    }

    // SRC
    if (numOfSrcImage < 1) {
        CLOGE("Invalid numOfSrcImage(%d). it must be bigger than 1",
            numOfDstImage);
        goto func_exit;
    }

    for (int i = 0; i < numOfSrcImage; i++) {
        int nodeType = (int)OUTPUT_NODE + i;

        ret = newFrame->getSrcBuffer(pipeId, &(srcImage[i].buf), nodeType);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getSrcBuffer(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        ret = newFrame->getSrcRect(pipeId, &(srcImage[i].rect), nodeType);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getSrcRect(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        srcImage[i].rotation = 0;
        srcImage[i].flipH = false;
        srcImage[i].flipV = false;
    }

    // DST
    if (numOfDstImage < 1) {
        CLOGE("Invalid numOfDstImage(%d). it must be bigger than 1",
            numOfDstImage);
        goto func_exit;
    }

    for (int i = 0; i < numOfDstImage; i++) {
        int nodeType = (int)OUTPUT_NODE + i;

        ret = newFrame->getDstBuffer(pipeId, &(dstImage[i].buf), nodeType);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getDstBuffer(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        ret = newFrame->getDstRect(pipeId, &(dstImage[i].rect), nodeType);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getDstRect(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        dstImage[i].rotation = rotation;
        dstImage[i].flipH = (flipHorizontal == 1) ? true : false;
        dstImage[i].flipV = (flipVertical   == 1) ? true : false;
    }

    /*
     * libacryl(G2D) cannot handle YV12(V4L2_PIX_FMT_YVU420).
     * so, it will have nextPP(PPLibcsc).
     */
    if (strcmp("ExynosCameraPPLibacryl", m_pp->getName()) == 0) {
        if (srcImageCapacity.flagSupportedColorFormat(srcImage[0].rect.colorFormat, srcImage[0].rect.fullW) == false ||
            dstImageCapacity.flagSupportedColorFormat(dstImage[0].rect.colorFormat, dstImage[0].rect.fullW) == false) {

            ExynosCameraPP *nextPP = m_pp->getNextPP();

            if (nextPP == NULL) {
                nextPP = ExynosCameraPPFactory::newPP(m_cameraId, PICTURE_GSC_NODE_NUM);

                CLOGD("m_pp(%s) cannot support [SRC]%c%c%c%c, fullW(%d) / [DST]%c%c%c%c, fullW(%d). so, it will have nextPP(nodeNum : %d)",
                    m_pp->getName(), nextPP->getNodeNum(),
                    v4l2Format2Char(srcImage[0].rect.colorFormat, 0),
                    v4l2Format2Char(srcImage[0].rect.colorFormat, 1),
                    v4l2Format2Char(srcImage[0].rect.colorFormat, 2),
                    v4l2Format2Char(srcImage[0].rect.colorFormat, 3),
                    srcImage[0].rect.fullW,
                    v4l2Format2Char(dstImage[0].rect.colorFormat, 0),
                    v4l2Format2Char(dstImage[0].rect.colorFormat, 1),
                    v4l2Format2Char(dstImage[0].rect.colorFormat, 2),
                    v4l2Format2Char(dstImage[0].rect.colorFormat, 3),
                    dstImage[0].rect.fullW);

                ret = m_pp->setNextPP(nextPP);
                if (ret != NO_ERROR) {
                    CLOGE("m_pp(%s)->setNextPP(%s) fail", m_pp->getName(), nextPP->getName());
                    goto func_exit;
                }
            }
        }
    }

    if (numOfSrcImage == 1 && numOfDstImage == 1) {
        ret = m_pp->draw(srcImage[0], dstImage[0]);
        if (ret != NO_ERROR) {
            CLOGE("m_pp->draw() fail");
            goto func_exit;
        }
    } else {
        ret = m_pp->draw(numOfSrcImage, srcImage,
                         numOfDstImage, dstImage);
        if (ret != NO_ERROR) {
            CLOGE("m_pp->draw(numOfSrcImage(%d), numOfDstImage(%d)) fail",
                numOfSrcImage,
                numOfDstImage);
            goto func_exit;
        }
    }

    //////////////////////////////
    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setDstBufferState(%s, ENTITY_BUFFER_STATE_COMPLETE) fail, frameCount(%d)",
            this->getPipeName(), newFrame->getFrameCount());
        goto func_exit;
    }

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setEntityState(%s, ENTITY_STATE_FRAME_DONE) fail, frameCount(%d)",
            this->getPipeName(), newFrame->getFrameCount());
        goto func_exit;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;

func_exit:
    ret = newFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setDstBufferState(%s, ENTITY_BUFFER_STATE_COMPLETE) fail, frameCount(%d)",
            this->getPipeName(), newFrame->getFrameCount());
    }

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setEntityState(%s, ENTITY_STATE_FRAME_DONE) fail, frameCount(%d)",
            this->getPipeName(), newFrame->getFrameCount());
    }

    m_outputFrameQ->pushProcessQ(&newFrame);
    return NO_ERROR;
}

void ExynosCameraPipePP::m_init(int32_t *nodeNums)
{
    if (nodeNums == NULL)
        m_nodeNum = -1;
    else
        m_nodeNum = nodeNums[0];

    m_pp = NULL;
}

}; /* namespace android */
