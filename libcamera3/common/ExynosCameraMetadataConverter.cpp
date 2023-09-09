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
#define LOG_TAG "ExynosCameraMetadataConverter"

#include "ExynosCameraMetadataConverter.h"
#include "ExynosCameraRequestManager.h"

namespace android {
#define SET_BIT(x)      (1 << x)

ExynosCameraMetadataConverter::ExynosCameraMetadataConverter(int cameraId, ExynosCameraParameters *parameters)
{
    ExynosCameraActivityControl *activityControl = NULL;

    m_cameraId = cameraId;
    m_parameters = parameters;
    activityControl = m_parameters->getActivityControl();
    m_flashMgr = activityControl->getFlashMgr();
    m_sensorStaticInfo = m_parameters->getSensorStaticInfo();
    m_preCaptureTriggerOn = false;
    m_isManualAeControl = false;
    m_prevMeta = NULL;

    m_afMode = AA_AFMODE_CONTINUOUS_PICTURE;
    m_preAfMode = AA_AFMODE_CONTINUOUS_PICTURE;

    m_focusDistance = -1;

    m_maxFps = 30;
    m_overrideFlashControl= false;
    m_defaultAntibanding = m_parameters->getAntibanding();
    m_sceneMode = 0;

    m_frameCountMapIndex = 0;
    memset(m_frameCountMap, 0x00, sizeof(m_frameCountMap));
    memset(m_name, 0x00, sizeof(m_name));

    m_aemode = AA_AEMODE_ON_AUTO_FLASH; //for iris test
    m_aperture = 150; //for iris test
}

ExynosCameraMetadataConverter::~ExynosCameraMetadataConverter()
{
    m_defaultRequestSetting.release();
}

status_t ExynosCameraMetadataConverter::constructDefaultRequestSettings(int type, camera_metadata_t **request)
{
    CLOGD("Type(%d)", type);

    CameraMetadata settings;
    uint8_t hwLevel = m_sensorStaticInfo->supportedHwLevel;
    uint64_t supportedCapabilities = m_sensorStaticInfo->supportedCapabilities;

    /** android.request */
    /* request type */
    const uint8_t requestType = ANDROID_REQUEST_TYPE_CAPTURE;
    settings.update(ANDROID_REQUEST_TYPE, &requestType, 1);

    /* meta data mode */
    const uint8_t metadataMode = ANDROID_REQUEST_METADATA_MODE_FULL;
    settings.update(ANDROID_REQUEST_METADATA_MODE, &metadataMode, 1);

    /* id */
    const int32_t id = 0;
    settings.update(ANDROID_REQUEST_ID, &id, 1);

    /* frame count */
    const int32_t frameCount = 0;
    settings.update(ANDROID_REQUEST_FRAME_COUNT, &frameCount, 1);

    /** android.control */
    /* control intent */
    uint8_t controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
    switch (type) {
    case CAMERA3_TEMPLATE_PREVIEW:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW;
        CLOGD("type is ANDROID_CONTROL_CAPTURE_INTENT_PREVIEW");
        break;
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_STILL_CAPTURE;
        CLOGD("type is CAMERA3_TEMPLATE_STILL_CAPTURE");
        break;
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_RECORD;
        CLOGD("type is CAMERA3_TEMPLATE_VIDEO_RECORD");
        break;
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_VIDEO_SNAPSHOT;
        CLOGD("type is CAMERA3_TEMPLATE_VIDEO_SNAPSHOT");
        break;
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_ZERO_SHUTTER_LAG;
        CLOGD("type is CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG");
        break;
    case CAMERA3_TEMPLATE_MANUAL:
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_MANUAL;
        CLOGD("type is CAMERA3_TEMPLATE_MANUAL");
        break;
    default:
        CLOGD("Custom intent type is selected for setting control intent(%d)", type);
        controlIntent = ANDROID_CONTROL_CAPTURE_INTENT_CUSTOM;
    break;
    }
    settings.update(ANDROID_CONTROL_CAPTURE_INTENT, &controlIntent, 1);

    /* 3AA control */
    uint8_t controlMode = ANDROID_CONTROL_MODE_OFF;
    uint8_t afMode = ANDROID_CONTROL_AF_MODE_OFF;
    uint8_t aeMode = ANDROID_CONTROL_AE_MODE_OFF;
    uint8_t awbMode = ANDROID_CONTROL_AWB_MODE_OFF;
    int32_t aeTargetFpsRange[2];
    aeTargetFpsRange[0] = m_sensorStaticInfo->defaultFpsMin[DEFAULT_FPS_STILL];
    aeTargetFpsRange[1] = m_sensorStaticInfo->defaultFpsMAX[DEFAULT_FPS_STILL];

    switch (type) {
    case CAMERA3_TEMPLATE_PREVIEW:
        controlMode = ANDROID_CONTROL_MODE_AUTO;
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        aeMode = ANDROID_CONTROL_AE_MODE_ON;
        awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        break;
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
        controlMode = ANDROID_CONTROL_MODE_AUTO;
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        aeMode = ANDROID_CONTROL_AE_MODE_ON;
        awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        break;
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
        controlMode = ANDROID_CONTROL_MODE_AUTO;
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        aeMode = ANDROID_CONTROL_AE_MODE_ON;
        awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        /* Fix FPS for Recording */
        aeTargetFpsRange[0] = m_sensorStaticInfo->defaultFpsMin[DEFAULT_FPS_VIDEO];
        aeTargetFpsRange[1] = m_sensorStaticInfo->defaultFpsMAX[DEFAULT_FPS_VIDEO];
        break;
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        controlMode = ANDROID_CONTROL_MODE_AUTO;
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
        aeMode = ANDROID_CONTROL_AE_MODE_ON;
        awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        break;
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
        controlMode = ANDROID_CONTROL_MODE_AUTO;
        afMode = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
        aeMode = ANDROID_CONTROL_AE_MODE_ON;
        awbMode = ANDROID_CONTROL_AWB_MODE_AUTO;
        break;
    case CAMERA3_TEMPLATE_MANUAL:
        controlMode = ANDROID_CONTROL_MODE_OFF;
        afMode = ANDROID_CONTROL_AF_MODE_OFF;
        aeMode = ANDROID_CONTROL_AE_MODE_OFF;
        awbMode = ANDROID_CONTROL_AWB_MODE_OFF;
        break;
    default:
        CLOGD("Custom intent type is selected for setting 3AA control(%d)", type);
        break;
    }
    settings.update(ANDROID_CONTROL_MODE, &controlMode, 1);
    if (m_sensorStaticInfo->minimumFocusDistance == 0.0f) {
        afMode = ANDROID_CONTROL_AF_MODE_OFF;
    }
    settings.update(ANDROID_CONTROL_AF_MODE, &afMode, 1);
    settings.update(ANDROID_CONTROL_AE_MODE, &aeMode, 1);
    settings.update(ANDROID_CONTROL_AWB_MODE, &awbMode, 1);
    settings.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE, aeTargetFpsRange, 2);

    const uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    settings.update(ANDROID_CONTROL_AF_TRIGGER, &afTrigger, 1);

    const uint8_t aePrecaptureTrigger = ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE;
    settings.update(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER, &aePrecaptureTrigger, 1);

    /* effect mode */
    const uint8_t effectMode = ANDROID_CONTROL_EFFECT_MODE_OFF;
    settings.update(ANDROID_CONTROL_EFFECT_MODE, &effectMode, 1);

    /* scene mode */
    const uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    settings.update(ANDROID_CONTROL_SCENE_MODE, &sceneMode, 1);

    /* ae lock mode */
    const uint8_t aeLock = ANDROID_CONTROL_AE_LOCK_OFF;
    settings.update(ANDROID_CONTROL_AE_LOCK, &aeLock, 1);

    /* ae region */
    int w,h;
    m_parameters->getMaxSensorSize(&w, &h);
    const int32_t controlRegions[5] = {
        0, 0, w, h, 0
    };
    if (m_sensorStaticInfo->max3aRegions[AE] > 0) {
        settings.update(ANDROID_CONTROL_AE_REGIONS, controlRegions, 5);
    }
    if (m_sensorStaticInfo->max3aRegions[AWB] > 0) {
        settings.update(ANDROID_CONTROL_AWB_REGIONS, controlRegions, 5);
    }
    if (m_sensorStaticInfo->max3aRegions[AF] > 0) {
        settings.update(ANDROID_CONTROL_AF_REGIONS, controlRegions, 5);
    }

    /* exposure compensation */
    const int32_t aeExpCompensation = 0;
    settings.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &aeExpCompensation, 1);

    /* anti-banding mode */
    const uint8_t aeAntibandingMode = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
    settings.update(ANDROID_CONTROL_AE_ANTIBANDING_MODE, &aeAntibandingMode, 1);

    /* awb lock */
    const uint8_t awbLock = ANDROID_CONTROL_AWB_LOCK_OFF;
    settings.update(ANDROID_CONTROL_AWB_LOCK, &awbLock, 1);

    /* video stabilization mode */
    const uint8_t vstabMode = ANDROID_CONTROL_VIDEO_STABILIZATION_MODE_OFF;
    settings.update(ANDROID_CONTROL_VIDEO_STABILIZATION_MODE, &vstabMode, 1);

    const int32_t rawsensitivity = 100;
    settings.update(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST, &rawsensitivity, 1);

    /** android.lens */
    const float focusDistance = -1.0f;
    settings.update(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);
    settings.update(ANDROID_LENS_FOCAL_LENGTH, &(m_sensorStaticInfo->focalLength), 1);
    settings.update(ANDROID_LENS_APERTURE, &(m_sensorStaticInfo->fNumber), 1); // ExifInterface :  TAG_APERTURE = "FNumber";
    settings.update(ANDROID_LENS_FILTER_DENSITY, &(m_sensorStaticInfo->filterDensity), 1);
    const uint8_t opticalStabilizationMode = ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF;
    settings.update(ANDROID_LENS_OPTICAL_STABILIZATION_MODE, &opticalStabilizationMode, 1);

    /** android.sensor */
    settings.update(ANDROID_SENSOR_EXPOSURE_TIME, &(m_sensorStaticInfo->exposureTime), 1);
    const int64_t frameDuration = 33333333L; /* 1/30 s */
    settings.update(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);
    const int32_t sensitivity = m_sensorStaticInfo->sensitivityRange[MIN];
    settings.update(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);

    /** android.flash */
    const uint8_t flashMode = ANDROID_FLASH_MODE_OFF;
    settings.update(ANDROID_FLASH_MODE, &flashMode, 1);
    const uint8_t firingPower = 0;
    settings.update(ANDROID_FLASH_FIRING_POWER, &firingPower, 1);
    const int64_t firingTime = 0;
    settings.update(ANDROID_FLASH_FIRING_TIME, &firingTime, 1);

    /** android.led */
    if (m_sensorStaticInfo->leds != NULL) {
        const uint8_t ledTransmit = ANDROID_LED_TRANSMIT_OFF ;
        settings.update(ANDROID_LED_TRANSMIT, &ledTransmit, 1);
    }

    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_MANUAL_POST_PROCESSING) {
        /** android.color_correction */
        const camera_metadata_rational_t colorTransform[9] = {
            {1,1}, {0,1}, {0,1},
            {0,1}, {1,1}, {0,1},
            {0,1}, {0,1}, {1,1}
        };
        settings.update(ANDROID_COLOR_CORRECTION_TRANSFORM, colorTransform, 9);

        const float correctionGains[4] = {
            0.0f, 0.0f,
            0.0f, 0.0f
        };
        settings.update(ANDROID_COLOR_CORRECTION_GAINS, correctionGains, 4);

        /** android.tonemap */
        const float tonemapCurve[4] = {
            0.0f, 0.0f,
            1.0f, 1.0f
        };
        settings.update(ANDROID_TONEMAP_CURVE_RED, tonemapCurve, 4);
        settings.update(ANDROID_TONEMAP_CURVE_GREEN, tonemapCurve, 4);
        settings.update(ANDROID_TONEMAP_CURVE_BLUE, tonemapCurve, 4);
    }

#if 0 //future
    /** android.noise_reduction */
    const uint8_t noiseStrength = 5;
    settings.update(ANDROID_NOISE_REDUCTION_STRENGTH, &noiseStrength, 1);

    /** android.edge */
    const uint8_t edgeStrength = 5;
    settings.update(ANDROID_EDGE_STRENGTH, &edgeStrength, 1);

    /** android.shading */
    const uint8_t shadingStrength = 5;
    settings.update(ANDROID_SHADING_STRENGTH, &shadingStrength, 1);
#endif

    /** android.scaler */
    const int32_t cropRegion[4] = {
        0, 0, w, h
    };
    settings.update(ANDROID_SCALER_CROP_REGION, cropRegion, 4);

    /** android.jpeg */
    const uint8_t jpegQuality = 96;
    settings.update(ANDROID_JPEG_QUALITY, &jpegQuality, 1);
    const int32_t thumbnailSize[2] = {
        m_sensorStaticInfo->maxThumbnailW, m_sensorStaticInfo->maxThumbnailH
    };
    settings.update(ANDROID_JPEG_THUMBNAIL_SIZE, thumbnailSize, 2);
    const uint8_t thumbnailQuality = 100;
    settings.update(ANDROID_JPEG_THUMBNAIL_QUALITY, &thumbnailQuality, 1);
    const double gpsCoordinates[3] = {
        0, 0 , 0
    };
    settings.update(ANDROID_JPEG_GPS_COORDINATES, gpsCoordinates, 3);
    const uint8_t gpsProcessingMethod[32] = "None";
    settings.update(ANDROID_JPEG_GPS_PROCESSING_METHOD, gpsProcessingMethod, 32);
    const int64_t gpsTimestamp = 0;
    settings.update(ANDROID_JPEG_GPS_TIMESTAMP, &gpsTimestamp, 1);
    const int32_t jpegOrientation = 0;
    settings.update(ANDROID_JPEG_ORIENTATION, &jpegOrientation, 1);

    /** android.stats */
    uint8_t faceDetectMode = ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
    uint8_t histogramMode = ANDROID_STATISTICS_HISTOGRAM_MODE_OFF;
    uint8_t sharpnessMapMode = ANDROID_STATISTICS_SHARPNESS_MAP_MODE_OFF;
    uint8_t hotPixelMapMode = 0;

    settings.update(ANDROID_STATISTICS_FACE_DETECT_MODE, &faceDetectMode, 1);
    settings.update(ANDROID_STATISTICS_HISTOGRAM_MODE, &histogramMode, 1);
    settings.update(ANDROID_STATISTICS_SHARPNESS_MAP_MODE, &sharpnessMapMode, 1);
    settings.update(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE, &hotPixelMapMode, 1);

    /** android.blacklevel */
    const uint8_t blackLevelLock = ANDROID_BLACK_LEVEL_LOCK_OFF;
    settings.update(ANDROID_BLACK_LEVEL_LOCK, &blackLevelLock, 1);

    const int32_t patternMode = ANDROID_SENSOR_TEST_PATTERN_MODE_OFF;
    settings.update(ANDROID_SENSOR_TEST_PATTERN_MODE, &patternMode, 1);

    /** Processing block modes */
    uint8_t hotPixelMode = ANDROID_HOT_PIXEL_MODE_OFF;
    uint8_t demosaicMode = ANDROID_DEMOSAIC_MODE_FAST;
    uint8_t noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_OFF;
    uint8_t shadingMode = ANDROID_SHADING_MODE_OFF;
    uint8_t colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX;
    uint8_t tonemapMode = ANDROID_TONEMAP_MODE_CONTRAST_CURVE;
    uint8_t edgeMode = ANDROID_EDGE_MODE_OFF;
    uint8_t lensShadingMapMode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_OFF;
    uint8_t colorCorrectionAberrationMode = ANDROID_COLOR_CORRECTION_ABERRATION_MODE_OFF;

    switch (type) {
    case CAMERA3_TEMPLATE_STILL_CAPTURE:
        if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
            || supportedCapabilities & CAPABILITIES_RAW) {
            lensShadingMapMode = ANDROID_STATISTICS_LENS_SHADING_MAP_MODE_ON;
        }
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY;
        noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY;
        shadingMode = ANDROID_SHADING_MODE_FAST;
        colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY;
        tonemapMode = ANDROID_TONEMAP_MODE_HIGH_QUALITY;
        edgeMode = ANDROID_EDGE_MODE_HIGH_QUALITY;
        break;
    case CAMERA3_TEMPLATE_VIDEO_SNAPSHOT:
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY;
        noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_FAST;
        shadingMode = ANDROID_SHADING_MODE_FAST;
        colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY;
        tonemapMode = ANDROID_TONEMAP_MODE_FAST;
        edgeMode = ANDROID_EDGE_MODE_FAST;
        break;
    case CAMERA3_TEMPLATE_ZERO_SHUTTER_LAG:
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_HIGH_QUALITY;
        noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG;
        shadingMode = ANDROID_SHADING_MODE_FAST;
        colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_HIGH_QUALITY;
        tonemapMode = ANDROID_TONEMAP_MODE_FAST;
        edgeMode = ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG;
        break;
    case CAMERA3_TEMPLATE_PREVIEW:
    case CAMERA3_TEMPLATE_VIDEO_RECORD:
    default:
        hotPixelMode = ANDROID_HOT_PIXEL_MODE_FAST;
        noiseReductionMode = ANDROID_NOISE_REDUCTION_MODE_FAST;
        shadingMode = ANDROID_SHADING_MODE_FAST;
        colorCorrectionMode = ANDROID_COLOR_CORRECTION_MODE_FAST;
        tonemapMode = ANDROID_TONEMAP_MODE_FAST;
        edgeMode = ANDROID_EDGE_MODE_FAST;
        break;
    }

    if (hwLevel != ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
        settings.update(ANDROID_HOT_PIXEL_MODE, &hotPixelMode, 1);
        settings.update(ANDROID_DEMOSAIC_MODE, &demosaicMode, 1);
        settings.update(ANDROID_SHADING_MODE, &shadingMode, 1);
        settings.update(ANDROID_TONEMAP_MODE, &tonemapMode, 1);
        settings.update(ANDROID_EDGE_MODE, &edgeMode, 1);
        settings.update(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE, &lensShadingMapMode, 1);
    }
    settings.update(ANDROID_NOISE_REDUCTION_MODE, &noiseReductionMode, 1);
    settings.update(ANDROID_COLOR_CORRECTION_MODE, &colorCorrectionMode, 1);
    settings.update(ANDROID_COLOR_CORRECTION_ABERRATION_MODE, &colorCorrectionAberrationMode, 1);

    /* Vendor RequestSettings */
    m_constructVendorDefaultRequestSettings(type, &settings);

    *request = settings.release();
    m_defaultRequestSetting = *request;

    CLOGD("Registered default request template(%d)", type);
    return OK;
}

