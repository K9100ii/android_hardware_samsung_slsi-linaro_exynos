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
#include <log/log.h>

#include <gralloc_buffer_priv.h>

#define CDBG_ASSERT(...) (void)0;

using namespace vendor::graphics;

/* Offsets that SLSI adds to gralloc due to HW bugs */
#if MALI_SEC_MSCL_ALIGN_RESTRICTION
#define SLSI_MSCL_EXT_SIZE 512
#else
/* TODO: read this value from device config */
//#define SLSI_MSCL_EXT_SIZE                                         0
#define SLSI_MSCL_EXT_SIZE 512
#endif
#define SLSI_MSCL_ALIGN 128
#define SLSI_RAND_OFFSET 256

#define WINSYS_ALIGN(value, align) ((value + (align - 1)) & ~(align - 1))
const int GRALLOC_AFBC_PADDED_HEADER_STRIDE = 1;

#define convert_handle(gralloc_hnd, buffer_handle, retval)                                                             \
    const private_handle_t *gralloc_hnd = static_cast<const private_handle_t *>(buffer_handle);                        \
    if (!gralloc_hnd)                                                                                                  \
        return retval;

#ifndef DRM_FORMAT_MOD_ARM_TYPE_MASK
#define DRM_FORMAT_MOD_ARM_TYPE_MASK 0xf
#endif

#define fourcc_mod_code_get_vendor(val) ((val) >> 56)
#define fourcc_mod_code_get_type(val) (((val) >> 52) & DRM_FORMAT_MOD_ARM_TYPE_MASK)

/* Returns true if the modifier describes an AFBC format. */
static inline bool drm_fourcc_modifier_is_afbc(uint64_t modifier) {
    uint32_t vendor = fourcc_mod_code_get_vendor(modifier);
    uint32_t type = fourcc_mod_code_get_type(modifier);
    return DRM_FORMAT_MOD_VENDOR_ARM == vendor && DRM_FORMAT_MOD_ARM_TYPE_AFBC == type;
}

/* Returns the number of planes represented by a fourcc format. */
static inline uint32_t drm_fourcc_format_get_num_planes(uint32_t format) {
    switch (format) {
        default:
            return 0;

        case DRM_FORMAT_C8:
        case DRM_FORMAT_R8:
        case DRM_FORMAT_RGB332:
        case DRM_FORMAT_BGR233:
        case DRM_FORMAT_XRGB4444:
        case DRM_FORMAT_XBGR4444:
        case DRM_FORMAT_RGBX4444:
        case DRM_FORMAT_BGRX4444:
        case DRM_FORMAT_ARGB4444:
        case DRM_FORMAT_ABGR4444:
        case DRM_FORMAT_RGBA4444:
        case DRM_FORMAT_BGRA4444:
        case DRM_FORMAT_XRGB1555:
        case DRM_FORMAT_XBGR1555:
        case DRM_FORMAT_RGBX5551:
        case DRM_FORMAT_BGRX5551:
        case DRM_FORMAT_ARGB1555:
        case DRM_FORMAT_ABGR1555:
        case DRM_FORMAT_RGBA5551:
        case DRM_FORMAT_BGRA5551:
        case DRM_FORMAT_RGB565:
        case DRM_FORMAT_BGR565:
        case DRM_FORMAT_RGB888:
        case DRM_FORMAT_BGR888:
        case DRM_FORMAT_XRGB8888:
        case DRM_FORMAT_XBGR8888:
        case DRM_FORMAT_RGBX8888:
        case DRM_FORMAT_BGRX8888:
        case DRM_FORMAT_ARGB8888:
        case DRM_FORMAT_ABGR8888:
        case DRM_FORMAT_RGBA8888:
        case DRM_FORMAT_BGRA8888:
        case DRM_FORMAT_XRGB2101010:
        case DRM_FORMAT_XBGR2101010:
        case DRM_FORMAT_RGBX1010102:
        case DRM_FORMAT_BGRX1010102:
        case DRM_FORMAT_ARGB2101010:
        case DRM_FORMAT_ABGR2101010:
        case DRM_FORMAT_RGBA1010102:
        case DRM_FORMAT_BGRA1010102:
        case DRM_FORMAT_YUYV:
        case DRM_FORMAT_YVYU:
        case DRM_FORMAT_UYVY:
        case DRM_FORMAT_VYUY:
        case DRM_FORMAT_AYUV:
        case DRM_FORMAT_ABGR16161616F:
        case DRM_FORMAT_AXBXGXRX106106106106:
        case DRM_FORMAT_R16:
        case DRM_FORMAT_GR1616:
        case DRM_FORMAT_Y410:
        case DRM_FORMAT_Y0L2:
        case DRM_FORMAT_Y210:
        case DRM_FORMAT_YUV420_8BIT:
        case DRM_FORMAT_YUV420_10BIT:
#if defined(MALI_PRIVATE_FOURCC) && MALI_PRIVATE_FOURCC
        case ARM_PRIV_DRM_FORMAT_AYUV2101010:
        case ARM_PRIV_DRM_FORMAT_YUYV10101010:
        case ARM_PRIV_DRM_FORMAT_VYUY10101010:
#endif /* defined(MALI_PRIVATE_FOURCC) && MALI_PRIVATE_FOURCC */
            return 1;

        case DRM_FORMAT_NV12:
        case DRM_FORMAT_NV21:
        case DRM_FORMAT_NV16:
        case DRM_FORMAT_NV61:
        case DRM_FORMAT_P010:
        case DRM_FORMAT_P210:
        case DRM_FORMAT_NV15:
#if defined(MALI_PRIVATE_FOURCC) && MALI_PRIVATE_FOURCC
        case ARM_PRIV_DRM_FORMAT_Y_UV422_10BIT:
#endif /* defined(MALI_PRIVATE_FOURCC) && MALI_PRIVATE_FOURCC */
            return 2;

        case DRM_FORMAT_YUV410:
        case DRM_FORMAT_YVU410:
        case DRM_FORMAT_YUV411:
        case DRM_FORMAT_YVU411:
        case DRM_FORMAT_YUV420:
        case DRM_FORMAT_YVU420:
        case DRM_FORMAT_YUV422:
        case DRM_FORMAT_YVU422:
        case DRM_FORMAT_YUV444:
        case DRM_FORMAT_YVU444:
        case DRM_FORMAT_Q410:
        case DRM_FORMAT_Q401:
            return 3;
    }
}

