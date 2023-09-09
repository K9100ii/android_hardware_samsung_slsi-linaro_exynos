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

#define LOG_TAG "ExynosVpuTask"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuTaskIf.h"
#include "ExynosVpuTask.h"

#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

static uint32_t
convertSubchainIndex(struct vpul_task *task, struct vpul_subchain *subchain)
{
    int32_t subchain_offset = (uint64_t)subchain - (uint64_t)task;

    subchain_offset -= sizeof(struct vpul_task);
    subchain_offset -= task->t_num_of_vertices * sizeof(struct vpul_vertex);

    if ((subchain_offset < 0) || ((subchain_offset % sizeof(struct vpul_subchain)) != 0)) {
        VXLOGE("subchain index in task is not valid");
        return 0;
    }

    uint32_t subchain_index = subchain_offset / sizeof(struct vpul_subchain);

    return subchain_index;
}

static uint32_t
convertPuIndex(struct vpul_task *task, struct vpul_pu *pu)
{
    int32_t pu_offset = (uint64_t)pu - (uint64_t)task;

    pu_offset -= sizeof(struct vpul_task);
    pu_offset -= task->t_num_of_vertices * sizeof(struct vpul_vertex);
    pu_offset -= task->t_num_of_subchains * sizeof(struct vpul_subchain);

    if ((pu_offset < 0) || ((pu_offset % sizeof(struct vpul_pu)) != 0)) {
        VXLOGE("subchain index in task is not valid");
        return 0;
    }

    uint32_t pu_index = pu_offset / sizeof(struct vpul_pu);

    return pu_index;
}

static uint32_t getMemmapIndexInTask(uint8_t *base, uint8_t *memmap_desc)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = memmap_desc - (uint8_t*)&task->memmap_desc[0];

    uint32_t index = remained_byte/sizeof(task->memmap_desc[0]);

    if (index >= task->n_memmap_desc) {
        VXLOGE("out of memmap index, %d, %d", index, task->n_memmap_desc);
        index = 0;
    }

    return index;
}

static uint32_t getExternalMemIndexInTask(uint8_t *base, uint8_t *external_mem_addr)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = external_mem_addr - (uint8_t*)&task->external_mem_addr[0];

    uint32_t index = remained_byte/sizeof(task->external_mem_addr[0]);

    if (index >= task->n_external_mem_addresses) {
        VXLOGE("out of n_external_mem_addr index, %d, %d", index, task->n_external_mem_addresses);
        index = 0;
    }

    return index;
}

void
ExynosVpuTask::display_structure_info(uint32_t tab_num, struct vpul_task *task)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("[Structure]");
    VXLOGI("%s[Task] id: 0x%x, priority: %d", MAKE_TAB(tap, tab_num), task->id, task->priority);
    VXLOGI("%s[Task] total sizes, byte: %d", MAKE_TAB(tap, tab_num), task->total_size);

    struct vpul_vertex *vertex = fst_vtx_ptr(task);
    struct vpul_3dnn_process_base *tdnn_proc_base = fst_3dnn_process_base_ptr(task);

    uint32_t i;

    VXLOGI("%s(Task) n_memmap_desc: %d", MAKE_TAB(tap, tab_num), task->n_memmap_desc);
    struct vpul_memory_map_desc *memmap_desc;
    for (i=0; i<task->n_memmap_desc; i++) {
        memmap_desc = &task->memmap_desc[i];
        ExynosVpuMemmap::display_structure_info(tab_num+1, task, memmap_desc);
    }

    VXLOGI("%s(Task) n_external_mem_addresses: %d", MAKE_TAB(tap, tab_num), task->n_external_mem_addresses);
    uint32_t *external_mem_addr;
    for (i=0; i<task->n_external_mem_addresses; i++) {
        external_mem_addr = &task->external_mem_addr[i];
        ExynosVpuIoExternalMem::display_structure_info(tab_num+1, task, external_mem_addr);
    }

    VXLOGI("%s(Task) n_internal_rams: %d", MAKE_TAB(tap, tab_num), task->n_internal_rams);
    struct vpul_internal_ram *internal_ram;
    for (i=0; i<task->n_internal_rams; i++) {
        internal_ram = &task->internal_rams[i];
        ExynosVpuIoInternalRam::display_structure_info(tab_num+1, internal_ram);
    }

    VXLOGI("%s[Task] total vertices num: %d", MAKE_TAB(tap, tab_num), task->t_num_of_vertices);
    VXLOGI("%s[Task] total 3dnn_proc_base num: %d", MAKE_TAB(tap, tab_num), task->t_num_of_3dnn_process_bases);
    VXLOGI("%s[Task] total subchains num: %d", MAKE_TAB(tap, tab_num), task->t_num_of_subchains);
    VXLOGI("%s[Task] total pus num: %d", MAKE_TAB(tap, tab_num), task->t_num_of_pus);
    VXLOGI("%s[Task] total pu params num: %d", MAKE_TAB(tap, tab_num), task->t_num_of_pu_params_on_invoke);

    for (i=0; i<task->t_num_of_3dnn_process_bases; i++, tdnn_proc_base++) {
        ExynosVpu3dnnProcessBase::display_structure_info(tab_num+1, task, tdnn_proc_base);
    }

    for (i=0; i<task->t_num_of_vertices; i++, vertex++) {
        ExynosVpuVertex::display_structure_info(tab_num+1, task, vertex);
    }

    struct vpul_pu_location *updatable_pu;
    updatable_pu = fst_updateble_pu_location_ptr(task);
    for (i=0; i<task->t_num_of_pu_params_on_invoke; i++, updatable_pu++) {
        ExynosVpuUpdatablePu::display_structure_info(tab_num+1, task, updatable_pu);
    }
}

