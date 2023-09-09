/*
 * Copyright 2013, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraNode"

#include"ExynosCameraNode.h"
#include"ExynosCameraUtils.h"

namespace android {

/* ExynosCameraNodeItem */
ExynosCameraNodeRequest::ExynosCameraNodeRequest()
{
    m_requestState = NODE_REQUEST_STATE_BASE;
    m_requestCount = 0;
}

ExynosCameraNodeRequest::~ExynosCameraNodeRequest()
{
}

void ExynosCameraNodeRequest::setState(enum node_request_state state)
{
    m_requestState = state;
}

enum node_request_state ExynosCameraNodeRequest::getState(void)
{
    return m_requestState;
}

void ExynosCameraNodeRequest::setRequest(unsigned int requestCount)
{
    m_requestCount = requestCount;
}

unsigned int ExynosCameraNodeRequest::getRequest(void)
{
    return m_requestCount;
}


ExynosCameraNode::ExynosCameraNode()
{
    memset(m_name,  0x00, sizeof(m_name));
    memset(m_alias, 0x00, sizeof(m_alias));
    memset(&m_v4l2Format,  0x00, sizeof(struct v4l2_format));
    memset(&m_v4l2ReqBufs, 0x00, sizeof(struct v4l2_requestbuffers));

    m_fd        = NODE_INIT_NEGATIVE_VALUE;
    m_flagCopyedFd = false;

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
    m_dummyColorIndex = 0;

    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        m_queueBufferList[i].index = NODE_INIT_NEGATIVE_VALUE;
    }

    m_nodeType = NODE_TYPE_BASE;
}

ExynosCameraNode::~ExynosCameraNode()
{
    EXYNOS_CAMERA_NODE_IN();

    destroy();
}

status_t ExynosCameraNode::create()
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeType = NODE_TYPE_BASE;

    m_flagCreate = true;

    return NO_ERROR;
}

status_t ExynosCameraNode::create(const char *nodeName)
{
    return create(nodeName, 0);
}

status_t ExynosCameraNode::create(const char *nodeName, int cameraId)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    if (nodeName == NULL)
        return BAD_VALUE;

    if (cameraId >= 0)
        m_cameraId = cameraId;

    ret = create(nodeName, nodeName);
    if (ret != NO_ERROR) {
        CLOGE("create fail [%d]", (int)ret);
        return ret;
    }

    return NO_ERROR;
}

