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

#include "ExynosCameraUtils.h"
#include "ExynosCameraActivityFlash.h"

namespace android {

class ExynosCamera;

ExynosCameraActivityFlash::ExynosCameraActivityFlash()
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
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    if (m_checkMainCaptureFcount == true) {
        /* Update m_waitingCount */
        m_waitingCount = checkMainCaptureFcount(shot_ext->shot.dm.request.frameCount);
        ALOGV("INFO(%s[%d]):m_waitingCount=0x%x", __FUNCTION__, __LINE__, m_waitingCount);
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

int ExynosCameraActivityFlash::t_func3ABefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    m_currentIspInputFcount = shot_ext->shot.dm.request.frameCount;

    ALOGV("INFO(%s[%d]):m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
        __FUNCTION__, __LINE__, (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

    if (m_flashPreStatus != m_flashStatus) {
        ALOGD("DEBUG(%s[%d]):m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
            __FUNCTION__, __LINE__,
            (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

        m_flashPreStatus = m_flashStatus;
    }

    if (m_aePreState != m_aeState) {
        ALOGV("INFO(%s[%d]):m_aeState=%d", __FUNCTION__, __LINE__, (int)m_aeState);

        m_aePreState = m_aeState;
    }

    if (m_flashStep == FLASH_STEP_CANCEL && m_checkFlashStepCancel == true) {
        ALOGV("DEBUG(%s[%d]): Flash step is CANCEL", __FUNCTION__, __LINE__);

        if (m_isMainFlashFiring == false) {
            m_isNeedFlash = false;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CANCEL;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CANCEL;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;
            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            m_waitingCount = -1;
            m_flashStepErrorCount = -1;

            m_checkMainCaptureRcount = false;
            m_checkMainCaptureFcount = false;
            m_checkFlashStepCancel = false;
            /* m_checkFlashWaitCancel = false; */
            m_isCapture = false;

            goto done;
        } else {
            ALOGW("WARN(%s[%d]) When Main Flash started, Skip Flash Cancel", __FUNCTION__, __LINE__);
        }
    }

    if (m_flashReq == FLASH_REQ_OFF) {
        ALOGV("DEBUG(%s[%d]): Flash request is OFF", __FUNCTION__, __LINE__);
        m_isNeedFlash = false;
        if (m_aeflashMode == AA_FLASHMODE_ON_ALWAYS)
            m_isNeedFlashOffDelay = true;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
        shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;
        m_flashStepErrorCount = -1;
        m_flashStep = FLASH_STEP_OFF;

        m_checkMainCaptureRcount = false;
        m_checkMainCaptureFcount = false;

        goto done;
    } else if (m_flashReq == FLASH_REQ_TORCH) {
        ALOGV("DEBUG(%s[%d]): Flash request is TORCH", __FUNCTION__, __LINE__);
        m_isNeedFlash = true;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON_ALWAYS;
#else
        shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON_ALWAYS;
#endif
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;

        goto done;
    } else if (m_flashReq == FLASH_REQ_ON) {
        ALOGV("DEBUG(%s[%d]): Flash request is ON", __FUNCTION__, __LINE__);
        m_isNeedFlash = true;

        if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            m_flashStatus = FLASH_STATUS_PRE_READY;
        } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE READY", __FUNCTION__, __LINE__);
            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            if (m_flashStep == FLASH_STEP_PRE_START) {
                ALOGV("DEBUG(%s[%d]): Flash step is PRE START", __FUNCTION__, __LINE__);

#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_START;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            }
        } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE ON", __FUNCTION__, __LINE__);

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
            shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

            m_flashStatus = FLASH_STATUS_PRE_ON;
            m_aeWaitMaxCount--;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE AE DONE", __FUNCTION__, __LINE__);

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
            shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_aeWaitMaxCount = 0;
            /* AE AWB LOCK */
        } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE AF", __FUNCTION__, __LINE__);

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
            shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

            m_flashStatus = FLASH_STATUS_PRE_AF;
            m_aeWaitMaxCount = 0;
            /*
        } else if (m_flashStatus == FLASH_STATUS_PRE_AF_DONE) {
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_AUTO;
            shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;

            m_waitingCount = -1;
            m_aeWaitMaxCount = 0;
            */
        } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE DONE", __FUNCTION__, __LINE__);

            if (m_aeLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
#endif
            else
                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            if (m_awbLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif
            else
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_AUTO;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_waitingCount = -1;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
            ALOGV("DEBUG(%s[%d]): Flash status is MAIN READY", __FUNCTION__, __LINE__);
            if (m_manualExposureTime != 0) {
                setMetaCtlExposureTime(shot_ext, m_manualExposureTime);
            }

            if (m_aeLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
#endif
            else
                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            if (m_awbLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif
            else
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            if (m_flashStep == FLASH_STEP_MAIN_START) {
                ALOGD("DEBUG(%s[%d]): Flash step is MAIN START (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                /* during capture, unlock AE and AWB */
                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_ON;

                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            }
        } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
            ALOGD("DEBUG(%s[%d]): Flash status is MAIN ON (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            m_flashStatus = FLASH_STATUS_MAIN_ON;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
            ALOGD("DEBUG(%s[%d]): Flash status is MAIN WAIT (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            m_flashStatus = FLASH_STATUS_MAIN_WAIT;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
            ALOGD("DEBUG(%s[%d]): Flash status is MAIN DONE (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.aeMode = m_aeMode;
            shot_ext->shot.ctl.aa.awbMode = m_awbMode;

            m_flashStatus = FLASH_STATUS_OFF;
            m_waitingCount = -1;

            m_aeWaitMaxCount = 0;
        }
    } else if (m_flashReq == FLASH_REQ_AUTO) {
        ALOGV("DEBUG(%s[%d]): Flash request is AUTO", __FUNCTION__, __LINE__);

        if (m_aeState == AE_STATE_INACTIVE) {
            ALOGV("DEBUG(%s[%d]): AE state is INACTIVE", __FUNCTION__, __LINE__);
            m_isNeedFlash = false;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
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
            ALOGV("DEBUG(%s[%d]): AE state is CONVERGED", __FUNCTION__, __LINE__);
            m_isNeedFlash = false;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
            shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
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
            ALOGV("DEBUG(%s[%d]): AE state is FLASH REQUIRED", __FUNCTION__, __LINE__);
            m_isNeedFlash = true;

            if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                m_flashStatus = FLASH_STATUS_PRE_READY;
            } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE READY", __FUNCTION__, __LINE__);
                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                if (m_flashStep == FLASH_STEP_PRE_START) {
                    ALOGV("DEBUG(%s[%d]): Flash step is PRE START", __FUNCTION__, __LINE__);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
#else
                    shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_START;
#endif
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;
                    shot_ext->shot.ctl.aa.aeMode = m_aeMode;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                    shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

                    m_flashStatus = FLASH_STATUS_PRE_ON;
                    m_aeWaitMaxCount--;
                }
            } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE ON", __FUNCTION__, __LINE__);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE AE DONE", __FUNCTION__, __LINE__);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

                m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
                m_aeWaitMaxCount = 0;
                /* AE AWB LOCK */
            } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE AF", __FUNCTION__, __LINE__);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_ON;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif

                m_flashStatus = FLASH_STATUS_PRE_AF;
                m_aeWaitMaxCount = 0;
                /*
            } else if (m_flashStatus == FLASH_STATUS_PRE_AF_DONE) {
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_AUTO;
                shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
                */
            } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE DONE", __FUNCTION__, __LINE__);

                if (m_aeLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
#else
                    shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
#endif
                else
                    shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                if (m_awbLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                    shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif
                else
                    shot_ext->shot.ctl.aa.awbMode = m_awbMode;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_AUTO;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
                ALOGV("DEBUG(%s[%d]): Flash status is MAIN READY", __FUNCTION__, __LINE__);
                if (m_manualExposureTime != 0) {
                    setMetaCtlExposureTime(shot_ext, m_manualExposureTime);
                }

                if (m_aeLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
#else
                    shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_LOCKED;
#endif
                else
                    shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                if (m_awbLock && m_flashTrigger != FLASH_TRIGGER_TOUCH_DISPLAY)
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
#else
                    shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_LOCKED;
#endif
                else
                    shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                if (m_flashStep == FLASH_STEP_MAIN_START) {
                    ALOGD("DEBUG(%s[%d]): Flash step is MAIN START (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
                    /* during capture, unlock AE and AWB */
                    shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                    shot_ext->shot.ctl.aa.awbMode = m_awbMode;

#if defined(USE_HAL3_2_METADATA_INTERFACE)
                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
                    shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;

                    m_flashStatus = FLASH_STATUS_MAIN_ON;

                    m_waitingCount--;
                    m_aeWaitMaxCount = 0;
                }
            } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
                ALOGD("DEBUG(%s[%d]): Flash status is MAIN ON (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                m_flashStatus = FLASH_STATUS_MAIN_ON;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
                ALOGD("DEBUG(%s[%d]): Flash status is MAIN WAIT (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_CAPTURE;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                m_flashStatus = FLASH_STATUS_MAIN_WAIT;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
                ALOGD("DEBUG(%s[%d]): Flash status is MAIN DONE (fcount %d)", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
#if defined(USE_HAL3_2_METADATA_INTERFACE)
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
#else
                shot_ext->shot.ctl.aa.aeflashMode = AA_FLASHMODE_OFF;
#endif
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.aeMode = m_aeMode;
                shot_ext->shot.ctl.aa.awbMode = m_awbMode;

                m_flashStatus = FLASH_STATUS_OFF;
                m_waitingCount = -1;

                m_aeWaitMaxCount = 0;
            }
        }
    }

    if (0 < m_flashStepErrorCount)
        m_flashStepErrorCount++;
#if defined(USE_HAL3_2_METADATA_INTERFACE)
    ALOGV("INFO(%s[%d]):aeflashMode=%d", __FUNCTION__, __LINE__, (int)shot_ext->shot.ctl.aa.vendor_aeflashMode);
#else
    ALOGV("INFO(%s[%d]):aeflashMode=%d", __FUNCTION__, __LINE__, (int)shot_ext->shot.ctl.aa.aeflashMode);
#endif

done:
    return 1;
}

int ExynosCameraActivityFlash::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    if (m_isCapture == false)
        m_aeState = shot_ext->shot.dm.aa.aeState;
    m_curAeState = shot_ext->shot.dm.aa.aeState;

    /* Convert aeState for Locked */
    if (shot_ext->shot.dm.aa.aeState == AE_STATE_LOCKED_CONVERGED ||
        shot_ext->shot.dm.aa.aeState == AE_STATE_LOCKED_FLASH_REQUIRED) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_LOCKED;
    }

    if (m_flashStep == FLASH_STEP_CANCEL &&
        m_checkFlashStepCancel == false) {
        m_flashStep = FLASH_STEP_OFF;
        m_flashStatus = FLASH_STATUS_OFF;

        goto done;
    }

    if (m_flashReq == FLASH_REQ_OFF) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        if (shot_ext->shot.dm.flash.vendor_flashReady == 3)
#else
        if (shot_ext->shot.dm.flash.flashReady == 3)
#endif
        {
            ALOGV("DEBUG(%s[%d]): flashReady = 3 frameCount %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            m_isFlashOff = true;
        }
    }

    if (m_flashStatus == FLASH_STATUS_PRE_CHECK) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        if (shot_ext->shot.dm.flash.vendor_decision == 2 ||
#else
        if (shot_ext->shot.dm.flash.decision == 2 ||
#endif
            FLASH_TIMEOUT_COUNT < m_timeoutCount) {
            m_flashStatus = FLASH_STATUS_PRE_READY;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        if (shot_ext->shot.dm.flash.vendor_flashReady == 1 ||
#else
        if (shot_ext->shot.dm.flash.flashReady == 1 ||
#endif
            FLASH_AE_TIMEOUT_COUNT < m_timeoutCount) {
            if (FLASH_AE_TIMEOUT_COUNT < m_timeoutCount)
                ALOGD("DEBUG(%s[%d]):auto exposure timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
        m_flashStatus = FLASH_STATUS_PRE_AF;
    } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
        if (m_flashStep == FLASH_STEP_PRE_DONE ||
            FLASH_AF_TIMEOUT_COUNT < m_timeoutCount) {
            if (FLASH_AF_TIMEOUT_COUNT < m_timeoutCount)
                ALOGD("DEBUG(%s[%d]):auto focus timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    /*
    } else if (m_flashStatus == FLASH_STATUS_PRE_AF_DONE) {
        if (shot_ext->shot.dm.flash.flashOffReady == 1 ||
            FLASH_TIMEOUT_COUNT < m_timeoutCount) {
            if (shot_ext->shot.dm.flash.flashOffReady == 1)
                ALOGD("[%s] (%d) flashOffReady == 1 frameCount %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
        */
    } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        if (shot_ext->shot.dm.flash.vendor_flashReady == 2 ||
#else
        if (shot_ext->shot.dm.flash.flashReady == 2 ||
#endif
            FLASH_TIMEOUT_COUNT < m_timeoutCount) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
            if (shot_ext->shot.dm.flash.vendor_flashReady == 2)
#else
            if (shot_ext->shot.dm.flash.flashReady == 2)
#endif
            {
                ALOGD("DEBUG(%s[%d]):flashReady == 2 frameCount %d",
                    __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            } else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {
                ALOGD("DEBUG(%s[%d]):m_timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
            }

            m_flashStatus = FLASH_STATUS_MAIN_READY;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
#if 0
        if (m_flashTrigger == FLASH_TRIGGER_TOUCH_DISPLAY) {
            if (FLASH_TIMEOUT_COUNT < m_timeoutCount) {
                ALOGD("DEBUG(%s[%d]):m_timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);

                m_flashStep = FLASH_STEP_OFF;
                m_flashStatus = FLASH_STATUS_OFF;
                /* m_flashTrigger = FLASH_TRIGGER_OFF; */
                m_isCapture = false;
                m_isPreFlash = false;

                m_waitingCount = -1;
                m_checkMainCaptureFcount = false;
                m_timeoutCount = 0;
            } else {
                m_timeoutCount++;
            }
        }
#endif
    } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
#if defined(USE_HAL3_2_METADATA_INTERFACE)
        if ((shot_ext->shot.dm.flash.vendor_flashOffReady == 2) ||
            (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) ||
#else
        if ((shot_ext->shot.dm.flash.flashOffReady == 2) ||
            (shot_ext->shot.dm.flash.firingStable == CAPTURE_STATE_FLASH) ||
#endif
            FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {

#if defined(USE_HAL3_2_METADATA_INTERFACE)
            if (shot_ext->shot.dm.flash.vendor_flashOffReady == 2) {
                ALOGD("DEBUG(%s[%d]):flashOffReady %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.flash.vendor_flashOffReady);
            } else if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH)
#else
            if (shot_ext->shot.dm.flash.flashOffReady == 2) {
                ALOGD("DEBUG(%s[%d]):flashOffReady %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.flash.flashOffReady);
            } else if (shot_ext->shot.dm.flash.firingStable == CAPTURE_STATE_FLASH)
#endif
            {
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
                ALOGD("DEBUG(%s[%d]):m_ShotFcount %u", __FUNCTION__, __LINE__, m_ShotFcount);
            } else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {
                ALOGD("DEBUG(%s[%d]):m_timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
            }
            ALOGD("DEBUG(%s[%d]):frameCount %d" ,__FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

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
            ALOGD("DEBUG(%s[%d]):frameCount=%d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount ++;
        } else {
            ALOGD("DEBUG(%s[%d]):frameCount=%d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount = 0;
            m_waitingCount = -1;
            m_flashStatus = FLASH_STATUS_MAIN_DONE;
        }
    }

#if defined(USE_HAL3_2_METADATA_INTERFACE)
    m_aeflashMode = shot_ext->shot.dm.aa.vendor_aeflashMode;

    ALOGV("INFO(%s[%d]):(m_aeState %d)(m_flashStatus %d)", __FUNCTION__, __LINE__, (int)m_aeState, (int)m_flashStatus);
    ALOGV("INFO(%s[%d]):(decision %d flashReady %d flashOffReady %d firingStable %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.flash.vendor_decision,
            (int)shot_ext->shot.dm.flash.vendor_flashReady,
            (int)shot_ext->shot.dm.flash.vendor_flashOffReady,
            (int)shot_ext->shot.dm.flash.vendor_firingStable);
    ALOGV("INFO(%s[%d]):(aeState %d)(aeflashMode %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.aa.aeState, (int)shot_ext->shot.dm.aa.vendor_aeflashMode);
#else
    m_aeflashMode = shot_ext->shot.dm.aa.aeflashMode;

    ALOGV("INFO(%s[%d]):(m_aeState %d)(m_flashStatus %d)", __FUNCTION__, __LINE__, (int)m_aeState, (int)m_flashStatus);
    ALOGV("INFO(%s[%d]):(decision %d flashReady %d flashOffReady %d firingStable %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.flash.decision,
            (int)shot_ext->shot.dm.flash.flashReady,
            (int)shot_ext->shot.dm.flash.flashOffReady,
            (int)shot_ext->shot.dm.flash.firingStable);
    ALOGV("INFO(%s[%d]):(aeState %d)(aeflashMode %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.aa.aeState, (int)shot_ext->shot.dm.aa.aeflashMode);
#endif

done:
    return 1;
}

#ifdef USE_HAL3_2_METADATA_INTERFACE
int ExynosCameraActivityFlash::t_func3ABeforeHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
        return false;
    }

    m_currentIspInputFcount = shot_ext->shot.dm.request.frameCount;

    ALOGV("INFO(%s[%d]):m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
            __FUNCTION__, __LINE__, (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

    if (m_flashPreStatus != m_flashStatus) {
        ALOGD("DEBUG(%s[%d]):m_flashReq=%d, m_flashStatus=%d, m_flashStep=%d",
                __FUNCTION__, __LINE__,
                (int)m_flashReq, (int)m_flashStatus, (int)m_flashStep);

        m_flashPreStatus = m_flashStatus;
    }

    if (m_aePreState != m_aeState) {
        ALOGV("INFO(%s[%d]):m_aeState=%d", __FUNCTION__, __LINE__, (int)m_aeState);
        m_aePreState = m_aeState;
    }

    if (m_flashStep == FLASH_STEP_CANCEL && m_checkFlashStepCancel == true) {
        ALOGV("DEBUG(%s[%d]): Flash step is CANCEL", __FUNCTION__, __LINE__);
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
        ALOGV("DEBUG(%s[%d]): Flash request is OFF", __FUNCTION__, __LINE__);
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
        ALOGV("DEBUG(%s[%d]): Flash request is SINGLE", __FUNCTION__, __LINE__);
        m_isNeedFlash = true;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON_ALWAYS;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;

        goto done;
    } else if (m_flashReq == FLASH_REQ_TORCH) {
        ALOGV("DEBUG(%s[%d]): Flash request is TORCH", __FUNCTION__, __LINE__);
        m_isNeedFlash = true;

        shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON_ALWAYS;
        shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
        shot_ext->shot.ctl.flash.firingTime = 0;
        shot_ext->shot.ctl.flash.firingPower = 0;

        m_waitingCount = -1;

        goto done;
    } else if (m_flashReq == FLASH_REQ_ON) {
        ALOGV("DEBUG(%s[%d]): Flash request is ON", __FUNCTION__, __LINE__);
        m_isNeedFlash = true;

        if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_PRE_READY;
        } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE READY", __FUNCTION__, __LINE__);

            if (m_flashStep == FLASH_STEP_PRE_START) {
                ALOGV("DEBUG(%s[%d]): Flash step is PRE START", __FUNCTION__, __LINE__);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            }
        } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE ON", __FUNCTION__, __LINE__);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_ON;
            m_aeWaitMaxCount--;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE AE DONE", __FUNCTION__, __LINE__);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
            ALOGV("DEBUG(%s[%d]): Flash status is PRE AF", __FUNCTION__, __LINE__);

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
            ALOGV("DEBUG(%s[%d]): Flash status is PRE DONE", __FUNCTION__, __LINE__);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_waitingCount = -1;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
            ALOGV("DEBUG(%s[%d]): Flash status is MAIN READY", __FUNCTION__, __LINE__);
            m_ShotFcount = 0;

            if (m_flashStep == FLASH_STEP_MAIN_START) {
                ALOGD("DEBUG(%s[%d]): Flash step is MAIN START (fcount %d)",
                        __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_ON;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            }
        } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
            ALOGV("DEBUG(%s[%d]): Flash status is MAIN ON (fcount %d)",
                    __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_MAIN_ON;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
            ALOGV("DEBUG(%s[%d]): Flash status is MAIN WAIT (fcount %d)",
                    __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

            shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
            shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
            shot_ext->shot.ctl.flash.firingTime = 0;
            shot_ext->shot.ctl.flash.firingPower = 0;

            m_flashStatus = FLASH_STATUS_MAIN_WAIT;
            m_waitingCount--;
            m_aeWaitMaxCount = 0;
        } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
            ALOGV("DEBUG(%s[%d]): Flash status is MAIN DONE (fcount %d)",
                    __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

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
        ALOGV("DEBUG(%s[%d]): Flash request is AUTO", __FUNCTION__, __LINE__);

        if (m_aeState == AE_STATE_INACTIVE) {
            ALOGV("DEBUG(%s[%d]): AE state is INACTIVE", __FUNCTION__, __LINE__);
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
            ALOGV("DEBUG(%s[%d]): AE state is CONVERGED", __FUNCTION__, __LINE__);
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
            ALOGV("DEBUG(%s[%d]): AE state is FLASH REQUIRED", __FUNCTION__, __LINE__);
            m_isNeedFlash = true;

            if (m_flashStatus == FLASH_STATUS_OFF || m_flashStatus == FLASH_STATUS_PRE_CHECK) {
                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_PRE_READY;
            } else if (m_flashStatus == FLASH_STATUS_PRE_READY) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE READY", __FUNCTION__, __LINE__);

                if (m_flashStep == FLASH_STEP_PRE_START) {
                    ALOGV("DEBUG(%s[%d]): Flash step is PRE START", __FUNCTION__, __LINE__);

                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_START;
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;

                    shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                    shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                    m_flashStatus = FLASH_STATUS_PRE_ON;
                    m_aeWaitMaxCount--;
                }
            } else if (m_flashStatus == FLASH_STATUS_PRE_ON) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE ON", __FUNCTION__, __LINE__);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_flashStatus = FLASH_STATUS_PRE_ON;
                m_aeWaitMaxCount--;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AE_DONE) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE AE DONE", __FUNCTION__, __LINE__);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                shot_ext->shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
                shot_ext->shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
                ALOGV("DEBUG(%s[%d]): Flash status is PRE AF", __FUNCTION__, __LINE__);

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
                ALOGV("DEBUG(%s[%d]): Flash status is PRE DONE", __FUNCTION__, __LINE__);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_waitingCount = -1;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_READY) {
                ALOGV("DEBUG(%s[%d]): Flash status is MAIN READY", __FUNCTION__, __LINE__);
                m_ShotFcount = 0;

                if (m_flashStep == FLASH_STEP_MAIN_START) {
                    ALOGV("DEBUG(%s[%d]): Flash step is MAIN START (fcount %d)",
                            __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                    shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                    shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                    shot_ext->shot.ctl.flash.firingTime = 0;
                    shot_ext->shot.ctl.flash.firingPower = 0;

                    m_flashStatus = FLASH_STATUS_MAIN_ON;
                    m_waitingCount--;
                    m_aeWaitMaxCount = 0;
                }
            } else if (m_flashStatus == FLASH_STATUS_MAIN_ON) {
                ALOGV("DEBUG(%s[%d]): Flash status is MAIN ON (fcount %d)",
                        __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_ON;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_WAIT) {
                ALOGV("DEBUG(%s[%d]): Flash status is MAIN WAIT (fcount %d)",
                        __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

                shot_ext->shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
                shot_ext->shot.ctl.flash.flashMode = CAM2_FLASH_MODE_NONE;
                shot_ext->shot.ctl.flash.firingTime = 0;
                shot_ext->shot.ctl.flash.firingPower = 0;

                m_flashStatus = FLASH_STATUS_MAIN_WAIT;
                m_waitingCount--;
                m_aeWaitMaxCount = 0;
            } else if (m_flashStatus == FLASH_STATUS_MAIN_DONE) {
                ALOGV("DEBUG(%s[%d]): Flash status is MAIN DONE (fcount %d)",
                        __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

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
    ALOGV("INFO(%s[%d]):aeflashMode=%d", __FUNCTION__, __LINE__, (int)shot_ext->shot.ctl.aa.vendor_aeflashMode);

done:
    return 1;
}

int ExynosCameraActivityFlash::t_func3AAfterHAL3(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[1]);

    if (shot_ext == NULL) {
        ALOGE("ERR(%s[%d]):shot_ext is null", __FUNCTION__, __LINE__);
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

    if (m_flashStep == FLASH_STEP_CANCEL &&
            m_checkFlashStepCancel == false) {
        m_flashStep = FLASH_STEP_OFF;
        m_flashStatus = FLASH_STATUS_OFF;

        goto done;
    }

    if (m_flashReq == FLASH_REQ_OFF) {
        if (shot_ext->shot.dm.flash.vendor_flashReady == 3) {
            ALOGV("DEBUG(%s[%d]): flashReady = 3 frameCount %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
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
                ALOGD("DEBUG(%s[%d]):auto exposure timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
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
        } else {
            m_flashStatus = FLASH_STATUS_PRE_AE_DONE;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_AF) {
        shot_ext->shot.dm.aa.aeState = AE_STATE_PRECAPTURE;

        if (shot_ext->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED ||
            shot_ext->shot.dm.aa.afState == AA_AFSTATE_NOT_FOCUSED_LOCKED ||
            FLASH_AF_TIMEOUT_COUNT < m_timeoutCount) {
            if (FLASH_AF_TIMEOUT_COUNT < m_timeoutCount)
                ALOGD("DEBUG(%s[%d]):auto focus timeoutCount %d",
                        __FUNCTION__, __LINE__, m_timeoutCount);
            m_flashStatus = FLASH_STATUS_PRE_DONE;
            m_timeoutCount = 0;
        } else {
            m_timeoutCount++;
        }
    } else if (m_flashStatus == FLASH_STATUS_PRE_DONE) {
        if (shot_ext->shot.dm.flash.vendor_flashReady == 2 ||
                FLASH_TIMEOUT_COUNT < m_timeoutCount) {
            if (shot_ext->shot.dm.flash.vendor_flashReady == 2)
                ALOGD("DEBUG(%s[%d]):flashReady == 2 frameCount %d",
                        __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount)
                ALOGD("DEBUG(%s[%d]):m_timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);

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
                ALOGD("DEBUG(%s[%d]):flashOffReady %d", __FUNCTION__, __LINE__, shot_ext->shot.dm.flash.vendor_flashOffReady);
            } else if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_FLASH) {
                m_ShotFcount = shot_ext->shot.dm.request.frameCount;
                ALOGD("DEBUG(%s[%d]):m_ShotFcount %u", __FUNCTION__, __LINE__, m_ShotFcount);
            } else if (FLASH_MAIN_TIMEOUT_COUNT < m_timeoutCount) {
                ALOGD("DEBUG(%s[%d]):m_timeoutCount %d", __FUNCTION__, __LINE__, m_timeoutCount);
            }
            ALOGD("DEBUG(%s[%d]):frameCount %d" ,__FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);

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
            ALOGD("DEBUG(%s[%d]):frameCount=%d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount++;
        } else {
            ALOGD("DEBUG(%s[%d]):frameCount=%d", __FUNCTION__, __LINE__, shot_ext->shot.dm.request.frameCount);
            m_mainWaitCount = 0;
            m_waitingCount = -1;
            m_flashStatus = FLASH_STATUS_MAIN_DONE;
        }
    }

    /* HACK : Firmware does NOT update the dynamic metadata for flash mode */
    shot_ext->shot.dm.flash.flashMode = shot_ext->shot.ctl.flash.flashMode;

    m_aeflashMode = shot_ext->shot.dm.aa.vendor_aeflashMode;

    ALOGV("INFO(%s[%d]):(m_aeState %d)(m_flashStatus %d)", __FUNCTION__, __LINE__,
            (int)m_aeState, (int)m_flashStatus);
    ALOGV("INFO(%s[%d]):(decision %d flashReady %d flashOffReady %d firingStable %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.flash.vendor_decision,
            (int)shot_ext->shot.dm.flash.vendor_flashReady,
            (int)shot_ext->shot.dm.flash.vendor_flashOffReady,
            (int)shot_ext->shot.dm.flash.vendor_firingStable);
    ALOGV("INFO(%s[%d]):(aeState %d)(aeflashMode %d)", __FUNCTION__, __LINE__,
            (int)shot_ext->shot.dm.aa.aeState, (int)shot_ext->shot.dm.aa.vendor_aeflashMode);

done:
    return 1;
}
#endif

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
        ALOGD("DEBUG(%s[%d]):flashReq=%d", __FUNCTION__, __LINE__, (int)m_flashReq);
    }

    if (m_flashReq == FLASH_REQ_ON)
        m_isNeedFlash = true;
    if (m_flashReq == FLASH_REQ_TORCH)
        m_isNeedCaptureFlash = false;
    if (m_flashReq == FLASH_REQ_OFF)
        m_isFlashOff = false;

    return true;
}

enum ExynosCameraActivityFlash::FLASH_REQ ExynosCameraActivityFlash::getFlashReq(void)
{
    return m_flashReq;
}

bool ExynosCameraActivityFlash::setFlashStatus(enum FLASH_STATUS flashStatusVal)
{
    m_flashStatus = flashStatusVal;
    ALOGV("DEBUG(%s[%d]):flashStatus=%d", __FUNCTION__, __LINE__, (int)m_flashStatus);
    return true;
}

int ExynosCameraActivityFlash::getFlashStatus()
{
    return m_aeflashMode;
}

bool ExynosCameraActivityFlash::setFlashExposure(enum aa_aemode aeModeVal)
{
    m_aeMode = aeModeVal;
    ALOGV("DEBUG(%s[%d]):aeMode=%d", __FUNCTION__, __LINE__, (int)m_aeMode);
    return true;
}

bool ExynosCameraActivityFlash::setFlashWhiteBalance(enum aa_awbmode wbModeVal)
{
    m_awbMode = wbModeVal;
    ALOGV("DEBUG(%s[%d]):awbMode=%d", __FUNCTION__, __LINE__, (int)m_awbMode);
    return true;
}

void ExynosCameraActivityFlash::setAeLock(bool aeLock)
{
    m_aeLock = aeLock;
    ALOGV("DEBUG(%s[%d]):aeLock=%d", __FUNCTION__, __LINE__, (int)m_aeLock);
}

void ExynosCameraActivityFlash::setAwbLock(bool awbLock)
{
    m_awbLock = awbLock;
    ALOGV("DEBUG(%s[%d]):awbLock=%d", __FUNCTION__, __LINE__, (int)m_awbLock);
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
        /* m_checkFlashWaitCancel = false; */
        m_flashStatus = FLASH_STATUS_OFF;
        m_isPreFlash = false;
        m_isCapture = false;
        m_isMainFlashFiring = false;
        m_manualExposureTime = 0;
        break;
    case FLASH_STEP_PRE_START:
        m_aeWaitMaxCount = 25;
        m_isPreFlash = true;
        m_isFlashOff = false;
        m_isMainFlashFiring = false;
        if ((m_flashStatus == FLASH_STATUS_PRE_DONE || m_flashStatus == FLASH_STATUS_MAIN_READY) &&
            (m_flashTrigger == FLASH_TRIGGER_LONG_BUTTON || m_flashTrigger == FLASH_TRIGGER_TOUCH_DISPLAY))
            m_flashStatus = FLASH_STATUS_OFF;
        break;
    case FLASH_STEP_PRE_DONE:
        break;
    case FLASH_STEP_MAIN_START:
        m_isMainFlashFiring = true;
        setShouldCheckedFcount(m_currentIspInputFcount + CAPTURE_SKIP_COUNT);

        m_waitingCount = 15;
        m_timeoutCount = 0;
        m_checkMainCaptureFcount = false;
        break;
    case FLASH_STEP_MAIN_DONE:
        m_waitingCount = -1;
        m_checkMainCaptureFcount = false;
        m_isPreFlash = false;
        m_isMainFlashFiring = false;
        break;
    case FLASH_STEP_CANCEL:
        m_checkFlashStepCancel = true;
        m_checkFlashWaitCancel = true;
        m_isNeedCaptureFlash = true;
        m_isPreFlash = false;
        break;
    case FLASH_STEP_END:
        break;
    default:
        break;
    }

    ALOGD("DEBUG(%s[%d]):flashStepVal=%d", __FUNCTION__, __LINE__, (int)flashStepVal);

    if (flashStepVal != FLASH_STEP_OFF)
        m_flashStepErrorCount = 0;

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

    ALOGD("DEBUG(%s[%d]):flashTriggerVal=%d", __FUNCTION__, __LINE__, (int)flashTriggerVal);

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
    ALOGV("DEBUG(%s[%d]):mainCaptureRcount=%d", __FUNCTION__, __LINE__, m_mainCaptureRcount);

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
                ALOGV("DEBUG(%s[%d]):aeWaitMaxCount=%d", __FUNCTION__, __LINE__, m_aeWaitMaxCount);
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
        ALOGW("DEBUG(%s):waiting too much (%d msec)", __FUNCTION__, totalWaitingTime);
        ret = false;
    }

    return ret;
}

bool ExynosCameraActivityFlash::waitMainReady(void)
{
    bool ret = true;
    unsigned int totalWaitingTime = 0;
    unsigned int waitTimeoutFpsValue = 0;

    ALOGV("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    if (getFpsValue() > 0) {
        waitTimeoutFpsValue = 30 / getFpsValue();
    }
    if (waitTimeoutFpsValue < 1)
        waitTimeoutFpsValue = 1;

    ALOGI("INFO(%s[%d]):waitTimeoutFpsValue(%d) , getFpsValue(%d)",
        __FUNCTION__, __LINE__, waitTimeoutFpsValue, getFpsValue());

    while (m_flashStatus < FLASH_STATUS_MAIN_READY &&
        totalWaitingTime <= FLASH_MAX_PRE_DONE_WAITING_TIME * waitTimeoutFpsValue &&
        m_checkFlashWaitCancel == false && m_isPreFlash == true) {
        ALOGV("DEBUG(%s[%d]):(%d)(%d)(%d)", __FUNCTION__, __LINE__, m_flashStatus, totalWaitingTime, m_checkFlashWaitCancel);

        usleep(FLASH_WAITING_SLEEP_TIME);
        totalWaitingTime += FLASH_WAITING_SLEEP_TIME;
    }

    if (m_flashStatus < FLASH_STATUS_MAIN_READY && FLASH_MAX_PRE_DONE_WAITING_TIME * waitTimeoutFpsValue < totalWaitingTime) {
        ALOGW("DEBUG(%s)::waiting too much (%d msec)", __FUNCTION__, totalWaitingTime);
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
    ALOGV("DEBUG(%s[%d]):mainCaptureFcount=%d", __FUNCTION__, __LINE__, m_mainCaptureFcount);

    return true;
}

int ExynosCameraActivityFlash::checkMainCaptureFcount(int fcount)
{
    ALOGV("DEBUG(%s[%d]):mainCaptureFcount=%d, fcount=%d",
        __FUNCTION__, __LINE__, m_mainCaptureFcount, fcount);

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
    ALOGD("DEBUG(%s[%d]):m_isNeedFlash=%d", __FUNCTION__, __LINE__, m_isNeedFlash);
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

    ALOGD("DEBUG(%s[%d]): aeState %d", __FUNCTION__, __LINE__, (int)m_aeState);

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
        ALOGD("DEBUG(%s[%d]): AF DONE (for flash)", __FUNCTION__, __LINE__);
    }
}

void ExynosCameraActivityFlash::setMainFlashFiring(bool isMainFlashFiring)
{
    m_isMainFlashFiring = isMainFlashFiring;
}

void ExynosCameraActivityFlash::setManualExposureTime(uint64_t exposureTime)
{
    ALOGD("DEBUG(%s[%d]):exposureTime(%lld)", __FUNCTION__, __LINE__, (long long)exposureTime);
    m_manualExposureTime = exposureTime;
}

}/* namespace android */

