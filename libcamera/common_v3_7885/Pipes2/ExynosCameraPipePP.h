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

#ifndef EXYNOS_CAMERA_PIPE_PP_H
#define EXYNOS_CAMERA_PIPE_PP_H

#include "ExynosCameraSWPipe.h"

#include "ExynosCameraPPFactory.h"

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

class ExynosCameraPipePP : protected virtual ExynosCameraSWPipe {
public:
    ExynosCameraPipePP()
    {
        m_init(NULL);
    }

    ExynosCameraPipePP(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
        m_init(nodeNums);
    }

    virtual ~ExynosCameraPipePP();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);

    virtual status_t        start(void);
    virtual status_t        stop(void);

protected:
    virtual status_t        m_run(void);

private:
    void                    m_init(int32_t *nodeNums);

private:
    int                     m_nodeNum;
    ExynosCameraPP         *m_pp;
};

}; /* namespace android */

#endif
