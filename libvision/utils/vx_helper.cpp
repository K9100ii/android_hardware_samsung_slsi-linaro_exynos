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

#define LOG_TAG "OpenVXHelper"
#include <cutils/log.h>

#include "ExynosVisionCommonConfig.h"

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_helper.h>

vx_bool vxFindOverlapRectangle(vx_rectangle_t *rect_a, vx_rectangle_t *rect_b, vx_rectangle_t *rect_res)
{
    vx_bool res = vx_false_e;
    if ((rect_a) && (rect_b) && (rect_res))
    {
        enum {sx = 0, sy = 1, ex = 2, ey = 3};
        vx_uint32 a[4] = {rect_a->start_x, rect_a->start_y, rect_a->end_x, rect_a->end_y};
        vx_uint32 b[4] = {rect_b->start_x, rect_b->start_y, rect_b->end_x, rect_b->end_y};
        vx_uint32 c[4] = {0};
        c[sx] = (a[sx] > b[sx] ? a[sx] : b[sx]);
        c[sy] = (a[sy] > b[sy] ? a[sy] : b[sy]);
        c[ex] = (a[ex] < b[ex] ? a[ex] : b[ex]);
        c[ey] = (a[ey] < b[ey] ? a[ey] : b[ey]);
        if (c[sx] < c[ex] && c[sy] < c[ey])
        {
            rect_res->start_x = c[sx];
            rect_res->start_y = c[sy];
            rect_res->end_x = c[ex];
            rect_res->end_y = c[ey];
            res = vx_true_e;
        }
    }
    return res;
}

vx_bool vxIsValidType(vx_enum type)
{
    vx_bool ret = vx_false_e;
    if (type <= VX_TYPE_INVALID)
    {
        ret = vx_false_e;
    }
    else if (VX_TYPE_IS_SCALAR(type)) /* some scalar */
    {
        ret = vx_true_e;
    }
    else if (VX_TYPE_IS_STRUCT(type)) /* some struct */
    {
        ret = vx_true_e;
    }
    else if (VX_TYPE_IS_OBJECT(type)) /* some object */
    {
        ret = vx_true_e;
    }
    else
    {
        VXLOGE("Type 0x%08x is invalid!");
    }
    return ret; /* otherwise, not a valid type */
}

vx_bool vxIsValidDirection(vx_enum dir)
{
    if ((dir == VX_INPUT) || (dir == VX_OUTPUT))  {
        return vx_true_e;
    } else if (dir == VX_BIDIRECTIONAL) {
        /* Bidirectional is not valid for user kernels accroding to OpenVX specification. But the reason is not written. */
        VXLOGD2("bidirection is detected");
        return vx_true_e;
    } else {
        return vx_false_e;
    }
}

vx_bool vxIsValidState(vx_enum state)
{
    if ((state == VX_PARAMETER_STATE_REQUIRED) ||
        (state == VX_PARAMETER_STATE_OPTIONAL))
    {
        return vx_true_e;
    }
    else
    {
        return vx_false_e;
    }
}

vx_bool vxIsValidImport(vx_enum type)
{
    vx_bool ret = vx_false_e;
    switch(type)
    {
        case VX_IMPORT_TYPE_HOST:
        case VX_IMPORT_TYPE_ION:
            ret = vx_true_e;
            break;
        case VX_IMPORT_TYPE_NONE:
        default:
            ret = vx_false_e;
            break;
    }
    return ret;
}

void vxClearLog(vx_reference ref)
{
    if (!ref) {
        VXLOGE("ref is null");
    }
}

