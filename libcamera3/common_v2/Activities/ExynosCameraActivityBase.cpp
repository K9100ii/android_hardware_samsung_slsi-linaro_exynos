/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraActivityBase"
#include <log/log.h>

#include "ExynosCameraActivityBase.h"

namespace android {

ExynosCameraActivityBase::ExynosCameraActivityBase(int cameraId)
{
    t_isExclusiveReq = false;
    t_isActivated = false;
    t_reqNum = 0;
    t_reqStatus = 0;
    m_cameraId = cameraId;
    memset(m_name, 0x00, sizeof(m_name));
}

ExynosCameraActivityBase::~ExynosCameraActivityBase()
{
}

int ExynosCameraActivityBase::execFunction(CALLBACK_TYPE callbackType, void *args)
{
    int ret = INVALID_OPERATION;

    switch (callbackType) {
    case CALLBACK_TYPE_SENSOR_BEFORE:
        ret = t_funcSensorBefore(args);
        break;
    case CALLBACK_TYPE_SENSOR_AFTER:
        ret = t_funcSensorAfter(args);
        break;
    case CALLBACK_TYPE_3A_BEFORE:
        ret = t_func3ABefore(args);
        break;
    case CALLBACK_TYPE_3A_AFTER:
        ret = t_func3AAfter(args);
        break;
    case CALLBACK_TYPE_3A_BEFORE_HAL3:
        ret = t_func3ABeforeHAL3(args);
        break;
    case CALLBACK_TYPE_3A_AFTER_HAL3:
        ret = t_func3AAfterHAL3(args);
        break;
    case CALLBACK_TYPE_ISP_BEFORE:
        ret = t_funcISPBefore(args);
        break;
    case CALLBACK_TYPE_ISP_AFTER:
        ret = t_funcISPAfter(args);
        break;
    case CALLBACK_TYPE_VRA_BEFORE:
        ret = t_funcVRABefore(args);
        break;
    case CALLBACK_TYPE_VRA_AFTER:
        ret = t_funcVRAAfter(args);
        break;
    default:
        CLOGE("invalid callbackType:%d", callbackType);
        break;
    }

    return ret;
}

} /* namespace android */
