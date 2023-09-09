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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "SecCameraUtil"
#include <cutils/log.h>

#include "SecCameraUtil.h"

namespace android {

#ifdef SENSOR_NAME_GET_FROM_FILE
#define SENSOR_NAME_PATH_BACK "/sys/class/camera/rear/rear_sensorid"
#define SENSOR_NAME_PATH_FRONT "/sys/class/camera/front/front_sensorid"
#endif
#define SENSOR_FW_PATH_BACK "/sys/class/camera/rear/rear_camfw"
#define SENSOR_FW_PATH_FRONT "/sys/class/camera/front/front_camfw"
#ifdef SAMSUNG_OIS
#define OIS_EXIF_PATH_BACK "/sys/class/camera/ois/ois_exif"
#endif
#define SENSOR_ID_EXIF_PATH_BACK "/sys/class/camera/rear/rear_sensorid_exif"
#define SENSOR_ID_EXIF_PATH_FRONT "/sys/class/camera/front/front_sensorid_exif"

#ifdef SAMSUNG_FRONT_LCD_FLASH
#define HBM_DATA_MAX_LEN    4
#define HBM_BRIGHTNESS_DATA "255"
#define HBM_AUTO_BRIGHTNESS_DATA    "12"
#define HBM_BACKUP_DEFUALT_STRING   "LCDF"
#define HBM_AUTO_BRIGHTNESS_LEVEL_MAX   "12"
#define HBM_BRIGHTNESS  "/sys/class/backlight/panel/brightness"
#define HBM_AUTO_BRIGHTNESS  "/sys/class/backlight/panel/auto_brightness"
#define HBM_AUTO_BRIGHTNESS_LEVEL  "/sys/class/backlight/panel/auto_brightness_level"
#endif

int checkPropertyForShotMode(int newShotMode, __unused int vtMode)
{
    int shotMode = newShotMode;
    char propertyValue[PROPERTY_VALUE_MAX];

#ifdef USE_LIMITATION_FOR_THIRD_PARTY
    property_get("sys.cameramode.blackbox", propertyValue, "0");
    if (strcmp(propertyValue, "1") == 0) {
        shotMode = THIRD_PARTY_BLACKBOX_MODE;
    } else {
        property_get("sys.hangouts.fps", propertyValue, "0");
        int newHangOutFPS = atoi(propertyValue);
        if (newHangOutFPS > 0) {
            shotMode = THIRD_PARTY_HANGOUT_MODE;
        } else {
            if ((vtMode <= 0) || (vtMode > 2)) {
                property_get("sys.cameramode.vtcall", propertyValue, "0");
                if (strcmp(propertyValue, "1") == 0) {
                    shotMode = THIRD_PARTY_VTCALL_MODE;
                }
            }
        }
    }
#endif
    return shotMode;
}

bool checkProperty(bool check_cc)
{
    bool ret = false;
    char propertyValue[PROPERTY_VALUE_MAX];

    if (check_cc) {
        property_get("system.camera.CC.disable", propertyValue, "0");
        if (strcmp(propertyValue, "0") != 0) {
            return true;
        } else {
            property_get("system.camera.CC.fac.disable", propertyValue, "0");
            if (strcmp(propertyValue, "1") == 0) {
                property_set("system.camera.CC.fac.disable", "0");
                return true;
            }
        }
    }

    property_get("sys.cameramode.blackbox", propertyValue, "0");
    if (strcmp(propertyValue, "1") == 0) {
        ret = true;
    } else {
        int newHangOutFPS = 0;
        property_get("sys.hangouts.fps", propertyValue, "0");
        newHangOutFPS = atoi(propertyValue);
        if (newHangOutFPS > 0) {
            ret = true;
        } else {
            property_get("sys.cameramode.vtcall", propertyValue, "0");
            if (strcmp(propertyValue, "1") == 0) {
                ret = true;
            }
        }
    }
    return ret;
}

#ifdef SAMSUNG_QUICK_SWITCH
bool checkQuickSwitchProperty(int cameraId)
{
    bool ret = false;
    char propertyValue[PROPERTY_VALUE_MAX];

    if (cameraId == CAMERA_ID_BACK) {
        property_get("system.camera.CC.disable", propertyValue, "0");
        if ((strcmp(propertyValue, "0") != 0)
            && !(strcmp(propertyValue, "1") == 0))
            return true;
    } else {
        return checkProperty(true);
    }

    return ret;
}
#endif

#ifdef USE_PREVIEW_CROP_FOR_ROATAION
int checkRotationProperty()
{
    char propertyValue[PROPERTY_VALUE_MAX];

    property_get("system.camera.force.preview", propertyValue, "0");
    ALOGI("INFO(%s[%d]): set rotation property %s", __FUNCTION__, __LINE__, propertyValue);
    if (strcmp(propertyValue, "1") == 0) {
        return FORCE_PREVIEW_WINDOW_SET_CROP;
    } else if (strcmp(propertyValue, "2") == 0) {
        return FORCE_PREVIEW_BUFFER_CROP_ROTATION_270;
    }
    return 0;
}
#endif

bool isCompanion(int cameraId)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
#ifdef MAIN_CAMERA_USE_SAMSUNG_COMPANION
#ifdef SAMSUNG_QUICK_SWITCH
        if (MAIN_CAMERA_USE_SAMSUNG_COMPANION) {
            if (checkQuickSwitchProperty(cameraId) == true) {
                ret = false; /* not use */
            } else {
                ret = true; /* use */
            }
        }
#else
        ret = MAIN_CAMERA_USE_SAMSUNG_COMPANION;
#endif
#else
        ALOGI("INFO(%s[%d]): MAIN_CAMERA_USE_SAMSUNG_COMPANION is not defined", __FUNCTION__, __LINE__);
#endif
    } else if (cameraId == CAMERA_ID_SECURE) {
        ret = false; /* not use */
    } else {
#ifdef FRONT_CAMERA_USE_SAMSUNG_COMPANION
        if (FRONT_CAMERA_USE_SAMSUNG_COMPANION) {
            if (checkProperty(true) == true) {
                ret = false; /* not use */
            } else {
                ret = true; /* use */
            }
        }
#else
        ALOGI("INFO(%s[%d]): FRONT_CAMERA_USE_SAMSUNG_COMPANION is not defined", __FUNCTION__, __LINE__);
#endif
    }

    return ret;
}

