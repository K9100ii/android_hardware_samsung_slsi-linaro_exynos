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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraActivityUCTL"
#include <log/log.h>

#include "ExynosCameraActivityUCTL.h"
//#include "ExynosCamera.h"

namespace android {

class ExynosCamera;

ExynosCameraActivityUCTL::ExynosCameraActivityUCTL(int cameraId) : ExynosCameraActivityBase(cameraId)
{
    m_metadata = NULL;
    m_rotation = 0;
}

ExynosCameraActivityUCTL::~ExynosCameraActivityUCTL()
{
}

int ExynosCameraActivityUCTL::t_funcNull(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_funcSensorBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_funcSensorAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_funcISPBefore(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_funcISPAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_func3ABefore(void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext != NULL) {
#ifdef FD_ROTATION
        shot_ext->shot.uctl.scalerUd.orientation = m_rotation;
#endif
    }

    return 1;
}

int ExynosCameraActivityUCTL::t_func3AAfter(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_func3ABeforeHAL3(__unused void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext != NULL) {
        if (m_metadata != NULL) {
#ifdef SAMSUNG_HRM
            if (m_metadata->shot.uctl.aaUd.hrmInfo.ir_data != 0)
                shot_ext->shot.uctl.aaUd.hrmInfo = m_metadata->shot.uctl.aaUd.hrmInfo;
#endif

#ifdef SAMSUNG_LIGHT_IR
            if (m_metadata->shot.uctl.aaUd.illuminationInfo.visible_exptime != 0)
                shot_ext->shot.uctl.aaUd.illuminationInfo = m_metadata->shot.uctl.aaUd.illuminationInfo;
#endif

#ifdef SAMSUNG_GYRO
            shot_ext->shot.uctl.aaUd.gyroInfo = m_metadata->shot.uctl.aaUd.gyroInfo;
#endif

#ifdef SAMSUNG_ACCELEROMETER
            shot_ext->shot.uctl.aaUd.accInfo = m_metadata->shot.uctl.aaUd.accInfo;
#endif

#ifdef SAMSUNG_PROX_FLICKER
            shot_ext->shot.uctl.aaUd.proximityInfo = m_metadata->shot.uctl.aaUd.proximityInfo;
#endif

#ifdef SAMSUNG_DOF
            if (m_metadata->shot.uctl.lensUd.pos > 0) {
                shot_ext->shot.uctl.lensUd.pos = m_metadata->shot.uctl.lensUd.pos;
                shot_ext->shot.uctl.lensUd.posSize = m_metadata->shot.uctl.lensUd.posSize;
                shot_ext->shot.uctl.lensUd.direction = m_metadata->shot.uctl.lensUd.direction;
                shot_ext->shot.uctl.lensUd.slewRate = m_metadata->shot.uctl.lensUd.slewRate;
            }
#endif

#ifdef SAMSUNG_SW_VDIS_USE_OIS
            shot_ext->shot.uctl.lensUd.oisCoefVal = m_metadata->shot.uctl.lensUd.oisCoefVal;
#endif
        }

#ifdef FD_ROTATION
        shot_ext->shot.uctl.scalerUd.orientation = m_rotation;
#endif
    }

    return 1;
}

int ExynosCameraActivityUCTL::t_func3AAfterHAL3(__unused void *args)
{
    return 1;
}

int ExynosCameraActivityUCTL::t_funcVRABefore(__unused void *args)
{
    ExynosCameraBuffer *buf = (ExynosCameraBuffer *)args;
    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)(buf->addr[buf->getMetaPlaneIndex()]);

    if (shot_ext != NULL) {
#ifdef FD_ROTATION
        shot_ext->shot.uctl.scalerUd.orientation = m_rotation;
#endif
    }
    CLOGV("[F(%d) B%d]-IN-",
                shot_ext->shot.dm.request.frameCount, buf->index);

    return 1;
}

int ExynosCameraActivityUCTL::t_funcVRAAfter(__unused void *args)
{
    CLOGV("");

    return 1;
}

void ExynosCameraActivityUCTL::setMetadata(struct camera2_shot_ext *metadata)
{
    if (metadata == NULL) {
        CLOGE("metadata is NULL");
        return;
    }

    m_metadata = metadata;
    return;
}

void ExynosCameraActivityUCTL::setDeviceRotation(int rotation)
{
    m_rotation = rotation;
}

int ExynosCameraActivityUCTL::getDeviceRotation(void)
{
    return m_rotation;
}
} /* namespace android */

