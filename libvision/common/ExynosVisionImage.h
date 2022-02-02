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

#ifndef EXYNOS_VISION_IMAGE_H
#define EXYNOS_VISION_IMAGE_H

#include "ExynosVisionReference.h"
#include "ExynosVisionContext.h"

enum vx_MEMORY_e {
    VX_MEM_0 = 0,
    VX_MEM_1,
    VX_MEM_2,
    VX_MEM_3,
    VX_MEM_MAX,
};

enum vx_plane_e {
    VX_PLANE_0 = 0,
    VX_PLANE_1,
    VX_PLANE_2,
    VX_PLANE_3,
    VX_PLANE_MAX,
};

enum vx_dim_e {
    /*! \brief Channels dimension, stride */
    VX_DIM_C = 0,
    /*! \brief Width (dimension) or x stride */
    VX_DIM_X,
    /*! \brief Height (dimension) or y stride */
    VX_DIM_Y,
    /*! \brief [hidden] The maximum number of dimensions */
    VX_DIM_MAX,
};

enum vx_bounds_e {
    /*! \brief The starting inclusive bound */
    VX_BOUND_START,
    /*! \brief The ending exclusive bound */
    VX_BOUND_END,
    /*! \brief [hidden] The maximum bound dimension */
    VX_BOUND_MAX,
};

namespace android {

struct image_buffer {
    /* plane accessing information */
    vx_enum access_mode[VX_PLANE_MAX];
    vx_enum usage[VX_PLANE_MAX];

    /* vx_uint16 : 2B, vx_uint32 : 4B, other : 1B(Each Y, U, V, R, G, B is 1B) */
    vx_int32 element_byte_size;

    /*! \brief The number of memory */
    vx_uint32 memory_num;

    /*! \brief The number of sub-plane including each memory */
    vx_uint32 subplane_num[VX_MEM_MAX];
    /*! \brief The offset of plane from start address of memory */
    vx_uint32 subplane_mem_offset[VX_MEM_MAX][VX_PLANE_MAX];

    /*! \brief The dimensional values per memory */
    vx_uint32 dims[VX_MEM_MAX][VX_PLANE_MAX][VX_DIM_MAX];
    /*! \brief The per memory stride values per dimension */
    vx_uint32 strides[VX_MEM_MAX][VX_PLANE_MAX][VX_DIM_MAX];

    /* memory offset for ROI, offset of memory on parent image's memory */
    vx_uint64 parent_plane_offset[VX_MEM_MAX];
};

class ExynosVisionImageROI;

class ExynosVisionImage : public ExynosVisionDataReference {

typedef struct _mem_location_t {
    /* index of nptrs */
    vx_uint32 memory_index;
    /* plane index of each ptr */
    vx_uint32 subplane_index;
} mem_location_t;

private:
    List<ExynosVisionImageROI*> m_roi_descendant_list;

protected:
    /* variable for resource management */
    ExynosVisionResManager<image_resource_t*> *m_res_mngr;
    List<image_resource_t*> m_res_list;

    /* variable for accessing resoruce, each buf_element coressponding with each plane */
    image_resource_t *m_cur_res;

    /*! \brief The write locks. Used by Access/Commit pairs on usages which have
     * VX_WRITE_ONLY or VX_READ_AND_WRITE flag parts. Only single writers are permitted.
     */
    Mutex m_memory_locks[VX_PLANE_MAX];
    struct image_buffer m_memory;

    /*! \brief Width of the Image in Pixels */
    vx_uint32      m_width;
    /*! \brief Height of the Image in Pixels */
    vx_uint32      m_height;
    /*! \brief Format of the Image in VX_DF_IMAGE codes */
    vx_df_image      m_format;

    /*! \brief The number of active planes */
    vx_uint32      m_planes;
    /* indicate that the memories of planes are continuous (default is a non-continuous) */
    vx_bool m_continuous;

    /*! \brief The constants space (BT601 or BT709) */
    vx_enum        m_space;
    /*! \brief The desired color range */
    vx_enum        m_range;

    /*! \brief The location information of each plane */
    mem_location_t      m_plane_loc[VX_PLANE_MAX];
    /*! \brief The sub-channel scaling for each plane */
    vx_uint32      m_scale[VX_PLANE_MAX][VX_DIM_MAX];
    /*! \brief The per-plane, per-dimension bounds (start, end). */
    vx_uint32      m_bounds[VX_PLANE_MAX][VX_DIM_MAX][VX_BOUND_MAX];

