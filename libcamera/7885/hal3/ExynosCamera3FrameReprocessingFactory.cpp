/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCamera3FrameReprocessingFactory"
#include <cutils/log.h>

#include "ExynosCamera3FrameReprocessingFactory.h"

namespace android {

ExynosCamera3FrameReprocessingFactory::~ExynosCamera3FrameReprocessingFactory()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCamera3FrameReprocessingFactory::create(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    int headPipeId = PIPE_3AA_REPROCESSING;

    m_setupConfig();
    m_constructReprocessingPipes();

    if (m_supportPureBayerReprocessing == true) {
        /* 3AA_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->create();
        if (ret != NO_ERROR) {
            CLOGE("3AA create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->getPipeName(), PIPE_3AA_REPROCESSING);
    } else {
        headPipeId = PIPE_ISP_REPROCESSING;
    }

    /* ISP_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->getPipeName(), PIPE_ISP_REPROCESSING);

    /* MCSC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ISP create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->getPipeName(), PIPE_MCSC_REPROCESSING);

    /* GSC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("GSC create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->getPipeName(), PIPE_GSC_REPROCESSING);

    if (m_flagHWFCEnabled == false) {
        /* JPEG_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->create();
        if (ret != NO_ERROR) {
            CLOGE("JPEG create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->getPipeName(), PIPE_JPEG_REPROCESSING);
    }

    /* EOS */
    ret = m_pipes[INDEX(headPipeId)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("PIPE_%s V4L2_CID_IS_END_OF_STREAM fail, ret(%d)",
                 m_pipes[INDEX(headPipeId)]->getPipeName(), ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);

    return ret;
}

ExynosCameraFrameSP_sptr_t ExynosCamera3FrameReprocessingFactory::createNewFrame(uint32_t frameCount)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }
    ExynosCameraFrameSP_sptr_t frame =  m_frameMgr->createFrame(m_parameters, frameCount, FRAME_TYPE_REPROCESSING);
    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = PIPE_3AA_REPROCESSING;
    int curShotMode = 0;
    int curSeriesShotMode = 0;
    if (m_parameters != NULL) {
        curShotMode = m_parameters->getShotMode();
        curSeriesShotMode = m_parameters->getSeriesShotMode();
    }

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return NULL;
    }

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("frame(%d) metadata initialize fail", m_frameCount);

