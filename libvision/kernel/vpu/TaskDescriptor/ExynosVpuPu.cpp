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

#define LOG_TAG "ExynosVpuPu"
#include <cutils/log.h>

#include "ExynosVpuTaskIf.h"
#include "ExynosVpuPu.h"

namespace android {

ExynosVpuPu*
ExynosVpuPuFactory::createPu(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
{
    ExynosVpuPu *pu = NULL;
    enum vpu_task_vertex_type vertex_type = subchain->getVertex()->getType();

    if (vertex_type == VPUL_VERTEXT_PROC) {
        switch (pu_instance) {
        case VPU_PU_DMAIN0:
        case VPU_PU_DMAIN_MNM0:
        case VPU_PU_DMAIN_MNM1:
        case VPU_PU_DMAIN_WIDE0:
        case VPU_PU_DMAIN_WIDE1:
            pu = new ExynosVpuProcPuDmaIn(subchain, pu_instance);
            break;
        case VPU_PU_DMAOT0:
        case VPU_PU_DMAOT_MNM0:
        case VPU_PU_DMAOT_MNM1:
        case VPU_PU_DMAOT_WIDE0:
        case VPU_PU_DMAOT_WIDE1:
            pu = new ExynosVpuProcPuDmaOut(subchain, pu_instance);
            break;
        case VPU_PU_SALB0:
        case VPU_PU_SALB1:
        case VPU_PU_SALB2:
        case VPU_PU_SALB3:
        case VPU_PU_SALB4:
        case VPU_PU_SALB5:
        case VPU_PU_SALB6:
        case VPU_PU_SALB7:
        case VPU_PU_SALB8:
            pu = new ExynosVpuPuSalb(subchain, pu_instance);
            break;
        case VPU_PU_CALB0:
        case VPU_PU_CALB1:
        case VPU_PU_CALB2:
            pu = new ExynosVpuPuCalb(subchain, pu_instance);
            break;
        case VPU_PU_ROIS0:
        case VPU_PU_ROIS1:
            pu = new ExynosVpuPuRois(subchain, pu_instance);
            break;
        case VPU_PU_CROP0:
        case VPU_PU_CROP1:
        case VPU_PU_CROP2:
            pu = new ExynosVpuPuCrop(subchain, pu_instance);
            break;
        case VPU_PU_MDE:
            pu = new ExynosVpuPuMde(subchain, pu_instance);
            break;
        case VPU_PU_MAP2LIST:
            pu = new ExynosVpuPuMapToList(subchain, pu_instance);
            break;
            break;
        case VPU_PU_NMS:
            pu = new ExynosVpuPuNms(subchain, pu_instance);
            break;
        case VPU_PU_SLF50:
        case VPU_PU_SLF51:
        case VPU_PU_SLF52:
        case VPU_PU_SLF70:
        case VPU_PU_SLF71:
        case VPU_PU_SLF72:
            pu = new ExynosVpuPuSlf(subchain, pu_instance);
            break;
        case VPU_PU_GLF50:
        case VPU_PU_GLF51:
            pu = new ExynosVpuPuGlf(subchain, pu_instance);
            break;
        case VPU_PU_CCM:
            pu = new ExynosVpuPuCcm(subchain, pu_instance);
            break;
        case VPU_PU_LUT:
            pu = new ExynosVpuPuLut(subchain, pu_instance);
            break;
        case VPU_PU_INTEGRAL:
            pu = new ExynosVpuPuIntegral(subchain, pu_instance);
            break;
        case VPU_PU_UPSCALER0:
        case VPU_PU_UPSCALER1:
            pu = new ExynosVpuPuUpscaler(subchain, pu_instance);
            break;
        case VPU_PU_DNSCALER0:
        case VPU_PU_DNSCALER1:
            pu = new ExynosVpuPuDownscaler(subchain, pu_instance);
            break;
        case VPU_PU_JOINER0:
        case VPU_PU_JOINER1:
            pu = new ExynosVpuPuJoiner(subchain, pu_instance);
            break;
        case VPU_PU_SPLITTER0:
        case VPU_PU_SPLITTER1:
            pu = new ExynosVpuPuSplitter(subchain, pu_instance);
            break;
        case VPU_PU_DUPLICATOR0:
        case VPU_PU_DUPLICATOR1:
        case VPU_PU_DUPLICATOR2:
        case VPU_PU_DUPLICATOR_WIDE0:
        case VPU_PU_DUPLICATOR_WIDE1:
            pu = new ExynosVpuPuDuplicator(subchain, pu_instance);
            break;
        case VPU_PU_HISTOGRAM:
            pu = new ExynosVpuPuHistogram(subchain, pu_instance);
            break;
        case VPU_PU_NLF:
            pu = new ExynosVpuPuNlf(subchain, pu_instance);
            break;
        case VPU_PU_FIFO_0:
        case VPU_PU_FIFO_1:
        case VPU_PU_FIFO_2:
        case VPU_PU_FIFO_3:
        case VPU_PU_FIFO_4:
        case VPU_PU_FIFO_5:
        case VPU_PU_FIFO_6:
        case VPU_PU_FIFO_7:
        case VPU_PU_FIFO_8:
        case VPU_PU_FIFO_9:
        case VPU_PU_FIFO_10:
        case VPU_PU_FIFO_11:
            pu = new ExynosVpuPuFifo(subchain, pu_instance);
            break;
        default:
            VXLOGE("%d is not defined yet", pu_instance);
            break;
        }
    } else if (vertex_type == VPUL_VERTEXT_3DNN_PROC) {
        switch (pu_instance) {
        case VPU_PU_DMAIN0:
        case VPU_PU_DMAIN_MNM0:
        case VPU_PU_DMAIN_MNM1:
        case VPU_PU_DMAIN_WIDE0:
        case VPU_PU_DMAIN_WIDE1:
            pu = new ExynosVpu3dnnPuDmaIn(subchain, pu_instance);
            break;
        case VPU_PU_DMAOT0:
        case VPU_PU_DMAOT_MNM0:
        case VPU_PU_DMAOT_MNM1:
        case VPU_PU_DMAOT_WIDE0:
        case VPU_PU_DMAOT_WIDE1:
            pu = new ExynosVpu3dnnPuDmaOut(subchain, pu_instance);
            break;
        case VPU_PU_CNN:
            pu = new ExynosVpuPuCnn(subchain, pu_instance);
            break;
        default:
            VXLOGE("%d is not defined yet", pu_instance);
            break;
        }
    } else {
        VXLOGE("un-supported vertex type:%d", vertex_type);
    }

    return pu;
}

ExynosVpuPu*
ExynosVpuPuFactory::createPu(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
{
    ExynosVpuPu *pu = NULL;
    enum vpu_task_vertex_type vertex_type = subchain->getVertex()->getType();

    if (vertex_type == VPUL_VERTEXT_PROC) {
        switch (pu_info->instance) {
        case VPU_PU_DMAIN0:
        case VPU_PU_DMAIN_MNM0:
        case VPU_PU_DMAIN_MNM1:
        case VPU_PU_DMAIN_WIDE0:
        case VPU_PU_DMAIN_WIDE1:
            pu = new ExynosVpuProcPuDmaIn(subchain, pu_info);
            break;
        case VPU_PU_DMAOT0:
        case VPU_PU_DMAOT_MNM0:
        case VPU_PU_DMAOT_MNM1:
        case VPU_PU_DMAOT_WIDE0:
        case VPU_PU_DMAOT_WIDE1:
            pu = new ExynosVpuProcPuDmaOut(subchain, pu_info);
            break;
        case VPU_PU_SALB0:
        case VPU_PU_SALB1:
        case VPU_PU_SALB2:
        case VPU_PU_SALB3:
        case VPU_PU_SALB4:
        case VPU_PU_SALB5:
        case VPU_PU_SALB6:
        case VPU_PU_SALB7:
        case VPU_PU_SALB8:
            pu = new ExynosVpuPuSalb(subchain, pu_info);
            break;
        case VPU_PU_CALB0:
        case VPU_PU_CALB1:
        case VPU_PU_CALB2:
            pu = new ExynosVpuPuCalb(subchain, pu_info);
            break;
        case VPU_PU_ROIS0:
        case VPU_PU_ROIS1:
            pu = new ExynosVpuPuRois(subchain, pu_info);
            break;
        case VPU_PU_CROP0:
        case VPU_PU_CROP1:
        case VPU_PU_CROP2:
            pu = new ExynosVpuPuCrop(subchain, pu_info);
            break;
        case VPU_PU_MDE:
            pu = new ExynosVpuPuMde(subchain, pu_info);
            break;
        case VPU_PU_MAP2LIST:
            pu = new ExynosVpuPuMapToList(subchain, pu_info);
            break;
            break;
        case VPU_PU_NMS:
            pu = new ExynosVpuPuNms(subchain, pu_info);
            break;
        case VPU_PU_SLF50:
        case VPU_PU_SLF51:
        case VPU_PU_SLF52:
        case VPU_PU_SLF70:
        case VPU_PU_SLF71:
        case VPU_PU_SLF72:
            pu = new ExynosVpuPuSlf(subchain, pu_info);
            break;
        case VPU_PU_GLF50:
        case VPU_PU_GLF51:
            pu = new ExynosVpuPuGlf(subchain, pu_info);
            break;
        case VPU_PU_CCM:
            pu = new ExynosVpuPuCcm(subchain, pu_info);
            break;
        case VPU_PU_LUT:
            pu = new ExynosVpuPuLut(subchain, pu_info);
            break;
        case VPU_PU_INTEGRAL:
            pu = new ExynosVpuPuIntegral(subchain, pu_info);
            break;
        case VPU_PU_UPSCALER0:
        case VPU_PU_UPSCALER1:
            pu = new ExynosVpuPuUpscaler(subchain, pu_info);
            break;
        case VPU_PU_DNSCALER0:
        case VPU_PU_DNSCALER1:
            pu = new ExynosVpuPuDownscaler(subchain, pu_info);
            break;
        case VPU_PU_JOINER0:
        case VPU_PU_JOINER1:
            pu = new ExynosVpuPuJoiner(subchain, pu_info);
            break;
        case VPU_PU_SPLITTER0:
        case VPU_PU_SPLITTER1:
            pu = new ExynosVpuPuSplitter(subchain, pu_info);
            break;
        case VPU_PU_DUPLICATOR0:
        case VPU_PU_DUPLICATOR1:
        case VPU_PU_DUPLICATOR2:
        case VPU_PU_DUPLICATOR_WIDE0:
        case VPU_PU_DUPLICATOR_WIDE1:
            pu = new ExynosVpuPuDuplicator(subchain, pu_info);
            break;
        case VPU_PU_HISTOGRAM:
            pu = new ExynosVpuPuHistogram(subchain, pu_info);
            break;
        case VPU_PU_NLF:
            pu = new ExynosVpuPuNlf(subchain, pu_info);
            break;
        case VPU_PU_FIFO_0:
        case VPU_PU_FIFO_1:
        case VPU_PU_FIFO_2:
        case VPU_PU_FIFO_3:
        case VPU_PU_FIFO_4:
        case VPU_PU_FIFO_5:
        case VPU_PU_FIFO_6:
        case VPU_PU_FIFO_7:
        case VPU_PU_FIFO_8:
        case VPU_PU_FIFO_9:
        case VPU_PU_FIFO_10:
        case VPU_PU_FIFO_11:
            pu = new ExynosVpuPuFifo(subchain, pu_info);
            break;
        default:
            VXLOGE("%d is not defined yet", pu_info->instance);
            break;
        }
    } else if (vertex_type == VPUL_VERTEXT_3DNN_PROC) {
        switch (pu_info->instance) {
        case VPU_PU_DMAIN0:
        case VPU_PU_DMAIN_MNM0:
        case VPU_PU_DMAIN_MNM1:
        case VPU_PU_DMAIN_WIDE0:
        case VPU_PU_DMAIN_WIDE1:
            pu = new ExynosVpu3dnnPuDmaIn(subchain, pu_info);
            break;
        case VPU_PU_DMAOT0:
        case VPU_PU_DMAOT_MNM0:
        case VPU_PU_DMAOT_MNM1:
        case VPU_PU_DMAOT_WIDE0:
        case VPU_PU_DMAOT_WIDE1:
            pu = new ExynosVpu3dnnPuDmaOut(subchain, pu_info);
            break;
        case VPU_PU_SLF50:
        case VPU_PU_SLF51:
        case VPU_PU_SLF52:
        case VPU_PU_SLF70:
        case VPU_PU_SLF71:
        case VPU_PU_SLF72:
            pu = new ExynosVpuPuSlf(subchain, pu_info);
            break;
        case VPU_PU_CNN:
            pu = new ExynosVpuPuCnn(subchain, pu_info);
            break;
        default:
            VXLOGE("%d is not defined yet", pu_info->instance);
            break;
        }
    } else {
        VXLOGE("un-supported vertex type:%d", vertex_type);
    }
    return pu;
}

static uint32_t getPuIndexInTask(uint8_t *base, uint8_t *pu)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = pu - base;
    remained_byte -= task->pus_vec_ofs;

    uint32_t i;
    for (i=0; i<task->t_num_of_pus; i++) {
        if (remained_byte == 0) {
            break;
        }

        remained_byte -= sizeof(struct vpul_pu);
    }

    if (i == task->t_num_of_pus) {
        VXLOGE("can't find pu index");
    }

    return i;
}

static uint32_t getBaseIndexInSubchain(uint8_t *base, uint8_t *pu)
{
    struct vpul_task *task = (struct vpul_task*)base;

    uint32_t base_pu_index = 0;

    uint32_t pu_index = getPuIndexInTask(base, pu);
    uint32_t remained_pu_num = pu_index + 1;

    uint32_t i;
    struct vpul_subchain *subchain;
    for (i=0; i<task->t_num_of_subchains; i++) {
        subchain = sc_ptr((struct vpul_task*)base, i);
        if (subchain->stype != VPUL_SUB_CH_HW)
            continue;

        if (remained_pu_num <= subchain->num_of_pus) {
            base_pu_index = getPuIndexInTask(base, base + subchain->pus_ofs);
            break;
        }

        remained_pu_num -= subchain->num_of_pus;
    }

    if (i == task->t_num_of_subchains) {
        VXLOGE("can't find subchain");
    }

    return base_pu_index;
}

static uint32_t getUpdatablePuIndexInTask(uint8_t *base, uint8_t *updatable_pu)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = updatable_pu - base;
    remained_byte -= task->invoke_params_vec_ofs;
    if (remained_byte < 0) {
        VXLOGE("task descriptor is broken");
        return 0;
    }

    uint32_t i;
    for (i=0; i<task->t_num_of_pu_params_on_invoke; i++) {
        if (remained_byte == 0) {
            break;
        }

        remained_byte -= sizeof(struct vpul_pu_location);
    }

    if (i == task->t_num_of_pu_params_on_invoke) {
        VXLOGE("can't find updata_pu index");
    }

    return i;
}

status_t
ExynosVpuPu::updateStructure(void)
{
    status_t ret = NO_ERROR;

    uint32_t out_port_num = 0;

    Vector<pair<ExynosVpuPu*, uint32_t>>::iterator port_iter;
    for (port_iter=m_out_port_slots.begin(); port_iter!=m_out_port_slots.end(); port_iter++) {
        if (port_iter->first != NULL) {
            out_port_num++;
        }
    }

    uint32_t i;

    /* pu connection */
    struct vpu_pu_input *input;
    for (port_iter=m_in_port_slots.begin(), i=0; port_iter!=m_in_port_slots.end(); port_iter++,  i++) {
        input = &m_pu_info.in_connect[i];
        if (port_iter->first != NULL) {
            input->pu_idx = port_iter->first->getIndexInSubchain();
            input->s_pu_out_idx = port_iter->second;
            m_pu_info.n_in_connect = i+1;
        } else {
            input->pu_idx = NO_PU_CONNECTED;
            input->s_pu_out_idx = 0;
        }
    }

    m_pu_info.n_out_connect = out_port_num;

    /* in, out size */
    m_pu_info.in_size_idx = getInSizeIndex();
    m_pu_info.out_size_idx = getOutSizeIndex();

    return ret;
}

status_t
ExynosVpuPu::updateObject(void)
{
    status_t ret = NO_ERROR;
    uint32_t i;
    ExynosVpuPu *pu_object;

    struct vpu_pu_input *pu_input_str;
    for (i=0; i<m_pu_info.n_in_connect; i++) {
        pu_input_str = &m_pu_info.in_connect[i];
        if (pu_input_str->pu_idx != NO_PU_CONNECTED) {
            pu_object = m_subchain->getPu(pu_input_str->pu_idx);
            m_in_port_slots.editItemAt(i).first = pu_object;
            m_in_port_slots.editItemAt(i).second = pu_input_str->s_pu_out_idx;

            ret = pu_object->updateObjectOutputPort(pu_input_str->s_pu_out_idx, this, i);
            if (ret != NO_ERROR) {
                return ret;
            }
        }
    }

    ExynosVpuVertex *vertex = m_subchain->getVertex();
    enum vpu_task_vertex_type vertex_type = vertex->getType();
    if (vertex_type == VPUL_VERTEXT_PROC) {
        ExynosVpuProcess *process = (ExynosVpuProcess*)vertex;
        m_in_io_size = process->getIoSize(m_pu_info.in_size_idx);
        m_out_io_size = process->getIoSize(m_pu_info.out_size_idx);
        if ((m_in_io_size==NULL) || (m_out_io_size==NULL)) {
            VXLOGE("getting in/out size fails");
            return INVALID_OPERATION;
        }
    } else if (vertex_type == VPUL_VERTEXT_3DNN_PROC) {
        ExynosVpu3dnnProcess *tdnn_process = (ExynosVpu3dnnProcess*)vertex;
        ExynosVpu3dnnProcessBase *tdnn_proc_base = tdnn_process->getTdnnProcBase();
        m_in_io_tdnn_size = tdnn_proc_base->getIoTdnnSize(m_pu_info.in_size_idx);
        m_out_io_tdnn_size = tdnn_proc_base->getIoTdnnSize(m_pu_info.out_size_idx);
        if ((m_in_io_tdnn_size==NULL) || (m_out_io_tdnn_size==NULL)) {
            VXLOGE("getting in/out size fails");
            return INVALID_OPERATION;
        }
    } else {
        VXLOGE("un-supported vtype:%d", vertex_type);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuPu::updateObjectOutputPort(uint32_t prev_pu_out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port)
{
    if (m_out_port_slots.size() <= prev_pu_out_port) {
        VXLOGE("out of bound in output port, size:%d, index:%d", m_out_port_slots.size(), prev_pu_out_port);
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    m_out_port_slots.editItemAt(prev_pu_out_port).first = post_pu;
    m_out_port_slots.editItemAt(prev_pu_out_port).second = post_pu_in_port;

    return NO_ERROR;
}

status_t
ExynosVpuPu::updateIoInfo(List<struct io_format_t> *io_format_list)
{
    VXLOGE("pu_%d, un-supported pu, %p", m_pu_index_in_task, io_format_list);
    return INVALID_OPERATION;
}

status_t
ExynosVpuPu::insertPuToTaskDescriptor(void)
{
    status_t ret = NO_ERROR;
    ret = updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating info fails");
        displayObjectInfo(0);
        goto EXIT;
    }

    ret = checkContraint();
    if (ret != NO_ERROR) {
        VXLOGE("%s has invalid configuration", getName());
        goto EXIT;
    }

    void *task_descriptor_base;
    task_descriptor_base = m_subchain->getVertex()->getTask()->getTaskIf()->getTaskDescriptorBase();

    struct vpul_pu *td_pu;
    td_pu = fst_pu_ptr((struct vpul_task*)task_descriptor_base) + getIndexInTask();

    memcpy(td_pu, &m_pu_info, sizeof(struct vpul_pu));

EXIT:
    return ret;
}

status_t
ExynosVpuPu::insertPuToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;
    ret = updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating info fails");
        displayObjectInfo(0);
        goto EXIT;
    }

    ret = checkContraint();
    if (ret != NO_ERROR) {
        VXLOGE("%s has invalid configuration", getName());
        goto EXIT;
    }

    struct vpul_pu *td_pu;
    td_pu = fst_pu_ptr((struct vpul_task*)task_descriptor_base) + getIndexInTask();

    memcpy(td_pu, &m_pu_info, sizeof(struct vpul_pu));

EXIT:
    return ret;
}

status_t
ExynosVpuPu::getOffsetAndValue(void *param_addr, uint32_t *ret_offset, uint32_t *ret_value)
{
    *ret_offset = (uint8_t*)param_addr - ((uint8_t*)&m_pu_info);
    *ret_offset &= ~(0x3);  /* 4 byte align */
    *ret_value = *((uint32_t*)((uint8_t*)&m_pu_info + *ret_offset));

    return NO_ERROR;
}

void
ExynosVpuPu::display_pu__prarm(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    switch(pu->instance) {
    case VPU_PU_DMAIN0:
    case VPU_PU_DMAIN_MNM0:
    case VPU_PU_DMAIN_MNM1:
    case VPU_PU_DMAIN_WIDE0:
    case VPU_PU_DMAIN_WIDE1:
        ExynosVpuProcPuDmaIn::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_DMAOT0:
    case VPU_PU_DMAOT_MNM0:
    case VPU_PU_DMAOT_MNM1:
    case VPU_PU_DMAOT_WIDE0:
    case VPU_PU_DMAOT_WIDE1:
        ExynosVpuProcPuDmaOut::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_SALB0:
    case VPU_PU_SALB1:
    case VPU_PU_SALB2:
    case VPU_PU_SALB3:
    case VPU_PU_SALB4:
    case VPU_PU_SALB6:
    case VPU_PU_SALB7:
    case VPU_PU_SALB8:
        ExynosVpuPuSalb::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_CALB0:
    case VPU_PU_CALB1:
    case VPU_PU_CALB2:
        ExynosVpuPuCalb::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_ROIS0:
    case VPU_PU_ROIS1:
        ExynosVpuPuRois::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_CROP0:
    case VPU_PU_CROP1:
    case VPU_PU_CROP2:
        ExynosVpuPuCrop::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_MDE:
        ExynosVpuPuMde::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_MAP2LIST:
        ExynosVpuPuMapToList::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_NMS:
        ExynosVpuPuNms::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_SLF50:
    case VPU_PU_SLF51:
    case VPU_PU_SLF52:
        ExynosVpuPuSlf::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_SLF70:
    case VPU_PU_SLF71:
    case VPU_PU_SLF72:
        ExynosVpuPuSlf::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_GLF50:
    case VPU_PU_GLF51:
        ExynosVpuPuGlf::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_CCM:
        ExynosVpuPuCcm::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_LUT:
        ExynosVpuPuLut::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_INTEGRAL:
        ExynosVpuPuIntegral::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_UPSCALER0:
    case VPU_PU_UPSCALER1:
        ExynosVpuPuUpscaler::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_DNSCALER0:
    case VPU_PU_DNSCALER1:
        ExynosVpuPuDownscaler::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_JOINER0:
    case VPU_PU_JOINER1:
        ExynosVpuPuJoiner::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_SPLITTER0:
    case VPU_PU_SPLITTER1:
        ExynosVpuPuSplitter::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_DUPLICATOR0:
    case VPU_PU_DUPLICATOR1:
    case VPU_PU_DUPLICATOR2:
    case VPU_PU_DUPLICATOR_WIDE0:
    case VPU_PU_DUPLICATOR_WIDE1:
        ExynosVpuPuDuplicator::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_HISTOGRAM:
        ExynosVpuPuHistogram::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_NLF:
        ExynosVpuPuNlf::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_CNN:
        ExynosVpuPuCnn::display_structure_info(tab_num, pu);
        break;
    case VPU_PU_FIFO_0:
    case VPU_PU_FIFO_1:
    case VPU_PU_FIFO_2:
    case VPU_PU_FIFO_3:
    case VPU_PU_FIFO_4:
    case VPU_PU_FIFO_5:
    case VPU_PU_FIFO_6:
    case VPU_PU_FIFO_7:
    case VPU_PU_FIFO_8:
    case VPU_PU_FIFO_9:
    case VPU_PU_FIFO_10:
    case VPU_PU_FIFO_11:
        ExynosVpuPuFifo::display_structure_info(tab_num, pu);
        break;
    default:
        VXLOGE("%d is not defined", pu->instance );
        break;
    }
}

void
ExynosVpuPu::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[Pu_X/X] op_type:%d, instance:%d", MAKE_TAB(tap, tab_num), pu->op_type, pu->instance);
    VXLOGI("%s[Pu_X/X] n_in_connect:%d, n_out_connect:%d", MAKE_TAB(tap, tab_num), pu->n_in_connect, pu->n_out_connect);
    VXLOGI("%s[Pu_X/X] in_size_idx:%d, out_size_idx:%d", MAKE_TAB(tap, tab_num), pu->in_size_idx, pu->out_size_idx);
    VXLOGI("%s[Pu_X/X] n_run_time_params_from_hw:%d", MAKE_TAB(tap, tab_num), pu->n_run_time_params_from_hw);

