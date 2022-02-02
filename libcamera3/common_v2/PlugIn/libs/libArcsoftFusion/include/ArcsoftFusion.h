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
 * \file      ArcsoftFusion.h
 * \brief     header file for ArcsoftFusion
 * \author    Sangwoo, Park(sw5771.park@samsung.com)
 * \date      2017/11/09
 *
 * <b>Revision History: </b>
 * - 2017/11/0 : Sangwoo, Park(sw5771.park@samsung.com) \n
 *   Initial version
 */

#ifndef ARCSOFT_FUSION_H
#define ARCSOFT_FUSION_H

#include <sys/mman.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <utils/Timers.h>
#include <ion/ion.h>
#include "string.h"

#include "exynos_format.h"
#include "csc.h"
#include "acryl.h"

#include "PlugInCommon.h"

#include "amcomdef.h"
#include "asvloffscreen.h"
#include "arcsoft_dualcam_optical_zoom_common.h"
#include "arcsoft_dualcam_image_optical_zoom.h"
#include "arcsoft_dualcam_common_refocus.h"

using namespace android;

#define ARCSOFT_FUSION_EMULATOR
#define ARCSOFT_FUSION_EMULATOR_CAPTURE_FULLSIZE

#ifdef ARCSOFT_FUSION_EMULATOR
#define ARCSOFT_FUSION_WRAPPER
#else
//#define ARCSOFT_FUSION_WRAPPER
#endif

#define ARCSOFT_FUSION_EMULATOR_WIDE_WIDTH      (4032)
#define ARCSOFT_FUSION_EMULATOR_WIDE_HEIGHT     (3024)

#define ARCSOFT_FUSION_EMULATOR_TELE_WIDTH      (4032)
#define ARCSOFT_FUSION_EMULATOR_TELE_HEIGHT     (3024)

#define ARCSOFT_FUSION_EMULATOR_CAPTURE_WIDTH   (4032)
#define ARCSOFT_FUSION_EMULATOR_CAPTURE_HEIGHT  (3024)

#define ARCSOFT_FUSION_EMULATOR_PREVIEW_WIDTH   (1440)
#define ARCSOFT_FUSION_EMULATOR_PREVIEW_HEIGHT  (1080)

#define ARCSOFT_FUSION_EMULATOR_VIEW_RATIO (1.0f)


#define ARCSOFT_FUSION_EMULATOR_WIDE_OPEN (1)
#define ARCSOFT_FUSION_EMULATOR_TELE_OPEN (1)

//#define ARCSOFT_FUSION_USE_G2D

#define ARCSOFT_FUSION_DEBUG
#define FUSION_PROCESSTIME_STANDARD (34000)

#define ARCSOFT_FUSION_CAPTURE_ZOOM_MIN           (1.50)
#define ARCSOFT_FUSION_CAPTURE_ZOOM_MAX           (2.00)

#define CALIB_DATA_ARCSOFT_SIZE 2048


#ifdef ARCSOFT_FUSION_DEBUG
#define ARCSOFT_FUSION_LOGD ALOGD
#else
#define ARCSOFT_FUSION_LOGD ALOGV
#endif

//! ArcsoftFusion
/*!
 * \ingroup ExynosCamera
 */
class ArcsoftFusion
{
public:
    //! Constructor
    ArcsoftFusion();
    //! Destructor
    virtual ~ArcsoftFusion();

    //! create
    virtual status_t create(void) final;

    //! destroy
    virtual status_t destroy(void) final;

    //! execute
    virtual status_t execute(Map_t *map) final;

    //! init
    virtual status_t init(Map_t *map) final;

    //! uninit
    virtual status_t uninit(Map_t *map) final;

    //! query
    virtual status_t query(Map_t *map);

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_execute(Map_t *map);
    virtual status_t m_init(Map_t *map);
    virtual status_t m_uninit(Map_t *map);
    virtual status_t m_query(Map_t *map);

protected:
    void             m_init_ARC_DCOZ_INITPARAM(ARC_DCOZ_INITPARAM *initParam);

