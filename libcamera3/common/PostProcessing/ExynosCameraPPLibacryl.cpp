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
#define LOG_TAG "ExynosCameraPPLibacryl"

#include "ExynosCameraPPLibacryl.h"

ExynosCameraPPLibacryl::~ExynosCameraPPLibacryl()
{
}

status_t ExynosCameraPPLibacryl::m_create(void)
{
    status_t ret = NO_ERROR;
    int     iRet = -1;

    // create your own library.
    m_acylic = AcrylicFactory::createAcrylic("default_compositor");
    if (m_acylic == NULL) {
        CLOGE("AcrylicFactory::createAcrylic() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    m_layer  = m_acylic->createLayer();
    if (m_layer == NULL) {
        CLOGE("m_acylic->createLayer() fail");
        ret = INVALID_OPERATION;
        goto done;
    }

    iRet = m_acylic->prioritize(2);
    if (iRet < 0) {
        CLOGE("m_acylic->priortize() fail");
    }

done:
    if (ret != NO_ERROR) {
        SAFE_DELETE(m_layer);
        SAFE_DELETE(m_acylic);
    }

    return ret;
}

status_t ExynosCameraPPLibacryl::m_destroy(void)
{
    status_t ret = NO_ERROR;
    int     iRet = -1;

    iRet = m_acylic->prioritize(0);
    if (iRet < 0) {
        CLOGE("m_acylic->priortize() fail");
    }

    // destroy your own library.
    SAFE_DELETE(m_layer);
    SAFE_DELETE(m_acylic);

    return ret;
}

status_t ExynosCameraPPLibacryl::m_draw(ExynosCameraImage *srcImage,
                                        ExynosCameraImage *dstImage)
{
    status_t ret = NO_ERROR;
    bool    bRet = false;

    /* draw your own library. */

    ///////////////////////
    // src image
    int srcW = ALIGN_UP(srcImage[0].rect.fullW, GSCALER_IMG_ALIGN);
    int srcH = srcImage[0].rect.fullH;

    bRet = m_layer->setImageDimension(srcW, srcH);
    if (bRet == false) {
        CLOGE("m_layer->setImageDimension(srcW : %d, srcH : %d) fail", srcW, srcH);
        return INVALID_OPERATION;
    }

    hwc_rect_t rect;
    rect.left   = srcImage[0].rect.x;
    rect.top    = srcImage[0].rect.y;
    rect.right  = srcImage[0].rect.x + srcImage[0].rect.w;
    rect.bottom = srcImage[0].rect.y + srcImage[0].rect.h;

    uint32_t transform = 0;
    uint32_t attr = 0;

    // caller set rotation on destination.
    switch (dstImage[0].rotation) {
    case 0:
        transform = 0;
        break;
    case 90:
        transform |= HW2DCapability::TRANSFORM_ROT_90;
        break;
    case 180:
        transform |= HW2DCapability::TRANSFORM_ROT_180;
        break;
    case 270:
        transform |= HW2DCapability::TRANSFORM_ROT_270;
        break;
    default:
        CLOGE("Invalid dstImage.rotation(%d). so, fail", dstImage[0].rotation);
        return INVALID_OPERATION;
        break;
    }

    // caller set flip on destination.
    if (dstImage[0].flipH == true) {
        transform |= HW2DCapability::TRANSFORM_FLIP_H;
    }

    if (dstImage[0].flipV == true) {
        transform |= HW2DCapability::TRANSFORM_FLIP_V;
    }

    if ((srcImage[0].rect.w > dstImage[0].rect.w && srcImage[0].rect.w < dstImage[0].rect.w * 4) ||
        (srcImage[0].rect.h > dstImage[0].rect.h && srcImage[0].rect.h < dstImage[0].rect.h * 4)) {
        // when making smaller, dst line is bigger than src line by x1/4 : ex : 1/2 ~ 1/4
        attr = 0;
    } else {
        // when making smaller, dst line is smaller than src line by x1/4 : ex : 1/4 ~ 1/8
        // when make bigger or same. : ex x1 ~ x8
        attr = AcrylicLayer::ATTR_NORESAMPLING;
    }

    m_layer->setCompositArea(rect, transform, attr);
    if (bRet == false) {
        CLOGE("m_layer->setCompositArea(left : %d, top : %d, right : %d, bottom : %d, transform : %d, attr : %d) fail",
            rect.left, rect.top, rect.right, rect.bottom, transform, attr);
        return INVALID_OPERATION;
    }

    int srcColorFormat = V4L2_PIX_2_HAL_PIXEL_FORMAT(srcImage[0].rect.colorFormat);
    int srcDataSpace = HAL_DATASPACE_V0_JFIF;

    bRet = m_layer->setImageType(srcColorFormat, srcDataSpace);
    if (bRet == false) {
        CLOGE("m_layer->setImageType(srcColorFormat : %d, srcDataSpace : %d) fail",
            srcColorFormat, srcDataSpace);
        return INVALID_OPERATION;
    }

    int    fd[MAX_HW2D_PLANES] = {-1,};
    size_t len[MAX_HW2D_PLANES] = {0,};

    for (int i = 0; i < srcPlaneCount; i++) {
        fd[i]  = srcImage[0].buf.fd[i];
        len[i] = srcImage[0].buf.size[i];
    }

    int srcPlaneCount = getYuvPlaneCount(srcImage[0].rect.colorFormat);
    if (srcPlaneCount < 0) {
        CLOGE("getYuvPlaneCount(format:%c%c%c%c) fail",
            v4l2Format2Char(srcImage[0].rect.colorFormat, 0),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 1),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 2),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 3));
        return INVALID_OPERATION;
    }

    bRet = m_layer->setImageBuffer(fd, len, srcPlaneCount);
    if (bRet == false) {
        CLOGE("m_layer->setImageBuffer(fd : %d/%d/%d, len : %d/%d/%d, srcPlaneCount : %d) fail",
            fd[0],  fd[1],  fd[2],
            len[0], len[2], len[2],
            srcPlaneCount);
        return INVALID_OPERATION;
    }

    ///////////////////////
    // dst image
    int dstW = dstImage[0].rect.fullW;
    int dstH = dstImage[0].rect.fullH;
    int dstColorFormat = V4L2_PIX_2_HAL_PIXEL_FORMAT(dstImage[0].rect.colorFormat);
    int dstDataSpace = HAL_DATASPACE_V0_BT601_525;

    bRet = m_acylic->setCanvasDimension(dstW, dstH);
    if (bRet == false) {
        CLOGE("m_acylic->setCanvasDimension(dstW : %d, dstH : %d) fail", dstW, dstH);
        return INVALID_OPERATION;
    }

    bRet = m_acylic->setCanvasImageType(dstColorFormat, dstDataSpace);
    if (bRet == false) {
        CLOGE("m_acylic->setCanvasImageType(dstColorFormat : %d, dstDataSpace : %d) fail",
            dstColorFormat, dstDataSpace);
        return INVALID_OPERATION;
    }

    for (int i = 0; i < dstPlaneCount; i++) {
        fd[i]  = dstImage[0].buf.fd[i];
        len[i] = dstImage[0].buf.size[i];
    }

    int dstPlaneCount = getYuvPlaneCount(dstImage[0].rect.colorFormat);
    if (dstPlaneCount < 0) {
        CLOGE("getYuvPlaneCount(format:%c%c%c%c) fail",
            v4l2Format2Char(srcImage[0].rect.colorFormat, 0),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 1),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 2),
            v4l2Format2Char(srcImage[0].rect.colorFormat, 3));
        return INVALID_OPERATION;
    }

    bRet = m_acylic->setCanvasBuffer(fd, len, dstPlaneCount);
    if (bRet == false) {
        CLOGE("m_acylic->setCanvasImageType(fd : %d/%d/%d, len : %d/%d/%d, dstPlaneCount : %d) fail",
            fd[0],  fd[1],  fd[2],
            len[0], len[2], len[2],
            dstPlaneCount);
        return INVALID_OPERATION;
    }

    ///////////////////////
    // execute
    bRet = m_acylic->execute(NULL);
    if (bRet == false) {
        CLOGE("m_acylic->execute() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

void ExynosCameraPPLibacryl::m_init(void)
{
    m_acylic = NULL;
    m_layer  = NULL;

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420M, 32);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420, 32);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YUYV);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420M, 32);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420, 32);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YUYV);
}
