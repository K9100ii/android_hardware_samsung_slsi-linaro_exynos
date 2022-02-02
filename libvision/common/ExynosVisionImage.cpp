/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

#define LOG_TAG "ExynosVisionImage"
#include <cutils/log.h>

#include <stdlib.h>

#include <VX/vx.h>
#include <VX/vx_khr_opencl.h>

#include "ExynosVisionGraph.h"

#include "ExynosVisionImage.h"
#include "ExynosVisionDelay.h"

namespace android {

static vx_bool isSupportedFourcc(vx_df_image code)
{
    switch (code)
    {
        case VX_DF_IMAGE_RGB:
        case VX_DF_IMAGE_RGBX:
        case VX_DF_IMAGE_BGR:
        case VX_DF_IMAGE_BGRX:
        case VX_DF_IMAGE_NV12:
        case VX_DF_IMAGE_NV21:
        case VX_DF_IMAGE_UYVY:
        case VX_DF_IMAGE_YUYV:
        case VX_DF_IMAGE_IYUV:
        case VX_DF_IMAGE_YUV4:
        case VX_DF_IMAGE_U8:
        case VX_DF_IMAGE_U16:
        case VX_DF_IMAGE_S16:
        case VX_DF_IMAGE_U32:
        case VX_DF_IMAGE_S32:
        case VX_DF_IMAGE_F32:
        case VX_DF_IMAGE_VIRT:
            return vx_true_e;
        default:
            VXLOGE("format 0x%08x is not supported\n", code);
            return vx_false_e;
    }
}

static vx_size vxSizeOfChannel(vx_df_image color)
{
    vx_size size = 0ul;
    if (isSupportedFourcc(color))
    {
        switch (color)
        {
            case VX_DF_IMAGE_S16:
            case VX_DF_IMAGE_U16:
                size = sizeof(vx_uint16);
                break;
            case VX_DF_IMAGE_U32:
            case VX_DF_IMAGE_S32:
            case VX_DF_IMAGE_F32:
                size = sizeof(vx_uint32);
                break;
            default:
                size = 1ul;
                break;
        }
    }
    return size;
}

static vx_bool vxIsOdd(vx_uint32 a)
{
    if (a & 0x1)
        return vx_true_e;
    else
        return vx_false_e;
}

static vx_bool vxIsValidDimensions(vx_uint32 width, vx_uint32 height, vx_df_image color)
{
    if (vxIsOdd(width) && (color == VX_DF_IMAGE_UYVY || color == VX_DF_IMAGE_YUYV))
    {
        return vx_false_e;
    }
    else if ((vxIsOdd(width) || vxIsOdd(height)) &&
              (color == VX_DF_IMAGE_IYUV || color == VX_DF_IMAGE_NV12 || color == VX_DF_IMAGE_NV21))
    {
        return vx_false_e;
    }
    return vx_true_e;
}

static vx_uint32 vxComputePatchOffset(vx_uint32 x, vx_uint32 y, const vx_imagepatch_addressing_t *addr)
{
    return ((addr->stride_y * ((addr->scale_y * y)/VX_SCALE_UNITY)) +
             (addr->stride_x * ((addr->scale_x * x)/VX_SCALE_UNITY)));
}

static vx_uint32 vxComputePatchRangeSize(vx_uint32 range, const vx_imagepatch_addressing_t *addr)
{
    return (range * addr->stride_x * addr->scale_x) / VX_SCALE_UNITY;
}

vx_status
ExynosVisionImage::checkValidCreateImage(vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    vx_status status = VX_SUCCESS;

    if ((width == 0) || (height == 0) ||
        (isSupportedFourcc(format) == vx_false_e) || (format == VX_DF_IMAGE_VIRT))
        status = VX_ERROR_INVALID_PARAMETERS;

    return status;
}

vx_uint32
ExynosVisionImage::computePlaneOffset(vx_uint32 x, vx_uint32 y, vx_uint32 p)
{
    vx_uint32 mem_index = m_plane_loc[p].memory_index;
    vx_uint32 subplane_index = m_plane_loc[p].subplane_index;

    /* parameter is a logical coordinate, not memory coordinate. For example, all planes of NV12 have a same logical coordinate range. */
    return  (((y * m_memory.strides[mem_index][subplane_index][VX_DIM_Y]) / m_scale[p][VX_DIM_Y]) +
              ((x * m_memory.strides[mem_index][subplane_index][VX_DIM_X]) / m_scale[p][VX_DIM_X]));
}

vx_uint32
ExynosVisionImage::computePlaneRangeSize(vx_uint32 range, vx_uint32 p)
{
    vx_uint32 mem_index = m_plane_loc[p].memory_index;
    vx_uint32 subplane_index = m_plane_loc[p].subplane_index;

    return (range * m_memory.strides[mem_index][subplane_index][VX_DIM_X]) / m_scale[p][VX_DIM_X];
}

void*
ExynosVisionImage::formatImagePatchAddress1d(void *ptr, vx_uint32 index, const vx_imagepatch_addressing_t *addr)
{
    vx_uint8 *new_ptr = NULL;
    if (ptr && index < addr->dim_x*addr->dim_y) {
        vx_uint32 x = index % addr->dim_x;
        vx_uint32 y = index / addr->dim_x;
        vx_uint32 offset = vxComputePatchOffset(x, y, addr);
        new_ptr = (vx_uint8 *)ptr;
        new_ptr = &new_ptr[offset];
    }
    return new_ptr;
}

void*
ExynosVisionImage::formatImagePatchAddress2d(void *ptr, vx_uint32 x, vx_uint32 y, const vx_imagepatch_addressing_t *addr)
{
    vx_uint8 *new_ptr = NULL;
    if (ptr && x < addr->dim_x && y < addr->dim_y) {
        vx_uint32 offset = vxComputePatchOffset(x, y, addr);
        new_ptr = (vx_uint8 *)ptr;
        new_ptr = &new_ptr[offset];
    }
    return new_ptr;
}

vx_bool
ExynosVisionImage::checkImageDependenct(ExynosVisionImage *image1, ExynosVisionImage *image2)
{
    vx_rectangle_t rr, rw;
    memset(&rr, 0x0, sizeof(vx_rectangle_t));
    memset(&rw, 0x0, sizeof(vx_rectangle_t));
    ExynosVisionImage *imgr = image1->locateROI(&rr);
    ExynosVisionImage *imgw = image2->locateROI(&rw);
    if (imgr == imgw) {
        /* check for ROI intersection */
        if (rr.start_x < rw.end_x && rr.end_x > rw.start_x && rr.start_y < rw.end_y && rr.end_y > rw.start_y) {
            return vx_true_e;
        }
    }

    return vx_false_e;
}

void
ExynosVisionImage::initMemory(vx_uint32 plane_index,
                                                 vx_uint32 mem_index,
                                                 vx_uint32 subplane_index,
                                                 vx_uint32 channels,
                                                 vx_uint32 width,
                                                 vx_uint32 height)
{
    m_plane_loc[plane_index].memory_index = mem_index;
    m_plane_loc[plane_index].subplane_index= subplane_index;

    m_memory.dims[mem_index][subplane_index][VX_DIM_C] = channels;
    m_memory.dims[mem_index][subplane_index][VX_DIM_X] = width;
    m_memory.dims[mem_index][subplane_index][VX_DIM_Y] = height;
}

void
ExynosVisionImage::initPlane(vx_uint32 index,
                 vx_uint32 channels,
                 vx_uint32 coordinate_scale,
                 vx_uint32 width,
                 vx_uint32 height)
{
    m_scale[index][VX_DIM_C] = 1;
    m_scale[index][VX_DIM_X] = coordinate_scale;
    m_scale[index][VX_DIM_Y] = coordinate_scale;
    m_bounds[index][VX_DIM_C][VX_BOUND_START] = 0;
    m_bounds[index][VX_DIM_C][VX_BOUND_END] = channels;
    m_bounds[index][VX_DIM_X][VX_BOUND_START] = 0;
    m_bounds[index][VX_DIM_X][VX_BOUND_END] = width;
    m_bounds[index][VX_DIM_Y][VX_BOUND_START] = 0;
    m_bounds[index][VX_DIM_Y][VX_BOUND_END] = height;
}

void
ExynosVisionImage::initImage(vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    vx_uint32 element_size = (vx_uint32)vxSizeOfChannel(format);
    m_width = width;
    m_height = height;
    m_format = format;
    m_range = VX_CHANNEL_RANGE_FULL;
    m_import_type = VX_IMPORT_TYPE_NONE;
    /* when an image is allocated, it's not valid until it's been written to.
     * this inverted rectangle is needed for the initial write case.
     */
    m_region.start_x = width;
    m_region.start_y = height;
    m_region.end_x = 0;
    m_region.end_y = 0;
    switch (format)
    {
        case VX_DF_IMAGE_U8:
        case VX_DF_IMAGE_U16:
        case VX_DF_IMAGE_U32:
        case VX_DF_IMAGE_S16:
        case VX_DF_IMAGE_S32:
        case VX_DF_IMAGE_F32:
            m_space = VX_COLOR_SPACE_NONE;
            break;
        default:
            m_space = VX_COLOR_SPACE_DEFAULT;
            break;
    }

    m_memory.element_byte_size = element_size;

    switch (m_format) {
    case VX_DF_IMAGE_VIRT:
        break;
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        m_planes = 2;
        initPlane(VX_PLANE_0, 1, 1, m_width, m_height);
        initPlane(VX_PLANE_1, 2, 2, m_width, m_height);

        if (m_continuous) {
            m_memory.memory_num = 1;
            m_memory.subplane_num[VX_MEM_0] = 2;

            initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(VX_PLANE_1, VX_MEM_0, 1, 2, m_width/2, m_height/2);
        } else {
            m_memory.memory_num = 2;
            m_memory.subplane_num[VX_MEM_0] = 1;
            m_memory.subplane_num[VX_MEM_1] = 1;

            initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(VX_PLANE_1, VX_MEM_1, 0, 2, m_width/2, m_height/2);
        }
        break;
    case VX_DF_IMAGE_RGB:
    case VX_DF_IMAGE_BGR:
        m_planes = 1;
        initPlane(VX_PLANE_0, 3, 1, m_width, m_height);

        m_memory.memory_num = 1;
        m_memory.subplane_num[VX_MEM_0] = 1;
        initMemory(VX_PLANE_0, VX_MEM_0, 0, 3, m_width, m_height);
        break;
    case VX_DF_IMAGE_RGBX:
    case VX_DF_IMAGE_BGRX:
        m_planes = 1;
        initPlane(VX_PLANE_0, 4, 1, m_width, m_height);

        m_memory.memory_num = 1;
        m_memory.subplane_num[VX_MEM_0] = 1;
        initMemory(VX_PLANE_0, VX_MEM_0, 0, 4, m_width, m_height);
        break;
    case VX_DF_IMAGE_UYVY:
    case VX_DF_IMAGE_YUYV:
        m_planes = 1;
        initPlane(VX_PLANE_0, 2, 1, m_width, m_height);

        m_memory.memory_num = 1;
        m_memory.subplane_num[VX_MEM_0] = 1;
        initMemory(VX_PLANE_0, VX_MEM_0, 0, 2, m_width, m_height);
        break;
    case VX_DF_IMAGE_YUV4:
        m_planes = 3;
        initPlane(VX_PLANE_0, 1, 1, m_width, m_height);
        initPlane(VX_PLANE_1, 1, 1, m_width, m_height);
        initPlane(VX_PLANE_2, 1, 1, m_width, m_height);

        if (m_continuous) {
            m_memory.memory_num = 1;
            m_memory.subplane_num[VX_MEM_0] = 3;

            initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(VX_PLANE_1, VX_MEM_0, 1, 1, m_width, m_height);
            initMemory(VX_PLANE_2, VX_MEM_0, 2, 1, m_width, m_height);
        } else {
            m_memory.memory_num = 3;
            m_memory.subplane_num[VX_MEM_0] = 1;
            m_memory.subplane_num[VX_MEM_1] = 1;
            m_memory.subplane_num[VX_MEM_2] = 1;

            initMemory(0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(1, VX_MEM_1, 0, 1, m_width, m_height);
            initMemory(2, VX_MEM_2, 0, 1, m_width, m_height);
        }
        break;
    case VX_DF_IMAGE_IYUV:
        m_planes = 3;
        initPlane(VX_PLANE_0, 1, 1, m_width, m_height);
        initPlane(VX_PLANE_1, 1, 2, m_width, m_height);
        initPlane(VX_PLANE_2, 1, 2, m_width, m_height);

        if (m_continuous) {
            m_memory.memory_num = 1;
            m_memory.subplane_num[VX_MEM_0] = 3;

            initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(VX_PLANE_1, VX_MEM_0, 1, 1, m_width/2, m_height/2);
            initMemory(VX_PLANE_2, VX_MEM_0, 2, 1, m_width/2, m_height/2);
        } else {
            m_memory.memory_num = 3;
            m_memory.subplane_num[VX_MEM_0] = 1;
            m_memory.subplane_num[VX_MEM_1] = 1;
            m_memory.subplane_num[VX_MEM_2] = 1;

            initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
            initMemory(VX_PLANE_1, VX_MEM_1, 0, 1, m_width/2, m_height/2);
            initMemory(VX_PLANE_2, VX_MEM_2, 0, 1, m_width/2, m_height/2);
        }
        break;
    case VX_DF_IMAGE_U8:
    case VX_DF_IMAGE_U16:
    case VX_DF_IMAGE_S16:
    case VX_DF_IMAGE_U32:
    case VX_DF_IMAGE_S32:
    case VX_DF_IMAGE_F32:
        m_planes = 1;
        initPlane(VX_PLANE_0, 1, 1, m_width, m_height);

        m_memory.memory_num = 1;
        m_memory.subplane_num[VX_MEM_0] = 1;
        initMemory(VX_PLANE_0, VX_MEM_0, 0, 1, m_width, m_height);
        break;
    default:
        /*! should not get here unless there's a bug in the
         * vxIsSupportedFourcc call.
         */
        VXLOGE("Unsupported IMAGE FORMAT!!!");
        break;
    }
}

ExynosVisionImage::ExynosVisionImage(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual)
                                                                        : ExynosVisionDataReference(context, VX_TYPE_IMAGE, scope, vx_true_e, is_virtual)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_width = 0;
    m_height = 0;
    m_format = 0;
    m_planes = 0;
    m_continuous = vx_false_e;
    m_space = 0;
    m_range = 0;

    memset(&m_plane_loc, 0x0, sizeof(m_plane_loc));
    memset(&m_scale, 0x0, sizeof(m_scale));
    memset(&m_bounds, 0x0, sizeof(m_bounds));

    m_constant = vx_false_e;

    m_region.start_x = ~(0);
    m_region.start_y = ~(0);
    m_region.end_x = 0;
    m_region.end_y = 0;

    m_import_type = VX_IMPORT_TYPE_NONE;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionImage::~ExynosVisionImage()
{
    /* do nothing */
}

void
ExynosVisionImage::operator=(const ExynosVisionImage& src_image)
{
    m_width = src_image.m_width;
    m_height = src_image.m_height;
    m_format = src_image.m_format;
    m_planes = src_image.m_planes;
    m_continuous = src_image.m_continuous;
    m_space = src_image.m_space;
    m_range = src_image.m_range;

    m_import_type = src_image.m_import_type;

    memcpy(&m_plane_loc, &src_image.m_plane_loc, sizeof(m_plane_loc));
    memcpy(&m_scale, &src_image.m_scale, sizeof(m_scale));
    memcpy(&m_bounds, &src_image.m_bounds, sizeof(m_bounds));

    m_memory.memory_num = src_image.m_memory.memory_num;
    memcpy(&m_memory.subplane_num, &src_image.m_memory.subplane_num, sizeof(m_memory.subplane_num));
    memcpy(&m_memory.dims, &src_image.m_memory.dims, sizeof(m_memory.dims));
    m_memory.element_byte_size = src_image.m_memory.element_byte_size;
}

vx_status
ExynosVisionImage::init(vx_uint32 width, vx_uint32 height, vx_df_image color)
{
    if (isSupportedFourcc(color) == vx_true_e) {
        if (vxIsValidDimensions(width, height, color) == vx_true_e) {
            initImage(width, height, color);
        } else {
            VXLOGE("Requested Image Dimensions are invalid!");
            getContext()->addLogEntry(this, VX_ERROR_INVALID_DIMENSION, "Requested Image Dimensions was invalid!\n");
            return VX_ERROR_INVALID_DIMENSION;
        }
    } else {
        VXLOGE("Requested Image Format was invalid!");
        getContext()->addLogEntry(this, VX_ERROR_INVALID_FORMAT, "Requested Image Format was invalid!\n");
        return VX_ERROR_INVALID_FORMAT;
    }

    return VX_SUCCESS;
}

vx_status
ExynosVisionImage::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;
    status = freeMemory_T<image_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionImage::queryImage(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_IMAGE_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3))
            *(vx_df_image *)ptr = m_format;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_width;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_height;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_PLANES:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_planes;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_SPACE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_space;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_RANGE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_range;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            vx_size size = 0ul;
            vx_uint32 m,p;
            for (m = 0; m < m_memory.memory_num; m++) {
                for (p = 0; p < m_memory.subplane_num[m]; p++) {
                    size += (abs(m_memory.strides[m][p][VX_DIM_Y]) * m_memory.dims[m][p][VX_DIM_Y]);
                }
            }
            *(vx_size *)ptr = size;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_IMAGE_ATTRIBUTE_IMPORT:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_import_type;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_MEMORIES:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_memory.memory_num;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionImage::setImageAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_IMAGE_ATTRIBUTE_SPACE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            m_space = *(vx_enum *)ptr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_IMAGE_ATTRIBUTE_RANGE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            m_range = *(vx_enum *)ptr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionImage::setDirective(vx_enum directive)
{
    vx_status status = VX_SUCCESS;

    switch (directive) {
    /*! \todo add directive to the sample implementation */
    case VX_DIRECTIVE_IMAGE_CONTINUOUS:
        if (m_is_allocated == vx_true_e) {
            VXLOGE("the memory of %s is already allocated", getName());
            status = VX_FAILURE;
        } else {
            m_continuous = vx_true_e;
            initImage(m_width, m_height, m_format);
            status = VX_SUCCESS;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionImage::swapImageHandle(void *const new_ptrs[], void *prev_ptrs[], vx_size num_planes)
{
    VXLOGE("%s can't support this, %p, %p, %d", getName(), new_ptrs, prev_ptrs, num_planes);
    displayInfo(0, vx_true_e);

    return VX_ERROR_INVALID_PARAMETERS;
}

ExynosVisionImage*
ExynosVisionImage::locateROI(vx_rectangle_t *rect)
{
    if ((rect->start_x == 0) && (rect->start_y == 0) &&
        (rect->end_x == 0) && (rect->end_y == 0)) {
        rect->end_x = m_width;
        rect->end_y = m_height;
    }

    if ((m_width < rect->end_x) || (m_height < rect->end_y)) {
        VXLOGE("the locate of ROI is out of bound, %dx%d -> %dx%d", rect->end_x, rect->end_y, m_width, m_height);
        return NULL;
    }

    return this;
}

vx_status
ExynosVisionImage::verifyMeta(ExynosVisionMeta *meta)
{
    vx_status status = VX_SUCCESS;
    vx_df_image meta_image_format;
    vx_uint32 meta_image_width;
    vx_uint32 meta_image_height;
    status |= meta->getMetaFormatAttribute(VX_IMAGE_ATTRIBUTE_FORMAT, &meta_image_format, sizeof(meta_image_format));
    status |= meta->getMetaFormatAttribute(VX_IMAGE_ATTRIBUTE_WIDTH, &meta_image_width, sizeof(meta_image_width));
    status |= meta->getMetaFormatAttribute(VX_IMAGE_ATTRIBUTE_HEIGHT, &meta_image_height, sizeof(meta_image_height));

    if (status != VX_SUCCESS) {
        VXLOGE("get meta fail, err:%d", status);
    }

    if (isVirtual()) {
        if (status != VX_SUCCESS) {
            VXLOGE("verify meta image fail");
        }

        if (m_format == VX_DF_IMAGE_VIRT || m_format == meta_image_format)
        {
            VXLOGD3("meta_image_format:0x%x", meta_image_format);

            /* we have to go set all the other dimensional information up. */
            initImage(meta_image_width, meta_image_height, meta_image_format);
        }
        else
        {
            status = VX_ERROR_INVALID_FORMAT;
            getContext()->addLogEntry(this, status, "invalid format %08x!\n", m_format);
            VXLOGE("invalid format 0x%X", m_format);
        }
    } else {
        /* check the data that came back from the output validator against the object */
        if ((m_width != meta_image_width) ||
            (m_height != meta_image_height)) {
            status = VX_ERROR_INVALID_DIMENSION;
            getContext()->addLogEntry(this, status, "invalid dimension %ux%u, meta %ux%u\n", m_width, m_height, meta_image_width, meta_image_height);
            VXLOGE("invalid dimension %ux%u, meta %ux%u", m_width, m_height, meta_image_width, meta_image_height);
        }
        if (m_format != meta_image_format) {
            status = VX_ERROR_INVALID_FORMAT;
            getContext()->addLogEntry(this, status, "invalid format: %p, meta: %p\n", m_format, meta_image_format);
            VXLOGE("invalid format: %p, meta: %p", m_format, meta_image_format);
        }
    }

    return status;
}

vx_status
ExynosVisionImage::accessImagePatch(const vx_rectangle_t *rect,
                                                                    vx_uint32 plane_index,
                                                                    vx_imagepatch_addressing_t *addr,
                                                                    void **ptr,
                                                                    vx_enum usage)
{
    vx_uint8 *p = NULL;
    vx_status status = VX_FAILURE;
    vx_bool mapped = vx_false_e;
    vx_uint32 start_x = rect ? rect->start_x : 0u;
    vx_uint32 start_y = rect ? rect->start_y : 0u;
    vx_uint32 end_x = rect ? rect->end_x : 0u;
    vx_uint32 end_y = rect ? rect->end_y : 0u;
    vx_bool zero_area = ((((end_x - start_x) == 0) || ((end_y - start_y) == 0)) ? vx_true_e : vx_false_e);

    vx_uint32 m = m_plane_loc[plane_index].memory_index;
    vx_uint32 sp = m_plane_loc[plane_index].subplane_index;

    /* bad parameters */
    if ((usage < VX_READ_ONLY) || (VX_READ_AND_WRITE < usage) ||
        (addr == NULL) || (ptr == NULL)) {
        VXLOGE("usage and ptr is invalid");
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    /* bad references */
    if (!rect) {
        status = VX_ERROR_INVALID_REFERENCE;
        goto EXIT;
    }

    /* determine if virtual before checking for memory */
    if (m_is_virtual == vx_true_e) {
        if (m_kernel_count == 0) {
            /* User tried to access a "virtual" image. */
            VXLOGE("Can not access a virtual image");
            status = VX_ERROR_OPTIMIZED_AWAY;
            goto EXIT;
        }
        /* framework trying to access a virtual image, this is ok. */
    }

    /* more bad parameters */
    if (zero_area == vx_false_e &&
         ((plane_index >= m_planes) ||
         (rect->start_x >= rect->end_x) ||
         (rect->start_y >= rect->end_y))) {
        VXLOGE("rect info is invalid");
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if ((m_continuous == vx_false_e) && (plane_index >= (vx_uint32)abs(m_memory.memory_num))) {
        VXLOGE("plane index is wrong");
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    /* verify has not been call yet or will not be called (immediate mode)
     * this is a trick to "touch" an image so that it can be created
     */
    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    /* can't write to constant */
    if ((m_constant == vx_true_e) && (usage != VX_READ_ONLY)) {
        status = VX_ERROR_NOT_SUPPORTED;
        VXLOGE("Can't write to constant data, only read");
        getContext()->addLogEntry(this, status, "Can't write to constant data, only read!\n");
        goto EXIT;
    }

    vx_uint8 *res_base_ptr;
    res_base_ptr = (vx_uint8 *)(m_cur_res->itemAt(m)->getAddr());
    if (res_base_ptr == NULL) {
        VXLOGE("resource is null");
        status = VX_ERROR_NO_MEMORY;
        goto EXIT;
    }

    /*************************************************************************/
    VXLOGD3("accessImagePatch from %s to ptr %p from {%u,%u} to {%u,%u} plane %u",
            getName(), *ptr, rect->start_x, rect->start_y, rect->end_x, rect->end_y, plane_index);

    if (increaseAccessCount() == 1) {
        if (m_external_lock.tryLock() != NO_ERROR) {
            VXLOGE("%s is locking", getName());
            if (decreaseAccessCount() == 0) {
                m_external_lock.unlock();
            }
            status = VX_FAILURE;
            goto EXIT;
        }
    }
    m_memory.usage[plane_index] = usage;

    /* POSSIBILITIES:
     * 1.) !*ptr && RO == MAP
     * 2.) !*ptr && WO == MAP
     * 3.) !*ptr && RW == MAP
     * 4.)  *ptr && RO||RW == COPY (UNLESS MAP)
     */
    if (*ptr == NULL) {
        mapped = vx_true_e;
    }

    /* lock the memory plane for multiple writers*/
    if (usage != VX_READ_ONLY) {
        m_memory_locks[plane_index].lock();
    }

    /* MAP mode */
    if (mapped == vx_true_e) {
        m_memory.access_mode[plane_index] = ACCESS_MAP_MODE;

        vx_uint32 index;

        p = (vx_uint8 *)(res_base_ptr + m_memory.subplane_mem_offset[m][sp]);

        /* use the addressing of the internal format */
        addr->dim_x = rect->end_x - rect->start_x;
        addr->dim_y = rect->end_y - rect->start_y;
        addr->stride_x = m_memory.strides[m][sp][VX_DIM_X];
        addr->stride_y = m_memory.strides[m][sp][VX_DIM_Y];
        addr->step_x = m_scale[plane_index][VX_DIM_X];
        addr->step_y = m_scale[plane_index][VX_DIM_Y];
        addr->scale_x = VX_SCALE_UNITY / m_scale[plane_index][VX_DIM_X];
        addr->scale_y = VX_SCALE_UNITY / m_scale[plane_index][VX_DIM_Y];

        VXLOGD3("%s, format:0x%x, plane:%d", getName(), m_format, plane_index);
        VXLOGD3("strides[%d][%d][X]:%d, strides[%d][%d][Y]:%d", m , sp, m_memory.strides[m][sp][VX_DIM_X], m, sp, m_memory.strides[m][sp][VX_DIM_Y]);

        index = vxComputePatchOffset(rect->start_x, rect->start_y, addr);
        *ptr = &p[index];

        status = VX_SUCCESS;
    } else {
        /* COPY mode */
        m_memory.access_mode[plane_index] = ACCESS_COPY_MODE;

        /* stride is already filled by application */
        addr->dim_x = rect->end_x - rect->start_x;
        addr->dim_y = rect->end_y - rect->start_y;
        addr->step_x = m_scale[plane_index][VX_DIM_X];
        addr->step_y = m_scale[plane_index][VX_DIM_Y];
        addr->scale_x = VX_SCALE_UNITY / m_scale[plane_index][VX_DIM_X];
        addr->scale_y = VX_SCALE_UNITY / m_scale[plane_index][VX_DIM_Y];

        vx_uint32 x, y;
        vx_uint8 *external_ptr = (vx_uint8*)*ptr;

        if ((usage == VX_READ_ONLY) || (usage == VX_READ_AND_WRITE)) {
            if ((vx_uint32)addr->stride_x == m_memory.strides[m][sp][VX_DIM_X]) {
                /* Both have compact lines */
                for (y = rect->start_y; y < rect->end_y; y+=addr->step_y) {
                    vx_uint32 i = computePlaneOffset(rect->start_x, y, plane_index);
                    vx_uint32 j = vxComputePatchOffset(0, (y - rect->start_y), addr);
                    vx_uint32 len = computePlaneRangeSize(addr->dim_x, plane_index);
                    vx_uint8 *base_ptr = (vx_uint8 *)(res_base_ptr + m_memory.subplane_mem_offset[m][sp]);
                    memcpy(&external_ptr[j], base_ptr+i, len);
                }
            } else {
                /* The destination is not compact, we need to copy per element */
                vx_uint8 *pDestLine = &external_ptr[0];
                for (y = rect->start_y; y < rect->end_y; y+=addr->step_y) {
                    vx_uint8 *pDest = pDestLine;

                    vx_uint32 offset = computePlaneOffset(rect->start_x, y, plane_index);
                    vx_uint8 *base_ptr = (vx_uint8 *)(res_base_ptr + m_memory.subplane_mem_offset[m][sp]);
                    vx_uint8 *pSrc = base_ptr+offset;

                    for (x = rect->start_x; x < rect->end_x; x+=addr->step_x) {
                        /* One element */
                        memcpy(pDest, pSrc, m_memory.strides[m][sp][VX_DIM_X]);

                        pSrc += m_memory.strides[m][sp][VX_DIM_X];
                        pDest += addr->stride_x;
                    }

                    pDestLine += addr->stride_y;
                }
            }

            VXLOGD2("Copied image into %p\n", *ptr);
        }

        status = VX_SUCCESS;
    }

EXIT:
    return status;
}

vx_status
ExynosVisionImage::commitImagePatch(vx_rectangle_t *rect,
                                                            vx_uint32 plane_index,
                                                            vx_imagepatch_addressing_t *addr,
                                                            const void *ptr)
{
    vx_status status = VX_ERROR_INVALID_REFERENCE;
    vx_int32 i = 0;
    vx_bool external = vx_true_e; // assume that it was an allocated buffer
    vx_uint32 start_x = rect ? rect->start_x : 0u;
    vx_uint32 start_y = rect ? rect->start_y : 0u;
    vx_uint32 end_x = rect ? rect->end_x : 0u;
    vx_uint32 end_y = rect ? rect->end_y : 0u;
    vx_uint8 *external_ptr = (vx_uint8 *)ptr;
    vx_bool zero_area = ((((end_x - start_x) == 0) || ((end_y - start_y) == 0)) ? vx_true_e : vx_false_e);
    vx_uint32 index = UINT32_MAX; // out of bounds, if given to remove, won't do anything

    vx_uint32 m = m_plane_loc[plane_index].memory_index;
    vx_uint32 sp = m_plane_loc[plane_index].subplane_index;

    VXLOGD3("CommitImagePatch to ref(%d) from ptr %p plane %u to {%u,%u},{%u,%u}\n",
        getId(), ptr, plane_index, start_x, start_y, end_x, end_y);

    /* determine if virtual before checking for memory */
    if (m_is_virtual == vx_true_e && zero_area == vx_false_e) {
        if (m_kernel_count == 0) {
            /* User tried to access a "virtual" image. */
            VXLOGE("Can not access a virtual image");
            status = VX_ERROR_OPTIMIZED_AWAY;
            goto EXIT_ERR;
        }
        /* framework trying to access a virtual image, this is ok. */
    }

    if (zero_area == vx_true_e) {
        status = VX_SUCCESS;
        goto EXIT_SUCC;
    }

    if (m_constant == vx_true_e) {
        /* we tried to modify constant data! */
        VXLOGE("Can't set constant image data");
        status = VX_ERROR_NOT_SUPPORTED;
        /* don't modify the accessor here, it's an error case */
        goto EXIT_ERR;
    }

    if ((plane_index >= m_planes) ||
         (ptr == NULL) ||
         (addr == NULL)) {
        status = VX_ERROR_INVALID_PARAMETERS;
        VXLOGE("some parameter is not valid, plane_index:%d, ptr:%p, addr:%p", plane_index, ptr, addr);
        goto EXIT_ERR;
    }

    if ((m_continuous == vx_false_e) && (plane_index >= (vx_uint32)abs(m_memory.memory_num))) {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT_ERR;
    }

    /* check the rectangle, it has to be in actual plane space */
    if ((start_x >= end_x) || ((end_x - start_x) > addr->dim_x) ||
        (start_y >= end_y) || ((end_y - start_y) > addr->dim_y) ||
        (end_x > (vx_uint32)m_memory.dims[m][sp][VX_DIM_X] * (vx_uint32)m_scale[plane_index][VX_DIM_X]) ||
        (end_y > (vx_uint32)m_memory.dims[m][sp][VX_DIM_Y] * (vx_uint32)m_scale[plane_index][VX_DIM_X])) {
        VXLOGE("Invalid start,end coordinates! plane %u {%u,%u},{%u,%u}",
                 plane_index, start_x, start_y, end_x, end_y);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT_ERR;
    }

    if (m_memory.usage[plane_index] == VX_READ_ONLY) {
        VXLOGD2("committing image with read-only");
        status = VX_SUCCESS;
        goto EXIT_SUCC;
    }

    if (m_memory.access_mode[plane_index] == ACCESS_MAP_MODE) {
        /* determine if this grows the valid region */
        if (m_region.start_x > start_x)
            m_region.start_x = start_x;
        if (m_region.start_y > start_y)
            m_region.start_y = start_y;
        if (m_region.end_x < end_x)
            m_region.end_x = end_x;
        if (m_region.end_y < end_y)
            m_region.end_y = end_y;
    } else if (m_memory.access_mode[plane_index] == ACCESS_COPY_MODE) {
        /* copy the patch back to the image. */
        if ((vx_uint32)addr->stride_x == m_memory.strides[m][sp][VX_DIM_X]) {
            /* Both source and destination have compact lines */
            vx_uint32 y;
            for (y = start_y; y < end_y; y+=addr->step_y) {
                vx_uint32 i = computePlaneOffset(start_x, y, plane_index);
                vx_uint32 j = vxComputePatchOffset(0, (y - start_y), addr);
                vx_uint32 len = vxComputePatchRangeSize((end_x - start_x), addr);
                vx_uint8 *base_ptr = (vx_uint8 *)(m_cur_res->itemAt(m)->getAddr() + m_memory.subplane_mem_offset[m][sp]);
                VXLOGD2("%p[%u] <= %p[%u] for %u\n", base_ptr, j, external_ptr, i, len);
                memcpy(base_ptr+i, &external_ptr[j], len);
            }
        } else {
            /* The source is not compact, we need to copy per element */
            vx_uint32 x, y;
            vx_uint8 *pDestLine = &external_ptr[0];
            for (y = start_y; y < end_y; y+=addr->step_y) {
                vx_uint8 *pSrc = pDestLine;

                vx_uint32 offset = computePlaneOffset(start_x, y, plane_index);
                vx_uint8 *base_ptr = (vx_uint8 *)(m_cur_res->itemAt(m)->getAddr() + m_memory.subplane_mem_offset[m][sp]);
                vx_uint8 *pDest = base_ptr + offset;

                for (x = start_x; x < end_x; x+=addr->step_x) {
                    /* One element */
                    memcpy(pDest, pSrc, m_memory.strides[m][sp][VX_DIM_X]);

                    pDest += m_memory.strides[m][sp][VX_DIM_X];
                    pSrc += addr->stride_x;
                }

                pDestLine += addr->stride_y;
            }
        }

    }

    status = VX_SUCCESS;

EXIT_SUCC:
    if (m_memory.usage[plane_index] != VX_READ_ONLY)
        m_memory_locks[plane_index].unlock();

    m_memory.access_mode[plane_index] = ACCESS_NONE;
    if (decreaseAccessCount() == 0) {
        m_external_lock.unlock();
    }

EXIT_ERR:

    return status;
}

vx_status
ExynosVisionImage::accessImageHandle(vx_uint32 plane_index,
                                                                        vx_int32 *fd,
                                                                        vx_rectangle_t *rect,
                                                                        vx_enum usage)
{
    vx_status status = VX_FAILURE;
    vx_uint32 m;

    /* verify has not been call yet or will not be called (immediate mode)
     * this is a trick to "touch" an image so that it can be created
     */
    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    m = m_plane_loc[plane_index].memory_index;

    /* bad parameters */
    if ((usage < VX_READ_ONLY) || (VX_READ_AND_WRITE < usage)) {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    /* determine if virtual before checking for memory */
    if (m_is_virtual == vx_true_e) {
        if (m_kernel_count == 0) {
            /* User tried to access a "virtual" image. */
            VXLOGE("Can not access a virtual image");
            status = VX_ERROR_OPTIMIZED_AWAY;
            goto EXIT;
        }
        /* framework trying to access a virtual image, this is ok. */
    }

    /* can't write to constant */
    if ((m_constant == vx_true_e) && ((usage == VX_WRITE_ONLY) ||
                                         (usage == VX_READ_AND_WRITE))) {
        status = VX_ERROR_NOT_SUPPORTED;
        VXLOGE("Can't write to constant data, only read");
        getContext()->addLogEntry(this, status, "Can't write to constant data, only read!\n");
        goto EXIT;
    }

    if (increaseAccessCount() == 1) {
        if (m_external_lock.tryLock() != NO_ERROR) {
            VXLOGE("%s is already locking", getName());
            if (decreaseAccessCount() == 0) {
                m_external_lock.unlock();
            }
            status = VX_FAILURE;
            goto EXIT;
        }
    }
    m_memory.usage[plane_index] = usage;

    /* lock the memory plane for multiple writers*/
    if (usage != VX_READ_ONLY)
        m_memory_locks[plane_index].lock();

    int fd_tmp;
    fd_tmp = m_cur_res->itemAt(m)->getFd();
    if (fd_tmp == 0) {
        VXLOGE("fd is not allocated");
        status = VX_FAILURE;
        goto EXIT;
    }

    *fd = fd_tmp;
    rect->start_x = 0;
    rect->start_y = 0;
    rect->end_x = m_width;
    rect->end_y = m_height;

    status = VX_SUCCESS;

EXIT:
    return status;
}

vx_status
ExynosVisionImage::commitImageHandle(vx_uint32 plane_index, const vx_int32 fd)
{
    if (fd == -1) {
        VXLOGE("wrong fd:%d", fd);
        return VX_FAILURE;
    }

    vx_status status = VX_ERROR_INVALID_REFERENCE;

    vx_uint32 m = m_plane_loc[plane_index].memory_index;

    /* determine if virtual before checking for memory */
    if (m_is_virtual == vx_true_e) {
        if (m_kernel_count == 0) {
            /* User tried to access a "virtual" image. */
            VXLOGE("Can not access a virtual image");
            status = VX_ERROR_OPTIMIZED_AWAY;
            goto EXIT;
        }
        /* framework trying to access a virtual image, this is ok. */
    }

    status = VX_SUCCESS;

    if (m_memory.usage[plane_index] != VX_READ_ONLY)
        m_memory_locks[plane_index].unlock();

    m_memory.access_mode[plane_index] = ACCESS_NONE;
    if (decreaseAccessCount() == 0) {
        m_external_lock.unlock();
    }

EXIT:
    return status;
}

vx_size
ExynosVisionImage::computeImagePatchSize(const vx_rectangle_t *rect, vx_uint32 plane_index)
{
    vx_size size = 0ul;
    vx_uint32 start_x = 0u, start_y = 0u, end_x = 0u, end_y = 0u;

    vx_uint32 m = m_plane_loc[plane_index].memory_index;
    vx_uint32 sp = m_plane_loc[plane_index].subplane_index;

    if (rect) {
        start_x = rect->start_x;
        start_y = rect->start_y;
        end_x = rect->end_x;
        end_y = rect->end_y;

        if (m_is_allocated == vx_false_e)  {
            VXLOGE("Failed to allocate image");
            getContext()->addLogEntry(this, VX_ERROR_NO_MEMORY, "Failed to allocate image!\n");
            return 0;
        }
        if (plane_index < m_planes) {
            vx_size numPixels = ((end_x-start_x)/m_scale[plane_index][VX_DIM_X]) *
                                ((end_y-start_y)/m_scale[plane_index][VX_DIM_Y]);
            vx_size pixelSize = m_memory.strides[m][sp][VX_DIM_X];
            VXLOGD("numPixels = %d pixelSize = %d", numPixels, pixelSize);
            size = numPixels * pixelSize;
        } else {
            VXLOGE("Plane index %u is out of bounds!", plane_index);
            getContext()->addLogEntry(this, VX_ERROR_INVALID_PARAMETERS, "Plane index %u is out of bounds!", plane_index);
        }

        VXLOGD("image(%d) for patch {%u,%u to %u,%u} has a byte size of %d",
                 getId(), start_x, start_y, end_x, end_y, size);
    } else {
        VXLOGE("Image Reference is invalid");
    }

    return size;
}

vx_status
ExynosVisionImage::getValidRegionImage(vx_rectangle_t *rect)
{
    vx_status status = VX_ERROR_INVALID_REFERENCE;

    if (rect) {
        if ((m_region.start_x <= m_region.end_x) && (m_region.start_y <= m_region.end_y)) {
            rect->start_x = m_region.start_x;
            rect->start_y = m_region.start_y;
            rect->end_x = m_region.end_x;
            rect->end_y = m_region.end_y;
        } else {
            rect->start_x = 0;
            rect->start_y = 0;
            rect->end_x = m_width;
            rect->end_y = m_height;
        }
        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVisionImage::getDimension(vx_uint32 *width, vx_uint32 *height)
{
    *width = m_width;
    *height = m_height;

    return VX_SUCCESS;
}

vx_status
ExynosVisionImage::fillUniformValue(const void *value)
{
    vx_status status = VX_SUCCESS;
    vx_rectangle_t rect = {0, 0, m_width, m_height};

    for (vx_uint32 p = 0; p < m_planes; p++) {
        vx_imagepatch_addressing_t addr;
        void *base = NULL;
        status = accessImagePatch(&rect, p, &addr, &base, VX_WRITE_ONLY);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing image fails, plane:%d, err:%d", p, status);
            break;
        }

        for (vx_uint32 y = 0; y < addr.dim_y; y+=addr.step_y) {
            for (vx_uint32 x = 0; x < addr.dim_x; x+=addr.step_x) {
                if (m_format == VX_DF_IMAGE_U8) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = *pixel;
                } else if (m_format == VX_DF_IMAGE_U16) {
                    vx_uint16 *pixel = (vx_uint16 *)value;
                    vx_uint16 *ptr = (vx_uint16*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = *pixel;
                } else if (m_format == VX_DF_IMAGE_U32) {
                    vx_uint32 *pixel = (vx_uint32 *)value;
                    vx_uint32 *ptr = (vx_uint32*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = *pixel;
                } else if (m_format == VX_DF_IMAGE_S16) {
                    vx_int16 *pixel = (vx_int16 *)value;
                    vx_int16 *ptr = (vx_int16*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = *pixel;
                } else if (m_format == VX_DF_IMAGE_S32) {
                    vx_int32 *pixel = (vx_int32 *)value;
                    vx_int32 *ptr = (vx_int32*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = *pixel;
                } else if ((m_format == VX_DF_IMAGE_RGB)  ||
                         (m_format == VX_DF_IMAGE_RGBX)) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    ptr[0] = pixel[0];
                    ptr[1] = pixel[1];
                    ptr[2] = pixel[2];
                    if (m_format == VX_DF_IMAGE_RGBX)
                        ptr[3] = pixel[3];
                } else if ((m_format == VX_DF_IMAGE_YUV4) ||
                         (m_format == VX_DF_IMAGE_IYUV)) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = pixel[p];
                } else if ((p == 0) &&
                         ((m_format == VX_DF_IMAGE_NV12) ||
                          (m_format == VX_DF_IMAGE_NV21))) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr =(vx_uint8*) vxFormatImagePatchAddress2d(base, x, y, &addr);
                    *ptr = pixel[0];
                } else if ((p == 1) && (m_format == VX_DF_IMAGE_NV12)) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    ptr[0] = pixel[1];
                    ptr[1] = pixel[2];
                } else if ((p == 1) && (m_format == VX_DF_IMAGE_NV21)) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    ptr[0] = pixel[2];
                    ptr[1] = pixel[1];
                } else if (m_format == VX_DF_IMAGE_UYVY) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    if (x % 2 == 0) {
                        ptr[0] = pixel[1];
                        ptr[1] = pixel[0];
                    } else {
                        ptr[0] = pixel[2];
                        ptr[1] = pixel[0];
                    }
                } else if (m_format == VX_DF_IMAGE_YUYV) {
                    vx_uint8 *pixel = (vx_uint8 *)value;
                    vx_uint8 *ptr = (vx_uint8*)vxFormatImagePatchAddress2d(base, x, y, &addr);
                    if (x % 2 == 0) {
                        ptr[0] = pixel[0];
                        ptr[1] = pixel[1];
                    } else {
                        ptr[0] = pixel[0];
                        ptr[1] = pixel[2];
                    }
                }
            }
        }

        status = commitImagePatch(&rect, p, &addr, base);
        if (status != VX_SUCCESS) {
            VXLOGE("commiting image fails, plane:%d, err:%d", p, status);
            break;
        }
    } /* for loop */

    return status;
}

void
ExynosVisionImage::copyMemoryInfo(const ExynosVisionImage *src_image)
{
    memcpy(&m_memory.strides, &src_image->m_memory.strides, sizeof(m_memory.strides));
}

vx_status
ExynosVisionImage::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_true_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionImage::allocateResource(image_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("return pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    image_resource_t*buf_vector = new image_resource_t();

    vx_size plane_size = 0;

    for (vx_uint32 m = 0; m < m_memory.memory_num; m++) {
        vx_size memory_size = 0;
        for (vx_uint32 sp = 0; sp < m_memory.subplane_num[m]; sp++) {
            plane_size = m_memory.element_byte_size;
            for (vx_uint32 d = 0; d < VX_DIM_MAX; d++) {
                m_memory.strides[m][sp][d] = (vx_int32)plane_size;
                plane_size *= (vx_size)abs(m_memory.dims[m][sp][d]);
                m_memory.subplane_mem_offset[m][sp] = memory_size;
            }
            memory_size += plane_size;
        }

        if (memory_size == 0) {
            VXLOGE("%s, calculating allocation size is zero", getName());
            displayInfoMemory();
            break;
        }

        ExynosVisionBufMemory *buf = new ExynosVisionBufMemory();
        VXLOGD2("mem[%d] allocation size: %d", m, memory_size);
        status = buf->alloc(m_allocator, memory_size);
        if (status != VX_SUCCESS) {
            VXLOGE("buffer allocation fails at %s", getName());
            break;
        }

        buf_vector->push_back(buf);
    }

    if (status == VX_SUCCESS)
        *ret_resource = buf_vector;

    return status;
}

vx_status
ExynosVisionImage::freeResource(image_resource_t *buf_vector)
{
    vx_status status = VX_SUCCESS;

    image_resource_t::iterator buf_iter;
    for(buf_iter=buf_vector->begin(); buf_iter!=buf_vector->end(); buf_iter++) {
        status = (*buf_iter)->free(m_allocator);
        if (status != VX_SUCCESS) {
            VXLOGE("memory free fail, err:%d", status);
            break;
        }
        delete (*buf_iter);
    }

    buf_vector->clear();
    delete buf_vector;

    return status;
}

ExynosVisionDataReference*
ExynosVisionImage::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<image_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionImage::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<image_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionImage::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<image_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionImage::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<image_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

vx_status
ExynosVisionImage::createCloneObject(image_resource_t *buf_vector)
{
    if (m_clone_object_map[buf_vector]) {
        VXLOGE("%s has already clone object of %p", getName(), buf_vector);
        return VX_FAILURE;
    }

    ExynosVisionImage *clone_image = new ExynosVisionImageClone(getContext(), this);
    *clone_image = *this;
    clone_image->copyMemoryInfo(this);
    clone_image->m_res_type = RESOURCE_MNGR_NULL;
    clone_image->m_cur_res = buf_vector;
    clone_image->m_is_allocated = vx_true_e;

    m_clone_object_map[buf_vector] = clone_image;
    clone_image->incrementReference(VX_REF_INTERNAL, this);

    return VX_SUCCESS;
}

vx_status
ExynosVisionImage::destroyCloneObject(image_resource_t *buf_vector)
{
    if (!m_clone_object_map[buf_vector]) {
        VXLOGE("%s doesn't have clone object of %p", getName(), buf_vector);
        return VX_FAILURE;
    }

    ExynosVisionDataReference *clone_object = m_clone_object_map[buf_vector];
    vx_status status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&clone_object, VX_REF_INTERNAL, this);
    if (status != VX_SUCCESS) {
        VXLOGE("release %s fails at %s", clone_object->getName(), this->getName());
    }
    if (clone_object != NULL) {
        VXLOGE("clone_object should be destroyed when original obejct release it");
        clone_object->displayInfo(0, vx_true_e);
    }

    m_clone_object_map.erase(buf_vector);

    return VX_SUCCESS;
}

vx_status
ExynosVisionImage::registerRoiDescendant(ExynosVisionImageROI *roi_descendant)
{
    vx_status status;

    List<ExynosVisionImageROI*>::iterator image_iter;
    for (image_iter=m_roi_descendant_list.begin(); image_iter!=m_roi_descendant_list.end(); image_iter++) {
        if (ExynosVisionImage::checkImageDependenct(*image_iter, roi_descendant)) {
            status = ExynosVisionDataReference::jointAlliance(*image_iter, roi_descendant);
            if (status != VX_SUCCESS) {
                VXLOGE("joint alliance between %s and %s fails", (*image_iter)->getName(), roi_descendant->getName());
                break;
            }
        }
    }

    status = ExynosVisionDataReference::jointAlliance(this, roi_descendant);
    if (status != VX_SUCCESS) {
        VXLOGE("joint alliance between %s and %s fails", this->getName(), roi_descendant->getName());
    }
    m_roi_descendant_list.push_back(roi_descendant);

    return status;
}

void
ExynosVisionImage::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Image  ] %s(%p), resol:%dx%d, format:0x%x, virtual:%d, alloc:%d, refCnt:%d/%d", MAKE_TAB(tap, tab_num), getName(), this,
                                                m_width, m_height, m_format, isVirtual(), m_is_allocated,
                                                getInternalCnt(), getExternalCnt());

    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        VXLOGI("%s[--Image] alliance : %s", MAKE_TAB(tap, tab_num), (*ref_iter)->getName());
    }

    if (isDelayElement()) {
        VXLOGI("%s[--Image] delay element, %s, delay phy index:%d", MAKE_TAB(tap, tab_num), getDelay()->getName(), getDelaySlotIndex());
    }

    LOG_FLUSH_TIME();

    if ((m_res_type == RESOURCE_MNGR_SLOT) || (m_res_type == RESOURCE_MNGR_QUEUE)) {
        m_res_mngr->displayBuff(tab_num+1, vx_true_e);
    }

    if (detail_info)
        displayConn(tab_num);
}

void
ExynosVisionImage::displayInfoMemory(void)
{
    VXLOGI("%s, format:0x%x, size:%d x %d", getName(), m_format, m_width, m_height);

    for (vx_uint32 m=0; m<m_memory.memory_num; m++) {
        VXLOGI("[m_%d] sub-planes: %d", m, m_memory.subplane_num[m]);
        for (vx_uint32 sp=0; sp<m_memory.subplane_num[m]; sp++) {
            VXLOGI("memory offset: %p", m_memory.subplane_mem_offset[m][sp]);
            VXLOGI("dims[%d][%d] {%d, %d, %d}", m, sp, m_memory.dims[m][sp][0], m_memory.dims[m][sp][1], m_memory.dims[m][sp][2]);
            VXLOGI("stride[%d][%d] {%d, %d, %d}", m, sp, m_memory.strides[m][sp][0], m_memory.strides[m][sp][1], m_memory.strides[m][sp][2]);
        }
    }
}

#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionImage::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
{
    vx_status status = VX_SUCCESS;
    vx_uint32 m, p, sp;
    cl_int err = CL_SUCCESS;

    m_cl_memory.cl_type = CL_MEM_OBJECT_IMAGE2D;
    m_cl_memory.nptrs = m_planes;
    m_cl_memory.ndims = VX_DIM_MAX;

    switch (m_format) {
        case VX_DF_IMAGE_U8:
            m_cl_memory.cl_format = {CL_LUMINANCE, CL_UNORM_INT8};
            break;
        case VX_DF_IMAGE_U16:
            m_cl_memory.cl_format = {CL_LUMINANCE, CL_UNORM_INT16};
            break;
        case VX_DF_IMAGE_S16:
            m_cl_memory.cl_format = {CL_LUMINANCE, CL_SIGNED_INT16};
            break;
        case VX_DF_IMAGE_RGB:
            m_cl_memory.cl_format = {CL_RGB, CL_UNORM_INT8};
            break;
        case VX_DF_IMAGE_RGBX:
            m_cl_memory.cl_format = {CL_RGBA, CL_UNORM_INT8};
            break;
        case VX_DF_IMAGE_S32:
        case VX_DF_IMAGE_U32:
        default:
            VXLOGE("Image format(0x%X) is not supported", m_format);
			err = CL_IMAGE_FORMAT_NOT_SUPPORTED;
            status = VX_ERROR_INVALID_FORMAT;
            goto EXIT;
    }

    for (m = 0; m < m_memory.memory_num; m++) {
        for (p = 0, m = 0, sp = 0; sp < m_memory.subplane_num[m]; p++, sp++) {
            m_cl_memory.ptrs[p] = (vx_uint8 *)(m_cur_res->itemAt(m)->getAddr() + m_memory.subplane_mem_offset[m][sp]);
            vx_uint32 plane_size = m_memory.element_byte_size;
            for (vx_uint32 d = 0; d < VX_DIM_MAX; d++) {
                m_cl_memory.dims[p][d] = m_memory.dims[m][sp][d];
                m_cl_memory.strides[p][d] = m_memory.strides[m][sp][d];
                plane_size *= (vx_size)abs(m_memory.dims[m][sp][d]);
            }
            m_cl_memory.sizes[p] = plane_size;
            m_cl_memory.hdls[p] = clCreateImage2D(clContext,
                                    CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                    &m_cl_memory.cl_format,           // Image Format
                                    m_cl_memory.dims[p][VX_DIM_X],    // Image Width
                                    m_cl_memory.dims[p][VX_DIM_Y],    // Image Height
                                    m_cl_memory.strides[p][VX_DIM_Y], //Image Row Pitch
                                    m_cl_memory.ptrs[p],              // Host Ptr
                                    &err);
            if (err != CL_SUCCESS) {
                VXLOGE("failed in clCreateImage2D : err(%d)", err);
                status = VX_FAILURE;
                goto EXIT;
            }
        }
    }

EXIT:
    if (err != CL_SUCCESS) {
        m_cl_memory.allocated = vx_false_e;
        *memory = NULL;
    } else {
        m_cl_memory.allocated = vx_true_e;
        *memory = &m_cl_memory;
    }
    return status;
}
#endif
ExynosVisionImageROI::ExynosVisionImageROI(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                                        : ExynosVisionImage(context, scope, vx_false_e)
{
    memset(&m_rect_info, 0x0, sizeof(m_rect_info));
}

vx_status
ExynosVisionImageROI::destroy(void)
{
    vx_status status;

    if (m_roi_parent) {
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_roi_parent, VX_REF_INTERNAL, this);
        if (status != VX_SUCCESS) {
            VXLOGE("%s can't release internal count of %s", this->getName(), m_roi_parent->getName());
        }
        m_roi_parent = NULL;
    } else {
        VXLOGE("ImageROI(%s) can't have null pointer of parent", getName());
        status = VX_FAILURE;
    }

    m_cur_res = NULL;
    memset(&m_memory, 0x0, sizeof(m_memory));
    m_is_allocated = vx_false_e;

    return status;
}

vx_status
ExynosVisionImageROI::init(vx_uint32 width, vx_uint32 height, vx_df_image color)
{
    VXLOGE("%s, roi can't support norma init(use initFromROI), width:%d, height:%d, color:%d", width, height, color);

    return VX_FAILURE;
}

vx_status
ExynosVisionImageROI::registerRoiDescendant(ExynosVisionImageROI *roi_descendant)
{
    /* just hand over if not progenitor */
    return m_roi_parent->registerRoiDescendant(roi_descendant);
}

vx_status
ExynosVisionImageROI::initFromROI(ExynosVisionImage *parent_image, const vx_rectangle_t *rect)
{
    vx_status status = VX_SUCCESS;

    if (parent_image->isAllocated() == vx_false_e) {
        status = ((ExynosVisionDataReference*)parent_image)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("can't allocated parent resource, %s", parent_image->getName());
            goto EXIT;
        }
    }

    *((ExynosVisionImage*)this) = *parent_image;
    this->copyMemoryInfo(parent_image);

    m_width = rect->end_x - rect->start_x;
    m_height = rect->end_y - rect->start_y;

    vx_uint32 parent_width, parent_height;
    parent_image->getDimension(&parent_width, &parent_height);
    if ((parent_width < m_width) || (parent_height < m_height)) {
        VXLOGE("roi region is not valid, P(%d, %d), C(%d, %d)", parent_width, parent_height, m_width, m_height);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }
    m_rect_info = *rect;

    /* modify the dimensions */
    for (vx_uint32 m = 0; m < m_memory.memory_num; m++) {
        for (vx_uint32 p = 0; p < m_memory.subplane_num[m]; p++) {
            m_memory.dims[m][p][VX_DIM_X] = m_width;
            m_memory.dims[m][p][VX_DIM_Y] = m_height;

            m_memory.parent_plane_offset[p] = parent_image->computePlaneOffset(rect->start_x, rect->start_y, p);
        }
    }

    m_roi_parent = parent_image;
    parent_image->incrementReference(VX_REF_INTERNAL, this);

    parent_image->registerRoiDescendant(this);

    /* m_cur_res is not deallocated at destroy time, so the resource be only destroyed at parent data object. */
    m_res_type = RESOURCE_MNGR_NULL;
    m_cur_res = NULL;

    /* Creating image object from ROI can't support stream mode */
    m_is_allocated = vx_true_e;

EXIT:
    return status;
}

vx_status
ExynosVisionImageROI::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    VXLOGE("%s, roi can't allocate memory, res_type:%d, &param:%p", getName(), res_type, param);
    return VX_FAILURE;
}

