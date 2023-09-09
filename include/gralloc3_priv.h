/*
 * Copyright (C) 2017 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef GRALLOC3_PRIV_H_
#define GRALLOC3_PRIV_H_

#include <linux/fb.h>
#include <linux/ion.h>
#include <hardware/gralloc1.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <log/log.h>

#include <cutils/ashmem.h>

#define AFBC_INFO_SIZE                              (sizeof(int))
#define AFBC_ENABLE                                 (0xafbc)

/* Exynos specific usages */
#define GRALLOC1_PRODUCER_USAGE_CAMERA_RESERVED                 GRALLOC1_PRODUCER_USAGE_PRIVATE_2
#define GRALLOC1_PRODUCER_USAGE_SECURE_CAMERA_RESERVED          GRALLOC1_PRODUCER_USAGE_PRIVATE_3
#define GRALLOC1_PRODUCER_USAGE_PRIVATE_NONSECURE               GRALLOC1_PRODUCER_USAGE_PRIVATE_8
#define GRALLOC1_PRODUCER_USAGE_NOZEROED                        GRALLOC1_PRODUCER_USAGE_PRIVATE_9
#define GRALLOC1_PRODUCER_USAGE_HFR_MODE                        GRALLOC1_PRODUCER_USAGE_PRIVATE_10

#define GRALLOC1_CONSUMER_USAGE_YUV_RANGE_FULL                  GRALLOC1_CONSUMER_USAGE_PRIVATE_4
#define GRALLOC1_CONSUMER_USAGE_DAYDREAM_SINGLE_BUFFER_MODE     GRALLOC1_CONSUMER_USAGE_PRIVATE_5
#define GRALLOC1_CONSUMER_USAGE_VIDEO_EXT                       GRALLOC1_CONSUMER_USAGE_PRIVATE_6
#define GRALLOC1_CONSUMER_USAGE_VIDEO_PRIVATE_DATA              GRALLOC1_CONSUMER_USAGE_PRIVATE_7

/* for legacy */
#define GRALLOC_USAGE_PRIVATE_NONSECURE                         GRALLOC1_PRODUCER_USAGE_PRIVATE_NONSECURE

/* Macros used for handle contructor for 7885 (For Camera HAL1) */
#define GRALLOC_UNUSED(x) ((void)x)
#define GRALLOC_ALIGN(value, base) ((((value) + (base) -1) / (base)) * (base))
#define GRALLOC_PLANE_SIZE(w, h, ext_size)                            ((w) * (h) + ext_size)
#define GRALLOC_NV12N_S8B_LUMA_SIZE(w, h, ext_size)                   GRALLOC_PLANE_SIZE(w, h, ext_size)
#define GRALLOC_NV12N_S8B_CHROMA_SIZE(w, h, ext_size)                 (GRALLOC_ALIGN(((w) * (h)) + ext_size, 16))
#define GRALLOC_MSCL_ALIGN    128
#define GRALLOC_MSCL_EXT_SIZE 512
#define GRALLOC_CHROMA_VALIGN 32

#define GRALLOC_ARM_NUM_FDS 2
#define NUM_INTS_IN_PRIVATE_HANDLE ((sizeof(struct private_handle_t) - sizeof(native_handle)) / sizeof(int) - GRALLOC_ARM_NUM_FDS)

