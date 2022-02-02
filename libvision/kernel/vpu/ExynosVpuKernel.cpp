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

#define LOG_TAG "ExynosVpuKernel"
#include <cutils/log.h>
#include <ctype.h>
#include <string.h>

#include "ExynosVisionPerfMonitor.h"
#include "ExynosVisionAutoTimer.h"

#include "vpu_kernel_util.h"

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"
#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

#define DEFAULT_GRAPH_PRIORITY  10
#define SERIALIZE_VPU_KERNEL    1

#if (SERIALIZE_VPU_KERNEL==1)
Mutex m_global_vpu_kernel_lock;
#endif

ExynosVpuTaskIfWrapper::ExynosVpuTaskIfWrapper(ExynosVpuKernel *kernel, uint32_t task_index)
{
    m_task_if = new  ExynosVpuTaskIf(task_index);

    m_kernel = kernel;

    if (m_kernel->getGraphReConstPolicy() == vx_false_e) {
        status_t ret;
        ret = m_task_if->driverNodeOpen();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to graph, ret:%d", ret);
        }
    }
}

ExynosVpuTaskIfWrapper::~ExynosVpuTaskIfWrapper()
{
    if (m_task_if) {
        if (m_kernel->getGraphReStreamPolicy() == vx_false_e) {
            m_task_if->driverNodeStreamOff();
        }
        if (m_kernel->getGraphReConstPolicy() == vx_false_e) {
            m_task_if->driverNodeClose();
        }
        delete m_task_if;
    } else {
        VXLOGE("task_if is null");
    }
}

vx_status
ExynosVpuTaskIfWrapper::setIoVxParam(vx_enum io_dir, vx_uint32 io_index, vx_uint32 param_index, vx_param_info_t param_info)
{
    Vector<io_param_info_t> *io_param_info_list;

    if (io_dir == VS4L_DIRECTION_IN)
        io_param_info_list = &m_in_io_param_info;
    else
        io_param_info_list = &m_out_io_param_info;

    if (m_task_if->getIoNum(io_dir) <= io_index) {
        VXLOGE("out of bound, dir:%d, index:%d, size:%d", io_dir, io_index, m_task_if->getIoNum(io_dir));
        return VX_FAILURE;
    }

    vx_uint32 io_num = io_param_info_list->size();
    if (io_index <= io_num) {
        io_param_info_t io_param_info;
        memset(&io_param_info, 0x0, sizeof(io_param_info));
        io_param_info_list->insertAt(io_param_info, io_num, io_index-io_num+1);
    }

    io_param_info_t *io_param_info = &(io_param_info_list->editItemAt(io_index));
    io_param_info->type = IO_PARAM_VX;
    io_param_info->vx_param.index = param_index;
    io_param_info->vx_param.info = param_info;

    return VX_SUCCESS;
}

vx_status
ExynosVpuTaskIfWrapper::setIoCustomParam(vx_enum io_dir, vx_uint32 io_index, struct io_format_t *io_format, struct io_memory_t *io_memory, io_buffer_info_t *io_buffer_info)
{
    Vector<io_param_info_t> *io_param_info_list;

    if (io_dir == VS4L_DIRECTION_IN)
        io_param_info_list = &m_in_io_param_info;
    else
        io_param_info_list = &m_out_io_param_info;

    if (m_task_if->getIoNum(io_dir) <= io_index) {
        VXLOGE("out of bound, dir:%d, index:%d, size:%d", io_dir, io_index, m_task_if->getIoNum(io_dir));
        return VX_FAILURE;
    }

    vx_uint32 io_num = io_param_info_list->size();
    if (io_index <= io_num) {
        io_param_info_t io_param_info;
        memset(&io_param_info, 0x0, sizeof(io_param_info));
        io_param_info_list->insertAt(io_param_info, io_num, io_index-io_num+1);
    }

    io_param_info_t *io_param_info = &(io_param_info_list->editItemAt(io_index));
    io_param_info->type = IO_PARAM_CUSTOM;
    io_param_info->custom_param.io_format = *io_format;
    io_param_info->custom_param.io_memory = *io_memory;
    for (vx_uint32 i=0; i<io_memory->count; i++) {
        io_param_info->custom_param.io_buffer_info[i] = io_buffer_info[i];
    }

    return VX_SUCCESS;
}

