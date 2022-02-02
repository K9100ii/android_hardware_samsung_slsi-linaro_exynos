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

#define LOG_TAG "ExynosVpu3dnnProcess"
#include <cutils/log.h>

#include "ExynosVpu3dnnProcess.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

void
ExynosVpu3dnnProcess::display_structure_info(uint32_t tab_num, struct vpul_3dnn_process *tdnn_process)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(3dnn_Proc) base_3dnn_ind: %d", MAKE_TAB(tap, tab_num), tdnn_process->base_3dnn_ind);
    VXLOGI("%s(3dnn_Proc) first_3dnn_layer: %d", MAKE_TAB(tap, tab_num), tdnn_process->first_3dnn_layer);
    VXLOGI("%s(3dnn_Proc) num_of_3dnn_layers: %d", MAKE_TAB(tap, tab_num), tdnn_process->num_of_3dnn_layers);
    VXLOGI("%s(3dnn_Proc) Priority: %d", MAKE_TAB(tap, tab_num), tdnn_process->Priority);
}

void
ExynosVpu3dnnProcess::displayProcessInfo(uint32_t tab_num)
{
    VXLOGE("not implemented yet, %d", tab_num);
}

ExynosVpu3dnnProcess::ExynosVpu3dnnProcess(ExynosVpuTask *task)
                                                                : ExynosVpuVertex(task, VPUL_VERTEXT_3DNN_PROC)
{
    m_tdnn_proc_base = 0;
}

ExynosVpu3dnnProcess::ExynosVpu3dnnProcess(ExynosVpuTask *task, const struct vpul_vertex *vertex_info)
                                                                : ExynosVpuVertex(task, vertex_info)
{
    m_tdnn_proc_base = 0;
}

ExynosVpu3dnnProcess::~ExynosVpu3dnnProcess()
{

}

ExynosVpu3dnnProcessBase*
ExynosVpu3dnnProcess::getTdnnProcBase(void)
{
    if (m_tdnn_proc_base==NULL) {
        VXLOGE("tdnn_proc_base is not assigned");
    }

    return m_tdnn_proc_base;
}

status_t
ExynosVpu3dnnProcess::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuVertex::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("update vertex structure fails");
        goto EXIT;
    }

    if (m_tdnn_proc_base == NULL) {
        VXLOGE("tdnnProcBase is not assigned");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    m_vertex_info.proc3dnn.base_3dnn_ind = m_tdnn_proc_base->getIndex();

EXIT:
    return ret;
}

status_t
ExynosVpu3dnnProcess::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuVertex::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("updating vertex object fails");
        goto EXIT;
    }

    m_tdnn_proc_base = getTask()->getTdnnProcBase(m_vertex_info.proc3dnn.base_3dnn_ind);
    if (m_tdnn_proc_base == NULL) {
        VXLOGE("getting tdnnProcBase fails");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

EXIT:
    return ret;
}

}; /* namespace android */
