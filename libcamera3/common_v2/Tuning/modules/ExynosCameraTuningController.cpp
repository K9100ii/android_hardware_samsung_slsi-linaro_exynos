#define LOG_TAG "ExynosCameraTuningController"

#include "ExynosCameraTuningModule.h"
#include "ExynosCameraTuningCommandIdList.h"

#include "../ExynosCameraTuner.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraTuningISRegion.h"


namespace android {

ExynosCameraTuningController::ExynosCameraTuningController()
{
    CLOGD2("");
    m_cmdBufferAddr = (unsigned long)(malloc(SIMMY_RESERVED_PARAM_NUM * 16));
    m_fwBufferAddr = (unsigned long)(malloc(1<<26));
    m_afLogBufferAddr = (unsigned long)(malloc(0x00020000));
    m_exifBufferAddr = (unsigned long)(malloc(0x00020000));
    m_shareBase = (uint32_t *)malloc(0x00010000);
    m_debugBase = (uint32_t *)malloc(0x0007D000);
    memset((char *)m_cmdBufferAddr, 0, SIMMY_RESERVED_PARAM_NUM * 16);
    memset((char *)m_fwBufferAddr, 0, 1<<26);
    memset((char *)m_afLogBufferAddr, 0, 0x00020000);
    memset((char *)m_exifBufferAddr, 0, 0x00020000);
    memset((char *)m_shareBase, 0, 0x00010000);
    memset((char *)m_debugBase, 0, 0x0007D000);

    m_maxParamIndex = 0;
}

ExynosCameraTuningController::~ExynosCameraTuningController()
{
    free((void *)m_cmdBufferAddr);
    free((void *)m_fwBufferAddr);
    free((void *)m_afLogBufferAddr);
    free((void *)m_exifBufferAddr);
}

status_t ExynosCameraTuningController::m_OnCommandHandler(ExynosCameraTuningCmdSP_sptr_t cmd)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    ExynosCameraTuningCommand::t_data * data = cmd->getData();

    if (cmd->getCommandGroup() == ExynosCameraTuningCommand::UPDATE_EXIF) {
        CLOGD2("UPDATE EXIF");
        m_updateExif();
    } else if (cmd->getCommandGroup() == ExynosCameraTuningCommand::READ_DATA) {
        CLOGD2("READ_DATA(Size:%d, Addr:%lx)", data->moduleCmd.length, data->commandAddr);

        uint32_t writeSize = (524288);
        char *writeAddr = (char *)(data->commandAddr);
        uint32_t remainingSize = data->moduleCmd.length;
        int writeRet = -1;
        while (remainingSize > 0) {
            if (writeSize > remainingSize) {
                writeSize = remainingSize;
            }

            CLOGD2("write image size(%d), remainingSize(%d)", writeSize, remainingSize);
            writeRet = write(m_socketImageFD, writeAddr, writeSize);
            if (writeRet < 0) {
                CLOGD2("Write fail, size(%d)", writeRet);
                continue;
            }

            writeAddr += writeSize;
            remainingSize -= writeSize;
            //usleep(1000);
        }
    } else if (cmd->getCommandGroup() == ExynosCameraTuningCommand::GET_CONTROL) {
        m_getControl(data);
    } else {
        if (data->commandAddr != 0) {
            CLOGD2("cmdID(%x), addr(%lx), cmd(%d) Buffer(%lx)", data->commandID, data->commandAddr, *((uint32_t *)m_cmdBufferAddr), m_cmdBufferAddr);
            *((uint32_t *)(data->commandAddr)) = data->commandID;
            if (data->commandAddr > m_cmdBufferAddr
                && data->commandAddr <= (m_cmdBufferAddr + (4*256))) {
                uint32_t paramIndex = ((data->commandAddr - m_cmdBufferAddr) / 4) - 1;
                if (paramIndex > m_maxParamIndex) {
                    m_maxParamIndex = paramIndex;
                }
            }
            CLOGD2("cmdID(%x), addr(%lx), cmd(%d) Buffer(%lx)", data->commandID, data->commandAddr, *((uint32_t *)m_cmdBufferAddr), m_cmdBufferAddr);
        }

        if (*((uint32_t *)m_cmdBufferAddr) != 0) {
            for (uint32_t i = 0; i <= m_maxParamIndex; i++) {
                data->Parameter[i] = m_getParamFromBuffer(i);
            }

            if ((data->commandID & SIMMYCMD_SET_VRA_START_STOP) == SIMMYCMD_SET_VRA_START_STOP) {
                ret = m_VRAControl(data);
            } else if ((data->commandID & SIMMYCMD_SET_MCSC_FLIP_CONTROL) == SIMMYCMD_SET_MCSC_FLIP_CONTROL) {
                ret = m_MCSCControl(data);
            } else if ((data->commandID & SIMMYCMD_AF_CHANGE_MODE) == SIMMYCMD_AF_CHANGE_MODE) {
                ret = m_AFControl(data);
            } else if ((data->commandID & SIMMYCMD_SET_POWER_OFF) == SIMMYCMD_SET_POWER_OFF) {
                ret = m_SystemControl(data);
            } else if (data->commandID <= SIMMYCMD_SET_CURRENT_TIME) {
                ret = m_SystemControl(data);
            } else {
                ret = BAD_VALUE;
            }

            if (ret == BAD_VALUE) {
                ret = ExynosCameraTuner::getInstance()->storeToMap(cmd);
                cmd = NULL;
            }

            *(uint32_t *)m_cmdBufferAddr = SIMMYCMD_NO_CMD;
            m_maxParamIndex = 0;
        }
    }

    if (cmd != NULL) {
        ret = ExynosCameraTuner::getInstance()->sendCommand(cmd);
    }

    return ret;
}

