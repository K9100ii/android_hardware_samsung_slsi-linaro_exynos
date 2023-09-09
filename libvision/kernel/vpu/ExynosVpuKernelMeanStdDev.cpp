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

#define LOG_TAG "ExynosVpuKernelMeanStdDev"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuKernelMeanStdDev.h"

#include "vpu_kernel_util.h"

#include "td-binary-meanstddev.h"

namespace android {

using namespace std;

#define ROIS_OUTREG_NUM 6

static vx_uint16 td_binary[] =
    TASK_test_binary_meanstddev_from_VDE_DS;

vx_status
ExynosVpuKernelMeanStdDev::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        vx_image image = 0;
        vx_df_image format = 0;

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
        if (image == NULL) {
            status = VX_ERROR_INVALID_PARAMETERS;
        } else {
            status = VX_SUCCESS;
            vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format != VX_DF_IMAGE_U8 && format != VX_DF_IMAGE_U16 ) {
                status = VX_ERROR_INVALID_PARAMETERS;
            }
            vxReleaseImage(&image);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelMeanStdDev::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1 || index == 2) {
        vx_enum scalar_type = VX_TYPE_FLOAT32;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    } else {
        VXLOGE("index is out of bound, %p, %d, %p", node, index, meta);
    }

    return status;
}

ExynosVpuKernelMeanStdDev::ExynosVpuKernelMeanStdDev(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "meanstddev");
}

ExynosVpuKernelMeanStdDev::~ExynosVpuKernelMeanStdDev(void)
{

}

vx_status
ExynosVpuKernelMeanStdDev::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVpuKernelMeanStdDev::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if 1
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
ExynosVpuKernelMeanStdDev::initTaskFromBinary(void)
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
ExynosVpuKernelMeanStdDev::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, SW subchain */
    struct vpul_subchain *subchain;
    subchain = fst_sc_ptr((struct vpul_task*)td_binary);

    /* SC_1 is SW subchain for report result */
    subchain[1].stype = VPUL_SUB_CH_CPU_OP;
    subchain[1].num_of_cpu_op = 1;
    subchain[1].cpu.FstIntRamInd = 0;
    subchain[1].cpu.IntRamIndices = 0;
    subchain[1].cpu.CpuRamIndices = 0;

    struct vpul_pu_location lcl_src_pu;

    /*describe ROIS pu*/
    lcl_src_pu.vtx_idx = 1;
    lcl_src_pu.sc_idx_in_vtx = 0; /* first SC */
    lcl_src_pu.pu_idx_in_sc = 1;        /* 2nd pu, after DMA */

    /* Cpu op that copy max value from rois to map2list */
    subchain[1].cpu.cpu_op_desc[0].opcode =  VPUL_OP_WRITE_RPRT_PU_RSLTS ;
    subchain[1].cpu.cpu_op_desc[0].wa_src_dest.src_index_type = VPUL_PU_INDEX;
    subchain[1].cpu.cpu_op_desc[0].wa_src_dest.src_pu_index = lcl_src_pu;
    subchain[1].cpu.cpu_op_desc[0].params.num_of_32B_reported_results = ROIS_OUTREG_NUM; /* single 32B pass to report */
#endif

    ExynosVpuTaskIfWrapperMeanStdDev *task_wr = new ExynosVpuTaskIfWrapperMeanStdDev(this, 0);
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
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)1);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelMeanStdDev::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;

    task_if_0 = initTask0FromApi();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
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
ExynosVpuKernelMeanStdDev::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
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

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(rois_process);

    /* define subchain */
    ExynosVpuSubchainHw *rois_subchain = new ExynosVpuSubchainHw(rois_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(rois_subchain, VPU_PU_DMAIN0);
    ExynosVpuPu *rois = pu_factory.createPu(rois_subchain, VPU_PU_ROIS0);

    ExynosVpuPu::connect(dma_in, 0, rois, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_rois_out *rois_param = (struct vpul_pu_rois_out*)rois->getParameter();

    rois_param->work_mode = 1;

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
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 1, rois);
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
ExynosVpuKernelMeanStdDev::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    /* do nothing */

EXIT:
    return status;
}

vx_status
ExynosVpuKernelMeanStdDev::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    struct io_format_t io_format;
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = ROIS_OUTREG_NUM;
    io_format.height = 1;
    io_format.pixel_byte = 4;

    struct io_memory_t io_memory;
    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    io_buffer_info_t io_buffer_info;
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

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapperMeanStdDev::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_scalar vx_mean_scalar;
    vx_scalar vx_stddev_scalar;
    vx_mean_scalar = (vx_scalar)parameters[1];
    vx_stddev_scalar = (vx_scalar)parameters[2];

    const io_buffer_info_t *io_buffer_info;
    vx_uint32 *result_array;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    result_array = (vx_uint32*)io_buffer_info->addr;

    vx_float32 mean, stddev;
    mean = (vx_float32)((vx_uint64)result_array[3] << 32 | result_array[2]) / (vx_float32)result_array[4];
    stddev = (vx_float32)((vx_uint64)result_array[1] << 32 | result_array[0]) / (vx_float32)result_array[4];

    status |= vxWriteScalarValue(vx_mean_scalar, &mean);
    status |= vxWriteScalarValue(vx_stddev_scalar, &stddev);
    if (status != VX_SUCCESS) {
        VXLOGE("writing scalar value fails");
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperMeanStdDev::ExynosVpuTaskIfWrapperMeanStdDev(ExynosVpuKernel *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

}; /* namespace android */

