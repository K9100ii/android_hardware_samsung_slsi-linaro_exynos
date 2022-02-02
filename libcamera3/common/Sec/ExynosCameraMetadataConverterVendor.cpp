/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraMetadataConverterVendor"

#include "ExynosCameraMetadataConverter.h"
#include "ExynosCameraRequestManager.h"

namespace android {

void ExynosCameraMetadataConverter::m_constructVendorDefaultRequestSettings( int type, CameraMetadata *settings)
{
    /** android.stats */
    uint8_t faceDetectMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    settings->update(ANDROID_STATISTICS_FACE_DETECT_MODE, &faceDetectMode, 1);

    return;
}

void ExynosCameraMetadataConverter::m_constructVendorStaticInfo(struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                CameraMetadata *info, int cameraId)
{
    return;
}

status_t ExynosCameraMetadataConverter::initShotVendorData(struct camera2_shot *shot)
{
    /* utrl */
    shot->uctl.opMode = CAMERA_OP_MODE_HAL3_GED;

    return OK;
}

status_t ExynosCameraMetadataConverter::translateControlControlData(CameraMetadata *settings,
                                                                     struct camera2_shot_ext *dst_ext,
                                                                     struct CameraMetaParameters *metaParameters)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;
    uint32_t vendorAeMode = 0;
    uint32_t vendorAfMode = 0;

    if (m_flashMgr == NULL) {
        CLOGE("FlashMgr is NULL!!");
        return BAD_VALUE;
    }

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    translatePreVendorControlControlData(settings, dst_ext);

