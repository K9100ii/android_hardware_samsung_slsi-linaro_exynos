/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraNodeCIP"

#include"ExynosCameraNodeCIP.h"
#include"ExynosCameraUtils.h"

namespace android {

/* HACK */
exif_attribute_t exifInfo__;

ExynosCameraNodeCIP::ExynosCameraNodeCIP()
{
    memset(m_name,  0x00, sizeof(m_name));
    memset(m_alias, 0x00, sizeof(m_alias));
    memset(&m_v4l2Format,  0x00, sizeof(struct v4l2_format));
    memset(&m_v4l2ReqBufs, 0x00, sizeof(struct v4l2_requestbuffers));
    //memset(&m_exifInfo, 0x00, sizeof(exif_attribute_t));
    memset(&m_debugInfo, 0x00, sizeof(debug_attribute_t));

    m_fd        = NODE_INIT_NEGATIVE_VALUE;

    m_v4l2Format.fmt.pix_mp.width       = NODE_INIT_NEGATIVE_VALUE;
    m_v4l2Format.fmt.pix_mp.height      = NODE_INIT_NEGATIVE_VALUE;
    m_v4l2Format.fmt.pix_mp.pixelformat = NODE_INIT_NEGATIVE_VALUE;
    m_v4l2Format.fmt.pix_mp.num_planes  = NODE_INIT_NEGATIVE_VALUE;
    m_v4l2Format.fmt.pix_mp.colorspace  = (enum v4l2_colorspace)7; /* V4L2_COLORSPACE_JPEG */
    /*
     * 7 : Full YuvRange, 4 : Limited YuvRange
     * you can refer m_YUV_RANGE_2_V4L2_COLOR_RANGE() and m_V4L2_COLOR_RANGE_2_YUV_RANGE()
     */

    m_v4l2ReqBufs.count  = NODE_INIT_NEGATIVE_VALUE;
    m_v4l2ReqBufs.memory = (v4l2_memory)NODE_INIT_ZERO_VALUE;
    m_v4l2ReqBufs.type   = (v4l2_buf_type)NODE_INIT_ZERO_VALUE;

    m_crop.type = (v4l2_buf_type)NODE_INIT_ZERO_VALUE;
    m_crop.c.top = NODE_INIT_ZERO_VALUE;
    m_crop.c.left = NODE_INIT_ZERO_VALUE;
    m_crop.c.width = NODE_INIT_ZERO_VALUE;
    m_crop.c.height =NODE_INIT_ZERO_VALUE;

    m_flagStart  = false;
    m_flagCreate = false;

    memset(m_flagQ, 0x00, sizeof(m_flagQ));
    m_flagStreamOn = false;
    m_flagDup = false;
    m_paramState = 0;
    m_nodeState = 0;
    m_cameraId = 0;
    m_sensorId = -1;
    m_videoNodeNum = -1;

    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        m_queueBufferList[i].index = NODE_INIT_NEGATIVE_VALUE;
    }

    m_nodeType = NODE_TYPE_BASE;

    m_cipMode       = 0;
    m_cipInterface  = NULL;
    m_nodeLocation  = NODE_LOCATION_DST;
}

ExynosCameraNodeCIP::~ExynosCameraNodeCIP()
{
    EXYNOS_CAMERA_NODE_IN();

    destroy();
}

status_t ExynosCameraNodeCIP::create(
        const char *nodeName,
        int cameraId,
        enum EXYNOS_CAMERA_NODE_LOCATION location,
        void *module)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    if (nodeName == NULL) {
        CLOGE("nodeName is NULL!!");
        return BAD_VALUE;
    }

    if (cameraId >= 0) {
        setCameraId(cameraId);
    }

    if (module == NULL) {
        CLOGD("cipInterface is NULL!!");
    }

    /*
     * Parent's function was hided, because child's function was overrode.
     * So, it should call parent's function explicitly.
     */
    ret = ExynosCameraNode::create(nodeName, nodeName);
    if (ret != NO_ERROR) {
        CLOGE("create fail [%d]", (int)ret);
        return ret;
    }

    m_cipInterface = (cipInterface *)module;

    m_nodeType = NODE_TYPE_BASE;
    m_nodeLocation = location;

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeCIP::open(int videoNodeNum)
{
    if (m_cipInterface == NULL) {
        CLOGE("m_cipInterface is NULL, Invalid function call");
        return INVALID_OPERATION;
    }

    return open(videoNodeNum, -1);
}

