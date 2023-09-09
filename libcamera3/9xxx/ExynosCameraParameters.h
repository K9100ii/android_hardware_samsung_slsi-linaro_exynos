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
#include "ExynosCameraConfigurations.h"

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

#define STATE_REG_LIVE_OUTFOCUS         (1<<22)
#define STATE_REG_BINNING_MODE          (1<<20)
#define STATE_REG_LONG_CAPTURE          (1<<18)
#define STATE_REG_RTHDR_AUTO            (1<<16)
#define STATE_REG_NEED_LLS              (1<<14)
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
#define STATE_VIDEO_CAPTURE                     (STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_STILL_CAPTURE_LLS                 (STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)
#define STATE_STILL_CAPTURE_LONG                (STATE_REG_FLAG_REPROCESSING|STATE_REG_LONG_CAPTURE)
#define STATE_STILL_CAPTURE_LLS_ZOOM            (STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)

#define STATE_STILL_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM            (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_ON_LLS_ZOOM        (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)
#define STATE_VIDEO_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_VIDEO_CAPTURE_WDR_ON_LLS             (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)

#define STATE_STILL_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM            (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_VIDEO_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_STILL_CAPTURE_WDR_AUTO_LLS             (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)
#define STATE_STILL_CAPTURE_WDR_AUTO_LLS_ZOOM        (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS|STATE_REG_ZOOM)

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

#define STATE_STILL_BINNING_PREVIEW             (STATE_REG_BINNING_MODE)
#define STATE_STILL_BINNING_PREVIEW_WDR_AUTO    (STATE_REG_BINNING_MODE|STATE_REG_RTHDR_AUTO)
#define STATE_VIDEO_BINNING                     (STATE_REG_RECORDINGHINT|STATE_REG_BINNING_MODE)
#define STATE_DUAL_STILL_BINING_PREVIEW         (STATE_REG_DUAL_MODE|STATE_REG_BINNING_MODE)
#define STATE_DUAL_VIDEO_BINNING                (STATE_REG_DUAL_RECORDINGHINT|STATE_REG_DUAL_MODE|STATE_REG_BINNING_MODE)

#define STATE_LIVE_OUTFOCUS_PREVIEW             (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_PREVIEW)
#define STATE_LIVE_OUTFOCUS_PREVIEW_WDR_ON      (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_PREVIEW|STATE_REG_RTHDR_ON)
#define STATE_LIVE_OUTFOCUS_PREVIEW_WDR_AUTO    (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_PREVIEW|STATE_REG_RTHDR_AUTO)
#define STATE_LIVE_OUTFOCUS_CAPTURE             (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_CAPTURE)
#define STATE_LIVE_OUTFOCUS_CAPTURE_WDR_ON      (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_CAPTURE|STATE_REG_RTHDR_ON)
#define STATE_LIVE_OUTFOCUS_CAPTURE_WDR_AUTO    (STATE_REG_LIVE_OUTFOCUS|STATE_STILL_CAPTURE|STATE_REG_RTHDR_AUTO)

namespace android {

using namespace std;
using ::android::hardware::camera::common::V1_0::helper::CameraParameters;
using ::android::hardware::camera::common::V1_0::helper::CameraMetadata;

class ExynosCameraConfigurations;

enum HW_INFO_SIZE_TYPE {
    HW_INFO_SENSOR_MARGIN_SIZE,
    HW_INFO_MAX_SENSOR_SIZE,
    HW_INFO_MAX_PREVIEW_SIZE,
    HW_INFO_MAX_PICTURE_SIZE,
    HW_INFO_MAX_THUMBNAIL_SIZE,
    HW_INFO_HW_YUV_SIZE,
    HW_INFO_MAX_HW_YUV_SIZE,
    HW_INFO_HW_SENSOR_SIZE,
    HW_INFO_HW_BNS_SIZE,
    HW_INFO_HW_PREVIEW_SIZE,
    HW_INFO_HW_PICTURE_SIZE,
#ifdef SAMSUNG_TN_FEATURE
#endif
    HW_INFO_SIZE_MAX,
};

enum HW_INFO_MODE_TYPE {
    HW_INFO_MODE_MAX,
};

enum SUPPORTED_HW_FUNCTION_TYPE {
    SUPPORTED_HW_FUNCTION_SENSOR_STANDBY,
    SUPPORTED_HW_FUNCTION_MAX,
};

typedef enum DUAL_STANDBY_STATE {
    DUAL_STANDBY_STATE_ON,
    DUAL_STANDBY_STATE_ON_READY,
    DUAL_STANDBY_STATE_OFF,
    DUAL_STANDBY_STATE_OFF_READY,
} dual_standby_state_t;

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
    ExynosCameraParameters(int cameraId, int scenario, ExynosCameraConfigurations *configurations);

