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

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "SecCameraSWVDIS"
#include <log/log.h>


#include "SecCameraSWVdis.h"

#include "ExynosCameraBufferManager.h"

namespace android {

#ifdef SUPPORT_SW_VDIS
bool m_swVDIS_Mode;
void *m_swVDIS_Handle;
ExynosCameraBufferManager *m_swVDIS_BufferMgr;
ExynosCameraBuffer *m_swVDIS_OutputBuffer[NUM_PREVIEW_BUFFERS];
int m_swVDIS_InputIndex[NUM_PREVIEW_BUFFERS];
int m_swVDIS_OutputIndex[NUM_PREVIEW_BUFFERS];
int m_swVDIS_OutW, m_swVDIS_OutH, m_swVDIS_InW, m_swVDIS_InH;
int m_swVDIS_FrameNum, m_swVDIS_Delay;
int m_swVDIS_Count, m_swVDIS_InputQIndex;
int m_swVDIS_Ion_Client;

status_t m_swVDIS_AdjustPreviewSize(int *Width, int *Height)
{
#ifdef SW_VDIS_10pcMARGIN
    if(*Width == 2304 && *Height == 1296) {
        *Width  = 1920;
        *Height = 1080;
    }
    else if(*Width == 1536 && *Height == 864) {
        *Width  = 1280;
        *Height = 720;
    }
    else
#endif //SW_VDIS_10pcMARGIN

#ifdef SW_VDIS_6pcMARGIN
    if(*Width == 2176 && *Height == 1216) {
        *Width  = 1920;
        *Height = 1080;
    }
    else if(*Width == 1408 && *Height == 800) {
        *Width  = 1280;
        *Height = 720;
    }
    else
#endif //SW_VDIS_6pcMARGIN

#ifdef SW_VDIS_5pcMARGIN
    if(*Width == 2112 && *Height == 1188) {
        *Width  = 1920;
        *Height = 1080;
    }
    else if(*Width == 1408 && *Height == 800) {
        *Width  = 1280;
        *Height = 720;
    }
    else
#endif //SW_VDIS_5pcMARGIN

#ifdef SW_VDIS_4pcMARGIN
    if(*Width == 2080 && *Height == 1168) {
        *Width  = 1920;
        *Height = 1080;
    }
    else if(*Width == 1392 && *Height == 784) {
        *Width  = 1280;
        *Height = 720;
    }
    else
#endif //SW_VDIS_4pcMARGIN
    {
        *Width  = (*Width * 5) / 6;
        *Height = (*Height * 5) / 6;
    }
    return NO_ERROR;
}

status_t m_swVDIS_Init(int swVDIS_InW, int swVDIS_InH)
{
    int i;
    m_swVDIS_Mode		= true;
    m_swVDIS_Delay		= 2;
    m_swVDIS_FrameNum	= 0;
    m_swVDIS_Count		= 0;
    m_swVDIS_Handle		= NULL;

    int bRollingFlag, bTransFlag, bRotateFlag, swVDIS_BlankNum, swVDIS_Version;
    bRollingFlag = bTransFlag = bRotateFlag = 1;
    swVDIS_BlankNum = 0;
    swVDIS_Version	= 2;

    m_swVDIS_InW = swVDIS_InW;
    m_swVDIS_InH = swVDIS_InH;
    m_swVDIS_OutW = m_swVDIS_InW;
    m_swVDIS_OutH = m_swVDIS_InH;

    m_swVDIS_AdjustPreviewSize(&m_swVDIS_OutW, &m_swVDIS_OutH);

#if 1
    m_swVDIS_Handle = vsInit(m_swVDIS_InW, m_swVDIS_InH, m_swVDIS_OutW, m_swVDIS_OutH,
        bRollingFlag, bTransFlag, bRotateFlag, swVDIS_BlankNum,
        m_swVDIS_Delay, swVDIS_Version, SW_VDIS_MODEL);
#endif

    VDIS_LOG("VDIS_HAL_INIT: VS_INIT: Input: %d x %d  Output: %d x %d",
    m_swVDIS_InW, m_swVDIS_InH, m_swVDIS_OutW, m_swVDIS_OutH);

    m_swVDIS_Ion_Client = ion_open();
    if (m_swVDIS_Ion_Client < 0) {
        ALOGE("ERR(%s):m_swVDIS_Ion_Client() fail", __func__);
        m_swVDIS_Ion_Client = -1;
    }
    for(i=0;i<NUM_PREVIEW_BUFFERS;i++)
        m_swVDIS_OutputBuffer[i] = new ExynosCameraBuffer;

    return NO_ERROR;
}

status_t m_swVDIS_Execute(ExynosCameraBuffer *vs_InputBuffer, ExynosCameraBuffer *vs_OutputBuffer, nsecs_t swVDIS_timeStamp, int swVDIS_Lux)
{
    int swVDIS_OutputIndex, swVdis_InputIndexCount;
    swVdis_InputIndexCount = (m_swVDIS_FrameNum%NUM_PREVIEW_BUFFERS);

    m_swVDIS_InputIndex[swVdis_InputIndexCount] = vs_InputBuffer->index;
    swVDIS_OutputIndex = m_swVDIS_OutputIndex[swVdis_InputIndexCount];
    memcpy(vs_OutputBuffer, m_swVDIS_OutputBuffer[swVDIS_OutputIndex], sizeof(struct ExynosCameraBuffer));

    char *vdis_LibInputAddr[2], *vdis_LibOutputAddr[2];

    vdis_LibInputAddr[0]    = (char *)vs_InputBuffer->addr[0];
    vdis_LibInputAddr[1]    = (char *)vs_InputBuffer->addr[1];
    vdis_LibOutputAddr[0]   = (char *)vs_OutputBuffer->addr[0];
    vdis_LibOutputAddr[1]   = (char *)vs_OutputBuffer->addr[1];

#if 1
    if (swVDIS_Lux>20)
        swVDIS_Lux = -1*(16777216- swVDIS_Lux);     //2^24 = 16777216

    vsSetLux(m_swVDIS_Handle, swVDIS_Lux);

    //VDIS_1_0(m_swVDIS_Handle, vdis_LibInputAddr, vdis_LibOutputAddr);
    VDIS_2_0(m_swVDIS_Handle, vdis_LibInputAddr, vdis_LibOutputAddr, swVDIS_timeStamp);
#else
    int swVdis_MarginX = (m_swVDIS_InW-m_swVDIS_OutW)/2;
    int swVdis_MarginY = (m_swVDIS_InH-m_swVDIS_OutH)/2;

    for(int i=0; i<m_swVDIS_OutH; i++) {
        memcpy(vs_OutputBuffer->addr[0]+(i*m_swVDIS_OutW),
        vs_InputBuffer->addr[0]+swVdis_MarginX+((i+swVdis_MarginY)*m_swVDIS_InW), m_swVDIS_OutW);

        if(i<m_swVDIS_OutH/2)
            memcpy(vs_OutputBuffer->addr[1]+(i*m_swVDIS_OutW),
            vs_InputBuffer->addr[1]+swVdis_MarginX+((i+(swVdis_MarginY/2))*m_swVDIS_InW), m_swVDIS_OutW);
    }
#endif

    swVDIS_OutputIndex = (swVdis_InputIndexCount>1) ? m_swVDIS_OutputIndex[swVdis_InputIndexCount-2] : m_swVDIS_OutputIndex[swVdis_InputIndexCount+NUM_PREVIEW_BUFFERS-2];
    m_swVDIS_InputQIndex = (swVdis_InputIndexCount>1) ? m_swVDIS_InputIndex[swVdis_InputIndexCount-2] : m_swVDIS_InputIndex[swVdis_InputIndexCount+NUM_PREVIEW_BUFFERS-2];
    memcpy(vs_OutputBuffer, m_swVDIS_OutputBuffer[swVDIS_OutputIndex], sizeof(struct ExynosCameraBuffer));

    if (m_swVDIS_Ion_Client >= 0) {
        for (int i = 0; i < 2; i++) {
            if (ion_sync_fd(m_swVDIS_Ion_Client, vs_OutputBuffer->fd[i]))
                ALOGE("ERR(%s):ion_sync(): m_swVDIS_Ion_Client fail [%d]", __func__, i);
        }
    }

    m_swVDIS_FrameNum++;

    return NO_ERROR;
}

status_t m_swVDIS_Release()
{
    if(m_swVDIS_Handle)
        vsParamFree(m_swVDIS_Handle);

    m_swVDIS_Handle = NULL;
    m_swVDIS_Mode	= false;
    m_swVDIS_FrameNum    = 0;

    if (0 < m_swVDIS_Ion_Client)
        ion_close(m_swVDIS_Ion_Client);
    m_swVDIS_Ion_Client = -1;

    VDIS_LOG("VDIS_HAL: VS_RELEASE: vsParamFree done");

    return NO_ERROR;
}

#endif

}; /* namespace android */
