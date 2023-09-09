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

#define LOG_TAG "ExynosVpuKernelScaleImage"
#include <cutils/log.h>

#include "ExynosVpuKernelScaleImage.h"

#include "vpu_kernel_util.h"

#include "td-binary-scaleimage_up.h"
#include "td-binary-scaleimage_down.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_up[] =
    TASK_test_binary_scaleimage_up_from_VDE_DS;

static vx_uint16 td_binary_down[] =
    TASK_test_binary_scaleimage_down_from_VDE_DS;

vx_status
ExynosVpuKernelScaleImage::inputValidator(vx_node node, vx_uint32 index)
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
    } else if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar scalar = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar) {
                vx_enum stype = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_ENUM) {
                    vx_enum interp = 0;
                    vxReadScalarValue(scalar, &interp);
                    if ((interp == VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR) ||
                        (interp == VX_INTERPOLATION_TYPE_BILINEAR) ||
                        (interp == VX_INTERPOLATION_TYPE_AREA)) {
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
ExynosVpuKernelScaleImage::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        vx_parameter dst_param = vxGetParameterByIndex(node, index);
        if (src_param && dst_param) {
            vx_image src = 0;
            vx_image dst = 0;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            vxQueryParameter(dst_param, VX_PARAMETER_ATTRIBUTE_REF, &dst, sizeof(dst));
            if ((src) && (dst)) {
                vx_uint32 w1 = 0, h1 = 0, w2 = 0, h2 = 0;
                vx_df_image f1 = VX_DF_IMAGE_VIRT, f2 = VX_DF_IMAGE_VIRT;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &w1, sizeof(w1));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &h1, sizeof(h1));
                vxQueryImage(dst, VX_IMAGE_ATTRIBUTE_WIDTH, &w2, sizeof(w2));
                vxQueryImage(dst, VX_IMAGE_ATTRIBUTE_HEIGHT, &h2, sizeof(h2));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &f1, sizeof(f1));
                vxQueryImage(dst, VX_IMAGE_ATTRIBUTE_FORMAT, &f2, sizeof(f2));
                /* output can not be virtual */
                if ((w2 != 0) && (h2 != 0) && (f2 != VX_DF_IMAGE_VIRT) && (f1 == f2)) {
                    /* fill in the meta data with the attributes so that the checker will pass */
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &f2, sizeof(f2));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &w2, sizeof(w2));
                    vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &h2, sizeof(h2));

                    status = VX_SUCCESS;
                }
                vxReleaseImage(&src);
                vxReleaseImage(&dst);
            }
            vxReleaseParameter(&src_param);
            vxReleaseParameter(&dst_param);
        }
    }

    return status;
}

ExynosVpuKernelScaleImage::ExynosVpuKernelScaleImage(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_src_width = 0;
    m_src_height = 0;

    m_dst_width = 0;
    m_dst_height = 0;

    strcpy(m_task_name, "scale");
}

ExynosVpuKernelScaleImage::~ExynosVpuKernelScaleImage(void)
{

}

