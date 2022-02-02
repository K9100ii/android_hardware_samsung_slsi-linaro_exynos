/*
 * Copyright (C) 2014, Samsung Electronics Co. LTD
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

//#define LOG_NDEBUG 0
#define LOG_TAG "ExynosCameraPlugInConverterArcsoftFusionZoomCapture"

#include "ExynosCameraPlugInConverterArcsoftFusionZoomCapture.h"
#include "ExynosCamera.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterArcsoftFusionZoomCapture::m_init(void)
{
    strncpy(m_name, "ConverterArcsoftFusionZoomCapture", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomCapture::m_make(Map_t *map)
{
    ALOGD("ExynosCameraPlugInConverterArcsoftFusionZoomCapture::m_make in");

    status_t ret = NO_ERROR;
    enum PLUGIN_CONVERT_TYPE_T type;
    ExynosCameraParameters *m_parameters = NULL;
    ExynosCameraConfigurations *configurations = NULL;
    ExynosCameraParameters *m_WideParameters = NULL;
    ExynosCameraParameters *m_TeleParameters = NULL;
    type = (enum PLUGIN_CONVERT_TYPE_T)(unsigned long)(*map)[PLUGIN_CONVERT_TYPE];
    m_parameters = (ExynosCameraParameters *)(*map)[PLUGIN_CONVERT_PARAMETER];

    configurations = (ExynosCameraConfigurations *)(*map)[PLUGIN_CONVERT_CONFIGURATIONS];
    if (configurations != NULL) {
        m_WideParameters = configurations->getParameters(m_cameraId);

        int teleCameraId = ExynosCameraPlugInConverterArcsoftFusion::ExynosCameraPlugInConverterArcsoftFusion::getOtherDualCameraId(m_cameraId);
        m_TeleParameters = configurations->getParameters(teleCameraId);
    } else {
        CLOGE("configurations is NULL");
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    switch (type) {
    case PLUGIN_CONVERT_PROCESS_BEFORE:
	    ret = m_convertParameters2Map(configurations, m_WideParameters, m_TeleParameters, map);
	    if (ret != NO_ERROR) {
		    CLOGE("convertBufferInfoFromParamters(%d, %d) fail",
				    m_pipeId, type);
		    goto func_exit;
	    }
	    break;
    case PLUGIN_CONVERT_PROCESS_AFTER:
    case PLUGIN_CONVERT_SETUP_BEFORE:
    case PLUGIN_CONVERT_SETUP_AFTER:
	    break;
    default:
	    CLOGE("invalid convert type(%d)!! pipeId(%d)", type, m_pipeId);
	    goto func_exit;
    }

    func_exit:

    return ret;

}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomCapture::m_convertParameters2Map(ExynosCameraConfigurations *configurations,
                                                                                      ExynosCameraParameters *wideParameters,
                                                                                      ExynosCameraParameters* teleParameters,
                                                                                      Map_t *map)
{
    int i, j;
    status_t ret = NO_ERROR;

    ExynosRect yuvWideCropSize;
    ExynosRect yuvTeleCropSize;
    enum DUAL_OPERATION_MODE dualOpMode = DUAL_OPERATION_MODE_MASTER;

    int zoomLevel = 0;

    ExynosCameraFrameSP_sptr_t frame = NULL;
    const struct camera2_shot_ext *metaData[2];

    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];

    if (wideParameters == NULL || teleParameters == NULL) {
        CLOGE("parameters is NULL !! pipeId(%d)", m_pipeId);
        ret = INVALID_OPERATION;
        return ret;
    }

    /* init all default converting variable */
    frame_type_t frameType = (frame_type_t)frame->getFrameType();
    CLOGV("frameType(%d)", frameType);

    m_syncType = ExynosCameraPlugInConverterArcsoftFusion::frameType2SyncType(frameType);
    (*map)[PLUGIN_SYNC_TYPE] = (Map_data_t)m_syncType;

    dualOpMode = ExynosCameraPlugInConverterArcsoftFusion::frameType2DualOperationMode(frameType);

    ExynosCameraPlugInConverterArcsoftFusion::getMetaData(frame, metaData);

    //wideParameters->getPreviewYuvCropSize(&yuvWideCropSize);
    //teleParameters->getPreviewYuvCropSize(&yuvTeleCropSize);
    wideParameters->getHwBayerCropRegion(&yuvWideCropSize.w,&yuvWideCropSize.h,&yuvWideCropSize.x,&yuvWideCropSize.y);
    m_wideFullsizeW =  yuvWideCropSize.w;
    m_wideFullsizeH =  yuvWideCropSize.h;

    teleParameters->getHwBayerCropRegion(&yuvTeleCropSize.w,&yuvTeleCropSize.h,&yuvTeleCropSize.x,&yuvTeleCropSize.y);
    m_teleFullsizeW =  yuvTeleCropSize.w;
    m_teleFullsizeH =  yuvTeleCropSize.h;

    ALOGD("Wide Full Size:(%d, %d), Tele Full Size:(%d, %d)",
        m_wideFullsizeW, m_wideFullsizeH, m_teleFullsizeW, m_teleFullsizeH);

    // TODO : seperate master / slave
    switch (metaData[0]->shot.dm.aa.afState) {
    case AA_AFSTATE_PASSIVE_FOCUSED:
    case AA_AFSTATE_FOCUSED_LOCKED:
        m_afStatus[0] = 1;
        m_afStatus[1] = 1;
        break;
    default:
        m_afStatus[0] = 0;
        m_afStatus[1] = 0;
        break;
    }

    (*map)[PLUGIN_WIDE_FULLSIZE_W] = (Map_data_t)m_wideFullsizeW;
    (*map)[PLUGIN_WIDE_FULLSIZE_H] = (Map_data_t)m_wideFullsizeH;
    (*map)[PLUGIN_TELE_FULLSIZE_W] = (Map_data_t)m_teleFullsizeW;
    (*map)[PLUGIN_TELE_FULLSIZE_H] = (Map_data_t)m_teleFullsizeH;

    (*map)[PLUGIN_AF_STATUS]       = (Map_data_t)m_afStatus;

    m_zoomRatio[0] = wideParameters->getActiveZoomRatio();
    m_zoomRatio[1] = teleParameters->getActiveZoomRatio();
    (*map)[PLUGIN_ZOOM_RATIO]      = (Map_data_t)m_zoomRatio;

    m_iso[0] = metaData[0]->shot.dm.aa.vendor_isoValue;
    m_iso[1] = metaData[1]->shot.dm.aa.vendor_isoValue;
	(*map)[PLUGIN_ISO_LIST]		   = (Map_data_t)m_iso;

    int cameraIndex = configurations->getArcSoftCameraIndex();
    (*map)[PLUGIN_CAMERA_INDEX] = (Map_data_t)cameraIndex;

    float viewRatio = configurations->getArcSoftViewRatio();
    (*map)[PLUGIN_VIEW_RATIO] = (Map_data_t)viewRatio;

    m_imageRatio[0] = wideParameters->getArcSoftImageRatio();
    m_imageRatio[1] = teleParameters->getArcSoftImageRatio();
    (*map)[PLUGIN_IMAGE_RATIO]  = (Map_data_t)m_imageRatio;

    int imageShiftX = 0;
    int imageShiftY = 0;

    configurations->getArcSoftImageShiftXY(&imageShiftX, &imageShiftY);
    (*map)[PLUGIN_IMAGE_SHIFT_X] = (Map_data_t)imageShiftX;
    (*map)[PLUGIN_IMAGE_SHIFT_Y] = (Map_data_t)imageShiftY;

    return ret;
}

}; /* namespace android */
