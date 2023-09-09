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

#ifndef EXYNOS_CAMERA_CONFIGURATIONS_H
#define EXYNOS_CAMERA_CONFIGURATIONS_H

#include <utils/threads.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <cutils/properties.h>
#include <CameraParameters.h>

#include <videodev2.h>
#include <videodev2_exynos_media.h>
#include <videodev2_exynos_camera.h>
#include <map>

#ifdef USE_CSC_FEATURE
#include <SecNativeFeature.h>
#endif

#include "ExynosCameraObject.h"
#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraSensorInfoBase.h"
#include "ExynosCameraCounter.h"
#include "fimc-is-metadata.h"
#include "ExynosRect.h"
#include "exynos_format.h"
#include "ExynosExif.h"
#include "ExynosCameraUtils.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraAutoTimer.h"

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
#include "SecCameraUtil.h"
#endif
#ifdef SAMSUNG_OIS
#include "ExynosCameraNode.h"
#endif
#ifdef SAMSUNG_DNG
#include "SecCameraDng.h"
#include "SecCameraDngThumbnail.h"
#endif

#include "ExynosCameraSensorInfo.h"

#ifdef SAMSUNG_UNI_API
#include "uni_api_wrapper.h"
#endif

#define FW_CUSTOM_OFFSET (1)
#define V4L2_FOURCC_LENGTH 5
#define IS_WBLEVEL_DEFAULT (4)

#define GPS_PROCESSING_METHOD_SIZE 32

#define EXYNOS_CONFIG_DEFINED (-1)
#define EXYNOS_CONFIG_NOTDEFINED (-2)

