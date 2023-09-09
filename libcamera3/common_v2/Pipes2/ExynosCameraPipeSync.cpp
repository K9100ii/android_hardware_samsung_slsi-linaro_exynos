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
#define LOG_TAG "ExynosCameraPipeSync"
#include <log/log.h>

#include "ExynosCameraPipeSync.h"

namespace android {

status_t ExynosCameraPipeSync::create(int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::create(sensorIds);
    if (ret != NO_ERROR) {
        CLOGE("SWPipe creation fail!");
        return ret;
    }

    m_masterFrameQ = new frame_queue_t;
    m_slaveFrameQ = new frame_queue_t;

    m_inputFrameQ->setWaitTime(100000000);

    if (m_reprocessing) {
        /* Actually, it sync any frame if there's a pair frame */
        m_syncType = SYNC_TYPE_FIFO;
    } else {
        m_syncType = SYNC_TYPE_TIMESTAMP;
    }

    m_syncTimestampMinCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;
    m_syncTimestampMaxCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;

    CLOGI("create() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::start(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;
    uint32_t minFps = 30;
    uint32_t maxFps = 30;

    ret = ExynosCameraSWPipe::start();
    if (ret != NO_ERROR) {
        CLOGE("SWPipe start fail!");
        return ret;
    }

    m_cleanFrameByMinCount(m_slaveFrameQ, 0);
    m_cleanFrameByMinCount(m_masterFrameQ, 0);

    m_lastTimeStamp = 0;
    m_lastFrameCount = 0;

    /* find out current preview fps */
    m_configurations->getPreviewFpsRange(&minFps, &maxFps);
    m_syncTimestampMinCalbTime = (1000 / maxFps / 2) + SYNC_TIMESTAMP_CALB_TIME_MARGIN;
    m_syncTimestampMaxCalbTime = (1000 / minFps / 2) + SYNC_TIMESTAMP_CALB_TIME_MARGIN;
    m_syncCalbTimeThreshold = 1000 / maxFps + SYNC_TIMESTAMP_CALB_TIME_MARGIN;

    /* init each camera's lastTimeStamp info for sync */
    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_syncLastTimeStamp[i] = 0;
        m_needMaxCalbTime[i] = false;
    }

    CLOGD("calbTime(%d/%d ms), fps(%d, %d)",
            m_syncTimestampMinCalbTime, m_syncTimestampMaxCalbTime, minFps, maxFps);

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::stop(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::stop();
    if (ret != NO_ERROR) {
        CLOGE("SWPipe stop fail!");
        return ret;
    }

    m_cleanFrameByMinCount(m_slaveFrameQ, 0);
    m_cleanFrameByMinCount(m_masterFrameQ, 0);

    m_lastTimeStamp = 0;
    m_lastFrameCount = 0;
    m_syncTimestampMinCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;
    m_syncTimestampMaxCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;

    return NO_ERROR;
}

status_t ExynosCameraPipeSync::m_destroy(void)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::m_destroy();
    if (ret != NO_ERROR)
        CLOGE("Destroy fail!");

    if (m_masterFrameQ != NULL) {
        m_masterFrameQ->release();
        delete m_masterFrameQ;
        m_masterFrameQ = NULL;
    }

    if (m_slaveFrameQ != NULL) {
        m_slaveFrameQ->release();
        delete m_slaveFrameQ;
        m_slaveFrameQ = NULL;
    }

    return ret;
}

status_t ExynosCameraPipeSync::m_run(void)
{
    status_t ret = NO_ERROR;

    uint64_t curMs = 0;
    int cameraId = -1;
    static int internalLogCount[CAMERA_ID_MAX];
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameSP_sptr_t syncFrame = NULL;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            enum DUAL_OPERATION_MODE dualOperationMode =
                m_configurations->getDualOperationMode();

            if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                CLOGW("wait timeout");
                return ret;
            }

            if (m_reprocessing) {
                CLOGW("wait timeout for reprocessing(%d)",
                        m_configurations->getDualOperationModeReprocessing());
                return ret;
            }

