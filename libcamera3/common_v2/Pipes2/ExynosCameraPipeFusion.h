/*
**
** Copyright 2016, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_PIPE_FUSION_H
#define EXYNOS_CAMERA_PIPE_FUSION_H

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraFusionWrapper.h"
#include "ExynosCameraBokehWrapper.h"

namespace android {

class ExynosCameraPipeFusion : public ExynosCameraSWPipe {
public:
    ExynosCameraPipeFusion()
    {
        m_init();
    }

    ExynosCameraPipeFusion(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
        getDualCameraId(&m_masterCameraId, &m_slaveCameraId);
    }

    virtual status_t        create(int32_t *sensorIds = NULL);

    virtual status_t        setFusionWrapper(ExynosCameraFusionWrapper *fusionWrapper);
    virtual status_t        setBokehWrapper(ExynosCameraFusionWrapper *bokehWrapper);
protected:
    virtual status_t        m_destroy(void);

    virtual status_t        m_run(void);
    virtual status_t        m_createFusion(void);
    virtual status_t        m_destroyFusion(void);
    virtual status_t        m_manageFusion(ExynosCameraFrameSP_sptr_t newFrame);
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    virtual void            m_checkFallbackCondition(uint32_t frameType);
#endif
    virtual void            m_manageFusionDualRearZoom(ExynosCameraFrameSP_sptr_t newFrame, int i, int cameraId, int vraInputSizeWidth, int vraInputSizeHeight);
    virtual void            m_manageFusionDualPortrait(ExynosCameraFrameSP_sptr_t newFrame, int i, int cameraId, int vraInputSizeWidth, int vraInputSizeHeight);

private:
    void                    m_init(void);

private:
    ExynosCameraDurationTimer m_fusionProcessTimer;
    ExynosCameraFusionWrapper *m_fusionWrapper;
    ExynosCameraFusionWrapper *m_bokehWrapper;
    int                     m_masterCameraId;
    int                     m_slaveCameraId;
};
}; /* namespace android */

#endif