void vxReadRectangle(const void *base,
                     const vx_imagepatch_addressing_t *addr,
                     const vx_border_mode_t *borders,
                     vx_df_image type,
                     vx_uint32 center_x,
                     vx_uint32 center_y,
                     vx_uint32 radius_x,
                     vx_uint32 radius_y,
                     void *destination)
{
    vx_int32 width = (vx_int32)addr->dim_x, height = (vx_int32)addr->dim_y;
    vx_int32 stride_y = addr->stride_y;
    vx_int32 stride_x = addr->stride_x;
    const vx_uint8 *ptr = (const vx_uint8 *)base;
    vx_int32 ky, kx;
    vx_uint32 dest_index = 0;
    // kx, kx - kernel x and y
    if( borders->mode == VX_BORDER_MODE_REPLICATE || borders->mode == VX_BORDER_MODE_UNDEFINED )
    {
        for (ky = -(int32_t)radius_y; ky <= (int32_t)radius_y; ++ky)
        {
            vx_int32 y = (vx_int32)(center_y + ky);
            y = y < 0 ? 0 : y >= height ? height - 1 : y;

            for (kx = -(int32_t)radius_x; kx <= (int32_t)radius_x; ++kx, ++dest_index)
            {
                vx_int32 x = (int32_t)(center_x + kx);
                x = x < 0 ? 0 : x >= width ? width - 1 : x;

                switch(type)
                {
                case VX_DF_IMAGE_U8:
                    ((vx_uint8*)destination)[dest_index] = *(vx_uint8*)(ptr + y*stride_y + x*stride_x);
                    break;
                case VX_DF_IMAGE_S16:
                case VX_DF_IMAGE_U16:
                    ((vx_uint16*)destination)[dest_index] = *(vx_uint16*)(ptr + y*stride_y + x*stride_x);
                    break;
                case VX_DF_IMAGE_S32:
                case VX_DF_IMAGE_U32:
                    ((vx_uint32*)destination)[dest_index] = *(vx_uint32*)(ptr + y*stride_y + x*stride_x);
                    break;
                default:
                    VXLOGE("un-supported data type");
                    return;
                    break;
                }
            }
        }
    }
    else if( borders->mode == VX_BORDER_MODE_CONSTANT )
    {
        vx_uint32 cval = borders->constant_value;
        for (ky = -(int32_t)radius_y; ky <= (int32_t)radius_y; ++ky)
        {
            vx_int32 y = (vx_int32)(center_y + ky);
            int ccase_y = y < 0 || y >= height;

            for (kx = -(int32_t)radius_x; kx <= (int32_t)radius_x; ++kx, ++dest_index)
            {
                vx_int32 x = (int32_t)(center_x + kx);
                int ccase = ccase_y || x < 0 || x >= width;

                switch(type)
                {
                    case VX_DF_IMAGE_U8:
                        if( !ccase )
                            ((vx_uint8*)destination)[dest_index] = *(vx_uint8*)(ptr + y*stride_y + x*stride_x);
                        else
                            ((vx_uint8*)destination)[dest_index] = (vx_uint8)cval;
                        break;
                    case VX_DF_IMAGE_S16:
                    case VX_DF_IMAGE_U16:
                        if( !ccase )
                            ((vx_uint16*)destination)[dest_index] = *(vx_uint16*)(ptr + y*stride_y + x*stride_x);
                        else
                            ((vx_uint16*)destination)[dest_index] = (vx_uint16)cval;
                        break;
                    case VX_DF_IMAGE_S32:
                    case VX_DF_IMAGE_U32:
                        if( !ccase )
                            ((vx_uint32*)destination)[dest_index] = *(vx_uint32*)(ptr + y*stride_y + x*stride_x);
                        else
                            ((vx_uint32*)destination)[dest_index] = (vx_uint32)cval;
                        break;
                    default:
                        VXLOGE("un-supported data type");
                        return;
                        break;
                }
            }
        }
    }
    else {
        VXLOGE("un-supported border type");
    }
}

vx_status vxAlterRectangle(vx_rectangle_t *rect,
                           vx_int32 dsx,
                           vx_int32 dsy,
                           vx_int32 dex,
                           vx_int32 dey)
{
    if (rect)
    {
        rect->start_x += dsx;
        rect->start_y += dsy;
        rect->end_x += dex;
        rect->end_y += dey;
        return VX_SUCCESS;
    }

    return VX_ERROR_INVALID_REFERENCE;
}

vx_status vxSetAffineRotationMatrix(vx_matrix matrix,
                                    vx_float32 angle,
                                    vx_float32 scale,
                                    vx_float32 center_x,
                                    vx_float32 center_y)
{
    vx_status status = VX_FAILURE;
    vx_float32 mat[3][2];
    vx_size columns = 0ul, rows = 0ul;
    vx_enum type = 0;
    vxQueryMatrix(matrix, VX_MATRIX_ATTRIBUTE_COLUMNS, &columns, sizeof(columns));
    vxQueryMatrix(matrix, VX_MATRIX_ATTRIBUTE_ROWS, &rows, sizeof(rows));
    vxQueryMatrix(matrix, VX_MATRIX_ATTRIBUTE_TYPE, &type, sizeof(type));
    if ((columns == 2) && (rows == 3) && (type == VX_TYPE_FLOAT32))
    {
        status = vxReadMatrix(matrix, mat);
        if (status == VX_SUCCESS)
        {
            vx_float32 radians = (angle / 360.0f) * (vx_float32)VX_TAU;
            vx_float32 a = scale * (vx_float32)cos(radians);
            vx_float32 b = scale * (vx_float32)sin(radians);
            mat[0][0] = a;
            mat[1][0] = b;
            mat[2][0] = ((1.0f - a) * center_x) - (b * center_y);
            mat[0][1] = -b;
            mat[1][1] = a;
            mat[2][1] = (b * center_x) + ((1.0f - a) * center_y);
            status = vxWriteMatrix(matrix, mat);
        }
    }
    else
    {
        vxAddLogEntry((vx_reference)matrix, status, "Failed to set affine matrix due to type or dimension mismatch!\n");
    }
    return status;
}

