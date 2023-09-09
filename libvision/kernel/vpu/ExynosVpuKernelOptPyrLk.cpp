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

#define LOG_TAG "ExynosVpuKernelOptPyrLk"
#include <cutils/log.h>

#include "ExynosVpuKernelOptPyrLk.h"

#include "vpu_kernel_util.h"

#define LK_HYBRID_PROCESS 1

#include "example_lk_task_ds.h"

namespace android {

using namespace std;

static vx_uint16 td_binary[] =
    TASK_DS;

typedef struct _vx_keypoint_t_optpyrlk_internal {
    vx_float32 x;                 /*!< \brief The x coordinate. */
    vx_float32 y;                 /*!< \brief The y coordinate. */
    vx_float32 strength;        /*!< \brief The strength of the keypoint. */
    vx_float32 scale;           /*!< \brief Unused field reserved for future use. */
    vx_float32 orientation;     /*!< \brief Unused field reserved for future use. */
    vx_int32 tracking_status;   /*!< \brief A zero indicates a lost point. */
    vx_float32 error;           /*!< \brief An tracking method specific error. */
} vx_keypoint_t_optpyrlk_internal;

#define  INT_ROUND(x,n)     (((x) + (1 << ((n)-1))) >> (n))

static vx_status createGradientTask(ExynosVpuTaskIf *task_if, vx_uint32 priority)
{
    ExynosVpuPuFactory pu_factory;

    ExynosVpuTask *task = new ExynosVpuTask(task_if);
    struct vpul_task *task_param = task->getTaskInfo();
    task_param->priority = priority;

    ExynosVpuVertex *start_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_START);
    ExynosVpuProcess *glf_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, glf_process);
    ExynosVpuVertex::connect(glf_process, end_vertex);

    vx_int32 gradient_xy_filter[] = {-3, 0, +3,
								-10, 0, +10,
								-3, 0, +3,
								-3, -10, -3,
								0, 0, 0,
								+3, +10, +3};

    /* define subchain */
    ExynosVpuPu *dma_in;
    ExynosVpuPu *dma_out1;
    ExynosVpuPu *dma_out2;

    ExynosVpuIoSize *iosize = new ExynosVpuIoSizeInout(glf_process);

    ExynosVpuSubchainHw *glf_subchain = new ExynosVpuSubchainHw(glf_process);

    /* define pu */
    dma_in = pu_factory.createPu(glf_subchain, VPU_PU_DMAIN0);
    dma_in->setSize(iosize, iosize);
    ExynosVpuPu *glf = pu_factory.createPu(glf_subchain, VPU_PU_GLF50);
    glf->setSize(iosize, iosize);

    struct vpul_pu_glf *glf_param = (struct vpul_pu_glf*)glf->getParameter();
    glf_param->filter_size_mode = 1;
    glf_param->two_outputs = 1;
    glf_param->bits_out = 1;
    glf_param->signed_out = 1;
    glf_param->RAM_type = 1;
    glf_param->border_mode_up = 0;
    glf_param->border_mode_down =0;
    glf_param->border_mode_left = 0;
    glf_param->border_mode_right = 0;
    glf_param->signed_coefficients = 1;

    ExynosVpuPu::connect(dma_in, 0, glf, 0);

    ((ExynosVpuPuGlf*)glf)->setStaticCoffValue(gradient_xy_filter, 3, sizeof(gradient_xy_filter)/sizeof(gradient_xy_filter[0]));

    dma_out1 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT0);
    dma_out1->setSize(iosize, iosize);
    ExynosVpuPu::connect(glf, 0, dma_out1, 0);

    dma_out2 = pu_factory.createPu(glf_subchain, VPU_PU_DMAOT_MNM0);
    dma_out2->setSize(iosize, iosize);
    ExynosVpuPu::connect(glf, 1, dma_out2, 0);

    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmapExternal *memmap;
    ExynosVpuIoFixedMapRoi *fixed_roi;
    ExynosVpuIoTypesDesc *iotyps;

    /* connect pu to io */
    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
    dma_in->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, dma_in);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
    dma_out1->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, dma_out1);

    io_external_mem = new ExynosVpuIoExternalMem(task);
    memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
    fixed_roi = new ExynosVpuIoFixedMapRoi(glf_process, memmap);
    iotyps = new ExynosVpuIoTypesDesc(glf_process, fixed_roi);
    dma_out2->setIoTypesDesc(iotyps);
    task_if->setIoPu(VS4L_DIRECTION_OT, 1, dma_out2);

    return VX_SUCCESS;
}