    uint32_t i;
    for (i=0; i<pu->n_in_connect; i++)
        VXLOGI("%s(in_%d) pu_idx:%d, s_pu_out_idx:%d", MAKE_TAB(tap, tab_num+1), i, pu->in_connect[i].pu_idx, pu->in_connect[i].s_pu_out_idx);


    for (i=0; i<pu->n_out_connect; i++)
        VXLOGI("%s(ot_%d) _________________", MAKE_TAB(tap, tab_num+1), i);

    display_pu__prarm(tab_num, pu);
}

void
ExynosVpuPu::display_structure_info(uint32_t tab_num, void *base, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    uint32_t pu_index = getPuIndexInTask((uint8_t*)base, (uint8_t*)pu);
    uint32_t base_index = getBaseIndexInSubchain((uint8_t*)base, (uint8_t*)pu);

    VXLOGI("%s[Pu_%d/%d] op_type:%d, instance:%d", MAKE_TAB(tap, tab_num), pu_index, pu_index-base_index, pu->op_type, pu->instance);
    VXLOGI("%s[Pu_%d/%d] n_in_connect:%d, n_out_connect:%d", MAKE_TAB(tap, tab_num), pu_index, pu_index-base_index, pu->n_in_connect, pu->n_out_connect);
    VXLOGI("%s[Pu_%d/%d] in_size_idx:%d, out_size_idx:%d", MAKE_TAB(tap, tab_num), pu_index, pu_index-base_index, pu->in_size_idx, pu->out_size_idx);
    VXLOGI("%s[Pu_%d/%d] n_run_time_params_from_hw:%d", MAKE_TAB(tap, tab_num), pu_index, pu_index-base_index, pu->n_run_time_params_from_hw);

    uint32_t i;
    for (i=0; i<pu->n_in_connect; i++)
        VXLOGI("%s(in_%d) pu_idx:%d, s_pu_out_idx:%d", MAKE_TAB(tap, tab_num+1), i, pu->in_connect[i].pu_idx, pu->in_connect[i].s_pu_out_idx);


    for (i=0; i<pu->n_out_connect; i++)
        VXLOGI("%s(ot_%d) _________________", MAKE_TAB(tap, tab_num+1), i);

    display_pu__prarm(tab_num, pu);
}

