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

#ifndef EXYNOS_CAMERA_PARAMETERS_H
#define EXYNOS_CAMERA_PARAMETERS_H

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
#include "ExynosCameraConfig.h"
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
#endif
#ifdef SAMSUNG_HYPER_MOTION
#include "SecCameraHyperMotion.h"
#endif /*SAMSUNG_HYPER_MOTION*/
#ifdef SAMSUNG_OIS
#include "ExynosCameraNode.h"
#endif
#ifdef SAMSUNG_DNG
#include "SecCameraDng.h"
#include "SecCameraDngThumbnail.h"
#endif

#include "ExynosCameraSensorInfo.h"
#include "SecCameraUtil.h"

#ifdef SAMSUNG_UNI_API
#include "uni_api_wrapper.h"
#endif

#define FW_CUSTOM_OFFSET (1)
#define V4L2_FOURCC_LENGTH 5
#define IS_WBLEVEL_DEFAULT (4)

#define GPS_PROCESSING_METHOD_SIZE 32

#define EXYNOS_CONFIG_DEFINED (-1)
#define EXYNOS_CONFIG_NOTDEFINED (-2)

#define STATE_REG_BINNING_MODE          (1<<28)
#define STATE_REG_MANUAL_ISO            (1<<26)
#define STATE_REG_LONG_CAPTURE          (1<<24)
#define STATE_REG_SHARPEN_SINGLE        (1<<22)
#define STATE_REG_RTHDR_AUTO            (1<<20)
#define STATE_REG_NEED_LLS              (1<<18)
#define STATE_REG_ZOOM_INDOOR           (1<<16)
#define STATE_REG_ZOOM_OUTDOOR          (1<<14)
#define STATE_REG_ZOOM                  (1<<12)
#define STATE_REG_RTHDR_ON              (1<<10)
#define STATE_REG_RECORDINGHINT         (1<<8)
#define STATE_REG_DUAL_RECORDINGHINT    (1<<6)
#define STATE_REG_UHD_RECORDING         (1<<4)
#define STATE_REG_DUAL_MODE             (1<<2)
#define STATE_REG_FLAG_REPROCESSING     (1)

#define STATE_STILL_PREVIEW                     (0)
#define STATE_STILL_PREVIEW_WDR_ON              (STATE_REG_RTHDR_ON)
#define STATE_STILL_PREVIEW_WDR_AUTO            (STATE_REG_RTHDR_AUTO)

#define STATE_STILL_CAPTURE                     (STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_ZOOM                (STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_ZOOM_OUTDOOR        (STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_OUTDOOR)
#define STATE_STILL_CAPTURE_ZOOM_INDOOR         (STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_INDOOR)
#define STATE_VIDEO_CAPTURE                     (STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_STILL_CAPTURE_LLS                 (STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)
#define STATE_STILL_CAPTURE_LONG                (STATE_REG_FLAG_REPROCESSING|STATE_REG_LONG_CAPTURE)
#define STATE_STILL_CAPTURE_MANUAL_ISO          (STATE_REG_FLAG_REPROCESSING|STATE_REG_MANUAL_ISO)
#define STATE_STILL_CAPTURE_LLS_ZOOM            (STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)

#define STATE_STILL_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM            (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR    (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_OUTDOOR)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR     (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_INDOOR)
#define STATE_STILL_CAPTURE_WDR_ON_LLS_ZOOM        (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)
#define STATE_VIDEO_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_VIDEO_CAPTURE_WDR_ON_LLS             (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)

#define STATE_STILL_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM            (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR    (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_OUTDOOR)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR     (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_INDOOR)
#define STATE_VIDEO_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_STILL_CAPTURE_WDR_AUTO_LLS             (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)
#define STATE_STILL_CAPTURE_WDR_AUTO_LLS_ZOOM        (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_AUTO_SHARPEN         (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_SHARPEN_SINGLE)

#define STATE_VIDEO                             (STATE_REG_RECORDINGHINT)
#define STATE_VIDEO_WDR_ON                      (STATE_REG_RECORDINGHINT|STATE_REG_RTHDR_ON)
#define STATE_VIDEO_WDR_AUTO                    (STATE_REG_RECORDINGHINT|STATE_REG_RTHDR_AUTO)

#define STATE_DUAL_VIDEO                        (STATE_REG_DUAL_RECORDINGHINT|STATE_REG_DUAL_MODE)
#define STATE_DUAL_VIDEO_CAPTURE                (STATE_REG_DUAL_RECORDINGHINT|STATE_REG_DUAL_MODE|STATE_REG_FLAG_REPROCESSING)
#define STATE_DUAL_STILL_PREVIEW                (STATE_REG_DUAL_MODE)
#define STATE_DUAL_STILL_CAPTURE                (STATE_REG_DUAL_MODE|STATE_REG_FLAG_REPROCESSING)