vx_uint32
ExynosVpuTaskIfWrapper::findIoIndex(vx_enum dir, const vx_reference parameters[])
{
    Vector<void*> new_param_set;

    List<Vector<void*>>    *io_set_list;
    Vector<io_param_info_t> *io_param_info_list;

    if (dir == VS4L_DIRECTION_IN) {
        io_set_list = &m_input_set_list;
        io_param_info_list = &m_in_io_param_info;
    } else {
        io_set_list = &m_output_set_list;
        io_param_info_list = &m_out_io_param_info;
    }

    Vector<io_param_info_t>::iterator param_iter;
    for (param_iter=io_param_info_list->begin(); param_iter!=io_param_info_list->end(); param_iter++) {
        if ((*param_iter).type == IO_PARAM_VX) {
            new_param_set.push_back(parameters[(*param_iter).vx_param.index]);
        } else {
            new_param_set.push_back(NULL);
        }
    }

    uint32_t index = 0;
    uint32_t i;
    List<Vector<void*>>::iterator set_iter;
    for (set_iter=io_set_list->begin(), index=0; set_iter!=io_set_list->end(); set_iter++, index++) {
        Vector<void*> *param_set = &(*set_iter);
        if (param_set->size() != new_param_set.size())
            continue;

        for (i=0; i<param_set->size(); i++)
            if (param_set->itemAt(i) != new_param_set.itemAt(i))
                break;

        if (i == param_set->size()) {
            /* find exist set */
            goto EXIT;
        }
    }

    if (set_iter == io_set_list->end()) {
        io_set_list->push_back(new_param_set);
    } else {
        VXLOGE("broken searching algorithm");
    }

EXIT:
    return index;
}

status_t
ExynosVpuTaskIfWrapper::initBufferVectorBunch(vx_enum io_direction, vx_uint32 io_index, List<struct io_format_t> *io_format_list)
{
    status_t ret = NO_ERROR;

    io_buffer_t io_buffer;
    memset(&io_buffer, 0x0, sizeof(io_buffer));

    Vector<io_buffer_t> buffer_vector;
    buffer_vector.insertAt(io_buffer, 0, io_format_list->size());

    Vector<io_buffer_t>::iterator buffer_iter;
    List<struct io_format_t>::iterator format_iter;
    for (buffer_iter=buffer_vector.begin(), format_iter=io_format_list->begin();
            buffer_iter!=buffer_vector.end();
            buffer_iter++, format_iter++) {
        buffer_iter->roi.x = 0;
        buffer_iter->roi.y = 0;
        buffer_iter->roi.w = format_iter->width;
        buffer_iter->roi.h = format_iter->height;
    }

    /* init io vector */
    Vector<Vector<io_buffer_t>> *buffer_vector_bunch;
    if (io_direction == VS4L_DIRECTION_IN)
        buffer_vector_bunch = &m_in_buffer_vector_bunch;
    else
        buffer_vector_bunch = &m_out_buffer_vector_bunch;

    if (buffer_vector_bunch->size() < (io_index+1)) {
        buffer_vector_bunch->insertAt(buffer_vector_bunch->size(), (io_index+1)-buffer_vector_bunch->size());
    }

    buffer_vector_bunch->editItemAt(io_index) = buffer_vector;

    return NO_ERROR;
}

