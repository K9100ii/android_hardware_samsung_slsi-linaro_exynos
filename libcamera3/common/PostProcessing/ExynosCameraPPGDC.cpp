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
#define LOG_TAG "ExynosCameraPPGDC"

#include "ExynosCameraPPGDC.h"

//#define EXYNOS_CAMERA_GDC_DEBUG

#ifdef EXYNOS_CAMERA_GDC_DEBUG
#define EXYNOS_CAMERA_GDC_DEBUG_LOG CLOGD
#else
#define EXYNOS_CAMERA_GDC_DEBUG_LOG CLOGV
#endif

ExynosCameraPPGDC::~ExynosCameraPPGDC()
{
}

status_t ExynosCameraPPGDC::m_create(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    int outputNodeFd = -1;
    ExynosRect nullRect;

    // create your own library.
    if (m_flagValidNode(m_nodeNum) == false) {
        CLOGE("m_flagValidNode(m_nodeNum : %d) == false. so, fail", m_nodeNum);
        return INVALID_OPERATION;
    }

    ////////////////////////
    // output node
    m_node[IMAGE_POS_SRC]= new ExynosCameraNode();

    ret = m_node[IMAGE_POS_SRC]->create("GDC_OUTPUT", m_cameraId);
    if (ret != NO_ERROR) {
        CLOGE("m_node[IMAGE_POS_SRC]->create(m_name : %s) fail", "GDC_OUTPUT");
        ret = INVALID_OPERATION;
        goto done;
    }

    ret = m_node[IMAGE_POS_SRC]->open(m_nodeNum);
    if (ret != NO_ERROR) {
        CLOGE("m_node->open(%d) fail", m_nodeNum);
        ret = INVALID_OPERATION;
        goto done;
    }

    ret = m_node[IMAGE_POS_SRC]->getFd(&outputNodeFd);
    if (ret != NO_ERROR) {
        CLOGE("m_node[IMAGE_POS_SRC]->getFd() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    ////////////////////////
    // capture node
    m_node[IMAGE_POS_DST]= new ExynosCameraNode();

    ret = m_node[IMAGE_POS_DST]->create("GDC_CAPTURE", m_cameraId, outputNodeFd);
    if (ret != NO_ERROR) {
        CLOGE("m_node[IMAGE_POS_DST]->create(m_name : %s, outputNodeFd : %d) fail", "GDC_CAPTURE", outputNodeFd);
        ret = INVALID_OPERATION;
        goto done;
    }

    m_sensorBcropRect = nullRect;

done:
    funcRet |= ret;

    if (ret != NO_ERROR) {
        for (int i = 0; i < IMAGE_POS_MAX; i++) {
            ret = m_destroyNode(m_node[i]);
            funcRet |= ret;
            if (ret != NO_ERROR) {
                CLOGE("m_destroyNode(m_node[%d] fail", i);
            }

            SAFE_DELETE(m_node[i]);
        }
    }

    return funcRet;
}

status_t ExynosCameraPPGDC::m_destroy(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    // destroy your own library.
    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_resetNode(m_node[i]);
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("m_resetNode(m_node[%d]) fail", i);
        }
    }

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_destroyNode(m_node[i]);
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("m_destroyNode(m_node[%d]) fail", i);
        }

        SAFE_DELETE(m_node[i]);
    }

    return funcRet;
}

status_t ExynosCameraPPGDC::m_draw(ExynosCameraImage *srcImage,
                                   ExynosCameraImage *dstImage)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    ExynosCameraImage *ptrImage[IMAGE_POS_MAX];

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ptrImage[i] = NULL;
    }

    ptrImage[IMAGE_POS_SRC]   = &srcImage[0];
    ptrImage[IMAGE_POS_BCROP] = &srcImage[1];
    ptrImage[IMAGE_POS_DST]   = &dstImage[0];

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_setFormat(m_node[i], ptrImage[i], (enum IMAGE_POS)i);
        if (ret != NO_ERROR) {
            CLOGE("m_setFormat(m_node[%d]) fail", i);
            goto done;
        }
    }

    // set for bcrop, sensor size
    ret = m_setCtrl(m_node[IMAGE_POS_SRC], ptrImage[IMAGE_POS_BCROP]);
    if (ret != NO_ERROR) {
        CLOGE("m_setCtrl(m_node[IMAGE_POS_SRC], ptrImage[IMAGE_POS_BCROP]) fail");
        goto done;
    }

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_start(m_node[i]);
        if (ret != NO_ERROR) {
            CLOGE("m_start(m_node[%d]) fail", i);
            goto done;
        }
    }

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_putBuffer(m_node[i], ptrImage[i]);
        if (ret != NO_ERROR) {
            CLOGE("m_putBuffer(m_node[%d]) fail", i);
            goto done;
        }
    }

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_getBuffer(m_node[i], ptrImage[i]);
        if (ret != NO_ERROR) {
            CLOGE("m_getBuffer(m_node[%d]) fail", i);
            goto done;
        }
    }

