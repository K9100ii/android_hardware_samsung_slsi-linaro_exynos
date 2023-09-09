/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_STREAM_MANAGER_H
#define EXYNOS_CAMERA_STREAM_MANAGER_H

#include <log/log.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <utils/List.h>
#include <utils/threads.h>
#include <utils/RefBase.h>
#include <map>
#include <list>

#include <hardware/camera.h>
#include <hardware/camera3.h>

#include "ExynosCameraDefine.h"
#include "ExynosCameraParameters.h"

#include "exynos_format.h"

namespace android {

using namespace std;

namespace EXYNOS_STREAM {
    enum STATE {
        HAL_STREAM_STS_INIT = 0x00,
        HAL_STREAM_STS_INVALID = 0x01,
        HAL_STREAM_STS_VALID = 0x02,
        HAL_STREAM_STS_UNREGISTERED = 0x11,
        HAL_STREAM_STS_REGISTERED = 0x12
    };
};

class ExynosCameraStream  : public RefBase {
private:
    ExynosCameraStream(){};

public:


    ExynosCameraStream(int id, camera3_stream_t *stream);
    virtual ~ExynosCameraStream();

    virtual status_t          setStream(camera3_stream_t *stream);
    virtual status_t          getStream(camera3_stream_t **stream);
    virtual status_t          setID(int id);
    virtual status_t          getID(int *id);
    virtual status_t          setFormat(int format, camera_pixel_size pixelSize = CAMERA_PIXEL_SIZE_8BIT);
    virtual status_t          getFormat(int *format, camera_pixel_size *pixelSize);
    virtual status_t          setPlaneCount(int planes);
    virtual status_t          getPlaneCount(int *planes);
    virtual status_t          setOutputPortId(int id);
    virtual status_t          getOutputPortId(int *id);
    virtual status_t          setRegisterStream(EXYNOS_STREAM::STATE state);
    virtual status_t          getRegisterStream(EXYNOS_STREAM::STATE *state);
    virtual status_t          setRegisterBuffer(EXYNOS_STREAM::STATE state);
    virtual status_t          getRegisterBuffer(EXYNOS_STREAM::STATE *state);
    virtual status_t          setRequestBuffer(int bufferCnt);
    virtual status_t          getRequestBuffer(int *bufferCnt);

private:
    status_t          m_init();
    status_t          m_deinit();

private:
    camera3_stream_t            *m_stream;
    int                         m_id;
    int                         m_actualFormat;
    camera_pixel_size           m_actualPixelSize;
    int                         m_planeCount;
    int                         m_outputPortId;
    EXYNOS_STREAM::STATE        m_registerStream;
    EXYNOS_STREAM::STATE        m_registerBuffer;
    int                         m_requestbuffer;
};

class ExynosCameraStreamManager : public RefBase {
public:
    /* Constructor */
    ExynosCameraStreamManager(int cameraId);

    /* Destructor */
    virtual ~ExynosCameraStreamManager();

    status_t createStream(int id, camera3_stream_t *stream, ExynosCameraStream **newStream);
    status_t deleteStream(int id);

    status_t getStream(int id, ExynosCameraStream **stream);

    status_t getStreamKeys(List<int>* keylist);

    bool     findStream(int id);

    status_t setYuvStreamMaxCount(int32_t count);
    int32_t  getYuvStreamCount(void);
    int32_t  getYuvStallStreamCount(void);
    int32_t  getTotalYuvStreamCount(void);

    int      getYuvStreamId(int outputPortId);
    int      getOutputPortId(int streamId);

    status_t increaseInputStreamCount(int inputStreamMaxNum);

    status_t dumpCurrentStreamList(void);

protected:
    typedef map<int, ExynosCameraStream*>            StreamInfoMap;
    typedef map<int, ExynosCameraStream*>::iterator  StreamInfoIterator;

private:
    void     m_init();
    void     m_deinit();

    status_t m_insert(int id, ExynosCameraStream *item, StreamInfoMap *list, Mutex *lock);
    status_t m_find(int id, StreamInfoMap *list, Mutex *lock);
    status_t m_get(int id, ExynosCameraStream **item, StreamInfoMap *list, Mutex *lock);
    status_t m_delete(int id, StreamInfoMap *list, Mutex *lock);
    status_t m_delete(ExynosCameraStream *stream);

    status_t m_setYuvStreamId(int outputPortId, int streamId);
    status_t m_increaseYuvStreamCount(int streamId);
    status_t m_decreaseYuvStreamCount(int streamId);
    status_t m_decreaseInputStreamCount(int streamId);

protected:
    int                         m_cameraId;

    StreamInfoMap               m_streamInfoMap;
    mutable Mutex               m_streamInfoLock;
    int32_t                     m_yuvStreamCount;
    int32_t                     m_yuvStallStreamCount;
    int32_t                     m_yuvStreamMaxCount;
    int                         m_inputStreamCount;
    int                         m_yuvStreamIdMap[ExynosCameraParameters::YUV_OUTPUT_PORT_ID_MAX];
};

}; /* namespace android */
#endif
