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

#define LOG_TAG "ExynosGraphicBufferMali"

#include <ExynosGraphicBufferMali.h>
#include <drmutils.h>
#include <exynos_format.h>
#include <hidl_common/SharedMetadata.h>
#include <log/log.h>

#define HIDL_COMMON_VERSION_SCALED 120
#include <mali_gralloc_usages.h>

using namespace vendor::graphics;
using arm::mapper::common::get_crop_rect;
typedef aidl::android::hardware::graphics::common::Rect aidlRect;

#define convert_handle(gralloc_hnd, buffer_handle, retval)                                                             \
    const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(buffer_handle);                        \
    if (!gralloc_hnd)                                                                                                  \
        return retval;

int ExynosGraphicBufferMali::get_num_planes(const buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    switch (gralloc_hnd->get_alloc_format()) {
        case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M:
            return 3;

        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_10B_SBWC:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L50:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L75:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L50:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L75:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L40:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L60:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L80:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L40:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L60:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L80:
            return 2;
    }

    return gralloc_hnd->is_multi_plane() ? (gralloc_hnd->plane_info[2].offset == 0 ? 2 : 3) : 1;
}

uint32_t ExynosGraphicBufferMali::get_offsetInBytes(buffer_handle_t hnd, int plane_num) {
    return ExynosGraphicBufferMeta::get_plane_offset(hnd, plane_num);
}

uint32_t ExynosGraphicBufferMali::get_strideInBytes(buffer_handle_t hnd, int plane_num) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->plane_info[plane_num].byte_stride;
}

uint32_t ExynosGraphicBufferMali::get_widthInSamples(buffer_handle_t hnd, int plane_num) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->plane_info[plane_num].alloc_width;
}

uint32_t ExynosGraphicBufferMali::get_heightInSamples(buffer_handle_t hnd, int plane_num) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->plane_info[plane_num].alloc_height;
}

uint32_t ExynosGraphicBufferMali::get_pixel_format_fourcc(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return drm_fourcc_from_handle(gralloc_hnd);
}

uint64_t ExynosGraphicBufferMali::get_pixel_format_modifier(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return drm_modifier_from_handle(gralloc_hnd);
}

std::vector<ExynosGraphicBufferMali::Rect> ExynosGraphicBufferMali::get_crop_rects(buffer_handle_t hnd) {
    convert_handle(handle, hnd, {});

    const int num_planes = get_num_planes(hnd);
    std::vector<ExynosGraphicBufferMali::Rect> crops(num_planes);

    for (size_t plane_index = 0; plane_index < num_planes; ++plane_index) {
        /* Set the default crop rectangle. Android mandates that it must fit [0, 0, widthInSamples, heightInSamples]
         * We always require using the requested width and height for the crop rectangle size.
         * For planes > 0 the size might need to be scaled, but since we only use plane[0] for crop set it to the
         * Android default of [0, 0, widthInSamples, heightInSamples] for other planes.
         */
        aidl::android::hardware::graphics::common::Rect rect = {
            .top = 0,
            .left = 0,
            .right = static_cast<int32_t>(handle->plane_info[plane_index].alloc_width),
            .bottom = static_cast<int32_t>(handle->plane_info[plane_index].alloc_height)};

        if (plane_index == 0) {
            std::optional<aidlRect> crop_rect;
            get_crop_rect(handle, &crop_rect);
            if (crop_rect.has_value()) {
                rect = crop_rect.value();
            } else {
                rect = {.top = 0, .left = 0, .right = handle->width, .bottom = handle->height};
            }
        }
        crops[plane_index].top = rect.top;
        crops[plane_index].left = rect.left;
        crops[plane_index].right = rect.right;
        crops[plane_index].bottom = rect.bottom;
    }

    return crops;
}

uint64_t ExynosGraphicBufferMali::get_total_alloc_size(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    uint64_t total_size = 0;
    for (int fidx = 0; fidx < gralloc_hnd->fd_count; fidx++) {
        total_size += gralloc_hnd->alloc_sizes[fidx];
    }

    return total_size;
}

uint32_t ExynosGraphicBufferMali::get_layer_count(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->layer_count;
}

bool ExynosGraphicBufferMali::check_plane_is_on_new_fd(buffer_handle_t hnd, int plane) {
    convert_handle(gralloc_hnd, hnd, false);

    return gralloc_hnd->plane_info[plane].fd_idx == plane;
}

int ExynosGraphicBufferMali::get_plane_fd(buffer_handle_t hnd, int plane) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    //#define MALI_GRALLOC_INTFMT_FMT_MASK  0x00000000ffffffffULL
    uint64_t base_format = gralloc_hnd->alloc_format & MALI_GRALLOC_INTFMT_FMT_MASK;

    switch (base_format) {
        case HAL_PIXEL_FORMAT_EXYNOS_YV12_M: {
            if (plane == 1) {
                return gralloc_hnd->fds[2];
            } else if (plane == 2) {
                return gralloc_hnd->fds[1];
            }
        } break;
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M: // HAL_PIXEL_FORMAT_I420:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M: // HAL_PIXEL_FORMAT_YCbCr_420_SP: //HAL_PIXEL_FORMAT_NV12:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL: // HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED: // HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        {
            if (plane == 1) {
                return gralloc_hnd->fds[1];
            } else if (plane == 2) {
                return gralloc_hnd->fds[2];
            }
        }
    }

    return gralloc_hnd->fds[0];
}

bool ExynosGraphicBufferMali::check_usage_afbc_padding(buffer_handle_t hnd, uint64_t usage) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return (usage & MALI_GRALLOC_USAGE_AFBC_PADDING) == MALI_GRALLOC_USAGE_AFBC_PADDING;
}

/* Mali interface end */
