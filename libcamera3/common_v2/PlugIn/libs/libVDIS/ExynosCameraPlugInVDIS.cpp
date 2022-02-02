/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPlugInVDIS"
#include <log/log.h>

#include "ExynosCameraPlugInVDIS.h"

namespace android {

volatile int32_t ExynosCameraPlugInVDIS::initCount = 0;

DECLARE_CREATE_PLUGIN_SYMBOL(ExynosCameraPlugInVDIS);

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInVDIS::m_init(void)
{
    int count = android_atomic_inc(&initCount);

    CLOGD("count(%d)", count);

    if (count == 1) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_deinit(void)
{
    int count = android_atomic_dec(&initCount);

    CLOGD("count(%d)", count);

    if (count == 0) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_create(void)
{
    CLOGD("");

    vdis = new VDIS();
    vdis->create();
    strncpy(m_name, "VDIS_PLUGIN", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_destroy(void)
{
    CLOGD("");

    if (vdis) {
        vdis->destroy();
        delete vdis;
        vdis = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_setup(Map_t *map)
{
    CLOGD("");

    vdis->setup(map);
    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_process(Map_t *map)
{
    return vdis->execute(map);
}

status_t ExynosCameraPlugInVDIS::m_setParameter(int key, void *data)
{
    status_t ret = NO_ERROR;

    switch(key) {
    case PLUGIN_PARAMETER_KEY_START:
        ret = this->m_start();
        break;
    case PLUGIN_PARAMETER_KEY_STOP:
        ret = this->m_stop();
        break;
    default:
        CLOGW("Unknown key(%d)", key);
    }

    return ret;
}

status_t ExynosCameraPlugInVDIS::m_getParameter(int key, void *data)
{
    return NO_ERROR;
}

void ExynosCameraPlugInVDIS::m_dump(void)
{
    /* do nothing */
}

status_t ExynosCameraPlugInVDIS::m_query(Map_t *map)
{
    return vdis->query(map);
}

status_t ExynosCameraPlugInVDIS::m_start(void)
{
    vdis->start();

    return NO_ERROR;
}

status_t ExynosCameraPlugInVDIS::m_stop(void)
{
    vdis->stop();

    return NO_ERROR;
}

}
