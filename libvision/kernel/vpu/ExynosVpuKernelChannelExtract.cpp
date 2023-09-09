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

#define LOG_TAG "ExynosVpuKernelChannelExtract"
#include <cutils/log.h>

#include "ExynosVpuKernelChannelExtract.h"

#include "vpu_kernel_util.h"

#include "td-binary-channelextract_rgb2r.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_rgb2r[] =
    TASK_test_binary_channelext_rbg2r_from_VDE_DS;

vx_status
ExynosVpuKernelChannelExtract::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_SUCCESS;

    vx_parameter param = vxGetParameterByIndex(node, index);
    if (index == 0) {
        vx_image image = 0;
        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
        if (image) {
            vx_df_image format = 0;
            vx_uint32 width, height;
            vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
            vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            // check to make sure the input format is supported.
            switch (format) {
            case VX_DF_IMAGE_RGB:
            case VX_DF_IMAGE_RGBX:
            case VX_DF_IMAGE_YUV4:
                status = VX_SUCCESS;
                break;
            /* 4:2:0 */
            case VX_DF_IMAGE_NV12:
            case VX_DF_IMAGE_NV21:
            case VX_DF_IMAGE_IYUV:
                if (width % 2 != 0 || height % 2 != 0)
                    status = VX_ERROR_INVALID_DIMENSION;
                else
                    status = VX_SUCCESS;
                break;
            /* 4:2:2 */
            case VX_DF_IMAGE_UYVY:
            case VX_DF_IMAGE_YUYV:
                if (width % 2 != 0)
                    status = VX_ERROR_INVALID_DIMENSION;
                else
                    status = VX_SUCCESS;
                break;
            default:
                status = VX_ERROR_INVALID_FORMAT;
                break;
            }
            vxReleaseImage(&image);
        }  else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
    } else if (index == 1) {
        vx_scalar scalar;
        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
        if (scalar) {
            vx_enum type = 0;
            vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
            if (type == VX_TYPE_ENUM) {
                vx_enum channel = 0;
                vx_parameter param0;

                vxReadScalarValue(scalar, &channel);
                param0 = vxGetParameterByIndex(node, 0);

                if (param0) {
                    vx_image image = 0;
                    vxQueryParameter(param0, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));

                    if (image) {
                        vx_df_image format = VX_DF_IMAGE_VIRT;
                        vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

                        status = VX_ERROR_INVALID_VALUE;
                        switch (format) {
                        case VX_DF_IMAGE_RGB:
                        case VX_DF_IMAGE_RGBX:
                            if ( (channel == VX_CHANNEL_R) ||
                                 (channel == VX_CHANNEL_G) ||
                                 (channel == VX_CHANNEL_B) ||
                                 (channel == VX_CHANNEL_A) ) {
                                status = VX_SUCCESS;
                            }
                            break;
                        case VX_DF_IMAGE_YUV4:
                        case VX_DF_IMAGE_NV12:
                        case VX_DF_IMAGE_NV21:
                        case VX_DF_IMAGE_IYUV:
                        case VX_DF_IMAGE_UYVY:
                        case VX_DF_IMAGE_YUYV:
                            if ( (channel == VX_CHANNEL_Y) ||
                                 (channel == VX_CHANNEL_U) ||
                                 (channel == VX_CHANNEL_V) ) {
                                status = VX_SUCCESS;
                            }
                            break;
                        default:
                            break;
                        }

                        vxReleaseImage(&image);
                    }

                    vxReleaseParameter(&param0);
                }
            } else {
                status = VX_ERROR_INVALID_TYPE;
            }
            vxReleaseScalar(&scalar);
        }
    } else {
        status = VX_ERROR_INVALID_PARAMETERS;
    }
    vxReleaseParameter(&param);

    return status;
}

