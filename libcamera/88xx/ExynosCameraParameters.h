/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#include <camera/CameraParameters.h>

#include <videodev2.h>
#include <videodev2_exynos_media.h>
#include <videodev2_exynos_camera.h>

#ifdef USE_CSC_FEATURE
#include <SecNativeFeature.h>
#endif

#include "ExynosCameraConfig.h"
#include "ExynosCameraSensorInfoBase.h"
#include "ExynosCameraCounter.h"
#include "fimc-is-metadata.h"
#include "ExynosRect.h"
#include "exynos_format.h"
#include "ExynosExif.h"
#include "ExynosCameraUtils.h"
//#include "ExynosCameraUtilsModule.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraAutoTimer.h"

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
#include "ExynosCameraFusionInclude.h"
#endif

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
#endif
#if 0//def SUPPORT_SW_VDIS
#include "SecCameraSWVdis.h"
#endif /*SUPPORT_SW_VDIS*/
#ifdef SUPPORT_SW_VDIS
#include "SecCameraSWVdis_3_0.h"
#endif /*SUPPORT_SW_VDIS*/
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

#include <map>

#define CAMERA_SAVE_PATH_PHONE "/data/media/0"
#define CAMERA_SAVE_PATH_EXT "/mnt/extSdCard"
#define CAMERA_FILE_PATH_SIZE 100

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

#define STATE_STILL_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM            (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM_OUTDOOR    (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_OUTDOOR)
#define STATE_STILL_CAPTURE_WDR_ON_ZOOM_INDOOR     (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_INDOOR)
#define STATE_VIDEO_CAPTURE_WDR_ON                 (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_VIDEO_CAPTURE_WDR_ON_LLS             (STATE_REG_RTHDR_ON|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)

#define STATE_STILL_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM            (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_OUTDOOR    (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_OUTDOOR)
#define STATE_STILL_CAPTURE_WDR_AUTO_ZOOM_INDOOR     (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_ZOOM_INDOOR)
#define STATE_VIDEO_CAPTURE_WDR_AUTO                 (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_RECORDINGHINT)
#define STATE_STILL_CAPTURE_WDR_AUTO_LLS             (STATE_REG_RTHDR_AUTO|STATE_REG_FLAG_REPROCESSING|STATE_REG_NEED_LLS)
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

namespace CONFIG_MODE {
    enum MODE {
        NORMAL        = 0x00,
        HIGHSPEED_60,
        HIGHSPEED_120,
        HIGHSPEED_240,
        MAX
    };
};

struct CONFIG_PIPE {
    uint32_t prepare[MAX_PIPE_NUM_REPROCESSING];
};

struct CONFIG_BUFFER {
    uint32_t num_sensor_buffers;
    uint32_t num_bayer_buffers;
    uint32_t init_bayer_buffers;
    uint32_t num_3aa_buffers;
    uint32_t num_hwdis_buffers;
    uint32_t num_preview_buffers;
    uint32_t num_preview_cb_buffers;
    uint32_t num_picture_buffers;
    uint32_t num_reprocessing_buffers;
    uint32_t num_recording_buffers;
    uint32_t num_fastaestable_buffer;
    uint32_t reprocessing_bayer_hold_count;
    uint32_t front_num_bayer_buffers;
    uint32_t front_num_picture_buffers;
    uint32_t preview_buffer_margin;
    uint32_t num_hiding_mode_buffers;
    /* for USE_CAMERA2_API_SUPPORT */
    uint32_t num_request_raw_buffers;
    uint32_t num_request_preview_buffers;
    uint32_t num_request_callback_buffers;
    uint32_t num_request_video_buffers;
    uint32_t num_request_jpeg_buffers;
    uint32_t num_min_block_request;
    uint32_t num_max_block_request;
    /* for USE_CAMERA2_API_SUPPORT */
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

struct fast_ctl_capture {
    uint32_t ready;
    uint32_t capture_intent;
    uint32_t capture_count;
    uint32_t capture_exposureTime;
};

class IMetadata {
public:
    IMetadata(){};
    virtual ~IMetadata(){};