vx_status
ExynosVpuKernelScaleImage::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_image output = (vx_image)parameters[1];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        goto EXIT;
    }

    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &m_src_width, sizeof(m_src_width));
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &m_src_height, sizeof(m_src_height));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_WIDTH, &m_dst_width, sizeof(m_dst_width));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_HEIGHT, &m_dst_height, sizeof(m_dst_height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails, err:%d", status);
    }

    if ((m_src_width >= m_dst_width) && (m_src_height >= m_dst_height)) {
        m_down_scaling = vx_true_e;
    } else if ((m_src_width <= m_dst_width) && (m_src_height <= m_dst_height)) {
        m_down_scaling = vx_false_e;
    } else {
        VXLOGE("the scale ratio is mixed, src:%dx%d -> dst:%dx%d", m_src_width, m_src_height, m_dst_width, m_dst_height);
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelScaleImage::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelScaleImage::initTaskFromBinary(void)
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
ExynosVpuKernelScaleImage::initTask0FromBinary(void)
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
    if (m_down_scaling)
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_down);
    else
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_up);

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
ExynosVpuKernelScaleImage::initTaskFromApi(void)
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
ExynosVpuKernelScaleImage::initTask0FromApi(void)
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
    ExynosVpuProcess *scale_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, scale_process);
    ExynosVpuVertex::connect(scale_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(scale_process);

    /* define subchain */
    ExynosVpuSubchainHw *scale_subchain = new ExynosVpuSubchainHw(scale_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(scale_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *scale = NULL;

    if (m_down_scaling) {
        ExynosVpuIoSizeOpScale *sizeop_scale = new ExynosVpuIoSizeOpScale(scale_process);
        ExynosVpuIoSizeScale *iosize_down = new ExynosVpuIoSizeScale(scale_process, iosize, sizeop_scale);

        scale = pu_factory.createPu(scale_subchain, VPU_PU_DNSCALER0);
        scale->setSize(iosize, iosize_down);
        struct vpul_pu_downscaler *downscaler_param = (struct vpul_pu_downscaler*)scale->getParameter();
        downscaler_param->do_rounding = 1;
    } else {
        ExynosVpuIoSizeOpScale *sizeop_scale = new ExynosVpuIoSizeOpScale(scale_process);
        ExynosVpuIoSizeScale *iosize_up = new ExynosVpuIoSizeScale(scale_process, iosize, sizeop_scale);

        scale = pu_factory.createPu(scale_subchain, VPU_PU_UPSCALER0);
        scale->setSize(iosize, iosize_up);
    }

    ExynosVpuPu *dma_out = pu_factory.createPu(scale_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, scale, 0);
    ExynosVpuPu::connect(scale, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    if (m_down_scaling) {
        struct vpul_pu_downscaler *scale_down_param = (struct vpul_pu_downscaler*)scale->getParameter();
    } else {
        struct vpul_pu_upscaler *scale_up_param = (struct vpul_pu_upscaler*)scale->getParameter();
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(scale_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(scale_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(scale_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(scale_process, fixed_roi);
    dma_out->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelScaleImage::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update task object from vx */
    vx_scalar intepolation = (vx_scalar)parameters[2];

    vx_enum intepolation_value;
    status = vxReadScalarValue(intepolation, &intepolation_value);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar, err:%d", status);
        goto EXIT;
    }

    ExynosVpuPu *scaler;
    if (m_down_scaling) {
        scaler = getTask(0)->getPu(VPU_PU_DNSCALER0, 1, 0);
        if (scaler == NULL) {
            VXLOGE("getting pu fails");
            status = VX_FAILURE;
            goto EXIT;
        }
        struct vpul_pu_downscaler *downscaler_param;
        downscaler_param = (struct vpul_pu_downscaler*)scaler->getParameter();
        switch (intepolation_value) {
        case VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR:
            downscaler_param->interpolation_method = 2;
            break;
        case VX_INTERPOLATION_TYPE_AREA:
            downscaler_param->interpolation_method = 1;
            break;
        case VX_INTERPOLATION_TYPE_BILINEAR:
            downscaler_param->interpolation_method = 0;
            break;
        default:
            VXLOGE("un-defined enum: 0x%x", intepolation_value);
            break;
        }
    } else {
        scaler = getTask(0)->getPu(VPU_PU_UPSCALER0, 1, 0);
        if (scaler == NULL) {
            VXLOGE("getting pu fails");
            status = VX_FAILURE;
            goto EXIT;
        }
        struct vpul_pu_upscaler *upscaler_param;
        upscaler_param = (struct vpul_pu_upscaler*)scaler->getParameter();
        switch (intepolation_value) {
        case VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR:
            upscaler_param->interpolation_method = 2;
            break;
        case VX_INTERPOLATION_TYPE_AREA:
            upscaler_param->interpolation_method = 1;
            break;
        case VX_INTERPOLATION_TYPE_BILINEAR:
            upscaler_param->interpolation_method = 0;
            break;
        default:
            VXLOGE("un-defined enum: 0x%x", intepolation_value);
            break;
        }
    }

    ExynosVpuIoSizeScale *scale;
    scale = (ExynosVpuIoSizeScale*)scaler->getOutIoSize();
    if (scale->getType() != VPUL_SIZEOP_SCALE) {
        VXLOGE("io size should be scale type, %d", scale->getType());
        status = VX_FAILURE;
        goto EXIT;
    }

    ExynosVpuIoSizeOpScale *scale_op;
    struct vpul_scales *scale_info;
    scale_op = scale->getSizeOpScale();
    scale_info = scale_op->getScaleInfo();
    scale_info->horizontal.numerator = m_dst_width;
    scale_info->horizontal.denominator = m_src_width;
    scale_info->vertical.numerator = m_dst_height;
    scale_info->vertical.denominator = m_src_height;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelScaleImage::initVxIo(const vx_reference *parameters)
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

