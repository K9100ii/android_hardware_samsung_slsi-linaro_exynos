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
#include "ExynosCameraDualFrameSelector.h"

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

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
        int32_t *nodeNums,
        bool flagSelectedByAllCamera = false) : ExynosCameraSWPipe(cameraId, obj_param, isReprocessing, nodeNums)
    {
        m_init();

        m_cameraId = cameraId;
        m_reprocessing = isReprocessing ? 1 : 0;
        m_oneShotMode = isReprocessing;
        m_flagSelectedByAllCamera = flagSelectedByAllCamera;

        getDualCameraId(&m_masterCameraId, &m_slaveCameraId);

        if (obj_param) {
            m_parameters = obj_param;
            m_activityControl = m_parameters->getActivityControl();
            m_exynosconfig = m_parameters->getConfig();
        }
    }

    virtual ~ExynosCameraPipeSync();

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        destroy(void);

    virtual status_t        start(void);
    virtual status_t        stop(void);
    virtual status_t        setBufferManager(ExynosCameraBufferManager **bufferManager);
    virtual status_t        startThread(void);
    virtual status_t        stopThread(void);

    virtual void            setDualSelector(ExynosCameraDualFrameSelector *dualSelector);

protected:
    virtual status_t        m_run(void);

    virtual bool            m_selectThreadFunc(void);
    virtual status_t        m_select(void);

    virtual bool            m_removeFrameThreadFunc(void);
    virtual status_t        m_removeFrame(void);

private:
    void                    m_init(void);

private:
    bool                    m_flagSelectedByAllCamera;
    sp<Thread>              m_selectThread;
    sp<Thread>              m_removeFrameThread;
    int                     m_masterCameraId;
    int                     m_slaveCameraId;
    ExynosCameraList<ExynosCameraDualFrameSelector::Message> *m_notifyQ;
    ExynosCameraList<ExynosCameraFrameSP_sptr_t> *m_removeFrameQ;
    ExynosCameraDualFrameSelector *m_dualSelector;
    int                     m_lastTimeStamp;
};

}; /* namespace android */

#endif
