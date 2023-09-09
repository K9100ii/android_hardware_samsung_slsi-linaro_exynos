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

#define LOG_TAG "ExynosVpuCpuOp"
#include <cutils/log.h>

#include "ExynosVpuCpuOp.h"

namespace android {

ExynosVpuCpuOp*
ExynosVpuCpuOpFactory::createCpuOp(ExynosVpuSubchainCpu *subchain, enum vpul_cpu_op_code opcode)
{
    ExynosVpuCpuOp *cpu_op = NULL;

    switch (opcode) {
    case VPUL_OP_CTRL_CH_LOOP:
        cpu_op = new ExynosVpuCpuOpCtrlChLoop(subchain);
        break;
    case VPUL_OP_CTRL_SEP_FILT_FROM_FRAC:
        cpu_op = new ExynosVpuCpuOpCtrlSepFiltFromFrac(subchain);
        break;
    case VPUL_OP_LK_STAT_UP_PT_CH_THR:
        cpu_op = new ExynosVpuCpuOpLkStatUpPtChThr(subchain);
        break;
    case VPUL_OP_HW_UPDATE_M2LI_RECORD:
        cpu_op = new ExynosVpuCpuOpHwUpdateM2LRecord(subchain);
        break;
    case VPUL_OP_LK_UPDATE_PT_BY_XY_OFS:
        cpu_op = new ExynosVpuCpuOpLkUpdatePtByXyOfs(subchain);
        break;
    case VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS:
        cpu_op = new ExynosVpuCpuOpCopyPuResult2PuParam(subchain);
        break;
    case VPUL_OP_WRITE_RPRT_PU_RSLTS:
        cpu_op = new ExynosVpuCpuOpWriteRprtPuResult(subchain);
        break;
    default:
        VXLOGE("%d is not defined yet", opcode);
        break;
    }

    return cpu_op;
}

ExynosVpuCpuOp*
ExynosVpuCpuOpFactory::createCpuOp(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
{
    ExynosVpuCpuOp *cpu_op = NULL;

    switch (cpu_op_info->opcode) {
    case VPUL_OP_CTRL_CH_LOOP:
        cpu_op = new ExynosVpuCpuOpCtrlChLoop(subchain, cpu_op_info);
        break;
    case VPUL_OP_CTRL_SEP_FILT_FROM_FRAC:
        cpu_op = new ExynosVpuCpuOpCtrlSepFiltFromFrac(subchain, cpu_op_info);
        break;
    case VPUL_OP_LK_STAT_UP_PT_CH_THR:
        cpu_op = new ExynosVpuCpuOpLkStatUpPtChThr(subchain, cpu_op_info);
        break;
    case VPUL_OP_HW_UPDATE_M2LI_RECORD:
        cpu_op = new ExynosVpuCpuOpHwUpdateM2LRecord(subchain, cpu_op_info);
        break;
    case VPUL_OP_LK_UPDATE_PT_BY_XY_OFS:
        cpu_op = new ExynosVpuCpuOpLkUpdatePtByXyOfs(subchain, cpu_op_info);
        break;
    case VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS:
        cpu_op = new ExynosVpuCpuOpCopyPuResult2PuParam(subchain, cpu_op_info);
        break;
    case VPUL_OP_WRITE_RPRT_PU_RSLTS:
        cpu_op = new ExynosVpuCpuOpWriteRprtPuResult(subchain, cpu_op_info);
        break;
    default:
        VXLOGE("%d is not defined yet", cpu_op_info->opcode);
    }

    return cpu_op;
}

void
ExynosVpuCpuOp::display_structure_info(uint32_t tab_num, struct vpul_cpu_op *cpu_op)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[CpuOp] opcode:%d", MAKE_TAB(tap, tab_num), cpu_op->opcode);
    VXLOGI("%s[CpuOp] src_index_type:%d", MAKE_TAB(tap, tab_num), cpu_op->wa_src_dest.src_index_type);
    switch(cpu_op->wa_src_dest.src_index_type) {
    case VPUL_DIRECT_INDEX:
        VXLOGI("%s[CpuOp] src_wa_index:%d", MAKE_TAB(tap, tab_num), cpu_op->wa_src_dest.src_wa_index);
        break;
    case VPUL_PU_INDEX:
        VXLOGI("%s[CpuOp] src_pu_index:%d/%d/%d", MAKE_TAB(tap, tab_num), cpu_op->wa_src_dest.src_pu_index.vtx_idx,
                                                                                                                cpu_op->wa_src_dest.src_pu_index.sc_idx_in_vtx,
                                                                                                                cpu_op->wa_src_dest.src_pu_index.pu_idx_in_sc);
        break;
    default:
        VXLOGE("un-defined indication, %d", cpu_op->wa_src_dest.src_index_type);
        break;
    }
    VXLOGI("%s[CpuOp] dest_wa_index:%d", MAKE_TAB(tap, tab_num), cpu_op->wa_src_dest.dest_wa_index);

    switch(cpu_op->opcode) {
    case VPUL_OP_CTRL_CH_LOOP:
        ExynosVpuCpuOpCtrlChLoop::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_CTRL_SEP_FILT_FROM_FRAC:
        ExynosVpuCpuOpCtrlSepFiltFromFrac::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_LK_STAT_UP_PT_CH_THR:
        ExynosVpuCpuOpLkStatUpPtChThr::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_HW_UPDATE_M2LI_RECORD:
        ExynosVpuCpuOpHwUpdateM2LRecord::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_LK_UPDATE_PT_BY_XY_OFS:
        ExynosVpuCpuOpLkUpdatePtByXyOfs::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS:
        ExynosVpuCpuOpCopyPuResult2PuParam::display_structure_info(tab_num, &cpu_op->params);
        break;
    case VPUL_OP_WRITE_RPRT_PU_RSLTS:
        ExynosVpuCpuOpWriteRprtPuResult::display_structure_info(tab_num, &cpu_op->params);
        break;
    default:
        VXLOGE("unknown opcode, %d", cpu_op->opcode);
        break;
    }
}

void
ExynosVpuCpuOp::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[CpuOp] opcode:%d", MAKE_TAB(tap, tab_num), m_op_info.opcode);
    VXLOGI("%s[CpuOp] src_indication:%d", MAKE_TAB(tap, tab_num), m_op_info.wa_src_dest.src_index_type);
    switch(m_op_info.wa_src_dest.src_index_type) {
    case VPUL_DIRECT_INDEX:
        VXLOGI("%s[CpuOp] src_wa_index:%d", MAKE_TAB(tap, tab_num), m_op_info.wa_src_dest.src_wa_index);
        break;
    case VPUL_PU_INDEX:
        VXLOGI("%s[CpuOp] src_pu_index:%d/%d/%d", MAKE_TAB(tap, tab_num), m_op_info.wa_src_dest.src_pu_index.vtx_idx,
                                                                                                                m_op_info.wa_src_dest.src_pu_index.sc_idx_in_vtx,
                                                                                                                m_op_info.wa_src_dest.src_pu_index.pu_idx_in_sc);
        break;
    default:
        VXLOGE("un-defined indication, %d", m_op_info.wa_src_dest.src_index_type);
        break;
    }
    VXLOGI("%s[CpuOp] dest_wa_index:%d", MAKE_TAB(tap, tab_num), m_op_info.wa_src_dest.dest_wa_index);
}

