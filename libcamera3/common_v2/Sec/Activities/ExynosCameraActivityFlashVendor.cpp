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
#define LOG_TAG "ExynosCameraActivityFlashSec"
#include <log/log.h>


#include "ExynosCameraActivityFlash.h"

namespace android {

int ExynosCameraActivityFlash::t_func3ABefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityFlash::t_func3ABeforeHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

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
            if (m_prevFlashMode != shot_ext->shot.ctl.flash.flashMode) {
                m_ShotFcount = 0;
                m_waitingFlashStableCount = FLASH_MAIN_TIMEOUT_COUNT;
                CLOGV("CAM2_FLASH_MODE_SINGLE: m_waitingFlashStableCount set %d", FLASH_MAIN_TIMEOUT_COUNT);
            }
            break;
        default:
            break;
        }
        m_prevFlashMode = shot_ext->shot.ctl.flash.flashMode;
    } else {
        m_prevFlashMode = CAM2_FLASH_MODE_OFF;
        m_waitingFlashStableCount = 0;
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
        m_isCapture = false;

        goto done;
    }

    if (m_flashReq == FLASH_REQ_OFF) {
        if (m_waitingFlashStableCount == 0) {
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
        } else {
            CLOGV("  Waiting Flash Stable for SINGLE");

            m_isNeedFlash = true;

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_waitingCount = -1;
        }
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

        m_waitingFlashStableCount = 0;
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

                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            }
#ifdef SAMSUNG_FRONT_LCD_FLASH
            else if (m_flashStep >= FLASH_STEP_PRE_LCD_ON && m_flashStep <= FLASH_STEP_LCD_OFF) {
                setAeFlashModeForLcdFlashHAL3(shot_ext, m_flashStep);
            }
#endif /* SAMSUNG_FRONT_LCD_FLASH */
        } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
            CLOGV(" Flash status is PRE ON");

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_ON;
            m_aeWaitMaxCount--;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
            CLOGV(" Flash status is PRE AE DONE");

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
            CLOGV(" Flash status is PRE AF");

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

            m_flashStep = FLASH_STEP_OFF;
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

            m_checkMainCaptureRcount = false;
            m_checkMainCaptureFcount = false;
            m_waitingCount = -1;

            goto done;
        } else if (m_aeState == AE_STATE_FLASH_REQUIRED
                   || m_aeState == AE_STATE_LOCKED_FLASH_REQUIRED
                   || m_aeState == AE_STATE_SEARCHING_FLASH_REQUIRED) {
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
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                    m_flashStatus = FLASH_STATUS_PRE_ON;
                    m_aeWaitMaxCount--;
                }
#ifdef SAMSUNG_FRONT_LCD_FLASH
                else if (m_flashStep >= FLASH_STEP_PRE_LCD_ON && m_flashStep <= FLASH_STEP_LCD_OFF) {
                    setAeFlashModeForLcdFlashHAL3(shot_ext, m_flashStep);
                }
#endif /* SAMSUNG_FRONT_LCD_FLASH */
            } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
                CLOGV(" Flash status is PRE ON");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
                CLOGV(" Flash status is PRE AE DONE");

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
                CLOGV(" Flash status is PRE AF");

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

                m_flashStep = FLASH_STEP_OFF;
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
    CLOGV("aeflashMode(%d) ctl flashMode(%d)",
        (int)shot_ext->shot.ctl.aa.vendor_aeflashMode,
        (int)shot_ext->shot.ctl.flash.flashMode);

    return 1;
}

int ExynosCameraActivityFlash::t_func3AAfter(__unused void *args)
{
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
        shot_ext->shot.dm.aa.aeState = AE_STATE_FLASH_REQUIRED;
    }

    if (shot_ext->shot.dm.aa.aeState != AE_STATE_LOCKED
        && shot_ext->shot.dm.aa.aePrecaptureTrigger == AA_AE_PRECAPTURE_TRIGGER_START) {
        CLOGV("shot_ext->shot.dm.aa.aeState(%d) -> AE_STATE_PRECAPTURE",
                shot_ext->shot.dm.aa.aeState);
        shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;
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
        } else if (m_waitingFlashStableCount > 0) {
            m_waitingFlashStableCount--;

            shot_ext->shot.dm.flash.flashMode = CAM2_FLASH_MODE_SINGLE;
            shot_ext->shot.dm.flash.flashState = FLASH_STATE_FIRED;

            if (m_waitingFlashStableCount == 0) {
                CLOGV("Don't receive firingStable.");
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) {
                CLOGV(" frameCount %d is flashed frame", shot_ext->shot.dm.request.frameCount);
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
                m_waitingFlashStableCount = 0;
            }
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
    }
#ifdef SAMSUNG_FRONT_LCD_FLASH
    else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
        if (m_flashStep == FLASH_STEP_PRE_LCD_ON) {
            if (shot_ext->shot.dm.aa.aePrecaptureTrigger == AA_AE_PRECAPTURE_TRIGGER_START
                && shot_ext->shot.dm.aa.aeState == AE_STATE_LOCKED) {
                CLOGV("flashStep(%d) shot_ext->shot.dm.aa.aeState(%d) -> AE_STATE_PRECAPTURE",
                                m_flashStep,
                                shot_ext->shot.dm.aa.aeState);
                shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;
            }
        } else if (m_flashStep == FLASH_STEP_LCD_ON) {
            CLOGV("Flash step is LCD_ON(firingStable=%d)", shot_ext->shot.dm.flash.vendor_firingStable);
            if (!m_ShotFcount && shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) {
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
                m_flashStep = FLASH_STEP_LCD_ON_WAIT;
            }
        }
    }
