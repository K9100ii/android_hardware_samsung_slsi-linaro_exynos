/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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
#include "SecCameraVendorTags.h"

namespace android {

void ExynosCamera3MetadataConverter::m_constructVendorDefaultRequestSettings(int type, CameraMetadata *settings)
{
#ifdef SAMSUNG_COMPANION
    /* PAF */
    int32_t pafmode = SAMSUNG_ANDROID_CONTROL_PAF_MODE_ON;
    settings->update(SAMSUNG_ANDROID_CONTROL_PAF_MODE, &pafmode, 1);

     /* Hdr level */
    int32_t hdrlevel = 0;
    settings->update(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL, &hdrlevel, 1);
#endif

#ifdef SAMSUNG_OIS
    /* OIS */
    const uint8_t oismode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON;
    settings->update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &oismode, 1);

    int32_t vendoroismode = SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_PICTURE;
    switch (type) {
    case CAMERA3_TEMPLATE_PREVIEW:
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
    case CAMERA3_TEMPLATE_MANUAL:
        vendoroismode = SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_PICTURE;
        break;
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        vendoroismode = SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_VIDEO;
        break;
    default:
        ALOGD("ERR(%s[%d]):Custom intent type is selected for setting control intent(%d)",
            __FUNCTION__, __LINE__, type);
        break;
    }
    settings->update(SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE, &vendoroismode, 1);
#endif

#ifdef SAMSUNG_CONTROL_METERING
    /* Metering */
    int32_t meteringmode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER;
    switch (type) {
    case CAMERA3_TEMPLATE_PREVIEW:
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
    case CAMERA3_TEMPLATE_MANUAL:
        meteringmode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER;
        break;
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        meteringmode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_MATRIX;
        break;
    default:
        ALOGD("ERR(%s[%d]):Custom intent type is selected for setting control intent(%d)",
            __FUNCTION__, __LINE__, type);
        break;
    }
    settings->update(SAMSUNG_ANDROID_CONTROL_METERING_MODE, &meteringmode, 1);
#endif
    return;
}

void ExynosCamera3MetadataConverter::m_constructVendorStaticInfo(struct ExynosSensorInfoBase *sensorStaticInfo, CameraMetadata *info)
{
    status_t ret = NO_ERROR;

#ifdef SAMSUNG_COMPANION
    /* samsung.android.control.liveHdrLevelRange */
    ret = info->update(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL_RANGE,
            sensorStaticInfo->vendorHdrRange,
            ARRAY_LENGTH(sensorStaticInfo->vendorHdrRange));
    if (ret < 0)
        ALOGD("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL_RANGE update failed(%d)",  __FUNCTION__, ret);

    /* samsung.android.control.pafAvailableMode */
    ret = info->update(SAMSUNG_ANDROID_CONTROL_PAF_AVAILABLE_MODE,
            &(sensorStaticInfo->vendorPafAvailable), 1);
    if (ret < 0)
        ALOGD("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_PAF_AVAILABLE_MODE update failed(%d)",  __FUNCTION__, ret);
#endif

#ifdef SAMSUNG_CONTROL_METERING
    /* samsung.android.control.meteringAvailableMode */
    if (sensorStaticInfo->vendorMeteringModes != NULL) {
        ret = info->update(SAMSUNG_ANDROID_CONTROL_METERING_AVAILABLE_MODE,
                sensorStaticInfo->vendorMeteringModes,
                sensorStaticInfo->vendorMeteringModesLength);
        if (ret < 0)
            ALOGD("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_METERING_AVAILABLE_MODE update failed(%d)",  __FUNCTION__, ret);
    } else {
        ALOGD("DEBUG(%s[%d]):vendorMeteringModes at sensorStaticInfo is NULL", __FUNCTION__, __LINE__);
    }
#endif

#ifdef SAMSUNG_OIS
    /* samsung.android.lens.info.availableOpticalStabilizationOperationMode */
    if (sensorStaticInfo->vendorOISModes != NULL) {
        ret = info->update(SAMSUNG_ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION_OPERATION_MODE ,
                sensorStaticInfo->vendorOISModes,
                sensorStaticInfo->vendorOISModesLength);
        if (ret < 0)
            ALOGD("DEBUG(%s):SAMSUNG_ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION_OPERATION_MODE",
                    "update failed(%d)", __FUNCTION__, ret);
    } else {
        ALOGD("DEBUG(%s[%d]):vendorOISModes at sensorStaticInfo is NULL", __FUNCTION__, __LINE__);
    }
#endif
    return;
}

void ExynosCamera3MetadataConverter::translateVendorControlControlData(CameraMetadata *settings,
                                                                    struct camera2_shot_ext *dst_ext)
{
    camera_metadata_entry_t entry;
    struct camera2_shot *dst = NULL;

