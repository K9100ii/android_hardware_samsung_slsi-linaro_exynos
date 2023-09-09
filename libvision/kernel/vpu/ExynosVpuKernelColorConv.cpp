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

#define LOG_TAG "ExynosVpuKernelColorConv"
#include <cutils/log.h>

#include "ExynosVpuKernelColorConv.h"

#include "vpu_kernel_util.h"

#include "td-binary-colorconv_rgb2nv12.h"
#include "td-binary-colorconv_yuyv2rgbx.h"
#include "td-binary-colorconv_nv212bgrx.h"
#include "td-binary-colorconv_nv212bgr.h"
#include "td-binary-colorconv_bgr2bgrx.h"
#include "td-binary-colorconv_u82bgrx.h"
#include "td-binary-colorconv_s162bgrx.h"
#include "td-binary-colorconv_bgrx2nv12.h"
#include "td-binary-colorconv_nv122bgrx.h"

namespace android {

using namespace std;

enum format_domain_t {
    INVALID_DOMAIN,

    YUV_DOMAIN,
    RGB_DOMAIN
};

static vx_uint16 td_binary_rgb2nv12[] =
    TASK_test_binary_colorconv_rbg2nv12_from_VDE_DS;

static vx_uint16 td_binary_yuyv2rgbx[] =
    TASK_test_binary_colorconv_yuyv2rgb_from_VDE_DS;

static vx_uint16 td_binary_nv212bgrx[] =
    TASK_test_binary_colorconv_nv212bgrx_from_VDE_DS;

static vx_uint16 td_binary_nv212bgr[] =
    TASK_test_binary_colorconv_nv212bgr_from_VDE_DS;

static vx_uint16 td_binary_bgr2bgrx[] =
    TASK_test_binary_colorconv_bgr2bgrx_from_VDE_DS;

static vx_uint16 td_binary_u82bgrx[] =
    TASK_test_binary_colorconv_u82bgrx_from_VDE_DS;

static vx_uint16 td_binary_s162bgrx[] =
    TASK_test_binary_colorconv_s162bgrx_from_VDE_DS;

static vx_uint16 td_binary_bgrx2nv12[] =
    TASK_test_binary_colorconv_bgrx2nv12_from_VDE_DS;

static vx_uint16 td_binary_nv122bgrx[] =
    TASK_test_binary_colorconv_nv122bgrx_from_VDE_DS;

static enum format_domain_t getFormatDomain(vx_df_image format)
{
    enum format_domain_t domain = INVALID_DOMAIN;

    switch(format) {
    case VX_DF_IMAGE_RGB:
    case VX_DF_IMAGE_RGBX:
    case VX_DF_IMAGE_BGR:
    case VX_DF_IMAGE_BGRX:
        domain = RGB_DOMAIN;
        break;
    case VX_DF_IMAGE_NV12:
    case VX_DF_IMAGE_NV21:
    case VX_DF_IMAGE_UYVY:
    case VX_DF_IMAGE_YUYV:
    case VX_DF_IMAGE_IYUV:
        domain = YUV_DOMAIN;
        break;
    }

    return domain;
}

vx_status
ExynosVpuKernelColorConv::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_SUCCESS;
    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, 0);
        if (param) {
            vx_image image = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
            if (image) {
                vx_df_image format = 0;
                vx_uint32 width = 0, height = 0;

                vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxQueryImage(image, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(image, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                // check to make sure the input format is supported.
                switch (format) {
                case VX_DF_IMAGE_U8:
                case VX_DF_IMAGE_S16:
                    break;
                case VX_DF_IMAGE_RGB:  /* 8:8:8 interleaved */
                case VX_DF_IMAGE_RGBX: /* 8:8:8:8 interleaved */
                case VX_DF_IMAGE_BGR:  /* 8:8:8 interleaved */
                case VX_DF_IMAGE_BGRX: /* 8:8:8:8 interleaved */
                case VX_DF_IMAGE_NV12: /* 4:2:0 co-planar*/
                case VX_DF_IMAGE_NV21: /* 4:2:0 co-planar*/
                case VX_DF_IMAGE_IYUV: /* 4:2:0 planar */
                    if (height & 1) {
                        status = VX_ERROR_INVALID_DIMENSION;
                        break;
                    }
                    /* no break */
                case VX_DF_IMAGE_YUYV: /* 4:2:2 interleaved */
                case VX_DF_IMAGE_UYVY: /* 4:2:2 interleaved */
                    if (width & 1) {
                        status = VX_ERROR_INVALID_DIMENSION;
                    }
                    break;
                default:
                    status = VX_ERROR_INVALID_FORMAT;
                    break;
                }
                vxReleaseImage(&image);
            } else {
                status = VX_ERROR_INVALID_PARAMETERS;
            }
            vxReleaseParameter(&param);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
    } else {
        status = VX_ERROR_INVALID_PARAMETERS;
    }

    return status;
}

static vx_df_image color_combos[][2] = {
    /* {src, dst} */
    {VX_DF_IMAGE_RGB, VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_RGB, VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_RGB, VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_RGB, VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_RGBX,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_RGBX,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_RGBX,VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_RGBX,VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_NV21},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_UYVY,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_UYVY,VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_UYVY,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_UYVY,VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_UYVY,VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_YUYV,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_YUYV,VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_YUYV,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_YUYV,VX_DF_IMAGE_YUV4},
    {VX_DF_IMAGE_YUYV,VX_DF_IMAGE_IYUV},
    {VX_DF_IMAGE_IYUV,VX_DF_IMAGE_RGB},
    {VX_DF_IMAGE_IYUV,VX_DF_IMAGE_RGBX},
    {VX_DF_IMAGE_IYUV,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_IYUV,VX_DF_IMAGE_YUV4},

    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_BGRX},
    {VX_DF_IMAGE_NV21,VX_DF_IMAGE_BGR},
    {VX_DF_IMAGE_BGR,VX_DF_IMAGE_BGRX},
    {VX_DF_IMAGE_U8,VX_DF_IMAGE_BGRX},
    {VX_DF_IMAGE_S16,VX_DF_IMAGE_BGRX},
    {VX_DF_IMAGE_BGRX,VX_DF_IMAGE_NV12},
    {VX_DF_IMAGE_NV12,VX_DF_IMAGE_BGRX},
};