status_t ExynosCameraMetadataConverter::constructStaticInfo(int cameraId, camera_metadata_t **cameraInfo)
{
    status_t ret = NO_ERROR;

    CLOGD2("ID(%d)", cameraId);
    struct ExynosCameraSensorInfoBase *sensorStaticInfo = NULL;
    CameraMetadata info;
    Vector<int64_t> i64Vector;
    Vector<int32_t> i32Vector;
    Vector<uint8_t> i8Vector;

    sensorStaticInfo = createExynosCameraSensorInfo(cameraId);
    if (sensorStaticInfo == NULL) {
        CLOGE2("sensorStaticInfo is NULL");
        return BAD_VALUE;
    }

    /* android.colorCorrection static attributes */
    if (sensorStaticInfo->colorAberrationModes != NULL) {
        ret = info.update(ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES,
                sensorStaticInfo->colorAberrationModes,
                sensorStaticInfo->colorAberrationModesLength);
        if (ret < 0) {
            CLOGD2("ANDROID_COLOR_CORRECTION_AVAILABLE_ABERRATION_MODES update failed(%d)", ret);
        }
    } else {
        CLOGD2("colorAberrationModes at sensorStaticInfo is NULL");
    }

    /* andorid.control static attributes */
    if (sensorStaticInfo->antiBandingModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES,
                sensorStaticInfo->antiBandingModes,
                sensorStaticInfo->antiBandingModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AE_AVAILABLE_ANTIBANDING_MODES update failed(%d)", ret);
    } else {
        CLOGD2("antiBandingModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->aeModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                sensorStaticInfo->aeModes,
                sensorStaticInfo->aeModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AE_AVAILABLE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("aeModes at sensorStaticInfo is NULL");
    }

    i32Vector.clear();
    m_createAeAvailableFpsRanges(sensorStaticInfo, &i32Vector);
    ret = info.update(ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
            i32Vector.array(), i32Vector.size());
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES update failed(%d)", ret);

    ret = info.update(ANDROID_CONTROL_AE_COMPENSATION_RANGE,
            sensorStaticInfo->exposureCompensationRange,
            ARRAY_LENGTH(sensorStaticInfo->exposureCompensationRange));
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_AE_COMPENSATION_RANGE update failed(%d)", ret);

    const camera_metadata_rational exposureCompensationStep =
    {(int32_t)((sensorStaticInfo->exposureCompensationStep) * 100.0), 100};
    ret = info.update(ANDROID_CONTROL_AE_COMPENSATION_STEP,
            &exposureCompensationStep, 1);
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_AE_COMPENSATION_STEP update failed(%d)", ret);

    if (sensorStaticInfo->afModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                sensorStaticInfo->afModes,
                sensorStaticInfo->afModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AF_AVAILABLE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("afModes is NULL");
    }

    if (sensorStaticInfo->effectModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AVAILABLE_EFFECTS,
                sensorStaticInfo->effectModes,
                sensorStaticInfo->effectModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AVAILABLE_EFFECTS update failed(%d)", ret);
    } else {
        CLOGD2("effectModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->sceneModes != NULL) {
        i8Vector.clear();
        i8Vector.appendArray(sensorStaticInfo->sceneModes, sensorStaticInfo->sceneModesLength);
        if (sensorStaticInfo->sceneHDRSupport) {
            i8Vector.add(ANDROID_CONTROL_SCENE_MODE_HDR);
        }
        ret = info.update(ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
                    i8Vector.array(), i8Vector.size());
        if (ret < 0)
            CLOGE2("ANDROID_CONTROL_AVAILABLE_SCENE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("sceneModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->sceneModeOverrides != NULL) {
        i8Vector.clear();
        i8Vector.appendArray(sensorStaticInfo->sceneModeOverrides, sensorStaticInfo->sceneModeOverridesLength);
        if (sensorStaticInfo->sceneHDRSupport) {
            i8Vector.add(ANDROID_CONTROL_AE_MODE_ON);
            i8Vector.add(ANDROID_CONTROL_AWB_MODE_AUTO);
            i8Vector.add(ANDROID_CONTROL_AF_MODE_AUTO);
        }
        ret  = info.update(ANDROID_CONTROL_SCENE_MODE_OVERRIDES,
                        i8Vector.array(), i8Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_SCENE_MODE_OVERRIDES update failed(%d)", ret);
    } else {
        CLOGD2("sceneModeOverrides at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->videoStabilizationModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES,
                sensorStaticInfo->videoStabilizationModes,
                sensorStaticInfo->videoStabilizationModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES update failed(%d)", ret);
    } else {
        CLOGD2("videoStabilizationModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->awbModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                sensorStaticInfo->awbModes,
                sensorStaticInfo->awbModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AWB_AVAILABLE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("awbModes at sensorStaticInfo is NULL");
    }

    ret = info.update(ANDROID_CONTROL_MAX_REGIONS,
            sensorStaticInfo->max3aRegions,
            ARRAY_LENGTH(sensorStaticInfo->max3aRegions));
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_MAX_REGIONS update failed(%d)", ret);

    i32Vector.clear();
    if (m_createControlAvailableHighSpeedVideoConfigurations(sensorStaticInfo, &i32Vector) == NO_ERROR ) {
        ret = info.update(ANDROID_CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS,
             i32Vector.array(), i32Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS update failed(%d)", ret);
    } else {
        CLOGD2("ANDROID_CONTROL_AVAILABLE_HIGH_SPEED_VIDEO_CONFIGURATIONS is NULL");
    }

    ret = info.update(ANDROID_CONTROL_AE_LOCK_AVAILABLE,
            &(sensorStaticInfo->aeLockAvailable), 1);
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_AE_LOCK_AVAILABLE update failed(%d)", ret);

    ret = info.update(ANDROID_CONTROL_AWB_LOCK_AVAILABLE,
            &(sensorStaticInfo->awbLockAvailable), 1);
    if (ret < 0)
        CLOGD2("ANDROID_CONTROL_AWB_LOCK_AVAILABLE update failed(%d)", ret);

    if (sensorStaticInfo->controlModes != NULL) {
        ret = info.update(ANDROID_CONTROL_AVAILABLE_MODES,
                sensorStaticInfo->controlModes,
                sensorStaticInfo->controlModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_CONTROL_AVAILABLE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("controlModes at sensorStaticInfo is NULL");
    }

    ret = info.update(ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE,
            sensorStaticInfo->postRawSensitivityBoost,
            ARRAY_LENGTH(sensorStaticInfo->postRawSensitivityBoost));
    if (ret < 0) {
        CLOGD2("ANDROID_CONTROL_POST_RAW_SENSITIVITY_BOOST_RANGE update failed(%d)", ret);
    }

    /* android.edge static attributes */
    if (sensorStaticInfo->edgeModes != NULL) {
        i8Vector.clear();
        i8Vector.appendArray(sensorStaticInfo->edgeModes, sensorStaticInfo->edgeModesLength);
        if (sensorStaticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING) {
            i8Vector.add(ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG);
        }
        ret = info.update(ANDROID_EDGE_AVAILABLE_EDGE_MODES, i8Vector.array(), i8Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_EDGE_AVAILABLE_EDGE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("edgeModes at sensorStaticInfo is NULL");
    }

    /* andorid.flash static attributes */
    ret = info.update(ANDROID_FLASH_INFO_AVAILABLE,
            &(sensorStaticInfo->flashAvailable), 1);
    if (ret < 0)
        CLOGD2("ANDROID_FLASH_INFO_AVAILABLE update failed(%d)", ret);

    ret = info.update(ANDROID_FLASH_INFO_CHARGE_DURATION,
            &(sensorStaticInfo->chargeDuration), 1);
    if (ret < 0)
        CLOGD2("ANDROID_FLASH_INFO_CHARGE_DURATION update failed(%d)", ret);

    ret = info.update(ANDROID_FLASH_COLOR_TEMPERATURE,
            &(sensorStaticInfo->colorTemperature), 1);
    if (ret < 0)
        CLOGD2("ANDROID_FLASH_COLOR_TEMPERATURE update failed(%d)", ret);

    ret = info.update(ANDROID_FLASH_MAX_ENERGY,
            &(sensorStaticInfo->maxEnergy), 1);
    if (ret < 0)
        CLOGD2("ANDROID_FLASH_MAX_ENERGY update failed(%d)", ret);

    /* android.hotPixel static attributes */
    if (sensorStaticInfo->hotPixelModes != NULL) {
        ret = info.update(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES,
                sensorStaticInfo->hotPixelModes,
                sensorStaticInfo->hotPixelModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES update failed(%d)", ret);
    } else {
        CLOGD2("hotPixelModes at sensorStaticInfo is NULL");
    }

    /* andorid.jpeg static attributes */
    i32Vector.clear();
    m_createJpegAvailableThumbnailSizes(sensorStaticInfo, &i32Vector);
    ret = info.update(ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES, i32Vector.array(), i32Vector.size());
    if (ret < 0)
        CLOGD2("ANDROID_JPEG_AVAILABLE_THUMBNAIL_SIZES update failed(%d)", ret);

    const int32_t jpegMaxSize = sensorStaticInfo->maxPictureW * sensorStaticInfo->maxPictureH * 2;
    ret = info.update(ANDROID_JPEG_MAX_SIZE, &jpegMaxSize, 1);
    if (ret < 0)
        CLOGD2("ANDROID_JPEG_MAX_SIZE update failed(%d)", ret);


    /* android.lens static attributes */
    if (sensorStaticInfo->availableApertureValues != NULL) {
        ret = info.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                sensorStaticInfo->availableApertureValues, sensorStaticInfo->availableApertureValuesLength);
        if (ret < 0)
            CLOGD2("ANDROID_LENS_INFO_AVAILABLE_APERTURES update failed(%d)", ret);
    } else {
        ret = info.update(ANDROID_LENS_INFO_AVAILABLE_APERTURES,
                &(sensorStaticInfo->fNumber), 1);
        if (ret < 0)
            CLOGD2("ANDROID_LENS_INFO_AVAILABLE_APERTURES update failed(%d)", ret);
    }

    ret = info.update(ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES,
            &(sensorStaticInfo->filterDensity), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_AVAILABLE_FILTER_DENSITIES update failed(%d)", ret);

    ret = info.update(ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS,
            &(sensorStaticInfo->focalLength), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_AVAILABLE_FOCAL_LENGTHS update failed(%d)", ret);

    if (sensorStaticInfo->opticalStabilization != NULL
        && m_hasTagInList(sensorStaticInfo->requestKeys,
                          sensorStaticInfo->requestKeysLength,
                          ANDROID_LENS_OPTICAL_STABILIZATION_MODE))
    {
        ret = info.update(ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION,
                sensorStaticInfo->opticalStabilization,
                sensorStaticInfo->opticalStabilizationLength);
        if (ret < 0)
            CLOGD2("ANDROID_LENS_INFO_AVAILABLE_OPTICAL_STABILIZATION update failed(%d)",
                    ret);
    } else {
        CLOGD2("opticalStabilization at sensorStaticInfo is NULL");
    }

    ret = info.update(ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE,
            &(sensorStaticInfo->hyperFocalDistance), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_HYPERFOCAL_DISTANCE update failed(%d)", ret);

    ret = info.update(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE,
            &(sensorStaticInfo->minimumFocusDistance), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE update failed(%d)", ret);

    ret = info.update(ANDROID_LENS_INFO_SHADING_MAP_SIZE,
            sensorStaticInfo->shadingMapSize,
            ARRAY_LENGTH(sensorStaticInfo->shadingMapSize));
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_SHADING_MAP_SIZE update failed(%d)", ret);

    ret = info.update(ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION,
            &(sensorStaticInfo->focusDistanceCalibration), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION update failed(%d)", ret);

    ret = info.update(ANDROID_LENS_FACING,
            &(sensorStaticInfo->lensFacing), 1);
    if (ret < 0)
        CLOGD2("ANDROID_LENS_FACING update failed(%d)", ret);

    /* android.noiseReduction static attributes */
    if (sensorStaticInfo->noiseReductionModes != NULL) {
        i8Vector.clear();
        i8Vector.appendArray(sensorStaticInfo->noiseReductionModes, sensorStaticInfo->noiseReductionModesLength);
        if (sensorStaticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING) {
            i8Vector.add(ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG);
        }
        ret = info.update(ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES,
                        i8Vector.array(), i8Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_NOISE_REDUCTION_AVAILABLE_NOISE_REDUCTION_MODES update failed(%d)", ret);
    } else {
        CLOGD2("noiseReductionModes at sensorStaticInfo is NULL");
    }

    /* android.request static attributes */
    ret = info.update(ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS,
            sensorStaticInfo->maxNumOutputStreams,
            ARRAY_LENGTH(sensorStaticInfo->maxNumOutputStreams));
    if (ret < 0)
        CLOGD2("ANDROID_REQUEST_MAX_NUM_OUTPUT_STREAMS update failed(%d)", ret);

    ret = info.update(ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS,
            &(sensorStaticInfo->maxNumInputStreams), 1);
    if (ret < 0)
        CLOGD2("ANDROID_REQUEST_MAX_NUM_INPUT_STREAMS update failed(%d)", ret);

    ret = info.update(ANDROID_REQUEST_PIPELINE_MAX_DEPTH,
            &(sensorStaticInfo->maxPipelineDepth), 1);
    if (ret < 0)
        CLOGD2("ANDROID_REQUEST_PIPELINE_MAX_DEPTH update failed(%d)", ret);

    ret = info.update(ANDROID_REQUEST_PARTIAL_RESULT_COUNT,
            &(sensorStaticInfo->partialResultCount), 1);
    if (ret < 0)
        CLOGD2("ANDROID_REQUEST_PARTIAL_RESULT_COUNT update failed(%d)", ret);

    /* android.scaler static attributes */
    const float maxZoom = ((float)sensorStaticInfo->maxZoomRatio) / 1000.0f;
    ret = info.update(ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM, &maxZoom, 1);
    if (ret < 0) {
        CLOGD2("ANDROID_SCALER_AVAILABLE_MAX_DIGITAL_ZOOM update failed(%d)", ret);
    }

    i32Vector.clear();
    if (m_createScalerAvailableInputOutputFormatsMap(sensorStaticInfo, &i32Vector) == NO_ERROR) {
        /* Update AvailableInputOutputFormatsMap only if private reprocessing is supported */
        ret = info.update(ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP, i32Vector.array(), i32Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_SCALER_AVAILABLE_INPUT_OUTPUT_FORMATS_MAP update failed(%d)", ret);
    }

    i32Vector.clear();
    m_createScalerAvailableStreamConfigurationsOutput(sensorStaticInfo, &i32Vector);
    ret = info.update(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, i32Vector.array(), i32Vector.size());
    if (ret < 0)
        CLOGD2("ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS update failed(%d)", ret);

    i64Vector.clear();
    m_createScalerAvailableMinFrameDurations(sensorStaticInfo, &i64Vector);
    ret = info.update(ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS, i64Vector.array(), i64Vector.size());
    if (ret < 0)
        CLOGD2("ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS update failed(%d)", ret);

    if (sensorStaticInfo->stallDurations != NULL) {
        ret = info.update(ANDROID_SCALER_AVAILABLE_STALL_DURATIONS,
                sensorStaticInfo->stallDurations,
                sensorStaticInfo->stallDurationsLength);
        if (ret < 0)
            CLOGD2("ANDROID_SCALER_AVAILABLE_STALL_DURATIONS update failed(%d)", ret);
    } else {
        CLOGD2("stallDurations at sensorStaticInfo is NULL");
    }

    ret = info.update(ANDROID_SCALER_CROPPING_TYPE,
            &(sensorStaticInfo->croppingType), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SCALER_CROPPING_TYPE update failed(%d)", ret);

    /* android.sensor static attributes */
    const int32_t kResolution[4] =
    {0, 0, sensorStaticInfo->maxSensorW, sensorStaticInfo->maxSensorH};
    ret = info.update(ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE, kResolution, 4);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_ACTIVE_ARRAY_SIZE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
            sensorStaticInfo->sensitivityRange,
            ARRAY_LENGTH(sensorStaticInfo->sensitivityRange));
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_SENSITIVITY_RANGE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT,
            &(sensorStaticInfo->colorFilterArrangement), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
            sensorStaticInfo->exposureTimeRange,
            ARRAY_LENGTH(sensorStaticInfo->exposureTimeRange));
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_MAX_FRAME_DURATION,
            &(sensorStaticInfo->maxFrameDuration), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_MAX_FRAME_DURATION update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_PHYSICAL_SIZE,
            sensorStaticInfo->sensorPhysicalSize,
            ARRAY_LENGTH(sensorStaticInfo->sensorPhysicalSize));
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_PHYSICAL_SIZE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE, &(kResolution[2]), 2);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_PIXEL_ARRAY_SIZE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_WHITE_LEVEL,
            &(sensorStaticInfo->whiteLevel), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_WHITE_LEVEL update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE,
            &(sensorStaticInfo->timestampSource), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_REFERENCE_ILLUMINANT1,
            &(sensorStaticInfo->referenceIlluminant1), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_REFERENCE_ILLUMINANT1 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_REFERENCE_ILLUMINANT2,
            &(sensorStaticInfo->referenceIlluminant2), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_REFERENCE_ILLUMINANT2 update failed(%d)", ret);

    int32_t calibrationRG = 1024, calibrationBG = 1024;
    getAWBCalibrationGain(&calibrationRG, &calibrationBG,
                    sensorStaticInfo->masterRGain, sensorStaticInfo->masterBGain);
    camera_metadata_rational_t calibrationMatrix[9] = {
        {calibrationRG, 1024}, {0, 1024}, {0, 1024},
        {0, 1024}, {1024, 1024}, {0, 1024},
        {0, 1024}, {0, 1024}, {calibrationBG, 1024},
    };

    ret = info.update(ANDROID_SENSOR_CALIBRATION_TRANSFORM1, calibrationMatrix, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_CALIBRATION_TRANSFORM2 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_CALIBRATION_TRANSFORM2, calibrationMatrix, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_CALIBRATION_TRANSFORM2 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_COLOR_TRANSFORM1, sensorStaticInfo->colorTransformMatrix1, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_COLOR_TRANSFORM1 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_COLOR_TRANSFORM2, sensorStaticInfo->colorTransformMatrix2, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_COLOR_TRANSFORM2 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_FORWARD_MATRIX1, sensorStaticInfo->forwardMatrix1, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_FORWARD_MATRIX1 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_FORWARD_MATRIX2, sensorStaticInfo->forwardMatrix2, 9);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_FORWARD_MATRIX2 update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_BLACK_LEVEL_PATTERN,
            sensorStaticInfo->blackLevelPattern,
            ARRAY_LENGTH(sensorStaticInfo->blackLevelPattern));
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_BLACK_LEVEL_PATTERN update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_MAX_ANALOG_SENSITIVITY,
            &(sensorStaticInfo->maxAnalogSensitivity), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_MAX_ANCLOG_SENSITIVITY update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_ORIENTATION,
            &(sensorStaticInfo->orientation), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_ORIENTATION update failed(%d)", ret);

    ret = info.update(ANDROID_SENSOR_PROFILE_HUE_SAT_MAP_DIMENSIONS,
            sensorStaticInfo->profileHueSatMapDimensions,
            ARRAY_LENGTH(sensorStaticInfo->profileHueSatMapDimensions));
    if (ret < 0)
        CLOGD2("ANDROID_SENSOR_PROFILE_HUE_SAT_MAP_DIMENSIONS update failed(%d)", ret);

    if (sensorStaticInfo->testPatternModes != NULL) {
        ret = info.update(ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES,
                sensorStaticInfo->testPatternModes,
                sensorStaticInfo->testPatternModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_SENSOR_AVAILABLE_TEST_PATTERN_MODES update failed(%d)", ret);
    } else {
        CLOGD2("testPatternModes at sensorStaticInfo is NULL");
    }

    /* android.statistics static attributes */
    if (sensorStaticInfo->faceDetectModes != NULL) {
        ret = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES,
                sensorStaticInfo->faceDetectModes,
                sensorStaticInfo->faceDetectModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_STATISTICS_INFO_AVAILABLE_FACE_DETECT_MODES update failed(%d)", ret);
    } else {
        CLOGD2("faceDetectModes at sensorStaticInfo is NULL");
    }

    ret = info.update(ANDROID_STATISTICS_INFO_HISTOGRAM_BUCKET_COUNT,
            &(sensorStaticInfo->histogramBucketCount), 1);
    if (ret < 0)
        CLOGD2("ANDROID_STATISTICS_INFO_HISTOGRAM_BUCKET_COUNT update failed(%d)", ret);

    ret = info.update(ANDROID_STATISTICS_INFO_MAX_FACE_COUNT,
            &sensorStaticInfo->maxNumDetectedFaces, 1);
    if (ret < 0)
        CLOGD2("ANDROID_STATISTICS_INFO_MAX_FACE_COUNT update failed(%d)", ret);

    ret = info.update(ANDROID_STATISTICS_INFO_MAX_HISTOGRAM_COUNT,
            &sensorStaticInfo->maxHistogramCount, 1);
    if (ret < 0)
        CLOGD2("ANDROID_STATISTICS_INFO_MAX_HISTOGRAM_COUNT update failed(%d)", ret);

    ret = info.update(ANDROID_STATISTICS_INFO_MAX_SHARPNESS_MAP_VALUE,
            &(sensorStaticInfo->maxSharpnessMapValue), 1);
    if (ret < 0)
        CLOGD2("ANDROID_STATISTICS_INFO_MAX_SHARPNESS_MAP_VALUE update failed(%d)", ret);

    ret = info.update(ANDROID_STATISTICS_INFO_SHARPNESS_MAP_SIZE,
            sensorStaticInfo->sharpnessMapSize,
            ARRAY_LENGTH(sensorStaticInfo->sharpnessMapSize));
    if (ret < 0)
        CLOGD2("ANDROID_STATISTICS_INFO_SHARPNESS_MAP_SIZE update failed(%d)", ret);

    if (sensorStaticInfo->hotPixelMapModes != NULL) {
        ret = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES,
                sensorStaticInfo->hotPixelMapModes,
                sensorStaticInfo->hotPixelMapModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES update failed(%d)", ret);
    } else {
        CLOGD2("hotPixelMapModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->lensShadingMapModes != NULL) {
        ret = info.update(ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES,
                sensorStaticInfo->lensShadingMapModes,
                sensorStaticInfo->lensShadingMapModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_STATISTICS_INFO_AVAILABLE_LENS_SHADING_MAP_MODES update failed(%d)", ret);
    } else {
        CLOGD2("lensShadingMapModes at sensorStaticInfo is NULL");
    }

    if (sensorStaticInfo->shadingAvailableModes != NULL) {
        ret = info.update(ANDROID_SHADING_AVAILABLE_MODES,
                sensorStaticInfo->shadingAvailableModes,
                sensorStaticInfo->shadingAvailableModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_SHADING_AVAILABLE_MODES update failed(%d)", ret);
    } else {
        CLOGD2("shadingAvailableModes at sensorStaticInfo is NULL");
    }

    /* andorid.tonemap static attributes */
    ret = info.update(ANDROID_TONEMAP_MAX_CURVE_POINTS,
            &(sensorStaticInfo->tonemapCurvePoints), 1);
    if (ret < 0)
        CLOGD2("ANDROID_TONEMAP_MAX_CURVE_POINTS update failed(%d)", ret);

    if (sensorStaticInfo->toneMapModes != NULL) {
        ret = info.update(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES,
                sensorStaticInfo->toneMapModes,
                sensorStaticInfo->toneMapModesLength);
        if (ret < 0)
            CLOGD2("ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES update failed(%d)", ret);
    } else {
        CLOGD2("toneMapModes at sensorStaticInfo is NULL");
    }

    /* android.led static attributes */
    if (sensorStaticInfo->leds != NULL) {
        ret = info.update(ANDROID_LED_AVAILABLE_LEDS,
                sensorStaticInfo->leds,
                sensorStaticInfo->ledsLength);
        if (ret < 0)
            CLOGD2("ANDROID_LED_AVAILABLE_LEDS update failed(%d)", ret);
    } else {
        CLOGD2("leds at sensorStaticInfo is NULL");
    }

    /* andorid.info static attributes */
    ret = info.update(ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL,
            &(sensorStaticInfo->supportedHwLevel), 1);
    if (ret < 0)
        CLOGD2("ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL update failed(%d)", ret);

    /* android.sync static attributes */
    ret = info.update(ANDROID_SYNC_MAX_LATENCY,
            &(sensorStaticInfo->maxLatency), 1);
    if (ret < 0)
        CLOGD2("ANDROID_SYNC_MAX_LATENCY update failed(%d)", ret);

    if (sensorStaticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING) {
        /* Set Stall duration for reprocessing */
        ret = info.update(ANDROID_REPROCESS_MAX_CAPTURE_STALL, &(sensorStaticInfo->maxCaptureStall), 1);
        if (ret < 0)
            CLOGD2("ANDROID_REPROCESS_MAX_CAPTURE_STALL update failed(%d)", ret);
    }

    /* android.request.availableCapabilities */
    i8Vector.clear();
    if (m_createAvailableCapabilities(sensorStaticInfo, &i8Vector) == NO_ERROR) {
        ret = info.update(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, i8Vector.array(), i8Vector.size());
        if (ret < 0)
            CLOGD2("ANDROID_REQUEST_AVAILABLE_CAPABILITIES update failed(%d)", ret);
    }

    /* android.request.availableRequestKeys */
    /* android.request.availableResultKeys */
    /* android.request.availableCharacteristicsKeys */
    Vector<int32_t> requestList, resultList, characteristicsList;
    requestList.clear();
    resultList.clear();
    characteristicsList.clear();
    if (m_createAvailableKeys(sensorStaticInfo, &requestList, &resultList, &characteristicsList, cameraId) == NO_ERROR) {
        ret = info.update(ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS,
                            requestList.array(), requestList.size());
        if (ret < 0)
            CLOGD2("ANDROID_REQUEST_AVAILABLE_REQUEST_KEYS update failed(%d)", ret);

        ret = info.update(ANDROID_REQUEST_AVAILABLE_RESULT_KEYS,
                            resultList.array(), resultList.size());
        if (ret < 0)
            CLOGD2("ANDROID_REQUEST_AVAILABLE_RESULT_KEYS update failed(%d)", ret);

        ret = info.update(ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS,
                            characteristicsList.array(), characteristicsList.size());
        if (ret < 0)
            CLOGD2("ANDROID_REQUEST_AVAILABLE_CHARACTERISTICS_KEYS update failed(%d)", ret);
    }

#ifdef SAMSUNG_TN_FEATURE
    /* Vendor staticInfo*/
    m_constructVendorStaticInfo(sensorStaticInfo, &info, cameraId);
#endif

    *cameraInfo = info.release();

    return OK;
}

void ExynosCameraMetadataConverter::setStaticInfo(int camId, camera_metadata_t *info)
{
    if (info == NULL) {
        camera_metadata_t *meta;
        CLOGW("info is null");
        ExynosCameraMetadataConverter::constructStaticInfo(camId, &meta);
        m_staticInfo = meta;
    } else {
        m_staticInfo = info;
    }
}

status_t ExynosCameraMetadataConverter::initShotData(struct camera2_shot_ext *shot_ext)
{
    CLOGV("IN");

    memset(shot_ext, 0x00, sizeof(struct camera2_shot_ext));

    struct camera2_shot *shot = &(shot_ext->shot);

    // TODO: make this from default request settings
    /* request */
    shot->ctl.request.id = 0;
    shot->ctl.request.metadataMode = METADATA_MODE_FULL;
    shot->ctl.request.frameCount = 0;

    /* lens */
    shot->ctl.lens.focusDistance = -1.0f;
    shot->ctl.lens.aperture = m_sensorStaticInfo->fNumber; // ExifInterface :  TAG_APERTURE = "FNumber";
    shot->ctl.lens.focalLength = m_sensorStaticInfo->focalLength;
    shot->ctl.lens.filterDensity = 0.0f;
    shot->ctl.lens.opticalStabilizationMode = OPTICAL_STABILIZATION_MODE_OFF;

    shot->uctl.lensUd.pos = 0;
    shot->uctl.lensUd.posSize = 0;

    /* aa */
    shot->ctl.aa.vendor_afState = AA_AFSTATE_INACTIVE;
    shot->ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;

    int minFps = m_sensorStaticInfo->minFps;
    int maxFps = m_sensorStaticInfo->maxFps;

    /* The min fps can not be '0'. Therefore it is set up default value '15'. */
    if (minFps == 0) {
        CLOGW("Invalid min fps value(%d)", minFps);
        minFps = 7;
    }
    /*  The initial fps can not be '0' and bigger than '30'. Therefore it is set up default value '30'. */
    if (maxFps == 0 || 30 < maxFps) {
        CLOGW("Invalid max fps value(%d)", maxFps);
        maxFps = 30;
    }

    m_maxFps = maxFps;

    /* sensor */
    shot->ctl.sensor.exposureTime = 0;
    shot->ctl.sensor.frameDuration = (1000 * 1000 * 1000) / maxFps;
    shot->ctl.sensor.sensitivity = 0;

    /* flash */
    shot->ctl.flash.flashMode = CAM2_FLASH_MODE_OFF;
    shot->ctl.flash.firingPower = 0;
    shot->ctl.flash.firingTime = 0;
    shot->uctl.flashMode = CAMERA_FLASH_MODE_OFF;
    m_overrideFlashControl = false;

    /* hotpixel */
    shot->ctl.hotpixel.mode = (enum processing_mode)0;

    /* demosaic */
    shot->ctl.demosaic.mode = (enum demosaic_processing_mode)0;

    /* noise */
    shot->ctl.noise.mode = ::PROCESSING_MODE_FAST;
    shot->ctl.noise.strength = 5;

    /* shading */
    shot->ctl.shading.mode = (enum processing_mode)0;

    /* color */
    shot->ctl.color.mode = COLORCORRECTION_MODE_FAST;
    static const camera_metadata_rational_t colorTransform[9] = {
        {1, 1}, {0, 1}, {0, 1},
        {0, 1}, {1, 1}, {0, 1},
        {0, 1}, {0, 1}, {1, 1},
    };
    memcpy(shot->ctl.color.transform, colorTransform, sizeof(shot->ctl.color.transform));
    shot->ctl.color.vendor_saturation = 3; /* "3" is default. */

    /* tonemap */
    shot->ctl.tonemap.mode = ::TONEMAP_MODE_FAST;
    static const float tonemapCurve[4] = {
        0.f, 0.f,
        1.f, 1.f
    };

    int tonemapCurveSize = sizeof(tonemapCurve);
    int sizeOfCurve = sizeof(shot->ctl.tonemap.curveRed) / sizeof(shot->ctl.tonemap.curveRed[0]);

    for (int i = 0; i < sizeOfCurve; i += 4) {
        memcpy(&(shot->ctl.tonemap.curveRed[i]),   tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveGreen[i]), tonemapCurve, tonemapCurveSize);
        memcpy(&(shot->ctl.tonemap.curveBlue[i]),  tonemapCurve, tonemapCurveSize);
    }

    /* edge */
    shot->ctl.edge.mode = ::PROCESSING_MODE_FAST;
    shot->ctl.edge.strength = 5;

    /* scaler */
    float zoomRatio = 1.0f;
    if (setMetaCtlCropRegion(shot_ext, 0,
                             m_sensorStaticInfo->maxSensorW,
                             m_sensorStaticInfo->maxSensorH,
                             m_sensorStaticInfo->maxPreviewW,
                             m_sensorStaticInfo->maxPreviewH,
                             zoomRatio) != NO_ERROR) {
        CLOGE("m_setZoom() fail");
    }

    /* jpeg */
    shot->ctl.jpeg.quality = 96;
    shot->ctl.jpeg.thumbnailSize[0] = m_sensorStaticInfo->maxThumbnailW;
    shot->ctl.jpeg.thumbnailSize[1] = m_sensorStaticInfo->maxThumbnailH;
    shot->ctl.jpeg.thumbnailQuality = 100;
    shot->ctl.jpeg.gpsCoordinates[0] = 0;
    shot->ctl.jpeg.gpsCoordinates[1] = 0;
    shot->ctl.jpeg.gpsCoordinates[2] = 0;
    memset(&shot->ctl.jpeg.gpsProcessingMethod, 0x0,
        sizeof(shot->ctl.jpeg.gpsProcessingMethod));
    shot->ctl.jpeg.gpsTimestamp = 0L;
    shot->ctl.jpeg.orientation = 0L;

    /* stats */
    shot->ctl.stats.faceDetectMode = ::FACEDETECT_MODE_OFF;
    shot->ctl.stats.histogramMode = ::STATS_MODE_OFF;
    shot->ctl.stats.sharpnessMapMode = ::STATS_MODE_OFF;

    /* aa */
    shot->ctl.aa.captureIntent = ::AA_CAPTURE_INTENT_CUSTOM;
    shot->ctl.aa.mode = ::AA_CONTROL_AUTO;
    shot->ctl.aa.effectMode = ::AA_EFFECT_OFF;
    shot->ctl.aa.sceneMode = ::AA_SCENE_MODE_FACE_PRIORITY;
    shot->ctl.aa.videoStabilizationMode = VIDEO_STABILIZATION_MODE_OFF;

    /* default metering is center */
    shot->ctl.aa.aeMode = ::AA_AEMODE_CENTER;
    shot->ctl.aa.aeRegions[0] = 0;
    shot->ctl.aa.aeRegions[1] = 0;
    shot->ctl.aa.aeRegions[2] = 0;
    shot->ctl.aa.aeRegions[3] = 0;
    shot->ctl.aa.aeRegions[4] = 1000;
    shot->ctl.aa.aeExpCompensation = 0; /* 0 is middle */
    shot->ctl.aa.vendor_aeExpCompensationStep = m_sensorStaticInfo->exposureCompensationStep;
    shot->ctl.aa.aeLock = ::AA_AE_LOCK_OFF;

    shot->ctl.aa.aeTargetFpsRange[0] = minFps;
    shot->ctl.aa.aeTargetFpsRange[1] = maxFps;

    shot->ctl.aa.aeAntibandingMode = ::AA_AE_ANTIBANDING_AUTO;
    shot->ctl.aa.vendor_aeflashMode = ::AA_FLASHMODE_OFF;

    shot->ctl.aa.awbMode = ::AA_AWBMODE_WB_AUTO;
    shot->ctl.aa.awbLock = ::AA_AWB_LOCK_OFF;
    shot->ctl.aa.afMode = ::AA_AFMODE_OFF;
    shot->ctl.aa.afRegions[0] = 0;
    shot->ctl.aa.afRegions[1] = 0;
    shot->ctl.aa.afRegions[2] = 0;
    shot->ctl.aa.afRegions[3] = 0;
    shot->ctl.aa.afRegions[4] = 1000;
    shot->ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;

    shot->ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
    shot->ctl.aa.vendor_isoValue = 0;
    shot->ctl.aa.vendor_videoMode = AA_VIDEOMODE_OFF;

    shot->uctl.opMode = CAMERA_OP_MODE_HAL3_GED;

    /* udm */

    /* magicNumber */
    shot->magicNumber = SHOT_MAGIC_NUMBER;

    for (int i = 0; i < INTERFACE_TYPE_MAX; i++) {
        shot->uctl.scalerUd.mcsc_sub_blk_port[i] = MCSC_PORT_NONE;
    }

    /* default setfile index */
    setMetaSetfile(shot_ext, ISS_SUB_SCENARIO_STILL_PREVIEW);

    /* user request */
    shot_ext->drc_bypass = 1;
    shot_ext->dis_bypass = 1;
    shot_ext->dnr_bypass = 1;
    shot_ext->fd_bypass  = 1;

    initShotVendorData(shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateColorControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_COLOR_CORRECTION_MODE */
    entry = settings->find(ANDROID_COLOR_CORRECTION_MODE);
    if (entry.count > 0) {
        dst->ctl.color.mode = (enum colorcorrection_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_COLOR_CORRECTION_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum colorcorrection_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.color.mode) {
            CLOGD("ANDROID_COLOR_CORRECTION_MODE(%d)", dst->ctl.color.mode);
        }
        isMetaExist = false;
    }

    /* ANDROID_COLOR_CORRECTION_TRANSFORM */
    entry = settings->find(ANDROID_COLOR_CORRECTION_TRANSFORM);
    if (entry.count > 0) {
        for (size_t i = 0; i < entry.count && i < 9; i++) {
            /* Convert rational to float */
            dst->ctl.color.transform[i].num = entry.data.r[i].numerator;
            dst->ctl.color.transform[i].den = entry.data.r[i].denominator;
        }
    }

    /* ANDROID_COLOR_CORRECTION_GAINS */
    entry = settings->find(ANDROID_COLOR_CORRECTION_GAINS);
    if (entry.count > 0) {
        for (size_t i = 0; i < entry.count && i < 4; i++) {
            dst->ctl.color.gains[i] = entry.data.f[i];
        }
        CLOGV("ANDROID_COLOR_CORRECTION_GAINS(%f,%f,%f,%f)",
                entry.data.f[0], entry.data.f[1], entry.data.f[2], entry.data.f[3]);
    }

    /* ANDROID_COLOR_CORRECTION_ABERRATION_MODE */
    entry = settings->find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
    if (entry.count > 0) {
        dst->ctl.color.aberrationCorrectionMode = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_COLOR_CORRECTION_ABERRATION_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum processing_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.color.aberrationCorrectionMode) {
            CLOGD("ANDROID_COLOR_CORRECTION_ABERRATION_MODE(%d)",
                dst->ctl.color.aberrationCorrectionMode);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateDemosaicControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_DEMOSAIC_MODE */
    entry = settings->find(ANDROID_DEMOSAIC_MODE);
    if (entry.count > 0) {
        dst->ctl.demosaic.mode = (enum demosaic_processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_DEMOSAIC_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum demosaic_processing_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.demosaic.mode) {
            CLOGD("ANDROID_DEMOSAIC_MODE(%d)", dst->ctl.demosaic.mode);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateEdgeControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_EDGE_STRENGTH */
    entry = settings->find(ANDROID_EDGE_STRENGTH);
    if (entry.count > 0) {
        dst->ctl.edge.strength = (uint32_t) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_EDGE_STRENGTH);
        if (prev_entry.count > 0 ) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.edge.strength) {
            CLOGD("ANDROID_EDGE_STRENGTH(%d)", dst->ctl.edge.strength);
        }
        isMetaExist = false;
    }

    /* ANDROID_EDGE_MODE */
    entry = settings->find(ANDROID_EDGE_MODE);
    if (entry.count > 0) {
        dst->ctl.edge.mode = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_EDGE_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum processing_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.edge.mode) {
            CLOGD("ANDROID_EDGE_MODE(%d)", dst->ctl.edge.mode);
        }
        isMetaExist = false;
        switch (entry.data.u8[0]) {
        case ANDROID_EDGE_MODE_FAST:
        case ANDROID_EDGE_MODE_HIGH_QUALITY:
            dst->ctl.edge.strength = 5;
            break;
        case ANDROID_EDGE_MODE_OFF:
        case ANDROID_EDGE_MODE_ZERO_SHUTTER_LAG:
            dst->ctl.edge.strength = 2;
            break;
        default:
            break;
        }
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateFlashControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    uint64_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    if (m_overrideFlashControl == true) {
        return OK;
    }

    /* ANDROID_FLASH_FIRING_POWER */
    entry = settings->find(ANDROID_FLASH_FIRING_POWER);
    if (entry.count > 0) {
        dst->ctl.flash.firingPower = (uint32_t) entry.data.u8[0];

        prev_entry = m_prevMeta->find(ANDROID_FLASH_FIRING_POWER);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.flash.firingPower) {
            CLOGD("ANDROID_FLASH_FIRING_POWER(%d)", dst->ctl.flash.firingPower);
        }
        isMetaExist = false;
    }

    /* ANDROID_FLASH_FIRING_TIME */
    entry = settings->find(ANDROID_FLASH_FIRING_TIME);
    if (entry.count > 0 ) {
        dst->ctl.flash.firingTime = (uint64_t) entry.data.i64[0];
        prev_entry = m_prevMeta->find(ANDROID_FLASH_FIRING_TIME);
        if (prev_entry.count > 0) {
            prev_value = (uint64_t) (prev_entry.data.i64[0]);
            isMetaExist = true;
        }
        if (!isMetaExist || prev_value != dst->ctl.flash.firingTime) {
            CLOGD("ANDROID_FLASH_FIRING_TIME(%lld)", (long long)dst->ctl.flash.firingTime);
        }
        isMetaExist = false;
    }

    /* ANDROID_FLASH_MODE */
    entry = settings->find(ANDROID_FLASH_MODE);
    if (entry.count > 0) {
        dst->ctl.flash.flashMode = (enum flash_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_FLASH_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum flash_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.flash.flashMode) {
            CLOGD("ANDROID_FLASH_MODE(%d)", dst->ctl.flash.flashMode);
        }
        isMetaExist = false;

        if (dst->ctl.flash.flashMode == CAM2_FLASH_MODE_TORCH) {
            dst->uctl.flashMode = CAMERA_FLASH_MODE_TORCH;
            m_parameters->setFlashMode(FLASH_MODE_TORCH);
        }

        entry = settings->find(ANDROID_CONTROL_CAPTURE_INTENT);
        if (entry.count > 0) {
            if ((enum aa_capture_intent) entry.data.u8[0] == AA_CAPTURE_INTENT_STILL_CAPTURE
                && dst->ctl.flash.flashMode >= CAM2_FLASH_MODE_SINGLE) {
                m_parameters->setMarkingOfExifFlash(1);
            }
        }
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateHotPixelControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_HOT_PIXEL_MODE */
    entry = settings->find(ANDROID_HOT_PIXEL_MODE);
    if (entry.count > 0) {
        dst->ctl.hotpixel.mode = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_HOT_PIXEL_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.hotpixel.mode) {
            CLOGD("ANDROID_HOT_PIXEL_MODE(%d)", dst->ctl.hotpixel.mode);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateJpegControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    uint64_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_JPEG_GPS_COORDINATES */
    entry = settings->find(ANDROID_JPEG_GPS_COORDINATES);
    if (entry.count > 0) {
        for (size_t i = 0; i < entry.count && i < 3; i++)
            dst->ctl.jpeg.gpsCoordinates[i] = entry.data.d[i];
        CLOGV("ANDROID_JPEG_GPS_COORDINATES(%f,%f,%f)",
                entry.data.d[0], entry.data.d[1], entry.data.d[2]);
    }

    /* ANDROID_JPEG_GPS_PROCESSING_METHOD */
    entry = settings->find(ANDROID_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count > 0) {
        size_t len = entry.count;

        if (len > GPS_PROCESSING_METHOD_SIZE) {
            len = GPS_PROCESSING_METHOD_SIZE;
        }
        strncpy((char *)dst->ctl.jpeg.gpsProcessingMethod, (char *)entry.data.u8, len);
        CLOGV("ANDROID_JPEG_GPS_PROCESSING_METHOD(%s)",
                dst->ctl.jpeg.gpsProcessingMethod);
    }

    /* ANDROID_JPEG_GPS_TIMESTAMP */
    entry = settings->find(ANDROID_JPEG_GPS_TIMESTAMP);
    if (entry.count > 0) {
        dst->ctl.jpeg.gpsTimestamp = (uint64_t) entry.data.i64[0];
        prev_entry = m_prevMeta->find(ANDROID_JPEG_GPS_TIMESTAMP);
        if (prev_entry.count > 0) {
            prev_value = (uint64_t) (prev_entry.data.i64[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.jpeg.gpsTimestamp) {
            CLOGD("ANDROID_JPEG_GPS_TIMESTAMP(%lld)", (long long)dst->ctl.jpeg.gpsTimestamp);
        }
        isMetaExist = false;
    }

    /* ANDROID_JPEG_ORIENTATION */
    entry = settings->find(ANDROID_JPEG_ORIENTATION);
    if (entry.count > 0) {
        dst->ctl.jpeg.orientation = (uint32_t) entry.data.i32[0];
        prev_entry = m_prevMeta->find(ANDROID_JPEG_ORIENTATION);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.i32[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.jpeg.orientation) {
            CLOGD("ANDROID_JPEG_ORIENTATION(%d)", dst->ctl.jpeg.orientation);
        }
        isMetaExist = false;
    }

    /* ANDROID_JPEG_QUALITY */
    entry = settings->find(ANDROID_JPEG_QUALITY);
    if (entry.count > 0) {
        dst->ctl.jpeg.quality = (uint32_t) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_JPEG_QUALITY);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.jpeg.quality) {
            CLOGD("ANDROID_JPEG_QUALITY(%d)", dst->ctl.jpeg.quality);
        }
        isMetaExist = false;
    }

    /* ANDROID_JPEG_THUMBNAIL_QUALITY */
    entry = settings->find(ANDROID_JPEG_THUMBNAIL_QUALITY);
    if (entry.count > 0) {
        dst->ctl.jpeg.thumbnailQuality = (uint32_t) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_JPEG_THUMBNAIL_QUALITY);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.jpeg.thumbnailQuality) {
            CLOGD("ANDROID_JPEG_THUMBNAIL_QUALITY(%d)", dst->ctl.jpeg.thumbnailQuality);
        }
        isMetaExist = false;
    }

    /* ANDROID_JPEG_THUMBNAIL_SIZE */
    entry = settings->find(ANDROID_JPEG_THUMBNAIL_SIZE);
    if (entry.count > 0) {
        for (size_t i = 0; i < entry.count && i < 2; i++) {
            dst->ctl.jpeg.thumbnailSize[i] = (uint32_t) entry.data.i32[i];
        }

        prev_entry = m_prevMeta->find(ANDROID_JPEG_THUMBNAIL_SIZE);
        if (prev_entry.count > 0) {
           isMetaExist = true;
        }

        if (!isMetaExist || (prev_entry.data.i32[0] != entry.data.i32[0]) ||
            (prev_entry.data.i32[1] != entry.data.i32[1])) {
            CLOGD("ANDROID_JPEG_THUMBNAIL_SIZE(%d, %d)", entry.data.i32[0], entry.data.i32[1]);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateLensControlData(CameraMetadata *settings,
                                                                  struct camera2_shot_ext *dst_ext,
#ifndef SAMSUNG_TN_FEATURE
                                                                  __unused
#endif
                                                                  struct CameraMetaParameters *metaParameters)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    float prev_value_f;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_LENS_APERTURE */
    entry = settings->find(ANDROID_LENS_APERTURE);
    if (entry.count > 0) {
        dst->ctl.lens.aperture = (int32_t)(entry.data.f[0] * 100);
        prev_entry = m_prevMeta->find(ANDROID_LENS_APERTURE);
        if (prev_entry.count > 0) {
            prev_value_f = (int32_t)(prev_entry.data.f[0] * 100);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value_f != dst->ctl.lens.aperture) {
            CLOGD("ANDROID_LENS_APERTURE(%d)", dst->ctl.lens.aperture);
        }
        isMetaExist = false;
    }

    /* For IRIS test. Must be removed */
    entry = settings->find(ANDROID_CONTROL_AE_MODE);
    if (entry.count > 0) {
        enum aa_aemode aeMode = AA_AEMODE_OFF;

        aeMode = (enum aa_aemode) FIMC_IS_METADATA(entry.data.u8[0]);

        if (m_aemode != aeMode) {
            if (m_aperture == 150) {
                    m_aperture = 240;
            } else {
                    m_aperture = 150;
            }
            m_aemode = aeMode;
	}
    }

    dst->ctl.lens.aperture = m_aperture;

    /* ANDROID_LENS_FILTER_DENSITY */
    entry = settings->find(ANDROID_LENS_FILTER_DENSITY);
    if (entry.count > 0) {
        dst->ctl.lens.filterDensity = entry.data.f[0];
        prev_entry = m_prevMeta->find(ANDROID_LENS_FILTER_DENSITY);
        if (prev_entry.count > 0) {
            prev_value_f = prev_entry.data.f[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value_f != dst->ctl.lens.filterDensity) {
            CLOGD("ANDROID_LENS_FILTER_DENSITY(%f)", dst->ctl.lens.filterDensity);
        }
        isMetaExist = false;
    }

    /* ANDROID_LENS_FOCAL_LENGTH */
    entry = settings->find(ANDROID_LENS_FOCAL_LENGTH);
    if (entry.count > 0) {
        dst->ctl.lens.focalLength = entry.data.f[0];
        prev_entry = m_prevMeta->find(ANDROID_LENS_FOCAL_LENGTH);
        if (prev_entry.count > 0) {
            prev_value_f = prev_entry.data.f[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value_f != dst->ctl.lens.focalLength) {
            CLOGD("ANDROID_LENS_FOCAL_LENGTH(%f)", dst->ctl.lens.focalLength);
        }
        isMetaExist = false;
    }

    /* ANDROID_LENS_FOCUS_DISTANCE */
    entry = settings->find(ANDROID_LENS_FOCUS_DISTANCE);
    if (entry.count > 0) {
        /* should not control afMode and focusDistance at the same time
        should not set the same focusDistance continuously
        set the -1 to focusDistance if you do not need to change focusDistance
        */
        if (m_afMode != AA_AFMODE_OFF || m_afMode != m_preAfMode || m_focusDistance == entry.data.f[0]) {
            dst->ctl.lens.focusDistance = -1;
        } else {
            if (entry.data.f[0] <= m_sensorStaticInfo->minimumFocusDistance) {
                dst->ctl.lens.focusDistance = entry.data.f[0];
            } else {
                dst->ctl.lens.focusDistance = m_sensorStaticInfo->minimumFocusDistance;
            }
        }
        m_focusDistance = dst->ctl.lens.focusDistance;
        prev_entry = m_prevMeta->find(ANDROID_LENS_FOCUS_DISTANCE);
        if (prev_entry.count > 0) {
            prev_value_f = prev_entry.data.f[0];
            isMetaExist = true;
        }
        if (!isMetaExist || prev_value_f != entry.data.f[0]) {
            CLOGD("ANDROID_LENS_FOCUS_DISTANCE(%f)", entry.data.f[0]);
        }
        isMetaExist = false;
    }

    /* ANDROID_LENS_OPTICAL_STABILIZATION_MODE */
    entry = settings->find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE);
    if (entry.count > 0) {
        uint8_t ois_mode = (uint8_t)entry.data.u8[0];

        prev_entry = m_prevMeta->find(ANDROID_LENS_OPTICAL_STABILIZATION_MODE);
        if (prev_entry.count > 0) {
            prev_value = (uint8_t)prev_entry.data.u8[0];
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != ois_mode) {
            CLOGD("ANDROID_LENS_OPTICAL_STABILIZATION_MODE(%d)", ois_mode);
        }
        isMetaExist = false;

        switch (ois_mode) {
        case ANDROID_LENS_OPTICAL_STABILIZATION_MODE_ON:
            dst->ctl.lens.opticalStabilizationMode = OPTICAL_STABILIZATION_MODE_STILL;
            break;
        case ANDROID_LENS_OPTICAL_STABILIZATION_MODE_OFF:
        default:
            dst->ctl.lens.opticalStabilizationMode = OPTICAL_STABILIZATION_MODE_CENTERING;
            break;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    translateVendorLensControlData(settings, dst_ext, metaParameters);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateNoiseControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_NOISE_REDUCTION_STRENGTH */
    entry = settings->find(ANDROID_NOISE_REDUCTION_STRENGTH);
    if (entry.count > 0) {
        dst->ctl.noise.strength = (uint32_t) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_NOISE_REDUCTION_STRENGTH);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.noise.strength) {
            CLOGD("ANDROID_NOISE_REDUCTION_STRENGTH(%d)", dst->ctl.noise.strength);
        }
        isMetaExist = false;
    }

    /* ANDROID_NOISE_REDUCTION_MODE */
    entry = settings->find(ANDROID_NOISE_REDUCTION_MODE);
    if (entry.count > 0) {
        dst->ctl.noise.mode = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_NOISE_REDUCTION_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum processing_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.noise.mode) {
            CLOGD("ANDROID_NOISE_REDUCTION_MODE(%d)", dst->ctl.noise.mode);
        }
        isMetaExist = false;

        switch (entry.data.u8[0]) {
        case ANDROID_NOISE_REDUCTION_MODE_FAST:
        case ANDROID_NOISE_REDUCTION_MODE_HIGH_QUALITY:
            dst->ctl.noise.strength = 5;
            break;
        case ANDROID_NOISE_REDUCTION_MODE_OFF:
        case ANDROID_NOISE_REDUCTION_MODE_MINIMAL:
        case ANDROID_NOISE_REDUCTION_MODE_ZERO_SHUTTER_LAG:
            dst->ctl.noise.strength = 2;
            break;
        default:
            break;
        }
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateRequestControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext, int *reqId)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    uint32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_REQUEST_ID */
    entry = settings->find(ANDROID_REQUEST_ID);
    if (entry.count > 0) {
        dst->ctl.request.id = (uint32_t) entry.data.i32[0];
        prev_entry = m_prevMeta->find(ANDROID_REQUEST_ID);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.i32[0]);
            isMetaExist = true;
        }
        if (!isMetaExist || prev_value != dst->ctl.request.id) {
            CLOGD("ANDROID_REQUEST_ID(%d)", dst->ctl.request.id);
        }
        isMetaExist = false;

        if (reqId != NULL)
            *reqId = dst->ctl.request.id;
    }

    /* ANDROID_REQUEST_METADATA_MODE */
    entry = settings->find(ANDROID_REQUEST_METADATA_MODE);
    if (entry.count > 0) {
        dst->ctl.request.metadataMode = (enum metadata_mode) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_REQUEST_METADATA_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum metadata_mode) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.request.metadataMode) {
            CLOGD("ANDROID_REQUEST_METADATA_MODE(%d)", dst->ctl.request.metadataMode);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateScalerControlData(CameraMetadata *settings,
                                                                       struct camera2_shot_ext *dst_ext,
                                                                       struct CameraMetaParameters *metaParameters)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_SCALER_CROP_REGION */
    entry = settings->find(ANDROID_SCALER_CROP_REGION);
    if (entry.count > 0) {
        int maxSensorW  = 0, maxSensorH  = 0;
        float newZoomRatio = 0.0f;
        float maxZoomRatio = m_parameters->getMaxZoomRatio() / 1000.0f;

        for (size_t i = 0; i < entry.count && i < 4; i++) {
            dst->ctl.scaler.cropRegion[i] = (uint32_t) entry.data.i32[i];
        }
        CLOGV("ANDROID_SCALER_CROP_REGION(%d,%d,%d,%d)",
                entry.data.i32[0], entry.data.i32[1],
                entry.data.i32[2], entry.data.i32[3]);

        m_parameters->getMaxSensorSize(&maxSensorW, &maxSensorH);
        newZoomRatio = (float)(maxSensorW) / (float)(dst->ctl.scaler.cropRegion[2]);
        if (newZoomRatio > maxZoomRatio) {
            int cropRegionMinW = 0, cropRegionMinH = 0;

            cropRegionMinW = ceil((float)maxSensorW / maxZoomRatio);
            cropRegionMinH = ceil((float)maxSensorH / maxZoomRatio);
            dst->ctl.scaler.cropRegion[2] = cropRegionMinW;
            dst->ctl.scaler.cropRegion[3] = cropRegionMinH;
            newZoomRatio = maxZoomRatio;

            CLOGW("invalid CROP_REGION(%d,%d,%d,%d), change CROP_REGION(%d,%d,%d,%d)",
                    entry.data.i32[0], entry.data.i32[1],
                    entry.data.i32[2], entry.data.i32[3],
                    dst->ctl.scaler.cropRegion[0],
                    dst->ctl.scaler.cropRegion[1],
                    dst->ctl.scaler.cropRegion[2],
                    dst->ctl.scaler.cropRegion[3]);
        }
        CLOGV("MaxSensorSize(%dx%d) newZoomRatio(%f)",
                maxSensorW, maxSensorH, newZoomRatio);
        metaParameters->m_zoomRatio = newZoomRatio;
    }else {
        metaParameters->m_zoomRatio = 1.0f;
    }

#ifdef SAMSUNG_TN_FEATURE
    translateVendorScalerControlData(settings, dst_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateSensorControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    uint64_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_SENSOR_EXPOSURE_TIME */
    if (m_isManualAeControl == true) {
        entry = settings->find(ANDROID_SENSOR_EXPOSURE_TIME);
        if (entry.count > 0) {
            dst->ctl.sensor.exposureTime = (uint64_t) entry.data.i64[0];
            prev_entry = m_prevMeta->find(ANDROID_SENSOR_EXPOSURE_TIME);
            if (prev_entry.count > 0) {
                prev_value = (uint64_t) (prev_entry.data.i64[0]);
                isMetaExist = true;
            }

            if (!isMetaExist || prev_value != dst->ctl.sensor.exposureTime) {
                CLOGD("ANDROID_SENSOR_EXPOSURE_TIME(%lld)",
                    (long long)dst->ctl.sensor.exposureTime);
            }
            isMetaExist = false;
        }
    }

    /* ANDROID_SENSOR_FRAME_DURATION */
    if (m_isManualAeControl == true) {
        entry = settings->find(ANDROID_SENSOR_FRAME_DURATION);
        if (entry.count > 0) {
            uint64_t frameDuration = 0L;
            uint64_t exposureTime = 0L;
            frameDuration = (uint64_t) entry.data.i64[0];

            entry = settings->find(ANDROID_SENSOR_EXPOSURE_TIME);
            if (entry.count > 0) {
                exposureTime = (uint64_t) entry.data.i64[0];
            }
            if (frameDuration == 0L || frameDuration < exposureTime) {
                frameDuration = exposureTime;
            }

            dst->ctl.sensor.frameDuration = frameDuration;
            prev_entry = m_prevMeta->find(ANDROID_SENSOR_FRAME_DURATION);
            if (prev_entry.count > 0) {
                prev_value = (uint64_t) (prev_entry.data.i64[0]);
                isMetaExist = true;
            }

            if (!isMetaExist || prev_value != dst->ctl.sensor.frameDuration) {
                CLOGD("ANDROID_SENSOR_FRAME_DURATION(%lld)",
                    (long long)dst->ctl.sensor.frameDuration);
            }
            isMetaExist = false;
        } else {
            /* default value */
            dst->ctl.sensor.frameDuration = (1000 * 1000 * 1000) / m_maxFps;
        }
    } else {
        /* default value */
        dst->ctl.sensor.frameDuration = (1000 * 1000 * 1000) / m_maxFps;
    }

    /* ANDROID_SENSOR_SENSITIVITY */
    if (m_isManualAeControl == true) {
        entry = settings->find(ANDROID_SENSOR_SENSITIVITY);
        if (entry.count > 0) {
            dst->ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
            dst->ctl.sensor.sensitivity = (uint32_t) entry.data.i32[0];
            dst->ctl.aa.vendor_isoValue = (uint32_t) entry.data.i32[0];
            prev_entry = m_prevMeta->find(ANDROID_SENSOR_SENSITIVITY);
            if (prev_entry.count > 0) {
                prev_value = (uint32_t) (prev_entry.data.i32[0]);
                isMetaExist = true;
            }
            if (!isMetaExist || prev_value != dst->ctl.sensor.sensitivity) {
                CLOGD("ANDROID_SENSOR_SENSITIVITY(%d)",
                     dst->ctl.sensor.sensitivity);
            }
            isMetaExist = false;
        } else {
            dst->ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            dst->ctl.sensor.sensitivity = 0;
            dst->ctl.aa.vendor_isoValue = 0;
        }
    } else {
        dst->ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        dst->ctl.sensor.sensitivity = 0;
        dst->ctl.aa.vendor_isoValue = 0;
    }

    /* ANDROID_SENSOR_TEST_PATTERN_DATA */
    entry = settings->find(ANDROID_SENSOR_TEST_PATTERN_DATA);
    if (entry.count > 0) {
        for (size_t i = 0; i < entry.count && i < 4; i++)
            dst->ctl.sensor.testPatternData[i] = entry.data.i32[i];
        CLOGV("ANDROID_SENSOR_TEST_PATTERN_DATA(%d,%d,%d,%d)",
                entry.data.i32[0], entry.data.i32[1], entry.data.i32[2], entry.data.i32[3]);
    }

    /* ANDROID_SENSOR_TEST_PATTERN_MODE */
    entry = settings->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
    if (entry.count > 0) {
        /* TODO : change SENSOR_TEST_PATTERN_MODE_CUSTOM1 from 256 to 267 */
        if (entry.data.i32[0] == ANDROID_SENSOR_TEST_PATTERN_MODE_CUSTOM1)
            dst->ctl.sensor.testPatternMode = SENSOR_TEST_PATTERN_MODE_CUSTOM1;
        else
            dst->ctl.sensor.testPatternMode = (enum sensor_test_pattern_mode) FIMC_IS_METADATA(entry.data.i32[0]);

        prev_entry = m_prevMeta->find(ANDROID_SENSOR_TEST_PATTERN_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum sensor_test_pattern_mode) FIMC_IS_METADATA(prev_entry.data.i32[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.sensor.testPatternMode) {
            CLOGD("ANDROID_SENSOR_TEST_PATTERN_MODE(%d)",
                dst->ctl.sensor.testPatternMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_SENSOR_TIMESTAMP : Get sensor result meta for ZSL_INPUT */
    entry = settings->find(ANDROID_SENSOR_TIMESTAMP);
    if (entry.count > 0) {
        dst->udm.sensor.timeStampBoot = (uint64_t)entry.data.i64[0];
        dst->dm.request.frameCount = m_getFrameInfoForTimeStamp(FRAMECOUNT, dst->udm.sensor.timeStampBoot);
        dst->dm.lens.aperture = m_getFrameInfoForTimeStamp(APERTURE, dst->udm.sensor.timeStampBoot);
        CLOGD("ANDROID_SENSOR_TIMESTAMP(%llu)",
                (unsigned long long)dst->udm.sensor.timeStampBoot);
        CLOGD("dst->dm.request.frameCount(%d), dst->dm.lens.aperture(%d)",
                dst->dm.request.frameCount, dst->dm.lens.aperture);
    }

    /* ANDROID_SENSOR_EXPOSURE_TIME */
    entry = settings->find(ANDROID_SENSOR_EXPOSURE_TIME);
    if (entry.count > 0) {
        dst->dm.sensor.exposureTime = (uint64_t)entry.data.i64[0];
        CLOGV("ANDROID_SENSOR_EXPOSURE_TIME(%llu)",
                (unsigned long long)dst->dm.sensor.exposureTime);
    }

    /* ANDROID_SENSOR_SENSITIVITY */
    entry = settings->find(ANDROID_SENSOR_SENSITIVITY);
    if (entry.count > 0) {
        dst->dm.sensor.sensitivity = entry.data.i32[0];
        CLOGV("ANDROID_SENSOR_SENSITIVITY(%d)",
                dst->dm.sensor.sensitivity);
    }

#ifdef SAMSUNG_TN_FEATURE
    translateVendorSensorControlData(settings, dst_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateShadingControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_SHADING_MODE */
    entry = settings->find(ANDROID_SHADING_MODE);
    if (entry.count > 0) {
        dst->ctl.shading.mode = (enum processing_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_SHADING_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum processing_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.shading.mode) {
            CLOGD("ANDROID_SHADING_MODE(%d)", dst->ctl.shading.mode);
        }
        isMetaExist = false;
    }

    /* ANDROID_SHADING_STRENGTH */
    entry = settings->find(ANDROID_SHADING_STRENGTH);
    if (entry.count > 0) {
        dst->ctl.shading.strength = (uint32_t) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_SHADING_STRENGTH);
        if (prev_entry.count > 0) {
            prev_value = (uint32_t) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.shading.strength) {
            CLOGD("ANDROID_SHADING_STRENGTH(%d)",
                dst->ctl.shading.strength);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateStatisticsControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_STATISTICS_FACE_DETECT_MODE */
    entry = settings->find(ANDROID_STATISTICS_FACE_DETECT_MODE);
    if (entry.count > 0) {
        dst->ctl.stats.faceDetectMode = (enum facedetect_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        dst_ext->fd_bypass = (dst->ctl.stats.faceDetectMode == FACEDETECT_MODE_OFF) ? 1 : 0;

        if (m_parameters->getHfdMode() == true
            && m_parameters->getRecordingHint() == false) {
            dst_ext->hfd.hfd_enable = !(dst_ext->fd_bypass);
        }

        prev_entry = m_prevMeta->find(ANDROID_STATISTICS_FACE_DETECT_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum facedetect_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.stats.faceDetectMode) {
            CLOGD("ANDROID_STATISTICS_FACE_DETECT_MODE(%d)",
                dst->ctl.stats.faceDetectMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_STATISTICS_HISTOGRAM_MODE */
    entry = settings->find(ANDROID_STATISTICS_HISTOGRAM_MODE);
    if (entry.count > 0) {
        dst->ctl.stats.histogramMode = (enum stats_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_STATISTICS_HISTOGRAM_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum stats_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.stats.histogramMode) {
            CLOGD("ANDROID_STATISTICS_HISTOGRAM_MODE(%d)",
                dst->ctl.stats.histogramMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_STATISTICS_SHARPNESS_MAP_MODE */
    entry = settings->find(ANDROID_STATISTICS_SHARPNESS_MAP_MODE);
    if (entry.count > 0) {
        dst->ctl.stats.sharpnessMapMode = (enum stats_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_STATISTICS_SHARPNESS_MAP_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum stats_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.stats.sharpnessMapMode) {
            CLOGD("ANDROID_STATISTICS_SHARPNESS_MAP_MODE(%d)",
                dst->ctl.stats.sharpnessMapMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE */
    entry = settings->find(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE);
    if (entry.count > 0) {
        dst->ctl.stats.hotPixelMapMode = (enum stats_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum stats_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.stats.hotPixelMapMode) {
            CLOGD("ANDROID_STATISTICS_HOT_PIXEL_MAP_MODE(%d)",
                dst->ctl.stats.hotPixelMapMode);
        }
        isMetaExist = false;
    }

    /* ANDROID_STATISTICS_LENS_SHADING_MAP_MODE */
    entry = settings->find(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE);
    if (entry.count > 0) {
        dst->ctl.stats.lensShadingMapMode = (enum stats_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_STATISTICS_LENS_SHADING_MAP_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum stats_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.stats.lensShadingMapMode) {
            CLOGD("ANDROID_STATISTICS_LENS_SHADING_MAP_MODE(%d)",
                dst->ctl.stats.lensShadingMapMode);
        }
        isMetaExist = false;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateTonemapControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_TONEMAP_MODE */
    entry = settings->find(ANDROID_TONEMAP_MODE);
    if (entry.count > 0) {
        dst->ctl.tonemap.mode = (enum tonemap_mode) FIMC_IS_METADATA(entry.data.u8[0]);
        prev_entry = m_prevMeta->find(ANDROID_TONEMAP_MODE);
        if (prev_entry.count > 0) {
            prev_value = (enum tonemap_mode) FIMC_IS_METADATA(prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.tonemap.mode) {
            CLOGD("ANDROID_TONEMAP_MODE(%d)", dst->ctl.tonemap.mode);
        }
        isMetaExist = false;
    }

    if(dst->ctl.tonemap.mode == TONEMAP_MODE_CONTRAST_CURVE) {
        /* ANDROID_TONEMAP_CURVE_BLUE */
        entry = settings->find(ANDROID_TONEMAP_CURVE_BLUE);
        if (entry.count > 0) {
            float tonemapCurveBlue[64];

            if (entry.count < 64) {
                if (entry.count == 4) {
                    float deltaIn, deltaOut;

                    deltaIn = entry.data.f[2] - entry.data.f[0];
                    deltaOut = entry.data.f[3] - entry.data.f[1];
                    for (size_t i = 0; i < 61; i += 2) {
                        tonemapCurveBlue[i] = deltaIn * i / 64.0 + entry.data.f[0];
                        tonemapCurveBlue[i+1] = deltaOut * i / 64.0 + entry.data.f[1];
                        CLOGV("ANDROID_TONEMAP_CURVE_BLUE([%zu]:%f)", i, tonemapCurveBlue[i]);
                    }
                    tonemapCurveBlue[62] = entry.data.f[2];
                    tonemapCurveBlue[63] = entry.data.f[3];
                } else if (entry.count == 32) {
                    size_t i;
                    for (i = 0; i < 30; i += 2) {
                        tonemapCurveBlue[2*i] = entry.data.f[i];
                        tonemapCurveBlue[2*i+1] = entry.data.f[i+1];
                        tonemapCurveBlue[2*i+2] = (entry.data.f[i] + entry.data.f[i+2])/2;
                        tonemapCurveBlue[2*i+3] = (entry.data.f[i+1] + entry.data.f[i+3])/2;
                    }
                    i = 30;
                    tonemapCurveBlue[2*i] = entry.data.f[i];
                    tonemapCurveBlue[2*i+1] = entry.data.f[i+1];
                    tonemapCurveBlue[2*i+2] = entry.data.f[i];
                    tonemapCurveBlue[2*i+3] = entry.data.f[i+1];
                } else {
                    CLOGE("ANDROID_TONEMAP_CURVE_BLUE( entry count : %zu)", entry.count);
                }
            } else {
                for (size_t i = 0; i < entry.count && i < 64; i++) {
                    tonemapCurveBlue[i] = entry.data.f[i];
                    CLOGV("ANDROID_TONEMAP_CURVE_BLUE([%zu]:%f)", i, entry.data.f[i]);
                }
            }
            memcpy(&(dst->ctl.tonemap.curveBlue[0]), tonemapCurveBlue, sizeof(float)*64);
        }

        /* ANDROID_TONEMAP_CURVE_GREEN */
        entry = settings->find(ANDROID_TONEMAP_CURVE_GREEN);
        if (entry.count > 0) {
            float tonemapCurveGreen[64];

            if (entry.count < 64) {
                if (entry.count == 4) {
                    float deltaIn, deltaOut;

                    deltaIn = entry.data.f[2] - entry.data.f[0];
                    deltaOut = entry.data.f[3] - entry.data.f[1];
                    for (size_t i = 0; i < 61; i += 2) {
                        tonemapCurveGreen[i] = deltaIn * i / 64.0 + entry.data.f[0];
                        tonemapCurveGreen[i+1] = deltaOut * i / 64.0 + entry.data.f[1];
                        CLOGV("ANDROID_TONEMAP_CURVE_GREEN([%zu]:%f)", i, tonemapCurveGreen[i]);
                    }
                    tonemapCurveGreen[62] = entry.data.f[2];
                    tonemapCurveGreen[63] = entry.data.f[3];
                } else if (entry.count == 32) {
                    size_t i;
                    for (i = 0; i < 30; i += 2) {
                        tonemapCurveGreen[2*i] = entry.data.f[i];
                        tonemapCurveGreen[2*i+1] = entry.data.f[i+1];
                        tonemapCurveGreen[2*i+2] = (entry.data.f[i] + entry.data.f[i+2])/2;
                        tonemapCurveGreen[2*i+3] = (entry.data.f[i+1] + entry.data.f[i+3])/2;
                    }
                    i = 30;
                    tonemapCurveGreen[2*i] = entry.data.f[i];
                    tonemapCurveGreen[2*i+1] = entry.data.f[i+1];
                    tonemapCurveGreen[2*i+2] = entry.data.f[i];
                    tonemapCurveGreen[2*i+3] = entry.data.f[i+1];
                } else {
                    CLOGE("ANDROID_TONEMAP_CURVE_GREEN( entry count : %zu)", entry.count);
                }
            } else {
                for (size_t i = 0; i < entry.count && i < 64; i++) {
                    tonemapCurveGreen[i] = entry.data.f[i];
                    CLOGV("ANDROID_TONEMAP_CURVE_GREEN([%zu]:%f)", i, entry.data.f[i]);
                }
            }
            memcpy(&(dst->ctl.tonemap.curveGreen[0]), tonemapCurveGreen, sizeof(float)*64);
        }

        /* ANDROID_TONEMAP_CURVE_RED */
        entry = settings->find(ANDROID_TONEMAP_CURVE_RED);
        if (entry.count > 0) {
            float tonemapCurveRed[64];

            if (entry.count < 64) {
                if (entry.count == 4) {
                    float deltaIn, deltaOut;

                    deltaIn = entry.data.f[2] - entry.data.f[0];
                    deltaOut = entry.data.f[3] - entry.data.f[1];
                    for (size_t i = 0; i < 61; i += 2) {
                        tonemapCurveRed[i] = deltaIn * i / 64.0 + entry.data.f[0];
                        tonemapCurveRed[i+1] = deltaOut * i / 64.0 + entry.data.f[1];
                        CLOGV("ANDROID_TONEMAP_CURVE_RED([%zu]:%f)", i, tonemapCurveRed[i]);
                    }
                    tonemapCurveRed[62] = entry.data.f[2];
                    tonemapCurveRed[63] = entry.data.f[3];
                } else if (entry.count == 32) {
                    size_t i;
                    for (i = 0; i < 30; i += 2) {
                        tonemapCurveRed[2*i] = entry.data.f[i];
                        tonemapCurveRed[2*i+1] = entry.data.f[i+1];
                        tonemapCurveRed[2*i+2] = (entry.data.f[i] + entry.data.f[i+2])/2;
                        tonemapCurveRed[2*i+3] = (entry.data.f[i+1] + entry.data.f[i+3])/2;
                    }
                    i = 30;
                    tonemapCurveRed[2*i] = entry.data.f[i];
                    tonemapCurveRed[2*i+1] = entry.data.f[i+1];
                    tonemapCurveRed[2*i+2] = entry.data.f[i];
                    tonemapCurveRed[2*i+3] = entry.data.f[i+1];
                } else {
                    CLOGE("ANDROID_TONEMAP_CURVE_RED( entry count : %zu)", entry.count);
                }
            } else {
                for (size_t i = 0; i < entry.count && i < 64; i++) {
                    tonemapCurveRed[i] = entry.data.f[i];
                    CLOGV("ANDROID_TONEMAP_CURVE_RED([%zu]:%f)", i, entry.data.f[i]);
                }
            }
            memcpy(&(dst->ctl.tonemap.curveRed[0]), tonemapCurveRed, sizeof(float)*64);
        }
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateLedControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_LED_TRANSMIT */
    if (m_sensorStaticInfo->leds != NULL) {
        entry = settings->find(ANDROID_LED_TRANSMIT);
        if (entry.count > 0) {
            dst->ctl.led.transmit = (enum led_transmit) entry.data.u8[0];
            prev_entry = m_prevMeta->find(ANDROID_LED_TRANSMIT);
            if (prev_entry.count > 0) {
                prev_value = (enum led_transmit) (prev_entry.data.u8[0]);
                isMetaExist = true;
            }

            if (!isMetaExist || prev_value != dst->ctl.led.transmit) {
                CLOGD("ANDROID_LED_TRANSMIT(%d)", dst->ctl.led.transmit);
            }
            isMetaExist = false;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    translateVendorLedControlData(settings, dst_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateBlackLevelControlData(CameraMetadata *settings, struct camera2_shot_ext *dst_ext)
{
    struct camera2_shot *dst = NULL;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t prev_entry;
    int32_t prev_value;
    bool isMetaExist = false;

    dst = &dst_ext->shot;
    dst->magicNumber = SHOT_MAGIC_NUMBER;

    /* ANDROID_BLACK_LEVEL_LOCK */
    entry = settings->find(ANDROID_BLACK_LEVEL_LOCK);
    if (entry.count > 0) {
        dst->ctl.blacklevel.lock = (enum blacklevel_lock) entry.data.u8[0];
        prev_entry = m_prevMeta->find(ANDROID_BLACK_LEVEL_LOCK);
        if (prev_entry.count > 0) {
            prev_value = (enum blacklevel_lock) (prev_entry.data.u8[0]);
            isMetaExist = true;
        }

        if (!isMetaExist || prev_value != dst->ctl.blacklevel.lock) {
            CLOGD("ANDROID_BLACK_LEVEL_LOCK(%d)", dst->ctl.blacklevel.lock);
        }
        isMetaExist = false;
    }

    return OK;
}

void ExynosCameraMetadataConverter::setPreviousMeta(CameraMetadata *meta)
{
    m_prevMeta = meta;
}

status_t ExynosCameraMetadataConverter::convertRequestToShot(ExynosCameraRequestSP_sprt_t request, int *reqId)
{
    status_t ret = OK;
    uint32_t errorFlag = 0;
    struct camera2_shot_ext *dst_ext = NULL;
    CameraMetadata *meta;
    struct CameraMetaParameters *metaParameters = NULL;
    request->setRequestLock();

    meta = request->getServiceMeta();
    dst_ext = request->getServiceShot();
    metaParameters = request->getMetaParameters();

    if (meta->isEmpty()) {
        CLOGE("Settings is NULL!!");
        request->setRequestUnlock();
        return BAD_VALUE;
    }
    if (dst_ext == NULL) {
        CLOGE("dst_ext is NULL!!");
        request->setRequestUnlock();
        return BAD_VALUE;
    }

    if (metaParameters == NULL) {
        CLOGE("metaParameters is NULL!!");
        request->setRequestUnlock();
        return BAD_VALUE;
    }
    initShotData(dst_ext);

    ret = translateColorControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 0);
    ret = translateControlControlData(meta, dst_ext, metaParameters);
    if (ret != OK)
        errorFlag |= (1 << 1);
    ret = translateDemosaicControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 2);
    ret = translateEdgeControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 3);
    ret = translateFlashControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 4);
    ret = translateHotPixelControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 5);
    ret = translateJpegControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 6);
    ret = translateScalerControlData(meta, dst_ext, metaParameters);
    if (ret != OK)
        errorFlag |= (1 << 7);
    ret = translateLensControlData(meta, dst_ext, metaParameters);
    if (ret != OK)
        errorFlag |= (1 << 8);
    ret = translateNoiseControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 9);
    ret = translateRequestControlData(meta, dst_ext, reqId);
    if (ret != OK)
        errorFlag |= (1 << 10);
    ret = translateSensorControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 11);
    ret = translateShadingControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 12);
    ret = translateStatisticsControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 13);
    ret = translateTonemapControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 14);
    ret = translateLedControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 15);
    ret = translateBlackLevelControlData(meta, dst_ext);
    if (ret != OK)
        errorFlag |= (1 << 16);

    request->setRequestUnlock();

    if (errorFlag != 0) {
        CLOGE("failed to translate Control Data(%d)", errorFlag);
        return INVALID_OPERATION;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::translateColorMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    const uint8_t colorMode = (uint8_t) CAMERA_METADATA(src->dm.color.mode);
    settings->update(ANDROID_COLOR_CORRECTION_MODE, &colorMode, 1);
    CLOGV("dm.color.mode(%d)", src->dm.color.mode);

    camera_metadata_rational_t colorTransform[9];
    for (int i = 0; i < 9; i++) {
        colorTransform[i].numerator = (int32_t) src->dm.color.transform[i].num;
        colorTransform[i].denominator = (int32_t) src->dm.color.transform[i].den;
    }
    settings->update(ANDROID_COLOR_CORRECTION_TRANSFORM, colorTransform, 9);
    CLOGV("dm.color.transform");

    float colorGains[4];
    for (int i = 0; i < 4; i++) {
        colorGains[i] = src->dm.color.gains[i];
    }
    settings->update(ANDROID_COLOR_CORRECTION_GAINS, colorGains, 4);
    CLOGV("dm.color.gains(%f,%f,%f,%f)",
            colorGains[0], colorGains[1], colorGains[2], colorGains[3]);

    const uint8_t aberrationMode = (uint8_t) CAMERA_METADATA(src->dm.color.aberrationCorrectionMode);
    settings->update(ANDROID_COLOR_CORRECTION_ABERRATION_MODE, &aberrationMode, 1);
    CLOGV("dm.color.aberrationCorrectionMode(%d)",
            src->dm.color.aberrationCorrectionMode);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateControlMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

#ifdef SAMSUNG_TN_FEATURE
    translateVendorControlMetaData(settings, shot_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateEdgeMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateFlashMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    uint8_t flashState = ANDROID_FLASH_STATE_READY;
    if (m_flashMgr == NULL)
        flashState = ANDROID_FLASH_STATE_UNAVAILABLE;
    else if (m_sensorStaticInfo->flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_FALSE)
        flashState = ANDROID_FLASH_STATE_UNAVAILABLE;
    else
        flashState = src->dm.flash.flashState;
    settings->update(ANDROID_FLASH_STATE, &flashState , 1);
    CLOGV("flashState=%d", flashState);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateHotPixelMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateJpegMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

#ifdef SAMSUNG_TN_FEATURE
    translateVendorJpegMetaData(settings);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateLensMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    const uint8_t lensState = src->dm.lens.state;
    settings->update(ANDROID_LENS_STATE, &lensState, 1);
    CLOGV("dm.lens.state(%d)", src->dm.lens.state);

    const float focusRange[2] =
    { src->dm.lens.focusRange[0], src->dm.lens.focusRange[1] };
    settings->update(ANDROID_LENS_FOCUS_RANGE, focusRange, 2);
    CLOGV("dm.lens.focusRange(%f,%f)",
            focusRange[0], focusRange[1]);

    /* Focus distance 0 means infinite */
    float focusDistance = src->dm.lens.focusDistance;
    if(focusDistance < 0) {
        focusDistance = 0;
    } else if (focusDistance > m_sensorStaticInfo->minimumFocusDistance) {
        focusDistance = m_sensorStaticInfo->minimumFocusDistance;
    }
    settings->update(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);
    CLOGV("dm.lens.focusDistance(%f)", src->dm.lens.focusDistance);

    float aperture = ((float) src->dm.lens.aperture / 100);
    settings->update(ANDROID_LENS_APERTURE, &aperture, 1);
    CLOGV("dm.lens.aperture(%d)", src->dm.lens.aperture);

#ifdef SAMSUNG_TN_FEATURE
    translateVendorLensMetaData(settings, shot_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateNoiseMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateRequestMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

/*
 *
 * pipelineDepth is filed of 'REQUEST'
 *
 * but updating pipelineDepth data can be conflict
 * and we separeted this data not using data but request's private data
 *
 * remaining this code as comment is that to prevent missing update pieplineDepth data in the medta of 'REQUEST' field
 *
 */
/*
 *   const uint8_t pipelineDepth = src->dm.request.pipelineDepth;
 *   settings.update(ANDROID_REQUEST_PIPELINE_DEPTH, &pipelineDepth, 1);
 *   CLOGV("ANDROID_REQUEST_PIPELINE_DEPTH(%d)", pipelineDepth);
 */

    return OK;
}

status_t ExynosCameraMetadataConverter::translateScalerMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();

    src = &(shot_ext->shot);

    const int32_t cropRegion[4] =
    {
        static_cast<int32_t>(src->ctl.scaler.cropRegion[0]),
        static_cast<int32_t>(src->ctl.scaler.cropRegion[1]),
        static_cast<int32_t>(src->ctl.scaler.cropRegion[2]),
        static_cast<int32_t>(src->ctl.scaler.cropRegion[3])
    };
    settings->update(ANDROID_SCALER_CROP_REGION, cropRegion, 4);
    CLOGV("dm.scaler.cropRegion(%d,%d,%d,%d)",
            src->ctl.scaler.cropRegion[0],
            src->ctl.scaler.cropRegion[1],
            src->ctl.scaler.cropRegion[2],
            src->ctl.scaler.cropRegion[3]);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateSensorMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    int64_t frameDuration = (int64_t) src->dm.sensor.frameDuration;
    settings->update(ANDROID_SENSOR_FRAME_DURATION, &frameDuration, 1);
    CLOGV("dm.sensor.frameDuration(%lld)", (long long)src->dm.sensor.frameDuration);

    int64_t exposureTime = (int64_t)src->dm.sensor.exposureTime;

    /*
     * HACK
     * The frameDuration must always be bigger than the exposureTime.
     * But firmware may not guarantee that. so we check the exposureTime and frameDuration.
     */
    if (exposureTime == 0 ||
        (frameDuration > 0 && exposureTime > frameDuration)) {
        exposureTime = frameDuration;
    }
    src->dm.sensor.exposureTime = exposureTime; //  for  EXIF Data
    settings->update(ANDROID_SENSOR_EXPOSURE_TIME, &exposureTime, 1);

    int32_t sensitivity = (int32_t) src->dm.sensor.sensitivity;
    if (sensitivity < m_sensorStaticInfo->sensitivityRange[MIN]) {
        sensitivity = m_sensorStaticInfo->sensitivityRange[MIN];
    } else if (sensitivity > m_sensorStaticInfo->sensitivityRange[MAX]) {
        sensitivity = m_sensorStaticInfo->sensitivityRange[MAX];
    }
    src->dm.sensor.sensitivity = sensitivity; //  for  EXIF Data
    settings->update(ANDROID_SENSOR_SENSITIVITY, &sensitivity, 1);

    CLOGV("[frameCount is %d] exposureTime(%lld) sensitivity(%d)",
            src->dm.request.frameCount, (long long)exposureTime, sensitivity);

    const int64_t timeStamp = (int64_t) src->udm.sensor.timeStampBoot;
    settings->update(ANDROID_SENSOR_TIMESTAMP, &timeStamp, 1);
    CLOGV("udm.sensor.timeStampBoot(%lld)", (long long)src->udm.sensor.timeStampBoot);

    /* Store the timeStamp and framecount mapping info */
    m_frameCountMap[m_frameCountMapIndex][TIMESTAMP] = src->udm.sensor.timeStampBoot;
    m_frameCountMap[m_frameCountMapIndex][FRAMECOUNT] = (uint64_t) src->dm.request.frameCount;
    m_frameCountMap[m_frameCountMapIndex][APERTURE] = (uint64_t) src->dm.lens.aperture;

    m_frameCountMapIndex = (m_frameCountMapIndex + 1) % FRAMECOUNT_MAP_LENGTH;

    const camera_metadata_rational_t neutralColorPoint[3] =
    {
        {(int32_t) src->dm.sensor.neutralColorPoint[0].num,
         (int32_t) src->dm.sensor.neutralColorPoint[0].den},
        {(int32_t) src->dm.sensor.neutralColorPoint[1].num,
         (int32_t) src->dm.sensor.neutralColorPoint[1].den},
        {(int32_t) src->dm.sensor.neutralColorPoint[2].num,
         (int32_t) src->dm.sensor.neutralColorPoint[2].den}
    };

    settings->update(ANDROID_SENSOR_NEUTRAL_COLOR_POINT, neutralColorPoint, 3);
    CLOGV("dm.sensor.neutralColorPoint(%d/%d,%d/%d,%d/%d)",
            src->dm.sensor.neutralColorPoint[0].num,
            src->dm.sensor.neutralColorPoint[0].den,
            src->dm.sensor.neutralColorPoint[1].num,
            src->dm.sensor.neutralColorPoint[1].den,
            src->dm.sensor.neutralColorPoint[2].num,
            src->dm.sensor.neutralColorPoint[2].den);

    const double noiseProfile[8] =
    {
        src->dm.sensor.noiseProfile[0][0], src->dm.sensor.noiseProfile[0][1],
        src->dm.sensor.noiseProfile[1][0], src->dm.sensor.noiseProfile[1][1],
        src->dm.sensor.noiseProfile[2][0], src->dm.sensor.noiseProfile[2][1],
        src->dm.sensor.noiseProfile[3][0], src->dm.sensor.noiseProfile[3][1]
    };
    settings->update(ANDROID_SENSOR_NOISE_PROFILE, noiseProfile , 8);
    CLOGV("dm.sensor.noiseProfile({%f,%f},{%f,%f},{%f,%f},{%f,%f})",
            src->dm.sensor.noiseProfile[0][0],
            src->dm.sensor.noiseProfile[0][1],
            src->dm.sensor.noiseProfile[1][0],
            src->dm.sensor.noiseProfile[1][1],
            src->dm.sensor.noiseProfile[2][0],
            src->dm.sensor.noiseProfile[2][1],
            src->dm.sensor.noiseProfile[3][0],
            src->dm.sensor.noiseProfile[3][1]);

    const float greenSplit = src->dm.sensor.greenSplit;
    settings->update(ANDROID_SENSOR_GREEN_SPLIT, &greenSplit, 1);
    CLOGV("dm.sensor.greenSplit(%f)", src->dm.sensor.greenSplit);

    const int64_t rollingShutterSkew = (int64_t) src->dm.sensor.rollingShutterSkew;
    settings->update(ANDROID_SENSOR_ROLLING_SHUTTER_SKEW, &rollingShutterSkew, 1);
    CLOGV("dm.sensor.rollingShutterSkew(%lld)",
            (long long)src->dm.sensor.rollingShutterSkew);

    //settings->update(ANDROID_SENSOR_TEMPERATURE, , );
    //settings->update(ANDROID_SENSOR_PROFILE_HUE_SAT_MAP, , );
    //settings->update(ANDROID_SENSOR_PROFILE_TONE_CURVE, , );

#ifdef SAMSUNG_TN_FEATURE
    translateVendorSensorMetaData(settings, shot_ext);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::translateShadingMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateStatisticsMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);
    camera_metadata_entry_t entry;

    entry = settings->find(ANDROID_STATISTICS_FACE_DETECT_MODE);
    if (entry.count > 0) {
        uint8_t faceDetectMode = entry.data.u8[0];
        if (faceDetectMode > ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
            shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS] = (enum mcsc_port) requestInfo->getDsInputPortId();
            m_updateFaceDetectionMetaData(settings, shot_ext);
        }
    }

    /* HACK : F/W does NOT support this field */
    //int32_t *hotPixelMap = (int32_t *) src->dm.stats.hotPixelMap;
    const int32_t hotPixelMap[] = {};
    settings->update(ANDROID_STATISTICS_HOT_PIXEL_MAP, hotPixelMap, ARRAY_LENGTH(hotPixelMap));
    CLOGV("dm.stats.hotPixelMap");

    /* HACK : F/W does NOT support this field */
    //float *lensShadingMap = (float *) src->dm.stats.lensShadingMap;
    const float lensShadingMap[] = {1.0, 1.0, 1.0, 1.0};
    settings->update(ANDROID_STATISTICS_LENS_SHADING_MAP, lensShadingMap, 4);
    CLOGV("dm.stats.lensShadingMap(%f,%f,%f,%f)",
            lensShadingMap[0], lensShadingMap[1],
            lensShadingMap[2], lensShadingMap[3]);

    uint8_t sceneFlicker = (uint8_t) CAMERA_METADATA(src->dm.stats.sceneFlicker);
    settings->update(ANDROID_STATISTICS_SCENE_FLICKER, &sceneFlicker, 1);
    CLOGV("dm.stats.sceneFlicker(%d)", src->dm.stats.sceneFlicker);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateTonemapMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateLedMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    //settings->update(ANDROID_LED_TRANSMIT, (uint8_t *) NULL, 0);
    CLOGV("dm.led.transmit(%d)", src->dm.led.transmit);

    return OK;
}

status_t ExynosCameraMetadataConverter::translateBlackLevelMetaData(ExynosCameraRequestSP_sprt_t requestInfo)
{
    CameraMetadata *settings;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    return OK;
}

status_t ExynosCameraMetadataConverter::translatePartialMetaData(ExynosCameraRequestSP_sprt_t requestInfo,
                                                                                enum metadata_type metaType)
{
    CameraMetadata *settings;
    camera_metadata_entry_t entry;
    camera_metadata_entry_t cropRegionEntry;
    struct camera2_shot_ext *shot_ext = NULL;
    struct camera2_shot *src = NULL;

    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL");
        return BAD_VALUE;
    }

    settings = requestInfo->getServiceMeta();
    shot_ext = requestInfo->getServiceShot();
    src = &(shot_ext->shot);

    if (metaType == PARTIAL_3AA) {
        const int64_t timeStamp = (int64_t) src->udm.sensor.timeStampBoot;
        settings->update(ANDROID_SENSOR_TIMESTAMP, &timeStamp, 1);
        CLOGV("udm.sensor.timeStampBoot(%lld)", (long long)src->udm.sensor.timeStampBoot);

        /* Store the timeStamp and framecount mapping info */
        m_frameCountMap[m_frameCountMapIndex][TIMESTAMP] = src->udm.sensor.timeStampBoot;
        m_frameCountMap[m_frameCountMapIndex][FRAMECOUNT] = (uint64_t) src->dm.request.frameCount;
        m_frameCountMap[m_frameCountMapIndex][APERTURE] = (uint64_t) src->dm.lens.aperture;
        m_frameCountMapIndex = (m_frameCountMapIndex + 1) % FRAMECOUNT_MAP_LENGTH;
        CLOGV("ANDROID_SENSOR_TIMESTAMP(%llu) (%u)",
                (long long)src->udm.sensor.timeStampBoot, src->dm.request.frameCount);

        if (m_sensorStaticInfo->max3aRegions[AE] > 0
            && src->dm.aa.aeMode > AA_AEMODE_OFF) {
            /* HACK: Result AE_REGION must be updated based of the value from  F/W */
            int32_t aeRegion[5];
            cropRegionEntry = settings->find(ANDROID_SCALER_CROP_REGION);
            entry = settings->find(ANDROID_CONTROL_AE_REGIONS);
            if (cropRegionEntry.count > 0 && entry.count > 0) {
                /* ae region is bigger than crop region */
                if (cropRegionEntry.data.i32[2] < entry.data.i32[2] - entry.data.i32[0]
                        || cropRegionEntry.data.i32[3] < entry.data.i32[3] - entry.data.i32[1]) {
                    aeRegion[0] = cropRegionEntry.data.i32[0];
                    aeRegion[1] = cropRegionEntry.data.i32[1];
                    aeRegion[2] = cropRegionEntry.data.i32[2] + aeRegion[0];
                    aeRegion[3] = cropRegionEntry.data.i32[3] + aeRegion[1];
                    aeRegion[4] = entry.data.i32[4];
                } else {
                    aeRegion[0] = entry.data.i32[0];
                    aeRegion[1] = entry.data.i32[1];
                    aeRegion[2] = entry.data.i32[2];
                    aeRegion[3] = entry.data.i32[3];
                    aeRegion[4] = entry.data.i32[4];
                }
            } else {
                aeRegion[0] = src->ctl.aa.aeRegions[0];
                aeRegion[1] = src->ctl.aa.aeRegions[1];
                aeRegion[2] = src->ctl.aa.aeRegions[2];
                aeRegion[3] = src->ctl.aa.aeRegions[3];
                aeRegion[4] = src->ctl.aa.aeRegions[4];
            }

            settings->update(ANDROID_CONTROL_AE_REGIONS, aeRegion, 5);
            CLOGV("dm.aa.aeRegions(%d,%d,%d,%d,%d)",
                    src->dm.aa.aeRegions[0],
                    src->dm.aa.aeRegions[1],
                    src->dm.aa.aeRegions[2],
                    src->dm.aa.aeRegions[3],
                    src->dm.aa.aeRegions[4]);
        }

        if (m_sensorStaticInfo->max3aRegions[AWB] > 0
            && src->dm.aa.awbMode > AA_AWBMODE_OFF) {
            /* HACK: Result AWB_REGION must be updated based of the value from  F/W */
            int32_t awbRegion[5];
            cropRegionEntry = settings->find(ANDROID_SCALER_CROP_REGION);
            entry = settings->find(ANDROID_CONTROL_AWB_REGIONS);
            if (cropRegionEntry.count > 0 && entry.count > 0) {
                /* awb region is bigger than crop region */
                if (cropRegionEntry.data.i32[2] < entry.data.i32[2] - entry.data.i32[0]
                        || cropRegionEntry.data.i32[3] < entry.data.i32[3] - entry.data.i32[1]) {
                    awbRegion[0] = cropRegionEntry.data.i32[0];
                    awbRegion[1] = cropRegionEntry.data.i32[1];
                    awbRegion[2] = cropRegionEntry.data.i32[2] + awbRegion[0];
                    awbRegion[3] = cropRegionEntry.data.i32[3] + awbRegion[1];
                    awbRegion[4] = entry.data.i32[4];
                } else {
                    awbRegion[0] = entry.data.i32[0];
                    awbRegion[1] = entry.data.i32[1];
                    awbRegion[2] = entry.data.i32[2];
                    awbRegion[3] = entry.data.i32[3];
                    awbRegion[4] = entry.data.i32[4];
                }
            } else {
                awbRegion[0] = src->ctl.aa.awbRegions[0];
                awbRegion[1] = src->ctl.aa.awbRegions[1];
                awbRegion[2] = src->ctl.aa.awbRegions[2];
                awbRegion[3] = src->ctl.aa.awbRegions[3];
                awbRegion[4] = src->ctl.aa.awbRegions[4];
            }

            settings->update(ANDROID_CONTROL_AWB_REGIONS, awbRegion, 5);
            CLOGV("dm.aa.awbRegions(%d,%d,%d,%d,%d)",
                    src->dm.aa.awbRegions[0],
                    src->dm.aa.awbRegions[1],
                    src->dm.aa.awbRegions[2],
                    src->dm.aa.awbRegions[3],
                    src->dm.aa.awbRegions[4]);
        }

        if (m_sensorStaticInfo->max3aRegions[AF] > 0
            && src->dm.aa.afMode > AA_AFMODE_OFF) {
            int32_t afRegion[5];
            ExynosRect2 afRect;
            cropRegionEntry = settings->find(ANDROID_SCALER_CROP_REGION);
            entry = settings->find(ANDROID_CONTROL_AF_REGIONS);

#ifdef SAMSUNG_OT
            if (src->ctl.aa.vendor_afmode_option & SET_BIT(AA_AFMODE_OPTION_BIT_OBJECT_TRACKING)) {
                ExynosCameraActivityControl *activityControl = NULL;
                ExynosCameraActivityAutofocus *autoFocusMgr = NULL;
                UniPluginFocusData_t OTpredictedData;

                activityControl = m_parameters->getActivityControl();
                autoFocusMgr = activityControl->getAutoFocusMgr();
                autoFocusMgr->getObjectTrackingAreas(&OTpredictedData);

                afRect.x1 = OTpredictedData.FocusROILeft;
                afRect.y1 = OTpredictedData.FocusROITop;
                afRect.x2 = OTpredictedData.FocusROIRight;
                afRect.y2 = OTpredictedData.FocusROIBottom;

                m_convert3AAToActiveArrayRegion(&afRect);
                afRegion[0] = afRect.x1;
                afRegion[1] = afRect.y1;
                afRegion[2] = afRect.x2;
                afRegion[3] = afRect.y2;
            } else
#endif
            {
#if 1
                /*
                 * HACK
                 * Result AF_REGION must be updated based of the value from F/W.
                 * But now, due to the AF algorithm constraints of the AF algorithm,
                 * the AF region value of the control is filled in meta.
                 */
                if (cropRegionEntry.count > 0 && entry.count > 0) {
                    /* af region is bigger than crop region */
                    if (cropRegionEntry.data.i32[2] < entry.data.i32[2] - entry.data.i32[0]
                            || cropRegionEntry.data.i32[3] < entry.data.i32[3] - entry.data.i32[1]) {
                        afRegion[0] = cropRegionEntry.data.i32[0];
                        afRegion[1] = cropRegionEntry.data.i32[1];
                        afRegion[2] = cropRegionEntry.data.i32[2] + afRegion[0];
                        afRegion[3] = cropRegionEntry.data.i32[3] + afRegion[1];
                        afRegion[4] = entry.data.i32[4];
                    } else {
                        afRegion[0] = entry.data.i32[0];
                        afRegion[1] = entry.data.i32[1];
                        afRegion[2] = entry.data.i32[2];
                        afRegion[3] = entry.data.i32[3];
                        afRegion[4] = entry.data.i32[4];
                    }
                } else {
                    afRegion[0] = src->ctl.aa.afRegions[0];
                    afRegion[1] = src->ctl.aa.afRegions[1];
                    afRegion[2] = src->ctl.aa.afRegions[2];
                    afRegion[3] = src->ctl.aa.afRegions[3];
                    afRegion[4] = src->ctl.aa.afRegions[4];
                }
#else
                if (src->ctl.aa.afMode == AA_AFMODE_AUTO
                    || src->ctl.aa.afMode == AA_AFMODE_MACRO) {
                    afRect.x1 = src->dm.aa.afRegions[0];
                    afRect.y1 = src->dm.aa.afRegions[1];
                    afRect.x2 = src->dm.aa.afRegions[2];
                    afRect.y2 = src->dm.aa.afRegions[3];

                    m_convert3AAToActiveArrayRegion(&afRect);
                    afRegion[0] = afRect.x1;
                    afRegion[1] = afRect.y1;
                    afRegion[2] = afRect.x2;
                    afRegion[3] = afRect.y2;
                    afRegion[4] = src->dm.aa.afRegions[4];
                } else if (src->ctl.aa.afMode == AA_AFMODE_CONTINUOUS_VIDEO
                           || src->ctl.aa.afMode == AA_AFMODE_CONTINUOUS_PICTURE) {
                    /*
                     * CTS(testDigitalZoom) require that af region value is exactly same about control value.
                     * If af mode is AA_AFMODE_CONTINUOUS_VIDEO or AA_AFMODE_CONTINUOUS_PICTURE,
                     * af region in DM is not considered about CONTROL value.
                     * So, HAL should set af region based on control value.
                     */
                    if (cropRegionEntry.count > 0 && entry.count > 0) {
                        /* af region is bigger than crop region */
                        if (cropRegionEntry.data.i32[2] < entry.data.i32[2] - entry.data.i32[0]
                                || cropRegionEntry.data.i32[3] < entry.data.i32[3] - entry.data.i32[1]) {
                            afRegion[0] = cropRegionEntry.data.i32[0];
                            afRegion[1] = cropRegionEntry.data.i32[1];
                            afRegion[2] = cropRegionEntry.data.i32[2] + afRegion[0];
                            afRegion[3] = cropRegionEntry.data.i32[3] + afRegion[1];
                            afRegion[4] = entry.data.i32[4];
                        } else {
                            afRegion[0] = entry.data.i32[0];
                            afRegion[1] = entry.data.i32[1];
                            afRegion[2] = entry.data.i32[2];
                            afRegion[3] = entry.data.i32[3];
                            afRegion[4] = entry.data.i32[4];
                        }
                    } else {
                        afRegion[0] = src->ctl.aa.afRegions[0];
                        afRegion[1] = src->ctl.aa.afRegions[1];
                        afRegion[2] = src->ctl.aa.afRegions[2];
                        afRegion[3] = src->ctl.aa.afRegions[3];
                        afRegion[4] = src->ctl.aa.afRegions[4];
                    }
                }
#endif
            }

            settings->update(ANDROID_CONTROL_AF_REGIONS, afRegion, 5);
            CLOGV("dm.aa.afRegions(%d,%d,%d,%d,%d)",
                    src->dm.aa.afRegions[0],
                    src->dm.aa.afRegions[1],
                    src->dm.aa.afRegions[2],
                    src->dm.aa.afRegions[3],
                    src->dm.aa.afRegions[4]);
        }

        uint8_t tmpAeState = (uint8_t) CAMERA_METADATA(src->dm.aa.aeState);

        /* HACK: forcely set AE state during init skip count (FW not supported) */
        if (src->dm.request.frameCount < INITIAL_SKIP_FRAME) {
            tmpAeState = (uint8_t) CAMERA_METADATA(AE_STATE_SEARCHING);
        }

#ifdef USE_AE_CONVERGED_UDM
        if (m_cameraId == CAMERA_ID_BACK &&
                tmpAeState == (uint8_t) CAMERA_METADATA(AE_STATE_CONVERGED)) {
            uint32_t aeUdmState = (uint32_t)src->udm.ae.vendorSpecific[397];
            /*  1: converged, 0: searching */
            if (aeUdmState == 0) {
                tmpAeState = (uint8_t) CAMERA_METADATA(AE_STATE_SEARCHING);
            }
        }
#endif

        uint8_t aeMode = ANDROID_CONTROL_AE_MODE_OFF;
        entry = settings->find(ANDROID_CONTROL_AE_MODE);
        if (entry.count > 0) {
            aeMode = entry.data.u8[0];
        }

        switch (src->dm.aa.aeState) {
            case AE_STATE_CONVERGED:
            case AE_STATE_LOCKED:
                if (m_flashMgr != NULL)
                    m_flashMgr->notifyAeResult();
                if (aeMode == ANDROID_CONTROL_AE_MODE_ON_ALWAYS_FLASH) {
                    tmpAeState = (uint8_t) CAMERA_METADATA(AE_STATE_FLASH_REQUIRED);
                }
                break;
            case AE_STATE_INACTIVE:
            case AE_STATE_SEARCHING:
            case AE_STATE_FLASH_REQUIRED:
            case AE_STATE_PRECAPTURE:
            default:
                break;
        }

        const uint8_t aeState = tmpAeState;
        settings->update(ANDROID_CONTROL_AE_STATE, &aeState, 1);
        CLOGV("dm.aa.aeState(%d), AE_STATE(%d)", src->dm.aa.aeState, aeState);

        const uint8_t afState = (uint8_t) CAMERA_METADATA(src->dm.aa.afState);
        settings->update(ANDROID_CONTROL_AF_STATE, &afState, 1);
        CLOGV("dm.aa.afState(%d)", src->dm.aa.afState);

        const uint8_t awbState = (uint8_t) CAMERA_METADATA(src->dm.aa.awbState);
        settings->update(ANDROID_CONTROL_AWB_STATE, &awbState, 1);
        CLOGV("dm.aa.awbState(%d)", src->dm.aa.awbState);

        switch (src->dm.aa.afState) {
        case AA_AFSTATE_FOCUSED_LOCKED:
        case AA_AFSTATE_NOT_FOCUSED_LOCKED:
            if (m_flashMgr != NULL)
                m_flashMgr->notifyAfResultHAL3();
            break;
        case AA_AFSTATE_INACTIVE:
        case AA_AFSTATE_PASSIVE_SCAN:
        case AA_AFSTATE_PASSIVE_FOCUSED:
        case AA_AFSTATE_ACTIVE_SCAN:
        case AA_AFSTATE_PASSIVE_UNFOCUSED:
        default:
            break;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    translateVendorPartialMetaData(settings, shot_ext, metaType);
#endif

    return OK;
}

status_t ExynosCameraMetadataConverter::updateDynamicMeta(ExynosCameraRequestSP_sprt_t requestInfo, enum metadata_type metaType)
{
    status_t ret = OK;
    uint32_t errorFlag = 0;

    CLOGV("%d frame", requestInfo->getFrameCount());
    /* Validation check */
    if (requestInfo == NULL) {
        CLOGE("RequestInfo is NULL!!");
        return BAD_VALUE;
    }

    requestInfo->setRequestLock();

    if (metaType == PARTIAL_NONE) {
        ret = translateColorMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 0);
        ret = translateControlMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 1);
        ret = translateEdgeMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 2);
        ret = translateFlashMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 3);
        ret = translateHotPixelMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 4);
        ret = translateJpegMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 5);
        ret = translateLensMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 6);
        ret = translateNoiseMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 7);
        ret = translateRequestMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 8);
        ret = translateScalerMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 9);
        ret = translateSensorMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 10);
        ret = translateShadingMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 11);
        ret = translateStatisticsMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 12);
        ret = translateTonemapMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 13);
        ret = translateLedMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 14);
        ret = translateBlackLevelMetaData(requestInfo);
        if (ret != OK)
            errorFlag |= (1 << 15);
    } else {
        ret = translatePartialMetaData(requestInfo, metaType);
        if (ret != OK)
            errorFlag |= (1 << 16);
    }

    requestInfo->setRequestUnlock();

    if (errorFlag != 0) {
        CLOGE("failed to translate Meta Type(%d) Data(%d)", metaType, errorFlag);
        return INVALID_OPERATION;
    }

    return OK;
}

status_t ExynosCameraMetadataConverter::checkAvailableStreamFormat(int format)
{
    int ret = OK;
    CLOGD(" format(%d)", format);

    // TODO:check available format
    return ret;
}

status_t ExynosCameraMetadataConverter::m_createAvailableCapabilities(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo, Vector<uint8_t> *capabilities)
{
    status_t ret = NO_ERROR;
    uint8_t hwLevel;
    uint64_t supportedCapabilities;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (capabilities == NULL) {
        CLOGE2("NULL");
        return BAD_VALUE;
    }

    hwLevel = sensorStaticInfo->supportedHwLevel;
    supportedCapabilities = sensorStaticInfo->supportedCapabilities;

    CLOGD2("supportedHwLevel(%d) supportedCapabilities(0x%4llx)", hwLevel, (unsigned long long)supportedCapabilities);

    capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BACKWARD_COMPATIBLE);

    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_MANUAL_SENSOR) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR);
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_READ_SENSOR_SETTINGS);
    }
    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_MANUAL_POST_PROCESSING) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_POST_PROCESSING);
    }
    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_BURST_CAPTURE) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_BURST_CAPTURE);
    }
    if (supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_PRIVATE_REPROCESSING);
    }
    if (supportedCapabilities & CAPABILITIES_YUV_REPROCESSING) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_YUV_REPROCESSING);
    }
    if (supportedCapabilities & CAPABILITIES_RAW) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_RAW);
    }
    if (supportedCapabilities & CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO) {
        capabilities->add(ANDROID_REQUEST_AVAILABLE_CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO);
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createAvailableKeys(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *request, Vector<int32_t> *result, Vector<int32_t> *characteristics, int cameraId)
{
    status_t ret = NO_ERROR;
    uint8_t hwLevel;
    uint64_t supportedCapabilities;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (request == NULL || result == NULL || characteristics == NULL) {
        CLOGE2("NULL");
        return BAD_VALUE;
    }

    hwLevel = sensorStaticInfo->supportedHwLevel;
    supportedCapabilities = sensorStaticInfo->supportedCapabilities;

    if (sensorStaticInfo->requestKeys != NULL) {
        request->appendArray(sensorStaticInfo->requestKeys, sensorStaticInfo->requestKeysLength);
    }
    if (sensorStaticInfo->resultKeys != NULL) {
        result->appendArray(sensorStaticInfo->resultKeys, sensorStaticInfo->resultKeysLength);
    }
    if (sensorStaticInfo->characteristicsKeys != NULL) {
        characteristics->appendArray(sensorStaticInfo->characteristicsKeys,
                                    sensorStaticInfo->characteristicsKeysLength);
    }

    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL) {
        request->add(ANDROID_EDGE_MODE);
        result->add(ANDROID_EDGE_MODE);
        characteristics->add(ANDROID_EDGE_AVAILABLE_EDGE_MODES);
    }
    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_MANUAL_SENSOR) {
        request->insertArrayAt(availableRequestManualSensor, request->size(), ARRAY_LENGTH(availableRequestManualSensor));
        result->insertArrayAt(availableResultManualSensor, result->size(), ARRAY_LENGTH(availableResultManualSensor));
    }
    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL
        || supportedCapabilities & CAPABILITIES_MANUAL_POST_PROCESSING) {
        request->insertArrayAt(availableRequestManualPostProcessing, request->size(), ARRAY_LENGTH(availableRequestManualPostProcessing));
        result->insertArrayAt(availableResultManualPostProcessing, result->size(), ARRAY_LENGTH(availableResultManualPostProcessing));
        characteristics->add(ANDROID_TONEMAP_AVAILABLE_TONE_MAP_MODES);
        characteristics->add(ANDROID_TONEMAP_MAX_CURVE_POINTS);
    }
    if (supportedCapabilities & CAPABILITIES_RAW) {
        request->insertArrayAt(availableRequestRaw, request->size(), ARRAY_LENGTH(availableRequestRaw));
        result->insertArrayAt(availableResultRaw, result->size(), ARRAY_LENGTH(availableResultRaw));
        characteristics->add(ANDROID_HOT_PIXEL_AVAILABLE_HOT_PIXEL_MODES);
        characteristics->add(ANDROID_STATISTICS_INFO_AVAILABLE_HOT_PIXEL_MAP_MODES);
    }

    if (hwLevel == ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY
        && cameraId == CAMERA_ID_SECURE) {
        request->add(ANDROID_SENSOR_EXPOSURE_TIME);
        result->add(ANDROID_SENSOR_EXPOSURE_TIME);
        characteristics->add(ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE);
    }

    if (sensorStaticInfo->max3aRegions[AE] > 0) {
        request->add(ANDROID_CONTROL_AE_REGIONS);
        result->add(ANDROID_CONTROL_AE_REGIONS);
    }
    if (sensorStaticInfo->max3aRegions[AWB] > 0) {
        request->add(ANDROID_CONTROL_AWB_REGIONS);
        result->add(ANDROID_CONTROL_AWB_REGIONS);
    }
    if (sensorStaticInfo->max3aRegions[AF] > 0) {
        request->add(ANDROID_CONTROL_AF_REGIONS);
        result->add(ANDROID_CONTROL_AF_REGIONS);
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createControlAvailableHighSpeedVideoConfigurations(
        const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
        Vector<int32_t> *streamConfigs)
{
    int (*highSpeedVideoSizeList)[SIZE_OF_RESOLUTION] = NULL;
    int highSpeedVideoSizeListLength = 0;
    int (*highSpeedVideoFPSList)[2] = NULL;
    int highSpeedVideoFPSListLength = 0;
    int streamConfigSize = 0;
    bool isSupportHighSpeedVideo = false;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    isSupportHighSpeedVideo = (sensorStaticInfo->supportedCapabilities & CAPABILITIES_CONSTRAINED_HIGH_SPEED_VIDEO);

    if (isSupportHighSpeedVideo) {
        highSpeedVideoSizeList = sensorStaticInfo->highSpeedVideoList;
        highSpeedVideoSizeListLength = sensorStaticInfo->highSpeedVideoListMax;
        highSpeedVideoFPSList = sensorStaticInfo->highSpeedVideoFPSList;
        highSpeedVideoFPSListLength = sensorStaticInfo->highSpeedVideoFPSListMax;

        streamConfigSize = (highSpeedVideoSizeListLength * highSpeedVideoFPSListLength * 5);

        for (int i = 0; i < highSpeedVideoFPSListLength; i++) {
            for (int j = 0; j < highSpeedVideoSizeListLength; j++) {
                streamConfigs->add(highSpeedVideoSizeList[j][0]);
                streamConfigs->add(highSpeedVideoSizeList[j][1]);
                streamConfigs->add(highSpeedVideoFPSList[i][0]/1000);
                streamConfigs->add(highSpeedVideoFPSList[i][1]/1000);
#if defined(SUPPORT_HFR_BATCH_MODE) && !defined(USE_SERVICE_BATCH_MODE)
                streamConfigs->add((highSpeedVideoFPSList[i][1]/1000)/MULTI_BUFFER_BASE_FPS);
#else
                streamConfigs->add(1);
#endif
            }
        }
        return NO_ERROR;
    } else {
        return NAME_NOT_FOUND;
    }
}

/*
   - Returns NO_ERROR if private reprocessing is supported: streamConfigs will have valid entries.
   - Returns NAME_NOT_FOUND if private reprocessing is not supported: streamConfigs will be returned as is,
     and scaler.AvailableInputOutputFormatsMap should not be updated.
*/
status_t ExynosCameraMetadataConverter::m_createScalerAvailableInputOutputFormatsMap(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                         Vector<int32_t> *streamConfigs)
{
    int streamConfigSize = 0;
    bool isSupportPrivReprocessing = false;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    isSupportPrivReprocessing = (sensorStaticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING);

    if (isSupportPrivReprocessing == true) {
        streamConfigs->add(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        streamConfigs->add(2);
        streamConfigs->add(HAL_PIXEL_FORMAT_YCbCr_420_888);
        streamConfigs->add(HAL_PIXEL_FORMAT_BLOB);
        streamConfigs->setCapacity(streamConfigSize);

        return NO_ERROR;
    } else {
        return NAME_NOT_FOUND;
    }
}

status_t ExynosCameraMetadataConverter::m_createScalerAvailableStreamConfigurationsOutput(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                                           Vector<int32_t> *streamConfigs)
{
    status_t ret = NO_ERROR;
    int (*yuvSizeList)[SIZE_OF_RESOLUTION] = NULL;
    int yuvSizeListLength = 0;
    int (*jpegSizeList)[SIZE_OF_RESOLUTION] = NULL;
    int jpegSizeListLength = 0;
    int streamConfigSize = 0;
    bool isSupportHighResolution = false;
    bool isSupportPrivReprocessing = false;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (streamConfigs == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    isSupportHighResolution = (sensorStaticInfo->supportedCapabilities & CAPABILITIES_BURST_CAPTURE);

    yuvSizeList = sensorStaticInfo->yuvList;
    yuvSizeListLength = sensorStaticInfo->yuvListMax;
    jpegSizeList = sensorStaticInfo->jpegList;
    jpegSizeListLength = sensorStaticInfo->jpegListMax;

    /* Check wheather the private reprocessing is supported or not */
    isSupportPrivReprocessing = (sensorStaticInfo->supportedCapabilities & CAPABILITIES_PRIVATE_REPROCESSING);

    /* TODO: Add YUV reprocessing if necessary */

    /* HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED stream configuration list size */
    streamConfigSize = yuvSizeListLength * 4;
    /* YUV output stream configuration list size */
    streamConfigSize += (yuvSizeListLength * 4) * (ARRAY_LENGTH(YUV_FORMATS));
    /* Stall output stream configuration list size */
    streamConfigSize += (jpegSizeListLength * 4) * (ARRAY_LENGTH(STALL_FORMATS));
    /* RAW output stream configuration list size */
    streamConfigSize += (1 * 4) * (ARRAY_LENGTH(RAW_FORMATS));
    /* ZSL input stream configuration list size */
    if(isSupportPrivReprocessing == true) {
        streamConfigSize += 4;
    }
    streamConfigs->setCapacity(streamConfigSize);

    /* HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED stream supported size list */
    for (int i = 0; i < yuvSizeListLength; i++) {
        streamConfigs->add(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        streamConfigs->add(yuvSizeList[i][0]);
        streamConfigs->add(yuvSizeList[i][1]);
        streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
    }

    /* YUV output stream supported size list */
    for (size_t i = 0; i < ARRAY_LENGTH(YUV_FORMATS); i++) {
        for (int j = 0; j < yuvSizeListLength; j++) {
            int pixelSize = yuvSizeList[j][0] * yuvSizeList[j][1];
            if (isSupportHighResolution == false
                && pixelSize > HIGH_RESOLUTION_MIN_PIXEL_SIZE) {
                streamConfigSize -= 4;
                continue;
            }

            streamConfigs->add(YUV_FORMATS[i]);
            streamConfigs->add(yuvSizeList[j][0]);
            streamConfigs->add(yuvSizeList[j][1]);
            streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
        }
    }

    /* Stall output stream supported size list */
    for (size_t i = 0; i < ARRAY_LENGTH(STALL_FORMATS); i++) {
        for (int j = 0; j < jpegSizeListLength; j++) {
            streamConfigs->add(STALL_FORMATS[i]);
            streamConfigs->add(jpegSizeList[j][0]);
            streamConfigs->add(jpegSizeList[j][1]);
            streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
        }
    }

    /* RAW output stream supported size list */
    for (size_t i = 0; i < ARRAY_LENGTH(RAW_FORMATS); i++) {
        /* Add sensor max size */
        streamConfigs->add(RAW_FORMATS[i]);
        streamConfigs->add(sensorStaticInfo->maxSensorW);
        streamConfigs->add(sensorStaticInfo->maxSensorH);
        streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT);
    }

    /* ZSL input stream supported size list */
    {
        if(isSupportPrivReprocessing == true) {
            streamConfigs->add(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
            streamConfigs->add(yuvSizeList[0][0]);
            streamConfigs->add(yuvSizeList[0][1]);
            streamConfigs->add(ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_INPUT);
        }
    }

    streamConfigs->setCapacity(streamConfigSize);

#ifdef DEBUG_STREAM_CONFIGURATIONS
    const int32_t* streamConfigArray = NULL;
    streamConfigArray = streamConfigs->array();
    for (int i = 0; i < streamConfigSize; i = i + 4) {
        CLOGD2("Size %4dx%4d Format %2x %6s",
                streamConfigArray[i+1], streamConfigArray[i+2],
                streamConfigArray[i],
                (streamConfigArray[i+3] == ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT)?
                "OUTPUT" : "INPUT");
    }
#endif

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createScalerAvailableMinFrameDurations(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                                 Vector<int64_t> *minDurations)
{
    status_t ret = NO_ERROR;
    int (*yuvSizeList)[SIZE_OF_RESOLUTION] = NULL;
    int yuvSizeListLength = 0;
    int (*jpegSizeList)[SIZE_OF_RESOLUTION] = NULL;
    int jpegSizeListLength = 0;
    int minDurationSize = 0;
    bool isSupportHighResolution = false;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (minDurations == NULL) {
        CLOGE2("Stream configs is NULL");
        return BAD_VALUE;
    }

    isSupportHighResolution = (sensorStaticInfo->supportedCapabilities & CAPABILITIES_BURST_CAPTURE);

    yuvSizeList = sensorStaticInfo->yuvList;
    yuvSizeListLength = sensorStaticInfo->yuvListMax;
    jpegSizeList = sensorStaticInfo->jpegList;
    jpegSizeListLength = sensorStaticInfo->jpegListMax;

    /* HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED stream min frame duration list size */
    minDurationSize = yuvSizeListLength * 4;
    /* YUV output stream min frame duration list size */
    minDurationSize += (yuvSizeListLength * 4) * (ARRAY_LENGTH(YUV_FORMATS));
    /* Stall output stream configuration list size */
    minDurationSize += (jpegSizeListLength * 4) * (ARRAY_LENGTH(STALL_FORMATS));
    /* RAW output stream min frame duration list size */
    minDurationSize += (1 * 4) * (ARRAY_LENGTH(RAW_FORMATS));
    minDurations->setCapacity(minDurationSize);

    /* HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED stream min frame duration list */
    for (int i = 0; i < yuvSizeListLength; i++) {
        minDurations->add(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        minDurations->add((int64_t)yuvSizeList[i][0]);
        minDurations->add((int64_t)yuvSizeList[i][1]);
        minDurations->add((int64_t)yuvSizeList[i][2]);
    }

    /* YUV output stream min frame duration list */
    for (size_t i = 0; i < ARRAY_LENGTH(YUV_FORMATS); i++) {
        for (int j = 0; j < yuvSizeListLength; j++) {
            int pixelSize = yuvSizeList[j][0] * yuvSizeList[j][1];
            if (isSupportHighResolution == false
                && pixelSize > HIGH_RESOLUTION_MIN_PIXEL_SIZE) {
                minDurationSize -= 4;
                continue;
            }

            minDurations->add((int64_t)YUV_FORMATS[i]);
            minDurations->add((int64_t)yuvSizeList[j][0]);
            minDurations->add((int64_t)yuvSizeList[j][1]);
            minDurations->add((int64_t)yuvSizeList[j][2]);
        }
    }

    /* Stall output stream min frame duration list */
    for (size_t i = 0; i < ARRAY_LENGTH(STALL_FORMATS); i++) {
        for (int j = 0; j < jpegSizeListLength; j++) {
            minDurations->add((int64_t)STALL_FORMATS[i]);
            minDurations->add((int64_t)jpegSizeList[j][0]);
            minDurations->add((int64_t)jpegSizeList[j][1]);
            minDurations->add((int64_t)jpegSizeList[j][2]);
        }
    }

    /* RAW output stream min frame duration list */
    for (size_t i = 0; i < ARRAY_LENGTH(RAW_FORMATS); i++) {
        /* Add sensor max size */
        minDurations->add((int64_t)RAW_FORMATS[i]);
        minDurations->add((int64_t)sensorStaticInfo->maxSensorW);
        minDurations->add((int64_t)sensorStaticInfo->maxSensorH);
        minDurations->add((int64_t)yuvSizeList[0][2]);
    }

    minDurations->setCapacity(minDurationSize);

#ifdef DEBUG_STREAM_CONFIGURATIONS
    const int64_t* minDurationArray = NULL;
    minDurationArray = minDurations->array();
    for (int i = 0; i < minDurationSize; i = i + 4) {
        CLOGD2("Size %4lldx%4lld Format %2x MinDuration %9lld",
                minDurationArray[i+1], minDurationArray[i+2],
                (int)minDurationArray[i], minDurationArray[i+3]);
    }
#endif

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createJpegAvailableThumbnailSizes(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                             Vector<int32_t> *thumbnailSizes)
{
    int ret = OK;
    int (*thumbnailSizeList)[SIZE_OF_RESOLUTION] = NULL;
    size_t thumbnailSizeListLength = 0;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (thumbnailSizes == NULL) {
        CLOGE2("Thumbnail sizes is NULL");
        return BAD_VALUE;
    }

    thumbnailSizeList = sensorStaticInfo->thumbnailList;
    thumbnailSizeListLength = sensorStaticInfo->thumbnailListMax;
    thumbnailSizes->setCapacity(thumbnailSizeListLength * 2);

    /* JPEG thumbnail sizes must be delivered with ascending ordering */
    for (int i = (int)thumbnailSizeListLength - 1; i >= 0; i--) {
        thumbnailSizes->add(thumbnailSizeList[i][0]);
        thumbnailSizes->add(thumbnailSizeList[i][1]);
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::m_createAeAvailableFpsRanges(const struct ExynosCameraSensorInfoBase *sensorStaticInfo,
                                                                      Vector<int32_t> *fpsRanges)
{
    int ret = OK;
    int (*fpsRangesList)[2] = NULL;
    size_t fpsRangesLength = 0;

    if (sensorStaticInfo == NULL) {
        CLOGE2("Sensor static info is NULL");
        return BAD_VALUE;
    }
    if (fpsRanges == NULL) {
        CLOGE2("FPS ranges is NULL");
        return BAD_VALUE;
    }

    fpsRangesList = sensorStaticInfo->fpsRangesList;
    fpsRangesLength = sensorStaticInfo->fpsRangesListMax;

    fpsRanges->setCapacity(fpsRangesLength * 2);

    for (size_t i = 0; i < fpsRangesLength; i++) {
        fpsRanges->add(fpsRangesList[i][0]/1000);
        fpsRanges->add(fpsRangesList[i][1]/1000);
    }

    return ret;
}

bool ExynosCameraMetadataConverter::m_hasTagInList(int32_t *list, size_t listSize, int32_t tag)
{
    bool hasTag = false;

    for (size_t i = 0; i < listSize; i++) {
        if (list[i] == tag) {
            hasTag = true;
            break;
        }
    }

    return hasTag;
}

bool ExynosCameraMetadataConverter::m_hasTagInList(uint8_t *list, size_t listSize, int32_t tag)
{
    bool hasTag = false;

    for (size_t i = 0; i < listSize; i++) {
        if (list[i] == tag) {
            hasTag = true;
            break;
        }
    }

    return hasTag;
}

status_t ExynosCameraMetadataConverter::m_integrateOrderedSizeList(int (*list1)[SIZE_OF_RESOLUTION], size_t list1Size,
                                                                    int (*list2)[SIZE_OF_RESOLUTION], size_t list2Size,
                                                                    int (*orderedList)[SIZE_OF_RESOLUTION])
{
    int *currentSize = NULL;
    size_t sizeList1Index = 0;
    size_t sizeList2Index = 0;

    if (list1 == NULL || list2 == NULL || orderedList == NULL) {
        CLOGE2("Arguments are NULL. list1 %p list2 %p orderedlist %p",
                list1, list2, orderedList);
        return BAD_VALUE;
    }

    /* This loop will integrate two size list in descending order */
    for (size_t i = 0; i < list1Size + list2Size; i++) {
        if (sizeList1Index >= list1Size) {
            currentSize = list2[sizeList2Index++];
        } else if (sizeList2Index >= list2Size) {
            currentSize = list1[sizeList1Index++];
        } else {
            if (list1[sizeList1Index][0] < list2[sizeList2Index][0]) {
                currentSize = list2[sizeList2Index++];
            } else if (list1[sizeList1Index][0] > list2[sizeList2Index][0]) {
                currentSize = list1[sizeList1Index++];
            } else {
                if (list1[sizeList1Index][1] < list2[sizeList2Index][1])
                    currentSize = list2[sizeList2Index++];
                else
                    currentSize = list1[sizeList1Index++];
            }
        }
        orderedList[i][0] = currentSize[0];
        orderedList[i][1] = currentSize[1];
        orderedList[i][2] = currentSize[2];
    }

    return NO_ERROR;
}

void ExynosCameraMetadataConverter::m_updateFaceDetectionMetaData(CameraMetadata *settings, struct camera2_shot_ext *shot_ext)
{
    status_t ret = NO_ERROR;
    int32_t faceIds[NUM_OF_DETECTED_FACES];
    /* {leftEyeX, leftEyeY, rightEyeX, rightEyeY, mouthX, mouthY} */
    int32_t faceLandmarks[NUM_OF_DETECTED_FACES * FACE_LANDMARKS_MAX_INDEX];
    /* {xmin, ymin, xmax, ymax} with the absolute coordinate */
    int32_t faceRectangles[NUM_OF_DETECTED_FACES * RECTANGLE_MAX_INDEX];
    uint8_t faceScores[NUM_OF_DETECTED_FACES];
    uint8_t detectedFaceCount = 0;
    ExynosRect activeArraySize;
    ExynosRect hwSensorSize;
    ExynosRect hwActiveArraySize;
    ExynosRect hwBcropSize;
    ExynosRect vraInputSize;
    int offsetX = 0, offsetY = 0;

    if (settings == NULL) {
        CLOGE("CameraMetadata is NULL");
        return;
    }

    if (shot_ext == NULL) {
        CLOGE("camera2_shot_ext is NULL");
        return;
    }

    /* Original FD region : based on FD input size (e.g. preview size)
     * Camera Metadata FD region : based on sensor active array size (e.g. max sensor size)
     * The FD region from firmware must be scaled into the size based on sensor active array size
     */
    m_parameters->getMaxSensorSize(&activeArraySize.w, &activeArraySize.h);
    m_parameters->getPreviewBayerCropSize(&hwSensorSize, &hwBcropSize);
    if (m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
        m_parameters->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                        shot_ext->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
    } else {
        m_parameters->getHwYuvSize(&vraInputSize.w, &vraInputSize.h, 0);
    }

    CLOGV("ActiveArraySize %dx%d(%d) HWSensorSize %dx%d(%d) HWBcropSize %d,%d %dx%d(%d) VRAInputSize %dx%d(%d)",
            activeArraySize.w, activeArraySize.h, SIZE_RATIO(activeArraySize.w, activeArraySize.h),
            hwSensorSize.w, hwSensorSize.h, SIZE_RATIO(hwSensorSize.w, hwSensorSize.h),
            hwBcropSize.x, hwBcropSize.y, hwBcropSize.w, hwBcropSize.h, SIZE_RATIO(hwBcropSize.w, hwBcropSize.h),
            vraInputSize.w, vraInputSize.h, SIZE_RATIO(vraInputSize.w, vraInputSize.h));

    /* Calculate scale down ratio and offset to adjust VRA input size into HW bcrop size */
    ExynosRect scaledVraInputSize;
    ret = getCropRectAlign(hwBcropSize.w, hwBcropSize.h,
                           vraInputSize.w, vraInputSize.h,
                           &scaledVraInputSize.x, &scaledVraInputSize.y,
                           &scaledVraInputSize.w, &scaledVraInputSize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                hwBcropSize.w, hwBcropSize.h,
                vraInputSize.w, vraInputSize.h);
        return;
    }

    float vraScaleRatioW = 0.0f, vraScaleRatioH = 0.0f;
    vraScaleRatioW = (float) scaledVraInputSize.w / (float) vraInputSize.w;
    vraScaleRatioH = (float) scaledVraInputSize.h / (float) vraInputSize.h;
    offsetX = scaledVraInputSize.x;
    offsetY = scaledVraInputSize.y;

    /* Add HW bcrop offset */
    offsetX += hwBcropSize.x;
    offsetY += hwBcropSize.y;

    /* Calculate HW active array size for current sensor aspect ratio
       based on active array size
     */
    ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                           hwSensorSize.w, hwSensorSize.h,
                           &hwActiveArraySize.x, &hwActiveArraySize.y,
                           &hwActiveArraySize.w, &hwActiveArraySize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwSensorSize.w, hwSensorSize.h);
        return;
    }

    /* Scale down HW active array offset */
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;
    scaleRatioW = (float) hwSensorSize.w / (float) hwActiveArraySize.w;
    scaleRatioH = (float) hwSensorSize.h / (float) hwActiveArraySize.h;

    hwActiveArraySize.x = (int) (((float) hwActiveArraySize.x) * scaleRatioW);
    hwActiveArraySize.y = (int) (((float) hwActiveArraySize.y) * scaleRatioH);

    /* Add HW active array size offset */
    offsetX += hwActiveArraySize.x;
    offsetY += hwActiveArraySize.y;

    /* Apply offset and scale ratio to FD region
       FD region' = ((FD region x vraScaleRatioW,H) + offsetX,Y) / scaleRatioW,H
     */
    for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
        if (shot_ext->shot.dm.stats.faceScores[i]) {
            switch (shot_ext->shot.dm.stats.faceDetectMode) {
                case FACEDETECT_MODE_FULL:
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + LEFT_EYE_X] = -1;
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + LEFT_EYE_Y] = -1;
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + RIGHT_EYE_X] = -1;
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + RIGHT_EYE_Y] = -1;
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + MOUTH_X] = -1;
                    faceLandmarks[(i * FACE_LANDMARKS_MAX_INDEX) + MOUTH_Y] = -1;
                case FACEDETECT_MODE_SIMPLE:
                    faceIds[i] = shot_ext->shot.dm.stats.faceIds[i];
                    /* Normalize the score into the range of [0, 100] */
                    faceScores[i] = (uint8_t) ((float)shot_ext->shot.dm.stats.faceScores[i] / (255.0f / 100.0f));

                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + X1] = (int32_t) (((shot_ext->shot.dm.stats.faceRectangles[i][X1] * vraScaleRatioW) + offsetX) / scaleRatioW);
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y1] = (int32_t) (((shot_ext->shot.dm.stats.faceRectangles[i][Y1] * vraScaleRatioH) + offsetY) / scaleRatioH);
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + X2] = (int32_t) (((shot_ext->shot.dm.stats.faceRectangles[i][X2] * vraScaleRatioW) + offsetX) / scaleRatioW);
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y2] = (int32_t) (((shot_ext->shot.dm.stats.faceRectangles[i][Y2] * vraScaleRatioH) + offsetY) / scaleRatioH);
                    CLOGV("faceIds[%d](%d), faceScores[%d](%d), original(%d,%d,%d,%d), converted(%d,%d,%d,%d)",
                        i, faceIds[i], i, faceScores[i],
                        shot_ext->shot.dm.stats.faceRectangles[i][X1],
                        shot_ext->shot.dm.stats.faceRectangles[i][Y1],
                        shot_ext->shot.dm.stats.faceRectangles[i][X2],
                        shot_ext->shot.dm.stats.faceRectangles[i][Y2],
                        faceRectangles[(i * RECTANGLE_MAX_INDEX) + X1],
                        faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y1],
                        faceRectangles[(i * RECTANGLE_MAX_INDEX) + X2],
                        faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y2]);

                    detectedFaceCount++;
                    break;
                case FACEDETECT_MODE_OFF:
                    faceScores[i] = 0;
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + X1] = 0;
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y1] = 0;
                    faceRectangles[(i * RECTANGLE_MAX_INDEX) + X2] = 0;
                    faceRectangles[(i * RECTANGLE_MAX_INDEX)+ Y2] = 0;
                    break;
                default:
                    CLOGE("Not supported FD mode(%d)",
                            shot_ext->shot.dm.stats.faceDetectMode);
                    break;
            }
        } else {
            faceIds[i] = 0;
            faceScores[i] = 0;
            faceRectangles[(i * RECTANGLE_MAX_INDEX) + X1] = 0;
            faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y1] = 0;
            faceRectangles[(i * RECTANGLE_MAX_INDEX) + X2] = 0;
            faceRectangles[(i * RECTANGLE_MAX_INDEX) + Y2] = 0;
        }
    }

    switch (shot_ext->shot.dm.stats.faceDetectMode) {
        case FACEDETECT_MODE_FULL:
            settings->update(ANDROID_STATISTICS_FACE_LANDMARKS, faceLandmarks,
                    detectedFaceCount * FACE_LANDMARKS_MAX_INDEX);
            CLOGV("dm.stats.faceLandmarks(%d)", detectedFaceCount);
        case FACEDETECT_MODE_SIMPLE:
            settings->update(ANDROID_STATISTICS_FACE_IDS, faceIds, detectedFaceCount);
            CLOGV("dm.stats.faceIds(%d)", detectedFaceCount);

            settings->update(ANDROID_STATISTICS_FACE_RECTANGLES, faceRectangles,
                    detectedFaceCount * RECTANGLE_MAX_INDEX);
            CLOGV("dm.stats.faceRectangles(%d)", detectedFaceCount);

            settings->update(ANDROID_STATISTICS_FACE_SCORES, faceScores, detectedFaceCount);
            CLOGV("dm.stats.faceScores(%d)", detectedFaceCount);
            break;
        case FACEDETECT_MODE_OFF:
        default:
            if (detectedFaceCount > 0) {
                CLOGE("Not supported FD mode(%d)",
                        shot_ext->shot.dm.stats.faceDetectMode);
            }
            break;
    }

    return;
}

