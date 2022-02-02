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
* \file      ArcsoftFusionBokehCapture.h
* \brief     header file for ArcsoftFusionBokehCapture
* \author    Sangwoo, Park(sw5771.park@samsung.com)
* \date      2017/11/09
*
* <b>Revision History: </b>
* - 2017/11/0 : Sangwoo, Park(sw5771.park@samsung.com) \n
*   Initial version
*/

#ifndef ARCSOFT_FUSION_BOKEH_CAPTURE_H
#define ARCSOFT_FUSION_BOKEH_CAPTURE_H

#include <utils/Log.h>

#include "ArcsoftFusion.h"

#include "arcsoft_dualcam_image_refocus.h"
#include "arcsoft_dualcam_image_refocus_dll.h"

using namespace android;

#define ARC_BOKEHPP_DEBUG
#define BOKEHPP_PROCESSTIME_STANDARD (34000)

#define LIB_PATH_LENGTH 100
#define LIB_PATH_BOKEH "/system/lib/libarcsoft_dualcam_refocus.so"
#define BOKEH_KERNELBIN_BLOCKSIZE 4096
#define LIB_PATH_BOKEH_KERNELBIN "data/misc/cameraserver/kernel.bin"
#define BOKEH_MAX_FACE_NUM 16
#define ARC_DEBUG_DUMP_NAME "/data/misc/cameraserver/"

#ifdef ARC_BOKEHPP_DEBUG
#define ARC_BOKEHPP_LOGD ALOGD
#else
#define ARC_BOKEHPP_LOGD ALOGV
#endif

enum halBokehPPInputType {
    BOKEH_WIDE_INPUT = 0,
    BOKEH_TELE_INPUT = 1
};

enum bokeh_af_status_t {
    _AF_STATUS_VALID,
    _AF_STATUS_INVALID
};

typedef struct _bokeh_input_params_t {
    MPOINT focus;
    ASVLOFFSCREEN teleimg;
    ASVLOFFSCREEN wideimg;
    ARC_DCIR_PARAM faceparam;
    ASVLOFFSCREEN output;
    ARC_REFOCUSCAMERAIMAGE_PARAM caminfo;

    bokeh_af_status_t af_status;
} bokeh_frame_params_t;

//! ArcsoftFusionBokehCapture
/*!
* \ingroup ExynosCamera
*/
class ArcsoftFusionBokehCapture : public ArcsoftFusion {
public:
    //! Constructor
    ArcsoftFusionBokehCapture();
    //! Destructor
    virtual ~ArcsoftFusionBokehCapture();

protected:
    virtual status_t m_create(void);
    virtual status_t m_destroy(void);
    virtual status_t m_execute(Map_t *map);
    virtual status_t m_init(Map_t *map);
    virtual status_t m_query(Map_t *map);

protected:
    void getFrameParams(Map_t* map);
    int32_t loadlibrary();
    int32_t unloadlibrary();
    int32_t doBokehPPInit();
    int32_t doBokehPPUninit();
    int32_t doBokehPPProcess(uint32_t bulrlevel);
    void dumpDatatoFile(MVoid *pData, int size, const char* name);
    void dumpYUVtoFile(ASVLOFFSCREEN *pAsvl, uint64_t idx, const char* name_prefix);
    void dumpFrameParams(bokeh_frame_params_t& p);
    int32_t loadKernelBin(ARC_DC_KERNALBIN *pKernelBin);
    void unloadKernelBin(ARC_DC_KERNALBIN *pKernelBin);

private:
    static void  *m_handle;
    static Mutex  m_handleLock;

    bokeh_image_dll_export_t *m_pBokehDllDes;
    uint8_t* m_pDisparityData;
    int32_t  m_i32DisparityDataSize;
    PMRECT   m_prtFaces;
    MInt32*  m_pi32FaceAngles;
    MFloat   m_fMaxFOV;  /* max FOV of Tele camera */
    ARC_DC_KERNALBIN m_KernelBin;
    bokeh_frame_params_t m_inParams;
};

#endif //ARCSOFT_FUSION_BOKEH_CAPTURE_H
