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
#define LOG_TAG "ExynosCameraNodeJpegHAL"

#include"ExynosCameraNodeJpegHAL.h"
#include"ExynosCameraUtils.h"

namespace android {

/* HACK */
exif_attribute_t exifInfo__;

ExynosCameraNodeJpegHAL::ExynosCameraNodeJpegHAL()
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

    m_jpegEncoder = NULL;
    m_jpegNodeLocation = NODE_LOCATION_DST;
}

ExynosCameraNodeJpegHAL::~ExynosCameraNodeJpegHAL()
{
    EXYNOS_CAMERA_NODE_IN();

    destroy();
}

status_t ExynosCameraNodeJpegHAL::create(
        const char *nodeName,
        int cameraId,
        enum EXYNOS_CAMERA_NODE_JPEG_HAL_LOCATION location,
        ExynosJpegEncoderForCamera *jpegEncoder)
{
    EXYNOS_CAMERA_NODE_IN();

    status_t ret = NO_ERROR;

    if (nodeName == NULL) {
        CLOGE("nodeName is NULL!!");
        return BAD_VALUE;
    }

    if (cameraId >= 0)
        m_cameraId = cameraId;

    if (jpegEncoder == NULL) {
        CLOGE("jpegEncoder is NULL!!");
        return BAD_VALUE;
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

    m_jpegEncoder = jpegEncoder;

    m_nodeType = NODE_TYPE_BASE;
    m_jpegNodeLocation = location;

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::open(int videoNodeNum)
{
    return open(videoNodeNum, false);
}

status_t ExynosCameraNodeJpegHAL::open(int videoNodeNum, bool useThumbnailHWFC)
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

    if (m_jpegEncoder == NULL) {
        m_jpegEncoder = new ExynosJpegEncoderForCamera(useThumbnailHWFC);
        if (m_jpegEncoder == NULL) {
            CLOGE("Cannot create ExynosJpegEncoderForCamera class");
            return INVALID_OPERATION;
        }

        ret = m_jpegEncoder->create();
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera create fail, ret(%d)",
                     ret);
            return ret;
        }

        m_jpegEncoder->EnableHWFC();
        /*
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera Enable HWFC fail, ret(%d)",
                     ret);
            m_jpegEncoder->destroy();
            return ret;
        }
        */

        CLOGD("Node(%d)(%s) opened.",
                  videoNodeNum, node_name);
    }

    m_videoNodeNum = videoNodeNum;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_OPENED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::close(void)
{
    EXYNOS_CAMERA_NODE_IN();

    CLOGD(" close");

    status_t ret = NO_ERROR;
    int maxIndex = sizeof(m_debugInfo.debugData)/sizeof(char *);

    if (m_nodeState == NODE_STATE_CREATED) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }

    for(int i = 0; i < maxIndex; i++) {
        if(m_debugInfo.debugData[i] != NULL) {
            CLOGD("delete DebugInfo[%d].", i);
            delete[] m_debugInfo.debugData[i];
        }
        m_debugInfo.debugData[i] = NULL;
        m_debugInfo.debugSize[i] = 0;
    }

    if (m_jpegEncoder != NULL) {
        if (m_jpegNodeLocation == NODE_LOCATION_SRC
            && m_videoNodeNum == FIMC_IS_VIDEO_HWFC_JPEG_NUM) {
            ret = m_jpegEncoder->destroy();
            if (ret != NO_ERROR) {
                CLOGE("close fail");
                return ret;
            }

            delete m_jpegEncoder;
        }
        m_jpegEncoder = NULL;
    }

    m_nodeType = NODE_TYPE_BASE;
    m_videoNodeNum = -1;

    m_nodeStateLock.lock();
    m_nodeState = NODE_STATE_CREATED;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::getFd(int *fd)
{
    *fd = m_fd;

    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::getJpegEncoder(ExynosJpegEncoderForCamera **jpegEncoder)
{
    *jpegEncoder = m_jpegEncoder;

    return NO_ERROR;
}

char *ExynosCameraNodeJpegHAL::getName(void)
{
    return m_name;
}

status_t ExynosCameraNodeJpegHAL::setColorFormat(int v4l2Colorformat, __unused int planeCount, __unused int batchSize, __unused enum YUV_RANGE yuvRange)
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

    if(m_jpegNodeLocation == NODE_LOCATION_SRC) {
        CLOGD("format %d(%c %c %c %c), yuvRange %d",
            v4l2Colorformat,
            (char)((v4l2Colorformat >> 0) & 0xFF),
            (char)((v4l2Colorformat >> 8) & 0xFF),
            (char)((v4l2Colorformat >> 16) & 0xFF),
            (char)((v4l2Colorformat >> 24) & 0xFF),
            m_v4l2Format.fmt.pix_mp.colorspace);

        ret = m_jpegEncoder->setColorFormat(v4l2Colorformat);
    } else {
        int jpegFormat = V4L2_PIX_FMT_JPEG_422;
        switch (v4l2Colorformat) {
        case V4L2_PIX_FMT_YUYV:
            jpegFormat = V4L2_PIX_FMT_JPEG_422;
            break;
        case V4L2_PIX_FMT_NV21:
            jpegFormat = V4L2_PIX_FMT_JPEG_420;
            break;
        default:
            CLOGE("Invalid jpegColorFormat(%d)!", v4l2Colorformat);
            m_nodeStateLock.unlock();
            return INVALID_OPERATION;
        }
        CLOGD("format %d(%c %c %c %c), yuvRange %d",
            jpegFormat,
            (char)((jpegFormat >> 0) & 0xFF),
            (char)((jpegFormat >> 8) & 0xFF),
            (char)((jpegFormat >> 16) & 0xFF),
            (char)((jpegFormat >> 24) & 0xFF),
            m_v4l2Format.fmt.pix_mp.colorspace);

        ret = m_jpegEncoder->setJpegFormat(jpegFormat);
    }

    if (ret != NO_ERROR)
        CLOGE("ExynosJpegEncoderForCamera setColorFormat fail, ret(%d)",
                 ret);

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::setQuality(int quality)
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

    CLOGD("Quality (%d)", quality);

    switch (m_videoNodeNum) {
    case FIMC_IS_VIDEO_HWFC_JPEG_NUM:
        ret = m_jpegEncoder->setQuality(quality);
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setQuality fail, ret(%d)",
                     ret);
            m_nodeStateLock.unlock();
            return ret;
        }
        break;
    case FIMC_IS_VIDEO_HWFC_THUMB_NUM:
        ret = m_jpegEncoder->setThumbnailQuality(quality);
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setThumbnailQuality fail, ret(%d)",
                     ret);
            m_nodeStateLock.unlock();
            return ret;
        }
        break;
    default:
        CLOGE("Invalid node num(%d)", ret);
        break;
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::setQuality(const unsigned char qtable[])
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

    CLOGD("setQuality(qtable[])");

    switch (m_videoNodeNum) {
    case FIMC_IS_VIDEO_HWFC_JPEG_NUM:
        ret = m_jpegEncoder->setQuality(qtable);
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setQuality fail, ret(%d)",
                     ret);
            m_nodeStateLock.unlock();
            return ret;
        }
        break;
    default:
        CLOGE("Invalid node num(%d)", ret);
        break;
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::setSize(int w, int h)
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

    switch (m_videoNodeNum) {
    case FIMC_IS_VIDEO_HWFC_JPEG_NUM:
        ret = m_jpegEncoder->setSize(w, h);
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setSize fail, ret(%d)",
                     ret);
            m_nodeStateLock.unlock();
            return ret;
        }
        break;
    case FIMC_IS_VIDEO_HWFC_THUMB_NUM:
        ret = m_jpegEncoder->setThumbnailSize(w, h);
        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setThumbnailSize fail, ret(%d)",
                     ret);
            m_nodeStateLock.unlock();
            return ret;
        }
        break;
    default:
        CLOGE("Invalid node num(%d)", ret);
        break;
    }

    m_nodeState = NODE_STATE_IN_PREPARE;
    m_nodeStateLock.unlock();

    EXYNOS_CAMERA_NODE_OUT();

    return ret;
}

