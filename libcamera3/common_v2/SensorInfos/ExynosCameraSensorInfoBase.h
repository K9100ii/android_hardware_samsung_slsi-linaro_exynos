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

#ifndef EXYNOS_CAMERA_SENSOR_INFO_BASE_H
#define EXYNOS_CAMERA_SENSOR_INFO_BASE_H

#include <videodev2.h>
#include <videodev2_exynos_camera.h>
#include <CameraMetadata.h>
#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraSizeTable.h"
#include "ExynosCameraAvailabilityTable.h"
#include "fimc-is-metadata.h"

#define UNIQUE_ID_BUF_SIZE          (32)

#if defined(SUPPORT_X10_ZOOM)
#define MAX_ZOOM_RATIO (8000)
#define MAX_ZOOM_RATIO_FRONT (4000)
#define MAX_ZOOM_RATIO_VENDOR (10000)
#elif defined(SUPPORT_X8_ZOOM)
#define MAX_ZOOM_RATIO (8000)
#define MAX_ZOOM_RATIO_FRONT (4000)
#define MAX_ZOOM_RATIO_VENDOR (8000)
#else
#define MAX_ZOOM_RATIO (4000)
#define MAX_ZOOM_RATIO_FRONT (4000)
#define MAX_ZOOM_RATIO_VENDOR (4000)
#endif

#define ARRAY_LENGTH(x)          (sizeof(x)/sizeof(x[0]))
#define COMMON_DENOMINATOR       (100)
#define EFFECTMODE_META_2_HAL(x) (1 << (x -1))

#ifdef SAMSUNG_OIS
#define OIS_EXIF_SIZE         50
#define OIS_EXIF_TAG         "ssois"
#endif

#ifdef SAMSUNG_BD
#define BD_EXIF_SIZE    70
#define BD_EXIF_TAG "ssbd"
#endif

#if defined(SAMSUNG_LLS_DEBLUR) || defined(SAMSUNG_DUAL_ZOOM_CAPTURE)
#define LLS_EXIF_SIZE    310
#define LLS_EXIF_TAG "sslls"
#define FUSION_EXIF_TAG "ssfusion"
#endif

#ifdef SAMSUNG_MFHDR_CAPTURE
#define MFHDR_EXIF_TAG "ssmfhdr"
#endif

#ifdef SAMSUNG_LLHDR_CAPTURE
#define LLHDR_EXIF_TAG "ssllhdr"
#endif

#ifdef SAMSUNG_LENS_DC
#define LDC_EXIF_SIZE    12
#define LDC_EXIF_TAG "ssldc"
#endif

#ifdef SAMSUNG_STR_CAPTURE
#define STR_EXIF_SIZE    120
#define STR_EXIF_TAG "ssstr"
#endif

#ifdef SAMSUNG_MTF
#define MTF_EXIF_TAG "ssmtf"
#define MTF_EXIF_SIZE    61
#endif

#define SENSOR_ID_EXIF_SIZE         42
#define SENSOR_ID_EXIF_UNIT_SIZE    16
#define SENSOR_ID_EXIF_TAG         "ssuniqueid"

#ifdef SAMSUNG_HIFI_VIDEO
#define HIFI_VIDEO_EXIF_SIZE    314
#define HIFI_VIDEO_EXIF_TAG "sshifivideo"
#endif