ExynosVpuCpuOp::ExynosVpuCpuOp(ExynosVpuSubchainCpu *subchain, enum vpul_cpu_op_code op_code)
{
    memset(&m_op_info, 0x0, sizeof(m_op_info));
    m_op_info.opcode = op_code;

    m_subchain = subchain;
    m_op_index_in_subchain = subchain->addCpuOp(this);
}

ExynosVpuCpuOp::ExynosVpuCpuOp(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
{
    m_op_info = *cpu_op_info;

    m_subchain = subchain;
    m_op_index_in_subchain = subchain->addCpuOp(this);
}

status_t
ExynosVpuCpuOp::checkConstraint(void)
{
    return NO_ERROR;
}

struct vpul_cpu_op*
ExynosVpuCpuOp::getCpuOpInfo(void)
{
    return &m_op_info;
}

status_t
ExynosVpuCpuOp::updateStructure(void)
{
    /* do nothing */
    return NO_ERROR;
}

void
ExynosVpuCpuOpCtrlSepFiltFromFrac::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_CTRL_SEP_FILT_FROM_FRAC], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] total_num_of_records:%d", MAKE_TAB(tap, tab_num+1), params->flt_frac.total_num_of_records);
}

void
ExynosVpuCpuOpCtrlSepFiltFromFrac::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpCtrlSepFiltFromFrac::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpCtrlSepFiltFromFrac::ExynosVpuCpuOpCtrlSepFiltFromFrac(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_CTRL_SEP_FILT_FROM_FRAC)
{

}

ExynosVpuCpuOpCtrlSepFiltFromFrac::ExynosVpuCpuOpCtrlSepFiltFromFrac(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpLkStatUpPtChThr::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_LK_STAT_UP_PT_CH_THR], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] up_pt_ch_thr_is_fst:%d", MAKE_TAB(tap, tab_num+1), params->lk_stat.up_pt_ch_thr_is_fst);
    VXLOGI("%s[------] up_pt_ch_thr:%d", MAKE_TAB(tap, tab_num+1), params->lk_stat.up_pt_ch_thr);
}

void
ExynosVpuCpuOpLkStatUpPtChThr::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpLkStatUpPtChThr::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpLkStatUpPtChThr::ExynosVpuCpuOpLkStatUpPtChThr(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_LK_STAT_UP_PT_CH_THR)
{

}

