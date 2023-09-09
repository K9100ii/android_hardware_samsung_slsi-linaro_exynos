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
#define LOG_TAG "ExynosCameraPPUniPluginFP"

#include "ExynosCameraPPUniPluginFP.h"

ExynosCameraPPUniPluginFP::~ExynosCameraPPUniPluginFP()
{
}

status_t ExynosCameraPPUniPluginFP::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    UniPluginBufferData_t bufferInfo;
    UniPluginBufferData_t depthMapInfo;
    UniPluginExtraBufferInfo_t extraInfo;
    ExynosCameraDurationTimer m_timer;
    long long durationTime = 0;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[FocusPeaking] FocusPeaking Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_FPstatus) {
    case FOCUS_PEAKING_IDLE:
        break;
    case FOCUS_PEAKING_RUN:
        if (dstImage[0].buf.index < 0) {
            return NO_ERROR;
        }

#ifdef SUPPORT_DEPTH_MAP
        /* set depthMap Info : If depthMap info is invaild, set InBuffY to NULL and set width/height info. */
        memset(&depthMapInfo, 0, sizeof(UniPluginBufferData_t));
        depthMapInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_DEPTHMAP;
        depthMapInfo.inWidth = srcImage[0].rect.w;
        depthMapInfo.inHeight= srcImage[0].rect.h;

        if (srcImage[0].buf.index < 0 || srcImage[0].buf.addr[0] == NULL) {
            CLOGD("[FocusPeaking] depthMap info is not invalid (%d)", srcImage[0].buf.index);
            depthMapInfo.inBuffY = NULL;
        } else {
            depthMapInfo.inBuffY = srcImage[0].buf.addr[0];
            depthMapInfo.inFormat = UNI_PLUGIN_FORMAT_RAW10;
        }

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &depthMapInfo);
        if(ret < 0)
            CLOGE("[FocusPeaking] Plugin set DepthMap (UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
#endif

        /* set buffer Info */
        memset(&bufferInfo, 0, sizeof(UniPluginBufferData_t));
        bufferInfo.bufferType = UNI_PLUGIN_BUFFER_TYPE_PREVIEW;
        bufferInfo.inBuffY = dstImage[0].buf.addr[0];
        bufferInfo.inBuffU = dstImage[0].buf.addr[1];
        bufferInfo.inWidth = dstImage[0].rect.w;
        bufferInfo.inHeight= dstImage[0].rect.h;
        bufferInfo.inFormat = UNI_PLUGIN_FORMAT_NV21;
        bufferInfo.outBuffY = dstImage[0].buf.addr[0];
        bufferInfo.outBuffU = dstImage[0].buf.addr[1];
        bufferInfo.outWidth = dstImage[0].rect.w;
        bufferInfo.outHeight= dstImage[0].rect.h;

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferInfo);
        if(ret < 0) {
            CLOGE("[FocusPeaking] Plugin set(UNI_PLUGIN_INDEX_BUFFER_INFO) failed!!");
        }

        /* set zoom Info */
        memset(&extraInfo, 0, sizeof(UniPluginExtraBufferInfo_t));
        extraInfo.zoomRatio = m_configurations->getZoomRatio();

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraInfo);
        if(ret < 0) {
            CLOGE("[FocusPeaking] Plugin set zoomInfo (UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO) failed!!");
        }

        /* Start processing */
        m_timer.start();
        ret = m_UniPluginProcess();
        if(ret < 0) {
           CLOGE("[FocusPeaking] FocusPeaking plugin process failed!!");
        }
        m_timer.stop();
        durationTime = m_timer.durationMsecs();

        if (durationTime > 10) {
            CLOGD("[FocusPeaking] duration time(%5d msec) (%d)", (int)durationTime, srcImage[0].buf.index);
        }
        break;
    case FOCUS_PEAKING_DEINIT:
        CLOGD("[FocusPeaking] FOCUS_PEAKING_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

void ExynosCameraPPUniPluginFP::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, FOCUS_PEAKING_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(DEPTH_MAP_FORMAT);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_FPstatus = FOCUS_PEAKING_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginFP::m_UniPluginInit(void)
{
    status_t ret = NO_ERROR;

    if(m_loadThread != NULL) {
        m_loadThread->join();
    }

    if (m_uniPluginHandle != NULL) {
        ret = uni_plugin_init(m_uniPluginHandle);
        if (ret < 0) {
            CLOGE("Uni plugin(%s) init failed!!, ret(%d)", m_uniPluginName, ret);
            return INVALID_OPERATION;
        }

#ifdef SUPPORT_DEPTH_MAP
        UTbool supportDepthMap = UTtrue;
        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_SUPPORT_DEPTH_MAP, &supportDepthMap);
        if(ret < 0)
            CLOGE("[FocusPeaking] Plugin set (UNI_PLUGIN_INDEX_SUPPORT_DEPTH_MAP) failed!!");
#endif
    } else {
        CLOGE("Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    return NO_ERROR;
}

status_t ExynosCameraPPUniPluginFP::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("[FocusPeaking]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[FocusPeaking] FocusPeaking Plugin init failed!!");
    }

    m_FPstatus = FOCUS_PEAKING_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginFP::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[FocusPeaking]");

    if (m_uniPluginHandle == NULL) {
        CLOGE("[FocusPeaking] FocusPeaking Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[FocusPeaking] FocusPeaking Tracking plugin deinit failed!!");
    }

    m_FPstatus = FOCUS_PEAKING_DEINIT;

    return ret;
}