    virtual status_t        duplicateCtrlMetadata(void *buf) = 0;
};

class IHWConfig {
public:
    IHWConfig(){};
    virtual ~IHWConfig(){};
    virtual bool                        getUsePureBayerReprocessing(void) = 0;
    virtual bool                        isSingleChain(void) = 0;
    virtual bool                        isSccCapture(void) = 0;
    virtual bool                        isReprocessing(void) = 0;
    virtual status_t                    getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH) = 0;
    virtual void                        getHwBayerCropRegion(int *w, int *h, int *x, int *y) = 0;
    virtual void                        getHwPreviewSize(int *w, int *h) = 0;
    virtual void                        getVideoSize(int *w, int *h) = 0;
    virtual void                        getHwVideoSize(int *w, int *h) = 0;
    virtual void                        getPictureSize(int *w, int *h) = 0;
    virtual bool                        getHWVdisMode(void) = 0;
    virtual status_t                    getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect) = 0;
    virtual status_t                    getPictureBdsSize(ExynosRect *dstRect) = 0;
    virtual void                        getHwSensorSize(int *w, int *h) = 0;
    virtual bool                        isIspMcscOtf(void) = 0;
    virtual bool                        isTpuMcscOtf(void) = 0;
    virtual bool                        isMcscVraOtf(void) {return true;};
    virtual bool                        isReprocessing3aaIspOTF(void) = 0;
    virtual bool                        isReprocessingIspMcscOtf(void) = 0;
    virtual bool                        isReprocessingTpuMcscOtf(void) = 0;
    virtual bool                        isUse3aaInputCrop(void) = 0;
    virtual bool                        isUseIspInputCrop(void) = 0;
    virtual bool                        isUseMcscInputCrop(void) = 0;
    virtual bool                        isUseReprocessing3aaInputCrop(void) = 0;
    virtual bool                        isUseReprocessingIspInputCrop(void) = 0;
    virtual bool                        isUseReprocessingMcscInputCrop(void) = 0;
    virtual bool                        isUseEarlyFrameReturn(void) = 0;
    virtual int                         getHwPreviewFormat(void) = 0;
    virtual bool                        getDvfsLock(void) = 0;
    virtual void                        getMaxSensorSize(int *w, int *h) = 0;
    virtual status_t                    getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom = true) = 0;
    virtual status_t                    getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom = true) = 0;
    virtual void                        getPreviewSize(int *w, int *h) = 0;
    virtual status_t                    getPreviewYuvCropSize(ExynosRect *dstRect) = 0;
    virtual status_t                    getPictureYuvCropSize(ExynosRect *dstRect) = 0;
    virtual bool                        getUseDynamicScc(void) = 0;
    virtual bool                        getUseDynamicBayerVideoSnapShot(void) = 0;
    virtual int                         getHWVdisFormat(void) = 0;
    virtual void                        getHwVraInputSize(__unused int *w, __unused int *h) {return;};
    virtual int                         getHwVraInputFormat(void) {return 0;};
    virtual uint32_t                    getBnsScaleRatio(void) = 0;
    virtual bool                        needGSCForCapture(int camId) = 0;
    virtual bool                        getSetFileCtlMode(void) = 0;
    virtual bool                        getSetFileCtl3AA_ISP(void) = 0;
    virtual bool                        getSetFileCtl3AA(void) = 0;
    virtual bool                        getSetFileCtlISP(void) = 0;
    virtual bool                        getSetFileCtlSCP(void) = 0;
    virtual uint64_t                    getCaptureExposureTime(void) = 0;
};

class IModeConfig {
public:
    IModeConfig(){};
    virtual ~IModeConfig(){};

