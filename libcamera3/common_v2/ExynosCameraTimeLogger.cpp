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

#define LOG_TAG "ExynosCameraTimeLogger"


#include "ExynosCameraTimeLogger.h"

/*
 * Class ExynosCameraTimeLogger
 */
ExynosCameraTimeLogger::ExynosCameraTimeLogger()
{
    strncpy(m_name, "TimeLogger", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));

    m_typeStr[LOGGER_TYPE_BASE]                                         = MAKE_STRING(INIT);
    m_typeStr[LOGGER_TYPE_INTERVAL]                                     = MAKE_STRING(INTERVAL);
    m_typeStr[LOGGER_TYPE_DURATION]                                     = MAKE_STRING(DURATION);
    m_typeStr[LOGGER_TYPE_CUMULATIVE_CNT]                               = MAKE_STRING(CUMULATIVE_CNT);
    m_typeStr[LOGGER_TYPE_USER_DATA]                                    = MAKE_STRING(USER_DATA);

    m_categoryStr[LOGGER_CATEGORY_BASE]                                 = MAKE_STRING(INIT);
    m_categoryStr[LOGGER_CATEGORY_QBUF]                                 = MAKE_STRING(QBUF);
    m_categoryStr[LOGGER_CATEGORY_DQBUF]                                = MAKE_STRING(DQBUF);

    m_categoryStr[LOGGER_CATEGORY_POINT0]                               = MAKE_STRING(POINT0);
    m_categoryStr[LOGGER_CATEGORY_POINT1]                               = MAKE_STRING(POINT1);
    m_categoryStr[LOGGER_CATEGORY_POINT2]                               = MAKE_STRING(POINT2);
    m_categoryStr[LOGGER_CATEGORY_POINT3]                               = MAKE_STRING(POINT3);
    m_categoryStr[LOGGER_CATEGORY_POINT4]                               = MAKE_STRING(POINT4);
    m_categoryStr[LOGGER_CATEGORY_POINT5]                               = MAKE_STRING(POINT5);
    m_categoryStr[LOGGER_CATEGORY_POINT6]                               = MAKE_STRING(POINT6);
    m_categoryStr[LOGGER_CATEGORY_POINT7]                               = MAKE_STRING(POINT7);
    m_categoryStr[LOGGER_CATEGORY_POINT8]                               = MAKE_STRING(POINT8);
    m_categoryStr[LOGGER_CATEGORY_POINT9]                               = MAKE_STRING(POINT9);

    m_categoryStr[LOGGER_CATEGORY_OPEN_START]                           = MAKE_STRING(OPEN_START);
    m_categoryStr[LOGGER_CATEGORY_OPEN_END]                             = MAKE_STRING(OPEN_END);
    m_categoryStr[LOGGER_CATEGORY_INITIALIZE_START]                     = MAKE_STRING(INITIALIZE_START);
    m_categoryStr[LOGGER_CATEGORY_INITIALIZE_END]                       = MAKE_STRING(INITIALIZE_END);
    m_categoryStr[LOGGER_CATEGORY_FIRST_SET_PARAMETERS_START]           = MAKE_STRING(FIRST_SET_PARAMETERS_START);
    m_categoryStr[LOGGER_CATEGORY_READ_ROM_THREAD_START]                = MAKE_STRING(READ_ROM_THREAD_START);
    m_categoryStr[LOGGER_CATEGORY_READ_ROM_THREAD_END]                  = MAKE_STRING(READ_ROM_THREAD_END);
    m_categoryStr[LOGGER_CATEGORY_READ_ROM_THREAD_JOIN_START]           = MAKE_STRING(READ_ROM_THREAD_JOIN_START);
    m_categoryStr[LOGGER_CATEGORY_READ_ROM_THREAD_JOIN_END]             = MAKE_STRING(READ_ROM_THREAD_JOIN_END);
    m_categoryStr[LOGGER_CATEGORY_FIRST_SET_PARAMETERS_END]             = MAKE_STRING(FIRST_SET_PARAMETERS_END);
    m_categoryStr[LOGGER_CATEGORY_CONFIGURE_STREAM_START]               = MAKE_STRING(CONFIGURE_STREAM_START);
    m_categoryStr[LOGGER_CATEGORY_FASTEN_AE_THREAD_START]               = MAKE_STRING(FASTEN_AE_THREAD_START);
    m_categoryStr[LOGGER_CATEGORY_FASTEN_AE_THREAD_END]                 = MAKE_STRING(FASTEN_AE_THREAD_END);
    m_categoryStr[LOGGER_CATEGORY_FASTEN_AE_THREAD_JOIN_START]          = MAKE_STRING(FASTEN_AE_THREAD_JOIN_START);
    m_categoryStr[LOGGER_CATEGORY_FASTEN_AE_THREAD_JOIN_END]            = MAKE_STRING(FASTEN_AE_THREAD_JOIN_END);
    m_categoryStr[LOGGER_CATEGORY_STREAM_BUFFER_ALLOC_START]            = MAKE_STRING(STREAM_BUFFER_ALLOC_START);
    m_categoryStr[LOGGER_CATEGORY_STREAM_BUFFER_ALLOC_END]              = MAKE_STRING(STREAM_BUFFER_ALLOC_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_CREATE_START]                 = MAKE_STRING(FACTORY_CREATE_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_CREATE_END]                   = MAKE_STRING(FACTORY_CREATE_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_CREATE_THREAD_JOIN_START]     = MAKE_STRING(FACTORY_CREATE_THREAD_JOIN_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_CREATE_THREAD_JOIN_END]       = MAKE_STRING(FACTORY_CREATE_THREAD_JOIN_END);
    m_categoryStr[LOGGER_CATEGORY_CONFIGURE_STREAM_END]                 = MAKE_STRING(CONFIGURE_STREAM_END);
    m_categoryStr[LOGGER_CATEGORY_PROCESS_CAPTURE_REQUEST_START]        = MAKE_STRING(PROCESS_CAPTURE_REQUEST_START);
    m_categoryStr[LOGGER_CATEGORY_SET_BUFFER_THREAD_START]              = MAKE_STRING(SET_BUFFER_THREAD_START);
    m_categoryStr[LOGGER_CATEGORY_SET_BUFFER_THREAD_END]                = MAKE_STRING(SET_BUFFER_THREAD_END);
    m_categoryStr[LOGGER_CATEGORY_SET_BUFFER_THREAD_JOIN_START]         = MAKE_STRING(SET_BUFFER_THREAD_JOIN_START);
    m_categoryStr[LOGGER_CATEGORY_SET_BUFFER_THREAD_JOIN_END]           = MAKE_STRING(SET_BUFFER_THREAD_JOIN_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_START_THREAD_START]           = MAKE_STRING(FACTORY_START_THREAD_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_INIT_PIPES_START]             = MAKE_STRING(FACTORY_INIT_PIPES_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_INIT_PIPES_END]               = MAKE_STRING(FACTORY_INIT_PIPES_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_STREAM_START_START]           = MAKE_STRING(FACTORY_STREAM_START_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_STREAM_START_END]             = MAKE_STRING(FACTORY_STREAM_START_END);
    m_categoryStr[LOGGER_CATEGORY_HFD_CREATE_START]                     = MAKE_STRING(HFD_CREATE_START);
    m_categoryStr[LOGGER_CATEGORY_HFD_CREATE_END]                       = MAKE_STRING(HFD_CREATE_END);
    m_categoryStr[LOGGER_CATEGORY_PREVIEW_STREAM_THREAD]                = MAKE_STRING(PREVIEW_STREAM_THREAD);
    m_categoryStr[LOGGER_CATEGORY_RESULT_CALLBACK]                      = MAKE_STRING(RESULT_CALLBACK);

    m_categoryStr[LOGGER_CATEGORY_PROCESS_CAPTURE_REQUEST]              = MAKE_STRING(PROCESS_CAPTURE_REQUEST);
    m_categoryStr[LOGGER_CATEGORY_BLOCK_PROCESS_CAPTURE_REQUEST]        = MAKE_STRING(BLOCK_PROCESS_CAPTURE_REQUEST);
    m_categoryStr[LOGGER_CATEGORY_CREATE_PREVIEW_FRAME]                 = MAKE_STRING(CREATE_PREVIEW_FRAME);
    m_categoryStr[LOGGER_CATEGORY_CREATE_CAPTURE_FRAME]                 = MAKE_STRING(CREATE_CAPTURE_FRAME);
    m_categoryStr[LOGGER_CATEGORY_CREATE_INTERNAL_FRAME]                = MAKE_STRING(CREATE_INTERNAL_FRAME);
    m_categoryStr[LOGGER_CATEGORY_UPDATE_RESULT]                        = MAKE_STRING(UPDATE_RESULT);

    m_categoryStr[LOGGER_CATEGORY_CLOSE_START]                          = MAKE_STRING(CLOSE_START);
    m_categoryStr[LOGGER_CATEGORY_CLOSE_END]                            = MAKE_STRING(CLOSE_END);
    m_categoryStr[LOGGER_CATEGORY_RELEASE_DEVICE_START]                 = MAKE_STRING(RELEASE_DEVICE_START);
    m_categoryStr[LOGGER_CATEGORY_RELEASE_DEVICE_END]                   = MAKE_STRING(RELEASE_DEVICE_END);
    m_categoryStr[LOGGER_CATEGORY_MONITOR_THREAD_STOP_START]            = MAKE_STRING(MONITOR_THREAD_STOP_START);
    m_categoryStr[LOGGER_CATEGORY_MONITOR_THREAD_STOP_END]              = MAKE_STRING(MONITOR_THREAD_STOP_END);
    m_categoryStr[LOGGER_CATEGORY_FLUSH_START]                          = MAKE_STRING(FLUSH_START);
    m_categoryStr[LOGGER_CATEGORY_FLUSH_END]                            = MAKE_STRING(FLUSH_END);
    m_categoryStr[LOGGER_CATEGORY_FRAME_CREATE_THREAD_STOP_END]         = MAKE_STRING(FRAME_CREATE_THREAD_STOP_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_STREAM_STOP_START]            = MAKE_STRING(FACTORY_STREAM_STOP_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_STREAM_STOP_END]              = MAKE_STRING(FACTORY_STREAM_STOP_END);
    m_categoryStr[LOGGER_CATEGORY_LIBRARY_DEINIT_END]                   = MAKE_STRING(LIBRARY_DEINIT_END);
    m_categoryStr[LOGGER_CATEGORY_STREAM_THREAD_STOP_END]               = MAKE_STRING(STREAM_THREAD_STOP_END);
    m_categoryStr[LOGGER_CATEGORY_REQUEST_FLUSH_START]                  = MAKE_STRING(REQUEST_FLUSH_START);
    m_categoryStr[LOGGER_CATEGORY_REQUEST_FLUSH_END]                    = MAKE_STRING(REQUEST_FLUSH_END);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_RESET_START]                   = MAKE_STRING(BUFFER_RESET_START);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_RESET_END]                     = MAKE_STRING(BUFFER_RESET_END);
    m_categoryStr[LOGGER_CATEGORY_WAIT_PROCESS_CAPTURE_REQUEST_START]   = MAKE_STRING(WAIT_PROCESS_CAPTURE_REQUEST_START);
    m_categoryStr[LOGGER_CATEGORY_WAIT_PROCESS_CAPTURE_REQUEST_END]     = MAKE_STRING(WAIT_PROCESS_CAPTURE_REQUEST_END);
    m_categoryStr[LOGGER_CATEGORY_DESTRUCTOR_START]                     = MAKE_STRING(DESTRUCTOR_START);
    m_categoryStr[LOGGER_CATEGORY_DESTRUCTOR_END]                       = MAKE_STRING(DESTRUCTOR_END);
    m_categoryStr[LOGGER_CATEGORY_PRE_DESTRUCTOR_END]                   = MAKE_STRING(PRE_DESTRUCTOR_END);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_SUPPLIER_DEINIT_START]         = MAKE_STRING(BUFFER_SUPPLIER_DEINIT_START);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_SUPPLIER_DEINIT_END]           = MAKE_STRING(BUFFER_SUPPLIER_DEINIT_END);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_DESTROY_START]                = MAKE_STRING(FACTORY_DESTROY_START);
    m_categoryStr[LOGGER_CATEGORY_FACTORY_DESTROY_END]                  = MAKE_STRING(FACTORY_DESTROY_END);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_SUPPLIER_DEINIT_JOIN_START]    = MAKE_STRING(BUFFER_SUPPLIER_DEINIT_JOIN_START);
    m_categoryStr[LOGGER_CATEGORY_BUFFER_SUPPLIER_DEINIT_JOIN_END]      = MAKE_STRING(BUFFER_SUPPLIER_DEINIT_JOIN_END);
    m_categoryStr[LOGGER_CATEGORY_POST_DESTRUCTOR_END]                  = MAKE_STRING(POST_DESTRUCTOR_END);

    for (int i = 0; i < CAMERA_ID_MAX; i++) {
        m_buffer[i] = NULL;
        m_bufferIndex[i] = 0;
        m_stopFlag[i] = true;
        m_firstCheckFlag[i] = true;
    }
}

