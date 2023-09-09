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
#define LOG_TAG "ExynosCameraConfigurationsSec"
#include <log/log.h>

#include "ExynosCameraConfigurations.h"
#include "ExynosCameraMetadataConverter.h"

namespace android {

int ExynosCameraConfigurations::m_staticValue[CONFIGURATION_STATIC_VALUE_MAX];

void ExynosCameraConfigurations::m_vendorSpecificConstructor(void)
{
    // CAUTION!! : Initial values must be prior to setDefaultParameter() function.
    // Initial Values : START
    m_samsungCamera = false;
    m_useFastenAeStable = false;

#ifdef SAMSUNG_OT
    m_metaParameters.m_startObjectTracking = false;
    m_objectTrackingArea.x1 = 0;
    m_objectTrackingArea.x2 = 0;
    m_objectTrackingArea.y1 = 0;
    m_objectTrackingArea.y2 = 0;
    m_objectTrackingWeight = 0;
#endif

#ifdef SAMSUNG_IDDQD
    m_lensDirtyDetected = false;
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    m_dualSelectedCam = -1;
    m_fallbackOnCount = 0;
    m_fallbackOffCount = 0;
    m_fusionParam = NULL;
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */

#ifdef SAMSUNG_SW_VDIS
    m_previewOffsetLeft = 0;
    m_previewOffsetTop = 0;
#endif

#ifdef SAMSUNG_SSM
    memset(&m_SSMRegion, 0, sizeof(ExynosRect2));
    setModeValue(CONFIGURATION_SSM_TRIGGER, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_TRIGGER_IDLE);
    setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_UNDEFINED);
    setModeValue(CONFIGURATION_SSM_MODE_VALUE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_MODE_MANUAL);
#endif

    m_isFactoryBin = false;
    if (m_scenario == SCENARIO_SECURE) {
        char propertyValue[PROPERTY_VALUE_MAX];

        m_exposureTime = m_staticInfo->exposureTime;
        m_gain = m_staticInfo->gain;
        m_ledPulseWidth = m_staticInfo->ledPulseWidth;
        m_ledPulseDelay = m_staticInfo->ledPulseDelay;
        m_ledCurrent = m_staticInfo->ledCurrent;

        property_get("ro.factory.factory_binary", propertyValue, "0");
        if (strncmp(propertyValue, "factory", 7) == 0) {
            m_isFactoryBin = true;
            m_ledMaxTime = 0L;
        } else {
            m_ledMaxTime = m_staticInfo->ledMaxTime;
        }

        setMode(CONFIGURATION_VISION_MODE, true);
    }

#ifdef SAMSUNG_TN_FEATURE
    m_width[CONFIGURATION_DS_YUV_STALL_SIZE] = YUVSTALL_DSCALED_SIZE_16_9_W;
    m_height[CONFIGURATION_DS_YUV_STALL_SIZE] = YUVSTALL_DSCALED_SIZE_16_9_H;
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_bokehBlurStrength = 0;
    m_zoomInOutPhoto = SAMSUNG_ANDROID_CONTROL_ZOOM_IN_OUT_PHOTO_OFF;
    m_bokehProcessResult = 0;

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    m_dualCaptureFlag = -1;
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    m_fpsProperty = checkFpsProperty();
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    m_bokehPreviewState = SAMSUNG_ANDROID_CONTROL_BOKEH_STATE_SUCCESS;
    m_bokehCaptureState = SAMSUNG_ANDROID_CONTROL_BOKEH_STATE_CAPTURE_PROCESSING_SUCCESS;
#endif
#if defined(SAMSUNG_DUAL_ZOOM_CAPTURE) && defined(DEBUG_IQ_CAPTURE_MODE)
    char propertyValue[PROPERTY_VALUE_MAX];
    property_get("system.camera.debug.capture", propertyValue, "0");
    int debugCaptureMode = atoi(propertyValue);

    if (debugCaptureMode == DEBUG_IQ_CAPTURE_MODE_ALWAYS_FUSION_ENABLE
        || debugCaptureMode == DEBUG_IQ_CAPTURE_MODE_ALWAYS_FUSION_DISABLE) {
        setModeValue(CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE, debugCaptureMode);
    } else {
        setModeValue(CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE, DEBUG_IQ_CAPTURE_MODE_NORMAL);
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    m_productColorInfo = checkColorCodeProperty();
#endif

    m_repeatingRequestHint = 0;
    m_temperature = 0xFFFF;

    setModeValue(CONFIGURATION_FLICKER_DATA, 0);

#ifdef SAMSUNG_OT
    memset(&m_OTfocusData, 0, sizeof(UniPluginFocusData_t));
#endif
}

void ExynosCameraConfigurations::m_vendorSpecificDestructor(void)
{
    return;
}

status_t ExynosCameraConfigurations::setSize(enum CONFIGURATION_SIZE_TYPE type, uint32_t width, uint32_t height, int outputPortId)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case CONFIGURATION_YUV_SIZE:
    {
        int widthArrayNum = sizeof(m_yuvWidth)/sizeof(m_yuvWidth[0]);
        int heightArrayNum = sizeof(m_yuvHeight)/sizeof(m_yuvHeight[0]);

        if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
            android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                    " YUV_OUTPUT_PORT_ID_MAX %d",
                    widthArrayNum, heightArrayNum,
                    YUV_OUTPUT_PORT_ID_MAX);
            return INVALID_OPERATION;
        }

        if (outputPortId < 0) {
            CLOGE("Invalid outputPortId(%d)", outputPortId);
            return INVALID_OPERATION;
        }

        m_yuvWidth[outputPortId] = width;
        m_yuvHeight[outputPortId] = height;
        break;
    }
    case CONFIGURATION_MIN_YUV_SIZE:
    case CONFIGURATION_MAX_YUV_SIZE:
    case CONFIGURATION_PREVIEW_SIZE:
    case CONFIGURATION_VIDEO_SIZE:
    case CONFIGURATION_PICTURE_SIZE:
    case CONFIGURATION_THUMBNAIL_SIZE:
        m_width[type] = width;
        m_height[type] = height;
        break;
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_THUMBNAIL_CB_SIZE:
        m_width[type] = width;
        m_height[type] = height;
        break;
    case CONFIGURATION_DS_YUV_STALL_SIZE:
    {
        int ratio = SIZE_RATIO(width, height);

        if (ratio == (int)SIZE_RATIO(4,3)) {
            m_width[type] = YUVSTALL_DSCALED_SIZE_4_3_W;
            m_height[type] = YUVSTALL_DSCALED_SIZE_4_3_H;
        } else if (ratio == (int)SIZE_RATIO(16,9)) {
            m_width[type] = YUVSTALL_DSCALED_SIZE_16_9_W;
            m_height[type] = YUVSTALL_DSCALED_SIZE_16_9_H;
        } else if (ratio == (int)SIZE_RATIO(1,1)) {
            m_width[type] = YUVSTALL_DSCALED_SIZE_1_1_W;
            m_height[type] = YUVSTALL_DSCALED_SIZE_1_1_H;
        }  else if (ratio == (int)SIZE_RATIO(18.5,9)) {
            m_width[type] = YUVSTALL_DSCALED_SIZE_18P5_9_W;
            m_height[type] = YUVSTALL_DSCALED_SIZE_18P5_9_H;
        }
        break;
    }
