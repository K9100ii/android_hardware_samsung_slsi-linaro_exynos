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
#define LOG_TAG "ExynosCameraPlugInConverterVDIS"

#include "ExynosCameraPlugInConverterVDIS.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterVDIS::m_init(void)
{
    strncpy(m_name, "ConverterVDIS", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterVDIS::m_deinit(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterVDIS::m_create(Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterVDIS::m_setup(Map_t *map)
{
    ExynosCameraParameters *parameters = NULL;
    ExynosCameraConfigurations *configurations = NULL;

    parameters = (ExynosCameraParameters *)(*map)[PLUGIN_CONVERT_PARAMETER];
    configurations = (ExynosCameraConfigurations *)(*map)[PLUGIN_CONVERT_CONFIGURATIONS];

    uint32_t width = 0, height = 0;

    //parameters->getSize(HW_INFO_HW_YUV_SIZE, &width, &height, parameters->getRecordingPortId());
    (*map)[PLUGIN_ARRAY_RECT_FULL_W] = (Map_data_t)VIDEO_MARGIN_FHD_W;
    (*map)[PLUGIN_ARRAY_RECT_FULL_H] = (Map_data_t)VIDEO_MARGIN_FHD_H;

    //configurations->getSize(CONFIGURATION_YUV_SIZE, &width, &height);
    (*map)[PLUGIN_ARRAY_RECT_W] = (Map_data_t)1920;
    (*map)[PLUGIN_ARRAY_RECT_H] = (Map_data_t)1080;

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterVDIS::m_make(Map_t *map)
{
    status_t ret = NO_ERROR;
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    struct camera2_shot_ext *metaData;
    ExynosCameraBuffer buffer;
    ExynosCameraConfigurations *configurations = NULL;

    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];
    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];
    configurations = (ExynosCameraConfigurations *)(*map)[PLUGIN_CONVERT_CONFIGURATIONS];

#ifdef USE_MSL_VDIS_GYRO
    camera_gyro_data_t* gyroData;
    Array_pointer_gyro_data_t pGyroData_t;
#endif

    camera2_node_group node_group_info_bcrop;

#if 0
    char filePath[70];
    time_t rawtime;
    struct tm* timeinfo;
    int meW = 320;
    int meH = 240;
#endif

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
        (*map)[PLUGIN_TIMESTAMPBOOT ] = (Map_data_t)metaData->shot.udm.sensor.timeStampBoot;
        (*map)[PLUGIN_FRAME_DURATION] = (Map_data_t)metaData->shot.ctl.sensor.frameDuration;
        (*map)[PLUGIN_ROLLING_SHUTTER_SKEW] = (Map_data_t)metaData->shot.dm.sensor.rollingShutterSkew;
        (*map)[PLUGIN_OPTICAL_STABILIZATION_MODE_CTRL] = (Map_data_t)metaData->shot.ctl.lens.opticalStabilizationMode;
        (*map)[PLUGIN_OPTICAL_STABILIZATION_MODE_DM] = (Map_data_t)metaData->shot.dm.lens.opticalStabilizationMode;
        (*map)[PLUGIN_SRC_BUF_USED]   = (Map_data_t)-1;
        (*map)[PLUGIN_DST_BUF_VALID]  = (Map_data_t)1;


        frame->getNodeGroupInfo(&node_group_info_bcrop, PERFRAME_INFO_FLITE);
        m_bcrop.x = node_group_info_bcrop.leader.input.cropRegion[0];
        m_bcrop.y = node_group_info_bcrop.leader.input.cropRegion[1];
        m_bcrop.w = node_group_info_bcrop.leader.input.cropRegion[2];
        m_bcrop.h = node_group_info_bcrop.leader.input.cropRegion[3];
        //CLOGD("bcrop (%d %d %d %d)", m_bcrop.x, m_bcrop.y, m_bcrop.w, m_bcrop.h);
        (*map)[PLUGIN_BCROP_RECT] = (Map_data_t)&m_bcrop;

        m_zoomRatio[0] = configurations->getZoomRatio();
        (*map)[PLUGIN_ZOOM_RATIO] = (Map_data_t)m_zoomRatio;

#ifdef USE_MSL_VDIS_GYRO
        gyroData = frame->getGyroData();
        pGyroData_t = (Array_pointer_gyro_data_t)gyroData;
        (*map)[PLUGIN_GYRO_DATA] = (Map_data_t)pGyroData_t;
        (*map)[PLUGIN_GYRO_DATA_SIZE] = (Map_data_t)VDIS_GYRO_DATA_SIZE;

        for (int i=0; i<VDIS_GYRO_DATA_SIZE; i++) {
            CLOGE("gyro %x-%x-%x %llu",
                    pGyroData_t[i].x,
                    pGyroData_t[i].y,
                    pGyroData_t[i].z,
                    pGyroData_t[i].timestamp);
        }
#endif

#if 0
        CLOGD("timeStamp(%lu), timeStampBoot(%lu), exposureTime(%lu), frameDuration(%d), RollingShutterSkew(%d), OpticalStabilizationModeCtrl(%d), OpticalStabilizationModeDm(%d)",
                frame->getTimeStamp(),
                metaData->shot.udm.sensor.timeStampBoot, metaData->shot.dm.sensor.exposureTime, metaData->shot.ctl.sensor.frameDuration,
                metaData->shot.dm.sensor.rollingShutterSkew, metaData->shot.ctl.lens.opticalStabilizationMode, metaData->shot.dm.lens.opticalStabilizationMode);
#endif

#ifdef SUPPORT_ME
        ret = frame->getSrcBuffer(m_pipeId, &buffer, 1);
        if (ret < 0 || buffer.index < 0) {
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", m_pipeId, ret);
        } else {
            (*map)[PLUGIN_ME_DOWNSCALED_BUF] = (Map_data_t)buffer.addr[0];
            (*map)[PLUGIN_MOTION_VECTOR_BUF] = (Map_data_t)&(metaData->shot.udm.me.motion_vector);
            (*map)[PLUGIN_CURRENT_PATCH_BUF] = (Map_data_t)&(metaData->shot.udm.me.current_patch);
        }

#if 0
        memset(filePath, 0, sizeof(filePath));
        time(&rawtime);
        timeinfo = localtime(&rawtime);
        snprintf(filePath, sizeof(filePath), "/data/misc/cameraserver/ME_%d_%d_%02d%02d%02d_%02d%02d%02d.y12",
        m_cameraId, frame->getFrameCount(), timeinfo->tm_year+1900, timeinfo->tm_mon+1, timeinfo->tm_mday,
        timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

        CLOGD("[ME_DUMP] size (%d x %d) : %s", meW, meH, filePath);

        ret = dumpToFile((char *)filePath, buffer.addr[0], meW * meH);

        if (ret != true) {
            CLOGE("couldn't make a raw file");
        }
#endif

#endif

        break;
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