status_t ExynosCameraNode::create(const char *nodeName, const char *nodeAlias)
{
    EXYNOS_CAMERA_NODE_IN();

    if ((nodeName == NULL) || (nodeAlias == NULL))
        return BAD_VALUE;

    m_nodeType = NODE_TYPE_BASE;

    strncpy(m_name,  nodeName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    strncpy(m_alias, nodeAlias, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    CLOGV("Create");

    m_nodeStateLock.lock();
    m_nodeState  = NODE_STATE_CREATED;
    m_flagCreate = true;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::create(const char *nodeName, int cameraId, int fd)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    if (nodeName == NULL)
        return BAD_VALUE;

    if (cameraId > 0)
        m_cameraId = cameraId;

    ret = create(nodeName, nodeName);
    if (ret != NO_ERROR) {
        CLOGE("create fail [%d]", (int)ret);
        return ret;
    }

    m_fd = fd;
    m_flagCopyedFd = true;

    m_nodeType = NODE_TYPE_BASE;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_OPENED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::create(
        __unused const char *nodeName,
        __unused int cameraId,
        __unused enum EXYNOS_CAMERA_NODE_JPEG_HAL_LOCATION location,
        __unused ExynosJpegEncoderForCamera *jpegEncoder)
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::destroy(void)
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeStateLock.lock();
    m_nodeState  = NODE_STATE_DESTROYED;
    m_flagCreate = false;
    m_nodeStateLock.unlock();

    m_removeItemBufferQ();

    m_nodeType = NODE_TYPE_BASE;
    m_flagCopyedFd = false;

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::open(int videoNodeNum)
{
    EXYNOS_CAMERA_NODE_IN();

    CLOGV("open");

    char node_name[30];

    if (m_nodeState != NODE_STATE_CREATED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    memset(&node_name, 0x00, sizeof(node_name));
    snprintf(node_name, sizeof(node_name), "%s%d", NODE_PREFIX, videoNodeNum);

    if (videoNodeNum == (int)NODE_TYPE_DUMMY) {
        m_fd = NODE_TYPE_DUMMY;
        m_nodeType = NODE_TYPE_DUMMY;
        m_dummyIndexQ.clear();

        CLOGW(" dummy node opened");
    } else if (m_flagCopyedFd == true) {
        CLOGD("Node(%d)(%s) is copyed fd. so, skip exynos_v4l2_open(%d)", videoNodeNum, node_name, m_fd);
    } else {
        m_fd = exynos_v4l2_open(node_name, O_RDWR, 0);
        if (m_fd < 0) {
            CLOGE("exynos_v4l2_open(%s) fail, ret(%d) errno(%s)",
                node_name, m_fd, strerror(errno));
            return INVALID_OPERATION;
        }
        CLOGD(" Node(%d)(%s) opened. m_fd(%d)", videoNodeNum, node_name, m_fd);
    }

    m_videoNodeNum = videoNodeNum;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_OPENED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::open(int videoNodeNum, __unused bool useThumbnailHWFC)
{
    return open(videoNodeNum);
}

status_t ExynosCameraNode::close(void)
{
    EXYNOS_CAMERA_NODE_IN();

    CLOGD("close(fd:%d)", m_fd);

    if (m_nodeState == NODE_STATE_CREATED) {
        CLOGE("m_nodeState = [%d] is not valid",
            (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_nodeType == NODE_TYPE_DUMMY) {
        m_dummyIndexQ.clear();
        CLOGW("dummy node closed");
    } else if (m_flagCopyedFd == true) {
        CLOGD("Node(%d)(%s) is copyed fd. so, skip exynos_v4l2_close(%d)", m_videoNodeNum, m_name, m_fd);
    } else {
        if (exynos_v4l2_close(m_fd) != 0) {
            CLOGE("close fail");
            return INVALID_OPERATION;
        }
    }

    m_nodeType = NODE_TYPE_BASE;
    m_videoNodeNum = -1;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_CREATED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}


bool ExynosCameraNode::flagOpened(void)
{
    bool bRet = false;

    {
        m_nodeStateLock.lock();

        switch (m_nodeState) {
        case NODE_STATE_OPENED:
        case NODE_STATE_IN_PREPARE:
        case NODE_STATE_RUNNING:
            bRet = true;
            break;
        default:
            break;
        }

        m_nodeStateLock.unlock();
    }

    return bRet;
}

status_t ExynosCameraNode::getFd(int *fd)
{
    *fd = m_fd;

    return NO_ERROR;
}

status_t ExynosCameraNode::getJpegEncoder(__unused ExynosJpegEncoderForCamera **jpegEncoder)
{
    return NO_ERROR;
}

char *ExynosCameraNode::getName(void)
{
    return m_name;
}

status_t ExynosCameraNode::setColorFormat(int v4l2Colorformat, int planesCount, int batchSize, enum YUV_RANGE yuvRange)
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    m_v4l2Format.fmt.pix_mp.pixelformat = v4l2Colorformat;
    m_v4l2Format.fmt.pix_mp.num_planes  = planesCount;

    /* Indicates num_buffers for multi buffer */
    m_v4l2Format.fmt.pix_mp.reserved[0] = batchSize;

    int v4l2ColorRange = m_YUV_RANGE_2_V4L2_COLOR_RANGE(yuvRange);
    if (v4l2ColorRange < 0) {
        CLOGE("invalid yuvRange : %d. so, fail",
             (int)yuvRange);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    m_v4l2Format.fmt.pix_mp.colorspace  = (enum v4l2_colorspace)v4l2ColorRange;

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::getColorFormat(int *v4l2Colorformat, int *planesCount, enum YUV_RANGE *yuvRange)
{
    EXYNOS_CAMERA_NODE_IN();

    *v4l2Colorformat = m_v4l2Format.fmt.pix_mp.pixelformat;
    *planesCount     = m_v4l2Format.fmt.pix_mp.num_planes;

    if (yuvRange)
        *yuvRange = m_V4L2_COLOR_RANGE_2_YUV_RANGE(m_v4l2Format.fmt.pix_mp.colorspace);

    return NO_ERROR;
}

status_t ExynosCameraNode::setQuality(__unused int quality)
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::setQuality(__unused const unsigned char qtable[])
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::setSize(int w, int h)
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    CLOGD("w (%d) h (%d)", w, h);

    m_v4l2Format.fmt.pix_mp.width  = w;
    m_v4l2Format.fmt.pix_mp.height = h;

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::getSize(int *w, int *h)
{
    EXYNOS_CAMERA_NODE_IN();

    *w = m_v4l2Format.fmt.pix_mp.width;
    *h = m_v4l2Format.fmt.pix_mp.height;

    return NO_ERROR;
}

status_t ExynosCameraNode::setId(__unused int id)
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::setBufferType(
        int bufferCount,
        enum v4l2_buf_type type,
        enum v4l2_memory bufferMemoryType)
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    m_v4l2ReqBufs.count  = bufferCount;
    m_v4l2ReqBufs.type   = type;
    m_v4l2ReqBufs.memory = bufferMemoryType;

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::getBufferType(
        int *bufferCount,
        enum v4l2_buf_type *type,
        enum v4l2_memory *bufferMemoryType)
{
    EXYNOS_CAMERA_NODE_IN();

    *bufferCount      = m_v4l2ReqBufs.count;
    *type             = (enum v4l2_buf_type)m_v4l2ReqBufs.type;
    *bufferMemoryType = (enum v4l2_memory)m_v4l2ReqBufs.memory;

    return NO_ERROR;
}

/* Should not implement in DMA_BUF and USER_PTR types */
status_t ExynosCameraNode::queryBuf(void)
{
    return INVALID_OPERATION;
}

status_t ExynosCameraNode::reqBuffers(void)
{
    EXYNOS_CAMERA_NODE_IN();

    int result = 0;

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    if (m_reqBuffers(&result) < 0) {
        CLOGE("m_setFmt fail result[%d]", result);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    m_nodeState = NODE_STATE_IN_PREPARE;

    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::clrBuffers(void)
{
    EXYNOS_CAMERA_NODE_IN();

    int result = 0;

    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_clrBuffers(&result) < 0) {
        CLOGE("m_clrBuffers fail result[%d]", result);
        return INVALID_OPERATION;
    }

#if 0
    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();
#endif

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

unsigned int ExynosCameraNode::reqBuffersCount(void)
{
    return m_nodeRequest.getRequest();
}

status_t ExynosCameraNode::setControl(unsigned int id, int value)
{
    EXYNOS_CAMERA_NODE_IN();

/*
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("[%s] [%d] m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
*/

    if (m_setControl(id, value) < 0) {
        CLOGE("m_setControl fail");
        return INVALID_OPERATION;
    }

/*
    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();
*/
    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::getControl(unsigned int id, int *value)
{
    EXYNOS_CAMERA_NODE_IN();

/*
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("[%s] [%d] m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
*/

    if (m_getControl(id, value) < 0) {
        CLOGE("m_getControl fail");
        return INVALID_OPERATION;
    }

/*
    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();
*/
    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::setExtControl(struct v4l2_ext_controls *ctrl)
{
    EXYNOS_CAMERA_NODE_IN();

    if (m_setExtControl(ctrl) < 0) {
        CLOGE("m_setExtControl fail");
        return INVALID_OPERATION;
    }

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::polling(void)
{
    EXYNOS_CAMERA_NODE_IN();
/*
    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("[%s] [%d] m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
*/
    if (m_polling() < 0) {
        CLOGE("m_polling fail");
        return INVALID_OPERATION;
    }
/*
    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();
*/
    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::setInput(int sensorId)
{
    EXYNOS_CAMERA_NODE_IN();

    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_setInput(sensorId) < 0) {
        CLOGE("m_setInput fail [%d]", sensorId);
        return INVALID_OPERATION;
    }

    m_sensorId = sensorId;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

int ExynosCameraNode::getInput(void)
{
    return m_sensorId;
}

int ExynosCameraNode::resetInput(void)
{
    m_sensorId = 0;

    return NO_ERROR;
}

int ExynosCameraNode::getNodeNum(void)
{
    return m_videoNodeNum;
}

status_t ExynosCameraNode::setFormat(void)
{
    EXYNOS_CAMERA_NODE_IN();

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    if (m_setFmt() < 0) {
        CLOGE("m_setFmt fail");
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::setFormat(unsigned int bytesPerPlane[])
{
    status_t ret = NO_ERROR;

    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    for (int i = 0; i < m_v4l2Format.fmt.pix_mp.num_planes; i++)
        m_v4l2Format.fmt.pix_mp.plane_fmt[i].bytesperline = bytesPerPlane[i];

    if (0 < m_v4l2Format.fmt.pix_mp.num_planes &&
        m_v4l2Format.fmt.pix_mp.plane_fmt[0].bytesperline < m_v4l2Format.fmt.pix_mp.width) {
        CLOGW("name(%s) fd %d, bytesperline(%d) < width(%d), height(%d)",
            m_name, m_fd,
            m_v4l2Format.fmt.pix_mp.plane_fmt[0].bytesperline,
            m_v4l2Format.fmt.pix_mp.width,
            m_v4l2Format.fmt.pix_mp.height);
    }

    ret = setFormat();
    if (ret != NO_ERROR) {
        CLOGE("setFormat() is failed [%d]", (int)ret);
        return ret;
    }

    return NO_ERROR;
}
status_t ExynosCameraNode::setCrop(enum v4l2_buf_type type, int x, int y, int w, int h)
{
    EXYNOS_CAMERA_NODE_IN();

    int ret = 0;
    struct v4l2_crop crop;
    memset(&crop, 0x00, sizeof(struct v4l2_crop));

    m_nodeStateLock.lock();
    if (m_nodeState != NODE_STATE_IN_PREPARE && m_nodeState != NODE_STATE_OPENED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        m_nodeStateLock.unlock();
        return INVALID_OPERATION;
    }

    crop.type     = type;
    crop.c.top    = x;
    crop.c.left   = y;
    crop.c.width  = w;
    crop.c.height = h;

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("type %d, %d x %d, top %d, left %d",
        crop.type,
        crop.c.width,
        crop.c.height,
        crop.c.top,
        crop.c.left);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_s_crop(m_fd, &crop);
        if (ret < 0) {
            CLOGE("exynos_v4l2_s_crop fail (%d)", ret);
            m_nodeStateLock.unlock();
            return ret;
        }
    }

    /* back-up to debug */
    m_crop = crop;

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::setExifInfo(__unused exif_attribute_t *exifInfo)
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::setDebugInfo(__unused debug_attribute_t *debugInfo)
{
    EXYNOS_CAMERA_NODE_IN();

    return NO_ERROR;
}

status_t ExynosCameraNode::start(void)
{
    EXYNOS_CAMERA_NODE_IN();

    if (m_nodeState != NODE_STATE_IN_PREPARE) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_streamOn() < 0) {
        CLOGE("m_streamOn fail");
        return INVALID_OPERATION;
    }

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_RUNNING;
    m_flagStart = true;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

status_t ExynosCameraNode::stop(void)
{
    EXYNOS_CAMERA_NODE_IN();

    if (m_nodeState < NODE_STATE_OPENED || m_nodeState > NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_streamOff() < 0) {
        CLOGE("m_streamOff fail");
        return INVALID_OPERATION;
    }

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_IN_PREPARE;
    m_flagStart = false;
    m_nodeStateLock.unlock();

    m_removeItemBufferQ();

    EXYNOS_CAMERA_NODE_OUT();

    return NO_ERROR;
}

bool ExynosCameraNode::isCreated(void)
{
    return m_flagCreate;
}

bool ExynosCameraNode::isStarted(void)
{
    return m_flagStart;
}

status_t ExynosCameraNode::prepareBuffer(ExynosCameraBuffer *buf)
{
    status_t ret = NO_ERROR;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];

    if (m_isValidBuffer(buf) == false) {
        CLOGE("Invalid buffer is delivered. node(%s) index(%d)",
                m_name, buf->index);
        return (int)BAD_VALUE;
    }

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

    v4l2_buf.m.planes = planes;
    v4l2_buf.type     = m_v4l2ReqBufs.type;
    v4l2_buf.memory   = m_v4l2ReqBufs.memory;
    v4l2_buf.index    = buf->index;
    v4l2_buf.length   = (m_v4l2Format.fmt.pix_mp.num_planes - 1)
                        * m_v4l2Format.fmt.pix_mp.reserved[0] + 1;

    for (int i = 0; i < (int)v4l2_buf.length; i++) {
        if (v4l2_buf.memory == V4L2_MEMORY_DMABUF) {
            v4l2_buf.m.planes[i].m.fd = (int)(buf->fd[i]);
        } else if (v4l2_buf.memory == V4L2_MEMORY_USERPTR) {
            v4l2_buf.m.planes[i].m.userptr = (unsigned long)(buf->addr[i]);
        } else {
            CLOGE("invalid srcNode->memory(%d)", v4l2_buf.memory);
            return INVALID_OPERATION;
        }

        v4l2_buf.m.planes[i].length = (unsigned long)(buf->size[i]);
    }

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d, index(%d), length(%d), width(%d), height(%d), bytesperline(%d)",
            m_name,
            m_fd,
            v4l2_buf.index,
            v4l2_buf.length,
            m_v4l2Format.fmt.pix_mp.width,
            m_v4l2Format.fmt.pix_mp.height,
            m_v4l2Format.fmt.pix_mp.plane_fmt[v4l2_buf.index].bytesperline);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        //m_dummyIndexQ.push_back(v4l2_buf.index);
    } else {
        ret = exynos_v4l2_prepare(m_fd, &v4l2_buf);
        if (ret < 0) {
            CLOGE("exynos_v4l2_prepare(m_fd:%d, buf->index:%d) fail (%d)",
                     m_fd, v4l2_buf.index, ret);
            return ret;
        }
    }

    CLOGI("prepareBuffers(%d)", v4l2_buf.index);

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d, index %d done",
             m_name, m_fd, v4l2_buf.index);
#endif

    return ret;
}

status_t ExynosCameraNode::putBuffer(ExynosCameraBuffer *buf)
{
    if (buf == NULL) {
        CLOGE("buffer is NULL");
        return BAD_VALUE;
    }

    if (m_nodeRequest.getRequest() == 0 ) {
        CLOGE("requestBuf is 0, disable queue/dequeue");
        return BAD_VALUE;
    }

#if 0
    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
#endif

    if (m_qBuf(buf) < 0) {
        CLOGE("qBuf fail");
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraNode::getBuffer(ExynosCameraBuffer *buf, int *dqIndex)
{
    int ret = NO_ERROR;

    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    if (m_nodeRequest.getRequest() == 0 ) {
        CLOGE("requestBuf is 0, disable queue/dequeue");
        return BAD_VALUE;
    }

    ret = m_dqBuf(buf, dqIndex);

    return ret;
}

void ExynosCameraNode::dump(void)
{
    dumpState();
    dumpQueue();

    return;
}

void ExynosCameraNode::dumpState(void)
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

void ExynosCameraNode::dumpQueue(void)
{
    m_printBufferQ();

    return;
}

int  ExynosCameraNode::m_pixelDepth(void)
{
    return NO_ERROR;
}

bool ExynosCameraNode::m_getFlagQ(__unused int index)
{
    return NO_ERROR;
}

bool ExynosCameraNode::m_setFlagQ(__unused int index, __unused bool toggle)
{
    return NO_ERROR;
}

int ExynosCameraNode::m_polling(void)
{
    struct pollfd events;

    if (m_nodeType == NODE_TYPE_DUMMY)
        return 0;

    /* 50 msec * 40 = 2sec */
    int cnt = 40;
    long sec = 50; /* 50 msec */

    int ret = 0;
    int pollRet = 0;

    events.fd = m_fd;
    events.events = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM | POLLERR;
    events.revents = 0;

    while (cnt--) {
        pollRet = poll(&events, 1, sec);
        if (pollRet < 0) {
            ret = -1;
        } else if (0 < pollRet) {
            if (events.revents & POLLIN) {
                break;
            } else if (events.revents & POLLERR) {
                ret = -1;
            }
        }
    }

    if (ret < 0 || cnt <= 0) {
        CLOGE("poll[%d], pollRet(%d) event(0x%x), cnt(%d)",
             m_fd, pollRet, events.revents, cnt);

        if (cnt <= 0)
            ret = -1;
    }

    return ret;
}

int ExynosCameraNode::m_streamOn(void)
{
    int ret = 0;
#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("fd %d, type %d",
         m_fd, (int)m_v4l2ReqBufs.type);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_streamon(m_fd, (enum v4l2_buf_type)m_v4l2ReqBufs.type);
        if (ret < 0) {
            CLOGE("exynos_v4l2_streamon(fd:%d, type:%d) fail (%d)",
                 m_fd, (int)m_v4l2ReqBufs.type, ret);
            return ret;
        }
    }

    m_nodeRequest.setState(NODE_REQUEST_STATE_READY);

    return ret;
}

int ExynosCameraNode::m_streamOff(void)
{
    int ret = 0;

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("fd %d, type %d",
         m_fd, (int)m_v4l2ReqBufs.type);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_streamoff(m_fd, (enum v4l2_buf_type)m_v4l2ReqBufs.type);
        if (ret < 0) {
            CLOGE("exynos_v4l2_streamoff(fd:%d, type:%d) fail (%d)",
                 m_fd, (int)m_v4l2ReqBufs.type, ret);
            return ret;
        }
    }

    m_nodeRequest.setState(NODE_REQUEST_STATE_STOPPED);

/*
    for (int i = 0; i < VIDEO_MAX_FRAME; i++)
        cam_int_m_setQ(node, i, false);
*/
    return ret;
}

int ExynosCameraNode::m_setInput(int id)
{
    int ret = 0;

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("fd %d, index %d", m_fd, id);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_s_input(m_fd, id);
        if (ret < 0)
            CLOGE("exynos_v4l2_s_input(fd:%d, index:%d) fail (%d)",
                 m_fd, id, ret);
    }

    return ret;
}

int ExynosCameraNode::m_setFmt(void)
{
    int ret = 0;

    if (m_v4l2Format.fmt.pix_mp.num_planes <= 0) {
        CLOGE("S_FMT, Out of bound : Number of element plane(%d)",
             m_v4l2Format.fmt.pix_mp.num_planes);
        return -1;
    }

/* #ifdef EXYNOS_CAMERA_NODE_TRACE */
    CLOGD("type %d, %d x %d, format %d(%c %c %c %c), yuvRange %d",
        m_v4l2ReqBufs.type,
        m_v4l2Format.fmt.pix_mp.width,
        m_v4l2Format.fmt.pix_mp.height,
        m_v4l2Format.fmt.pix_mp.pixelformat,
        (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 0) & 0xFF),
        (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 8) & 0xFF),
        (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 16) & 0xFF),
        (char)((m_v4l2Format.fmt.pix_mp.pixelformat >> 24) & 0xFF),
        m_v4l2Format.fmt.pix_mp.colorspace);
/* #endif */

    m_v4l2Format.type = m_v4l2ReqBufs.type;

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_s_fmt(m_fd, &m_v4l2Format);
        if (ret < 0) {
            CLOGE("exynos_v4l2_s_fmt(fd:%d) fail (%d)",
                 m_fd, ret);
            return ret;
        }
    }

    return ret;
}

int ExynosCameraNode::m_reqBuffers(int *reqCount)
{
    int ret = 0;

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("fd %d, count %d, type %d, memory %d",
        m_fd,
        m_v4l2ReqBufs.count,
        m_v4l2ReqBufs.type,
        m_v4l2ReqBufs.memory);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_reqbufs(m_fd, &m_v4l2ReqBufs);
        if (ret < 0) {
            CLOGE("exynos_v4l2_reqbufs(fd:%d, count:%d) fail (%d)",
                 m_fd, m_v4l2ReqBufs.count, ret);
            return ret;
        }
    }

#if 0
    for (int i = 0; i < VIDEO_MAX_FRAME; i++)
        cam_int_m_setQ(node, i, false);
#endif

    m_nodeRequest.setRequest(m_v4l2ReqBufs.count);

    *reqCount = m_v4l2ReqBufs.count;

    return ret;
}

void ExynosCameraNode::removeItemBufferQ()
{
    m_removeItemBufferQ();
}

int ExynosCameraNode::m_clrBuffers(int *reqCount)
{
    int ret = 0;

    if (m_nodeRequest.getRequest() == 0) {
        CLOGE("not yet requested. so, just return 0(fd:%d, count:%d)",
                 m_fd, m_v4l2ReqBufs.count);

        m_v4l2ReqBufs.count = 0;

        goto done;
    }

    m_v4l2ReqBufs.count = 0;

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("fd %d, count %d, type %d, memory %d",
        m_fd,
        m_v4l2ReqBufs.count,
        m_v4l2ReqBufs.type,
        m_v4l2ReqBufs.memory);
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_reqbufs(m_fd, &m_v4l2ReqBufs);
        if (ret < 0) {
            CLOGE("exynos_v4l2_reqbufs(fd:%d, count:%d) fail (%d)",
                 m_fd, m_v4l2ReqBufs.count, ret);
            return ret;
        }
    }

done:

#if 0
    for (int i = 0; i < VIDEO_MAX_FRAME; i++)
        cam_int_m_setQ(node, i, false);
#endif

    m_nodeRequest.setRequest(m_v4l2ReqBufs.count);

    *reqCount = m_v4l2ReqBufs.count;

    return ret;
}

int ExynosCameraNode::m_setCrop(__unused int v4l2BufType, __unused ExynosRect *rect)
{
    return NO_ERROR;
}

int ExynosCameraNode::setParam(struct v4l2_streamparm *stream_parm)
{
    int ret = 0;

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_s_parm(m_fd, stream_parm);
        if (ret < 0) {
            CLOGE("exynos_v4l2_s_parm(fd:%d) fail (%d)",
                 m_fd, ret);
        }
    }

    return ret;
}

int ExynosCameraNode::m_setControl(unsigned int id, int value)
{
    int ret = 0;

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* nop */
    } else {
        ret = exynos_v4l2_s_ctrl(m_fd, id, value);
        if (ret < 0) {
            CLOGE("exynos_v4l2_s_ctrl(fd:%d) fail (%d) [id %d, value %d]",
                 m_fd, ret, id, value);
            return ret;
        }
    }

    return ret;
}

int ExynosCameraNode::m_getControl(unsigned int id, int *value)
{
    int ret = 0;

    if (m_nodeType == NODE_TYPE_DUMMY) {
         /* nop */
        if (id == V4L2_CID_IS_G_BNS_SIZE) {
            // You can change this size, up to your sensor size and fastAE size.
            int bnsWidth = 4032;
            //int bnsHeight = 2268;
            int bnsHeight = 3024;
            int bns = (bnsWidth << 16) | (bnsHeight & 0xffff);
            *value = bns;
        }
    } else {
        ret = exynos_v4l2_g_ctrl(m_fd, id, value);
        if (ret < 0) {
            CLOGE("exynos_v4l2_g_ctrl(fd:%d) fail (%d) [id %d, value %d]",
                 m_fd, ret, id, *value);
            return ret;
        }
    }

    return ret;
}

int ExynosCameraNode::m_setExtControl(struct v4l2_ext_controls *ctrl)
{
    int ret = 0;

    if (m_nodeType == NODE_TYPE_DUMMY) {
        /* no operation */
    } else {
        ret = exynos_v4l2_s_ext_ctrl(m_fd, ctrl);
        if (ret < 0) {
            CLOGE("exynos_v4l2_s_ext_ctrl(fd:%d) fail (%d)",
                     m_fd, ret);
            return ret;
        }
    }

    return ret;
}

int ExynosCameraNode::m_qBuf(ExynosCameraBuffer *buf)
{
    int ret = 0;
    int error = 0;
    int i = 0;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    if( m_isExistBufferQ(buf) ) {
        CLOGE("queue index already exist!! index(%d)", buf->index);
        m_printBufferQ();
        return -1;
    }

    if (m_isValidBuffer(buf) == false) {
        CLOGE("Invalid buffer is delivered. node(%s) index(%d)",
                m_name, buf->index);
        return (int)BAD_VALUE;
    }

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

    v4l2_buf.m.planes = planes;
    v4l2_buf.type     = m_v4l2ReqBufs.type;
    v4l2_buf.memory   = m_v4l2ReqBufs.memory;
    v4l2_buf.index    = buf->index;
    v4l2_buf.length   = (m_v4l2Format.fmt.pix_mp.num_planes - 1)
                        * m_v4l2Format.fmt.pix_mp.reserved[0] + 1;

    for (i = 0; i < (int)v4l2_buf.length; i++) {
        if (v4l2_buf.memory == V4L2_MEMORY_DMABUF) {
            v4l2_buf.m.planes[i].m.fd = (int)(buf->fd[i]);
        } else if (v4l2_buf.memory == V4L2_MEMORY_USERPTR) {
            v4l2_buf.m.planes[i].m.userptr = (unsigned long)(buf->addr[i]);
        } else {
            CLOGE("invalid srcNode->memory(%d)", v4l2_buf.memory);
            return -1;
        }

        v4l2_buf.m.planes[i].length = (unsigned long)(buf->size[i]);
    }

    /* set fence */
    v4l2_buf.flags = V4L2_BUF_FLAG_USE_SYNC;
    /* set cache flags */
    if (V4L2_TYPE_IS_OUTPUT(v4l2_buf.type)) {
        /*
         * TODO:
         * To reduce cache operation for optimization,
         * V4L2_BUF_FLAG_NO_CACHE_CLEAN is set now.
         * Until now, there's any usecase to need cache operation.
         * But there might be the scenaio to do cache operation.
        */
        v4l2_buf.flags |= ((V4L2_BUF_FLAG_NO_CACHE_CLEAN) | (V4L2_BUF_FLAG_NO_CACHE_INVALIDATE));
    } else {
        v4l2_buf.flags |= (V4L2_BUF_FLAG_NO_CACHE_CLEAN);
    }

    v4l2_buf.reserved = buf->acquireFence;

    int orgFence = v4l2_buf.reserved;

    m_nodeRequest.setState(NODE_REQUEST_STATE_QBUF_BLOCK);

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d, index(%d), length(%d), width(%d), height(%d), bytesperline(%d)",
        m_name,
        m_fd,
        v4l2_buf.index,
        v4l2_buf.length,
        m_v4l2Format.fmt.pix_mp.width,
        m_v4l2Format.fmt.pix_mp.height,
        m_v4l2Format.fmt.pix_mp.plane_fmt[v4l2_buf.index].bytesperline);
#endif

#ifdef EXYNOS_CAMERA_NODE_TRACE_Q_DURATION
    m_qTimer.stop();
    CLOGD("EXYNOS_CAMERA_NODE_TRACE_Q_DURATION name(%s) : %d msec",
         m_name, (int)m_qTimer.durationUsecs() / 1000);
    m_qTimer.start();
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        m_dummyIndexQ.push_back(v4l2_buf.index);
    } else {
        ret = exynos_v4l2_qbuf(m_fd, &v4l2_buf);
        if (ret < 0) {
            CLOGE("exynos_v4l2_qbuf(m_fd:%d, buf->index:%d) fail (%d)",
                 m_fd, buf->index, ret);
            return ret;
        }
    }

    /*
     * Disable below close.
     * This seems not need.
     */
#if 0
    /*
     * After give acquire fence to driver,
     * HAL don't need any more.
     */
    if (0 <= buf->acquireFence) {
        exynos_v4l2_close(buf->acquireFence);
    }
#endif

    /* Important : Give the changed fence (by driver) to caller. */
    buf->releaseFence = static_cast<int>(v4l2_buf.reserved);

    CLOGV("fence:v4l2_buf.reserved %d -> %d",
         orgFence, static_cast<int>(v4l2_buf.reserved));

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d, index %d done",
        m_name, m_fd, v4l2_buf.index);
