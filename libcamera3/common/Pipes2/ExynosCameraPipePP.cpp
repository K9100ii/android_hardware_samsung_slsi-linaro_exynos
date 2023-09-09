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
#define LOG_TAG "ExynosCameraPipePP"

#include "ExynosCameraPipePP.h"

namespace android {

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

status_t ExynosCameraPipePP::startThread(void)
{
    CLOGV("");

    if (m_outputFrameQ == NULL) {
        CLOGE("outputFrameQ is NULL, cannot start");
        return INVALID_OPERATION;
    }

    m_mainThread->run(m_name);

    CLOGI("startThread is succeed (%d)", getPipeId());

    return NO_ERROR;
}

bool ExynosCameraPipePP::m_checkThreadLoop()
{
    Mutex::Autolock lock(m_pipeframeLock);
    bool loop = false;

    if (m_oneShotMode == false)
        loop = true;

    if (m_inputFrameQ->getSizeOfProcessQ() > 0)
        loop = true;

    if (m_flagTryStop == true)
        loop = false;

    return loop;
}

bool ExynosCameraPipePP::m_mainThreadFunc(void)
{
    status_t ret = NO_ERROR;

    ret = m_run();
    if (ret != NO_ERROR) {
        if (ret != TIMED_OUT) {
            CLOGE("m_putBuffer fail");
        }
    }

    return m_checkThreadLoop();
}

status_t ExynosCameraPipePP::m_run(void)
{
    status_t ret = NO_ERROR;

    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;
    int pipeId = getPipeId();

    ExynosCameraImage srcImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];
    ExynosCameraImage dstImage[ExynosCameraImageCapacity::MAX_NUM_OF_IMAGE_MAX];

    int numOfSrcImage = 0;
    int numOfDstImage = 0;
    int rotation = 0;
    int flipHorizontal = 0;
    int flipVertical = 0;
    int multiShotCount = 0;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGV("wait timeout");
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

    entity = newFrame->searchEntityByPipeId(pipeId);
    if (entity == NULL
        || entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("frame(%d) entityState(ENTITY_BUFFER_STATE_ERROR), skip msc", newFrame->getFrameCount());
        goto func_exit;
    }

    //////////////////////////////
    //// This is core drawing part
    if (m_pp == NULL) {
        CLOGE("m_pp == NULL. so, fail, frameCount(%d)", newFrame->getFrameCount());

        goto func_exit;
    }

    numOfSrcImage = m_pp->getNumOfSrcImage();
    numOfDstImage = m_pp->getNumOfDstImage();

    // SRC
    multiShotCount = newFrame->getFrameSpecialCaptureStep();

    if (numOfSrcImage < 1) {
        CLOGE("Invalid numOfSrcImage(%d). it must be bigger than 1",
            numOfDstImage);
        goto func_exit;
    }

    for (int i = 0; i < numOfSrcImage; i++) {
        int nodeType = (int)OUTPUT_NODE + i;

        ret = newFrame->getSrcBuffer(pipeId, &(srcImage[i].buf), i);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getSrcBuffer(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        ret = newFrame->getSrcRect(pipeId, &(srcImage[i].rect), i);
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getSrcRect(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        srcImage[i].rotation = 0;
        srcImage[i].flipH = false;
        srcImage[i].flipV = false;
        srcImage[i].multiShotCount = multiShotCount;
    }

    // DST
    rotation = newFrame->getRotation(pipeId);
    CLOGV(" getPipeId(%d), rotation(%d)", pipeId, rotation);

    flipHorizontal = newFrame->getFlipHorizontal(pipeId);
    flipVertical = newFrame->getFlipVertical(pipeId);

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

        ret = newFrame->getDstRect(pipeId, &(dstImage[i].rect));
        if (ret != NO_ERROR) {
            CLOGE("newFrame->getDstRect(pipeId(%d), nodeType : %d)", pipeId, nodeType);
            goto func_exit;
        }

        dstImage[i].rotation = rotation;
        dstImage[i].flipH = (flipHorizontal == 1) ? true : false;
        dstImage[i].flipV = (flipVertical   == 1) ? true : false;
    }

    /*
     * HACK: libacryl(G2D) cannot handle YV12(V4L2_PIX_FMT_YVU420).
     *       so, it will have nextPP(PPLibcsc).
     */
    if (m_nodeNum == 222) {
        if (m_pp->isSupportedSrcImage(srcImage[0].rect.colorFormat, srcImage[0].rect.fullW) == false
            || m_pp->isSupportedDstImage(dstImage[0].rect.colorFormat, dstImage[0].rect.fullW) == false) {
            ExynosCameraPP *nextPP = m_pp->getNextPP();

            if (nextPP == NULL) {
                nextPP = ExynosCameraPPFactory::newPP(m_cameraId, m_parameters, PICTURE_GSC_NODE_NUM);

                CLOGD("m_pp(%s) can't support [SRC]%c%c%c%c, fullW(%d) / [DST]%c%c%c%c, fullW(%d). make nextPP(nodeNum : %d)",
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
                    CLOGE("m_pp(%s)->setNextPP(%s) fail",
                            m_pp->getName(), nextPP->getName());
                    goto func_exit;
                }
            }
        }
    }

    ret = m_pp->draw(srcImage, dstImage);
    if (ret != NO_ERROR) {
        CLOGE("m_pp->draw(numOfSrcImage(%d), numOfDstImage(%d)) fail",
                numOfSrcImage, numOfDstImage);
        goto func_exit;
    }

    //////////////////////////////
    ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_COMPLETE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setDstBufferState(%s, ENTITY_BUFFER_STATE_COMPLETE) fail, frameCount(%d)",
            this->getName(), newFrame->getFrameCount());
        goto func_exit;
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setEntityState(%s, ENTITY_STATE_FRAME_DONE) fail, frameCount(%d)",
            this->getName(), newFrame->getFrameCount());
        goto func_exit;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;

func_exit:
    ret = newFrame->setDstBufferState(pipeId, ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setDstBufferState(%s, ENTITY_BUFFER_STATE_COMPLETE) fail, frameCount(%d)",
            this->getName(), newFrame->getFrameCount());
    }

    ret = newFrame->setEntityState(pipeId, ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("newFrame->setEntityState(%s, ENTITY_STATE_FRAME_DONE) fail, frameCount(%d)",
            this->getName(), newFrame->getFrameCount());
    }

    m_outputFrameQ->pushProcessQ(&newFrame);
    return NO_ERROR;
}

}; /* namespace android */
