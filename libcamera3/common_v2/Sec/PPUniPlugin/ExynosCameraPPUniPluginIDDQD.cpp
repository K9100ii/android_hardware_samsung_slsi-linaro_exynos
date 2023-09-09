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
#define LOG_TAG "ExynosCameraPPUniPluginIDDQD"

#include "ExynosCameraPPUniPluginIDDQD.h"

ExynosCameraPPUniPluginIDDQD::~ExynosCameraPPUniPluginIDDQD()
{
}

status_t ExynosCameraPPUniPluginIDDQD::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginExtraBufferInfo_t extraData;
    UniPluginPrivInfo_t privInfo;
    UniPluginSensorData_t sensorData;
    UniPluginFocusData_t focusData;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;
    camera2_shot_ext *shot_ext = NULL;
    int lens_dirty_detected;
    UniPluginSceneDetectInfo_t sceneDetectInfo;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_IDDQDstatus != IDDQD_RUN) {
        CLOGD("[IDDQD] status is not run");
        return ret;
    }

    if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
        return NO_ERROR;
    }

    shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];

    /* set buffer Info */
    memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
    bufferInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
    bufferInfo.inBuffY = srcImage[0].buf.addr[0];
    if (srcImage[0].rect.colorFormat == V4L2_PIX_FMT_NV21) { /* preview callback stream */
        bufferInfo.inBuffU = srcImage[0].buf.addr[0] + (srcImage[0].rect.w * srcImage[0].rect.h);
    } else {/* preview stream */
        bufferInfo.inBuffU = srcImage[0].buf.addr[1];
    }
    bufferInfo.inWidth = srcImage[0].rect.w;
    bufferInfo.inHeight= srcImage[0].rect.h;
    bufferInfo.inFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.size = 0; // Not used
    bufferInfo.cameraType = getUniCameraType(m_configurations->getScenario(), m_cameraId);
    CLOGV("[IDDQD] prieviewSize %d x %d, size %d, CameraType %d",
        bufferInfo.inWidth, bufferInfo.inHeight, bufferInfo.size, bufferInfo.cameraType);
    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Set extraBuffer Info */
    memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraData.zoomRatio = m_configurations->getZoomRatio();
    extraData.brightnessValue.snum = shot_ext->shot.udm.ae.vendorSpecific[5];
    extraData.brightnessValue.den = 256;
    extraData.iso[0] = shot_ext->shot.udm.ae.vendorSpecific[391]; // short ISO
    extraData.iso[1] = shot_ext->shot.udm.ae.vendorSpecific[393]; // Long ISO
    CLOGV("[IDDQD] zoomRatio %f, BV snum %d, BV den %d, CameraType %d",
        extraData.zoomRatio, extraData.brightnessValue.snum, extraData.brightnessValue.den, extraData.cameraType);
    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Set priv Info */
    memset(&privInfo, 0, sizeof(UniPluginPrivInfo_t));
    privInfo.priv[0] = shot_ext->shot.udm.ae.vendorSpecific;
    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_PRIV_INFO, &privInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_PRIV_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Set focus Data */
    memset(&focusData, 0, sizeof(UniPluginFocusData_t));
    short *af_meta_short = (short *)shot_ext->shot.udm.af.vendorSpecific;
    focusData.focusState = (int)af_meta_short[8];
    focusData.focusPositionMin = (int)af_meta_short[6];
    focusData.focusPositionMax = (int)af_meta_short[5];
    focusData.focusPositionCur = (int)af_meta_short[9];
    CLOGV("[IDDQD] focusState: %d, focusPosition(Min %d, Max %d, Cur %d)",
        focusData.focusState, focusData.focusPositionMin, focusData.focusPositionMax, focusData.focusPositionCur);
    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_FOCUS_INFO) failed!!", __FUNCTION__, __LINE__);
    }

