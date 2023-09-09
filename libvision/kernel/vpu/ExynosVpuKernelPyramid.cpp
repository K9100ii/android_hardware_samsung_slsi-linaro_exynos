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

#define LOG_TAG "ExynosVpuKernelPyramid"
#include <cutils/log.h>

#include <utility>

#include "ExynosVpuKernelPyramid.h"

#include "vpu_kernel_util.h"

#include "td-binary-pyramid_half4.h"
#include "td-binary-pyramid_orb3.h"

#define TASK_PYRAMID_HALF_CAPA      4

#define TASK_PYRAMID_ORB_1ST_CAPA      3
#define TASK_PYRAMID_ORB_NTH_CAPA      2

namespace android {

using namespace std;

static vx_uint16 td_binary_half4[] =
    TASK_test_binary_gaussianpyramid_half4_from_VDE_DS;

static vx_uint16 td_binary_orb3[] =
    TASK_test_binary_gaussianpyramid_orb3_from_VDE_DS;

vx_status
ExynosVpuKernelPyramid::inputValidator(vx_node node, vx_uint32 index)
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
    }

    return status;
}

vx_status
ExynosVpuKernelPyramid::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_image src = 0;
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        vx_parameter dst_param = vxGetParameterByIndex(node, index);

        vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
        if (src) {
            vx_pyramid dst = 0;
            vxQueryParameter(dst_param, VX_PARAMETER_ATTRIBUTE_REF, &dst, sizeof(dst));

            if (dst) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format;
                vx_size num_levels;
                vx_float32 scale;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxQueryPyramid(dst, VX_PYRAMID_ATTRIBUTE_LEVELS, &num_levels, sizeof(num_levels));
                vxQueryPyramid(dst, VX_PYRAMID_ATTRIBUTE_SCALE, &scale, sizeof(scale));

                /* fill in the meta data with the attributes so that the checker will pass */
                vx_uint32 pyramid_width = width;
                vx_uint32 pyramid_height = height;
                vx_df_image pyramid_format = format;
                vx_size pyramid_levels = num_levels;
                vx_float32 pyramid_scale = scale;
                vxSetMetaFormatAttribute(meta, VX_PYRAMID_ATTRIBUTE_WIDTH, &pyramid_width, sizeof(pyramid_width));
                vxSetMetaFormatAttribute(meta, VX_PYRAMID_ATTRIBUTE_HEIGHT, &pyramid_height, sizeof(pyramid_height));
                vxSetMetaFormatAttribute(meta, VX_PYRAMID_ATTRIBUTE_FORMAT, &pyramid_format, sizeof(pyramid_format));
                vxSetMetaFormatAttribute(meta, VX_PYRAMID_ATTRIBUTE_LEVELS, &pyramid_levels, sizeof(pyramid_levels));
                vxSetMetaFormatAttribute(meta, VX_PYRAMID_ATTRIBUTE_SCALE, &pyramid_scale, sizeof(pyramid_scale));

                if ((pyramid_scale==VX_SCALE_PYRAMID_HALF) || (pyramid_scale==VX_SCALE_PYRAMID_ORB))
                    status = VX_SUCCESS;
                else
                    status = VX_ERROR_INVALID_REFERENCE;
                vxReleasePyramid(&dst);
            }
            vxReleaseImage(&src);
        }
        vxReleaseParameter(&dst_param);
        vxReleaseParameter(&src_param);
    }

    return status;
}

ExynosVpuKernelPyramid::ExynosVpuKernelPyramid(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_out_pyramid_level = 0;
    m_pyramid_scale = 0;
    strcpy(m_task_name, "gaussianpyramid");
}

ExynosVpuKernelPyramid::~ExynosVpuKernelPyramid(void)
{

}

