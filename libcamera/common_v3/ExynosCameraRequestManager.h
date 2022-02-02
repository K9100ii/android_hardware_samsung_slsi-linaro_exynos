/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_REQUEST_MANAGER_H__
#define EXYNOS_CAMERA_REQUEST_MANAGER_H__
#define CALLBACK_FPS_CHECK

#include <cutils/log.h>
#include <utils/RefBase.h>
#include <hardware/camera3.h>
#include <camera/CameraMetadata.h>
#include <map>
#include <list>

#include "ExynosCameraDefine.h"
#include "ExynosCameraStreamManager.h"
#include "ExynosCamera3FrameFactory.h"
#include "ExynosCameraParameters.h"
#include "ExynosCamera3Parameters.h"
#include "ExynosCameraSensorInfo.h"
#include "ExynosCameraMetadataConverter.h"

namespace android {

using namespace std;

namespace EXYNOS_REQUEST_RESULT {
    enum TYPE {
        CALLBACK_INVALID     = -1,
        CALLBACK_NOTIFY_ONLY = 0x00,
        CALLBACK_BUFFER_ONLY = 0x01,
        CALLBACK_PARTIAL_3AA = 0x02,
        CALLBACK_ALL_RESULT  = 0x03,
        CALLBACK_MAX         = 0x04,
    };
};

namespace EXYNOS_LIST_OPER {
    enum MODE {
        SINGLE_BACK  = 0,
        SINGLE_FRONT = 1,
        SINGLE_ORDER = 2,
        MULTI_GET    = 3
    };
};

typedef list< camera3_stream_buffer_t* >                  StreamBufferList;

class ExynosCamera3;
class ExynosCameraRequest;
class ExynosCamera3FrameFactory;

typedef sp<ExynosCameraRequest> ExynosCameraRequestSP_sprt_t;
typedef sp<ExynosCameraRequest>& ExynosCameraRequestSP_dptr_t;

typedef ExynosCameraList<uint32_t>                        stream_callback_queue_t;
typedef ExynosCameraThread<ExynosCameraRequestManager>    callbackThread;

typedef status_t (ExynosCamera3::*factory_handler_t)(ExynosCameraRequestSP_sprt_t, ExynosCamera3FrameFactory*);
typedef bool (ExynosCamera3::*factory_donehandler_t)();

class ExynosCameraRequestResult : public virtual RefBase {
public:
    ExynosCameraRequestResult(){};
    ~ExynosCameraRequestResult(){};

    virtual uint32_t getFrameCount() = 0;
    virtual uint32_t getKey() = 0;

    virtual EXYNOS_REQUEST_RESULT::TYPE getType() = 0;
    virtual status_t setCaptureRequest(camera3_capture_result_t *captureResult) = 0;
    virtual status_t getCaptureRequest(camera3_capture_result_t *captureResult) = 0;

    virtual status_t setNofityRequest(camera3_notify_msg_t *notifyResult) = 0;
    virtual status_t getNofityRequest(camera3_notify_msg_t *notifyResult) = 0;
    virtual status_t pushStreamBuffer(camera3_stream_buffer_t *streamBuffer) = 0;
    virtual status_t popStreamBuffer(camera3_stream_buffer_t *streamBuffer) = 0;
    virtual int      getNumOfStreamBuffer() = 0;
    virtual status_t getStreamBuffer(camera3_stream_buffer_t *streamBuffer) = 0;

};

class ExynosCamera3RequestResult : public virtual ExynosCameraRequestResult {
public:
    ExynosCamera3RequestResult(uint32_t key, uint32_t frameCount, EXYNOS_REQUEST_RESULT::TYPE type, camera3_capture_result_t *captureResult = NULL, camera3_notify_msg_t *notityMsg = NULL);
    ~ExynosCamera3RequestResult();

    virtual uint32_t getFrameCount();
    virtual uint32_t getKey();

    virtual EXYNOS_REQUEST_RESULT::TYPE getType();
    virtual status_t setCaptureRequest(camera3_capture_result_t *captureResult);
    virtual status_t getCaptureRequest(camera3_capture_result_t *captureResult);