vx_status
ExynosVpuKernelOptPyrLk::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0 || index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_pyramid input = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input) {
                vx_size level = 0;
                vxQueryPyramid(input, VX_PYRAMID_ATTRIBUTE_LEVELS, &level, sizeof(level));
                if (level !=0) {
                    status = VX_SUCCESS;
                }
                vxReleasePyramid(&input);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 2 || index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_array arr = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &arr, sizeof(arr));
            if (arr) {
                vx_enum item_type = 0;
                vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &item_type, sizeof(item_type));
                if (item_type == VX_TYPE_KEYPOINT) {
                    status = VX_SUCCESS;
                }
                vxReleaseArray(&arr);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 5) {
        vx_parameter param = vxGetParameterByIndex(node, index);
         if (param) {
             vx_scalar sens = 0;
             status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
             if ((status == VX_SUCCESS) && (sens)) {
                 vx_enum type = 0;
                 vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                 if (type == VX_TYPE_ENUM) {
                     status = VX_SUCCESS;
                 } else {
                     status = VX_ERROR_INVALID_TYPE;
                 }
                 vxReleaseScalar(&sens);
             }
             vxReleaseParameter(&param);
         }
    } else if (index ==6) {
        vx_parameter param = vxGetParameterByIndex(node, index);
         if (param) {
             vx_scalar sens = 0;
             status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
             if ((status == VX_SUCCESS) && (sens)) {
                 vx_enum type = 0;
                 vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                 if (type == VX_TYPE_FLOAT32) {
                     status = VX_SUCCESS;
                 } else {
                     status = VX_ERROR_INVALID_TYPE;
                 }
                 vxReleaseScalar(&sens);
             }
             vxReleaseParameter(&param);
         }
    } else if (index ==7) {
        vx_parameter param = vxGetParameterByIndex(node, index);
         if (param) {
             vx_scalar sens = 0;
             status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
             if ((status == VX_SUCCESS) && (sens)) {
                 vx_enum type = 0;
                 vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                 if (type == VX_TYPE_UINT32) {
                     status = VX_SUCCESS;
                 } else {
                     status = VX_ERROR_INVALID_TYPE;
                 }
                 vxReleaseScalar(&sens);
             }
             vxReleaseParameter(&param);
         }
    } else if (index ==8) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar sens = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
            if ((status == VX_SUCCESS) && (sens)) {
                vx_enum type = 0;
                vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_BOOL) {
                    status = VX_SUCCESS;
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&sens);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 9) {
        vx_parameter param = vxGetParameterByIndex(node, index);
         if (param) {
             vx_scalar sens = 0;
             status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
             if ((status == VX_SUCCESS) && (sens)) {
                 vx_enum type = 0;
                 vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                 if (type == VX_TYPE_SIZE) {
                     status = VX_SUCCESS;
                 } else {
                     status = VX_ERROR_INVALID_TYPE;
                 }
                 vxReleaseScalar(&sens);
             }
             vxReleaseParameter(&param);
         }
    }

    return status;
}

vx_status
ExynosVpuKernelOptPyrLk::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 4) {
        vx_array arr = 0;
        vx_size capacity = 0;
        vx_parameter param = vxGetParameterByIndex(node, 2);
        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &arr, sizeof(arr));
        vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));

        vx_enum array_item_type = VX_TYPE_KEYPOINT;
        vx_size array_capacity = capacity;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;

        vxReleaseArray(&arr);
        vxReleaseParameter(&param);
    }

    return status;
}

ExynosVpuKernelOptPyrLk::ExynosVpuKernelOptPyrLk(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    strcpy(m_task_name, "optpyrlk");

    m_pyramid_level = 0;
    m_prev_pts_mod = NULL;
}

ExynosVpuKernelOptPyrLk::~ExynosVpuKernelOptPyrLk(void)
{
    if (m_prev_pts_mod) {
        vxReleaseArray(&m_prev_pts_mod);
        m_prev_pts_mod = NULL;
    }
}

vx_status
ExynosVpuKernelOptPyrLk::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    vx_pyramid pyramid;

    pyramid = (vx_pyramid)parameters[0];
    status = vxQueryPyramid(pyramid, VX_PYRAMID_ATTRIBUTE_LEVELS, &m_pyramid_level, sizeof(m_pyramid_level));
    if (status != VX_SUCCESS) {
        VXLOGE("querying pyramid fails");
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelOptPyrLk::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

#if (LK_HYBRID_PROCESS==0)
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

    vx_array prevPts;
    prevPts =  (vx_array)parameters[2];
    vx_enum src_item_type;
    vx_size src_capacity;

    status |= vxQueryArray(prevPts, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &src_item_type, sizeof(src_item_type));
    status |= vxQueryArray(prevPts, VX_ARRAY_ATTRIBUTE_CAPACITY, &src_capacity, sizeof(src_capacity));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }
    m_prev_pts_mod = vxCreateArray(vxGetContext((vx_reference)node), src_item_type, src_capacity);
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelOptPyrLk::initTaskFromBinary(void)
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
ExynosVpuKernelOptPyrLk::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