status_t ExynosCameraTuningController::setISPControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->forceGetShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    ExynosCameraConfigurations* configurations = ExynosCameraTuner::getInstance()->getConfigurations();

    static int effectMode = 0;
    effectMode++;

    switch (data->commandID) {
    case SIMMYCMD_SET_ISP_AWB:
        CLOGD2("SIMMYCMD_SET_ISP_AWB");
        if (data->Parameter[0] == 0) //auto
        {
            CLOGD2("SIMMYCMD_SET_ISP_AWB :%d", data->Parameter[0]);
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
        }
        else if (data->Parameter[0] == 1) //illumination
        {
            CLOGD2("SIMMYCMD_SET_ISP_AWB :%d/%d", data->Parameter[0], data->Parameter[1]);
            if ( data->Parameter[1] == ISP_AWB_ILLUMINATION_DAYLIGHT )
            {
                shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;
            }
            else if ( data->Parameter[1] == ISP_AWB_ILLUMINATION_CLOUDY )
            {
                shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_CLOUDY_DAYLIGHT;
            }
            else if ( data->Parameter[1] == ISP_AWB_ILLUMINATION_TUNGSTEN )
            {
                shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_INCANDESCENT;
            }
            else if ( data->Parameter[1] == ISP_AWB_ILLUMINATION_FLUORESCENT )
            {
                shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_FLUORESCENT;
            }
            else
            {
                ret = INVALID_OPERATION;
            }
        }
        else if (data->Parameter[0] == 2) //manual
        {
            CLOGD2("SIMMYCMD_SET_ISP_AWB :%d", data->Parameter[0]);
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_OFF;
        }
        else
        {
            ret = INVALID_OPERATION;
        }

        break;
    case SIMMYCMD_SET_ISP_EFFECT:
        shot_ext.shot.ctl.aa.effectMode = (enum aa_effect_mode)(effectMode % AA_EFFECT_AQUA);
        break;
    case SIMMYCMD_SET_ISP_ISO:
        CLOGD2("SIMMYCMD_SET_ISP_ISO");
        if (data->Parameter[0] == 0)
        {
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
        }
        else if(data->Parameter[0] == 1)
        {
            uint32_t uISOValue = 0;
            if (data->Parameter[1] == 1)
            {
                uISOValue = 50;
            }
            else if (data->Parameter[1] == 2)
            {
                uISOValue = 100;
            }
            else if (data->Parameter[1] == 3)
            {
                uISOValue = 200;
            }
            else if (data->Parameter[1] == 4)
            {
                uISOValue = 400;
            }
            else if (data->Parameter[1] == 5)
            {
                uISOValue = 800;
            }
            else
            {
                uISOValue = 0;
            }
            CLOGD2("uISOValue : %d", uISOValue);
            if (uISOValue)
            {
                shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
                shot_ext.shot.ctl.aa.vendor_isoValue = uISOValue;
            }
            else
            {
                ret = INVALID_OPERATION;
            }
        }
        else
        {
            ret = INVALID_OPERATION;
        }

        break;
    case SIMMYCMD_SET_ISP_ADJUST:
        CLOGD2("SIMMYCMD_SET_ISP_ADJUST");
        if (data->Parameter[0] == ISP_ADJUST_COMMAND_AUTO)  //AUTO
        {
            shot_ext.shot.ctl.color.vendor_contrast = 3;
            shot_ext.shot.ctl.color.vendor_saturation = 3;
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_OFF;
            shot_ext.shot.ctl.color.vendor_brightness = 3;
            shot_ext.shot.ctl.color.vendor_hue = 3;
            shot_ext.shot.ctl.aa.aeExpCompensation = 0;
            shot_ext.shot.ctl.aa.vendor_aeExpCompensationStep = 0.5f;
        }
        else if (data->Parameter[0] <= ISP_ADJUST_COMMAND_MANUAL_ALL)  //manual
        {
            if(data->Parameter[0] & ISP_ADJUST_COMMAND_MANUAL_CONTRAST)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_CONTRAST %d", (int32_t)data->Parameter[1]);
                shot_ext.shot.ctl.color.vendor_contrast = (int32_t)data->Parameter[1] + 3;
            }
            if(data->Parameter[0] & ISP_ADJUST_COMMAND_MANUAL_SATURATION)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_SATURATION %d", (int32_t)data->Parameter[2]);
                shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2] + 3;
            }
            if(data->Parameter[0]  & ISP_ADJUST_COMMAND_MANUAL_SHARPNESS)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_SHARPNESS %d", (int32_t)data->Parameter[3]);
                shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
                shot_ext.shot.ctl.edge.strength = ((int32_t)data->Parameter[3] == -2) ? 1 : ((int32_t)data->Parameter[3] + 2) * 2;
            }
            if(data->Parameter[0]  & ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_BRIGHTNESS %d", (int32_t)data->Parameter[4]);
                shot_ext.shot.ctl.color.vendor_brightness = (int32_t)data->Parameter[4] + 3;
            }
            if(data->Parameter[0]  & ISP_ADJUST_COMMAND_MANUAL_HUE)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_HUE %d", (int32_t)data->Parameter[5]);
                shot_ext.shot.ctl.color.vendor_hue = (int32_t)data->Parameter[5] + 3;
            }
            if(data->Parameter[0]  & ISP_ADJUST_COMMAND_MANUAL_EXPOSURE)
            {
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_EXPOSURE %d %f", (int32_t)data->Parameter[6], (double)data->Parameter[6]/10);
                CLOGD2("ISP_ADJUST_COMMAND_MANUAL_EXPOSURE_STEP %d %f", (int32_t)data->Parameter[9], (double)data->Parameter[9]/10.);
                float actualExpVal = (float)((int32_t)data->Parameter[6]/10) * ((int32_t)data->Parameter[9]/10.);
                if (actualExpVal >= -2.0f && actualExpVal <= 2.0f) {
                    shot_ext.shot.ctl.aa.aeExpCompensation = (int32_t)data->Parameter[6]/10;
                    shot_ext.shot.ctl.aa.vendor_aeExpCompensationStep = (int32_t)data->Parameter[9]/10.;
                }
            }
        }
        else
        {
            ret = INVALID_OPERATION;
        }

        break;
    case SIMMYCMD_SET_ISP_EXPOSURE_CONTROL:
        break;
    case SIMMYCMD_SET_ISP_AE_METERING:
        CLOGD2("SIMMYCMD_SET_ISP_AE_METERING");
         shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
         shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

         switch (data->Parameter[0]) { // 20121227 injong.yu AE Metering [START]
         case ISP_METERING_COMMAND_AVERAGE:
             CLOGD2("ISP_METERING_COMMAND_AVERAGE");
             shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
             break;

         case ISP_METERING_COMMAND_SPOT:
             CLOGD2("ISP_METERING_COMMAND_SPOT");
             shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_SPOT;
             break;

         case ISP_METERING_COMMAND_MATRIX:
             CLOGD2("ISP_METERING_COMMAND_MATRIX");
             shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_MATRIX;
             break;

         case ISP_METERING_COMMAND_CENTER:
             CLOGD2("ISP_METERING_COMMAND_CENTER");
             shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
             break;

         default:
             ret = INVALID_OPERATION;
             break;
         }    // 20121227 injong.yu AE Metering [END]

        break;
    case SIMMYCMD_SET_ISP_AFC:
        CLOGD2("SIMMYCMD_SET_ISP_AFC mode(%d)", (int)(data->Parameter[0]));
        if (data->Parameter[0] == 0)
        {
            shot_ext.shot.ctl.aa.aeAntibandingMode = AA_AE_ANTIBANDING_OFF;
        }
        else if (data->Parameter[0] == 1)
        {
            shot_ext.shot.ctl.aa.aeAntibandingMode = AA_AE_ANTIBANDING_AUTO;
        }
        else if (data->Parameter[0] == 2)
        {
            shot_ext.shot.ctl.aa.aeAntibandingMode = AA_AE_ANTIBANDING_50HZ;
        }
        else if (data->Parameter[0] == 3)
        {
            shot_ext.shot.ctl.aa.aeAntibandingMode = AA_AE_ANTIBANDING_60HZ;
        }
        else
        {
            ret = INVALID_OPERATION;
        }

        break;
    case SIMMYCMD_SET_ISP_LOCK_AWB_AE:
        CLOGD2("SIMMYCMD_SET_ISP_LOCK_AWB_AE");
        if (data->Parameter[0] == 0) //no lock
        {
            shot_ext.shot.ctl.aa.awbLock = AA_AWB_LOCK_OFF;
            shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
        }
        else if (data->Parameter[0] == 1) //lock
        {
            shot_ext.shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;
            shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
        }
        else
        {
            ret = INVALID_OPERATION;
        }
        break;
    case SIMMYCMD_SET_ISP_FLASH:
        CLOGD2("SIMMYCMD_SET_ISP_FLASH");
         if (data->Parameter[1] <= ISP_FLASH_REDEYE_ENABLE)
         {
             if (data->Parameter[0] == 0) //disable
             {
                 shot_ext.shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_OFF;
             }
             else if (data->Parameter[0] == 1) //manual
             {
                 CLOGD2("manual Not implemented");
                 //IHFApp_Cmd_SetFlashMode(ISP_FLASH_COMMAND_MANUAL_ON, data->Parameter[1], 0);
             }
             else if (data->Parameter[0] == 2) //Auto
             {
                 shot_ext.shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_AUTO;
             }
             else if (data->Parameter[0] == 3) //_TORCH
             {
                 shot_ext.shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_ON;
             }
             else if (data->Parameter[0] == 4) //Froced Auto
             {
                 CLOGD2("Forced auto Not implemented");
                 //IHFApp_Cmd_SetFlashMode(ISP_FLASH_COMMAND_FLASH_ON, data->Parameter[1], 0);
             }
             else if (data->Parameter[0] == 5) //Capture
             {
                 shot_ext.shot.ctl.aa.vendor_aeflashMode = AA_FLASHMODE_CAPTURE;
             }
             else
             {
                 ret = false;
             }
         }
         else
         {
             ret = false;
         }

        break;
    case SIMMYCMD_SET_ISP_SCENEMODE:
        CLOGD2("SIMMYCMD_SET_ISP_SCENEMODE, mode(%d)", (int)(data->Parameter[0]));

        switch (data->Parameter[0]) {
        case ISP_SCENE_NONE:
            CLOGD2("ISP_SCENE_NONE");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_PORTRAIT:
            CLOGD2("ISP_SCENE_PORTRAIT");
            shot_ext.shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext.shot.ctl.aa.sceneMode = AA_SCENE_MODE_PORTRAIT;
            break;

        case ISP_SCENE_LANDSCAPE:
            CLOGD2("ISP_SCENE_LANDSCAPE");
            shot_ext.shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext.shot.ctl.aa.sceneMode = AA_SCENE_MODE_LANDSCAPE;
            break;

        case ISP_SCENE_SPORTS:
            CLOGD2("ISP_SCENE_SPORTS");
            shot_ext.shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext.shot.ctl.aa.sceneMode = AA_SCENE_MODE_SPORTS;
            break;

        case ISP_SCENE_PARTYINDOOR:
            CLOGD2("ISP_SCENE_PARTYINDOOR");
            shot_ext.shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext.shot.ctl.aa.sceneMode = AA_SCENE_MODE_PARTY;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
            shot_ext.shot.ctl.aa.aeExpCompensation = 5; // means '0'
            shot_ext.shot.ctl.aa.aeTargetFpsRange[0] = 15;
            shot_ext.shot.ctl.aa.aeTargetFpsRange[1] = 30; // Not used in F/W
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
            break;

        case ISP_SCENE_BEACHSNOW:
            CLOGD2("ISP_SCENE_BEACHSNOW");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
            shot_ext.shot.ctl.aa.vendor_isoValue = 50;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_SUNSET:
            CLOGD2("ISP_SCENE_SUNSET");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_DAWN:
            CLOGD2("ISP_SCENE_DAWN");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_FLUORESCENT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_FALL:
            CLOGD2("ISP_SCENE_FALL");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_NIGHT:
            CLOGD2("ISP_SCENE_NIGHT");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_AGAINSTLIGHTWLIGHT:
            CLOGD2("ISP_SCENE_AGAINSTLIGHTWLIGHT");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_AGAINSTLIGHTWOLIGHT:
            CLOGD2("ISP_SCENE_AGAINSTLIGHTWOLIGHT");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_FIRE:
            CLOGD2("ISP_SCENE_FIRE");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_MANUAL;
            shot_ext.shot.ctl.aa.vendor_isoValue = 50;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_TEXT:
            CLOGD2("ISP_SCENE_TEXT");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_CANDLE:
            CLOGD2("ISP_SCENE_CANDLE");
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_AVERAGE;
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_DAYLIGHT;

            shot_ext.shot.ctl.color.vendor_contrast = data->Parameter[1];
            shot_ext.shot.ctl.color.vendor_saturation = (int32_t)data->Parameter[2];
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_HIGH_QUALITY;
            shot_ext.shot.ctl.edge.strength = data->Parameter[3];
            shot_ext.shot.ctl.color.vendor_brightness = data->Parameter[4];
            shot_ext.shot.ctl.color.vendor_hue = data->Parameter[5];
            shot_ext.shot.ctl.aa.aeExpCompensation = data->Parameter[6];
            break;

        case ISP_SCENE_ANTISHAKE:
            CLOGD2("ISP_SCENE_ANTISHAKE");
            shot_ext.shot.ctl.aa.mode = AA_CONTROL_USE_SCENE_MODE;
            shot_ext.shot.ctl.aa.sceneMode = AA_SCENE_MODE_ANTISHAKE;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
            shot_ext.shot.ctl.aa.aeExpCompensation = 5; // means '0'
            shot_ext.shot.ctl.aa.aeTargetFpsRange[0] = 30;
            shot_ext.shot.ctl.aa.aeTargetFpsRange[1] = 30; // Not used in F/W
            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
            shot_ext.shot.ctl.aa.vendor_isoValue = 0;
            shot_ext.shot.ctl.color.vendor_saturation = 3; // means '0'
            shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_OFF;
            shot_ext.shot.ctl.edge.strength = 0;
            shot_ext.shot.ctl.noise.mode = PROCESSING_MODE_OFF;
            shot_ext.shot.ctl.noise.strength = 0;
            break;

        default:
            ret = false;
            break;
        }

        break;
    case SIMMYCMD_SET_ISP_FLASH_SCENARIO:
        break;
    case SIMMYCMD_SET_ISP_3A_MODE:
        CLOGD2("SIMMYCMD_SET_ISP_3A_MODE");
        switch (data->Parameter[0])
        {
            case 0:
                shot_ext.shot.ctl.aa.mode = AA_CONTROL_OFF;
            case 1:
                shot_ext.shot.ctl.aa.mode = AA_CONTROL_AUTO;
                //shot_ext.shot.ctl.aa.sceneMode = (enum aa_mode)0;
                shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;
                if ((shot_ext.shot.ctl.aa.aeTargetFpsRange[0]==0) || (shot_ext.shot.ctl.aa.aeTargetFpsRange[1]==0))
                {
                    shot_ext.shot.ctl.aa.aeTargetFpsRange[0] = 15;
                    shot_ext.shot.ctl.aa.aeTargetFpsRange[1] = 30; // Not used in F/W
                }

                shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
                shot_ext.shot.ctl.aa.vendor_isoMode = AA_ISOMODE_AUTO;
                shot_ext.shot.ctl.aa.vendor_isoValue = 0;

                shot_ext.shot.ctl.color.vendor_hue = 3;
                shot_ext.shot.ctl.color.vendor_saturation = 3; // means '0'
                shot_ext.shot.ctl.color.vendor_brightness = 3;
                shot_ext.shot.ctl.color.vendor_contrast = 3;

                shot_ext.shot.ctl.tonemap.mode = TONEMAP_MODE_FAST;

                shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_OFF;
                shot_ext.shot.ctl.edge.strength = 0;

                shot_ext.shot.ctl.noise.mode = PROCESSING_MODE_OFF;
                shot_ext.shot.ctl.noise.strength = 0;
                break;

            default:
                CLOGD2("- Not supported mode: %d", data->Parameter[0]);
                break;
        }

        break;
    case SIMMYCMD_SET_ISP_3AAE_MODE:
        CLOGD2("SIMMYCMD_SET_ISP_3AAE_MODE(%d)", data->Parameter[0]);
        switch (data->Parameter[0])
        {
            case AA_AEMODE_OFF:
            case AA_AEMODE_ON:
            case AA_AEMODE_ON_AUTO_FLASH:
            case AA_AEMODE_ON_ALWAYS_FLASH:
            case AA_AEMODE_ON_AUTO_FLASH_REDEYE:
            case AA_AEMODE_CENTER:
            case AA_AEMODE_AVERAGE:
            case AA_AEMODE_MATRIX:
            case AA_AEMODE_SPOT:
            case AA_AEMODE_CENTER_TOUCH:
            case AA_AEMODE_AVERAGE_TOUCH:
            case AA_AEMODE_MATRIX_TOUCH:
            case AA_AEMODE_SPOT_TOUCH:
            case 11: /* AA_AEMODE_EMUL */
                shot_ext.shot.ctl.aa.aeMode = (enum aa_aemode)data->Parameter[0];
                CLOGD2("AE mode %d", data->Parameter[0]);
                break;
            default:
                CLOGD2("- Not supported mode: %d", data->Parameter[0]);
                break;
        }

        break;
    case SIMMYCMD_GET_NOISE_REDUCTION:
        break;
    case SIMMYCMD_SET_NOISE_REDUCTION:
        CLOGD2("SIMMYCMD_SET_NOISE_REDUCTION");
        if (data->Parameter[0] == PROCESSING_MODE_OFF)
        {
            shot_ext.shot.ctl.noise.mode = PROCESSING_MODE_OFF;
        }
        else if (data->Parameter[0] > 0 && data->Parameter[0] < 10)
        {
            shot_ext.shot.ctl.noise.strength = data->Parameter[0];
            shot_ext.shot.ctl.noise.mode = PROCESSING_MODE_FAST;
        }
        else
        {
            CLOGD2("Not supported mode: %d",data->Parameter[0]);
        }

        break;
    case SIMMYCMD_GET_EDGE_ENHANCEMENT:
        break;
    case SIMMYCMD_SET_EDGE_ENHANCEMENT:
        CLOGD2("SIMMYCMD_SET_EDGE_ENHANCEMENT");
         if (data->Parameter[0] == PROCESSING_MODE_OFF)
         {
             shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_OFF;
         }
         else if (data->Parameter[0] > 0 && data->Parameter[0] < 10)
         {
             shot_ext.shot.ctl.edge.strength = data->Parameter[0];
             shot_ext.shot.ctl.edge.mode = PROCESSING_MODE_FAST;
         }
         else
         {
             CLOGD2("Not supported mode: %d",data->Parameter[0]);
         }
         break;

        break;
    case SIMMYCMD_GET_IP_STATUS:
        break;
