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

#ifndef EXYNOS_CAMERA_PIPE_VRA_H
#define EXYNOS_CAMERA_PIPE_VRA_H

#include "ExynosCameraPipe.h"

/* #define DUMP_VRA_INPUT */
/* #define USE_VRA_FILE_INPUT */

#ifdef CAMERA_GED_FEATURE
#define DUMP_VRA_INPUT_PATH "/data/dump"
#else
#define DUMP_VRA_INPUT_PATH "/data/media/0"
#endif

namespace android {

typedef ExynosCameraList<ExynosCameraFrameSP_sptr_t> frame_queue_t;

class ExynosCameraPipeVRA : protected virtual ExynosCameraPipe {
public:
    ExynosCameraPipeVRA()
    {
        m_init();
    }

    ExynosCameraPipeVRA(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)  : ExynosCameraPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    virtual ~ExynosCameraPipeVRA();
    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        setUseLatestFrame(bool flag);

protected:
    virtual bool            m_mainThreadFunc(void);
    virtual status_t        m_putBuffer(void);
    virtual status_t        m_getBuffer(void);
    virtual status_t        m_updateMetadataToFrame(void *metadata, int index);

private:
    void                    m_init(void);

    status_t                m_useImageFromFile(ExynosCameraBuffer *buffer);
    status_t                m_dumpVraInput(ExynosCameraBuffer *buffer);
private:
    bool                    m_useLatestFrame;
};

}; /* namespace android */

#endif
