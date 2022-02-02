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

#define LOG_TAG "ExynosVpuTaskIf"
#include <cutils/log.h>
#include <sys/stat.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "ExynosVpuKernel.h"

#include "ExynosVpuTaskIf.h"
#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"
#include "ExynosVpuCpuOp.h"

namespace android {

static uint32_t getTaskDescriptorSize(uint32_t vertex_num, uint32_t tdnn_proc_base_num, uint32_t subchain_num, uint32_t pu_num, uint32_t updatable_pu_num)
{
    uint32_t returnValue = 0;

    /* For each level we need to calculate the size of static structure times the number of instances */
    returnValue += sizeof(struct vpul_task);
    returnValue += vertex_num*sizeof(struct vpul_vertex);
    returnValue += tdnn_proc_base_num*sizeof(struct vpul_3dnn_process_base);
    returnValue += subchain_num*sizeof(struct vpul_subchain);
    returnValue += pu_num*sizeof(struct vpul_pu);
    returnValue += updatable_pu_num*sizeof(struct vpul_pu_location);

EXIT:
    return returnValue;
}

ExynosVpuTaskIf::ExynosVpuTaskIf(uint32_t task_index)
{
    m_task_index = task_index;

    m_task = NULL;
    m_vpu_node = new ExynosVpuDrvNode();

    m_input_format_bunch.direction = VS4L_DIRECTION_IN;
    m_output_format_bunch.direction = VS4L_DIRECTION_OT;

    m_input_container_bunch.direction = VS4L_DIRECTION_IN;
    m_output_container_bunch.direction = VS4L_DIRECTION_OT;

    m_task_descriptor = NULL;
    m_allocator = NULL;

    m_cnn_task_flag = false;
}

ExynosVpuTaskIf::~ExynosVpuTaskIf(void)
{
    if (m_task)
        delete m_task;
    if (m_vpu_node)
        delete m_vpu_node;

    if (m_inter_chain_buffer_list.size() && !m_allocator) {
        VXLOGE("allocator is not assigned");
    }

    List<struct io_buffer_info_t>::iterator buffer_iter;
    for (buffer_iter=m_inter_chain_buffer_list.begin(); buffer_iter!=m_inter_chain_buffer_list.end(); buffer_iter++) {
        m_allocator->free_mem(buffer_iter->size, &buffer_iter->fd, &buffer_iter->addr, buffer_iter->mapped);
    }

    if (m_allocator)
        delete m_allocator;

    if (m_task_descriptor) {
        free(m_task_descriptor);
        m_task_descriptor = NULL;
    }
}

status_t
ExynosVpuTaskIf::createMemoryAllocator(void)
{
    if (m_allocator) {
        VXLOGE("allocator is already created");
        return INVALID_OPERATION;
    }

    m_allocator = new ExynosVisionIonAllocator();

    int ret = NO_ERROR;
    ret = m_allocator->init(true);
    if (ret != NO_ERROR) {
        VXLOGE("ion allocator's init fail, ret:%d", ret);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

bool
ExynosVpuTaskIf::connect(ExynosVpuTaskIf *prev_task_if, uint32_t prev_io_out_port, ExynosVpuTaskIf *post_task_if, uint32_t post_io_in_port)
{
    if (prev_task_if->connectPostTaskIf(prev_io_out_port, post_task_if, post_io_in_port) == false) {
        VXLOGE("can't add post taskIf");
        return false;
    }

    if (post_task_if->connectPrevTaskIf(post_io_in_port, prev_task_if, prev_io_out_port) == false) {
        VXLOGE("can't add post taskIf");
        return false;
    }

    return true;
}

bool
ExynosVpuTaskIf::connectPostTaskIf(uint32_t prev_io_out_port, ExynosVpuTaskIf *post_task_if, uint32_t post_io_in_port)
{
    ExynosVpuPu *io_out_pu = getIoPu(VS4L_DIRECTION_OT, prev_io_out_port);
    ExynosVpuPu *io_in_pu = post_task_if->getIoPu(VS4L_DIRECTION_IN, post_io_in_port);

    if (!(io_out_pu && io_in_pu)) {
        VXLOGE("getting io pu fails");
        return false;
    }

    if (ExynosVpuMemmap::compareSize(io_out_pu->getMemmap(), io_in_pu->getMemmap()) == false) {
        VXLOGE("memmap is not matching, %p, %p", io_out_pu->getMemmap(), io_in_pu->getMemmap());
        VXLOGE("[prev pu]");
        io_out_pu->displayObjectInfo();
        if (io_out_pu->getMemmap())
            io_out_pu->getMemmap()->displayObjectInfo();
        VXLOGE("[post pu]");
        io_in_pu->displayObjectInfo();
        if (io_in_pu->getMemmap())
            io_in_pu->getMemmap()->displayObjectInfo();
        return false;
    }

    return true;
}

bool
ExynosVpuTaskIf::connectPrevTaskIf(uint32_t post_io_in_port, ExynosVpuTaskIf *prev_task_if, uint32_t prev_io_out_port)
{
    ExynosVpuPu *io_out_pu = prev_task_if->getIoPu(VS4L_DIRECTION_OT, prev_io_out_port);
    ExynosVpuPu *io_in_pu = getIoPu(VS4L_DIRECTION_IN, post_io_in_port);

    if (!(io_out_pu && io_in_pu)) {
        VXLOGE("getting io pu fails");
        return false;
    }

    if (ExynosVpuMemmap::compareSize(io_out_pu->getMemmap(), io_in_pu->getMemmap()) == false) {
        VXLOGE("memmap is not matching, %p, %p", io_out_pu->getMemmap(), io_in_pu->getMemmap());
        VXLOGE("[prev pu]");
        io_out_pu->displayObjectInfo();
        if (io_out_pu->getMemmap())
            io_out_pu->getMemmap()->displayObjectInfo();
        VXLOGE("[post pu]");
        io_in_pu->displayObjectInfo();
        if (io_in_pu->getMemmap())
            io_in_pu->getMemmap()->displayObjectInfo();
        return false;
    }

    return true;
}

status_t
ExynosVpuTaskIf::createStrFromObject(void)
{
    int ret = NO_ERROR;

    if (m_task_descriptor != NULL) {
        VXLOGE("task desc object already exist");
        return INVALID_OPERATION;
    }

    uint32_t  task_descriptor_size = getTaskDescriptorSize(m_task->getVertexNumber(),
                                                                                            m_task->getTotalTdnnProcBaseNumber(),
                                                                                            m_task->getTotalSubchainNumber(),
                                                                                            m_task->getTotalPuNumber(),
                                                                                            m_task->getTotalUpdatablePuNumber());

    /* additional struct to task descriptor for io interface */
    m_task_descriptor = malloc(task_descriptor_size + sizeof(struct io_graph_ext_t));
    m_task->setTotalSize(task_descriptor_size);

    struct io_graph_ext_t *graph_ext = (struct io_graph_ext_t*)(((uint8_t*)m_task_descriptor) + task_descriptor_size);
    graph_ext->vs4l_magic[0] = 'V';
    graph_ext->vs4l_magic[1] = 'S';
    graph_ext->vs4l_magic[2] = '4';
    graph_ext->vs4l_magic[3] = 'L';

    ret = updateStrFromObject();
    if (ret != NO_ERROR) {
        VXLOGE("updating str from object fails");
        displayObjectInfo();
 }

    return ret;
}

status_t
ExynosVpuTaskIf::updateStrFromObject(void)
{
    int ret = NO_ERROR;

    ret = m_task->insertTaskToTaskDescriptor(m_task_descriptor);

    return ret;
}

status_t
ExynosVpuTaskIf::createObjectFromStr(void)
{
    int ret = NO_ERROR;

    ExynosVpuPuFactory pu_factory;
    ExynosVpuCpuOpFactory cpu_op_factory;
    uint32_t i, j, k;

    struct vpul_task *task_str;
    struct vpul_vertex *vertex_str;
    struct vpul_subchain *subchain_str;
    struct vpul_pu *pu_str;

    ExynosVpuTask *task_object;
    ExynosVpuVertex *vertex_object;
    ExynosVpu3dnnProcessBase *tdnn_proc_base_object;
    ExynosVpuSubchainHw *subchain_hw_object;
    ExynosVpuSubchainCpu *subchain_cpu_object;
    ExynosVpuPu *pu_object;
    ExynosVpuCpuOp *cpu_op_object;
    ExynosVpuUpdatablePu *updatable_pu_object;

    task_str = (struct vpul_task*)m_task_descriptor;
    task_object = new ExynosVpuTask(this, task_str);

    vertex_str = fst_vtx_ptr((struct vpul_task*)m_task_descriptor);
    for (i=0; i < task_str->t_num_of_vertices; i++, vertex_str++) {
        switch(vertex_str->vtype) {
        case VPUL_VERTEXT_PROC:
            vertex_object = new ExynosVpuProcess(task_object, vertex_str);
            break;
        case VPUL_VERTEXT_3DNN_PROC:
            vertex_object = new ExynosVpu3dnnProcess(task_object, vertex_str);
            break;
        default:
            vertex_object = new ExynosVpuVertex(task_object, vertex_str);
            break;
        }

        subchain_str = fst_vtx_sc_ptr(m_task_descriptor, vertex_str);
        for (j=0; j <vertex_str->num_of_subchains; j++, subchain_str++) {
            switch (subchain_str->stype) {
            case VPUL_SUB_CH_HW:
                subchain_hw_object = new ExynosVpuSubchainHw(vertex_object, subchain_str);

                pu_str = fst_sc_pu_ptr(m_task_descriptor, subchain_str);
                for (k=0; k<subchain_str->num_of_pus; k++, pu_str++) {
                    pu_object = pu_factory.createPu(subchain_hw_object, pu_str);
                }
                break;
            case VPUL_SUB_CH_CPU_OP:
                subchain_cpu_object = new ExynosVpuSubchainCpu(vertex_object, subchain_str);

                for (k=0; k<subchain_str->num_of_cpu_op; k++) {
                    cpu_op_object = cpu_op_factory.createCpuOp(subchain_cpu_object, &subchain_str->cpu.cpu_op_desc[k]);
                }
                break;
            default:
                VXLOGE("unknwon stype: %d", subchain_str->stype);
                break;
            }
        }
    }

    struct vpul_3dnn_process_base *tdnn_proc_base;
    tdnn_proc_base = fst_3dnn_process_base_ptr((struct vpul_task*)m_task_descriptor);
    for (i=0; i < task_str->t_num_of_3dnn_process_bases; i++, tdnn_proc_base++) {
        tdnn_proc_base_object = new ExynosVpu3dnnProcessBase(task_object, tdnn_proc_base);
    }

    struct vpul_pu_location *updatable_pu;
    updatable_pu = fst_updateble_pu_location_ptr((struct vpul_task*)m_task_descriptor);
    for (i=0; i < task_str->t_num_of_pu_params_on_invoke; i++, updatable_pu++) {
        updatable_pu_object = new ExynosVpuUpdatablePu(task_object, updatable_pu);
    }

    /* update object from str */
    task_object->updateObject();

    for (i=0; i < task_object->getVertexNumber(); i++) {
        task_object->getVertex(i)->updateObject();
    }
    for (i=0; i < task_object->getTotalSubchainNumber(); i++) {
        task_object->getSubchain(i)->updateObject();
    }
    for (i=0; i < task_object->getTotalPuNumber(); i++) {
        task_object->getPu(i)->updateObject();
    }

    return ret;
}

status_t
ExynosVpuTaskIf::importTaskStr(const void *task_descriptor)
{
    int ret = NO_ERROR;

    if (m_task_descriptor != NULL) {
        VXLOGE("task desc object already exist");
        return INVALID_OPERATION;
    }

    uint32_t  task_descriptor_size = getTaskDescriptorSize(((struct vpul_task*)task_descriptor)->t_num_of_vertices,
                                                                                            ((struct vpul_task*)task_descriptor)->t_num_of_3dnn_process_bases,
                                                                                            ((struct vpul_task*)task_descriptor)->t_num_of_subchains,
                                                                                            ((struct vpul_task*)task_descriptor)->t_num_of_pus,
                                                                                            ((struct vpul_task*)task_descriptor)->t_num_of_pu_params_on_invoke);

    if (task_descriptor_size !=  ((struct vpul_task*)task_descriptor)->total_size) {
        VXLOGE("task descriptor size is not matching, it might be old version");
        return INVALID_OPERATION;
    }

    /* additional struct to task descriptor for io interface */
    m_task_descriptor = malloc(task_descriptor_size + sizeof(struct io_graph_ext_t));
    memcpy(m_task_descriptor, task_descriptor, task_descriptor_size);

    struct io_graph_ext_t *graph_ext = (struct io_graph_ext_t*)(((uint8_t*)m_task_descriptor) + task_descriptor_size);
    graph_ext->vs4l_magic[0] = 'V';
    graph_ext->vs4l_magic[1] = 'S';
    graph_ext->vs4l_magic[2] = '4';
    graph_ext->vs4l_magic[3] = 'L';

    ret = createObjectFromStr();
    if (ret != NO_ERROR) {
        VXLOGE("task object doesn't created");
    }

    return ret;
}

status_t
ExynosVpuTaskIf::allocInterSubchainBuf(void)
{
    int ret = NO_ERROR;
    vx_uint32 i;

    /* allocation intermediate buffer in task */
    vx_uint32 external_mem_num = m_task->getExternalMemNumber();
    ExynosVpuIoExternalMem *external_mem;
    for (vx_uint32 i=0; i<external_mem_num; i++) {
        external_mem = m_task->getExternalMem(i);
        if (external_mem->isIoBuffer()) {
            continue;
        }

        if (external_mem->isAllocated()) {
            continue;
        }

        if (external_mem->getMemmapNumber() == 0) {
            VXLOGE("no body connects with external memory");
            external_mem->displayObjectInfo();
            continue;
        }

        if (m_allocator == NULL) {
            ret = createMemoryAllocator();
            if (ret != NO_ERROR) {
                VXLOGE("creating memory allocator fails, ret:%d", ret);
                goto EXIT;
            }
        }

        vx_uint32 memory_size = external_mem->getRequiredBufferSize();
        if (memory_size == 0) {
            VXLOGE("allcoating external buffer fails");
            external_mem->displayObjectInfo();
            ret = INVALID_OPERATION;
            goto EXIT;
        }

        io_buffer_info_t intermediate_buffer;
        intermediate_buffer.size = memory_size;
        intermediate_buffer.mapped = false;
        ret = m_allocator->alloc_mem(intermediate_buffer.size, &intermediate_buffer.fd, &intermediate_buffer.addr, intermediate_buffer.mapped);
        if (ret != NO_ERROR) {
            VXLOGE("allocating buffer fails, ret:%d", ret);
            goto EXIT;
        }

        external_mem->setBuffer(intermediate_buffer.fd, intermediate_buffer.size);

        m_inter_chain_buffer_list.push_back(intermediate_buffer);
    }

EXIT:
    return ret;
}

status_t
ExynosVpuTaskIf::setInterPair(ExynosVpuPu *output_pu, ExynosVpuPu *input_pu)
{
    return ExynosVpuProcPuDma::connectIntermediate(output_pu, input_pu);
}

status_t
ExynosVpuTaskIf::setInterPair(uint32_t output_pu_index, uint32_t input_pu_index)
{
    ExynosVpuPu *output_pu = m_task->getPu(output_pu_index);
    ExynosVpuPu *input_pu = m_task->getPu(input_pu_index);
    if ((output_pu == NULL) || (input_pu == NULL)) {
        VXLOGE("getting pu object fails");
        return INVALID_OPERATION;
    }

    return setInterPair(output_pu, input_pu);
}

status_t
ExynosVpuTaskIf::setIoPu(int32_t io_direction, uint32_t io_index, ExynosVpuPu *pu)
{
    if (pu == NULL) {
        VXLOGE("pu is null");
        return INVALID_OPERATION;
    }

    Vector<io_info_t> *io_info_list;
    if (io_direction == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    vx_uint32 io_num = io_info_list->size();
    if (io_index <= io_num) {
        io_info_t io_info;
        memset(&io_info, 0x0, sizeof(io_info));
        io_info_list->insertAt(io_info, io_num, io_index-io_num+1);
    }

    if (io_info_list->itemAt(io_index).pu == NULL) {
        io_info_list->editItemAt(io_index).pu = pu;
    } else {
        VXLOGE("other pu is already assigned at dir:%d, index:%d", io_direction, io_index);
        VXLOGE("[assigned pu]");
        io_info_list->itemAt(io_index).pu->displayObjectInfo();
        VXLOGE("[assigning pu]");
        pu->displayObjectInfo();
        return INVALID_OPERATION;
    }

    if (pu->getOpcode() == VPUL_OP_DMA) {
        status_t ret;
        List<ExynosVpuMemmap*> memmap_list;
        ret = pu->getMemmap(&memmap_list);
        if (ret != NO_ERROR) {
            VXLOGE("getting memmap fails");
            return ret;
        }

        List<ExynosVpuMemmap*>::iterator iter_memmap;
        for (iter_memmap=memmap_list.begin(); iter_memmap!=memmap_list.end(); iter_memmap++) {
            ret = (*iter_memmap)->getMemory()->setIoBuffer();
            if (ret != NO_ERROR) {
                VXLOGE("setting external mem to io buffer fails");
                return ret;
            }
        }
    }

    return NO_ERROR;
}

status_t
ExynosVpuTaskIf::setIoPu(int32_t io_direction, uint32_t io_index, uint32_t pu_index)
{
    ExynosVpuPu *pu = m_task->getPu(pu_index);
    if (pu == NULL) {
        VXLOGE("getting pu object fails");
        return INVALID_OPERATION;
    }

    return setIoPu(io_direction, io_index, pu);
}

status_t
ExynosVpuTaskIf::setIoMemmap(int32_t io_direction, uint32_t io_index, uint32_t memmap_index)
{
    ExynosVpuMemmap *memmap = m_task->getMemmap(memmap_index);
    if (memmap == NULL) {
        VXLOGE("getting memmap object fails");
        return INVALID_OPERATION;
    }

    Vector<io_info_t> *io_info_list;
    if (io_direction == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    vx_uint32 io_num = io_info_list->size();
    if (io_index <= io_num) {
        io_info_t io_info;
        memset(&io_info, 0x0, sizeof(io_info));
        io_info_list->insertAt(io_info, io_num, io_index-io_num+1);
    }

    if (io_info_list->itemAt(io_index).pu == NULL) {
        io_info_list->editItemAt(io_index).memmap = memmap;
    } else {
        VXLOGE("other memmap is already assigned at dir:%d, index:%d", io_direction, io_index);
        VXLOGE("[assigned memmap]");
        io_info_list->itemAt(io_index).memmap->displayObjectInfo();
        VXLOGE("[assigning memmap]");
        memmap->displayObjectInfo();
        return INVALID_OPERATION;
    }

    status_t ret;
    ret = memmap->getMemory()->setIoBuffer();
    if (ret != NO_ERROR) {
        VXLOGE("setting external mem to io buffer fails");
        return ret;
    }

    return NO_ERROR;
}

ExynosVpuPu*
ExynosVpuTaskIf::getIoPu(uint32_t dir, uint32_t index)
{
    Vector<io_info_t> *io_info_list;
    if (dir == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    if (io_info_list->size() <= index) {
        VXLOGE("io index is out of bound, dir:%d, index:%d, size:%d", dir, index, io_info_list->size());
    }

    return io_info_list->itemAt(index).pu;
}

uint32_t
ExynosVpuTaskIf::getIoNum(int32_t dir)
{
    Vector<io_info_t> *io_info_list;
    if (dir == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    return io_info_list->size();
}

status_t
ExynosVpuTaskIf::updateTaskIo(uint32_t direction, uint32_t index, List<struct io_format_t> *io_format_list)
{
    status_t ret = NO_ERROR;

    Vector<io_info_t> *io_info_list;
    if (direction == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    if (io_info_list->size() <= index) {
        VXLOGE("out of index, size:%d, index:%d", io_info_list->size(), index);
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    ExynosVpuPu *pu;
    pu = io_info_list->editItemAt(index).pu;
    ret = pu->updateIoInfo(io_format_list);
    if (ret != NO_ERROR) {
        VXLOGE("updating io fails, dir:%d, index:%d, pu_%d", direction, index, pu->getIndexInTask());
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuTaskIf::spreadTaskIoInfo(void)
{
    return m_task->spreadTaskIoInfo();
}

bool
ExynosVpuTaskIf::setIoInfo(uint32_t direction, uint32_t index, struct io_format_t *io_format, struct io_memory_t *io_memory)
{
    bool ret = true;

    Vector <struct io_format_t> *io_format_vector;
    Vector<io_containter_t> *io_container_vector;

    /* init format and io vector */
    if (direction == VS4L_DIRECTION_IN) {
        io_format_vector = &m_input_format_bunch.format_vector;
        io_container_vector = &m_input_container_bunch.container_vector;
    } else {
        io_format_vector = &m_output_format_bunch.format_vector;
        io_container_vector = &m_output_container_bunch.container_vector;
    }

    if (io_format_vector->size() < (index+1)) {
        io_format_vector->insertAt(io_format_vector->size(), (index+1)-io_format_vector->size());
    }
    io_format_vector->editItemAt(index) = *io_format;

    if (io_container_vector->size() < (index+1)) {
        io_container_vector->insertAt(io_container_vector->size(), (index+1)-io_container_vector->size());
    }
    io_container_vector->editItemAt(index).type = io_memory->type;
    io_container_vector->editItemAt(index).target = io_format->target;
    io_container_vector->editItemAt(index).memory = io_memory->memory;

    Vector<io_buffer_t> *buffer_vector = &io_container_vector->editItemAt(index).buffer_vector;
    buffer_vector->clear();
    buffer_vector->insertAt(0, io_memory->count);

EXIT:

    return ret;
}

uint32_t
ExynosVpuTaskIf::getTargetId(uint32_t io_direction, uint32_t io_index)
{
    Vector<io_info_t> *io_info_list;
    if (io_direction == VS4L_DIRECTION_IN)
        io_info_list = &m_in_io_info_list;
    else
        io_info_list = &m_out_io_info_list;

    if (io_info_list->size() <= io_index) {
        VXLOGE("io_index is out of bound, dir:%d, size:%d, io_index:%d", io_direction, io_info_list->size(), io_index);
        return 0;
    }

    uint32_t target_id = 0;

    if (m_cnn_task_flag) {
        ExynosVpuMemmap *memmap = io_info_list->itemAt(io_index).memmap;
        target_id = MEMMAP_TARGET_ID(m_task->getId(), memmap->getIndex());
    } else {
        ExynosVpuPu *pu = io_info_list->itemAt(io_index).pu;
        ExynosVpuSubchain *subchain = pu->getSubchain();
        target_id = PU_TARGET_ID(subchain->getId(), pu->getInstance());
    }

    return target_id;
}

void
ExynosVpuTaskIf::display_td_info(void *task_descriptor_base)
{
    struct vpul_task *task = (struct vpul_task*)task_descriptor_base;
    ExynosVpuTask::display_structure_info(0, task);
}

void
ExynosVpuTaskIf::print_td(void *task_descriptor_base)
{
    if (!task_descriptor_base) {
        VXLOGE("task descriptor is not allcoated");
        return;
    }

    struct vpul_task *task_descriptor = (struct vpul_task*)task_descriptor_base;

    uint32_t td_size = task_descriptor->total_size;

    uint16_t *ptr = (uint16_t*)task_descriptor;
    uint32_t cnt = (td_size + 1) / 2;

    uint32_t i;
    for (i=0; i < cnt; ptr++, i++) {
        if (i !=0 && ((i%16) == 0)) {
            printf("\\\n");
        }
        printf("0x%04x, ", *ptr);
    }

    printf("}\n");
}

void
ExynosVpuTaskIf::fwrite_td(char *task_name, uint32_t task_index, void *task_descriptor_base)
{
    if (!task_descriptor_base) {
        VXLOGE("task descriptor is not allcoated");
        return;
    }

    /* file create */
    FILE* fp = NULL;
    vx_char index_no[32];
    sprintf(index_no, "_%d", task_index);

    vx_char file_name[64] = "./test-ds/test-ds-";
    strcat(file_name, task_name);
    if (task_index != 0)
        strcat(file_name, index_no);
    strcat(file_name, ".h");

    fp = fopen(file_name, "w+t");
    if (fp == NULL) {
        mkdir("./test-ds/", 0777);
        fp = fopen(file_name, "w+t");
    }

    /* write define */
    vx_char upper_name[64];
    strcpy(upper_name, task_name);
    for (vx_char *iter = upper_name; *iter != '\0'; ++iter) {
        *iter = toupper(*iter);
    }
    fprintf(fp, "#define TASK_%s_DS {\\\n", upper_name);

    /* write td */
    struct vpul_task *task_descriptor = (struct vpul_task*)task_descriptor_base;

    uint32_t td_size = task_descriptor->total_size;

    uint16_t *ptr = (uint16_t*)task_descriptor;
    uint32_t cnt = (td_size + 1) / 2;

    uint32_t i;
    for (i=0; i < cnt; ptr++, i++) {
        if (i !=0 && ((i%16) == 0)) {
            fprintf(fp, "\\\n");
        }
        fprintf(fp, "0x%04x, ", *ptr);
    }

    fprintf(fp, "}\n");

    fclose(fp);
}

void
ExynosVpuTaskIf::driverNodeSetContainerInfo(void)
{
    m_vpu_node->setContainerInfo(&m_input_container_bunch);
    m_vpu_node->setContainerInfo(&m_output_container_bunch);
}

status_t
ExynosVpuTaskIf::driverNodeOpen(void)
{
    status_t ret;

    ret = m_vpu_node->open();

    if (ret != NO_ERROR) {
        VXLOGE("can't open node");
    }

    return  ret;
}

void
ExynosVpuTaskIf::displayObjectInfo(uint32_t tab_num)
{
    m_task->displayObjectInfo(tab_num);
}

status_t
ExynosVpuTaskIf::displayTdInfo(void)
{
    if (m_task_descriptor == NULL) {
        VXLOGE("task descriptor is not created yet");
        return NO_INIT;
    }

    ExynosVpuTaskIf::display_td_info(m_task_descriptor);

    return NO_ERROR;
}

uint32_t
ExynosVpuTaskIf::addTask(ExynosVpuTask *task)
{
    if (m_vpu_node == NULL) {
        VXLOGE("the node of taskif is not initialized yet");
        return 0;
    }

    if (m_task) {
        VXLOGE("task is already assigned");
    }
    m_task = task;

    return m_vpu_node->getNodeFd();
}

ExynosVpuTask*
ExynosVpuTaskIf::getTask(void)
{
    return m_task;
}

struct timeval*
ExynosVpuTaskIf::getVpuDrvTimeStamp(uint32_t dir)
{
    if (dir == VS4L_DIRECTION_IN) {
        return m_input_container_bunch.timestamp;
    } else {
        return m_output_container_bunch.timestamp;
    }
}

status_t
ExynosVpuTaskIf::driverNodeSetGraph(void)
{
    struct vs4l_graph graph;
    memset(&graph, 0x0, sizeof(graph));

    graph.id = ((struct vpul_task*)m_task_descriptor)->id;
    graph.priority = ((struct vpul_task*)m_task_descriptor)->priority;

    if (m_cnn_task_flag) {
        graph.flags = (1 << VS4L_GRAPH_FLAG_EXCLUSIVE) |
                (1 << VS4L_GRAPH_FLAG_SHARED_AMONG_SUBCHAINS) |
                (1 << VS4L_GRAPH_FLAG_DSBL_LATENCY_BALANCING) |
                (1 << VS4L_STATIC_ALLOC_LARGE_MPRB_INSTEAD_SMALL_FLAG) |
                (1 << VS4L_GRAPH_FLAG_PRIMITIVE) |
                (VPU_PU_SLF50 << VS4L_STATIC_ALLOC_PU_INSTANCE_LSB);
    } else {
        graph.flags = (1 << VS4L_GRAPH_FLAG_EXCLUSIVE) |
    		(1 << VS4L_GRAPH_FLAG_SHARED_AMONG_SUBCHAINS) |
    		(1 << VS4L_GRAPH_FLAG_DSBL_LATENCY_BALANCING);
    }

    graph.size = ((struct vpul_task*)m_task_descriptor)->total_size + sizeof(struct io_graph_ext_t);
    graph.addr = (unsigned long)m_task_descriptor;

#if 0
    /* JUN_DBG */
    displayTdInfo();
    ExynosVpuTaskIf::fwrite_td(m_task_name, m_task_index, m_task_descriptor);
#endif

    return m_vpu_node->setGraph(&graph);
}

status_t
ExynosVpuTaskIf::driverNodeSetFormat(void)
{
    status_t ret;

    VXLOGD2("m_input_format_bunch, dir:%d, count:%d", m_input_format_bunch.direction, m_input_format_bunch.format_vector.size());
    ret = m_vpu_node->setFormat(&m_input_format_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("input formating fails, ret:%d", ret);
        goto EXIT;
    }

    VXLOGD2("m_output_format_bunch, dir:%d, count:%d", m_output_format_bunch.direction, m_output_format_bunch.format_vector.size());
    ret = m_vpu_node->setFormat(&m_output_format_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("output formating fails, ret:%d", ret);
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuTaskIf::driverNodeSetParam(uint32_t target, void *addr, uint32_t size)
{
    if (m_task_descriptor == NULL) {
        VXLOGE("task descriptor is not allocatd yet");
        return INVALID_OPERATION;
    }

    return m_vpu_node->setParam(target, addr, 0, size);
}

status_t
ExynosVpuTaskIf::driverNodeStreamOn(void)
{
    return m_vpu_node->streamOn();
}

status_t
ExynosVpuTaskIf::driverNodeStreamOff(void)
{
    return m_vpu_node->streamOff();
}

status_t
ExynosVpuTaskIf::driverNodeClose(void)
{
    return m_vpu_node->close();
}

status_t
ExynosVpuTaskIf::driverNodePutBuffer(uint32_t frame_id,
                                                                    uint32_t in_index, Vector<Vector<io_buffer_t>> *in_buffer_vector_bunch,
                                                                    uint32_t out_index, Vector<Vector<io_buffer_t>> *out_buffer_vector_bunch)
{
    status_t ret;

    /* input container setting */
    m_input_container_bunch.id = frame_id;
    m_input_container_bunch.index = in_index;

    if (m_input_container_bunch.container_vector.size() != in_buffer_vector_bunch->size()) {
        VXLOGE("container vector size is not matching");
        return INVALID_OPERATION;
    }

    for (uint32_t i=0; i<m_input_container_bunch.container_vector.size(); i++) {
        m_input_container_bunch.container_vector.editItemAt(i).buffer_vector = in_buffer_vector_bunch->itemAt(i);
    }

    /* output container setting */
    m_output_container_bunch.id = frame_id;
    m_output_container_bunch.index = out_index;

    if (m_output_container_bunch.container_vector.size() != out_buffer_vector_bunch->size()) {
        VXLOGE("container vector size is not matching");
        return INVALID_OPERATION;
    }

    for (uint32_t i=0; i<m_output_container_bunch.container_vector.size(); i++) {
        m_output_container_bunch.container_vector.editItemAt(i).buffer_vector = out_buffer_vector_bunch->itemAt(i);

        VXLOGD3("[%d][OT] out_buffer_vector_bunch, buffer_vector[0]'s fd: 0x%x", i, (*out_buffer_vector_bunch)[i][0].m.fd);
        VXLOGD3("[%d][OT] m_output_container_bunch, buffer_vector[0]'s fd: 0x%x", i, m_output_container_bunch.container_vector[i].buffer_vector[0].m.fd);
    }

    ret = m_vpu_node->qbuf(&m_input_container_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("input qbuf fails, ret:%d", ret);
        goto EXIT;
    }

    ret = m_vpu_node->qbuf(&m_output_container_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("input qbuf fails, ret:%d", ret);
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuTaskIf::driverNodeGetBuffer(uint32_t *frame_id, uint32_t *in_index, uint32_t *out_index)
{
    status_t ret = NO_ERROR;
    uint32_t in_frame_id, out_frame_id;

    m_input_container_bunch.id = 0;
    m_input_container_bunch.index = 0xFFFFFFFF;
    ret = m_vpu_node->dqbuf(&m_input_container_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("input qbuf fails, ret:%d", ret);
        goto EXIT;
    }

    in_frame_id = m_input_container_bunch.id;
    *in_index = m_input_container_bunch.index;

    m_output_container_bunch.id = 0;
    m_output_container_bunch.index = 0xFFFFFFFF;
    ret = m_vpu_node->dqbuf(&m_output_container_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("input qbuf fails, ret:%d", ret);
        goto EXIT;
    }
    out_frame_id = m_output_container_bunch.id;
    *out_index = m_output_container_bunch.index;

    if (in_frame_id != out_frame_id) {
        VXLOGE("in/out frame id is not matching, in:%d, out:%d", in_frame_id, out_frame_id);
        ret = INVALID_OPERATION;
    } else {
        *frame_id = in_frame_id;
    }

EXIT:
    return ret;
}

}; /* namespace android */