namespace android {

/* Define Hidden Service IDs */ 
#define SERVICE_ID_DUAL_REAR_ZOOM               20
#define SERVICE_ID_DUAL_REAR_PORTRAIT           21
#define SERVICE_ID_DUAL_FRONT_PORTRAIT          22
#define SERVICE_ID_REAR_2ND                     50
#define SERVICE_ID_FRONT_2ND                    51
#define SERVICE_ID_IRIS                         90

#define xstr(x)         #x
#define toStr(x)        xstr(x)
#define toSize(x,y)     (sizeof(x)/sizeof(y))

struct HAL_CameraInfo_t {
    int cameraId;
    int facing_info;
    int orientation;
    int resource_cost;
    char **conflicting_devices;
    size_t conflicting_devices_length;
};

enum max_3a_region {
    AE,
    AWB,
    AF,
    REGIONS_INDEX_MAX,
};
enum size_direction {
    WIDTH,
    HEIGHT,
    SIZE_DIRECTION_MAX,
};
enum coordinate_3d {
    X_3D,
    Y_3D,
    Z_3D,
    COORDINATE_3D_MAX,
};
enum output_streams_type {
    RAW,
    PROCESSED,
    PROCESSED_STALL,
    OUTPUT_STREAM_TYPE_MAX,
};
enum range_type {
    MIN,
    MAX,
    RANGE_TYPE_MAX,
};
enum bayer_cfa_mosaic_channel {
    R,
    GR,
    GB,
    B,
    BAYER_CFA_MOSAIC_CHANNEL_MAX,
};
enum hue_sat_value_index {
    HUE,
    SATURATION,
    VALUE,
    HUE_SAT_VALUE_INDEX_MAX,
};
enum sensor_margin_base_index {
    LEFT_BASE,
    TOP_BASE,
    WIDTH_BASE,
    HEIGHT_BASE,
    BASE_MAX,
};
enum default_fps_index {
    DEFAULT_FPS_STILL,
    DEFAULT_FPS_VIDEO,
    DEFAULT_FPS_EFFECT_STILL,
    DEFAULT_FPS_EFFECT_VIDEO,
    DEFAULT_FPS_MAX,
};

enum CAMERA_SERVICE_ID { // External ID From Camera Service
    /* 0 ~ 19 : Open ID Section */
    /* should be defined in camera config header */
    /* #define CAMERA_OPEN_ID_REAR_0       0 */
    /* #define CAMERA_OPEN_ID_FRONT_1      1 */
    CAMERA_SERVICE_ID_OPEN_CAMERA_MAX = 20,
    /* 20 ~ 49 : Dual Hidden ID Section */
    CAMERA_SERVICE_ID_DUAL_REAR_ZOOM    = CAMERA_SERVICE_ID_OPEN_CAMERA_MAX,
    CAMERA_SERVICE_ID_DUAL_REAR_PORTRAIT  = 21,
    CAMERA_SERVICE_ID_DUAL_FRONT_PORTRAIT  = 22,
    /* 50 ~  : Single Hidden ID Section */
    CAMERA_SERVICE_ID_REAR_2ND             = 50,
    CAMERA_SERVICE_ID_FRONT_2ND             = 51,
    /* 90 ~ :  Secure Hidden ID Section */
    CAMERA_SERVICE_ID_IRIS              = 90,
    /* ETC. */
    CAMERA_SERVICE_ID_MAX
};

enum CAMERA_ID {  // Internal ID
    CAMERA_ID_BACK              = 0,
    CAMERA_ID_FRONT             = 1,
    CAMERA_ID_BACK_0  = CAMERA_ID_BACK,
    CAMERA_ID_FRONT_0 = CAMERA_ID_FRONT,
    CAMERA_ID_BACK_1,
    CAMERA_ID_FRONT_1,
    CAMERA_ID_SECURE,
    CAMERA_ID_MAX,
};
enum SCENARIO {
    SCENARIO_NORMAL              = 0,
    SCENARIO_SECURE              = 1,
    SCENARIO_DUAL_REAR_ZOOM      = 2,
    SCENARIO_DUAL_REAR_PORTRAIT	 = 3,
    SCENARIO_DUAL_FRONT_PORTRAIT = 4,
};
enum MODE {
    MODE_PREVIEW = 0,
    MODE_PICTURE,
    MODE_VIDEO,
    MODE_THUMBNAIL,
    MODE_DNG_PICTURE,
};
enum {
    FOCUS_MODE_AUTO                     = (1 << 0),
    FOCUS_MODE_INFINITY                 = (1 << 1),
    FOCUS_MODE_MACRO                    = (1 << 2),
    FOCUS_MODE_FIXED                    = (1 << 3),
    FOCUS_MODE_EDOF                     = (1 << 4),
    FOCUS_MODE_CONTINUOUS_VIDEO         = (1 << 5),
    FOCUS_MODE_CONTINUOUS_PICTURE       = (1 << 6),
    FOCUS_MODE_TOUCH                    = (1 << 7),
    FOCUS_MODE_CONTINUOUS_PICTURE_MACRO = (1 << 8),
#ifdef SAMSUNG_OT
    FOCUS_MODE_OBJECT_TRACKING_PICTURE = (1 << 9),
    FOCUS_MODE_OBJECT_TRACKING_VIDEO = (1 << 10),
#endif
#ifdef SAMSUNG_MANUAL_FOCUS
    FOCUS_MODE_MANUAL = (1 << 11),
#endif
#ifdef SAMSUNG_FIXED_FACE_FOCUS
    FOCUS_MODE_FIXED_FACE = (1 << 12),
#endif
};
enum {
    FLASH_MODE_OFF     = (1 << 0),
    FLASH_MODE_AUTO    = (1 << 1),
    FLASH_MODE_ON      = (1 << 2),
    FLASH_MODE_RED_EYE = (1 << 3),
    FLASH_MODE_TORCH   = (1 << 4),
};

enum SERIES_SHOT_MODE {
    SERIES_SHOT_MODE_NONE              = 0,
    SERIES_SHOT_MODE_LLS               = 1,
    SERIES_SHOT_MODE_SIS               = 2,
    SERIES_SHOT_MODE_BURST             = 3,
    SERIES_SHOT_MODE_ERASER            = 4,
    SERIES_SHOT_MODE_BEST_FACE         = 5,
    SERIES_SHOT_MODE_BEST_PHOTO        = 6,
    SERIES_SHOT_MODE_MAGIC             = 7,
    SERIES_SHOT_MODE_SELFIE_ALARM      = 8,
#ifdef ONE_SECOND_BURST_CAPTURE
    SERIES_SHOT_MODE_ONE_SECOND_BURST  = 9,
#endif
    SERIES_SHOT_MODE_MAX,
};
enum MULTI_CAPTURE_MODE {
    MULTI_CAPTURE_MODE_NONE  = 0,
    MULTI_CAPTURE_MODE_BURST = 1,
    MULTI_CAPTURE_MODE_AGIF  = 2,
    MULTI_CAPTURE_MODE_MAX,
};

#ifdef SAMSUNG_LLS_DEBLUR
enum MULTI_SHOT_MODE {
    MULTI_SHOT_MODE_NONE              = 0,
    MULTI_SHOT_MODE_MULTI1            = 1,
    MULTI_SHOT_MODE_MULTI2            = 2,
    MULTI_SHOT_MODE_MULTI3            = 3,
    MULTI_SHOT_MODE_MULTI4            = 4,
    MULTI_SHOT_MODE_FLASHED_LLS       = 5,
    MULTI_SHOT_MODE_SR                = 6,
    MULTI_SHOT_MODE_MF_HDR            = 7,
    MULTI_SHOT_MODE_LL_HDR            = 8,
    MULTI_SHOT_MODE_MAX,
};
#endif

enum TRANSIENT_ACTION {
    TRANSIENT_ACTION_NONE               = 0,
    TRANSIENT_ACTION_ZOOMING            = 1,
    TRANSIENT_ACTION_MANUAL_FOCUSING    = 2,
    TRANSIENT_ACTION_MAX,
};

#ifdef SENSOR_NAME_GET_FROM_FILE
int getSensorIdFromFile(int camId);
#endif
#ifdef SENSOR_FW_GET_FROM_FILE
const char *getSensorFWFromFile(struct ExynosCameraSensorInfoBase *info, int camId);
#endif
#ifdef SAMSUNG_OIS
char *getOisEXIFFromFile(struct ExynosCameraSensorInfoBase *info, int mode);

struct ois_exif_data {
    char ois_exif[OIS_EXIF_SIZE];
};
#endif
#ifdef SAMSUNG_BD
struct bd_exif_data {
    char bd_exif[BD_EXIF_SIZE];
};
#endif
#ifdef SAMSUNG_LLS_DEBLUR
struct lls_exif_data {
    char lls_exif[LLS_EXIF_SIZE];
};
#endif
#ifdef SAMSUNG_LENS_DC
struct ldc_exif_data {
    char ldc_exif[LDC_EXIF_SIZE];
};
#endif
#ifdef SAMSUNG_STR_CAPTURE
struct str_exif_data {
    char str_exif[STR_EXIF_SIZE];
};
#endif
#ifdef SAMSUNG_MTF
struct mtf_exif_data {
    char mtf_exif[MTF_EXIF_SIZE];
};
#endif
struct sensor_id_exif_data {
    char sensor_id_exif[SENSOR_ID_EXIF_SIZE];
};
#ifdef SAMSUNG_HIFI_VIDEO
struct hifi_video_exif_data {
    char hifi_video_exif[HIFI_VIDEO_EXIF_SIZE];
};
#endif

#ifdef SAMSUNG_UTC_TS
#define UTC_TS_SIZE 24 /* Tag (2bytes) + Length (2bytes) + Time (20bytes) */
struct utc_ts {
    char utc_ts_data[UTC_TS_SIZE];
};
#endif

/* Helpper functions */
int getSensorId(int camId);
#ifdef USE_DUAL_CAMERA
void getDualCameraId(int *cameraId_0, int *cameraId_1);
#endif
#ifdef SENSOR_NAME_GET_FROM_FILE
int getSensorIdFromFile(int camId);
#endif
#ifdef SENSOR_FW_GET_FROM_FILE
const char *getSensorFWFromFile(struct ExynosCameraSensorInfoBase *info, int camId);
#endif

struct exynos_camera_info {
public:
    int     previewW;
    int     previewH;
    int     previewFormat;
    int     previewStride;

