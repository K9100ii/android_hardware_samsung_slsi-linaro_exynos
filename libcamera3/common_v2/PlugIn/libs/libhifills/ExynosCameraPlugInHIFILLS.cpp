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
#define LOG_TAG "ExynosCameraPlugInHIFILLS"
#include <log/log.h>

#include "ExynosCameraPlugInHIFILLS.h"

namespace android {

volatile int32_t ExynosCameraPlugInHIFILLS::initCount = 0;

DECLARE_CREATE_PLUGIN_SYMBOL(ExynosCameraPlugInHIFILLS);

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInHIFILLS::m_init(void)
{
    int count = android_atomic_inc(&initCount);

    CLOGD("count(%d)", count);

    if (count == 1) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_deinit(void)
{
    int count = android_atomic_dec(&initCount);

    CLOGD("count(%d)", count);

    if (count == 0) {
        /* do nothing */
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_create(void)
{
    CLOGD("");

    strncpy(m_name, "HIFILLS_PLUGIN", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_destroy(void)
{
    CLOGD("");

    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_setup(Map_t *map)
{
    CLOGD("");

    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_process(Map_t *map)
{
    status_t ret = NO_ERROR;

    int frameCurIndex = 0;
    int frameMaxIndex = 0;
    int operationMode = 0;
    int frameCount = 0;
    int64_t timestamp = 0;
    bool crop_flag = false;
    int rearOrFront = 0;
    int sensorType = 0;
    int hdr_mode = 0;
    int sensorW = 0, sensorH = 0;
    int pictureW = 0, pictureH = 0;
    Array_buf_t shutterspeed;
    Array_buf_t iso;
    Data_pointer_rect_t bcrop;
    Data_pointer_rect_t faceRect;
    int faceCount = 0;
    uint64_t exposureTime;
    uint64_t timestampBoot;
    float zoomRatio;
    int brightness;
    int inputW = 0, inputH;

    frameCount    = (Data_int32_t     )(*map)[PLUGIN_SRC_FRAMECOUNT];
    timestamp     = (Data_int64_t     )(*map)[PLUGIN_TIMESTAMP  ];
    exposureTime  = (Data_uint64_t    )(*map)[PLUGIN_EXPOSURE_TIME ];
    timestampBoot = (Data_uint64_t    )(*map)[PLUGIN_TIMESTAMPBOOT ];

    frameMaxIndex = (Data_int32_t     )(*map)[PLUGIN_HIFI_TOTAL_BUFFER_NUM];
    frameCurIndex = (Data_int32_t     )(*map)[PLUGIN_HIFI_CUR_BUFFER_NUM];

    operationMode = (Data_int32_t     )(*map)[PLUGIN_HIFI_OPERATION_MODE];

    sensorW       = (Data_int32_t     )(*map)[PLUGIN_HIFI_MAX_SENSOR_WIDTH];
    sensorH       = (Data_int32_t     )(*map)[PLUGIN_HIFI_MAX_SENSOR_HEIGHT];

    crop_flag     = (Data_bool_t      )(*map)[PLUGIN_HIFI_CROP_FLAG];

    rearOrFront   = (Data_int32_t     )(*map)[PLUGIN_HIFI_CAMERA_TYPE];

    sensorType    = (Data_int32_t     )(*map)[PLUGIN_HIFI_SENSOR_TYPE];

    hdr_mode      = (Data_int32_t     )(*map)[PLUGIN_HIFI_HDR_MODE];

    zoomRatio     = (Data_float_t     )(*map)[PLUGIN_ZOOM_RATIO];

    shutterspeed  = (Array_buf_t      )(*map)[PLUGIN_HIFI_SHUTTER_SPEED];

    brightness    = (Data_int32_t     )(*map)[PLUGIN_HIFI_BV]; /* brightnessValue */

    iso           = (Array_buf_t      )(*map)[PLUGIN_HIFI_FRAME_ISO]; /* shot, long iso*/

    bcrop         = (Data_pointer_rect_t)(*map)[PLUGIN_HIFI_FOV_RECT];

    faceCount     = (Data_int32_t       )(*map)[PLUGIN_HIFI_FRAME_FACENUM];
    faceRect      = (Data_pointer_rect_t)(*map)[PLUGIN_HIFI_FRAME_FACERECT];

#if 1//def ENABLE_DEBUG
    CLOGD("Detected Face count(%d) Dump Start", faceCount);
    for(int i = 0; i < faceCount ; i++) {
        CLOGD("Detected Face(%d %d %d %d)", faceRect[i].x, faceRect[i].y, faceRect[i].w, faceRect[i].h);
    }
    CLOGD("Detected Face count(%d) Dump End", faceCount);
#endif

    inputW       = (Data_int32_t     )(*map)[PLUGIN_HIFI_INPUT_WIDTH];
    inputH       = (Data_int32_t     )(*map)[PLUGIN_HIFI_INPUT_HEIGHT];

    pictureW       = (Data_int32_t     )(*map)[PLUGIN_HIFI_OUTPUT_WIDTH];
    pictureH       = (Data_int32_t     )(*map)[PLUGIN_HIFI_OUTPUT_HEIGHT];

#if 1//def ENABLE_DEBUG
    CLOGD("frame(%d) count(%d / %d) oper(%d) sensor(%d %d) crop_flag(%d) facing(%d) sensorType(%d) hdr(%d) zoom(%f) shutter(%d %d) bv(%d) iso(%d) bcrop(%d %d %d %d) input(%d %d) output(%d %d) ", 
          frameCount,
          frameMaxIndex, frameCurIndex,
          operationMode,
          sensorW, sensorH,
          crop_flag,
          rearOrFront,
          sensorType,
          hdr_mode,
          zoomRatio,
          shutterspeed[0], shutterspeed[1],
          brightness,
          iso[0],
          bcrop->x,bcrop->y,bcrop->w,bcrop->h,
          inputW, inputH,
          pictureW, pictureH);
#endif

    if (frameCurIndex == 0) {
    /* 1st frame */
#if 1//def ENABLE_DEBUG
        CLOGD("HIFILLS 1st frame(%d %d)", frameMaxIndex, frameCurIndex);
#endif
    }

    int               mapSrcBufCnt      = (Data_int32_t     )(*map)[PLUGIN_SRC_BUF_CNT];
    Array_buf_t       mapSrcBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_PLANE_CNT];
    Array_buf_plane_t mapSrcBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_SIZE];
    Array_buf_rect_t  mapSrcRect        = (Array_buf_rect_t )(*map)[PLUGIN_SRC_BUF_RECT];
    Array_buf_t       mapSrcV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapSrcBufAddr[2];
    mapSrcBufAddr[0] = (Array_buf_addr_t)(*map)[PLUGIN_SRC_BUF_1];           //master
    mapSrcBufAddr[1] = (Array_buf_addr_t)(*map)[PLUGIN_SRC_BUF_2];           //slave

    int               mapDstBufCnt      = (Data_int32_t     )(*map)[PLUGIN_DST_BUF_CNT];
    Array_buf_t       mapDstBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_PLANE_CNT];
    Array_buf_plane_t mapDstBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_SIZE];
    Array_buf_rect_t  mapDstRect        = (Array_buf_rect_t )(*map)[PLUGIN_DST_BUF_RECT];
    Array_buf_t       mapDstV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapDstBufAddr     = (Array_buf_addr_t )(*map)[PLUGIN_DST_BUF_1];   //target;


#if 1//def ENABLE_DEBUG
    for (int i = 0; i < mapSrcBufCnt; i++) {
        PLUGIN_LOGD("HIFILLS src[%d/%d]::(addr:%p, P%d, S%d, [%d, %d, %d, %d, %d, %d] format :%d",
                i,
                mapSrcBufCnt,
                mapSrcBufAddr[i][0],
                mapSrcBufPlaneCnt[i],
                mapSrcBufSize[i][0],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_X],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_Y],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_W],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_H],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H],
                mapSrcV4L2Format[i]);


    }

    for (int i = 0; i < mapDstBufCnt; i++) {
        PLUGIN_LOGD("HIFILLS dst[%d]::(addr:%p, P%d, S%d, [%d, %d, %d, %d, %d, %d] format :%d",
                mapDstBufCnt,
                mapDstBufAddr[0],
                mapDstBufPlaneCnt[0],
                mapDstBufSize[0][0],
                mapDstRect[0][PLUGIN_ARRAY_RECT_X],
                mapDstRect[0][PLUGIN_ARRAY_RECT_Y],
                mapDstRect[0][PLUGIN_ARRAY_RECT_W],
                mapDstRect[0][PLUGIN_ARRAY_RECT_H],
                mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_W],
                mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_H],
                mapDstV4L2Format[0]);
    }
