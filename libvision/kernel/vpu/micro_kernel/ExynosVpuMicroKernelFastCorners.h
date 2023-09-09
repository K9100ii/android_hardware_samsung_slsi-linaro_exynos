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

#ifndef EXYNOS_VPU_MICRO_KERNEL_FASTCORNERS_H
#define EXYNOS_VPU_MICRO_KERNEL_FASTCORNERS_H

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"
#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

class ExynosVpuMicroKernelFastCorners0 : public ExynosVpuKernel {
private:
    vx_float32  m_thresh;
    vx_bool  m_nms_enable;

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    vx_status initTaskFromApi(void);

    ExynosVpuTaskIf* initTask0FromApi(void);

public:
    /* Contructor */
    ExynosVpuMicroKernelFastCorners0(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuMicroKernelFastCorners0();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

class ExynosVpuMicroKernelFastCorners1 : public ExynosVpuKernel {
private:
    vx_float32  m_thresh;
    vx_bool  m_nms_enable;

    vx_bool m_corner_num_enable;

    /* the buffer for receiving the number of corners from driver */
    vx_size *m_corner_num_ptr;

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    vx_status initTaskFromApi(void);

    ExynosVpuTaskIf* initTask0FromApi(void);

public:
    /* Contructor */
    ExynosVpuMicroKernelFastCorners1(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuMicroKernelFastCorners1();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

}; // namespace android
#endif

