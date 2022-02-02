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
#define LOG_TAG "ExynosCameraPPLibcsc"
#include <cutils/log.h>

#include "ExynosCameraPPLibcsc.h"

#define CSC_HW_PROPERTY_DEFAULT ((CSC_HW_PROPERTY_TYPE)2) /* Not fixed mode */
#define CSC_MEMORY_TYPE         CSC_MEMORY_DMABUF /* (CSC_MEMORY_USERPTR) */

ExynosCameraPPLibcsc::~ExynosCameraPPLibcsc()
{
}

status_t ExynosCameraPPLibcsc::m_create(void)
{
    status_t ret = NO_ERROR;

    // create your own library.
    CSC_METHOD cscMethod = CSC_METHOD_HW;

    m_csc = csc_init(cscMethod);
    if (m_csc == NULL) {
        CLOGE("csc_init() fail");
        return INVALID_OPERATION;
    }

    csc_set_hw_property(m_csc, m_property, m_nodeNum);

    return ret;
}

status_t ExynosCameraPPLibcsc::m_destroy(void)
{
    status_t ret = NO_ERROR;

    // destroy your own library.
    if (m_csc != NULL)
        csc_deinit(m_csc);
    m_csc = NULL;

    return ret;
}

status_t ExynosCameraPPLibcsc::m_draw(ExynosCameraImage srcImage, ExynosCameraImage dstImage)
{
    status_t ret = NO_ERROR;

    // draw your own library.
    switch (srcImage.rect.fullH) {
    case V4L2_PIX_FMT_NV21:
        srcImage.rect.fullH = ALIGN_UP(srcImage.rect.fullH, 2);
        break;
    default:
        srcImage.rect.fullH = ALIGN_UP(srcImage.rect.fullH, GSCALER_IMG_ALIGN);
        break;
    }

    csc_set_src_format(m_csc,
        ALIGN_UP(srcImage.rect.fullW, GSCALER_IMG_ALIGN),
        srcImage.rect.fullH,
        srcImage.rect.x, srcImage.rect.y, srcImage.rect.w, srcImage.rect.h,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(srcImage.rect.colorFormat),
        0);

    csc_set_dst_format(m_csc,
        dstImage.rect.fullW, dstImage.rect.fullH,
        dstImage.rect.x, dstImage.rect.y, dstImage.rect.w, dstImage.rect.h,
        V4L2_PIX_2_HAL_PIXEL_FORMAT(dstImage.rect.colorFormat),
        0);

    csc_set_src_buffer(m_csc,
            (void **)srcImage.buf.fd, CSC_MEMORY_TYPE);

    csc_set_dst_buffer(m_csc,
            (void **)dstImage.buf.fd, CSC_MEMORY_TYPE);

    ret = csc_convert_with_rotation(m_csc, dstImage.rotation, dstImage.flipH, dstImage.flipV);
    if (ret != NO_ERROR) {
        CLOGE("csc_convert_with_rotation() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

void ExynosCameraPPLibcsc::m_setName(void)
{
    strncpy(m_name, "ExynosCameraPPLibcsc",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
}

void ExynosCameraPPLibcsc::m_init(void)
{
    m_csc = NULL;
    m_property = CSC_HW_PROPERTY_FIXED_NODE;

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420M);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_YUYV);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV12);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420M);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YVU420);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_YUYV);
}
