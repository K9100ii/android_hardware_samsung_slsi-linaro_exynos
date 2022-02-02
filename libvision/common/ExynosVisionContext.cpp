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

#define LOG_TAG "ExynosVisionContext"
#include <cutils/log.h>

#include "ExynosVisionContext.h"

const vx_char implementation[VX_MAX_IMPLEMENTATION_NAME] = "samsung.exynosvisionframework";

const vx_char extensions[] =
    " ";

namespace android {

kernel_target_t default_kernel_target_preset[] = {
    {VX_KERNEL_COLOR_CONVERT,       VX_TARGET_VPU},
    {VX_KERNEL_CHANNEL_EXTRACT,     VX_TARGET_VPU},
    {VX_KERNEL_CHANNEL_COMBINE,     VX_TARGET_VPU},
    {VX_KERNEL_SOBEL_3x3,           VX_TARGET_VPU},
    {VX_KERNEL_MAGNITUDE,           VX_TARGET_VPU},
    {VX_KERNEL_PHASE,               VX_TARGET_VPU},
    {VX_KERNEL_SCALE_IMAGE,         VX_TARGET_VPU},
    {VX_KERNEL_TABLE_LOOKUP,        VX_TARGET_VPU},
    {VX_KERNEL_HISTOGRAM,           VX_TARGET_VPU},
    {VX_KERNEL_EQUALIZE_HISTOGRAM,  VX_TARGET_VPU},
    {VX_KERNEL_ABSDIFF,             VX_TARGET_VPU},
    {VX_KERNEL_MEAN_STDDEV,         VX_TARGET_VPU},
    {VX_KERNEL_THRESHOLD,           VX_TARGET_VPU},
    {VX_KERNEL_INTEGRAL_IMAGE,      VX_TARGET_VPU},
    {VX_KERNEL_DILATE_3x3,          VX_TARGET_VPU},
    {VX_KERNEL_ERODE_3x3,           VX_TARGET_VPU},
    {VX_KERNEL_MEDIAN_3x3,          VX_TARGET_VPU},
    {VX_KERNEL_BOX_3x3,             VX_TARGET_VPU},
    {VX_KERNEL_GAUSSIAN_3x3,        VX_TARGET_VPU},
    {VX_KERNEL_CUSTOM_CONVOLUTION,  VX_TARGET_VPU},
    {VX_KERNEL_GAUSSIAN_PYRAMID,    VX_TARGET_VPU},
    {VX_KERNEL_ACCUMULATE,          VX_TARGET_VPU},
    {VX_KERNEL_ACCUMULATE_WEIGHTED, VX_TARGET_VPU},
    {VX_KERNEL_ACCUMULATE_SQUARE,   VX_TARGET_VPU},
    {VX_KERNEL_MINMAXLOC,           VX_TARGET_VPU},
    {VX_KERNEL_CONVERTDEPTH,        VX_TARGET_VPU},
    {VX_KERNEL_CANNY_EDGE_DETECTOR, VX_TARGET_VPU},
    {VX_KERNEL_AND,                 VX_TARGET_VPU},
    {VX_KERNEL_OR,                  VX_TARGET_VPU},
    {VX_KERNEL_XOR,                 VX_TARGET_VPU},
    {VX_KERNEL_NOT,                 VX_TARGET_VPU},
    {VX_KERNEL_MULTIPLY,            VX_TARGET_VPU},
    {VX_KERNEL_ADD,                 VX_TARGET_VPU},
    {VX_KERNEL_SUBTRACT,            VX_TARGET_VPU},
    {VX_KERNEL_WARP_AFFINE,         VX_TARGET_CPU},
    {VX_KERNEL_WARP_PERSPECTIVE,    VX_TARGET_CPU},
    {VX_KERNEL_HARRIS_CORNERS,      VX_TARGET_VPU},
    {VX_KERNEL_FAST_CORNERS,        VX_TARGET_VPU},
    {VX_KERNEL_OPTICAL_FLOW_PYR_LK, VX_TARGET_VPU},
    {VX_KERNEL_REMAP,               VX_TARGET_CPU},
    {VX_KERNEL_HALFSCALE_GAUSSIAN,  VX_TARGET_VPU},
};

static vx_bool vxIsValidBorderMode(vx_enum mode)
{
    vx_bool ret = vx_true_e;
    switch (mode)
    {
        case VX_BORDER_MODE_UNDEFINED:
        case VX_BORDER_MODE_CONSTANT:
        case VX_BORDER_MODE_REPLICATE:
            break;
        default:
            ret = vx_false_e;
            break;
    }
    return ret;
}

vx_bool
ExynosVisionContext::isValidContext(ExynosVisionContext *context)
{
    vx_bool ret = vx_false_e;
    if ((context != NULL) &&
        (context->m_magic == VX_MAGIC) &&
        (context->getType() == VX_TYPE_CONTEXT)) {
        ret = vx_true_e; /* this is the top level context */
    }

    return ret;
}

ExynosVisionContext::ExynosVisionContext(void) : ExynosVisionReference(this, VX_TYPE_CONTEXT, (ExynosVisionReference*)this, vx_false_e)
{
    EXYNOS_VISION_SYSTEM_IN();

    m_log_callback = NULL;
    m_log_enabled = vx_false_e;
    m_log_reentrant = vx_false_e;;

    m_imm_border.mode = VX_BORDER_MODE_UNDEFINED;
    m_imm_border.constant_value = 0;

    m_immediate_target = NULL;

    m_performance_monitor = NULL;

    EXYNOS_VISION_SYSTEM_OUT();
}

ExynosVisionContext::~ExynosVisionContext(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    EXYNOS_VISION_SYSTEM_OUT();
}

vx_status
ExynosVisionContext::init(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status load_status = VX_SUCCESS;
    
    if (createConstErrors() == vx_false_e) {
        return VX_ERROR_NO_MEMORY;
    }

#ifdef USE_OPENCL_KERNEL
    /* Load opencl kernels, samsung */
    VXLOGD("loading openvx-opencl kernels");
    load_statu = loadKernels("openvx-opencl");
    if (load_status != VX_SUCCESS)
        VXLOGE("ERR(%d):loading kernel of openvx-opencl fail", load_status);
    else
        VXLOGD("loading kernels: openvx-opencl");
#endif

    /* Load vpu kernels, samsung */
    VXLOGD("loading exynosvpukernel");
    load_status = loadKernels("exynosvpukernel");
    if (load_status != VX_SUCCESS)
        VXLOGE("ERR(%d):loading kernel of exynosvpukernel fail", load_status);
    else
        VXLOGD("loading kernels: exynosvpukernel");

    /* Load score kernels, samsung */
    VXLOGD("loading exynosscorekernel");
    load_status = loadKernels("exynosscorekernel");
    if (load_status != VX_SUCCESS)
        VXLOGE("ERR(%d):loading kernel of exynosscorekernel fail", load_status);
    else
        VXLOGD("loading kernels: exynosscorekernel");

    m_performance_monitor = new ExynosVisionPerfMonitor<ExynosVisionGraph*>;
    if (m_performance_monitor == NULL) {
        VXLOGE("performance monitor can't create");
        return VX_FAILURE;
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return VX_SUCCESS;
}

vx_status
ExynosVisionContext::destroy(void)
{
    vx_status status = VX_SUCCESS;
    registerLogCallback(NULL, vx_false_e);

    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        status = (*target_iter)->destroy();
        if (status != VX_SUCCESS)
            VXLOGE("destorying target(%d) fails, err:%d", (*target_iter)->getId(), status);
        delete (*target_iter);
    }
    m_target_list.clear();

    /* m_ref_list could be modified inside of releaseReferenceInt(), so we have to use clone of m_ref_list. */
    List<ExynosVisionReference*>::iterator ref_iter;

    m_internal_lock.lock();
    List<ExynosVisionReference*> external_ref_list;
    for (ref_iter=m_ref_list.begin(); ref_iter!=m_ref_list.end(); ref_iter++) {
        if ((*ref_iter)->getExternalCnt()) {
            external_ref_list.push_back(*ref_iter);
        }
    }
    m_internal_lock.unlock();

    for (ref_iter=external_ref_list.begin(); ref_iter!=external_ref_list.end(); ref_iter++) {
         while((*ref_iter)->getExternalCnt()) {
            /* kernel object is not released commonly, external referencing is occured at vxAddKernel API. */
            if ((*ref_iter)->getType() != VX_TYPE_KERNEL)
                VXLOGW("external referencing is not released, %s", (*ref_iter)->getName());
            status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&(*ref_iter), VX_REF_EXTERNAL);
            if (status != VX_SUCCESS) {
                VXLOGE("release external reference of %s fails, err:%d", (*ref_iter)->getName(), status);
            }

            if (*ref_iter == NULL)
                break;
        }
    }
    if (m_ref_list.size()) {
        VXLOGE("some object doesn't be removed");
        for (ref_iter=m_ref_list.begin(); ref_iter!=m_ref_list.end(); ref_iter++)
            (*ref_iter)->displayInfo(0, vx_true_e);
    }