    virtual status_t setNofityRequest(camera3_notify_msg_t *notifyResult);
    virtual status_t getNofityRequest(camera3_notify_msg_t *notifyResult);

    virtual status_t pushStreamBuffer(camera3_stream_buffer_t *streamBuffer);
    virtual status_t popStreamBuffer(camera3_stream_buffer_t *streamBuffer);
    virtual int      getNumOfStreamBuffer();
    virtual status_t getStreamBuffer(camera3_stream_buffer_t *streamBuffer);

private:
    status_t         m_init();
    status_t         m_deinit();
    status_t         m_pushBuffer(camera3_stream_buffer_t *src, StreamBufferList *list, Mutex *lock);
    status_t         m_popBuffer(camera3_stream_buffer_t *dst, StreamBufferList *list, Mutex *lock);
    int              m_getNumOfBuffer(StreamBufferList *list, Mutex *lock);
    status_t         m_getBuffer(camera3_stream_buffer_t *dst, StreamBufferList *list, Mutex *lock);

private:
    uint32_t                     m_key;
    EXYNOS_REQUEST_RESULT::TYPE  m_type;
    uint32_t                     m_frameCount;
    camera3_capture_result_t     m_captureResult;
    camera3_notify_msg_t         m_notityMsg;

    StreamBufferList             m_streamBufferList;
    mutable Mutex                m_streamBufferListLock;

};

namespace EXYNOS_REQUEST {
    enum STATE {
        INVALID  = -1,
        SERVICE  =  0,
        RUNNING  =  1
    };
};

typedef sp<ExynosCameraRequestResult>                            ResultRequest;
typedef list< uint32_t >                                         ResultRequestkeys;
typedef list< uint32_t >::iterator                               ResultRequestkeysIterator;

typedef list< sp<ExynosCameraRequestResult> >                    ResultRequestList;
typedef map< uint32_t, sp<ExynosCameraRequestResult> >           ResultRequestMap;
typedef list< sp<ExynosCameraRequestResult> >::iterator          ResultRequestListIterator;
typedef map< uint32_t, sp<ExynosCameraRequestResult> >::iterator ResultRequestMapIterator;

typedef map< int32_t, ExynosCamera3FrameFactory* >               FrameFactoryMap;
typedef map< int32_t, ExynosCamera3FrameFactory* >::iterator     FrameFactoryMapIterator;
typedef list<ExynosCamera3FrameFactory*>                         FrameFactoryList;
typedef list<ExynosCamera3FrameFactory*>::iterator               FrameFactoryListIterator;

typedef map< int32_t, sp<callbackThread>>                        StreamThreadMap;
typedef map< int32_t, sp<callbackThread>>::iterator              StreamThreadMapIterator;

class ExynosCameraRequest : public virtual RefBase {
public:
    ExynosCameraRequest(){};
    virtual ~ExynosCameraRequest(){};

    virtual uint32_t                       getKey() = 0;
    virtual void                           setFrameCount(uint32_t frameCount) = 0;
    virtual uint32_t                       getFrameCount() = 0;
    virtual uint8_t                        getCaptureIntent() = 0;

    virtual camera3_capture_request_t*     getService() = 0;

    virtual uint32_t                       setServiceMeta(CameraMetadata meta) = 0;
    virtual CameraMetadata                 getServiceMeta() = 0;

    virtual status_t                       setServiceShot(struct camera2_shot_ext *shot) = 0;
    virtual status_t                       getServiceShot(struct camera2_shot_ext *shot) = 0;

    virtual status_t                       setResultMeta(CameraMetadata meta) = 0;
    virtual CameraMetadata                 getResultMeta() = 0;
    virtual status_t                       setResultShot(struct camera2_shot_ext *shot) = 0;
    virtual status_t                       getResultShot(struct camera2_shot_ext *shot) = 0;
    virtual status_t                       setRequestState(EXYNOS_REQUEST::STATE state) = 0;
    virtual EXYNOS_REQUEST::STATE          getRequestState() = 0;
    virtual status_t                       setPrevShot(struct camera2_shot_ext *shot) = 0;
    virtual status_t                       getPrevShot(struct camera2_shot_ext *shot) = 0;

