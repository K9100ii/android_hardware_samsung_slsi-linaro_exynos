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

#ifndef _VX_REF_INTERNAL_H_
#define _VX_REF_INTERNAL_H_

/*! \brief A magic value to look for and set in references.
 * \ingroup group_int_defines
 */
#define VX_MAGIC            (0xFACEC0DE)
#define VX_BAD_MAGIC (42)

#define VX_INT_MAX_PATH     (256)

#define VX_MAX_REFERENCE_NAME (64)

#define VX_MAX_TARGET_NAME (64)

#define VX_MAX_SUBGRAPH_NAME (64)


/*! \brief Maximum number of parameters to a kernel.
 * \ingroup group_int_defines
 */
#define VX_INT_MAX_PARAMS   (10)

/*! \brief The largest convolution matrix the specification requires support for is 15x15.
 * \ingroup group_int_defines
 */
#define VX_INT_MAX_CONVOLUTION_DIM (15)

/*! \brief The largest optical flow pyr LK window.
 * \ingroup group_int_defines
 */
#define VX_OPTICALFLOWPYRLK_MAX_DIM (9)

/*! A convenience typedef for void pointers.
 * \ingroup group_int_types
 */
typedef void *vx_ptr_t;

typedef void *vx_module_handle_t;
/*! A POSIX symbol handle.
 * \ingroup group_int_osal
 */
typedef void *vx_symbol_t;

typedef struct _vx_module_t {
    /*! \brief The name of the module. */
    vx_char             name[VX_INT_MAX_PATH];
    /*! \brief The module handle */
    vx_module_handle_t  handle;
} vx_module_t;

/*! \brief The kernel attributes structure.
 * \ingroup group_int_kernel
 */
typedef struct _vx_kernel_attr_t {
    /*! \brief The local data size for this kernel */
    vx_size       localDataSize;
    /*! \brief The local data pointer for this kernel */
    vx_ptr_t      localDataPtr;
    /*! \brief The global data size for the kernel */
    vx_size       globalDataSize;
    /*! \brief The global data pointer for this kernel */
    vx_ptr_t      globalDataPtr;
    /*! \brief The border mode of this node */
    vx_border_mode_t borders;
} vx_kernel_attr_t;

typedef struct _vx_target_info_t {
    vx_enum target_enum;
    vx_char target_name[VX_MAX_TARGET_NAME];
} vx_target_info_t;

/*! \brief Used to determine if a type is a scalar.
 * \ingroup group_int_macros
 */
#define VX_TYPE_IS_SCALAR(type) (VX_TYPE_INVALID < (type) && (type) < VX_TYPE_SCALAR_MAX)

/*! \brief Used to determine if a type is a struct.
 * \ingroup group_int_macros
 */
#define VX_TYPE_IS_STRUCT(type) ((type) >= VX_TYPE_RECTANGLE && (type) < VX_TYPE_STRUCT_MAX)

/*! \brief Used to determine if a type is an object.
 * \ingroup group_int_macros
 */
#define VX_TYPE_IS_OBJECT(type) ((type) >= VX_TYPE_REFERENCE && (type) < VX_TYPE_OBJECT_MAX)

/*! A parameter checker for size and alignment.
 * \ingroup group_int_macros
 */
#define VX_CHECK_PARAM(ptr, size, type, align) (size == sizeof(type) && ((vx_size)ptr & align) == 0)

#endif