    m_internal_lock.lock();
    List<ExynosVisionError*>::iterator err_iter;
    for (err_iter=m_error_ref_list.begin(); err_iter!=m_error_ref_list.end(); err_iter++) {
        status = (*err_iter)->destroy();
        if (status != VX_SUCCESS)
            VXLOGE("destorying err ref(%d) fails, err:%d", (*err_iter)->getId(), status);
        delete (*err_iter);
    }

    vx_symbol_t sym = NULL;
    vx_module_deinitializer_f deinitializer = NULL;

    List<vx_module_t>::iterator module_iter;
    for (module_iter=m_module_list.begin(); module_iter!=m_module_list.end(); module_iter++) {
        /* Do not consider as error when Module Deinitializer is absent. */
        sym = vxGetSymbol((*module_iter).handle, (vx_char*)"vxModuleDeinitializer");
        deinitializer = (vx_module_deinitializer_f)sym;
        if (deinitializer) {
            VXLOGD2("Calling %s module deinitializer function\n", (*module_iter).name);
            status = deinitializer();
            if (status != VX_SUCCESS) {
                VXLOGE("Failed to deinitialize module");
            }
        }
        vxUnloadModule((*module_iter).handle);
    }
    m_internal_lock.unlock();

    if (m_performance_monitor)
        delete m_performance_monitor;