    int     pictureW;
    int     pictureH;
    int     pictureFormat;
    int     hwPictureFormat;
    camera_pixel_size hwPicturePixelSize;

    int     videoW;
    int     videoH;

    int     yuvWidth[6];
    int     yuvHeight[6];
    int     yuvFormat[6];

    int     hwYuvWidth[6];
    int     hwYuvHeight[6];

    /* This size for internal */
    int     hwSensorW;
    int     hwSensorH;
    int     yuvSizeRatioId;
    int     yuvSizeLutIndex;
    int     hwPictureW;
    int     hwPictureH;
    int     pictureSizeRatioId;
    int     pictureSizeLutIndex;
    int     hwDisW;
    int     hwDisH;
    int     hwPreviewFormat;

    int     hwBayerCropW;
    int     hwBayerCropH;
    int     hwBayerCropX;
    int     hwBayerCropY;

    int     bnsW;
    int     bnsH;

    int     jpegQuality;
    int     thumbnailW;
    int     thumbnailH;
    int     thumbnailQuality;

    uint32_t    bnsScaleRatio;
    uint32_t    binningScaleRatio;

    bool    is3dnrMode;
    bool    isDrcMode;
    bool    isOdcMode;

    int     flipHorizontal;
    int     flipVertical;

    int     operationMode;
    bool    visionMode;
    int     visionModeFps;
    int     visionModeAeTarget;

