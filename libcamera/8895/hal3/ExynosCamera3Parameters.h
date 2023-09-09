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

#ifndef EXYNOS_CAMERA_3_PARAMETERS_H
#define EXYNOS_CAMERA_3_PARAMETERS_H

#include "ExynosCameraParameters.h"
#include "ExynosCamera3SensorInfo.h"
#include "ExynosCameraUtilsModule.h"
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
#include "SecCameraUtil.h"
#endif

#define V4L2_FOURCC_LENGTH 5

namespace android {

class ExynosCamera3Parameters : public ExynosCameraParameters {
public:
    /* Constructor */
    ExynosCamera3Parameters(int cameraId, bool flagCompanion = false, int halVersion = IS_HAL_VER_3_2);

    /* Destructor */
    virtual ~ExynosCamera3Parameters();

    /* Create the instance */
    bool            create(int cameraId);
    /* Destroy the instance */
    bool            destroy(void);
    /* Check if the instance was created */
    bool            flagCreate(void);

    void            setDefaultCameraInfo(void);
    void            setDefaultParameter(void);

public:
    status_t        checkPreviewSize(int previewW, int previewH);
    status_t        checkPreviewFormat(const int streamPreviewFormat);
    status_t        checkPictureSize(int pictureW, int pctureH);
    status_t        checkJpegQuality(int quality);
    status_t        checkThumbnailSize(int thumbnailW, int thumbnailH);
    status_t        checkThumbnailQuality(int quality);
    status_t        checkVideoSize(int newVideoW, int newVideoH);
    status_t        checkCallbackSize(int callbackW, int callbackH);
    status_t        checkCallbackFormat(int callbackFormat);
    status_t        checkPreviewFpsRange(uint32_t minFps, uint32_t maxFps);

    status_t        checkYuvSize(const int width, const int height, const int outputPortId);
    status_t        checkYuvFormat(const int format, const int outputPortId);
#ifdef HAL3_YUVSIZE_BASED_BDS
    status_t        initYuvSizes();
#endif
    int             m_minYuvW;
    int             m_minYuvH;
    status_t        resetMinYuvSize();
    status_t        getMinYuvSize(int* w, int* h)   const;

    void            getYuvSize(int *width, int *height, const int index);
    int             getYuvFormat(const int outputPortId);

    status_t        calcPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcHighResolutionPreviewGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcRecordingGSCRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureRect(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureRect(int originW, int originH, ExynosRect *srcRect, ExynosRect *dstRect);

    status_t        getPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect, bool applyZoom = true);
    status_t        getPreviewBdsSize(ExynosRect *dstRect, bool applyZoom = true);
    status_t        getPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        getPictureBdsSize(ExynosRect *dstRect);
    status_t        getPreviewYuvCropSize(ExynosRect *yuvCropSize);
    status_t        getPictureYuvCropSize(ExynosRect *yuvCropSize);
    status_t        getFastenAeStableSensorSize(int *hwSensorW, int *hwSensorH);
    status_t        getFastenAeStableBcropSize(int *hwBcropW, int *hwBcropH);
    status_t        getFastenAeStableBdsSize(int *hwBdsW, int *hwBdsH);

    status_t        calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcNormalToTpuSize(int srcW, int srcH, int *dstW, int *dstH);
    status_t        calcTpuToNormalSize(int srcW, int srcH, int *dstW, int *dstH);
    status_t        calcPreviewDzoomCropSize(ExynosRect *srcRect, ExynosRect *dstRect);

private:
    /* Sets the dimensions for preview pictures. */
    void            m_setPreviewSize(int w, int h);
    /* Sets the image format for preview pictures. */
    void            m_setPreviewFormat(int colorFormat);
    /* Sets the dimensions for pictures. */
    void            m_setPictureSize(int w, int h);
    /* Sets the image format for picture-related HW. */
    void            m_setHwPictureFormat(int colorFormat);
    /* Sets video's width, height */
    void            m_setVideoSize(int w, int h);
    /* Sets video's color format */
    void            m_setVideoFormat(int colorFormat);
    /* Sets the dimensions for callback pictures. */
    void            m_setCallbackSize(int w, int h);
    /* Sets the image format for callback pictures. */
    void            m_setCallbackFormat(int colorFormat);
    void            m_setYuvSize(const int width, const int height, const int index);
    void            m_setYuvFormat(const int format, const int index);

