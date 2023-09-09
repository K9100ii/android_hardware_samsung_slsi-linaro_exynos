/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#define LOG_TAG "ExynosCameraDualFrameSelector"

#include "ExynosCameraDualFrameSelector.h"

/* #define DUAL_DEBUG */
#define DUAL_SELECTOR_CALIB_TIME (2) // 2msec

/*
 * ID : SyncId, S.State : Select State
 * H.F : HAL FrameCount, D.F : Driver FrameCount, T : Timestamp, D.T : Diff Time
 */
#define ONE_SYNC_OBJ_ID "[CAM%d/HF%d/MF%d/T:%d/ID:%d/S.State:%d]"
#define TWO_SYNC_OBJ_ID "[D.T:%dms][CAM%d/H.F%d/M.F%d/T:%d/ID:%d/S.State:%d vs CAM%d/H.F%d/M.F%d/T:%d/ID:%d/S.State:%d]"

#define ONE_SYNC_OBJ_ARGS(syncObj) \
    (syncObj)->getCameraId(),   \
    ((syncObj)->getFrame() != NULL) ? (syncObj)->getFrame()->getFrameCount() : -1, \
    ((syncObj)->getFrame() != NULL) ? (syncObj)->getFrame()->getMetaFrameCount() : -1, \
    (syncObj)->getTimeStamp(), \
    (syncObj)->getSyncId(), \
    (syncObj)->getSelectState()

#define SYNC_OBJ_DIFF_TIME_ARGS(syncObj, otherObj) \
        (abs((syncObj)->getTimeStamp() - (otherObj)->getTimeStamp()))

#define TWO_SYNC_OBJ_ARGS(syncObj, otherObj) \
        SYNC_OBJ_DIFF_TIME_ARGS(syncObj, otherObj), \
        ONE_SYNC_OBJ_ARGS(syncObj), ONE_SYNC_OBJ_ARGS(otherObj)