#if (SCALER_YUV_RANGE == 1)
    case     SIMMYCMD_SET_YUV_RANGE:
        break;
#endif
    case SIMMYCMD_SET_ISP_START_STOP:
        break;
    case SIMMYCMD_SET_ISP_USERMAXFRAMETIME:
        break;
    case SIMMYCMD_SET_ISP_YUV_DMAOUT:
        break;
    case SIMMYCMD_SET_ISP_BAYER_DMAOUT:
        break;
    case SIMMYCMD_SET_ISP_CAPTURE_JPEG:
        break;
    case SIMMYCMD_SET_ISP_CAPTURE_YUV:
        break;
    case SIMMYCMD_SET_ISP_CAPTURE_BAYER:
        break;
    case SIMMYCMD_SET_ISP_CAPTURE_BMP:
        break;
    case SIMMYCMD_SET_ISP_TRANSFER_JPEGQTABLE_DATA:
        break;
    case SIMMYCMD_SET_ISP_START_MFCENCODING:
        break;
    case SIMMYCMD_GET_ISP_MFC_STATUS:
        break;
    case SIMMYCMD_SET_ISP_SAVE_MFCENCODING:
        break;
    case SIMMYCMD_GET_FIMC_LITE_SCC_BUFFER:
        break;
    case SIMMYCMD_SET_WDR_COMMAND:
        CLOGD2("SIMMYCMD_SET_WDR_COMMAND (%d)", data->Parameter[0]);

        /*
        if (data->Parameter[0] >= 1 && data->Parameter[0] <= 4) {
            shot_ext.shot.uctl.companionUd.wdr_mode = (enum companion_wdr_mode)data->Parameter[0];
        }
        */
        break;
    case SIMMYCMD_SET_EMULATOR_START:
        //gTuningEmulatorOn = true;
        shot_ext.shot.ctl.aa.aeMode = (enum aa_aemode)11; /* AA_AEMODE_EMUL */
        shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
        shot_ext.shot.ctl.aa.awbLock = AA_AWB_LOCK_OFF;
        if (configurations->getModeValue(CONFIGURATION_OBTE_TUNING_MODE) != 1) {
            CLOGD2("SIMMYCMD_SET_EMULATOR_START");
            shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_ON;
            shot_ext.shot.ctl.aa.awbLock = AA_AWB_LOCK_ON;

            configurations->setModeValue(CONFIGURATION_OBTE_TUNING_MODE_NEW, 1);
            configurations->setRestartStream(true);
        }
        break;

    case SIMMYCMD_SET_EMULATOR_STOP:
        //gTuningEmulatorOn = false;
        if (configurations->getModeValue(CONFIGURATION_OBTE_TUNING_MODE) != 0) {
            CLOGD2("SIMMYCMD_SET_EMULATOR_STOP");
            shot_ext.shot.uctl.lensUd.pos = 0;
            shot_ext.shot.uctl.lensUd.posSize = 0;
            shot_ext.shot.uctl.lensUd.direction = 0;
            shot_ext.shot.uctl.lensUd.slewRate = 0;

            shot_ext.shot.ctl.aa.aeLock = AA_AE_LOCK_OFF;
            shot_ext.shot.ctl.aa.awbLock = AA_AWB_LOCK_OFF;

            shot_ext.shot.ctl.aa.awbMode = AA_AWBMODE_WB_AUTO;
            shot_ext.shot.ctl.aa.aeMode = AA_AEMODE_CENTER;

            configurations->setModeValue(CONFIGURATION_OBTE_TUNING_MODE_NEW, 0);
            configurations->setRestartStream(true);
        }
        break;

    case SIMMYCMD_SET_EMULATOR_PARAMETERS:
        CLOGD2("SIMMYCMD_SET_EMULATOR_PARAMETERS");
        if (data->Parameter[0] == 1) {
            for (uint32_t i = 0; i < 10; i++) {
                shot_ext.shot.uctl.fdUd.faceIds[i] = data->Parameter[(i*6)+1];
                shot_ext.shot.uctl.fdUd.faceScores[i] = data->Parameter[(i*6)+2];
                shot_ext.shot.uctl.fdUd.faceRectangles[i][0] = data->Parameter[(i*6)+3];
                shot_ext.shot.uctl.fdUd.faceRectangles[i][1] = data->Parameter[(i*6)+4];
                shot_ext.shot.uctl.fdUd.faceRectangles[i][2] = data->Parameter[(i*6)+5];
                shot_ext.shot.uctl.fdUd.faceRectangles[i][3] = data->Parameter[(i*6)+5];

                CLOGD2("SIMMYCMD_SET_EMULATOR_PARAMETERS Active(%d), \
                        %d's face ID(%d), Score(%d), top(%d,%d), bottom(%d,%d)",
                        data->Parameter[0], i,
                        shot_ext.shot.uctl.fdUd.faceIds[i],
                        shot_ext.shot.uctl.fdUd.faceScores[i],
                        shot_ext.shot.uctl.fdUd.faceRectangles[i][0],
                        shot_ext.shot.uctl.fdUd.faceRectangles[i][1],
                        shot_ext.shot.uctl.fdUd.faceRectangles[i][2],
                        shot_ext.shot.uctl.fdUd.faceRectangles[i][3]);
            }
        }
        break;
    }

    ExynosCameraTuner::getInstance()->forceSetShot(&shot_ext);

    return ret;
}