bool isEEprom(int cameraId)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
#ifdef SAMSUNG_EEPROM_REAR
        ret = SAMSUNG_EEPROM_REAR;
#else
        ALOGI("INFO(%s[%d]): SAMSUNG_EEPROM_REAR is not defined", __FUNCTION__, __LINE__);
#endif
    } else {
#ifdef SAMSUNG_EEPROM_FRONT
        ret = SAMSUNG_EEPROM_FRONT;
#else
        ALOGI("INFO(%s[%d]): SAMSUNG_EEPROM_FRONT is not defined", __FUNCTION__, __LINE__);
#endif
    }

    return ret;
}

bool isFastenAeStable(int cameraId, __unused bool useCompanion)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
#ifdef USE_FASTEN_AE_STABLE
#ifdef SAMSUNG_QUICK_SWITCH
        if(useCompanion) {
            ret = USE_FASTEN_AE_STABLE;
        } else {
            ret = false;
        }
#else /* SAMSUNG_QUICK_SWITCH */
        ret = USE_FASTEN_AE_STABLE;
#endif /* SAMSUNG_QUICK_SWITCH */
#else
        ALOGI("INFO(%s[%d]): USE_FASTEN_AE_STABLE is not defined", __FUNCTION__, __LINE__);
#endif
    } else {
#ifdef USE_FASTEN_AE_STABLE_FRONT
        if(useCompanion) {
            ret = USE_FASTEN_AE_STABLE_FRONT;
        } else {
            ret = false;
        }
#else
        ALOGI("INFO(%s[%d]): USE_FASTEN_AE_STABLE_FRONT is not defined", __FUNCTION__, __LINE__);
#endif
    }

    return ret;
}

