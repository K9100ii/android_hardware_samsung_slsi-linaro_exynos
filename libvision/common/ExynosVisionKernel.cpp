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

#define LOG_TAG "ExynosVisionKernel"
#include <cutils/log.h>

#include "ExynosVisionKernel.h"

#include "ExynosVisionNode.h"

namespace android {

using namespace std;

ExynosVisionKernel::ExynosVisionKernel(ExynosVisionContext *context)
                                                                    : ExynosVisionReference(context, VX_TYPE_KERNEL, (ExynosVisionReference*)context, vx_true_e)
{
    memset(m_kernel_name, 0x0, sizeof(m_kernel_name));
    m_enumeration = 0;
    m_function = 0;
    memset(&m_signature, 0x0, sizeof(m_signature));

    m_enabled = vx_false_e;

    m_validate_input = 0;
    m_validate_output = 0;
    m_initialize = 0;
    m_deinitialize = 0;
    memset(&m_attributes, 0x0, sizeof(m_attributes));
}

ExynosVisionKernel::~ExynosVisionKernel()
{

}

vx_status
ExynosVisionKernel::destroy(void)
{
    return VX_SUCCESS;
}

vx_status
ExynosVisionKernel::assignFunc(const vx_char name[VX_MAX_KERNEL_NAME],
                                     vx_enum enumeration,
                                     vx_kernel_f func_ptr,
                                     vx_uint32 numParams,
                                     vx_kernel_input_validate_f input,
                                     vx_kernel_output_validate_f output,
                                     vx_kernel_initialize_f init,
                                     vx_kernel_deinitialize_f deinit)
{
    strncpy(m_kernel_name, name, VX_MAX_KERNEL_NAME);
    m_enumeration = enumeration;
    m_function = func_ptr;
    m_signature.num_parameters = numParams;
    m_validate_input = input;
    m_validate_output = output;
    m_initialize = init;
    m_deinitialize = deinit;
    m_attributes.borders.mode = VX_BORDER_MODE_UNDEFINED;
    m_attributes.borders.constant_value = 0;

    return VX_SUCCESS;
}

vx_status
ExynosVisionKernel::setKernelAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    if (m_enabled == vx_true_e)
        return VX_ERROR_NOT_SUPPORTED;