    bool    recordingHint;
    bool    ysumRecordingMode;
    bool    pipMode;
#ifdef USE_DUAL_CAMERA
    bool    dualMode;
#endif
    bool    pipRecordingHint;
    bool    effectHint;
    bool    effectRecordingHint;

    bool    highSpeedRecording;
    bool    videoStabilization;
    bool    swVdisMode;
    bool    hyperlapseMode;
    int     hyperlapseSpeed;
    int     shotMode;
    int     vtMode;
    bool    hdrMode;

    char    imageUniqueId[UNIQUE_ID_BUF_SIZE];
    bool    samsungCamera;
    bool    isFactoryApp;
    int     recordingFps;

    int     seriesShotMode;
    int     seriesShotCount;
    int     multiCaptureMode;

    int     deviceOrientation;
};

struct ExynosCameraSensorInfoBase {
public:
#ifdef SENSOR_FW_GET_FROM_FILE
    char	sensor_fw[25];
#endif
#ifdef SAMSUNG_OIS
    struct ois_exif_data     ois_exif_info;
#endif
#ifdef SAMSUNG_BD
    struct bd_exif_data     bd_exif_info;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    struct lls_exif_data     lls_exif_info;
#endif
#ifdef SAMSUNG_LENS_DC
    struct ldc_exif_data     ldc_exif_info;
#endif
#ifdef SAMSUNG_STR_CAPTURE
    struct str_exif_data     str_exif_info;
#endif
#ifdef SAMSUNG_MTF
	struct mtf_exif_data	mtf_exif_info;
#endif
    struct sensor_id_exif_data sensor_id_exif_info;
#ifdef SAMSUNG_HIFI_VIDEO
    struct hifi_video_exif_data hifi_video_exif_info;
#endif