    virtual bool                        getHdrMode(void) = 0;
    virtual bool                        getRecordingHint(void) = 0;
    virtual int                         getFocusMode(void) = 0;
    virtual int                         getZoomLevel(void) = 0;
    virtual void                        getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange) = 0;
    virtual float                       getZoomRatio(int zoom) = 0;
    virtual float                       getZoomRatio(void) = 0;
    virtual struct ExynosConfigInfo     *getConfig() = 0;
    virtual void                        setFlipHorizontal(int val) = 0;
    virtual int                         getFlipHorizontal(void) = 0;
    virtual void                        setFlipVertical(int val) = 0;
    virtual int                         getFlipVertical(void) = 0;
    virtual int                         getPictureFormat(void) = 0;
    virtual status_t                    getFdMeta(bool reprocessing, void *buf) = 0;
    virtual float                       getMaxZoomRatio(void)= 0;
    virtual int                         getHalVersion(void) = 0;



//    virtual int                         getGrallocUsage(void) = 0;
//    virtual int                         getGrallocLockUsage(void) = 0;
    virtual int                         getHDRDelay(void) = 0;
    virtual int                         getReprocessingBayerHoldCount(void) = 0;
    virtual int                         getFastenAeFps(void) = 0;

    virtual int                         getPerFrameControlPipe(void) = 0;
    virtual int                         getPerFrameControlReprocessingPipe(void) = 0;

    virtual int                         getPerFrameInfo3AA(void) = 0;
    virtual int                         getPerFrameInfoIsp(void) = 0;
    virtual int                         getPerFrameInfoDis(void) = 0;
    virtual int                         getPerFrameInfoReprocessingPure3AA(void) = 0;
    virtual int                         getPerFrameInfoReprocessingPureIsp(void) = 0;

    virtual int                         getScalerNodeNumPicture(void) = 0;

    virtual bool                         isOwnScc(int cameraId) = 0;

    virtual bool                        getDualMode(void) = 0;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    virtual bool                        getDualCameraMode(void) = 0;
    virtual bool                        isFusionEnabled(void) = 0;
    virtual status_t                    getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect) = 0;
    virtual status_t                    setFusionInfo(camera2_shot_ext *shot_ext) = 0;
    virtual DOF                        *getDOF(void) = 0;
    virtual bool                        getUseJpegCallbackForYuv(void) = 0;
#endif // BOARD_CAMERA_USES_DUAL_CAMERA
};

class IModeVendorConfig {
public:
    IModeVendorConfig(){};
    virtual ~IModeVendorConfig(){};

    virtual int                         getShotMode(void) = 0;
//    virtual bool                        getOISCaptureModeOn(void) = 0;
//    virtual void                        setOISCaptureModeOn(bool enable) = 0;
    virtual int                         getSeriesShotCount(void) = 0;
    virtual bool                        getHighResolutionCallbackMode(void) = 0;
};

class IActivityController {
public:
    IActivityController(){};
    virtual ~IActivityController(){};

    virtual ExynosCameraActivityControl *getActivityControl(void) = 0;

};

class IJpegConfig {
public:
    IJpegConfig(){};
    virtual ~IJpegConfig(){};

    virtual int                         getJpegQuality(void) = 0;
    virtual int                         getThumbnailQuality(void) = 0;
    virtual status_t                    getFixedExifInfo(exif_attribute_t *exifInfo) = 0;
    virtual debug_attribute_t           *getDebugAttribute(void) = 0;
    virtual void                        getThumbnailSize(int *w, int *h) = 0;
    virtual void                        setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                            ExynosRect          *PictureRect,
                                            ExynosRect          *thumbnailRect,
                                            camera2_shot_t      *shot) = 0;
    virtual status_t                    setMarkingOfExifFlash(int flag) = 0;
    virtual int                         getMarkingOfExifFlash(void) = 0;
};

class ISensorStaticInfo {
public:
    ISensorStaticInfo(){};
    virtual ~ISensorStaticInfo(){};

