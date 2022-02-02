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

#define LOG_TAG "ExynosVpuSubchain"
#include <cutils/log.h>

#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"
#include "ExynosVpuCpuOp.h"

namespace android {

static uint32_t getSubchainIndexInTask(uint8_t *base, uint8_t *subchain)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = subchain - base;
    remained_byte -= task->sc_vec_ofs;

    uint32_t i;
    for (i=0; i<task->t_num_of_subchains; i++) {
        if (remained_byte == 0) {
            break;
        }

        remained_byte -= sizeof(struct vpul_subchain);
    }

    if (i == task->t_num_of_subchains) {
        VXLOGE("can't find subchain index");
    }

    return i;
}

status_t
ExynosVpuSubchain::updateObject(void)
{
    /* do nothing */
    return NO_ERROR;
}

void
ExynosVpuSubchain::display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain)
{
    char tap[MAX_TAB_NUM];

    uint32_t subchain_index = getSubchainIndexInTask((uint8_t*)base, (uint8_t*)subchain);

    VXLOGI("%s[SC_%d] stype:%d", MAKE_TAB(tap, tab_num), subchain_index, subchain->stype);
    VXLOGI("%s[SC_%d] id:0x%x", MAKE_TAB(tap, tab_num), subchain_index, subchain->id);

    switch(subchain->stype) {
    case VPUL_SUB_CH_HW:
        ExynosVpuSubchainHw::display_structure_info(tab_num, base, subchain);
        break;
    case VPUL_SUB_CH_CPU_OP:
        ExynosVpuSubchainCpu::display_structure_info(tab_num, base, subchain);
        break;
    default:
        VXLOGE("un-defined stype:%d", subchain->stype);
        break;
    }
}

void
ExynosVpuSubchain::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[SC_%d] stype:%d", MAKE_TAB(tap, tab_num), m_subchain_index_in_task, m_subchain_info.stype);
    VXLOGI("%s[SC_%d] id:0x%x", MAKE_TAB(tap, tab_num), m_subchain_index_in_task, m_subchain_info.id);
}

ExynosVpuSubchain::ExynosVpuSubchain(ExynosVpuVertex *vertex, enum  vpul_subchain_types type)
{
    m_vertex = vertex;

    memset(&m_subchain_info, 0x0, sizeof(m_subchain_info));

    m_subchain_index_in_task = vertex->addSubchain(this);
    m_subchain_index_in_vertex = m_subchain_index_in_task - vertex->getSubchain(0)->getIndexInTask();
    m_subchain_info.stype = type;

    m_subchain_info.id = (uint16_t)SUBCHAIN_ID(vertex->getTask()->getId(), m_subchain_index_in_task);
}

ExynosVpuSubchain::ExynosVpuSubchain(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info)
{
    m_vertex = vertex;

    m_subchain_info = *subchain_info;
    m_subchain_index_in_task = vertex->addSubchain(this);
    m_subchain_index_in_vertex = m_subchain_index_in_task - vertex->getSubchain(0)->getIndexInTask();

    /* id should be replaced by framework */
    m_subchain_info.id = (uint16_t)SUBCHAIN_ID(vertex->getTask()->getId(), m_subchain_index_in_task);
}

enum vpul_subchain_types
ExynosVpuSubchain::getType(void)
{
    return m_subchain_info.stype;
}

uint32_t
ExynosVpuSubchain::getId(void)
{
    return m_subchain_info.id;
}

uint32_t
ExynosVpuSubchain::getIndexInTask(void)
{
    return m_subchain_index_in_task;
}

uint32_t
ExynosVpuSubchain::getIndexInVertex(void)
{
    return m_subchain_index_in_vertex;
}

ExynosVpuVertex*
ExynosVpuSubchain::getVertex(void)
{
    return m_vertex;
}

ExynosVpuProcess*
ExynosVpuSubchain::getProcess(void)
{
    if (m_vertex->getType() == VPUL_VERTEXT_PROC)
        return (ExynosVpuProcess*)m_vertex;
    else
        return NULL;
}

ExynosVpu3dnnProcess*
ExynosVpuSubchain::getTdnnProcess(void)
{
    if (m_vertex->getType() == VPUL_VERTEXT_3DNN_PROC)
        return (ExynosVpu3dnnProcess*)m_vertex;
    else
        return NULL;
}

status_t
ExynosVpuSubchain::spreadSubchainIoInfo(void)
{
    return INVALID_OPERATION;
}