    int     maxPreviewW;
    int     maxPreviewH;
    int     maxPictureW;
    int     maxPictureH;
    int     maxSensorW;
    int     maxSensorH;
    int     sensorMarginW;
    int     sensorMarginH;
    int     sensorMarginBase[BASE_MAX];
    int     sensorArrayRatio;

    int     maxThumbnailW;
    int     maxThumbnailH;

    float   horizontalViewAngle[SIZE_RATIO_END];
    float   verticalViewAngle;

    int     minFps;
    int     maxFps;
    int     defaultFpsMin[DEFAULT_FPS_MAX];
    int     defaultFpsMAX[DEFAULT_FPS_MAX];

    /*
    ** Camera HAL 3.2 Static Metadatas
    **
    ** The order of declaration follows the order of
    ** Android Camera HAL3.2 Properties.
    ** Please refer the "/system/media/camera/docs/docs.html"
    */
    /* Android ColorCorrection Static Metadata */
    uint8_t    *colorAberrationModes;
    size_t     colorAberrationModesLength;

    /* Android Control Static Metadata */
    uint8_t    *antiBandingModes;
    uint8_t    *aeModes;
    int32_t    exposureCompensationRange[RANGE_TYPE_MAX];
    float      exposureCompensationStep;
    uint8_t    *afModes;
    uint8_t    *effectModes;
    uint8_t    *sceneModes;
    uint8_t    *videoStabilizationModes;
    uint8_t    *awbModes;
    int32_t    *vendorAwbModes;
    int32_t    vendorWbColorTempRange[RANGE_TYPE_MAX];
    int32_t    vendorWbColorTemp;
    int32_t    vendorWbLevelRange[RANGE_TYPE_MAX];
    int32_t    max3aRegions[REGIONS_INDEX_MAX];
    uint8_t    *controlModes;
    size_t     controlModesLength;
    uint8_t    *sceneModeOverrides;
    uint8_t    aeLockAvailable;
    uint8_t    awbLockAvailable;
    size_t     antiBandingModesLength;
    size_t     aeModesLength;
    size_t     afModesLength;
    size_t     effectModesLength;
    size_t     sceneModesLength;
    size_t     videoStabilizationModesLength;
    size_t     awbModesLength;
    size_t     vendorAwbModesLength;
    size_t     sceneModeOverridesLength;
    int32_t    postRawSensitivityBoost[RANGE_TYPE_MAX];

