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

#ifndef EXYNOS_VPU_NODE_H
#define EXYNOS_VPU_NODE_H

#include <string.h>
#include <fcntl.h>

#include <utils/Mutex.h>
#include <utils/Vector.h>

#include "ExynosVisionCommonConfig.h"

#include "vs4l.h"
#include "exynos_vpu_driver.h"

namespace android {

/* #define EXYNOS_VPU_NODE_TRACE */
#ifdef EXYNOS_VPU_NODE_TRACE
#define EXYNOS_VPU_NODE_IN()   VXLOGD("IN...m_nodeState[%d]", m_nodeState)
#define EXYNOS_VPU_NODE_OUT()  VXLOGD("OUT..m_nodeState[%d]", m_nodeState)
#else
#define EXYNOS_VPU_NODE_IN()   ((void *)0)
#define EXYNOS_VPU_NODE_OUT()  ((void *)0)
#endif

class ExynosVpuDrvNode {
private:
    int                m_fd;
    int  m_nodeState;
    mutable Mutex m_nodeStateLock;

    struct vs4l_container_list m_input_container_list;
    struct vs4l_container_list m_output_container_list;

public:
    enum EXYNOS_VPU_NODE_STATE {
        NODE_STATE_BASE = 0,
        NODE_STATE_NONE,
        NODE_STATE_OPEN,
        NODE_STATE_SET_GRAPH,
        NODE_STATE_SET_FORMAT,
        NODE_STATE_STREAM_ON,
        NODE_STATE_RUNNING,
        NODE_STATE_MAX
    };

private:

public:
    /* Constructor */
    ExynosVpuDrvNode();

    /* Destructor */
    virtual ~ExynosVpuDrvNode();

    int getNodeFd();

    void setContainerInfo(struct io_container_bunch_t *container_bunch);

    status_t open(void);
    status_t close(void);

    status_t setGraph(struct vs4l_graph *graph);
    status_t setFormat(struct io_format_bunch_t *format_bunch);
    status_t setParam(uint32_t target, void *addr, uint32_t offset, uint32_t size);
    status_t streamOn(void);
    status_t streamOff(void);

    status_t qbuf(struct io_container_bunch_t *container_bunch);
    status_t dqbuf(struct io_container_bunch_t *container_bunch);
};

}; // namespace android
#endif
