/*
 * Copyright 2012, Samsung Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed toggle an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*!
 * \file      ExynosCameraActivityVendorLibrary.h
 * \brief     hearder file for CAMERA HAL MODULE
 * \author    Pilsun Jang(pilsun.jang@samsung.com)
 * \date      2012/12/19
 *
 */

#ifndef EXYNOS_CAMERA_ACTIVITY_VENDOR_LIB_H__
#define EXYNOS_CAMERA_ACTIVITY_VENDOR_LIB_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utils/threads.h>

#include <videodev2.h>
#include <videodev2_exynos_camera.h>
#include <linux/vt.h>

#include <utils/RefBase.h>
#include <utils/String8.h>
#include <utils/List.h>
#include "cutils/properties.h"

#include "exynos_format.h"
#include "ExynosBuffer.h"
#include "ExynosRect.h"
#include "ExynosJpegEncoderForCamera.h"
#include "ExynosExif.h"
#include "exynos_v4l2.h"
#include "ExynosCameraActivityBase.h"

#include "fimc-is-metadata.h"

namespace android {

class ExynosCameraActivityVendorLibrary : public ExynosCameraActivityBase {
public:

    enum VENDORLIB_STEP {
        VENDOR_STEP_OFF,
        VENDOR_STEP_START,
        /* antishake */
        VENDOR_STEP_ANTISHAKE_PARAM_SET,
        VENDOR_STEP_ANTISHAKE_WAIT_FRAME,

        /* etc library*/

        VENDOR_STEP_END
    };

public:
    ExynosCameraActivityVendorLibrary();
    virtual ~ExynosCameraActivityVendorLibrary();

protected:
    int t_funcNull(void *args);
    int t_funcSensorBefore(void *args);
    int t_funcSensorAfter(void *args);
    int t_func3ABefore(void *args);
    int t_func3AAfter(void *args);
    int t_func3ABeforeHAL3(void *args);
    int t_func3AAfterHAL3(void *args);
    int t_funcISPBefore(void *args);
    int t_funcISPAfter(void *args);
    int t_funcSCPBefore(void *args);
    int t_funcSCPAfter(void *args);
    int t_funcSCCBefore(void *args);
    int t_funcSCCAfter(void *args);

    /* update the antishake status, set the manual sensor control. */
    int m_updateAntiShake(camera2_shot_ext *shot_ext);
    /* check the antishake status, update preview / capture best frame. */
    int m_checkAntiShake(camera2_shot_ext *shot_ext);
public:
    bool setLibraryManager(sp<ExynosCameraSFLMgr> manager);

    int setMode(SFLIBRARY_MGR::SFLType type);
    int setStep(enum VENDORLIB_STEP step);

private:
    sp<ExynosCameraSFLMgr> m_libraryMgr;
    SFLIBRARY_MGR::SFLType m_mode;
    enum VENDORLIB_STEP m_step;

    int                 m_delay;
    bool                m_check;
    camera2_shot_ext    m_shotExt;

    bool                m_requireRestore;

};
}

#endif /* EXYNOS_CAMERA_ACTIVITY_VENDOR_LIB_H__ */
