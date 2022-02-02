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

#define LOG_TAG "ExynosVisionRemap"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionRemap.h"

namespace android {

vx_status
ExynosVisionRemap::checkValidCreateRemap(vx_uint32 src_width, vx_uint32 src_height, vx_uint32 dst_width, vx_uint32 dst_height)
{
    vx_status status = VX_SUCCESS;

    if (src_width == 0 || src_height == 0 || dst_width == 0 || dst_height == 0) {
        VXLOGE("Invalid parameters to remap, src:%dx%d, dst:%dx%d", src_width, src_height, dst_width, dst_height);
        status = VX_ERROR_INVALID_PARAMETERS;
    }

    return status;
}

ExynosVisionRemap::ExynosVisionRemap(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_REMAP, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_src_width = 0;
    m_src_height = 0;
    m_dst_width = 0;
    m_dst_height = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionRemap::~ExynosVisionRemap()
{

}

void
ExynosVisionRemap::operator=(const ExynosVisionRemap& src_remap)
{
    m_src_width = src_remap.m_src_width;
    m_src_height = src_remap.m_src_height;
    m_dst_width = src_remap.m_dst_width;
    m_dst_height = src_remap.m_dst_height;
}

vx_status
ExynosVisionRemap::init(vx_uint32 src_width, vx_uint32 src_height, vx_uint32 dst_width, vx_uint32 dst_height)
{
    vx_status status = VX_SUCCESS;

    m_src_width = src_width;
    m_src_height = src_height;
    m_dst_width = dst_width;
    m_dst_height = dst_height;

    return status;
}

vx_status
ExynosVisionRemap::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_src_width = 0;
    m_src_height = 0;
    m_dst_width = 0;
    m_dst_height = 0;

    status = freeMemory_T<default_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

void*
ExynosVisionRemap::formatMemoryPtr(vx_uint32 dst_x, vx_uint32 dst_y, vx_uint32 pair_index)
{
    vx_uint32 elemement_size = sizeof(vx_float32);
    vx_uint32 offset = ((elemement_size*2)*m_dst_width) * dst_y +
                                 (elemement_size*2) * dst_x +
                                 elemement_size * pair_index;

    vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
    void *ptr = (void *)&base_ptr[offset];

    return ptr;
}

vx_status
ExynosVisionRemap::queryRemap(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_REMAP_ATTRIBUTE_SOURCE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_src_width;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_REMAP_ATTRIBUTE_SOURCE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_src_height;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_REMAP_ATTRIBUTE_DESTINATION_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_dst_width;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_REMAP_ATTRIBUTE_DESTINATION_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_dst_height;
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
ExynosVisionRemap::setRemapPoint(vx_uint32 dst_x, vx_uint32 dst_y, vx_float32 src_x, vx_float32 src_y)
{
    vx_status status = VX_FAILURE;

//    VXLOGD("(%d, %d) <- (%f, %f)", dst_x, dst_y, src_x, src_y);

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    if ((dst_x < m_dst_width) && (dst_y < m_dst_height)) {
        vx_float32 *coords[] = {
             (vx_float32*)formatMemoryPtr(dst_x, dst_y, 0),
             (vx_float32*)formatMemoryPtr(dst_x, dst_y, 1),
        };
        *coords[0] = src_x;
        *coords[1] = src_y;

        status = VX_SUCCESS;
    } else {
        VXLOGE("Invalid source or destintation values, param:(%d,%d)<-(%f,%f), max:%dx%x<-%dx%d", dst_x, dst_y, src_x, src_y,
                                                                                                                        m_dst_width, m_dst_height, m_src_width, m_src_height);
        status = VX_ERROR_INVALID_VALUE;
    }

EXIT:
    return status;
}

vx_status
ExynosVisionRemap::getRemapPoint(vx_uint32 dst_x, vx_uint32 dst_y, vx_float32 *src_x, vx_float32 *src_y)
{
    vx_status status = VX_FAILURE;

    if ((dst_x < m_dst_width) && (dst_y < m_dst_height)) {
        vx_float32 *coords[] = {
             (vx_float32*)formatMemoryPtr(dst_x, dst_y, 0),
             (vx_float32*)formatMemoryPtr(dst_x, dst_y, 1),
        };
        *src_x = *coords[0];
        *src_y = *coords[1];
        status = VX_SUCCESS;
    } else {
        VXLOGE("Invalid source or destintation values, param:(%d,%d)<-(%f,%f), max:%dx%x<-%dx%d", dst_x, dst_y, src_x, src_y,
                                                                                                                        m_dst_width, m_dst_height, m_src_width, m_src_height);
        status = VX_ERROR_INVALID_VALUE;
    }

//    VXLOGD("(%d, %d) <- (%f, %f)", dst_x, dst_y, *src_x, *src_y);

    return status;
}

vx_status
ExynosVisionRemap::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_true_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionRemap::allocateResource(default_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    default_resource_t *memory = new default_resource_t();

    /* 2d array of floating pair */
    vx_size size = m_dst_width * m_dst_height * sizeof(vx_float32)*2;
    status = memory->alloc(m_allocator, size);
    if (status != VX_SUCCESS)
        VXLOGE("memory allocation fails at %s", getName());

    *ret_resource = memory;

    return status;
}

vx_status
ExynosVisionRemap::freeResource(default_resource_t *memory)
{
    vx_status status = VX_SUCCESS;

    status = memory->free(m_allocator);
    if (status != VX_SUCCESS)
        VXLOGE("memory free fail, err:%d", status);
    delete memory;

    return status;
}

ExynosVisionDataReference*
ExynosVisionRemap::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionRemap::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionRemap::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionRemap::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionRemap::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
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
    m_cl_memory.ndims = VX_NUM_REMAP_DIMS;

    m_cl_memory.ptrs[VX_CL_PLANE_0] = (vx_uint8 *)m_cur_res->getAddr();
    m_cl_memory.sizes[VX_CL_PLANE_0] = (vx_size)(m_dst_width * m_dst_height * sizeof(vx_float32)*2);
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
