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
#include <log/log.h>

#include "ExynosCameraPipeFusion.h"

namespace android {

status_t ExynosCameraPipeFusion::create(int32_t *sensorIds)
{
    status_t ret = NO_ERROR;

    ret = m_createFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_createFusion() fail");
        return ret;
    }

    ret = ExynosCameraSWPipe::create(sensorIds);

    return ret;
}

status_t ExynosCameraPipeFusion::setFusionWrapper(ExynosCameraFusionWrapper *fusionWrapper)
{
    m_fusionWrapper = fusionWrapper;
    CLOGD("%p", m_fusionWrapper);
    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::setBokehWrapper(ExynosCameraFusionWrapper *bokehWrapper)
{
    m_bokehWrapper = bokehWrapper;
    CLOGD("%p", m_bokehWrapper);
    return NO_ERROR;
}

status_t ExynosCameraPipeFusion::m_destroy(void)
{
    CLOGD("");

    status_t ret;

    ret = m_destroyFusion();
    if (ret != NO_ERROR) {
        CLOGE("m_destroyFusion() fail");
    }

    m_fusionWrapper = NULL;
    m_bokehWrapper = NULL;

    ret = ExynosCameraSWPipe::m_destroy();
    if (ret != NO_ERROR) {
        CLOGE("destroy fail");
        return ret;
    }

    CLOGI("destroy() is succeed (%d)", getPipeId());

    return ret;
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
            enum DUAL_OPERATION_MODE dualOperationMode =
                m_configurations->getDualOperationMode();

            if (dualOperationMode == DUAL_OPERATION_MODE_SYNC) {
                CLOGW("wait timeout");
            }
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

    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
        if (m_fusionWrapper == NULL || m_fusionWrapper->flagCreate(m_cameraId) == false) {
            CLOGE("frame(%d, %d) fusionWrpper is invalid",
                    newFrame->getFrameCount(), newFrame->getFrameState());
            goto func_exit;
        }
    } else if (m_configurations->getScenario()== SCENARIO_DUAL_REAR_PORTRAIT) {
        if (m_bokehWrapper == NULL || m_bokehWrapper->flagCreate(m_cameraId) == false) {
            CLOGE("frame(%d, %d) bokehWrpper is invalid",
                    newFrame->getFrameCount(), newFrame->getFrameState());
            goto func_exit;
        }
    }

    /* do fusion */
    ret = m_manageFusion(newFrame);
    if (ret != NO_ERROR) {
        CLOGE("m_manageFusion(newFrame(%d)) fail", newFrame->getFrameCount());
        //goto func_exit;
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

    m_fusionWrapper = NULL;
    m_bokehWrapper = NULL;
    m_masterCameraId = -1;
    m_slaveCameraId = -1;
}

status_t ExynosCameraPipeFusion::m_createFusion(void)
{
    status_t ret = NO_ERROR;

    m_fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionZoomPreviewWrapper>::getInstance();
    m_bokehWrapper = ExynosCameraSingleton<ExynosCameraBokehPreviewWrapper>::getInstance();

    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
        if (m_fusionWrapper != NULL && m_fusionWrapper->flagCreate(m_cameraId) == false ) {
            int previewW = 0, previewH = 0;
            m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, 0);

            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;

#if 0
            ret = m_parameters->getFusionSize(previewW, previewH, &fusionSrcRect, &fusionDstRect);
            if (ret != NO_ERROR) {
                CLOGE("getFusionSize() fail");
                return ret;
            }
#endif

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
                                  previewW, previewH,
                                  previewW, previewH,
                                  calData, calDataSize);
            if (ret != NO_ERROR) {
                CLOGE("m_fusionWrapper->create() fail");
                return ret;
            }

            m_fusionWrapper->flagReady(true);
        }
    } else if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
        if (m_bokehWrapper != NULL && m_bokehWrapper->flagCreate(m_cameraId) == false) {
            int previewW = 0, previewH = 0;
            m_configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, 0);

            ExynosRect bokehSrcRect;
            ExynosRect bokehDstRect;

            char *calData  = NULL;
            int   calDataSize = 0;

#ifdef USE_CP_FUSION_LIB
            calData = m_parameters->getFusionCalData(&calDataSize);
            if (calData == NULL) {
                CLOGE("getFusionCalData() fail");
                return INVALID_OPERATION;
            }
#endif

            m_bokehWrapper->create(m_cameraId,
                                  previewW, previewH,
                                  previewW, previewH,
                                  calData, calDataSize);
            if (ret != NO_ERROR) {
                CLOGE("m_fusionWrapper->create() fail");
                return ret;
            }

            m_bokehWrapper->flagReady(true);
        }
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

    if (m_bokehWrapper != NULL && m_bokehWrapper->flagCreate(m_cameraId) == true) {
        m_bokehWrapper->destroy(m_cameraId);
        if (ret != NO_ERROR) {
            CLOGE("m_bokehWrapper->destroy() fail");
            return ret;
        }
    }

    return ret;
}

