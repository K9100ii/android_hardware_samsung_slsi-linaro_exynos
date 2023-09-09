/*
**
** Copyright 2017, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_CAMERA_ACTIVITY_CONTROL_H
#define EXYNOS_CAMERA_ACTIVITY_CONTROL_H

#include <log/log.h>
#include <utils/threads.h>
#include <utils/RefBase.h>
#include <utils/String8.h>
#include <cutils/properties.h>

#include "ExynosCameraActivityAutofocus.h"
#include "ExynosCameraActivityFlash.h"
#include "ExynosCameraActivitySpecialCapture.h"
#include "ExynosCameraActivityUCTL.h"
#include "ExynosCameraSensorInfoBase.h"
#include "ExynosRect.h"

namespace android{

enum auto_focus_type {
    AUTO_FOCUS_SERVICE      = 0,
    AUTO_FOCUS_HAL,
};

class ExynosCameraActivityControl {
public:

public:
    /* Constructor */
    ExynosCameraActivityControl(int cameraId);

    /* Destructor */
    virtual ~ExynosCameraActivityControl();
    /* Destroy the instance */
    bool            destroy(void);
    /* Check if the instance was created */
    bool            flagCreate(void);

#ifdef OIS_CAPTURE
    /* Sets OIS mode */
    void            setOISCaptureMode(bool oisMode);
#endif

    void            activityBeforeExecFunc(int pipeId, void *args);
    void            activityAfterExecFunc(int pipeId, void *args);
    ExynosCameraActivityFlash *getFlashMgr(void);
    ExynosCameraActivitySpecialCapture *getSpecialCaptureMgr(void);
    ExynosCameraActivityAutofocus *getAutoFocusMgr(void);
    ExynosCameraActivityUCTL *getUCTLMgr(void);

private:
    bool m_checkSensorOnThisPipe(int pipeId, void *args);

public:


private:
    ExynosCameraActivityAutofocus *m_autofocusMgr;
    ExynosCameraActivityFlash *m_flashMgr;
    ExynosCameraActivitySpecialCapture * m_specialCaptureMgr;
    ExynosCameraActivityUCTL * m_uctlMgr;
    int  m_camId;
};

}

#endif