#endif
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraConfigurations::getSize(enum CONFIGURATION_SIZE_TYPE type, uint32_t *width, uint32_t *height, int outputPortId) const
{
    status_t ret = NO_ERROR;

    switch(type) {
    case CONFIGURATION_YUV_SIZE:
    {
        int widthArrayNum = sizeof(m_yuvWidth)/sizeof(m_yuvWidth[0]);
        int heightArrayNum = sizeof(m_yuvHeight)/sizeof(m_yuvHeight[0]);

        if (widthArrayNum != YUV_OUTPUT_PORT_ID_MAX
            || heightArrayNum != YUV_OUTPUT_PORT_ID_MAX) {
            android_printAssert(NULL, LOG_TAG, "ASSERT:Invalid yuvSize array length %dx%d."\
                    " YUV_OUTPUT_PORT_ID_MAX %d",
                    widthArrayNum, heightArrayNum,
                    YUV_OUTPUT_PORT_ID_MAX);
            return INVALID_OPERATION;
        }

        if (outputPortId < 0) {
            CLOGE("Invalid outputPortId(%d)", outputPortId);
            return INVALID_OPERATION;
        }

        *width = m_yuvWidth[outputPortId];
        *height = m_yuvHeight[outputPortId];
        break;
    }
    case CONFIGURATION_MIN_YUV_SIZE:
    case CONFIGURATION_MAX_YUV_SIZE:
    case CONFIGURATION_PREVIEW_SIZE:
    case CONFIGURATION_VIDEO_SIZE:
    case CONFIGURATION_PICTURE_SIZE:
    case CONFIGURATION_THUMBNAIL_SIZE:
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_THUMBNAIL_CB_SIZE:
    case CONFIGURATION_DS_YUV_STALL_SIZE:
#endif
        *width = m_width[type];
        *height = m_height[type];
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraConfigurations::resetSize(enum CONFIGURATION_SIZE_TYPE type)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case CONFIGURATION_YUV_SIZE:
        memset(m_yuvWidth, 0, sizeof(m_yuvWidth));
        memset(m_yuvHeight, 0, sizeof(m_yuvHeight));
        break;
    case CONFIGURATION_MIN_YUV_SIZE:
    case CONFIGURATION_MAX_YUV_SIZE:
    case CONFIGURATION_PREVIEW_SIZE:
    case CONFIGURATION_VIDEO_SIZE:
    case CONFIGURATION_PICTURE_SIZE:
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_THUMBNAIL_CB_SIZE:
#endif
        m_width[type] = 0;
        m_width[type] = 0;
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraConfigurations::setMode(enum CONFIGURATION_MODE_TYPE type, bool enable)
{
    status_t ret = NO_ERROR;

    switch(type) {
#ifdef USE_DUAL_CAMERA
    case CONFIGURATION_DUAL_MODE:
    case CONFIGURATION_DUAL_PRE_MODE:
        m_flagCheck[type] = true;
        m_mode[type] = enable;
        break;
#endif
    case CONFIGURATION_DYNAMIC_BAYER_MODE:
        m_mode[type] = enable;
        m_useDynamicBayer[CONFIG_MODE::NORMAL] = enable;
        break;
    case CONFIGURATION_PIP_MODE:
    case CONFIGURATION_RECORDING_MODE:
        m_flagCheck[type] = true;
    case CONFIGURATION_FULL_SIZE_LUT_MODE:
    case CONFIGURATION_ODC_MODE:
    case CONFIGURATION_VISION_MODE:
    case CONFIGURATION_PIP_RECORDING_MODE:
    case CONFIGURATION_VIDEO_STABILIZATION_MODE:
    case CONFIGURATION_HDR_MODE:
    case CONFIGURATION_HDR_RECORDING_MODE:
    case CONFIGURATION_DVFS_LOCK_MODE:
    case CONFIGURATION_MANUAL_AE_CONTROL_MODE:
    case CONFIGURATION_OBTE_MODE:
    case CONFIGURATION_OBTE_STREAM_CHANGING:
#ifdef SUPPORT_DEPTH_MAP
    case CONFIGURATION_DEPTH_MAP_MODE:
#endif
        m_mode[type] = enable;
        break;
    case CONFIGURATION_YSUM_RECORDING_MODE:
#ifdef USE_YSUM_RECORDING
        m_mode[type] = enable;

        if (getMode(CONFIGURATION_PIP_RECORDING_MODE) == true)
#endif
        {
            m_mode[type] = false;
        }
        CLOGD("YSUM Recording %s", m_mode[type] == true ? "ON" : "OFF");
        break;
#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_OT
    case CONFIGURATION_OBJECT_TRACKING_MODE:
#endif
    case CONFIGURATION_PRO_MODE:
    case CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE:
    case CONFIGURATION_FACTORY_TEST_MODE:
#ifdef SAMSUNG_SW_VDIS
    case CONFIGURATION_SWVDIS_MODE:
#endif
#ifdef SAMSUNG_HYPERLAPSE
    case CONFIGURATION_HYPERLAPSE_MODE:
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
    case CONFIGURATION_VIDEO_BEAUTY_MODE:
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
    case CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE:
#endif
#ifdef LLS_CAPTURE
    case CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE:
#ifdef SET_LLS_CAPTURE_SETFILE
    case CONFIGURATION_LLS_CAPTURE_MODE:
#endif
#endif
#ifdef OIS_CAPTURE
    case CONFIGURATION_OIS_CAPTURE_MODE:
#endif
#ifdef RAWDUMP_CAPTURE
    case CONFIGURATION_RAWDUMP_CAPTURE_MODE:
#endif
#ifdef SAMSUNG_STR_CAPTURE
    case CONFIGURATION_STR_CAPTURE_MODE:
#endif
#ifdef SAMSUNG_HRM
    case CONFIGURATION_HRM_MODE:
#endif
#ifdef SAMSUNG_LIGHT_IR
    case CONFIGURATION_LIGHT_IR_MODE:
#endif
#ifdef SAMSUNG_GYRO
    case CONFIGURATION_GYRO_MODE:
#endif
#ifdef SAMSUNG_ACCELEROMETER
    case CONFIGURATION_ACCELEROMETER_MODE:
#endif
#ifdef SAMSUNG_ROTATION
    case CONFIGURATION_ROTATION_MODE:
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    case CONFIGURATION_FUSION_CAPTURE_MODE:
#endif
#ifdef SUPPORT_ZSL_MULTIFRAME
    case CONFIGURATION_ZSL_MULTIFRAME_MODE:
#endif
#ifdef SAMSUNG_PROX_FLICKER
    case CONFIGURATION_PROX_FLICKER_MODE:
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    case CONFIGURATION_HIFIVIDEO_MODE:
#endif
#ifdef FAST_SHUTTER_NOTIFY
    case CONFIGURATION_FAST_SHUTTER_MODE:
#endif
#ifdef USE_ALWAYS_FD_ON
    case CONFIGURATION_ALWAYS_FD_ON_MODE:
#endif
        m_mode[type] = enable;
        break;
#endif
    default:
        break;
    }

    return ret;
}

bool ExynosCameraConfigurations::getMode(enum CONFIGURATION_MODE_TYPE type) const
{
    bool modeEnable = false;

    switch(type) {
    case CONFIGURATION_DYNAMIC_BAYER_MODE:
    {
        int32_t configMode = getConfigMode();
        if (configMode >= 0)
            modeEnable = m_useDynamicBayer[configMode];
        else {
            CLOGE(" invalid config mode. couldn't get Dynamic Bayer mode");
            return false;
        }
        break;
    }
    case CONFIGURATION_PIP_MODE:
    case CONFIGURATION_RECORDING_MODE:
#ifdef USE_DUAL_CAMERA
    case CONFIGURATION_DUAL_MODE:
    case CONFIGURATION_DUAL_PRE_MODE:
#endif
        if (m_flagCheck[type] == false) {
            return false;
        }
    case CONFIGURATION_FULL_SIZE_LUT_MODE:
    case CONFIGURATION_ODC_MODE:
    case CONFIGURATION_VISION_MODE:
    case CONFIGURATION_PIP_RECORDING_MODE:
    case CONFIGURATION_YSUM_RECORDING_MODE:
    case CONFIGURATION_VIDEO_STABILIZATION_MODE:
    case CONFIGURATION_HDR_MODE:
    case CONFIGURATION_HDR_RECORDING_MODE:
    case CONFIGURATION_DVFS_LOCK_MODE:
    case CONFIGURATION_MANUAL_AE_CONTROL_MODE:
    case CONFIGURATION_OBTE_MODE:
    case CONFIGURATION_OBTE_STREAM_CHANGING:
#ifdef SUPPORT_DEPTH_MAP
    case CONFIGURATION_DEPTH_MAP_MODE:
#endif
        modeEnable = m_mode[type];
        break;
    case CONFIGURATION_GMV_MODE:
        modeEnable = m_getGmvMode();
        break;
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_PRO_MODE:
    case CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE:
    case CONFIGURATION_FACTORY_TEST_MODE:
#ifdef SAMSUNG_SW_VDIS
    case CONFIGURATION_SWVDIS_MODE:
#endif
#ifdef SAMSUNG_HYPERLAPSE
    case CONFIGURATION_HYPERLAPSE_MODE:
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
    case CONFIGURATION_VIDEO_BEAUTY_MODE:
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
    case CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE:
#endif
#ifdef LLS_CAPTURE
    case CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE:
#ifdef SET_LLS_CAPTURE_SETFILE
    case CONFIGURATION_LLS_CAPTURE_MODE:
#endif
#endif
#ifdef OIS_CAPTURE
    case CONFIGURATION_OIS_CAPTURE_MODE:
#endif
#ifdef SAMSUNG_LENS_DC
    case CONFIGURATION_LENS_DC_MODE:
#endif
#ifdef RAWDUMP_CAPTURE
    case CONFIGURATION_RAWDUMP_CAPTURE_MODE:
#endif
#ifdef SAMSUNG_STR_CAPTURE
    case CONFIGURATION_STR_CAPTURE_MODE:
#endif
#ifdef SAMSUNG_OT
    case CONFIGURATION_OBJECT_TRACKING_MODE:
#endif
#ifdef SAMSUNG_HRM
    case CONFIGURATION_HRM_MODE:
#endif
#ifdef SAMSUNG_LIGHT_IR
    case CONFIGURATION_LIGHT_IR_MODE:
#endif
#ifdef SAMSUNG_GYRO
    case CONFIGURATION_GYRO_MODE:
#endif
#ifdef SAMSUNG_ACCELEROMETER
    case CONFIGURATION_ACCELEROMETER_MODE:
#endif
#ifdef SAMSUNG_ROTATION
    case CONFIGURATION_ROTATION_MODE:
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    case CONFIGURATION_FUSION_CAPTURE_MODE:
#endif
#ifdef SUPPORT_ZSL_MULTIFRAME
    case CONFIGURATION_ZSL_MULTIFRAME_MODE:
#endif
#ifdef SAMSUNG_PROX_FLICKER
    case CONFIGURATION_PROX_FLICKER_MODE:
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    case CONFIGURATION_HIFIVIDEO_MODE:
#endif
#ifdef FAST_SHUTTER_NOTIFY
    case CONFIGURATION_FAST_SHUTTER_MODE:
#endif
#ifdef USE_ALWAYS_FD_ON
    case CONFIGURATION_ALWAYS_FD_ON_MODE:
#endif
        modeEnable = m_mode[type];
        break;
#endif
    default:
        break;
    }

    return modeEnable;
}

bool ExynosCameraConfigurations::getDynamicMode(enum DYNAMIC_MODE_TYPE type) const
{
    bool modeEnable = false;

    switch(type) {
    case DYNAMIC_UHD_RECORDING_MODE:
    {
        uint32_t videoW = 0, videoH = 0;

        getSize(CONFIGURATION_VIDEO_SIZE, &videoW, &videoH);

        if (getMode(CONFIGURATION_RECORDING_MODE) == true
            && ((videoW == 3840 && videoH == 2160)
                || (videoW == 2560 && videoH == 1440))) {
            modeEnable = true;
        }
        break;
    }
    case DYNAMIC_HIGHSPEED_RECORDING_MODE:
    {
        uint32_t configMode = (uint32_t)getConfigMode();
        switch(configMode){
        case CONFIG_MODE::HIGHSPEED_60:
        case CONFIG_MODE::HIGHSPEED_120:
        case CONFIG_MODE::HIGHSPEED_240:
        case CONFIG_MODE::HIGHSPEED_480:
#ifdef SAMSUNG_SSM
        case CONFIG_MODE::SSM_240:
#endif
            modeEnable = true;
            break;
        case CONFIG_MODE::NORMAL:
        default:
            modeEnable = false;
            break;
        }
        break;
    }
#ifdef USE_DUAL_CAMERA
    case DYNAMIC_DUAL_FORCE_SWITCHING:
    {
        uint32_t videoW = 0, videoH = 0;

        getSize(CONFIGURATION_VIDEO_SIZE, &videoW, &videoH);

        if (getMode(CONFIGURATION_RECORDING_MODE) == true
            && (videoW == 3840 && videoH == 2160)) {
            modeEnable = true;
        }
        break;
    }
#endif
#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_HIFI_CAPTURE
    case DYNAMIC_HIFI_CAPTURE_MODE:
        if (getSamsungCamera() == true) {
            if (getMode(CONFIGURATION_RECORDING_MODE) == false
                && getMode(CONFIGURATION_PIP_MODE) == false
                && getMode(CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE) == true
#ifdef SAMSUNG_MFHDR_CAPTURE
                && getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_MF_HDR
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
                && getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_LL_HDR
#endif
                && getModeValue(CONFIGURATION_LD_CAPTURE_MODE) != MULTI_SHOT_MODE_NONE) {
                modeEnable = true;
            }
        }

        CLOGV("[LLS_MBR] HiFiCaptureEnable %d (SC %d, RH %d, DM %d, LLS %d, LD %d)",
                modeEnable, getSamsungCamera(),
                getMode(CONFIGURATION_RECORDING_MODE),
                getMode(CONFIGURATION_PIP_MODE),
                getMode(CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE),
                getModeValue(CONFIGURATION_LD_CAPTURE_MODE));
        break;
#endif
#ifdef SAMSUNG_IDDQD
    case DYNAMIC_IDDQD_MODE:
        if (getSamsungCamera() == false || getMode(CONFIGURATION_RECORDING_MODE) == true) {
                modeEnable = false;
        } else if (m_cameraId == CAMERA_ID_BACK || m_cameraId == CAMERA_ID_FRONT) {
            if (getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE) == MULTI_CAPTURE_MODE_NONE
                && (getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_AUTO
                    || getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_BEAUTY)) {
                modeEnable = true;
            }
        }
    break;
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    case DYNAMIC_DUAL_CAMERA_DISABLE:
    {
        uint32_t videoW = 0, videoH = 0;

        getSize(CONFIGURATION_VIDEO_SIZE, &videoW, &videoH);

        if (((videoW == 2560 && videoH == 1440)
            || (videoW == 1920 && videoH == 1080)
            || (videoW == 2224 && videoH == 1080))
            && getMode(CONFIGURATION_RECORDING_MODE) == true
            && getModeValue(CONFIGURATION_DUAL_CAMERA_DISABLE) == true) {
            modeEnable = true;
        }
        break;
    }
#endif
#endif
    default:
        break;
    }

    return modeEnable;
}

status_t ExynosCameraConfigurations::setStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type, __unused int value)
{
    status_t ret = NO_ERROR;

    switch(type) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    case CONFIGURATION_DUAL_DISP_FALLBACK_RESULT:
    case CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT:
    case CONFIGURATION_DUAL_OP_MODE_FALLBACK:
        m_staticValue[type] = value;
        break;
#endif
    default:
        break;
    }
    return ret;
}

