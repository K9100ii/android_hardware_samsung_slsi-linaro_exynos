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
#define LOG_TAG "ExynosCameraPlugInConverter"

#include "ExynosCameraPlugInConverter.h"

namespace android {

ExynosCameraPlugInConverter::~ExynosCameraPlugInConverter()
{
    /* do nothing */
    deinit();
}

/*********************************************/
/*  public functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverter::init(void)
{
    status_t ret = NO_ERROR;

    ALOGI("%s(%d)", __FUNCTION__, __LINE__);

    ret = m_init();
    if (ret != NO_ERROR) {
        CLOGE("m_init(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    CLOGD("done");

    return ret;
}

status_t ExynosCameraPlugInConverter::deinit(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    CLOGI("");

    ret = ExynosCameraPlugInConverter::m_deinit();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugInConverter::m_deinit(%d) fail", m_pipeId);
    }

    ret = this->m_deinit();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("this->m_deinit(%d) fail", m_pipeId);
    }

    CLOGD("done");

    return funcRet;
}

status_t ExynosCameraPlugInConverter::create(Map_t *map)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId(%d)", m_pipeId);

    ret = ExynosCameraPlugInConverter::m_create(map);
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugInConverter::m_create(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    ret = this->m_create(map);
    if (ret != NO_ERROR) {
        CLOGE("this->m_map(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    CLOGD("done, pipeId(%d)", m_pipeId);

    return ret;
}

status_t ExynosCameraPlugInConverter::setup(Map_t *map)
{
    status_t ret = NO_ERROR;

    CLOGI("pipeId(%d)", m_pipeId);

    ret = ExynosCameraPlugInConverter::m_setup(map);
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugInConverter::m_setup(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    ret = this->m_setup(map);
    if (ret != NO_ERROR) {
        CLOGE("this->m_map(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    CLOGD("done, pipeId(%d)", m_pipeId);

    return ret;
}

status_t ExynosCameraPlugInConverter::make(Map_t *map)
{
    status_t ret = NO_ERROR;

    CLOGV("pipeId(%d)", m_pipeId);

    ret = ExynosCameraPlugInConverter::m_make(map);
    if (ret != NO_ERROR) {
        CLOGE("ExynosCameraPlugInConverter::m_make(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    ret = this->m_make(map);
    if (ret != NO_ERROR) {
        CLOGE("this->m_make(%d) fail", m_pipeId);
        return INVALID_OPERATION;
    }

    CLOGV("done, pipeId(%d)", m_pipeId);

    return ret;
}

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverter::m_init(void)
{
    m_version      = 0;
    m_libName      = NULL;
    m_buildDate    = NULL;
    m_buildTime    = NULL;
    m_curSrcBufCnt = 1;
    m_curDstBufCnt = 1;
    m_maxSrcBufCnt = 1;
    m_maxDstBufCnt = 1;
    strncpy(m_name, "", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverter::m_deinit(void)
{
    return NO_ERROR;
}

status_t ExynosCameraPlugInConverter::m_create(Map_t *map)
{
    if (map != NULL) {
        m_version      = (Data_uint32_t)(*map)[PLUGIN_VERSION];
        m_libName      = (Pointer_const_char_t)(*map)[PLUGIN_LIB_NAME];
        m_buildDate    = (Pointer_const_char_t)(*map)[PLUGIN_BUILD_DATE];
        m_buildTime    = (Pointer_const_char_t)(*map)[PLUGIN_BUILD_TIME];
        m_curSrcBufCnt = (Data_int32_t)(*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT];
        m_curDstBufCnt = (Data_int32_t)(*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT];
        m_maxSrcBufCnt = (Data_int32_t)(*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT];
        m_maxDstBufCnt = (Data_int32_t)(*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT];
    }

    CLOGD("pipeId(%d), bufCount(cur[%d, %d], max[%d, %d])",
            m_pipeId, m_curSrcBufCnt, m_curDstBufCnt, m_maxSrcBufCnt, m_maxDstBufCnt);

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverter::m_setup(Map_t *map)
{
    status_t ret = NO_ERROR;
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];

    switch (type) {
    case PLUGIN_CONVERT_SETUP_BEFORE:
        break;
    case PLUGIN_CONVERT_SETUP_AFTER:
        /* default information update */
        (*map)[PLUGIN_VERSION]                = (Map_data_t)m_version;
        (*map)[PLUGIN_LIB_NAME]               = (Map_data_t)m_libName;
        (*map)[PLUGIN_BUILD_DATE]             = (Map_data_t)m_buildDate;
        (*map)[PLUGIN_BUILD_TIME]             = (Map_data_t)m_buildTime;
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Map_data_t)m_curSrcBufCnt;
        (*map)[PLUGIN_PLUGIN_CUR_DST_BUF_CNT] = (Map_data_t)m_curDstBufCnt;
        (*map)[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT] = (Map_data_t)m_maxSrcBufCnt;
        (*map)[PLUGIN_PLUGIN_MAX_DST_BUF_CNT] = (Map_data_t)m_maxDstBufCnt;
        break;
    default:
        CLOGE("invalid convert type(%d)!! pipeId(%d)", type, m_pipeId);
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCameraPlugInConverter::m_make(Map_t *map)
{
    status_t ret = NO_ERROR;
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraFrameSP_sptr_t frame = NULL;

    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];
    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];

    switch (type) {
    case PLUGIN_CONVERT_PROCESS_BEFORE:
        /* default information update */
        ret = m_convertBufferInfoFromFrame(frame, map);
        if (ret != NO_ERROR) {
            CLOGE("convertBufferInfoFromFrame(%d, %d, %d) fail",
                    m_pipeId, type, (frame != NULL) ? frame->getFrameCount() : -1);
            goto func_exit;
        }
        break;
    case PLUGIN_CONVERT_PROCESS_AFTER:
        break;
    default:
        CLOGE("invalid convert type(%d)!! pipeId(%d)", type, m_pipeId);
        goto func_exit;
    }