    virtual uint32_t                       getNumOfInputBuffer() = 0;
    virtual camera3_stream_buffer_t*       getInputBuffer() = 0;

    virtual uint64_t                       getSensorTimestamp() = 0;
    virtual uint32_t                       getNumOfOutputBuffer() = 0;
    virtual const camera3_stream_buffer_t* getOutputBuffers() = 0;
    virtual status_t                       pushResult(ResultRequest result) = 0;
    virtual ResultRequest                  popResult(uint32_t resultKey) = 0;
    virtual ResultRequest                  getResult(uint32_t resultKey) = 0;
    virtual status_t                       getAllResultKeys(ResultRequestkeys *keys) = 0;
    virtual status_t                       getResultKeys(ResultRequestkeys *keys, EXYNOS_REQUEST_RESULT::TYPE type) = 0;

    virtual status_t                       pushFrameFactory(int StreamID, ExynosCamera3FrameFactory* factory) = 0;
    virtual ExynosCamera3FrameFactory*      popFrameFactory(int streamID) = 0;
    virtual ExynosCamera3FrameFactory*      getFrameFactory(int streamID) = 0;
    virtual bool                           isFrameFactory(int streamID) = 0;
    virtual status_t                       getFrameFactoryList(FrameFactoryList *list) = 0;

    virtual void                           increaseCompleteBufferCount(void) = 0;
    virtual void                           increasecallbackBufferCount(void) = 0;
    virtual void                           resetCompleteBufferCount(void) = 0;
    virtual void                           resetcallbackBufferCount(void) = 0;
    virtual int                            getCompleteBufferCount(void) = 0;
    virtual int                            getcallbackBufferCount(void) = 0;

    virtual void                           increaseDuplicateBufferCount() = 0;
    virtual void                           resetDuplicateBufferCount() = 0;
    virtual int                            getDuplicateBufferCount(void) = 0;

    virtual void                           setRequestId(int reqId) = 0;
    virtual int                            getRequestId() = 0;
    virtual status_t                       getAllRequestOutputStreams(List <int> **list) = 0;
    virtual status_t                       pushRequestOutputStreams(int requestStreamId) = 0;
    virtual status_t                       getAllRequestInputStreams(List <int> **list) = 0;
    virtual status_t                       pushRequestInputStreams(int requestStreamId) = 0;

    virtual status_t                       popResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestList *resultList) = 0;
    virtual status_t                       popAndEraseResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestList *resultList) = 0;
    virtual status_t                       popAndEraseResultsByTypeAndStreamId(EXYNOS_REQUEST_RESULT::TYPE reqType, int streamId, ResultRequestList *resultList) = 0;
    virtual status_t                       setCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType, bool flag) = 0;
    virtual bool                           getCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType) = 0;
    virtual void                           printCallbackDoneState() = 0;
    virtual bool                           isComplete() = 0;

    virtual int                            getStreamId(int bufferIndex) = 0;
    virtual bool                           isExistStream(int streamId) = 0;

    virtual void                           setNeedInternalFrame(bool isNeedInternalFrame) = 0;
    virtual bool                           getNeedInternalFrame(void) = 0;
    virtual void                           increasePipelineDepth(void) = 0;
    virtual void                           updatePipelineDepth(void) = 0;
    virtual CameraMetadata                 get3AAResultMeta(void) = 0;
};

class ExynosCamera3Request : public virtual ExynosCameraRequest {
public:
    ExynosCamera3Request(camera3_capture_request_t* request, CameraMetadata previousMeta);
    ~ExynosCamera3Request();

    virtual uint32_t                       getKey();
    virtual void                           setFrameCount(uint32_t frameCount);
    virtual uint32_t                       getFrameCount();
    virtual uint8_t                        getCaptureIntent();

    virtual camera3_capture_request_t*     getService();

    virtual uint32_t                       setServiceMeta(CameraMetadata meta);
    virtual CameraMetadata                 getServiceMeta();

    virtual status_t                       setServiceShot(struct camera2_shot_ext *shot);
    virtual status_t                       getServiceShot(struct camera2_shot_ext *shot);