#define STATE_UHD_PREVIEW                       (STATE_REG_UHD_RECORDING)
#define STATE_UHD_PREVIEW_WDR_ON                (STATE_REG_UHD_RECORDING|STATE_REG_RTHDR_ON)
#define STATE_UHD_PREVIEW_WDR_AUTO              (STATE_REG_UHD_RECORDING|STATE_REG_RTHDR_AUTO)
#define STATE_UHD_VIDEO                         (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT)
#define STATE_UHD_VIDEO_WDR_ON                  (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT|STATE_REG_RTHDR_ON)
#define STATE_UHD_VIDEO_WDR_AUTO                (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT|STATE_REG_RTHDR_AUTO)

#define STATE_UHD_PREVIEW_CAPTURE               (STATE_REG_UHD_RECORDING|STATE_REG_FLAG_REPROCESSING)
#define STATE_UHD_PREVIEW_CAPTURE_WDR_ON        (STATE_REG_UHD_RECORDING|STATE_REG_FLAG_REPROCESSING|STATE_REG_RTHDR_ON)
#define STATE_UHD_PREVIEW_CAPTURE_WDR_AUTO      (STATE_REG_UHD_RECORDING|STATE_REG_FLAG_REPROCESSING|STATE_REG_RTHDR_AUTO)
#define STATE_UHD_VIDEO_CAPTURE                 (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT|STATE_REG_FLAG_REPROCESSING)
#define STATE_UHD_VIDEO_CAPTURE_WDR_ON          (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT|STATE_REG_FLAG_REPROCESSING|STATE_REG_RTHDR_ON)
#define STATE_UHD_VIDEO_CAPTURE_WDR_AUTO        (STATE_REG_UHD_RECORDING|STATE_REG_RECORDINGHINT|STATE_REG_FLAG_REPROCESSING|STATE_REG_RTHDR_AUTO)

#define STATE_STILL_BINNING_PREVIEW              (STATE_REG_BINNING_MODE)
#define STATE_VIDEO_BINNING                       (STATE_REG_RECORDINGHINT|STATE_REG_BINNING_MODE)
#define STATE_DUAL_STILL_BINING_PREVIEW         (STATE_REG_DUAL_MODE|STATE_REG_BINNING_MODE)
#define STATE_DUAL_VIDEO_BINNING                 (STATE_REG_DUAL_RECORDINGHINT|STATE_REG_DUAL_MODE|STATE_REG_BINNING_MODE)

namespace android {

using namespace std;
using ::android::hardware::camera::common::V1_0::helper::CameraParameters;
using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

namespace CONFIG_MODE {
    enum MODE {
        NORMAL        = 0x00,
        HIGHSPEED_60,
        HIGHSPEED_120,
        HIGHSPEED_240,
        MAX
    };
};

enum YuvStallUsage {
    YUV_STALL_USAGE_DSCALED = 0,
    YUV_STALL_USAGE_PICTURE = 1
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
    uint32_t reprocessing_bayer_hold_count;
    uint32_t num_request_raw_buffers;
    uint32_t num_request_preview_buffers;
    uint32_t num_request_callback_buffers;
    uint32_t num_request_video_buffers;
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
    float m_zoomRatio;
#ifdef SAMSUNG_OT
    bool m_startObjectTracking;
#endif
};

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

class ExynosCameraParameters : public ExynosCameraObject {
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

    enum critical_section_type {
        /* Order is VERY important.
           It indicates the order of entering critical section on run-time.
         */
        CRITICAL_SECTION_TYPE_START,
        CRITICAL_SECTION_TYPE_HWFC = CRITICAL_SECTION_TYPE_START,
        CRITICAL_SECTION_TYPE_VOTF,
        CRITICAL_SECTION_TYPE_END,
    };

    /* Constructor */
    ExynosCameraParameters(int cameraId);

    /* Destructor */
    virtual ~ExynosCameraParameters();

    void            setDefaultCameraInfo(void);

public:
    status_t        checkPictureSize(int pictureW, int pctureH);
    status_t        checkJpegQuality(int quality);
    status_t        checkThumbnailSize(int thumbnailW, int thumbnailH);
    status_t        checkThumbnailQuality(int quality);
    status_t        checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps);

    status_t        resetYuvSizeRatioId(void);
    status_t        checkYuvSize(const int width, const int height, const int outputPortId, bool reprocessing = false);
    status_t        checkHwYuvSize(const int width, const int height, const int outputPortId, bool reprocessing = false);
    status_t        checkYuvFormat(const int format, const int outputPortId);
#ifdef HAL3_YUVSIZE_BASED_BDS
    status_t        initYuvSizes();
#endif
    void            resetMinYuvSize();
    void            resetMaxYuvSize();
    status_t        getMinYuvSize(int* w, int* h)   const;
    status_t        getMaxYuvSize(int* w, int* h)   const;

    void            resetMaxHwYuvSize();
    status_t        getMaxHwYuvSize(int* w, int* h)   const;

    void            getYuvSize(int *width, int *height, int outputPortId);
    int             getYuvFormat(const int outputPortId);
    void            resetYuvSize(void);

    void            getHwYuvSize(int *width, int *height, int outputPortId);
    void            resetHwYuvSize(void);

    void            setPreviewPortId(int outputPortId);
    bool            isPreviewPortId(int outputPortId);
    int             getPreviewPortId(void);

    void            setRecordingPortId(int outputPortId);
    bool            isRecordingPortId(int outputPortId);
    int             getRecordingPortId(void);

