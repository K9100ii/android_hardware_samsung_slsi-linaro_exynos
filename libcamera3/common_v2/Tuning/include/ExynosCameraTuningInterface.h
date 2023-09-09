#ifndef EXYNOS_CAMERA_TUNING_INTERFACE_H
#define EXYNOS_CAMERA_TUNING_INTERFACE_H

#include "utils/Errors.h"
#include "fimc-is-metadata.h"
#include "ExynosCameraTuningTypes.h"

namespace android {

class ExynosCameraTuner;
class EXYNOSCAMERA_FRAME_FACTORY;

class ExynosCameraTuningInterface {
public:
    static status_t start();

    static status_t start(EXYNOSCAMERA_FRAME_FACTORY* previewFrameFactory, EXYNOSCAMERA_FRAME_FACTORY* captureFrameFactory);

    static status_t stop();

    static status_t updateShot(struct camera2_shot_ext* shot);

    static status_t updateImage(ExynosCameraFrameSP_sptr_t frame, int pipeId, EXYNOSCAMERA_FRAME_FACTORY* factory);

    static status_t setConfigurations(ExynosCameraConfigurations *configurations);
    static status_t setParameters(ExynosCameraParameters *parameters);

    static status_t setRequest(int pipeId, bool enable);
    static bool     getRequest(int pipeId);
};
}; //namespace android

#endif //EXYNOS_CAMERA_TUNING_INTERFACE_H