    virtual status_t                       setResultMeta(CameraMetadata meta);
    virtual CameraMetadata                 getResultMeta();
    virtual status_t                       setResultShot(struct camera2_shot_ext *shot);
    virtual status_t                       getResultShot(struct camera2_shot_ext *shot);
    virtual status_t                       setRequestState(EXYNOS_REQUEST::STATE state);
    virtual EXYNOS_REQUEST::STATE          getRequestState();
    virtual status_t                       setPrevShot(struct camera2_shot_ext *shot);
    virtual status_t                       getPrevShot(struct camera2_shot_ext *shot);

    virtual uint64_t                       getSensorTimestamp();
    virtual uint32_t                       getNumOfInputBuffer();
    virtual camera3_stream_buffer_t*       getInputBuffer();

    virtual uint32_t                       getNumOfOutputBuffer();
    virtual const camera3_stream_buffer_t* getOutputBuffers();
    virtual status_t                       pushResult(ResultRequest result);
    virtual ResultRequest                  popResult(uint32_t resultKey);
    virtual ResultRequest                  getResult(uint32_t resultKey);
    virtual status_t                       getAllResultKeys(ResultRequestkeys *keys);
    virtual status_t                       getResultKeys(ResultRequestkeys *keys, EXYNOS_REQUEST_RESULT::TYPE type);

    virtual status_t                       pushFrameFactory(int StreamID, ExynosCamera3FrameFactory* factory);
    virtual ExynosCamera3FrameFactory*     popFrameFactory(int streamID);
    virtual ExynosCamera3FrameFactory*     getFrameFactory(int streamID);
    virtual bool                           isFrameFactory(int streamID);
    virtual status_t                       getFrameFactoryList(FrameFactoryList *list);

    virtual void                           increaseCompleteBufferCount(void);
    virtual void                           increasecallbackBufferCount(void);
    virtual void                           resetCompleteBufferCount(void);
    virtual void                           resetcallbackBufferCount(void);
    virtual int                            getCompleteBufferCount(void);
    virtual int                            getcallbackBufferCount(void);

    virtual void                           increaseDuplicateBufferCount(void);
    virtual void                           resetDuplicateBufferCount(void);
    virtual int                            getDuplicateBufferCount(void);

    virtual void                           setRequestId(int reqId);
    virtual int                            getRequestId();
    virtual status_t                       getAllRequestOutputStreams(List<int> **list);
    virtual status_t                       pushRequestOutputStreams(int requestStreamId);
    virtual status_t                       getAllRequestInputStreams(List <int> **list);
    virtual status_t                       pushRequestInputStreams(int requestStreamId);

    virtual status_t                       popResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestList *resultList);
    virtual status_t                       popAndEraseResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestList *resultList);
    virtual status_t                       popAndEraseResultsByTypeAndStreamId(EXYNOS_REQUEST_RESULT::TYPE reqType, int streamId, ResultRequestList *resultList);
    virtual status_t                       setCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType, bool flag);
    virtual bool                           getCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType);
    virtual void                           printCallbackDoneState();
    virtual bool                           isComplete();

    virtual int                            getStreamId(int bufferIndex);
    virtual bool                           isExistStream(int streamId);

    virtual void                           setNeedInternalFrame(bool isNeedInternalFrame);
    virtual bool                           getNeedInternalFrame(void);
    virtual void                           increasePipelineDepth(void);
    virtual void                           updatePipelineDepth(void);
    virtual CameraMetadata                 get3AAResultMeta(void);

