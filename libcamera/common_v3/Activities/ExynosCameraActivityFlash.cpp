/*
**
** Copyright 2012, Samsung Electronics Co. LTD
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
#include <cutils/log.h>


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
    m_isMainFlashFiring = false;
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

    m_aeflashMode = AA_FLASHMODE_OFF;
    m_checkFlashStepCancel = false;
    m_checkFlashWaitCancel = false;
    m_mainCaptureRcount = 0;
    m_mainCaptureFcount = 0;
    m_isRecording = false;
    m_isFlashOff = false;
    m_flashMode = CAM2_FLASH_MODE_OFF;
    m_currentIspInputFcount = 0;
    m_awbMode = AA_AWBMODE_OFF;
    m_aeState = AE_STATE_INACTIVE;
    m_aeMode = AA_AEMODE_OFF;
    m_isPreFlash = false;
    m_curAeState = AE_STATE_INACTIVE;
    m_aeLock = false;
    m_awbLock = false;
    m_fpsValue = 1;
    m_manualExposureTime = 0;
    m_needAfTrigger = false;
}

ExynosCameraActivityFlash::~ExynosCameraActivityFlash()
{
    m_checkFlashWaitCancel = false;
}

int ExynosCameraActivityFlash::t_funcNull(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
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

int ExynosCameraActivityFlash::t_funcISPBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityFlash::t_funcISPAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityFlash::t_func3ABeforeHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    enum flash_mode tempFlashMode = CAM2_FLASH_MODE_NONE;

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

    m_currentIspInputFcount = shot_ext->shot.dm.request.frameCount;

    CLOGV("m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
            (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

    if (m_flashPreStatus != m_flashStatus) {
        CLOGD("m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
                (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

        m_flashPreStatus = m_flashStatus;
    }

    if (m_aePreState != m_aeState) {
        CLOGV("m_aeState=%d", (int)m_aeState);
        m_aePreState = m_aeState;
    }

    if (m_overrideFlashControl == false) {
        switch (shot_ext->shot.ctl.flash.flashMode) {
        case CAM2_FLASH_MODE_OFF:
            this->setFlashReq(ExynosCameraActivityFlash::FLASH_REQ_OFF);
            break;
        case CAM2_FLASH_MODE_TORCH:
            this->setFlashReq(ExynosCameraActivityFlash::FLASH_REQ_TORCH);
            break;
        case CAM2_FLASH_MODE_SINGLE:
            this->setFlashReq(ExynosCameraActivityFlash::FLASH_REQ_SINGLE);
            break;
        default:
            break;
        }
        tempFlashMode = shot_ext->shot.ctl.flash.flashMode;
    }

    if (m_flashStep == FLASH_STEP_CANCEL && m_checkFlashStepCancel == true) {
        CLOGV(" Flash step is CANCEL");
        m_isNeedFlash = false;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CANCEL;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;
        m_flashStepErrorCount = -1;

        m_checkMainCaptureRcount = false;
        m_checkMainCaptureFcount = false;
        m_checkFlashStepCancel = false;
        /* m_checkFlashWaitCancel = false; */
        m_isCapture = false;

        goto done;
    }

    if (m_flashReq == FLASH_REQ_OFF) {
        CLOGV(" Flash request is OFF");
        m_isNeedFlash = false;
        if (m_aeflashMode == AA_FLASHMODE_ON_ALWAYS)
            m_isNeedFlashOffDelay = true;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;
        m_flashStepErrorCount = -1;
        m_flashStep = FLASH_STEP_OFF;

        m_checkMainCaptureRcount = false;
        m_checkMainCaptureFcount = false;

        goto done;
    } else if (m_flashReq == FLASH_REQ_SINGLE) {
        CLOGV(" Flash request is SINGLE");
        m_isNeedFlash = true;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;

        goto done;
    } else if (m_flashReq == FLASH_REQ_TORCH) {
        CLOGV(" Flash request is TORCH");
        m_isNeedFlash = true;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON_ALWAYS;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;

        goto done;
    } else if (m_flashReq == FLASH_REQ_ON) {
        CLOGV(" Flash request is ON");
        m_isNeedFlash = true;

        if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_PRE_READY;
        } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
            CLOGV(" Flash status is PRE READY");

            if (m_flashStep == FLASH_STEP_PRE_START) {
                CLOGV(" Flash step is PRE START");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
#if 0
                if (shot_ext->shot.ctl.aa.afMode == AA_AFMODE_AUTO
                    || shot_ext->shot.ctl.aa.afMode == AA_AFMODE_MACRO) {
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
                } else {
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }
#endif
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            }
        } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
            CLOGV(" Flash status is PRE ON");

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            //shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_ON;
            m_aeWaitMaxCount--;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
            CLOGV(" Flash status is PRE AE DONE");

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            //shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
            CLOGV(" Flash status is PRE AF");

            if (m_needAfTrigger == true) {
                m_needAfTrigger = false;
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
            } else {
                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            }

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_AF;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
            CLOGV(" Flash status is PRE DONE");

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_waitingCount = -1;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
            CLOGV(" Flash status is MAIN READY");
            m_ShotFcount = 0;

            if (m_flashStep == FLASH_STEP_MAIN_START) {
                CLOGD(" Flash step is MAIN START (fcount %d)",
                        shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_ON;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            }
        } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
            CLOGV(" Flash status is MAIN ON (fcount %d)",
                    shot_ext->shot.dm.request.frameCount);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_MAIN_ON;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
            CLOGV(" Flash status is MAIN WAIT (fcount %d)",
                    shot_ext->shot.dm.request.frameCount);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_MAIN_WAIT;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
            CLOGV(" Flash status is MAIN DONE (fcount %d)",
                    shot_ext->shot.dm.request.frameCount);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_OFF;
            m_waitingCount = -1;
            m_aeWaitMaxCount = 0;
            m_isCapture = false;
        }
    } else if (m_flashReq == FLASH_REQ_AUTO) {
        CLOGV(" Flash request is AUTO");

        if (m_aeState == AE_STATE_INACTIVE) {
            CLOGV(" AE state is INACTIVE");
            m_isNeedFlash = false;

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_OFF;
            m_flashStep = FLASH_STEP_OFF;

            m_checkMainCaptureRcount = false;
            m_checkMainCaptureFcount = false;
            m_waitingCount = -1;

            goto done;
        } else if (m_aeState == AE_STATE_CONVERGED || m_aeState == AE_STATE_LOCKED_CONVERGED) {
            CLOGV(" AE state is CONVERGED");
            m_isNeedFlash = false;

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_OFF;
            m_flashStep = FLASH_STEP_OFF;

            m_isCapture = false;
            m_isPreFlash = false;

            m_checkMainCaptureRcount = false;
            m_checkMainCaptureFcount = false;
            m_waitingCount = -1;

            goto done;
        } else if (m_aeState == AE_STATE_FLASH_REQUIRED || m_aeState == AE_STATE_LOCKED_FLASH_REQUIRED) {
            CLOGV(" AE state is FLASH REQUIRED");
            m_isNeedFlash = true;

            if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_PRE_READY;
            } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
                CLOGV(" Flash status is PRE READY");

                if (m_flashStep == FLASH_STEP_PRE_START) {
                    CLOGV(" Flash step is PRE START");

                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;
#if 0
                    if (shot_ext->shot.ctl.aa.afMode == AA_AFMODE_AUTO
                            || shot_ext->shot.ctl.aa.afMode == AA_AFMODE_MACRO) {
                        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_CANCEL;
                    } else {
                        shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                    }
#endif
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                    m_flashStatus = FLASH_STATUS_PRE_ON;
                    m_aeWaitMaxCount--;
                }
            } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
                CLOGV(" Flash status is PRE ON");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                //shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
                CLOGV(" Flash status is PRE AE DONE");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                //shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
                CLOGV(" Flash status is PRE AF");

                if (m_needAfTrigger == true) {
                    m_needAfTrigger = false;
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                } else {
                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                }

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_AF;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
                CLOGV(" Flash status is PRE DONE");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
                CLOGV(" Flash status is MAIN READY");
                m_ShotFcount = 0;

                if (m_flashStep == FLASH_STEP_MAIN_START) {
                    CLOGV(" Flash step is MAIN START (fcount %d)",
                            shot_ext->shot.dm.request.frameCount);

                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;

                    m_flashStatus = FLASH_STATUS_MAIN_ON;
                    m_waitingCount--;
                    m_aeWaitMaxCount = 0;
                }
            } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
                CLOGV(" Flash status is MAIN ON (fcount %d)",
                        shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_ON;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
                CLOGV(" Flash status is MAIN WAIT (fcount %d)",
                        shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_WAIT;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
                CLOGV(" Flash status is MAIN DONE (fcount %d)",
                        shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_OFF;
                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
                m_isCapture = false;
            }
        }
    }

    if (0 < m_flashStepErrorCount)
        m_flashStepErrorCount++;

    CLOGV("aeflashMode=%d", (int)shot_ext->shot.ctl.aa.vendor_aeflashMode);