func_exit:

    return ret;
}

status_t ExynosCameraPlugInConverter::m_convertBufferInfoFromFrame(ExynosCameraFrameSP_sptr_t frame, Map_t *map)
{
    int i, j;
    status_t ret = NO_ERROR;

    if (frame == NULL) {
        CLOGE("frame is NULL !! pipeId(%d)", m_pipeId);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* init all default converting variable */
    memset(&m_srcBufPlaneCnt,   0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufIndex,      0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufSize,       0x0, sizeof(int) * PLUGIN_MAX_BUF * PLUGIN_MAX_PLANE);
    memset(&m_srcBufFd,         0x0, sizeof(int) * PLUGIN_MAX_BUF * PLUGIN_MAX_PLANE);
    memset(&m_srcBufRect,       0x0, sizeof(int) * PLUGIN_MAX_BUF * 6);
    memset(&m_srcBufWStride,    0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufHStride,    0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufV4L2Format, 0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufHALFormat,  0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufRotation,   0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufFlipH,      0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_srcBufFlipV,      0x0, sizeof(int) * PLUGIN_MAX_BUF);

    memset(&m_dstBufPlaneCnt,   0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufIndex,      0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufSize,       0x0, sizeof(int) * PLUGIN_MAX_BUF * PLUGIN_MAX_PLANE);
    memset(&m_dstBufFd,         0x0, sizeof(int) * PLUGIN_MAX_BUF * PLUGIN_MAX_PLANE);
    memset(&m_dstBufRect,       0x0, sizeof(int) * PLUGIN_MAX_BUF * 6);
    memset(&m_dstBufWStride,    0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufHStride,    0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufV4L2Format, 0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufHALFormat,  0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufRotation,   0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufFlipH,      0x0, sizeof(int) * PLUGIN_MAX_BUF);
    memset(&m_dstBufFlipV,      0x0, sizeof(int) * PLUGIN_MAX_BUF);

    /* 1. src buffer setting from frame */
    m_curSrcBufCnt = (Data_int32_t)(*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT];

    for (i = 0; i < m_curSrcBufCnt; i++) {
        ExynosRect rect;
        ExynosCameraBuffer buffer;

        /* get buffer */
        ret = frame->getSrcBuffer(m_pipeId, &buffer, i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getSrcBuffer(%d) fail, ret(%d)",
                    i, frame->getFrameCount(), m_pipeId, ret);
            goto func_exit;
        }

        /* get rect */
        ret = frame->getSrcRect(m_pipeId, &rect, i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getSrcRect(%d) fail, ret(%d)",
                    i, frame->getFrameCount(), m_pipeId, ret);
            goto func_exit;
        }

        m_srcBufPlaneCnt[i] = buffer.planeCount;
        m_srcBufIndex[i] = buffer.index;
        for (j = 0; j < buffer.planeCount; j++) {
            m_srcBuf[i][j] = buffer.addr[j];
            m_srcBufSize[i][j] = buffer.size[j];
            m_srcBufFd[i][j] = buffer.fd[j];
        }
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_X] = rect.x;
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_Y] = rect.y;
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_W] = rect.w;
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_H] = rect.h;
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_FULL_W] = rect.fullW;
        m_srcBufRect[i][PLUGIN_ARRAY_RECT_FULL_H] = rect.fullH;
        m_srcBufWStride[i] = buffer.bytesPerLine[0];

        if (m_srcBufWStride[i] == 0) {
            m_srcBufWStride[i] = ROUND_UP(m_srcBufRect[i][PLUGIN_ARRAY_RECT_FULL_W], CAMERA_16PX_ALIGN);
            CLOGV("m_srcBufWStride[%d] is 0. so just set it as %d", i, m_srcBufWStride[i]);
        }

        m_srcBufHStride[i] = 0;     //TODO: how can I get this value ?
        m_srcBufV4L2Format[i] = rect.colorFormat;
        m_srcBufHALFormat[i] = V4L2_PIX_2_HAL_PIXEL_FORMAT(rect.colorFormat);
        m_srcBufRotation[i] = 0;    //TODO: how can I get this value ?
        m_srcBufFlipH[i] = 0;       //TODO: how can I get this value ?
        m_srcBufFlipV[i] = 0;       //TODO: how can I get this value ?