#endif

    m_nodeRequest.setState(NODE_REQUEST_STATE_QBUF_DONE);

#if 0
    cam_int_m_setQ(node, index, true);
#endif

    error = m_putBufferQ(buf, &buf->index);
    if( error != NO_ERROR ) {
        ret = error;
    }
    /* m_printBufferQ(); */

    return ret;
}

int ExynosCameraNode::m_mBuf(ExynosCameraBuffer *buf)
{
    int ret = 0;
    int i = 0;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];
    if(m_isExistBufferQ(buf)) {
        CLOGE("queue index already exist!! index(%d)", buf->index);
        m_printBufferQ();
        return -1;
    }

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

    v4l2_buf.m.planes = planes;
    v4l2_buf.type     = m_v4l2ReqBufs.type;
    v4l2_buf.memory   = m_v4l2ReqBufs.memory;
    v4l2_buf.index    = buf->index;
    v4l2_buf.length   = (m_v4l2Format.fmt.pix_mp.num_planes - 1)
                        * m_v4l2Format.fmt.pix_mp.reserved[0] + 1;

    for (i = 0; i < (int)v4l2_buf.length; i++) {
        if (v4l2_buf.memory == V4L2_MEMORY_DMABUF) {
            v4l2_buf.m.planes[i].m.fd = (int)(buf->fd[i]);
        } else if (v4l2_buf.memory == V4L2_MEMORY_USERPTR) {
            v4l2_buf.m.planes[i].m.userptr = (unsigned long)(buf->addr[i]);
        } else {
            CLOGE("invalid srcNode->memory(%d)", v4l2_buf.memory);
            return -1;
        }

        v4l2_buf.m.planes[i].length = (unsigned long)(buf->size[i]);
    }