status_t ExynosCameraPipeFusion::m_manageFusion(ExynosCameraFrameSP_sptr_t newFrame)
{
    status_t ret = NO_ERROR;
    int cameraId = -1;
    //ExynosCameraParameters *params[2];
    ExynosRect srcImageRect[2];
    ExynosCameraBuffer srcBuffer[2];
    ExynosCameraBufferManager *srcBufferManager[2] = {NULL, };
    camera2_node_group node_group_info[2];
    struct camera2_shot_ext src_shot_ext[2];
    struct camera2_stream *shot_stream[2] = {0, };
    struct camera2_shot_ext *dst_shot_ext = NULL;
    struct camera2_stream *dst_shot_stream = NULL;

    ExynosRect dstImageRect;
    ExynosCameraBuffer dstBuffer;
    ExynosCameraBufferManager *dstBufferManager = NULL;
    enum DUAL_OPERATION_MODE dualOperationMode;
    uint32_t frameType = 0;
    uint32_t frameCount = 0;
    int sizeW = 0, sizeH = 0;
    int vraInputSizeWidth = 0, vraInputSizeHeight = 0;
    int32_t format = 0;

    /* dst */
    ret = newFrame->getDstBuffer(getPipeId(), &dstBuffer);
    if (ret != NO_ERROR) {
        CLOGE("frame[%d]->getDstBuffer(%d) fail, ret(%d)",
                newFrame->getFrameCount(), getPipeId(), ret);
        goto func_exit;
    }

    if (dstBuffer.index < 0) {
        CLOGE("Dst buffer Index(%d)", dstBuffer.index);
        goto func_exit;
    }

    /* get dst rect */
    // HACK : set dst info
    if (newFrame->getFrameType() == FRAME_TYPE_REPROCESSING_DUAL_MASTER) {
        m_configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&sizeW, (uint32_t *)&sizeH);
        format = V4L2_PIX_FMT_NV21;

        dstImageRect.x = 0;
        dstImageRect.y = 0;
        dstImageRect.w = sizeW;
        dstImageRect.h = sizeH;
        dstImageRect.fullW = sizeW;
        dstImageRect.fullH = sizeH;
        dstImageRect.colorFormat = format;
    } else {
        ret = newFrame->getDstRect(PIPE_FUSION, &dstImageRect);
        format = V4L2_PIX_FMT_NV21M;

        dstImageRect.colorFormat = format;
    }

    dst_shot_ext = (struct camera2_shot_ext *)(dstBuffer.addr[dstBuffer.getMetaPlaneIndex()]);
    dst_shot_stream = (struct camera2_stream *)(dstBuffer.addr[dstBuffer.getMetaPlaneIndex()]);

    /* src1, src2 */
    for (int i = OUTPUT_NODE_1; i <= OUTPUT_NODE_2; i++) {
        if ((newFrame->getFrameType() != FRAME_TYPE_PREVIEW_DUAL_MASTER &&
             newFrame->getFrameType() != FRAME_TYPE_REPROCESSING_DUAL_MASTER)
             && (i == OUTPUT_NODE_2))
            break;

        switch (newFrame->getFrameType()) {
        case FRAME_TYPE_PREVIEW:
            cameraId = m_masterCameraId;
            break;
        case FRAME_TYPE_PREVIEW_DUAL_MASTER:
        case FRAME_TYPE_REPROCESSING_DUAL_MASTER:
            if (i == OUTPUT_NODE_1)
                cameraId = m_masterCameraId;
            else
                cameraId = m_slaveCameraId;
            break;
        case FRAME_TYPE_PREVIEW_SLAVE:
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

        if (srcBuffer[i].index < 0) {
            if (i == OUTPUT_NODE_1) {
                CLOGE("src[%d] buffer Index(%d)", i, srcBuffer[i].index);
                goto func_exit;
            } else {
                continue;
            }
        }

        /* get src rect */
        shot_stream[i] = (struct camera2_stream *)(srcBuffer[i].addr[srcBuffer[i].getMetaPlaneIndex()]);

        srcImageRect[i].x = shot_stream[i]->output_crop_region[0];
        srcImageRect[i].y = shot_stream[i]->output_crop_region[1];
        srcImageRect[i].w = shot_stream[i]->output_crop_region[2];
        srcImageRect[i].h = shot_stream[i]->output_crop_region[3];
        srcImageRect[i].fullW = shot_stream[i]->output_crop_region[2];
        srcImageRect[i].fullH = shot_stream[i]->output_crop_region[3];
        srcImageRect[i].colorFormat = format;

        // is this need?
        switch (srcImageRect[i].colorFormat) {
        case V4L2_PIX_FMT_NV21:
            srcImageRect[i].fullH = ALIGN_UP(srcImageRect[i].fullH, 2);
            break;
        default:
            srcImageRect[i].fullH = ALIGN_UP(srcImageRect[i].fullH, GSCALER_IMG_ALIGN);
            break;
        }

        /* get the nodeGroupInfo */
        newFrame->getNodeGroupInfo(&node_group_info[i], PERFRAME_INFO_3AA, i);

        /* get the metadata */
        newFrame->getMetaData(&src_shot_ext[i], i);

        /* get VRA Information For Face Rect */
        if (m_parameters->getHwConnectionMode(PIPE_MCSC, PIPE_VRA) == HW_CONNECTION_MODE_M2M) {
#ifdef CAPTURE_FD_SYNC_WITH_PREVIEW
            m_parameters->getHwVraInputSize(&vraInputSizeWidth, &vraInputSizeHeight,
                                            m_parameters->getDsInputPortId(false));
#else
            m_parameters->getHwVraInputSize(&vraInputSizeWidth, &vraInputSizeHeight,
                                            src_shot_ext[i].shot.uctl.scalerUd.mcsc_sub_blk_port[INTERFACE_TYPE_DS]);
#endif
        } else {
            m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&vraInputSizeWidth, (uint32_t *)&vraInputSizeHeight, 0);
        }

        if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
            m_manageFusionDualRearZoom(newFrame, i, cameraId, vraInputSizeWidth, vraInputSizeHeight);
        } else if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
            m_manageFusionDualPortrait(newFrame, i, cameraId, vraInputSizeWidth, vraInputSizeHeight);
        }

