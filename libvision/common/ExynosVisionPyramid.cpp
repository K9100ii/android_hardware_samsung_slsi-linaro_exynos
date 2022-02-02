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

#define LOG_TAG "ExynosVisionPyramid"
#include <cutils/log.h>

#include <VX/vx.h>

#include "ExynosVisionPyramid.h"
#include "ExynosVisionDelay.h"

namespace android {

vx_status
ExynosVisionPyramid::checkValidCreatePyramid(vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    if ((width == 0) || (height == 0) || (format == VX_DF_IMAGE_VIRT))
        return VX_ERROR_INVALID_PARAMETERS;

    if ((scale != VX_SCALE_PYRAMID_HALF) &&
        (scale != VX_SCALE_PYRAMID_ORB))
        return VX_ERROR_INVALID_PARAMETERS;

    if (levels == 0 || levels > 8)
        return VX_ERROR_INVALID_PARAMETERS;

    return VX_SUCCESS;
}

ExynosVisionPyramid::ExynosVisionPyramid(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual)
                                                               : ExynosVisionDataReference(context, VX_TYPE_PYRAMID, scope, vx_true_e, is_virtual)
{
    m_res_mngr = NULL;
    m_cur_res = NULL;

    m_num_level = 0;
    m_scale = 0;

    m_width = 0;
    m_height = 0;
    m_format = 0;

#ifdef USE_OPENCL_KERNEL
    m_cl_memory.allocated = vx_false_e;
#endif
}

ExynosVisionPyramid::~ExynosVisionPyramid()
{

}

void
ExynosVisionPyramid::operator=(const ExynosVisionPyramid& src_pyramid)
{
    m_num_level = src_pyramid.m_num_level;
    m_scale = src_pyramid.m_scale;

    m_width = src_pyramid.m_width;
    m_height = src_pyramid.m_height;
    m_format = src_pyramid.m_format;
}

vx_status
ExynosVisionPyramid::init(vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    vx_status status = VX_SUCCESS;

    /* very first init will come in here */
    m_num_level = levels;
    m_scale = scale;

    /* these could be "virtual" values or hard values */
    m_width = width;
    m_height = height;
    m_format = format;

    return status;
}

vx_status
ExynosVisionPyramid::destroy(void)
{
    vx_status status = VX_SUCCESS;

    /* very first init will come in here */
    m_num_level = 0;
    m_scale = 0;

    /* these could be "virtual" values or hard values */
    m_width = 0;
    m_height = 0;
    m_format = 0;

    m_cur_res = NULL;

    status = freeMemory_T<pyramid_resource_t>(&m_res_mngr, &m_res_list);
    if (status != VX_SUCCESS)
        VXLOGE("free memory fails at %s, err:%d", getName(), status);

    return status;
}

ExynosVisionImage*
ExynosVisionPyramid::getPyramidLevel(vx_uint32 index)
{
    ExynosVisionImage *image = NULL;

    if (index < m_num_level) {
        if (m_is_allocated != vx_true_e) {
            vx_status status = ((ExynosVisionDataReference*)this)->allocateMemory();
            if (status != VX_SUCCESS) {
                VXLOGE("memory is not allocated yet");
                goto EXIT;
            }
        }

        image = m_cur_res->editItemAt(index);
        image->incrementReference(VX_REF_EXTERNAL);
    }

EXIT:
    return image;
}

vx_status
ExynosVisionPyramid::queryPyramid(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_PYRAMID_ATTRIBUTE_LEVELS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_num_level;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PYRAMID_ATTRIBUTE_SCALE:
        if (VX_CHECK_PARAM(ptr, size, vx_float32, 0x3))
            *(vx_float32 *)ptr = m_scale;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PYRAMID_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_width;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PYRAMID_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_height;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_PYRAMID_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3))
            *(vx_df_image *)ptr = m_format;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionPyramid::verifyMeta(ExynosVisionMeta *meta)
{
    vx_status status = VX_SUCCESS;

    vx_size num_level;
    vx_float32 scale;
    vx_uint32 width;
    vx_uint32 height;
    vx_df_image format;

    status |= meta->getMetaFormatAttribute(VX_PYRAMID_ATTRIBUTE_LEVELS, &num_level, sizeof(num_level));
    status |= meta->getMetaFormatAttribute(VX_PYRAMID_ATTRIBUTE_SCALE, &scale, sizeof(scale));
    status |= meta->getMetaFormatAttribute(VX_PYRAMID_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= meta->getMetaFormatAttribute(VX_PYRAMID_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    status |= meta->getMetaFormatAttribute(VX_PYRAMID_ATTRIBUTE_FORMAT, &format, sizeof(format));
    if (status != VX_SUCCESS) {
        VXLOGE("get meta fail, err:%d", status);
    }

    if ((m_num_level != num_level) || (m_scale != scale)) {
        status = VX_ERROR_INVALID_VALUE;
        VXLOGE("Either levels (%u = %u) or scale (%lf = %lf) are invalid",
                m_num_level, num_level,
                m_scale, scale);
        getContext()->addLogEntry(this, status, "Either levels (%u?=%u) or scale (%lf?=%lf) are invalid\n",
                m_num_level, num_level,
                m_scale, scale);
        goto EXIT;
    }

    if ((m_format != VX_DF_IMAGE_VIRT) && (m_format != format)) {
        status = VX_ERROR_INVALID_FORMAT;
        VXLOGE("Invalid pyramid format %x, needs %x",
                m_format, format);
        getContext()->addLogEntry(this, status, "Invalid pyramid format %x, needs %x\n",
                m_format, format);
        goto EXIT;
    }

    if (((m_width != 0) && (m_width != width)) ||
        ((m_height != 0) && (m_height != height))) {
        status = VX_ERROR_INVALID_DIMENSION;
        VXLOGE("Invalid pyramid dimensions %ux%u, needs %ux%u",
                m_width, m_height, width, height);
        getContext()->addLogEntry(this, status, "Invalid pyramid dimensions %ux%u, needs %ux%u\n",
                m_width, m_height, width, height);
        goto EXIT;
    }

    if (isVirtual()) {
        init(m_num_level, m_scale, width, height, format);
    }

EXIT:

    return status;
}

vx_status
ExynosVisionPyramid::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    return allocateMemory_T(res_type, param, vx_false_e, &m_res_mngr, &m_res_list, &m_cur_res);
}

vx_status
ExynosVisionPyramid::allocateResource(pyramid_resource_t **ret_resource)
{
    if (ret_resource == NULL) {
        VXLOGE("pointer is null at %s", getName());
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status = VX_SUCCESS;
    pyramid_resource_t *resource = new pyramid_resource_t();

    vx_uint32 w = m_width;
    vx_uint32 h = m_height;

    for (vx_uint32 i = 0; i < m_num_level; i++) {
        ExynosVisionImage *image = new ExynosVisionImage(getContext(), this, vx_false_e);
        image->incrementReference(VX_REF_INTERNAL, this);
        status = image->init(w, h, m_format);
        resource->push_back(image);

        w = (vx_uint32)ceilf((vx_float32)w * m_scale);
        h = (vx_uint32)ceilf((vx_float32)h * m_scale);

        ExynosVisionDataReference::jointAlliance(this, image);
    }

    *ret_resource = resource;

    return status;
}

vx_status
ExynosVisionPyramid::freeResource(pyramid_resource_t *image_vector)
{
    vx_status status = VX_SUCCESS;

    Vector<ExynosVisionImage*>::iterator image_iter;
    for(image_iter=image_vector->begin(); image_iter!=image_vector->end(); image_iter++) {
        if (*image_iter != NULL) {
            status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&(*image_iter), VX_REF_INTERNAL, this);
            if (status != VX_SUCCESS)
                VXLOGE("release %s fails at %s", (*image_iter)->getName(), this->getName());
        }
    }
    image_vector->clear();
    delete image_vector;

    return status;
}

ExynosVisionDataReference*
ExynosVisionPyramid::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    return getInputShareRef_T<pyramid_resource_t>(m_res_mngr, frame_cnt, ret_data_valid);
}

