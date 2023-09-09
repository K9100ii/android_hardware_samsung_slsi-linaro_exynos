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

#define LOG_TAG "ExynosVpuKernelUtil"
#include <cutils/log.h>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "vpu_kernel_util.h"

using namespace std;
using namespace android;

static void convertRectToRoi(vx_rectangle_t *rect, struct io_roi *roi) {
    roi->x = rect->start_x;
    roi->y = rect->start_y;
    roi->w = rect->end_x - rect->start_x;
    roi->h = rect->end_y - rect->start_y;
}

vx_status getVxObjectHandle(struct io_vx_info_t vx_param, vx_reference ref, Vector<io_buffer_t> *container, vx_enum usage)
{
    vx_status status = VX_SUCCESS;
    vx_int32 fd = -1;
    vx_enum type = 0;
    vx_rectangle_t rect;
    io_buffer_t *io_buffer;

    status = vxQueryReference(ref, VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    if (status != VX_SUCCESS) {
        VXLOGE("querying object fails");
        goto EXIT;
    }

    switch(type) {
    case VX_TYPE_IMAGE:
        vx_enum import_type;
        if (vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_IMPORT, &import_type, sizeof(import_type)) == VX_ERROR_NOT_SUPPORTED) {
            import_type = VX_IMPORT_TYPE_HOST;
        }

        if (import_type != VX_IMPORT_TYPE_HOST) {
            memset(&rect, 0x0, sizeof(rect));
            status = vxAccessImageHandle((vx_image)ref, vx_param.info.image.plane, &fd, &rect, usage);
            if (status != VX_SUCCESS) {
                VXLOGE("accessing image fails");
            }

            if (container->size() != 1) {
                VXLOGE("buffer container size is not matching, %d", container->size());
                status = VX_FAILURE;
                goto EXIT;
            }
            io_buffer = &container->editItemAt(0);
            io_buffer->m.fd = fd;
            convertRectToRoi(&rect, &io_buffer->roi);
        } else {
            memset(&rect, 0x0, sizeof(rect));
            vx_imagepatch_addressing_t addr;
            void *ptr = NULL;
            status = vxAccessImagePatch((vx_image)ref, &rect, vx_param.info.image.plane, &addr, &ptr, usage);
            if (status != VX_SUCCESS) {
                VXLOGE("accessing image fails");
            }

            if (container->size() != 1) {
                VXLOGE("buffer container size is not matching, %d", container->size());
                status = VX_FAILURE;
                goto EXIT;
            }
            io_buffer = &container->editItemAt(0);
            io_buffer->m.userptr = (unsigned long)ptr;
            convertRectToRoi(&rect, &io_buffer->roi);
        }
        break;
    case VX_TYPE_PYRAMID:
        vx_image image;
        if (vx_param.info.pyramid.level == PYRAMID_WHOLE_LEVEL) {
            vx_size pyramid_level_num;
            status = vxQueryPyramid((vx_pyramid)ref, VX_PYRAMID_ATTRIBUTE_LEVELS, &pyramid_level_num, sizeof(pyramid_level_num));
            if (status != VX_SUCCESS) {
                VXLOGE("querying pyramid fails, err:%d", status);
                goto EXIT;
            }

            if (container->size() != pyramid_level_num) {
                VXLOGE("buffer container size is not matching, %d, %d", container->size(), pyramid_level_num);
                status = VX_FAILURE;
                goto EXIT;
            }

            for (vx_uint32 i=0; i<pyramid_level_num; i++) {
                image = vxGetPyramidLevel((vx_pyramid)ref, i);
                memset(&rect, 0x0, sizeof(rect));
                status = vxAccessImageHandle((vx_image)image, 0, &fd, &rect, usage);
                if (status != VX_SUCCESS) {
                    VXLOGE("accessing image fails");
                }
                vxReleaseImage(&image);

                io_buffer = &container->editItemAt(i);
                io_buffer->m.fd = fd;
                convertRectToRoi(&rect, &io_buffer->roi);
            }
        } else  {
            image = vxGetPyramidLevel((vx_pyramid)ref, vx_param.info.pyramid.level);
            memset(&rect, 0x0, sizeof(rect));
            status = vxAccessImageHandle((vx_image)image, 0, &fd, &rect, usage);
            if (status != VX_SUCCESS) {
                VXLOGE("accessing image fails");
            }
            vxReleaseImage(&image);

            if (container->size() != 1) {
                VXLOGE("buffer container size is not matching, %d", container->size());
                status = VX_FAILURE;
                goto EXIT;
            }
            io_buffer = &container->editItemAt(0);
            io_buffer->m.fd = fd;
            convertRectToRoi(&rect, &io_buffer->roi);
        }
        break;
    case VX_TYPE_ARRAY:
        status = vxAccessArrayHandle((vx_array)ref, &fd, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing array fails");
        }

        if (container->size() != 1) {
            VXLOGE("buffer container size is not matching, %d", container->size());
            status = VX_FAILURE;
            goto EXIT;
        }
        container->editItemAt(0).m.fd = fd;
        break;
    case VX_TYPE_LUT:
        status = vxAccessLUTHandle((vx_lut)ref, &fd, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing lut fails");
        }

        if (container->size() != 1) {
            VXLOGE("buffer container size is not matching, %d", container->size());
            status = VX_FAILURE;
            goto EXIT;
        }
        container->editItemAt(0).m.fd = fd;
        break;
    case VX_TYPE_DISTRIBUTION:
        status = vxAccessDistributionHandle((vx_distribution)ref, &fd, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing distribution fails");
        }

        if (container->size() != 1) {
            VXLOGE("buffer container size is not matching, %d", container->size());
            status = VX_FAILURE;
            goto EXIT;
        }
        container->editItemAt(0).m.fd = fd;
        break;
    default:
        VXLOGE("un-supported type: 0x%x", type);
        status = VX_FAILURE;
        break;
    }

EXIT:
    return status;
}

