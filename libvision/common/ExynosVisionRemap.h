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

#ifndef EXYNOS_VISION_REMAP_H
#define EXYNOS_VISION_REMAP_H

#include "ExynosVisionReference.h"

namespace android {

struct remap_buffer {
    vx_enum access_mode;
    vx_enum usage;
    /* this value saves the external buffer's stride when access_copy_mode */
    vx_size external_stride;
};

class ExynosVisionRemap : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<default_resource_t*> *m_res_mngr;
    List<default_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    default_resource_t *m_cur_res;

    Mutex m_memory_lock;
    struct remap_buffer m_memory;

    /*! \brief Input Width */
    vx_uint32 m_src_width;
    /*! \brief Input Height */
    vx_uint32 m_src_height;
    /*! \brief Output Width */
    vx_uint32 m_dst_width;
    /*! \brief Output Height */
    vx_uint32 m_dst_height;

public:

private:
    void* formatMemoryPtr(vx_uint32 dst_x, vx_uint32 dst_y, vx_uint32 pair_index);

public:
    static vx_status checkValidCreateRemap(vx_uint32 src_width, vx_uint32 src_height, vx_uint32 dst_width, vx_uint32 dst_height);

    /* Constructor */
    ExynosVisionRemap(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionRemap();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionRemap& src_remap);
    vx_status init(vx_uint32 src_width, vx_uint32 src_height, vx_uint32 dst_width, vx_uint32 dst_height);
    virtual vx_status destroy(void);

    vx_status queryRemap(vx_enum attribute, void *ptr, vx_size size);
    vx_status setRemapPoint(vx_uint32 dst_x, vx_uint32 dst_y, vx_float32 src_x, vx_float32 src_y);
    vx_status getRemapPoint(vx_uint32 dst_x, vx_uint32 dst_y, vx_float32 *src_x, vx_float32 *src_y);

    /* resource management function */
    vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(default_resource_t **ret_resource);
    virtual vx_status freeResource(default_resource_t *image_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

#ifdef USE_OPENCL_KERNEL
    vx_status getClMemoryInfo(cl_context context, vxcl_mem_t **memory);
#endif
};

}; // namespace android
#endif
