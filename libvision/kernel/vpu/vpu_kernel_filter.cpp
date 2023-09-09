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

#include "ExynosVpuKernelFilter.h"

using namespace std;
using namespace android;

/* A map of kernel's instance, the index is a node object address. */
static map<void*, ExynosVpuKernelMedian*> vpu_kernel_median_node_map;
static map<void*, ExynosVpuKernelBox*> vpu_kernel_box_node_map;
static map<void*, ExynosVpuKernelGaussian*> vpu_kernel_gaussian_node_map;

vx_param_description_t filter_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED}
};

extern vx_kernel_description_t median3x3_vpu_kernel;
extern vx_kernel_description_t box3x3_vpu_kernel;
extern vx_kernel_description_t gaussian3x3_vpu_kernel;

static vx_status vxFilterInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelFilter::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFilterOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelFilter::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxMedianKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelMedian *vpu_kernel = vpu_kernel_median_node_map[(void*)node];
    if (vpu_kernel == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxMedianInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelMedian *median;

    if (dimof(filter_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(filter_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_median_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        median = new ExynosVpuKernelMedian(median3x3_vpu_kernel.name, dimof(filter_kernel_params));
    }

    status = median->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete median;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = median->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete median;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_median_node_map[(void*)node] = median;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxMedianDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_median_node_map[(void*)node] != NULL)
        delete vpu_kernel_median_node_map[(void*)node];

    vpu_kernel_median_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBoxKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelBox *vpu_kernel = vpu_kernel_box_node_map[(void*)node];
    if (vpu_kernel == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBoxInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelBox *box;

    if (dimof(filter_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(filter_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_box_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        box = new ExynosVpuKernelBox(box3x3_vpu_kernel.name, dimof(filter_kernel_params));
    }

    status = box->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete box;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = box->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete box;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_box_node_map[(void*)node] = box;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxBoxDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_box_node_map[(void*)node] != NULL)
        delete vpu_kernel_box_node_map[(void*)node];

    vpu_kernel_box_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxGaussianKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelGaussian *vpu_kernel = vpu_kernel_gaussian_node_map[(void*)node];
    if (vpu_kernel == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vpu_kernel->kernelFunction(node, parameters, num);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxGaussianInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelGaussian *gaussian;

    if (dimof(filter_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(filter_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_gaussian_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        gaussian = new ExynosVpuKernelGaussian(gaussian3x3_vpu_kernel.name, dimof(filter_kernel_params));
    }

    status = gaussian->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete gaussian;
        status = VX_FAILURE;
        goto EXIT;
    }
    status = gaussian->setupDriver();
    if (status != VX_SUCCESS) {
        VXLOGE("setup driver node of kernel fails at initialize, err:%d", status);
        delete gaussian;
        status = VX_ERROR_NO_RESOURCES;
        goto EXIT;
    }

    vpu_kernel_gaussian_node_map[(void*)node] = gaussian;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxGaussianDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_gaussian_node_map[(void*)node] != NULL)
        delete vpu_kernel_gaussian_node_map[(void*)node];

    vpu_kernel_gaussian_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t median3x3_vpu_kernel = {
    VX_KERNEL_MEDIAN_3x3,
    "com.samsung.vpu.median_3x3",
    vxMedianKernel,
    filter_kernel_params, dimof(filter_kernel_params),
    vxFilterInputValidator,
    vxFilterOutputValidator,
    vxMedianInitializer,
    vxMedianDeinitializer,
};

vx_kernel_description_t box3x3_vpu_kernel = {
    VX_KERNEL_BOX_3x3,
    "com.samsung.vpu.box_3x3",
    vxBoxKernel,
    filter_kernel_params, dimof(filter_kernel_params),
    vxFilterInputValidator,
    vxFilterOutputValidator,
    vxBoxInitializer,
    vxBoxDeinitializer,
};

vx_kernel_description_t gaussian3x3_vpu_kernel = {
    VX_KERNEL_GAUSSIAN_3x3,
    "com.samsung.vpu.gaussian_3x3",
    vxGaussianKernel,
    filter_kernel_params, dimof(filter_kernel_params),
    vxFilterInputValidator,
    vxFilterOutputValidator,
    vxGaussianInitializer,
    vxGaussianDeinitializer,
};

