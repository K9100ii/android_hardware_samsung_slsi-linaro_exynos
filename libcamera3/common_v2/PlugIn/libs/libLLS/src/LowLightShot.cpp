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

#define LOG_TAG "LowLightShot"

#include "LowLightShot.h"
#include "videodev2_exynos_camera.h"

LowLightShot::LowLightShot()
{
    PLUGIN_LOGD("new LowLightShot object allocated");
}

LowLightShot::~LowLightShot()
{
    PLUGIN_LOGD("new LowLightShot object deallocated");
}

status_t LowLightShot::create(void)
{
    status_t ret = NO_ERROR;

    PLUGIN_LOGD("");

    m_gdcPrepareThread = new LowLightShotThread(this, &LowLightShot::m_gdcPrepareThreadFunc, "m_gdcPrepareThreadFunc");
    PLUGIN_LOGD("Memory Thread created");

    allocator = new ExynosCameraPlugInUtilsIonAllocator(true);

    return NO_ERROR;
}

status_t LowLightShot::destroy(void)
{
    status_t ret = NO_ERROR;

    PLUGIN_LOGD("");

    m_gdcPrepareThread->requestExitAndWait();
    PLUGIN_LOGD("Memory Thread exit");

    /* free all memory */
    for (int i = 0; i < 2; i++) {
        if (m_gdcDstBuf[i].fd >= 0) {
            allocator->free(&m_gdcDstBuf[i].fd, m_gdcDstBuf[i].size, &m_gdcDstBuf[i].addr, true);
            if (ret < 0)
                PLUGIN_LOGE("free fail(%d) ret(%d)", m_gdcDstBuf[i].size, ret);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (m_gdcSrcBuf[i].fd >= 0) {
            allocator->free(&m_gdcSrcBuf[i].fd, m_gdcSrcBuf[i].size, &m_gdcSrcBuf[i].addr, true);
            if (ret < 0)
                PLUGIN_LOGE("free fail(%d) ret(%d)", m_gdcSrcBuf[i].size, ret);
        }
    }

    if (m_mergeBuf.fd >= 0) {
        allocator->free(&m_mergeBuf.fd, m_mergeBuf.size, &m_mergeBuf.addr, true);
        if (ret < 0)
            PLUGIN_LOGE("free fail(%d) ret(%d)", m_mergeBuf.size, ret);
    }

    if (allocator) {
        delete allocator;
        allocator = NULL;
    }

    ret = closeGDC();
    if (ret < 0)
        PLUGIN_LOGE("closeGDC is failed(%d)", ret);

    return NO_ERROR;
}

status_t LowLightShot::setup(Map_t *map)
{
    PLUGIN_LOGD("");

    status_t ret = NO_ERROR;

    m_maxBcropX     = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_X];
    m_maxBcropY     = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_Y];
    m_maxBcropW     = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_W];
    m_maxBcropH     = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_H];
    m_maxBcropFullW = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_FULLW];
    m_maxBcropFullH = (Data_int32_t)(*map)[PLUGIN_MAX_BCROP_FULLH];
    m_bayerV4L2Format = (Data_int32_t)(*map)[PLUGIN_BAYER_V4L2_FORMAT];

    PLUGIN_LOGD("setup([%d, %d, %d, %d, %dx%d, %c%c%c%c]",
            m_maxBcropX, m_maxBcropY,
            m_maxBcropW, m_maxBcropH,
            m_maxBcropFullW, m_maxBcropFullH,
            (char)((m_bayerV4L2Format >> 0) & 0xFF),
            (char)((m_bayerV4L2Format >> 8) & 0xFF),
            (char)((m_bayerV4L2Format >> 16) & 0xFF),
            (char)((m_bayerV4L2Format >> 24) & 0xFF));

    /* alloc the buffer */
    m_gdcPrepareThread->run();

    return ret;
}