void
ExynosVpuPu::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    uint32_t in_port_num = 0;
    uint32_t out_port_num = 0;

    Vector<pair<ExynosVpuPu*, uint32_t>>::iterator port_iter;
    for (port_iter=m_in_port_slots.begin(); port_iter!=m_in_port_slots.end(); port_iter++) {
        if (port_iter->first != NULL) {
            in_port_num++;
        }
    }
    for (port_iter=m_out_port_slots.begin(); port_iter!=m_out_port_slots.end(); port_iter++) {
        if (port_iter->first != NULL) {
            out_port_num++;
        }
    }

    VXLOGI("%s[Pu_%d/%d] op_type:%d, instance:%d (%s)", MAKE_TAB(tap, tab_num), m_pu_index_in_task, m_pu_index_in_subchain, m_pu_info.op_type, m_pu_info.instance, m_name);
    VXLOGI("%s[Pu_%d/%d] n_in_connect:%d, n_out_connect:%d", MAKE_TAB(tap, tab_num), m_pu_index_in_task, m_pu_index_in_subchain, in_port_num, out_port_num);
    VXLOGI("%s[Pu_%d/%d] in_size_idx:%d, out_size_idx:%d", MAKE_TAB(tap, tab_num), m_pu_index_in_task, m_pu_index_in_subchain, getInSizeIndex(), getOutSizeIndex());
    VXLOGI("%s[Pu_%d/%d] n_run_time_params_from_hw:%d", MAKE_TAB(tap, tab_num), m_pu_index_in_task, m_pu_index_in_subchain, m_pu_info.n_run_time_params_from_hw);

    uint32_t i;
    uint32_t found_port_num = 0;
    for (port_iter=m_in_port_slots.begin(), i=0; port_iter!=m_in_port_slots.end(); port_iter++, i++) {
        if (found_port_num == in_port_num)
            break;

        if (port_iter->first != NULL) {
            VXLOGI("%s(in_%d) pu_idx:%d, s_pu_out_idx:%d", MAKE_TAB(tap, tab_num+1), i, port_iter->first->getIndexInSubchain(), port_iter->second);
            found_port_num++;
        } else {
            VXLOGI("%s(in_%d) NULL", MAKE_TAB(tap, tab_num+1), i);
        }
    }
    found_port_num = 0;
    for (port_iter=m_out_port_slots.begin(), i=0; port_iter!=m_out_port_slots.end(); port_iter++, i++) {
        if (found_port_num == out_port_num)
            break;

        if (port_iter->first != NULL) {
            VXLOGI("%s(ot_%d) pu_idx:%d, t_pu_in_idx:%d, instance:%d", MAKE_TAB(tap, tab_num+1), i, port_iter->first->getIndexInSubchain(), port_iter->second, 0);
            found_port_num++;
        } else {
            VXLOGI("%s(ot_%d) NULL", MAKE_TAB(tap, tab_num+1), i);
        }
    }
}

bool
ExynosVpuPu::connect(ExynosVpuPu *prev_pu, uint32_t prev_pu_out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port)
{
    if (prev_pu->connectPostPu(prev_pu_out_port, post_pu, post_pu_in_port) == false) {
        VXLOGE("can't add post vertex");
        return false;
    }

    if (post_pu->connectPrevPu(post_pu_in_port, prev_pu, prev_pu_out_port) == false) {
        VXLOGE("can't add post vertex");
        return false;
    }

    return true;
}

