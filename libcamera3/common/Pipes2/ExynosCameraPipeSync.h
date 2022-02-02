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

#ifndef EXYNOS_CAMERA_PIPE_Sync_H
#define EXYNOS_CAMERA_PIPE_Sync_H

#include "ExynosCameraSWPipe.h"

namespace android {

class ExynosCameraPipeSync : public ExynosCameraSWPipe {
public:
    ExynosCameraPipeSync()
    {
        m_init();
    }

    ExynosCameraPipeSync(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        stop(void);

protected:
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    status_t                m_syncFrame(ExynosCameraFrameSP_sptr_t masterFrame,
                                        ExynosCameraFrameSP_sptr_t slaveFrame);

    void                    m_init(void);

private:
    frame_queue_t           *m_masterFrameQ;
    frame_queue_t           *m_slaveFrameQ;
    int                     m_lastTimeStamp;
};

}; /* namespace android */

#endif
