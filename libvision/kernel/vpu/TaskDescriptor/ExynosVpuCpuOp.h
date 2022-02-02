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

#ifndef EXYNOS_VPU_CPUOP_H
#define EXYNOS_VPU_CPUOP_H

#include <utility>

#include "ExynosVpuSubchain.h"

namespace android {

using namespace std;

class ExynosVpuCpuOpFactory {
public:
    ExynosVpuCpuOp* createCpuOp(ExynosVpuSubchainCpu *subchain, enum vpul_cpu_op_code opcode);
    ExynosVpuCpuOp* createCpuOp(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);
};

class ExynosVpuCpuOp {
private:
    ExynosVpuSubchainCpu *m_subchain;

protected:
    struct vpul_cpu_op m_op_info;

    uint32_t m_op_index_in_subchain;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_cpu_op *cpu_op);

    /* Constructor */
    ExynosVpuCpuOp(ExynosVpuSubchainCpu *subchain, enum vpul_cpu_op_code op_code);
    ExynosVpuCpuOp(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOp()
    {

    }

    status_t checkConstraint(void);
    struct vpul_cpu_op *getCpuOpInfo(void);

    virtual status_t updateStructure(void);

    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpCtrlSepFiltFromFrac : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpCtrlSepFiltFromFrac(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpCtrlSepFiltFromFrac(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpCtrlSepFiltFromFrac()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpLkStatUpPtChThr : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpLkStatUpPtChThr(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpLkStatUpPtChThr(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpLkStatUpPtChThr()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpCtrlChLoop : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpCtrlChLoop(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpCtrlChLoop(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpCtrlChLoop()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpHwUpdateM2LRecord : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpHwUpdateM2LRecord(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpHwUpdateM2LRecord(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpHwUpdateM2LRecord()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpLkUpdatePtByXyOfs : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpLkUpdatePtByXyOfs(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpLkUpdatePtByXyOfs(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpLkUpdatePtByXyOfs()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpCopyPuResult2PuParam : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpCopyPuResult2PuParam(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpCopyPuResult2PuParam(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpCopyPuResult2PuParam()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuCpuOpWriteRprtPuResult : public ExynosVpuCpuOp {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params);

    /* Constructor */
    ExynosVpuCpuOpWriteRprtPuResult(ExynosVpuSubchainCpu *subchain);
    ExynosVpuCpuOpWriteRprtPuResult(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info);

    /* Destructor */
    virtual ~ExynosVpuCpuOpWriteRprtPuResult()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

}; // namespace android
#endif