void
ExynosVpuPu::init(enum vpul_pu_instance pu_instance)
{
    switch (pu_instance) {
    case VPU_PU_DMAIN0:
    case VPU_PU_DMAIN_MNM0:
    case VPU_PU_DMAIN_MNM1:
    case VPU_PU_DMAIN_WIDE0:
    case VPU_PU_DMAIN_WIDE1:
        m_pu_info.op_type = VPUL_OP_DMA;
        setPortNum(0, 1);
        sprintf((char*)m_name, "DMAIN_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_DMAIN0);
        break;
    case VPU_PU_DMAOT0:
    case VPU_PU_DMAOT_MNM0:
    case VPU_PU_DMAOT_MNM1:
    case VPU_PU_DMAOT_WIDE0:
    case VPU_PU_DMAOT_WIDE1:
        m_pu_info.op_type = VPUL_OP_DMA;
        setPortNum(1, 0);
        sprintf((char*)m_name, "DMAOUT_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_DMAOT0);
        break;
    case VPU_PU_SALB0:
    case VPU_PU_SALB1:
    case VPU_PU_SALB2:
    case VPU_PU_SALB3:
    case VPU_PU_SALB4:
    case VPU_PU_SALB5:
    case VPU_PU_SALB6:
    case VPU_PU_SALB7:
    case VPU_PU_SALB8:
        m_pu_info.op_type = VPUL_OP_FULL_SALB;
        setPortNum(2, 1);
        sprintf((char*)m_name, "SALB_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_SALB0);
        break;
    case VPU_PU_CALB0:
    case VPU_PU_CALB1:
    case VPU_PU_CALB2:
        m_pu_info.op_type = VPUL_OP_FULL_CALB;
        setPortNum(2, 1);
        sprintf((char*)m_name, "CALB_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_CALB0);
        break;
    case VPU_PU_ROIS0:
    case VPU_PU_ROIS1:
        m_pu_info.op_type = VPUL_OP_ROI;
        setPortNum(3, 0);
        sprintf((char*)m_name, "ROIS_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_ROIS0);
        break;
    case VPU_PU_CROP0:
    case VPU_PU_CROP1:
    case VPU_PU_CROP2:
        m_pu_info.op_type = VPUL_OP_CROP;
        setPortNum(1, 1);
        sprintf((char*)m_name, "CROP_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_CROP0);
        break;
    case VPU_PU_MDE:
        m_pu_info.op_type = VPUL_OP_MDE;
        setPortNum(3, 2);
        sprintf((char*)m_name, "MDE_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_MDE);
        break;
    case VPU_PU_MAP2LIST:
        m_pu_info.op_type = VPUL_OP_MAP_2_LST;
        setPortNum(1, 2);
        sprintf((char*)m_name, "MAP2LIST_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_MAP2LIST);
        break;
        break;
    case VPU_PU_NMS:
        m_pu_info.op_type = VPUL_OP_NMS;
        setPortNum(1, 1);
        sprintf((char*)m_name, "NMS_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_NMS);
        break;
    case VPU_PU_SLF50:
    case VPU_PU_SLF51:
    case VPU_PU_SLF52:
    case VPU_PU_SLF70:
    case VPU_PU_SLF71:
    case VPU_PU_SLF72:
        m_pu_info.op_type = VPUL_OP_SEP_FLT;
        setPortNum(1, 2);
        if (pu_instance < VPU_PU_SLF70)
            sprintf((char*)m_name, "SLF5x5_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_SLF50);
        else
            sprintf((char*)m_name, "SLF7x7_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_SLF70);
        break;
    case VPU_PU_GLF50:
    case VPU_PU_GLF51:
        m_pu_info.op_type = VPUL_OP_GEN_FLT;
        setPortNum(1, 3);
        sprintf((char*)m_name, "GLF_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_GLF50);
        break;
    case VPU_PU_CCM:
        m_pu_info.op_type = VPUL_OP_CCM;
        setPortNum(3, 3);
        sprintf((char*)m_name, "CCM_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_CCM);
        break;
    case VPU_PU_LUT:
        m_pu_info.op_type = VPUL_OP_LUT;
        setPortNum(1, 1);
        sprintf((char*)m_name, "LUT_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_LUT);
        break;
    case VPU_PU_INTEGRAL:
        m_pu_info.op_type = VPUL_OP_INTEGRAL_IMG;
        setPortNum(1, 1);
        sprintf((char*)m_name, "INTEGRAL_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_INTEGRAL);
        break;
    case VPU_PU_UPSCALER0:
    case VPU_PU_UPSCALER1:
        m_pu_info.op_type = VPUL_OP_UPSCALER;
        setPortNum(1, 1);
        sprintf((char*)m_name, "UPSCALER_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_UPSCALER0);
        break;
    case VPU_PU_DNSCALER0:
    case VPU_PU_DNSCALER1:
        m_pu_info.op_type = VPUL_OP_DOWNSCALER;
        setPortNum(1, 1);
        sprintf((char*)m_name, "DNSCALER_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_DNSCALER0);
        break;
    case VPU_PU_JOINER0:
    case VPU_PU_JOINER1:
        m_pu_info.op_type = VPUL_OP_JOIN;
        setPortNum(4, 1);
        sprintf((char*)m_name, "JOINER_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_JOINER0);
        break;
    case VPU_PU_SPLITTER0:
    case VPU_PU_SPLITTER1:
        m_pu_info.op_type = VPUL_OP_SPLIT;
        setPortNum(1, 4);
        sprintf((char*)m_name, "SPLIT_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_SPLITTER0);
        break;
    case VPU_PU_DUPLICATOR0:
    case VPU_PU_DUPLICATOR1:
    case VPU_PU_DUPLICATOR2:
    case VPU_PU_DUPLICATOR_WIDE0:
    case VPU_PU_DUPLICATOR_WIDE1:
        m_pu_info.op_type = VPUL_OP_DUPLICATE;
        setPortNum(1, 2);
        sprintf((char*)m_name, "DUPLI_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_DUPLICATOR0);
        break;
    case VPU_PU_HISTOGRAM:
        m_pu_info.op_type = VPUL_OP_HIST;
        setPortNum(2, 0);
        sprintf((char*)m_name, "HIST_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_HISTOGRAM);
        break;
    case VPU_PU_NLF:
        m_pu_info.op_type = VPUL_OP_NLF_FLT;
        setPortNum(1, 1);
        sprintf((char*)m_name, "NLF_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_NLF);
        break;
    case VPU_PU_CNN:
        m_pu_info.op_type = VPUL_OP_CNN;
        setPortNum(5, 1);
        sprintf((char*)m_name, "CNN_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_CNN);
        break;
    case VPU_PU_FIFO_0:
    case VPU_PU_FIFO_1:
    case VPU_PU_FIFO_2:
    case VPU_PU_FIFO_3:
    case VPU_PU_FIFO_4:
    case VPU_PU_FIFO_5:
    case VPU_PU_FIFO_6:
    case VPU_PU_FIFO_7:
    case VPU_PU_FIFO_8:
    case VPU_PU_FIFO_9:
    case VPU_PU_FIFO_10:
    case VPU_PU_FIFO_11:
        m_pu_info.op_type = VPUL_OP_FIFO;
        setPortNum(1, 1);
        sprintf((char*)m_name, "FIFO_%d", (uint32_t)pu_instance - (uint32_t)VPU_PU_FIFO_0);
        break;
    default:
        VXLOGE("%d is not defined yet", pu_instance);
        break;
    }
}

ExynosVpuPu::ExynosVpuPu(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
{
    memset(&m_pu_info, 0x0, sizeof(m_pu_info));
    m_pu_info.instance = pu_instance;
    init(m_pu_info.instance);

    m_subchain = subchain;
    m_pu_index_in_task = subchain->addPu(this);
    m_pu_index_in_subchain = m_pu_index_in_task - subchain->getPu(0)->getIndexInTask();

    m_in_io_size = NULL;
    m_out_io_size = NULL;
    m_in_io_tdnn_size = NULL;
    m_out_io_tdnn_size = NULL;
}

ExynosVpuPu::ExynosVpuPu(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
{
    m_pu_info = *pu_info;

    m_subchain = subchain;
    m_pu_index_in_task = subchain->addPu(this);
    m_pu_index_in_subchain = m_pu_index_in_task - subchain->getPu(0)->getIndexInTask();

    m_in_io_size = NULL;
    m_out_io_size = NULL;
    m_in_io_tdnn_size = NULL;
    m_out_io_tdnn_size = NULL;

    init(m_pu_info.instance);
}

bool
ExynosVpuPu::connectPostPu(uint32_t out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port)
{
    if (m_out_port_slots.size() <= out_port) {
        VXLOGE("%s, out port_%d is out of bound(size:%d)", m_name, out_port, m_out_port_slots.size());
        return false;
    }

    pair<ExynosVpuPu*, uint32_t> *out_port_slot = &m_out_port_slots.editItemAt(out_port);
    if (out_port_slot->first != NULL) {
        VXLOGE("%s, out port_%d is already connected with %s", m_name, out_port, out_port_slot->first->getName());
        return false;
    }

    out_port_slot->first = post_pu;
    out_port_slot->second = post_pu_in_port;

    return true;
}

bool
ExynosVpuPu::connectPrevPu(uint32_t in_port, ExynosVpuPu *prev_pu, uint32_t prev_pu_out_port)
{
    if (m_in_port_slots.size() <= in_port) {
        VXLOGE("%s, in port_%d is out of bound(size:%d)", m_name, in_port, m_in_port_slots.size());
        return false;
    }

    pair<ExynosVpuPu*, uint32_t> *in_port_slot = &m_in_port_slots.editItemAt(in_port);
    if (in_port_slot->first != NULL) {
        VXLOGE("%s, out port_%d is already connected with %s", m_name, in_port, in_port_slot->first->getName());
        return false;
    }

    in_port_slot->first = prev_pu;
    in_port_slot->second = prev_pu_out_port;

    return true;
}

uint8_t*
ExynosVpuPu::getName()
{
    return m_name;
}

enum vpul_logical_op_types
ExynosVpuPu::getOpcode(void)
{
    return m_pu_info.op_type;
}

enum vpul_pu_instance
ExynosVpuPu::getInstance(void)
{
    return m_pu_info.instance;
}

ExynosVpuSubchain*
ExynosVpuPu::getSubchain(void)
{
    return m_subchain;
}

void*
ExynosVpuPu::getParameter(void)
{
    return &m_pu_info.params;
}

uint32_t
ExynosVpuPu::getIndexInTask(void)
{
    return m_pu_index_in_task;
}

uint32_t
ExynosVpuPu::getIndexInSubchain(void)
{
    return m_pu_index_in_subchain;
}

uint32_t
ExynosVpuPu::getTargetId(void)
{
    return PU_TARGET_ID(m_subchain->getId(), m_pu_info.instance);
}

void
ExynosVpuPu::setBypass(bool bypass_flag)
{
    m_pu_info.bypass = bypass_flag;
}

void
ExynosVpuPu::setSize(ExynosVpuIoSize *in_size, ExynosVpuIoSize *out_size)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_PROC) {
        VXLOGE("vertex type is not matching");
        return;
    }

    m_in_io_size = in_size;
    m_out_io_size = out_size;
}

void
ExynosVpuPu::setSize(ExynosVpuIo3dnnSize *in_size, ExynosVpuIo3dnnSize *out_size)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_3DNN_PROC) {
        VXLOGE("vertex type is not matching");
        return;
    }

    m_in_io_tdnn_size = in_size;
    m_out_io_tdnn_size = out_size;
}

uint32_t
ExynosVpuPu::getInSizeIndex(void)
{
    uint32_t index = 0;

    ExynosVpuVertex *vertex = m_subchain->getVertex();
    enum vpu_task_vertex_type vertex_type = vertex->getType();

    if (vertex_type == VPUL_VERTEXT_PROC) {
        ExynosVpuIoSize *io_size = getInIoSize();
        if (io_size == NULL) {
            VXLOGE("io_size is not assigned");
            return 0;
        }
        index = io_size->getIndex();;
    } else if (vertex_type == VPUL_VERTEXT_3DNN_PROC) {
        ExynosVpuIo3dnnSize *io_tdnn_size = getInIoTdnnSize();
        if (io_tdnn_size == NULL) {
            VXLOGE("io_tdnn_size is not assigned");
            return 0;
        }
        index = io_tdnn_size->getIndex();;
    } else {
        VXLOGE("un-supported vtype:%d", vertex_type);
        return 0;
    }

    return index;
}

uint32_t
ExynosVpuPu::getOutSizeIndex(void)
{
    uint32_t index = 0;

    ExynosVpuVertex *vertex = m_subchain->getVertex();
    enum vpu_task_vertex_type vertex_type = vertex->getType();

    if (vertex_type == VPUL_VERTEXT_PROC) {
        ExynosVpuIoSize *io_size = getOutIoSize();
        if (io_size == NULL) {
            VXLOGE("io_size is not assigned");
            return 0;
        }
        index = io_size->getIndex();;
    } else if (vertex_type == VPUL_VERTEXT_3DNN_PROC) {
        ExynosVpuIo3dnnSize *io_tdnn_size = getOutIoTdnnSize();
        if (io_tdnn_size == NULL) {
            VXLOGE("io_tdnn_size is not assigned");
            return 0;
        }
        index = io_tdnn_size->getIndex();;
    } else {
        VXLOGE("un-supported vtype:%d", vertex_type);
        return 0;
    }

    return index;
}

ExynosVpuIoSize*
ExynosVpuPu::getInIoSize(void)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_PROC) {
        VXLOGE("vertex type is not matching");
        return NULL;
    }

    if (m_in_io_size == NULL) {
        VXLOGE("io size is not assigned");
        return 0;
    }

    return m_in_io_size;
}

ExynosVpuIoSize*
ExynosVpuPu::getOutIoSize(void)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_PROC) {
        VXLOGE("vertex type is not matching");
        return NULL;
    }

    if (m_out_io_size == NULL) {
        VXLOGE("io size is not assigned");
        return 0;
    }

    return m_out_io_size;
}

ExynosVpuIo3dnnSize*
ExynosVpuPu::getInIoTdnnSize(void)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_3DNN_PROC) {
        VXLOGE("vertex type is not matching");
        return NULL;
    }

    if (m_in_io_tdnn_size == NULL) {
        VXLOGE("io size is not assigned");
        return 0;
    }

    return m_in_io_tdnn_size;
}

ExynosVpuIo3dnnSize*
ExynosVpuPu::getOutIoTdnnSize(void)
{
    if (m_subchain->getVertex()->getType() != VPUL_VERTEXT_3DNN_PROC) {
        VXLOGE("vertex type is not matching");
        return NULL;
    }

    if (m_out_io_tdnn_size == NULL) {
        VXLOGE("io size is not assigned");
        return 0;
    }

    return m_out_io_tdnn_size;
}

void
ExynosVpuPu::setPortNum(uint32_t in_port_num, uint32_t out_port_num)
{
    m_in_port_slots.insertAt(make_pair((ExynosVpuPu*)NULL, 0), 0, in_port_num);
    m_out_port_slots.insertAt(make_pair((ExynosVpuPu*)NULL, 0), 0, out_port_num);
}

status_t
ExynosVpuPu::setIoTypesDesc(ExynosVpuIoTypesDesc *iotypes_desc)
{
    VXLOGE("%s doesn't use iotyps, %p", m_name, iotypes_desc);
    return INVALID_OPERATION;
}

ExynosVpuIoTypesDesc*
ExynosVpuPu::getIoTypesDesc(void)
{
    VXLOGE("%s doesn't use iotyps", m_name);
    return NULL;
}

status_t
ExynosVpuPu::setMemmap(ExynosVpuMemmap *memmap)
{
    VXLOGE("%s doesn't use memmap, %p", m_name, memmap);
    return INVALID_OPERATION;
}

ExynosVpuMemmap*
ExynosVpuPu::getMemmap(void)
{
    VXLOGE("%s doesn't use memmap", m_name);
    return NULL;
}

status_t
ExynosVpuPu::getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list)
{
    VXLOGE("%s doesn't use memmap, %p", m_name, ret_memmap_list);
    return INVALID_OPERATION;
}

status_t
ExynosVpuPu::checkContraint()
{
    return NO_ERROR;
}

status_t
ExynosVpuPu::checkAndUpdatePuIoInfo(void)
{
    return NO_ERROR;
}

void
ExynosVpuUpdatablePu::display_structure_info(uint32_t tab_num, void *base, struct vpul_pu_location *updatable_pu)
{
    char tap[MAX_TAB_NUM];

    uint32_t updatable_pu_index = getUpdatablePuIndexInTask((uint8_t*)base, (uint8_t*)updatable_pu);

    VXLOGI("%s[Updatable_Pu_%d] vet:%d, sc:%d, pu:%d", MAKE_TAB(tap, tab_num), updatable_pu_index,
                                                                                    updatable_pu->vtx_idx,
                                                                                    updatable_pu->sc_idx_in_vtx,
                                                                                    updatable_pu->pu_idx_in_sc);
}

void
ExynosVpuUpdatablePu::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[Updatable_Pu_%d] vet:%d, sc:%d, pu:%d", MAKE_TAB(tap, tab_num), m_updatable_pu_index_in_task,
                                                                                    m_target_pu->getIndexInSubchain(),
                                                                                    m_target_pu->getSubchain()->getIndexInVertex(),
                                                                                    m_target_pu->getSubchain()->getVertex()->getIndex());
}

ExynosVpuUpdatablePu::ExynosVpuUpdatablePu(ExynosVpuTask *task, ExynosVpuPu * target_pu)
{
    memset(&m_updatable_pu_info, 0x0, sizeof(m_updatable_pu_info));

    m_task = task;
    m_updatable_pu_index_in_task = task->addUpdatablePu(this);

    m_target_pu = target_pu;
}

ExynosVpuUpdatablePu::ExynosVpuUpdatablePu(ExynosVpuTask *task, const struct vpul_pu_location *updatable_pu_info)
{
    m_updatable_pu_info = *updatable_pu_info;

    m_task = task;
    m_updatable_pu_index_in_task = task->addUpdatablePu(this);
}

status_t
ExynosVpuUpdatablePu::checkContraint()
{
    return NO_ERROR;
}

status_t
ExynosVpuUpdatablePu::updateStructure()
{
    m_updatable_pu_info.vtx_idx = m_target_pu->getSubchain()->getVertex()->getIndex();
    m_updatable_pu_info.sc_idx_in_vtx =  m_target_pu->getSubchain()->getIndexInVertex();
    m_updatable_pu_info.pu_idx_in_sc = m_target_pu->getIndexInSubchain();

    return NO_ERROR;
}

status_t
ExynosVpuUpdatablePu::updateObject()
{
    ExynosVpuVertex *vertex = m_task->getVertex(m_updatable_pu_info.vtx_idx);
    ExynosVpuSubchainHw *subchain = (ExynosVpuSubchainHw*)vertex->getSubchain(m_updatable_pu_info.sc_idx_in_vtx);
    if (subchain->getType() != VPUL_SUB_CH_HW) {
        VXLOGE("subchain type is not hw");
        subchain->displayObjectInfo();
        return INVALID_OPERATION;
    }

    m_target_pu = subchain->getPu(m_updatable_pu_info.pu_idx_in_sc);

    return NO_ERROR;
}

status_t
ExynosVpuUpdatablePu::insertUpdatablePuToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;
    ret = updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating info fails");
        displayObjectInfo(0);
        goto EXIT;
    }

    ret = checkContraint();
    if (ret != NO_ERROR) {
        VXLOGE("updatable_pu_%d invalid configuration", m_updatable_pu_index_in_task);
        goto EXIT;
    }

    struct vpul_pu_location *td_updatable_pu;
    td_updatable_pu = updateble_pu_location_ptr((struct vpul_task*)task_descriptor_base, m_updatable_pu_index_in_task);

    memcpy(td_updatable_pu, &m_updatable_pu_info, sizeof(struct vpul_pu_location));

EXIT:
    return ret;
}

status_t
ExynosVpuProcPuDma::connectIntermediate(ExynosVpuPu *output_pu, ExynosVpuPu *input_pu)
{
    status_t ret = NO_ERROR;
    enum vpul_pu_instance pu_instance;

    pu_instance = output_pu->getInstance();
    switch (pu_instance) {
    case VPU_PU_DMAOT0:
    case VPU_PU_DMAOT_MNM0:
    case VPU_PU_DMAOT_MNM1:
    case VPU_PU_DMAOT_WIDE0:
    case VPU_PU_DMAOT_WIDE1:
        break;
    default:
        VXLOGE("output pu is not dma out pu, instance:%d", pu_instance);
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    pu_instance = input_pu->getInstance();
    switch (pu_instance) {
    case VPU_PU_DMAIN0:
    case VPU_PU_DMAIN_MNM0:
    case VPU_PU_DMAIN_MNM1:
    case VPU_PU_DMAIN_WIDE0:
    case VPU_PU_DMAIN_WIDE1:
        break;
    default:
        VXLOGE("input pu is not dma in pu, instance:%d", pu_instance);
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    ExynosVpuProcPuDmaOut *dma_out;
    ExynosVpuProcPuDmaIn *dma_in;

    dma_out = (ExynosVpuProcPuDmaOut*)output_pu;
    dma_in = (ExynosVpuProcPuDmaIn*)input_pu;

    dma_out->addIntermediatePostDma(dma_in);

EXIT:
    return ret;
}

ExynosVpuProcPuDma::ExynosVpuProcPuDma(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpuPu(subchain, pu_instance)
{
    m_iotype_desc = NULL;
}

ExynosVpuProcPuDma::ExynosVpuProcPuDma(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpuPu(subchain, pu_info)
{
    m_iotype_desc = NULL;
}

status_t
ExynosVpuProcPuDma::setIoTypesDesc(ExynosVpuIoTypesDesc *iotypes_desc)
{
    if (m_iotype_desc != NULL) {
        VXLOGE("pu_%d(%s) is already assigend memmap_%d", m_pu_index_in_task, m_name, m_iotype_desc->getIndex());
        return INVALID_OPERATION;
    }

    m_iotype_desc = iotypes_desc;

    return NO_ERROR;
}

ExynosVpuMemmap*
ExynosVpuProcPuDma::getMemmap(void)
{
    ExynosVpuMemmap *memmap = NULL;

    if (m_iotype_desc == NULL) {
        VXLOGE("iotype is not assigned");
        return NULL;
    }

    if (m_iotype_desc->getIoTypesInfo()->is_roi_derived) {
        VXLOGE("pts map has the several memmap");
        return NULL;
    }

    ExynosVpuIoMapRoi *map_roi = m_iotype_desc->getMapRoi();
    if (map_roi == NULL) {
        VXLOGE("map_roi is not assigned");
        return NULL;
    }

    memmap = map_roi->getMemmap();
    if (memmap == NULL) {
        VXLOGE("memmap is not allocated");
        return NULL;
    }

    return memmap;
}

status_t
ExynosVpuProcPuDma::getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list)
{
    status_t ret = NO_ERROR;

    if (m_iotype_desc == NULL) {
        VXLOGE("iotype is not assigned");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    ret_memmap_list->clear();

    if (m_iotype_desc->getIoTypesInfo()->is_roi_derived) {
        ExynosVpuProcess *process;
        process = m_iotype_desc->getProcess();
        int32_t pts_map_index;
        pts_map_index = process->calcPtsMapIndex(m_iotype_desc->getIndex());
        if (pts_map_index < 0) {
            VXLOGE("calc invalid index");
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        ret = process->getPtsMapMemmapList(pts_map_index, ret_memmap_list);
        if (ret != NO_ERROR) {
            VXLOGE("getting pts_map memmap fails");
            goto EXIT;
        }
    } else {
        ExynosVpuMemmap *memmap = NULL;
        ExynosVpuIoMapRoi *map_roi = m_iotype_desc->getMapRoi();
        if (map_roi == NULL) {
            VXLOGE("map_roi is not assigned");
            ret = INVALID_OPERATION;
            goto EXIT;
        }

        memmap = map_roi->getMemmap();
        if (memmap == NULL) {
            VXLOGE("memmap is not assigned");
            ret = INVALID_OPERATION;
            goto EXIT;
        }

        ret_memmap_list->push_back(memmap);
    }

EXIT:
    return ret;
}

status_t
ExynosVpuProcPuDma::updateIoInfo(List<struct io_format_t> *io_format_list)
{
    status_t ret = NO_ERROR;

    List<ExynosVpuMemmap*> memmap_list;
    ret = getMemmap(&memmap_list);
    if (ret != NO_ERROR) {
        VXLOGE("getting memmap list fails");
        return ret;
    }

    if (memmap_list.size() != io_format_list->size()) {
        VXLOGE("io size is not matching, %d, %d", memmap_list.size(), io_format_list->size());
        displayObjectInfo();
        ret = INVALID_OPERATION;
        return ret;
    }

    struct vpul_memory_map_desc *memmap_info;
    List<ExynosVpuMemmap*>::iterator memmap_iter;
    List<struct io_format_t>::iterator format_iter;
    for (memmap_iter = memmap_list.begin(), format_iter = io_format_list->begin();
            memmap_iter != memmap_list.end();
            memmap_iter++, format_iter++) {
        memmap_info = (*memmap_iter)->getMemmapInfo();
        memmap_info->image_sizes.width = format_iter->width;
        memmap_info->image_sizes.height = format_iter->height;
        memmap_info->image_sizes.pixel_bytes = format_iter->pixel_byte;
        memmap_info->image_sizes.line_offset = format_iter->width * format_iter->pixel_byte;
    }

    return ret;
}

status_t
ExynosVpuProcPuDma::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuPu::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("common update info structure fails");
        goto EXIT;
    }

    if (m_iotype_desc) {
        m_pu_info.params.dma.inout_index = m_iotype_desc->getIndex();
    } else {
        VXLOGE("iotypes is not assigned");
        displayObjectInfo(0);
        m_pu_info.params.dma.inout_index = 0xFF;
        ret = INVALID_OPERATION;
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuProcPuDma::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuPu::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("common update object fails");
        goto EXIT;
    }

    ExynosVpuProcess *process;
    process = getSubchain()->getProcess();
    if (process == NULL) {
        VXLOGE("getting process fails");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    ExynosVpuIoTypesDesc *iotypes_desc;
    iotypes_desc = process->getIotypesDesc(m_pu_info.params.dma.inout_index);
    if (iotypes_desc == NULL) {
        VXLOGE("getting iotypes_desc fails");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    m_iotype_desc = iotypes_desc;

EXIT:
    return ret;
}

status_t
ExynosVpuProcPuDmaIn::checkContraint(void)
{
    if (m_pu_info.n_in_connect != 0) {
        VXLOGE("pu_%d, the n_in_connect of dma-in pu should be zero", m_pu_index_in_task);
        return INVALID_OPERATION;
    }

    if (m_pu_info.n_out_connect != 1) {
        VXLOGE("pu_%d, the n_out_connect of dma-in pu is not matching, expect:1, real:%d", m_pu_index_in_task, m_pu_info.n_out_connect);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuProcPuDmaIn::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] dma-in, inout_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.inout_index);
    VXLOGI("%s[------] offset_lines_inc:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.offset_lines_inc);
    VXLOGI("%s[------] is_output_vector_dma:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.is_output_vector_dma);
}

void
ExynosVpuProcPuDmaIn::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);

    char tap[MAX_TAB_NUM];

    if (m_iotype_desc)
        VXLOGI("%s[------] dma-in, inout_index:%d", MAKE_TAB(tap, tab_num+1), m_iotype_desc->getIndex());
    else
        VXLOGI("%s[------] dma-in, inout_index:%d", MAKE_TAB(tap, tab_num+1), 0xFF);

    VXLOGI("%s[------] offset_lines_inc:%d", MAKE_TAB(tap, tab_num+1), m_pu_info.params.dma.offset_lines_inc);
    VXLOGI("%s[------] is_output_vector_dma:%d", MAKE_TAB(tap, tab_num+1), m_pu_info.params.dma.is_output_vector_dma);
}

ExynosVpuProcPuDmaIn::ExynosVpuProcPuDmaIn(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpuProcPuDma(subchain, pu_instance)
{

}

ExynosVpuProcPuDmaIn::ExynosVpuProcPuDmaIn(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpuProcPuDma(subchain, pu_info)
{

}

status_t
ExynosVpuProcPuDmaIn::checkAndUpdatePuIoInfo(void)
{
    status_t ret;

    if (m_iotype_desc == NULL) {
        VXLOGE("io type is not assigned");
        return INVALID_OPERATION;
    }

    ExynosVpuMemmap *memmap = m_iotype_desc->getMapRoi()->getMemmap();
    struct vpul_image_size_desc size_desc = memmap->getMemmapInfo()->image_sizes;

    if (memmap->getType() == VPUL_MEM_PRELOAD_PU) {
        /* the DMA of preload pu doesn't care the size information */
        return NO_ERROR;
    }

    if ((size_desc.width==0) || (size_desc.height==0) ||
        (size_desc.pixel_bytes==0) || (size_desc.line_offset==0)) {
        VXLOGE("size desc is not updated, width:%d, height:%d, pixel_bype:%d, line_offset:%d",
            size_desc.width, size_desc.height, size_desc.pixel_bytes, size_desc.line_offset);
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    ExynosVpuIoSize *in_io_size = getInIoSize();
    if (in_io_size == NULL) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    if (in_io_size->isAssignDimension()) {
        uint32_t in_width, in_height, in_roi_width, in_roi_height;
        ret = in_io_size->getDimension(&in_width, &in_height, &in_roi_width, &in_roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("getting dimension information fails");
            return INVALID_OPERATION;
        }

        if ((size_desc.width != in_width) || (size_desc.height != in_height)) {
            VXLOGE("dimension is not matching, (%d,%d) (%d,%d)", size_desc.width, size_desc.height, in_width, in_height);
            displayObjectInfo();
            return INVALID_OPERATION;
        }
    } else {
        uint32_t roi_width = 0;
        uint32_t roi_height = 0;
        if (m_iotype_desc->getIoTypesInfo()->is_roi_derived) {
            struct vpul_roi_desc *pt_roi_desc = m_iotype_desc->getProcess()->getPtRoiDesc(0);
            roi_width = pt_roi_desc->roi_width;
            roi_height = pt_roi_desc->roi_height;
        }

        ret = in_io_size->setOriginDimension(size_desc.width, size_desc.height, roi_width, roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("setting dimension information fails");
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t
ExynosVpuProcPuDmaOut::checkContraint(void)
{
    if (m_pu_info.n_in_connect != 1) {
        VXLOGE("pu_%d, the n_in_connect of dma-out is not matching, expect:1, real:%d", m_pu_index_in_task, m_pu_info.n_in_connect);
        return INVALID_OPERATION;
    }

    if (m_pu_info.n_out_connect != 0) {
        VXLOGE("pu_%d, the n_out_connect of dma-out pu should be zero", m_pu_index_in_task);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuProcPuDmaOut::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] dma-out, inout_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.inout_index);
    VXLOGI("%s[------] offset_lines_inc:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.offset_lines_inc);
    VXLOGI("%s[------] is_output_vector_dma:%d", MAKE_TAB(tap, tab_num+1), pu->params.dma.is_output_vector_dma);
}

void
ExynosVpuProcPuDmaOut::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);

    char tap[MAX_TAB_NUM];

    if (m_iotype_desc)
        VXLOGI("%s[------] dma-out, inout_index:%d", MAKE_TAB(tap, tab_num+1), m_iotype_desc->getIndex());
    else
        VXLOGI("%s[------] dma-out, inout_index:%d", MAKE_TAB(tap, tab_num+1), 0xFF);

    VXLOGI("%s[------] offset_lines_inc:%d", MAKE_TAB(tap, tab_num+1), m_pu_info.params.dma.offset_lines_inc);
    VXLOGI("%s[------] is_output_vector_dma:%d", MAKE_TAB(tap, tab_num+1), m_pu_info.params.dma.is_output_vector_dma);
}

ExynosVpuProcPuDmaOut::ExynosVpuProcPuDmaOut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpuProcPuDma(subchain, pu_instance)
{

}

ExynosVpuProcPuDmaOut::ExynosVpuProcPuDmaOut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpuProcPuDma(subchain, pu_info)
{

}

void
ExynosVpuProcPuDmaOut::addIntermediatePostDma(ExynosVpuProcPuDmaIn *dma_in)
{
    m_inter_post_dma_list.push_back(dma_in);

    ExynosVpuIoMemory *io_memory_pre = m_iotype_desc->getMapRoi()->getMemmap()->getMemory();
    ExynosVpuIoMemory *io_memory_post = dma_in->getIoTypesDesc()->getMapRoi()->getMemmap()->getMemory();
    io_memory_pre->addAlliancePostIoMemory(io_memory_post);
}

status_t
ExynosVpuProcPuDmaOut::checkAndUpdatePuIoInfo(void)
{
    status_t ret;

    ExynosVpuMemmap* memmap;
    memmap = m_iotype_desc->getMapRoi()->getMemmap();

    if (memmap->getType() == VPUL_MEM_PRELOAD_PU) {
        return NO_ERROR;
    }

    ExynosVpuIoSize *out_io_size = getOutIoSize();
    if (out_io_size == NULL) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    uint32_t out_width, out_height, out_roi_width, out_roi_height;
    bool is_size_info = out_io_size->isAssignDimension();
    if (is_size_info) {
        ret = out_io_size->getDimension(&out_width, &out_height, &out_roi_width, &out_roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("getting dimension information fails");
            return INVALID_OPERATION;
        }
    } else {
        out_width = 0;
        out_height = 0;
    }

    struct vpul_image_size_desc *memmap_size;
    memmap_size = &(memmap->getMemmapInfo()->image_sizes);
    if (memmap->getMemory()->isIoBuffer()) {
        if ((memmap_size->width==0) || (memmap_size->height==0) || (memmap_size->pixel_bytes==0) || (memmap_size->line_offset==0)) {
            VXLOGE("the memmap_desc of io buffer should be set, %d, %d, %d, %d", memmap_size->width, memmap_size->height,
                                                                        memmap_size->pixel_bytes, memmap_size->line_offset);
            displayObjectInfo();
            return INVALID_OPERATION;
        }

        pair<ExynosVpuPu*, uint32_t> *in_port_slot = &m_in_port_slots.editItemAt(0);
        if ((in_port_slot->first->getOpcode() == VPUL_OP_MAP_2_LST) && (in_port_slot->second == 1)) {
            /* for DMA pu for array */
            /* do nothing */
        } else {
            if (is_size_info) {
                /* check size validation only if the struct vpul_sizes is exist */
                if ((memmap_size->width != out_width) || (memmap_size->height != out_height)) {
                    VXLOGE("can't matching io size");
                    VXLOGE("size information:%d %d", out_width, out_height);
                    VXLOGE("own size:%d %d %d %d", memmap_size->width, memmap_size->height, memmap_size->pixel_bytes, memmap_size->line_offset);
                    displayObjectInfo();
                    return INVALID_OPERATION;
                }
            }
        }
    } else {
        /* intermediate buffer */
        if (memmap_size->pixel_bytes == 0) {
            VXLOGE("pixel_bytes is zero");
            displayObjectInfo();
            return INVALID_OPERATION;
        }
        memmap_size->width = out_roi_width ? out_roi_width : out_width;
        memmap_size->height = out_roi_height ? out_roi_height : out_height;
        memmap_size->line_offset = memmap_size->width * memmap_size->pixel_bytes;
    }

    /* for intermediate dma buffer */
    List<ExynosVpuProcPuDmaIn*>::iterator dma_iter;
    ExynosVpuMemmap* dma_in_memmap;
    struct vpul_image_size_desc *dma_in_size_desc;
    for (dma_iter=m_inter_post_dma_list.begin(); dma_iter!=m_inter_post_dma_list.end(); dma_iter++) {
        dma_in_memmap = (*dma_iter)->getMemmap();
        if (dma_in_memmap == NULL) {
            VXLOGE("getting memmap fails");
            return INVALID_OPERATION;
        }
        if (dma_in_memmap->getMemory()->isIoBuffer()) {
            VXLOGE("io buffer can't assigned intermediate buffer");
            VXLOGE("[dma-out]");
            displayObjectInfo();
            VXLOGE("[dma-in]");
            dma_in_memmap->displayObjectInfo();
            return INVALID_OPERATION;
        }
        dma_in_size_desc = &(dma_in_memmap->getMemmapInfo()->image_sizes);

        /* intermediate buffer */
        if (dma_in_size_desc->pixel_bytes == 0) {
            VXLOGE("[memmap_%d] pixel_bytes is zero", dma_in_memmap->getIndex());
            displayObjectInfo();
            return INVALID_OPERATION;
        }
        dma_in_size_desc->width = memmap_size->width;
        dma_in_size_desc->height = memmap_size->height;
        dma_in_size_desc->line_offset = dma_in_size_desc->width * dma_in_size_desc->pixel_bytes;
    }

    return NO_ERROR;
}

ExynosVpu3dnnPuDma::ExynosVpu3dnnPuDma(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpuPu(subchain, pu_instance)
{

}

ExynosVpu3dnnPuDma::ExynosVpu3dnnPuDma(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpuPu(subchain, pu_info)
{

}

status_t
ExynosVpu3dnnPuDma::getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list)
{
    status_t ret = NO_ERROR;

    ret_memmap_list->clear();

    ExynosVpu3dnnProcess *tdnn_proc = getSubchain()->getTdnnProcess();
    if (tdnn_proc == NULL) {
        VXLOGE("vertex is null");
        return INVALID_OPERATION;
    }

    ret = tdnn_proc->getTdnnProcBase()->getLayerInoutMemmapList(m_pu_info.params.dma.inout_index, ret_memmap_list);

EXIT:
    return ret;
}

status_t
ExynosVpu3dnnPuDma::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuPu::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("common update object fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpu3dnnPuDma::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuPu::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("common update object fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpu3dnnPuDma::updateIoInfo(List<struct io_format_t> *io_format_list)
{
    List<struct io_format_t>::iterator format_iter;
    for (format_iter=io_format_list->begin(); format_iter!=io_format_list->end(); format_iter++) {
        if ((format_iter->width==0) || (format_iter->height==0) || (format_iter->pixel_byte==0)) {
            VXLOGE("io info is not valid, width:%d, height:%d, pixel_byte:%d", format_iter->width, format_iter->height, format_iter->pixel_byte);
            return INVALID_OPERATION;
        }
    }

    /* JUN_TBD, update memmap_desc */

    return NO_ERROR;
}

ExynosVpu3dnnPuDmaIn::ExynosVpu3dnnPuDmaIn(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpu3dnnPuDma(subchain, pu_instance)
{

}

ExynosVpu3dnnPuDmaIn::ExynosVpu3dnnPuDmaIn(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpu3dnnPuDma(subchain, pu_info)
{

}

status_t
ExynosVpu3dnnPuDmaIn::checkAndUpdatePuIoInfo(void)
{
    return NO_ERROR;
}

ExynosVpu3dnnPuDmaOut::ExynosVpu3dnnPuDmaOut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpu3dnnPuDma(subchain, pu_instance)
{

}

ExynosVpu3dnnPuDmaOut::ExynosVpu3dnnPuDmaOut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpu3dnnPuDma(subchain, pu_info)
{

}

status_t
ExynosVpu3dnnPuDmaOut::checkAndUpdatePuIoInfo(void)
{
    return NO_ERROR;
}

void
ExynosVpuPuSalb::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [salb]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] bits_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.bits_in0);
    VXLOGI("%s[------] bits_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.bits_in1);
    VXLOGI("%s[------] bits_out0:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.bits_out0);
    VXLOGI("%s[------] signed_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.signed_in0);
    VXLOGI("%s[------] signed_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.signed_in1);
    VXLOGI("%s[------] signed_out0:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.signed_out0);
    VXLOGI("%s[------] input_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.input_enable);
    VXLOGI("%s[------] use_mask:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.use_mask);
    VXLOGI("%s[------] operation_code:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.operation_code);
    VXLOGI("%s[------] abs_neg:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.abs_neg);
    VXLOGI("%s[------] trunc_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.trunc_out);
    VXLOGI("%s[------] org_val_med:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.org_val_med);
    VXLOGI("%s[------] shift_bits:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.shift_bits);
    VXLOGI("%s[------] cmp_op:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.cmp_op);
    VXLOGI("%s[------] const_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.const_in1);
    VXLOGI("%s[------] thresh_lo:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.thresh_lo);
    VXLOGI("%s[------] thresh_hi:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.thresh_hi);
    VXLOGI("%s[------] val_lo:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.val_lo);
    VXLOGI("%s[------] val_hi:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.val_hi);
    VXLOGI("%s[------] val_med_filler:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.val_med_filler);
    VXLOGI("%s[------] salbregs_custom_trunc_en:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.salbregs_custom_trunc_en);
    VXLOGI("%s[------] salbregs_custom_trunc_bittage:%d", MAKE_TAB(tap, tab_num+1), pu->params.salb.salbregs_custom_trunc_bittage);
}

void
ExynosVpuPuSalb::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuSalb::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuCalb::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [calb]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] bits_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.bits_in0);
    VXLOGI("%s[------] bits_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.bits_in1);
    VXLOGI("%s[------] bits_out0:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.bits_out0);
    VXLOGI("%s[------] signed_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.signed_in0);
    VXLOGI("%s[------] signed_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.signed_in1);
    VXLOGI("%s[------] signed_out0:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.signed_out0);
    VXLOGI("%s[------] input_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.input_enable);
    VXLOGI("%s[------] use_mask:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.use_mask);
    VXLOGI("%s[------] operation_code:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.operation_code);
    VXLOGI("%s[------] abs_neg:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.abs_neg);
    VXLOGI("%s[------] trunc_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.trunc_out);
    VXLOGI("%s[------] org_val_med:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.org_val_med);
    VXLOGI("%s[------] shift_bits:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.shift_bits);
    VXLOGI("%s[------] mult_round:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.mult_round);
    VXLOGI("%s[------] cmp_op:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.cmp_op);
    VXLOGI("%s[------] const_in1:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.const_in1);
    VXLOGI("%s[------] div_shift_bits:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.div_shift_bits);
    VXLOGI("%s[------] div_overflow_remainder:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.div_overflow_remainder);
    VXLOGI("%s[------] thresh_lo:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.thresh_lo);
    VXLOGI("%s[------] thresh_hi:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.thresh_hi);
    VXLOGI("%s[------] val_lo:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.val_lo);
    VXLOGI("%s[------] val_hi:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.val_hi);
    VXLOGI("%s[------] val_med_filler:%d", MAKE_TAB(tap, tab_num+1), pu->params.calb.val_med_filler);
}

void
ExynosVpuPuCalb::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuCalb::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuRois::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [rois]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] bits_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.bits_in0);
    VXLOGI("%s[------] signed_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.signed_in0);
    VXLOGI("%s[------] use_mask:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.use_mask);
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.work_mode);
    VXLOGI("%s[------] first_min_max:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.first_min_max);
    VXLOGI("%s[------] thresh_lo_temp:%d", MAKE_TAB(tap, tab_num+1), pu->params.rois_out.thresh_lo_temp);
}

void
ExynosVpuPuRois::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuRois::display_structure_info(tab_num, &m_pu_info);
}

status_t
ExynosVpuPuRois::updateIoInfo(List<struct io_format_t> *io_format_list)
{
    if (io_format_list->size() != 1) {
        VXLOGE("the rois should have only one io");
        return INVALID_OPERATION;
    }

    List<struct io_format_t>::iterator format_iter = io_format_list->begin();

    /* 6 output reg */
    if ((format_iter->width == 6) && (format_iter->height == 1) && (format_iter->pixel_byte == 4)) {
        return NO_ERROR;
    } else {
        VXLOGE("the io of rois is invalid, %d, %d, %d", format_iter->width, format_iter->height, format_iter->pixel_byte);
        return INVALID_OPERATION;
    }
}

void
ExynosVpuPuCrop::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [crop]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.work_mode);
    VXLOGI("%s[------] pad_value:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.pad_value);
    VXLOGI("%s[------] mask_val_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.mask_val_in);
    VXLOGI("%s[------] mask_val_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.mask_val_out);
    VXLOGI("%s[------] roi_startx:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.roi_startx);
    VXLOGI("%s[------] roi_starty:%d", MAKE_TAB(tap, tab_num+1), pu->params.crop.roi_starty);
}

void
ExynosVpuPuCrop::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuCrop::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuMde::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [mde]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.work_mode);
    VXLOGI("%s[------] result_shift:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.result_shift);
    VXLOGI("%s[------] use_thresh:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.use_thresh);
    VXLOGI("%s[------] calc_quantized_angle:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.calc_quantized_angle);
    VXLOGI("%s[------] eig_coeff:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.eig_coeff);
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.bits_in);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.signed_in);
    VXLOGI("%s[------] bits_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.bits_out);
    VXLOGI("%s[------] signed_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.signed_out);
    VXLOGI("%s[------] output_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.output_enable);
    VXLOGI("%s[------] thresh:%d", MAKE_TAB(tap, tab_num+1), pu->params.mde.thresh);
}

void
ExynosVpuPuMde::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuMde::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuMapToList::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [map2list]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.bits_in);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.signed_in);
    VXLOGI("%s[------] value_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.value_in);
    VXLOGI("%s[------] enable_out_map:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.enable_out_map);
    VXLOGI("%s[------] enable_out_lst:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.enable_out_lst);
    VXLOGI("%s[------] inout_indx:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.inout_indx);
    VXLOGI("%s[------] threshold_low:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.threshold_low);
    VXLOGI("%s[------] threshold_high:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.threshold_high);
    VXLOGI("%s[------] num_of_point:%d", MAKE_TAB(tap, tab_num+1), pu->params.map2list.num_of_point);
}

void
ExynosVpuPuMapToList::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuMapToList::display_structure_info(tab_num, &m_pu_info);
}

status_t
ExynosVpuPuMapToList::checkAndUpdatePuIoInfo(void)
{
    /* JUN_TBD, inout_indx of m2l will be removed */
    ExynosVpuPu *list_out_pu = m_out_port_slots[1].first;
    if (list_out_pu != NULL) {
        ExynosVpuIoTypesDesc *iotype = list_out_pu->getIoTypesDesc();
        if (m_pu_info.params.map2list.inout_indx == 0) {
            m_pu_info.params.map2list.inout_indx = iotype->getIndex();
        } else {
            if (m_pu_info.params.map2list.inout_indx != iotype->getIndex()) {
                VXLOGE("inout_indx of m2l is not matching with DMA pu");
                displayObjectInfo();
                return INVALID_OPERATION;
            }
        }
    }

    return NO_ERROR;
}

status_t
ExynosVpuPuMapToList::checkContraint(void)
{
    if (m_pu_info.params.map2list.inout_indx == 0) {
        VXLOGE("inout_indx is not set");
        return INVALID_OPERATION;
    }

    if ((m_out_port_slots[1].first) && (m_out_port_slots[1].first->getInstance() != VPU_PU_DMAOT_WIDE1)) {
        VXLOGE("the second output of map2list should be connected with DMAOT_WIDE1");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuPuNms::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [nms]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.work_mode);
    VXLOGI("%s[------] keep_equals:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.keep_equals);
    VXLOGI("%s[------] directional_nms:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.directional_nms);
    VXLOGI("%s[------] census_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.census_mode);
    VXLOGI("%s[------] add_orig_pixel:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.add_orig_pixel);
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.bits_in);
    VXLOGI("%s[------] bits_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.bits_out);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.signed_in);
    VXLOGI("%s[------] support:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.support);
    VXLOGI("%s[------] org_val_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.org_val_out);
    VXLOGI("%s[------] trunc_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.trunc_out);
    VXLOGI("%s[------] image_height:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.image_height);
    VXLOGI("%s[------] thresh:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.thresh);
    VXLOGI("%s[------] border_mode:%d/%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.border_mode_up,
                                                                                                                                pu->params.nms.border_mode_down,
                                                                                                                                pu->params.nms.border_mode_left,
                                                                                                                                pu->params.nms.border_mode_right);
    VXLOGI("%s[------] border_fill:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.border_fill);
    VXLOGI("%s[------] border_fill_constant:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.border_fill_constant);
    VXLOGI("%s[------] strict_comparison_mask:%d", MAKE_TAB(tap, tab_num+1), pu->params.nms.strict_comparison_mask);
    VXLOGI("%s[------] cens_thres:%d %d %d %d %d %d %d %d", MAKE_TAB(tap, tab_num+1), pu->params.nms.cens_thres_0,
                                                                                                                                                pu->params.nms.cens_thres_1,
                                                                                                                                                pu->params.nms.cens_thres_2,
                                                                                                                                                pu->params.nms.cens_thres_3,
                                                                                                                                                pu->params.nms.cens_thres_4,
                                                                                                                                                pu->params.nms.cens_thres_5,
                                                                                                                                                pu->params.nms.cens_thres_6,
                                                                                                                                                pu->params.nms.cens_thres_7);
}

void
ExynosVpuPuNms::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuNms::display_structure_info(tab_num, &m_pu_info);
}

status_t
ExynosVpuPuNms::checkAndUpdatePuIoInfo(void)
{
    status_t ret = NO_ERROR;

    ExynosVpuIoSize *in_io_size = getInIoSize();
    if (in_io_size == NULL) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    uint32_t in_width, in_height, in_roi_width, in_roi_height;
    ret = in_io_size->getDimension(&in_width, &in_height, &in_roi_width, &in_roi_height);
    if (ret != NO_ERROR) {
        VXLOGE("getting diemnsion infroation");
        return INVALID_OPERATION;
    }

    m_pu_info.params.nms.image_height = in_height;

    return NO_ERROR;
}

status_t
ExynosVpuPuNms::checkContraint(void)
{
    if (m_pu_info.n_in_connect != 1) {
        VXLOGE("%s(pu_%d), the n_in_connect is not matching, expect:1, real:%d", m_name, m_pu_index_in_task, m_pu_info.n_in_connect);
        displayObjectInfo(0);
        return INVALID_OPERATION;
    }

    if (m_pu_info.n_out_connect != 1) {
        VXLOGE("%s(pu_%d), the n_out_connect is not matching, expect:1, real:%d", m_name, m_pu_index_in_task, m_pu_info.n_out_connect);
        displayObjectInfo(0);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuPuSlf::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [slf]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] invert_columns:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.invert_columns);
    VXLOGI("%s[------] upsample_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.upsample_mode);
    VXLOGI("%s[------] downsample_rows:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.downsample_rows);
    VXLOGI("%s[------] downsample_cols:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.downsample_cols);
    VXLOGI("%s[------] sampling_offset_x:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sampling_offset_x);
    VXLOGI("%s[------] sampling_offset_y:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sampling_offset_y);
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.work_mode);
    VXLOGI("%s[------] filter_size_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.filter_size_mode);
    VXLOGI("%s[------] out_enable_1:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.out_enable_1);
    VXLOGI("%s[------] horizontal_only:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.horizontal_only);
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.bits_in);
    VXLOGI("%s[------] bits_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.bits_out);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.signed_in);
    VXLOGI("%s[------] signed_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.signed_out);
    VXLOGI("%s[------] do_rounding:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.do_rounding);
    VXLOGI("%s[------] border_mode_up:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_mode_up);
    VXLOGI("%s[------] border_mode_down:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_mode_down);
    VXLOGI("%s[------] border_mode_left:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_mode_left);
    VXLOGI("%s[------] border_mode_right:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_mode_right);
    VXLOGI("%s[------] border_fill:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_fill);
    VXLOGI("%s[------] border_fill_constant:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.border_fill_constant);
    VXLOGI("%s[------] coefficient_fraction:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.coefficient_fraction);
    VXLOGI("%s[------] sepfregs_is_max_pooling_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_is_max_pooling_mode);
    VXLOGI("%s[------] sepfregs_stride_value:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_stride_value);
    VXLOGI("%s[------] sepfregs_stride_offset_height:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_stride_offset_height);
    VXLOGI("%s[------] sepfregs_stride_offset_width:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_stride_offset_width);
    VXLOGI("%s[------] sepfregs_subimage_height:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_subimage_height);
    VXLOGI("%s[------] sepfregs_convert_16f_to_32f:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_convert_16f_to_32f);
    VXLOGI("%s[------] sepfregs_convert_output_sm_to_2scomp:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_convert_output_sm_to_2scomp);
    VXLOGI("%s[------] sepfregs_convert_input_2scomp_to_sm:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.sepfregs_convert_input_2scomp_to_sm);
    VXLOGI("%s[------] max_pooling_size_spoof_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.max_pooling_size_spoof_index);
    VXLOGI("%s[------] maxp_num_slices:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.maxp_num_slices);
    VXLOGI("%s[------] maxp_sizes_filt_hor:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.maxp_sizes_filt_hor);
    VXLOGI("%s[------] maxp_sizes_filt_ver:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.maxp_sizes_filt_ver);
    VXLOGI("%s[------] horizontal_filter_coeff:%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.horizontal_filter_coeff.is_dynamic,
                                                                                                                                    pu->params.slf.horizontal_filter_coeff.vsize,
                                                                                                                                    pu->params.slf.horizontal_filter_coeff.offset);
    VXLOGI("%s[------] vertical_filter_coeff:%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.vertical_filter_coeff.is_dynamic,
                                                                                                                                    pu->params.slf.vertical_filter_coeff.vsize,
                                                                                                                                    pu->params.slf.vertical_filter_coeff.offset);
    VXLOGI("%s[------] coefficient_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.slf.coefficient_index);
}

void
ExynosVpuPuSlf::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuSlf::display_structure_info(tab_num, &m_pu_info);
}

status_t
ExynosVpuPuSlf::setStaticCoffValue(int32_t coff[], uint32_t vsize, uint32_t coff_num)
{
    status_t ret = NO_ERROR;

    ExynosVpuProcess *process;
    process = getSubchain()->getProcess();

    if ((coff_num%2) != 0) {
        VXLOGE("coff_num should be even, coff_num:%d", coff_num);
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    uint32_t coff_index;
    ret = process->addStaticCoffValue(coff, coff_num, &coff_index);
    if (ret != NO_ERROR) {
        VXLOGE("adding static coff to process fails");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    switch (vsize) {
    case 1:
        m_pu_info.params.slf.filter_size_mode = 0;
        if (coff_num != 2) {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 3:
        m_pu_info.params.slf.filter_size_mode = 1;
        if (coff_num != 6) {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 5:
        m_pu_info.params.slf.filter_size_mode = 2;
        if (coff_num != 10) {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 7:
        m_pu_info.params.slf.filter_size_mode = 3;
        if (coff_num != 14) {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    default:
        VXLOGE("un-support filter size:%d", vsize);
        ret = INVALID_OPERATION;
        goto EXIT;
        break;
    }

    m_pu_info.params.slf.vertical_filter_coeff.vsize = vsize;
    m_pu_info.params.slf.vertical_filter_coeff.offset = coff_index;

    m_pu_info.params.slf.horizontal_filter_coeff.vsize = vsize;
    m_pu_info.params.slf.horizontal_filter_coeff.offset = coff_index + m_pu_info.params.slf.vertical_filter_coeff.vsize;

EXIT:
    return ret;
}

status_t
ExynosVpuPuSlf::checkAndUpdatePuIoInfo(void)
{
    status_t ret = NO_ERROR;

    /* skip the io check if the 3dnn proc */
    if (getSubchain()->getVertex()->getType() == VPUL_VERTEXT_3DNN_PROC)
        return NO_ERROR;

    uint32_t in_width, in_height, in_roi_widht, in_roi_height;
    uint32_t out_width, out_height, out_roi_width, out_roi_height;

    ExynosVpuIoSize *in_io_size = getInIoSize();
    ExynosVpuIoSize *out_io_size = getOutIoSize();
    if ((in_io_size == NULL) || (out_io_size == NULL)) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    ret |= in_io_size->getDimension(&in_width, &in_height, &in_roi_widht, &in_roi_height);
    ret |= out_io_size->getDimension(&out_width, &out_height, &out_roi_width, &out_roi_height);
    if (ret != NO_ERROR) {
        VXLOGE("getting dimension infroation");
        return INVALID_OPERATION;
    }

    if (m_pu_info.params.slf.downsample_cols== 1) {
        if (in_width != (out_width*2)) {
            VXLOGE("scaling factor is not valid, in_width:%d, out_width:%d", in_width, out_width);
            return INVALID_OPERATION;
        }
    }
    if (m_pu_info.params.slf.downsample_rows == 1) {
        if (in_height != (out_height*2)) {
            VXLOGE("scaling factor is not valid, in_height:%d, out_height:%d", in_height, out_height);
            return INVALID_OPERATION;
        }
    }

    if (m_pu_info.params.slf.upsample_mode== 1) {
        if (((in_width*2) != out_width) ||
            ((in_height*2) != out_height)) {
            VXLOGE("scaling factor is not valid, (%d, %d) (%d, %d)", in_width, in_height, out_width, out_height);
            displayObjectInfo();
            return INVALID_OPERATION;
        }
    } else if (m_pu_info.params.slf.upsample_mode== 2) {
        if ((in_height*2) != out_height) {
            VXLOGE("scaling factor is not valid, in_height:%d, out_height:%d", in_height, out_height);
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

void
ExynosVpuPuGlf::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [glf]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] filter_size_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.filter_size_mode);
    VXLOGI("%s[------] sad_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.sad_mode);
    VXLOGI("%s[------] out_enable_2:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.out_enable_2);
    VXLOGI("%s[------] two_outputs:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.two_outputs);
    VXLOGI("%s[------] input_enable1:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.input_enable1);
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.bits_in);
    VXLOGI("%s[------] bits_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.bits_out);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.signed_in);
    VXLOGI("%s[------] signed_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.signed_out);
    VXLOGI("%s[------] do_rounding:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.do_rounding);
    VXLOGI("%s[------] RAM_type:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.RAM_type);
    VXLOGI("%s[------] RAM_offset:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.RAM_offset);
    VXLOGI("%s[------] image_height:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.image_height);
    VXLOGI("%s[------] border_mode_up:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_mode_up);
    VXLOGI("%s[------] border_mode_down:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_mode_down);
    VXLOGI("%s[------] border_mode_left:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_mode_left);
    VXLOGI("%s[------] border_mode_right:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_mode_right);
    VXLOGI("%s[------] border_fill:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_fill);
    VXLOGI("%s[------] border_fill_constant:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.border_fill_constant);
    VXLOGI("%s[------] coefficient_fraction:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.coefficient_fraction);
    VXLOGI("%s[------] signed_coefficients:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.signed_coefficients);
    VXLOGI("%s[------] coefficient_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.coefficient_index);
    VXLOGI("%s[------] coeffs_from_dma:%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.coeffs_from_dma);
    VXLOGI("%s[------] filter_coeff:%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.glf.filter_coeff.is_dynamic,
                                                                                                                        pu->params.glf.filter_coeff.vsize,
                                                                                                                        pu->params.glf.filter_coeff.offset);
}

void
ExynosVpuPuGlf::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuGlf::display_structure_info(tab_num, &m_pu_info);
};

status_t
ExynosVpuPuGlf::setStaticCoffValue(int32_t coff[], uint32_t vsize, uint32_t coff_num)
{
    status_t ret = NO_ERROR;

    ExynosVpuProcess *process;
    process = getSubchain()->getProcess();

    uint32_t coff_index = 0;
    ret = process->addStaticCoffValue(coff, coff_num, &coff_index);
    if (ret != NO_ERROR) {
        VXLOGE("adding static coff to process fails");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    switch (vsize) {
    case 1:
        m_pu_info.params.glf.filter_size_mode = 0;
        if (coff_num == 1) {
            m_pu_info.params.glf.two_outputs = 0;
        } else if (coff_num == 2) {
            m_pu_info.params.glf.two_outputs = 1;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 3:
        m_pu_info.params.glf.filter_size_mode = 1;
        if (coff_num == 9) {
            m_pu_info.params.glf.two_outputs = 0;
        } else if (coff_num == 18) {
            m_pu_info.params.glf.two_outputs = 1;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 5:
        m_pu_info.params.glf.filter_size_mode = 2;
        if (coff_num == 25) {
            m_pu_info.params.glf.two_outputs = 0;
        } else if (coff_num == 50) {
            m_pu_info.params.glf.two_outputs = 1;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 7:
        m_pu_info.params.glf.filter_size_mode = 3;
        if (coff_num == 49) {
            m_pu_info.params.glf.two_outputs = 0;
        } else if (coff_num == 98) {
            m_pu_info.params.glf.two_outputs = 1;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 9:
        m_pu_info.params.glf.filter_size_mode = 4;
        if (coff_num == 81) {
            m_pu_info.params.glf.two_outputs = 0;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    case 11:
        m_pu_info.params.glf.filter_size_mode = 5;
        if (coff_num == 121) {
            m_pu_info.params.glf.two_outputs = 0;
        } else {
            VXLOGE("vsize and coff_num is not matching, %d, %d", vsize, coff_num);
            ret = INVALID_OPERATION;
            goto EXIT;
        }
        break;
    default:
        VXLOGE("un-support filter size:%d", vsize);
        ret = INVALID_OPERATION;
        goto EXIT;
        break;
    }

    m_pu_info.params.glf.filter_coeff.vsize = vsize;
    m_pu_info.params.glf.filter_coeff.offset = coff_index;

EXIT:
    return ret;
}

status_t
ExynosVpuPuGlf::checkAndUpdatePuIoInfo(void)
{
    status_t ret = NO_ERROR;

    ExynosVpuIoSize *in_io_size = getInIoSize();
    if (in_io_size == NULL) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    uint32_t in_width, in_height, in_roi_widht, in_roi_height;
    ret = in_io_size->getDimension(&in_width, &in_height, &in_roi_widht, &in_roi_height);
    if (ret != NO_ERROR) {
        VXLOGE("getting diemnsion infroation");
        return INVALID_OPERATION;
    }

    m_pu_info.params.glf.image_height = in_height;

    return NO_ERROR;
}

void
ExynosVpuPuCcm::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [ccm]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.signed_in);
    VXLOGI("%s[------] signed_out:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.signed_out);
    VXLOGI("%s[------] output_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.output_enable);
    VXLOGI("%s[------] input_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.input_enable);
    VXLOGI("%s[------] coefficient_shift:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_shift);
    VXLOGI("%s[------] coefficient_0:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_0);
    VXLOGI("%s[------] coefficient_1:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_1);
    VXLOGI("%s[------] coefficient_2:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_2);
    VXLOGI("%s[------] coefficient_3:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_3);
    VXLOGI("%s[------] coefficient_4:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_4);
    VXLOGI("%s[------] coefficient_5:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_5);
    VXLOGI("%s[------] coefficient_6:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_6);
    VXLOGI("%s[------] coefficient_7:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_7);
    VXLOGI("%s[------] coefficient_8:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.coefficient_8);
    VXLOGI("%s[------] offset_0:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.offset_0);
    VXLOGI("%s[------] offset_1:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.offset_1);
    VXLOGI("%s[------] offset_2:%d", MAKE_TAB(tap, tab_num+1), pu->params.ccm.offset_2);
}

void
ExynosVpuPuCcm::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuCcm::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuLut::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [lut]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] interpolation_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.interpolation_mode);
    VXLOGI("%s[------] lut_size:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.lut_size);
    VXLOGI("%s[------] signed_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.signed_in0);
    VXLOGI("%s[------] signed_out0:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.signed_out0);
    VXLOGI("%s[------] offset:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.offset);
    VXLOGI("%s[------] binsize:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.binsize);
    VXLOGI("%s[------] inverse_binsize:%d", MAKE_TAB(tap, tab_num+1), pu->params.lut.inverse_binsize);
}

void
ExynosVpuPuLut::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuLut::display_structure_info(tab_num, &m_pu_info);
}

ExynosVpuPuLut::ExynosVpuPuLut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                        : ExynosVpuPu(subchain, pu_instance)
{
    m_memmap = NULL;
}
ExynosVpuPuLut::ExynosVpuPuLut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                        : ExynosVpuPu(subchain, pu_info)
{
    m_memmap = NULL;
}

status_t
ExynosVpuPuLut::setMemmap(ExynosVpuMemmap *memmap)
{
    if (m_memmap != NULL) {
        VXLOGE("memmap is already assigned");
        return INVALID_OPERATION;
    }

    m_memmap = memmap;

    return NO_ERROR;
}
ExynosVpuMemmap*
ExynosVpuPuLut::getMemmap(void)
{
    return m_memmap;
}

void
ExynosVpuPuIntegral::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [integral]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] integral_image_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.integral_image_mode);
    VXLOGI("%s[------] overflow_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.overflow_mode);
    VXLOGI("%s[------] dt_right_shift:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.dt_right_shift);
    VXLOGI("%s[------] dt_left_shift:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.dt_left_shift);
    VXLOGI("%s[------] dt_coefficient:%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.dt_coefficient0,
                                                                                                                pu->params.integral.dt_coefficient1,
                                                                                                                pu->params.integral.dt_coefficient2);
    VXLOGI("%s[------] cc_min_label:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.cc_min_label);
    VXLOGI("%s[------] cc_scan_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.cc_scan_mode);
    VXLOGI("%s[------] cc_smart_label_search_en:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.cc_smart_label_search_en);
    VXLOGI("%s[------] cc_reset_labels_array:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.cc_reset_labels_array);
    VXLOGI("%s[------] cc_label_vector_size:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.cc_label_vector_size);
    VXLOGI("%s[------] lut_init_en:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.lut_init_en);
    VXLOGI("%s[------] lut_number_of_values:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.lut_number_of_values);
    VXLOGI("%s[------] lut_value_shift:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.lut_value_shift);
    VXLOGI("%s[------] lut_default_overflow:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.lut_default_overflow);
    VXLOGI("%s[------] lut_default_underflow:%d", MAKE_TAB(tap, tab_num+1), pu->params.integral.lut_default_underflow);
}

void
ExynosVpuPuIntegral::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuIntegral::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuUpscaler::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [upscaler]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] interpolation_method:%d", MAKE_TAB(tap, tab_num+1), pu->params.upscaler.interpolation_method);
    VXLOGI("%s[------] border_fill_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.upscaler.border_fill_mode);
    VXLOGI("%s[------] do_rounding:%d", MAKE_TAB(tap, tab_num+1), pu->params.upscaler.do_rounding);
    VXLOGI("%s[------] border_fill_constant:%d", MAKE_TAB(tap, tab_num+1), pu->params.upscaler.border_fill_constant);
}

void
ExynosVpuPuUpscaler::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuUpscaler::display_structure_info(tab_num, &m_pu_info);
};

status_t
ExynosVpuPuUpscaler::checkAndUpdatePuIoInfo(void)
{
    status_t ret = NO_ERROR;

    ExynosVpuIoSize *in_io_size = getInIoSize();
    ExynosVpuIoSize *out_io_size = getOutIoSize();
    if ((in_io_size == NULL) || (out_io_size == NULL)) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    uint32_t in_width, in_height, in_roi_widht, in_roi_height;
    ret = in_io_size->getDimension(&in_width, &in_height, &in_roi_widht, &in_roi_height);
    if (ret != NO_ERROR) {
        VXLOGE("getting diemnsion infroation");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    if (out_io_size->getType() != VPUL_SIZEOP_SCALE) {
        VXLOGE("the scaler's out size should be VPUL_SIZEOP_SCALE");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    ExynosVpuIoSizeOpScale *size_op_scale = ((ExynosVpuIoSizeScale*)out_io_size)->getSizeOpScale();
    struct vpul_scales *scales_info = size_op_scale->getScaleInfo();

    uint32_t out_width, out_height;
    out_width = ceilf((float)in_width * ((float)scales_info->horizontal.numerator/(float)scales_info->horizontal.denominator));
    out_height = ceilf((float)in_height * ((float)scales_info->vertical.numerator/(float)scales_info->vertical.denominator));

    /* replace ratio to target in/out size */
    scales_info->horizontal.numerator = out_width;
    scales_info->horizontal.denominator = in_width;
    scales_info->vertical.numerator = out_height;
    scales_info->vertical.denominator = in_height;

    return NO_ERROR;
}

void
ExynosVpuPuDownscaler::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [downscaler]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] interpolation_method:%d", MAKE_TAB(tap, tab_num+1), pu->params.downScaler.interpolation_method);
    VXLOGI("%s[------] do_rounding:%d", MAKE_TAB(tap, tab_num+1), pu->params.downScaler.do_rounding);
}

void
ExynosVpuPuDownscaler::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuDownscaler::display_structure_info(tab_num, &m_pu_info);
};

status_t
ExynosVpuPuDownscaler::checkAndUpdatePuIoInfo(void)
{
    status_t ret = NO_ERROR;

    ExynosVpuIoSize *in_io_size = getInIoSize();
    ExynosVpuIoSize *out_io_size = getOutIoSize();
    if ((in_io_size == NULL) || (out_io_size == NULL)) {
        VXLOGE("getting iosize fails");
        return INVALID_OPERATION;
    }

    uint32_t in_width, in_height, in_roi_widht, in_roi_height;
    ret = in_io_size->getDimension(&in_width, &in_height, &in_roi_widht, &in_roi_height);
    if (ret != NO_ERROR) {
        VXLOGE("getting diemnsion infroation");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    if (out_io_size->getType() != VPUL_SIZEOP_SCALE) {
        VXLOGE("the scaler's out size should be VPUL_SIZEOP_SCALE");
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    ExynosVpuIoSizeOpScale *size_op_scale = ((ExynosVpuIoSizeScale*)out_io_size)->getSizeOpScale();
    struct vpul_scales *scales_info = size_op_scale->getScaleInfo();

    uint32_t out_width, out_height;
    out_width = ceilf((float)in_width * ((float)scales_info->horizontal.numerator/(float)scales_info->horizontal.denominator));
    out_height = ceilf((float)in_height * ((float)scales_info->vertical.numerator/(float)scales_info->vertical.denominator));

    /* replace ratio to target in/out size */
    scales_info->horizontal.numerator = out_width;
    scales_info->horizontal.denominator = in_width;
    scales_info->vertical.numerator = out_height;
    scales_info->vertical.denominator = in_height;

    return NO_ERROR;
}

void
ExynosVpuPuJoiner::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [joiner]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] out_byte0_source_stream:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.out_byte0_source_stream);
    VXLOGI("%s[------] out_byte1_source_stream:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.out_byte1_source_stream);
    VXLOGI("%s[------] out_byte2_source_stream:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.out_byte2_source_stream);
    VXLOGI("%s[------] out_byte3_source_stream:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.out_byte3_source_stream);
    VXLOGI("%s[------] input0_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.input0_enable);
    VXLOGI("%s[------] input1_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.input1_enable);
    VXLOGI("%s[------] input2_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.input2_enable);
    VXLOGI("%s[------] input3_enable:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.input3_enable);
    VXLOGI("%s[------] work_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.joiner.work_mode);
}

void
ExynosVpuPuJoiner::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuJoiner::display_structure_info(tab_num, &m_pu_info);
};

void
ExynosVpuPuSplitter::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [splitter]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] out0_byte0:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out0_byte0);
    VXLOGI("%s[------] out0_byte1:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out0_byte1);
    VXLOGI("%s[------] out1_byte0:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out1_byte0);
    VXLOGI("%s[------] out1_byte1:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out1_byte1);
    VXLOGI("%s[------] out2_byte0:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out2_byte0);
    VXLOGI("%s[------] out2_byte1:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out2_byte1);
    VXLOGI("%s[------] out3_byte0:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out3_byte0);
    VXLOGI("%s[------] out3_byte1:%d", MAKE_TAB(tap, tab_num+1), pu->params.spliter.out3_byte1);
}

void
ExynosVpuPuSplitter::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuSplitter::display_structure_info(tab_num, &m_pu_info);
};

status_t
ExynosVpuPuSplitter::checkContraint(void)
{
    uint32_t output_en_num = 0;

    if ((m_pu_info.params.spliter.out0_byte0 != SPLIT_OUT_DISABLE) || (m_pu_info.params.spliter.out0_byte1 != SPLIT_OUT_DISABLE))
        output_en_num++;
    if ((m_pu_info.params.spliter.out1_byte0 != SPLIT_OUT_DISABLE) || (m_pu_info.params.spliter.out1_byte1 != SPLIT_OUT_DISABLE))
        output_en_num++;
    if ((m_pu_info.params.spliter.out2_byte0 != SPLIT_OUT_DISABLE) || (m_pu_info.params.spliter.out2_byte1 != SPLIT_OUT_DISABLE))
        output_en_num++;
    if ((m_pu_info.params.spliter.out3_byte0 != SPLIT_OUT_DISABLE) || (m_pu_info.params.spliter.out3_byte1 != SPLIT_OUT_DISABLE))
        output_en_num++;

    uint32_t connected_pu_num = 0;
    Vector<pair<ExynosVpuPu*, uint32_t>>::iterator port_iter;
    for (port_iter=m_out_port_slots.begin(); port_iter!=m_out_port_slots.end(); port_iter++) {
        if (port_iter->first != NULL) {
            connected_pu_num++;
        }
    }

    if (output_en_num != connected_pu_num) {
        VXLOGE("output enable num is not matching with connected pu, %d, %d", output_en_num, connected_pu_num);
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuPuDuplicator::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [duplicator] %p", MAKE_TAB(tap, tab_num+1), pu);
    /* no param */
}

void
ExynosVpuPuDuplicator::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuDuplicator::display_structure_info(tab_num, &m_pu_info);
}

status_t
ExynosVpuPuDuplicator::checkContraint(void)
{
    if (m_pu_info.n_in_connect != 1) {
        VXLOGE("%s(pu_%d), the n_in_connect is not matching, expect:1, real:%d", m_name, m_pu_index_in_task, m_pu_info.n_in_connect);
        displayObjectInfo(0);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuPuHistogram::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [histogram] %p", MAKE_TAB(tap, tab_num+1), pu);
    VXLOGI("%s[------] offset:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.offset);
    VXLOGI("%s[------] inverse_binsize:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.inverse_binsize);
    VXLOGI("%s[------] variable_increment:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.variable_increment);
    VXLOGI("%s[------] dual_histogram:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.dual_histogram);
    VXLOGI("%s[------] signed_in0:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.signed_in0);

    VXLOGI("%s[------] round_index:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.round_index);
    VXLOGI("%s[------] max_val:%d", MAKE_TAB(tap, tab_num+1), pu->params.histogram.max_val);
}

void
ExynosVpuPuHistogram::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuHistogram::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuNlf::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [nlf]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] filter_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.filter_mode);
    VXLOGI("%s[------] fast_score_direction:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.fast_score_direction);
    VXLOGI("%s[------] border_mode:%d/%d/%d/%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.border_mode_up,
                                                                                                                                pu->params.nlf.border_mode_down,
                                                                                                                                pu->params.nlf.border_mode_left,
                                                                                                                                pu->params.nlf.border_mode_right);
    VXLOGI("%s[------] border_fill:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.border_fill);
    VXLOGI("%s[------] border_fill_constant:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.border_fill_constant);
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.bits_in);
    VXLOGI("%s[------] signed_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.signed_in);
    VXLOGI("%s[------] census_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.census_mode);
    VXLOGI("%s[------] census_out_image:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.census_out_image);
    VXLOGI("%s[------] add_orig_pixel:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.add_orig_pixel);
    VXLOGI("%s[------] cens_thres:%d %d %d %d %d %d %d %d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.cens_thres_0,
                                                                                                                                                pu->params.nlf.cens_thres_1,
                                                                                                                                                pu->params.nlf.cens_thres_2,
                                                                                                                                                pu->params.nlf.cens_thres_3,
                                                                                                                                                pu->params.nlf.cens_thres_4,
                                                                                                                                                pu->params.nlf.cens_thres_5,
                                                                                                                                                pu->params.nlf.cens_thres_6,
                                                                                                                                                pu->params.nlf.cens_thres_7);
    VXLOGI("%s[------] neighbors_mask:%d", MAKE_TAB(tap, tab_num+1), pu->params.nlf.neighbors_mask);
}

void
ExynosVpuPuNlf::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuNlf::display_structure_info(tab_num, &m_pu_info);
};

void
ExynosVpuPuCnn::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [cnn]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] is_fetch_mode:%d", MAKE_TAB(tap, tab_num+1), pu->params.cnn.is_fetch_mode);
    VXLOGI("%s[------] is_bypass:%d", MAKE_TAB(tap, tab_num+1), pu->params.cnn.is_bypass);
}

void
ExynosVpuPuCnn::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuCnn::display_structure_info(tab_num, &m_pu_info);
}

void
ExynosVpuPuFifo::display_structure_info(uint32_t tab_num, struct vpul_pu *pu)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[------] [fifo]", MAKE_TAB(tap, tab_num+1));
    VXLOGI("%s[------] bits_in:%d", MAKE_TAB(tap, tab_num+1), pu->params.fifo.bits_in);
}

void
ExynosVpuPuFifo::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuPu::displayObjectInfo(tab_num);
    ExynosVpuPuFifo::display_structure_info(tab_num, &m_pu_info);
};

}; /* namespace android */