done:
    shot_ext->shot.ctl.flash.flashMode = tempFlashMode;

    if(shot_ext->shot.ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH)
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;

    CLOGV("aeflashMode(%d) ctl flashMode(%d)",
        (int)shot_ext->shot.ctl.aa.vendor_aeflashMode,
        (int)shot_ext->shot.ctl.flash.flashMode);

    return 1;
}

int ExynosCameraActivityFlash::t_func3AAfterHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext == NULL) {
        CLOGE("shot_ext is null");
        return false;
    }

    shot_ext->shot.dm.aa.aePrecaptureTrigger = shot_ext->shot.ctl.aa.aePrecaptureTrigger;

    if (m_isCapture == false)
        m_aeState = shot_ext->shot.dm.aa.aeState;

    m_curAeState = shot_ext->shot.dm.aa.aeState;

    /* Convert aeState for Locked */
    if (shot_ext->shot.dm.aa.aeState == AE_STATE_LOCKED_CONVERGED ||
        shot_ext->shot.dm.aa.aeState == AE_STATE_LOCKED_FLASH_REQUIRED) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_LOCKED;
    }

    if (shot_ext->shot.dm.aa.aeState == AE_STATE_SEARCHING_FLASH_REQUIRED) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_SEARCHING;
    }

    if (m_flashStep == FLASH_STEP_CANCEL &&
            m_checkFlashStepCancel == false) {
        m_flashStep = FLASH_STEP_OFF;
        m_flashStatus = FLASH_STATUS_OFF;

        goto done;
    }

    if (m_flashStep == FLASH_STEP_OFF) {
        if (shot_ext->shot.dm.aa.vendor_aeflashMode == AA_FLASHMODE_ON_ALWAYS) {
            shot_ext->shot.dm.flash.flashMode = CAM2_FLASH_MODE_TORCH;
        } else if (shot_ext->shot.ctl.flash.flashMode == CAM2_FLASH_MODE_SINGLE) {
            shot_ext->shot.dm.flash.flashMode = CAM2_FLASH_MODE_SINGLE;
            shot_ext->shot.dm.flash.flashState = FLASH_STATE_FIRED;
        }
    }

    if (m_flashReq == FLASH_REQ_OFF) {
        if (shot_ext->shot.dm.flash.vendor_flashReady == 3) {
            CLOGV(" flashReady = 3 frameCount %d",
                shot_ext->shot.dm.request.frameCount);
            m_isFlashOff = true;
        }
    }

    if (m_flashStatus == FLASH_STATUS_PRE_CHECK) {
        if (shot_ext->shot.dm.flash.vendor_decision == 2 ||
            FLASH_TIMEOUT_COUNT < m_timeoutCount) {
            m_flashStatus = FLASH_STATUS_PRE_READY;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;
        if (shot_ext->shot.dm.flash.vendor_flashReady == 1 ||
                FLASH_AE_TIMEOUT_COUNT < m_timeoutCount) {
            if (FLASH_AE_TIMEOUT_COUNT < m_timeoutCount)
                CLOGD("auto exposure timeoutCount %d",
                    m_timeoutCount);
            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;

        if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED ||
            shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_UNFOCUSED ||
            shot_ext->shot.dm.aa.afState == AA_AFSTATE_INACTIVE) {
            m_needAfTrigger = true;
            m_flashStatus = FLASH_STATUS_PRE_AF;
        } else if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED
                    || shot_ext->shot.dm.aa.afState == AA_AFSTATE_NOT_FOCUSED_LOCKED) {
            ALOGD("DEBUG(%s[%d]):FLASH_STATUS_PRE_DONE afTrigger(%d) af state(%d)",
                __FUNCTION__, __LINE__, shot_ext->shot.dm.aa.afTrigger, shot_ext->shot.dm.aa.afState);
            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else {
            CLOGD("FLASH_STATUS_PRE_AE_DONE aeState(%d) afState(%d)",
                shot_ext->shot.dm.aa.aeState, shot_ext->shot.dm.aa.afState);
            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;

        if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED ||
            shot_ext->shot.dm.aa.afState == AA_AFSTATE_NOT_FOCUSED_LOCKED ||
            FLASH_AF_TIMEOUT_COUNT < m_timeoutCount) {
            if (FLASH_AF_TIMEOUT_COUNT < m_timeoutCount) {
                CLOGD("auto focus timeoutCount %d",
                        m_timeoutCount);
            }
            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else {
            if (shot_ext->shot.ctl.aa.afMode == AA_AFMODE_OFF) {
                m_flashStatus = FLASH_STATUS_PRE_DONE;
                m_timeoutCount = 0;
            } else {
                m_timeoutCount++;
            }
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
        if (shot_ext->shot.dm.flash.vendor_flashReady == 2 ||
            FLASH_TIMEOUT_COUNT < m_timeoutCount) {
            if (shot_ext->shot.dm.flash.vendor_flashReady == 2) {
                CLOGD("flashReady == 2 frameCount %d",
                        shot_ext->shot.dm.request.frameCount);
            } else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {
                CLOGD("m_timeoutCount %d",
                        m_timeoutCount);
            }

            m_flashStatus = FLASH_STATUS_MAIN_READY;
            m_timeoutCount = 0;
        } else {
            shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
        if ((shot_ext->shot.dm.flash.vendor_flashOffReady == 2) ||
            (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) ||
            FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {

            if (shot_ext->shot.dm.flash.vendor_flashOffReady == 2) {
                CLOGD("flashOffReady %d",
                    shot_ext->shot.dm.flash.vendor_flashOffReady);
            } else if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) {
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_ShotFcount %u", m_ShotFcount);
            } else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {
                CLOGD("m_timeoutCount %d", m_timeoutCount);
            }
            CLOGD("frameCount %d" ,
                shot_ext->shot.dm.request.frameCount);

            m_flashStatus = FLASH_STATUS_MAIN_DONE;
            m_timeoutCount = 0;
            m_mainWaitCount = 0;

            m_waitingCount--;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
        /* 1 frame is used translate status MAIN_ON to MAIN_WAIT */
        if (m_mainWaitCount < FLASH_MAIN_WAIT_COUNT -1) {
            CLOGD("frameCount=%d",
                shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount++;
        } else {
            CLOGD("frameCount=%d",
                shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount = 0;
            m_waitingCount = -1;
            m_flashStatus = FLASH_STATUS_MAIN_DONE;
        }
    }

    /* [HACK]
    * - replace AE state to pass CTS in GED (RobustnessTest)
    */
    if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_FLASH_REQUIRED;
    }

    m_aeflashMode = shot_ext->shot.dm.aa.vendor_aeflashMode;

    CLOGV("(m_aeState %d)(m_flashStatus %d)",
            (int)m_aeState, (int)m_flashStatus);
    CLOGV("(decision %d flashReady %d flashOffReady %d firingStable %d)",
            (int)shot_ext->shot.dm.flash.vendor_decision,
            (int)shot_ext->shot.dm.flash.vendor_flashReady,
            (int)shot_ext->shot.dm.flash.vendor_flashOffReady,
            (int)shot_ext->shot.dm.flash.vendor_firingStable);
    CLOGV("(aeState %d)(aeflashMode %d)(dm flashMode %d)(flashState %d)(ctl flashMode %d)",
            (int)shot_ext->shot.dm.aa.aeState, (int)shot_ext->shot.dm.aa.vendor_aeflashMode,
            (int)shot_ext->shot.dm.flash.flashMode, (int)shot_ext->shot.dm.flash.flashState,
            (int)shot_ext->shot.ctl.flash.flashMode);

done:
    return 1;
}

int ExynosCameraActivityFlash::t_funcSCPBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityFlash::t_funcSCPAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityFlash::t_funcSCCBefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    return 1;
}

int ExynosCameraActivityFlash::t_funcSCCAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

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

enum ExynosCameraActivityFlash::FLASH_REQ ExynosCameraActivityFlash::getFlashReq(void)
{
    return m_flashReq;
}

bool ExynosCameraActivityFlash::setFlashStatus(enum FLASH_STATUS flashStatusVal)
{
    m_flashStatus = flashStatusVal;
    CLOGV("flashStatus=%d", (int)m_flashStatus);
    return true;
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

bool ExynosCameraActivityFlash::setFlashWhiteBalance(enum aa_awbmode wbModeVal)
{
    m_awbMode = wbModeVal;
    CLOGV("awbMode=%d", (int)m_awbMode);
    return true;
}

void ExynosCameraActivityFlash::setAeLock(bool aeLock)
{
    m_aeLock = aeLock;
    CLOGV("aeLock=%d", (int)m_aeLock);
}

void ExynosCameraActivityFlash::setAwbLock(bool awbLock)
{
    m_awbLock = awbLock;
    CLOGV("awbLock=%d", (int)m_awbLock);
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

bool ExynosCameraActivityFlash::setShouldCheckedRcount(int rcount)
{
    m_mainCaptureRcount = rcount;
    CLOGV("mainCaptureRcount=%d", m_mainCaptureRcount);

    return true;
}

bool ExynosCameraActivityFlash::waitAeDone(void)
{
    bool ret = true;

    int status = 0;
    unsigned int totalWaitingTime = 0;

    while (status == 0 &&
        totalWaitingTime <= FLASH_MAX_AEDONE_WAITING_TIME &&
        m_checkFlashWaitCancel == false) {
        if (m_flashStatus == FLASH_STATUS_PRE_ON || m_flashStep == FLASH_STEP_PRE_START) {
            if ((m_aeWaitMaxCount <= 0) || (m_flashStatus == FLASH_STATUS_PRE_AE_DONE)) {
                status = 1;
                break;
            } else {
                CLOGV("aeWaitMaxCount=%d", m_aeWaitMaxCount);
                status = 0;
            }
        } else {
            status = 1;
            break;
        }

        usleep(FLASH_WAITING_SLEEP_TIME);
        totalWaitingTime += FLASH_WAITING_SLEEP_TIME;
    }

    if (status == 0 || FLASH_MAX_AEDONE_WAITING_TIME < totalWaitingTime) {
        CLOGW("waiting too much (%d msec)", totalWaitingTime);
        ret = false;
    }

    return ret;
}

bool ExynosCameraActivityFlash::waitMainReady(void)
{
    bool ret = true;
    unsigned int totalWaitingTime = 0;
    unsigned int waitTimeoutFpsValue = 0;

    CLOGV("");

    if (getFpsValue() > 0) {
        waitTimeoutFpsValue = 30 / getFpsValue();
    }
    if (waitTimeoutFpsValue < 1)
        waitTimeoutFpsValue = 1;

    CLOGI("waitTimeoutFpsValue(%d) , getFpsValue(%d)",
        waitTimeoutFpsValue, getFpsValue());

    while (m_flashStatus < FLASH_STATUS_MAIN_READY &&
        totalWaitingTime <= FLASH_MAX_PRE_DONE_WAITING_TIME * waitTimeoutFpsValue &&
        m_checkFlashWaitCancel == false && m_isPreFlash == true) {
        CLOGV("(%d)(%d)(%d)", m_flashStatus, totalWaitingTime, m_checkFlashWaitCancel);

        usleep(FLASH_WAITING_SLEEP_TIME);
        totalWaitingTime += FLASH_WAITING_SLEEP_TIME;
    }

    if (m_flashStatus < FLASH_STATUS_MAIN_READY && FLASH_MAX_PRE_DONE_WAITING_TIME * waitTimeoutFpsValue < totalWaitingTime) {
        CLOGW("waiting too much (%d msec)", totalWaitingTime);
        m_flashStatus = FLASH_STATUS_MAIN_READY;
        ret = false;
    }

    return ret;
}

int ExynosCameraActivityFlash::checkMainCaptureRcount(int rcount)
{
    if (rcount == m_mainCaptureRcount)
        return 0;
    else if (rcount < m_mainCaptureRcount)
        return 1;
    else
        return -1;
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

int ExynosCameraActivityFlash::getWaitingCount()
{
    return m_waitingCount;
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

bool ExynosCameraActivityFlash::setRecordingHint(bool hint)
{
    m_isRecording = hint;
    if ((m_isRecording == true) || (m_flashReq == FLASH_REQ_TORCH))
        m_isNeedCaptureFlash = false;
    else
        m_isNeedCaptureFlash = true;

    return true;
}

bool ExynosCameraActivityFlash::checkPreFlash()
{
    return m_isPreFlash;
}

bool ExynosCameraActivityFlash::checkFlashOff()
{
    return m_isFlashOff;
}

bool ExynosCameraActivityFlash::updateAeState()
{
    m_aePreState = m_aeState;
    m_aeState = m_curAeState;

    if (m_aePreState != m_aeState) {
        if ((m_aeState == AE_STATE_CONVERGED || m_aeState == AE_STATE_LOCKED_CONVERGED) &&
            m_flashReq != FLASH_REQ_ON)
            m_isNeedFlash = false;
        else if (m_aeState == AE_STATE_FLASH_REQUIRED || m_aeState == AE_STATE_LOCKED_FLASH_REQUIRED)
            m_isNeedFlash = true;
    }

    CLOGD(" aeState %d", (int)m_aeState);

    return true;
}

void ExynosCameraActivityFlash::setFlashWaitCancel(bool cancel)
{
    m_checkFlashWaitCancel = cancel;
}

bool ExynosCameraActivityFlash::getFlashWaitCancel(void)
{
    return m_checkFlashWaitCancel;
}

status_t ExynosCameraActivityFlash::setNeedFlashOffDelay(bool delay)
{
    m_isNeedFlashOffDelay = delay;
    return NO_ERROR;
}

bool ExynosCameraActivityFlash::getNeedFlashOffDelay(void)
{
    return m_isNeedFlashOffDelay;
}

void ExynosCameraActivityFlash::setFpsValue(int fpsValue)
{
    m_fpsValue = fpsValue;
}

int ExynosCameraActivityFlash::getFpsValue()
{
    return m_fpsValue;
}

void ExynosCameraActivityFlash::notifyAfResult(void)
{
    if (m_flashStatus == FLASH_STATUS_PRE_AF) {
        setFlashStep(FLASH_STEP_PRE_DONE);
        CLOGD(" AF DONE (for flash)");
    }
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

void ExynosCameraActivityFlash::setMainFlashFiring(bool isMainFlashFiring)
{
    m_isMainFlashFiring = isMainFlashFiring;
}

void ExynosCameraActivityFlash::setManualExposureTime(uint64_t exposureTime)
{
    CLOGD("exposureTime(%lld)", exposureTime);
    m_manualExposureTime = exposureTime;
}

}/* namespace android */

