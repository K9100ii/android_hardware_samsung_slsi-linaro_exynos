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

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "vpu_kernel_module.h"

#include "ExynosVpuKernel.h"

extern vx_kernel_description_t colorconv_vpu_kernel;
extern vx_kernel_description_t channelextract_vpu_kernel;
extern vx_kernel_description_t channelcomb_vpu_kernel;
extern vx_kernel_description_t sobel3x3_vpu_kernel;
extern vx_kernel_description_t magnitude_vpu_kernel;
extern vx_kernel_description_t phase_vpu_kernel;
extern vx_kernel_description_t scaleimage_vpu_kernel;
extern vx_kernel_description_t lut_vpu_kernel;
extern vx_kernel_description_t histogram_vpu_kernel;
extern vx_kernel_description_t equalizehist_vpu_kernel;
extern vx_kernel_description_t absdiff_vpu_kernel;
extern vx_kernel_description_t meanstddev_vpu_kernel;
extern vx_kernel_description_t threshold_vpu_kernel;
extern vx_kernel_description_t integralimage_vpu_kernel;
extern vx_kernel_description_t dilate3x3_vpu_kernel;
extern vx_kernel_description_t erode3x3_vpu_kernel;
extern vx_kernel_description_t median3x3_vpu_kernel;
extern vx_kernel_description_t box3x3_vpu_kernel;
extern vx_kernel_description_t gaussian3x3_vpu_kernel;
extern vx_kernel_description_t convolution_vpu_kernel;
extern vx_kernel_description_t pyramid_vpu_kernel;
extern vx_kernel_description_t accum_vpu_kernel;
extern vx_kernel_description_t accumweighted_vpu_kernel;
extern vx_kernel_description_t accumsquare_vpu_kernel;
extern vx_kernel_description_t minmaxloc_vpu_kernel;
extern vx_kernel_description_t convertdepth_vpu_kernel;
extern vx_kernel_description_t cannyedge_vpu_kernel;
extern vx_kernel_description_t bitwiseand_vpu_kernel;
extern vx_kernel_description_t bitwiseor_vpu_kernel;
extern vx_kernel_description_t bitwisexor_vpu_kernel;
extern vx_kernel_description_t bitwisenot_vpu_kernel;
extern vx_kernel_description_t multiply_vpu_kernel;
extern vx_kernel_description_t add_vpu_kernel;
extern vx_kernel_description_t subtract_vpu_kernel;
extern vx_kernel_description_t harris_vpu_kernel;
extern vx_kernel_description_t fastcorners_vpu_kernel;
extern vx_kernel_description_t optpyrlk_vpu_kernel;
extern vx_kernel_description_t halfscalegaussian_vpu_kernel;
extern vx_kernel_description_t saitfr_ea_vpu_kernel;
extern vx_kernel_description_t saitfr_la_vpu_kernel;

static vx_kernel_description_t *vpu_kernels[] = {
    &colorconv_vpu_kernel,
    &channelextract_vpu_kernel,
    &channelcomb_vpu_kernel,
    &sobel3x3_vpu_kernel,
    &magnitude_vpu_kernel,
    &phase_vpu_kernel,
    &scaleimage_vpu_kernel,
    &lut_vpu_kernel,
    &histogram_vpu_kernel,
    &equalizehist_vpu_kernel,
    &absdiff_vpu_kernel,
    &meanstddev_vpu_kernel,
    &threshold_vpu_kernel,
    &integralimage_vpu_kernel,
    &dilate3x3_vpu_kernel,
    &erode3x3_vpu_kernel,
    &median3x3_vpu_kernel,
    &box3x3_vpu_kernel,
    &gaussian3x3_vpu_kernel,
    &convolution_vpu_kernel,
    &pyramid_vpu_kernel,
    &accum_vpu_kernel,
    &accumweighted_vpu_kernel,
    &accumsquare_vpu_kernel,
    &minmaxloc_vpu_kernel,
    &convertdepth_vpu_kernel,
    &cannyedge_vpu_kernel,
    &bitwiseand_vpu_kernel,
    &bitwiseor_vpu_kernel,
    &bitwisexor_vpu_kernel,
    &bitwisenot_vpu_kernel,
    &multiply_vpu_kernel,
    &add_vpu_kernel,
    &subtract_vpu_kernel,
    &harris_vpu_kernel,
    &fastcorners_vpu_kernel,
    &optpyrlk_vpu_kernel,
    &halfscalegaussian_vpu_kernel,
    &saitfr_ea_vpu_kernel,
    &saitfr_la_vpu_kernel
};

VX_API_ENTRY vx_status VX_API_CALL vxPublishKernels(vx_context context)
{
    EXYNOS_VPU_KERNEL_IF_IN();

    vx_status status = VX_SUCCESS;

    vx_uint32 i;
    vx_uint32 num_vpu_kernels = dimof(vpu_kernels);

    for (vx_uint32 k = 0; k < num_vpu_kernels; k++)
    {
        vx_kernel kernel = vxAddKernel(context,
                             vpu_kernels[k]->name,
                             vpu_kernels[k]->enumeration,
                             vpu_kernels[k]->function,
                             vpu_kernels[k]->numParams,
                             vpu_kernels[k]->input_validate,
                             vpu_kernels[k]->output_validate,
                             vpu_kernels[k]->initialize,
                             vpu_kernels[k]->deinitialize);

        if (kernel)
        {
            vx_uint32 num_kernel_params = vpu_kernels[k]->numParams;
            vx_param_description_t *parameters  = vpu_kernels[k]->parameters;

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

    EXYNOS_VPU_KERNEL_IF_OUT();

    return status;
}

