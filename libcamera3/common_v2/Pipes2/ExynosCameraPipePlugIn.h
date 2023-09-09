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

#ifndef EXYNOS_CAMERA_PIPE_PLUG_IN_H
#define EXYNOS_CAMERA_PIPE_PLUG_IN_H

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraFactoryPlugIn.h"

namespace android {

class ExynosCameraPipePlugIn : public ExynosCameraSWPipe {
public:
    ExynosCameraPipePlugIn()
    {
        m_init(NULL);
    }

    ExynosCameraPipePlugIn(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums,
        __unused PLUGIN::MODE mode = PLUGIN::MODE::BASE) : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init(nodeNums);
    }

    virtual ~ExynosCameraPipePlugIn();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);
    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        setupPipe(Map_t *map);
    virtual status_t        setParameter(int key, void *data);
    virtual status_t        getParameter(int key, void *data);

protected:
    typedef enum handle_status {
        PLUGIN_ERROR = -1,
        PLUGIN_NO_ERROR,
        PLUGIN_SKIP,
    } handle_status_t;
    virtual status_t                m_run(void);
    virtual handle_status_t         m_handleFrameBefore(ExynosCameraFrameSP_sptr_t frame, Map_t *map);
    virtual handle_status_t         m_handleFrameAfter(ExynosCameraFrameSP_sptr_t frame, Map_t *map);
private:
    void                    m_init(int32_t *nodeNums);
    status_t                m_setup(Map_t *map);

private:
    ExynosCameraFactoryPlugIn           *m_globalPlugInFactory;
    ExynosCameraPlugInSP_sptr_t          m_plugIn;
    ExynosCameraPlugInConverterSP_sptr_t m_plugInConverter;
    Map_t                                m_map;
    PLUGIN::MODE                         m_mode;
};
}; /* namespace android */

#endif