    /* ANDROID_CONTROL_AE_ANTIBANDING_MODE */
    entry = settings->find(ANDROID_CONTROL_AE_ANTIBANDING_MODE);
    if (entry.count > 0) {
        dst->ctl.aa.aeAntibandingMode = (enum aa_ae_antibanding_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        if (dst->ctl.aa.aeAntibandingMode != AA_AE_ANTIBANDING_OFF) {
            dst->ctl.aa.aeAntibandingMode = (enum aa_ae_antibanding_mode) m_defaultAntibanding;
        }
        CLOGV("ANDROID_COLOR_AE_ANTIBANDING_MODE(%d) m_defaultAntibanding(%d)",
            entry.data.u8[0], m_defaultAntibanding);
    }

    /* ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION */
    entry = settings->find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    if (entry.count > 0) {
        dst->ctl.aa.aeExpCompensation = (int32_t) (entry.data.i32[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
        if (prev_entry.count > 0) {
            prev_value = (int32_t) (prev_entry.data.i32[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.aeExpCompensation) {
            CLOGD("ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION(%d)",
                dst->ctl.aa.aeExpCompensation);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AE_MODE */
    entry = settings->find(ANDROID_CONTROL_AE_MODE);
    if (entry.count > 0) {
        enum aa_aemode aeMode = AA_AEMODE_OFF;
        enum ExynosCameraActivityFlash::FLASH_REQ flashReq = ExynosCameraActivityFlash::FLASH_REQ_OFF;

        vendorAeMode = entry.data.u8[0];
        aeMode = (enum aa_aemode) FIMC_IS_METADATA(entry.data.u8[0]);

        dst->ctl.aa.aeMode = aeMode;
        dst->uctl.flashMode = CAMERA_FLASH_MODE_OFF;

        switch (aeMode) {
        case AA_AEMODE_ON_AUTO_FLASH_REDEYE:
            metaParameters->m_flashMode = FLASH_MODE_RED_EYE;
            flashReq = ExynosCameraActivityFlash::FLASH_REQ_AUTO;
            dst->ctl.aa.aeMode = AA_AEMODE_CENTER;
            dst->uctl.flashMode = CAMERA_FLASH_MODE_RED_EYE;
            m_overrideFlashControl = true;
            break;
        case AA_AEMODE_ON_AUTO_FLASH:
            metaParameters->m_flashMode = FLASH_MODE_AUTO;
            flashReq = ExynosCameraActivityFlash::FLASH_REQ_AUTO;
            dst->ctl.aa.aeMode = AA_AEMODE_CENTER;
            dst->uctl.flashMode = CAMERA_FLASH_MODE_AUTO;
            m_overrideFlashControl = true;
            break;
        case AA_AEMODE_ON_ALWAYS_FLASH:
            metaParameters->m_flashMode = FLASH_MODE_ON;
            flashReq = ExynosCameraActivityFlash::FLASH_REQ_ON;
            dst->ctl.aa.aeMode = AA_AEMODE_CENTER;
            dst->uctl.flashMode = CAMERA_FLASH_MODE_ON;
            m_overrideFlashControl = true;
            break;
        case AA_AEMODE_ON:
            dst->ctl.aa.aeMode = AA_AEMODE_CENTER;
        case AA_AEMODE_OFF:
            metaParameters->m_flashMode = FLASH_MODE_OFF;
            m_overrideFlashControl = false;
            break;
        default:
            m_parameters->setFlashMode(FLASH_MODE_OFF);
            m_overrideFlashControl = false;
            break;
        }

        CLOGV("flashReq(%d)", flashReq);
        m_flashMgr->setFlashExposure(aeMode);
        m_flashMgr->setFlashReq(flashReq, m_overrideFlashControl);

        /* ANDROID_CONTROL_AE_MODE */
        entry = m_prevMeta->find(ANDROID_CONTROL_AE_MODE);
        if (entry.count > 0) {
            prev_value = entry.data.u8[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != vendorAeMode) {
            CLOGD("ANDROID_CONTROL_AE_MODE(%d)", vendorAeMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AE_LOCK */
    entry = settings->find(ANDROID_CONTROL_AE_LOCK);
    if (entry.count > 0) {
        dst->ctl.aa.aeLock = (enum aa_ae_lock) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AE_LOCK);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_ae_lock) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.aeLock) {
            CLOGD("ANDROID_CONTROL_AE_LOCK(%d)", dst->ctl.aa.aeLock);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AE_REGIONS */
    entry = settings->find(ANDROID_CONTROL_AE_REGIONS);
    if (entry.count > 0) {
        ExynosRect2 aeRegion;

        aeRegion.x1 = entry.data.i32[0];
        aeRegion.y1 = entry.data.i32[1];
        aeRegion.x2 = entry.data.i32[2];
        aeRegion.y2 = entry.data.i32[3];
        dst->ctl.aa.aeRegions[4] = entry.data.i32[4];

        m_convertActiveArrayTo3AARegion(&aeRegion);

        dst->ctl.aa.aeRegions[0] = aeRegion.x1;
        dst->ctl.aa.aeRegions[1] = aeRegion.y1;
        dst->ctl.aa.aeRegions[2] = aeRegion.x2;
        dst->ctl.aa.aeRegions[3] = aeRegion.y2;
        CLOGV("ANDROID_CONTROL_AE_REGIONS(%d,%d,%d,%d,%d)",
                entry.data.i32[0],
                entry.data.i32[1],
                entry.data.i32[2],
                entry.data.i32[3],
                entry.data.i32[4]);

        // If AE region has meaningful value, AE region can be applied to the output image
        if (entry.data.i32[0] && entry.data.i32[1] && entry.data.i32[2] && entry.data.i32[3]) {
            dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_SPOT;
            CLOGV("update AA_AEMODE(%d)", dst->ctl.aa.aeMode);
        }
    }

    /* ANDROID_CONTROL_AWB_REGIONS */
    /* AWB region value would not be used at the f/w,
    because AWB is not related with a specific region */
    entry = settings->find(ANDROID_CONTROL_AWB_REGIONS);
    if (entry.count > 0) {
        ExynosRect2 awbRegion;

        awbRegion.x1 = entry.data.i32[0];
        awbRegion.y1 = entry.data.i32[1];
        awbRegion.x2 = entry.data.i32[2];
        awbRegion.y2 = entry.data.i32[3];
        dst->ctl.aa.awbRegions[4] = entry.data.i32[4];
        m_convertActiveArrayTo3AARegion(&awbRegion);

        dst->ctl.aa.awbRegions[0] = awbRegion.x1;
        dst->ctl.aa.awbRegions[1] = awbRegion.y1;
        dst->ctl.aa.awbRegions[2] = awbRegion.x2;
        dst->ctl.aa.awbRegions[3] = awbRegion.y2;
        CLOGV("ANDROID_CONTROL_AWB_REGIONS(%d,%d,%d,%d,%d)",
                entry.data.i32[0],
                entry.data.i32[1],
                entry.data.i32[2],
                entry.data.i32[3],
                entry.data.i32[4]);
    }

    /* ANDROID_CONTROL_AF_REGIONS */
    entry = settings->find(ANDROID_CONTROL_AF_REGIONS);
    if (entry.count > 0) {
        ExynosRect2 afRegion;

        afRegion.x1 = entry.data.i32[0];
        afRegion.y1 = entry.data.i32[1];
        afRegion.x2 = entry.data.i32[2];
        afRegion.y2 = entry.data.i32[3];
        dst->ctl.aa.afRegions[4] = entry.data.i32[4];
        m_convertActiveArrayTo3AARegion(&afRegion);

        dst->ctl.aa.afRegions[0] = afRegion.x1;
        dst->ctl.aa.afRegions[1] = afRegion.y1;
        dst->ctl.aa.afRegions[2] = afRegion.x2;
        dst->ctl.aa.afRegions[3] = afRegion.y2;
        CLOGV("ANDROID_CONTROL_AF_REGIONS(%d,%d,%d,%d,%d)",
                entry.data.i32[0],
                entry.data.i32[1],
                entry.data.i32[2],
                entry.data.i32[3],
                entry.data.i32[4]);
    }

    /* ANDROID_CONTROL_AE_TARGET_FPS_RANGE */
    entry = settings->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
    if (entry.count > 0) {
        int32_t prev_fps[2] = {0, };
        uint32_t max_fps, min_fps;

        m_parameters->checkPreviewFpsRange(entry.data.i32[0], entry.data.i32[1]);

        bool delayed = false;
#ifdef SUPPORT_RESTART_TRANSITION_HIGHSPEED
        if (m_parameters->getRestartStream() == true) {
            delayed = true;
        }
#endif

        m_parameters->getPreviewFpsRange(&min_fps, &max_fps, delayed);
        dst->ctl.aa.aeTargetFpsRange[0] = min_fps;
        dst->ctl.aa.aeTargetFpsRange[1] = max_fps;
        m_maxFps = dst->ctl.aa.aeTargetFpsRange[1];

        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AE_TARGET_FPS_RANGE);
        if (prev_entry.count > 0) {
            prev_fps[0] = prev_entry.data.i32[0];
            prev_fps[1] = prev_entry.data.i32[1];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_fps[0] != dst->ctl.aa.aeTargetFpsRange[0] ||
            prev_fps[1] != dst->ctl.aa.aeTargetFpsRange[1]) {
            CLOGD("ANDROID_CONTROL_AE_TARGET_FPS_RANGE(%d-%d)",
                dst->ctl.aa.aeTargetFpsRange[0], dst->ctl.aa.aeTargetFpsRange[1]);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER */
    entry = settings->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    if (entry.count > 0) {
        dst->ctl.aa.aePrecaptureTrigger = (enum aa_ae_precapture_trigger) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_ae_precapture_trigger) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.aePrecaptureTrigger) {
            CLOGD("ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER(%d)",
                dst->ctl.aa.aePrecaptureTrigger);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AF_MODE */
    entry = settings->find(ANDROID_CONTROL_AF_MODE);
    if (entry.count > 0) {
        int faceDetectionMode = FACEDETECT_MODE_OFF;
        camera_metadata_entry_t fd_entry;

        fd_entry = settings->find(ANDROID_STATISTICS_FACE_DETECT_MODE);
        if (fd_entry.count > 0) {
            faceDetectionMode = (enum facedetect_mode) FIMC_IS_METADATA(fd_entry.data.u8[0]);
        }

        vendorAfMode = entry.data.u8[0];
        dst->ctl.aa.afMode = (enum aa_afmode) FIMC_IS_METADATA(entry.data.u8[0]);

        switch (dst->ctl.aa.afMode) {
        case AA_AFMODE_AUTO:
            if (faceDetectionMode > FACEDETECT_MODE_OFF) {
                dst->ctl.aa.vendor_afmode_option = 0x00 | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
            } else {
                dst->ctl.aa.vendor_afmode_option = 0x00;
            }
            dst->ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            break;
        case AA_AFMODE_MACRO:
            dst->ctl.aa.vendor_afmode_option = 0x00 | SET_BIT(AA_AFMODE_OPTION_BIT_MACRO);
            dst->ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            break;
        case AA_AFMODE_CONTINUOUS_VIDEO:
            dst->ctl.aa.vendor_afmode_option = 0x00 | SET_BIT(AA_AFMODE_OPTION_BIT_VIDEO);
            dst->ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            /* The afRegion value should be (0,0,0,0) at the Continuous Video mode */
            dst->ctl.aa.afRegions[0] = 0;
            dst->ctl.aa.afRegions[1] = 0;
            dst->ctl.aa.afRegions[2] = 0;
            dst->ctl.aa.afRegions[3] = 0;
            break;
        case AA_AFMODE_CONTINUOUS_PICTURE:
            if (faceDetectionMode > FACEDETECT_MODE_OFF) {
                dst->ctl.aa.vendor_afmode_option = 0x00 | SET_BIT(AA_AFMODE_OPTION_BIT_FACE);
            } else {
                dst->ctl.aa.vendor_afmode_option = 0x00;
            }
            dst->ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            /* The afRegion value should be (0,0,0,0) at the Continuous Picture mode */
            dst->ctl.aa.afRegions[0] = 0;
            dst->ctl.aa.afRegions[1] = 0;
            dst->ctl.aa.afRegions[2] = 0;
            dst->ctl.aa.afRegions[3] = 0;
            break;
        case AA_AFMODE_OFF:
        default:
            dst->ctl.aa.vendor_afmode_option = 0x00;
            dst->ctl.aa.vendor_afmode_ext = AA_AFMODE_EXT_OFF;
            break;
        }

        m_preAfMode = m_afMode;
        m_afMode = dst->ctl.aa.afMode;

        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AF_MODE);
        if (prev_entry.count > 0) {
            prev_value = prev_entry.data.u8[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != entry.data.u8[0]) {
            CLOGD("ANDROID_CONTROL_AF_MODE(%d)", entry.data.u8[0]);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AF_TRIGGER */
    entry = settings->find(ANDROID_CONTROL_AF_TRIGGER);
    if (entry.count > 0) {
        dst->ctl.aa.afTrigger = (enum aa_af_trigger)entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AF_TRIGGER);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_af_trigger) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.afTrigger) {
            CLOGD("ANDROID_CONTROL_AF_TRIGGER(%d)", dst->ctl.aa.afTrigger);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AWB_LOCK */
    entry = settings->find(ANDROID_CONTROL_AWB_LOCK);
    if (entry.count > 0) {
        dst->ctl.aa.awbLock = (enum aa_awb_lock) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AWB_LOCK);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_awb_lock) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.awbLock) {
            CLOGD("ANDROID_CONTROL_AWB_LOCK(%d)", dst->ctl.aa.awbLock);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_AWB_MODE */
    entry = settings->find(ANDROID_CONTROL_AWB_MODE);
    if (entry.count > 0) {
        dst->ctl.aa.awbMode = (enum aa_awbmode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_AWB_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_awbmode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.awbMode) {
            CLOGD("ANDROID_CONTROL_AWB_MODE(%d)", dst->ctl.aa.awbMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_EFFECT_MODE */
    entry = settings->find(ANDROID_CONTROL_EFFECT_MODE);
    if (entry.count > 0) {
        dst->ctl.aa.effectMode = (enum aa_effect_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_EFFECT_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_effect_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.effectMode) {
            CLOGD("ANDROID_CONTROL_EFFECT_MODE(%d)", dst->ctl.aa.effectMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_MODE */
    entry = settings->find(ANDROID_CONTROL_MODE);
    if (entry.count > 0) {
        dst->ctl.aa.mode = (enum aa_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.mode) {
            CLOGD("ANDROID_CONTROL_MODE(%d)", dst->ctl.aa.mode);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_MODE check must be prior to ANDROID_CONTROL_SCENE_MODE check */
    /* ANDROID_CONTROL_SCENE_MODE */
    entry = settings->find(ANDROID_CONTROL_SCENE_MODE);
    if (entry.count > 0) {
        uint8_t scene_mode;

        scene_mode = (uint8_t)entry.data.u8[0];
        m_sceneMode = scene_mode;
        setSceneMode(scene_mode, dst_ext);
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_SCENE_MODE);
        if (prev_entry.count > 0) {
            prev_value = (uint8_t)prev_entry.data.u8[0];
            isMetaExist = true;
        }

        if (m_parameters->getRestartStream() == false) {
            if (isMetaExist && prev_value != scene_mode) {
                if (prev_value == ANDROID_CONTROL_SCENE_MODE_HDR
                    || scene_mode == ANDROID_CONTROL_SCENE_MODE_HDR) {
                    m_parameters->setRestartStream(true);
                    CLOGD("setRestartStream(SCENE_MODE_HDR)");
                }
            }
        }

        if (!isMetaExist || prev_value != scene_mode) {
            CLOGD("ANDROID_CONTROL_SCENE_MODE(%d)", scene_mode);
        }
        isMetaExist = false;
    }

    /* ANDROID_CONTROL_VIDEO_STABILIZATION_MODE */
    entry = settings->find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE);
    if (entry.count > 0) {
        dst->ctl.aa.videoStabilizationMode = (enum aa_videostabilization_mode) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_videostabilization_mode) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.videoStabilizationMode) {
            CLOGD("ANDROID_CONTROL_VIDEO_STABILIZATION_MODE(%d)",
                dst->ctl.aa.videoStabilizationMode);
        }
        isMetaExist = false;
    }

    enum ExynosCameraActivityFlash::FLASH_STEP flashStep = ExynosCameraActivityFlash::FLASH_STEP_OFF;
    bool isFlashStepChanged = false;

    /* Check Precapture Trigger to turn on the pre-flash */
    switch (dst->ctl.aa.aePrecaptureTrigger) {
    case AA_AE_PRECAPTURE_TRIGGER_START:
        if (m_flashMgr->getNeedCaptureFlash() == true
            && m_flashMgr->getFlashStatus() == AA_FLASHMODE_OFF) {
            flashStep = ExynosCameraActivityFlash::FLASH_STEP_PRE_START;
            m_flashMgr->setCaptureStatus(true);
            isFlashStepChanged = true;
        }
        break;
    case AA_AE_PRECAPTURE_TRIGGER_CANCEL:
        if (m_flashMgr->getNeedCaptureFlash() == true
            && m_flashMgr->getFlashStatus() != AA_FLASHMODE_OFF
            && m_flashMgr->getFlashStatus() != AA_FLASHMODE_CANCEL) {
            flashStep = ExynosCameraActivityFlash::FLASH_STEP_CANCEL;
            m_flashMgr->setCaptureStatus(false);
            isFlashStepChanged = true;
        }
        break;
    case AA_AE_PRECAPTURE_TRIGGER_IDLE:
    default:
        break;
    }
    /* Check Capture Intent to turn on the main-flash */

    /* ANDROID_CONTROL_CAPTURE_INTENT */
    entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
    if (entry.count > 0) {
        dst->ctl.aa.captureIntent = (enum aa_capture_intent) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_CONTROL_CAPTURE_INTENT);
        if (prev_entry.count > 0) {
            prev_value = (enum aa_capture_intent) prev_entry.data.u8[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.aa.captureIntent) {
            CLOGD("ANDROID_CONTROL_CAPTURE_INTENT(%d)",
                dst->ctl.aa.captureIntent);
        }
        isMetaExist = false;
    }

    switch (dst->ctl.aa.captureIntent) {
    case AA_CAPTURE_INTENT_STILL_CAPTURE:
        if (m_flashMgr->getNeedCaptureFlash() == true) {
            flashStep = ExynosCameraActivityFlash::FLASH_STEP_MAIN_START;
            isFlashStepChanged = true;
            m_parameters->setMarkingOfExifFlash(1);
        } else {
            m_parameters->setMarkingOfExifFlash(0);
        }
        break;
    case AA_CAPTURE_INTENT_VIDEO_RECORD:
        m_parameters->setRecordingHint(true);
        break;
    case AA_CAPTURE_INTENT_CUSTOM:
    case AA_CAPTURE_INTENT_PREVIEW:
    case AA_CAPTURE_INTENT_VIDEO_SNAPSHOT:
    case AA_CAPTURE_INTENT_ZERO_SHUTTER_LAG:
    case AA_CAPTURE_INTENT_MANUAL:
    default:
        break;
    }

    if (isFlashStepChanged == true && m_flashMgr != NULL)
        m_flashMgr->setFlashStep(flashStep);

    if (m_parameters->getRecordingHint()) {
        dst->ctl.aa.vendor_videoMode = AA_VIDEOMODE_ON;
    }

    /* If aeMode or Mode is NOT Off, Manual AE control can NOT be operated */
    if (dst->ctl.aa.aeMode == AA_AEMODE_OFF
        || dst->ctl.aa.mode == AA_CONTROL_OFF) {
        m_isManualAeControl = true;
        CLOGV("Operate Manual AE Control, aeMode(%d), Mode(%d)",
                dst->ctl.aa.aeMode, dst->ctl.aa.mode);
    } else {
        m_isManualAeControl = false;
    }
    m_parameters->setManualAeControl(m_isManualAeControl);

    translateVendorControlControlData(settings, dst_ext);

    return OK;
}

void ExynosCameraMetadataConverter::translatePreVendorControlControlData(CameraMetadata *settings,
                                                                         struct camera2_shot_ext *dst_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorControlControlData(CameraMetadata *settings,
							                                         struct camera2_shot_ext *dst_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorAeControlControlData(struct camera2_shot_ext *dst_ext,
                                                                    uint32_t vendorAeMode,
                                                                    aa_aemode *aeMode,
                                                                    ExynosCameraActivityFlash::FLASH_REQ *flashReq,
                                                                    struct CameraMetaParameters *metaParameters)
{
}

void ExynosCameraMetadataConverter::translateVendorAfControlControlData(struct camera2_shot_ext *dst_ext,
							                                            uint32_t vendorAfMode)
{
}

void ExynosCameraMetadataConverter::translateVendorLensControlData(CameraMetadata *settings,
                                                                   struct camera2_shot_ext *dst_ext,
                                                                   struct CameraMetaParameters *metaParameters)
{
}

void ExynosCameraMetadataConverter::translateVendorSensorControlData(CameraMetadata *settings,
                                                                    struct camera2_shot_ext *dst_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorLedControlData(CameraMetadata *settings,
                                                                   struct camera2_shot_ext *dst_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorControlMetaData(CameraMetadata *settings,
                                                                   struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorJpegMetaData(CameraMetadata *settings)
{
}

void ExynosCameraMetadataConverter::translateVendorLensMetaData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorScalerControlData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorSensorMetaData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorPartialLensMetaData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorPartialControlMetaData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext)
{
}

void ExynosCameraMetadataConverter::translateVendorPartialMetaData(CameraMetadata *settings, 
                                                                 struct camera2_shot_ext *src_ext, enum metadata_type metaType)
{
}

status_t ExynosCameraMetadataConverter::checkRangeOfValid(int32_t tag, int32_t value)
{
    return NO_ERROR;
}

void ExynosCameraMetadataConverter::setShootingMode(int shotMode, struct camera2_shot_ext *dst_ext)
{
}

void ExynosCameraMetadataConverter::setSceneMode(int value, struct camera2_shot_ext *dst_ext)
{
    enum aa_scene_mode sceneMode = AA_SCENE_MODE_FACE_PRIORITY;

    if (dst_ext->shot.ctl.aa.mode != AA_CONTROL_USE_SCENE_MODE) {
        return;
    }

    switch (value) {
    case ANDROID_CONTROL_SCENE_MODE_PORTRAIT:
        sceneMode = AA_SCENE_MODE_PORTRAIT;
        break;
    case ANDROID_CONTROL_SCENE_MODE_LANDSCAPE:
        sceneMode = AA_SCENE_MODE_LANDSCAPE;
        break;
    case ANDROID_CONTROL_SCENE_MODE_NIGHT:
        sceneMode = AA_SCENE_MODE_NIGHT;
        break;
    case ANDROID_CONTROL_SCENE_MODE_BEACH:
        sceneMode = AA_SCENE_MODE_BEACH;
        break;
    case ANDROID_CONTROL_SCENE_MODE_SNOW:
        sceneMode = AA_SCENE_MODE_SNOW;
        break;
    case ANDROID_CONTROL_SCENE_MODE_SUNSET:
        sceneMode = AA_SCENE_MODE_SUNSET;
        break;
    case ANDROID_CONTROL_SCENE_MODE_FIREWORKS:
        sceneMode = AA_SCENE_MODE_FIREWORKS;
        break;
    case ANDROID_CONTROL_SCENE_MODE_SPORTS:
        sceneMode = AA_SCENE_MODE_SPORTS;
        break;
    case ANDROID_CONTROL_SCENE_MODE_PARTY:
        sceneMode = AA_SCENE_MODE_PARTY;
        break;
    case ANDROID_CONTROL_SCENE_MODE_CANDLELIGHT:
        sceneMode = AA_SCENE_MODE_CANDLELIGHT;
        break;
    case ANDROID_CONTROL_SCENE_MODE_STEADYPHOTO:
        sceneMode = AA_SCENE_MODE_STEADYPHOTO;
        break;
    case ANDROID_CONTROL_SCENE_MODE_ACTION:
        sceneMode = AA_SCENE_MODE_ACTION;
        break;
    case ANDROID_CONTROL_SCENE_MODE_NIGHT_PORTRAIT:
        sceneMode = AA_SCENE_MODE_NIGHT_PORTRAIT;
        break;
    case ANDROID_CONTROL_SCENE_MODE_THEATRE:
        sceneMode = AA_SCENE_MODE_THEATRE;
        break;
    case ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY:
        sceneMode = AA_SCENE_MODE_FACE_PRIORITY;
        break;
    case ANDROID_CONTROL_SCENE_MODE_HDR:
        sceneMode = AA_SCENE_MODE_HDR;
        break;
    case ANDROID_CONTROL_SCENE_MODE_DISABLED:
    default:
        sceneMode = AA_SCENE_MODE_DISABLED;
        break;
    }

    setMetaCtlSceneMode(dst_ext, sceneMode);
}

status_t ExynosCameraMetadataConverter::m_createVendorControlAvailableVideoConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *streamConfigs)
{
    status_t ret = NO_ERROR;
    int (*availableVideoSizeList)[7] = NULL;
    int availableVideoSizeListLength = 0;
    int cropRatio = 0;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    if (sensorStaticInfo->availableVideoList == NULL) {
        CLOGE2("availableVideoList is NULL");
        return BAD_VALUE;
    }

    availableVideoSizeList = sensorStaticInfo->availableVideoList;
    availableVideoSizeListLength = sensorStaticInfo->availableVideoListMax;

    for (int i = 0; i < availableVideoSizeListLength; i++) {
        streamConfigs->add(availableVideoSizeList[i][0]);
        streamConfigs->add(availableVideoSizeList[i][1]);
        streamConfigs->add(availableVideoSizeList[i][2]/1000);
        streamConfigs->add(availableVideoSizeList[i][3]/1000);
        cropRatio = 0;
        /* cropRatio = vdisW * 1000 / nVideoW;
         * cropRatio = ((cropRatio - 1000) + 5) / 10 */
        if (availableVideoSizeList[i][4] != 0) {
            cropRatio = (int)(availableVideoSizeList[i][4] * 1000) / (int)availableVideoSizeList[i][0];
            cropRatio = ((cropRatio - 1000) + 5) / 10;
        }
        streamConfigs->add(cropRatio);
        streamConfigs->add(availableVideoSizeList[i][6]);
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createVendorControlAvailableHighSpeedVideoConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *streamConfigs)
{
    status_t ret = NO_ERROR;
    int (*availableHighSpeedVideoList)[5] = NULL;
    int availableHighSpeedVideoListLength = 0;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    if (sensorStaticInfo->availableHighSpeedVideoList == NULL) {
        CLOGE2("availableHighSpeedVideoList is NULL");
        return BAD_VALUE;
    }

    availableHighSpeedVideoList = sensorStaticInfo->availableHighSpeedVideoList;
    availableHighSpeedVideoListLength = sensorStaticInfo->availableHighSpeedVideoListMax;

    for (int i = 0; i < availableHighSpeedVideoListLength; i++) {
        streamConfigs->add(availableHighSpeedVideoList[i][0]);
        streamConfigs->add(availableHighSpeedVideoList[i][1]);
        streamConfigs->add(availableHighSpeedVideoList[i][2]/1000);
        streamConfigs->add(availableHighSpeedVideoList[i][3]/1000);
        streamConfigs->add(availableHighSpeedVideoList[i][4]);
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createVendorControlAvailableAeModeConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<uint8_t> *vendorAeModes)
{
    return NO_ERROR;
}

status_t ExynosCameraMetadataConverter::m_createVendorControlAvailableAfModeConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<uint8_t> *vendorAfModes)
{
    status_t ret = NO_ERROR;
    uint8_t (*baseAfModes) = NULL;
    int baseAfModesLength = 0;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (vendorAfModes == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    if (sensorStaticInfo->afModes == NULL) {
        CLOGE2("availableafModes is NULL");
        return BAD_VALUE;
    }

    baseAfModes = sensorStaticInfo->afModes;
    baseAfModesLength = sensorStaticInfo->afModesLength;

    /* default af modes */
    for (int i = 0; i < baseAfModesLength; i++) {
        vendorAfModes->add(baseAfModes[i]);
    }

    return ret;
}

#ifdef SUPPORT_DEPTH_MAP
status_t ExynosCameraMetadataConverter::m_createVendorDepthAvailableDepthConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *streamConfigs)
{
    status_t ret = NO_ERROR;
    int availableDepthSizeListMax = 0;
    int (*availableDepthSizeList)[2] = NULL;
    int availableDepthFormatListMax = 0;
    int *availableDepthFormatList = NULL;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }

    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    availableDepthSizeList = sensorStaticInfo->availableDepthSizeList;
    availableDepthSizeListMax = sensorStaticInfo->availableDepthSizeListMax;
    availableDepthFormatList = sensorStaticInfo->availableDepthFormatList;
    availableDepthFormatListMax = sensorStaticInfo->availableDepthFormatListMax;

    for (int i = 0; i < availableDepthFormatListMax; i++) {
        for (int j = 0; j < availableDepthSizeListMax; j++) {
            streamConfigs->add(availableDepthFormatList[i]);
            streamConfigs->add(availableDepthSizeList[j][0]);
            streamConfigs->add(availableDepthSizeList[j][1]);
            streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
        }
    }

#ifdef DEBUG_STREAM_CONFIGURATIONS
    const int32_t* streamConfigArray = NULL;
    streamConfigArray = streamConfigs->array();
    for (int i = 0; i < streamConfigs->size(); i = i + 4) {
        CLOGD2("Size %4dx%4d Format %2x %6s",
                streamConfigArray[i+1], streamConfigArray[i+2],
                streamConfigArray[i],
                (streamConfigArray[i+3] == ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)?
                "OUTPUT" : "INPUT");
    }
#endif

    return ret;
}
#endif

status_t ExynosCameraMetadataConverter::m_createVendorScalerAvailableThumbnailConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *streamConfigs)
{
    status_t ret = NO_ERROR;
    int availableThumbnailCallbackSizeListMax = 0;
    int (*availableThumbnailCallbackSizeList)[2] = NULL;
    int availableThumbnailCallbackFormatListMax = 0;
    int *availableThumbnailCallbackFormatList = NULL;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }

    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    availableThumbnailCallbackSizeList = sensorStaticInfo->availableThumbnailCallbackSizeList;
    availableThumbnailCallbackSizeListMax = sensorStaticInfo->availableThumbnailCallbackSizeListMax;
    availableThumbnailCallbackFormatList = sensorStaticInfo->availableThumbnailCallbackFormatList;
    availableThumbnailCallbackFormatListMax = sensorStaticInfo->availableThumbnailCallbackFormatListMax;

    for (int i = 0; i < availableThumbnailCallbackFormatListMax; i++) {
        for (int j = 0; j < availableThumbnailCallbackSizeListMax; j++) {
            streamConfigs->add(availableThumbnailCallbackFormatList[i]);
            streamConfigs->add(availableThumbnailCallbackSizeList[j][0]);
            streamConfigs->add(availableThumbnailCallbackSizeList[j][1]);
            streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
        }
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createVendorControlAvailableEffectModesConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<uint8_t> *vendorEffectModes)
{
    return NO_ERROR;
}

}; /* namespace android */
