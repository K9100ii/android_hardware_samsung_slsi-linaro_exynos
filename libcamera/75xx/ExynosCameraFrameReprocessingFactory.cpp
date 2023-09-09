/*
**
** Copyright 2014, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraFrameReprocessingFactory"
#include <cutils/log.h>

#include "ExynosCameraFrameReprocessingFactory.h"

namespace android {

ExynosCameraFrameReprocessingFactory::~ExynosCameraFrameReprocessingFactory()
{
    int ret = 0;

    ret = destroy();
    if (ret < 0)
        CLOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);

    m_setCreate(false);
}

status_t ExynosCameraFrameReprocessingFactory::create(bool active)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    m_setupConfig();

    int ret = 0;
    int32_t nodeNums[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++)
        nodeNums[i] = -1;

    m_pipes[INDEX(PIPE_3AA_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, true, &m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)]);

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[PIPE_3AA_REPROCESSING].pipeId[i], ret);
            return ret;
        }
    }

    m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setPipeName("PIPE_3AA_REPROCESSING");

    m_pipes[INDEX(PIPE_3AC_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipe3AC(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_3AC_REPROCESSING)]);
    m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->setPipeId(PIPE_3AC_REPROCESSING);
    m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->setPipeName("PIPE_3AC_REPROCESSING");

    m_pipes[INDEX(PIPE_ISP_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, true, &m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)]);

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].pipeId[i], ret);
            return ret;
        }
    }

    m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setPipeName("PIPE_ISP_REPROCESSING");

    m_pipes[INDEX(PIPE_GSC_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_GSC_REPROCESSING)]);
    m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->setPipeId(PIPE_GSC_REPROCESSING);
    m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->setPipeName("PIPE_GSC_REPROCESSING");

    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, true, m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)]);
    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->setPipeId(PIPE_JPEG_REPROCESSING);
    m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->setPipeName("PIPE_JPEG_REPROCESSING");

    CLOGD("DEBUG(%s[%d]):pipe ids for reprocessing", __FUNCTION__, __LINE__);

    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGD("DEBUG(%s[%d]): -> m_pipes[%d] : PipeId(%d)", __FUNCTION__, __LINE__ , i, m_pipes[i]->getPipeId());
        }
    }

    /* PIPE_ISP_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->create();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_ISP_REPROCESSING));

    /* PIPE_3AA_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->create();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):3AA create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_3AA_REPROCESSING));

#if 1 /* reprocess does not need 3AC : HACK firmware ISSUE*/
    /* PIPE_3AC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->create();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):3AC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_3AC_REPROCESSING));
#endif

    /* PIPE_GSC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->create();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_GSC_REPROCESSING));

    /* PIPE_JPEG_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->create();
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* EOS */
    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):%s V4L2_CID_IS_END_OF_STREAM fail, ret(%d)", __FUNCTION__, __LINE__, m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->getPipeName(), ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_setCreate(true);

    CLOGD("DEBUG(%s):Pipe(%d) created", __FUNCTION__, INDEX(PIPE_JPEG_REPROCESSING));

    return NO_ERROR;
}