#ifndef SUPPORT_64BITS
    ret = exynos_v4l2_s_ctrl(m_fd, V4L2_CID_IS_MAP_BUFFER, (int)&v4l2_buf);
    if (ret < 0) {
        CLOGE("exynos_v4l2_s_ctrl(m_fd:%d, buf->index:%d) fail (%d)",
                 m_fd, buf->index, ret);
        return ret;
    }
#endif
#if 0
    cam_int_m_setQ(node, index, true);
#endif

    /* m_printBufferQ(); */

    return ret;
}

status_t ExynosCameraNode::m_drawColor(ExynosCameraBuffer *buf, int v4l2Format)
{
    status_t ret = NO_ERROR;

    #define DUMMY_YUV_COLOR_COUNTS (8)

    char color[DUMMY_YUV_COLOR_COUNTS][3] = {
        { /* red */
            81, 90, 240
        },
        { /* blue */
            41, 240, 110
        },
        { /* cyan */
            170, 166, 16
        },
        { /* magenta */
            106, 202, 222
        },
        { /* yellow */
            210, 16, 146
        },
        { /* green */
            145, 54, 34
        },
        { /* white */
            235, 128, 128
        },
        { /* black */
            16, 128, 128
        },
    };

    int colorIndex = 0;
    if (m_dummyColorIndex != 0) {
        colorIndex = ((m_dummyColorIndex / DUMMY_YUV_COLOR_COUNTS) % DUMMY_YUV_COLOR_COUNTS);
    }

    m_dummyColorIndex++;

    char *oneColor = color[colorIndex];

    switch (v4l2Format) {
    case V4L2_PIX_FMT_NV12:
        ret = m_drawNV12(buf, oneColor);
        break;
    case V4L2_PIX_FMT_NV12M:
        ret = m_drawNV12M(buf, oneColor);
        break;
    case V4L2_PIX_FMT_NV21:
        ret = m_drawNV21(buf, oneColor);
        break;
    case V4L2_PIX_FMT_NV21M:
        ret = m_drawNV21M(buf, oneColor);
        break;
    case V4L2_PIX_FMT_YVU420M:
        ret = m_drawYV12(buf, oneColor);
        break;
    case V4L2_PIX_FMT_YUYV:
        ret = m_drawYUYV(buf, oneColor);
        break;
    default:
        CLOGE("invalid v4l2Format(%d)", v4l2Format);
        ret = INVALID_OPERATION;
        break;
    }

    if (ret != NO_ERROR) {
        CLOGE("m_draw%c%c%c%c() fail",
            v4l2Format2Char(v4l2Format, 0),
            v4l2Format2Char(v4l2Format, 1),
            v4l2Format2Char(v4l2Format, 2),
            v4l2Format2Char(v4l2Format, 3));
    }

    return ret;
}

