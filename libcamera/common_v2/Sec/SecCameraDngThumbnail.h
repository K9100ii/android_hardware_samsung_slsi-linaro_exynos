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

#ifndef SEC_CAMERA_DNG_THUMBNAIL_H_
#define SEC_CAMERA_DNG_THUMBNAIL_H_

#include <utils/threads.h>

#include "ExynosCameraConfig.h"
#include "SecCameraDng.h"

namespace android {

#ifdef SAMSUNG_DNG

class SecCameraDngThumbnail {
public:
    SecCameraDngThumbnail();
    virtual ~SecCameraDngThumbnail();
    void cleanNode();
    dng_thumbnail_t *createNode(int thumbnailSize);
    void deleteNode(dng_thumbnail_t *node);
    dng_thumbnail_t *putNode(dng_thumbnail_t *node);
    dng_thumbnail_t *getNode(int frameCount);

private:
    dng_thumbnail_t *addtoList(dng_thumbnail_t* newNode);

    dng_thumbnail_t m_head;
    dng_thumbnail_t m_tail;
    mutable Mutex       m_thumbnailLock;
    mutable Mutex       m_processLock;
    mutable Condition   m_processCondition;
};

#endif /*SUPPORT_SAMSUNG_DNG*/

}; /* namespace android */

#endif /* SEC_CAMERA_DNG_THUMBNAIL_H_ */