namespace android {

using namespace std;
using ::android::hardware::camera::common::V1_0::helper::CameraParameters;
using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

enum YuvStallUsage {
    YUV_STALL_USAGE_DSCALED = 0,
    YUV_STALL_USAGE_PICTURE = 1
};

namespace CONFIG_MODE {
    enum MODE {
        NORMAL        = 0x00,
        HIGHSPEED_60,
        HIGHSPEED_120,
        HIGHSPEED_240,
        HIGHSPEED_480,
#ifdef SAMSUNG_SSM
        SSM_240,
#endif
        MAX
    };
};

struct CONFIG_PIPE {
    uint32_t prepare[MAX_PIPE_NUM_REPROCESSING];
};

struct CONFIG_BUFFER {
    uint32_t num_sensor_buffers;
    uint32_t num_bayer_buffers;
    uint32_t num_3aa_buffers;
    uint32_t num_hwdis_buffers;
    uint32_t num_preview_buffers;
    uint32_t num_preview_cb_buffers;
    uint32_t num_picture_buffers;
    uint32_t num_nv21_picture_buffers;
    uint32_t num_reprocessing_buffers;
    uint32_t num_recording_buffers;
    uint32_t num_fastaestable_buffer;
    uint32_t num_vra_buffers;
#ifdef USE_DUAL_CAMERA
    uint32_t dual_num_fusion_buffers;
#endif
    uint32_t reprocessing_bayer_hold_count;
    uint32_t num_request_raw_buffers;
    uint32_t num_request_bayer_buffers;
    uint32_t num_request_preview_buffers;
    uint32_t num_request_callback_buffers;
    uint32_t num_request_video_buffers;
    uint32_t num_request_burst_capture_buffers;
    uint32_t num_request_capture_buffers;
    uint32_t num_request_jpeg_buffers;
    uint32_t num_batch_buffers;
};

struct CONFIG_BUFFER_PIPE {
    struct CONFIG_PIPE pipeInfo;
    struct CONFIG_BUFFER bufInfo;
};

struct ExynosConfigInfo {
    struct CONFIG_BUFFER_PIPE *current;
    struct CONFIG_BUFFER_PIPE info[CONFIG_MODE::MAX];
    uint32_t mode;
};

struct CameraMetaParameters {
    int m_flashMode;
    float m_prevZoomRatio;
    float m_zoomRatio;
    ExynosRect m_zoomRect;
#ifdef SAMSUNG_OT
    bool m_startObjectTracking;
#endif
};

typedef union {
    unsigned short value;
    struct {
        unsigned int af       : 1;
        unsigned int ae       : 1;
        unsigned int w_done   : 1;
        unsigned int t_done   : 1;
        unsigned int bDist    : 1;
        unsigned int bLux     : 1;
        unsigned int w_state  : 4;
        unsigned int t_state  : 4;
        unsigned int reserved : 2;
    } bit;
} uFusionCaptureState;

#ifdef USE_DUAL_CAMERA
enum DUAL_PREVIEW_MODE {
    DUAL_PREVIEW_MODE_OFF,
    DUAL_PREVIEW_MODE_HW,
    DUAL_PREVIEW_MODE_SW,
    DUAL_PREVIEW_MODE_MAX,
};

enum DUAL_REPROCESSING_MODE {
    DUAL_REPROCESSING_MODE_OFF,
    DUAL_REPROCESSING_MODE_HW,
    DUAL_REPROCESSING_MODE_SW,
    DUAL_REPROCESSING_MODE_MAX,
};

enum DUAL_OPERATION_MODE {
    DUAL_OPERATION_MODE_NONE,
    DUAL_OPERATION_MODE_MASTER,
    DUAL_OPERATION_MODE_SLAVE,
    DUAL_OPERATION_MODE_SYNC,
    DUAL_OPERATION_MODE_MAX,
};

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
enum dual_solution_size_enum {
    DUAL_SOLUTION_SIZE_DST_W        = 0,
    DUAL_SOLUTION_SIZE_DST_H,
    DUAL_SOLUTION_SIZE_SRC_WIDE_W,
    DUAL_SOLUTION_SIZE_SRC_WIDE_H,
    DUAL_SOLUTION_SIZE_SRC_TELE_W,
    DUAL_SOLUTION_SIZE_SRC_TELE_H,
    DUAL_SOLUTION_SIZE_MAX,
};
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */
#endif

#ifdef SAMSUNG_HIFI_VIDEO
enum hifivideo_solution_size_enum {
    HIFIVIDEO_SOLUTION_SIZE_VIDEO_W = 0,
    HIFIVIDEO_SOLUTION_SIZE_VIDEO_H,
    HIFIVIDEO_SOLUTION_SIZE_VIDEO_FPS,
    HIFIVIDEO_SOLUTION_SIZE_MAX,
};
#endif

/* refer to fimc_is_ex_mode on kernel side*/
enum EXTEND_SENSOR_MODE {
    EXTEND_SENSOR_MODE_NONE = 0,
    EXTEND_SENSOR_MODE_DRAM_TEST = 1,
    EXTEND_SENSOR_MODE_LIVE_FOCUS = 2,
    EXTEND_SENSOR_MODE_DUAL_FPS_960 = 3,
    EXTEND_SENSOR_MODE_DUAL_FPS_480 = 4,
};

#ifdef SAMSUNG_FACTORY_DRAM_TEST
enum FACTORY_DRAM_TEST_STATE {
    FACTORY_DRAM_TEST_NONE = 0,
    FACTORY_DRAM_TEST_CEHCKING = 1,
    FACTORY_DRAM_TEST_DONE_FAIL = 2,
    FACTORY_DRAM_TEST_DONE_SUCCESS = 3,
};
#endif

enum CONFIGURATION_SIZE_TYPE {
    CONFIGURATION_YUV_SIZE,
    CONFIGURATION_MIN_YUV_SIZE,
    CONFIGURATION_MAX_YUV_SIZE,
    CONFIGURATION_PREVIEW_SIZE,
    CONFIGURATION_VIDEO_SIZE,
    CONFIGURATION_PICTURE_SIZE,
    CONFIGURATION_THUMBNAIL_SIZE,
//#ifdef SAMSUNG_TN_FEATURE
    CONFIGURATION_THUMBNAIL_CB_SIZE,
    CONFIGURATION_DS_YUV_STALL_SIZE,
//#endif
    CONFIGURATION_SIZE_MAX,
};

enum CONFIGURATION_MODE_TYPE {
    CONFIGURATION_FULL_SIZE_LUT_MODE,
    CONFIGURATION_ODC_MODE,
    CONFIGURATION_VISION_MODE,
    CONFIGURATION_PIP_MODE,
    CONFIGURATION_RECORDING_MODE,
    CONFIGURATION_PIP_RECORDING_MODE,
    CONFIGURATION_YSUM_RECORDING_MODE,
    CONFIGURATION_HDR_RECORDING_MODE,
    CONFIGURATION_VIDEO_STABILIZATION_MODE,
    CONFIGURATION_HDR_MODE,
    CONFIGURATION_DYNAMIC_BAYER_MODE,
    CONFIGURATION_DVFS_LOCK_MODE,
    CONFIGURATION_MANUAL_AE_CONTROL_MODE,
    CONFIGURATION_OBTE_MODE,
    CONFIGURATION_OBTE_STREAM_CHANGING,
#ifdef USE_DUAL_CAMERA
    CONFIGURATION_DUAL_MODE,
    CONFIGURATION_DUAL_PRE_MODE,
#endif
#ifdef SUPPORT_DEPTH_MAP
    CONFIGURATION_DEPTH_MAP_MODE,
#endif
    CONFIGURATION_GMV_MODE,
#ifdef SAMSUNG_TN_FEATURE
    CONFIGURATION_PRO_MODE,
    CONFIGURATION_DYNAMIC_PICK_CAPTURE_MODE,
    CONFIGURATION_FACTORY_TEST_MODE,
#ifdef SAMSUNG_SW_VDIS
    CONFIGURATION_SWVDIS_MODE,
#endif
#ifdef SAMSUNG_HYPERLAPSE
    CONFIGURATION_HYPERLAPSE_MODE,
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
    CONFIGURATION_VIDEO_BEAUTY_MODE,
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
    CONFIGURATION_VIDEO_BEAUTY_SNAPSHOT_MODE,
#endif
#ifdef LLS_CAPTURE
    CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE,
#endif
#ifdef SET_LLS_CAPTURE_SETFILE
    CONFIGURATION_LLS_CAPTURE_MODE,
#endif
#ifdef OIS_CAPTURE
    CONFIGURATION_OIS_CAPTURE_MODE,
#endif
#ifdef SAMSUNG_LENS_DC
    CONFIGURATION_LENS_DC_MODE,
#endif
#ifdef RAWDUMP_CAPTURE
    CONFIGURATION_RAWDUMP_CAPTURE_MODE,
#endif
#ifdef SAMSUNG_STR_CAPTURE
    CONFIGURATION_STR_CAPTURE_MODE,
#endif
#ifdef SAMSUNG_OT
    CONFIGURATION_OBJECT_TRACKING_MODE,
#endif
#ifdef SAMSUNG_HRM
    CONFIGURATION_HRM_MODE,
#endif
#ifdef SAMSUNG_LIGHT_IR
    CONFIGURATION_LIGHT_IR_MODE,
#endif
#ifdef SAMSUNG_GYRO
    CONFIGURATION_GYRO_MODE,
#endif
#ifdef SAMSUNG_ACCELEROMETER
    CONFIGURATION_ACCELEROMETER_MODE,
#endif
#ifdef SAMSUNG_ROTATION
    CONFIGURATION_ROTATION_MODE,
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    CONFIGURATION_FUSION_CAPTURE_MODE,
#endif
#ifdef SUPPORT_ZSL_MULTIFRAME
    CONFIGURATION_ZSL_MULTIFRAME_MODE,
#endif
#ifdef SAMSUNG_PROX_FLICKER
    CONFIGURATION_PROX_FLICKER_MODE,
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    CONFIGURATION_HIFIVIDEO_MODE,
#endif
#ifdef FAST_SHUTTER_NOTIFY
    CONFIGURATION_FAST_SHUTTER_MODE,
#endif
#ifdef USE_ALWAYS_FD_ON
    CONFIGURATION_ALWAYS_FD_ON_MODE,
#endif
#endif
    CONFIGURATION_MODE_MAX,
};

enum DYNAMIC_MODE_TYPE {
    DYNAMIC_UHD_RECORDING_MODE,
    DYNAMIC_HIGHSPEED_RECORDING_MODE,
#ifdef USE_DUAL_CAMERA
    DYNAMIC_DUAL_FORCE_SWITCHING,
#endif
#ifdef SAMSUNG_TN_FEATURE
#ifdef SAMSUNG_HIFI_CAPTURE
    DYNAMIC_HIFI_CAPTURE_MODE,
#endif
#ifdef SAMSUNG_IDDQD
    DYNAMIC_IDDQD_MODE,
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    DYNAMIC_DUAL_CAMERA_DISABLE,
#endif
#endif
    DYNAMIC_MODE_MAX,
};

enum CONFIGURATON_STATIC_VALUE_TYPE {
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    CONFIGURATION_DUAL_DISP_FALLBACK_RESULT,
    CONFIGURATION_DUAL_TARGET_FALLBACK_RESULT,
    CONFIGURATION_DUAL_OP_MODE_FALLBACK,
#endif
    CONFIGURATION_STATIC_VALUE_MAX
};

enum CONFIGURATION_VALUE_TYPE {
    CONFIGURATION_VT_MODE,
    CONFIGURATION_JPEG_QUALITY,
    CONFIGURATION_THUMBNAIL_QUALITY,
    CONFIGURATION_YUV_SIZE_RATIO_ID,
    CONFIGURATION_DEVICE_ORIENTATION,
    CONFIGURATION_FD_ORIENTATION,
    CONFIGURATION_FLIP_HORIZONTAL,
    CONFIGURATION_FLIP_VERTICAL,
    CONFIGURATION_HIGHSPEED_MODE,
    CONFIGURATION_RECORDING_FPS,
    CONFIGURATION_BURSTSHOT_FPS,
    CONFIGURATION_BURSTSHOT_FPS_TARGET,
    CONFIGURATION_NEED_DYNAMIC_BAYER_COUNT,
    CONFIGURATION_EXTEND_SENSOR_MODE,
    CONFIGURATION_MARKING_EXIF_FLASH,
    CONFIGURATION_FLASH_MODE,
    CONFIGURATION_BINNING_RATIO,
    CONFIGURATION_OBTE_TUNING_MODE,
    CONFIGURATION_OBTE_TUNING_MODE_NEW,
    CONFIGURATION_OBTE_DATA_PATH,
    CONFIGURATION_OBTE_DATA_PATH_NEW,
#ifdef SAMSUNG_TN_FEATURE
    CONFIGURATION_SHOT_MODE,
    CONFIGURATION_MULTI_CAPTURE_MODE,
    CONFIGURATION_OPERATION_MODE,
    CONFIGURATION_SERIES_SHOT_MODE,
    CONFIGURATION_SERIES_SHOT_COUNT,
#endif
    CONFIGURATION_YUV_STALL_PORT,
    CONFIGURATION_YUV_STALL_PORT_USAGE,
#ifdef SAMSUNG_TN_FEATURE
    CONFIGURATION_PREV_TRANSIENT_ACTION_MODE,
    CONFIGURATION_TRANSIENT_ACTION_MODE,
#ifdef SAMSUNG_HYPERLAPSE
    CONFIGURATION_HYPERLAPSE_SPEED,
#endif
#ifdef LLS_CAPTURE
    CONFIGURATION_LLS_VALUE,
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    CONFIGURATION_LD_CAPTURE_MODE,
    CONFIGURATION_LD_CAPTURE_COUNT,
    CONFIGURATION_LD_CAPTURE_LLS_VALUE,
#endif
    CONFIGURATION_EXIF_CAPTURE_STEP_COUNT,
#ifdef SAMSUNG_DOF
    CONFIGURATION_FOCUS_LENS_POS,
#endif
#ifdef SAMSUNG_FACTORY_DRAM_TEST
    CONFIGURATION_FACTORY_DRAM_TEST_STATE,
#endif
#ifdef SAMSUNG_SSM
    CONFIGURATION_SSM_SHOT_MODE,
    CONFIGURATION_SSM_MODE_VALUE,
    CONFIGURATION_SSM_STATE,
    CONFIGURATION_SSM_TRIGGER,
#endif
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    CONFIGURATION_DUAL_DISP_CAM_TYPE,     /* enum UNI_PLUGIN_CAMERA_TYPE */
    CONFIGURATION_DUAL_RECOMMEND_CAM_TYPE,  /* enum UNI_PLUGIN_CAMERA_TYPE */
    CONFIGURATION_DUAL_CAMERA_DISABLE,  /* for high temperature scenario*/
    CONFIGURATION_DUAL_PREVIEW_SHIFT_X,
    CONFIGURATION_DUAL_PREVIEW_SHIFT_Y,
    CONFIGURATION_DUAL_HOLD_FALLBACK_STATE,
    CONFIGURATION_DUAL_2X_BUTTON,
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    CONFIGURATION_FUSION_CAPTURE_READY,
#ifdef DEBUG_IQ_CAPTURE_MODE
    CONFIGURATION_DEBUG_FUSION_CAPTURE_MODE,
#endif
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    CONFIGURATION_HIFIVIDEO_OPMODE,
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_DUAL_PORTRAIT_BEAUTY)
    CONFIGURATION_BEAUTY_RETOUCH_LEVEL,
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    CONFIGURATION_CURRENT_BOKEH_PREVIEW_RESULT,    /* used for bokeh capture input value */
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    CONFIGURATION_BEAUTY_FACE_SKIN_COLOR_LEVEL,
#endif
#endif
    CONFIGURATION_FLICKER_DATA,
    CONFIGURATION_VALUE_MAX,
};

