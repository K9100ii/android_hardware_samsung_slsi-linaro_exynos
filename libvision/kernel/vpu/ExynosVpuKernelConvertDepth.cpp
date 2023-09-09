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

#define LOG_TAG "ExynosVpuKernelConvertDepth"
#include <cutils/log.h>

#include "ExynosVpuKernelConvertDepth.h"

#include "vpu_kernel_util.h"

#include "td-binary-convertdepth_s162u8.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_s162u8[] =
    TASK_test_binary_convertdepth_s162u8_from_VDE_DS;

vx_status
ExynosVpuKernelConvertDepth::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if ((status == VX_SUCCESS) && input) {
                vx_df_image format = 0;
                status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if ((status != VX_SUCCESS) ||
                    (format == VX_DF_IMAGE_U8)  ||
                    (format == VX_DF_IMAGE_U16) ||
                    (format == VX_DF_IMAGE_U32) ||
                    (format == VX_DF_IMAGE_S32) ||
                    (format == VX_DF_IMAGE_S16)) {
                    status = VX_SUCCESS;
                } else {
                    status = VX_ERROR_INVALID_PARAMETERS;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2) {
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
                        printf("Overflow given as %08x\n", overflow_policy);
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
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar scalar = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (status == VX_SUCCESS) {
                vx_enum type = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_INT32) {
                    vx_int32 shift = 0;
                    status = vxReadScalarValue(scalar, &shift);
                    if (status == VX_SUCCESS) {
                        /*! \internal Allowing \f$ 0 \le shift < 32 \f$ could
                         * produce weird results for smaller bit depths */
                        if (shift < 0 || shift >= 32) {
                            status = VX_ERROR_INVALID_VALUE;
                        }
                        /* status should be VX_SUCCESS from call */
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
ExynosVpuKernelConvertDepth::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 1),
        };
        if (param[0] && param[1]) {
            vx_image images[2] = {0,0};
            status  = VX_SUCCESS;
            status |= vxQueryParameter(param[0], VX_PARAMETER_ATTRIBUTE_REF, &images[0], sizeof(images[0]));
            status |= vxQueryParameter(param[1], VX_PARAMETER_ATTRIBUTE_REF, &images[1], sizeof(images[1]));
            if ((status == VX_SUCCESS) && (images[0]) && (images[1])) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format[2] = {VX_DF_IMAGE_VIRT, VX_DF_IMAGE_VIRT};
                status |= vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                status |= vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                status |= vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_FORMAT, &format[0], sizeof(format[0]));
                status |= vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_FORMAT, &format[1], sizeof(format[1]));
                if (((format[0] == VX_DF_IMAGE_U8)  && (format[1] == VX_DF_IMAGE_S16)) ||
                    ((format[0] == VX_DF_IMAGE_U8)  && (format[1] == VX_DF_IMAGE_U16)) ||
                    ((format[0] == VX_DF_IMAGE_U8)  && (format[1] == VX_DF_IMAGE_U32)) ||
                    ((format[0] == VX_DF_IMAGE_U16) && (format[1] == VX_DF_IMAGE_U8))  ||
                    ((format[0] == VX_DF_IMAGE_U16) && (format[1] == VX_DF_IMAGE_U32)) ||
                    ((format[0] == VX_DF_IMAGE_S16) && (format[1] == VX_DF_IMAGE_S32)) ||
                    ((format[0] == VX_DF_IMAGE_U32) && (format[1] == VX_DF_IMAGE_U8))  ||
                    ((format[0] == VX_DF_IMAGE_U32) && (format[1] == VX_DF_IMAGE_U16)) ||
                    ((format[0] == VX_DF_IMAGE_S32) && (format[1] == VX_DF_IMAGE_S16)) ||
                    ((format[0] == VX_DF_IMAGE_S16) && (format[1] == VX_DF_IMAGE_U8))) {
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format[1], sizeof(format[1]));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                    status = VX_SUCCESS;
                } else {
                    status = VX_ERROR_INVALID_PARAMETERS;
                }
                vxReleaseImage(&images[0]);
                vxReleaseImage(&images[1]);
            }
            vxReleaseParameter(&param[0]);
            vxReleaseParameter(&param[1]);
        }
    }

    return status;
}

ExynosVpuKernelConvertDepth::ExynosVpuKernelConvertDepth(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "convertbitdepth");
}

ExynosVpuKernelConvertDepth::~ExynosVpuKernelConvertDepth(void)
{

}

vx_status
ExynosVpuKernelConvertDepth::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_image output = (vx_image)parameters[1];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &m_src_format, sizeof(m_src_format));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &m_dst_format, sizeof(m_dst_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image format fails, err:%d", status);
        return status;
    }

    return status;
}