vx_status
ExynosVpuKernelChannelExtract::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 2) {
        vx_parameter param0 = vxGetParameterByIndex(node, 0);
        vx_parameter param1 = vxGetParameterByIndex(node, 1);

        if ((param0) && (param1)) {
            vx_image input = 0;
            vx_scalar chan = 0;
            vx_enum channel = 0;
            vxQueryParameter(param0, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            vxQueryParameter(param1, VX_PARAMETER_ATTRIBUTE_REF, &chan, sizeof(chan));
            vxReadScalarValue(chan, &channel);

            if ((input) && (chan)) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = VX_DF_IMAGE_VIRT;

                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

                if (channel != VX_CHANNEL_Y) {
                    switch (format) {
                        case VX_DF_IMAGE_IYUV:
                        case VX_DF_IMAGE_NV12:
                        case VX_DF_IMAGE_NV21:
                            width /= 2;
                            height /= 2;
                            break;
                        case VX_DF_IMAGE_YUYV:
                        case VX_DF_IMAGE_UYVY:
                            width /= 2;
                            break;
                    }
                }

                vx_df_image meta_format = VX_DF_IMAGE_U8;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&input);
                vxReleaseScalar(&chan);
            }
            vxReleaseParameter(&param0);
            vxReleaseParameter(&param1);
        }
    } else {
        status = VX_ERROR_INVALID_PARAMETERS;
    }

    return status;
}

ExynosVpuKernelChannelExtract::ExynosVpuKernelChannelExtract(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_src_format = VX_DF_IMAGE_VIRT;

    m_channel = 0;

    m_src_mem_continuous = vx_false_e;
    m_src_plane = 0;

    strcpy(m_task_name, "chextract");
}

ExynosVpuKernelChannelExtract::~ExynosVpuKernelChannelExtract(void)
{

}

vx_status
ExynosVpuKernelChannelExtract::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    vx_image input = (vx_image)parameters[0];
    vx_scalar channel = (vx_scalar)parameters[1];

    status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &m_src_format, sizeof(m_src_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image format fails, err:%d", status);
        return status;
    }

    status  = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        return status;
    }

    status = vxReadScalarValue(channel, &m_channel);
    if (status != VX_SUCCESS) {
        VXLOGE("querying scalar fails, err:%d", status);
        return status;
    }

    vx_uint32 src_mem_num;
    status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_MEMORIES, &src_mem_num, sizeof(src_mem_num));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image memories fails, err:%d", status);
        return status;
    }

    vx_uint32 src_plane_num;
    status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_PLANES, &src_plane_num, sizeof(src_plane_num));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image plane fails, err:%d", status);
        return status;
    }

    switch (m_src_format) {
    case VX_DF_IMAGE_RGB:
    case VX_DF_IMAGE_RGBX:
    case VX_DF_IMAGE_UYVY:
    case VX_DF_IMAGE_YUYV:
        m_src_plane = 0;
        break;
    case VX_DF_IMAGE_YUV4:
    case VX_DF_IMAGE_IYUV:
        if (m_channel == VX_CHANNEL_Y) {
            m_src_plane = 0;
        } else if (m_channel == VX_CHANNEL_U) {
            m_src_plane = 1;
        } else if (m_channel == VX_CHANNEL_V) {
            m_src_plane = 2;
        }
        break;
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        if (m_channel == VX_CHANNEL_Y) {
            m_src_plane = 0;
        } else if ((m_channel == VX_CHANNEL_U) || (m_channel == VX_CHANNEL_V)) {
            m_src_plane = 1;
        }
        break;
    default:
        VXLOGE("un-supported source format: 0x%x", m_src_format);
        status = VX_FAILURE;
        break;
    }

    return status;
}

