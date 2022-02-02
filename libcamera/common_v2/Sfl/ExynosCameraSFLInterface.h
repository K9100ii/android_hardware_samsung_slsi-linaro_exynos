/*
 * Copyright 2013, Samsung Electronics Co. LTD
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

/*!
 * \file      ExynosCameraSFLInterface.h
 * \brief     header file for ExynosCameraSFLInterface
 * \author    VijayaKumar S N (vijay.sathenahallin@samsung.com)
 * \date      2015/10/27
 *
 */
#ifndef EXYNOS_CAMERA_SFL_INTERFACE_H__
#define EXYNOS_CAMERA_SFL_INTERFACE_H__

#include <utils/Errors.h>
#include <utils/Mutex.h>
#include <utils/RefBase.h>
#include <cutils/log.h>
#include <list>
#include "ExynosCameraSFLdefine.h"
#include "ExynosCameraCommonDefine.h"
#include "ExynosCameraBuffer.h"

namespace android {

using namespace std;

struct CommandInfo {
    SFL::BufferType type;
    SFL::BufferPos  pos;
    SFL::Command    cmd; /* buffer index or value */
};

struct BufferInfo {
    SFL::BufferType type;
    SFL::BufferPos  pos;
    uint32_t        value; /* buffer index or value */
};

struct SFLBuffer {
    unsigned int pixelFormat;
    int  width;
    int  height;
    char *plane[4];
    int  size[4];
    int  stride[4];
    struct BufferInfo info;
    ExynosCameraBuffer buffer;
    int planeCount;
};

namespace FlawLess {
struct configInfo {
    bool faceBeauty;
    bool slimFace;
    bool skinBright;
    bool eyeEnlargment;
    uint32_t width;
    uint32_t height;
    uint32_t faceBeautyIntensity;
    uint32_t slimFaceIntensity;
    uint32_t skinBrightIntensity;
    uint32_t eyeEnlargmentIntensity;
};
};

#ifdef SFLINFO_TRACE
extern int sflInfoObjectCount;
extern Mutex sflInfoObjectCountLock;
#endif

class ExynosCameraSFLInfo : public virtual RefBase{
public:
    ExynosCameraSFLInfo(struct SFLBuffer *buffer, bool flagInternalBuffer = false) : m_flagIBuffer(flagInternalBuffer) {
        memset(&m_buffer, 0x00, sizeof(m_buffer));
        memcpy(&m_buffer, buffer, sizeof(m_buffer));

#ifdef SFLINFO_TRACE
        {
        Mutex::Autolock lock(sflInfoObjectCountLock);
        sflInfoObjectCount++;
        ALOGE("DEBUG(%s[%d]): sflInfo Object increase (%d) ",__FUNCTION__, __LINE__, sflInfoObjectCount);
        }
#endif
    }

    virtual ~ExynosCameraSFLInfo() {
        memset(&m_buffer, 0x00, sizeof(m_buffer));

#ifdef SFLINFO_TRACE
        {
        Mutex::Autolock lock(sflInfoObjectCountLock);
        sflInfoObjectCount--;
        ALOGE("DEBUG(%s[%d]): sflInfo Object decrease (%d) ",__FUNCTION__, __LINE__, sflInfoObjectCount);
        }
#endif
    }

    void setBuffer(struct SFLBuffer *buffer) {
        memcpy(&m_buffer, buffer, sizeof(m_buffer));
    }

    void getBuffer(struct SFLBuffer *buffer) {
        memcpy(buffer, &m_buffer, sizeof(m_buffer));
    }

    bool getBufferUsage() {
        return m_flagIBuffer;
    }

private:
    struct SFLBuffer m_buffer;
    bool m_flagIBuffer;
};

typedef sp<ExynosCameraSFLInfo> ExynosCameraSFLInfoType;


class ExynosCameraSFLInterface : public virtual RefBase {
public:
    ExynosCameraSFLInterface(const char* name, int cameraId) : m_cameraId(cameraId), m_enable(false), m_runningEnable(false) {
        memset(m_name, 0x00, sizeof(m_name));

        m_cameraId = cameraId;
        strncpy(m_name, name, EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }
    virtual ~ExynosCameraSFLInterface(){
        m_enable = false;
        m_runningEnable = false;
    }

    virtual SFLIBRARY_MGR::SFLType getType() = 0;
    virtual status_t init() = 0;
    virtual status_t deinit() = 0;
    virtual status_t registerCallbacks() = 0;
    virtual status_t prepare() = 0;                 /* adaptive MAX buffer count migration to MAX buffer */
    virtual status_t setMetaInfo(void* param) = 0;  /* update adaptive algorithm running info */
    virtual status_t processCommand(struct CommandInfo *cmdinfo, void* arg) = 0;
    virtual void     setEnable(bool enable) = 0;
    virtual bool     getEnable() = 0;
    virtual void     setRunEnable(bool enable) = 0;
    virtual bool     getRunEnable() = 0;
    virtual void     reset() = 0;

protected:
    int             m_cameraId;
    char            m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    bool            m_enable;
    bool            m_runningEnable;
};

}; //namespace android

#endif