    /* Android Edge Static Metadata */
    uint8_t    *edgeModes;
    size_t     edgeModesLength;

    /* Android Flash Static Metadata */
    uint8_t    flashAvailable;
    int64_t    chargeDuration;
    uint8_t    colorTemperature;
    uint8_t    maxEnergy;

    /* Android Hot Pixel Static Metadata */
    uint8_t   *hotPixelModes;
    size_t    hotPixelModesLength;

    /* Android Lens Static Metadata */
    float     aperture;
    float     fNumber;
    float     filterDensity;
    float     focalLength;
    int       focalLengthIn35mmLength;
    uint8_t   *opticalStabilization;
    float     hyperFocalDistance;
    float     minimumFocusDistance;
    int32_t   shadingMapSize[SIZE_DIRECTION_MAX];
    uint8_t   focusDistanceCalibration;
    uint8_t   lensFacing;
    float     opticalAxisAngle[2];
    float     lensPosition[COORDINATE_3D_MAX];
    size_t    opticalStabilizationLength;

    /* Android Noise Reduction Static Metadata */
    uint8_t   *noiseReductionModes;
    size_t    noiseReductionModesLength;

    /* Android Request Static Metadata */
    int32_t   maxNumOutputStreams[OUTPUT_STREAM_TYPE_MAX];
    int32_t   maxNumInputStreams;
    uint8_t   maxPipelineDepth;
    int32_t   partialResultCount;
    int32_t   *requestKeys;
    int32_t   *resultKeys;
    int32_t   *characteristicsKeys;
    int32_t   *sessionKeys;
    size_t    requestKeysLength;
    size_t    resultKeysLength;
    size_t    characteristicsKeysLength;
    size_t    sessionKeysLength;

    /* Android Scaler Static Metadata */
    bool      zoomSupport;
    int       maxZoomRatio;
    int       maxZoomRatioVendor;
    int64_t   *stallDurations;
    uint8_t   croppingType;
    size_t    stallDurationsLength;

    /* Android Sensor Static Metadata */
    int32_t   sensitivityRange[RANGE_TYPE_MAX];
    uint8_t   colorFilterArrangement;
    int64_t   exposureTimeRange[RANGE_TYPE_MAX];
    int64_t   maxFrameDuration;
    float     sensorPhysicalSize[SIZE_DIRECTION_MAX];
    int32_t   whiteLevel;
    uint8_t   timestampSource;
    uint8_t   referenceIlluminant1;
    uint8_t   referenceIlluminant2;
    int32_t   blackLevelPattern[BAYER_CFA_MOSAIC_CHANNEL_MAX];
    int32_t   maxAnalogSensitivity;
    int32_t   orientation;
    int32_t   profileHueSatMapDimensions[HUE_SAT_VALUE_INDEX_MAX];
    int32_t   *testPatternModes;
    size_t    testPatternModesLength;
    camera_metadata_rational *colorTransformMatrix1;
    camera_metadata_rational *colorTransformMatrix2;
    camera_metadata_rational *forwardMatrix1;
    camera_metadata_rational *forwardMatrix2;
    camera_metadata_rational *calibration1;
    camera_metadata_rational *calibration2;
    float   masterRGain;
    float   masterBGain;

    int32_t   gain;
    int64_t   exposureTime;
    int32_t   ledCurrent;
    int64_t   ledPulseDelay;
    int64_t   ledPulseWidth;
    int32_t   ledMaxTime;

    int32_t   gainRange[RANGE_TYPE_MAX];
    int32_t   ledCurrentRange[RANGE_TYPE_MAX];
    int64_t   ledPulseDelayRange[RANGE_TYPE_MAX];
    int64_t   ledPulseWidthRange[RANGE_TYPE_MAX];
    int32_t   ledMaxTimeRange[RANGE_TYPE_MAX];

