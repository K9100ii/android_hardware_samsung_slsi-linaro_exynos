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

#define LOG_TAG "ExynosVpuKernelMultiply"
#include <cutils/log.h>

#include "ExynosVpuKernelMultiply.h"

#include "vpu_kernel_util.h"

#include "td-binary-multiply.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_multiply_from_VDE_DS;

vx_status
ExynosVpuKernelMultiply::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8 || format == VX_DF_IMAGE_S16)
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
            vx_df_image format1;

            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width[0], sizeof(width[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_WIDTH, &width[1], sizeof(width[1]));
            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[0], sizeof(height[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[1], sizeof(height[1]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_FORMAT, &format1, sizeof(format1));
            if (width[0] == width[1] && height[0] == height[1] &&
                (format1 == VX_DF_IMAGE_U8 || format1 == VX_DF_IMAGE_S16))
                status = VX_SUCCESS;
            vxReleaseImage(&images[0]);
            vxReleaseImage(&images[1]);
        }
        vxReleaseParameter(&param[0]);
        vxReleaseParameter(&param[1]);
    } else if (index == 2) {
        vx_scalar scalar = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum type = -1;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 scale = 0.0f;
                    if ((vxReadScalarValue(scalar, &scale) == VX_SUCCESS) &&
                        (scale >= 0)) {
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
    } else if (index == 3) {
        /* overflow_policy: truncate or saturate. */
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar scalar = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum stype = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_ENUM) {
                    vx_enum overflow_policy = 0;
                    vxReadScalarValue(scalar, &overflow_policy);
                    if ((overflow_policy == VX_CONVERT_POLICY_WRAP) ||
                        (overflow_policy == VX_CONVERT_POLICY_SATURATE)) {
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
    } else if (index == 4) {
        /* rounding_policy: truncate or saturate. */
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar scalar = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum stype = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_ENUM) {
                    vx_enum rouding_policy = 0;
                    vxReadScalarValue(scalar, &rouding_policy);
                    if ((rouding_policy == VX_ROUND_POLICY_TO_ZERO) ||
                        (rouding_policy == VX_ROUND_POLICY_TO_NEAREST_EVEN)) {
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
    }

    return status;
}

vx_status
ExynosVpuKernelMultiply::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 5) {
        /*
         * We need to look at both input images, but only for the format:
         * if either is S16 or the output type is not U8, then it's S16.
         * The geometry of the output image is copied from the first parameter:
         * the input images are known to match from input parameters validation.
         */
        vx_parameter param[] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 1),
            vxGetParameterByIndex(node, index),
        };
        if (param[0] && param[1] && param[2]) {
            vx_image images[3];
            vxQueryParameter(param[0], VX_PARAMETER_ATTRIBUTE_REF, &images[0], sizeof(images[0]));
            vxQueryParameter(param[1], VX_PARAMETER_ATTRIBUTE_REF, &images[1], sizeof(images[1]));
            vxQueryParameter(param[2], VX_PARAMETER_ATTRIBUTE_REF, &images[2], sizeof(images[2]));
            if (images[0] && images[1] && images[2]) {
                vx_uint32 width = 0, height = 0;
                vx_df_image informat[2] = {VX_DF_IMAGE_VIRT, VX_DF_IMAGE_VIRT};
                vx_df_image outformat = VX_DF_IMAGE_VIRT;

                /*
                 * When passing on the geometry to the output image, we only look at
                 * image 0, as both input images are verified to match, at input
                 * validation.
                 */
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_FORMAT, &informat[0], sizeof(informat[0]));
                vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_FORMAT, &informat[1], sizeof(informat[1]));
                vxQueryImage(images[2], VX_IMAGE_ATTRIBUTE_FORMAT, &outformat, sizeof(outformat));

                if (informat[0] == VX_DF_IMAGE_U8 && informat[1] == VX_DF_IMAGE_U8 && outformat == VX_DF_IMAGE_U8) {
                    status = VX_SUCCESS;
                } else {
                    status = VX_SUCCESS;
                    outformat = VX_DF_IMAGE_S16;
                }

                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &outformat, sizeof(outformat));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vxReleaseImage(&images[0]);
                vxReleaseImage(&images[1]);
                vxReleaseImage(&images[2]);
            }
            vxReleaseParameter(&param[0]);
            vxReleaseParameter(&param[1]);
            vxReleaseParameter(&param[2]);
        }
    }

    return status;
}

ExynosVpuKernelMultiply::ExynosVpuKernelMultiply(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "multiply");
}

ExynosVpuKernelMultiply::~ExynosVpuKernelMultiply(void)
{

}

