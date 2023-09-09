#ifndef EXYNOS_CAMERA_TUNING_COMMAND_H__
#define EXYNOS_CAMERA_TUNING_COMMAND_H__

#ifdef _WIN32_
#include "include/WIN32/utils/types.h"
#else
#include "ExynosCameraTuningTypes.h"
#endif

#include "include/ExynosCameraTuningDefines.h"

namespace android {

class ExynosCameraTuningCommand : public RefBase
{

public:
    typedef enum {
        SET_CONTROL = 0,
        GET_CONTROL,
        WRITE_IMAGE,
        READ_IMAGE,
        READ_DATA,
        UPDATE_EXIF,
        WRITE_JSON,
        READ_JSON,
    }T_EXYNOS_TUNE_CMD_GROUP;

    typedef struct {
        int command;
        int subCommand;
        uint32_t length;
    } t_module_cmd;

    typedef struct {
        uint32_t commandID;
        unsigned long commandAddr;
        t_module_cmd moduleCmd;
        uint32_t Parameter[SIMMIAN_CTRL_MAXPACKET];
    } t_data;

private:
    struct TuneCommand {
        int module;
        int commandGroup;
        t_data data;
    };

public:
    ExynosCameraTuningCommand();
    virtual ~ExynosCameraTuningCommand();

public:
    int getModule() { return cmd.module; }

    status_t setModule(int module) { cmd.module = module; return NO_ERROR; }

    int getCommandGroup() { return cmd.commandGroup; }

    status_t setCommandGroup(int command) { cmd.commandGroup = command; return NO_ERROR; }

    t_data* getData() { return &cmd.data; }

    status_t setData(t_data *data);

    void initData(void);

private:
    struct TuneCommand cmd;
};

}; //namespace android

#endif //EXYNOS_CAMERA_TUNING_COMMAND__
