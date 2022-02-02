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

#ifndef EXYNOS_SCORE_KERNEL_UTIL_H
#define EXYNOS_SCORE_KERNEL_UTIL_H

#include <VX/vx.h>

#include "ExynosScoreKernel.h"

using namespace android;
using namespace score;

#ifdef __cplusplus
extern "C" {
#endif
vx_status getScVxObjectHandle(vx_reference ref, vx_int32 *fd, vx_enum usage);
vx_status putScVxObjectHandle(vx_reference ref, vx_int32 fd);

vx_status getScImageParamInfo(vx_image image, ScBuffer *sc_buf);
vx_status getScParamInfo(vx_reference ref, ScBuffer *sc_buf);


#ifdef __cplusplus
}
#endif

#endif
