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

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraSensorInfoBase"
#include <log/log.h>

#include "ExynosCameraSensorInfoBase.h"
#include "ExynosExif.h"

namespace android {

#ifdef SENSOR_NAME_GET_FROM_FILE
int g_rearSensorId = -1;
int g_rear2SensorId = -1;
int g_frontSensorId = -1;
int g_secureSensorId = -1;
#endif

static camera_metadata_rational UNIT_MATRIX[] =
{
    {1024, 1024}, {   0, 1024}, {   0, 1024},
    {   0, 1024}, {1024, 1024}, {   0, 1024},
    {   0, 1024}, {   0, 1024}, {1024, 1024}
};

static int AVAILABLE_THUMBNAIL_LIST[][SIZE_OF_RESOLUTION] =
{
    /* { width, height, minFrameDuration, ratioId } */
    {  512,  384, 0, SIZE_RATIO_4_3},
    {  512,  288, 0, SIZE_RATIO_16_9},
    {  384,  384, 0, SIZE_RATIO_1_1},
    {    0,    0, 0, SIZE_RATIO_1_1},
};

static int AVAILABLE_HIDDEN_THUMBNAIL_LIST[][2] =
{
    {  512,  248}, /* SIZE_RATIO_18P5_9 */
};

static int AVAILABLE_THUMBNAIL_CALLBACK_SIZE_LIST[][2] =
{
    {  512,  384},
    {  512,  288},
    {  512,  248},
    {  384,  384},
};

static int AVAILABLE_THUMBNAIL_CALLBACK_FORMATS_LIST[] =
{
    HAL_PIXEL_FORMAT_YCbCr_420_888,
};

int getSensorId(int camId)
{
    int sensorId = -1;

#ifdef SENSOR_NAME_GET_FROM_FILE
    int *curSensorId;

    if (camId == CAMERA_ID_BACK)
        curSensorId = &g_rearSensorId;
#ifdef USE_DUAL_CAMERA
    else if (camId == CAMERA_ID_BACK_1)
        curSensorId = &g_rear2SensorId;
#endif
    else if (camId == CAMERA_ID_SECURE)
        curSensorId = &g_secureSensorId;
    else
        curSensorId = &g_frontSensorId;

    if (*curSensorId < 0) {
        *curSensorId = getSensorIdFromFile(camId);
        if (*curSensorId < 0) {
            ALOGE("ERR(%s): invalid sensor ID %d", __FUNCTION__, sensorId);
        }
    }

    sensorId = *curSensorId;
#else
    if (camId == CAMERA_ID_BACK) {
        sensorId = MAIN_CAMERA_SENSOR_NAME;
    } else if (camId == CAMERA_ID_FRONT) {
        sensorId = FRONT_CAMERA_SENSOR_NAME;
#ifdef USE_DUAL_CAMERA
    } else if (camId == CAMERA_ID_BACK_1) {
#ifdef MAIN_1_CAMERA_SENSOR_NAME
        sensorId = MAIN_1_CAMERA_SENSOR_NAME;
#endif
    } else if (camId == CAMERA_ID_FRONT_1) {
#ifdef FRONT_1_CAMERA_SENSOR_NAME
        sensorId = FRONT_1_CAMERA_SENSOR_NAME;
#endif
#endif
    } else if (camId == CAMERA_ID_SECURE) {
        sensorId = SECURE_CAMERA_SENSOR_NAME;
    } else {
        ALOGE("ERR(%s):Unknown camera ID(%d)", __FUNCTION__, camId);
    }
#endif

    if (sensorId == SENSOR_NAME_NOTHING) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):camId(%d):sensorId == SENSOR_NAME_NOTHING, assert!!!!",
            __FUNCTION__, __LINE__, camId);
    }

    return sensorId;
}

#ifdef USE_DUAL_CAMERA
void getDualCameraId(int *cameraId_0, int *cameraId_1)
{
    if (cameraId_0 == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId_0 == NULL, assert!!!!",
            __FUNCTION__, __LINE__);
    }

    if (cameraId_1 == NULL) {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):cameraId_1 == NULL, assert!!!!",
            __FUNCTION__, __LINE__);
    }

    int sensor1Name = -1;
    int tempCameraId_0 = -1;
    int tempCameraId_1 = -1;

#ifdef MAIN_1_CAMERA_SENSOR_NAME
    sensor1Name = MAIN_1_CAMERA_SENSOR_NAME;

    if (sensor1Name != SENSOR_NAME_NOTHING) {
        tempCameraId_0 = CAMERA_ID_BACK;
        tempCameraId_1 = CAMERA_ID_BACK_1;

        goto done;
    }
#endif

#ifdef FRONT_1_CAMERA_SENSOR_NAME
    sensor1Name = FRONT_1_CAMERA_SENSOR_NAME;

    if (sensor1Name != SENSOR_NAME_NOTHING) {
        tempCameraId_0 = CAMERA_ID_FRONT;
        tempCameraId_1 = CAMERA_ID_FRONT_1;

        goto done;
    }
#endif

done:
    *cameraId_0 = tempCameraId_0;
    *cameraId_1 = tempCameraId_1;
}
#endif

