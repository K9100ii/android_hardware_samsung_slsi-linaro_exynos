#define LOG_TAG "ExynosCameraTuningInterface"

#include "include/ExynosCameraTuningInterface.h"
#include "ExynosCameraTuner.h"
#include "ExynosCameraFrameFactory.h"

namespace android {
#if 1
status_t ExynosCameraTuningInterface::start() {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::start(__unused EXYNOSCAMERA_FRAME_FACTORY* previewFrameFactory, __unused EXYNOSCAMERA_FRAME_FACTORY* captureFrameFactory) {
    return NO_ERROR;
}


status_t ExynosCameraTuningInterface::stop() {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::updateShot(__unused struct camera2_shot_ext* shot) {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::updateImage(__unused ExynosCameraFrameSP_sptr_t frame, __unused int pipeId, __unused EXYNOSCAMERA_FRAME_FACTORY* factory) {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::setConfigurations(__unused ExynosCameraConfigurations *configurations) {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::setParameters(__unused ExynosCameraParameters *parameters) {
    return NO_ERROR;
}

status_t ExynosCameraTuningInterface::setRequest(__unused int pipeId, __unused bool enable) {
    return NO_ERROR;
}

bool ExynosCameraTuningInterface::getRequest(__unused int pipeId) {
    return NO_ERROR;
}
#else
status_t exynoscameratuninginterface::start() {
    clogd2("exynoscameratuninginterface::start()");

    return exynoscameratuner::getinstance()->start();

}

status_t exynoscameratuninginterface::start(exynoscamera_frame_factory* previewframefactory, exynoscamera_frame_factory* captureframefactory) {
    clogd2("exynoscameratuninginterface::start()");

    return exynoscameratuner::getinstance()->start(previewframefactory, captureframefactory);
}


status_t exynoscameratuninginterface::stop() {
    clogd2("exynoscameratuninginterface::stop()");

    return exynoscameratuner::getinstance()->stop();
}

status_t exynoscameratuninginterface::updateshot(struct camera2_shot_ext* shot) {

    return exynoscameratuner::getinstance()->updateshot(shot, tune_meta_set_target_sensor, tune_meta_set_from_hal);
}

status_t exynoscameratuninginterface::updateimage(exynoscameraframesp_sptr_t frame, int pipeid, exynoscamera_frame_factory* factory) {

    return exynoscameratuner::getinstance()->updateimage(frame, pipeid, factory);
}

status_t exynoscameratuninginterface::setconfigurations(exynoscameraconfigurations *configurations) {

    return exynoscameratuner::getinstance()->setconfigurations(configurations);
}

status_t exynoscameratuninginterface::setparameters(exynoscameraparameters *parameters) {

    return exynoscameratuner::getinstance()->setparameters(parameters);
}

status_t exynoscameratuninginterface::setrequest(int pipeid, bool enable) {
    return exynoscameratuner::getinstance()->setrequest(pipeid, enable);
}

bool exynoscameratuninginterface::getrequest(int pipeid) {
    return exynoscameratuner::getinstance()->getrequest(pipeid);
}
#endif
}; //namespace android

