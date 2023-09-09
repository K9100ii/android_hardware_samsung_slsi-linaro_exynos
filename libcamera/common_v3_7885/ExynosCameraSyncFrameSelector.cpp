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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraSyncFrameSelector"

#include "ExynosCameraSyncFrameSelector.h"

namespace android {

ExynosCameraSyncFrameSelector::ExynosCameraSyncFrameSelector(int cameraId,
                                                     ExynosCameraParameters *param,
                                                     ExynosCameraBufferManager *bufMgr,
                                                     ExynosCameraFrameManager *manager
#ifdef SUPPORT_DEPTH_MAP
                                                    , depth_callback_queue_t *depthCallbackQ
                                                    , ExynosCameraBufferManager *depthMapbufMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                                                    , ExynosCameraBufferManager *DNGbufMgr
#endif
                                                     ) :
                               ExynosCameraFrameSelector(cameraId, param, bufMgr, manager
#ifdef SUPPORT_DEPTH_MAP
                                                    , depthCallbackQ, depthMapbufMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                                                    , DNGbufMgr
#endif
                                                    )

{
    ExynosCameraFrameSelector(cameraId, param, bufMgr, manager
#ifdef SUPPORT_DEPTH_MAP
            , depthCallbackQ, depthMapbufMgr
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
            , DNGbufMgr
#endif
            );

    m_state = STATE_BASE;
    m_lastSyncType = SYNC_TYPE_BASE;
    m_dualFrameSelector = NULL;
    m_dualFrameSelector = ExynosCameraSingleton<ExynosCameraDualBayerFrameSelector>::getInstance();

    /* get the singleton dual selector and init */
    if (m_parameters->getDualCameraMode() == true &&
            m_parameters != NULL &&
            m_bufMgr != NULL) {
        int ret = 0;
        int masterCameraId, slaveCameraId;

        getDualCameraId(&masterCameraId, &slaveCameraId);

        m_dualFrameSelector->setFlagValidCameraId(masterCameraId, slaveCameraId);
        ret = m_dualFrameSelector->setInfo(m_cameraId,
                m_parameters,
                m_bufMgr,
                this);
        if (ret != NO_ERROR) {
            CLOGE("m_dualFrameSelector->setInfo(id:%d, param:%p, bufMgr:%p, selector:%p)",
                    m_cameraId, m_parameters, m_bufMgr, this);
        }

        ret = m_dualFrameSelector->setFrameHoldCount(m_cameraId, m_frameHoldCount, 1);
        if (ret != NO_ERROR) {
            CLOGE("m_dualFrameSelector->setFrameHoldCount(id:%d, holdCount:%d)",
                    m_cameraId, m_frameHoldCount);
        }
    }
}

ExynosCameraSyncFrameSelector::~ExynosCameraSyncFrameSelector()
{
    if (m_dualFrameSelector != NULL)
        m_dualFrameSelector->deinit(m_cameraId);
}

status_t ExynosCameraSyncFrameSelector::setFrameHoldCount(int32_t count)
{
    if (count < 0) {
        CLOGE("frame hold count cannot be negative value, current value(%d)",
                 count);
        return BAD_VALUE;
    }

    Mutex::Autolock lock(m_lock);

    CLOGD("holdCount : %d", count, m_syncFrameHoldCount);

    m_frameHoldCount = count + m_syncFrameHoldCount;

    return NO_ERROR;
}

status_t ExynosCameraSyncFrameSelector::setSyncFrameHoldCount(int32_t holdCountForSync)
{
    if (holdCountForSync < 0) {
        CLOGE("syncFrame hold count cannot be negative value, current value(%d)",
                 holdCountForSync);
        return BAD_VALUE;
    }

    /* do not anything if syncHoldCounts are same */
    if (holdCountForSync == m_syncFrameHoldCount)
        return NO_ERROR;

    Mutex::Autolock lock(m_lock);

    CLOGD("HoldCount : %d, syncHoldCount : %d", m_frameHoldCount, holdCountForSync);

    m_frameHoldCount = m_frameHoldCount - m_syncFrameHoldCount;
    m_syncFrameHoldCount = holdCountForSync;
    m_frameHoldCount = m_frameHoldCount + m_syncFrameHoldCount;

    return NO_ERROR;
}
}
