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
                                           ExynosCameraImage *dstImage)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;
    bool result = true;

    UniPluginBufferData_t bufferData;

    ExynosRect cropRegionRect;
    ExynosRect2 oldrect, newRect;
    ExynosCameraActivityControl *activityControl = NULL;
    ExynosCameraActivityAutofocus *autoFocusMgr = NULL;

    Mutex::Autolock l(m_uniPluginLock);

    if (m_UniPluginHandle == NULL) {
        CLOGE("[OBTR] OBTR Uni plugin(%s) is NULL!!", m_UniPluginName);
        return BAD_VALUE;
    }

    switch (m_OTstatus) {
    case OBJECT_TRACKING_IDLE:
        break;
    case OBJECT_TRACKING_RUN:
        memset(&bufferData, 0, sizeof(UniPluginBufferData_t));

        bufferData.InBuffY  = srcImage[0].buf.addr[0];
        bufferData.InBuffU  = srcImage[0].buf.addr[1];

        CLOGV("[OBTR] OBJECT_TRACKING_RUN [X%d Y%d W%d H%d FW%d FH%d CF%d]",
            srcImage[0].rect.x,
            srcImage[0].rect.y,
            srcImage[0].rect.w,
            srcImage[0].rect.h,
            srcImage[0].rect.fullW,
            srcImage[0].rect.fullH,
            srcImage[0].rect.colorFormat);

        bufferData.InWidth = srcImage[0].rect.w;
        bufferData.InHeight = srcImage[0].rect.h;
        bufferData.InFormat = UNI_PLUGIN_FORMAT_NV21;

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_BUFFER_INFO, &bufferData);
        if(ret < 0) {
            CLOGE("[OBTR] Object Tracking plugin set buffer info failed!!");
        }

        UniPluginExtraBufferInfo_t extraData;
        memset(&extraData, 0, sizeof(UniPluginExtraBufferInfo_t));
        extraData.zoomRatio = m_parameters->getZoomRatio();

        ret = m_UniPluginSet(UNI_PLUGIN_INDEX_EXTRA_BUFFER_INFO, &extraData);
        if(ret < 0)
            CLOGE("[OBTR]:Object Tracking plugin set extraBuffer info failed!!");

        ret = m_UniPluginProcess();
        if(ret < 0) {
           CLOGE("[OBTR] Object Tracking plugin process failed!!");
        }

        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_FOCUS_INFO, &m_OTfocusData);
        CLOGV("[OBTR] Focus state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
            m_OTfocusData.FocusState,
            m_OTfocusData.FocusROILeft, m_OTfocusData.FocusROIRight,
            m_OTfocusData.FocusROITop, m_OTfocusData.FocusROIBottom, m_OTfocusData.FocusWeight);
        CLOGV("[OBTR] Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
            m_OTfocusData.W_Movement, m_OTfocusData.H_Movement,
            m_OTfocusData.W_Velocity, m_OTfocusData.H_Velocity);

        ret = m_UniPluginGet(UNI_PLUGIN_INDEX_FOCUS_PREDICTED, &m_OTpredictedData);
        CLOGV("[OBTR] Predicted state: %d, x1: %d, x2: %d, y1: %d, y2: %d, weight: %d",
            m_OTpredictedData.FocusState,
            m_OTpredictedData.FocusROILeft, m_OTpredictedData.FocusROIRight,
            m_OTpredictedData.FocusROITop, m_OTpredictedData.FocusROIBottom, m_OTpredictedData.FocusWeight);
        CLOGV("[OBTR] Wmove: %d, Hmove: %d, Wvel: %d, Hvel: %d",
            m_OTpredictedData.W_Movement, m_OTpredictedData.H_Movement,
            m_OTpredictedData.W_Velocity, m_OTpredictedData.H_Velocity);

        oldrect.x1 = m_OTpredictedData.FocusROILeft;
        oldrect.x2 = m_OTpredictedData.FocusROIRight;
        oldrect.y1 = m_OTpredictedData.FocusROITop;
        oldrect.y2 = m_OTpredictedData.FocusROIBottom;

        m_parameters->getHwBayerCropRegion(&cropRegionRect.w, &cropRegionRect.h, &cropRegionRect.x, &cropRegionRect.y);
        newRect = convertingAndroidArea2HWAreaBcropOut(&oldrect, &cropRegionRect);

        m_OTpredictedData.FocusROILeft = newRect.x1;
        m_OTpredictedData.FocusROIRight = newRect.x2;
        m_OTpredictedData.FocusROITop = newRect.y1;
        m_OTpredictedData.FocusROIBottom = newRect.y2;

        activityControl = m_parameters->getActivityControl();
        autoFocusMgr = activityControl->getAutoFocusMgr();
#ifdef SAMSUNG_OT
        result = autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
        if (result == false) {
            CLOGE("[OBTR] setObjectTrackingAreas failed!!");
        }
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

    strncpy(m_UniPluginName, OBJECT_TRACKING_PLUGIN_NAME, EXYNOS_CAMERA_NAME_STR_SIZE - 1);

    m_srcImageCapacity.setNumOfImage(1);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_srcImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

    m_dstImageCapacity.setNumOfImage(1);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21);
    m_dstImageCapacity.addColorFormat(V4L2_PIX_FMT_NV21M);

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
    m_parameters->getObjectTrackingAreas(&validNumber, &objectTrackingArea, &objectTrackingWeight);
#endif

    focusData.FocusROILeft = objectTrackingArea.x1;
    focusData.FocusROIRight = objectTrackingArea.x2;
    focusData.FocusROITop = objectTrackingArea.y1;
    focusData.FocusROIBottom = objectTrackingArea.y2;
    focusData.FocusWeight = objectTrackingWeight;

    ret = m_UniPluginSet(UNI_PLUGIN_INDEX_FOCUS_INFO, &focusData);
    if(ret < 0) {
        CLOGE("[OBTR] Object Tracking plugin set focus info failed!!");
    } else {
        m_OTstatus = OBJECT_TRACKING_RUN;
    }

    CLOGD("[OBTR] OBJECT_TRACKING_INIT [L%d R%d T%d B%d W%d]",
        focusData.FocusROILeft,
        focusData.FocusROIRight,
        focusData.FocusROITop,
        focusData.FocusROIBottom,
        focusData.FocusWeight);

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
#ifdef SAMSUNG_OT
    autoFocusMgr->setObjectTrackingAreas(&m_OTpredictedData);
#endif

    if (m_UniPluginHandle == NULL) {
        CLOGE("[OBTR] OBTR Uni plugin(%s) is NULL!!", m_UniPluginName);
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

