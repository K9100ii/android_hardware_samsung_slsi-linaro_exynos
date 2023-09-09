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

#include <log/log.h>

#include <ExynosGraphicBuffer.h>
#include <VendorVideoAPI.h>
#include <sync/sync.h>

#include <hardware/exynos/sbwcwrapper.h>

#include "sbwcwrapper_common.h"
#include "sbwcwrapper_mscl.h"
#ifdef LIBSBWC_DPU_ENABLED
#include <hardware/exynos/sbwcdecoder_dpu.h>
#else
#include <sbwcdecoder_dpu_default.h>
#endif

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

enum decodeIP_t {
    DECODE_IP_ERR = 0,

    DECODE_IP_MSCL = 1,
    DECODE_IP_DPU = 2,
};

SbwcDecoderIP::SbwcDecoderIP(int decodeIPType) : mDecodeIPType(decodeIPType){
    switch (decodeIPType) {
    case DECODE_IP_DPU:
        mHandleIP = new DpuSbwcDecoder();
        break;
    case DECODE_IP_MSCL:
        mHandleIP = new SbwcAcrylInfo();
        break;
    default:
        ALOGE("failed to create SbwcDecoder type %d", decodeIPType);
        mHandleIP = NULL;
        break;
    }
}

SbwcDecoderIP::~SbwcDecoderIP() {
    switch (mDecodeIPType) {
    case DECODE_IP_DPU:
        delete static_cast<DpuSbwcDecoder*>(mHandleIP);
        break;
    case DECODE_IP_MSCL:
        delete static_cast<SbwcAcrylInfo*>(mHandleIP);
        break;
    default:
        ALOGE("failed to remove SbwcDecoder type %d", mDecodeIPType);
        break;
    }
}

/*
 * If new project has a new priority to request decompression to SBWC,
 * add here about that. And by using BoardConfig, use something appropriate to project.
 */
static struct sbwcDecoderInfo {
    const char *name;
    int len;
    decodeIP_t type;
} arrDecoderInfo[] = {
    { "Dpu", 3, DECODE_IP_DPU },
    { "Mscl", 4, DECODE_IP_MSCL },
};

bool SbwcWrapper::initSbwcDecoder(void)
{
    const char *priority = LIBSBWC_DECODER_PRIORITY;
    unsigned int i;

    while (*priority != '\0') {
        for (i = 0; i < ARRSIZE(arrDecoderInfo); i++) {
            if (strncmp(priority, arrDecoderInfo[i].name, arrDecoderInfo[i].len) == 0) {
                mVecSbwcDecoderIP.emplace_back(arrDecoderInfo[i].type);
                priority += arrDecoderInfo[i].len;
                break;
            }
        }

        if (i == ARRSIZE(arrDecoderInfo)) {
            ALOGE("failed to get SbwcDecoderPriority of '%s'", LIBSBWC_DECODER_PRIORITY);
            return false;
        }
    }

    return true;
}

SbwcWrapper::SbwcWrapper()
{
    ATRACE_CALL();

    initSbwcDecoder();
}

SbwcWrapper::~SbwcWrapper()
{
    ATRACE_CALL();
}

static bool decodeDPU(void *decoderHandle, buffer_handle_t srcBH, buffer_handle_t dstBH, unsigned int attr,
                      unsigned int cropWidth, unsigned int cropHeight)
{
    ATRACE_CALL();

    auto *SbwcDecoderDPU = static_cast<DpuSbwcDecoder*>(decoderHandle);
    if (!SbwcDecoderDPU)
        return false;

    int srcFmt;
    srcFmt = getFormatFromVideoMeta(srcBH);
    if (!srcFmt)
        srcFmt = ExynosGraphicBufferMeta::get_internal_format(srcBH);
    else if (srcFmt < 0)
        return false;

    int dstFmt;
    dstFmt = getFormatFromVideoMeta(dstBH);
    if (!dstFmt)
        dstFmt = ExynosGraphicBufferMeta::get_internal_format(dstBH);
    else if (dstFmt < 0)
        return false;

    int srcBuf[3], dstBuf[3];
    size_t srcLen[3], dstLen[3];

    int dataspace = ExynosGraphicBufferMeta::get_dataspace(srcBH);

    for (int i = 0; i < 3; i++) {
        srcBuf[i] = ExynosGraphicBufferMeta::get_fd(srcBH, i);
        srcLen[i] = ExynosGraphicBufferMeta::get_size(srcBH, i);
        dstBuf[i] = ExynosGraphicBufferMeta::get_fd(dstBH, i);
        dstLen[i] = ExynosGraphicBufferMeta::get_size(dstBH, i);
    }

    if (!SbwcDecoderDPU->decodeSBWC(srcFmt, dstFmt, dataspace,
        cropWidth, cropHeight,
	ExynosGraphicBufferMeta::get_width(dstBH), ExynosGraphicBufferMeta::get_height(dstBH),
	ExynosGraphicBufferMeta::get_stride(srcBH), ExynosGraphicBufferMeta::get_stride(dstBH),
	attr, srcBuf, dstBuf)) {
        return false;
    }

    return true;
}

bool SbwcWrapper::decode(void *srcHandle, void *dstHandle, unsigned int attr, unsigned int framerate)
{
    auto *srcBH = static_cast<buffer_handle_t>(srcHandle);
    if (!srcBH)
        return false;

    return decode(srcHandle, dstHandle, attr, static_cast<unsigned int>(ExynosGraphicBufferMeta::get_width(srcBH)),
                  static_cast<unsigned int>(ExynosGraphicBufferMeta::get_height(srcBH)), framerate);
}

bool SbwcWrapper::decode(void *srcHandle, void *dstHandle, unsigned int attr, unsigned int cropWidth, unsigned int cropHeight, unsigned int framerate)
{
    ATRACE_CALL();

    auto *srcBH = static_cast<buffer_handle_t>(srcHandle);
    if (!srcBH)
        return false;

    auto *dstBH = static_cast<buffer_handle_t>(dstHandle);
    if (!dstBH)
        return false;

    for(auto &decoderIP : mVecSbwcDecoderIP) {
        bool ret = false;

        switch (decoderIP.mDecodeIPType) {
        case DECODE_IP_DPU:
            ret = decodeDPU(decoderIP.mHandleIP, srcBH, dstBH, attr, cropWidth, cropHeight);
            break;
        case DECODE_IP_MSCL:
            ret = decodeMSCL(decoderIP.mHandleIP, srcBH, dstBH, attr, cropWidth, cropHeight, framerate);
            break;
        default:
            ALOGE("invalid name for sbwc decompress IP type %d", decoderIP.mDecodeIPType);
            ret = false;
            break;
        }

        if (ret)
            return ret;
    }

    ALOGE("failed to decompress SBWC");
    return false;
}
