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
* \file      ArcsoftFusionBokehPreview.h
* \brief     header file for ArcsoftFusionBokehPreview
* \author    Sangwoo, Park(sw5771.park@samsung.com)
* \date      2017/11/09
*
* <b>Revision History: </b>
* - 2017/11/0 : Sangwoo, Park(sw5771.park@samsung.com) \n
*   Initial version
*/

#ifndef ARCSOFT_FUSION_BOKEH_PREVIEW_H
#define ARCSOFT_FUSION_BOKEH_PREVIEW_H

#include <utils/Log.h>

#include "ArcsoftFusion.h"

#include "arcsoft_dualcam_video_refocus.h"

using namespace android;

//#define ARC_BOKEH_DEBUG
#define BOKEH_PROCESSTIME_STANDARD (34000)

#ifdef ARC_BOKEH_DEBUG
#define ARC_BOKEH_LOGD ALOGD
#else
#define ARC_BOKEH_LOGD ALOGV
#endif


#define RTB_DUAL_CAM_DEGREE 0
#define RTB_AUX_CAM_CACHE_NUM 4
#define RTB_MAIN_META_CACHE_NUM 4
#define ARC_DEBUG_DUMP_NAME "/data/misc/cameraserver/"
#define PROPERTY_VALUE_MAX 100


enum rtb_af_status_t {
    _AF_RTB_STATUS_INVALID,
    _AF_RTB_STATUS_VALID,
};

typedef struct _rtb_input_params_t {
    MPOINT focus;
    ASVLOFFSCREEN mainimg;
    ASVLOFFSCREEN auximg;
    ASVLOFFSCREEN output;
    ARC_REFOCUSCAMERAIMAGE_PARAM caminfo;
    rtb_af_status_t af_status;
} rtb_frame_params_t;

//! ArcsoftFusionBokehPreview
/*!
* \ingroup ExynosCamera
*/
class ArcsoftFusionBokehPreview : public ArcsoftFusion {
public:
    //! Constructor
    ArcsoftFusionBokehPreview();
    //! Destructor
    virtual ~ArcsoftFusionBokehPreview();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_execute(Map_t *map);
    virtual status_t m_init(Map_t *map);
    virtual status_t m_uninit(Map_t *map);
    virtual status_t m_query(Map_t *map);

private:
    void getFrameParams(Map_t *map);
    int32_t doArcRTBInit();
    int32_t doArcRTBUninit();
    int32_t doArcRTBProcess();
    void dumpDatatoFile(MVoid *pData, int size, const char* name);
    void dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, const char* name_prefix, const int64_t name_timestamp);
    void dumpFrameParams(rtb_frame_params_t& p);
    nsecs_t getFrameTimeStamp(void *preview_frame);

private:
    static void  *m_handle;
    static Mutex  m_handleLock;

    const void *m_pCaps;
    rtb_frame_params_t m_inParams;
    int32_t m_framelength;
    int32_t g_arc_dump_frame_idx;
    int32_t g_arc_dump_gap_frame_idx;
};

#endif //ARCSOFT_FUSION_BOKEH_PREVIEW_H
