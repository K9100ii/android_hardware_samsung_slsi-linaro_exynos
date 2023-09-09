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
#define LOG_TAG "ExynosCameraPipeFusion"
#include <cutils/log.h>

#include "ExynosCameraPipeFusion.h"

namespace android {

ExynosCameraPipeFusion::~ExynosCameraPipeFusion()
{
    this->destroy();
}

status_t ExynosCameraPipeFusion::create(__unused int32_t *sensorIds)
{
    CLOGD("");

    status_t ret;

    ret = ExynosCameraSWPipe::create(sensorIds);
    if (ret < 0) {
        CLOGE("startThread fail");
        return ret;
    }

    CLOGI("create() is succeed (%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeFusion::destroy(void)
{
    CLOGD("");

    status_t ret;

    ret = ExynosCameraSWPipe::release();
    if (ret < 0) {
        CLOGE("release fail");
        return ret;
    }

    ret = m_destroyFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_destroyFusion() fail");
    }

    m_dualSelector = NULL;
    m_fusionWrapper = NULL;

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return ret;
}

status_t ExynosCameraPipeFusion::start(void)
{
    status_t ret = NO_ERROR;

    CLOGD("");

    m_flagTryStop = false;

    ret = m_createFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_createFusion() fail");
    }

    return ret;
}

status_t ExynosCameraPipeFusion::stop(void)
{
    CLOGD("");

    status_t ret = NO_ERROR;

    m_flagTryStop = true;

    m_mainThread->requestExitAndWait();

    CLOGD(" thead exited");

    m_inputFrameQ->release();

    m_flagTryStop = false;

    return ret;
}

void ExynosCameraPipeFusion::setDualSelector(ExynosCameraDualFrameSelector *dualSelector)
{
    CLOGD("");
    CLOGD("%p", m_dualSelector);

    status_t ret = NO_ERROR;

    m_dualSelector = dualSelector;
}

void ExynosCameraPipeFusion::setFusionWrapper(ExynosCameraFusionWrapper *fusionWrapper)
{
    CLOGD("");
    CLOGD("%p", m_fusionWrapper);

    status_t ret = NO_ERROR;

    m_fusionWrapper = fusionWrapper;
}

status_t ExynosCameraPipeFusion::m_run(void)
{
    status_t ret = 0;
    bool isSrc = true;
    bool needFusion = false;
    ExynosCameraFrameSP_sptr_t newFrame = NULL;
    ExynosCameraFrameEntity *entity = NULL;

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

    if (m_dualSelector == NULL) {
        CLOGE("m_dualSelector is NULL");
        return NO_ERROR;
    }

    /* check the frame state */
    switch (newFrame->getFrameState()) {
    case FRAME_STATE_SKIPPED:
    case FRAME_STATE_INVALID:
        CLOGE("frame(%d) state is invalid(%d)",
                newFrame->getFrameCount(), newFrame->getFrameState());
        goto func_exit;
        break;
    default:
        break;
    }

    if (m_fusionWrapper == NULL || m_fusionWrapper->flagCreate(m_cameraId) == false) {
        CLOGE("frame(%d, %d) fusionWrpper is invalid",
                newFrame->getFrameCount(), newFrame->getFrameState());
        goto func_exit;
    }

    /* do fusion */
    ret = m_manageFusion(newFrame);
    if (ret != NO_ERROR) {
        CLOGE("m_manageFusion(newFrame(%d)) fail", newFrame->getFrameCount());
        goto func_exit;
    }

    /* set the entity state to done */
    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret < 0) {
        CLOGE("newFrame(%d)->setEntityState(pipeId(%d), ENTITY_STATE_FRAME_DONE), ret(%d)",
                newFrame->getFrameCount(), getPipeId(), ret);
        goto func_exit;
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;

func_exit:
    /* comptle the frame to remove the processList */
    newFrame->setFrameState(FRAME_STATE_SKIPPED);

    ret = newFrame->setEntityState(getPipeId(), ENTITY_STATE_FRAME_DONE);
    if (ret != NO_ERROR) {
        CLOGE("setEntityState(%d, ENTITY_STATE_FRAME_DONE) fail", getPipeId());
    }

    m_outputFrameQ->pushProcessQ(&newFrame);

    return NO_ERROR;
}

void ExynosCameraPipeFusion::m_init(void)
{
    CLOGD("");

    m_dualSelector = NULL;
    m_fusionWrapper = NULL;
    m_masterCameraId = -1;
    m_slaveCameraId = -1;
}

status_t ExynosCameraPipeFusion::m_createFusion(void)
{
    status_t ret = NO_ERROR;

    if (m_fusionWrapper != NULL && m_fusionWrapper->flagCreate(m_cameraId) == false) {
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

        m_fusionWrapper->create(m_cameraId,
                              fusionSrcRect.fullW, fusionSrcRect.fullH,
                              fusionDstRect.fullW, fusionDstRect.fullH,
                              calData, calDataSize);
        if (ret != NO_ERROR) {
            CLOGE("m_fusionWrapper->create() fail");
            return ret;
        }

        m_fusionWrapper->flagReady(true);
    }

    return ret;
}

status_t ExynosCameraPipeFusion::m_destroyFusion(void)
{
    status_t ret = NO_ERROR;

    if (m_fusionWrapper != NULL && m_fusionWrapper->flagCreate(m_cameraId) == true) {
        m_fusionWrapper->destroy(m_cameraId);
        if (ret != NO_ERROR) {
            CLOGE("m_fusionWrapper->destroy() fail");
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraPipeFusion::m_manageFusion(ExynosCameraFrameSP_sptr_t newFrame)
{
    status_t ret = NO_ERROR;
    int cameraId = -1;
    DOF *dof[2];
    ExynosCameraParameters *params[2];
    ExynosRect srcRect[2];
    ExynosCameraBuffer srcBuffer[2];
    ExynosCameraBufferManager *srcBufferManager[2];
    camera2_node_group node_group_info[2];
    struct camera2_shot_ext src_shot_ext[2];

    ExynosRect dstRect;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraBufferManager *dstBufferManager;

    /* dst */
    ret = newFrame->getDstBuffer(getPipeId(), &dstBuffer);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d]->getDstBuffer(%d) fail, ret(%d)",
                newFrame->getFrameCount(), getPipeId(), ret);
        goto func_exit;
    }

    ret = newFrame->getDstRect(getPipeId(), &dstRect);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d]->getDstRect(%d) fail, ret(%d)",
                newFrame->getFrameCount(), getPipeId(), ret);
        goto func_exit;
    }

    dstBufferManager = m_bufferManager[CAPTURE_NODE];
    if (dstBufferManager == NULL) {
        CLOGE("frame(%d), dstBufferManager is NULL",
                newFrame->getFrameCount());
        goto func_exit;
    }

    /* src1, src2 */
    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if ((newFrame->getSyncType() != SYNC_TYPE_SYNC) &&
                (i == OUTPUT_NODE_2))
            break;

        switch (newFrame->getSyncType()) {
        case SYNC_TYPE_BYPASS:
            cameraId = m_masterCameraId;
            break;
        case SYNC_TYPE_SYNC:
            if (i == OUTPUT_NODE_1)
                cameraId = m_masterCameraId;
            else
                cameraId = m_slaveCameraId;
            break;
        case SYNC_TYPE_SWITCH:
            cameraId = m_slaveCameraId;
            break;
        default:
            cameraId = m_masterCameraId;
            break;
        }

        /* get buffer */
        ret = newFrame->getSrcBuffer(getPipeId(), &srcBuffer[i], i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getSrcBuffer1(%d) fail, ret(%d)",
                    i, newFrame->getFrameCount(), getPipeId(), ret);
            goto func_exit;
        }

        /* get src rect */
        ret = newFrame->getSrcRect(getPipeId(), &srcRect[i], i);
        if (ret != NO_ERROR) {
            CLOGE("[%d] frame[%d]->getSrcRect(%d) fail, ret(%d)",
                    i, newFrame->getFrameCount(), getPipeId(), ret);
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

        /* get the dual selector to find source buffer manager/param */
        ret = m_dualSelector->getBufferManager(cameraId, &srcBufferManager[i]);
        if (ret != NO_ERROR) {
            CLOGE("[cameraId%d] frame(%d) m_dualSelector->getBufferManager() fail, ret(%d)",
                    cameraId, newFrame->getFrameCount(), ret);
            goto func_exit;
        }

        ret = m_dualSelector->getParameter(cameraId, &params[i]);
        if (ret != NO_ERROR) {
            CLOGE("[cameraId%d] frame(%d) m_dualSelector->getBufferManager() fail, ret(%d)",
                    cameraId, newFrame->getFrameCount(), ret);
            goto func_exit;
        } else {
            dof[i] = params[i]->getDOF();
        }

        /* get the nodeGroupInfo */
        newFrame->getNodeGroupInfo(&node_group_info[i], PERFRAME_INFO_3AA, i);

        /* get the metadata */
        newFrame->getMetaData(&src_shot_ext[i], i);

#if 0 /* Check wide/tele face data */
        if (i == OUTPUT_NODE_1) {
            CLOGE("node 1 faceDetectMode(%d)", src_shot_ext[i].shot.dm.stats.faceDetectMode);
            CLOGE("[%d %d]", src_shot_ext[i].shot.dm.stats.faceRectangles[0][0], src_shot_ext[i].shot.dm.stats.faceRectangles[0][1]);
            CLOGE("[%d %d]", src_shot_ext[i].shot.dm.stats.faceRectangles[0][2], src_shot_ext[i].shot.dm.stats.faceRectangles[0][3]);
        } else if (i == OUTPUT_NODE_2) {
            CLOGE("node 2 faceDetectMode(%d)", src_shot_ext[i].shot.dm.stats.faceDetectMode);
            CLOGE("[%d %d]", src_shot_ext[i].shot.dm.stats.faceRectangles[0][0], src_shot_ext[i].shot.dm.stats.faceRectangles[0][1]);
            CLOGE("[%d %d]", src_shot_ext[i].shot.dm.stats.faceRectangles[0][2], src_shot_ext[i].shot.dm.stats.faceRectangles[0][3]);
        }
#endif

#ifdef SAMSUNG_DUAL_SOLUTION
        m_fusionWrapper->m_setCurZoomRatio(cameraId, m_parameters->getZoomRatio(cameraId, newFrame->getZoom(i)));
        m_fusionWrapper->m_setCurViewRatio(cameraId, m_parameters->getZoomOrgRatio(newFrame->getZoom(i)));
        CLOGV("[Fusion] cameraId: %d, zoomRatio: %f, viewRatio: %f", cameraId,
                    m_parameters->getZoomRatio(cameraId, newFrame->getZoom(i)), m_parameters->getZoomOrgRatio(newFrame->getZoom(i)));
        m_fusionWrapper->m_setSyncType(newFrame->getSyncType());

        m_checkForceWideCond(cameraId, newFrame->getSyncType());
        m_fusionWrapper->m_setForceWide();
#endif

#ifdef USE_DUAL_CAMERA_LOG_TRACE
        CLOGI("[%d] frame[%d:%d/%d] zoom(%d), index:%d, input(%d, %d, %d, %d), output(%d, %d, %d, %d), srcRect(%d, %d, %d, %d  in %d x %d), dstRect(%d, %d, %d, %d  in %d x %d)",
                i,
                newFrame->getSyncType(),
                newFrame->getFrameCount(),
                newFrame->getMetaFrameCount(),
                newFrame->getZoom(i),
                srcBuffer[i].index,
                node_group_info[i].leader.input.cropRegion[0],
                node_group_info[i].leader.input.cropRegion[1],
                node_group_info[i].leader.input.cropRegion[2],
                node_group_info[i].leader.input.cropRegion[3],
                node_group_info[i].leader.output.cropRegion[0],
                node_group_info[i].leader.output.cropRegion[1],
                node_group_info[i].leader.output.cropRegion[2],
                node_group_info[i].leader.output.cropRegion[3],
                srcRect[i].x,
                srcRect[i].y,
                srcRect[i].w,
                srcRect[i].h,
                srcRect[i].fullW,
                srcRect[i].fullH,
                dstRect.x,
                dstRect.y,
                dstRect.w,
                dstRect.h,
                dstRect.fullW,
                dstRect.fullH);
#endif
    }

    /* do fusion */
    ret = m_fusionWrapper->execute(m_cameraId, newFrame->getSyncType(),
            src_shot_ext, dof,
            srcBuffer, srcRect, srcBufferManager,
            dstBuffer, dstRect, dstBufferManager);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d] pipe(%d) fusionWrapper->excute(%d) fail",
                newFrame->getFrameCount(), getPipeId(), ret);
        return ret;
    }

    /* cache flush for dst in master */
    if (params[0] != NULL && params[0]->getIonClient() >= 0 &&
            params[0]->getEffectHint() == true) {
        for (int plane = 0; plane < dstBuffer.getMetaPlaneIndex(); plane++)
            ion_sync_fd(params[0]->getIonClient(), dstBuffer.fd[plane]);
    }