bool get_buffer_fourcc_format(buffer_handle_t handle, uint32_t *fourcc_format, uint64_t *fourcc_modifier) {
    private_handle_t *gralloc_hnd = static_cast<private_handle_t *>(const_cast<native_handle *>(handle));

    *fourcc_format = drm_fourcc_from_handle(gralloc_hnd);
    *fourcc_modifier = drm_modifier_from_handle(gralloc_hnd);

    return (*fourcc_format != 0);
}

void get_gralloc_format_info(buffer_handle_t handle, int *offsets, int *strides, int *num_planes,
                             uint32_t layer_index) {
    const private_handle_t *hnd = static_cast<const private_handle_t *>(handle);
    assert(hnd);

    int local_offsets[3] = {0, 0, 0};
    int local_strides[3] = {0, 0, 0};
    uint32_t fourcc_format;
    uint64_t fourcc_modifier;
    int local_num_planes = 0;

    /* SLSI_INTEGRAION */
    int random_offset = SLSI_RAND_OFFSET;
    if (hnd->width % SLSI_MSCL_ALIGN) {
        random_offset += SLSI_MSCL_EXT_SIZE;
    }

    int layer_size = (hnd->size - random_offset) / hnd->layer_count;
    if ((hnd->size - random_offset) % hnd->layer_count) {
        ALOGE("The layer size computation is wrong - expect errors!");
    }

    if (!get_buffer_fourcc_format(handle, &fourcc_format, &fourcc_modifier)) {
        ALOGD(" Passed in buffer is BLOB or RAW data \n");
        /* Return with buffer info stored in plane_info[0] */
        CDBG_ASSERT(!hnd->is_multi_plane());
        if (NULL != offsets) {
            offsets[0] = hnd->plane_info[0].offset + layer_index * layer_size;
        }
        if (NULL != strides) {
            strides[0] = hnd->plane_info[0].byte_stride;
        }
        if (NULL != num_planes) {
            *num_planes = 1;
        }
        return;
    }

    local_num_planes = drm_fourcc_format_get_num_planes(fourcc_format);
    CDBG_ASSERT(local_num_planes > 0);

    switch (hnd->format) {
        case HAL_PIXEL_FORMAT_EXYNOS_YV12_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P_M: // HAL_PIXEL_FORMAT_I420:
        {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride;

            /* U plane: Stride is half of Y-Stride aligned to 16 pixels. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0];
            local_strides[1] = WINSYS_ALIGN(local_strides[0] / 2, 16);

            /* V plane: Stride is same is U-Stride. Offset is U offset + U plane */
            local_offsets[2] = local_offsets[0];
            local_strides[2] = local_strides[1];

            if (hnd->format == HAL_PIXEL_FORMAT_EXYNOS_YV12_M) {
                int temp_offset = local_offsets[1];
                local_offsets[1] = local_offsets[2];
                local_offsets[2] = temp_offset;
            }

            local_num_planes = 3;
            break;
        }
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN: {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0] + WINSYS_ALIGN(hnd->height, 16) * local_strides[0] + 256;
            local_strides[1] = local_strides[0];

            local_num_planes = 2;
            break;
        }
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_S10B: {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0] + WINSYS_ALIGN(hnd->height, 16) * local_strides[0] + 256 +
                               WINSYS_ALIGN((hnd->width / 4), 16) * WINSYS_ALIGN(hnd->height, 16) + 64;
            local_strides[1] = local_strides[0];

            local_num_planes = 2;
            break;
        }
        case HAL_PIXEL_FORMAT_YCrCb_420_SP: {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0] + (hnd->height * local_strides[0]);
            local_strides[1] = local_strides[0];

            local_num_planes = 2;
            break;
        }
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_S10B:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_PRIV:
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M: // HAL_PIXEL_FORMAT_YCbCr_420_SP: //HAL_PIXEL_FORMAT_NV12:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M:
        case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_FULL: // HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_FULL
        {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0];
            local_strides[1] = local_strides[0];

            local_num_planes = 2;
            break;
        }
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M: {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset. Byte align is 2x */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride * 2;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0];
            local_strides[1] = local_strides[0];

            local_num_planes = 2;
            break;
        }
        case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_TILED: // HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        {
            /* Y-plane: Stride is aligned to 16 pixels. 0 offset */
            local_offsets[0] = hnd->offset;
            local_strides[0] = hnd->stride * 16;

            /* UV plane: Stride is equal to Y-Stride. Offset is size of Y plane */
            local_offsets[1] = local_offsets[0];
            local_strides[1] = hnd->stride * 8;

            local_num_planes = 2;
            break;
        }
        default: {
            for (int i = 0; i < local_num_planes; i++) {
                local_offsets[i] = hnd->plane_info[i].offset;
                local_strides[i] = hnd->plane_info[i].byte_stride;
                CDBG_ASSERT(hnd->plane_info[i].alloc_width);
                CDBG_ASSERT(hnd->plane_info[i].alloc_height);
            }

            uint64_t usage = hnd->producer_usage || hnd->consumer_usage;
            if (strides != nullptr && drm_fourcc_modifier_is_afbc(fourcc_modifier)) {
                strides[0] = strides[1] = strides[2] = 0;

                if (ExynosGraphicBufferMali::check_usage_afbc_padding(handle, usage)) {
                    strides[0] = GRALLOC_AFBC_PADDED_HEADER_STRIDE;
                }
            }
        }
    }

    if (NULL != offsets) {
        for (int i = 0; i < local_num_planes; i++) {
            /* The planes are laid out in each layer as they would in the first layer */
            offsets[i] = layer_index * layer_size + local_offsets[i];
        }
    }

    if (NULL != strides) {
        for (int i = 0; i < local_num_planes; i++) {
            /* The stride are not affected by multiple layers */
            strides[i] = local_strides[i];
        }
    }
    if (NULL != num_planes) {
        *num_planes = local_num_planes;
    }
}

