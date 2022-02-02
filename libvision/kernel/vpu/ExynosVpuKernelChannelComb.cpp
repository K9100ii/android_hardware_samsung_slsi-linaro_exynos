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

#define LOG_TAG "ExynosVpuKernelChannelComb"
#include <cutils/log.h>

#include "ExynosVpuKernelChannelComb.h"

#include "vpu_kernel_util.h"

#include "td-binary-channelcomb_rgb.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_rgb[] =
    TASK_test_binary_channelcomb_rgb_from_VDE_DS;

vx_status
ExynosVpuKernelChannelComb::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index < 4) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image image = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
            if (image) {
                vx_df_image format = 0;
                vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if (format == VX_DF_IMAGE_U8) {
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&image);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

vx_status
ExynosVpuKernelChannelComb::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 4) {
        vx_uint32 p, width = 0, height = 0;
        vx_uint32 uv_x_scale = 0, uv_y_scale = 0;
        vx_parameter params[] = {
                vxGetParameterByIndex(node, 0),
                vxGetParameterByIndex(node, 1),
                vxGetParameterByIndex(node, 2),
                vxGetParameterByIndex(node, 3),
                vxGetParameterByIndex(node, index)
        };
        vx_bool planes_present[4] = { vx_false_e, vx_false_e, vx_false_e, vx_false_e };
        /* check for equal plane sizes and determine plane presence */
        for (p = 0; p < index; p++) {
            if (params[p]) {
                vx_image image = 0;
                vxQueryParameter(params[p], VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
                planes_present[p] = (vx_bool)(image != 0);

                if (image) {
                    uint32_t w = 0, h = 0;
                    vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &w, sizeof(w));
                    vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &h, sizeof(h));
                    if (width == 0 && height == 0) {
                        width = w;
                        height = h;
                    } else if (uv_x_scale == 0 && uv_y_scale == 0) {
                        uv_x_scale = width == w ? 1 : (width == 2*w ? 2 : 0);
                        uv_y_scale = height == h ? 1 : (height == 2*h ? 2 : 0);
                        if (uv_x_scale == 0 || uv_y_scale == 0 || uv_y_scale > uv_x_scale) {
                            status = VX_ERROR_INVALID_DIMENSION;
                            vxAddLogEntry((vx_reference)image, status, "Input image channel %u does not match in dimensions!\n", p);
                            goto exit;
                        }
                    } else if (width != w * uv_x_scale || height != h * uv_y_scale) {
                        status = VX_ERROR_INVALID_DIMENSION;
                        vxAddLogEntry((vx_reference)image, status, "Input image channel %u does not match in dimensions!\n", p);
                        goto exit;
                    }
                    vxReleaseImage(&image);
                }
            }
        }

        if (params[index]) {
            vx_image output = 0;
            vxQueryParameter(params[index], VX_PARAMETER_ATTRIBUTE_REF, &output, sizeof(output));
            if (output) {
                vx_df_image format = VX_DF_IMAGE_VIRT;
                vx_bool supported_format = vx_true_e;
                vx_bool correct_planes = (vx_bool)(planes_present[0] && planes_present[1] && planes_present[2]);

                vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                switch (format) {
                case VX_DF_IMAGE_RGB:
                case VX_DF_IMAGE_YUV4:
                    correct_planes = (vx_bool)(correct_planes && uv_y_scale == 1 && uv_x_scale == 1);
                    break;
                case VX_DF_IMAGE_RGBX:
                    correct_planes = (vx_bool)(correct_planes && planes_present[3] && uv_y_scale == 1 && uv_x_scale == 1);
                    break;
                case VX_DF_IMAGE_YUYV:
                case VX_DF_IMAGE_UYVY:
                    correct_planes = (vx_bool)(correct_planes && uv_y_scale == 1 && uv_x_scale == 2);
                    break;
                case VX_DF_IMAGE_NV12:
                case VX_DF_IMAGE_NV21:
                case VX_DF_IMAGE_IYUV:
                    correct_planes = (vx_bool)(correct_planes && uv_y_scale == 2 && uv_x_scale == 2);
                    break;
                default:
                    supported_format = vx_false_e;
                }
                if (supported_format) {
                    if (correct_planes) {
                        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                        status = VX_SUCCESS;
                    } else {
                        printf("Valid format but missing planes!\n");
                    }
                }
                vxReleaseImage(&output);
            }
        }