private:
    virtual status_t                       m_init();
    virtual status_t                       m_deinit();
    virtual status_t                       m_pushResult(ResultRequest item, ResultRequestMap *list, Mutex *lock);
    virtual ResultRequest                  m_popResult(uint32_t key, ResultRequestMap *list, Mutex *lock);
    virtual ResultRequest                  m_getResult(uint32_t key, ResultRequestMap *list, Mutex *lock);
    virtual status_t                       m_getAllResultKeys(ResultRequestkeys *keylist, ResultRequestMap *list, Mutex *lock);
    virtual status_t                       m_getResultKeys(ResultRequestkeys *keylist, ResultRequestMap *list, EXYNOS_REQUEST_RESULT::TYPE type, Mutex *lock);

    virtual status_t                       m_push(int key, ExynosCamera3FrameFactory* item, FrameFactoryMap *list, Mutex *lock);
    virtual status_t                       m_pop(int key, ExynosCamera3FrameFactory** item, FrameFactoryMap *list, Mutex *lock);
    virtual status_t                       m_get(int streamID, ExynosCamera3FrameFactory** item, FrameFactoryMap *list, Mutex *lock);
    virtual bool                           m_find(int streamID, FrameFactoryMap *list, Mutex *lock);
    virtual status_t                       m_getList(FrameFactoryList *factorylist, FrameFactoryMap *list, Mutex *lock);
    virtual status_t                       m_setCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType, bool flag, Mutex *lock);
    virtual bool                           m_getCallbackDone(EXYNOS_REQUEST_RESULT::TYPE reqType, Mutex *lock);
    virtual status_t                       m_popResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestMap *list, ResultRequestList *resultList, Mutex *lock);
    virtual status_t                       m_popAndEraseResultsByType(EXYNOS_REQUEST_RESULT::TYPE reqType, ResultRequestMap *list, ResultRequestList *resultList, Mutex *lock);
    virtual status_t                       m_popAndEraseResultsByTypeAndStreamId(EXYNOS_REQUEST_RESULT::TYPE reqType, int streamID, ResultRequestMap *list, ResultRequestList *resultList, Mutex *lock);


private:
    uint32_t                      m_key;
    uint32_t                      m_frameCount;
    uint8_t                       m_captureIntent;
    camera3_capture_request_t     *m_request;
    CameraMetadata                m_serviceMeta;
    mutable Mutex                 m_serviceMetaLock;
    struct camera2_shot_ext       m_serviceShot;
    mutable Mutex                 m_serviceShotLock;
    CameraMetadata                m_resultMeta;
    mutable Mutex                 m_resultMetaLock;
    struct camera2_shot_ext       m_resultShot;
    mutable Mutex                 m_resultShotLock;
    struct camera2_shot_ext       m_prevShot;
    mutable Mutex                 m_prevShotLock;
    int                           m_streamIdList[HAL_STREAM_ID_MAX];

    EXYNOS_REQUEST::STATE         m_requestState;
    int                           m_requestId;

    ResultRequestMap              m_resultList;
    mutable Mutex                 m_resultListLock;
    bool                          m_resultStatus[EXYNOS_REQUEST_RESULT::CALLBACK_MAX];
    mutable Mutex                 m_resultStatusLock;
    mutable Mutex                 m_resultcallbackLock;
    int                           m_numOfOutputBuffers;
    int                           m_numOfCompleteBuffers;
    int                           m_numOfcallbackBuffers;
    int                           m_numOfDuplicateBuffers;
    List<int>                     m_requestOutputStreamList;
    List<int>                     m_requestInputStreamList;

    FrameFactoryMap               m_factoryMap;
    mutable Mutex                 m_factoryMapLock;

    bool                          m_isNeedInternalFrame;
    unsigned int                  m_pipelineDepth;
};

typedef list< uint32_t >           CallbackListkeys;
typedef list< uint32_t >::iterator CallbackListkeysIter;

class ExynosCameraCallbackSequencer{
public:
    ExynosCameraCallbackSequencer();
    ~ExynosCameraCallbackSequencer();

    status_t        pushFrameCount(uint32_t frameCount);
    uint32_t        popFrameCount();
    uint32_t        size();
    status_t        getFrameCountList(CallbackListkeys *list);
    status_t        deleteFrameCount(uint32_t frameCount);
    void            dumpList();
    status_t        flush();

private:
    status_t        m_init();
    status_t        m_deinit();
    status_t        m_push(EXYNOS_LIST_OPER::MODE operMode, uint32_t frameCount, CallbackListkeys *list, Mutex *lock);
    uint32_t        m_pop(EXYNOS_LIST_OPER::MODE operMode, CallbackListkeys *list, Mutex *lock);
    status_t        m_delete(uint32_t frameCount, CallbackListkeys *list, Mutex *lock);

private:
    CallbackListkeys m_requestFrameCountList;
    mutable Mutex    m_requestCbListLock;

};