ExynosCameraSensorInfoBase::ExynosCameraSensorInfoBase()
{
    /* implement AP chip variation */
    maxPreviewW = 1920;
    maxPreviewH = 1080;
    maxPictureW = 4128;
    maxPictureH = 3096;
    maxSensorW = 4128;
    maxSensorH = 3096;
    sensorMarginW = 16;
    sensorMarginH = 10;
    sensorMarginBase[LEFT_BASE] = 0;
    sensorMarginBase[TOP_BASE] = 0;
    sensorMarginBase[WIDTH_BASE] = 0;
    sensorMarginBase[HEIGHT_BASE] = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    minFps = 7;
    maxFps = 30;
    defaultFpsMin[DEFAULT_FPS_STILL] = 15;
    defaultFpsMAX[DEFAULT_FPS_STILL] = 30;
    defaultFpsMin[DEFAULT_FPS_VIDEO] = 30;
    defaultFpsMAX[DEFAULT_FPS_VIDEO] = 30;
    defaultFpsMin[DEFAULT_FPS_EFFECT_STILL] = 10;
    defaultFpsMAX[DEFAULT_FPS_EFFECT_STILL] = 24;
    defaultFpsMin[DEFAULT_FPS_EFFECT_VIDEO] = 24;
    defaultFpsMAX[DEFAULT_FPS_EFFECT_VIDEO] = 24;

    horizontalViewAngle[SIZE_RATIO_16_9] = 62.2f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 48.8f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 37.4f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 55.2f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 48.8f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 58.4f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 48.8f;
    verticalViewAngle = 39.4f;

    bnsSupport = false;
    /* flite->3aa otf support */
    flite3aaOtfSupport = true;

    gain = 0;
    exposureTime = 0L;
    ledCurrent = 0;
    ledPulseDelay = 0L;
    ledPulseWidth = 0L;
    ledMaxTime = 0;

    gainRange[MIN] = 0;
    gainRange[MAX] = 0;
    ledCurrentRange[MIN] = 0;
    ledCurrentRange[MAX] = 0;
    ledPulseDelayRange[MIN] = 0;
    ledPulseDelayRange[MAX] = 0;
    ledPulseWidthRange[MIN] = 0;
    ledPulseWidthRange[MAX] = 0;
    ledMaxTimeRange[MIN] = 0;
    ledMaxTimeRange[MAX] = 0;

    yuvListMax = 0;
    jpegListMax = 0;
    thumbnailListMax =
        sizeof(AVAILABLE_THUMBNAIL_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax = 0;
    fpsRangesListMax = 0;
    highSpeedVideoFPSListMax = 0;

    yuvList = NULL;
    jpegList = NULL;
    thumbnailList = AVAILABLE_THUMBNAIL_LIST;
    highSpeedVideoList = NULL;
    fpsRangesList = NULL;
    highSpeedVideoFPSList = NULL;

    previewSizeLutMax           = 0;
    previewFullSizeLutMax       = 0;
    dualPreviewSizeLutMax       = 0;
    pictureSizeLutMax           = 0;
    pictureFullSizeLutMax       = 0;
    videoSizeLutMax             = 0;
    videoSizeLutHighSpeed60Max  = 0;
    videoSizeLutHighSpeed120Max = 0;
    videoSizeLutHighSpeed240Max = 0;
    videoSizeLutHighSpeed480Max = 0;
    vtcallSizeLutMax            = 0;
    fastAeStableLutMax          = 0;
    depthMapSizeLutMax          = 0;

    previewSizeLut              = NULL;
    previewFullSizeLut          = NULL;
    dualPreviewSizeLut          = NULL;
    pictureSizeLut              = NULL;
    pictureFullSizeLut          = NULL;
    videoSizeLut                = NULL;
    videoSizeLutHighSpeed60     = NULL;
    videoSizeLutHighSpeed120    = NULL;
    videoSizeLutHighSpeed240    = NULL;
    videoSizeLutHighSpeed480    = NULL;
    vtcallSizeLut               = NULL;
    fastAeStableLut             = NULL;
    depthMapSizeLut             = NULL;

    sizeTableSupport      = false;

#ifdef SAMSUNG_SSM
    videoSizeLutSSMMax         = 0;
    videoSizeLutSSM            = NULL;
#endif

    /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */
    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = NULL;
    colorAberrationModesLength = 0;

    /* Android Control Static Metadata */
    antiBandingModes = NULL;
    aeModes = NULL;
    exposureCompensationRange[MIN] = -2;
    exposureCompensationRange[MAX] = 2;
    exposureCompensationStep = 1.0f;
    afModes = NULL;
    effectModes = NULL;
    sceneModes = NULL;
    videoStabilizationModes = NULL;
    awbModes = NULL;
    controlModes = NULL;
    controlModesLength = 0;
    max3aRegions[AE] = 0;
    max3aRegions[AWB] = 0;
    max3aRegions[AF] = 0;
    sceneModeOverrides = NULL;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_FALSE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_FALSE;
    antiBandingModesLength = 0;
    aeModesLength = 0;
    afModesLength = 0;
    effectModesLength = 0;
    sceneModesLength = 0;
    videoStabilizationModesLength = 0;
    awbModesLength = 0;
    sceneModeOverridesLength = 0;
    postRawSensitivityBoost[MIN] = 100;
    postRawSensitivityBoost[MAX] = 100;

    /* Android Edge Static Metadata */
    edgeModes = NULL;
    edgeModesLength = 0;

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_FALSE;
    chargeDuration = 0L;
    colorTemperature = 0;
    maxEnergy = 0;

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = NULL;
    hotPixelModesLength = 0;

    /* Android Lens Static Metadata */
    aperture = 2.2f;
    fNumber = 2.2f;
    filterDensity = 0.0f;
    focalLength = 4.8f;
    focalLengthIn35mmLength = 31;
    opticalStabilization = NULL;
    hyperFocalDistance = 0.0f;
    minimumFocusDistance = 0.0f;
    shadingMapSize[WIDTH] = 0;
    shadingMapSize[HEIGHT] = 0;
    focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    lensFacing = ANDROID_LENS_FACING_BACK;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 0.0f;
    lensPosition[Z_3D] = 0.0f;
    opticalStabilizationLength = 0;

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = NULL;
    noiseReductionModesLength = 0;

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 0;
    maxPipelineDepth = 5;
    partialResultCount = 1;
    requestKeys = NULL;
    resultKeys = NULL;
    characteristicsKeys = NULL;
    sessionKeys = NULL;
    requestKeysLength = 0;
    resultKeysLength = 0;
    characteristicsKeysLength = 0;
    sessionKeysLength = 0;

    /* Android Scaler Static Metadata */
    zoomSupport = false;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = NULL;
    stallDurationsLength = 0;
    croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 100;
    sensitivityRange[MAX] = 1600;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    exposureTimeRange[MIN] = 14000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 125000000L;
    sensorPhysicalSize[WIDTH] = 3.20f;
    sensorPhysicalSize[HEIGHT] = 2.40f;
    whiteLevel = 4000;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 1000;
    blackLevelPattern[GR] = 1000;
    blackLevelPattern[GB] = 1000;
    blackLevelPattern[B] = 1000;
    maxAnalogSensitivity = 800;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = NULL;
    testPatternModesLength = 0;
    colorTransformMatrix1 = UNIT_MATRIX;
    colorTransformMatrix2 = UNIT_MATRIX;
    forwardMatrix1 = UNIT_MATRIX;
    forwardMatrix2 = UNIT_MATRIX;
    calibration1 = UNIT_MATRIX;
    calibration2 = UNIT_MATRIX;
    masterRGain = 0.0f;
    masterBGain = 0.0f;

    /* Android Statistics Static Metadata */
    faceDetectModes = NULL;
    histogramBucketCount = 64;
    maxNumDetectedFaces = 1;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = NULL;
    faceDetectModesLength = 0;
    hotPixelMapModesLength = 0;
    lensShadingMapModes = NULL;
    lensShadingMapModesLength = 0;
    shadingAvailableModes = NULL;
    shadingAvailableModesLength = 0;

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = NULL;
    toneMapModesLength = 0;

    /* Android LED Static Metadata */
    leds = NULL;
    ledsLength = 0;

    /* Android Reprocess Static Metadata */
    maxCaptureStall = 4;

    /* Android Info Static Metadata */
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    supportedCapabilities = CAPABILITIES_BACKWARD_COMPATIBLE;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

    availableThumbnailCallbackSizeListMax =
        sizeof(AVAILABLE_THUMBNAIL_CALLBACK_SIZE_LIST) / (sizeof(int) * 2);
    availableThumbnailCallbackSizeList =
        AVAILABLE_THUMBNAIL_CALLBACK_SIZE_LIST;
    availableThumbnailCallbackFormatListMax =
        sizeof(AVAILABLE_THUMBNAIL_CALLBACK_FORMATS_LIST) / sizeof(int);
    availableThumbnailCallbackFormatList =
        AVAILABLE_THUMBNAIL_CALLBACK_FORMATS_LIST;

    availableIrisSizeListMax = 0;
    availableIrisSizeList = NULL;
    availableIrisFormatListMax = 0;
    availableIrisFormatList = NULL;

#ifdef SUPPORT_DEPTH_MAP
    availableDepthSizeListMax = 0;
    availableDepthSizeList = NULL;
    availableDepthFormatListMax = 0;
    availableDepthFormatList = NULL;
#endif

    availableVideoListMax = 0;
    availableVideoList = NULL;

    availableHighSpeedVideoListMax = 0;
    availableHighSpeedVideoList = NULL;

    hiddenPreviewListMax = 0;
    hiddenPreviewList = NULL;

    hiddenPictureListMax = 0;
    hiddenPictureList = NULL;

    hiddenThumbnailListMax =
        sizeof(AVAILABLE_HIDDEN_THUMBNAIL_LIST) / (sizeof(int) * 2);
    hiddenThumbnailList = AVAILABLE_HIDDEN_THUMBNAIL_LIST;

    /* Samsung Vendor Feature */
    /* Samsung Vendor Control Metadata */
#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = NULL;
    vendorMeteringModesLength = 0;
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 0;
    vendorHdrModes = NULL;
    vendorHdrModesLength = 0;
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 0;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = NULL;
    vendorOISModesLength = 0;
#endif
    vendorAwbModes = NULL;
    vendorAwbModesLength = 0;
    vendorWbColorTemp = 0;
    vendorWbColorTempRange[MIN] = 0;
    vendorWbColorTempRange[MAX] = 0;
    vendorWbLevelRange[MIN] = 0;
    vendorWbLevelRange[MAX] = 0;

    /* Samsung Vendor Scaler Metadata */
    vendorFlipModes = NULL;
    vendorFlipModesLength = 0;

#ifdef SUPPORT_MULTI_AF
    vendorMultiAfAvailable = NULL;
    vendorMultiAfAvailableLength = 0;
#endif

    /* Samsung Vendor Feature Available & Support */
    sceneHDRSupport = false;
    screenFlashAvailable = false;
    objectTrackingAvailable = false;
    fixedFaceFocusAvailable = false;
    factoryDramTestCount = 0;

#ifdef SAMSUNG_TN_FEATURE
    availableBasicFeaturesListLength = ARRAY_LENGTH(AVAILABLE_VENDOR_BASIC_FEATURES);
    availableBasicFeaturesList = AVAILABLE_VENDOR_BASIC_FEATURES;
#else
    availableBasicFeaturesListLength = 0;
    availableBasicFeaturesList = NULL;
#endif
    availableOptionalFeaturesListLength = 0;
    availableOptionalFeaturesList = NULL;

    effectFpsRangesListMax = 0;
    effectFpsRangesList = NULL;

    availableApertureValues = NULL;
    availableApertureValuesLength = 0;

    availableBurstShotFps = NULL;
    availableBurstShotFpsLength = 0;

    /* END of Camera HAL 3.2 Static Metadatas */
}

ExynosCameraSensor2L7Base::ExynosCameraSensor2L7Base() : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4032;
    maxSensorH = 3024;
    maxPreviewW = 4032;
    maxPreviewH = 3024;
    maxPictureW = 4032;
    maxPictureH = 3024;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_S5K2L7)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_S5K2L7)              / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_S5K2L7)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_S5K2L7)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_S5K2L7)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_S5K2L7)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_S5K2L7)   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_S5K2L7)   / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_S5K2L7)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_S5K2L7)            / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_S5K2L7;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_S5K2L7;
    pictureSizeLut              = PICTURE_SIZE_LUT_S5K2L7;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_S5K2L7;
    videoSizeLut                = VIDEO_SIZE_LUT_S5K2L7;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_S5K2L7;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_S5K2L7;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_S5K2L7;
    vtcallSizeLut               = VTCALL_SIZE_LUT_S5K2L7;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_S5K2L7;

    yuvListMax                  = sizeof(S5K2L7_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(S5K2L7_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(S5K2L7_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(S5K2L7_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(S5K2L7_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    yuvList                     = S5K2L7_YUV_LIST;
    jpegList                    = S5K2L7_JPEG_LIST;
    highSpeedVideoList          = S5K2L7_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = S5K2L7_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = S5K2L7_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING | CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.70f;
    fNumber = 1.7f;
    filterDensity = 0.0f;
    focalLength = 4.2f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.0f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 50;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 125000000L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    colorTransformMatrix1 = COLOR_MATRIX1_2L7_3X3;
    colorTransformMatrix2 = COLOR_MATRIX2_2L7_3X3;
    forwardMatrix1 = FORWARD_MATRIX1_2L7_3X3;
    forwardMatrix2 = FORWARD_MATRIX2_2L7_3X3;

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    availableVideoListMax  = sizeof(S5K2L7_VIDEO_LIST) / (sizeof(int) * 7);
    availableVideoList     = S5K2L7_VIDEO_LIST;
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor2P8Base::ExynosCameraSensor2P8Base() : ExynosCameraSensorInfoBase()
{
    maxSensorW = 5328;
    maxSensorH = 3000;
    maxPreviewW = 5328;
    maxPreviewH = 3000;
    maxPictureW = 5328;
    maxPictureH = 3000;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 16;
    sensorMarginH = 12;
    sensorMarginBase[LEFT_BASE] = 2;
    sensorMarginBase[TOP_BASE] = 2;
    sensorMarginBase[WIDTH_BASE] = 4;
    sensorMarginBase[HEIGHT_BASE] = 4;
    sensorArrayRatio = SIZE_RATIO_16_9;

    bnsSupport = false;
    sizeTableSupport            = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_2P8_BDS)              / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_2P8)             / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_2P8)                  / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_2P8)             / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_2P8_BDS_DIS_FHD)        / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_2P8)        / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2P8)  / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2P8)  / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_2P8)                   / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_2P8)           / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_2P8_BDS;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_2P8;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_2P8_BDS;
    pictureSizeLut              = PICTURE_SIZE_LUT_2P8;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_2P8;
    videoSizeLut                = VIDEO_SIZE_LUT_2P8_BDS_DIS_FHD;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_2P8;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2P8;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2P8;
    vtcallSizeLut               = VTCALL_SIZE_LUT_2P8;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_2P8;

    yuvListMax                  = sizeof(S5K2P8_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(S5K2P8_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(S5K2P8_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(S5K2P8_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(S5K2P8_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    yuvList                 = S5K2P8_YUV_LIST;
    jpegList                = S5K2P8_JPEG_LIST;
    highSpeedVideoList      = S5K2P8_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList           = S5K2P8_FPS_RANGE_LIST;
    highSpeedVideoFPSList   = S5K2P8_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_BACKWARD_COMPATIBLE | CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE | CAPABILITIES_RAW | CAPABILITIES_PRIVATE_REPROCESSING);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 2.2f;
    fNumber = 2.2f;
    filterDensity = 0.0f;
    focalLength = 4.8f;
    focalLengthIn35mmLength = 31;
    hyperFocalDistance = 1.0f / 5.0f;
    minimumFocusDistance = 1.0f / 0.05f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1; //RAW
    maxNumOutputStreams[PROCESSED] = 3; //PROC
    maxNumOutputStreams[PROCESSED_STALL] = 1; //PROC_STALL
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_CENTER_ONLY;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 100;
    sensitivityRange[MAX] = 1600;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    exposureTimeRange[MIN] = 14000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    sensorPhysicalSize[WIDTH] = 3.20f;
    sensorPhysicalSize[HEIGHT] = 2.40f;
    whiteLevel = 4000;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    blackLevelPattern[R] = 1000;
    blackLevelPattern[GR] = 1000;
    blackLevelPattern[GB] = 1000;
    blackLevelPattern[B] = 1000;
    maxAnalogSensitivity = 800;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    colorTransformMatrix1 = COLOR_MATRIX1_2P8_3X3;
    colorTransformMatrix2 = COLOR_MATRIX2_2P8_3X3;
    forwardMatrix1 = FORWARD_MATRIX1_2P8_3X3;
    forwardMatrix2 = FORWARD_MATRIX2_2P8_3X3;
    calibration1 = UNIT_MATRIX_2P8_3X3;
    calibration2 = UNIT_MATRIX_2P8_3X3;

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[0] = 64;
    sharpnessMapSize[1] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 56.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 43.4f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 33.6f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 55.2f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 48.8f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 58.4f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 48.8f;
    verticalViewAngle = 39.4f;

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    availableVideoListMax   = sizeof(S5K2P8_VIDEO_LIST) / (sizeof(int) * 7);
    availableVideoList      = S5K2P8_VIDEO_LIST;
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensorIMX333_2L2Base::ExynosCameraSensorIMX333_2L2Base(__unused int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4032;
    maxSensorH = 3024;
    maxPreviewW = 4032;
    maxPreviewH = 3024;
    maxPictureW = 4032;
    maxPictureH = 3024;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_IMX333_2L2)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_IMX333_2L2)              / (sizeof(int) * SIZE_OF_LUT);
    dualPreviewSizeLutMax       = sizeof(PREVIEW_SIZE_LUT_IMX333_2L2)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_IMX333_2L2)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_IMX333_2L2)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_IMX333_2L2)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_IMX333_2L2)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX333_2L2)   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_IMX333_2L2)   / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_IMX333_2L2)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_IMX333_2L2)            / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_IMX333_2L2;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_IMX333_2L2;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_IMX333_2L2;
    pictureSizeLut              = PICTURE_SIZE_LUT_IMX333_2L2;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_IMX333_2L2;
    videoSizeLut                = VIDEO_SIZE_LUT_IMX333_2L2;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_IMX333_2L2;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX333_2L2;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_IMX333_2L2;
    vtcallSizeLut               = VTCALL_SIZE_LUT_IMX333_2L2;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_IMX333_2L2;

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(IMX333_2L2_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(IMX333_2L2_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(IMX333_2L2_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(IMX333_2L2_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(IMX333_2L2_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    /* Set supported  size/fps lists */
    yuvList                     = IMX333_2L2_YUV_LIST;
    jpegList                    = IMX333_2L2_JPEG_LIST;
    highSpeedVideoList          = IMX333_2L2_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = IMX333_2L2_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = IMX333_2L2_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.70f;
    fNumber = 1.7f;
    filterDensity = 0.0f;
    focalLength = 4.2f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.66f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 50;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 125000000L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    if (sensorId == SENSOR_NAME_IMX333) {
        colorTransformMatrix1 = COLOR_MATRIX1_IMX333_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_IMX333_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_IMX333_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_IMX333_3X3;
    } else {
        colorTransformMatrix1 = COLOR_MATRIX1_2L2_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_2L2_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_2L2_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_2L2_3X3;
    }

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    availableVideoListMax          = sizeof(IMX333_2L2_AVAILABLE_VIDEO_LIST)            / (sizeof(int) * 7);
    availableVideoList             = IMX333_2L2_AVAILABLE_VIDEO_LIST;

    availableHighSpeedVideoListMax = sizeof(IMX333_2L2_AVAILABLE_HIGH_SPEED_VIDEO_LIST) / (sizeof(int) * 5);
    availableHighSpeedVideoList    = IMX333_2L2_AVAILABLE_HIGH_SPEED_VIDEO_LIST;

    hiddenPreviewListMax          = sizeof(IMX333_2L2_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList             = IMX333_2L2_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax          = sizeof(IMX333_2L2_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList             = IMX333_2L2_HIDDEN_PICTURE_SIZE_LIST;

    /* Samsung Vendor Feature */
#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 1;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = AVAILABLE_VENDOR_OIS_MODES;
    vendorOISModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_OIS_MODES);
#endif

    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);

    vendorWbColorTempRange[MIN] = 2300;
    vendorWbColorTempRange[MAX] = 10000;
    vendorWbLevelRange[MIN] = -4;
    vendorWbLevelRange[MAX] = 4;

#ifdef SUPPORT_MULTI_AF
    vendorMultiAfAvailable = AVAILABLE_VENDOR_MULTI_AF_MODE;
    vendorMultiAfAvailableLength = ARRAY_LENGTH(AVAILABLE_VENDOR_MULTI_AF_MODE);
#endif
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor2L3Base::ExynosCameraSensor2L3Base(__unused int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4032;
    maxSensorH = 3024;
    maxPreviewW = 4032;
    maxPreviewH = 3024;
    maxPictureW = 4032;
    maxPictureH = 3024;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_2L3)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_2L3)              / (sizeof(int) * SIZE_OF_LUT);
    dualPreviewSizeLutMax       = sizeof(PREVIEW_SIZE_LUT_2L3)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_2L3)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_2L3)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_2L3)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_2L3)                      / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2L3)   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2L3)   / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_2L3)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_2L3)            / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_2L3;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_2L3;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_2L3;
    pictureSizeLut              = PICTURE_SIZE_LUT_2L3;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_2L3;
    videoSizeLut                = VIDEO_SIZE_LUT_2L3;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_2L3;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2L3;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2L3;
    vtcallSizeLut               = VTCALL_SIZE_LUT_2L3;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_2L3;