vx_status
ExynosVpuKernelMultiply::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelMultiply::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelMultiply::initTaskFromBinary(void)
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
ExynosVpuKernelMultiply::initTask0FromBinary(void)
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
ExynosVpuKernelMultiply::initTaskFromApi(void)
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
ExynosVpuKernelMultiply::initTask0FromApi(void)
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
    ExynosVpuProcess *calb_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, calb_process);
    ExynosVpuVertex::connect(calb_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(calb_process);

    /* define subchain */
    ExynosVpuSubchainHw *calb_subchain = new ExynosVpuSubchainHw(calb_process);

    /* define pu */
    ExynosVpuPu *in1 = pu_factory.createPu(calb_subchain, VPU_PU_DMAIN0);
    in1->setSize(iosize, iosize);
    ExynosVpuPu *calb = pu_factory.createPu(calb_subchain, VPU_PU_CALB0);
    calb->setSize(iosize, iosize);
    ExynosVpuPu *in2 = pu_factory.createPu(calb_subchain, VPU_PU_DMAIN_WIDE0);
    in2->setSize(iosize, iosize);
    ExynosVpuPu *out = pu_factory.createPu(calb_subchain, VPU_PU_DMAOT0);
    out->setSize(iosize, iosize);

    ExynosVpuPu::connect(in1, 0, calb, 0);
    ExynosVpuPu::connect(in2, 0, calb, 1);
    ExynosVpuPu::connect(calb, 0, out, 0);

    struct vpul_pu_dma *in1_param = (struct vpul_pu_dma*)in1->getParameter();
    struct vpul_pu_calb *calb_param = (struct vpul_pu_calb*)calb->getParameter();
    struct vpul_pu_dma *in2_param = (struct vpul_pu_dma*)in2->getParameter();
    struct vpul_pu_dma *out_param = (struct vpul_pu_dma*)out->getParameter();

    calb_param->input_enable = 7;
    calb_param->operation_code = 22;
    calb_param->thresh_lo = 1;

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(calb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(calb_process, fixed_roi);
    in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(calb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(calb_process, fixed_roi);
    in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(calb_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(calb_process, fixed_roi);
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
ExynosVpuKernelMultiply::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_image input0 = (vx_image)parameters[0];
    vx_image input1 = (vx_image)parameters[1];
    vx_image output = (vx_image)parameters[5];

    vx_float32 scale;
    vx_enum overflow_policy;
    vx_enum rounding_policy;
    status |= vxReadScalarValue((vx_scalar)parameters[2], &scale);
    status |= vxReadScalarValue((vx_scalar)parameters[3], &overflow_policy);
    status |= vxReadScalarValue((vx_scalar)parameters[4], &rounding_policy);
    if (status != VX_SUCCESS) {
        VXLOGE("querying node, err:%d", status);
        goto EXIT;
    }

    vx_df_image input0_format;
    vx_df_image input1_format;
    vx_df_image output_format;
    status |= vxQueryImage(input0, VX_IMAGE_ATTRIBUTE_FORMAT, &input0_format, sizeof(input0_format));
    status |= vxQueryImage(input1, VX_IMAGE_ATTRIBUTE_FORMAT, &input1_format, sizeof(input1_format));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
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
    vx_size shift, thresh_lo;
    shift = 0;
    thresh_lo = 0;
    status = convertFloatToMultiShift(scale, &shift, &thresh_lo);
    if (status != VX_SUCCESS) {
        VXLOGE("calculating float fails, err:%d", status);
        goto EXIT;
    }

    calb_param->shift_bits = shift;
    calb_param->thresh_lo = thresh_lo;

    switch (overflow_policy) {
    case VX_CONVERT_POLICY_WRAP:
        calb_param->trunc_out = 1;
        break;
    case VX_CONVERT_POLICY_SATURATE:
        calb_param->trunc_out = 0;
        break;
    default:
        VXLOGE("un-defined enum: 0x%x", overflow_policy);
        break;
    }

    switch (rounding_policy) {
    case VX_ROUND_POLICY_TO_ZERO:
        calb_param->mult_round = 4;
        break;
    case VX_ROUND_POLICY_TO_NEAREST_EVEN:
        calb_param->mult_round = 2;
        break;
    default:
        VXLOGE("un-defined enum: 0x%x", rounding_policy);
        break;
    }

    switch (input0_format) {
    case VX_DF_IMAGE_U8:
        calb_param->bits_in0 = 0;
        calb_param->signed_in0 = 0;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->bits_in0 = 1;
        calb_param->signed_in0 = 1;
        break;
    default:
        VXLOGE("un-defined type: 0x%x", input0_format);
        status = VX_FAILURE;
        goto EXIT;
        break;
    }

    switch (input1_format) {
    case VX_DF_IMAGE_U8:
        calb_param->bits_in1 = 0;
        calb_param->signed_in1 = 0;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->bits_in1 = 1;
        calb_param->signed_in1 = 1;
        break;
    default:
        VXLOGE("un-defined type: 0x%x", input1_format);
        status = VX_FAILURE;
        goto EXIT;
        break;
    }

    switch (output_format) {
    case VX_DF_IMAGE_U8:
        calb_param->bits_out0 = 0;
        calb_param->signed_out0 = 0;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->bits_out0= 1;
        calb_param->signed_out0 = 1;
        break;
    default:
        VXLOGE("un-defined type: 0x%x", output_format);
        status = VX_FAILURE;
        goto EXIT;
        break;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelMultiply::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 5, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

}; /* namespace android */

