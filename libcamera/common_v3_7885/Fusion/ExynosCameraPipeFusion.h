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
#include "ExynosCameraDualFrameSelector.h"
#include "ExynosCameraFusionWrapper.h"

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

class ExynosCameraPipeFusion : public ExynosCameraSWPipe {
public:
    ExynosCameraPipeFusion()
    {
        m_init();
    }

    ExynosCameraPipeFusion(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
        m_init();

        m_cameraId = cameraId;
        m_reprocessing = isReprocessing ? 1 : 0;
        m_oneShotMode = isReprocessing;

        if (obj_param) {
            m_parameters = obj_param;
            m_activityControl = m_parameters->getActivityControl();
            m_exynosconfig = m_parameters->getConfig();
        }

        getDualCameraId(&m_masterCameraId, &m_slaveCameraId);
    }

    virtual ~ExynosCameraPipeFusion();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);

    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual void            setDualSelector(ExynosCameraDualFrameSelector *dualSelector);
    virtual void            setFusionWrapper(ExynosCameraFusionWrapper *fusionWrapper);

protected:
    virtual status_t        m_run(void);
    virtual status_t        m_createFusion(void);
    virtual status_t        m_destroyFusion(void);
    virtual status_t        m_manageFusion(ExynosCameraFrameSP_sptr_t newFrame);
#ifdef SAMSUNG_DUAL_SOLUTION
    virtual void            m_checkForceWideCond(int32_t cameraId, sync_type_t syncType);
#endif

private:
    void                    m_init(void);

private:
    ExynosCameraDualFrameSelector *m_dualSelector;
    ExynosCameraDurationTimer m_fusionProcessTimer;
    ExynosCameraFusionWrapper *m_fusionWrapper;
    int                     m_masterCameraId;
    int                     m_slaveCameraId;
};
}; /* namespace android */

#endif
