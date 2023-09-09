/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_3_FRAME_FACTORY_PREVIEW_H
#define EXYNOS_CAMERA_3_FRAME_FACTORY_PREVIEW_H

#include "ExynosCamera3FrameFactory.h"

#include "ExynosCameraFrame.h"

namespace android {

class ExynosCamera3FrameFactoryPreview : public ExynosCamera3FrameFactory {
public:
    ExynosCamera3FrameFactoryPreview()
    {
        m_init();
    }

    ExynosCamera3FrameFactoryPreview(int cameraId, ExynosCamera3Parameters *param) : ExynosCamera3FrameFactory(cameraId, param)
    {
        m_init();

        const char *myName = (m_cameraId == CAMERA_ID_BACK) ? "FactoryBack" : "FactoryFront";
        strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }

public:
    virtual ~ExynosCamera3FrameFactoryPreview();

    virtual status_t        create(void);
    virtual status_t        precreate(void);
    virtual status_t        postcreate(void);

    virtual status_t        fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers);

    virtual status_t        initPipes(void);
    virtual status_t        mapBuffers(void);
    virtual status_t        preparePipes(void);

    virtual status_t        startPipes(void);
    virtual status_t        stopPipes(void);
    virtual status_t        startInitialThreads(void);
    virtual status_t        setStopFlag(void);

    virtual ExynosCameraFrameSP_sptr_t createNewFrame(uint32_t frameCount = 0);

protected:
    virtual status_t        m_setupConfig(void);
    virtual status_t        m_constructMainPipes(void);

    /* setting node number on every pipe */
    virtual status_t        m_setDeviceInfo(void);

    /* pipe setting */
    virtual status_t        m_initPipes(uint32_t frameRate);

    /* pipe setting for fastAE */
    virtual status_t        m_initPipesFastenAeStable(int32_t numFrames,
                                                      int hwSensorW,
                                                      int hwSensorH,
                                                      uint32_t frameRate);

    virtual status_t        m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame);
private:
    void                    m_init(void);

};

}; /* namespace android */

#endif
