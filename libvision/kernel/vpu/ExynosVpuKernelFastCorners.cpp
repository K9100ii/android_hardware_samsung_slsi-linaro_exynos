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

#define LOG_TAG "ExynosVpuKernelFastCorners"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuKernelFastCorners.h"

#include "vpu_kernel_util.h"

#include "td-binary-fastcorners.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_fastcorners_from_VDE_DS;

vx_status
ExynosVpuKernelFastCorners::inputValidator(vx_node node, vx_uint32 index)
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
ExynosVpuKernelFastCorners::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    if (!node) {
        VXLOGE("node is null");
    }

    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 3) {
        vx_enum array_item_type = VX_TYPE_KEYPOINT;
        vx_size array_capacity = 0;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    } else if (index == 4) {
        vx_enum scalar_type = VX_TYPE_SIZE;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }

    return status;
}

ExynosVpuKernelFastCorners::ExynosVpuKernelFastCorners(vx_char *name, vx_uint32 param_num)
                                                                                                       : ExynosVpuKernel(name, param_num)
{
    m_nms_enable = vx_false_e;
    m_array_capacity = 0;
    strcpy(m_task_name, "fastcorner");
}

ExynosVpuKernelFastCorners::~ExynosVpuKernelFastCorners(void)
{

}

vx_status
ExynosVpuKernelFastCorners::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_scalar thresh = (vx_scalar)parameters[1];
    vx_scalar nms = (vx_scalar)parameters[2];
    vx_array out_corner_array = (vx_array)parameters[3];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    status = vxReadScalarValue(nms, &m_nms_enable);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails, err:%d", status);
        goto EXIT;
    }

    status = vxQueryArray(out_corner_array, VX_ARRAY_ATTRIBUTE_CAPACITY, &m_array_capacity, sizeof(m_array_capacity));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelFastCorners::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if 1
    status = initTaskFromBinary();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from binary fails, %p %p", node, parameters);
        goto EXIT;
    }