    /* Sets the dimensions for Sesnor-related HW. */
    void            m_setHwSensorSize(int w, int h);
    /* Sets the dimensions for preview-related HW. */
    void            m_setHwPreviewSize(int w, int h);
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
    /* Sets the scene mode. */
    void            m_setVideoStabilization(bool stabilization);
    /* Sets Bayer Crop Region */
    status_t            m_setParamCropRegion(int zoom, int srcW, int srcH, int dstW, int dstH);
    /* Sets recording mode hint. */
    void            m_setRecordingHint(bool hint);
    /* Sets the rotation angle in degrees relative to the orientation of the camera. */
    void            m_setRotation(int rotation);

/*
 * Additional API.
 */
    /* Sets WDR */
    void            m_setHdrMode(bool hdr);
    /* Sets WDR */
    void            m_setWdrMode(bool wdr);
    /* Sets ODC */
    void            m_setOdcMode(bool toggle);
    /* Sets 3DNR */
    void            m_set3dnrMode(bool toggle);
    /* Sets DRC */
    void            m_setDrcMode(bool toggle);

/*
 * Vendor specific APIs
 */
    /* Sets Intelligent mode */
    status_t        m_setIntelligentMode(int intelligentMode);
    void            m_setVisionMode(bool vision);
    void            m_setVisionModeFps(int fps);
    void            m_setVisionModeAeTarget(int ae);

    void            m_setSWVdisMode(bool swVdis);
    void            m_setSWVdisUIMode(bool swVdisUI);

    /* Sets VT mode */
    void            m_setVtMode(int vtMode);

    /* Sets dual recording mode hint. */
    void            m_setDualRecordingHint(bool hint);
    /* Sets effect hint. */
    void            m_setEffectHint(bool hint);
    /* Sets effect recording mode hint. */
    void            m_setEffectRecordingHint(bool hint);

    void            m_setHighResolutionCallbackMode(bool enable);
    void            m_setHighSpeedRecording(bool highSpeed);
    void            m_setCityId(long long int cityId);
    void            m_setWeatherId(unsigned char cityId);
    status_t        m_setImageUniqueId(const char *uniqueId);
    /* Sets camera angle */
    bool            m_setAngle(int angle);
    /* Sets Top-down mirror */
    bool            m_setTopDownMirror(void);
    /* Sets Left-right mirror */
    bool            m_setLRMirror(void);
    /* Sets Burst mode */
    void            m_setSeriesShotCount(int seriesShotCount);
    bool            m_setAutoFocusMacroPosition(int autoFocusMacroPosition);
    /* Sets Low Light A */
    bool            m_setLLAMode(void);

    /* Sets object tracking */
    bool            m_setObjectTracking(bool toggle);
    /* Start or stop object tracking operation */
    bool            m_setObjectTrackingStart(bool toggle);
    /* Sets x, y position for object tracking operation */
    bool            m_setObjectPosition(int x, int y);

/*
 * Others
 */
    void            m_setRestartPreviewChecked(bool restart);
    bool            m_getRestartPreviewChecked(void);
    void            m_setRestartPreview(bool restart);
    void            m_setExifFixedAttribute(void);
public:

#ifdef SAMSUNG_DOF
    /* Sets Lens Position */
    void            m_setLensPosition(int step);
#endif

