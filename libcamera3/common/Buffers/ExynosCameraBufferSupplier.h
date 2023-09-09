/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_CAMERA_BUFFER_SUPPLIER_H__
#define EXYNOS_CAMERA_BUFFER_SUPPLIER_H__

#include <stdio.h>
#include <stdlib.h>
#include <utils/List.h>
#include <cutils/properties.h>
#include <list>

#include "ExynosCameraConfig.h"
#include "ExynosCameraList.h"
#include "ExynosCameraAutoTimer.h"
#include "ExynosCameraBuffer.h"
#include "ExynosCameraBufferManager.h"

namespace android {

using namespace std;

class ExynosCameraBufferSupplier : public ExynosCameraObject {
public:
    ExynosCameraBufferSupplier(int cameraId);
    virtual ~ExynosCameraBufferSupplier();

    status_t createBufferManager(const char *name, void *allocator,
                                 const buffer_manager_tag_t tag, void *stream = NULL, int actualFormat = -1);
    status_t resetBuffers(void);
    status_t resetBuffers(const buffer_manager_tag_t tag);
    void deinit(void);
    void deinit(const buffer_manager_tag_t tag);

    status_t setInfo(const buffer_manager_tag_t tag, const buffer_manager_configuration_t info);
    status_t alloc(const buffer_manager_tag_t tag);
    status_t getBuffer(const buffer_manager_tag_t tag, ExynosCameraBuffer *buffer);
    status_t putBuffer(ExynosCameraBuffer buffer);

    int getNumOfAvailableBuffer(const buffer_manager_tag_t tag);
    void dump(void);

private:
    ExynosCameraBufferSupplier();

    void m_deinit(void);
    ExynosCameraBufferManager* m_getBufferManager(const buffer_manager_tag_t tag);

private:
    typedef pair<buffer_manager_tag_t, ExynosCameraBufferManager *> buffer_manager_map_item_t;
    typedef list<buffer_manager_map_item_t> buffer_manager_map_t;
    typedef buffer_manager_map_t::iterator buffer_manager_map_iter_t;

    buffer_manager_map_t m_bufferMgrMap;
    Mutex m_bufferMgrMapLock;
};
}
#endif
