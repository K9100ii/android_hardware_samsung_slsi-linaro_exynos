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

#ifndef EXYNOS_VPU_KERNEL_UTIL_H
#define EXYNOS_VPU_KERNEL_UTIL_H

#include <utils/List.h>

#include <VX/vx.h>

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"

using namespace android;

#ifdef __cplusplus
extern "C" {
#endif

vx_status getVxObjectHandle(struct io_vx_info_t vx_param, vx_reference ref, Vector<io_buffer_t> *container, vx_enum usage);
vx_status putVxObjectHandle(struct io_vx_info_t vx_param, vx_reference ref, Vector<io_buffer_t> *container);

vx_status getIoInfoFromVx(vx_reference ref, vx_param_info_t param_info, List<struct io_format_t> *io_format_list, struct io_memory_t *io_memory);

vx_status convertFloatToMultiShift(vx_float32 float_value, vx_size *shift_value, vx_size *multiply_value);

vx_status copyArray(vx_array src, vx_array dst);

#ifdef __cplusplus
}
#endif

#endif
