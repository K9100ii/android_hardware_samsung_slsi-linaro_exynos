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

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "ExynosScoreKernel.h"
#include "score_kernel_module.h"

extern vx_kernel_description_t and_score_kernel;
extern vx_kernel_description_t cannyedge_score_kernel;
extern vx_kernel_description_t convolution_score_kernel;
extern vx_kernel_description_t fastcorners_score_kernel;
extern vx_kernel_description_t histogram_score_kernel;
extern vx_kernel_description_t lut_score_kernel;

static vx_kernel_description_t *score_kernels[] = {
    &and_score_kernel,
    &cannyedge_score_kernel,
    &convolution_score_kernel,
    &fastcorners_score_kernel,
    &histogram_score_kernel,
    &lut_score_kernel,
};

VX_API_ENTRY vx_status VX_API_CALL vxPublishKernels(vx_context context)
{
    EXYNOS_SCORE_KERNEL_IF_IN();

    vx_status status = VX_SUCCESS;

    vx_uint32 i;
    vx_uint32 num_score_kernels = dimof(score_kernels);

    for (vx_uint32 k = 0; k < num_score_kernels; k++)
    {
        vx_kernel kernel = vxAddKernel(context,
                             score_kernels[k]->name,
                             score_kernels[k]->enumeration,
                             score_kernels[k]->function,
                             score_kernels[k]->numParams,
                             score_kernels[k]->input_validate,
                             score_kernels[k]->output_validate,
                             score_kernels[k]->initialize,
                             score_kernels[k]->deinitialize);

        if (kernel)
        {
            vx_uint32 num_kernel_params = score_kernels[k]->numParams;
            vx_param_description_t *parameters  = score_kernels[k]->parameters;

            for (vx_uint32 p = 0; p < num_kernel_params; p++)
            {
                status = vxAddParameterToKernel(kernel, p, parameters[p].direction, parameters[p].data_type, parameters[p].state);
                if (status != VX_SUCCESS) {
                    VXLOGE("s: add parameter to kernel fail(%d)", status);
                }
            }

            status = vxFinalizeKernel(kernel);
            if (status != VX_SUCCESS) {
                VXLOGE("s: finalize kernel fail(%d)", status);
            }
        } else {
            VXLOGE("s: add kernel fail");
        }
    }

    EXYNOS_SCORE_KERNEL_IF_OUT();

    return status;
}