            m_cleanFrameByMinCount(m_slaveFrameQ, 0);
            m_cleanFrameByMinCount(m_masterFrameQ, 0);
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return BAD_VALUE;
    }

    cameraId = newFrame->getCameraId();
    if (cameraId < 0 || cameraId >= CAMERA_ID_MAX) {
        CLOGE("new frame is inavlid cameraId(%d)", cameraId);
        return BAD_VALUE;
    }

    /*
     * Calculate this camera's current frameduration.
     * This information would be used to select min/max calbTime.
     */
    curMs = (uint64_t)ns2ms(newFrame->getTimeStampBoot());
    if (m_syncType == SYNC_TYPE_TIMESTAMP &&
            m_configurations->getDualHwSyncOn() == false) {
        bool needMaxCalbTime = false;
        uint64_t lastMs = m_syncLastTimeStamp[cameraId];
        int64_t diffMs = curMs - lastMs;

        /* for abs */
        if (diffMs < 0)
            diffMs = -diffMs;

        if (diffMs > m_syncCalbTimeThreshold)
            needMaxCalbTime = true;

        m_needMaxCalbTime[cameraId] = needMaxCalbTime;
    }
#if 0
    CLOGD("input frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d)",
            newFrame->getCameraId(),
            newFrame->getFrameCount(),
            newFrame->getMetaFrameCount(),
            newFrame->getFrameType(),
            newFrame->getTimeStampBoot() / 1000000LL,
            newFrame->getFrameState());
#endif

    /* remove under this timestamp in master, slaveQ */
    uint64_t removeTimeStamp = (uint64_t)ns2ms(newFrame->getTimeStampBoot());
    if (removeTimeStamp > SYNC_REMOVE_TIMESTAMP_DIFF)
        removeTimeStamp -= SYNC_REMOVE_TIMESTAMP_DIFF;
    else
        removeTimeStamp = 0;

    /* check the frame state */
    switch (newFrame->getFrameState()) {
    case FRAME_STATE_INVALID:
    case FRAME_STATE_SKIPPED:
        CLOGE("[F%d,T%d] This frame's state is invalid(%d)",
                newFrame->getFrameCount(), newFrame->getFrameType(), newFrame->getFrameState());
        newFrame->setFrameState(FRAME_STATE_SKIPPED);
        m_cleanFrameBySyncType(m_slaveFrameQ, newFrame, m_syncType, false);
        m_cleanFrameBySyncType(m_masterFrameQ, newFrame, m_syncType, false);
        m_pushFrameToOutputQ(newFrame);
        goto p_exit;
        break;
    default:
        break;
    }

    /* check the frame type */
    switch (newFrame->getFrameType()) {
    case FRAME_TYPE_INTERNAL:
    case FRAME_TYPE_INTERNAL_SLAVE:
        if ((internalLogCount[m_cameraId]++ % 100) == 0) {
            CLOGI("[INTERNAL_FRAME] frame(%d) type(%d), (%d)",
                    newFrame->getFrameCount(), newFrame->getFrameType(), internalLogCount[m_cameraId]);
        }
        newFrame->setFrameState(FRAME_STATE_SKIPPED);
        m_cleanFrameBySyncType(m_slaveFrameQ, newFrame, m_syncType, false);
        m_cleanFrameBySyncType(m_masterFrameQ, newFrame, m_syncType, false);
        break;
    /*
     * Case 1. FRAME_TYPE_PREVIEW
     *      => save all masterFrameQ ( > m_lastFrameCount ) and clean slaveFrameQ
     * Case 2. FRAME_TYPE_PREVIEW_SLAVE
     *      => save all slaveFrameQ ( > m_lastFrameCount ) and clean masterFrameQ
     * Case 3. FRAME_TYPE_INTERNAL
     *      => clean masterFrameQ and clean slaveFrameQ ( <= slaveFrameCount )
     * Case 4. FRAME_TYPE_INTERNAL_SLAVE
     *      => clean slaveFrameQ and clean masterFrameQ ( <= masterFrameCount )
     * Case 5. Other
     *      => just push master or slaveFrameQ
     */
    case FRAME_TYPE_PREVIEW:
    case FRAME_TYPE_PREVIEW_SLAVE:
    case FRAME_TYPE_REPROCESSING:
    case FRAME_TYPE_REPROCESSING_SLAVE:
        if (newFrame->getFrameType() == FRAME_TYPE_PREVIEW ||
                newFrame->getFrameType() == FRAME_TYPE_REPROCESSING) {
            /* I can save master frame */
            m_cleanFrameBySyncType(m_masterFrameQ, newFrame, m_syncType, true);
            m_cleanFrameBySyncType(m_slaveFrameQ, newFrame, m_syncType, false);
        } else {
            /* I can't save slave frame cause of no service buffer */
            m_cleanFrameBySyncType(m_slaveFrameQ, newFrame, m_syncType, false);
            m_cleanFrameBySyncType(m_masterFrameQ, newFrame, m_syncType, false);
        }
        m_pushFrameToOutputQ(newFrame);
        break;
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
        ret = m_syncFrame(newFrame, m_syncType, syncFrame);
        if (syncFrame != NULL)
            m_pushFrameToOutputQ(syncFrame);
        break;
    case FRAME_TYPE_JPEG_REPROCESSING:
        goto p_exit;
    default:
        CLOGE("[F%d] invalid frameType(%d)", newFrame->getFrameCount(), newFrame->getFrameType());
        break;
    }


    if (!(newFrame->getFrameType() == FRAME_TYPE_INTERNAL || newFrame->getFrameType() == FRAME_TYPE_INTERNAL_SLAVE))
        internalLogCount[m_cameraId] = 0;

