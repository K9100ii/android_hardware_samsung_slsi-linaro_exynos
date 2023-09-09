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

#define LOG_TAG "ExynosVpuKernelFilter"
#include <cutils/log.h>

#include "ExynosVpuKernelFilter.h"

#include "vpu_kernel_util.h"

#include "td-binary-median.h"
#include "td-binary-box.h"
#include "td-binary-gaussian.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_median[] =
    TASK_test_binary_median_from_VDE_DS;

static vx_uint16 td_binary_box[] =
    TASK_test_binary_box_from_VDE_DS;

static vx_uint16 td_binary_gaussian[] =
    TASK_test_binary_gaussian_from_VDE_DS;

vx_status
ExynosVpuKernelFilter::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
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
    }

    return status;
}

vx_status
ExynosVpuKernelFilter::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, 0); /* we reference an input image */
        if (param) {
            vx_image input = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = VX_DF_IMAGE_U8;

                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

                vxReleaseImage(&input);

                status = VX_SUCCESS;
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

ExynosVpuKernelFilter::ExynosVpuKernelFilter(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{

}

ExynosVpuKernelFilter::~ExynosVpuKernelFilter(void)
{

}

vx_status
ExynosVpuKernelFilter::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelFilter::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_border_mode_t border;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_BORDER_MODE, &border, sizeof(border));
    if (status != VX_SUCCESS) {
        VXLOGE("querying node, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *slf;
    slf = getTask(0)->getPu(VPU_PU_SLF50, 1, 0);
    if (slf == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_slf *slf_param;
    slf_param = (struct vpul_pu_slf*)slf->getParameter();
    switch (border.mode) {
    case VX_BORDER_MODE_UNDEFINED:
    case VX_BORDER_MODE_CONSTANT:
        slf_param->border_fill = 0;
        slf_param->border_fill_constant = border.constant_value;
        break;
    case VX_BORDER_MODE_REPLICATE:
        slf_param->border_fill = 2;
        break;
    default:
        break;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelFilter::initVxIo(const vx_reference *parameters)
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

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 1, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuKernelMedian::ExynosVpuKernelMedian(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelFilter(name, param_num)
{
    strcpy(m_task_name, "median");
}

vx_status
ExynosVpuKernelMedian::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelMedian::initTaskFromBinary(void)
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
ExynosVpuKernelMedian::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_median);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)2);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelMedian::initTaskFromApi(void)
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
ExynosVpuKernelMedian::initTask0FromApi(void)
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
    ExynosVpuProcess *nlf_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, nlf_process);
    ExynosVpuVertex::connect(nlf_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(nlf_process);

    /* define subchain */
    ExynosVpuSubchainHw *nlf_subchain = new ExynosVpuSubchainHw(nlf_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(nlf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *nlf = pu_factory.createPu(nlf_subchain, VPU_PU_NLF);
    nlf->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(nlf_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, nlf, 0);
    ExynosVpuPu::connect(nlf, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_nlf *nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    nlf_param->filter_mode = 2;
    nlf_param->border_mode_up = 1;
    nlf_param->border_mode_down = 1;
    nlf_param->border_mode_left = 1;
    nlf_param->border_mode_right = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(nlf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(nlf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(nlf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(nlf_process, fixed_roi);
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
ExynosVpuKernelMedian::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_border_mode_t border;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_BORDER_MODE, &border, sizeof(border));
    if (status != VX_SUCCESS) {
        VXLOGE("querying node, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *nlf;
    nlf = getTask(0)->getPu(VPU_PU_NLF, 1, 0);
    if (nlf == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_nlf *nlf_param;
    nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();
    switch (border.mode) {
    case VX_BORDER_MODE_UNDEFINED:
    case VX_BORDER_MODE_CONSTANT:
        nlf_param->border_fill = 0;
        nlf_param->border_fill_constant = border.constant_value;
        break;
    case VX_BORDER_MODE_REPLICATE:
        nlf_param->border_fill = 2;
        break;
    default:
        break;
    }

EXIT:
    return status;
}

ExynosVpuKernelBox::ExynosVpuKernelBox(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelFilter(name, param_num)
{
    strcpy(m_task_name, "boxfilter");
}

vx_status
ExynosVpuKernelBox::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelBox::initTaskFromBinary(void)
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
ExynosVpuKernelBox::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_box);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)2);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelBox::initTaskFromApi(void)
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
ExynosVpuKernelBox::initTask0FromApi(void)
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
    ExynosVpuProcess *slf_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, slf_process);
    ExynosVpuVertex::connect(slf_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(slf_process);

    /* define subchain */
    ExynosVpuSubchainHw *slf_subchain = new ExynosVpuSubchainHw(slf_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(slf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *slf = pu_factory.createPu(slf_subchain, VPU_PU_SLF50);
    slf->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(slf_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, slf, 0);
    ExynosVpuPu::connect(slf, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    slf_param->filter_size_mode = 1;
    slf_param->border_mode_up = 1;
    slf_param->border_mode_down = 1;
    slf_param->border_mode_left = 1;
    slf_param->border_mode_right = 1;
    slf_param->coefficient_fraction = 7;
    slf_param->maxp_sizes_filt_hor = 7;
    slf_param->maxp_sizes_filt_ver = 7;
    int32_t slf_coff[] = {1821, 1821, 1821, 1, 1, 1};
    ((ExynosVpuPuSlf*)slf)->setStaticCoffValue(slf_coff, 3, sizeof(slf_coff)/sizeof(slf_coff[0]));

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(slf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(slf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(slf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(slf_process, fixed_roi);
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

ExynosVpuKernelGaussian::ExynosVpuKernelGaussian(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelFilter(name, param_num)
{
    strcpy(m_task_name, "gaussianfilter");
}

vx_status
ExynosVpuKernelGaussian::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelGaussian::initTaskFromBinary(void)
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
ExynosVpuKernelGaussian::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_gaussian);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)2);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelGaussian::initTaskFromApi(void)
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
ExynosVpuKernelGaussian::initTask0FromApi(void)
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
    ExynosVpuProcess *slf_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, slf_process);
    ExynosVpuVertex::connect(slf_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(slf_process);

    /* define subchain */
    ExynosVpuSubchainHw *slf_subchain = new ExynosVpuSubchainHw(slf_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(slf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *slf = pu_factory.createPu(slf_subchain, VPU_PU_SLF50);
    slf->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(slf_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, slf, 0);
    ExynosVpuPu::connect(slf, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    slf_param->filter_size_mode = 1;
    slf_param->border_mode_up = 1;
    slf_param->border_mode_down = 1;
    slf_param->border_mode_left = 1;
    slf_param->border_mode_right = 1;
    slf_param->coefficient_fraction = 2;
    slf_param->maxp_sizes_filt_hor = 7;
    slf_param->maxp_sizes_filt_ver = 7;
    int32_t slf_coff[] = {1, 2, 1, 1, 2, 1};
    ((ExynosVpuPuSlf*)slf)->setStaticCoffValue(slf_coff, 3, sizeof(slf_coff)/sizeof(slf_coff[0]));

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(slf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(slf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(slf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(slf_process, fixed_roi);
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

}; /* namespace android */

