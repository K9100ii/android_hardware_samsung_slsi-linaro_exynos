/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef SEC_CAMERA_SW_VDIS_IMP_H
#define SEC_CAMERA_SW_VDIS_IMP_H

#include "uni_plugin_wrapper.h"

#include <utils/Timers.h>
#include <ion/ion.h>
#include <log/log.h>
#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraBufferManager.h"

#define NUM_VDIS_BUFFERS 27
#define NUM_VDIS_PREVIEW_INFO_BUFFERS 16
#define NUM_VDIS_PREVIEW_DELAY 2

#define NUM_VDIS_OUT_BUFFERS_MAX NUM_REQUEST_VIDEO_BUFFER

namespace android {

#ifdef SUPPORT_SW_VDIS

class ExynosCameraFrame;
class ExynosCameraBuffer;
class ExynosCameraBufferManager;

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

extern int m_swVDIS_OutBufIndex[NUM_VDIS_OUT_BUFFERS_MAX];
extern bool m_swVDIS_Mode;
extern long long int m_swVDIS_timeStamp[NUM_VDIS_BUFFERS];
extern int m_swVDIS_InW, m_swVDIS_InH;
extern int m_swVDIS_InQIndex, m_swVDIS_OutQIndex;
extern int m_swVDIS_CameraID;
extern int m_swVDIS_SensorType;
extern int m_swVDIS_FrameNum;
extern int m_swVDIS_Delay;
extern int m_swVDIS_APType;
extern ExynosCameraBuffer m_swVDIS_OutBuf[NUM_VDIS_OUT_BUFFERS_MAX];
extern int m_sw_VDIS_OutBufferNum;
extern int m_swVDIS_TSIndex;
extern int m_swVDIS_TSIndexCount;
extern int m_swVDIS_NextOutBufIdx;
extern int m_swVDIS_DeviceOrientation;
extern struct camera2_dm *m_swVDIS_fd_dm;
extern swVDIS_FaceData_t *m_swVDIS_FaceData;
#ifdef SAMSUNG_OIS_VDIS
extern UNI_PLUGIN_OIS_MODE m_swVDIS_OISMode;
extern uint32_t m_swVDIS_OISGain;
extern bool m_swVDIS_UHD;
#endif
extern uint32_t m_swVDIS_MinFps, m_swVDIS_MaxFps;
#ifdef SAMSUNG_UNIPLUGIN
extern UTrect m_swVDIS_RectInfo;
extern UniPluginBufferData_t m_swVDIS_pluginData_tstamp;
#endif

extern int m_swVDIS_AdjustPreviewSize(int *Width, int *Height);
extern int m_swVDIS_init();
extern int m_swVDIS_process(ExynosCameraBuffer *vs_InputBuffer, int swVdis_InputIndexCount,
                            nsecs_t swVDIS_timeStamp,nsecs_t swVDIS_timeStampBoot, int swVDIS_Lux, int swVDIS_ZoomLevel,
                            float swVDIS_ExposureValue, int oisGain, swVDIS_FaceData_t *FaceData, int swVDIS_ExposureTime);
extern int m_swVDIS_StoreVDISOutInfo();
extern int m_swVDIS_GetOffset(int *offset_x, int *offset_y, long long int displayTimeStamp);
extern int m_swVDIS_deinit();
#ifdef SAMSUNG_OIS_VDIS
extern UNI_PLUGIN_OIS_MODE m_swVDIS_getOISmode();
#endif
#endif /*SUPPORT_SW_VDIS*/

}; /* namespace android */

#endif