    /*! \brief Indicates if the image is constant. */
    vx_bool        m_constant;
    /*! \brief The valid region(region that datas are written) */
    vx_rectangle_t m_region;
    /*! \brief The import type */
    vx_enum        m_import_type;

public:

private:
    void initImage(vx_uint32 width, vx_uint32 height, vx_df_image format);
    vx_uint32 computePlaneRangeSize(vx_uint32 range, vx_uint32 p);
    void initPlane(vx_uint32 index, vx_uint32 channels, vx_uint32 coordinate_scale, vx_uint32 width, vx_uint32 height);
    void initMemory(vx_uint32 plane_index, vx_uint32 mem_index, vx_uint32 subplane_index, vx_uint32 channels, vx_uint32 width, vx_uint32 height);

public:
    static vx_status checkValidCreateImage(vx_uint32 width, vx_uint32 height, vx_df_image format);
    static void *formatImagePatchAddress1d(void *ptr, vx_uint32 index, const vx_imagepatch_addressing_t *addr);
    static void *formatImagePatchAddress2d(void *ptr, vx_uint32 x, vx_uint32 y, const vx_imagepatch_addressing_t *addr);
    static vx_bool checkImageDependenct(ExynosVisionImage *image1, ExynosVisionImage *image2);

    /* Constructor */
    ExynosVisionImage(ExynosVisionContext *context, ExynosVisionReference *scope, vx_bool is_virtual);

    /* Destructor */
    virtual ~ExynosVisionImage();

    /* copy member variables that initialized at init() */
    void operator=(const ExynosVisionImage& src_image);
    virtual vx_status init(vx_uint32 width, vx_uint32 height, vx_df_image color);
    virtual vx_status destroy(void);

    vx_uint32 computePlaneOffset(vx_uint32 x, vx_uint32 y, vx_uint32 p);
    virtual ExynosVisionImage *locateROI(vx_rectangle_t *rect);

    vx_status fillUniformValue(const void *value);

    virtual vx_status verifyMeta(ExynosVisionMeta *meta);
    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
    void displayInfoMemory(void);

    virtual vx_status accessImagePatch(const vx_rectangle_t *rect,
                                                        vx_uint32 plane_index,
                                                        vx_imagepatch_addressing_t *addr,
                                                        void **ptr,
                                                        vx_enum usage);
    virtual vx_status commitImagePatch(vx_rectangle_t *rect,
                                                vx_uint32 plane_index,
                                                vx_imagepatch_addressing_t *addr,
                                                const void *ptr);
    virtual vx_status accessImageHandle(vx_uint32 plane_index,
                                                                vx_int32 *fd,
                                                                vx_rectangle_t *rect,
                                                                vx_enum usage);
    virtual vx_status commitImageHandle(vx_uint32 plane_index,
                                                    const vx_int32 fd);

    vx_size computeImagePatchSize(const vx_rectangle_t *rect,
                                       vx_uint32 plane_index);

    vx_status getValidRegionImage(vx_rectangle_t *rect);
    vx_status getDimension(vx_uint32 *width, vx_uint32 *height);
    vx_status queryImage(vx_enum attribute, void *ptr, vx_size size);
    vx_status releaseImage(void);
    vx_status setImageAttribute(vx_enum attribute, const void *ptr, vx_size size);
    virtual vx_status swapImageHandle(void *const new_ptrs[], void *prev_ptrs[], vx_size num_planes);

    vx_status setDirective(vx_enum directive);
    vx_status setImportType(vx_enum import_type)
    {
        m_import_type = import_type;
        return VX_SUCCESS;
    }

    virtual vx_status pushImagePatch(vx_uint32 index, void *ptrs[], vx_uint32 num_buf)
    {
        VXLOGE("%s can't support this type, %d, %p, %d", getName(), index, ptrs, num_buf);
        displayInfo(0, vx_true_e);

        return VX_FAILURE;
    }
    virtual vx_status pushImageHandle(vx_uint32 index, vx_int32 fd[], vx_uint32 num_buf)
    {
        VXLOGE("%s can't support this type, %d, %p, %d", getName(), index, fd, num_buf);
        displayInfo(0, vx_true_e);

        return VX_FAILURE;
    }
    virtual vx_status popImage(vx_uint32 *ret_index, vx_bool *ret_data_valid)
    {
        VXLOGE("%s can't support this type, %p, %p", getName(), ret_index, ret_data_valid);
        displayInfo(0, vx_true_e);

        return VX_FAILURE;
    }

