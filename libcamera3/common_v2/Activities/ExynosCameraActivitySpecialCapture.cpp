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
#define LOG_TAG "ExynosCameraActivitySpecialCapture"
#include <log/log.h>

#include "ExynosCameraActivitySpecialCapture.h"

#define TIME_CHECK 1

namespace android {

class ExynosCamera;

ExynosCameraActivitySpecialCapture::ExynosCameraActivitySpecialCapture(int cameraId) : ExynosCameraActivityBase(cameraId)
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0x1F;
    t_reqStatus = 0;

    m_hdrFcount = 0;
    m_currentInputFcount = 0;
    m_backupAeExpCompensation = 0;
    m_hdrStartFcount[0] = 0;
    m_hdrStartFcount[1] = 0;
    m_hdrStartFcount[2] = 0;
    m_hdrDropFcount[0] = 0;
    m_hdrDropFcount[1] = 0;
    m_hdrDropFcount[2] = 0;
    m_delay = 0;
    m_specialCaptureMode = SCAPTURE_MODE_NONE;
    m_check = false;
    m_specialCaptureStep = SCAPTURE_STEP_OFF;
    m_backupSceneMode = AA_SCENE_MODE_DISABLED;
    m_backupAaMode = AA_CONTROL_OFF;
    m_backupAeLock = AA_AE_LOCK_OFF;
    memset(m_backupAeTargetFpsRange, 0x00, sizeof(m_backupAeTargetFpsRange));
    m_backupFrameDuration = 0L;
#ifdef OIS_CAPTURE
    m_multiCaptureMode = false;
    m_waitSignalTime = 100000000;
    m_waitAvailable = false;
    m_OISCaptureFcount = 0;
#endif /* OIS_CAPTURE */
#ifdef RAWDUMP_CAPTURE
    m_RawCaptureFcount = 0;
#endif
    m_dynamicPickMode = DYNAMIC_PICK_NONE;
    m_DynamicPickCaptureFcount = 0;
    memset(m_hdrBuffer, 0x00, sizeof(m_hdrBuffer));
}

ExynosCameraActivitySpecialCapture::~ExynosCameraActivitySpecialCapture()
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0x1F;
    t_reqStatus = 0;

    m_hdrFcount = 0;
    m_currentInputFcount = 0;
    m_backupAeExpCompensation = 0;
    m_hdrStartFcount[0] = 0;
    m_hdrStartFcount[1] = 0;
    m_hdrStartFcount[2] = 0;
    m_hdrDropFcount[0] = 0;
    m_hdrDropFcount[1] = 0;
    m_hdrDropFcount[2] = 0;
    m_delay = 0;
    m_specialCaptureMode = SCAPTURE_MODE_NONE;
    m_check = false;
}

int ExynosCameraActivitySpecialCapture::t_funcNull(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_funcSensorBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_funcSensorAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);
    int ret = 1;

    if (shot_ext != NULL && m_specialCaptureMode == SCAPTURE_MODE_HDR) {
        if (m_hdrDropFcount[2] + 1 == shot_ext->shot.dm.request.frameCount) {
            ret = 2;

            CLOGD("(%d / B_LOCK %d)", m_hdrStartFcount[0], shot_ext->shot.dm.request.frameCount);
        }

        if (m_hdrDropFcount[2] + 2 == shot_ext->shot.dm.request.frameCount) {
            ret = 3;

            CLOGD("(%d / B_LOCK %d)", m_hdrStartFcount[1], shot_ext->shot.dm.request.frameCount);
        }

        if (m_hdrDropFcount[2]  + 3 == shot_ext->shot.dm.request.frameCount) {
            ret = 4;

            CLOGD("(%d / B_LOCK %d)", m_hdrStartFcount[2], shot_ext->shot.dm.request.frameCount);
        }
    }

    return ret;
}

