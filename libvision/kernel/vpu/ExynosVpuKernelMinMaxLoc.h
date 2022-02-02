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

#ifndef EXYNOS_VPU_KERNEL_MINMAXLOC_H
#define EXYNOS_VPU_KERNEL_MINMAXLOC_H

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

struct key_triple {
    vx_uint16 x;
    vx_uint16 y;
    vx_uint16 val;
};

class ExynosVpuKernelMinMaxLoc : public ExynosVpuKernel {
private:
    vx_df_image m_input_format;
    vx_int16 m_min_value;
    vx_int16 m_max_value;

    vx_bool m_min_loc_enable;
    vx_bool m_max_loc_enable;
    vx_bool m_min_cnt_enable;
    vx_bool m_max_cnt_enable;

    vx_uint32 m_min_array_capacity;
    vx_uint32 m_max_array_capacity;

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    vx_status initTaskFromApi(void);

    ExynosVpuTaskIf* initTask0FromBinary(void);
    ExynosVpuTaskIf* initTask0FromApi(void);
    ExynosVpuTaskIf* initTask1FromApi(void);
    ExynosVpuTaskIf* initTask2FromApi(void);

public:
    /* Contructor */
    ExynosVpuKernelMinMaxLoc(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernelMinMaxLoc();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);

    vx_df_image getInputFormat(void)
    {
        return m_input_format;
    }
    void reportMinValue(vx_int16 min_value)
    {
        m_min_value = min_value;
    }
    void reportMaxValue(vx_int16 max_value)
    {
        m_max_value = max_value;
    }
    vx_int16 getMinValue(void)
    {
        return m_min_value;
    }
    vx_int16 getMaxValue(void)
    {
        return m_max_value;
    }
};

class ExynosVpuTaskIfWrapperRois : public ExynosVpuTaskIfWrapper {
private:

public:

private:
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperRois(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index);
};

class ExynosVpuTaskIfWrapperMaxArray : public ExynosVpuTaskIfWrapper {
private:

public:

private:
    vx_status preProcessTask(const vx_reference parameters[]);
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperMaxArray(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index);
};

class ExynosVpuTaskIfWrapperMinArray : public ExynosVpuTaskIfWrapper {
private:

public:

private:
    vx_status preProcessTask(const vx_reference parameters[]);
    vx_status postProcessTask(const vx_reference parameters[]);

public:
    ExynosVpuTaskIfWrapperMinArray(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index);
};

}; // namespace android
#endif
