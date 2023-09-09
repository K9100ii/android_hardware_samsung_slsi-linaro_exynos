/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_1_PARAMETERS_H
#define EXYNOS_CAMERA_1_PARAMETERS_H

#if 0
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

#include "ExynosCameraCommonDefine.h"
#include "ExynosCameraCommonEnum.h"
//#include "ExynosCameraConfig.h"

#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraCounter.h"
#include "fimc-is-metadata.h"
#include "ExynosRect.h"
#include "exynos_format.h"
#include "ExynosExif.h"
#include "ExynosCameraUtils.h"
#include "ExynosCameraUtilsModule.h"
#include "ExynosCameraActivityControl.h"
#include "ExynosCameraAutoTimer.h"
#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraParameters.h"
#endif
#ifdef SUPPORT_SW_VDIS
#include "SecCameraSWVdis_3_0.h"
#endif /*SUPPORT_SW_VDIS*/
#ifdef SAMSUNG_OIS
#include"ExynosCameraNode.h"
#endif
#endif

#include "ExynosCameraConfig.h"
#include "ExynosCameraParameters.h"

#include "ExynosCameraUtilsModule.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraNode.h"

#ifdef SAMSUNG_UNI_API
#include "uni_api_wrapper.h"
#endif

#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
#include "SecCameraUtil.h"
#endif
#ifdef SAMSUNG_DNG
#include "SecCameraDng.h"
#endif

#define IS_WBLEVEL_DEFAULT (4)
#define FW_CUSTOM_OFFSET (1)

namespace android {

class ExynosCamera1Parameters : public ExynosCameraParameters {
public:
    /* Constructor */
    ExynosCamera1Parameters(int cameraId, bool flagCompanion = false, int halVersion = IS_HAL_VER_1_0, bool flagDummy = false);

    /* Destructor */
    virtual ~ExynosCamera1Parameters();

    /* Create the instance */
    bool            create(int cameraId);
    /* Destroy the instance */
    bool            destroy(void);
    /* Check if the instance was created */
    bool            flagCreate(void);

    void            setDefaultCameraInfo(void);
    void            setDefaultParameter(void);
    status_t        setRestartParameters(const CameraParameters& params);
    bool            checkRestartPreview(const CameraParameters& params);

    void            operator=(const ExynosCamera1Parameters& params);

public:
    status_t        checkVisionMode(const CameraParameters& params);
    status_t        checkRecordingHint(const CameraParameters& params);
    status_t        checkDualMode(const CameraParameters& params);
    status_t        checkDualRecordingHint(const CameraParameters& params);
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    status_t        checkDualCameraMode(const CameraParameters& params);
#endif
    status_t        checkEffectHint(const CameraParameters& params);
    status_t        checkEffectRecordingHint(const CameraParameters& params);
    /*
     * Check preview frame rate
     * both previewFpsRange(new API) and previewFrameRate(Old API)
     */
    status_t        checkPreviewFps(const CameraParameters& params);
    status_t        checkPreviewFpsRange(const CameraParameters& params);
    status_t        checkPreviewFrameRate(const CameraParameters& params);
    status_t        checkVideoSize(const CameraParameters& params);
    status_t        checkFastFpsMode(const CameraParameters& params);
    status_t        checkVideoStabilization(const CameraParameters& params);
    status_t        checkSWVdisMode(const CameraParameters& params);
    status_t        checkHWVdisMode(const CameraParameters& params);
    status_t        checkPreviewSize(const CameraParameters& params);
    status_t        checkPreviewFormat(const CameraParameters& params);
    status_t        checkPictureSize(const CameraParameters& params);
    status_t        checkPictureFormat(const CameraParameters& params);
    status_t        checkJpegQuality(const CameraParameters& params);
    status_t        checkThumbnailSize(const CameraParameters& params);
    status_t        checkThumbnailQuality(const CameraParameters& params);
    status_t        check3dnrMode(const CameraParameters& params);
    status_t        checkDrcMode(const CameraParameters& params);
    status_t        checkOdcMode(const CameraParameters& params);
    status_t        checkZoomLevel(const CameraParameters& params);
    status_t        checkRotation(const CameraParameters& params);
    status_t        checkAutoExposureLock(const CameraParameters& params);
    status_t        checkExposureCompensation(const CameraParameters& params);
    status_t        checkExposureTime(const CameraParameters& params);
    status_t        checkMeteringAreas(const CameraParameters& params);
    status_t        checkMeteringMode(const CameraParameters& params);
    status_t        checkAntibanding(const CameraParameters& params);
    status_t        checkSceneMode(const CameraParameters& params);
    status_t        checkFocusMode(const CameraParameters& params);
#ifdef SAMSUNG_MANUAL_FOCUS
    status_t        checkFocusDistance(const CameraParameters& params);
#endif
    status_t        checkFlashMode(const CameraParameters& params);
    status_t        checkWhiteBalanceMode(const CameraParameters& params);
    status_t        checkAutoWhiteBalanceLock(const CameraParameters& params);
#ifdef SAMSUNG_FOOD_MODE
    status_t        checkWbLevel(const CameraParameters& params);
    void            m_setWbLevel(int32_t value);
    int32_t         getWbLevel(void);
#endif
    status_t        checkWbKelvin(const CameraParameters& params);
    void            m_setWbKelvin(int32_t value);
    int32_t         getWbKelvin(void);
    status_t        checkFocusAreas(const CameraParameters& params);
    status_t        checkColorEffectMode(const CameraParameters& params);
    status_t        checkGpsAltitude(const CameraParameters& params);
    status_t        checkGpsLatitude(const CameraParameters& params);
    status_t        checkGpsLongitude(const CameraParameters& params);
    status_t        checkGpsProcessingMethod(const CameraParameters& params);
    status_t        checkGpsTimeStamp(const CameraParameters& params);
    status_t        checkCityId(const CameraParameters& params);
    status_t        checkWeatherId(const CameraParameters& params);
    status_t        checkBrightness(const CameraParameters& params);
    status_t        checkSaturation(const CameraParameters& params);
    status_t        checkSharpness(const CameraParameters& params);
    status_t        checkHue(const CameraParameters& params);
#ifdef SAMSUNG_COMPANION
    status_t        checkRTDrc(const CameraParameters& params);
    status_t        checkPaf(const CameraParameters& params);
    status_t        checkRTHdr(const CameraParameters& params);
    status_t        checkSceneRTHdr(bool onoff);
#endif
    status_t        checkIso(const CameraParameters& params);
    status_t        checkContrast(const CameraParameters& params);
    status_t        checkHdrMode(const CameraParameters& params);
    status_t        checkWdrMode(const CameraParameters& params);
    status_t        checkShotMode(const CameraParameters& params);
    status_t        checkAntiShake(const CameraParameters& params);
    status_t        checkVtMode(const CameraParameters& params);
    status_t        checkVRMode(const CameraParameters& params);
    status_t        checkGamma(const CameraParameters& params);
    status_t        checkSlowAe(const CameraParameters& params);
    status_t        checkScalableSensorMode(const CameraParameters& params);
    status_t        checkImageUniqueId(const CameraParameters& params);
    status_t        checkSeriesShotMode(const CameraParameters& params);
#ifdef BURST_CAPTURE
    status_t        checkSeriesShotFilePath(const CameraParameters& params);
#endif

#ifdef SAMSUNG_LLV
    status_t        checkLLV(const CameraParameters& params);
#endif

#ifdef SAMSUNG_DOF
    status_t	    checkMoveLens(const CameraParameters& params);
#endif
#ifdef SUPPORT_DEPTH_MAP
    status_t        checkUseDepthMap(void);
#endif
#ifdef SAMSUNG_HYPER_MOTION
    status_t        checkHyperMotionSpeed(const CameraParameters& params);
#endif
#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    status_t        checkCameraDirection(const CameraParameters& params);
#endif
#ifdef SAMSUNG_LENS_DC
    status_t        checkLensDCMode(const CameraParameters& params);
#endif

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
    status_t        getDepthMapSize(int *depthMapW, int *depthMapH);
#ifdef SUPPORT_DEPTH_MAP
    bool            getUseDepthMap(void);
    bool            getDepthCallbackOnPreview(void);
    bool            getDepthCallbackOnCapture(void);
    void            setDepthCallbackOnPreview(bool enable);
    void            setDepthCallbackOnCapture(bool enable);
    void            setDisparityMode(enum companion_disparity_mode disparity_mode);
#endif

