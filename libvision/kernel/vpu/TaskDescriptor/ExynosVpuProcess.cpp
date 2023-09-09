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

#define LOG_TAG "ExynosVpuVertex"
#include <cutils/log.h>

#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

static uint32_t getVertexIndexInTask(uint8_t *base, uint8_t *vertex)
{
    struct vpul_task *task = (struct vpul_task*)base;

    int32_t remained_byte = vertex - base;
    remained_byte -= task->vertices_vec_ofs;

    uint32_t i;
    for (i=0; i<task->t_num_of_vertices; i++) {
        if (remained_byte == 0) {
            break;
        }

        remained_byte -= sizeof(struct vpul_vertex);
    }

    if (i == task->t_num_of_vertices) {
        VXLOGE("can't find vertex index");
    }

    return i;
}

void
ExynosVpuIoDynamicMapRoi::display_structure_info(uint32_t tab_num, struct vpul_dynamic_map_roi *dyn_roi)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(dynmap) memmap_idx:%d", MAKE_TAB(tap, tab_num),
                                                                    dyn_roi->memmap_idx);
}

void
ExynosVpuIoDynamicMapRoi::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(dynmap) memmap_idx:%d", MAKE_TAB(tap, tab_num),
                                                                    m_memmap ? m_memmap->getIndex() : 0xFF);
}

ExynosVpuIoDynamicMapRoi::ExynosVpuIoDynamicMapRoi(ExynosVpuProcess *process)
                                               : ExynosVpuIoMapRoi(MAP_ROI_TYPE_DYNAMIC)
{
    m_process = process;
    memset(&m_dynamic_map_roi_info, 0x0, sizeof(m_dynamic_map_roi_info));

    m_maproi_index_in_process = process->addDynamicMapRoi(this);
    m_memmap = NULL;
}

ExynosVpuIoDynamicMapRoi::ExynosVpuIoDynamicMapRoi(ExynosVpuProcess *process, const struct vpul_dynamic_map_roi *dynamic_map_info)
                                               : ExynosVpuIoMapRoi(MAP_ROI_TYPE_DYNAMIC)
{
    m_process = process;
    m_dynamic_map_roi_info = *dynamic_map_info;

    m_maproi_index_in_process = process->addDynamicMapRoi(this);
    m_memmap = NULL;
}

status_t
ExynosVpuIoDynamicMapRoi::updateStructure(void)
{
    m_dynamic_map_roi_info.memmap_idx = m_memmap->getIndex();

    return NO_ERROR;
}

status_t
ExynosVpuIoDynamicMapRoi::updateObject(void)
{
    uint32_t task_memmap_idx = m_dynamic_map_roi_info.memmap_idx;
    m_memmap = m_process->getTask()->getMemmap(task_memmap_idx);
    if (m_memmap == NULL) {
        VXLOGE("getting memmap fails");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoDynamicMapRoi::checkConstraint(void)
{
    if (m_memmap == NULL) {
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuIoFixedMapRoi::display_structure_info(uint32_t tab_num, struct vpul_fixed_map_roi *fixed_roi)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(fixmap) memmap_idx:%d, use_roi:%d, (%d,%d,%d,%d)", MAKE_TAB(tap, tab_num),
                                                                    fixed_roi->memmap_idx,
                                                                    fixed_roi->use_roi,
                                                                    fixed_roi->roi.first_col,
                                                                    fixed_roi->roi.first_line,
                                                                    fixed_roi->roi.width,
                                                                    fixed_roi->roi.height);
}

void
ExynosVpuIoFixedMapRoi::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(fixmap) memmap_idx:%d, use_roi:%d, (%d,%d,%d,%d)", MAKE_TAB(tap, tab_num),
                                                                    m_memmap ? m_memmap->getIndex() : 0xFF,
                                                                    m_fixed_map_roi_info.use_roi,
                                                                    m_fixed_map_roi_info.roi.first_col,
                                                                    m_fixed_map_roi_info.roi.first_line,
                                                                    m_fixed_map_roi_info.roi.width,
                                                                    m_fixed_map_roi_info.roi.height);
}

ExynosVpuIoFixedMapRoi::ExynosVpuIoFixedMapRoi(ExynosVpuProcess *process, ExynosVpuMemmap *memmap)
                                               : ExynosVpuIoMapRoi(MAP_ROI_TYPE_FIXED)
{
    m_process = process;
    memset(&m_fixed_map_roi_info, 0x0, sizeof(m_fixed_map_roi_info));

    m_maproi_index_in_process = process->addFixedMapRoi(this);
    m_memmap = memmap;
}

ExynosVpuIoFixedMapRoi::ExynosVpuIoFixedMapRoi(ExynosVpuProcess *process, const struct vpul_fixed_map_roi *fixed_map_info)
                                               : ExynosVpuIoMapRoi(MAP_ROI_TYPE_FIXED)
{
    m_process = process;

    m_fixed_map_roi_info = *fixed_map_info;
    if (m_fixed_map_roi_info.use_roi == 0)
        memset(&m_fixed_map_roi_info.roi, 0x0, sizeof(m_fixed_map_roi_info.roi));

    m_maproi_index_in_process = process->addFixedMapRoi(this);
    m_memmap = NULL;
}

status_t
ExynosVpuIoFixedMapRoi::updateStructure(void)
{
    m_fixed_map_roi_info.memmap_idx = m_memmap->getIndex();

    return NO_ERROR;
}

status_t
ExynosVpuIoFixedMapRoi::updateObject(void)
{
    uint32_t task_memmap_idx = m_fixed_map_roi_info.memmap_idx;
    m_memmap = m_process->getTask()->getMemmap(task_memmap_idx);
    if (m_memmap == NULL) {
        VXLOGE("getting memmap fails");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoFixedMapRoi::checkConstraint(void)
{
    if (m_memmap == NULL) {
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuIoTypesDesc::display_structure_info(uint32_t tab_num, struct vpul_iotypes_desc *iotype)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(iotype) is_dynamic:%d, roi_index:%d", MAKE_TAB(tap, tab_num),
                                                                    iotype->is_dynamic, iotype->roi_index);
}

void
ExynosVpuIoTypesDesc::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(iotype) is_dynamic:%d, roi_index:%d", MAKE_TAB(tap, tab_num),
                                                                    m_iotypes_info.is_dynamic, m_map_roi ? m_map_roi->getIndex() : 0xFF);
}

