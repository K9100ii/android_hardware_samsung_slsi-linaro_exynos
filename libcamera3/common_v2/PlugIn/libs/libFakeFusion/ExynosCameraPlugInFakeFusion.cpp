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
#define LOG_TAG "ExynosCameraPlugInFakeFusion"
#include <log/log.h>

#include "ExynosCameraPlugInFakeFusion.h"

namespace android {

volatile int32_t ExynosCameraPlugInFakeFusion::initCount = 0;

DECLARE_CREATE_PLUGIN_SYMBOL(ExynosCameraPlugInFakeFusion);

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInFakeFusion::m_init(void)
{
    int count = android_atomic_inc(&initCount);

    CLOGD("count(%d)", count);

    if (count == 1) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_deinit(void)
{
    int count = android_atomic_dec(&initCount);

    CLOGD("count(%d)", count);

    if (count == 0) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_create(void)
{
    CLOGD("");

    fusion = new FakeFusion();
    fusion->create();
    strncpy(m_name, "FakeFusionPL", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_destroy(void)
{
    CLOGD("");

    if (fusion) {
        fusion->destroy();
        delete fusion;
        fusion = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_setup(Map_t *map)
{
    CLOGD("");

    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_process(Map_t *map)
{
    return fusion->execute(map);
}

status_t ExynosCameraPlugInFakeFusion::m_setParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_getParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

void ExynosCameraPlugInFakeFusion::m_dump(void)
{
    /* do nothing */
}

status_t ExynosCameraPlugInFakeFusion::m_query(Map_t *map)
{
    return fusion->query(map);
}

status_t ExynosCameraPlugInFakeFusion::m_start(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInFakeFusion::m_stop(void)
{
    return NO_ERROR;
}

}