vx_status
ExynosVpuKernelPyramid::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_pyramid pyramid = (vx_pyramid)parameters[1];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        goto EXIT;
    }

    status = vxQueryPyramid(pyramid, VX_PYRAMID_ATTRIBUTE_LEVELS, &m_out_pyramid_level, sizeof(m_out_pyramid_level));
    if (status != VX_SUCCESS) {
        VXLOGE("querying pyramid fails, err:%d", status);
        goto EXIT;
    }

    status = vxQueryPyramid(pyramid, VX_PYRAMID_ATTRIBUTE_SCALE, &m_pyramid_scale, sizeof(m_pyramid_scale));
    if (status != VX_SUCCESS) {
        VXLOGE("querying pyramid fails, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelPyramid::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelPyramid::initTaskFromBinary(void)
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
ExynosVpuKernelPyramid::initTask0FromBinary(void)
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
    if (m_pyramid_scale == VX_SCALE_PYRAMID_HALF) {
        if (m_out_pyramid_level == 4) {
            ret = task_if->importTaskStr((struct vpul_task*)td_binary_half4);
        } else {
            VXLOGE("half pyramid binary only support level 4");
            ret = INVALID_OPERATION;
        }
    } else if (m_pyramid_scale == VX_SCALE_PYRAMID_ORB) {
        if (m_out_pyramid_level == 3) {
            ret = task_if->importTaskStr((struct vpul_task*)td_binary_orb3);
        } else {
            VXLOGE("orb pyramid binary only support level 3");
            ret = INVALID_OPERATION;
        }
    } else {
        ret = INVALID_OPERATION;
    }
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect pu to io */
    if (m_pyramid_scale == VX_SCALE_PYRAMID_HALF) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)2);
        task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)5);
        task_if->setIoPu(VS4L_DIRECTION_OT, 2, (uint32_t)8);
        task_if->setIoPu(VS4L_DIRECTION_OT, 3, (uint32_t)10);
        status = VX_SUCCESS;
    } else if (m_pyramid_scale == VX_SCALE_PYRAMID_ORB) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)2);
        task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)5);
        task_if->setIoPu(VS4L_DIRECTION_OT, 2, (uint32_t)9);
        status = VX_SUCCESS;
    } else {
        status = VX_FAILURE;
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelPyramid::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIf *prev_task_if = NULL;
    ExynosVpuTaskIf *cur_task_if = NULL;

    if (m_pyramid_scale==VX_SCALE_PYRAMID_HALF) {
        vx_uint32 task_index = 0;
        vx_int32 remaind_pyramid_level = m_out_pyramid_level;

        vx_uint32 cur_pyramid_level = (remaind_pyramid_level>TASK_PYRAMID_HALF_CAPA) ? TASK_PYRAMID_HALF_CAPA : remaind_pyramid_level;
        cur_task_if = initTask0FromApiForHalfScale(task_index++, cur_pyramid_level);
        if (cur_task_if == NULL) {
            VXLOGE("creating task fails");
            return VX_FAILURE;
        }
        prev_task_if = cur_task_if;
        remaind_pyramid_level -= cur_pyramid_level;

        while(remaind_pyramid_level > 0) {
            vx_uint32 cur_pyramid_level = (remaind_pyramid_level>TASK_PYRAMID_HALF_CAPA) ? TASK_PYRAMID_HALF_CAPA : remaind_pyramid_level;
            cur_task_if = initTasknFromApiForHalfScale(task_index++, cur_pyramid_level);
            if (cur_task_if == NULL) {
                VXLOGE("creating task fails");
                return VX_FAILURE;
            }

            ExynosVpuTaskIf::connect(prev_task_if, 0, cur_task_if, 0);
            prev_task_if = cur_task_if;

            remaind_pyramid_level -= cur_pyramid_level;
        }
    } else if (m_pyramid_scale == VX_SCALE_PYRAMID_ORB) {
        vx_uint32 task_index = 0;
        vx_int32 remaind_pyramid_level = m_out_pyramid_level;

        vx_uint32 cur_pyramid_level = (remaind_pyramid_level>TASK_PYRAMID_ORB_1ST_CAPA) ? TASK_PYRAMID_ORB_1ST_CAPA : remaind_pyramid_level;
        cur_task_if = initTask0FromApiForORBScale(task_index++, cur_pyramid_level);
        if (cur_task_if == NULL) {
            VXLOGE("creating task fails");
            return VX_FAILURE;
        }
        prev_task_if = cur_task_if;
        remaind_pyramid_level -= cur_pyramid_level;

        while(remaind_pyramid_level > 0) {
            vx_uint32 cur_pyramid_level = (remaind_pyramid_level>TASK_PYRAMID_ORB_NTH_CAPA) ? TASK_PYRAMID_ORB_NTH_CAPA : remaind_pyramid_level;
            cur_task_if = initTasknFromApiForORBScale(task_index++, cur_pyramid_level);
            if (cur_task_if == NULL) {
                VXLOGE("creating task fails");
                return VX_FAILURE;
            }

            ExynosVpuTaskIf::connect(prev_task_if, 0, cur_task_if, 0);
            prev_task_if = cur_task_if;

            remaind_pyramid_level -= cur_pyramid_level;
        }
    } else {
        VXLOGE("not implemented yet");
        status = VX_FAILURE;
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
ExynosVpuKernelPyramid::initTask0FromApiForHalfScale(vx_uint32 task_index, vx_uint32 pyramid_level)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *pyramid_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, pyramid_process);
    ExynosVpuVertex::connect(pyramid_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(pyramid_process);

    ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(pyramid_process);
    ExynosVpuIoSizeScale *iosize_half = new ExynosVpuIoSizeScale(pyramid_process, iosize, sizeop_sacle);

    struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
    scales_info->horizontal.numerator = 1;
    scales_info->horizontal.denominator = 2;
    scales_info->vertical.numerator = 1;
    scales_info->vertical.denominator = 2;

    ExynosVpuSubchainHw *pyramid_subchain = new ExynosVpuSubchainHw(pyramid_process);

    ExynosVpuPu *io_input_dma = NULL;
    Vector<ExynosVpuPu*> io_output_dma_list;

    pair<ExynosVpuPu*, vx_uint32> previous_pu;

    vx_uint32 cur_pyramid_level = 0;

    /* pyramid level 0 just copy the original image */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *dma_in;
        ExynosVpuPu *duplicator_first;
        ExynosVpuPu *dma_out;

        dma_in = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);

        duplicator_first = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR0);
        duplicator_first->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_WIDE0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, duplicator_first, 0);
        ExynosVpuPu::connect(duplicator_first, 0, dma_out, 0);

        io_input_dma = dma_in;
        io_output_dma_list.push_back(dma_out);
        previous_pu = make_pair(duplicator_first, 1);
    }

    /* pyramid level 1  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF50);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR1);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 2  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF51);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR2);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 3  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF52);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM1);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
    io_input_dma->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_input_dma);
    for (vx_uint32 i=0; i<io_output_dma_list.size(); i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
        io_output_dma_list.editItemAt(i)->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, i, io_output_dma_list.editItemAt(i));
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelPyramid::initTasknFromApiForHalfScale(vx_uint32 task_index, vx_uint32 pyramid_level)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *pyramid_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, pyramid_process);
    ExynosVpuVertex::connect(pyramid_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(pyramid_process);

    ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(pyramid_process);
    ExynosVpuIoSizeScale *iosize_half = new ExynosVpuIoSizeScale(pyramid_process, iosize, sizeop_sacle);

    struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
    scales_info->horizontal.numerator = 1;
    scales_info->horizontal.denominator = 2;
    scales_info->vertical.numerator = 1;
    scales_info->vertical.denominator = 2;

    ExynosVpuSubchainHw *pyramid_subchain = new ExynosVpuSubchainHw(pyramid_process);

    ExynosVpuPu *io_input_dma = NULL;
    Vector<ExynosVpuPu*> io_output_dma_list;

    pair<ExynosVpuPu*, vx_uint32> previous_pu;

    vx_uint32 cur_pyramid_level = 0;

    /* pyramid level 0  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *dma_in;
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        dma_in = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF70);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR_WIDE0);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_WIDE0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);


        io_input_dma = dma_in;
        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 1  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF50);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR1);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 2  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF51);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR2);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 3  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF52);
        slf5x5->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->downsample_rows = 1;
        slf_param->downsample_cols = 1;
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM1);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
    io_input_dma->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_input_dma);
    for (vx_uint32 i=0; i<io_output_dma_list.size(); i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
        io_output_dma_list.editItemAt(i)->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, i, io_output_dma_list.editItemAt(i));
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelPyramid::initTask0FromApiForORBScale(vx_uint32 task_index, vx_uint32 pyramid_level)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *pyramid_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, pyramid_process);
    ExynosVpuVertex::connect(pyramid_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(pyramid_process);

    ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(pyramid_process);
    ExynosVpuIoSizeScale *iosize_orb = new ExynosVpuIoSizeScale(pyramid_process, iosize, sizeop_sacle);

    struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
    scales_info->horizontal.numerator = 3611622536 ;
    scales_info->horizontal.denominator = 4294967295;
    scales_info->vertical.numerator = 3611622536 ;
    scales_info->vertical.denominator = 4294967295;

    ExynosVpuSubchainHw *pyramid_subchain = new ExynosVpuSubchainHw(pyramid_process);

    ExynosVpuPu *io_input_dma = NULL;
    Vector<ExynosVpuPu*> io_output_dma_list;

    pair<ExynosVpuPu*, vx_uint32> previous_pu;

    vx_uint32 cur_pyramid_level = 0;

    /* pyramid level 0 just copy the original image */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *dma_in;
        ExynosVpuPu *duplicator_first;
        ExynosVpuPu *dma_out;

        dma_in = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);

        duplicator_first = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR0);
        duplicator_first->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_WIDE0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, duplicator_first, 0);
        ExynosVpuPu::connect(duplicator_first, 0, dma_out, 0);

        io_input_dma = dma_in;
        io_output_dma_list.push_back(dma_out);
        previous_pu = make_pair(duplicator_first, 1);
    }

    /* pyramid level 1  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *dnscaler;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF50);
        slf5x5->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dnscaler = pu_factory.createPu(pyramid_subchain, VPU_PU_DNSCALER0);
        dnscaler->setSize(iosize, iosize_orb);

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR1);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dnscaler, 0);
        ExynosVpuPu::connect(dnscaler, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 2  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *dnscaler;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF51);
        slf5x5->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dnscaler = pu_factory.createPu(pyramid_subchain, VPU_PU_DNSCALER1);
        dnscaler->setSize(iosize, iosize_orb);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dnscaler, 0);
        ExynosVpuPu::connect(dnscaler, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
    io_input_dma->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_input_dma);
    for (vx_uint32 i=0; i<io_output_dma_list.size(); i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
        io_output_dma_list.editItemAt(i)->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, i, io_output_dma_list.editItemAt(i));
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelPyramid::initTasknFromApiForORBScale(vx_uint32 task_index, vx_uint32 pyramid_level)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *pyramid_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, pyramid_process);
    ExynosVpuVertex::connect(pyramid_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(pyramid_process);

    ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(pyramid_process);
    ExynosVpuIoSizeScale *iosize_orb = new ExynosVpuIoSizeScale(pyramid_process, iosize, sizeop_sacle);

    struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
    scales_info->horizontal.numerator = 3611622536 ;
    scales_info->horizontal.denominator = 4294967295;
    scales_info->vertical.numerator = 3611622536 ;
    scales_info->vertical.denominator = 4294967295;

    ExynosVpuSubchainHw *pyramid_subchain = new ExynosVpuSubchainHw(pyramid_process);

    ExynosVpuPu *io_input_dma = NULL;
    Vector<ExynosVpuPu*> io_output_dma_list;

    pair<ExynosVpuPu*, vx_uint32> previous_pu;

    vx_uint32 cur_pyramid_level = 0;

    /* pyramid level 0  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *dma_in;
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *dnscaler;
        ExynosVpuPu *duplicator;
        ExynosVpuPu *dma_out;

        dma_in = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF50);
        slf5x5->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dnscaler = pu_factory.createPu(pyramid_subchain, VPU_PU_DNSCALER0);
        dnscaler->setSize(iosize, iosize_orb);

        duplicator = pu_factory.createPu(pyramid_subchain, VPU_PU_DUPLICATOR1);
        duplicator->setSize(iosize, iosize);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in, 0, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dnscaler, 0);
        ExynosVpuPu::connect(dnscaler, 0, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, dma_out, 0);

        io_input_dma = dma_in;
        io_output_dma_list.push_back(dma_out);

        previous_pu = make_pair(duplicator, 1);
    }

    /* pyramid level 1  */
    if (cur_pyramid_level++ < pyramid_level) {
        ExynosVpuPu *slf5x5;
        ExynosVpuPu *dnscaler;
        ExynosVpuPu *dma_out;

        slf5x5 = pu_factory.createPu(pyramid_subchain, VPU_PU_SLF51);
        slf5x5->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param = (struct vpul_pu_slf*)slf5x5->getParameter();
        slf_param->filter_size_mode = 2;
        slf_param->border_mode_up = 1;
        slf_param->border_mode_down = 1;
        slf_param->border_mode_left = 1;
        slf_param->border_mode_right = 1;
        slf_param->coefficient_fraction = 4;
        slf_param->maxp_sizes_filt_hor = 7;
        slf_param->maxp_sizes_filt_ver = 7;
        int32_t slf_coff[] = {1, 4, 6, 4, 1, 1, 4, 6, 4, 1};
        ((ExynosVpuPuSlf*)slf5x5)->setStaticCoffValue(slf_coff, 5, sizeof(slf_coff)/sizeof(slf_coff[0]));

        dnscaler = pu_factory.createPu(pyramid_subchain, VPU_PU_DNSCALER1);
        dnscaler->setSize(iosize, iosize_orb);

        dma_out = pu_factory.createPu(pyramid_subchain, VPU_PU_DMAOT_MNM0);
        dma_out->setSize(iosize, iosize);

        ExynosVpuPu::connect(previous_pu.first, previous_pu.second, slf5x5, 0);
        ExynosVpuPu::connect(slf5x5, 0, dnscaler, 0);
        ExynosVpuPu::connect(dnscaler, 0, dma_out, 0);

        io_output_dma_list.push_back(dma_out);
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
    io_input_dma->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, io_input_dma);

    for (vx_uint32 i=0; i<io_output_dma_list.size(); i++) {
        io_external_mem = new ExynosVpuIoExternalMem(task);
        memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        fixed_roi = new ExynosVpuIoFixedMapRoi(pyramid_process, memmap);
        iotyps = new ExynosVpuIoTypesDesc(pyramid_process, fixed_roi);
        io_output_dma_list.editItemAt(i)->setIoTypesDesc(iotyps);
        task_if->setIoPu(VS4L_DIRECTION_OT, i, io_output_dma_list.editItemAt(i));
    }

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelPyramid::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */

