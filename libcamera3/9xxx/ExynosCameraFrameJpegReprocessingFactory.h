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

#ifndef EXYNOS_CAMERA_FRAME_JPEG_REPROCESSING_FACTORY_H
#define EXYNOS_CAMERA_FRAME_JPEG_REPROCESSING_FACTORY_H

#include "ExynosCameraFrameFactory.h"

namespace android {

class ExynosCameraFrameJpegReprocessingFactory : public ExynosCameraFrameFactory {
public:
    ExynosCameraFrameJpegReprocessingFactory()
    {
        m_init();
    }

    ExynosCameraFrameJpegReprocessingFactory(int cameraId, ExynosCameraConfigurations *configurations, ExynosCameraParameters *param) : ExynosCameraFrameFactory(cameraId, configurations, param)
    {
        m_init();

        switch (m_cameraId) {
        case CAMERA_ID_BACK:
        case CAMERA_ID_FRONT:
            strncpy(m_name, "JpegReprocessingFactory",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
            break;
#ifdef USE_DUAL_CAMERA
        case CAMERA_ID_BACK_1:
        case CAMERA_ID_FRONT_1:
            strncpy(m_name, "JpegReprocessingFactorySlave",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
            break;
#endif
        default:
            CLOGE("Invalid camera ID(%d)", m_cameraId);
            break;
        }
    }

    virtual ~ExynosCameraFrameJpegReprocessingFactory();

    virtual status_t        create(void);

    virtual status_t        initPipes(void);
    virtual status_t        preparePipes(void);

    virtual status_t        startPipes(void);
    virtual status_t        stopPipes(void);
    virtual status_t        startInitialThreads(void);
    virtual status_t        setStopFlag(void);

    virtual ExynosCameraFrameSP_sptr_t createNewFrame(uint32_t frameCount, bool useJpegFlag = false);

    /* Helper functions for PipePP */
    virtual void            connectScenario(int pipeId, int scenario);
    virtual void            startScenario(int pipeId);
    virtual void            stopScenario(int pipeId, bool suspendFlag = false);
    virtual int             getScenario(int pipeId);

protected:
    virtual status_t        m_setupConfig(void);
    virtual status_t        m_constructPipes(void);
    virtual status_t        m_initFrameMetadata(ExynosCameraFrameSP_sptr_t frame);
    virtual status_t        m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame);

private:
    void                    m_init(void);

protected:
    bool                    m_flagHWFCEnabled;

private:
    struct camera2_shot_ext    *m_shot_ext;
};

}; /* namespace android */

#endif