namespace EXYNOS_REQUEST_TYPE {
    enum TYPE {
        PREVIEW      = 0,
        REPROCESSING = 1,
        MAX          = 2
    };
};

class ExynosCameraRequestManager : public virtual RefBase {
public:
    /* Constructor */
    ExynosCameraRequestManager(int cameraId, ExynosCameraParameters *param);

    /* Destructor */
    virtual ~ExynosCameraRequestManager();

public:
    /* related to camera3 device operations */
    status_t             constructDefaultRequestSettings(int type, camera_metadata_t **request);

    /* Android meta data translation functions */
    ExynosCameraRequestSP_sprt_t           registerServiceRequest(camera3_capture_request *request);
    ExynosCameraRequestSP_sprt_t           createServiceRequest();
    status_t                       deleteServiceRequest(uint32_t frameCount);
    ExynosCameraRequestSP_sprt_t           getServiceRequest(uint32_t frameCount);

    status_t                       setMetaDataConverter(ExynosCameraMetadataConverter *converter);
    ExynosCameraMetadataConverter* getMetaDataConverter();


    status_t                       setRequestsInfo(int key, ExynosCamera3FrameFactory *factory, ExynosCamera3FrameFactory *zslFactory = NULL);
    ExynosCamera3FrameFactory*      getFrameFactory(int key);

    status_t                       flush();

    /* other helper functions */
    status_t                       isPrevRequest(void);
    status_t                       clearPrevRequest(void);
    status_t                       clearPrevShot(void);

    status_t                       setCallbackOps(const camera3_callback_ops *callbackOps);
    status_t                       callbackRequest(ResultRequest result);
    ResultRequest                  createResultRequest(uint32_t frameCount,
                                                    EXYNOS_REQUEST_RESULT::TYPE type,
                                                    camera3_capture_result_t *captureResult,
                                                    camera3_notify_msg_t *notifyMsg);
    status_t                       getPreviousShot(struct camera2_shot_ext *pre_shot_ext);
    uint32_t                       getRequestCount(void);
    uint32_t                       getServiceRequestCount(void);
    void                           callbackSequencerLock();
    void                           callbackSequencerUnlock();

    status_t                       setFrameCount(uint32_t frameCount, uint32_t requestKey);
    status_t                       createCallbackThreads(camera3_stream_configuration *stream_list);
    status_t                       waitforRequestflush();
private:
    typedef map<uint32_t, ExynosCameraRequestSP_sprt_t>           RequestInfoMap;
    typedef map<uint32_t, ExynosCameraRequestSP_sprt_t>::iterator RequestInfoMapIterator;
    typedef list<ExynosCameraRequestSP_sprt_t>                    RequestInfoList;
    typedef list<ExynosCameraRequestSP_sprt_t>::iterator          RequestInfoListIterator;
    typedef map<uint32_t, uint32_t>                       RequestFrameCountMap;
    typedef map<uint32_t, uint32_t>::iterator             RequestFrameCountMapIterator;

    status_t                       m_pushBack(ExynosCameraRequestSP_sprt_t item, RequestInfoList *list, Mutex *lock);
    status_t                       m_popBack(ExynosCameraRequestSP_dptr_t item, RequestInfoList *list, Mutex *lock);
    status_t                       m_pushFront(ExynosCameraRequestSP_sprt_t item, RequestInfoList *list, Mutex *lock);
    status_t                       m_popFront(ExynosCameraRequestSP_dptr_t item, RequestInfoList *list, Mutex *lock);
    status_t                       m_get(uint32_t frameCount, ExynosCameraRequestSP_dptr_t item, RequestInfoList *list, Mutex *lock);

    status_t                       m_push(ExynosCameraRequestSP_sprt_t item, RequestInfoMap *list, Mutex *lock);
    status_t                       m_pop(uint32_t frameCount, ExynosCameraRequestSP_dptr_t item, RequestInfoMap *list, Mutex *lock);
    status_t                       m_get(uint32_t frameCount, ExynosCameraRequestSP_dptr_t item, RequestInfoMap *list, Mutex *lock);