    status_t        calcPreviewBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBayerCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPreviewBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcPictureBDSSize(ExynosRect *srcRect, ExynosRect *dstRect);
    status_t        calcNormalToTpuSize(int srcW, int srcH, int *dstW, int *dstH);
    status_t        calcTpuToNormalSize(int srcW, int srcH, int *dstW, int *dstH);
    status_t        calcPreviewDzoomCropSize(ExynosRect *srcRect, ExynosRect *dstRect);
    /* Sets the auto-exposure lock state. */
    void            m_setAutoExposureLock(bool lock);
    /* Sets the auto-white balance lock state. */
    void            m_setAutoWhiteBalanceLock(bool value);

private:
#ifdef SUPPORT_DEPTH_MAP
    void            m_setUseDepthMap(bool useDepthMap);
#endif
    /* Sets the dimensions for preview pictures. */
    void            m_setPreviewSize(int w, int h);
    /* Sets the image format for preview pictures. */
    void            m_setPreviewFormat(int colorFormat);
    /* Sets the dimensions for pictures. */
    void            m_setPictureSize(int w, int h);
    /* Sets the image format for pictures. */
    void            m_setPictureFormat(int colorFormat);
    /* Sets the image format for picture-related HW. */
    void            m_setHwPictureFormat(int colorFormat);
    /* Sets hw video's width, height */
    void            m_setHwVideoSize(int w, int h);
    /* Sets video's width, height */
    void            m_setVideoSize(int w, int h);
    /* Sets video's color format */
    void            m_setVideoFormat(int colorFormat);

    /* Sets the dimensions for Sesnor-related HW. */
    void            m_setHwSensorSize(int w, int h);
    /* Sets the dimensions for preview-related HW. */
    void            m_setHwPreviewSize(int w, int h);
    /* Sets the image format for preview-related HW. */
    void            m_setHwPreviewFormat(int colorFormat);
    /* Sets the dimensions for picture-related HW. */
    void            m_setHwPictureSize(int w, int h);
    /* Sets HW Bayer Crop Size */
    void            m_setHwBayerCropRegion(int w, int h, int x, int y);
    /* Sets the antibanding. */
    void            m_setAntibanding(int value);

    /* Sets the current color effect setting. */
    void            m_setColorEffectMode(int effect);

