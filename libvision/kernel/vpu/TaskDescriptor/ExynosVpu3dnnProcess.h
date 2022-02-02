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

#ifndef EXYNOS_VPU_3DNN_PROCESS_H
#define EXYNOS_VPU_3DNN_PROCESS_H

#include <utils/Vector.h>
#include <utils/List.h>

#include "ExynosVpuVertex.h"

namespace android {

class ExynosVpu3dnnProcess : public ExynosVpuVertex {
private:
    ExynosVpu3dnnProcessBase *m_tdnn_proc_base;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_3dnn_process *tdnn_process);

    /* Constructor */
    ExynosVpu3dnnProcess(ExynosVpuTask *task);
    ExynosVpu3dnnProcess(ExynosVpuTask *task, const struct vpul_vertex *vertex_info);

    /* Destructor */
    virtual ~ExynosVpu3dnnProcess();
    struct vpul_3dnn_process *get3dnnProcessInfo(void)
    {
        return &m_vertex_info.proc3dnn;
    }

    ExynosVpu3dnnProcessBase* getTdnnProcBase(void);

    status_t updateStructure(void);
    status_t updateObject(void);

    virtual void displayProcessInfo(uint32_t tab_num);
};

}; // namespace android
#endif
