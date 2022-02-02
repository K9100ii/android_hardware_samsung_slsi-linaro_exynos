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

#define LOG_TAG "ExynosVpuKernelAccum"
#include <cutils/log.h>

#include "ExynosVpuKernelAccum.h"

#include "vpu_kernel_util.h"

#include "td-binary-accum.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_acc_from_VDE_DS;

vx_status
ExynosVpuKernelAccum::inputValidator(vx_node node, vx_uint32 index)
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
            if (format == VX_DF_IMAGE_U8)
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
            if (width[0] == width[1] &&
               height[0] == height[1] &&
               format[0] == VX_DF_IMAGE_U8 &&
               format[1] == VX_DF_IMAGE_S16) {
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
ExynosVpuKernelAccum::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    VXLOGE("node:%p, index_%d is output of bound, ptr:%p", node, index, meta);

    return status;
}

ExynosVpuKernelAccum::ExynosVpuKernelAccum(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "acc");

}

ExynosVpuKernelAccum::~ExynosVpuKernelAccum(void)
{

}

vx_status
ExynosVpuKernelAccum::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelAccum::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelAccum::initTaskFromBinary(void)
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
ExynosVpuKernelAccum::initTask0FromBinary(void)
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
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)3);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelAccum::initTaskFromApi(void)
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
ExynosVpuKernelAccum::initTask0FromApi(void)
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
    ExynosVpuProcess *accum_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, accum_process);
    ExynosVpuVertex::connect(accum_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(accum_process);

    /* define subchain */
    ExynosVpuSubchainHw *accum_subchain = new ExynosVpuSubchainHw(accum_process);

    /* define pu */
    ExynosVpuPu *in1 = pu_factory.createPu(accum_subchain, VPU_PU_DMAIN0);
    in1->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(accum_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *in2 = pu_factory.createPu(accum_subchain, VPU_PU_DMAIN_WIDE0);
    in2->setSize(iosize, iosize);
    ExynosVpuPu *out = pu_factory.createPu(accum_subchain, VPU_PU_DMAOT0);
    out->setSize(iosize, iosize);

    ExynosVpuPu::connect(in1, 0, salb, 0);
    ExynosVpuPu::connect(in2, 0, salb, 1);
    ExynosVpuPu::connect(salb, 0, out, 0);

    struct vpul_pu_dma *in1_param = (struct vpul_pu_dma*)in1->getParameter();
    struct vpul_pu_dma *in2_param = (struct vpul_pu_dma*)in2->getParameter();
    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
    struct vpul_pu_dma *out_param = (struct vpul_pu_dma*)out->getParameter();

    salb_param->bits_in0 = 0;
    salb_param->bits_in1 = 1;
    salb_param->bits_out0 = 1;
    salb_param->signed_in0 = 0;
    salb_param->signed_in1 = 1;
    salb_param->signed_out0 = 1;
    salb_param->input_enable = 3;
    salb_param->operation_code = 8;
    salb_param->salbregs_custom_trunc_bittage = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(accum_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(accum_process, fixed_roi);
    in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(accum_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(accum_process, fixed_roi);
    in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(accum_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(accum_process, fixed_roi);
    out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, in1);
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 1, in2);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, out);
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
ExynosVpuKernelAccum::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelAccum::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 1, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

}; /* namespace android */