    return status;
}

vx_status
ExynosVisionContext::loadKernels(const vx_char *name)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_FAILURE;
    vx_char module[VX_INT_MAX_PATH];
    vx_symbol_t sym = NULL;
    vx_module_initializer_f initializer = NULL;
    vx_publish_kernels_f publish = NULL;
    vx_module_t module_str;

    List<vx_module_t>::iterator module_iter;
    for (module_iter=m_module_list.begin(); module_iter!=m_module_list.end(); module_iter++) {
        if (strcmp((*module_iter).name, name) == 0) {
            VXLOGD3("module(%s) is already loaded", name);
            status = VX_SUCCESS;
            goto EXIT;
        }
    }

    sprintf(module, "lib""%s"".so", name);

    module_str.handle = vxLoadModule(module);
    VXLOGD("loaded a module: %s", module);
    if (module_str.handle) {
        /* Do not consider as error when Module Initializer is absent. */
        sym = vxGetSymbol(module_str.handle, (vx_char*)"vxModuleInitializer");
        initializer = (vx_module_initializer_f)sym;
        if (initializer) {
            VXLOGD2("Calling %s module initializer function\n", module);
            status = initializer((vx_context)this);
            if (status != VX_SUCCESS) {
                VXLOGE("Failed to initialize module");
            }
        }

        sym = vxGetSymbol(module_str.handle, (vx_char*)"vxPublishKernels");
        publish = (vx_publish_kernels_f)sym;
        if (publish == NULL) {
            VXLOGE("Failed to load symbol vxPublishKernels");
            status = VX_ERROR_INVALID_MODULE;
            vxUnloadModule(module_str.handle);
        }
        else
        {
            VXLOGD2("Calling %s publish function\n", module);
            status = publish((vx_context)this);
            if (status != VX_SUCCESS) {
                VXLOGE("Failed to publish kernels in module");
                vxUnloadModule(module_str.handle);
            }
            else {
                strncpy(module_str.name, name, VX_INT_MAX_PATH);
                m_module_list.push_back(module_str);
            }
        }
    } else {
        VXLOGE("Failed to find module %s in libraries path", module);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("Failed to load module %s; error %d", module, status);
    } else {
        VXLOGD2("total module list");
        for (List<vx_module_t>::iterator iterPos = m_module_list.begin(); iterPos != m_module_list.end(); iterPos++)
            VXLOGD2("module: %s\n", (*iterPos).name);
    }

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

