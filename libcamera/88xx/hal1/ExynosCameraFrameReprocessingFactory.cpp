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
        CLOGE("ERR(%s[%d]):destroy fail", __FUNCTION__, __LINE__);

    m_setCreate(false);
}

status_t ExynosCameraFrameReprocessingFactory::create(__unused bool active)
{
    Mutex::Autolock lock(ExynosCameraStreamMutex::getInstance()->getStreamMutex());

    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    m_setupConfig();
    m_constructReprocessingPipes();

    /* 3AA_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):3AA create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->getPipeName(), PIPE_3AA_REPROCESSING);

    /* ISP_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->getPipeName(), PIPE_ISP_REPROCESSING);

    /* MCSC_REPROCESSING pipe initialize */
    ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):ISP create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->getPipeName(), PIPE_MCSC_REPROCESSING);

    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        /* GSC_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):GSC create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->getPipeName(), PIPE_GSC_REPROCESSING);
    }

    /* GSC_REPROCESSING3 pipe initialize */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->create();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):GSC3 create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }
    CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
            m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->getPipeName(), PIPE_GSC_REPROCESSING3);

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG_REPROCESSING pipe initialize */
        ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->create();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):JPEG create fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
        CLOGD("DEBUG(%s[%d]):%s(%d) created", __FUNCTION__, __LINE__,
                m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->getPipeName(), PIPE_JPEG_REPROCESSING);
    }

    /* EOS */
    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setControl(V4L2_CID_IS_END_OF_STREAM, 1);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):PIPE_%s V4L2_CID_IS_END_OF_STREAM fail, ret(%d)",
                __FUNCTION__, __LINE__, m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->getPipeName(), ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    m_setCreate(true);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::initPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

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

    CLOGI("INFO(%s[%d]): MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", __FUNCTION__, __LINE__, maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI("INFO(%s[%d]): MaxPixtureSize(%dx%d), HwPixtureSize(%dx%d)", __FUNCTION__, __LINE__, maxPictureW, maxPictureH, hwPictureW, hwPictureH);
    CLOGI("INFO(%s[%d]): PreviewSize(%dx%d), PictureSize(%dx%d)", __FUNCTION__, __LINE__, previewW, previewH, pictureW, pictureH);
    CLOGI("INFO(%s[%d]): MaxThumbnailSize(%dx%d)", __FUNCTION__, __LINE__, maxThumbnailW, maxThumbnailH);


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
                CLOGE("ERR(%s[%d]):3AA setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
                /* TODO: exception handling */
                return INVALID_OPERATION;
            }

            /* clear pipeInfo for next setupPipe */
            for (int i = 0; i < MAX_NODE; i++)
                pipeInfo[i] = nullPipeInfo;
        }
    } else {
        /*
        * 3A video node is opened for dirty bayer.
        * So, we have to do setinput to 3A video node.
        */
        pipeId = PIPE_3AA_REPROCESSING;

        /* setup pipe info to 3AA pipe */
        ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA setupPipe for dirty bayer reprocessing  fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }

        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;
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
            CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
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

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_MCSC2_POS;

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
        CLOGE("ERR(%s[%d]):ISP setupPipe fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
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
            CLOGE("ERR(%s[%d]):ISP prepare fail, ret(%d)", __FUNCTION__, __LINE__, ret);
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
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* MCSC Reprocessing */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP start fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* 3AA Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
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
    status_t ret = NO_ERROR;
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    /* 3AA Reprocessing Thread stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing Thread stop */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* MCSC Reprocessing Thread stop */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* GSC Reprocessing Thread stop */
    if (m_parameters->needGSCForCapture(m_cameraId) == true
        && m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):GSC stopThread fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* 3AA Reprocessing stop */
    if (m_supportPureBayerReprocessing == true) {
        ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):3AA stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing stop */
    if (m_flag3aaIspOTF == false) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):ISP stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* MCSC Reprocessing stop */
    if (m_flagIspMcscOTF == false && m_flagTpuMcscOTF == false) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):MCSC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* GSC Reprocessing stop */
    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):GSC stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* GSC3 Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):GSC3 stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG Reprocessing stop */
        ret = m_pipes[INDEX(PIPE_JPEG_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ERR(%s[%d]):JPEG stop fail, ret(%d)", __FUNCTION__, __LINE__, ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    CLOGI("INFO(%s[%d]):Stopping Reprocessing [3AA>MCSC] Success!", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::startInitialThreads(void)
{
    status_t ret = NO_ERROR;

    CLOGI("INFO(%s[%d]):start pre-ordered initial pipe thread", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::setStopFlag(void)
{
    CLOGI("INFO(%s[%d]):", __FUNCTION__, __LINE__);

    status_t ret = NO_ERROR;

    ret = m_pipes[INDEX(PIPE_3AA_REPROCESSING)]->setStopFlag();
    ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->setStopFlag();
    ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->setStopFlag();

    return NO_ERROR;
}

ExynosCameraFrame * ExynosCameraFrameReprocessingFactory::createNewFrame(void)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    ExynosCameraFrame *frame =  m_frameMgr->createFrame(m_parameters, m_frameCount, FRAME_TYPE_REPROCESSING);

    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = PIPE_3AA_REPROCESSING;
    int curShotMode = 0;
    int curSeriesShotMode = 0;
    if (m_parameters != NULL) {
        curShotMode = m_parameters->getShotMode();
        curSeriesShotMode = m_parameters->getSeriesShotMode();
    }

    ret = m_initFrameMetadata(frame);
    if (ret != NO_ERROR)
        CLOGE("(%s[%d]):frame(%d) metadata initialize fail", __FUNCTION__, __LINE__, m_frameCount);

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

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("ERR(%s[%d]):m_initPipelines fail, ret(%d)", __FUNCTION__, __LINE__, ret);
    }

    frame->setNumRequestPipe(requestEntityCount);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactory::m_setupConfig(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeTpu = -1;
    int nodeMcsc = -1, nodeMcscp2 = -1, nodeMcscp3 = -1, nodeMcscp4 = -1;
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

    if (m_supportSingleChain == true) {
        node3aa = FIMC_IS_VIDEO_30S_NUM;
        node3ac = FIMC_IS_VIDEO_30C_NUM;
        node3ap = FIMC_IS_VIDEO_30P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        nodeMcscp2 = FIMC_IS_VIDEO_M0P_NUM;
        nodeMcscp3 = FIMC_IS_VIDEO_M1P_NUM;
        nodeMcscp4 = FIMC_IS_VIDEO_M2P_NUM;
    } else {
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
        nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
        nodeMcscp3 = FIMC_IS_VIDEO_M3P_NUM;
        nodeMcscp4 = FIMC_IS_VIDEO_M4P_NUM;
    }

    m_initDeviceInfo(INDEX(PIPE_3AA_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_ISP_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_MCSC_REPROCESSING));


    /*
     * 3AA for Reprocessing
     */
    pipeId = INDEX(PIPE_3AA_REPROCESSING);
    previousPipeId = PIPE_FLITE;

    /* 3AS */

    /*
     * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
     */
    if (m_supportPureBayerReprocessing == false && m_flag3aaIspOTF == false)
        flagStreamLeader = false;
    else
        flagStreamLeader = true;

    nodeType = getNodeType(PIPE_3AA_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    m_deviceInfo[pipeId].connectionMode[nodeType] = (unsigned int)m_flagFlite3aaOTF;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(m_getFliteNodenum(), false, flagStreamLeader, m_flagReprocessing);

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
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

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

    /* MCSC2 */
    nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
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

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_constructReprocessingPipes(void)
{
    CLOGI("INFO(%s[%d])", __FUNCTION__, __LINE__);

    int pipeId = -1;

    /* 3AA for Reprocessing */
    pipeId = PIPE_3AA_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraMCPipe(m_cameraId, m_parameters, m_flagReprocessing, &m_deviceInfo[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_3AA_REPROCESSING");

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

    /* GSC3 for Reprocessing */
    pipeId = PIPE_GSC_REPROCESSING3;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeGSC(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_GSC_REPROCESSING3");

    if (m_flagHWFCEnabled == false || (m_flagHWFCEnabled && m_parameters->isHWFCOnDemand())) {
        /* JPEG for Reprocessing */
        pipeId = PIPE_JPEG_REPROCESSING;
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeJpeg(m_cameraId, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_JPEG_REPROCESSING");
    }

    CLOGI("INFO(%s[%d]):pipe ids for reprocessing", __FUNCTION__, __LINE__);
    for (int i = 0; i < MAX_NUM_PIPES; i++) {
        if (m_pipes[i] != NULL) {
            CLOGI("INFO(%s[%d]):-> m_pipes[%d] : PipeId(%d)", __FUNCTION__, __LINE__ , i, m_pipes[i]->getPipeId());
        }
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactory::m_fillNodeGroupInfo(ExynosCameraFrame *frame)
{
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group *node_group_info_temp = NULL;
    size_control_info_t sizeControlInfo;

    int zoom = m_parameters->getZoomLevel();
    int pipeId = -1;
    uint32_t perframePosition = 0;

#ifdef SR_CAPTURE
    if(m_parameters->getSROn()) {
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

    if (m_flagIspMcscOTF == false) {
        node_group_info_temp->capture[perframePosition].request = m_requestISPC;
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISPC_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
        perframePosition++;
    }

    if (m_requestISPP == true) {
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

    node_group_info_temp->capture[perframePosition].request = m_requestMCSC2;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC2_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    node_group_info_temp->capture[perframePosition].request = m_requestMCSC3;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC3_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;
    perframePosition++;

    node_group_info_temp->capture[perframePosition].request = m_requestMCSC4;
    node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC4_REPROCESSING)] - FIMC_IS_VIDEO_BAS_NUM;

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

    updateNodeGroupInfo(
            PIPE_3AA_REPROCESSING,
            m_parameters,
            sizeControlInfo,
            &node_group_info_3aa);
    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA, zoom);

    if (m_supportPureBayerReprocessing == false || m_flag3aaIspOTF == false) {
        updateNodeGroupInfo(
                PIPE_ISP_REPROCESSING,
                m_parameters,
                sizeControlInfo,
                &node_group_info_isp);
        if (m_supportPureBayerReprocessing == true)
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP, zoom);
        else
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_DIRTY_REPROCESSING_ISP, zoom);
    }

    return NO_ERROR;
}

void ExynosCameraFrameReprocessingFactory::m_init(void)
{
    m_flagReprocessing = true;
    m_flagHWFCEnabled = m_parameters->isUseHWFC();
}

}; /* namespace android */
