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

#ifndef EXYNOS_CAMERA_PIPE_PP_H
#define EXYNOS_CAMERA_PIPE_PP_H

#include "ExynosCameraPipe.h"
#include "ExynosCameraPPFactory.h"

namespace android {

class ExynosCameraPipePP : protected ExynosCameraPipe {
public:
    ExynosCameraPipePP()
    {
        m_init(NULL, false);
    }

    ExynosCameraPipePP(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums,
        bool isPreviewFactory = false) : ExynosCameraPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init(nodeNums, isPreviewFactory);
    }

    virtual ~ExynosCameraPipePP()
    {
        m_destroy();
    }

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        startThread(void);
    virtual status_t        stopThread(void);

    /* Helper functions for PipePP */
    void                    connectPPScenario(int32_t scenario);
    void                    extControl(int scenario, int controlType, void *data);
    void                    startPPScenario(void);
    void                    stopPPScenario(bool suspendFlag = false);
    int                     getPPScenario(void);

protected:
    virtual bool            m_mainThreadFunc(void);
    virtual bool            m_checkThreadLoop(void);
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    void                    m_init(int32_t *nodeNums, bool isPreviewFactory);

private:
    int                     m_nodeNum;
    ExynosCameraPP         *m_pp;
#ifdef SAMSUNG_TN_FEATURE
    static ExynosCameraPP  *m_ppScenario[PP_SCENARIO_MAX];
    int                     m_scenario;
    bool                    m_isPreviewFactory;
#endif
};

}; /* namespace android */

#endif
