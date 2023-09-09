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

#define LOG_TAG "vdis"

#include "libvdis.h"

VDIS::VDIS()
{
    ALOGD("DEBUG(%s[%d]):new VDIS object allocated", __FUNCTION__, __LINE__);
}

VDIS::~VDIS()
{
    ALOGD("DEBUG(%s[%d]):new VDIS object deallocated", __FUNCTION__, __LINE__);
}

status_t VDIS::create(void)
{
    status_t ret = NO_ERROR;

#if 0
    m_emulationProcessTime = VDIS_PROCESSTIME_STANDARD;
    m_emulationCopyRatio = 1.0f;
#endif

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t VDIS::destroy(void)
{
    status_t ret = NO_ERROR;

    ALOGD("DEBUG(%s[%d]):", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t VDIS::execute(Map_t *map)
{
    status_t ret = NO_ERROR;
    nsecs_t start = systemTime();

    char *srcYAddr    = NULL;
    char *srcCbcrAddr = NULL;
    char *dstYAddr    = NULL;
    char *dstCbcrAddr = NULL;

    unsigned int bpp = 0;
    unsigned int planeCount  = 1;

    int srcPlaneSize = 0;
    int srcHalfPlaneSize = 0;
    int dstPlaneSize = 0;
    int dstHalfPlaneSize = 0;
    int copySize = 0;

    /* from map */
    int               mapSrcBufCnt      = (Data_int32_t     )(*map)[PLUGIN_SRC_BUF_CNT];
    Array_buf_t       mapSrcBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_PLANE_CNT];
    Array_buf_plane_t mapSrcBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_SIZE];
    Array_buf_rect_t  mapSrcRect        = (Array_buf_rect_t )(*map)[PLUGIN_SRC_BUF_RECT];
    Array_buf_t       mapSrcV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapSrcBufAddr[2];
    mapSrcBufAddr[0] = (Array_buf_addr_t)(*map)[PLUGIN_SRC_BUF_1];           //master
    mapSrcBufAddr[1] = (Array_buf_addr_t)(*map)[PLUGIN_SRC_BUF_2];           //slave

    Array_buf_t       mapSrcBufIndex    = (Array_buf_t     )(*map)[PLUGIN_SRC_BUF_INDEX];
    Array_buf_plane_t mapSrcBufFd       = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_FD];

    int               mapDstBufCnt      = (Data_int32_t     )(*map)[PLUGIN_DST_BUF_CNT];
    Array_buf_t       mapDstBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_PLANE_CNT];
    Array_buf_plane_t mapDstBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_SIZE];
    Array_buf_rect_t  mapDstRect        = (Array_buf_rect_t )(*map)[PLUGIN_DST_BUF_RECT];
    Array_buf_t       mapDstV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapDstBufAddr     = (Array_buf_addr_t )(*map)[PLUGIN_DST_BUF_1];   //target;

    Array_buf_t       mapDstBufIndex    = (Array_buf_t     )(*map)[PLUGIN_DST_BUF_INDEX];
    Array_buf_plane_t mapDstBufFd       = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_FD];

    Data_int64_t      mapTimestamp        = (Data_int64_t     )(*map)[PLUGIN_TIMESTAMP        ];
    Data_uint64_t      mapExposureTime     = (Data_uint64_t     )(*map)[PLUGIN_EXPOSURE_TIME    ];
    Data_uint64_t     mapTimestampBoot    = (Data_uint64_t     )(*map)[PLUGIN_TIMESTAMPBOOT    ];
    Data_int32_t      mapFrameDuration    = (Data_int32_t     )(*map)[PLUGIN_FRAME_DURATION   ];
    Data_int32_t      mapRollingShutterSkew    = (Data_int32_t     )(*map)[PLUGIN_ROLLING_SHUTTER_SKEW    ];
    Data_int32_t      mapOpticalStabilizationModeCtrl    = (Data_int32_t     )(*map)[PLUGIN_OPTICAL_STABILIZATION_MODE_CTRL    ];
    Data_int32_t      mapOpticalStabilizationModeDm    = (Data_int32_t     )(*map)[PLUGIN_OPTICAL_STABILIZATION_MODE_DM    ];

    Single_buf_t      mapMeDownscaledBuf  = (Single_buf_t)(*map)[PLUGIN_ME_DOWNSCALED_BUF];
    Single_buf_t      mapMotionVectorBuf  = (Single_buf_t)(*map)[PLUGIN_MOTION_VECTOR_BUF];
    Single_buf_t      mapCurrentPatchBuf  = (Single_buf_t)(*map)[PLUGIN_CURRENT_PATCH_BUF];
    Data_int32_t      mapMeFramecount     = (Data_int32_t)(*map)[PLUGIN_ME_FRAMECOUNT];

    Data_pointer_rect_t       mapBcropRect		= (Data_pointer_rect_t)(*map)[PLUGIN_BCROP_RECT];
    Data_int32_t      zoomLevel = (Data_int32_t)(*map)[PLUGIN_ZOOM_LEVEL];
    Pointer_float_t   zoomRatio = (Pointer_float_t)(*map)[PLUGIN_ZOOM_RATIO];
    VDIS_LOGD("bcrop (%d %d %d %d) zoom(%d, %f)", mapBcropRect->x, mapBcropRect->y, mapBcropRect->w, mapBcropRect->h, zoomLevel, zoomRatio[0]);

    (*map)[PLUGIN_SRC_BUF_USED] = (Map_data_t)mapSrcBufIndex[0];

    Data_int32_t      mapGyroDataSize = (Data_int32_t)(*map)[PLUGIN_GYRO_DATA_SIZE];
    Array_pointer_gyro_data_t mapGyroData_p = (Array_pointer_gyro_data_t)(*map)[PLUGIN_GYRO_DATA];
    for (int i = 0; i < mapGyroDataSize; i++) {
        VDIS_LOGD("gyro %x-%x-%x %llu",
                mapGyroData_p[i].x,
                mapGyroData_p[i].y,
                mapGyroData_p[i].z,
                mapGyroData_p[i].timestamp);
    }