#ifdef SAMSUNG_SSM
    videoSizeLutSSMMax          = sizeof(VIDEO_SIZE_LUT_SSM_2L3) / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutSSM             = VIDEO_SIZE_LUT_SSM_2L3;
#endif

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(SAK2L3_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(SAK2L3_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(SAK2L3_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(SAK2L3_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(SAK2L3_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    /* Set supported  size/fps lists */
    yuvList                     = SAK2L3_YUV_LIST;
    jpegList                    = SAK2L3_JPEG_LIST;
    highSpeedVideoList          = SAK2L3_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = SAK2L3_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = SAK2L3_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE | CAPABILITIES_RAW | CAPABILITIES_PRIVATE_REPROCESSING);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    sessionKeys = AVAILABLE_SESSION_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);
    sessionKeysLength = ARRAY_LENGTH(AVAILABLE_SESSION_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.5f;
    fNumber = 1.5f;
    filterDensity = 0.0f;
    focalLength = 4.3f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.00f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 40;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 64;
    blackLevelPattern[GR] = 64;
    blackLevelPattern[GB] = 64;
    blackLevelPattern[B] = 64;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    if (sensorId == SENSOR_NAME_IMX333) {
        colorTransformMatrix1 = COLOR_MATRIX1_IMX333_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_IMX333_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_IMX333_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_IMX333_3X3;
    } else {
        colorTransformMatrix1 = COLOR_MATRIX1_2L3_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_2L3_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_2L3_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_2L3_3X3;
    }

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.1f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    horizontalViewAngle[SIZE_RATIO_9_16] = 27.4f;
    horizontalViewAngle[SIZE_RATIO_18P5_9] = 65.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    /* Samsung Vendor Feature */
    availableVideoListMax          = sizeof(SAK2L3_AVAILABLE_VIDEO_LIST)            / (sizeof(int) * 7);
    availableVideoList             = SAK2L3_AVAILABLE_VIDEO_LIST;

    availableHighSpeedVideoListMax = sizeof(SAK2L3_AVAILABLE_HIGH_SPEED_VIDEO_LIST) / (sizeof(int) * 5);
    availableHighSpeedVideoList    = SAK2L3_AVAILABLE_HIGH_SPEED_VIDEO_LIST;

    hiddenPreviewListMax          = sizeof(SAK2L3_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList             = SAK2L3_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax          = sizeof(SAK2L3_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList             = SAK2L3_HIDDEN_PICTURE_SIZE_LIST;

#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 1;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = AVAILABLE_VENDOR_OIS_MODES;
    vendorOISModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_OIS_MODES);
#endif

    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);

    vendorWbColorTempRange[MIN] = 2300;
    vendorWbColorTempRange[MAX] = 10000;
    vendorWbLevelRange[MIN] = -4;
    vendorWbLevelRange[MAX] = 4;

#ifdef SUPPORT_MULTI_AF
    vendorMultiAfAvailable = AVAILABLE_VENDOR_MULTI_AF_MODE;
    vendorMultiAfAvailableLength = ARRAY_LENGTH(AVAILABLE_VENDOR_MULTI_AF_MODE);
#endif

    factoryDramTestCount = 16;
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor6B2Base::ExynosCameraSensor6B2Base(__unused int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 1936;
    maxSensorH = 1090;
    maxPreviewW = 1920;
    maxPreviewH = 1080;
    maxPictureW = 1920;
    maxPictureH = 1080;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_16_9;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_6B2)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_6B2)              / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_6B2)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_6B2)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_6B2)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = 0;
    videoSizeLutHighSpeed120Max = 0;
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_6B2)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = 0;

    previewSizeLut              = PREVIEW_SIZE_LUT_6B2;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_6B2;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_6B2;
    pictureSizeLut              = PICTURE_SIZE_LUT_6B2;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_6B2;
    videoSizeLut                = VIDEO_SIZE_LUT_6B2;
    videoSizeLutHighSpeed60     = NULL;
    videoSizeLutHighSpeed120    = NULL;
    vtcallSizeLut               = VTCALL_SIZE_LUT_6B2;
    fastAeStableLut             = NULL;

    /* Set the max of size/fps lists */
    yuvListMax              = sizeof(S5K6B2_YUV_LIST)               / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax             = sizeof(S5K6B2_JPEG_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    thumbnailListMax        = sizeof(S5K6B2_THUMBNAIL_LIST)         / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax        = sizeof(S5K6B2_FPS_RANGE_LIST)         / (sizeof(int) * 2);

    /* Set supported size/fps lists */
    yuvList                 = S5K6B2_YUV_LIST;
    jpegList                = S5K6B2_JPEG_LIST;
    thumbnailList           = S5K6B2_THUMBNAIL_LIST;
    fpsRangesList           = S5K6B2_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_FRONT;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_BACKWARD_COMPATIBLE | CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE | CAPABILITIES_RAW | CAPABILITIES_PRIVATE_REPROCESSING);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 0;
    max3aRegions[AWB] = 0;
    max3aRegions[AF] = 0;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_FALSE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 2.4f;
    fNumber = 2.4f;
    filterDensity = 0.0f;
    focalLength = 1.86f;
    focalLengthIn35mmLength = 27;
    hyperFocalDistance = 0.0f;
    minimumFocusDistance = 0.0f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 20.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = 0.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1; //RAW
    maxNumOutputStreams[PROCESSED] = 3; //PROC
    maxNumOutputStreams[PROCESSED_STALL] = 1; //PROC_STALL
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO_FRONT;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 100;
    sensitivityRange[MAX] = 1600;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    exposureTimeRange[MIN] = 14000L;
    exposureTimeRange[MAX] = 250000000L;
    maxFrameDuration = 250000000L;
    sensorPhysicalSize[WIDTH] = 3.495f;
    sensorPhysicalSize[HEIGHT] = 2.626f;
    whiteLevel = 4000;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    blackLevelPattern[R] = 1000;
    blackLevelPattern[GR] = 1000;
    blackLevelPattern[GB] = 1000;
    blackLevelPattern[B] = 1000;
    maxAnalogSensitivity = 800;
    orientation = FRONT_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[0] = 64;
    sharpnessMapSize[1] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 69.7f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 54.2f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 42.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 60.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 54.2f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 64.8f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 54.2f;
    verticalViewAngle = 39.4f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensorIMX320_3H1Base::ExynosCameraSensorIMX320_3H1Base(__unused int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 3264;
    maxSensorH = 2448;
    maxPreviewW = 3264;
    maxPreviewH = 2448;
    maxPictureW = 3264;
    maxPictureH = 2448;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_IMX320_3H1)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_IMX320_3H1)              / (sizeof(int) * SIZE_OF_LUT);
    dualPreviewSizeLutMax       = sizeof(PREVIEW_SIZE_LUT_IMX320_3H1)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_IMX320_3H1)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_IMX320_3H1)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_IMX320_3H1)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_IMX320_3H1)    / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX320_3H1)   / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_IMX320_3H1)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX320_3H1)   / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_IMX320_3H1;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_IMX320_3H1;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_IMX320_3H1;
    pictureSizeLut              = PICTURE_SIZE_LUT_IMX320_3H1;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_IMX320_3H1;
    videoSizeLut                = VIDEO_SIZE_LUT_IMX320_3H1;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_IMX320_3H1;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX320_3H1;
    vtcallSizeLut               = VTCALL_SIZE_LUT_IMX320_3H1;
    fastAeStableLut             = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_IMX320_3H1;

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(IMX320_3H1_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(IMX320_3H1_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(IMX320_3H1_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(IMX320_3H1_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(IMX320_3H1_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    /* Set supported size/fps lists */
    yuvList                 = IMX320_3H1_YUV_LIST;
    jpegList                = IMX320_3H1_JPEG_LIST;
    highSpeedVideoList      = IMX320_3H1_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList           = IMX320_3H1_FPS_RANGE_LIST;
    highSpeedVideoFPSList   = IMX320_3H1_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_FRONT;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LIMITED;
    /* Limited-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    sessionKeys = AVAILABLE_SESSION_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);
    sessionKeysLength = ARRAY_LENGTH(AVAILABLE_SESSION_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_FALSE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.7f;
    fNumber = 1.7f;
    filterDensity = 0.0f;
    focalLength = 2.92f;
    focalLengthIn35mmLength = 25;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.0f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 20.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = 0.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1; //RAW
    maxNumOutputStreams[PROCESSED] = 3; //PROC
    maxNumOutputStreams[PROCESSED_STALL] = 1; //PROC_STALL
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO_FRONT;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_FRONT;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 40;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    sensorPhysicalSize[WIDTH] = 3.982f;
    sensorPhysicalSize[HEIGHT] = 2.987f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 800;
    orientation = FRONT_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[0] = 64;
    sharpnessMapSize[1] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 68.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 68.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 53.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 68.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 64.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 67.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 63.0f;
    verticalViewAngle = 53.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    availableVideoListMax      = sizeof(IMX320_3H1_AVAILABLE_VIDEO_LIST)   / (sizeof(int) * 7);
    availableVideoList         = IMX320_3H1_AVAILABLE_VIDEO_LIST;

    hiddenPreviewListMax       = sizeof(IMX320_3H1_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList          = IMX320_3H1_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax       = sizeof(IMX320_3H1_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList          = IMX320_3H1_HIDDEN_PICTURE_SIZE_LIST;

#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
    vendorFlipModes = AVAILABLE_VENDOR_FLIP_MODES;
    vendorFlipModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_FLIP_MODES);
    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor3M3Base::ExynosCameraSensor3M3Base(__unused int cameraId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4032;
    maxSensorH = 3024;
    maxPreviewW = 4032;
    maxPreviewH = 3024;
    maxPictureW = 4032;
    maxPictureH = 3024;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    /*
    zoomSupport = true;
    videoSnapshotSupport = true;
    videoStabilizationSupport = false;
    autoWhiteBalanceLockSupport = true;
    autoExposureLockSupport = true;
    */

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_3M3)    / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_3M3)    / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_3M3)        / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_3M3)             / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_3M3)      / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_3M3) / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_3M3) / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_3M3)     / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_3M3) / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut        = PREVIEW_SIZE_LUT_3M3;
    previewFullSizeLut    = PREVIEW_FULL_SIZE_LUT_3M3;
    pictureSizeLut        = PICTURE_SIZE_LUT_3M3;
    pictureFullSizeLut    = PICTURE_FULL_SIZE_LUT_3M3;
    videoSizeLut          = VIDEO_SIZE_LUT_3M3;
    videoSizeLutHighSpeed60  = VIDEO_SIZE_LUT_60FPS_HIGH_SPEED_3M3;
    videoSizeLutHighSpeed120 = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_3M3;
    vtcallSizeLut         = VTCALL_SIZE_LUT_3M3;
    fastAeStableLut       = FAST_AE_STABLE_SIZE_LUT_3M3;


    /* Set the max of size/fps lists */
    yuvListMax              = sizeof(S5K3M3_YUV_LIST)               / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax             = sizeof(S5K3M3_JPEG_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax        = sizeof(S5K3M3_FPS_RANGE_LIST)         / (sizeof(int) * 2);

    /* Set supported size/fps lists */
    yuvList                 = S5K3M3_YUV_LIST;
    jpegList                = S5K3M3_JPEG_LIST;
    fpsRangesList           = S5K3M3_FPS_RANGE_LIST;

  /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 2.4f;
    fNumber = 2.4f;
    filterDensity = 0.0f;
    focalLength = 6.0f;
    focalLengthIn35mmLength = 52;
    hyperFocalDistance = 1.0f / 7.2f;
    minimumFocusDistance = 1.0f / 0.5f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 0;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 25;
    sensitivityRange[MAX] = 1600;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    /* pixel size : 1um , 12M sensor */
    sensorPhysicalSize[WIDTH] = 4.032f;
    sensorPhysicalSize[HEIGHT] = 3.024001f; /* ITS test_metadata : float precision issue */
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);

    colorTransformMatrix1 = COLOR_MATRIX1_3M3_3X3; /*Copied from 2L1 setting */
    colorTransformMatrix2 = COLOR_MATRIX2_3M3_3X3; /* Copied from 2L1 setting */
    forwardMatrix1 = FORWARD_MATRIX1_3M3_3X3; /* Copied from 2L1 setting */
    forwardMatrix2 = FORWARD_MATRIX2_3M3_3X3; /* Copied from 2L1 setting */

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    availableVideoListMax   = sizeof(S5K3M3_AVAILABLE_VIDEO_LIST) / (sizeof(int) * 7);
    availableVideoList      = S5K3M3_AVAILABLE_VIDEO_LIST;

    hiddenPreviewListMax    = sizeof(S5K3M3_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList       = S5K3M3_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax    = sizeof(S5K3M3_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList       = S5K3M3_HIDDEN_PICTURE_SIZE_LIST;

#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 1;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = AVAILABLE_VENDOR_OIS_MODES;
    vendorOISModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_OIS_MODES);
#endif

    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);

    vendorWbColorTempRange[MIN] = 2300;
    vendorWbColorTempRange[MAX] = 10000;
    vendorWbLevelRange[MIN] = -4;
    vendorWbLevelRange[MAX] = 4;

