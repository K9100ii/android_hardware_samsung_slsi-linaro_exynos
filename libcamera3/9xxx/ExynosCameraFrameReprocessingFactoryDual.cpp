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
#define LOG_TAG "ExynosCameraFrameReprocessingFactoryDual"
#include <log/log.h>

#include "ExynosCameraFrameReprocessingFactoryDual.h"

namespace android {
ExynosCameraFrameReprocessingFactoryDual::~ExynosCameraFrameReprocessingFactoryDual()
{
    status_t ret = NO_ERROR;

    ret = destroy();
    if (ret != NO_ERROR)
        CLOGE("destroy fail");
}

status_t ExynosCameraFrameReprocessingFactoryDual::initPipes(void)
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
    uint32_t hwSensorW = 0, hwSensorH = 0;
    uint32_t maxPreviewW = 0, maxPreviewH = 0, hwPreviewW = 0, hwPreviewH = 0;
    uint32_t maxPictureW = 0, maxPictureH = 0, hwPictureW = 0, hwPictureH = 0;
    uint32_t maxThumbnailW = 0, maxThumbnailH = 0;
    uint32_t yuvWidth[ExynosCameraParameters::YUV_MAX] = {0};
    uint32_t yuvHeight[ExynosCameraParameters::YUV_MAX] = {0};
    int bayerFormat = m_parameters->getBayerFormat(PIPE_3AA_REPROCESSING);
    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    camera_pixel_size yuvPixelSize[ExynosCameraParameters::YUV_MAX] = {CAMERA_PIXEL_SIZE_8BIT};
    int dsWidth = MAX_VRA_INPUT_WIDTH;
    int dsHeight = MAX_VRA_INPUT_HEIGHT;
    int dsFormat = m_parameters->getHwVraInputFormat();
    int pictureFormat = m_parameters->getHwPictureFormat();
    camera_pixel_size picturePixelSize = m_parameters->getHwPicturePixelSize();
    struct ExynosConfigInfo *config = m_configurations->getConfig();
    int perFramePos = 0;
    int yuvIndex = -1;
    bool supportJpeg = false;

    memset(&nullPipeInfo, 0, sizeof(camera_pipe_info_t));

#ifdef DEBUG_RAWDUMP
    if (m_configurations->checkBayerDumpEnable()) {
        bayerFormat = CAMERA_DUMP_BAYER_FORMAT;
    }
#endif

    m_parameters->getSize(HW_INFO_HW_SENSOR_SIZE, &hwSensorW, &hwSensorH);
    m_parameters->getSize(HW_INFO_MAX_PREVIEW_SIZE, &maxPreviewW, &maxPreviewH);
    m_parameters->getSize(HW_INFO_HW_PREVIEW_SIZE, &hwPreviewW, &hwPreviewH);
    m_parameters->getSize(HW_INFO_MAX_PICTURE_SIZE, &maxPictureW, &maxPictureH);
    m_parameters->getSize(HW_INFO_HW_PICTURE_SIZE, &hwPictureW, &hwPictureH);
    m_parameters->getSize(HW_INFO_MAX_THUMBNAIL_SIZE, &maxThumbnailW, &maxThumbnailH);
    m_parameters->getPreviewBayerCropSize(&bnsSize, &bayerCropSize, false);

    CLOGI(" MaxPreviewSize(%dx%d), HwPreviewSize(%dx%d)", maxPreviewW, maxPreviewH, hwPreviewW, hwPreviewH);
    CLOGI(" MaxPictureSize(%dx%d), HwPictureSize(%dx%d)", maxPictureW, maxPictureH, hwPictureW, hwPictureH);
    CLOGI(" MaxThumbnailSize(%dx%d)", maxThumbnailW, maxThumbnailH);
    CLOGI(" PreviewBayerCropSize(%dx%d)", bayerCropSize.w, bayerCropSize.h);
    CLOGI(" DS Size %dx%d Format %x Buffer count %d",
            dsWidth, dsHeight, dsFormat, config->current->bufInfo.num_vra_buffers);

    for (int i = ExynosCameraParameters::YUV_STALL_0; i < ExynosCameraParameters::YUV_STALL_MAX; i++) {
        yuvIndex = i % ExynosCameraParameters::YUV_MAX;
        m_configurations->getSize(CONFIGURATION_YUV_SIZE, &yuvWidth[yuvIndex], &yuvHeight[yuvIndex], i);
        yuvFormat[yuvIndex] = m_configurations->getYuvFormat(i);
        yuvPixelSize[yuvIndex] = m_configurations->getYuvPixelSize(i);

        if (m_configurations->getModeValue(CONFIGURATION_YUV_STALL_PORT) == yuvIndex) {
            if ((yuvWidth[yuvIndex] == 0 && yuvHeight[yuvIndex] == 0)
#ifdef SAMSUNG_HIFI_CAPTURE
                || (m_configurations->getMode(CONFIGURATION_LIGHT_CONDITION_ENABLE_MODE) == true)
#endif
            ) {
                m_parameters->getSize(HW_INFO_HW_PICTURE_SIZE, (uint32_t *)&yuvWidth[yuvIndex], (uint32_t *)&yuvHeight[yuvIndex]);
                yuvFormat[yuvIndex] = V4L2_PIX_FMT_NV21;
            }
        }
        CLOGI("YUV_STALL[%d] Size %dx%d Format %x PixelSizeNum %d",
                i, yuvWidth[yuvIndex], yuvHeight[yuvIndex], yuvFormat[yuvIndex], yuvPixelSize[yuvIndex]);
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
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_PURE_REPROCESSING_3AA);