    /* Returns the image format for FLITE/3AC/3AP bayer */
    int             getBayerFormat(int pipeId);
    /* Returns the dimensions setting for preview pictures. */
    void            getPreviewSize(int *w, int *h);
    /* Returns the image format for preview frames got from Camera.PreviewCallback. */
    int             getPreviewFormat(void);
    /* Returns the dimension setting for pictures. */
    void            getPictureSize(int *w, int *h);
    /* Returns the image format for picture-related HW. */
    int             getHwPictureFormat(void);
    int             getPictureFormat(void) {return 0;}
    /* Gets video's width, height */
    void            getHwVideoSize(__unused int *w, __unused int *h){};
    /* Gets video's width, height */
    void            getVideoSize(int *w, int *h);
    /* Gets video's color format */
    int             getVideoFormat(void);
    /* Gets the dimensions setting for callback pictures. */
    void            getCallbackSize(int *w, int *h);
    /* Gets the image format for callback pictures. */
    int             getCallbackFormat(void);
    /* Gets the supported sensor sizes. */
    void            getMaxSensorSize(int *w, int *h);
    /* Gets the supported sensor margin. */
    void            getSensorMargin(int *w, int *h);
    /* Gets the supported preview sizes. */
    void            getMaxPreviewSize(int *w, int *h);
    /* Gets the supported picture sizes. */
    void            getMaxPictureSize(int *w, int *h);
    /* Gets the supported video frame sizes that can be used by MediaRecorder. */
    void            getMaxVideoSize(int *w, int *h);
    /* Gets the supported jpeg thumbnail sizes. */
    bool            getSupportedJpegThumbnailSizes(int *w, int *h);

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
    /* Gets the current focus mode setting. */
    int             getFocusMode(void);
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
    /* Sets current zoom value. */
    status_t        setZoomLevel(int value);
    /* Gets current zoom value. */
    int             getZoomLevel(void);
    /* Set the current crop region info */
    status_t        setCropRegion(int x, int y, int w, int h);
    /* Returns the recording mode hint. */
    bool            getRecordingHint(void);
    /* Gets the rotation angle in degrees relative to the orientation of the camera. */
    int             getRotation(void);

/*
 * Additional API.
 */

    /* Gets ExposureTime for capture */
    uint64_t        getCaptureExposureTime(void){return 0L;};
    int32_t         getLongExposureShotCount(void){return 0;};

    /* Gets WDR */
    bool            getHdrMode(void);
    /* Gets ODC */
    bool            getOdcMode(void);
    /* Gets Shot mode */
    int             getShotMode(void);
    /* Gets Preview Buffer Count */
    int             getPreviewBufferCount(void);
    /* Sets Preview Buffer Count */
    void            setPreviewBufferCount(int previewBufferCount);
    /* Get BatchSize for HFR */
    int             getBatchSize(enum pipeline pipeId){return 1;};

    /* Gets 3DNR */
    bool            get3dnrMode(void);
    /* Gets DRC */
    bool            getDrcMode(void);
    /* Gets TPU enable case or not */
    bool            getTpuEnabledMode(void);

#ifdef SAMSUNG_COMPANION
     /* Gets RT DRC */
    enum companion_drc_mode            getRTDrc(void);
     /* Gets PAF */
    enum companion_paf_mode            getPaf(void);
     /* Gets RT HDR */
    enum companion_wdr_mode            getRTHdr(void);
#endif

/*
 * Vendor specific APIs
 */

    /* Gets Intelligent mode */
    int             getIntelligentMode(void);
    bool            getVisionMode(void);
    int             getVisionModeFps(void);
    int             getVisionModeAeTarget(void);

    bool            isSWVdisMode(void); /* need to change name */
    bool            isSWVdisModeWithParam(int nPreviewW, int nPreviewH);
    bool            getSWVdisMode(void);
    bool            getSWVdisUIMode(void);

    bool            getHWVdisMode(void);
    int             getHWVdisFormat(void);

    /* Gets VT mode */
    int             getVtMode(void);

    /* Set/Get Dual mode */
    void            setDualMode(bool toggle);
    bool            getDualMode(void);
    /* Returns the dual recording mode hint. */
    bool            getDualRecordingHint(void);
    /* Returns the effect hint. */
    bool            getEffectHint(void);
    /* Returns the effect recording mode hint. */
    bool            getEffectRecordingHint(void);

    void            setFastFpsMode(int fpsMode);
    int             getFastFpsMode(void);

