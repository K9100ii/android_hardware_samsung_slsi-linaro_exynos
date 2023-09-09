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

/*!
 * \file      ExynosCameraDualFrameSelector.h
 * \brief     header file for ExynosCameraDualFrameSelector
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2014/10/08
 *
 * <b>Revision History: </b>
 * - 2014/10/08 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 * - 2016/07/20 : Teahyung, Kim(tkon.kim@samsung.com) \n
 *   Update version2.
 */

#ifndef EXYNOS_CAMERA_DUAL_FRAME_SELECTOR_H
#define EXYNOS_CAMERA_DUAL_FRAME_SELECTOR_H

#include "string.h"
#include <utils/Log.h>
#include <utils/threads.h>

#include "ExynosCameraPipe.h"
#include "ExynosCameraFrame.h"
#include "ExynosCameraFrameSelector.h"

#define MESSAGE_MAX 32

using namespace android;

/*
 * Class ExynosCameraDualFrameSelector
 * ExynosCameraDualFrameSelector is sync logic to, get sync frame,
 * among asynchronously coming frames from multiple camera.
 */
class ExynosCameraDualFrameSelector
{
protected:
    /*
     * Class SyncObj
     * SyncObj is the object to have frame
     */
    class SyncObj {
    public:
        /* syncObj's selecting state */
        typedef enum SELECT_STATE {
            SELECT_STATE_BASE,
            SELECT_STATE_NO_SELECTED,
            SELECT_STATE_SELECTED,
            SELECT_STATE_WILL_BE_SELECTED,
            SELECT_STATE_MAX,
        } select_state_t;

        /* syncObj's type */
        typedef enum TYPE {
            TYPE_BASE,
            TYPE_NORMAL,
            TYPE_DUMMY,
            TYPE_MAX,
        } type_t;

        SyncObj();
        ~SyncObj();

        /*
         * create the initiate syncObj
         * @cameraId
         * @frame
         * @pipeId : caller's pipeId
         * @isSrc : src or dst buffer?
         * @nodeIndex : to distinguish src buffer
         */
        status_t create(int cameraId,
                        ExynosCameraFrameSP_sptr_t frame,
                        int pipeId,
                        bool isSrc,
                        int nodeIndex,
                        const char *name);

        /* destroy obj (this leads frame's buffer release) */
        status_t destroy(void);

        /* get cameraId of frame */
        int getCameraId(void);

        ExynosCameraFrameSP_sptr_t getFrame(void);

        /* get pipeId of frame */
        int getPipeId(void);

        /* src or dst buffer ? */
        bool getIsSrc(void);

        /* get nodeIndex */
        int getNodeIndex(void);

        int getTimeStamp(void);

        status_t getBufferFromFrame(ExynosCameraBuffer *buffer);

        /*
         * return the state of selecting this SyncObj.
         * In case of thease cases, sync frames can be pop
         *  - new sync frame be pushed and over than holdCount
         *  - if selectSingleFrame called.
         *    But both camera should select the same sync frame.
         *    (In this point, dual selector check this "selec_state")
         *    For example
         *    "N" : no selected
         *    "S" : selected
         *    "W" : will be selected
         *
         *   | Master   |  Slave   |          SyncObjList
         * -----------------------------------------------------------
         * T1| F1 push  |          | [M]-F1("N")         [S]-
         * -----------------------------------------------------------
         * T2|          | F1 push  | [M]-F1("N")         [S]-F1("N")
         * -----------------------------------------------------------
         * T3| F1 select|          | [M]-F1("S")         [S]-F1("W")
         *   |          |          |  "don't remove F1"
         * -----------------------------------------------------------
         * T4| F2 push  |          |  same
         * -----------------------------------------------------------
         * T5|          | F2 push  | [M]-F1("S")-F2("N") [S]-F1("W")-F2("N")
         * -----------------------------------------------------------
         * T6|          | F1 select|   [M]-F1("S")-F2("N") [S]-F1("S")-F2("N")
         *   |          |          | =>[M]-F2("N")         [S]-F2("N") "removed F1"
         * -----------------------------------------------------------
         * T7|          | F2 select|   [M]-F1("W")         [S]-F1("S")
         * ------------------------------------------------------------
         *
         *  - if selectFrame called
         *    user forcely flush the sync frame
         */
        select_state_t getSelectState(void);