    dst = &dst_ext->shot;

#ifdef SAMSUNG_CONTROL_METERING
    if (dst->ctl.aa.aeMode != AA_AEMODE_OFF) {
        if (settings->exists(SAMSUNG_ANDROID_CONTROL_METERING_MODE)) {
            entry = settings->find(SAMSUNG_ANDROID_CONTROL_METERING_MODE);
            switch (entry.data.i32[0]) {
            case SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER:
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_CENTER;
                break;
            case SAMSUNG_ANDROID_CONTROL_METERING_MODE_SPOT:
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_SPOT;
                break;
            case SAMSUNG_ANDROID_CONTROL_METERING_MODE_MATRIX:
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_MATRIX;
                break;
            case SAMSUNG_ANDROID_CONTROL_METERING_MODE_MANUAL:
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_SPOT_TOUCH;
                break;
            default:
                break;
            }
            ALOGV("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_METERING_MODE(%d, aeMode = %d)",
                __FUNCTION__, entry.data.u8[0], dst->ctl.aa.aeMode );
        }
    }
#endif

#ifdef SAMSUNG_COMPANION
    if (settings->exists(SAMSUNG_ANDROID_CONTROL_PAF_MODE)) {
        entry = settings->find(SAMSUNG_ANDROID_CONTROL_PAF_MODE);
        dst->uctl.companionUd.paf_mode = (enum companion_paf_mode) FIMC_IS_METADATA(entry.data.i32[0]);
        ALOGV("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_PAF_MODE(%d)", __FUNCTION__, entry.data.i32[0]);
    }

    /* HDR control priority : HDR_SCENE (high) > HDR_MODE > HDR_LEVEL (low) */
    int32_t cur_hdr_level = -1;
    if (settings->exists(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL)) {
        entry = settings->find(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL);
        cur_hdr_level = entry.data.i32[0];
        if (cur_hdr_level) {
            dst->uctl.companionUd.wdr_mode = COMPANION_WDR_AUTO;
            dst->uctl.companionUd.drc_mode = COMPANION_DRC_ON;
        }
        ALOGV("DEBUG(%s):SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL(%d)", __FUNCTION__, entry.data.i32[0]);
    }

    int32_t prev_hdr_level = -1;
    bool flagChanged = false;
    camera_metadata_entry_t prevEntry;
    if (m_parameters->getCheckRestartStream() == false) {
        if (m_prevMeta->exists(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL)
            && settings->exists(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL)) {
            prevEntry = m_prevMeta->find(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL);
            prev_hdr_level = prevEntry.data.i32[0];
            flagChanged = true;
        }

        if (flagChanged && prev_hdr_level != cur_hdr_level) {
            m_parameters->setCheckRestartStream(true);
            ALOGD("SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL changed(%d) (%d)",
                (int)prev_hdr_level, (int)cur_hdr_level);
        }
    }

    if (dst->ctl.aa.sceneMode == AA_SCENE_MODE_HDR) {
        dst->uctl.companionUd.wdr_mode = COMPANION_WDR_AUTO;
        dst->uctl.companionUd.drc_mode = COMPANION_DRC_ON;
    }

    m_parameters->setRTHdr(dst->uctl.companionUd.wdr_mode);
#endif

#ifdef SAMSUNG_CONTROL_METERING
    if (settings->exists(ANDROID_CONTROL_AE_REGIONS)) {
        ExynosRect2 aeRegion;

        entry = settings->find(ANDROID_CONTROL_AE_REGIONS);
        aeRegion.x1 = entry.data.i32[0];
        aeRegion.y1 = entry.data.i32[1];
        aeRegion.x2 = entry.data.i32[2];
        aeRegion.y2 = entry.data.i32[3];
        dst->ctl.aa.aeRegions[4] = entry.data.i32[4];

        m_convert3AARegion(&aeRegion);

        if (entry.data.i32[0] && entry.data.i32[1] && entry.data.i32[2] && entry.data.i32[3]) {
            if (dst->ctl.aa.aeMode == AA_AEMODE_CENTER) {
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_CENTER_TOUCH;
            } else if (dst->ctl.aa.aeMode == AA_AEMODE_MATRIX) {
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_MATRIX_TOUCH;
            } else if (dst->ctl.aa.aeMode == AA_AEMODE_SPOT) {
                dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_SPOT_TOUCH;
            }
        }

        dst->ctl.aa.aeRegions[0] = aeRegion.x1;
        dst->ctl.aa.aeRegions[1] = aeRegion.y1;
        dst->ctl.aa.aeRegions[2] = aeRegion.x2;
        dst->ctl.aa.aeRegions[3] = aeRegion.y2;
        ALOGV("DEBUG(%s):ANDROID_CONTROL_AE_REGIONS(%d,%d,%d,%d,%d)", __FUNCTION__,
                entry.data.i32[0],
                entry.data.i32[1],
                entry.data.i32[2],
                entry.data.i32[3],
                entry.data.i32[4]);
    } else {
        if (dst->ctl.aa.aeMode == AA_AEMODE_SPOT_TOUCH)
            dst->ctl.aa.aeMode = (enum aa_aemode)AA_AEMODE_CENTER; //default ae
    }
#endif
    return;
}

