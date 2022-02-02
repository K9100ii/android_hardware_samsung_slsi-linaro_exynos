/*
 * Copyright 2008, The Android Open Source Project
 * Copyright 2013, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

 /*!
 * \file      SecCameraParameters.cpp
 * \brief     source file for Android Camera Ext HAL
 * \author    teahyung kim (tkon.kim@samsung.com)
 * \date      2013/04/30
 *
 */

#define LOG_TAG "SecCameraParams"
#include <utils/Log.h>

#include <string.h>
#include <stdlib.h>
#include <camera/CameraParameters.h>

#include "SecCameraParameters.h"

namespace android {

/* Parameter keys to communicate between camera application and driver. */
const char SecCameraParameters::KEY_FIRMWARE_MODE[] = "firmware-mode";
const char SecCameraParameters::KEY_DTP_MODE[] = "chk_dataline";

const char SecCameraParameters::KEY_VT_MODE[] = "vtmode";
const char SecCameraParameters::KEY_MOVIE_MODE[] = "cam_mode";

const char SecCameraParameters::KEY_ISO[] = "iso";
const char SecCameraParameters::KEY_METERING[] = "metering";
const char SecCameraParameters::KEY_AUTO_CONTRAST[] = "wdr";
const char SecCameraParameters::KEY_ANTI_SHAKE[] = "anti-shake";
const char SecCameraParameters::KEY_FACE_BEAUTY[] = "face_beauty";
const char SecCameraParameters::KEY_HDR_MODE[] = "hdr-mode";
const char SecCameraParameters::KEY_BLUR[] = "blur";
const char SecCameraParameters::KEY_ANTIBANDING[] = "antibanding";


/* for Image Adjust  */
const char SecCameraParameters::KEY_COLOR[] = "color";
const char SecCameraParameters::KEY_CONTRAST[] = "contrast";
const char SecCameraParameters::KEY_SHARPNESS[] = "sharpness";
const char SecCameraParameters::KEY_SATURATION[] = "saturation";


/* Values for scene mode settings. */
const char SecCameraParameters::SCENE_MODE_DUSK_DAWN[] = "dusk-dawn";
const char SecCameraParameters::SCENE_MODE_FALL_COLOR[] = "fall-color";
const char SecCameraParameters::SCENE_MODE_BACK_LIGHT[] = "back-light";
const char SecCameraParameters::SCENE_MODE_TEXT[] = "text";

/* Values for focus mode settings. */
const char SecCameraParameters::FOCUS_MODE_FACEDETECT[] = "facedetect";

/* Values for iso settings. */
const char SecCameraParameters::ISO_AUTO[] = "auto";
const char SecCameraParameters::ISO_50[] = "50";
const char SecCameraParameters::ISO_100[] = "100";
const char SecCameraParameters::ISO_200[] = "200";
const char SecCameraParameters::ISO_400[] = "400";
const char SecCameraParameters::ISO_800[] = "800";
const char SecCameraParameters::ISO_1600[] = "1600";
const char SecCameraParameters::ISO_SPORTS[] = "sports";
const char SecCameraParameters::ISO_NIGHT[] = "night";

/* Values for metering settings. */
const char SecCameraParameters::METERING_CENTER[] = "center";
const char SecCameraParameters::METERING_MATRIX[] = "matrix";
const char SecCameraParameters::METERING_SPOT[] = "spot";

/* for NSM Mode */
const char SecCameraParameters::KEY_NSM_MODE_SYSTEM[] = "nsm-system";
const char SecCameraParameters::KEY_NSM_MODE_STATE[] = "nsm-state";

const char SecCameraParameters::KEY_NSM_RGB[] = "nsm-rgb";
const char SecCameraParameters::KEY_NSM_GLOBAL_CONTSHARP[] = "nsm-global-cont,sharp";
const char SecCameraParameters::KEY_NSM_HUE_ALLREDORANGE[] = "nsm-hue-all,red,orange";
const char SecCameraParameters::KEY_NSM_HUE_YELLOWGREENCYAN[] = "nsm-hue-yellow,green,cyan";
const char SecCameraParameters::KEY_NSM_HUE_BLUEVIOLETPURPLE[] = "nsm-hue-blue,violet,purple";
const char SecCameraParameters::KEY_NSM_SAT_ALLREDORANGE[] = "nsm-sat-all,red,orange";
const char SecCameraParameters::KEY_NSM_SAT_YELLOWGREENCYAN[] = "nsm-sat-yellow,green,cyan";
const char SecCameraParameters::KEY_NSM_SAT_BLUEVIOLETPURPLE[] = "nsm-sat-blue,violet,purple";

const char SecCameraParameters::KEY_NSM_R[] = "nsm-r";
const char SecCameraParameters::KEY_NSM_G[] = "nsm-g";
const char SecCameraParameters::KEY_NSM_B[] = "nsm-b";
const char SecCameraParameters::KEY_NSM_GLOBAL_CONTRAST[] = "nsm-global-contrast";
const char SecCameraParameters::KEY_NSM_GLOBAL_SHARPNESS[] = "nsm-global-sharpness";

const char SecCameraParameters::KEY_NSM_HUE_ALL[] = "nsm-hue-all";
const char SecCameraParameters::KEY_NSM_HUE_RED[] = "nsm-hue-red";
const char SecCameraParameters::KEY_NSM_HUE_ORANGE[] = "nsm-hue-orange";
const char SecCameraParameters::KEY_NSM_HUE_YELLOW[] = "nsm-hue-yellow";
const char SecCameraParameters::KEY_NSM_HUE_GREEN[] = "nsm-hue-green";
const char SecCameraParameters::KEY_NSM_HUE_CYAN[] = "nsm-hue-cyan";
const char SecCameraParameters::KEY_NSM_HUE_BLUE[] = "nsm-hue-blue";
const char SecCameraParameters::KEY_NSM_HUE_VIOLET[] = "nsm-hue-violet";
const char SecCameraParameters::KEY_NSM_HUE_PURPLE[] = "nsm-hue-purple";

const char SecCameraParameters::KEY_NSM_SAT_ALL[] = "nsm-sat-all";
const char SecCameraParameters::KEY_NSM_SAT_RED[] = "nsm-sat-red";
const char SecCameraParameters::KEY_NSM_SAT_ORANGE[] = "nsm-sat-orange";
const char SecCameraParameters::KEY_NSM_SAT_YELLOW[] = "nsm-sat-yellow";
const char SecCameraParameters::KEY_NSM_SAT_GREEN[] = "nsm-sat-green";
const char SecCameraParameters::KEY_NSM_SAT_CYAN[] = "nsm-sat-cyan";
const char SecCameraParameters::KEY_NSM_SAT_BLUE[] = "nsm-sat-blue";
const char SecCameraParameters::KEY_NSM_SAT_VIOLET[] = "nsm-sat-violet";
const char SecCameraParameters::KEY_NSM_SAT_PURPLE[] = "nsm-sat-purple";

const char SecCameraParameters::KEY_NSM_FD_INFO[] = "nsm-fd-info";
const char SecCameraParameters::KEY_NSM_SCENE_DETECT[] = "nsm-scene-detect";
/* end NSM Mode */

/* Values for firmware mode settings. */
const char SecCameraParameters::FIRMWARE_MODE_NONE[] = "none";
const char SecCameraParameters::FIRMWARE_MODE_VERSION[] = "version";
const char SecCameraParameters::FIRMWARE_MODE_UPDATE[] = "update";
const char SecCameraParameters::FIRMWARE_MODE_DUMP[] = "dump";

#ifndef FCJUNG
const char SecCameraParameters::FACTORY_MEM_ISP_RAM[] = "isp_ram";
const char SecCameraParameters::FACTORY_MEM_ISP_NOR[] = "isp_nor";
const char SecCameraParameters::FACTORY_MEM_BARREL_EEP[] = "barrel_eep";

const char SecCameraParameters::FACTORY_EEP_WRITE_CHECK[] = "barrel_function";
const char SecCameraParameters::FACTORY_EEP_WRITE_REPAIR[] = "repair";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP0[] = "step0";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP1[] = "step1";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP2[] = "step2";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP3[] = "step3";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP4[] = "step4";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP5[] = "step5";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP6[] = "step6";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP7[] = "step7";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP8[] = "step8";
const char SecCameraParameters::FACTORY_EEP_WRITE_STEP9[] = "step9";
const char SecCameraParameters::FACTORY_EEP_WRITE_AP[] = "ap";
const char SecCameraParameters::FACTORY_EEP_WRITE_ISP[] = "isp";
const char SecCameraParameters::FACTORY_EEP_WRITE_SN[] = "sn";
const char SecCameraParameters::FACTORY_EEP_WRITE_OIS_SHIFT[] = "ois_shift";
#endif

SecCameraParameters::SecCameraParameters()
{
}

SecCameraParameters::~SecCameraParameters()
{
}

int SecCameraParameters::lookupAttr(const cam_strmap_t arr[], int len, const char *name)
{
    if (name) {
        for (int i = 0; i < len; i++) {
            if (!strcmp(arr[i].desc, name))
                return arr[i].val;
        }
    }
    return NOT_FOUND;
}

String8 SecCameraParameters::createSizesStr(const image_rect_type *sizes, int len)
{
    String8 str;
    char buffer[32];

    if (len > 0) {
        snprintf(buffer, sizeof(buffer), "%dx%d", sizes[0].width, sizes[0].height);
        str.append(buffer);
    }

    for (int i = 1; i < len; i++) {
        snprintf(buffer, sizeof(buffer), ",%dx%d", sizes[i].width, sizes[i].height);
        str.append(buffer);
    }
    return str;
}

String8 SecCameraParameters::createValuesStr(const cam_strmap_t *values, int len)
{
    String8 str;

    if (len > 0)
        str.append(values[0].desc);

    for (int i=1; i<len; i++) {
        str.append(",");
        str.append(values[i].desc);
    }
    return str;
}

};