    virtual struct ExynosSensorInfoBase     *getSensorStaticInfo() = 0;
};

class ExynosCameraParameters : public IMetadata, public IHWConfig, public IModeConfig, public IModeVendorConfig, public IActivityController, public IJpegConfig, public ISensorStaticInfo {
public:
    ExynosCameraParameters(){};
    virtual ~ExynosCameraParameters(){};

//class Interface Metadata
    virtual status_t                    duplicateCtrlMetadata(void *buf);

//class Interface HWConfig
    virtual bool                        getUsePureBayerReprocessing(void) = 0;
    virtual bool                        isSccCapture(void) = 0;
    virtual bool                        isReprocessing(void) = 0;
    virtual status_t                    getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH) = 0;
    virtual void                        getHwBayerCropRegion(int *w, int *h, int *x, int *y) = 0;
    virtual void                        getHwPreviewSize(int *w, int *h) = 0;
    virtual void                        getVideoSize(int *w, int *h) = 0;
    virtual void                        getHwVideoSize(int *w, int *h) = 0;
    virtual void                        getPictureSize(int *w, int *h) = 0;
    virtual void                        getYuvSize(int *w, int *h, int index) = 0;
    virtual bool                        getHWVdisMode(void) = 0;
    virtual status_t                    getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect) = 0;
    virtual void                        getHwSensorSize(int *w, int *h) = 0;
    virtual status_t                    getPictureBdsSize(ExynosRect *dstRect) = 0;
    virtual bool                        isIspMcscOtf(void) = 0;
    virtual bool                        isTpuMcscOtf(void) = 0;
    virtual bool                        isMcscVraOtf(void) {return true;};
    virtual bool                        isReprocessing3aaIspOTF(void) = 0;
    virtual bool                        isReprocessingIspMcscOtf(void) = 0;
    virtual bool                        isReprocessingTpuMcscOtf(void) = 0;
    virtual bool                        isUse3aaInputCrop(void) = 0;
    virtual bool                        isUseIspInputCrop(void) = 0;
    virtual bool                        isUseMcscInputCrop(void) = 0;
    virtual bool                        isUseReprocessing3aaInputCrop(void) = 0;
    virtual bool                        isUseReprocessingIspInputCrop(void) = 0;
    virtual bool                        isUseReprocessingMcscInputCrop(void) = 0;
    virtual bool                        isUseEarlyFrameReturn(void) = 0;
    virtual bool                        isUseThumbnailHWFC(void) {return true;};
    virtual int                         getHwPreviewFormat(void) = 0;
    virtual bool                        getDvfsLock(void) = 0;
    virtual void                        getMaxSensorSize(int *w, int *h) = 0;
    virtual status_t                    getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom = true) = 0;
    virtual status_t                    getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom = true) = 0;
    virtual void                        getPreviewSize(int *w, int *h) = 0;
    virtual status_t                    getPreviewYuvCropSize(ExynosRect *dstRect) = 0;
    virtual status_t                    getPictureYuvCropSize(ExynosRect *dstRect) = 0;
    virtual bool                        getUseDynamicScc(void) = 0;
    virtual bool                        getUseDynamicBayerVideoSnapShot(void) = 0;
    virtual int                         getHWVdisFormat(void) = 0;
    virtual void                        getHwVraInputSize(__unused int *w, __unused int *h) {return;};
    virtual int                         getHwVraInputFormat(void) {return 0;};
    virtual uint32_t                    getBnsScaleRatio(void) = 0;
    virtual bool                        needGSCForCapture(int camId) = 0;
    virtual bool                        getSetFileCtlMode(void) = 0;
    virtual bool                        getSetFileCtl3AA_ISP(void) = 0;
    virtual bool                        getSetFileCtl3AA(void) = 0;
    virtual bool                        getSetFileCtlISP(void) = 0;
    virtual bool                        getSetFileCtlSCP(void) = 0;
    virtual uint64_t                    getCaptureExposureTime(void) = 0;
    virtual int32_t                     getLongExposureShotCount(void) = 0;