uint32_t ExynosGraphicBufferMali::get_offsetInBytes(buffer_handle_t hnd, int plane_num) {
    if (plane_num < 0 || plane_num > 2)
        return -1;

    int offsets[3];

    get_gralloc_format_info(hnd, offsets, nullptr, nullptr, 0);

    return offsets[plane_num];
}

uint32_t ExynosGraphicBufferMali::get_strideInBytes(buffer_handle_t hnd, int plane_num) {
    if (plane_num < 0 || plane_num > 2)
        return -1;

    int strides[3];

    get_gralloc_format_info(hnd, nullptr, strides, nullptr, 0);

    return strides[plane_num];
}

uint32_t ExynosGraphicBufferMali::get_widthInSamples(buffer_handle_t hnd, int plane_num) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->plane_info[plane_num].alloc_width;
}

uint32_t ExynosGraphicBufferMali::get_heightInSamples(buffer_handle_t hnd, int plane_num) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->plane_info[plane_num].alloc_height;
}

bool ExynosGraphicBufferMali::check_usage_afbc_padding(buffer_handle_t hnd, uint64_t usage) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return (usage & MALI_GRALLOC_USAGE_AFBC_PADDING) == MALI_GRALLOC_USAGE_AFBC_PADDING;
}

std::vector<ExynosGraphicBufferMali::Rect> ExynosGraphicBufferMali::get_crop_rects(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, {});

    const int num_planes = get_num_planes(hnd);
    std::vector<ExynosGraphicBufferMali::Rect> crops(num_planes);

    crops[0].top = 0;
    crops[0].left = 0;
    crops[0].right = gralloc_hnd->width;
    crops[0].bottom = gralloc_hnd->height;

    return crops;
}

int ExynosGraphicBufferMali::get_num_planes(const buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    switch (get_format(hnd)) {
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

uint32_t ExynosGraphicBufferMali::get_layer_count(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return gralloc_hnd->layer_count;
}

uint32_t ExynosGraphicBufferMali::get_pixel_format_fourcc(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return drm_fourcc_from_handle(gralloc_hnd);
}

uint64_t ExynosGraphicBufferMali::get_pixel_format_modifier(buffer_handle_t hnd) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

    return drm_modifier_from_handle(gralloc_hnd);
}

bool ExynosGraphicBufferMali::check_plane_is_on_new_fd(buffer_handle_t hnd, int plane) {
    switch (plane) {
        case 0:
            return true;
        case 1:
            return get_num_planes(hnd) >= 2;
        case 2:
            return get_num_planes(hnd) >= 3;
    }

    return false;
}

int ExynosGraphicBufferMali::get_plane_fd(buffer_handle_t hnd, int plane) {
    convert_handle(gralloc_hnd, hnd, -EINVAL);

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