typedef enum
{
	/*
	 * Allocation will be used as a front-buffer, which
	 * supports concurrent producer-consumer access.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER
	 */
	MALI_GRALLOC_USAGE_FRONTBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_12,

	/*
	 * Allocation will be used as a back-buffer.
	 * Use when switching from front-buffer as a workaround for Android
	 * buffer queue, which does not re-allocate for a sub-set of
	 * existing usage.
	 *
	 * NOTE: Must not be used with MALI_GRALLOC_USAGE_FRONTBUFFER.
	 */
	MALI_GRALLOC_USAGE_FORCE_BACKBUFFER = GRALLOC1_PRODUCER_USAGE_PRIVATE_13,

	/*
	 * Buffer will not be allocated with AFBC.
	 *
	 * NOTE: Not compatible with MALI_GRALLOC_USAGE_FORCE_BACKBUFFER so cannot be
	 * used when switching from front-buffer to back-buffer.
	 */
	MALI_GRALLOC_USAGE_NO_AFBC = GRALLOC1_PRODUCER_USAGE_PRIVATE_1,

	/* Custom alignment for AFBC headers.
	 *
	 * NOTE: due to usage flag overlap, AFBC_PADDING cannot be used with FORCE_BACKBUFFER.
	 */
	MALI_GRALLOC_USAGE_AFBC_PADDING = GRALLOC1_PRODUCER_USAGE_PRIVATE_14,
	/* Private format usage.
	 * 'format' argument to allocation function will be interpreted in a
	 * private manner and must be constructed via GRALLOC_PRIVATE_FORMAT_WRAPPER_*
	 * macros which pack base format and AFBC format modifiers into 32-bit value.
	 */
	MALI_GRALLOC_USAGE_PRIVATE_FORMAT = GRALLOC1_PRODUCER_USAGE_PRIVATE_15,

	/* YUV only. */
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT601 = GRALLOC1_PRODUCER_USAGE_PRIVATE_18,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT709 = GRALLOC1_PRODUCER_USAGE_PRIVATE_19,
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020 = (GRALLOC1_PRODUCER_USAGE_PRIVATE_18 | GRALLOC1_PRODUCER_USAGE_PRIVATE_19),
	MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_MASK = MALI_GRALLOC_USAGE_YUV_COLOR_SPACE_BT2020,

	MALI_GRALLOC_USAGE_RANGE_DEFAULT = 0,
	MALI_GRALLOC_USAGE_RANGE_NARROW = GRALLOC1_PRODUCER_USAGE_PRIVATE_16,
	MALI_GRALLOC_USAGE_RANGE_WIDE = GRALLOC1_PRODUCER_USAGE_PRIVATE_17,
	MALI_GRALLOC_USAGE_RANGE_MASK = (GRALLOC1_PRODUCER_USAGE_PRIVATE_16 | GRALLOC1_PRODUCER_USAGE_PRIVATE_17),
} mali_gralloc_usage_type;

typedef struct
{
	struct hw_module_t common;
} gralloc1_module_t;

typedef enum
{
	MALI_DPY_TYPE_UNKNOWN = 0,
	MALI_DPY_TYPE_CLCD,
	MALI_DPY_TYPE_HDLCD
} mali_dpy_type;

typedef enum
{
	MALI_YUV_NO_INFO,
	MALI_YUV_BT601_NARROW,
	MALI_YUV_BT601_WIDE,
	MALI_YUV_BT709_NARROW,
	MALI_YUV_BT709_WIDE,
	MALI_YUV_BT2020_NARROW,
	MALI_YUV_BT2020_WIDE
} mali_gralloc_yuv_info;

typedef struct private_module
{
	gralloc1_module_t base;

	struct private_handle_t *framebuffer;
	uint32_t flags;
	uint32_t numBuffers;
	uint32_t bufferMask;
	pthread_mutex_t lock;
	buffer_handle_t currentBuffer;
	mali_dpy_type dpy_type;

	struct fb_var_screeninfo info;
	struct fb_fix_screeninfo finfo;
	float xdpi;
	float ydpi;
	float fps;
	int swapInterval;
	uint64_t fbdev_format;
	int ionfd;
} private_module_t;

/*
 * Maximum number of pixel format planes.
 * Plane [0]: Single plane formats (inc. RGB, YUV) and Y
 * Plane [1]: U/V, UV
 * Plane [2]: V/U
 */
#define MAX_PLANES 3

typedef struct plane_info {

	/*
	 * Offset to plane (in bytes),
	 * from the start of the allocation.
	 */
	uint32_t offset;

	/*
	 * Byte Stride: number of bytes between two vertically adjacent
	 * pixels in given plane. This can be mathematically described by:
	 *
	 * byte_stride = ALIGN((alloc_width * bpp)/8, alignment)
	 *
	 * where,
	 *
	 * alloc_width: width of plane in pixels (c.f. pixel_stride)
	 * bpp: average bits per pixel
	 * alignment (in bytes): dependent upon pixel format and usage
	 *
	 * For uncompressed allocations, byte_stride might contain additional
	 * padding beyond the alloc_width. For AFBC, alignment is zero.
	 */
	uint32_t byte_stride;

	/*
	 * Dimensions of plane (in pixels).
	 *
	 * For single plane formats, pixels equates to luma samples.
	 * For multi-plane formats, pixels equates to the number of sample sites
	 * for the corresponding plane, even if subsampled.
	 *
	 * AFBC compressed formats: requested width/height are rounded-up
	 * to a whole AFBC superblock/tile (next superblock at minimum).
	 * Uncompressed formats: dimensions typically match width and height
	 * but might require pixel stride alignment.
	 *
	 * See 'byte_stride' for relationship between byte_stride and alloc_width.
	 *
	 * Any crop rectangle defined by GRALLOC_ARM_BUFFER_ATTR_CROP_RECT must
	 * be wholly within the allocation dimensions. The crop region top-left
	 * will be relative to the start of allocation.
	 */
	uint32_t alloc_width;
	uint32_t alloc_height;
} plane_info_t;

