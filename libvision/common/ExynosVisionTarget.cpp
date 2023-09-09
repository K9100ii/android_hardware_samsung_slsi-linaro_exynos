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

#define LOG_TAG "ExynosVisionTarget"
#include <cutils/log.h>

#include "ExynosVisionTarget.h"

static vx_target_info_t target_list[] = {
    {VX_TARGET_VPU, "com.samsung.vpu"},
    {VX_TARGET_GPU, "com.samsung.opencl"},
    {VX_TARGET_CPU, "org.khronos.openvx"},
    {VX_TARGET_DSP, "com.samsung.score"},
};

namespace android {

typedef pair <vx_int32, ExynosVisionKernel*> kernel_pair;

/* Constructor */
ExynosVisionTarget::ExynosVisionTarget(ExynosVisionContext *context, const vx_char name[VX_MAX_TARGET_NAME])
                                                                    : ExynosVisionReference(context, VX_TYPE_TARGET, (ExynosVisionReference*)context, vx_false_e)
{
    strncpy(m_target_name, name, VX_MAX_TARGET_NAME);
    m_target_enum = getTargetEnumByName(name);
}

/* Destructor */
ExynosVisionTarget::~ExynosVisionTarget(void)
{

}

vx_status
ExynosVisionTarget::destroy(void)
{
    vx_status status = VX_SUCCESS;

    map<vx_int32, ExynosVisionKernel*>::iterator map_iter;
    ExynosVisionKernel *kernel;
    m_internal_lock.lock();
    for (map_iter = m_kernel_map.begin(); map_iter != m_kernel_map.end(); map_iter++) {
        kernel = (*map_iter).second;
        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&kernel, VX_REF_INTERNAL, this);
    }
    m_kernel_map.clear();
    m_internal_lock.unlock();

    return status;
}

ExynosVisionKernel*
ExynosVisionTarget::addKernel(const vx_char name[VX_MAX_KERNEL_NAME],
                                                     vx_enum enumeration,
                                                     vx_kernel_f func_ptr,
                                                     vx_uint32 numParams,
                                                     vx_kernel_input_validate_f input,
                                                     vx_kernel_output_validate_f output,
                                                     vx_kernel_initialize_f init,
                                                     vx_kernel_deinitialize_f deinit)
{
    m_internal_lock.lock();
    map<vx_enum, ExynosVisionKernel*>::iterator kernel_iter = m_kernel_map.find(enumeration);
    m_internal_lock.unlock();
    if (kernel_iter != m_kernel_map.end()) {
        VXLOGI("cannot add new kernel, same enum(0x%x, %s) is already exist at target(%s)", enumeration, name, m_target_name);
        return kernel_iter->second;
    }

    ExynosVisionKernel *kernel = new ExynosVisionKernel(getContext());
    if (kernel->getCreationStatus() != VX_SUCCESS) {
        VXLOGE("kernel creation fail");
        return NULL;
    }

    kernel->assignFunc(name, enumeration, func_ptr, numParams, input, output, init, deinit);

    pair< map<vx_enum, ExynosVisionKernel*>::iterator, bool > result;

    m_internal_lock.lock();
    result = m_kernel_map.insert(kernel_pair(enumeration, kernel));
    if (result.second == true) {
        kernel->incrementReference(VX_REF_INTERNAL, this);
    } else {
        VXLOGE("cannot add new kernel, enum:%x, name:%s at %s", enumeration, name, m_target_name);
        /* just remove this from context's ref list */
        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&kernel, VX_REF_NONE, NULL);
    }
    m_internal_lock.unlock();

    if (kernel) {
        VXLOGD3("add kernel(%s) to target(%s)", kernel->getKernelName(), m_target_name);
    }

    return kernel;
}

ExynosVisionKernel*
ExynosVisionTarget::getKernelByEnum(vx_enum kernel_enum)
{
    Mutex::Autolock lock(m_internal_lock);

    ExynosVisionKernel *kernel;

    map<vx_int32, ExynosVisionKernel*>::iterator map_iter;
    map_iter = m_kernel_map.find(kernel_enum);
    if (map_iter != m_kernel_map.end()) {
        kernel = map_iter->second;
    } else {
        kernel = NULL;
    }

    return kernel;
}