void ExynosCameraMetadataConverter::m_convertActiveArrayTo3AARegion(ExynosRect2 *region)
{
    status_t ret = NO_ERROR;
    ExynosRect activeArraySize;
    ExynosRect hwSensorSize;
    ExynosRect hwActiveArraySize;
    ExynosRect hwBcropSize;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;

    m_parameters->getMaxSensorSize(&activeArraySize.w, &activeArraySize.h);
    m_parameters->getPreviewBayerCropSize(&hwSensorSize, &hwBcropSize);

    CLOGV("ActiveArraySize %dx%d(%d) Region %d,%d %d,%d %dx%d(%d) HWSensorSize %dx%d(%d) HWBcropSize %d,%d %dx%d(%d)",
            activeArraySize.w, activeArraySize.h, SIZE_RATIO(activeArraySize.w, activeArraySize.h),
            region->x1, region->y1, region->x2, region->y2, region->x2 - region->x1, region->y2 - region->y1, SIZE_RATIO(region->x2 - region->x1, region->y2 - region->y1),
            hwSensorSize.w, hwSensorSize.h, SIZE_RATIO(hwSensorSize.w, hwSensorSize.h),
            hwBcropSize.x, hwBcropSize.y, hwBcropSize.w, hwBcropSize.h, SIZE_RATIO(hwBcropSize.w, hwBcropSize.h));

    /* Calculate HW active array size for current sensor aspect ratio
       based on active array size
     */
    ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                           hwSensorSize.w, hwSensorSize.h,
                           &hwActiveArraySize.x, &hwActiveArraySize.y,
                           &hwActiveArraySize.w, &hwActiveArraySize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwSensorSize.w, hwSensorSize.h);
        return;
    }

    /* Scale down the 3AA region & HW active array size
       to adjust them to the 3AA input size without sensor margin
     */
    scaleRatioW = (float) hwSensorSize.w / (float) hwActiveArraySize.w;
    scaleRatioH = (float) hwSensorSize.h / (float) hwActiveArraySize.h;

    region->x1 = (int) (((float) region->x1) * scaleRatioW);
    region->y1 = (int) (((float) region->y1) * scaleRatioH);
    region->x2 = (int) (((float) region->x2) * scaleRatioW);
    region->y2 = (int) (((float) region->y2) * scaleRatioH);

    hwActiveArraySize.x = (int) (((float) hwActiveArraySize.x) * scaleRatioW);
    hwActiveArraySize.y = (int) (((float) hwActiveArraySize.y) * scaleRatioH);
    hwActiveArraySize.w = (int) (((float) hwActiveArraySize.w) * scaleRatioW);
    hwActiveArraySize.h = (int) (((float) hwActiveArraySize.h) * scaleRatioH);

    /* Remove HW active array size offset */
    /* Top-left */
    if (region->x1 < hwActiveArraySize.x) region->x1 = 0;
    else region->x1 -= hwActiveArraySize.x;
    if (region->y1 < hwActiveArraySize.y) region->y1 = 0;
    else region->y1 -= hwActiveArraySize.y;
    if (region->x2 < hwActiveArraySize.x) region->x2 = 0;
    else region->x2 -= hwActiveArraySize.x;
    if (region->y2 < hwActiveArraySize.y) region->y2 = 0;
    else region->y2 -= hwActiveArraySize.y;

    /* Bottom-right */
    if (region->x1 > hwActiveArraySize.w) region->x1 = hwActiveArraySize.w;
    if (region->y1 > hwActiveArraySize.h) region->y1 = hwActiveArraySize.h;
    if (region->x2 > hwActiveArraySize.w) region->x2 = hwActiveArraySize.w;
    if (region->y2 > hwActiveArraySize.h) region->y2 = hwActiveArraySize.h;

    /* Adjust the boundary of HW bcrop size */
    /* Top-left */
    if (region->x1 < hwBcropSize.x) region->x1 = 0;
    else region->x1 -= hwBcropSize.x;
    if (region->y1 < hwBcropSize.y) region->y1 = 0;
    else region->y1 -= hwBcropSize.y;
    if (region->x2 < hwBcropSize.x) region->x2 = 0;
    else region->x2 -= hwBcropSize.x;
    if (region->y2 < hwBcropSize.y) region->y2 = 0;
    else region->y2 -= hwBcropSize.y;

    /* Bottom-right */
    if (region->x1 > hwBcropSize.w) region->x1 = hwBcropSize.w;
    if (region->y1 > hwBcropSize.h) region->y1 = hwBcropSize.h;
    if (region->x2 > hwBcropSize.w) region->x2 = hwBcropSize.w;
    if (region->y2 > hwBcropSize.h) region->y2 = hwBcropSize.h;

    CLOGV("Region %d,%d %d,%d %dx%d(%d)",
            region->x1, region->y1, region->x2, region->y2, region->x2 - region->x1, region->y2 - region->y1, SIZE_RATIO(region->x2 - region->x1, region->y2 - region->y1));
}

