/*
 * Copyright (C) 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_PLUGIN_CONVERTER_ARCHSOFT_FUSION_ZOOM_PREVIEW_H__
#define EXYNOS_CAMERA_PLUGIN_CONVERTER_ARCHSOFT_FUSION_ZOOM_PREVIEW_H__

#include "ExynosCameraPlugInConverterArcsoftFusion.h"

namespace android {

class ExynosCameraPlugInConverterArcsoftFusionZoomPreview : public virtual ExynosCameraPlugInConverter {
public:
    ExynosCameraPlugInConverterArcsoftFusionZoomPreview() : ExynosCameraPlugInConverter()
    {
        m_init();
    }

    ExynosCameraPlugInConverterArcsoftFusionZoomPreview(int cameraId, int pipeId) : ExynosCameraPlugInConverter(cameraId, pipeId)
    {
        m_init();
    }

    virtual ~ExynosCameraPlugInConverterArcsoftFusionZoomPreview() { ALOGD("%s", __FUNCTION__); };

protected:
    // inherit this function.
    virtual status_t m_init(void);
    virtual status_t m_deinit(void);
    //virtual status_t m_create(Map_t *map);
    virtual status_t m_setup(Map_t *map);
    virtual status_t m_make(Map_t *map);

protected:
    // help function.
    bool     m_checkFallback(enum DUAL_OPERATION_MODE dualOpMode, float zoomRatio, const struct camera2_shot_ext* wideMeta, const struct camera2_shot_ext* teleMeta);
    status_t m_convertParameters2Map(ExynosCameraConfigurations *configurations,
                                     ExynosCameraParameters *wideParameters,
                                     ExynosCameraParameters* teleParameters,
                                     Map_t *map);
    status_t m_convertMap2Parameters(Map_t *map,
                                     ExynosCameraConfigurations *configurations,
                                     ExynosCameraParameters *wideParameters,
                                     ExynosCameraParameters* teleParameters);

private:
    // for default converting to send the plugIn
    int m_syncType;

    int m_wideFullsizeW;
    int m_wideFullsizeH;
    int m_teleFullsizeW;
    int m_teleFullsizeH;

    int m_afStatus[2];
    uint32_t m_iso[2];
    float    m_zoomRatio[2];
    int      m_zoomLevel;
    int      m_zoomRatioListSize[2];
    float   *m_zoomRatioList[2];
    bool     m_flagHaveZoomRatioList;
    bool     m_fallback;


    int      m_oldDualOpMode;
    int      m_oldfallback;
    enum DUAL_OPERATION_MODE  m_oldMaster3a;
    int      m_oldMasterCameraHAL2Arc;
};
}; /* namespace android */
#endif
