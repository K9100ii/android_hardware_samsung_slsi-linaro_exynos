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
#include "ExynosScoreKernelFastCorners.h"

using namespace std;
using namespace android;
using namespace score;

/* ExynosScoreKernel instance map from an index of node object address. */
static map<void*, ExynosScoreKernelFastCorners*> score_kernel_fastcorners_node_map;

vx_param_description_t fastcorners_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL}
};

extern vx_kernel_description_t fastcorners_score_kernel;

static vx_status vxFastCornersKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status;
    ExynosScoreKernelFastCorners *fastcorners = score_kernel_fastcorners_node_map[(void*)node];
    if (fastcorners == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = fastcorners->kernelFunction(parameters);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelFastCorners::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelFastCorners::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosScoreKernelFastCorners *fastcorners;

    if (dimof(fastcorners_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(fastcorners_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (score_kernel_fastcorners_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        fastcorners = new ExynosScoreKernelFastCorners(fastcorners_score_kernel.name, dimof(fastcorners_kernel_params));
    }

    status = fastcorners->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete fastcorners;
        status = VX_FAILURE;
        goto EXIT;
    }

    score_kernel_fastcorners_node_map[(void*)node] = fastcorners;

EXIT:
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxFastCornersDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    ExynosScoreKernelFastCorners *fastcorners = score_kernel_fastcorners_node_map[(void*)node];

    if (fastcorners != NULL) {
        fastcorners->finalizeKernel();
        delete fastcorners;
    }

    score_kernel_fastcorners_node_map.erase((void*)node);

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t fastcorners_score_kernel = {
    VX_KERNEL_FAST_CORNERS,
    "com.samsung.score.fast_corners",
    vxFastCornersKernel,
    fastcorners_kernel_params, dimof(fastcorners_kernel_params),
    vxFastCornersInputValidator,
    vxFastCornersOutputValidator,
    vxFastCornersInitializer,
    vxFastCornersDeinitializer,
};