vx_status
ExynosVpuKernelColorConv::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 1) {
        vx_parameter param0 = vxGetParameterByIndex(node, 0);
        vx_parameter param1 = vxGetParameterByIndex(node, 1);
        if (param0 && param1) {
            vx_image output = 0, input = 0;
            vxQueryParameter(param0, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            vxQueryParameter(param1, VX_PARAMETER_ATTRIBUTE_REF, &output, sizeof(output));
            if (input && output) {
                vx_df_image src = VX_DF_IMAGE_VIRT;
                vx_df_image dst = VX_DF_IMAGE_VIRT;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &src, sizeof(src));
                vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &dst, sizeof(dst));

                /* can't be a unspecified format */
                if (dst != VX_DF_IMAGE_VIRT) {
                    vx_uint32 i = 0;
                    for (i = 0; i < dimof(color_combos); i++) {
                        if ((color_combos[i][0] == src) &&
                            (color_combos[i][1] == dst)) {
                            vx_uint32 width, height;
                            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                            vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &dst, sizeof(dst));
                            vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                            vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                            status = VX_SUCCESS;
                            break;
                        }
                    }
                }
                vxReleaseImage(&input);
                vxReleaseImage(&output);
            }
            vxReleaseParameter(&param0);
            vxReleaseParameter(&param1);
        }
    }

    return status;
}

ExynosVpuKernelColorConv::ExynosVpuKernelColorConv(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{
    m_src_format = VX_DF_IMAGE_VIRT;
    m_dst_format = VX_DF_IMAGE_VIRT;

    m_src_io_num = 0;
    m_dst_io_num = 0;

    strcpy(m_task_name, "colorconvert");
}

ExynosVpuKernelColorConv::~ExynosVpuKernelColorConv(void)
{

}

vx_status
ExynosVpuKernelColorConv::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    vx_image input = (vx_image)parameters[0];
    vx_image output = (vx_image)parameters[1];

    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &m_src_format, sizeof(m_src_format));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &m_dst_format, sizeof(m_dst_format));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image format fails, err:%d", status);
        return status;
    }

    status  = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
        return status;
    }

    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_PLANES, &m_src_io_num, sizeof(m_src_io_num));
    status |= vxQueryImage(output, VX_IMAGE_ATTRIBUTE_PLANES, &m_dst_io_num, sizeof(m_dst_io_num));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image planes fails, err:%d", status);
        return status;
    }

    return status;
}

