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
#include <VX/vx_helper.h>

#include "ExynosVpuKernelAbsDiff.h"
#include "ExynosVpuKernelColorConv.h"
#include "ExynosVpuKernelFastCorners.h"

#include "vpu_test_api.h"

using namespace android;

extern vx_kernel_description_t absdiff_vpu_kernel;
extern vx_kernel_description_t colorconv_vpu_kernel;

void vpu_absDiff_test(void)
{
#if 0
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;

    ExynosVpuKernelAbsDiff *vpu_kernel_absdiff = new ExynosVpuKernelAbsDiff(absdiff_vpu_kernel.name, 3);
    status = vpu_kernel_absdiff->driverInterfaceTest();
    if (status == VX_SUCCESS) {
        VXLOGD("pass");
    } else {
        VXLOGE("fail, err:%d", status);
    }

    delete vpu_kernel_absdiff;

    EXYNOS_VPU_KERNEL_IF_OUT();
#endif
}

void vpu_colorConv_test(void)
{
#if 0
    EXYNOS_VPU_KERNEL_IF_IN();
    vx_status status;

    ExynosVpuKernelColorConv *vpu_kernel_colorconv = new ExynosVpuKernelColorConv(colorconv_vpu_kernel.name);
    status = vpu_kernel_colorconv->driverInterfaceTest();
    if (status == VX_SUCCESS) {
        VXLOGD("pass");
    } else {
        VXLOGE("fail, err:%d", status);
    }

    delete vpu_kernel_colorconv;

    EXYNOS_VPU_KERNEL_IF_OUT();
#endif
}

