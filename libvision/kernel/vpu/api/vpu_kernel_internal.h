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

#ifndef EXYNOS_VPU_KERNEL_INTERNAL_H
#define EXYNOS_VPU_KERNEL_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#define VX_LIBRARY_SAMSUNG_INTERNAL (0x3)

enum vx_kernel_vpu_extra_e {
    VX_KERNEL_INTERNAL_FAST_CORNERS0_MICRO = VX_KERNEL_BASE(VX_ID_SAMSUNG, VX_LIBRARY_SAMSUNG_INTERNAL) + 0x0,
    VX_KERNEL_INTERNAL_FAST_CORNERS1_MICRO = VX_KERNEL_BASE(VX_ID_SAMSUNG, VX_LIBRARY_SAMSUNG_INTERNAL) + 0x1
};

vx_node vxFastCornesMicro0Node(vx_graph graph, vx_image input, vx_scalar strength_thresh, vx_scalar nonmax_suppression, vx_image output);
vx_node vxFastCornesMicro1Node(vx_graph graph, vx_image in1, vx_image in2, vx_scalar strength_thresh, vx_scalar nonmax_suppression, vx_array corners, vx_scalar num_corners);

#ifdef __cplusplus
}
#endif

#endif