status_t LowLightShot::execute(Map_t *map)
{
    PLUGIN_LOGD("");

    status_t ret = NO_ERROR;
    nsecs_t start = systemTime();

    bool printLog     = false;
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
    Data_int32_t      mapSrcBufCnt      = (Data_int32_t     )(*map)[PLUGIN_SRC_BUF_CNT];
    Array_buf_t       mapSrcBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_PLANE_CNT];
    Array_buf_plane_t mapSrcBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_SRC_BUF_SIZE];
    Array_buf_rect_t  mapSrcRect        = (Array_buf_rect_t )(*map)[PLUGIN_SRC_BUF_RECT];
    Array_buf_t       mapSrcV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_SRC_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapSrcBufAddr     = (Array_buf_addr_t )(*map)[PLUGIN_SRC_BUF_1];   //source;
    Data_int32_t      mapSrcFramecount  = (Data_int32_t     )(*map)[PLUGIN_SRC_FRAMECOUNT];

    Data_int32_t      mapDstBufCnt      = (Data_int32_t     )(*map)[PLUGIN_DST_BUF_CNT];
    Array_buf_t       mapDstBufPlaneCnt = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_PLANE_CNT];
    Array_buf_plane_t mapDstBufSize     = (Array_buf_plane_t)(*map)[PLUGIN_DST_BUF_SIZE];
    Array_buf_rect_t  mapDstRect        = (Array_buf_rect_t )(*map)[PLUGIN_DST_BUF_RECT];
    Array_buf_t       mapDstV4L2Format  = (Array_buf_t      )(*map)[PLUGIN_DST_BUF_V4L2_FORMAT];
    Array_buf_addr_t  mapDstBufAddr     = (Array_buf_addr_t )(*map)[PLUGIN_DST_BUF_1];   //target;

    Data_int32_t      mapLLSIntent        = (Data_int32_t     )(*map)[PLUGIN_LLS_INTENT       ];
    Single_buf_t      mapMeDownscaledBuf  = (Single_buf_t)(*map)[PLUGIN_ME_DOWNSCALED_BUF];
    Single_buf_t      mapMotionVectorBuf  = (Single_buf_t)(*map)[PLUGIN_MOTION_VECTOR_BUF];
    Single_buf_t      mapCurrentPatchBuf  = (Single_buf_t)(*map)[PLUGIN_CURRENT_PATCH_BUF];
    Data_int32_t      mapMeFramecount     = (Data_int32_t     )(*map)[PLUGIN_ME_FRAMECOUNT    ];
    Data_int64_t      mapTimestamp        = (Data_int64_t     )(*map)[PLUGIN_TIMESTAMP        ];
    Data_int64_t      mapExposureTime     = (Data_int64_t     )(*map)[PLUGIN_EXPOSURE_TIME    ];
    Data_int32_t      mapISO              = (Data_int32_t     )(*map)[PLUGIN_ISO              ];

    if (mapLLSIntent == CAPTURE_START || mapLLSIntent == CAPTURE_PROCESS)
        printLog = true;

    if (printLog) {
        if (mapSrcBufCnt > 0)
            PLUGIN_LOGD("[%d] src[%d]::(adr:%p, P%d, S%d, [%d, %d, %d, %d, %d, %d] format :%d",
                    m_processCount,
                    mapSrcBufCnt,
                    mapSrcBufAddr[0],
                    mapSrcBufPlaneCnt[0],
                    mapSrcBufSize[0][0],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_X],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_Y],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_W],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_H],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_FULL_W],
                    mapSrcRect[0][PLUGIN_ARRAY_RECT_FULL_H],
                    mapSrcV4L2Format[0]);

        if (mapDstBufCnt > 0)
            PLUGIN_LOGD("[%d] dst[%d]::(adr:%p, P%d, S%d, [%d, %d, %d, %d, %d, %d] format :%d",
                    m_processCount,
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

        PLUGIN_LOGD("[%d][I%d][F%d] {me[Vector](%p, %p) me[QVGA](%p)"
                " me fcount:%d, timestamp:%lld, expTime:%lld, iso:%d}",
                m_processCount,
                mapLLSIntent,
                mapSrcFramecount,
                mapMotionVectorBuf,
                mapCurrentPatchBuf,
                mapMeDownscaledBuf,
                mapMeFramecount,
                mapTimestamp,
                mapExposureTime,
                mapISO);
    }

