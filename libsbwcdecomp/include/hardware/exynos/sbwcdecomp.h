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

#ifndef HARDWARE_SAMSUNG_SLSI_EXYNOS_LIBSBWCDECOMP_SBWCDECOMP_H
#define HARDWARE_SAMSUNG_SLSI_EXYNOS_LIBSBWCDECOMP_SBWCDECOMP_H

class SbwcDecomp {
public:
    SbwcDecomp();
    ~SbwcDecomp();

    bool decomp(const ::android::sp<::android::GraphicBuffer>& srcBuf, const ::android::sp<::android::GraphicBuffer>& dstBuf);
    bool decomp(const ::android::sp<::android::GraphicBuffer>& srcBuf, const ::android::sp<::android::GraphicBuffer>& dstBuf, unsigned int cropWidth, unsigned int cropHeight);
};

#endif
