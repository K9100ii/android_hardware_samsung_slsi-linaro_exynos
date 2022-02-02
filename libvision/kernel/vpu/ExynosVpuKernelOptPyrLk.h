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

#ifndef EXYNOS_VPU_KERNEL_OPTPYRLK_H
#define EXYNOS_VPU_KERNEL_OPTPYRLK_H

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"
#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

#define MAX_PYRAMID_LEVEL 10

namespace android {

class ExynosVpuKernelOptPyrLk : public ExynosVpuKernel {
private:
    vx_size m_pyramid_level;

    vx_int16 *m_gradient_x_base[MAX_PYRAMID_LEVEL];
    vx_int16 *m_gradient_y_base[MAX_PYRAMID_LEVEL];

    /* temporal array to modify input array point */
    vx_array m_prev_pts_mod;

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    vx_status initTaskFromApi(void);

    ExynosVpuTaskIf* initTask0FromBinary(void);
    ExynosVpuTaskIf* initTasknFromApi(vx_uint32 task_index);
    ExynosVpuTaskIf* initLastTaskFromApi(vx_uint32 task_index);

    vx_status LKTracker(
        const vx_image prevImg, vx_int16 *prevDerivIx, vx_int16 *prevDerivIy, const vx_image nextImg,
        const vx_array prevPts, vx_array nextPts,
        vx_scalar winSize_s, vx_scalar criteria_s,
        vx_uint32 level,vx_scalar epsilon,
        vx_scalar num_iterations);

public:
    /* Contructor */
    ExynosVpuKernelOptPyrLk(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernelOptPyrLk();

    vx_status opticalFlowPyrLk(const vx_reference parameters[]);

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

class ExynosVpuTaskIfWrapperLk : public ExynosVpuTaskIfWrapper {
private:
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperLk(ExynosVpuKernelOptPyrLk *kernel, vx_uint32 task_index);
};

}; // namespace android
#endif