    bool            getHighResolutionCallbackMode(void);
    bool            getSamsungCamera(void);
    void            setSamsungCamera(bool value);
    bool            getHighSpeedRecording(void);
    bool            getScalableSensorMode(void);
    void            setScalableSensorMode(bool scaleMode);
#ifdef USE_BINNING_MODE
    int *           getBinningSizeTable(void);
#endif
    long long int   getCityId(void);
    unsigned char   getWeatherId(void);
    /* Gets ImageUniqueId */
    const char     *getImageUniqueId(void);
#ifdef SAMSUNG_TN_FEATURE
    void            setImageUniqueId(char *uniqueId);
#endif
    /* Gets camera angle */
    int             getAngle(void);

    void            setFlipHorizontal(int val);
    int             getFlipHorizontal(void);
    void            setFlipVertical(int val);
    int             getFlipVertical(void);

    /* Gets Burst mode */
    int             getSeriesShotCount(void);
    /* Return callback need CSC */
    bool            getCallbackNeedCSC(void);
    /* Return callback need copy to rendering */
    bool            getCallbackNeedCopy2Rendering(void);

    /* Gets Illumination */
    int             getIllumination(void);
    /* Gets Low Light Shot */
    int             getLLS(struct camera2_shot_ext *shot);
    /* Gets Low Light A */
    bool            getLLAMode(void);
    /* Sets the device orientation angle in degrees to camera FW for FD scanning property. */
    bool            setDeviceOrientation(int orientation);
    /* Gets the device orientation angle in degrees . */
    int             getDeviceOrientation(void);
    /* Gets the FD orientation angle in degrees . */
    int             getFdOrientation(void);
    /* Gets object tracking */
    bool            getObjectTracking(void);
    /* Gets status of object tracking operation */
    int             getObjectTrackingStatus(void);
    /* Gets smart auto */
    bool            getSmartAuto(void);
    /* Gets the status of smart auto operation */
    int             getSmartAutoStatus(void);
    /* Gets beauty shot */
    bool            getBeautyShot(void);

/*
 * Static info
 */

    /* Gets the maximum zoom value allowed for snapshot. */
    int             getMaxZoomLevel(void);

    /* Gets the supported preview fps range. */
    bool            getMaxPreviewFpsRange(int *min, int *max);

    /* Gets max zoom ratio */
    float           getMaxZoomRatio(void);
    /* Gets zoom ratio */
    float           getZoomRatio(int zoom);
    /* Gets current zoom ratio */
    float           getZoomRatio(void);

    /* Returns true if video snapshot is supported. */
    bool            getVideoSnapshotSupported(void);

    /* Returns true if video stabilization is supported. */
    bool            getVideoStabilizationSupported(void);

    /* Returns true if zoom is supported. */
    bool            getZoomSupported(void);

    /* Gets FocalLengthIn35mmFilm */
    int             getFocalLengthIn35mmFilm(void);

    bool            isScalableSensorSupported(void);

    status_t        getFixedExifInfo(exif_attribute_t *exifInfo);
    void            setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                            ExynosRect          *PictureRect,
                                            ExynosRect          *thumbnailRect,
                                            camera2_shot_t      *shot);

    debug_attribute_t *getDebugAttribute(void);

#ifdef DEBUG_RAWDUMP
    bool           checkBayerDumpEnable(void);
#endif/* DEBUG_RAWDUMP */
#ifdef USE_BINNING_MODE
    int             getBinningMode(void);
#endif /* USE_BINNING_MODE */
public:
    bool            DvfsLock();
    bool            DvfsUnLock();

    void            updateHwSensorSize(void);
    void            updateBinningScaleRatio(void);
    void            updateBnsScaleRatio(void);

    void            setHwPreviewStride(int stride);
    int             getHwPreviewStride(void);

    status_t        duplicateCtrlMetadata(void *buf);

    status_t        setRequestDis(int value);

    status_t        setDisEnable(bool enable);
    status_t        setDrcEnable(bool enable);
    status_t        setDnrEnable(bool enable);
    status_t        setFdEnable(bool enable);