/* LLS_Deblur Capture */
bool isLDCapture(int cameraId)
{
    bool ret = false;

    if (cameraId == CAMERA_ID_BACK) {
#ifdef SAMSUNG_LDCAPTURE_REAR
        ret = SAMSUNG_LDCAPTURE_REAR;
#else
        ALOGI("INFO(%s[%d]): SAMSUNG_LDCAPTURE_REAR is not defined", __FUNCTION__, __LINE__);
#endif
    } else {
#ifdef SAMSUNG_LDCAPTURE_FRONT
        ret = SAMSUNG_LDCAPTURE_FRONT;
#else
        ALOGI("INFO(%s[%d]): SAMSUNG_LDCAPTURE_FRONT is not defined", __FUNCTION__, __LINE__);
#endif
    }

    return ret;
}

void setMetaCtlRTDrc(struct camera2_shot_ext *shot_ext, enum companion_drc_mode mode)
{
    shot_ext->shot.uctl.companionUd.drc_mode = mode;
}

void getMetaCtlRTDrc(struct camera2_shot_ext *shot_ext, enum companion_drc_mode *mode)
{
    *mode = shot_ext->shot.uctl.companionUd.drc_mode;
}

void setMetaCtlPaf(struct camera2_shot_ext *shot_ext, enum companion_paf_mode mode)
{
    shot_ext->shot.uctl.companionUd.paf_mode = mode;
}

void getMetaCtlPaf(struct camera2_shot_ext *shot_ext, enum companion_paf_mode *mode)
{
    *mode = shot_ext->shot.uctl.companionUd.paf_mode;
}

void setMetaCtlRTHdr(struct camera2_shot_ext *shot_ext, enum companion_wdr_mode mode)
{
    shot_ext->shot.uctl.companionUd.wdr_mode = mode;
}

void getMetaCtlRTHdr(struct camera2_shot_ext *shot_ext, enum companion_wdr_mode *mode)
{
    *mode = shot_ext->shot.uctl.companionUd.wdr_mode;
}

#ifdef SAMSUNG_OIS
void setMetaCtlOIS(struct camera2_shot_ext *shot_ext, enum optical_stabilization_mode mode)
{
    shot_ext->shot.ctl.lens.opticalStabilizationMode = mode;
}

void getMetaCtlOIS(struct camera2_shot_ext *shot_ext, enum optical_stabilization_mode *mode)
{
    *mode = shot_ext->shot.ctl.lens.opticalStabilizationMode;
}
#endif

#ifdef SAMSUNG_MANUAL_FOCUS
void setMetaCtlFocusDistance(struct camera2_shot_ext *shot_ext, float distance)
{
    shot_ext->shot.ctl.lens.focusDistance = distance;
}

void getMetaCtlFocusDistance(struct camera2_shot_ext *shot_ext, float *distance)
{
    *distance = shot_ext->shot.ctl.lens.focusDistance;
}
#endif

#ifdef SAMSUNG_DOF
void setMetaCtlLensPos(struct camera2_shot_ext *shot_ext, int value)
{
    shot_ext->shot.uctl.lensUd.pos = value;
    shot_ext->shot.uctl.lensUd.posSize = 10;
    shot_ext->shot.uctl.lensUd.direction = 0;
    shot_ext->shot.uctl.lensUd.slewRate = 0;

    ALOGD("[DOF][%s][%d] lens pos : %d", __func__, __LINE__, value);
}
#endif