    void            setYuvOutPortId(enum pipeline pipeId, int outputPortId);
    int             getYuvOutPortId(enum pipeline pipeId);

    void            setThumbnailCbSize(int w, int h);
    void            getThumbnailCbSize(int *w, int *h);
    void            resetThumbnailCbSize();

    void            getDScaledYuvStallSize(int *w, int *h);

    status_t        getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom = true);
    status_t        getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom = true);
    status_t        getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        getPictureBdsSize(ExynosRect *dstRect);
    status_t        getPreviewYuvCropSize(ExynosRect *yuvCropSize);
    status_t        getPictureYuvCropSize(ExynosRect *yuvCropSize);
    status_t        getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH, int index);
    status_t        getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH, int index);
    status_t        getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH, int index);

    status_t        calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);

    void            setRestartStream(bool restart);
    bool            getRestartStream(void);

private:
    /* Sets the dimensions for pictures. */
    void            m_setPictureSize(int w, int h);
    /* Sets the image format for picture-related HW. */
    void            m_setHwPictureFormat(int colorFormat);
    /* Sets the image pixel size for picture-related HW. */
    void            m_setHwPicturePixelSize(camera_pixel_size pixelSize);
    void            m_setYuvSize(const int width, const int height, const int index);
    void            m_setYuvFormat(const int format, const int index);
    void            m_setHwYuvSize(const int width, const int height, const int index);

    /* Sets the dimensions for Sesnor-related HW. */
    void            m_setHwSensorSize(int w, int h);
    /* Sets the dimensions for picture-related HW. */
    void            m_setHwPictureSize(int w, int h);
    /* Sets HW Bayer Crop Size */
    void            m_setHwBayerCropRegion(int w, int h, int x, int y);
    /* Sets Jpeg quality of captured picture. */
    void            m_setJpegQuality(int quality);
    /* Sets the quality of the EXIF thumbnail in Jpeg picture. */
    void            m_setThumbnailQuality(int quality);
    /* Sets the dimensions for EXIF thumbnail in Jpeg picture. */
    void            m_setThumbnailSize(int w, int h);
    /* Sets the frame rate range for preview. */
    void            m_setPreviewFpsRange(uint32_t min, uint32_t max);
    /* Sets Bayer Crop Region */
    status_t        m_setParamCropRegion(int srcW, int srcH, int dstW, int dstH);

/*
 * Additional API.
 */
    /* Sets ODC */
    void            m_setOdcMode(bool toggle);
/*
 * Vendor specific APIs
 */
    /* Sets Intelligent mode */
    void            m_setIntelligentMode(int intelligentMode);
    void            m_setVisionMode(bool vision);
    void            m_setVisionModeFps(int fps);
    void            m_setVisionModeAeTarget(int ae);

    /* Sets VT mode */
    void            m_setVtMode(int vtMode);

    void            m_setHighSpeedRecording(bool highSpeed);
    status_t        m_setImageUniqueId(const char *uniqueId);

/*
 * Others
 */
    void            m_setExifFixedAttribute(void);
    status_t        m_adjustPreviewFpsRange(uint32_t &newMinFps, uint32_t &newMaxFps);

public:
#ifdef SAMSUNG_DOF
    /* Sets Lens Position */
    void            m_setLensPos(int pos);
#endif
#ifdef SAMSUNG_HRM
    void            m_setHRM(int ir_data, int flicker_data, int status);
    void            m_setHRM_Hint(bool flag);
    bool            m_getHRM_Hint(void);
#endif
#ifdef SAMSUNG_LIGHT_IR
    void            m_setLight_IR(SensorListenerEvent_t data);
    void            m_setLight_IR_Hint(bool flag);
    bool            m_getLight_IR_Hint(void);
#endif
#ifdef SAMSUNG_GYRO
    void            m_setGyro(SensorListenerEvent_t data);
    void            m_setGyroHint(bool flag);
    bool            m_getGyroHint(void);
    SensorListenerEvent_t            *getGyroData(void);
#endif
#ifdef SAMSUNG_ACCELEROMETER
    void            m_setAccelerometer(SensorListenerEvent_t data);
    void            m_setAccelerometerHint(bool flag);
    bool            m_getAccelerometerHint(void);
#endif
#ifdef SAMSUNG_ROTATION
    void            m_setRotationHint(bool flag);
    bool            m_getRotationHint(void);