    /* Destructor */
    virtual ~ExynosCameraParameters();

    void            setDefaultCameraInfo(void);

public:
    /* Size configuration */
    status_t        setSize(enum HW_INFO_SIZE_TYPE type, uint32_t width, uint32_t height, int outputPortId = -1);
    status_t        getSize(enum HW_INFO_SIZE_TYPE type, uint32_t *width, uint32_t *height, int outputPortId = -1);
    status_t        resetSize(enum HW_INFO_SIZE_TYPE type);

    /* Function configuration */
    bool            isSupportedFunction(enum SUPPORTED_HW_FUNCTION_TYPE type) const;

    status_t        checkPictureSize(int pictureW, int pctureH);
    status_t        checkThumbnailSize(int thumbnailW, int thumbnailH);

    status_t        resetYuvSizeRatioId(void);
    status_t        checkYuvSize(const int width, const int height, const int outputPortId, bool reprocessing = false);
    status_t        checkHwYuvSize(const int width, const int height, const int outputPortId, bool reprocessing = false);
#ifdef HAL3_YUVSIZE_BASED_BDS
    status_t        initYuvSizes();
#endif

    void            setPreviewPortId(int outputPortId);
    bool            isPreviewPortId(int outputPortId);
    int             getPreviewPortId(void);

    void            setRecordingPortId(int outputPortId);
    bool            isRecordingPortId(int outputPortId);
    int             getRecordingPortId(void);

    void            setYuvOutPortId(enum pipeline pipeId, int outputPortId);
    int             getYuvOutPortId(enum pipeline pipeId);

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

private:
    /* Sets the image format for preview-related HW. */
    void            m_setHwPreviewFormat(int colorFormat);
    /* Sets the image format for picture-related HW. */
    void            m_setHwPictureFormat(int colorFormat);
    /* Sets the image pixel size for picture-related HW. */
    void            m_setHwPicturePixelSize(camera_pixel_size pixelSize);

    /* Sets HW Bayer Crop Size */
    void            m_setHwBayerCropRegion(int w, int h, int x, int y);
    /* Sets Bayer Crop Region */
    status_t        m_setParamCropRegion(int srcW, int srcH, int dstW, int dstH);

/*
 * Additional API.
 */
/*
 * Vendor specific APIs
 */
/*
 * Others
 */
    void            m_setExifFixedAttribute(void);
    status_t        m_adjustPreviewFpsRange(uint32_t &newMinFps, uint32_t &newMaxFps);
    status_t        m_adjustPreviewCropSizeForPicture(ExynosRect *inputRect, ExynosRect *previewCropRect);

public:
    /* Returns the image format for FLITE/3AC/3AP bayer */
    int             getBayerFormat(int pipeId);
    /* Returns the image format for picture-related HW. */
    int             getHwPictureFormat(void);
    /* Returns the image pixel size for picture-related HW. */
    camera_pixel_size getHwPicturePixelSize(void);

    /* Returns the image format for preview-related HW. */
    int             getHwPreviewFormat(void);
    /* Returns HW Bayer Crop Size */
    void            getHwBayerCropRegion(int *w, int *h, int *x, int *y);
    /* Set the current crop region info */
    status_t        setCropRegion(int x, int y, int w, int h);

    void            getHwVraInputSize(int *w, int *h, int dsInputPortId);
    void            getHw3aaVraInputSize(int dsInW, int dsInH, ExynosRect * dsOutRect);
    int             getHwVraInputFormat(void);
    int             getHW3AFdFormat(void);

    void            setDsInputPortId(int dsInputPortId, bool isReprocessing);
    int             getDsInputPortId(bool isReprocessing);

    void            setYsumPordId(int ysumPortId, struct camera2_shot_ext *shot_ext);
    int             getYsumPordId(void);

    int             getYuvSizeRatioId(void);
/*
 * Additional API.
 */

