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

#ifndef EXYNOS_VPU_KERNEL_H
#define EXYNOS_VPU_KERNEL_H

#include <utils/List.h>
#include <utils/Vector.h>

#include <VX/vx.h>

#include "ExynosVisionMemoryAllocator.h"
#include "ExynosVpuTaskIf.h"

//#define EXYNOS_VPU_KERNEL_IF_TRACE
#ifdef EXYNOS_VPU_KERNEL_IF_TRACE
#define EXYNOS_VPU_KERNEL_IF_IN()   VXLOGD("IN...")
#define EXYNOS_VPU_KERNEL_IF_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VPU_KERNEL_IF_IN()   ((void *)0)
#define EXYNOS_VPU_KERNEL_IF_OUT()  ((void *)0)
#endif

//#define EXYNOS_VPU_KERNEL_TRACE
#ifdef EXYNOS_VPU_KERNEL_TRACE
#define EXYNOS_VPU_KERNEL_IN()   VXLOGD("IN..")
#define EXYNOS_VPU_KERNEL_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VPU_KERNEL_IN()   ((void *)0)
#define EXYNOS_VPU_KERNEL_OUT()  ((void *)0)
#endif

namespace android {

using namespace std;

class ExynosVpuKernel;

#define MAX_BUFFER_LIST_NUM 10

struct vx_image_info_t {
    vx_uint32 plane;
};

#define PYRAMID_WHOLE_LEVEL 0xFF
struct vx_pyramid_info_t {
    vx_uint32 level;
};

struct vx_array_info_t {
};

struct vx_scalar_info_t {
    void *ptr;
};

typedef struct _vx_param_info_t {
    union {
        struct vx_image_info_t	image;
        struct vx_pyramid_info_t	pyramid;
        struct vx_array_info_t	array;
        struct vx_scalar_info_t	scalar;
    };
} vx_param_info_t;

struct io_vx_info_t {
    /* param index shows that which vx parameter object is related. */
    vx_uint32 index;
    /* param_info has the information of vx parameter object such as the plane index. */
    vx_param_info_t info;
};

#define MAX_IO_BUF_NUM  20

struct io_custom_info_t {
    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info[MAX_IO_BUF_NUM];
};

enum param_type_t {
    IO_PARAM_VX,
    IO_PARAM_CUSTOM
};

typedef struct _io_param_info_t {
    enum param_type_t type;
    union {
        struct io_vx_info_t vx_param;
        struct io_custom_info_t custom_param;
    };
} io_param_info_t;

class ExynosVpuTaskIfWrapper {

private:
    /* buffers of each container from container list. vpu kernel handles only buffer information.
      container information for vpu driver is held by ExynosVpuTask.  */
    Vector<Vector<io_buffer_t>> m_in_buffer_vector_bunch;
    Vector<Vector<io_buffer_t>> m_out_buffer_vector_bunch;

    /* buffer index of buffer-set, void* is a vx data reference address */
    List<Vector<void*>>    m_input_set_list;
    List<Vector<void*>>    m_output_set_list;

    ExynosVpuKernel *m_kernel;

    /* JUN_TBD, task reconstruction flag to avoid vpu resource sharing */
    vx_bool m_task_reconst;

protected:
    ExynosVpuTaskIf *m_task_if;

    Vector<io_param_info_t> m_in_io_param_info;
    Vector<io_param_info_t> m_out_io_param_info;

private:
    vx_uint32 findIoIndex(vx_enum dir, const vx_reference parameters[]);
    status_t initBufferVectorBunch(vx_enum io_direction, vx_uint32 io_index, List<struct io_format_t> *io_format_list);

protected:
    virtual vx_status preProcessTask(const vx_reference parameters[]);
    virtual vx_status postProcessTask(const vx_reference parameters[]);

public:
    /* Constructor */
    ExynosVpuTaskIfWrapper(ExynosVpuKernel *kernel, uint32_t task_index = 0);

        /* Destructor */
    virtual ~ExynosVpuTaskIfWrapper();

    ExynosVpuTaskIf* getTaskIf()
    {
        return m_task_if;
    }
    ExynosVpuKernel* getKernel(void)
    {
        return m_kernel;
    }

    vx_status setIoVxParam(vx_enum io_dir, vx_uint32 io_index, vx_uint32 param_index, vx_param_info_t param_info);
    vx_status setIoCustomParam(vx_enum io_dir, vx_uint32 io_index, struct io_format_t *io_format, struct io_memory_t *io_memory, io_buffer_info_t *io_buffer_info);

    void setTaskName(vx_char *task_name);
    void setReconstPerFrame(vx_bool reconst);

    status_t setupDriverNode(void);

    vx_status updateTaskIoFromVX(const vx_reference *parameters);
    vx_status finalizeTaskif(const vx_reference *parameters);

    virtual vx_status processTask(const vx_reference parameters[], vx_uint32 frame_number);
};

#define SAVE_INTERMEDIATE_BUFFER  0

class ExynosVpuKernel  {

private:
    vx_char m_name[VX_MAX_KERNEL_NAME];

    /* allocator for intermediate buffer */
    ExynosVisionAllocator *m_allocator;

#if (SAVE_INTERMEDIATE_BUFFER==0)
    /* the intermediate fd list */
    List<struct io_buffer_info_t> m_inter_task_buffer_list;
#endif

protected:
    vx_char m_task_name[VX_MAX_KERNEL_NAME];

    vx_uint32 m_priority;
    vx_bool m_vpu_share_resource;
    vx_uint32 m_param_num;

    vx_rectangle_t m_valid_rect;
    vx_uint32 m_kernelState;

    Vector<ExynosVpuTaskIfWrapper*> m_task_wr_list;

public:
#if (SAVE_INTERMEDIATE_BUFFER==1)
    /* the intermediate fd list */
    List<struct io_buffer_info_t> m_inter_task_buffer_list;
#endif

private:
    /* gathering and caculaating basic vx information for kernel processing from parameter */
    virtual vx_status setupBaseVxInfo(const vx_reference parameters[])
    {
        VXLOGD("Nothing to setup base vx information at %s, param[0]:%p", getName(), &parameters[0]);

        return VX_SUCCESS;
    }
    virtual vx_status initTask(vx_node node, const vx_reference *parameters) = 0;
    virtual vx_status initVxIo(const vx_reference *parameters) = 0;
    virtual vx_status updateTaskFromVX(vx_node node, const vx_reference *parameters);
    vx_status finalizeKernel(const vx_reference *parameters);

protected:
    virtual vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters) = 0;

    vx_status createStrFromObjectOfTask(void);
    status_t allocateBuffer(io_buffer_info_t *io_buffer_info);

    vx_status setTaskIfWrapper(vx_uint32 task_index, ExynosVpuTaskIfWrapper *task_wr);
    ExynosVpuTaskIf* getTaskIf(vx_uint32 task_index);
    ExynosVpuTask* getTask(vx_uint32 task_index);

public:
    /* Constructor */
    ExynosVpuKernel(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernel();

    vx_char* getName(void)
    {
        return m_name;
    }

    vx_bool getGraphReConstPolicy(void)
    {
        if (m_vpu_share_resource)
            return vx_false_e;
        else
            return vx_true_e;
    }

    vx_bool getGraphReStreamPolicy(void)
    {
        if (m_vpu_share_resource)
            return vx_false_e;
        else
            return vx_true_e;
    }

    vx_status initKernel(vx_node node, const vx_reference *parameters, vx_uint32 num);
    vx_status setupDriver(void);
    vx_status kernelFunction(vx_node node, const vx_reference parameters[], vx_uint32 num);
};

}; // namespace android
#endif
