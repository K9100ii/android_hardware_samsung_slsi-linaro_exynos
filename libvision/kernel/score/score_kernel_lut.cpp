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
#include "ExynosScoreKernelLut.h"

using namespace std;
using namespace android;
using namespace score;

/* ExynosScoreKernel instance map from an index of node object address. */
static map<void*, ExynosScoreKernelLut*> score_kernel_lut_node_map;

vx_param_description_t lut_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_LUT,   VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT,VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED}
};

extern vx_kernel_description_t lut_score_kernel;

static vx_status vxLutKernel(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status;
    ExynosScoreKernelLut *score_kernel_lut = score_kernel_lut_node_map[(void*)node];
    if (score_kernel_lut == NULL) {
        VXLOGE("kernel is not found");
        status = VX_ERROR_NOT_ALLOCATED;
    } else {
        status = score_kernel_lut->kernelFunction(parameters);
        if (status != VX_SUCCESS) {
            VXLOGE("main kernel fail, err:%d", status);
        }
    }

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxLutInputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelLut::inputValidator(node, index);
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxLutOutputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = ExynosScoreKernelLut::outputValidator(node, index, meta);
    if (status != VX_SUCCESS) {
        VXLOGE("output validation fail, err:%d", status);
    }
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxLutInitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    ExynosScoreKernelLut *lut;

    if (dimof(lut_kernel_params) != num) {
        VXLOGE("parameter number is not matching. expected:%d, received:%d", dimof(lut_kernel_params), num);
        status = VX_ERROR_INVALID_PARAMETERS;
        goto EXIT;
    }

    if (score_kernel_lut_node_map[(void*)node] != NULL) {
        VXLOGE("already allocated kernel");
        status = VX_ERROR_NOT_ALLOCATED;
        goto EXIT;
    } else {
        lut = new ExynosScoreKernelLut(lut_score_kernel.name, dimof(lut_kernel_params));
    }

    status = lut->initKernel(node, parameters, num);
    if (status != VX_SUCCESS) {
        VXLOGE("Init task of kernel fails at initialize, err:%d", status);
        delete lut;
        status = VX_FAILURE;
        goto EXIT;
    }

    score_kernel_lut_node_map[(void*)node] = lut;

EXIT:
    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

static vx_status vxLutDeinitializer(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    if (!node) {
        VXLOGE("node is null, %p, %d", parameters, num);
    }

    EXYNOS_SCORE_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    ExynosScoreKernelLut *lut = score_kernel_lut_node_map[(void*)node];

    if (lut != NULL) {
        lut->finalizeKernel();
        delete lut;
    }

    score_kernel_lut_node_map.erase((void*)node);

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

vx_kernel_description_t lut_score_kernel = {
    VX_KERNEL_TABLE_LOOKUP,
    "com.samsung.score.table_lookup",
    vxLutKernel,
    lut_kernel_params, dimof(lut_kernel_params),
    vxLutInputValidator,
    vxLutOutputValidator,
    vxLutInitializer,
    vxLutDeinitializer,
};
