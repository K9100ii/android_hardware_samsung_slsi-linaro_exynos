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
#define LOG_TAG "ExynosCameraFrameReprocessingFactory"
#include <cutils/log.h>

#include "ExynosCameraFrameReprocessingFactory.h"

namespace android {

ExynosCameraFrameReprocessingFactory::~ExynosCameraFrameReprocessingFactory()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");

    m_setCreate(false);
}

status_t ExynosCameraFrameReprocessingFactory::create(__unused bool active)
{
    Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* SYNC_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):SYNC_REPROCESSING create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->getPipeName(), PIPE_SYNC_REPROCESSING);

        /* FUSION_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->create();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):FUSION_REPROCESSING create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->getPipeName(), PIPE_FUSION_REPROCESSING);
    }
#endif

    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        /* GSC_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->create();
        if (ret != NO_ERROR) {
            CLOGE("GSC create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->getPipeName(), PIPE_GSC_REPROCESSING);
    }

    /* GSC_REPROCESSING3 pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->create();
    if (ret != NO_ERROR) {
        CLOGE("GSC3 create fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGV("%s(%d) created",
            m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->getPipeName(), PIPE_GSC_REPROCESSING3);

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
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

    /* TPU1 */
    if (m_flagTPU1Enabled == true) {
        /* TPU1 pipe initialize */
        ret = m_pipes[INDEX(PIPE_TPU1)]->create();
        if (ret != NO_ERROR) {
            CLOGE("GSC create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[INDEX(PIPE_TPU1)]->getPipeName(), PIPE_TPU1);

        /* GSC_REPROCESSING4 pipe initialize */
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING4)]->create();
        if (ret != NO_ERROR) {
            CLOGE("GSC4 create fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGV("%s(%d) created",
                m_pipes[INDEX(PIPE_GSC_REPROCESSING4)]->getPipeName(), PIPE_GSC_REPROCESSING4);

        /* EOS */
        ret = m_pipes[INDEX(PIPE_TPU1)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
        if (ret != NO_ERROR) {
            CLOGE("PIPE_%s V4L2_CID_IS_END_OF_STREAM fail, ret(%d)",
                    m_pipes[INDEX(PIPE_TPU1)]->getPipeName(), ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    m_setCreate(true);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::initPipes(void)
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
    int previewW = 0, previewH = 0;
    int pictureW = 0, pictureH = 0;
    int maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    int maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    int maxThumbnailW = 0, maxThumbnailH = 0;
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA_REPROCESSING);
    int previewFormat = m_parameters->getPreviewFormat();
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
    m_parameters->getPreviewSize(&previewW, &previewH);
    m_parameters->getPictureSize(&pictureW, &pictureH);
    m_parameters->getMaxThumbnailSize(&maxThumbnailW, &maxThumbnailH);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize, false);

    CLOGI(" MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI(" MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", maxPictureW, maxPictureH, hwPictureW, hwPictureH);
    CLOGI(" PreviewSize(%dx%d), PictureSize(%dx%d)", previewW, previewH, pictureW, pictureH);
    CLOGI(" MaxThumbnailSize(%dx%d)", maxThumbnailW, maxThumbnailH);


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

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

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
            tempRect.fullW = bayerCropSize.w;
            tempRect.fullH = bayerCropSize.h;
        }
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node bytes per plane */
        pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = m_supportPureBayerReprocessing ? config->current->bufInfo.num_picture_buffers \
                                                                          : config->current->bufInfo.num_bayer_buffers;

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

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC1_POS;

    /* set v4l2 buffer size */
#ifdef SAMSUNG_LENS_DC
    if (!m_parameters->getSamsungCamera() && m_parameters->getLensDCMode()) {
        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
    } else
#endif
    {
        tempRect.fullW = pictureW;
        tempRect.fullH = pictureH;
    }

    tempRect.colorFormat = V4L2_PIX_FMT_NV21;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* MCSC3 */
    nodeType = getNodeType(PIPE_MCSC3_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC3_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = maxPictureW;
    tempRect.fullH = maxPictureH;
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
        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
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
        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
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

    /* setup pipe info to TPU1 pipe */
    if (m_flagTPU1Enabled) {

        /* TPU1 */
        pipeId = PIPE_TPU1;
        nodeType = getNodeType(PIPE_TPU1);

        /* set v4l2 buffer size */
        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_REPROCESSING_TPU);

        /* TPU1C */
        nodeType = getNodeType(PIPE_TPU1C);

        /* set v4l2 buffer size */
        tempRect.fullW = maxPictureW;
        tempRect.fullH = maxPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;

        ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
        if (ret != NO_ERROR) {
            CLOGE("TPU1 setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::preparePipes(void)
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

status_t ExynosCameraFrameReprocessingFactory::startPipes(void)
{
    status_t ret = NO_ERROR;
    CLOGV("");

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_FUSION_REPROCESSING start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->start();
        if (ret < 0) {
            CLOGE("ERR(%s[%d]):PIPE_SYNC_REPROCESSING start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        }
    }
#endif

    /* TPU1 Reprocessing */
    if (m_flagTPU1Enabled) {
        ret = m_pipes[INDEX(PIPE_TPU1)]->start();
        if (ret != NO_ERROR) {
            CLOGE("TPU1 start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

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

    CLOGI("Starting Reprocessing [SCC>ISP] Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::stopPipes(void)
{
    status_t ret = NO_ERROR;
    status_t funcRet = NO_ERROR;
    CLOGV("");

    /* 3AA Reprocessing Thread stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("3AA stopThread fail, ret(%d)", ret);
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

    /* MCSC Reprocessing Thread stop */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* GSC Reprocessing Thread stop */
    if (m_parameters->needGSCForCapture(m_cameraId) == true
        && m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("GSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL &&
                m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->stopThread();
            if (ret != NO_ERROR) {
                CLOGE("SYNC_REPROCESSING stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }

        if (m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] != NULL &&
                m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->isThreadRunning() == true) {
            ret = stopThread(INDEX(PIPE_FUSION_REPROCESSING));
            if (ret < 0) {
                CLOGE("ERR(%s[%d]):PIPE_FUSION_REPROCESSING stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }
#endif

    /* 3AA Reprocessing stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("3AA stop fail, ret(%d)", ret);
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL) {
            ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->stop();
            if (ret != NO_ERROR) {
                CLOGE("SYNC_REPROCESSING stop fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }

        if (m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] != NULL) {
            ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->stop();
            if (ret < 0) {
                CLOGE("ERR(%s[%d]): m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->stop() fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }
#endif

    /* GSC Reprocessing stop */
    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("GSC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* GSC3 Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("GSC3 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG Reprocessing stop */
        ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("JPEG stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* TPU1 stop */
    if (m_flagTPU1Enabled == true) {
        ret = m_pipes[INDEX(PIPE_TPU1)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("TPU1 stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    CLOGI("Stopping Reprocessing [3AA>MCSC] Success!");

    return funcRet;
}

status_t ExynosCameraFrameReprocessingFactory::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGV("start pre-ordered initial pipe thread");

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::setStopFlag(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;

    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setStopFlag();
    }
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setStopFlag();
    ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->setStopFlag();

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL &&
                m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->flagStart() == true)
            ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->setStopFlag();

        if (m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] != NULL &&
                m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->flagStart() == true)
            ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->setStopFlag();
    }
#endif

    if (m_flagTPU1Enabled)
        ret = m_pipes[INDEX(PIPE_TPU1)]->setStopFlag();

    return NO_ERROR;
}

ExynosCameraFrameSP_sptr_t ExynosCameraFrameReprocessingFactory::createNewFrame(ExynosCameraFrameSP_sptr_t refFrame)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrameSP_sptr_t frame =  m_frameMgr->createFrame(m_parameters, m_frameCount, FRAME_TYPE_REPROCESSING);

    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = PIPE_3AA_REPROCESSING;
    int curShotMode = 0;
    int curSeriesShotMode = 0;
#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    int masterCameraId = -1;
    int slaveCameraId = -1;
#endif

    if (m_parameters != NULL) {
        curShotMode = m_parameters->getShotMode();
        curSeriesShotMode = m_parameters->getSeriesShotMode();
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

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        getDualCameraId(&masterCameraId, &slaveCameraId);

        newEntity[INDEX(PIPE_SYNC_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_SYNC_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_SYNC_REPROCESSING)]);
        requestEntityCount++;

        /* slave frame's life cycle is util sync pipe */
        if (m_cameraId == slaveCameraId)
            goto SKIP_SLAVE_CAMERA;

        newEntity[INDEX(PIPE_FUSION_REPROCESSING)] = new ExynosCameraFrameEntity(PIPE_FUSION_REPROCESSING, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(PIPE_FUSION_REPROCESSING)]);
        requestEntityCount++;
    }
#endif

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
    if (m_parameters->getIsThumbnailCallbackOn() == true
#ifdef SAMSUNG_MAGICSHOT
        || curShotMode == SHOT_MODE_MAGIC
#endif
       )
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
        && (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand()))) {
        requestEntityCount++;
    }

#ifdef SUPPORT_DEPTH_MAP
    /* set VC1 pipe to linkageList */
    pipeId = PIPE_VC1;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
#endif

    if (m_flagTPU1Enabled) {
        pipeId = PIPE_TPU1;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

        /* set GSC pipe to linkageList */
        pipeId = PIPE_GSC_REPROCESSING4;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    }

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
SKIP_SLAVE_CAMERA:
#endif

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame, refFrame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactory::m_setupConfig(void)
{
    CLOGI("");

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeTpu = -1;
    int nodeMcsc = -1, nodeMcscp1 = -1, nodeMcscp3 = -1, nodeMcscp4 = -1;
    int previousPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = false;

    m_flagFlite3aaOTF = m_parameters->isFlite3aaOtf();
    m_flag3aaIspOTF = m_parameters->isReprocessing3aaIspOTF();
    m_flagIspTpuOTF = m_parameters->isReprocessingIspTpuOtf();
    m_flagIspMcscOTF = m_parameters->isReprocessingIspMcscOtf();
    m_flagTpuMcscOTF = m_parameters->isReprocessingTpuMcscOtf();
#ifdef USE_ODC_CAPTURE
    m_flagTPU1Enabled = m_parameters->getODCCaptureMode();
#endif

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

    if (m_supportSingleChain == true) {
        if (m_supportPureBayerReprocessing == true) {
            node3aa = FIMC_IS_VIDEO_30S_NUM;
            node3ac = FIMC_IS_VIDEO_30C_NUM;
            node3ap = FIMC_IS_VIDEO_30P_NUM;
        }
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M0P_NUM;
        nodeMcscp3 = FIMC_IS_VIDEO_M1P_NUM;
        nodeMcscp4 = FIMC_IS_VIDEO_M2P_NUM;
    } else {
        if (m_supportPureBayerReprocessing == true) {
            if (((m_cameraId == CAMERA_ID_FRONT || m_cameraId == CAMERA_ID_BACK_1) &&
                (m_parameters->getDualMode() == true)) ||
                (m_parameters->getUseCompanion() == false)) {
                node3aa = FIMC_IS_VIDEO_31S_NUM;
                node3ac = FIMC_IS_VIDEO_31C_NUM;
                node3ap = FIMC_IS_VIDEO_31P_NUM;
            } else {
                node3aa = FIMC_IS_VIDEO_30S_NUM;
                node3ac = FIMC_IS_VIDEO_30C_NUM;
                node3ap = FIMC_IS_VIDEO_30P_NUM;
            }
        }
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
        nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
        nodeMcscp3 = FIMC_IS_VIDEO_M3P_NUM;
        nodeMcscp4 = FIMC_IS_VIDEO_M4P_NUM;
    }

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

    /* 3AS */
    bool flagUseCompanion = false;
    bool flagQuickSwitchFlag = false;

#ifdef SAMSUNG_COMPANION
    flagUseCompanion = m_parameters->getUseCompanion();
#endif

#ifdef SAMSUNG_QUICK_SWITCH
    flagQuickSwitchFlag = m_parameters->getQuickSwitchFlag();
#endif

    if (m_supportPureBayerReprocessing == true) {
        /*
         * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
         */
        flagStreamLeader = true;

        nodeType = getNodeType(PIPE_3AA_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
        m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flagFlite3aaOTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        int fliteNodeNum = getFliteNodenum(m_cameraId, flagUseCompanion, flagQuickSwitchFlag);
        m_sensorIds[pipeId][nodeType]  = m_getSensorId(getFliteCaptureNodenum(m_cameraId, fliteNodeNum), false, flagStreamLeader, m_flagReprocessing);

        /* Other nodes is not stream leader */
        flagStreamLeader = false;

#if 0
        /* 3AC */
        nodeType = getNodeType(PIPE_3AC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_OPT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
#endif

        /* 3AP */
        nodeType = getNodeType(PIPE_3AP_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
        m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flag3aaIspOTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }

    /*
     * ISP for Reprocessing
     */
    previousPipeId = pipeId;
    pipeId = m_flag3aaIspOTF ? INDEX(PIPE_3AA_REPROCESSING) : INDEX(PIPE_ISP_REPROCESSING);

    /*
     * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
     */
    if (m_supportPureBayerReprocessing == false && m_flag3aaIspOTF == false)
        flagStreamLeader = true;

    /* ISPS */
    nodeType = getNodeType(PIPE_ISP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flag3aaIspOTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId((flagStreamLeader == true) ? nodeIsp : m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

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

    /* MCSC1 */
    nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_SPECIAL", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

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

    /* GSC3 for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING3)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG for Reprocessing */
    m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)][OUTPUT_NODE] = -1;

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    m_nodeNums[INDEX(PIPE_SYNC_REPROCESSING)][OUTPUT_NODE] = -1;

    m_nodeNums[INDEX(PIPE_FUSION_REPROCESSING)][OUTPUT_NODE] = 0;
    m_nodeNums[INDEX(PIPE_FUSION_REPROCESSING)][CAPTURE_NODE_1] = -1;
    m_nodeNums[INDEX(PIPE_FUSION_REPROCESSING)][CAPTURE_NODE_2] = -1;
#endif

    /* TPU1 */
    if (m_flagTPU1Enabled) {
        pipeId = PIPE_TPU1;
        nodeType = getNodeType(PIPE_TPU1);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_TPU1;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_D1S_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_M2M;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_TPU1", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_TPU1)], false, true, m_flagReprocessing);

        nodeType = getNodeType(PIPE_TPU1C);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_TPU1C;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_D1C_NUM;
        m_deviceInfo[pipeId].connectionMode[nodeType] = HW_CONNECTION_MODE_OTF;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_TPU1_CAPTURE", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_TPU1)], true, false, m_flagReprocessing);

        /* GSC4 for Reprocessing */
        m_nodeNums[INDEX(PIPE_GSC_REPROCESSING4)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_constructReprocessingPipes(void)
{
    CLOGI("");

    int pipeId = -1;

    /* 3AA for Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = PIPE_3AA_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_3AA_REPROCESSING");
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    }

    /* ISP for Reprocessing */
    pipeId = PIPE_ISP_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_ISP_REPROCESSING");
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);

    /* MCSC for Reprocessing */
    pipeId = PIPE_MCSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_MCSC_REPROCESSING");
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* Sync */
        ExynosCameraPipeSync *syncPipe = new ExynosCameraPipeSync(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_SYNC_REPROCESSING)]);
        m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] = (ExynosCameraPipe*)syncPipe;
        m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->setPipeId(PIPE_SYNC_REPROCESSING);
        m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->setPipeName("PIPE_SYNC_REPROCESSING");

        /* Fusion */
        ExynosCameraPipeFusion *fusionPipe = new ExynosCameraPipeFusion(m_cameraId, m_parameters, false, m_nodeNums[INDEX(PIPE_FUSION_REPROCESSING)]);
        m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] = (ExynosCameraPipe*)fusionPipe;
        m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->setPipeId(PIPE_FUSION_REPROCESSING);
        m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->setPipeName("PIPE_FUSION_REPROCESSING");

        /* set the dual selector or fusionWrapper */
        ExynosCameraDualCaptureFrameSelector *dualSelector = ExynosCameraSingleton<ExynosCameraDualCaptureFrameSelector>::getInstance();
#ifdef USE_CP_FUSION_REPROCESSING_LIB
        ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionCaptureWrapperCP>::getInstance();
#else
        ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionCaptureWrapper>::getInstance();
#endif
        syncPipe->setDualSelector(dualSelector);
        fusionPipe->setDualSelector(dualSelector);
        fusionPipe->setFusionWrapper(fusionWrapper);
    }
#endif

    /* GSC for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING");
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);

    /* GSC3 for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING3;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING3");
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG for Reprocessing */
        pipeId = PIPE_JPEG_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_JPEG_REPROCESSING");
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    }

    if (m_flagTPU1Enabled == true) {
        /* TPU1 for Reprocessing */
        pipeId = PIPE_TPU1;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_TPU1");
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);

        /* GSC4 for Reprocessing */
        pipeId = PIPE_GSC_REPROCESSING4;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING4");
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    }

    CLOGI("pipe ids for Reprocessing");
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("-> m_pipes[%d] : PipeId(%d), Name(%s)" , i, m_pipes[i]->getPipeId(), m_pipes[i]->getPipeName());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame, ExynosCameraFrameSP_sptr_t refFrame)
{
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group node_group_info_tpu1;
    camera2_node_group *node_group_info_temp = NULL;
    size_control_info_t sizeControlInfo;

    int zoom = 0;
    int pipeId = -1;
    uint32_t perframePosition = 0;

    /* for capture node's request */
    uint32_t request3AP = m_request3AP;
    uint32_t request3AC = m_request3AC;
    uint32_t requestISPC = m_requestISPC;
    uint32_t requestISPP = m_requestISPP;
    uint32_t requestMCSC1 = m_requestMCSC1;
    uint32_t requestMCSC3 = m_requestMCSC3;
    uint32_t requestMCSC4 = m_requestMCSC4;
    uint32_t requestJPEG = m_requestJPEG;
    uint32_t requestThumbnail = m_requestThumbnail;

#ifdef SR_CAPTURE
    if(m_parameters->getSROn()) {
        zoom = ZOOM_LEVEL_0;
    }
#endif
    if (refFrame == NULL) {
        zoom = m_parameters->getZoomLevel();
    } else {
        zoom = refFrame->getReprocessingZoom();
        frame->setFrameType((frame_type_t)refFrame->getReprocessingFrameType());
        frame->setZoom(refFrame->getReprocessingZoom());
        frame->setSyncType(refFrame->getReprocessingSyncType());
        if (frame->getFrameType() == FRAME_TYPE_INTERNAL) {
            struct camera2_shot_ext shot_ext;
            refFrame->getMetaData(&shot_ext);
            frame->setMetaData(&shot_ext);
        }
    }

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_tpu1, 0x0, sizeof(camera2_node_group));

#ifdef BOARD_CAMERA_USES_DUAL_CAMERA
    if (m_parameters->getDualCameraMode() == true) {
        /* for internal frame */
        if (frame->getFrameType() == FRAME_TYPE_INTERNAL) {
            /*
             * Clear all capture node's request.
             * But don't touch MCSC3, Jpeg, Thumbnail for capture in master camera.
             */
            request3AP = 0;
            request3AC = 0;
            requestISPC = 0;
            requestISPP = 0;
            requestMCSC1 = 0;
            requestMCSC3 = 0;
            /* requestMCSC4 = 0; */
            /* requestJPEG = 0; */
            /* requestThumbnail = 0; */

            frame->setRequest(PIPE_3AP_REPROCESSING, request3AP);
            frame->setRequest(PIPE_3AP_REPROCESSING, request3AC);
            frame->setRequest(PIPE_ISPC_REPROCESSING, requestISPC);
            frame->setRequest(PIPE_ISPP_REPROCESSING, requestISPP);
            frame->setRequest(PIPE_MCSC1_REPROCESSING, requestMCSC1);
            /* frame->setRequest(PIPE_MCSC3_REPROCESSING, requestMCSC3); */
            frame->setRequest(PIPE_MCSC4_REPROCESSING, requestMCSC4);
            /* frame->setRequest(PIPE_HWFC_JPEG_SRC_REPROCESSING, requestJPEG); */
            /* frame->setRequest(PIPE_HWFC_JPEG_DST_REPROCESSING, requestJPEG); */
            /* frame->setRequest(PIPE_HWFC_THUMB_SRC_REPROCESSING, requestThumbnail); */
            /* frame->setRequest(PIPE_HWFC_THUMB_DST_REPROCESSING, requestThumbnail); */
        }
    }
#endif

    /* 3AA for Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = INDEX(PIPE_3AA_REPROCESSING);
        node_group_info_temp = &node_group_info_3aa;
        node_group_info_temp->leader.request = 1;
        if (request3AC == true) {
            node_group_info_temp->capture[perframePosition].request = request3AC;
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AC_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
            perframePosition++;
        }

        node_group_info_temp->capture[perframePosition].request = request3AP;
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

    if (m_flagIspMcscOTF == false) {
        node_group_info_temp->capture[perframePosition].request = requestISPC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPC_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    if (requestISPP == true) {
        node_group_info_temp->capture[perframePosition].request = requestISPP;
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

    if (perframePosition < CAPTURE_NODE_MAX) {
        node_group_info_temp->capture[perframePosition].request = requestMCSC1;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC1_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    } else {
        CLOGE("Perframe capture node index out of bound, requestMCSC1 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
    }

    if (perframePosition < CAPTURE_NODE_MAX) {
        node_group_info_temp->capture[perframePosition].request = requestMCSC3;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC3_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    } else {
        CLOGE("Perframe capture node index out of bound, requestMCSC3 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
    }

    if (perframePosition < CAPTURE_NODE_MAX) {
        node_group_info_temp->capture[perframePosition].request = requestMCSC4;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC4_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
    } else {
        CLOGE("Perframe capture node index out of bound, requestMCSC4 index(%d) CAPTURE_NODE_MAX(%d)", perframePosition, CAPTURE_NODE_MAX);
    }

    if (m_parameters->getUsePureBayerReprocessing() == true) {
        m_parameters->getPictureBayerCropSize(&sizeControlInfo.bnsSize,
                                              &sizeControlInfo.bayerCropSize);
        m_parameters->getPictureBdsSize(&sizeControlInfo.bdsSize);
    } else {
        /* If dirty bayer is used for reprocessing,
         * reprocessing ISP input size should be set preview bayer crop size.
         */
        m_parameters->getPreviewBayerCropSize(&sizeControlInfo.bnsSize,
                                              &sizeControlInfo.bayerCropSize);
        sizeControlInfo.bdsSize = sizeControlInfo.bayerCropSize;
    }

    m_parameters->getPictureYuvCropSize(&sizeControlInfo.yuvCropSize);
    m_parameters->getPreviewSize(&sizeControlInfo.previewSize.w,
                                 &sizeControlInfo.previewSize.h);
    m_parameters->getPictureSize(&sizeControlInfo.pictureSize.w,
                                 &sizeControlInfo.pictureSize.h);
    m_parameters->getThumbnailSize(&sizeControlInfo.thumbnailSize.w,
                                   &sizeControlInfo.thumbnailSize.h);

    frame->setZoom(zoom);

    if (m_supportPureBayerReprocessing == true) {
        updateNodeGroupInfo(
                PIPE_3AA_REPROCESSING,
                m_parameters,
                sizeControlInfo,
                &node_group_info_3aa);
        frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA);
    }

    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        updateNodeGroupInfo(
                PIPE_ISP_REPROCESSING,
                m_parameters,
                sizeControlInfo,
                &node_group_info_isp);
        if (m_supportPureBayerReprocessing == true)
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP);
        else
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    if (m_flagTPU1Enabled) {
        pipeId = INDEX(PIPE_TPU1);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_tpu1;
        node_group_info_temp->leader.request = 1;

        node_group_info_temp->capture[perframePosition].request = 1;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_TPU1C)] - FIMC_IS_VIDEO_BAS_NUM;
        updateNodeGroupInfo(
                PIPE_TPU1,
                m_parameters,
                sizeControlInfo,
                &node_group_info_tpu1);
        frame->storeNodeGroupInfo(&node_group_info_tpu1, PERFRAME_INFO_REPROCESSING_TPU);
    }

    return NO_ERROR;
}

void ExynosCameraFrameReprocessingFactory::m_init(void)
{
    m_flagReprocessing = true;
    m_flagHWFCEnabled = m_parameters->isUseHWFC();

    m_flagTPU1Enabled = false;
}

}; /* namespace android */
