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

#define LOG_TAG "ExynosVisionParameter"
#include <cutils/log.h>

#include "ExynosVisionParameter.h"

#include "ExynosVisionKernel.h"
#include "ExynosVisionNode.h"

namespace android {

ExynosVisionParameter::ExynosVisionParameter(ExynosVisionContext *context, ExynosVisionReference *scope)
                                                                    : ExynosVisionReference(context, VX_TYPE_PARAMETER, scope, vx_true_e)
{
    m_index = 0xFFFFFFFF;
    m_node = NULL;
    m_kernel = NULL;
}

/* Destructor */
ExynosVisionParameter::~ExynosVisionParameter(void)
{

}

void
ExynosVisionParameter::init(vx_uint32 index, ExynosVisionNode *node, ExynosVisionKernel *kernel)
{
    m_index = index;
    m_node = node;
    if (node != NULL)
        node->incrementReference(VX_REF_INTERNAL, this);

    m_kernel = kernel;
    if (kernel != NULL)
        kernel->incrementReference(VX_REF_INTERNAL, this);
}

vx_status
ExynosVisionParameter::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_node)
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_node, VX_REF_INTERNAL, this);

    if (m_kernel)
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_kernel, VX_REF_INTERNAL, this);

    if (status != VX_SUCCESS)
        VXLOGE("destroying pamameter fails, err:%d", status);

    return status;
}

vx_status
ExynosVisionParameter::queryParameter(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;
    switch (attribute) {
    case VX_PARAMETER_ATTRIBUTE_DIRECTION:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_kernel->getParamDirection(m_index);
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PARAMETER_ATTRIBUTE_INDEX:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_index;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PARAMETER_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_kernel->getParamType(m_index);
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PARAMETER_ATTRIBUTE_STATE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = (vx_enum)m_kernel->getParamState(m_index);
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PARAMETER_ATTRIBUTE_REF:
        if (VX_CHECK_PARAM(ptr, size, vx_reference, 0x3)) {
            if (m_node) {
                ExynosVisionDataReference *ref = m_node->getDataRefByIndex(m_index);
                /* does this object have USER access? */
                if (ref) {
                    ref->incrementReference(VX_REF_EXTERNAL);
                }
                *(vx_reference *)ptr = (vx_reference)ref;
            } else {
                status = VX_ERROR_NOT_SUPPORTED;
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

void
ExynosVisionParameter::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGD("%s[Paramet][%d] %s,  kernel:%s, node(%d), index(%d), refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(),
                                                                                                    m_kernel?m_kernel->getKernelName():"",
                                                                                                    m_node?m_node->getId():0,
                                                                                                    m_index,
                                                                                                    getInternalCnt(), getExternalCnt());
}

}; /* namespace android */