p_exit:
    /* If frameQ have frame over expected count, clean Q */
#ifdef SAMSUNG_DUAL_PORTRAIT_LLS_CAPTURE
    if ((m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT
        || m_configurations->getScenario() == SCENARIO_DUAL_FRONT_PORTRAIT)
            && m_reprocessing == true) {
        int LDCaptureTotalCount = m_configurations->getModeValue(CONFIGURATION_LD_CAPTURE_COUNT);

        if (LDCaptureTotalCount > 0) {
            m_cleanFrameByMinCount(m_slaveFrameQ, LDCaptureTotalCount + 1);
            m_cleanFrameByMinCount(m_masterFrameQ, LDCaptureTotalCount + 1);
        } else {
            m_cleanFrameByMinCount(m_slaveFrameQ, SYNC_WAITING_COUNT);
            m_cleanFrameByMinCount(m_masterFrameQ, SYNC_WAITING_COUNT);
        }
    } else
#endif
    {
        m_cleanFrameByMinCount(m_slaveFrameQ, SYNC_WAITING_COUNT);
        m_cleanFrameByMinCount(m_masterFrameQ, SYNC_WAITING_COUNT);
    }

    /*
     * Clean old frame in both FrameQ.
     * Because service might stop request and wait remained frames
     * in each FrameQ that wait for sync.
     */
    if (m_syncType != SYNC_TYPE_FIFO) {
        m_cleanFrameByTimeStamp(m_slaveFrameQ, removeTimeStamp);
        m_cleanFrameByTimeStamp(m_masterFrameQ, removeTimeStamp);
    }

    /* save the last timestamp for sync */
    m_syncLastTimeStamp[cameraId] = curMs;

    return NO_ERROR;
}

void ExynosCameraPipeSync::m_init(void)
{
    m_lastTimeStamp = 0;
    m_lastFrameCount = 0;
    m_syncTimestampMinCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;
    m_syncTimestampMaxCalbTime = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;
    m_lastFrameType = FRAME_TYPE_BASE;
    m_masterFrameQ = NULL;
    m_slaveFrameQ = NULL;
    m_syncType = SYNC_TYPE_INIT;
    m_syncCalbTimeThreshold = SYNC_TIMESTAMP_CALB_TIME_DEFAULT;
}

