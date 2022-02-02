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

#ifndef EXYNOS_VPU_KERNEL_SAITFR_H
#define EXYNOS_VPU_KERNEL_SAITFR_H

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"
#include "ExynosVpu3dnnProcess.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

struct buffer_addr_size {
    char *addr;
    uint32_t size;
};

typedef struct _cnn_buffer_info_t {
    uint32_t io_direction;
    uint32_t io_index;

    /* memmap index in td */
    uint32_t memmap_index;

    struct buffer_addr_size *default_buf_array;
} cnn_buffer_info_t;

class ExynosVpuKernelCnn: public ExynosVpuKernel {
protected:
    void allocAndFillBuf(cnn_buffer_info_t *buf_info, vx_uint32 buf_size, io_buffer_info_t *io_buffer_info);

public:
    /* Contructor */
    ExynosVpuKernelCnn(vx_char *name, vx_uint32 param_num);
};

class ExynosVpuKernelSaitFrEa: public ExynosVpuKernelCnn {
private:

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    ExynosVpuTaskIf* initTask0FromBinary(void);

public:
    /* Contructor */
    ExynosVpuKernelSaitFrEa(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernelSaitFrEa();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

class ExynosVpuTaskIfWrapperSaitFrEa : public ExynosVpuTaskIfWrapper {
private:
    vx_status preProcessTask(const vx_reference parameters[]);
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperSaitFrEa(ExynosVpuKernelSaitFrEa *kernel, vx_uint32 task_index);
};

class ExynosVpuKernelSaitFrLa: public ExynosVpuKernelCnn {
private:

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    ExynosVpuTaskIf* initTask0FromBinary(void);

public:
    /* Contructor */
    ExynosVpuKernelSaitFrLa(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernelSaitFrLa();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

class ExynosVpuTaskIfWrapperSaitFrLa : public ExynosVpuTaskIfWrapper {
private:
    vx_status preProcessTask(const vx_reference parameters[]);
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperSaitFrLa(ExynosVpuKernelSaitFrLa *kernel, vx_uint32 task_index);
};

}; // namespace android
#endif
