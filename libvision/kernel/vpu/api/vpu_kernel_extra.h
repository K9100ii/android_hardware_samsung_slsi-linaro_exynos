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

#ifndef EXYNOS_VPU_KERNEL_EXTRA_H
#define EXYNOS_VPU_KERNEL_EXTRA_H

#ifdef __cplusplus
extern "C" {
#endif

#define VX_LIBRARY_SAMSUNG_EXTRAS (0x2)

enum vx_kernel_vpu_extra_e {
    VX_KERNEL_EXTRAS_EXAMPLE1_KERNEL = VX_KERNEL_BASE(VX_ID_SAMSUNG, VX_LIBRARY_SAMSUNG_EXTRAS) + 0x0,
    VX_KERNEL_EXTRAS_SAITFR_EA = VX_KERNEL_BASE(VX_ID_SAMSUNG, VX_LIBRARY_SAMSUNG_EXTRAS) + 0x1,
    VX_KERNEL_EXTRAS_SAITFR_LA = VX_KERNEL_BASE(VX_ID_SAMSUNG, VX_LIBRARY_SAMSUNG_EXTRAS) + 0x2,
};

vx_node vxExample1KernelNode(vx_graph graph, vx_image input, vx_image output);
vx_node vxSaitFrEarlyAcceptNode(vx_graph graph, vx_image input, vx_image output0, vx_image output1);
vx_node vxSaitFrLaterAcceptNode(vx_graph graph, vx_image input, vx_image output0, vx_image output1, vx_image output2);

#ifdef __cplusplus
}
#endif

#endif