ExynosVisionImage*
ExynosVisionImageROI::locateROI(vx_rectangle_t *rect)
{
    if ((rect->start_x == 0) && (rect->start_y == 0) &&
        (rect->end_x == 0) && (rect->end_y == 0)) {
        rect->end_x = m_width;
        rect->end_y = m_height;
    }

    vx_size plane_offset = m_memory.parent_plane_offset[0];
    vx_uint32 parent_shitf_y = plane_offset * m_scale[0][VX_DIM_Y] / m_memory.strides[0][0][VX_DIM_Y];
    vx_uint32 parent_shift_x = (plane_offset - (parent_shitf_y * m_memory.strides[0][0][VX_DIM_Y] / m_scale[0][VX_DIM_Y])) * m_scale[0][VX_DIM_X] / m_memory.strides[0][0][VX_DIM_X];
    rect->start_x += parent_shift_x;
    rect->end_x   += parent_shift_x;
    rect->start_y += parent_shitf_y;
    rect->end_y   += parent_shitf_y;

    return m_roi_parent->locateROI(rect);
}

vx_status
ExynosVisionImageROI::accessImagePatch(const vx_rectangle_t *rect,
                                                                    vx_uint32 plane_index,
                                                                    vx_imagepatch_addressing_t *addr,
                                                                    void **ptr,
                                                                    vx_enum usage)
{
    vx_status status;

    if (m_cur_res != NULL) {
        VXLOGE("resource should be null");
        return VX_ERROR_NO_RESOURCES;
    }

    vx_rectangle_t ancestor_rect = *rect;
    ExynosVisionImage *ancestor = locateROI(&ancestor_rect);
    if (ancestor == NULL) {
        VXLOGE("getting ancestor fails");
        displayInfo(0, vx_true_e);
        return VX_FAILURE;
    }

    status = ancestor->accessImagePatch(&ancestor_rect, plane_index, addr, ptr, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image patch fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVisionImageROI::commitImagePatch(vx_rectangle_t *rect,
                                                            vx_uint32 plane_index,
                                                            vx_imagepatch_addressing_t *addr,
                                                            const void *ptr)
{
    vx_status status;

    if (m_cur_res != NULL) {
        VXLOGE("resource should be null");
        return VX_ERROR_NO_RESOURCES;
    }

    vx_rectangle_t ancestor_rect;
    memset(&ancestor_rect, 0x0, sizeof(vx_rectangle_t));

    if (rect) {
        ancestor_rect = *rect;
    }

    ExynosVisionImage *ancestor = locateROI(&ancestor_rect);
    if (ancestor == NULL) {
        VXLOGE("getting ancestor fails");
        displayInfo(0, vx_true_e);
        return VX_FAILURE;
    }

    status = ancestor->commitImagePatch(&ancestor_rect, plane_index, addr, ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image patch fails, err:%d", status);

    }

    return status;
}

vx_status
ExynosVisionImageROI::accessImageHandle(vx_uint32 plane_index,
                                                                vx_int32 *fd,
                                                                vx_rectangle_t *rect,
                                                                vx_enum usage)
{
    vx_status status;

    if (m_cur_res != NULL) {
        VXLOGE("resource should be null");
        return VX_ERROR_NO_RESOURCES;
    }

    status = m_roi_parent->accessImageHandle(plane_index, fd, rect, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image handle fails, err:%d", status);
        goto EXIT;
    }

    if ((m_rect_info.end_x - m_rect_info.start_x) > (rect->end_x - rect->start_x) ||
        (m_rect_info.end_y - m_rect_info.start_y) > (rect->end_y - rect->start_y)) {
        VXLOGE("roi size is not matching, P(%d, %d), C(%d, %d)", rect->end_x - rect->start_x,
                                                                rect->end_y - rect->start_y,
                                                                m_rect_info.end_x - m_rect_info.start_x,
                                                                m_rect_info.end_y - m_rect_info.start_y);
        displayInfo(0, vx_true_e);
        status = VX_FAILURE;
        goto EXIT;
    }

    rect->end_x = m_rect_info.end_x + rect->start_x;
    rect->end_y = m_rect_info.end_y + rect->start_y;
    rect->start_x += m_rect_info.start_x;
    rect->start_y += m_rect_info.start_y;

EXIT:
    return status;
}

vx_status
ExynosVisionImageROI::commitImageHandle(vx_uint32 plane_index,
                                                                const vx_int32 fd)
{
    vx_status status;

    if (m_cur_res != NULL) {
        VXLOGE("resource should be null");
        return VX_ERROR_NO_RESOURCES;
    }

    status = m_roi_parent->commitImageHandle(plane_index, fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commit image handle fails, err:%d", status);

    }

    return status;
}

ExynosVisionDataReference*
ExynosVisionImageROI::getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    if (m_res_type != RESOURCE_MNGR_NULL) {
        VXLOGE("the mngr of ROI object should be null, %d, %d", frame_cnt, ret_data_valid);
        *ret_data_valid = vx_false_e;
        return NULL;
    }

    *ret_data_valid = vx_true_e;
    return this;
}

vx_status
ExynosVisionImageROI::putInputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type != RESOURCE_MNGR_NULL) {
        VXLOGE("the mngr of ROI object should be null, %d", frame_cnt);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

ExynosVisionDataReference*
ExynosVisionImageROI::getOutputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type != RESOURCE_MNGR_NULL) {
        VXLOGE("the mngr of ROI object should be null, %d", frame_cnt);
        return NULL;
    }

    return this;
}