int ExynosCameraConfigurations::getStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type) const
{
    int value = -1;

    switch(type) {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    case CONFIGURATION_DUAL_DISP_FALLBACK_RESULT:
    case CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT:
    case CONFIGURATION_DUAL_OP_MODE_FALLBACK:
        value = m_staticValue[type];
        break;
#endif
    default:
        break;
    }
    return value;
}

status_t ExynosCameraConfigurations::setModeValue(enum CONFIGURATION_VALUE_TYPE type, int value)
{
    status_t ret = NO_ERROR;

    switch(type) {
    case CONFIGURATION_JPEG_QUALITY:
    case CONFIGURATION_THUMBNAIL_QUALITY:
        if (value < 0 || value > 100) {
            CLOGE("Invalid mode(%d) value(%d)!", (int)type, value);
            return BAD_VALUE;
        }

        m_modeValue[type] = value;
        break;
    case CONFIGURATION_DEVICE_ORIENTATION:
        if (value < 0 || value % 90 != 0) {
            CLOGE("Invalid orientation value(%d)!", value);
            return BAD_VALUE;
        }

        m_modeValue[type] = value;
        break;
    case CONFIGURATION_FD_ORIENTATION:
        /* Gets the FD orientation angle in degrees. Calibrate FRONT FD orientation */
        if (getCameraId() == CAMERA_ID_FRONT || getCameraId() == CAMERA_ID_SECURE) {
            m_modeValue[type] = (value + FRONT_ROTATION + 180) % 360;
        } else {
            m_modeValue[type] = (value + BACK_ROTATION) % 360;
        }
        break;
    case CONFIGURATION_HIGHSPEED_MODE:
        if (value < 0) {
            CLOGE("Invalid Highspeed Mode value(%d)!", value);
            return BAD_VALUE;
        }

        m_modeValue[type] = value;
        setConfigMode((uint32_t)value);
        break;
    case CONFIGURATION_FLASH_MODE:
        m_modeValue[type] = value;
        break;
    case CONFIGURATION_BINNING_RATIO:
        if (value < MIN_BINNING_RATIO || value > MAX_BINNING_RATIO) {
            CLOGE(" Out of bound, ratio(%d), min:max(%d:%d)",
                    value, MAX_BINNING_RATIO, MAX_BINNING_RATIO);
            return BAD_VALUE;
        }

        m_modeValue[type] = value;
        break;
    case CONFIGURATION_OBTE_TUNING_MODE_NEW:
    case CONFIGURATION_OBTE_DATA_PATH_NEW:
        if (value < 0) {
            CLOGE("Invalid mode(%d) value(%d)!", (int)type, value);
            return BAD_VALUE;
        }

        if (m_modeValue[type] != value) {
            setMode(CONFIGURATION_OBTE_STREAM_CHANGING, true);
        }
    case CONFIGURATION_OBTE_TUNING_MODE:
    case CONFIGURATION_OBTE_DATA_PATH:
    case CONFIGURATION_VT_MODE:
    case CONFIGURATION_YUV_SIZE_RATIO_ID:
    case CONFIGURATION_FLIP_HORIZONTAL:
    case CONFIGURATION_FLIP_VERTICAL:
    case CONFIGURATION_RECORDING_FPS:
    case CONFIGURATION_BURSTSHOT_FPS:
    case CONFIGURATION_BURSTSHOT_FPS_TARGET:
    case CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT:
        if (value < 0) {
            CLOGE("Invalid mode(%d) value(%d)!", (int)type, value);
            return BAD_VALUE;
        }
    case CONFIGURATION_EXTEND_SENSOR_MODE:
    case CONFIGURATION_MARKING_EXIF_FLASH:
        m_modeValue[type] = value;
        break;
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_YUV_STALL_PORT:
        if (value < 0) {
            m_modeValue[type] = ExynosCameraConfigurations::YUV_2;
            break;
        }
    case CONFIGURATION_SHOT_MODE:
    case CONFIGURATION_MULTI_CAPTURE_MODE:
    case CONFIGURATION_SERIES_SHOT_MODE:
    case CONFIGURATION_SERIES_SHOT_COUNT:
    case CONFIGURATION_YUV_STALL_PORT_USAGE:
    case CONFIGURATION_PREV_TRANSIENT_ACTION_MODE:
    case CONFIGURATION_TRANSIENT_ACTION_MODE:
#ifdef SAMSUNG_HYPERLAPSE
    case CONFIGURATION_HYPERLAPSE_SPEED:
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_DUAL_PORTRAIT_BEAUTY)
    case CONFIGURATION_BEAUTY_RETOUCH_LEVEL:
#endif
#ifdef LLS_CAPTURE
    case CONFIGURATION_LLS_VALUE:
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    case CONFIGURATION_LD_CAPTURE_MODE:
    case CONFIGURATION_LD_CAPTURE_COUNT:
    case CONFIGURATION_LD_CAPTURE_LLS_VALUE:
#endif
    case CONFIGURATION_EXIF_CAPTURE_STEP_COUNT:
#ifdef SAMSUNG_DOF
    case CONFIGURATION_FOCUS_LENS_POS:
#endif
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    case CONFIGURATION_FACTORY_DRAM_TEST_STATE:
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    case CONFIGURATION_DUAL_DISP_CAM_TYPE:    /* result image base on main or sub image in solution */
    case CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE: /* solution recommend */
    case CONFIGURATION_DUAL_CAMERA_DISABLE:
    case CONFIGURATION_DUAL_PREVIEW_SHIFT_X:
    case CONFIGURATION_DUAL_PREVIEW_SHIFT_Y:
    case CONFIGURATION_DUAL_HOLD_FALLBACK_STATE:
    case CONFIGURATION_DUAL_2X_BUTTON:
#endif  /* SAMSUNG_DUAL_ZOOM_PREVIEW */
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    case CONFIGURATION_FUSION_CAPTURE_READY:
#ifdef DEBUG_IQ_CAPTURE_MODE
    case CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE:
#endif
#endif /* SAMSUNG_DUAL_ZOOM_CAPTURE */
#ifdef SAMSUNG_SSM
    case CONFIGURATION_SSM_SHOT_MODE:
    case CONFIGURATION_SSM_MODE_VALUE:
    case CONFIGURATION_SSM_STATE:
    case CONFIGURATION_SSM_TRIGGER:
#endif /* SAMSUNG_SSM */
#ifdef SAMSUNG_HIFI_VIDEO
    case CONFIGURATION_HIFIVIDEO_OPMODE:
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    case CONFIGURATION_CURRENT_BOKEH_PREVIEW_RESULT:    /* used for bokeh capture input value */
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    case CONFIGURATION_BEAUTY_FACE_SKIN_COLOR_LEVEL:
#endif /* SAMSUNG_DUAL_PORTRAIT_BEAUTY */
    case CONFIGURATION_FLICKER_DATA:
        m_modeValue[type] = value;
        break;
    case CONFIGURATION_OPERATION_MODE:
        m_modeValue[type] = value;

#ifdef SAMSUNG_FACTORY_DRAM_TEST
        if (m_modeValue[type] == OPERATION_MODE_DRAM_TEST) {
            setModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE, FACTORY_DRAM_TEST_CEHCKING);
        } else {
            setModeValue(CONFIGURATION_FACTORY_DRAM_TEST_STATE, FACTORY_DRAM_TEST_NONE);
        }
#endif
        break;
#endif
    default:
        break;
    }

    return ret;
}

