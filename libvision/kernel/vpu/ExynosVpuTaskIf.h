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

#ifndef EXYNOS_VPU_TASKIF_H
#define EXYNOS_VPU_TASKIF_H

#include <utils/List.h>
#include <utils/Vector.h>

#include "ExynosVisionMemoryAllocator.h"

#include "vpul-ds.h"

#include "ExynosVpuDrvNode.h"
#include "ExynosVpuTask.h"

namespace android {

class ExynosVpuTaskIfWrapper;
class ExynosVpuVertex;
class ExynosVpuSubchain;
class ExynosVpuPu;
class ExynosVpuTask;

struct io_graph_ext_t {
    uint8_t vs4l_magic[4];
};

typedef struct _io_info_t {
    union {
        ExynosVpuPu *pu;
        ExynosVpuMemmap *memmap;
    };
} io_info_t;

struct io_memory_t {
    uint32_t type;
    uint32_t memory;
    uint32_t count;
};

struct io_format_t {
    uint32_t target;
    uint32_t format;
    uint32_t plane;
    uint32_t width;
    uint32_t height;

    uint32_t pixel_byte;
};

struct io_format_bunch_t {
    uint32_t direction;
    Vector<struct io_format_t> format_vector;
};

struct io_roi {
    uint32_t			x;
    uint32_t			y;
    uint32_t			w;
    uint32_t			h;
};

struct io_buffer_t {
    struct io_roi		roi;
    union {
        unsigned long	userptr;
        int32_t		fd;
    } m;
};

struct io_containter_t {
    uint32_t			type;
    uint32_t			target;
    uint32_t			memory;
    Vector<io_buffer_t>  buffer_vector;
};

enum vpudrv_time_measure_point {
    VPUDRV_TMP_QUEUE,
    VPUDRV_TMP_REQUEST,
    VPUDRV_TMP_RESOURCE,
    VPUDRV_TMP_PROCESS,
    VPUDRV_TMP_DONE,
    VPUDRV_TMP_COUNT,

    VPUDRV_TMP_MAX
};

struct io_container_bunch_t {
    uint32_t direction;
    uint32_t id;
    uint32_t index;
    Vector<io_containter_t> container_vector;

    struct timeval		timestamp[VPUDRV_TMP_MAX];
};

struct io_buffer_info_t {
    int32_t size;
    int32_t fd;
    char *addr;
    bool mapped;
};

class ExynosVpuTaskIf  {
private:
    uint32_t m_task_index;

    ExynosVpuTask *m_task;

    uint8_t m_cnn_task_flag;

    char *m_task_name;
    void *m_task_descriptor;
    ExynosVpuDrvNode   *m_vpu_node;

    Vector<io_info_t> m_in_io_info_list;
    Vector<io_info_t> m_out_io_info_list;

    struct io_format_bunch_t m_input_format_bunch;
    struct io_format_bunch_t m_output_format_bunch;

    struct io_container_bunch_t m_input_container_bunch;
    struct io_container_bunch_t m_output_container_bunch;

    /* buffer index of buffer-set, void* is a vx data reference address */
    List<Vector<void*>>    m_input_set_list;
    List<Vector<void*>>    m_output_set_list;

    /* allocator for intermediate buffer */
    ExynosVisionAllocator *m_allocator;

    /* the intermediate fd list */
    List<struct io_buffer_info_t> m_inter_chain_buffer_list;

public:

private:
    status_t createObjectFromStr(void);
    status_t createMemoryAllocator(void);

public:
    static void display_td_info(void *task_descriptor_base);
    static void print_td(void *task_descriptor_base);
    static void fwrite_td(char *task_name, uint32_t task_index, void *task_descriptor_base);

    static bool connect(ExynosVpuTaskIf *prev_task_if, uint32_t prev_io_out_port, ExynosVpuTaskIf *post_task_if, uint32_t post_io_in_port);

    /* Constructor */
    ExynosVpuTaskIf(uint32_t task_index);

    /* Destructor */
    virtual ~ExynosVpuTaskIf(void);

    status_t createStrFromObject(void);
    status_t updateStrFromObject(void);

    status_t updateTaskIo(uint32_t direction, uint32_t index, List<struct io_format_t> *io_format_list);
    status_t spreadTaskIoInfo(void);
    status_t allocInterSubchainBuf(void);

    status_t importTaskStr(const void *task_descriptor);

    status_t setInterPair(ExynosVpuPu *output_pu, ExynosVpuPu *input_pu);
    status_t setInterPair(uint32_t output_pu_index, uint32_t input_pu_index);

    status_t setIoPu(int32_t direction, uint32_t index, ExynosVpuPu *pu);
    status_t setIoPu(int32_t direction, uint32_t index, uint32_t pu_index);

    status_t setIoMemmap(int32_t io_direction, uint32_t io_index, uint32_t memmap_index);

    ExynosVpuPu* getIoPu(uint32_t dir, uint32_t index);
    uint32_t getIoNum(int32_t dir);
    bool setIoInfo(uint32_t direction, uint32_t index, struct io_format_t *io_format, struct io_memory_t *io_memory);

    uint32_t getTargetId(uint32_t io_direction, uint32_t io_index);

    void setTaskName(char* task_name)
    {
        m_task_name = task_name;
    }

    uint8_t isCnnTask(void)
    {
        return m_cnn_task_flag;
    }

    void setCnnTask(void)
    {
        m_cnn_task_flag = true;
    }

    bool connectPostTaskIf(uint32_t prev_io_out_port, ExynosVpuTaskIf *post_task_if, uint32_t post_io_in_port);
    bool connectPrevTaskIf(uint32_t post_io_in_port, ExynosVpuTaskIf *prev_task_if, uint32_t prev_io_out_port);

    void displayObjectInfo(uint32_t tab_num = 0);
    status_t displayTdInfo(void);

    uint32_t addTask(ExynosVpuTask *task);
    ExynosVpuTask* getTask(void);
    void* getTaskDescriptorBase(void)
    {
        return m_task_descriptor;
    }

    struct timeval* getVpuDrvTimeStamp(uint32_t dir);

    void driverNodeSetContainerInfo(void);
    status_t driverNodeOpen(void);
    status_t driverNodeSetGraph(void);
    status_t driverNodeSetFormat(void);
    status_t driverNodeSetParam(uint32_t pu_index, void *addr, uint32_t size);
    status_t driverNodeStreamOn(void);
    status_t driverNodeStreamOff(void);
    status_t driverNodeClose(void);

    status_t driverNodePutBuffer(uint32_t frame_id,
                                                uint32_t in_index, Vector<Vector<io_buffer_t>> *in_buffer_vector_bunch,
                                                uint32_t out_index, Vector<Vector<io_buffer_t>> *out_buffer_vector_bunch);
    status_t driverNodeGetBuffer(uint32_t *frame_id, uint32_t *in_index, uint32_t *out_index);
};

}; // namespace android
#endif
