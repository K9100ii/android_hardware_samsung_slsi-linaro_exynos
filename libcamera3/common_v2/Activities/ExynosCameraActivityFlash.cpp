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
#define LOG_TAG "ExynosCameraActivityFlash"
#include <log/log.h>


#include "ExynosCameraActivityFlash.h"

namespace android {

class ExynosCamera;

ExynosCameraActivityFlash::ExynosCameraActivityFlash(int cameraId) : ExynosCameraActivityBase(cameraId)
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0x1F;
    t_reqStatus = 0;

    m_isNeedFlash = false;
    m_isNeedCaptureFlash = true;
    m_isNeedFlashOffDelay = false;
    m_flashTriggerStep = 0;

    m_flashStepErrorCount = -1;

    m_checkMainCaptureRcount = false;
    m_checkMainCaptureFcount = false;

    m_waitingCount = -1;
    m_isCapture = false;
    m_timeoutCount = 0;
    m_aeWaitMaxCount = 0;

    m_flashStatus = FLASH_STATUS_OFF;
    m_flashReq = FLASH_REQ_OFF;
    m_flashStep = FLASH_STEP_OFF;
    m_overrideFlashControl = false;
    m_ShotFcount = 0;

    m_flashPreStatus = FLASH_STATUS_OFF;
    m_aePreState = AE_STATE_INACTIVE;
    m_flashTrigger = FLASH_TRIGGER_OFF;
    m_mainWaitCount = 0;
    m_lcdWaitCount = 0;

    m_aeflashMode = AA_FLASHMODE_OFF;
    m_checkFlashStepCancel = false;
    m_mainCaptureRcount = 0;
    m_mainCaptureFcount = 0;
    m_isRecording = false;
    m_isFlashOff = false;
    m_flashMode = CAM2_FLASH_MODE_OFF;
    m_currentIspInputFcount = 0;
    m_awbMode = AA_AWBMODE_OFF;
    m_aeState = AE_STATE_INACTIVE;
    m_aeMode = AA_AEMODE_OFF;
    m_curAeState = AE_STATE_INACTIVE;

    m_waitingFlashStableCount = 0;
    m_prevFlashMode = CAM2_FLASH_MODE_OFF;
    m_isNeedFlashMainStart = false;
}

ExynosCameraActivityFlash::~ExynosCameraActivityFlash()
{
}

int ExynosCameraActivityFlash::t_funcNull(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityFlash::t_funcSensorBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    m_reqBuf = *buf;

    return 1;
}

int ExynosCameraActivityFlash::t_funcSensorAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

    if (m_checkMainCaptureFcount == true) {
        /* Update m_waitingCount */
        m_waitingCount = checkMainCaptureFcount(shot_ext->shot.dm.request.frameCount);
        CLOGV("m_waitingCount=0x%x", m_waitingCount);
    }

    return 1;
}

int ExynosCameraActivityFlash::t_funcISPBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityFlash::t_funcISPAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityFlash::t_funcVRABefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityFlash::t_funcVRAAfter(__unused void *args)
{
    return 1;
}

bool ExynosCameraActivityFlash::setFlashReq(enum FLASH_REQ flashReqVal)
{
    if (m_flashReq != flashReqVal) {
        m_flashReq = flashReqVal;
        setFlashStep(ExynosCameraActivityFlash::FLASH_STEP_OFF);
        if (m_isRecording == false)
            m_isNeedCaptureFlash = true;
        CLOGD("flashReq=%d", (int)m_flashReq);
    }

    if (m_flashReq == FLASH_REQ_ON)
        m_isNeedFlash = true;
    if (m_flashReq == FLASH_REQ_TORCH)
        m_isNeedCaptureFlash = false;
    if (m_flashReq == FLASH_REQ_OFF) {
        m_isNeedCaptureFlash = false;
        m_isFlashOff = false;
    }

    return true;
}