int ExynosCameraConfigurations::getModeValue(enum CONFIGURATION_VALUE_TYPE type)
{
    int value = -1;

    switch (type) {
    case CONFIGURATION_OBTE_TUNING_MODE:
        if (getMode(CONFIGURATION_OBTE_STREAM_CHANGING) == false) {
            setModeValue(type, getModeValue(CONFIGURATION_OBTE_TUNING_MODE_NEW));
        }
        value = m_modeValue[type];
        break;
    case CONFIGURATION_OBTE_DATA_PATH:
        if (getMode(CONFIGURATION_OBTE_STREAM_CHANGING) == false) {
            setModeValue(type, getModeValue(CONFIGURATION_OBTE_DATA_PATH_NEW));
        }
        value = m_modeValue[type];
        break;
    case CONFIGURATION_OBTE_TUNING_MODE_NEW:
    case CONFIGURATION_OBTE_DATA_PATH_NEW:
    case CONFIGURATION_VT_MODE:
    case CONFIGURATION_JPEG_QUALITY:
    case CONFIGURATION_THUMBNAIL_QUALITY:
    case CONFIGURATION_YUV_SIZE_RATIO_ID:
    case CONFIGURATION_DEVICE_ORIENTATION:
    case CONFIGURATION_FD_ORIENTATION:
    case CONFIGURATION_FLIP_HORIZONTAL:
    case CONFIGURATION_FLIP_VERTICAL:
    case CONFIGURATION_HIGHSPEED_MODE:
    case CONFIGURATION_RECORDING_FPS:
    case CONFIGURATION_BURSTSHOT_FPS:
    case CONFIGURATION_BURSTSHOT_FPS_TARGET:
    case CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT:
    case CONFIGURATION_EXTEND_SENSOR_MODE:
    case CONFIGURATION_MARKING_EXIF_FLASH:
    case CONFIGURATION_FLASH_MODE:
    case CONFIGURATION_BINNING_RATIO:
        value = m_modeValue[type];
        break;
#ifdef SAMSUNG_TN_FEATURE
    case CONFIGURATION_SHOT_MODE:
    case CONFIGURATION_MULTI_CAPTURE_MODE:
    case CONFIGURATION_OPERATION_MODE:
    case CONFIGURATION_SERIES_SHOT_MODE:
    case CONFIGURATION_SERIES_SHOT_COUNT:
    case CONFIGURATION_YUV_STALL_PORT:
    case CONFIGURATION_YUV_STALL_PORT_USAGE:
    case CONFIGURATION_PREV_TRANSIENT_ACTION_MODE:
    case CONFIGURATION_TRANSIENT_ACTION_MODE:
#ifdef SAMSUNG_HYPERLAPSE
    case CONFIGURATION_HYPERLAPSE_SPEED:
#endif
#ifdef LLS_CAPTURE
    case CONFIGURATION_LLS_VALUE:
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    case CONFIGURATION_LD_CAPTURE_MODE:
    case CONFIGURATION_LD_CAPTURE_COUNT:
    case CONFIGURATION_LD_CAPTURE_LLS_VALUE:
#endif
    case CONFIGURATION_EXIF_CAPTURE_STEP_COUNT:
#ifdef SAMSUNG_DOF
    case CONFIGURATION_FOCUS_LENS_POS:
#endif
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    case CONFIGURATION_FACTORY_DRAM_TEST_STATE:
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    case CONFIGURATION_DUAL_DISP_CAM_TYPE:
    case CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE:
    case CONFIGURATION_DUAL_CAMERA_DISABLE:
    case CONFIGURATION_DUAL_PREVIEW_SHIFT_X:
    case CONFIGURATION_DUAL_PREVIEW_SHIFT_Y:
    case CONFIGURATION_DUAL_HOLD_FALLBACK_STATE:
    case CONFIGURATION_DUAL_2X_BUTTON:
#endif  /* SAMSUNG_DUAL_ZOOM_PREVIEW */
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    case CONFIGURATION_FUSION_CAPTURE_READY:
#ifdef DEBUG_IQ_CAPTURE_MODE
    case CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE:
#endif
#endif /* SAMSUNG_DUAL_ZOOM_CAPTURE */
#ifdef SAMSUNG_SSM
    case CONFIGURATION_SSM_SHOT_MODE:
    case CONFIGURATION_SSM_MODE_VALUE:
    case CONFIGURATION_SSM_STATE:
    case CONFIGURATION_SSM_TRIGGER:
#endif /* SAMSUNG_SSM */
#ifdef SAMSUNG_HIFI_VIDEO
    case CONFIGURATION_HIFIVIDEO_OPMODE:
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_DUAL_PORTRAIT_BEAUTY)
    case CONFIGURATION_BEAUTY_RETOUCH_LEVEL:
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    case CONFIGURATION_CURRENT_BOKEH_PREVIEW_RESULT:    /* used for bokeh capture input value */
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    case CONFIGURATION_BEAUTY_FACE_SKIN_COLOR_LEVEL:
#endif /* SAMSUNG_DUAL_PORTRAIT_BEAUTY */
    case CONFIGURATION_FLICKER_DATA:
        value = m_modeValue[type];
        break;
#endif
    default:
        break;
    }

    return value;
}

bool ExynosCameraConfigurations::isSupportedFunction(enum SUPPORTED_FUNCTION_TYPE type) const
{
    bool functionSupport = false;

    switch(type) {
    case SUPPORTED_FUNCTION_GDC:
#ifdef SUPPORT_HW_GDC
        functionSupport = SUPPORT_HW_GDC;
#endif
        break;
    case SUPPORTED_FUNCTION_SERVICE_BATCH_MODE:
#ifdef USE_SERVICE_BATCH_MODE
        functionSupport = true;
#else
        functionSupport = false;
#endif
        break;
    default:
        break;
    }

    return functionSupport;
}

void ExynosCameraConfigurations::setFastEntrance(bool flag)
{
     m_fastEntrance = flag;
}

bool ExynosCameraConfigurations::getFastEntrance(void) const
{
    return m_fastEntrance;
}

void ExynosCameraConfigurations::setSamsungCamera(bool flag)
{
     m_samsungCamera = flag;
}

bool ExynosCameraConfigurations::getSamsungCamera(void) const
{
    return m_samsungCamera;
}

void ExynosCameraConfigurations::setUseFastenAeStable(bool enable)
{
     m_useFastenAeStable = enable;
}

bool ExynosCameraConfigurations::getUseFastenAeStable(void) const
{
    return m_useFastenAeStable;
}

status_t ExynosCameraConfigurations::setParameters(const CameraParameters& params)
{
    status_t ret = NO_ERROR;

    if (m_checkFastEntrance(params) !=  NO_ERROR) {
        CLOGE("checkFastEntrance faild");
    }

    if (m_checkSamsungCamera(params) !=  NO_ERROR) {
        CLOGE("checkSamsungCamera faild");
    }

    if (m_checkShootingMode(params) != NO_ERROR) {
        CLOGE("checkShootingMode failed");
    }

    if (m_checkRecordingFps(params) !=  NO_ERROR) {
        CLOGE("checkRecordingFps faild");
    }

#ifdef SAMSUNG_TN_FEATURE
    if (m_checkFactorytest(params) != NO_ERROR) {
        CLOGE("checkFactorytest fail");
    }
#endif

    if (m_checkOperationMode(params) !=  NO_ERROR) {
        CLOGE("checkOperationMode faild");
    }

#ifdef SAMSUNG_SSM
    if (m_checkSSMShotMode(params) != NO_ERROR) {
        CLOGE("m_checkSSMShotMode failed");
    }
#endif

#ifdef USE_DUAL_CAMERA
    if (m_checkDualMode(params) !=  NO_ERROR) {
        CLOGE("checkDualMode faild");
    }
#endif

#ifdef SAMSUNG_SW_VDIS
    if (m_checkSWVdisMode(params) !=  NO_ERROR) {
        CLOGE("checkSWVdisMode faild");
    }
#endif

#ifdef SAMSUNG_HIFI_VIDEO
    if (m_checkHiFiVideoMode(params) !=  NO_ERROR) {
        CLOGE("checkHiFiVideoMode faild");
    }
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY
    if (m_checkVideoBeautyMode(params) !=  NO_ERROR) {
        CLOGE("checkVideoBeautyMode faild");
    }
#endif

    if (m_checkVtMode(params) != NO_ERROR) {
        CLOGE("checkVtMode failed");
    }

    if (m_checkExtSensorMode() != NO_ERROR) {
        CLOGE("checkExtSensorMode failed");
    }

    if (m_checkRecordingDrMode(params) != NO_ERROR) {
        CLOGE("m_checkRecordingDRMode failed");
    }

    return ret;
}

