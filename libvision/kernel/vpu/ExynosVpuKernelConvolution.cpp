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

#define LOG_TAG "ExynosVpuKernelConvolution"
#include <cutils/log.h>

#include "ExynosVpuKernelConvolution.h"

#include "vpu_kernel_util.h"

#include "td-binary-convolution.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_convolution_from_VDE_DS;

vx_status
ExynosVpuKernelConvolution::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vx_uint32 width = 0, height = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if ((width > VX_INT_MAX_CONVOLUTION_DIM) &&
                (height > VX_INT_MAX_CONVOLUTION_DIM) &&
                ((format == VX_DF_IMAGE_U8) || (format == VX_DF_IMAGE_S16))) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_convolution conv = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &conv, sizeof(conv));
        if (conv) {
            vx_df_image dims[2] = {0,0};
            vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_COLUMNS, &dims[0], sizeof(dims[0]));
            vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_ROWS, &dims[1], sizeof(dims[1]));
            if ((dims[0] <= VX_INT_MAX_CONVOLUTION_DIM) &&
                (dims[1] <= VX_INT_MAX_CONVOLUTION_DIM)) {
                status = VX_SUCCESS;
            }
            vxReleaseConvolution(&conv);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelConvolution::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 2) {
        vx_parameter params[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, index),
        };

        if (params[0] && params[1]) {
            vx_image input = 0;
            vx_image output = 0;
            vxQueryParameter(params[0], VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            vxQueryParameter(params[1], VX_PARAMETER_ATTRIBUTE_REF, &output, sizeof(output));
            if (input && output) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = 0;
                vx_df_image output_format = 0;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format));

                vx_df_image meta_format = output_format == VX_DF_IMAGE_U8 ? VX_DF_IMAGE_U8 : VX_DF_IMAGE_S16;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;

                vxReleaseImage(&input);
                vxReleaseImage(&output);
            }
            vxReleaseParameter(&params[0]);
            vxReleaseParameter(&params[1]);
        }
    }

    return status;
}

ExynosVpuKernelConvolution::ExynosVpuKernelConvolution(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "convolution");
}

ExynosVpuKernelConvolution::~ExynosVpuKernelConvolution(void)
{

}

vx_status
ExynosVpuKernelConvolution::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelConvolution::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelConvolution::initTaskFromBinary(void)
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
ExynosVpuKernelConvolution::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary);
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
ExynosVpuKernelConvolution::initTaskFromApi(void)
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
ExynosVpuKernelConvolution::initTask0FromApi(void)
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
    ExynosVpuProcess *glf_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, glf_process);
    ExynosVpuVertex::connect(glf_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(glf_process);

    /* define subchain */
    ExynosVpuSubchainHw *glf_subchain = new ExynosVpuSubchainHw(glf_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(glf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *glf = pu_factory.createPu(glf_subchain, VPU_PU_GLF50);
    glf->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, glf, 0);
    ExynosVpuPu::connect(glf, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_glf *glf_param = (struct vpul_pu_glf*)glf->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    glf_param->RAM_type = 1;
    glf_param->border_mode_up = 1;
    glf_param->border_mode_down = 1;
    glf_param->border_mode_left = 1;
    glf_param->border_mode_right = 1;
    glf_param->signed_coefficients = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
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
ExynosVpuKernelConvolution::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    vx_image input_image = (vx_image)parameters[0];
    vx_convolution conv = (vx_convolution)parameters[1];
    vx_image output_image = (vx_image)parameters[2];

    vx_uint32 height;
    status = vxQueryImage(input_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    status = vxReadConvolutionCoefficients(conv, m_coff_array);
    if (status != VX_SUCCESS) {
        VXLOGE("reading convolution fails");
        goto EXIT;
    }

    vx_size rows;
    vx_size columns;
    status |= vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_ROWS, &rows, sizeof(rows));
    status |= vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_COLUMNS, &columns, sizeof(columns));
    if (status != VX_SUCCESS) {
        VXLOGE("querying convolution fails");
        goto EXIT;
    }
    if (rows != columns) {
        VXLOGE("rows and columns is not same, rows:%d, columns:%d", rows, columns);
        goto EXIT;
    }

    vx_df_image format;
    status = vxQueryImage(output_image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    ExynosVpuPu *glf;
    glf = getTask(0)->getPu(VPU_PU_GLF50, 1, 0);
    if (glf == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_glf *glf_param;
    glf_param = (struct vpul_pu_glf*)glf->getParameter();
    switch (columns) {
    case 1:
        glf_param->filter_size_mode = 0;
        break;
    case 3:
        glf_param->filter_size_mode = 1;
        break;
    case 5:
        glf_param->filter_size_mode = 2;
        break;
    case 7:
        glf_param->filter_size_mode = 3;
        break;
    case 9:
        glf_param->filter_size_mode = 4;
        break;
    case 11:
        glf_param->filter_size_mode = 5;
        break;
    default:
        VXLOGE("un-supported size, %d", columns);
        goto EXIT;
        break;
    }

    if (format == VX_DF_IMAGE_U8) {
        glf_param->bits_out = 0;
        glf_param->signed_out = 0;
    } else if (format == VX_DF_IMAGE_S16) {
        glf_param->bits_out = 1;
        glf_param->signed_out = 1;
    }else {
        VXLOGE("un-supported output format, 0x%x", format);
        goto EXIT;
    }

    glf_param->image_height = height;

    vx_int32 *coff_array32;
    coff_array32 = (vx_int32*)malloc(rows*columns*sizeof(vx_int32));
    if (coff_array32 == NULL) {
        VXLOGE("allocating array fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    for (vx_uint32 i=0; i<rows*columns; i++)
        coff_array32[i] = m_coff_array[i];

    ((ExynosVpuPuGlf*)glf)->setStaticCoffValue(coff_array32, rows, (uint32_t)(rows * columns));
    free(coff_array32);

EXIT:
    return status;
}

vx_status
ExynosVpuKernelConvolution::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 2, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

}; /* namespace android */