status_t ExynosCameraTuningController::setAFControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    ISP_AfSceneEnum     uiScene;
    ISP_AfTouchEnum     uiTouch;
    ISP_AfFaceEnum      uiFace;
    ISP_AfResponseEnum  uiResponse;
    enum aa_afmode afMode = AA_AFMODE_OFF;
    uint32_t uiTouchX = 0, uiTouchY = 0;
    uint32_t uiAfManualSetting = 0;
    uint32_t uModeOption = 0;
    uint32_t ISPCropWidth = 0, ISPCropHeight = 0;
    int32_t left = 0, top = 0, right = 0, bottom = 0;

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->forceGetShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    ExynosCameraParameters* parameters = ExynosCameraTuner::getInstance()->getParameters();

    ExynosRect bnsRect, bcropRect;
    parameters->getPreviewBayerCropSize(&bnsRect, &bcropRect, true);

    switch (data->commandID) {
    case SIMMYCMD_AF_CHANGE_MODE:
        CLOGD2("SIMMYCMD_AF_CHANGE_MODE");

        if (data->Parameter[0] == ISP_AF_SINGLE || data->Parameter[0] == ISP_AF_CONTINUOUS) {
            CLOGD2("[SIMMYCMD_AF_CHANGE_MODE] %s, %s, %s, %s, %s, (%d, %d)",
                    data->Parameter[0] == ISP_AF_SINGLE ? "Single" : "Continuous",
                    data->Parameter[1] ? "Macro" : "Normal",
                    data->Parameter[2] ? "Touch" : "Center",
                    data->Parameter[3] ? "Face Enable" : "Face Disable",
                    data->Parameter[4] ? "Movie" : "Preview",
                    data->Parameter[5], data->Parameter[6]);
        } else {
            CLOGD2("[SIMMYCMD_AF_CHANGE_MODE] %d, %d, %d, %d, %d, (%d, %d)",
                    data->Parameter[0], data->Parameter[1], data->Parameter[2],
                    data->Parameter[3], data->Parameter[4], data->Parameter[5], data->Parameter[6]);
        }

        switch (data->Parameter[0])
        {
        case ISP_AF_MANUAL:
            CLOGD2("ISP_AF_MANUAL");

            uiAfManualSetting = data->Parameter[1]; //manual AF position
            if ((int32_t)uiAfManualSetting < 0) {
                uiAfManualSetting = 0;
            }

            if (uiAfManualSetting > 1023) {
                uiAfManualSetting = 1023;
            }

            shot_ext.shot.uctl.lensUd.pos = uiAfManualSetting;
            shot_ext.shot.uctl.lensUd.posSize = 10;
            shot_ext.shot.uctl.lensUd.direction = 0;
            shot_ext.shot.uctl.lensUd.slewRate = 0;

            afMode = AA_AFMODE_OFF;

            CLOGD2("ISP_AF_MANUAL %d", uiAfManualSetting);

            break;
        case ISP_AF_SINGLE:
            CLOGD2("ISP_AF_SINGLE");
            uiScene = (ISP_AfSceneEnum)data->Parameter[1];
            uiTouch = (ISP_AfTouchEnum)data->Parameter[2];
            uiFace = (ISP_AfFaceEnum)data->Parameter[3];
            uiResponse = (ISP_AfResponseEnum)data->Parameter[4];

            if (uiTouch == ISP_AF_TOUCH_ENABLE) {
                // uiTouchX/Y: Current Simmian command uses SIRC_ISP_AF_WINDOW_RESOLUTION resolution
                uiTouchX = data->Parameter[5];
                uiTouchY = data->Parameter[6];

                left    = uiTouchX - (SINGLE_INNER_WIDTH>> 1);
                right   = uiTouchX + (SINGLE_INNER_WIDTH>> 1);
                top     = uiTouchY - (SINGLE_INNER_HEIGHT >> 1);
                bottom  = uiTouchY + (SINGLE_INNER_HEIGHT >> 1);

                // Clipping
                if (left < 0) {
                    right += -left;
                    left = 0;
                } else if (right >= SIRC_ISP_AF_WINDOW_RESOLUTION) {
                    left -= (right - SIRC_ISP_AF_WINDOW_RESOLUTION + 1);
                    right = SIRC_ISP_AF_WINDOW_RESOLUTION + 1;
                }
                if (top < 0) {
                    bottom += -top;
                    top = 0;
                } else if (bottom >= SIRC_ISP_AF_WINDOW_RESOLUTION) {
                    top -= (bottom - SIRC_ISP_AF_WINDOW_RESOLUTION + 1);
                    bottom = SIRC_ISP_AF_WINDOW_RESOLUTION + 1;
                }

                ISPCropWidth = bcropRect.w;
                ISPCropHeight = bcropRect.h;

                left = (left * ISPCropWidth  + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                top = (top * ISPCropHeight + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                right = (right * ISPCropWidth + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                bottom = (bottom * ISPCropHeight + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
            } else {
                left = top = right = bottom = 0;// If region is all 0, AF uses default window like Legacy
            }

            shot_ext.shot.uctl.lensUd.pos = 0;
            shot_ext.shot.uctl.lensUd.posSize = 0;
            shot_ext.shot.uctl.lensUd.direction = 0;
            shot_ext.shot.uctl.lensUd.slewRate = 0;

            CLOGD2("AF region: %d, %d, %d, %d", left, top, right, bottom);
            shot_ext.shot.ctl.aa.afRegions[0] = left;
            shot_ext.shot.ctl.aa.afRegions[1] = top;
            shot_ext.shot.ctl.aa.afRegions[2] = right;
            shot_ext.shot.ctl.aa.afRegions[3] = bottom;
            shot_ext.shot.ctl.aa.afRegions[4] = 1;

            // Check uiResponse
            if (uiResponse == ISP_AF_RESPONSE_MOVIE) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_VIDEO);
            }

            // Check uiScene
            if (uiScene == ISP_AF_SCENE_MACRO) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_MACRO);
            }

            // Check uiFace
            if (uiFace == ISP_AF_FACE_ENABLE) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_FACE);
            }

            afMode = AA_AFMODE_AUTO;
            break;
        case ISP_AF_CONTINUOUS:
            CLOGD2("ISP_AF_CONTINUOUS");
            uiScene =       (ISP_AfSceneEnum)data->Parameter[1];
            uiTouch =       (ISP_AfTouchEnum)data->Parameter[2];
            uiFace =        (ISP_AfFaceEnum)data->Parameter[3];
            uiResponse =    (ISP_AfResponseEnum)data->Parameter[4];
            if (uiTouch == ISP_AF_TOUCH_ENABLE)
            {
                // uiTouchX/Y: Current Simmian command uses SIRC_ISP_AF_WINDOW_RESOLUTION resolution
                uiTouchX = data->Parameter[5];
                uiTouchY = data->Parameter[6];

                left    = uiTouchX - (CONTINUOUS_INNER_WIDTH >> 1);
                right   = uiTouchX + (CONTINUOUS_INNER_WIDTH >> 1);
                top     = uiTouchY - (CONTINUOUS_INNER_HEIGHT >> 1);
                bottom  = uiTouchY + (CONTINUOUS_INNER_HEIGHT >> 1);

                // Clipping
                if (left < 0) {
                    right += -left;
                    left = 0;
                } else if (right >= SIRC_ISP_AF_WINDOW_RESOLUTION) {
                    left -= (right - SIRC_ISP_AF_WINDOW_RESOLUTION + 1);
                    right = SIRC_ISP_AF_WINDOW_RESOLUTION + 1;
                }
                if (top < 0) {
                    bottom += -top;
                    top = 0;
                } else if (bottom >= SIRC_ISP_AF_WINDOW_RESOLUTION) {
                    top -= (bottom - SIRC_ISP_AF_WINDOW_RESOLUTION + 1);
                    bottom = SIRC_ISP_AF_WINDOW_RESOLUTION + 1;
                }

                ISPCropWidth = bcropRect.w;
                ISPCropHeight = bcropRect.h;


                left = (left * ISPCropWidth  + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                top = (top * ISPCropHeight + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                right = (right * ISPCropWidth + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
                bottom = (bottom * ISPCropHeight + (SIRC_ISP_AF_WINDOW_RESOLUTION >> 1)) / SIRC_ISP_AF_WINDOW_RESOLUTION;
            }
            else
            {
                left = top = right = bottom = 0;// If region is all 0, AF uses default window like Legacy
            }

            shot_ext.shot.uctl.lensUd.pos = 0;
            shot_ext.shot.uctl.lensUd.posSize = 0;
            shot_ext.shot.uctl.lensUd.direction = 0;
            shot_ext.shot.uctl.lensUd.slewRate = 0;

            CLOGD2("AF region: %d, %d, %d, %d", left, top, right, bottom);
            shot_ext.shot.ctl.aa.afRegions[0] = left;
            shot_ext.shot.ctl.aa.afRegions[1] = top;
            shot_ext.shot.ctl.aa.afRegions[2] = right;
            shot_ext.shot.ctl.aa.afRegions[3] = bottom;
            shot_ext.shot.ctl.aa.afRegions[4] = 1;

            // Check uiResponse
            if (uiResponse == ISP_AF_RESPONSE_MOVIE) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_VIDEO);
            }

            // Check uiScene
            if (uiScene == ISP_AF_SCENE_MACRO) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_MACRO);
            }

            // Check uiFace
            if (uiFace == ISP_AF_FACE_ENABLE) {
                uModeOption |= (1 << AA_AFMODE_OPTION_BIT_FACE);
            }

            if (uiResponse == ISP_AF_RESPONSE_PREVIEW) {
                afMode = AA_AFMODE_CONTINUOUS_PICTURE;
            } else {
                afMode = AA_AFMODE_CONTINUOUS_VIDEO;
            }
            break;
        }

        shot_ext.shot.ctl.aa.afMode = afMode;
        shot_ext.shot.ctl.aa.vendor_afmode_option = uModeOption;

        switch (afMode)
        {
            case AA_AFMODE_OFF:
            case AA_AFMODE_CONTINUOUS_PICTURE:
            case AA_AFMODE_CONTINUOUS_VIDEO:
                shot_ext.shot.ctl.aa.afTrigger = AA_AF_TRIGGER_IDLE;
                break;
            case AA_AFMODE_AUTO:
            case AA_AFMODE_MACRO:
                shot_ext.shot.ctl.aa.afTrigger = AA_AF_TRIGGER_START;
                ret = BAD_VALUE;
                break;
            default:
                CLOGD2("Not supported mode:%d", afMode);
        }
        break;
    case SIMMYCMD_AF_SET_WINDOW:
        CLOGD2("SIMMYCMD_AF_SET_WINDOW");
        break;
    case SIMMYCMD_AF_SET_ONSCRREN_AF_WINDOW:
        CLOGD2("SIMMYCMD_AF_SET_ONSCRREN_AF_WINDOW");
        break;
    case SIMMYCMD_AF_GET_WINDOW_START_POSITION:
    case SIMMYCMD_AF_GET_WINDOW_SIZE:
    case SIMMYCMD_AF_GET_WINDOW:
    case SIMMYCMD_AF_GET_GRAD1:
    case SIMMYCMD_AF_GET_GRAD2:
    case SIMMYCMD_AF_GET_HPF:
    case SIMMYCMD_AF_GET_FILTER_3x9:
    case SIMMYCMD_AF_GET_CLIP_COUNTER:
    case SIMMYCMD_AF_GET_Y_SUM:
    case SIMMYCMD_AF_GET_FILTER_1x17:
    case SIMMYCMD_AF_GET_LENS_POSITION:
    case SIMMYCMD_AF_GET_FOCUS_STATUS:
        break;
    }

    ExynosCameraTuner::getInstance()->forceSetShot(&shot_ext);

    return ret;
}