#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensorS5K5F1Base::ExynosCameraSensorS5K5F1Base(__unused int cameraId) : ExynosCameraSensorInfoBase()
{
    maxPreviewW = 2400;
    maxPreviewH = 2400;
    maxSensorW = 2400;
    maxSensorH = 2400;
    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorMarginBase[LEFT_BASE] = 0;
    sensorMarginBase[TOP_BASE] = 0;
    sensorMarginBase[WIDTH_BASE] = 0;
    sensorMarginBase[HEIGHT_BASE] = 0;

    sensorArrayRatio = SIZE_RATIO_1_1;

    bnsSupport = false;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_5F1) / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_5F1) / (sizeof(int) * SIZE_OF_LUT);
    previewSizeLut              = PREVIEW_SIZE_LUT_5F1;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_5F1;
    sizeTableSupport            = true;

    /* Set the max of size/fps lists */
    yuvListMax              = sizeof(S5K5F1_YUV_LIST) / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax        = sizeof(S5K5F1_FPS_RANGE_LIST) / (sizeof(int) * 2);
    thumbnailListMax = 0;
    hiddenThumbnailListMax = 0;

    /* Set supported size/fps lists */
    yuvList                 = S5K5F1_YUV_LIST;
    fpsRangesList           = S5K5F1_FPS_RANGE_LIST;
    thumbnailList = NULL;
    hiddenThumbnailList = NULL;

    /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */

    lensFacing = ANDROID_LENS_FACING_FRONT;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY;
    requestKeys = AVAILABLE_REQUEST_KEYS_LEGACY;
    resultKeys = AVAILABLE_RESULT_KEYS_LEGACY;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_LEGACY;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_LEGACY);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_LEGACY);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_LEGACY);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    max3aRegions[AE] = 0;
    max3aRegions[AWB] = 0;
    max3aRegions[AF] = 0;

    /* Android Edge Static Metadata */

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_FALSE;

    /* Android Hot Pixel Static Metadata */

    /* Android Lens Static Metadata */
    aperture = 1.9f;
    fNumber = 1.9f;
    filterDensity = 0.0f;
    focalLength = 2.2f;
    focalLengthIn35mmLength = 22;
    hyperFocalDistance = 0.0f;
    minimumFocusDistance = 0.0f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 20.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = 0.0f;

    /* Android Noise Reduction Static Metadata */

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 0; //RAW
    maxNumOutputStreams[PROCESSED] = 0; //PROC
    maxNumOutputStreams[PROCESSED_STALL] = 0; //PROC_STALL
    maxNumInputStreams = 0;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = false;
    maxZoomRatio = 1000;
    maxZoomRatioVendor = 1000;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 100;
    sensitivityRange[MAX] = 1600;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;
    exposureTimeRange[MIN] = 100000L; //   0.1ms
    exposureTimeRange[MAX] = 33200000L; //  33.2ms
    maxFrameDuration = 125000000L;
    sensorPhysicalSize[WIDTH] = 3.495f;
    sensorPhysicalSize[HEIGHT] = 2.626f;
    whiteLevel = 4000;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_DAYLIGHT;
    blackLevelPattern[R] = 1000;
    blackLevelPattern[GR] = 1000;
    blackLevelPattern[GB] = 1000;
    blackLevelPattern[B] = 1000;
    maxAnalogSensitivity = 800;
    orientation = SECURE_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;

    /* Android Statistics Static Metadata */
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[0] = 64;
    sharpnessMapSize[1] = 64;

    horizontalViewAngle[SIZE_RATIO_16_9] = 78.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 78.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 62.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 78.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 75.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 78.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 73.0f;
    verticalViewAngle = 61.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_UNKNOWN;