/* define for dual selecot's log */
#ifdef DUAL_DEBUG
#define DUAL_LOGV(cameraId, fmt, ...) \
        ALOGD(Paste2(CAM_ID"VERB" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)
#else
#define DUAL_LOGV(cameraId, fmt, ...) \
        ALOGV(Paste2(CAM_ID"VERB" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)
#endif

#define DUAL_LOGD(cameraId, fmt, ...) \
        ALOGD(Paste2(CAM_ID"DEBUG" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define DUAL_LOGW(cameraId, fmt, ...) \
        ALOGW(Paste2(CAM_ID"WARN" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define DUAL_LOGE(cameraId, fmt, ...) \
        ALOGE(Paste2(CAM_ID"ERR" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define DUAL_LOGI(cameraId, fmt, ...) \
        ALOGI(Paste2(CAM_ID"INFO" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define DUAL_ASSERT(cameraId, fmt, ...)  \
        android_printAssert(NULL, LOG_TAG, Paste2(CAM_ID, fmt), cameraId, m_name, ##__VA_ARGS__)

/* for one syncObj */
#define ONE_SYNC_OBJ_LOGV(cameraId, syncObj, fmt, ...)  \
        DUAL_LOGV(cameraId, ONE_SYNC_OBJ_ID fmt, ONE_SYNC_OBJ_ARGS(syncObj), ##__VA_ARGS__)

#define ONE_SYNC_OBJ_LOGD(cameraId, syncObj, fmt, ...)  \
        DUAL_LOGD(cameraId, ONE_SYNC_OBJ_ID fmt, ONE_SYNC_OBJ_ARGS(syncObj), ##__VA_ARGS__)

#define ONE_SYNC_OBJ_LOGW(cameraId, syncObj, fmt, ...)  \
        DUAL_LOGW(cameraId, ONE_SYNC_OBJ_ID fmt, ONE_SYNC_OBJ_ARGS(syncObj), ##__VA_ARGS__)

#define ONE_SYNC_OBJ_LOGE(cameraId, syncObj, fmt, ...)  \
        DUAL_LOGE(cameraId, ONE_SYNC_OBJ_ID fmt, ONE_SYNC_OBJ_ARGS(syncObj), ##__VA_ARGS__)

/* for two syncObjs */
#define TWO_SYNC_OBJ_LOGV(cameraId, syncObj, otherObj, fmt, ...)  \
        DUAL_LOGV(cameraId, TWO_SYNC_OBJ_ID fmt, TWO_SYNC_OBJ_ARGS(syncObj, otherObj), ##__VA_ARGS__)

#define TWO_SYNC_OBJ_LOGD(cameraId, syncObj, otherObj, fmt, ...)  \
        DUAL_LOGD(cameraId, TWO_SYNC_OBJ_ID fmt, TWO_SYNC_OBJ_ARGS(syncObj, otherObj), ##__VA_ARGS__)

#define TWO_SYNC_OBJ_LOGW(cameraId, syncObj, otherObj, fmt, ...)  \
        DUAL_LOGW(cameraId, TWO_SYNC_OBJ_ID fmt, TWO_SYNC_OBJ_ARGS(syncObj, otherObj), ##__VA_ARGS__)

#define TWO_SYNC_OBJ_LOGE(cameraId, syncObj, otherObj, fmt, ...)  \
        DUAL_LOGE(cameraId, TWO_SYNC_OBJ_ID fmt, TWO_SYNC_OBJ_ARGS(syncObj, otherObj), ##__VA_ARGS__)


/*
 * Class SyncObj
 */
ExynosCameraDualFrameSelector::SyncObj::SyncObj()
{
    m_cameraId = -1;
    m_frame = NULL;
    m_pipeId = -1;
    m_isSrc = false;
    m_nodeIndex = -1;
    m_timeStamp = 0;
    m_selectState = SELECT_STATE_BASE;
    memset(m_name, 0x00, sizeof(m_name));
    m_syncId = -1;
}

ExynosCameraDualFrameSelector::SyncObj::~SyncObj()
{}

status_t ExynosCameraDualFrameSelector::SyncObj::create(int cameraId,
                        ExynosCameraFrameSP_sptr_t frame,
                        int pipeId,
                        bool isSrc,
                        int nodeIndex,
                        const char *name)
{
    status_t ret = NO_ERROR;

    m_cameraId = cameraId;
    m_frame = frame;
    m_pipeId = pipeId;
    m_isSrc = isSrc;
    m_nodeIndex = nodeIndex;
    m_timeStamp = (int)(ns2ms(m_frame->getTimeStamp()));
    m_selectState = SELECT_STATE_NO_SELECTED;

#ifdef DUAL_DEBUG
    strncpy(m_name, name,  EXYNOS_CAMERA_NAME_STR_SIZE - 1);
#endif

    ONE_SYNC_OBJ_LOGV(m_cameraId, this, " pipe:%d, src:%d, nodeIndex:%d",
            m_pipeId, m_isSrc, m_nodeIndex);

    return ret;
}

status_t ExynosCameraDualFrameSelector::SyncObj::destroy(void)
{
    m_frame = NULL;

    ONE_SYNC_OBJ_LOGV(m_cameraId, this, "");

    return NO_ERROR;
}

int ExynosCameraDualFrameSelector::SyncObj::getCameraId(void)
{
    return m_cameraId;
}

ExynosCameraFrameSP_sptr_t ExynosCameraDualFrameSelector::SyncObj::getFrame(void)
{
    return m_frame;
}

int ExynosCameraDualFrameSelector::SyncObj::getPipeId(void)
{
    return m_pipeId;
}

bool ExynosCameraDualFrameSelector::SyncObj::getIsSrc(void)
{
    return m_isSrc;
}

int ExynosCameraDualFrameSelector::SyncObj::getNodeIndex(void)
{
    return m_nodeIndex;
}

int ExynosCameraDualFrameSelector::SyncObj::getTimeStamp(void)
{
    return m_timeStamp;
}

int ExynosCameraDualFrameSelector::SyncObj::getSyncId(void)
{
    return m_syncId;
}

void ExynosCameraDualFrameSelector::SyncObj::setSyncId(int syncId)
{
    m_syncId = syncId;
}

status_t ExynosCameraDualFrameSelector::SyncObj::getBufferFromFrame(ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer selectedBuffer;

    if (m_frame == NULL) {
        ONE_SYNC_OBJ_LOGE(m_cameraId, this, "m_frame == NULL");
        return BAD_VALUE;
    }

    if (buffer == NULL) {
        ONE_SYNC_OBJ_LOGE(m_cameraId, this, "outBuffer == NULL");
        return BAD_VALUE;
    }

    if (m_isSrc) {
        ret = m_frame->getSrcBuffer(m_pipeId, &selectedBuffer);
        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(m_cameraId, this, "getSrcBuffer(pipeId %d) fail", m_pipeId);
    } else {
        if (m_nodeIndex < 0) {
            ret = m_frame->getDstBuffer(m_pipeId, &selectedBuffer);
            if (ret != NO_ERROR)
                ONE_SYNC_OBJ_LOGE(m_cameraId, this, "getDstBuffer(pipeId %d) fail", m_pipeId);
        } else {
            ret = m_frame->getDstBuffer(m_pipeId, &selectedBuffer, m_nodeIndex);
            if (ret != NO_ERROR)
                ONE_SYNC_OBJ_LOGE(m_cameraId, this, "getDstBuffer(pipeId %d, dstPos %d) fail", m_pipeId, m_nodeIndex);
        }
    }

    *buffer = selectedBuffer;

    return ret;
}

bool ExynosCameraDualFrameSelector::SyncObj::isSimilarTimeStamp(SyncObj *other)
{
    bool ret = false;

    int diffTime = abs(other->getTimeStamp() - this->getTimeStamp());

    /*
     * This is close other than calib time.
     * The diffTime(myself and other's timestamp) have not to be
     * over than calib time.
     *  ex. If calib time is 16ms,
     *   (sysnObj's time - 16msec) <= (other's time) <= (sysnObj's time + 16msec)
     *   => consider sync
     */
    if ((other->getTimeStamp() != 0) &&
        (this->getTimeStamp() != 0) &&
        (diffTime <= DUAL_SELECTOR_CALIB_TIME))
        ret = true;

    TWO_SYNC_OBJ_LOGV(m_cameraId, this, other, " %s", ret ? "matched" : "not matched");

    return ret;
}

ExynosCameraDualFrameSelector::SyncObj::select_state_t ExynosCameraDualFrameSelector::SyncObj::getSelectState(void)
{
    return m_selectState;
}

void ExynosCameraDualFrameSelector::SyncObj::setSelectState(ExynosCameraDualFrameSelector::SyncObj::select_state_t selectState)
{
    m_selectState = selectState;
}

/*
 * Class Message
 */
ExynosCameraDualFrameSelector::Message::Message()
{
    m_cameraId = -1;
    m_timeStamp = -1;
    m_zoom = -1;
    m_syncType = SYNC_TYPE_BASE;
    m_frameType = FRAME_TYPE_INTERNAL;
}

ExynosCameraDualFrameSelector::Message::~Message()
{}

int ExynosCameraDualFrameSelector::Message::getCameraId(void)
{
    return m_cameraId;
}

int ExynosCameraDualFrameSelector::Message::getTimeStamp(void)
{
    return m_timeStamp;
}

int ExynosCameraDualFrameSelector::Message::getZoom(void)
{
    return m_zoom;
}

sync_type_t ExynosCameraDualFrameSelector::Message::getSyncType(void)
{
    return m_syncType;
}

uint32_t ExynosCameraDualFrameSelector::Message::getFrameType(void)
{
    return m_frameType;
}

void ExynosCameraDualFrameSelector::Message::setCameraId(int cameraId)
{
    m_cameraId = cameraId;
}

void ExynosCameraDualFrameSelector::Message::setZoom(int zoom)
{
    m_zoom = zoom;
}

void ExynosCameraDualFrameSelector::Message::setTimeStamp(int timeStamp)
{
    m_timeStamp = timeStamp;
}

void ExynosCameraDualFrameSelector::Message::setSyncType(sync_type_t syncType)
{
    m_syncType = syncType;
}

void ExynosCameraDualFrameSelector::Message::setFrameType(uint32_t frameType)
{
    m_frameType = frameType;
}

/*
 * Class ExynosCameraDualSelector
 */
ExynosCameraDualFrameSelector::ExynosCameraDualFrameSelector()
{
    init();
}

ExynosCameraDualFrameSelector::~ExynosCameraDualFrameSelector()
{}

status_t ExynosCameraDualFrameSelector::init(void)
{
    status_t ret = NO_ERROR;

    DUAL_LOGD(-1, "");

    Mutex::Autolock lock(m_lock);

    m_syncId = 0;
    m_holdCount = 1;
    m_prepareHoldCount = 0;
    m_msgIndex = 0;
    m_validCameraCnt = 0;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_flagValidCameraId[i] = false;
        m_bufMgr[i] = NULL;
        m_param[i] = NULL;
        m_flagNotifyRegister[i] = false;
        m_flagRemoveFrameRegister[i] = false;
    }

    return ret;
}

status_t ExynosCameraDualFrameSelector::deinit(int cameraId)
{
    int cameraCnt = 0;
    status_t ret = NO_ERROR;

    DUAL_LOGD(cameraId, "");

    Mutex::Autolock lock(m_lock);

    if (m_flagValidCameraId[cameraId] == true &&
            m_validCameraCnt > 0) {
        m_validCameraCnt--;
        m_flagValidCameraId[cameraId] = false;
        m_bufMgr[cameraId] = NULL;
        m_param[cameraId] = NULL;
        m_flagNotifyRegister[cameraId] = false;
        m_flagRemoveFrameRegister[cameraId] = false;
        m_notifyQ[cameraId].release();
        m_removeFrameQ[cameraId].release();

        if (m_validCameraCnt == 0) {
            m_syncId = 0;
            m_holdCount = 1;
            m_prepareHoldCount = 0;
            m_msgIndex = 0;
        }
    } else {
        DUAL_LOGI(cameraId, "already deinited(%d, %d)",
                m_flagValidCameraId[cameraId], m_validCameraCnt);
    }

    return ret;
}

status_t ExynosCameraDualFrameSelector::setInfo(int cameraId,
                     ExynosCameraParameters *param,
                     ExynosCameraBufferManager *bufMgr)
{
    status_t ret = NO_ERROR;
    int validCameraCnt = 0;

    Mutex::Autolock lock(m_lock);

    DUAL_LOGD(cameraId, "param:%p, bufMgr:%p, validCamera(%d)", param, bufMgr, m_validCameraCnt);

    if (CAMERA_ID_MAX <= cameraId) {
        DUAL_LOGE(cameraId, "invalid cameraId");
        return INVALID_OPERATION;
    }

    m_bufMgr[cameraId] = bufMgr;
    m_param[cameraId] = param;

    m_validCameraCnt++;
    m_flagValidCameraId[cameraId] = true;

    /* when over than 2, assert */
    if (m_validCameraCnt > 2)
        DUAL_ASSERT(cameraId, "two more than cameras was set(%d)", validCameraCnt);

    return ret;
}

status_t ExynosCameraDualFrameSelector::manageNormalFrameHoldList(int cameraId,
                                        ExynosCameraFrameSP_sptr_t frame,
                                        int pipeId, bool isSrc, int32_t nodeIndex)
{
#ifdef DUAL_DEBUG
    //ExynosCameraAutoTimer autoTimer(__func__);
#endif
    status_t ret = NO_ERROR;
    bool flagAllCameraSync = false;
    int prepareHoldCount = 0;

    if (CAMERA_ID_MAX <= cameraId) {
        DUAL_LOGE(cameraId, "invalid cameraId");
        return INVALID_OPERATION;
    }

    if (frame == NULL) {
        DUAL_LOGE(cameraId, "frame == NULL");
        return INVALID_OPERATION;
    }

    /* create SyncObj */
    SyncObj syncObj;
    SyncObj otherSyncObj;
    ret = syncObj.create(cameraId, frame, pipeId, isSrc, nodeIndex, m_name);
    if (ret != NO_ERROR) {
        DUAL_LOGE(cameraId, "[%dms] syncObj.create fail", m_getTimeStamp(frame));
        return INVALID_OPERATION;
    }

    DUAL_LOGV(cameraId, "list size[%d, %d]",
            m_prepareSyncObjList[cameraId].size(),
            m_syncObjList[cameraId].size());

    /* when smaller than 2, single stream is can be assume not synced frame. */
    int otherCameraId = m_findOppositeCameraId(cameraId);

    /* backup state for reprocessing */
    frame->setReprocessingFrameType((frame_type_t)frame->getFrameType());
    frame->setReprocessingZoom(frame->getZoom());
    frame->setReprocessingSyncType(frame->getSyncType());

    /* to SyncObjList */
    Mutex::Autolock lock(m_lock);

    /* case of not opening other camera */
    if (otherCameraId < 0) {
        DUAL_LOGW(cameraId, "There's no opposite camera");

        /* push the syncObj to prepareSyncObjList */
        m_pushList(cameraId, &m_prepareSyncObjList[cameraId], &syncObj);
    } else {
        /* check the timestamp */
        if (m_checkSyncObjOnList(cameraId, &m_prepareSyncObjList[otherCameraId], &syncObj, &otherSyncObj))
            flagAllCameraSync = true;

        if (flagAllCameraSync) {
            /* set the syncID for synced frames */
            syncObj.setSyncId(m_syncId);
            otherSyncObj.setSyncId(m_syncId++);

            /* push the syncObj to syncObjList */
            m_pushList(cameraId, &m_syncObjList[cameraId], &syncObj);
            m_pushList(cameraId, &m_syncObjList[otherCameraId], &otherSyncObj);

            /* pop the syncObj until holdCount */
            m_clearListUntilMinSize(cameraId, &m_syncObjList[cameraId], m_holdCount);
            m_clearListUntilMinSize(cameraId, &m_syncObjList[otherCameraId], m_holdCount);

            /* flush the prepareSyncList until new SyncObj's timestamp */
            m_clearListUntilTimeStamp(cameraId, &m_prepareSyncObjList[cameraId], syncObj.getTimeStamp());
            m_clearListUntilTimeStamp(cameraId, &m_prepareSyncObjList[otherCameraId], otherSyncObj.getTimeStamp());

            /* notify */
            Message *msg;
            msg = &m_msg[m_msgIndex++ % MESSAGE_MAX];
            msg->setCameraId(syncObj.getCameraId());
            msg->setTimeStamp(syncObj.getTimeStamp());
            msg->setZoom(frame->getZoom());
            msg->setSyncType((syncObj.getFrame())->getSyncType());
            msg->setFrameType((syncObj.getFrame())->getFrameType());
            for (int i = 0; i < CAMERA_ID_MAX; i++) {
                if (!m_flagNotifyRegister[i])
                    continue;

                m_notifyQ[i].pushProcessQ(msg);
            }
        } else {
            /* push the syncObj to prepareSyncObjList */
            m_pushList(cameraId, &m_prepareSyncObjList[cameraId], &syncObj);
        }
    }

    if (m_prepareHoldCount > 0) {
        /* manual prepareHoldCount setting */
        prepareHoldCount = m_prepareHoldCount;
    } else {
        /*
         *  Automatic prepareHoldCount setting
         *
         *  ex. Total Buffer Count            (A) : 8
         *      Actual SyncObjList Size       (B) : 2
         *      Diff Count                    (C) : A - B = 6
         *      prepareSyncObjList hold count (D) : C / 2 = 3 (if C is 1, D is 1)
         *
         *   => Only 3 frame was remained in prepareSyncObjList
         *   => At least, 3 buffer can be run in Pre-MCPipe.
         */
        int totalBufferCount = m_bufMgr[cameraId]->getNumOfAllowedMaxBuffer();
        int diffCount = totalBufferCount - m_syncObjList[cameraId].size();
        prepareHoldCount = diffCount > 1 ? (diffCount / 2) : 0;
    }

    /* forcely maintain the prepareSyncList's size to half of holdCount */
    m_clearListUntilMinSize(cameraId, &m_prepareSyncObjList[cameraId], prepareHoldCount);

    ONE_SYNC_OBJ_LOGV(cameraId, &syncObj, "pushed frame(prepare:%d)", prepareHoldCount);

    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCameraDualFrameSelector::selectFrame(int cameraId)
{
#ifdef DUAL_DEBUG
    //ExynosCameraAutoTimer autoTimer(__func__);
#endif

    status_t ret = NO_ERROR;

    int otherCameraId = m_findOppositeCameraId(cameraId);
    if (otherCameraId < 0) {
        DUAL_LOGE(cameraId, "There's no opposite camera");
        return NULL;
    }

    if (getSizeOfSyncList(cameraId) <= 0)
        return NULL;

    bool flagSelectMaster = false;
    bool flagSelectSlave = false;
    int output_node_index = OUTPUT_NODE_1;

    /* for master */
    bool isSrc;
    int pipeId;
    int nodeIndex;
    SyncObj syncObj;
    ExynosCameraFrameSP_sptr_t selectedFrame = NULL;
    sync_type_t syncType;
    uint32_t frameType;

    /* for slave */
    bool otherIsSrc;
    int otherPipeId;
    int otherNodeIndex;
    SyncObj otherSyncObj;
    ExynosCameraBuffer otherBuffer;
    ExynosCameraFrameSP_sptr_t selectedOtherFrame = NULL;
    sync_type_t otherSyncType;
    uint32_t otherFrameType;

    /* we must protect list of various camera. */
    Mutex::Autolock lock(m_lock);

    /* check the integrity of SyncObjList */
    if (m_syncObjList[cameraId].size() != m_syncObjList[otherCameraId].size()) {
            DUAL_ASSERT(cameraId, "invalid size matching : syncObj size:%d/%d",
                    m_syncObjList[cameraId].size(),
                    m_syncObjList[otherCameraId].size());
    }

    /* pop from sync obj */
    if (m_popList(cameraId, &m_syncObjList[cameraId], &syncObj) != NO_ERROR) {
        DUAL_ASSERT(cameraId, "Nothing in popList : syncObj size:%d/%d",
                    m_syncObjList[cameraId].size(),
                    m_syncObjList[otherCameraId].size());
    }

    /* pop from opposite camera's sync obj */
    if (m_popList(cameraId, &m_syncObjList[otherCameraId], &otherSyncObj) != NO_ERROR) {
        DUAL_ASSERT(cameraId, "Nothing in otherPopList : syncObj size:%d/%d",
                    m_syncObjList[cameraId].size(),
                    m_syncObjList[otherCameraId].size());
    }

    /* for master */
    pipeId = syncObj.getPipeId();
    nodeIndex = syncObj.getNodeIndex();
    isSrc = syncObj.getIsSrc();
    selectedFrame = syncObj.getFrame();
    syncType = selectedFrame->getSyncType();
    frameType = selectedFrame->getFrameType();

    /* for slave */
    otherPipeId = otherSyncObj.getPipeId();
    otherNodeIndex = otherSyncObj.getNodeIndex();
    otherIsSrc = otherSyncObj.getIsSrc();
    selectedOtherFrame = otherSyncObj.getFrame();
    otherSyncType = selectedOtherFrame->getSyncType();
    otherFrameType = selectedOtherFrame->getFrameType();

    /*
     *  frame    | camera[syncType][frameType ] + camera[syncType][frameType  ]
     * ---------------------------------------------------------------------------------
     *  case    1) Master[Bypass  ][Normal    ] + Slave [-       ][-          ] => select master
     *  case    2) Master[-       ][-         ] + Slave [Switch  ][Normal     ] => select slave
     *  case    3) Master[Sync    ][-         ] + Slave [Sync    ][-          ] => select master + slave
     *  case    4) Master[Bypass  ][Transition] + Slave [-       ][-          ] => select master
     *  case    5) Master[-       ][-         ] + Slave [Switch  ][Transition ] => select slave
     *  other    )                                                         => drop
     *
     *  but if selected frame have "Internal" frame type, drop the both frame
     */

    if (syncType == SYNC_TYPE_BYPASS &&
            (frameType != FRAME_TYPE_INTERNAL && frameType != FRAME_TYPE_TRANSITION)) {
        /* case 1 */
        flagSelectMaster = true;
        flagSelectSlave = false;
    } else if (otherSyncType == SYNC_TYPE_SWITCH &&
            (otherFrameType != FRAME_TYPE_INTERNAL && otherFrameType != FRAME_TYPE_TRANSITION)) {
        /* case 2 */
        flagSelectMaster = false;
        flagSelectSlave = true;
    } else if (syncType == SYNC_TYPE_SYNC && otherSyncType == SYNC_TYPE_SYNC &&
            (frameType != FRAME_TYPE_INTERNAL && otherFrameType != FRAME_TYPE_INTERNAL)) {
        /* case 3 */
        flagSelectMaster = true;
        flagSelectSlave = true;

	/* change the output node_index */
	output_node_index = OUTPUT_NODE_2;
    } else if (syncType == SYNC_TYPE_BYPASS &&
            frameType == FRAME_TYPE_TRANSITION) {
        /* case 4 */
        flagSelectMaster = true;
        flagSelectSlave = false;
    } else if (otherSyncType == SYNC_TYPE_SWITCH &&
            otherFrameType == FRAME_TYPE_TRANSITION) {
        /* case 5 */
        flagSelectMaster = false;
        flagSelectSlave = true;
    } else {
        /* drop */
        flagSelectMaster = false;
        flagSelectSlave = false;

        TWO_SYNC_OBJ_LOGE(cameraId, &syncObj, &otherSyncObj, "this frame will be dropped (M:%d/%d)/(S:%d/%d)",
                syncType, frameType,
                otherSyncType, otherFrameType);
        selectedFrame->setFrameState(FRAME_STATE_SKIPPED);
    }

    TWO_SYNC_OBJ_LOGV(cameraId, &syncObj, &otherSyncObj, "selected(%d, %d) M(%d,%d)/S(%d,%d)",
            flagSelectMaster, flagSelectSlave,
            syncType, frameType,
            otherSyncType, otherFrameType);

    /* release the buffer before overwritng by slave buffer */
    if (flagSelectMaster == false && flagSelectSlave == true) {
        ret = m_destroySyncObj(&syncObj, true, false);
        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(cameraId, &syncObj, "removed fail");
    }

    /* select the frame */
    if ((flagSelectMaster == true && flagSelectSlave == true) ||
           (flagSelectMaster == false && flagSelectSlave == true)) {
        /*
         * 1) master + slave (set the slave's info to master)
         * 2) slave (overwrite the frame info)
         */
        /* get the slave's Buffer and setting */
        ret = otherSyncObj.getBufferFromFrame(&otherBuffer);
        if (ret != NO_ERROR) {
            ONE_SYNC_OBJ_LOGE(cameraId, &otherSyncObj, "get%sBuffer fail(%d, %d)",
                    otherIsSrc ? "Src" : "Dst", otherPipeId, otherNodeIndex);
        } else {
            ret = selectedFrame->setSrcBufferState(pipeId, ENTITY_BUFFER_STATE_REQUESTED, output_node_index);
            if (ret < 0) {
                DUAL_LOGE(cameraId, "setSrcBufferState(ENTITY_BUFFER_STATE_REQUESTED, %d) fail, pipeId(%d), ret(%d) frame(%d)",
                        output_node_index,
                        pipeId, ret, selectedFrame->getFrameCount());
            }

            ret = selectedFrame->setSrcBuffer(pipeId, otherBuffer, output_node_index);
            if (ret < 0) {
                DUAL_LOGE(cameraId, "setSrcBuffer(%d) fail, pipeId(%d), index(%d) frame(%d) ret(%d)",
                        output_node_index,
                        pipeId, otherBuffer.index, selectedFrame->getFrameCount(), ret);
            }
        }

        /* Node Group Setting */
        struct camera2_node_group node_group;
        for (int i = 0; i < PERFRAME_NODE_GROUP_MAX; i++) {
            selectedOtherFrame->getNodeGroupInfo(&node_group, i);
            selectedFrame->storeNodeGroupInfo(&node_group, i, output_node_index);
        }

        /* Meta Setting */
        struct camera2_shot_ext shot;
        selectedOtherFrame->getMetaData(&shot);
        selectedFrame->setMetaData(&shot, output_node_index);

        /* Zoom Setting */
        selectedFrame->setZoom(selectedOtherFrame->getZoom(), output_node_index);

        /* SyncType Setting */
        selectedFrame->setSyncType(selectedOtherFrame->getSyncType());

        /* FrameType Setting */
        selectedFrame->setFrameType((frame_type_t)selectedOtherFrame->getFrameType());

        /* FrameState Setting */
        switch (selectedFrame->getFrameState()) {
        case FRAME_STATE_SKIPPED:
        case FRAME_STATE_INVALID:
            selectedFrame->setFrameState(FRAME_STATE_READY);
            break;
        default:
            break;
        }
    } else {
        /* 3) master (don't do anything) */
    }

    /* release the unnecessary buffer */
    if (flagSelectMaster == false && flagSelectSlave == false) {
        ret = m_destroySyncObj(&syncObj, true, false);
        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(cameraId, &syncObj, "removed fail");
    }

    if (flagSelectSlave == false) {
        ret = m_destroySyncObj(&otherSyncObj, true, false);
        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(cameraId, &otherSyncObj, "removed fail");
    }

    return selectedFrame;
}

ExynosCameraFrameSP_sptr_t ExynosCameraDualFrameSelector::selectSingleFrame(int cameraId)
{
#ifdef DUAL_DEBUG
    //ExynosCameraAutoTimer autoTimer(__func__);
#endif

    status_t ret = NO_ERROR;
    bool popAllSyncObj = false;
    SyncObj tempSyncObj;
    SyncObj tempOtherSyncObj;
    SyncObj *syncObj = NULL;
    SyncObj *otherSyncObj = NULL;

    int otherCameraId = m_findOppositeCameraId(cameraId);
    if (otherCameraId < 0) {
        DUAL_LOGE(cameraId, "There's no opposite camera");
        return NULL;
    }

    if (getSizeOfSyncList(cameraId) <= 0) {
        DUAL_LOGE(cameraId, "There's no synced frame");
        return NULL;
    }

    /* to SyncObjList */
    Mutex::Autolock lock(m_lock);

    /* check the integrity of SyncObjList */
    if (m_syncObjList[cameraId].size() != m_syncObjList[otherCameraId].size()) {
        DUAL_ASSERT(cameraId, "invalid size matching : syncObj size:%d/%d, prepareSyncObj size:%d/%d)",
                m_syncObjList[cameraId].size(),
                m_syncObjList[otherCameraId].size(),
                m_prepareSyncObjList[cameraId].size(),
                m_prepareSyncObjList[otherCameraId].size());
    }

    /* peek syncObj from sync obj */
    if (m_peekList(cameraId, &m_syncObjList[cameraId], &syncObj, SyncObj::SELECT_STATE_WILL_BE_SELECTED) != NO_ERROR) {
        if (m_peekList(cameraId, &m_syncObjList[cameraId], &syncObj, SyncObj::SELECT_STATE_NO_SELECTED) != NO_ERROR) {
            DUAL_LOGW(cameraId, "There's no synced frame");
            return NULL;
        }
    }

    /* peek other camera's syncObj from sync obj */
    if (m_peekList(cameraId, &m_syncObjList[otherCameraId], &otherSyncObj, syncObj->getSyncId()) != NO_ERROR) {
        DUAL_ASSERT(cameraId, "Nothing in peekList : syncObj size:%d/%d, noSyncObj size:%d/%d)",
                m_syncObjList[cameraId].size(),
                m_syncObjList[otherCameraId].size(),
                m_prepareSyncObjList[cameraId].size(),
                m_prepareSyncObjList[otherCameraId].size());
    }

    /* if both camera selected synced frame, we actually pop the syncObj */
    if (otherSyncObj->getSelectState() == SyncObj::SELECT_STATE_SELECTED) {
        /* pop from sync obj */
        if (m_popList(cameraId, &m_syncObjList[cameraId], &tempSyncObj, syncObj->getSyncId()) != NO_ERROR) {
            DUAL_ASSERT(cameraId, "Nothing in popList : syncObj size:%d/%d, prepareSyncObj size:%d/%d)",
                    m_syncObjList[cameraId].size(),
                    m_syncObjList[otherCameraId].size(),
                    m_prepareSyncObjList[cameraId].size(),
                    m_prepareSyncObjList[otherCameraId].size());
        }

        /* pop from opposite camera's sync obj */
        if (m_popList(cameraId, &m_syncObjList[otherCameraId], &tempOtherSyncObj, otherSyncObj->getSyncId()) != NO_ERROR) {
            DUAL_ASSERT(cameraId, "Nothing in otherPopList : syncObj size:%d/%d, prepareSyncObj size:%d/%d)",
                    m_syncObjList[cameraId].size(),
                    m_syncObjList[otherCameraId].size(),
                    m_prepareSyncObjList[cameraId].size(),
                    m_prepareSyncObjList[otherCameraId].size());
        }
    } else {
        /* change the select_state for both camera */
        syncObj->setSelectState(SyncObj::SELECT_STATE_SELECTED);
        otherSyncObj->setSelectState(SyncObj::SELECT_STATE_WILL_BE_SELECTED);

        TWO_SYNC_OBJ_LOGV(cameraId, syncObj, otherSyncObj, "changed");
    }

    TWO_SYNC_OBJ_LOGV(cameraId, syncObj, otherSyncObj, "selected");

    return syncObj->getFrame();
}

status_t ExynosCameraDualFrameSelector::releaseBuffer(int cameraId, ExynosCameraBuffer *buffer)
{
    status_t ret = NO_ERROR;

    if ((buffer != NULL) && (buffer->index >= 0)) {
        DUAL_LOGV(cameraId, "index : %d", buffer->index);

        if (m_bufMgr[cameraId] == NULL) {
            DUAL_LOGE(cameraId, "buffer manager is null");
            ret = INVALID_OPERATION;
            return ret;
        } else {
            ret = m_bufMgr[cameraId]->putBuffer(buffer->index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret < 0) {
                DUAL_LOGE(cameraId, "putBuffer failed (Index : %d)", buffer->index);
                m_bufMgr[cameraId]->printBufferState();
                m_bufMgr[cameraId]->printBufferQState();
            }
        }
    }

    return ret;
}

status_t ExynosCameraDualFrameSelector::getBufferManager(int cameraId, ExynosCameraBufferManager **bufMgr)
{
    status_t ret = NO_ERROR;

    if (m_bufMgr[cameraId] == NULL) {
        DUAL_LOGE(cameraId, "buffer manager is null");
        ret = INVALID_OPERATION;
    } else {
        *bufMgr = m_bufMgr[cameraId];
    }

    return ret;
}

status_t ExynosCameraDualFrameSelector::getParameter(int cameraId, ExynosCameraParameters **param)
{
    status_t ret = NO_ERROR;

    if (m_param[cameraId] == NULL) {
        DUAL_LOGE(cameraId, "parameters is null");
        ret = INVALID_OPERATION;
    } else {
        *param = m_param[cameraId];
    }

    return ret;
}

status_t ExynosCameraDualFrameSelector::setFrameHoldCount(int cameraId, int count, int32_t prepareCount)
{
    Mutex::Autolock lock(m_lock);

    m_holdCount = count;
    m_prepareHoldCount = prepareCount;

    DUAL_LOGD(cameraId, "count:%d, prepareHoldCount:%d", m_holdCount, m_prepareHoldCount);

    return NO_ERROR;
}

int ExynosCameraDualFrameSelector::getSizeOfSyncList(int cameraId)
{
    Mutex::Autolock lock(m_lock);

    return m_syncObjList[cameraId].size();
}

status_t ExynosCameraDualFrameSelector::registerNotifyQ(int cameraId, ExynosCameraList<Message> **notifyQ)
{
    Mutex::Autolock lock(m_lock);

    if (notifyQ == NULL) {
        DUAL_LOGE(cameraId, "notifyQ == NULL");
        return INVALID_OPERATION;
    }

    DUAL_LOGD(cameraId, "");

    if (m_notifyQ[cameraId].getSizeOfProcessQ() > 0) {
        DUAL_ASSERT(cameraId, "Old Notifications(%d) was already notifyQ",
                m_notifyQ[cameraId].getSizeOfProcessQ());
    }

    *notifyQ = &m_notifyQ[cameraId];
    m_flagNotifyRegister[cameraId] = true;

    return NO_ERROR;
}

status_t ExynosCameraDualFrameSelector::registerRemoveFrameQ(int cameraId, ExynosCameraList<ExynosCameraFrameSP_sptr_t> **removeFrameQ)
{
    Mutex::Autolock lock(m_lock);

    if (removeFrameQ == NULL) {
        DUAL_LOGE(cameraId, "removeFrameQ == NULL");
        return INVALID_OPERATION;
    }

    DUAL_LOGD(cameraId, "");

    if (m_removeFrameQ[cameraId].getSizeOfProcessQ() > 0) {
        DUAL_ASSERT(cameraId, "Old Frames(%d) was already removeFrameQ",
                m_removeFrameQ[cameraId].getSizeOfProcessQ());
    }

    *removeFrameQ = &m_removeFrameQ[cameraId];
    m_flagRemoveFrameRegister[cameraId] = true;

    return NO_ERROR;
}

status_t ExynosCameraDualFrameSelector::flushSyncList(int cameraId)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock lock(m_lock);

    m_notifyQ[cameraId].release();
    m_removeFrameQ[cameraId].release();

    m_clearListAll(cameraId, &m_prepareSyncObjList[cameraId]);
    m_clearListAll(cameraId, &m_syncObjList[cameraId]);

    return ret;
}

status_t ExynosCameraDualFrameSelector::flush(int cameraId)
{
    status_t ret = NO_ERROR;

    DUAL_LOGD(cameraId, "");

    Mutex::Autolock lock(m_lock);

    /* flush notify, removeQ */
    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if ((!m_flagValidCameraId[i]))
            continue;

        m_notifyQ[i].release();
        m_removeFrameQ[i].release();
    }

    /* flush syncFrameQ */
    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if ((!m_flagValidCameraId[i]))
            continue;

        m_clearListAll(cameraId, &m_prepareSyncObjList[i]);
        m_clearListAll(cameraId, &m_syncObjList[i]);
    }

    return ret;
}

void ExynosCameraDualFrameSelector::dump(int cameraId)
{

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if ((!m_flagValidCameraId[i]))
                continue;

        DUAL_LOGD(i, "=== DUMP ===");
        {
            /* Sync List Dump */
            Mutex::Autolock lock(m_lock);

            DUAL_LOGD(i, "=== PrepareObjList ===");
            m_printList(i, &m_prepareSyncObjList[i]);

            DUAL_LOGD(i, "=== SyncObjList ===");
            m_printList(i, &m_syncObjList[i]);
        }
    }
}

/*
 * protected
 */
int ExynosCameraDualFrameSelector::m_findOppositeCameraId(int cameraId)
{
    int otherCameraId = -1;

    /* find opposite cameraId */
    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if ((!m_flagValidCameraId[i]) || (cameraId == i))
            continue;

        otherCameraId = i;
        break;
    }

    return otherCameraId;
}

status_t ExynosCameraDualFrameSelector::m_destroySyncObj(SyncObj *syncObj, bool flagReleaseBuffer, bool notifyRemove)
{
    status_t ret = NO_ERROR;
    ExynosCameraBuffer buffer;
    int cameraId = syncObj->getCameraId();

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj is NULL(%d)", flagReleaseBuffer);
        return NO_ERROR;
    }

    if (syncObj->getFrame() == NULL) {
        ONE_SYNC_OBJ_LOGE(cameraId, syncObj, "syncObj has no frame");
        return NO_ERROR;
    }

    ExynosCameraFrameSP_sptr_t frame = syncObj->getFrame();
    int timeStamp = syncObj->getTimeStamp();
    int pipeId = syncObj->getPipeId();
    int nodeIndex = syncObj->getNodeIndex();
    bool isSrc = syncObj->getIsSrc();

    ONE_SYNC_OBJ_LOGV(cameraId, syncObj, "removed(%d)", flagReleaseBuffer);

    /* 1. push the frame to registered camera's list to remove from processList */
    if (notifyRemove == true && m_flagRemoveFrameRegister[cameraId] == true) {
        ONE_SYNC_OBJ_LOGD(cameraId, syncObj, "pushProcessQ");
        m_removeFrameQ[cameraId].pushProcessQ(&frame);
    }

    /* 2. put the buffer to bufMgr */
    if (frame->getFrameType() != FRAME_TYPE_INTERNAL &&
        flagReleaseBuffer == true) {
        if (isSrc == true)
            ret = frame->getSrcBuffer(pipeId, &buffer, nodeIndex);
        else
            ret = frame->getDstBuffer(pipeId, &buffer, nodeIndex);

        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(cameraId, syncObj, "get%sBuffer fail(%d, %d)",
                    isSrc ? "Src" : "Dst", pipeId, nodeIndex);
        else
            releaseBuffer(cameraId, &buffer);
    }

    /* 3. destroy the syncObj */
    syncObj->destroy();

    return ret;
}

status_t ExynosCameraDualFrameSelector::m_pushList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj)
{
    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    ONE_SYNC_OBJ_LOGV(cameraId, syncObj, "pushed");

    list->push_back(*syncObj);

    return NO_ERROR;
}

status_t ExynosCameraDualFrameSelector::m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj, SyncObj::select_state_t selectState)
{
    bool found = false;
    List<SyncObj>::iterator r;

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    if (list->empty())
        return NOT_ENOUGH_DATA;

    r = list->begin()++;

    do {
        if (r->getSelectState() == selectState) {
            *syncObj = &(*r);
            found = true;
            break;
        }

        r++;
    } while (r != list->end());

    if (found == false) {
        if (selectState == SyncObj::SELECT_STATE_NO_SELECTED)
            DUAL_LOGE(cameraId, "there's no selectState(%d)", selectState);
        return NOT_ENOUGH_DATA;
    } else {
        ONE_SYNC_OBJ_LOGV(cameraId, *syncObj, "peeked");
    }

    return NO_ERROR;
};

status_t ExynosCameraDualFrameSelector::m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj, int syncId)
{
    bool found = false;
    List<SyncObj>::iterator r;

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    if (list->empty())
        return NOT_ENOUGH_DATA;

    r = list->begin()++;

    do {
        if (r->getSyncId() == syncId) {
            *syncObj = &(*r);
            found = true;
            break;
        }

        r++;
    } while (r != list->end());

    if (found == false) {
        DUAL_LOGE(cameraId, "there's no syncId(%d)", syncId);
        return NOT_ENOUGH_DATA;
    } else {
        ONE_SYNC_OBJ_LOGV(cameraId, *syncObj, "peeked");
    }

    return NO_ERROR;
};

status_t ExynosCameraDualFrameSelector::m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj)
{
    List<SyncObj>::iterator r;

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    if (list->empty())
        return NOT_ENOUGH_DATA;

    r = list->begin()++;
    *syncObj = &(*r);

    ONE_SYNC_OBJ_LOGV(cameraId, *syncObj, "peeked");

    return NO_ERROR;
};

status_t ExynosCameraDualFrameSelector::m_popList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj, int syncId)
{
    bool found = false;
    List<SyncObj>::iterator r;

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    if (list->empty())
        return NOT_ENOUGH_DATA;

    r = list->begin()++;

    do {
        if (r->getSyncId() == syncId) {
            *syncObj = *r;
            list->erase(r);
            found = true;
            break;
        }

        r++;
    } while (r != list->end());

    if (found == false) {
        ONE_SYNC_OBJ_LOGE(cameraId, syncObj, "there's no syncId(%d)", syncId);
        return NOT_ENOUGH_DATA;
    }

    ONE_SYNC_OBJ_LOGV(cameraId, syncObj, "poped");

    return NO_ERROR;
};

status_t ExynosCameraDualFrameSelector::m_popList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj)
{
    List<SyncObj>::iterator r;

    if (syncObj == NULL) {
        DUAL_LOGE(cameraId, "syncObj == NULL");
        return INVALID_OPERATION;
    }

    if (list->empty())
        return NOT_ENOUGH_DATA;

    r = list->begin()++;
    *syncObj = *r;

    ONE_SYNC_OBJ_LOGV(cameraId, syncObj, "poped");

    list->erase(r);

    return NO_ERROR;
};

bool ExynosCameraDualFrameSelector::m_checkSyncObjOnList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *otherPrepareList, SyncObj *syncObj, SyncObj *resultSyncObj)
{
    status_t ret = NO_ERROR;
    List<SyncObj>::iterator r;
    int size = otherPrepareList->size();
    SyncObj otherSyncObj;

    if (otherPrepareList->empty()) {
        ONE_SYNC_OBJ_LOGV(cameraId, syncObj, "empty size");
        return false;
    }

    r = otherPrepareList->begin()++;

    do {
        otherSyncObj = *r;

        if (syncObj->isSimilarTimeStamp(&otherSyncObj)) {
            *resultSyncObj = otherSyncObj;
            return true;
        }

        r++;
    } while (r != otherPrepareList->end());

    return false;
};

status_t ExynosCameraDualFrameSelector::m_clearListAll(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list)
{
    status_t ret = NO_ERROR;
    List<SyncObj>::iterator r;
    int size = list->size();
    SyncObj curSyncObj;

    if (list->empty()) {
        DUAL_LOGV(cameraId, "List Size : %d", size);
        return NO_ERROR;
    }

    DUAL_LOGV(cameraId, "delete List Size : %d", size);

    r = list->begin()++;

    do {
        curSyncObj = *r;

        ONE_SYNC_OBJ_LOGV(cameraId, &curSyncObj, "removed(%d) flag(%d)", size, m_flagRemoveFrameRegister[cameraId]);

        list->erase(r);

        /* release SyncObj with source buffer */
        ret = m_destroySyncObj(&curSyncObj, true, true);
        if (ret != NO_ERROR)
            ONE_SYNC_OBJ_LOGE(cameraId, &curSyncObj, "removed(%d)", size);

        r++;
    } while (r != list->end());

    return NO_ERROR;
}

status_t ExynosCameraDualFrameSelector::m_clearListUntilMinSize(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, int minSize, bool flagReleaseBuffer)
{
    status_t ret = NO_ERROR;
    List<SyncObj>::iterator r;
    int size = list->size();
    SyncObj curSyncObj;

    if (list->empty() || size <= minSize) {
        DUAL_LOGV(cameraId, "List Size : %d/%d", size, minSize);
        return NO_ERROR;
    }

    DUAL_LOGV(cameraId, "delete List Size : %d/%d, flagReleaseBuffer(%d)", size, minSize, flagReleaseBuffer);

    r = list->begin()++;

    do {
        curSyncObj = *r;

        if (size <= minSize)
            break;

        /* remove only "no_selected" frame */
        if (curSyncObj.getSelectState() == SyncObj::SELECT_STATE_NO_SELECTED) {

            ONE_SYNC_OBJ_LOGV(cameraId, &curSyncObj, "removed(%d/%d) flag(%d)", size, minSize, m_flagRemoveFrameRegister[cameraId]);

            list->erase(r);

            /* release SyncObj with source buffer */
            ret = m_destroySyncObj(&curSyncObj, flagReleaseBuffer, true);
            if (ret != NO_ERROR)
                ONE_SYNC_OBJ_LOGE(cameraId, &curSyncObj, "removed(%d/%d)", size, minSize);

            size--;
        } else {
            ONE_SYNC_OBJ_LOGW(cameraId, &curSyncObj, "can't removed(%d/%d) flag(%d)", size, minSize, m_flagRemoveFrameRegister[cameraId]);
        }

        r++;
    } while (r != list->end());

    return NO_ERROR;
}

status_t ExynosCameraDualFrameSelector::m_clearListUntilTimeStamp(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, int timeStamp, bool flagReleaseBuffer)
{
    status_t ret = NO_ERROR;
    List<SyncObj>::iterator r;
    int size = list->size();
    SyncObj curSyncObj;
    bool notifyRemove = true;

    if (list->empty()) {
        DUAL_LOGV(cameraId, "List Size : %d", size);
        return NO_ERROR;
    }

    DUAL_LOGV(cameraId, "delete List Size : %d, timeStamp(%d), flagReleaseBuffer(%d)", size, timeStamp, flagReleaseBuffer);

    r = list->begin()++;

    do {
        curSyncObj = *r;

        if (curSyncObj.getTimeStamp() > timeStamp) {
            break;
        } else if (curSyncObj.getTimeStamp() == timeStamp) {
            /* reserve the frame */
            notifyRemove = false;
            flagReleaseBuffer = false;
        } else {
            /* remove the frame */
            notifyRemove = true;
        }

        /* remove only "no_selected" frame */
        if (curSyncObj.getSelectState() == SyncObj::SELECT_STATE_NO_SELECTED) {

            ONE_SYNC_OBJ_LOGV(cameraId, &curSyncObj, "removed(%d/%d) flag(%d)", curSyncObj.getTimeStamp(), timeStamp, m_flagRemoveFrameRegister[cameraId]);

            list->erase(r);

            /* release SyncObj with source buffer */
            ret = m_destroySyncObj(&curSyncObj, flagReleaseBuffer, notifyRemove);
            if (ret != NO_ERROR)
                ONE_SYNC_OBJ_LOGE(cameraId, &curSyncObj, "removed(%d)", timeStamp);
        } else {
            ONE_SYNC_OBJ_LOGW(cameraId, &curSyncObj, "can't removed(%d/%d) flag(%d)", curSyncObj.getTimeStamp(), timeStamp, m_flagRemoveFrameRegister[cameraId]);
        }

        r++;
    } while (r != list->end());

    return NO_ERROR;
}

void ExynosCameraDualFrameSelector::m_printList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list)
{
    List<SyncObj>::iterator r;

    SyncObj curSyncObj;

    if (m_flagValidCameraId[cameraId] == false) {
        DUAL_LOGE(cameraId, "invalid cameraId");
        return;
    }

    DUAL_LOGD(cameraId, " List Size : %d", list->size());
    if (list->empty())
        return;

    r = list->begin()++;

    do {
        curSyncObj = *r;
        ONE_SYNC_OBJ_LOGD(cameraId, &curSyncObj, "");
        r++;
    } while (r != list->end());
}

int ExynosCameraDualFrameSelector::m_getTimeStamp(ExynosCameraFrameSP_sptr_t frame)
{
    return (int)(ns2ms(frame->getTimeStamp()));
}

/* for Preview Singleton in Sync Pipe */
ExynosCameraDualPreviewFrameSelector::ExynosCameraDualPreviewFrameSelector()
{
    strncpy(m_name, "DualPreviewSelector", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
    init();
}

ExynosCameraDualPreviewFrameSelector::~ExynosCameraDualPreviewFrameSelector()
{
}

/* for Reprocessing Singleton in Sync Pipe */
ExynosCameraDualCaptureFrameSelector::ExynosCameraDualCaptureFrameSelector()
{
    strncpy(m_name, "DualCaptureSelector", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
    init();
}

ExynosCameraDualCaptureFrameSelector::~ExynosCameraDualCaptureFrameSelector()
{
}

/* for Reprocessing Singleton in captureSelector */
ExynosCameraDualBayerFrameSelector::ExynosCameraDualBayerFrameSelector()
{
    strncpy(m_name, "DualBayerSelector", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));
    init();
}

ExynosCameraDualBayerFrameSelector::~ExynosCameraDualBayerFrameSelector()
{
}