vx_status
ExynosVisionImageROI::putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid)
{
    if (m_res_type != RESOURCE_MNGR_NULL) {
        VXLOGE("the mngr of ROI object should be null, %d, %d", frame_cnt, data_valid);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

ExynosVisionImageHandle::ExynosVisionImageHandle(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                                        : ExynosVisionImage(context, scope, vx_false_e)
{

}

vx_status
ExynosVisionImageHandle::destroy(void)
{
    m_cur_res = NULL;
    memset(&m_memory, 0x0, sizeof(m_memory));
    m_is_allocated = vx_false_e;

    return VX_SUCCESS;
}

vx_status
ExynosVisionImageHandle::swapImageHandle(void *const new_ptrs[], void *prev_ptrs[], vx_size num_planes)
{
    if (m_external_lock.tryLock() != NO_ERROR) {
        VXLOGE("%s is already locked by other object", getName());
        return VX_FAILURE;
    }

    vx_status status = VX_SUCCESS;

    if (m_planes != num_planes) {
        VXLOGE("the number of planes is not equal, %d, %d", m_planes, num_planes);
        status = VX_FAILURE;
        goto EXIT;
    }

    if (m_continuous) {
        vx_bool conitnuous_check = vx_false_e;
        switch(m_format) {
        case VX_DF_IMAGE_NV12:
        case VX_DF_IMAGE_NV21:
            if ((new_ptrs[0] == new_ptrs[1]) && (m_cur_res->size() == 1)) {
                conitnuous_check = vx_true_e;
            }
            break;
        case VX_DF_IMAGE_IYUV:
        case VX_DF_IMAGE_YUV4:
            if ((new_ptrs[0] == new_ptrs[1]) && (new_ptrs[1] == new_ptrs[2]) && (m_cur_res->size() == 1))
                conitnuous_check = vx_true_e;
            break;
        }

        if (conitnuous_check != vx_true_e) {
            VXLOGE("%s created as continuous memory");
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    image_resource_t::iterator res_iter;
    vx_uint32 m;
    for (res_iter=m_cur_res->begin(), m=0; res_iter!=m_cur_res->end(); res_iter++, m++) {
        if (m_import_type == VX_IMPORT_TYPE_HOST) {
            if (prev_ptrs) {
                prev_ptrs[m] = (*res_iter)->getAddr();
            }

            if (new_ptrs) {
                (*res_iter)->import(new_ptrs[m], 0);
            } else {
                (*res_iter)->import(NULL, 0);
            }
        } else if (m_import_type == VX_IMPORT_TYPE_ION) {
            if (prev_ptrs) {
                prev_ptrs[m] = (void*)(long long)((*res_iter)->getFd());
            }

            if (new_ptrs) {
                (*res_iter)->import(NULL, *((int*)&new_ptrs[m]));
            } else {
                (*res_iter)->import(NULL, 0);
            }
        } else {
            VXLOGE("un-supported import type");
        }
    }

    if (m_continuous) {
        for (vx_uint32 i=0; i<num_planes; i++) {
            prev_ptrs[i] = prev_ptrs[0];
        }
    }

EXIT:
    m_external_lock.unlock();

    return status;
}

vx_status
ExynosVisionImageHandle::checkContinuous(vx_df_image format, void *ptrs[])
{
    switch(format) {
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        if (ptrs[0] == ptrs[1])
            m_continuous = vx_true_e;
        break;
    case VX_DF_IMAGE_IYUV:
    case VX_DF_IMAGE_YUV4:
        if ((ptrs[0] == ptrs[1]) && (ptrs[1] == ptrs[2]))
            m_continuous = vx_true_e;
        break;
    }

    m_continuous = vx_false_e;

    return VX_SUCCESS;
}

vx_status
ExynosVisionImageHandle::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    VXLOGE("%s, handle can't allocate memory, res_type:%d, &param:%p", getName(), res_type, param);
    return VX_FAILURE;
}

vx_status
ExynosVisionImageHandle::importMemory(vx_imagepatch_addressing_t addrs[], void *ptrs[], vx_enum import_type)
{
    EXYNOS_VISION_REF_IN();
    vx_status status = VX_SUCCESS;
    vx_uint32 m;

    if (m_is_allocated == vx_false_e) {
        VXLOGD3("%s, importing %u pointers\n", getName(), m_memory.memory_num);

        m_res_type = RESOURCE_MNGR_SOLID;

        image_resource_t *resource = new image_resource_t();

        for (m = 0; m < m_memory.memory_num; m++) {
            if (ptrs[m] == NULL || addrs[m].dim_x == 0) {
                VXLOGE("importing memory information is empty, plane:%d", m);
                status = VX_FAILURE;
                break;
            }

            ExynosVisionBufMemory *buf = new ExynosVisionBufMemory();
            if (import_type == VX_IMPORT_TYPE_HOST)
                status = buf->import(ptrs[m], 0);
            else if (import_type == VX_IMPORT_TYPE_ION)
                status = buf->import(0, *((int*)&ptrs[m]));
            else
                status = VX_FAILURE;
            if (status != VX_SUCCESS) {
                VXLOGE("buffer allocation fails at %s", getName());
                break;
            }

            if (status == VX_SUCCESS) {
                m_memory.strides[m][0][VX_DIM_C] = m_memory.element_byte_size;
                m_memory.strides[m][0][VX_DIM_X] = addrs[m].stride_x;
                m_memory.strides[m][0][VX_DIM_Y] = addrs[m].stride_y;

                m_plane_loc[m].memory_index = m;
                m_plane_loc[m].subplane_index = 0;
           }

            resource->push_back(buf);
        }
        m_res_list.push_back(resource);

        if (status == VX_SUCCESS) {
            m_res_mngr = new ExynosVisionResSlotType<image_resource_t*>();
            status = m_res_mngr->registerResource(resource);
        } else {
            VXLOGE("resource allocation fails at %s", getName());
        }

        if (status == VX_SUCCESS) {
            m_res_type = RESOURCE_MNGR_SOLID;
            m_cur_res = resource;
        } else {
            VXLOGE("buffer allocation fails, err:%d", status);
        }
    }

    if (status == VX_SUCCESS) {
        m_is_allocated = vx_true_e; /* don't let the system realloc this memory */
        m_import_type = import_type;
    }

EXIT:

    return status;
}

ExynosVisionImageQueue::ExynosVisionImageQueue(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                                        : ExynosVisionImage(context, scope, vx_false_e)
{

}

vx_status
ExynosVisionImageQueue::allocateMemory(void)
{
    vx_enum res_type;
    struct resource_param res_param;
    vx_status status = VX_SUCCESS;

    if (m_is_allocated == vx_true_e) {
        VXLOGE("already allocated memory %s", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    ExynosVisionReference *scope;
    scope = getScope();
    if (scope->getType() != VX_TYPE_GRAPH) {
        VXLOGE("%s, queue data object should be belong to graph", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    if (((ExynosVisionGraph*)scope)->getExecMode() != GRAPH_EXEC_STREAM) {
        VXLOGE("%s, queue data object should be belong to stream graph", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    if (getDirectInputNodeNum((ExynosVisionGraph*)scope) == 0) {
        res_param.param.queue_param.direction = VX_INPUT;
    } else if (getDirectOutputNodeNum((ExynosVisionGraph*)scope) == 0) {
        res_param.param.queue_param.direction = VX_OUTPUT;
    } else {
        VXLOGE("%s, queue data object should be input or output of graph", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    res_type = RESOURCE_MNGR_QUEUE;

    status = ((ExynosVisionDataReference*)this)->allocateMemory(res_type, &res_param);
    if (status != VX_SUCCESS) {
        VXLOGE("allocating memory fails, err:%d", status);
    }

EXIT:

    return status;
}

vx_status
ExynosVisionImageQueue::pushImagePatch(vx_uint32 index, void *ptrs[], vx_uint32 num_buf)
{
    if (m_memory.memory_num!= num_buf) {
        VXLOGE("param is not correct, expected num_buf:%d, received:%d", m_memory.memory_num, num_buf);

        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (m_res_type != RESOURCE_MNGR_QUEUE)
        return VX_ERROR_INVALID_REFERENCE;

    ExynosVisionGraph *graph = (ExynosVisionGraph*)getScope();

    vx_status status = VX_SUCCESS;
    image_resource_t *buf_vector = new image_resource_t();

    for (vx_uint32 p = 0; p < m_memory.memory_num; p++) {
        if (ptrs[p] == NULL) {
            VXLOGE("imported buf is null");
            return VX_ERROR_INVALID_PARAMETERS;
        }

        ExynosVisionBufMemory *buf = new ExynosVisionBufMemory();
        status = buf->import(ptrs[p], 0);
        if (status != VX_SUCCESS) {
            VXLOGE("buffer allocation fails at %s", getName());
            break;
        }

        buf_vector->push_back(buf);
    }

    if (getDirectInputNodeNum(graph) == 0) {
        /* input queue type */
        vx_uint32 frame_cnt = graph->requestNewFrameCnt(this);
        m_res_mngr->pushResource(index, buf_vector, frame_cnt, getDirectOutputNodeNum(graph));

        triggerDoneEventDirect(graph, frame_cnt);
    } else if (getDirectOutputNodeNum(graph) == 0) {
        /* output queue type */
        m_res_mngr->pushResource(index, buf_vector);
    } else {
        VXLOGE("queue type object shoud be input or output of graph");
        status = VX_ERROR_INVALID_GRAPH;
    }

    return status;
}

vx_status
ExynosVisionImageQueue::pushImageHandle(vx_uint32 index, vx_int32 fd[], vx_uint32 num_buf)
{
    if (m_memory.memory_num != num_buf) {
        VXLOGE("param is not correct, expected num_buf:%d, received:%d", m_memory.memory_num, num_buf);

        return VX_ERROR_INVALID_PARAMETERS;
    }

    if (m_res_type != RESOURCE_MNGR_QUEUE)
        return VX_ERROR_INVALID_REFERENCE;

    ExynosVisionGraph *graph = (ExynosVisionGraph*)getScope();

    vx_status status = VX_SUCCESS;
    image_resource_t *buf_vector = new image_resource_t();

    for (vx_uint32 p = 0; p < m_memory.memory_num; p++) {
        ExynosVisionBufMemory *buf = new ExynosVisionBufMemory();
        status = buf->import(NULL, fd[p]);
        if (status != VX_SUCCESS) {
            VXLOGE("buffer allocation fails at %s", getName());
            break;
        }

        buf_vector->push_back(buf);
    }

    if (getDirectInputNodeNum(graph) == 0) {
        /* input queue type */
        vx_uint32 frame_cnt = graph->requestNewFrameCnt(this);
        m_res_mngr->pushResource(index, buf_vector, frame_cnt, getDirectOutputNodeNum(graph));

        triggerDoneEventDirect(graph, frame_cnt);
    } else if (getDirectOutputNodeNum(graph) == 0) {
        /* output queue type */
        m_res_mngr->pushResource(index, buf_vector);
    } else {
        VXLOGE("queue type object shoud be input or output of graph");
        status = VX_ERROR_INVALID_GRAPH;
    }

    return status;
}

vx_status
ExynosVisionImageQueue::popImage(vx_uint32 *ret_index, vx_bool *ret_data_valid)
{
    vx_status status;

    image_resource_t *buf_vector = NULL;
    status = m_res_mngr->popResource(ret_index, ret_data_valid, &buf_vector);
    if (status != VX_SUCCESS) {
        VXLOGE("popping resource fails, err:%d", status);
        return status;
    }

    for (vx_uint32 p = 0; p < m_memory.memory_num; p++) {
        ExynosVisionBufMemory *buf = buf_vector->editItemAt(p);
        delete buf;
    }

    delete buf_vector;

    return status;
}

ExynosVisionDataReference*
ExynosVisionImageQueue::getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    if (m_res_type != RESOURCE_MNGR_QUEUE) {
        VXLOGE("the mngr of object object should be queue type, %d, %d", frame_cnt, ret_data_valid);
        *ret_data_valid = vx_false_e;
        return NULL;
    }

    ExynosVisionDataReference *ref = getInputExclusiveRef_T<image_resource_t>(m_res_mngr, frame_cnt, ret_data_valid, &m_cur_res);
    if (*ret_data_valid == vx_false_e) {
        VXLOGE("input queuing data should be valid");
        displayInfo(0, vx_true_e);
    }

    return ref;
}

vx_status
ExynosVisionImageQueue::putInputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type != RESOURCE_MNGR_QUEUE) {
        VXLOGE("the mngr of queue object should be queue type, %d", frame_cnt);
        return VX_FAILURE;
    }

    return putInputExclusiveRef_T<image_resource_t>(m_res_mngr, frame_cnt, &m_cur_res);
}

ExynosVisionDataReference*
ExynosVisionImageQueue::getOutputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type != RESOURCE_MNGR_QUEUE) {
        VXLOGE("the mngr of queue object should be queue type, %d", frame_cnt);
        return NULL;
    }

    return getOutputExclusiveRef_T<image_resource_t>(m_res_mngr, frame_cnt, &m_cur_res);
}

vx_status
ExynosVisionImageQueue::putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid)
{
    if (m_res_type != RESOURCE_MNGR_QUEUE) {
        VXLOGE("the mngr of queue object should be queue type, %d", frame_cnt);
        return VX_FAILURE;
    }

    return putOutputExclusiveRef_T<image_resource_t>(m_res_mngr, frame_cnt, &m_cur_res, data_valid);
}

ExynosVisionImageClone::ExynosVisionImageClone(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                                        : ExynosVisionImage(context, scope, vx_false_e)
{

}

vx_status
ExynosVisionImageClone::destroy(void)
{
    m_cur_res = NULL;
    memset(&m_memory, 0x0, sizeof(m_memory));
    m_is_allocated = vx_false_e;

    return VX_SUCCESS;
}

vx_status
ExynosVisionImageClone::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    VXLOGE("%s, clone can't allocate memory, res_type:%d, &param:%p", getName(), res_type, param);
    return VX_FAILURE;
}

}; /* namespace android */
