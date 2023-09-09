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

#ifndef __SBWCWRAPPER_H__
#define __SBWCWRAPPER_H__

class SbwcDecoderIP {
public:
    SbwcDecoderIP(int decodeIPType);
    ~SbwcDecoderIP();

    int mDecodeIPType;
    void *mHandleIP;
};

class SbwcWrapper {
public:
    SbwcWrapper();
    ~SbwcWrapper();

    bool decode(void *srcHandle, void *dstHandle, unsigned int attr, unsigned int framerate = 0);
    bool decode(void *srcHandle, void *dstHandle, unsigned int attr, unsigned int cropWidth, unsigned int cropHeight, unsigned int framerate = 0);
private:
    bool initSbwcDecoder(void);

    std::vector<SbwcDecoderIP> mVecSbwcDecoderIP;
};

#endif // __SBWCWRAPPER_H__
