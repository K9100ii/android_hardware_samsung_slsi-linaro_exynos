/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraPlugInConverterLowLightShot"

#include "ExynosCameraPlugInConverterLowLightShot.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterLowLightShot::m_init(void)
{
    strncpy(m_name, "ConverterLowLightShot", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterLowLightShot::m_deinit(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterLowLightShot::m_create(Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterLowLightShot::m_setup(Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterLowLightShot::m_make(Map_t *map)
{
    status_t ret = NO_ERROR;
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    struct camera2_shot_ext *metaData;
    ExynosCameraBuffer buffer;

    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];
    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];

    switch (type) {
    case PLUGIN_CONVERT_PROCESS_BEFORE:
        if (frame == NULL) {
            CLOGE("frame is NULL!! type(%d), pipeId(%d)", type, m_pipeId);
            goto func_exit;
        }

        /* meta data setting */
        ret = frame->getSrcBuffer(m_pipeId, &buffer, OUTPUT_NODE_1);
        if (ret < 0 || buffer.index < 0)
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", m_pipeId, ret);

        metaData = (struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
        frame->getMetaData(metaData);
        (*map)[PLUGIN_SRC_FRAMECOUNT] = (Map_data_t)frame->getMetaFrameCount();
        (*map)[PLUGIN_ME_FRAMECOUNT ] = (Map_data_t)frame->getMetaFrameCount();
        (*map)[PLUGIN_TIMESTAMP     ] = (Map_data_t)frame->getTimeStamp();
        (*map)[PLUGIN_EXPOSURE_TIME ] = (Map_data_t)metaData->shot.dm.sensor.exposureTime;
        (*map)[PLUGIN_ISO           ] = (Map_data_t)metaData->shot.dm.aa.vendor_isoValue;

#ifdef SUPPORT_ME
        ret = frame->getSrcBuffer(m_pipeId, &buffer, OUTPUT_NODE_2);
        if (ret < 0 || buffer.index < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", m_pipeId, ret);
        } else {
            (*map)[PLUGIN_ME_DOWNSCALED_BUF] = (Map_data_t)buffer.addr[0];
            (*map)[PLUGIN_MOTION_VECTOR_BUF] = (Map_data_t)&(metaData->shot.udm.me.motion_vector);
            (*map)[PLUGIN_CURRENT_PATCH_BUF] = (Map_data_t)&(metaData->shot.udm.me.current_patch);
        }
#endif
        /* intent setting */
        switch (m_pipeId) {
        case PIPE_LLS_REPROCESSING:
            CLOGD("frame fcount(%d), metaFcount(%d), index(%d)",
                    frame->getFrameCount(),
                    frame->getMetaFrameCount(),
                    frame->getFrameIndex());

            /* first frame!! */
            if (frame->getFrameIndex() == 1)
                (*map)[PLUGIN_LLS_INTENT] = CAPTURE_START;
            else
                (*map)[PLUGIN_LLS_INTENT] = CAPTURE_PROCESS;
            break;
        }
    case PLUGIN_CONVERT_PROCESS_AFTER:
        break;
    case PLUGIN_CONVERT_SETUP_AFTER:
        (*map)[PLUGIN_SRC_FRAMECOUNT] = (Map_data_t)frame->getMetaFrameCount();
        break;
    default:
        CLOGE("invalid convert type(%d)!! pipeId(%d)", type, m_pipeId);
        goto func_exit;
    }

func_exit:

    return NO_ERROR;
}
}; /* namespace android */