status_t ExynosCameraTuningController::m_AFControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    uint32_t AF_WindowsIndx = 0;
    IS_ShareRegionStr *pISShareRegion = (IS_ShareRegionStr *)m_shareBase;

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->getShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    ExynosCameraParameters* parameters = ExynosCameraTuner::getInstance()->getParameters();

    ExynosRect bnsRect, bcropRect;
    parameters->getPreviewBayerCropSize(&bnsRect, &bcropRect, true);
    uint32_t cameraId = (uint32_t)parameters->getCameraId();

    switch (data->commandID) {
    case SIMMYCMD_AF_GET_WINDOW_START_POSITION:
        CLOGD2("SIMMYCMD_AF_GET_WINDOW_START_POSITION");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t AF_StartX = pISShareRegion->AFWindowInfo[cameraId][AF_WindowsIndx].uStartX;
            uint32_t AF_StartY = pISShareRegion->AFWindowInfo[cameraId][AF_WindowsIndx].uStartY;
            m_setParamToBuffer(1, AF_StartX);
            m_setParamToBuffer(2, AF_StartY);

            CLOGD2("Window%d(%d,%d)", AF_WindowsIndx, AF_StartX, AF_StartY);
        }
        break;
    case SIMMYCMD_AF_GET_WINDOW_SIZE:
        CLOGD2("SIMMYCMD_AF_GET_WINDOW_SIZE");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t AF_Width = pISShareRegion->AFWindowInfo[cameraId][AF_WindowsIndx].uWidth;
            uint32_t AF_Height = pISShareRegion->AFWindowInfo[cameraId][AF_WindowsIndx].uHeight;
            m_setParamToBuffer(1, AF_Width);
            m_setParamToBuffer(2, AF_Height);

            CLOGD2("Window%d(%dx%d)", AF_WindowsIndx, AF_Width, AF_Height);
        }
        break;
    case SIMMYCMD_AF_GET_WINDOW:
        CLOGD2("SIMMYCMD_AF_GET_WINDOW");
        {
            m_updateShareBase();

            int i;
            uint32_t AF_StartX, AF_StartY, AF_Width, AF_Height;
            for (i = 0; i < TOTAL_NUM_AF_WINDOWS;i++)
            {
                AF_StartX = pISShareRegion->AFWindowInfo[cameraId][i].uStartX;
                AF_StartY = pISShareRegion->AFWindowInfo[cameraId][i].uStartY;
                AF_Width = pISShareRegion->AFWindowInfo[cameraId][i].uWidth;
                AF_Height = pISShareRegion->AFWindowInfo[cameraId][i].uHeight;
                m_setParamToBuffer(i * 4 , AF_StartX);
                m_setParamToBuffer(i * 4 + 1 , AF_StartY);
                m_setParamToBuffer(i * 4 + 2 , AF_StartX + AF_Width);
                m_setParamToBuffer(i * 4 + 3 , AF_StartY + AF_Height);

                CLOGD2("\t%d (%d, %d, %d, %d)", i, AF_StartX, AF_StartY, AF_StartX + AF_Width, AF_StartY + AF_Height);
            }
        }
        break;
    case SIMMYCMD_AF_GET_GRAD1:
        CLOGD2("SIMMYCMD_AF_GET_GRAD1");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t grad1_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient1SumLow;
            uint32_t grad1_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient1SumHigh;
            uint32_t grad1_cnt = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient1Count;
            m_setParamToBuffer(1 , grad1_sum_lsb);
            m_setParamToBuffer(2 , grad1_sum_msb);
            m_setParamToBuffer(3 , grad1_cnt);

            CLOGD2("(0x%x,0x%x,0x%x)", grad1_sum_lsb, grad1_sum_msb, grad1_cnt);
        }
        break;
    case SIMMYCMD_AF_GET_GRAD2:
        CLOGD2("SIMMYCMD_AF_GET_GRAD2");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t grad2_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient2SumLow;
            uint32_t grad2_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient2SumHigh;
            uint32_t grad2_cnt = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].gradient2Count;
            m_setParamToBuffer(1 , grad2_sum_lsb);
            m_setParamToBuffer(2 , grad2_sum_msb);
            m_setParamToBuffer(3 , grad2_cnt);

            CLOGD2("(0x%x,0x%x,0x%x)", grad2_sum_lsb, grad2_sum_msb, grad2_cnt);
        }
        break;
    case SIMMYCMD_AF_GET_HPF:
        CLOGD2("SIMMYCMD_AF_GET_HPF");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t hpf_max_sum = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].highpassFilterMaxSum;
            uint32_t hpf_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].highpassFilterSumLow;
            uint32_t hpf_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].highpassFilterSumHigh;
            uint32_t hpf_cnt = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].highpassFilterCount;
            m_setParamToBuffer(1 , hpf_max_sum);
            m_setParamToBuffer(2 , hpf_sum_lsb);
            m_setParamToBuffer(3 , hpf_sum_msb);
            m_setParamToBuffer(4 , hpf_cnt);

            CLOGD2("(0x%x,0x%x,0x%x,0x%x)", hpf_max_sum, hpf_sum_lsb, hpf_sum_msb, hpf_cnt);
        }
        break;
    case SIMMYCMD_AF_GET_FILTER_3x9:
        CLOGD2("SIMMYCMD_AF_GET_FILTER_3x9");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t filter_3x9_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter3x9SumLow;
            uint32_t filter_3x9_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter3x9SumHigh;
            uint32_t filter_3x9_cnt = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter3x9Count;
            m_setParamToBuffer(1 , filter_3x9_sum_lsb);
            m_setParamToBuffer(2 , filter_3x9_sum_msb);
            m_setParamToBuffer(3 , filter_3x9_cnt);

            CLOGD2("(0x%x,0x%x,0x%x)", filter_3x9_sum_lsb, filter_3x9_sum_msb, filter_3x9_cnt);
        }
        break;
    case SIMMYCMD_AF_GET_CLIP_COUNTER:
        CLOGD2("SIMMYCMD_AF_GET_CLIP_COUNTER");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t clip_counter = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].clipCount;
            m_setParamToBuffer(1 , clip_counter);

            CLOGD2("(0x%x)", clip_counter);
        }
        break;
    case SIMMYCMD_AF_GET_Y_SUM:
        CLOGD2("SIMMYCMD_AF_GET_Y_SUM");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t y_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].luminanceSumLow;
            uint32_t y_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].luminanceSumHigh;
            m_setParamToBuffer(1 , y_sum_lsb);
            m_setParamToBuffer(2 , y_sum_msb);

            CLOGD2("(0x%x,0x%x)", y_sum_lsb, y_sum_msb);
        }
        break;

    case SIMMYCMD_AF_GET_FILTER_1x17:
        CLOGD2("SIMMYCMD_AF_GET_FILTER_1x17");
        {
            m_updateShareBase();

            AF_WindowsIndx = data->Parameter[0];
            uint32_t filter_1x17_sum_lsb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter1x17SumLow;
            uint32_t filter_1x17_sum_msb = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter1x17SumHigh;
            uint32_t filter_1x17_cnt     = pISShareRegion->AFStatInfo[cameraId][AF_WindowsIndx].filter1x17Count;
            m_setParamToBuffer(1 , filter_1x17_sum_lsb);
            m_setParamToBuffer(2 , filter_1x17_sum_msb);
            m_setParamToBuffer(3 , filter_1x17_cnt);

            CLOGD2("(0x%x,0x%x,0x%x)", filter_1x17_sum_lsb, filter_1x17_sum_msb, filter_1x17_cnt);
        }
        break;
    case SIMMYCMD_AF_GET_LENS_POSITION:
        CLOGD2("SIMMYCMD_AF_GET_LENS_POSITION");
        {
            uint32_t uAFAlgPosition = shot_ext.shot.udm.lens.pos;
            uint32_t uMacroPosition = shot_ext.shot.udm.af.lensPositionMacro;
            uint32_t uInfPosition = shot_ext.shot.udm.af.lensPositionInfinity;
            m_setParamToBuffer(0, (uint16_t)uAFAlgPosition);
            m_setParamToBuffer(1, uInfPosition);
            m_setParamToBuffer(2, uMacroPosition);

            CLOGD2("uAFAlgPosition %d ,Macro: %d, Inf: %d", uAFAlgPosition, uMacroPosition, uInfPosition);
        }
        break;
    case SIMMYCMD_AF_GET_FOCUS_STATUS:
        CLOGD2("SIMMYCMD_AF_GET_FOCUS_STATUS");
        {
            int32_t writeVal = 1;
            if (shot_ext.shot.dm.aa.afState == AA_AFSTATE_PASSIVE_SCAN
                || shot_ext.shot.dm.aa.afState == AA_AFSTATE_ACTIVE_SCAN) {
                writeVal = 0;
            }

            m_setParamToBuffer(0, writeVal);

            CLOGD2("SIMMYCMD_AF_GET_FOCUS_STATUS(%d)", writeVal);
        }
        break;
    case SIMMYCMD_AF_CHANGE_MODE:
    case SIMMYCMD_AF_SET_WINDOW:
    case SIMMYCMD_AF_SET_ONSCRREN_AF_WINDOW:
    default:
        ret = BAD_VALUE;
        break;
    }

    return ret;
}