    /* Get sensor control frame delay count */
    int             getSensorControlDelay(void);

    /* Static flag for LLS */
    bool            getLLSOn(void);
    void            setLLSOn(bool enable);

/*
 * Vendor specific APIs
 */

    /* Gets Intelligent mode */
    bool            getHWVdisMode(void);
    int             getHWVdisFormat(void);
#ifdef SUPPORT_ME
    int             getMeFormat(void);
    void            getMeSize(int *meWidth, int *meHeight);
    int             getLeaderPipeOfMe();
#endif

    void            getYuvVendorSize(int *width, int *height, int outputPortId, ExynosRect ispSize);

    uint32_t        getSensorStandbyDelay(void);

#ifdef USE_BINNING_MODE
    int *           getBinningSizeTable(void);
#endif
    /* Gets ImageUniqueId */
    const char     *getImageUniqueId(void);
/*
 * Static info
 */
    /* Gets max zoom ratio */
    float           getMaxZoomRatio(void);

    /* Gets current zoom ratio about Bcrop */
    float           getActiveZoomRatio(void);
    /* Sets current zoom ratio about Bcrop */
    void            setActiveZoomRatio(float zoomRatio);
    /* Gets current zoom rect about Bcrop */
    void            getActiveZoomRect(ExynosRect *zoomRect);
    /*Sets current zoom rect about Bcrop */
    void            setActiveZoomRect(ExynosRect zoomRect);

    /* Gets current zoom margin */
    int             getActiveZoomMargin(void);
    /* Sets current zoom margin */
    void            setActiveZoomMargin(int zoomMargin);

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

#ifdef USE_BINNING_MODE
    int             getBinningMode(void);
#endif /* USE_BINNING_MODE */
    void            updatePreviewStatRoi(struct camera2_shot_ext *shot_ext, ExynosRect *bCropRect);

public:
    /* For Vendor */
    void            updateHwSensorSize(void);
    void            updateBinningScaleRatio(void);

    status_t        duplicateCtrlMetadata(void *buf);

    status_t        reInit(void);
    void            vendorSpecificConstructor(int cameraId);

private:
    bool            m_isSupportedYuvSize(const int width, const int height, const int outputPortId, int *ratio);
    bool            m_isSupportedPictureSize(const int width, const int height);
    status_t        m_getSizeListIndex(int (*sizelist)[SIZE_OF_LUT], int listMaxSize, int ratio, int *index);
    status_t        m_getPictureSizeList(int *sizeList);
    status_t        m_getPreviewSizeList(int *sizeList);
    bool            m_isSupportedFullSizePicture(void);

    void            m_initMetadata(void);

/*
 * Vendor specific adjust function
 */
    void            m_vendorSpecificDestructor(void);
    status_t        m_getPreviewBdsSize(ExynosRect *dstRect);
    status_t        m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                        int *newHwPictureW, int *newHwPictureH);
    void            m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH);
    void            m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void            m_getCropRegion(int *x, int *y, int *w, int *h);

    void            m_setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                              ExynosRect          *PictureRect,
                                              ExynosRect          *thumbnailRect,
                                              camera2_shot_t      *shot,
                                              bool                useDebugInfo2 = false);


    /* H/W Chain Scenario Infos */
    enum HW_CONNECTION_MODE         m_getFlite3aaOtf(void);
    enum HW_CONNECTION_MODE         m_get3aaIspOtf(void);
    enum HW_CONNECTION_MODE         m_get3aaVraOtf(void);
    enum HW_CONNECTION_MODE         m_getIspMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getMcscVraOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessing3Paf3AAOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessing3aaIspOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessing3aaVraOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingIspMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingMcscVraOtf(void);
#ifdef USE_DUAL_CAMERA
    enum HW_CONNECTION_MODE         m_getIspDcpOtf(void);
    enum HW_CONNECTION_MODE         m_getDcpMcscOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingIspDcpOtf(void);
    enum HW_CONNECTION_MODE         m_getReprocessingDcpMcscOtf(void);
#endif

    int                 m_adjustDsInputPortId(const int dsInputPortId);