#if 0
    VDIS_LOGD("timeStame(%lu), timeStampBoot(%lu), exposureTime(%lu), frameDuration(%d), RollingShutterSkew(%d), OpticalStabilizationModeCtrl(%d), OpticalStabilizationModeDm(%d)",
                mapTimestamp, mapTimestampBoot, mapExposureTime, mapFrameDuration, mapRollingShutterSkew, mapOpticalStabilizationModeCtrl, mapOpticalStabilizationModeDm);
#endif

    for (int i = 0; i < mapSrcBufCnt; i++) {
        VDIS_LOGD("%s(%d): src[%d/%d] idx(%d)::(adr:%p, P%d, S%d, FD%d [%d, %d, %d, %d, %d, %d] format :%d",
                __FUNCTION__, __LINE__,
                i,
                mapSrcBufCnt,
                mapSrcBufIndex[i],
                mapSrcBufAddr[i][0],
                mapSrcBufPlaneCnt[i],
                mapSrcBufSize[i][0],
                mapSrcBufFd[i][0],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_X],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_Y],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_W],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_H],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_W],
                mapSrcRect[i][PLUGIN_ARRAY_RECT_FULL_H],
                mapSrcV4L2Format[i]);
    }

    VDIS_LOGD("%s(%d): dst[%d] idx(%d)::(adr:%p, P%d, S%d, FD%d[%d, %d, %d, %d, %d, %d] format :%d",
            __FUNCTION__, __LINE__,
            mapDstBufCnt,
            mapDstBufIndex[0],
            mapDstBufAddr[0],
            mapDstBufPlaneCnt[0],
            mapDstBufSize[0][0],
            mapDstBufFd[0][0],
            mapDstRect[0][PLUGIN_ARRAY_RECT_X],
            mapDstRect[0][PLUGIN_ARRAY_RECT_Y],
            mapDstRect[0][PLUGIN_ARRAY_RECT_W],
            mapDstRect[0][PLUGIN_ARRAY_RECT_H],
            mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_W],
            mapDstRect[0][PLUGIN_ARRAY_RECT_FULL_H],
            mapDstV4L2Format[0]);