void ExynosCameraPipeSync::m_pushFrameToOutputQ(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret;
    uint32_t currentFrameCount = frame->getFrameCount();
    uint64_t currentTimeStamp = (uint64_t)frame->getTimeStampBoot();

#if 0
    CLOGD("output frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d) last(%d, %d, %d)",
            frame->getCameraId(),
            frame->getFrameCount(),
            frame->getMetaFrameCount(),
            frame->getFrameType(),
            frame->getTimeStampBoot() / 1000000LL,
            frame->getFrameState(),
            m_lastFrameCount,
            m_lastTimeStamp / 1000000LL,
            m_lastFrameType);
#endif


    switch (frame->getFrameState()) {
    case FRAME_STATE_SKIPPED:
    case FRAME_STATE_INVALID:
        CLOGV("drop this frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d) last(%d, %d, %d)",
                frame->getCameraId(),
                frame->getFrameCount(),
                frame->getMetaFrameCount(),
                frame->getFrameType(),
                frame->getTimeStampBoot() / 1000000LL,
                frame->getFrameState(),
                m_lastFrameCount,
                m_lastTimeStamp / 1000000LL,
                m_lastFrameType);
        break;
    default:
        if (!(frame->getFrameType() == FRAME_TYPE_PREVIEW_DUAL_SLAVE ||
            frame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_SLAVE)) {
            if (m_lastTimeStamp > currentTimeStamp) {
                CLOGE("detected reversed frame!! (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d) last(%d, %d, %d)",
                        frame->getCameraId(),
                        frame->getFrameCount(),
                        frame->getMetaFrameCount(),
                        frame->getFrameType(),
                        frame->getTimeStampBoot() / 1000000LL,
                        frame->getFrameState(),
                        m_lastFrameCount,
                        m_lastTimeStamp / 1000000LL,
                        m_lastFrameType);
                frame->setFrameState(FRAME_STATE_SKIPPED);
            } else {
                m_lastFrameCount = currentFrameCount;
                m_lastTimeStamp = currentTimeStamp;
                m_lastFrameType = frame->getFrameType();
            }
        }
        break;
    }

    ret = frame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) frameCount%d fail", getPipeId(), frame->getFrameCount());
    }

    m_outputFrameQ->pushProcessQ(&frame);
}

/*
 * clean frameQ
 * @frameQ : Target FrameQ
 * @frame : Base Frame
 *          If caller specified this frame is not NULL, this function clean all frames older than base frame.
 *          If caller specified this frame is NULL, this function clean all frames.
 * @syncType : method to compare which frame is older than base frame
 *            (SYNC_TYPE_TIMESTAMP, SYNC_TYPE_FRAMECOUNT)
 * @flagSaveFrame : If it's true, older frames will be saved by changing frameType.
 */
status_t ExynosCameraPipeSync::m_cleanFrameBySyncType(frame_queue_t *frameQ, ExynosCameraFrameSP_sptr_t baseFrame,
                                                      sync_type_t syncType, bool flagSaveFrame)
{
    if (frameQ == NULL) {
        CLOGE("frameQ is null");
        return INVALID_OPERATION;
    }

    status_t ret;
    int numOfProcessQ = frameQ->getSizeOfProcessQ();
    ExynosCameraFrameSP_sptr_t tempFrame = NULL;
    bool flagHasBaseFrame = false;
    bool flagClean = false;
    uint64_t baseTime, tempTime;

    /* clean all frames */
    if (baseFrame != NULL)
        flagHasBaseFrame = true;

    for (int i = 0; i < numOfProcessQ; i++) {
        tempFrame = NULL;
        ret = frameQ->popProcessQ(&tempFrame);
        if (ret != NO_ERROR) {
            CLOGE("popProcessQ fail!, ret(%d)", ret);
        }

        if (flagHasBaseFrame) {
            switch (syncType) {
            case SYNC_TYPE_FIFO:
                /* don't remove */
                break;
            case SYNC_TYPE_FRAMECOUNT:
                if (tempFrame->getFrameCount() < baseFrame->getFrameCount())
                    flagClean = true;
                else
                    flagClean = false;
                break;
            case SYNC_TYPE_TIMESTAMP:
                baseTime = (uint64_t)ns2ms(baseFrame->getTimeStampBoot());
                tempTime = (uint64_t)ns2ms(tempFrame->getTimeStampBoot());
                if (tempTime < baseTime)
                    flagClean = true;
                else
                    flagClean = false;
                break;
            default:
                CLOGE("invalid syncType(%d)", syncType);
                flagClean = true;
                break;
            }
        } else {
            /* all clean */
            flagClean = true;
        }

        if (flagClean) {
            /* save the frame by changing frameType */
            if (flagSaveFrame) {
                switch (tempFrame->getFrameType()) {
                case FRAME_TYPE_PREVIEW_DUAL_MASTER:
                    tempFrame->setFrameType(FRAME_TYPE_PREVIEW);
                    break;
                case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
                    tempFrame->setFrameType(FRAME_TYPE_PREVIEW_SLAVE);
                    break;
                case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
                    tempFrame->setFrameType(FRAME_TYPE_REPROCESSING);
                    break;
                case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
                    tempFrame->setFrameType(FRAME_TYPE_REPROCESSING_SLAVE);
                    break;
                default:
                    CLOGE("[F%d] invalid frameType(%d)",
                            tempFrame->getFrameCount(), tempFrame->getFrameType());
                    break;
                }
            } else {
                tempFrame->setFrameState(FRAME_STATE_SKIPPED);
            }
#if 0
            CLOGD("clean frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d)",
                    tempFrame->getCameraId(),
                    tempFrame->getFrameCount(),
                    tempFrame->getMetaFrameCount(),
                    tempFrame->getFrameType(),
                    tempFrame->getTimeStampBoot() / 1000000LL,
                    tempFrame->getFrameState());
#endif
            m_pushFrameToOutputQ(tempFrame);
        } else {
            frameQ->pushProcessQ(&tempFrame);
        }
    }

    return NO_ERROR;
}

