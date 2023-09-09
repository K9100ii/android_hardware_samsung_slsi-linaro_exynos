/*
 * Copyright Samsung Electronics Co.,LTD.
 * Copyright (C) 2015 The Android Open Source Project
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

#define LOG_TAG "SecCameraDngThumbnail"

#include <utils/Log.h>

#include "SecCameraDngThumbnail.h"

namespace android {

#ifdef SAMSUNG_DNG

SecCameraDngThumbnail::SecCameraDngThumbnail()
{
    m_head.buf = NULL;
    m_head.size = 0;
    m_head.frameCount = 0;
    m_head.next = NULL;

    m_tail.buf = NULL;
    m_tail.size = 0;
    m_tail.frameCount = 0;
    m_tail.prev = NULL;
}

SecCameraDngThumbnail::~SecCameraDngThumbnail()
{
    cleanNode();
}

void SecCameraDngThumbnail::cleanNode()
{
    Mutex::Autolock lock(m_thumbnailLock);

    dng_thumbnail_t *curNode;

    while (m_head.next) {
        curNode = m_head.next;
        m_head.next = curNode->next;
        deleteNode(curNode);
    }

    m_head.next = NULL;
    m_tail.prev = NULL;
}

dng_thumbnail_t *SecCameraDngThumbnail::addtoList(dng_thumbnail_t* newNode)
{
    Mutex::Autolock lock(m_thumbnailLock);

    dng_thumbnail_t *tempNode;

    if (!newNode)
        return NULL;

    if (m_head.next == NULL) {
        m_head.next = newNode;
        m_tail.prev = newNode;
    } else {
        tempNode = m_tail.prev;
        tempNode->next = newNode;
        newNode->prev = tempNode;
        m_tail.prev = newNode;
    }

    return newNode;
}

dng_thumbnail_t *SecCameraDngThumbnail::createNode(int thumbnailSize)
{
    dng_thumbnail_t *node = new dng_thumbnail_t;

    ALOGD("DEBUG(%s): [DNG] Node Create Start", __FUNCTION__);

    node->buf = new char[thumbnailSize];
    if (!node->buf) {
        ALOGE("ERR(%s):[DNG] memory alloc fail", __FUNCTION__);
        delete node;
        return NULL;
    }

    node->prev = NULL;
    node->next = NULL;
    node->size = 0;
    node->frameCount = -1;

    return node;
}

void SecCameraDngThumbnail::deleteNode(dng_thumbnail_t *node)
{
    if (node->buf)
        delete[] node->buf;

    delete node;
}

dng_thumbnail_t *SecCameraDngThumbnail::putNode(dng_thumbnail_t *node)
{
    dng_thumbnail_t *addedNode;
    Mutex::Autolock lock(m_processLock);

    addedNode = addtoList(node);
    m_processCondition.signal();

    return addedNode;
}

dng_thumbnail_t *SecCameraDngThumbnail::getNode(int frameCount)
{
    int retryCount = 9; /* 100ms * 9 */
    dng_thumbnail_t *curNode;

    ALOGD("DEBUG(%s): [DNG] find getNode : frameCount(%d)", __FUNCTION__, frameCount);

    while(retryCount > 0) {
        m_processLock.lock();
        curNode = m_head.next;

        while (curNode) {
            if (curNode->frameCount >= frameCount)
                break;

            curNode = curNode->next;
        }

        if(curNode) {
            ALOGD("DEBUG(%s): [DNG] Success DNG thumbnail(%d)", __FUNCTION__, retryCount);
            m_processLock.unlock();
            break;
        }
        retryCount--;
        ALOGD("DEBUG(%s): [DNG] Wait DNG thumbnail(%d)", __FUNCTION__, retryCount);

        m_processCondition.waitRelative(m_processLock, 100*1000*1000);
        m_processLock.unlock();
    }

    if(retryCount == 0) {
        ALOGE("ERR(%s): [DNG] Couldn't get Thumbnail Image", __FUNCTION__);
        return NULL;
    }

    Mutex::Autolock lock(m_thumbnailLock);

    if (curNode) {
        dng_thumbnail_t *tempNode;

        if (curNode->prev == NULL && curNode->next == NULL) {
            m_head.next = NULL;
            m_tail.prev = NULL;
        } else if (curNode->prev == NULL) {
            m_head.next = curNode->next;
            tempNode = curNode->next;
            tempNode->prev = NULL;
            curNode->next = NULL;
        } else if (curNode->next == NULL) {
            m_tail.prev = curNode->prev;
            tempNode = curNode->prev;
            tempNode->next = NULL;
            curNode->prev = NULL;
        } else {
            tempNode = curNode->prev;
            tempNode->next = curNode->next;
            tempNode = curNode->next;
            tempNode->prev = curNode->prev;
            curNode->next = NULL;
            curNode->prev = NULL;
        }
    }

    return curNode;
}

#endif /*SUPPORT_SAMSUNG_DNG*/

}; /* namespace android */