exit:
        for (p = 0; p < dimof(params); p++) {
            if (params[p]) {
                vxReleaseParameter(&params[p]);
            }
        }
    }

    return status;
}

ExynosVpuKernelChannelComb::ExynosVpuKernelChannelComb(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_dst_format = VX_DF_IMAGE_VIRT;

    m_src_io_num = 0;
    m_dst_io_num = 0;

    strcpy(m_task_name, "chcombine");
}

ExynosVpuKernelChannelComb::~ExynosVpuKernelChannelComb(void)
{

}


vx_status
ExynosVpuKernelChannelComb::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    vx_image input = (vx_image)parameters[0];
    vx_image output = (vx_image)parameters[4];

    status = vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &m_dst_format, sizeof(m_dst_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image format fails, err:%d", status);
        return status;
    }

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    for (vx_uint32 i=0; i<4; i++) {
        if (parameters[i])
            m_src_io_num++;
    }

    status = vxQueryImage(output, VX_IMAGE_ATTRIBUTE_PLANES, &m_dst_io_num, sizeof(m_dst_io_num));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image planes fails, err:%d", status);
        return status;
    }

    return status;
}

vx_status
ExynosVpuKernelChannelComb::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelChannelComb::initTaskFromBinary(void)
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
ExynosVpuKernelChannelComb::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_rgb);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_IN, 2, (uint32_t)3);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)4);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelChannelComb::initTaskFromApi(void)
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
ExynosVpuKernelChannelComb::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

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
    ExynosVpuProcess *chcomb_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, chcomb_process);
    ExynosVpuVertex::connect(chcomb_process, end_vertex);

    /* define subchain */
    ExynosVpuSubchainHw *chcomb_subchain = new ExynosVpuSubchainHw(chcomb_process);

    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_in_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> main_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> post_out_pu_list;

    pair<ExynosVpuPu*, vx_uint32> pu;

    status = setPreChCombPu(chcomb_subchain, &pre_in_pu_list, &pre_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setMainChCombPu(chcomb_subchain, &pre_out_pu_list, &main_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setPostChCombPu(chcomb_subchain, &main_out_pu_list, &post_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }

    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *pu_memmap;
    ExynosVpuIoFixedMapRoi *pu_fixed_roi;
    ExynosVpuIoTypesDesc *pu_iotyps;

    /* connect pu to io */
    vx_uint32 i;
    for (i=0; i<m_src_io_num; i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        pu_fixed_roi = new ExynosVpuIoFixedMapRoi(chcomb_process, pu_memmap);
        pu_iotyps = new ExynosVpuIoTypesDesc(chcomb_process, pu_fixed_roi);

        pu = pre_in_pu_list.editItemAt(i);
        pu.first->setIoTypesDesc(pu_iotyps);
        task_if->setIoPu(VS4L_DIRECTION_IN, i, pu.first);
    }
    for (i=0; i<m_dst_io_num; i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        pu_fixed_roi = new ExynosVpuIoFixedMapRoi(chcomb_process, pu_memmap);
        pu_iotyps = new ExynosVpuIoTypesDesc(chcomb_process, pu_fixed_roi);

        pu = post_out_pu_list.editItemAt(i);
        pu.first->setIoTypesDesc(pu_iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, i, pu.first);
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelChannelComb::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelChannelComb::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    /* 0~3 is the input param */
    vx_uint32 i;
    for (i=0; i<m_src_io_num; i++) {
        param_info.image.plane = 0;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, i, i, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

    for (i=0; i<m_dst_io_num; i++) {
        param_info.image.plane = i;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, i, 4, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelChannelComb::setPreChCombPu(ExynosVpuSubchainHw *subchain,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_in_pu_list,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    ExynosVpuPuFactory pu_factory;
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());

    ExynosVpuPu *dma_in1 = NULL;
    ExynosVpuPu *dma_in2 = NULL;
    ExynosVpuPu *dma_in3 = NULL;
    ExynosVpuPu *dma_in4 = NULL;

    switch (m_dst_format) {
    case VX_DF_IMAGE_RGBX:
        dma_in1 = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in1->setSize(iosize, iosize);
        dma_in2 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_in2->setSize(iosize, iosize);
        dma_in3 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM1);
        dma_in3->setSize(iosize, iosize);
        dma_in4 = pu_factory.createPu(subchain, VPU_PU_DMAIN_WIDE0);
        dma_in4->setSize(iosize, iosize);
        break;

    case VX_DF_IMAGE_YUYV:
    case VX_DF_IMAGE_UYVY:
        dma_in1 = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in1->setSize(iosize, iosize);
        dma_in2 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_in2->setSize(iosize, iosize);
        dma_in3 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM1);
        dma_in3->setSize(iosize, iosize);
        break;
    default:
        dma_in1 = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in1->setSize(iosize, iosize);
        dma_in2 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_in2->setSize(iosize, iosize);
        dma_in3 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM1);
        dma_in3->setSize(iosize, iosize);
        break;
    }

    if (dma_in1) {
        ret_in_pu_list->push_back(make_pair(dma_in1, 0));
        ret_out_pu_list->push_back(make_pair(dma_in1, 0));
    }
    if (dma_in2) {
        ret_in_pu_list->push_back(make_pair(dma_in2, 0));
        ret_out_pu_list->push_back(make_pair(dma_in2, 0));
    }
    if (dma_in3) {
        ret_in_pu_list->push_back(make_pair(dma_in3, 0));
        ret_out_pu_list->push_back(make_pair(dma_in3, 0));
    }
    if (dma_in4) {
        ret_in_pu_list->push_back(make_pair(dma_in4, 0));
        ret_out_pu_list->push_back(make_pair(dma_in4, 0));
    }

    if (ret_in_pu_list->size()  != m_src_io_num) {
        VXLOGE("the number of src io is not matching, %d, %d", ret_in_pu_list->size(), m_src_io_num);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

vx_status
ExynosVpuKernelChannelComb::setMainChCombPu(ExynosVpuSubchainHw *subchain,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());

    ExynosVpuPu *first_joiner = NULL;
    ExynosVpuPu *second_joiner = NULL;
    struct vpul_pu_joiner *first_joiner_param = NULL;
    struct vpul_pu_joiner *second_joiner_param = NULL;

    pair<ExynosVpuPu*, vx_uint32> in_pu;

    switch (m_dst_format) {
    case VX_DF_IMAGE_RGB:
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 1;
        first_joiner_param->input3_enable = 0;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 8;
        first_joiner_param->out_byte3_source_stream	= 16;

        for (vx_uint32 i=0; i<in_pu_list->size(); i++) {
            in_pu = in_pu_list->editItemAt(i);
            ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, i);
        }
        ret_out_pu_list->push_back(make_pair(first_joiner, 0));
        break;
    case VX_DF_IMAGE_RGBX:
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 1;
        first_joiner_param->input3_enable = 1;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 8;
        first_joiner_param->out_byte3_source_stream	= 12;

        for (vx_uint32 i=0; i<in_pu_list->size(); i++) {
            in_pu = in_pu_list->editItemAt(i);
            ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, i);
        }
        ret_out_pu_list->push_back(make_pair(first_joiner, 0));
        break;
    case VX_DF_IMAGE_YUV4:
    case VX_DF_IMAGE_IYUV:
        for (vx_uint32 i=0; i<in_pu_list->size(); i++) {
            ret_out_pu_list->push_back(in_pu_list->editItemAt(i));
        }
        break;
    case VX_DF_IMAGE_NV12:
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 0;
        first_joiner_param->input3_enable = 0;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 16;
        first_joiner_param->out_byte3_source_stream	= 16;

        in_pu = in_pu_list->editItemAt(0);
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));

        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 0);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 1);
        ret_out_pu_list->push_back(make_pair(first_joiner, 0));
        break;
    case VX_DF_IMAGE_NV21:
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 0;
        first_joiner_param->input3_enable = 0;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 16;
        first_joiner_param->out_byte3_source_stream	= 16;

        in_pu = in_pu_list->editItemAt(0);
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));

        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 1);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 0);
        ret_out_pu_list->push_back(make_pair(first_joiner, 0));
        break;
    case VX_DF_IMAGE_YUYV:
        /* combine U and V */
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 0;
        first_joiner_param->input3_enable = 0;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 16;
        first_joiner_param->out_byte3_source_stream	= 16;

        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 0);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 1);

        second_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER1);
        second_joiner->setSize(iosize, iosize);
        second_joiner_param = (struct vpul_pu_joiner*)second_joiner->getParameter();
        second_joiner_param->work_mode = 0;
        second_joiner_param->input0_enable = 1;
        second_joiner_param->input1_enable = 1;
        second_joiner_param->input2_enable = 0;
        second_joiner_param->input3_enable = 0;
        second_joiner_param->out_byte0_source_stream = 0;
        second_joiner_param->out_byte1_source_stream = 4;
        second_joiner_param->out_byte2_source_stream = 1;
        second_joiner_param->out_byte3_source_stream	= 5;

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, second_joiner, 0);
        ExynosVpuPu::connect(first_joiner, 0, second_joiner, 1);

        ret_out_pu_list->push_back(make_pair(second_joiner, 0));
        break;
    case VX_DF_IMAGE_UYVY:
        /* combine U and V */
        first_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        first_joiner->setSize(iosize, iosize);
        first_joiner_param = (struct vpul_pu_joiner*)first_joiner->getParameter();
        first_joiner_param->work_mode = 0;
        first_joiner_param->input0_enable = 1;
        first_joiner_param->input1_enable = 1;
        first_joiner_param->input2_enable = 0;
        first_joiner_param->input3_enable = 0;
        first_joiner_param->out_byte0_source_stream = 0;
        first_joiner_param->out_byte1_source_stream = 4;
        first_joiner_param->out_byte2_source_stream = 16;
        first_joiner_param->out_byte3_source_stream	= 16;

        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 0);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_joiner, 1);

        second_joiner = pu_factory.createPu(subchain, VPU_PU_JOINER1);
        second_joiner->setSize(iosize, iosize);
        second_joiner_param = (struct vpul_pu_joiner*)second_joiner->getParameter();
        second_joiner_param->work_mode = 0;
        second_joiner_param->input0_enable = 1;
        second_joiner_param->input1_enable = 1;
        second_joiner_param->input2_enable = 0;
        second_joiner_param->input3_enable = 0;
        second_joiner_param->out_byte0_source_stream = 4;
        second_joiner_param->out_byte1_source_stream = 0;
        second_joiner_param->out_byte2_source_stream = 5;
        second_joiner_param->out_byte3_source_stream = 1;

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, second_joiner, 0);
        ExynosVpuPu::connect(first_joiner, 0, second_joiner, 1);

        ret_out_pu_list->push_back(make_pair(second_joiner, 0));
        break;
    default:
        VXLOGE("un-supported dst format:0x%x", m_dst_format);
        status = VX_FAILURE;
        break;
    }

    return status;
}