enum SUPPORTED_FUNCTION_TYPE {
    SUPPORTED_FUNCTION_GDC,
    SUPPORTED_FUNCTION_SERVICE_BATCH_MODE,
    SUPPORTED_FUNCTION_MAX,
};

class ExynosCameraConfigurations : public ExynosCameraObject {
public:
    /* Enumerator */
    enum yuv_output_port_id {
        YUV_0,
        YUV_1,
        YUV_2,
        YUV_MAX, //3

        YUV_STALL_0 = YUV_MAX,
        YUV_STALL_1,
        YUV_STALL_2,
        YUV_STALL_MAX, //6
        YUV_OUTPUT_PORT_ID_MAX = YUV_STALL_MAX,
    };

    /* Constructor */
    ExynosCameraConfigurations(int cameraId, int scenario);

    /* Destructor */
    virtual ~ExynosCameraConfigurations();

    void            setDefaultCameraInfo(void);

private:
    /* Vendor specific con/destructor */
    void            m_vendorSpecificConstructor(void);
    void            m_vendorSpecificDestructor(void);

public:
    /* Size configuration */
    status_t        setSize(enum CONFIGURATION_SIZE_TYPE type, uint32_t width, uint32_t height, int outputPortId = -1);
    status_t        getSize(enum CONFIGURATION_SIZE_TYPE type, uint32_t *width, uint32_t *height, int outputPortId = -1) const;
    status_t        resetSize(enum CONFIGURATION_SIZE_TYPE type);