#ifdef SAMSUNG_TN_FEATURE
    availableIrisSizeListMax   = sizeof(S5K5F1_AVAILABLE_IRIS_SIZE_LIST) / (sizeof(int) * 2);
    availableIrisSizeList      = S5K5F1_AVAILABLE_IRIS_SIZE_LIST;
    availableIrisFormatListMax = sizeof(S5K5F1_AVAILABLE_IRIS_FORMATS_LIST) / sizeof(int);
    availableIrisFormatList    = S5K5F1_AVAILABLE_IRIS_FORMATS_LIST;
#endif

    availableThumbnailCallbackSizeListMax = 0;
    availableThumbnailCallbackSizeList = NULL;
    availableThumbnailCallbackFormatListMax = 0;
    availableThumbnailCallbackFormatList = NULL;
    /* END of Camera HAL 3.2 Static Metadatas */
};

/* based on IMX333/2L2 */
ExynosCameraSensorS5KRPBBase::ExynosCameraSensorS5KRPBBase(__unused int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4656;
    maxSensorH = 3520;
    maxPreviewW = 4656;
    maxPreviewH = 3492;
    maxPictureW = 4656;
    maxPictureH = 3492;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_RPB)                   / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_RPB)              / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_RPB)                   / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_RPB)              / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_RPB)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_RPB)                     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_RPB)   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_RPB)   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed480Max = sizeof(VIDEO_SIZE_LUT_480FPS_HIGH_SPEED_RPB)   / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_RPB)                    / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_RPB)            / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_RPB;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_RPB;
    dualPreviewSizeLut          = PREVIEW_FULL_SIZE_LUT_RPB;
    pictureSizeLut              = PICTURE_SIZE_LUT_RPB;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_RPB;
    videoSizeLut                = VIDEO_SIZE_LUT_RPB;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_RPB;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_RPB;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_RPB;
    videoSizeLutHighSpeed480    = VIDEO_SIZE_LUT_480FPS_HIGH_SPEED_RPB;
    vtcallSizeLut               = VTCALL_SIZE_LUT_RPB;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_RPB;

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(RPB_YUV_LIST)                           / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(RPB_JPEG_LIST)                          / (sizeof(int) * SIZE_OF_RESOLUTION);
    thumbnailListMax            = sizeof(RPB_THUMBNAIL_LIST)                     / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(RPB_HIGH_SPEED_VIDEO_LIST)              / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(RPB_FPS_RANGE_LIST)                     / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(RPB_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)    / (sizeof(int) * 2);

    /* Set supported  size/fps lists */
    yuvList                     = RPB_YUV_LIST;
    jpegList                    = RPB_JPEG_LIST;
    thumbnailList               = RPB_THUMBNAIL_LIST;
    highSpeedVideoList          = RPB_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = RPB_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = RPB_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_BACKWARD_COMPATIBLE | CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                             CAPABILITIES_BURST_CAPTURE | CAPABILITIES_RAW | CAPABILITIES_PRIVATE_REPROCESSING);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.70f;
    fNumber = 1.7f;
    filterDensity = 0.0f;
    focalLength = 4.2f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.66f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 50;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 125000000L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    colorTransformMatrix1 = COLOR_MATRIX1_RPB_3X3;
    colorTransformMatrix2 = COLOR_MATRIX2_RPB_3X3;
    forwardMatrix1 = FORWARD_MATRIX1_RPB_3X3;
    forwardMatrix2 = FORWARD_MATRIX2_RPB_3X3;

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.0f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor2P7SQBase::ExynosCameraSensor2P7SQBase(int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 4608;
    maxSensorH = 3456;
    maxPreviewW = 4608;
    maxPreviewH = 3456;
    maxPictureW = 4608;
    maxPictureH = 3456;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_2P7SQ)                     / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_2P7SQ)            / (sizeof(int) * SIZE_OF_LUT);
    dualPreviewSizeLutMax       = sizeof(PREVIEW_SIZE_LUT_2P7SQ)                     / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_2P7SQ)                     / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_2P7SQ)            / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_2P7SQ)                   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_2P7SQ)                    / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2P7SQ)     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2P7SQ)     / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_2P7SQ)                  / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_2P7SQ)          / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_2P7SQ;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_2P7SQ;
    dualPreviewSizeLut          = PREVIEW_SIZE_LUT_2P7SQ;
    pictureSizeLut              = PICTURE_SIZE_LUT_2P7SQ;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_2P7SQ;
    videoSizeLut                = VIDEO_SIZE_LUT_2P7SQ;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_2P7SQ;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2P7SQ;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2P7SQ;
    vtcallSizeLut               = VTCALL_SIZE_LUT_2P7SQ;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_2P7SQ;

