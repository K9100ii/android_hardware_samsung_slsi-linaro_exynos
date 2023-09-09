#ifndef _EXYNOS_CAMERA_TUNING_IS_REGION_H_
#define _EXYNOS_CAMERA_TUNING_IS_REGION_H_

typedef enum {
    ISP_AA_COMMAND_START                = 0,
    ISP_AA_COMMAND_STOP                 = 1
}ISP_LockCommandEnum;

typedef enum {
    ISP_AA_TARGET_AF                    = 1,
    ISP_AA_TARGET_AE                    = 2,
    ISP_AA_TARGET_AWB                   = 4
}ISP_LockTargetEnum;

typedef enum {
    ISP_AF_MANUAL = 0,
    ISP_AF_SINGLE,
    ISP_AF_CONTINUOUS,
    ISP_AF_SET_DEFAULT_POS,
    ISP_AF_MANUAL_WITH_DIOPTERS
} ISP_AfModeEnum;

typedef enum {
    ISP_AF_SCENE_NORMAL = 0,
    ISP_AF_SCENE_MACRO
} ISP_AfSceneEnum;

typedef enum {
    ISP_AF_TOUCH_DISABLE = 0,
    ISP_AF_TOUCH_ENABLE
} ISP_AfTouchEnum;

typedef enum {
    ISP_AF_FACE_DISABLE = 0,
    ISP_AF_FACE_ENABLE
} ISP_AfFaceEnum;

typedef enum {
    ISP_AF_RESPONSE_PREVIEW = 0,
    ISP_AF_RESPONSE_MOVIE
} ISP_AfResponseEnum;

typedef enum {
    ISP_FLASH_COMMAND_DISABLE           = 0 ,
    ISP_FLASH_COMMAND_MANUAL_ON         = 1, //(forced flash)
    ISP_FLASH_COMMAND_AUTO              = 2,
    ISP_FLASH_COMMAND_TORCH             = 3,  //(3√ )
    ISP_FLASH_COMMAND_FLASH_ON          = 4,
    ISP_FLASH_COMMAND_CAPTURE           = 5,
    // SIRC added:
    ISP_FLASH_COMMAND_TRIGGER           = 6,
    ISP_FLASH_COMMAND_CALIBRATION       = 7,
    // [2012.12.15 ist.song] add for cml flash algorithm interface
    ISP_FLASH_COMMAND_START             = 8,
    ISP_FLASH_COMMAND_CANCEL            = 9
}ISP_FlashCommandEnum;

typedef enum {
    ISP_FLASH_REDEYE_DISABLE            = 0,
    ISP_FLASH_REDEYE_ENABLE             = 1
}ISP_FlashRedeyeEnum;

typedef enum {
    ISP_AWB_COMMAND_AUTO                = 0,
    ISP_AWB_COMMAND_ILLUM               = 1,
    ISP_AWB_COMMAND_MANUAL              = 2,
    ISP_AWB_COMMAND_CUSTOM_K            = 3
}ISP_AwbCommandEnum;

typedef enum {
    ISP_AWB_ILLUMINATION_DAYLIGHT       = 0,
    ISP_AWB_ILLUMINATION_CLOUDY         = 1,
    ISP_AWB_ILLUMINATION_TUNGSTEN       = 2,
    ISP_AWB_ILLUMINATION_FLUORESCENT    = 3
}ISP_AwbIlluminationEnum;

