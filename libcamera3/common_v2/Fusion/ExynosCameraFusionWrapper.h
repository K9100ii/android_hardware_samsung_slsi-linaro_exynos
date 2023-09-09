/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*!
 * \file      ExynosCameraFusionWrapper.h
 * \brief     header file for ExynosCameraFusionWrapper
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2014/10/08
 *
 * <b>Revision History: </b>
 * - 2014/10/08 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 *
 */

#ifndef EXYNOS_CAMERA_FUSION_WRAPPER_H
#define EXYNOS_CAMERA_FUSION_WRAPPER_H

#include "string.h"
#include <dlfcn.h>
#include <utils/Log.h>
#include <utils/threads.h>

#include "ExynosCameraFusionInclude.h"

using namespace android;

//#define EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG

#ifdef EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG
#define EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG_LOG CLOGD
#else
#define EXYNOS_CAMERA_FUSION_WRAPPER_DEBUG_LOG CLOGV
#endif

#define FUSION_PROCESSTIME_STANDARD (34000)

#if defined(SAMSUNG_DUAL_ZOOM_PREVIEW) || defined(SAMSUNG_DUAL_PORTRAIT_SOLUTION)
#define WIDE_CAMERA_FOV (76.5)
#define TELE_CAMERA_FOV (45)
#define MARGIN_RATIO (1.2)
#define DUAL_CAL_DATA_SIZE (4112)
#define FUSION_PREVIEW_WRAPPER  (0)
#define FUSION_CAPTURE_WRAPPER  (1)
#endif

//! ExynosCameraFusionWrapper
/*!
 * \ingroup ExynosCamera
 */
class ExynosCameraFusionWrapper
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraFusionWrapper>;

    //! Constructor
    ExynosCameraFusionWrapper();
    //! Destructor
    virtual ~ExynosCameraFusionWrapper();

public:
    //! create
    virtual status_t create(int cameraId,
                            int srcWidth, int srcHeight,
                            int dstWidth, int dstHeight,
                            char *calData = NULL, int calDataSize = 0);

    //! destroy
    virtual status_t  destroy(int cameraId);

    //! flagCreate
    virtual bool      flagCreate(int cameraId);

    //! flagReady to run execute
    virtual bool      flagReady(int cameraId);

    //! execute
    virtual status_t  execute(int cameraId,
                              struct camera2_shot_ext shot_ext[],
                              ExynosCameraBuffer srcBuffer[],
                              ExynosRect srcRect[],
                              ExynosCameraBufferManager *srcBufferManager[],
                              ExynosCameraBuffer dstBuffer,
                              ExynosRect dstRect,
                              ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                              , int multiShotCount = 0,
                              int LDCaptureTotalCount = 0
#endif
                              );

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    virtual status_t          m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect) = 0;
    virtual status_t          m_getDebugInfo(int cameraId, void *data) = 0;
#endif

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    virtual status_t          m_initDualSolution(int cameraId,
                                                 int maxSensorW, int maxSensorH,
                                                 int sensorW, int sensorH,
                                                 int previewW, int previewH) = 0;
    virtual status_t          m_deinitDualSolution(int cameraId) = 0;

    virtual bool              m_getIsInit() = 0;

    void                      m_setFrameType(uint32_t frameType);
    uint32_t                  m_getFrameType(void);

    void                      m_setOrientation(int orientation);
    int                       m_getOrientation();

    void                      m_setFocusStatus(int cameraId, int AFstatus);
    int                       m_getFocusStatus(int cameraId);

    void                      m_setSolutionHandle(void *previewHandle, void *captureHandle);
    void*                     m_getPreviewHandle();
    void*                     m_getCaptureHandle();
    void                      m_getCalBuf(unsigned char **buf);
#endif

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    virtual status_t          m_getWhichWrapper(void) = 0;
    void                      m_setFallback(void);
    void                      m_setFallbackMode(bool fallback);
    bool                      m_getFallbackMode(void);
    void                      m_getActiveZoomInfo(ExynosRect viewZoomRect,
                                                  ExynosRect *wideZoomRect,
                                                  ExynosRect *teleZoomRect,
                                                  int *wideZoomMargin,
                                                  int *teleZoomMargin);
    void                      m_setCurZoomRect(int cameraId, ExynosRect zoomRect);
    void                      m_setCurViewRect(int cameraId, ExynosRect viewRect);

    void                      m_getCurZoomRect(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect *zoomRect);
    void                      m_getCurViewRect(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect *viewZoomRect);

    void                      m_getCurZoomRectUT(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *zoomRect);
    void                      m_getCurViewRectUT(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *viewZoomRect);

    int                       m_getCurZoomMargin(UNI_PLUGIN_CAMERA_TYPE cameraType);
    void                      m_setCurZoomMargin(int cameraId, int zoomMargin);

    void                      m_setDualSelectedCam(int selectedCam);

    int                       m_getCurDisplayedCam();
    void                      m_setCurDisplayedCam(int cameraId);
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    void                      m_setCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect cropROI);
    void                      m_getCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect *cropROI);
    void                      m_setCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType, float cropROIRatio);
    float                     m_getCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType);
    void                      m_setBcropROI(int cameraId, int bCropX, int bCropY);
    void                      m_setWideFaceInfo(void *faceROIs, int faceNums);
    void                      m_setTeleFaceInfo(void *faceROIs, int faceNums);
    void                      m_resetFaceInfo(void);
