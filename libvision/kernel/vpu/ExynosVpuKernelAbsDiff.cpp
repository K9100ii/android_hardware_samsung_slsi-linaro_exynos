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

#define LOG_TAG "ExynosVpuKernelAbsDiff"
#include <cutils/log.h>

#include "ExynosVpuKernelAbsDiff.h"

#include "vpu_kernel_util.h"

#include "td-binary-absdiff.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_absdiff_from_VDE_DS;

vx_status
ExynosVpuKernelAbsDiff::inputValidator(vx_node node, vx_uint32 index)
{
    EXYNOS_VPU_KERNEL_IN();

    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0 ) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8
                || format == VX_DF_IMAGE_S16)
                status = VX_SUCCESS;
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_image images[2];
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 1),
        };
        vxQueryParameter(param[0], VX_PARAMETER_ATTRIBUTE_REF, &images[0], sizeof(images[0]));
        vxQueryParameter(param[1], VX_PARAMETER_ATTRIBUTE_REF, &images[1], sizeof(images[1]));
        if (images[0] && images[1]) {
            vx_uint32 width[2], height[2];
            vx_df_image format[2];

            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width[0], sizeof(width[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_WIDTH, &width[1], sizeof(width[1]));
            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[0], sizeof(height[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[1], sizeof(height[1]));
            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_FORMAT, &format[0], sizeof(format[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_FORMAT, &format[1], sizeof(format[1]));
            if (width[0] == width[1] && height[0] == height[1] && format[0] == format[1]) {
                status = VX_SUCCESS;
            }

            vxReleaseImage(&images[0]);
            vxReleaseImage(&images[1]);
        }

        vxReleaseParameter(&param[0]);
        vxReleaseParameter(&param[1]);
    }

    EXYNOS_VPU_KERNEL_OUT();

    return status;
}

vx_status
ExynosVpuKernelAbsDiff::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    EXYNOS_VPU_KERNEL_IN();

    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 2) {
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 1),
        };

        if (param[0] && param[1]) {
            vx_image images[2];
            vxQueryParameter(param[0], VX_PARAMETER_ATTRIBUTE_REF, &images[0], sizeof(images[0]));
            vxQueryParameter(param[1], VX_PARAMETER_ATTRIBUTE_REF, &images[1], sizeof(images[1]));
            if (images[0] && images[1]) {
                vx_uint32 width[2], height[2];
                vx_df_image format = 0;
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width[0], sizeof(width[0]));
                vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_WIDTH, &width[1], sizeof(width[1]));
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[0], sizeof(height[0]));
                vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[1], sizeof(height[1]));
                if (width[0] == width[1] && height[0] == height[1] &&
                    (format == VX_DF_IMAGE_U8
                     || format == VX_DF_IMAGE_S16)) {
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width[0], sizeof(width[0]));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height[1], sizeof(height[1]));
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&images[0]);
                vxReleaseImage(&images[1]);
            }
            vxReleaseParameter(&param[0]);
            vxReleaseParameter(&param[1]);
        }
    }

    EXYNOS_VPU_KERNEL_OUT();

    return status;
}

ExynosVpuKernelAbsDiff::ExynosVpuKernelAbsDiff(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_image_format = 0;
    strcpy(m_task_name, "absdiff");
}

ExynosVpuKernelAbsDiff::~ExynosVpuKernelAbsDiff(void)
{

}

vx_status
ExynosVpuKernelAbsDiff::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image in1 = (vx_image)parameters[0];
    vx_image in2 = (vx_image)parameters[1];
    vx_image output = (vx_image)parameters[2];

    vx_rectangle_t r_in1, r_in2;

    status  = vxGetValidAncestorRegionImage(in1, &r_in1);
    status |= vxGetValidAncestorRegionImage(in2, &r_in2);
    if (status != VX_SUCCESS) {
        VXLOGE("can't getting valid region");
        return status;
    }
    if (vxFindOverlapRectangle(&r_in1, &r_in2, &m_valid_rect) == vx_false_e) {
        status |= VX_FAILURE;
        return status;
    }

    status = vxQueryImage(in1, VX_IMAGE_ATTRIBUTE_FORMAT, &m_image_format, sizeof(m_image_format));
    if (status != VX_SUCCESS) {
        VXLOGE("can't querying image object");
        return status;
    }

    return status;
}

vx_status
ExynosVpuKernelAbsDiff::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelAbsDiff::initTaskFromBinary(void)
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
ExynosVpuKernelAbsDiff::initTask0FromBinary(void)
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
        goto EXIT;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)3);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelAbsDiff::initTaskFromApi(void)
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
ExynosVpuKernelAbsDiff::initTask0FromApi(void)
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
    ExynosVpuProcess *salb_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, salb_process);
    ExynosVpuVertex::connect(salb_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(salb_process);

    /* define subchain */
    ExynosVpuSubchainHw *salb_subchain = new ExynosVpuSubchainHw(salb_process);

    /* define pu */
    ExynosVpuPu *in1 = pu_factory.createPu(salb_subchain, VPU_PU_DMAIN0);
    in1->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(salb_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *in2 = pu_factory.createPu(salb_subchain, VPU_PU_DMAIN_WIDE0);
    in2->setSize(iosize, iosize);
    ExynosVpuPu *out = pu_factory.createPu(salb_subchain, VPU_PU_DMAOT0);
    out->setSize(iosize, iosize);

    ExynosVpuPu::connect(in1, 0, salb, 0);
    ExynosVpuPu::connect(in2, 0, salb, 1);
    ExynosVpuPu::connect(salb, 0, out, 0);

    struct vpul_pu_dma *in1_param = (struct vpul_pu_dma*)in1->getParameter();
    struct vpul_pu_dma *in2_param = (struct vpul_pu_dma*)in2->getParameter();
    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
    struct vpul_pu_dma *out_param = (struct vpul_pu_dma*)out->getParameter();

    salb_param->input_enable = 3;
    salb_param->abs_neg = 1;
    salb_param->salbregs_custom_trunc_bittage = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(salb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(salb_process, fixed_roi);
    in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(salb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(salb_process, fixed_roi);
    in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(salb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(salb_process, fixed_roi);
    out->setIoTypesDesc(iotyps);

    task_if->setIoPu(VS4L_DIRECTION_IN, 0, in1);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, in2);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, out);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelAbsDiff::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    vx_df_image format = 0;
    status = vxQueryImage((vx_image)parameters[0], VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *salb;
    salb = getTask(0)->getPu(VPU_PU_SALB0, 1, 0);
    if (!salb) {
        VXLOGE("getting pu object fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_salb *salb_param;
    switch (format) {
    case VX_DF_IMAGE_U8:
        salb_param = (struct vpul_pu_salb*)salb->getParameter();
        salb_param->bits_in0 = 0;
        salb_param->bits_in1 = 0;
        salb_param->bits_out0= 0;
        salb_param->signed_in0 = 0;
        salb_param->signed_in1 = 0;
        salb_param->signed_out0 = 0;
        break;
    case VX_DF_IMAGE_S16:
        salb_param = (struct vpul_pu_salb*)salb->getParameter();
        salb_param->bits_in0 = 1;
        salb_param->bits_in1 = 1;
        salb_param->bits_out0= 1;
        salb_param->signed_in0 = 1;
        salb_param->signed_in1 = 1;
        salb_param->signed_out0 = 1;
        break;
    default:
        VXLOGE("un-supported format, 0x%x", format);
        status = VX_FAILURE;
        goto EXIT;
        break;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelAbsDiff::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 1, 1, param_info);
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

