/*
 * Copyright (C) 2020 Samsung Electronics Co. Ltd.
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

#ifndef EXYNOS_GRAPHIC_BUFFER_MALI_H_
#define EXYNOS_GRAPHIC_BUFFER_MALI_H_

#include "ExynosGraphicBufferCore.h"
#include <vector>

namespace vendor {
namespace graphics {

class ExynosGraphicBufferMali : public ExynosGraphicBufferMeta {
  public:
    struct Rect {
        int32_t left;
        int32_t top;
        int32_t right;
        int32_t bottom;
    };

    static int get_num_planes(buffer_handle_t);
    static uint32_t get_offsetInBytes(buffer_handle_t, int plane_num);
    static uint32_t get_strideInBytes(buffer_handle_t, int plane_num);
    static uint32_t get_widthInSamples(buffer_handle_t, int plane_num);
    static uint32_t get_heightInSamples(buffer_handle_t, int plane_num);
    static uint32_t get_pixel_format_fourcc(buffer_handle_t);
    static uint64_t get_pixel_format_modifier(buffer_handle_t);
    static std::vector<Rect> get_crop_rects(buffer_handle_t);
    static uint64_t get_total_alloc_size(buffer_handle_t);
    static uint32_t get_layer_count(buffer_handle_t);
    static int get_plane_fd(buffer_handle_t, int plane_num);
    static bool check_plane_is_on_new_fd(buffer_handle_t, int plane_num);
    static bool check_usage_afbc_padding(buffer_handle_t, uint64_t usage);
};

} /* namespace graphics */
} /* namespace vendor */

#endif /* EXYNOS_GRAPHIC_BUFFER_MALI_H_ */