#endif
    void                      m_setCameraParam(UniPluginDualCameraParam_t* param);
    UniPluginDualCameraParam_t* m_getCameraParam(void);
    int                       m_getConvertRect2Origin(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect2 *srcRect, ExynosRect2 *destRect);
    int                       m_getConvertRect2Screen(UNI_PLUGIN_CAMERA_TYPE cameraType, ExynosRect2 *srcRect, ExynosRect2 *destRect, int margin);
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */

    void                  m_setVRAInputSize(int width, int height);
    void                  m_getVRAInputSize(int *width, int *height);

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    void                  m_setBokehBlurStrength(int bokehBlurStrength);
    int                   m_getBokehBlurStrength(void);

#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    void                  m_setBeautyFaceRetouchLevel(int beautyFaceRetouchLevel);
    int                   m_getBeautyFaceRetouchLevel(void);

    void                  m_setBeautyFaceSkinColorLevel(int beautyFaceSkinColorLevel);
    int                   m_getBeautyFaceSkinColorLevel(void);
#endif

    void                  m_setHwBcropSize(int width, int height);
    void                  m_getHwBcropSize(int *width, int *height);

    void                  m_setBokehPreviewResultValue(int result);
    int                   m_getBokehPreviewResultValue(void);

    void                  m_setBokehCaptureResultValue(int result);
    int                   m_getBokehCaptureResultValue(void);

    void                  m_setBokehPreviewProcessResult(int result);
    int                   m_getBokehPreviewProcessResult(void);

    void                  m_setCurrentBokehPreviewResult(int result);
    int                   m_getCurrentBokehPreviewResult(void);

    void                  m_setBokehCaptureProcessing(int bokehCaptureProcessing);
    int                   m_getBokehCaptureProcessing(void);

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    void                  m_setDualCaptureFlag(int dualCaptureFlag);
    int                   m_getDualCaptureFlag(void);
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

    void                      m_setScenario(uint32_t scenario);
    uint32_t                  m_getScenario(void);

protected:
    void      m_init(int cameraId);

    status_t  m_execute(int cameraId,
                        ExynosCameraBuffer srcBuffer[], ExynosRect srcRect[],
                        ExynosCameraBuffer dstBuffer,   ExynosRect dstRect,
                        struct camera2_shot_ext shot_ext[],
                        bool isCapture = false
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                        , int multiShotCount = 0,
                        int LDCaptureTotalCount = 0
#endif
                        );

protected:
    bool      m_flagCreated[CAMERA_ID_MAX];
    Mutex     m_createLock;
    Mutex     m_Lock;
    Mutex     m_captureLock;

    int       m_mainCameraId;
    int       m_subCameraId;
    bool      m_flagValidCameraId[CAMERA_ID_MAX];

    int       m_width      [CAMERA_ID_MAX];
    int       m_height     [CAMERA_ID_MAX];
    int       m_stride     [CAMERA_ID_MAX];

    ExynosCameraDurationTimer m_emulationProcessTimer;
    int                       m_emulationProcessTime;
    float                     m_emulationCopyRatio;

#if defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION)
    int       m_orientation;
    void*     m_previewHandle;
    void*     m_captureHandle;
    int       m_wideAfStatus;
    int       m_teleAfStatus;
    bool      m_isInit;
    unsigned char    m_calData[DUAL_CAL_DATA_SIZE];
    uint32_t  m_frameType;
#endif /* defined (SAMSUNG_DUAL_ZOOM_PREVIEW) || defined (SAMSUNG_DUAL_PORTRAIT_SOLUTION) */

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    ExynosRect     m_wideZoomRect;
    ExynosRect     m_teleZoomRect;
    ExynosRect     m_wideViewRect;
    ExynosRect     m_teleViewRect;

    UTrect    m_wideZoomRectUT;
    UTrect    m_teleZoomRectUT;
    UTrect    m_imageRectUT;
    UTrect    m_wideViewRectUT;
    UTrect    m_teleViewRectUT;

    int       m_wideZoomMargin;
    int       m_teleZoomMargin;
    int       m_mode;
    int       m_displayedCamera;
    bool     m_fallback;