#ifdef SAMSUNG_SSM
    videoSizeLutSSMMax          = sizeof(VIDEO_SIZE_LUT_SSM_2P7SQ) / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutSSM             = VIDEO_SIZE_LUT_SSM_2P7SQ;
#endif

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(SAK2P7SQ_YUV_LIST)                         / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(SAK2P7SQ_JPEG_LIST)                            / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(SAK2P7SQ_HIGH_SPEED_VIDEO_LIST)                / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(SAK2P7SQ_FPS_RANGE_LIST)                   / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(SAK2P7SQ_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)  / (sizeof(int) * 2);

    /* Set supported  size/fps lists */
    yuvList                     = SAK2P7SQ_YUV_LIST;
    jpegList                    = SAK2P7SQ_JPEG_LIST;
    highSpeedVideoList          = SAK2P7SQ_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = SAK2P7SQ_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = SAK2P7SQ_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.5f;
    fNumber = 1.5f;
    filterDensity = 0.0f;
    focalLength = 4.3f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.00f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 40;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    if (sensorId == SENSOR_NAME_IMX333) {
        colorTransformMatrix1 = COLOR_MATRIX1_IMX333_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_IMX333_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_IMX333_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_IMX333_3X3;
    } else {
        colorTransformMatrix1 = COLOR_MATRIX1_2P7SQ_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_2P7SQ_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_2P7SQ_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_2P7SQ_3X3;
    }

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.1f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    horizontalViewAngle[SIZE_RATIO_9_16] = 27.4f;
    horizontalViewAngle[SIZE_RATIO_18P5_9] = 65.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    /* Samsung Vendor Feature */
    availableVideoListMax          = sizeof(SAK2P7SQ_AVAILABLE_VIDEO_LIST)          / (sizeof(int) * 7);
    availableVideoList             = SAK2P7SQ_AVAILABLE_VIDEO_LIST;

    availableHighSpeedVideoListMax = sizeof(SAK2P7SQ_AVAILABLE_HIGH_SPEED_VIDEO_LIST) / (sizeof(int) * 5);
    availableHighSpeedVideoList    = SAK2P7SQ_AVAILABLE_HIGH_SPEED_VIDEO_LIST;

    hiddenPreviewListMax          = sizeof(SAK2P7SQ_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList             = SAK2P7SQ_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax          = sizeof(SAK2P7SQ_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList             = SAK2P7SQ_HIDDEN_PICTURE_SIZE_LIST;

    effectFpsRangesListMax        = sizeof(SAK2P7SQ_EFFECT_FPS_RANGE_LIST) / (sizeof(int) * 2);
    effectFpsRangesList           = SAK2P7SQ_EFFECT_FPS_RANGE_LIST;

#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 1;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = AVAILABLE_VENDOR_OIS_MODES;
    vendorOISModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_OIS_MODES);
#endif

    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);

    vendorWbColorTempRange[MIN] = 2300;
    vendorWbColorTempRange[MAX] = 10000;
    vendorWbLevelRange[MIN] = -4;
    vendorWbLevelRange[MAX] = 4;

#ifdef SUPPORT_MULTI_AF
    vendorMultiAfAvailable = AVAILABLE_VENDOR_MULTI_AF_MODE;
    vendorMultiAfAvailableLength = ARRAY_LENGTH(AVAILABLE_VENDOR_MULTI_AF_MODE);
#endif

    factoryDramTestCount = 16;
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