    /* set 3AA pipe to linkageList */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = PIPE_3AA_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        parentPipeId = pipeId;
    }

    /* set ISP pipe to linkageList */
    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        if (m_supportPureBayerReprocessing == true)
            frame->addChildEntity(newEntity[INDEX(parentPipeId)], newEntity[INDEX(pipeId)], INDEX(PIPE_3AP_REPROCESSING));
        else
            frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        parentPipeId = pipeId;
    }

    requestEntityCount++;

    /* set MCSC pipe to linkageList */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addChildEntity(newEntity[INDEX(parentPipeId)], newEntity[INDEX(pipeId)], INDEX(PIPE_ISPC_REPROCESSING));
        requestEntityCount++;
    }

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (m_parameters->needGSCForCapture(m_cameraId) == true)
        requestEntityCount++;

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING2;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (curShotMode == SHOT_MODE_MAGIC)
        requestEntityCount++;

    /* set JPEG pipe to linkageList */
    pipeId = PIPE_JPEG_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (curShotMode != SHOT_MODE_RICH_TONE
        && curSeriesShotMode != SERIES_SHOT_MODE_LLS
        && curSeriesShotMode != SERIES_SHOT_MODE_SIS
        && m_parameters->getShotMode() != SHOT_MODE_FRONT_PANORAMA
        && m_parameters->getHighResolutionCallbackMode() == false
        && m_flagHWFCEnabled == false)
        requestEntityCount++;

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCamera3FrameReprocessingFactory::initPipes(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    camera_pipe_info_t pipeInfo[MAX_NODE];
    camera_pipe_info_t nullPipeInfo;

    int pipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    enum NODE_TYPE leaderNodeType = OUTPUT_NODE;

    int32_t nodeNums[MAX_NODE];
    int32_t sensorIds[MAX_NODE];
    int32_t secondarySensorIds[MAX_NODE];
    for (int i = 0; i < MAX_NODE; i++) {
        nodeNums[i] = -1;
        sensorIds[i] = -1;
        secondarySensorIds[i] = -1;
    }

    ExynosRect tempRect;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    int hwSensorW = 0, hwSensorH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int maxThumbnailW = 0, maxThumbnailH = 0;
    int yuvWidth[ExynosCamera3Parameters::YUV_MAX] = {0};
    int yuvHeight[ExynosCamera3Parameters::YUV_MAX] = {0};
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA_REPROCESSING);
    int yuvFormat[ExynosCamera3Parameters::YUV_MAX] = {0};
    int pictureFormat = m_parameters->getHwPictureFormat();
    struct ExynosConfigInfo *config = m_parameters->getConfig();
    int perFramePos = 0;

    memset(&nullPipeInfo, 0, sizeof(camera_pipe_info_t));

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
    m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);

    CLOGI(" MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI(" MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", maxPictureW, maxPictureH, hwPictureW, hwPictureH);
    CLOGI(" MaxThumbnailSize(%dx%d)", maxThumbnailW, maxThumbnailH);
    CLOGI(" PreviewBayerCropSize(%dx%d)", bayerCropSize.w, bayerCropSize.h);

    for (int i = ExynosCamera3Parameters::YUV_STALL_0; i < ExynosCamera3Parameters::YUV_STALL_MAX; i++) {
        m_parameters->getYuvSize(&yuvWidth[i % ExynosCamera3Parameters::YUV_MAX],
                                 &yuvHeight[i % ExynosCamera3Parameters::YUV_MAX], i);
        yuvFormat[i % ExynosCamera3Parameters::YUV_MAX] = m_parameters->getYuvFormat(i);

        CLOGI("YUV_STALL[%d] Size %dx%d Format %x",
                i,
                yuvWidth[i % ExynosCamera3Parameters::YUV_MAX],
                yuvHeight[i % ExynosCamera3Parameters::YUV_MAX],
                yuvFormat[i % ExynosCamera3Parameters::YUV_MAX]);
    }

    /*
     * 3AA for Reprocessing
     */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = PIPE_3AA_REPROCESSING;

        /* 3AS */
        nodeType = getNodeType(PIPE_3AA_REPROCESSING);
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AA_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_sensor_buffers + config->current->bufInfo.num_bayer_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_PURE_REPROCESSING_3AA);

