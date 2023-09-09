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

#ifndef __SBWCWRAPPER_MSCL_H__
#define __SBWCWRAPPER_MSCL_H__

#include <hardware/exynos/acryl.h>

class SbwcAcrylInfo {
public:
    SbwcAcrylInfo();
    ~SbwcAcrylInfo();

    Acrylic *mHandle = NULL;
    AcrylicLayer *mLayer = NULL;

};

bool decodeMSCL(void *decoderHandle, buffer_handle_t srcBH, buffer_handle_t dstBH,
                unsigned int attr, unsigned int cropWidth, unsigned int cropHeight, unsigned int framerate);
bool decodeMSCL(void *decoderHandle, buffer_handle_t srcBH, buffer_handle_t dstBH,
                unsigned int attr, hwc_rect_t area, unsigned int framerate, int *fenceFd);

#endif