        /* 3AC */
        nodeType = getNodeType(PIPE_3AC_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_3AC_POS;
        bayerFormat = m_parameters->getBayerFormat(PIPE_3AC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwSensorW;
        tempRect.fullH = hwSensorH;
        tempRect.colorFormat = bayerFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

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
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
            /* 3AF */
            nodeType = getNodeType(PIPE_3AF_REPROCESSING);
            perFramePos = 0; //it is dummy info
            // TODO: need to remove perFramePos from the SET_CAPTURE_DEVICE_BASIC_INFO

            /* set v4l2 buffer size */
            tempRect.fullW = MAX_VRA_INPUT_WIDTH;
            tempRect.fullH = MAX_VRA_INPUT_HEIGHT;
            tempRect.colorFormat = m_parameters->getHW3AFdFormat();

            /* set v4l2 video node buffer count */
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

            /* Set capture node default info */
            SET_CAPTURE_DEVICE_BASIC_INFO();
        }

        if (m_parameters->isUse3aaDNG()) {
            /* 3AG */
            nodeType = getNodeType(PIPE_3AG_REPROCESSING);
            perFramePos = 0;
            // TODO: need to remove perFramePos from the SET_CAPTURE_DEVICE_BASIC_INFO
            bayerFormat = m_parameters->getBayerFormat(PIPE_3AG_REPROCESSING);

            /* set v4l2 buffer size */
            tempRect.fullW = hwPictureW;
            tempRect.fullH = hwPictureH;
            tempRect.colorFormat = bayerFormat;

            /* set v4l2 video node bytes per plane */
            pipeInfo[nodeType].bytesPerPlane[0] = getBayerLineSize(tempRect.fullW, bayerFormat);

            /* set v4l2 video node buffer count */
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

            /* Set capture node default info */
            SET_CAPTURE_DEVICE_BASIC_INFO();
        }

        /* setup pipe info to 3AA pipe */
        if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
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
    }


    /*
     * ISP for Reprocessing
     */