    bool            getDisEnable(void);
    bool            getDrcEnable(void);
    bool            getDnrEnable(void);
    bool            getFdEnable(void);

    status_t        setFdMode(enum facedetect_mode mode);
    status_t        getFdMeta(bool reprocessing, void *buf);
    bool            getUHDRecordingMode(void);

private:
    bool            m_isSupportedPreviewSize(const int width, const int height);
    bool            m_isSupportedPictureSize(const int width, const int height);
    bool            m_isSupportedVideoSize(const int width, const int height);
    bool            m_isHighResolutionCallbackSize(const int width, const int height);
    void            m_isHighResolutionMode(const CameraParameters& params);
    status_t        m_getPreviewSizeList(int *sizeList);

    void            m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH);
    void            m_getScalableSensorSize(int *newSensorW, int *newSensorH);

    void            m_initMetadata(void);

    bool            m_isUHDRecordingMode(void);

/*
 * Vendor specific adjust function
 */
private:
    status_t        m_getPreviewBdsSize(ExynosRect *dstRect);
    status_t        m_adjustPreviewSize(int *newPreviewW, int *newPreviewH,
                                        int *newCalPreviewW, int *newCalPreviewH);
    status_t        m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                        int *newHwPictureW, int *newHwPictureH);
    bool            m_adjustScalableSensorMode(const int scaleMode);
    void            m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH);
    void            m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void            m_getCropRegion(int *x, int *y, int *w, int *h);

    /* for initial 120fps start due to quick launch */
/*
    void            set120FpsState(enum INIT_120FPS_STATE state);
    void            clear120FpsState(enum INIT_120FPS_STATE state);
    bool            flag120FpsStart(void);
    bool            setSensorFpsAfter120fps(void);
    void            setInitValueAfter120fps(bool isAfter);
*/

    status_t        m_setBinningScaleRatio(int ratio);
    status_t        m_setBnsScaleRatio(int ratio);
    void            m_setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                              ExynosRect          *PictureRect,
                                              ExynosRect          *thumbnailRect,
                                              camera2_shot_t      *shot);

    void            m_getV4l2Name(char* colorName, size_t length, int colorFormat);

public:
    int                 getCameraId(void);
    /* Gets the detected faces areas. */
    int                 getDetectedFacesAreas(int num, int *id,
                                              int *score, ExynosRect *face,
                                              ExynosRect *leftEye, ExynosRect *rightEye,
                                              ExynosRect *mouth);
    /* Gets the detected faces areas. (Using ExynosRect2) */
    int                 getDetectedFacesAreas(int num, int *id,
                                              int *score, ExynosRect2 *face,
                                              ExynosRect2 *leftEye, ExynosRect2 *rightEye,
                                              ExynosRect2 *mouth);

    void                enableMsgType(int32_t msgType);
    void                disableMsgType(int32_t msgType);
    bool                msgTypeEnabled(int32_t msgType);

    status_t            setFrameSkipCount(int count);
    status_t            getFrameSkipCount(int *count);
    int                 getFrameSkipCount(void);

    ExynosCameraActivityControl *getActivityControl(void);

    void                getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void                setSetfileYuvRange(void);
    void                setSetfileYuvRange(bool flagReprocessing, int setfile, int yuvRange);
    status_t            checkSetfileYuvRange(void);

    void                setUseDynamicBayer(bool enable);
    bool                getUseDynamicBayer(void);
    void                setUseDynamicBayerVideoSnapShot(bool enable);
    bool                getUseDynamicBayerVideoSnapShot(void);
    void                setUseDynamicBayer120FpsVideoSnapShot(bool enable);
    bool                getUseDynamicBayer120FpsVideoSnapShot(void);
    void                setUseDynamicBayer240FpsVideoSnapShot(bool enable);
    bool                getUseDynamicBayer240FpsVideoSnapShot(void);
    void                setUseDynamicScc(bool enable);
    bool                getUseDynamicScc(void);

    void                setUseFastenAeStable(bool enable);
    bool                getUseFastenAeStable(void);

    void                setUsePureBayerReprocessing(bool enable);
    bool                getUsePureBayerReprocessing(void);

    int32_t             getReprocessingBayerMode(void);

    void                setAdaptiveCSCRecording(bool enable);
    bool                getAdaptiveCSCRecording(void);
    bool                doCscRecording(void);

    uint32_t            getBinningScaleRatio(void);
    uint32_t            getBnsScaleRatio(void);
    /* Sets the dimensions for Sesnor-related BNS. */
    void                setBnsSize(int w, int h);
    /* Gets the dimensions for Sesnor-related BNS. */
    void                getBnsSize(int *w, int *h);

    /*
     * This must call before startPreview(),
     * this update h/w setting at once.
     */
    bool                updateTpuParameters(void);

    int                 getHalVersion(void);
    void                setHalVersion(int halVersion);
    struct ExynosSensorInfoBase *getSensorStaticInfo();

    int32_t             getYuvStreamMaxNum(void);