#if 0
        /* 3AC */
        nodeType = getNodeType(PIPE_3AC_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_3AC_POS;
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
#endif

        /* 3AP */
        nodeType = getNodeType(PIPE_3AP_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_3AP_POS;
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AP_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* setup pipe info to 3AA pipe */
        if (m_flag3aaIspOTF == false) {
            ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
            if (ret != NO_ERROR) {
                CLOGE("3AA setupPipe fail, ret(%d)", ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    } else {
#if 0
        /*
        * 3A video node is opened for dirty bayer.
        * So, we have to do setinput to 3A video node.
        */
        pipeId = PIPE_3AA_REPROCESSING;

        /* setup pipe info to 3AA pipe */
        ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
        if (ret != NO_ERROR) {
            CLOGE("3AA setupPipe for dirty bayer reprocessing  fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
#endif
    }


    /*
     * ISP for Reprocessing
     */

    /* ISP */
    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        pipeId = PIPE_ISP_REPROCESSING;
        nodeType = getNodeType(PIPE_ISP_REPROCESSING);
        bayerFormat = m_parameters->getBayerFormat(PIPE_ISP_REPROCESSING);

        /* set v4l2 buffer size */
        if (m_parameters->getUsePureBayerReprocessing() == true) {
            tempRect.fullW = hwPictureW;
            tempRect.fullH = hwPictureH;
        } else {
            // Dirtybayer input is bCrop size
            tempRect.fullW = bayerCropSize.w;
            tempRect.fullH = bayerCropSize.h;
        }
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        if(m_supportPureBayerReprocessing) {
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
        } else if (m_parameters->isSupportZSLInput()) {
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
        } else {
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
        }

        /* Set output node default info */
        int ispPerframeInfoIndex = m_supportPureBayerReprocessing ? PERFRAME_INFO_PURE_REPROCESSING_ISP : PERFRAME_INFO_DIRTY_REPROCESSING_ISP;
        SET_OUTPUT_DEVICE_BASIC_INFO(ispPerframeInfoIndex);
    }

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_ISPC_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_ISPP_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false) {
        ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
        if (ret != NO_ERROR) {
            CLOGE("ISP setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }


    /*
     * MCSC for Reprocessing
     */

    /* MCSC */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = PIPE_MCSC_REPROCESSING;
        nodeType = getNodeType(PIPE_MCSC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set output node default info */
        int mcscPerframeInfoIndex = m_supportPureBayerReprocessing ? PERFRAME_INFO_PURE_REPROCESSING_MCSC : PERFRAME_INFO_DIRTY_REPROCESSING_MCSC;
        SET_OUTPUT_DEVICE_BASIC_INFO(mcscPerframeInfoIndex);
    }
#if 0
    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC0_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[ExynosCamera3Parameters::YUV_STALL_0 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.fullH = yuvHeight[ExynosCamera3Parameters::YUV_STALL_0 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.colorFormat = yuvFormat[ExynosCamera3Parameters::YUV_STALL_0 % ExynosCamera3Parameters::YUV_MAX];

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
    pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();
#endif
    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC1_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[ExynosCamera3Parameters::YUV_STALL_1 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.fullH = yuvHeight[ExynosCamera3Parameters::YUV_STALL_1 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.colorFormat = yuvFormat[ExynosCamera3Parameters::YUV_STALL_1 % ExynosCamera3Parameters::YUV_MAX];

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
    pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();
#if 0
    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC2_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = yuvWidth[ExynosCamera3Parameters::YUV_STALL_2 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.fullH = yuvHeight[ExynosCamera3Parameters::YUV_STALL_2 % ExynosCamera3Parameters::YUV_MAX];
    tempRect.colorFormat = yuvFormat[ExynosCamera3Parameters::YUV_STALL_2 % ExynosCamera3Parameters::YUV_MAX];

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
    pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();
#endif
    /* MCSC3 */
    nodeType = getNodeType(PIPE_MCSC3_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC3_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;

    tempRect.colorFormat = pictureFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC4 */
    nodeType = getNodeType(PIPE_MCSC4_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC4_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = maxThumbnailW;
    tempRect.fullH = maxThumbnailH;
    tempRect.colorFormat = pictureFormat;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    if (m_flagHWFCEnabled == true) {
#ifdef SUPPORT_HWFC_SERIALIZATION
        /* Do serialized Q/DQ operation to guarantee the H/W flow control sequence limitation */
        m_pipes[INDEX(pipeId)]->needSerialization(true);
#endif

        /* JPEG Src */
        nodeType = getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

        /* Thumbnail Src */
        nodeType = getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = maxThumbnailW;
        tempRect.fullH = maxThumbnailH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

        /* JPEG Dst */
        nodeType = getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

        /* Thumbnail Dst */
        nodeType = getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = maxThumbnailW;
        tempRect.fullH = maxThumbnailH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    }

    ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
    if (ret != NO_ERROR) {
        CLOGE("ISP setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    ret = m_transitState(FRAME_FACTORY_STATE_INIT);

    return ret;
}

status_t ExynosCamera3FrameReprocessingFactory::preparePipes(void)
{
#if 0
    status_t ret = NO_ERROR;

    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->prepare();
        if (ret != NO_ERROR) {
            CLOGE("ISP prepare fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }
#endif

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::startPipes(void)
{
    status_t ret = NO_ERROR;
    CLOGI("");

    /* MCSC Reprocessing */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("ISP start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* 3AA Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("ISP start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    if (m_supportSingleChain == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC_REPROCESSING start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        return ret;
    }

    CLOGI("Starting Reprocessing [SCC>ISP] Success!");

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::stopPipes(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;
    CLOGI("");

    /* 3AA Reprocessing Thread stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* ISP Reprocessing Thread stop */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_supportSingleChain == true) {
        /* GSC Reprocessing Thread stop */
        if (m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->stopThread();
            if (ret != NO_ERROR) {
                CLOGE("PIPE_GSC_REPROCESSING stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

    /* MCSC Reprocessing Thread stop */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* 3AA Reprocessing stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* ISP Reprocessing stop */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* MCSC Reprocessing stop */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_supportSingleChain == true) {
        ret = stopThreadAndWait(PIPE_GSC_REPROCESSING);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC_REPROCESSING stopThreadAndWait fail, ret(%d)", ret);
        }
    }

    ret = m_transitState(FRAME_FACTORY_STATE_CREATE);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d",
                 ret);
        funcRet |= ret;
    }

    CLOGI("Stopping Reprocessing [3AA>MCSC] Success!");

    return funcRet;
}

status_t ExynosCamera3FrameReprocessingFactory::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGI("start pre-ordered initial pipe thread");

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::setStopFlag(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setStopFlag();
    }
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setStopFlag();
    ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->setStopFlag();
    if (m_supportSingleChain == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->setStopFlag();
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::m_setupConfig(void)
{
    CLOGI("");

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1;
    int nodeMcscp2 = -1, nodeMcscp3 = -1, nodeMcscp4 = -1;
    int previousPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = false;

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->isReprocessing3aaIspOTF();
    m_flagIspTpuOTF = m_parameters->isReprocessingIspTpuOtf();
    m_flagIspMcscOTF = m_parameters->isReprocessingIspMcscOtf();
    m_flagTpuMcscOTF = m_parameters->isReprocessingTpuMcscOtf();

    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportSingleChain = m_parameters->isSingleChain();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request3AP = !(m_flag3aaIspOTF);
    if (m_flagIspTpuOTF == false && m_flagIspMcscOTF == false)
        m_requestISPC = true;
    m_requestMCSC2 = false;
    m_requestMCSC3 = true;
    m_requestMCSC4 = true;
    if (m_flagHWFCEnabled == true) {
        m_requestJPEG = true;
        m_requestThumbnail = true;
    }

#ifdef EXYNOS7885 /* for lassen */
    if (m_supportPureBayerReprocessing) {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
    }
    nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
    nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
    nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
    nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
    //nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M0P_NUM;
    //nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeMcscp3 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp4 = FIMC_IS_VIDEO_M2P_NUM;
#else /* for kangchen */
    if (m_supportPureBayerReprocessing) {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
    }
    nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
    nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
    nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
    nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
    nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeMcscp3 = FIMC_IS_VIDEO_M3P_NUM;
    nodeMcscp4 = FIMC_IS_VIDEO_M4P_NUM;
#endif

    if (m_supportPureBayerReprocessing == true) {
        m_initDeviceInfo(INDEX(PIPE_3AA_REPROCESSING));
    }
    m_initDeviceInfo(INDEX(PIPE_ISP_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_MCSC_REPROCESSING));


    /*
     * 3AA for Reprocessing
     */
    pipeId = INDEX(PIPE_3AA_REPROCESSING);
    previousPipeId = PIPE_FLITE;

    /*
     * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
     */
    if (m_supportPureBayerReprocessing == false && m_flag3aaIspOTF == false)
        flagStreamLeader = false;
    else
        flagStreamLeader = true;

    bool flagUseCompanion = false;
    bool flagQuickSwitchFlag = false;

#ifdef SAMSUNG_COMPANION
    flagUseCompanion = m_parameters->getUseCompanion();
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    flagQuickSwitchFlag = m_parameters->getQuickSwitchFlag();
#endif

    if (m_supportPureBayerReprocessing) {
        /* 3AS */
        nodeType = getNodeType(PIPE_3AA_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        int fliteNodeNum = getFliteNodenum(m_cameraId, flagUseCompanion, flagQuickSwitchFlag);
        m_sensorIds[pipeId][nodeType]  = m_getSensorId(getFliteCaptureNodenum(m_cameraId, fliteNodeNum), false, flagStreamLeader, m_flagReprocessing);

        // Restore leader flag
        flagStreamLeader = false;

#if 0
        /* 3AC */
        nodeType = getNodeType(PIPE_3AC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_OPT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
#endif

        /* 3AP */
        nodeType = getNodeType(PIPE_3AP_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }
    /*
     * ISP for Reprocessing
     */
    previousPipeId = pipeId;
    pipeId = m_flag3aaIspOTF ? INDEX(PIPE_3AA_REPROCESSING) : INDEX(PIPE_ISP_REPROCESSING);

    if (m_supportPureBayerReprocessing == false && m_flag3aaIspOTF == false) {
        /* Set leader flag on ISP pipe for Dirty bayer reprocessing */
        flagStreamLeader = true;
    }
    /* ISPS */
    nodeType = getNodeType(PIPE_ISP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flag3aaIspOTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId((flagStreamLeader == true) ? nodeIsp : m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

    // Restore leader flag
    flagStreamLeader = false;

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_M2M", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flagIspMcscOTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_OTF", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /*
     * MCSC for Reprocessing
     */
    previousPipeId = pipeId;

    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false)
        pipeId = INDEX(PIPE_MCSC_REPROCESSING);

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flagIspMcscOTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPP_REPROCESSING)], m_flagIspMcscOTF, flagStreamLeader, m_flagReprocessing);
#if 0
    /* MCSC0 */
    nodeType = getNodeType(PIPE_MCSC0_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_0", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
#endif
    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_1", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
#if 0
    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_2", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
#endif
    /* MCSC3 */
    nodeType = getNodeType(PIPE_MCSC3_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC3_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp3;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* MCSC4 */
    nodeType = getNodeType(PIPE_MCSC4_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC4_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp4;
    m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_THUMBNAIL", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    if (m_flagHWFCEnabled == true) {
        /* JPEG Src */
        nodeType = getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Src */
        nodeType = getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* JPEG Dst */
        nodeType = getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Dst  */
        nodeType = getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }

    /* GSC for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG for Reprocessing */
    m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)][OUTPUT_NODE] = -1;

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::m_constructReprocessingPipes(void)
{
    CLOGI("");

    int pipeId = -1;

    /* 3AA for Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = PIPE_3AA_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_3AA_REPROCESSING");
    }

    /* ISP for Reprocessing */
    pipeId = PIPE_ISP_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_ISP_REPROCESSING");

    /* MCSC for Reprocessing */
    pipeId = PIPE_MCSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_MCSC_REPROCESSING");

    /* GSC for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING");

    if (m_flagHWFCEnabled == false) {
        /* JPEG for Reprocessing */
        pipeId = PIPE_JPEG_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_JPEG_REPROCESSING");
    }

    CLOGI("pipe ids for reprocessing");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("-> m_pipes[%d] : PipeId(%d)" , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCamera3FrameReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group *node_group_info_temp = NULL;

    int zoom = m_parameters->getZoomLevel();
    int pipeId = -1;
    uint32_t perframePosition = 0;

#if defined(SR_CAPTURE) || defined(SAMSUNG_HIFI_LLS)
    if (m_parameters->getNoBayerCrop()) {
        zoom = ZOOM_LEVEL_0;
    }
#endif

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));

    /* 3AA for Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = INDEX(PIPE_3AA_REPROCESSING);
        node_group_info_temp = &node_group_info_3aa;
        node_group_info_temp->leader.request = 1;
        if (m_request3AC == true) {
            node_group_info_temp->capture[perframePosition].request = m_request3AC;
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AC_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        node_group_info_temp->capture[perframePosition].request = m_request3AP;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    /* ISP for Reprocessing */
    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        pipeId = INDEX(PIPE_ISP_REPROCESSING);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_isp;
        node_group_info_temp->leader.request = 1;
    }

    if (m_flagIspMcscOTF == false && node_group_info_temp != NULL) {
        node_group_info_temp->capture[perframePosition].request = m_requestISPC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPC_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    if (m_requestISPP == true && node_group_info_temp != NULL) {
        node_group_info_temp->capture[perframePosition].request = m_requestISPP;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPP_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    /* MCSC for Reprocessing */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        pipeId = INDEX(PIPE_MCSC_REPROCESSING);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_mcsc;
        node_group_info_temp->leader.request = 1;
    }
    
    if (node_group_info_temp != NULL) {
#if 0
        node_group_info_temp->capture[perframePosition].request = m_requestMCSC0;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC0_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
#endif
        node_group_info_temp->capture[perframePosition].request = m_requestMCSC1;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC1_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
#if 0
        node_group_info_temp->capture[perframePosition].request = m_requestMCSC2;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC2_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
#endif
        node_group_info_temp->capture[perframePosition].request = m_requestMCSC3;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC3_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;

        node_group_info_temp->capture[perframePosition].request = m_requestMCSC4;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC4_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
    }
    
    frame->setZoom(zoom);

    if (m_supportPureBayerReprocessing == true) {
        updateNodeGroupInfo(
                PIPE_3AA_REPROCESSING,
                m_parameters,
                &node_group_info_3aa);
        frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA);
    }

    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        updateNodeGroupInfo(
                PIPE_ISP_REPROCESSING,
                m_parameters,
                &node_group_info_isp);
        if (m_supportPureBayerReprocessing == true)
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP);
        else
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    return NO_ERROR;
}

void ExynosCamera3FrameReprocessingFactory::m_init(void)
{
    m_flagReprocessing = true;
    m_flagHWFCEnabled = m_parameters->isUseHWFC();
}

}; /* namespace android */