#endif
    /* Returns the image format for FLITE/3AC/3AP bayer */
    int             getBayerFormat(int pipeId);
    /* Returns the dimensions setting for preview pictures. */
    void            getPreviewSize(int *w, int *h);
    /* Returns the dimension setting for pictures. */
    void            getPictureSize(int *w, int *h);
    /* Returns the image format for picture-related HW. */
    int             getHwPictureFormat(void);
    /* Returns the image pixel size for picture-related HW. */
    camera_pixel_size getHwPicturePixelSize(void);
    /* Sets video's width, height */
    void            setVideoSize(int w, int h);
    /* Gets video's width, height */
    void            getVideoSize(int *w, int *h);
    /* Gets video's color format */
    int             getVideoFormat(void);
    /* Gets the supported sensor sizes. */
    void            getMaxSensorSize(int *w, int *h);
    /* Gets the supported sensor margin. */
    void            getSensorMargin(int *w, int *h);
    /* Gets the supported preview sizes. */
    void            getMaxPreviewSize(int *w, int *h);
    /* Gets the supported picture sizes. */
    void            getMaxPictureSize(int *w, int *h);

    /* Returns the dimensions setting for preview-related HW. */
    void            getHwSensorSize(int *w, int *h);
    /* Returns the dimensions setting for preview-related HW. */
    void            getHwPreviewSize(int *w, int *h);
    /* Returns the image format for preview-related HW. */
    int             getHwPreviewFormat(void);
    /* Returns the dimension setting for picture-related HW. */
    void            getHwPictureSize(int *w, int *h);
    /* Returns HW Bayer Crop Size */
    void            getHwBayerCropRegion(int *w, int *h, int *x, int *y);
    /* Returns the quality setting for the JPEG picture. */
    int             getJpegQuality(void);
    /* Returns the quality setting for the EXIF thumbnail in Jpeg picture. */
    int             getThumbnailQuality(void);
    /* Returns the dimensions for EXIF thumbnail in Jpeg picture. */
    void            getThumbnailSize(int *w, int *h);
    /* Returns the max size for EXIF thumbnail in Jpeg picture. */
    void            getMaxThumbnailSize(int *w, int *h);
    /* Returns the current minimum and maximum preview fps. */
    void            getPreviewFpsRange(uint32_t *min, uint32_t *max);
    /* Gets the current state of video stabilization. */
    bool            getVideoStabilization(void);
    /* Set the current crop region info */
    status_t        setCropRegion(int x, int y, int w, int h);
    /* Returns the recording mode hint. */
    bool            getRecordingHint(void);
    /* Sets recording mode hint. */
    void            setRecordingHint(bool hint);
    /* Sets the YSUM recording mode */
    void            setYsumRecordingMode(bool);
    /* Returns the YSUM recording mode */
    bool            getYsumRecordingMode(void);

    void            getHwVraInputSize(int *w, int *h, int dsInputPortId);
    int             getHwVraInputFormat(void);

    void            setDsInputPortId(int dsInputPortId, bool isReprocessing);
    int             getDsInputPortId(bool isReprocessing);

    void            updateYsumPordId(struct camera2_shot_ext *shot_ext);
    status_t		updateYsumBuffer(struct ysum_data *ysumdata, ExynosCameraBuffer *dstBuf);

/*
 * Additional API.
 */

    /* Sets ExposureTime for capture */
    void            setCaptureExposureTime(uint64_t exposureTime);
    /* Gets ExposureTime for capture */
    uint64_t        getCaptureExposureTime(void);
    /* Gets LongExposureTime for capture */
    uint64_t        getLongExposureTime(void);
    /* Gets LongExposureShotCount for capture */
    int32_t         getLongExposureShotCount(void);

    /* Gets WDR */
    bool            getHdrMode(void);
    /* Gets ODC */
    bool            getOdcMode(void);
    /* Gets Shot mode */
    int             getShotMode(void);
    /* Gets GDC enable case or not */
    bool            getGDCEnabledMode(void);
    /* Get sensor control frame delay count */
    int             getSensorControlDelay(void);

#ifdef SAMSUNG_PAF
     /* Gets PAF */
    enum companion_paf_mode            getPaf(void);
#endif
#ifdef SAMSUNG_RTHDR
     /* Gets RT HDR */
    enum companion_wdr_mode            getRTHdr(void);
    void                               setRTHdr(enum companion_wdr_mode rtHdrMode);
#endif

/*
 * Vendor specific APIs
 */

    /* Gets Intelligent mode */
    int             getIntelligentMode(void);
    bool            getVisionMode(void);
    int             getVisionModeFps(void);
    int             getVisionModeAeTarget(void);

    bool            getHWVdisMode(void);
    int             getHWVdisFormat(void);

    void            getYuvVendorSize(int *width, int *height, int outputPortId, ExynosRect ispSize);

    /* Gets VT mode */
    int             getVtMode(void);

    /* Set/Get Dual mode */
    void            setPIPMode(bool toggle);
    bool            getPIPMode(void);

#ifdef USE_DUAL_CAMERA
    void            setDualMode(bool enabled);
    bool            getDualMode(void);
    enum DUAL_PREVIEW_MODE      getDualPreviewMode(void);
    enum DUAL_REPROCESSING_MODE getDualReprocessingMode(void);
    void                        setDualOperationMode(enum DUAL_OPERATION_MODE mode);
    enum DUAL_OPERATION_MODE    getDualOperationMode(void);
    bool            isSupportMasterSensorStandby(void);
    bool            isSupportSlaveSensorStandby(void);
#endif
    uint32_t        getSensorStandbyDelay(void);

    void            setProMode(bool proMode);

    /* Returns the dual recording mode hint. */
    bool            getPIPRecordingHint(void);

    bool            getHighSpeedRecording(void);
#ifdef USE_BINNING_MODE
    int *           getBinningSizeTable(void);
#endif
    /* Gets ImageUniqueId */
    const char     *getImageUniqueId(void);