    /* Sets the exposure compensation index. */
    void            m_setExposureCompensation(int value);
    /* Sets the flash mode. */
    void            m_setFlashMode(int flashMode);
    /* Sets focus areas. */
    void            m_setFocusAreas(uint32_t numValid, ExynosRect* rects, int *weights);
    /* Sets focus areas. (Using ExynosRect2) */
    void            m_setFocusAreas(uint32_t numValid, ExynosRect2* rect2s, int *weights);
    /* Sets the focus mode. */
    void            m_setFocusMode(int focusMode);
#ifdef SAMSUNG_MANUAL_FOCUS
    void            m_setFocusDistance(int32_t distance);
#endif
    /* Sets Jpeg quality of captured picture. */
    void            m_setJpegQuality(int quality);
    /* Sets the quality of the EXIF thumbnail in Jpeg picture. */
    void            m_setThumbnailQuality(int quality);
    /* Sets the dimensions for EXIF thumbnail in Jpeg picture. */
    void            m_setThumbnailSize(int w, int h);
    /* Sets ExposureTime for preview. */
    void            m_setExposureTime(uint64_t exposureTime);
    /* Sets metering areas. */
    void            m_setMeteringAreas(uint32_t num, ExynosRect  *rects, int *weights);
    /* Sets metering areas.(Using ExynosRect2) */
    void            m_setMeteringAreas(uint32_t num, ExynosRect2 *rect2s, int *weights);
    /* Sets the frame rate range for preview. */
    void            m_setPreviewFpsRange(uint32_t min, uint32_t max);
    /* Sets the scene mode. */
    void            m_setSceneMode(int sceneMode);
    /* Enables and disables video stabilization. */
    void            m_setVideoStabilization(bool stabilization);
    /* Sets the white balance. */
    status_t        m_setWhiteBalanceMode(int whiteBalance);
    /* Sets Bayer Crop Region */
    status_t            m_setParamCropRegion(int zoom, int srcW, int srcH, int dstW, int dstH);
    /* Sets recording mode hint. */
    void            m_setRecordingHint(bool hint);
    /* Sets GPS altitude. */
    void            m_setGpsAltitude(double altitude);
    /* Sets GPS latitude coordinate. */
    void            m_setGpsLatitude(double latitude);
    /* Sets GPS longitude coordinate. */
    void            m_setGpsLongitude(double longitude);
    /* Sets GPS processing method. */
    void            m_setGpsProcessingMethod(const char *gpsProcessingMethod);
    /* Sets GPS timestamp. */
    void            m_setGpsTimeStamp(long timeStamp);
    /* Sets the rotation angle in degrees relative to the orientation of the camera. */
    void            m_setRotation(int rotation);

/*
 * Additional API.
 */
    /* Sets metering areas. */
    void            m_setMeteringMode(int meteringMode);
    /* Sets brightness */
    void            m_setBrightness(int brightness);
#ifdef SAMSUNG_COMPANION
    /* Sets RT DRC */
    void            m_setRTDrc(enum  companion_drc_mode rtdrc);
    /* Sets PAF */
    void            m_setPaf(enum companion_paf_mode paf);
    /* Sets RT HDR */
    void            m_setRTHdr(enum companion_wdr_mode rthdr);
#endif
    /* Sets ISO */
    void            m_setIso(uint32_t iso);
    /* Sets Contrast */
    void            m_setContrast(uint32_t contrast);
    /* Sets Saturation */
    void            m_setSaturation(int saturation);
    /* Sets Sharpness */
    void            m_setSharpness(int sharpness);
    /* Sets Hue */
    void            m_setHue(int hue);
    /* Sets WDR */
    void            m_setHdrMode(bool hdr);
    /* Sets WDR */
    void            m_setWdrMode(bool wdr);
    /* Sets anti shake */
    void            m_setAntiShake(bool toggle);
    /* Sets gamma */
    void            m_setGamma(bool gamma);
    /* Sets ODC */
    void            m_setOdcMode(bool toggle);
    /* Sets Slow AE */
    void            m_setSlowAe(bool slowAe);
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

    /* Sets VR mode */
    void            m_setVRMode(int vrMode);

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
    /* Sets smart auto */
    bool            m_setSmartAuto(bool toggle);
    /* Sets beauty shot */
    bool            m_setBeautyShot(bool toggle);

/*
 * Others
 */
    void            m_setRestartPreviewChecked(bool restart);
    bool            m_getRestartPreviewChecked(void);
    void            m_setRestartPreview(bool restart);
    void            m_setExifFixedAttribute(void);
    int             m_convertMetaCtlAwbMode(struct camera2_shot_ext *shot_ext);

public:

#ifdef SAMSUNG_DOF
    /* Sets Lens Position */
    void            m_setLensPosition(int step);
#endif
#ifdef SAMSUNG_HRM
    void            m_setHRM(int ir_data, int flicker_data, int status);
    void            m_setHRM_Hint(bool flag);
#endif
#ifdef SAMSUNG_LIGHT_IR
    void            m_setLight_IR(SensorListenerEvent_t data);
    void            m_setLight_IR_Hint(bool flag);
#endif
#ifdef SAMSUNG_GYRO
    void            m_setGyro(SensorListenerEvent_t data);
    void            m_setGyroHint(bool flag);
#endif
#ifdef SAMSUNG_ACCELEROMETER
    void            m_setAccelerometer(SensorListenerEvent_t data);
    void            m_setAccelerometerHint(bool flag);
#endif
#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    void            m_setCameraDirection(int cameraDirection);
#endif

#ifdef SAMSUNG_HYPER_MOTION
    virtual bool    getHyperMotionMode(void);
    virtual void    getHyperMotionCropSize(int inputW, int inputH, int outputW, int outputH,
                                                int *cropX, int *corpY, int *cropW, int *cropH);
    int             getHyperMotionSpeed(void);
#endif
    /* Returns the image format for FLITE/3AC/3AP bayer */
    int             getBayerFormat(int pipeId);
    /* Returns the dimensions setting for preview pictures. */
    void            getPreviewSize(int *w, int *h);
    /* Returns the image format for preview frames got from Camera.PreviewCallback. */
    int             getPreviewFormat(void);
    /* Returns the dimension setting for pictures. */
    void            getPictureSize(int *w, int *h);
    /* Returns the image format for pictures. */
    int             getPictureFormat(void);
    /* Returns the image format for picture-related HW. */
    int             getHwPictureFormat(void);
    /* Gets video's width, height */
    void            getHwVideoSize(int *w, int *h);
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

    /* Gets the current antibanding setting. */
    int             getAntibanding(void);
    /* Gets the state of the auto-exposure lock. */
    bool            getAutoExposureLock(void);
    /* Gets the state of the auto-white balance lock. */
    bool            getAutoWhiteBalanceLock(void);

    /* Gets the current color effect setting. */
    int             getColorEffectMode(void);

