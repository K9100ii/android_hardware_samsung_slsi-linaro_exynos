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

    if (!m_supportMultiLibrary) {
        ret = m_create(sensorIds);
    } else {
        m_loaderThread = new ExynosCameraThread<ExynosCameraPipePlugIn>(this, &ExynosCameraPipePlugIn::m_loaderThreadFunc, "LoaderThread");
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::destroy(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    ExynosCameraPlugInSP_sptr_t          plugIn;
    ExynosCameraPlugInConverterSP_sptr_t plugInConverter;
    keyList list;
    keyList::iterator iter;
    PlugInSP_sptr_t item;

    m_getKeyList(&m_plugInList, &m_plugInListLock, &list);

    for(iter = list.begin(); iter != list.end(); iter++) {
        if (m_popItem(*iter, &m_plugInList, &m_plugInListLock, &item) != NO_ERROR) {
            CLOGE("pop fail key(%d)", *iter);
            continue;
        }
        plugIn = item->m_plugIn;
        plugInConverter = item->m_plugInConverter;

        ret = plugIn->destroy();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("m_plugIn->destroy() fail");
        }

        ret =  m_globalPlugInFactory->destroy(m_cameraId, plugIn, plugInConverter);
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

    ret = ExynosCameraSWPipe::start();
    if (ret != NO_ERROR) {
        CLOGE("start was failed, ret(%d)", ret);
        return ret;
    }

    if (!m_supportMultiLibrary) {
        ret = m_start(m_plugIn);
        if (ret != NO_ERROR) {
            CLOGE("m_plugIn->start() fail");
            return INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::stop(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    keyList list;
    keyList::iterator iter;
    PlugInSP_sptr_t item;

    m_getKeyList(&m_plugInList, &m_plugInListLock, &list);

    for(iter = list.begin(); iter != list.end(); iter++) {
        if (m_getItem(*iter, &m_plugInList, &m_plugInListLock, &item) != NO_ERROR) {
            CLOGE("pop fail key(%d)", *iter);
            continue;
        }
        funcRet |= m_stop(item->m_plugIn);
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

    if (!m_supportMultiLibrary) {
        ret = m_setupPipe(m_plugIn, m_plugInConverter, map);
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::setParameter(int key, void *data)
{
    m_updateSetCmd(key, data);

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

    m_updateGetCmd(key, data);

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
    map[PLUGIN_CONVERT_CONFIGURATIONS] = (Map_data_t)m_configurations;

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
    status_t ret;
    handle_status_t handleRet = PLUGIN_NO_ERROR;

    switch (m_scenario) {
#ifdef USE_DUAL_CAMERA
#ifdef USES_DUAL_CAMERA_SOLUTION_FAKE
    case PLUGIN_SCENARIO_FAKEFUSION_PREVIEW:
    case PLUGIN_SCENARIO_FAKEFUSION_REPROCESSING:
#endif
#ifdef USES_DUAL_CAMERA_SOLUTION_ARCSOFT
    case PLUGIN_SCENARIO_ZOOMFUSION_PREVIEW:
    case PLUGIN_SCENARIO_ZOOMFUSION_REPROCESSING:
    case PLUGIN_SCENARIO_BOKEHFUSION_PREVIEW:
    case PLUGIN_SCENARIO_BOKEHFUSION_REPROCESSING:
#endif
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
#endif // USE_DUAL_CAMERA

#ifdef USES_SW_VDIS
    case PLUGIN_SCENARIO_SWVDIS_PREVIEW:
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)1;
        break;
#endif //USES_SW_VDIS
    case PLUGIN_SCENARIO_HIFILLS_REPROCESSING:
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)1;
        break;
    default:
        CLOGW("Unknown scenario(%d)", m_scenario);
        (*map)[PLUGIN_PLUGIN_CUR_SRC_BUF_CNT] = (Data_int32_t)1;
        break;
    }

    return handleRet;
}

ExynosCameraPipePlugIn::handle_status_t ExynosCameraPipePlugIn::m_handleFrameAfter(ExynosCameraFrameSP_sptr_t frame, Map_t *map)
{
    status_t ret;
    handle_status_t handleRet = PLUGIN_NO_ERROR;

    switch (m_scenario) {
#ifdef USES_SW_VDIS
    case PLUGIN_SCENARIO_SWVDIS_PREVIEW:
        {
            buffer_manager_tag_t initTag;
            ExynosCameraBuffer srcBuffer;
            ExynosCameraBuffer dstBuffer;
            entity_buffer_state_t bufferState = ENTITY_BUFFER_STATE_NOREQ;

            int32_t srcBufIndex = -1;
            int32_t dstBufValid = -1;


            //Handle SRC buffer
            ret = frame->getSrcBuffer(m_pipeId, &srcBuffer);
            if (ret < NO_ERROR) {
                CLOGE("getBuffer fail, pipeId(%d), ret(%d), frame(%d)",
                        m_pipeId, ret, frame->getFrameCount());
            }

            srcBufIndex = (Data_uint32_t)(*map)[PLUGIN_SRC_BUF_USED];

            if (srcBufIndex != -1) {
                CLOGD("release buffer(%d)", srcBufIndex);

                srcBuffer.index = srcBufIndex;

                bufferState = ENTITY_BUFFER_STATE_COMPLETE;
            } else {
                CLOGW("Skip release buffer(%d) buffer_used(%d)", srcBuffer.index, srcBufIndex);
                bufferState = ENTITY_BUFFER_STATE_PROCESSING;
            }

            ret = frame->setSrcBufferState(m_pipeId, bufferState);
            if (ret != NO_ERROR) {
                CLOGE("Failed to set SRC_BUFFER_STATE to replace target buffer index(%d) to release pipeId(%d), ret(%d), frame(%d)",
                        srcBuffer.index, m_pipeId, ret, frame->getFrameCount());
            }

            ret = frame->setSrcBuffer(m_pipeId, srcBuffer);
            if (ret != NO_ERROR) {
                CLOGE("Failed to set SRC_BUFFER to replace target buffer index(%d) to release pipeId(%d), ret(%d), frame(%d)",
                        srcBuffer.index, m_pipeId, ret, frame->getFrameCount());
            }


            //Handle DST buffer
            dstBufValid = (Data_uint32_t)(*map)[PLUGIN_DST_BUF_VALID];
            if (dstBufValid >= 0) {
                handleRet = PLUGIN_NO_ERROR;
            } else {
                CLOGE("Invalid DST buffer(%d)", dstBufValid);
                handleRet = PLUGIN_ERROR;
            }
        }
        break;
#endif
    case PLUGIN_SCENARIO_HIFILLS_REPROCESSING:
        {
            ExynosCameraBuffer buffer;

            ret = frame->getSrcBuffer(m_pipeId, &buffer);
            if (ret < 0) {
                CLOGE("getSrcBuffer fail, pipeId(%d), ret(%d)", m_pipeId, ret);
            }

            ret = m_bufferSupplier->putBuffer(buffer);
            if (ret != NO_ERROR) {
                CLOGE("[F%d B%d]Failed to putBuffer for JPEG_SRC. ret %d",
                    frame->getFrameCount(), buffer.index, ret);
            }
        }
        break;
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
    m_plugInList.clear();
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

status_t ExynosCameraPipePlugIn::m_create(__unused int32_t *sensorIds)
{
    status_t ret = NO_ERROR;
    PlugInSP_sptr_t item;

    if (m_globalPlugInFactory == NULL) {
        CLOGE("m_globalPlugInFactory is NULL!!");
        return INVALID_OPERATION;
    }

    /* create the plugIn by pipeId */
    ret =  m_globalPlugInFactory->create(m_cameraId, m_pipeId, m_plugIn, m_plugInConverter, m_scenario);
    if (ret != NO_ERROR || m_plugIn == NULL || m_plugInConverter == NULL) {
        CLOGE("plugIn create failed(cameraId(%d), pipeId(%d)", m_cameraId, m_pipeId);
        return INVALID_OPERATION;
    }

    item = new PlugInObject(m_plugIn, m_plugInConverter);

    m_pushItem(m_scenario, &m_plugInList, &m_plugInListLock, item);

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

status_t ExynosCameraPipePlugIn::m_start(ExynosCameraPlugInSP_sptr_t plugIn)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (plugIn == NULL) {
        CLOGE("plugIn is NULL");
        return INVALID_OPERATION;
    }

    /* run start() in plugIn */
    if (ExynosCameraPlugIn::PLUGIN_START == plugIn->getState()) {
        CLOGE("plugIn PLUGIN_START skipped, already START");
    } else {
        ret = plugIn->start();
        if (ret != NO_ERROR) {
            CLOGE("plugIn->start() fail");
            return INVALID_OPERATION;
        }
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::m_stop(ExynosCameraPlugInSP_sptr_t plugIn)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;

    if (plugIn == NULL) {
        CLOGE("plugIn is NULL");
        return INVALID_OPERATION;
    }

    /* run stop() in plugIn */
    if (ExynosCameraPlugIn::PLUGIN_STOP == plugIn->getState()) {
        CLOGE("plugIn PLUGIN_STOP skipped, already STOP");
    } else {
        ret = plugIn->stop();
        funcRet |= ret;
        if (ret != NO_ERROR) {
            CLOGE("plugIn->stop() fail");
        }
    }

    return funcRet;
}

status_t ExynosCameraPipePlugIn::m_setupPipe(ExynosCameraPlugInSP_sptr_t plugIn, ExynosCameraPlugInConverterSP_sptr_t plugInConverter, Map_t *map)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (plugInConverter == NULL) {
        CLOGE("m_plugInConverter is NULL");
        return INVALID_OPERATION;
    }

    if (plugIn == NULL) {
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
    ret = plugInConverter->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    /* run setup() in plugIn */
    ret = plugIn->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    /* save the result to map */
    (*map)[PLUGIN_CONVERT_TYPE]        = (Map_data_t)PLUGIN_CONVERT_SETUP_AFTER;
    ret = plugInConverter->setup(map);
    if (ret != NO_ERROR) {
        CLOGE("m_plugIn->setup() fail");
        return INVALID_OPERATION;
    }

    return ret;
}


status_t ExynosCameraPipePlugIn::m_pushItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t item)
{
    status_t ret = NO_ERROR;
    PlugInMap::iterator iter;

    lock->lock();

    iter = pluginList->find(key);
    if (iter != pluginList->end()) {
        CLOGE("push pluginList failed fail", key);
        ret = INVALID_OPERATION;
    } else {
        PlugInPair object(key, item);
        pluginList->insert(object);
    }

    lock->unlock();
    return ret;
}

status_t ExynosCameraPipePlugIn::m_popItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t *item)
{
    status_t ret = NO_ERROR;
    PlugInMap::iterator iter;

    lock->lock();

    iter = pluginList->find(key);
    if (iter != pluginList->end()) {
        *item = iter->second;
        pluginList->erase(iter);
    } else {
        CLOGE("pop pluginList failed fail", key);
        ret = INVALID_OPERATION;
    }

    lock->unlock();
    return ret;
}

status_t ExynosCameraPipePlugIn::m_getItem(int key, PlugInMap *pluginList, Mutex *lock, PlugInSP_sptr_t *item)
{
    status_t ret = NO_ERROR;
    PlugInMap::iterator iter;

    lock->lock();

    iter = pluginList->find(key);
    if (iter != pluginList->end()) {
        *item = iter->second;
    } else {
        CLOGE("get pluginList failed fail", key);
        ret = INVALID_OPERATION;
    }

    lock->unlock();
    return ret;
}

bool ExynosCameraPipePlugIn::m_findItem(int key, PlugInMap *pluginList, Mutex *lock)
{
    bool ret = false;
    PlugInMap::iterator iter;

    lock->lock();

    iter = pluginList->find(key);
    if (iter != pluginList->end()) {
        ret = true;
    } else {
        ret = false;
    }

    lock->unlock();
    return ret;
}

status_t ExynosCameraPipePlugIn::m_getKeyList(PlugInMap *pluginList, Mutex *lock, keyList *list)
{
    status_t ret = NO_ERROR;
    PlugInMap::iterator iter;

    lock->lock();

    for (iter = pluginList->begin(); iter != pluginList->end() ; iter++) {
        list->push_back(iter->first);
    }

    lock->unlock();
    return ret;
}

status_t ExynosCameraPipePlugIn::m_updateSetCmd(int cmd, void *data)
{
    status_t ret = NO_ERROR;
    int nextScenario = -1;
    int curScenario = -1;

    if (m_supportMultiLibrary) {
    }

    switch(cmd) {
    case PLUGIN_PARAMETER_KEY_PREPARE:
        {
            CLOGD("command PLUGIN_PARAMETER_KEY_PREPARE");

            /* 0. thread join */
            if (m_supportMultiLibrary) {
                m_loaderThread->join();
            }

            /* 1. get scenario */
            nextScenario = *(int*)data;
            m_getScenario(curScenario);

            if (m_findItem(nextScenario, &m_plugInList, &m_plugInListLock) == true) {
                /* 2-1. update plugin data */
                CLOGD("command PLUGIN_PARAMETER_KEY_PREPARE already load plugin");
                PlugInSP_sptr_t item;
                m_setScenario(nextScenario);
                m_getItem(nextScenario, &m_plugInList, &m_plugInListLock, &item);
                m_plugIn = item->m_plugIn;
                m_plugInConverter = item->m_plugInConverter;
            } else {
                CLOGD("command PLUGIN_PARAMETER_KEY_PREPARE load plugin start");
                /* 2-2-1 set scenario */
                m_setScenario(nextScenario);

                /* 2-2-2 create */
                m_create();

                /* 2-2-3 thread run */
                if (m_supportMultiLibrary) {
                    m_loaderThread->run(PRIORITY_URGENT_DISPLAY);
                }
            }
        }
        break;
    case PLUGIN_PARAMETER_KEY_START:
        {
            CLOGD("command PLUGIN_PARAMETER_KEY_START");
            /* 0. thread join */
            if (m_supportMultiLibrary) {
                m_loaderThread->join();
            }

            /* 1. start plugin */
            m_start(m_plugIn);
            m_plugIn->setParameter(cmd, data);
        }
        break;
    case PLUGIN_PARAMETER_KEY_STOP:
        {
            CLOGD("command PLUGIN_PARAMETER_KEY_STOP");
            /* 0. thread join */
            if (m_supportMultiLibrary) {
                m_loaderThread->join();
            }

            /* 1. stop plugin */
            ret = m_stop(m_plugIn);
            if (ret != NO_ERROR) {
                CLOGE("plugIn is NULL, skipped stop");
                return INVALID_OPERATION;
            }

            m_plugIn->setParameter(cmd, data);
        }
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::m_updateGetCmd(int cmd, void *data)
{
    status_t ret = NO_ERROR;
    int nextScenario = -1;

    switch(cmd) {
    case PLUGIN_PARAMETER_KEY_GET_SCENARIO:
        {
            CLOGD("command PLUGIN_PARAMETER_KEY_GET_SCENARIO scenario(%d)", m_scenario);
            *(int*)data = m_scenario;
        }
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraPipePlugIn::m_setScenario(int scenario)
{
    status_t ret = NO_ERROR;
    if (m_scenario != scenario)
        CLOGD("change scenario(%d -> %d)", m_scenario, scenario);

    m_scenario = scenario;
    return ret;
}

status_t ExynosCameraPipePlugIn::m_getScenario(int& scenario)
{
    status_t ret = NO_ERROR;
    scenario = m_scenario;
    return ret;
}

bool ExynosCameraPipePlugIn::m_loaderThreadFunc(void)
{
    bool ret = false;

    /* 2. setup */
    m_setupPipe(m_plugIn, m_plugInConverter, &m_map);

    return ret;
}


}; /* namespace android */
