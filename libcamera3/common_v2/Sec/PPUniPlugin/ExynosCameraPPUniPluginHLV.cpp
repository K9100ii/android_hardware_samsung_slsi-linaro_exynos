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
#define LOG_TAG "ExynosCameraPPUniPluginHLV"

#include "ExynosCameraPPUniPluginHLV.h"

ExynosCameraPPUniPluginHLV::~ExynosCameraPPUniPluginHLV()
{
}

status_t ExynosCameraPPUniPluginHLV::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;

    UniPluginFocusData_t focusData;
    UniPluginBufferData_t bufferData;
    UniPluginExtraBufferInfo_t extraData;

    if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
        return ret;
    }

    camera2_shot_ext *shot_ext = (struct camera2_shot_ext *)srcImage[0].buf.addr[srcImage[0].buf.getMetaPlaneIndex()];
    struct camera2_udm *udm = &(shot_ext->shot.udm);
    struct camera2_dm *dm = &(shot_ext->shot.dm);

    Mutex::Autolock l(m_uniPluginLock);

    if (m_HLVstatus != HLV_RUN) {
        CLOGD("[HLV] status is not run");
        return ret;
    }

    if (m_uniPluginHandle == NULL) {
        CLOGE("[HLV] Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    memset(&bufferData, 0, sizeof(UniPluginBufferData_t));
    bufferData.inWidth = srcImage[0].rect.w;
    bufferData.inHeight = srcImage[0].rect.h;
    bufferData.inBuffY = srcImage[0].buf.addr[0];
    bufferData.inBuffU = srcImage[0].buf.addr[1];
    bufferData.inFormat = UNI_PLUGIN_FORMAT_NV21;
    if (udm != NULL) {
        bufferData.timestamp = (uint64_t)udm->sensor.timeStampBoot / 1000000; /* in MilliSec */
    }

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_set fail!");

    memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
    extraData.zoomRatio = m_configurations->getZoomRatio();

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_set fail!");

    int FocusAreaWeight = 0;
    ExynosRect2 CurrentFocusArea, newRect;
    ExynosRect cropRegionRect;

    memset(&CurrentFocusArea, 0, sizeof(ExynosRect2));

    CurrentFocusArea.x1 = dm->aa.afRegions[0];
    CurrentFocusArea.y1 = dm->aa.afRegions[1];
    CurrentFocusArea.x2 = dm->aa.afRegions[2];
    CurrentFocusArea.y2 = dm->aa.afRegions[3];
    FocusAreaWeight = dm->aa.afRegions[4];

    params->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h,
                                      &cropRegionRect.x, &cropRegionRect.y);
    newRect = convertingHWArea2AndroidArea(&CurrentFocusArea, &cropRegionRect);

    memset(&focusData, 0, sizeof(UniPluginFocusData_t));
    focusData.focusROI.left = newRect.x1;
    focusData.focusROI.right = newRect.x2;
    focusData.focusROI.top = newRect.y1;
    focusData.focusROI.bottom = newRect.y2;
    focusData.focusWeight = FocusAreaWeight;

    CLOGV("HLV: CurrentFocusArea : %d, %d, %d, %d",
        CurrentFocusArea.x1, CurrentFocusArea.x2,
        CurrentFocusArea.y1, CurrentFocusArea.y2);

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
    if (ret < 0)
        CLOGE("HLV: uni_plugin_set fail!");

    ret = m_UniPluginProcess();
    if (ret < 0)
        CLOGE("HLV: uni_plugin_process FAIL");

    return ret;
}

void ExynosCameraPPUniPluginHLV::m_init(void)
{
    CLOGD(" ");

    strncpy(m_uniPluginName, HIGHLIGHT_VIDEO_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_HLVstatus = HLV_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginHLV::start(void)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    CLOGD("[HLV]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[HLV] Plugin init failed!");
    }

    if (!strcmp(m_uniPluginName, HIGHLIGHT_VIDEO_PLUGIN_NAME)) {
        UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_START;
        ret = uni_plugin_set(m_uniPluginHandle,
            m_uniPluginName, UNI_PLUGIN_INDEX_CAPTURE_STATUS, &value);
        if (ret < 0) {
            CLOGE("Plugin set UNI_PLUGIN_INDEX_CAPTURE_STATUS failed!!");
        }
    }

    m_HLVstatus = HLV_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginHLV::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[HLV]");

    if (m_uniPluginHandle == NULL) {
        CLOGE("[HLV] HLV Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    if (!strcmp(m_uniPluginName, HIGHLIGHT_VIDEO_PLUGIN_NAME)) {
        UNI_PLUGIN_CAPTURE_STATUS value = UNI_PLUGIN_CAPTURE_STATUS_VID_REC_STOP;
        ret = uni_plugin_set(m_uniPluginHandle,
            m_uniPluginName, UNI_PLUGIN_INDEX_CAPTURE_STATUS, &value);
        if (ret < 0) {
            CLOGE("Plugin set UNI_PLUGIN_INDEX_CAPTURE_STATUS failed!!");
        }
    }

    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[HLV] Highlight Video plugin deinit failed!!");
    }

    m_HLVstatus = HLV_DEINIT;

    return ret;
}