ExynosCameraTimeLogger::~ExynosCameraTimeLogger()
{}

status_t ExynosCameraTimeLogger::init(int cameraId)
{
    status_t ret = NO_ERROR;

    CLOGD3(cameraId, "");

    /* alloc buffer for dump */
    m_buffer[cameraId] = (timeLogger_t *)malloc(sizeof(timeLogger_t) * TIME_LOGGER_SIZE);
    if (m_buffer[cameraId] == NULL) {
        CLOGE3(cameraId, "can't alloc buffer");
        return INVALID_OPERATION;
    }

    memset(m_buffer[cameraId], 0x0, sizeof(timeLogger_t) * TIME_LOGGER_SIZE);

    /* init the variables */
    m_bufferIndex[cameraId] = 0;
    m_stopFlag[cameraId] = false;
    m_firstCheckFlag[cameraId] = false;
    m_count[cameraId] = 0;

    return ret;
}

status_t ExynosCameraTimeLogger::update(int cameraId, uint64_t key, uint32_t pipeId, LOGGER_TYPE type, LOGGER_CATEGORY category, uint64_t userData)
{
    status_t ret = NO_ERROR;
    ExynosCameraDurationTimer *timer;
    timeLogger_t *buffer;
    int bufferIndex;

    if (m_stopFlag[cameraId] == true || checkCondition(category) == false)
        return ret;

    if (m_buffer[cameraId] == NULL) {
        CLOGE3(cameraId, "can't alloc buffer");
        return INVALID_OPERATION;
    }

    if (type <= LOGGER_TYPE_BASE || type >= LOGGER_TYPE_MAX) {
        CLOGE3(cameraId, "invalid type(%d)", type);
        return INVALID_OPERATION;
    }

    if (category <= LOGGER_CATEGORY_BASE || category >= LOGGER_CATEGORY_MAX) {
        CLOGE3(cameraId, "invalid category(%d)", category);
        return INVALID_OPERATION;
    }

    timer = &m_timer[cameraId][type][category][pipeId];

    switch (type) {
    case LOGGER_TYPE_INTERVAL:
        timer->stop();

        if (m_firstCheckFlag[cameraId] == false) {
            timer->start();
            m_firstCheckFlag[cameraId] = true;
        } else {
            goto p_logger;
        }
        break;
    case LOGGER_TYPE_DURATION:
        if (userData) {
            timer->start();
        } else {
            timer->stop();
            goto p_logger;
        }
        break;
    case LOGGER_TYPE_CUMULATIVE_CNT:
        if (m_firstCheckFlag[cameraId] == false) {
            m_count[cameraId] = 1;
            m_firstCheckFlag[cameraId] = true;
        } else {
            m_count[cameraId]++;
        }
        goto p_logger;
        break;
    case LOGGER_TYPE_USER_DATA:
        goto p_logger;
        break;
    default:
        break;
    }

    return ret;

p_logger:
    bufferIndex = (android_atomic_inc(&m_bufferIndex[cameraId])) % TIME_LOGGER_SIZE;
    buffer = &m_buffer[cameraId][bufferIndex];

    /* save the time(us) or count */
    switch (type) {
    case LOGGER_TYPE_INTERVAL:
        buffer->calTime = timer->durationUsecs();
        /* for interval time */
        if (type == LOGGER_TYPE_INTERVAL)
            timer->start();
        break;
    case LOGGER_TYPE_DURATION:
        buffer->calTime = timer->durationUsecs();
        break;
    case LOGGER_TYPE_CUMULATIVE_CNT:
        buffer->calTime = m_count[cameraId];
        break;
    case LOGGER_TYPE_USER_DATA:
        buffer->calTime = userData;
        break;
    default:
        break;
    };
    buffer->timeStamp = systemTime(SYSTEM_TIME_REALTIME) / 1000000LL;
    buffer->cameraId = cameraId;
    buffer->key = key;
    buffer->pipeId = pipeId;
    buffer->type = type;
    buffer->category = category;

    CLOGV3(cameraId, "TimeStamp:%jd,Key:%jd,Pipe:%d,Type:%s,Cate:%s,CalT:%d",
            buffer->timeStamp,
            buffer->key,
            buffer->pipeId,
            m_typeStr[buffer->type],
            m_categoryStr[buffer->category],
            buffer->calTime);

    return ret;
}