int ExynosCameraActivitySpecialCapture::t_funcISPBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_funcISPAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_func3ABefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext != NULL && m_specialCaptureMode == SCAPTURE_MODE_HDR) {
        m_currentInputFcount = shot_ext->shot.dm.request.frameCount;

        /* HACK UNLOCK AE */
#ifndef USE_LSI_3A
        shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif

        if (m_specialCaptureStep == SCAPTURE_STEP_START) {
            m_backupAeExpCompensation = shot_ext->shot.ctl.aa.aeExpCompensation;
            m_backupAaMode = shot_ext->shot.ctl.aa.mode;
            m_backupAeLock = shot_ext->shot.ctl.aa.aeLock;
            m_backupSceneMode = shot_ext->shot.ctl.aa.sceneMode;

            m_specialCaptureStep = SCAPTURE_STEP_MINUS_SET;
            CLOGD("SCAPTURE_STEP_START");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_MINUS_SET) {
            shot_ext->shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_HDR;
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif
            m_specialCaptureStep = SCAPTURE_STEP_ZERO_SET;
            CLOGD("SCAPTURE_STEP_MINUS_SET");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_ZERO_SET) {
            m_delay = 0;
            shot_ext->shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_HDR;
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif
            m_specialCaptureStep = SCAPTURE_STEP_PLUS_SET;
            CLOGD("SCAPTURE_STEP_ZERO_SET");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_PLUS_SET) {
            shot_ext->shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_HDR;
            shot_ext->shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif

            m_specialCaptureStep = SCAPTURE_STEP_RESTORE;
            CLOGD("SCAPTURE_STEP_PLUS_SET");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_RESTORE) {
            shot_ext->shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext->shot.ctl.aa.sceneMode = AA_SCENE_MODE_HDR;
            shot_ext->shot.ctl.aa.aeLock = m_backupAeLock;
            shot_ext->shot.ctl.aa.aeExpCompensation = m_backupAeExpCompensation;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif

            m_specialCaptureStep = SCAPTURE_STEP_WAIT_CAPTURE_DELAY;
            CLOGD("SCAPTURE_STEP_RESTORE");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_WAIT_CAPTURE_DELAY) {
            shot_ext->shot.ctl.aa.sceneMode = m_backupSceneMode;
            shot_ext->shot.ctl.aa.mode = m_backupAaMode;
            shot_ext->shot.ctl.aa.aeLock = m_backupAeLock;
            shot_ext->shot.ctl.aa.aeExpCompensation = m_backupAeExpCompensation;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif

            m_specialCaptureStep = SCAPTURE_STEP_WAIT_CAPTURE;
            CLOGD("SCAPTURE_STEP_WAIT_CAPTURE_DELAY");
        } else if (m_specialCaptureStep == SCAPTURE_STEP_WAIT_CAPTURE) {
            m_specialCaptureStep = SCAPTURE_STEP_WAIT_CAPTURE;
            shot_ext->shot.ctl.aa.sceneMode = m_backupSceneMode;
            shot_ext->shot.ctl.aa.mode = m_backupAaMode;
            shot_ext->shot.ctl.aa.aeLock = m_backupAeLock;
            shot_ext->shot.ctl.aa.aeExpCompensation = m_backupAeExpCompensation;
#ifdef USE_LSI_3A
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
#endif
        } else {
            m_specialCaptureStep = SCAPTURE_STEP_OFF;
            m_delay = 0;
            m_check = false;
        }
    }
