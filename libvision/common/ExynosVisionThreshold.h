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

#ifndef EXYNOS_VISION_THRESHOLD_H
#define EXYNOS_VISION_THRESHOLD_H

#include "ExynosVisionReference.h"

namespace android {

class ExynosVisionThreshold : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<empty_resource_t*> *m_res_mngr;
    List<empty_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    empty_resource_t *m_cur_res;

    /*! \brief From \ref vx_threshold_type_e */
    vx_enum m_thresh_type;
    /*! \brief The binary threshold value */
    vx_uint8 m_value;
    /*! \brief Lower bound for range threshold */
    vx_uint8 m_lower;
    /*! \brief Upper bound for range threshold */
    vx_uint8 m_upper;
    /*! \brief True value for output */
    vx_uint8 m_true_value;
    /*! \brief Fasle value for output */
    vx_uint8 m_false_value;

public:

private:

public:
    static vx_status checkValidCreateThreshold(vx_enum thresh_type, vx_enum data_type);

    /* Constructor */
    ExynosVisionThreshold(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionThreshold();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionThreshold& src_threshold);
    vx_status init(vx_enum thresh_type);
    virtual vx_status destroy(void);

    vx_status queryThreshold(vx_enum attribute, void *ptr, vx_size size);
    vx_status setThresholdAttribute(vx_enum attribute, const void *ptr, vx_size size);

    /* resource management function */
    vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(empty_resource_t **ret_resource);
    virtual vx_status freeResource(empty_resource_t *image_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);
};

}; // namespace android
#endif
