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
#include <cutils/log.h>

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
    ret =  m_globalPlugInFactory->create(m_cameraId, m_pipeId, m_plugIn, m_plugInConverter);
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
    handleRet = m_handleFrame(frame);
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

    /* default map setting */
    map[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_PROCESS_BEFORE;
    map[PLUGIN_CONVERT_FRAME]       = (Map_data_t)frame.get();
    map[PLUGIN_CONVERT_PARAMETER]   = (Map_data_t)m_parameters;
    m_plugInConverter->make(&map);

    /* run process() in plugIn */
    ret = m_plugIn->process(&map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->process(frame(%d)) fail", frame->getFrameCount());
        goto func_exit;
    }

    /* result update */
    map[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_PROCESS_AFTER;
    m_plugInConverter->make(&map);

    state = ENTITY_BUFFER_STATE_COMPLETE;

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

ExynosCameraPipePlugIn::handle_status_t ExynosCameraPipePlugIn::m_handleFrame(ExynosCameraFrameSP_sptr_t frame)
{
    status_t ret;
    handle_status_t handleRet = PLUGIN_NO_ERROR;

    switch (m_pipeId) {
#ifdef USE_DUAL_CAMERA
    case PIPE_FUSION:
    case PIPE_FUSION_FRONT:
    case PIPE_FUSION_REPROCESSING:
        switch (frame->getSyncType()) {
        case SYNC_TYPE_BYPASS:
        case SYNC_TYPE_SWITCH:
            {
                ExynosCameraBuffer srcBuffer;
                ExynosCameraBuffer dstBuffer;

                ret = frame->getDstBuffer(getPipeId(), &dstBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("frame[%d]->getDstBuffer(%d) fail, ret(%d)",
                            frame->getFrameCount(), getPipeId(), ret);
                    handleRet = PLUGIN_ERROR;
                }

                ret = frame->getSrcBuffer(getPipeId(), &srcBuffer);
                if (ret != NO_ERROR) {
                    CLOGE("frame[%d]->getSrcBuffer1(%d) fail, ret(%d)",
                            frame->getFrameCount(), getPipeId(), ret);
                    handleRet = PLUGIN_ERROR;
                }

                for (int i = 0; i < srcBuffer.planeCount; i++) {
                    if (srcBuffer.fd[i] > 0) {
                        memcpy(dstBuffer.addr[i], srcBuffer.addr[i], srcBuffer.size[i]);
                    }
                }

                handleRet = PLUGIN_SKIP;
            }
            break;
        default:
            break;
        }
        break;
#endif
    default:
        break;
    }

    return handleRet;
}

void ExynosCameraPipePlugIn::m_init(int32_t *nodeNums)
{
    /* get the singleton instance for plugInFactory */
    m_globalPlugInFactory = ExynosCameraSingleton<ExynosCameraFactoryPlugIn>::getInstance();

    m_plugIn = NULL;
    m_plugInConverter = NULL;
}

}; /* namespace android */