#ifdef SAMSUNG_TN_FEATURE
    void            setImageUniqueId(char *uniqueId);
#endif

    void            setFlipHorizontal(int val);
    int             getFlipHorizontal(void);
    void            setFlipVertical(int val);
    int             getFlipVertical(void);

    /* Gets Burst mode */
    int             getSeriesShotCount(void);
    /* Gets Low Light Shot */
    int             getLLS(struct camera2_shot_ext *shot);
    /* Gets Low Light A */
    /* Sets the device orientation angle in degrees to camera FW for FD scanning property. */
    bool            setDeviceOrientation(int orientation);
    /* Gets the device orientation angle in degrees . */
    int             getDeviceOrientation(void);
    /* Gets the FD orientation angle in degrees . */
    int             getFdOrientation(void);

/*
 * Static info
 */
    /* Gets max zoom ratio */
    float           getMaxZoomRatio(void);
    /* Gets current zoom ratio */
    float           getZoomRatio(void);
    /* Sets current zoom ratio */
    void            setZoomRatio(float zoomRatio);

    /* Gets FocalLengthIn35mmFilm */
    int             getFocalLengthIn35mmFilm(void);

    status_t        getFixedExifInfo(exif_attribute_t *exifInfo);
    void            setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                            ExynosRect          *PictureRect,
                                            ExynosRect          *thumbnailRect,
                                            camera2_shot_t      *shot,
                                            bool                useDebugInfo2 = false);

    debug_attribute_t *getDebugAttribute(void);
    debug_attribute_t *getDebug2Attribute(void);

#ifdef DEBUG_RAWDUMP
    bool           checkBayerDumpEnable(void);
#endif/* DEBUG_RAWDUMP */
#ifdef USE_BINNING_MODE
    int             getBinningMode(void);
#endif /* USE_BINNING_MODE */

public:
    /* For Vendor */
    bool            getSamsungCamera(void);
    void            setSamsungCamera(bool flag);
    status_t        checkSamsungCamera(const CameraParameters& params);
    int             getRecordingFps(void);
    void            setRecordingFps(int fps);
    status_t        checkRecordingFps(const CameraParameters& params);
    status_t        setParameters(const CameraParameters& params);
    status_t        checkDualMode(const CameraParameters& params);
    status_t        checkIntelligentMode(const CameraParameters& params);
    status_t        checkVtMode(const CameraParameters& params);
    status_t        checkFactorytest(const CameraParameters& params);
    bool            getFactorytest(void);

    void            updateHwSensorSize(void);
    void            updateBinningScaleRatio(void);

    status_t        duplicateCtrlMetadata(void *buf);

    bool            getUHDRecordingMode(void);
    status_t        init(void);
    void            vendorSpecificConstructor(int cameraId);
    bool            getSWVdisMode(void);
    bool            getHyperMotionMode(void);
#ifdef SAMSUNG_STR_PREVIEW
    bool            getSTRPreviewEnable(void);
#endif

#ifdef SAMSUNG_SW_VDIS
    void            getSWVdisYuvSize(int w, int h, int *newW, int *newH);
    void            getSWVdisAdjustYuvSize(int *width, int *height);
    void            setSWVdisPreviewOffset(int left, int top);
    void            getSWVdisPreviewOffset(int *left, int *top);
    bool            isSWVdisOnPreview(void);
    void            setSWVdisVideoIndex(int index);
    int             getSWVdisVideoIndex(void);
#endif

private:
    bool            m_isSupportedYuvSize(const int width, const int height, const int outputPortId, int *ratio);
    bool            m_isSupportedPictureSize(const int width, const int height);
    status_t        m_getSizeListIndex(int (*sizelist)[SIZE_OF_LUT], int listMaxSize, int ratio, int *index);
    status_t        m_getPictureSizeList(int *sizeList);
    status_t        m_getPreviewSizeList(int *sizeList);
    bool            m_isSupportedFullSizePicture(void);
    void            m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH);

    void            m_initMetadata(void);
    bool            m_isUHDRecordingMode(void);

    status_t        checkSWVdisMode(const CameraParameters& params);
    status_t        checkHyperMotionMode(const CameraParameters& params);
/*
 * Vendor specific adjust function
 */
    status_t        m_vendorInit(void);
    void            m_vendorSpecificDestructor(void);
    status_t        m_getPreviewBdsSize(ExynosRect *dstRect);
    status_t        m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                        int *newHwPictureW, int *newHwPictureH);
    void            m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH);
    void            m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void            m_getCropRegion(int *x, int *y, int *w, int *h);
#ifdef USE_CSC_FEATURE
    bool            m_isLatinOpenCSC();
    int             m_getAntiBandingFromLatinMCC();
#endif

    status_t        m_setBinningScaleRatio(int ratio);
    void            m_setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                              ExynosRect          *PictureRect,
                                              ExynosRect          *thumbnailRect,
                                              camera2_shot_t      *shot,
                                              bool                useDebugInfo2 = false);

    void            m_getV4l2Name(char* colorName, size_t length, int colorFormat);
    bool            m_getUseFullSizeLUT(void);

    /* H/W Chain Scenario Infos */
    enum HW_CONNECTION_MODE         m_getFlite3aaOtf(void);
    enum HW_CONNECTION_MODE         m_get3aaIspOtf(void);
    enum HW_CONNECTION_MODE         m_getIspMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getMcscVraOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessing3aaIspOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingIspMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingMcscVraOtf(void);
