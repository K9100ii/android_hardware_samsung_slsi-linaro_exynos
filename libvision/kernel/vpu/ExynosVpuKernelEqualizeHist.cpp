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

#define LOG_TAG "ExynosVpuKernelEqualizeHist"
#include <cutils/log.h>

#include "ExynosVpuKernelEqualizeHist.h"

#include "vpu_kernel_util.h"

#include "td-binary-histogram.h"
#include "td-binary-lut.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_0[] =
    TASK_test_binary_histogram_from_VDE_DS;
static vx_uint16 td_binary_1[] =
    TASK_test_binary_tablelookup_from_VDE_DS;

vx_status
ExynosVpuKernelEqualizeHist::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelEqualizeHist::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, 0); /* we reference the input image */

        if (param) {
            vx_image input = 0;

            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input) {
                vx_uint32 width = 0, height = 0;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vx_df_image meta_format = VX_DF_IMAGE_U8;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

ExynosVpuKernelEqualizeHist::ExynosVpuKernelEqualizeHist(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_histogram_ptr = NULL;
    m_total_pixel_num = 0;
    strcpy(m_task_name, "equalize");
}

ExynosVpuKernelEqualizeHist::~ExynosVpuKernelEqualizeHist(void)
{

}

vx_status
ExynosVpuKernelEqualizeHist::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        goto EXIT;
    }

    vx_uint32 width, height;
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
        goto EXIT;
    }

    m_total_pixel_num = width * height;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelEqualizeHist::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelEqualizeHist::initTaskFromBinary(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0, *task_if_1;

    task_if_0 = initTask0FromBinary();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

    task_if_1 = initTask1FromBinary();
    if (task_if_1 == NULL) {
        VXLOGE("task1 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelEqualizeHist::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, VDE doesn't support preload_pu memmap */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary_0)->memmap_desc[0]);
    memmap[1].mtype = VPUL_MEM_PRELOAD_PU;
    memmap[1].pu_index.pu = 1;
    memmap[1].pu_index.sc = 0;
    memmap[1].pu_index.proc = 1;
    memmap[1].pu_index.ports_bit_map = 0x3; /* Reading from MPRBs used for ports 0,1*/
    memmap[1].image_sizes.width = 4;
    memmap[1].image_sizes.height = 1024;
    memmap[1].image_sizes.pixel_bytes = 1;
    memmap[1].image_sizes.line_offset = memmap[1].image_sizes.width * memmap[1].image_sizes.pixel_bytes;
    memmap[2].index = 1;

    /* JUN_TBD, remove un-used external_mem */
    ((struct vpul_task*)td_binary_0)->n_external_mem_addresses = 2;

    /* JUN_TBD, change binsize to 1 */
    struct vpul_pu *histogram;
    histogram = pu_ptr((struct vpul_task*)td_binary_0, 1);
    histogram->params.histogram.inverse_binsize = 65535;
#endif

#if (HOST_HISTOGRAM_PROCESS==1)
    ExynosVpuTaskIfWrapperEqualHist *task_wr = new ExynosVpuTaskIfWrapperEqualHist(this, 0);
#else
    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
#endif
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_0);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)3);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelEqualizeHist::initTask1FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, VDE doesn't support preload_pu memmap */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary_1)->memmap_desc[0]);
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
    ((struct vpul_task*)td_binary_1)->n_external_mem_addresses = 3;
    memmap[2].index = 1;
    memmap[3].index = 2;
