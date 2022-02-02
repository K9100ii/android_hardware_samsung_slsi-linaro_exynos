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

#ifdef SAMSUNG_DUAL_SOLUTION
#define WIDE_CAMERA_FOV (76.5)
#define TELE_CAMERA_FOV (45)
#define MARGIN_RATIO (1.2)
#define DUAL_CAL_DATA_SIZE (512)
#define DUAL_CAL_DATA_PATH	"/sys/class/camera/rear/rear_dualcal"
#endif

//! ExynosCameraFusionWrapper
/*!
 * \ingroup ExynosCamera
 */
class ExynosCameraFusionWrapper
{
public:
    //! Constructor
    ExynosCameraFusionWrapper();
    //! Destructor
    virtual ~ExynosCameraFusionWrapper();

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
                              DOF *dof[],
                              ExynosCameraBuffer srcBuffer[],
                              ExynosRect srcRect[],
                              ExynosCameraBufferManager *srcBufferManager[],
                              ExynosCameraBuffer dstBuffer,
                              ExynosRect dstRect,
                              ExynosCameraBufferManager *dstBufferManager);
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    virtual status_t          m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect) = 0;
#endif
#ifdef SAMSUNG_DUAL_SOLUTION
    virtual status_t          m_initDualSolution(int cameraId, int sensorW, int sensorH, int previewW, int previewH,
                                                                    int * zoomList, int zoomListSize) = 0;
    void                      m_setSyncType(sync_type_t syncType);
    sync_type_t               m_getSyncType(void);
    float                     m_getZoomRatio(int cameraID, int zoomLevel);
    void                      m_setCurZoomRatio(int cameraId, float zoomRatio);
    void                      m_setCurViewRatio(int cameraId, float zoomRatio);
    float                     m_getCurZoomRatio(UNI_PLUGIN_CAMERA_TYPE cameraType);
    float                     m_getCurViewRatio(UNI_PLUGIN_CAMERA_TYPE cameraType);
    int                       m_getNeedMargin(int cameraID, int zoomLevel);
    void                      m_setForceWide(void);
    void                      m_setForceWideMode(bool forceWide) { m_forceWide = forceWide; }
    bool                      m_getForceWideMode(void) { return m_forceWide; }
    virtual status_t          m_deinitDualSolution(int cameraId) = 0;
    virtual bool              m_getIsInit() = 0;

    void                      m_setOrientation(int orientation);
    int                       m_getOrientation();
    void                      m_setFocusStatus(int cameraId, int AFstatus);
    int                       m_getFocusStatus(int cameraId);
    int                       m_getCurDisplayedCam();
    void                      m_setCurDisplayedCam(int cameraId);
    status_t                  m_getDualCalDataFromSysfs(void);
    void                      m_setSolutionHandle(void* previewHandle, void* captureHandle);
    void*                     m_getPreviewHandle();
    void*                     m_getCaptureHandle();
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    void                      m_setCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType, UTrect cropROI);
    UTrect*                   m_getCropROI(UNI_PLUGIN_CAMERA_TYPE cameraType);
    void                      m_setCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType, float cropROIRatio);
    float                     m_getCropROIRatio(UNI_PLUGIN_CAMERA_TYPE cameraType);
#endif
    void                      m_setCameraParam(UniPlugin_DCVOZ_CAMERAPARAM_t* param);
    int                       m_getCameraParamIndex(void);
    UniPlugin_DCVOZ_CAMERAPARAM_t* m_getCameraParam(void);
#endif

protected:
    void      m_init(int cameraId);

    status_t  m_execute(int cameraId,
                        ExynosCameraBuffer srcBuffer[], ExynosRect srcRect[],
                        ExynosCameraBuffer dstBuffer,   ExynosRect dstRect,
                        struct camera2_shot_ext shot_ext[],
                        bool isCapture = false);