status_t ExynosCameraTuningController::m_SystemControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    int pipeId = -1;

    struct camera2_shot_ext shot_ext;
    do {
        ret = ExynosCameraTuner::getInstance()->getShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    ExynosCameraParameters* parameters = ExynosCameraTuner::getInstance()->getParameters();
    ExynosCameraConfigurations* configurations = ExynosCameraTuner::getInstance()->getConfigurations();

    switch (data->commandID)
    {
        case SIMMYCMD_GET_FW_VER:
            CLOGD2("SIMMYCMD_GET_FW_VER");
            {
                char version[60] = {0,};
                m_updateDdkVersion(version);
                memset((char *)(m_cmdBufferAddr + 4), 0x00, SIMMY_RESERVED_PARAM_NUM * 4);
                sprintf((char *)(m_cmdBufferAddr + 4), "%s", version);
            }
            //ret = SimmyCmdApi_SIMMYCMD_GET_FW_VER(cmd);
            break;
        case SIMMYCMD_GET_SIRC_SDK_VER:
            CLOGD2("SIMMYCMD_GET_SIRC_SDK_VER");
            //ret = SimmyCmdApi_SIMMYCMD_GET_SIRC_SDK_VER(cmd);
            break;
        case SIMMYCMD_GET_TUNESETFILE_VER:
            CLOGD2("SIMMYCMD_GET_TUNESETFILE_VER");
            break;
        case SIMMYCMD_GET_SENSOR_INFO:
            CLOGD2("SIMMYCMD_GET_SENSOR_INFO");
            if (parameters != NULL && configurations != NULL) {
                m_setParamToBuffer(1, 0); /* Bayer margin */

                uint32_t sensorW, sensorH;
                parameters->getSize(HW_INFO_HW_SENSOR_SIZE, &sensorW, &sensorH);
                uint32_t sensorSize = (uint32_t)sensorW << 16 | (uint32_t)sensorH;
                m_setParamToBuffer(2, sensorSize); /* Sensor size */

                uint32_t maxFrameRate = (uint32_t)shot_ext.shot.ctl.aa.aeTargetFpsRange[1];
                uint32_t maxSutterSpeed = 0x10000;
                m_setParamToBuffer(3, maxFrameRate << 24 | maxSutterSpeed); /* Framerate */

                ExynosRect bnsRect, bcropRect;
                parameters->getPreviewBayerCropSize(&bnsRect, &bcropRect, true);
                uint32_t bcropSize = (uint32_t)bcropRect.w << 16 | (uint32_t)bcropRect.h;
                m_setParamToBuffer(4, bcropSize); /* Bcrop size */

                uint32_t bnsSize = (uint32_t)bnsRect.w << 16 | (uint32_t)bnsRect.h;
                m_setParamToBuffer(5, bnsSize); /* BNS size */

                ExynosRect bdsRect;
                parameters->getPreviewBdsSize(&bdsRect, true);
                uint32_t bdsSize = (uint32_t)bdsRect.w << 16 | (uint32_t)bdsRect.h;
                m_setParamToBuffer(6, bdsSize); /* BDS size */

                uint32_t previewW, previewH;
                configurations->getSize(CONFIGURATION_PREVIEW_SIZE, &previewW, &previewH);
                uint32_t previewSize = (uint32_t)previewW << 16 | (uint32_t)previewH;
                m_setParamToBuffer(7, previewSize); /* preview size */

                m_setParamToBuffer(8, 0); /* sensor config */
            }

            //ret = SimmyCmdApi_SIMMYCMD_GET_SENSOR_INFO(cmd);
            break;
        case SIMMYCMD_GET_SYSTEM_INFO:
            CLOGD2("SIMMYCMD_GET_SYSTEM_INFO");
            m_setParamToBuffer(1, 0xe9810);
            m_setParamToBuffer(2, 0x0);
            m_setParamToBuffer(3, 0x1);
            m_setParamToBuffer(4, 0x2);
            break;
        case SIMMYCMD_GET_HOST_VER:
            CLOGD2("SIMMYCMD_GET_HOST_VER");
            sprintf((char *)(m_cmdBufferAddr + 4), "%s %s", "OBTE Rev 1.0 for Lassen", "2018/04/04");
            //ret = SimmyCmdApi_SIMMYCMD_GET_HOST_VER(cmd);
            break;
        case SIMMYCMD_GET_A5_LOG:
            CLOGD2("SIMMYCMD_GET_A5_LOG");
            m_setParamToBuffer(1, (unsigned long)m_debugBase);
            m_setParamToBuffer(2, 0x0007D000);
            //ret = SimmyCmdApi_SIMMYCMD_GET_A5_LOG(cmd);
            break;
        case SIMMYCMD_GET_ISPTUNERAW_SETFILEV3:
            CLOGD2("SIMMYCMD_GET_ISPTUNERAW_SETFILEV3");
            break;
        case SIMMYCMD_GET_HOST_LOG:
            CLOGD2("SIMMYCMD_GET_HOST_LOG");
            m_updateDebugBase();
            m_setParamToBuffer(1, (unsigned long)m_debugBase);
            m_setParamToBuffer(2, 0x0007D000);
            break;
            /*
             * +++++++++++++++++++++++++++++++
             *  System Control
             * +++++++++++++++++++++++++++++++
             */
        case SIMMYCMD_SET_POWER_OFF:
            CLOGD2("SIMMYCMD_SET_POWER_OFF");
            //pthread_mutex_lock(&simmianinfo_mutex);
            //stop_preview(cmd);
            //pthread_mutex_unlock(&simmianinfo_mutex);
            break;
        case SIMMYCMD_SET_POWER_ON:
            CLOGD2("SIMMYCMD_SET_POWER_ON");
            break;
        case SIMMYCMD_SET_FW_LOAD:
            CLOGD2("SIMMYCMD_SET_FW_LOAD");
            //ret = SimmyCmdApi_SIMMYCMD_SET_FW_LOAD(cmd);
            break;
        case SIMMYCMD_LOAD_COMPANION_FW:
            CLOGD2("SIMMYCMD_LOAD_COMPANION_FW");
            break;
        case SIMMYCMD_SET_TUNESET_LOAD:
            CLOGD2("SIMMYCMD_SET_TUNESET_LOAD");
            break;
        case SIMMYCMD_LOAD_COMPANION_MASTER_SETFILE:
            CLOGD2("SIMMYCMD_LOAD_COMPANION_MASTER_SETFILE");
            break;
        case SIMMYCMD_LOAD_COMPANION_MODE_SETFILE:
            CLOGD2("SIMMYCMD_LOAD_COMPANION_MODE_SETFILE");
            break;
        case SIMMYCMD_SET_FW_RESET:
            CLOGD2("SIMMYCMD_SET_FW_RESET");
            break;
        case SIMMYCMD_SET_SENSOR_SELECT:
            CLOGD2("SIMMYCMD_SET_SENSOR_SELECT");
            //ret = SimmyCmdApi_SIMMYCMD_SET_SENSOR_SELECT(cmd);
            break;
        case SIMMYCMD_SET_FW_START:
            CLOGD2("SIMMYCMD_SET_FW_START 3AA(%d),ISP(%d)", data->Parameter[2], data->Parameter[1]);
            /*
               camerastopflag = 0;
               cmd->ISPRuningFlag = true;
               eIspId =(IS_ISPID)(data->Parameter[1]);
               uiActiveScenarioId = data->Parameter[3];
               JsonApi_SetTargetISPID(eIspId);
               sem_post(&startpreview);
             */
            {
                int dataPath = data->Parameter[2];
                dataPath += data->Parameter[1] << 1;
                configurations->setModeValue(CONFIGURATION_OBTE_DATA_PATH_NEW, dataPath);
                configurations->setRestartStream(true);
                ret = BAD_VALUE;
            }
            break;
            //  SIMMYCMD_SET_MODE_CHANGE=0x107,
            //  SIMMYCMD_GET_MODE_CHANGE=0x108,
        case SIMMYCMD_SET_CURRENT_SENSOR_FPS_CHANGE:
            CLOGD2("SIMMYCMD_SET_CURRENT_SENSOR_FPS_CHANGE");
            break;
        case SIMMYCMD_SET_MEDIA_STORE_DEVICE:
            CLOGD2("SIMMYCMD_SET_MEDIA_STORE_DEVICE");
            //ret = SimmyCmdApi_SIMMYCMD_SET_MEDIA_STORE_DEVICE(cmd);
            break;
        case SIMMYCMD_SET_LCDBACKLIGHT_ONOFF:
            CLOGD2("SIMMYCMD_SET_LCDBACKLIGHT_ONOFF");
            break;
        case SIMMYCMD_SET_SIMMIAN_SRC_CHANGE:
            CLOGD2("SIMMYCMD_SET_SIMMIAN_SRC_CHANGE (%d)", data->Parameter[0]);

            for(int i = 0; i < MAX_NUM_PIPES; i++) {
                /* HACK: HAL1 preview port */
                if (ExynosCameraTuner::getInstance()->getRequest(i) == true) {
                    ExynosCameraTuner::getInstance()->setRequest(i, false);
                }
            }

            switch (data->Parameter[0]) {
                case 1: // Processed bayer
                    pipeId = PIPE_3AC;
                    break;
                case 6: // Pure bayer
                    pipeId = PIPE_VC0;
                    break;
                    /*
                case 7: // YUV422
                    pipeId = PIPE_ISPC;
                    break;
                    */
                    /*
                case 11: // YUV0
                case 21: // YUV0
                    pipeId = PIPE_MCSC0;
                    break;
                case 12: // YUV1
                case 22: // YUV1
                    pipeId = PIPE_MCSC1;
                    break;
                    */
                    /* recording is not supported
                case 13: // YUV2
                case 23: // YUV2
                    pipeId = PIPE_MCSC2;
                    break;
                     */
                default:
                    CLOGD2("Invalid parameters(%d)", data->Parameter[0]);
                    pipeId = parameters->getPreviewPortId() + PIPE_MCSC0;
                    break;
            }

            if (pipeId != -1) {
                ret = ExynosCameraTuner::getInstance()->setRequest(pipeId, true);
            }

            //ret = SimmyCmdApi_SIMMYCMD_SET_SIMMIAN_SRC_CHANGE(cmd);
            break;
        case SIMMYCMD_UPDATE_SENSOR_INFO:
            CLOGD2("SIMMYCMD_UPDATE_SENSOR_INFO");
            break;
        case SIMMYCMD_CHANGE_ZOOM_FACTOR:
            CLOGD2("SIMMYCMD_CHANGE_ZOOM_FACTOR");
            break;
        case SIMMYCMD_SET_DZOOM_ON_OFF:
            CLOGD2("SIMMYCMD_SET_DZOOM_ON_OFF");
            break;
        case SIMMYCMD_GET_IP_STATUS:
            CLOGD2("SIMMYCMD_GET_IP_STATUS");
            break;
        case SIMMYCMD_GET_FIMC_LITE_SCC_BUFFER:
            CLOGD2("SIMMYCMD_GET_FIMC_LITE_SCC_BUFFER");
            break;
#if (SCALER_YUV_RANGE == 1)
        case SIMMYCMD_SET_YUV_RANGE:
            CLOGD2("SIMMYCMD_SET_YUV_RANGE");
#endif

        //  SIMMYCMD_SET_EMMC_FUSING= 0x116,
        //  SIMMYCMD_GET_FROM_DATA= 0x117,
#if defined(AP_SUPPORT_TUNING_EMULATOR)
    case SIMMYCMD_GET_APACCESS_EMULATOR_BUFFER_ADDR:
        CLOGD2("SIMMYCMD_GET_APACCESS_EMULATOR_BUFFER_ADDR");
        break;
    case SIMMYCMD_GET_APACCESS_EMULATOR_BUFFER_CAPTURE_TO_SD_CARD:
        CLOGD2("SIMMYCMD_GET_APACCESS_EMULATOR_BUFFER_CAPTURE_TO_SD_CARD");
        break;
#endif //#if defined(AP_SUPPORT_TUNING_EMULATOR)
    case SIMMYCMD_SET_JSON_TARGET_ISP:
        CLOGD2("SIMMYCMD_SET_JSON_TARGET_ISP(%d)", data->Parameter[0]);
        //eIspId =(IS_ISPID)(data->Parameter[0]);
        //ret = APAccessApi_SetTargetISPID(eIspId);
        break;
    case SIMMYCMD_GET_JSON_TARGET_ISP:
        CLOGD2("SIMMYCMD_GET_JSON_TARGET_ISP");
        break;
    case SIMMYCMD_SET_COMPANION_CTRL:
        CLOGD2("SIMMYCMD_SET_COMPANION_CTRL");
        break;
    case SIMMYCMD_SET_COMPANION_BYPASS:
        CLOGD2("SIMMYCMD_SET_COMPANION_BYPASS");
        break;
    case SIMMYCMD_GET_COMPANION_CTRL:
        CLOGD2("SIMMYCMD_GET_COMPANION_CTRL");
        break;
    case SIMMYCMD_GET_COMPANION_BYPASS:
        CLOGD2("SIMMYCMD_GET_COMPANION_BYPASS");
        break;
#if (JIG_IHF_TUNING_SUPPORT == 1)
    case SIMMYCMD_GENERAL_I2C_ACCESS_DATA:
        CLOGD2("SIMMYCMD_GENERAL_I2C_ACCESS_DATA");
        break;
    case SIMMYCMD_REQUEST_BUFFER_FOR_GERNEAL_I2C_ACCESS:
        CLOGD2("SIMMYCMD_REQUEST_BUFFER_FOR_GERNEAL_I2C_ACCESS");
        break;
#endif
    case SIMMYCMD_SET_FW_STOP:
        CLOGD2("SIMMYCMD_SET_FW_STOP");
        break;
    default:
        ret = NO_ERROR;
        break;
    }

    return ret;

}