typedef enum {
    ISP_IMAGE_EFFECT_DISABLE            =  0,
    ISP_IMAGE_EFFECT_MONOCHROME         =  1, // SIRC_ISP_IMAGEEFFECT_MONOCHROME
    ISP_IMAGE_EFFECT_NEGATIVE_MONO      =  2, // SIRC_ISP_IMAGEEFFECT_NEGATIVE_MONOCHROME
    ISP_IMAGE_EFFECT_NEGATIVE_COLOR     =  3, // SIRC_ISP_IMAGEEFFECT_NEGATIVE
    ISP_IMAGE_EFFECT_SEPIA              =  4, // SIRC_ISP_IMAGEEFFECT_SEPIA
    ISP_IMAGE_EFFECT_AQUA               =  5, // SIRC_ISP_IMAGEEFFECT_AQUA
    ISP_IMAGE_EFFECT_EMBOSS             =  6, // SIRC_ISP_IMAGEEFFECT_EMBOSS
    ISP_IMAGE_EFFECT_EMBOSS_MONO        =  7, // SIRC_ISP_IMAGEEFFECT_EMBOSS_MONOCHROME
    ISP_IMAGE_EFFECT_SKETCH             =  8, // SIRC_ISP_IMAGEEFFECT_SKETCH
    ISP_IMAGE_EFFECT_RED_YELLOW_POINT   =  9, // SIRC_ISP_IMAGEEFFECT_RED_YELLOW_POINT
    ISP_IMAGE_EFFECT_GREEN_POINT        = 10, // SIRC_ISP_IMAGEEFFECT_GREEN_POINT
    ISP_IMAGE_EFFECT_BLUE_POINT         = 11, // SIRC_ISP_IMAGEEFFECT_BLUE_POINT
    ISP_IMAGE_EFFECT_MAGENTA_POINT      = 12, // SIRC_ISP_IMAGEEFFECT_MAGENTA_POINT
    ISP_IMAGE_EFFECT_WARM_VINTAGE       = 13, // SIRC_ISP_IMAGEEFFECT_WARM_VINTAGE
    ISP_IMAGE_EFFECT_COLD_VINTAGE       = 14, // SIRC_ISP_IMAGEEFFECT_COLD_VINTAGE
    ISP_IMAGE_EFFECT_POSTERIZE          = 15, // SIRC_ISP_IMAGEEFFECT_POSTERIZE
    ISP_IMAGE_EFFECT_SOLARIZE           = 16, // SIRC_ISP_IMAGEEFFECT_SOLARIZE
    ISP_IMAGE_EFFECT_WASHED             = 17, // SIRC_ISP_IMAGEEFFECT_WASHED
    ISP_IMAGE_EFFECT_CCM                = 18, // SIRC_ISP_IMAGEEFFECT_CUSTOM_COLOR_CORRECTION
    ISP_IMAGE_EFFECT_BEAUTY_FACE        = 19  // SIRC_ISP_IMAGEEFFECT_BEAUTY_FACE
}ISP_ImageEffectEnum;

typedef enum {
    ISP_ISO_COMMAND_AUTO                = 0,
    ISP_ISO_COMMAND_MANUAL              = 1
}ISP_IsoCommandEnum;

typedef enum {
    ISP_ADJUST_COMMAND_AUTO                     = 0,
    ISP_ADJUST_COMMAND_MANUAL_CONTRAST          = (1 << 0),
    ISP_ADJUST_COMMAND_MANUAL_SATURATION        = (1 << 1),
    ISP_ADJUST_COMMAND_MANUAL_SHARPNESS         = (1 << 2),
    ISP_ADJUST_COMMAND_MANUAL_EXPOSURE          = (1 << 3),
    ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS        = (1 << 4),
    ISP_ADJUST_COMMAND_MANUAL_HUE               = (1 << 5),
    ISP_ADJUST_COMMAND_MANUAL_HOTPIXEL          = (1 << 6),
    ISP_ADJUST_COMMAND_MANUAL_NOISEREDUCTION    = (1 << 7),
    ISP_ADJUST_COMMAND_MANUAL_SHADING           = (1 << 8),
    ISP_ADJUST_COMMAND_MANUAL_GAMMA             = (1 << 9),
    ISP_ADJUST_COMMAND_MANUAL_EDGEENHANCEMENT   = (1 << 10),
    ISP_ADJUST_COMMAND_MANUAL_SCENE             = (1 << 11),
    ISP_ADJUST_COMMAND_MANUAL_FRAMETIME         = (1 << 12),
    ISP_ADJUST_COMMAND_MANUAL_SENSOR_SHUTTER    = (1 << 13),
    ISP_ADJUST_COMMAND_MANUAL_ALL               = 0x3FFF
}ISP_AdjustCommandEnum;