ExynosVisionKernel*
ExynosVisionContext::addKernel(const vx_char name[VX_MAX_KERNEL_NAME],
                                                     vx_enum enumeration,
                                                     vx_kernel_f func_ptr,
                                                     vx_uint32 numParams,
                                                     vx_kernel_input_validate_f input,
                                                     vx_kernel_output_validate_f output,
                                                     vx_kernel_initialize_f init,
                                                     vx_kernel_deinitialize_f deinit)
{
    if (func_ptr == NULL ||
        input == NULL ||
        output == NULL ||
        numParams > VX_INT_MAX_PARAMS ||
        name == NULL ||
        strncmp(name, "",  VX_MAX_KERNEL_NAME) == 0) {

        /* initialize and de-initialize can be NULL */
        VXLOGE("invalid Parameters, func_ptr:%p, input:%p, output:%p, name:%s, numParams:%d", func_ptr, input, output, name, numParams);
        addLogEntry(this, VX_ERROR_INVALID_PARAMETERS, "Invalid Parameters supplied to vxAddKernel\n");
        return NULL;
    }

    vx_char target_name[VX_MAX_TARGET_NAME];
    memset(target_name, 0x0, sizeof(target_name));

    vx_char *last_char = strrchr(name, '.');
    vx_int64 index = (vx_int64)last_char - (vx_int64)name;
    if (index == 0)
        strcpy(target_name, name);
    else
        strncpy(target_name, name, last_char-name);

    ExynosVisionTarget *cur_target = NULL;

    for (List<ExynosVisionTarget*>::iterator target_iter = m_target_list.begin(); target_iter != m_target_list.end(); target_iter++) {
        if (strcmp((*target_iter)->getTargetName(), target_name) == 0) {
            cur_target = *target_iter;
        }
    }

    if (cur_target == NULL) {
        VXLOGD2("making new target : %s", target_name);
        cur_target = new ExynosVisionTarget(this, target_name);
        addTarget(cur_target);

        VXLOGD2("total target");
        for (List<ExynosVisionTarget*>::iterator target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++)
            VXLOGD2("target: %s", (*target_iter)->getTargetName());
    }

    ExynosVisionKernel *kernel = cur_target->addKernel(name, enumeration, func_ptr, numParams, input, output, init, deinit);

    return kernel;
}

ExynosVisionKernel*
ExynosVisionContext::getKernelByEnum(vx_enum kernel_enum)
{
    ExynosVisionKernel *kernel = NULL;
    ExynosVisionTarget *target = NULL;

    // priority 1: immediate target
    if(m_immediate_target) {
        kernel = m_immediate_target->getKernelByEnum(kernel_enum);
        if (kernel)
            return kernel;
    }

    // priority 2: default target (target any)
    for (vx_uint32 k = 0; k < dimof(default_kernel_target_preset); k++) {
        if (kernel_enum == default_kernel_target_preset[k].kernel_enum) {
            target = getTargetByEnum(default_kernel_target_preset[k].target_enum);
            if (target) {
                kernel = target->getKernelByEnum(kernel_enum);
                if (kernel) {
                    VXLOGD3("Find kernel(0x%X) at target(0x%X)", kernel_enum, default_kernel_target_preset[k].target_enum);
                    break;
                }
            }
        }
    }

    // priority 3: first target found
    if (kernel == NULL) {
        VXLOGD3("defalut target for kernel(0x%x) is not found", kernel_enum);

        List<ExynosVisionTarget*>::iterator target_iter;
        for (target_iter = m_target_list.begin(); target_iter != m_target_list.end(); target_iter++) {
            kernel = (*target_iter)->getKernelByEnum(kernel_enum);
            if (kernel) {
                VXLOGD3("Find kernel(0x%X) at target(%s)", kernel_enum, (*target_iter)->getTargetName());
                break;
            }
        }
    }

    if (kernel == NULL)
        VXLOGE("kernel enum 0x%X not found at context", kernel_enum);

    return kernel;
}