        /* set the flag to be selected in this SyncObj */
        void setSelectState(select_state_t selectState);

        /* get syncId for syncObj */
        int getSyncId(void);

        /* set syncId for syncObj */
        void setSyncId(int syncId);

        /* make dummy SyncObj */
        void makeDummy(int cameraId, int timeStamp);

        type_t getType(void);
    public:
        SyncObj& operator =(const SyncObj &other) {
            m_cameraId   = other.m_cameraId;
            m_syncId     = other.m_syncId;
            m_frame      = other.m_frame;
            m_pipeId     = other.m_pipeId;
            m_isSrc      = other.m_isSrc;
            m_nodeIndex  = other.m_nodeIndex;
            m_timeStamp  = other.m_timeStamp;
            m_selectState = other.m_selectState;
            m_type       = other.m_type;

            return *this;
        }

        bool operator ==(const SyncObj &other) const {
            bool ret = true;

            if (m_cameraId   != other.m_cameraId ||
                m_syncId     != other.m_syncId   ||
                m_frame      != other.m_frame    ||
                m_pipeId     != other.m_pipeId   ||
                m_isSrc      != other.m_isSrc    ||
                m_nodeIndex  != other.m_nodeIndex ||
                m_timeStamp  != other.m_timeStamp ||
                m_selectState   != other.m_selectState ||
                m_type       != other.m_type) {
                ret = false;
            }

            return ret;
        }

        bool operator !=(const SyncObj &other) const {
            return !(*this == other);
        }

    private:
        /* to find the pair of opposite syncObj */
        int m_syncId;

        int m_cameraId;
        ExynosCameraFrameSP_sptr_t m_frame;
        int m_pipeId;
        bool m_isSrc;
        int m_nodeIndex;
        int m_timeStamp;
        select_state_t m_selectState;
        type_t m_type;
        char m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    }; /* class SyncObj */

public:
    /*
     * Class Message
     * It is the object to have the frame information pushed in dual selector
     * It will be used to notify to other class
     * when multiple camera push the frame to dual selector.
     */
    class Message {
    public:
        Message();
        ~Message();

        /* getter */
        int getCameraId(void);
        int getTimeStamp(void);
        int getZoom(void);
        sync_type_t getSyncType(void);
        uint32_t getFrameType(void);

        /* setter */
        void setCameraId(int cameraId);
        void setTimeStamp(int timeStamp);
        void setZoom(int zoom);
        void setSyncType(sync_type_t syncType);
        void setFrameType(uint32_t frameType);

    private:
        int m_cameraId;
        int m_timeStamp;
        int m_zoom;
        sync_type_t m_syncType;
        uint32_t m_frameType;
    }; /* class Message */

public:
    ExynosCameraDualFrameSelector();
    virtual ~ExynosCameraDualFrameSelector();

    /* recommanded only master camera calls this init function */
    status_t init(void);

    /* deinit the dual frame selector */
    status_t deinit(int cameraId);

    /*
     * setting information of cameraId's stream
     * @cameraId
     * @parm
     * @bufMgr : bufferManager to return buffer.
     */
    status_t setInfo(int cameraId,
                     ExynosCameraParameters *param,
                     ExynosCameraBufferManager *bufMgr,
                     ExynosCameraFrameSelector *selector = NULL);

    /*
     *  It always trigger sync logic with a frame.
     *  when this function call, this class try to compare to find sync frame.
     *  If logic find synced frame, push frame to "syncObjList" which is from argument.
     *  This list is type of FIFO not stack.
     *  and, you can get sync frame by selectFrame()
     * @cameraId
     * @frame
     * @pipeId : caller's pipeId
     * @isSrc : to check srcBuffer or dstBuffer
     * @nodeIndex : it need index to get buffer
     */
    status_t  manageNormalFrameHoldList(int cameraId,
                                        ExynosCameraFrameSP_sptr_t frame,
                                        int pipeId, bool isSrc, int32_t nodeIndex);