    void                           m_printAllRequestInfo(RequestInfoMap *map, Mutex *lock);

    status_t                       m_delete(ExynosCameraRequestSP_sprt_t item);


    status_t                       m_pushBack(ResultRequest item, RequestInfoList *list, Mutex *lock);
    ResultRequest                  m_popBack(RequestInfoList *list, Mutex *lock);
    status_t                       m_pushFront(ResultRequest item, RequestInfoList *list, Mutex *lock);
    ResultRequest                  m_popFront(RequestInfoList *list, Mutex *lock);
    status_t                       m_get(uint32_t frameCount, RequestInfoList *list, Mutex *lock);

    status_t                       m_pushFactory(int key, ExynosCamera3FrameFactory* item, FrameFactoryMap *list, Mutex *lock);
    status_t                       m_popFactory(int key, ExynosCamera3FrameFactory** item, FrameFactoryMap *list, Mutex *lock);
    status_t                       m_getFactory(int key, ExynosCamera3FrameFactory** item, FrameFactoryMap *list, Mutex *lock);

    status_t                       m_pushStreamThread(int key, sp<callbackThread> item, StreamThreadMap *list, Mutex *lock);
    status_t                       m_popStreamThread(int key, sp<callbackThread> item, StreamThreadMap *list, Mutex *lock);
    status_t                       m_deleteStreamThread(StreamThreadMap *list, Mutex *lock);
//    status_t                       m_getFactory(int key, ExynosCamera3FrameFactory** item, FrameFactoryMap *list, Mutex *lock);

    status_t                       m_callbackCaptureRequest(camera3_capture_result_t *result);
    status_t                       m_callbackNotifyRequest(camera3_notify_msg_t *msg);
    status_t                       m_callbackPackingOutputBuffers(ExynosCameraRequestSP_sprt_t callbackRequest);
    ResultRequest                  m_callbackOutputBuffers(ExynosCameraRequestSP_sprt_t callbackRequest, ResultRequest callback);

    status_t                       m_getKey(uint32_t *key, uint32_t frameCount);
    status_t                       m_popKey(uint32_t *key, uint32_t frameCount);

    uint32_t                       m_generateResultKey();
    uint32_t                       m_getResultKey();

    status_t                       m_checkCallbackRequestSequence();
    status_t                       m_callbackRequest(ResultRequest result);

    status_t                       m_increasePipelineDepth(RequestInfoMap *map, Mutex *lock);

    void                           m_debugCallbackFPS();
    status_t                       m_createCallbackThreads(camera3_stream_configuration *stream_list);
    status_t                       m_NotifyCallback();
    status_t                       m_MetaCallback();
    status_t                       m_3AAMetaCallback();
    bool                           m_CallbackThreadFunc(int id, bool testflag = false);
#if 0
    /* Other helper functions */
    status_t        initShotData(void);
    status_t        checkAvailableStreamFormat(int format);
    uint32_t        getFrameNumber(void);
#endif
private:
    char                          m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    RequestInfoList               m_serviceRequests[EXYNOS_REQUEST_TYPE::MAX];
    mutable Mutex                 m_serviceRequestLock[EXYNOS_REQUEST_TYPE::MAX];

    RequestInfoMap                m_runningRequests[EXYNOS_REQUEST_TYPE::MAX];
    mutable Mutex                 m_runningRequestLock[EXYNOS_REQUEST_TYPE::MAX];

    mutable Mutex                 m_requestLock;

    int                           m_cameraId;

    camera_metadata_t             *m_defaultRequestTemplate[CAMERA3_TEMPLATE_COUNT];
    CameraMetadata                m_previousMeta;

    struct camera2_shot_ext       m_dummyShot;
    struct camera2_shot_ext       m_currShot;
    // TODO: remove this
    ExynosCameraParameters        *m_parameters;
    ExynosCameraMetadataConverter *m_converter;

    const camera3_callback_ops_t  *m_callbackOps;

    int32_t                       m_requestKey;
    mutable Mutex                 m_requestKeyLock;

    int32_t                       m_requestResultKey;
    mutable Mutex                 m_requestResultKeyLock;

