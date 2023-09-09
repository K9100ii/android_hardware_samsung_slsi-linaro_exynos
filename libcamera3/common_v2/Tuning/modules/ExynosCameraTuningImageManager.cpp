#define LOG_TAG "ExynosCameraTuningImageManager"

#include "ExynosCameraTuningModule.h"
#include "ExynosCameraTuningCommandIdList.h"

#include "../ExynosCameraTuner.h"

namespace android {

ExynosCameraTuningImageManager::ExynosCameraTuningImageManager()
{
    CLOGD2("");

    m_imageState = T_IMAGE_STATE_WAIT;
}

ExynosCameraTuningImageManager::~ExynosCameraTuningImageManager()
{
}

status_t ExynosCameraTuningImageManager::m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd)
{
    status_t ret = NO_ERROR;
    CLOGD2("group(%d), command(%x)", cmd->getCommandGroup(), cmd->getData()->commandID);

    uint8_t writeData[SIMMIAN_CTRL_MAXPACKET] = {0,};
    uint32_t writeSize = 0;
    int writeRet = -1;
    uint32_t index = cmd->getData()->moduleCmd.subCommand;
    uint32_t length = cmd->getData()->moduleCmd.length;
    uint8_t indexL = (uint8_t)(index & 0x00ff);

    ExynosCameraParameters* parameters = ExynosCameraTuner::getInstance()->getParameters();
    ExynosCameraConfigurations* configurations = ExynosCameraTuner::getInstance()->getConfigurations();

    if (cmd->getCommandGroup() == ExynosCameraTuningCommand::WRITE_IMAGE) {
        switch(cmd->getData()->commandID) {
        case TUNE_IMAGE_INTERNAL_CMD_READY:
            break;
        case TUNE_IMAGE_INTERNAL_CMD_UPDATE:
            CLOGD2("TUNE_IMAGE_INTERNAL_CMD_UPDATE");
            if (m_imageState == T_IMAGE_STATE_READ_SIZE) {
                ExynosCameraTuner::getInstance()->getImage();
                m_imageState = T_IMAGE_STATE_READ_DONE;
            }
            break;
        case TUNE_IMAGE_INTERNAL_CMD_WRITE:
            CLOGD2("TUNE_IMAGE_INTERNAL_CMD_WRITE(Size:%d, Addr:%lx)", length, cmd->getData()->commandAddr);
            ExynosCameraTuner::getInstance()->setImage();
            break;
        default:
            break;
        }
    }
    else if (cmd->getCommandGroup() == ExynosCameraTuningCommand::READ_IMAGE) {
        switch(cmd->getData()->commandID) {
        case TUNE_IMAGE_INTERNAL_CMD_FPGA_CTRL:
            CLOGD2("TUNE_IMAGE_INTERNAL_CMD_FPGA_CTRL(%x)", indexL);
            if (parameters != NULL
                && m_imageState == T_IMAGE_STATE_READY) {
                uint32_t pipeId = 0;
                int imageW = 0, imageH = 0;
                int sensorMarginW = 0, sensorMarginH = 0;
                uint32_t imageSize = 0;
                uint32_t imageFormat = 0;
                ExynosRect bdsRect;
                unsigned int bpp = 0;
                unsigned int planeCount = 1;
                unsigned int planeSize[EXYNOS_CAMERA_BUFFER_MAX_PLANES] = {0};

                for(int i = 0; i < MAX_NUM_PIPES; i++) {
                    if (ExynosCameraTuner::getInstance()->getRequest(i) == true) {
                        pipeId = i;
                        break;
                    }
                }

                switch (pipeId) {
                case PIPE_VC0:
                    parameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t*)&imageW, (uint32_t*)&imageH);
                    imageSize = getBayerPlaneSize(imageW, imageH, parameters->getBayerFormat(pipeId));
                    break;
                case PIPE_3AC:
                    parameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t*)&imageW, (uint32_t*)&imageH);
                    parameters->getSize(HW_INFO_SENSOR_MARGIN_SIZE, (uint32_t*)&sensorMarginW, (uint32_t*)&sensorMarginH);
                    imageW -= sensorMarginW;
                    imageH -= sensorMarginH;
                    imageSize = getBayerPlaneSize(imageW, imageH, parameters->getBayerFormat(pipeId));
                    break;
                case PIPE_ISPC:
                    parameters->getPreviewBdsSize(&bdsRect);
                    imageW = bdsRect.w;
                    imageH = bdsRect.h;
                    imageFormat = V4L2_PIX_FMT_YUYV;
                    getV4l2FormatInfo(imageFormat, &bpp, &planeCount);
                    getYuvPlaneSize(imageFormat, planeSize, imageW, imageH);
                    for (uint32_t i = 0; i < planeCount; i++) {
                        imageSize += planeSize[i];
                    }
                    break;
                case PIPE_MCSC0:
                    configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t*)&imageW, (uint32_t*)&imageH, 0);
                    imageFormat = configurations->getYuvFormat(0);
                    getV4l2FormatInfo(imageFormat, &bpp, &planeCount);
                    getYuvPlaneSize(imageFormat, planeSize, imageW, imageH);
                    for (uint32_t i = 0; i < planeCount; i++) {
                        imageSize += planeSize[i];
                    }
                case PIPE_MCSC1:
                    configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t*)&imageW, (uint32_t*)&imageH, 1);
                    imageFormat = configurations->getYuvFormat(1);
                    getV4l2FormatInfo(imageFormat, &bpp, &planeCount);
                    getYuvPlaneSize(imageFormat, planeSize, imageW, imageH);
                    for (uint32_t i = 0; i < planeCount; i++) {
                        imageSize += planeSize[i];
                    }
                    break;
                case PIPE_MCSC2:
                    configurations->getSize(CONFIGURATION_YUV_SIZE, (uint32_t*)&imageW, (uint32_t*)&imageH, 2);
                    imageFormat = configurations->getYuvFormat(2);
                    getV4l2FormatInfo(imageFormat, &bpp, &planeCount);
                    getYuvPlaneSize(imageFormat, planeSize, imageW, imageH);
                    for (uint32_t i = 0; i < planeCount; i++) {
                        imageSize += planeSize[i];
                    }
                    break;
                default:
                    CLOGE2("Invalid request Pipe(%d)", pipeId);
                    break;
                }

                CLOGD2("W(%d), H(%d), S(%d)", imageW, imageH, imageSize);

                switch (indexL)
                {
                case 0x00:      //Revision number register ((only for old Simmian B/D V1)
                    *writeData = 0x20;
                    writeSize = 1;
                    break;
                case 0x02:      //Global configuration register (only for old Simmian B/D V1)
                case 0x03:
                case 0x07:  //we don't know
                case 0x11:  //ISP_FRM_CNT()_HIGH
                    *writeData = 0;
                    writeSize = 1;
                    break;
                    memset(writeData, 0, length);
                    writeSize = length;
                    break;
                case 0x10:  //ISP_FRM_CNT()_LOW
                    *writeData = 33; //33ms @ 1fps
                    writeSize = 1;
                    break;
                case 0x0C:  //ISP_IMG_HSZ_read()
#if 0
                    if (SimmianInfo.ImgType==SIMMIAN_JPEG) //SIMMIAN_JPEG
                        *writeData = (previewW)&0x000000ff;
                    else
#endif
                        *writeData = (uint8_t)((imageW * 2) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x0D:  //ISP_IMG_HSZ_read()
#if 0
                    if (SimmianInfo.ImgType==SIMMIAN_JPEG) //SIMMIAN_JPEG
                        *writeData = ((previewW)>>8)&0x000000ff;
                    else
#endif
                        *writeData = (uint8_t)(((imageW * 2) >> 8) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x0E:  //ISP_IMG_VSZ_read()
                    *writeData= (uint8_t)(imageH & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x0F:  //ISP_IMG_VSZ_read()
                    *writeData= (uint8_t)((imageH >> 8) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x20:
                    *writeData = (uint8_t)((imageSize >> 0) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x21:
                    *writeData = (uint8_t)((imageSize >> 8) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x22:
                    *writeData = (uint8_t)((imageSize >> 16) & 0x000000ff);
                    writeSize = 1;
                    break;
                case 0x23:
                    *writeData = (uint8_t)((imageSize >>24) & 0x000000ff);
                    writeSize = 1;
                    break;

                case 0x5a: //_Get_image_size and pixel counter
#if 0//(AP_SUPPORT_SIMMIAN420==1) //[2014/09/22,jspark] we decided to transfer original Width size to Simmian (for supporting YUV420-2p, over Simmian V72)
                    *((uint8_t *)(writeData + 0))= ((previewW*3)/2)&0x000000ff;
                    *((uint8_t *)(writeData + 1))= (((previewW*3)/2)>>8)&0x000000ff;
#else
#if 0
                    if (SimmianInfo.ImgType==SIMMIAN_JPEG)
                        *(writeData+0) = ((previewW))&0x000000ff;
                    else
#endif
                        *(writeData + 0) = (uint8_t)((imageW * 2) & 0x000000ff);

#if 0
                    if (SimmianInfo.ImgType==SIMMIAN_JPEG)
                        *(writeData + 1)= ((previewW)>>8)&0x000000ff;
                    else
#endif
                        *(writeData + 1) = (uint8_t)(((imageW * 2) >> 8) & 0x000000ff);

#endif
                    *(writeData + 2) = (uint8_t)(imageH & 0x000000ff);
                    *(writeData + 3) = (uint8_t)((imageH >> 8) & 0x000000ff);

                    //pixel byte counter
                    *(writeData + 4) = (uint8_t)((imageSize >> 0) & 0x000000ff);
                    *(writeData + 5) = (uint8_t)((imageSize >> 8) & 0x000000ff);
                    *(writeData + 6) = (uint8_t)((imageSize >> 16) & 0x000000ff);
                    *(writeData + 7) = (uint8_t)((imageSize >>24) & 0x000000ff);

                    writeSize = 8;
                    break;

                case 0x90:  //we don't know
                default:
                    memset(writeData, 0, length);
                        writeSize = length;
                        CLOGD2("Not supported mode: %d", indexL);
                        break;
                }

                m_imageState = T_IMAGE_STATE_READ_SIZE;
            }

            writeRet = write(m_socketCmdFD, writeData, writeSize);
            if (writeRet < 0) {
                CLOGD2("Write fail, size(%d)", writeRet);
            }

            CLOGD2("SY:write(%d) %x,%x,%x,%x", writeRet, writeData[0], writeData[1], writeData[2], writeData[3]);
            break;
        case TUNE_IMAGE_INTERNAL_CMD_READY:
            CLOGD2("TUNE_IMAGE_INTERNAL_CMD_READY");
            if (m_imageState == T_IMAGE_STATE_WAIT
                || m_imageState == T_IMAGE_STATE_READY) {
                bool imageReady = ExynosCameraTuner::getInstance()->isImageReady();
                if (imageReady == true) {
                    *writeData = 1;
                    m_imageState = T_IMAGE_STATE_READY;
                } else {
                    *writeData = 0;
                }
                writeSize = 1;

                writeRet = write(m_socketCmdFD, writeData, writeSize);
                if (writeRet < 0) {
                    CLOGD2("Write fail, size(%d)", writeRet);
                }

                CLOGD2("SY:write(%d) %x,%x,%x,%x", writeRet, writeData[0], writeData[1], writeData[2], writeData[3]);
            }
            break;
        case TUNE_IMAGE_INTERNAL_CMD_UPDATE:
            break;
        default:
            break;
        }
    }

    if (cmd != NULL) {
        ret = ExynosCameraTuner::getInstance()->sendCommand(cmd);
    }

    return ret;
}

}; //namespace android