    /* Gets the current exposure compensation index. */
    int             getExposureCompensation(void);
    /* Gets the current flash mode setting. */
    int             getFlashMode(void);
    /* Gets the current focus areas. */
    void            getFocusAreas(int *validFocusArea, ExynosRect2 *rect2s, int *weights);
    /* Gets the current focus mode setting. */
    int             getFocusMode(void);
#ifdef SAMSUNG_MANUAL_FOCUS
    int32_t        getFocusDistance(void);
#endif
    /* Returns the quality setting for the JPEG picture. */
    int             getJpegQuality(void);
    /* Returns the quality setting for the EXIF thumbnail in Jpeg picture. */
    int             getThumbnailQuality(void);
    /* Returns the dimensions for EXIF thumbnail in Jpeg picture. */
    void            getThumbnailSize(int *w, int *h);
    /* Returns the max size for EXIF thumbnail in Jpeg picture. */
    void            getMaxThumbnailSize(int *w, int *h);
    /* Gets the current metering areas. */
    void            getMeteringAreas(ExynosRect *rects);
    /* Gets the current metering areas.(Using ExynosRect2) */
    void            getMeteringAreas(ExynosRect2 *rect2s);
    /* Returns the current minimum and maximum preview fps. */
    void            getPreviewFpsRange(uint32_t *min, uint32_t *max);
    /* Gets scene mode */
    int             getSceneMode(void);
    /* Gets the current state of video stabilization. */
    bool            getVideoStabilization(void);
    /* Gets the current white balance setting. */
    int             getWhiteBalanceMode(void);
    /* Sets current zoom value. */
    status_t        setZoomLevel(int value);
    /* Gets current zoom value. */
    int             getZoomLevel(void);
    /* Returns the recording mode hint. */
    bool            getRecordingHint(void);
    /* Gets GPS altitude. */
    double          getGpsAltitude(void);
    /* Gets GPS latitude coordinate. */
    double          getGpsLatitude(void);
    /* Gets GPS longitude coordinate. */
    double          getGpsLongitude(void);
    /* Gets GPS processing method. */
    const char *    getGpsProcessingMethod(void);
    /* Gets GPS timestamp. */
    long            getGpsTimeStamp(void);
    /* Gets the rotation angle in degrees relative to the orientation of the camera. */
    int             getRotation(void);

/*
 * Additional API.
 */

    /* Gets metering */
    int             getMeteringMode(void);
    /* Gets metering List */
    int             getSupportedMeteringMode(void);
    /* Gets brightness */
    int             getBrightness(void);
    /* Gets ISO */
    uint32_t        getIso(void);
    /* Set ExposureTime */
    void            setExposureTime(void);
    void            setPreviewExposureTime(void);
    /* Gets ExposureTime for preview */
    uint64_t        getPreviewExposureTime(void);
    /* Gets ExposureTime for capture */
    uint64_t        getCaptureExposureTime(void);
    /* Gets LongExposureTime for capture */
    uint64_t        getLongExposureTime(void);
    /* Gets LongExposureShotCount for capture */
    int32_t         getLongExposureShotCount(void);

#ifdef SAMSUNG_TN_FEATURE
    /* Export iso, exposureTime info to camera parameters */
    void            setParamExifInfo(camera2_udm *udm);
#endif

    /* Gets Contrast */
    uint32_t        getContrast(void);
    /* Gets Saturation */
    int             getSaturation(void);
    /* Gets Sharpness */
    int             getSharpness(void);
    /* Gets Hue */
    int             getHue(void);
    /* Gets WDR */
    bool            getHdrMode(void);
    /* Gets WDR */
    bool            getWdrMode(void);
    /* Gets anti shake */
    bool            getAntiShake(void);
    /* Gets gamma */
    bool            getGamma(void);
    /* Gets ODC */
    bool            getOdcMode(void);
    /* Gets Slow AE */
    bool            getSlowAe(void);
    /* Gets Shot mode */
    int             getShotMode(void);
    /* Gets Preview Buffer Count */
    int             getPreviewBufferCount(void);
    /* Sets Preview Buffer Count */
    void            setPreviewBufferCount(int previewBufferCount);

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
    bool                               getFlagChangedRtHdr(void);
    void                               setFlagChangedRtHdr(bool state);
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
    bool            isSWVdisOnPreview(void);
    bool            getSWVdisMode(void);
    bool            getSWVdisUIMode(void);

    bool            getHWVdisMode(void);
    int             getHWVdisFormat(void);

    /* Gets VT mode */
    int             getVtMode(void);

    /* Gets VR mode */
    int             getVRMode(void);

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
    int *           getHighSpeedSizeTable(int fpsMode);
    int *           getBinningSizeTable(void);
    int *           getBnsRecordingSizeTable(void);
    int *           getNormalRecordingSizeTable(void);
    int *           getDualModeSizeTable(void);
    bool            getScalableSensorMode(void);
    void            setScalableSensorMode(bool scaleMode);
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
    /* Gets the exposure compensation step. */
    float           getExposureCompensationStep(void);

    /* Gets the focal length (in millimeter) of the camera. */
    void            getFocalLength(int *num, int *den);

    /* Gets the distances from the camera to where an object appears to be in focus. */
    void            getFocusDistances(int *num, int *den);;

    /* Gets the minimum exposure compensation index. */
    int             getMinExposureCompensation(void);

    /* Gets the maximum exposure compensation index. */
    int             getMaxExposureCompensation(void);

    /* Gets the minimum exposure Time. */
    int             getMinExposureTime(void);

    /* Gets the maximum exposure Time. */
    int             getMaxExposureTime(void);

    /* Gets the possible minimum exposure Time of the Sensor. */
    int64_t         getSensorMinExposureTime(void);

    /* Gets the possible maximum exposure Time of the Sensor. */
    int64_t         getSensorMaxExposureTime(void);

    /* Gets the minimum WB Kelvin value. */
    int             getMinWBK(void);

    /* Gets the maximum WB Kelvin value. */
    int             getMaxWBK(void);

    /* Gets the maximum number of detected faces supported. */
    int             getMaxNumDetectedFaces(void);

    /* Gets the maximum number of focus areas supported. */
    uint32_t        getMaxNumFocusAreas(void);