void ExynosCameraMetadataConverter::m_convert3AAToActiveArrayRegion(ExynosRect2 *region)
{
    status_t ret = NO_ERROR;
    ExynosRect activeArraySize;
    ExynosRect hwSensorSize;
    ExynosRect hwActiveArraySize;
    ExynosRect hwBcropSize;
    int offsetX = 0, offsetY = 0;
    float scaleRatioW = 0.0f, scaleRatioH = 0.0f;

    m_parameters->getMaxSensorSize(&activeArraySize.w, &activeArraySize.h);
    m_parameters->getPreviewBayerCropSize(&hwSensorSize, &hwBcropSize);

    /* Calculate HW active array size for current sensor aspect ratio
    based on active array size
    */
    ret = getCropRectAlign(activeArraySize.w, activeArraySize.h,
                           hwSensorSize.w, hwSensorSize.h,
                           &hwActiveArraySize.x, &hwActiveArraySize.y,
                           &hwActiveArraySize.w, &hwActiveArraySize.h,
                           2, 2, 1.0f);
    if (ret != NO_ERROR) {
        CLOGE("Failed to getCropRectAlign. Src %dx%d, Dst %dx%d",
                activeArraySize.w, activeArraySize.h,
                hwSensorSize.w, hwSensorSize.h);
        return;
    }

    /* Add HW bcrop offset */
    offsetX = hwBcropSize.x;
    offsetY = hwBcropSize.y;

    /* Scale down HW active array offset */
    scaleRatioW = (float) hwSensorSize.w / (float) hwActiveArraySize.w;
    scaleRatioH = (float) hwSensorSize.h / (float) hwActiveArraySize.h;

    hwActiveArraySize.x = (int) (((float) hwActiveArraySize.x) * scaleRatioW);
    hwActiveArraySize.y = (int) (((float) hwActiveArraySize.y) * scaleRatioH);

    /* Add HW active array size offset */
    offsetX += hwActiveArraySize.x;
    offsetY += hwActiveArraySize.y;

    region->x1 = (region->x1 + offsetX) / scaleRatioW;
    region->y1 = (region->y1 + offsetY) / scaleRatioH;
    region->x2 = (region->x2 + offsetX) / scaleRatioW;
    region->y2 = (region->y2 + offsetY) / scaleRatioH;
}