#ifdef SAMSUNG_HRM
void setMetaCtlHRM(struct camera2_shot_ext *shot_ext, int ir_data, int flicker_data, int status)
{
    shot_ext->shot.uctl.aaUd.hrmInfo.ir_data = ir_data;
    shot_ext->shot.uctl.aaUd.hrmInfo.flicker_data = flicker_data;
    shot_ext->shot.uctl.aaUd.hrmInfo.status = status;
}
#endif
#ifdef SAMSUNG_LIGHT_IR
void setMetaCtlLight_IR(struct camera2_shot_ext *shot_ext, SensorListenerEvent_t data)
{
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_cdata = data.light_ir.light_white;
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_rdata = data.light_ir.light_red;
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_gdata = data.light_ir.light_green;
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_bdata = data.light_ir.light_blue;
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_gain = data.light_ir.ir_again;
    shot_ext->shot.uctl.aaUd.illuminationInfo.visible_exptime = data.light_ir.ir_atime;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_north = data.light_ir.ir_data;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_south = data.light_ir.ir_data;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_east = data.light_ir.ir_data;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_west = data.light_ir.ir_data;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_gain = data.light_ir.ir_again;
    shot_ext->shot.uctl.aaUd.illuminationInfo.ir_exptime = data.light_ir.ir_atime;
}
#endif
#ifdef SAMSUNG_GYRO
void setMetaCtlGyro(struct camera2_shot_ext *shot_ext, SensorListenerEvent_t data)
{
    shot_ext->shot.uctl.aaUd.gyroInfo.x = data.gyro.x;
    shot_ext->shot.uctl.aaUd.gyroInfo.y = data.gyro.y;
    shot_ext->shot.uctl.aaUd.gyroInfo.z = data.gyro.z;
}
#endif
#ifdef SAMSUNG_ACCELEROMETER
void setMetaCtlAcceleration(struct camera2_shot_ext * shot_ext,SensorListenerEvent_t data)
{
    shot_ext->shot.uctl.aaUd.accInfo.x = data.acceleration.x;
    shot_ext->shot.uctl.aaUd.accInfo.y = data.acceleration.y;
    shot_ext->shot.uctl.aaUd.accInfo.z = data.acceleration.z;
}
#endif
#ifdef USE_FW_ZOOMRATIO
void setMetaCtlZoom(struct camera2_shot_ext *shot_ext, float data)
{
    shot_ext->shot.uctl.zoomRatio = data;
}
#endif

#ifdef SAMSUNG_OIS_VDIS
void setMetaCtlOISCoef(struct camera2_shot_ext *shot_ext, uint32_t data)
{
    shot_ext->shot.uctl.lensUd.oisCoefVal = data;
}
#endif

#ifdef USE_FW_FLASHMODE
void setMetaUctlFlashMode(struct camera2_shot_ext *shot_ext, enum camera_flash_mode mode)
{
    shot_ext->shot.uctl.flashMode = mode;
}
#endif

#ifdef USE_FW_OPMODE
void setMetaUctlOPMode(struct camera2_shot_ext *shot_ext, enum camera_op_mode mode)
{
    shot_ext->shot.uctl.opMode = mode;
}
#endif

#ifdef SAMSUNG_OIS
char *getOisEXIFFromFile(struct ExynosSensorInfoBase *info, int mode)
{
    FILE *fp = NULL;
    char ois_mode[5] = {0, };
    char ois_data[OIS_EXIF_SIZE] = {0, };

    fp = fopen(OIS_EXIF_PATH_BACK, "r");
    if (fp == NULL) {
        ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
        goto err;
    }

    /* ois tag */
    memset(info->ois_exif_info.ois_exif, 0, OIS_EXIF_SIZE);
    memcpy(info->ois_exif_info.ois_exif, OIS_EXIF_TAG, sizeof(OIS_EXIF_TAG));

    if (fgets(ois_data, sizeof(ois_data), fp) == NULL) {
        ALOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
	    goto err;
    }

    /* ois data */
    sprintf(ois_mode, "%d\n", mode);
    strncat(ois_data, " ", 1);
    strncat(ois_data, ois_mode, sizeof(ois_mode));
    strncat(info->ois_exif_info.ois_exif, ois_data, OIS_EXIF_SIZE - (sizeof(OIS_EXIF_TAG) + 1));

    ALOGD("DEBUG(%s[%d]):ois exif data : %s", __FUNCTION__, __LINE__, info->ois_exif_info.ois_exif);

err:
    if (fp != NULL)
        fclose(fp);

    return info->ois_exif_info.ois_exif;
}
#endif

