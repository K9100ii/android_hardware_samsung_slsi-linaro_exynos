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

#define LOG_TAG "ExynosVisionConvolution"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionConvolution.h"

namespace android {

static int isodd(size_t a)
{
    return (int)(a & 1);
}

static vx_bool vxIsPowerOfTwo(vx_uint32 a)
{
    if (a == 0)
        return vx_false_e;
    else if ((a & ((a) - 1)) == 0)
        return vx_true_e;
    else
        return vx_false_e;
}

vx_status
ExynosVisionConvolution::checkValidCreateConvolution(vx_size columns, vx_size rows)
{
    vx_status status = VX_SUCCESS;

    if (!(isodd(columns) && columns >= 3 && isodd(rows) && rows >= 3)) {
        VXLOGE("failed to create convolution, invalid dimensions");
        status = VX_ERROR_INVALID_DIMENSION;
    }

    return status;
}

ExynosVisionConvolution::ExynosVisionConvolution(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_CONVOLUTION, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_columns = 0;
    m_rows = 0;
    m_scale = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionConvolution::~ExynosVisionConvolution()
{

}

void
ExynosVisionConvolution::operator=(const ExynosVisionConvolution& src_conv)
{
    m_columns = src_conv.m_columns;
    m_rows = src_conv.m_rows;

    m_scale = src_conv.m_scale;
}

vx_status
ExynosVisionConvolution::init(vx_size columns, vx_size rows)
{
    vx_status status = VX_SUCCESS;

    m_columns = columns;
    m_rows = rows;

    m_scale = 1;

    return status;
}

vx_status
ExynosVisionConvolution::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    m_columns = 0;
    m_rows = 0;
    m_scale = 0;

    status = freeMemory_T<convolution_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionConvolution::queryConvolution(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails");
            return VX_FAILURE;
        }
    }

    switch (attribute) {
    case VX_CONVOLUTION_ATTRIBUTE_ROWS:
        m_cur_res->queryMatrix(VX_MATRIX_ATTRIBUTE_ROWS, ptr, size);
        break;
    case VX_CONVOLUTION_ATTRIBUTE_COLUMNS:
        m_cur_res->queryMatrix(VX_MATRIX_ATTRIBUTE_COLUMNS, ptr, size);
        break;
    case VX_CONVOLUTION_ATTRIBUTE_SCALE:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_scale;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_CONVOLUTION_ATTRIBUTE_SIZE:
        vx_size columns;
        vx_size rows;
        status = m_cur_res->queryMatrix(VX_MATRIX_ATTRIBUTE_COLUMNS,  &columns, sizeof(columns));
        status |= m_cur_res->queryMatrix(VX_MATRIX_ATTRIBUTE_ROWS,  &rows, sizeof(rows));
        if ((VX_CHECK_PARAM(ptr, size, vx_size, 0x3) && (status == VX_SUCCESS)))
            *(vx_size *)ptr = columns * rows * sizeof(vx_int16);
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
ExynosVisionConvolution::setConvolutionAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_CONVOLUTION_ATTRIBUTE_SCALE:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            vx_uint32 scale = *(vx_uint32 *)ptr;
            if (vxIsPowerOfTwo(scale) == vx_true_e) {
                VXLOGD3("convolution scale assigned to %u", scale);
                m_scale = scale;
            } else  {
                VXLOGE("convolution scale should be power of two, scale:%d", scale);
                status = VX_ERROR_INVALID_VALUE;
            }
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    default:
        status = VX_ERROR_INVALID_PARAMETERS;
        break;
    }

    return status;
}

vx_status
ExynosVisionConvolution::readConvolutionCoefficients(vx_int16 *array)
{
    vx_status status = VX_SUCCESS;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->readMatrix(array);
    if (status != VX_SUCCESS)
        VXLOGE("read matrix(%s) fails, err:%d", m_cur_res->getName(), status);

EXIT:
    return status;
}

vx_status
ExynosVisionConvolution::writeConvolutionCoefficients(const vx_int16 *array)
{
    vx_status status = VX_SUCCESS;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->writeMatrix(array);
    if (status != VX_SUCCESS)
        VXLOGE("read matrix(%s) fails, err:%d", m_cur_res->getName(), status);

EXIT:
    return status;
}

vx_status
ExynosVisionConvolution::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_false_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionConvolution::allocateResource(convolution_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionMatrix *matrix = new ExynosVisionMatrix(getContext(), this);
    status = matrix->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("matrix object creation fails, err:%d", status);
        delete matrix;
        goto EXIT;
    }

    status = matrix->init(VX_TYPE_INT16, m_columns, m_rows);
    if (status != VX_SUCCESS) {
        VXLOGE("matrix object init fails, err:%d", status);
        delete matrix;
        goto EXIT;
    }

    *ret_resource = matrix;
    matrix->incrementReference(VX_REF_INTERNAL, this);

EXIT:

    return status;
}

vx_status
ExynosVisionConvolution::freeResource(convolution_resource_t *array)
{
    vx_status status = VX_SUCCESS;

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&array, VX_REF_INTERNAL, this);
    if (status != VX_SUCCESS)
        VXLOGE("release %s fails at %s", array->getName(), this->getName());

    return status;
}

ExynosVisionDataReference*
ExynosVisionConvolution::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<convolution_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionConvolution::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<convolution_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionConvolution::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<convolution_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionConvolution::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<convolution_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

#ifdef USE_OPENCL_KERNEL

vx_status
ExynosVisionConvolution::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
{
    vx_status status = VX_SUCCESS;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    status = m_cur_res->getClMemoryInfo(clContext, memory);

EXIT:
    return status;
}
#endif
}; /* namespace android */