vx_status putVxObjectHandle(struct io_vx_info_t vx_param, vx_reference ref, Vector<io_buffer_t> *container)
{
    vx_status status = VX_SUCCESS;

    vx_enum type = 0;
    status = vxQueryReference(ref, VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    if (status != VX_SUCCESS) {
        VXLOGE("querying object fails");
        goto EXIT;
    }

    switch(type) {
    case VX_TYPE_IMAGE:
        status = vxCommitImageHandle((vx_image)ref, vx_param.info.image.plane, container->editItemAt(0).m.fd);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing image fails");
        }
        break;
    case VX_TYPE_PYRAMID:
        vx_image image;
        if (vx_param.info.pyramid.level == PYRAMID_WHOLE_LEVEL) {
            for (vx_uint32 i=0; i<container->size(); i++) {
                image = vxGetPyramidLevel((vx_pyramid)ref, i);
                status = vxCommitImageHandle((vx_image)image, 0, container->editItemAt(i).m.fd);
                if (status != VX_SUCCESS) {
                    VXLOGE("commit image fails");
                }
                vxReleaseImage(&image);
            }
        } else {
            image = vxGetPyramidLevel((vx_pyramid)ref, vx_param.info.pyramid.level);
            status = vxCommitImageHandle((vx_image)image, 0, container->editItemAt(0).m.fd);
            if (status != VX_SUCCESS) {
                VXLOGE("commit image fails");
            }
            vxReleaseImage(&image);
        }
        break;
    case VX_TYPE_ARRAY:
        status = vxCommitArrayHandle((vx_array)ref, container->editItemAt(0).m.fd);
        if (status != VX_SUCCESS) {
            VXLOGE("commit array fails");
        }
        break;
    case VX_TYPE_LUT:
        status = vxCommitLUTHandle((vx_lut)ref, container->editItemAt(0).m.fd);
        if (status != VX_SUCCESS) {
            VXLOGE("commit lut fails");
        }
        break;
    case VX_TYPE_DISTRIBUTION:
        status = vxCommitDistributionHandle((vx_distribution)ref, container->editItemAt(0).m.fd);
        if (status != VX_SUCCESS) {
            VXLOGE("commit distribution fails");
        }
        break;
    default:
        VXLOGE("un-supported type: 0x%x", type);
        status = VX_FAILURE;
        break;
    }

EXIT:
    return status;
}