/*
 * clean frameQ
 * @frameQ : Target FrameQ
 * @minCount : minimum count to keep in this frameQ
 *             if this value is 0, clean all frameQ
 */
status_t ExynosCameraPipeSync::m_cleanFrameByMinCount(frame_queue_t *frameQ, int minCount)
{
    if (frameQ == NULL) {
        CLOGE("frameQ is null");
        return INVALID_OPERATION;
    }

    if (minCount < 0) {
        CLOGE("invalid minCount(%d)", minCount);
        return INVALID_OPERATION;
    }

    /* clean all frames */
    status_t ret;
    ExynosCameraFrameSP_sptr_t tempFrame = NULL;
    int numOfProcessQ = frameQ->getSizeOfProcessQ();
    if (minCount >= numOfProcessQ)
        return NO_ERROR;

    for (int i = 0; i < (numOfProcessQ - minCount); i++) {
        tempFrame = NULL;
        ret = frameQ->popProcessQ(&tempFrame);
        if (ret != NO_ERROR) {
            CLOGE("popProcessQ fail!, ret(%d)", ret);
        }

        tempFrame->setFrameState(FRAME_STATE_SKIPPED);
#if 0
        CLOGD("clean frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d)",
                tempFrame->getCameraId(),
                tempFrame->getFrameCount(),
                tempFrame->getMetaFrameCount(),
                tempFrame->getFrameType(),
                tempFrame->getTimeStampBoot() / 1000000LL,
                tempFrame->getFrameState());
#endif
        m_pushFrameToOutputQ(tempFrame);
    }

    return NO_ERROR;
}

/*
 * clean frameQ
 * @removeTimeStamp : baseline of timestamp (ms)
 */
status_t ExynosCameraPipeSync::m_cleanFrameByTimeStamp(frame_queue_t *frameQ, uint64_t removeTimeStamp)
{
    if (frameQ == NULL) {
        CLOGE("frameQ is null");
        return INVALID_OPERATION;
    }

    status_t ret;
    int numOfProcessQ = frameQ->getSizeOfProcessQ();
    ExynosCameraFrameSP_sptr_t tempFrame = NULL;
    uint64_t tempTime;
    bool flagClean;

    for (int i = 0; i < numOfProcessQ; i++) {
        tempFrame = NULL;
        ret = frameQ->popProcessQ(&tempFrame);
        if (ret != NO_ERROR) {
            CLOGE("popProcessQ fail!, ret(%d)", ret);
        }

        tempTime = (uint64_t)ns2ms(tempFrame->getTimeStampBoot());
        if (tempTime <= removeTimeStamp)
            flagClean = true;
        else
            flagClean = false;

        if (flagClean) {
            CLOGV("clean frame (Cam:%d, Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld, State:%d)",
                    tempFrame->getCameraId(),
                    tempFrame->getFrameCount(),
                    tempFrame->getMetaFrameCount(),
                    tempFrame->getFrameType(),
                    tempFrame->getTimeStampBoot() / 1000000LL,
                    tempFrame->getFrameState());
            tempFrame->setFrameState(FRAME_STATE_SKIPPED);
            m_pushFrameToOutputQ(tempFrame);
        } else {
            frameQ->pushProcessQ(&tempFrame);
        }
    }

    return NO_ERROR;
}