vx_status
ExynosVpuKernelChannelExtract::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelChannelExtract::initTaskFromBinary(void)
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
ExynosVpuKernelChannelExtract::initTask0FromBinary(void)
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
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_rgb2r);
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
ExynosVpuKernelChannelExtract::initTaskFromApi(void)
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
ExynosVpuKernelChannelExtract::initTask0FromApi(void)
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
    ExynosVpuProcess *chextract_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, chextract_process);
    ExynosVpuVertex::connect(chextract_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(chextract_process);

    /* define subchain */
    ExynosVpuSubchainHw *chextract_subchain = new ExynosVpuSubchainHw(chextract_process);

    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_in_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> main_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> post_out_pu_list;

    pair<ExynosVpuPu*, vx_uint32> pu;

    status = setPreChExtractPu(chextract_subchain, iosize, &pre_in_pu_list, &pre_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setMainChExtractPu(chextract_subchain, iosize, &pre_out_pu_list, &main_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setPostChExtractPu(chextract_subchain, iosize, &main_out_pu_list, &post_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }

    if ((pre_in_pu_list.size() != 1) || (post_out_pu_list.size() != 1)) {
        VXLOGE("pu list configuration is invalid");
        goto EXIT;
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *pu_memmap;
    ExynosVpuIoFixedMapRoi *pu_fixed_roi;
    ExynosVpuIoTypesDesc *pu_iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    pu_fixed_roi = new ExynosVpuIoFixedMapRoi(chextract_process, pu_memmap);
    pu_iotyps = new ExynosVpuIoTypesDesc(chextract_process, pu_fixed_roi);

    pu = pre_in_pu_list.editItemAt(0);
    pu.first->setIoTypesDesc(pu_iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, pu.first);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    pu_fixed_roi = new ExynosVpuIoFixedMapRoi(chextract_process, pu_memmap);
    pu_iotyps = new ExynosVpuIoTypesDesc(chextract_process, pu_fixed_roi);

    pu = post_out_pu_list.editItemAt(0);
    pu.first->setIoTypesDesc(pu_iotyps);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, pu.first);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelChannelExtract::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelChannelExtract::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    switch(m_src_format) {
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
        if (m_channel == VX_CHANNEL_Y) {
            param_info.image.plane=0;
        } else {
            param_info.image.plane=1;
        }
        break;
    case VX_DF_IMAGE_YUV4:
    case VX_DF_IMAGE_IYUV:
        if (m_channel == VX_CHANNEL_Y) {
            param_info.image.plane=0;
        } else if (m_channel == VX_CHANNEL_U) {
            param_info.image.plane=1;
        } else if (m_channel == VX_CHANNEL_V) {
            param_info.image.plane=2;
        }
        break;
    default:
        param_info.image.plane=0;
        break;
    }
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
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
ExynosVpuKernelChannelExtract::setPreChExtractPu(ExynosVpuSubchainHw *subchain,
                                                                    ExynosVpuIoSizeInout *std_size,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_in_pu_list,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    ExynosVpuPuFactory pu_factory;

    ExynosVpuPu *dma_in = pu_factory.createPu(subchain, VPU_PU_DMAIN_WIDE0);
    dma_in->setSize(std_size, std_size);

    ret_in_pu_list->push_back(make_pair(dma_in, 0));
    ret_out_pu_list->push_back(make_pair(dma_in, 0));

    return VX_SUCCESS;
}

vx_status
ExynosVpuKernelChannelExtract::setMainChExtractPu(ExynosVpuSubchainHw *subchain,
                                                    ExynosVpuIoSizeInout *std_size,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuPu *first_splitter = NULL;
    ExynosVpuPu *second_splitter = NULL;
    struct vpul_pu_spliter *first_splitter_param = NULL;
    struct vpul_pu_spliter *second_splitter_param = NULL;

    switch (m_src_format) {
    case VX_DF_IMAGE_RGB:
    case VX_DF_IMAGE_RGBX:
        first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        first_splitter->setSize(std_size, std_size);
        first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
        if (m_channel == VX_CHANNEL_R) {
            first_splitter_param->out0_byte0 = 0;
            first_splitter_param->out0_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_G) {
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_B) {
            first_splitter_param->out0_byte0 = 2;
            first_splitter_param->out0_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_A) {
            first_splitter_param->out0_byte0 = 3;
            first_splitter_param->out0_byte1 = 4;
        }

        first_splitter_param->out1_byte0 = 4;
        first_splitter_param->out1_byte1 = 4;
        first_splitter_param->out2_byte0 = 4;
        first_splitter_param->out2_byte1 = 4;
        first_splitter_param->out3_byte0 = 4;
        first_splitter_param->out3_byte1 = 4;
        break;
    case VX_DF_IMAGE_NV12:
        if (m_channel == VX_CHANNEL_Y) {
            /* do nothing */
        } else if (m_channel == VX_CHANNEL_U) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 0;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_V) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;
        }
        break;
    case VX_DF_IMAGE_NV21:
        if (m_channel == VX_CHANNEL_Y) {
            /* do nothing */
        } else if (m_channel == VX_CHANNEL_U) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_V) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 0;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;
        }
        break;
    case VX_DF_IMAGE_YUYV:
        if (m_channel == VX_CHANNEL_Y) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 0;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_U) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 3;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;

            second_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
            second_splitter->setSize(std_size, std_size);
            second_splitter_param = (struct vpul_pu_spliter*)second_splitter->getParameter();
            second_splitter_param->out0_byte0 = 0;
            second_splitter_param->out0_byte1 = 4;
            second_splitter_param->out1_byte0 = 4;
            second_splitter_param->out1_byte1 = 4;
            second_splitter_param->out2_byte0 = 4;
            second_splitter_param->out2_byte1 = 4;
            second_splitter_param->out3_byte0 = 4;
            second_splitter_param->out3_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_V) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 3;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;

            second_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
            second_splitter->setSize(std_size, std_size);
            second_splitter_param = (struct vpul_pu_spliter*)second_splitter->getParameter();
            second_splitter_param->out0_byte0 = 1;
            second_splitter_param->out0_byte1 = 4;
            second_splitter_param->out1_byte0 = 4;
            second_splitter_param->out1_byte1 = 4;
            second_splitter_param->out2_byte0 = 4;
            second_splitter_param->out2_byte1 = 4;
            second_splitter_param->out3_byte0 = 4;
            second_splitter_param->out3_byte1 = 4;
        }
        break;
    case VX_DF_IMAGE_UYVY:
        if (m_channel == VX_CHANNEL_Y) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 4;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;

        } else if (m_channel == VX_CHANNEL_U) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 0;
            first_splitter_param->out0_byte1 = 2;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;

            second_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
            second_splitter->setSize(std_size, std_size);
            second_splitter_param = (struct vpul_pu_spliter*)second_splitter->getParameter();
            second_splitter_param->out0_byte0 = 0;
            second_splitter_param->out0_byte1 = 4;
            second_splitter_param->out1_byte0 = 4;
            second_splitter_param->out1_byte1 = 4;
            second_splitter_param->out2_byte0 = 4;
            second_splitter_param->out2_byte1 = 4;
            second_splitter_param->out3_byte0 = 4;
            second_splitter_param->out3_byte1 = 4;
        } else if (m_channel == VX_CHANNEL_V) {
            first_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
            first_splitter->setSize(std_size, std_size);
            first_splitter_param = (struct vpul_pu_spliter*)first_splitter->getParameter();
            first_splitter_param->out0_byte0 = 1;
            first_splitter_param->out0_byte1 = 3;
            first_splitter_param->out1_byte0 = 4;
            first_splitter_param->out1_byte1 = 4;
            first_splitter_param->out2_byte0 = 4;
            first_splitter_param->out2_byte1 = 4;
            first_splitter_param->out3_byte0 = 4;
            first_splitter_param->out3_byte1 = 4;

            second_splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
            second_splitter->setSize(std_size, std_size);
            second_splitter_param = (struct vpul_pu_spliter*)second_splitter->getParameter();
            second_splitter_param->out0_byte0 = 1;
            second_splitter_param->out0_byte1 = 4;
            second_splitter_param->out1_byte0 = 4;
            second_splitter_param->out1_byte1 = 4;
            second_splitter_param->out2_byte0 = 4;
            second_splitter_param->out2_byte1 = 4;
            second_splitter_param->out3_byte0 = 4;
            second_splitter_param->out3_byte1 = 4;
        }
        break;
    default:
        /* do nothing */
        break;
    }

    pair<ExynosVpuPu*, vx_uint32> in_pu;
    in_pu = in_pu_list->editItemAt(0);

    if (second_splitter) {
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_splitter, 0);
        ExynosVpuPu::connect(first_splitter, 0, second_splitter, 0);
        ret_out_pu_list->push_back(make_pair(second_splitter, 0));
    } else if (first_splitter) {
        ExynosVpuPu::connect(in_pu.first, in_pu.second, first_splitter, 0);
        ret_out_pu_list->push_back(make_pair(first_splitter, 0));
    } else {
        /* do nothing if the splitter is not necessary */
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));
    }

    return status;
}

vx_status
ExynosVpuKernelChannelExtract::setPostChExtractPu(ExynosVpuSubchainHw *subchain,
                                                                            ExynosVpuIoSizeInout *std_size,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    ExynosVpuPuFactory pu_factory;

    pair<ExynosVpuPu*, vx_uint32> in_pu;

    in_pu = in_pu_list->editItemAt(0);

    ExynosVpuPu *dma_out = pu_factory.createPu(subchain, VPU_PU_DMAOT0);
    dma_out->setSize(std_size, std_size);

    ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out, 0);
    ret_out_pu_list->push_back(make_pair(dma_out, 0));

    return VX_SUCCESS;
}

}; /* namespace android */