status_t ExynosCameraTuningController::m_ModeChangeControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    switch (data->commandID)
    {
    case SIMMYCMD_SET_MODE_CHANGE:
        CLOGD2("SIMMYCMD_SET_MODE_CHANGE");
        //uint32_t uSubModeIdx      = data->Parameter[5];
        //IS_ISPID eISPId = (IS_ISPID)data->Parameter[18];
        //gCurrentDisplayedISPID = eISPId;
        //APAccessApi_SetTargetISPID(eISPId);
        //APAccessApi_SetTargetISPID(JsonApi_GetTargetISPID());
        //if(gCurrentDisplayedISPID)
        //    gTuningEmulatorCapJsonFlagOn = true;
        //else
        //    gTuningEmulatorCapJsonFlagOn = false;
        //m_metaData.setfile = uSubModeIdx;
        //CLOGD2( "[SIMMYCMD_SET_MODE_CHANGE] %d",gCurrentDisplayedISPID);
        break;
    case SIMMYCMD_SET_SENSOR_MODE_CHANGE:
        CLOGD2("SIMMYCMD_SET_SENSOR_MODE_CHANGE");
        break;
    case SIMMYCMD_GET_MODE_CHANGE:
        CLOGD2("SIMMYCMD_GET_MODE_CHANGE");
        break;
    default:
        ret = NO_ERROR;
        break;
    }

    return ret;
}

status_t ExynosCameraTuningController::m_MCSCControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    ExynosCameraConfigurations* configurations = ExynosCameraTuner::getInstance()->getConfigurations();

    switch (data->commandID) {
    case SIMMYCMD_SET_MCSC_FLIP_CONTROL:
        CLOGD2("SIMMYCMD_SET_MCSC_FLIP_CONTROL (%d)", data->Parameter[0]);

        if ((data->Parameter[0] & SCALER_FLIP_COMMAND_X_MIRROR)
                == SCALER_FLIP_COMMAND_X_MIRROR) {
            configurations->setModeValue(CONFIGURATION_FLIP_VERTICAL, 1);
        }

        if ((data->Parameter[0] & SCALER_FLIP_COMMAND_Y_MIRROR)
                == SCALER_FLIP_COMMAND_Y_MIRROR) {
            configurations->setModeValue(CONFIGURATION_FLIP_VERTICAL, 1);
        }

        break;
    default:
        ret = NO_ERROR;
        break;
    }
    return ret;
}

status_t ExynosCameraTuningController::setVRAControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->forceGetShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    switch (data->commandID) {
    case SIMMYCMD_SET_VRA_START_STOP:
        CLOGD2("SIMMYCMD_SET_VRA_START_STOP (%s)",
                (data->Parameter[0] == 1) ? "Start" : "Stop");

        if (data->Parameter[0] == 1) {
            shot_ext.shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_SIMPLE;
        } else if (data->Parameter[0] == 0) {
            shot_ext.shot.ctl.stats.faceDetectMode = FACEDETECT_MODE_OFF;
        }

        shot_ext.fd_bypass = (shot_ext.shot.ctl.stats.faceDetectMode == FACEDETECT_MODE_OFF) ? 1 : 0;
        break;
    case SIMMYCMD_SET_VRA_ORIENTATION:
    default:
        ret = NO_ERROR;
        break;
    }

    ExynosCameraTuner::getInstance()->forceSetShot(&shot_ext);

    return ret;
}

