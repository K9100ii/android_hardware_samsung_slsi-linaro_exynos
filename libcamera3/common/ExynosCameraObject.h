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

#ifndef EXYNOS_CAMERA_OBJECT_H
#define EXYNOS_CAMERA_OBJECT_H

#include "ExynosCameraCommonDefine.h"

namespace android {

class ExynosCameraObject {
public:
    /* Constructor */
    ExynosCameraObject()
    {
        m_cameraId = -1;
        memset(m_name, 0x00, sizeof(m_name));
    };

    status_t setCameraId(int cameraId)
    {
        m_cameraId = cameraId;

        return NO_ERROR;
    };

    int getCameraId(void)
    {
        return m_cameraId;
    };

    status_t setName(const char *name)
    {
        strncpy(m_name,  name,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        return NO_ERROR;
    };

    char *getName(void)
    {
        return m_name;
    };

protected:
    int                 m_cameraId;
    char                m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
};

}; /* namespace android */

#endif
