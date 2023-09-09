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

#ifndef EXYNOS_CAMERA_PIPE_GMV_H
#define EXYNOS_CAMERA_PIPE_GMV_H

#include <dlfcn.h>

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraTimeLogger.h"

/* #define DUMP_GMV_INPUT */

#ifdef CAMERA_GED_FEATURE
#define DUMP_GMV_INPUT_PATH "/data/dump"
#else
#define DUMP_GMV_INPUT_PATH "/data/camera"
#endif

#define GMV_LIBRARY_PATH "/vendor/lib/libgmv.so"

namespace android {

class ExynosCameraPipeGMV : protected virtual ExynosCameraSWPipe {
public:
    ExynosCameraPipeGMV()
    {
        m_init();
    }

    ExynosCameraPipeGMV(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)  : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        start(void);
    virtual status_t        stop(void);

protected:
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    void                    m_init(void);

    /* Library APIs */
    void    (*getGlobalMotionEst)(const uint8_t *pkdQvga,
                                  const uint32_t *prevVecList, const uint32_t *currVecList,
                                  int *deltaX, int *deltaY);

    void    *m_gmvDl;
private:
    ExynosCameraDurationTimer       m_timer;

#ifdef DUMP_GMV_INPUT
    status_t        m_dumpGmvInput(const char *pkdQvga, const struct camera2_shot_ext *shotExt);
    FILE            *prevVecListFd;
    FILE            *currVecListFd;
#endif
};

}; /* namespace android */

#endif