done:
    funcRet |= ret;

    for (int i = 0; i < IMAGE_POS_MAX; i++) {
        ret = m_stop(m_node[i]);
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("m_stop(m_node[%d]) fail", i);
        }
    }

    return funcRet;
}

status_t ExynosCameraPPGDC::m_destroyNode(ExynosCameraNode *node)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (node == NULL)
        return funcRet;

    if (node->flagOpened() == true) {
        ret = node->close();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("node->close() fail");
        }
    }

    if (node->isCreated() == true) {
        ret = node->destroy();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("node->destroy() fail");
        }
    }

    return funcRet;
}

status_t ExynosCameraPPGDC::m_resetNode(ExynosCameraNode *node)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (node == NULL)
        return funcRet;

    if (node->isStarted() == true) {
        ret = node->stop();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("node->stop() fail");
        }
    }

    int requestBufCount = node->reqBuffersCount();
    if (0 < requestBufCount) {
        ret = node->clrBuffers();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("node->clrBuffers() fail");
        }
    }

    return funcRet;
}

status_t ExynosCameraPPGDC::m_setCtrl(ExynosCameraNode *node, ExynosCameraImage *image)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (image == NULL) {
        CLOGE("image == NULL. so, fail");
        return INVALID_OPERATION;
    }

    if (m_sensorBcropRect == image->rect) {
        EXYNOS_CAMERA_GDC_DEBUG_LOG("setExtControl(V4L2_CID_CAMERAPP_GDC_GRID_CONTROL) :  same  : [FROM] : x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            m_sensorBcropRect.x, m_sensorBcropRect.y, m_sensorBcropRect.w, m_sensorBcropRect.h, m_sensorBcropRect.fullW, m_sensorBcropRect.fullH);

        EXYNOS_CAMERA_GDC_DEBUG_LOG("setExtControl(V4L2_CID_CAMERAPP_GDC_GRID_CONTROL) :  same  : [ TO ] : x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            image->rect.x, image->rect.y, image->rect.w, image->rect.h, image->rect.fullW, image->rect.fullH);
    } else {
        EXYNOS_CAMERA_GDC_DEBUG_LOG("setExtControl(V4L2_CID_CAMERAPP_GDC_GRID_CONTROL) : change : [FROM] : x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            m_sensorBcropRect.x, m_sensorBcropRect.y, m_sensorBcropRect.w, m_sensorBcropRect.h, m_sensorBcropRect.fullW, m_sensorBcropRect.fullH);

        EXYNOS_CAMERA_GDC_DEBUG_LOG("setExtControl(V4L2_CID_CAMERAPP_GDC_GRID_CONTROL) : change : [ TO ] : x(%4d) y(%4d) w(%4d) h(%4d) fullW(%4d) fullH(%4d)",
            image->rect.x, image->rect.y, image->rect.w, image->rect.h, image->rect.fullW, image->rect.fullH);

        m_sensorBcropRect = image->rect;

        struct gdc_crop_param {
            uint32_t sensor_num;
            uint32_t sensor_width;
            uint32_t sensor_height;
            uint32_t crop_start_x;
            uint32_t crop_start_y;
            uint32_t crop_width;
            uint32_t crop_height;
            bool     is_crop_dzoom;
            bool     is_scaled;
            int      reserved[32];
        };

        struct v4l2_ext_controls extCtrls;
        memset(&extCtrls, 0x00, sizeof(extCtrls));

        struct v4l2_ext_control extCtrl;
        memset(&extCtrl, 0x00, sizeof(extCtrl));

        struct gdc_crop_param   gdcCropParam;
        memset(&gdcCropParam, 0x00, sizeof(gdcCropParam));

        extCtrls.ctrl_class = V4L2_CTRL_CLASS_CAMERA;
        extCtrls.count = 1;
        extCtrls.controls = &extCtrl;

        extCtrl.id = V4L2_CID_CAMERAPP_GDC_GRID_CONTROL;
        extCtrl.ptr = &gdcCropParam;

        gdcCropParam.sensor_num    = getSensorId(m_cameraId);
        gdcCropParam.crop_start_x  = m_sensorBcropRect.x;
        gdcCropParam.crop_start_y  = m_sensorBcropRect.y;
        gdcCropParam.crop_width    = m_sensorBcropRect.w;
        gdcCropParam.crop_height   = m_sensorBcropRect.h;
        gdcCropParam.sensor_width  = m_sensorBcropRect.fullW;
        gdcCropParam.sensor_height = m_sensorBcropRect.fullH;
        //gdcCropParam.is_crop_dzoom; // handle by driver
        //gdcCropParam.is_scaled;     // handle by driver

        ret = node->setExtControl(&extCtrls);
        if (ret != NO_ERROR) {
            CLOGE("node->setExtControl() fail");
            return INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCameraPPGDC::m_setFormat(ExynosCameraNode *node, ExynosCameraImage *image, enum IMAGE_POS nodeType)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (image == NULL) {
        CLOGE("image == NULL. so, fail");
        return INVALID_OPERATION;
    }

    ExynosRect rect = image->rect;

    bool flagSetRequest = false;
    unsigned int requestBufCount = 0;

    enum v4l2_buf_type v4l2BufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

    if (nodeType == IMAGE_POS_SRC) {
        v4l2BufType = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    } else {
        v4l2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    }

    int currentW = 0;
    int currentH = 0;
    int currentV4l2Colorformat = 0;
    int currentPlanesCount = 0;

    requestBufCount = node->reqBuffersCount();

    /* If it already set */
    if (0 < requestBufCount) {
        node->getSize(&currentW, &currentH);
        node->getColorFormat(&currentV4l2Colorformat, &currentPlanesCount);

        if (/* setSize */
            currentW               != rect.w ||
            currentH               != rect.h ||
            /* setColorFormat */
            currentV4l2Colorformat != rect.colorFormat) {
            flagSetRequest = true;

            CLOGW("Node is already requested. call clrBuffers()");
            // m_printImage("reset():[SRC]", *image, true);

            ret = m_resetNode(node);
            if (ret != NO_ERROR) {
                CLOGE("m_resetNode() fail");
                return ret;
            }
        }
    } else {
        flagSetRequest = true;
    }

    if (flagSetRequest == false) {
        CLOGV("Skip set pipeInfos nodeType(%d), setFormat(%d, %d), reqBuffers(%d)",
                (int)nodeType, rect.w, rect.h, 1);
        return ret;
    }

    if (rect.w == 0 || rect.h == 0) {
        CLOGW("Invalid size(%d x %d), skip setSize()",
            rect.w, rect.h);
        ret = INVALID_OPERATION;
        return ret;
    }

    ret = node->setSize(rect.w, rect.h);
    if (ret != NO_ERROR) {
        CLOGE("node->setSize(rect.w : %d, rect.h ; %d) fail",
            rect.w,
            rect.h);
        return ret;
    }

    if (rect.colorFormat == 0 || image->buf.planeCount == 0) {
        CLOGW("invalid colorFormat(%c%c%c%c), planeCount(%d), skip setColorFormat()",
            v4l2Format2Char(rect.colorFormat, 0),
            v4l2Format2Char(rect.colorFormat, 1),
            v4l2Format2Char(rect.colorFormat, 2),
            v4l2Format2Char(rect.colorFormat, 3),
            image->buf.planeCount);
        return ret;
    }

    ret = node->setColorFormat(rect.colorFormat, image->buf.planeCount);
    if (ret != NO_ERROR) {
        CLOGE("node->setColorFormat(colorFormat : %c%c%c%c, planeCount : %d) fail",
            v4l2Format2Char(rect.colorFormat, 0),
            v4l2Format2Char(rect.colorFormat, 1),
            v4l2Format2Char(rect.colorFormat, 2),
            v4l2Format2Char(rect.colorFormat, 3),
            image->buf.planeCount);
        return ret;
    }

    ret = node->setBufferType(VIDEO_MAX_FRAME, v4l2BufType, (enum v4l2_memory)V4L2_CAMERA_MEMORY_TYPE);
    if (ret != NO_ERROR) {
        CLOGE("node->setBufferType() fail");
        return ret;
    }

    ret = node->setFormat();
    if (ret != NO_ERROR) {
        CLOGE("node->setFormat() fail");
        return ret;
    }

    ret = node->reqBuffers();
    if (ret != NO_ERROR) {
        CLOGE("node->reqBuffers() fail");
        return ret;
    }

    return ret;
}

status_t ExynosCameraPPGDC::m_start(ExynosCameraNode *node)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (node->isStarted() == false) {
        ret = node->start();
        if (ret != NO_ERROR) {
            CLOGE("node->start() fail");
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraPPGDC::m_stop(ExynosCameraNode *node)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (node->isStarted() == true) {
        ret = node->stop();
        if (ret != NO_ERROR) {
            CLOGE("node->stop() fail");
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraPPGDC::m_putBuffer(ExynosCameraNode *node, ExynosCameraImage *image)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (image == NULL) {
        CLOGE("image == NULL. so, fail");
        return INVALID_OPERATION;
    }

    ret = node->putBuffer(&(image->buf));
    if (ret != NO_ERROR) {
        CLOGE("node->putBuffer() fail");
        return ret;
    }

    return ret;
}

status_t ExynosCameraPPGDC::m_getBuffer(ExynosCameraNode *node, ExynosCameraImage *image)
{
    status_t ret = NO_ERROR;

    if (node == NULL)
        return ret;

    if (image == NULL) {
        CLOGE("image == NULL. so, fail");
        return INVALID_OPERATION;
    }

    int dqIndex = -1;

    ret = node->getBuffer(&(image->buf), &dqIndex);
    if (ret != NO_ERROR) {
        CLOGE("node->getBuffer() fail");
        return ret;
    }

    return ret;
}

bool ExynosCameraPPGDC::m_flagValidNode(int nodeNum)
{
    if (0 < nodeNum)
        return true;
    else
        return false;
}

void ExynosCameraPPGDC::m_init(void)
{
    for (int i = 0; i < IMAGE_POS_MAX; i++)
        m_node[i] = NULL;

    m_srcImageCapacity.setNumOfImage(2);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
}