void ExynosCamera3MetadataConverter::translateVendorControlMetaData(CameraMetadata *service_settings,
                                                                    CameraMetadata *settings,
                                                                    struct camera2_shot_ext *src_ext, bool partialMeta)
{
    struct camera2_shot *src = NULL;
    camera_metadata_entry_t entry;

    src = &src_ext->shot;

#ifdef SAMSUNG_COMPANION
    int32_t vendorPafMode = (int32_t) CAMERA_METADATA(src->udm.companion.paf_mode);
    settings->update(SAMSUNG_ANDROID_CONTROL_PAF_MODE, &vendorPafMode, 1);
    ALOGV("DEBUG(%s): udm.companion.paf_mode(%d)", __FUNCTION__, src->udm.companion.paf_mode);

    int32_t vendorHdrMode = 0;
    if (src->udm.companion.wdr_mode == COMPANION_WDR_AUTO
        || src->udm.companion.wdr_mode == COMPANION_WDR_ON) {
        vendorHdrMode = 1;
    }
    settings->update(SAMSUNG_ANDROID_CONTROL_LIVE_HDR_LEVEL, &vendorHdrMode, 1);
    ALOGV("DEBUG(%s): udm.companion.wdr_mode(%d)", __FUNCTION__, src->udm.companion.wdr_mode);
#endif
#ifdef SAMSUNG_CONTROL_METERING
    if (partialMeta == true) {
        entry = service_settings->find(SAMSUNG_ANDROID_CONTROL_METERING_MODE);
        if (entry.count > 0) {
            int32_t vendorAeMode = entry.data.i32[0];                
            settings->update(SAMSUNG_ANDROID_CONTROL_METERING_MODE, &vendorAeMode, 1);
            ALOGV("DEBUG(%s):vendorAeMode(%d)", __FUNCTION__, vendorAeMode);
        }
    }
#endif

    return;
}

void ExynosCamera3MetadataConverter::translateVendorLensControlData(CameraMetadata *settings,
                                                                    struct camera2_shot_ext *dst_ext)
{
    camera_metadata_entry_t entry;
    struct camera2_shot *dst = NULL;

    dst = &dst_ext->shot;

#ifdef SAMSUNG_OIS
    if (settings->exists(SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE)) {
        entry = settings->find(SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE);

        if (dst->ctl.lens.opticalStabilizationMode == OPTICAL_STABILIZATION_MODE_STILL) {
            switch (entry.data.u8[0]) {
            case SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_VIDEO:
                dst->ctl.lens.opticalStabilizationMode = OPTICAL_STABILIZATION_MODE_VIDEO;
                break;
            }
        }
        ALOGV("DEBUG(%s):SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE(%d)",
                __FUNCTION__, entry.data.u8[0]);
    }
#endif
    return;
}

void ExynosCamera3MetadataConverter::translateVendorLensMetaData(CameraMetadata *settings,
                                                                 struct camera2_shot_ext *src_ext,
                                                                 struct camera2_shot_ext *service_settings)
{
    struct camera2_shot *src = NULL;
    struct camera2_shot *service_shot = NULL;

    src = &src_ext->shot;
    service_shot = &service_settings->shot;

#ifdef SAMSUNG_OIS
    int32_t vendorOpticalStabilizationMode = SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_PICTURE;
    uint8_t opticalStabilizationMode = (uint8_t) service_shot->ctl.lens.opticalStabilizationMode;

    switch (opticalStabilizationMode) {
    case OPTICAL_STABILIZATION_MODE_VIDEO:
        vendorOpticalStabilizationMode = SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE_VIDEO;
        break;
    default:
        break;
    }

    settings->update(SAMSUNG_ANDROID_LENS_OPTICAL_STABILIZATION_OPERATION_MODE, &vendorOpticalStabilizationMode, 1);
    ALOGV("DEBUG(%s):SAMSUNG_ANDROID_LENS is (%d)", __FUNCTION__,
            src->dm.lens.opticalStabilizationMode);
#endif
    return;
}

void ExynosCamera3MetadataConverter::translateVendorPartialMetaData(CameraMetadata *settings,
                                                                   struct camera2_shot_ext *src_ext)
{
    struct camera2_shot *src = NULL;
    src = &src_ext->shot;

#ifdef SAMSUNG_CONTROL_METERING
    int32_t vendorAeMode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_MANUAL;
    switch (src->dm.aa.aeMode) {
    case AA_AEMODE_CENTER:
        vendorAeMode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_CENTER;
        break;
    case AA_AEMODE_SPOT:
        vendorAeMode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_SPOT;
        break;
    case AA_AEMODE_MATRIX:
        vendorAeMode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_MATRIX;
        break;
    case AA_AEMODE_SPOT_TOUCH:
        vendorAeMode = SAMSUNG_ANDROID_CONTROL_METERING_MODE_MANUAL;
        break;
    default:
        break;
    }
    settings->update(SAMSUNG_ANDROID_CONTROL_METERING_MODE, &vendorAeMode, 1);
    ALOGV("DEBUG(%s):vendorAeMode(%d)", __FUNCTION__, vendorAeMode);
#endif

    return ;
}
}; /* namespace android */
