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

/*!
* \file      ArcsoftFusionZoomPreview.h
* \brief     header file for ArcsoftFusionZoomPreview
* \author    Sangwoo, Park(sw5771.park@samsung.com)
* \date      2017/11/09
*
* <b>Revision History: </b>
* - 2017/11/0 : Sangwoo, Park(sw5771.park@samsung.com) \n
*   Initial version
*/

#ifndef ARCSOFT_FUSION_ZOOM_PREVIEW_H
#define ARCSOFT_FUSION_ZOOM_PREVIEW_H

#include <utils/Log.h>

#include "ArcsoftFusion.h"

#include "arcsoft_dualcam_video_optical_zoom.h"

using namespace android;

#define RECOMMEND_DEFAULT (0x00)
#define RECOMMEND_TO_TELE (0x10)
#define RECOMMEND_TO_FB (0x20)
#define RECOMMEND_TO_WIDE (0x30)


typedef struct{
    MInt32 i32WideZoomListSize;
    MFloat *pfWideZoomList;
    MInt32 i32TeleZoomListSize;
    MFloat *pfTeleZoomList;
}Plugin_DCOZ_ZoomlistParam_t;

typedef enum{
    PLUGIN_CAMERA_TYPE_WIDE = 0,
    PLUGIN_CAMERA_TYPE_TELE,
    PLUGIN_CAMERA_TYPE_BOTH_WIDE_TELE,
}Plugin_DCOZ_CameraType;

typedef struct{
    MInt32 CameraIndex;
    MInt32 ImageShiftX;
    MInt32 ImageShiftY;
    Plugin_DCOZ_CameraType NeedSwitchCamera;
    MInt32 SwitchTo;
}Plugin_DCOZ_CameraParam_t;

//! ArcsoftFusionZoomPreview
/*!
* \ingroup ExynosCamera
*/
class ArcsoftFusionZoomPreview : public ArcsoftFusion {
public:
    //! Constructor
    ArcsoftFusionZoomPreview();
    //! Destructor
    virtual ~ArcsoftFusionZoomPreview();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_execute(Map_t *map);
    virtual status_t m_init(Map_t *map);
    virtual status_t m_uninit(Map_t *map);
    virtual status_t m_query(Map_t *map);

    status_t m_initLib(Map_t *map);
    status_t m_uninitLib(void);
    status_t m_setPreviewSize(Map_t *map);

    status_t m_setZoomList(Map_t *map);

    status_t m_setCameraControlFlag(Map_t *map);
    status_t m_getCameraControlFlag(Map_t *map);
    status_t getFrameParams(Map_t *map);
    status_t m_getDualFocusRect(Map_t *map);
    status_t m_getFaceResultRect(Map_t *map);
private:
    status_t m_InitZoomValue(Map_t *map);
    void dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, char* name_prefix);
    void OpenFileLog();
    void CloseFileLog();
    void WriteFileLog(const char *fmt, ...);

protected:
    ARC_DCVOZ_CAMERAIMAGEINFO m_cameraImageInfo;
    ARC_DCVOZ_CAMERAPARAM m_cameraParam;

    ASVLOFFSCREEN m_pWideImg;
    ASVLOFFSCREEN m_pTeleImg;
    ASVLOFFSCREEN m_pDstImg;
    ARC_DCVOZ_RATIOPARAM m_pRatioParam;
    ARC_DCOZ_IMAGEFD m_pFdWideImg;
    ARC_DCOZ_IMAGEFD m_pFdTeleImg;
    ARC_DCOZ_IMAGEFD m_pFdResultImg;

private:
    bool m_bInit;

    static MHandle m_handle;
    static Mutex   m_handleLock;

    float* m_mapOutImageRatioList[2];
    int*   m_mapOutNeedMarginList[2];
};

#endif //ARCSOFT_FUSION_ZOOM_PREVIEW_H
