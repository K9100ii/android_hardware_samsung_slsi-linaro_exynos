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
#define LOG_TAG "ExynosCameraPlugInArcsoftFusion"
#include <log/log.h>

#include "ExynosCameraPlugInArcsoftFusion.h"

namespace android {

volatile int32_t ExynosCameraPlugInArcsoftFusion::initCount = 0;

DECLARE_CREATE_PLUGIN_SYMBOL(ExynosCameraPlugInArcsoftFusion);

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInArcsoftFusion::m_init(void)
{
    int count = android_atomic_inc(&initCount);

    CLOGD("count(%d)", count);

    if (count == 1) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInArcsoftFusion::m_deinit(void)
{
    int count = android_atomic_dec(&initCount);

    CLOGD("count(%d)", count);

    if (count == 0) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInArcsoftFusion::m_create(void)
{
    CLOGD("");

#if 1
    int scenario = m_sceanrio;
#else
    char prop[PROPERTY_VALUE_MAX];
    memset(prop, 0, sizeof(prop));
    property_get("persist.camera.fusion.mode", prop, "0");
    int scenario = atoi(prop);
#endif

    // hack : it need to distinguish. zoom preview / zoom capture / bokeh.
    if (m_pipeId < 200) {
        // PIPE_FUSION, or PIPE_FUSION_FRONT
        if (scenario == PLUGIN_SCENARIO_ZOOMFUSION_PREVIEW) {
            CLOGD("new ArcsoftFusionZoomPreview");
            fusion = new ArcsoftFusionZoomPreview();
        } else {
            CLOGD("new ArcsoftFusionBokehPreview");
            fusion = new ArcsoftFusionBokehPreview();
        }
    } else {
        // PIPE_FUSION_REPROCESSING
        if (scenario == PLUGIN_SCENARIO_ZOOMFUSION_REPROCESSING) {
            CLOGD("new ArcsoftFusionZoomCapture");
            fusion = new ArcsoftFusionZoomCapture();
        } else {
            CLOGD("new ArcsoftFusionBokehCapture");
            fusion = new ArcsoftFusionBokehCapture();
        }
    }

    fusion->create();

    return NO_ERROR;
}

status_t ExynosCameraPlugInArcsoftFusion::m_destroy(void)
{
    CLOGD("");

    if (fusion) {
        fusion->destroy();
        delete fusion;
        fusion = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInArcsoftFusion::m_setup(Map_t *map)
{
    CLOGD("");

    fusion->uninit(map);

    return fusion->init(map);
}

status_t ExynosCameraPlugInArcsoftFusion::m_process(Map_t *map)
{
    return fusion->execute(map);
}

status_t ExynosCameraPlugInArcsoftFusion::m_setParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

status_t ExynosCameraPlugInArcsoftFusion::m_getParameter(int key, void *data)
{
    /* do nothing */
    return NO_ERROR;
}

void ExynosCameraPlugInArcsoftFusion::m_dump(void)
{
    /* do nothing */
}

status_t ExynosCameraPlugInArcsoftFusion::m_query(Map_t *map)
{
    return fusion->query(map);
}
}