struct private_handle_t : public native_handle
{
	enum
	{
		PRIV_FLAGS_FRAMEBUFFER            = 1U << 0,
		PRIV_FLAGS_USES_ION_COMPOUND_HEAP = 1U << 1,
		PRIV_FLAGS_USES_ION               = 1U << 2,
		PRIV_FLAGS_USES_ION_DMA_HEAP      = 1U << 3,
		PRIV_FLAGS_USES_2PRIVATE_DATA     = 1U << 4,
		PRIV_FLAGS_USES_3PRIVATE_DATA     = 1U << 5,
		PRIV_FLAGS_USES_HFR_MODE          = 1U << 6,
	};

	/*
	 * Shared file descriptor for dma_buf sharing. This must be the first element in the
	 * structure so that binder knows where it is and can properly share it between
	 * processes.
	 * DO NOT MOVE THIS ELEMENT!
	 */
	int fd;
	int fd1;
	int fd2;
	int fd3;
	int fd4;

	// ints
	int magic;
	int flags;

	/*
	 * Input properties.
	 *
	 * req_format: Pixel format, base + private modifiers.
	 * width/height: Buffer dimensions.
	 * producer/consumer_usage: Buffer usage (indicates IP)
	 */
	int width;
	int height;
	/* LSI integration: Needed by Camera */
	int frameworkFormat;

	uint64_t producer_usage;
	uint64_t consumer_usage;

	union
	{
		int format;
		uint64_t internal_format;
	};

	/*
	 * Allocation properties.
	 *
	 * alloc_format: Pixel format (base + modifiers). NOTE: base might differ from requested
	 *               format (req_format) where fallback to single-plane format was required.
	 * plane_info:   Per plane allocation information.
	 * size:         Total bytes allocated for buffer (inc. all planes, layers. etc.).
	 * layer_count:  Number of layers allocated to buffer.
	 *               All layers are the same size (in bytes).
	 *               Multi-layers supported in v1.0, where GRALLOC1_CAPABILITY_LAYERED_BUFFERS is enabled.
	 *               Layer size: 'size' / 'layer_count'.
	 *               Layer (n) offset: n * ('size' / 'layer_count'), n=0 for the first layer.
	 *
	 */
	uint64_t alloc_format;
	union
	{
		plane_info_t plane_info[MAX_PLANES];
		struct
		{
			int plane_offset;
			int byte_stride;
			int alloc_width;
			int vstride;
		};
	};
	int size;
	int size1;
	int size2;
	uint32_t stride;
	uint32_t layer_count;

	union
	{
		void *base;
		uint64_t bases[3];
	};

	uint64_t backing_store_id;
	int backing_store_size;
	int cpu_read;               /**< Buffer is locked for CPU read when non-zero. */
	int cpu_write;              /**< Buffer is locked for CPU write when non-zero. */
	int allocating_pid;
	int remote_pid;
	int ref_count;
	// locally mapped shared attribute area
	union
	{
		void *attr_base;
		uint64_t padding3;
	};

	mali_gralloc_yuv_info yuv_info;

	// Following members is for framebuffer only
	int fb_fd;
	union
	{
		off_t offset;
		uint64_t padding4;
	};

	/*
	 * min_pgsz denotes minimum phys_page size used by this buffer.
	 * if buffer memory is physical contiguous set min_pgsz to buff->size
	 * if not sure buff's real phys_page size, you can use SZ_4K for safe.
	 */
	int min_pgsz;

	int is_compressible;

	ion_user_handle_t ion_handles[3];

	int     PRIVATE_1 = 0;
	int     PRIVATE_2 = 0;
	int     plane_count = 0;

