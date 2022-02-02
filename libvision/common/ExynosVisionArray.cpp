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

#define LOG_TAG "ExynosVisionArray"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionArray.h"

#include "ExynosVisionContext.h"
#include "ExynosVisionDelay.h"

namespace android {

static vx_size vxArrayItemSize(ExynosVisionContext *context, vx_enum item_type)
{
    vx_size res = ExynosVisionReference::sizeOfType(item_type);
    if (res == 0ul)
        res = context->getUserStructsSize(item_type);

    return res;
}

vx_status
ExynosVisionArray::checkValidCreateArray(ExynosVisionContext *context, vx_enum item_type, vx_size capacity)
{
    vx_status status = VX_SUCCESS;
    if (vxArrayItemSize(context, item_type) == 0) {
        VXLOGE("creating array's parameter is not valid, item_type(0x%X), capacity(%d)", item_type, capacity);
        status = VX_ERROR_INVALID_PARAMETERS;
    }

    return status;
}

ExynosVisionArray::ExynosVisionArray(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual)
                                                               : ExynosVisionDataReference(context, VX_TYPE_ARRAY, scope, vx_true_e, is_virtual)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_item_type = VX_TYPE_INVALID;
    m_item_size = 0;
    m_num_items = 0;
    m_capacity = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionArray::~ExynosVisionArray()
{

}

void
ExynosVisionArray::operator=(const ExynosVisionArray& src_array)
{
    m_item_type = src_array.m_item_type;
    m_item_size = src_array.m_item_size;
    m_capacity = src_array.m_capacity;
}

vx_status
ExynosVisionArray::init(vx_enum item_type, vx_size capacity)
{
    vx_status status = VX_SUCCESS;

    m_item_type = item_type;
    m_item_size = vxArrayItemSize(getContext(), item_type);
    m_capacity = capacity;

    return status;
}

vx_status
ExynosVisionArray::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    memset(&m_memory, 0x0, sizeof(m_memory));

    m_item_type = VX_TYPE_INVALID;
    m_item_size = 0;
    m_num_items = 0;
    m_capacity = 0;

