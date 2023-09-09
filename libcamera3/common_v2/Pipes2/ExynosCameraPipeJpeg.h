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

#ifndef EXYNOS_CAMERA_PIPE_JPEG_H
#define EXYNOS_CAMERA_PIPE_JPEG_H

#include "ExynosCameraSWPipe.h"
#include "ExynosJpegEncoderForCamera.h"

namespace android {

class ExynosCameraPipeJpeg : protected virtual ExynosCameraSWPipe {
public:
    ExynosCameraPipeJpeg()
    {
        m_init();
    }

    ExynosCameraPipeJpeg(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums) : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    ~ExynosCameraPipeJpeg()
    {
        if (m_shot_ext) {
            delete m_shot_ext;
            m_shot_ext = NULL;
        }
    }

    virtual status_t        stop(void);

protected:
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    void                    m_init(void);

private:
    ExynosJpegEncoderForCamera  m_jpegEnc;
    struct camera2_shot_ext    *m_shot_ext;
#ifdef SAMSUNG_JQ
    unsigned char               m_qtable[128];
#endif
};

}; /* namespace android */

#endif
