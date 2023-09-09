/*
 * Copyright@ Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

/*#define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraPPUniPlugin"

#include "ExynosCameraPPUniPlugin.h"

ExynosCameraPPUniPlugin::~ExynosCameraPPUniPlugin()
{
}

status_t ExynosCameraPPUniPlugin::m_create(void)
{
    m_loadThread = new ExynosCameraPPUniPluginThread(this, &ExynosCameraPPUniPlugin::m_loadThreadFunc, "PPUniPluginThread");

    if(m_loadThread != NULL) {
        m_loadThread->run();
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_extControl(int controlType, void *data)
{
    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_destroy(void)
{
    status_t ret = NO_ERROR;
    CLOGD("IN");

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        CLOGD("uni_plugin_unload(%s)", m_uniPluginName);
        ret = uni_plugin_unload(&m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) unload failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
        m_uniPluginHandle = NULL;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    m_sensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
    m_auxSensorId = UNI_PLUGIN_SENSOR_NAME_NONE;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }

        /* Start case */
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(m_configurations->getScenario(), m_cameraId);
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)m_getUniSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("Set camera info: %d:%d",
                cameraInfo.cameraType, cameraInfo.sensorType);
        ret = uni_plugin_set(m_uniPluginHandle,
                      m_uniPluginName, UNI_PLUGIN_INDEX_CAMERA_INFO, &cameraInfo);
        if (ret < 0) {
            CLOGE("Plugin set UNI_PLUGIN_INDEX_CAMERA_INFO failed!!");
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginDeinit(void)
{
    status_t ret = NO_ERROR;

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_deinit(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) deinit failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginProcess(void)
{
    status_t ret = NO_ERROR;

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_process(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) process failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginSet(int index, void *data)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_set(m_uniPluginHandle, m_uniPluginName, index, data);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) set(index%d) failed!!, ret(%d)", m_uniPluginName, index, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginGet(int index, void *data)
{
    status_t ret = NO_ERROR;

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_get(m_uniPluginHandle, m_uniPluginName, index, data);
        if (ret < 0) {
            CLOGE("(Uni plugin(%s) get(index%d) failed!!, ret(%d)", m_uniPluginName, index, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("(Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void ExynosCameraPPUniPlugin::m_init(void)
{
    CLOGD(" ");
    m_uniPluginHandle = NULL;

    m_sensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
    m_auxSensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
}

bool ExynosCameraPPUniPlugin::m_loadThreadFunc(void)
{
    CLOGD("IN:(%s)", m_uniPluginName);

    if (m_uniPluginHandle == NULL) {
        // create your own library.
        m_uniPluginHandle = uni_plugin_load(m_uniPluginName);
        if (m_uniPluginHandle == NULL) {
            CLOGE("Uni Plugin(%s) load failed!!", m_uniPluginName);
            return INVALID_OPERATION;
        }
    }
    CLOGD("OUT");

    return false;
}

UNI_PLUGIN_SENSOR_TYPE ExynosCameraPPUniPlugin::m_checkUniSensorId(int cameraId)
{
    FILE *fp = NULL;
    UNI_PLUGIN_SENSOR_TYPE uniSensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
    int sensorId = getSensorId(cameraId);

    switch (sensorId) {
    case SENSOR_NAME_S5K3H1:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_S5K3H1;
        break;
    case SENSOR_NAME_S5K2L2:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_S5K2L2;
        break;
    case SENSOR_NAME_S5K3M3:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_S5K3M3;
        break;
    case SENSOR_NAME_SAK2L3:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_SAK2L3;
        break;
    case SENSOR_NAME_IMX320:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_IMX320;
        break;
    case SENSOR_NAME_IMX333:
        uniSensorId = UNI_PLUGIN_SENSOR_NAME_IMX333;
        break;
    default:
        ALOGE("ERR(%s): check the sensorId(%d)", __FUNCTION__, sensorId);
        break;
    }

    return uniSensorId;
}

UNI_PLUGIN_SENSOR_TYPE ExynosCameraPPUniPlugin::m_getUniSensorId(int cameraId)
{
    m_sensorId = m_checkUniSensorId(cameraId);

    return m_sensorId;
}

UNI_PLUGIN_SENSOR_TYPE ExynosCameraPPUniPlugin::m_getUniAuxSensorId(int cameraId)
{
#ifdef USE_DUAL_CAMERA
    m_auxSensorId = m_checkUniSensorId(cameraId);
#else
    m_auxSensorId = UNI_PLUGIN_SENSOR_NAME_NONE;
#endif

    return m_auxSensorId;
}

UNI_PLUGIN_HDR_MODE ExynosCameraPPUniPlugin::m_getUniHDRMode()
{
    UNI_PLUGIN_HDR_MODE hdrData = UNI_PLUGIN_HDR_MODE_OFF;

#ifdef SAMSUNG_RTHDR
    switch (m_parameters->getRTHdr()) {
        case CAMERA_WDR_OFF:
            hdrData = UNI_PLUGIN_HDR_MODE_OFF;
            break;
        case CAMERA_WDR_AUTO:
            hdrData = UNI_PLUGIN_HDR_MODE_AUTO;
            break;
        case CAMERA_WDR_ON:
            hdrData = UNI_PLUGIN_HDR_MODE_ON;
            break;
    }
#endif

    return hdrData;
}

UNI_PLUGIN_FLASH_MODE ExynosCameraPPUniPlugin::m_getUniFlashMode()
{
    UNI_PLUGIN_FLASH_MODE flashData = UNI_PLUGIN_FLASH_MODE_OFF;

    switch (m_configurations->getModeValue(CONFIGURATION_FLASH_MODE)) {
        case android::FLASH_MODE_OFF:
            flashData = UNI_PLUGIN_FLASH_MODE_OFF;
            break;
        case android::FLASH_MODE_AUTO:
            flashData = UNI_PLUGIN_FLASH_MODE_AUTO;
            break;
        case android::FLASH_MODE_ON:
            flashData = UNI_PLUGIN_FLASH_MODE_ON;
            break;
    }
    return flashData;
}

UNI_PLUGIN_DEVICE_ORIENTATION ExynosCameraPPUniPlugin::m_getUniOrientationMode(bool captureFlag)
{
    /* translate Orientation enum(match to exif data) for uniPlugin */
    UNI_PLUGIN_DEVICE_ORIENTATION orientation = UNI_PLUGIN_ORIENTATION_0;

    if (getCameraId() == CAMERA_ID_FRONT) {
        /* please check when flip horizontal is on */
        switch (m_configurations->getModeValue(CONFIGURATION_DEVICE_ORIENTATION)) {
            case 0:
                orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case 90:
                orientation = UNI_PLUGIN_ORIENTATION_180;
                break;
            case 180:
                orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            case 270:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
            default:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
        }

        /* convert orientation value when flip horizontal */
        if (captureFlag && m_configurations->getModeValue(CONFIGURATION_FLIP_HORIZONTAL) == true) {
            switch (orientation) {
                case UNI_PLUGIN_ORIENTATION_90:
                    orientation = UNI_PLUGIN_ORIENTATION_270;
                    break;
                case UNI_PLUGIN_ORIENTATION_270:
                    orientation = UNI_PLUGIN_ORIENTATION_90;
                    break;
                default:
                    break;
            }
        }
    } else {
        switch (m_configurations->getModeValue(CONFIGURATION_DEVICE_ORIENTATION)) {
            case 0:
                orientation = UNI_PLUGIN_ORIENTATION_270;
                break;
            case 90:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
            case 180:
                orientation = UNI_PLUGIN_ORIENTATION_90;
                break;
            case 270:
                orientation = UNI_PLUGIN_ORIENTATION_180;
                break;
            default:
                orientation = UNI_PLUGIN_ORIENTATION_0;
                break;
        }
    }
    return orientation;
}

status_t ExynosCameraPPUniPlugin::m_UniPluginThreadJoin(void)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    } else {
        return INVALID_OPERATION;
    }

    return ret;
}


