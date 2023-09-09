/*
**
** Copyright 2013, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraPipeFusion"
#include <cutils/log.h>

#include "ExynosCameraPipeFusion.h"

namespace android {

//#define EXYNOS_CAMERA_FUSION_PIPE_DEBUG

#ifdef EXYNOS_CAMERA_FUSION_PIPE_DEBUG
#define EXYNOS_CAMERA_FUSION_PIPE_DEBUG_LOG CLOGD
#else
#define EXYNOS_CAMERA_FUSION_PIPE_DEBUG_LOG CLOGV
#endif

ExynosCameraPipeFusion::~ExynosCameraPipeFusion()
{
    this->destroy();
}

status_t ExynosCameraPipeFusion::create(__unused int32_t *sensorIds)
{
    m_mainThread = ExynosCameraThreadFactory::createThread(this, &ExynosCameraPipeFusion::m_mainThreadFunc, "FusionThread");

    m_inputFrameQ = new frame_queue_t(m_mainThread);

    CLOGI("create() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::destroy(void)
{
    status_t ret = NO_ERROR;

    if (m_inputFrameQ != NULL) {
        m_inputFrameQ->release();
        delete m_inputFrameQ;
        m_inputFrameQ = NULL;
    }

    ret = m_destroyFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_destroyFusion() fail");
        return ret;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::start(void)
#ifdef USE_DUAL_CAMERA
{
    CLOGD(" ");

    status_t ret = NO_ERROR;

    // setInfo when start
    ExynosCameraDualPreviewFrameSelector *dualPreviewFrameSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
    ret = dualPreviewFrameSelector->setInfo(m_cameraId, 2, m_parameters->getDOF());
    if (ret != NO_ERROR) {
        CLOGE("dualPreviewFrameSelector->setInfo(id(%d)", m_cameraId);
        return ret;
    }

    /*
     * postpone create fusion library on m_run()
     * because, fusion library must call by same thread id.
     * it is back camera.
     */
    /*
    // create fusion library
    ret = m_createFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_createFusion() fail");
        return ret;
    }
    */

    m_flagTryStop = false;

    return NO_ERROR;
}
#else
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d])invalid function, assert!!!!", __FUNCTION__, __LINE__);
}
#endif // USE_DUAL_CAMERA

