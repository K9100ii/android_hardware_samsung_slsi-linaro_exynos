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

#ifndef EXYNOS_VISION_DISTRIBUTION_H
#define EXYNOS_VISION_DISTRIBUTION_H

#include "ExynosVisionReference.h"

namespace android {

typedef ExynosVisionBufMemory distribution_resource_t;

struct distribution_buffer {
    vx_enum usage;
};

class ExynosVisionDistribution : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<default_resource_t*> *m_res_mngr;
    List<default_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    default_resource_t *m_cur_res;

    Mutex m_memory_lock;
    struct distribution_buffer m_memory;

    /*! \brief The number of elements in the active X dimension of the distribution. */
    vx_uint32 m_window_x;

    /*! \brief The number of inactive elements from zero in the X dimension */
    vx_int32 m_offset_x;
    /*! \brief The total number of elements in the X dimension */
    vx_uint32 m_range_x;

    vx_uint32 m_num_bins;

public:

private:

public:
    static vx_status checkValidCreateDistribution(vx_size numBins, vx_uint32 range);

    /* Constructor */
    ExynosVisionDistribution(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionDistribution();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionDistribution& src_dist);
    vx_status init(vx_size numBins, vx_int32 offset, vx_uint32 range);
    virtual vx_status destroy(void);

    vx_status queryDistribution(vx_enum attribute, void *ptr, vx_size size);
    vx_status accessDistribution(void **ptr, vx_enum usage);
    vx_status accessDistributionHandle(vx_int32 *fd, vx_enum usage);
    vx_status commitDistribution(const void *ptr);
    vx_status commitDistributionHandle(const vx_int32 fd);

    /* resource management function */
    vx_status allocateMemory(vx_enum buf_type, struct resource_param *param);
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