#endif

    ExynosVpuTaskIfWrapperEqualLut *task_wr = new ExynosVpuTaskIfWrapperEqualLut(this, 1);
    status = setTaskIfWrapper(1, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_1);
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
ExynosVpuKernelEqualizeHist::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0, *task_if_1;

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
ExynosVpuKernelEqualizeHist::initTask0FromApi(void)
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
    ExynosVpuProcess *hist_process = new ExynosVpuProcess(task);
    ExynosVpuProcess *lut_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, hist_process);
    ExynosVpuVertex::connect(hist_process, lut_process);
    ExynosVpuVertex::connect(lut_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(hist_process);

    ExynosVpuPu *io_dma_in1;
    ExynosVpuPu *io_dma_in2;
    ExynosVpuPu *io_dma_out;

    {
        /* define subchain */
        ExynosVpuSubchainHw *hist_subchain = new ExynosVpuSubchainHw(hist_process);

        /* define pu */
        ExynosVpuPu *dma_in = pu_factory.createPu(hist_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);
        ExynosVpuPu *hist = pu_factory.createPu(hist_subchain, VPU_PU_HISTOGRAM);
        hist->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, hist, 0);

        struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
        struct vpul_pu_histogram *hist_param = (struct vpul_pu_histogram*)hist->getParameter();

        hist_param->offset = 0;
        hist_param->inverse_binsize = 1;
        hist_param->signed_in0 = 0;
        hist_param->max_val = 255;

        io_dma_in1 = dma_in;
    }

    {
        /* define subchain */
        ExynosVpuSubchainHw *lut_subchain = new ExynosVpuSubchainHw(hist_process);

        /* define pu */
        ExynosVpuPu *dma_in = pu_factory.createPu(lut_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);
        ExynosVpuPu *lut = pu_factory.createPu(lut_subchain, VPU_PU_LUT);
        lut->setSize(iosize, iosize);
        ExynosVpuPu *dma_out = pu_factory.createPu(lut_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, lut, 0);
        ExynosVpuPu::connect(lut, 0, dma_out, 0);

        struct vpul_pu_lut *lut_param = (struct vpul_pu_lut*)lut->getParameter();

        lut_param->interpolation_mode = 1;
        lut_param->inverse_binsize = 1;
        lut_param->binsize = 1;

        io_dma_in2 = dma_in;
        io_dma_out = dma_out;
    }

    /* JUN_TBD, cpu vertex construction */

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(hist_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(hist_process, fixed_roi);
    io_dma_in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(hist_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(hist_process, fixed_roi);
    io_dma_in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(hist_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(hist_process, fixed_roi);
    io_dma_out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_dma_in1);
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 1, io_dma_in2);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, io_dma_out);
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
ExynosVpuKernelEqualizeHist::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelEqualizeHist::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];
    ExynosVpuTaskIfWrapper *task_wr_1 = m_task_wr_list[1];

    vx_param_info_t param_info;
    struct io_format_t io_format;
    struct io_memory_t io_memory;

    /* task 0 */
    /* connect vx param to io */
    memset(&param_info, 0x0, sizeof(param_info));

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* for distribution */
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = 4;
    io_format.height = 1024;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for histogram pu */
    io_buffer_info_t io_hist_buffer_info;
    memset(&io_hist_buffer_info, 0x0, sizeof(io_hist_buffer_info));
    io_hist_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_hist_buffer_info.mapped = true;
    status = allocateBuffer(&io_hist_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_hist_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    m_histogram_ptr = io_hist_buffer_info.addr;

    /* task 1 */
    /* connect vx param to io */
    memset(&param_info, 0x0, sizeof(param_info));

    param_info.image.plane = 0;
    status = task_wr_1->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* table width is 16-bit */
    io_format.format = VS4L_DF_IMAGE_U16;
    io_format.plane = 0;
    io_format.width = 256;
    io_format.height = 1;
    io_format.pixel_byte = 2;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for lut pu */
    io_buffer_info_t io_lut_buffer_info;
    memset(&io_lut_buffer_info, 0x0, sizeof(io_lut_buffer_info));
    io_lut_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_lut_buffer_info.mapped = true;
    status = allocateBuffer(&io_lut_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_1->setIoCustomParam(VS4L_DIRECTION_IN, 1, &io_format, &io_memory, &io_lut_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    param_info.image.plane = 0;
    status = task_wr_1->setIoVxParam(VS4L_DIRECTION_OT, 0, 1, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

void*
ExynosVpuKernelEqualizeHist::getHistogramPtr(void)
{
    return m_histogram_ptr;
}

vx_uint32
ExynosVpuKernelEqualizeHist::getTotalPixelNum(void)
{
    return m_total_pixel_num;
}

#if (HOST_HISTOGRAM_PROCESS==1)
static vx_status processHistogram(const vx_reference parameters[], vx_uint32 *hist_ptr)
{
    vx_status status = VX_SUCCESS;

    vx_image image   = (vx_image) parameters[0];

    vx_int32 offset = 0;
    vx_uint32 range = 256;
    vx_uint32 window_size = 1;
    vx_rectangle_t rect;
    vx_imagepatch_addressing_t addr;
    void *image_ptr = NULL;
    vx_uint32 x = 0;
    vx_uint32 y = 0;

    status = vxGetValidRegionImage(image, &rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        goto EXIT;
    }

    status = vxAccessImagePatch(image, &rect, 0, &addr, &image_ptr, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image fails, err:%d", status);
        goto EXIT;
    }

    if (status == VX_SUCCESS) {
        for (y = 0; y < addr.dim_y; y++) {
            for (x = 0; x < addr.dim_x; x++) {
                vx_uint8 *src_ptr = (vx_uint8*)vxFormatImagePatchAddress2d(image_ptr, x, y, &addr);
                vx_uint8 pixel = *src_ptr;
                if ((offset <= (vx_size)pixel) && ((vx_size)pixel < (offset+range))) {
                    vx_size index = (pixel - (vx_uint16)offset) / window_size;
                    hist_ptr[index]++;
                }
            }
        }
    }

    status = vxCommitImagePatch(image, NULL, 0, &addr, image_ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image fails, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperEqualHist::ExynosVpuTaskIfWrapperEqualHist(ExynosVpuKernel *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperEqualHist::processTask(const vx_reference parameters[], vx_uint32 frame_number)
{
    vx_status status = VX_SUCCESS;

    status = preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("pre-processing fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    vx_uint32 *vpu_hist_array;
    vpu_hist_array = (vx_uint32*)io_buffer_info->addr;

    status = processHistogram(parameters, vpu_hist_array);
    if (status != VX_SUCCESS) {
        VXLOGE("processing histogram fails");
        goto EXIT;
    }

    status = postProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("post-processing fails");
    }

EXIT:
    return status;
}
#endif

#define NUM_BINS    256
static status_t equalize_histogram(const vx_uint32 *histogram_in, vx_uint16 *histogram_out, vx_uint32 total_pixel_num)
{
    vx_uint32 sum, div;
    sum = 0;
    vx_uint8 minv;
    minv = 0xFF;
    vx_uint32 cdf[NUM_BINS];
    for (vx_uint32 i=0; i<NUM_BINS; i++) {
        if (minv > histogram_in[i])
            minv = histogram_in[i];
        sum += histogram_in[i];
        cdf[i] = sum;
    }
    div = total_pixel_num - cdf[minv];
    if( div > 0 ) {
        /* recompute the histogram to be a LUT for replacing pixel values */
        for (vx_uint32 i = 0; i < NUM_BINS; i++) {
            uint32_t cdfx = cdf[i] - cdf[minv];
            vx_float32 p = (vx_float32)cdfx/(vx_float32)div;
            histogram_out[i] = (uint8_t)(p * 255.0f + 0.5f);
        }
    } else {
        for (vx_uint32 i = 0; i < NUM_BINS; i++)
            histogram_out[i] = i;
    }

    return NO_ERROR;
}

ExynosVpuTaskIfWrapperEqualLut::ExynosVpuTaskIfWrapperEqualLut(ExynosVpuKernelEqualizeHist *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperEqualLut::preProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    vx_uint32 *histogram_array;
    histogram_array = (vx_uint32*)((ExynosVpuKernelEqualizeHist*)getKernel())->getHistogramPtr();

    /* convert&copy array from vpu format to vx format */
    const io_buffer_info_t *io_buffer_info;
    vx_uint16 *vpu_lut_array;
    io_buffer_info = &m_in_io_param_info[1].custom_param.io_buffer_info[0];
    vpu_lut_array = (vx_uint16*)io_buffer_info->addr;

    vx_uint32 total_pixel_num;
    total_pixel_num = ((ExynosVpuKernelEqualizeHist*)getKernel())->getTotalPixelNum();

    equalize_histogram(histogram_array, vpu_lut_array, total_pixel_num);

EXIT:
    return status;
}

}; /* namespace android */