typedef enum {
    ISP_ADJUST_SCENE_NORMAL             = 0,
    ISP_ADJUST_SCENE_NIGHT_PREVIEW      = 1,
    ISP_ADJUST_SCENE_NIGHT_CAPTURE      = 2
}ISP_AdjustSceneIndexEnum;

typedef enum {
    ISP_METERING_COMMAND_AVERAGE        = 0,
    ISP_METERING_COMMAND_SPOT           = 1,
    ISP_METERING_COMMAND_MATRIX         = 2,
    ISP_METERING_COMMAND_CENTER         = 3,
    ISP_METERING_COMMAND_EXPOSURE_MODE  = (1 << 8)
}ISP_MetertingCommandEnum;

typedef enum {
    ISP_EXPOSUREMODE_OFF                = 1,
    ISP_EXPOSUREMODE_AUTO               = 2,
    ISP_EXPOSUREMODE_MANUAL             = 3
}ISP_ExposureModeEnum;

typedef enum {
    ISP_AFC_COMMAND_DISABLE             = 0,
    ISP_AFC_COMMAND_AUTO                = 1,
    ISP_AFC_COMMAND_MANUAL              = 2,
    ISP_AFC_COMMAND_AUTO_50HZ           = 3,
    ISP_AFC_COMMAND_AUTO_60HZ           = 4
}ISP_AfcCommandEnum;

typedef enum {
    ISP_AFC_MANUAL_50HZ                 = 50,
    ISP_AFC_MANUAL_60HZ                 = 60
}ISP_AfcManualEnum;

typedef enum {
    ISP_SCENE_NONE                      = 0,
    ISP_SCENE_PORTRAIT                  = 1,
    ISP_SCENE_LANDSCAPE                 = 2,
    ISP_SCENE_SPORTS                    = 3,
    ISP_SCENE_PARTYINDOOR               = 4,
    ISP_SCENE_BEACHSNOW                 = 5,
    ISP_SCENE_SUNSET                    = 6,
    ISP_SCENE_DAWN                      = 7,
    ISP_SCENE_FALL                      = 8,
    ISP_SCENE_NIGHT                     = 9,
    ISP_SCENE_AGAINSTLIGHTWLIGHT        = 10,
    ISP_SCENE_AGAINSTLIGHTWOLIGHT       = 11,
    ISP_SCENE_FIRE                      = 12,
    ISP_SCENE_TEXT                      = 13,
    ISP_SCENE_CANDLE                    = 14,
    ISP_SCENE_ANTISHAKE                 = 15
}ISP_SceneEnum;

typedef enum {
    ISP_BDS_COMMAND_DISABLE             = 0,
    ISP_BDS_COMMAND_ENABLE              = 1
}ISP_BDSCommandEnum;

typedef enum {
    ISP_BNS_BINNING_COMMAND_DISABLE             = 0,
    ISP_BNS_BINNING_COMMAND_ENABLE              = 1
}ISP_BnsBinningCommandEnum;

typedef enum {
    ISP_BAYER_CROP_COMMAND_DISABLE             = 0,
    ISP_BAYER_CROP_COMMAND_ENABLE              = 1
}ISP_BayerCropCommandEnum;

#define MAX_FRAMEDESCRIPTOR_CONTEXT_NUM 500 //30*20 // 600 frame
#define MAX_VERSION_DISPLAY_BUF 128
#ifndef TOTAL_NUM_AF_WINDOWS
#define TOTAL_NUM_AF_WINDOWS        11
#endif
#ifndef TOTAL_NUM_AF_WINDOWS_V37
#define TOTAL_NUM_AF_WINDOWS_V37    2
#endif

typedef struct {
    uint32_t uStartX;
    uint32_t uStartY;
    uint32_t uWidth;
    uint32_t uHeight;
} AF_Window_Info;

