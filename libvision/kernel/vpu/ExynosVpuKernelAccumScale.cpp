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

#define LOG_TAG "ExynosVpuKernelAccumScale"
#include <cutils/log.h>

#include "ExynosVpuKernelAccumScale.h"

#include "vpu_kernel_util.h"

#include "td-binary-accumweighted.h"
#include "td-binary-accumsquare.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_accumweighted[] =
    TASK_test_binary_accweighted_from_VDE_DS;

static vx_uint16 td_binary_accumsquare[] =
    TASK_test_binary_accsquare_from_VDE_DS;

vx_status
ExynosVpuKernelAccumScale::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    VXLOGE("node:%p, index_%d is output of bound, ptr:%p", node, index, meta);

    return status;
}

ExynosVpuKernelAccumScale::ExynosVpuKernelAccumScale(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{

}

ExynosVpuKernelAccumScale::~ExynosVpuKernelAccumScale(void)
{

}

vx_status
ExynosVpuKernelAccumScale::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        return status;
    }

    return status;
}

vx_status
ExynosVpuKernelAccumScale::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 1, 2, param_info);
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

vx_status
ExynosVpuKernelAccumWeighted::inputValidator(vx_node node, vx_uint32 index)
{
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
        vx_scalar scalar = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param)
        {
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum type = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 alpha = 0.0f;
                    if ((vxReadScalarValue(scalar, &alpha) == VX_SUCCESS) &&
                        (0.0f <= alpha) && (alpha <= 1.0f)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&scalar);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2) {
        vx_image images[2];
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 2),
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
               format[1] == VX_DF_IMAGE_U8) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&images[0]);
            vxReleaseImage(&images[1]);
        }
        vxReleaseParameter(&param[0]);
        vxReleaseParameter(&param[1]);
    }

    return status;
}

ExynosVpuKernelAccumWeighted::ExynosVpuKernelAccumWeighted(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelAccumScale(name, param_num)
{
    strcpy(m_task_name, "accweighted");
}

vx_status
ExynosVpuKernelAccumWeighted::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelAccumWeighted::initTaskFromBinary(void)
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
ExynosVpuKernelAccumWeighted::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_accumweighted);
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
ExynosVpuKernelAccumWeighted::initTaskFromApi(void)
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
ExynosVpuKernelAccumWeighted::initTask0FromApi(void)
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
    ExynosVpuPu *calb = pu_factory.createPu(accum_subchain, VPU_PU_CALB0);
    calb->setSize(iosize, iosize);
    ExynosVpuPu *in2 = pu_factory.createPu(accum_subchain, VPU_PU_DMAIN_WIDE0);
    in2->setSize(iosize, iosize);
    ExynosVpuPu *out = pu_factory.createPu(accum_subchain, VPU_PU_DMAOT0);
    out->setSize(iosize, iosize);

    ExynosVpuPu::connect(in1, 0, calb, 0);
    ExynosVpuPu::connect(in2, 0, calb, 1);
    ExynosVpuPu::connect(calb, 0, out, 0);

    struct vpul_pu_dma *in1_param = (struct vpul_pu_dma*)in1->getParameter();
    struct vpul_pu_calb *calb_param = (struct vpul_pu_calb*)calb->getParameter();
    struct vpul_pu_dma *in2_param = (struct vpul_pu_dma*)in2->getParameter();
    struct vpul_pu_dma *out_param = (struct vpul_pu_dma*)out->getParameter();

    calb_param->input_enable = 3;
    calb_param->operation_code = 21;
    calb_param->shift_bits = 1;
    calb_param->mult_round = 3;
    calb_param->thresh_lo = 1;
    calb_param->thresh_hi = 1;

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
ExynosVpuKernelAccumWeighted::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_float32 alpha;
    status = vxReadScalarValue((vx_scalar)parameters[1], &alpha);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *calb;
    calb = getTask(0)->getPu(VPU_PU_CALB0, 1, 0);
    if (calb == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_calb *calb_param;
    calb_param = (struct vpul_pu_calb*)calb->getParameter();
    vx_size shift_lo, shift_hi, thresh_lo, thresh_hi;
    shift_lo = 0;
    shift_hi = 0;
    thresh_lo = 0;
    thresh_hi = 0;
    convertFloatToMultiShift(alpha, &shift_lo, &thresh_lo);
    convertFloatToMultiShift(1-alpha, &shift_hi, &thresh_hi);

    if (shift_lo < shift_hi) {
        shift_hi = shift_lo;
        convertFloatToMultiShift(1-alpha, &shift_hi, &thresh_hi);
    } else if (shift_lo > shift_hi) {
        shift_lo = shift_hi;
        convertFloatToMultiShift(alpha, &shift_lo, &thresh_lo);
    }

    calb_param->shift_bits = shift_lo;
    calb_param->thresh_lo = thresh_lo;
    calb_param->thresh_hi = thresh_hi;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelAccumSquare::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0 ) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input)
        {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8)
                status = VX_SUCCESS;
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_scalar scalar = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum type = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_UINT32) {
                    vx_uint32 shift = 0u;
                    if ((vxReadScalarValue(scalar, &shift) == VX_SUCCESS) && (shift <= 15)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&scalar);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2) {
        vx_image images[2];
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 2),
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

    return status;
}

