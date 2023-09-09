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
 * \file      ExynosCameraTimeLogger.h
 * \brief     header file for ExynosCameraTimeLogger
 * \author    Teahyung, Kim(tkon.kim@samsung.com)
 * \date      2017/01/04
 *
 * <b>Revision History: </b>
 * - 2017/01/04 : Teahyung, Kim(tkon.kim@samsung.com) \n
 */

#ifndef EXYNOS_CAMERA_TIME_LOGGER_H
#define EXYNOS_CAMERA_TIME_LOGGER_H

#include "string.h"
#include <utils/Log.h>
#include <cutils/atomic.h>

#include "ExynosCameraUtils.h"
#include "ExynosCameraAutoTimer.h"
#include "ExynosCameraConfig.h"
#include "ExynosCameraSingleton.h"
#include "ExynosCameraSensorInfoBase.h"

#define TIME_LOGGER_SIZE (1024 * 100) /* 100K * logger */
#define TIME_LOGGER_PATH "/data/media/0/exynos_camera_time_logger_cam%d_%lld.csv"

#define TIME_LOGGER_INIT_BASE(logger, cameraId)          \
        ({ (logger)->init(cameraId); })
#define TIME_LOGGER_UPDATE_BASE(logger, cameraId, key, pipeId, type, category, userData)  \
        ({ (logger)->update(cameraId, key, pipeId, LOGGER_TYPE_ ## type, LOGGER_CATEGORY_ ## category, userData); })
#define TIME_LOGGER_SAVE_BASE(logger, cameraId)          \
        ({ (logger)->save(cameraId); })

#ifdef TIME_LOGGER_ENABLE
#define TIME_LOGGER_INIT(cameraId)          \
        ({                                  \
            ExynosCameraTimeLogger *logger = ExynosCameraSingleton<ExynosCameraTimeLogger>::getInstance(); \
            TIME_LOGGER_INIT_BASE(logger, cameraId);    \
        })
/* you can remove "LOGGER_TYPE_", "LOGGER_CATEGORRY_" prefix */
#define TIME_LOGGER_UPDATE(cameraId, key, pipeId, type, category, userData)  \
        ({                                                                \
            ExynosCameraTimeLogger *logger = ExynosCameraSingleton<ExynosCameraTimeLogger>::getInstance(); \
            TIME_LOGGER_UPDATE_BASE(logger, cameraId, key, pipeId, type, category, userData); \
        })
#define TIME_LOGGER_SAVE(cameraId)          \
        ({                                  \
            ExynosCameraTimeLogger *logger = ExynosCameraSingleton<ExynosCameraTimeLogger>::getInstance(); \
            TIME_LOGGER_SAVE_BASE(logger, cameraId);         \
        })
#else
#define TIME_LOGGER_INIT(cameraId)
#define TIME_LOGGER_UPDATE(cameraId, key, pipeId, type, category, userData) \
        ({LOGGER_TYPE_ ## type; LOGGER_CATEGORY_ ## category; })
#define TIME_LOGGER_SAVE(cameraId)
#endif

#ifdef USE_DUAL_CAMERA
/* if it's dual camera, master camera should be measured for launching time */
#define TIME_LOGGER_LAUNCH_COND(param, cameraId)        \
    ({ bool ret; int masterCameraId, slaveCameraId;           \
       getDualCameraId(&masterCameraId, &slaveCameraId);    \
       ret = (((param)->getDualCameraMode() == false) ||      \
        ((param)->getDualCameraMode() == true &&        \
         (cameraId) == masterCameraId)); ret; })
#endif

using namespace android;

typedef enum LOGGER_TYPE {
    LOGGER_TYPE_BASE,
    LOGGER_TYPE_INTERVAL,
    LOGGER_TYPE_DURATION,
    LOGGER_TYPE_CUMULATIVE_CNT, /* to count error or specific events */
    LOGGER_TYPE_USER_DATA,
    LOGGER_TYPE_MAX,
} logger_type_t;

typedef enum LOGGER_CATEGORY {
    LOGGER_CATEGORY_BASE,
    /* for fixed purporse */
    LOGGER_CATEGORY_QBUF,
    LOGGER_CATEGORY_DQBUF,
    LOGGER_CATEGORY_RECORDING_CALLBACK,
    LOGGER_CATEGORY_RECORDING_RELEASE_FRAME,
    LOGGER_CATEGORY_PREVIEW_TRIGGER,  /* push the frame to previewThread */
    LOGGER_CATEGORY_PREVIEW_SERVICE_ENQUEUE,
    LOGGER_CATEGORY_PREVIEW_CALLBACK,
    /* for genral purpose */
    LOGGER_CATEGORY_POINT0,
    LOGGER_CATEGORY_POINT1,
    LOGGER_CATEGORY_POINT2,
    LOGGER_CATEGORY_POINT3,
    LOGGER_CATEGORY_POINT4,
    LOGGER_CATEGORY_POINT5,
    LOGGER_CATEGORY_POINT6,
    LOGGER_CATEGORY_POINT7,
    LOGGER_CATEGORY_POINT8,
    LOGGER_CATEGORY_POINT9,
    /* for launching time */
#ifdef TIME_LOGGER_LAUNCH_ENABLE
    LOGGER_CATEGORY_LAUNCH_TOTAL_TIME,
    LOGGER_CATEGORY_LAUNCH_OPEN_START,
    LOGGER_CATEGORY_LAUNCH_OPEN_END,
    LOGGER_CATEGORY_LAUNCH_FIRST_PARAMETER_START,
    LOGGER_CATEGORY_LAUNCH_FIRST_PARAMETER_END,
    LOGGER_CATEGORY_LAUNCH_START_PREVIEW_START,
    LOGGER_CATEGORY_LAUNCH_SET_BUFFER_START,
    LOGGER_CATEGORY_LAUNCH_SET_BUFFER_END,
    LOGGER_CATEGORY_LAUNCH_FASTAE_THREAD_WAIT_START,
    LOGGER_CATEGORY_LAUNCH_FASTAE_THREAD_WAIT_END,
    LOGGER_CATEGORY_LAUNCH_FACTORY_INIT_PIPES_START,
    LOGGER_CATEGORY_LAUNCH_FACTORY_INIT_PIPES_END,
    LOGGER_CATEGORY_LAUNCH_FACTORY_START_START,
    LOGGER_CATEGORY_LAUNCH_SENSOR_STREAM_ON,
    LOGGER_CATEGORY_LAUNCH_FACTORY_START_END,
    LOGGER_CATEGORY_LAUNCH_START_PREVIEW_END,
    LOGGER_CATEGORY_LAUNCH_SUB_COMPANION_THREAD,
    LOGGER_CATEGORY_LAUNCH_SUB_FASTAE_THREAD_START,
    LOGGER_CATEGORY_LAUNCH_SUB_COMPANION_WAIT,
    LOGGER_CATEGORY_LAUNCH_SUB_FASTAE_PRE_POST_CREATE_START,
    LOGGER_CATEGORY_LAUNCH_SUB_FASTAE_PRE_POST_CREATE_END,
    LOGGER_CATEGORY_LAUNCH_SUB_FASTAE_INSTANT_ON,
    LOGGER_CATEGORY_LAUNCH_SUB_FASTAE_THREAD_END,
#endif
    LOGGER_CATEGORY_MAX,
} logger_category_t;

/*
 * Class ExynosCameraTimeLogger
 * ExynosCameraTimeLogger is the time logging class for profiling performance.
 * This class cannot support concurrent processing of timelogging for same logger
 * (same logger condition : same pipeLine && same logger_type && same logger_category)
 */
class ExynosCameraTimeLogger
{
public:

    typedef struct timeLogger {
        uint64_t timeStamp;     /* ns : logging time */
        int cameraId;
        uint64_t key;
        uint32_t pipeId;
        LOGGER_TYPE type;
        LOGGER_CATEGORY category;
        uint32_t calTime;       /* us : cacluated time(duration, interval..) */
    } timeLogger_t;

    /* memset all logger information */
    status_t init(int cameraId);

    /*
     * update information
     *  @cameraId : (mandatory)
     *  @key : (mandatory) like buffer index or frameCount
     *  @pipeId : (optional)
     *  @type : (mandatory)
     *  @category : (mandatory)
     *  @userData : (mandatory) Always true in case of INTERVAL type
     * eg.
     *  <interval>
     *    getBuffer(&node);
     *    update(backCameraId, frameCount, PIPE_MCSC1, LOGGER_TYPE_INTERVAL, LOGGER_CATEGORY_DQBUF, true);
     *
     *  <duration>
     *    update(backCameraId, frameCount, PIPE_MCSC1, LOGGER_TYPE_DURATION, LOGGER_CATEGORY_DQBUF, true);
     *    getBuffer(&node);
     *    update(backCameraId, frameCount, PIPE_MCSC1, LOGGER_TYPE_DURATION, LOGGER_CATEGORY_DQBUF, false);
     */
    status_t update(int cameraId, uint64_t key, uint32_t pipeId, LOGGER_TYPE type, LOGGER_CATEGORY category, uint64_t userData);

    /*
     * save all information to file
     */
    status_t save(int cameraId);

    friend class ExynosCameraSingleton<ExynosCameraTimeLogger>;
    ExynosCameraTimeLogger();
    virtual ~ExynosCameraTimeLogger();

private:
    bool                    m_stopFlag[CAMERA_ID_MAX];
    bool                    m_firstCheckFlag[CAMERA_ID_MAX];
    ExynosCameraDurationTimer   m_timer[CAMERA_ID_MAX][LOGGER_TYPE_MAX][LOGGER_CATEGORY_MAX][MAX_PIPE_NUM];
    uint32_t                m_count[CAMERA_ID_MAX];
    volatile int32_t        m_bufferIndex[CAMERA_ID_MAX];
    timeLogger_t            *m_buffer[CAMERA_ID_MAX];
    char                    m_name[EXYNOS_CAMERA_NAME_STR_SIZE];
    char                    *m_typeStr[LOGGER_TYPE_MAX];
    char                    *m_categoryStr[LOGGER_CATEGORY_MAX];
};
#endif //EXYNOS_CAMERA_TIME_LOGGER_H
