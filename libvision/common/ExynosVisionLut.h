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

#ifndef EXYNOS_VISION_LUT_H
#define EXYNOS_VISION_LUT_H

#include "ExynosVisionReference.h"

#include "ExynosVisionArray.h"

namespace android {

class ExynosVisionLut : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<lut_resource_t*> *m_res_mngr;
    List<lut_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    lut_resource_t *m_cur_res;

    vx_enum m_data_type;
    vx_size m_count;

public:

private:

public:
    static vx_status checkValidCreateLut(vx_enum data_type, vx_size count);

    /* Constructor */
    ExynosVisionLut(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionLut();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionLut& src_lut);
    vx_status init(vx_enum data_type, vx_size count);
    virtual vx_status destroy(void);

    vx_status queryLut(vx_enum attribute, void *ptr, vx_size size);

    vx_status accessLut(void **ptr, vx_enum usage);
    vx_status accessLutHandle(vx_int32 *fd, vx_enum usage);
    vx_status commitLut(const void *ptr);
    vx_status commitLutHandle(const vx_int32 fd);

    /* resource management function */
    vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(lut_resource_t **ret_resource);
    virtual vx_status freeResource(lut_resource_t *image_vector);

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
