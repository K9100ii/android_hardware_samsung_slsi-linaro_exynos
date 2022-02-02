#ifndef EXYNOS_CAMERA_COMMON_DEFINE_H__
#define EXYNOS_CAMERA_COMMON_DEFINE_H__

#include <math.h>

#define BUILD_DATE()   ALOGE("Build Date is (__DATE__) (__TIME__)")
#define WHERE_AM_I()   ALOGE("[(%s)%d] ", __FUNCTION__, __LINE__)
#define LOG_DELAY()    usleep(100000)

#define TARGET_ANDROID_VER_MAJ 4
#define TARGET_ANDROID_VER_MIN 4

/* ---------------------------------------------------------- */
/* log */
#define XPaste(s) s
#define Paste2(a, b) XPaste(a)b
#define CAM_ID "[CAM_ID(%d)][%s]-"
#define ID_PARM m_cameraId, m_name

#define CLOGD(fmt, ...) \
    ALOGD(Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__)

#define CLOGV(fmt, ...) \
    ALOGV(Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__)

#define CLOGW(fmt, ...) \
    ALOGW(Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__)

#define CLOGE(fmt, ...) \
    ALOGE(Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__)

#define CLOGI(fmt, ...) \
    ALOGI(Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__)

#define CLOGT(cnt, fmt, ...) \
    if (cnt != 0) CLOGI(Paste2("#TRACE#", fmt), ##__VA_ARGS__) \

#define CLOG_ASSERT(fmt, ...) \
    android_printAssert(NULL, LOG_TAG, Paste2(CAM_ID, fmt), ID_PARM, ##__VA_ARGS__);

/* ---------------------------------------------------------- */
/* Align */
#define ROUND_UP(x, a)              (((x) + ((a)-1)) / (a) * (a))
#define ROUND_OFF_HALF(x, dig)      ((float)(floor((x) * pow(10.0f, dig) + 0.5) / pow(10.0f, dig)))
#define ROUND_OFF_DIGIT(x, dig)     ((uint32_t)(floor(((double)x)/((double)dig) + 0.5f) * dig))

/* ---------------------------------------------------------- */
/* Node Prefix */
#define NODE_PREFIX "/dev/video"

/* ---------------------------------------------------------- */
/* Max Camera Name Size */
#define EXYNOS_CAMERA_NAME_STR_SIZE (256)

/* ---------------------------------------------------------- */
#define SIZE_RATIO(w, h)         ((w) * 10 / (h))
#endif