ExynosVisionKernel*
ExynosVisionContext::getKernelByName(const vx_char *name)
{
    ExynosVisionKernel *kernel;
    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        kernel = (*target_iter)->getKernelByName(name);
        if (kernel) {
            VXLOGD3("Find kernel(%s) at target(%s)", name, (*target_iter)->getTargetName());
            break;
        }
    }

    if (kernel == NULL)
        VXLOGE("kernel name %s not found at context", name);

    return kernel;
}

ExynosVisionKernel*
ExynosVisionContext::getKernelByTarget(vx_enum kernel_enum, vx_enum target_enum)
{
    ExynosVisionKernel *kernel = NULL;
    ExynosVisionTarget *target = NULL;

    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        if ((*target_iter)->getTargetEnum() == target_enum) {
            target = *target_iter;
            break;
        }
    }

    if (target) {
        kernel = target->getKernelByEnum(kernel_enum);

        if (kernel)
            VXLOGD3("Find kernel (0x%X) at target(%s)", kernel_enum, target->getTargetName());
        else
            VXLOGE("kernel 0x%X not found at target(%s)", kernel_enum, target->getTargetName());
    } else {
        VXLOGE("target(0x%x) is not loaded", target_enum);
    }

    return kernel;
}

vx_status
ExynosVisionContext::removeKernel(ExynosVisionKernel **kernel)
{
    vx_status status = VX_FAILURE;

    const vx_char *kernel_name = (*kernel)->getKernelName();

    vx_char target_name[VX_MAX_TARGET_NAME];
    memset(target_name, 0x0, sizeof(target_name));

    vx_char *last_char = strrchr(kernel_name, '.');
    vx_int64 index = (vx_int64)last_char - (vx_int64)kernel_name;
    if (index == 0)
        strcpy(target_name, kernel_name);
    else
        strncpy(target_name, kernel_name, last_char-kernel_name);

    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        if (strcmp(target_name, (*target_iter)->getTargetName()) == 0)
            break;
    }

    if (target_iter == m_target_list.end()) {
        VXLOGE("can't find perfer target, expected target name is %s", target_name);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    status = (*target_iter)->removeKernel(kernel);
    if (status != VX_SUCCESS) {
        VXLOGE("can't find %s in %s", (*kernel)->getKernelName(), (*target_iter)->getTargetName());
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_uint32
ExynosVisionContext::getUniqueKernelsNum(void)
{
    vx_uint32 kernels_num;
    ExynosVisionKernel *kernel;
    map<vx_enum, ExynosVisionKernel*> unique_kernels_map;

    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        kernels_num = (*target_iter)->getKernelsNum();

        for (vx_uint32 i=0; i<kernels_num; i++) {
            kernel = (*target_iter)->getKernelByIndex(i);
            pair <vx_enum, ExynosVisionKernel*> kernel_pair(kernel->getEnumeration(), kernel);
            unique_kernels_map.insert(kernel_pair);
        }
    }

    return unique_kernels_map.size();
}

vx_status
ExynosVisionContext::fillUniqueKernelsTable(vx_kernel_info_t *table)
{
    vx_uint32 kernels_num;
    ExynosVisionKernel *kernel;
    map<vx_enum, ExynosVisionKernel*> unique_kernels_map;
    pair< map<vx_enum, ExynosVisionKernel*>::iterator, bool > result;
    vx_uint32 cnt = 0;
    vx_status status = VX_SUCCESS;

    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        kernels_num = (*target_iter)->getKernelsNum();
        for (vx_uint32 i=0; i<kernels_num; i++) {
            kernel = (*target_iter)->getKernelByIndex(i);
            pair <vx_enum, ExynosVisionKernel*> kernel_pair(kernel->getEnumeration(), kernel);
            result = unique_kernels_map.insert(kernel_pair);
            if (result.second == true) {
                table[cnt].enumeration = kernel->getEnumeration();
                memcpy(&table[cnt++].name, kernel->getKernelName(), VX_MAX_KERNEL_NAME);
            }
        }
    }

    if (cnt != unique_kernels_map.size()) {
        VXLOGE("unique kernel number is not corrent (%d, %d)", cnt, unique_kernels_map.size());
        status =  VX_FAILURE;
    }

    return VX_SUCCESS;
}

