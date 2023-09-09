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
#define LOG_TAG "ExynosCameraActivityControl"

#include "ExynosCameraActivityControl.h"

namespace android {

ExynosCameraActivityControl::ExynosCameraActivityControl(int cameraId)
{
    m_autofocusMgr = new ExynosCameraActivityAutofocus(cameraId);
    m_flashMgr = new ExynosCameraActivityFlash(cameraId);
    m_specialCaptureMgr = new ExynosCameraActivitySpecialCapture(cameraId);
    m_uctlMgr = new ExynosCameraActivityUCTL(cameraId);

    m_camId = cameraId;
}

ExynosCameraActivityControl::~ExynosCameraActivityControl()
{
    if (m_autofocusMgr != NULL) {
        delete m_autofocusMgr;
        m_autofocusMgr = NULL;
    }

    if (m_flashMgr != NULL) {
        delete m_flashMgr;
        m_flashMgr = NULL;
    }

    if (m_specialCaptureMgr != NULL) {
        delete m_specialCaptureMgr;
        m_specialCaptureMgr = NULL;
    }

    if (m_uctlMgr != NULL) {
        delete m_uctlMgr;
        m_uctlMgr = NULL;
    }
}

#ifdef OIS_CAPTURE
void ExynosCameraActivityControl::setOISCaptureMode(bool oisMode)
{
    if (oisMode)
        m_specialCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_OIS);
    else
        m_specialCaptureMgr->setCaptureMode(ExynosCameraActivitySpecialCapture::SCAPTURE_MODE_NONE);
}
#endif

void ExynosCameraActivityControl::activityBeforeExecFunc(int pipeId, void *args)
{
    switch(pipeId) {
    case PIPE_FLITE:
        m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
        break;
    case PIPE_3AA:
        if (m_checkSensorOnThisPipe(pipeId, args) == true) {
            m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
            m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
            m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
            m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_BEFORE, args);
        }

        m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_BEFORE_HAL3, args);
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_BEFORE_HAL3, args);
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_BEFORE_HAL3, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_BEFORE, args);
        break;
    case PIPE_ISP:
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_BEFORE, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_BEFORE, args);
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_BEFORE, args);
        break;
    case PIPE_VRA:
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_VRA_BEFORE, args);
        break;
    default:
        break;
    }
}

void ExynosCameraActivityControl::activityAfterExecFunc(int pipeId, void *args)
{
    switch(pipeId) {
    case PIPE_FLITE:
        m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
        break;
    case PIPE_3AA:
        if (m_checkSensorOnThisPipe(pipeId, args) == true) {
            m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
            m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
            m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
            m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_SENSOR_AFTER, args);
        }

        m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_AFTER, args);
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_AFTER_HAL3, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_3A_AFTER, args);
        break;
    case PIPE_ISP:
        m_autofocusMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_AFTER, args);
        m_flashMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_AFTER, args);
        m_specialCaptureMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_AFTER, args);
        m_uctlMgr->execFunction(ExynosCameraActivityBase::CALLBACK_TYPE_ISP_AFTER, args);
        break;
    default:
        break;
    }
}

ExynosCameraActivityFlash *ExynosCameraActivityControl::getFlashMgr(void)
{
    return m_flashMgr;
}

ExynosCameraActivitySpecialCapture *ExynosCameraActivityControl::getSpecialCaptureMgr(void)
{
    return m_specialCaptureMgr;
}

ExynosCameraActivityAutofocus*ExynosCameraActivityControl::getAutoFocusMgr(void)
{
    return m_autofocusMgr;
}

ExynosCameraActivityUCTL*ExynosCameraActivityControl::getUCTLMgr(void)
{
    return m_uctlMgr;
}

bool ExynosCameraActivityControl::m_checkSensorOnThisPipe(int pipeId, void *args)
{
    bool flagCheckSensor = false;

    if (pipeId != PIPE_3AA) {
        ALOGE("ERR(%s[%d]):pipeId(%d) != PIPE_3AA. so, fail", __FUNCTION__, __LINE__, pipeId);
        return false;
    }

    if (args == NULL) {
        ALOGE("ERR(%s[%d]):pipeId(%d), args == NULL. so, fail", __FUNCTION__, __LINE__, pipeId);
        return false;
    }

    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):pipeId(%d), shot_ext == NULL. so, fail", __FUNCTION__, __LINE__, pipeId);
        return false;
    }

    // we need to change to leader of sensor
    switch (shot_ext->node_group.leader.vid) {
    case FIMC_IS_VIDEO_SS0_NUM:
    case FIMC_IS_VIDEO_SS1_NUM:
    case FIMC_IS_VIDEO_SS2_NUM:
    case FIMC_IS_VIDEO_SS3_NUM:
        flagCheckSensor = true;
        break;
    default:
        break;
    }

    return flagCheckSensor;
}
}; /* namespace android */