status_t ExynosCameraPipeFusion::stop(void)
{
    CLOGD(" ");
    int ret = 0;

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    {
        Mutex::Autolock lock(ExynosCameraFusionMutex::getInstance()->getFusionMutex());
        // clear when stop
        ExynosCameraDualPreviewFrameSelector *dualPreviewFrameSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
        ret = dualPreviewFrameSelector->clear(m_cameraId);
        if (ret != NO_ERROR) {
            CLOGE("dualPreviewFrameSelector->clear(%d)", m_cameraId);
            return ret;
        }
    }
    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::startThread(void)
{
    CLOGV(" ");

    if (m_outputFrameQ == NULL) {
        CLOGE("outputFrameQ is NULL, cannot start");
        return INVALID_OPERATION;
    }

    m_mainThread->run(m_name);

    CLOGI("startThread is succeed (%d)", getPipeId());

    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::m_run(void)
{
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;

    int ret = 0;

    ret = m_inputFrameQ->waitAndPopProcessQ(&newFrame);
    if (ret < 0) {
        /* TODO: We need to make timeout duration depends on FPS */
        if (ret == TIMED_OUT) {
            CLOGW("wait timeout");
        } else {
            CLOGE("wait and pop fail, ret(%d)", ret);
            /* TODO: doing exception handling */
        }
        return ret;
    }

    if (newFrame == NULL) {
        CLOGE("new frame is NULL");
        return NO_ERROR;
    }

    entity = newFrame->searchEntityByPipeId(getPipeId());
    if (entity == NULL) {
        CLOGE("frame(%d) entity == NULL, skip", newFrame->getFrameCount());
        goto func_exit;
    }

    if (entity->getEntityState() == ENTITY_STATE_FRAME_SKIP) {
        CLOGE("frame(%d) entityState(ENTITY_STATE_FRAME_SKIP), skip", newFrame->getFrameCount());
        goto func_exit;
    }

    if (entity->getSrcBufState() == ENTITY_BUFFER_STATE_ERROR) {
        CLOGE("frame(%d) entityState(ENTITY_BUFFER_STATE_ERROR), skip", newFrame->getFrameCount());
        goto func_exit;
    }

    /*
     * if m_mangeFusion is fail,
     * the input newFrame need to drop
     */
    ret = m_manageFusion(newFrame);
    if (ret != NO_ERROR) {
        CLOGE("m_manageFusion(newFrame(%d)) fail", newFrame->getFrameCount());
        goto func_exit;
    }

    // the frame though fusion, will pushProcessQ in m_manageFusion()

    return NO_ERROR;

func_exit:
    ret = entity->setDstBufState(ENTITY_BUFFER_STATE_ERROR);
    if (ret != NO_ERROR) {
        CLOGE("setDstBufState(ENTITY_BUFFER_STATE_ERROR) fail");
    }

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::m_manageFusion(ExynosCameraFrameSP_sptr_t newFrame)
#ifdef USE_DUAL_CAMERA
{
    status_t ret = NO_ERROR;

    bool flagSynced = false;

    ExynosCameraDualPreviewFrameSelector *dualPreviewFrameSelector = NULL;
    Mutex *fusionMutex = ExynosCameraFusionMutex::getInstance()->getFusionMutex();

    array<ExynosCameraFrameSP_sptr_t, CAMERA_ID_MAX> frame;
    frame_queue_t     *outputFrameQ[CAMERA_ID_MAX] = {NULL, };
    ExynosCameraBufferManager *srcBufferManager[CAMERA_ID_MAX] = {NULL, };
    ExynosCameraBufferManager *dstBufferManager = NULL;

    DOF *dof[CAMERA_ID_MAX] = {NULL, };

    ExynosCameraBuffer srcBuffer[CAMERA_ID_MAX];
    ExynosRect srcRect[CAMERA_ID_MAX];

    ExynosCameraBuffer dstBuffer;
    ExynosRect dstRect;

    struct camera2_shot_ext  src_shot_ext[CAMERA_ID_MAX];
    struct camera2_shot_ext *ptr_src_shot_ext[CAMERA_ID_MAX] = {NULL, };

    struct camera2_udm tempUdm;

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        frame[i] = NULL;
        outputFrameQ[i] = NULL;
        srcBufferManager[i] = NULL;

        dof[i] = NULL;

        ptr_src_shot_ext[i] = NULL;
    }

    frame[m_cameraId] = newFrame;
    outputFrameQ[m_cameraId] = m_outputFrameQ;
    srcBufferManager[m_cameraId] =  m_bufferManager[OUTPUT_NODE];
    dstBufferManager = m_bufferManager[CAPTURE_NODE];

    // create fusion library
    ret = m_createFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_createFusion() fail");
        goto func_exit;
    }

    if (m_flagReadyFusion() == false) {
        goto func_exit;
    }

    /* update zoomRatio intentially for fast zoom effect */
    newFrame->getUserDynamicMeta(&tempUdm);
    tempUdm.zoomRatio = m_parameters->getZoomRatio();
    newFrame->storeUserDynamicMeta(&tempUdm);

    dualPreviewFrameSelector = ExynosCameraSingleton<ExynosCameraDualPreviewFrameSelector>::getInstance();
    ret = dualPreviewFrameSelector->managePreviewFrameHoldList(m_cameraId,
        m_outputFrameQ,
        newFrame,
        getPipeId(),
        true,
        0,
        srcBufferManager[m_cameraId]);

    if (ret != NO_ERROR) {
        CLOGE("dualPreviewFrameSelector->managePreviewFrameHoldList(%d)", m_cameraId);
        return ret;
    }

    /*
     * when camera front, just return true.
     * when camera back, it will check sync and running fusion
     */
    if (m_cameraId == m_subCameraId) {
        return ret;
    }

    // we need protect selectFrames ~ m_executeFusion ~  pushProcessQ. with stop()'s clear();
    fusionMutex->lock();

    flagSynced = dualPreviewFrameSelector->selectFrames(m_cameraId,
                                                        frame[m_mainCameraId], &outputFrameQ[m_mainCameraId], &srcBufferManager[m_mainCameraId],
                                                        frame[m_subCameraId], &outputFrameQ[m_subCameraId], &srcBufferManager[m_subCameraId]);

    // we did't get synced frame.
    if (flagSynced == false) {
        CLOGD("not synced.");
        goto done;
    }

    /*
     * until here, if fail is happen, just return.
     * after this code, it will handle synced frame0, frame1
     */

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_flagValidCameraId[i] == false)
            continue;

        if (frame[i] == NULL || outputFrameQ[i] == NULL || srcBufferManager[i] == NULL) {
            CLOGE("frame[%d] == NULL || outputFrameQ[%d] == %p || srcBufferManager[%d] == %p)",
                i, i, outputFrameQ[i], i, srcBufferManager[i]);
            goto func_exit;
        }
    }

    EXYNOS_CAMERA_FUSION_PIPE_DEBUG_LOG("dualPreviewFrameSelector->selectFrames([%d]:%d, [%d]:%d)",
        m_mainCameraId, (int)(ns2ms(frame[m_mainCameraId]->getTimeStamp())),
        m_subCameraId, (int)(ns2ms(frame[m_subCameraId]->getTimeStamp())));

    ///////////////////////////////////////
    // get source information of all cameras

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_flagValidCameraId[i] == false)
            continue;

        if (frame[i] == NULL)
            continue;

        // buffer
        ret = frame[i]->getSrcBuffer(getPipeId(), &srcBuffer[i]);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d]->getSrcBuffer(%d) fail, ret(%d)", i, getPipeId(), ret);
            goto func_exit;
        }

        // rect
        ret = frame[i]->getSrcRect(getPipeId(), &srcRect[i]);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d]->getSrcRect(%d) fail, ret(%d)", i, getPipeId(), ret);
            goto func_exit;
        }

        // is this need?
        switch (srcRect[i].colorFormat) {
        case V4L2_PIX_FMT_NV21:
            srcRect[i].fullH = ALIGN_UP(srcRect[i].fullH, 2);
            break;
        default:
            srcRect[i].fullH = ALIGN_UP(srcRect[i].fullH, GSCALER_IMG_ALIGN);
            break;
        }

        camera2_node_group node_group_info;
        memset(&node_group_info, 0x0, sizeof(camera2_node_group));
        int zoom = 0;
        frame[i]->getNodeGroupInfo(&node_group_info, PERFRAME_INFO_3AA);
        zoom = frame[i]->getZoom();

        if (node_group_info.leader.input.cropRegion[2] == 0 ||
            node_group_info.leader.input.cropRegion[3] == 0) {
            CLOGW("frame[%d] node_group_info.leader.input.cropRegion(%d, %d, %d, %d) is not valid",
                node_group_info.leader.input.cropRegion[0],
                node_group_info.leader.input.cropRegion[1],
                node_group_info.leader.input.cropRegion[2],
                node_group_info.leader.input.cropRegion[3]);
        } else {
            if (zoom == 0) {
                srcRect[i].x = 0;
                srcRect[i].y = 0;
            } else {
                srcRect[i].x = node_group_info.leader.input.cropRegion[0];
                srcRect[i].y = node_group_info.leader.input.cropRegion[1];
            }
            srcRect[i].w = node_group_info.leader.input.cropRegion[2];
            srcRect[i].h = node_group_info.leader.input.cropRegion[3];

            EXYNOS_CAMERA_FUSION_PIPE_DEBUG_LOG("frame[%d] zoom(%d), input(%d, %d, %d, %d), output(%d, %d, %d, %d), srcRect(%d, %d, %d, %d  in %d x %d)",
                i,
                zoom,
                node_group_info.leader.input.cropRegion[0],
                node_group_info.leader.input.cropRegion[1],
                node_group_info.leader.input.cropRegion[2],
                node_group_info.leader.input.cropRegion[3],
                node_group_info.leader.output.cropRegion[0],
                node_group_info.leader.output.cropRegion[1],
                node_group_info.leader.output.cropRegion[2],
                node_group_info.leader.output.cropRegion[3],
                srcRect[i].x,
                srcRect[i].y,
                srcRect[i].w,
                srcRect[i].h,
                srcRect[i].fullW,
                srcRect[i].fullH);
        }

        // dof
        dof[i] = dualPreviewFrameSelector->getDOF(i);
        if (dof[i] == NULL) {
            CLOGE("dualPreviewFrameSelector->getDOF(%d) fail", i);
            goto func_exit;
        }

        frame[i]->getMetaData(&src_shot_ext[i]);

        ptr_src_shot_ext[i] = &src_shot_ext[i];
    }

    ///////////////////////////////////////
    // get destination information of all cameras
    ret = frame[m_cameraId]->getDstBuffer(getPipeId(), &dstBuffer);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d]->getSrcBuffer(%d) fail, ret(%d)", m_cameraId, getPipeId(), ret);
        goto func_exit;
    }

    ret = frame[m_cameraId]->getDstRect(getPipeId(), &dstRect);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d]->getDstRect(%d) fail, ret(%d)", m_cameraId, getPipeId(), ret);
        goto func_exit;
    }

    {
        m_fusionProcessTimer.start();

        ret = m_executeFusion(ptr_src_shot_ext, dof,
                              srcBuffer, srcRect, srcBufferManager,
                              dstBuffer, dstRect, dstBufferManager);

        m_fusionProcessTimer.stop();
        int fisionProcessTime = (int)m_fusionProcessTimer.durationUsecs();

        if (FUSION_PROCESSTIME_STANDARD < fisionProcessTime) {
            CLOGW("fisionProcessTime(%d) is too more slow than than %d usec",
                fisionProcessTime, FUSION_PROCESSTIME_STANDARD);
        }

        if (ret != NO_ERROR) {
            CLOGE("m_executeFusion() fail");
            goto func_exit;
        }
    }

    ///////////////////////////////////////
    // push frame to all cameras handlePreviewFrame

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_flagValidCameraId[i] == false)
            continue;

        if (frame[i] == NULL)
            continue;

        ret = frame[i]->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d]->setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", i, getPipeId());
            goto func_exit;
        }

        if (outputFrameQ[i]) {
            outputFrameQ[i]->pushProcessQ(&frame[i]);
        }
    }

    goto done;