    /* Mode configuration */
    status_t        setMode(enum CONFIGURATION_MODE_TYPE type, bool enabled);
    bool            getMode(enum CONFIGURATION_MODE_TYPE type) const;
    bool            getDynamicMode(enum DYNAMIC_MODE_TYPE type) const;
    status_t        setStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type, int value);
    int             getStaticValue(enum CONFIGURATON_STATIC_VALUE_TYPE type) const;
    status_t        setModeValue(enum CONFIGURATION_VALUE_TYPE type, int value);
    int             getModeValue(enum CONFIGURATION_VALUE_TYPE type);

    /* Function configuration */
    bool            isSupportedFunction(enum SUPPORTED_FUNCTION_TYPE type) const;

    int             getScenario(void);
    void            setRestartStream(bool restart);
    bool            getRestartStream(void);

    bool            setConfig(struct ExynosConfigInfo* config);
    struct ExynosConfigInfo* getConfig(void);

    bool            setConfigMode(uint32_t mode);
    int             getConfigMode(void) const;

    void            updateMetaParameter(struct CameraMetaParameters *metaParameters);
    status_t        checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps);
    /* Sets the frame rate range for preview. */
    void            setPreviewFpsRange(uint32_t min, uint32_t max);
    /* Returns the current minimum and maximum preview fps. */
    void            getPreviewFpsRange(uint32_t *min, uint32_t *max);

    status_t        checkYuvFormat(const int format, const camera_pixel_size pixelSize, const int outputPortId);
    /* Sets the image pixel size for picture-related HW. */
    void            setYuvFormat(const int format, const int index);
    int             getYuvFormat(const int outputPortId);

    void            setYuvPixelSize(const camera_pixel_size pixelSize, const int index);
    camera_pixel_size getYuvPixelSize(const int outputPortId);

    status_t        setYuvBufferCount(const int count, const int outputPortId);
    int             getYuvBufferCount(const int outputPortId);
    void            resetYuvBufferCount(void);

    status_t        setFrameSkipCount(int count);
    status_t        getFrameSkipCount(int *count);

    /* Gets previous zoom ratio */
    float           getPrevZoomRatio(void);
    /* Gets current zoom ratio */
    float           getZoomRatio(void);
    /* Gets current zoom rect */
    void            getZoomRect(ExynosRect *zoomRect);
    /* Sets current applied zoom ratio */
    void            setAppliedZoomRatio(float zoomRatio);
    /* Gets current applied zoom ratio */
    float           getAppliedZoomRatio(void);

