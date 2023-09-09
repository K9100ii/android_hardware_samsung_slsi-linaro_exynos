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

#define LOG_TAG "ExynosVpuMicroKernelFastCorners"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuMicroKernelFastCorners.h"

#include "vpu_kernel_util.h"

namespace android {

using namespace std;

vx_status
ExynosVpuMicroKernelFastCorners0::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if ((status == VX_SUCCESS) && (input)) {
                vx_df_image format = 0;
                status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if ((status == VX_SUCCESS) && (format == VX_DF_IMAGE_U8)) {
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    }
    if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar sens = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
            if ((status == VX_SUCCESS) && (sens)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 k = 0.0f;
                    status = vxReadScalarValue(sens, &k);
                    if ((status == VX_SUCCESS) && (k > 0) && (k < 256)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&sens);
            }
            vxReleaseParameter(&param);
        }
    }
    if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar s_nonmax = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &s_nonmax, sizeof(s_nonmax));
            if ((status == VX_SUCCESS) && (s_nonmax)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(s_nonmax, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_BOOL) {
                    vx_bool nonmax = vx_false_e;
                    status = vxReadScalarValue(s_nonmax, &nonmax);
                    if ((status == VX_SUCCESS) && ((nonmax == vx_false_e) || (nonmax == vx_true_e))) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&s_nonmax);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners0::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    if (!node) {
        VXLOGE("node is null");
    }

    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, 0);
        if (param) {
            vx_image inimage = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &inimage, sizeof(inimage));
            if (inimage) {
                vx_uint32 width = 0, height = 0;
                vxQueryImage(inimage, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(inimage, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vx_df_image format = VX_DF_IMAGE_U8;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&inimage);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

ExynosVpuMicroKernelFastCorners0::ExynosVpuMicroKernelFastCorners0(vx_char *name, vx_uint32 param_num)
                                                                                                       : ExynosVpuKernel(name, param_num)
{
    m_thresh = 0;
    m_nms_enable = vx_false_e;
}

ExynosVpuMicroKernelFastCorners0::~ExynosVpuMicroKernelFastCorners0(void)
{

}

vx_status
ExynosVpuMicroKernelFastCorners0::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_scalar thresh = (vx_scalar)parameters[1];
    vx_scalar nms = (vx_scalar)parameters[2];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    status = vxReadScalarValue(thresh, &m_thresh);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    status = vxReadScalarValue(nms, &m_nms_enable);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners0::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if 0
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
ExynosVpuMicroKernelFastCorners0::initTaskFromApi(void)
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
ExynosVpuMicroKernelFastCorners0::initTask0FromApi(void)
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

    ExynosVpuPu *dma_in = pu_factory.createPu(nlf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *nlf = pu_factory.createPu(nlf_subchain, VPU_PU_NLF);
    nlf->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(nlf_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, nlf, 0);
    ExynosVpuPu::connect(nlf, 0, dma_out, 0);

    struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();
    dma_out_param->inout_index = 0; // TBD, intermediate buffer

    struct vpul_pu_nlf *nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();

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
ExynosVpuMicroKernelFastCorners0::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */

EXIT:
    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners0::initVxIo(const vx_reference *parameters)
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

    memset(&param_info, 0x0, sizeof(param_info));
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 3, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners1::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if ((status == VX_SUCCESS) && (input)) {
                vx_df_image format = 0;
                status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if ((status == VX_SUCCESS) && (format == VX_DF_IMAGE_U8)) {
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if ((status == VX_SUCCESS) && (input)) {
                vx_df_image format = 0;
                status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if ((status == VX_SUCCESS) && (format == VX_DF_IMAGE_U8)) {
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar sens = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
            if ((status == VX_SUCCESS) && (sens)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 k = 0.0f;
                    status = vxReadScalarValue(sens, &k);
                    if ((status == VX_SUCCESS) && (k > 0) && (k < 256)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&sens);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar s_nonmax = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &s_nonmax, sizeof(s_nonmax));
            if ((status == VX_SUCCESS) && (s_nonmax)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(s_nonmax, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_BOOL) {
                    vx_bool nonmax = vx_false_e;
                    status = vxReadScalarValue(s_nonmax, &nonmax);
                    if ((status == VX_SUCCESS) && ((nonmax == vx_false_e) || (nonmax == vx_true_e))) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&s_nonmax);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners1::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    if (!node) {
        VXLOGE("node is null");
    }

    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 4) {
        vx_enum array_item_type = VX_TYPE_KEYPOINT;
        vx_size array_capacity = 0;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    } else if (index == 5) {
        vx_enum scalar_type = VX_TYPE_SIZE;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }

    return status;
}

ExynosVpuMicroKernelFastCorners1::ExynosVpuMicroKernelFastCorners1(vx_char *name, vx_uint32 param_num)
                                                                                                       : ExynosVpuKernel(name, param_num)
{
    m_thresh = 0;
    m_nms_enable = vx_false_e;
    m_corner_num_enable = vx_false_e;

    m_corner_num_ptr = NULL;
}

ExynosVpuMicroKernelFastCorners1::~ExynosVpuMicroKernelFastCorners1(void)
{
    if (m_corner_num_ptr)
        free(m_corner_num_ptr);
}

vx_status
ExynosVpuMicroKernelFastCorners1::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_scalar thresh = (vx_scalar)parameters[2];
    vx_scalar nms = (vx_scalar)parameters[3];
    vx_array out_corner_array = (vx_array)parameters[4];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    status = vxReadScalarValue(thresh, &m_thresh);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    status = vxReadScalarValue(nms, &m_nms_enable);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    if (parameters[5]) {
        m_corner_num_enable = vx_true_e;
        m_corner_num_ptr = (vx_size*)malloc(sizeof(*m_corner_num_ptr));
        *m_corner_num_ptr = 0;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners1::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if 0
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
ExynosVpuMicroKernelFastCorners1::initTaskFromBinary(void)
{
    return VX_FAILURE;
}

vx_status
ExynosVpuMicroKernelFastCorners1::initTaskFromApi(void)
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
ExynosVpuMicroKernelFastCorners1::initTask0FromApi(void)
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
    ExynosVpuProcess *fast_second_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, fast_second_process);
    ExynosVpuVertex::connect(fast_second_process, end_vertex);

    ExynosVpuIoSizeInout*iosize = new ExynosVpuIoSizeInout(fast_second_process);

    /* define subchain */
    ExynosVpuSubchainHw *fsat_second_subchain = new ExynosVpuSubchainHw(fast_second_process);

    /* define pu */
    ExynosVpuPu *dma_in1 = pu_factory.createPu(fsat_second_subchain, VPU_PU_DMAIN0);
    dma_in1->setSize(iosize, iosize);
    ExynosVpuPu *dma_in2 = pu_factory.createPu(fsat_second_subchain, VPU_PU_DMAIN_WIDE0);
    dma_in2->setSize(iosize, iosize);
    ExynosVpuPu *nlf = pu_factory.createPu(fsat_second_subchain, VPU_PU_NLF);
    nlf->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(fsat_second_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *nms = pu_factory.createPu(fsat_second_subchain, VPU_PU_NMS);
    nms->setSize(iosize, iosize);
    ExynosVpuPu *map2list = pu_factory.createPu(fsat_second_subchain, VPU_PU_MAP2LIST);
    map2list->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(fsat_second_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in1, 0, nlf, 0);
    ExynosVpuPu::connect(nlf, 0, salb, 0);
    ExynosVpuPu::connect(dma_in2, 0, salb, 1);
    ExynosVpuPu::connect(salb, 0, nms, 0);
    ExynosVpuPu::connect(nms, 0, map2list, 0);
    ExynosVpuPu::connect(map2list, 0, dma_out, 0);

    struct vpul_pu_dma *dma_in1_param = (struct vpul_pu_dma*)dma_in1->getParameter();
    struct vpul_pu_dma *dma_in2_param = (struct vpul_pu_dma*)dma_in2->getParameter();
    dma_in2_param->inout_index = 0; // TBD, intermediate buffer

    struct vpul_pu_nlf *nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();
    struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();

    struct vpul_pu_nms *nms_param = (struct vpul_pu_nms*)nms->getParameter();

    struct vpul_pu_map2list *map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(fast_second_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(fast_second_process, fixed_roi);
    dma_in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(fast_second_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(fast_second_process, fixed_roi);
    dma_in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(fast_second_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(fast_second_process, fixed_roi);
    dma_out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in1);
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 1, dma_in2);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out);
    if (m_corner_num_enable)
        ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 1, map2list);
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
ExynosVpuMicroKernelFastCorners1::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */

EXIT:
    return status;
}

vx_status
ExynosVpuMicroKernelFastCorners1::initVxIo(const vx_reference *parameters)
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

    memset(&param_info, 0x0, sizeof(param_info));
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 4, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    if (m_corner_num_enable) {
        param_info.scalar.ptr = m_corner_num_ptr;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 1, 5, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

EXIT:
    return status;
}

}; /* namespace android */