#ifdef OIS_CAPTURE
    else if (shot_ext != NULL && m_specialCaptureMode == SCAPTURE_MODE_OIS) {
        switch (m_specialCaptureStep) {
        case SCAPTURE_STEP_START:
            if (m_multiCaptureMode == false) {
                /* HACK: On single OIS capture mode, Capture intent is delivered by setControl at takePicture */
                shot_ext->shot.ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
                m_specialCaptureStep = SCAPTURE_STEP_WAIT_CAPTURE;
            } else {
                shot_ext->shot.ctl.aa.captureIntent = AA_CAPTURE_INTENT_STILL_CAPTURE_OIS_MULTI;
            }

            CLOGD("SCAPTURE_STEP_START m_multiCaptureMode(%d)", m_multiCaptureMode);
            break;
        case SCAPTURE_STEP_WAIT_CAPTURE:
            shot_ext->shot.ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
            if(m_multiCaptureMode == true)
                m_specialCaptureStep = SCAPTURE_STEP_END;

            CLOGD("SCAPTURE_STEP_WAIT_CAPTURE m_multiCaptureMode(%d)", m_multiCaptureMode);
            break;
        case SCAPTURE_STEP_END:
            if(m_multiCaptureMode == true) {
                m_specialCaptureStep = SCAPTURE_STEP_OFF;
                m_specialCaptureMode = SCAPTURE_MODE_NONE;
                m_multiCaptureMode = false;
            }

            CLOGD("SCAPTURE_STEP_END m_multiCaptureMode(%d)", m_multiCaptureMode);
            break;
        default:
            m_specialCaptureStep = SCAPTURE_STEP_OFF;
            m_specialCaptureMode = SCAPTURE_MODE_NONE;
            break;
        }
    }
#endif
#ifdef RAWDUMP_CAPTURE
    else if (shot_ext != NULL && m_specialCaptureMode == SCAPTURE_MODE_RAW) {
        switch (m_specialCaptureStep) {
            case SCAPTURE_STEP_START:
                shot_ext->shot.ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
                m_specialCaptureStep = SCAPTURE_STEP_OFF;
                CLOGD("SCAPTURE_STEP_START");
                break;
            case SCAPTURE_STEP_OFF:
                shot_ext->shot.ctl.aa.captureIntent = AA_CAPTURE_INTENT_PREVIEW;
                CLOGD("SCAPTURE_STEP_OFF");
                break;
            default:
                m_specialCaptureStep = SCAPTURE_STEP_OFF;
                m_specialCaptureMode = SCAPTURE_MODE_NONE;
                break;
        }
    }
#endif

    return 1;
}

int ExynosCameraActivitySpecialCapture::t_func3AAfter(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    CLOGV("");

    if (shot_ext == NULL)
        return 1;

    switch (m_specialCaptureMode) {
        case SCAPTURE_MODE_HDR:
            if (shot_ext->shot.dm.flash.vendor_firingStable == 2) {
                m_hdrStartFcount[0] = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_hdrStartFcount[0] (%d / %d)",
                    m_hdrStartFcount[0], shot_ext->shot.dm.request.frameCount);
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == 3) {
                m_hdrStartFcount[1] = shot_ext->shot.dm.request.frameCount;
                m_check = true;
                CLOGD("m_hdrStartFcount[1] (%d / %d)",
                    m_hdrStartFcount[1], shot_ext->shot.dm.request.frameCount);
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == 4) {
                m_hdrStartFcount[2] = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_hdrStartFcount[2] (%d / %d)",
                    m_hdrStartFcount[2], shot_ext->shot.dm.request.frameCount);
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_HDR_DARK) {
                m_hdrDropFcount[0] = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_hdrDropFcount[0] (%d / %d)",
                    m_hdrDropFcount[0], shot_ext->shot.dm.request.frameCount);
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_HDR_NORMAL) {
                m_hdrDropFcount[1] = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_hdrDropFcount[1] (%d / %d)",
                    m_hdrDropFcount[1], shot_ext->shot.dm.request.frameCount);
            }

            if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_HDR_BRIGHT) {
                m_hdrDropFcount[2] = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_hdrDropFcount[2] (%d / %d)",
                    m_hdrDropFcount[2], shot_ext->shot.dm.request.frameCount);
            }
            break;

        case SCAPTURE_MODE_DYNAMIC_PICK:
            if (m_DynamicPickCaptureFcount == 0) {
                switch (m_dynamicPickMode) {
                    case DYNAMIC_PICK_OUTFOCUS:
                        if (shot_ext->shot.dm.lens.state == LENS_STATE_STATIONARY) {
                            m_DynamicPickCaptureFcount = shot_ext->shot.dm.request.frameCount + 1;
                        }
                        break;
                    default:
                        break;
                }
            }

            if (m_DynamicPickCaptureFcount) {
                CLOGD("m_DynamicPickCaptureFcount (%d / %d)",
                    m_DynamicPickCaptureFcount, shot_ext->shot.dm.request.frameCount);
                m_specialCaptureMode = SCAPTURE_MODE_NONE;
                m_dynamicPickMode = DYNAMIC_PICK_NONE;
            }
            break;

