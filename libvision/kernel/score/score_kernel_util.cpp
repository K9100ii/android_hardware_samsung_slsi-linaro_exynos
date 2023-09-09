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

#define LOG_TAG "ExynosScoreKernelUtil"
#include <cutils/log.h>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include <score.h>
#include "score_kernel_util.h"

using namespace std;
using namespace android;
using namespace score;
vx_status getScVxObjectHandle(vx_reference ref, vx_int32 *fd, vx_enum usage)
{
    vx_status status = VX_SUCCESS;

    vx_enum type = 0;
    status = vxQueryReference(ref, VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    if (status != VX_SUCCESS) {
        VXLOGE("querying object fails");
        goto EXIT;
    }

    switch (type) {
    case VX_TYPE_IMAGE:
        vx_rectangle_t rect;
        memset(&rect, 0x0, sizeof(rect));
        status = vxAccessImageHandle((vx_image)ref, 0, fd, &rect, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing image fails");
        }
        break;
    case VX_TYPE_LUT:
        status = vxAccessLUTHandle((vx_lut)ref, fd, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing lut fails");
        }
        break;
    case VX_TYPE_DISTRIBUTION:
        status = vxAccessDistributionHandle((vx_distribution)ref, fd, usage);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing distribution fails");
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

vx_status putScVxObjectHandle(vx_reference ref, vx_int32 fd)
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
        status = vxCommitImageHandle((vx_image)ref, 0, fd);
        if (status != VX_SUCCESS) {
            VXLOGE("accessing image fails");
        }
        break;
    case VX_TYPE_LUT:
        status = vxCommitLUTHandle((vx_lut)ref, fd);
        if (status != VX_SUCCESS) {
            VXLOGE("commit lut fails");
        }
        break;
    case VX_TYPE_DISTRIBUTION:
        status = vxCommitDistributionHandle((vx_distribution)ref, fd);
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

vx_status getScImageParamInfo(vx_image image, ScBuffer *sc_buf)
{
    vx_status status = VX_SUCCESS;

    vx_uint32 width, height;

    vx_df_image vx_image_format;
    data_buf_type sc_image_format;

    status = vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    status |= vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &vx_image_format, sizeof(vx_image_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
        return status;
    }

    switch (vx_image_format) {
    case VX_DF_IMAGE_RGB:
        sc_image_format = SC_RGB;
        break;
    case VX_DF_IMAGE_RGBX:
        sc_image_format = SC_RGBA;
        break;
    case VX_DF_IMAGE_UYVY:
        sc_image_format = SC_UYVY;
        break;
    case VX_DF_IMAGE_YUYV:
        sc_image_format = SC_YUYV;
        break;
    case VX_DF_IMAGE_NV12:
        sc_image_format = SC_NV12;
        break;
    case VX_DF_IMAGE_NV21:
        sc_image_format = SC_NV21;
        break;
    case VX_DF_IMAGE_IYUV:
        sc_image_format = SC_IYUV;
        break;
    case VX_DF_IMAGE_YUV4:
        sc_image_format = SC_YUV4;
        break;
    case VX_DF_IMAGE_U8:
        sc_image_format = SC_TYPE_U8;
        break;
    case VX_DF_IMAGE_U16:
        sc_image_format = SC_TYPE_U16;
        break;
    case VX_DF_IMAGE_S16:
        sc_image_format = SC_TYPE_S16;
        break;
    case VX_DF_IMAGE_U32:
        sc_image_format = SC_TYPE_U32;
        break;
    case VX_DF_IMAGE_S32:
        sc_image_format = SC_TYPE_S32;
        break;
    default:
        VXLOGE("Invalid vx image format(0x%x)", vx_image_format);
        sc_image_format = SC_TYPE_U8;
        break;
    }

    if (status == VX_SUCCESS) {
        sc_buf->buf.width = width;
        sc_buf->buf.height = height;
        sc_buf->buf.type = sc_image_format;
    } else {
        VXLOGE("querying image fails, err:%d", status);
    }

    return status;
}

vx_status getScParamInfo(vx_reference ref, ScBuffer *sc_buf)
{
    vx_status status = VX_SUCCESS;
    vx_enum type = 0;
    status = vxQueryReference(ref, VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    if (status != VX_SUCCESS) {
        VXLOGE("querying reference fails, err:%d", status);
        goto EXIT;
    }

    switch (type) {
    case VX_TYPE_IMAGE:
        status = getScImageParamInfo((vx_image)ref, sc_buf);
        if (status != VX_SUCCESS) {
            VXLOGE("getting info from image fails");
        }
        break;
    case VX_TYPE_LUT:
        vx_size lut_size;
        status = vxQueryLUT((vx_lut)ref, VX_LUT_ATTRIBUTE_SIZE, &lut_size, sizeof(lut_size));
        if (status != VX_SUCCESS) {
            VXLOGE("querying lut fails, err:%d", status);
            goto EXIT;
        }
        sc_buf->buf.type = SC_TYPE_U8;
        sc_buf->buf.width = lut_size;
        sc_buf->buf.height = 1;
        break;
    case VX_TYPE_DISTRIBUTION:
        vx_size bins;
        status = vxQueryDistribution((vx_distribution)ref, VX_DISTRIBUTION_ATTRIBUTE_BINS, &bins, sizeof(bins));
        if (status != VX_SUCCESS) {
            VXLOGE("querying dist fails, err:%d", status);
            goto EXIT;
        }
        sc_buf->buf.type = SC_TYPE_U32;
        sc_buf->buf.width = bins;
        sc_buf->buf.height = 1;
        break;
    case VX_TYPE_CONVOLUTION:
        vx_size rows;
        vx_size columns;
        status |= vxQueryConvolution((vx_convolution)ref, VX_CONVOLUTION_ATTRIBUTE_ROWS, &rows, sizeof(rows));
        status |= vxQueryConvolution((vx_convolution)ref, VX_CONVOLUTION_ATTRIBUTE_COLUMNS, &columns, sizeof(columns));
        if (status != VX_SUCCESS) {
            VXLOGE("querying convolution fails");
            goto EXIT;
        }
        if (rows != columns) {
            VXLOGE("rows and columns is not same, rows:%d, columns:%d", rows, columns);
            goto EXIT;
        }
        sc_buf->buf.type = SC_TYPE_S16;
        sc_buf->buf.width = columns;
        sc_buf->buf.height = rows;
        break;
    default:
        VXLOGE("not supported type: 0x%x", type);
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
EXIT:
    return status;
}