//class Interface ModeConfig
    virtual bool                        getHdrMode(void) = 0;
    virtual bool                        getRecordingHint(void) = 0;
    virtual int                         getFocusMode(void) = 0;
    virtual int                         getZoomLevel(void) = 0;
    virtual void                        getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange) = 0;
    virtual float                       getZoomRatio(int zoom) = 0;
    virtual float                       getZoomRatio(void) = 0;
    virtual struct ExynosConfigInfo     *getConfig() = 0;
    virtual void                        setFlipHorizontal(int val) = 0;
    virtual int                         getFlipHorizontal(void) = 0;
    virtual void                        setFlipVertical(int val) = 0;
    virtual int                         getFlipVertical(void) = 0;
    virtual int                         getPictureFormat(void) = 0;
    virtual int                         getHwPictureFormat(void) = 0;

    virtual int                         getHalVersion(void) = 0;





//    virtual int                         getGrallocUsage(void) = 0;
//    virtual int                         getGrallocLockUsage(void) = 0;
    virtual int                         getHDRDelay(void) = 0;
    virtual int                         getReprocessingBayerHoldCount(void) = 0;
    virtual int                         getFastenAeFps(void) = 0;
    virtual int                         getPerFrameControlPipe(void) = 0;
    virtual int                         getPerFrameControlReprocessingPipe(void) = 0;
    virtual int                         getPerFrameInfo3AA(void) = 0;
    virtual int                         getPerFrameInfoIsp(void) = 0;
    virtual int                         getPerFrameInfoDis(void) = 0;
    virtual int                         getPerFrameInfoReprocessingPure3AA(void) = 0;
    virtual int                         getPerFrameInfoReprocessingPureIsp(void) = 0;

    virtual int                         getScalerNodeNumPicture(void) = 0;
    virtual int                         getScalerNodeNumPreview(void) = 0;
    virtual int                         getScalerNodeNumVideo(void) = 0;
    virtual bool                         isOwnScc(int cameraId) = 0;

    virtual bool                        getDualMode(void) = 0;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    virtual bool                        getDualCameraMode(void) = 0;
    virtual bool                        isFusionEnabled(void) = 0;
    virtual status_t                    getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect) = 0;
    virtual status_t                    setFusionInfo(camera2_shot_ext *shot_ext) = 0;
    virtual DOF                        *getDOF(void) = 0;
    virtual bool                        getUseJpegCallbackForYuv(void) = 0;
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

    virtual bool                        getTpuEnabledMode(void) = 0;

    virtual int                         getCameraId(void) = 0;



//class Interface ModeVendorConfig
    virtual int                         getShotMode(void) = 0;
    virtual bool                        getOISCaptureModeOn(void){return false;};
    virtual void                        setOISCaptureModeOn(__unused bool enable){};
    virtual int                         getSeriesShotCount(void) = 0;
    virtual bool                        getHighResolutionCallbackMode(void) = 0;

    virtual void                        setNormalBestFrameCount(__unused uint32_t count){};
    virtual uint32_t                    getNormalBestFrameCount(void){return 0;};
    virtual void                        resetNormalBestFrameCount(void){};
    virtual void                        setSCPFrameCount(__unused uint32_t count){};
    virtual uint32_t                    getSCPFrameCount(void){return 0;};
    virtual void                        resetSCPFrameCount(void){};
    virtual void                        setBayerFrameCount(__unused uint32_t count){};
    virtual uint32_t                    getBayerFrameCount(void){return 0;};
    virtual void                        resetBayerFrameCount(void){};
    virtual bool                        getUseBestPic(void){return false;};
    virtual int                         getSeriesShotMode(void) = 0;

    virtual void                        setLLSCaptureCount(__unused int count){};
    virtual int                         getLLSCaptureCount() {return 0;};