    /* Android Statistics Static Metadata */
    uint8_t   *faceDetectModes;
    int32_t   histogramBucketCount;
    int32_t   maxNumDetectedFaces;
    int32_t   maxHistogramCount;
    int32_t   maxSharpnessMapValue;
    int32_t   sharpnessMapSize[SIZE_DIRECTION_MAX];
    uint8_t   *hotPixelMapModes;
    uint8_t   *lensShadingMapModes;
    size_t    lensShadingMapModesLength;
    uint8_t   *shadingAvailableModes;
    size_t    shadingAvailableModesLength;
    size_t    faceDetectModesLength;
    size_t    hotPixelMapModesLength;

    /* Android Tone Map Static Metadata */
    int32_t   tonemapCurvePoints;
    uint8_t   *toneMapModes;
    size_t    toneMapModesLength;

    /* Android LED Static Metadata */
    uint8_t   *leds;
    size_t    ledsLength;

    /* Android Reprocess Static Metadata */
    int32_t 	maxCaptureStall;

    /* Samsung Vendor Feature */
#ifdef SAMSUNG_CONTROL_METERING
    int32_t   *vendorMeteringModes;
    size_t    vendorMeteringModesLength;
#endif
#ifdef SAMSUNG_RTHDR
    int32_t   vendorHdrRange[RANGE_TYPE_MAX];
    int32_t   *vendorHdrModes;
    size_t    vendorHdrModesLength;
#endif
#ifdef SAMSUNG_PAF
    uint8_t   vendorPafAvailable;
#endif
#ifdef SAMSUNG_OIS
    int32_t   *vendorOISModes;
    size_t    vendorOISModesLength;
#endif
    int32_t   *vendorFlipModes;
    size_t    vendorFlipModesLength;

#ifdef SUPPORT_MULTI_AF
    uint8_t   *vendorMultiAfAvailable;
    size_t    vendorMultiAfAvailableLength;
#endif
    float   *availableApertureValues;
    size_t  availableApertureValuesLength;

    int32_t *availableBurstShotFps;
    size_t  availableBurstShotFpsLength;

    /* Android Info Static Metadata */
    uint8_t   supportedHwLevel;
    uint64_t  supportedCapabilities;

    /* Android Sync Static Metadata */
    int32_t   maxLatency;
    /* END of Camera HAL 3.2 Static Metadatas */

    bool   bnsSupport;
    bool   flite3aaOtfSupport;

    /* vendor specifics available */
    bool   sceneHDRSupport;
    bool   screenFlashAvailable;
    bool   objectTrackingAvailable;
    bool   fixedFaceFocusAvailable;
    int    factoryDramTestCount;

    /* The number of YUV(preview, video)/JPEG(picture) sizes in each list */
    int    yuvListMax;
    int    jpegListMax;
    int    thumbnailListMax;
    int    highSpeedVideoListMax;
    int    availableVideoListMax;
    int    availableHighSpeedVideoListMax;
    int    fpsRangesListMax;
    int    highSpeedVideoFPSListMax;

    /* Supported YUV(preview, video)/JPEG(picture) Lists */
    int    (*yuvList)[SIZE_OF_RESOLUTION];
    int    (*jpegList)[SIZE_OF_RESOLUTION];
    int    (*thumbnailList)[SIZE_OF_RESOLUTION];
    int    (*highSpeedVideoList)[SIZE_OF_RESOLUTION];
    int    (*availableVideoList)[7];
    int    (*availableHighSpeedVideoList)[5];
    int    (*fpsRangesList)[2];
    int    (*highSpeedVideoFPSList)[2];

