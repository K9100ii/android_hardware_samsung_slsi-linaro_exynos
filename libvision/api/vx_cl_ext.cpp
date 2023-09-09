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
#ifdef USE_OPENCL_KERNEL

#define LOG_TAG "ExynosEvfClInterface"
#include <cutils/log.h>
#include <VX/vx.h>

#include "ExynosVisionReference.h"
#include "ExynosVisionGraph.h"
#include "ExynosVisionImage.h"
#include "ExynosVisionLut.h"
#include "ExynosVisionDistribution.h"
#include "ExynosVisionArray.h"
#include "ExynosVisionConvolution.h"
#include "ExynosVisionRemap.h"
#include "ExynosVisionMatrix.h"

 using namespace android;

 VX_API_ENTRY vx_status VX_API_CALL vxGetClMemoryInfo(vx_reference parameter, vx_enum type, cl_context context, vxcl_mem_t **memory)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)parameter, type) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", parameter);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    switch (type)
    {
        case VX_TYPE_IMAGE:
            VXLOGD3("parameter is VX_TYPE_IMAGE(0x%X)", type);
            status = ((ExynosVisionImage*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_ARRAY:
            VXLOGD3("parameter is VX_TYPE_ARRAY(0x%X)", type);
            status = ((ExynosVisionArray*)parameter)->getClMemoryInfo(context, memory);
        case VX_TYPE_LUT:
            VXLOGD3("parameter is VX_TYPE_LUT(0x%X)", type);
            status = ((ExynosVisionLut*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_MATRIX:
            VXLOGD3("parameter is VX_TYPE_MATRIX(0x%X)", type);
            status = ((ExynosVisionMatrix*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_CONVOLUTION:
            VXLOGD3("parameter is VX_TYPE_CONVOLUTION(0x%X)", type);
            status = ((ExynosVisionConvolution*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_DISTRIBUTION:
            VXLOGD3("parameter is VX_TYPE_DISTRIBUTION(0x%X)", type);
            status = ((ExynosVisionDistribution*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_REMAP:
            VXLOGD3("parameter is VX_TYPE_REMAP(0x%X)", type);
            status = ((ExynosVisionRemap*)parameter)->getClMemoryInfo(context, memory);
            break;
        case VX_TYPE_SCALAR:
            VXLOGD3("parameter is VX_TYPE_SCALAR(0x%X)", type);
            *memory = NULL;
            break;
        case VX_TYPE_THRESHOLD:
            VXLOGD3("parameter is VX_TYPE_THRESHOLD(0x%X)", type);
            *memory = NULL;
            break;
        case VX_TYPE_PYRAMID:
            VXLOGD3("parameter is VX_TYPE_PYRAMID(0x%X)", type);
            *memory = NULL;
            break;
        default:
            VXLOGE("parameter is INVALID_TYPE(0x%X)", type);
            status = VX_ERROR_INVALID_TYPE;
            *memory = NULL;
            break;
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

#endif
