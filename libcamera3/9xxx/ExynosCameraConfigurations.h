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
#include "ExynosCameraParameters.h"
#include "ExynosCameraSensorInfo.h"

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
#endif

/* refer to fimc_is_ex_mode on kernel side*/
enum EXTEND_SENSOR_MODE {
    EXTEND_SENSOR_MODE_NONE = 0,
    EXTEND_SENSOR_MODE_DRAM_TEST = 1,
    EXTEND_SENSOR_MODE_LIVE_FOCUS = 2,
};

enum CONFIGURATION_SIZE_TYPE {
    CONFIGURATION_YUV_SIZE,
    CONFIGURATION_MIN_YUV_SIZE,
    CONFIGURATION_MAX_YUV_SIZE,
    CONFIGURATION_PREVIEW_SIZE,
    CONFIGURATION_VIDEO_SIZE,
    CONFIGURATION_PICTURE_SIZE,
    CONFIGURATION_THUMBNAIL_SIZE,
    CONFIGURATION_THUMBNAIL_CB_SIZE,
    CONFIGURATION_DS_YUV_STALL_SIZE,
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
    CONFIGURATION_VIDEO_STABILIZATION_MODE,
    CONFIGURATION_HDR_MODE,
    CONFIGURATION_DYNAMIC_BAYER_MODE,
    CONFIGURATION_DVFS_LOCK_MODE,
    CONFIGURATION_MANUAL_AE_CONTROL_MODE,
    CONFIGURATION_HIFI_LLS_MODE,
#ifdef USE_DUAL_CAMERA
    CONFIGURATION_DUAL_MODE,
    CONFIGURATION_DUAL_PRE_MODE,
#endif
#ifdef SUPPORT_DEPTH_MAP
    CONFIGURATION_DEPTH_MAP_MODE,
#endif
    CONFIGURATION_GMV_MODE,
    CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE,
#ifdef SET_LLS_CAPTURE_SETFILE
    CONFIGURATION_LLS_CAPTURE_MODE,
#endif
    CONFIGURATION_MODE_MAX,
};

enum DYNAMIC_MODE_TYPE {
    DYNAMIC_UHD_RECORDING_MODE,
    DYNAMIC_HIGHSPEED_RECORDING_MODE,
#ifdef USE_DUAL_CAMERA
    DYNAMIC_DUAL_FORCE_SWITCHING,
#endif
    DYNAMIC_HIFI_CAPTURE_MODE,
    DYNAMIC_MODE_MAX,
};

enum CONFIGURATON_STATIC_VALUE_TYPE {
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
    CONFIGURATION_EXTEND_SENSOR_MODE,
    CONFIGURATION_MARKING_EXIF_FLASH,
    CONFIGURATION_FLASH_MODE,
    CONFIGURATION_BINNING_RATIO,
    CONFIGURATION_YUV_STALL_PORT,
    CONFIGURATION_YUV_STALL_PORT_USAGE,
    CONFIGURATION_BRIGHTNESS_VALUE,
    CONFIGURATION_CAPTURE_COUNT,
#ifdef LLS_CAPTURE
    CONFIGURATION_LLS_VALUE,
#endif
    CONFIGURATION_VALUE_MAX,
};

enum SUPPORTED_FUNCTION_TYPE {
    SUPPORTED_FUNCTION_GDC,
    SUPPORTED_FUNCTION_SERVICE_BATCH_MODE,
    SUPPORTED_FUNCTION_HIFILLS,
    SUPPORTED_FUNCTION_MAX,
};