    /*
     * This api must return "one frame", although there are synced
     * master, slave frames. From "syncObjList", get one synced frames
     * which have two camera's each buffers and another information.
     *
     * One synced frame to be returned is based on "cameraId" param.
     * Opposite cameraId's frame would be deleted.
     *
     * It return the frame in base of thease scheme
     *
     *  frame    | camera[syncType][frameType ] + camera[syncType][frameType  ]
     * ---------------------------------------------------------------------------------
     *  case    1) Master[Bypass  ][Normal    ] + Slave [-       ][-          ] => select master
     *  case    2) Master[-       ][-         ] + Slave [Switch  ][Normal     ] => select slave
     *  case    3) Master[Sync    ][-         ] + Slave [Sync    ][-          ] => select master + slave
     *  case    4) Master[Bypass  ][Transition] + Slave [-       ][Inter/Trans] => select master
     *  case    5) Master[-       ][Internal  ] + Slave [Switch  ][Transition ] => select slave
     *  other    )                                                         => drop
     *
     *  but if selected frame have "Internal" frame type, drop the both frame
     */
    ExynosCameraFrameSP_sptr_t selectFrame(int cameraId);

    /*
     * From "syncObjList", get one synced frame(master or slave).
     *
     * @cameraId
     */
    ExynosCameraFrameSP_sptr_t selectSingleFrame(int cameraId);

    /* release the buffer by using cameraId's buffer manager */
    status_t releaseBuffer(int cameraId, ExynosCameraBuffer *buffer);

    /* get the source buffer manager by cameraId */
    status_t getBufferManager(int cameraId, ExynosCameraBufferManager **bufMgr);

    /* get the ExynosCameraParameters by cameraId */
    status_t getParameter(int cameraId, ExynosCameraParameters **param);

    /*
     * recommanded only master camera calls this setFrameHoldCount function
     * @count : count for synced frames to hold on.
     *          if hold Count is 1, it maintain only last 1 synced frame.
     * @prepareCount : prepare count for prepareSynced frame (to be synced Frames) to hold on.
     *   case1) if prepare hold Count is N (N > 0), it maintain only last N preparesynced frame.
     *   case2) if prepare hold Count is 0, it maintain automately preparesynced frame by bufferMgr's allocatedCount.
     *          ex. Total Buffer Count            (A) : 8
     *              Actual SyncObjList Size       (B) : 2
     *              Diff Count                    (C) : A - B = 6
     *              prepareSyncObjList hold count (D) : C / 2 = 3 (if C is 1, D is 1)
     *
     *           => Only 3 frame was remained in prepareSyncObjList
     *           => At least, 3 buffer can be run in Pre-MCPipe.
     */
    status_t setFrameHoldCount(int cameraId, int32_t count, int32_t prepareCount = 0);

    int getSizeOfSyncList(int cameraId);

    /*
     * register the notifyQ to send message about frame pushed.
     * @cameraId
     * @notifyQ : caller get this notifyQ pointer.
     */
    status_t registerNotifyQ(int cameraId, ExynosCameraList<Message> **notifyQ);

    /*
     * register the removeFrameQ to push frame
     * @cameraId
     * @removeFrameQ : caller get this removeFrameQ pointer.
     */
    status_t registerRemoveFrameQ(int cameraId, ExynosCameraList<ExynosCameraFrameSP_sptr_t> **removeFrameQ);

    /*
     * flush sync list.
     * @cameraId
    */
    status_t flushSyncList(int cameraId);

    /*
     * flush all frames(both sync and noSync).
     * @cameraId
    */
    status_t flush(int cameraId);

    /*
     * print All Dual Selector's Managed List.
     * @cameraId
    */
    void dump(int cameraId);

    /* register camera ids to use */
    void setFlagValidCameraId(int cameraId, int otherCameraId);

    /* change the selector mode */
    void setPreventDropFlag(int cameraId, bool dropFlag);

protected:
    SyncObj     m_selectSingleSyncObj(int cameraId);
    bool        m_isSimilarTimeStamp(int cameraId, SyncObj* self, SyncObj *other);
    int         m_findOppositeCameraId(int cameraId);
    status_t    m_destroySyncObj(SyncObj *syncObj, bool flagReleaseBuffer, bool notifyRemove);
    status_t    m_pushList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj);
    status_t    m_popList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj, int syncId);
    status_t    m_popList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj *syncObj);
    status_t    m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj, SyncObj::select_state_t selectState);
    status_t    m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj, int syncId);
    status_t    m_peekList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, SyncObj **syncObj);
    bool        m_checkSyncObjOnList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *otherPrepareList, SyncObj *syncObj, SyncObj *resultSyncObj);
    status_t    m_clearListAll(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list);
    status_t    m_clearListUntilMinSize(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, int minSize, bool flagReleaseBuffer = true);
    status_t    m_clearListUntilTimeStamp(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list, int timeStamp, bool flagReleaseBuffer = true);
    void        m_printList(int cameraId, List<ExynosCameraDualFrameSelector::SyncObj> *list);
    int         m_getTimeStamp(ExynosCameraFrameSP_sptr_t frame);