#ifdef SAMSUNG_DOF
    int                 getMoveLensTotal(void);
    void                setMoveLensCount(int count);
    void                setMoveLensTotal(int count);
#endif

#ifdef BURST_CAPTURE
    int                 getSeriesShotSaveLocation(void);
    void                setSeriesShotSaveLocation(int ssaveLocation);
    char                *getSeriesShotFilePath(void);
#endif
    int                 getSeriesShotMode(void);

    int                 getHalPixelFormat(void);
    int                 convertingHalPreviewFormat(int previewFormat, int yuvRange);

    void                setDvfsLock(bool lock);
    bool                getDvfsLock(void);

#ifdef SAMSUNG_LLV
    void                setLLV(bool enable);
    bool                getLLV(void);
#endif
    bool                getSensorOTFSupported(void);
    bool                isReprocessing(void);
    bool                isSccCapture(void);

    /* True if private reprocessing or YUV reprocessing is supported */
    bool                isSupportZSLInput(void);

    /* H/W Chain Scenario Infos */
    bool                isFlite3aaOtf(void);
    bool                is3aaIspOtf(void);
    bool                isIspTpuOtf(void);
    bool                isIspMcscOtf(void);
    bool                isTpuMcscOtf(void);
    bool                isReprocessing3aaIspOTF(void);
    bool                isReprocessingIspTpuOtf(void);
    bool                isReprocessingIspMcscOtf(void);
    bool                isReprocessingTpuMcscOtf(void);

    bool                isSingleChain(void);

    bool                isUse3aaInputCrop(void);
    bool                isUseIspInputCrop(void);
    bool                isUseMcscInputCrop(void);
    bool                isUseReprocessing3aaInputCrop(void);
    bool                isUseReprocessingIspInputCrop(void);
    bool                isUseReprocessingMcscInputCrop(void);
    bool                isUseEarlyFrameReturn(void);
    bool                isUseHWFC(void);

    bool                getSupportedZoomPreviewWIthScaler(void);
    void                setZoomPreviewWIthScaler(bool enable);
    bool                getZoomPreviewWIthScaler(void);

    bool                getReallocBuffer();
    bool                setReallocBuffer(bool enable);

    bool                setConfig(struct ExynosConfigInfo* config);
    struct ExynosConfigInfo* getConfig();

    bool                setConfigMode(uint32_t mode);
    int                 getConfigMode();
    /* Sets Shot mode */
    void                m_setShotMode(int shotMode);
#ifdef LLS_CAPTURE
    void                setLLSOn(uint32_t enable);
    bool                getLLSOn(void);
    void                m_setLLSShotMode();
#ifdef SET_LLS_CAPTURE_SETFILE
    void                setLLSCaptureOn(bool enable);
    int                 getLLSCaptureOn();
#endif
#endif
#ifdef SR_CAPTURE
    void                setSROn(uint32_t enable);
    bool                getSROn(void);
#endif
#ifdef OIS_CAPTURE
    void                setOISCaptureModeOn(bool enable);
    bool                getOISCaptureModeOn(void);
