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
#define LOG_TAG "ExynosCameraPPUniPluginBeauty_Preview"

#include "ExynosCameraPPUniPluginBeauty_Preview.h"

ExynosCameraPPUniPluginBeauty_Preview::~ExynosCameraPPUniPluginBeauty_Preview()
{
}

status_t ExynosCameraPPUniPluginBeauty_Preview::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[BEAUTY] BEAUTY Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_beautyStatus) {
    case BEAUTY_IDLE:
        break;
    case BEAUTY_RUN:
        m_timer.start();
        ret = process(srcImage, dstImage, params);
        m_timer.stop();
        durationTime = m_timer.durationMsecs();

        CLOGV("[BEAUTY] duration time(%5d msec)", (int)durationTime);
        break;
    case BEAUTY_DEINIT:
        CLOGD("[BEAUTY] BEAUTY_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

status_t ExynosCameraPPUniPluginBeauty_Preview::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        UniPluginCameraInfo_t cameraInfo;
        cameraInfo.cameraType = getUniCameraType(m_configurations->getScenario(), m_cameraId);
        cameraInfo.sensorType = (UNI_PLUGIN_SENSOR_TYPE)getSensorId(m_cameraId);
        cameraInfo.APType = UNI_PLUGIN_AP_TYPE_SLSI;

        CLOGD("Set camera info: %d:%d-%s",
                cameraInfo.cameraType, cameraInfo.sensorType, m_uniPluginName);

        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Beauty plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Beauty plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

void ExynosCameraPPUniPluginBeauty_Preview::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, VIDEO_BEAUTY_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_beautyStatus = BEAUTY_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginBeauty_Preview::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("[BEAUTY]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[BEAUTY] BEAUTY Plugin init failed!!");
    }

    m_beautyStatus = BEAUTY_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginBeauty_Preview::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[BEAUTY]");

    if (m_uniPluginHandle == NULL) {
        CLOGE("[BEAUTY] BEAUTY Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[BEAUTY] BEAUTY plugin deinit failed!!");
    }

    m_beautyStatus = BEAUTY_DEINIT;

    return ret;
}

status_t ExynosCameraPPUniPluginBeauty_Preview::process(ExynosCameraImage *srcImage,
                                                            ExynosCameraImage *dstImage,
                                                            ExynosCameraParameters *params)
{
    int w = 0 ,  h = 0;
    int beautyFaceRetouchLevel = 0;
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    int portId;
    int bokehRecordingHint = 0;
    camera2_stream *p_src_shot_stream;
    camera2_stream *p_dst_shot_stream;

    portId = params->getPreviewPortId();

    params->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&w, (uint32_t *)&h, portId);

    /* Set buffer Info */
    memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
    bufferInfo.inBuffY = (char *)srcImage[0].buf.addr[0];
    bufferInfo.inBuffU = (char *)srcImage[0].buf.addr[1];
    bufferInfo.inWidth = w;
    bufferInfo.inHeight= h;
    bufferInfo.inFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferInfo.outBuffY = (char *)dstImage[0].buf.addr[0];
    bufferInfo.outBuffU = (char *)dstImage[0].buf.addr[1];
    bufferInfo.outWidth = w;
    bufferInfo.outHeight= h;

    p_src_shot_stream = (struct camera2_stream *) srcImage->buf.addr[srcImage->buf.getMetaPlaneIndex()];
    p_dst_shot_stream = (struct camera2_stream *) dstImage->buf.addr[dstImage->buf.getMetaPlaneIndex()];

    memcpy(p_dst_shot_stream, p_src_shot_stream, sizeof(struct camera2_stream));

#ifdef SAMSUNG_VIDEO_BEAUTY
    bokehRecordingHint = m_configurations->getBokehRecordingHint();
#endif
    ALOGV("[BEAUTY_PREVIEW] bokehRecordingHint %d", bokehRecordingHint);

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BEAUTY_MODE, &bokehRecordingHint);
    if (ret < 0) {
        CLOGE("[BEAUTY_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_BEAUTY_MODE) failed!!");
    }

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
    if (ret < 0) {
        CLOGE("[BEAUTY_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
    }

#ifdef SAMSUNG_VIDEO_BEAUTY
    beautyFaceRetouchLevel = m_configurations->getModeValue(CONFIGURATION_BEAUTY_RETOUCH_LEVEL);
#endif

    CLOGV("[BEAUTY_PREVIEW] w:%d, h:%d Retouch(%d)", w, h, beautyFaceRetouchLevel);

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BEAUTY_LEVEL_SOFTEN, &beautyFaceRetouchLevel);
    if (ret < 0) {
        CLOGE("[BEAUTY_PREVIEW] Plugin set(UNI_PLUGIN_INDEX_BEAUTY_LEVEL_SOFTEN) failed!!");
    }

    ret = m_UniPluginProcess();
    if (ret < 0) {
        ALOGE("[BEAUTY_PREVIEW] BEAUTY plugin process failed!!");
        ret = INVALID_OPERATION;
    }

    return ret;
}