func_exit:
    /*
     * if fusion operation is fail,
     * all frames's frame need to drop
     */
    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        if (m_flagValidCameraId[i] == false)
            continue;

        if (frame[i] == NULL)
            continue;

        ExynosCameraFrameEntity *entity = NULL;

        entity = frame[i]->searchEntityByPipeId(getPipeId());
        if (entity == NULL) {
            CLOGE("frame[%d]->searchEntityByPipeId(%d) == NULL, skip", i, frame[i]->getFrameCount());
            goto done;
        }

        ret = entity->setDstBufState(ENTITY_BUFFER_STATE_ERROR);
        if (ret != NO_ERROR) {
            CLOGE("setDstBufState(ENTITY_BUFFER_STATE_ERROR) fail");
        }

        ret = frame[i]->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d]->setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", i, getPipeId());
        }

        if (outputFrameQ[i])
            outputFrameQ[i]->pushProcessQ(&frame[i]);
    }

done:
    fusionMutex->unlock();

    return NO_ERROR;
}
#else
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d])invalid function, assert!!!!", __FUNCTION__, __LINE__);
}
#endif // USE_DUAL_CAMERA

bool ExynosCameraPipeFusion::m_flagReadyFusion(void)
{
    bool ret = false;

#ifdef USE_CP_FUSION_LIB
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapperCP>::getInstance();
#else
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapper>::getInstance();
#endif

    return fusionWrapper->flagReady(m_cameraId);
}

