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

#include <utils/Timers.h>
#include <ion/ion.h>
#include "ExynosCameraCommonInclude.h"
#include "vdis_interface.h"

namespace android {

#ifdef SUPPORT_SW_VDIS

class ExynosCameraFrame;
class ExynosCameraBuffer;
class ExynosCameraBufferManager;

extern bool m_swVDIS_Mode;
extern void *m_swVDIS_Handle;
extern ExynosCameraBufferManager *m_swVDIS_BufferMgr;
extern ExynosCameraBuffer *m_swVDIS_OutputBuffer[NUM_PREVIEW_BUFFERS];
extern int m_swVDIS_InputIndex[NUM_PREVIEW_BUFFERS];
extern int m_swVDIS_OutputIndex[NUM_PREVIEW_BUFFERS];
extern int m_swVDIS_OutW, m_swVDIS_OutH, m_swVDIS_InW, m_swVDIS_InH;
extern int m_swVDIS_FrameNum, m_swVDIS_Delay;
extern int m_swVDIS_Ion_Client;
extern int m_swVDIS_Count, m_swVDIS_InputQIndex;

extern int m_swVDIS_AdjustPreviewSize(int *Width, int *Height);
extern int m_swVDIS_SetHandle(void *Handle);
extern int* m_swVDIS_GetHandle();
extern int m_swVDIS_Init(int swVDIS_InW, int swVDIS_InH);
extern int m_swVDIS_Execute(ExynosCameraBuffer *vs_InputBuffer, ExynosCameraBuffer *vs_OutputBuffer, nsecs_t swVDIS_timeStamp, int swVDIS_Lux);
extern int m_swVDIS_Release(void);

#endif

}; /* namespace android */

#endif