status_t
ExynosVpuSubchain::insertSubchainToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    struct vpul_subchain *td_subchain;

    ret = updateStructure(task_descriptor_base);
    if (ret != NO_ERROR) {
        VXLOGE("updating info fails");
        displayObjectInfo(0);
        goto EXIT;
    }

    td_subchain = sc_ptr((struct vpul_task*)task_descriptor_base, m_subchain_index_in_task);
    memcpy(td_subchain, &m_subchain_info, sizeof(struct vpul_subchain));

EXIT:
    return ret;
}

status_t
ExynosVpuSubchain::updateStructure(void *task_descriptor_base)
{
    if (task_descriptor_base==NULL) {
        VXLOGE("base is null");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuSubchainHw::display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain)
{
    char tap[MAX_TAB_NUM];

    uint32_t subchain_index = getSubchainIndexInTask((uint8_t*)base, (uint8_t*)subchain);

    VXLOGI("%s[SC_%d] num of pu:%d, pus_ofs:%p", MAKE_TAB(tap, tab_num), subchain_index, subchain->num_of_pus, subchain->pus_ofs);

    if (subchain->num_of_pus) {
        struct vpul_pu *pu = fst_sc_pu_ptr(base, subchain);
        for (uint32_t i=0; i<subchain->num_of_pus; i++, pu++) {
            ExynosVpuPu::display_structure_info(tab_num+1, base, pu);
        }
    }
}

void
ExynosVpuSubchainHw::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuSubchain::displayObjectInfo(tab_num);

    VXLOGI("%s[SC_%d] num of pu:%d, pus_ofs:NULL", MAKE_TAB(tap, tab_num), m_subchain_index_in_task, m_pu_vector.size());

    Vector<ExynosVpuPu*>::iterator pu_iter;
    for (pu_iter=m_pu_vector.begin(); pu_iter!=m_pu_vector.end(); pu_iter++) {
        (*pu_iter)->displayObjectInfo(tab_num+1);
    }
}

ExynosVpuSubchainHw::ExynosVpuSubchainHw(ExynosVpuVertex *vertex)
                                                                                    :ExynosVpuSubchain(vertex, VPUL_SUB_CH_HW)
{
    memset(&m_used_pu_map, 0x0, sizeof(m_used_pu_map));
}

ExynosVpuSubchainHw::ExynosVpuSubchainHw(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info)
                                                                                    :ExynosVpuSubchain(vertex, subchain_info)
{
    memset(&m_used_pu_map, 0x0, sizeof(m_used_pu_map));
}

uint32_t
ExynosVpuSubchainHw::addPu(ExynosVpuPu *pu)
{
    uint32_t pu_instance = pu->getInstance();
    if (m_used_pu_map[pu_instance]) {
        VXLOGE("already same pu(%d) is assigned in subchain_%d", pu_instance, m_subchain_index_in_task);
        return 0;
    } else {
        m_used_pu_map[pu_instance] = 1;
    }

    uint32_t pu_idx_in_task = m_vertex->getTask()->addPu(pu);

    if (m_pu_vector.empty()) {
        /* it will match with pus_ofs */
        m_base_pu_idx = pu_idx_in_task;
    }

    m_pu_vector.push_back(pu);

    return pu_idx_in_task;
}

ExynosVpuPu*
ExynosVpuSubchainHw::getPu(uint32_t index)
{
    if (index >= m_pu_vector.size()) {
        VXLOGE("pu index is out of bound, index:%d, size:%d", index, m_pu_vector.size());

        return NULL;
    }

    return m_pu_vector[index];
}

ExynosVpuPu*
ExynosVpuSubchainHw::getPu(enum vpul_pu_instance pu_instance)
{
    Vector<ExynosVpuPu*>::iterator pu_iter;
    for (pu_iter=m_pu_vector.begin(); pu_iter!=m_pu_vector.end(); pu_iter++) {
        if ((*pu_iter)->getInstance() == pu_instance) {
            return (*pu_iter);
        }
    }

    VXLOGE("can't find pu, instance:%d", pu_instance);

    return NULL;
}

status_t
ExynosVpuSubchainHw::spreadSubchainIoInfo(void)
{
    status_t ret = NO_ERROR;

    Vector<ExynosVpuPu*>::iterator pu_iter;
    for (pu_iter=m_pu_vector.begin(); pu_iter!=m_pu_vector.end(); pu_iter++) {
        ret = (*pu_iter)->checkAndUpdatePuIoInfo();
        if (ret != NO_ERROR) {
            VXLOGE("spreading io info fails");
            break;
        }
    }

    return ret;
}