ExynosVisionKernel*
ExynosVisionTarget::getKernelByName(const vx_char *name)
{
    Mutex::Autolock lock(m_internal_lock);

    ExynosVisionKernel *kernel = NULL;

    map<vx_int32, ExynosVisionKernel*>::iterator map_iter;
    for (map_iter=m_kernel_map.begin(); map_iter!=m_kernel_map.end(); map_iter++) {
        if (strcmp(name, map_iter->second->getKernelName()) == 0) {
            kernel = map_iter->second;
            break;
        }
    }

    return kernel;
}

ExynosVisionKernel*
ExynosVisionTarget::getKernelByIndex(vx_uint32 index)
{
    Mutex::Autolock lock(m_internal_lock);

    ExynosVisionKernel *kernel = NULL;

    if (index >= m_kernel_map.size()) {
        VXLOGE("kernel index(%d) is out of bound(%d)", index, m_kernel_map.size());
    } else {
        map<vx_int32, ExynosVisionKernel*>::iterator map_iter = m_kernel_map.begin();
        for (vx_uint32 i=0; i<index; i++, map_iter++);
        kernel = map_iter->second;
    }

    return kernel;
}

vx_status
ExynosVisionTarget::removeKernel(ExynosVisionKernel **kernel)
{
    vx_status status = VX_SUCCESS;

    Mutex::Autolock lock(m_internal_lock);

    map<vx_int32, ExynosVisionKernel*>::iterator map_iter;
    map_iter = m_kernel_map.find((*kernel)->getEnumeration());

    if (map_iter == m_kernel_map.end()) {
        VXLOGE("can't find %s in target %s", (*kernel)->getName(), this->getName());
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    m_kernel_map.erase(map_iter);
    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)kernel, VX_REF_INTERNAL, this);
    if (status != VX_SUCCESS) {
        VXLOGE("can't release internal count");
        goto EXIT;
    }

EXIT:
    return status;
}

void
ExynosVisionTarget::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Target ] : %s", MAKE_TAB(tap, tab_num), m_target_name);

    if (detail_info == vx_true_e) {
        map<vx_int32, ExynosVisionKernel*>::iterator map_iter;
        for (map_iter = m_kernel_map.begin(); map_iter != m_kernel_map.end(); map_iter++) {
            (*map_iter).second->displayInfo(tab_num+1, vx_true_e);
        }
    }
}

vx_enum
ExynosVisionTarget::getTargetEnumByName(const vx_char *name)
{
    for (vx_uint32 t = 0; t < dimof(target_list); t++) {
        if(strcmp(name, target_list[t].target_name) == 0) {
            return target_list[t].target_enum;
        }
    }

    return VX_TARGET_INVALID;
}

vx_char *
ExynosVisionTarget::getTargetNameByEnum(vx_enum target_enum)
{
    for (vx_uint32 t = 0; t < dimof(target_list); t++) {
        if(target_enum == target_list[t].target_enum) {
            return target_list[t].target_name;
        }
    }

    return NULL;
}

vx_enum
ExynosVisionTarget::getTargetEnumByTargetString(const vx_char *target_string)
{
    if (strcasecmp(target_string, "any") == 0) {
        return VX_TARGET_ANY;
    } else if (strcasecmp(target_string, "vpu") == 0) {
        return VX_TARGET_VPU;
    } else if (strcasecmp(target_string, "cpu") == 0) {
        return VX_TARGET_CPU;
    } else if (strcasecmp(target_string, "gpu") == 0) {
        return VX_TARGET_GPU;
    } else if (strcasecmp(target_string, "dsp") == 0) {
        return VX_TARGET_DSP;
    }

    VXLOGE("Target string (%s) is not supported", target_string);
    return VX_TARGET_INVALID;
}

}; /* namespace android */