bool ExynosCameraActivityFlash::setFlashReq(enum FLASH_REQ flashReqVal, bool overrideFlashControl)
{
    m_overrideFlashControl = overrideFlashControl;

    if (m_overrideFlashControl == true) {
        return this->setFlashReq(flashReqVal);
    }

    return true;
}

bool ExynosCameraActivityFlash::setRecordingHint(bool hint)
{
    m_isRecording = hint;
    if ((m_isRecording == true) || (m_flashReq == FLASH_REQ_TORCH))
        m_isNeedCaptureFlash = false;
    else
        m_isNeedCaptureFlash = true;

    return true;
}

enum ExynosCameraActivityFlash::FLASH_REQ ExynosCameraActivityFlash::getFlashReq(void)
{
    return m_flashReq;
}

int ExynosCameraActivityFlash::getFlashStatus()
{
    return m_aeflashMode;
}

bool ExynosCameraActivityFlash::setFlashExposure(enum aa_aemode aeModeVal)
{
    m_aeMode = aeModeVal;
    CLOGV("aeMode=%d", (int)m_aeMode);
    return true;
}

bool ExynosCameraActivityFlash::getFlashStep(enum FLASH_STEP *flashStepVal)
{
    *flashStepVal = m_flashStep;

    return true;
}

bool ExynosCameraActivityFlash::setFlashTrigerPath(enum FLASH_TRIGGER flashTriggerVal)
{
    m_flashTrigger = flashTriggerVal;

    CLOGD("flashTriggerVal=%d", (int)flashTriggerVal);

    return true;
}

bool ExynosCameraActivityFlash::getFlashTrigerPath(enum FLASH_TRIGGER *flashTriggerVal)
{
    *flashTriggerVal = m_flashTrigger;

    return true;
}

bool ExynosCameraActivityFlash::setShouldCheckedFcount(int fcount)
{
    m_mainCaptureFcount = fcount;
    CLOGV("mainCaptureFcount=%d", m_mainCaptureFcount);

    return true;
}

int ExynosCameraActivityFlash::checkMainCaptureFcount(int fcount)
{
    CLOGV("mainCaptureFcount=%d, fcount=%d",
        m_mainCaptureFcount, fcount);

    if (fcount < m_mainCaptureFcount)
        return (m_mainCaptureFcount - fcount); /* positive count  */
    else
        return 0;
}

bool ExynosCameraActivityFlash::getNeedFlash()
{
    CLOGD("m_isNeedFlash=%d", m_isNeedFlash);
    return m_isNeedFlash;
}

bool ExynosCameraActivityFlash::getNeedCaptureFlash()
{
    if (m_isNeedFlash == true)
        return m_isNeedCaptureFlash;

    return false;
}

unsigned int ExynosCameraActivityFlash::getShotFcount()
{
    return m_ShotFcount;
}
void ExynosCameraActivityFlash::resetShotFcount(void)
{
    m_ShotFcount = 0;
}
void ExynosCameraActivityFlash::setCaptureStatus(bool isCapture)
{
    m_isCapture = isCapture;
}

void ExynosCameraActivityFlash::notifyAfResultHAL3(void)
{
    if (m_flashStatus == FLASH_STATUS_PRE_AF) {
        setFlashStep(FLASH_STEP_PRE_AF_DONE);
        CLOGD(" AF DONE (for flash)");
    }
}

void ExynosCameraActivityFlash::notifyAeResult(void)
{
    if (m_flashStatus == FLASH_STATUS_PRE_ON) {
        setFlashStep(FLASH_STEP_PRE_DONE);
        CLOGD(" AE DONE (for flash)");
    }
}

void ExynosCameraActivityFlash::setNeedFlashMainStart(bool isFlashMainStart)
{
    m_isNeedFlashMainStart = isFlashMainStart;

}
bool ExynosCameraActivityFlash::getNeedFlashMainStart(void)
{
    return m_isNeedFlashMainStart;
}

}/* namespace android */