static vx_uint32 getScalarTypeSize(vx_enum scalar_type)
{
    vx_uint32 size = 0;

    switch (scalar_type) {
    case VX_TYPE_CHAR:
        size = sizeof(vx_char);
        break;
    case VX_TYPE_INT8:
        size = sizeof(vx_int8);
        break;
    case VX_TYPE_UINT8:
        size = sizeof(vx_uint8);
        break;
    case VX_TYPE_INT16:
        size = sizeof(vx_int16);
        break;
    case VX_TYPE_UINT16:
        size = sizeof(vx_uint16);
        break;
    case VX_TYPE_INT32:
        size = sizeof(vx_int32);
        break;
    case VX_TYPE_UINT32:
        size = sizeof(vx_uint32);
        break;
    case VX_TYPE_INT64:
        size = sizeof(vx_int64);
        break;
    case VX_TYPE_UINT64:
        size = sizeof(vx_uint64);
        break;
    case VX_TYPE_FLOAT32:
        size = sizeof(vx_float32);
        break;
    case VX_TYPE_FLOAT64:
        size = sizeof(vx_float64);
        break;
    case VX_TYPE_DF_IMAGE:
        size = sizeof(vx_df_image);
        break;
    case VX_TYPE_ENUM:
        size = sizeof(vx_enum);
        break;
    case VX_TYPE_SIZE:
        size = sizeof(vx_size);
        break;
    case VX_TYPE_BOOL:
        size = sizeof(vx_bool);
        break;
    default:
        VXLOGE("un-known scalar type");
        size = 0;
        break;
    }

    return size;
}

vx_status getIoInfoFromVxImage(vx_reference ref, vx_param_info_t param_info, struct io_format_t *io_format, struct io_memory_t *io_memory)
{
    vx_status status = VX_SUCCESS;

    vx_df_image vx_image_format;
    vx_uint32 width, height;
    vx_size planes, memories;
    vx_uint32 io_image_format;
    vx_bool is_continuous_mem;
    vx_uint32 pixel_byte;

    status = vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_FORMAT, &vx_image_format, sizeof(vx_image_format));
    status |= vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    status |= vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_PLANES, &planes, sizeof(planes));
    status |= vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_MEMORIES, &memories, sizeof(memories));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
        return status;
    }

    if (planes == memories)
        is_continuous_mem = vx_false_e;
    else
        is_continuous_mem = vx_true_e;

    switch (vx_image_format) {
    case VX_DF_IMAGE_RGB:
    case VX_DF_IMAGE_BGR:
        io_image_format = VS4L_DF_IMAGE_RGB;
        pixel_byte = 3;
        break;
    case VX_DF_IMAGE_RGBX:
    case VX_DF_IMAGE_BGRX:
        io_image_format = VS4L_DF_IMAGE_RGBX;
        pixel_byte = 4;
        break;
    case VX_DF_IMAGE_UYVY:
    case VX_DF_IMAGE_YUYV:
        io_image_format = VS4L_DF_IMAGE_U16;
        pixel_byte = 2;
        break;
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        if (is_continuous_mem) {
            io_image_format = vx_image_format;
        } else {
            /* in non-continuous memory case, each memory of multi-plane is handled as simple type by vpu driver */
            if (param_info.image.plane == 0)
                io_image_format = VS4L_DF_IMAGE_U8;
            else
                io_image_format = VS4L_DF_IMAGE_U16;
        }

        if (param_info.image.plane == 0)
            pixel_byte = 1;
        else
            pixel_byte = 2;

        if (param_info.image.plane != 0) {
            width /= 2;
            height /= 2;
        }
        break;
    case VX_DF_IMAGE_IYUV:
    case VX_DF_IMAGE_YUV4:
        if (is_continuous_mem) {
            io_image_format = vx_image_format;
        } else {
            /* in non-continuous memory case, each memory of multi-plane is handled as simple type by vpu driver */
            io_image_format = VS4L_DF_IMAGE_U8;
        }
        pixel_byte = 1;
        if (param_info.image.plane != 0) {
            width /= 2;
        }
        break;
    case VX_DF_IMAGE_U8:
        pixel_byte = 1;
        io_image_format = VS4L_DF_IMAGE_U8;
        break;
    case VX_DF_IMAGE_U16:
        pixel_byte = 2;
        io_image_format = VS4L_DF_IMAGE_U16;
        break;
    case VX_DF_IMAGE_S16:
        pixel_byte = 2;
        io_image_format = VS4L_DF_IMAGE_S16;
        break;
    case VX_DF_IMAGE_U32:
        pixel_byte = 4;
        io_image_format = VS4L_DF_IMAGE_U32;
        break;
    case VX_DF_IMAGE_S32:
        pixel_byte = 4;
        io_image_format = VS4L_DF_IMAGE_S32;
        break;
    default:
        pixel_byte = 1;
        io_image_format = vx_image_format;
        break;
    }

    if (status == VX_SUCCESS) {
        io_format->format = io_image_format;
        io_format->width = width;
        io_format->height = height;
        if (is_continuous_mem)
            io_format->plane = planes;
        else
            io_format->plane = 0;

        io_format->pixel_byte = pixel_byte;
    } else {
        VXLOGE("querying image fails, err:%d", status);
    }

    vx_enum import_type;
    if (vxQueryImage((vx_image)ref, VX_IMAGE_ATTRIBUTE_IMPORT, &import_type, sizeof(import_type)) == VX_ERROR_NOT_SUPPORTED) {
        import_type = VX_IMPORT_TYPE_HOST;
    }

    io_memory->type = VS4L_BUFFER_LIST;
    if (import_type == VX_IMPORT_TYPE_HOST)
        io_memory->memory = VS4L_MEMORY_USERPTR;
    else
        io_memory->memory = VS4L_MEMORY_DMABUF;
    io_memory->count = 1;

    return status;
}

