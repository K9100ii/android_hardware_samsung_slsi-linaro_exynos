/*
 * Copyright (C) 2016, Samsung Electronics Co. LTD
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

#ifdef USE_OPENCL_KERNEL

#ifndef _OPENVX_EXT_OPENCL_H_
#define _OPENVX_EXT_OPENCL_H_

#include <CL/cl.h>

#ifdef __cplusplus
extern "C" {
#endif

enum vxcl_plane_e {
    VX_CL_PLANE_0 = 0,
    VX_CL_PLANE_1,
    VX_CL_PLANE_2,
    VX_CL_PLANE_3,
    VX_CL_PLANE_MAX,
};

enum vxcl_dim_e {
    /*! \brief Channels dimension, stride */
    VX_CL_DIM_C = 0,
    /*! \brief Width (dimension) or x stride */
    VX_CL_DIM_X,
    /*! \brief Height (dimension) or y stride */
    VX_CL_DIM_Y,
    /*! \brief [hidden] The maximum number of dimensions */
    VX_CL_DIM_MAX,
};

#define VX_CL_MAX_ARGUMENTS (10)

#define VX_NUM_DEFAULT_PLANES (1)
#define VX_NUM_DEFAULT_DIMS   (2)

#define VX_NUM_REMAP_DIMS (3)

typedef struct _vxcl_memory_t {
    /*! \brief Determines if this memory was allocated by the system */
    vx_bool        allocated;
    /*! \brief The number of pointers in the array */
    vx_uint32      nptrs;
    /*! \brief The array of pointers (one per plane for images) */
    vx_uint8*      ptrs[VX_CL_PLANE_MAX];
    /*! \brief The number of dimensions per ptr */
    vx_uint32      ndims;
    /*! \brief The dimensional values per ptr */
    vx_uint32      dims[VX_CL_PLANE_MAX][VX_CL_DIM_MAX];
    /*! \brief The per ptr stride values per dimension */
    vx_uint32      strides[VX_CL_PLANE_MAX][VX_CL_DIM_MAX];
    vx_size        sizes[VX_CL_PLANE_MAX];
    /*! \brief This contains the OpenCL memory references */
    cl_mem hdls[VX_CL_PLANE_MAX];
    /*! \brief This describes the type of memory allocated with OpenCL */
    cl_mem_object_type cl_type;
    /*! \brief This describes the image format (if it is an image) */
    cl_image_format cl_format;
} vxcl_mem_t;


/*! \brief Bring memory information for operating OpenCL Kernels
 * \param [in]  parameter Input/output data for node.
 * \param [in]  type      Data type of parameter.
 * \param [in]  context   CL context for create buffer
 * \param [out] memory    The memory information for cl kernel
 * \return Returns a \ref vx_status_e enumeration.
 * \ingroup group_helper
 */
VX_API_ENTRY vx_status VX_API_CALL vxGetClMemoryInfo(vx_reference parameter, vx_enum type, cl_context context, vxcl_mem_t **memory);

#ifdef __cplusplus
}
#endif

#endif

#endif
