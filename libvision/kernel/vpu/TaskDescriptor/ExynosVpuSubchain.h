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

#ifndef EXYNOS_VPU_SUBCHAIN_H
#define EXYNOS_VPU_SUBCHAIN_H

#include <utils/Vector.h>

#include "ExynosVpuVertex.h"

namespace android {

class ExynosVpuPu;
class ExynosVpuCpuOp;

class ExynosVpuSubchain {
private:

protected:
    ExynosVpuVertex *m_vertex;

    struct vpul_subchain m_subchain_info;
    uint32_t m_subchain_index_in_task;
    uint32_t m_subchain_index_in_vertex;

    uint32_t m_base_pu_idx;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain);

    /* Constructor */
    ExynosVpuSubchain(ExynosVpuVertex *vertex, enum  vpul_subchain_types type);
    ExynosVpuSubchain(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info);

    /* Destructor */
    virtual ~ExynosVpuSubchain()
    {

    }

    enum vpul_subchain_types getType(void);
    uint32_t getId(void);
    uint32_t getIndexInTask(void);
    uint32_t getIndexInVertex(void);
    ExynosVpuVertex* getVertex(void);
    ExynosVpuProcess* getProcess(void);
    ExynosVpu3dnnProcess* getTdnnProcess(void);

    virtual status_t spreadSubchainIoInfo(void);
    virtual status_t updateStructure(void *task_descriptor_base);
    virtual status_t updateObject(void);
    status_t insertSubchainToTaskDescriptor(void *task_descriptor_base);

    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuSubchainHw : public ExynosVpuSubchain {
private:
    Vector<ExynosVpuPu*> m_pu_vector;

    /* checking map to prevent same instance is allocated within subchain */
    uint8_t m_used_pu_map[VPU_PU_NUMBER];

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain);

    /* Constructor */
    ExynosVpuSubchainHw(ExynosVpuVertex *vertex);
    ExynosVpuSubchainHw(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info);

    uint32_t addPu(ExynosVpuPu *pu);
    ExynosVpuPu* getPu(uint32_t index);
    ExynosVpuPu* getPu(enum vpul_pu_instance pu_instance);

    status_t spreadSubchainIoInfo(void);
    status_t updateStructure(void *task_descriptor_base);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuSubchainCpu : public ExynosVpuSubchain {
private:
    Vector<ExynosVpuCpuOp*> m_op_vector;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_subchain *subchain);

    /* Constructor */
    ExynosVpuSubchainCpu(ExynosVpuVertex *vertex);
    ExynosVpuSubchainCpu(ExynosVpuVertex *vertex, const struct vpul_subchain *subchain_info);

    uint32_t addCpuOp(ExynosVpuCpuOp *cpu_op);

    status_t spreadSubchainIoInfo(void);
    status_t updateStructure(void *task_descriptor_base);

    void displayObjectInfo(uint32_t tab_num = 0);
};

}; // namespace android
#endif
