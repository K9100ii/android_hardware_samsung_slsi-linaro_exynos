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
#define LOG_TAG "ExynosCameraPPUniPluginOT"

#include "ExynosCameraPPUniPluginOT.h"

ExynosCameraPPUniPluginOT::~ExynosCameraPPUniPluginOT()
{
}

status_t ExynosCameraPPUniPluginOT::m_draw(ExynosCameraImage *srcImage,
                                           ExynosCameraImage *dstImage,
                                           ExynosCameraParameters *params)
{
    status_t ret = NO_ERROR;
    bool result = true;

    UniPluginBufferData_t bufferData;

    ExynosRect cropRegionRect;
    ExynosRect2 oldrect, newRect;
    ExynosCameraActivityControl *activityControl = NULL;
    ExynosCameraActivityAutofocus *autoFocusMgr = NULL;
    ExynosRect hwSensorSize;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_uniPluginHandle == NULL) {
        CLOGE("[OBTR] OBTR Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    switch (m_OTstatus) {
    case OBJECT_TRACKING_IDLE:
        break;
    case OBJECT_TRACKING_RUN:
        if (srcImage[0].buf.index < 0 || dstImage[0].buf.index < 0) {
            return NO_ERROR;
        }

        memset(&bufferData, 0, sizeof(UniPluginBufferData_t));

        bufferData.inBuffY  = srcImage[0].buf.addr[0];
        bufferData.inBuffU  = srcImage[0].buf.addr[1];

        CLOGV("[OBTR] OBJECT_TRACKING_RUN [X%d Y%d W%d H%d FW%d FH%d CF%d]",
            srcImage[0].rect.x,
            srcImage[0].rect.y,
            srcImage[0].rect.w,
            srcImage[0].rect.h,
            srcImage[0].rect.fullW,
            srcImage[0].rect.fullH,
            srcImage[0].rect.colorFormat);

        bufferData.inWidth = srcImage[0].rect.w;
        bufferData.inHeight = srcImage[0].rect.h;
        bufferData.inFormat = UNI_PLUGIN_FORMAT_NV21;

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
        if(ret < 0) {
            CLOGE("[OBTR] Object Tracking plugin set buffer info failed!!");
        }

        UniPluginExtraBufferInfo_t extraData;
        memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
        extraData.zoomRatio = m_configurations->getZoomRatio();

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
        if(ret < 0)
            CLOGE("[OBTR]:Object Tracking plugin set extraBuffer info failed!!");

        ret = m_UniPluginProcess();
        if(ret < 0) {
           CLOGE("[OBTR] Object Tracking plugin process failed!!");
        }

        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_FOCUS_INFO, &m_OTfocusData);
        CLOGV("[OBTR] Focus state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
            m_OTfocusData.focusState,
            m_OTfocusData.focusROI.left, m_OTfocusData.focusROI.right,
            m_OTfocusData.focusROI.top, m_OTfocusData.focusROI.bottom, m_OTfocusData.focusWeight);
        CLOGV("[OBTR] Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
            m_OTfocusData.w_movement, m_OTfocusData.h_movement,
            m_OTfocusData.w_velocity, m_OTfocusData.h_velocity);

        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_FOCUS_PREDICTED, &m_OTpredictedData);
        CLOGV("[OBTR] Predicted state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
            m_OTpredictedData.focusState,
            m_OTpredictedData.focusROI.left, m_OTpredictedData.focusROI.right,
            m_OTpredictedData.focusROI.top, m_OTpredictedData.focusROI.bottom, m_OTpredictedData.focusWeight);
        CLOGV("[OBTR] Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
            m_OTpredictedData.w_movement, m_OTpredictedData.h_movement,
            m_OTpredictedData.w_velocity, m_OTpredictedData.h_velocity);

        oldrect.x1 = m_OTpredictedData.focusROI.left;
        oldrect.x2 = m_OTpredictedData.focusROI.right;
        oldrect.y1 = m_OTpredictedData.focusROI.top;
        oldrect.y2 = m_OTpredictedData.focusROI.bottom;

        params->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
        newRect = convertingAndroidArea2HWAreaBcropOut(&oldrect, &cropRegionRect);

        m_OTpredictedData.focusROI.left = newRect.x1;
        m_OTpredictedData.focusROI.right = newRect.x2;
        m_OTpredictedData.focusROI.top = newRect.y1;
        m_OTpredictedData.focusROI.bottom = newRect.y2;

        activityControl = params->getActivityControl();
        autoFocusMgr = activityControl->getAutoFocusMgr();
#ifdef SAMSUNG_OT
        result = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
        if (result == false) {
            CLOGE("[OBTR] setObjectTrackingAreas failed!!");
        }