typedef struct {
    uint32_t gradient1SumLow;
    uint32_t gradient1SumHigh;
    uint32_t gradient1Count;
    uint32_t gradient2SumLow;
    uint32_t gradient2SumHigh;
    uint32_t gradient2Count;
    uint32_t highpassFilterSumLow;
    uint32_t highpassFilterSumHigh;
    uint32_t highpassFilterCount;
    uint32_t highpassFilterMaxSum;
    uint32_t filter3x9SumLow;
    uint32_t filter3x9SumHigh;
    uint32_t filter3x9Count;
    uint32_t filter1x17SumLow;
    uint32_t filter1x17SumHigh;
    uint32_t filter1x17Count;
    uint32_t clipCount;
    uint32_t luminanceSumLow;
    uint32_t luminanceSumHigh;
} AF_Stat_Info;

typedef struct {
    // Physical range
    uint16_t uInfPosition;
    uint16_t uMacroPosition;

    // Default pos
    uint16_t uPanPosition;
    uint16_t uDefaultMacroPosition;
} AF_Cal_Info;

typedef struct {
    //Sensor
    uint32_t Sensor_FrameTime;
    uint32_t Sensor_ExposureTime;
    uint32_t Sensor_AnalogGain;
    //Monitor for AA
    uint32_t ReqLei;

    uint32_t NextNextLEI_Exp;
    uint32_t NextNextLEI_AGain;
    uint32_t NextNextLEI_DGain;
    uint32_t NextNextLEI_StatLei;
    uint32_t NextNextLEI_Lei;

    uint32_t Dummy0;
} IS_Debug_FrameDescriptor;

typedef struct {
    uint32_t uMinTimeUs;      //min time
    uint32_t uMaxTimeUs;     //max time
    uint32_t uAvrgTimeUs;    //avergage time
    uint32_t uCurrentTimeUs; //current time
} IS_TimeMeasureUS;

typedef enum
{
    NO_WARNING_ERROR        = 0x00000000,
    SDK_W_ISPEND_EOF        = 1,
    SDK_W_ISPEND_UNEXPECTED = 2,
    SDK_E_CORRUP_FRAME      = 3,
    SDK_E_CHAIN_HANG        = 4,
    SDK_E_JSON_PARING       = 5,

    //you can add additional error code
} SDK_WARNING_ERROREnum;

// Shared Region Structure for Jig and Tuning Solution
#if (USE_PERF_MEASUREMENT == 1)
#define MAX_NUM_PERF 40
#define MAX_STREAM_ID 4

typedef struct
{
    uint32_t uMeasureTime;
    uint32_t uframeNum;
} IS_TimeMetricStr;

typedef struct
{
    uint32_t uPreviousCount;
    uint32_t uCurrIdx;
    uint32_t uCpuUsageRate[MAX_NUM_PERF];
} IS_CpuUsageMetricStr;

