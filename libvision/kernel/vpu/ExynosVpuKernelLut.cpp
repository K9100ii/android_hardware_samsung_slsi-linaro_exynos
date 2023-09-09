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

#define LOG_TAG "ExynosVpuKernelLut"
#include <cutils/log.h>

#include "ExynosVpuKernelLut.h"

#include "vpu_kernel_util.h"

#include "td-binary-lut.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_tablelookup_from_VDE_DS;

vx_status
ExynosVpuKernelLut::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8 || format == VX_DF_IMAGE_S16) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);

    } else if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        vx_lut lut = 0;
        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &lut, sizeof(lut));
        if (lut) {
            vx_enum type = 0;
            vxQueryLUT(lut, VX_LUT_ATTRIBUTE_TYPE, &type, sizeof(type));
            if (type == VX_TYPE_UINT8 || type == VX_TYPE_INT16) {
                status = VX_SUCCESS;
            }
            vxReleaseLUT(&lut);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelLut::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 2) {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        if (src_param) {
            vx_image src = 0;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            if (src) {
                vx_uint32 width = 0, height = 0;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                /* output is equal type and size */
                vx_df_image format = VX_DF_IMAGE_U8;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                status = VX_SUCCESS;
                vxReleaseImage(&src);
            }
            vxReleaseParameter(&src_param);
        }
    }

    return status;
}

ExynosVpuKernelLut::ExynosVpuKernelLut(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "lut");
}

ExynosVpuKernelLut::~ExynosVpuKernelLut(void)
{

}

vx_status
ExynosVpuKernelLut::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelLut::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelLut::initTaskFromBinary(void)
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
ExynosVpuKernelLut::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, VDE doesn't support preload_pu memmap */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary)->memmap_desc[0]);
    memmap[1].mtype = VPUL_MEM_PRELOAD_PU;
    memmap[1].pu_index.pu = 1;
    memmap[1].pu_index.sc = 1;
    memmap[1].pu_index.proc = 1;
    memmap[1].pu_index.ports_bit_map = 0x1;
    memmap[1].image_sizes.width = 256;
    memmap[1].image_sizes.height = 1;
    memmap[1].image_sizes.pixel_bytes = 2;
    memmap[1].image_sizes.line_offset = memmap[1].image_sizes.width * memmap[1].image_sizes.pixel_bytes;

    /* JUN_TBD, remove un-used external_mem */
    ((struct vpul_task*)td_binary)->n_external_mem_addresses = 3;
    memmap[2].index = 1;
    memmap[3].index = 2;
#endif

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapperLut(this, 0);
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
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)4);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelLut::initTaskFromApi(void)
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
ExynosVpuKernelLut::initTask0FromApi(void)
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
    ExynosVpuProcess *lut_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, lut_process);
    ExynosVpuVertex::connect(lut_process, end_vertex);

    ExynosVpuIoSizeInout *iosize_table = new ExynosVpuIoSizeInout(lut_process);
    ExynosVpuIoSizeInout *iosize_image = new ExynosVpuIoSizeInout(lut_process);

    /* define subchain */
    ExynosVpuSubchainHw *table_subchain = new ExynosVpuSubchainHw(lut_process);
    ExynosVpuSubchainHw *lut_subchain = new ExynosVpuSubchainHw(lut_process);

    /* table preloading subchain */
    ExynosVpuPu *dma_in_table = pu_factory.createPu(table_subchain, VPU_PU_DMAIN0);
    dma_in_table->setSize(iosize_table, iosize_table);
    ExynosVpuPu *dma_out_table = pu_factory.createPu(table_subchain, VPU_PU_DMAOT0);
    dma_out_table->setSize(iosize_table, iosize_table);

    ExynosVpuPu::connect(dma_in_table, 0, dma_out_table, 0);

    /* lut_subchain */
    /* define pu */
    ExynosVpuPu *dma_in_image = pu_factory.createPu(lut_subchain, VPU_PU_DMAIN0);
    dma_in_image->setSize(iosize_image, iosize_image);
    ExynosVpuPu *lut = pu_factory.createPu(lut_subchain, VPU_PU_LUT);
    lut->setSize(iosize_image, iosize_image);
    ExynosVpuPu *dma_out_image = pu_factory.createPu(lut_subchain, VPU_PU_DMAOT0);
    dma_out_image->setSize(iosize_image, iosize_image);

    ExynosVpuPu::connect(dma_in_image, 0, lut, 0);
    ExynosVpuPu::connect(lut, 0, dma_out_image, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in_image->getParameter();
    struct vpul_pu_lut *lut_param = (struct vpul_pu_lut*)lut->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out_image->getParameter();

    lut_param->interpolation_mode = 1;
    lut_param->lut_size = 0;
    lut_param->signed_in0 = 0;
    lut_param->signed_out0 = 0;
    lut_param->offset = 0;
    lut_param->binsize = 1;
    lut_param->inverse_binsize = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap_external;
    ExynosVpuMemmapPreloadPu *memmap_preload;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap_external = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(lut_process, memmap_external);
    iotyps = new ExynosVpuIoTypesDesc(lut_process, fixed_roi);
    dma_in_table->setIoTypesDesc(iotyps);

    memmap_preload = new ExynosVpuMemmapPreloadPu(task, lut);
    fixed_roi = new ExynosVpuIoFixedMapRoi(lut_process, memmap_preload);
    iotyps = new ExynosVpuIoTypesDesc(lut_process, fixed_roi);
    dma_out_table->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap_external = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(lut_process, memmap_external);
    iotyps = new ExynosVpuIoTypesDesc(lut_process, fixed_roi);
    dma_in_image->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap_external = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(lut_process, memmap_external);
    iotyps = new ExynosVpuIoTypesDesc(lut_process, fixed_roi);
    dma_out_image->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in_image);
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 1, dma_in_table);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out_image);
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
ExynosVpuKernelLut::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelLut::initVxIo(const vx_reference *parameters)
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

    /* table width is 16-bit */
    struct io_format_t io_format;
    io_format.format = VS4L_DF_IMAGE_U16;
    io_format.plane = 0;
    io_format.width = 256;
    io_format.height = 1;
    io_format.pixel_byte = 2;

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
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_IN, 1, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 2, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperLut::ExynosVpuTaskIfWrapperLut(ExynosVpuKernelLut *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperLut::preProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_lut vx_lut_object;
    vx_lut_object = (vx_lut)parameters[1];

    vx_uint8 *vx_lut_array;
    vx_lut_array = NULL;
    status = vxAccessLUT(vx_lut_object, (void**)&vx_lut_array, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing lut fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    vx_uint16 *vpu_lut_array;
    io_buffer_info = &m_in_io_param_info[1].custom_param.io_buffer_info[0];
    vpu_lut_array = (vx_uint16*)io_buffer_info->addr;

    for (vx_uint32 i=0; i<256; i++)
        vpu_lut_array[i] = vx_lut_array[i];

    status = vxCommitLUT(vx_lut_object, vx_lut_array);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting lut fails");
        goto EXIT;
    }

EXIT:
    return status;
}

}; /* namespace android */