#ifdef USE_DUAL_CAMERA
    enum HW_CONNECTION_MODE         m_getIspDcpOtf(void);
    enum HW_CONNECTION_MODE         m_getDcpMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingIspDcpOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingDcpMcscOtf(void);
#endif

public:
    void                setBurstShotFps(int value);
    int                 getBurstShotFps(void);
    void                setBurstShotTargetFps(int value);
    int                 getBurstShotTargetFps(void);
    int                 getLightCondition(void);

    ExynosCameraActivityControl *getActivityControl(void);

    void                getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void                setSetfileYuvRange(void);
    void                setSetfileYuvRange(bool flagReprocessing, int setfile, int yuvRange);
    status_t            checkSetfileYuvRange(void);

    void                setUseSensorPackedBayer(bool enable);
    bool                getUsePureBayerReprocessing(void);
    int32_t             getReprocessingBayerMode(void);
    void                setUseDynamicBayer(bool enable);
    bool                getUseDynamicBayer(void);
    bool                getUseDynamicBayer60FpsVideoSnapShot(void);
    bool                getUseDynamicBayer120FpsVideoSnapShot(void);
    bool                getUseDynamicBayer240FpsVideoSnapShot(void);

    void                setUseFastenAeStable(bool enable);
    bool                getUseFastenAeStable(void);
    bool                isFastenAeStableEnable(void);

    uint32_t            getBinningScaleRatio(void);
    /* Sets the dimensions for Sesnor-related BNS. */
    void                setBnsSize(int w, int h);
    /* Gets the dimensions for Sesnor-related BNS. */
    void                getBnsSize(int *w, int *h);
    /* Gets BatchSize for HFR */
    int                 getBatchSize(enum pipeline pipeId);
    /* Check to use Service Batch Mode */
    bool                useServiceBatchMode(void);
    /* Decide to enter critical section */
    bool                isCriticalSection(enum pipeline pipeId,
                                          enum critical_section_type type);

    struct ExynosCameraSensorInfoBase *getSensorStaticInfo();

    int32_t             getYuvStreamMaxNum(void);
    int32_t             getInputStreamMaxNum(void);

#ifdef SAMSUNG_DOF
    int                 getFocusLensPos(void);
    void                setFocusLensPos(int pos);
#endif

    int                 getSeriesShotMode(void);

    int                 getMultiCaptureMode(void);
    void                setMultiCaptureMode(int captureMode);

    void                setExposureTime(int64_t shutterSpeed);
    int64_t             getExposureTime(void);
    void                setGain(int gain);
    int                 getGain(void);
    void                setLedPulseWidth(int64_t ledPulseWidth);
    int64_t             getLedPulseWidth(void);
    void                setLedPulseDelay(int64_t ledPulseDelay);
    int64_t             getLedPulseDelay(void);
    void                setLedCurrent(int ledCurrent);
    int                 getLedCurrent(void);
    void                setLedMaxTime(int ledMaxTime);
    int                 getLedMaxTime(void);

    void                setDvfsLock(bool lock);
    bool                getDvfsLock(void);

    bool                getSensorOTFSupported(void);
    bool                isReprocessing(void);
    bool                isSccCapture(void);

    /* True if private reprocessing or YUV reprocessing is supported */
    bool                isSupportZSLInput(void);

    enum HW_CONNECTION_MODE getHwConnectionMode(
                                enum pipeline srcPipeId,
                                enum pipeline dstPipeId);

    bool                isUse3aaInputCrop(void);
    bool                isUseIspInputCrop(void);
    bool                isUseMcscInputCrop(void);
    bool                isUseReprocessing3aaInputCrop(void);
    bool                isUseReprocessingIspInputCrop(void);
    bool                isUseReprocessingMcscInputCrop(void);
    bool                isUseEarlyFrameReturn(void);
    bool                isUseHWFC(void);
    bool                isUseThumbnailHWFC(void) {return true;};
    bool                isHWFCOnDemand(void);

    bool                setConfig(struct ExynosConfigInfo* config);
    struct ExynosConfigInfo* getConfig();

    bool                setConfigMode(uint32_t mode);
    int                 getConfigMode();
    /* Sets Shot mode */
    void                setShotMode(int shotMode);
#ifdef LLS_CAPTURE
    void                setLLSOn(uint32_t enable);
    bool                getLLSOn(void);
    void                setLLSValue(int value);
    int                 getLLSValue(void);
#ifdef SET_LLS_CAPTURE_SETFILE
    void                setLLSCaptureOn(bool enable);
    int                 getLLSCaptureOn();
#endif
#endif
#ifdef SAMSUNG_HIFI_CAPTURE
    bool                getHiFiCatureEnable(void);
#endif
    void                setManualAeControl(bool isManualAeControl);
    bool                getManualAeControl(void);
    void                setFlashMode(int flashMode);
    int                 getFlashMode(void);
    void                updateMetaParameter(struct CameraMetaParameters *metaParameters);
