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

#define LOG_TAG "ExynosVisionThreshold"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionThreshold.h"

namespace android {

static vx_bool vxIsValidThresholdType(vx_enum thresh_type)
{
    vx_bool ret = vx_false_e;
    if ((thresh_type == VX_THRESHOLD_TYPE_BINARY) ||
        (thresh_type == VX_THRESHOLD_TYPE_RANGE)) {
        ret = vx_true_e;
    }
    return ret;
}

static vx_bool vxIsValidThresholdDataType(vx_enum data_type)
{
    vx_bool ret = vx_false_e;
    if (data_type == VX_TYPE_UINT8) {
        ret = vx_true_e;
    }
    return ret;
}

vx_status
ExynosVisionThreshold::checkValidCreateThreshold(vx_enum thresh_type, vx_enum data_type)
{
    vx_status status = VX_FAILURE;

    if (vxIsValidThresholdDataType(data_type) == vx_true_e) {
        if (vxIsValidThresholdType(thresh_type) == vx_true_e) {
            status = VX_SUCCESS;
        } else {
            VXLOGE("Invalid threshold type(%p)", thresh_type);
            status = VX_ERROR_INVALID_TYPE;
        }
    } else {
        VXLOGE("Invalid data type(%p)", data_type);
        status = VX_ERROR_INVALID_TYPE;
    }

    return status;
}

ExynosVisionThreshold::ExynosVisionThreshold(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                               : ExynosVisionDataReference(context, VX_TYPE_THRESHOLD, scope, vx_true_e, vx_false_e)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_thresh_type = 0;
    m_value = 0;
    m_lower = 0;
    m_upper = 0;
    m_true_value = 0;
    m_false_value = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionThreshold::~ExynosVisionThreshold()
{

}

void
ExynosVisionThreshold::operator=(const ExynosVisionThreshold& src_threshold)
{
    m_thresh_type = src_threshold.m_thresh_type;
}

vx_status
ExynosVisionThreshold::init(vx_enum thresh_type)
{
    m_thresh_type = thresh_type;

    return VX_SUCCESS;
}

vx_status
ExynosVisionThreshold::destroy(void)
{
    vx_status status = VX_SUCCESS;

    m_cur_res = NULL;

    m_thresh_type = 0;
    m_value = 0;
    m_lower = 0;
    m_upper = 0;
    m_true_value = 0;
    m_false_value = 0;

    status = freeMemory_T<empty_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

vx_status
ExynosVisionThreshold::queryThreshold(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_BINARY)) {
            *(vx_int32 *)ptr = m_value;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_LOWER:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_RANGE)) {
            *(vx_int32 *)ptr = m_lower;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_UPPER:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_RANGE)) {
            *(vx_int32 *)ptr = m_upper;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_TRUE_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3)) {
            *(vx_int32 *)ptr = m_true_value;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_FALSE_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3)) {
            *(vx_int32 *)ptr = m_false_value;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            *(vx_enum *)ptr = m_thresh_type;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_DATA_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            *(vx_enum *)ptr = VX_TYPE_UINT8;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionThreshold::setThresholdAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute)  {
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_BINARY)) {
            m_value = *(vx_int32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_LOWER:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_RANGE)) {
            m_lower = *(vx_int32 *)ptr;
        } else  {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_THRESHOLD_UPPER:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3) &&
            (m_thresh_type == VX_THRESHOLD_TYPE_RANGE)) {
            m_upper = *(vx_int32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_TRUE_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3)) {
            m_true_value = *(vx_int32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_FALSE_VALUE:
        if (VX_CHECK_PARAM(ptr, size, vx_int32, 0x3)) {
            m_false_value = *(vx_int32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_THRESHOLD_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            vx_enum thresh_type = *(vx_enum *)ptr;
            if (vxIsValidThresholdType(thresh_type) == vx_true_e) {
                m_thresh_type = thresh_type;
            } else {
                status = VX_ERROR_INVALID_PARAMETERS;
            }
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionThreshold::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_false_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionThreshold::allocateResource(empty_resource_t **ret_resource)
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
ExynosVisionThreshold::freeResource(empty_resource_t *empty)
{
    vx_status status = VX_SUCCESS;

    if (empty != NULL) {
        VXLOGE("resource is not corrent at %s", getName());
        status = VX_FAILURE;
    }

    return status;
}

ExynosVisionDataReference*
ExynosVisionThreshold::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionThreshold::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionThreshold::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionThreshold::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<empty_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

}; /* namespace android */
