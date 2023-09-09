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

#define LOG_TAG "ExynosVpuKernelCannyEdge"
#include <cutils/log.h>

#include "ExynosVpuKernelCannyEdge.h"

#include "vpu_kernel_util.h"

#include "td-binary-canny.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_canny_from_VDE_DS;

#define TASK_INIT_BINARY    1
#define TASK_INIT_API   2

#define TASK_INIT_MODE  TASK_INIT_BINARY

vx_status
ExynosVpuKernelCannyEdge::inputValidator(vx_node node, vx_uint32 index)
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
            } else {
                VXLOGE("format is not matching, index: %d, format: 0x%x", index, format);
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_threshold hyst = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &hyst, sizeof(hyst));
        if (hyst) {
            vx_enum type = 0;
            vxQueryThreshold(hyst, VX_THRESHOLD_ATTRIBUTE_TYPE, &type, sizeof(type));
            if (type == VX_THRESHOLD_TYPE_RANGE) {
                status = VX_SUCCESS;
            }
            vxReleaseThreshold(&hyst);
        }
        vxReleaseParameter(&param);
    } else if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar value = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &value, sizeof(value));
            if (value) {
                vx_enum stype = 0;
                vxQueryScalar(value, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_INT32) {
                    vx_int32 gs = 0;
                    vxReadScalarValue(value, &gs);
                    if ((gs == 3) || (gs == 5) || (gs == 7)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&value);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar value = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &value, sizeof(value));
            if (value) {
                vx_enum norm = 0;
                vxReadScalarValue(value, &norm);
                if ((norm == VX_NORM_L1) ||
                    (norm == VX_NORM_L2)) {
                    status = VX_SUCCESS;
                }
                vxReleaseScalar(&value);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

vx_status
ExynosVpuKernelCannyEdge::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 4) {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        if (src_param) {
            vx_image src = NULL;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            if (src) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = VX_DF_IMAGE_VIRT;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

                /* fill in the meta data with the attributes so that the checker will pass */
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&src);
            }
            vxReleaseParameter(&src_param);
        }
    }

    return status;
}

ExynosVpuKernelCannyEdge::ExynosVpuKernelCannyEdge(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "canny");
}

ExynosVpuKernelCannyEdge::~ExynosVpuKernelCannyEdge(void)
{

}

vx_status
ExynosVpuKernelCannyEdge::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);

    return status;
}

vx_status
ExynosVpuKernelCannyEdge::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

