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

#ifndef EXYNOS_VISION_DELAY_H
#define EXYNOS_VISION_DELAY_H

#include "ExynosVisionReference.h"

namespace android {

struct vx_delay_node_info {
    ExynosVisionNode *node;
    vx_uint32 index;
} ;

class ExynosVisionDelay : public ExynosVisionDataReference {
private:
    /*! \brief The number of objects in the delay. */
    vx_size m_count;
    /*! \brief The current index which is '0' */
    vx_uint32 m_base_index;
    /*! \brief Object Type in the Delay. */
    vx_enum m_delay_type;
    /*! \brief The set of Nodes each object is associated with in the delay. */
    Vector<List<struct vx_delay_node_info>> m_set_vector;
    /*! \brief The set of objects in the delay. */
    Vector<ExynosVisionDataReference*> m_ref_vector;

public:

private:

public:
    static vx_status checkValidCreateDelay(ExynosVisionContext *context, ExynosVisionDataReference *reference);

    /* Constructor */
    ExynosVisionDelay(ExynosVisionContext *context);

    /* Destructor */
    virtual ~ExynosVisionDelay();

    vx_status init(ExynosVisionReference *exemplar, vx_size count);
    virtual vx_status destroy(void);

    vx_status queryDelay(vx_enum attribute, void *ptr, vx_size size);
    ExynosVisionDataReference* getReferenceFromDelay(vx_int32 index);
    vx_status ageDelay(void);

    vx_bool addAssociationToDelay(ExynosVisionDataReference *ref, ExynosVisionNode *node, vx_uint32 index);
    vx_bool removeAssociationToDelay(ExynosVisionDataReference *ref, ExynosVisionNode *node, vx_uint32 index);

    /* resource management function */
    vx_status allocateMemory(vx_enum buf_type, struct resource_param *param);
    virtual vx_status allocateResource(empty_resource_t **ret_resource);
    virtual vx_status freeResource(empty_resource_t *image_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