ExynosCameraSensor2T7SXBase::ExynosCameraSensor2T7SXBase(int sensorId) : ExynosCameraSensorInfoBase()
{
    maxSensorW = 5184;
    maxSensorH = 3880;
    maxPreviewW = 5184;
    maxPreviewH = 3880;
    maxPictureW = 5184;
    maxPictureH = 3880;
    maxThumbnailW = 512;
    maxThumbnailH = 384;

    sensorMarginW = 0;
    sensorMarginH = 0;
    sensorArrayRatio = SIZE_RATIO_4_3;

    bnsSupport = false;
    sizeTableSupport = true;

    previewSizeLutMax           = sizeof(PREVIEW_SIZE_LUT_2T7SX)                     / (sizeof(int) * SIZE_OF_LUT);
    previewFullSizeLutMax       = sizeof(PREVIEW_FULL_SIZE_LUT_2T7SX)            / (sizeof(int) * SIZE_OF_LUT);
    dualPreviewSizeLutMax       = sizeof(DUAL_PREVIEW_SIZE_LUT_2T7SX)                     / (sizeof(int) * SIZE_OF_LUT);
    pictureSizeLutMax           = sizeof(PICTURE_SIZE_LUT_2T7SX)                     / (sizeof(int) * SIZE_OF_LUT);
    pictureFullSizeLutMax       = sizeof(PICTURE_FULL_SIZE_LUT_2T7SX)            / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutMax             = sizeof(VIDEO_SIZE_LUT_2T7SX)                   / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed60Max  = sizeof(VIDEO_SIZE_LUT_2T7SX)                    / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed120Max = sizeof(VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2T7SX)     / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutHighSpeed240Max = sizeof(VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2T7SX)     / (sizeof(int) * SIZE_OF_LUT);
    vtcallSizeLutMax            = sizeof(VTCALL_SIZE_LUT_2T7SX)                  / (sizeof(int) * SIZE_OF_LUT);
    fastAeStableLutMax          = sizeof(FAST_AE_STABLE_SIZE_LUT_2T7SX)          / (sizeof(int) * SIZE_OF_LUT);

    previewSizeLut              = PREVIEW_SIZE_LUT_2T7SX;
    previewFullSizeLut          = PREVIEW_FULL_SIZE_LUT_2T7SX;
    dualPreviewSizeLut          = DUAL_PREVIEW_SIZE_LUT_2T7SX;
    pictureSizeLut              = PICTURE_SIZE_LUT_2T7SX;
    pictureFullSizeLut          = PICTURE_FULL_SIZE_LUT_2T7SX;
    videoSizeLut                = VIDEO_SIZE_LUT_2T7SX;
    videoSizeLutHighSpeed60     = VIDEO_SIZE_LUT_2T7SX;
    videoSizeLutHighSpeed120    = VIDEO_SIZE_LUT_120FPS_HIGH_SPEED_2T7SX;
    videoSizeLutHighSpeed240    = VIDEO_SIZE_LUT_240FPS_HIGH_SPEED_2T7SX;
    vtcallSizeLut               = VTCALL_SIZE_LUT_2T7SX;
    fastAeStableLut             = FAST_AE_STABLE_SIZE_LUT_2T7SX;

#ifdef SAMSUNG_SSM
    videoSizeLutSSMMax          = sizeof(VIDEO_SIZE_LUT_SSM_2T7SX) / (sizeof(int) * SIZE_OF_LUT);
    videoSizeLutSSM             = VIDEO_SIZE_LUT_SSM_2T7SX;
#endif

    /* Set the max of size/fps lists */
    yuvListMax                  = sizeof(SAK2T7SX_YUV_LIST)                         / (sizeof(int) * SIZE_OF_RESOLUTION);
    jpegListMax                 = sizeof(SAK2T7SX_JPEG_LIST)                            / (sizeof(int) * SIZE_OF_RESOLUTION);
    highSpeedVideoListMax       = sizeof(SAK2T7SX_HIGH_SPEED_VIDEO_LIST)                / (sizeof(int) * SIZE_OF_RESOLUTION);
    fpsRangesListMax            = sizeof(SAK2T7SX_FPS_RANGE_LIST)                   / (sizeof(int) * 2);
    highSpeedVideoFPSListMax    = sizeof(SAK2T7SX_HIGH_SPEED_VIDEO_FPS_RANGE_LIST)  / (sizeof(int) * 2);

    /* Set supported  size/fps lists */
    yuvList                     = SAK2T7SX_YUV_LIST;
    jpegList                    = SAK2T7SX_JPEG_LIST;
    highSpeedVideoList          = SAK2T7SX_HIGH_SPEED_VIDEO_LIST;
    fpsRangesList               = SAK2T7SX_FPS_RANGE_LIST;
    highSpeedVideoFPSList       = SAK2T7SX_HIGH_SPEED_VIDEO_FPS_RANGE_LIST;

    /*
     ** Camera HAL 3.2 Static Metadatas
     **
     ** The order of declaration follows the order of
     ** Android Camera HAL3.2 Properties.
     ** Please refer the "/system/media/camera/docs/docs.html"
     */

    lensFacing = ANDROID_LENS_FACING_BACK;
    supportedHwLevel = ANDROID_INFO_SUPPORTED_HARDWARE_LEVEL_FULL;
    /* FULL-Level default capabilities */
    supportedCapabilities = (CAPABILITIES_MANUAL_SENSOR | CAPABILITIES_MANUAL_POST_PROCESSING |
                            CAPABILITIES_BURST_CAPTURE);
    requestKeys = AVAILABLE_REQUEST_KEYS_BASIC;
    resultKeys = AVAILABLE_RESULT_KEYS_BASIC;
    characteristicsKeys = AVAILABLE_CHARACTERISTICS_KEYS_BASIC;
    requestKeysLength = ARRAY_LENGTH(AVAILABLE_REQUEST_KEYS_BASIC);
    resultKeysLength = ARRAY_LENGTH(AVAILABLE_RESULT_KEYS_BASIC);
    characteristicsKeysLength = ARRAY_LENGTH(AVAILABLE_CHARACTERISTICS_KEYS_BASIC);

    /* Android ColorCorrection Static Metadata */
    colorAberrationModes = AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES;
    colorAberrationModesLength = ARRAY_LENGTH(AVAILABLE_COLOR_CORRECTION_ABERRATION_MODES);

    /* Android Control Static Metadata */
    antiBandingModes = AVAILABLE_ANTIBANDING_MODES;
#if defined(USE_SUBDIVIDED_EV)
    exposureCompensationRange[MIN] = -20;
    exposureCompensationRange[MAX] = 20;
    exposureCompensationStep = 0.1f;
#else
    exposureCompensationRange[MIN] = -4;
    exposureCompensationRange[MAX] = 4;
    exposureCompensationStep = 0.5f;
#endif
    effectModes = AVAILABLE_EFFECT_MODES;
    sceneModes = AVAILABLE_SCENE_MODES;
    videoStabilizationModes = AVAILABLE_VIDEO_STABILIZATION_MODES;
    awbModes = AVAILABLE_AWB_MODES;
    controlModes = AVAILABLE_CONTROL_MODES;
    controlModesLength = ARRAY_LENGTH(AVAILABLE_CONTROL_MODES);
    max3aRegions[AE] = 1;
    max3aRegions[AWB] = 1;
    max3aRegions[AF] = 1;
    sceneModeOverrides = SCENE_MODE_OVERRIDES;
    aeLockAvailable = ANDROID_CONTROL_AE_LOCK_AVAILABLE_TRUE;
    awbLockAvailable = ANDROID_CONTROL_AWB_LOCK_AVAILABLE_TRUE;
    antiBandingModesLength = ARRAY_LENGTH(AVAILABLE_ANTIBANDING_MODES);
    effectModesLength = ARRAY_LENGTH(AVAILABLE_EFFECT_MODES);
    sceneModesLength = ARRAY_LENGTH(AVAILABLE_SCENE_MODES);
    videoStabilizationModesLength = ARRAY_LENGTH(AVAILABLE_VIDEO_STABILIZATION_MODES);
    awbModesLength = ARRAY_LENGTH(AVAILABLE_AWB_MODES);
    sceneModeOverridesLength = ARRAY_LENGTH(SCENE_MODE_OVERRIDES);

    /* Android Edge Static Metadata */
    edgeModes = AVAILABLE_EDGE_MODES;
    edgeModesLength = ARRAY_LENGTH(AVAILABLE_EDGE_MODES);

    /* Android Flash Static Metadata */
    flashAvailable = ANDROID_FLASH_INFO_AVAILABLE_TRUE;
    if (flashAvailable == ANDROID_FLASH_INFO_AVAILABLE_TRUE) {
        aeModes = AVAILABLE_AE_MODES;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES);
    } else {
        aeModes = AVAILABLE_AE_MODES_NOFLASH;
        aeModesLength = ARRAY_LENGTH(AVAILABLE_AE_MODES_NOFLASH);
    }

    /* Android Hot Pixel Static Metadata */
    hotPixelModes = AVAILABLE_HOT_PIXEL_MODES;
    hotPixelModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MODES);

    /* Android Lens Static Metadata */
    aperture = 1.5f;
    fNumber = 1.5f;
    filterDensity = 0.0f;
    focalLength = 4.3f;
    focalLengthIn35mmLength = 26;
    hyperFocalDistance = 1.0f / 3.6f;
    minimumFocusDistance = 1.00f / 0.1f;
    if (minimumFocusDistance > 0.0f) {
        afModes = AVAILABLE_AF_MODES;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_CALIBRATED;
    } else {
        afModes = AVAILABLE_AF_MODES_FIXED;
        afModesLength = ARRAY_LENGTH(AVAILABLE_AF_MODES_FIXED);
        focusDistanceCalibration = ANDROID_LENS_INFO_FOCUS_DISTANCE_CALIBRATION_UNCALIBRATED;
    }
    shadingMapSize[WIDTH] = 1;
    shadingMapSize[HEIGHT] = 1;
    opticalAxisAngle[0] = 0.0f;
    opticalAxisAngle[1] = 0.0f;
    lensPosition[X_3D] = 0.0f;
    lensPosition[Y_3D] = 20.0f;
    lensPosition[Z_3D] = -5.0f;
    opticalStabilization = AVAILABLE_OPTICAL_STABILIZATION_BACK;
    opticalStabilizationLength = ARRAY_LENGTH(AVAILABLE_OPTICAL_STABILIZATION_BACK);

    /* Android Noise Reduction Static Metadata */
    noiseReductionModes = AVAILABLE_NOISE_REDUCTION_MODES;
    noiseReductionModesLength = ARRAY_LENGTH(AVAILABLE_NOISE_REDUCTION_MODES);

    /* Android Request Static Metadata */
    maxNumOutputStreams[RAW] = 1;
    maxNumOutputStreams[PROCESSED] = 3;
    maxNumOutputStreams[PROCESSED_STALL] = 1;
    maxNumInputStreams = 1;
    maxPipelineDepth = 8;
    partialResultCount = 2;

    /* Android Scaler Static Metadata */
    zoomSupport = true;
    maxZoomRatio = MAX_ZOOM_RATIO;
    maxZoomRatioVendor = MAX_ZOOM_RATIO_VENDOR;
    stallDurations = AVAILABLE_STALL_DURATIONS;
    stallDurationsLength = ARRAY_LENGTH(AVAILABLE_STALL_DURATIONS);
    croppingType = ANDROID_SCALER_CROPPING_TYPE_FREEFORM;

    /* Android Sensor Static Metadata */
    sensitivityRange[MIN] = 40;
    sensitivityRange[MAX] = 1250;
    colorFilterArrangement = ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_GRBG;
    exposureTimeRange[MIN] = 60000L;
    exposureTimeRange[MAX] = 100000000L;
    maxFrameDuration = 142857142L;
    sensorPhysicalSize[WIDTH] = 5.645f;
    sensorPhysicalSize[HEIGHT] = 4.234f;
    whiteLevel = 1023;
    timestampSource = ANDROID_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME;
    referenceIlluminant1 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_D65;
    referenceIlluminant2 = ANDROID_SENSOR_REFERENCE_ILLUMINANT1_STANDARD_A;
    blackLevelPattern[R] = 0;
    blackLevelPattern[GR] = 0;
    blackLevelPattern[GB] = 0;
    blackLevelPattern[B] = 0;
    maxAnalogSensitivity = 640;
    orientation = BACK_ROTATION;
    profileHueSatMapDimensions[HUE] = 1;
    profileHueSatMapDimensions[SATURATION] = 2;
    profileHueSatMapDimensions[VALUE] = 1;
    testPatternModes = AVAILABLE_TEST_PATTERN_MODES;
    testPatternModesLength = ARRAY_LENGTH(AVAILABLE_TEST_PATTERN_MODES);
    if (sensorId == SENSOR_NAME_IMX333) {
        colorTransformMatrix1 = COLOR_MATRIX1_IMX333_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_IMX333_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_IMX333_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_IMX333_3X3;
    } else {
        colorTransformMatrix1 = COLOR_MATRIX1_2T7SX_3X3;
        colorTransformMatrix2 = COLOR_MATRIX2_2T7SX_3X3;
        forwardMatrix1 = FORWARD_MATRIX1_2T7SX_3X3;
        forwardMatrix2 = FORWARD_MATRIX2_2T7SX_3X3;
    }

    /* Android Statistics Static Metadata */
    faceDetectModes = AVAILABLE_FACE_DETECT_MODES;
    faceDetectModesLength = ARRAY_LENGTH(AVAILABLE_FACE_DETECT_MODES);
    histogramBucketCount = 64;
    maxNumDetectedFaces = 16;
    maxHistogramCount = 1000;
    maxSharpnessMapValue = 1000;
    sharpnessMapSize[WIDTH] = 64;
    sharpnessMapSize[HEIGHT] = 64;
    hotPixelMapModes = AVAILABLE_HOT_PIXEL_MAP_MODES;
    hotPixelMapModesLength = ARRAY_LENGTH(AVAILABLE_HOT_PIXEL_MAP_MODES);
    lensShadingMapModes = AVAILABLE_LENS_SHADING_MAP_MODES;
    lensShadingMapModesLength = ARRAY_LENGTH(AVAILABLE_LENS_SHADING_MAP_MODES);
    shadingAvailableModes = SHADING_AVAILABLE_MODES;
    shadingAvailableModesLength = ARRAY_LENGTH(SHADING_AVAILABLE_MODES);

    /* Android Tone Map Static Metadata */
    tonemapCurvePoints = 128;
    toneMapModes = AVAILABLE_TONE_MAP_MODES;
    toneMapModesLength = ARRAY_LENGTH(AVAILABLE_TONE_MAP_MODES);

    horizontalViewAngle[SIZE_RATIO_16_9] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_4_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_1_1] = 51.1f;
    horizontalViewAngle[SIZE_RATIO_3_2] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_5_4] = 61.0f;
    horizontalViewAngle[SIZE_RATIO_5_3] = 65.0f;
    horizontalViewAngle[SIZE_RATIO_11_9] = 60.0f;
    horizontalViewAngle[SIZE_RATIO_9_16] = 27.4f;
    horizontalViewAngle[SIZE_RATIO_18P5_9] = 65.0f;
    verticalViewAngle = 41.0f;

    /* Android Sync Static Metadata */
    maxLatency = ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL; //0