#ifdef OIS_CAPTURE
        case SCAPTURE_MODE_OIS:
            if (m_OISCaptureFcount == 0
                && (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_ZSL_LIKE
                    || shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_STREAM_ON_CAPTURE)) {
                int OISCaptureDelay = 0;
                m_OISCaptureFcount = shot_ext->shot.dm.request.frameCount;
                if (shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_STREAM_ON_CAPTURE) {
                    OISCaptureDelay = 1;
                } else {
                    OISCaptureDelay = 2;
                }
                m_OISCaptureFcount += OISCaptureDelay;

                if(m_waitAvailable) {
                    m_SignalCondition.signal();
                    CLOGD("shutter callback signal!!!");
                }
                if(m_multiCaptureMode == false) {
                    m_specialCaptureStep = SCAPTURE_STEP_OFF;
                    m_specialCaptureMode = SCAPTURE_MODE_NONE;
                }
                CLOGD("m_OISCaptureFcount (%d / %d) OISCaptureDelay(%d) firingStable(%d)",
                    m_OISCaptureFcount, shot_ext->shot.dm.request.frameCount, OISCaptureDelay
                    , shot_ext->shot.dm.flash.vendor_firingStable);
            }
            break;
#endif

#ifdef RAWDUMP_CAPTURE
        case SCAPTURE_MODE_RAW:
            if (m_RawCaptureFcount == 0 && shot_ext->shot.dm.flash.vendor_firingStable == CAPTURE_STATE_RAW_CAPTURE)
            {
                m_RawCaptureFcount = shot_ext->shot.dm.request.frameCount;
                CLOGD("m_RawCaptureFcount (%d / %d)",
                    m_RawCaptureFcount, shot_ext->shot.dm.request.frameCount);
                m_specialCaptureMode = SCAPTURE_MODE_NONE;
            }
            break;
#endif

        default:
            break;
    }

    return 1;
}

int ExynosCameraActivitySpecialCapture::t_func3ABeforeHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_func3AAfterHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivitySpecialCapture::t_funcVRABefore(__unused void *args)
{
    CLOGV("");

    return 1;
}


int ExynosCameraActivitySpecialCapture::t_funcVRAAfter(__unused void *args)
{
    CLOGV("");

    return 1;
}

int ExynosCameraActivitySpecialCapture::setCaptureMode(enum SCAPTURE_MODE sCaptureModeVal)
{
    m_specialCaptureMode = sCaptureModeVal;

    CLOGD("(%d)", m_specialCaptureMode);

    return 1;
}

int ExynosCameraActivitySpecialCapture::setCaptureMode(enum SCAPTURE_MODE sCaptureModeVal, int ModeVal)
{
    m_specialCaptureMode = sCaptureModeVal;
    m_dynamicPickMode = (enum DYNAMIC_PICK_MODE)ModeVal;
    CLOGD("specialCaptureMode(%d), dynamicPickMode(%d)", m_specialCaptureMode, m_dynamicPickMode);

    return 1;
}

int ExynosCameraActivitySpecialCapture::getIsHdr()
{
    if (m_specialCaptureMode == SCAPTURE_MODE_HDR)
        return true;
    else
        return false;
}

int ExynosCameraActivitySpecialCapture::setCaptureStep(enum SCAPTURE_STEP sCaptureStepVal)
{
    m_specialCaptureStep = sCaptureStepVal;

    if (m_specialCaptureStep == SCAPTURE_STEP_OFF) {
        m_hdrFcount = 0;
        m_currentInputFcount = 0;
        m_backupAeExpCompensation = 0;
        m_hdrStartFcount[0] = 0;
        m_check = false;

        m_hdrStartFcount[0] = 0;
        m_hdrStartFcount[1] = 0;
        m_hdrStartFcount[2] = 0;
        m_hdrDropFcount[0] = 0;
        m_hdrDropFcount[1] = 0;
        m_hdrDropFcount[2] = 0;

        /* dealloc buffers */
    }

    if (m_specialCaptureStep == SCAPTURE_STEP_START) {
        /* alloc buffers */
    }

    CLOGD("(%d)", m_specialCaptureStep);

    return 1;
}