#endif
#ifdef SAMSUNG_OIS
    void                m_setOIS(enum optical_stabilization_mode ois);
    enum optical_stabilization_mode getOIS(void);
    void                setOISNode(ExynosCameraNode *node);
    void                setOISMode(void);
    void                setOISModeSetting(bool enable);
    int                 getOISModeSetting(void);
#endif
#ifdef RAWDUMP_CAPTURE
    void                setRawCaptureModeOn(bool enable);
    bool                getRawCaptureModeOn(void);
#endif
    void                setZoomActiveOn(bool enable);
    bool                getZoomActiveOn(void);
    status_t            setMarkingOfExifFlash(int flag);
    int                 getMarkingOfExifFlash(void);
    bool                increaseMaxBufferOfPreview(void);

    status_t            setYuvBufferCount(const int count, const int index);
    int                 getYuvBufferCount(const int index);

    void                setHighSpeedMode(uint32_t mode);

//Added
    int                 getHDRDelay(void) { return HDR_DELAY; }
    int                 getReprocessingBayerHoldCount(void) { return REPROCESSING_BAYER_HOLD_COUNT; }
    int                 getFastenAeFps(void) { return FASTEN_AE_FPS; }
    int                 getPerFrameControlPipe(void) {return PERFRAME_CONTROL_PIPE; }
    int                 getPerFrameControlReprocessingPipe(void) {return PERFRAME_CONTROL_REPROCESSING_PIPE; }
    int                 getPerFrameInfo3AA(void) { return PERFRAME_INFO_3AA; };
    int                 getPerFrameInfoIsp(void) { return PERFRAME_INFO_ISP; };
    int                 getPerFrameInfoDis(void) { return PERFRAME_INFO_DIS; };
    int                 getPerFrameInfoReprocessingPure3AA(void) { return PERFRAME_INFO_PURE_REPROCESSING_3AA; }
    int                 getPerFrameInfoReprocessingPureIsp(void) { return PERFRAME_INFO_PURE_REPROCESSING_ISP; }
    int                 getScalerNodeNumPicture(void) { return PICTURE_GSC_NODE_NUM;}
    int                 getScalerNodeNumPreview(void) { return PREVIEW_GSC_NODE_NUM;}
    int                 getScalerNodeNumVideo(void) { return VIDEO_GSC_NODE_NUM;}
    bool                isOwnScc(int cameraId);
    bool                needGSCForCapture(int camId) { return (camId == CAMERA_ID_BACK) ? USE_GSC_FOR_CAPTURE_BACK : USE_GSC_FOR_CAPTURE_FRONT; }
    bool                getSetFileCtlMode(void);
    bool                getSetFileCtl3AA_ISP(void);
    bool                getSetFileCtl3AA(void);
    bool                getSetFileCtlISP(void);
    bool                getSetFileCtlSCP(void);
    bool                getOutPutFormatNV21Enable(void) { return false; };


#ifdef SAMSUNG_COMPANION
    void    setUseCompanion(bool use);
    bool    getUseCompanion();
#endif

    virtual void setNormalBestFrameCount(uint32_t) {}
    virtual uint32_t getNormalBestFrameCount() {return 0;}
    virtual void resetNormalBestFrameCount() {}
    virtual void setSCPFrameCount(uint32_t) {}
    virtual uint32_t getSCPFrameCount() {return 0;}
    virtual void resetSCPFrameCount() {}
    virtual void setBayerFrameCount(uint32_t) {}
    virtual uint32_t getBayerFrameCount() {return 0;}
    virtual void resetBayerFrameCount() {}
    virtual bool getUseBestPic() {return false;}
    virtual void setLLSCaptureCount(int) {}
    virtual int getLLSCaptureCount() {return 0;};
    status_t    getDepthMapSize(int *depthMapW, int *depthMapH);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    /* Set/Get Dual camera mode */
    void                setDualCameraMode(bool toggle);
    bool                getDualCameraMode(void);
    bool                isFusionEnabled(void);
    status_t            getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect);
    status_t            setFusionInfo(camera2_shot_ext *shot_ext);
    DOF                *getDOF(void);
