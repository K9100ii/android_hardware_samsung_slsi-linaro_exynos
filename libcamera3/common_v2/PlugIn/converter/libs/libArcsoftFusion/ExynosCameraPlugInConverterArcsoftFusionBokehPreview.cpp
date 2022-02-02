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
#define LOG_TAG "ExynosCameraPlugInConverterArcsoftFusionBokehPreview"

#include "ExynosCameraPlugInConverterArcsoftFusionBokehPreview.h"
#include "ExynosCamera.h"


namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterArcsoftFusionBokehPreview::m_init(void)
{
    strncpy(m_name, "ConverterArcsoftFusionBokehPreview", (PLUGIN_NAME_STR_SIZE - 1));

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterArcsoftFusionBokehPreview::m_make(Map_t *map)
{
//    ALOGD("ExynosCameraPlugInConverterArcsoftFusionBokehPreview::m_make in");

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
        ret = m_convertMap2Parameters(map, m_WideParameters, m_TeleParameters);
        if(ret != NO_ERROR) {
            CLOGE("m_convertMap2Parameters(%d, %d) fail", m_pipeId, type);
            goto func_exit;
        }
        break;
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

status_t ExynosCameraPlugInConverterArcsoftFusionBokehPreview::m_convertParameters2Map(ExynosCameraConfigurations *configurations, 
                                                                                       ExynosCameraParameters *wideparameters,
                                                                                       ExynosCameraParameters* teleparameters,
                                                                                       Map_t *map)
{
    int i, j;
    status_t ret = NO_ERROR;

    ExynosRect yuvWideCropSize;
    ExynosRect yuvTeleCropSize;

    ExynosCameraFrameSP_sptr_t frame = NULL;
    struct camera2_shot_ext metaData;

    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];
    frame->getMetaData(&metaData);

    if (wideparameters == NULL || teleparameters == NULL) {
        CLOGE("parameters is NULL !! pipeId(%d)", m_pipeId);
        ret = INVALID_OPERATION;
        goto func_exit;
    }

    /* init all default converting variable */
    // ArcSoft: I think it should set like this. pls check
    // Samgsung : OK. But, let's check, after merge.
    wideparameters->getHwBayerCropRegion(&yuvWideCropSize.w,&yuvWideCropSize.h,&yuvWideCropSize.x,&yuvWideCropSize.y);
    teleparameters->getHwBayerCropRegion(&yuvTeleCropSize.w,&yuvTeleCropSize.h,&yuvTeleCropSize.x,&yuvTeleCropSize.y);

    m_wideFullsizeW =  yuvWideCropSize.w;
    m_wideFullsizeH =  yuvWideCropSize.h;

#ifdef USE_VENDOR_BOKEH_SUPPORT
    m_teleFullsizeW =  yuvTeleCropSize.w * USE_BOKEH_BINNING_RATIO;
    m_teleFullsizeH =  yuvTeleCropSize.h * USE_BOKEH_BINNING_RATIO;
#else
    m_teleFullsizeW =  yuvTeleCropSize.w;
    m_teleFullsizeH =  yuvTeleCropSize.h;
#endif

    m_caliData      = NULL;
    m_caliSize      = 2048;

    m_bokehIntensity = configurations->getBokehBlurStrength();

    // ArcSoft: I think it should set like this, because we need focus point related to input image, not roi width and height. pls check
    m_focusX = (metaData.shot.dm.aa.afRegions[2] + metaData.shot.dm.aa.afRegions[0]) / 2;
    if (m_focusX <= 0) {
        m_focusX = m_wideFullsizeW / 2;

        CLOGW("m_focusX <= 0. afRegions[0](%d), afRegions[2](%d). so, just set %d",
            metaData.shot.dm.aa.afRegions[0],
            metaData.shot.dm.aa.afRegions[2],
            m_srcBufRect[0][2] / 2);
    }

    m_focusY = (metaData.shot.dm.aa.afRegions[3] + metaData.shot.dm.aa.afRegions[1]) / 2;
    if (m_focusY <= 0) {
        m_focusY = m_wideFullsizeH / 2;

        CLOGW("m_focusY <= 0. afRegions[1](%d), afRegions[3](%d). so, just set %d",
            metaData.shot.dm.aa.afRegions[1],
            metaData.shot.dm.aa.afRegions[3],
            m_srcBufRect[0][3] / 2);
    }

    // TODO : seperate master / slave
    switch (metaData.shot.dm.aa.afState) {
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

//    ALOGD("Wide crop size :%d,%d,%d,%d", yuvWideCropSize.w, yuvWideCropSize.h, yuvWideCropSize.x, yuvWideCropSize.y);
//    ALOGD("Tele crop size :%d,%d,%d,%d", yuvTeleCropSize.w, yuvTeleCropSize.h, yuvTeleCropSize.x, yuvTeleCropSize.y);

    (*map)[PLUGIN_WIDE_FULLSIZE_W]  = (Map_data_t)m_wideFullsizeW;
    (*map)[PLUGIN_WIDE_FULLSIZE_H]  = (Map_data_t)m_wideFullsizeH;
    (*map)[PLUGIN_TELE_FULLSIZE_W]  = (Map_data_t)m_teleFullsizeW;
    (*map)[PLUGIN_TELE_FULLSIZE_H]  = (Map_data_t)m_teleFullsizeH;

    (*map)[PLUGIN_FOCUS_POSTRION_X] = (Map_data_t)m_focusX;
    (*map)[PLUGIN_FOCUS_POSTRION_Y] = (Map_data_t)m_focusY;
    (*map)[PLUGIN_AF_STATUS]        = (Map_data_t)m_afStatus;
    (*map)[PLUGIN_CALI_DATA]        = (Map_data_t)m_caliData;
    (*map)[PLUGIN_CALI_SIZE]        = (Map_data_t)m_caliSize;

    (*map)[PLUGIN_BOKEH_INTENSITY]  = (Map_data_t)m_bokehIntensity;

func_exit:

    return ret;
}

status_t ExynosCameraPlugInConverterArcsoftFusionBokehPreview::m_convertMap2Parameters(Map_t *map, ExynosCameraParameters *wideParameters, ExynosCameraParameters* teleParameters)
{
    status_t ret = NO_ERROR;

    return ret;
}

}; /* namespace android */
