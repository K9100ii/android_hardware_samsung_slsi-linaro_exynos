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
 * \file      ExynosCameraBokehWrapper.h
 * \brief     header file for ExynosCameraBokehWrapper
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2014/10/08
 *
 * <b>Revision History: </b>
 * - 2014/10/08 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 *
 */

#ifndef EXYNOS_CAMERA_BOKEH_WRAPPER_H
#define EXYNOS_CAMERA_BOKEH_WRAPPER_H

#include "string.h"
#include <dlfcn.h>
#include <utils/Log.h>
#include <utils/threads.h>

//#include "ExynosCameraBokehInclude.h"
//#include "ExynosCameraFusionInclude.h"

#include "ExynosCameraFusionWrapper.h"


using namespace android;

//#define EXYNOS_CAMERA_BOKEH_WRAPPER_DEBUG

#ifdef EXYNOS_CAMERA_BOKEH_WRAPPER_DEBUG
#define EXYNOS_CAMERA_BOKEH_WRAPPER_DEBUG_LOG CLOGD
#else
#define EXYNOS_CAMERA_BOKEH_WRAPPER_DEBUG_LOG CLOGV
#endif

#define BOKEH_PROCESSTIME_STANDARD (34000)

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
#define BOKEH_PREVIEW_WRAPPER  (0)
#define BOKEH_CAPTURE_WRAPPER  (1)
#endif

/* for Preview Bokeh wrapper */
class ExynosCameraBokehPreviewWrapper : public ExynosCameraFusionWrapper
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraBokehPreviewWrapper>;

    ExynosCameraBokehPreviewWrapper();
    virtual ~ExynosCameraBokehPreviewWrapper();

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
public:
    virtual status_t  m_calRoiRect(__unused int cameraId, __unused ExynosRect mainRoiRect, __unused ExynosRect subRoiRect){return NO_ERROR;};
    virtual status_t  m_getDebugInfo(__unused int cameraId, __unused void *data){return NO_ERROR;};
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
    virtual status_t  m_initDualSolution(int cameraId,
                                         int maxSensorW, int maxSensorH,
                                         int sensorW, int sensorH,
                                         int previewW, int previewH);
    virtual status_t  m_deinitDualSolution(int cameraId);
    virtual bool      m_getIsInit() { return m_isInit; };
    virtual status_t  m_getWhichWrapper(void) { return BOKEH_PREVIEW_WRAPPER; };
#else
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
public:
    virtual   status_t  m_calRoiRect(__unused int cameraId, __unused ExynosRect mainRoiRect, __unused ExynosRect subRoiRect){return NO_ERROR;};
    virtual   status_t  m_getDebugInfo(__unused int cameraId, __unused void *data){return NO_ERROR;};
    status_t  m_initDualSolution(int cameraId,
                                 int maxSensorW, int maxSensorH,
                                 int sensorW, int sensorH,
                                 int previewW, int previewH){return NO_ERROR;};
    status_t  m_deinitDualSolution(int cameraId){return NO_ERROR;};
    bool      m_getIsInit() { return m_isInit; };
    status_t  m_getWhichWrapper(void) { return 0; };
#endif
#endif
};

/* for Capture Bokeh wrapper */
class ExynosCameraBokehCaptureWrapper : public ExynosCameraFusionWrapper
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraBokehCaptureWrapper>;

    ExynosCameraBokehCaptureWrapper();
    virtual ~ExynosCameraBokehCaptureWrapper();

#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
public:
    virtual status_t  m_calRoiRect(__unused int cameraId, __unused ExynosRect mainRoiRect, __unused ExynosRect subRoiRect){return NO_ERROR;};
    virtual status_t  m_getDebugInfo(int cameraId, void *data);

    virtual status_t  m_getDepthMap(int cameraId, UniPluginBufferData_t *data);
    virtual status_t  m_getArcExtraData(int cameraId, void *data);
    virtual status_t  m_getDOFSMetaData(int cameraId, void *data);

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
    virtual status_t  m_initDualSolution(int cameraId,
                                         int maxSensorW, int maxSensorH,
                                         int sensorW, int sensorH,
                                         int captureW, int captureH);
    virtual status_t  m_deinitDualSolution(int cameraId);
    virtual bool      m_getIsInit() { return m_isInit; };
    virtual status_t  m_getWhichWrapper(void) { return BOKEH_CAPTURE_WRAPPER; };
#endif
};

#endif //EXYNOS_CAMERA_BOKEH_WRAPPER_H
