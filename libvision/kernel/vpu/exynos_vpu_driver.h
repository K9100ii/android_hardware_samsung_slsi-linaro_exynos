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

#ifndef EXYNOS_VPU_DRIVER_H
#define EXYNOS_VPU_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vs4l.h"

int exynos_vpu_open(const char *filename, int oflag, ...);
int exynos_vpu_close(int fd);
int exynos_vpu_s_graph(int fd, struct vs4l_graph *graph);
int exynos_vpu_s_format(int fd, struct vs4l_format_list *format_list);
int exynos_vpu_s_param(int fd, struct vs4l_param_list *param_list);
int exynos_vpu_stream_on(int fd);
int exynos_vpu_stream_off(int fd);
int exynos_vpu_qbuf(int fd, struct vs4l_container_list *buf);
int exynos_vpu_dqbuf(int fd, struct vs4l_container_list *buf);

#ifdef __cplusplus
}
#endif

#endif
