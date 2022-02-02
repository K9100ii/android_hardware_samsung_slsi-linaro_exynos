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

#define LOG_TAG "ExynosVpuKernelHistogram"
#include <cutils/log.h>

#include "ExynosVpuKernelHistogram.h"

#include "vpu_kernel_util.h"

#include "td-binary-histogram.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_histogram_from_VDE_DS;

vx_status
ExynosVpuKernelHistogram::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8 || format == VX_DF_IMAGE_U16 ) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelHistogram::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_image src = 0;
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        vx_parameter dst_param = vxGetParameterByIndex(node, 1);
        vx_distribution dist;

        vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
        vxQueryParameter(dst_param, VX_PARAMETER_ATTRIBUTE_REF, &dist, sizeof(dist));
        if ((src) && (dist)) {
            vx_uint32 width = 0, height = 0;
            vx_df_image format;
            vx_size numBins = 0;
            vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_BINS, &numBins, sizeof(numBins));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

            /* fill in the meta data with the attributes so that the checker will pass */

            status = VX_SUCCESS;
            vxReleaseDistribution(&dist);
            vxReleaseImage(&src);
        }
        vxReleaseParameter(&dst_param);
        vxReleaseParameter(&src_param);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("output vliadator fails, %p", meta);
    }

    return status;
}

ExynosVpuKernelHistogram::ExynosVpuKernelHistogram(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "histogram");
}

ExynosVpuKernelHistogram::~ExynosVpuKernelHistogram(void)
{

}

vx_status
ExynosVpuKernelHistogram::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelHistogram::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelHistogram::initTaskFromBinary(void)
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
ExynosVpuKernelHistogram::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    /* JUN_TBD, VDE doesn't support preload_pu memmap */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary)->memmap_desc[0]);
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
    ((struct vpul_task*)td_binary)->n_external_mem_addresses = 2;
#endif

    ExynosVpuTaskIfWrapperHistogram *task_wr = new ExynosVpuTaskIfWrapperHistogram(this, 0);
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
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)3);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelHistogram::initTaskFromApi(void)
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
ExynosVpuKernelHistogram::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperHistogram *task_wr = new ExynosVpuTaskIfWrapperHistogram(this, 0);
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
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, hist_process);
    ExynosVpuVertex::connect(hist_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(hist_process);

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

    hist_param->signed_in0 = 0;
    hist_param->max_val = 255;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(hist_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(hist_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(hist_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(hist_process, fixed_roi);
    hist->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, hist);
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
ExynosVpuKernelHistogram::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_distribution distribution = (vx_distribution)parameters[1];

    vx_int32 offset;
    status = vxQueryDistribution(distribution, VX_DISTRIBUTION_ATTRIBUTE_OFFSET, &offset, sizeof(offset));
    if (status != VX_SUCCESS) {
        VXLOGE("querying distribution, err:%d", status);
        goto EXIT;
    }

    vx_uint32 window_size;
    status = vxQueryDistribution(distribution, VX_DISTRIBUTION_ATTRIBUTE_WINDOW, &window_size, sizeof(window_size));
    if (status != VX_SUCCESS) {
        VXLOGE("querying distribution, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *hist;
    hist = getTask(0)->getPu(VPU_PU_HISTOGRAM, 1, 0);
    if (hist == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_histogram *hist_param;
    hist_param = (struct vpul_pu_histogram*)hist->getParameter();
    hist_param->offset = offset;
    hist_param->inverse_binsize = (1<<16) / window_size;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelHistogram::initVxIo(const vx_reference *parameters)
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

    /* for distribution */
    struct io_format_t io_format;
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = 4;
    io_format.height = 1024;
    io_format.pixel_byte = 1;

    struct io_memory_t io_memory;
    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for histogram pu */
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

#if (HOST_HISTOGRAM_PROCESS==1)
static vx_status processHistogram(const vx_reference parameters[], vx_uint32 *hist_ptr)
{
    vx_status status = VX_SUCCESS;

    vx_image image   = (vx_image) parameters[0];
    vx_distribution dist = (vx_distribution)parameters[1];

    vx_int32 offset = 0;
    vx_uint32 range = 0;
    vx_uint32 window_size = 0;
    vx_rectangle_t rect;
    vx_imagepatch_addressing_t addr;
    void *image_ptr = NULL;
    vx_uint32 x = 0;
    vx_uint32 y = 0;


    status |= vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_RANGE, &range, sizeof(range));
    status |= vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_OFFSET, &offset, sizeof(offset));
    status |= vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_WINDOW, &window_size, sizeof(window_size));
    if (status != VX_SUCCESS) {
        VXLOGE("querying distribution fails");
        goto EXIT;
    }

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

vx_status
ExynosVpuTaskIfWrapperHistogram::processTask(const vx_reference parameters[], vx_uint32 frame_number)
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

vx_status
ExynosVpuTaskIfWrapperHistogram::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_distribution vx_dist_object;
    vx_dist_object = (vx_distribution)parameters[1];

    vx_size bins_num;
    status = vxQueryDistribution(vx_dist_object, VX_DISTRIBUTION_ATTRIBUTE_BINS, &bins_num, sizeof(bins_num));
    if (status != NO_ERROR) {
        VXLOGE("querying dist fails");
        goto EXIT;
    }

    vx_uint32 *vx_bin_array;
    vx_bin_array = NULL;
    status = vxAccessDistribution(vx_dist_object, (void**)&vx_bin_array, VX_WRITE_ONLY);
    if (status != NO_ERROR) {
        VXLOGE("accessing dist fails");
        goto EXIT;
    }

    if (m_out_io_param_info[0].type != IO_PARAM_CUSTOM) {
        VXLOGE("io type is not matching");
        status = VX_FAILURE;
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    vx_uint32 *vpu_hist_array;
    vpu_hist_array = (vx_uint32*)io_buffer_info->addr;
    vx_uint32 i;
    for (i=0; i<bins_num; i++) {
        vx_bin_array[i] = vpu_hist_array[i];
    }

    status = vxCommitDistribution(vx_dist_object, vx_bin_array);
    if (status != NO_ERROR) {
        VXLOGE("commit dist fails");
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperHistogram::ExynosVpuTaskIfWrapperHistogram(ExynosVpuKernelHistogram *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

}; /* namespace android */

