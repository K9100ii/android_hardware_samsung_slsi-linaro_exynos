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

#define LOG_TAG "ExynosVpuKernelHalfScaleGaussian"
#include <cutils/log.h>

#include "ExynosVpuKernelHalfScaleGaussian.h"

#include "vpu_kernel_util.h"

#include "td-binary-halfscalegaussian.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_halfscalegaussian_from_VDE_DS;

vx_status
ExynosVpuKernelHalfScaleGaussian::inputValidator(vx_node node, vx_uint32 index)
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
                if (stype == VX_TYPE_INT32) {
                    vx_int32 ksize = 0;
                    vxReadScalarValue(scalar, &ksize);
                    if ((ksize == 3) || (ksize == 5)) {
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
ExynosVpuKernelHalfScaleGaussian::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
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
                vx_uint32 w1 = 0, h1 = 0;
                vx_df_image f1 = VX_DF_IMAGE_VIRT;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &w1, sizeof(w1));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &h1, sizeof(h1));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &f1, sizeof(f1));

                /* fill in the meta data with the attributes so that the checker will pass */
                vx_uint32 width = (w1 + 1) / 2;
                vx_uint32 height = (h1 + 1) / 2;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &f1, sizeof(f1));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
            }
            if (src) vxReleaseImage(&src);
            if (dst) vxReleaseImage(&dst);
            vxReleaseParameter(&src_param);
            vxReleaseParameter(&dst_param);
        }
    }

    return status;
}

ExynosVpuKernelHalfScaleGaussian::ExynosVpuKernelHalfScaleGaussian(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "halfscalegaussian");
}

ExynosVpuKernelHalfScaleGaussian::~ExynosVpuKernelHalfScaleGaussian(void)
{

}

vx_status
ExynosVpuKernelHalfScaleGaussian::setupBaseVxInfo(const vx_reference parameters[])
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
ExynosVpuKernelHalfScaleGaussian::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelHalfScaleGaussian::initTaskFromBinary(void)
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
ExynosVpuKernelHalfScaleGaussian::initTask0FromBinary(void)
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
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)3);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelHalfScaleGaussian::initTaskFromApi(void)
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
ExynosVpuKernelHalfScaleGaussian::initTask0FromApi(void)
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

    ExynosVpuIoSizeOpScale *sizeop_scale = new ExynosVpuIoSizeOpScale(scale_process);
    ExynosVpuIoSizeScale *iosize_down = new ExynosVpuIoSizeScale(scale_process, iosize, sizeop_scale);

    struct vpul_scales *scale_info;
    scale_info = sizeop_scale->getScaleInfo();
    scale_info->horizontal.denominator = 2;
    scale_info->horizontal.numerator = 1;
    scale_info->vertical.denominator = 2;
    scale_info->vertical.numerator = 1;

    /* define subchain */
    ExynosVpuSubchainHw *scale_subchain = new ExynosVpuSubchainHw(scale_process);

    /* define pu */
    ExynosVpuPu *dma_in = pu_factory.createPu(scale_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *slf7x7 = pu_factory.createPu(scale_subchain, VPU_PU_SLF50);
    slf7x7->setSize(iosize, iosize);
    ExynosVpuPu *ds = pu_factory.createPu(scale_subchain, VPU_PU_DNSCALER0);
    ds->setSize(iosize, iosize_down);
    ExynosVpuPu *dma_out = pu_factory.createPu(scale_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, slf7x7, 0);
    ExynosVpuPu::connect(slf7x7, 0, ds, 0);
    ExynosVpuPu::connect(ds, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in_param = (struct vpul_pu_dma*)dma_in->getParameter();
    struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf7x7->getParameter();
    struct vpul_pu_downscaler *ds_param = (struct vpul_pu_downscaler*)ds->getParameter();
    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();

    slf_param->border_mode_up = 1;
    slf_param->border_mode_down = 1;
    slf_param->border_mode_left = 1;
    slf_param->border_mode_right = 1;
    slf_param->coefficient_fraction = 2;
    slf_param->maxp_sizes_filt_hor = 7;
    slf_param->maxp_sizes_filt_ver = 7;

    ds_param->interpolation_method = 2;

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

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(scale_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(scale_process, fixed_roi);
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
ExynosVpuKernelHalfScaleGaussian::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    int32_t slf_coff_3x3[] = {1, 2, 1, 1, 2, 1};
    int32_t slf_coff_5x5[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};

    /* update vpu param from vx param */
    vx_scalar kernel_size = (vx_scalar)parameters[2];

    vx_border_mode_t border;
    status = vxQueryNode(node, VX_NODE_ATTRIBUTE_BORDER_MODE, &border, sizeof(border));
    if (status != VX_SUCCESS) {
        VXLOGE("querying node, err:%d", status);
        goto EXIT;
    }

    vx_int32 ksize;
    status = vxReadScalarValue(kernel_size, &ksize);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar value fails");
        goto EXIT;
    }

#if 1
    /* JUN_TBD, enable after test */
    ExynosVpuPu *slf;
    slf = getTask(0)->getPu(VPU_PU_SLF50, 1, 0);
    if (slf == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_slf *slf_param;
    slf_param = (struct vpul_pu_slf*)slf->getParameter();
    switch (ksize) {
    case 3:
        slf_param->filter_size_mode = 1;
        ((ExynosVpuPuSlf*)slf)->setStaticCoffValue(slf_coff_3x3, 3, sizeof(slf_coff_3x3)/sizeof(slf_coff_3x3[0]));
        break;
    case 5:
        slf_param->filter_size_mode = 2;
        ((ExynosVpuPuSlf*)slf)->setStaticCoffValue(slf_coff_5x5, 5, sizeof(slf_coff_5x5)/sizeof(slf_coff_5x5[0]));
        break;
    default:
        VXLOGE("un-supported kernel size: %d", ksize);
        break;
    }

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
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelHalfScaleGaussian::initVxIo(const vx_reference *parameters)
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