    /* Gets the maximum number of metering areas supported. */
    uint32_t        getMaxNumMeteringAreas(void);

    /* Gets the maximum zoom value allowed for snapshot. */
    int             getMaxZoomLevel(void);

    /* Gets the supported antibanding values. */
    int             getSupportedAntibanding(void);


    /* Gets the supported color effects. */
    int             getSupportedColorEffects(void);
    /* Gets the supported color effects & hidden color effect. */
    bool            isSupportedColorEffects(int effectMode);

    /* Check whether the target support Flash */
    int             getSupportedFlashModes(void);

    /* Gets the supported focus modes. */
    int             getSupportedFocusModes(void);

    /* Gets the supported preview fps range. */
    bool            getMaxPreviewFpsRange(int *min, int *max);

    /* Gets the supported scene modes. */
    int             getSupportedSceneModes(void);

    /* Gets the supported white balance. */
    int             getSupportedWhiteBalance(void);

    /* Gets the supported Iso values. */
    int             getSupportedISO(void);

    /* Gets max zoom ratio */
    float           getMaxZoomRatio(void);
    /* Gets zoom ratio */
    float           getZoomRatio(int zoom);
    /* Gets current zoom ratio */
    float           getZoomRatio(void);

    /* Returns true if auto-exposure locking is supported. */
    bool            getAutoExposureLockSupported(void);

    /* Returns true if auto-white balance locking is supported. */
    bool            getAutoWhiteBalanceLockSupported(void);

    /* Returns true if smooth zoom is supported. */
    bool            getSmoothZoomSupported(void);

    /* Returns true if video snapshot is supported. */
    bool            getVideoSnapshotSupported(void);

    /* Returns true if video stabilization is supported. */
    bool            getVideoStabilizationSupported(void);

    /* Returns true if zoom is supported. */
    bool            getZoomSupported(void);

    /* Gets the horizontal angle of view in degrees. */
    void            checkHorizontalViewAngle(void);
    float           getHorizontalViewAngle(void);

    /* Sets the horizontal angle of view in degrees. */
    void            setHorizontalViewAngle(int pictureW, int pictureH);

    /* Gets the vertical angle of view in degrees. */
    float           getVerticalViewAngle(void);

    /* Gets Fnumber */
    void            getFnumber(int *num, int *den);

    /* Gets Aperture value */
    void            getApertureValue(int *num, int *den);

    /* Gets FocalLengthIn35mmFilm */
    int             getFocalLengthIn35mmFilm(void);

    bool            isScalableSensorSupported(void);

    status_t        getFixedExifInfo(exif_attribute_t *exifInfo);
    void            setExifChangedAttribute(exif_attribute_t    *exifInfo,
                                            ExynosRect          *PictureRect,
                                            ExynosRect          *thumbnailRect,
                                            camera2_shot_t      *shot);

    debug_attribute_t *getDebugAttribute(void);
#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    int             getCameraDirection(void);
#endif

#ifdef DEBUG_RAWDUMP
    bool           checkBayerDumpEnable(void);
#endif/* DEBUG_RAWDUMP */
#ifdef SAMSUNG_LBP
    int32_t            getHoldFrameCount(void);
#endif
#ifdef USE_BINNING_MODE
    int             getBinningMode(void);
#endif /* USE_BINNING_MODE */
public:
    bool            DvfsLock();
    bool            DvfsUnLock();

    void            updatePreviewFpsRange(void);
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

    status_t        check3dnrBypass(bool is3dnrMode, bool *flagBypass);

    status_t        setFdMode(enum facedetect_mode mode);
    status_t        getFdMeta(bool reprocessing, void *buf);
    bool            getUHDRecordingMode(void);
    bool            getFaceDetectionMode(bool flagCheckingRecording = true);

    void            vendorSpecificConstructor(int cameraId, bool flagCompanion = false);

private:
    bool            m_isSupportedPreviewSize(const int width, const int height);
    bool            m_isSupportedPictureSize(const int width, const int height);
    bool            m_isSupportedVideoSize(const int width, const int height);
    bool            m_isHighResolutionCallbackSize(const int width, const int height);
    void            m_isHighResolutionMode(const CameraParameters& params);
#ifdef SAMSUNG_HYPER_MOTION
    void            m_isHyperMotionMode(const CameraParameters& params);
    void            m_setHyperMotionMode(bool enable);
    void            m_setHyperMotionSpeed(int motionSpeed);
    void            m_getHyperMotionPreviewSize(int w, int h, int *newW, int *newH);
#endif

    bool            m_getSupportedVariableFpsList(int min, int max,
                                                int *newMin, int *newMax);

    status_t        m_getPreviewSizeList(int *sizeList);

    void            m_getSWVdisPreviewSize(int w, int h, int *newW, int *newH);
    void            m_getSWVdisVideoSize(int w, int h, int *newW, int *newH);
    void            m_getScalableSensorSize(int *newSensorW, int *newSensorH);

    void            m_initMetadata(void);