ExynosVpuIoTypesDesc::ExynosVpuIoTypesDesc(ExynosVpuProcess *process, ExynosVpuIoDynamicMapRoi *dyn_roi)
{
    m_process = process;
    memset(&m_iotypes_info, 0x0, sizeof(m_iotypes_info));
    m_iotypes_info.is_dynamic = 1;

    m_iotypes_index_in_process = process->addIotypes(this);
    m_map_roi = dyn_roi;
}

ExynosVpuIoTypesDesc::ExynosVpuIoTypesDesc(ExynosVpuProcess *process, ExynosVpuIoFixedMapRoi *fixed_roi)
{
    m_process = process;
    memset(&m_iotypes_info, 0x0, sizeof(m_iotypes_info));
    m_iotypes_info.is_dynamic = 0;

    m_iotypes_index_in_process = process->addIotypes(this);
    m_map_roi = fixed_roi;
}

ExynosVpuIoTypesDesc::ExynosVpuIoTypesDesc(ExynosVpuProcess *process, const struct vpul_iotypes_desc *iotypes_info)
{
    m_process = process;
    m_iotypes_info = *iotypes_info;

    m_iotypes_index_in_process = process->addIotypes(this);
    m_map_roi = NULL;
}

status_t
ExynosVpuIoTypesDesc::updateStructure(void)
{
    m_iotypes_info.roi_index = m_map_roi->getIndex();

    return NO_ERROR;
}

status_t
ExynosVpuIoTypesDesc::updateObject(void)
{
    if (m_iotypes_info.is_dynamic==1) {
        m_map_roi = m_process->getDynamicMapRoi(m_iotypes_info.roi_index);
    } else {
        m_map_roi = m_process->getFixedMapRoi(m_iotypes_info.roi_index);
    }

    if (m_map_roi == NULL) {
        VXLOGE("getting map_roi fails");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoTypesDesc::checkConstraint(void)
{
    if (m_map_roi == NULL) {
        displayObjectInfo();
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

void
ExynosVpuIoSize::display_structure_info(uint32_t tab_num, struct vpul_sizes *size)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(size) type:%d, op_idx:%d, src_idx:%d", MAKE_TAB(tap, tab_num),
                                                                    size->type, size->op_ind, size->src_idx);
}

void
ExynosVpuIoSize::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];
    VXLOGI("%s(size) type:%d, op_idx:%d, src_idx:%d", MAKE_TAB(tap, tab_num),
                                                                    m_size_info.type, m_size_info.op_ind, m_src_size ? m_src_size->getIndex() : m_size_index_in_process);
}

ExynosVpuIoSize::ExynosVpuIoSize(ExynosVpuProcess *process, enum vpul_sizes_op_type type, ExynosVpuIoSize *src_size)
{
    m_process = process;
    memset(&m_size_info, 0x0, sizeof(m_size_info));
    m_size_info.type = type;

    m_size_index_in_process = process->addSize(this);
    m_src_size = src_size;

    m_origin_width = 0;
    m_origin_height = 0;
}

ExynosVpuIoSize::ExynosVpuIoSize(ExynosVpuProcess *process, const struct vpul_sizes *size_info)
{
    m_process = process;
    m_size_info = *size_info;

    m_size_index_in_process = process->addSize(this);
    m_src_size = NULL;

    m_origin_width = 0;
    m_origin_height = 0;
    m_origin_roi_width = 0;
    m_origin_roi_height = 0;
}

bool
ExynosVpuIoSize::isAssignDimension(void)
{
    if ((m_origin_width !=0) && (m_origin_height != 0))
        return true;

    if (m_src_size == NULL)
        return false;

    return m_src_size->isAssignDimension();
}

status_t
ExynosVpuIoSize::setOriginDimension(uint32_t width, uint32_t height, uint32_t roi_width, uint32_t roi_height)
{
    VXLOGE("type:0x%x can't be set as ancestor size, %d, %d, %d, %d", m_size_info.type, width, height, roi_width, roi_height);

    return INVALID_OPERATION;
}