class ExynosCameraParameters;

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
    int             getModeValue(enum CONFIGURATION_VALUE_TYPE type) const;

    /* Parameters configuration */
    void            setParameters(int cameraId, ExynosCameraParameters *parameters);
    ExynosCameraParameters *getParameters(int cameraId);

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

    status_t        checkYuvFormat(const int format, const int outputPortId);
    /* Sets the image pixel size for picture-related HW. */
    void            setYuvFormat(const int format, const int index);
    int             getYuvFormat(const int outputPortId);

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

    void                        setTryDualOperationMode(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getTryDualOperationMode(void);

    void                        setDualOperationMode(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getDualOperationMode(void);

    void                        setTryDualOperationModeReprocessing(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getTryDualOperationModeReprocessing(void);

    void                        setDualOperationModeReprocessing(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getDualOperationModeReprocessing(void);

    void                        setDualOperationModeLockCount(int32_t count);
    int32_t                     getDualOperationModeLockCount(void);
    void                        decreaseDualOperationModeLockCount(void);
    void                        setDualHwSyncOn(bool hwSyncOn);
    bool                        getDualHwSyncOn(void) const;
#endif

#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
    void                        setArcSoftCameraIndex(int cameraIndex) { m_arcSoftCameraIndex = cameraIndex; }
    int                         getArcSoftCameraIndex(void) { return m_arcSoftCameraIndex; }

    void                        setArcSoftViewRatio(float viewRatio) { m_arcSoftViewRatio = viewRatio; }
    float                       getArcSoftViewRatio(void) { return m_arcSoftViewRatio; }

    void                        setArcSoftImageShiftXY(int shiftX, int shiftY) { m_arcSoftImageShiftX = shiftX; m_arcSoftImageShiftY = shiftY; }
    void                        getArcSoftImageShiftXY(int *shiftX, int *shiftY) { *shiftX = m_arcSoftImageShiftX;  *shiftY = m_arcSoftImageShiftY; }

    void                        setDualCameraMaster3a(enum DUAL_OPERATION_MODE master3a) { m_arcSoftMaster3A = master3a; }
    enum DUAL_OPERATION_MODE    getDualCameraMaster3a(void) { return m_arcSoftMaster3A; }
#endif // USES_DUAL_CAMERA_SOLUTION_ARCSOFT

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
    void            setUseFastenAeStable(bool enable);
    bool            getUseFastenAeStable(void) const;
    status_t        setParameters(const CameraParameters& params);

private:
    /* Called by setParameters */
    status_t        m_checkFastEntrance(const CameraParameters& params);
    status_t        m_checkRecordingFps(const CameraParameters& params);
    status_t        m_checkFactorytest(const CameraParameters& params);
    status_t        m_checkOperationMode(const CameraParameters& params);
#ifdef USE_DUAL_CAMERA
    status_t        m_checkDualMode(const CameraParameters& params);
#endif

    status_t        m_checkExtSensorMode();
    status_t        m_checkVtMode(const CameraParameters& params);
    status_t        m_checkShootingMode(const CameraParameters& params);

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

    int             getAntibanding();

    void                setBokehBlurStrength(int bokehBlurStrength);
    int                 getBokehBlurStrength(void);

    void                setRepeatingRequestHint(int repeatingRequestHint);
    int                 getRepeatingRequestHint(void);

    void			setTemperature(int temperature);
    int 			getTemperature(void);

private:

private:
    int                         m_scenario;
    static int                  m_staticValue[CONFIGURATION_STATIC_VALUE_MAX];
    struct ExynosConfigInfo     *m_exynosconfig;
    struct CameraMetaParameters m_metaParameters;
    ExynosCameraParameters     *m_parameters[CAMERA_ID_MAX];
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
    int                         m_yuvBufferCount[YUV_OUTPUT_PORT_ID_MAX];

    bool                        m_useDynamicBayer[CONFIG_MODE::MAX];

    bool                        m_yuvBufferStat;

    ExynosCameraCounter         m_frameSkipCounter;

    uint32_t                    m_minFps;
    uint32_t                    m_maxFps;
    uint64_t                    m_exposureTimeCapture;
    int64_t                     m_exposureTime;
    int                         m_gain;
    int64_t                     m_ledPulseWidth;
    int64_t                     m_ledPulseDelay;
    int                         m_ledCurrent;
    int                         m_ledMaxTime;

    bool                        m_fastEntrance;

    bool                        m_useFastenAeStable;
#ifdef USE_DUAL_CAMERA
    enum DUAL_OPERATION_MODE    m_tryDualOperationMode;
    enum DUAL_OPERATION_MODE    m_tryDualOperationModeReprocessing;

    mutable Mutex               m_lockDualOperationModeLock;
    int32_t                     m_dualOperationModeLockCount;
    enum DUAL_OPERATION_MODE    m_dualOperationMode;
    enum DUAL_OPERATION_MODE    m_dualOperationModeReprocessing;
    bool                        m_dualHwSyncOn;
#endif

    bool                        m_isFactoryBin;

    int                         m_bokehBlurStrength;

    int                         m_repeatingRequestHint;
    float                       m_appliedZoomRatio;
    int                         m_temperature;

#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
private:
    int                         m_arcSoftCameraIndex;
    float                       m_arcSoftViewRatio;
    int                         m_arcSoftImageShiftX;
    int                         m_arcSoftImageShiftY;

    enum DUAL_OPERATION_MODE    m_arcSoftMaster3A;
#endif
};

}; /* namespace android */

#endif
