#define LOG_TAG "ExynosCameraTuningCommandParser"

#include "ExynosCameraTuningCommandParser.h"

#include "ExynosCameraThread.h"

#include "../ExynosCameraTuner.h"
#include "include/ExynosCameraTuningDefines.h"
#include "ExynosCameraTuningCommand.h"
#include "ExynosCameraTuningDispatcher.h"
#include "modules/ExynosCameraTuningModule.h"

namespace android {

ExynosCameraTuningCommandParser::ExynosCameraTuningCommandParser()
{
    //create();
    m_togleTest = true;
    m_skipCount = 100;
    m_socketCmdFD = -1;
    m_socketJsonFD = -1;
    m_socketImageFD = -1;
    m_socketOpened = false;
    m_socketCmdOpened = false;
    m_socketJsonOpened = false;
    m_socketImageOpened = false;
}

ExynosCameraTuningCommandParser::~ExynosCameraTuningCommandParser()
{
    //destroy();
}

status_t ExynosCameraTuningCommandParser::create(ExynosCameraTuningDispatcher* dispatcher)
{
    CLOGD2("");

    status_t ret = NO_ERROR;

    m_dispatcher = dispatcher;

    m_socketOpenThread = new ExynosCameraThread<ExynosCameraTuningCommandParser>(this, &ExynosCameraTuningCommandParser::m_socketOpen, "ExynosCameraTuningSocketOpener");

    ret = m_socketOpenThread->run("SocketOpenThread");

    m_mainThread = new ExynosCameraThread<ExynosCameraTuningCommandParser>(this, &ExynosCameraTuningCommandParser::m_parse, "ExynosCameraTuningCommandParser");

    ret = m_mainThread->run("ParsingThread");

    return ret;
}

status_t ExynosCameraTuningCommandParser::destroy()
{
    CLOGD2("");

    status_t ret = NO_ERROR;

    m_dispatcher = NULL;

    if (m_socketOpenThread != NULL) {
        if (m_socketOpenThread->isRunning()) {
            CLOGD2("stop socket open thread");
            m_socketOpenThread->requestExit();
        }
    }

    if (m_mainThread != NULL) {
        if (m_mainThread->isRunning()) {
            CLOGD2("stop main thread");
            m_mainThread->requestExit();
        }
    }

    if (m_socketCmdOpened == true && m_socketCmdFD > 0) {
        close(m_socketCmdFD);
    }

    if (m_socketJsonOpened == true && m_socketJsonFD > 0) {
        close(m_socketJsonFD);
    }

    if (m_socketImageOpened == true && m_socketImageFD > 0) {
        close(m_socketImageFD);
    }

    return ret;
}

bool ExynosCameraTuningCommandParser::m_socketOpen()
{
    status_t ret = NO_ERROR;

    if (m_socketCmdOpened == false) {
        m_socketCmdFD = open("/dev/is_tuning_socket", O_RDWR);
        if (m_socketCmdFD < 0) {
            CLOGD2("CMD socket open fail(FD:%d)", m_socketCmdFD);
            return true;
        }

        if (m_dispatcher != NULL) {
            ret = m_dispatcher->setSocketFd(m_socketCmdFD, TUNE_SOCKET_CMD);
        }

        m_socketCmdOpened = true;

        CLOGD2("CMD Socket opened(fd:%d)", m_socketCmdFD);
     }

    if (m_socketJsonOpened == false) {
        m_socketJsonFD = open("/dev/is_tuning_socket", O_RDWR);
        if (m_socketJsonFD < 0) {
            CLOGD2("JSON socket open fail(FD:%d)", m_socketJsonFD);
            return true;
        }

        if (m_dispatcher != NULL) {
            ret = m_dispatcher->setSocketFd(m_socketJsonFD, TUNE_SOCKET_JSON);
        }

        m_socketJsonOpened = true;

        CLOGD2("JSON Socket opened(fd:%d)", m_socketJsonFD);
     }

    if (m_socketImageOpened == false) {
        m_socketImageFD = open("/dev/is_tuning_socket", O_RDWR);
        if (m_socketImageFD < 0) {
            CLOGD2("IMAGE socket open fail(FD:%d)", m_socketImageFD);
            return true;
        }

        if (m_dispatcher != NULL) {
            ret = m_dispatcher->setSocketFd(m_socketImageFD, TUNE_SOCKET_IMAGE);
        }

        m_socketImageOpened = true;

        CLOGD2("IMAGE Socket opened(fd:%d)", m_socketImageFD);
    }

    m_socketOpened = true;

    return false;

}

int ExynosCameraTuningCommandParser::getSocketFd(TUNE_SOCKET_ID socketId)
{
    switch (socketId) {
    case TUNE_SOCKET_CMD:
       return m_socketCmdFD;
        break;
    case TUNE_SOCKET_JSON:
        return m_socketJsonFD;
        break;
    case TUNE_SOCKET_IMAGE:
        return m_socketImageFD;
        break;
    default:
        CLOGE2("Invalid socket ID(%d)", socketId);
        break;
    }

    return -1;
}

bool ExynosCameraTuningCommandParser::m_parse()
{
    bool loop = true;
    status_t ret = NO_ERROR;

    /*
    if(m_skipCount > 0) {
        m_skipCount--;
        CLOGD2("skip parse");
        usleep(100000);
        return loop;
    }
    */

    if (m_socketOpened == false) {
        usleep(200000);
        return loop;
    }

    //CLOGD2("m_parser(%d)", m_skipCount);

    uint8_t read_request[8] = {0,};
    uint8_t read_addr[8] = {0,};

    //Getting the Request and buffer from USB
    ret = m_readCommand(read_request, 8);
    if (ret != NO_ERROR) {
        usleep(200000);
        return loop;
    }

    ExynosCameraTuningCmdSP_sptr_t tuneCmd = NULL;
    tuneCmd = ExynosCameraTuner::getInstance()->getCommand();
    if (tuneCmd == NULL) {
        CLOGD2("CMD is NULL");
        tuneCmd = m_generateTuneCommand();
    } else {
        tuneCmd->initData();
    }

    //typecasting with SIMMIAN_CTRL_S
    SIMMIAN_CTRL_S * request = (SIMMIAN_CTRL_S*)read_request;

    //extract request value
    uint8_t vendorReq = request->bRequest & ~0x80;
    uint8_t setReq = request->bRequest & 0x80;

#if 0
    /*
    ********************** TEST begin**********************
    */
    static int testIdx = 1;
    switch (testIdx % 100) {

    case 3:
        setReq = TUNE_SET_REQUEST;
        vendorReq = TUNE_VENDOR_WRITE_ISP32;
        break;
    case 1:
        setReq = TUNE_SET_REQUEST;
        vendorReq = TUNE_VENDOR_WRITE_ISPMEM;
        break;
    case 2:
        setReq = TUNE_SET_REQUEST;
        vendorReq = TUNE_VENDOR_WRITE_JSON;
        break;

    //Get request
    case 0:
        setReq = TUNE_GET_REQUEST;
        vendorReq = TUNE_VENDOR_READ_JSON;
        CLOGD2("TUNE_VENDOR_READ_JSON");
        break;
    default:
        return loop;
        break;
    }
    testIdx++;
    /********************TEST end**************************/
    if (m_togleTest == true) {
        setReq = TUNE_GET_REQUEST;
        vendorReq = TUNE_VENDOR_READ_JSON;
        //m_togleTest = false;
        CLOGD2("TUNE_VENDOR_READ_JSON");
    } else if (m_togleTest == false) {
        setReq = TUNE_SET_REQUEST;
        vendorReq = TUNE_VENDOR_WRITE_JSON;
        //m_togleTest = true;
        CLOGD2("TUNE_VENDOR_WRITE_JSON");
    }
#endif

    switch (setReq)
    {
    case TUNE_GET_REQUEST:
        m_OnGetRequest(vendorReq, tuneCmd);
        break;
    case TUNE_SET_REQUEST:
    default:
        m_OnSetRequest(vendorReq, tuneCmd);
        break;
    }

    //Set command id
    uint32_t value = request->value;
    uint32_t index = request->index;
    uint32_t length = request->length;

    CLOGD2("%u, %u, %u, %u, %u, %u", request->bRequestType, setReq, vendorReq, value, index, length);
    ExynosCameraTuningCommand::t_data* data = tuneCmd->getData();

    switch(vendorReq) {
    case TUNE_VENDOR_READ_ISP32:
        data->commandID = index | value << 16;
        break;
    case TUNE_VENDOR_WRITE_ISP32:
        data->commandID = index | value << 16;

        ret = m_readCommand(read_addr, length);
        if (ret != NO_ERROR) {
            CLOGD2("read addr fail! (ret:%d)", ret);
        }
        data->commandAddr = read_addr[0] | read_addr[1] << 8 | read_addr[2] << 16 | read_addr[3] << 24;

        /*********************** TEST ******************/
        //data->commandID = 0x281;
        /**********************************************/
        break;

    case TUNE_VENDOR_WRITE_JSON:
        if (value == 0xFFFE) {
            if (index == 0x0100) {
                ret = m_readCommand(read_addr, length);
                if (ret != NO_ERROR) {
                    CLOGD2("read addr fail! (ret:%d)", ret);
                }
                data->commandAddr = read_addr[0] | read_addr[1] << 8 | read_addr[2] << 16 | read_addr[3] << 24;
            }
        } else if (value == 0xFFFC) {
            if (index == 0x0100) {
                ret = m_readCommand(read_addr, length);
                if (ret != NO_ERROR) {
                    CLOGD2("read addr fail! (ret:%d)", ret);
                }
                data->commandAddr = read_addr[0] | read_addr[1] << 8 | read_addr[2] << 16 | read_addr[3] << 24;
            }
        }
    case TUNE_VENDOR_READ_JSON:
        data->moduleCmd.command = value;
        data->moduleCmd.subCommand = index;
        data->moduleCmd.length = length;

        /*********************** TEST ******************/
        //data->jsonCmd.command = 0xFFFF;//ready check
        //data->jsonCmd.subCommand = 0;
        /**********************************************/
        break;
    case TUNE_VENDOR_FPGA_CTRL:
        data->commandID = TUNE_IMAGE_INTERNAL_CMD_FPGA_CTRL;
        data->moduleCmd.command = value;
        data->moduleCmd.subCommand = index;
        data->moduleCmd.length = length;
        break;
    case TUNE_VENDOR_IMG_READY:
        data->commandID = TUNE_IMAGE_INTERNAL_CMD_READY;
        data->moduleCmd.command = value;
        data->moduleCmd.subCommand = index;
        data->moduleCmd.length = length;
        break;
    case TUNE_VENDOR_IMG_UPDATE:
        data->commandID = TUNE_IMAGE_INTERNAL_CMD_UPDATE;
        data->moduleCmd.command = value;
        data->moduleCmd.subCommand = index;
        data->moduleCmd.length = length;
        break;
    case TUNE_VENDOR_WRITE_ISPMEM:
        data->commandID = TUNE_IMAGE_INTERNAL_CMD_WRITE;
    case TUNE_VENDOR_READ_ISPMEM:
    case TUNE_VENDOR_READ_ISPMEM2:
        ret = m_readCommand(read_addr, length);
        if (ret != NO_ERROR) {
            CLOGD2("read addr fail! (ret:%d)", ret);
        }
        data->moduleCmd.length = read_addr[3] | read_addr[2] << 8 | read_addr[1] << 16 | read_addr[0] << 24;
        data->commandAddr = read_addr[7] | read_addr[6] << 8 | read_addr[5] << 16 | read_addr[4] << 24;
        break;

    }

    if (m_dispatcher != NULL)
        m_dispatcher->dispatch(tuneCmd);

    //usleep(200000);

    return loop;
}

status_t ExynosCameraTuningCommandParser::m_readCommand(uint8_t* request, uint32_t readLength)
{
    Mutex::Autolock l(ExynosCameraTuner::getInstance()->readSocketLock);
    int readCnt = -1;

    //read command from USB
    if (m_socketCmdOpened == true && m_socketCmdFD > 0 && readLength > 0) {
        readCnt = read(m_socketCmdFD, request, readLength);
        if (readCnt <= 0) {
            CLOGV2("file read fail");
            return INVALID_OPERATION;
        }

        CLOGD2("SY:read(%d) %x,%x,%x,%x,%x,%x,%x,%x", readCnt, request[0], request[1], request[2], request[3], request[4], request[5], request[6], request[7]);
    }

    return NO_ERROR;

    /*
    simmian_ep0_data_stage(&rBuf, request.wLength) {
        ioctl(ep0FD, SIMMIAN_EP0_DATA_STAGE, buf);
    }
    */
}

status_t ExynosCameraTuningCommandParser::m_OnSetRequest(int vendorRequest, ExynosCameraTuningCmdSP_sptr_t tuneCmd)
{
    status_t ret = NO_ERROR;

    switch (vendorRequest)
    {
    case TUNE_VENDOR_IMG_READY:
    case TUNE_VENDOR_IMG_UPDATE:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::WRITE_IMAGE);
        break;
    case TUNE_VENDOR_WRITE_ISP32:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::SET_CONTROL);
        break;
    case TUNE_VENDOR_WRITE_ISPMEM:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::WRITE_IMAGE);
        break;
