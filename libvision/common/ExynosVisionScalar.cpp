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

#define LOG_TAG "ExynosVisionScalar"
#include <cutils/log.h>

#include "ExynosVisionScalar.h"

#include "ExynosVisionContext.h"

namespace android {

vx_status
ExynosVisionScalar::isValidCreateScalar(ExynosVisionContext *context, vx_enum data_type)
{
    vx_status status = VX_SUCCESS;

    if (!VX_TYPE_IS_SCALAR(data_type)) {
        VXLOGE("Invalid type to scalar, type(0x%X)", data_type);
        context->addLogEntry((ExynosVisionReference*)context, VX_ERROR_INVALID_TYPE, "Invalid type to scalar\n");
        status = VX_ERROR_INVALID_TYPE;
    }

    return status;
}

/* Constructor */
ExynosVisionScalar::ExynosVisionScalar(ExynosVisionContext *context, ExynosVisionReference *scope)
                            : ExynosVisionDataReference(context, VX_TYPE_SCALAR, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_data_type = VX_TYPE_INVALID;
    memset(&m_data, 0x0, sizeof(m_data));

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

/* Destructor */
ExynosVisionScalar::~ExynosVisionScalar(void)
{

}

void
ExynosVisionScalar::operator=(const ExynosVisionScalar& src_scalar)
{
    m_data_type = src_scalar.m_data_type;
    memcpy(&m_data, &src_scalar.m_data, sizeof(m_data));
}

vx_status
ExynosVisionScalar::init(vx_enum data_type, const void *ptr)
{
    vx_status status = VX_SUCCESS;

    m_data_type = data_type;

    /* scalar for reading is created with null pointer */
    if (ptr != NULL) {
        status = writeScalarValue(ptr);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalr value fails, err:%d", status);
        }
    }

    return status;
}

vx_status
ExynosVisionScalar::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    m_data_type = VX_TYPE_INVALID;
    memset(&m_data, 0x0, sizeof(m_data));