status_t ExynosCameraTimeLogger::save(int cameraId)
{
    status_t ret = NO_ERROR;
    FILE *fd = NULL;
    char filePath[128];
    timeLogger_t *buffer;

    if (m_stopFlag[cameraId] == true) {
        return ret;
    } else if (m_bufferIndex[cameraId] == 0) {
        CLOGD3(cameraId, "No data to save");
        return ret;
    }

    m_stopFlag[cameraId] = true;

    if (m_buffer[cameraId] == NULL) {
        CLOGE3(cameraId, "can't alloc buffer");
        return INVALID_OPERATION;
    }

    snprintf(filePath, sizeof(filePath), TIME_LOGGER_PATH, cameraId, (unsigned long long)systemTime(SYSTEM_TIME_MONOTONIC));

    fd = fopen(filePath, "w+");
    if (fd == NULL) {
        CLOGE3(cameraId, "can't open file(%s)", filePath);
        return INVALID_OPERATION;
    }

    CLOGD3(cameraId, "save the time logger(%s)", filePath);

    /* save the file */
    fprintf(fd, "timestamp(ms),key,pipeId,type,category,calTime(us)\n");
    for (int i = 0; i < TIME_LOGGER_SIZE; i++) {
        buffer = &m_buffer[cameraId][i];
        if (buffer->timeStamp == 0)
            continue;

        fprintf(fd, "%jd,%jd,%d,%s,%s,%d\n",
                buffer->timeStamp,
                buffer->key,
                buffer->pipeId,
                m_typeStr[buffer->type],
                m_categoryStr[buffer->category],
                buffer->calTime);
    }
    fprintf(fd, "totalCount:%d\n", m_count[cameraId]);

    fflush(fd);

    CLOGD3(cameraId, "success!! to save the time logger(%s)", filePath);

    /* free the memory */
    if (fd)
        fclose(fd);
    free(m_buffer[cameraId]);
    m_buffer[cameraId] = NULL;

    return ret;
}

bool ExynosCameraTimeLogger::checkCondition(LOGGER_CATEGORY category)
{
    if (category > LOGGER_CATEGORY_LAUNCHING_TIME_START
        && category < LOGGER_CATEGORY_LAUNCHING_TIME_END) {
#ifdef TIME_LOGGER_LAUNCH_ENABLE
        return true;
#else
        return false;
#endif
    } else if (category > LOGGER_CATEGORY_STREAM_PERFORMANCE_START
               && category < LOGGER_CATEGORY_STREAM_PERFORMANCE_END) {
#ifdef TIME_LOGGER_STREAM_PERFORMANCE_ENABLE
        return true;
#else
        return false;
#endif
    } else if (category > LOGGER_CATEGORY_CLOSING_TIME_START
               && category < LOGGER_CATEGORY_CLOSING_TIME_END) {
#ifdef TIME_LOGGER_CLOSE_ENABLE
        return true;
#else
        return false;
#endif
    } else {
#ifdef TIME_LOGGER_ENABLE
        return true;
#else
        return false;
#endif
    }

    return false;
}
