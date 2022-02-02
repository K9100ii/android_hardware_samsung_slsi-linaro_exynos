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

#define LOG_TAG "ExynosVisionMeta"
#include <cutils/log.h>

#include "ExynosVisionMeta.h"

namespace android {

ExynosVisionMeta::ExynosVisionMeta(ExynosVisionContext *context)
                            : ExynosVisionReference(context, VX_TYPE_META_FORMAT, (ExynosVisionReference*)context, vx_false_e)
{
    if (getCreationStatus() != VX_SUCCESS) {
        VXLOGE("create fail, err:%d", getCreationStatus());
    }
    m_meta_type = VX_TYPE_INVALID;

    memset(&m_dim, 0x0, sizeof(m_dim));
}

ExynosVisionMeta::~ExynosVisionMeta()
{

}

vx_status
ExynosVisionMeta::destroy(void)
{
    m_meta_type = VX_TYPE_INVALID;

    return VX_SUCCESS;
}

vx_status
ExynosVisionMeta::setMetaFormatAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    if (VX_TYPE(attribute) != (vx_uint32)m_meta_type) {
        VXLOGE("format is not matching, meta:0x%X, attr:0x%X", m_meta_type, VX_TYPE(attribute));
        return VX_ERROR_INVALID_TYPE;
    }

    switch(attribute) {
    /**********************************************************************/
    case VX_META_FORMAT_ATTRIBUTE_DELTA_RECTANGLE:
        if (VX_CHECK_PARAM(ptr, size, vx_delta_rectangle_t, 0x3)) {
            memcpy(&m_dim.image.delta, ptr, size);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_IMAGE_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3)) {
            m_dim.image.format = *(vx_df_image *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_IMAGE_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            m_dim.image.height = *(vx_uint32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_IMAGE_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            m_dim.image.width = *(vx_uint32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_ARRAY_ATTRIBUTE_CAPACITY:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            m_dim.array.capacity = *(vx_size *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_ARRAY_ATTRIBUTE_ITEMTYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            m_dim.array.item_type = *(vx_enum *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_PYRAMID_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3)) {
            m_dim.pyramid.format = *(vx_df_image *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            m_dim.pyramid.height = *(vx_uint32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            m_dim.pyramid.width = *(vx_uint32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_LEVELS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            m_dim.pyramid.levels = *(vx_size *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_SCALE:
        if (VX_CHECK_PARAM(ptr, size, vx_float32, 0x3)) {
            m_dim.pyramid.scale = *(vx_float32 *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_SCALAR_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            m_dim.scalar.type = *(vx_enum *)ptr;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
    return status;
}

vx_status
ExynosVisionMeta::getMetaFormatAttribute(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    if (VX_TYPE(attribute) != (vx_uint32)m_meta_type) {
        VXLOGE("format is not matching, meta:0x%X, attr:0x%X", m_meta_type, VX_TYPE(attribute));
        return VX_ERROR_INVALID_TYPE;
    }

    switch(attribute) {
    /**********************************************************************/
    case VX_META_FORMAT_ATTRIBUTE_DELTA_RECTANGLE:
        if (VX_CHECK_PARAM(ptr, size, vx_delta_rectangle_t, 0x3)) {
            memcpy(ptr, &m_dim.image.delta, size);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_IMAGE_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3)) {
            *(vx_df_image *)ptr = m_dim.image.format;

        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_IMAGE_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32 *)ptr = m_dim.image.height;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_IMAGE_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32 *)ptr = m_dim.image.width;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_ARRAY_ATTRIBUTE_CAPACITY:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            *(vx_size *)ptr = m_dim.array.capacity;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_ARRAY_ATTRIBUTE_ITEMTYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            *(vx_enum *)ptr = m_dim.array.item_type;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_PYRAMID_ATTRIBUTE_FORMAT:
        if (VX_CHECK_PARAM(ptr, size, vx_df_image, 0x3)) {
            *(vx_df_image *)ptr = m_dim.pyramid.format;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_HEIGHT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32 *)ptr = m_dim.pyramid.height;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_WIDTH:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32 *)ptr = m_dim.pyramid.width;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_LEVELS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3)) {
            *(vx_size *)ptr = m_dim.pyramid.levels;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_PYRAMID_ATTRIBUTE_SCALE:
        if (VX_CHECK_PARAM(ptr, size, vx_float32, 0x3)) {
            *(vx_float32 *)ptr = m_dim.pyramid.scale;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    case VX_SCALAR_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3)) {
            *(vx_enum *)ptr = m_dim.scalar.type;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    /**********************************************************************/
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
    return status;
}


}; /* namespace android */