func_exit:

    return ret;
}

#ifdef SAMSUNG_DUAL_SOLUTION
void ExynosCameraPipeFusion::m_checkForceWideCond(int32_t cameraId, sync_type_t syncType)
{
    /*
       (FALLBACK Scenario)
       LLS Lux: T->W = 4.3 * 256
                W->T = 5.3 * 256
       objectDistance: T->W = TBD
                       W->T = TBD
     */
    if (m_parameters->getForceWide()) {
        if (m_fusionWrapper->m_getForceWideMode() == false) {
            /* forcely set to wide : enable FALLBACK flag(0x100)*/
            if (syncType != SYNC_TYPE_SWITCH) {
                m_fusionWrapper->m_setForceWideMode(true);
                CLOGD("[ForceWide] forceWide = %d", m_fusionWrapper->m_getForceWideMode());
            }
        }
    } else {
        if (m_fusionWrapper->m_getForceWideMode() == true) {
            /* get back to normal mode : disable FALLBACK flag */
            if (syncType != SYNC_TYPE_BYPASS) {
                m_fusionWrapper->m_setForceWideMode(false);
                CLOGD("[ForceWide] forceWide = %d", m_fusionWrapper->m_getForceWideMode());
            }
        }
    }
}
#endif
}; /* namespace android */