status_t ExynosCameraNode::m_drawNV12(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 1; i++) {
        if (buf->addr[i] == NULL) {
            CLOGE("buf->addr[%d] == NULL", i);
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y */
    memset(buf->addr[0], color[0], buf->size[0]);

    /* CrCb */
    // we don't know Y,cbcr size
    /*
    for (int i = 0; i < buf->size[1]; i += 2) {
        buf->addr[1][i + 0] = (char)color[1];
        buf->addr[1][i + 1] = (char)color[2];
    }
    */

    return NO_ERROR;
}

status_t ExynosCameraNode::m_drawNV12M(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 2; i++) {
        if (buf->addr[i] == NULL) {
            CLOGE("buf->addr[%d] == NULL", i);
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y */
    memset(buf->addr[0], color[0], buf->size[0]);

    /* CrCb */
    for (int i = 0; i < (int)buf->size[1]; i += 2) {
        buf->addr[1][i + 0] = (char)color[1];
        buf->addr[1][i + 1] = (char)color[2];
    }

    return NO_ERROR;
}

status_t ExynosCameraNode::m_drawNV21(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 1; i++) {
        if (buf->addr[i] == NULL) {
            CLOGE("buf->addr[%d] == NULL", i);
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y */
    memset(buf->addr[0], color[0], buf->size[0]);

    /* CrCb */
    // we don't know Y,cbcr size
    /*
    for (int i = 0; i < buf->size[1]; i += 2) {
        buf->addr[1][i + 0] = (char)color[2];
        buf->addr[1][i + 1] = (char)color[1];
    }
    */

    return NO_ERROR;
}

status_t ExynosCameraNode::m_drawNV21M(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 2; i++) {
        if (buf->addr[i] == NULL) {
            CLOGE("buf->addr[%d] == NULL", i);
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y */
    memset(buf->addr[0], color[0], buf->size[0]);

    /* CrCb */
    for (int i = 0; i < (int)buf->size[1]; i += 2) {
        buf->addr[1][i + 0] = (char)color[2];
        buf->addr[1][i + 1] = (char)color[1];
    }

    return NO_ERROR;
}

status_t ExynosCameraNode::m_drawYV12(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 3; i++) {
        if (buf->addr[i] == NULL) {
            CLOGE("buf->addr[%d] == NULL", i);
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y */
    memset(buf->addr[0], color[0], buf->size[0]);

    /* Cr */
    memset(buf->addr[1], color[1], buf->size[1]);

    /* Cb */
    memset(buf->addr[2], color[2], buf->size[2]);

    return NO_ERROR;
}

status_t ExynosCameraNode::m_drawYUYV(ExynosCameraBuffer *buf, char color[])
{
    for (int i = 0; i < 1; i++) {
        if (buf->addr[i] == NULL) {
            /* Hack */
            /* CLOGE("buf->addr[%d] == NULL", i); */
            return INVALID_OPERATION;
        } else if (buf->size[i] <= 0) {
            CLOGE("buf->size[%d] <= 0", i);
            return INVALID_OPERATION;
        }
    }

    /* Y Cb Y Cr*/
    for (int i = 0; i < (int)buf->size[0]; i += 4) {
        buf->addr[0][i + 0] = (char)color[0];
        buf->addr[0][i + 1] = (char)color[1];
        buf->addr[0][i + 2] = (char)color[0];
        buf->addr[0][i + 3] = (char)color[2];
    }

    return NO_ERROR;
}

int ExynosCameraNode::m_dqBuf(ExynosCameraBuffer *buf, int *dqIndex)
{
    int ret = 0;
    int error = 0;
    struct v4l2_buffer v4l2_buf;
    struct v4l2_plane planes[VIDEO_MAX_PLANES];

    memset(&v4l2_buf, 0, sizeof(struct v4l2_buffer));
    memset(&planes, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

    v4l2_buf.type       = m_v4l2ReqBufs.type;
    v4l2_buf.memory     = m_v4l2ReqBufs.memory;
    v4l2_buf.m.planes   = planes;
    v4l2_buf.length     = (m_v4l2Format.fmt.pix_mp.num_planes - 1)
                          * m_v4l2Format.fmt.pix_mp.reserved[0] + 1;

    m_nodeRequest.setState(NODE_REQUEST_STATE_DQBUF_BLOCK);

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d",
        m_name, m_fd);
#endif

#ifdef EXYNOS_CAMERA_NODE_TRACE_DQ_DURATION
    m_dqTimer.stop();
    CLOGD("EXYNOS_CAMERA_NODE_TRACE_DQ_DURATION name(%s) : %d msec",
         m_name, (int)m_dqTimer.durationUsecs() / 1000);
    m_dqTimer.start();
#endif

#ifdef EXYNOS_CAMERA_NODE_TRACE_DQ_DURATION
    ExynosCameraDurationTimer m_dqPerformanceTimer;
    m_dqPerformanceTimer.start();
#endif

    if (m_nodeType == NODE_TYPE_DUMMY) {
        if (m_dummyIndexQ.size() == 0) {
            v4l2_buf.index = 0;
        } else {
            List<int>::iterator r;

            r = m_dummyIndexQ.begin()++;
            v4l2_buf.index = *r;
            m_dummyIndexQ.erase(r);
        }
    } else {
        ret = exynos_v4l2_dqbuf(m_fd, &v4l2_buf);
        if (ret < 0) {
            if (ret != -EAGAIN)
                CLOGE("exynos_v4l2_dqbuf(fd:%d) fail (%d)", m_fd, ret);

            return ret;
        }
    }

#ifdef EXYNOS_CAMERA_NODE_TRACE_DQ_DURATION
    m_dqPerformanceTimer.stop();
    CLOGD("EXYNOS_CAMERA_NODE_TRACE_DQ_DURATION : DQ_PERFORMANCE name(%s) : %d msec",
         m_name, (int)m_dqPerformanceTimer.durationUsecs() / 1000);
#endif

#ifdef EXYNOS_CAMERA_NODE_TRACE
    CLOGD("name(%s) fd %d, index %d done",
        m_name, m_fd, v4l2_buf.index);
#endif

    m_nodeRequest.setState(NODE_REQUEST_STATE_DQBUF_DONE);

#if 1
//#ifdef USE_FOR_DTP
    if (v4l2_buf.flags & V4L2_BUF_FLAG_ERROR) {
        CLOGE("exynos_v4l2_dqbuf(fd:%d) returned with error (%d)",
             m_fd, V4L2_BUF_FLAG_ERROR);
        ret = -1;
    }
#endif

#if 0
    cam_int_m_setQ(node, v4l2_buf.index, false);
#endif

    *dqIndex = v4l2_buf.index;
    error = m_getBufferQ(buf, dqIndex);
    if( error != NO_ERROR ) {
        ret = error;
    }

    if (m_nodeType == NODE_TYPE_DUMMY) {
        int  waitTime = 0;

        /* fill with meaningful color */
        if (strcmp(m_name, "MCSC_PREVIEW") == 0 ||
            strcmp(m_name, "MCSC_PREVIEW_CALLBACK") == 0 ||
            strcmp(m_name, "MCSC_RECORDING") == 0 ||
            strcmp(m_name, "MCSC_PREVIEW_CALLBACK") == 0 ||
            strcmp(m_name, "MCSC_CAPTURE_MAIN") == 0 ||
            strcmp(m_name, "ODC_CAPTURE") == 0) {

            int  waitTime = 10000; // 10 msec

            if (buf->addr[0] == NULL) {
                CLOGW("node(%s), buf->addr[0] == NULL. so, can not fill meaningful color", m_name);
            } else {
                ExynosCameraDurationTimer drawColorTimer;

                drawColorTimer.start();

                status_t localRet = m_drawColor(buf, m_v4l2Format.fmt.pix_mp.pixelformat);
                if (localRet != NO_ERROR)
                    CLOGW("node(%s), m_drawColor() fail", m_name);

                drawColorTimer.stop();

                int drawTime = (int)drawColorTimer.durationMsecs();

                waitTime -= drawTime;
                if (waitTime < 0)
                    waitTime = 0;
            }
        } else if (strcmp(m_name, "3AA_OUTPUT") == 0 ||
                   strcmp(m_name, "FLITE") == 0) {
            waitTime = 30000; // 30msec;
        }

        if (0 < waitTime) {
            usleep(waitTime);
        }
    }

    return ret;
}

status_t ExynosCameraNode::m_putBufferQ(ExynosCameraBuffer *buf, int *qindex)
{
    Mutex::Autolock lock(m_queueBufferListLock);
    if( m_queueBufferList[*qindex].index == NODE_INIT_NEGATIVE_VALUE ) {
        m_queueBufferList[*qindex] = *buf;
    } else {
       CLOGE(" buf.index(%d) already exist!", *qindex);
       m_printBufferQ();
       return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraNode::mapBuffer(ExynosCameraBuffer *buf)
{
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

    m_mBuf(buf);

    return NO_ERROR;
}

status_t ExynosCameraNode::m_getBufferQ(ExynosCameraBuffer *buf, int *dqindex)
{
    Mutex::Autolock lock(m_queueBufferListLock);
    if( m_queueBufferList[*dqindex].index == NODE_INIT_NEGATIVE_VALUE ) {
       CLOGE(" buf.index(%d) not exist!", *dqindex);
       m_printBufferQ();
       return BAD_VALUE;
    } else {
        *buf = m_queueBufferList[*dqindex];
        m_queueBufferList[*dqindex].index = NODE_INIT_NEGATIVE_VALUE;
    }
    return NO_ERROR;
}

bool ExynosCameraNode::m_isExistBufferQ(ExynosCameraBuffer *buf)
{
    Mutex::Autolock lock(m_queueBufferListLock);

    if( m_queueBufferList[buf->index].index != NODE_INIT_NEGATIVE_VALUE) {
        return true;
    } else {
       return false;
    }
    return true;
}

bool ExynosCameraNode::m_isValidBuffer(ExynosCameraBuffer *buf)
{
    /* Check the validation of batchSize in buffer */
    int currentBatchSize = m_v4l2Format.fmt.pix_mp.reserved[0];
    int bufferBatchSize = buf->batchSize;
    if (currentBatchSize != bufferBatchSize) {
        CLOGE("Invalid batchSize! v4l2Format %d buffer %d",
                currentBatchSize, bufferBatchSize);
        CLOG_ASSERT("ASSERT(%s):Inavlid batchSize! v4l2Format %d buffer %d",
                __FUNCTION__,
                currentBatchSize, bufferBatchSize);
        return false;
    }

    return true;
}

void ExynosCameraNode::m_printBufferQ()
{
    bool empty = true;

    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        if( m_queueBufferList[i].index != NODE_INIT_NEGATIVE_VALUE ) {
            CLOGD(" index(%d) buf.index(%d)", i, m_queueBufferList[i].index);
            displayExynosBuffer(&m_queueBufferList[i]);
            empty = false;
        }
    }

    if( empty ) {
        CLOGD("no items");
        return;
    }
    return;
}

void ExynosCameraNode::m_removeItemBufferQ()
{
    List<ExynosCameraBuffer>::iterator r;
    Mutex::Autolock lock(m_queueBufferListLock);

    if (m_nodeType == NODE_TYPE_DUMMY) {
        m_dummyIndexQ.clear();
    }

#if 0
    for (r = m_queueBufferList.begin(); r != m_queueBufferList.end(); r++) {
        m_queueBufferList.erase(r);
    }
#else
    for (uint32_t i = 0; i < MAX_BUFFERS; i++) {
        m_queueBufferList[i].index = NODE_INIT_NEGATIVE_VALUE;
    }
#endif
    return;
}


int ExynosCameraNode::m_YUV_RANGE_2_V4L2_COLOR_RANGE(enum YUV_RANGE yuvRange)
{
    int v42ColorRange = -1;

    switch (yuvRange) {
    case YUV_FULL_RANGE:
        v42ColorRange = 7; /* V4L2_COLORSPACE_JPEG */
        break;
    case YUV_LIMITED_RANGE:
        v42ColorRange = 4; /* V4L2_COLORSPACE_BT878 */
        break;
    default:
        CLOGE("invalid yuvRange : %d. so, fail",
             (int)yuvRange);
        break;
    }

    return v42ColorRange;
}

enum YUV_RANGE ExynosCameraNode::m_V4L2_COLOR_RANGE_2_YUV_RANGE(int v4l2ColorRange)
{
    enum YUV_RANGE yuvRange = YUV_FULL_RANGE;

    switch (v4l2ColorRange) {
    case 7:
        yuvRange = YUV_FULL_RANGE;
        break;
    case 4:
        yuvRange = YUV_LIMITED_RANGE;
        break;
    default:
        CLOGE("invalid v4l2ColorRange : %d. so, fail",
             (int)v4l2ColorRange);
        break;
    }

    return yuvRange;
}
};