status_t ExynosCameraConfigurations::m_checkFastEntrance(const CameraParameters& params)
{
    const char *newStrFastEntrance = params.get("first-entrance");
    bool fastEntrance = false;

    if (newStrFastEntrance != NULL
        && !strcmp(newStrFastEntrance, "true")) {
        fastEntrance = true;
    }

    CLOGD("Fast Entrance %s !!!", (fastEntrance?"Enabled":"Disabled"));
    setFastEntrance(fastEntrance);

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_checkSamsungCamera(const CameraParameters& params)
{
    const char *newStrSamsungCamera = params.get("samsungcamera");
    bool samsungCamera = false;

    if (newStrSamsungCamera != NULL
        && !strcmp(newStrSamsungCamera, "true")) {
        samsungCamera = true;
    }

    CLOGD("Samsung Camera %s !!!", (samsungCamera?"Enabled":"Disabled"));
    setSamsungCamera(samsungCamera);

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_checkRecordingFps(const CameraParameters& params)
{
    int newRecordingFps = params.getInt("recording-fps");

    CLOGD("%d fps", newRecordingFps);
    setModeValue(CONFIGURATION_RECORDING_FPS, newRecordingFps);

    return NO_ERROR;
}

#ifdef SAMSUNG_TN_FEATURE
status_t ExynosCameraConfigurations::m_checkFactorytest(const CameraParameters& params)
{
    /* Check factorytest */
    const char *newStrFactorytest = params.get("factorytest");
    bool factorytest = false;

    if (newStrFactorytest != NULL
        && !strcmp(newStrFactorytest, "true")) {
        factorytest = true;
    }

    CLOGD("factory test (%d)  !!!", factorytest);
    setMode(CONFIGURATION_FACTORY_TEST_MODE, factorytest);

    return NO_ERROR;
}
#endif

status_t ExynosCameraConfigurations::m_checkOperationMode(__unused const CameraParameters& params)
{
#ifdef SAMSUNG_TN_FEATURE
    /* Check intelligent mode */
    const char *newOperationMode = params.get("operation_mode");
    int operationMode = OPERATION_MODE_NONE;

    if (newOperationMode != NULL) {
        if (strcmp(newOperationMode, "smartstay") == 0) {
            operationMode = OPERATION_MODE_SMART_STAY;
#ifdef SAMSUNG_FACTORY_DRAM_TEST
        } else if (strcmp(newOperationMode, "dramtest") == 0) {
            operationMode = OPERATION_MODE_DRAM_TEST;
#endif
#ifdef SAMSUNG_FACTORY_LN_TEST
        } else if (strcmp(newOperationMode, "ln2test") == 0) {
            operationMode = OPERATION_MODE_LN2_TEST;
        } else if (strcmp(newOperationMode, "ln4test") == 0) {
            operationMode = OPERATION_MODE_LN4_TEST;
#endif
#ifdef SAMSUNG_FACTORY_SSM_TEST
        } else if (strcmp(newOperationMode, "ssmtest") == 0) {
            operationMode = OPERATION_MODE_SSM_TEST;
#endif
        } else if (strcmp(newOperationMode, "crossapp") == 0) {
            setSamsungCamera(true);
            CLOGD("Crossapp Camera. Set Samsung Camera Enabled !!!");
        }
    }

    CLOGD("operation_mode : %d", operationMode);
    setModeValue(CONFIGURATION_OPERATION_MODE, operationMode);
#endif

    return NO_ERROR;
}

#ifdef USE_DUAL_CAMERA
status_t ExynosCameraConfigurations::m_checkDualMode(const CameraParameters& params)
{
    /* dual_mode */
    bool flagDualMode = false;
    const char *newDualMode = params.get("dual_mode");

    if ((newDualMode != NULL) && ( !strcmp(newDualMode, "true")))
        flagDualMode = true;

    CLOGD("newDualMode %s", newDualMode);
    setMode(CONFIGURATION_PIP_MODE, flagDualMode);

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_SW_VDIS
status_t ExynosCameraConfigurations::m_checkSWVdisMode(const CameraParameters& params)
{
    const char *newSwVdis = params.get("sw-vdis");
    bool flagSwVdis = false;

    CLOGD(" newSwVdis %s", newSwVdis);

    if ((newSwVdis != NULL) && ( !strcmp(newSwVdis, "true"))) {
        flagSwVdis = true;
    }

    setMode(CONFIGURATION_SWVDIS_MODE, flagSwVdis);

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY
status_t ExynosCameraConfigurations::m_checkVideoBeautyMode(const CameraParameters& params)
{
    const char *newVideoBeauty = params.get("video-beautyface");
    bool flagVideoBeauty = false;

    CLOGD(" newVideoBeauty %s", newVideoBeauty);

    if ((newVideoBeauty != NULL) && ( !strcmp(newVideoBeauty, "true"))) {
        flagVideoBeauty = true;
    }

    setMode(CONFIGURATION_VIDEO_BEAUTY_MODE, flagVideoBeauty);

    return NO_ERROR;
}
#endif

#ifdef SAMSUNG_HIFI_VIDEO
status_t ExynosCameraConfigurations::m_checkHiFiVideoMode(const CameraParameters& params)
{
    int newShootingMode = params.getInt("shootingmode");
    bool flagHiFiVideo = false;

    CLOGD("[HIFIVIDEO] getSamsungCamera %d newShootingMode %d getCameraId %d", getSamsungCamera(), newShootingMode, getCameraId());

    if (getSamsungCamera() == true
        && newShootingMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_VIDEO
        && getCameraId() == CAMERA_ID_FRONT) {
        flagHiFiVideo = true;
    }

    setMode(CONFIGURATION_HIFIVIDEO_MODE, flagHiFiVideo);

    int operationMode = HIFIVIDEO_OPMODE_NONE;

    if (flagHiFiVideo == true) {
        operationMode = HIFIVIDEO_OPMODE_HIFIONLY_PREVIEW;
    }

    setModeValue(CONFIGURATION_HIFIVIDEO_OPMODE, operationMode);

    return NO_ERROR;
}
#endif

status_t ExynosCameraConfigurations::m_checkExtSensorMode()
{
#ifdef SAMSUNG_TN_FEATURE
    if (getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_LIVE_FOCUS) {
        setModeValue(CONFIGURATION_EXTEND_SENSOR_MODE, EXTEND_SENSOR_MODE_LIVE_FOCUS);
    }
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    else if (getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_DRAM_TEST) {
        setModeValue(CONFIGURATION_EXTEND_SENSOR_MODE, EXTEND_SENSOR_MODE_DRAM_TEST);
    }
#endif
#ifdef SAMSUNG_SSM
    else if (getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION
#ifdef SAMSUNG_FACTORY_SSM_TEST
        || getModeValue(CONFIGURATION_OPERATION_MODE) == OPERATION_MODE_SSM_TEST
#endif
    ) {
#ifdef SAMSUNG_SSM_FRC
        if (getModeValue(CONFIGURATION_SSM_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_SHOT_MODE_MULTI_FRC
            || getModeValue(CONFIGURATION_SSM_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_SHOT_MODE_SINGLE_FRC) {
            setModeValue(CONFIGURATION_EXTEND_SENSOR_MODE, EXTEND_SENSOR_MODE_DUAL_FPS_480);
        } else
#endif
        {
            setModeValue(CONFIGURATION_EXTEND_SENSOR_MODE, EXTEND_SENSOR_MODE_DUAL_FPS_960);
        }
    }
#endif
    else
#endif
    {
        setModeValue(CONFIGURATION_EXTEND_SENSOR_MODE, EXTEND_SENSOR_MODE_NONE);
    }

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_checkVtMode(const CameraParameters& params)
{
    int newVTMode = params.getInt("vtmode");
    int curVTMode = -1;

    CLOGD("newVTMode (%d)", newVTMode);
    /*
     * VT mode
     *   1: 3G vtmode (176x144, Fixed 7fps)
     *   2: LTE or WIFI vtmode (640x480, Fixed 15fps)
     *   3: Reserved : Smart Stay
     *   4: CHINA vtmode (1280x720, Fixed 30fps)
     */
    if (newVTMode == 3 || (newVTMode < 0 || newVTMode > 4)) {
        newVTMode = 0;
    }

    curVTMode = getModeValue(CONFIGURATION_VT_MODE);

    if (curVTMode != newVTMode) {
        setModeValue(CONFIGURATION_VT_MODE, newVTMode);
    }

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_checkShootingMode(const CameraParameters& params)
{
    int newShootingMode = params.getInt("shootingmode");

    CLOGD("shootingmode %d", newShootingMode);

#ifdef SAMSUNG_TN_FEATURE
    /* HACK : ignore shooting mode for secure camera */
    if (m_cameraId == CAMERA_ID_SECURE) {
        newShootingMode = SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SINGLE;
        CLOGD("shootingmode changed internally (%d)", newShootingMode);
    }
#endif

#if defined(SAMSUNG_VIDEO_BEAUTY)
    if (getModeValue(CONFIGURATION_SHOT_MODE) == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SELFIE_FOCUS
        && newShootingMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_VIDEO) {
        setBokehRecordingHint(1);
    } else {
        setBokehRecordingHint(0);
    }
#endif /* SAMSUNG_VIDEO_BEAUTY */

#ifdef SAMSUNG_HYPERLAPSE
    if (newShootingMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_HYPER_MOTION) {
        setMode(CONFIGURATION_HYPERLAPSE_MODE, true);
    } else {
        setMode(CONFIGURATION_HYPERLAPSE_MODE, false);
    }
#endif

#ifdef SAMSUNG_SSM
    if (newShootingMode == SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SUPER_SLOW_MOTION) {
        setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_READY);
    } else {
        setModeValue(CONFIGURATION_SSM_STATE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_STATE_UNDEFINED);
    }
#endif

#ifdef SAMSUNG_TN_FEATURE
    if (newShootingMode >= SAMSUNG_ANDROID_CONTROL_SHOOTING_MODE_SINGLE) {
        setModeValue(CONFIGURATION_SHOT_MODE, newShootingMode);
    }
#endif

    return NO_ERROR;
}

#ifdef SAMSUNG_SSM
status_t ExynosCameraConfigurations::m_checkSSMShotMode(const CameraParameters& params)
{
    int newSsmShotMode = params.getInt("ssm_shot_mode");

    CLOGD("ssm_shot_mode %d", newSsmShotMode);

    if (newSsmShotMode < 0) {
        setModeValue(CONFIGURATION_SSM_SHOT_MODE, SAMSUNG_ANDROID_CONTROL_SUPER_SLOW_MOTION_SHOT_MODE_MULTI);
    } else {
        setModeValue(CONFIGURATION_SSM_SHOT_MODE, newSsmShotMode);
    }

    return NO_ERROR;
}
#endif

status_t ExynosCameraConfigurations::m_checkRecordingDrMode(const CameraParameters& params)
{
    const char *newRecordingDrMmode = params.get("recording_dr_mode");
    bool flagHdr10 = false;

    CLOGD("newRecordingDrMmode %s", newRecordingDrMmode);

#ifdef SAMSUNG_HDR10_RECORDING
    if ((newRecordingDrMmode != NULL) && (!strcmp(newRecordingDrMmode, "hdr10"))) {
        flagHdr10 = true;
    }
#endif

    setMode(CONFIGURATION_HDR_RECORDING_MODE, flagHdr10);

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::reInit(void)
{
    resetSize(CONFIGURATION_MIN_YUV_SIZE);
    resetSize(CONFIGURATION_MAX_YUV_SIZE);

    m_vendorReInit();

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_vendorReInit(void)
{
    resetYuvStallPort();
    resetSize(CONFIGURATION_THUMBNAIL_CB_SIZE);


    setMode(CONFIGURATION_RECORDING_MODE, false);
    setMode(CONFIGURATION_DEPTH_MAP_MODE, false);

#ifdef SAMSUNG_SSM
    setModeValue(CONFIGURATION_SSM_TRIGGER, (int)false);
#endif

#ifdef SAMSUNG_TN_FEATURE
    setMode(CONFIGURATION_PRO_MODE, false);
    setModeValue(CONFIGURATION_EXIF_CAPTURE_STEP_COUNT, 0);
    setModeValue(CONFIGURATION_PREV_TRANSIENT_ACTION_MODE, \
                (int)SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE);
    setModeValue(CONFIGURATION_TRANSIENT_ACTION_MODE, \
                (int)SAMSUNG_ANDROID_CONTROL_TRANSIENT_ACTION_NONE);
    setModeValue(CONFIGURATION_FLICKER_DATA, 0);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    setModeValue(CONFIGURATION_DUAL_HOLD_FALLBACK_STATE, 0);
    if (m_scenario == SCENARIO_DUAL_REAR_PORTRAIT) {
        resetStaticFallbackState();
    }
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    memset(&m_fusionCaptureInfo, 0x00, sizeof(uFusionCaptureState));
    memset(&m_syncFrameStateInfo, 0x00, sizeof(uFusionCaptureState));
#endif

    m_appliedZoomRatio = -1.0f;
    m_temperature = 0xFFFF;

    return NO_ERROR;
}

void ExynosCameraConfigurations::resetYuvStallPort(void)
{
    m_modeValue[CONFIGURATION_YUV_STALL_PORT] = -1;
}

void ExynosCameraConfigurations::setCaptureExposureTime(uint64_t exposureTime)
{
    m_exposureTimeCapture = exposureTime;
}

uint64_t ExynosCameraConfigurations::getCaptureExposureTime(void)
{
    return m_exposureTimeCapture;
}

uint64_t ExynosCameraConfigurations::getLongExposureTime(void)
{
#ifdef SAMSUNG_TN_FEATURE
#ifndef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture >= CAMERA_SENSOR_EXPOSURE_TIME_MAX) {
        return CAMERA_SENSOR_EXPOSURE_TIME_MAX;
    } else
#endif
    {
        int32_t longExposureShotCount = getLongExposureShotCount();
        if (longExposureShotCount != 0) {
            return (m_exposureTimeCapture / longExposureShotCount);
        } else {
            CLOGW("LongExposureShotCount(%d) is zero", longExposureShotCount);
            return m_exposureTimeCapture;
        }
    }
#else
    return 0;
#endif
}

int32_t ExynosCameraConfigurations::getLongExposureShotCount(void)
{
#ifdef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture <= CAMERA_SENSOR_EXPOSURE_TIME_MAX)
#endif
    {
        return 1;
    }

#ifdef SAMSUNG_TN_FEATURE
    int32_t count = 0;
    bool getResult;
    if (m_exposureTimeCapture % CAMERA_SENSOR_EXPOSURE_TIME_MAX) {
        count = 2;
        getResult = false;
        while (!getResult) {
            if (m_exposureTimeCapture % count) {
                count++;
                continue;
            }
            if (CAMERA_SENSOR_EXPOSURE_TIME_MAX < (m_exposureTimeCapture / count)) {
                count++;
                continue;
            }
            getResult = true;
        }
        return count;
    } else {
        return m_exposureTimeCapture / CAMERA_SENSOR_EXPOSURE_TIME_MAX;
    }
#else
    return 0;
#endif
}

void ExynosCameraConfigurations::setExposureTime(int64_t exposureTime)
{
    m_exposureTime = exposureTime;
}

int64_t ExynosCameraConfigurations::getExposureTime(void)
{
    return m_exposureTime;
}

void ExynosCameraConfigurations::setGain(int gain)
{
    m_gain = gain;
}

int ExynosCameraConfigurations::getGain(void)
{
    return m_gain;
}

void ExynosCameraConfigurations::setLedPulseWidth(int64_t ledPulseWidth)
{
    m_ledPulseWidth = ledPulseWidth;
}

int64_t ExynosCameraConfigurations::getLedPulseWidth(void)
{
    return m_ledPulseWidth;
}

void ExynosCameraConfigurations::setLedPulseDelay(int64_t ledPulseDelay)
{
    m_ledPulseDelay = ledPulseDelay;
}

int64_t ExynosCameraConfigurations::getLedPulseDelay(void)
{
    return m_ledPulseDelay;
}

void ExynosCameraConfigurations::setLedCurrent(int ledCurrent)
{
    m_ledCurrent = ledCurrent;
}

int ExynosCameraConfigurations::getLedCurrent(void)
{
    return m_ledCurrent;
}

void ExynosCameraConfigurations::setLedMaxTime(int ledMaxTime)
{
    if (m_isFactoryBin) {
        m_ledMaxTime = 0L;
        return;
    }

    m_ledMaxTime = ledMaxTime;
}

int ExynosCameraConfigurations::getLedMaxTime(void)
{
    return m_ledMaxTime;
}

#ifdef SAMSUNG_TN_FEATURE
static int LIGHT_CONDITION_MAP[][2] =
{
    { LLS_LEVEL_ZSL,                            SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_HIGH },
    { LLS_LEVEL_LOW,                            SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_FLASH,                          SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_FLASH },
    { LLS_LEVEL_SIS,                            SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_SIS_LOW },
    { LLS_LEVEL_ZSL_LIKE,                       SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_ZSL_LIKE1,                      SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_SHARPEN_DR,                     SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_SHARPEN_IMA,                    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_STK,                            SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_LLS_FLASH,                      SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_FLASH },
    { LLS_LEVEL_MULTI_MERGE_2,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_3,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_4,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_5,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_PICK_2,                   SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_PICK_3,                   SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_PICK_4,                   SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_PICK_5,                   SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_2,        SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_3,        SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_4,        SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_5,        SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_2,    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_3,    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_4,    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MERGE_INDICATOR_LOW_5,    SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_MFHDR_2,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MFHDR_3,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MFHDR_4,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MFHDR_5,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_LOW_2,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_LOW_3,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_LOW_4,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_LOW_5,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_ZSL_2,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_ZSL_3,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_ZSL_4,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_MERGE_ZSL_5,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_LLHDR_2,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_LLHDR_3,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_LLHDR_4,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_LLHDR_5,                  SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LOW },
    { LLS_LEVEL_MULTI_LLHDR_LOW_2,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_LLHDR_LOW_3,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_LLHDR_LOW_4,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
    { LLS_LEVEL_MULTI_LLHDR_LOW_5,              SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_LLS_LOW },
};
#endif

int ExynosCameraConfigurations::getLightCondition(void)
{
    int lightCondition = -1;

#ifdef SAMSUNG_TN_FEATURE
#ifdef LLS_CAPTURE
    int LLS = getModeValue(CONFIGURATION_LLS_VALUE);
#else
    int LLS = LLS_LEVEL_ZSL;
#endif
    int lightConditionTableSize = sizeof(LIGHT_CONDITION_MAP) / (sizeof(int) * 2);
    int cnt = 0;

    lightCondition = SAMSUNG_ANDROID_CONTROL_LIGHT_CONDITION_HIGH;

    for (cnt = 0 ; cnt < lightConditionTableSize ; cnt++) {
        if (LLS == LIGHT_CONDITION_MAP[cnt][0]) {
            lightCondition = LIGHT_CONDITION_MAP[cnt][1];
            break;
        }
    }
#endif /* SAMSUNG_TN_FEATURE */

    return lightCondition;
}

void ExynosCameraConfigurations::setYuvBufferStatus(bool flag)
{
    m_yuvBufferStat = flag;
}

bool ExynosCameraConfigurations::getYuvBufferStatus(void)
{
    return m_yuvBufferStat;
}

#ifdef DEBUG_RAWDUMP
bool ExynosCameraConfigurations::checkBayerDumpEnable(void)
{
#ifndef RAWDUMP_CAPTURE
    char enableRawDump[PROPERTY_VALUE_MAX];
    property_get("ro.debug.rawdump", enableRawDump, "0");

    if (strcmp(enableRawDump, "1") == 0) {
        /*CLOGD("checkBayerDumpEnable : 1");*/
        return true;
    } else {
        /*CLOGD("checkBayerDumpEnable : 0");*/
        return false;
    }
#endif
    return true;
}
#endif  /* DEBUG_RAWDUMP */

#ifdef SAMSUNG_SW_VDIS
void ExynosCameraConfigurations::setSWVdisPreviewOffset(int left, int top)
{
    m_previewOffsetLeft = left;
    m_previewOffsetTop = top;
}

void ExynosCameraConfigurations::getSWVdisPreviewOffset(int *left, int *top)
{
    *left = m_previewOffsetLeft;
    *top = m_previewOffsetTop;
}
#endif/* SAMSUNG_SW_VDIS */

#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_DUAL_PORTRAIT_BEAUTY)
void ExynosCameraConfigurations::setBeautyRetouchLevel(int beautyRetouchLevel)
{
    m_beautyRetouchLevel = beautyRetouchLevel;
}

int ExynosCameraConfigurations::getBeautyRetouchLevel(void)
{
    return m_beautyRetouchLevel;
}
#endif/* SAMSUNG_VIDEO_BEAUTY || SAMSUNG_DUAL_PORTRAIT_BEAUTY */

#if defined(SAMSUNG_VIDEO_BEAUTY)
void ExynosCameraConfigurations::setBokehRecordingHint(int bokehRecordingHint)
{
    m_bokehRecordingHint = bokehRecordingHint;
}

int ExynosCameraConfigurations::getBokehRecordingHint(void)
{
    return m_bokehRecordingHint;
}
#endif /* SAMSUNG_VIDEO_BEAUTY */

#ifdef SAMSUNG_BD
void ExynosCameraConfigurations::setBlurInfo(unsigned char *data, unsigned int size)
{
#ifdef SAMSUNG_UNI_API
    uni_appMarker_add(BD_EXIF_TAG, (char *)data, size, APP_MARKER_5);
#endif
}
#endif

#ifdef SAMSUNG_UTC_TS
void ExynosCameraConfigurations::setUTCInfo()
{
    struct timeval rawtime;
    struct tm timeinfo;
    gettimeofday(&rawtime, NULL);

    gmtime_r((time_t *)&rawtime.tv_sec, &timeinfo);
    struct utc_ts m_utc;
    strftime(m_utc.utc_ts_data, 20, "%Y:%m:%d %H:%M:%S", &timeinfo);
    /* UTC Time Info Tag : 0x1001 */
    char utc_ts_tag[3];
    utc_ts_tag[0] = 0x10;
    utc_ts_tag[1] = 0x01;
    utc_ts_tag[2] = '\0';

#ifdef SAMSUNG_UNI_API
    uni_appMarker_add(utc_ts_tag, m_utc.utc_ts_data, 20, APP_MARKER_5);
#endif
}
#endif

#ifdef SAMSUNG_OT
int ExynosCameraConfigurations::getObjectTrackingAreas(int *validFocusArea, ExynosRect2 *areas, int *weights)
{
    *validFocusArea = 1;
    *areas = m_objectTrackingArea;
    *weights = m_objectTrackingWeight;

    return NO_ERROR;
}

void ExynosCameraConfigurations::setObjectTrackingAreas(int validFocusArea, ExynosRect2 area, int weight)
{
    m_objectTrackingArea = area;
    m_objectTrackingWeight = weight;
}

int ExynosCameraConfigurations::setObjectTrackingFocusData(UniPluginFocusData_t *focusData)
{
    if(focusData == NULL) {
        CLOGE(" NULL focusData is received!!");
        return false;
    }
    Mutex::Autolock l(m_OTfocusDataLock);

    memcpy(&m_OTfocusData, focusData, sizeof(UniPluginFocusData_t));

    return true;
}

int ExynosCameraConfigurations::getObjectTrackingFocusData(UniPluginFocusData_t *focusData)
{
    Mutex::Autolock l(m_OTfocusDataLock);

    memcpy(focusData, &m_OTfocusData, sizeof(UniPluginFocusData_t));

    return true;
}
#endif

#ifdef SAMSUNG_IDDQD
void ExynosCameraConfigurations::setIDDQDresult(bool isdetected)
{
    m_lensDirtyDetected = isdetected;
}

bool ExynosCameraConfigurations::getIDDQDresult(void)
{
    return m_lensDirtyDetected;
}
#endif



#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
void ExynosCameraConfigurations::setFusionParam(UniPluginDualCameraParam *cameraParam)
{
    m_fusionParam = cameraParam;

#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
#if 0   /* HACK : temp */
    if (m_dualParameters->getFlagForceSwitchingOnly(m_cameraId) == true)
        return;
#endif

    setModeValue(CONFIGURATION_DUAL_DISP_CAM_TYPE, cameraParam->baseCameraType);
    setModeValue(CONFIGURATION_DUAL_PREVIEW_SHIFT_X, cameraParam->previewImageShiftX);
    setModeValue(CONFIGURATION_DUAL_PREVIEW_SHIFT_Y, cameraParam->previewImageShiftY);

    if (cameraParam->switchTo == RECOMMEND_TO_WIDE || cameraParam->switchTo == RECOMMEND_TO_FALL_BACK) {
        setModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE, (int)UNI_PLUGIN_CAMERA_TYPE_WIDE);
    } else {
        setModeValue(CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE, (int)UNI_PLUGIN_CAMERA_TYPE_TELE);
    }
#endif
}

UniPluginDualCameraParam* ExynosCameraConfigurations::getFusionParam(void)
{
    return m_fusionParam;
}

int32_t ExynosCameraConfigurations::getDualSelectedCam(void)
{
    if (m_dualSelectedCam == -1) {
        if (getZoomRatio() >= DUAL_SWITCHING_SYNC_MODE_MAX_ZOOM_RATIO) {
            if (getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT))
                m_dualSelectedCam = UNI_PLUGIN_CAMERA_TYPE_WIDE;
            else
                m_dualSelectedCam = UNI_PLUGIN_CAMERA_TYPE_TELE;
        } else {
            m_dualSelectedCam = UNI_PLUGIN_CAMERA_TYPE_WIDE;
        }
    }

    return m_dualSelectedCam;
}

void ExynosCameraConfigurations::setDualSelectedCam(int32_t selectedCam)
{
    m_dualSelectedCam = selectedCam;
}

void ExynosCameraConfigurations::resetStaticFallbackState(void)
{
    setStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT, 0);
    setStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT, 0);
    setStaticValue(CONFIGURATION_DUAL_OP_MODE_FALLBACK, 0);
}

#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
void ExynosCameraConfigurations::checkFallbackCondition(const struct camera2_shot_ext *meta, bool flagTargetCam)
{
    bool result = false;
    int32_t lux = (int32_t)meta->shot.dm.aa.vendor_wideTeleConvEv;
    int32_t objectDistance = (int32_t)meta->shot.dm.aa.vendor_objectDistanceCm;

    if (getDynamicMode(DYNAMIC_DUAL_CAMERA_DISABLE) == true) {
        result = true;
    } else {
        if (lux <= DUAL_LOW_LIGHT_CONDITION_FORCE ||
                (objectDistance < DUAL_DISTANCE_CONDITION_FORCE && objectDistance > 0)) {
            result = true;
        } else if (lux >= DUAL_LOW_LIGHT_CONDITION_NORMAL &&
                objectDistance >= DUAL_DISTANCE_CONDITION_NORMAL) {
            result = false;
        } else {
            m_fallbackOffCount = 0;
            m_fallbackOnCount = 0;
            return;
        }
    }

    CLOGV("TargetCam(%d) Lux(%d), objectDistance(%d), Result(%d)", flagTargetCam, lux, objectDistance, result);

    if (flagTargetCam) {
        setStaticValue(CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT, result);
    } else {
        if (result == true && m_fallbackOnCount < FALLBACK_ON_DELAY) {
            m_fallbackOnCount++;
            m_fallbackOffCount = 0;
        } else if (result == false && m_fallbackOffCount < FALLBACK_OFF_DELAY) {
            m_fallbackOffCount++;
            m_fallbackOnCount = 0;
        } else {
            m_fallbackOffCount = 0;
            m_fallbackOnCount = 0;
            return;
        }

        if (m_fallbackOnCount >= FALLBACK_ON_DELAY) {
            setFallbackResult(true, lux, objectDistance);
        } else if (m_fallbackOffCount >= FALLBACK_OFF_DELAY) {
            setFallbackResult(false, lux, objectDistance);
        }
    }
}

void ExynosCameraConfigurations::setFallbackResult(bool enable, int32_t lux, int32_t objectDistance)
{
    int prevFallbackResult = getStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT);
    int recordingMode = getMode(CONFIGURATION_RECORDING_MODE);
    float viewZoomRatio = getZoomRatio();
    bool dualCameraDisable = false;

    if (recordingMode == true && viewZoomRatio >= DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
        dualCameraDisable = getDynamicMode(DYNAMIC_DUAL_CAMERA_DISABLE);
        if (dualCameraDisable == true) {
            enable = true;
        } else {
            return;
        }
    }

    if (enable != prevFallbackResult) {
        CLOGD("change fallback state(%d -> %d), dualCameraDisable(%d), lux(%d), objectDistance(%d)",
                prevFallbackResult, enable, dualCameraDisable, lux, objectDistance);
        setStaticValue(CONFIGURATION_DUAL_DISP_FALLBACK_RESULT, (int)enable);
     }
}
#endif /* SAMSUNG_DUAL_ZOOM_FALLBACK */
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
void ExynosCameraConfigurations::updateFusionFrameStateInfo(void)
{
    Mutex::Autolock l(m_syncFrameStateInfoLock);

    m_fusionCaptureInfo.bit.ae = m_syncFrameStateInfo.bit.ae;
    m_fusionCaptureInfo.bit.af = m_syncFrameStateInfo.bit.af;
    m_fusionCaptureInfo.bit.w_state = m_syncFrameStateInfo.bit.w_state;
    m_fusionCaptureInfo.bit.w_done = m_syncFrameStateInfo.bit.w_done;
    m_fusionCaptureInfo.bit.t_state = m_syncFrameStateInfo.bit.t_state;
    m_fusionCaptureInfo.bit.t_done = m_syncFrameStateInfo.bit.t_done;
    m_fusionCaptureInfo.bit.bDist = m_syncFrameStateInfo.bit.bDist;
    m_fusionCaptureInfo.bit.bLux = m_syncFrameStateInfo.bit.bLux;

    CLOGD("fusionCaptureInfo : 0x%x", m_fusionCaptureInfo);
#if 0
    CLOGD("fusionCaptureInfo.bit.af:%d", m_fusionCaptureInfo.bit.af);
    CLOGD("fusionCaptureInfo.bit.ae:%d", m_fusionCaptureInfo.bit.ae);
    CLOGD("fusionCaptureInfo.bit.w_done:%d", m_fusionCaptureInfo.bit.w_done);
    CLOGD("fusionCaptureInfo.bit.t_done:%d", m_fusionCaptureInfo.bit.t_done);
    CLOGD("fusionCaptureInfo.bit.bDist:%d", m_fusionCaptureInfo.bit.bDist);
    CLOGD("fusionCaptureInfo.bit.bLux:%d", m_fusionCaptureInfo.bit.bLux);
    CLOGD("fusionCaptureInfo.bit.w_state:%d", m_fusionCaptureInfo.bit.w_state);
    CLOGD("fusionCaptureInfo.bit.t_state:%d", m_fusionCaptureInfo.bit.t_state);
#endif
}

void *ExynosCameraConfigurations::getFusionFrameStateInfo(void)
{
    if (getDualOperationMode() == DUAL_OPERATION_MODE_SYNC
        && getZoomRatio() < DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
        return (void *)(&m_fusionCaptureInfo);
    } else {
        return NULL;
    }
}

void ExynosCameraConfigurations::checkFusionCaptureMode(void)
{
    enum DUAL_OPERATION_MODE dualOperationReprocessingMode = getDualOperationModeReprocessing();
    int isFusionCaptureReady = getModeValue(CONFIGURATION_FUSION_CAPTURE_READY);
    int multiCaptureMode = getModeValue(CONFIGURATION_MULTI_CAPTURE_MODE);
    int llsValue = getModeValue(CONFIGURATION_LD_CAPTURE_LLS_VALUE);

    setMode(CONFIGURATION_FUSION_CAPTURE_MODE, (int)false);

    if (getSamsungCamera() == false
        || getCameraId() == CAMERA_ID_FRONT
        || getMode(CONFIGURATION_RECORDING_MODE) == true
        || getDualReprocessingMode() != DUAL_REPROCESSING_MODE_SW) {
        CLOGD("[Fusion] getFusionCaptureMode %d",
            getMode(CONFIGURATION_FUSION_CAPTURE_MODE));
        return;
    }

    CLOGV("[Fusion] dualOperationMode(%d) dualOperationReprocessingMode(%d) isFusionCaptureReady(%d)",
           m_dualOperationMode, dualOperationReprocessingMode, isFusionCaptureReady);

    if (dualOperationReprocessingMode == DUAL_OPERATION_MODE_SYNC) {
        if (isFusionCaptureReady == false
#ifdef SAMSUNG_MFHDR_CAPTURE
            || (llsValue >= LLS_LEVEL_MULTI_MFHDR_2 && llsValue <= LLS_LEVEL_MULTI_MFHDR_5)
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
            || (llsValue >= LLS_LEVEL_MULTI_LLHDR_LOW_2 && llsValue <= LLS_LEVEL_MULTI_LLHDR_5) 
#endif
            || multiCaptureMode == MULTI_CAPTURE_MODE_BURST
            || multiCaptureMode == MULTI_CAPTURE_MODE_AGIF) {
            CLOGV("[Fusion] SYNC, set to MASTER : isFusionCaptureReady(%d) getMultiCaptureMode(%d)",
                    isFusionCaptureReady, multiCaptureMode);
            setDualOperationModeReprocessing(DUAL_OPERATION_MODE_MASTER);
        } else {
            CLOGD("[Fusion] SYNC : FUSION");
            setMode(CONFIGURATION_FUSION_CAPTURE_MODE, (int)true);
        }
    }

    if (getDualOperationMode() == DUAL_OPERATION_MODE_SYNC
        && getZoomRatio() < DUAL_CAPTURE_SYNC_MODE_MAX_ZOOM_RATIO) {
        /* For debugInfo */
        updateFusionFrameStateInfo();
    }

    return;
}

void ExynosCameraConfigurations::checkFusionCaptureCondition(struct camera2_shot_ext *wideMeta, struct camera2_shot_ext *teleMeta)
{
    int32_t dualLux;
    int32_t dualFocusDistance;
    uint32_t aeFlag;
    uint32_t afFlag;

    dualLux = (int32_t)wideMeta->shot.dm.aa.vendor_wideTeleConvEv;
    dualFocusDistance = (int32_t)teleMeta->shot.dm.aa.vendor_objectDistanceCm;
    aeFlag = wideMeta->shot.dm.aa.vendor_fusionCaptureAeInfo;
    afFlag = wideMeta->shot.dm.aa.vendor_fusionCaptureAfInfo;

    if (dualLux >= DUAL_LOW_LIGHT_CONDITION_NORMAL
        && dualFocusDistance >= DUAL_DISTANCE_CONDITION_NORMAL
        && aeFlag == true
        && (teleMeta->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED
           || teleMeta->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED)) {
        setModeValue(CONFIGURATION_FUSION_CAPTURE_READY, (int)true);
    } else {
        setModeValue(CONFIGURATION_FUSION_CAPTURE_READY, (int)false);
    }

    CLOGV("[Fusion] Wide Lux(%d) dualFocusDistance(%f) afState(%d) aeFlag(%d)",
            wideMeta->shot.dm.aa.vendor_wideTeleConvEv,
            wideMeta->shot.dm.aa.vendor_objectDistanceCm,
            wideMeta->shot.dm.aa.afState,
            wideMeta->shot.dm.aa.vendor_fusionCaptureAeInfo);

    CLOGV("[Fusion] Tele Lux(%d) dualFocusDistance(%f) afState(%d) aeFlag(%d)",
            teleMeta->shot.dm.aa.vendor_wideTeleConvEv,
            teleMeta->shot.dm.aa.vendor_objectDistanceCm,
            teleMeta->shot.dm.aa.afState,
            teleMeta->shot.dm.aa.vendor_fusionCaptureAeInfo);

    /* update state for debuginfo */
    {
        Mutex::Autolock l(m_syncFrameStateInfoLock);

        m_syncFrameStateInfo.value = 0;
        m_syncFrameStateInfo.bit.ae = (aeFlag == true);
        m_syncFrameStateInfo.bit.af = (afFlag == true);
        m_syncFrameStateInfo.bit.w_state = wideMeta->shot.dm.aa.afState;
        m_syncFrameStateInfo.bit.w_done = ((wideMeta->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED)
                                        || (wideMeta->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED));
        m_syncFrameStateInfo.bit.t_state = teleMeta->shot.dm.aa.afState;
        m_syncFrameStateInfo.bit.t_done = ((teleMeta->shot.dm.aa.afState == AA_AFSTATE_PASSIVE_FOCUSED)
                                        || (teleMeta->shot.dm.aa.afState == AA_AFSTATE_FOCUSED_LOCKED));

        m_syncFrameStateInfo.bit.bDist = (dualFocusDistance >= DUAL_DISTANCE_CONDITION_NORMAL);
        m_syncFrameStateInfo.bit.bLux = (dualLux >= DUAL_LOW_LIGHT_CONDITION_NORMAL);
    }

#ifdef DEBUG_IQ_CAPTURE_MODE
    int debugCaptureMode = getModeValue(CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE);

    if (debugCaptureMode == DEBUG_IQ_CAPTURE_MODE_ALWAYS_FUSION_ENABLE) {
        setModeValue(CONFIGURATION_FUSION_CAPTURE_READY, (int)true);
    } else if (debugCaptureMode == DEBUG_IQ_CAPTURE_MODE_ALWAYS_FUSION_DISABLE) {
        setModeValue(CONFIGURATION_FUSION_CAPTURE_READY, (int)false);
    }
#endif
}
#endif

#ifdef SAMSUNG_SSM
void ExynosCameraConfigurations::getSSMRegion(ExynosRect2 *region)
{
    *region = m_SSMRegion;
}

void ExynosCameraConfigurations::setSSMRegion(ExynosRect2 region)
{
    m_SSMRegion = region;
}
#endif

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
void ExynosCameraConfigurations::setBokehBlurStrength(int bokehBlurStrength)
{
    m_bokehBlurStrength = bokehBlurStrength;
}

int ExynosCameraConfigurations::getBokehBlurStrength(void)
{
    return m_bokehBlurStrength;
}

void ExynosCameraConfigurations::setZoomInOutPhoto(int zoomInOutPhoto)
{
    m_zoomInOutPhoto = zoomInOutPhoto;
}

int ExynosCameraConfigurations::getZoomInOutPhoto(void)
{
    return m_zoomInOutPhoto;
}

void ExynosCameraConfigurations::setBokehPreviewState(int bokehPreviewStatus)
{
    m_bokehPreviewState = bokehPreviewStatus;
}

int ExynosCameraConfigurations::getBokehPreviewState(void)
{
    return m_bokehPreviewState;
}

void ExynosCameraConfigurations::setBokehCaptureState(int bokehCaptureStatus)
{
    m_bokehCaptureState = bokehCaptureStatus;
}

int ExynosCameraConfigurations::getBokehCaptureState(void)
{
    return m_bokehCaptureState;
}

void ExynosCameraConfigurations::setBokehProcessResult(int bokehProcessResult)
{
    m_bokehProcessResult = bokehProcessResult;
}

int ExynosCameraConfigurations::getBokehProcessResult(void)
{
    return m_bokehProcessResult;
}

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
void ExynosCameraConfigurations::setDualCaptureFlag(int dualCaptureFlag)
{
    m_dualCaptureFlag = dualCaptureFlag;
}

int ExynosCameraConfigurations::getDualCaptureFlag(void)
{
    return m_dualCaptureFlag;
}
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

#ifdef USE_CSC_FEATURE
bool ExynosCameraConfigurations::m_isLatinOpenCSC()
{
    char sales_code[PROPERTY_VALUE_MAX] = {0};

    property_get("ro.csc.sales_code", sales_code, "");
    if (strstr(sales_code,"TFG")
        || strstr(sales_code,"TPA")
        || strstr(sales_code,"TTT")
        || strstr(sales_code,"JDI")
        || strstr(sales_code,"PCI")) {
        return true;
    } else {
        return false;
    }
}

int ExynosCameraConfigurations::m_getAntiBandingFromLatinMCC()
{
    char value[PROPERTY_VALUE_MAX];
    char country_value[10];
    int antibanding = 60;

    memset(value, 0x00, sizeof(value));
    memset(country_value, 0x00, sizeof(country_value));

    if (!property_get("gsm.operator.numeric", value,"")) {
        return antibanding;
    }

    memcpy(country_value, value, 3);

    /** MCC Info. Jamaica : 338 / Argentina : 722 / Chile : 730 / Paraguay : 744 / Uruguay : 748  **/
    if (strstr(country_value,"338")
        || strstr(country_value,"722")
        || strstr(country_value,"730")
        || strstr(country_value,"744")
        || strstr(country_value,"748")) {
        antibanding = 50;
    } else {
        antibanding = 60;
    }

    return antibanding;
}

int ExynosCameraConfigurations::getAntibanding()
{
    enum aa_ae_antibanding_mode newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;

    if (m_isLatinOpenCSC()) {
        if (m_getAntiBandingFromLatinMCC() == 60) {
            newAntibanding = AA_AE_ANTIBANDING_AUTO_60HZ;
        } else {
            newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;
        }
    } else {
        char *CSCstr = NULL;
        CSCstr = (char *)SecNativeFeature::getInstance()->getString("CscFeature_Camera_CameraFlicker");
        if (CSCstr == NULL|| strlen(CSCstr) == 0) {
            newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;
        } else {
            if (!strcmp(CSCstr, "50hz"))
                newAntibanding = AA_AE_ANTIBANDING_AUTO_50HZ;
            else if (!strcmp(CSCstr, "60hz"))
                newAntibanding = AA_AE_ANTIBANDING_AUTO_60HZ;
            else if (!strcmp(CSCstr, "auto"))
                newAntibanding = AA_AE_ANTIBANDING_AUTO;
            else if (!strcmp(CSCstr, "off"))
                newAntibanding = AA_AE_ANTIBANDING_OFF;
        }
    }

    return newAntibanding;
}
#else
int ExynosCameraConfigurations::getAntibanding()
{
    return AA_AE_ANTIBANDING_AUTO;
}
#endif

void ExynosCameraConfigurations::getZoomRect(ExynosRect *zoomRect)
{
    zoomRect->x = m_metaParameters.m_zoomRect.x;
    zoomRect->y = m_metaParameters.m_zoomRect.y;
    zoomRect->w = m_metaParameters.m_zoomRect.w;
    zoomRect->h = m_metaParameters.m_zoomRect.h;
}

void ExynosCameraConfigurations::setAppliedZoomRatio(float zoomRatio)
{
    m_appliedZoomRatio = zoomRatio;
}

float ExynosCameraConfigurations::getAppliedZoomRatio(void)
{
    return m_appliedZoomRatio;
}

void ExynosCameraConfigurations::setRepeatingRequestHint(int repeatingRequestHint)
{
    m_repeatingRequestHint = repeatingRequestHint;
}

int ExynosCameraConfigurations::getRepeatingRequestHint(void)
{
    return m_repeatingRequestHint;
}

#ifdef SAMSUNG_HLV
bool ExynosCameraConfigurations::getHLVEnable(bool recordingEnabled)
{
    bool enable = false;
    int curVideoW = 0, curVideoH = 0;
    uint32_t curMinFps = 0, curMaxFps = 0;

    getSize(CONFIGURATION_VIDEO_SIZE, (uint32_t *)&curVideoW, (uint32_t *)&curVideoH);
    getPreviewFpsRange(&curMinFps, &curMaxFps);

    if (curVideoW <= 1920 && curVideoH <= 1080
        && curMinFps <= 60 && curMaxFps <= 60
        && getSamsungCamera()
#ifdef SAMSUNG_HYPERLAPSE
        && getMode(CONFIGURATION_HYPERLAPSE_MODE) == false
#endif
#ifdef SAMSUNG_SW_VDIS
        && getMode(CONFIGURATION_SWVDIS_MODE) == false
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
        && getMode(CONFIGURATION_VIDEO_BEAUTY_MODE) == false
#endif
#ifdef SAMSUNG_HIFI_VIDEO
        && getMode(CONFIGURATION_HIFIVIDEO_MODE) == false
#endif
        && recordingEnabled) {
        enable = true;
    } else {
        enable = false;
    }

    return enable;
}
#endif

void ExynosCameraConfigurations::setTemperature(int temperature)
{
    m_temperature = temperature;
}

int ExynosCameraConfigurations::getTemperature(void)
{
    return m_temperature;
}

#ifdef SAMSUNG_TN_FEATURE
int ExynosCameraConfigurations::getProductColorInfo(void)
{
    return m_productColorInfo;
}
#endif

status_t ExynosCameraConfigurations::getExifMetaData(struct camera2_shot *shot)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&shot->dm, &m_copyExifShot.dm, sizeof(struct camera2_dm));
    memcpy(&shot->udm, &m_copyExifShot.udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::setExifMetaData(struct camera2_shot *shot)
{
    if (shot == NULL) {
        CLOGE(" buffer is NULL");
        return BAD_VALUE;
    }

    memcpy(&m_copyExifShot.dm, &shot->dm, sizeof(struct camera2_dm));
    memcpy(&m_copyExifShot.udm, &shot->udm, sizeof(struct camera2_udm));

    return NO_ERROR;
}
}; /* namespace android */