vx_status
ExynosVpuTaskIfWrapper::updateTaskIoFromVX(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    Vector<io_param_info_t>::iterator param_iter;
    vx_uint32 io_index;
    List<struct io_format_t> io_format_list;
    struct io_memory_t io_memory;

    for (param_iter=m_in_io_param_info.begin(), io_index=0; param_iter!=m_in_io_param_info.end(); param_iter++, io_index++) {
        io_format_list.clear();
        memset(&io_memory, 0x0, sizeof(io_memory));
        if (param_iter->type == IO_PARAM_VX) {
            status = getIoInfoFromVx(parameters[param_iter->vx_param.index], param_iter->vx_param.info, &io_format_list, &io_memory);
            if (status != VX_SUCCESS) {
                VXLOGE("getting io info fails");
                break;
            }
        } else {
            struct io_format_t io_format;
            io_format = param_iter->custom_param.io_format;
            io_format_list.push_back(io_format);
        }

        if (m_task_if->isCnnTask())
            continue;

        ret = m_task_if->updateTaskIo(VS4L_DIRECTION_IN, io_index, &io_format_list);
        if (ret != NO_ERROR) {
            VXLOGE("updating task io fails, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    for (param_iter=m_out_io_param_info.begin(), io_index=0; param_iter!=m_out_io_param_info.end(); param_iter++, io_index++) {
        io_format_list.clear();
        memset(&io_memory, 0x0, sizeof(io_memory));
        if (param_iter->type == IO_PARAM_VX) {
            status = getIoInfoFromVx(parameters[param_iter->vx_param.index], param_iter->vx_param.info, &io_format_list, &io_memory);
            if (status != VX_SUCCESS) {
                VXLOGE("getting io info fails");
                break;
            }
        } else {
            struct io_format_t io_format;
            io_format = param_iter->custom_param.io_format;
            io_format_list.push_back(io_format);
        }

        if (m_task_if->isCnnTask())
            continue;

        ret = m_task_if->updateTaskIo(VS4L_DIRECTION_OT, io_index, &io_format_list);
        if (ret != NO_ERROR) {
            VXLOGE("updating task io fails, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    ret = m_task_if->spreadTaskIoInfo();
    if (ret != NO_ERROR) {
        VXLOGE("spreading task io info fails, ret:%d", ret);
        m_task_if->displayObjectInfo();
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapper::finalizeTaskif(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    Vector<io_param_info_t>::iterator param_iter;
    vx_uint32 io_index;
    List<struct io_format_t> io_format_list;
    List<struct io_format_t>::iterator format_iter;
    struct io_memory_t io_memory;
    struct io_format_t io_format_rep;

    for (param_iter=m_in_io_param_info.begin(), io_index=0; param_iter!=m_in_io_param_info.end(); param_iter++, io_index++) {
        io_format_list.clear();
        memset(&io_memory, 0x0, sizeof(io_memory));
        if (param_iter->type == IO_PARAM_VX) {
            status = getIoInfoFromVx(parameters[param_iter->vx_param.index], param_iter->vx_param.info, &io_format_list, &io_memory);
            if (status != VX_SUCCESS) {
                VXLOGE("getting io info fails");
                break;
            }
        } else {
            struct io_format_t io_format;
            io_format = param_iter->custom_param.io_format;
            io_memory = param_iter->custom_param.io_memory;
            for (vx_uint32 i=0; i<io_memory.count; i++)
                io_format_list.push_back(io_format);
        }

        /* the biggest one is a representative */
        memset(&io_format_rep, 0x0, sizeof(io_format_rep));
        for (format_iter=io_format_list.begin(); format_iter!=io_format_list.end(); format_iter++) {
            if ((format_iter->width * format_iter->height * format_iter->pixel_byte) >
                (io_format_rep.width * io_format_rep.height * io_format_rep.pixel_byte))
                io_format_rep = *format_iter;
        }
        io_format_rep.target = m_task_if->getTargetId(VS4L_DIRECTION_IN, io_index);

        if (m_task_if->setIoInfo(VS4L_DIRECTION_IN, io_index,  &io_format_rep, &io_memory) != true) {
            VXLOGE("setting io info fails, err:%d", status);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            break;
        }

        ret = initBufferVectorBunch(VS4L_DIRECTION_IN, io_index, &io_format_list);
        if (ret != NO_ERROR) {
            VXLOGE("init buffer vector fails");
            status = VX_FAILURE;
            break;
        }
    }

    if (status != VX_SUCCESS)
        goto EXIT;

    for (param_iter=m_out_io_param_info.begin(), io_index=0; param_iter!=m_out_io_param_info.end(); param_iter++, io_index++) {
        io_format_list.clear();
        memset(&io_memory, 0x0, sizeof(io_memory));
        if (param_iter->type == IO_PARAM_VX) {
            status = getIoInfoFromVx(parameters[param_iter->vx_param.index], param_iter->vx_param.info, &io_format_list, &io_memory);
            if (status != VX_SUCCESS) {
                VXLOGE("getting io info fails");
                break;
            }
        } else {
            struct io_format_t io_format;
            io_format = param_iter->custom_param.io_format;
            io_memory = param_iter->custom_param.io_memory;
            for (vx_uint32 i=0; i<io_memory.count; i++)
                io_format_list.push_back(io_format);
        }

        /* the biggest one is a representative */
        memset(&io_format_rep, 0x0, sizeof(io_format_rep));
        for (format_iter=io_format_list.begin(); format_iter!=io_format_list.end(); format_iter++) {
            if ((format_iter->width * format_iter->height * format_iter->pixel_byte) >
                (io_format_rep.width * io_format_rep.height * io_format_rep.pixel_byte))
                io_format_rep = *format_iter;
        }
        io_format_rep.target = m_task_if->getTargetId(VS4L_DIRECTION_OT, io_index);

        if (m_task_if->setIoInfo(VS4L_DIRECTION_OT, io_index, &io_format_rep, &io_memory) != true) {
                VXLOGE("setting io info fails, err:%d", status);
                m_task_if->displayObjectInfo();
                status = VX_FAILURE;
                break;
        }

        ret = initBufferVectorBunch(VS4L_DIRECTION_OT, io_index, &io_format_list);
        if (ret != NO_ERROR) {
            VXLOGE("init buffer vector fails");
            status = VX_FAILURE;
            break;
        }
    }

    if (status != VX_SUCCESS)
        goto EXIT;

EXIT:
    return status;
}

void
ExynosVpuTaskIfWrapper::setTaskName(vx_char* task_name)
{
    if(task_name)
        m_task_if->setTaskName(task_name);
}

status_t
ExynosVpuTaskIfWrapper::setupDriverNode(void)
{
    status_t ret = NO_ERROR;

    if (m_kernel->getGraphReConstPolicy() == vx_false_e) {
        ret = m_task_if->driverNodeSetGraph();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to graph, ret:%d", ret);
            m_task_if->displayObjectInfo();
            m_task_if->displayTdInfo();
            goto EXIT;
        }
        ret = m_task_if->driverNodeSetFormat();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to format, ret:%d", ret);
            m_task_if->displayObjectInfo();
            goto EXIT;
        }
    }

    if (m_kernel->getGraphReStreamPolicy() == vx_false_e) {
        ret = m_task_if->driverNodeStreamOn();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be stream-on, ret:%d", ret);
            m_task_if->displayObjectInfo();
            goto EXIT;
        }
    }

    /* setpu the container's common data that shared by every frame */
    m_task_if->driverNodeSetContainerInfo();

EXIT:
    return ret;
}

vx_status
ExynosVpuTaskIfWrapper::preProcessTask(const vx_reference parameters[])
{
    EXYNOS_VPU_KERNEL_IN();

    vx_status status = VX_SUCCESS;

    Vector<io_param_info_t>::iterator param_iter;
    vx_uint32 index;
    Vector<io_buffer_t> *container;

    for (param_iter=m_in_io_param_info.begin(), index=0; param_iter!=m_in_io_param_info.end(); param_iter++, index++) {
        container = &m_in_buffer_vector_bunch.editItemAt(index);
        if ((*param_iter).type == IO_PARAM_VX) {
            status = getVxObjectHandle((*param_iter).vx_param, parameters[(*param_iter).vx_param.index], container, VX_READ_ONLY);
            if (status != VX_SUCCESS) {
                VXLOGE("getting handle of the vx object fails");
                goto EXIT;
            }
        } else {
            for (vx_uint32 i=0; i<container->size(); i++)
                container->editItemAt(i).m.fd = param_iter->custom_param.io_buffer_info[i].fd;
        }
    }

    for (param_iter=m_out_io_param_info.begin(), index=0; param_iter!=m_out_io_param_info.end(); param_iter++, index++) {
        container = &m_out_buffer_vector_bunch.editItemAt(index);
        if ((*param_iter).type == IO_PARAM_VX) {
            status = getVxObjectHandle((*param_iter).vx_param, parameters[(*param_iter).vx_param.index], container, VX_WRITE_ONLY);
            if (status != VX_SUCCESS) {
                VXLOGE("getting handle of the vx object fails");
                goto EXIT;
            }
        } else {
            for (vx_uint32 i=0; i<container->size(); i++)
                container->editItemAt(i).m.fd = param_iter->custom_param.io_buffer_info[i].fd;
        }
    }

    status_t ret;
    if (m_kernel->getGraphReConstPolicy() == vx_true_e) {
        ret = m_task_if->driverNodeOpen();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to graph, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }

        ret = m_task_if->driverNodeSetGraph();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to graph, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }

        ret = m_task_if->driverNodeSetFormat();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be setted to format, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    if (m_kernel->getGraphReStreamPolicy() == vx_true_e) {
        ret = m_task_if->driverNodeStreamOn();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be stream-on, ret:%d", ret);
            m_task_if->displayObjectInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    EXYNOS_VPU_KERNEL_OUT();

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapper::postProcessTask(const vx_reference parameters[])
{
    EXYNOS_VPU_KERNEL_IN();

    vx_status status = VX_SUCCESS;

    Vector<io_param_info_t>::iterator param_iter;
    vx_uint32 index;
    Vector<io_buffer_t> *container;

    for (param_iter=m_in_io_param_info.begin(), index=0; param_iter!=m_in_io_param_info.end(); param_iter++, index++) {
        if ((*param_iter).type == IO_PARAM_VX) {
            container = &m_in_buffer_vector_bunch.editItemAt(index);
            status = putVxObjectHandle((*param_iter).vx_param, parameters[(*param_iter).vx_param.index], container);
            if (status != VX_SUCCESS) {
                VXLOGE("putting handle of the vx object fails");
                goto EXIT;
            }
        }
    }

    for (param_iter=m_out_io_param_info.begin(), index=0; param_iter!=m_out_io_param_info.end(); param_iter++, index++) {
        if ((*param_iter).type == IO_PARAM_VX) {
            container = &m_out_buffer_vector_bunch.editItemAt(index);
            status = putVxObjectHandle((*param_iter).vx_param, parameters[(*param_iter).vx_param.index], container);
            if (status != VX_SUCCESS) {
                VXLOGE("putting handle of the vx object fails");
                goto EXIT;
            }
        }
    }

    status_t ret;
    if (m_kernel->getGraphReStreamPolicy() == vx_true_e) {
        ret = m_task_if->driverNodeStreamOff();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be stream-off, ret:%d", ret);
            m_task_if->displayObjectInfo();
            m_task_if->displayTdInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    if (m_kernel->getGraphReConstPolicy() == vx_true_e) {
        ret = m_task_if->driverNodeClose();
        if (ret != NO_ERROR) {
            VXLOGE("driver node cannot be closed, ret:%d", ret);
            m_task_if->displayObjectInfo();
            m_task_if->displayTdInfo();
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    EXYNOS_VPU_KERNEL_OUT();

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapper::processTask(const vx_reference parameters[], vx_uint32 frame_number)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    status = preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("pre-processing fails");
        goto EXIT;
    }

    vx_uint32 in_index, out_index;
    in_index = findIoIndex(VS4L_DIRECTION_IN, parameters);
    out_index = findIoIndex(VS4L_DIRECTION_OT, parameters);

    /* non-blocking function */
    ret = m_task_if->driverNodePutBuffer(frame_number, in_index, &m_in_buffer_vector_bunch, out_index, &m_out_buffer_vector_bunch);
    if (ret != NO_ERROR) {
        VXLOGE("queueing  fails");
        status  = VX_FAILURE;
        goto EXIT;
    }

    /* blocking function */
    uint32_t dq_frame_cnt, dq_in_index, dq_out_index;
    ret = m_task_if->driverNodeGetBuffer(&dq_frame_cnt, &dq_in_index, &dq_out_index);
    if (ret != NO_ERROR) {
        VXLOGE("de-queueing  fails");
        status  = VX_FAILURE;
        goto EXIT;
    }

    if ((dq_frame_cnt != frame_number) || (dq_in_index != in_index) || (dq_out_index != out_index)) {
        VXLOGE("q/dq is not sync, frame:%d/%d, in_index:%d/%d, out_index:%d/%d", frame_number, dq_frame_cnt, in_index, dq_in_index, out_index, dq_out_index);
        status  = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    /* release holding resource */
    vx_status status_post;
    status_post = postProcessTask(parameters);
    if (status_post != VX_SUCCESS) {
        VXLOGE("post-processing fails");
    }

    return (status | status_post);
}

ExynosVpuKernel::ExynosVpuKernel(vx_char *name, vx_uint32 param_num)
{
    strcpy(m_name, name);

    m_priority = DEFAULT_GRAPH_PRIORITY;
    m_vpu_share_resource = vx_true_e;
    m_param_num = param_num;
    m_allocator = NULL;
}

/* Destructor */
ExynosVpuKernel::~ExynosVpuKernel(void)
{
    Vector<ExynosVpuTaskIfWrapper*>::iterator task_iter;
    for (task_iter=m_task_wr_list.begin(); task_iter!=m_task_wr_list.end(); task_iter++) {
        if (*task_iter) {
            delete (*task_iter);
        } else {
            VXLOGE("taskif object is null");
        }
    }
    m_task_wr_list.clear();

    if (m_inter_task_buffer_list.size() && !m_allocator) {
        VXLOGE("allocator is not assigned");
    }

    List<struct io_buffer_info_t>::iterator buffer_iter;
    for (buffer_iter=m_inter_task_buffer_list.begin(); buffer_iter!=m_inter_task_buffer_list.end(); buffer_iter++) {
        m_allocator->free_mem(buffer_iter->size, &buffer_iter->fd, &buffer_iter->addr, buffer_iter->mapped);
    }

    if (m_allocator)
        delete m_allocator;
}

vx_status
ExynosVpuKernel::setTaskIfWrapper(vx_uint32 task_index, ExynosVpuTaskIfWrapper *task_wr)
{
    vx_uint32 task_num = m_task_wr_list.size();
    if (task_index <= task_num) {
        ExynosVpuTaskIfWrapper *task_wr = NULL;
        m_task_wr_list.insertAt(task_wr, task_num, task_index-task_num+1);
    } else {
        VXLOGE("task_index_%d is out of bound, task_num:%d", task_index, task_num);
        return VX_FAILURE;
    }

    if (m_task_wr_list[task_index] != NULL) {
        VXLOGE("task_%d is already assigned", task_index);
        m_task_wr_list[task_index]->getTaskIf()->displayObjectInfo();
        return VX_FAILURE;
    }

    m_task_wr_list.editItemAt(task_index) = task_wr;

    return VX_SUCCESS;
}

ExynosVpuTaskIf*
ExynosVpuKernel::getTaskIf(vx_uint32 task_index)
{
    if (m_task_wr_list[task_index] == NULL) {
        VXLOGE("task_%d is null", task_index);
        return NULL;
    }

    return m_task_wr_list[task_index]->getTaskIf();
}

ExynosVpuTask*
ExynosVpuKernel::getTask(vx_uint32 task_index)
{
    if (m_task_wr_list[task_index] == NULL) {
        VXLOGE("taskif_%d is null", task_index);
        return NULL;
    }

    ExynosVpuTask *task;
    task = m_task_wr_list[task_index]->getTaskIf()->getTask();
    if (task == NULL) {
        VXLOGE("task_%d is null", task_index);
        return NULL;
    }

    return task;
}

vx_status
ExynosVpuKernel::initKernel(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    if (num != m_param_num) {
        VXLOGE("parameter number is not matching, expected:%d, received:%d", m_param_num, num);
        return VX_FAILURE;
    }

    vx_uint32 priority;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_PRIORITY, &priority, sizeof(priority));
    if (status == NO_ERROR) {
        m_priority = priority;
    } else {
        m_priority = DEFAULT_GRAPH_PRIORITY;
    }

    vx_bool shared_resource;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_SHARE_RESOURCE, &shared_resource, sizeof(shared_resource));
    if (status == NO_ERROR) {
        m_vpu_share_resource = shared_resource;
    }

    status = setupBaseVxInfo(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("Setting base info of kernel fails at initialize, err:%d", status);
        goto EXIT;
    }

    status = initTask(node, parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("initialization task fails, err:%d", status);
        goto EXIT;
    }

    status = initVxIo(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("intiailzation io fails, err:%d", status);
        goto EXIT;
    }

    status = updateTaskFromVX(node, parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("intiailzation io fails, err:%d", status);
        goto EXIT;
    }

    status = finalizeKernel(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("intiailzation io fails, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernel::updateTaskFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    /* update task object from vx for each kernel specific character */
    status = updateTaskParamFromVX(node, parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("updating taks io fails, err:%d, %p, %p", status, node, parameters);
        goto EXIT;
    }

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;

    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        ret = (*taskwr_iter)->updateTaskIoFromVX(parameters);
        if (ret != NO_ERROR) {
            VXLOGE("updating memmap fails");
            (*taskwr_iter)->getTaskIf()->displayObjectInfo();
            status = VX_FAILURE;
        }
    }

    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        ret = (*taskwr_iter)->getTaskIf()->allocInterSubchainBuf();
        if (ret != NO_ERROR) {
            VXLOGE("allocating inter buf fails");
            (*taskwr_iter)->getTaskIf()->displayObjectInfo();
            status = VX_FAILURE;
        }
    }

    /* update task str from task object */
    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        ret = (*taskwr_iter)->getTaskIf()->updateStrFromObject();
        if (ret != NO_ERROR) {
            VXLOGE("updating task str fails");
            (*taskwr_iter)->getTaskIf()->displayObjectInfo();
            status = VX_FAILURE;
        }
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernel::finalizeKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;
    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        if (*taskwr_iter) {
            status = (*taskwr_iter)->finalizeTaskif(parameters);
            if (status != VX_SUCCESS) {
                VXLOGE("processing task fails");
                goto EXIT;
            }
        } else {
            VXLOGE("taskif object is null");
        }
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernel::setupDriver(void)
{
    status_t ret = NO_ERROR;

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;
    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        if (*taskwr_iter) {
            (*taskwr_iter)->setTaskName(m_task_name);
            ret = (*taskwr_iter)->setupDriverNode();
            if (ret != NO_ERROR) {
                VXLOGE("setup driver node fails");
                goto EXIT;
            }
        } else {
            VXLOGE("taskif object is null");
        }
    }

EXIT:
    if (ret == NO_ERROR)
        return VX_SUCCESS;
    else
        return VX_ERROR_NO_RESOURCES;
}

vx_status
ExynosVpuKernel::kernelFunction(vx_node node, const vx_reference parameters[], vx_uint32 num)
{
    EXYNOS_VPU_KERNEL_IN();

    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;
    time_pair_t *time_pair = NULL;

    if (num != m_param_num) {
        VXLOGE("parameter number is not matching, expected:%d, received:%d", m_param_num, num);
        return VX_FAILURE;
    }

    vx_uint32 frame_number;
    if (node) {
        status = vxQueryNode(node, VX_NODE_ATTRIBUTE_FRAME_NUMBER, &frame_number, sizeof(frame_number));
        if (status != VX_SUCCESS) {
            VXLOGW("node doesn't support fraem information, err:%d", status);
            frame_number = 0;
        }

        status = vxQueryNode(node, VX_NODE_ATTRIBUTE_LOCAL_INFO_PTR, &time_pair, sizeof(time_pair));
        if (status != VX_SUCCESS) {
            VXLOGW("node doesn't support local information str");
            time_pair = NULL;
        }

    } else {
        VXLOGE("node is null");
        frame_number = 0;
        status = VX_FAILURE;
        goto EXIT;
    }

#if (SERIALIZE_VPU_KERNEL==1)
    m_global_vpu_kernel_lock.lock();
    VXLOGD2("[IN]%s", m_name);
#endif

    TIMESTAMP_START(time_pair, TIMEPAIR_KERNEL);

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;
    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        if (*taskwr_iter) {
            status = (*taskwr_iter)->processTask(parameters, frame_number);
            if (status != VX_SUCCESS) {
                VXLOGE("processing task fails");
#if (SERIALIZE_VPU_KERNEL==1)
                VXLOGD2("[OT]%s", m_name);
                m_global_vpu_kernel_lock.unlock();
#endif
                goto EXIT;
            }
        } else {
            VXLOGE("taskif object is null");
        }
    }

    TIMESTAMP_END(time_pair, TIMEPAIR_KERNEL);

#if (SERIALIZE_VPU_KERNEL==1)
    VXLOGD2("[OT]%s", m_name);
    m_global_vpu_kernel_lock.unlock();
#endif

    struct timeval *vpudrv_timeval_array;
    uint64_t *task_time_array;
    vpudrv_timeval_array = m_task_wr_list[0]->getTaskIf()->getVpuDrvTimeStamp(VS4L_DIRECTION_OT);

    TIMESTAMP_START_VALUE(time_pair, TIMEPAIR_FIRMWARE, convertTimeUs(&vpudrv_timeval_array[VPUDRV_TMP_PROCESS]));
    TIMESTAMP_END_VALUE(time_pair, TIMEPAIR_FIRMWARE, convertTimeUs(&vpudrv_timeval_array[VPUDRV_TMP_DONE]));

    EXYNOS_VPU_KERNEL_OUT();

EXIT:
    return status;
}

status_t
ExynosVpuKernel::createStrFromObjectOfTask(void)
{
    status_t ret = NO_ERROR;

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;
    for (taskwr_iter=m_task_wr_list.begin(); taskwr_iter!=m_task_wr_list.end(); taskwr_iter++) {
        ret = (*taskwr_iter)->getTaskIf()->createStrFromObject();
        if (ret != NO_ERROR) {
            break;
        }
    }

    return ret;
}

status_t
ExynosVpuKernel::allocateBuffer(io_buffer_info_t *io_buffer_info)
{
    status_t ret = NO_ERROR;

    if (io_buffer_info->size == 0) {
        VXLOGE("size is not set");
        ret = INVALID_OPERATION;
        goto EXIT;
    }

    if (m_allocator == NULL) {
        m_allocator = new ExynosVisionIonAllocator();
        ret = m_allocator->init(false);
        if (ret != NO_ERROR) {
            VXLOGE("ion allocator's init fail, ret:%d", ret);
        goto EXIT;
        }
    }

    ret = m_allocator->alloc_mem(io_buffer_info->size, &io_buffer_info->fd, &io_buffer_info->addr, io_buffer_info->mapped);
    if (ret != NO_ERROR) {
        VXLOGE("allocating buffer fails, ret:%d", ret);
        goto EXIT;
    }

    m_inter_task_buffer_list.push_back(*io_buffer_info);

EXIT:
    return ret;
}

}; /* namespace android */