#if 0
    PlugInBuffer_t meBuffer;
    meBuffer.size = 320 * 240 * 12 / 8;
    meBuffer.addr = mapMeDownscaledBuf;
    int *meMeta1 = (int *)mapMotionVectorBuf;
    int *meMeta2 = (int *)mapCurrentPatchBuf;

    /* File Dump */
    dumpToFile("meDownScaled", &meBuffer, 1, mapMeFramecount);
    for (int i = 0; i < 200; i+=10) {
        PLUGIN_LOGD("[META1][F%d][%X, %X, %X, %X, %X   %X, %X %X, %X, %X]",
                mapSrcFramecount,
                meMeta1[i], meMeta1[i+1], meMeta1[i+2], meMeta1[i+3], meMeta1[i+4],
                meMeta1[i+5], meMeta1[i+6], meMeta1[i+7], meMeta1[i+8], meMeta1[i+9]);
    }

    for (int i = 0; i < 200; i+=10) {
        PLUGIN_LOGD("[META2][F%d][%X, %X, %X, %X, %X   %X, %X %X, %X, %X]",
                mapSrcFramecount,
                meMeta2[i], meMeta2[i+1], meMeta2[i+2], meMeta2[i+3], meMeta2[i+4],
                meMeta2[i+5], meMeta2[i+6], meMeta2[i+7], meMeta2[i+8], meMeta2[i+9]);
    }
#endif

    if (mapLLSIntent == CAPTURE_START)
        m_processCount = 1;
    else if (mapLLSIntent == CAPTURE_PROCESS)
        m_processCount++;

    /* join the Thread */
    if (m_gdcPrepareThread->isRunning() == true) {
        PLUGIN_LOGD("Memory Thread Join Start");
        m_gdcPrepareThread->join();
        PLUGIN_LOGD("Memory Thread Join End");
    }

    switch (mapLLSIntent) {
    /* capture case */
    case CAPTURE_START:
    case CAPTURE_PROCESS:
        /* copy to gdcSrcBuf from bayer source */
        memcpy(m_gdcSrcBuf[0].addr, mapSrcBufAddr[0], m_gdcSrcBuf[0].size);
        memcpy(m_gdcSrcBuf[1].addr, mapSrcBufAddr[0], m_gdcSrcBuf[1].size);
        ret = runGDC(m_gdcSrcBuf, m_gdcDstBuf);
        if (ret < 0)
            PLUGIN_LOGE("runGDC failed!! ret(%d)", ret);

        /* last frame */
        if (m_processCount == LLS_MAX_COUNT) {
            int copySize = mapDstBufSize[0][0] / 2;
            PLUGIN_LOGD("[%d][I%d][F%d] memcpy start", m_processCount, mapLLSIntent, mapSrcFramecount);
            memcpy(mapDstBufAddr[0], mapSrcBufAddr[0], copySize);
            memcpy(mapDstBufAddr[0] + copySize, m_gdcDstBuf[0].addr, copySize);
        }
        break;
    default:
        break;
    }

    PLUGIN_LOGD("END");

    return ret;
}

status_t LowLightShot::init(Map_t *map)
{
    return NO_ERROR;
}

status_t LowLightShot::query(Map_t *map)
{
    if (map != NULL) {
        (*map)[PLUGIN_VERSION]                = (Map_data_t)MAKE_VERSION(1, 0);
        (*map)[PLUGIN_LIB_NAME]               = (Map_data_t) "LowLightShotLib";
        (*map)[PLUGIN_BUILD_DATE]             = (Map_data_t)__DATE__;
        (*map)[PLUGIN_BUILD_TIME]             = (Map_data_t)__TIME__;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)1;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)LLS_MAX_COUNT;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)1;
    }

    return NO_ERROR;
}

