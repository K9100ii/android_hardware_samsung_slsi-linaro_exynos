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
#include <sync/sync.h>

#define ATRACE_TAG ATRACE_TAG_GRAPHICS
#include <utils/Trace.h>

#include "sbwcwrapper_common.h"
#include "sbwcwrapper_mscl.h"

SbwcAcrylInfo::SbwcAcrylInfo()
{
    mHandle = Acrylic::createScaler();
    if (mHandle == NULL) {
        ALOGE("Failed to create default scaler");
    } else {
        mLayer = mHandle->createLayer();
        if (mLayer == NULL) {
            ALOGE("Failed to create layer");
            delete mHandle;
            mHandle = NULL;
        }
    }
}

SbwcAcrylInfo::~SbwcAcrylInfo()
{
    delete mLayer;
    delete mHandle;
}


bool decodeMSCL(void *decoderHandle, buffer_handle_t srcBH, buffer_handle_t dstBH,
                unsigned int attr, unsigned int cropWidth, unsigned int cropHeight, unsigned int framerate)
{
    int fenceFd;

    hwc_rect_t rect = { .left = 0, .top = 0, .right = static_cast<int>(cropWidth),
                        .bottom = static_cast<int>(cropHeight) };

    if (!decodeMSCL(decoderHandle, srcBH, dstBH, attr, rect, framerate, &fenceFd))
        return false;

    // TODO : check attr about blocking.
    sync_wait(fenceFd, 16);
    close(fenceFd);
    return true;
}

bool decodeMSCL(void *decoderHandle, buffer_handle_t srcBH, buffer_handle_t dstBH,
                unsigned int attr, hwc_rect_t area, unsigned int framerate, int *fenceFd)
{
    ATRACE_CALL();

    if (!decoderHandle)
        return false;

    auto *acrylHandle = static_cast<SbwcAcrylInfo*>(decoderHandle)->mHandle;
    auto *layer = static_cast<SbwcAcrylInfo*>(decoderHandle)->mLayer;

    int srcFmt;
    srcFmt = getFormatFromVideoMeta(srcBH);
    if (!srcFmt)
        srcFmt = ExynosGraphicBufferMeta::get_internal_format(srcBH);
    else if (srcFmt < 0)
        return false;

    if (!layer->setImageDimension(static_cast<unsigned int>(ExynosGraphicBufferMeta::get_stride(srcBH)),
                                  static_cast<unsigned int>(ExynosGraphicBufferMeta::get_vstride(srcBH))))
        return false;
    if (!layer->setImageType(srcFmt, static_cast<unsigned int>(ExynosGraphicBufferMeta::get_dataspace(srcBH))))
        return false;

    if (!acrylHandle->setCanvasDimension(static_cast<unsigned int>(ExynosGraphicBufferMeta::get_stride(dstBH)),
                                         static_cast<unsigned int>(ExynosGraphicBufferMeta::get_vstride(dstBH))))
        return false;
    if (!acrylHandle->setCanvasImageType(static_cast<unsigned int>(ExynosGraphicBufferMeta::get_internal_format(dstBH)),
                                         static_cast<unsigned int>(ExynosGraphicBufferMeta::get_dataspace(dstBH))))
        return false;

    int srcBuf[3], dstBuf[3];
    size_t srcLen[3], dstLen[3];
    off_t offDummy[3] = {};

    for (int i = 0; i < 3; i++) {
        srcBuf[i] = ExynosGraphicBufferMeta::get_fd(srcBH, i);
        srcLen[i] = ExynosGraphicBufferMeta::get_size(srcBH, i);
        dstBuf[i] = ExynosGraphicBufferMeta::get_fd(dstBH, i);
        dstLen[i] = ExynosGraphicBufferMeta::get_size(dstBH, i);
    }

    uint32_t attrAcryl = (attr & SBWCDECODER_ATTR_SECURE_BUFFER) ? AcrylicCanvas::ATTR_PROTECTED
                                                                 : AcrylicCanvas::ATTR_NONE;

    if (!layer->setImageBuffer(srcBuf, srcLen, offDummy,
                               ExynosGraphicBufferMeta::get_num_image_fds(srcBH), -1, attrAcryl))
        return false;

    if (!acrylHandle->setCanvasBuffer(dstBuf, dstLen, offDummy,
                                      ExynosGraphicBufferMeta::get_num_image_fds(dstBH), -1, attrAcryl))
        return false;

    layer->setCompositArea(area, area, 0, 0);

    if (!acrylHandle->execute(fenceFd, 1)) {
        ALOGE("decode is failed");
        return false;
    }

    AcrylicPerformanceRequest request;

    request.reset(1);
    request.getFrame(0)->setFrameRate(framerate);
    acrylHandle->requestPerformanceQoS(&request);

    return true;
}