void
ExynosVpuTask::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("[Object]");
    VXLOGI("%s[Task] id: 0x%x, priority: %d", MAKE_TAB(tap, tab_num), m_task_info.id, m_task_info.priority);
    VXLOGI("%s[Task] total sizes, byte: NULL", MAKE_TAB(tap, tab_num));

    VXLOGI("%s(Task) n_memmap_desc: %d", MAKE_TAB(tap, tab_num), m_io_memmap_list.size());
    Vector<ExynosVpuMemmap*>::iterator memmap_iter;
    for (memmap_iter=m_io_memmap_list.begin(); memmap_iter!=m_io_memmap_list.end(); memmap_iter++) {
        (*memmap_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Task) n_external_mem_addresses: %d", MAKE_TAB(tap, tab_num), m_io_external_mem_list.size());
    Vector<ExynosVpuIoExternalMem*>::iterator external_iter;
    for (external_iter=m_io_external_mem_list.begin(); external_iter!=m_io_external_mem_list.end(); external_iter++) {
        (*external_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Task) n_internal_rams: %d", MAKE_TAB(tap, tab_num), m_io_internal_ram_list.size());
    Vector<ExynosVpuIoInternalRam*>::iterator ram_iter;
    for (ram_iter=m_io_internal_ram_list.begin(); ram_iter!=m_io_internal_ram_list.end(); ram_iter++) {
        (*ram_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s[Task] total vertices num: %d", MAKE_TAB(tap, tab_num), m_vertex_vector.size());
    VXLOGI("%s[Task] total subchains num: %d", MAKE_TAB(tap, tab_num), m_subchain_vector.size());
    VXLOGI("%s[Task] total pus num: %d", MAKE_TAB(tap, tab_num), m_pu_vector.size());
    VXLOGI("%s[Task] total pu params num: %d", MAKE_TAB(tap, tab_num), m_updatable_pu_vector.size());

    Vector<ExynosVpuVertex*>::iterator vertex_iter;
    for (vertex_iter=m_vertex_vector.begin(); vertex_iter!=m_vertex_vector.end(); vertex_iter++) {
        (*vertex_iter)->displayObjectInfo(tab_num+1);
    }

    Vector<ExynosVpuUpdatablePu*>::iterator updatable_iter;
    for (updatable_iter=m_updatable_pu_vector.begin(); updatable_iter!=m_updatable_pu_vector.end(); updatable_iter++) {
        (*updatable_iter)->displayObjectInfo(tab_num+1);
    }
}

ExynosVpuTask::ExynosVpuTask(ExynosVpuTaskIf *task_if)
{
    m_task_if = task_if;
    memset(&m_task_info, 0x0, sizeof(m_task_info));

    m_task_info.id = m_task_if->addTask(this);
}

ExynosVpuTask::ExynosVpuTask(ExynosVpuTaskIf *task_if, const struct vpul_task *task_info)
{
    m_task_if = task_if;
    m_task_info = *task_info;

    m_task_info.id = m_task_if->addTask(this);

    uint32_t i;

    ExynosVpuMemmap *memmap_object;
    const struct vpul_memory_map_desc *memmap_info;
    for (i=0; i<task_info->n_memmap_desc; i++) {
        memmap_info = &task_info->memmap_desc[i];
        switch (memmap_info->mtype) {
        case VPUL_MEM_EXTERNAL:
            memmap_object = new ExynosVpuMemmapExternal(this, memmap_info);
            break;
        case VPUL_MEM_PRELOAD_PU:
            memmap_object = new ExynosVpuMemmapPreloadPu(this, memmap_info);
            break;
        default:
            VXLOGE("un-supported memmap type:0x%x", memmap_info->mtype);
            break;
        }
    }

    ExynosVpuIoExternalMem *external_mem_object;
    const __u32 *external_mem_info;
    for (i=0; i<task_info->n_external_mem_addresses; i++) {
        external_mem_info = &task_info->external_mem_addr[i];
        external_mem_object = new ExynosVpuIoExternalMem(this, external_mem_info);
    }

    ExynosVpuIoInternalRam *ram_object;
    const struct vpul_internal_ram *ram_info;
    for (i=0; i<task_info->n_internal_rams; i++) {
        ram_info = &task_info->internal_rams[i];
        ram_object = new ExynosVpuIoInternalRam(this, ram_info);
    }
}

ExynosVpuTask::~ExynosVpuTask()
{
    uint32_t i;

    for (i=0; i<m_vertex_vector.size(); i++)
        delete m_vertex_vector[i];

    for (i=0; i<m_subchain_vector.size(); i++)
        delete m_subchain_vector[i];

    for (i=0; i<m_pu_vector.size(); i++)
        delete m_pu_vector[i];

    for (i=0; i<m_io_memmap_list.size(); i++)
        delete m_io_memmap_list[i];

    for (i=0; i<m_io_external_mem_list.size(); i++)
        delete m_io_external_mem_list[i];
}

ExynosVpuTaskIf*
ExynosVpuTask::getTaskIf(void)
{
    return m_task_if;
}

void
ExynosVpuTask::setId(uint32_t task_id)
{
    m_task_info.id = task_id;
}

void
ExynosVpuTask::setTotalSize(uint32_t total_size)
{
    m_task_info.total_size = total_size;
}

uint32_t
ExynosVpuTask::getId(void)
{
    return m_task_info.id;
}

uint32_t
ExynosVpuTask::addUpdatablePu(ExynosVpuUpdatablePu *updatable_up)
{
    m_updatable_pu_vector.push_back(updatable_up);

    return m_updatable_pu_vector.size() - 1;
}

uint32_t
ExynosVpuTask::addPu(ExynosVpuPu *pu)
{
    m_pu_vector.push_back(pu);

    return m_pu_vector.size() - 1;
}

uint32_t
ExynosVpuTask::addSubchain(ExynosVpuSubchain *subchain)
{
    m_subchain_vector.push_back(subchain);

    return m_subchain_vector.size() - 1;
}

uint32_t
ExynosVpuTask::addVertex(ExynosVpuVertex *vertex)
{
    m_vertex_vector.push_back(vertex);

    return m_vertex_vector.size() - 1;
}

uint32_t
ExynosVpuTask::addTdnnProcBase(ExynosVpu3dnnProcessBase *tdnn_proc_base)
{
    m_tdnn_proc_base_vector.push_back(tdnn_proc_base);

    return m_tdnn_proc_base_vector.size() - 1;
}

uint32_t
ExynosVpuTask::getVertexNumber(void)
{
    return m_vertex_vector.size();
}

uint32_t
ExynosVpuTask::getTotalTdnnProcBaseNumber(void)
{
    return m_tdnn_proc_base_vector.size();
}

uint32_t
ExynosVpuTask::getTotalSubchainNumber(void)
{
    return m_subchain_vector.size();
}

uint32_t
ExynosVpuTask::getTotalPuNumber(void)
{
    return m_pu_vector.size();
}

uint32_t
ExynosVpuTask::getTotalUpdatablePuNumber(void)
{
    return m_updatable_pu_vector.size();
}

ExynosVpuVertex*
ExynosVpuTask::getVertex(uint32_t index)
{
    if (m_vertex_vector.size() <= index) {
        VXLOGE("vertex_%d is out of bound(size:%d)", index, m_vertex_vector.size());
        return NULL;
    }

    return m_vertex_vector.editItemAt(index);
}

ExynosVpu3dnnProcessBase*
ExynosVpuTask::getTdnnProcBase(uint32_t index)
{
    if (m_tdnn_proc_base_vector.size() <= index) {
        VXLOGE("tdnnProcBase_%d is out of bound(size:%d)", index, m_tdnn_proc_base_vector.size());
        return NULL;
    }

    return m_tdnn_proc_base_vector.editItemAt(index);
}

ExynosVpuSubchain*
ExynosVpuTask::getSubchain(uint32_t index)
{
    if (m_subchain_vector.size() <= index) {
        VXLOGE("subchain_%d is out of bound(size:%d)", index, m_subchain_vector.size());
        return NULL;
    }

    return m_subchain_vector.editItemAt(index);
}

ExynosVpuPu*
ExynosVpuTask::getPu(uint32_t phy_index)
{
    if (m_pu_vector.size() <= phy_index) {
        VXLOGE("pu_%d is out of bound(size:%d)", phy_index, m_pu_vector.size());
        return NULL;
    }

    return m_pu_vector.editItemAt(phy_index);
}

ExynosVpuPu*
ExynosVpuTask::getPu(enum vpul_pu_instance pu_instance, uint32_t vertex_index, uint32_t subchain_index)
{
    ExynosVpuVertex *vertex = getVertex(vertex_index);
    if (vertex == NULL) {
        VXLOGE("vertex index is out of bound, index:%d, total:%d", vertex_index, getVertexNumber());
        return NULL;
    }

    ExynosVpuSubchain *subchain = vertex->getSubchain(subchain_index);
    if (subchain == NULL) {
        VXLOGE("getting subchain fails");
        return NULL;
    }

    ExynosVpuSubchainHw *subchain_hw = NULL;
    if (subchain->getType() == VPUL_SUB_CH_HW) {
        subchain_hw = (ExynosVpuSubchainHw*)subchain;
    } else {
        VXLOGE("subchain type is not hw, type:%d", subchain->getType());
        return NULL;
    }

    ExynosVpuPu *pu = subchain_hw->getPu(pu_instance);
    if (pu == NULL) {
        VXLOGE("subchain doesn't have pu_instance:%d", pu_instance);
        return NULL;
    }

    return pu;
}

ExynosVpuPu*
ExynosVpuTask::getPu(uint32_t pu_index, uint32_t vertex_index, uint32_t subchain_index)
{
    ExynosVpuVertex *vertex = getVertex(vertex_index);
    if (vertex == NULL) {
        VXLOGE("vertex index is out of bound, index:%d, total:%d", vertex_index, getVertexNumber());
        return NULL;
    }

    ExynosVpuSubchain *subchain = vertex->getSubchain(subchain_index);
    if (subchain == NULL) {
        VXLOGE("getting subchain fails");
        return NULL;
    }

    ExynosVpuSubchainHw *subchain_hw = NULL;
    if (subchain->getType() == VPUL_SUB_CH_HW) {
        subchain_hw = (ExynosVpuSubchainHw*)subchain;
    } else {
        VXLOGE("subchain type is not hw, type:%d", subchain->getType());
        return NULL;
    }

    ExynosVpuPu *pu = subchain_hw->getPu(pu_index);
    if (pu == NULL) {
        VXLOGE("subchain doesn't have pu_%d", pu_index);
        return NULL;
    }

    return pu;
}

uint32_t
ExynosVpuTask::addMemmap(ExynosVpuMemmap *memmap)
{
    m_io_memmap_list.push_back(memmap);

    return m_io_memmap_list.size() - 1;
}

uint32_t
ExynosVpuTask::getMemmapNumber(void)
{
    return m_io_memmap_list.size();
}

ExynosVpuMemmap*
ExynosVpuTask::getMemmap(uint32_t index)
{
    if (m_io_memmap_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_memmap_list.size());
        return NULL;
    }

    return m_io_memmap_list[index];
}

uint32_t
ExynosVpuTask::addExternalMem(ExynosVpuIoExternalMem *external_mem)
{
    m_io_external_mem_list.push_back(external_mem);

    return m_io_external_mem_list.size() - 1;
}

uint32_t
ExynosVpuTask::getExternalMemNumber(void)
{
    return m_io_external_mem_list.size();
}

ExynosVpuIoExternalMem*
ExynosVpuTask::getExternalMem(uint32_t index)
{
    if (m_io_external_mem_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_external_mem_list.size());
        return NULL;
    }

    return m_io_external_mem_list[index];
}

uint32_t
ExynosVpuTask::addInternalRam(ExynosVpuIoInternalRam *internal_ram)
{
    m_io_internal_ram_list.push_back(internal_ram);

    return m_io_internal_ram_list.size() - 1;
}

ExynosVpuIoInternalRam*
ExynosVpuTask::getInternalRam(uint32_t index)
{
    if (m_io_internal_ram_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_internal_ram_list.size());
        return NULL;
    }

    return m_io_internal_ram_list[index];
}