	static const int sNumFds = GRALLOC_ARM_NUM_FDS;
	static const int sMagic = 0x3141592;

	private_handle_t(int _fd, int _fd1, int _fd2, int _size, int _size1, int _size2,
	                 int _flags, uint64_t _producer_usage, uint64_t _consumer_usage,
	                 int _width, int _height, int _format, uint64_t _internal_format, int _frameworkFormat,
	                 int _stride, int _byte_stride, int _vstride, int _is_compressible, uint32_t _layer_count)
		: fd(_fd)
		, magic(sMagic)
		, flags(_flags)
		, width(_width)
		, height(_height)
		, frameworkFormat(_frameworkFormat)
		, producer_usage(_producer_usage)
		, consumer_usage(_consumer_usage)
		, internal_format(_internal_format)
		, alloc_format(_internal_format)
		, stride(_stride)
		, layer_count(_layer_count)
		, base(NULL)
		, backing_store_id(0x0)
		, backing_store_size(0)
		, cpu_read(0)
		, cpu_write(0)
		, allocating_pid(getpid())
		, remote_pid(-1)
		, ref_count(1)
		, yuv_info(MALI_YUV_NO_INFO)
		, fb_fd(-1)
		, offset(0)
		, min_pgsz(4096)
		, is_compressible(_is_compressible)
		, plane_count(1)
	{
		GRALLOC_UNUSED(_byte_stride);

		version = sizeof(native_handle);

		fd1 = fd2 = fd3 = fd4 = -1;
		size= _size;
		size1 = _size1;
		size2 = _size2;

		numFds = sNumFds - 1;
		numInts = NUM_INTS_IN_PRIVATE_HANDLE + 1;

		if (_fd1 != -1)
		{
			fd1 = _fd1;
			numFds++;
			numInts--;
		}
		if (_fd2 != -1)
		{
			fd2 = _fd2;
			numFds++;
			numInts--;
		}

		bases[1] = 0;
		bases[2] = 0;

		plane_offset = 0;
		byte_stride = _stride;
		alloc_width = _stride;
		vstride = _vstride;

		GRALLOC_UNUSED(_format);

		int luma_vstride = height;
		int chroma_size = 0;
		int luma_size = 0;
		int ext_size = 256;

		ion_handles[0] = ion_handles[1] = ion_handles[2] = -1;

		switch(internal_format)
		{
			case HAL_PIXEL_FORMAT_YCrCb_420_SP:
				plane_info[0].offset = 0;
				plane_info[0].byte_stride = byte_stride;
				plane_info[0].alloc_width = stride;
				plane_info[0].alloc_height = vstride;

				plane_info[1].offset = stride * vstride;
				plane_info[1].byte_stride = byte_stride;
				plane_info[1].alloc_width = stride;
				plane_info[1].alloc_height = vstride / 2;

				plane_count = 2;
				break;

			case HAL_PIXEL_FORMAT_YCbCr_422_SP:
				plane_info[0].offset = 0;
				plane_info[0].byte_stride = byte_stride;
				plane_info[0].alloc_width = stride;
				plane_info[0].alloc_height = vstride;

				plane_info[1].offset = stride * vstride;
				plane_info[1].byte_stride = byte_stride;
				plane_info[1].alloc_width = stride;
				plane_info[1].alloc_height = vstride;

				plane_count = 2;
				break;

			case HAL_PIXEL_FORMAT_YCbCr_422_I:
				luma_vstride = height;
				chroma_size = GRALLOC_PLANE_SIZE(stride, luma_vstride, ext_size);
				luma_size = GRALLOC_PLANE_SIZE(stride, luma_vstride, 0);


				plane_info[0].alloc_width  = stride;
				plane_info[0].alloc_height = luma_vstride * 2;
				plane_info[0].byte_stride  = byte_stride;
				plane_info[0].offset       = 0;

				plane_count = 1;
				break;

			case 0x11f: // HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_P:
				chroma_size = GRALLOC_PLANE_SIZE(GRALLOC_ALIGN(stride / 2, 16), height, ext_size);
				luma_size = GRALLOC_PLANE_SIZE(stride, height, 0);


				plane_info[0].alloc_width  = _stride;
				plane_info[0].alloc_height = height;
				plane_info[0].byte_stride  = byte_stride;
				plane_info[0].offset       = 0;

				plane_info[1].alloc_width  = GRALLOC_ALIGN(_stride / 2, 16);
				plane_info[1].alloc_height = height;
				plane_info[1].byte_stride  = byte_stride;
				plane_info[1].offset       = luma_size;

				plane_count = 3;
				break;

			case 0x123: //HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN:
				chroma_size = GRALLOC_NV12N_S8B_CHROMA_SIZE(stride, GRALLOC_ALIGN(luma_vstride / 2, GRALLOC_CHROMA_VALIGN), ext_size);
				luma_size = GRALLOC_NV12N_S8B_LUMA_SIZE(stride, luma_vstride, ext_size);

				if (width % GRALLOC_MSCL_ALIGN)
				{
					luma_size += GRALLOC_MSCL_EXT_SIZE;
					chroma_size += GRALLOC_MSCL_EXT_SIZE/2;
				}

				plane_info[0].alloc_width  = stride;
				plane_info[0].alloc_height = luma_vstride;
				plane_info[0].byte_stride  = byte_stride;
				plane_info[0].offset       = 0;

				plane_info[1].alloc_width  = stride;
				plane_info[1].alloc_height = GRALLOC_ALIGN(luma_vstride / 2, GRALLOC_CHROMA_VALIGN);
				plane_info[1].byte_stride  = byte_stride;
				plane_info[1].offset       = luma_size;

				plane_count = 2;
				break;

			default:
				ALOGE("unknown format from Camera HAL");
				break;
		}

/* Experimental: Allocate share attr FD for buffer created externally by 7885 Camera HAL. */
#if 0
		int share_attr_fd = ashmem_create_region("gralloc_shared_attr", PAGE_SIZE);
		if (share_attr_fd < 0)
		{
			ALOGE("Failed to allocate page for shared attribute region");
			return;
		}

		attr_base = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, share_attr_fd, 0);

