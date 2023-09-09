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

#include "vpu_kernel_internal.h"

#include "ExynosVpuMicroKernelFastCorners.h"

using namespace std;
using namespace android;

/* A map of kernel's instance, the index is a node object address. */
static map<void*, ExynosVpuMicroKernelFastCorners0*> micro_fastcorners0_node_map;

vx_param_description_t fastcorners0_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
};

extern vx_kernel_description_t fastcorners0_vpu_micro_kernel;

static vx_status vxFastCorners0MicroKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuMicroKernelFastCorners0 *vpu_kernel_fastcorners0 = micro_fastcorners0_node_map[(void*)node];
    if (vpu_kernel_fastcorners0 == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_fastcorners0->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners0MicroInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuMicroKernelFastCorners0::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners0MicroOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuMicroKernelFastCorners0::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners0MicroInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuMicroKernelFastCorners0 *fastcorners;

    if (dimof(fastcorners0_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(fastcorners0_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (micro_fastcorners0_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        fastcorners = new ExynosVpuMicroKernelFastCorners0(fastcorners0_vpu_micro_kernel.name, dimof(fastcorners0_kernel_params));
    }

    status = fastcorners->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete fastcorners;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = fastcorners->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete fastcorners;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    micro_fastcorners0_node_map[(void*)node] = fastcorners;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners0MicroDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (micro_fastcorners0_node_map[(void*)node] != NULL)
        delete micro_fastcorners0_node_map[(void*)node];

    micro_fastcorners0_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t fastcorners0_vpu_micro_kernel = {
    VX_KERNEL_INTERNAL_FAST_CORNERS0_MICRO,
    "com.samsung.vpu.fast_corners0_micro",
    vxFastCorners0MicroKernel,
    fastcorners0_kernel_params, dimof(fastcorners0_kernel_params),
    vxFastCorners0MicroInputValidator,
    vxFastCorners0MicroOutputValidator,
    vxFastCorners0MicroInitializer,
    vxFastCorners0MicroDeinitializer,
};

/* A map of kernel's instance, the index is a node object address. */
static map<void*, ExynosVpuMicroKernelFastCorners1*> micro_fastcorners1_node_map;

vx_param_description_t fastcorners1_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL}
};

extern vx_kernel_description_t fastcorners1_vpu_micro_kernel;

static vx_status vxFastCorners1MicroKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuMicroKernelFastCorners1 *vpu_kernel_fastcorners = micro_fastcorners1_node_map[(void*)node];
    if (vpu_kernel_fastcorners == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel_fastcorners->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners1MicroInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuMicroKernelFastCorners1::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners1MicroOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuMicroKernelFastCorners1::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners1MicroInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuMicroKernelFastCorners1 *fastcorners;

    if (dimof(fastcorners1_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(fastcorners1_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (micro_fastcorners1_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        fastcorners = new ExynosVpuMicroKernelFastCorners1(fastcorners1_vpu_micro_kernel.name, dimof(fastcorners1_kernel_params));
    }

    status = fastcorners->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete fastcorners;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = fastcorners->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete fastcorners;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    micro_fastcorners1_node_map[(void*)node] = fastcorners;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCorners1MicroDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (micro_fastcorners1_node_map[(void*)node] != NULL)
        delete micro_fastcorners1_node_map[(void*)node];

    micro_fastcorners1_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t fastcorners1_vpu_micro_kernel = {
    VX_KERNEL_INTERNAL_FAST_CORNERS1_MICRO,
    "com.samsung.vpu.fast_corners1_micro",
    vxFastCorners1MicroKernel,
    fastcorners1_kernel_params, dimof(fastcorners1_kernel_params),
    vxFastCorners1MicroInputValidator,
    vxFastCorners1MicroOutputValidator,
    vxFastCorners1MicroInitializer,
    vxFastCorners1MicroDeinitializer,
};

