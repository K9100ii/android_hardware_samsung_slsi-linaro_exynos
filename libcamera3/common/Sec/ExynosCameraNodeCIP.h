/*
 * Copyright 2017, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef EXYNOS_CAMERA_NODE_CIP_H__
#define EXYNOS_CAMERA_NODE_CIP_H__

#include "ExynosCameraNode.h"

using namespace android;

namespace android {
/* ExynosCameraNode
 *
 * ingroup Exynos
 */

/*
 * Mapping table
 *
 * |-------------------------------------------------------------------------
 * | ExynosCamera        | ExynosCameraNode             | Jpeg HAL Interface
 * |-------------------------------------------------------------------------
 * | setSize()           | setSize() - NONE             | S_FMT
 * | setColorFormat()    | setColorFormat() - NONE      | S_FMT
 * | setBufferType()     | setBufferTyoe() - NONE       | S_FMT
 * | prepare()           | prepare() - m_setInput()     | S_INPUT
 * |                     | prepare() - m_setFmt()       | S_FMT
 * | reqBuffer()         | reqBuffer() - m_reqBuf()     | REQ_BUF
 * | queryBuffer()       | queryBuffer() - m_queryBuf() | QUERY_BUF
 * | setBuffer()         | setBuffer() - m_qBuf()       | Q_BUF
 * | getBuffer()         | getBuffer() - m_qBuf()       | Q_BUF
 * | putBuffer()         | putBuffer() - m_dqBuf()      | DQ_BUF
 * | start()             | start() - m_streamOn()       | STREAM_ON
 * | polling()           | polling() - m_poll()         | POLL
 * |-------------------------------------------------------------------------
 * | setBufferRef()      |                              |
 * | getSize()           |                              |
 * | getColorFormat()    |                              |
 * | getBufferType()     |                              |
 * |-------------------------------------------------------------------------
 *
 */

class ExynosCameraNodeCIP : public virtual ExynosCameraNode {
private:

public:
    /* Constructor */
    ExynosCameraNodeCIP();
    /* Destructor */
    virtual ~ExynosCameraNodeCIP();

    /* Create the instance */
    status_t create(const char *nodeName,
                            int cameraId,
                            enum EXYNOS_CAMERA_NODE_LOCATION location,
                            void *module = NULL);

    /* open Node */
    status_t open(int videoNodeNum);
    /* open Node */
    status_t open(int videoNodeNum, int operationMode);
    /* close Node */
    status_t close(void);
    /* get Jpeg Encoder */
    status_t getInternalModule(void **module);

    /* set v4l2 color format */
    status_t setColorFormat(int v4l2Colorformat, int planeCount, int batchSize,
                enum YUV_RANGE yuvRange = YUV_FULL_RANGE, camera_pixel_size pixelSize = CAMERA_PIXEL_SIZE_8BIT);

    /* set size */
    status_t setSize(int w, int h);

    /* request buffers */
    status_t reqBuffers(void);
    /* clear buffers */
    status_t clrBuffers(void);
    /* check buffers */
    unsigned int reqBuffersCount(void);

    /* set id */
    status_t setControl(unsigned int id, int value);
    status_t getControl(unsigned int id, int *value);

    /* polling */
    status_t polling(void);

    /* setInput */
    status_t setInput(int sensorId);

    /* setCrop */
    status_t setCrop(enum v4l2_buf_type type, int x, int y, int w, int h);

    /* setFormat */
    status_t setFormat(void);
    status_t setFormat(unsigned int bytesPerPlane[]);

    /* startNode */
    status_t start(void);
    /* stopNode */
    status_t stop(void);

    /* prepare Buffers */
    virtual status_t prepareBuffer(ExynosCameraBuffer *buf);

    /* putBuffer */
    status_t putBuffer(ExynosCameraBuffer *buf);

    /* getBuffer */
    status_t getBuffer(ExynosCameraBuffer *buf, int *dqIndex);

    /* dump the object info */
    void dump(void);
    /* dump state info */
    void dumpState(void);
    /* dump queue info */
    void dumpQueue(void);

private:

    /*
     * thoes member value should be declare in private
     * but we declare in publuc to support backward compatibility
     */
public:

private:
    cipInterface       *m_cipInterface;
    enum EXYNOS_CAMERA_NODE_LOCATION   m_nodeLocation;
    cipMode             m_cipMode;
    ExynosCameraBuffer  m_buffer;
};

};
#endif /* EXYNOS_CAMERA_NODE_JPEG_HAL_H__ */
