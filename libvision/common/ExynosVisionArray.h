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

#ifndef EXYNOS_VISION_ARRAY_H
#define EXYNOS_VISION_ARRAY_H

#include "ExynosVisionReference.h"

namespace android {

struct array_buffer {
    vx_enum access_mode;
    vx_enum usage;
    /* this value saves the external buffer's stride when access_copy_mode */
    vx_size external_stride;
};

class ExynosVisionArray : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<default_resource_t*> *m_res_mngr;
    List<default_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    default_resource_t *m_cur_res;

    Mutex m_memory_lock;
    struct array_buffer m_memory;

    /*! \brief The item type of the array. */
    vx_enum m_item_type;
    /*! \brief The size of array item in bytes */
    vx_size m_item_size;
    /*! \brief The number of items in the array */
    vx_size m_num_items;
    /*! \brief The array capacity */
    vx_size m_capacity;

public:

private:
    vx_bool initVirtualArray(vx_enum item_type, vx_size capacity);
    vx_bool validateArray(vx_enum item_type, vx_size capacity);

public:
    static vx_status checkValidCreateArray(ExynosVisionContext *context, vx_enum item_type, vx_size capacity);

    /* Constructor */
    ExynosVisionArray(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual);

    /* Destructor */
    virtual ~ExynosVisionArray();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionArray& src_array);
    vx_status init(vx_enum item_type, vx_size capacity);
    virtual vx_status destroy(void);

    vx_status queryArray(vx_enum attribute, void *ptr, vx_size size);
    vx_status truncateArray(vx_size new_num_items);

    vx_status addArrayItems(vx_size count, const void *ptr, vx_size stride);
    /* just increase item number, this function is used by lut */
    vx_status addEmptyArrayItems(vx_size count);
    vx_status accessArrayRange(vx_size start, vx_size end, vx_size *stride, void **ptr, vx_enum usage);
    vx_status accessArrayHandle(vx_int32 *fd, vx_enum usage);
    vx_status commitArrayRange(vx_size start, vx_size end, const void *ptr);
    vx_status commitArrayHandle(const vx_int32 fd);

    virtual vx_status verifyMeta(ExynosVisionMeta *meta);

    /* resource management function */
    vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(default_resource_t **ret_resource);
    virtual vx_status freeResource(default_resource_t *image_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);

#ifdef USE_OPENCL_KERNEL
    vx_status getClMemoryInfo(cl_context context, vxcl_mem_t **memory);
#endif
};

}; // namespace android
#endif
