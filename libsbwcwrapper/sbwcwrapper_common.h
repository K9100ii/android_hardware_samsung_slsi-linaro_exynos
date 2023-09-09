/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef __SBWCWRAPPER_COMMON_H__
#define __SBWCWRAPPER_COMMON_H__

#include <log/log.h>

#include <ExynosGraphicBuffer.h>
#include <VendorVideoAPI.h>

#ifndef SBWCDECODER_ATTR_SECURE_BUFFER
#define SBWCDECODER_ATTR_SECURE_BUFFER  (1 << 0)
#endif

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#ifndef ARRSIZE
#define ARRSIZE(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

using ::vendor::graphics::ExynosGraphicBufferMeta;

static int getFormatFromVideoMeta(buffer_handle_t handle)
{
    ATRACE_CALL();

    int metaDataFd = ExynosGraphicBufferMeta::get_video_metadata_fd(handle);
    if (metaDataFd < 0)
        return 0;

    ExynosVideoMeta *metaData = static_cast<ExynosVideoMeta*>(mmap(0, sizeof(*metaData), PROT_READ | PROT_WRITE, MAP_SHARED, metaDataFd, 0));
    if (!metaData) {
        ALOGE("fail to mmap ExynosVideoMeta");
        return -ENOMEM;
    }

    unsigned int pixelFormat = metaData->nPixelFormat;

    munmap(metaData, sizeof(*metaData));

    return pixelFormat;
}

#endif