bool ExynosCameraPipeFusion::m_mainThreadFunc(void)
{
    int ret = 0;

    ret = m_run();
    if (ret < 0) {
        if (ret != TIMED_OUT)
            CLOGE("m_putBuffer fail");
    }

    return m_checkThreadLoop();
}

void ExynosCameraPipeFusion::m_init(__unused int32_t *nodeNums)
{
    /* TODO : we need to change here, when cameraId is changed */
    getDualCameraId(&m_mainCameraId, &m_subCameraId);

    CLOGD("m_mainCameraId(CAMERA_ID_%d), m_subCameraId(CAMERA_ID_%d)",
        m_mainCameraId, m_subCameraId);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_flagValidCameraId[i] = false;
    }

    m_flagValidCameraId[m_mainCameraId] = true;
    m_flagValidCameraId[m_subCameraId] = true;
}

status_t ExynosCameraPipeFusion::m_createFusion(void)
#ifdef USE_DUAL_CAMERA
{
    status_t ret = NO_ERROR;

#ifdef USE_CP_FUSION_LIB
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapperCP>::getInstance();
#else
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapper>::getInstance();
#endif

    /*
     * we will create front -> back,
     * for dczoom_init and dczoom_execute happen on same back camera thread
     */
    if (m_cameraId == m_mainCameraId) {
        if (fusionWrapper->flagCreate(m_subCameraId) == false) {
            CLOGE("CAMERA_ID_%d is not created, yet, so postpone create(CAMERA_ID_%d)",
                m_subCameraId, m_mainCameraId);
            return ret;
        }
    }

    if (fusionWrapper->flagCreate(m_cameraId) == false) {
        int previewW = 0, previewH = 0;
        m_parameters->getPreviewSize(&previewW, &previewH);

        ExynosRect fusionSrcRect;
        ExynosRect fusionDstRect;

        ret = m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);
        if (ret != NO_ERROR) {
            CLOGE("getFusionSize() fail");
            return ret;
        }

        char *calData  = NULL;
        int   calDataSize = 0;

#ifdef USE_CP_FUSION_LIB
        calData = m_parameters->getFusionCalData(&calDataSize);
        if (calData == NULL) {
            CLOGE("getFusionCalData() fail");
            return INVALID_OPERATION;
        }
#endif

        fusionWrapper->create(m_cameraId,
                              fusionSrcRect.fullW, fusionSrcRect.fullH,
                              fusionDstRect.fullW, fusionDstRect.fullH,
                              calData, calDataSize);
        if (ret != NO_ERROR) {
            CLOGE("fusionWrapper->create() fail");
            return ret;
        }
    }

    return ret;
}
#else
{
    android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d])invalid function, assert!!!!", __FUNCTION__, __LINE__);
}
#endif // USE_DUAL_CAMERA