#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    UTrect    m_WideCropROI;
    UTrect    m_TeleCropROI;
    float     m_WideCropROIRatio;
    float     m_TeleCropROIRatio;
    UTrect        m_bCropROI;
    ExynosRect2   m_WidefaceROIs[NUM_OF_DETECTED_FACES];
    ExynosRect2   m_TelefaceROIs[NUM_OF_DETECTED_FACES];
    int           m_WidefaceNums;
    int           m_TelefaceNums;
#endif

    UniPluginDualCameraParam_t* m_cameraParam;
#endif /* SAMSUNG_DUAL_ZOOM_PREVIEW */

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
    int m_bokehBlurStrength;

    int m_hwBcropSizeWidth;
    int m_hwBcropSizeHeight;

    int m_BokehPreviewResultValue;
    int m_BokehCaptureResultValue;
    int m_BokehPreviewProcessResult;
#ifdef SAMSUNG_DUAL_PORTRAIT_BEAUTY
    int m_beautyFaceRetouchLevel;
    int m_beautyFaceSkinColorLevel;
#endif
    int m_CurrentBokehPreviewResult;

    static Mutex m_bokehCaptureProcessingLock;
    static int m_bokehCaptureProcessing;

#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    int m_prevDualCaptureFlag;
    int m_dualCaptureFlag;
#endif
#endif /* SAMSUNG_DUAL_PORTRAIT_SOLUTION */

    int m_vraInputSizeWidth;
    int m_vraInputSizeHeight;
    uint32_t m_scenario;
};

/* for Preview fusion zoom wrapper */
class ExynosCameraFusionZoomPreviewWrapper : public ExynosCameraFusionWrapper
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraFusionZoomPreviewWrapper>;

    ExynosCameraFusionZoomPreviewWrapper();
    virtual ~ExynosCameraFusionZoomPreviewWrapper();

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
public:
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    virtual status_t  m_calRoiRect(__unused int cameraId, __unused ExynosRect mainRoiRect, __unused ExynosRect subRoiRect){return NO_ERROR;};
    virtual status_t  m_getDebugInfo(__unused int cameraId, __unused void *data){return NO_ERROR;};
#endif
    virtual status_t  execute(int cameraId,
                          struct camera2_shot_ext shot_ext[],
                          ExynosCameraBuffer srcBuffer[],
                          ExynosRect srcRect[],
                          ExynosCameraBufferManager *srcBufferManager[],
                          ExynosCameraBuffer dstBuffer,
                          ExynosRect dstRect,
                          ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                          , int multiShotCount = 0,
                          int LDCaptureTotalCount = 0
#endif
                          );
    status_t  m_initDualSolution(int cameraId,
                                 int maxSensorW, int maxSensorH,
                                 int sensorW, int sensorH,
                                 int previewW, int previewH);
    virtual status_t  m_deinitDualSolution(int cameraId);
    virtual bool      m_getIsInit() { return m_isInit; };
    virtual status_t  m_getWhichWrapper(void) { return FUSION_PREVIEW_WRAPPER; };
#else
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
public:
	status_t  m_initDualSolution(int cameraId,
                                     int maxSensorW, int maxSensorH,
                                     int sensorW, int sensorH,
                                     int previewW, int previewH){return NO_ERROR;};
	status_t  m_deinitDualSolution(int cameraId){return NO_ERROR;};
	bool	  m_getIsInit() { return m_isInit; };
#endif
#endif
};

/* for Capture fusion zoom wrapper */
class ExynosCameraFusionZoomCaptureWrapper : public ExynosCameraFusionWrapper
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraFusionZoomCaptureWrapper>;

    ExynosCameraFusionZoomCaptureWrapper();
    virtual ~ExynosCameraFusionZoomCaptureWrapper();

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
public:
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
    virtual status_t  m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect);
    virtual status_t  m_getDebugInfo(int cameraId, void *data);
#endif
    virtual status_t  execute(int cameraId,
                              struct camera2_shot_ext shot_ext[],
                              ExynosCameraBuffer srcBuffer[],
                              ExynosRect srcRect[],
                              ExynosCameraBufferManager *srcBufferManager[],
                              ExynosCameraBuffer dstBuffer,
                              ExynosRect dstRect,
                              ExynosCameraBufferManager *dstBufferManager
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
                              , int multiShotCount = 0,
                              int LDCaptureTotalCount = 0
#endif
                              );
    status_t  m_initDualSolution(int cameraId,
                                 int maxSensorW, int maxSensorH,
                                 int sensorW, int sensorH,
                                 int captureW, int captureH);
    virtual status_t  m_deinitDualSolution(int cameraId);
    virtual bool      m_getIsInit() { return m_isInit; };
    virtual status_t  m_getWhichWrapper(void) { return FUSION_CAPTURE_WRAPPER; };
#endif
};

#endif //EXYNOS_CAMERA_FUSION_WRAPPER_H