void ExynosCameraMetadataConverter::setMetaCtlSceneMode(struct camera2_shot_ext *shot_ext,
                                                        enum aa_scene_mode sceneMode, int mode)
{
    enum processing_mode default_edge_mode = PROCESSING_MODE_FAST;
    enum processing_mode default_noise_mode = PROCESSING_MODE_FAST;
    int default_edge_strength = 5;
    int default_noise_strength = 5;

    shot_ext->shot.ctl.aa.sceneMode = sceneMode;
    if (mode != 0) {
        shot_ext->shot.ctl.aa.mode = (enum aa_mode)mode;
    }

    switch (sceneMode) {
    case AA_SCENE_MODE_ACTION:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PORTRAIT:
    case AA_SCENE_MODE_LANDSCAPE:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT:
        /* AE_LOCK is prohibited */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF ||
            shot_ext->shot.ctl.aa.aeLock == AA_AE_LOCK_ON) {
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
        }

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_NIGHT_PORTRAIT:
    case AA_SCENE_MODE_THEATRE:
    case AA_SCENE_MODE_BEACH:
    case AA_SCENE_MODE_SNOW:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_SUNSET:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_STEADYPHOTO:
    case AA_SCENE_MODE_FIREWORKS:
    case AA_SCENE_MODE_SPORTS:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_PARTY:
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
        shot_ext->shot.ctl.aa.vendor_isoValue = 200;
        shot_ext->shot.ctl.sensor.sensitivity = 200;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 4; /* "4" is default + 1. */
        break;
    case AA_SCENE_MODE_CANDLELIGHT:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
#ifdef SAMSUNG_FOOD_MODE
    case AA_SCENE_MODE_FOOD:
        shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_MATRIX;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */

