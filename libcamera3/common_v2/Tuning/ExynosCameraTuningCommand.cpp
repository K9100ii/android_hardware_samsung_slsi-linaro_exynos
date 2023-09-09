#define LOG_TAG "ExynosCameraTuningCommand"

#include <string.h> //memcyp
#include "ExynosCameraTuningCommand.h"

namespace android {

ExynosCameraTuningCommand::ExynosCameraTuningCommand()
{
    initData();
}

ExynosCameraTuningCommand::~ExynosCameraTuningCommand()
{
    CLOGE2("###################### deleted command ##################");
}

status_t ExynosCameraTuningCommand::setData(t_data *data) {
    memcpy(&cmd.data, data, sizeof(t_data));
    return NO_ERROR;
}

void ExynosCameraTuningCommand::initData(void) {
    cmd.commandGroup = 0;
    cmd.module = 0;
    cmd.data.commandID = 0;

    cmd.data.moduleCmd.command = 0;
    cmd.data.moduleCmd.subCommand = 0;

    memset(&cmd.data.Parameter, 0, sizeof(SIMMIAN_CTRL_MAXPACKET));
}

}; //namespace android