    status = freeMemory_T<default_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionArray::queryArray(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_ARRAY_ATTRIBUTE_ITEMTYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_item_type;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_ARRAY_ATTRIBUTE_NUMITEMS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_num_items;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_ARRAY_ATTRIBUTE_CAPACITY:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_capacity;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_ARRAY_ATTRIBUTE_ITEMSIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_item_size;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_ARRAY_ATTRIBUTE_STRIDE:
        if(m_memory.external_stride != 0)
            *(vx_size *)ptr = m_memory.external_stride;
        else
            *(vx_size *)ptr = m_item_size;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionArray::truncateArray(vx_size new_num_items)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (new_num_items <= m_num_items) {
        m_num_items = new_num_items;

        status = VX_SUCCESS;
    }

    return status;
}

vx_bool
ExynosVisionArray::initVirtualArray(vx_enum item_type, vx_size capacity)
{
    vx_bool res = vx_false_e;
    vx_size item_size = vxArrayItemSize(getContext(), item_type);
    if ((item_size != 0) &&
        ((m_capacity > 0 || capacity > 0) && (capacity <= m_capacity || m_capacity == 0))) {
        if ((m_item_type == VX_TYPE_INVALID) || (m_item_type == item_type)) {
            m_item_type = item_type;
            m_item_size = item_size;

            if (m_capacity == 0)
                m_capacity = capacity;

            res = vx_true_e;
        }
    }
    return res;
}

vx_bool
ExynosVisionArray::validateArray(vx_enum item_type, vx_size capacity)
{
    vx_bool res = vx_false_e;
    vx_size item_size = vxArrayItemSize(getContext(), item_type);
    if ((item_size != 0) && (m_item_type == item_type)) {
        /* if the required capacity is > 0 and the objects capacity is not sufficient */
        if ((capacity > 0) && (capacity > m_capacity)) {
            res = vx_false_e;
            VXLOGE("validating error, capacity:%d, m_capacity:%d", capacity, m_capacity);
        } else {
            res = vx_true_e;
        }
    } else {
        VXLOGE("validating error, item_size:%d, m_item_type:0x%x, item_type:0x%x", item_size, m_item_type, item_type);
    }

    return res;
}

vx_status
ExynosVisionArray::verifyMeta(ExynosVisionMeta *meta)
{
    vx_status status = VX_SUCCESS;

    vx_enum item_type;
    vx_size capacity;

    status |= meta->getMetaFormatAttribute(VX_ARRAY_ATTRIBUTE_ITEMTYPE, &item_type, sizeof(item_type));
    status |= meta->getMetaFormatAttribute(VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
    if (status != VX_SUCCESS) {
        VXLOGE("get meta fail, err:%d", status);
    }

    if (isVirtual()) {
        if (initVirtualArray(item_type, capacity) != vx_true_e) {
            status = VX_ERROR_INVALID_DIMENSION;
            getContext()->addLogEntry(this, VX_ERROR_INVALID_DIMENSION,
                "meta[%u] has an invalid item type 0x%08x or capacity %d\n", this, item_type, capacity);
            VXLOGE("meta[%u] has an invalid item type 0x%08x or capacity %d", this, item_type, capacity);
        }
    } else {
        if (validateArray(item_type, capacity) != vx_true_e) {
            status = VX_ERROR_INVALID_DIMENSION;
            getContext()->addLogEntry(this, VX_ERROR_INVALID_DIMENSION,
                "invalid item type 0x%08x or capacity %d\n", m_item_type, m_capacity);
            VXLOGE("invalid item type 0x%x or capacity %d", m_item_type, m_capacity);
        }
    }

    return status;
}

vx_status
ExynosVisionArray::addArrayItems(vx_size count, const void *ptr, vx_size stride)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    if ((count > 0) && (ptr != NULL) && (stride == 0 || stride >= m_item_size)) {
        if (m_num_items + count <= m_capacity) {
            vx_size offset = m_num_items * m_item_size;
            vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
            vx_uint8 *dst_ptr = (vx_uint8*)&base_ptr[offset];

            if (stride == 0 || stride == m_item_size) {
                memcpy(dst_ptr, ptr, count * m_item_size);
            } else {
                vx_size i;
                for (i = 0; i < count; ++i) {
                    vx_uint8 *tmp = (vx_uint8 *)ptr;
                    memcpy(&dst_ptr[i * m_item_size], &tmp[i * stride], m_item_size);
                }
            }

            m_num_items += count;

            status = VX_SUCCESS;
        } else {
            status = VX_FAILURE;
        }
    } else {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVisionArray::addEmptyArrayItems(vx_size count)
{
    vx_status status = VX_FAILURE;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    if (count > 0) {
        if (m_num_items + count <= m_capacity) {
            m_num_items += count;

            status = VX_SUCCESS;
        } else {
            VXLOGE("adding empty array fails, count:%d, m_num_items:%d, m_capacity:%d", count, m_num_items, m_capacity);
            status = VX_FAILURE;
        }
    } else {
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVisionArray::accessArrayRange(vx_size start, vx_size end, vx_size *stride, void **ptr, vx_enum usage)
{
    vx_status status = VX_FAILURE;

    /* bad parameters */
    if ((usage < VX_READ_ONLY) || (VX_READ_AND_WRITE < usage) ||
        (ptr == NULL) ||
        (stride == NULL) ||
        (start >= end) || (end > m_num_items)) {
        VXLOGE("invalid param of %s, start:%d, end:%d, num_items:%d, stride:%p, ptr:%p, usage:0x%x", getName(), start, end, m_num_items, stride, ptr, usage);

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

    /* verify has not run or will not run yet. this allows this API to "touch"
     * the array to create it.
     */
    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    m_memory.usage = usage;
    if (usage != VX_READ_ONLY) {
        m_memory_lock.lock();
    }

    /* POSSIBILITIES:
     * 1.) !*ptr && RO == COPY-ON-READ (make ptr=alloc)
     * 2.) !*ptr && WO == MAP
     * 3.) !*ptr && RW == MAP
     * 4.)  *ptr && RO||RW == COPY (UNLESS MAP)
     */

    /* MAP mode */
    if (*ptr == NULL) {
        m_memory.access_mode = ACCESS_MAP_MODE;
        m_memory.external_stride = 0;
        *stride = m_item_size;

        vx_size offset = start * m_item_size;

        vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
        *ptr = (void*)&base_ptr[offset];

        incrementReference(VX_REF_EXTERNAL);

        status = VX_SUCCESS;
    } else {
        m_memory.access_mode = ACCESS_COPY_MODE;
        m_memory.external_stride = *stride;

        /* COPY mode */
        vx_size size = ((end - start) * m_item_size);

        if (usage != VX_WRITE_ONLY) {
            vx_uint32 i;
            vx_uint8 *pSrc, *pDest;

            vx_uint8 *base_ptr = (vx_uint8 *)m_cur_res->getAddr();
            for (i = start, pDest = (vx_uint8*)*ptr, pSrc = (vx_uint8*)&base_ptr[start * m_item_size];
                i < end;
                i++, pDest += *stride, pSrc += m_item_size) {
                memcpy(pDest, pSrc, m_item_size);
            }
        }

        incrementReference(VX_REF_EXTERNAL);

        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVisionArray::accessArrayHandle(vx_int32 *fd, vx_enum usage)
{
    vx_status status = VX_FAILURE;

    /* bad parameters */
    if ((usage < VX_READ_ONLY) || (VX_READ_AND_WRITE < usage) ||
        (fd == NULL)) {
        VXLOGE("invalid param of %s, num_items:%d, fd:%p, usage:0x%x", getName(), m_num_items, fd, usage);

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

    /* verify has not run or will not run yet. this allows this API to "touch"
     * the array to create it.
     */
    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS)
            return status;
    }

    m_memory.usage = usage;
    if (usage != VX_READ_ONLY) {
        m_memory_lock.lock();
    }

    m_memory.access_mode = ACCESS_MAP_MODE;
    m_memory.external_stride = 0;

    *fd = m_cur_res->getFd();

    incrementReference(VX_REF_EXTERNAL);

    status = VX_SUCCESS;

    return status;
}

vx_status
ExynosVisionArray::commitArrayRange(vx_size start, vx_size end, const void *ptr)
{
    VXLOGD2("%s, start%d, end:%d, ptr:%p", getName(), start, end, ptr);

    vx_status status = VX_ERROR_INVALID_REFERENCE;

    vx_bool external = vx_true_e; // assume that it was an allocated buffer

    if ((ptr == NULL) ||
        (start > end) || (end > m_num_items)) {
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

    /* VARIABLES:
     * 1.) ZERO_AREA
     * 2.) CONSTANT - independant
     * 3.) INTERNAL - independant of area
     * 4.) EXTERNAL - dependant on area (do nothing on zero, determine on non-zero)
     * 5.) !INTERNAL && !EXTERNAL == MAPPED
     */

    /* check to see if the range is zero area */
    vx_bool zero_area = (end == 0) ? vx_true_e : vx_false_e;
    vx_uint32 index = UINT32_MAX; // out of bounds, if given to remove, won't do anything

    if (zero_area == vx_false_e) {
        /* this could be a write-back */
        if ((m_memory.usage != VX_READ_ONLY) && (m_memory.access_mode == ACCESS_COPY_MODE)) {
            vx_uint8 *beg_ptr = (vx_uint8 *)m_cur_res->getAddr();
            vx_uint8 *end_ptr = &beg_ptr[m_item_size * m_num_items];

            /* the pointer was not mapped, copy. */
            vx_size offset = start * m_item_size;
            vx_size len = (end - start) * m_item_size;

            if (m_memory.external_stride == 0) {
                VXLOGE("external stride information is empty at copy mode");
                status = VX_FAILURE;
                goto EXIT;
            }

            if (m_memory.external_stride != m_item_size) {
                vx_size stride = m_memory.external_stride;

                vx_uint32 i;
                const vx_uint8 *pSrc; vx_uint8 *pDest;

                for (i = start, pSrc = (vx_uint8*)ptr, pDest= &beg_ptr[offset];
                     i < end;
                     i++, pSrc += stride, pDest += m_item_size) {
                    memcpy(pDest, pSrc, m_item_size);
                }

            } else {
                memcpy(&beg_ptr[offset], ptr, len);
            }
        }

        status = VX_SUCCESS;
    }
    else {
        status = VX_SUCCESS;
    }

    if (m_memory.usage != VX_READ_ONLY)
        m_memory_lock.unlock();

    m_memory.access_mode = ACCESS_NONE;
    decrementReference(VX_REF_EXTERNAL);

EXIT:
    return status;
}

vx_status
ExynosVisionArray::commitArrayHandle(const vx_int32 fd)
{
    vx_status status = VX_ERROR_INVALID_REFERENCE;

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

    status = VX_SUCCESS;

    if (m_memory.usage != VX_READ_ONLY)
        m_memory_lock.unlock();

    m_memory.access_mode = ACCESS_NONE;
    decrementReference(VX_REF_EXTERNAL);

    return status;
}

vx_status
ExynosVisionArray::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_true_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionArray::allocateResource(default_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    default_resource_t *memory = new default_resource_t();

    vx_size size = m_item_size * m_capacity;
    status = memory->alloc(m_allocator, size);
    if (status != VX_SUCCESS)
        VXLOGE("memory allocation fails at %s", getName());

    *ret_resource = memory;

    return status;
}

vx_status
ExynosVisionArray::freeResource(default_resource_t *memory)
{
    vx_status status = VX_SUCCESS;

    status = memory->free(m_allocator);
    if (status != VX_SUCCESS)
        VXLOGE("memory free fail, err:%d", status);
    delete memory;

    return status;
}

ExynosVisionDataReference*
ExynosVisionArray::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionArray::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionArray::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionArray::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<default_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

void
ExynosVisionArray::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Array  ] %s(%p), type:0x%x, size:%d, num:%d, cappa:%d, virtual:%d, refCnt:%d/%d", MAKE_TAB(tap, tab_num), getName(), this,
                                                m_item_type, m_item_size, m_num_items, m_capacity, isVirtual(),
                                                getInternalCnt(), getExternalCnt());
    if (isDelayElement())
        VXLOGI("%s          delay element, %s, delay phy index:%d", MAKE_TAB(tap, tab_num), getDelay()->getName(), getDelaySlotIndex());

    LOG_FLUSH_TIME();

    if (detail_info)
        displayConn(tab_num);
}
#ifdef USE_OPENCL_KERNEL
vx_status
ExynosVisionArray::getClMemoryInfo(cl_context clContext, vxcl_mem_t **memory)
{
    vx_status status = VX_SUCCESS;
    cl_int err = 0;

    if (m_is_allocated != vx_true_e) {
        status =  ((ExynosVisionDataReference*)this)->allocateMemory();
        if (status != VX_SUCCESS) {
            VXLOGE("allocating memory fails at %s, err:%d", getName(), status);
            goto EXIT;
        }
    }

    m_cl_memory.cl_type = CL_MEM_OBJECT_BUFFER;
    m_cl_memory.nptrs = VX_NUM_DEFAULT_PLANES;
    m_cl_memory.ndims = VX_NUM_DEFAULT_DIMS;

    m_cl_memory.ptrs[VX_CL_PLANE_0] = (vx_uint8 *)m_cur_res->getAddr();
    m_cl_memory.sizes[VX_CL_PLANE_0] = (vx_size)(m_item_size * m_capacity);
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