    int    previewSizeLutMax;
    int    pictureSizeLutMax;
    int    videoSizeLutMax;
    int    dualPreviewSizeLutMax;
    int    vtcallSizeLutMax;
    int    videoSizeLutHighSpeed60Max;
    int    videoSizeLutHighSpeed120Max;
    int    videoSizeLutHighSpeed240Max;
    int    videoSizeLutHighSpeed480Max;
    int    fastAeStableLutMax;
    int    previewFullSizeLutMax;
    int    pictureFullSizeLutMax;
    int    depthMapSizeLutMax;

    int    (*previewSizeLut)[SIZE_OF_LUT];
    int    (*pictureSizeLut)[SIZE_OF_LUT];
    int    (*videoSizeLut)[SIZE_OF_LUT];
    int    (*dualPreviewSizeLut)[SIZE_OF_LUT];
    int    (*videoSizeLutHighSpeed60)[SIZE_OF_LUT];
    int    (*videoSizeLutHighSpeed120)[SIZE_OF_LUT];
    int    (*videoSizeLutHighSpeed240)[SIZE_OF_LUT];
    int    (*videoSizeLutHighSpeed480)[SIZE_OF_LUT];
    int    (*vtcallSizeLut)[SIZE_OF_LUT];
    int    (*fastAeStableLut)[SIZE_OF_LUT];
    int    (*previewFullSizeLut)[SIZE_OF_LUT];
    int    (*pictureFullSizeLut)[SIZE_OF_LUT];
    int    (*depthMapSizeLut)[3];
    bool   sizeTableSupport;

#ifdef SAMSUNG_SSM
    int    videoSizeLutSSMMax;
    int    (*videoSizeLutSSM)[SIZE_OF_LUT];
#endif

    int    hiddenPreviewListMax;
    int    (*hiddenPreviewList)[SIZE_OF_RESOLUTION];

    int    hiddenPictureListMax;
    int    (*hiddenPictureList)[SIZE_OF_RESOLUTION];

    int    hiddenThumbnailListMax;
    int    (*hiddenThumbnailList)[2];

    int    effectFpsRangesListMax;
    int    (*effectFpsRangesList)[2];

#ifdef SUPPORT_DEPTH_MAP
    int    availableDepthSizeListMax;
    int    (*availableDepthSizeList)[2];
    int    availableDepthFormatListMax;
    int    *availableDepthFormatList;
#endif

    int    availableThumbnailCallbackSizeListMax;
    int    (*availableThumbnailCallbackSizeList)[2];
    int    availableThumbnailCallbackFormatListMax;
    int    *availableThumbnailCallbackFormatList;

    int    availableIrisSizeListMax;
    int    (*availableIrisSizeList)[2];
    int    availableIrisFormatListMax;
    int    *availableIrisFormatList;

    int     availableBasicFeaturesListLength;
    int     *availableBasicFeaturesList;

    int     availableOptionalFeaturesListLength;
    int     *availableOptionalFeaturesList;

public:
    ExynosCameraSensorInfoBase();
};

struct ExynosCameraSensor2L7Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor2L7Base();
};

struct ExynosCameraSensor2P8Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor2P8Base();
};

struct ExynosCameraSensorIMX333_2L2Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensorIMX333_2L2Base(int sensorId);
};

struct ExynosCameraSensor2L3Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor2L3Base(int sensorId);
};

struct ExynosCameraSensor6B2Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor6B2Base(int sensorId);
};

struct ExynosCameraSensorIMX320_3H1Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensorIMX320_3H1Base(int sensorId);
};

struct ExynosCameraSensor3M3Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor3M3Base(int sensorId);
};

struct ExynosCameraSensorS5K5F1Base : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensorS5K5F1Base(int sensorId);
};

struct ExynosCameraSensorS5KRPBBase : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensorS5KRPBBase(int sensorId);
};

struct ExynosCameraSensor2P7SQBase : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor2P7SQBase(int sensorId);
};

struct ExynosCameraSensor2T7SXBase : public ExynosCameraSensorInfoBase {
public:
    ExynosCameraSensor2T7SXBase(int sensorId);
};

}; /* namespace android */
#endif