#ifdef SAMSUNG_FRONT_LCD_FLASH
void setHighBrightnessModeOfLCD(int on, char *prevHBM, char *prevAutoHBM) {
    FILE *fp_hbm = NULL;
#ifndef UNSUPPORTED_AUTO_BRIGHTNESS_LCD_FLASH
    FILE *fp_auto_hbm = NULL;
    FILE *fp_auto_hbm_level = NULL;
    char autoHbmLevel[HBM_DATA_MAX_LEN] = {0, };
#endif

    fp_hbm = fopen(HBM_BRIGHTNESS, "w+");
    if (fp_hbm == NULL) {
        ALOGE("ERR(%s[%d]):failed to open sysfs entry(%s)",
            __FUNCTION__, __LINE__, HBM_BRIGHTNESS);
        goto err;
    }
#ifndef UNSUPPORTED_AUTO_BRIGHTNESS_LCD_FLASH
    fp_auto_hbm = fopen(HBM_AUTO_BRIGHTNESS, "w+");
    if (fp_auto_hbm == NULL) {
        ALOGE("ERR(%s[%d]):failed to open sysfs entry(%s)",
            __FUNCTION__, __LINE__, HBM_AUTO_BRIGHTNESS);
        goto err;
    }

    fp_auto_hbm_level = fopen(HBM_AUTO_BRIGHTNESS_LEVEL, "r");
    if (fp_auto_hbm_level == NULL) {
        ALOGE("ERR(%s[%d]):failed to open sysfs entry(%s)",
            __FUNCTION__, __LINE__, HBM_AUTO_BRIGHTNESS_LEVEL);
        goto err;
    }
#endif
    if (on == 1) {
        /* Check Backup data. -> Read Brightness. -> backup */
        if (!strncmp(prevHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN)) {
            memset(prevHBM, 0, HBM_DATA_MAX_LEN);
            fread(prevHBM, sizeof(char), HBM_DATA_MAX_LEN, fp_hbm);
            prevHBM[HBM_DATA_MAX_LEN - 1] = 0;
            ALOGD("(%s[%d]):E:on(%d), HBM(%s)", __FUNCTION__, __LINE__, on, prevHBM);
        } else {
            ALOGE("(%s):BackUp Data isn't empty.", __FUNCTION__);
        }

        /* Write Brightness */
        fwrite(HBM_BRIGHTNESS_DATA,
                sizeof(char),
                strlen(HBM_BRIGHTNESS_DATA),
                fp_hbm);

#ifndef UNSUPPORTED_AUTO_BRIGHTNESS_LCD_FLASH
        /* Read Auto Brightness Level*/
        fread(autoHbmLevel, sizeof(char), 2, fp_auto_hbm_level);
        ALOGD("(%s[%d]):E:on(%d),%d/autoHbmLevel(%c%c)",
            __FUNCTION__, __LINE__, on,
            strncmp(autoHbmLevel, HBM_AUTO_BRIGHTNESS_LEVEL_MAX, strlen(HBM_AUTO_BRIGHTNESS_LEVEL_MAX)),
            autoHbmLevel[0], autoHbmLevel[1]);

        /* Check Backup data. -> Read Auto Brightness. -> backup */
        if (!strncmp(prevAutoHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN)) {
            memset(prevAutoHBM, 0, HBM_DATA_MAX_LEN);
            fread(prevAutoHBM, sizeof(char), HBM_DATA_MAX_LEN, fp_auto_hbm);
            prevAutoHBM[HBM_DATA_MAX_LEN - 1] = 0;
            ALOGD("(%s[%d]):E:on(%d), AutoHBM(%s)", __FUNCTION__, __LINE__, on, prevAutoHBM);
        } else {
            ALOGE("(%s):BackUp Data in't empty.", __FUNCTION__);
        }

        if (!strncmp(autoHbmLevel, HBM_AUTO_BRIGHTNESS_LEVEL_MAX,
                    strlen(HBM_AUTO_BRIGHTNESS_LEVEL_MAX))) {
            /* Write Auto Brightness */
            fwrite(HBM_AUTO_BRIGHTNESS_DATA,
                    sizeof(char),
                    strlen(HBM_AUTO_BRIGHTNESS_DATA),
                    fp_auto_hbm);
        } else {
            ALOGE("(%s)LCD isn't supported HBM Mode.(%d)",
                __FUNCTION__, autoHbmLevel);
        }
#endif
    } else {
        /* Check Data & Restore */
        if (!strncmp(prevHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN)) {
            ALOGE("(%s[%d]):backUp Data is wrong.(%c%c%c)",
                __FUNCTION__, __LINE__,
                prevHBM[0], prevHBM[1], prevHBM[2]);
        } else {
            fwrite(prevHBM, sizeof(char), HBM_DATA_MAX_LEN, fp_hbm);
        }
#ifndef UNSUPPORTED_AUTO_BRIGHTNESS_LCD_FLASH
        if (!strncmp(prevAutoHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN)) {
            ALOGE("(%s[%d]):backUp Data is wrong.(%c%c)",
                __FUNCTION__, __LINE__,
                prevAutoHBM[0], prevAutoHBM[1]);
        } else {
            fwrite(prevAutoHBM, sizeof(char), HBM_DATA_MAX_LEN, fp_auto_hbm);
        }
#endif
        /* Set Default BackUp Data for defence*/
        strncpy(prevHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN);
        strncpy(prevAutoHBM, HBM_BACKUP_DEFUALT_STRING, HBM_DATA_MAX_LEN);
    }

err:
#ifndef UNSUPPORTED_AUTO_BRIGHTNESS_LCD_FLASH
    if (fp_auto_hbm_level) {
        fclose(fp_auto_hbm_level);
    }

    if (fp_auto_hbm) {
        fclose(fp_auto_hbm);
    }
#endif
    if (fp_hbm) {
        fclose(fp_hbm);
    }
}
#endif