struct vpul_task*
ExynosVpuTask::getTaskInfo(void)
{
    return &m_task_info;
}

status_t
ExynosVpuTask::spreadTaskIoInfo(void)
{
    status_t ret = NO_ERROR;

    Vector<ExynosVpuVertex*>::iterator vertex_iter;
    for (vertex_iter=m_vertex_vector.begin(); vertex_iter!=m_vertex_vector.end(); vertex_iter++) {
        ret = (*vertex_iter)->spreadVertexIoInfo();
        if (ret != NO_ERROR) {
            VXLOGE("spreading io info fails");
            break;
        }
    }

    return ret;
}

status_t
ExynosVpuTask::updateStructure(void)
{
    status_t ret = NO_ERROR;

    m_task_info.t_num_of_vertices = m_vertex_vector.size();
    m_task_info.t_num_of_3dnn_process_bases = m_tdnn_proc_base_vector.size();
    m_task_info.t_num_of_subchains = m_subchain_vector.size();
    m_task_info.t_num_of_pus = m_pu_vector.size();
    m_task_info.t_num_of_pu_params_on_invoke = m_updatable_pu_vector.size();

    m_task_info.vertices_vec_ofs = sizeof(struct vpul_task);
    m_task_info.sc_vec_ofs = m_task_info.vertices_vec_ofs +
                                        (sizeof(struct vpul_vertex) * m_task_info.t_num_of_vertices);
    m_task_info.pus_vec_ofs = m_task_info.sc_vec_ofs +
                                        (sizeof(struct vpul_subchain) * m_task_info.t_num_of_subchains);
    m_task_info.process_bases_3dnn_vec_ofs = m_task_info.pus_vec_ofs +
                                        (sizeof(struct vpul_pu) * m_task_info.t_num_of_pus);
    m_task_info.invoke_params_vec_ofs = m_task_info.process_bases_3dnn_vec_ofs +
                                        (sizeof(struct vpul_3dnn_process_base) * m_task_info.t_num_of_3dnn_process_bases);

    if (m_task_info.t_num_of_vertices == 0)
        m_task_info.vertices_vec_ofs = 0;
    if (m_task_info.t_num_of_3dnn_process_bases == 0)
        m_task_info.process_bases_3dnn_vec_ofs = 0;
    if (m_task_info.t_num_of_subchains == 0)
        m_task_info.sc_vec_ofs = 0;
    if (m_task_info.t_num_of_pus == 0)
        m_task_info.pus_vec_ofs = 0;
    if (m_task_info.t_num_of_pu_params_on_invoke == 0)
        m_task_info.invoke_params_vec_ofs = 0;

    uint32_t i;
    /* memmap_desc part */
    m_task_info.n_memmap_desc = m_io_memmap_list.size();
    Vector<ExynosVpuMemmap*>::iterator memmap_iter;
    for (memmap_iter=m_io_memmap_list.begin(), i=0; memmap_iter!=m_io_memmap_list.end(); memmap_iter++, i++) {
        (*memmap_iter)->updateStructure();
        if (i == (*memmap_iter)->getIndex()) {
            memcpy(&m_task_info.memmap_desc[i], (*memmap_iter)->getMemmapInfo(), sizeof(m_task_info.memmap_desc[i]));
        } else {
            VXLOGE("the index of memmap is not matching, expected:%d, received:%d", i, (*memmap_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* external_mem_addr part */
    m_task_info.n_external_mem_addresses= m_io_external_mem_list.size();
    Vector<ExynosVpuIoExternalMem*>::iterator external_mem_iter;
    for (external_mem_iter=m_io_external_mem_list.begin(), i=0; external_mem_iter!=m_io_external_mem_list.end(); external_mem_iter++, i++) {
        (*external_mem_iter)->updateStructure();
        if (i == (*external_mem_iter)->getIndex()) {
            memcpy(&m_task_info.external_mem_addr[i], (*external_mem_iter)->getExternalMemInfo(), sizeof(m_task_info.external_mem_addr[i]));
        } else {
            VXLOGE("the index of external_mem is not matching, expected:%d, received:%d", i, (*external_mem_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* internal_rams part */
    m_task_info.n_internal_rams = m_io_internal_ram_list.size();
    Vector<ExynosVpuIoInternalRam*>::iterator ram_iter;
    for (ram_iter=m_io_internal_ram_list.begin(), i=0; ram_iter!=m_io_internal_ram_list.end(); ram_iter++, i++) {
        (*ram_iter)->updateStructure();
        if (i == (*ram_iter)->getIndex()) {
            if ((*ram_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&m_task_info.internal_rams[i], (*ram_iter)->getInternalRamInfo(), sizeof(m_task_info.internal_rams[i]));
            } else {
                VXLOGE("ram doesn't have valid configuration");
                (*ram_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of ram is not matching, expected:%d, received:%d", i, (*ram_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

EXIT:
    return NO_ERROR;
}

status_t
ExynosVpuTask::updateObject(void)
{
    Vector<ExynosVpuMemmap*>::iterator memmap_iter;
    for (memmap_iter=m_io_memmap_list.begin(); memmap_iter!=m_io_memmap_list.end(); memmap_iter++)
        (*memmap_iter)->updateObject();

    Vector<ExynosVpuIoExternalMem*>::iterator external_mem_iter;
    for (external_mem_iter=m_io_external_mem_list.begin(); external_mem_iter!=m_io_external_mem_list.end(); external_mem_iter++)
        (*external_mem_iter)->updateObject();

    Vector<ExynosVpuIoInternalRam*>::iterator ram_iter;
    for (ram_iter=m_io_internal_ram_list.begin(); ram_iter!=m_io_internal_ram_list.end(); ram_iter++)
        (*ram_iter)->updateObject();

    return NO_ERROR;
}

status_t
ExynosVpuTask::checkConstraint(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuTask::insertTaskToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    ret = updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating info fails");
        displayObjectInfo(0);
        goto EXIT;
    }

    ret = checkConstraint();
    if (ret != NO_ERROR) {
        VXLOGE("constraint checking fails");
        goto EXIT;
    }

    memcpy(task_descriptor_base, &m_task_info, sizeof(struct vpul_task));

    /* insert vertex structure information */
    for (uint32_t i=0; i<m_task_info.t_num_of_vertices; i++) {
        ret = m_vertex_vector[i]->insertVertexToTaskDescriptor(task_descriptor_base);
        if (ret != NO_ERROR) {
            VXLOGE("inserting vertex fails");
            break;
        }
    }

    /* insert 3dnn_proc_base structure information */
    for (uint32_t i=0; i<m_task_info.t_num_of_3dnn_process_bases; i++) {
        ret = m_tdnn_proc_base_vector[i]->insertTdnnProcBaseToTaskDescriptor(task_descriptor_base);
        if (ret != NO_ERROR) {
            VXLOGE("inserting 3dnn_proc_base fails");
            break;
        }
    }

    /* insert subchain structure information */
    for (uint32_t i=0; i<m_task_info.t_num_of_subchains; i++) {
        ret = m_subchain_vector[i]->insertSubchainToTaskDescriptor(task_descriptor_base);
        if (ret != NO_ERROR) {
            VXLOGE("insert subchain fails");
            goto EXIT;
        }
    }

    /* insert pu structure information */
    for (uint32_t i=0; i<m_task_info.t_num_of_pus; i++) {
        ret = m_pu_vector[i]->insertPuToTaskDescriptor(task_descriptor_base);
        if (ret != NO_ERROR) {
            VXLOGE("inserting pu fails");
            goto EXIT;
        }
    }

    /* insert pu location structure information */
    for (uint32_t i=0; i<m_task_info.t_num_of_pu_params_on_invoke; i++) {
        ret = m_updatable_pu_vector[i]->insertUpdatablePuToTaskDescriptor(task_descriptor_base);
        if (ret != NO_ERROR) {
            VXLOGE("inserting updatable pu fails");
            goto EXIT;
        }
    }

EXIT:
    return ret;
}

bool
ExynosVpuMemmap::compareSize(ExynosVpuMemmap *memmap0, ExynosVpuMemmap *memmap1)
{
    struct vpul_memory_map_desc *memmap_desc0 = memmap0->getMemmapInfo();
    struct vpul_memory_map_desc *memmap_desc1 = memmap1->getMemmapInfo();

    if ((memmap_desc0->image_sizes.width != memmap_desc1->image_sizes.width) ||
        (memmap_desc0->image_sizes.height != memmap_desc1->image_sizes.height) ||
        (memmap_desc0->image_sizes.pixel_bytes != memmap_desc1->image_sizes.pixel_bytes) ||
        (memmap_desc0->image_sizes.line_offset != memmap_desc1->image_sizes.line_offset)) {
        return false;
    }

    return true;
}

void
ExynosVpuMemmap::display_structure_info(uint32_t tab_num, void *base, struct vpul_memory_map_desc *memmap_desc)
{
    char tap[MAX_TAB_NUM];

    uint32_t memmap_index = getMemmapIndexInTask((uint8_t*)base, (uint8_t*)memmap_desc);

    VXLOGI("%s(memmap_%d) type:%d", MAKE_TAB(tap, tab_num),
                                                                    memmap_index,
                                                                    memmap_desc->mtype);
    VXLOGI("%s(--------) (size)width:%d, hegith:%d, pixel_bytes:%d, line_offset:%d", MAKE_TAB(tap, tab_num),
                                                                                            memmap_desc->image_sizes.width,
                                                                                            memmap_desc->image_sizes.height,
                                                                                            memmap_desc->image_sizes.pixel_bytes,
                                                                                            memmap_desc->image_sizes.line_offset);

    switch(memmap_desc->mtype) {
    case VPUL_MEM_EXTERNAL:
        ExynosVpuMemmapExternal::display_structure_info(tab_num, &memmap_desc->index);
        break;
    case VPUL_MEM_PRELOAD_PU:
        ExynosVpuMemmapPreloadPu::display_structure_info(tab_num, memmap_desc);
        break;
    default:
        VXLOGE("un-supported memmap type:0x%x", memmap_desc->mtype);
        break;
    }
}

void
ExynosVpuMemmap::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(memmap_%d) type:%d", MAKE_TAB(tap, tab_num),
                                                                    m_memmap_index,
                                                                    m_memmap_info.mtype);
    VXLOGI("%s(--------) (size)width:%d, hegith:%d, pixel_bytes:%d, line_offset:%d", MAKE_TAB(tap, tab_num),
                                                                                            m_memmap_info.image_sizes.width,
                                                                                            m_memmap_info.image_sizes.height,
                                                                                            m_memmap_info.image_sizes.pixel_bytes,
                                                                                            m_memmap_info.image_sizes.line_offset);
}

ExynosVpuMemmap::ExynosVpuMemmap(ExynosVpuTask *task, enum vpul_memory_types type)
{
    m_task = task;
    memset(&m_memmap_info, 0x0, sizeof(m_memmap_info));
    m_memmap_info.mtype = type;

    m_memmap_index = task->addMemmap(this);
}

ExynosVpuMemmap::ExynosVpuMemmap(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info)
{
    m_task = task;
    m_memmap_info = *memmap_info;

    m_memmap_index = task->addMemmap(this);
}

uint32_t
ExynosVpuMemmap::getIndex(void)
{
    return m_memmap_index;
}

struct vpul_memory_map_desc*
ExynosVpuMemmap::getMemmapInfo(void)
{
    return &m_memmap_info;
}

ExynosVpuIoMemory*
ExynosVpuMemmap::getMemory(void)
{
    VXLOGE("getting memory doesn't support in mtype:0x%x", m_memmap_info.mtype);
    return NULL;
}

status_t
ExynosVpuMemmap::updateStructure(void)
{
    status_t ret = NO_ERROR;

    /* do nothing */

    return ret;
}

status_t
ExynosVpuMemmap::updateObject(void)
{
    status_t ret = NO_ERROR;

    /* do nothing */

    return ret;
}

void
ExynosVpuMemmapExternal::display_structure_info(uint32_t tab_num, uint32_t *external_mem_desc)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(--------) [external mem] index:%d", MAKE_TAB(tap, tab_num), *external_mem_desc);
}

void
ExynosVpuMemmapExternal::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuMemmap::displayObjectInfo(tab_num);
    ExynosVpuMemmapExternal::display_structure_info(tab_num, &m_memmap_info.index);
}

ExynosVpuMemmapExternal::ExynosVpuMemmapExternal(ExynosVpuTask *task, ExynosVpuIoMemory *memory)
                                                                                            :ExynosVpuMemmap(task, VPUL_MEM_EXTERNAL)
{
    if (memory != NULL) {
        memory->addMemmap(this);
    } else {
        VXLOGE("memory object should be assigned");
    }

    m_memory = memory;
}

ExynosVpuMemmapExternal::ExynosVpuMemmapExternal(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info)
                                                                                            :ExynosVpuMemmap(task, memmap_info)
{
    m_memory = NULL;
}

ExynosVpuIoMemory*
ExynosVpuMemmapExternal::getMemory(void)
{
    return m_memory;
}

status_t
ExynosVpuMemmapExternal::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuMemmap::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("update memmap structure fails");
        goto EXIT;
    }

    if (m_memory) {
        m_memmap_info.index = m_memory->getIndex();
    } else {
        VXLOGE("external memory is not assigned");
        ret = INVALID_OPERATION;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuMemmapExternal::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuMemmap::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("update memmap object fails");
        goto EXIT;
    }

    m_memory = m_task->getExternalMem(m_memmap_info.index);
    if (m_memory == NULL) {
        VXLOGE("getting external memory fails");
        ret = INVALID_OPERATION;
    }

EXIT:
    return ret;
}

void
ExynosVpuMemmapPreloadPu::display_structure_info(uint32_t tab_num, struct vpul_memory_map_desc *memmap_desc)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(--------) [preload pu] index:%d/%d/%d, 0x%x", MAKE_TAB(tap, tab_num), memmap_desc->pu_index.proc, memmap_desc->pu_index.sc, memmap_desc->pu_index.pu, memmap_desc->pu_index.ports_bit_map);
}

void
ExynosVpuMemmapPreloadPu::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuMemmap::displayObjectInfo(tab_num);
    ExynosVpuMemmapPreloadPu::display_structure_info(tab_num, &m_memmap_info);
}

ExynosVpuMemmapPreloadPu::ExynosVpuMemmapPreloadPu(ExynosVpuTask *task, ExynosVpuPu *pu)
                                                                                            :ExynosVpuMemmap(task, VPUL_MEM_PRELOAD_PU)
{
    if (pu != NULL) {
        pu->setMemmap(this);
    } else {
        VXLOGE("pu object should be assigned");
    }

    m_pu = pu;
}

ExynosVpuMemmapPreloadPu::ExynosVpuMemmapPreloadPu(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info)
                                                                                            :ExynosVpuMemmap(task, memmap_info)
{
    m_pu = NULL;
}

status_t
ExynosVpuMemmapPreloadPu::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuMemmap::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("update memmap structure fails");
        goto EXIT;
    }

    if (m_pu) {
        m_memmap_info.pu_index.pu = m_pu->getIndexInSubchain();
        m_memmap_info.pu_index.sc = m_pu->getSubchain()->getIndexInVertex();
        m_memmap_info.pu_index.proc = m_pu->getSubchain()->getVertex()->getIndex();
    } else {
        VXLOGE("external memory is not assigned");
        ret = INVALID_OPERATION;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuMemmapPreloadPu::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuMemmap::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("update memmap object fails");
        goto EXIT;
    }

    m_pu = m_task->getPu(m_memmap_info.pu_index.pu, m_memmap_info.pu_index.proc, m_memmap_info.pu_index.sc);
    if (m_pu == NULL) {
        VXLOGE("getting pu fails");
        ret = INVALID_OPERATION;
    }

EXIT:
    return ret;
}

void
ExynosVpuIoMemory::addMemmap(ExynosVpuMemmap *memmap)
{
    m_memmap_list.push_back(memmap);
}

void
ExynosVpuIoExternalMem::display_structure_info(uint32_t tab_num, void *base, uint32_t *external_mem_addr)
{
    char tap[MAX_TAB_NUM];

    uint32_t external_mem_index = getExternalMemIndexInTask((uint8_t*)base, (uint8_t*)external_mem_addr);

    VXLOGI("%s(extmem_%d) addr:0x%x", MAKE_TAB(tap, tab_num),
                                                    external_mem_index,
                                                    *external_mem_addr);
}

void
ExynosVpuIoExternalMem::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(extmem_%d) addr:0x%x, size:%d, io_flag:%d", MAKE_TAB(tap, tab_num),
                                                    m_external_mem_index,
                                                    m_external_mem_info,
                                                    m_allocated_size,
                                                    m_io_buffer_flag);
}

