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

#define LOG_TAG "ExynosScoreKernelInterface"
#include <cutils/log.h>

#include <map>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_helper.h>

#include "ExynosScoreKernel.h"
#include "ExynosScoreKernelConvolution.h"

using namespace std;
using namespace android;
using namespace score;

/* ExynosScoreKernel instance map from an index of node object address. */
static map<void*, ExynosScoreKernelConvolution*> score_kernel_convolution_node_map;

vx_param_description_t convolution_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_CONVOLUTION, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED}
};

extern vx_kernel_description_t convolution_score_kernel;

static vx_status vxConvolutionKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status;
    ExynosScoreKernelConvolution *score_kernel_convolution = score_kernel_convolution_node_map[(void*)node];
    if (score_kernel_convolution == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = score_kernel_convolution->kernelFunction(parameters);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxConvolutionInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelConvolution::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxConvolutionOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelConvolution::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxConvolutionInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosScoreKernelConvolution *convolution;

    if (dimof(convolution_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(convolution_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (score_kernel_convolution_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        convolution = new ExynosScoreKernelConvolution(convolution_score_kernel.name, dimof(convolution_kernel_params));
    }

    status = convolution->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete convolution;
        status = VX_FAILURE;
        goto EXIT;
    }

    score_kernel_convolution_node_map[(void*)node] = convolution;

EXIT:
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxConvolutionDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    ExynosScoreKernelConvolution *convolution = score_kernel_convolution_node_map[(void*)node];

    if (convolution != NULL) {
        convolution->finalizeKernel();
        delete convolution;
    }

    score_kernel_convolution_node_map.erase((void*)node);

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t convolution_score_kernel = {
    VX_KERNEL_CUSTOM_CONVOLUTION,
    "com.samsung.score.VX_KERNEL_CUSTOM_CONVOLUTION",
    vxConvolutionKernel,
    convolution_kernel_params, dimof(convolution_kernel_params),
    vxConvolutionInputValidator,
    vxConvolutionOutputValidator,
    vxConvolutionInitializer,
    vxConvolutionDeinitializer,
};