ExynosVpuKernelAccumSquare::ExynosVpuKernelAccumSquare(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelAccumScale(name, param_num)
{
    strcpy(m_task_name, "accsquared");
}

vx_status
ExynosVpuKernelAccumSquare::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelAccumSquare::initTaskFromBinary(void)
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
ExynosVpuKernelAccumSquare::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_accumsquare);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)4);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelAccumSquare::initTaskFromApi(void)
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
ExynosVpuKernelAccumSquare::initTask0FromApi(void)
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
    ExynosVpuPu *calb = pu_factory.createPu(accum_subchain, VPU_PU_CALB0);
    calb->setSize(iosize, iosize);
    ExynosVpuPu *in2 = pu_factory.createPu(accum_subchain, VPU_PU_DMAIN_WIDE0);
    in2->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(accum_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *out = pu_factory.createPu(accum_subchain, VPU_PU_DMAOT0);
    out->setSize(iosize, iosize);

    ExynosVpuPu::connect(in1, 0, calb, 0);
    ExynosVpuPu::connect(calb, 0, salb, 0);
    ExynosVpuPu::connect(in2, 0, salb, 1);
    ExynosVpuPu::connect(salb, 0, out, 0);

    struct vpul_pu_dma *in1_param = (struct vpul_pu_dma*)in1->getParameter();
    struct vpul_pu_calb *calb_param = (struct vpul_pu_calb*)calb->getParameter();
    struct vpul_pu_dma *in2_param = (struct vpul_pu_dma*)in2->getParameter();
    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
    struct vpul_pu_dma *out_param = (struct vpul_pu_dma*)out->getParameter();

    calb_param->bits_in0 = 0;   /* u8 */
    calb_param->signed_in0 = 0;
    calb_param->bits_out0 = 1; /* u16 */
    calb_param->signed_out0 = 0;
    calb_param->input_enable = 1;
    calb_param->operation_code = 23;
    calb_param->shift_bits = 4;
    calb_param->mult_round = 3;
    calb_param->const_in1 = 1;
    calb_param->thresh_lo = 1;

    salb_param->signed_in0 = 0; /* u16 */
    salb_param->bits_in0 = 1;
    salb_param->signed_in1 = 1; /* s16 */
    salb_param->bits_in1 = 1;
    salb_param->signed_out0 = 1; /* s16 */
    salb_param->bits_out0 = 1;
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
ExynosVpuKernelAccumSquare::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_uint32 shift;
    status = vxReadScalarValue((vx_scalar)parameters[1], &shift);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *calb;
    calb = getTask(0)->getPu(VPU_PU_CALB0, 1, 0);
    if (calb == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_calb *calb_param;
    calb_param = (struct vpul_pu_calb*)calb->getParameter();
    calb_param->shift_bits = shift;

EXIT:
    return status;
}

}; /* namespace android */

