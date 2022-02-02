/*
**
** Copyright 2016, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_PIPE_Sync_H
#define EXYNOS_CAMERA_PIPE_Sync_H

#include "ExynosCameraSWPipe.h"

namespace android {

#define SYNC_TIMESTAMP_CALB_TIME_MARGIN  (2)   /* about 2ms */
#define SYNC_TIMESTAMP_CALB_TIME_DEFAULT (30)  /* about 30ms */
#define SYNC_REMOVE_TIMESTAMP_DIFF       (120) /* about 120ms */
class ExynosCameraPipeSync : public ExynosCameraSWPipe {
public:
    typedef enum SYNC_TYPE {
        SYNC_TYPE_INIT,
        SYNC_TYPE_FIFO,         /* just match queued frames */
        SYNC_TYPE_FRAMECOUNT,   /* match with frames which have same framecount */
        SYNC_TYPE_TIMESTAMP,    /* match with frames which have similar timestamp */
        SYNC_TYPE_MAX,
    } sync_type_t;

    ExynosCameraPipeSync()
    {
        m_init();
    }

    ExynosCameraPipeSync(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        start(void);
    virtual status_t        stop(void);

protected:
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    void                    m_init(void);

    void                    m_pushFrameToOutputQ(ExynosCameraFrameSP_sptr_t frame);
    status_t                m_cleanFrameBySyncType(frame_queue_t *frameQ, ExynosCameraFrameSP_sptr_t baseFrame, sync_type_t syncType, bool flagSaveFrame);
    status_t                m_cleanFrameByMinCount(frame_queue_t *frameQ, int minCount);
    status_t                m_cleanFrameByTimeStamp(frame_queue_t *frameQ, uint64_t removeTimeStamp);
    status_t                m_syncFrame(ExynosCameraFrameSP_sptr_t newFrame, sync_type_t syncType, ExynosCameraFrameSP_dptr_t finalFrame);
    void                    m_makeOneMasterFrame(ExynosCameraFrameSP_sptr_t masterFrame, ExynosCameraFrameSP_sptr_t slaveFrame);

private:
    frame_queue_t           *m_masterFrameQ;
    frame_queue_t           *m_slaveFrameQ;
    uint64_t                 m_lastTimeStamp;
    uint32_t                m_lastFrameCount;
    sync_type_t             m_syncType;
    uint32_t                m_lastFrameType;

    /* unit(ms), based on max fps */
    uint32_t                m_syncTimestampMinCalbTime;
    /* unit(ms), based on min fps */
    uint32_t                m_syncTimestampMaxCalbTime;
    /*
     * unit(ms)
     * if current frameduration(ms) is bigger than this value,
     * syncCalbTime would be changed to syncTimestampMaxCalbTime.
     */
    uint32_t                m_syncCalbTimeThreshold;
    uint64_t                m_syncLastTimeStamp[CAMERA_ID_MAX];
    bool                    m_needMaxCalbTime[CAMERA_ID_MAX];
};

}; /* namespace android */

#endif