    switch (attribute) {
    case VX_KERNEL_ATTRIBUTE_LOCAL_DATA_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            m_attributes.localDataSize = *(vx_size *)ptr;
            VXLOGD("Set Local Data Size to %d bytes", m_attributes.localDataSize);
        } else  {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_KERNEL_ATTRIBUTE_LOCAL_DATA_PTR:
        if (VX_CHECK_PARAM(ptr, size, vx_ptr_t, 0x1))
            m_attributes.localDataPtr = *(vx_ptr_t *)ptr;
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
ExynosVisionKernel::queryKernel(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_KERNEL_ATTRIBUTE_PARAMETERS:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = getNumParams();
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_KERNEL_ATTRIBUTE_NAME:
        if (ptr != NULL && size == VX_MAX_KERNEL_NAME) {
            vx_char kname[VX_MAX_KERNEL_NAME];
            vx_char *k;
            strncpy(kname, m_kernel_name, VX_MAX_KERNEL_NAME);
            k = strtok(kname, ":");
            strncpy((char*)ptr, k, VX_MAX_KERNEL_NAME);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_KERNEL_ATTRIBUTE_ENUM:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_enumeration;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_KERNEL_ATTRIBUTE_LOCAL_DATA_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_attributes.localDataSize;
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
ExynosVisionKernel::addParameterToKernel(vx_uint32 index, vx_enum dir, vx_enum data_type, vx_enum state)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    VXLOGD2("INFO: %s, Adding index %u, data_type 0x%X, dir:%d state:0x%X\n", getKernelName(), index, data_type, dir, state);

    if (index < m_signature.num_parameters) {
        if ((vxIsValidType(data_type) == vx_false_e) ||
            (vxIsValidDirection(dir) == vx_false_e) ||
            (vxIsValidState(state) == vx_false_e)) {
            status = VX_ERROR_INVALID_PARAMETERS;
            VXLOGE("%s, Invalid parameter, type:%p, direction:%p, state:%p", getKernelName(), data_type, dir, state);
        } else {
            m_signature.directions[index] = dir;
            m_signature.types[index] = data_type;
            m_signature.states[index] = state;
            status = VX_SUCCESS;
        }
    }
    else
    {
        status = VX_ERROR_INVALID_PARAMETERS;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

ExynosVisionParameter*
ExynosVisionKernel::getKernelParameterByIndex(vx_uint32 index)
{
    EXYNOS_VISION_SYSTEM_IN();
    ExynosVisionParameter *param = NULL;

    if (index < VX_INT_MAX_PARAMS && index < getNumParams()) {
        param = new ExynosVisionParameter(getContext(), getContext());
        param->init(index, NULL, this);
    } else {
        getContext()->addLogEntry(this, VX_ERROR_INVALID_PARAMETERS, "Index %u out of range for kernel %s (numparams = %u)!\n",
                index, m_kernel_name, getNumParams());
        VXLOGE("index:%d is out of bound at %s", getName());
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return param;
}

vx_status
ExynosVisionKernel::finalizeKernel(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;

    vx_uint32 p = 0;
    for (p = 0; p < VX_INT_MAX_PARAMS; p++)
    {
        if (p >= m_signature.num_parameters)
        {
            break;
        }
        if ((m_signature.directions[p] < VX_INPUT) ||
            (m_signature.directions[p] > VX_BIDIRECTIONAL))
        {
            status = VX_ERROR_INVALID_PARAMETERS;
            VXLOGE("invalid parameter");
            break;
        }
        if (vxIsValidType(m_signature.types[p]) == vx_false_e)
        {
            status = VX_ERROR_INVALID_PARAMETERS;
            VXLOGE("invalid type");
            break;
        }
    }

    if (p == m_signature.num_parameters)
    {
        m_enabled = vx_true_e;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionKernel::validateInput(ExynosVisionNode *node, vx_uint32 index)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    status = m_validate_input((vx_node)node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail at kernel(%s) from index_%d of %s, err:%d", m_kernel_name, index, node->getName(), status);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionKernel::validateOutput(ExynosVisionNode *node, vx_uint32 index, ExynosVisionMeta *meta)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;
    status = m_validate_output((vx_node)node, index, (vx_meta_format)meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail at kernel(%s) from index_%d of %s, err:%d", m_kernel_name, index, node->getName(), status);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionKernel::initialize(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    if (m_initialize) {
        status = m_initialize((vx_node)node, (vx_reference*)parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("initialization fail at kernel(%s) from %s, err:%d", m_kernel_name, node->getName(), status);
        }
    } else {
        status = VX_SUCCESS;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionKernel::deinitialize(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    if (m_deinitialize) {
        status = m_deinitialize((vx_node)node, (vx_reference*)parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("deinitialization fail at kernel(%s) from %s, err:%d", m_kernel_name, node->getName(), status);
        }
    } else {
        status = VX_SUCCESS;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionKernel::kernelFunction(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num) const
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    if (m_function) {
        status = m_function((vx_node)node, (vx_reference*)parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main function fail at kernel(%s) from %s, err:%d", m_kernel_name, node->getName(), status);
        }
    } else {
        VXLOGE("kernel function is not implemented");
        status = VX_ERROR_NOT_IMPLEMENTED;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

void
ExynosVisionKernel::fiiledAttr(vx_kernel_attr_t *attributes)
{
    Mutex::Autolock lock(m_internal_lock);
    memcpy(attributes, &m_attributes, sizeof(vx_kernel_attr_t));
}

void
ExynosVisionKernel::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Kernel ][%d] %s, name:%s, enum:0x%X, refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(), m_kernel_name, m_enumeration, getInternalCnt(), getExternalCnt());
}

}; /* namespace android */
