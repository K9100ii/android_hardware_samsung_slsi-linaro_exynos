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

#define LOG_TAG "ExynosVpuDrvNode"
#include <cutils/log.h>

#include "ExynosVisionAutoTimer.h"

#include "ExynosVpuDrvNode.h"

#include "ExynosVpuTaskIf.h"

namespace android {

using namespace std;

ExynosVpuDrvNode::ExynosVpuDrvNode()
{
    m_fd = -1;
    m_nodeState = NODE_STATE_NONE;

    memset(&m_input_container_list, 0x0, sizeof(m_input_container_list));
    memset(&m_output_container_list, 0x0, sizeof(m_output_container_list));
}

ExynosVpuDrvNode::~ExynosVpuDrvNode()
{
    struct vs4l_container_list *vs4l_container_list;

    vs4l_container_list = &m_input_container_list;
    for (uint32_t i=0; i<vs4l_container_list->count; i++) {
        if (vs4l_container_list->containers[i].count) {
            if (vs4l_container_list->containers[i].buffers) {
                delete vs4l_container_list->containers[i].buffers;
            } else {
                VXLOGE("containter information is not valid");
                break;
            }
        }

    }
    if (vs4l_container_list->containers)
        delete vs4l_container_list->containers;

    vs4l_container_list = &m_output_container_list;
    for (uint32_t i=0; i<vs4l_container_list->count; i++) {
        if (vs4l_container_list->containers[i].count) {
            if (vs4l_container_list->containers[i].buffers) {
                delete vs4l_container_list->containers[i].buffers;
            } else {
                VXLOGE("containter information is not valid");
                break;
            }
        }

    }
    if (vs4l_container_list->containers)
        delete vs4l_container_list->containers;
}

void
ExynosVpuDrvNode::setContainerInfo(struct io_container_bunch_t *container_bunch)
{
    struct vs4l_container_list *vs4l_container_list;

    if (container_bunch->direction == VS4L_DIRECTION_IN)
        vs4l_container_list = &m_input_container_list;
    else
        vs4l_container_list = &m_output_container_list;

    vs4l_container_list->direction = container_bunch->direction;
    vs4l_container_list->flags = (1 << VS4L_CL_FLAG_TIMESTAMP);
    vs4l_container_list->count = container_bunch->container_vector.size();
    vs4l_container_list->containers = new struct vs4l_container[vs4l_container_list->count];

    for (uint32_t i=0; i<vs4l_container_list->count; i++) {
        struct vs4l_container *vs4l_container = &vs4l_container_list->containers[i];
        const struct io_containter_t *io_container = &container_bunch->container_vector.itemAt(i);
        vs4l_container->type = io_container->type;
        vs4l_container->target = io_container->target;
        vs4l_container->memory = io_container->memory;
        vs4l_container->count = io_container->buffer_vector.size();
        vs4l_container->buffers = new struct vs4l_buffer[vs4l_container->count];
    }
}

/* JUN_TBD, should be removed */
uint32_t global_vpu_node_cnt = 0;

int
ExynosVpuDrvNode::getNodeFd()
{
    if (m_nodeState == NODE_STATE_NONE) {
        VXLOGW("getting fd from global node counter");
        return ++global_vpu_node_cnt;
    }

    return m_fd;
}

status_t
ExynosVpuDrvNode::open(void)
{
    EXYNOS_VPU_NODE_IN();

    char node_name[30];

    memset(&node_name, 0x00, sizeof(node_name));
    snprintf(node_name, sizeof(node_name), "%s", NODE_PREFIX);

    m_fd = exynos_vpu_open(node_name, O_RDWR, 0);
    if (m_fd < 0) {
        VXLOGE("%s:exynos_vpu_open fail, ret(%d)", node_name, m_fd);
        return INVALID_OPERATION;
    }
    VXLOGD2("Node(%s) opened. m_fd(%d)", node_name, m_fd);
    m_nodeState = NODE_STATE_OPEN;

    EXYNOS_VPU_NODE_OUT();

    return NO_ERROR;
}

status_t
ExynosVpuDrvNode::close(void)
{
    EXYNOS_VPU_NODE_IN();

    if (exynos_vpu_close(m_fd) != 0) {
        VXLOGE("close fail");
        return INVALID_OPERATION;
    }
    m_nodeState = NODE_STATE_NONE;

    EXYNOS_VPU_NODE_OUT();

    return NO_ERROR;
}

status_t
ExynosVpuDrvNode::setGraph(struct vs4l_graph *graph)
{
    EXYNOS_VPU_NODE_IN();

    int ret = 0;

    VXLOGD2("s_graph, fd:%d, td:%p", m_fd, graph);
    ret = exynos_vpu_s_graph(m_fd, graph);

    if (ret < 0) {
        VXLOGE("exynos_vpu_s_graphfail (%d)", ret);
        return ret;
    }
    m_nodeState = NODE_STATE_SET_GRAPH;

    EXYNOS_VPU_NODE_OUT();

    return NO_ERROR;
}

status_t
ExynosVpuDrvNode::setFormat(struct io_format_bunch_t *format_bunch)
{
    EXYNOS_VPU_NODE_IN();

    int ret = 0;

    struct vs4l_format_list format_list;
    Vector<struct io_format_t> *format_vector = &format_bunch->format_vector;

    format_list.direction = format_bunch->direction;
    format_list.count = format_vector->size();
    format_list.formats = new struct vs4l_format[format_list.count];
    for (uint32_t i=0; i<format_list.count; i++) {
        format_list.formats[i].target = (*format_vector)[i].target;
        format_list.formats[i].format = (*format_vector)[i].format;
        format_list.formats[i].plane = (*format_vector)[i].plane;
        format_list.formats[i].width = (*format_vector)[i].width;
        format_list.formats[i].height = (*format_vector)[i].height;
    }

    ret = exynos_vpu_s_format(m_fd, &format_list);
    if (ret < 0) {
        VXLOGE("exynos_vpu_s_format (%d)", ret);
        goto EXIT;
    }
    m_nodeState = NODE_STATE_SET_FORMAT;

EXIT:
    EXYNOS_VPU_NODE_OUT();

    if (format_list.formats)
        delete format_list.formats;

    return ret;
}

status_t
ExynosVpuDrvNode::setParam(uint32_t target, void *addr, uint32_t offset, uint32_t size)
{
    EXYNOS_VPU_NODE_IN();
    int ret = 0;

    struct vs4l_param param;
    param.target = target;
    param.addr = (unsigned long)addr;
    param.offset = offset;
    param.size = size;

    struct vs4l_param_list param_list;
    param_list.count = 1;
    param_list.params = &param;

    ret = exynos_vpu_s_param(m_fd, &param_list);
    if (ret < 0) {
        VXLOGE("exynos_vpu_s_param (%d)", ret);
        goto EXIT;
    }

    EXYNOS_VPU_NODE_OUT();

EXIT:
    return ret;
}

status_t
ExynosVpuDrvNode::streamOn(void)
{
    EXYNOS_VPU_NODE_IN();

    int ret = 0;

    ret = exynos_vpu_stream_on(m_fd);

    if (ret < 0) {
        VXLOGE("exynos_vpu_stream_on (%d)", ret);
        return ret;
    }
    m_nodeState = NODE_STATE_STREAM_ON;

    EXYNOS_VPU_NODE_OUT();

    return NO_ERROR;
}

status_t
ExynosVpuDrvNode::streamOff(void)
{
    EXYNOS_VPU_NODE_IN();
    int ret = 0;

    if (m_nodeState != NODE_STATE_STREAM_ON)
        return NO_ERROR;

    ret = exynos_vpu_stream_off(m_fd);
    if (ret < 0) {
        VXLOGE("exynos_vpu_stream_off (%d)", ret);
        return ret;
    }
    m_nodeState = NODE_STATE_SET_FORMAT;

    EXYNOS_VPU_NODE_OUT();

    return NO_ERROR;
}

status_t
ExynosVpuDrvNode::qbuf(struct io_container_bunch_t *container_bunch)
{
    EXYNOS_VPU_NODE_IN();

    status_t ret = 0;

    struct vs4l_container_list *vs4l_container_list;
    if (container_bunch->direction == VS4L_DIRECTION_IN) {
        vs4l_container_list = &m_input_container_list;
    } else {
        vs4l_container_list = &m_output_container_list;
    }

    vs4l_container_list->id = container_bunch->id;
    vs4l_container_list->index = container_bunch->index;

    for (uint32_t i=0; i<vs4l_container_list->count; i++) {
        struct vs4l_container *vs4l_container = &vs4l_container_list->containers[i];
        const struct io_containter_t *io_container = &container_bunch->container_vector.itemAt(i);

        for (uint32_t j=0; j<vs4l_container->count; j++) {
            if (vs4l_container->memory == VS4L_MEMORY_USERPTR) {
                vs4l_container->buffers[j].m.userptr = io_container->buffer_vector[j].m.userptr;
            } else if (vs4l_container->memory == VS4L_MEMORY_DMABUF) {
                vs4l_container->buffers[j].m.fd = io_container->buffer_vector[j].m.fd;
            } else {
                VXLOGE("unsupported memory, memory:%d", vs4l_container->memory);
            }
            memcpy(&vs4l_container->buffers[j].roi, &io_container->buffer_vector[j].roi, sizeof(struct vs4l_roi));
        }
    }

    VXLOGD3("[fd:%d]qbuf, id:%d, index:%d", m_fd, vs4l_container_list->id, vs4l_container_list->index);
    ret = exynos_vpu_qbuf(m_fd, vs4l_container_list);
    if (ret < 0) {
        VXLOGE("exynos_vpu_qbuf(m_fd:%d) fail (%d)", m_fd, ret);
        return ret;
    }
    m_nodeState = NODE_STATE_RUNNING;

    EXYNOS_VPU_NODE_OUT();

    return ret;
}

status_t
ExynosVpuDrvNode::dqbuf(struct io_container_bunch_t *container_bunch)
{
    EXYNOS_VPU_NODE_IN();

    status_t ret = 0;

    struct vs4l_container_list vs4l_container_list;
    memset(&vs4l_container_list, 0x0, sizeof(vs4l_container_list));

    vs4l_container_list.direction = container_bunch->direction;

    ret = exynos_vpu_dqbuf(m_fd, &vs4l_container_list);
    if (ret < 0) {
        if (ret != -EAGAIN)
            VXLOGE("exynos_v4l2_dqbuf(fd:%d) fail (%d)", m_fd, ret);
    } else {
        VXLOGD2("[fd:%d]dqbuf, id:%d, index:%d", m_fd, vs4l_container_list.id, vs4l_container_list.index);
        container_bunch->id = vs4l_container_list.id;
        container_bunch->index = vs4l_container_list.index;
        memcpy(&container_bunch->timestamp[0], &vs4l_container_list.timestamp[0], sizeof(vs4l_container_list.timestamp));
        m_nodeState = NODE_STATE_STREAM_ON;
    }

    EXYNOS_VPU_NODE_OUT();

    return ret;
}

}; /* namespace android */