status_t
ExynosVpuIoSize::checkConstraint(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoSize::updateStructure(void)
{
    if (m_src_size) {
        m_size_info.src_idx = m_src_size->getIndex();
    } else {
        m_size_info.src_idx =m_size_index_in_process;
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoSize::updateObject(void)
{
    if (m_size_index_in_process == m_size_info.src_idx) {
        /* acenstor size */
        m_src_size = NULL;
    } else {
        m_src_size = m_process->getIoSize(m_size_info.src_idx);
        if (m_src_size == NULL) {
            VXLOGE("size object isn't exist, index:%d", m_size_info.src_idx);
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

ExynosVpuIoSizeInout::ExynosVpuIoSizeInout(ExynosVpuProcess *process, ExynosVpuIoSize *src_size)
                                                                            :ExynosVpuIoSize(process, VPUL_SIZEOP_INOUT, src_size)
{

}

ExynosVpuIoSizeInout::ExynosVpuIoSizeInout(ExynosVpuProcess *process, const struct vpul_sizes *size_info)
                                                                            :ExynosVpuIoSize(process, size_info)
{

}

status_t
ExynosVpuIoSizeInout::setOriginDimension(uint32_t width, uint32_t height, uint32_t roi_width, uint32_t roi_height)
{
    if (m_src_size == NULL) {
        if ((m_origin_width==0) && (m_origin_height==0)) {
            m_origin_width = width;
            m_origin_height = height;
            m_origin_roi_width = roi_width;
            m_origin_roi_height = roi_height;
        } else {
            if ((m_origin_width != width) || (m_origin_height != height)) {
                VXLOGE("origin dimensino conflict, (%d,%d) (%d,%d)", m_origin_width, m_origin_height, width, height);
                return INVALID_OPERATION;
            }
        }

        return NO_ERROR;
    } else {
        VXLOGE("only ancestor size could be set origin size");

        return INVALID_OPERATION;
    }
}

status_t
ExynosVpuIoSizeInout::getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height)
{
    if (m_src_size == NULL) {
        /* ancestor size */
        if (m_origin_width && m_origin_height) {
            *ret_width = m_origin_width;
            *ret_height = m_origin_height;
            *ret_roi_width = m_origin_roi_width;
            *ret_roi_height = m_origin_roi_height;

            VXLOGD2("(size_%d) origin -> (%d,%d,%d,%d)", m_size_index_in_process, *ret_width, *ret_height, *ret_roi_width, *ret_roi_height);

            return NO_ERROR;
        } else {
            *ret_width = 0;
            *ret_height = 0;
            *ret_roi_width = 0;
            *ret_roi_height = 0;

            VXLOGE("ancestor size is not set");
            return INVALID_OPERATION;
        }
    } else {
        status_t ret;
        ret = m_src_size->getDimension(ret_width, ret_height, ret_roi_width, ret_roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("getting dimension from ancestor fails");
            return ret;
        }

        /* the current size is equal to previous roi */
        if (*ret_roi_width && *ret_roi_height) {
            *ret_width = *ret_roi_width;
            *ret_height = *ret_roi_height;
            *ret_roi_width = 0;
            *ret_roi_height = 0;
        }
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoSizeInout::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("update size structure fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

status_t
ExynosVpuIoSizeInout::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("update size object fails");
        goto EXIT;
    }

EXIT:
    return ret;
}

ExynosVpuIoSizeFix::ExynosVpuIoSizeFix(ExynosVpuProcess *process)
                                                                        :ExynosVpuIoSize(process, VPUL_SIZEOP_FIX, NULL)
{

}

ExynosVpuIoSizeFix::ExynosVpuIoSizeFix(ExynosVpuProcess *process, const struct vpul_sizes *size_info)
                                                                        :ExynosVpuIoSize(process, size_info)
{

}

status_t
ExynosVpuIoSizeFix::getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height)
{
    VXLOGE("not implemented yet, %d, %d, %d, %d", *ret_width, *ret_height, *ret_roi_width, *ret_roi_height);

    return INVALID_OPERATION;
}

ExynosVpuIoSizeCrop::ExynosVpuIoSizeCrop(ExynosVpuProcess *process, ExynosVpuIoSize *src_size, ExynosVpuIoSizeOpCropper *op_crop)
                                                                            :ExynosVpuIoSize(process, VPUL_SIZEOP_FORCE_CROP, src_size)
{
    m_op_crop = op_crop;
}

ExynosVpuIoSizeCrop::ExynosVpuIoSizeCrop(ExynosVpuProcess *process, const struct vpul_sizes *size_info)
                                                                            :ExynosVpuIoSize(process, size_info)
{

}

status_t
ExynosVpuIoSizeCrop::getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height)
{
    if (m_src_size == NULL) {
        /* ancestor size */
        VXLOGE("ancestor size is not set");
        return INVALID_OPERATION;
    } else {
        status_t ret;
        ret = m_src_size->getDimension(ret_width, ret_height, ret_roi_width, ret_roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("getting dimension from ancestor fails");
            return ret;
        }

        /* the current size is equal to previous roi */
        if (*ret_roi_width && *ret_roi_height) {
            *ret_width = *ret_roi_width;
            *ret_height = *ret_roi_height;
            *ret_roi_width = 0;
            *ret_roi_height = 0;
        }

        struct vpul_croppers* crop_info = m_op_crop->getCropperInfo();
        if ((*ret_width < (crop_info->Left + crop_info->Right)) || (*ret_height < (crop_info->Top+crop_info->Bottom))) {
            VXLOGE("crop size is not valid, (%d, %d), (%d, %d, %d, %d)", *ret_width, *ret_height,
                                crop_info->Left, crop_info->Right, crop_info->Top, crop_info->Bottom);
            return INVALID_OPERATION;
        }

        *ret_width -= crop_info->Left;
        *ret_width -= crop_info->Right;
        *ret_height -= crop_info->Top;
        *ret_height -= crop_info->Bottom;

        VXLOGD2("(size_%d) crop -> (%d,%d,%d,%d)", m_size_index_in_process, *ret_width, *ret_height, *ret_roi_width, *ret_roi_height);
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoSizeCrop::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating size structure fails");
        return INVALID_OPERATION;
    }

    if (m_op_crop==NULL) {
        VXLOGE("crop object isn't assigned");
        return INVALID_OPERATION;
    }

    m_size_info.op_ind = m_op_crop->getIndex();

    return NO_ERROR;
}

status_t
ExynosVpuIoSizeCrop::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("updating size object fails");
        return INVALID_OPERATION;
    }

    m_op_crop = m_process->getIoSizeOpCropper(m_size_info.op_ind);
    if (m_op_crop==NULL) {
        VXLOGE("crop object isn't exist, index:%d", m_size_info.op_ind);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

ExynosVpuIoSizeScale::ExynosVpuIoSizeScale(ExynosVpuProcess *process, ExynosVpuIoSize *src_size, ExynosVpuIoSizeOpScale *op_scale)
                                                                            :ExynosVpuIoSize(process, VPUL_SIZEOP_SCALE, src_size)
{
    m_op_scale= op_scale;
}

ExynosVpuIoSizeScale::ExynosVpuIoSizeScale(ExynosVpuProcess *process, const struct vpul_sizes *size_info)
                                                                            :ExynosVpuIoSize(process, size_info)
{

}

ExynosVpuIoSizeOpScale*
ExynosVpuIoSizeScale::getSizeOpScale(void)
{
    return m_op_scale;
}

status_t
ExynosVpuIoSizeScale::getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height)
{
    if (m_src_size == NULL) {
        /* ancestor size */
        VXLOGE("ancestor size is not set");
        return INVALID_OPERATION;
    } else {
        status_t ret;
        ret = m_src_size->getDimension(ret_width, ret_height, ret_roi_width, ret_roi_height);
        if (ret != NO_ERROR) {
            VXLOGE("getting dimension from ancestor fails");
            return ret;
        }

        /* the current size is equal to previous roi */
        if (*ret_roi_width && *ret_roi_height) {
            *ret_width = *ret_roi_width;
            *ret_height = *ret_roi_height;
            *ret_roi_width = 0;
            *ret_roi_height = 0;
        }

        struct vpul_scales* scale_info = m_op_scale->getScaleInfo();

        *ret_width = ceilf((float)*ret_width * ((float)scale_info->horizontal.numerator / (float)scale_info->horizontal.denominator));
        *ret_height = ceilf((float)*ret_height * ((float)scale_info->vertical.numerator / (float)scale_info->vertical.denominator));

        VXLOGD2("(size_%d) scale  -> (%d,%d,%d,%d)", m_size_index_in_process, *ret_width, *ret_height, *ret_roi_width, *ret_roi_height);
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoSizeScale::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("updating size object fails");
        return INVALID_OPERATION;
    }

    m_op_scale = m_process->getIoSizeOpScale(m_size_info.op_ind);
    if (m_op_scale==NULL) {
        VXLOGE("scale object isn't exist, index:%d", m_size_info.op_ind);
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t
ExynosVpuIoSizeScale::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuIoSize::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("updating size structure fails");
        return INVALID_OPERATION;
    }

    if (m_op_scale==NULL) {
        VXLOGE("scale object isn't assigned");
        return INVALID_OPERATION;
    }

    m_size_info.op_ind = m_op_scale->getIndex();

    return NO_ERROR;
}

void
ExynosVpuIoSizeOpScale::display_structure_info(uint32_t tab_num, struct vpul_scales *scale)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(opscale) horizontal:%d/%d, vertical:%d/%d", MAKE_TAB(tap, tab_num),
                                                                    scale->horizontal.numerator, scale->horizontal.denominator,
                                                                    scale->vertical.numerator, scale->vertical.denominator);
}

void
ExynosVpuIoSizeOpScale::displayObjectInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(opscale) horizontal:%d/%d, vertical:%d/%d", MAKE_TAB(tap, tab_num),
                                                                    m_scales_info.horizontal.numerator, m_scales_info.horizontal.denominator,
                                                                    m_scales_info.vertical.numerator, m_scales_info.vertical.denominator);
}