#if (TASK_INIT_MODE==TASK_INIT_BINARY)
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
ExynosVpuKernelCannyEdge::initTaskFromBinary(void)
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
ExynosVpuKernelCannyEdge::initTask0FromBinary(void)
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

    /* connect pu for intermediate between subchain */
    task_if->setInterPair(7, 14);
    task_if->setInterPair(17, 18);
    task_if->setInterPair(21, 22);

    /* connect pu to io */
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)28);
    if (ret != NO_ERROR) {
        VXLOGE("setting io pu fails");
        task_if->displayObjectInfo();
        status = VX_FAILURE;
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelCannyEdge::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;
    ExynosVpuTaskIf *task_if_1;

    task_if_0 = initTask0FromApi();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }
    task_if_1 = initTask1FromApi();
    if (task_if_1 == NULL) {
        VXLOGE("task1 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect between tasks */
    ExynosVpuTaskIf::connect(task_if_0, 0, task_if_1, 0);

    status = createStrFromObjectOfTask();
    if (status != VX_SUCCESS) {
        VXLOGE("creating task str fails");
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelCannyEdge::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

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
    ExynosVpuProcess *canny_first_half_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, canny_first_half_process);
    ExynosVpuVertex::connect(canny_first_half_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(canny_first_half_process);

    /* define 1st subchain */
    ExynosVpuSubchainHw *canny_1st_subchain = new ExynosVpuSubchainHw(canny_first_half_process);

    /* define pus of 1st subchain */
    ExynosVpuPu *dma_in = pu_factory.createPu(canny_1st_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *glf = pu_factory.createPu(canny_1st_subchain, VPU_PU_GLF50);
    glf->setSize(iosize, iosize);
    ExynosVpuPu *mde = pu_factory.createPu(canny_1st_subchain, VPU_PU_MDE);
    mde->setSize(iosize, iosize);
    ExynosVpuPu *calb_0 = pu_factory.createPu(canny_1st_subchain, VPU_PU_CALB0);
    calb_0->setSize(iosize, iosize);
    ExynosVpuPu *calb_1 = pu_factory.createPu(canny_1st_subchain, VPU_PU_CALB1);
    calb_1->setSize(iosize, iosize);
    ExynosVpuPu *nms = pu_factory.createPu(canny_1st_subchain, VPU_PU_NMS);
    nms->setSize(iosize, iosize);
    ExynosVpuPu *salb = pu_factory.createPu(canny_1st_subchain, VPU_PU_SALB0);
    salb->setSize(iosize, iosize);
    ExynosVpuPu *integral = pu_factory.createPu(canny_1st_subchain, VPU_PU_INTEGRAL);
    integral->setSize(iosize, iosize);
    ExynosVpuPu *dma_out = pu_factory.createPu(canny_1st_subchain, VPU_PU_DMAOT0);
    dma_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, glf, 0);
    ExynosVpuPu::connect(glf, 0, mde, 0);
    ExynosVpuPu::connect(glf, 1, mde, 1);
    ExynosVpuPu::connect(mde, 1, calb_0, 0);
    ExynosVpuPu::connect(mde, 0, calb_1, 0);
    ExynosVpuPu::connect(calb_0, 0, calb_1, 1);
    ExynosVpuPu::connect(calb_1, 0, nms, 0);
    ExynosVpuPu::connect(nms, 0, salb, 0);
    ExynosVpuPu::connect(salb, 0, integral, 0);
    ExynosVpuPu::connect(integral, 0, dma_out, 0);

    ExynosVpuIoExternalMem *io_external_mem[2];
    io_external_mem[0] = new ExynosVpuIoExternalMem(task);
    io_external_mem[1] = new ExynosVpuIoExternalMem(task);

    ExynosVpuMemmapExternal *in_memmap = new ExynosVpuMemmapExternal(task, io_external_mem[0]);
    ExynosVpuMemmapExternal *out_memmap = new ExynosVpuMemmapExternal(task, io_external_mem[1]);

    ExynosVpuIoFixedMapRoi *in_fixed_roi = new ExynosVpuIoFixedMapRoi(canny_first_half_process, in_memmap);
    ExynosVpuIoFixedMapRoi *out_fixed_roi = new ExynosVpuIoFixedMapRoi(canny_first_half_process, out_memmap);

    ExynosVpuIoTypesDesc *in_iotyps = new ExynosVpuIoTypesDesc(canny_first_half_process, in_fixed_roi);
    ExynosVpuIoTypesDesc *out_iotyps = new ExynosVpuIoTypesDesc(canny_first_half_process, out_fixed_roi);

    dma_in->setIoTypesDesc(in_iotyps);
    dma_out->setIoTypesDesc(out_iotyps);

    /* connect pu to io */
    ret = task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    if (ret != NO_ERROR) {
        VXLOGE("assigning pu to io fails");
        status = VX_FAILURE;
    }
    ret = task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out);
    if (ret != NO_ERROR) {
        VXLOGE("assigning pu to io fails");
        status = VX_FAILURE;
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelCannyEdge::initTask1FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(1, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = m_priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *canny_second_half_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, canny_second_half_process);
    ExynosVpuVertex::connect(canny_second_half_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(canny_second_half_process);

    /* define 2nd ~ 4th subchain */
    ExynosVpuSubchainHw *canny_2nd_subchain;

    ExynosVpuPu *dma_in[3];
    ExynosVpuPu *dma_out[3];
    ExynosVpuPu *slf;
    ExynosVpuPu *integral;

    /* it will be replaced loop */
    for (vx_uint32 i=0; i<3; i++) {
        canny_2nd_subchain = new ExynosVpuSubchainHw(canny_second_half_process);

        dma_in[i] = pu_factory.createPu(canny_2nd_subchain, VPU_PU_DMAIN0);
        dma_in[i]->setSize(iosize, iosize);
        slf = pu_factory.createPu(canny_2nd_subchain, VPU_PU_SLF70);
        slf->setSize(iosize, iosize);
        integral = pu_factory.createPu(canny_2nd_subchain, VPU_PU_INTEGRAL);
        integral->setSize(iosize, iosize);
        dma_out[i] = pu_factory.createPu(canny_2nd_subchain, VPU_PU_DMAOT0);
        dma_out[i]->setSize(iosize, iosize);

        ExynosVpuPu::connect(dma_in[i], 0, slf, 0);
        ExynosVpuPu::connect(slf, 0, integral, 0);
        ExynosVpuPu::connect(integral, 0, dma_out[i], 0);
    }

    ExynosVpuIoExternalMem *io_external_mem[2];
    io_external_mem[0] = new ExynosVpuIoExternalMem(task);
    io_external_mem[1] = new ExynosVpuIoExternalMem(task);

    ExynosVpuIoExternalMem *inter_external_mem[2];
    inter_external_mem[0] = new ExynosVpuIoExternalMem(task);
    inter_external_mem[1] = new ExynosVpuIoExternalMem(task);

    struct vpul_memory_map_desc *memmap_info;
    ExynosVpuMemmapExternal *in_memmap[3];
    ExynosVpuMemmapExternal *out_memmap[3];
    in_memmap[0] = new ExynosVpuMemmapExternal(task, io_external_mem[0]);
    out_memmap[0] = new ExynosVpuMemmapExternal(task, inter_external_mem[0]);
    memmap_info = out_memmap[0]->getMemmapInfo();
    memmap_info->image_sizes.pixel_bytes = 1;
    in_memmap[1] = new ExynosVpuMemmapExternal(task, inter_external_mem[0]);
    memmap_info = in_memmap[1]->getMemmapInfo();
    memmap_info->image_sizes.pixel_bytes = 1;
    out_memmap[1] = new ExynosVpuMemmapExternal(task, inter_external_mem[1]);
    memmap_info = out_memmap[1]->getMemmapInfo();
    memmap_info->image_sizes.pixel_bytes = 1;
    in_memmap[2] = new ExynosVpuMemmapExternal(task, inter_external_mem[1]);
    memmap_info = in_memmap[2]->getMemmapInfo();
    memmap_info->image_sizes.pixel_bytes = 1;
    out_memmap[2] = new ExynosVpuMemmapExternal(task, io_external_mem[1]);

    ExynosVpuIoFixedMapRoi *in_fixed_roi[3];
    ExynosVpuIoFixedMapRoi *out_fixed_roi[3];
    in_fixed_roi[0] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, in_memmap[0]);
    out_fixed_roi[0] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, out_memmap[0]);
    in_fixed_roi[1] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, in_memmap[1]);
    out_fixed_roi[1] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, out_memmap[1]);
    in_fixed_roi[2] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, in_memmap[2]);
    out_fixed_roi[2] = new ExynosVpuIoFixedMapRoi(canny_second_half_process, out_memmap[2]);

    ExynosVpuIoTypesDesc *in_iotyps[3];
    ExynosVpuIoTypesDesc *out_iotyps[3];
    in_iotyps[0] = new ExynosVpuIoTypesDesc(canny_second_half_process, in_fixed_roi[0]);
    out_iotyps[0] = new ExynosVpuIoTypesDesc(canny_second_half_process, out_fixed_roi[0]);
    in_iotyps[1] = new ExynosVpuIoTypesDesc(canny_second_half_process, in_fixed_roi[1]);
    out_iotyps[1] = new ExynosVpuIoTypesDesc(canny_second_half_process, out_fixed_roi[1]);
    in_iotyps[2] = new ExynosVpuIoTypesDesc(canny_second_half_process, in_fixed_roi[2]);
    out_iotyps[2] = new ExynosVpuIoTypesDesc(canny_second_half_process, out_fixed_roi[2]);

    dma_in[0]->setIoTypesDesc(in_iotyps[0]);
    dma_out[0]->setIoTypesDesc(out_iotyps[0]);

    dma_in[1]->setIoTypesDesc(in_iotyps[1]);
    dma_out[1]->setIoTypesDesc(out_iotyps[1]);

    dma_in[2]->setIoTypesDesc(in_iotyps[2]);
    dma_out[2]->setIoTypesDesc(out_iotyps[2]);

    /* connect pu for intermediate between subchain */
    status_t ret;
    ret = NO_ERROR;
    ret |= task_if->setInterPair(dma_out[0], dma_in[1]);
    ret |= task_if->setInterPair(dma_out[1], dma_in[2]);
    if (ret != NO_ERROR) {
        VXLOGE("connecting intermediate fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in[0]);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out[2]);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelCannyEdge::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_int32 gradient_3_xy_filter[] =
                {-1, 0, 1,
                -2, 0, 2,
                -1, 0, 1,

                -1, -2, -1,
                0, 0, 0,
                1, 2, 1};
    vx_int32 gradient_5_xy_filter[] =
                {-1, -2, 0, 2, 1,
                -4, -8, 0, 8, 4,
                -6, -12, 0, 12, 6,
                -4, -8, 0, 8, 4,
                -1, -2, 0, 2, 1,

                -1,-4, -6,-4,-1,
                -2,-8,-12,-8,-2,
                0, 0,  0, 0, 0,
                2, 8, 12, 8, 2,
                1, 4,  6, 4, 1};
    vx_int32 gradient_7_xy_filter[] =
                {-1,  -4,  -5, 0, 5, 4, 1,
                -6, -24, -30, 0, 30, 24, 6,
                -15, -60, -75, 0, 75, 60, 15,
                -20, -80, -100, 0, 100, 80, 20,
                -15, -60, -75, 0, 75, 60, 15,
                -6, -24, -30, 0, 30, 24, 6,
                -1,  -4,  -5, 0, 5, 4, 1,

                -1, -6,-15, -20,-15, -6,-1,
                -4,-24,-60, -80,-60,-24,-4,
                -5,-30,-75,-100,-75,-30,-5,
                0,  0,  0,   0,  0,  0, 0,
                5, 30, 75, 100, 75, 30, 5,
                4, 24, 60,  80, 60, 24, 4,
                1,  6, 15,  20, 15,  6, 1};

    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

#if 1
    /* JUN_TBD, enable after test */
    /* update vpu param from vx param */
    vx_threshold hyst = (vx_threshold)parameters[1];
    vx_scalar gradient_scalar = (vx_scalar)parameters[2];
    vx_scalar norm_scalar = (vx_scalar)parameters[3];

    vx_int32 lower = 0;
    vx_int32 upper = 0;
    status |= vxQueryThreshold(hyst, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_LOWER, &lower, sizeof(lower));
    status |= vxQueryThreshold(hyst, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_UPPER, &upper, sizeof(upper));
    if (status != VX_SUCCESS) {
        VXLOGE("querying threshold fails");
        goto EXIT;
    }

    vx_int32 gradient_size;
    status = vxReadScalarValue(gradient_scalar, &gradient_size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying scalar fails");
        goto EXIT;
    }

    vx_enum norm_type;
    status = vxReadScalarValue(norm_scalar, &norm_type);
    if (status != VX_SUCCESS) {
        VXLOGE("querying scalar fails");
        goto EXIT;
    }

    ExynosVpuPu *salb;
    salb = getTask(0)->getPu(VPU_PU_SALB0, 1, 0);
    if (salb == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }
    struct vpul_pu_salb *salb_param;
    salb_param = (struct vpul_pu_salb*)salb->getParameter();
    salb_param->thresh_lo = lower;
    salb_param->thresh_hi = upper;

    ExynosVpuPuGlf *glf;
    glf = (ExynosVpuPuGlf*)getTask(0)->getPu(VPU_PU_GLF50, 1, 0);
    if (glf == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }

    switch (gradient_size) {
    case 3:
        glf->setStaticCoffValue(gradient_3_xy_filter, 3, sizeof(gradient_3_xy_filter)/sizeof(gradient_3_xy_filter[0]));
        break;
    case 5:
        glf->setStaticCoffValue(gradient_5_xy_filter, 5, sizeof(gradient_5_xy_filter)/sizeof(gradient_5_xy_filter[0]));
        break;
    case 7:
        glf->setStaticCoffValue(gradient_7_xy_filter, 7, sizeof(gradient_7_xy_filter)/sizeof(gradient_7_xy_filter[0]));
        break;
    default:
        VXLOGE("un-supported gradient size, %d", gradient_size);
        status = VX_FAILURE;
        goto EXIT;
    }

    ExynosVpuPu *mde;
    mde = getTask(0)->getPu(VPU_PU_MDE, 1, 0);
    if (mde == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }
    struct vpul_pu_mde *mde_param;
    mde_param = (struct vpul_pu_mde*)mde->getParameter();
    switch (norm_type) {
    case VX_NORM_L1:
        mde_param->work_mode = 6;
        break;
    case VX_NORM_L2:
        mde_param->work_mode = 0;
        break;
    default:
        VXLOGE("un-supported norm type, %d", norm_type);
        status = VX_FAILURE;
        goto EXIT;
    }
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelCannyEdge::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

#if (TASK_INIT_MODE==TASK_INIT_BINARY)
    /* For task init from binary loading */
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
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, 0, 4, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
#else // TASK_INIT_MODE==TASK_INIT_API
    /* For task init from API */
    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];
    ExynosVpuTaskIfWrapper *task_wr_1 = m_task_wr_list[1];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    struct io_format_t io_format;
    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = m_valid_rect.end_x - m_valid_rect.start_x;
    io_format.height = m_valid_rect.end_y - m_valid_rect.start_y;
    io_format.pixel_byte = 1;

    struct io_memory_t io_memory;
    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    /* allocating intermediate buffer between task */
    io_buffer_info_t io_buffer_info;
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));
    io_buffer_info.size = io_format.width * io_format.height * io_format.pixel_byte;
    io_buffer_info.mapped = false;
    status = allocateBuffer(&io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
        goto EXIT;
    }

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    status = task_wr_1->setIoCustomParam(VS4L_DIRECTION_IN, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
    param_info.image.plane = 0;
    status = task_wr_1->setIoVxParam(VS4L_DIRECTION_OT, 0, 4, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }
#endif

EXIT:
    return status;
}

}; /* namespace android */

