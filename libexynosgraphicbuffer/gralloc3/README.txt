# ExynosGraphicBuffer

libexynosgraphicbuffer abstracts away the gralloc version used by the system.
So vendor modules that need to allocate/map buffers or look into a gralloc buffer's private_handle
don't need to depend on a specific version of gralloc or allocator/mapper interfaces.


To use libexynosgraphicbuffer add it as a shared library to your module and
#include <ExynosGraphicBuffer.h>

Location of header:
/hardware/samsung_slsi/exynos/include

Location of source code:
/hardware/samsung_slsi/exynos/gralloc3/libexynosgraphicbuffer
/hardware/samsung_slsi/exynos/gralloc4/libexynosgraphicbuffer (will be added with gralloc4)



### How to look at the buffer handle's metadata ###

Standard metadata stored in the handle as value

    ExynosGraphicBufferMeta gmeta;
    gmeta.init(dst_handle);

                or

    ExynosGraphicBufferMeta gmeta(dst_handle);


    gmeta.fd
    gmeta.width
    gmeta.format
    .....
    etc

    Please refer to ExynosGraphicBufferMeta class to which values are available.
    Please contact <sy.byeon@samsung.com> if you need access to metadata not preset here!


Dynamic metadata (stored as file descriptor)

  Get them by calling static functions of ExynosGraphicBufferMeta

- AFBC:
        static int is_afbc(buffer_handle_t);

        Returns 1 if buffer is compressed with AFBC (AFBC 1.0)
        Returns 0 if buffer is not compressed.

        e.g:
                ExynosGraphicBufferMeta::is_afbc(handle);

- Video MetaData:
        static void* get_video_metadata(buffer_handle_t);

        Returns the address of video metadata that must be type cast to video metadata struct by
        the user.

        Please DO NOT FREE this address!

        e.g:
            metaData = (ExynosVideoMeta*)ExynosGraphicBufferMeta::get_video_metadata(bufferHandle);

- Buffer align information (needed by HWC)

        static int get_pad_align(buffer_handle_t, pad_align_t *pad_align);

        pad_align_t struct is defined in ExynosGraphicBuffer.h header
        and gets filled with short int type.

        pad_align->align.w
        pad_align->align.h
        pad_align->pad.w
        pad_align->pad.h


### How to lock/unlock buffers ###

1) Get a Singleton instance of ExynosGraphicBufferMapper:

    static ExynosGraphicBufferMapper& gmapper(ExynosGraphicBufferMapper::get());

2) Create Rect object that will be used as input to mapper function (and android_ycbcr object
   if locking YUV buffers)

        Android::Rect bounds(width, height);
        android_ycbcr outLayout;


   Android::Rect class is defined in /frameworks/native/libs/ui/include_vndk/ui/Rect.h
   android_ycbcr struct is diefeind in /system/core/libsystem/include/system/graphics.h

   Both headers should get included if you include ExynosGraphicBuffer.h

3) lock the buffer:
   To make sure you use 64-bit usages while locking the buffer:
        status_t err = gmapper.lock64(bufferHandle, 64bit_usage, bounds, &vaddr);
        status_t err = gmapper.lockYCbCr64(bufferHandle, 64_bit_usage, bounds, &outLayout);

   You can still use the default mapper functions in the original GraphicBufferMapper class:
        status_t err = gmapper.lock(....);

   Refer to following header if you need to use more mapper functions:
        /frameworks/native/libs/ui/include_vndk/ui/GraphicBufferMapper.h

4) unlock the buffer:
    gmapper.unlock(bufferHandle);


### How to allocate buffers ###

1) Get a Singleton instnace of ExynosGraphicBufferAllocator:
    ExynosGraphicBufferAllocator& gAllocator(ExynosGraphicBufferAllocator::get());

2) Allocate buffer
    status_t error = gAllocator.allocate(width, height, format, layer_count, allocUsage, &dstBuffer, &dstStride, "requestorName");

3) To free the buffer
    gAllocator.free(freeBuffer.bufferHandle);

    You can still use the default allocator functions in the original GraphicBufferAllocator class:

    Refer to following header if you need to use more mapper functions:
        /frameworks/native/libs/ui/include_vndk/ui/GraphicBufferAllocator.h


### New name for S.LSI specific USAGES ###

New usages names can be accessed by adding the namespace containing them:
using namespace vendor::graphics
    or
using vendor::graphics::BufferUsage
using vendor::graphics::ExynosGraphicBufferUsage
    or directly by
vendor::graphics::BufferUsage::<USAGE>
vendor::graphics::ExynosGraphicBufferUsage::<USAGE>


*** gralloc1 default usages can still be used ***
They remain so the users of libexynosgraphicbuffer don't have to change too much code when moving
over to libexynosgraphicbuffer.
But I recommend moving over the Usages declared in
        /hardware/interfaces/graphics/common/1.0/types.hal

   Instead of using GRALLOC1_PRODUCE_USAGE_VIDEO_DECORDER please consider using
   BufferUsage::VIDEO_DECORDER


*** But S.LSI specific usages have been RENAMED ***

/* S.LSI specific usages */
enum ExynosGraphicBufferUsage {
        PROTECTED_DPB                   = 1ULL << 28,
        NO_AFBC                         = 1ULL << 29,
        CAMERA_RESERVED                 = 1ULL << 30,
        SECURE_CAMERA_RESERVED          = 1ULL << 31,
        HFR_MODE                        = 1ULL << 57,
        NOZEROED                        = 1ULL << 58,
        PRIVATE_NONSECURE               = 1ULL << 59,
        VIDEO_PRIVATE_DATA              = 1ULL << 60,
        VIDEO_EXT                       = 1ULL << 61,
        DAYDREAM_SINGLE_BUFFER_MODE     = 1ULL << 62,
        YUV_RANGE_FULL                  = 1ULL << 63,
};

    Instead of using GRALLOC1_PRODUCER_USAGE_CAMERA_RESERVED,
    plese use ExynosGraphicBufferUsage::CAMERA_RESERVEd.

This change was force to remove confusion between Android default and Exynos only usages.