vx_status
ExynosVpuKernelColorConv::initTask(vx_node node, const vx_reference *parameters)
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
ExynosVpuKernelColorConv::initTaskFromBinary(void)
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
ExynosVpuKernelColorConv::initTask0FromBinary(void)
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
    if (m_src_format==VX_DF_IMAGE_RGB && m_dst_format==VX_DF_IMAGE_NV12) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_rgb2nv12);
    } else if (m_src_format==VX_DF_IMAGE_YUYV && m_dst_format==VX_DF_IMAGE_RGBX) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_yuyv2rgbx);
    } else if (m_src_format==VX_DF_IMAGE_NV21 && m_dst_format==VX_DF_IMAGE_BGRX) {
#if 1
        /* JUN_TBD, switch crop/scale type */
        struct vpul_vertex *vertex;
        vertex = vtx_ptr((struct vpul_task*)td_binary_nv212bgrx, 1);
        vertex->proc.io.sizes[2].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[3].type = VPUL_SIZEOP_SCALE;
        vertex->proc.io.sizes[4].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[5].type = VPUL_SIZEOP_SCALE;
#endif
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_nv212bgrx);
    } else if (m_src_format==VX_DF_IMAGE_NV21 && m_dst_format==VX_DF_IMAGE_BGR) {
#if 1
        /* JUN_TBD, switch crop/scale type */
        struct vpul_vertex *vertex;
        vertex = vtx_ptr((struct vpul_task*)td_binary_nv212bgr, 1);
        vertex->proc.io.sizes[2].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[3].type = VPUL_SIZEOP_SCALE;
        vertex->proc.io.sizes[4].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[5].type = VPUL_SIZEOP_SCALE;
#endif
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_nv212bgr);
    } else if (m_src_format==VX_DF_IMAGE_BGR && m_dst_format==VX_DF_IMAGE_BGRX) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_bgr2bgrx);
    } else if (m_src_format==VX_DF_IMAGE_U8 && m_dst_format==VX_DF_IMAGE_BGRX) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_u82bgrx);
    } else if (m_src_format==VX_DF_IMAGE_S16 && m_dst_format==VX_DF_IMAGE_BGRX) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_s162bgrx);
    } else if (m_src_format==VX_DF_IMAGE_BGRX && m_dst_format==VX_DF_IMAGE_NV12) {
        ret = task_if->importTaskStr((struct vpul_task*)td_binary_bgrx2nv12);
    } else if (m_src_format==VX_DF_IMAGE_NV12 && m_dst_format==VX_DF_IMAGE_BGRX) {
#if 1
        /* JUN_TBD, switch crop/scale type */
        struct vpul_vertex *vertex;
        vertex = vtx_ptr((struct vpul_task*)td_binary_nv122bgrx, 1);
        vertex->proc.io.sizes[2].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[3].type = VPUL_SIZEOP_SCALE;
        vertex->proc.io.sizes[4].type = VPUL_SIZEOP_FORCE_CROP;
        vertex->proc.io.sizes[5].type = VPUL_SIZEOP_SCALE;
#endif

        ret = task_if->importTaskStr((struct vpul_task*)td_binary_nv122bgrx);
    } else {
        VXLOGE("no supporting binary, src:0x%x, dst:0x%x", m_src_format, m_dst_format);
        ret = INVALID_OPERATION;
    }
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect pu to io */
    if (m_src_format==VX_DF_IMAGE_RGB && m_dst_format==VX_DF_IMAGE_NV12) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)9);
        task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)4);
    } else if (m_src_format==VX_DF_IMAGE_YUYV && m_dst_format==VX_DF_IMAGE_RGBX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)9);
    } else if (m_src_format==VX_DF_IMAGE_NV21 && m_dst_format==VX_DF_IMAGE_BGRX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)9);
    } else if (m_src_format==VX_DF_IMAGE_NV21 && m_dst_format==VX_DF_IMAGE_BGR) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)7);
    } else if (m_src_format==VX_DF_IMAGE_BGR && m_dst_format==VX_DF_IMAGE_BGRX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)1);
    } else if (m_src_format==VX_DF_IMAGE_U8 && m_dst_format==VX_DF_IMAGE_BGRX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)9);
    } else if (m_src_format==VX_DF_IMAGE_S16 && m_dst_format==VX_DF_IMAGE_BGRX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)10);
    } else if (m_src_format==VX_DF_IMAGE_BGRX && m_dst_format==VX_DF_IMAGE_NV12) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)9);
        task_if->setIoPu(VS4L_DIRECTION_OT, 1, (uint32_t)4);
    } else if (m_src_format==VX_DF_IMAGE_NV12 && m_dst_format==VX_DF_IMAGE_BGRX) {
        task_if->setIoPu(VS4L_DIRECTION_IN, 0, (uint32_t)0);
        task_if->setIoPu(VS4L_DIRECTION_IN, 1, (uint32_t)2);
        task_if->setIoPu(VS4L_DIRECTION_OT, 0, (uint32_t)7);
    } else {
        status = VX_FAILURE;
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelColorConv::initTaskFromApi(void)
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
ExynosVpuKernelColorConv::initTask0FromApi(void)
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
    ExynosVpuProcess *colorconv_process = new ExynosVpuProcess(task);
    ExynosVpuVertex *end_vertex = new ExynosVpuVertex(task, VPUL_VERTEXT_END);

    ExynosVpuVertex::connect(start_vertex, colorconv_process);
    ExynosVpuVertex::connect(colorconv_process, end_vertex);

    /* define subchain */
    ExynosVpuSubchainHw *colcorconv_subchain = new ExynosVpuSubchainHw(colorconv_process);

    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_in_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> pre_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> post_out_pu_list;
    Vector<pair<ExynosVpuPu*, vx_uint32>> main_out_pu_list;

    vx_uint32 i;
    pair<ExynosVpuPu*, vx_uint32> pu;

    status = setPreColorConvPu(colcorconv_subchain, &pre_in_pu_list, &pre_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setMainColorConvPu(colcorconv_subchain, &pre_out_pu_list, &main_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }
    status = setPostColorConvPu(colcorconv_subchain, &main_out_pu_list, &post_out_pu_list);
    if (status != VX_SUCCESS) {
        VXLOGE("making subchain fails");
        goto EXIT;
    }

    /* connect pu to io */
    ExynosVpuIoExternalMem *io_external_mem;
    ExynosVpuMemmap *pu_memmap;
    ExynosVpuIoFixedMapRoi *pu_fixed_roi;
    ExynosVpuIoTypesDesc *pu_iotyps;

    for (i=0; i<m_src_io_num; i++) {
        pu = pre_in_pu_list.editItemAt(i);

        io_external_mem = new ExynosVpuIoExternalMem(task);
        pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        pu_fixed_roi = new ExynosVpuIoFixedMapRoi(colorconv_process, pu_memmap);
        pu_iotyps = new ExynosVpuIoTypesDesc(colorconv_process, pu_fixed_roi);
        pu.first->setIoTypesDesc(pu_iotyps);

        task_if->setIoPu(VS4L_DIRECTION_IN, i, pu.first);
    }
    for (i=0; i<m_dst_io_num; i++) {
        pu = post_out_pu_list.editItemAt(i);

        io_external_mem = new ExynosVpuIoExternalMem(task);
        pu_memmap = new ExynosVpuMemmapExternal(task, io_external_mem);
        pu_fixed_roi = new ExynosVpuIoFixedMapRoi(colorconv_process, pu_memmap);
        pu_iotyps = new ExynosVpuIoTypesDesc(colorconv_process, pu_fixed_roi);
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
ExynosVpuKernelColorConv::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    /* update vpu param from vx param */
    /* do nothing */
    if (node == NULL)
        VXLOGE("node is null, %p, %p", node, parameters);

EXIT:
    return status;
}

vx_status
ExynosVpuKernelColorConv::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    /* connect vx param to io */
    vx_param_info_t param_info;
    memset(&param_info, 0x0, sizeof(param_info));

    vx_uint32 i;
    for (i=0; i<m_src_io_num; i++) {
        param_info.image.plane = i;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_IN, i, 0, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }
    for (i=0; i<m_dst_io_num; i++) {
        param_info.image.plane = i;
        status = task_wr_0->setIoVxParam(VS4L_DIRECTION_OT, i, 1, param_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelColorConv::setPreColorConvPu(ExynosVpuSubchainHw *subchain,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_in_pu_list,
                                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    ExynosVpuPuFactory pu_factory;
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());

    if ((m_src_format==VX_DF_IMAGE_RGB) || (m_src_format==VX_DF_IMAGE_RGBX)) {
        ExynosVpuPu *dma_in = pu_factory.createPu(subchain, VPU_PU_DMAIN_WIDE0);
        dma_in->setSize(iosize, iosize);
        ret_in_pu_list->push_back(make_pair(dma_in, 0));

        ExynosVpuPu *splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        splitter->setSize(iosize, iosize);
        struct vpul_pu_spliter *splitter_param = (struct vpul_pu_spliter*)splitter->getParameter();
        splitter_param->out0_byte0 = 0;
        splitter_param->out0_byte1 = 4;
        splitter_param->out1_byte0 = 1;
        splitter_param->out1_byte1 = 4;
        splitter_param->out2_byte0 = 2;
        splitter_param->out2_byte1 = 4;
        /* disable */
        splitter_param->out3_byte0 = 4;
        splitter_param->out3_byte1 = 4;

        ExynosVpuPu::connect(dma_in, 0, splitter, 0);

        ret_out_pu_list->push_back(make_pair(splitter, 0));
        ret_out_pu_list->push_back(make_pair(splitter, 1));
        ret_out_pu_list->push_back(make_pair(splitter, 2));
    } else if (m_src_format == VX_DF_IMAGE_NV12) {
        ExynosVpuPu *dma_in1 = pu_factory.createPu(subchain, VPU_PU_DMAIN_WIDE0);
        dma_in1->setSize(iosize, iosize);
        ExynosVpuPu *dma_in2 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_in2->setSize(iosize, iosize);

        ret_in_pu_list->push_back(make_pair(dma_in1, 0));
        ret_in_pu_list->push_back(make_pair(dma_in2, 0));

        ExynosVpuPu *splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        splitter->setSize(iosize, iosize);
        ExynosVpuPu::connect(dma_in2, 0, splitter, 0);

        struct vpul_pu_spliter *splitter_param = (struct vpul_pu_spliter*)splitter->getParameter();
        splitter_param->out0_byte0 = 0;     /* U */
        splitter_param->out0_byte1 = 4;
        splitter_param->out1_byte0 = 1;     /* V */
        splitter_param->out1_byte1 = 4;
        /* disable */
        splitter_param->out2_byte0 = 4;
        splitter_param->out2_byte1 = 4;
        splitter_param->out3_byte0 = 4;
        splitter_param->out3_byte1 = 4;

        ExynosVpuPu *upsampling_u = pu_factory.createPu(subchain, VPU_PU_SLF50);
        upsampling_u->setSize(iosize, iosize);
        ExynosVpuPu::connect(splitter, 0, upsampling_u, 0);

        struct vpul_pu_slf *slf_param_u = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_u->work_mode = 0;
        slf_param_u->upsample_mode = 1;

        ExynosVpuPu *upsampling_v = pu_factory.createPu(subchain, VPU_PU_SLF51);
        upsampling_v->setSize(iosize, iosize);
        ExynosVpuPu::connect(splitter, 1, upsampling_v, 0);

        struct vpul_pu_slf *slf_param_v = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_v->work_mode = 0;
        slf_param_v->upsample_mode = 1;

        ret_out_pu_list->push_back(make_pair(dma_in1, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_u, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_v, 0));
    } else if (m_src_format == VX_DF_IMAGE_NV21) {
        ExynosVpuPu *dma_in1 = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in1->setSize(iosize, iosize);
        ExynosVpuPu *dma_in2 = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_in2->setSize(iosize, iosize);

        ret_in_pu_list->push_back(make_pair(dma_in1, 0));
        ret_in_pu_list->push_back(make_pair(dma_in2, 0));

        ExynosVpuPu *splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        splitter->setSize(iosize, iosize);
        ExynosVpuPu::connect(dma_in2, 0, splitter, 0);

        struct vpul_pu_spliter *splitter_param = (struct vpul_pu_spliter*)splitter->getParameter();
        splitter_param->out0_byte0 = 1;     /* U */
        splitter_param->out0_byte1 = 4;
        splitter_param->out1_byte0 = 0;     /* V */
        splitter_param->out1_byte1 = 4;
        /* disable */
        splitter_param->out2_byte0 = 4;
        splitter_param->out2_byte1 = 4;
        splitter_param->out3_byte0 = 4;
        splitter_param->out3_byte1 = 4;

        ExynosVpuPu *upsampling_u = pu_factory.createPu(subchain, VPU_PU_SLF50);
        upsampling_u->setSize(iosize, iosize);
        ExynosVpuPu::connect(splitter, 0, upsampling_u, 0);

        struct vpul_pu_slf *slf_param_u = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_u->work_mode = 0;
        slf_param_u->upsample_mode = 1;

        ExynosVpuPu *upsampling_v = pu_factory.createPu(subchain, VPU_PU_SLF51);
        upsampling_v->setSize(iosize, iosize);
        ExynosVpuPu::connect(splitter, 1, upsampling_v, 0);

        struct vpul_pu_slf *slf_param_v = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_v->work_mode = 0;
        slf_param_v->upsample_mode = 1;

        ret_out_pu_list->push_back(make_pair(dma_in1, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_u, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_v, 0));
    } else if (m_src_format == VX_DF_IMAGE_UYVY) {
        ExynosVpuPu *dma_in = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);
        ret_in_pu_list->push_back(make_pair(dma_in, 0));

        ExynosVpuPu *splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        splitter->setSize(iosize, iosize);
        struct vpul_pu_spliter *splitter_param = (struct vpul_pu_spliter*)splitter->getParameter();
        splitter_param->out0_byte0 = 0;     /* Y */
        splitter_param->out0_byte1 = 4;
        splitter_param->out1_byte0 = 1;     /* U, V */
        splitter_param->out1_byte1 = 4;
        /* disable */
        splitter_param->out2_byte0 = 4;
        splitter_param->out2_byte1 = 4;
        splitter_param->out3_byte0 = 4;
        splitter_param->out3_byte1 = 4;

        ExynosVpuPu *duplicator = pu_factory.createPu(subchain, VPU_PU_DUPLICATOR0);
        duplicator->setSize(iosize, iosize);

        ExynosVpuPu *joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        joiner->setSize(iosize, iosize);
        struct vpul_pu_joiner *joiner_param = (struct vpul_pu_joiner*)joiner->getParameter();
        joiner_param->work_mode = 0;
        joiner_param->input0_enable = 1;
        joiner_param->input1_enable = 1;
        joiner_param->out_byte0_source_stream = 2;
        joiner_param->out_byte1_source_stream = 0;
        joiner_param->out_byte2_source_stream = 6;
        joiner_param->out_byte3_source_stream = 4;

        ExynosVpuPu *fifo = pu_factory.createPu(subchain, VPU_PU_FIFO_0);
        fifo->setSize(iosize, iosize);

        ExynosVpuPu *splitter2 = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
        splitter2->setSize(iosize, iosize);
        struct vpul_pu_spliter *splitter2_param = (struct vpul_pu_spliter*)splitter2->getParameter();
        splitter2_param->out0_byte0 = 0;     /* U */
        splitter2_param->out0_byte1 = 4;
        splitter2_param->out1_byte0 = 1;     /* V */
        splitter2_param->out1_byte1 = 4;
        /* disable */
        splitter2_param->out2_byte0 = 4;
        splitter2_param->out2_byte1 = 4;
        splitter2_param->out3_byte0 = 4;
        splitter2_param->out3_byte1 = 4;

        ExynosVpuPu *offset_u = pu_factory.createPu(subchain, VPU_PU_SALB0);
        offset_u->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_u = (struct vpul_pu_salb*)offset_u->getParameter();
        salb_param_u->bits_out0 = 1;
        salb_param_u->signed_out0 = 1;
        salb_param_u->input_enable = 1;
        salb_param_u->operation_code = 9;
        salb_param_u->val_med_filler = 65408;
        salb_param_u->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu *offset_v = pu_factory.createPu(subchain, VPU_PU_SALB1);
        offset_v->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_v = (struct vpul_pu_salb*)offset_v->getParameter();
        salb_param_v->bits_out0 = 1;
        salb_param_v->signed_out0 = 1;
        salb_param_v->input_enable = 1;
        salb_param_v->operation_code = 9;
        salb_param_v->val_med_filler = 65408;
        salb_param_v->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu::connect(dma_in, 0, splitter, 0);
        ExynosVpuPu::connect(splitter, 0, fifo, 0);
        ExynosVpuPu::connect(splitter, 1, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, joiner, 0);
        ExynosVpuPu::connect(duplicator, 1, joiner, 1);
        ExynosVpuPu::connect(joiner, 0, splitter2, 0);
        ExynosVpuPu::connect(splitter2, 0, offset_u, 0);
        ExynosVpuPu::connect(splitter2, 1, offset_v, 0);

        ret_out_pu_list->push_back(make_pair(fifo, 0));
        ret_out_pu_list->push_back(make_pair(offset_u, 0));
        ret_out_pu_list->push_back(make_pair(offset_v, 0));
    } else if (m_src_format == VX_DF_IMAGE_YUYV) {
        ExynosVpuPu *dma_in = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_in->setSize(iosize, iosize);
        ret_in_pu_list->push_back(make_pair(dma_in, 0));

        ExynosVpuPu *splitter = pu_factory.createPu(subchain, VPU_PU_SPLITTER0);
        splitter->setSize(iosize, iosize);
        struct vpul_pu_spliter *splitter_param = (struct vpul_pu_spliter*)splitter->getParameter();
        splitter_param->out0_byte0 = 0;     /* Y */
        splitter_param->out0_byte1 = 4;
        splitter_param->out1_byte0 = 1;     /* U, V */
        splitter_param->out1_byte1 = 4;
        /* disable */
        splitter_param->out2_byte0 = 4;
        splitter_param->out2_byte1 = 4;
        splitter_param->out3_byte0 = 4;
        splitter_param->out3_byte1 = 4;

        ExynosVpuPu *duplicator = pu_factory.createPu(subchain, VPU_PU_DUPLICATOR0);
        duplicator->setSize(iosize, iosize);

        ExynosVpuPu *joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        joiner->setSize(iosize, iosize);
        struct vpul_pu_joiner *joiner_param = (struct vpul_pu_joiner*)joiner->getParameter();
        joiner_param->work_mode = 0;
        joiner_param->input0_enable = 1;
        joiner_param->input1_enable = 1;
        joiner_param->out_byte0_source_stream = 0;
        joiner_param->out_byte1_source_stream = 2;
        joiner_param->out_byte2_source_stream = 4;
        joiner_param->out_byte3_source_stream = 6;

        ExynosVpuPu *fifo = pu_factory.createPu(subchain, VPU_PU_FIFO_0);
        fifo->setSize(iosize, iosize);

        ExynosVpuPu *splitter2 = pu_factory.createPu(subchain, VPU_PU_SPLITTER1);
        splitter2->setSize(iosize, iosize);
        struct vpul_pu_spliter *splitter2_param = (struct vpul_pu_spliter*)splitter2->getParameter();
        splitter2_param->out0_byte0 = 0;     /* U */
        splitter2_param->out0_byte1 = 4;
        splitter2_param->out1_byte0 = 1;     /* V */
        splitter2_param->out1_byte1 = 4;
        /* disable */
        splitter2_param->out2_byte0 = 4;
        splitter2_param->out2_byte1 = 4;
        splitter2_param->out3_byte0 = 4;
        splitter2_param->out3_byte1 = 4;

        ExynosVpuPu *offset_u = pu_factory.createPu(subchain, VPU_PU_SALB0);
        offset_u->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_u = (struct vpul_pu_salb*)offset_u->getParameter();
        salb_param_u->bits_out0 = 1;
        salb_param_u->signed_out0 = 1;
        salb_param_u->input_enable = 1;
        salb_param_u->operation_code = 9;
        salb_param_u->val_med_filler = 65408;
        salb_param_u->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu *offset_v = pu_factory.createPu(subchain, VPU_PU_SALB1);
        offset_v->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_v = (struct vpul_pu_salb*)offset_v->getParameter();
        salb_param_v->bits_out0 = 1;
        salb_param_v->signed_out0 = 1;
        salb_param_v->input_enable = 1;
        salb_param_v->operation_code = 9;
        salb_param_v->val_med_filler = 65408;
        salb_param_v->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu::connect(dma_in, 0, splitter, 0);
        ExynosVpuPu::connect(splitter, 0, fifo, 0);
        ExynosVpuPu::connect(splitter, 1, duplicator, 0);
        ExynosVpuPu::connect(duplicator, 0, joiner, 0);
        ExynosVpuPu::connect(duplicator, 1, joiner, 1);
        ExynosVpuPu::connect(joiner, 0, splitter2, 0);
        ExynosVpuPu::connect(splitter2, 0, offset_u, 0);
        ExynosVpuPu::connect(splitter2, 1, offset_v, 0);

        ret_out_pu_list->push_back(make_pair(fifo, 0));
        ret_out_pu_list->push_back(make_pair(offset_u, 0));
        ret_out_pu_list->push_back(make_pair(offset_v, 0));
    } else if (m_src_format == VX_DF_IMAGE_IYUV) {
        ExynosVpuPu *dma_y_in = pu_factory.createPu(subchain, VPU_PU_DMAIN0);
        dma_y_in->setSize(iosize, iosize);
        ExynosVpuPu *dma_u_in = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM0);
        dma_u_in->setSize(iosize, iosize);
        ExynosVpuPu *dma_v_in = pu_factory.createPu(subchain, VPU_PU_DMAIN_MNM1);
        dma_v_in->setSize(iosize, iosize);

        ret_in_pu_list->push_back(make_pair(dma_y_in, 0));
        ret_in_pu_list->push_back(make_pair(dma_u_in, 0));
        ret_in_pu_list->push_back(make_pair(dma_v_in, 0));

        ExynosVpuPu *fifo = pu_factory.createPu(subchain, VPU_PU_FIFO_0);
        fifo->setSize(iosize, iosize);

        ExynosVpuPu *upsampling_u = pu_factory.createPu(subchain, VPU_PU_SLF50);
        upsampling_u->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param_u = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_u->work_mode = 0;
        slf_param_u->upsample_mode = 2;

        ExynosVpuPu *upsampling_v = pu_factory.createPu(subchain, VPU_PU_SLF51);
        upsampling_v->setSize(iosize, iosize);
        struct vpul_pu_slf *slf_param_v = (struct vpul_pu_slf*)upsampling_u->getParameter();
        slf_param_u->work_mode = 0;
        slf_param_u->upsample_mode = 2;

        ExynosVpuPu::connect(dma_y_in, 0, fifo, 0);
        ExynosVpuPu::connect(dma_u_in, 0, upsampling_u, 0);
        ExynosVpuPu::connect(dma_v_in, 1, upsampling_v, 0);

        ret_out_pu_list->push_back(make_pair(fifo, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_u, 0));
        ret_out_pu_list->push_back(make_pair(upsampling_v, 0));
    } else {
        VXLOGE("un-supported input format, 0x%x", m_src_format);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

vx_status
ExynosVpuKernelColorConv::setMainColorConvPu(ExynosVpuSubchainHw *subchain,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    vx_status status = VX_FAILURE;
    ExynosVpuPuFactory pu_factory;

    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());
    ExynosVpuPu *ccm = NULL;

    if (getFormatDomain(m_src_format)==getFormatDomain(m_dst_format)) {
        /* do nothing if the domain is same */
        pair<ExynosVpuPu*, vx_uint32> in_pu;

        in_pu = in_pu_list->editItemAt(0);
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));
        in_pu = in_pu_list->editItemAt(1);
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));
        in_pu = in_pu_list->editItemAt(2);
        ret_out_pu_list->push_back(make_pair(in_pu.first, in_pu.second));

        status = VX_SUCCESS;
    } else {
        ccm = pu_factory.createPu(subchain, VPU_PU_CCM);
        ccm->setSize(iosize, iosize);
        struct vpul_pu_ccm *ccm_param = (struct vpul_pu_ccm*)ccm->getParameter();
        ccm_param->input_enable = (1<<2) | (1<<1) | (1<<0);
        ccm_param->output_enable = (1<<2) | (1<<1) | (1<<0);

        if (getFormatDomain(m_src_format)==YUV_DOMAIN && getFormatDomain(m_dst_format)==RGB_DOMAIN) {
            ccm_param->signed_in = 1;
            ccm_param->signed_out = 0;
            ccm_param->coefficient_shift = 11;
            ccm_param->coefficient_0 = 2048;
            ccm_param->coefficient_1 = 0;
            ccm_param->coefficient_2 = 3225;
            ccm_param->coefficient_3 = 2048;
            ccm_param->coefficient_4 = 3712;
            ccm_param->coefficient_5 = 3137;
            ccm_param->coefficient_6 = 2048;
            ccm_param->coefficient_7 = 3800;
            ccm_param->coefficient_8 = 0;
        } else if (getFormatDomain(m_src_format)==RGB_DOMAIN && getFormatDomain(m_dst_format)==YUV_DOMAIN) {
            ccm_param->signed_in = 0;
            ccm_param->signed_out = 1;
            ccm_param->coefficient_shift = 11;
            ccm_param->coefficient_0 = 435;
            ccm_param->coefficient_1 = 1465;
            ccm_param->coefficient_2 = 148;
            ccm_param->coefficient_3 = 3861;
            ccm_param->coefficient_4 = 3307;
            ccm_param->coefficient_5 = 1024;
            ccm_param->coefficient_6 = 1024;
            ccm_param->coefficient_7 = 3166;
            ccm_param->coefficient_8 = 4002;
        }

        pair<ExynosVpuPu*, vx_uint32> in_pu;

        in_pu = in_pu_list->editItemAt(0);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, ccm, 0);
        in_pu = in_pu_list->editItemAt(1);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, ccm, 1);
        in_pu = in_pu_list->editItemAt(2);
        ExynosVpuPu::connect(in_pu.first, in_pu.second, ccm, 2);

        ret_out_pu_list->push_back(make_pair(ccm, 0));
        ret_out_pu_list->push_back(make_pair(ccm, 1));
        ret_out_pu_list->push_back(make_pair(ccm, 2));

        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVpuKernelColorConv::setPostColorConvPu(ExynosVpuSubchainHw *subchain,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                                            Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list)
{
    ExynosVpuPuFactory pu_factory;
    ExynosVpuIoSizeInout *iosize = new ExynosVpuIoSizeInout(subchain->getProcess());

    pair<ExynosVpuPu*, vx_uint32> in_pu;

    if ((m_dst_format==VX_DF_IMAGE_RGB) || (m_dst_format==VX_DF_IMAGE_RGBX)) {
        ExynosVpuPu *joiner = pu_factory.createPu(subchain, VPU_PU_JOINER1);
        joiner->setSize(iosize, iosize);
        struct vpul_pu_joiner *joiner_param = (struct vpul_pu_joiner*)joiner->getParameter();
        joiner_param->work_mode = 0;
        joiner_param->input0_enable = 1;
        joiner_param->input1_enable = 1;
        joiner_param->input2_enable = 1;
        joiner_param->out_byte0_source_stream = 0;
        joiner_param->out_byte1_source_stream = 4;
        joiner_param->out_byte2_source_stream = 8;
        joiner_param->out_byte3_source_stream = 16;

        in_pu = in_pu_list->editItemAt(0);  /* r */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, joiner, 0);  /* r */
        in_pu = in_pu_list->editItemAt(1);  /* g */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, joiner, 1);  /* g */
        in_pu = in_pu_list->editItemAt(2);  /* b */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, joiner, 2);  /* b */

        ExynosVpuPu *dma_out_rgb = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out_rgb->setSize(iosize, iosize);

        ExynosVpuPu::connect(joiner, 0, dma_out_rgb, 0);

        ret_out_pu_list->push_back(make_pair(dma_out_rgb, 0));
    } else if (m_dst_format==VX_DF_IMAGE_NV12) {
        if (m_dst_io_num != 2) {
            VXLOGE("dest io number is invalid, format:0x%x, io_num:%d", m_dst_format, m_dst_io_num);
            return VX_FAILURE;
        }

        ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(subchain->getProcess());
        ExynosVpuIoSizeScale *iosize_half = new ExynosVpuIoSizeScale(subchain->getProcess(), iosize, sizeop_sacle);
        struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
        scales_info->horizontal.numerator = 1;
        scales_info->horizontal.denominator = 2;

        ExynosVpuPu *offset_u = pu_factory.createPu(subchain, VPU_PU_SALB0);
        offset_u->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_u = (struct vpul_pu_salb*)offset_u->getParameter();
        salb_param_u->signed_in0 = 1;
        salb_param_u->signed_out0 = 1;
        salb_param_u->input_enable = 1;
        salb_param_u->operation_code = 9;
        salb_param_u->val_med_filler = 128;
        salb_param_u->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu *offset_v = pu_factory.createPu(subchain, VPU_PU_SALB1);
        offset_v->setSize(iosize, iosize);
        struct vpul_pu_salb *salb_param_v = (struct vpul_pu_salb*)offset_v->getParameter();
        salb_param_v->signed_in0 = 1;
        salb_param_v->signed_out0 = 1;
        salb_param_v->input_enable = 1;
        salb_param_v->operation_code = 9;
        salb_param_v->val_med_filler = 128;
        salb_param_v->salbregs_custom_trunc_bittage = 1;

        ExynosVpuPu *downsampling_u = pu_factory.createPu(subchain, VPU_PU_SLF50);
        downsampling_u->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param_u = (struct vpul_pu_slf*)downsampling_u->getParameter();
        slf_param_u->border_mode_up = 1;
        slf_param_u->border_mode_down = 1;
        slf_param_u->border_mode_left = 1;
        slf_param_u->border_mode_right = 1;

        ExynosVpuPu *downsampling_v = pu_factory.createPu(subchain, VPU_PU_SLF51);
        downsampling_v->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param_v = (struct vpul_pu_slf*)downsampling_v->getParameter();
        slf_param_v->border_mode_up = 1;
        slf_param_v->border_mode_down = 1;
        slf_param_v->border_mode_left = 1;
        slf_param_v->border_mode_right = 1;

        /* yuv 444 -> yuv 420 */
        slf_param_u->downsample_cols = 1;
        slf_param_u->downsample_rows = 1;
        slf_param_u->sampling_offset_x = 1;
        slf_param_u->sampling_offset_y = 1;

        slf_param_v->downsample_cols = 1;
        slf_param_v->downsample_rows = 1;
        slf_param_v->sampling_offset_x = 1;
        slf_param_v->sampling_offset_y = 1;

        ExynosVpuPu *joiner = pu_factory.createPu(subchain, VPU_PU_JOINER0);
        joiner->setSize(iosize, iosize);
        struct vpul_pu_joiner *joiner_param = (struct vpul_pu_joiner*)joiner->getParameter();
        joiner_param->work_mode = 0;
        joiner_param->input0_enable = 1;
        joiner_param->input1_enable = 1;
        joiner_param->out_byte0_source_stream = 0;
        joiner_param->out_byte1_source_stream = 4;
        joiner_param->out_byte2_source_stream = 16;
        joiner_param->out_byte3_source_stream = 16;

        in_pu = in_pu_list->editItemAt(1);  /* u */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, offset_u, 0);

        in_pu = in_pu_list->editItemAt(2);  /* v */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, offset_v, 0);

        ExynosVpuPu::connect(offset_u, 0, downsampling_u, 0);  /* u */
        ExynosVpuPu::connect(offset_v, 0, downsampling_v, 0);  /* v */
        ExynosVpuPu::connect(downsampling_u, 0, joiner, 0);  /* u */
        ExynosVpuPu::connect(downsampling_v, 0, joiner, 1);  /* v */

        ExynosVpuPu *dma_out_y = pu_factory.createPu(subchain, VPU_PU_DMAOT0);
        dma_out_y->setSize(iosize, iosize);
        ExynosVpuPu *dma_out_uv = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out_uv->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);  /* y */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out_y, 0);

        ExynosVpuPu::connect(joiner, 0, dma_out_uv, 0);

        ret_out_pu_list->push_back(make_pair(dma_out_y, 0));
        ret_out_pu_list->push_back(make_pair(dma_out_uv, 1));
    } else if (m_dst_format==VX_DF_IMAGE_IYUV) {
        if (m_dst_io_num != 3) {
            VXLOGE("dest io number is invalid, format:0x%x, io_num:%d", m_dst_format, m_dst_io_num);
            return VX_FAILURE;
        }

        ExynosVpuIoSizeOpScale *sizeop_sacle = new ExynosVpuIoSizeOpScale(subchain->getProcess());
        ExynosVpuIoSizeScale *iosize_half = new ExynosVpuIoSizeScale(subchain->getProcess(), iosize, sizeop_sacle);
        struct vpul_scales *scales_info = sizeop_sacle->getScaleInfo();
        scales_info->horizontal.numerator = 1;
        scales_info->horizontal.denominator = 2;

        ExynosVpuPu *downsampling_u = pu_factory.createPu(subchain, VPU_PU_SLF50);
        downsampling_u->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param_u = (struct vpul_pu_slf*)downsampling_u->getParameter();
        slf_param_u->border_mode_up = 1;
        slf_param_u->border_mode_down = 1;
        slf_param_u->border_mode_left = 1;
        slf_param_u->border_mode_right = 1;

        ExynosVpuPu *downsampling_v = pu_factory.createPu(subchain, VPU_PU_SLF51);
        downsampling_v->setSize(iosize, iosize_half);
        struct vpul_pu_slf *slf_param_v = (struct vpul_pu_slf*)downsampling_v->getParameter();
        slf_param_v->border_mode_up = 1;
        slf_param_v->border_mode_down = 1;
        slf_param_v->border_mode_left = 1;
        slf_param_v->border_mode_right = 1;

        /* yuv 444 -> yuv 420 */
        slf_param_u->downsample_cols = 1;
        slf_param_u->downsample_rows = 1;
        slf_param_u->sampling_offset_x = 1;
        slf_param_u->sampling_offset_y = 1;

        slf_param_v->downsample_cols = 1;
        slf_param_v->downsample_rows = 1;
        slf_param_v->sampling_offset_x = 1;
        slf_param_v->sampling_offset_y = 1;

        in_pu = in_pu_list->editItemAt(1);  /* u */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, downsampling_u, 0);

        in_pu = in_pu_list->editItemAt(2);  /* v */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, downsampling_v, 0);

        ExynosVpuPu *dma_out_y = pu_factory.createPu(subchain, VPU_PU_DMAOT0);
        dma_out_y->setSize(iosize, iosize);
        ExynosVpuPu *dma_out_u = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out_u->setSize(iosize, iosize);
        ExynosVpuPu *dma_out_v = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE1);
        dma_out_v->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);  /* y */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out_y, 0);
        in_pu = in_pu_list->editItemAt(1);  /* u */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, downsampling_u, 0);
        in_pu = in_pu_list->editItemAt(2);  /* v */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, downsampling_v, 0);
        ExynosVpuPu::connect(downsampling_u, 0, dma_out_u, 0);
        ExynosVpuPu::connect(downsampling_v, 0, dma_out_v, 0);

        ret_out_pu_list->push_back(make_pair(dma_out_y, 0));
        ret_out_pu_list->push_back(make_pair(dma_out_u, 0));
        ret_out_pu_list->push_back(make_pair(dma_out_v, 0));
    } else if (m_dst_format==VX_DF_IMAGE_YUV4) {
        if (m_dst_io_num != 3) {
            VXLOGE("dest io number is invalid, format:0x%x, io_num:%d", m_dst_format, m_dst_io_num);
            return VX_FAILURE;
        }

        ExynosVpuPu *dma_out_y = pu_factory.createPu(subchain, VPU_PU_DMAOT0);
        dma_out_y->setSize(iosize, iosize);
        ExynosVpuPu *dma_out_u = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE0);
        dma_out_u->setSize(iosize, iosize);
        ExynosVpuPu *dma_out_v = pu_factory.createPu(subchain, VPU_PU_DMAOT_WIDE1);
        dma_out_v->setSize(iosize, iosize);

        in_pu = in_pu_list->editItemAt(0);  /* y */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out_y, 0);
        in_pu = in_pu_list->editItemAt(1);  /* u */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out_u, 0);
        in_pu = in_pu_list->editItemAt(2);  /* v */
        ExynosVpuPu::connect(in_pu.first, in_pu.second, dma_out_v, 0);

        ret_out_pu_list->push_back(make_pair(dma_out_y, 0));
        ret_out_pu_list->push_back(make_pair(dma_out_u, 0));
        ret_out_pu_list->push_back(make_pair(dma_out_v, 0));
    } else {
        VXLOGE("un-supported input format, 0x%x", m_dst_format);
        return VX_FAILURE;
    }

    return VX_SUCCESS;
}

}; /* namespace android */

