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

#define LOG_TAG "ExynosVpuKernelMinMaxLoc"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuCpuOp.h"

#include "ExynosVpuKernelMinMaxLoc.h"

#include "vpu_kernel_util.h"

#define TASK_API_MODE  1

#include "td-binary-minmaxloc.h"

#define MINIMUM_ARRAY_SIZE  10

namespace android {

using namespace std;

#define ROIS_OUTREG_NUM 6

static vx_uint16 td_binary[] =
    TASK_test_binary_minmaxloc_from_VDE_DS;

vx_status
ExynosVpuKernelMinMaxLoc::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if ((format == VX_DF_IMAGE_U8)
                || (format == VX_DF_IMAGE_S16)
                || (format == VX_DF_IMAGE_U16)
                || (format == VX_DF_IMAGE_U32)
                || (format == VX_DF_IMAGE_S32)) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelMinMaxLoc::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if ((index == 1) || (index == 2)) {
        vx_parameter param = vxGetParameterByIndex(node, 0);
        if (param) {
            vx_image input = NULL;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input) {
                vx_df_image format;
                vx_enum type = VX_TYPE_INVALID;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                switch (format) {
                case VX_DF_IMAGE_U8:
                    type = VX_TYPE_UINT8;
                    break;
                case VX_DF_IMAGE_U16:
                    type = VX_TYPE_UINT16;
                    break;
                case VX_DF_IMAGE_U32:
                    type = VX_TYPE_UINT32;
                    break;
                case VX_DF_IMAGE_S16:
                    type = VX_TYPE_INT16;
                    break;
                case VX_DF_IMAGE_S32:
                    type = VX_TYPE_INT32;
                    break;
                default:
                    type = VX_TYPE_INVALID;
                    break;
                }
                if (type != VX_TYPE_INVALID) {
                    status = VX_SUCCESS;
                    vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    } else if ((index == 3) || (index == 4)) {
        /* nothing to check here */
        vx_enum array_item_type = VX_TYPE_COORDINATES2D;
        vx_size array_capacity = 1;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    } else if ((index == 5) || (index == 6)) {
        vx_enum scalar_type = VX_TYPE_UINT32;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }

    return status;
}

ExynosVpuKernelMinMaxLoc::ExynosVpuKernelMinMaxLoc(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_input_format = VX_DF_IMAGE_VIRT;
    m_min_value = 0;
    m_max_value = 0;

    m_min_array_capacity = 0;
    m_max_array_capacity = 0;

    strcpy(m_task_name, "minmaxloc");
}

ExynosVpuKernelMinMaxLoc::~ExynosVpuKernelMinMaxLoc(void)
{

}

vx_status
ExynosVpuKernelMinMaxLoc::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &m_input_format, sizeof(m_input_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    if (parameters[3]) {
        m_min_loc_enable = vx_true_e;
        status = vxQueryArray((vx_array)parameters[3], VX_ARRAY_ATTRIBUTE_CAPACITY, &m_min_array_capacity, sizeof(m_min_array_capacity));
        if (status != VX_SUCCESS) {
            VXLOGE("querying array fails");
            goto EXIT;
        }
    }

    if (parameters[4]) {
        m_max_loc_enable = vx_true_e;
        status = vxQueryArray((vx_array)parameters[4], VX_ARRAY_ATTRIBUTE_CAPACITY, &m_max_array_capacity, sizeof(m_max_array_capacity));
        if (status != VX_SUCCESS) {
            VXLOGE("querying array fails");
            goto EXIT;
        }
    }

    if (parameters[5])
        m_min_cnt_enable = vx_true_e;

    if (parameters[6])
        m_max_cnt_enable = vx_true_e;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelMinMaxLoc::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if (TASK_API_MODE==0)
    status = initTaskFromBinary();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from binary fails, %p, %p", node, parameters);
        goto EXIT;
    }
#else
    status = initTaskFromApi();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from api fails, %p, %p", node, parameters);
        goto EXIT;
    }
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelMinMaxLoc::initTaskFromBinary(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;

    task_if_0 = initTask0FromBinary();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelMinMaxLoc::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, SW subchain */
    struct vpul_pu *pu;
    pu = fst_pu_ptr((struct vpul_task*)td_binary);

    /* map2list in max array subchain */
    pu[3].n_run_time_params_from_hw = 1;
    /* map2list in min array subchain */
    pu[6].n_run_time_params_from_hw = 1;

    struct vpul_subchain *cpu_subchain;
    cpu_subchain = fst_sc_ptr((struct vpul_task*)td_binary);

    /*describe ROIS pu*/
    struct vpul_pu_location lcl_src_rois;
    lcl_src_rois.vtx_idx = 1;
    lcl_src_rois.sc_idx_in_vtx = 0; /* first SC */
    lcl_src_rois.pu_idx_in_sc = 1;        /* 2nd pu, after DMA */

    /* For map2lst */
    cpu_subchain[1].cpu.FstIntRamInd		= 0;
    cpu_subchain[1].cpu.IntRamIndices		= 0;
    cpu_subchain[1].cpu.CpuRamIndices		= 0;
    cpu_subchain[1].stype = VPUL_SUB_CH_CPU_OP;
    cpu_subchain[1].num_of_cpu_op = 1;

    /*describe map2list pu for max*/
    struct vpul_pu_location lcl_dst_pu_max;
    lcl_dst_pu_max.vtx_idx = 1;
    lcl_dst_pu_max.sc_idx_in_vtx = 2; /* 3rd SC, after ROIS and CPU copy */
    lcl_dst_pu_max.pu_idx_in_sc = 1;        /* 2nd pu, after DMA */

    /* Cpu op that copy max value from rois to map2list */
    cpu_subchain[1].cpu.cpu_op_desc[0].opcode =  VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS ;
    cpu_subchain[1].cpu.cpu_op_desc[0].params.cpy_src_dest.dest_pu = lcl_dst_pu_max;
    cpu_subchain[1].cpu.cpu_op_desc[0].params.cpy_src_dest.offset_in_dest_pu_prms = VPUC_M2LI_THR_HIGH_LSB;        /* HIGH THR on map2list*/
    cpu_subchain[1].cpu.cpu_op_desc[0].params.cpy_src_dest.offset_in_src_pu_prms = VPUC_ROI_OUT_MINMAX_MAX_VALUE; /* take MAX (from ROIS)*/
    cpu_subchain[1].cpu.cpu_op_desc[0].wa_src_dest.src_index_type = VPUL_PU_INDEX;
    cpu_subchain[1].cpu.cpu_op_desc[0].wa_src_dest.src_pu_index = lcl_src_rois;

    cpu_subchain[3].cpu.FstIntRamInd		= 0;
    cpu_subchain[3].cpu.IntRamIndices		= 0;
    cpu_subchain[3].cpu.CpuRamIndices		= 0;
    cpu_subchain[3].stype = VPUL_SUB_CH_CPU_OP;
    cpu_subchain[3].num_of_cpu_op = 1;

    /*describe map2list pu for min*/
    struct vpul_pu_location lcl_dst_pu_min;
    lcl_dst_pu_min.vtx_idx = 1;
    lcl_dst_pu_min.sc_idx_in_vtx = 4; /* 5th SC, after ROIS and CPU copy and map2list and CPU copy*/
    lcl_dst_pu_min.pu_idx_in_sc = 1;        /* 2nd pu, after DMA */

    /* Cpu op that copy min value from rois to map2list */
    cpu_subchain[3].cpu.cpu_op_desc[0].opcode =  VPUL_OP_CPY_PU_RSLTS_2_PU_PRMS ;
    cpu_subchain[3].cpu.cpu_op_desc[0].params.cpy_src_dest.dest_pu = lcl_dst_pu_min;
    cpu_subchain[3].cpu.cpu_op_desc[0].params.cpy_src_dest.offset_in_dest_pu_prms = VPUC_M2LI_THR_HIGH_LSB;        /* HIGH THR on map2list*/
    cpu_subchain[3].cpu.cpu_op_desc[0].params.cpy_src_dest.offset_in_src_pu_prms = 3; // VPUC_ROI_OUT_MINMAX_MAX_VALUE /* take MIN (from ROIS)*/
    cpu_subchain[3].cpu.cpu_op_desc[0].wa_src_dest.src_index_type = VPUL_PU_INDEX;
    cpu_subchain[3].cpu.cpu_op_desc[0].wa_src_dest.src_pu_index = lcl_src_rois;

    /* SC_1 is SW subchain for report result */
    cpu_subchain[5].stype = VPUL_SUB_CH_CPU_OP;
    cpu_subchain[5].num_of_cpu_op = 1;
    cpu_subchain[5].cpu.FstIntRamInd = 0;
    cpu_subchain[5].cpu.IntRamIndices = 0;
    cpu_subchain[5].cpu.CpuRamIndices = 0;

    /* Cpu op that copy max value from rois to map2list */
    cpu_subchain[5].cpu.cpu_op_desc[0].opcode =  VPUL_OP_WRITE_RPRT_PU_RSLTS ;
    cpu_subchain[5].cpu.cpu_op_desc[0].wa_src_dest.src_index_type = VPUL_PU_INDEX;
    cpu_subchain[5].cpu.cpu_op_desc[0].wa_src_dest.src_pu_index = lcl_src_rois;
    cpu_subchain[5].cpu.cpu_op_desc[0].params.num_of_32B_reported_results = ROIS_OUTREG_NUM; /* single 32B pass to report */
#endif

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_IN, 2, (uint32_t)5);

    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)4);
    task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)7);
    task_if->setIoPu(VS4L_DIRECTION_OT, 2, (uint32_t)1);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelMinMaxLoc::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;
    ExynosVpuTaskIf *task_if_1;
    ExynosVpuTaskIf *task_if_2;

    task_if_0 = initTask0FromApi();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

    task_if_1 = initTask1FromApi();
    if (task_if_1 == NULL) {
        VXLOGE("task1 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

    task_if_2 = initTask2FromApi();
    if (task_if_2 == NULL) {
        VXLOGE("task2 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

    status = createStrFromObjectOfTask();
    if (status != VX_SUCCESS) {
        VXLOGE("creating task str fails");
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelMinMaxLoc::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperRois *task_wr = new ExynosVpuTaskIfWrapperRois(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *rois_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, rois_process);
    ExynosVpuVertex::connect(rois_process, end_vertex);

    /* define subchain */
    ExynosVpuSubchainHw *rois_subchain = new ExynosVpuSubchainHw(rois_process);
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(rois_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(rois_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *rois = pu_factory.createPu(rois_subchain, VPU_PU_ROIS0);
    rois->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, rois, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_rois_out *rois_param = (struct vpul_pu_rois_out*)rois->getParameter();

    rois_param->work_mode = 2;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(rois_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(rois_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, rois);
    if (ret != NO_ERROR) {
        VXLOGE("connectting pu to io fails");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelMinMaxLoc::initTask1FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperMaxArray *task_wr = new ExynosVpuTaskIfWrapperMaxArray(this, 1);
    status = setTaskIfWrapper(1, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *max_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, max_process);
    ExynosVpuVertex::connect(max_process, end_vertex);

    /* define subchain */
    ExynosVpuSubchainHw *max_subchain = new ExynosVpuSubchainHw(max_process);
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(max_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(max_subchain, VPU_PU_DMAIN_MNM0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(max_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *map2list = pu_factory.createPu(max_subchain, VPU_PU_MAP2LIST);
    map2list->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(max_subchain, VPU_PU_DMAOT_WIDE1);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, salb, 0);
    ExynosVpuPu::connect(salb, 0, map2list, 0);
    ExynosVpuPu::connect(map2list, 1, dma_out, 0);

    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
    salb_param->operation_code = 16;
    salb_param->input_enable = 1;
    salb_param->val_hi = 255;
    salb_param->val_lo = 0;

    struct vpul_pu_map2list *map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();
    map2list_param->value_in = 1;
    map2list_param->enable_out_lst = 1;
    map2list_param->inout_indx = 1;
    map2list_param->threshold_high = 255;
    map2list_param->threshold_low = 255;

    /* define SW subchain */
    ExynosVpuSubchainCpu *sw_subchain = new ExynosVpuSubchainCpu(max_process);
    ExynosVpuCpuOpHwUpdateM2LRecord *update_m2li = new ExynosVpuCpuOpHwUpdateM2LRecord(sw_subchain);

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    /* define updatable pu */
    ExynosVpuUpdatablePu *updatable_pu = new ExynosVpuUpdatablePu(task, salb);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(max_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(max_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(max_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(max_process, fixed_roi);
    dma_out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out);
    if (ret != NO_ERROR) {
        VXLOGE("connectting pu to io fails");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelMinMaxLoc::initTask2FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperMinArray *task_wr = new ExynosVpuTaskIfWrapperMinArray(this, 2);
    status = setTaskIfWrapper(2, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *min_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, min_process);
    ExynosVpuVertex::connect(min_process, end_vertex);

    /* define subchain */
    ExynosVpuSubchainHw *min_subchain = new ExynosVpuSubchainHw(min_process);
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(min_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(min_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(min_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *map2list = pu_factory.createPu(min_subchain, VPU_PU_MAP2LIST);
    map2list->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(min_subchain, VPU_PU_DMAOT_WIDE1);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, salb, 0);
    ExynosVpuPu::connect(salb, 0, map2list, 0);
    ExynosVpuPu::connect(map2list, 1, dma_out, 0);

    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
    salb_param->operation_code = 16;
    salb_param->input_enable = 1;
    salb_param->val_hi = 0;
    salb_param->val_lo = 255;

    struct vpul_pu_map2list *map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();
    map2list_param->value_in = 1;
    map2list_param->enable_out_lst = 1;
    map2list_param->inout_indx = 1;
    map2list_param->threshold_high = 255;
    map2list_param->threshold_low = 255;

    /* define SW subchain */
    ExynosVpuSubchainCpu *sw_subchain = new ExynosVpuSubchainCpu(min_process);
    ExynosVpuCpuOpHwUpdateM2LRecord *update_m2li = new ExynosVpuCpuOpHwUpdateM2LRecord(sw_subchain);

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    /* define updatable pu */
    ExynosVpuUpdatablePu *updatable_pu = new ExynosVpuUpdatablePu(task, salb);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(min_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(min_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(min_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(min_process, fixed_roi);
    dma_out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out);
    if (ret != NO_ERROR) {
        VXLOGE("connectting pu to io fails");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelMinMaxLoc::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    struct vpul_pu_map2list *map2list_param;

#if (TASK_API_MODE==0)
    ExynosVpuPu *map2list_max;
    map2list_max = getTask(0)->getPu(VPU_PU_MAP2LIST, 1, 2);
    if (map2list_max == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    map2list_param = (struct vpul_pu_map2list*)map2list_max->getParameter();
    map2list_param->num_of_point = m_max_array_capacity;

    ExynosVpuPu *map2list_min;
    map2list_min = getTask(0)->getPu(VPU_PU_MAP2LIST, 1, 4);
    if (map2list_min == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    map2list_param = (struct vpul_pu_map2list*)map2list_min->getParameter();
    map2list_param->num_of_point = m_min_array_capacity;
#else   // #if (TASK_API_MODE==0)
    ExynosVpuPu *map2list_max;
    map2list_max = getTask(1)->getPu(VPU_PU_MAP2LIST, 1, 0);
    if (map2list_max == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    map2list_param = (struct vpul_pu_map2list*)map2list_max->getParameter();
    map2list_param->num_of_point = m_max_array_capacity;

    ExynosVpuPu *map2list_min;
    map2list_min = getTask(2)->getPu(VPU_PU_MAP2LIST, 1, 0);
    if (map2list_min == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    map2list_param = (struct vpul_pu_map2list*)map2list_min->getParameter();
    map2list_param->num_of_point = m_min_array_capacity;
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelMinMaxLoc::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

#if (TASK_API_MODE==0)
    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    /* input image for rois subchain */
    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* input image for first map2list subchain */
    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 1, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* input image for second map2list subchain */
    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 2, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    vx_uint32 capacity;
    vx_uint32 byte_per_record, byte_per_line, line_number;
    vx_uint32 total_array_size;
    vx_uint32 width;
    vx_uint32 height;

    /* max array */
    capacity = m_max_array_capacity ? m_max_array_capacity : MINIMUM_ARRAY_SIZE;

    byte_per_record = sizeof(struct key_triple);  /* 6B, (x, y, val) , 16-bit, 16-bit, 16-bit */
    byte_per_line = (byte_per_record==8) ? 240 : 252;
    line_number = ceilf((float)byte_per_record * (float)capacity / (float)byte_per_line);

    total_array_size = byte_per_line * line_number + 2;
    if (total_array_size > 2000) {
        width = 2000;
        height = (total_array_size/width) + 1;
    } else {
        width = total_array_size;
        height = 1;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = width;
    io_format.height = height;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* min array */
    capacity = m_min_array_capacity ? m_min_array_capacity : MINIMUM_ARRAY_SIZE;

    byte_per_record = sizeof(struct key_triple);  /* 6B, (x, y, val) , 16-bit, 16-bit, 16-bit */
    byte_per_line = (byte_per_record==8) ? 240 : 252;
    line_number = ceilf((float)byte_per_record * (float)capacity / (float)byte_per_line);

    total_array_size = byte_per_line * line_number + 2;
    if (total_array_size > 2000) {
        width = 2000;
        height = (total_array_size/width) + 1;
    } else {
        width = total_array_size;
        height = 1;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = width;
    io_format.height = height;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 1, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* rois result report */
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = ROIS_OUTREG_NUM;
    io_format.height = 1;
    io_format.pixel_byte = 4;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 2, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
#else   // #if (TASK_API_MODE==0)
    ExynosVpuTaskIfWrapper *task_wr_0, *task_wr_1, *task_wr_2;

    task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    /* input image for rois subchain */
    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    task_wr_1 = m_task_wr_list[1];
    /* input image for first map2list subchain */
    param_info.image.plane = 0;
    status = task_wr_1->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    task_wr_2 = m_task_wr_list[2];
    /* input image for second map2list subchain */
    param_info.image.plane = 0;
    status = task_wr_2->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    /* rois result report */
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = ROIS_OUTREG_NUM;
    io_format.height = 1;
    io_format.pixel_byte = 4;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    vx_uint32 capacity;
    vx_uint32 byte_per_record, byte_per_line, line_number;
    vx_uint32 total_array_size;
    vx_uint32 width;
    vx_uint32 height;

    /* max array */
    capacity = m_max_array_capacity ? m_max_array_capacity : MINIMUM_ARRAY_SIZE;

    byte_per_record = sizeof(struct key_triple);  /* 6B, (x, y, val) , 16-bit, 16-bit, 16-bit */
    byte_per_line = (byte_per_record==8) ? 240 : 252;
    line_number = ceilf((float)byte_per_record * (float)capacity / (float)byte_per_line);

    total_array_size = byte_per_line * line_number + 2;
    if (total_array_size > 2000) {
        width = 2000;
        height = (total_array_size/width) + 1;
    } else {
        width = total_array_size;
        height = 1;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = width;
    io_format.height = height;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_1->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* min array */
    capacity = m_min_array_capacity ? m_min_array_capacity : MINIMUM_ARRAY_SIZE;

    byte_per_record = sizeof(struct key_triple);  /* 6B, (x, y, val) , 16-bit, 16-bit, 16-bit */
    byte_per_line = (byte_per_record==8) ? 240 : 252;
    line_number = ceilf((float)byte_per_record * (float)capacity / (float)byte_per_line);

    total_array_size = byte_per_line * line_number + 2;
    if (total_array_size > 2000) {
        width = 2000;
        height = (total_array_size/width) + 1;
    } else {
        width = total_array_size;
        height = 1;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = width;
    io_format.height = height;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_2->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
#endif

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperRois::ExynosVpuTaskIfWrapperRois(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperRois::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_scalar vx_min_scalar;
    vx_scalar vx_max_scalar;
    vx_min_scalar = (vx_scalar)parameters[1];
    vx_max_scalar = (vx_scalar)parameters[2];

    const io_buffer_info_t *io_buffer_info;
    vx_uint32 *result_array;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    result_array = (vx_uint32*)io_buffer_info->addr;

    vx_uint8 min_value_u8, max_value_u8;
    vx_int16 min_value_s16, max_value_s16;

    min_value_u8 = (vx_uint8)result_array[3];
    max_value_u8 = (vx_uint8)result_array[4];
    min_value_s16 = (vx_int16)result_array[3];
    max_value_s16 = (vx_int16)result_array[4];

    ExynosVpuKernelMinMaxLoc *kernel;
    kernel = (ExynosVpuKernelMinMaxLoc*)getKernel();
    kernel->reportMinValue(min_value_s16);
    kernel->reportMaxValue(max_value_s16);

    switch (kernel->getInputFormat()) {
    case VX_DF_IMAGE_U8:
        status |= vxWriteScalarValue(vx_min_scalar, &min_value_u8);
        status |= vxWriteScalarValue(vx_max_scalar, &max_value_u8);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
            goto EXIT;
        }
        break;
    case VX_DF_IMAGE_S16:
        status |= vxWriteScalarValue(vx_min_scalar, &min_value_s16);
        status |= vxWriteScalarValue(vx_max_scalar, &max_value_s16);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
            goto EXIT;
        }
        break;
    default:
        VXLOGE("unsupported type, 0x%x", kernel->getInputFormat());
        break;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperMaxArray::ExynosVpuTaskIfWrapperMaxArray(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperMaxArray::preProcessTask(const vx_reference parameters[])
{
    status_t ret = NO_ERROR;
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    ExynosVpuPu *salb_max;
    salb_max = m_task_if->getTask()->getPu(VPU_PU_SALB0, 1, 0);
    if (salb_max == NULL) {
        VXLOGE("getting pu fails, %p", parameters);
        ret = INVALID_OPERATION;
        goto EXIT;
    }
    struct vpul_pu_salb *salb_param;
    salb_param = (struct vpul_pu_salb*)salb_max->getParameter();

    ExynosVpuKernelMinMaxLoc *kernel;
    kernel = (ExynosVpuKernelMinMaxLoc*)getKernel();
    vx_df_image input_image;
    input_image = kernel->getInputFormat();
    if (input_image == VX_DF_IMAGE_U8) {
        salb_param->thresh_lo = (vx_uint8)kernel->getMaxValue();
        salb_param->thresh_hi = (vx_uint8)kernel->getMaxValue();
    } else {
        salb_param->thresh_lo = kernel->getMaxValue();
        salb_param->thresh_hi = kernel->getMaxValue();
    }

    if (salb_param->thresh_lo != 0) {
        salb_param->thresh_lo--;
        salb_param->thresh_hi--;
    }

    salb_max->insertPuToTaskDescriptor();

    ret = m_task_if->driverNodeSetParam(salb_max->getTargetId(), salb_param, sizeof(union vpul_pu_parameters));
    if (ret != NO_ERROR) {
        VXLOGE("setting param fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

vx_status
ExynosVpuTaskIfWrapperMaxArray::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_array vx_max_array_object;
    vx_scalar num_max_corners_scalar;
    vx_max_array_object = (vx_array)parameters[4];
    num_max_corners_scalar = (vx_scalar)parameters[6];

    vx_size capacity;
    vx_coordinates2d_t *keypoint_array;
    if (vx_max_array_object) {
        status = vxQueryArray(vx_max_array_object, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
        if (status != NO_ERROR) {
            VXLOGE("querying array fails");
            goto EXIT;
        }

        status = vxAddEmptyArrayItems(vx_max_array_object, capacity);
        if (status != NO_ERROR) {
            VXLOGE("adding empty array fails");
            goto EXIT;
        }

        vx_size array_stride;
        keypoint_array = NULL;
        array_stride = 0;
        status = vxAccessArrayRange(vx_max_array_object, 0, capacity, &array_stride, (void**)&keypoint_array, VX_WRITE_ONLY);
        if (status != NO_ERROR) {
            VXLOGE("accessing array fails");
            goto EXIT;
        }
        if (array_stride != sizeof(vx_coordinates2d_t)) {
            VXLOGE("stride is not matching, %d, %d", array_stride, sizeof(vx_coordinates2d_t));
            status = VX_FAILURE;
            goto EXIT;
        }
    } else {
        capacity = 0;
        keypoint_array = NULL;
    }

    if (m_out_io_param_info[0].type != IO_PARAM_CUSTOM) {
        VXLOGE("io type is not matching");
        status = VX_FAILURE;
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    struct key_triple *keytriple_array;
    vx_uint32 keytriple_array_size;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];

    /* two byte for blank is made by firmware */
    vx_uint32 found_point_num;
    found_point_num = *((vx_uint16*)io_buffer_info->addr);

    if (vx_max_array_object) {
        keytriple_array = (struct key_triple*)(io_buffer_info->addr+2);
        vx_uint32 i;
        for (i=0; i<found_point_num; i++) {
            keypoint_array[i].x = keytriple_array[i].x;
            keypoint_array[i].y = keytriple_array[i].y;
        }

        status = vxCommitArrayRange(vx_max_array_object, 0, capacity, keypoint_array);
        if (status != NO_ERROR) {
            VXLOGE("commit array fails");
            goto EXIT;
        }

        if (found_point_num != capacity)
            vxTruncateArray(vx_max_array_object, found_point_num);
    }

    if (num_max_corners_scalar) {
        status = vxWriteScalarValue(num_max_corners_scalar, &found_point_num);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
            goto EXIT;
        }
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperMinArray::ExynosVpuTaskIfWrapperMinArray(ExynosVpuKernelMinMaxLoc *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperMinArray::preProcessTask(const vx_reference parameters[])
{
    status_t ret = NO_ERROR;
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

   ExynosVpuPu *salb_min;
    salb_min = m_task_if->getTask()->getPu(VPU_PU_SALB0, 1, 0);
    if (salb_min == NULL) {
        VXLOGE("getting pu fails, %p", parameters);
        ret = INVALID_OPERATION;
        goto EXIT;
    }
    struct vpul_pu_salb *salb_param;
    salb_param = (struct vpul_pu_salb*)salb_min->getParameter();

    ExynosVpuKernelMinMaxLoc *kernel;
    kernel = (ExynosVpuKernelMinMaxLoc*)getKernel();
    vx_df_image input_image;
    input_image = kernel->getInputFormat();
    if (input_image == VX_DF_IMAGE_U8) {
        salb_param->thresh_lo = (vx_uint8)kernel->getMinValue();
        salb_param->thresh_hi = (vx_uint8)kernel->getMinValue();
    } else {
        salb_param->thresh_lo = kernel->getMinValue();
        salb_param->thresh_hi = kernel->getMinValue();
    }
    if (salb_param->thresh_lo != 255) {
        salb_param->thresh_lo++;
        salb_param->thresh_hi++;
    }

    salb_min->insertPuToTaskDescriptor();

    ret = m_task_if->driverNodeSetParam(salb_min->getTargetId(), salb_param, sizeof(union vpul_pu_parameters));
    if (ret != NO_ERROR) {
        VXLOGE("setting param fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

vx_status
ExynosVpuTaskIfWrapperMinArray::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_array vx_min_array_object;
    vx_scalar num_min_corners_scalar;
    vx_min_array_object = (vx_array)parameters[3];
    num_min_corners_scalar = (vx_scalar)parameters[5];

    vx_size capacity;
    vx_coordinates2d_t *keypoint_array;
    if (vx_min_array_object) {
        status = vxQueryArray(vx_min_array_object, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
        if (status != NO_ERROR) {
            VXLOGE("querying array fails");
            goto EXIT;
        }

        status = vxAddEmptyArrayItems(vx_min_array_object, capacity);
        if (status != NO_ERROR) {
            VXLOGE("adding empty array fails");
            goto EXIT;
        }

        vx_size array_stride;
        keypoint_array = NULL;
        array_stride = 0;
        status = vxAccessArrayRange(vx_min_array_object, 0, capacity, &array_stride, (void**)&keypoint_array, VX_WRITE_ONLY);
        if (status != NO_ERROR) {
            VXLOGE("accessing array fails");
            goto EXIT;
        }
        if (array_stride != sizeof(vx_coordinates2d_t)) {
            VXLOGE("stride is not matching, %d, %d", array_stride, sizeof(vx_coordinates2d_t));
            status = VX_FAILURE;
            goto EXIT;
        }
    } else {
        capacity = 0;
        keypoint_array = NULL;
    }

    if (m_out_io_param_info[0].type != IO_PARAM_CUSTOM) {
        VXLOGE("io type is not matching");
        status = VX_FAILURE;
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    struct key_triple *keytriple_array;
    vx_uint32 keytriple_array_size;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];

    /* two byte for blank is made by firmware */
    vx_uint32 found_point_num;
    found_point_num = *((vx_uint16*)io_buffer_info->addr);

    if (vx_min_array_object) {
        keytriple_array = (struct key_triple*)(io_buffer_info->addr+2);
        vx_uint32 i;
        for (i=0; i<found_point_num; i++) {
            keypoint_array[i].x = keytriple_array[i].x;
            keypoint_array[i].y = keytriple_array[i].y;
        }

        status = vxCommitArrayRange(vx_min_array_object, 0, capacity, keypoint_array);
        if (status != NO_ERROR) {
            VXLOGE("commit array fails");
            goto EXIT;
        }

        if (found_point_num != capacity)
            vxTruncateArray(vx_min_array_object, found_point_num);
    }

    if (num_min_corners_scalar) {
        status = vxWriteScalarValue(num_min_corners_scalar, &found_point_num);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
            goto EXIT;
        }
    }

EXIT:
    return status;
}

}; /* namespace android */