ExynosVpuCpuOpLkStatUpPtChThr::ExynosVpuCpuOpLkStatUpPtChThr(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpCtrlChLoop::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_CTRL_CH_LOOP], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] is_loop_abort:%d", MAKE_TAB(tap, tab_num+1), params->loop.is_loop_abort);
    VXLOGI("%s[------] is_task_loop:%d", MAKE_TAB(tap, tab_num+1), params->loop.is_task_loop);
    VXLOGI("%s[------] loop_index:%d", MAKE_TAB(tap, tab_num+1), params->loop.loop_index);
    VXLOGI("%s[------] loop_offset:%d", MAKE_TAB(tap, tab_num+1), params->loop.loop_offset);
}

void
ExynosVpuCpuOpCtrlChLoop::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpCtrlChLoop::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpCtrlChLoop::ExynosVpuCpuOpCtrlChLoop(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_CTRL_CH_LOOP)
{

}

ExynosVpuCpuOpCtrlChLoop::ExynosVpuCpuOpCtrlChLoop(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpHwUpdateM2LRecord::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [HW_UPDATE_M2LI_RECORD], %p", MAKE_TAB(tap, tab_num), params);
}

void
ExynosVpuCpuOpHwUpdateM2LRecord::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpHwUpdateM2LRecord::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpHwUpdateM2LRecord::ExynosVpuCpuOpHwUpdateM2LRecord(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_HW_UPDATE_M2LI_RECORD)
{

}

ExynosVpuCpuOpHwUpdateM2LRecord::ExynosVpuCpuOpHwUpdateM2LRecord(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpLkUpdatePtByXyOfs::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_LK_UPDATE_PT_BY_XY_OFS], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] is_loop_abort:%d", MAKE_TAB(tap, tab_num+1), params->update_pt_by_xy_offs.cfg_up_pt_ind);
    VXLOGI("%s[------] is_task_loop:%d", MAKE_TAB(tap, tab_num+1), params->update_pt_by_xy_offs.cfg_prev_pt_ind);
}

void
ExynosVpuCpuOpLkUpdatePtByXyOfs::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpLkUpdatePtByXyOfs::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpLkUpdatePtByXyOfs::ExynosVpuCpuOpLkUpdatePtByXyOfs(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_LK_UPDATE_PT_BY_XY_OFS)
{

}

ExynosVpuCpuOpLkUpdatePtByXyOfs::ExynosVpuCpuOpLkUpdatePtByXyOfs(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpCopyPuResult2PuParam::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] dest_pu:%d/%d/%d", MAKE_TAB(tap, tab_num+1), params->cpy_src_dest.dest_pu.pu_idx_in_sc,
                                                                params->cpy_src_dest.dest_pu.sc_idx_in_vtx,
                                                                params->cpy_src_dest.dest_pu.vtx_idx);
    VXLOGI("%s[------] offset_in_dest_pu_prms:%d", MAKE_TAB(tap, tab_num+1), params->cpy_src_dest.offset_in_dest_pu_prms);
    VXLOGI("%s[------] offset_in_src_pu_prms:%d", MAKE_TAB(tap, tab_num+1), params->cpy_src_dest.offset_in_src_pu_prms);
}

void
ExynosVpuCpuOpCopyPuResult2PuParam::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpCopyPuResult2PuParam::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpCopyPuResult2PuParam::ExynosVpuCpuOpCopyPuResult2PuParam(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS)
{

}

ExynosVpuCpuOpCopyPuResult2PuParam::ExynosVpuCpuOpCopyPuResult2PuParam(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

void
ExynosVpuCpuOpWriteRprtPuResult::display_structure_info(uint32_t tab_num, union vpul_cpu_parameters *params)
{
    char tap[MAX_TAB_NUM];

    /* do nothing */
    VXLOGI("%s[------] [VPUL_OP_WRITE_RPRT_PU_RSLTS], %p", MAKE_TAB(tap, tab_num), params);
    VXLOGI("%s[------] num_of_32B_reported_results:%d", MAKE_TAB(tap, tab_num+1), params->num_of_32B_reported_results);
}

void
ExynosVpuCpuOpWriteRprtPuResult::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    ExynosVpuCpuOp::displayObjectInfo(tab_num);
    ExynosVpuCpuOpWriteRprtPuResult::display_structure_info(tab_num, &m_op_info.params);
}

ExynosVpuCpuOpWriteRprtPuResult::ExynosVpuCpuOpWriteRprtPuResult(ExynosVpuSubchainCpu *subchain)
                                        : ExynosVpuCpuOp(subchain, VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS)
{

}

ExynosVpuCpuOpWriteRprtPuResult::ExynosVpuCpuOpWriteRprtPuResult(ExynosVpuSubchainCpu *subchain, const struct vpul_cpu_op *cpu_op_info)
                                        : ExynosVpuCpuOp(subchain, cpu_op_info)
{

}

}; /* namespace android */