ExynosVpuIoSizeOpScale::ExynosVpuIoSizeOpScale(ExynosVpuProcess *process)
{
    m_process = process;
    memset(&m_scales_info, 0x0, sizeof(m_scales_info));

    m_sizeop_index_in_process = process->addSizeOpScale(this);
}

ExynosVpuIoSizeOpScale::ExynosVpuIoSizeOpScale(ExynosVpuProcess *process, const struct vpul_scales *scales_info)
{
    m_process = process;
    m_scales_info = *scales_info;

    m_sizeop_index_in_process = process->addSizeOpScale(this);
}

status_t
ExynosVpuIoSizeOpScale::updateStructure(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoSizeOpScale::updateObject(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoSizeOpScale::checkConstraint(void)
{
    return NO_ERROR;
}

void
ExynosVpuIoSizeOpCropper::display_structure_info(uint32_t tab_num, struct vpul_croppers *cropper)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(opcrop) left:%d, right:%d, top:%d, bottom;%d", MAKE_TAB(tap, tab_num),
                                                                    cropper->Left, cropper->Right, cropper->Top, cropper->Bottom);
}

void
ExynosVpuIoSizeOpCropper::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuIoSizeOpCropper::display_structure_info(tab_num, &m_cropper_info);
}

ExynosVpuIoSizeOpCropper::ExynosVpuIoSizeOpCropper(ExynosVpuProcess *process)
{
    m_process = process;
    memset(&m_cropper_info, 0x0, sizeof(m_cropper_info));

    m_sizeop_index_in_process = process->addSizeOpCropper(this);
}

ExynosVpuIoSizeOpCropper::ExynosVpuIoSizeOpCropper(ExynosVpuProcess *process, const struct vpul_croppers *cropper_info)
{
    m_process = process;
    m_cropper_info = *cropper_info;

    m_sizeop_index_in_process = process->addSizeOpCropper(this);
}

