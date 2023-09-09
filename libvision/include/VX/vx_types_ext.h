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

#ifndef _OPENVX_TYPES_EXT_H_
#define _OPENVX_TYPES_EXT_H_

enum vx_node_attribute_ext_e {
    /*! \brief Queries the current processing frame number
     * Use a vx_uint32 parameter.
     */
    VX_NODE_ATTRIBUTE_FRAME_NUMBER = VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_NODE) + 0x0,

    VX_NODE_ATTRIBUTE_LOCAL_INFO_PTR = VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_NODE) + 0x1,
    /*! \brief Queries the connected kernel name. Use a <tt>\ref const vx_char*</tt> parameter. */
    VX_NODE_ATTRIBUTE_KERNEL_NAME =  VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_NODE) + 0x2,
    /*! \brief Queries the priority of node. Use a <tt>\ref const vx_uint32</tt> parameter. */
    VX_NODE_ATTRIBUTE_PRIORITY =  VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_NODE) + 0x3,
    /*! \brief Queries the resource policy of node. Use a <tt>\ref const vx_bool</tt> parameter. */
    VX_NODE_ATTRIBUTE_SHARE_RESOURCE =  VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_NODE) + 0x4,
};

enum vx_target_ext_e {
    VX_TARGET_VPU = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_TARGET) + 0x0,
    VX_TARGET_CPU = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_TARGET) + 0x1,
    VX_TARGET_GPU = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_TARGET) + 0x2,
    VX_TARGET_DSP = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_TARGET) + 0x3,
};

enum vx_image_attribute_ext_e {
    /*! \brief Queries an image for its import type. Use a <tt>\ref vx_enum</tt> parameter. */
    VX_IMAGE_ATTRIBUTE_IMPORT = VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_IMAGE) + 0x0,
    /*! \brief Queries an image for its number of memories. Use a <tt>\ref vx_size</tt> parameter. */
    VX_IMAGE_ATTRIBUTE_MEMORIES = VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_IMAGE) + 0x1,
};

enum vx_df_image_ext_e {
    /*! \brief A single plane of 24-bit pixel as 3 interleaved 8-bit units of
     * R then G then B data. This uses the BT709 full range by default.
     */
    VX_DF_IMAGE_BGR  = VX_DF_IMAGE('B','G','R','2'),
    /*! \brief A single plane of 32-bit pixel as 4 interleaved 8-bit units of
     * R then G then B data, then a <i>don't care</i> byte.
     * This uses the BT709 full range by default.
     */
    VX_DF_IMAGE_BGRX = VX_DF_IMAGE('B','G','R','A'),
}; 

enum vx_array_attribute_ext_e {
    /*! \brief Queries an array stride. Use a <tt>\ref vx_size</tt> parameter. */
    VX_ARRAY_ATTRIBUTE_STRIDE = VX_ATTRIBUTE_BASE(VX_ID_SAMSUNG, VX_TYPE_ARRAY) + 0x0,
};

enum vx_hint_ext_e {
    /*! \brief Indicates to the implementation that the user wants to disable
     * any parallelization techniques. Implementations may not be parallelized,
     * so this is a hint only.
     */
    VX_HINT_STREAM = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_HINT) + 0x0,
};

enum vx_directive_ext_e {
    /*! \brief Indicate that allocate continous memory for multi-plane. */
    VX_DIRECTIVE_IMAGE_CONTINUOUS = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_DIRECTIVE) + 0x0,
};

/*! \brief An enumeration of memory import types.
 * \ingroup group_context
 */
enum vx_import_type_ext_e {
    /*! \brief The ion memory type to import from the Host. */
    VX_IMPORT_TYPE_ION = VX_ENUM_BASE(VX_ID_SAMSUNG, VX_ENUM_IMPORT_MEM) + 0x0,
};

/*!
 * \brief The entry point into modules loaded by <tt>\ref vxLoadKernels</tt>.
 * \param [in] context The handle to the implementation context.
 * \note The symbol exported from the user module must be <tt>vxModuleInitializer</tt> in extern C format.
 * \ingroup group_user_kernels
 */
typedef vx_status (VX_API_CALL *vx_module_initializer_f)(vx_context context);

/*!
 * \brief The release point for modules.
 * \note The symbol exported from the user module must be <tt>vxModuleDeinitializer</tt> in extern C format.
 * \ingroup group_user_kernels
 */
typedef vx_status (VX_API_CALL *vx_module_deinitializer_f)(void);

#endif