/* This function return unified frame (master frame which have slave buffer) */
status_t ExynosCameraPipeSync::m_syncFrame(ExynosCameraFrameSP_sptr_t newFrame, sync_type_t syncType,
                                           ExynosCameraFrameSP_dptr_t finalFrame)
{
    bool iamFound = false;
    bool iamMasterFrame = false;
    uint64_t myTime, pairTime;
    int64_t diffTimeMs;
    int64_t syncCalbTime;

    frame_queue_t *myFrameQ = NULL;
    frame_queue_t *pairFrameQ = NULL;
    ExynosCameraFrameSP_sptr_t pairFrame = NULL;

    switch (newFrame->getFrameType()) {
    case FRAME_TYPE_PREVIEW_DUAL_MASTER:
    case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
        iamMasterFrame = true;
        myFrameQ = m_masterFrameQ;
        pairFrameQ = m_slaveFrameQ;
        break;
    case FRAME_TYPE_PREVIEW_DUAL_SLAVE:
    case FRAME_TYPE_REPROCESSING_DUAL_SLAVE:
        myFrameQ = m_slaveFrameQ;
        pairFrameQ = m_masterFrameQ;
        break;
    default:
        CLOG_ASSERT("[F%d] invalid frameType(%d)", newFrame->getFrameCount(), newFrame->getFrameType());
        break;
    }

    /* let's find my pair frame by syncType */
    raw_frame_queue_t *rawPairFrameQ = pairFrameQ->getRawProcessList();

    pairFrameQ->rawLockProcessList(); /* lock */
    for (raw_frame_queue_t::iterator r = rawPairFrameQ->begin(); r != rawPairFrameQ->end(); ++r) {
        pairFrame = *r;

        switch (syncType) {
        case SYNC_TYPE_FIFO:
            iamFound = true;
            break;
        case SYNC_TYPE_FRAMECOUNT:
            if (pairFrame->getFrameCount() == newFrame->getFrameCount())
                iamFound = true;
            break;
        case SYNC_TYPE_TIMESTAMP:
            myTime = (uint64_t)ns2ms(newFrame->getTimeStampBoot());
            pairTime = (uint64_t)ns2ms(pairFrame->getTimeStampBoot());

            diffTimeMs = myTime - pairTime;
            /* for abs */
            if (diffTimeMs < 0) {
                diffTimeMs = -diffTimeMs;
            }

            syncCalbTime = m_syncTimestampMinCalbTime;

            /* The case to be able to support hw sync exactly (in variable fps in sync mode) */
            if (m_configurations->getDualHwSyncOn() == false) {
                if (m_needMaxCalbTime[newFrame->getCameraId()] == true &&
                        m_needMaxCalbTime[pairFrame->getCameraId()] == true) {
                    /* need maximum calbTime (both are low fps) */
                    CLOGV("dynamic calbTime change [min:%d -> max:%d]",
                            m_syncTimestampMinCalbTime, m_syncTimestampMaxCalbTime);
                    syncCalbTime = m_syncTimestampMaxCalbTime;
                }
            }

            if (diffTimeMs <= syncCalbTime)
                iamFound = true;
#if 0
            CLOGD("compare diff(%lldms) calbTime(%lldms) new [Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld]"
                    "vs pair [Fcount:%d, MetaFrameCount:%d, Type:%d, Time:%lld]",
                    diffTimeMs,
                    syncCalbTime,
                    newFrame->getFrameCount(),
                    newFrame->getMetaFrameCount(),
                    newFrame->getFrameType(),
                    myTime,
                    pairFrame->getFrameCount(),
                    pairFrame->getMetaFrameCount(),
                    pairFrame->getFrameType(),
                    pairTime);
#endif
            break;
        default:
            CLOG_ASSERT("invalid syncType(%d)", syncType);
            break;
        }

        if (iamFound) {
            /* remove */
            rawPairFrameQ->erase(r);
            break;
        }
    }
    pairFrameQ->rawUnLockProcessList(); /* unlock */

    if (iamFound) {
        bool saveNotSyncedMyFrame = false;
        bool saveNotSyncedPairFrame = false;

        /* can save master frame only cause of service buffer */
        if (m_lastFrameType == FRAME_TYPE_PREVIEW) {
            if (iamMasterFrame)
                saveNotSyncedMyFrame = true;
            else
                saveNotSyncedPairFrame = true;
        }

        /* clear old frames */
        m_cleanFrameBySyncType(myFrameQ, newFrame, syncType, saveNotSyncedMyFrame);
        m_cleanFrameBySyncType(pairFrameQ, pairFrame, syncType, saveNotSyncedPairFrame);

        if (iamMasterFrame) {
            m_makeOneMasterFrame(newFrame, pairFrame);

            /* return useless slave frame */
            m_pushFrameToOutputQ(pairFrame);

            finalFrame = newFrame;
        } else {
            m_makeOneMasterFrame(pairFrame, newFrame);

            /* return useless slave frame */
            m_pushFrameToOutputQ(newFrame);

            finalFrame = pairFrame;
        }
    } else {
        myFrameQ->pushProcessQ(&newFrame);
    }

    return NO_ERROR;
}

