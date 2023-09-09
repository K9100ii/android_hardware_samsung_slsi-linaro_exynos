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

#include "vpu_kernel_extra.h"

vx_node vxExample1KernelNode(vx_graph graph, vx_image input, vx_image output)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)output
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_EXTRAS_EXAMPLE1_KERNEL,
                                   params,
                                   dimof(params));
}

vx_node vxSaitFrEarlyAcceptNode(vx_graph graph, vx_image input, vx_image output0, vx_image output1)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)output0,
        (vx_reference)output1
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_EXTRAS_SAITFR_EA,
                                   params,
                                   dimof(params));
}

vx_node vxSaitFrLaterAcceptNode(vx_graph graph, vx_image input, vx_image output0, vx_image output1, vx_image output2)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)output0,
        (vx_reference)output1,
        (vx_reference)output2
    };
    return vxCreateNodeByStructure(graph,
                                   VX_KERNEL_EXTRAS_SAITFR_LA,
                                   params,
                                   dimof(params));
}

