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

#ifndef EXYNOS_CAMERA_PIPE_STK_PREVIEW_H
#define EXYNOS_CAMERA_PIPE_STK_PREVIEW_H

#include <dlfcn.h>

#include "ExynosCameraSWPipe.h"
#include "ExynosCameraMetadataConverter.h"
#include "ExynosCameraTimeLogger.h"

#define HFD_LIBRARY_PATH "/vendor/lib/libhfd.so"

namespace android {

/* libHfd defined structures */
struct RectStr {
	int32_t     x1;
	int32_t     y1;
	int32_t     x2;
	int32_t     y2;
};


/*! \brief Type of poses Yaw angles. */
typedef enum
{
	VRA_YAW_FRONT = 0,				/*!< No YAW */
	VRA_YAW_RIGHT_PROFILE = 1,	/*!< for right profile => Yaw Angle = 90. 270 stands for left profile */
#ifdef VRA_OLD_POSES
	VRA_YAW_All = 2
#else
	VRA_YAW_RIGHT_SEMI_PROFILE = 2,	/*!< for right semi profile => Yaw Angle = 45. 315 stands for left semi profile */
	VRA_YAW_All = 3
#endif
}VRA_YawType;


struct FacesStr {
	uint32_t        id;
	float           score;
	struct RectStr  rect;
	bool            isRot;
#ifdef VRA_OLD_POSES
	bool            isYaw;
#else
    VRA_YawType     YawType;
#endif
	int32_t         rot;
	bool            mirrorX;
	int32_t         hwRotAndMirror;
};

struct FrameStr {
	unsigned char*  yPtr;
	uint32_t        yLength;
	unsigned char*  uvPtr;
	uint32_t        uvLength;
};

struct FrameProperties {
	uint32_t width;
	uint32_t height;
};
/* ------ */

class ExynosCameraPipeHFD : protected virtual ExynosCameraSWPipe {
public:
    ExynosCameraPipeHFD()
    {
        m_init();
    }

    ExynosCameraPipeHFD(
        int cameraId,
        ExynosCameraConfigurations *configurations,
        ExynosCameraParameters *obj_param,
        bool isReprocessing,
        int32_t *nodeNums)  : ExynosCameraSWPipe(cameraId, configurations, obj_param, isReprocessing, nodeNums)
    {
        m_init();
    }

    virtual status_t        create(int32_t *sensorIds = NULL);
    virtual status_t        stop(void);

protected:
    virtual status_t        m_destroy(void);
    virtual status_t        m_run(void);

private:
    void                    m_init(void);

    /* Library APIs */
    void*   (*createHfdCnnLib)(void);
    void    (*destroyHfdCnnLib)(void *handle);
    bool    (*handleHfdFrame)(void *handle, struct FrameStr *frame, struct FrameProperties *properties, bool resetTrackerDB,
                              const Vector<struct FacesStr>& facesIn, Vector<struct FacesStr>& facesOut);

    void    *m_hfdDl;
    void    *m_hfdHandle;
private:
    ExynosCameraDurationTimer      m_timer;

    typedef ExynosCameraThread<ExynosCameraPipeHFD> PipeHFDThread;
    sp<PipeHFDThread>                       m_initHfdThread;
    virtual bool                            m_initHfdThreadFunc(void);
};

}; /* namespace android */

#endif
