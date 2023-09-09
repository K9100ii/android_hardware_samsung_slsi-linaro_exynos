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

#define LOG_TAG "ExynosVisionDistribution"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionDistribution.h"

namespace android {

vx_status
ExynosVisionDistribution::checkValidCreateDistribution(vx_size numBins, vx_uint32 range)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if ((numBins != 0) && (range != 0)) {
        status = VX_SUCCESS;
    }

    return status;
}

ExynosVisionDistribution::ExynosVisionDistribution(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_DISTRIBUTION, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_memory.usage = 0;

    m_window_x = 0;
    m_offset_x = 0;
    m_range_x = 0;
    m_num_bins = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif

}

ExynosVisionDistribution::~ExynosVisionDistribution()
{

}

void
ExynosVisionDistribution::operator=(const ExynosVisionDistribution& src_dist)
{
    m_num_bins = src_dist.m_num_bins;
    m_window_x = src_dist.m_window_x;
    m_offset_x = src_dist.m_offset_x;
    m_range_x = src_dist.m_range_x;
}

vx_status
ExynosVisionDistribution::init(vx_size numBins, vx_int32 offset, vx_uint32 range)
{
    vx_status status = VX_SUCCESS;

    m_num_bins = numBins;
    m_window_x = (vx_uint32)range/(vx_uint32)numBins;

    m_offset_x = offset;
    m_range_x = range;

    if ((range % numBins) != 0) {
        VXLOGW("range is not multiply of numBins");
        VXLOGW("numBins:%d, offset:%d, range:%d, m_window_x:%d", numBins, offset, range, m_window_x);
    }

    return status;
}

vx_status
ExynosVisionDistribution::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_window_x = 0;
    m_offset_x = 0;
    m_range_x = 0;
    m_num_bins = 0;

    status = freeMemory_T<default_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionDistribution::queryDistribution(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_DISTRIBUTION_ATTRIBUTE_DIMENSIONS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size*)ptr = 1;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_DISTRIBUTION_ATTRIBUTE_RANGE:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32*)ptr = m_range_x;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_DISTRIBUTION_ATTRIBUTE_BINS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size*)ptr = (vx_size)m_num_bins;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_DISTRIBUTION_ATTRIBUTE_WINDOW:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32*)ptr = m_window_x;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_DISTRIBUTION_ATTRIBUTE_OFFSET:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3))
            *(vx_int32*)ptr = m_offset_x;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_DISTRIBUTION_ATTRIBUTE_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size*)ptr = m_num_bins * sizeof(vx_int32);
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
ExynosVisionDistribution::accessDistribution(void **ptr, vx_enum usage)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    if (ptr != NULL) {
        m_memory.usage = usage;
        if (usage != VX_READ_ONLY)
            m_memory_lock.lock();

        m_internal_lock.lock();
        if (*ptr == NULL) {
            vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
            *ptr = base_ptr;
        } else if (*ptr != NULL) {
            vx_size size = m_num_bins * sizeof(vx_int32);
            vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
            memcpy(*ptr, base_ptr, size);
        }
        m_internal_lock.unlock();

        incrementReference(VX_REF_EXTERNAL);
        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVisionDistribution::accessDistributionHandle(vx_int32 *fd, vx_enum usage)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    m_memory.usage = usage;
    if (usage != VX_READ_ONLY)
        m_memory_lock.lock();

    m_internal_lock.lock();
    *fd = m_cur_res->getFd();
    m_internal_lock.unlock();

    incrementReference(VX_REF_EXTERNAL);
    status = VX_SUCCESS;

    return status;
}

vx_status
ExynosVisionDistribution::commitDistribution(const void *ptr)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    if (ptr != NULL) {
        m_internal_lock.lock();
        vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
        if (ptr != base_ptr) {
            vx_size size = m_num_bins * sizeof(vx_int32);
            vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
            memcpy(base_ptr, ptr, size);
            VXLOGD("copied distribution from %p to %p for %d bytes\n", ptr, base_ptr, size);
        }
        m_internal_lock.unlock();

        if (m_memory.usage != VX_READ_ONLY)
            m_memory_lock.unlock();

        decrementReference(VX_REF_EXTERNAL);
        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVisionDistribution::commitDistributionHandle(const vx_int32 fd)
{
    if (fd == 0) {
        return VX_ERROR_INVALID_PARAMETERS;
    }

    /* determine if virtual before checking for memory */
    if (m_is_virtual == vx_true_e) {
        if (m_kernel_count == 0) {
            /* User tried to access a "virtual" array. */
            VXLOGE("Can not access a virtual array");
            return VX_ERROR_OPTIMIZED_AWAY;
        }
        /* framework trying to access a virtual image, this is ok. */
    }

    if (m_memory.usage != VX_READ_ONLY)
        m_memory_lock.unlock();

    decrementReference(VX_REF_EXTERNAL);

    return VX_SUCCESS;
}

vx_status
ExynosVisionDistribution::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_true_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionDistribution::allocateResource(default_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    default_resource_t *memory = new default_resource_t();

    vx_size size = m_num_bins * sizeof(vx_int32);
    status = memory->alloc(m_allocator, size);
    if (status != VX_SUCCESS)
        VXLOGE("memory allocation fails at %s", getName());

    *ret_resource = memory;

    return status;
}

vx_status
ExynosVisionDistribution::freeResource(default_resource_t *memory)
{
    vx_status status = VX_SUCCESS;

    status = memory->free(m_allocator);
    if (status != VX_SUCCESS)
        VXLOGE("memory free fail, err:%d", status);
    delete memory;

    return status;
}

ExynosVisionDataReference*
ExynosVisionDistribution::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionDistribution::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionDistribution::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionDistribution::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionDistribution::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
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
    m_cl_memory.sizes[VX_CL_PLANE_0] = (vx_size)(m_num_bins * sizeof(vx_int32));
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
