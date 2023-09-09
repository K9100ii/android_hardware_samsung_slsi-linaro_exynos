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
#include <cutils/log.h>
#include "ExynosCameraConfig.h"
#include "ExynosCameraBufferManager.h"

#define NUM_VDIS_BUFFERS 21

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

extern bool m_swVDIS_Mode;
extern ExynosCameraBufferManager *m_swVDIS_BufferMgr;
extern long long int m_swVDIS_timeStamp[NUM_VDIS_BUFFERS];
extern int m_swVDIS_InW, m_swVDIS_InH;
extern int m_swVDIS_InQIndex, m_swVDIS_OutQIndex;
extern int m_swVDIS_CameraID;
extern int m_swVDIS_SensorType;
extern int m_swVDIS_FrameNum;
extern ExynosCameraBuffer m_swVDIS_OutBuf[NUM_VDIS_BUFFERS];
extern struct camera2_dm *m_swVDIS_fd_dm;
extern swVDIS_FaceData_t *m_swVDIS_FaceData;
#ifdef SAMSUNG_OIS_VDIS
extern UNI_PLUGIN_OIS_MODE m_swVDIS_OISMode;
#endif

extern int m_swVDIS_AdjustPreviewSize(int *Width, int *Height);
extern int m_swVDIS_init();
extern int m_swVDIS_process(ExynosCameraBuffer *vs_InputBuffer, int swVdis_InputIndexCount,
	nsecs_t swVDIS_timeStamp,nsecs_t swVDIS_timeStampBoot, int swVDIS_Lux, int swVDIS_ZoomLevel,
	float swVDIS_ExposureValue, swVDIS_FaceData_t *FaceData);
extern int m_swVDIS_deinit();
#ifdef SAMSUNG_OIS_VDIS
extern UNI_PLUGIN_OIS_MODE m_swVDIS_getOISmode();
#endif
#endif /*SUPPORT_SW_VDIS*/

}; /* namespace android */

#endif