#if 0
        /* TODO: get the rotation and flip information */
        rotation = newFrame->getRotation(getPipeId());
        flipHorizontal = newFrame->getFlipHorizontal(getPipeId());
        flipVertical = newFrame->getFlipVertical(getPipeId());
#endif
        (*map)[PLUGIN_SRC_BUF_1 + i]   = (Map_data_t)(&m_srcBuf[i][0]);

        CLOGV("[%d] source frame(%d) buf[%d, %d, %p, %d byte fd(%d) [%d, %d, %d, %d, %d, %d]",
                i,
                frame->getFrameCount(),
                m_srcBufPlaneCnt[i],
                m_srcBufIndex[i],
                m_srcBuf[i][0],
                m_srcBufSize[i][0],
                m_srcBufFd[i][0],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_X],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_Y],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_W],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_H],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_FULL_W],
                m_srcBufRect[i][PLUGIN_ARRAY_RECT_FULL_H]);
    }

    (*map)[PLUGIN_SRC_BUF_CNT]         = (Map_data_t)m_curSrcBufCnt;
    (*map)[PLUGIN_SRC_BUF_PLANE_CNT]   = (Map_data_t)m_srcBufPlaneCnt;
    (*map)[PLUGIN_SRC_BUF_INDEX]       = (Map_data_t)m_srcBufIndex;
    (*map)[PLUGIN_SRC_BUF_SIZE]        = (Map_data_t)m_srcBufSize;
    (*map)[PLUGIN_SRC_BUF_FD]          = (Map_data_t)m_srcBufFd;
    (*map)[PLUGIN_SRC_BUF_RECT]        = (Map_data_t)m_srcBufRect;
    (*map)[PLUGIN_SRC_BUF_WSTRIDE]     = (Map_data_t)m_srcBufWStride;
    (*map)[PLUGIN_SRC_BUF_HSTRIDE]     = (Map_data_t)m_srcBufHStride;
    (*map)[PLUGIN_SRC_BUF_V4L2_FORMAT] = (Map_data_t)m_srcBufV4L2Format;
    (*map)[PLUGIN_SRC_BUF_HAL_FORMAT]  = (Map_data_t)m_srcBufHALFormat;
    (*map)[PLUGIN_SRC_BUF_ROTATION]    = (Map_data_t)m_srcBufRotation;
    (*map)[PLUGIN_SRC_BUF_FLIPH]       = (Map_data_t)m_srcBufFlipH;
    (*map)[PLUGIN_SRC_BUF_FLIPV]       = (Map_data_t)m_srcBufFlipV;
    (*map)[PLUGIN_SRC_FRAMECOUNT]      = (Map_data_t)frame->getMetaFrameCount();

    /* 2. dst buffer setting from frame */
    for (i = 0; i < m_curDstBufCnt; i++) {
        ExynosRect rect;
        ExynosCameraBuffer buffer;

        /* get buffer */
        ret = frame->getDstBuffer(m_pipeId, &buffer, i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getDstBuffer(%d) fail, ret(%d)",
                    i, frame->getFrameCount(), m_pipeId, ret);
            goto func_exit;
        }

        /* get rect */
        ret = frame->getDstRect(m_pipeId, &rect, i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getDstRect(%d) fail, ret(%d)",
                    i, frame->getFrameCount(), m_pipeId, ret);
            goto func_exit;
        }

        m_dstBufPlaneCnt[i] = buffer.planeCount;
        m_dstBufIndex[i] = buffer.index;
        for (j = 0; j < buffer.planeCount; j++) {
            m_dstBuf[i][j] = buffer.addr[j];
            m_dstBufSize[i][j] = buffer.size[j];
            m_dstBufFd[i][j] = buffer.fd[j];
        }

        m_dstBufRect[i][PLUGIN_ARRAY_RECT_X] = rect.x;
        m_dstBufRect[i][PLUGIN_ARRAY_RECT_Y] = rect.y;
        m_dstBufRect[i][PLUGIN_ARRAY_RECT_W] = rect.w;
        m_dstBufRect[i][PLUGIN_ARRAY_RECT_H] = rect.h;
        m_dstBufRect[i][PLUGIN_ARRAY_RECT_FULL_W] = rect.fullW;
        m_dstBufRect[i][PLUGIN_ARRAY_RECT_FULL_H] = rect.fullH;
        m_dstBufWStride[i] = buffer.bytesPerLine[0];
        if (m_dstBufWStride[i] == 0) {
            m_dstBufWStride[i] = ROUND_UP(m_dstBufRect[i][PLUGIN_ARRAY_RECT_FULL_W], CAMERA_16PX_ALIGN);
            CLOGV("m_dstBufWStride[%d] is 0. so just set it as %d", i, m_dstBufWStride[i]);
        }

        m_dstBufHStride[i] = 0;     //TODO: how can I get this value ?
        m_dstBufV4L2Format[i] = rect.colorFormat;
        m_dstBufHALFormat[i] = V4L2_PIX_2_HAL_PIXEL_FORMAT(rect.colorFormat);
        m_dstBufRotation[i] = 0;    //TODO: how can I get this value ?
        m_dstBufFlipH[i] = 0;       //TODO: how can I get this value ?
        m_dstBufFlipV[i] = 0;       //TODO: how can I get this value ?
