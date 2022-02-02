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

#define LOG_TAG "VxclKernelInterface"
#include <cutils/log.h>

#include <VX/vx.h>
#include <VX/vx_helper.h>

#include "vxcl_kernel_extra.h"

VX_API_ENTRY vx_node VX_API_CALL vxCLWarpPerspectiveInverseRGBNode(vx_graph graph, vx_image input, vx_matrix matr, vx_scalar inter, vx_image output)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)matr,
        (vx_reference)inter,
        (vx_reference)output,
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_WARP_PERSPECTIVE_INVERSE_RGB,
                                   params,
                                   dimof(params));
}