#if 0
    PlugInBuffer_t meBuffer;
    meBuffer.size = 320 * 240;
    meBuffer.addr = mapMeDownscaledBuf;
    int *meMeta1 = (int *)mapMotionVectorBuf;
    int *meMeta2 = (int *)mapCurrentPatchBuf;

    /* File Dump */
    dumpToFile("meDownScaled", &meBuffer, 1, mapMeFramecount);
    for (int i = 0; i < 200; i+=10) {
        PLUGIN_LOGD("[META1][F%d][%X, %X, %X, %X, %X   %X, %X %X, %X, %X]",
                    mapMeFramecount,
                    meMeta1[i], meMeta1[i+1], meMeta1[i+2], meMeta1[i+3], meMeta1[i+4],
                    meMeta1[i+5], meMeta1[i+6], meMeta1[i+7], meMeta1[i+8], meMeta1[i+9]);
    }

    for (int i = 0; i < 200; i+=10) {
        PLUGIN_LOGD("[META2][F%d][%X, %X, %X, %X, %X   %X, %X %X, %X, %X]",
                    mapMeFramecount,
                    meMeta2[i], meMeta2[i+1], meMeta2[i+2], meMeta2[i+3], meMeta2[i+4],
                    meMeta2[i+5], meMeta2[i+6], meMeta2[i+7], meMeta2[i+8], meMeta2[i+9]);
    }
#endif


    memcpy(mapDstBufAddr[0], mapSrcBufAddr[0][0], mapDstBufSize[0][0]);//y plane
    memcpy(mapDstBufAddr[1], mapSrcBufAddr[0][1], mapDstBufSize[0][1]);//cbcr plane
    memcpy(mapDstBufAddr[2], mapSrcBufAddr[0][2], mapDstBufSize[0][2]);//metaplane
    //memset(mapDstBufAddr[0], 0xff, mapDstBufSize[0][0]/2);

    return ret;
}

status_t VDIS::init(Map_t *map)
{
    return NO_ERROR;
}

status_t VDIS::setup(Map_t *map)
{
    int hwWidth =  (Data_int32_t     )(*map)[PLUGIN_ARRAY_RECT_FULL_W];
    int hwHeight = (Data_int32_t     )(*map)[PLUGIN_ARRAY_RECT_FULL_H];
    int width =    (Data_int32_t     )(*map)[PLUGIN_ARRAY_RECT_W];
    int height =   (Data_int32_t     )(*map)[PLUGIN_ARRAY_RECT_H];

    VDIS_LOGD("%s(%d) : input size(%d x %d), target size(%d x %d)", hwWidth, hwHeight, width, height);

    return NO_ERROR;
}

status_t VDIS::query(Map_t *map)
{
    if (map != NULL) {
        (*map)[PLUGIN_VERSION]                = (Map_data_t)MAKE_VERSION(1, 0);
        (*map)[PLUGIN_LIB_NAME]               = (Map_data_t) "VDISLib";
        (*map)[PLUGIN_BUILD_DATE]             = (Map_data_t)__DATE__;
        (*map)[PLUGIN_BUILD_TIME]             = (Map_data_t)__TIME__;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)1;
    }

    return NO_ERROR;
}

status_t VDIS::start(void)
{
    VDIS_LOGD("start");
    return NO_ERROR;
}

status_t VDIS::stop(void)
{
    VDIS_LOGD("stop");
    return NO_ERROR;
}