protected:
    bool      m_flagCreated[CAMERA_ID_MAX];
    Mutex     m_createLock;

    int       m_mainCameraId;
    int       m_subCameraId;
    bool      m_flagValidCameraId[CAMERA_ID_MAX];

    int       m_width      [CAMERA_ID_MAX];
    int       m_height     [CAMERA_ID_MAX];
    int       m_stride     [CAMERA_ID_MAX];

    ExynosCameraDurationTimer m_emulationProcessTimer;
    int                       m_emulationProcessTime;
    float                     m_emulationCopyRatio;

#ifdef SAMSUNG_DUAL_SOLUTION
    void*     m_previewHandle;
    void*     m_captureHandle;
    float     m_wideZoomRatio;
    float     m_teleZoomRatio;
    float     m_imageRatio;
    float     m_wideViewRatio;
    float     m_teleViewRatio;
    int       m_mode;
    int       m_orientation;
    int       m_wideAfStatus;
    int       m_teleAfStatus;
    int       m_displayedCamera;
    bool      m_isInit;
    unsigned char	m_calData[DUAL_CAL_DATA_SIZE];
    float*    m_viewRatioList;
    float*    m_wideImageRatioList;
    float*    m_teleImageRatioList;
    int*      m_wideNeedMarginList;
    int*      m_teleNeedMarginList;
    sync_type_t m_syncType;
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    UTrect 	  m_WideCropROI;
    UTrect 	  m_TeleCropROI;
    float  	  m_WideCropROIRatio;
    float  	  m_TeleCropROIRatio;
#endif
    bool      m_forceWide;
    UniPlugin_DCVOZ_CAMERAPARAM_t* m_cameraParam;
#endif
};

/* for Preview fusion wrapper */
class ExynosCameraFusionPreviewWrapper : public ExynosCameraFusionWrapper
{
public:
    ExynosCameraFusionPreviewWrapper();
    virtual ~ExynosCameraFusionPreviewWrapper();

#ifdef SAMSUNG_DUAL_SOLUTION
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    virtual status_t  m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect){return NO_ERROR;};
#endif
    virtual status_t  execute(int cameraId,
                          sync_type_t syncType,
                          struct camera2_shot_ext shot_ext[],
                          DOF *dof[],
                          ExynosCameraBuffer srcBuffer[],
                          ExynosRect srcRect[],
                          ExynosCameraBufferManager *srcBufferManager[],
                          ExynosCameraBuffer dstBuffer,
                          ExynosRect dstRect,
                          ExynosCameraBufferManager *dstBufferManager);
    virtual status_t          m_initDualSolution(int cameraId, int sensorW, int sensorH, int previewW, int previewH,
                                                                    int * zoomList, int zoomListSize);
    virtual status_t          m_deinitDualSolution(int cameraId);
    virtual bool              m_getIsInit() { return m_isInit; };
#endif
};

/* for Capture fusion wrapper */
class ExynosCameraFusionCaptureWrapper : public ExynosCameraFusionWrapper
{
public:
    ExynosCameraFusionCaptureWrapper();
    virtual ~ExynosCameraFusionCaptureWrapper();

#ifdef SAMSUNG_DUAL_SOLUTION
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
    virtual status_t  m_calRoiRect(int cameraId, ExynosRect mainRoiRect, ExynosRect subRoiRect);
#endif
    virtual status_t  execute(int cameraId,
                          sync_type_t syncType,
                          struct camera2_shot_ext shot_ext[],
                          DOF *dof[],
                          ExynosCameraBuffer srcBuffer[],
                          ExynosRect srcRect[],
                          ExynosCameraBufferManager *srcBufferManager[],
                          ExynosCameraBuffer dstBuffer,
                          ExynosRect dstRect,
                          ExynosCameraBufferManager *dstBufferManager);
    virtual status_t          m_initDualSolution(int cameraId, int sensorW, int sensorH, int captureW, int captureH,
																		int * zoomList, int zoomListSize);
    virtual status_t          m_deinitDualSolution(int cameraId);
    virtual bool              m_getIsInit() { return m_isInit; };
#endif
};

#endif //EXYNOS_CAMERA_FUSION_WRAPPER_H