#ifdef OIS_CAPTURE
    void                checkOISCaptureMode(int multiCaptureMode);
    void                setOISCaptureModeOn(bool enable);
    bool                getOISCaptureModeOn(void);
#endif
    void                setDynamicPickCaptureModeOn(bool enable);
    bool                getDynamicPickCaptureModeOn(void);
#ifdef RAWDUMP_CAPTURE
    void                setRawCaptureModeOn(bool enable);
    bool                getRawCaptureModeOn(void);
#endif
    status_t            setMarkingOfExifFlash(int flag);
    int                 getMarkingOfExifFlash(void);

    status_t            setYuvBufferCount(const int count, const int outputPortId);
    int                 getYuvBufferCount(const int outputPortId);
    void                resetYuvBufferCount(void);

    void                setHighSpeedMode(uint32_t mode);
    int                 getMaxHighSpeedFps(void);

//Added
    int                 getHDRDelay(void) { return HDR_DELAY; }
    int                 getReprocessingBayerHoldCount(void) { return REPROCESSING_BAYER_HOLD_COUNT; }
    int                 getPerFrameControlPipe(void) {return PERFRAME_CONTROL_PIPE; }
    int                 getPerFrameControlReprocessingPipe(void) {return PERFRAME_CONTROL_REPROCESSING_PIPE; }
    int                 getPerFrameInfo3AA(void) { return PERFRAME_INFO_3AA; };
    int                 getPerFrameInfoIsp(void) { return PERFRAME_INFO_ISP; };
    int                 getPerFrameInfoReprocessingPure3AA(void) { return PERFRAME_INFO_PURE_REPROCESSING_3AA; }
    int                 getPerFrameInfoReprocessingPureIsp(void) { return PERFRAME_INFO_PURE_REPROCESSING_ISP; }
    int                 getScalerNodeNumPicture(void) { return PICTURE_GSC_NODE_NUM;}

    bool                needGSCForCapture(int camId) { return (camId == CAMERA_ID_BACK) ? USE_GSC_FOR_CAPTURE_BACK : USE_GSC_FOR_CAPTURE_FRONT; }
    bool                getSetFileCtlMode(void);
    bool                getSetFileCtl3AA(void);
    bool                getSetFileCtlISP(void);
    void                setYuvStallPortUsage(int usage);
    int                 getYuvStallPortUsage(void);
    void                resetYuvStallPort(void);
    void                setYuvStallPort(int port);
    int                 getYuvStallPort(void);

#ifdef SUPPORT_DEPTH_MAP
    status_t    getDepthMapSize(int *depthMapW, int *depthMapH);
    void        setDepthMapSize(int depthMapW, int depthMapH);
    bool        getUseDepthMap(void);
    void        setUseDepthMap(bool useDepthMap);
    bool        getDepthCallbackOnPreview(void) {return 0;};
    bool        getDepthCallbackOnCapture(void) {return 0;};
    bool        isDepthMapSupported(void);
#endif
#ifdef SAMSUNG_BD
    void                setBlurInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SAMSUNG_UTC_TS
    void                setUTCInfo();
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    void                setLDCaptureMode(int LDMode);
    int                 getLDCaptureMode(void);
    int                 getLDCaptureCount(void);
    void                setLDCaptureCount(int count);
    void                checkLDCaptureMode(void);
    void                setLDCaptureLLSValue(uint32_t lls);
    int                 getLDCaptureLLSValue(void);
    void                setLLSdebugInfo(unsigned char *data, unsigned int size);
#endif

#ifdef SAMSUNG_LENS_DC
    bool                getLensDCEnable(void);
#endif

#ifdef SAMSUNG_STR_CAPTURE
    void                checkSTRCaptureMode(bool hasCaptureStream);
    void                setSTRCaptureEnable(bool enable);
    bool                getSTRCaptureEnable(void);
    void                setSTRdebugInfo(unsigned char *data, unsigned int size);
#endif

    int                 getAntibanding();
    void                setUseFullSizeLUT(bool enable);

    bool                checkFaceDetectMeta(struct camera2_shot_ext *shot_ext);

#ifdef SAMSUNG_OT
    void                setObjectTrackingEnable(bool startObjectTracking);
    bool                getObjectTrackingEnable(void);
    void                setObjectTrackingAreas(int validFocusArea, ExynosRect2 area, int weight);
    int                 getObjectTrackingAreas(int* validFocusArea, ExynosRect2* areas, int* weights);
#endif

#ifdef SAMSUNG_HLV
    bool                getHLVEnable(bool recordingEnabled);
#endif
#ifdef SAMSUNG_IDDQD
    bool                getIDDQDEnable(void);
    void                setIDDQDresult(bool isdetected);
    bool                getIDDQDresult(void);
#endif
    void                m_setSWVdisMode(bool swVdis);
    void                m_setHyperMotionMode(bool enable);

    void                setTransientActionMode(int transientAction);
    int                 getTransientActionMode();

    bool                getHfdMode(void);
#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo* getSsrmCameraInfo(void) {return &m_ssrmCameraInfo; }
#endif