    void             m_checkExcuteStartTime(void);
    void             m_checkExcuteStopTime(bool flagLog = false);
    void             m_copyHalfHalf2Dst(Map_t *map);
    void             m_copyMetaData(char *srcMetaAddr, int srcMetaSize, char *dstMetaAddr, int dstMetaSize);
    void             m_loadCalData(void);

    status_t         m_allocIonBuf(int size, int *fd, char **addr, bool mapNeeded);
    status_t         m_freeIonBuf(int size, int *fd, char **addr);
    void             m_destroyIon(void);

    status_t         m_createScaler(void);
    void             m_destroyScaler(void);
    status_t         m_drawScaler(int *srcRect, int srcFd, int srcBufSize,
                                  int *dstRect, int dstFd, int dstBufSize);

    void             m_saveValue(Map_t *map);

private:
    status_t         m_createG2D(void);
    void             m_destroyG2D(void);
    status_t         m_drawG2D(int *srcRect, int srcFd, int srcBufSize,
                               int *dstRect, int dstFd, int dstBufSize);

    status_t         m_createCsc(void);
    void             m_destroyCsc(void);
    status_t         m_drawCsc(int *srcRect, int srcFd, int srcBufSize,
                               int *dstRect, int dstFd, int dstBufSize);

protected:
    static MByte      m_calData[ARC_DCOZ_CALIBRATIONDATA_LEN];
    static bool       m_flagCalDataLoaded;

    nsecs_t           m_excuteStartTime;
    nsecs_t           m_excuteStopTime;

    int               m_emulationProcessTime;
    float             m_emulationCopyRatio;

    int               m_syncType;

    // src
    int               m_mapSrcBufCnt;
    Array_buf_t       m_mapSrcBufPlaneCnt;
    Array_buf_plane_t m_mapSrcBufSize;
    Array_buf_rect_t  m_mapSrcRect;
    Array_buf_t       m_mapSrcV4L2Format;
    Array_buf_addr_t  m_mapSrcBufAddr[2];
    Array_buf_plane_t m_mapSrcBufFd;
    Array_buf_t       m_mapSrcBufWStride;

    Array_buf_t       m_mapSrcAfStatus;
    Pointer_float_t   m_mapSrcZoomRatio;

    Array_buf_t       m_mapSrcZoomRatioListSize;
    Array_float_addr_t  m_mapSrcZoomRatioList;
    float            *m_zoomRatioList[2];
    int               m_zoomMaxSize;
    int               m_wideFullsizeW;
    int               m_wideFullsizeH;
    int               m_teleFullsizeW;
    int               m_teleFullsizeH;

    // dst
    int               m_mapDstBufCnt;
    Array_buf_t       m_mapDstBufPlaneCnt;
    Array_buf_plane_t m_mapDstBufSize;
    Array_buf_rect_t  m_mapDstRect;
    Array_buf_t       m_mapDstV4L2Format;
    Array_buf_addr_t  m_mapDstBufAddr;
    Array_buf_plane_t m_mapDstBufFd;
    Array_buf_t       m_mapDstBufWStride;

    uint8_t           m_bokehIntensity;
    Array_buf_t       m_iso;

    MRECT             m_resultRect[16];

    // about ion alloc
    int               m_ionClient;

    int               m_tempSrcBufferSize[2];
    int               m_tempSrcBufferFd[2];
    char             *m_tempSrcBufferAddr[2];

    void             *m_csc;
    Acrylic          *m_acylic;
    AcrylicLayer     *m_layer;

    int               m_cameraIndex;
    int               m_needSwitchCamera;
    int               m_masterCameraArc2HAL;
    float             m_viewRatio;
    float             m_imageRatio[2];

    int               m_imageShiftX;
    int               m_imageShiftY;

    int               m_buttonZoomMode;
};

#endif //ARCSOFT_FUSION_H