vx_status
ExynosVpuKernelConvertDepth::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelConvertDepth::initTaskFromBinary(void)
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
ExynosVpuKernelConvertDepth::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_s162u8);
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
ExynosVpuKernelConvertDepth::initTaskFromApi(void)
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
ExynosVpuKernelConvertDepth::initTask0FromApi(void)
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
    ExynosVpuProcess *convert_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, convert_process);
    ExynosVpuVertex::connect(convert_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(convert_process);

    /* define subchain */
    ExynosVpuSubchainHw *calb_subchain = new ExynosVpuSubchainHw(convert_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(calb_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *calb = pu_factory.createPu(calb_subchain, VPU_PU_CALB0);
    calb->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(calb_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, calb, 0);
    ExynosVpuPu::connect(calb, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_calb *calb_param = (struct vpul_pu_calb*)calb->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    calb_param->input_enable = 1;

    switch(m_src_format) {
    case VX_DF_IMAGE_U8:
        calb_param->signed_in0 = 0;
        calb_param->bits_in0 = 0;
        break;
    case VX_DF_IMAGE_U16:
        calb_param->signed_in0 = 0;
        calb_param->bits_in0 = 1;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->signed_in0 = 1;
        calb_param->bits_in0 = 1;
        break;
    case VX_DF_IMAGE_U32:
        calb_param->signed_in0 = 0;
        calb_param->bits_in0 = 2;
        break;
    case VX_DF_IMAGE_S32:
        calb_param->signed_in0 = 1;
        calb_param->bits_in0 = 2;
        break;
    default:
        VXLOGE("un-supported type, 0x%x", m_src_format);
        goto EXIT;
        break;
    }

    switch(m_dst_format) {
    case VX_DF_IMAGE_U8:
        calb_param->signed_out0 = 0;
        calb_param->bits_out0 = 0;
        break;
    case VX_DF_IMAGE_U16:
        calb_param->signed_out0 = 0;
        calb_param->bits_out0 = 1;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->signed_out0 = 1;
        calb_param->bits_out0 = 1;
        break;
    case VX_DF_IMAGE_U32:
        calb_param->signed_out0 = 0;
        calb_param->bits_out0 = 2;
        break;
    case VX_DF_IMAGE_S32:
        calb_param->signed_out0 = 1;
        calb_param->bits_out0 = 2;
        break;
    default:
        VXLOGE("un-supported type, 0x%x", m_dst_format);
        goto EXIT;
        break;
    }

    /* set scale direction */
    if (m_src_format == VX_DF_IMAGE_S16) {
        switch (m_dst_format) {
        case VX_DF_IMAGE_U8:
            /* down-shift */
            calb_param->operation_code = 15;
            break;
        case VX_DF_IMAGE_U32:
        case VX_DF_IMAGE_S32:
            /* up-shift */
            calb_param->operation_code = 14;
            break;
        default:
            /* do nothing */
            break;
        }
    } else if (m_src_format == VX_DF_IMAGE_S32) {
        switch (m_dst_format) {
        case VX_DF_IMAGE_U8:
        case VX_DF_IMAGE_U16:
        case VX_DF_IMAGE_S16:
            /* down-shift */
            calb_param->operation_code = 15;
            break;
        default:
            /* do nothing */
            break;
        }
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(convert_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(convert_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(convert_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(convert_process, fixed_roi);
    dma_out->setIoTypesDesc(iotyps);

    status_t ret;
    ret = NO_ERROR;
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
ExynosVpuKernelConvertDepth::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    vx_enum policy;
    status = vxReadScalarValue((vx_scalar)parameters[2], &policy);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar, err:%d", status);
        goto EXIT;
    }

    vx_int32 shift;
    status = vxReadScalarValue((vx_scalar)parameters[3], &shift);
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

    switch (policy) {
    case VX_CONVERT_POLICY_WRAP:
        calb_param->trunc_out = 1;
        break;
    case VX_CONVERT_POLICY_SATURATE:
        calb_param->trunc_out = 0;
        break;
    default:
        VXLOGE("un-defined enum: 0x%x", policy);
        break;
    }
    calb_param->shift_bits = shift;

    switch(m_src_format) {
    case VX_DF_IMAGE_U8:
        calb_param->signed_in0 = 0;
        calb_param->bits_in0 = 0;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->signed_in0 = 1;
        calb_param->bits_in0 = 1;
        break;
    default:
        VXLOGE("un-supported type, 0x%x", m_src_format);
        goto EXIT;
        break;
    }

    switch(m_dst_format) {
    case VX_DF_IMAGE_U8:
        calb_param->signed_out0 = 0;
        calb_param->bits_out0 = 0;
        break;
    case VX_DF_IMAGE_S16:
        calb_param->signed_out0 = 1;
        calb_param->bits_out0 = 1;
        break;
    default:
        VXLOGE("un-supported type, 0x%x", m_dst_format);
        goto EXIT;
        break;
    }

    /* set scale direction */
    if ((m_src_format == VX_DF_IMAGE_S16) && (m_dst_format == VX_DF_IMAGE_U8)) {
        /* down-shift */
        calb_param->operation_code = 15;
    } else if ((m_src_format == VX_DF_IMAGE_U8) && (m_dst_format == VX_DF_IMAGE_S16)) {
        /* up-shift */
        calb_param->operation_code = 14;
    } else {
        VXLOGE("un-supported type, 0x%x 0x%x", m_src_format, m_dst_format);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelConvertDepth::initVxIo(const vx_reference *parameters)
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

}; /* namespace android */

