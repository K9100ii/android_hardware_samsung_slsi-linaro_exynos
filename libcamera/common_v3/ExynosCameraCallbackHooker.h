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

#ifndef EXYNOS_CAMERA_CALLBACK_HOOKER_H
#define EXYNOS_CAMERA_CALLBACK_HOOKER_H

#include <utils/RefBase.h>

#include "ExynosCamera.h"

namespace android {

#define MAX_NUM_OF_CAMERA (5)

/*
 * callback hooking class
 * it refer frameworks/av/services/camera/libcameraservice/device1/CameraHardwareInterface.h
 */
class ExynosCameraCallbackHooker {
private:
    ExynosCameraCallbackHooker()
    {
        android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):invalid call, assert!!!!", __FUNCTION__, __LINE__);
    }

public:
    ExynosCameraCallbackHooker(int cameraId);
    virtual ~ExynosCameraCallbackHooker();

    virtual int  getCameraId(void);

    virtual void setCallbacks(struct camera_device *dev,
                    camera_notify_callback notify_cb,
                    camera_data_callback data_cb,
                    camera_data_timestamp_callback data_cb_timestamp,
                    camera_request_memory get_memory,
                    void* user);

    virtual void setHooking(bool flagHooking);
    virtual void setHooking(int32_t msgType, bool flagHooking);

    virtual bool getHooking(void);
    virtual bool getHooking(int32_t msgType);

private:
    virtual camera_notify_callback          m_getOriginNotifyCb(void);
    virtual camera_data_callback            m_getOriginDataCb(void);
    virtual camera_data_timestamp_callback  m_getOriginDataCbTimestamp(void);
    virtual void                           *m_getOriginCbUser(void);

    virtual ExynosCamera *m_getExynosCamera(struct camera_device *dev);
    virtual ExynosCamera *m_getExynosCamera(void);

private:
    static void m_notify_cb(int32_t msg_type,
                    int32_t ext1,
                    int32_t ext2,
                    void *user);

    static void m_data_cb(int32_t msg_type,
                    const camera_memory_t *data,
                    unsigned int index,
                    camera_frame_metadata_t *metadata,
                    void *user);

    static void m_data_cb_timestamp(nsecs_t timestamp,
                    int32_t msg_type,
                    const camera_memory_t *data,
                    unsigned index,
                    void *user);

private:
    int                             m_cameraId;
    ExynosCamera                   *m_exynosCamera;

    int                             m_hookingMsg;

    camera_notify_callback          m_originNotifyCb;
    camera_data_callback            m_originDataCb;
    camera_data_timestamp_callback  m_originDataCbTimestamp;
    camera_request_memory           m_originRequestMemory;
    camera_request_memory           m_originGetMemory;

    void                           *m_originCbUser;
};

}; /* namespace android */
#endif
