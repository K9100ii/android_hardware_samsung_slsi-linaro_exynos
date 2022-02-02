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
#define LOG_TAG "ExynosCameraPlugInConverterHifills"

#include "ExynosCameraPlugInConverterHifills.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterHifills::m_init(void)
{
    strncpy(m_name, "CoverterHIFILLS", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterHifills::m_deinit(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterHifills::m_create(Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterHifills::m_setup(Map_t *map)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterHifills::m_make(Map_t *map)
{
    status_t ret = NO_ERROR;

#if 1
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    struct camera2_shot_ext *metaData;
    ExynosCameraBuffer buffer;
    ExynosCameraConfigurations *configurations = NULL;
    ExynosCameraParameters *parameter = NULL;
    plugin_rect_t rect;

    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];
    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];
    configurations = (ExynosCameraConfigurations *)(*map)[PLUGIN_CONVERT_CONFIGURATIONS];
    parameter = (ExynosCameraParameters *)(*map)[PLUGIN_CONVERT_PARAMETER];
    float zoomRatio;

    camera2_node_group node_group_info_bcrop;

    switch (type) {
    case PLUGIN_CONVERT_PROCESS_BEFORE:
    {
        int frameCurIndex = 0;
        int frameMaxIndex = 0;
        int operationMode = 0;
        bool crop_flag = false;
        int rearOrFront = 0;
        int sensorType = 0;
        int hdr_mode = 0;
        int sensorW = 0, sensorH = 0;
        int pictureW = 0, pictureH = 0;
        int faceCount = 0;
        struct camera2_dm *dm = NULL;

        if (frame == NULL) {
            CLOGE("frame is NULL!! type(%d), pipeId(%d)", type, m_pipeId);
            goto func_exit;
        }

        /* meta data setting */
        ret = frame->getSrcBuffer(m_pipeId, &buffer);
        if (ret < 0 || buffer.index < 0)
            CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", m_pipeId, ret);

#if 0
        dumpToFile("/data/camera/dump.raw", buffer.addr[0], buffer.size[0]);
#endif

        frameCurIndex = frame->getFrameIndex();
        frameMaxIndex = frame->getMaxFrameIndex();
        operationMode = 0 /* 0 : LLS, 1 : Best Pick */;
        crop_flag = (parameter->getUsePureBayerReprocessing() == true)? 0 : 1; /* pure 0 processed 1 */
        rearOrFront = (frame->getCameraId() == 0)? 0: 1; /* facing back 0 front 1 */
        sensorType = frame->getCameraId(); /* camera id for tunning */
        hdr_mode = 0 ; /* HDR off 0, HDR on 1, HDR auto 2*/
        zoomRatio = configurations->getZoomRatio();
        parameter->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&sensorW, (uint32_t *)&sensorH);
        parameter->getSize(HW_INFO_HW_PICTURE_SIZE, (uint32_t *)&pictureW, (uint32_t *)&pictureH);

        parameter->getPreviewBayerCropSize(&m_bnsSize, &m_bayerCropSize);

        metaData = (struct camera2_shot_ext *)buffer.addr[buffer.getMetaPlaneIndex()];
        (*map)[PLUGIN_SRC_FRAMECOUNT] = (Map_data_t)frame->getMetaFrameCount();
        (*map)[PLUGIN_TIMESTAMP     ] = (Map_data_t)frame->getTimeStamp();
        (*map)[PLUGIN_EXPOSURE_TIME ] = (Map_data_t)metaData->shot.dm.sensor.exposureTime;
        (*map)[PLUGIN_TIMESTAMPBOOT ] = (Map_data_t)metaData->shot.udm.sensor.timeStampBoot;
        (*map)[PLUGIN_FRAME_DURATION] = (Map_data_t)metaData->shot.ctl.sensor.frameDuration;
        (*map)[PLUGIN_SRC_BUF_USED]   = (Map_data_t)-1;
        (*map)[PLUGIN_DST_BUF_VALID]  = (Map_data_t)1;

        (*map)[PLUGIN_HIFI_TOTAL_BUFFER_NUM]    = (Map_data_t)frameMaxIndex;
        (*map)[PLUGIN_HIFI_CUR_BUFFER_NUM]      = (Map_data_t)frameCurIndex;
        (*map)[PLUGIN_HIFI_OPERATION_MODE]      = (Map_data_t)operationMode;
        (*map)[PLUGIN_HIFI_MAX_SENSOR_WIDTH]    = (Map_data_t)sensorW;
        (*map)[PLUGIN_HIFI_MAX_SENSOR_HEIGHT]   = (Map_data_t)sensorH;
        (*map)[PLUGIN_HIFI_CROP_FLAG]           = (Map_data_t)crop_flag;
        (*map)[PLUGIN_HIFI_CAMERA_TYPE]         = (Map_data_t)rearOrFront;
        (*map)[PLUGIN_HIFI_SENSOR_TYPE]         = (Map_data_t)sensorType;
        (*map)[PLUGIN_HIFI_HDR_MODE]            = (Map_data_t)hdr_mode;
        (*map)[PLUGIN_ZOOM_RATIO]               = (Map_data_t)zoomRatio;

        m_shutterspeed[0] = metaData->shot.udm.ae.vendorSpecific[390]; /* short shutter speed(us) */
        m_shutterspeed[1] = metaData->shot.udm.ae.vendorSpecific[392]; /* long shutter speed(us) */
        (*map)[PLUGIN_HIFI_SHUTTER_SPEED]       = (Map_data_t)&m_shutterspeed[0];

#if 0 // metadata is not support SLSI
        (*map)[PLUGIN_HIFI_BV]                  = (Map_data_t)metaData->shot.udm.ae.vendorSpecific[5]; /* brightnessValue.snum */
#else
        (*map)[PLUGIN_HIFI_BV]                  = (Map_data_t)metaData->shot.udm.internal.vendorSpecific[2]; /* brightnessValue */
#endif



#if 0 // metadata is not support SLSI
        iso[0] = metaData->shot.udm.ae.vendorSpecific[391]; /* short ISO */
        iso[1] = metaData->shot.udm.ae.vendorSpecific[393]; /* long ISO */
#else
        m_iso[0] = metaData->shot.udm.internal.vendorSpecific[1];
        m_iso[1] = metaData->shot.udm.internal.vendorSpecific[1];
#endif
        (*map)[PLUGIN_HIFI_FRAME_ISO]           = (Map_data_t)&m_iso[0]; /* shot, long iso*/

        m_bcrop.x = m_bayerCropSize.x;
        m_bcrop.y = m_bayerCropSize.y;
        m_bcrop.w = m_bayerCropSize.w;
        m_bcrop.h = m_bayerCropSize.h;
        (*map)[PLUGIN_HIFI_FOV_RECT]           = (Map_data_t)&m_bcrop; /* crop size */

        /* 1. calibration the Face detection data. */
        metaData->shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_SIMPLE;

        dm = &(metaData->shot.dm);
        if (dm != NULL) {
            ExynosRect vraInputSize;
            ExynosRect2 detectedFace[NUM_OF_DETECTED_FACES];

            memset(detectedFace, 0x00, sizeof(detectedFace));
            memset(m_faceRect, 0x00, sizeof(m_faceRect));

            if (parameter->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
                parameter->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h, parameter->getDsInputPortId(false));
#else
                parameter->getHwVraInputSize(&vraInputSize.w, &vraInputSize.h,
                                                metaData->shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
            } else {
                parameter->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSize.w, (uint32_t *)&vraInputSize.h, 0);
            }
            CLOGD("[LLS_MBR] VRA Size(%dx%d)", vraInputSize.w, vraInputSize.h);

            if (dm->stats.faceDetectMode != FACEDETECT_MODE_OFF) {
                if (sensorType == CAMERA_ID_FRONT
                    && configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL) == true) {
                    /* convert left, right values when flip horizontal(front camera) */
                    for (int i = 0; i < 10 /*NUM_OF_DETECTED_FACES */; i++) {
                        if (dm->stats.faceIds[i]) {
                            detectedFace[i].x1 = vraInputSize.w - dm->stats.faceRectangles[i][2];
                            detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                            detectedFace[i].x2 = vraInputSize.w - dm->stats.faceRectangles[i][0];
                            detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                        }
                    }
                } else {
                    for (int i = 0; i < 10 /*NUM_OF_DETECTED_FACES */; i++) {
                        if (dm->stats.faceIds[i]) {
                            detectedFace[i].x1 = dm->stats.faceRectangles[i][0];
                            detectedFace[i].y1 = dm->stats.faceRectangles[i][1];
                            detectedFace[i].x2 = dm->stats.faceRectangles[i][2];
                            detectedFace[i].y2 = dm->stats.faceRectangles[i][3];
                        }
                    }
                }
            }

            for (int i = 0; i < 10 /*NUM_OF_DETECTED_FACES */; i++) {
                if (dm->stats.faceIds[i]) {
                    m_faceRect[i].x   = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x1) - 1000;
                    m_faceRect[i].y   = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y1) - 1000;
                    m_faceRect[i].w  = calibratePosition(vraInputSize.w, 2000, detectedFace[i].x2) - 1000;
                    m_faceRect[i].h = calibratePosition(vraInputSize.h, 2000, detectedFace[i].y2) - 1000;
                    CLOGD("PlugInConverter Detected Face(%d %d %d %d", m_faceRect[i].x, m_faceRect[i].y, m_faceRect[i].w, m_faceRect[i].h);
                    faceCount++;
                }
            }
        }

        CLOGD("PlugInConverter Detected FaceCount(%d) ", faceCount);

        (*map)[PLUGIN_HIFI_FRAME_FACENUM]      = (Map_data_t)faceCount;
        (*map)[PLUGIN_HIFI_FRAME_FACERECT]     = (Map_data_t)&m_faceRect;

        (*map)[PLUGIN_HIFI_INPUT_WIDTH]        = (Map_data_t)m_bcrop.w;
        (*map)[PLUGIN_HIFI_INPUT_HEIGHT]       = (Map_data_t)m_bcrop.h;

        (*map)[PLUGIN_HIFI_OUTPUT_WIDTH]       = (Map_data_t)pictureW;
        (*map)[PLUGIN_HIFI_OUTPUT_HEIGHT]      = (Map_data_t)pictureH;
#if 1
        CLOGD("frame(%d) count(%d / %d) buffer(%p) oper(%d) sensor(%d %d) crop_flag(%d) facing(%d) sensorType(%d) hdr(%d) zoom(%f) shutter(%d %d) bv(%d) iso(%d) bcrop(%d %d %d %d) input(%d %d) output(%d %d) ", 
              frame->getMetaFrameCount(),
              frameMaxIndex,
              frameCurIndex,
              buffer.addr[0],
              operationMode,
              sensorW, sensorH,
              crop_flag,
              rearOrFront,
              sensorType,
              hdr_mode,
              zoomRatio,
              m_shutterspeed[0], m_shutterspeed[1],
              metaData->shot.udm.internal.vendorSpecific[2],
              metaData->shot.udm.internal.vendorSpecific[1],
              m_bcrop.x,m_bcrop.y,m_bcrop.w,m_bcrop.h,
              m_bcrop.w, m_bcrop.h,
              pictureW,pictureH);
#endif
    }
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
#endif
    return NO_ERROR;
}
}; /* namespace android */
