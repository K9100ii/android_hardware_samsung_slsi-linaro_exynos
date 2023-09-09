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

#define LOG_TAG "ExynosVpuVertex"
#include <cutils/log.h>

#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

static uint32_t getVertexIndexInTask(uint8_t *base, uint8_t *vertex)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = vertex - base;
    remained_byte -= task->vertices_vec_ofs;

    uint32_t i;
    for (i=0; i<task->t_num_of_vertices; i++) {
        if (remained_byte == 0) {
            break;
        }

        remained_byte -= sizeof(struct vpul_vertex);
    }

    if (i == task->t_num_of_vertices) {
        VXLOGE("can't find vertex index");
    }

    return i;
}

void
ExynosVpuVertex::display_structure_info(uint32_t tab_num, void *base, struct vpul_vertex *vertex)
{
    char tap[MAX_TAB_NUM];

    uint32_t vertex_index = getVertexIndexInTask((uint8_t*)base, (uint8_t*)vertex);

    VXLOGI("%s[VT_%d] vtype:%d", MAKE_TAB(tap, tab_num), vertex_index, vertex->vtype);
    VXLOGI("%s[VT_%d] n_out_edges:%d", MAKE_TAB(tap, tab_num), vertex_index, vertex->n_out_edges);
    VXLOGI("%s[VT_%d] num of subchain:%d, sc_ofs:%p", MAKE_TAB(tap, tab_num), vertex_index, vertex->num_of_subchains, vertex->sc_ofs);
    VXLOGI("%s[VT_%d] n_sc_first_iteration:%d, n_sc_last_iteration:%p", MAKE_TAB(tap, tab_num), vertex_index,
                                                                                        vertex->n_subchain_on_first_iteration,
                                                                                        vertex->n_subchain_on_last_iteration);

    switch(vertex->vtype) {
    case VPUL_VERTEXT_PROC:
        ExynosVpuProcess::display_structure_info(tab_num+1, &vertex->proc);
        break;
    default:
        break;
    }

    if (vertex->num_of_subchains) {
        struct vpul_subchain *subchain = fst_vtx_sc_ptr(base, vertex);
        for (uint32_t i=0; i<vertex->num_of_subchains; i++, subchain++) {
            ExynosVpuSubchain::display_structure_info(tab_num+1, base, subchain);
        }
    }
}

void
ExynosVpuVertex::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[VT_%d] vtype:%d", MAKE_TAB(tap, tab_num), m_vertex_index_in_task, m_vertex_info.vtype);
    VXLOGI("%s[VT_%d] n_out_edges:%d", MAKE_TAB(tap, tab_num), m_vertex_index_in_task, m_post_vertex_list.size());
    VXLOGI("%s[VT_%d] num of subchain:%d, sc_ofs:NULL", MAKE_TAB(tap, tab_num), m_vertex_index_in_task, m_subchain_list.size());
    VXLOGI("%s[VT_%d] n_sc_first_iteration:%d, n_sc_last_iteration:%p", MAKE_TAB(tap, tab_num), m_vertex_index_in_task,
                                                                                                m_vertex_info.n_subchain_on_first_iteration,
                                                                                                m_vertex_info.n_subchain_on_last_iteration);

    switch(m_vertex_info.vtype) {
    case VPUL_VERTEXT_PROC:
        displayProcessInfo(tab_num+1);
        break;
    default:
        break;
    }

    Vector<ExynosVpuSubchain*>::iterator subchain_iter;
    for (subchain_iter=m_subchain_list.begin(); subchain_iter!=m_subchain_list.end(); subchain_iter++) {
        (*subchain_iter)->displayObjectInfo(tab_num+1);
    }
}

bool
ExynosVpuVertex::connect(ExynosVpuVertex *prev_vertex, ExynosVpuVertex *post_vertex)
{
    if (prev_vertex->connectPostVertex(post_vertex) != true) {
        VXLOGE("can't add post vertex");
        return false;
    }

    if (post_vertex->connectPrevVertex(prev_vertex) != true) {
        VXLOGE("can't add prev vertex");
        return false;
    }

    return true;
}

ExynosVpuVertex::ExynosVpuVertex(ExynosVpuTask *task, enum vpu_task_vertex_type type)
{
    m_task = task;

    memset(&m_vertex_info, 0x0, sizeof(m_vertex_info));

    m_vertex_index_in_task = task->addVertex(this);
    m_vertex_info.vtype = type;
}

ExynosVpuVertex::ExynosVpuVertex(ExynosVpuTask *task, const struct vpul_vertex *vertex_info)
{
    m_task = task;

    m_vertex_info = *vertex_info;

    m_vertex_index_in_task = task->addVertex(this);
}

