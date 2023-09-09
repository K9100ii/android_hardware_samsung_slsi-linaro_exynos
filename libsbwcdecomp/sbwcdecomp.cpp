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

#include <ui/GraphicBuffer.h>
#include <log/log.h>

#include <hardware/exynos/sbwcdecomp.h>

#include <vendor/samsung_slsi/hardware/SbwcDecompService/1.0/ISbwcDecompService.h>

enum {
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M      = 0x105,   /* HAL_PIXEL_FORMAT_YCbCr_420_SP */
    /* 10-bit format (2 fd, 10bit, 2x byte) custom formats */
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_P010_M        = 0x127,
    /* SBWC format */
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC = 0x130,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC  = 0x131,

    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC = 0x132,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC  = 0x133,

    HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC = 0x134,

    /* SBWC Lossy formats */
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L50 = 0x140,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC_L75 = 0x141,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L50  = 0x150,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC_L75  = 0x151,

    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L40 = 0x160,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L60 = 0x161,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC_L80 = 0x162,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L40  = 0x170,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L60  = 0x171,
    HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC_L80  = 0x172,
};

using namespace android;
using namespace vendor::samsung_slsi::hardware::SbwcDecompService::V1_0;

SbwcDecomp::SbwcDecomp()
{
}

SbwcDecomp::~SbwcDecomp()
{
}

bool isSBWCFormat(const PixelFormat format)
{
    switch (format) {
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_SBWC:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_SBWC:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SP_M_10B_SBWC:
    case HAL_PIXEL_FORMAT_EXYNOS_YCbCr_420_SPN_10B_SBWC:
    case HAL_PIXEL_FORMAT_EXYNOS_YCrCb_420_SP_M_SBWC:
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
        return true;
    default:
        break;
    }

    return false;
}

bool isValideForDecomp(const sp<GraphicBuffer>& srcBuf, const sp<GraphicBuffer>& dstBuf)
{
    if (!srcBuf) {
        ALOGE("srcBuf buffer is null");
        return false;
    }

    if (!isSBWCFormat(srcBuf->getPixelFormat())) {
        ALOGE("src is not SBWC");
        return false;
    }

    if (!dstBuf) {
        ALOGE("dstBuf buffer is null");
        return false;
    }

    return true;
}

#define SBWCDECODER_ATTR_SECURE_BUFFER  (1 << 0)
bool SbwcDecomp::decomp(const sp<GraphicBuffer>& srcBuf, const sp<GraphicBuffer>& dstBuf)
{
    if (!srcBuf) {
        ALOGE("srcBuf buffer is null");
        return false;
    }

    return decomp(srcBuf, dstBuf, srcBuf->getWidth(), srcBuf->getHeight());
}

bool SbwcDecomp::decomp(const ::android::sp<::android::GraphicBuffer>& srcBuf, const ::android::sp<::android::GraphicBuffer>& dstBuf, unsigned int cropWidth, unsigned int cropHeight)
{
    sp<ISbwcDecompService> sbwcDecompService = ISbwcDecompService::getService();
    if (!sbwcDecompService) {
        ALOGE("failed to getService to ISbwcDecompService");
        return false;
    }

    if (!isValideForDecomp(srcBuf, dstBuf))
        return false;

    unsigned int attr = 0;

    if (srcBuf->getUsage() & GRALLOC_USAGE_PROTECTED)
        attr |= SBWCDECODER_ATTR_SECURE_BUFFER;

    hardware::hidl_handle srcHH(srcBuf->handle);
    hardware::hidl_handle dstHH(dstBuf->handle);

    return sbwcDecompService->decodeWithCrop(srcHH, dstHH, attr, cropWidth, cropHeight) == NO_ERROR;
}

extern "C" void *createSbwcDecomp(void)
{
    return new SbwcDecomp();
}

extern "C" void removeSbwcDecomp(void *handle)
{
    if (handle == nullptr)
        return;

    delete static_cast<SbwcDecomp*>(handle);

    return;
}

extern "C" bool decomp(void *handle,
                       const sp<GraphicBuffer>& srcBuf, const sp<GraphicBuffer>& dstBuf,
                       unsigned int cropWidth, unsigned int cropHeight) {
    if (srcBuf == nullptr || dstBuf == nullptr) {
        return false;
    }

    if (handle == nullptr) {
        ALOGE("handle is nullptr");
        return false;
    }

    SbwcDecomp *sbwcDecomp = static_cast<SbwcDecomp*>(handle);

    return sbwcDecomp->decomp(srcBuf, dstBuf, cropWidth, cropHeight);
}