status_t LowLightShot::setupGDC()
{
    int i = 0;
    status_t ret = NO_ERROR;
    enum v4l2_buf_type deviceBufType[LLS_GDC_VIDEO_TYPE_MAX] = {
        V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    };

    /* 1. open */
    m_gdcFd = exynos_v4l2_open(LLS_GDC_VIDEO_NAME, O_RDWR, 0);
    if (m_gdcFd < 0) {
        PLUGIN_LOGE("exynos_v4l2_open(%s) fail, ret(%d)",
                LLS_GDC_VIDEO_NAME, m_gdcFd);
        return INVALID_OPERATION;
    }
    PLUGIN_LOGD(" Node(%s) opened. m_gdcFd(%d)", LLS_GDC_VIDEO_NAME, m_gdcFd);


    /* 2-1. setFmt(OUTPUT)-src */
    /* 2-2. setFmt(CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        /* 10bit YUV420 */
        m_v4l2Format.type = deviceBufType[i];
        m_v4l2Format.fmt.pix_mp.width = m_maxBcropW;
        m_v4l2Format.fmt.pix_mp.height = m_maxBcropH;
        m_v4l2Format.fmt.pix_mp.colorspace = V4L2_COLORSPACE_DEFAULT;
        m_v4l2Format.fmt.pix_mp.pixelformat = LLS_GDC_VIDEO_FORMAT;
        m_v4l2Format.fmt.pix_mp.num_planes  = 2; /* YUV420 2 plane */
        m_v4l2Format.fmt.pix_mp.quantization = V4L2_QUANTIZATION_FULL_RANGE;

        PLUGIN_LOGD("type %d, %d x %d, format %d(%c %c %c %c), piexeSizeIndex %d, yuvRange %d",
                m_v4l2ReqBufs.type,
                m_v4l2Format.fmt.pix_mp.width,
                m_v4l2Format.fmt.pix_mp.height,
                m_v4l2Format.fmt.pix_mp.pixelformat,
                (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 0) & 0xFF),
                (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 8) & 0xFF),
                (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 16) & 0xFF),
                (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 24) & 0xFF),
                m_v4l2Format.fmt.pix_mp.flags,
                m_v4l2Format.fmt.pix_mp.colorspace);

        ret = exynos_v4l2_s_fmt(m_gdcFd, &m_v4l2Format);
        if (ret < 0) {
            PLUGIN_LOGE("exynos_v4l2_s_fmt(fd:%d) fail (%d)",
                    m_gdcFd, ret);
            return ret;
        }
    }

    /* 3-1. reqbuf(OUTPUT)-src */
    /* 3-2. reqbuf(CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        m_v4l2ReqBufs.type = deviceBufType[i];
        m_v4l2ReqBufs.count = VIDEO_MAX_FRAME;
        m_v4l2ReqBufs.memory = V4L2_MEMORY_DMABUF;

        ret = exynos_v4l2_reqbufs(m_gdcFd, &m_v4l2ReqBufs);
        if (ret < 0) {
            PLUGIN_LOGE("exynos_v4l2_reqbufs(fd:%d, count:%d) fail (%d)",
                 m_gdcFd, m_v4l2ReqBufs.count, ret);
            return ret;
        }

        PLUGIN_LOGD("fd %d, count %d, type %d, memory %d",
                m_gdcFd,
                m_v4l2ReqBufs.count,
                m_v4l2ReqBufs.type,
                m_v4l2ReqBufs.memory);
    }

    return NO_ERROR;
}

/* For GDC Test */
int g_test_grid_x[7][9] =
{
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168}
};

int g_test_grid_y[7][9] =
{
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168},
    {-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168,-103168}
};