#endif

        oldrect.x1 = m_OTfocusData.focusROI.left;
        oldrect.x2 = m_OTfocusData.focusROI.right;
        oldrect.y1 = m_OTfocusData.focusROI.top;
        oldrect.y2 = m_OTfocusData.focusROI.bottom;

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
        if (m_configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
            && m_configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
            ExynosRect fusionSrcRect;
            ExynosRect fusionDstRect;
            int outWidth = 0, outHeight = 0;
            m_configurations->getSize(CONFIGURATION_PREVIEW_SIZE, (uint32_t *)&outWidth, (uint32_t *)&outHeight);

            /* Don't care outWidth and outHeight */
            m_parameters->getFusionSize(outWidth, outHeight, &fusionSrcRect, &fusionDstRect);
            cropRegionRect.x = 0;
            cropRegionRect.y = 0;
            cropRegionRect.w = fusionDstRect.w;
            cropRegionRect.h = fusionDstRect.h;
        } else
#endif
        {
            m_parameters->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
        }

        newRect = convertingAndroidArea2HWArea(&oldrect, &cropRegionRect);
        CLOGV("[OBTR] newRect(%d,%d,%d,%d)", newRect.x1, newRect.y1, newRect.x2, newRect.y2);

        m_OTfocusData.focusROI.left = newRect.x1;
        m_OTfocusData.focusROI.right = newRect.x2;
        m_OTfocusData.focusROI.top = newRect.y1;
        m_OTfocusData.focusROI.bottom = newRect.y2;

#ifdef SAMSUNG_OT
        m_configurations->setObjectTrackingFocusData(&m_OTfocusData);
#endif
        break;
    case OBJECT_TRACKING_DEINIT:
        CLOGD("[OBTR] OBJECT_TRACKING_DEINIT");
        break;
    default:
        break;
    }

    return ret;
}

void ExynosCameraPPUniPluginOT::m_init(void)
{
    CLOGD("[OBTR]");

    strncpy(m_uniPluginName, OBJECT_TRACKING_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    memset(&m_OTfocusData, 0, sizeof(UniPluginFocusData_t));
    memset(&m_OTpredictedData, 0, sizeof(UniPluginFocusData_t));
    m_OTstatus = OBJECT_TRACKING_IDLE;

    m_refCount = 0;
}

status_t ExynosCameraPPUniPluginOT::start(void)
{
    status_t ret = NO_ERROR;
    UniPluginFocusData_t focusData;

    ExynosRect2 objectTrackingArea;
    int objectTrackingWeight;
    int validNumber;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_refCount++ > 0) {
        return ret;
    }

    if (m_OTstatus == OBJECT_TRACKING_RUN) {
        return ret;
    }

    CLOGD("[OBTR]");

    ret = m_UniPluginInit();
    if (ret != NO_ERROR) {
        CLOGE("[OBTR] LLS Plugin init failed!!");
    }

    memset(&focusData, 0, sizeof(UniPluginFocusData_t));
#ifdef SAMSUNG_OT
    m_configurations->getObjectTrackingAreas(&validNumber, &objectTrackingArea, &objectTrackingWeight);
#endif

    focusData.focusROI.left = objectTrackingArea.x1;
    focusData.focusROI.right = objectTrackingArea.x2;
    focusData.focusROI.top = objectTrackingArea.y1;
    focusData.focusROI.bottom = objectTrackingArea.y2;
    focusData.focusWeight = objectTrackingWeight;

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
    if(ret < 0) {
        CLOGE("[OBTR] Object Tracking plugin set focus info failed!!");
    } else {
        m_OTstatus = OBJECT_TRACKING_RUN;
    }

    CLOGD("[OBTR] OBJECT_TRACKING_INIT [L%d R%d T%d B%d W%d]",
        focusData.focusROI.left,
        focusData.focusROI.right,
        focusData.focusROI.top,
        focusData.focusROI.bottom,
        focusData.focusWeight);

    m_OTstatus = OBJECT_TRACKING_RUN;

    return ret;
}

status_t ExynosCameraPPUniPluginOT::stop(bool suspendFlag)
{
    status_t ret = NO_ERROR;
    ExynosCameraActivityControl *activityControl = NULL;
    ExynosCameraActivityAutofocus *autoFocusMgr = NULL;

    Mutex::Autolock l(m_uniPluginLock);

    if (--m_refCount > 0) {
        return ret;
    }

    CLOGD("[OBTR] suspendFlag(%d)", suspendFlag);

    if (suspendFlag == true) {
        return ret;
    }

    activityControl = m_parameters->getActivityControl();
    autoFocusMgr = activityControl->getAutoFocusMgr();
    memset(&m_OTpredictedData, 0, sizeof(UniPluginFocusData_t));
    memset(&m_OTfocusData, 0, sizeof(UniPluginFocusData_t));
#ifdef SAMSUNG_OT
    autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
    m_configurations->setObjectTrackingFocusData(&m_OTfocusData);
#endif

    if (m_uniPluginHandle == NULL) {
        CLOGE("[OBTR] OBTR Uni plugin(%s) is NULL!!", m_uniPluginName);
        return BAD_VALUE;
    }

    /* Stop case */
    ret = m_UniPluginDeinit();
    if (ret < 0) {
        CLOGE("[OBTR] Object Tracking plugin deinit failed!!");
    }

    m_OTstatus = OBJECT_TRACKING_DEINIT;

    return ret;
}