    bool            m_isUHDRecordingMode(void);

/*
 * Vendor specific adjust function
 */
private:
    status_t        m_adjustPreviewFpsRange(int &newMinFps, int &newMaxFps);
    status_t        m_getPreviewBdsSize(ExynosRect *dstRect);
    status_t        m_adjustPreviewSize(const CameraParameters &params,
                                        int previewW, int previewH,
                                        int *newPreviewW, int *newPreviewH,
                                        int *newCalPreviewW, int *newCalPreviewH);
    status_t        m_adjustPreviewFormat(int &previewFormat, int &hwPreviewFormatH);
    status_t        m_adjustPictureSize(int *newPictureW, int *newPictureH,
                                        int *newHwPictureW, int *newHwPictureH);
    bool            m_adjustHighSpeedRecording(int curMinFps, int curMaxFps, int newMinFps, int newMaxFps);
    const char *    m_adjustAntibanding(const char *strAntibanding);
    const char *    m_adjustFocusMode(const char *focusMode);
    const char *    m_adjustFlashMode(const char *flashMode);
    const char *    m_adjustWhiteBalanceMode(const char *whiteBalance);
    bool            m_adjustScalableSensorMode(const int scaleMode);
    void            m_adjustAeMode(enum aa_aemode curAeMode, enum aa_aemode *newAeMode);
    void            m_adjustSensorMargin(int *sensorMarginW, int *sensorMarginH);
    void            m_getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);

#ifdef USE_CSC_FEATURE
    void            m_getAntiBandingFromLatinMCC(char *temp_str);
    int             m_IsLatinOpenCSC();
    void            m_chooseAntiBandingFrequency();
#endif
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
    status_t        m_addHiddenResolutionList(String8 &string8Buf, struct ExynosSensorInfoBase *sensorInfo,
                                              int w, int h, enum MODE mode, int cameraId);
/* 1.0 Function */
    void            m_setExifChangedAttribute(exif_attribute_t  *exifInfo,
                                              ExynosRect        *PictureRect,
                                              ExynosRect        *thumbnailRect,
                                              camera2_dm        *dm,
                                              camera2_udm       *udm);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    status_t        m_getFusionSize(int w, int h, ExynosRect *rect, bool flagSrc);
#endif

public:
    status_t            setParameters(const CameraParameters& params);
    CameraParameters    getParameters() const;
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

    void                setIsFirstStartFlag(bool flag);
    int                 getIsFirstStartFlag(void);

    void                setPreviewRunning(bool enable);
    void                setPictureRunning(bool enable);
    void                setRecordingRunning(bool enable);
#if defined(SAMSUNG_COMPANION) || defined(SAMSUNG_EEPROM)
    void                setRomReadThreadDone(bool enable);
#endif
    bool                getPreviewRunning(void);
    bool                getPictureRunning(void);
    bool                getRecordingRunning(void);
    bool                getRestartPreview(void);

    ExynosCameraActivityControl *getActivityControl(void);
    status_t            setAutoFocusMacroPosition(int autoFocusMacroPosition);

    void                getSetfileYuvRange(bool flagReprocessing, int *setfile, int *yuvRange);
    void                setSetfileYuvRange(void);
    status_t            checkSetfileYuvRange(void);

    void                setUseDynamicBayer(bool enable);
    bool                getUseDynamicBayer(void);
    void                setUseDynamicBayerVideoSnapShot(bool enable);
    bool                getUseDynamicBayerVideoSnapShot(void);
    void                setUseDynamicScc(bool enable);
    bool                getUseDynamicScc(void);

    void                setUseFastenAeStable(bool enable);
    bool                getUseFastenAeStable(void);

    void                setFastenAeStableOn(bool enable);
    bool                getFastenAeStableOn(void);

    void                setUsePureBayerReprocessing(bool enable);
    bool                getUsePureBayerReprocessing(void);
#ifdef SAMSUNG_LBP
    bool                getUseBestPic(void);
#endif

    int32_t             updateReprocessingBayerMode(void);
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

#ifdef SAMSUNG_DOF
    int                 getMoveLensTotal(void);
    void                setMoveLensCount(int count);
    void                setMoveLensTotal(int count);
#endif
#ifdef SUPPORT_MULTI_AF
    status_t            checkMultiAf(const CameraParameters& params);
#endif
    bool                checkEnablePicture(void);
#ifdef SAMSUNG_OIS_VDIS
    void                setOISCoef(uint32_t coef);
#endif

#ifdef BURST_CAPTURE
    int                 getSeriesShotSaveLocation(void);
    void                setSeriesShotSaveLocation(int ssaveLocation);
    char                *getSeriesShotFilePath(void);
#ifdef VARIABLE_BURST_FPS
    void                setBurstShotDuration(int fps);
    int                 getBurstShotDuration(void);
    int                 m_burstShotDuration;
#endif
    int                 m_seriesShotSaveLocation;
    char                m_seriesShotFilePath[CAMERA_FILE_PATH_SIZE];
#endif
    int                 getSeriesShotDuration(void);
    int                 getSeriesShotMode(void);
    void                setSeriesShotMode(int sshotMode, int count = 0);

    int                 getHalPixelFormat(void);
    int                 convertingHalPreviewFormat(int previewFormat, int yuvRange);

