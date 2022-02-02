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

#include "ExynosVpuKernelFastCorners.h"

#define USE_MICRO_KERNEL    0

using namespace std;
using namespace android;

/* A map of kernel's instance, the index is a node object address. */
#if (USE_MICRO_KERNEL==1)
static map<void*, vx_graph> vpu_kernel_fastcorners_node_map;
#else
static map<void*, ExynosVpuKernelFastCorners*> vpu_kernel_fastcorners_node_map;
#endif

vx_param_description_t fastcorners_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL}
};

extern vx_kernel_description_t fastcorners_vpu_kernel;

#if (USE_MICRO_KERNEL==1)
static vx_status vxFastCornersKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    vx_graph fastcorners_graph = vpu_kernel_fastcorners_node_map[(void*)node];
    if (fastcorners_graph != vxGetChildGraphOfNode(node)) {
        VXLOGE("child graph is not matching");
        return VX_FAILURE;
    }

    if (fastcorners_graph == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = vxProcessGraph(fastcorners_graph);
        if (status != VX_SUCCESS) {
            VXLOGE("main graph fail, err:%d, %p, %d, ", status, parameters, num);
            vxReferenceDisplayInfo((vx_reference)fastcorners_graph);
        }
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}
#else
static vx_status vxFastCornersKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;
    ExynosVpuKernelFastCorners *vpu_kernel_fastcorners = vpu_kernel_fastcorners_node_map[(void*)node];
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
#endif

static vx_status vxFastCornersInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelFastCorners::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = ExynosVpuKernelFastCorners::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

#if (USE_MICRO_KERNEL==1)
static vx_status vxFastCornersInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    vx_context context;
    vx_image input = (vx_image)parameters[0];
    vx_scalar strength_thresh = (vx_scalar)parameters[1];
    vx_scalar nonmax_suppression = (vx_scalar)parameters[2];
    vx_array corners = (vx_array)parameters[3];
    vx_scalar num_corners = (vx_scalar)parameters[4];

    vx_graph fastcorners_graph;
    vx_node micro0_node, micro1_node;
    vx_image inter_image;

    if (dimof(fastcorners_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(fastcorners_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_fastcorners_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    }

    context = vxGetContext((vx_reference)node);
    fastcorners_graph = vxCreateGraph(context);
    if (vxGetStatus((vx_reference)fastcorners_graph) != VX_SUCCESS) {
        VXLOGE("creating child graph fails");
        goto EXIT;
    }

    inter_image = vxCreateVirtualImage(fastcorners_graph, 0, 0, VX_DF_IMAGE_VIRT);
    micro0_node = vxFastCornesMicro0Node(fastcorners_graph, input, strength_thresh, nonmax_suppression, inter_image);
    if (vxGetStatus((vx_reference)micro0_node) != VX_SUCCESS) {
        VXLOGE("creating micro node fails");
        goto EXIT;
    }
    micro1_node = vxFastCornesMicro1Node(fastcorners_graph, input, inter_image, strength_thresh, nonmax_suppression, corners, num_corners);
    if (vxGetStatus((vx_reference)micro1_node) != VX_SUCCESS) {
        VXLOGE("creating micro node fails");
        goto EXIT;
    }

    status |= vxAddParameterToGraphByIndex(fastcorners_graph, micro0_node, 0);  // input
    status |= vxAddParameterToGraphByIndex(fastcorners_graph, micro0_node, 1);  // strength_thresh
    status |= vxAddParameterToGraphByIndex(fastcorners_graph, micro0_node, 2);  // nonmax_suppression
    status |= vxAddParameterToGraphByIndex(fastcorners_graph, micro1_node, 4);  // corners
    status |= vxAddParameterToGraphByIndex(fastcorners_graph, micro1_node, 5);  // num_corners
    if (status != VX_SUCCESS) {
        VXLOGE("adding parameter to graph fails");
        goto EXIT;
    }

    vxReleaseNode(&micro0_node);
    vxReleaseNode(&micro1_node);
    vxReleaseImage(&inter_image);

    status = vxVerifyGraph(fastcorners_graph);
    if (status != VX_SUCCESS) {
        VXLOGE("verifying child graph fails");
        goto EXIT;
    }

    status = vxSetChildGraphOfNode(node, fastcorners_graph);
    if (status != VX_SUCCESS) {
        VXLOGE("setting child graph fails");
        goto EXIT;
    }

    vpu_kernel_fastcorners_node_map[(void*)node] = fastcorners_graph;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    vx_graph fastcorners_graph = NULL;

    if (vpu_kernel_fastcorners_node_map[(void*)node] != NULL) {
        fastcorners_graph = vpu_kernel_fastcorners_node_map[(void*)node];
    } else {
        VXLOGE("can't find child graph in node_map");
        return VX_FAILURE;
    }

    if (fastcorners_graph != vxGetChildGraphOfNode(node)) {
        VXLOGE("child graph is not matching");
        vxReferenceDisplayInfo((vx_reference)fastcorners_graph);
        vxReferenceDisplayInfo((vx_reference)vxGetChildGraphOfNode(node));
        return VX_FAILURE;
    }

    vxReleaseGraph(&fastcorners_graph);
    vxSetChildGraphOfNode(node, NULL);

    vpu_kernel_fastcorners_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}
#else   // #if (USE_MICRO_KERNEL==1)
static vx_status vxFastCornersInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosVpuKernelFastCorners *fastcorners;

    if (dimof(fastcorners_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(fastcorners_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (vpu_kernel_fastcorners_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        fastcorners = new ExynosVpuKernelFastCorners(fastcorners_vpu_kernel.name, dimof(fastcorners_kernel_params));
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

    vpu_kernel_fastcorners_node_map[(void*)node] = fastcorners;

EXIT:
    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    if (vpu_kernel_fastcorners_node_map[(void*)node] != NULL)
        delete vpu_kernel_fastcorners_node_map[(void*)node];

    vpu_kernel_fastcorners_node_map.erase((void*)node);

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}
#endif

vx_kernel_description_t fastcorners_vpu_kernel = {
    VX_KERNEL_FAST_CORNERS,
    "com.samsung.vpu.fast_corners",
    vxFastCornersKernel,
    fastcorners_kernel_params, dimof(fastcorners_kernel_params),
    vxFastCornersInputValidator,
    vxFastCornersOutputValidator,
    vxFastCornersInitializer,
    vxFastCornersDeinitializer,
};
