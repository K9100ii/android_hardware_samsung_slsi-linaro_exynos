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

#ifndef EXYNOS_CAMERA_SW_PIPE_H
#define EXYNOS_CAMERA_SW_PIPE_H

#include "ExynosCameraPipe.h"

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

class ExynosCameraSWPipe : public ExynosCameraPipe {
public:
    ExynosCameraSWPipe()
    {
    /* NOP */
    }

    ExynosCameraSWPipe(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
    /* NOP */
    }

    virtual ~ExynosCameraSWPipe();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        release(void);
    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        startThread(void);

protected:
    virtual bool            m_mainThreadFunc(void);
    virtual status_t        m_run(void) = 0;

};

}; /* namespace android */

#endif
