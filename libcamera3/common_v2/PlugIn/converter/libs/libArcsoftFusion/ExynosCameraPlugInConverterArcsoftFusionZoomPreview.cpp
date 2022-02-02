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
#define LOG_TAG "ExynosCameraPlugInConverterArcsoftFusionZoomPreview"

#include "ExynosCameraPlugInConverterArcsoftFusionZoomPreview.h"
#include "ExynosCamera.h"

namespace android {

/*********************************************/
/*  protected functions                      */
/*********************************************/
status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_init(void)
{
    strncpy(m_name, "ConverterArcsoftFusionZoomPreview", (PLUGIN_NAME_STR_SIZE - 1));

    m_syncType = 0;

    m_wideFullsizeW = 0;
    m_wideFullsizeH = 0;
    m_teleFullsizeW = 0;
    m_teleFullsizeH = 0;

    for (int i = 0; i < 2; i++) {
        m_afStatus[i] = 0;
        m_iso[i] = 0;
        m_zoomRatio[i] = 1000.0f;

        m_zoomRatioListSize[i] = 0;
        m_zoomRatioList[i] = NULL;
    }
    m_flagHaveZoomRatioList = false;

    m_oldDualOpMode = DUAL_OPERATION_MODE_NONE;
    m_oldfallback = -1;
    m_oldMaster3a = DUAL_OPERATION_MODE_NONE;
    m_oldMasterCameraHAL2Arc = -1;

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_deinit(void)
{
    if (m_flagHaveZoomRatioList == true) {
        for (int i = 0; i < 2; i++) {
            m_zoomRatioListSize[i] = 0;
            SAFE_DELETE(m_zoomRatioList[i]);
        }

        m_flagHaveZoomRatioList = false;
    }

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_setup(Map_t *map)
{
    ALOGD("m_setup()");

    ExynosCameraParameters *pParameters = NULL;

    pParameters = (ExynosCameraParameters *)(*map)[PLUGIN_CONVERT_PARAMETER];

    int maxSensorW = 0;
    int maxSensorH = 0;
    int previewW = 0;
    int previewH = 0;

    pParameters->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&maxSensorW, (uint32_t *)&maxSensorH);

    int portId = pParameters->getPreviewPortId();
    pParameters->getSize(HW_INFO_HW_YUV_SIZE, (uint32_t *)&previewW, (uint32_t *)&previewH, portId);

    m_srcBufRect[0][PLUGIN_ARRAY_RECT_X] = 0;
    m_srcBufRect[0][PLUGIN_ARRAY_RECT_Y] = 0;
    m_srcBufRect[0][PLUGIN_ARRAY_RECT_W] = previewW;
    m_srcBufRect[0][PLUGIN_ARRAY_RECT_H] = previewH;
    m_srcBufRect[0][PLUGIN_ARRAY_RECT_FULL_W] = maxSensorW;
    m_srcBufRect[0][PLUGIN_ARRAY_RECT_FULL_H] = maxSensorH;

    (*map)[PLUGIN_SRC_BUF_RECT]        = (Map_data_t)m_srcBufRect;

    int *zoomList;
    int maxZoomLevel;
    // hack : comment out for compile
#if 0
    pParameters->getZoomList(&zoomList, &maxZoomLevel);

    m_zoomRatioListSize[0] = maxZoomLevel;
    m_zoomRatioListSize[1] = maxZoomLevel;

    //Wide
    if (m_zoomRatioList[0] == NULL) {
        m_zoomRatioList[0] = new float[maxZoomLevel];
    }

    for(int i = 0; i < maxZoomLevel; i++) {
        m_zoomRatioList[0][i] = (float)((float)zoomList[i] / 1000.f);
        //ALOGD("zoomList[%d] = %d, m_zoomRatioList[0][%d] = %f", i, zoomList[i], i, m_zoomRatioList[0][i]);
    }
#else
    maxZoomLevel = 10;

    m_zoomRatioListSize[0] = maxZoomLevel;
    m_zoomRatioListSize[1] = maxZoomLevel;

    //Wide
    if(m_zoomRatioList[0] == NULL) {
        m_zoomRatioList[0] = new float[maxZoomLevel];
    }

    for(int i = 0; i < maxZoomLevel; i++) {
        m_zoomRatioList[0][i] = (float)((float)i * 1000.f);
    }
#endif
    (*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE] = (Map_data_t)m_zoomRatioListSize;
    (*map)[PLUGIN_ZOOM_RATIO_LIST]        = (Map_data_t)m_zoomRatioList;

    return NO_ERROR;
}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_make(Map_t *map)
{
    ALOGV("ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_make in");

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
    ret = m_convertMap2Parameters(map, configurations, m_WideParameters, m_TeleParameters);
        if (ret != NO_ERROR) {
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

/*
[ ArcSoft Preview Pseudo code ]

ArcSoft set to HAL() {
    ArcsoftFusionZoomPreview::m_getCameraControlFlag() {

        // !!! Blue Line
        // arcSoft library set CameraControlFlag, about which camera open.
        switch (CameraControlFlag & ARC_DCOZ_CAMERANEEDFLAGMASK) {
        case ARC_DCOZ_NEEDWIDEFRAMEONLY:
            NeedSwitchCamera = PLUGIN_CAMERA_TYPE_WIDE;
            break;
        case ARC_DCOZ_NEEDTELEFRAMEONLY:
            NeedSwitchCamera = PLUGIN_CAMERA_TYPE_TELE;
            break;
        case ARC_DCOZ_NEEDBOTHFRAMES:
            NeedSwitchCamera = PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE
            break;
        }

        // !!! Green Line
        // arcSoft library set CameraControlFlag, about which camera is master
        switch (CameraControlFlag & ARC_DCOZ_RECOMMENDMASK){
        case ARC_DCOZ_RECOMMENDTOTELE:
            SwitchTo = RECOMMEND_TO_TELE;
            break;
        case ARC_DCOZ_RECOMMENDTOWIDE:
            SwitchTo = RECOMMEND_TO_WIDE;
            break;
        case ARC_DCOZ_RECOMMENDFB:
            pCameraParam.SwitchTo = RECOMMEND_TO_FB;
            break;
        }
    }

    ExynosCameraParameters::setMasterCamera() {
        // !!! Green Line
        if (HAL get ARC_DCOZ_RECOMMENDTOWIDE or ARC_DCOZ_RECOMMENDFB from zoom library) {
            camera2_uctl.masterCam = AA_CAMERATYPE_WIDE;
        }
        else if (HAL get  ARC_DCOZ_RECOMMENDTOTELE  from zoom library) {
            camera2_uctl.masterCam = AA_CAMERATYPE_TELE;
        }
        else { // single scenario such as recording
            camera2_uctl.masterCam = AA_CAMERATYPE_SINGLE;
        }
    }
}

HAL set to ArcSoft() {

    ArcsoftFusionZoomPreview::m_setFallbackFlag() {
        // !!! Blue Line (Red Letter)
        // just set fallback flag here, zoom library will inform HAL to change mode in m_getCameraControlFlag()

        int32_t focus_distance = meta->shot.dm.aa.vendor_objectDistanceCm;
        int32_t illumination = (int32_t)meta->shot.udm.ae.vendorSpecific2[101];

        bool bFallback = false;

        // if ARC_DCOZ_WIDECAMERAOPENED,
        // send focus distance and illumination of Wide Camera,
        if (wide-> focus distance < 50cm or wide-> illumination > ISO800)

        // if ARC_DCOZ_TELECAMERAOPENED,
        // send focus distance and illumination of Tele Camera,
        if (tele-> focus distance < 50cm or tele-> illumination > ISO800)

        // if ARC_DCOZ_WIDECAMERAOPENED | ARC_DCOZ_TELECAMERAOPENED,
        // send focus distance and illumination of both Wide and Tele Camera
        if (wide-> focus distance < 50cm or wide-> illumination > ISO800 ||
            tele-> focus distance < 50cm or tele-> illumination > ISO800) {
            bFallback = true;
        } else {
            bFallback = false;
        }
    }

    bool ArcsoftFusionZoomPreview::m_setFallbackFlag() {
    {
        // !!! Blue Line
        // About which camera open.
        switch (hal frame mode) {
        case bypass_mode:
            CameraControlFlag = ARC_DCOZ_WIDECAMERAOPENED;
            break;
        case sync_smode:
            CameraControlFlag = ARC_DCOZ_WIDECAMERAOPENED | ARC_DCOZ_TELECAMERAOPENED;
            break;
        case switch_mode:
            CameraControlFlag = ARC_DCOZ_TELECAMERAOPENED;
            break;
        }

        // !!! Blue Point Line
        // About fallback
        // it is decided NeedToImplement_0()
        if (bFallback == true) {
            ARC_DCOZ_SWITCHTOWIDE to inform Arcsoft lilbrary to enter fallback mode
        } else {
            ARC_DCOZ_SWITCHTOWIDE to inform Arcsoft lilbrary to enter fallback mode
        }

        // !!! Green Line
        // About which camera master.
        if (3A master == Wide) {
            CameraControlFlag = ARC_DCOZ_WIDESESSION;
        } else { // 3A master == Tele
            CameraControlFlag = ARC_DCOZ_TELESESSION;
        }
    }
}
*/

bool ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_checkFallback(enum DUAL_OPERATION_MODE dualOpMode,
                                                                          float zoomRatio,
                                                                          const struct camera2_shot_ext* wideMeta,
                                                                          const struct camera2_shot_ext* teleMeta)
{
    bool fallback = false;

    int cameraIndex = 0;
    enum aa_fallback metaFallback = AA_FALLBACK_INACTIVE;

    switch (dualOpMode) {
    case DUAL_OPERATION_MODE_MASTER:
        cameraIndex = 0;
        metaFallback = wideMeta->shot.udm.fallback;
        break;
    case DUAL_OPERATION_MODE_SYNC:
    {
        enum aa_cameraMode cameraMode;
        enum aa_sensorPlace masterCamera;
        struct camera2_shot_ext* meta = (struct camera2_shot_ext*)wideMeta;

        getMetaCtlMasterCamera(meta, &cameraMode, &masterCamera);

        if (masterCamera == AA_SENSORPLACE_REAR) {
            cameraIndex = 0;
            metaFallback = wideMeta->shot.udm.fallback;
        } else if (masterCamera == AA_SENSORPLACE_REAR_SECOND) {
            cameraIndex = 1;
            metaFallback = teleMeta->shot.udm.fallback;
        } else {
            CLOGE("Invalid masterCamera(%d). so, just set by wide", masterCamera);
            cameraIndex = 0;
            metaFallback = wideMeta->shot.udm.fallback;
        }
        break;
    }
    case DUAL_OPERATION_MODE_SLAVE:
        cameraIndex = 1;
        metaFallback = teleMeta->shot.udm.fallback;
        break;
    default:
        CLOGW("Invalid dualOpMode(%d). so, just set fallback as false", dualOpMode);
        metaFallback = AA_FALLBACK_INACTIVE;
        break;
    }

    // refer Pseudo code
    if (metaFallback == AA_FALLBACK_ACTIVE) {
        fallback = true;
    } else if (metaFallback == AA_FALLBACK_INACTIVE) {
        fallback = false;
    } else {
        CLOGE("Invalid metaFallback(%d). so, just set fallback = false", metaFallback);
        fallback = false;
    }

    CLOGV("dualOpMode(%d), cameraIndex(%d), metaFallback(%d) -> fallback(%d)", dualOpMode, cameraIndex, metaFallback, fallback);

    return fallback;
}

status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_convertParameters2Map(ExynosCameraConfigurations *configurations,
                                                                                      ExynosCameraParameters *wideParameters,
                                                                                      ExynosCameraParameters* teleParameters,
                                                                                      Map_t *map)
{
    int i, j;
    status_t ret = NO_ERROR;

    ExynosRect yuvWideCropSize;
    ExynosRect yuvTeleCropSize;
    enum DUAL_OPERATION_MODE dualOpMode = DUAL_OPERATION_MODE_MASTER;

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

    /* face detection */
    int numOfDetectedFaces = 0;
    for (int i = 0; i < NUM_OF_DETECTED_FACES; i++) {
        if (metaData[0]->shot.dm.stats.faceIds[i] || metaData[1]->shot.dm.stats.faceIds[i]) {
            numOfDetectedFaces++;
        }
    }

    (*map)[PLUGIN_MASTER_FACE_RECT] = (Map_data_t)(&metaData[0]->shot.dm.stats.faceRectangles);
    (*map)[PLUGIN_SLAVE_FACE_RECT] = (Map_data_t)(&metaData[1]->shot.dm.stats.faceRectangles);
    (*map)[PLUGIN_FACE_NUM] = (Map_data_t)numOfDetectedFaces;

    // hack : comment out for compile
    //(*map)[PLUGIN_BUTTON_ZOOM] = (Map_data_t)wideParameters->getButtonZoomMode();

    /*
     * The Sequence
     * 0. ArcSoft give "pCameraParam.SwitchTo" in ArcsoftFusionZoomPreview::m_getCameraControlFlag().
     * 1. ArcsoftFusion::m_saveValue() give "the value" through PLUGIN_MASTER_CAMERA_ARC2HAL.
     * 2. HAL set "the value" on DDK, and, it change the master camera.
     * 3. ExynosCameraPlugInConverterArcsoftFusionZoomPreview can get "the really changed master camera" through "int masterCameraHAL2Arc" in this m_convertParameters2Map().
     * 4. it put "the really changed master camera" through PLUGIN_MASTER_CAMERA_HAL2ARC.
     * 5. then, we finally set "the really changed master camera" on arcSoft lib, on ArcsoftFusionZoomPreview::m_setCameraControlFlag().
     */
    enum DUAL_OPERATION_MODE master3a = frame->getMaster3a();
    int masterCameraHAL2Arc = 0x30;

    // /ArcsoftFusionZoomPreview.h
    //#define RECOMMEND_DEFAULT(0x00)
    //#define RECOMMEND_TO_TELE(0x10)
    //#define RECOMMEND_TO_FB  (0x20)
    //#define RECOMMEND_TO_WIDE(0x30)

    switch (master3a) {
    case DUAL_OPERATION_MODE_MASTER:
    case DUAL_OPERATION_MODE_SYNC:
        masterCameraHAL2Arc = 0x30;
        break;
    case DUAL_OPERATION_MODE_SLAVE:
        masterCameraHAL2Arc = 0x10;
        break;
    default:
        masterCameraHAL2Arc = 0x30;
        CLOGE("Invalid master3a(%d). so, work as master", master3a);
        break;

    }

    (*map)[PLUGIN_MASTER_CAMERA_HAL2ARC] = (Map_data_t)masterCameraHAL2Arc;

    //wideParameters->getPreviewYuvCropSize(&yuvWideCropSize);
    //teleParameters->getPreviewYuvCropSize(&yuvTeleCropSize);
    wideParameters->getHwBayerCropRegion(&yuvWideCropSize.w,&yuvWideCropSize.h,&yuvWideCropSize.x,&yuvWideCropSize.y);
    m_wideFullsizeW =  yuvWideCropSize.w;
    m_wideFullsizeH =  yuvWideCropSize.h;

    teleParameters->getHwBayerCropRegion(&yuvTeleCropSize.w,&yuvTeleCropSize.h,&yuvTeleCropSize.x,&yuvTeleCropSize.y);
    m_teleFullsizeW =  yuvTeleCropSize.w;
    m_teleFullsizeH =  yuvTeleCropSize.h;

    // TODO : seperate master / slave
    for (int i = 0; i < 2; i++) {
        switch (metaData[i]->shot.dm.aa.afState) {
        case AA_AFSTATE_PASSIVE_FOCUSED:
        case AA_AFSTATE_FOCUSED_LOCKED:
            m_afStatus[i] = 1;
            break;
        default:
            m_afStatus[i] = 0;
            break;
        }
    }

    (*map)[PLUGIN_WIDE_FULLSIZE_W] = (Map_data_t)m_wideFullsizeW;
    (*map)[PLUGIN_WIDE_FULLSIZE_H] = (Map_data_t)m_wideFullsizeH;
    (*map)[PLUGIN_TELE_FULLSIZE_W] = (Map_data_t)m_teleFullsizeW;
    (*map)[PLUGIN_TELE_FULLSIZE_H] = (Map_data_t)m_teleFullsizeH;

    (*map)[PLUGIN_AF_STATUS]       = (Map_data_t)m_afStatus;

    // HAL1
    m_zoomLevel = (int)(frame->getActiveZoomRatio() / 1000);
    (*map)[PLUGIN_ZOOM_LEVEL]      = (Map_data_t)m_zoomLevel;
    // HAL3
    //?

    m_zoomRatio[0] = wideParameters->getActiveZoomRatio();
    m_zoomRatio[1] = teleParameters->getActiveZoomRatio();
    (*map)[PLUGIN_ZOOM_RATIO]      = (Map_data_t)m_zoomRatio;

    m_iso[0] = metaData[0]->shot.dm.aa.vendor_isoValue;
    m_iso[1] = metaData[1]->shot.dm.aa.vendor_isoValue;
    (*map)[PLUGIN_ISO_LIST]        = (Map_data_t)m_iso;

    // fallback
    m_fallback = m_checkFallback(master3a, m_zoomRatio[0], metaData[0], metaData[1]);

    (*map)[PLUGIN_FALLBACK] = (Map_data_t)m_fallback;

    if (m_oldDualOpMode != dualOpMode ||
        m_oldfallback != m_fallback ||
        m_oldMaster3a != master3a ||
        m_oldMasterCameraHAL2Arc != masterCameraHAL2Arc) {
        CLOGD("[F%d] dualOpMode(%d), m_fallback(%d), master3a(%d), masterCameraHAL2Arc(0x%x)",
            frame->getFrameCount(), dualOpMode, m_fallback, master3a, masterCameraHAL2Arc);

        m_oldDualOpMode = dualOpMode;
        m_oldfallback = m_fallback;
        m_oldMaster3a = master3a;
        m_oldMasterCameraHAL2Arc = masterCameraHAL2Arc;
    }
    ///////////////////////////////////

    // hack : comment out for compile
#if 0
    // zoomRatioList
    if (m_flagHaveZoomRatioList == false) {
        int *zoomList;
        int zoomListSize;

        // wide
        wideParameters->getZoomList(&zoomList, &zoomListSize);

        m_zoomRatioListSize[0] = zoomListSize;

        if (m_zoomRatioList[0] == NULL) {
            m_zoomRatioList[0] = new float[zoomListSize];
        }

        for (int i = 0; i < zoomListSize; i++) {
            m_zoomRatioList[0][i] = (float)(zoomList[i] / 1000.f);
        }

        // tele
        teleParameters->getZoomList(&zoomList, &zoomListSize);

        m_zoomRatioListSize[1] = zoomListSize;

        if (m_zoomRatioList[1] == NULL) {
            m_zoomRatioList[1] = new float[zoomListSize];
        }

        for (int i = 0; i < zoomListSize; i++) {
            m_zoomRatioList[1][i] = (float)(zoomList[i] / 1000.f);
        }

        (*map)[PLUGIN_ZOOM_RATIO_LIST_SIZE] = (Map_data_t)m_zoomRatioListSize;
        (*map)[PLUGIN_ZOOM_RATIO_LIST]      = (Map_data_t)m_zoomRatioList;

        m_flagHaveZoomRatioList = true;
    }
#endif

    {
        static int prevInputDualOpMode = DUAL_OPERATION_MODE_NONE;
        if (prevInputDualOpMode != dualOpMode) {
            CLOGD("library input dualOpMode(%d ->%d)", prevInputDualOpMode, dualOpMode);
            prevInputDualOpMode = dualOpMode;
        }
    }

func_exit:

    return ret;
}


status_t ExynosCameraPlugInConverterArcsoftFusionZoomPreview::m_convertMap2Parameters(Map_t *map,
                                                                                      ExynosCameraConfigurations *configurations,
                                                                                      ExynosCameraParameters *wideParameters,
                                                                                      ExynosCameraParameters* teleParameters)
{
    int i, j;
    status_t ret = NO_ERROR;

    ExynosCameraFrameSP_sptr_t frame = NULL;

    frame = (ExynosCameraFrame *)(*map)[PLUGIN_CONVERT_FRAME];

    // Camera Index setting : for capture to know.
    int cameraIndex = (Data_int32_t)(*map)[PLUGIN_CAMERA_INDEX];
    configurations->setArcSoftCameraIndex(cameraIndex);

    //#define ARC_DCOZ_BASEWIDE       (0x2)
    //#define ARC_DCOZ_BASETELE       (0x3)

    int syncType = DUAL_OPERATION_MODE_MASTER;

    CLOGV("cameraIndex : %d", cameraIndex);

    if (cameraIndex == 0x2) { // ARC_DCOZ_BASEWIDE
        syncType = DUAL_OPERATION_MODE_MASTER;
    } else if (cameraIndex == 0x3) { // ARC_DCOZ_BASETELE
        syncType = DUAL_OPERATION_MODE_SLAVE;
    } else {
        CLOGE("Invalid cameraIndex(%d). so, set DUAL_OPERATION_MODE_MASTER", cameraIndex);

        syncType = DUAL_OPERATION_MODE_MASTER;
    }

    // Switch Camera : to set HAL for changing camera
    int needSwitchCamera = (Data_int32_t)(*map)[PLUGIN_NEED_SWITCH_CAMERA];

    // ArcsoftFusionZoomPreview.h
    // PLUGIN_CAMERA_TYPE_WIDE = 0,
    // PLUGIN_CAMERA_TYPE_TELE,
    // PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE,

    enum DUAL_OPERATION_MODE syncTypeForNeedSwitchCamera = DUAL_OPERATION_MODE_MASTER;
    switch (needSwitchCamera) {
    case 0: // PLUGIN_CAMERA_TYPE_WIDE
        syncTypeForNeedSwitchCamera = DUAL_OPERATION_MODE_MASTER;
        break;
    case 1: // PLUGIN_CAMERA_TYPE_TELE
        syncTypeForNeedSwitchCamera = DUAL_OPERATION_MODE_SLAVE;
        break;
    case 2: // PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE
        syncTypeForNeedSwitchCamera = DUAL_OPERATION_MODE_SYNC;
        break;
    default:
        CLOGE("Invalid needSwitchCamera(%d). so, set DUAL_OPERATION_MODE_MASTER",
            needSwitchCamera);
        syncTypeForNeedSwitchCamera = DUAL_OPERATION_MODE_MASTER;
        break;
    }

    // Master Camera setting : to set master for 3AA Algorithm
    int masterCameraArc2HAL = (Data_int32_t)(*map)[PLUGIN_MASTER_CAMERA_ARC2HAL];

    // /ArcsoftFusionZoomPreview.h
    //#define RECOMMEND_DEFAULT(0x00)
    //#define RECOMMEND_TO_TELE(0x10)
    //#define RECOMMEND_TO_FB  (0x20)
    //#define RECOMMEND_TO_WIDE(0x30)

    enum DUAL_OPERATION_MODE master3a = DUAL_OPERATION_MODE_MASTER;

    CLOGV("cameraIndex : %d", cameraIndex);

    if (masterCameraArc2HAL == 0x00) {
        master3a = DUAL_OPERATION_MODE_MASTER;
    } else if (masterCameraArc2HAL == 0x10) {
        master3a = DUAL_OPERATION_MODE_SLAVE;
    } else if (masterCameraArc2HAL == 0x20) {
        master3a = DUAL_OPERATION_MODE_MASTER;
    } else if (masterCameraArc2HAL == 0x30) {
        master3a = DUAL_OPERATION_MODE_MASTER;
    } else {
        CLOGE("Invalid masterCameraArc2HAL(0x%x). so, set DUAL_OPERATION_MODE_MASTER", masterCameraArc2HAL);

        master3a = DUAL_OPERATION_MODE_MASTER;
    }

    CLOGV("m_convertMap2Parameters [F%d] masterCameraArc2HAL(0x%x) master3a(%d)",
        frame->getFrameCount(), masterCameraArc2HAL, master3a);

    // View Ratio
    float viewRatio = (Data_float_t)(*map)[PLUGIN_VIEW_RATIO];
    configurations->setArcSoftViewRatio(viewRatio);

    // Image Ratio
    float *imageRatio = (Pointer_float_t)(*map)[PLUGIN_IMAGE_RATIO];
    wideParameters->setArcSoftImageRatio(imageRatio[0]);
    teleParameters->setArcSoftImageRatio(imageRatio[1]);

    int imageShiftX = (Data_int32_t)(*map)[PLUGIN_IMAGE_SHIFT_X];
    int imageShiftY = (Data_int32_t)(*map)[PLUGIN_IMAGE_SHIFT_Y];
    configurations->setArcSoftImageShiftXY(imageShiftX, imageShiftY);

    {
        static enum DUAL_OPERATION_MODE prevOutputSyncType = DUAL_OPERATION_MODE_NONE;
        static float prevViewRatio = 0.0F;
        if (prevOutputSyncType != syncTypeForNeedSwitchCamera || prevViewRatio != viewRatio) {
            CLOGD("library output syncType(%d ->%d) viewRatio(%f)", prevOutputSyncType, syncTypeForNeedSwitchCamera, viewRatio);
            prevOutputSyncType = syncTypeForNeedSwitchCamera;
            prevViewRatio = viewRatio;
        }
    }

    enum DUAL_OPERATION_MODE reprocessingSyncType = syncTypeForNeedSwitchCamera;
    if (reprocessingSyncType == DUAL_OPERATION_MODE_SYNC) {
        if (viewRatio >= DUAL_CAPTURE_BYPASS_MODE_MAX_VIEW_RATIO)
            reprocessingSyncType = DUAL_OPERATION_MODE_SLAVE;
        else
            /* don't support sync mode in reprocessing */
            reprocessingSyncType = DUAL_OPERATION_MODE_MASTER;
    }

    if (m_fallback == true) {
        switch (syncTypeForNeedSwitchCamera) {
        case DUAL_OPERATION_MODE_MASTER:
            reprocessingSyncType = DUAL_OPERATION_MODE_MASTER;
            break;
        case DUAL_OPERATION_MODE_SYNC:
            reprocessingSyncType = DUAL_OPERATION_MODE_MASTER;
            break;
        case DUAL_OPERATION_MODE_SLAVE:
            CLOGW("Invalid state : preview DUAL_OPERATION_MODE_SLAVE. so, capture can not make DUAL_OPERATION_MODE_MASTER");
            break;
        }
    }

    struct camera2_shot_ext metaData;
    unsigned int *fd;
    int numOfDetectedFaces = 0;

    frame->getDynamicMeta(&metaData);
    fd = (unsigned int *)(*map)[PLUGIN_RESULT_FACE_RECT];
    numOfDetectedFaces =(Map_data_t)(*map)[PLUGIN_FACE_NUM];
    memcpy(metaData.shot.dm.stats.faceRectangles, fd, sizeof(int) * numOfDetectedFaces * 4);
    frame->setDynamicMeta(&metaData);

    // HAL1
    //wideParameters->updateDualCameraSyncTypeVendor(syncTypeForNeedSwitchCamera, reprocessingSyncType, master3a, viewRatio);
    //teleParameters->updateDualCameraSyncTypeVendor(syncTypeForNeedSwitchCamera, reprocessingSyncType, master3a, viewRatio);

    // HAL3
    configurations->setTryDualOperationMode(syncTypeForNeedSwitchCamera);
    configurations->setTryDualOperationModeReprocessing(reprocessingSyncType);
    configurations->setDualCameraMaster3a(master3a);
    CLOGV("set dual operation on preview(%d), reprocessing(%d), master3a(%d)", syncTypeForNeedSwitchCamera, reprocessingSyncType, master3a);

    return ret;
}


}; /* namespace android */