#endif

    if (frameCurIndex == (frameMaxIndex-1)) {
        /* last frame */
#if 1//def ENABLE_DEBUG
        CLOGD("HIFILLS last frame(%d %d)", frameMaxIndex, frameCurIndex);
#endif

    }

    return ret;
}

status_t ExynosCameraPlugInHIFILLS::m_setParameter(int key, void *data)
{
    status_t ret = NO_ERROR;

    switch(key) {
    case PLUGIN_PARAMETER_KEY_START:
        break;
    case PLUGIN_PARAMETER_KEY_STOP:
        break;
    default:
        CLOGW("Unknown key(%d)", key);
    }

    return ret;
}

status_t ExynosCameraPlugInHIFILLS::m_getParameter(int key, void *data)
{
    return NO_ERROR;
}

void ExynosCameraPlugInHIFILLS::m_dump(void)
{
    /* do nothing */
}

status_t ExynosCameraPlugInHIFILLS::m_query(Map_t *map)
{
    status_t ret = NO_ERROR;
    if (map != NULL) {
        (*map)[PLUGIN_VERSION]                = (Map_data_t)MAKE_VERSION(1, 0);
        (*map)[PLUGIN_LIB_NAME]               = (Map_data_t) "SAMSUNG_HIFILLS";
        (*map)[PLUGIN_BUILD_DATE]             = (Map_data_t)__DATE__;
        (*map)[PLUGIN_BUILD_TIME]             = (Map_data_t)__TIME__;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)1;
    }
    return ret;
}

status_t ExynosCameraPlugInHIFILLS::m_start(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInHIFILLS::m_stop(void)
{
    return NO_ERROR;
}

}
