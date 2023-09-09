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
#define LOG_TAG "ExynosCameraPPUniPluginBD"

#include "ExynosCameraPPUniPluginBD.h"

ExynosCameraPPUniPluginBD::~ExynosCameraPPUniPluginBD()
{
}

status_t ExynosCameraPPUniPluginBD::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    m_timer.start();

    UniPluginBufferData_t bufferData;
    memset(&bufferData, 0, sizeof(UniPluginBufferData_t));
    bufferData.InWidth = srcImage[0].rect.w;
    bufferData.InHeight = srcImage[0].rect.h;
    bufferData.InFormat = UNI_PLUGIN_FORMAT_NV21;
    bufferData.InBuffY  = srcImage[0].buf.addr[0];
    bufferData.InBuffU  = srcImage[0].buf.addr[0] + (srcImage[0].rect.w * srcImage[0].rect.h);

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
    if (ret < 0) {
        CLOGE("[BD]Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
    }

    ret = m_UniPluginProcess();
    if (ret < 0) {
        CLOGE("[BD]Plugin process failed!!");
    }

#ifdef SAMSUNG_BD
    ret = m_UniPluginGet(UNI_PLUGIN_INDEX_DEBUG_INFO, &m_BDbuffer[m_BDbufferIndex]);
    if (ret < 0) {
        CLOGE("[BD]Plugin get failed!!");
    }

    m_parameters->setBlurInfo(m_BDbuffer[m_BDbufferIndex].data, m_BDbuffer[m_BDbufferIndex].size);

    if (++m_BDbufferIndex == MAX_BD_BUFF_NUM)
        m_BDbufferIndex = 0;
#endif

    m_timer.stop();
    durationTime = m_timer.durationMsecs();
    CLOGD("[BD]Plugin duration time(%5d msec)", (int)durationTime);

    return ret;
}

void ExynosCameraPPUniPluginBD::m_init(void)
{
    CLOGD(" ");

    strncpy(m_UniPluginName, BLUR_DETECTION_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginBD::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_UniPluginHandle != NULL) {
        ret = uni_plugin_init(m_UniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) init failed!!, ret(%d)", m_UniPluginName, ret);
            return INVALID_OPERATION;
        }
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPluginBD::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("[BD]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[BD] Blur Detect init failed!!");
    }

#ifdef SAMSUNG_BD
    for (int i = 0; i < MAX_BD_BUFF_NUM; i++) {
        m_BDbuffer[i].data = new unsigned char[BD_EXIF_SIZE];
    }
#endif

    m_BDbufferIndex = 0;

    return ret;
}

status_t ExynosCameraPPUniPluginBD::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[BD]");

    if (m_UniPluginHandle == NULL) {
        CLOGE("[BD] Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[BD] Blur Detect plugin deinit failed!!");
    }

#ifdef SAMSUNG_BD
    for (int i = 0; i < MAX_BD_BUFF_NUM; i++) {
        if (m_BDbuffer[i].data != NULL) {
            delete []m_BDbuffer[i].data;
            m_BDbuffer[i].data = NULL;
        }
    }
#endif

    return ret;
}