public:
    ExynosCameraActivityControl *getActivityControl(void);

    void                getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void                setSetfileYuvRange(void);
    void                setSetfileYuvRange(bool flagReprocessing, int setfile, int yuvRange);
    status_t            checkSetfileYuvRange(void);

    void                setUseSensorPackedBayer(bool enable);
    bool                getUsePureBayerReprocessing(void);
    int32_t             getReprocessingBayerMode(void);

    void                setFastenAeStableOn(bool enable);
    bool                getFastenAeStableOn(void);
    bool                checkFastenAeStableEnable(void);

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

    bool                getSensorOTFSupported(void);
    bool                isReprocessing(void);
    bool                isSccCapture(void);

    /* True if private reprocessing or YUV reprocessing is supported */
    bool                isSupportZSLInput(void);

    enum HW_CHAIN_TYPE  getHwChainType(void);
    uint32_t            getNumOfMcscInputPorts(void);
    uint32_t            getNumOfMcscOutputPorts(void);

    enum HW_CONNECTION_MODE getHwConnectionMode(
                                enum pipeline srcPipeId,
                                enum pipeline dstPipeId);

    bool                isUse3aaInputCrop(void);
    void                setVideoStreamExistStatus(bool);
    bool                isVideoStreamExist(void);
    bool                isUse3aaBDSOff();

    bool                isUseIspInputCrop(void);
    bool                isUseMcscInputCrop(void);
    bool                isUseReprocessing3aaInputCrop(void);
    bool                isUseReprocessingIspInputCrop(void);
    bool                isUseReprocessingMcscInputCrop(void);
    bool                isUseEarlyFrameReturn(void);
    bool                isUse3aaDNG(void);
    bool                isUseHWFC(void);
    bool                isUseThumbnailHWFC(void) {return true;};
    bool                isHWFCOnDemand(void);
    bool                isUseRawReverseReprocessing(void);

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

#ifdef USE_DUAL_CAMERA
    status_t                 setStandbyState(dual_standby_state_t state);
    dual_standby_state_t     getStandbyState(void);
#endif

#ifdef SUPPORT_DEPTH_MAP
    status_t    getDepthMapSize(int *depthMapW, int *depthMapH);
    void        setDepthMapSize(int depthMapW, int depthMapH);
    bool        isDepthMapSupported(void);
#endif

    bool                checkFaceDetectMeta(struct camera2_shot_ext *shot_ext);
    void                getFaceDetectMeta(camera2_shot_ext *shot_ext);

    bool                getHfdMode(void);
    bool                getGmvMode(void);

    void                getVendorRatioCropSizeForVRA(ExynosRect *ratioCropSize);
    void                getVendorRatioCropSize(ExynosRect *ratioCropSize
                                                            , ExynosRect *mcscSize
                                                            , int portIndex
                                                            , int isReprocessing
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
                                                            , ExynosRect *sensorSize
                                                            , ExynosRect *mcscInputSize
#endif
                                                            );
#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    uint64_t            getCorrectTimeForSensorFusion(void);
#endif

private:
    ExynosCameraConfigurations  *m_configurations;
    int                         m_scenario;
    struct camera2_shot_ext     m_metadata;
    struct exynos_camera_info   m_cameraInfo;
    struct ExynosCameraSensorInfoBase *m_staticInfo;

    exif_attribute_t            m_exifInfo;
    debug_attribute_t           mDebugInfo;
    debug_attribute_t           mDebugInfo2;

    mutable Mutex               m_parameterLock;
    mutable Mutex               m_staticInfoExifLock;
    mutable Mutex               m_faceDetectMetaLock;

    ExynosCameraActivityControl *m_activityControl;

    uint32_t                    m_width[HW_INFO_SIZE_MAX];
    uint32_t                    m_height[HW_INFO_SIZE_MAX];
    bool                        m_mode[HW_INFO_MODE_MAX];

    int                         m_hwYuvWidth[YUV_OUTPUT_PORT_ID_MAX];
    int                         m_hwYuvHeight[YUV_OUTPUT_PORT_ID_MAX];

    int                         m_depthMapW;
    int                         m_depthMapH;

    bool                        m_LLSOn;

    /* Flags for camera status */
    int                         m_setfile;
    int                         m_yuvRange;
    int                         m_setfileReprocessing;
    int                         m_yuvRangeReprocessing;

#ifdef USE_DUAL_CAMERA
    dual_standby_state_t        m_standbyState;
    mutable Mutex               m_standbyStateLock;
#endif

#ifdef USE_BINNING_MODE
    int                         m_binningProperty;