status_t ExynosCameraPipeFusion::m_executeFusion(struct camera2_shot_ext *shot_ext[], DOF *dof[],
                                                 ExynosCameraBuffer srcBuffer[], ExynosRect srcRect[], ExynosCameraBufferManager *srcBufferManager[],
                                                 ExynosCameraBuffer dstBuffer,   ExynosRect dstRect,   ExynosCameraBufferManager *dstBufferManager)
{
#ifdef EXYNOS_CAMERA_FUSION_PIPE_DEBUG
    ExynosCameraAutoTimer autoTimer(__func__);
#endif

    status_t ret = NO_ERROR;

#ifdef USE_CP_FUSION_LIB
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapperCP>::getInstance();
#else
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapper>::getInstance();
#endif

    if (fusionWrapper->flagCreate(m_cameraId) == false) {
        CLOGE("usionWrapper->flagCreate(%d) == false. so fail", m_cameraId);
        return INVALID_OPERATION;
    }

    //////////////////////////////////////
    // trigger dczoom_execute
    ret = fusionWrapper->execute(m_cameraId,
                                 shot_ext, dof,
                                 srcBuffer, srcRect, srcBufferManager,
                                 dstBuffer, dstRect, dstBufferManager);
    if (ret != NO_ERROR) {
        CLOGE("fusionWrapper->excute() fail");
        return ret;
    }

    return ret;
}

status_t ExynosCameraPipeFusion::m_destroyFusion(void)
{
    status_t ret = NO_ERROR;

#ifdef USE_CP_FUSION_LIB
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapperCP>::getInstance();
#else
    ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionWrapper>::getInstance();
#endif

    if (fusionWrapper->flagCreate(m_cameraId) == true) {
        fusionWrapper->destroy(m_cameraId);
        if (ret != NO_ERROR) {
            CLOGE("fusionWrapper->destroy() fail");
            return ret;
        }
    }

    return ret;
}

}; /* namespace android */
