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
 * \file      ExynosCameraPPUniPluginSWVdis_3_0.h
 * \brief     header file for ExynosCameraPPUniPluginSWVdis
 * \author    Byungho, Ahn(bh1212.ahn@samsung.com)
 * \date      2017/05/18
 *
 * <b>Revision History: </b>
 * - 2017/05/18 : Byungho, Ahn(bh1212.ahn@samsung.com) \n
 *   Initial version
 */

#ifndef EXYNOS_CAMERA_PP_UNIPLUGIN_SWVdis_H
#define EXYNOS_CAMERA_PP_UNIPLUGIN_SWVdis_H

#include "ExynosCameraDefine.h"
#include "ExynosCameraPPUniPlugin.h"

using namespace android;

#define NUM_VDIS_BUFFERS 27
#define NUM_VDIS_PREVIEW_INFO_BUFFERS 16
#define NUM_VDIS_PREVIEW_DELAY 2
#define NUM_VDIS_OUT_BUFFERS_MAX FPS60_NUM_REQUEST_VIDEO_BUFFER

enum vdis_status {
    VDIS_DEINIT              = 0,
    VDIS_IDLE,
    VDIS_RUN,
};

typedef struct swVDIS_FaceRect {
    int left;
    int top;
    int right;
    int bottom;
} swVDIS_FaceRect_t;

typedef struct swVDIS_FaceData {
    swVDIS_FaceRect_t FaceRect[NUM_OF_DETECTED_FACES];
    int FaceNum;
} swVDIS_FaceData_t;

typedef struct swVDIS_Offset {
    int left;
    int top;
    long long int timeStamp;
} swVDIS_Offset_t;

typedef struct swVDIS_Preview {
    int count;
    int get_offset_cnt;
    int store_offset_cnt;
    int prev_left;
    int prev_top;
} swVDIS_Preview_t;

/*
 * Class ExynosCameraPPUniPluginSWVdis
 */
class ExynosCameraPPUniPluginSWVdis : public ExynosCameraPPUniPlugin
{
protected:
    ExynosCameraPPUniPluginSWVdis()
    {
        m_init();
    }

    ExynosCameraPPUniPluginSWVdis(
            int cameraId,
            ExynosCameraConfigurations *configurations,
            ExynosCameraParameters *parameters,
            int nodeNum) : ExynosCameraPPUniPlugin(cameraId, configurations, parameters, nodeNum)
    {
        strncpy(m_name, "ExynosCameraPPUniPluginSWVdis",  EXYNOS_CAMERA_NAME_STR_SIZE - 1);

        m_init();
    }

    /* ExynosCameraPPUniPluginSWVdis's constructor is protected
     * to prevent new without ExynosCameraPPFactory::newPP()
     */
    friend class ExynosCameraPPFactory;

public:
    virtual ~ExynosCameraPPUniPluginSWVdis();
    status_t    start(void);
    status_t    stop(bool suspendFlag = false);

protected:
    virtual status_t m_draw(ExynosCameraImage *srcImage,
                            ExynosCameraImage *dstImage,
                            ExynosCameraParameters *params);

private:
    void                m_init(void);
    status_t            m_UniPluginInit(void);
    int                 m_VDISstatus;
    void                process(ExynosCameraImage *srcImage,
                                    ExynosCameraImage *dstImage,
                                    ExynosCameraParameters *params);
    void                getPreviewVDISInfo(void);
    status_t            getVideoOutInfo(ExynosCameraImage *image);
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    bool                isSWVdisUHD(void);
    int                 setSWVdisOISCoef(uint32_t coef);
    int                 getSWVdisOISGain(int exposureTime);
    void                setSWVdisOISFadeVal(int oisFadeVal);
    void                m_initSWVdisOISParam(void);
    status_t            m_extControl(int controlType, void *data);
#endif

    int                         m_refCount;
    UTrect                      m_swVDIS_rectInfo;
    UniPluginBufferData_t       m_swVDIS_pluginData;
    UniPluginBufferData_t       m_swVDIS_outBufInfo;
    UniPluginExtraBufferInfo_t  m_swVDIS_pluginExtraBufferInfo;
    UniPluginFPS_t              m_swVDIS_Fps;
#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
    UniPluginDualInfo_t         m_swVDIS_dualInfo;
#endif
    mutable Mutex               m_uniPluginLock;
#ifdef SAMSUNG_SW_VDIS_USE_OIS
    struct swVdisExtCtrlInfo    m_SWVdisExtCtrlInfo;
    int                         m_SWVdisOisFadeVal;
    int                         m_SWVdisOisGainFrameCount[2];
#endif
};

#endif //EXYNOS_CAMERA_PP_UNIPLUGIN_SWVdis_H