EXIT:
    return status;
}

vx_status
ExynosVpuKernelPyramid::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    Vector<ExynosVpuTaskIfWrapper*>::iterator taskwr_iter;
    vx_uint32 base_pyramid_level;

    if (m_pyramid_scale==VX_SCALE_PYRAMID_HALF) {
        for (taskwr_iter=m_task_wr_list.begin(), base_pyramid_level=0; taskwr_iter!=m_task_wr_list.end(); taskwr_iter++, base_pyramid_level+=TASK_PYRAMID_HALF_CAPA) {
            /* in_io param */
            if (base_pyramid_level == 0) {
                param_info.image.plane = 0;
                status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
                if (status != VX_SUCCESS) {
                    VXLOGE("assigning param fails, %p", parameters);
                    goto EXIT;
                }
            } else {
                param_info.pyramid.level = base_pyramid_level-1;
                status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_IN, 0, 1, param_info);
                if (status != VX_SUCCESS) {
                    VXLOGE("assigning param fails, %p", parameters);
                    goto EXIT;
                }
            }

            /* out_io param */
            for (vx_uint32 i=0; i<(*taskwr_iter)->getTaskIf()->getIoNum(VS4L_DIRECTION_OT); i++) {
                param_info.pyramid.level = base_pyramid_level+i;
                status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_OT, i, 1, param_info);
                if (status != VX_SUCCESS) {
                    VXLOGE("assigning param fails, %p", parameters);
                    goto EXIT;
                }
            }
        }
    } else if (m_pyramid_scale==VX_SCALE_PYRAMID_ORB) {
        taskwr_iter=m_task_wr_list.begin();
        base_pyramid_level = 0;
        param_info.image.plane = 0;
        status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }

        /* out_io param */
        for (vx_uint32 i=0; i<(*taskwr_iter)->getTaskIf()->getIoNum(VS4L_DIRECTION_OT); i++) {
            param_info.pyramid.level = base_pyramid_level+i;
            status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_OT, i, 1, param_info);
            if (status != VX_SUCCESS) {
                VXLOGE("assigning param fails, %p", parameters);
                goto EXIT;
            }
        }

        taskwr_iter++;
        base_pyramid_level += TASK_PYRAMID_ORB_1ST_CAPA;
        for (; taskwr_iter!=m_task_wr_list.end(); taskwr_iter++, base_pyramid_level+=TASK_PYRAMID_ORB_NTH_CAPA) {
                param_info.pyramid.level = base_pyramid_level-1;
                status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_IN, 0, 1, param_info);
                if (status != VX_SUCCESS) {
                    VXLOGE("assigning param fails, %p", parameters);
                    goto EXIT;
                }

            /* out_io param */
            for (vx_uint32 i=0; i<(*taskwr_iter)->getTaskIf()->getIoNum(VS4L_DIRECTION_OT); i++) {
                param_info.pyramid.level = base_pyramid_level+i;
                VXLOGE("base_pyramid_level: %d, i:%d", base_pyramid_level, i);
                status = (*taskwr_iter)->setIoVxParam(VS4L_DIRECTION_OT, i, 1, param_info);
                if (status != VX_SUCCESS) {
                    VXLOGE("assigning param fails, %p", parameters);
                    goto EXIT;
                }
            }
        }
    } else {
        VXLOGE("un-supported scale");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

}; /* namespace android */