status_t LowLightShot::runGDC(PlugInBuffer_t *srcBuf, PlugInBuffer_t *dstBuf)
{
    int i = 0;
    status_t ret = NO_ERROR;
    struct v4l2_ext_controls extCtrls;
    struct v4l2_ext_control extCtrl;
    struct gdc_crop_param gdcCropParam;
    struct v4l2_buffer v4l2Buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    enum v4l2_buf_type deviceBufType[LLS_GDC_VIDEO_TYPE_MAX] = {
        V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    };

    if (m_gdcFd < 0) {
        PLUGIN_LOGE("invalid m_gdcFd(%d)", m_gdcFd);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    PLUGIN_LOGD("");

    memset(&extCtrl, 0x00, sizeof(extCtrl));
    memset(&extCtrls, 0x00, sizeof(extCtrls));
    memset(&gdcCropParam, 0x00, sizeof(gdcCropParam));
    memset(&v4l2Buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

    /* 4. crop and grid setting src */
    extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
    extCtrls.count = 1;
    extCtrls.controls = &extCtrl;
    extCtrl.id = V4L2_CID_CAMERAPP_GDC_GRID_CONTROL;
    extCtrl.ptr = &gdcCropParam;
    gdcCropParam.use_calculated_grid = true;

    /* TODO: fill the grid table */
    /* For Test */
    memcpy(&gdcCropParam.calculated_grid_x, &g_test_grid_x, sizeof(int) * 7 * 9);
    memcpy(&gdcCropParam.calculated_grid_y, &g_test_grid_y, sizeof(int) * 7 * 9);

    ret = exynos_v4l2_s_ext_ctrl(m_gdcFd, &extCtrls);
    if (ret != NO_ERROR) {
        PLUGIN_LOGE("node->setExtControl() fail");
        return INVALID_OPERATION;
    }

    /* 5. stream on (OUTPUT)-src */
    /* 5. stream on (CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        ret = exynos_v4l2_streamon(m_gdcFd, deviceBufType[i]);
        if (ret < 0) {
            PLUGIN_LOGE("exynos_v4l2_streamon(fd:%d, type:%d) fail (%d)",
                 m_gdcFd, (int)m_v4l2ReqBufs.type, ret);
            return ret;
        }
    }

#if 0
    /* File Dump */
    dumpToFile("gdcSrc", srcBuf, 2, m_processCount);
#endif

    v4l2Buf.m.planes = planes; /* YUV420 2plane */
    v4l2Buf.memory   = m_v4l2ReqBufs.memory;
    v4l2Buf.length   = m_v4l2Format.fmt.pix_mp.num_planes;
    //v4l2Buf.flags = V4L2_BUF_FLAG_USE_SYNC;
    //v4l2_buf.flags |= ((V4L2_BUF_FLAG_NO_CACHE_CLEAN) | (V4L2_BUF_FLAG_NO_CACHE_INVALIDATE));
    //v4l2_buf.flags |= (V4L2_BUF_FLAG_NO_CACHE_CLEAN);

    /* 6-1. qbuf on (OUTPUT)-src */
    v4l2Buf.type = deviceBufType[0];
    v4l2Buf.index = 0;
    for (i = 0; i < (int)v4l2Buf.length; i++) {
        v4l2Buf.m.planes[i].m.fd = (int)(srcBuf[i].fd);
        v4l2Buf.m.planes[i].length = (unsigned long)(srcBuf[i].size);
    }

    ret = exynos_v4l2_qbuf(m_gdcFd, &v4l2Buf);
    if (ret < 0) {
        PLUGIN_LOGE("exynos_v4l2_qbuf(m_gdcFd:%d, buf->index:%d) fail (%d)",
                m_gdcFd, v4l2Buf.index, ret);
        return ret;
    }

    /* 6-2. qbuf on (CAPTURE)-dst */
    v4l2Buf.type = deviceBufType[1];
    v4l2Buf.index = 0;
    for (i = 0; i < (int)v4l2Buf.length; i++) {
        v4l2Buf.m.planes[i].m.fd = (int)(dstBuf[i].fd);
        v4l2Buf.m.planes[i].length = (unsigned long)(dstBuf[i].size);
    }

    ret = exynos_v4l2_qbuf(m_gdcFd, &v4l2Buf);
    if (ret < 0) {
        PLUGIN_LOGE("exynos_v4l2_qbuf(m_gdcFd:%d, buf->index:%d) fail (%d)",
                m_gdcFd, v4l2Buf.index, ret);
        return ret;
    }


    /* 7-1. dqbuf on (OUTPUT)-src */
    /* 7-2. dqbuf on (CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        v4l2Buf.type = deviceBufType[i];
        ret = exynos_v4l2_dqbuf(m_gdcFd, &v4l2Buf);
        if (ret < 0) {
            if (ret != -EAGAIN)
                PLUGIN_LOGE("exynos_v4l2_dqbuf(fd:%d) fail (%d)", m_gdcFd, ret);

            return ret;
        }
    }

#if 0
    /* File Dump */
    dumpToFile("gdcDst", dstBuf, 2, m_processCount);
#endif

    /* 8. stream off (OUTPUT)-src */
    /* 8. stream off (CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        ret = exynos_v4l2_streamoff(m_gdcFd, deviceBufType[i]);
        if (ret < 0) {
            PLUGIN_LOGE("exynos_v4l2_streamoff(fd:%d, type:%d) fail (%d)",
                 m_gdcFd, (int)m_v4l2ReqBufs.type, ret);
            return ret;
        }
    }

func_exit:

    return ret;
}

status_t LowLightShot::closeGDC()
{
    int i = 0;
    status_t ret = NO_ERROR;
    enum v4l2_buf_type deviceBufType[LLS_GDC_VIDEO_TYPE_MAX] = {
        V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
    };

    if (m_gdcFd < 0)
        goto func_exit;

    /* 9-1. reqbuf(OUTPUT)-src */
    /* 9-2. reqbuf(CAPTURE)-dst */
    for (i = 0; i < LLS_GDC_VIDEO_TYPE_MAX; i++) {
        m_v4l2ReqBufs.type = deviceBufType[i];
        m_v4l2ReqBufs.count  = 0;
        m_v4l2ReqBufs.memory = V4L2_MEMORY_DMABUF;

        ret = exynos_v4l2_reqbufs(m_gdcFd, &m_v4l2ReqBufs);
        if (ret < 0) {
            PLUGIN_LOGE("exynos_v4l2_reqbufs(fd:%d, count:%d) fail (%d)",
                 m_gdcFd, m_v4l2ReqBufs.count, ret);
        }

        PLUGIN_LOGD("fd %d, count %d, type %d, memory %d",
                m_gdcFd,
                m_v4l2ReqBufs.count,
                m_v4l2ReqBufs.type,
                m_v4l2ReqBufs.memory);
    }

    /* 10. close */
    ret = exynos_v4l2_close(m_gdcFd);
    if (ret < 0) {
        PLUGIN_LOGE("exynos_v4l2_close(%s) fail, ret(%d)",
                m_gdcFd, ret);
    }
    PLUGIN_LOGD("Node(%s) closed. m_gdcFd(%d)", LLS_GDC_VIDEO_NAME, m_gdcFd);
    m_gdcFd = -1;

func_exit:

    return NO_ERROR;
}


bool LowLightShot::m_gdcPrepareThreadFunc(void)
{
    int size;
    status_t ret = NO_ERROR;
    int fullBayerSize = m_maxBcropW * m_maxBcropH;

    /* mergeBuf alloc */
    switch (m_bayerV4L2Format) {
    case V4L2_PIX_FMT_SBGGR12:  /* packed 12bit */
        m_mergeBuf.size = ALIGN_UP(m_maxBcropW * 3 / 2, 16) * m_maxBcropH; /* HACK: 16bit Bayer */
        break;
    case V4L2_PIX_FMT_SBGGR10:  /* packed 10bit */
        m_mergeBuf.size = ALIGN_UP(m_maxBcropW * 5 / 4, 16) * m_maxBcropH; /* HACK: 16bit Bayer */
        break;
    case V4L2_PIX_FMT_SBGGR16:  /* unpacked (10bit+000000) */
    default:
        m_mergeBuf.size = fullBayerSize * 2; /* HACK: 16bit Bayer */
    }

    ret = allocator->alloc(m_mergeBuf.size, &m_mergeBuf.fd, &m_mergeBuf.addr, true);
    if (ret < 0) {
        PLUGIN_LOGE("size(%d) ret(%d)", m_mergeBuf.size, ret);
        goto alloc_fail1;
    }

    /* gdcSrcBuf alloc */
    switch (LLS_GDC_VIDEO_FORMAT) {
    case V4L2_PIX_FMT_NV12M_S10B:  /* 8+2 */
        /* Y plane */
        m_gdcSrcBuf[0].size = NV12M_Y_SIZE(m_maxBcropW, m_maxBcropH) +
                              NV12M_Y_2B_SIZE(m_maxBcropW, m_maxBcropH);
        m_gdcDstBuf[0].size = m_gdcSrcBuf[0].size;

        /* CbCr Plane */
        m_gdcSrcBuf[1].size = NV12M_CBCR_SIZE(m_maxBcropW, m_maxBcropH) +
                              NV12M_CBCR_2B_SIZE(m_maxBcropW, m_maxBcropH);
        m_gdcDstBuf[1].size = m_gdcSrcBuf[1].size;
        break;
    default:
        m_gdcSrcBuf[0].size = m_maxBcropW * m_maxBcropH * 2;
        m_gdcSrcBuf[1].size = m_maxBcropW * m_maxBcropH * 2;
        m_gdcDstBuf[0].size = m_maxBcropW * m_maxBcropH * 2;
        m_gdcDstBuf[1].size = m_maxBcropW * m_maxBcropH * 2;
        break;
    }

    for (int i = 0; i < 2; i++) {
        size = (i == 0) ? fullBayerSize * 2: fullBayerSize * 2;
        ret = allocator->alloc(m_gdcSrcBuf[i].size, &m_gdcSrcBuf[i].fd, &m_gdcSrcBuf[i].addr, true);
        if (ret < 0) {
            PLUGIN_LOGE("size(%d) ret(%d)", m_gdcSrcBuf[i].size, ret);
            goto alloc_fail2;
        }
    }

    /* gdcDstBuf alloc */
    for (int i = 0; i < 2; i++) {
        size = (i == 0) ? fullBayerSize * 2: fullBayerSize * 2;
        ret = allocator->alloc(m_gdcDstBuf[i].size, &m_gdcDstBuf[i].fd, &m_gdcDstBuf[i].addr, true);
        if (ret < 0) {
            PLUGIN_LOGE("size(%d) ret(%d)", m_gdcDstBuf[i].size, ret);
            goto alloc_fail2;
        }
    }

    for (int i = 0; i < 2; i++) {
        PLUGIN_LOGD("gdcSrcBuf[%d] fd(%d), size(%d), addr(%p)",
                i, m_gdcSrcBuf[i].fd, m_gdcSrcBuf[i].size, m_gdcSrcBuf[i].addr);
        PLUGIN_LOGD("gdcDstBuf[%d] fd(%d), size(%d), addr(%p)",
                i, m_gdcDstBuf[i].fd, m_gdcDstBuf[i].size, m_gdcDstBuf[i].addr);
    }

    PLUGIN_LOGD("mergeBuf fd(%d), size(%d), addr(%p)",
            m_mergeBuf.fd, m_mergeBuf.size, m_mergeBuf.addr);

    ret = setupGDC();
    if (ret < 0) {
        PLUGIN_LOGE("setupGDC is failed(%d)", ret);
        goto alloc_fail3;
    }

    return false;

alloc_fail3:
    for (int i = 0; i < 2; i++) {
        if (m_gdcDstBuf[i].fd >= 0) {
            allocator->free(&m_gdcDstBuf[i].fd, m_gdcDstBuf[i].size, &m_gdcDstBuf[i].addr, true);
            if (ret < 0)
                PLUGIN_LOGE("free fail(%d) ret(%d)", m_gdcDstBuf[i].size, ret);
        }
    }
alloc_fail2:
    for (int i = 0; i < 2; i++) {
        if (m_gdcSrcBuf[i].fd >= 0) {
            allocator->free(&m_gdcSrcBuf[i].fd, m_gdcSrcBuf[i].size, &m_gdcSrcBuf[i].addr, true);
            if (ret < 0)
                PLUGIN_LOGE("free fail(%d) ret(%d)", m_gdcSrcBuf[i].size, ret);
        }
    }
alloc_fail1:
    if (m_mergeBuf.fd >= 0) {
        allocator->free(&m_mergeBuf.fd, m_mergeBuf.size, &m_mergeBuf.addr, true);
        if (ret < 0)
            PLUGIN_LOGE("free fail(%d) ret(%d)", m_mergeBuf.size, ret);
    }

    return false;
}