    FrameFactoryMap               m_factoryMap;
    mutable Mutex                 m_factoryMapLock;

    StreamThreadMap               m_streamthreadMap;
    mutable Mutex                 m_streamthreadMapLock;

    FrameFactoryMap               m_zslFactoryMap;
    mutable Mutex                 m_zslFactoryMapLock;

    ExynosCameraCallbackSequencer *m_callbackSequencer;
    mutable Mutex                 m_callbackSequencerLock;

    int                           m_callbackTraceCount;

    struct camera2_shot_ext       *m_preShot;

    RequestFrameCountMap          m_requestFrameCountMap;
    mutable Mutex                 m_requestFrameCountMapLock;

    /* Thread for Partial Update */
    stream_callback_queue_t       *m_threadStreamDoneQ[HAL_STREAM_ID_MAX * 5];
    uint32_t                      m_streamPrevPos[HAL_STREAM_ID_MAX * 5];

    bool                          m_rawCallbackThreadFunc0(void);
    bool                          m_rawCallbackThreadFunc1(void);
    bool                          m_rawCallbackThreadFunc2(void);
    bool                          m_rawCallbackThreadFunc3(void);
    bool                          m_rawCallbackThreadFunc4(void);

    bool                          m_zslOutputCallbackThreadFunc0(void);
    bool                          m_zslOutputCallbackThreadFunc1(void);
    bool                          m_zslOutputCallbackThreadFunc2(void);
    bool                          m_zslOutputCallbackThreadFunc3(void);
    bool                          m_zslOutputCallbackThreadFunc4(void);

    bool                          m_zslInputCallbackThreadFunc0(void);
    bool                          m_zslInputCallbackThreadFunc1(void);
    bool                          m_zslInputCallbackThreadFunc2(void);
    bool                          m_zslInputCallbackThreadFunc3(void);
    bool                          m_zslInputCallbackThreadFunc4(void);

    bool                          m_previewCallbackThreadFunc0(void);
    bool                          m_previewCallbackThreadFunc1(void);
    bool                          m_previewCallbackThreadFunc2(void);
    bool                          m_previewCallbackThreadFunc3(void);
    bool                          m_previewCallbackThreadFunc4(void);

    bool                          m_jpegCallbackThreadFunc0(void);
    bool                          m_jpegCallbackThreadFunc1(void);
    bool                          m_jpegCallbackThreadFunc2(void);
    bool                          m_jpegCallbackThreadFunc3(void);
    bool                          m_jpegCallbackThreadFunc4(void);

    bool                          m_videoCallbackThreadFunc0(void);
    bool                          m_videoCallbackThreadFunc1(void);
    bool                          m_videoCallbackThreadFunc2(void);
    bool                          m_videoCallbackThreadFunc3(void);
    bool                          m_videoCallbackThreadFunc4(void);

    bool                          m_yuvCallbackThreadFunc0(void);
    bool                          m_yuvCallbackThreadFunc1(void);
    bool                          m_yuvCallbackThreadFunc2(void);
    bool                          m_yuvCallbackThreadFunc3(void);
    bool                          m_yuvCallbackThreadFunc4(void);

    ExynosCameraDurationTimer     m_callbackFlushTimer;

#ifdef CALLBACK_FPS_CHECK
    int32_t                       m_callbackFrameCnt = 0;
    ExynosCameraDurationTimer     m_callbackDurationTimer;
#endif

    sp<callbackThread>            m_notifyCallbackThread;
    bool                          m_notifyCallbackThreadFunc(void);
    stream_callback_queue_t       m_notifyQ;

    sp<callbackThread>            m_partial3AACallbackThread;
    bool                          m_partial3AACallbackThreadFunc(void);
    stream_callback_queue_t       m_partial3AAQ;

    sp<callbackThread>            m_metaCallbackThread;
    bool                          m_metaCallbackThreadFunc(void);
    stream_callback_queue_t       m_metaQ;

    sp<callbackThread>            m_requestDeleteThread;
    bool                          m_requestDeleteThreadFunc(void);
    stream_callback_queue_t       m_requestDeleteQ;
};

}; /* namespace android */
#endif