#ifdef SAMSUNG_TN_FEATURE
    /* Samsung Vendor Feature */
    availableVideoListMax          = sizeof(SAK2T7SX_AVAILABLE_VIDEO_LIST)          / (sizeof(int) * 7);
    availableVideoList             = SAK2T7SX_AVAILABLE_VIDEO_LIST;

    availableHighSpeedVideoListMax = sizeof(SAK2T7SX_AVAILABLE_HIGH_SPEED_VIDEO_LIST) / (sizeof(int) * 5);
    availableHighSpeedVideoList    = SAK2T7SX_AVAILABLE_HIGH_SPEED_VIDEO_LIST;

    hiddenPreviewListMax          = sizeof(SAK2T7SX_HIDDEN_PREVIEW_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPreviewList             = SAK2T7SX_HIDDEN_PREVIEW_SIZE_LIST;

    hiddenPictureListMax          = sizeof(SAK2T7SX_HIDDEN_PICTURE_SIZE_LIST)   / (sizeof(int) * SIZE_OF_RESOLUTION);
    hiddenPictureList             = SAK2T7SX_HIDDEN_PICTURE_SIZE_LIST;

    effectFpsRangesListMax        = sizeof(SAK2T7SX_EFFECT_FPS_RANGE_LIST) / (sizeof(int) * 2);
    effectFpsRangesList           = SAK2T7SX_EFFECT_FPS_RANGE_LIST;

#ifdef SAMSUNG_CONTROL_METERING
    vendorMeteringModes = AVAILABLE_VENDOR_METERING_MODES;
    vendorMeteringModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_METERING_MODES);
#endif
#ifdef SAMSUNG_RTHDR
    vendorHdrRange[MIN] = 0;
    vendorHdrRange[MAX] = 1;
    vendorHdrModes = AVAILABLE_VENDOR_HDR_MODES;
    vendorHdrModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_HDR_MODES);
#endif
#ifdef SAMSUNG_PAF
    vendorPafAvailable = 1;
#endif
#ifdef SAMSUNG_OIS
    vendorOISModes = AVAILABLE_VENDOR_OIS_MODES;
    vendorOISModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_OIS_MODES);
#endif

    vendorAwbModes = AVAILABLE_VENDOR_AWB_MODES;
    vendorAwbModesLength = ARRAY_LENGTH(AVAILABLE_VENDOR_AWB_MODES);

    vendorWbColorTempRange[MIN] = 2300;
    vendorWbColorTempRange[MAX] = 10000;
    vendorWbLevelRange[MIN] = -4;
    vendorWbLevelRange[MAX] = 4;

#ifdef SUPPORT_MULTI_AF
    vendorMultiAfAvailable = AVAILABLE_VENDOR_MULTI_AF_MODE;
    vendorMultiAfAvailableLength = ARRAY_LENGTH(AVAILABLE_VENDOR_MULTI_AF_MODE);
#endif

    factoryDramTestCount = 16;
#endif
    /* END of Camera HAL 3.2 Static Metadatas */
};

}; /* namespace android */
