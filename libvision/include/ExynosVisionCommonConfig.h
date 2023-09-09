#ifndef EXYNOS_OPENVX_COMMON_CONFIG_H__
#define EXYNOS_OPENVX_COMMON_CONFIG_H__

#include <math.h>

#include <cutils/log.h>

/* ---------------------------------------------------------- */
/* log */
#define XPaste(s) s
#define Paste2(a, b) XPaste(a)b
#define ID "[VX]"
#define ID2 "[VX][%s] "
#define ID3 "[VX][%s,%d] "

#define VXDBGMARKER(fmt, ...) \
    ALOGD(Paste2(ID3, fmt), __FUNCTION__, __LINE__, ##__VA_ARGS__)

#if 0
#define VXLOGD1(fmt, ...) \
    ALOGD(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)
#else
#define VXLOGD1(fmt, ...)
#endif

#if 0
#define VXLOGD2(fmt, ...) \
    ALOGD(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)
#else
#define VXLOGD2(fmt, ...)
#endif

#if 0
#define VXLOGD3(fmt, ...) \
    ALOGD(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)
#else
#define VXLOGD3(fmt, ...)
#endif

#if 1
#define VXLOGD(fmt, ...) \
    ALOGD(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)
#else
#define VXLOGD(fmt, ...)
#endif

#define VXLOGV(fmt, ...) \
    ALOGV(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)

#define VXLOGW(fmt, ...) \
    ALOGW(Paste2(ID2, fmt), __FUNCTION__, ##__VA_ARGS__)

#define VXLOGE(fmt, ...) \
    ALOGE(Paste2(ID3, fmt), __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define VXLOGI(fmt, ...) \
    ALOGI(Paste2(ID, fmt), ##__VA_ARGS__)

#define VXLOGT(cnt, fmt, ...) \
    if (cnt != 0) CLOGI(fmt, ##__VA_ARGS__) \

#define CLOG_ASSERT(fmt, ...) \
    android_printAssert(NULL, LOG_TAG, fmt, ##__VA_ARGS__);

/* #define LOG_FLUSH_TIME()  usleep(5*1000); */
#define LOG_FLUSH_TIME()

/* #define EXYNOS_VISION_API_TRACE */
#ifdef EXYNOS_VISION_API_TRACE
#define EXYNOS_VISION_API_IN()   VXLOGD("IN..")
#define EXYNOS_VISION_API_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_API_IN()   ((void *)0)
#define EXYNOS_VISION_API_OUT()  ((void *)0)
#endif

/* #define EXYNOS_VISION_REF_TRACE */
#ifdef EXYNOS_VISION_REF_TRACE
#define EXYNOS_VISION_REF_IN()   VXLOGD("IN..")
#define EXYNOS_VISION_REF_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_REF_IN()   ((void *)0)
#define EXYNOS_VISION_REF_OUT()  ((void *)0)
#endif

/* #define EXYNOS_VISION_SYSTEM_TRACE */
#ifdef EXYNOS_VISION_SYSTEM_TRACE
#define EXYNOS_VISION_SYSTEM_IN()   VXLOGD("IN..")
#define EXYNOS_VISION_SYSTEM_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_SYSTEM_IN()   ((void *)0)
#define EXYNOS_VISION_SYSTEM_OUT()  ((void *)0)
#endif

/* #define EXYNOS_VISION_DATA_TRACE */
#ifdef EXYNOS_VISION_DATA_TRACE
#define EXYNOS_VISION_DATA_IN()   VXLOGD("IN..")
#define EXYNOS_VISION_DATA_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_DATA_IN()   ((void *)0)
#define EXYNOS_VISION_DATA_OUT()  ((void *)0)
#endif

/* #define EXYNOS_VISION_BUF_TRACE */
#ifdef EXYNOS_VISION_BUF_TRACE
#define EXYNOS_VISION_BUF_IN()   VXLOGD("IN..")
#define EXYNOS_VISION_BUF_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_VISION_BUF_IN()   ((void *)0)
#define EXYNOS_VISION_BUF_OUT()  ((void *)0)
#endif

#define MAX_TAB_NUM     20

#define MAKE_TAB(tab, tab_num)   ({  \
    memset(tab, 0x0, sizeof(tab));  \
    memset(tab, '\t', (tab_num<(MAX_TAB_NUM-1)) ? tab_num : (MAX_TAB_NUM-1)); \
    })

/* ---------------------------------------------------------- */
/* Node Prefix */
#define NODE_PREFIX "/dev/vertex0"

/* ---------------------------------------------------------- */
/* Linux type */
#ifndef _LINUX_TYPES_H
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
/*typedef unsigned long long uint64_t;*/
#endif

#endif /* EXYNOS_OPENVX_COMMON_CONFIG_H__ */

