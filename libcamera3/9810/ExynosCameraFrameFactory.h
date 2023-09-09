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

#ifndef EXYNOS_CAMERA_FRAME_FACTORY_H
#define EXYNOS_CAMERA_FRAME_FACTORY_H

#include "ExynosCameraFrameFactoryBase.h"
#include "ExynosCameraPipeFlite.h"
#include "ExynosCameraPipeGSC.h"
#include "ExynosCameraPipeJpeg.h"
#include "ExynosCameraPipeVRA.h"
#include "ExynosCameraPipeHFD.h"

namespace android {

class ExynosCameraFrameFactory : public ExynosCameraFrameFactoryBase {
public:
    ExynosCameraFrameFactory()
    {
        m_init();
    }

    ExynosCameraFrameFactory(int cameraId, ExynosCameraParameters *param) : ExynosCameraFrameFactoryBase(cameraId, param)
    {
        m_init();
    }

public:
    virtual status_t        initPipes(void) = 0;
    virtual status_t        preparePipes(void) = 0;
    virtual status_t        startPipes(void) = 0;
    virtual status_t        stopPipes(void) = 0;
    virtual status_t        startInitialThreads(void) = 0;

    virtual enum NODE_TYPE  getNodeType(uint32_t pipeId);

    virtual ExynosCameraFrameSP_sptr_t createNewFrame(uint32_t frameCount, bool useJpegFlag = false) = 0;

    /* Helper functions for PipePP */
    virtual void            connectScenario(int pipeId, int scenario);
    virtual void            startScenario(int pipeId);
    virtual void            stopScenario(int pipeId, bool suspendFlag = false);
    virtual int             getScenario(int pipeId);

protected:
    virtual status_t        m_setupConfig(void) = 0;
    virtual status_t        m_constructPipes(void) = 0;

    /* flite pipe setting */
    virtual status_t        m_initFlitePipe(int sensorW, int sensorH, uint32_t frameRate);

    virtual int             m_getSensorId(unsigned int nodeNum, unsigned int connectionMode,
                                          bool flagLeader, bool reprocessing,
                                          int sensorScenario = SENSOR_SCENARIO_NORMAL);

    virtual int             m_getFliteNodenum(void);
#ifdef SUPPORT_DEPTH_MAP
    virtual int             m_getDepthVcNodeNum(void);
#endif

private:
    void                    m_init(void);
};

}; /* namespace android */

#endif
