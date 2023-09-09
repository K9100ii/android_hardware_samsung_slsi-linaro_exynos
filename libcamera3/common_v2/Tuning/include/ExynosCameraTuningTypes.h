#ifndef EXYNOS_CAMERA_TUNING_TYPE_H__
#define EXYNOS_CAMERA_TUNING_TYPE_H__

#include "utils/Errors.h"
#include "utils/RefBase.h"

#include "ExynosCameraCommonDefine.h"
#include "ExynosCameraFrame.h"
#include "ExynosCameraBufferManager.h"

namespace android {

class ExynosCameraTuningCommand;

typedef sp<ExynosCameraTuningCommand> ExynosCameraTuningCmdSP_sptr_t;
typedef sp<ExynosCameraTuningCommand>& ExynosCameraTuningCmdSP_dptr_t;

typedef sp<ExynosCameraFrame>  ExynosCameraFrameSP_sptr_t; /* single ptr */

#ifdef USE_HAL3_CAMERA
#define EXYNOSCAMERA_FRAME_FACTORY ExynosCamera3FrameFactory
#else
#define EXYNOSCAMERA_FRAME_FACTORY ExynosCameraFrameFactory
#endif


};

#endif //EXYNOS_CAMERA_TUNING_TYPE_H__