#if 0
    case TUNE_VENDOR_WRITE_ISPMEM2:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::WRITE_IMAGE);
        break;
#endif
    case TUNE_VENDOR_WRITE_JSON:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::WRITE_JSON);
        break;
    case TUNE_VENDOR_READ_ISPMEM:
    case TUNE_VENDOR_READ_ISPMEM2:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::READ_DATA);
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraTuningCommandParser::m_OnGetRequest(int vendorRequest, ExynosCameraTuningCmdSP_sptr_t tuneCmd)
{
    status_t ret = NO_ERROR;

    switch (vendorRequest)
    {
    case TUNE_VENDOR_FPGA_CTRL:
    case TUNE_VENDOR_IMG_READY:
    case TUNE_VENDOR_IMG_UPDATE:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::READ_IMAGE);
        break;
    case TUNE_VENDOR_READ_ISP32:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::GET_CONTROL);
        break;
    case TUNE_VENDOR_READ_JSON:
        tuneCmd->setCommandGroup(ExynosCameraTuningCommand::READ_JSON);
        break;
    default:
        break;
    }


    return ret;
}

ExynosCameraTuningCmdSP_sptr_t ExynosCameraTuningCommandParser::m_generateTuneCommand() {
    return new ExynosCameraTuningCommand();
}

}; //namespace andorid