status_t ExynosCameraNodeJpegHAL::reqBuffers(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::clrBuffers(void)
{
    m_jpegEncoder->destroy();
    return NO_ERROR;
}

unsigned int ExynosCameraNodeJpegHAL::reqBuffersCount(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setControl(__unused unsigned int id, __unused int value)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::getControl(__unused unsigned int id, __unused int *value)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::polling(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setInput(int sensorId)
{
    m_sensorId = sensorId;

    return NO_ERROR;
}

int ExynosCameraNodeJpegHAL::getInput(void)
{
    return m_sensorId;
}

int ExynosCameraNodeJpegHAL::resetInput(void)
{
    m_sensorId = 0;

    return NO_ERROR;
}

int ExynosCameraNodeJpegHAL::getNodeNum(void)
{
    return m_videoNodeNum;
}

status_t ExynosCameraNodeJpegHAL::setFormat(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setFormat(__unused unsigned int bytesPerPlane[])
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setCrop(__unused enum v4l2_buf_type type,
                                          __unused int x, __unused int y, __unused int w, __unused int h)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setExifInfo(exif_attribute_t *exifInfo)
{
    //memcpy(&m_exifInfo, exifInfo, sizeof(exif_attribute_t));
    memcpy(&exifInfo__, exifInfo, sizeof(exif_attribute_t));

    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::setDebugInfo(debug_attribute_t *debugInfo)
{
    /* Num of AppMarker */
    m_debugInfo.num_of_appmarker = debugInfo->num_of_appmarker;

    for(int i = 0; i < debugInfo->num_of_appmarker; i++) {
        int app_marker_index = debugInfo->idx[i][0];

        /* App Marker Index */
        m_debugInfo.idx[i][0] = app_marker_index;

        if(debugInfo->debugSize[app_marker_index] != 0) {
            if (!m_debugInfo.debugData[app_marker_index]) {
                CLOGD("Alloc DebugInfo Buffer(%d)",
                    debugInfo->debugSize[app_marker_index]);
                m_debugInfo.debugData[app_marker_index] = new char[debugInfo->debugSize[app_marker_index]+1];
            }

            /* Data */
            memset((void *)m_debugInfo.debugData[app_marker_index], 0, debugInfo->debugSize[app_marker_index]);
            memcpy((void *)(m_debugInfo.debugData[app_marker_index]),
                    (void *)(debugInfo->debugData[app_marker_index]),
                    debugInfo->debugSize[app_marker_index]);
            /* Size */
            m_debugInfo.debugSize[app_marker_index] = debugInfo->debugSize[app_marker_index];
        }
    }
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::start(void)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::stop(void)
{
    return NO_ERROR;
}

bool ExynosCameraNodeJpegHAL::isCreated(void)
{
    return m_flagCreate;
}

bool ExynosCameraNodeJpegHAL::isStarted(void)
{
    return m_flagStart;
}

status_t ExynosCameraNodeJpegHAL::prepareBuffer(__unused ExynosCameraBuffer *buf)
{
    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::putBuffer(ExynosCameraBuffer *buf)
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

    if(m_jpegNodeLocation == NODE_LOCATION_SRC) {
        switch (m_videoNodeNum) {
        case FIMC_IS_VIDEO_HWFC_JPEG_NUM:
            ret = m_jpegEncoder->setInBuf((int *)&(buf->fd), (int *)buf->size);
            break;
        case FIMC_IS_VIDEO_HWFC_THUMB_NUM:
            ret = m_jpegEncoder->setInBuf2((int *)&(buf->fd), (int *)buf->size);
            break;
        default:
            CLOGE("Invalid node num(%d)", ret);
            break;
        }

        if (ret != NO_ERROR) {
            CLOGE("ExynosJpegEncoderForCamera setInBuffer fail, ret(%d)",
                     ret);
            return ret;
        }
        m_srcBuffer = *buf;
    } else {
        switch (m_videoNodeNum) {
        case FIMC_IS_VIDEO_HWFC_JPEG_NUM:
            ret = m_jpegEncoder->setOutBuf(buf->fd[0], buf->size[0] + buf->size[1] + buf->size[2]);
            if (ret != NO_ERROR) {
                CLOGE("ExynosJpegEncoderForCamera setOutBuffer fail. ret(%d)",
                         ret);
                return ret;
            }

            ret = m_jpegEncoder->updateConfig();
            if (ret != NO_ERROR) {
                CLOGE("ExynosJpegEncoderForCamera updateConfig fail. ret(%d)",
                         ret);
                return ret;
            }

            CLOGI("Start encode. bufferIndex %d",
                     buf->index);
            /* HACK */
            /* ret = m_jpegEncoder->encode((int *)&buf->size, &m_exifInfo, (char **)buf->addr, &m_debugInfo); */
            ret = m_jpegEncoder->encode((int *)&buf->size, &exifInfo__, (char **)buf->addr, &m_debugInfo);
            if (ret != NO_ERROR) {
                CLOGE("ExynosJpegEncoderForCamera encode fail. ret(%d)",
                         ret);
                return ret;
            }
            break;
        case FIMC_IS_VIDEO_HWFC_THUMB_NUM:
            break;
        default:
            CLOGE("Invalid node num(%d)", ret);
            break;
        }

        m_dstBuffer = *buf;
    }

    return NO_ERROR;
}

status_t ExynosCameraNodeJpegHAL::getBuffer(ExynosCameraBuffer *buf, int *dqIndex)
{
    status_t ret = NO_ERROR;

#if 0
    if (m_nodeState != NODE_STATE_RUNNING) {
        CLOGE("m_nodeState = [%d] is not valid",
             (int)m_nodeState);
        return INVALID_OPERATION;
    }
#endif

    if(m_jpegNodeLocation == NODE_LOCATION_SRC) {
        *buf = m_srcBuffer;
        *dqIndex = m_srcBuffer.index;
    } else {
        if (m_videoNodeNum == FIMC_IS_VIDEO_HWFC_JPEG_NUM) {
            ssize_t jpegSize = -1;
            /* Blocking function.
             * This function call is returned when JPEG encoding operation is finished.
             */
            CLOGI("WaitForCompression. bufferIndex %d",
                     m_dstBuffer.index);
            jpegSize = m_jpegEncoder->WaitForCompression();
            if (jpegSize < 0) {
                CLOGE("Failed to JPEG Encoding. bufferIndex %d jpegSize %ld",
                         m_dstBuffer.index, (long)jpegSize);
                ret = INVALID_OPERATION;
            }
            m_dstBuffer.size[0] = (unsigned int)jpegSize;
        }
        *buf = m_dstBuffer;
        *dqIndex = m_dstBuffer.index;
    }

    return ret;
}

void ExynosCameraNodeJpegHAL::dump(void)
{
    dumpState();
    dumpQueue();

    return;
}

void ExynosCameraNodeJpegHAL::dumpState(void)
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

void ExynosCameraNodeJpegHAL::dumpQueue(void)
{
    return;
}

};
