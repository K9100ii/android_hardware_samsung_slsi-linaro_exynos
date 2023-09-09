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

#ifndef _VXCL_KERNEL_MODULE_H_
#define _VXCL_KERNEL_MODULE_H_

#include "vxcl_kernel_util.h"
#include "vxcl_kernel_extra.h"


/* OpenVX primitive kernels */
extern vx_cl_kernel_description_t warp_affine_clkernel;
extern vx_cl_kernel_description_t warp_perspective_clkernel;
extern vx_cl_kernel_description_t remap_clkernel;

/* User kernels */
extern vx_cl_kernel_description_t warp_perspective_inverse_rgb_clkernel;

#ifdef __cplusplus
extern "C" {
#endif
VX_API_ENTRY vx_status VX_API_CALL vxModuleInitializer(vx_context context);
VX_API_ENTRY vx_status VX_API_CALL vxModuleDeinitializer(void);
VX_API_ENTRY vx_status VX_API_CALL vxPublishKernels(vx_context context);


#ifdef __cplusplus
}
#endif

#endif