vx_status getIoInfoFromVx(vx_reference ref, vx_param_info_t param_info, List<struct io_format_t> *io_format_list, struct io_memory_t *io_memory)
{
    vx_status status = VX_SUCCESS;
    vx_enum type = 0;
    status = vxQueryReference(ref, VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    if (status != VX_SUCCESS) {
        VXLOGE("querying reference fails, err:%d", status);
        goto EXIT;
    }

    io_format_list->clear();
    struct io_format_t io_format;

    switch(type) {
    case VX_TYPE_IMAGE:
        status = getIoInfoFromVxImage(ref, param_info, &io_format, io_memory);
        if (status != VX_SUCCESS) {
            VXLOGE("getting info from image fails");
        }
        io_format_list->push_back(io_format);
        break;
    case VX_TYPE_PYRAMID:
        {
            vx_param_info_t image_param_info;
            image_param_info.image.plane = 0;

            if (param_info.pyramid.level != PYRAMID_WHOLE_LEVEL) {
                vx_image image = vxGetPyramidLevel((vx_pyramid)ref, param_info.pyramid.level);
                if (image != NULL) {
                    status = getIoInfoFromVxImage((vx_reference)image, image_param_info, &io_format, io_memory);
                    if (status != VX_SUCCESS) {
                        VXLOGE("getting info from image of pyramid fails");
                    }
                } else {
                    VXLOGE("getting image from pyramid fails, level:%d", param_info.pyramid.level);
                    status = VX_FAILURE;
                }
                vxReleaseImage(&image);
                io_format_list->push_back(io_format);
            } else {
                vx_size pyramid_level;
                status = vxQueryPyramid((vx_pyramid)ref, VX_PYRAMID_ATTRIBUTE_LEVELS, &pyramid_level, sizeof(pyramid_level));
                if (status != VX_SUCCESS) {
                    VXLOGE("querying pyramid fails, err:%d", status);
                    goto EXIT;
                }

                for (vx_uint32 i=0; i<pyramid_level; i++) {
                    vx_image image = vxGetPyramidLevel((vx_pyramid)ref, i);
                    if (image != NULL) {
                        status = getIoInfoFromVxImage((vx_reference)image, image_param_info, &io_format, io_memory);
                        if (status != VX_SUCCESS) {
                            VXLOGE("getting info from image of pyramid fails");
                        }
                    } else {
                        VXLOGE("getting image from pyramid fails, level:%d", param_info.pyramid.level);
                        status = VX_FAILURE;
                    }
                    vxReleaseImage(&image);
                    io_format_list->push_back(io_format);
                }

                io_memory->type = VS4L_BUFFER_PYRAMID;
                io_memory->count = pyramid_level;
            }
        }
        break;
    case VX_TYPE_ARRAY:
        vx_size item_size, capacity;
        status |= vxQueryArray((vx_array)ref, VX_ARRAY_ATTRIBUTE_ITEMSIZE, &item_size, sizeof(item_size));
        status |= vxQueryArray((vx_array)ref, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
        if (status != VX_SUCCESS) {
            VXLOGE("querying array fails, err:%d", status);
            goto EXIT;
        }

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = item_size;
        io_format.height = capacity;
        io_format.plane = 0;
        io_format.pixel_byte = 1;
        io_format_list->push_back(io_format);

        io_memory->type = VS4L_BUFFER_LIST;
        io_memory->memory = VS4L_MEMORY_DMABUF;
        io_memory->count = 1;
        break;
    case VX_TYPE_SCALAR:
        vx_enum scalar_type;
        status |= vxQueryScalar((vx_scalar)ref, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));
        if (status != VX_SUCCESS) {
            VXLOGE("querying lut fails, err:%d", status);
            goto EXIT;
        }

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = getScalarTypeSize(scalar_type);
        io_format.height = 1;
        io_format.plane = 0;
        io_format.pixel_byte = 1;
        io_format_list->push_back(io_format);

        io_memory->type = VS4L_BUFFER_LIST;
        io_memory->memory = VS4L_MEMORY_USERPTR;
        io_memory->count = 1;
        break;
    case VX_TYPE_LUT:
        vx_size lut_size;
        status |= vxQueryLUT((vx_lut)ref, VX_LUT_ATTRIBUTE_SIZE, &lut_size, sizeof(lut_size));
        if (status != VX_SUCCESS) {
            VXLOGE("querying lut fails, err:%d", status);
            goto EXIT;
        }

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = lut_size;
        io_format.height = 1;
        io_format.plane = 0;
        io_format.pixel_byte = 1;
        io_format_list->push_back(io_format);

        io_memory->type = VS4L_BUFFER_LIST;
        io_memory->memory = VS4L_MEMORY_DMABUF;
        io_memory->count = 1;
        break;
    case VX_TYPE_DISTRIBUTION:
        vx_size dist_size;
        status |= vxQueryDistribution((vx_distribution)ref, VX_DISTRIBUTION_ATTRIBUTE_SIZE, &dist_size, sizeof(dist_size));
        if (status != VX_SUCCESS) {
            VXLOGE("querying dist fails, err:%d", status);
            goto EXIT;
        }

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = dist_size;
        io_format.height = 1;
        io_format.plane = 0;
        io_format.pixel_byte = 1;
        io_format_list->push_back(io_format);

        io_memory->type = VS4L_BUFFER_LIST;
        io_memory->memory = VS4L_MEMORY_DMABUF;
        io_memory->count = 1;
        break;
    default:
        VXLOGE("not supported type: 0x%x", type);
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    if (io_format_list->size() != io_memory->count) {
        VXLOGE("getting info is invalid, %d, %d", io_format_list->size(), io_memory->count);
        status = VX_FAILURE;
    }

EXIT:
    return status;
}

vx_status convertFloatToMultiShift(vx_float32 float_value, vx_size *shift_value, vx_size *multiply_value)
{
    vx_status status = VX_SUCCESS;
    vx_uint64 candi_mul_value;
    vx_int32 final_mul_value = 0;
    vx_int32 i;

    for (i=(*shift_value==0)?16:*shift_value; i>=0; i--) {
        candi_mul_value = floor(float_value*pow(2, i));
        if (candi_mul_value <= INT16_MAX) {
            final_mul_value = (vx_int32)candi_mul_value;
            break;
        }
    }

    if (final_mul_value) {
        *shift_value = i;
        *multiply_value = final_mul_value;
    } else {
        VXLOGE("can't find value");
        status = VX_FAILURE;
    }

    return status;
}

vx_status copyArray(vx_array src, vx_array dst)
{
    vx_status status = VX_SUCCESS; // assume success until an error occurs.
    vx_size src_num_items = 0, dst_capacity = 0, src_stride = 0;
    void *srcp = NULL;

    status = VX_SUCCESS;
    status |= vxQueryArray(src, VX_ARRAY_ATTRIBUTE_NUMITEMS, &src_num_items, sizeof(src_num_items));
    status |= vxQueryArray(dst, VX_ARRAY_ATTRIBUTE_CAPACITY, &dst_capacity, sizeof(dst_capacity));
    if (status == VX_SUCCESS) {
        status |= vxAccessArrayRange(src, 0, src_num_items, &src_stride, &srcp, VX_READ_ONLY);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing array range fails, src_num_items:%d", src_num_items);
        }

        if (src_num_items <= dst_capacity && status == VX_SUCCESS) {
            status |= vxTruncateArray(dst, 0);
            status |= vxAddArrayItems(dst, src_num_items, srcp, src_stride);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }

        status |= vxCommitArrayRange(src, 0, 0, srcp);
    }

    return status;
}