        shot_ext->shot.ctl.aa.aeExpCompensation = 2;
        break;
#endif
    case AA_SCENE_MODE_AQUA:
        /* set default setting */
        if (shot_ext->shot.ctl.aa.aeMode == AA_AEMODE_OFF)
            shot_ext->shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

        shot_ext->shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
        shot_ext->shot.ctl.aa.vendor_isoValue = 0;
        shot_ext->shot.ctl.sensor.sensitivity = 0;
        shot_ext->shot.ctl.noise.mode = default_noise_mode;
        shot_ext->shot.ctl.noise.strength = default_noise_strength;
        shot_ext->shot.ctl.edge.mode = default_edge_mode;
        shot_ext->shot.ctl.edge.strength = default_edge_strength;
        shot_ext->shot.ctl.color.vendor_saturation = 3; /* "3" is default. */
        break;
    case AA_SCENE_MODE_FACE_PRIORITY:
    default:
        break;
    }
}

status_t ExynosCameraMetadataConverter::checkMetaValid(camera_metadata_tag_t tag, const void *data)
{
    status_t ret = NO_ERROR;
    camera_metadata_entry_t entry;

    int32_t value = 0;
    const int32_t *i32Range = NULL;

    if (m_staticInfo.exists(tag) == false) {
        CLOGE("Cannot find entry, tag(%d)", tag);
        return BAD_VALUE;
    }

    entry = m_staticInfo.find(tag);

    /* TODO: handle all tags
     *       need type check
     */
    switch (tag) {
    case ANDROID_SENSOR_INFO_SENSITIVITY_RANGE:
         value = *(int32_t *)data;
         i32Range = entry.data.i32;
        if (value < i32Range[0] || value > i32Range[1]) {
            CLOGE("Invalid Sensitivity value(%d), range(%d, %d)",
                value, i32Range[0], i32Range[1]);
            ret = BAD_VALUE;
        }
        break;
    default:
        CLOGE("Tag (%d) is not implemented", tag);
        break;
    }

    return ret;
}

