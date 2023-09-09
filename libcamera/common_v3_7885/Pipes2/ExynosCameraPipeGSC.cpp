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
#define LOG_TAG "ExynosCameraPipeGSC"
#include <cutils/log.h>

#include "ExynosCameraPipeGSC.h"

namespace android {

ExynosCameraPipeGSC::~ExynosCameraPipeGSC()
{
    this->destroy();
}

status_t ExynosCameraPipeGSC::create(__unused int32_t *sensorIds)
{
    CSC_METHOD cscMethod = CSC_METHOD_HW;

    m_csc = csc_init(cscMethod);
    if (m_csc == NULL) {
        CLOGE("csc_init() fail");
        return INVALID_OPERATION;
    }

    csc_set_hw_property(m_csc, m_property, m_gscNum);

    ExynosCameraSWPipe::create(sensorIds);

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::destroy(void)
{
    if (m_csc != NULL)
        csc_deinit(m_csc);
    m_csc = NULL;

    ExynosCameraSWPipe::release();

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::start(void)
{
    CLOGD("");

    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::stop(void)
{
    CLOGD("");

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    m_flagTryStop = false;

    return NO_ERROR;
}

status_t ExynosCameraPipeGSC::m_run(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraBuffer srcBuffer;
    ExynosCameraBuffer dstBuffer;
    ExynosRect srcRect;
    ExynosRect dstRect;
    ExynosCameraFrameEntity *entity = NULL;

    int ret = 0;
    int rotation = 0;
    int flipHorizontal = 0;
    int flipVertical = 0;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
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

    ret = newFrame->getSrcRect(getPipeId(), &srcRect);
    ret = newFrame->getDstRect(getPipeId(), &dstRect);

    switch (srcRect.colorFormat) {
    case V4L2_PIX_FMT_NV21:
        srcRect.fullH = ALIGN_UP(srcRect.fullH, 2);
        break;
    default:
        srcRect.fullH = ALIGN_UP(srcRect.fullH, GSCALER_IMG_ALIGN);
        break;
    }

    csc_set_src_format(m_csc,
        ALIGN_UP(srcRect.fullW, GSCALER_IMG_ALIGN),
        srcRect.fullH,
        srcRect.x, srcRect.y, srcRect.w, srcRect.h,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(srcRect.colorFormat),
        0);

    csc_set_dst_format(m_csc,
        dstRect.fullW, dstRect.fullH,
        dstRect.x, dstRect.y, dstRect.w, dstRect.h,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(dstRect.colorFormat),
        0);

    ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer);
    if (ret < 0) {
        CLOGE("frame get src buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    ret = newFrame->getDstBuffer(getPipeId(), &dstBuffer);
    if (ret < 0) {
        CLOGE("frame get dst buffer fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    csc_set_src_buffer(m_csc,
            (void **)srcBuffer.fd, CSC_MEMORY_TYPE);

    csc_set_dst_buffer(m_csc,
            (void **)dstBuffer.fd, CSC_MEMORY_TYPE);

    if (csc_convert_with_rotation(m_csc, rotation, flipHorizontal, flipVertical) != 0)
        CLOGE("csc_convert() fail");

    CLOGV("Rotation(%d), flip horizontal(%d), vertical(%d)",
             rotation, flipHorizontal, flipVertical);

    CLOGV("CSC(%d) converting done", m_gscNum);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;

func_exit:

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("set entity state fail, ret(%d)", ret);
        /* TODO: doing exception handling */
        return OK;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);
    return NO_ERROR;
}

void ExynosCameraPipeGSC::m_init(int32_t *nodeNums)
{
    if (nodeNums == NULL)
        m_gscNum = -1;
    else
        m_gscNum = nodeNums[0];

    m_csc = NULL;
    m_property = (nodeNums == NULL) ? CSC_HW_PROPERTY_DEFAULT : CSC_HW_PROPERTY_FIXED_NODE;
}

}; /* namespace android */
