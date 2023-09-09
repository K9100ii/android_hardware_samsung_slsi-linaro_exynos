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

#define LOG_TAG "ExynosVpuKernelInterface"
#include <cutils/log.h>

#include <map>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_helper.h>

#include "ExynosVpuKernelBitwiseBinary.h"

using namespace std;
using namespace android;

/* ExynosVpuKernel instance map from an index of node object address. */
static map<void*, ExynosVpuKernelBitwiseAnd*> vpu_kernel_bitwiseand_node_map;
static map<void*, ExynosVpuKernelBitwiseOr*> vpu_kernel_bitwiseor_node_map;
static map<void*, ExynosVpuKernelBitwiseXor*> vpu_kernel_bitwisexor_node_map;

static vx_param_description_t bitwisebinary_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
};

extern vx_kernel_description_t bitwiseand_vpu_kernel;
extern vx_kernel_description_t bitwiseor_vpu_kernel;
extern vx_kernel_description_t bitwisexor_vpu_kernel;

static vx_status vxBitwiseBinaryInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelBitwiseBinary::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseBinaryOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelBitwiseBinary::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseAndKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelBitwiseAnd *vpu_kernel_bitwiseand = vpu_kernel_bitwiseand_node_map[(void*)node];
    if (vpu_kernel_bitwiseand == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_bitwiseand->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseAndInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelBitwiseAnd *bitwiseand;

    if (dimof(bitwisebinary_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(bitwisebinary_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_bitwiseand_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        bitwiseand = new ExynosVpuKernelBitwiseAnd(bitwiseand_vpu_kernel.name, dimof(bitwisebinary_kernel_params));
    }

    status = bitwiseand->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete bitwiseand;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = bitwiseand->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete bitwiseand;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_bitwiseand_node_map[(void*)node] = bitwiseand;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseAndDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_bitwiseand_node_map[(void*)node] != NULL)
        delete vpu_kernel_bitwiseand_node_map[(void*)node];

    vpu_kernel_bitwiseand_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseOrKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelBitwiseOr *vpu_kernel_bitwiseor = vpu_kernel_bitwiseor_node_map[(void*)node];
    if (vpu_kernel_bitwiseor == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_bitwiseor->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseOrInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelBitwiseOr *bitwiseor;

    if (dimof(bitwisebinary_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(bitwisebinary_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_bitwiseor_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        bitwiseor = new ExynosVpuKernelBitwiseOr(bitwiseor_vpu_kernel.name, dimof(bitwisebinary_kernel_params));
    }

    status = bitwiseor->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete bitwiseor;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = bitwiseor->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete bitwiseor;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_bitwiseor_node_map[(void*)node] = bitwiseor;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseOrDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_bitwiseor_node_map[(void*)node] != NULL)
        delete vpu_kernel_bitwiseor_node_map[(void*)node];

    vpu_kernel_bitwiseor_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseXorKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelBitwiseXor *vpu_kernel_bitwisexor = vpu_kernel_bitwisexor_node_map[(void*)node];
    if (vpu_kernel_bitwisexor == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_bitwisexor->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseXorInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelBitwiseXor *bitwisexor;

    if (dimof(bitwisebinary_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(bitwisebinary_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_bitwisexor_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        bitwisexor = new ExynosVpuKernelBitwiseXor(bitwisexor_vpu_kernel.name, dimof(bitwisebinary_kernel_params));
    }

    status = bitwisexor->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete bitwisexor;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = bitwisexor->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete bitwisexor;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_bitwisexor_node_map[(void*)node] = bitwisexor;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBitwiseXorDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_bitwisexor_node_map[(void*)node] != NULL)
        delete vpu_kernel_bitwisexor_node_map[(void*)node];

    vpu_kernel_bitwisexor_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t bitwiseand_vpu_kernel = {
    VX_KERNEL_AND,
    "com.samsung.vpu.and",
    vxBitwiseAndKernel,
    bitwisebinary_kernel_params, dimof(bitwisebinary_kernel_params),
    vxBitwiseBinaryInputValidator,
    vxBitwiseBinaryOutputValidator,
    vxBitwiseAndInitializer,
    vxBitwiseAndDeinitializer,
};

vx_kernel_description_t bitwiseor_vpu_kernel = {
    VX_KERNEL_OR,
    "com.samsung.vpu.or",
    vxBitwiseOrKernel,
    bitwisebinary_kernel_params, dimof(bitwisebinary_kernel_params),
    vxBitwiseBinaryInputValidator,
    vxBitwiseBinaryOutputValidator,
    vxBitwiseOrInitializer,
    vxBitwiseOrDeinitializer,
};

vx_kernel_description_t bitwisexor_vpu_kernel = {
    VX_KERNEL_XOR,
    "com.samsung.vpu.xor",
    vxBitwiseXorKernel,
    bitwisebinary_kernel_params, dimof(bitwisebinary_kernel_params),
    vxBitwiseBinaryInputValidator,
    vxBitwiseBinaryOutputValidator,
    vxBitwiseXorInitializer,
    vxBitwiseXorDeinitializer,
};

