/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_DNG_CREATOR_FOR_CAMERA_H_
#define EXYNOS_DNG_CREATOR_FOR_CAMERA_H_

#include "ExynosDng.h"
#include "ExynosCameraParameters.h"

namespace android {

#ifdef SAMSUNG_DNG

class ExynosDngCreatorForCamera {
public :
    ExynosDngCreatorForCamera();
    virtual ~ExynosDngCreatorForCamera();

    int     makeDng(ExynosCameraParameters *param,
                            char* dngBuffer,
                            unsigned int *rawSize);

    int     makeDngHeader(unsigned char *dngHeaderOut,
                            dng_attribute_t *dngInfo,
                            unsigned int *size,
                            bool useMainbufForThumb = false);

private:
    inline void setStripOffset(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            unsigned int value,
                            unsigned int *offset,
                            unsigned char *start,
                            unsigned int interval);
    inline void setStripByteCount(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            unsigned int value,
                            unsigned int *offset,
                            unsigned char *start);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            uint32_t value);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            unsigned char *pValue);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            unsigned char *pValue,
                            unsigned int *offset,
                            unsigned char *start);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            unsigned int *pValue,
                            unsigned int *offset,
                            unsigned char *start);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            double *pValue,
                            unsigned int *offset,
                            unsigned char *start);
    inline void writeTiffIfd(unsigned char **pCur,
                            unsigned short tag,
                            unsigned short type,
                            unsigned int count,
                            rational_t *pValue,
                            unsigned int *offset,
                            unsigned char *start);
};

#endif /*SUPPORT_SAMSUNG_DNG*/

}; /* namespace android */

#endif /* __SEC_JPG_ENC_H__ */