status_t ExynosCameraNodeCIP::open(int videoNodeNum, int operationMode)
{
    EXYNOS_CAMERA_NODE_IN();

    CLOGV("open");

    status_t ret = NO_ERROR;
    char node_name[30];

    if (m_nodeState != NODE_STATE_CREATED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    memset(&node_name, 0x00, sizeof(node_name));
    snprintf(node_name, sizeof(node_name), "%s%d", NODE_PREFIX, videoNodeNum);

    if (m_cipInterface == NULL) {
        m_cipInterface = cip_create(operataionMode);
        if (m_cipInterface == NULL) {
            CLOGE("Cannot create cipInterface, mode(%d)", (int)operationMode);
            return INVALID_OPERATION;
        }

        ret = cip_open(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("CIP open fail, ret(%d)", ret);
            return ret;
        }

        CLOGI("CIP created");
    }

    m_cipMode = (cipMode)operationMode;
    CLOGD("Node(%d)(%s) location(%d) opened.",
            videoNodeNum, node_name, m_nodeLocation);

    m_videoNodeNum = videoNodeNum;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_OPENED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeCIP::close(void)
{
    EXYNOS_CAMERA_NODE_IN();

    CLOGD("close");

    status_t ret = NO_ERROR;

    if (m_nodeState == NODE_STATE_CREATED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_cipInterface != NULL) {
        if (m_videoNodeNum == FIMC_IS_VIDEO_CIP0S_NUM) {
            ret = cip_close(m_cipInterface);
            if (ret != NO_ERROR) {
                CLOGE("close fail");
                return ret;
            }

            ret = cip_destroy(&m_cipInterface);
            if (ret != NO_ERROR) {
                CLOGE("destory fail");
                return ret;
            }
            delete m_cipInterface;
        }
        m_cipInterface = NULL;
    }

    m_nodeType = NODE_TYPE_BASE;
    m_videoNodeNum = -1;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_CREATED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::getInternalModule(void **module)
{
    *module = m_cipInterface;

    if (m_cipInterface == NULL) {
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setColorFormat(int v4l2Colorformat, __unused int planeCount, __unused int batchSize,
                                                __unused enum YUV_RANGE yuvRange, __unused camera_pixel_size pixelSize)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    CLOGD("format %d(%c %c %c %c), yuvRange %d",
            v4l2Colorformat,
            (char)((v4l2Colorformat >> 0) & 0xFF),
            (char)((v4l2Colorformat >> 8) & 0xFF),
            (char)((v4l2Colorformat >> 16) & 0xFF),
            (char)((v4l2Colorformat >> 24) & 0xFF),
            m_v4l2Format.fmt.pix_mp.colorspace);

    if(m_nodeLocation == NODE_LOCATION_SRC) {
        ret = cip_setColorFormat(m_cipInterface, v4l2Colorformat);
    } else {
        ret = cip_setColorFormat(m_cipInterface, v4l2Colorformat);
    }

    if (ret != NO_ERROR) {
        CLOGE("CIP setColorFormat fail, ret(%d)", ret);
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeCIP::setSize(int w, int h)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    CLOGD("w (%d) h (%d)", w, h);

    if (m_videoNodeNum == FIMC_IS_VIDEO_CIP0S_NUM) {
        ret = cip_prepare(m_cipInterface, w, h, w, h);
        if (ret != NO_ERROR) {
            CLOGE("CIP prepare fail, ret(%d)", ret);
        }
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeCIP::reqBuffers(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::clrBuffers(void)
{
    return NO_ERROR;
}

unsigned int ExynosCameraNodeCIP::reqBuffersCount(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setControl(__unused unsigned int id, __unused int value)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::getControl(__unused unsigned int id, __unused int *value)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::polling(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setInput(int sensorId)
{
    m_sensorId = sensorId;

    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setFormat(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setFormat(__unused unsigned int bytesPerPlane[])
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::setCrop(__unused enum v4l2_buf_type type,
                                          __unused int x, __unused int y, __unused int w, __unused int h)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::start(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::stop(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::prepareBuffer(ExynosCameraBuffer *buf)
{
    status_t ret = NO_ERROR;

    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::putBuffer(ExynosCameraBuffer *buf)
{
    status_t ret = NO_ERROR;

    if (buf == NULL) {
        CLOGE("buffer is NULL");
        return BAD_VALUE;
    }

#if 0
    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
#endif

    cip_buffer_t *cipBuffer;
    cip_in_buffer_t property = CIP_WIDE_YUV;

    cipBuffer->planeCount = buf->planeCount;
    for (int i = 0; i < buf->planeCount; i++) {
        cipBuffer->fd[i] = buf->fd[i];
        cipBuffer->size[i] = buf->size[i];
        cipBuffer->addr[i] = buf->addr[i];
    }

    switch (m_videoNodeNum) {
    case FIMC_IS_VIDEO_CIP0S_NUM:
        if (m_cipMode == 0) {
            property = CIP_WIDE_YUV;
        } else {
            property = CIP_TELE_YUV;
        }
            
        ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            return ret;
        }

        ret = cip_updateConfig(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("CIP updateConfig fail, ret(%d)", ret);
            return ret;
        }

        ret = cip_execute(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("CIP execute fail, ret(%d)", ret);
            return ret;
        }

        break;
    case FIMC_IS_VIDEO_CIP1S_NUM:
        property = CIP_TELE_YUV;

        ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            return ret;
        }

        break;
    case FIMC_IS_VIDEO_CIP2S_NUM:
        property = CIP_DEPTH_MAP;

        ret = cip_setInBuf(m_cipInterface, property, rect, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            return ret;
        }

        break;
    case FIMC_IS_VIDEO_CIP0C_NUM:
        ret = cip_setOutBuf(m_cipInterface, cipBuffer);
        if (ret != NO_ERROR) {
            CLOGE("CIP setInBuffer fail, ret(%d)", ret);
            return ret;
        }

        break;
    default:
        CLOGE("Invalid node num(%d)", ret);
        break;
    }

    m_buffer = *buf;

    return NO_ERROR;
}

status_t ExynosCameraNodeCIP::getBuffer(ExynosCameraBuffer *buf, int *dqIndex)
{
    status_t ret = NO_ERROR;

#if 0
    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
#endif

    if (m_videoNodeNum == FIMC_IS_VIDEO_CIP0S_NUM) {
        ret = cip_waitDone(m_cipInterface);
        if (ret != NO_ERROR) {
            CLOGE("CIP waitDone fail, ret(%d)", ret);
            return ret;
        }
    }

    *buf = m_buffer;
    *dqIndex = m_buffer.index;

    return ret;
}

void ExynosCameraNodeCIP::dump(void)
{
    dumpState();
    dumpQueue();

    return;
}

void ExynosCameraNodeCIP::dumpState(void)
{
    CLOGD("");

    if (strnlen(m_name, sizeof(char) * EXYNOS_CAMERA_NAME_STR_SIZE) > 0 )
        CLOGD("m_name[%s]", m_name);
    if (strnlen(m_alias, sizeof(char) * EXYNOS_CAMERA_NAME_STR_SIZE) > 0 )
        CLOGD("m_alias[%s]", m_alias);

    CLOGD("m_fd[%d], width[%d], height[%d]",
        m_fd,
        m_v4l2Format.fmt.pix_mp.width,
        m_v4l2Format.fmt.pix_mp.height);
    CLOGD("m_format[%d], m_planes[%d], m_buffers[%d], m_memory[%d]",
        m_v4l2Format.fmt.pix_mp.pixelformat,
        m_v4l2Format.fmt.pix_mp.num_planes,
        m_v4l2ReqBufs.count,
        m_v4l2ReqBufs.memory);
    CLOGD("m_type[%d], m_flagStart[%d], m_flagCreate[%d]",
        m_v4l2ReqBufs.type,
        m_flagStart,
        m_flagCreate);
    CLOGD("m_crop type[%d], X : Y[%d : %d], W : H[%d : %d]",
        m_crop.type,
        m_crop.c.top,
        m_crop.c.left,
        m_crop.c.width,
        m_crop.c.height);

    CLOGI("Node state(%d)", m_nodeRequest.getState());

    return;
}

void ExynosCameraNodeCIP::dumpQueue(void)
{
    return;
}

};
