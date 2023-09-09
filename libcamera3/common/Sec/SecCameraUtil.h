/*
**
** Copyright 2015, Samsung Electronics Co. LTD
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

#ifndef SEC_CAMERA_UTIL_H
#define SEC_CAMERA_UTIL_H

#include <cutils/properties.h>
#include <utils/threads.h>
#include <utils/String8.h>

#include "ExynosCameraConfig.h"

#include "exynos_format.h"
#include "ExynosRect.h"

#include "fimc-is-metadata.h"

#include "ExynosCameraSensorInfo.h"
#include "videodev2_exynos_media.h"
#include "ExynosCameraBuffer.h"

#ifdef SAMSUNG_SENSOR_LISTENER
#include "sensor_listener_wrapper.h"
#endif

#ifdef SAMSUNG_QUICK_SWITCH
enum QuickSwitch_prop {
    QUICK_SWITCH_START_BACK = 2,
    QUICK_SWITCH_START_FRONT
};
#endif

namespace android {

enum ssrmCameraInfoOperation{
    SSRM_CAMERA_INFO_CLEAR,
    SSRM_CAMERA_INFO_SET,
    SSRM_CAMERA_INFO_UPDATE,
};

struct ssrmCameraInfo{
    int operation;
    int cameraId;
    int width;
    int height;
    int minFPS;
    int maxFPS;
    int sensorOn;
};

int checkFpsProperty(void);
bool checkBinningProperty(void);
bool checkCCProperty(void);
#ifdef USE_PREVIEW_CROP_FOR_ROATAION
int checkRotationProperty(void);
#endif
bool isCompanion(int cameraId);
bool isEEprom(int cameraId);
bool isFastenAeStable(int cameraId, bool useCompanion);
bool isLDCapture(int cameraId);
#ifdef SAMSUNG_QUICK_SWITCH
bool checkQuickSwitchProperty(void);
#endif
#ifdef SAMSUNG_FRONT_LCD_FLASH
void setHighBrightnessModeOfLCD(int on, char *prevHBM, char *prevAutoHBM);
#endif
#ifdef USE_SUBDIVIDED_EV
void setMetaCtlExposureCompensationStep(struct camera2_shot_ext *shot_ext, float expCompensationStep);
#endif

void setMetaCtlRTDrc(struct camera2_shot_ext *shot_ext, enum companion_drc_mode mode);
void getMetaCtlRTDrc(struct camera2_shot_ext *shot_ext, enum companion_drc_mode *mode);
void setMetaCtlPaf(struct camera2_shot_ext *shot_ext, enum companion_paf_mode mode);
void getMetaCtlPaf(struct camera2_shot_ext *shot_ext, enum companion_paf_mode *mode);
void setMetaCtlRTHdr(struct camera2_shot_ext *shot_ext, enum companion_wdr_mode mode);
void getMetaCtlRTHdr(struct camera2_shot_ext *shot_ext, enum companion_wdr_mode *mode);

#ifdef SAMSUNG_OIS
void setMetaCtlOIS(struct camera2_shot_ext *shot_ext, enum optical_stabilization_mode mode);
void getMetaCtlOIS(struct camera2_shot_ext *shot_ext, enum optical_stabilization_mode *mode);
#endif

#ifdef SAMSUNG_MANUAL_FOCUS
void setMetaCtlFocusDistance(struct camera2_shot_ext *shot_ext, float distance);
void getMetaCtlFocusDistance(struct camera2_shot_ext *shot_ext, float *distance);
#endif

#ifdef SAMSUNG_DOF
void setMetaCtlLensPos(struct camera2_shot_ext *shot_ext, int value);
#endif

#ifdef SAMSUNG_HRM
void setMetaCtlHRM(struct camera2_shot_ext *shot_ext, int ir_data, int flicker_data, int status);
#endif
#ifdef SAMSUNG_LIGHT_IR
void setMetaCtlLight_IR(struct camera2_shot_ext *shot_ext, SensorListenerEvent_t data);
#endif
#ifdef SAMSUNG_GYRO
void setMetaCtlGyro(struct camera2_shot_ext *shot_ext, SensorListenerEvent_t data);
#endif
#ifdef SAMSUNG_ACCELEROMETER
void setMetaCtlAcceleration(struct camera2_shot_ext * shot_ext,SensorListenerEvent_t data);
#endif
#ifdef USE_FW_ZOOMRATIO
void setMetaCtlZoom(struct camera2_shot_ext *shot_ext, float data);
#endif
#ifdef SAMSUNG_OIS_VDIS
void setMetaCtlOISCoef(struct camera2_shot_ext *shot_ext, uint32_t data);
#endif
void setMetaUctlFlashMode(struct camera2_shot_ext *shot_ext, enum camera_flash_mode mode);
void setMetaUctlOPMode(struct camera2_shot_ext *shot_ext, enum camera_op_mode mode);
#ifdef SAMSUNG_OIS
char *getOisEXIFFromFile(struct ExynosCameraSensorInfoBase *info, int mode);
#endif
#ifdef SAMSUNG_MTF
char *getMTFdataEXIFFromFile(struct ExynosCameraSensorInfoBase * info, int camid);
#endif
char *getSensorIdEXIFFromFile(struct ExynosCameraSensorInfoBase *info, int cameraId, int *realDataSize);

void getAWBCalibrationGain(int32_t *calibrationRG, int32_t *calibrationBG, float f_masterRG, float f_masterBG);
void storeSsrmCameraInfo(struct ssrmCameraInfo *info);

}

#endif
