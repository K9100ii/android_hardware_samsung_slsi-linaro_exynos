/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraCallbackHooker"
#include <cutils/log.h>

#include "ExynosCameraCallbackHooker.h"

namespace android {

ExynosCameraCallbackHooker::ExynosCameraCallbackHooker(int cameraId)
{
    m_cameraId = cameraId;
    m_exynosCamera = NULL;

    m_hookingMsg = 0;
}

ExynosCameraCallbackHooker::~ExynosCameraCallbackHooker()
{
}

int ExynosCameraCallbackHooker::getCameraId(void)
{
    return m_cameraId;
}

void ExynosCameraCallbackHooker::setCallbacks(
                    __unused struct camera_device *dev,
                    camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    camera_request_memory get_memory,
                    void* user)
{
    m_originNotifyCb = notify_cb;
    m_originDataCb = data_cb;
    m_originDataCbTimestamp = data_cb_timestamp;
    m_originGetMemory = get_memory;
    m_originCbUser = user;

    m_exynosCamera = m_getExynosCamera(dev);

    CLOGD2("cameraId(%d)->setCallbacks(hooking callback)", m_cameraId);

    m_exynosCamera->setCallbacks(ExynosCameraCallbackHooker::m_notify_cb,
                                 ExynosCameraCallbackHooker::m_data_cb,
                                 ExynosCameraCallbackHooker::m_data_cb_timestamp,
                                 get_memory,
                                 this);
}

void ExynosCameraCallbackHooker::setHooking(bool flagHooking)
{
    // hook all msg.
    if (flagHooking == true)
        m_hookingMsg = ~(0);
    else
        m_hookingMsg = 0;
}

void ExynosCameraCallbackHooker::setHooking(int32_t msgType, bool flagHooking)
{
    if (flagHooking == true)
        m_hookingMsg |= msgType;
    else
        m_hookingMsg &= ~msgType;
}

bool ExynosCameraCallbackHooker::getHooking(void)
{
    bool ret = false;

    if (m_hookingMsg != 0)
        ret = true;

    return ret;
}

bool ExynosCameraCallbackHooker::getHooking(int32_t msgType)
{
    bool ret = false;

    if (m_hookingMsg & msgType)
        ret = true;

    return ret;
}

camera_notify_callback ExynosCameraCallbackHooker::m_getOriginNotifyCb(void)
{
    return m_originNotifyCb;
}

camera_data_callback ExynosCameraCallbackHooker::m_getOriginDataCb(void)
{
    return m_originDataCb;
}

camera_data_timestamp_callback ExynosCameraCallbackHooker::m_getOriginDataCbTimestamp(void)
{
    return m_originDataCbTimestamp;
}

void *ExynosCameraCallbackHooker::m_getOriginCbUser(void)
{
    return m_originCbUser;
}

ExynosCamera *ExynosCameraCallbackHooker::m_getExynosCamera(struct camera_device *dev)
{
    return reinterpret_cast<ExynosCamera *>(dev->priv);
}

ExynosCamera *ExynosCameraCallbackHooker::m_getExynosCamera(void)
{
    return m_exynosCamera;
}

void ExynosCameraCallbackHooker::m_notify_cb(int32_t msg_type,
                                    int32_t ext1,
                                    int32_t ext2,
                                    void *user)
{
    ExynosCameraCallbackHooker *callbackHooker = (ExynosCameraCallbackHooker *)user;
    int cameraId = callbackHooker->getCameraId();

    bool flagCallCallback = false;

    if (callbackHooker->getHooking() == false) {
        flagCallCallback = true;
    }

    /*
    if (msg_type == CAMERA_MSG_ERROR) {
        flagCallCallback = true;
    }
    */

    if (flagCallCallback == true) {
        camera_notify_callback notifyCb = callbackHooker->m_getOriginNotifyCb();
        if (notifyCb) {
            notifyCb(msg_type, ext1, ext2, callbackHooker->m_getOriginCbUser());
        }
    } else {
        CLOGD2("hooking cameraId(%d):notifyCb(msg_type(0x%x), ext1(%d), ext2(%d), user(%p)) come",
            cameraId, msg_type, ext1, ext2, user);
    }
}

void ExynosCameraCallbackHooker::m_data_cb(int32_t msg_type,
                                    const camera_memory_t *data,
                                    unsigned int index,
                                    camera_frame_metadata_t *metadata,
                                    void *user)
{
    ExynosCameraCallbackHooker *callbackHooker = (ExynosCameraCallbackHooker *)user;
    int cameraId = callbackHooker->getCameraId();

    if (callbackHooker->getHooking() == false) {
        camera_data_callback dataCb = callbackHooker->m_getOriginDataCb();
        if (dataCb) {
            CLOGD2("cameraId(%d):call original(no hooking) data_cb(msg_type(0x%x), data(%p), index(%d), metadata(%p), user(%p)) come",
                    cameraId, msg_type, data, index, metadata, user);
            dataCb(msg_type, data, index, metadata, callbackHooker->m_getOriginCbUser());
        }
    } else {
        CLOGD2("hooking cameraId(%d):data_cb(msg_type(0x%x), data(%p), index(%d), metadata(%p), user(%p)) come",
            cameraId, msg_type, data, index, metadata, user);

        /*
         * in frameworks/av/services/camera/libcameraservice/device1/CameraHardwareInterface.h,
         * originalUser is CameraHardwareInterface itself.
         */
        /*
        ExynosCamera *exynosCamera = callbackHooker->m_getExynosCamera();
        if (exynosCamera == NULL) {
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):exynosCamera == NULL, assert!!!!",
                __FUNCTION__, __LINE__, cameraId);
        }

        CameraParameters params = exynosCamera->getParameters();
        */
    }
}

void ExynosCameraCallbackHooker::m_data_cb_timestamp(nsecs_t timestamp,
                                    int32_t msg_type,
                                    const camera_memory_t *data,
                                    unsigned index,
                                    void *user)
{
    ExynosCameraCallbackHooker *callbackHooker = (ExynosCameraCallbackHooker *)user;
    int cameraId = callbackHooker->getCameraId();

    if (callbackHooker->getHooking() == false) {
        camera_data_timestamp_callback dataCbTimestamp = callbackHooker->m_getOriginDataCbTimestamp();
        if (dataCbTimestamp) {
            dataCbTimestamp(timestamp, msg_type, data, index, callbackHooker->m_getOriginCbUser());
        }
    } else {
        CLOGD2("hooking cameraId(%d):data_cb_timestamp(timestamp(%lld), msg_type(0x%x), data(%p), index(%d), user(%p) come",
            cameraId, timestamp, msg_type, data, index, user);
    }
}

}; /* namespace android */
