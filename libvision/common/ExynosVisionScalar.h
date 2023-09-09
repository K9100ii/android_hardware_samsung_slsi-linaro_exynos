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

#ifndef EXYNOS_VISION_SCALAR_H
#define EXYNOS_VISION_SCALAR_H

#include "ExynosVisionReference.h"

namespace android {

class ExynosVisionScalar : public ExynosVisionDataReference {
private:
    /* variable for resource management */
    ExynosVisionResManager<empty_resource_t*> *m_res_mngr;
    List<empty_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    empty_resource_t *m_cur_res;

    /*! \brief The atomic type of the scalar */
    vx_enum               m_data_type;
    /*! \brief The value contained in the reference for a scalar type */
    union {
        /*! \brief A character */
        vx_char   chr;
        /*! \brief Signed 8 bit */
        vx_int8   s08;
        /*! \brief Unsigned 8 bit */
        vx_uint8  u08;
        /*! \brief Signed 16 bit */
        vx_int16  s16;
        /*! \brief Unsigned 16 bit */
        vx_uint16 u16;
        /*! \brief Signed 32 bit */
        vx_int32  s32;
        /*! \brief Unsigned 32 bit */
        vx_uint32 u32;
        /*! \brief Signed 64 bit */
        vx_int64  s64;
        /*! \brief Unsigned 64 bit */
        vx_int64  u64;
        /*! \brief 32 bit float */
        vx_float32 f32;
        /*! \brief 64 bit float */
        vx_float64 f64;
        /*! \brief 32 bit image format code */
        vx_df_image  fcc;
        /*! \brief Signed 32 bit*/
        vx_enum    enm;
        /*! \brief Architecture depth unsigned value */
        vx_size    size;
        /*! \brief Boolean Values */
        vx_bool    boolean;
    } m_data;

public:

private:

public:
    static vx_status isValidCreateScalar(ExynosVisionContext *context, vx_enum data_type);

    /* Constructor */
    ExynosVisionScalar(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionScalar(void);

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionScalar& src_scalar);
    vx_status init(vx_enum data_type, const void *ptr);
    virtual vx_status destroy(void);

    vx_status queryScalar(vx_enum attribute, void *ptr, vx_size size);

    vx_status readScalarValue(void *ptr);
    vx_status writeScalarValue(const void *ptr);

    virtual vx_status verifyMeta(ExynosVisionMeta *meta);

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