#endif
    bool                        m_useSizeTable;
    bool                        m_useSensorPackedBayer;
    bool                        m_usePureBayerReprocessing;

    bool                        m_fastenAeStableOn;
    bool                        m_videoStreamExist;

    int                         m_previewPortId;
    int                         m_recordingPortId;
    int                         m_yuvOutPortId[MAX_PIPE_NUM];
    int                         m_ysumPortId;

    bool                        m_isUniqueIdRead;

    int                         m_previewDsInputPortId;
    int                         m_captureDsInputPortId;


    ExynosRect                  m_activeZoomRect;
    float                       m_activeZoomRatio;
    int                         m_activeZoomMargin;

/* Vedor specific API */
private:
    status_t        m_vendorReInit(void);

#ifdef SAMSUNG_HRM
    void            m_setHRM(int ir_data, int flicker_data, int status);
#endif

#ifdef SAMSUNG_LIGHT_IR
    void            m_setLight_IR(SensorListenerEvent_t data);
#endif

#ifdef SAMSUNG_GYRO
    void            m_setGyro(SensorListenerEvent_t data);
    SensorListenerEvent_t            *getGyroData(void);
#endif

#ifdef SAMSUNG_ACCELEROMETER
    void            m_setAccelerometer(SensorListenerEvent_t data);
#endif

#ifdef SAMSUNG_DOF
    void            m_setLensPos(int pos);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    status_t        m_getFusionSize(int w, int h, ExynosRect *rect, bool flagSrc, int margin);
#endif


public:
#ifdef SAMSUNG_PROX_FLICKER
    void            setProxFlicker(SensorListenerEvent_t data);
#endif

/* Additional API. */
#ifdef SAMSUNG_PAF
     /* Gets PAF */
    enum camera2_paf_mode            getPaf(void);
#endif

#ifdef SAMSUNG_RTHDR
     /* Gets RT HDR */
    enum camera2_wdr_mode            getRTHdr(void);
    void                             setRTHdr(enum camera2_wdr_mode rtHdrMode);
#endif

/* Vendor specific APIs */
#ifdef USE_LLS_REPROCESSING
    /* Dynamic flag for LLS Capture */
    bool            getLLSCaptureOn(void);
    void            setLLSCaptureOn(bool enable);
    int             getLLSCaptureCount(void);
    void            setLLSCaptureCount(int count);
#endif

#ifdef SAMSUNG_TN_FEATURE
    void            setImageUniqueId(char *uniqueId);
#endif

#ifdef SAMSUNG_STR_PREVIEW
    bool            getSTRPreviewEnable(void);
#endif

#ifdef SAMSUNG_SW_VDIS
    void            getSWVdisYuvSize(int w, int h, int fps, int *newW, int *newH);
    void            getSWVdisAdjustYuvSize(int *width, int *height, int fps);
    bool            isSWVdisOnPreview(void);
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    void            setSWVdisMetaCtlOISCoef(uint32_t coef);
    void            setSWVdisPreviewFrameExposureTime(int exposureTime);
    int             getSWVdisPreviewFrameExposureTime(void);
#endif
#endif

#ifdef SAMSUNG_HYPERLAPSE
    void            getHyperlapseYuvSize(int w, int h, int fps, int *newW, int *newH);
    void            getHyperlapseAdjustYuvSize(int *width, int *height, int fps);
#endif

/* Vendor specific adjust function */
#ifdef SAMSUNG_SSM
    status_t        checkSSMEnable(const CameraParameters& params);
    void            m_setSSMEnable(bool flagSSMEnable);
#endif

#ifdef LLS_CAPTURE
    void                setLLSValue(struct camera2_shot_ext *shot);
    int                 getLLSValue(void);
#endif


#ifdef OIS_CAPTURE
    void                checkOISCaptureMode(int multiCaptureMode);
#endif

#ifdef SAMSUNG_LLS_DEBLUR
    void                checkLDCaptureMode(bool skipLDCapture = false);
    void                setLLSdebugInfo(unsigned char *data, unsigned int size);
#ifdef SUPPORT_ZSL_MULTIFRAME
    void                checkZslMultiframeMode(void);
#endif
#ifdef SAMSUNG_MFHDR_CAPTURE
    void                setMFHDRdebugInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SAMSUNG_LLHDR_CAPTURE
    void                setLLHDRdebugInfo(unsigned char *data, unsigned int size);