private:
    int                         m_scenario;
    struct camera2_shot_ext     m_metadata;
    struct exynos_camera_info   m_cameraInfo;
    struct ExynosCameraSensorInfoBase *m_staticInfo;
    struct ExynosConfigInfo     *m_exynosconfig;
    struct CameraMetaParameters m_metaParameters;

    exif_attribute_t            m_exifInfo;
    debug_attribute_t           mDebugInfo;
    debug_attribute_t           mDebugInfo2;

    mutable Mutex               m_parameterLock;
    mutable Mutex               m_staticInfoExifLock;
    mutable Mutex               m_faceDetectMetaLock;

    ExynosCameraActivityControl *m_activityControl;

    /* Flags for camera status */
    bool                        m_flagCheckPIPMode;
    bool                        m_flagRestartStream;

    bool                        m_flagCheckProMode;
#ifdef USE_DUAL_CAMERA
    bool                        m_flagCheckDualMode;
    enum DUAL_OPERATION_MODE    m_dualOperationMode;
#endif
   int                         m_burstShotFps;
    int                         m_burstShotTargetFps;
#ifdef LLS_CAPTURE
    bool                        m_flagLLSOn;
    int                         m_LLSValue;
    int                         m_LLSCaptureOn;
#endif
#ifdef OIS_CAPTURE
    bool                        m_flagOISCaptureOn;
#endif
#ifdef RAWDUMP_CAPTURE
    bool                        m_flagRawCaptureOn;
#endif
    bool                        m_flagDynamicPickCaptureOn;
    bool                        m_flagCheckRecordingHint;

    int                         m_setfile;
    int                         m_yuvRange;
    int                         m_setfileReprocessing;
    int                         m_yuvRangeReprocessing;
#ifdef USE_BINNING_MODE
    int                         m_binningProperty;
#endif
#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    int                         m_fpsProperty;
#endif
    bool                        m_useSizeTable;
    bool                        m_useSensorPackedBayer;
    bool                        m_useDynamicBayer;
    bool                        m_useDynamicBayer60Fps;
    bool                        m_useDynamicBayer120Fps;
    bool                        m_useDynamicBayer240Fps;
    bool                        m_usePureBayerReprocessing;
    bool                        m_useFastenAeStable;
    bool                        m_dvfsLock;

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo       m_ssrmCameraInfo;
#endif

#ifdef SAMSUNG_DOF
    int                         m_focusLensPos;
#endif

    int                         m_firing_flash_marking;
    uint64_t                    m_exposureTimeCapture;

#ifdef SUPPORT_DEPTH_MAP
    bool                        m_flagUseDepthMap;
    int                         m_depthMapW;
    int                         m_depthMapH;
#endif

    int                         m_yuvBufferCount[YUV_OUTPUT_PORT_ID_MAX];
    int                         m_previewPortId;
    int                         m_recordingPortId;
    int                         m_yuvOutPortId[MAX_PIPE_NUM];

#ifdef SAMSUNG_LLS_DEBLUR
    int                         m_flagLDCaptureMode;
    uint32_t                    m_flagLDCaptureLLSValue;
    int                         m_LDCaptureCount;
#endif
    int                         m_yuvStallPort;
    int                         m_flagYuvStallPortUsage;
#ifdef SAMSUNG_LENS_DC
    bool                        m_flagLensDCEnable;
    bool                        m_flagLensDCMode;
#endif
#ifdef SAMSUNG_STR_CAPTURE
    bool                        m_flagSTRCaptureEnable;
#endif
    bool                        m_isManualAeControl;
#ifdef SAMSUNG_HRM
    bool                        m_flagSensorHRM_Hint;
#endif
#ifdef SAMSUNG_LIGHT_IR
    bool                        m_flagSensorLight_IR_Hint;
#endif
#ifdef SAMSUNG_GYRO
    bool                        m_flagSensorGyroHint;
    SensorListenerEvent_t       m_gyroListenerData;
#endif
#ifdef SAMSUNG_ACCELEROMETER
    bool                        m_flagSensorAccelerationHint;
#endif
#ifdef SAMSUNG_ROTATION
    bool                        m_flagSensorRotationHint;
#endif
    bool                        m_isUniqueIdRead;

    bool                        m_isFactoryBin;
    int64_t                     m_exposureTime;
    int                         m_gain;
    int64_t                     m_ledPulseWidth;
    int64_t                     m_ledPulseDelay;
    int                         m_ledCurrent;
    int                         m_ledMaxTime;

    bool                        m_isFullSizeLut;
    int                         m_thumbnailCbW;
    int                         m_thumbnailCbH;

    int                         m_previewDsInputPortId;
    int                         m_captureDsInputPortId;

#ifdef SAMSUNG_OT
    ExynosRect2                 m_objectTrackingArea;
    int                         m_objectTrackingWeight;
#endif
    int                         m_flagtransientAction;
#ifdef SAMSUNG_IDDQD
    bool                        m_lensDirtyDetected;
#endif
#ifdef SAMSUNG_SW_VDIS
    int                         m_previewOffsetLeft;
    int                         m_previewOffsetTop;
    int                         m_videoOutIndex;
#endif
};

}; /* namespace android */

#endif
