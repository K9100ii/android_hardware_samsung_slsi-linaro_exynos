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

#define TIME_LOGGER_LOGV(cameraId, fmt, ...) \
        ALOGV(Paste2(CAM_ID"DEBUG" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define TIME_LOGGER_LOGD(cameraId, fmt, ...) \
        ALOGD(Paste2(CAM_ID"DEBUG" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define TIME_LOGGER_LOGW(cameraId, fmt, ...) \
        ALOGW(Paste2(CAM_ID"WARN" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define TIME_LOGGER_LOGE(cameraId, fmt, ...) \
        ALOGE(Paste2(CAM_ID"ERR" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

#define TIME_LOGGER_LOGI(cameraId, fmt, ...) \
        ALOGI(Paste2(CAM_ID"INFO" LOCATION_ID, fmt), cameraId, m_name, LOCATION_ID_PARM, ##__VA_ARGS__)

/*
 * Class ExynosCameraTimeLogger
 */
ExynosCameraTimeLogger::ExynosCameraTimeLogger()
{
    strncpy(m_name, "TimeLogger", (EXYNOS_CAMERA_NAME_STR_SIZE - 1));

    m_typeStr[LOGGER_TYPE_BASE]     = MAKE_STRING(INIT);
    m_typeStr[LOGGER_TYPE_INTERVAL] = MAKE_STRING(INTERVAL);
    m_typeStr[LOGGER_TYPE_DURATION] = MAKE_STRING(DURATION);
    m_typeStr[LOGGER_TYPE_CUMULATIVE_CNT] = MAKE_STRING(CUMULATIVE_CNT);
    m_typeStr[LOGGER_TYPE_USER_DATA] = MAKE_STRING(USER_DATA);

    m_categoryStr[LOGGER_CATEGORY_BASE]             = MAKE_STRING(INIT);
    m_categoryStr[LOGGER_CATEGORY_QBUF]             = MAKE_STRING(QBUF);
    m_categoryStr[LOGGER_CATEGORY_DQBUF]            = MAKE_STRING(DQBUF);
    m_categoryStr[LOGGER_CATEGORY_RECORDING_CALLBACK]  = MAKE_STRING(RECORDING_CALLBACK);
    m_categoryStr[LOGGER_CATEGORY_RECORDING_RELEASE_FRAME] = MAKE_STRING(RECORDING_RELEASE_FRAME);
    m_categoryStr[LOGGER_CATEGORY_PREVIEW_TRIGGER]  = MAKE_STRING(PREVIEW_TRIGGER);
    m_categoryStr[LOGGER_CATEGORY_PREVIEW_SERVICE_ENQUEUE] = MAKE_STRING(PREVIEW_SERVICE_ENQUEUE);
    m_categoryStr[LOGGER_CATEGORY_PREVIEW_CALLBACK] = MAKE_STRING(PREVIEW_CALLBACK);
    m_categoryStr[LOGGER_CATEGORY_POINT0]           = MAKE_STRING(POINT0);
    m_categoryStr[LOGGER_CATEGORY_POINT1]           = MAKE_STRING(POINT1);
    m_categoryStr[LOGGER_CATEGORY_POINT2]           = MAKE_STRING(POINT2);
    m_categoryStr[LOGGER_CATEGORY_POINT3]           = MAKE_STRING(POINT3);
    m_categoryStr[LOGGER_CATEGORY_POINT4]           = MAKE_STRING(POINT4);
    m_categoryStr[LOGGER_CATEGORY_POINT5]           = MAKE_STRING(POINT5);
    m_categoryStr[LOGGER_CATEGORY_POINT6]           = MAKE_STRING(POINT6);
    m_categoryStr[LOGGER_CATEGORY_POINT7]           = MAKE_STRING(POINT7);
    m_categoryStr[LOGGER_CATEGORY_POINT8]           = MAKE_STRING(POINT8);
    m_categoryStr[LOGGER_CATEGORY_POINT9]           = MAKE_STRING(POINT9);

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

    TIME_LOGGER_LOGD(cameraId, "");

    /* alloc buffer for dump */
    m_buffer[cameraId] = (timeLogger_t *)malloc(sizeof(timeLogger_t) * TIME_LOGGER_SIZE);
    if (m_buffer[cameraId] == NULL) {
        TIME_LOGGER_LOGE(cameraId, "can't alloc buffer");
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

    if (m_stopFlag[cameraId])
        return ret;

    if (m_buffer[cameraId] == NULL) {
        TIME_LOGGER_LOGE(cameraId, "can't alloc buffer");
        return INVALID_OPERATION;
    }

    if (type <= LOGGER_TYPE_BASE || type >= LOGGER_TYPE_MAX) {
        TIME_LOGGER_LOGE(cameraId, "invalid type(%d)", type);
        return INVALID_OPERATION;
    }

    if (category <= LOGGER_CATEGORY_BASE || category >= LOGGER_CATEGORY_MAX) {
        TIME_LOGGER_LOGE(cameraId, "invalid category(%d)", category);
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
    bufferIndex = (android_atomic_inc(&m_bufferIndex[cameraId]) - 1) % TIME_LOGGER_SIZE;
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
    buffer->timeStamp = systemTime(SYSTEM_TIME_MONOTONIC) / 1000000LL;
    buffer->cameraId = cameraId;
    buffer->key = key;
    buffer->pipeId = pipeId;
    buffer->type = type;
    buffer->category = category;

    TIME_LOGGER_LOGV(cameraId, "TimeStamp:%lld,Key:%lld,Pipe:%d,Type:%s,Cate:%s,CalT:%d",
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

    if (m_stopFlag[cameraId])
        return ret;

    m_stopFlag[cameraId] = true;

    if (m_buffer[cameraId] == NULL) {
        TIME_LOGGER_LOGE(cameraId, "can't alloc buffer");
        return INVALID_OPERATION;
    }

    snprintf(filePath, sizeof(filePath), TIME_LOGGER_PATH, cameraId, systemTime(SYSTEM_TIME_MONOTONIC));

    fd = fopen(filePath, "w+");
    if (fd == NULL) {
        TIME_LOGGER_LOGE(cameraId, "can't open file(%s)", filePath);
        return INVALID_OPERATION;
    }

    TIME_LOGGER_LOGD(cameraId, "save the time logger(%s)", filePath);

    /* save the file */
    fprintf(fd, "timestamp(ms),key,pipeId,type,category,calTime(us)\n");
    for (int i = 0; i < TIME_LOGGER_SIZE; i++) {
        buffer = &m_buffer[cameraId][i];
        if (buffer->timeStamp == 0)
            continue;

        fprintf(fd, "%lld,%lld,%d,%s,%s,%d\n",
                buffer->timeStamp,
                buffer->key,
                buffer->pipeId,
                m_typeStr[buffer->type],
                m_categoryStr[buffer->category],
                buffer->calTime);
    }
    fprintf(fd, "totalCount:%d\n", m_count[cameraId]);

    fflush(fd);

    TIME_LOGGER_LOGD(cameraId, "success!! to save the time logger(%s)", filePath);

    /* free the memory */
    if (fd)
        fclose(fd);
    free(m_buffer[cameraId]);
    m_buffer[cameraId] = NULL;

    return ret;
}