#ifdef SAMSUNG_GYRO
    /* Set sensor Data */
    SensorListenerEvent_t *gyroData = NULL;
    gyroData = params->getGyroData();
    memset(&sensorData, 0, sizeof(UniPluginSensorData_t));
    sensorData.gyroData.x = gyroData->gyro.x;
    sensorData.gyroData.y = gyroData->gyro.y;
    sensorData.gyroData.z = gyroData->gyro.z;
    sensorData.gyroData.timestamp = gyroData->gyro.timestamp;
    CLOGV("[IDDQD] gyro %f, %f, %f ", sensorData.gyroData.x, sensorData.gyroData.y, sensorData.gyroData.z);
    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_SENSOR_DATA, &sensorData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_SENSOR_DATA) failed!!", __FUNCTION__, __LINE__);
    }
#endif

    /* set Scene detect info data */
    sceneDetectInfo.timestamp           = (int64_t)shot_ext->shot.uctl.sceneDetectInfoUd.timeStamp;
    sceneDetectInfo.sceneIndex          = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.scene_index;
    sceneDetectInfo.confidenceScore     = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.confidence_score;
    sceneDetectInfo.objectRoi.left      = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.object_roi[0];
    sceneDetectInfo.objectRoi.top       = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.object_roi[1];
    sceneDetectInfo.objectRoi.right     = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.object_roi[2];
    sceneDetectInfo.objectRoi.bottom    = (int32_t)shot_ext->shot.uctl.sceneDetectInfoUd.object_roi[3];

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_SCENE_DETECT_INFO, &sceneDetectInfo);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): Plugin set(UNI_PLUGIN_INDEX_SCENE_DETECT_INFO) failed!!", __FUNCTION__, __LINE__);
    }

    /* Start processing */
    m_timer.start();
    ret = m_UniPluginProcess();
    if (ret < 0) {
        ALOGE("[IDDQD](%s[%d]): IDDQD plugin process failed!!", __FUNCTION__, __LINE__);
        ret = INVALID_OPERATION;
    }

    UTstr debugData;
    unsigned char IDDQDbuf[10];
    debugData.data = IDDQDbuf;
    debugData.size = 10;
    ret = m_UniPluginGet(UNI_PLUGIN_INDEX_DEBUG_INFO, &debugData);
    if (ret < 0) {
        CLOGE("[IDDQD](%s[%d]): get debug info failed!!", __FUNCTION__, __LINE__);
    }

    if (debugData.data[0] != '0')
        CLOGV("IDDQD DETECT[IDDQD](%s[%d]): DIRT=%c", __FUNCTION__, __LINE__, debugData.data[0]);

    lens_dirty_detected = debugData.data[0] - '0';
#ifdef SAMSUNG_IDDQD
    m_configurations->setIDDQDresult(lens_dirty_detected);
#endif
    m_timer.stop();
    durationTime = m_timer.durationMsecs();

    CLOGV("[IDDQD] duration time(%5d msec)", (int)durationTime);

    return ret;
}

void ExynosCameraPPUniPluginIDDQD::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, IDDQD_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_IDDQDstatus = IDDQD_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginIDDQD::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("[IDDQD]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[IDDQD] Plugin init failed!");
    }

    m_IDDQDstatus = IDDQD_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginIDDQD::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[IDDQD]");

    if (m_uniPluginHandle == NULL) {
        CLOGE("[IDDQD] IDDQD Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[IDDQD] Highlight Video plugin deinit failed!!");
    }

    m_IDDQDstatus = IDDQD_DEINIT;

    return ret;
}

status_t ExynosCameraPPUniPluginIDDQD::m_extControl(int controlType, void *data)
{
    switch(controlType) {
    case PP_EXT_CONTROL_POWER_CONTROL:
        break;
    case PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE:
        CLOGD("[IDDQD] PP_EXT_CONTROL_SET_UNI_PLUGIN_HANDLE");
        m_uniPluginHandle = data;
        break;
    default:
        break;
    }

    return NO_ERROR;
}