status_t
ExynosVpuIoSizeOpCropper::updateStructure(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoSizeOpCropper::updateObject(void)
{
    return NO_ERROR;
}

status_t
ExynosVpuIoSizeOpCropper::checkConstraint(void)
{
    return NO_ERROR;
}

void
ExynosVpuProcess::display_structure_info(uint32_t tab_num, struct vpul_process *process)
{
    char tap[MAX_TAB_NUM];

    uint32_t i;

    VXLOGI("%s(Proc) n_dynamic_map_rois: %d", MAKE_TAB(tap, tab_num), process->io.n_dynamic_map_rois);
    struct vpul_dynamic_map_roi *dynamic_map_roi;
    for (i=0; i<process->io.n_dynamic_map_rois; i++) {
        dynamic_map_roi = &process->io.dynamic_map_roi[i];
        ExynosVpuIoDynamicMapRoi::display_structure_info(tab_num+1, dynamic_map_roi);
    }

    VXLOGI("%s(Proc) n_fixed_map_roi: %d", MAKE_TAB(tap, tab_num), process->io.n_fixed_map_roi);
    struct vpul_fixed_map_roi *fixed_map_roi;
    for (i=0; i<process->io.n_fixed_map_roi; i++) {
        fixed_map_roi = &process->io.fixed_map_roi[i];
        ExynosVpuIoFixedMapRoi::display_structure_info(tab_num+1, fixed_map_roi);
    }

    VXLOGI("%s(Proc) n_inout_types: %d", MAKE_TAB(tap, tab_num), process->io.n_inout_types);
    struct vpul_iotypes_desc *inout_type;
    for (i=0; i<process->io.n_inout_types; i++) {
        inout_type = &process->io.inout_types[i];
        ExynosVpuIoTypesDesc::display_structure_info(tab_num+1, inout_type);
    }

    VXLOGI("%s(Proc) n_sizes_op: %d", MAKE_TAB(tap, tab_num), process->io.n_sizes_op);
    struct vpul_sizes *size;
    for (i=0; i<process->io.n_sizes_op; i++) {
        size = &process->io.sizes[i];
        ExynosVpuIoSize::display_structure_info(tab_num+1, size);
    }

    VXLOGI("%s(Proc) n_scales: %d", MAKE_TAB(tap, tab_num), process->io.n_scales);
    struct vpul_scales *scales;
    for (i=0; i<process->io.n_scales; i++) {
        scales = &process->io.scales[i];
        ExynosVpuIoSizeOpScale::display_structure_info(tab_num+1, scales);
    }

    VXLOGI("%s(Proc) n_croppers: %d", MAKE_TAB(tap, tab_num), process->io.n_croppers);
    struct vpul_croppers *croppers;
    for (i=0; i<process->io.n_croppers; i++) {
        croppers = &process->io.croppers[i];
        ExynosVpuIoSizeOpCropper::display_structure_info(tab_num+1, croppers);
    }

    VXLOGI("%s(Proc) n_static_coff: %d", MAKE_TAB(tap, tab_num), process->io.n_static_coff);
    for (i=0; i<process->io.n_static_coff; i+=5) {
        VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.static_coff[i+0], process->io.static_coff[i+1], process->io.static_coff[i+2], process->io.static_coff[i+3], process->io.static_coff[i+4]);
    }

    VXLOGI("%s(Proc) n_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_insets);
    VXLOGI("%s(Proc) n_presets_map_desc: %d", MAKE_TAB(tap, tab_num), process->io.n_presets_map_desc);
    VXLOGI("%s(Proc) n_postsets_map_desc: %d", MAKE_TAB(tap, tab_num), process->io.n_postsets_map_desc);

    VXLOGI("%s(Proc) n_subchain_before_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_subchain_before_insets);
    VXLOGI("%s(Proc) n_subchain_after_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_subchain_after_insets);
    VXLOGI("%s(Proc) n_invocations_per_input_tile: %d", MAKE_TAB(tap, tab_num), process->io.n_invocations_per_input_tile);

    VXLOGI("%s(Proc) max_input_sets_slots: %d", MAKE_TAB(tap, tab_num), process->io.max_input_sets_slots);

    VXLOGI("%s(Proc) pts_maps_sets: %d", MAKE_TAB(tap, tab_num), process->io.pts_maps_sets);
    VXLOGI("%s(Proc) pts_maps_each_set: %d", MAKE_TAB(tap, tab_num), process->io.pts_maps_each_set);
    VXLOGI("%s(Proc) n_map_indices: %d", MAKE_TAB(tap, tab_num), process->io.n_map_indices);
    VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.pts_map_indices[0],
                                                                                                    process->io.pts_map_indices[1],
                                                                                                    process->io.pts_map_indices[2],
                                                                                                    process->io.pts_map_indices[3],
                                                                                                    process->io.pts_map_indices[4]);
    VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.pts_map_indices[5],
                                                                                                    process->io.pts_map_indices[6],
                                                                                                    process->io.pts_map_indices[7],
                                                                                                    process->io.pts_map_indices[8],
                                                                                                    process->io.pts_map_indices[9]);
    VXLOGI("%s(Proc) pt_list_num: %d", MAKE_TAB(tap, tab_num), process->io.pt_list_num);
    VXLOGI("%s(Proc) pt_roi_sizes: %d", MAKE_TAB(tap, tab_num), process->io.pt_roi_sizes);
    VXLOGI("%s(Proc) n_roi: %d", MAKE_TAB(tap, tab_num), process->io.roi_sizes_desc.n_roi);
    for (i=0; i<process->io.roi_sizes_desc.n_roi; i++) {
        VXLOGI("%sroi_width%d, roi_height:%d", MAKE_TAB(tap, tab_num+1), process->io.roi_sizes_desc.roi_desc[i].roi_width,
                                                                                                            process->io.roi_sizes_desc.roi_desc[i].roi_height);
    }
    VXLOGI("%s(Proc) n_dyn_points: %d", MAKE_TAB(tap, tab_num), process->io.n_dyn_points);
}

void
ExynosVpuProcess::displayProcessInfo(uint32_t tab_num)
{
    char tap[MAX_TAB_NUM];

    uint32_t i;

    VXLOGI("%s(Proc) n_dynamic_map_rois: %d", MAKE_TAB(tap, tab_num), m_io_dynamic_roi_list.size());
    Vector<ExynosVpuIoDynamicMapRoi*>::iterator dynamic_iter;
    for (dynamic_iter=m_io_dynamic_roi_list.begin(); dynamic_iter!=m_io_dynamic_roi_list.end(); dynamic_iter++) {
        (*dynamic_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Proc) n_fixed_map_roi: %d", MAKE_TAB(tap, tab_num), m_io_fixed_roi_list.size());
    Vector<ExynosVpuIoFixedMapRoi*>::iterator fixed_iter;
    for (fixed_iter=m_io_fixed_roi_list.begin(); fixed_iter!=m_io_fixed_roi_list.end(); fixed_iter++) {
        (*fixed_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Proc) n_inout_types: %d", MAKE_TAB(tap, tab_num), m_io_iotypes_list.size());
    Vector<ExynosVpuIoTypesDesc*>::iterator iotype_iter;
    for (iotype_iter=m_io_iotypes_list.begin(); iotype_iter!=m_io_iotypes_list.end(); iotype_iter++) {
        (*iotype_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Proc) n_sizes_op: %d", MAKE_TAB(tap, tab_num), m_io_size_list.size());
    Vector<ExynosVpuIoSize*>::iterator size_iter;
    for (size_iter=m_io_size_list.begin(); size_iter!=m_io_size_list.end(); size_iter++) {
        (*size_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Proc) n_scales: %d", MAKE_TAB(tap, tab_num), m_io_sizeop_scale_list.size());
    Vector<ExynosVpuIoSizeOpScale*>::iterator scale_iter;
    for (scale_iter=m_io_sizeop_scale_list.begin(); scale_iter!=m_io_sizeop_scale_list.end(); scale_iter++) {
        (*scale_iter)->displayObjectInfo(tab_num+1);
    }

    VXLOGI("%s(Proc) n_croppers: %d", MAKE_TAB(tap, tab_num), m_io_sizeop_cropper_list.size());
    Vector<ExynosVpuIoSizeOpCropper*>::iterator cropper_iter;
    for (cropper_iter=m_io_sizeop_cropper_list.begin(); cropper_iter!=m_io_sizeop_cropper_list.end(); cropper_iter++) {
        (*cropper_iter)->displayObjectInfo(tab_num+1);
    }

    struct vpul_process *process = getProcessInfo();
    VXLOGI("%s(Proc) n_static_coff: %d", MAKE_TAB(tap, tab_num), process->io.n_static_coff);
    for (i=0; i<process->io.n_static_coff; i+=5) {
        VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.static_coff[i+0], process->io.static_coff[i+1], process->io.static_coff[i+2], process->io.static_coff[i+3], process->io.static_coff[i+4]);
    }

    VXLOGI("%s(Proc) n_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_insets);
    VXLOGI("%s(Proc) n_presets_map_desc: %d", MAKE_TAB(tap, tab_num), process->io.n_presets_map_desc);
    VXLOGI("%s(Proc) n_postsets_map_desc: %d", MAKE_TAB(tap, tab_num), process->io.n_postsets_map_desc);

    VXLOGI("%s(Proc) n_subchain_before_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_subchain_before_insets);
    VXLOGI("%s(Proc) n_subchain_after_insets: %d", MAKE_TAB(tap, tab_num), process->io.n_subchain_after_insets);
    VXLOGI("%s(Proc) n_invocations_per_input_tile: %d", MAKE_TAB(tap, tab_num), process->io.n_invocations_per_input_tile);

    VXLOGI("%s(Proc) max_input_sets_slots: %d", MAKE_TAB(tap, tab_num), process->io.max_input_sets_slots);

    VXLOGI("%s(Proc) pts_maps_sets: %d", MAKE_TAB(tap, tab_num), process->io.pts_maps_sets);
    VXLOGI("%s(Proc) pts_maps_each_set: %d", MAKE_TAB(tap, tab_num), process->io.pts_maps_each_set);
    VXLOGI("%s(Proc) n_map_indices: %d", MAKE_TAB(tap, tab_num), process->io.n_map_indices);
    if (process->io.n_map_indices) {
        VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.pts_map_indices[0],
                                                                                                        process->io.pts_map_indices[1],
                                                                                                        process->io.pts_map_indices[2],
                                                                                                        process->io.pts_map_indices[3],
                                                                                                        process->io.pts_map_indices[4]);
        VXLOGI("%s%d %d %d %d %d", MAKE_TAB(tap, tab_num+1), process->io.pts_map_indices[5],
                                                                                                        process->io.pts_map_indices[6],
                                                                                                        process->io.pts_map_indices[7],
                                                                                                        process->io.pts_map_indices[8],
                                                                                                        process->io.pts_map_indices[9]);
    }
    VXLOGI("%s(Proc) pt_list_num: %d", MAKE_TAB(tap, tab_num), process->io.pt_list_num);
    VXLOGI("%s(Proc) pt_roi_sizes: %d", MAKE_TAB(tap, tab_num), process->io.pt_roi_sizes);
    VXLOGI("%s(Proc) n_roi: %d", MAKE_TAB(tap, tab_num), process->io.roi_sizes_desc.n_roi);
    for (i=0; i<process->io.roi_sizes_desc.n_roi; i++) {
        VXLOGI("%sroi_width%d, roi_height:%d", MAKE_TAB(tap, tab_num+1), process->io.roi_sizes_desc.roi_desc[i].roi_width,
                                                                                                            process->io.roi_sizes_desc.roi_desc[i].roi_height);
    }
    VXLOGI("%s(Proc) n_dyn_points: %d", MAKE_TAB(tap, tab_num), process->io.n_dyn_points);
}

ExynosVpuProcess::ExynosVpuProcess(ExynosVpuTask *task)
                                                                : ExynosVpuVertex(task, VPUL_VERTEXT_PROC)
{
    struct vpul_process *process_info;
    process_info = &m_vertex_info.proc;

    /* JUN_TBD */
    process_info->io.n_insets = 1;
    process_info->io.n_invocations_per_input_tile = 1;
}

ExynosVpuProcess::ExynosVpuProcess(ExynosVpuTask *task, const struct vpul_vertex *vertex_info)
                                                                : ExynosVpuVertex(task, vertex_info)
{
    const struct vpul_process *process_info;
    process_info = &vertex_info->proc;

    uint32_t i;

    ExynosVpuIoDynamicMapRoi *dyn_roi_object;
    const struct vpul_dynamic_map_roi *dyn_roi_info;
    for (i=0; i<process_info->io.n_dynamic_map_rois; i++) {
        dyn_roi_info = &process_info->io.dynamic_map_roi[i];
        dyn_roi_object = new ExynosVpuIoDynamicMapRoi(this, dyn_roi_info);
    }

    ExynosVpuIoFixedMapRoi *fixed_roi_object;
    const struct vpul_fixed_map_roi *fixed_roi_info;
    for (i=0; i<process_info->io.n_fixed_map_roi; i++) {
        fixed_roi_info = &process_info->io.fixed_map_roi[i];
        fixed_roi_object = new ExynosVpuIoFixedMapRoi(this, fixed_roi_info);
    }

    ExynosVpuIoTypesDesc *iotype_object;
    const struct vpul_iotypes_desc *iotype_info;
    for (i=0; i<process_info->io.n_inout_types; i++) {
        iotype_info = &process_info->io.inout_types[i];
        iotype_object = new ExynosVpuIoTypesDesc(this, iotype_info);
    }

    ExynosVpuIoSize *size_object;
    const struct vpul_sizes *size;
    for (i=0; i<process_info->io.n_sizes_op; i++) {
        size = &process_info->io.sizes[i];
        switch (size->type) {
        case VPUL_SIZEOP_INOUT:
            size_object = new ExynosVpuIoSizeInout(this, size);
            break;
        case VPUL_SIZEOP_FIX:
            size_object = new ExynosVpuIoSizeFix(this, size);
            break;
        case VPUL_SIZEOP_FORCE_CROP:
            size_object = new ExynosVpuIoSizeCrop(this, size);
            break;
        case VPUL_SIZEOP_SCALE:
            size_object = new ExynosVpuIoSizeScale(this, size);
            break;
        default:
            VXLOGE("un-supported type:0x%x", size->type);
            break;
        }
    }

    ExynosVpuIoSizeOpScale *scale_object;
    const struct vpul_scales *scale_info;
    for (i=0; i<process_info->io.n_scales; i++) {
        scale_info = &process_info->io.scales[i];
        scale_object = new ExynosVpuIoSizeOpScale(this, scale_info);
    }

    ExynosVpuIoSizeOpCropper *cropper_object;
    const struct vpul_croppers *cropper_info;
    for (i=0; i<process_info->io.n_croppers; i++) {
        cropper_info = &process_info->io.croppers[i];
        cropper_object = new ExynosVpuIoSizeOpCropper(this, cropper_info);
    }

    /* JUN_TBD, update struct vpul_process */
    struct vpul_process *process_info_modify;
    process_info_modify = &m_vertex_info.proc;
    process_info_modify->io.n_insets = 1;
    process_info_modify->io.n_invocations_per_input_tile = 1;
}

ExynosVpuProcess::~ExynosVpuProcess()
{
    uint32_t i;

    for (i=0; i<m_io_size_list.size(); i++)
        delete m_io_size_list[i];
}

uint32_t
ExynosVpuProcess::addDynamicMapRoi(ExynosVpuIoDynamicMapRoi *dynamic_map_roi)
{
    m_io_dynamic_roi_list.push_back(dynamic_map_roi);

    return m_io_dynamic_roi_list.size() - 1;
}

uint32_t
ExynosVpuProcess::addFixedMapRoi(ExynosVpuIoFixedMapRoi *fixed_map_roi)
{
    m_io_fixed_roi_list.push_back(fixed_map_roi);

    return m_io_fixed_roi_list.size() - 1;
}

uint32_t
ExynosVpuProcess::addIotypes(ExynosVpuIoTypesDesc *iotypes)
{
    m_io_iotypes_list.push_back(iotypes);

    return m_io_iotypes_list.size() - 1;
}

uint32_t
ExynosVpuProcess::addSize(ExynosVpuIoSize *size)
{
    m_io_size_list.push_back(size);

    return m_io_size_list.size() - 1;
}

uint32_t
ExynosVpuProcess::addSizeOpScale(ExynosVpuIoSizeOpScale *scale)
{
    m_io_sizeop_scale_list.push_back(scale);

    return m_io_sizeop_scale_list.size() - 1;
}

uint32_t
ExynosVpuProcess::addSizeOpCropper(ExynosVpuIoSizeOpCropper *cropper)
{
    m_io_sizeop_cropper_list.push_back(cropper);

    return m_io_sizeop_cropper_list.size() - 1;
}

status_t
ExynosVpuProcess::addStaticCoffValue(int32_t coff[], uint32_t coff_num, uint32_t *index_ret)
{
    struct vpul_process_inout *inout = &m_vertex_info.proc.io;

    if ((inout->n_static_coff + coff_num) >= VPUL_MAX_PROC_STATIC_COFF) {
        VXLOGE("static coff poll is overflow, %d, %d", inout->n_static_coff, coff_num);
        return INVALID_OPERATION;
    }

    memcpy(&inout->static_coff[inout->n_static_coff], coff, coff_num * sizeof(coff[0]));
    *index_ret = inout->n_static_coff;
    inout->n_static_coff += coff_num;

    return NO_ERROR;
}

ExynosVpuIoDynamicMapRoi*
ExynosVpuProcess::getDynamicMapRoi(uint32_t index)
{
    if (m_io_dynamic_roi_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_dynamic_roi_list.size());
        return NULL;
    }

    return m_io_dynamic_roi_list[index];
}

ExynosVpuIoFixedMapRoi*
ExynosVpuProcess::getFixedMapRoi(uint32_t index)
{
    if (m_io_fixed_roi_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_fixed_roi_list.size());
        return NULL;
    }

    return m_io_fixed_roi_list[index];
}

ExynosVpuIoTypesDesc*
ExynosVpuProcess::getIotypesDesc(uint32_t index)
{
    if (m_io_iotypes_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_iotypes_list.size());
        return NULL;
    }

    return m_io_iotypes_list[index];
}

ExynosVpuIoSize*
ExynosVpuProcess::getIoSize(uint32_t index)
{
    if (m_io_size_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_size_list.size());
        return NULL;
    }

    return m_io_size_list[index];
}

ExynosVpuIoSizeOpScale*
ExynosVpuProcess::getIoSizeOpScale(uint32_t index)
{
    if (m_io_sizeop_scale_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_sizeop_scale_list.size());
        return NULL;
    }

    return m_io_sizeop_scale_list[index];
}