    status = freeMemory_T<empty_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionScalar::queryScalar(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_SCALAR_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_data_type;
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
ExynosVisionScalar::readScalarValue(void *ptr)
{
    vx_status status = VX_SUCCESS;

    if (ptr == NULL)
        return VX_ERROR_INVALID_PARAMETERS;

    m_internal_lock.lock();
    switch (m_data_type) {
    case VX_TYPE_CHAR:
        *(vx_char *)ptr = m_data.chr;
        break;
    case VX_TYPE_INT8:
        *(vx_int8 *)ptr = m_data.s08;
        break;
    case VX_TYPE_UINT8:
        *(vx_uint8 *)ptr = m_data.u08;
        break;
    case VX_TYPE_INT16:
        *(vx_int16 *)ptr = m_data.s16;
        break;
    case VX_TYPE_UINT16:
        *(vx_uint16 *)ptr = m_data.u16;
        break;
    case VX_TYPE_INT32:
        *(vx_int32 *)ptr = m_data.s32;
        break;
    case VX_TYPE_UINT32:
        *(vx_uint32 *)ptr = m_data.u32;
        break;
    case VX_TYPE_INT64:
        *(vx_int64 *)ptr = m_data.s64;
        break;
    case VX_TYPE_UINT64:
        *(vx_uint64 *)ptr = m_data.u64;
        break;
#if OVX_SUPPORT_HALF_FLOAT
    case VX_TYPE_FLOAT16:
        *(vx_float16 *)ptr = m_data.f16;
        break;
#endif
    case VX_TYPE_FLOAT32:
        *(vx_float32 *)ptr =m_data.f32;
        break;
    case VX_TYPE_FLOAT64:
        *(vx_float64 *)ptr = m_data.f64;
        break;
    case VX_TYPE_DF_IMAGE:
        *(vx_df_image *)ptr = m_data.fcc;
        break;
    case VX_TYPE_ENUM:
        *(vx_enum *)ptr = m_data.enm;
        break;
    case VX_TYPE_SIZE:
        *(vx_size *)ptr = m_data.size;
        break;
    case VX_TYPE_BOOL:
        *(vx_bool *)ptr = m_data.boolean;
        break;
    default:
        VXLOGE("some case is not covered");
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
    m_internal_lock.unlock();

    return status;
}

vx_status
ExynosVisionScalar::writeScalarValue(const void *ptr)
{
    vx_status status = VX_SUCCESS;

    if (ptr == NULL)
        return VX_ERROR_INVALID_PARAMETERS;

    m_internal_lock.lock();
    switch (m_data_type) {
    case VX_TYPE_CHAR:
        m_data.chr = *(vx_char *)ptr;
        break;
    case VX_TYPE_INT8:
        m_data.s08 = *(vx_int8 *)ptr;
        break;
    case VX_TYPE_UINT8:
        m_data.u08 = *(vx_uint8 *)ptr;
        break;
    case VX_TYPE_INT16:
        m_data.s16 = *(vx_int16 *)ptr;
        break;
    case VX_TYPE_UINT16:
        m_data.u16 = *(vx_uint16 *)ptr;
        break;
    case VX_TYPE_INT32:
        m_data.s32 = *(vx_int32 *)ptr;
        break;
    case VX_TYPE_UINT32:
        m_data.u32 = *(vx_uint32 *)ptr;
        break;
    case VX_TYPE_INT64:
        m_data.s64 = *(vx_int64 *)ptr;
        break;
    case VX_TYPE_UINT64:
        m_data.u64 = *(vx_uint64 *)ptr;
        break;
#if OVX_SUPPORT_HALF_FLOAT
    case VX_TYPE_FLOAT16:
        data.f16 = *(vx_float16 *)ptr;
        break;
#endif
    case VX_TYPE_FLOAT32:
        m_data.f32 = *(vx_float32 *)ptr;
        break;
    case VX_TYPE_FLOAT64:
        m_data.f64 = *(vx_float64 *)ptr;
        break;
    case VX_TYPE_DF_IMAGE:
        m_data.fcc = *(vx_df_image *)ptr;
        break;
    case VX_TYPE_ENUM:
        m_data.enm = *(vx_enum *)ptr;
        break;
    case VX_TYPE_SIZE:
        m_data.size = *(vx_size *)ptr;
        break;
    case VX_TYPE_BOOL:
        m_data.boolean = *(vx_bool *)ptr;
        break;
    default:
        VXLOGE("some case is not covered");
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
    m_internal_lock.unlock();

    return status;
}

vx_status
ExynosVisionScalar::verifyMeta(ExynosVisionMeta *meta)
{
    vx_status status = VX_SUCCESS;

    vx_enum meta_format;
    status |= meta->getMetaFormatAttribute(VX_SCALAR_ATTRIBUTE_TYPE, &meta_format, sizeof(meta_format));

    if (m_data_type != meta_format) {
        status = VX_ERROR_INVALID_TYPE;
        VXLOGE("scalar(%s) has invalid type, scalar:0x%x vs meta:0x%x", getName(), m_data_type, meta_format);
        getContext()->addLogEntry(this, VX_ERROR_INVALID_TYPE, "Scalar contains invalid typed objects\n");
    }

    return status;
}

vx_status
ExynosVisionScalar::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_false_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionScalar::allocateResource(empty_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;

    *ret_resource = NULL;

    return status;
}

vx_status
ExynosVisionScalar::freeResource(empty_resource_t *empty)
{
    vx_status status = VX_SUCCESS;

    if (empty != NULL) {
        VXLOGE("resource is not corrent at %s", getName());
        status = VX_FAILURE;
    }

    return status;
}

ExynosVisionDataReference*
ExynosVisionScalar::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionScalar::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionScalar::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionScalar::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

}; /* namespace android */
