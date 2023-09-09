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

#define LOG_TAG "ExynosEvfNodeInterface"
#include <cutils/log.h>

#include "ExynosVisionCommonConfig.h"

#include <VX/vx.h>
#include <VX/vx_internal.h>

#include "ExynosVisionContext.h"
#include "ExynosVisionGraph.h"
#include "ExynosVisionNode.h"
#include "ExynosVisionImage.h"

#define EXYNOS_VISION_VPU_API_TRACE
#ifdef EXYNOS_VISION_VPU_API_TRACE
#define EXYNOS_VISION_VPU_API_IN()   VXLOGD("IN..", __FUNCTION__, __LINE__)
#define EXYNOS_VISION_VPU_API_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_VPU_API_IN()   ((void *)0)
#define EXYNOS_VISION_VPU_API_OUT()  ((void *)0)
#endif

#include <VX/vx_api.h>
using namespace std;

using namespace android;

VX_API_ENTRY vx_node VX_API_CALL vxSaitFaceRecognitionNode(vx_graph graph, vx_image input, vx_image output)
{
    vx_reference params[] = {
        (vx_reference)input,
        (vx_reference)output,
    };
    return vxCreateNodeByStructure(graph, VX_KERNEL_COLOR_CONVERT, params, dimof(params));
}

