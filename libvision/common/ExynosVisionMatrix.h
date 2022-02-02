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

#ifndef EXYNOS_VISION_MATRIX_H
#define EXYNOS_VISION_MATRIX_H

#include "ExynosVisionReference.h"

namespace android {

struct matrix_buffer {

};

class ExynosVisionMatrix : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<default_resource_t*> *m_res_mngr;
    List<default_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    default_resource_t *m_cur_res;

    struct matrix_buffer m_memory;

    /*! \brief From \ref vx_type_e */
    vx_enum m_data_type;
    /*! \brief Number of columns */
    vx_size m_columns;
    /*! \brief Number of rows */
    vx_size m_rows;
    /*! \brief The size of matrix item in bytes */
    vx_size m_element_size;

public:

private:
    vx_status freeMemory();

public:
    static vx_status checkValidCreateMatrix(vx_enum data_type, vx_size columns, vx_size rows);

    /* Constructor */
    ExynosVisionMatrix(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionMatrix();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionMatrix& src_matrix);
    vx_status init(vx_enum data_type, vx_size columns, vx_size rows);
    virtual vx_status destroy(void);

    vx_status queryMatrix(vx_enum attribute, void *ptr, vx_size size);
    vx_status readMatrix(void *array);
    vx_status writeMatrix(const void *array);

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