status_t
ExynosVpuSubchainHw::updateStructure(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuSubchain::updateStructure(task_descriptor_base);
    if (ret != NO_ERROR) {
        VXLOGE("update vertex structure fails");
        goto EXIT;
    }

    m_subchain_info.num_of_pus = m_pu_vector.size();
    if (m_subchain_info.num_of_pus) {
        m_subchain_info.pus_ofs = (uint32_t)((uint8_t*)pu_ptr((struct vpul_task*)task_descriptor_base, getPu(0)->getIndexInTask()) - (uint8_t*)task_descriptor_base);
    }

EXIT:
    return ret;
}

void
ExynosVpuSubchainCpu::display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain)
{
    char tap[MAX_TAB_NUM];

    uint32_t subchain_index = getSubchainIndexInTask((uint8_t*)base, (uint8_t*)subchain);

    VXLOGI("%s[SC_%d] num_of_cpu_op:%d", MAKE_TAB(tap, tab_num), subchain_index, subchain->num_of_cpu_op);
    VXLOGI("%s[SC_%d] FstIntRamInd:%d, IntRamIndices:%d, CpuRamIndices:%d", MAKE_TAB(tap, tab_num), subchain_index,
                                                                                                        subchain->cpu.FstIntRamInd,
                                                                                                        subchain->cpu.IntRamIndices,
                                                                                                        subchain->cpu.CpuRamIndices);

    for (uint32_t i=0; i<subchain->num_of_cpu_op; i++) {
        ExynosVpuCpuOp::display_structure_info(tab_num+1, &subchain->cpu.cpu_op_desc[i]);
    }
}

void
ExynosVpuSubchainCpu::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuSubchain::displayObjectInfo(tab_num);

    VXLOGI("%s[SC_%d] num_of_cpu_op:%d", MAKE_TAB(tap, tab_num), m_subchain_index_in_task, m_subchain_info.num_of_cpu_op);
    VXLOGI("%s[SC_%d] FstIntRamInd:%d, IntRamIndices:%d, CpuRamIndices:%d", MAKE_TAB(tap, tab_num), m_subchain_index_in_task,
                                                                                                        m_subchain_info.cpu.FstIntRamInd,
                                                                                                        m_subchain_info.cpu.IntRamIndices,
                                                                                                        m_subchain_info.cpu.CpuRamIndices);

    Vector<ExynosVpuCpuOp*>::iterator op_iter;
    for (op_iter=m_op_vector.begin(); op_iter!=m_op_vector.end(); op_iter++) {
        (*op_iter)->displayObjectInfo(tab_num+1);
    }
}

ExynosVpuSubchainCpu::ExynosVpuSubchainCpu(ExynosVpuVertex *vertex)
                                                                                    :ExynosVpuSubchain(vertex, VPUL_SUB_CH_CPU_OP)
{

}

ExynosVpuSubchainCpu::ExynosVpuSubchainCpu(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info)
                                                                                    :ExynosVpuSubchain(vertex, subchain_info)
{

}

uint32_t
ExynosVpuSubchainCpu::addCpuOp(ExynosVpuCpuOp *cpu_op)
{
    uint32_t op_idx_in_subchain = m_op_vector.size();

    m_op_vector.push_back(cpu_op);

    return op_idx_in_subchain;
}

status_t
ExynosVpuSubchainCpu::spreadSubchainIoInfo(void)
{
    /* do nothing */
    return NO_ERROR;
}

status_t
ExynosVpuSubchainCpu::updateStructure(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuSubchain::updateStructure(task_descriptor_base);
    if (ret != NO_ERROR) {
        VXLOGE("update vertex structure fails");
        goto EXIT;
    }

    m_subchain_info.num_of_cpu_op = m_op_vector.size();

    uint32_t i;
    Vector<ExynosVpuCpuOp*>::iterator op_iter;
    for (op_iter=m_op_vector.begin(), i=0; op_iter!=m_op_vector.end(); op_iter++, i++) {
        (*op_iter)->updateStructure();
        if ((*op_iter)->checkConstraint() == NO_ERROR) {
            memcpy(&m_subchain_info.cpu.cpu_op_desc[i], (*op_iter)->getCpuOpInfo(), sizeof(m_subchain_info.cpu.cpu_op_desc[i]));
        }
    }

EXIT:
    return ret;
}

}; /* namespace android */

