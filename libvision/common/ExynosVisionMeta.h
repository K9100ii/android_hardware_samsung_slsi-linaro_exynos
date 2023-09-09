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

#ifndef EXYNOS_VISION_META_H
#define EXYNOS_VISION_META_H

#include "ExynosVisionReference.h"

namespace android {

class ExynosVisionMeta : public ExynosVisionReference {
private:
    /*!< \brief The size of struct. */
    vx_enum m_meta_type;               /*!< \brief The <tt>\ref vx_type_e</tt> or <tt>\ref vx_df_image_e</tt> code */

    /*! \brief A struct of the configuration types needed by all object types. */
    struct dim {
        /*! \brief When a VX_TYPE_IMAGE */
        struct image {
            vx_uint32 width;    /*!< \brief The width of the image in pixels */
            vx_uint32 height;   /*!< \brief The height of the image in pixels */
            vx_df_image format;   /*!< \brief The format of the image. */
            vx_delta_rectangle_t delta; /*!< \brief The delta rectangle applied to this image */
        } image;
        /*! \brief When a VX_TYPE_PYRAMID is specified */
        struct pyramid {
            vx_uint32 width;    /*!< \brief The width of the 0th image in pixels. */
            vx_uint32 height;   /*!< \brief The height of the 0th image in pixels. */
            vx_df_image format;   /*!< \brief The <tt>\ref vx_df_image_e</tt> format of the image. */
            vx_size levels;     /*!< \brief The number of scale levels */
            vx_float32 scale;   /*!< \brief The ratio between each level */
        } pyramid;
        /*! \brief When a VX_TYPE_SCALAR is specified */
        struct scalar {
            vx_enum type;       /*!< \brief The type of the scalar */
        } scalar;
        /*! \brief When a VX_TYPE_ARRAY is specified */
        struct array {
            vx_enum item_type;  /*!< \brief The type of the Array items */
            vx_size capacity;   /*!< \brief The capacity of the Array */
        } array;
    } m_dim;

public:

private:

public:
    /* Constructor */
    ExynosVisionMeta(ExynosVisionContext *context);

    /* Destructor */
    virtual ~ExynosVisionMeta();

    vx_status destroy(void);

    vx_status setMetaFormatAttribute(vx_enum attribute, const void *ptr, vx_size size);
    vx_status getMetaFormatAttribute(vx_enum attribute, void *ptr, vx_size size);

    void setMetaType(vx_enum meta_type)
    {
        m_meta_type = meta_type;
    }
    vx_enum getMetaType(void)
    {
        return m_meta_type;
    }
};

}; // namespace android
#endif
