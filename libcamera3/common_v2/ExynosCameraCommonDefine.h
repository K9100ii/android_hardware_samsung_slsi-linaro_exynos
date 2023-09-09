#ifndef EXYNOS_CAMERA_COMMON_DEFINE_H__
#define EXYNOS_CAMERA_COMMON_DEFINE_H__

#define BUILD_DATE()   ALOGE("Build Date is (%s) (%s)", __DATE__, __TIME__)
#define WHERE_AM_I()   ALOGE("[(%s)%d] ", __FUNCTION__, __LINE__)
#define LOG_DELAY()    usleep(100000)

#define TARGET_ANDROID_VER_MAJ 4
#define TARGET_ANDROID_VER_MIN 4

/* ---------------------------------------------------------- */
/* log */
#define PREFIX_FMT   "[CAM(%d)][%s]-"
#define LOCATION_FMT "(%s[%d]):"
#define PERFRAME_FMT "[%d][FRM:%d(CAM:%d,DRV:%d,T:%d)][REQ:%d]:"

#ifdef USE_DEBUG_PROPERTY
// logmanager init
#define LOGMGR_INIT() ExynosCameraLogManager::init()

// common log
#define CLOG_COMMON(_prio, _cameraId, _name, _fmt, ...)  \
        ExynosCameraLogManager::logCommon(_prio, LOG_TAG, PREFIX_FMT LOCATION_FMT, _cameraId, _name, \
                                          __FUNCTION__, __LINE__, _fmt, ##__VA_ARGS__)

// perframe log
#define CLOG_PERFRAME(_type, _cameraId, _name, _frame, _data, _requestKey, _fmt, ...) \
        ExynosCameraLogManager::logPerframe(ANDROID_LOG_VERBOSE, LOG_TAG, PREFIX_FMT LOCATION_FMT PERFRAME_FMT,    \
                                           _cameraId, _name, __FUNCTION__, __LINE__, _fmt,           \
                                           ExynosCameraLogManager::PERFRAME_TYPE_ ## _type,               \
                                           _frame, _data, _requestKey, \
                                           ##__VA_ARGS__)


// performance log
#define CLOG_PERFORMANCE(_type, _cameraId, _factoryType, _time, _pos, _posSubKey, _key, ...) \
        ExynosCameraLogManager::logPerformance(ANDROID_LOG_VERBOSE, LOG_TAG, PREFIX_FMT,    \
                                           _cameraId, _factoryType,                         \
                                           ExynosCameraLogManager::PERFORMANCE_TYPE_ ## _type, \
                                           ExynosCameraLogManager::TM_ ## _time,               \
                                           ExynosCameraLogManager::PERF_ ## _type ## _ ## _pos, \
                                           _posSubKey, _key, ##__VA_ARGS__)
#else
#define LOGMGR_INIT() ((void)0)
#define CLOG_COMMON(_prio, _cameraId, _name, _fmt, ...)  \
        do {                                             \
            if (_prio == ANDROID_LOG_VERBOSE) {          \
                ALOGV(PREFIX_FMT LOCATION_FMT _fmt, _cameraId, _name, __FUNCTION__, __LINE__, ##__VA_ARGS__);    \
            } else {                                     \
                LOG_PRI(_prio, LOG_TAG, PREFIX_FMT LOCATION_FMT _fmt, _cameraId, _name, __FUNCTION__, __LINE__, ##__VA_ARGS__);    \
            }                                            \
        } while(0)
#define CLOG_PERFRAME(_type, _frame, _request, _fmt, ...) ((void)0)
#define CLOG_PERFRAME2(_type, _cameraId, _name, _frame, _request, _fmt, ...) ((void)0)
#define CLOG_PERFORMANCE(_type, _cameraId, _factoryType, _pos, _posSubKey, _key, ...) ((void)0)
#endif

/**
 * Log with cameraId(member var) and instance name(member var)
 */
#define CLOGD(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_DEBUG, m_cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGV(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_VERBOSE, m_cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGW(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_WARN, m_cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGE(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_ERROR, m_cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGI(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_INFO, m_cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGT(cnt, fmt, ...) \
        if (cnt != 0) CLOGI(fmt, ##__VA_ARGS__)

#define CLOG_ASSERT(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_FATAL, m_cameraId, m_name, fmt, ##__VA_ARGS__)

/**
 * Log with no cameraId and no instance name
 */
#define CLOGD2(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_DEBUG, -1, "", fmt, ##__VA_ARGS__)

#define CLOGV2(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_VERBOSE, -1, "", fmt, ##__VA_ARGS__)

#define CLOGW2(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_WARN, -1, "", fmt, ##__VA_ARGS__)

#define CLOGE2(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_ERROR, -1, "", fmt, ##__VA_ARGS__)

#define CLOGI2(fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_INFO, -1, "", fmt, ##__VA_ARGS__)

/**
 * Log with cameraId(param) and instance name(member var)
 */
#define CLOGV3(cameraId, fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_VERBOSE, cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGD3(cameraId, fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_DEBUG, cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGW3(cameraId, fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_WARN, cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGE3(cameraId, fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_ERROR, cameraId, m_name, fmt, ##__VA_ARGS__)

#define CLOGI3(cameraId, fmt, ...) \
        CLOG_COMMON(ANDROID_LOG_INFO, cameraId, m_name, fmt, ##__VA_ARGS__)

/* ---------------------------------------------------------- */
/* Debug Timer */
#define DEBUG_TIMER_INIT \
    ExynosCameraDurationTimer debugPPPTimer;
#define DEBUG_TIMER_START \
    debugPPPTimer.start();
#define DEBUG_TIMER_STOP \
    debugPPPTimer.stop(); CLOGD("DEBUG(%s[%d]): DurationTimer #0 (%lld usec)", __FUNCTION__, __LINE__, debugPPPTimer.durationUsecs());

/* ---------------------------------------------------------- */
/* Node Prefix */
#define NODE_PREFIX "/dev/video"

/* ---------------------------------------------------------- */
/* Max Camera Name Size */
#define EXYNOS_CAMERA_NAME_STR_SIZE (256)

/* ---------------------------------------------------------- */
/* Sensor scenario enum for setInput */
#define SENSOR_SCENARIO_NORMAL          (0)
#define SENSOR_SCENARIO_VISION          (1)
#define SENSOR_SCENARIO_EXTERNAL        (2)
#define SENSOR_SCENARIO_OIS_FACTORY     (3)
#define SENSOR_SCENARIO_READ_ROM        (4)
#define SENSOR_SCENARIO_STANDBY         (5)
#define SENSOR_SCENARIO_SECURE          (6)
#define SENSOR_SCENARIO_VIRTUAL         (9)
#define SENSOR_SCENARIO_MAX            (10)

#define SENSOR_SIZE_WIDTH_MASK          0xFFFF0000
#define SENSOR_SIZE_WIDTH_SHIFT         16
#define SENSOR_SIZE_HEIGHT_MASK         0xFFFF
#define SENSOR_SIZE_HEIGHT_SHIFT        0

/* ---------------------------------------------------------- */

#endif /* EXYNOS_CAMERA_COMMON_DEFINE_H__ */