#endif
    else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
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

        if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED
            || shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_UNFOCUSED
            || shot_ext->shot.dm.aa.afState == AA_AFSTATE_INACTIVE) {
            CLOGD("FLASH_STATUS_PRE_DONE afMode(%d) afState(%d)",
                shot_ext->shot.dm.aa.afMode, shot_ext->shot.dm.aa.afState);
            m_flashStatus = FLASH_STATUS_PRE_DONE;
        } else if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED
                    || shot_ext->shot.dm.aa.afState == AA_AFSTATE_NOT_FOCUSED_LOCKED) {
            CLOGD("FLASH_STATUS_PRE_DONE afTrigger(%d) af state(%d)",
                      shot_ext->shot.dm.aa.afTrigger, shot_ext->shot.dm.aa.afState);
            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else if (shot_ext->shot.ctl.aa.captureIntent == AA_CAPTURE_INTENT_STILL_CAPTURE
                    && (shot_ext->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_SCAN
                     || shot_ext->shot.dm.aa.afState == AA_AFSTATE_ACTIVE_SCAN)) {
            CLOGE("Error AF State. Force to FLASH_STATUS_PRE_DONE. afTrigger(%d) af state(%d), captureIntent(%d)",
                      shot_ext->shot.dm.aa.afTrigger,
                      shot_ext->shot.dm.aa.afState,
                      shot_ext->shot.ctl.aa.captureIntent);
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

            if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF
                && shot_ext->shot.dm.aa.aeState == AE_STATE_INACTIVE) {
                shot_ext->shot.dm.aa.aeState = AE_STATE_FLASH_REQUIRED;
            }
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

bool ExynosCameraActivityFlash::setFlashStep(enum FLASH_STEP flashStepVal)
{
    m_flashStep = flashStepVal;

    /* trigger events */
    switch (flashStepVal) {
    case FLASH_STEP_OFF:
        m_waitingCount = -1;
        m_checkMainCaptureFcount = false;
        m_checkFlashStepCancel = false;
        m_flashStatus = FLASH_STATUS_OFF;
        m_isCapture = false;
        break;
    case FLASH_STEP_PRE_START:
        m_aeWaitMaxCount = 25;
        m_isFlashOff = false;
        if ((m_flashStatus == FLASH_STATUS_PRE_DONE || m_flashStatus == FLASH_STATUS_MAIN_READY) &&
            (m_flashTrigger == FLASH_TRIGGER_LONG_BUTTON || m_flashTrigger == FLASH_TRIGGER_TOUCH_DISPLAY))
            m_flashStatus = FLASH_STATUS_OFF;
        break;
    case FLASH_STEP_PRE_DONE:
        break;
    case FLASH_STEP_MAIN_START:
        setShouldCheckedFcount(m_currentIspInputFcount + CAPTURE_SKIP_COUNT);

        m_waitingCount = 15;
        m_timeoutCount = 0;
        m_checkMainCaptureFcount = false;
        break;
    case FLASH_STEP_MAIN_DONE:
        m_waitingCount = -1;
        m_checkMainCaptureFcount = false;
        break;
    case FLASH_STEP_CANCEL:
        m_checkFlashStepCancel = true;
        m_isNeedCaptureFlash = true;
        break;
    case FLASH_STEP_END:
        break;
#ifdef SAMSUNG_FRONT_LCD_FLASH
    case FLASH_STEP_LCD_ON:
        m_waitingCount = 15;
        m_timeoutCount = 0;
        break;
#endif
    default:
        break;
    }

    CLOGD("flashStepVal=%d", (int)flashStepVal);

    if (flashStepVal != FLASH_STEP_OFF)
        m_flashStepErrorCount = 0;

    return true;
}

#ifdef SAMSUNG_FRONT_LCD_FLASH
void ExynosCameraActivityFlash::setAeFlashModeForLcdFlashHAL3(camera2_shot_ext *shot_ext, int flashStep)
{
    if (flashStep == FLASH_STEP_PRE_LCD_ON) {
        CLOGV("Flash step is PRE_LCD_ON");
        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_LCD;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;
        m_mainWaitCount = 0;
    } else if (flashStep == FLASH_STEP_LCD_ON) {
        CLOGV("Flash step is LCD_ON");
        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_LCD;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;
        m_mainWaitCount = 0;
    } else if (flashStep == FLASH_STEP_LCD_ON_WAIT) {
        CLOGV("Flash step is LCD_ON_WAIT");

        if (FLASH_MAIN_WAIT_COUNT < m_mainWaitCount) {
            m_flashStep = FLASH_STEP_LCD_OFF;
        } else {
            m_mainWaitCount++;
        }
    } else if (flashStep == FLASH_STEP_LCD_OFF) {
        CLOGV("Flash step is LCD_OFF");
        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;
        m_mainWaitCount = 0;
        m_flashStep = FLASH_STEP_OFF;

        resetShotFcount();
    }
}

#endif

}/* namespace android */