vx_status
ExynosVisionPyramid::putInputShareRef(vx_uint32 frame_cnt)
{
    return putInputShareRef_T<pyramid_resource_t>(m_res_mngr, frame_cnt);
}

ExynosVisionDataReference*
ExynosVisionPyramid::getOutputShareRef(vx_uint32 frame_cnt)
{
    return getOutputShareRef_T<pyramid_resource_t>(m_res_mngr, frame_cnt);
}

vx_status
ExynosVisionPyramid::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    return putOutputShareRef_T<pyramid_resource_t>(m_res_mngr, frame_cnt, demand_num, data_valid);
}

void
ExynosVisionPyramid::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Pyramid] %s(%p), resol:%dx%d, format:0x%x, virtual:%d, alloc:%d, refCnt:%d/%d", MAKE_TAB(tap, tab_num), getName(), this,
                                                m_width, m_height, m_format, isVirtual(), m_is_allocated,
                                                getInternalCnt(), getExternalCnt());

    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        VXLOGI("%s[--Pyram] alliance : %s", MAKE_TAB(tap, tab_num), (*ref_iter)->getName());
    }

    if (isDelayElement())
        VXLOGI("%s          delay element, %s, delay phy index:%d", MAKE_TAB(tap, tab_num), getDelay()->getName(), getDelaySlotIndex());

    LOG_FLUSH_TIME();

    if (detail_info)
        displayConn(tab_num);
}

}; /* namespace android */
