/*
   **
   ** Copyright 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_FRAME_FACTORY_PREVIEW_FRONT_PIP_H
#define EXYNOS_CAMERA_FRAME_FACTORY_PREVIEW_FRONT_PIP_H

#include "ExynosCameraFrameFactoryPreview.h"

#include "ExynosCameraFrame.h"

namespace android {
class ExynosCameraFrameFactoryPreviewFrontPIP : public ExynosCameraFrameFactoryPreview {
public:
    ExynosCameraFrameFactoryPreviewFrontPIP()
    {
        m_init();
    }

    ExynosCameraFrameFactoryPreviewFrontPIP(int cameraId, ExynosCameraParameters *param)
        : ExynosCameraFrameFactoryPreview(cameraId, param)
    {
        m_init();

        const char *myName = "FactoryFrontPIP";
        strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }

public:
    virtual ~ExynosCameraFrameFactoryPreviewFrontPIP();

protected:
    /* setting node number on every pipe */
    virtual status_t        m_setDeviceInfo(void);

    /* pipe setting */
    virtual status_t        m_initPipes(uint32_t frameRate);

    virtual status_t        m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame);

private:
    void                    m_init(void);

};
}; /* namespace android */

#endif