#if 1
    struct vpul_task *task = (struct vpul_task*)td_binary;
    task->n_memmap_desc += 4;
    task->n_external_mem_addresses += 2;

    struct vpul_memory_map_desc *memmap;
    memmap = &task->memmap_desc[0];
    memset(&memmap[0].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[1].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[2].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    /* pixel size for intermediate buffer should be set */
    memmap[2].image_sizes.pixel_bytes = 2;
    memset(&memmap[3].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    /* pixel size for intermediate buffer should be set */
    memmap[3].image_sizes.pixel_bytes = 4;
    memset(&memmap[4].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    /* pixel size for intermediate buffer should be set */
    memmap[4].image_sizes.pixel_bytes = 2;
    memset(&memmap[5].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    /* pixel size for intermediate buffer should be set */
    memmap[5].image_sizes.pixel_bytes = 4;
    memset(&memmap[6].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[7].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[8].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[9].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memset(&memmap[10].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memmap[10].index = 0;
    memset(&memmap[11].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memmap[11].index = 7;
    memset(&memmap[12].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memmap[12].index = 12;
    memmap[12].image_sizes.pixel_bytes = 2;
    memset(&memmap[13].image_sizes, 0x0, sizeof(struct vpul_image_size_desc));
    memmap[13].index = 13;
    memmap[13].image_sizes.pixel_bytes = 2;

    struct vpul_vertex *vertices = fst_vtx_ptr((struct vpul_task*)td_binary);
    struct vpul_process_inout *proc_inout = &vertices[1].proc.io;
    proc_inout->n_inout_types += 4;
    proc_inout->inout_types[10].is_dynamic = 0;
    proc_inout->inout_types[10].roi_index = 10;
    proc_inout->inout_types[11].is_dynamic = 0;
    proc_inout->inout_types[11].roi_index = 11;
    proc_inout->inout_types[12].is_dynamic = 0;
    proc_inout->inout_types[12].roi_index = 12;
    proc_inout->inout_types[13].is_dynamic = 0;
    proc_inout->inout_types[13].roi_index = 13;

    proc_inout->n_fixed_map_roi += 4;
    proc_inout->fixed_map_roi[10].memmap_idx = 10;
    proc_inout->fixed_map_roi[10].use_roi = 0;
    proc_inout->fixed_map_roi[11].memmap_idx = 11;
    proc_inout->fixed_map_roi[11].use_roi = 0;
    proc_inout->fixed_map_roi[12].memmap_idx = 12;
    proc_inout->fixed_map_roi[12].use_roi = 0;
    proc_inout->fixed_map_roi[13].memmap_idx = 13;
    proc_inout->fixed_map_roi[13].use_roi = 0;

    struct vpul_pu *pu;
    pu = fst_pu_ptr((struct vpul_task*)td_binary);

    pu[0].params.dma.inout_index = 10;
    pu[1].params.dma.inout_index = 12;

    /* inout_index of intermediate dma is wrong */
    pu[19].params.dma.inout_index = 2;
    pu[21].params.dma.inout_index = 3;

    pu[31].params.dma.inout_index = 13;
    pu[32].params.dma.inout_index = 11;
#endif

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

    /* connect pu for intermediate between subchain, SC_2 and SC_4 */
    task_if->setInterPair(16, 19);
    task_if->setInterPair(17, 21);

#if 1
    /* JUN_TBD, hack for avoid internal memery check */
    task_if->setInterPair(1, 31);
#endif

    /* connect pu to io */
    task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
    task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
    task_if->setIoPu(VS4L_DIRECTION_IN, 2, (uint32_t)4);
    task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)32);

EXIT:
    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelOptPyrLk::initTaskFromApi(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if;

    vx_uint32 i;
    for (i=0; i<(m_pyramid_level - 1); i++) {
        task_if = initTasknFromApi(i);
        if (task_if == NULL) {
            VXLOGE("task_%d isn't created", i);
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    task_if = initLastTaskFromApi(i);
    if (task_if == NULL) {
        VXLOGE("last task_%d isn't created", i);
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
ExynosVpuKernelOptPyrLk::initTasknFromApi(vx_uint32 task_index)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr = new ExynosVpuTaskIfWrapper(this, 0);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    createGradientTask(task_if, m_priority);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

ExynosVpuTaskIf*
ExynosVpuKernelOptPyrLk::initLastTaskFromApi(vx_uint32 task_index)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapperLk *task_wr = new ExynosVpuTaskIfWrapperLk(this, task_index);
    status = setTaskIfWrapper(task_index, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    createGradientTask(task_if, m_priority);

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelOptPyrLk::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

    /* update vpu param from vx param */
    /* do nothing */

EXIT:
    return status;
}

#if (LK_HYBRID_PROCESS==1)
vx_status
ExynosVpuKernelOptPyrLk::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    vx_pyramid pyramid = (vx_pyramid)parameters[0];

    /* connect vx param to io */
    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;
    vx_param_info_t param_info;

    for (vx_uint32 i=0; i<m_pyramid_level; i++) {
        ExynosVpuTaskIfWrapper *task_wr;
        task_wr = m_task_wr_list[i];

        param_info.pyramid.level = i;
        status = task_wr->setIoVxParam(VS4L_DIRECTION_IN, 0, 0, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }

        vx_image image = vxGetPyramidLevel(pyramid, i);
        vx_uint32 width, height;
        status |= vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
        status |= vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
        if (status != VX_SUCCESS) {
            VXLOGE("querying image fails, err:%d", status);
            goto EXIT;
        }
        vxReleaseImage(&image);

        /* Gx, 64x64 */
        io_format.format = VS4L_DF_IMAGE_S16;
        io_format.plane = 0;
        io_format.width = width;
        io_format.height = height;
        io_format.pixel_byte = 2;

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
        status = task_wr->setIoCustomParam(VS4L_DIRECTION_OT, 0, &io_format, &io_memory, &io_buffer_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
        m_gradient_x_base[i] = (vx_int16*)io_buffer_info.addr;


        /* Gy, 64x64 */
        io_format.format = VS4L_DF_IMAGE_S16;
        io_format.plane = 0;
        io_format.width = width;
        io_format.height = height;
        io_format.pixel_byte = 2;

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
        status = task_wr->setIoCustomParam(VS4L_DIRECTION_OT, 1, &io_format, &io_memory, &io_buffer_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
        m_gradient_y_base[i] = (vx_int16*)io_buffer_info.addr;
    }

EXIT:
    return status;
}
#else
vx_status
ExynosVpuKernelOptPyrLk::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* for input array */
    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = 2*1024*1024;  // 2MB for temporary
    io_format.height = 1;
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
    status = task_wr_0->setIoCustomParam(VS4L_DIRECTION_IN, 0, &io_format, &io_memory, &io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    param_info.pyramid.level = PYRAMID_WHOLE_LEVEL;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 1, 0, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    param_info.pyramid.level = PYRAMID_WHOLE_LEVEL;
    status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, 2, 1, param_info);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning param fails, %p", parameters);
        goto EXIT;
    }

    io_format.format = VS4L_DF_IMAGE_U8;
    io_format.plane = 0;
    io_format.width = 2*1024*1024;  // 2MB for temporary
    io_format.height = 1;
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
#endif

vx_status
ExynosVpuKernelOptPyrLk::LKTracker(
        const vx_image prevImg, vx_int16 *prevDerivIx, vx_int16 *prevDerivIy, const vx_image nextImg,
        const vx_array prevPts, vx_array nextPts,
        vx_scalar winSize_s, vx_scalar criteria_s,
        vx_uint32 level,vx_scalar epsilon,
        vx_scalar num_iterations)
{
    vx_status status = VX_SUCCESS;

    vx_size winSize;
    vxReadScalarValue(winSize_s,&winSize);
    vx_size halfWin = winSize*0.5f;
    vx_size list_length,list_indx;
    const vx_image prev_image = prevImg;
    const vx_image next_image = nextImg;
    vx_int16 *derivIx_base = prevDerivIx;
    vx_int16 *derivIy_base = prevDerivIy;

    vx_enum termination_Criteria;
    vx_rectangle_t rect;

    void *next_base = 0, *prev_base = 0;
    vx_imagepatch_addressing_t  derivIx_addr, next_addr, derivIy_addr, prev_addr;

    vx_int16 *IWinBuf_base = 0,*derivIWinBuf_x_base = 0,*derivIWinBuf_y_base = 0;
    vx_imagepatch_addressing_t  IWinBuf_addr, derivIWinBuf_x_addr, derivIWinBuf_y_addr;

    vx_size prevPts_stride = 0;
    vx_size nextPts_stride = 0;
    void *prevPtsFirstItem = NULL;
    void *nextPtsFirstItem = NULL;
    status = vxQueryArray(prevPts, VX_ARRAY_ATTRIBUTE_NUMITEMS, &list_length, sizeof(list_length));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }
    status |= vxAccessArrayRange(prevPts, 0, list_length, &prevPts_stride, &prevPtsFirstItem, VX_READ_ONLY);
    status |= vxAccessArrayRange(nextPts, 0, list_length, &nextPts_stride, &nextPtsFirstItem, VX_READ_AND_WRITE);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array fails");
        goto EXIT;
    }

    vx_keypoint_t_optpyrlk_internal *nextPt_item;
    vx_keypoint_t_optpyrlk_internal *prevPt_item;
    nextPt_item = (vx_keypoint_t_optpyrlk_internal*)nextPtsFirstItem;
    prevPt_item = (vx_keypoint_t_optpyrlk_internal*)prevPtsFirstItem;

    int num_iterations_i;
    vx_float32 epsilon_f;

    vxReadScalarValue(num_iterations,&num_iterations_i);
    vxReadScalarValue(epsilon,&epsilon_f);
    vxReadScalarValue(criteria_s,&termination_Criteria);

    rect.start_x = 0; rect.start_y = 0;
    vxQueryImage(next_image, VX_IMAGE_ATTRIBUTE_WIDTH, &rect.end_x, sizeof(rect.end_x));
    vxQueryImage(next_image, VX_IMAGE_ATTRIBUTE_HEIGHT, &rect.end_y, sizeof(rect.end_y));

    status = VX_SUCCESS;

    status |= vxAccessImagePatch(next_image, &rect, 0, &next_addr, (void **)&next_base,VX_READ_ONLY);
    status |= vxAccessImagePatch(prev_image, &rect, 0, &prev_addr, (void **)&prev_base,VX_READ_ONLY);
    derivIx_addr = next_addr;
    derivIx_addr.stride_x = 2;
    derivIx_addr.stride_y = derivIx_addr.dim_x * derivIx_addr.stride_x;
    derivIy_addr = derivIx_addr;

    /* temporal buffer for window calculation, s16, [win][win] */
    IWinBuf_base = (vx_int16*)malloc(winSize* winSize * 2);
    derivIWinBuf_x_base = (vx_int16*)malloc(winSize* winSize * 2);
    derivIWinBuf_y_base = (vx_int16*)malloc(winSize* winSize * 2);
    IWinBuf_addr.dim_x = winSize;
    IWinBuf_addr.dim_y = winSize;
    IWinBuf_addr.stride_x = 2;
    IWinBuf_addr.stride_y = IWinBuf_addr.dim_x * IWinBuf_addr.stride_x;
    IWinBuf_addr.scale_x = VX_SCALE_UNITY;
    IWinBuf_addr.scale_y = VX_SCALE_UNITY;
    IWinBuf_addr.step_x = 1;
    IWinBuf_addr.step_y = 1;
    derivIWinBuf_x_addr = derivIWinBuf_y_addr = IWinBuf_addr;

#if 1
	/* JUN_TBD, it will be removed after fixing border option */
	short *debug_dsrc_x;
	short *debug_dsrc_y;
	debug_dsrc_x = (short*)vxFormatImagePatchAddress2d(derivIx_base, 0, 0, &derivIx_addr);
	debug_dsrc_y = (short*)vxFormatImagePatchAddress2d(derivIy_base, 0, 0, &derivIy_addr);

	for (vx_uint32 jj = 0; jj < derivIx_addr.dim_y; jj++) {
		for (vx_uint32 ii = 0; ii < derivIx_addr.dim_x; ii++) {
			if ((ii==0) || (ii==derivIx_addr.dim_x-1) || (jj==0) || (jj==derivIx_addr.dim_x-1)) {
				*(debug_dsrc_x + ii + (jj*derivIx_addr.dim_y)) = 0;
				*(debug_dsrc_y + ii + (jj*derivIx_addr.dim_y)) = 0;
			}
		}
	}
#endif

    for(list_indx=0;list_indx<list_length;list_indx++) {
        /* Does not compute a point that shouldn't be tracked */
        if (prevPt_item[list_indx].tracking_status == 0)
            continue;

        vx_keypoint_t_optpyrlk_internal nextPt,prevPt;
        vx_keypoint_t_optpyrlk_internal iprevPt, inextPt;

        prevPt.x = prevPt_item[list_indx].x - halfWin;
        prevPt.y = prevPt_item[list_indx].y - halfWin;

        nextPt.x = nextPt_item[list_indx].x - halfWin;
        nextPt.y = nextPt_item[list_indx].y - halfWin;

        iprevPt.x = floor(prevPt.x);
        iprevPt.y = floor(prevPt.y);

        vx_int32 val_x = derivIx_addr.dim_x - winSize-1;
        vx_int32 val_y = derivIx_addr.dim_y - winSize-1;
        if ( iprevPt.x < 0 || iprevPt.x >= val_x || iprevPt.y < 0 || iprevPt.y >= val_y || val_x < 0 || val_y < 0) {
            if( level == 0 ) {
                nextPt_item[list_indx].tracking_status = 0;
                nextPt_item[list_indx].error = 0;
            }
            continue;
        }

        float a = prevPt.x - iprevPt.x;
        float b = prevPt.y - iprevPt.y;
        const int W_BITS = 14, W_BITS1 = 14;
        const float FLT_SCALE = 1.f/(1 << 20);
        int iw00 = (int)(((1.f - a)*(1.f - b)*(1 << W_BITS))+0.5f);
        int iw01 = (int)((a*(1.f - b)*(1 << W_BITS))+0.5f);
        int iw10 = (int)(((1.f - a)*b*(1 << W_BITS))+0.5f);
        int iw11 = (1 << W_BITS) - iw00 - iw01 - iw10;

        int dstep_x = (int)(derivIx_addr.stride_y)/2;
        int dstep_y = (int)(derivIy_addr.stride_y)/2;
        int stepJ = (int)(next_addr.stride_y);
        int stepI = (int)(prev_addr.stride_y);
        double A11 = 0, A12 = 0, A22 = 0;

        /* extract the patch from the first image, compute covariation matrix of derivatives */
        vx_uint32 x, y;
        for( y = 0; y < winSize; y++ ) {
            unsigned char *src = (unsigned char*)vxFormatImagePatchAddress2d(prev_base, iprevPt.x, y + iprevPt.y, &prev_addr);
            short *dsrc_x = (short*)vxFormatImagePatchAddress2d(derivIx_base, iprevPt.x, y + iprevPt.y, &derivIx_addr);
            short *dsrc_y = (short*)vxFormatImagePatchAddress2d(derivIy_base, iprevPt.x, y + iprevPt.y, &derivIy_addr);
            if ((src==NULL) || (dsrc_x==NULL) || (dsrc_y==NULL)) {
                VXLOGE("patching fails, src:%p, dsrc_x:%p, dsrc_y:%p", src, dsrc_x, dsrc_y);
                status = VX_FAILURE;
                goto EXIT;
            }

            short* Iptr = (short*)vxFormatImagePatchAddress2d(IWinBuf_base, 0, y,&IWinBuf_addr);
            short* dIptr_x = (short*)vxFormatImagePatchAddress2d(derivIWinBuf_x_base, 0, y,&derivIWinBuf_x_addr);
            short* dIptr_y = (short*)vxFormatImagePatchAddress2d(derivIWinBuf_y_base, 0, y,&derivIWinBuf_y_addr);
            if ((Iptr==NULL) || (dIptr_x==NULL) || (dIptr_y==NULL)) {
                VXLOGE("patching fails, Iptr:%p, dIptr_x:%p, dIptr_y:%p", Iptr, dIptr_x, dIptr_y);
                status = VX_FAILURE;
                goto EXIT;
            }

            x = 0;
            for( ; x < winSize; x++, dsrc_x ++, dsrc_y ++) {
                int ival = INT_ROUND(src[x]*iw00 + src[x+1]*iw01 +
                                      src[x+stepI]*iw10 + src[x+stepI+1]*iw11, W_BITS1-5);
                int ixval = INT_ROUND(dsrc_x[0]*iw00 + dsrc_x[1]*iw01 +
                                       dsrc_x[dstep_x]*iw10 + dsrc_x[dstep_x+1]*iw11, W_BITS1);
                int iyval = INT_ROUND(dsrc_y[0]*iw00 + dsrc_y[1]*iw01 + dsrc_y[dstep_y]*iw10 +
                                       dsrc_y[dstep_y+1]*iw11, W_BITS1);

                Iptr[x] = (short)ival;
                dIptr_x[x] = (short)ixval;
                dIptr_y[x] = (short)iyval;

                A11 += (float)(ixval*ixval);
                A12 += (float)(ixval*iyval);
                A22 += (float)(iyval*iyval);
            }
        }

        A11 *= FLT_SCALE;
        A12 *= FLT_SCALE;
        A22 *= FLT_SCALE;

        double D = A11*A22 - A12*A12;
        float minEig = (A22 + A11 - sqrt((A11-A22)*(A11-A22) +
                        4.f*A12*A12))/(2*winSize*winSize);

        if( minEig < 1.0e-04F || D < 1.0e-07F  ) {
            if( level == 0 ) {
                nextPt_item[list_indx].tracking_status = 0;
                nextPt_item[list_indx].error = 0;
            }
            continue;
        }

        D = 1.f/D;

        float prevDelta_x = 0.0f, prevDelta_y = 0.0f;

        int j = 0;
        while(j < num_iterations_i || termination_Criteria == VX_TERM_CRITERIA_EPSILON) {
            inextPt.x = floor(nextPt.x);
            inextPt.y = floor(nextPt.y);

            if( inextPt.x < 0 || inextPt.x >= next_addr.dim_x-winSize-1 ||
               inextPt.y < 0 || inextPt.y >= next_addr.dim_y- winSize-1 ) {
                if( level == 0  ) {
                    nextPt_item[list_indx].tracking_status = 0;
                    nextPt_item[list_indx].error = 0;
                }
                break;
            }

            a = nextPt.x - inextPt.x;
            b = nextPt.y - inextPt.y;
            iw00 = (int)(((1.f - a)*(1.f - b)*(1 << W_BITS))+0.5);
            iw01 = (int)((a*(1.f - b)*(1 << W_BITS))+0.5);
            iw10 = (int)(((1.f - a)*b*(1 << W_BITS))+0.5);
            iw11 = (1 << W_BITS) - iw00 - iw01 - iw10;
            double b1 = 0, b2 = 0;

            for( y = 0; y < winSize; y++ ) {
                unsigned char* Jptr = (unsigned char*)vxFormatImagePatchAddress2d(next_base, inextPt.x, y + inextPt.y, &next_addr);
                short* Iptr = (short*)vxFormatImagePatchAddress2d(IWinBuf_base,0, y,&IWinBuf_addr);
                short* dIptr_x = (short*)vxFormatImagePatchAddress2d(derivIWinBuf_x_base,0, y,&derivIWinBuf_x_addr);
                short* dIptr_y = (short*)vxFormatImagePatchAddress2d(derivIWinBuf_y_base,0, y,&derivIWinBuf_y_addr);
                x = 0;

                for( ; x < winSize; x++) {
                    int diff = INT_ROUND(Jptr[x]*iw00 + Jptr[x+1]*iw01 +
                                          Jptr[x+stepJ]*iw10 + Jptr[x+stepJ+1]*iw11,
                                          W_BITS1-5) - Iptr[x];
                    b1 += (float)(diff*dIptr_x[x]);
                    b2 += (float)(diff*dIptr_y[x]);
                }
            }

            b1 *= FLT_SCALE;
            b2 *= FLT_SCALE;

            float delta_x = (float)((A12*b2 - A22*b1) * D);
            float delta_y = (float)((A12*b1 - A11*b2) * D);

            nextPt.x += delta_x;
            nextPt.y += delta_y;
            nextPt_item[list_indx].x = nextPt.x + halfWin;
            nextPt_item[list_indx].y = nextPt.y + halfWin;

            if( (delta_x*delta_x + delta_y*delta_y) <= epsilon_f && (termination_Criteria == VX_TERM_CRITERIA_EPSILON || termination_Criteria == VX_TERM_CRITERIA_BOTH))
                break;

            if( j > 0 && fabs(delta_x + prevDelta_x) < 0.01 &&
               fabs(delta_y + prevDelta_y) < 0.01 ) {
                nextPt_item[list_indx].x -= delta_x*0.5f;
                nextPt_item[list_indx].y -= delta_y*0.5f;
                break;
            }
            prevDelta_x = delta_x;
            prevDelta_y = delta_y;
            j++;
        }

        /* we're going to drop this feature in case of iteration number's overflow */
        if (j >= num_iterations_i) {
            nextPt_item[list_indx].tracking_status = 0;
            nextPt_item[list_indx].error = 0;
        }

    }

    status |= vxCommitArrayRange(prevPts, 0, list_length,prevPtsFirstItem);
    status |= vxCommitArrayRange(nextPts, 0, list_length,nextPtsFirstItem);

    free(IWinBuf_base);
    free(derivIWinBuf_x_base);
    free(derivIWinBuf_y_base);

    vx_uint32 rec_width, rec_height;
    rec_width = rect.end_x - rect.start_x;
    rec_height = rect.end_y - rect.start_y;
    if ((next_addr.dim_x >= rec_width) && (next_addr.dim_y >= rec_height)) {
        status |= vxCommitImagePatch(next_image, &rect, 0, &next_addr, (void *)next_base);
        status |= vxCommitImagePatch(prev_image, &rect, 0, &prev_addr, (void *)prev_base);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("committing patch fails");
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelOptPyrLk::opticalFlowPyrLk(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_size list_length, list_indx;
    vx_int32 level;
    vx_pyramid old_pyramid = (vx_pyramid)parameters[0];
    vx_pyramid new_pyramid = (vx_pyramid)parameters[1];
    vx_array prevPts =  (vx_array)parameters[2];
    vx_array estimatedPts =  (vx_array)parameters[3];
    vx_array nextPts =  (vx_array)parameters[4];
    vx_scalar termination =  (vx_scalar)parameters[5];
    vx_scalar epsilon =  (vx_scalar)parameters[6];
    vx_scalar num_iterations =  (vx_scalar)parameters[7];
    vx_scalar use_initial_estimate =  (vx_scalar)parameters[8];
    vx_scalar window_dimension =  (vx_scalar)parameters[9];
    vx_size prevPts_stride = 0;
    vx_size estimatedPts_stride = 0;
    vx_size nextPts_stride = 0;
    void *prevPtsFirstItem;
    void *initialPtsFirstItem;
    void *nextPtsFirstItem;

    vx_bool use_initial_estimate_b;
    vx_float32 pyramid_scale;
    vx_keypoint_t_optpyrlk_internal *prevPt;
    vx_keypoint_t *initialPt = NULL;
    vx_keypoint_t_optpyrlk_internal *nextPt = NULL;

    vxReadScalarValue(use_initial_estimate,&use_initial_estimate_b);
    vxQueryPyramid(old_pyramid, VX_PYRAMID_ATTRIBUTE_SCALE , &pyramid_scale, sizeof(pyramid_scale));
    status = vxQueryArray(prevPts, VX_ARRAY_ATTRIBUTE_NUMITEMS, &list_length,sizeof(list_length));
    if (status != VX_SUCCESS) {
        VXLOGE("querying array fails");
        goto EXIT;
    }
    if (list_length == 0) {
        VXLOGW("feature list is empty");
        status = VX_SUCCESS;
        goto EXIT;
    }

    status = copyArray(prevPts, m_prev_pts_mod);
    if (status != VX_SUCCESS) {
        VXLOGE("copying array fails");
        goto EXIT;
    }

    for (level = m_pyramid_level; level > 0; level--) {
        vx_image old_image = vxGetPyramidLevel(old_pyramid, level-1);
        vx_image new_image = vxGetPyramidLevel(new_pyramid, level-1);
        vx_rectangle_t rect;

        prevPtsFirstItem = NULL;
        vxAccessArrayRange(m_prev_pts_mod, 0, list_length, &prevPts_stride, &prevPtsFirstItem, VX_READ_AND_WRITE);
        prevPt = (vx_keypoint_t_optpyrlk_internal*)prevPtsFirstItem;

        if (level == (vx_int32)m_pyramid_level) {
            if (use_initial_estimate_b) {
                vx_size list_length2 = 0;
                vxQueryArray(estimatedPts, VX_ARRAY_ATTRIBUTE_NUMITEMS, &list_length2,sizeof(list_length2));
                if (list_length2 != list_length) {
                    VXLOGE("array length is not matching");
                    return VX_ERROR_INVALID_PARAMETERS;
                }

                initialPtsFirstItem = NULL;
                vxAccessArrayRange(estimatedPts, 0, list_length, &estimatedPts_stride, &initialPtsFirstItem, VX_READ_ONLY);
                initialPt = (vx_keypoint_t *)initialPtsFirstItem;
            } else {
                initialPt = (vx_keypoint_t *)prevPt;
            }

            vxTruncateArray(nextPts, 0);
        } else {
            nextPtsFirstItem = NULL;
            vxAccessArrayRange(nextPts, 0, list_length, &nextPts_stride, &nextPtsFirstItem, VX_READ_AND_WRITE);
            nextPt = (vx_keypoint_t_optpyrlk_internal*)nextPtsFirstItem;
        }

        for(list_indx=0;list_indx<list_length;list_indx++) {
            if(level == (vx_int32)m_pyramid_level) {
                vx_keypoint_t_optpyrlk_internal keypoint;

                /* Adjust the prevPt coordinates to the level */
                (prevPt)->x = (((vx_keypoint_t*)prevPt))->x *(float)((pow(pyramid_scale,level-1)));
                (prevPt)->y = (((vx_keypoint_t*)prevPt))->y *(float)((pow(pyramid_scale,level-1)));

                if (use_initial_estimate_b) {
                    /* Estimate point coordinates not already adjusted */
                    keypoint.x = (initialPt)->x*(float)((pow(pyramid_scale,level-1)));
                    keypoint.y = (initialPt)->y*(float)((pow(pyramid_scale,level-1)));
                } else {
                    /* prevPt coordinates already adjusted */
                    keypoint.x = (prevPt)->x;
                    keypoint.y = (prevPt)->y;
                }
                keypoint.strength = (initialPt)->strength;
                keypoint.tracking_status = (initialPt)->tracking_status;
                keypoint.error = (initialPt)->error;

                status |= vxAddArrayItems(nextPts, 1, &keypoint, 0);
            } else {
                (prevPt)->x = (prevPt)->x *(1.0f/pyramid_scale);
                (prevPt)->y = (prevPt)->y *(1.0f/pyramid_scale);
                (nextPt)->x = (nextPt)->x *(1.0f/pyramid_scale);
                (nextPt)->y = (nextPt)->y *(1.0f/pyramid_scale);
                nextPt++;
            }
            prevPt++;
            initialPt++;
        }

        vxCommitArrayRange(m_prev_pts_mod, 0, list_length, prevPtsFirstItem);

        if (level != (vx_int32)m_pyramid_level) {
            vxCommitArrayRange(nextPts, 0, list_length, nextPtsFirstItem);
        } else if (use_initial_estimate_b) {
            vxCommitArrayRange(estimatedPts, 0, list_length,initialPtsFirstItem);
        }

        status |= LKTracker(old_image, m_gradient_x_base[level-1], m_gradient_y_base[level-1], new_image,
                    m_prev_pts_mod, nextPts,
                    window_dimension, termination, level-1,
                    epsilon,num_iterations);

        vxReleaseImage(&new_image);
        vxReleaseImage(&old_image);
    }

    nextPtsFirstItem = NULL;
    vxAccessArrayRange(nextPts, 0, list_length, &nextPts_stride, &nextPtsFirstItem, VX_READ_AND_WRITE);
    nextPt = (vx_keypoint_t_optpyrlk_internal*)nextPtsFirstItem;

    prevPt = (vx_keypoint_t_optpyrlk_internal*)prevPtsFirstItem;
    initialPt = (vx_keypoint_t *)initialPtsFirstItem;
    nextPt = (vx_keypoint_t_optpyrlk_internal*)nextPtsFirstItem;
    for(list_indx=0;list_indx<list_length;list_indx++) {
        (((vx_keypoint_t*)nextPt))->x = (vx_int32)((nextPt)->x + 0.5f);
        (((vx_keypoint_t*)nextPt))->y = (vx_int32)((nextPt)->y + 0.5f);
        (((vx_keypoint_t*)prevPt))->x = (vx_int32)((prevPt)->x + 0.5f);
        (((vx_keypoint_t*)prevPt))->y = (vx_int32)((prevPt)->y + 0.5f);
        nextPt++;
        prevPt++;
    }

    vxCommitArrayRange(nextPts, 0, list_length,nextPtsFirstItem);

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperLk::ExynosVpuTaskIfWrapperLk(ExynosVpuKernelOptPyrLk *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperLk::postProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    ExynosVpuKernelOptPyrLk *kernel;
    kernel = (ExynosVpuKernelOptPyrLk*)getKernel();
    kernel->opticalFlowPyrLk(parameters);

EXIT:
    return status;
}

}; /* namespace android */

