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

#ifndef EXYNOS_VISION_PYRAMID_H
#define EXYNOS_VISION_PYRAMID_H

#include "ExynosVisionReference.h"
#include "ExynosVisionImage.h"

namespace android {

class ExynosVisionPyramid : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<pyramid_resource_t*> *m_res_mngr;
    List<pyramid_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    pyramid_resource_t *m_cur_res;

    /*! \brief Number of levels in the pyramid */
    vx_size m_num_level;
    /*! \brief Scaling factor between levels of the pyramid. */
    vx_float32 m_scale;
    /*! \brief Level 0 width */
    vx_uint32 m_width;
    /*! \brief Level 0 height */
    vx_uint32 m_height;
    /*! \brief Format for all levels */
    vx_df_image m_format;

public:

private:

public:
    static vx_status checkValidCreatePyramid(vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format);

    /* Constructor */
    ExynosVisionPyramid(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual);

    /* Destructor */
    virtual ~ExynosVisionPyramid();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionPyramid& src_pyramid);
    vx_status init(vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format);
    virtual vx_status destroy(void);

    ExynosVisionImage* getPyramidLevel(vx_uint32 index);
    vx_status queryPyramid(vx_enum attribute, void *ptr, vx_size size);

    virtual vx_status verifyMeta(ExynosVisionMeta *meta);

    /* resource management function */
    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(pyramid_resource_t **ret_resource);
    virtual vx_status freeResource(pyramid_resource_t *image_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
