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

#include "ExynosVpuKernelAccum.h"

using namespace std;
using namespace android;

/* ExynosVpuKernelAbsDiff instance map from an index of node object address. */
static map<void*, ExynosVpuKernelAccum*> vpu_kernel_accum_node_map;

vx_param_description_t accum_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_BIDIRECTIONAL, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
};

extern vx_kernel_description_t accum_vpu_kernel;

static vx_status vxAccumKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelAccum *vpu_kernel_accum = vpu_kernel_accum_node_map[(void*)node];
    if (vpu_kernel_accum == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_accum->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelAccum::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelAccum::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelAccum *accum;

    if (dimof(accum_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(accum_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_accum_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        accum = new ExynosVpuKernelAccum(accum_vpu_kernel.name, dimof(accum_kernel_params));
    }

    status = accum->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete accum;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = accum->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete accum;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_accum_node_map[(void*)node] = accum;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_accum_node_map[(void*)node] != NULL)
        delete vpu_kernel_accum_node_map[(void*)node];

    vpu_kernel_accum_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t accum_vpu_kernel = {
    VX_KERNEL_ACCUMULATE,
    "com.samsung.vpu.accumulate",
    vxAccumKernel,
    accum_kernel_params, dimof(accum_kernel_params),
    vxAccumInputValidator,
    vxAccumOutputValidator,
    vxAccumInitializer,
    vxAccumDeinitializer,
};