#else
    status = initTaskFromApi();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from api fails, %p %p", node, parameters);
        goto EXIT;
    }
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelFastCorners::initTaskFromBinary(void)
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
ExynosVpuKernelFastCorners::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapperFast *task_wr = new ExynosVpuTaskIfWrapperFast(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    if (m_nms_enable) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary);
    } else {
        VXLOGE("td binary doesn't support nms disable mode");
        ret  = INVALID_OPERATION;
    }
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu for intermediate between subchain */
    task_if->setInterPair(2, 5);

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)3);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)10);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelFastCorners::initTaskFromApi(void)
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
ExynosVpuKernelFastCorners::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperFast *task_wr = new ExynosVpuTaskIfWrapperFast(this, 0);
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
    ExynosVpuProcess *first_half_process = new ExynosVpuProcess(task);
    ExynosVpuProcess *second_half_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, first_half_process);
    ExynosVpuVertex::connect(first_half_process, second_half_process);
    ExynosVpuVertex::connect(second_half_process, end_vertex);

    ExynosVpuIoSizeInout *size_first_half = new ExynosVpuIoSizeInout(first_half_process);
    ExynosVpuIoSizeInout *size_second_half = new ExynosVpuIoSizeInout(second_half_process);

    /* define subchain */
    ExynosVpuSubchainHw *first_half_subchain = new ExynosVpuSubchainHw(first_half_process);
    ExynosVpuSubchainHw *second_half_subchain = new ExynosVpuSubchainHw(second_half_process);

    /* define pu */
    ExynosVpuPu *io_dma_in1;
    ExynosVpuPu *io_dma_in2;
    ExynosVpuPu *io_dma_out2;
    ExynosVpuPu *io_map2list_out;

    ExynosVpuPu *inter_dma_out1;
    ExynosVpuPu *inter_dma_in1;

    ExynosVpuIoExternalMem *inter_external_mem = new ExynosVpuIoExternalMem(task);

    {
        ExynosVpuPu *dma_in = pu_factory.createPu(first_half_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(size_first_half, size_first_half);
        ExynosVpuPu *nlf = pu_factory.createPu(first_half_subchain, VPU_PU_NLF);
        nlf->setSize(size_first_half, size_first_half);
        ExynosVpuPu *dma_out = pu_factory.createPu(first_half_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(size_first_half, size_first_half);

        ExynosVpuPu::connect(dma_in, 0, nlf, 0);
        ExynosVpuPu::connect(nlf, 0, dma_out, 0);

        ExynosVpuMemmapExternal *memmap = new ExynosVpuMemmapExternal(task, inter_external_mem);
        struct vpul_memory_map_desc *memmap_info = memmap->getMemmapInfo();
        ExynosVpuIoFixedMapRoi *fixed_roi = new ExynosVpuIoFixedMapRoi(first_half_process, memmap);
        ExynosVpuIoTypesDesc *iotyps = new ExynosVpuIoTypesDesc(first_half_process, fixed_roi);
        dma_out->setIoTypesDesc(iotyps);
        inter_dma_out1 = dma_out;

        struct vpul_pu_dma *dma_out_param = (struct vpul_pu_dma*)dma_out->getParameter();
        dma_out_param->inout_index = 0; // TBD, intermediate buffer

        struct vpul_pu_nlf *nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();
        io_dma_in1 = dma_in;
    }

    {
        ExynosVpuPu *dma_in1 = pu_factory.createPu(second_half_subchain, VPU_PU_DMAIN0);
        dma_in1->setSize(size_second_half, size_second_half);
        ExynosVpuPu *dma_in2 = pu_factory.createPu(second_half_subchain, VPU_PU_DMAIN_WIDE0);
        dma_in2->setSize(size_second_half, size_second_half);
        ExynosVpuPu *nlf = pu_factory.createPu(second_half_subchain, VPU_PU_NLF);
        nlf->setSize(size_second_half, size_second_half);
        ExynosVpuPu *salb = pu_factory.createPu(second_half_subchain, VPU_PU_SALB0);
        salb->setSize(size_second_half, size_second_half);
        ExynosVpuPu *nms = NULL;
        ExynosVpuPu *map2list = pu_factory.createPu(second_half_subchain, VPU_PU_MAP2LIST);
        map2list->setSize(size_second_half, size_second_half);
        ExynosVpuPu *dma_out2 = pu_factory.createPu(second_half_subchain, VPU_PU_DMAOT_WIDE0);
        dma_out2->setSize(size_second_half, size_second_half);

        if (m_nms_enable) {
            ExynosVpuPu *nms = pu_factory.createPu(second_half_subchain, VPU_PU_NMS);
            nms->setSize(size_second_half, size_second_half);
            struct vpul_pu_nms *nms_param = (struct vpul_pu_nms*)nms->getParameter();

            ExynosVpuPu::connect(dma_in1, 0, nlf, 0);
            ExynosVpuPu::connect(nlf, 0, salb, 0);
            ExynosVpuPu::connect(dma_in2, 0, salb, 1);
            ExynosVpuPu::connect(salb, 0, nms, 0);
            ExynosVpuPu::connect(nms, 0, map2list, 0);
            ExynosVpuPu::connect(map2list, 1, dma_out2, 0);
        } else {
            ExynosVpuPu::connect(dma_in1, 0, nlf, 0);
            ExynosVpuPu::connect(nlf, 0, salb, 0);
            ExynosVpuPu::connect(dma_in2, 0, salb, 1);
            ExynosVpuPu::connect(salb, 0, map2list, 0);
            ExynosVpuPu::connect(map2list, 1, dma_out2, 0);
        }

        ExynosVpuMemmapExternal *memmap = new ExynosVpuMemmapExternal(task, inter_external_mem);
        struct vpul_memory_map_desc *memmap_info = memmap->getMemmapInfo();
        ExynosVpuIoFixedMapRoi *fixed_roi = new ExynosVpuIoFixedMapRoi(second_half_process, memmap);
        ExynosVpuIoTypesDesc *iotyps = new ExynosVpuIoTypesDesc(second_half_process, fixed_roi);
        dma_in2->setIoTypesDesc(iotyps);
        inter_dma_in1 = dma_in2;

        struct vpul_pu_dma *dma_in1_param = (struct vpul_pu_dma*)dma_in1->getParameter();
        struct vpul_pu_dma *dma_in2_param = (struct vpul_pu_dma*)dma_in2->getParameter();
        struct vpul_pu_nlf *nlf_param = (struct vpul_pu_nlf*)nlf->getParameter();
        struct vpul_pu_salb *salb_param = (struct vpul_pu_salb*)salb->getParameter();
        struct vpul_pu_map2list *map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();

        io_dma_in2 = dma_in1;
        io_dma_out2 = dma_out2;
        io_map2list_out = map2list;
    }

    /* connect pu for intermediate between subchain */
    status_t ret = NO_ERROR;
    ret = task_if->setInterPair(inter_dma_out1, inter_dma_in1);
    if (ret != NO_ERROR) {
        VXLOGE("connecting intermediate fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(first_half_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(first_half_process, fixed_roi);
    io_dma_in1->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(second_half_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(second_half_process, fixed_roi);
    io_dma_in2->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(second_half_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(second_half_process, fixed_roi);
    io_dma_out2->setIoTypesDesc(iotyps);

    ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_dma_in1);
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 1, io_dma_in2);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, io_dma_out2);
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
ExynosVpuKernelFastCorners::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    vx_float32 strength_thresh;
    status = vxReadScalarValue((vx_scalar)parameters[1], &strength_thresh);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar, err:%d", status);
        goto EXIT;
    }

    if (!m_nms_enable) {
        ExynosVpuPu *nms;
        nms = getTask(0)->getPu(VPU_PU_NMS, 1, 1);
        if (nms == NULL) {
            VXLOGE("getting pu fails");
            status = VX_FAILURE;
            goto EXIT;
        }
        nms->setBypass(true);
    }

    ExynosVpuPu *map2list;
    map2list = getTask(0)->getPu(VPU_PU_MAP2LIST, 1, 1);
    if (map2list == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    struct vpul_pu_map2list *map2list_param;
    map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();
    map2list_param->threshold_low = (uint32_t)strength_thresh;
    map2list_param->num_of_point = m_array_capacity;

EXIT:
    return status;
}

vx_status
ExynosVpuKernelFastCorners::initVxIo(const vx_reference *parameters)
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 1, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* for array */
    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    vx_uint32 byte_per_record, byte_per_line, line_number;
    byte_per_record = sizeof(struct key_triple);  /* 6B, (x, y, val) each 16-bit */
    byte_per_line = (byte_per_record==8) ? 240 : 252;
    line_number = ceilf((float)byte_per_record * (float)m_array_capacity / (float)byte_per_line);

    vx_uint32 total_array_size;
    total_array_size = byte_per_line * line_number + 2;

    vx_uint32 width;
    vx_uint32 height;
    if (total_array_size > 2000) {
        width = 2000;
        height = (total_array_size/width) + 1;
    } else {
        width = total_array_size;
        height = 1;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = width;
    io_format.height = height;
    io_format.pixel_byte = 1;

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating custom buffer for feature list */
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = true;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperFast::ExynosVpuTaskIfWrapperFast(ExynosVpuKernelFastCorners *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperFast::postProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_array vx_array_object;
    vx_scalar num_corners_scalar;
    vx_array_object = (vx_array)parameters[3];
    num_corners_scalar = (vx_scalar)parameters[4];

    vx_size capacity;
    status = vxQueryArray(vx_array_object, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }

    status = vxAddEmptyArrayItems(vx_array_object, capacity);
    if (status != VX_SUCCESS) {
        VXLOGE("adding empty array fails");
        goto EXIT;
    }

    vx_keypoint_t *keypoint_array;
    vx_size array_stride;
    keypoint_array = NULL;
    array_stride = 0;
    status = vxAccessArrayRange(vx_array_object, 0, capacity, &array_stride, (void**)&keypoint_array, VX_WRITE_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array fails");
        goto EXIT;
    }
    if (array_stride != sizeof(vx_keypoint_t)) {
        VXLOGE("stride is not matching, %d, %d", array_stride, sizeof(vx_keypoint_t));
        status = VX_FAILURE;
        goto EXIT;
    }

    if (m_out_io_param_info[0].type != IO_PARAM_CUSTOM) {
        VXLOGE("io type is not matching");
        status = VX_FAILURE;
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    struct key_triple *keytriple_array;
    vx_uint32 keytriple_array_size;
    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];

    /* two byte for blank is made by firmware */
    keytriple_array = (struct key_triple*)(io_buffer_info->addr+2);

    vx_uint32 i;
    vx_size found_point_num;
    found_point_num = 0;
    for (i=0; i<capacity; i++) {
        if (keytriple_array[i].val == 0) {
            break;
        }

        found_point_num++;
        keypoint_array[i].x = keytriple_array[i].x;
        keypoint_array[i].y = keytriple_array[i].y;
        keypoint_array[i].strength = keytriple_array[i].val;
        keypoint_array[i].scale = 0;
        keypoint_array[i].orientation = 0;
        keypoint_array[i].tracking_status = 1;
        keypoint_array[i].error = 0;
    }

    status = vxCommitArrayRange(vx_array_object, 0, capacity, keypoint_array);
    if (status != VX_SUCCESS) {
        VXLOGE("commit array fails");
        goto EXIT;
    }

    if (i != capacity)
        vxTruncateArray(vx_array_object, i);

    if (num_corners_scalar) {
        status = vxWriteScalarValue(num_corners_scalar, &found_point_num);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
            goto EXIT;
        }
    }

EXIT:
    return status;
}

}; /* namespace android */