ExynosVpuIoSizeOpCropper*
ExynosVpuProcess::getIoSizeOpCropper(uint32_t index)
{
    if (m_io_sizeop_cropper_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_sizeop_cropper_list.size());
        return NULL;
    }

    return m_io_sizeop_cropper_list[index];
}

int32_t
ExynosVpuProcess::calcPtsMapIndex(uint32_t iotype_index)
{
    int32_t pts_map_index = -1;
    for (uint32_t i=0; i<=iotype_index; i++) {
        if (m_io_iotypes_list[iotype_index]->getIoTypesInfo()->is_roi_derived)
            pts_map_index++;
    }

    return pts_map_index;
}

status_t
ExynosVpuProcess::getPtsMapMemmapList(uint32_t pts_map_index, List<ExynosVpuMemmap*> *ret_memmap_list)
{
    ret_memmap_list->clear();

    struct vpul_process *process_info = getProcessInfo();
    if (pts_map_index >= process_info->io.pts_maps_each_set) {
        VXLOGE("pts_map_index is out of bound, %d, %d", pts_map_index, getProcessInfo()->io.pts_maps_each_set);
        return INVALID_OPERATION;
    }

    ExynosVpuMemmap *memmap;
    for (uint32_t i=0; i<process_info->io.pts_maps_sets; i++) {
        uint32_t map_indices_index = i * process_info->io.pts_maps_each_set + pts_map_index;
        memmap = getTask()->getMemmap(process_info->io.pts_map_indices[map_indices_index]);
        if (memmap == NULL) {
            VXLOGE("getting memmap fails, index:%d", i * process_info->io.pts_maps_each_set);
            return INVALID_OPERATION;
        }
        ret_memmap_list->push_back(memmap);
    }

    return NO_ERROR;
}