    /* ISP */
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
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
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;
        } else if (m_parameters->isSupportZSLInput()) {
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_bayer_buffers;
        } else {
            pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_sensor_buffers;
        }

        /* Set output node default info */
        perFramePos = m_supportPureBayerReprocessing ? PERFRAME_INFO_PURE_REPROCESSING_ISP : PERFRAME_INFO_DIRTY_REPROCESSING_ISP;
        SET_OUTPUT_DEVICE_BASIC_INFO(perFramePos);
    }

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_ISPC_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    /* set YUV pixel size */
    pipeInfo[nodeType].pixelSize = CAMERA_PIXEL_SIZE_PACKED_10BIT;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP_REPROCESSING);
    perFramePos = PERFRAME_REPROCESSING_ISPP_POS;

    /* set v4l2 buffer size */
    tempRect.fullW = hwPictureW;
    tempRect.fullH = hwPictureH;
    tempRect.colorFormat = pictureFormat;

    /* set YUV pixel size */
    pipeInfo[nodeType].pixelSize = picturePixelSize;

    /* set v4l2 video node buffer count */
    pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

    /* Set capture node default info */
    SET_CAPTURE_DEVICE_BASIC_INFO();

    /* setup pipe info to ISP pipe */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
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
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC_REPROCESSING;
        nodeType = getNodeType(PIPE_MCSC_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = V4L2_PIX_FMT_NV16M;

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_hwdis_buffers;

        /* Set output node default info */
        int mcscPerframeInfoIndex = m_supportPureBayerReprocessing ? PERFRAME_INFO_PURE_REPROCESSING_MCSC : PERFRAME_INFO_DIRTY_REPROCESSING_MCSC;
        SET_OUTPUT_DEVICE_BASIC_INFO(mcscPerframeInfoIndex);
    }

    switch (m_parameters->getNumOfMcscOutputPorts()) {
    case 5:
        supportJpeg = true;

        /* Jpeg Thumbnail */
        nodeType = getNodeType(PIPE_MCSC_THUMB_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC_THUMB_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = maxThumbnailW;
        tempRect.fullH = maxThumbnailH;
        tempRect.colorFormat = pictureFormat;

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* Jpeg Main */
        nodeType = getNodeType(PIPE_MCSC_JPEG_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC_JPEG_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = hwPictureW;
        tempRect.fullH = hwPictureH;
        tempRect.colorFormat = pictureFormat;

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* Not break; */
    case 3:
        /*
         * If the number of output was lower than 5,
         * try to use another reprocessing factory for jpeg
         * to support full device
         */
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC2_POS;
        yuvIndex = ExynosCameraParameters::YUV_STALL_2 % ExynosCameraParameters::YUV_MAX;

        /* set v4l2 buffer size */
        tempRect.fullW = yuvWidth[yuvIndex];
        tempRect.fullH = yuvHeight[yuvIndex];
        tempRect.colorFormat = yuvFormat[yuvIndex];

        /* set v4l2 format pixel size */
        pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
        /* pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW; */

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* MCSC1 */
        nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC1_POS;
        yuvIndex = ExynosCameraParameters::YUV_STALL_1 % ExynosCameraParameters::YUV_MAX;

        /* set v4l2 buffer size */
        tempRect.fullW = yuvWidth[yuvIndex];
        tempRect.fullH = yuvHeight[yuvIndex];
        tempRect.colorFormat = yuvFormat[yuvIndex];

        /* set v4l2 format pixel size */
        pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
        /* pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW; */

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

        /* Not break; */
    case 1:
        /* MCSC0 */
        nodeType = getNodeType(PIPE_MCSC0_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC0_POS;
        yuvIndex = ExynosCameraParameters::YUV_STALL_0 % ExynosCameraParameters::YUV_MAX;

        /* set v4l2 buffer size */
        tempRect.fullW = yuvWidth[yuvIndex];
        tempRect.fullH = yuvHeight[yuvIndex];
        tempRect.colorFormat = yuvFormat[yuvIndex];

        /* set v4l2 format pixel size */
        pipeInfo[nodeType].pixelSize = yuvPixelSize[yuvIndex];

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_picture_buffers;
        /* pipeInfo[nodeType].bytesPerPlane[0] = tempRect.fullW; */

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();
        break;
    default:
        CLOG_ASSERT("invalid MCSC output(%d)", m_parameters->getNumOfMcscOutputPorts());
        break;
    }

    if(supportJpeg == true && m_flagHWFCEnabled == true) {
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

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

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

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

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

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

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

        /* set YUV pixel size */
        pipeInfo[nodeType].pixelSize = picturePixelSize;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_reprocessing_buffers;

        /* Set capture node default info */
        pipeInfo[nodeType].rectInfo = tempRect;
        pipeInfo[nodeType].bufInfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        pipeInfo[nodeType].bufInfo.memory = V4L2_CAMERA_MEMORY_TYPE;
    }

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        /* MCSC5 */
        nodeType = getNodeType(PIPE_MCSC5_REPROCESSING);
        perFramePos = PERFRAME_REPROCESSING_MCSC5_POS;

        /* set v4l2 buffer size */
        tempRect.fullW = dsWidth;
        tempRect.fullH = dsHeight;
        tempRect.colorFormat = dsFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

        /* Set capture node default info */
        SET_CAPTURE_DEVICE_BASIC_INFO();

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

    /* VRA */
    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_VRA_REPROCESSING;
        nodeType = getNodeType(PIPE_VRA_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = dsWidth;
        tempRect.fullH = dsHeight;
        tempRect.colorFormat = dsFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_VRA);
    }

    ret = m_pipes[INDEX(pipeId)]->setupPipe(pipeInfo, m_sensorIds[INDEX(pipeId)]);
    if (ret != NO_ERROR) {
        CLOGE("ISP setupPipe fail, ret(%d)", ret);
        /* TODO: exception handling */
        return INVALID_OPERATION;
    }

    /* VRA */
    if ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) &&
        (m_flagMcscVraOTF != HW_CONNECTION_MODE_M2M)) {
        /* clear pipeInfo for next setupPipe */
        for (int i = 0; i < MAX_NODE; i++)
            pipeInfo[i] = nullPipeInfo;

        pipeId = PIPE_VRA_REPROCESSING;
        nodeType = getNodeType(PIPE_VRA_REPROCESSING);

        /* set v4l2 buffer size */
        tempRect.fullW = MAX_VRA_INPUT_WIDTH;
        tempRect.fullH = MAX_VRA_INPUT_HEIGHT;
        tempRect.colorFormat = dsFormat;

        /* set v4l2 video node buffer count */
        pipeInfo[nodeType].bufInfo.count = config->current->bufInfo.num_vra_buffers;

        /* Set output node default info */
        SET_OUTPUT_DEVICE_BASIC_INFO(PERFRAME_INFO_VRA);

        ret = m_pipes[pipeId]->setupPipe(pipeInfo, m_sensorIds[pipeId]);
        if (ret != NO_ERROR) {
            CLOGE("MCSC setupPipe fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    m_frameCount = 0;

    ret = m_transitState(FRAME_FACTORY_STATE_INIT);

    return ret;
}

status_t ExynosCameraFrameReprocessingFactoryDual::startPipes(void)
{
    status_t ret = NO_ERROR;
    CLOGI("");

    if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL &&
            m_configurations->getMode(CONFIGURATION_DUAL_MODE) == true) {
        ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("SYNC_REPROCESSING start fail, ret(%d)", ret);
            return INVALID_OPERATION;
        }
    }

    /* VRA Reprocessing */
    if ((m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) ||
        (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M)){
        ret = m_pipes[INDEX(PIPE_VRA_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("VRA start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* MCSC Reprocessing */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->start();
        if (ret != NO_ERROR) {
            CLOGE("MCSC start fail, ret(%d)", ret);
            /* TODO: exception handling */
            return INVALID_OPERATION;
        }
    }

    /* ISP Reprocessing */
    if (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
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

    ret = m_transitState(FRAME_FACTORY_STATE_RUN);
    if (ret != NO_ERROR) {
        CLOGE("Failed to transitState. ret %d", ret);
        return ret;
    }

    CLOGI("Starting Reprocessing [SCC>ISP] Success!");

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactoryDual::stopPipes(void)
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
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("ISP stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* MCSC Reprocessing Thread stop */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_VRA_REPROCESSING)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_VRA_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("VRA stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL
        && m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("SYNC_REPROCESSING stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        if (m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] != NULL
            && m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->isThreadRunning() == true) {
            ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->stopThread();
            if (ret != NO_ERROR) {
                CLOGE("FUSION_REPROCESSING stopThread fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    if (m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_PP_UNI_REPROCESSING stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING2)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING2)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_PP_UNI_REPROCESSING2 stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }
#endif

    if (m_pipes[INDEX(PIPE_GSC_REPROCESSING2)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING2)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC_REPROCESSING2 stopThread fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->isThreadRunning() == true) {
        ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->stopThread();
        if (ret != NO_ERROR) {
            CLOGE("PIPE_GSC_REPROCESSING3 stopThread fail, ret(%d)", ret);
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
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[INDEX(PIPE_ISP_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("ISP stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    /* MCSC Reprocessing stop */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        ret = m_pipes[INDEX(PIPE_MCSC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("MCSC stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_VRA_REPROCESSING)]->flagStart() == true) {
        ret = m_pipes[INDEX(PIPE_VRA_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("VRA stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_pipes[INDEX(PIPE_SYNC_REPROCESSING)] != NULL) {
        ret = m_pipes[INDEX(PIPE_SYNC_REPROCESSING)]->stop();
        if (ret != NO_ERROR) {
            CLOGE("SYNC_REPROCESSING stop fail, ret(%d)", ret);
            /* TODO: exception handling */
            funcRet |= ret;
        }
    }

    if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        if (m_pipes[INDEX(PIPE_FUSION_REPROCESSING)] != NULL) {
            ret = m_pipes[INDEX(PIPE_FUSION_REPROCESSING)]->stop();
            if (ret < 0) {
                CLOGE("FUSION_REPROCESSING stop fail, ret(%d)", ret);
                /* TODO: exception handling */
                funcRet |= ret;
            }
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    /* UNI Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_PP_UNI_REPROCESSING stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    /* UNI2 Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_PP_UNI_REPROCESSING2)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_PP_UNI_REPROCESSING2 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }
#endif

    /* GSC2 Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING2)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC_REPROCESSING2 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
    }

    /* GSC3 Reprocessing stop */
    ret = m_pipes[INDEX(PIPE_GSC_REPROCESSING3)]->stop();
    if (ret != NO_ERROR) {
        CLOGE("PIPE_GSC_REPROCESSING3 stop fail, ret(%d)", ret);
        /* TODO: exception handling */
        funcRet |= ret;
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

ExynosCameraFrameSP_sptr_t ExynosCameraFrameReprocessingFactoryDual::createNewFrame(uint32_t frameCount, bool useJpegFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraFrameEntity *newEntity[MAX_NUM_PIPES] = {0};
    if (frameCount <= 0) {
        frameCount = m_frameCount;
    }
    ExynosCameraFrameSP_sptr_t frame =  m_frameMgr->createFrame(m_configurations, frameCount, FRAME_TYPE_REPROCESSING);
    int requestEntityCount = 0;
    int pipeId = -1;
    int parentPipeId = PIPE_3AA_REPROCESSING;

    if (frame == NULL) {
        CLOGE("frame is NULL");
        return NULL;
    }

    m_setBaseInfoToFrame(frame);

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
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_ISP_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        if (m_supportPureBayerReprocessing == true) {
            frame->addChildEntity(newEntity[INDEX(parentPipeId)], newEntity[INDEX(pipeId)], INDEX(PIPE_3AP_REPROCESSING));
        } else {
            frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        }
    }

    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_PARTIAL, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
    frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
    requestEntityCount++;

    /* set MCSC pipe to linkageList */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = PIPE_MCSC_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set VRA pipe to linkageList */
    if (((m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)
        && m_request[INDEX(PIPE_MCSC5_REPROCESSING)] == true && m_request[INDEX(PIPE_VRA_REPROCESSING)] == true) ||
        ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M)
        && m_request[INDEX(PIPE_3AF_REPROCESSING)] == true && m_request[INDEX(PIPE_VRA_REPROCESSING)] == true)) {
        pipeId = PIPE_VRA_REPROCESSING;
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_ONLY, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        requestEntityCount++;
    }

    /* set Sync pipe to linkageList */
    pipeId = PIPE_SYNC_REPROCESSING;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set Fusion pipe to linkageList */
    if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        pipeId = PIPE_FUSION_REPROCESSING;
        if (m_request[INDEX(pipeId)] == true) {
            newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
            frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
            frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
            frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
            requestEntityCount++;
        }
    }

#ifdef SAMSUNG_TN_FEATURE
    pipeId = PIPE_PP_UNI_REPROCESSING;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    pipeId = PIPE_PP_UNI_REPROCESSING2;
    if (m_request[INDEX(pipeId)] == true) {
        newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
        frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }
#endif

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (m_parameters->needGSCForCapture(m_cameraId) == true) {
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set JPEG pipe to linkageList */
    pipeId = PIPE_JPEG_REPROCESSING;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);
    if (m_flagHWFCEnabled == false || useJpegFlag == true) {
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_BUFFER, (enum pipeline)pipeId);
        frame->setPipeIdForResultUpdate(ExynosCameraFrame::RESULT_UPDATE_TYPE_ALL, (enum pipeline)pipeId);
        requestEntityCount++;
    }

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING2;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

    /* set GSC pipe to linkageList */
    pipeId = PIPE_GSC_REPROCESSING3;
    newEntity[INDEX(pipeId)] = new ExynosCameraFrameEntity(pipeId, ENTITY_TYPE_INPUT_OUTPUT, ENTITY_BUFFER_FIXED);
    frame->addSiblingEntity(NULL, newEntity[INDEX(pipeId)]);

    ret = m_initPipelines(frame);
    if (ret != NO_ERROR) {
        CLOGE("m_initPipelines fail, ret(%d)", ret);
    }

    frame->setNumRequestPipe(requestEntityCount);
    frame->setParameters(m_parameters);
    frame->setCameraId(m_cameraId);

    m_fillNodeGroupInfo(frame);

    m_frameCount++;

    return frame;
}

status_t ExynosCameraFrameReprocessingFactoryDual::m_setupConfig(void)
{
    CLOGI("");

    int pipeId = -1;
    int node3aa = -1, node3ac = -1, node3ap = -1, node3af = -1;
    int nodeIsp = -1, nodeIspc = -1, nodeIspp = -1;
    int nodeMcsc = -1, nodeMcscp0 = -1, nodeMcscp1 = -1;
    int nodeMcscp2 = -1, nodeMcscp3 = -1, nodeMcscp4 = -1;
    int nodeMcscpDs = -1;
    int nodeVra = -1;
    int previousPipeId = -1;
    int vraSrcPipeId = -1;
    enum NODE_TYPE nodeType = INVALID_NODE;
    bool flagStreamLeader = false;
    bool supportJpeg = false;

    m_flagFlite3aaOTF = m_parameters->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
    m_flag3aaIspOTF = m_parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_ISP_REPROCESSING);
    m_flagIspMcscOTF = m_parameters->getHwConnectionMode(PIPE_ISP_REPROCESSING, PIPE_MCSC_REPROCESSING);
    m_flag3aaVraOTF = m_parameters->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_VRA_REPROCESSING);
    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_NONE)
        m_flagMcscVraOTF = m_parameters->getHwConnectionMode(PIPE_MCSC_REPROCESSING, PIPE_VRA_REPROCESSING);


    m_supportReprocessing = m_parameters->isReprocessing();
    m_supportPureBayerReprocessing = m_parameters->getUsePureBayerReprocessing();

    m_request[INDEX(PIPE_3AP_REPROCESSING)] = (m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M);
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        m_request[INDEX(PIPE_ISPC_REPROCESSING)] = true;
    }

    /* jpeg can be supported in this factory depending on mcsc output port */
    if (m_parameters->getNumOfMcscOutputPorts() > 3) {
        m_request[INDEX(PIPE_MCSC_JPEG_REPROCESSING)] = true;
        m_request[INDEX(PIPE_MCSC_THUMB_REPROCESSING)] = true;
        if (m_flagHWFCEnabled == true) {
            m_request[INDEX(PIPE_HWFC_JPEG_SRC_REPROCESSING)] = true;
            m_request[INDEX(PIPE_HWFC_JPEG_DST_REPROCESSING)] = true;
            m_request[INDEX(PIPE_HWFC_THUMB_SRC_REPROCESSING)] = true;
        }
    }

    switch (m_parameters->getHwChainType()) {
    case HW_CHAIN_TYPE_DUAL_CHAIN:
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        node3af = FIMC_IS_VIDEO_31F_NUM;
        nodeIsp = FIMC_IS_VIDEO_I1S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I1C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I1P_NUM;
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
        break;
    case HW_CHAIN_TYPE_SEMI_DUAL_CHAIN:
        node3aa = FIMC_IS_VIDEO_31S_NUM;
        node3ac = FIMC_IS_VIDEO_31C_NUM;
        node3ap = FIMC_IS_VIDEO_31P_NUM;
        node3af = FIMC_IS_VIDEO_31F_NUM;
        nodeIsp = FIMC_IS_VIDEO_I0S_NUM;
        nodeIspc = FIMC_IS_VIDEO_I0C_NUM;
        nodeIspp = FIMC_IS_VIDEO_I0P_NUM;
        break;
    default:
        CLOGE("invalid hw chain type(%d)", m_parameters->getHwChainType());
        break;
    }

    if (m_parameters->getNumOfMcscInputPorts() > 1)
#if 0//def USE_DUAL_CAMERA // HACK??
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;
#else
        nodeMcsc = FIMC_IS_VIDEO_M1S_NUM;
#endif
    else
        nodeMcsc = FIMC_IS_VIDEO_M0S_NUM;

    nodeMcscp0 = FIMC_IS_VIDEO_M0P_NUM;
    nodeMcscp1 = FIMC_IS_VIDEO_M1P_NUM;
    nodeMcscp2 = FIMC_IS_VIDEO_M2P_NUM;
    nodeMcscp3 = FIMC_IS_VIDEO_M3P_NUM;
    nodeMcscp4 = FIMC_IS_VIDEO_M4P_NUM;
    nodeVra = FIMC_IS_VIDEO_VRA_NUM;

    switch (m_parameters->getNumOfMcscOutputPorts()) {
    case 5:
        nodeMcscpDs = FIMC_IS_VIDEO_M5P_NUM;
        break;
    case 3:
        nodeMcscpDs = FIMC_IS_VIDEO_M3P_NUM;
        break;
    default:
        CLOGE("invalid output port(%d)", m_parameters->getNumOfMcscOutputPorts());
        break;
    }

    m_initDeviceInfo(INDEX(PIPE_3AA_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_ISP_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_MCSC_REPROCESSING));
    m_initDeviceInfo(INDEX(PIPE_VRA_REPROCESSING));

    /*
     * 3AA for Reprocessing
     */
    pipeId = INDEX(PIPE_3AA_REPROCESSING);
    previousPipeId = PIPE_FLITE;

    /*
     * If dirty bayer is used for reprocessing, the ISP video node is leader in the reprocessing stream.
     */
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        flagStreamLeader = false;
    } else {
        flagStreamLeader = true;
    }

    /* 3AS */
    nodeType = getNodeType(PIPE_3AA_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_3AA_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3aa;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    int fliteNodeNum = getFliteNodenum(m_cameraId);
    m_sensorIds[pipeId][nodeType]  = m_getSensorId(getFliteCaptureNodenum(m_cameraId, fliteNodeNum), false, flagStreamLeader, m_flagReprocessing);

    // Restore leader flag
    flagStreamLeader = false;

    /* 3AC */
    nodeType = getNodeType(PIPE_3AC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ac;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_OPT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* 3AP */
    nodeType = getNodeType(PIPE_3AP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = node3ap;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
        int vraPipeId = INDEX(PIPE_VRA_REPROCESSING);
        /* 3AF */
        nodeType = getNodeType(PIPE_3AF_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_3AF_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = node3af;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_3AA_FD", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_3AA_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /*
         * VRA
         */
        previousPipeId = pipeId;

        vraSrcPipeId = INDEX(PIPE_3AF_REPROCESSING);
        nodeType = getNodeType(PIPE_VRA_REPROCESSING);
        m_deviceInfo[vraPipeId].pipeId[nodeType]  = PIPE_VRA;
        m_deviceInfo[vraPipeId].nodeNum[nodeType] = nodeVra;
        m_deviceInfo[vraPipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[vraPipeId].nodeName[nodeType], "VRA_OUTPUT_REPROCESSING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[vraPipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flag3aaVraOTF, flagStreamLeader, m_flagReprocessing);
    }

    /*
     * ISP for Reprocessing
     */
    previousPipeId = pipeId;
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_ISP_REPROCESSING);
    }

    if (m_supportPureBayerReprocessing == false) {
        /* Set leader flag on ISP pipe for Dirty bayer reprocessing */
        flagStreamLeader = true;
    }
    /* ISPS */
    nodeType = getNodeType(PIPE_ISP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIsp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_3AP_REPROCESSING)], m_flag3aaIspOTF, flagStreamLeader, m_flagReprocessing);

    // Restore leader flag
    flagStreamLeader = false;

    /* ISPC */
    nodeType = getNodeType(PIPE_ISPC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_M2M", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /* ISPP */
    nodeType = getNodeType(PIPE_ISPP_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_ISPP_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeIspp;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_ISP_CAPTURE_OTF", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_ISP_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

    /*
     * MCSC for Reprocessing
     */
    previousPipeId = pipeId;

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_MCSC_REPROCESSING);
    }

    /* MCSC */
    nodeType = getNodeType(PIPE_MCSC_REPROCESSING);
    m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_REPROCESSING;
    m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcsc;
    m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
    strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
    m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(PIPE_ISPC_REPROCESSING)], m_flagIspMcscOTF, flagStreamLeader, m_flagReprocessing);

    switch (m_parameters->getNumOfMcscOutputPorts()) {
    case 5:
        supportJpeg = true;

        /* MCSC4 */
        nodeType = getNodeType(PIPE_MCSC_THUMB_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_THUMB_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp4;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_THUMBNAIL", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* MCSC3 */
        nodeType = getNodeType(PIPE_MCSC_JPEG_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC_JPEG_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp3;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_MAIN", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Not break; */
    case 3:
        /* MCSC2 */
        nodeType = getNodeType(PIPE_MCSC2_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC2_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp2;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_2", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* MCSC1 */
        nodeType = getNodeType(PIPE_MCSC1_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC1_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp1;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_1", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Not break; */
    case 1:
        /* MCSC0 */
        nodeType = getNodeType(PIPE_MCSC0_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC0_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscp0;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_SERVICE_GRALLOC_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_CAPTURE_YUV_0", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
        break;
    default:
        CLOG_ASSERT("invalid MCSC output(%d)", m_parameters->getNumOfMcscOutputPorts());
        break;
    }

    if (supportJpeg == true && m_flagHWFCEnabled == true) {
        /* JPEG Src */
        nodeType = getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Src */
        nodeType = getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_SRC_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_SRC", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_SRC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* JPEG Dst */
        nodeType = getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_JPEG_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_JPEG_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_JPEG_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_JPEG_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

        /* Thumbnail Dst  */
        nodeType = getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_HWFC_THUMB_DST_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = FIMC_IS_VIDEO_HWFC_THUMB_NUM;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "HWFC_THUMBNAIL_DST", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_HWFC_THUMB_DST_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);
    }

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        /* MCSC5 */
        nodeType = getNodeType(PIPE_MCSC5_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType] = PIPE_MCSC5_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeMcscpDs;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "MCSC_DS_REPROCESSING", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[pipeId].nodeNum[getNodeType(PIPE_MCSC_REPROCESSING)], true, flagStreamLeader, m_flagReprocessing);

#ifdef SAMSUNG_TN_FEATURE
        /* PostProcessing for Reprocessing */
        m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
        m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING2)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
#endif

        /*
         * VRA
         */
        previousPipeId = pipeId;
        vraSrcPipeId = PIPE_MCSC5_REPROCESSING;
        pipeId = INDEX(PIPE_VRA_REPROCESSING);

        nodeType = getNodeType(PIPE_VRA_REPROCESSING);
        m_deviceInfo[pipeId].pipeId[nodeType]  = PIPE_VRA_REPROCESSING;
        m_deviceInfo[pipeId].nodeNum[nodeType] = nodeVra;
        m_deviceInfo[pipeId].bufferManagerType[nodeType] = BUFFER_MANAGER_ION_TYPE;
        strncpy(m_deviceInfo[pipeId].nodeName[nodeType], "REPROCESSING_VRA_OUTPUT", EXYNOS_CAMERA_NAME_STR_SIZE - 1);
        m_sensorIds[pipeId][nodeType] = m_getSensorId(m_deviceInfo[previousPipeId].nodeNum[getNodeType(vraSrcPipeId)], m_flagMcscVraOTF, flagStreamLeader, m_flagReprocessing);
    }

    /* GSC for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* GSC2 for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING2)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* GSC3 for Reprocessing */
    m_nodeNums[INDEX(PIPE_GSC_REPROCESSING3)][OUTPUT_NODE] = PICTURE_GSC_NODE_NUM;

    /* JPEG for Reprocessing */
    m_nodeNums[INDEX(PIPE_JPEG_REPROCESSING)][OUTPUT_NODE] = -1;

#ifdef SAMSUNG_TN_FEATURE
    /* PostProcessing for Reprocessing */
    m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
    m_nodeNums[INDEX(PIPE_PP_UNI_REPROCESSING2)][OUTPUT_NODE] = UNIPLUGIN_NODE_NUM;
#endif

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactoryDual::m_constructPipes(void)
{
    CLOGI("");

    status_t ret = NO_ERROR;
    int pipeId = -1;

    ret = ExynosCameraFrameReprocessingFactory::m_constructPipes();
    if (ret != NO_ERROR) {
        CLOGE("Construct Pipes fail! ret(%d)", ret);
        return ret;
    }

    /* SYNC Pipe for Dual */
    pipeId = PIPE_SYNC_REPROCESSING;
    m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)new ExynosCameraPipeSync(m_cameraId, m_configurations, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
    m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
    m_pipes[INDEX(pipeId)]->setPipeName("PIPE_SYNC_REPROCESSING");

    CLOGI("m_pipes[%d] : PipeId(%d)" , INDEX(pipeId), m_pipes[INDEX(pipeId)]->getPipeId());

    if (m_configurations->getDualReprocessingMode() == DUAL_REPROCESSING_MODE_SW) {
        /* Fusion Pipe for Dual */
        pipeId = PIPE_FUSION_REPROCESSING;

#ifdef SAMSUNG_TN_FEATURE
        ExynosCameraPipeFusion *fusionPipe = new ExynosCameraPipeFusion(m_cameraId, m_configurations, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe *)fusionPipe;
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_FUSION_REPROCESSING");

        if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM) {
#ifdef SAMSUNG_DUAL_ZOOM_CAPTURE
            ExynosCameraFusionWrapper *fusionWrapper = ExynosCameraSingleton<ExynosCameraFusionZoomCaptureWrapper>::getInstance();
            fusionPipe->setFusionWrapper(fusionWrapper);
#endif
        } else if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_PORTRAIT) {
#ifdef SAMSUNG_DUAL_PORTRAIT_SOLUTION
            ExynosCameraFusionWrapper *bokehWrapper = ExynosCameraSingleton<ExynosCameraBokehCaptureWrapper>::getInstance();
            fusionPipe->setBokehWrapper(bokehWrapper);
#endif
        }
#else
        ExynosCameraPipePlugIn *fusionPipe = new ExynosCameraPipePlugIn(m_cameraId, m_configurations, m_parameters, m_flagReprocessing, m_nodeNums[INDEX(pipeId)]);
        m_pipes[INDEX(pipeId)] = (ExynosCameraPipe*)fusionPipe;
        m_pipes[INDEX(pipeId)]->setPipeId(pipeId);
        m_pipes[INDEX(pipeId)]->setPipeName("PIPE_FUSION_REPROCESSING");
#endif

        CLOGI("m_pipes[%d] : PipeId(%d)", INDEX(pipeId), m_pipes[INDEX(pipeId)]->getPipeId());
    }

    return NO_ERROR;
}

status_t ExynosCameraFrameReprocessingFactoryDual::m_fillNodeGroupInfo(ExynosCameraFrameSP_sptr_t frame)
{
    camera2_node_group node_group_info_3aa;
    camera2_node_group node_group_info_isp;
    camera2_node_group node_group_info_mcsc;
    camera2_node_group node_group_info_vra;
    camera2_node_group *node_group_info_temp = NULL;

    float zoomRatio = m_configurations->getZoomRatio();
    int pipeId = -1;
    int nodePipeId = -1;
    uint32_t perframePosition = 0;

    int yuvFormat[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvWidth[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvHeight[ExynosCameraParameters::YUV_MAX] = {0};
    int yuvIndex = -1;

    for (int i = ExynosCameraParameters::YUV_STALL_0; i < ExynosCameraParameters::YUV_STALL_MAX; i++) {
        yuvIndex = i % ExynosCameraParameters::YUV_MAX;
        m_parameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&yuvWidth[yuvIndex], (uint32_t *)&yuvHeight[yuvIndex], i);
        yuvFormat[yuvIndex] = m_configurations->getYuvFormat(i);
    }

    memset(&node_group_info_3aa, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_isp, 0x0, sizeof(camera2_node_group));
    memset(&node_group_info_mcsc, 0x0, sizeof(camera2_node_group));

    /* 3AA for Reprocessing */
    if (m_supportPureBayerReprocessing == true) {
        pipeId = INDEX(PIPE_3AA_REPROCESSING);
        node_group_info_temp = &node_group_info_3aa;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getBayerFormat(pipeId);

        nodePipeId = PIPE_3AC_REPROCESSING;
        if (m_request[INDEX(nodePipeId)] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
            perframePosition++;
        }

        nodePipeId = PIPE_3AG_REPROCESSING;
        if (m_request[INDEX(nodePipeId)] == true) {
            node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
            perframePosition++;
        }

        nodePipeId = PIPE_3AP_REPROCESSING;
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getBayerFormat(nodePipeId);
        perframePosition++;
    }

    /* VRA */
    if (m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) {
        nodePipeId = PIPE_3AF_REPROCESSING;
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwVraInputFormat();
        perframePosition++;
    }

    /* ISP for Reprocessing */
    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_ISP_REPROCESSING);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_isp;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getBayerFormat(pipeId);
    }

    nodePipeId = PIPE_ISPC_REPROCESSING;
    if (node_group_info_temp != NULL) {
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwPictureFormat();
        perframePosition++;
    }

    nodePipeId = PIPE_ISPP_REPROCESSING;
    if (m_request[INDEX(nodePipeId)] == true && node_group_info_temp != NULL) {
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwPictureFormat();
        perframePosition++;
    }

    /* MCSC for Reprocessing */
    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        pipeId = INDEX(PIPE_MCSC_REPROCESSING);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_mcsc;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getHwPictureFormat();
    }

    if (node_group_info_temp != NULL) {
        nodePipeId = PIPE_MCSC0_REPROCESSING;
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        yuvIndex = (ExynosCameraParameters::YUV_STALL_0 % ExynosCameraParameters::YUV_MAX);
        node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
        perframePosition++;

        nodePipeId = PIPE_MCSC1_REPROCESSING;
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        yuvIndex = (ExynosCameraParameters::YUV_STALL_1 % ExynosCameraParameters::YUV_MAX);
        node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
        perframePosition++;

        nodePipeId = PIPE_MCSC2_REPROCESSING;
        node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
        node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
        yuvIndex = (ExynosCameraParameters::YUV_STALL_2 % ExynosCameraParameters::YUV_MAX);
        node_group_info_temp->capture[perframePosition].pixelformat = yuvFormat[yuvIndex];
        perframePosition++;

        if (m_parameters->getNumOfMcscOutputPorts() > 3) {
            nodePipeId = PIPE_MCSC_JPEG_REPROCESSING;
            node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwPictureFormat();
            perframePosition++;

            nodePipeId = PIPE_MCSC_THUMB_REPROCESSING;
            node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwPictureFormat();
            perframePosition++;
        }

        if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
            nodePipeId = PIPE_MCSC5_REPROCESSING;
            node_group_info_temp->capture[perframePosition].request = m_request[INDEX(nodePipeId)];
            node_group_info_temp->capture[perframePosition].vid = m_deviceInfo[pipeId].nodeNum[getNodeType(nodePipeId)] - FIMC_IS_VIDEO_BAS_NUM;
            node_group_info_temp->capture[perframePosition].pixelformat = m_parameters->getHwVraInputFormat();
            perframePosition++;
        }
    }

    /* VRA */
    if ((m_flag3aaVraOTF == HW_CONNECTION_MODE_M2M) ||
        (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M)) {
        pipeId = INDEX(PIPE_VRA_REPROCESSING);
        perframePosition = 0;
        node_group_info_temp = &node_group_info_vra;
        node_group_info_temp->leader.request = 1;
        node_group_info_temp->leader.pixelformat = m_parameters->getHwVraInputFormat();
    }

    frame->setZoomRatio(zoomRatio);

    updateNodeGroupInfo(
            PIPE_3AA_REPROCESSING,
            frame,
            &node_group_info_3aa);
    frame->storeNodeGroupInfo(&node_group_info_3aa, PERFRAME_INFO_PURE_REPROCESSING_3AA);

    if (m_supportPureBayerReprocessing == false
        || m_flag3aaIspOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_ISP_REPROCESSING,
                frame,
                &node_group_info_isp);
        if (m_supportPureBayerReprocessing == true)
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_PURE_REPROCESSING_ISP);
        else
            frame->storeNodeGroupInfo(&node_group_info_isp, PERFRAME_INFO_DIRTY_REPROCESSING_ISP);
    }

    if (m_flagIspMcscOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_MCSC_REPROCESSING,
                frame,
                &node_group_info_mcsc);
        if (m_supportPureBayerReprocessing == true) {
            frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_PURE_REPROCESSING_MCSC);
        } else {
            frame->storeNodeGroupInfo(&node_group_info_mcsc, PERFRAME_INFO_DIRTY_REPROCESSING_MCSC);
        }
    }

    if (m_flagMcscVraOTF == HW_CONNECTION_MODE_M2M) {
        updateNodeGroupInfo(
                PIPE_VRA_REPROCESSING,
                frame,
                &node_group_info_vra);
        if (m_supportPureBayerReprocessing == true)
            frame->storeNodeGroupInfo(&node_group_info_vra, PERFRAME_INFO_PURE_REPROCESSING_VRA);
        else
            frame->storeNodeGroupInfo(&node_group_info_vra, PERFRAME_INFO_DIRTY_REPROCESSING_VRA);
    }

    return NO_ERROR;
}

void ExynosCameraFrameReprocessingFactoryDual::m_init(void)
{
    m_flagReprocessing = true;
}
}; /* namespace android */
