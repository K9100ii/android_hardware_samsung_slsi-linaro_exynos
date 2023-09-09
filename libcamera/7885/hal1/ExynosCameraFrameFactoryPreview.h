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

#ifndef EXYNOS_CAMERA_FRAME_FACTORY_PREVIEW_H
#define EXYNOS_CAMERA_FRAME_FACTORY_PREVIEW_H

#include "ExynosCameraFrameFactory.h"

#include "ExynosCameraFrame.h"

namespace android {

class ExynosCameraFrameFactoryPreview : public ExynosCameraFrameFactory {
public:
    ExynosCameraFrameFactoryPreview()
    {
        m_init();
    }

    ExynosCameraFrameFactoryPreview(int cameraId, ExynosCamera1Parameters *param) : ExynosCameraFrameFactory(cameraId, param)
    {
        m_init();

        const char *myName = (m_cameraId == CAMERA_ID_BACK) ? "FactoryBack" : "FactoryFront";
        strncpy(m_name, myName,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    }

public:
    virtual ~ExynosCameraFrameFactoryPreview();

    virtual status_t        create(bool active = true);
    virtual status_t        precreate(void);
    virtual status_t        postcreate(void);

    virtual status_t        fastenAeStable(int32_t numFrames, ExynosCameraBuffer *buffers);

    virtual status_t        initPipes(void);
    virtual status_t        mapBuffers(void);
    virtual status_t        preparePipes(void);

    virtual status_t        startPipes(void);
    virtual status_t        startSensor3AAPipe(void);
    virtual status_t        stopPipes(void);
    virtual status_t        startInitialThreads(void);
    virtual status_t        setStopFlag(void);

    virtual status_t        switchSensorMode(void);
    virtual status_t        finishSwitchSensorMode(void);

    virtual ExynosCameraFrameSP_sptr_t createNewFrame(ExynosCameraFrameSP_sptr_t refFrame = NULL);
#ifdef USE_DUAL_CAMERA
    virtual status_t        sensorStandby(bool standby);
#endif

protected:
    virtual status_t        m_setupConfig(bool isFastAE = false);
    virtual status_t        m_constructMainPipes(void);

    /* setting node number on every pipe */
    virtual status_t        m_setDeviceInfo(bool isFastAE = false);

    /* pipe setting */
    virtual status_t        m_initPipes(uint32_t frameRate);

    /* pipe setting for fastAE */
    virtual status_t        m_initPipesFastenAeStable(int32_t numFrames,
                                                      int hwSensorW,
                                                      int hwSensorH,
                                                      uint32_t frameRate);

    virtual status_t        m_setupRequestFlags(void);
    virtual status_t        m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame, ExynosCameraFrameSP_sptr_t refFrame = NULL);

#ifdef SUPPORT_GROUP_MIGRATION
    status_t        m_initNodeInfo(void);
    status_t        m_updateNodeInfo(void);
    void            m_dumpNodeInfo(const char* name, camera_node_objects_t* nodeInfo);
#endif
private:
    void                    m_init(void);

#ifdef SUPPORT_GROUP_MIGRATION
    camera_node_objects_t        m_nodeInfoTpu[MAX_NODE];
    camera_node_objects_t        m_nodeInfo[MAX_NODE];
    camera_node_objects_t        *m_curNodeInfo;
#endif

};

}; /* namespace android */

#endif
