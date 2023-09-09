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

#define LOG_TAG "ExynosVpuKernelHarris"
#include <cutils/log.h>

#include <stdlib.h>

#include "ExynosVpuKernelHarris.h"

#include "vpu_kernel_util.h"

#include "td-binary-harris.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_test_binary_harris_from_VDE_DS;

vx_status
ExynosVpuKernelHarris::inputValidator(vx_node node, vx_uint32 index)
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
            vx_scalar strength_thresh = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &strength_thresh, sizeof(strength_thresh));
            if ((status == VX_SUCCESS) && (strength_thresh)) {
                vx_enum type = 0;
                vxQueryScalar(strength_thresh, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    status = VX_SUCCESS;
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&strength_thresh);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar min_dist = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &min_dist, sizeof(min_dist));
            if ((status == VX_SUCCESS) && (min_dist)) {
                vx_enum type = 0;
                vxQueryScalar(min_dist, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 d = 0.0f;
                    status = vxReadScalarValue(min_dist, &d);
                    if ((status == VX_SUCCESS) && (0.0 <= d) && (d <= 30.0)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&min_dist);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar sens = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
            if ((status == VX_SUCCESS) && (sens)) {
                vx_enum type = 0;
                vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 k = 0.0f;
                    vxReadScalarValue(sens, &k);
                    if ((0.040000f <= k) && (k < 0.150001f)) {
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
    else if (index == 4 || index == 5) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar scalar = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if ((status == VX_SUCCESS) && (scalar)) {
                vx_enum type = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_INT32) {
                    vx_int32 size = 0;
                    vxReadScalarValue(scalar, &size);
                    if ((size == 3) || (size == 5) || (size == 7)) {
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
ExynosVpuKernelHarris::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 6) {
        vx_enum array_item_type = VX_TYPE_KEYPOINT;
        vx_size array_capacity = 0;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    } else if (index == 7) {
        vx_enum scalar_type = VX_TYPE_SIZE;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    } else {
        VXLOGE("out of index, %p, %d, %p", node, index, meta);
    }

    return status;
}

ExynosVpuKernelHarris::ExynosVpuKernelHarris(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_array_capacity = 0;
    strcpy(m_task_name, "harris");
}

ExynosVpuKernelHarris::~ExynosVpuKernelHarris(void)
{

}

vx_status
ExynosVpuKernelHarris::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];
    vx_array out_corner_array = (vx_array)parameters[6];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
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
ExynosVpuKernelHarris::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelHarris::initTaskFromBinary(void)
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
ExynosVpuKernelHarris::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapperHarris *task_wr = new ExynosVpuTaskIfWrapperHarris(this, 0);
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
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)19);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelHarris::initTaskFromApi(void)
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
ExynosVpuKernelHarris::initTask0FromApi(void)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuPuFactory pu_factory;

    ExynosVpuTaskIfWrapperHarris *task_wr = new ExynosVpuTaskIfWrapperHarris(this, 0);
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
    ExynosVpuProcess *harris_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, harris_process);
    ExynosVpuVertex::connect(harris_process, end_vertex);

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(harris_process);

    /* define 1st subchain */
    ExynosVpuSubchainHw *harris_subchain = new ExynosVpuSubchainHw(harris_process);

    /* define pus of 1st subchain */
    ExynosVpuPu *dma_in = pu_factory.createPu(harris_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *glf = pu_factory.createPu(harris_subchain, VPU_PU_GLF50);
    glf->setSize(iosize, iosize);
    ExynosVpuPu *calb_0 = pu_factory.createPu(harris_subchain, VPU_PU_CALB0);
    calb_0->setSize(iosize, iosize);
    ExynosVpuPu *slf7x7_0 = pu_factory.createPu(harris_subchain, VPU_PU_SLF70);
    slf7x7_0->setSize(iosize, iosize);
    ExynosVpuPu *calb_1 = pu_factory.createPu(harris_subchain, VPU_PU_CALB1);
    calb_1->setSize(iosize, iosize);
    ExynosVpuPu *slf7x7_1 = pu_factory.createPu(harris_subchain, VPU_PU_SLF71);
    slf7x7_1->setSize(iosize, iosize);
    ExynosVpuPu *calb_2 = pu_factory.createPu(harris_subchain, VPU_PU_CALB2);
    calb_2->setSize(iosize, iosize);
    ExynosVpuPu *slf7x7_2 = pu_factory.createPu(harris_subchain, VPU_PU_SLF72);
    slf7x7_2->setSize(iosize, iosize);
    ExynosVpuPu *dup_0_0 = pu_factory.createPu(harris_subchain, VPU_PU_DUPLICATOR0);
    dup_0_0->setSize(iosize, iosize);
    ExynosVpuPu *dup_1_0 = pu_factory.createPu(harris_subchain, VPU_PU_DUPLICATOR2);
    dup_1_0->setSize(iosize, iosize);
    ExynosVpuPu *fifo_1 = pu_factory.createPu(harris_subchain, VPU_PU_FIFO_1);
    fifo_1->setSize(iosize, iosize);
    ExynosVpuPu *dup_0_1 = pu_factory.createPu(harris_subchain, VPU_PU_DUPLICATOR_WIDE0);
    dup_0_1->setSize(iosize, iosize);
    ExynosVpuPu *fifo_0 = pu_factory.createPu(harris_subchain, VPU_PU_FIFO_0);
    fifo_0->setSize(iosize, iosize);
    ExynosVpuPu *dup_1_1 = pu_factory.createPu(harris_subchain, VPU_PU_DUPLICATOR_WIDE1);
    dup_1_1->setSize(iosize, iosize);
    ExynosVpuPu *splitter = pu_factory.createPu(harris_subchain, VPU_PU_SPLITTER0);
    splitter->setSize(iosize, iosize);
    ExynosVpuPu *crop_0 = pu_factory.createPu(harris_subchain, VPU_PU_CROP0);
    crop_0->setSize(iosize, iosize);
    ExynosVpuPu *crop_1 = pu_factory.createPu(harris_subchain, VPU_PU_CROP1);
    crop_1->setSize(iosize, iosize);
    ExynosVpuPu *jointer = pu_factory.createPu(harris_subchain, VPU_PU_JOINER0);
    jointer->setSize(iosize, iosize);
    ExynosVpuPu *map2list = pu_factory.createPu(harris_subchain, VPU_PU_MAP2LIST);
    map2list->setSize(iosize, iosize);
    ExynosVpuPu *mde = pu_factory.createPu(harris_subchain, VPU_PU_MDE);
    mde->setSize(iosize, iosize);
    ExynosVpuPu *nms = pu_factory.createPu(harris_subchain, VPU_PU_NMS);
    nms->setSize(iosize, iosize);
    ExynosVpuPu *dma_key_out = pu_factory.createPu(harris_subchain, VPU_PU_DMAOT_WIDE1);
    dma_key_out->setSize(iosize, iosize);

    ExynosVpuPu::connect(dma_in, 0, glf, 0);

    ExynosVpuPu::connect(glf, 0, dup_0_0, 0);
    ExynosVpuPu::connect(glf, 1, dup_0_1, 0);

    ExynosVpuPu::connect(dup_0_0, 0, dup_1_0, 0);
    ExynosVpuPu::connect(dup_0_0, 1, fifo_0, 0);
    ExynosVpuPu::connect(dup_0_1, 0, fifo_1, 0);
    ExynosVpuPu::connect(dup_0_1, 1, dup_1_1, 0);

    ExynosVpuPu::connect(dup_1_0, 0, calb_0, 0);
    ExynosVpuPu::connect(dup_1_0, 1, calb_0, 1);
    ExynosVpuPu::connect(fifo_0, 0, calb_1, 0);
    ExynosVpuPu::connect(fifo_1, 0, calb_1, 1);
    ExynosVpuPu::connect(dup_1_1, 0, calb_2, 0);
    ExynosVpuPu::connect(dup_1_1, 1, calb_2, 1);

    ExynosVpuPu::connect(calb_0, 0, slf7x7_0, 0);
    ExynosVpuPu::connect(calb_1, 0, slf7x7_1, 0);
    ExynosVpuPu::connect(calb_2, 0, slf7x7_2, 0);

    ExynosVpuPu::connect(slf7x7_0, 0, mde, 0);
    ExynosVpuPu::connect(slf7x7_1, 0, mde, 1);
    ExynosVpuPu::connect(slf7x7_2, 0, mde, 2);

    ExynosVpuPu::connect(mde, 0, nms, 0);

    ExynosVpuPu::connect(nms, 0, splitter, 0);

    ExynosVpuPu::connect(splitter, 0, crop_0, 0);
    ExynosVpuPu::connect(splitter, 1, crop_1, 0);

    ExynosVpuPu::connect(crop_0, 0, jointer, 0);
    ExynosVpuPu::connect(crop_1, 0, jointer, 1);

    ExynosVpuPu::connect(jointer, 0, map2list, 0);

    ExynosVpuPu::connect(map2list, 1, dma_key_out, 0);

    struct vpul_pu_glf *glf_param = (struct vpul_pu_glf*)glf->getParameter();
    glf_param->filter_size_mode = 2;
    glf_param->two_outputs = 1;
    glf_param->bits_out = 1;
    glf_param->RAM_type = 1;
    glf_param->border_mode_up = 1;
    glf_param->border_mode_down = 1;
    glf_param->border_mode_left = 1;
    glf_param->border_mode_right = 1;
    glf_param->signed_coefficients = 1;
    int32_t glf_coff[] = {-1, -2, 0, 2, 1,
                                    -4, -8, 0, 8, 4,
                                    -6, -12, 0, 12, 6,
                                    -4, -8, 0, 8, 4,
                                    -1, -2, 0, 2, 1};
    ((ExynosVpuPuSlf*)glf)->setStaticCoffValue(glf_coff, 5, sizeof(glf_coff)/sizeof(glf_coff[0]));

    struct vpul_pu_slf *slf_0_param = (struct vpul_pu_slf*)slf7x7_0->getParameter();

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(harris_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(harris_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(harris_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(harris_process, fixed_roi);
    dma_key_out->setIoTypesDesc(iotyps);

    status_t ret = NO_ERROR;
    ret |= task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);
    ret |= task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_key_out);
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
ExynosVpuKernelHarris::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
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

    vx_int32 block_3_xy_filter[] =
                {3277, 3277, 3277,
                3277, 3277, 3277};
    vx_int32 block_5_xy_filter[] =
                {3277, 3277, 3277, 3277, 3277,
                3277, 3277, 3277, 3277, 3277};
    vx_int32 block_7_xy_filter[] =
                {3277, 3277, 3277, 3277, 3277, 3277, 3277,
                3277, 3277, 3277, 3277, 3277, 3277, 3277};

    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* JUN_TBD, enable after test  */
    /* update vpu param from vx param */
    vx_scalar strength_scalar = (vx_scalar)parameters[1];
    vx_scalar min_scalar = (vx_scalar)parameters[2];
    vx_scalar sensitivity_scalar = (vx_scalar)parameters[3];
    vx_scalar gradient_scalar = (vx_scalar)parameters[4];
    vx_scalar block_scalar = (vx_scalar)parameters[5];
    vx_array point_array = (vx_array)parameters[6];

    vx_float32 strength_thresh;
    vx_float32 min_distance;
    vx_float32 sensitivity;
    vx_int32 gradient_size;
    vx_int32 block_size;

    status |= vxReadScalarValue(strength_scalar, &strength_thresh);
    status |= vxReadScalarValue(min_scalar, &min_distance);
    status |= vxReadScalarValue(sensitivity_scalar, &sensitivity);
    status |= vxReadScalarValue(gradient_scalar, &gradient_size);
    status |= vxReadScalarValue(block_scalar, &block_size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying scalar fails");
        goto EXIT;
    }

    ExynosVpuPu *map2list;
    map2list = getTask(0)->getPu(VPU_PU_MAP2LIST, 1, 0);
    if (map2list == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }
    struct vpul_pu_map2list *map2list_param;
    map2list_param = (struct vpul_pu_map2list*)map2list->getParameter();
    map2list_param->threshold_low = strength_thresh * pow(2, 32);
    map2list_param->num_of_point = m_array_capacity;

    /* min_distance can't apply to VPU chain */

    ExynosVpuPu *mde;
    mde = getTask(0)->getPu(VPU_PU_MDE, 1, 0);
    if (mde == NULL) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }
    struct vpul_pu_mde *mde_param;
    mde_param = (struct vpul_pu_mde*)mde->getParameter();
    mde_param->eig_coeff = sensitivity * pow(2, 24);

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

    ExynosVpuPuSlf *slf_0;
    ExynosVpuPuSlf *slf_1;
    ExynosVpuPuSlf *slf_2;
    slf_0 = (ExynosVpuPuSlf*)getTask(0)->getPu(VPU_PU_SLF70, 1, 0);
    slf_1 = (ExynosVpuPuSlf*)getTask(0)->getPu(VPU_PU_SLF71, 1, 0);
    slf_2 = (ExynosVpuPuSlf*)getTask(0)->getPu(VPU_PU_SLF72, 1, 0);
    if ((slf_0 == NULL) || (slf_1 == NULL) || (slf_2 == NULL)) {
        VXLOGE("getting pu fails");
        status = VX_FAILURE;
        goto EXIT;
    }
    struct vpul_pu_slf *slf_0_param;
    struct vpul_pu_slf *slf_1_param;
    struct vpul_pu_slf *slf_2_param;
    slf_0_param = (struct vpul_pu_slf*)slf_0->getParameter();
    slf_1_param = (struct vpul_pu_slf*)slf_1->getParameter();
    slf_2_param = (struct vpul_pu_slf*)slf_2->getParameter();
    switch (block_size) {
    case 3:
        slf_0->setStaticCoffValue(block_3_xy_filter, 3, sizeof(block_3_xy_filter)/sizeof(block_3_xy_filter[0]));
        slf_1->setStaticCoffValue(block_3_xy_filter, 3, sizeof(block_3_xy_filter)/sizeof(block_3_xy_filter[0]));
        slf_2->setStaticCoffValue(block_3_xy_filter, 3, sizeof(block_3_xy_filter)/sizeof(block_3_xy_filter[0]));
        break;
    case 5:
        slf_0->setStaticCoffValue(block_5_xy_filter, 5, sizeof(block_5_xy_filter)/sizeof(block_5_xy_filter[0]));
        slf_1->setStaticCoffValue(block_5_xy_filter, 5, sizeof(block_5_xy_filter)/sizeof(block_5_xy_filter[0]));
        slf_2->setStaticCoffValue(block_5_xy_filter, 5, sizeof(block_5_xy_filter)/sizeof(block_5_xy_filter[0]));
        break;
    case 7:
        slf_0->setStaticCoffValue(block_7_xy_filter, 7, sizeof(block_7_xy_filter)/sizeof(block_7_xy_filter[0]));
        slf_1->setStaticCoffValue(block_7_xy_filter, 7, sizeof(block_7_xy_filter)/sizeof(block_7_xy_filter[0]));
        slf_2->setStaticCoffValue(block_7_xy_filter, 7, sizeof(block_7_xy_filter)/sizeof(block_7_xy_filter[0]));
        break;
    default:
        VXLOGE("un-supported block size, %d", block_size);
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelHarris::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];
    vx_array point_array = (vx_array)parameters[6];
    vx_size capacity;
    status = vxQueryArray(point_array, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    param_info.image.plane = 0;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    /* for array */
    vx_uint32 byte_per_record, byte_per_line, line_number;
    byte_per_record = sizeof(struct key_triple);  /* 8B, (x, y, val) , 16-bit, 16-bit, 32-bit */
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

ExynosVpuTaskIfWrapperHarris::ExynosVpuTaskIfWrapperHarris(ExynosVpuKernelHarris *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperHarris::postProcessTask(const vx_reference parameters[])
{
    vx_status status = NO_ERROR;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != NO_ERROR) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy array from vpu format to vx format */
    vx_array vx_array_object;
    vx_scalar num_corners_scalar;
    vx_array_object = (vx_array)parameters[6];
    num_corners_scalar = (vx_scalar)parameters[7];

    vx_size capacity;
    status = vxQueryArray(vx_array_object, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
    if (status != NO_ERROR) {
        VXLOGE("querying array fails");
        goto EXIT;
    }

    status = vxAddEmptyArrayItems(vx_array_object, capacity);
    if (status != NO_ERROR) {
        VXLOGE("adding empty array fails");
        goto EXIT;
    }

    vx_keypoint_t *keypoint_array;
    vx_size array_stride;
    keypoint_array = NULL;
    array_stride = 0;
    status = vxAccessArrayRange(vx_array_object, 0, capacity, &array_stride, (void**)&keypoint_array, VX_WRITE_ONLY);
    if (status != NO_ERROR) {
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
    if (io_buffer_info->size < (int32_t)(sizeof(struct key_triple)*capacity + 2)) {
        VXLOGE("buffer size is too small, %d, %d", io_buffer_info->size, sizeof(struct key_triple)*capacity + 2);
        status = VX_FAILURE;
        goto EXIT;
    }

    /* two byte for blank is made by firmware */
    keytriple_array = (struct key_triple*)(io_buffer_info->addr+2);

    vx_size found_point_num;
    found_point_num = 0;

    for (vx_uint32 i=0; i < capacity; i++) {
        if ((keytriple_array[i].val_h == 0) && (keytriple_array[i].val_l == 0)) {
            break;
        }

        found_point_num++;
        keypoint_array[i].x = keytriple_array[i].x;
        keypoint_array[i].y = keytriple_array[i].y;
        keypoint_array[i].strength = (vx_float32)(keytriple_array[i].val_h << 16 | keytriple_array[i].val_l);
        keypoint_array[i].scale = 0;
        keypoint_array[i].orientation = 0;
        keypoint_array[i].tracking_status = 1;
        keypoint_array[i].error = 0;
    }

    status = vxCommitArrayRange(vx_array_object, 0, capacity, keypoint_array);
    if (status != NO_ERROR) {
        VXLOGE("commit array fails");
        goto EXIT;
    }

    if (found_point_num != capacity)
        vxTruncateArray(vx_array_object, found_point_num);

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

