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

#define LOG_TAG "ExynosVisionLut"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionLut.h"

namespace android {

vx_status
ExynosVisionLut::checkValidCreateLut(vx_enum data_type, vx_size count)
{
    vx_status status = VX_SUCCESS;

    if (data_type == VX_TYPE_UINT8) {
        if (count != 256) {
            VXLOGE("invalid parameter to LUT");
            status = VX_ERROR_INVALID_PARAMETERS;
        }
    } else if (data_type == VX_TYPE_INT16) {
    } else {
        VXLOGE("invalid data type, 0x%x", data_type);
        status = VX_ERROR_INVALID_TYPE;
    }

    return status;
}

ExynosVisionLut::ExynosVisionLut(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_LUT, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_data_type = 0;
    m_count = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionLut::~ExynosVisionLut()
{

}

void
ExynosVisionLut::operator=(const ExynosVisionLut& src_lut)
{
    m_data_type = src_lut.m_data_type;
    m_count = src_lut.m_count;
}

vx_status
ExynosVisionLut::init(vx_enum data_type, vx_size count)
{
    vx_status status = VX_SUCCESS;

    m_data_type = data_type;
    m_count = count;

    return status;
}

vx_status
ExynosVisionLut::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    m_data_type = 0;
    m_count = 0;

    status = freeMemory_T<lut_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionLut::queryLut(vx_enum attribute, void *ptr, vx_size size)
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
    case VX_LUT_ATTRIBUTE_TYPE:
        status = m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_ITEMTYPE,  ptr, size);
        break;
    case VX_LUT_ATTRIBUTE_COUNT:
        status = m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_NUMITEMS,  ptr, size);
        break;
    case VX_LUT_ATTRIBUTE_SIZE:
        vx_size item_size;
        vx_size num_items;
        status = m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_ITEMSIZE,  &item_size, sizeof(item_size));
        status |= m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_NUMITEMS,  &num_items, sizeof(num_items));
        if ((VX_CHECK_PARAM(ptr, size, vx_size, 0x3) && (status == VX_SUCCESS)))
            *(vx_size *)ptr = num_items * item_size;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = m_cur_res->queryArray(attribute, ptr, size);
        break;
    }

    return status;
}

vx_status
ExynosVisionLut::accessLut(void **ptr, vx_enum usage)
{
    vx_status status;
    vx_size num_items;
    vx_size stride = 0;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_NUMITEMS,  &num_items, sizeof(num_items));
    if (status != VX_SUCCESS) {
        VXLOGE("querying number of items fails at %s", getName());
        goto EXIT;
    }

    status = m_cur_res->accessArrayRange(0, num_items, &stride,  ptr, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array fails at %s", getName());
        goto EXIT;
    }

EXIT:
    return status;
}


vx_status
ExynosVisionLut::accessLutHandle(vx_int32 *fd, vx_enum usage)
{
    vx_status status = VX_FAILURE;

    /* bad parameters */
    if ((usage < VX_READ_ONLY) || (VX_READ_AND_WRITE < usage) ||
        (fd == NULL)) {
        VXLOGE("invalid param of %s, fd:%p, usage:0x%x", getName(), fd, usage);

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

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->accessArrayHandle(fd, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array fails at %s", getName());
    }

EXIT:

    return status;
}

vx_status
ExynosVisionLut::commitLut(const void *ptr)
{
    vx_status status;
    vx_size num_items;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->queryArray(VX_ARRAY_ATTRIBUTE_NUMITEMS,  &num_items, sizeof(num_items));
    if (status != VX_SUCCESS) {
        VXLOGE("querying number of items fails at %s", getName());
        goto EXIT;
    }

    status = m_cur_res->commitArrayRange(0, num_items, ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting array fails at %s", getName());
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVisionLut::commitLutHandle(const vx_int32 fd)
{
    vx_status status;

    if (m_is_allocated != vx_true_e) {
        status = ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("memory is not allocated yet");
            goto EXIT;
        }
    }

    status = m_cur_res->commitArrayHandle(fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting array fails at %s", getName());
    }

EXIT:
    return status;
}

vx_status
ExynosVisionLut::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_false_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionLut::allocateResource(lut_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionArray *array = new ExynosVisionArray(getContext(), this, vx_false_e);
    status = array->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("array object creation fails, err:%d", status);
        delete array;
        goto EXIT;
    }

    status = array->init(m_data_type, m_count);
    if (status != VX_SUCCESS) {
        VXLOGE("array object init fails, err:%d", status);
        delete array;
        goto EXIT;
    }

    status = array->addEmptyArrayItems(m_count);
    if (status != VX_SUCCESS) {
        VXLOGE("array object adding item fails, err:%d", status);
        delete array;
        goto EXIT;
    }

    *ret_resource = array;
    array->incrementReference(VX_REF_INTERNAL, this);

EXIT:

    return status;
}

vx_status
ExynosVisionLut::freeResource(lut_resource_t *array)
{
    vx_status status = VX_SUCCESS;

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&array, VX_REF_INTERNAL, this);
    if (status != VX_SUCCESS)
        VXLOGE("release %s fails at %s", array->getName(), this->getName());

    return status;
}

ExynosVisionDataReference*
ExynosVisionLut::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<lut_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionLut::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<lut_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionLut::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<lut_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionLut::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<lut_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionLut::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
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
