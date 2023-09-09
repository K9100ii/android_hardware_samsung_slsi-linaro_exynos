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

#define LOG_TAG "ExynosVisionMatrix"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionMatrix.h"

namespace android {

vx_status
ExynosVisionMatrix::checkValidCreateMatrix(vx_enum data_type, vx_size columns, vx_size rows)
{
    vx_status status = VX_SUCCESS;
    vx_size dim = 0ul;

    if ((data_type == VX_TYPE_INT8) || (data_type == VX_TYPE_UINT8))
        dim = sizeof(vx_uint8);
    else if ((data_type == VX_TYPE_INT16) || (data_type == VX_TYPE_UINT16))
        dim = sizeof(vx_uint16);
    else if ((data_type == VX_TYPE_INT32) || (data_type == VX_TYPE_UINT32) || (data_type == VX_TYPE_FLOAT32))
        dim = sizeof(vx_uint32);
    else if ((data_type == VX_TYPE_INT64) || (data_type == VX_TYPE_UINT64) || (data_type == VX_TYPE_FLOAT64))
        dim = sizeof(vx_uint64);

    if (dim == 0ul) {
        VXLOGE("invalid data type");
        return VX_ERROR_INVALID_TYPE;
    }

    if ((columns == 0ul) || (rows == 0ul)) {
        VXLOGE("invalid dimensions to matrix");
        return VX_ERROR_INVALID_DIMENSION;
    }

    return status;
}

ExynosVisionMatrix::ExynosVisionMatrix(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_MATRIX, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_data_type = 0;
    m_columns = 0;
    m_rows = 0;
    m_element_size = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionMatrix::~ExynosVisionMatrix()
{

}

void
ExynosVisionMatrix::operator=(const ExynosVisionMatrix& src_matrix)
{
    m_data_type = src_matrix.m_data_type;
    m_columns = src_matrix.m_columns;
    m_rows = src_matrix.m_rows;
    m_element_size = src_matrix.m_element_size;
}

vx_status
ExynosVisionMatrix::init(vx_enum data_type, vx_size columns, vx_size rows)
{
    vx_status status = VX_SUCCESS;

    vx_size dim = 0ul;

    if ((data_type == VX_TYPE_INT8) || (data_type == VX_TYPE_UINT8))
        dim = sizeof(vx_uint8);
    else if ((data_type == VX_TYPE_INT16) || (data_type == VX_TYPE_UINT16))
        dim = sizeof(vx_uint16);
    else if ((data_type == VX_TYPE_INT32) || (data_type == VX_TYPE_UINT32) || (data_type == VX_TYPE_FLOAT32))
        dim = sizeof(vx_uint32);
    else if ((data_type == VX_TYPE_INT64) || (data_type == VX_TYPE_UINT64) || (data_type == VX_TYPE_FLOAT64))
        dim = sizeof(vx_uint64);

    m_data_type = data_type;
    m_columns = columns;
    m_rows = rows;
    m_element_size = dim;

    return status;
}

vx_status
ExynosVisionMatrix::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_data_type = 0;
    m_columns = 0;
    m_rows = 0;
    m_element_size = 0;

    status = freeMemory_T<default_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionMatrix::queryMatrix(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_MATRIX_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_data_type;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_MATRIX_ATTRIBUTE_ROWS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_rows;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_MATRIX_ATTRIBUTE_COLUMNS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_columns;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_MATRIX_ATTRIBUTE_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_columns * m_rows * m_element_size;
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
ExynosVisionMatrix::readMatrix(void *array)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    m_internal_lock.lock();
    if (array) {
        vx_size size = m_rows * m_columns * m_element_size;
        vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
        memcpy(array, base_ptr, size);
    }
    m_internal_lock.unlock();

    status = VX_SUCCESS;

    return status;
}

vx_status
ExynosVisionMatrix::writeMatrix(const void *array)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    m_internal_lock.lock();
    if (array) {
        vx_size size = m_rows * m_columns * m_element_size;
        vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
        memcpy(base_ptr, array, size);
    }
    m_internal_lock.unlock();
    status = VX_SUCCESS;

    return status;
}

vx_status
ExynosVisionMatrix::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_true_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionMatrix::allocateResource(default_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    default_resource_t *memory = new default_resource_t();

    vx_size size = m_element_size * m_rows * m_columns;
    status = memory->alloc(m_allocator, size);
    if (status != VX_SUCCESS)
        VXLOGE("memory allocation fails at %s", getName());

    *ret_resource = memory;

    return status;
}

vx_status
ExynosVisionMatrix::freeResource(default_resource_t *memory)
{
    vx_status status = VX_SUCCESS;

    status = memory->free(m_allocator);
    if (status != VX_SUCCESS)
        VXLOGE("memory free fail, err:%d", status);
    delete memory;

    return status;
}

ExynosVisionDataReference*
ExynosVisionMatrix::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionMatrix::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionMatrix::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionMatrix::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionMatrix::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
{
    vx_status status = VX_SUCCESS;
    cl_int err = 0;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating m_cl_memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    m_cl_memory.cl_type = CL_MEM_OBJECT_BUFFER;
    m_cl_memory.nptrs = VX_NUM_DEFAULT_PLANES;
    m_cl_memory.ndims = VX_NUM_DEFAULT_DIMS;

    m_cl_memory.ptrs[VX_CL_PLANE_0] = (vx_uint8 *)m_cur_res->getAddr();
    m_cl_memory.sizes[VX_CL_PLANE_0] = (vx_size)(m_columns * m_rows * m_element_size);
    m_cl_memory.hdls[VX_CL_PLANE_0] = clCreateBuffer(clContext,
                                        CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR,
                                        m_cl_memory.sizes[VX_CL_PLANE_0],
                                        m_cl_memory.ptrs[VX_CL_PLANE_0],
                                        &err);
    if (err != CL_SUCCESS) {
        VXLOGE("failed in clCreateBuffer : err(%d)", err);
        m_cl_memory.allocated = vx_false_e;
        *memory = NULL;
    } else {
        m_cl_memory.allocated = vx_true_e;
        *memory = &m_cl_memory;
    }

EXIT:
    return status;
}
#endif

}; /* namespace android */