ExynosCameraFrame * ExynosCameraFrameReprocessingFactory::createNewFrame(void)
{
    int ret = 0;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrame *frame =  m_frameMgr->createFrame(m_parameters, m_frameCount, FRAME_TYPE_REPROCESSING);
    int requestEntityCount = 0;
    int curShotMode = 0;
    int curSeriesShotMode = 0;
    if (m_parameters != NULL) {
        curShotMode = m_parameters->getShotMode();
        curSeriesShotMode = m_parameters->getSeriesShotMode();
    }

    ret = m_initFrameMetadata(frame);
    if (ret < 0)
        CLOGE("(%s[%d]): frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* set 3AA pipe to linkageList */
        newEntity[INDEX(PIPE_3AA_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_3AA_REPROCESSING, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_3AA_REPROCESSING)]);

        /* set ISP pipe to linkageList */
        if (m_flag3aaIspOTF == true) {
            newEntity[INDEX(PIPE_ISP_REPROCESSING)] = NULL;
        } else {
            newEntity[INDEX(PIPE_ISP_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_ISP_REPROCESSING, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addChildEntity(newEntity[INDEX(PIPE_3AA_REPROCESSING)], newEntity[INDEX(PIPE_ISP_REPROCESSING)], INDEX(PIPE_3AP_REPROCESSING));
        }

        requestEntityCount++;
    } else {
        /* set 3AA pipe to linkageList */
        newEntity[INDEX(PIPE_3AA_REPROCESSING)] = NULL;

        /* set ISP pipe to linkageList */
        if (m_flag3aaIspOTF == true) {
            android_printAssert(NULL, LOG_TAG, "(%s[%d]In case of dirty bayer, we cannot use 3aa-isp otf feature. please check XXX_3AA_ISP_OTF_REPROCESSING, assert!!!!",
                __FUNCTION__, __LINE__);
        } else {
            newEntity[INDEX(PIPE_ISP_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_ISP_REPROCESSING, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_ISP_REPROCESSING)]);
        }

        requestEntityCount++;
    }

    /* set GSC pipe to linkageList */
    newEntity[INDEX(PIPE_GSC_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_GSC_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_GSC_REPROCESSING)]);
    if (needGSCForCapture(m_cameraId) == true) {
        requestEntityCount++;
    }

    /* set JPEG pipe to linkageList */
    newEntity[INDEX(PIPE_JPEG_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_JPEG_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_JPEG_REPROCESSING)]);
    if (!(curShotMode == SHOT_MODE_RICH_TONE ||
          curSeriesShotMode == SERIES_SHOT_MODE_LLS ||
          curSeriesShotMode == SERIES_SHOT_MODE_SIS ||
          m_parameters->getShotMode() == SHOT_MODE_FRONT_PANORAMA) &&
         m_parameters->getHighResolutionCallbackMode() == false) {
        requestEntityCount++;
    }

    ret = m_initPipelines(frame);
    if (ret < 0) {
        CLOGE("ERR(%s):m_initPipelines fail, ret(%d)", __FUNCTION__, ret);
    }

    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactory::startInitialThreads(void)
{
    int ret = 0;

    CLOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

enum NODE_TYPE ExynosCameraFrameReprocessingFactory::getNodeType(uint32_t pipeId)
{
    enum NODE_TYPE nodeType = INVALID_NODE;
    if (m_flag3aaIspOTF == false) {
        switch (pipeId) {
        case PIPE_FLITE_REPROCESSING:
            nodeType = CAPTURE_NODE;
            break;
        case PIPE_3AA_REPROCESSING:
            nodeType = OUTPUT_NODE;
            break;
        case PIPE_3AP_REPROCESSING:
            nodeType = CAPTURE_NODE;
            break;
        case PIPE_ISP_REPROCESSING:
            nodeType = OUTPUT_NODE;
            break;
        case PIPE_ISPC_REPROCESSING:
            nodeType = CAPTURE_NODE;
            break;
        case PIPE_SCC_REPROCESSING:
            nodeType = CAPTURE_NODE;
            break;
        default:
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
                __FUNCTION__, __LINE__, pipeId);
            break;
        }
    } else {
        switch (pipeId) {
        case PIPE_FLITE_REPROCESSING:
            nodeType = CAPTURE_NODE;
            break;
        case PIPE_3AA_REPROCESSING:
            nodeType = OUTPUT_NODE;
            break;
        case PIPE_ISPC_REPROCESSING:
            nodeType = CAPTURE_NODE_1;
            break;
        case PIPE_3AP_REPROCESSING:
            nodeType = CAPTURE_NODE_2;
            break;
        case PIPE_ISP_REPROCESSING:
            nodeType = OTF_NODE_1;
            break;
        case PIPE_SCC_REPROCESSING:
            nodeType = CAPTURE_NODE_4;
            break;
        default:
            android_printAssert(NULL, LOG_TAG, "ASSERT(%s[%d]):Unexpected pipe_id(%d), assert!!!!",
                __FUNCTION__, __LINE__, pipeId);
            break;
        }
    }

    return nodeType;
};

status_t ExynosCameraFrameReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_3aa, node_group_info_isp;
    int zoom = m_parameters->getZoomLevel();
    int pictureW = 0, pictureH = 0;
    ExynosRect bnsSize;
    ExynosRect bayerCropSizePicture;
    ExynosRect bayerCropSizePreview;
    ExynosRect bdsSize;

#ifdef SR_CAPTURE
    if(m_parameters->getSROn()) {
        zoom = ZOOM_LEVEL_0;
    }
