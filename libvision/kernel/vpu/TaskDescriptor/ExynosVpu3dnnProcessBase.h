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

#ifndef EXYNOS_VPU_3DNN_PROCESS_BASE_H
#define EXYNOS_VPU_3DNN_PROCESS_BASE_H

#include <utils/Vector.h>
#include <utils/List.h>

#include "ExynosVpuTask.h"

namespace android {

class ExynosVpuIo3dnnSize {
private:

protected:
    ExynosVpu3dnnProcessBase *m_tdnn_proc_base;

    struct vpul_3dnn_size m_tdnn_size_info;
    uint32_t m_size_index_in_3dnn_proc_base;

    ExynosVpuIo3dnnSize *m_src_size;

public:

private:

protected:
    /* Constructor */
    ExynosVpuIo3dnnSize(ExynosVpu3dnnProcessBase *tdnn_proc_base, enum vpul_3d_sizes_op_type type, ExynosVpuIo3dnnSize *src_size);
    ExynosVpuIo3dnnSize(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *size_info);

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_3dnn_size *tdnn_size);

    /* Destructor */
    virtual ~ExynosVpuIo3dnnSize()
    {

    }

    uint32_t getIndex(void)
    {
        return m_size_index_in_3dnn_proc_base;
    }

    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIo3dnnSizeInout : public ExynosVpuIo3dnnSize {
public:
    /* Constructor */
    ExynosVpuIo3dnnSizeInout(ExynosVpu3dnnProcessBase *tdnn_proc_base, enum vpul_inout_3dnn_type inout_tdnn_type, ExynosVpuIo3dnnSize *src_size = NULL);
    ExynosVpuIo3dnnSizeInout(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info);
};

class ExynosVpuIo3dnnSizeZinToXy : public ExynosVpuIo3dnnSize {
public:
    /* Constructor */
    ExynosVpuIo3dnnSizeZinToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size);
    ExynosVpuIo3dnnSizeZinToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info);
};

class ExynosVpuIo3dnnSizeZoutToXy : public ExynosVpuIo3dnnSize {
public:
    /* Constructor */
    ExynosVpuIo3dnnSizeZoutToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size);
    ExynosVpuIo3dnnSizeZoutToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info);
};

class ExynosVpuIo3dnnSizeCrop : public ExynosVpuIo3dnnSize {
public:
    /* Constructor */
    ExynosVpuIo3dnnSizeCrop(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size);
    ExynosVpuIo3dnnSizeCrop(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info);
};

class ExynosVpuIo3dnnSizeScale : public ExynosVpuIo3dnnSize {
public:
    /* Constructor */
    ExynosVpuIo3dnnSizeScale(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size);
    ExynosVpuIo3dnnSizeScale(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info);
};

class ExynosVpu3dnnLayer {
private:
    ExynosVpu3dnnProcessBase *m_tdnn_proc_base;

    uint32_t m_index_in_tdnr_proc_base;
    struct vpul_3dnn_layer m_tdnn_layer_info;

public:

private:

public:
    /* Constructor */
    ExynosVpu3dnnLayer(ExynosVpu3dnnProcessBase *tdnn_proc_base);
    ExynosVpu3dnnLayer(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_layer *layer_info);

    struct vpul_3dnn_layer* getLayerInfo();
};

class ExynosVpu3dnnProcessBase {
private:
    ExynosVpuTask *m_task;

    uint32_t m_tdnn_proc_base_index_in_task;

    struct vpul_3dnn_process_base m_tdnn_proc_base_info;

    Vector<ExynosVpuIo3dnnSize*> m_io_tdnn_size_list;
    Vector<ExynosVpu3dnnLayer*> m_tdnn_layer_list;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_3dnn_process_base *tdnn_proc_base);

    /* Constructor */
    ExynosVpu3dnnProcessBase(ExynosVpuTask *task);
    ExynosVpu3dnnProcessBase(ExynosVpuTask *task, const struct vpul_3dnn_process_base *tdnn_proc_base_info);

    /* Destructor */
    virtual ~ExynosVpu3dnnProcessBase();

    uint32_t getIndex(void);

    uint32_t addTdnnSize(ExynosVpuIo3dnnSize *size);
    uint32_t addTdnnLayer(ExynosVpu3dnnLayer *tdnn_layer);

    ExynosVpuIo3dnnSize* getIoTdnnSize(uint32_t index);

    status_t getLayerInoutMemmapList(uint32_t layer_inout_index, List<ExynosVpuMemmap*> *ret_memmap_list);

    status_t insertTdnnProcBaseToTaskDescriptor(void *task_descriptor_base);
};

}; // namespace android
#endif
