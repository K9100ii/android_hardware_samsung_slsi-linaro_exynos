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

#include "ExynosVpuKernelAccumScale.h"

using namespace std;
using namespace android;

/* ExynosVpuKernelAbsDiff instance map from an index of node object address. */
static map<void*, ExynosVpuKernelAccumWeighted*> vpu_kernel_accumweighted_node_map;
static map<void*, ExynosVpuKernelAccumSquare*> vpu_kernel_accumsquare_node_map;

vx_param_description_t accumscale_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_BIDIRECTIONAL, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED}
};

extern vx_kernel_description_t accumweighted_vpu_kernel;
extern vx_kernel_description_t accumsquare_vpu_kernel;

static vx_status vxAccumWeightedInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelAccumWeighted::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumSquareInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelAccumSquare::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumScaleOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelAccumScale::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumWeightedKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelAccumWeighted *accumweighted = vpu_kernel_accumweighted_node_map[(void*)node];
    if (accumweighted == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = accumweighted->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumWeightedInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelAccumWeighted *accumweighted;

    if (dimof(accumscale_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(accumscale_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_accumweighted_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        accumweighted = new ExynosVpuKernelAccumWeighted(accumweighted_vpu_kernel.name, dimof(accumscale_kernel_params));
    }

    status = accumweighted->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete accumweighted;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = accumweighted->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete accumweighted;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_accumweighted_node_map[(void*)node] = accumweighted;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumWeightedDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_accumweighted_node_map[(void*)node] != NULL)
        delete vpu_kernel_accumweighted_node_map[(void*)node];

    vpu_kernel_accumweighted_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumSquareKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelAccumSquare *accumsquare = vpu_kernel_accumsquare_node_map[(void*)node];
    if (accumsquare == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = accumsquare->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumSquareInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelAccumSquare *accumsquare;

    if (dimof(accumscale_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(accumscale_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_accumsquare_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        accumsquare = new ExynosVpuKernelAccumSquare(accumsquare_vpu_kernel.name, dimof(accumscale_kernel_params));
    }

    status = accumsquare->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete accumsquare;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = accumsquare->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete accumsquare;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_accumsquare_node_map[(void*)node] = accumsquare;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxAccumSquareDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_accumsquare_node_map[(void*)node] != NULL)
        delete vpu_kernel_accumsquare_node_map[(void*)node];

    vpu_kernel_accumsquare_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t accumweighted_vpu_kernel = {
    VX_KERNEL_ACCUMULATE_WEIGHTED,
    "com.samsung.vpu.accumulate_weighted",
    vxAccumWeightedKernel,
    accumscale_kernel_params, dimof(accumscale_kernel_params),
    vxAccumWeightedInputValidator,
    vxAccumScaleOutputValidator,
    vxAccumWeightedInitializer,
    vxAccumWeightedDeinitializer,
};

vx_kernel_description_t accumsquare_vpu_kernel = {
    VX_KERNEL_ACCUMULATE_SQUARE,
    "com.samsung.vpu.accumulate_square",
    vxAccumSquareKernel,
    accumscale_kernel_params, dimof(accumscale_kernel_params),
    vxAccumSquareInputValidator,
    vxAccumScaleOutputValidator,
    vxAccumSquareInitializer,
    vxAccumSquareDeinitializer,
};