protected:
    int             m_holdCount;
    int             m_prepareHoldCount;
    bool            m_flagValidCameraId[CAMERA_ID_MAX];
    bool            m_flagSetInfo[CAMERA_ID_MAX];
    int             m_validCameraCnt;

    /*
     * m_syncId : unique id for syncObj
     * In syncObjList, both SyncObjs synced have same syncId.
     * This id will be used to trace all syncObjs.
     * And it will be used by hashIndex indexing m_msg.
     * ex.
     *   SyncList {
     *     [CAM0] = {
     *          [TIMESTAMP=10, SyncId=1],
     *          [TIMESTAMP=40, SyncId=2],
     *     [CAM2] = {
     *          [TIMESTAMP=11, SyncId=1],
     *          [TIMESTAMP=41, SyncId=2],
     *   }
     */
    int             m_syncId;
    int             m_syncCalibTime; /* ms */

    Mutex           m_lock;
    List<SyncObj>   m_prepareSyncObjList[CAMERA_ID_MAX]; /* for syncObjList */
    List<SyncObj>   m_syncObjList[CAMERA_ID_MAX];

    ExynosCameraBufferManager   *m_bufMgr[CAMERA_ID_MAX];
    ExynosCameraParameters      *m_param[CAMERA_ID_MAX];
    ExynosCameraList<Message>   m_notifyQ[CAMERA_ID_MAX];
    bool                        m_flagNotifyRegister[CAMERA_ID_MAX];
    sync_type_t                 m_lastSyncType;
    int                         m_lastTimeStamp;

    ExynosCameraList<ExynosCameraFrameSP_sptr_t>   m_removeFrameQ[CAMERA_ID_MAX];
    bool                        m_flagRemoveFrameRegister[CAMERA_ID_MAX];

    Message                     m_msg[MESSAGE_MAX];
    int                         m_msgIndex;

    char                        m_name[EXYNOS_CAMERA_NAME_STR_SIZE];

    /* only used in bayer dual selector */
    ExynosCameraFrameSelector   *m_selector[CAMERA_ID_MAX];

    /* forceFlag to flush all prepareSyncObj if there's not matched syncObj */
    bool                        m_flagFlushForNotMatched;

    /* not to drop any frame */
    bool                        m_preventDropFlag;

    /* for debug */
    bool                        m_removeSyncObjTrace;
    bool                        m_pushSyncObjTrace;
};

/* for Preview Switching Singleton in Sync Pipe */
class ExynosCameraDualPreviewSwitchingFrameSelector : public ExynosCameraDualFrameSelector
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraDualPreviewSwitchingFrameSelector>;

    ExynosCameraDualPreviewSwitchingFrameSelector();
    virtual ~ExynosCameraDualPreviewSwitchingFrameSelector();
};

/* for Preview Singleton in Sync Pipe */
class ExynosCameraDualPreviewFrameSelector : public ExynosCameraDualFrameSelector
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>;

    ExynosCameraDualPreviewFrameSelector();
    virtual ~ExynosCameraDualPreviewFrameSelector();
};

/* for Reprocessing Singleton in Sync Pipe */
class ExynosCameraDualCaptureFrameSelector : public ExynosCameraDualFrameSelector
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraDualCaptureFrameSelector>;

    ExynosCameraDualCaptureFrameSelector();
    virtual ~ExynosCameraDualCaptureFrameSelector();
};

/* for Reprocessing Singleton in captureSelector */
class ExynosCameraDualBayerFrameSelector : public ExynosCameraDualFrameSelector
{
protected:
    friend class ExynosCameraSingleton<ExynosCameraDualBayerFrameSelector>;

    ExynosCameraDualBayerFrameSelector();
    virtual ~ExynosCameraDualBayerFrameSelector();
};
#endif //EXYNOS_CAMERA_DUAL_FRAME_SELECTOR_H