status_t ExynosCameraMetadataConverter::getDefaultSetting(camera_metadata_tag_t tag, void *data)
{
    status_t ret = NO_ERROR;
    camera_metadata_entry_t entry;

    const int32_t *i32Range = NULL;

    if (m_defaultRequestSetting.exists(tag) == false) {
        CLOGE("Cannot find entry, tag(%d)", tag);
        return BAD_VALUE;
    }

    entry = m_defaultRequestSetting.find(tag);

    /* TODO: handle all tags
     *       need type check
     */
    switch (tag) {
    case ANDROID_SENSOR_SENSITIVITY:
         i32Range = entry.data.i32;
         *(int32_t *)data = i32Range[0];
        break;
    default:
        CLOGE("Tag (%d) is not implemented", tag);
        break;
    }

    return ret;
}

uint32_t ExynosCameraMetadataConverter::m_getFrameInfoForTimeStamp(enum frame_count_map_item_index index,
                                                                                uint64_t timeStamp)
{
    int mapIndex = (m_frameCountMapIndex + FRAMECOUNT_MAP_LENGTH - 1) % FRAMECOUNT_MAP_LENGTH;

    for (int i = 0; i < FRAMECOUNT_MAP_LENGTH; i++) {
        if (m_frameCountMap[mapIndex][TIMESTAMP] == timeStamp) {
            return m_frameCountMap[mapIndex][index];
        }

        mapIndex = (mapIndex + FRAMECOUNT_MAP_LENGTH - 1) % FRAMECOUNT_MAP_LENGTH;
    }

    return 0;
}
}; /* namespace android */