unsigned int ExynosCameraActivitySpecialCapture::getHdrStartFcount(int index)
{
    return m_hdrStartFcount[index];
}

unsigned int ExynosCameraActivitySpecialCapture::getHdrDropFcount(void)
{
    return m_hdrDropFcount[0];
}

int ExynosCameraActivitySpecialCapture::resetHdrStartFcount()
{
    CLOGD("reset hdrStartFcount[%d, %d, %d]",
        m_hdrStartFcount[0], m_hdrStartFcount[1], m_hdrStartFcount[2]);

    m_hdrStartFcount[0] = m_hdrStartFcount[1] = m_hdrStartFcount[2] = 0;

    return 1;
}

int ExynosCameraActivitySpecialCapture::getHdrWaitFcount()
{
    return HDR_WAIT_COUNT;
}

void ExynosCameraActivitySpecialCapture::setHdrBuffer(ExynosCameraBuffer *secondBuffer, ExynosCameraBuffer *thirdBuffer)
{
    m_hdrBuffer[0] = secondBuffer;
    m_hdrBuffer[1] = thirdBuffer;

    CLOGD("(%p / %p)", m_hdrBuffer[0], secondBuffer);
    CLOGD("(%p / %p)", m_hdrBuffer[1], thirdBuffer);
    CLOGD("(%d) (%d)", m_hdrBuffer[0]->size[0], m_hdrBuffer[0]->size[1]);
    CLOGD("(%d) (%d)", m_hdrBuffer[1]->size[0], m_hdrBuffer[1]->size[1]);

    return;
}

ExynosCameraBuffer *ExynosCameraActivitySpecialCapture::getHdrBuffer(int index)
{
    CLOGD("(%d)", index);

    return (m_hdrBuffer[index]);
}

unsigned int ExynosCameraActivitySpecialCapture::getDynamicPickCaptureFcount(void)
{
    return m_DynamicPickCaptureFcount;
}

void ExynosCameraActivitySpecialCapture::resetDynamicPickCaptureFcount()
{
    CLOGD(" ");

    m_DynamicPickCaptureFcount = 0;
}

#ifdef OIS_CAPTURE
void ExynosCameraActivitySpecialCapture::setMultiCaptureMode(bool enable)
{
    CLOGD("(%d)", enable);
    m_multiCaptureMode = enable;
}

bool ExynosCameraActivitySpecialCapture::getMultiCaptureMode(void)
{
    return m_multiCaptureMode;
}

unsigned int ExynosCameraActivitySpecialCapture::getOISCaptureFcount(void)
{
    return m_OISCaptureFcount;
}

void ExynosCameraActivitySpecialCapture::resetOISCaptureFcount()
{
    CLOGD("reset : %d", m_OISCaptureFcount);

    m_OISCaptureFcount = 0;
}

void ExynosCameraActivitySpecialCapture::waitShutterCallback()
{
    CLOGD("waitSignalTime : %llu", m_waitSignalTime);

    m_SignalMutex.lock();
    m_waitAvailable = true;
    m_SignalCondition.waitRelative(m_SignalMutex, m_waitSignalTime);
    m_waitAvailable = false;
    m_SignalMutex.unlock();
}
#endif

#ifdef RAWDUMP_CAPTURE
unsigned int ExynosCameraActivitySpecialCapture::getRawCaptureFcount(void)
{
    return m_RawCaptureFcount;
}

void ExynosCameraActivitySpecialCapture::resetRawCaptureFcount()
{
    CLOGD("reset : %d", m_RawCaptureFcount);

    m_RawCaptureFcount = 0;
}
#endif
} /* namespace android */