		if (attr_base != MAP_FAILED)
		{
			memset(attr_base, 0xff, PAGE_SIZE);
			munmap(attr_base, PAGE_SIZE);
			attr_base = MAP_FAILED;
		}
		else
		{
			ALOGE("Failed to mmap shared attribute region");
			close(share_attr_fd);
			return;
		}

		fd1 = share_attr_fd;

		numFds++;
		numInts--;
#endif
	}


	static int validate(const native_handle *h)
	{
		static const int sNumFds = GRALLOC_ARM_NUM_FDS;
		static const int sMagic = 0x3141592;
		const private_handle_t *hnd = (const private_handle_t *)h;

		if (!h || h->version != sizeof(native_handle) ||
				hnd->numInts + hnd->numFds != NUM_INTS_IN_PRIVATE_HANDLE + sNumFds ||
				hnd->magic != sMagic)
		{
			return -EINVAL;
		}

		return 0;
	}

	static private_handle_t *dynamicCast(const native_handle *in)
	{
		if (validate(in) == 0)
		{
			return (private_handle_t *)in;
		}

		return NULL;
	}

	int get_num_ion_fds() const
	{
		return numFds - 1;
	}

	void dump(const char *str) const
	{
		ALOGD("[%s] "
				"unique_id(%" PRIu64 ") "
				"fd(%d %d %d %d) "
				"flags(%d) "
				"wh(%d %d) "
				"req_format(0x%x) "
				"usage_pc(0x%" PRIx64 " 0x%" PRIx64 ") "
				"format(0x%x) "
				"interal_format(0x%" PRIx64 ") "
				"stride(%d) byte_stride(%d) internal_wh(%d %d) "
				"alloc_format(0x%" PRIx64 ") "
				"size(%d %d %d) "
				"layer_count(%d) plane_count(%d)"
				"bases(0x%" PRIx64 " 0x%" PRIx64 " 0x%" PRIx64 ") "
				"\n",
				str,
				backing_store_id,
				fd, fd1, fd2, fd3,
				flags,
				width, height,
				frameworkFormat,
				producer_usage, consumer_usage,
				format, internal_format,
				stride, plane_info[0].byte_stride, plane_info[0].alloc_width, plane_info[0].alloc_height,
				alloc_format,
				size, size1, size2,
				layer_count, plane_count,
				bases[0], bases[1], bases[2]
			);
	}
};

#endif /* GRALLOC3_PRIV_H_ */
