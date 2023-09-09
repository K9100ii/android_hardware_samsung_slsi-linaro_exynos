/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraPipePlugIn"
#include <log/log.h>

#include "ExynosCameraPipePlugIn.h"
#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
#include "ExynosCameraPlugInConverterFakeFusion.h"
#endif

namespace android {

ExynosCameraPipePlugIn::~ExynosCameraPipePlugIn()
{
    this->destroy();
}

status_t ExynosCameraPipePlugIn::create(__unused int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    ret = ExynosCameraSWPipe::create(sensorIds);
    if (ret != NO_ERROR) {
        CLOGE("create was failed, ret(%d)", ret);
        return ret;
    }

    if (m_globalPlugInFactory == NULL) {
        CLOGE("m_globalPlugInFactory is NULL!!");
        return INVALID_OPERATION;
    }

    /* create the plugIn by pipeId */
    ret =  m_globalPlugInFactory->create(m_cameraId, m_pipeId, m_plugIn, m_plugInConverter, m_mode);
    if (ret != NO_ERROR || m_plugIn == NULL || m_plugInConverter == NULL) {
        CLOGE("plugIn create failed(cameraId(%d), pipeId(%d)", m_cameraId, m_pipeId);
        return INVALID_OPERATION;
    }

    /* run create() in plugIn */
    ret = m_plugIn->create();
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->create() fail");
        return INVALID_OPERATION;
    }

    /* query() in plugIn */
    ret = m_plugIn->query(&m_map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->query() fail");
        return INVALID_OPERATION;
    }

    CLOGD("Info[v%d.%d, libname:%s, build time:%s %s, buf(src:%d, dst:%d)]",
            GET_MAJOR_VERSION((Data_uint32_t)m_map[PLUGIN_VERSION]),
            GET_MINOR_VERSION((Data_uint32_t)m_map[PLUGIN_VERSION]),
            (Pointer_const_char_t)m_map[PLUGIN_LIB_NAME],
            (Pointer_const_char_t)m_map[PLUGIN_BUILD_DATE],
            (Pointer_const_char_t)m_map[PLUGIN_BUILD_TIME],
            (Data_int32_t)m_map[PLUGIN_PLUGIN_MAX_SRC_BUF_CNT],
            (Data_int32_t)m_map[PLUGIN_PLUGIN_MAX_DST_BUF_CNT]);

    ret = m_plugInConverter->create(&m_map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugInConverter->create() fail");
        return INVALID_OPERATION;
    }

    return NO_ERROR;
}

status_t ExynosCameraPipePlugIn::destroy(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (m_plugIn != NULL) {
        ret = m_plugIn->destroy();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("m_plugIn->destroy() fail");
        }

        ret =  m_globalPlugInFactory->destroy(m_cameraId, m_plugIn, m_plugInConverter);
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("plugIn destroy failed(cameraId(%d), pipeId(%d)",
                    m_cameraId, m_pipeId);
        }
    }

    m_plugIn = NULL;
    m_plugInConverter = NULL;
    m_globalPlugInFactory = NULL;

    ExynosCameraSWPipe::destroy();

    return funcRet;
}

status_t ExynosCameraPipePlugIn::start(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (m_plugIn == NULL) {
        CLOGE("m_plugIn is NULL");
        return INVALID_OPERATION;
    }

    ret = ExynosCameraSWPipe::start();
    if (ret != NO_ERROR) {
        CLOGE("start was failed, ret(%d)", ret);
        return ret;
    }

    /* run start() in plugIn */
    ret = m_plugIn->start();
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->start() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::stop(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    /* run stop() in plugIn */
    ret = m_plugIn->stop();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->stop() fail");
    }

    ret = ExynosCameraSWPipe::stop();
    funcRet |= ret;
    if (ret != NO_ERROR) {
        CLOGE("stop was failed, ret(%d)", ret);
    }

    return funcRet;
}