#endif

    m_parameters->getPictureSize(&pictureW, &pictureH);
    m_parameters->getPictureBayerCropSize(&bnsSize, &bayerCropSizePicture);
    m_parameters->getPictureBdsSize(&bdsSize);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSizePreview);

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* should add this request value in FrameFactory */
        node_group_info_3aa.leader.request = 1;

        /* should add this request value in FrameFactory */
        if (m_flag3aaIspOTF == true) {
            node_group_info_3aa.capture[PERFRAME_REPROCESSING_SCC_POS].request = 1;
        } else {
            node_group_info_3aa.capture[PERFRAME_REPROCESSING_3AP_POS].request = 1;
        }
    }

    /* should add this request value in FrameFactory */
    node_group_info_isp.leader.request = 1;
    node_group_info_isp.capture[PERFRAME_REPROCESSING_SCC_POS].request = 1;

    updateNodeGroupInfoReprocessing(
            m_cameraId,
            &node_group_info_3aa,
            &node_group_info_isp,
            bayerCropSizePreview,
            bayerCropSizePicture,
            bdsSize,
            pictureW, pictureH,
            m_parameters->getUsePureBayerReprocessing(),
            m_flag3aaIspOTF);

    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA, zoom);

    if (m_parameters->getUsePureBayerReprocessing() == true)
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP, zoom);
    else
        frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_DIRTY_REPROCESSING_ISP, zoom);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int ret = 0;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    int hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int bayerFormat = CAMERA_BAYER_FORMAT;
    int previewFormat = m_parameters->getHwPreviewFormat();
    int pictureFormat = m_parameters->getPictureFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();

#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getHwSensorSize(&hwSensorW, &hwSensorH);
    m_parameters->getMaxPreviewSize(&maxPreviewW, &maxPreviewH);
    m_parameters->getHwPreviewSize(&hwPreviewW, &hwPreviewH);
    m_parameters->getMaxPictureSize(&maxPictureW, &maxPictureH);
    m_parameters->getHwPictureSize(&hwPictureW, &hwPictureH);

    CLOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI("INFO(%s[%d]): MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", __FUNCTION__, __LINE__, maxPictureW, maxPictureH, hwPictureW, hwPictureH);

    /* 3AA pipe */
    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].pipeId[i]);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_3AA_REPROCESSING)][i];
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* 3AS */
        enum NODE_TYPE t3asNodeType = getNodeType(PIPE_3AA_REPROCESSING);

        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3asNodeType].rectInfo = tempRect;
        pipeInfo[t3asNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[t3asNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3asNodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

        /* per frame info */
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_PURE_REPROCESSING_3AA;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].nodeNum[t3asNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        if (m_flag3aaIspOTF == true) {
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 3; /* 3AA, 3AP, SCC or ISPC*/

            /* 3AP */
            enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP_REPROCESSING);
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_3AP_POS].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_3AP_POS].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].nodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);

            /* SCC or ISPC */
            enum NODE_TYPE ispcNodeType = getNodeType(PIPE_ISPC_REPROCESSING);

            int nodeNum = -1;
            if (isOwnScc(m_cameraId) == true) {
                nodeNum = m_nodeNums[INDEX(PIPE_SCC_REPROCESSING)][CAPTURE_NODE];
            } else {
                nodeNum = m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].nodeNum[ispcNodeType];
            }

            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameVideoID = (nodeNum - FIMC_IS_VIDEO_BAS_NUM);
        } else {
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 2; /* 3AA, 3AP */

            /* 3AP */
            enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP_REPROCESSING);
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_3AP_POS].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
            pipeInfo[t3asNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_3AP_POS].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].nodeNum[t3apNodeType] - FIMC_IS_VIDEO_BAS_NUM);
        }

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 16) * 2;
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3asNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW, 10) * 8 / 5;
        }
