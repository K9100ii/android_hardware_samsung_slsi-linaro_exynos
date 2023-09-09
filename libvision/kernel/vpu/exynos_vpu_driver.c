/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "ExynosVisionCommonConfig.h"
#include "exynos_vpu_driver.h"

#include "vs4l.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "vpudriver"
#include <utils/Log.h>

/* #define EXYNOS_VPU_TRACE */
#ifdef EXYNOS_VPU_TRACE
#define Exynos_vpu_In()   VXLOGD("IN..[fd:%d]", fd)
#define Exynos_vpu_Out()  VXLOGD("OUT..[fd:%d]", fd)
#else
#define Exynos_vpu_In()   ((void *)0)
#define Exynos_vpu_Out()  ((void *)0)
#endif

#define EXYNOS_VPU_INFO_DISPLAY  0
#define EXYNOS_VPU_KERNEL_INTERFACE_EN  1

static int __vpu_open(const char *filename, int oflag, va_list ap)
{
    mode_t mode = 0;
    int fd;

    if (oflag & O_CREAT)
        mode = va_arg(ap, int);

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    fd = open(filename, oflag, mode);
#else
    fd = 1;
#endif

    return fd;
}

int exynos_vpu_open(const char *filename, int oflag, ...)
{
    va_list ap;
    int fd;

    Exynos_vpu_In();

    va_start(ap, oflag);
    fd = __vpu_open(filename, oflag, ap);
    va_end(ap);

    Exynos_vpu_Out();

    return fd;
}

int exynos_vpu_close(int fd)
{
    int ret = -1;

    Exynos_vpu_In();

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    if (fd < 0)
        VXLOGE("invalid fd: %d", fd);
    else
        ret = close(fd);
#else
    ret = 0;
#endif

    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_s_graph(int fd, struct vs4l_graph *graph)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

    if (!graph) {
        VXLOGE("graph is NULL");
        return ret;
    }

#if (EXYNOS_VPU_INFO_DISPLAY==1)
    VXLOGD("id:%d", graph->id);
    VXLOGD("priority:%d", graph->priority);
    VXLOGD("time:%d", graph->time);
    VXLOGD("flags:%d", graph->flags);
    VXLOGD("size:%d", graph->size);
    VXLOGD("addr:%p", (void*)graph->addr);
    char *char_base = (char*)graph->addr;
    VXLOGD("ptr:%p, size:%d", (void*)graph->addr, graph->size);
    VXLOGD("magic number__: 0x%x, 0x%x, 0x%x, 0x%x", char_base[graph->size-4], char_base[graph->size-3]
                                                                , char_base[graph->size-2], char_base[graph->size-1]);
#endif

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_S_GRAPH, graph);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_S_GRAPH (%d - %s)", errno, strerror(errno));
        return ret;
    }

    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_s_format(int fd, struct vs4l_format_list *format_list)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

#if (EXYNOS_VPU_INFO_DISPLAY==1)
    VXLOGD("dir: %d, count: %d", format_list->direction, format_list->count);
    uint32_t i;
    for (i=0; i<format_list->count; i++) {
        VXLOGD("[%d]target: 0x%x", i, format_list->formats[i].target);
        VXLOGD("[%d]format: 0x%X", i, format_list->formats[i].format);
        VXLOGD("[%d]plane: %d", i, format_list->formats[i].plane);
        VXLOGD("[%d]width: %d", i, format_list->formats[i].width);
        VXLOGD("[%d]height: %d", i, format_list->formats[i].height);
    }
#endif

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_S_FORMAT, format_list);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_S_FORMAT (%d - %s)", errno, strerror(errno));
        return ret;
    }
    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_s_param(int fd, struct vs4l_param_list *param_list)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

#if (EXYNOS_VPU_INFO_DISPLAY==1)
    VXLOGD("[fd_%d]count: %d", fd, param_list->count);
    uint32_t i;
    for (i=0; i<param_list->count; i++) {
        VXLOGD("[%d]target: 0x%x", i, param_list->params[i].target);
        VXLOGD("[%d]addr: 0x%X", i, param_list->params[i].addr);
        VXLOGD("[%d]offset: 0x%X", i, param_list->params[i].offset);
        VXLOGD("[%d]size: %d", i, param_list->params[i].size);
    }
#endif

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_S_PARAM, param_list);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_S_PARAM (%d - %s)", errno, strerror(errno));
        return ret;
    }
    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_stream_on(int fd)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_STREAM_ON);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_STREAM_ON (%d - %s)", errno, strerror(errno));
        return ret;
    }

    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_stream_off(int fd)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_STREAM_OFF);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_STREAM_OFF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_qbuf(int fd, struct vs4l_container_list *buf)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

    if (!buf) {
        VXLOGE("buf is NULL");
        return ret;
    }

#if (EXYNOS_VPU_INFO_DISPLAY==1)
    VXLOGD("[fd_%d]dir: %d, id: %d, index: %d, container count: %d", fd, buf->direction, buf->id, buf->index, buf->count);
    uint32_t i, j;
    for (i=0; i<buf->count; i++) {
        struct vs4l_container *container = &buf->containers[i];
        VXLOGD("\t type: %d, target: 0x%x, memory: %d, count: %d", container->type, container->target, container->memory, container->count);
        for (j=0; j<container->count; j++) {
            struct vs4l_buffer *buffer = &container->buffers[j];
            VXLOGD("\t\tuserptr:%p, fd:%d", (void*)buffer->m.userptr, buffer->m.fd);
            VXLOGD("\t\t(%d, %d) width:%d, height:%d", buffer->roi.x, buffer->roi.y, buffer->roi.w, buffer->roi.h);
        }
    }
#endif

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_QBUF, buf);
#else
    ret = 0;
#endif
    if (ret) {
        VXLOGE("failed to ioctl: VS4L_VERTEXIOC_QBUF (%d - %s)", errno, strerror(errno));
        return ret;
    }

    Exynos_vpu_Out();

    return ret;
}

int exynos_vpu_dqbuf(int fd, struct vs4l_container_list *buf)
{
    int ret = -1;

    Exynos_vpu_In();

    if (fd < 0) {
        VXLOGE("invalid fd: %d", fd);
        return ret;
    }

    if (!buf) {
        VXLOGE("buf is NULL");
        return ret;
    }

#if (EXYNOS_VPU_KERNEL_INTERFACE_EN==1)
    ret = ioctl(fd, VS4L_VERTEXIOC_DQBUF, buf);
#else
    ret = 0;
#endif
    if (ret) {
        if (errno == EAGAIN)
            return -errno;

        ALOGW("failed to ioctl: VS4L_VERTEXIOC_DQBUF (%d - %s)", errno, strerror(errno));
        return ret;
    }

#if (EXYNOS_VPU_INFO_DISPLAY==1)
    VXLOGD("[fd_%d]dir: %d, id: %d, index: %d", fd, buf->direction, buf->id, buf->index);
#endif

    Exynos_vpu_Out();

    return ret;
}