#ifdef USE_CP_FUSION_LIB
    char               *readFusionCalData(int *readSize);
    void                setFusionCalData(char *addr, int size);
    char               *getFusionCalData(int *size);
#endif // USE_CP_FUSION_LIB
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

#ifdef SUPPORT_DEPTH_MAP
    bool        getUseDepthMap(void);
    status_t    checkUseDepthMap(void);
    void        m_setUseDepthMap(bool useDepthMap);
    bool        getDepthCallbackOnPreview(void) {return 0;};
    bool        getDepthCallbackOnCapture(void) {return 0;};
#endif
#ifdef USE_BDS_2_0_480P_YUV
    void        clearYuvSizeSetupFlag(void);
#endif

private:
    int                         m_cameraId;
    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    struct camera2_shot_ext     m_metadata;

    struct exynos_camera_info   m_cameraInfo;
    struct ExynosSensorInfoBase    *m_staticInfo;

    exif_attribute_t            m_exifInfo;
    debug_attribute_t           mDebugInfo;

    int32_t                     m_enabledMsgType;
    mutable Mutex               m_msgLock;

    float                       m_calculatedHorizontalViewAngle;
    /* frame skips */
    ExynosCameraCounter         m_frameSkipCounter;

    mutable Mutex               m_parameterLock;

    ExynosCameraActivityControl *m_activityControl;

    /* Flags for camera status */
    bool                        m_previewSizeChanged;
    bool                        m_flagCheckDualMode;
    bool                        m_flagRestartPreviewChecked;
    bool                        m_flagRestartPreview;
    int                         m_fastFpsMode;

    bool                        m_flagVideoStabilization;
    bool                        m_flag3dnrMode;

#ifdef LLS_CAPTURE
    bool                        m_flagLLSOn;
    int                         m_LLSCaptureOn;
#endif
#ifdef SR_CAPTURE
    bool                        m_flagSRSOn;
#endif
#ifdef OIS_CAPTURE
    bool                        m_flagOISCaptureOn;
    bool                        m_llsValue;
#endif
#ifdef RAWDUMP_CAPTURE
    bool                        m_flagRawCaptureOn;
#endif
    bool                        m_flagCheckRecordingHint;

    int                         m_setfile;
    int                         m_yuvRange;
    int                         m_setfileReprocessing;
    int                         m_yuvRangeReprocessing;
#ifdef USE_BINNING_MODE
    int                         m_binningProperty;
#endif
    bool                        m_useSizeTable;
    bool                        m_useDynamicBayer;
    bool                        m_useDynamicBayerVideoSnapShot;
    bool                        m_useDynamicBayer120FpsVideoSnapShot;
    bool                        m_useDynamicBayer240FpsVideoSnapShot;
    bool                        m_useDynamicScc;
    bool                        m_useFastenAeStable;
    bool                        m_usePureBayerReprocessing;
    bool                        m_useAdaptiveCSCRecording;
    bool                        m_dvfsLock;
    int                         m_previewBufferCount;

    bool                        m_reallocBuffer;
    mutable Mutex               m_reallocLock;

    struct ExynosConfigInfo     *m_exynosconfig;

#ifdef SAMSUNG_LLV
    bool                        m_isLLVOn;
#endif

#ifdef SAMSUNG_DOF
    int                         m_curLensStep;
    int                         m_curLensCount;
#endif
#ifdef SAMSUNG_OIS
    ExynosCameraNode        *m_oisNode;
    bool                        m_setOISmodeSetting;
#endif
    bool                        m_zoom_activated;
    int                         m_firing_flash_marking;

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    bool                        m_romReadThreadDone;
    bool                        m_use_companion;
#endif
    uint64_t                    m_exposureTimeCapture;

    bool                        m_zoomWithScaler;
    int                         m_halVersion;

    int                         m_yuvBufferCount[3];
#ifdef SUPPORT_DEPTH_MAP
    bool                        m_flaguseDepthMap;
#endif
#ifdef USE_BDS_2_0_480P_YUV
    bool                        m_yuvSizeSetup[3];
#endif
};


}; /* namespace android */

#endif