#endif
#endif

#ifdef SAMSUNG_STR_CAPTURE
    void                checkSTRCaptureMode(bool hasCaptureStream);
    void                setSTRdebugInfo(unsigned char *data, unsigned int size);
#endif

#ifdef SAMSUNG_VIDEO_BEAUTY_SNAPSHOT
    void                checkBeautyCaptureMode(bool hasCaptureStream);
#endif

#ifdef SAMSUNG_FACTORY_DRAM_TEST
    int                 getFactoryDramTestCount(void);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    status_t            adjustDualSolutionSize(int targetWidth, int targetHeight);
    status_t            getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect,
                                  int margin = DUAL_SOLUTION_MARGIN_VALUE_30);
    void                getDualSolutionSize(int *dstW, int *dstH, int *wideW, int *wideH, int *teleW, int *teleH,
                                    int margin = DUAL_SOLUTION_MARGIN_VALUE_30);
    int                 getDualSolutionMarginValue(ExynosRect* targetRect, ExynosRect* SrcRect);

    void                storeAeState(int aeState);
    int                 getAeState(void);
    void                storeSceneDetectIndex(int scene_index);
    int                 getSceneDetectIndex(void);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    void                setFusionCapturedebugInfo(unsigned char *data, unsigned int size);
    bool                useCaptureExtCropROI(void);
    void                setCaptureExtCropROI(UTrect captureExtCropROI);
#endif

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo* getSsrmCameraInfo(void) {return &m_ssrmCameraInfo; }
#endif

#ifdef SAMSUNG_HIFI_VIDEO
    bool            isValidHiFiVideoSize(int w, int h, int fps);
    void            getHiFiVideoYuvSize(int w, int h, int fps, int *newW, int *newH);
    void            getHiFiVideoAdjustYuvSize(int *width, int *height, int fps);
    void            getHiFiVideoScalerControlInfo(int vid, ExynosRect *refRect, ExynosRect *srcRect, ExynosRect *dstRect);
    void            getHiFiVideoZoomRatio(float viewZoomRatio, float *activeZoomRatio, int *activeMargin);
    void            getHiFiVideoCropRegion(ExynosRect *cropRegion);
    void            setHFVZdebugInfo(unsigned char *data, unsigned int size);
#endif

/* Vedor specific member */
private:
#ifdef USE_LLS_REPROCESSING
    bool                        m_LLSCaptureOn;
    int                         m_LLSCaptureCount;
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    int                         m_dualDstWidth;
    int                         m_dualDstHeight;
    int                         m_dualSrcWideWidth;
    int                         m_dualSrcWideHeight;
    int                         m_dualSrcTeleWidth;
    int                         m_dualSrcTeleHeight;
    int                         m_dualMargin20SrcWideWidth;
    int                         m_dualMargin20SrcWideHeight;
    int                         m_dualMargin20SrcTeleWidth;
    int                         m_dualMargin20SrcTeleHeight;

    int                         m_aeState;
    int                         m_sceneDetectIndex;
#endif

#ifdef SAMSUNG_GYRO
    SensorListenerEvent_t       m_gyroListenerData;
#endif

#ifdef SAMSUNG_HIFI_VIDEO
    mutable Mutex               m_metaInfoLock;
#endif

#ifdef SAMSUNG_SW_VDIS_USE_OIS
    int                         m_SWVdisPreviewFrameExposureTime;
#endif

#ifdef SAMSUNG_SSRM
    struct ssrmCameraInfo      m_ssrmCameraInfo;
#endif

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    ExynosRect                 m_captureExtCropROI;
#endif

#ifdef LLS_CAPTURE
    int                         m_LLSValue;
    int                         m_needLLS_history[LLS_HISTORY_COUNT];
#endif

#ifdef CORRECT_TIMESTAMP_FOR_SENSORFUSION
    uint64_t                    m_correctionTime;
#endif

/* Add vendor rearrange function */
private:
    status_t            m_vendorConstructorInitalize(int cameraId);
    void                m_vendorSWVdisMode(bool *ret);
    void                m_vendorSetExifChangedAttribute(debug_attribute_t    &debugInfo,
                                              unsigned int &offset,
                                              bool &isWriteExif,
                                              camera2_shot_t *shot,
                                              bool useDebugInfo2 = false);

};

}; /* namespace android */

#endif
