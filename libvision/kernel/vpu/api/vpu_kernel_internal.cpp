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

#define LOG_TAG "ExynosVpuKernelInterface"
#include <cutils/log.h>

#include <VX/vx.h>
#include <VX/vx_helper.h>

#include "vpu_kernel_internal.h"

vx_node vxFastCornesMicro0Node(vx_graph graph, vx_image input, vx_scalar strength_thresh, vx_scalar nonmax_suppression, vx_image output)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)strength_thresh,
        (vx_reference)nonmax_suppression,
        (vx_reference)output
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_INTERNAL_FAST_CORNERS0_MICRO,
                                   params,
                                   dimof(params));
}

vx_node vxFastCornesMicro1Node(vx_graph graph, vx_image in1, vx_image in2, vx_scalar strength_thresh, vx_scalar nonmax_suppression, vx_array corners, vx_scalar num_corners)
{
    vx_reference params[] = {
        (vx_reference)in1,
        (vx_reference)in2,
        (vx_reference)strength_thresh,
        (vx_reference)nonmax_suppression,
        (vx_reference)corners,
        (vx_reference)num_corners
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_INTERNAL_FAST_CORNERS1_MICRO,
                                   params,
                                   dimof(params));
}