//class Interface ActivityController
    virtual ExynosCameraActivityControl *getActivityControl(void) = 0;

//class Interface JpegConfig
    virtual int                         getJpegQuality(void) = 0;
    virtual int                         getThumbnailQuality(void) = 0;
    virtual status_t                    getFixedExifInfo(exif_attribute_t *exifInfo) = 0;
    virtual debug_attribute_t           *getDebugAttribute(void) = 0;
    virtual void                        getThumbnailSize(int *w, int *h) = 0;
    virtual void                        setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                            ExynosRect          *PictureRect,
                                            ExynosRect          *thumbnailRect,
                                            camera2_shot_t      *shot) = 0;

#ifdef DEBUG_RAWDUMP
    virtual bool                        checkBayerDumpEnable(void) = 0;
#endif/* DEBUG_RAWDUMP */
#ifdef SAMSUNG_DNG
    virtual bool                        getDNGCaptureModeOn(void) = 0;
    virtual dng_thumbnail_t             *createDngThumbnailBuffer(int size) = 0;
    virtual dng_thumbnail_t             *putDngThumbnailBuffer(dng_thumbnail_t *buf) = 0;
    virtual bool                        getUseDNGCapture(void) = 0;
    virtual bool                        getCheckMultiFrame(void) = 0;
#endif

#ifdef RAWDUMP_CAPTURE
    virtual bool                        getRawCaptureModeOn(void) = 0;
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    virtual int                         getLDCaptureMode(void) = 0;
    virtual int                         getLDCaptureCount(void) = 0;
#endif
#ifdef SAMSUNG_LENS_DC
    virtual bool                        getLensDCEnable(void) = 0;
#endif
#ifdef FLASHED_LLS_CAPTURE
    virtual bool                        getFlashedLLSCapture() = 0;
#endif
    virtual bool                        getOutPutFormatNV21Enable(void) = 0;
#ifdef SAMSUNG_JQ
    virtual int                         getJpegQtableStatus(void) = 0;
    virtual void                        setJpegQtableStatus(int state) = 0;
    virtual bool                        getJpegQtableOn(void) = 0;
    virtual void                        getJpegQtable(unsigned char* qtable) = 0;
#endif
    virtual status_t                    getDepthMapSize(int *depthMapW, int *depthMapH) = 0;
#ifdef SUPPORT_DEPTH_MAP
    virtual bool                        getUseDepthMap(void) = 0;
    virtual status_t                    checkUseDepthMap(void) = 0;
    virtual void                        m_setUseDepthMap(bool useDepthMap) = 0;
    virtual bool                        getDepthCallbackOnPreview(void) = 0;
    virtual bool                        getDepthCallbackOnCapture(void) = 0;
#endif
//Sensor Static Info
    virtual struct ExynosSensorInfoBase             *getSensorStaticInfo() = 0;
    virtual int                         getFrameSkipCount(void) = 0;
    virtual bool                        msgTypeEnabled(int32_t msgType) = 0;
#ifdef SAMSUNG_HYPER_MOTION
    virtual bool                        getHyperMotionMode(void) = 0;
    virtual void                        getHyperMotionCropSize(int inputW, int inputH, int outputW, int outputH,
                                                                    int *cropX, int *corpY, int *cropW, int *cropH) = 0;
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    virtual int                         getQuickSwitchCmd(void) = 0;
#endif
};


#if 0
class ExynosCameraConfiguration {
public:
    /* Constructor */
    ExynosCameraConfiguration(int cameraId);

    /* Destructor */
    virtual ~ExynosCameraConfiguration();

    status_t addKeyTable(int *keyTable);
    int      getKey(int key);
    /* status_t setKey(int key, int value); */

protected:
    typedef map<int, int> ConfigMap;
};
#endif


}; /* namespace android */

#endif