struct vpul_roi_desc*
ExynosVpuProcess::getPtRoiDesc(uint32_t index)
{
    if (getProcessInfo()->io.pt_roi_sizes <= index) {
        VXLOGE("the index of pt_roi_desc out of bound, %d, %d", index, getProcessInfo()->io.pt_roi_sizes);
        return NULL;
    }

    return &getProcessInfo()->io.roi_sizes_desc.roi_desc[index];
}

status_t
ExynosVpuProcess::updateVertexIo(uint32_t std_width, uint32_t std_height)
{
    struct vpul_fixed_map_roi *fixed_info;
    Vector<ExynosVpuIoFixedMapRoi*>::iterator fixed_iter;
    for (fixed_iter=m_io_fixed_roi_list.begin(); fixed_iter!=m_io_fixed_roi_list.end(); fixed_iter++) {
        fixed_info = (*fixed_iter)->getFixedMapInfo();
        if ((fixed_info->roi.first_col==0) && (fixed_info->roi.first_line==0) &&
            (fixed_info->roi.width==0) && (fixed_info->roi.height==0)) {
            fixed_info->roi.first_col = 0;
            fixed_info->roi.first_line = 0;
            fixed_info->roi.width = std_width;
            fixed_info->roi.height = std_height;
        }
    }

    return NO_ERROR;
}

