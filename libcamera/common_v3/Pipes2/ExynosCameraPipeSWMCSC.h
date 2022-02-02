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

#ifndef EXYNOS_CAMERA_PIPE_SWMCSC_H
#define EXYNOS_CAMERA_PIPE_SWMCSC_H

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraPipePP.h"

namespace android {

class ExynosCameraPipeSWMCSC : protected virtual ExynosCameraSWPipe {
public:
    ExynosCameraPipeSWMCSC()
    {
        m_init(NULL);
    }

    ExynosCameraPipeSWMCSC(
        int cameraId,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
        m_init(nodeNums);
    }

    virtual ~ExynosCameraPipeSWMCSC();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);
    virtual status_t        stop(void);
    virtual status_t        stopThread(void);

protected:
    virtual status_t        m_run(void);
    virtual status_t        m_runScalerSerial(ExynosCameraFrameSP_sptr_t frame);
    virtual status_t        m_runScalerParallel(ExynosCameraFrameSP_sptr_t frame);
    virtual bool            m_gscDoneThreadFunc(void);
    virtual status_t        m_handleGscDoneFrame(void);

private:
    void                    m_init(int32_t *nodeNums);

private:
    sp<Thread>              m_gscDoneThread;
    frame_queue_t          *m_handleGscDoneQ;
    frame_queue_t          *m_gscFrameDoneQ[4];
    ExynosCameraPipe       *m_gscPipes[4];
    int                     m_gscPipeId[4] = {PIPE_GSC, PIPE_GSC_CALLBACK, PIPE_GSC_VIDEO, PIPE_GSC_PICTURE};
    enum NODE_TYPE          m_nodeType[4];
};

}; /* namespace android */

#endif