status_t ExynosCameraTuningController::m_VRAControl(ExynosCameraTuningCommand::t_data* data)
{
    status_t ret = NO_ERROR;
    CLOGD2("");

    uint32_t uOrientationValue = 0;
    ExynosCameraActivityControl *activityCtl = NULL;
    ExynosCameraActivityUCTL *uctlMgr = NULL;
    ExynosCameraParameters* parameters = ExynosCameraTuner::getInstance()->getParameters();

    switch (data->commandID) {
    case SIMMYCMD_SET_VRA_ORIENTATION:
        uOrientationValue = data->Parameter[0];
        CLOGD2("SIMMYCMD_SET_VRA_ORIENTATION %d degrees", uOrientationValue);

        if (!(uOrientationValue == 0 || uOrientationValue == 90 || uOrientationValue == 180 || uOrientationValue == 270))
        {
            CLOGD2("[SIMMYCMD_SET_VRA_ORIENTATION] : Wrong FD orientation angle[%d]!!! Force to change to degree 0!!!", uOrientationValue);
            uOrientationValue = 0;
        }

        activityCtl = parameters->getActivityControl();

        if (activityCtl != NULL) {
            uctlMgr = activityCtl ->getUCTLMgr();
        }
        if (uctlMgr != NULL) {
            uctlMgr->setDeviceRotation(uOrientationValue);
        }
        break;
    case SIMMYCMD_SET_VRA_START_STOP:
        CLOGD2("SIMMYCMD_SET_VRA_START_STOP (%s)",
                (data->Parameter[0] == 1) ? "Start" : "Stop");
    default:
        ret = BAD_VALUE;
        break;
    }
    return ret;
}

status_t ExynosCameraTuningController::m_getControl(ExynosCameraTuningCommand::t_data* data)
{
    CLOGD2("");
    status_t ret = NO_ERROR;
    uint8_t writeData[4] = {0,};
    uint32_t writeAddr = m_getPhyAddr(data->commandID);
    int writeRet = -1;

    if (writeAddr == 0xFFFFFFFF) {
        writeAddr = 0;
    }

    for (int i = 0; i < 4; i++) {
        writeData[i] = (uint8_t)(writeAddr >> (8 * i));
    }

    writeRet = write(m_socketCmdFD, writeData, 4);
    if (writeRet < 0 ) {
        CLOGD2("Write fail, size(%d)", writeRet);
    }

    CLOGD2("SY:write(%d) %x,%x,%x,%x", writeRet, writeData[0], writeData[1], writeData[2], writeData[3]);
    return ret;
}

uint32_t ExynosCameraTuningController::m_getPhyAddr(uint32_t commandID)
{
    CLOGD2("");
    uint32_t uRetVal = 0;

    switch (commandID)
    {
        case ADDR_GET_SIMMYCMD_MAGIC:
            uRetVal = SIMMY_MAGIC;
            CLOGD2("ADDR_GET_SIMMYCMD_MAGIC(%u)", uRetVal);
            break;
        case ADDR_GET_SIMMYCMD_BASE_ADDR:
            uRetVal = (unsigned long)m_cmdBufferAddr;
            CLOGD2("ADDR_GET_SIMMYCMD_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_SIMMYCMD_AF_LOG_BASE_ADDR:
            m_updateAFLog();
            uRetVal = (unsigned long)m_afLogBufferAddr;
            CLOGD2("ADDR_GET_SIMMYCMD_AF_LOG_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_A5_FWLOAD_BASE_ADDR:
            uRetVal = (unsigned long)m_fwBufferAddr;
            CLOGD2("ADDR_GET_A5_FWLOAD_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_SETFILE_BASE_ADDR:
            uRetVal = (unsigned long)m_fwBufferAddr;
            CLOGD2("ADDR_GET_SETFILE_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_SIMULATOR_BAYER_IMAGE_ADDR:
            uRetVal = (unsigned long)(ExynosCameraTuner::getInstance()->getBufferAddr(T_BAYER_IN_BUFFER_MGR));
            CLOGD2("ADDR_GET_SIMULATOR_BAYER_IMAGE_ADDR(%u)", uRetVal);
            //uRetVal = (ulong)gTuningEmulatorBayerBufAddr;
            break;
        case ADDR_GET_COMPANION_FW_ADDR:
            uRetVal = 0xffffffff;
            break;
        case ADDR_GET_COMPANION_MASTER_SETFILE_ADDR:
            uRetVal = 0xffffffff;
            break;
        case ADDR_GET_COMPANION_MODE_SETFILE_ADDR:
            uRetVal = 0xffffffff;
            break;
        case ADDR_GET_SETFILE_BASE_ADDR_FRONT:
            uRetVal = (unsigned long)m_fwBufferAddr;
            CLOGD2("ADDR_GET_SETFILE_BASE_ADDR_FRONT(%u)", uRetVal);
            break;
        case ADDR_GET_RTA_BIN_BASE_ADDR:
            uRetVal = (unsigned long)m_fwBufferAddr;
            CLOGD2("ADDR_GET_RTA_BIN_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_VRA_BIN_BASE_ADDR:
            uRetVal = (unsigned long)m_fwBufferAddr;
            CLOGD2("ADDR_GET_VRA_BIN_BASE_ADDR(%u)", uRetVal);
            break;
        case ADDR_GET_EXIF_DUMP_BASE_ADDR:
            uRetVal = (unsigned long)m_exifBufferAddr;
            CLOGD2("ADDR_GET_EXIF_DUMP_BASE_ADDR(%u)", uRetVal);
            break;
        default:
            CLOGD2("Default(%u, %x)", commandID, commandID);
            uRetVal = *(uint32_t *)(unsigned long)(commandID|uRetVal);
            break;
    }

    CLOGD2("cmd(%x), ret(%u, %x)", commandID, uRetVal, uRetVal);
    return uRetVal;
}

status_t ExynosCameraTuningController::m_setParamToBuffer(uint32_t index, uint32_t data)
{
    CLOGD2("index(%d), data(%d)", index, data);
    *((uint32_t *)(m_cmdBufferAddr + (4 * (index + 1)))) = data;
    return NO_ERROR;
}

uint32_t ExynosCameraTuningController::m_getParamFromBuffer(uint32_t index)
{
    uint32_t data = *((uint32_t *)(m_cmdBufferAddr + (4 * (index + 1))));
    CLOGD2("index(%d), data(%d)", index, data);
    return data;
}

status_t ExynosCameraTuningController::m_updateShareBase(void)
{
    status_t ret = NO_ERROR;

    ret = ioctl(m_tuningFD, (unsigned int)FIMC_IS_SHAREBASE_READ, m_shareBase);
    if (ret < 0) {
        CLOGD2("exynos_v4l2_s_ctrl fd(fd:%d) fail (%d) [id %x]",
                m_tuningFD, ret, (unsigned int)FIMC_IS_SHAREBASE_READ);
    }

    return ret;
}

status_t ExynosCameraTuningController::m_updateDebugBase(void)
{
    status_t ret = NO_ERROR;

    ret = ioctl(m_tuningFD, (unsigned int)FIMC_IS_DEBUGBASE_READ, m_debugBase);
    if (ret < 0) {
        CLOGD2("exynos_v4l2_s_ctrl fd(fd:%d) fail (%d) [id %x]",
                m_tuningFD, ret, (unsigned int)FIMC_IS_DEBUGBASE_READ);
    }

    return ret;
}

status_t ExynosCameraTuningController::m_updateAFLog(void)
{
    status_t ret = NO_ERROR;

    ret = ioctl(m_tuningFD, (unsigned int)FIMC_IS_AFLOG_READ, m_afLogBufferAddr);
    if (ret < 0) {
        CLOGD2("exynos_v4l2_s_ctrl fd(fd:%d) fail (%d) [id %x]",
                m_tuningFD, ret, (unsigned int)FIMC_IS_DEBUGBASE_READ);
    }

    return ret;
}

status_t ExynosCameraTuningController::m_updateDdkVersion(char* version)
{
    status_t ret = NO_ERROR;

    ret = ioctl(m_tuningFD, (unsigned int)FIMC_IS_DDK_VERSION, version);
    if (ret < 0) {
        CLOGD2("exynos_v4l2_s_ctrl fd(fd:%d) fail (%d) [id %x]",
                m_tuningFD, ret, (unsigned int)FIMC_IS_DEBUGBASE_READ);
    }

    return ret;
}

status_t ExynosCameraTuningController::m_updateExif(void)
{
    status_t ret = NO_ERROR;
    struct camera2_shot_ext shot_ext;

    do {
        ret = ExynosCameraTuner::getInstance()->getShot(&shot_ext);
        if (ret != NO_ERROR) {
            usleep(100000);
        }
    } while(ret != NO_ERROR);

    DMforExif *exifData = (DMforExif *)(m_exifBufferAddr);
    if (exifData != NULL) {
        exifData->usize = sizeof(UDMforExif);
        memcpy(&exifData->udmForExif.ae, &shot_ext.shot.udm.ae, sizeof(struct camera2_ae_udm));
        memcpy(&exifData->udmForExif.awb, &shot_ext.shot.udm.awb, sizeof(struct camera2_awb_udm));
        memcpy(&exifData->udmForExif.af, &shot_ext.shot.udm.af, sizeof(struct camera2_af_udm));
        memcpy(&exifData->udmForExif.ipc, &shot_ext.shot.udm.ipc, sizeof(struct camera2_ipc_udm));
    }

    return NO_ERROR;
}

}; //namespace android