void ExynosCameraPipeSync::m_makeOneMasterFrame(ExynosCameraFrameSP_sptr_t masterFrame, ExynosCameraFrameSP_sptr_t slaveFrame)
{
    status_t ret;
    ExynosCameraBuffer buffer;

    ret = slaveFrame->getSrcBuffer(getPipeId(), &buffer);
    if (ret != NO_ERROR) {
        CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)",
                getPipeId(), ret);
    }

    /* TODO: Check timestamp and skip frame */
    if (buffer.index < 0) {
        CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                buffer.index, slaveFrame->getFrameCount(), getPipeId());

        masterFrame->setFrameState(FRAME_STATE_SKIPPED);
        slaveFrame->setFrameState(FRAME_STATE_SKIPPED);
    } else {
        ret = masterFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED);
        if (ret != NO_ERROR) {
            CLOGE("[F%d B%d]Failed to setDstBufferState. ret %d",
                    masterFrame->getFrameCount(), buffer.index, ret);
        } else {
            struct camera2_shot_ext *slave_shot_ext = NULL;

            ret = masterFrame->setDstBuffer(getPipeId(), buffer);
            if (ret != NO_ERROR) {
                CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)",
                        getPipeId(), ret);
            }

            /* set Zoom Ratio */
            masterFrame->setZoomRatio(slaveFrame->getZoomRatio(), OUTPUT_NODE_2);

            /* set Active Zoom Ratio */
            masterFrame->setActiveZoomRatio(slaveFrame->getActiveZoomRatio(), OUTPUT_NODE_2);

            /* set Active Zoom Rect */
            ExynosRect viewZoomRect = {0, };
            slaveFrame->getZoomRect(&viewZoomRect);
            masterFrame->setZoomRect(&viewZoomRect, OUTPUT_NODE_2);

            /* set Active Zoom Rect */
            ExynosRect slaveActiveZoomRect = {0, };
            slaveFrame->getActiveZoomRect(&slaveActiveZoomRect);
            masterFrame->setActiveZoomRect(slaveActiveZoomRect, OUTPUT_NODE_2);

            /* set ActiveZoomMargin */
            masterFrame->setActiveZoomMargin(slaveFrame->getActiveZoomMargin(), OUTPUT_NODE_2);

            /* shot_ext */
            slave_shot_ext = (struct camera2_shot_ext *)(buffer.addr[buffer.getMetaPlaneIndex()]);
            masterFrame->setMetaData(slave_shot_ext, OUTPUT_NODE_2);
        }
    }

    if (m_reprocessing == true && slaveFrame->getRequest(PIPE_HWFC_JPEG_DST_REPROCESSING)) {
        ret = slaveFrame->getSrcBuffer(getPipeId(), &buffer, OUTPUT_NODE_2);
        if (ret != NO_ERROR) {
            CLOGE("getDstBuffer fail, pipeId(%d), ret(%d)", getPipeId(), ret);
        }

        if (buffer.index < 0) {
            CLOGE("Invalid buffer index(%d), framecount(%d), pipeId(%d)",
                    buffer.index, slaveFrame->getFrameCount(), getPipeId());
        } else {
            ret = masterFrame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED, CAPTURE_NODE_2);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to setDstBufferState. ret %d",
                        masterFrame->getFrameCount(), buffer.index, ret);
            } else {
                ret = masterFrame->setDstBuffer(getPipeId(), buffer, CAPTURE_NODE_2);
                if (ret != NO_ERROR) {
                    CLOGE("setDstBuffer fail, pipeId(%d), ret(%d)", getPipeId(), ret);
                }
            }
        }
    }
}
}; /* namespace android */
