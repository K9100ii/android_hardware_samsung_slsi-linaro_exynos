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

#ifndef EXYNOS_CAMERA_SYNC_SELECTOR_H
#define EXYNOS_CAMERA_SYNC_SELECTOR_H

#include "ExynosCameraFrameSelector.h"
#include "ExynosCameraDualFrameSelector.h"

namespace android {

using namespace std;

class ExynosCameraSyncFrameSelector : public ExynosCameraFrameSelector {
public:
    ExynosCameraSyncFrameSelector (int cameraId,
                            ExynosCameraParameters *param,
                            ExynosCameraBufferManager *bufMgr, ExynosCameraFrameManager *manager = NULL
#ifdef SUPPORT_DEPTH_MAP
                            , depth_callback_queue_t *depthCallbackQ = NULL
                            , ExynosCameraBufferManager *depthMapbufMgr = NULL
#endif
#if defined(SAMSUNG_DNG_DIRTY_BAYER) || defined(DEBUG_RAWDUMP_DIRTY_BAYER)
                            , ExynosCameraBufferManager *DNGbufMgr = NULL
#endif
                            );
    ~ExynosCameraSyncFrameSelector();

    virtual status_t release(void);
    virtual status_t manageFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos = 0);
    virtual status_t clearList(int pipeID = -1 , bool isSrc = false, int32_t dstPos = 0);
    virtual status_t manageSyncFrameHoldList(ExynosCameraFrameSP_sptr_t frame, int pipeID, bool isSrc, int32_t dstPos = 0);
    virtual result_t selectFrames(ExynosCameraFrameSP_dptr_t selectedFrame, int count, int pipeID, bool isSrc, int tryCount, int32_t dstPos = 0);
    /*
     * In case of sync mode, specific frame have to be reserved.
     *  ex. Maybe there's a situation that
     *      Master Camera select N frame and process the frame in takePicture,
     *      Slave Camera haven't selected N frame yet. But new bayer buffer was
     *      out from driver and ExynosCamera push the frame to selector.
     *      In this situation, we want not to remove N frame (holdCount = 1).
     */
    virtual status_t setSyncFrameHoldCount(int32_t holdCountForSync);
    virtual status_t setFrameHoldCount(int32_t count);

private:
    ExynosCameraDualFrameSelector *m_dualFrameSelector;
};
}
#endif

