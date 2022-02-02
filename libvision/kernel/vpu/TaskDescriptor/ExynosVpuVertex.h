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

#ifndef EXYNOS_VPU_VERTEX_H
#define EXYNOS_VPU_VERTEX_H

#include <utils/Vector.h>
#include <utils/List.h>

#include "ExynosVpuTask.h"

namespace android {

class ExynosVpuProcess;
class ExynosVpu3dnnProcess;
class ExynosVpuIoInternalRam;
class ExynosVpuIoDynamicMapRoi;
class ExynosVpuIoFixedMapRoi;
class ExynosVpuIoTypesDesc;
class ExynosVpuIoSize;
class ExynosVpuIoSizeOp;
class ExynosVpuIoSizeOpScale;
class ExynosVpuIoSizeOpCropper;
class ExynosVpuIoExternalMem;

class ExynosVpuVertex {
private:
    ExynosVpuTask *m_task;

    Vector<ExynosVpuSubchain*> m_subchain_list;
    uint32_t m_vertex_index_in_task;

    Vector<ExynosVpuVertex*> m_prev_vertex_list;
    Vector<ExynosVpuVertex*> m_post_vertex_list;

protected:
    struct vpul_vertex m_vertex_info;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_vertex *vertex);

    static bool connect(ExynosVpuVertex *prev_vertex, ExynosVpuVertex *post_vertex);

    /* Constructor */
    ExynosVpuVertex(ExynosVpuTask *task, enum vpu_task_vertex_type type);
    ExynosVpuVertex(ExynosVpuTask *task, const struct vpul_vertex *vertex_info);

    /* Destructor */
    virtual ~ExynosVpuVertex()
    {

    }

    bool connectPostVertex(ExynosVpuVertex *post_vertex);
    bool connectPrevVertex(ExynosVpuVertex *prev_vertex);

    uint32_t addSubchain(ExynosVpuSubchain *subchain);
    ExynosVpuSubchain* getSubchain(uint32_t index);

    ExynosVpuTask* getTask(void);
    uint32_t getIndex(void);

    enum vpu_task_vertex_type getType(void);

    virtual status_t updateVertexIo(uint32_t std_width, uint32_t std_height);

    status_t spreadVertexIoInfo(void);

    status_t checkConstraint(void);
    virtual status_t updateStructure(void);
    virtual status_t updateObject(void);

    status_t insertVertexToTaskDescriptor(void *task_descriptor_base);

    void displayObjectInfo(uint32_t tab_num = 0);
    virtual void displayProcessInfo(uint32_t tab_num = 0)
    {
        VXLOGE("this is not process object, vtype:%d, tab_num:%d", m_vertex_info.vtype, tab_num);
    }
};

}; // namespace android
#endif