vx_status
ExynosVpuKernelChannelComb::setPostChCombPu(ExynosVpuSubchainHw *subchain,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());

    ExynosVpuPu *dma_out1 = NULL;
    ExynosVpuPu *dma_out2 = NULL;
    ExynosVpuPu *dma_out3 = NULL;

    pair<ExynosVpuPu*, vx_uint32> in_pu;
    switch (m_dst_format) {
    case VX_DF_IMAGE_RGB:
        dma_out1 = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out1->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out1, 0);
        break;
    case VX_DF_IMAGE_RGBX:
    case VX_DF_IMAGE_YUYV:
    case VX_DF_IMAGE_UYVY:
        dma_out1 = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out1->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out1, 0);
        break;
    case VX_DF_IMAGE_YUV4:
    case VX_DF_IMAGE_IYUV:
        dma_out1 = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out1->setSize(iosize, iosize);
        dma_out2 = pu_factory.createPu(subchain, VPU_PU_DMAOT_MNM0);
        dma_out2->setSize(iosize, iosize);
        dma_out3 = pu_factory.createPu(subchain, VPU_PU_DMAOT_MNM1);
        dma_out3->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out1, 0);
        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out2, 0);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out3, 0);
        break;
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        dma_out1 = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out1->setSize(iosize, iosize);
        dma_out2 = pu_factory.createPu(subchain, VPU_PU_DMAOT_MNM0);
        dma_out2->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out1, 0);
        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out2, 0);
        break;
    default:
        VXLOGE("un-supported dst format:0x%x", m_dst_format);
        status = VX_FAILURE;
        break;
    }

    if (dma_out1) {
        ret_out_pu_list->push_back(make_pair(dma_out1, 0));
    }
    if (dma_out2) {
        ret_out_pu_list->push_back(make_pair(dma_out2, 0));
    }
    if (dma_out3) {
        ret_out_pu_list->push_back(make_pair(dma_out3, 0));
    }

    if (ret_out_pu_list->size()  != m_dst_io_num) {
        VXLOGE("the number of src io is not matching, %d, %d", ret_out_pu_list->size(), m_dst_io_num);
        status = VX_FAILURE;
    }

    return status;
}

}; /* namespace android */

