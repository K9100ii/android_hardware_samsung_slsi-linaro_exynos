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
    m_useFastenAeStable = false;

    m_isFactoryBin = false;

    m_bokehBlurStrength = 0;

    m_repeatingRequestHint = 0;
    m_temperature = 0xFFFF;
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
    case CONFIGURATION_DVFS_LOCK_MODE:
    case CONFIGURATION_MANUAL_AE_CONTROL_MODE:
    case CONFIGURATION_HIFI_LLS_MODE:
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
        CLOGD("YSUM Recording %s", enable == true ? "ON" : "OFF");
        break;
#ifdef LLS_CAPTURE
    case CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE:
#endif
        m_mode[type] = enable;
        break;
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
    case CONFIGURATION_DVFS_LOCK_MODE:
    case CONFIGURATION_MANUAL_AE_CONTROL_MODE:
    case CONFIGURATION_HIFI_LLS_MODE:
#ifdef SUPPORT_DEPTH_MAP
    case CONFIGURATION_DEPTH_MAP_MODE:
#endif
        modeEnable = m_mode[type];
        break;
    case CONFIGURATION_GMV_MODE:
        modeEnable = m_getGmvMode();
        break;
#ifdef LLS_CAPTURE
    case CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE:
#ifdef SET_LLS_CAPTURE_SETFILE
    case CONFIGURATION_LLS_CAPTURE_MODE:
#endif
#endif
        modeEnable = m_mode[type];
        break;
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
    default:
        break;
    }

    return modeEnable;
}

status_t ExynosCameraConfigurations::setStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type, __unused int value)
{
    status_t ret = NO_ERROR;

    switch(type) {
    default:
        break;
    }
    return ret;
}

int ExynosCameraConfigurations::getStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type) const
{
    int value = -1;

    switch(type) {
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
        if (getCameraId() == CAMERA_ID_FRONT) {
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
    case CONFIGURATION_VT_MODE:
    case CONFIGURATION_YUV_SIZE_RATIO_ID:
    case CONFIGURATION_FLIP_HORIZONTAL:
    case CONFIGURATION_FLIP_VERTICAL:
    case CONFIGURATION_RECORDING_FPS:
    case CONFIGURATION_BURSTSHOT_FPS:
    case CONFIGURATION_BURSTSHOT_FPS_TARGET:
        if (value < 0) {
            CLOGE("Invalid mode(%d) value(%d)!", (int)type, value);
            return BAD_VALUE;
        }
    case CONFIGURATION_EXTEND_SENSOR_MODE:
    case CONFIGURATION_MARKING_EXIF_FLASH:
        m_modeValue[type] = value;
        break;
    case CONFIGURATION_YUV_STALL_PORT:
        if (value < 0) {
            m_modeValue[type] = ExynosCameraConfigurations::YUV_2;
            break;
        }
    case CONFIGURATION_YUV_STALL_PORT_USAGE:
#ifdef LLS_CAPTURE
    case CONFIGURATION_LLS_VALUE:
#endif
        m_modeValue[type] = value;
        break;
    case CONFIGURATION_BRIGHTNESS_VALUE:
    case CONFIGURATION_CAPTURE_COUNT:
        m_modeValue[type] = value;
        break;
    default:
        break;
    }

    return ret;
}

int ExynosCameraConfigurations::getModeValue(enum CONFIGURATION_VALUE_TYPE type) const
{
    int value = -1;

    switch(type) {
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
    case CONFIGURATION_EXTEND_SENSOR_MODE:
    case CONFIGURATION_MARKING_EXIF_FLASH:
    case CONFIGURATION_FLASH_MODE:
    case CONFIGURATION_BINNING_RATIO:
        value = m_modeValue[type];
        break;
    case CONFIGURATION_YUV_STALL_PORT:
    case CONFIGURATION_YUV_STALL_PORT_USAGE:
    case CONFIGURATION_BRIGHTNESS_VALUE:
    case CONFIGURATION_CAPTURE_COUNT:
#ifdef LLS_CAPTURE
    case CONFIGURATION_LLS_VALUE:
#endif
        value = m_modeValue[type];
        break;
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
    case SUPPORTED_FUNCTION_HIFILLS:
#ifdef USES_HIFI_LLS
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

    if (m_checkShootingMode(params) != NO_ERROR) {
        CLOGE("checkShootingMode failed");
    }

    if (m_checkRecordingFps(params) !=  NO_ERROR) {
        CLOGE("checkRecordingFps faild");
    }

    if (m_checkOperationMode(params) !=  NO_ERROR) {
        CLOGE("checkOperationMode faild");
    }

#ifdef USE_DUAL_CAMERA
    if (m_checkDualMode(params) !=  NO_ERROR) {
        CLOGE("checkDualMode faild");
    }
#endif

    if (m_checkVtMode(params) != NO_ERROR) {
        CLOGE("checkVtMode failed");
    }

    if (m_checkExtSensorMode() != NO_ERROR) {
        CLOGE("checkExtSensorMode failed");
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

status_t ExynosCameraConfigurations::m_checkRecordingFps(const CameraParameters& params)
{
    int newRecordingFps = params.getInt("recording-fps");

    CLOGD("%d fps", newRecordingFps);
    setModeValue(CONFIGURATION_RECORDING_FPS, newRecordingFps);

    return NO_ERROR;
}

status_t ExynosCameraConfigurations::m_checkOperationMode(__unused const CameraParameters& params)
{
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

status_t ExynosCameraConfigurations::m_checkExtSensorMode()
{
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
    return 0;
}

int32_t ExynosCameraConfigurations::getLongExposureShotCount(void)
{
#ifdef CAMERA_ADD_BAYER_ENABLE
    if (m_exposureTimeCapture <= CAMERA_SENSOR_EXPOSURE_TIME_MAX)
#endif
    {
        return 1;
    }

    return 0;
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
    char enableRawDump[PROPERTY_VALUE_MAX];
    property_get("ro.debug.rawdump", enableRawDump, "0");

    if (strcmp(enableRawDump, "1") == 0) {
        /*CLOGD("checkBayerDumpEnable : 1");*/
        return true;
    } else {
        /*CLOGD("checkBayerDumpEnable : 0");*/
        return false;
    }

    return true;
}
#endif  /* DEBUG_RAWDUMP */

void ExynosCameraConfigurations::setBokehBlurStrength(int bokehBlurStrength)
{
    m_bokehBlurStrength = bokehBlurStrength;
}

int ExynosCameraConfigurations::getBokehBlurStrength(void)
{
    return m_bokehBlurStrength;
}

int ExynosCameraConfigurations::getAntibanding()
{
    return AA_AE_ANTIBANDING_AUTO;
}

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

void ExynosCameraConfigurations::setTemperature(int temperature)
{
    m_temperature = temperature;
}

int ExynosCameraConfigurations::getTemperature(void)
{
    return m_temperature;
}
}; /* namespace android */