#ifdef USE_DUAL_CAMERA
    enum DUAL_PREVIEW_MODE      getDualPreviewMode(void);
    enum DUAL_REPROCESSING_MODE getDualReprocessingMode(void);
    void                        setDualOperationMode(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getDualOperationMode(void);
    void                        setDualOperationModeReprocessing(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getDualOperationModeReprocessing(void);
    void                        setDualOperationModeLockCount(int32_t count);
    int32_t                     getDualOperationModeLockCount(void);
    void                        decreaseDualOperationModeLockCount(void);
    void                        setDualHwSyncOn(bool hwSyncOn);
    bool                        getDualHwSyncOn(void) const;
#endif

private:
    status_t       m_adjustPreviewFpsRange(uint32_t &newMinFps, uint32_t &newMaxFps);
    bool           m_getGmvMode(void) const;

/*
 * Vendor specific function
 */
public:
    /* For Vendor */
    void            setFastEntrance(bool flag);
    bool            getFastEntrance(void) const;
    void            setSamsungCamera(bool flag);
    bool            getSamsungCamera(void) const;
    void            setUseFastenAeStable(bool enable);
    bool            getUseFastenAeStable(void) const;
    status_t        setParameters(const CameraParameters& params);

private:
    /* Called by setParameters */
    status_t        m_checkFastEntrance(const CameraParameters& params);
    status_t        m_checkSamsungCamera(const CameraParameters& params);
    status_t        m_checkRecordingFps(const CameraParameters& params);
    status_t        m_checkFactorytest(const CameraParameters& params);
    status_t        m_checkOperationMode(const CameraParameters& params);
#ifdef USE_DUAL_CAMERA
    status_t        m_checkDualMode(const CameraParameters& params);
#endif
#ifdef SAMSUNG_SW_VDIS
    status_t        m_checkSWVdisMode(const CameraParameters& params);
#endif
#ifdef SAMSUNG_HIFI_VIDEO
    status_t        m_checkHiFiVideoMode(const CameraParameters& params);
#endif
#ifdef SAMSUNG_VIDEO_BEAUTY
    status_t        m_checkVideoBeautyMode(const CameraParameters& params);
#endif

    status_t        m_checkExtSensorMode();
    status_t        m_checkVtMode(const CameraParameters& params);
    status_t        m_checkShootingMode(const CameraParameters& params);
    status_t        m_checkRecordingDrMode(const CameraParameters& params);

public:
    status_t        reInit(void);
private:
    status_t        m_vendorReInit(void);

public:
    void            resetYuvStallPort(void);

    /* Sets ExposureTime for capture */
    void            setCaptureExposureTime(uint64_t exposureTime);
    /* Gets ExposureTime for capture */
    uint64_t        getCaptureExposureTime(void);
    /* Gets LongExposureTime for capture */
    uint64_t        getLongExposureTime(void);
    /* Gets LongExposureShotCount for capture */
    int32_t         getLongExposureShotCount(void);

    void            setExposureTime(int64_t shutterSpeed);
    int64_t         getExposureTime(void);
    void            setGain(int gain);
    int             getGain(void);
    void            setLedPulseWidth(int64_t ledPulseWidth);
    int64_t         getLedPulseWidth(void);
    void            setLedPulseDelay(int64_t ledPulseDelay);
    int64_t         getLedPulseDelay(void);
    void            setLedCurrent(int ledCurrent);
    int             getLedCurrent(void);
    void            setLedMaxTime(int ledMaxTime);
    int             getLedMaxTime(void);

    int             getLightCondition(void);

    void            setYuvBufferStatus(bool flag);
    bool            getYuvBufferStatus(void);


#ifdef DEBUG_RAWDUMP
    bool           checkBayerDumpEnable(void);
#endif/* DEBUG_RAWDUMP */

#ifdef SUPPORT_DEPTH_MAP
    bool            getDepthCallbackOnPreview(void) {return 0;};
    bool            getDepthCallbackOnCapture(void) {return 0;};
#endif

#ifdef SAMSUNG_HYPERLAPSE
    status_t        checkHyperlapseMode(const CameraParameters& params);
#endif

#ifdef SAMSUNG_SW_VDIS
    status_t        checkSWVdisMode(const CameraParameters& params);
    void            setSWVdisPreviewOffset(int left, int top);
    void            getSWVdisPreviewOffset(int *left, int *top);
#endif

#if defined(SAMSUNG_VIDEO_BEAUTY) || defined (SAMSUNG_DUAL_PORTRAIT_BEAUTY)
    void            setBeautyRetouchLevel(int beautyRetouchLevel);
    int             getBeautyRetouchLevel(void);
#endif

#if defined(SAMSUNG_VIDEO_BEAUTY)
    void            setBokehRecordingHint(int bokehRecordingHint);
    int             getBokehRecordingHint(void);
#endif

#ifdef SAMSUNG_BD
    void                setBlurInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SAMSUNG_UTC_TS
    void                setUTCInfo();
#endif

#ifdef SAMSUNG_OT
    void                setObjectTrackingAreas(int validFocusArea, ExynosRect2 area, int weight);
    int                 getObjectTrackingAreas(int* validFocusArea, ExynosRect2* areas, int* weights);

    int                 setObjectTrackingFocusData(UniPluginFocusData_t *focusData);
    int                 getObjectTrackingFocusData(UniPluginFocusData_t *focusData);
#endif

#ifdef SAMSUNG_IDDQD
    void                setIDDQDresult(bool isdetected);
    bool                getIDDQDresult(void);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    void                setFusionParam(UniPluginDualCameraParam_t *cameraParam);
    UniPluginDualCameraParam_t* getFusionParam(void);

    void                resetStaticFallbackState(void);
    void                setDualSelectedCam(int32_t selectedCam);
    int32_t             getDualSelectedCam();
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
    void                checkFallbackCondition(const struct camera2_shot_ext *meta, bool flagTargetCam);
#endif /* SAMSUNG_DUAL_ZOOM_FALLBACK */
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    void                updateFusionFrameStateInfo(void);
    void                *getFusionFrameStateInfo(void);
    void                checkFusionCaptureMode(void);
    void                checkFusionCaptureCondition(struct camera2_shot_ext *wideMeta,
                                                    struct camera2_shot_ext *teleMeta);
#endif

#ifdef SAMSUNG_SSM
    void                getSSMRegion(ExynosRect2 *region);
    void                setSSMRegion(ExynosRect2 region);
    status_t            m_checkSSMShotMode(const CameraParameters& params);
#endif

    int             getAntibanding();

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    void                setBokehBlurStrength(int bokehBlurStrength);
    int                 getBokehBlurStrength(void);

    void                setZoomInOutPhoto(int zoomInOutPhoto);
    int                 getZoomInOutPhoto(void);

    void                setBokehPreviewState(int bokehState);
    int                 getBokehPreviewState(void);
    void                setBokehCaptureState(int bokehCaptureState);
    int                 getBokehCaptureState(void);

    void                setBokehProcessResult(int bokehProcessResult);
    int                 getBokehProcessResult(void);

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    void                setDualCaptureFlag(int dualCaptureFlag);
    int                 getDualCaptureFlag(void);
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

    void                setRepeatingRequestHint(int repeatingRequestHint);
    int                 getRepeatingRequestHint(void);

#ifdef SAMSUNG_HLV
    bool                getHLVEnable(bool recordingEnabled);
#endif

    void			setTemperature(int temperature);
    int 			getTemperature(void);
    int 			getProductColorInfo(void);

    status_t        getExifMetaData(struct camera2_shot *shot);
    status_t        setExifMetaData(struct camera2_shot *shot);

private:
#ifdef USE_CSC_FEATURE
    bool            m_isLatinOpenCSC();
    int             m_getAntiBandingFromLatinMCC();
#endif
#ifdef SAMSUNG_DUAL_ZOOM_FALLBACK
    void            setFallbackResult(bool enable, int32_t lux, int32_t objectDistance);
#endif

private:
    int                         m_scenario;
    static int                  m_staticValue[CONFIGURATION_STATIC_VALUE_MAX];
    struct ExynosConfigInfo     *m_exynosconfig;
    struct CameraMetaParameters m_metaParameters;
    struct ExynosCameraSensorInfoBase *m_staticInfo;

    mutable Mutex               m_modifyLock;

    /* Flags for camera status */
    bool                        m_flagRestartStream;

    uint32_t                    m_width[CONFIGURATION_SIZE_MAX];
    uint32_t                    m_height[CONFIGURATION_SIZE_MAX];
    bool                        m_mode[CONFIGURATION_MODE_MAX];
    bool                        m_flagCheck[CONFIGURATION_MODE_MAX];
    int                         m_modeValue[CONFIGURATION_VALUE_MAX];

    int                         m_yuvWidth[YUV_OUTPUT_PORT_ID_MAX];
    int                         m_yuvHeight[YUV_OUTPUT_PORT_ID_MAX];
    int                         m_yuvFormat[YUV_OUTPUT_PORT_ID_MAX];
    camera_pixel_size           m_yuvPixelSize[YUV_OUTPUT_PORT_ID_MAX];
    int                         m_yuvBufferCount[YUV_OUTPUT_PORT_ID_MAX];

    bool                        m_useDynamicBayer[CONFIG_MODE::MAX];

    bool                        m_yuvBufferStat;

    ExynosCameraCounter         m_frameSkipCounter;

    uint32_t                    m_minFps;
    uint32_t                    m_maxFps;
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    int                         m_fpsProperty;
#endif
    uint64_t                    m_exposureTimeCapture;
    int64_t                     m_exposureTime;
    int                         m_gain;
    int64_t                     m_ledPulseWidth;
    int64_t                     m_ledPulseDelay;
    int                         m_ledCurrent;
    int                         m_ledMaxTime;

    bool                        m_samsungCamera;

    bool                        m_fastEntrance;

    bool                        m_useFastenAeStable;
#ifdef USE_DUAL_CAMERA
    mutable Mutex               m_lockDualOperationModeLock;
    int32_t                     m_dualOperationModeLockCount;
    enum DUAL_OPERATION_MODE    m_dualOperationMode;
    enum DUAL_OPERATION_MODE    m_dualOperationModeReprocessing;
    bool                        m_dualHwSyncOn;
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int32_t                     m_fallbackOnCount;
    int32_t                     m_fallbackOffCount;
    int32_t                     m_dualSelectedCam;
    UniPluginDualCameraParam_t* m_fusionParam;
#endif
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    mutable Mutex               m_syncFrameStateInfoLock;

    uFusionCaptureState         m_syncFrameStateInfo;
    uFusionCaptureState         m_fusionCaptureInfo;
#endif

    bool                        m_isFactoryBin;

#ifdef SAMSUNG_OT
    ExynosRect2                 m_objectTrackingArea;
    int                         m_objectTrackingWeight;

    UniPluginFocusData_t        m_OTfocusData;
    mutable Mutex               m_OTfocusDataLock;
#endif
#ifdef SAMSUNG_IDDQD
    bool                        m_lensDirtyDetected;
#endif
#ifdef SAMSUNG_SW_VDIS
    int                         m_previewOffsetLeft;
    int                         m_previewOffsetTop;
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY) || defined(SAMSUNG_DUAL_PORTRAIT_BEAUTY)
    int                         m_beautyRetouchLevel;
#endif
#if defined(SAMSUNG_VIDEO_BEAUTY)
    int                         m_bokehRecordingHint;
#endif
#ifdef SAMSUNG_SSM
    ExynosRect2                 m_SSMRegion;
#endif
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    int                         m_bokehBlurStrength;
    int                         m_zoomInOutPhoto;
    int                         m_bokehPreviewState;
    int                         m_bokehCaptureState;
    int                         m_bokehProcessResult;

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    int                         m_dualCaptureFlag;
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */
    int                         m_repeatingRequestHint;
    float                       m_appliedZoomRatio;
    int                         m_temperature;
#ifdef SAMSUNG_TN_FEATURE
    int                         m_productColorInfo;
#endif
    struct camera2_shot         m_copyExifShot;
};

}; /* namespace android */

#endif