    /* resource management function */
    void copyMemoryInfo(const ExynosVisionImage *src_image);
    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    virtual vx_status allocateResource(image_resource_t **ret_resource);
    virtual vx_status freeResource(image_resource_t *buf_vector);

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

    virtual vx_status createCloneObject(image_resource_t *buf_vector);
    virtual vx_status destroyCloneObject(image_resource_t *buf_vector);

    virtual vx_status registerRoiDescendant(ExynosVisionImageROI *roi_descendant);

    image_resource_t* getCurResource(void) {
        return m_cur_res;
    }
#ifdef USE_OPENCL_KERNEL
    vx_status getClMemoryInfo(cl_context context, vxcl_mem_t **memory);
#endif
};

class ExynosVisionImageROI : public ExynosVisionImage {
private:
    /*! \brief A pointer to a parent image object. */
    ExynosVisionImage   *m_roi_parent;

    vx_rectangle_t m_rect_info;

public:
    /* Constructor */
    ExynosVisionImageROI(ExynosVisionContext *context, ExynosVisionReference *scope);

    virtual vx_status destroy(void);

    virtual vx_status init(vx_uint32 width, vx_uint32 height, vx_df_image color);
    vx_status initFromROI(ExynosVisionImage *parent_image, const vx_rectangle_t *rect);

    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);

    virtual ExynosVisionImage *locateROI(vx_rectangle_t *rect);
    virtual vx_status registerRoiDescendant(ExynosVisionImageROI *roi_descendant);

    virtual vx_status accessImagePatch(const vx_rectangle_t *rect,
                                                                    vx_uint32 plane_index,
                                                                    vx_imagepatch_addressing_t *addr,
                                                                    void **ptr,
                                                                    vx_enum usage);
    virtual vx_status commitImagePatch(vx_rectangle_t *rect,
                                                            vx_uint32 plane_index,
                                                            vx_imagepatch_addressing_t *addr,
                                                            const void *ptr);
    virtual vx_status accessImageHandle(vx_uint32 plane_index,
                                                                vx_int32 *fd,
                                                                vx_rectangle_t *rect,
                                                                vx_enum usage);
    virtual vx_status commitImageHandle(vx_uint32 plane_index,
                                                    const vx_int32 fd);

    virtual ExynosVisionDataReference* getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputExclusiveRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference* getOutputExclusiveRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid);
};

class ExynosVisionImageHandle : public ExynosVisionImage {
public:
    /* Constructor */
    ExynosVisionImageHandle(ExynosVisionContext *context, ExynosVisionReference *scope);

    virtual vx_status destroy(void);

    virtual vx_status swapImageHandle(void *const new_ptrs[], void *prev_ptrs[], vx_size num_planes);

    vx_status checkContinuous(vx_df_image format, void *ptrs[]);

    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
    vx_status importMemory(vx_imagepatch_addressing_t addrs[], void *ptrs[], vx_enum import_type);
};

class ExynosVisionImageQueue : public ExynosVisionImage {
public:
    /* Constructor */
    ExynosVisionImageQueue(ExynosVisionContext *context, ExynosVisionReference *scope);

    virtual vx_status allocateMemory(void);

    virtual vx_bool isQueue(void)
    {
        return vx_true_e;
    }

    virtual vx_status pushImagePatch(vx_uint32 index, void *ptrs[], vx_uint32 num_buf);
    virtual vx_status pushImageHandle(vx_uint32 index, vx_int32 fd[], vx_uint32 num_buf);
    virtual vx_status popImage(vx_uint32 *ret_index, vx_bool *ret_data_valid);

    virtual ExynosVisionDataReference* getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputExclusiveRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference* getOutputExclusiveRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid);
};

class ExynosVisionImageClone : public ExynosVisionImage {
public:
    /* Constructor */
    ExynosVisionImageClone(ExynosVisionContext *context, ExynosVisionReference *scope);

    virtual vx_status destroy(void);

    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);
};

}; // namespace android
#endif