status_t
ExynosVpuProcess::updateStructure(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuVertex::updateStructure();
    if (ret != NO_ERROR) {
        VXLOGE("update vertex structure fails");
        goto EXIT;
    }

    uint32_t i;
    struct vpul_process *process_info;
    process_info = &m_vertex_info.proc;

    /* io_dynamic_roi */
    process_info->io.n_dynamic_map_rois = m_io_dynamic_roi_list.size();
    Vector<ExynosVpuIoDynamicMapRoi*>::iterator dyn_iter;
    for (dyn_iter=m_io_dynamic_roi_list.begin(), i=0; dyn_iter!=m_io_dynamic_roi_list.end(); dyn_iter++, i++) {
        (*dyn_iter)->updateStructure();
        if (i == (*dyn_iter)->getIndex()) {
            if ((*dyn_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.dynamic_map_roi[i], (*dyn_iter)->getDynMapInfo(), sizeof(process_info->io.dynamic_map_roi[i]));
            } else {
                VXLOGE("dyn_map doesn't have valid configuration");
                (*dyn_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of dyn_map is not matching, expected:%d, received:%d", i, (*dyn_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* io_fixed_roi */
    process_info->io.n_fixed_map_roi = m_io_fixed_roi_list.size();
    Vector<ExynosVpuIoFixedMapRoi*>::iterator fixed_iter;
    for (fixed_iter=m_io_fixed_roi_list.begin(), i=0; fixed_iter!=m_io_fixed_roi_list.end(); fixed_iter++, i++) {
        (*fixed_iter)->updateStructure();
        if (i == (*fixed_iter)->getIndex()) {
            if ((*fixed_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.fixed_map_roi[i], (*fixed_iter)->getFixedMapInfo(), sizeof(process_info->io.fixed_map_roi[i]));
            } else {
                VXLOGE("fixed_map doesn't have valid configuration");
                (*fixed_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of fixed_map is not matching, expected:%d, received:%d", i, (*fixed_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* io_iotyps */
    process_info->io.n_inout_types = m_io_iotypes_list.size();
    Vector<ExynosVpuIoTypesDesc*>::iterator iotype_iter;
    for (iotype_iter=m_io_iotypes_list.begin(), i=0; iotype_iter!=m_io_iotypes_list.end(); iotype_iter++, i++) {
        (*iotype_iter)->updateStructure();
        if (i == (*iotype_iter)->getIndex()) {
            if ((*iotype_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.inout_types[i], (*iotype_iter)->getIoTypesInfo(), sizeof(process_info->io.inout_types[i]));
            } else {
                VXLOGE("iotype doesn't have valid configuration");
                (*iotype_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of iotype is not matching, expected:%d, received:%d", i, (*iotype_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* io_size */
    process_info->io.n_sizes_op = m_io_size_list.size();
    Vector<ExynosVpuIoSize*>::iterator size_iter;
    for (size_iter=m_io_size_list.begin(), i=0; size_iter!=m_io_size_list.end(); size_iter++, i++) {
        (*size_iter)->updateStructure();
        if (i == (*size_iter)->getIndex()) {
            if ((*size_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.sizes[i], (*size_iter)->getSizeInfo(), sizeof(process_info->io.sizes[i]));
            } else {
                VXLOGE("iosize doesn't have valid configuration");
                (*size_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of size is not matching, expected:%d, received:%d", i, (*size_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* io_scale */
    process_info->io.n_scales = m_io_sizeop_scale_list.size();
    Vector<ExynosVpuIoSizeOpScale*>::iterator scale_iter;
    for (scale_iter=m_io_sizeop_scale_list.begin(), i=0; scale_iter!=m_io_sizeop_scale_list.end(); scale_iter++, i++) {
        (*scale_iter)->updateStructure();
        if (i == (*scale_iter)->getIndex()) {
            if ((*scale_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.scales[i], (*scale_iter)->getScaleInfo(), sizeof(process_info->io.scales[i]));
            } else {
                VXLOGE("scale doesn't have valid configuration");
                (*scale_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of scale is not matching, expected:%d, received:%d", i, (*scale_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

    /* io_cropper */
    process_info->io.n_croppers = m_io_sizeop_cropper_list.size();
    Vector<ExynosVpuIoSizeOpCropper*>::iterator cropper_iter;
    for (cropper_iter=m_io_sizeop_cropper_list.begin(), i=0; cropper_iter!=m_io_sizeop_cropper_list.end(); cropper_iter++, i++) {
        (*cropper_iter)->updateStructure();
        if (i == (*cropper_iter)->getIndex()) {
            if ((*cropper_iter)->checkConstraint() == NO_ERROR) {
                memcpy(&process_info->io.croppers[i], (*cropper_iter)->getCropperInfo(), sizeof(process_info->io.croppers[i]));
            } else {
                VXLOGE("cropper doesn't have valid configuration");
                (*cropper_iter)->displayObjectInfo();
                ret = INVALID_OPERATION;
                goto EXIT;
            }
        } else {
            VXLOGE("the index of cropper is not matching, expected:%d, received:%d", i, (*cropper_iter)->getIndex());
            ret = INVALID_OPERATION;
            goto EXIT;
        }
    }

EXIT:
    return ret;
}

status_t
ExynosVpuProcess::updateObject(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosVpuVertex::updateObject();
    if (ret != NO_ERROR) {
        VXLOGE("updating vertex object fails");
        goto EXIT;
    }

    /* io_dynamic_roi */
    Vector<ExynosVpuIoDynamicMapRoi*>::iterator dyn_iter;
    for (dyn_iter=m_io_dynamic_roi_list.begin(); dyn_iter!=m_io_dynamic_roi_list.end(); dyn_iter++)
        (*dyn_iter)->updateObject();

    /* io_fixed_roi */
    Vector<ExynosVpuIoFixedMapRoi*>::iterator fixed_iter;
    for (fixed_iter=m_io_fixed_roi_list.begin(); fixed_iter!=m_io_fixed_roi_list.end(); fixed_iter++)
        (*fixed_iter)->updateObject();

    /* io_iotyps */
    Vector<ExynosVpuIoTypesDesc*>::iterator iotype_iter;
    for (iotype_iter=m_io_iotypes_list.begin(); iotype_iter!=m_io_iotypes_list.end(); iotype_iter++)
        (*iotype_iter)->updateObject();

    /* io_size */
    Vector<ExynosVpuIoSize*>::iterator size_iter;
    for (size_iter=m_io_size_list.begin(); size_iter!=m_io_size_list.end(); size_iter++)
        (*size_iter)->updateObject();

    /* io_scale */
    Vector<ExynosVpuIoSizeOpScale*>::iterator scale_iter;
    for (scale_iter=m_io_sizeop_scale_list.begin(); scale_iter!=m_io_sizeop_scale_list.end(); scale_iter++)
        (*scale_iter)->updateObject();

    /* io_cropper */
    Vector<ExynosVpuIoSizeOpCropper*>::iterator cropper_iter;
    for (cropper_iter=m_io_sizeop_cropper_list.begin(); cropper_iter!=m_io_sizeop_cropper_list.end(); cropper_iter++)
        (*cropper_iter)->updateObject();

EXIT:
    return ret;
}

}; /* namespace android */
