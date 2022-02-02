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

#define LOG_TAG "ExynosVpuKernelSobel"
#include <cutils/log.h>

#include "ExynosVpuKernelSobel.h"

#include "vpu_kernel_util.h"

#include "td-binary-sobel.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_sobel_from_VDE_DS;

vx_status
ExynosVpuKernelSobel::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_uint32 width = 0, height = 0;
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (width >= 3 && height >= 3 && format == VX_DF_IMAGE_U8)
                status = VX_SUCCESS;
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosVpuKernelSobel::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1 || index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, 0); /* we reference the input image */
        if (param) {
            vx_image input = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input) {
                vx_uint32 width = 0, height = 0;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vx_df_image meta_format = VX_DF_IMAGE_S16;
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

ExynosVpuKernelSobel::ExynosVpuKernelSobel(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_gradient_x_enable = vx_false_e;
    m_gradient_y_enable = vx_false_e;

    strcpy(m_task_name, "sobel");
}

ExynosVpuKernelSobel::~ExynosVpuKernelSobel(void)
{

}

vx_status
ExynosVpuKernelSobel::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    if (parameters[1])
        m_gradient_x_enable = vx_true_e;

    if (parameters[2])
        m_gradient_y_enable = vx_true_e;

    return status;
}

vx_status
ExynosVpuKernelSobel::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelSobel::initTaskFromBinary(void)
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
ExynosVpuKernelSobel::initTask0FromBinary(void)
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
    task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)3);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelSobel::initTaskFromApi(void)
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
ExynosVpuKernelSobel::initTask0FromApi(void)
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

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_glf *glf_param = (struct vpul_pu_glf*)glf->getParameter();
    glf_param->filter_size_mode = 1;
    glf_param->two_outputs = 1;
    glf_param->bits_out = 1;
    glf_param->signed_out = 1;
    glf_param->RAM_type = 1;
    glf_param->border_mode_up = 1;
    glf_param->border_mode_down = 1;
    glf_param->border_mode_left = 1;
    glf_param->border_mode_right = 1;
    glf_param->signed_coefficients = 1;
    glf_param->out_enable_2 = 0;

    ExynosVpuPu::connect(dma_in, 0, glf, 0);

    ExynosVpuPu *dma_out1 = NULL;
    ExynosVpuPu *dma_out2 = NULL;

    if (m_gradient_x_enable && m_gradient_y_enable) {
        vx_int32 gradient_xy_filter[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1, -1, -2, -1, 0, 0, 0, 1, 2, 1};
        ((ExynosVpuPuGlf*)glf)->setStaticCoffValue(gradient_xy_filter, 3, sizeof(gradient_xy_filter)/sizeof(gradient_xy_filter[0]));

        dma_out1 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT0);
        dma_out1->setSize(iosize, iosize);
        ExynosVpuPu::connect(glf, 0, dma_out1, 0);

        dma_out2 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT_MNM0);
        dma_out2->setSize(iosize, iosize);
        ExynosVpuPu::connect(glf, 1, dma_out2, 0);

        struct vpul_pu_dma *dma_out1_param = (struct vpul_pu_dma*)dma_out1->getParameter();
        struct vpul_pu_dma *dma_out2_param = (struct vpul_pu_dma*)dma_out2->getParameter();
    } else if (m_gradient_x_enable) {
        vx_int32 gradient_x_filter[] = {-1, 0, 1, -2, 0, 2, -1, 0, 1};
        ((ExynosVpuPuGlf*)glf)->setStaticCoffValue(gradient_x_filter, 3, sizeof(gradient_x_filter)/sizeof(gradient_x_filter[0]));

        dma_out1 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT0);
        dma_out1->setSize(iosize, iosize);
        ExynosVpuPu::connect(glf, 0, dma_out1, 0);

        struct vpul_pu_dma *dma_out1_param = (struct vpul_pu_dma*)dma_out1->getParameter();
    } else if (m_gradient_y_enable) {
        vx_int32 gradient_y_filter[] = {-1, -2, -1, 0, 0, 0, 1, 2, 1};
        ((ExynosVpuPuGlf*)glf)->setStaticCoffValue(gradient_y_filter, 3, sizeof(gradient_y_filter)/sizeof(gradient_y_filter[0]));

        dma_out2 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT_MNM0);
        dma_out2->setSize(iosize, iosize);
        ExynosVpuPu::connect(glf, 1, dma_out2, 0);

        struct vpul_pu_dma *dma_out2_param = (struct vpul_pu_dma*)dma_out2->getParameter();
    }

    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    /* connect pu to io */
    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);

    if (m_gradient_x_enable) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
        dma_out1->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out1);
    }
    if (m_gradient_y_enable) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
        dma_out2->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, 1, dma_out2);
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelSobel::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_image input_image = (vx_image)parameters[0];
    vx_uint32 height;
    status = vxQueryImage(input_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    vx_border_mode_t border;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_BORDER_MODE, &border, sizeof(border));
    if (status != VX_SUCCESS) {
        VXLOGE("querying node, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *glf;
    glf = getTask(0)->getPu(VPU_PU_GLF50, 1, 0);
    if (glf == NULL) {
        VXLOGE("getting glf fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_glf *glf_param;
    glf_param = (struct vpul_pu_glf*)glf->getParameter();

    glf_param->image_height = height;

    switch (border.mode) {
    case VX_BORDER_MODE_UNDEFINED:
    case VX_BORDER_MODE_CONSTANT:
        glf_param->border_fill = 0;
        glf_param->border_fill_constant = border.constant_value;
        break;
    case VX_BORDER_MODE_REPLICATE:
        glf_param->border_fill = 2;
        break;
    default:
        break;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSobel::initVxIo(const vx_reference *parameters)
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

    if (m_gradient_x_enable) {
        param_info.image.plane = 0;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 1, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

    if (m_gradient_y_enable) {
        param_info.image.plane = 0;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 1, 2, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

EXIT:
    return status;
}

}; /* namespace android */

