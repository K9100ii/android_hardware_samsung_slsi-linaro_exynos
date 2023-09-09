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

#ifndef SEC_CAMERA_HYPERMOTION_IMP_H
#define SEC_CAMERA_HYPERMOTION_IMP_H

#include "uni_plugin_wrapper.h"

#include <utils/Timers.h>
#include <ion/ion.h>
#include <cutils/log.h>
#include "ExynosCameraConfig.h"
#include "ExynosCameraBufferManager.h"

#define NUM_HYPERMOTION_BUFFERS 21
#define ABLE_FACECNT 10
namespace android {

#ifdef SAMSUNG_HYPER_MOTION

class ExynosCameraFrame;
class ExynosCameraBuffer;
class ExynosCameraBufferManager;

typedef struct swHM_FaceRect {
    int left;
    int top;
    int right;
    int bottom;
} swHM_FaceRect_t;

typedef struct swHM_FaceData {
    swHM_FaceRect_t FaceRect[ABLE_FACECNT];
    int FaceNum;
} swHM_FaceData_t;


extern struct camera2_dm *m_swHM_fd_dm;
extern swHM_FaceData_t *m_swHM_FaceData;
extern bool m_hyperMotion_Mode;
extern long long int m_hyperMotion_timeStamp[NUM_HYPERMOTION_BUFFERS];
extern int m_hyperMotion_InW, m_hyperMotion_InH;
extern int m_hyperMotion_InQIndex, m_hyperMotion_OutQIndex;
extern int m_hyperMotion_CameraID;
extern int m_hyperMotion_SensorType;
extern int m_hyperMotion_APType;
extern int m_hyperMotion_FrameNum;
extern int m_hyperMotion_Delay;


extern ExynosCameraBuffer m_hyperMotion_OutBuf[NUM_HYPERMOTION_BUFFERS];

extern int m_hyperMotion_AdjustPreviewSize(int *Width, int *Height);
extern int m_hyperMotion_init();
extern int m_hyperMotion_process(ExynosCameraBuffer *vs_InputBuffer, int hyperMotion_InputIndexCount,
                            nsecs_t hyperMotion_timeStamp,nsecs_t hyperMotion_timeStampBoot, int hyperMotion_Lux, int hyperMotion_ZoomLevel,
                            float hyperMotion_ExposureValue, int hyperMotion_ExposureTime, swHM_FaceData_t *FaceData, int orientation);
extern int m_hyperMotion_deinit();
extern void m_hyperMotionModeSet(bool mode);
extern void m_hyperMotionPlaySpeedSet(UNI_PLUGIN_OPERATION_MODE playspeed);
extern bool m_hyperMotionModeGet();
extern bool m_hyperMotionCanSendFrame();
extern nsecs_t m_hyperMotionTimeStampGet();
#endif /*SAMSUNG_HYPER_MOTION*/
}; /* namespace android */
#endif