#if 0
        /* TODO: get the rotation and flip information */
        rotation = newFrame->getRotation(getPipeId());
        flipHorizontal = newFrame->getFlipHorizontal(getPipeId());
        flipVertical = newFrame->getFlipVertical(getPipeId());
#endif
        (*map)[PLUGIN_DST_BUF_1 + i]   = (Map_data_t)(&m_dstBuf[i][0]);

        CLOGV("[%d] dst frame(%d) buf[%d, %d, %p, %dbyte fd(%d) [%d, %d, %d, %d, %d, %d]",
                i,
                frame->getFrameCount(),
                m_dstBufPlaneCnt[i],
                m_dstBufIndex[i],
                m_dstBuf[i][0],
                m_dstBufSize[i][0],
                m_dstBufFd[i][0],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_X],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_Y],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_W],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_H],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_FULL_W],
                m_dstBufRect[i][PLUGIN_ARRAY_RECT_FULL_H]);
    }

    (*map)[PLUGIN_DST_BUF_CNT]         = (Map_data_t)m_curDstBufCnt;
    (*map)[PLUGIN_DST_BUF_PLANE_CNT]   = (Map_data_t)m_dstBufPlaneCnt;
    (*map)[PLUGIN_DST_BUF_INDEX]       = (Map_data_t)m_dstBufIndex;
    (*map)[PLUGIN_DST_BUF_SIZE]        = (Map_data_t)m_dstBufSize;
    (*map)[PLUGIN_DST_BUF_FD]          = (Map_data_t)m_dstBufFd;
    (*map)[PLUGIN_DST_BUF_RECT]        = (Map_data_t)m_dstBufRect;
    (*map)[PLUGIN_DST_BUF_WSTRIDE]     = (Map_data_t)m_dstBufWStride;
    (*map)[PLUGIN_DST_BUF_HSTRIDE]     = (Map_data_t)m_dstBufHStride;
    (*map)[PLUGIN_DST_BUF_V4L2_FORMAT] = (Map_data_t)m_dstBufV4L2Format;
    (*map)[PLUGIN_DST_BUF_HAL_FORMAT]  = (Map_data_t)m_dstBufHALFormat;
    (*map)[PLUGIN_DST_BUF_ROTATION]    = (Map_data_t)m_dstBufRotation;
    (*map)[PLUGIN_DST_BUF_FLIPH]       = (Map_data_t)m_dstBufFlipH;
    (*map)[PLUGIN_DST_BUF_FLIPV]       = (Map_data_t)m_dstBufFlipV;

func_exit:

    return ret;
}
}; /* namespace android */