bool
ExynosVpuVertex::connectPostVertex(ExynosVpuVertex *post_vertex)
{
    Vector<ExynosVpuVertex*>::iterator vertex_iter;
    for (vertex_iter=m_post_vertex_list.begin(); vertex_iter!=m_post_vertex_list.end(); vertex_iter++) {
        if (*vertex_iter == post_vertex) {
            VXLOGE("VERTEX_%d, VERTEX_%s is already connected", this->m_vertex_index_in_task, post_vertex->getIndex());
            return false;
        }
    }

    m_post_vertex_list.push_back(post_vertex);

    return true;
}

bool
ExynosVpuVertex::connectPrevVertex(ExynosVpuVertex *prev_vertex)
{
    Vector<ExynosVpuVertex*>::iterator vertex_iter;
    for (vertex_iter=m_prev_vertex_list.begin(); vertex_iter!=m_prev_vertex_list.end(); vertex_iter++) {
        if (*vertex_iter == prev_vertex) {
            VXLOGE("VERTEX_%d, VERTEX_%s is already connected", this->m_vertex_index_in_task, prev_vertex->getIndex());
            return false;
        }
    }

    m_prev_vertex_list.push_back(prev_vertex);

    return true;
}

uint32_t
ExynosVpuVertex::addSubchain(ExynosVpuSubchain *subchain)
{
    m_subchain_list.push_back(subchain);

    return m_task->addSubchain(subchain);
}

ExynosVpuSubchain*
ExynosVpuVertex::getSubchain(uint32_t index)
{
    if (m_subchain_list.size() <= index) {
        VXLOGE("subchain_%d is out of bound(size:%d)", index, m_subchain_list.size());
        return NULL;
    }

    return m_subchain_list.editItemAt(index);
}

ExynosVpuTask*
ExynosVpuVertex::getTask(void)
{
    return m_task;
}

uint32_t
ExynosVpuVertex::getIndex(void)
{
    return m_vertex_index_in_task;
}

enum vpu_task_vertex_type
ExynosVpuVertex::getType(void)
{
    return m_vertex_info.vtype;
}

status_t
ExynosVpuVertex::updateVertexIo(uint32_t std_width, uint32_t std_height)
{
    if ((std_width==0) || (std_height==0)) {
        VXLOGE("size is wrong, %d x %d", std_width, std_height);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuVertex::spreadVertexIoInfo(void)
{
    status_t ret = NO_ERROR;

    Vector<ExynosVpuSubchain*>::iterator subchain_iter;
    for (subchain_iter=m_subchain_list.begin(); subchain_iter!=m_subchain_list.end(); subchain_iter++) {
        ret = (*subchain_iter)->spreadSubchainIoInfo();
        if (ret != NO_ERROR) {
            VXLOGE("spreading io info fails");
            break;
        }
    }

    return ret;
}

status_t
ExynosVpuVertex::checkConstraint(void)
{
    status_t ret = NO_ERROR;

    if (m_vertex_info.vtype == VPUL_VERTEXT_START) {
        if (m_vertex_info.n_out_edges != 1) {
            VXLOGE("start vertex should have single out edge");
            ret = BAD_VALUE;
            goto EXIT;
        }
    }

EXIT:

    return ret;
}

status_t
ExynosVpuVertex::updateStructure(void)
{
    status_t ret = NO_ERROR;

    m_vertex_info.n_out_edges = m_post_vertex_list.size();
    Vector<ExynosVpuVertex*>::iterator vertex_iter;
    struct vpul_edge *edge;
    int32_t i;
    for (vertex_iter=m_post_vertex_list.begin(), i=0; vertex_iter!=m_post_vertex_list.end(); vertex_iter++, i++) {
        struct vpul_edge *edge = &m_vertex_info.out_edges[i];
        edge->dst_vtx_idx = (*vertex_iter)->getIndex();
    }

    return ret;
}

status_t
ExynosVpuVertex::updateObject(void)
{
    struct vpul_edge *edge;
    uint32_t i;
    for (i=0; i<m_vertex_info.n_out_edges; i++) {
        edge = &m_vertex_info.out_edges[i];
        m_post_vertex_list.push_back(m_task->getVertex(edge->dst_vtx_idx));
    }

    return NO_ERROR;
}

status_t
ExynosVpuVertex::insertVertexToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    struct vpul_vertex *td_vertex;

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

    td_vertex = fst_vtx_ptr((struct vpul_task*)task_descriptor_base) + getIndex();

    memcpy(td_vertex, &m_vertex_info, sizeof(struct vpul_vertex));
    td_vertex->num_of_subchains = m_subchain_list.size();
    if (td_vertex->num_of_subchains)
        td_vertex->sc_ofs = (uint32_t)((uint8_t*)sc_ptr((struct vpul_task*)task_descriptor_base, m_subchain_list[0]->getIndexInTask()) - (uint8_t*)task_descriptor_base);

EXIT:
    return ret;
}

}; /* namespace android */