char *getSensorIdEXIFFromFile(struct ExynosSensorInfoBase *info, int cameraId)
{
    FILE *fp = NULL;
    char sensor_id_data[SENSOR_ID_EXIF_SIZE] = {0, };

    if(cameraId == CAMERA_ID_BACK)
        fp = fopen(SENSOR_ID_EXIF_PATH_BACK, "r");
    else
        fp = fopen(SENSOR_ID_EXIF_PATH_FRONT, "r");

    if (fp == NULL) {
        ALOGE("ERR(%s[%d]):failed to open sysfs entry", __FUNCTION__, __LINE__);
        goto err;
    }

    /* sensorID tag */
    memset(info->sensor_id_exif_info.sensor_id_exif, 0, SENSOR_ID_EXIF_SIZE);
    memcpy(info->sensor_id_exif_info.sensor_id_exif, SENSOR_ID_EXIF_TAG, sizeof(SENSOR_ID_EXIF_TAG));

    if (fread(sensor_id_data, sizeof(char), SENSOR_ID_EXIF_SIZE - (sizeof(SENSOR_ID_EXIF_TAG)), fp) == 0) {
        ALOGE("ERR(%s[%d]):failed to read sysfs entry", __FUNCTION__, __LINE__);
        goto err;
    }

    /* sensorID data */
    strncat(info->sensor_id_exif_info.sensor_id_exif, sensor_id_data, SENSOR_ID_EXIF_SIZE - (sizeof(SENSOR_ID_EXIF_TAG)));

    ALOGD("DEBUG(%s[%d]):sensorId exif data : %s", __FUNCTION__, __LINE__, info->sensor_id_exif_info.sensor_id_exif);

err:
    if (fp != NULL)
        fclose(fp);

    return info->sensor_id_exif_info.sensor_id_exif;
}
}