#endif

        enum NODE_TYPE t3apNodeType = getNodeType(PIPE_3AP_REPROCESSING);
        /* 3AP capture */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[t3apNodeType].rectInfo = tempRect;
        pipeInfo[t3apNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[t3apNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[t3apNodeType].bufInfo.count = NUM_BAYER_BUFFERS;
        /* per frame info */
        pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[t3apNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
        if (m_parameters->checkBayerDumpEnable()) {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[1].rectInfo.fullW * 2, 16);
        }
        else
#endif
        {
            /* packed bayer bytesPerPlane */
            pipeInfo[t3apNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[1].rectInfo.fullW * 3 / 2, 16);
        }
#endif

        if (m_flag3aaIspOTF == true) {
            /* ISPS capture */
            enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP_REPROCESSING);

            tempRect.fullW = hwPictureW;
            tempRect.fullH = hwPictureH;
            tempRect.colorFormat = bayerFormat;

            pipeInfo[ispsNodeType].rectInfo = tempRect;
            pipeInfo[ispsNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            pipeInfo[ispsNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
            pipeInfo[ispsNodeType].bufInfo.count = NUM_BAYER_BUFFERS;

            /* per frame info */
			pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
			pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

            /* ISPC capture */
            enum NODE_TYPE ispcNodeType = getNodeType(PIPE_ISPC_REPROCESSING);

            tempRect.fullW = hwPictureW;
            tempRect.fullH = hwPictureH;
            tempRect.colorFormat = pictureFormat;

            pipeInfo[ispcNodeType].rectInfo = tempRect;
            pipeInfo[ispcNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
            pipeInfo[ispcNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
            pipeInfo[ispcNodeType].bufInfo.count = NUM_BAYER_BUFFERS;
            /* per frame info */
            pipeInfo[ispcNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
            pipeInfo[ispcNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
#if 0
#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
            if (m_parameters->checkBayerDumpEnable()) {
                /* packed bayer bytesPerPlane */
                pipeInfo[ispcNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[2].rectInfo.fullW * 2, 16);
            }
            else
#endif
            {
                /* packed bayer bytesPerPlane */
                pipeInfo[ispcNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[2].rectInfo.fullW * 3 / 2, 16);
            }
#endif
#endif
        }

        sensorIds[OUTPUT_NODE] = setLeader(sensorIds[OUTPUT_NODE], true);

    } else {
        /*
         * this open another 3A video node for dirty bayer.
         * So, set reprocessing's src is null.
         */

        int tempSensorIds = sensorIds[OUTPUT_NODE];
        sensorIds[OUTPUT_NODE] = setSrcNodeEmpty(tempSensorIds);

        /* dirty bayer case, ISP is leader, (3AA is not leader) */
        tempSensorIds = sensorIds[OUTPUT_NODE];
        sensorIds[OUTPUT_NODE] = setLeader(tempSensorIds, false);
    }

    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setupPipe(pipeInfo, sensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* ISP pipe */
    enum NODE_TYPE ispsNodeType = getNodeType(PIPE_ISP_REPROCESSING);
    enum NODE_TYPE ispcNodeType = getNodeType(PIPE_ISPC_REPROCESSING);

    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = bayerFormat;

    pipeInfo[ispsNodeType].rectInfo = tempRect;
    pipeInfo[ispsNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    pipeInfo[ispsNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[ispsNodeType].bufInfo.count = NUM_BAYER_BUFFERS;

    /* per frame info */
    if (m_flag3aaIspOTF == true) {
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;
    } else {
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = CAPTURE_NODE_MAX; /* ISP, SCC*/

        if (m_parameters->getUsePureBayerReprocessing() == true)
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_PURE_REPROCESSING_ISP;
        else
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perframeInfoIndex = PERFRAME_INFO_DIRTY_REPROCESSING_ISP;

        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_LEADER;
        pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].nodeNum[ispsNodeType] - FIMC_IS_VIDEO_BAS_NUM);

        if (m_supportSCC == true) {
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameVideoID = (m_nodeNums[INDEX(PIPE_SCC_REPROCESSING)][CAPTURE_NODE] - FIMC_IS_VIDEO_BAS_NUM);
        } else {
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameNodeType = PERFRAME_NODE_TYPE_CAPTURE;
            pipeInfo[ispsNodeType].perFrameNodeGroupInfo.perFrameCaptureInfo[PERFRAME_REPROCESSING_SCC_POS].perFrameVideoID = (m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].nodeNum[ispcNodeType] - FIMC_IS_VIDEO_BAS_NUM);
        }
    }

#ifdef CAMERA_PACKED_BAYER_ENABLE
#ifdef DEBUG_RAWDUMP
    if (m_parameters->checkBayerDumpEnable()) {
        /* packed bayer bytesPerPlane */
        pipeInfo[ispsNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW * 2, 16);
    }
    else
#endif
    {
        /* packed bayer bytesPerPlane */
        pipeInfo[ispsNodeType].bytesPerPlane[0] = ROUND_UP(pipeInfo[0].rectInfo.fullW * 3 / 2, 16);
    }
#endif

    for (int i = 0; i < MAX_NODE; i++) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setPipeId((enum NODE_TYPE)i, m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].pipeId[i]);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):setPipeId(%d, %d) fail, ret(%d)", __FUNCTION__, __LINE__, i, m_deviceInfo[INDEX(PIPE_ISP_REPROCESSING)].pipeId[i]);
            return ret;
        }

        sensorIds[i] = m_sensorIds[INDEX(PIPE_ISP_REPROCESSING)][i];
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        sensorIds[ispsNodeType] = setLeader(sensorIds[ispsNodeType], false);
    } else {
        if (m_flag3aaIspOTF == true) {
            android_printAssert(NULL, LOG_TAG, "(%s[%d]In case of dirty bayer, we cannot use 3aa-isp otf feature. please check XXX_3AA_ISP_OTF_REPROCESSING, assert!!!!",
                __FUNCTION__, __LINE__);
        } else {
            sensorIds[ispsNodeType] = setLeader(sensorIds[ispsNodeType], true);
        }
    }

#if 0 /* reprocess does not need 3AC */
    /* 3AC output pipe */
    if (m_parameters->getUsePureBayerReprocessing() == true) {
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = bayerFormat;

        pipeInfo[0].rectInfo = tempRect;
        pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
        pipeInfo[0].bufInfo.count = config->current->bufInfo.num_3aa_buffers;
        /* per frame info */
        pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
        pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

        ret = m_pipes[INDEX(PIPE_3AC_REPROCESSING)]->setupPipe(pipeInfo);
        if (ret < 0) {
            CLOGE("ERR(%s[%d]): 3AC setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }
#endif
    /* SCC output pipe */
    if (m_supportSCC == true) {
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    pipeInfo[ispcNodeType].rectInfo = tempRect;
    pipeInfo[ispcNodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[ispcNodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[ispcNodeType].bufInfo.count = NUM_PICTURE_BUFFERS;
    /* per frame info */
    pipeInfo[ispcNodeType].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[ispcNodeType].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setupPipe(pipeInfo, sensorIds, secondarySensorIds);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

#if 0 /* disable after driver fix. (140520) */

    /* SCP pipe */
    /* TODO: Why we need setup pipe for reprocessing SCP pipe
     *       Need to remove after driver fix
     */
    for (int i = 0; i < MAX_NODE; i++)
        pipeInfo[i] = nullPipeInfo;

    hwPreviewW = m_parameters->getHwPreviewStride();
    CLOGV("INFO(%s[%d]):stride=%d", __FUNCTION__, __LINE__, hwPreviewW);
    tempRect.fullW = hwPreviewW;
    tempRect.fullH = hwPreviewH;
    tempRect.colorFormat = previewFormat;

    pipeInfo[0].rectInfo = tempRect;
    pipeInfo[0].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    pipeInfo[0].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    pipeInfo[0].bufInfo.count = m_parameters->getPreviewBufferCount();
    /* per frame info */
    pipeInfo[0].perFrameNodeGroupInfo.perframeSupportNodeNum = 0;
    pipeInfo[0].perFrameNodeGroupInfo.perFrameLeaderInfo.perFrameNodeType = PERFRAME_NODE_TYPE_NONE;

    ret = m_pipes[INDEX(PIPE_SCP_REPROCESSING)]->setupPipe(pipeInfo);
    if (ret < 0) {
        CLOGE("ERR(%s[%d]):SCP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::preparePipes(void)
{
    int ret = 0;

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->prepare();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::startPipes(void)
{
    int ret = 0;
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* ISP Reprocessing */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        /* 3AA Reprocessing */
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("INFO(%s[%d]):Starting Reprocessing [SCC>ISP] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::stopPipes(void)
{
    int ret = 0;
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    enum pipeline pipe;

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stopThread();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stopThread();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stop();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stop();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("INFO(%s[%d]):Stopping Reprocessing [ISP>SCC] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::setStopFlag(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    int ret = 0;

    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setStopFlag();

    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setStopFlag();

    return NO_ERROR;
}

void ExynosCameraFrameReprocessingFactory::m_init(void)
{
    m_requestFLITE = 0;

    m_request3AP = 1;
    m_request3AC = 0;
    m_requestISP = 1;

    m_requestISPC = 0;
    m_requestSCC = 0;

    if (m_supportSCC == true)
        m_requestSCC = 1;
    else
        m_requestISPC = 1;

    m_requestDIS = 0;
    m_requestSCP = 0;

    m_supportReprocessing = false;
    m_flagFlite3aaOTF = false;
    m_supportSCC = false;
    m_supportPureBayerReprocessing = false;
    m_flagReprocessing = false;

}

status_t ExynosCameraFrameReprocessingFactory::m_setupConfig(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int32_t *nodeNums = NULL;
    int32_t *controlId = NULL;
    int32_t *secondaryControlId = NULL;
    int32_t *prevNode = NULL;
    int nodeType = -1;
    int pipeId = -1;

    int t3aaNums[MAX_NODE];
    int ispNums[MAX_NODE];

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->isReprocessing3aaIspOTF();
    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportSCC = m_supportSCC;
    m_supportPureBayerReprocessing = USE_PURE_BAYER_REPROCESSING;
    m_flagReprocessing = true;

    if (m_flag3aaIspOTF == true) {
        m_requestISP = 0;
        m_request3AP = 0;
    }

    nodeNums = m_nodeNums[INDEX(PIPE_FLITE)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = MAIN_CAMERA_FLITE_NUM;
    nodeNums[SUB_NODE] = -1;
    controlId = m_sensorIds[INDEX(PIPE_FLITE)];
    controlId[CAPTURE_NODE] = m_getSensorId(nodeNums[CAPTURE_NODE], m_flagReprocessing);
    prevNode = nodeNums;

    t3aaNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_30S_NUM;
    t3aaNums[CAPTURE_NODE_1] = -1;
    t3aaNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_30P_NUM;

    ispNums[OUTPUT_NODE]    = FIMC_IS_VIDEO_I1S_NUM;
    ispNums[CAPTURE_NODE_1] = -1;
    ispNums[CAPTURE_NODE_2] = FIMC_IS_VIDEO_I1C_NUM;

    /* 1. 3AAS */
    pipeId = INDEX(PIPE_3AA_REPROCESSING);
    nodeType = getNodeType(PIPE_3AA_REPROCESSING);
    m_deviceInfo[pipeId].nodeNum[nodeType] = t3aaNums[OUTPUT_NODE];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_nodeNums[INDEX(PIPE_FLITE)][CAPTURE_NODE], false, true, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;

    /* 2. 3AAP */
    nodeType = getNodeType(PIPE_3AP_REPROCESSING);
    m_deviceInfo[pipeId].nodeNum[nodeType] = t3aaNums[CAPTURE_NODE_2];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, false, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP_REPROCESSING;

    /* 3. ISPS */
    if (m_flag3aaIspOTF == false) {
        pipeId = INDEX(PIPE_ISP_REPROCESSING);
    }

    nodeType = getNodeType(PIPE_ISP_REPROCESSING);
    m_deviceInfo[pipeId].nodeNum[nodeType] = ispNums[OUTPUT_NODE];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[INDEX(PIPE_3AA_REPROCESSING)].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, false, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISP_REPROCESSING;

    // ISPC
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    m_deviceInfo[pipeId].nodeNum[nodeType] = ispNums[CAPTURE_NODE_2];
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_PREVIEW", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, false, m_flagReprocessing);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_ISPC_REPROCESSING;

    nodeNums = m_nodeNums[INDEX(PIPE_3AC_REPROCESSING)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_30C_NUM;
    nodeNums[SUB_NODE] = -1;
    controlId = m_sensorIds[INDEX(PIPE_3AC_REPROCESSING)];
    prevNode = nodeNums;

    nodeNums = m_nodeNums[INDEX(PIPE_SCC_REPROCESSING)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = FIMC_IS_VIDEO_SCC_NUM;
    nodeNums[SUB_NODE] = -1;
    controlId = m_sensorIds[INDEX(PIPE_SCC_REPROCESSING)];
    prevNode = nodeNums;

    nodeNums = m_nodeNums[INDEX(PIPE_GSC_REPROCESSING)];
    nodeNums[OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;

    nodeNums = m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)];
    nodeNums[OUTPUT_NODE] = -1;
    nodeNums[CAPTURE_NODE] = -1;
    nodeNums[SUB_NODE] = -1;

    return NO_ERROR;
}

}; /* namespace android */