ExynosVpuIoExternalMem::ExynosVpuIoExternalMem(ExynosVpuTask *task)
{
    m_task = task;
    memset(&m_external_mem_info, 0x0, sizeof(m_external_mem_info));
    m_external_mem_index = task->addExternalMem(this);

    m_allocated = false;
    m_allocated_size = 0;

    m_io_buffer_flag = false;
}

ExynosVpuIoExternalMem::ExynosVpuIoExternalMem(ExynosVpuTask *task, const uint32_t *external_mem_info)
{
    m_task = task;
    m_external_mem_info = *external_mem_info;
    m_external_mem_index = task->addExternalMem(this);

    m_allocated = false;
    m_allocated_size = 0;

    m_io_buffer_flag = false;
}

uint32_t
ExynosVpuIoExternalMem::getRequiredBufferSize(void)
{
    uint32_t required_size = 0;

    struct vpul_memory_map_desc *memmap_info;
    List<ExynosVpuMemmap*>::iterator memmap_iter;
    for (memmap_iter=m_memmap_list.begin(); memmap_iter!=m_memmap_list.end(); memmap_iter++) {
        memmap_info = (*memmap_iter)->getMemmapInfo();
        uint32_t cur_size = memmap_info->image_sizes.width * memmap_info->image_sizes.height * memmap_info->image_sizes.pixel_bytes;
        if (required_size == 0) {
            required_size = cur_size;
        } else {
            if (required_size != cur_size) {
                VXLOGE("size is not matching");
                required_size = 0;
                displayObjectInfo();
                break;
            }
        }
    }

    if (required_size == 0) {
        VXLOGE("external buf size is not valid");
    }

    return required_size;
}