status_t ExynosCameraPipePlugIn::setupPipe(Map_t *map)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (m_plugInConverter == NULL) {
        CLOGE("m_plugInConverter is NULL");
        return INVALID_OPERATION;
    }

    if (m_plugIn == NULL) {
        CLOGE("m_plugIn is NULL");
        return INVALID_OPERATION;
    }

    if (map == NULL) {
        CLOGE("map is NULL");
        return INVALID_OPERATION;
    }

    /* prepare map */
    (*map)[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_SETUP_BEFORE;
    (*map)[PLUGIN_CONVERT_PARAMETER]   = (Map_data_t)m_parameters;
    (*map)[PLUGIN_CONVERT_CONFIGURATIONS]   = (Map_data_t)m_configurations;
    ret = m_plugInConverter->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    /* run setup() in plugIn */
    ret = m_plugIn->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    /* save the result to map */
    (*map)[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_SETUP_AFTER;
    ret = m_plugInConverter->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::setParameter(int key, void *data)
{
    if (m_plugIn == NULL) {
        CLOGE("m_plugIn is NULL");
        return INVALID_OPERATION;
    }

    return m_plugIn->setParameter(key, data);
}

status_t ExynosCameraPipePlugIn::getParameter(int key, void *data)
{
    if (m_plugIn == NULL) {
        CLOGE("m_plugIn is NULL");
        return INVALID_OPERATION;
    }

    return m_plugIn->getParameter(key, data);
}

status_t ExynosCameraPipePlugIn::m_run(void)
{
    status_t ret = NO_ERROR;

    Map_t map;
    ExynosCameraFrameSP_sptr_t frame = NULL;
    entity_buffer_state_t state = ENTITY_BUFFER_STATE_ERROR;
    handle_status_t handleRet;

    ret = m_inputFrameQ->waitAndPopProcessQ(&frame);
    if (ret != NO_ERROR) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout(%d, %d)",
                    m_inputFrameQ->getSizeOfProcessQ(),
                    frame != NULL ? frame->getFrameCount() : -1);
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (frame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    /* check the frame state */
    switch (frame->getFrameState()) {
    case FRAME_STATE_SKIPPED:
    case FRAME_STATE_INVALID:
        CLOGE("frame(%d) state is invalid(%d)",
                frame->getFrameCount(), frame->getFrameState());
        goto func_exit;
        break;
    default:
        break;
    }

    /*
     * 1. handling the frame
     *    logical area by each pipeId.
     */
    handleRet = m_handleFrameBefore(frame, &map);
    switch (handleRet) {
    case PLUGIN_SKIP:
        state = ENTITY_BUFFER_STATE_COMPLETE;
    case PLUGIN_ERROR:
        goto func_exit;
        break;
    default:
        break;
    }

    /*
     * 2. run the plugIn
     */
    if (m_plugIn == NULL) {
        CLOGE("m_plugIn is NULL");
        goto func_exit;
    }

    /* PROCESS_BEFORE */
    /* default map setting */
    map[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_PROCESS_BEFORE;
    map[PLUGIN_CONVERT_FRAME]       = (Map_data_t)frame.get();
    map[PLUGIN_CONVERT_PARAMETER]   = (Map_data_t)m_parameters;

    ret = m_plugInConverter->make(&map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->make(map) fcount(%d) fail", frame->getFrameCount());
        goto func_exit;
    }

    /* run process() in plugIn */
    ret = m_plugIn->process(&map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->process(frame(%d)) fail", frame->getFrameCount());
        goto func_exit;
    }

    /* PROCESS_AFTER */
    /* default map setting */
    map[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_PROCESS_AFTER;

    ret = m_plugInConverter->make(&map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->make(map) fcount(%d) fail", frame->getFrameCount());
        goto func_exit;
    }

    handleRet = m_handleFrameAfter(frame, &map);
    switch(handleRet) {
    case PLUGIN_ERROR:
        state = ENTITY_BUFFER_STATE_ERROR;
        break;
    case PLUGIN_SKIP:
    case PLUGIN_NO_ERROR:
    default:
        state = ENTITY_BUFFER_STATE_COMPLETE;
        break;
    }

func_exit:
    /*
     * 3. push the frame to outputQ
     */
    ret = frame->setDstBufferState(m_pipeId, state);
    if (ret != NO_ERROR) {
        CLOGE("frame->setDstBufferState(%s) state(%d) fail, frameCount(%d)",
            this->getPipeName(), state, frame->getFrameCount());
        goto func_exit;
    }

    ret = frame->setEntityState(m_pipeId, ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("frame->setEntityState(%s, ENTITY_STATE_FRAME_DONE) fail, frameCount(%d)",
            this->getPipeName(), frame->getFrameCount());
    }

    m_outputFrameQ->pushProcessQ(&frame);

    return NO_ERROR;
}

ExynosCameraPipePlugIn::handle_status_t ExynosCameraPipePlugIn::m_handleFrameBefore(ExynosCameraFrameSP_sptr_t frame, Map_t *map)
{
    handle_status_t handleRet = PLUGIN_NO_ERROR;

    switch (m_pipeId) {
#ifdef USE_DUAL_CAMERA
    case PIPE_FUSION:
    case PIPE_FUSION_FRONT:
    case PIPE_FUSION_REPROCESSING:
    {
#if 1
        switch (frame->getFrameType()) {
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
        case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
            (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)2;
            break;
        default:
            (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)1;
            break;
        }
#else

        ExynosCameraBuffer srcBuffer;
        ExynosCameraBuffer dstBuffer;

        ret = frame->getDstBuffer(getPipeId(), &dstBuffer);
        if(ret != NO_ERROR) {
            CLOGE("frame[%d]->getDstBuffer(%d) fail, ret(%d)",
                frame->getFrameCount(), getPipeId(), ret);
            handleRet = PLUGIN_ERROR;
        }

        ret = frame->getSrcBuffer(getPipeId(), &srcBuffer);
        if(ret != NO_ERROR) {
            CLOGE("frame[%d]->getSrcBuffer1(%d) fail, ret(%d)",
                frame->getFrameCount(), getPipeId(), ret);
            handleRet = PLUGIN_ERROR;
        }

        for(int i = 0; i < srcBuffer.planeCount; i++) {
            if(srcBuffer.fd[i] > 0) {
                int copySize = srcBuffer.size[i];
                if(dstBuffer.size[i] < copySize) {
                    CLOGW("dstBuffer.size[%d](%d) < copySize(%d). so, copy only dstBuffer.size[%d]",
                        i, dstBuffer.size[i], copySize, i);
                    copySize = dstBuffer.size[i];
                }

                memcpy(dstBuffer.addr[i], srcBuffer.addr[i], copySize);
            }
        }

        handleRet = PLUGIN_SKIP;
#endif
        break;
    }
#endif

#ifdef USES_SW_VDIS
    case PIPE_VDIS:
        {
        int dstBufferIndex = -1;
        ExynosCameraBuffer dstBuffer;

        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)1;

        ret = m_bufferManager[CAPTURE_NODE]->getBuffer(&dstBufferIndex, EXYNOS_CAMERA_BUFFER_POSITION_IN_HAL, &dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                    m_pipeId, ret, frame->getFrameCount());

            //Release SRC buffer
            ExynosCameraBuffer srcBuffer;
            ret = frame->getSrcBuffer(m_pipeId, &srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }

            ret = m_bufferManager[CAPTURE_NODE_3]->putBuffer(srcBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret < NO_ERROR) {
                CLOGE("getBufferManager fail, pipeId(%d), ret(%d), frame(%d), index(%d)",
                        m_pipeId, ret, frame->getFrameCount(), srcBuffer.index);
            }

#ifdef SUPPORT_ME
            //Relase ME buffer
            ExynosCameraBuffer meBuffer;
            ret = frame->getSrcBuffer(m_pipeId, &meBuffer, 1);
            if (ret < NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }

            ret = m_bufferManager[CAPTURE_NODE_2]->putBuffer(meBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret < NO_ERROR) {
                CLOGE("getBufferManager fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }
#endif

            return PLUGIN_ERROR;
        }

        CLOGD("dst buffer index(%d)", dstBufferIndex);

        ret = frame->setDstBufferState(getPipeId(), ENTITY_BUFFER_STATE_REQUESTED);
        if (ret != NO_ERROR) {
            CLOGE("setdst Buffer state failed(%d) frame(%d)", ret, frame->getFrameCount());
            handleRet = PLUGIN_ERROR;
        }

        ret = frame->setDstBuffer(getPipeId(), dstBuffer);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d]->setDstBuffer(%d) fail, ret(%d)",
                    frame->getFrameCount(), getPipeId(), ret);
            handleRet = PLUGIN_ERROR;
        }
        }
        break;
#endif //USES_SW_VDIS
    default:
        break;
    }

    return handleRet;
}

ExynosCameraPipePlugIn::handle_status_t ExynosCameraPipePlugIn::m_handleFrameAfter(__unused ExynosCameraFrameSP_sptr_t frame, __unused Map_t *map)
{
#ifdef USES_SW_VDIS
    status_t ret;
#endif
    handle_status_t handleRet = PLUGIN_NO_ERROR;

    switch (m_pipeId) {
#ifdef USES_SW_VDIS
    case PIPE_VDIS:
        {
            ExynosCameraBuffer buffer;
            ExynosCameraBuffer dstBuffer;
            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;

            int32_t srcBufIndex = -1;
            int32_t dstBufValid = -1;

            //Release SRC buffer
            ret = frame->getSrcBuffer(m_pipeId, &buffer);
            if (ret < NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }
#if 0
            srcBufIndex = buffer.index;
#else
            srcBufIndex = (Data_uint32_t)(*map)[PLUGIN_SRC_BUF_USED];
#endif
            if (srcBufIndex != -1) {
                CLOGD("release buffer(%d)", srcBufIndex);
                ret = m_bufferManager[CAPTURE_NODE_3]->putBuffer(srcBufIndex, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret < NO_ERROR) {
                    CLOGE("getBufferManager fail, pipeId(%d), ret(%d), frame(%d), index(%d)",
                            m_pipeId, ret, frame->getFrameCount(), srcBufIndex);
                }
            } else {
                CLOGW("Skip release buffer(%d) buffer_used(%d)", buffer.index, srcBufIndex);
            }

#ifdef SUPPORT_ME
            //Relase ME buffer
            ret = frame->getSrcBuffer(m_pipeId, &buffer, 1);
            if (ret < NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }

            ret = m_bufferManager[CAPTURE_NODE_2]->putBuffer(buffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
            if (ret < NO_ERROR) {
                CLOGE("getBufferManager fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }
#endif

            /* handling dst buffer */
            ret = frame->getDstBuffer(m_pipeId, &dstBuffer);
            if (ret < NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }

            dstBufValid = (Data_uint32_t)(*map)[PLUGIN_DST_BUF_VALID];
            if (dstBufValid >= 0) {
                bufferState = ENTITY_BUFFER_STATE_COMPLETE;
                handleRet = PLUGIN_NO_ERROR;
            } else {
                //Release DST buffer
                bufferState = ENTITY_BUFFER_STATE_ERROR;
                handleRet = PLUGIN_ERROR;

                CLOGD("release dst buffer(%d)", dstBuffer.index);
                ret = m_bufferManager[CAPTURE_NODE]->putBuffer(dstBuffer.index, EXYNOS_CAMERA_BUFFER_POSITION_NONE);
                if (ret < NO_ERROR) {
                    CLOGE("getBufferManager fail, pipeId(%d), ret(%d), frame(%d), index(%d)",
                            m_pipeId, ret, frame->getFrameCount(), srcBufIndex);
                }
            }

            CLOGD("release src buf(%d) dst buf(I:%d)(V:%d)(S:%d) me buf(%d)", srcBufIndex, dstBuffer.index, dstBufValid, bufferState, buffer.index);
        }
        break;
#endif
    default:
        break;
    }

    return handleRet;
}

void ExynosCameraPipePlugIn::m_init(__unused int32_t *nodeNums)
{
    /* get the singleton instance for plugInFactory */
    m_globalPlugInFactory = ExynosCameraSingleton<ExynosCameraFactoryPlugIn>::getInstance();

    m_plugIn = NULL;
    m_plugInConverter = NULL;
}

status_t ExynosCameraPipePlugIn::m_setup(Map_t *map)
{
    status_t ret = NO_ERROR;

    switch (m_pipeId) {
#ifdef USE_DUAL_CAMERA
    case PIPE_FUSION:
        float** outImageRatioList;
        int**   outNeedMarginList;
        int*    sizeOfList;

        /* HACK */
        if (m_cameraId == CAMERA_ID_BACK_1) {
            //break;
        }

        outImageRatioList = (Array_float_addr_t)(*map)[PLUGIN_IMAGE_RATIO_LIST];
        outNeedMarginList = (Array_pointer_int_t)(*map)[PLUGIN_NEED_MARGIN_LIST];
        sizeOfList = (Array_buf_t)(*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE];

        if (outImageRatioList == NULL || outNeedMarginList == NULL || sizeOfList == NULL) {
            CLOGE("Ratio list is NUL");
        } else {

            CLOGD("sizeOfList(%d)", sizeOfList[0]);

            for(int i = 0; i < sizeOfList[0]; i++) {
                CLOGD(" ImageRatio[%d](%f)(%f), Margin[%d](%d)(%d)",
                    i, outImageRatioList[0][i], outImageRatioList[1][i],
                    i, outNeedMarginList[0][i], outNeedMarginList[1][i]);
            }
#ifdef USE_ARCSOFT_FUSION_LIB
            m_parameters->setPreviewNeedMarginList(outNeedMarginList, sizeOfList[0]);
            m_parameters->setImageRatioList(outImageRatioList, sizeOfList[0]);
#endif
        }

        break;

    case PIPE_FUSION_FRONT:
    case PIPE_FUSION_REPROCESSING:
        break;
#endif
    default:
        break;
    }

    return ret;
}

}; /* namespace android */