    void                setDvfsLock(bool lock);
    bool                getDvfsLock(void);

#ifdef SAMSUNG_LLV
    void                setLLV(bool enable);
    bool                getLLV(void);
#endif
    void                setFocusModeSetting(bool enable);
    int                 getFocusModeSetting(void);
    bool                getSensorOTFSupported(void);
    bool                isReprocessing(void);
    bool                isSccCapture(void);

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
    bool                isHWFCOnDemand(void);
    void                setHWFCEnable(bool enable);
    bool                getHWFCEnable(void);
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
    void                m_setDualShotMode(const CameraParameters& params);
#ifdef LLS_CAPTURE
    void                setLLSOn(uint32_t enable);
    bool                getLLSOn(void);
    void                setLLSValue(int value);
    int                 getLLSValue(void);
    void                m_setLLSShotMode(void);
    void                m_setLLSShotMode(const CameraParameters& params);
#ifdef SET_LLS_CAPTURE_SETFILE
    void                setLLSCaptureOn(bool enable);
    int                 getLLSCaptureOn();
#endif
#endif
#ifdef LLS_REPROCESSING
    void                setLLSCaptureCount(int count);
    int                 getLLSCaptureCount();
#endif
#ifdef SR_CAPTURE
    void                setSROn(uint32_t enable);
    bool                getSROn(void);
#endif
#ifdef OIS_CAPTURE
    void                setOISCaptureModeOn(bool enable);
    void                checkOISCaptureMode(void);
    bool                getOISCaptureModeOn(void);
#endif
#ifdef SAMSUNG_DNG
    void                setDNGCaptureModeOn(bool enable);
    bool                getDNGCaptureModeOn(void);
    void                setUseDNGCapture(bool enable);
    bool                getUseDNGCapture(void);
    void                setCheckMultiFrame(bool enable);
    bool                getCheckMultiFrame(void);
    void                m_setDngFixedAttribute(void);
    dng_attribute_t     *getDngInfo();
    void                setDngChangedAttribute(struct camera2_dm *dm, struct camera2_udm *udm);
    void                setIsUsefulDngInfo(bool enable);
    bool                getIsUsefulDngInfo();
    status_t            checkDngFilePath(const CameraParameters& params);
    char                *getDngFilePath(void);
    int                 getDngSaveLocation(void);
    void                setDngSaveLocation(int saveLocation);
    dng_thumbnail_t     *createDngThumbnailBuffer(int size);
    dng_thumbnail_t     *putDngThumbnailBuffer(dng_thumbnail_t *buf);
    dng_thumbnail_t     *getDngThumbnailBuffer(unsigned int frameCount);
    void                deleteDngThumbnailBuffer(dng_thumbnail_t *curNode);
    void                cleanDngThumbnailBuffer();
#endif
#ifdef SAMSUNG_QUICKSHOT
    status_t            checkQuickShot(const CameraParameters& params);
    int                 getQuickShot(void);
#endif
#ifdef SAMSUNG_OIS
    status_t            checkOIS(const CameraParameters& params);
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
void                    setMotionPhotoOn(bool enable);
bool                    getMotionPhotoOn(void);
#ifdef SAMSUNG_LLS_DEBLUR
    void                checkLDCaptureMode(void);
    void                setLDCaptureMode(int LDMode);
    int                 getLDCaptureMode(void);
    void                setLDCaptureLLSValue(uint32_t lls);
    int                 getLDCaptureLLSValue(void);
    void                setLDCaptureCount(int count);
    int                 getLDCaptureCount(void);
#endif
    void                setOutPutFormatNV21Enable(bool enable);
    bool                getOutPutFormatNV21Enable(void);
#ifdef SAMSUNG_LENS_DC
    void                setLensDCMode(bool enable);
    bool                getLensDCMode(void);
    void                setLensDCEnable(bool enable);
    bool                getLensDCEnable(void);
#endif

    void                setZoomActiveOn(bool enable);
    bool                getZoomActiveOn(void);
    void                setFocusModeLock(bool enable);
    status_t            setMarkingOfExifFlash(int flag);
    int                 getMarkingOfExifFlash(void);
    bool                increaseMaxBufferOfPreview(void);






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

#ifdef SAMSUNG_OT
    bool                getObjectTrackingEnable(void);
    void                setObjectTrackingChanged(bool newValue);
    bool                getObjectTrackingChanged(void);
    int                 getObjectTrackingAreas(int* validFocusArea, ExynosRect2* areas, int* weights);
    void                setObjectTrackingGet(bool newValue);
    bool                getObjectTrackingGet(void);
#endif
#ifdef SAMSUNG_LBP
    void                setNormalBestFrameCount(uint32_t count);
    uint32_t            getNormalBestFrameCount(void);
    void                resetNormalBestFrameCount(void);
    void                setSCPFrameCount(uint32_t count);
    uint32_t            getSCPFrameCount(void);
    void                resetSCPFrameCount(void);
    void                setBayerFrameCount(uint32_t count);
    uint32_t            getBayerFrameCount(void);
    void                resetBayerFrameCount(void);
#endif
#ifdef SAMSUNG_JQ
    void                setJpegQtableStatus(int state);
    int                 getJpegQtableStatus(void);
    void                setJpegQtableOn(bool isOn);
    bool                getJpegQtableOn(void);
    void                setJpegQtable(unsigned char* qtable);
    void                getJpegQtable(unsigned char* qtable);
#endif
#ifdef SAMSUNG_BD
    void                setBlurInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SAMSUNG_UTC_TS
    void                setUTCInfo();
#endif
#ifdef SAMSUNG_LLS_DEBLUR
    void                setLLSdebugInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SAMSUNG_LENS_DC
    void                setLensDCdebugInfo(unsigned char *data, unsigned int size);
#endif
#ifdef SENSOR_FW_GET_FROM_FILE
    bool                m_checkCalibrationDataValid(char *sensor_fw);
#endif
    void                setUseCompanion(bool use);
    bool                getUseCompanion();
    void                setIsThumbnailCallbackOn(bool enable);
    bool                getIsThumbnailCallbackOn();
#ifdef USE_FADE_IN_ENTRANCE
    status_t            checkFirstEntrance(const CameraParameters& params);
    void                setFirstEntrance(bool value);
    bool                getFirstEntrance(void);
#endif
#ifdef USE_LIVE_BROADCAST /* PLB mode */
    void                setPLBMode(bool plbMode);
    bool                getPLBMode(void);
#endif
#ifdef FLASHED_LLS_CAPTURE
    void                setFlashedLLSCapture(bool isLLSCapture);
    bool                getFlashedLLSCapture();
#endif
    void                getYuvSize(__unused int *width, __unused int *height, __unused const int outputPortId) {};
#ifdef SAMSUNG_QUICK_SWITCH
    void                checkQuickSwitchProp();
    void                setQuickSwitchFlag(bool isQuickSwitch);
    bool                getQuickSwitchFlag();

    void                setQuickSwitchCmd(int cmd);
    int                 getQuickSwitchCmd();
#endif

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    /* Set/Get Dual camera mode */
    void                setDualCameraMode(bool toggle);
    bool                getDualCameraMode(void);
    bool                isFusionEnabled(void);
    status_t            getFusionSize(int w, int h, ExynosRect *srcRect, ExynosRect *dstRect);
    status_t            setFusionInfo(camera2_shot_ext *shot_ext);
    DOF                *getDOF(void);
    bool                getUseJpegCallbackForYuv(void);
#endif // BOARD_CAMERA_USES_DUAL_CAMERA

private:
    int                         m_cameraId;
    int                         m_cameraHiddenId;
    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    CameraParameters            m_params;
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
    mutable Mutex               m_staticInfoExifLock;

    ExynosCameraActivityControl *m_activityControl;