bool
ExynosVpuIoExternalMem::setBuffer(int32_t fd, uint32_t size)
{
    if (m_allocated) {
        VXLOGE("a buffer is already assigned");
        displayObjectInfo();
        return false;
    }

    m_external_mem_info = fd;

    m_allocated = true;
    m_allocated_size = size;

    List<ExynosVpuIoMemory*>::iterator memory_iter;
    for (memory_iter=m_alliance_memory_list.begin(); memory_iter!=m_alliance_memory_list.end(); memory_iter++) {
        if ((*memory_iter)->setBuffer(fd, size) == false) {
            VXLOGE("setting buffer to alliance memory object fails");
            return false;
        }
    }

    return true;
}

status_t
ExynosVpuIoExternalMem::updateStructure(void)
{
    status_t ret = NO_ERROR;

    return ret;
}

status_t
ExynosVpuIoExternalMem::updateObject(void)
{
    status_t ret = NO_ERROR;

    uint32_t memmap_num = m_task->getMemmapNumber();
    for (uint32_t i=0; i<memmap_num; i++) {
        ExynosVpuMemmap *memmap = m_task->getMemmap(i);
        if ((memmap->getType()==VPUL_MEM_EXTERNAL) && (memmap->getMemory() == this)) {
            addMemmap(memmap);
        }
    }

    return NO_ERROR;
}

void
ExynosVpuIoInternalRam::display_structure_info(uint32_t tab_num, struct vpul_internal_ram *ram)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(internal ram) first_mprb_group_idx:%d, n_mprb_groups:%d", MAKE_TAB(tap, tab_num), ram->first_mprb_group_idx, ram->n_mprb_groups);
}

void
ExynosVpuIoInternalRam::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuIoInternalRam::display_structure_info(tab_num, &m_ram_info);
}

ExynosVpuIoInternalRam::ExynosVpuIoInternalRam(ExynosVpuTask *task)
{
    m_task = task;
    memset(&m_ram_info, 0x0, sizeof(m_ram_info));

    m_ram_index_in_process = task->addInternalRam(this);
}

ExynosVpuIoInternalRam::ExynosVpuIoInternalRam(ExynosVpuTask *task, const struct vpul_internal_ram *ram_info)
{
    m_task = task;
    m_ram_info = *ram_info;

    m_ram_index_in_process = task->addInternalRam(this);
}

status_t
ExynosVpuIoInternalRam::updateStructure(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoInternalRam::updateObject(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoInternalRam::checkConstraint(void)
{
    return NO_ERROR;
}

}; /* namespace android */