#ifdef USE_DUAL_CAMERA_LOG_TRACE
        CLOGI("[%d] frame[T%d/F%d/MF%d/SF%d]"
               "src(%s, i%d, fd[%d]) dst(%s, i%d, fd[%d]),"
               "input(%d, %d, %d, %d),output(%d, %d, %d, %d), "
               "srcImageRect(%d, %d, %d, %d  in %d x %d), dstImageRect(%d, %d, %d, %d  in %d x %d) scenario(%d)",
                i,
                newFrame->getFrameType(),
                newFrame->getFrameCount(),
                newFrame->getMetaFrameCount(),
                shot_stream[i]->fcount,
                srcBuffer[i].bufMgrNm,
                srcBuffer[i].index,
                srcBuffer[i].fd[0],
                dstBuffer.bufMgrNm,
                dstBuffer.index,
                dstBuffer.fd[0],
                node_group_info[i].leader.input.cropRegion[0],
                node_group_info[i].leader.input.cropRegion[1],
                node_group_info[i].leader.input.cropRegion[2],
                node_group_info[i].leader.input.cropRegion[3],
                node_group_info[i].leader.output.cropRegion[0],
                node_group_info[i].leader.output.cropRegion[1],
                node_group_info[i].leader.output.cropRegion[2],
                node_group_info[i].leader.output.cropRegion[3],
                srcImageRect[i].x,
                srcImageRect[i].y,
                srcImageRect[i].w,
                srcImageRect[i].h,
                srcImageRect[i].fullW,
                srcImageRect[i].fullH,
                dstImageRect.x,
                dstImageRect.y,
                dstImageRect.w,
                dstImageRect.h,
                dstImageRect.fullW,
                dstImageRect.fullH,
                m_configurations->getScenario());
#endif
    }

    dst_shot_stream->output_crop_region[0] = 0;
    dst_shot_stream->output_crop_region[1] = 0;
    dst_shot_stream->output_crop_region[2] = dstImageRect.fullW;
    dst_shot_stream->output_crop_region[3] = dstImageRect.fullH;

#if 0
    dstBufferManager = m_bufferManager[CAPTURE_NODE];
    if (dstBufferManager == NULL) {
        CLOGE("frame(%d), dstBufferManager is NULL",
                newFrame->getFrameCount());
        goto func_exit;
    }
#endif

    if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
        /* do fusion */
        ret = m_fusionWrapper->execute(m_cameraId,
                src_shot_ext,
                srcBuffer, srcImageRect, srcBufferManager,
                dstBuffer, dstImageRect, dstBufferManager);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d] pipe(%d) fusionWrapper->excute(%d) fail",
                    newFrame->getFrameCount(), getPipeId(), ret);
            return ret;
        }
    } else if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
        /* do fusion */
        ret = m_bokehWrapper->execute(m_cameraId,
                src_shot_ext,
                srcBuffer, srcImageRect, srcBufferManager,
                dstBuffer, dstImageRect, dstBufferManager);
        if (ret != NO_ERROR) {
            CLOGE("frame[%d] pipe(%d) fusionWrapper->excute(%d) fail",
                    newFrame->getFrameCount(), getPipeId(), ret);
        }

    }

func_exit:
    return ret;
}

void ExynosCameraPipeFusion::m_manageFusionDualRearZoom(ExynosCameraFrameSP_sptr_t newFrame, int i, int cameraId, int vraInputSizeWidth, int vraInputSizeHeight)
{
    return;
}

void ExynosCameraPipeFusion::m_manageFusionDualPortrait(ExynosCameraFrameSP_sptr_t newFrame, int i, int cameraId, int vraInputSizeWidth, int vraInputSizeHeight)
{
    return;
}

}; /* namespace android */