    /* Flags for camera status */
    bool                        m_previewRunning;
    bool                        m_previewSizeChanged;
    bool                        m_pictureRunning;
    bool                        m_recordingRunning;
    bool                        m_flagCheckDualMode;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    bool                        m_flagCheckDualCameraMode;
#endif
    bool                        m_romReadThreadDone;
    bool                        m_use_companion;
    bool                        m_IsThumbnailCallbackOn;
    bool                        m_flagRestartPreviewChecked;
    bool                        m_flagRestartPreview;
    int                         m_fastFpsMode;
    bool                        m_flagFirstStart;
    bool                        m_flagMeteringRegionChanged;
    bool                        m_flagHWVDisMode;

    bool                        m_flagVideoStabilization;
    bool                        m_flag3dnrMode;

    int32_t                     m_reprocessingBayerMode;

#ifdef LLS_CAPTURE
    bool                        m_flagLLSOn;
    int                         m_LLSValue;
    int                         m_LLSCaptureOn;
#endif
#ifdef LLS_REPROCESSING
    int                         m_LLSCaptureCount;
#endif

#ifdef SR_CAPTURE
    bool                        m_flagSRSOn;
#endif
#ifdef OIS_CAPTURE
    bool                        m_flagOISCaptureOn;
    int                         m_llsValue;
#endif
#ifdef SAMSUNG_DNG
    bool                        m_flagDNGCaptureOn;
    bool                        m_use_DNGCapture;
    bool                        m_flagMultiFrame;
    bool                        m_isUsefulDngInfo;
    dng_attribute_t             m_dngInfo;
    int                         m_dngSaveLocation;
    char                        m_dngFilePath[CAMERA_FILE_PATH_SIZE];
    SecCameraDngThumbnail       m_dngThumbnail;
#endif
#ifdef RAWDUMP_CAPTURE
    bool                        m_flagRawCaptureOn;
#endif
    bool                        m_flagMotionPhotoOn;
#ifdef SAMSUNG_LLS_DEBLUR
    int                         m_flagLDCaptureMode;
    uint32_t                    m_flagLDCaptureLLSValue;
    int                         m_LDCaptureCount;
#endif
    bool                        m_flagOutPutFormatNV21Enable;
    bool                        m_flagCheckRecordingHint;
#ifdef SAMSUNG_LENS_DC
    bool                        m_flagLensDCEnable;
    bool                        m_flagLensDCMode;
#endif
#ifdef SAMSUNG_HRM
    bool                        m_flagSensorHRM_Hint;
#endif
#ifdef SAMSUNG_LIGHT_IR
    bool                        m_flagSensorLight_IR_Hint;
#endif
#ifdef SAMSUNG_GYRO
    bool                        m_flagSensorGyroHint;
#endif
#ifdef SAMSUNG_ACCELEROMETER
    bool                        m_flagSensorAccelerationHint;
#endif

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
    bool                        m_useDynamicScc;
    bool                        m_useFastenAeStable;
    bool                        m_fastenAeStableOn;
    bool                        m_usePureBayerReprocessing;
#ifdef SAMSUNG_LBP
    bool                        m_useBestPic;
#endif
    bool                        m_useAdaptiveCSCRecording;
    bool                        m_dvfsLock;
    int                         m_previewBufferCount;

    bool                        m_reallocBuffer;
    mutable Mutex               m_reallocLock;
#ifdef USE_FADE_IN_ENTRANCE
    bool                        m_flagFirstEntrance;
#endif
    struct ExynosConfigInfo     *m_exynosconfig;

#ifdef SAMSUNG_LLV
    bool                        m_isLLVOn;
#endif

#ifdef SAMSUNG_DOF
    int                         m_curLensStep;
    int                         m_curLensCount;
#endif
    bool                        m_setFocusmodeSetting;
#ifdef USE_CSC_FEATURE
    char                        m_antiBanding[10];
#endif
#ifdef SAMSUNG_OIS
    ExynosCameraNode           *m_oisNode;
    bool                        m_setOISmodeSetting;
#endif
    bool                        m_zoom_activated;
    int                         m_firing_flash_marking;

#ifdef SAMSUNG_OT
    bool                        m_startObjectTracking;
    bool                        m_objectTrackingAreaChanged;
    bool                        m_objectTrackingGet;
    ExynosRect2                 *m_objectTrackingArea;
    int                         *m_objectTrackingWeight;
#endif
#ifdef SAMSUNG_LBP
    uint32_t                    m_normal_best_frame_count;
    uint32_t                    m_scp_frame_count;
    uint32_t                    m_bayer_frame_count;
#endif
#ifdef SAMSUNG_JQ
    unsigned char               m_jpegQtable[128];
    int                         m_jpegQtableStatus;
    bool                        m_isJpegQtableOn;
    mutable Mutex               m_JpegQtableLock;
#endif
#ifdef FORCE_CAL_RELOAD
    bool                        m_calValid;
#endif

    uint64_t                    m_exposureTimeCapture;

    bool                        m_zoomWithScaler;
    float                       m_currentZoomRatio;
    int                         m_halVersion;
    ExynosCamera1Parameters      *m_dummyParameters;
    bool                        m_flagDummy;
#ifdef SUPPORT_DEPTH_MAP
    bool                        m_flaguseDepthMap;
    bool                        m_flagDepthCallbackOnPreview;
    bool                        m_flagDepthCallbackOnCapture;
#endif
    bool                        m_flagHWFCEnable;
#ifdef FLASHED_LLS_CAPTURE
    bool                        m_isFlashedLLSCapture;
#endif
#ifdef USE_MULTI_FACING_SINGLE_CAMERA
    int                         m_cameraDirection;
#endif
#ifdef SAMSUNG_QUICK_SWITCH
    bool                        m_isQuickSwitch;
    int                         m_quickSwitchCmd;
#endif
#ifdef SAMSUNG_COMPANION
    bool                        m_flagChangedRtHdr;
#endif
};


}; /* namespace android */

#endif