typedef struct
{
    IS_TimeMetricStr shot2shotdoneTimeGroup0[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup1[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup2[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup3[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup4[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup5[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup6[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup7[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup8[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shot2shotdoneTimeGroup9[MAX_STREAM_ID][MAX_NUM_PERF];

    IS_TimeMetricStr hwTimeCSIS[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTime3AA0[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTime3AA1[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeISP0[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeISP1[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeTPU[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeSCALERP[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeMCSC0[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeMCSC1[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeVRA[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeTPUGDC[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr hwTimeDCP[MAX_STREAM_ID][MAX_NUM_PERF];

    IS_TimeMetricStr IsrTimeCsisEnd[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr IsrTime3aaEnd[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr IsrTimeIspEnd[MAX_STREAM_ID][MAX_NUM_PERF];

    IS_TimeMetricStr shotdone2shotdoneTimeGroup2[MAX_STREAM_ID][MAX_NUM_PERF];
    IS_TimeMetricStr shotdone2shotdoneTimeGroup4[MAX_STREAM_ID][MAX_NUM_PERF];

    IS_CpuUsageMetricStr cpuUsage;
} IS_SharePerfMeasurementStr;
#endif

typedef struct
{
    uint32_t                   uFrameTime[3];
    uint32_t                   uExposureTime[3];
    uint32_t                   uAnalogGain[3];
    uint32_t                   uDigitalGain[3];

    uint32_t                   uRGains[3]; // WB
    uint32_t                   uGGains[3];
    uint32_t                   uBGains[3];

    uint16_t                   reserved_uAFActuatorPosition;   // AF pos in actuator coordinate
    uint16_t                   reserved_uAFAlgPosition;// AF pos in algorithm coordinate
    uint32_t                   reserved_uAFStatus;     // AF Status
    AF_Window_Info           AFWindowInfo[2][TOTAL_NUM_AF_WINDOWS];
    AF_Stat_Info             AFStatInfo[2][TOTAL_NUM_AF_WINDOWS];
    AF_Cal_Info              reserved_AFCalInfo;

    //later, we should add MON paramter for tune and debug
    uint32_t                   uFrmDescpOnOffCntr;
    uint32_t                   uFrmDescpUpdateDone;
    uint32_t                   uFrmDescpIdx;
    uint32_t                   uFrmDescpMaxIdx; // max index to store
    IS_Debug_FrameDescriptor uDbgFrmDescpCntxt[MAX_FRAMEDESCRIPTOR_CONTEXT_NUM]; // Dbg Frame Descript buffer

    //version information
    uint32_t                   uChipID;
    uint32_t                   uChipRevNo;
    uint8_t                    uIspfwVersionNo[MAX_VERSION_DISPLAY_BUF];
    uint8_t                    uIspfwVersionDate[MAX_VERSION_DISPLAY_BUF];
    uint8_t                    uSIRCSDKVersionNo[MAX_VERSION_DISPLAY_BUF];
    uint8_t                    uSIRCSDKRevsionNo[MAX_VERSION_DISPLAY_BUF];
    uint8_t                    uSIRCSDKVersionDate[MAX_VERSION_DISPLAY_BUF];

    //measure timing
    IS_TimeMeasureUS         uISPSDKTime;
    //A5 System Error
    SDK_WARNING_ERROREnum    uErrorCode;


    // KJ_120901 : Detecting face number.
    uint32_t                   uFaceNumber;

    // [hj529.kim, 2012/09/12] RawSetfile information for SetFile V3
    uint32_t                   uSetFileRawBufAddr[64]; //MAX_SETFILE_NUM;
    uint32_t                   uSetFileRawBufSize[64]; //MAX_SETFILE_NUM;

    // [hj529.kim, 2012/09/15] To tune DRC
    uint32_t                   uDrcOpMode[4];            // DRC Operation Mode [ms0119.lee, 2013/10/16]
    uint32_t                   uDrcSetfileAddress;
    uint32_t                   uDrcSetfileSize;

    // [hj529.kim, 2012/09/18] To tune DIS
    uint32_t                   uDisOpMode[4];
    uint32_t                   uDisSetfileAddress;
    uint32_t                   uDisSetfileSize;

    // [hj529.kim, 2012/09/14] To tune 3DNR
    uint32_t                   uTdnrOpMode[4];             // TDNR Operation Mode [ms0119.lee, 2013/10/16]
    uint32_t                   uTdnrSetfileAddress;
    uint32_t                   uTdnrSetfileSize;

    uint32_t                   uFdSetfileAddress;
    uint32_t                   uFdSetfileSize;
    uint32_t                   uFdUpdateStatus;     // Modified by payton (sehoon.kim) 20140818 - FD setfile update status parameter

    //[2013/12/13, jspark] to tranfer DT data to Host
    uint32_t                   uConcordDTBufAddr;
    uint32_t                   uConcordDTBufSize;
    //[2014/01/13, jspark] to tranfer SPI data to Host
    uint32_t                   uConcordSPIBufAddr;
    uint32_t                   uConcordSPIBufSize;

    uint32_t                  eConcordBypass;
    uint32_t                  eConcordLsc;

    //[2015/06/03] for FD strand alone
    uint32_t                  uFdDmaImageAddr[3];

#if (USE_PERF_MEASUREMENT == 1)
    IS_SharePerfMeasurementStr strISSharePerfMeasurement;
#endif

} IS_ShareRegionStr;

enum {
    SCALER_FLIP_COMMAND_NORMAL      = 0,
    SCALER_FLIP_COMMAND_X_MIRROR    = 1,
    SCALER_FLIP_COMMAND_Y_MIRROR    = 2,
    SCALER_FLIP_COMMAND_XY_MIRROR   = 3,
};
#endif