vx_status
ExynosVisionContext::queryContext(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;
        switch (attribute) {
        case VX_CONTEXT_ATTRIBUTE_VENDOR_ID:
            if (VX_CHECK_PARAM(ptr, size, vx_uint16, 0x1))
                *(vx_uint16 *)ptr = VX_ID_SAMSUNG;
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_VERSION:
            if (VX_CHECK_PARAM(ptr, size, vx_uint16, 0x1))
                *(vx_uint16 *)ptr = (vx_uint16)VX_VERSION;
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_MODULES:
            if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
                *(vx_uint32 *)ptr = m_module_list.size();
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_REFERENCES:
            if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
                vx_uint32 total_ref = m_ref_list.size() + m_error_ref_list.size();
                *(vx_uint32 *)ptr = total_ref;
            } else {
                status = VX_ERROR_INVALID_PARAMETERS;
            }
            break;
        case VX_CONTEXT_ATTRIBUTE_IMPLEMENTATION:
            if (size <= VX_MAX_IMPLEMENTATION_NAME && ptr)
                strncpy((char*)ptr, implementation, VX_MAX_IMPLEMENTATION_NAME);
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_EXTENSIONS_SIZE:
            if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
                *(vx_size *)ptr = sizeof(extensions);
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_EXTENSIONS:
            if (size <= sizeof(extensions) && ptr)
                strncpy((char*)ptr, extensions, sizeof(extensions));
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_CONVOLUTION_MAXIMUM_DIMENSION:
            if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
                *(vx_size *)ptr = VX_INT_MAX_CONVOLUTION_DIM;
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_OPTICAL_FLOW_WINDOW_MAXIMUM_DIMENSION:
            if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
                *(vx_size *)ptr = VX_OPTICALFLOWPYRLK_MAX_DIM;
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_IMMEDIATE_BORDER_MODE:
            if (VX_CHECK_PARAM(ptr, size, vx_border_mode_t, 0x3))
                *(vx_border_mode_t *)ptr = m_imm_border;
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_UNIQUE_KERNELS:
            if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
                *(vx_uint32 *)ptr = getUniqueKernelsNum();
            else
                status = VX_ERROR_INVALID_PARAMETERS;
            break;
        case VX_CONTEXT_ATTRIBUTE_UNIQUE_KERNEL_TABLE:
            vx_uint32 unique_kernels_num;
            unique_kernels_num = getUniqueKernelsNum();
            if ((size == (unique_kernels_num * sizeof(vx_kernel_info_t))) && (ptr != NULL))
                status = fillUniqueKernelsTable((vx_kernel_info_t*)ptr);
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
ExynosVisionContext::setContextAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_CONTEXT_ATTRIBUTE_IMMEDIATE_BORDER_MODE:
        if (VX_CHECK_PARAM(ptr, size, vx_border_mode_t, 0x3)) {
            vx_border_mode_t *config = (vx_border_mode_t *)ptr;
            if (vxIsValidBorderMode(config->mode) == vx_false_e)
                status = VX_ERROR_INVALID_VALUE;
            else
                m_imm_border = *config;
        } else  {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_enum
ExynosVisionContext::registerUserStruct(vx_size size)
{
    if (m_user_struct_list.size() >= VX_TYPE_USER_STRUCT_END - VX_TYPE_USER_STRUCT_START) {
        VXLOGE("Can't register new user struct, user struct num:%d", m_user_struct_list.size());
        return VX_TYPE_INVALID;
    }

    vx_enum type = VX_TYPE_USER_STRUCT_START + m_user_struct_list.size();

    struct user_struct user_str;
    user_str.type = type;
    user_str.size = size;

    m_user_struct_list.push_back(user_str);

    return type;
}

vx_bool
ExynosVisionContext::createConstErrors()
{
    vx_bool ret = vx_true_e;
    vx_enum e = 0;
    /* create an error object for each status enumeration */
    for (e = VX_STATUS_MIN; (e < VX_SUCCESS) && (ret == vx_true_e); e++)
    {
        ExynosVisionError *cError = new ExynosVisionError(this, e);
        addErrorReference(cError);
    }

    return ret;
}

ExynosVisionReference*
ExynosVisionContext::getErrorObject(vx_status status)
{
    EXYNOS_VISION_SYSTEM_IN();

    ExynosVisionError *error = NULL;
    vx_size i = 0ul;

    Mutex::Autolock lock(m_internal_lock);

    for (List<ExynosVisionError*>::iterator iterPos = m_error_ref_list.begin(); iterPos != m_error_ref_list.end(); iterPos++) {
        if ((*iterPos)->getStatus() == status) {
            error = *iterPos;
            break;
        }
    }

    if (error == NULL)
        VXLOGE("error object is not found");

    EXYNOS_VISION_SYSTEM_OUT();

    return error;
}

vx_bool
ExynosVisionContext::addErrorReference(ExynosVisionError *error_ref)
{
    EXYNOS_VISION_SYSTEM_IN();

    Mutex::Autolock lock(m_internal_lock);

    m_error_ref_list.push_back(error_ref);
    error_ref->incrementReference(VX_REF_INTERNAL, this);

    EXYNOS_VISION_SYSTEM_OUT();

    return vx_true_e;
}

vx_bool
ExynosVisionContext::addReference(ExynosVisionReference *ref)
{
    EXYNOS_VISION_SYSTEM_IN();

    Mutex::Autolock lock(m_internal_lock);
    m_ref_list.push_back(ref);

    EXYNOS_VISION_SYSTEM_OUT();

    return vx_true_e;
}

vx_bool
ExynosVisionContext::addTarget(ExynosVisionTarget *target)
{
    Mutex::Autolock lock(m_internal_lock);

    m_target_list.push_back(target);
    target->incrementReference(VX_REF_INTERNAL, this);

    return vx_true_e;
}

ExynosVisionTarget*
ExynosVisionContext::getTargetByEnum(vx_enum target_enum)
{
    List<ExynosVisionTarget*>::iterator target_iter;
    for (target_iter=m_target_list.begin(); target_iter!=m_target_list.end(); target_iter++) {
        if(target_enum == (*target_iter)->getTargetEnum()) {
            VXLOGD3("found target (%s) by enum(0x%X)", (*target_iter)->getTargetName(), target_enum);
            return *target_iter;
        }
    }

    VXLOGE("Failed to find target by enum(0x%X)", target_enum);
    return NULL;
}

vx_status
ExynosVisionContext::setImmediateModeTarget(vx_enum target_enum)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;

    if (target_enum == VX_TARGET_ANY) {
        status = VX_SUCCESS;
        goto EXIT;
    }

    m_immediate_target = getTargetByEnum(target_enum);

    if(m_immediate_target == NULL)
        status = VX_FAILURE;

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;

}

vx_status
ExynosVisionContext::removeReference(ExynosVisionReference *ref)
{
    EXYNOS_VISION_SYSTEM_IN();
    Mutex::Autolock lock(m_internal_lock);

    vx_status status = VX_FAILURE;

    if (ref->getType() == VX_TYPE_ERROR) {
        List<ExynosVisionError*>::iterator iterPos;
        for (iterPos = m_error_ref_list.begin(); iterPos != m_error_ref_list.end(); iterPos++) {
            if ((*iterPos) == ref) {
                m_error_ref_list.erase(iterPos);
                status = VX_SUCCESS;
            }
        }

        if (iterPos == m_error_ref_list.end()) {
            VXLOGE("error ref %d is not found", ref->getId());
            status = VX_ERROR_INVALID_REFERENCE;
        }
    } else if (ref->getType() == VX_TYPE_TARGET) {
        VXLOGE("removing target is not defined yet");
        status = VX_ERROR_INVALID_REFERENCE;
    } else {
        List<ExynosVisionReference*>::iterator iterPos;
        for (iterPos = m_ref_list.begin(); iterPos != m_ref_list.end(); iterPos++) {
            if ((*iterPos) == ref) {
                m_ref_list.erase(iterPos);
                status = VX_SUCCESS;
                break;
            }
        }
        if (iterPos == m_ref_list.end()) {
            VXLOGE("ref %d is not found", ref->getId());
            ref->displayInfo(0, vx_true_e);
            status = VX_ERROR_INVALID_REFERENCE;
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

void
ExynosVisionContext::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[CONTEXT][%d] =====[Context Information]======", MAKE_TAB(tap, tab_num), detail_info);
    VXLOGI("%s[CONTEXT] module number : %d", MAKE_TAB(tap, tab_num), m_module_list.size());
    for (List<vx_module_t>::iterator iterPos = m_module_list.begin(); iterPos != m_module_list.end(); iterPos++) {
        VXLOGI("%s[MODULE ] name : %s", MAKE_TAB(tap, tab_num), (*iterPos).name);
    }
    VXLOGI("%s[CONTEXT] ================================", MAKE_TAB(tap, tab_num));
    VXLOGI("%s[CONTEXT] target number : %d", MAKE_TAB(tap, tab_num), m_target_list.size());
    for (List<ExynosVisionTarget*>::iterator iterPos = m_target_list.begin(); iterPos != m_target_list.end(); iterPos++) {
        (*iterPos)->displayInfo(0, vx_true_e);
    }
    VXLOGI("%s[CONTEXT] ================================", MAKE_TAB(tap, tab_num));
    VXLOGI("%s[CONTEXT] ref number : %d", MAKE_TAB(tap, tab_num), m_ref_list.size());
    for (List<ExynosVisionReference*>::iterator iterPos = m_ref_list.begin(); iterPos != m_ref_list.end(); iterPos++) {
        (*iterPos)->displayInfo(0, vx_true_e);
    }
    VXLOGI("%s[CONTEXT] ================================", MAKE_TAB(tap, tab_num));

    EXYNOS_VISION_SYSTEM_OUT();
}

vx_status
ExynosVisionContext::setDirective(vx_enum directive)
{
    vx_status status = VX_SUCCESS;

    switch (directive) {
    /*! \todo add directive to the sample implementation */
    case VX_DIRECTIVE_DISABLE_LOGGING:
        m_log_enabled = vx_false_e;
        break;
    case VX_DIRECTIVE_ENABLE_LOGGING:
        m_log_enabled = vx_true_e;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

void
ExynosVisionContext::registerLogCallback(vx_log_callback_f callback, vx_bool reentrant)
{
    EXYNOS_VISION_SYSTEM_IN();

    if (callback == NULL)
        m_log_enabled = vx_false_e;
    else
        m_log_enabled = vx_true_e;

    m_log_callback = callback;
    m_log_reentrant = reentrant;

    EXYNOS_VISION_SYSTEM_OUT();
}

void
ExynosVisionContext::addLogEntry(ExynosVisionReference *ref, vx_status status, const char *message, ...)
{
    va_list ap;
    vx_char string[VX_MAX_LOG_MESSAGE_LEN];

    if (m_log_enabled == vx_false_e)
        return;

    if (m_log_callback == NULL) {
        VXLOGE("no callback is registered");
        return;
    }

    va_start(ap, message);
    vsnprintf(string, VX_MAX_LOG_MESSAGE_LEN, message, ap);
    string[VX_MAX_LOG_MESSAGE_LEN-1] = 0; // for MSVC which is not C99 compliant
    va_end(ap);
    if (m_log_reentrant == vx_false_e)
        m_log_mutex.lock();

    m_log_callback((vx_context)this, (vx_reference)ref, status, string);
    if (m_log_reentrant == vx_false_e)
        m_log_mutex.unlock();

    return;
}

vx_size
ExynosVisionContext::getUserStructsSize(vx_enum item_type)
{
    vx_size res = 0;

    List<struct user_struct>::iterator str_iter;

    for (str_iter=m_user_struct_list.begin(); str_iter!=m_user_struct_list.end(); str_iter++) {
        if ((*str_iter).type == item_type) {
            res = (*str_iter).size;
            break;
        }
    }

    return res;
}

}; /* namespace android */
