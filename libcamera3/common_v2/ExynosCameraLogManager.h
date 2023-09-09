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
 * \file      ExynosCameraLogManager.h
 * \brief     header file for ExynosCameraLogManager
 * \author    Teahyung, Kim(tkon.kim@samsung.com)
 * \date      2018/05/08
 *
 * <b>Revision History: </b>
 * - 2018/05/08 : Teahyung, Kim(tkon.kim@samsung.com) \n
 */

#ifndef EXYNOS_CAMERA_LOG_MANAGER_H
#define EXYNOS_CAMERA_LOG_MANAGER_H

#ifdef USE_DEBUG_PROPERTY

#include <map>
#include <utils/Log.h>
#include <utils/Mutex.h>
#include <utils/Timers.h>
#include <utils/String8.h>
#include <utils/RefBase.h>

#include "ExynosCameraConfig.h"
#include "ExynosCameraCommonDefine.h"
#include "ExynosCameraCommonEnum.h"

using namespace android;

class ExynosCameraProperty;

/**
 * ExynosCameraLogManager:
 * ExynosCameraLogManager is the class to manage all kind of logs in CameraHAL.
 * This Class can't be create by a instance. It should be used by accessing to
 * static method. We provide some macro function like CLOG_COMMON, CLOG_PERFRAME,
 * CLOG_PERFORMANCE in ExynosCameraCommonDefine.h.
 * There are some example like following.
 * This class has one instance of ExynosCameraProperty class. You can refer to
 * more details in ExynosCameraProperty.class for examples.
 *
 * ex.
 *  CLOGD("Test");
 *  CLOG_PERFRAME(META, m_cameraId, m_name, frame.get(), &shot_ext->shot,
 *                frame->getRequestKey(), "");
 *  CLOG_PERFORMANCE(FPS, factory->getCameraId(),
 *                   factory->getFactoryType(), DURATION,
 *                   FRAME_CREATE, 0, request->getKey(), nullptr);
 */
class ExynosCameraLogManager
{
public:

    /* Type of Perframe log */
    typedef enum  {
        PERFRAME_TYPE_SIZE,
        PERFRAME_TYPE_PATH,
        PERFRAME_TYPE_BUF,
        PERFRAME_TYPE_META,
        PERFRAME_TYPE_MAX,
    } PerframeType;

    /* Type of Performance log */
    typedef enum {
        PERFORMANCE_TYPE_FPS,
        PERFORMANCE_TYPE_MAX
    } PerformanceType;

    /* Position Tag of Performance log */
    typedef enum {
        PERF_FPS_SERVICE_REQUEST,
        PERF_FPS_FRAME_CREATE,
        PERF_FPS_PATIAL_RESULT,
        PERF_FPS_FRAME_COMPLETE,
        PERF_FPS_ALL_RESULT,
        PERFORMANCE_POS_MAX
    } PerformancePos;

    /* Type for Meta Perframe control */
    typedef enum {
        TYPE_INT8,
        TYPE_INT16,
        TYPE_INT32,
        TYPE_INT64,
        TYPE_FLOAT,
        TYPE_DOUBLE,
        TYPE_RATIONAL,
        TYPE_STRUCT,
        TYPE_MAX,
    } MetaFieldType;

    /* Meta of S.LSI's Metadata to map with string from get_property() */
    class MetaMapper : public RefBase {
    public:
        explicit MetaMapper(const char *pStr, MetaFieldType pType, long pOffset, uint32_t pArraySize) :
            str(pStr), type(pType), offset(pOffset), arraySize(pArraySize) {
        }

        void removeChild(std::map<std::string, MetaMapper *> &child) {
            if (child.size() == 0) return;

            std::map<std::string, MetaMapper *>::iterator it;
            it = child.begin();
            for (; it != child.end(); ++it) {
                MetaMapper *remove = it->second;
                if (remove != nullptr) {
                    removeChild(remove->child); //recursive
                    child.erase(it);
                    delete remove;
                }
            }
        }

        ~MetaMapper() {
            removeChild(child);
        }

    public:
        const char *str;
        MetaFieldType type;
        long offset;
        uint32_t arraySize;
        std::map<std::string, MetaMapper *> child;
    };

    /* The way to measure performance */
    typedef enum {
        TM_INTERVAL,
        TM_DURATION,
        /**
         * TBD
         * CUMUL_CNT : to count error or specific events
         * USER_DATA : some specific record
         */
        TM_CUMUL_CNT,
        TM_USER_DATA,
        TM_MAX,
    } TimeCalcType;

    /* basic timeunit for performance */
    typedef struct {
        int32_t                count;
        nsecs_t                min;
        nsecs_t                max;
        nsecs_t                sum;
        nsecs_t                lastTimestamp;
    } TimeUnitInfo;

    /* specific timesubunit for performance like all pipe's performance */
    class TimeSubInfo : public RefBase {
    public:
        std::map<int32_t, TimeUnitInfo> duration;
        std::map<int32_t, TimeUnitInfo> interval;
    };

    typedef sp<TimeSubInfo> TimeSubInfo_sp_t;

    /* summary for time performance */
    class TimeInfo {
    public:
        TimeInfo() {
            index = 0;
            count = 0;
            errCnt = 0;
            memset(&time, 0x0, sizeof(TimeUnitInfo));
            sub = new TimeSubInfo;
        }

    public:
        uint32_t               index;
        uint32_t               count;
        uint32_t               errCnt;
        TimeUnitInfo           time;
        /* sub infomation eg.each entry(pipe)'s info <subkey, TimeUnit> */
        TimeSubInfo_sp_t       sub;
    };


    /* init */
    static void init();

    /**
     * Print basic log.
     * But by searching m_property and matching the values,
     * some log can't be printed or this process can be panic by
     * enabling trap feature.
     *  @prio   : android log level
     *  @tag    : LOG_TAG for android log print
     *  @prefix : prefix string literal(for cameraId, instance name)
     *  @cameraId : camera device id (ex. back : 0, front : 1)
     *  @name : instance name to call
     *  @func : function name to call
     *  @line : linenumber to call
     *  @fmt : string literal for print
     * eg.
     *  logCommon(ANDROID_LOG_VERBOSE, LOG_TAG, "cam(%s), name(%s)",
     *            m_cameraId, m_name, __FUNCTION__, __LINE__,
     *            "Test log(%d)", i);
     *
     *  <Property>
     *  1. For verborse level log print
     *   # setprop log.tag."tag" V
     *   # setprop persist.log.tag."tag" V   //still keep it's value maintain after reboot
     *
     *  2. Enable Trap for debuging
     *   # setprop sys.camera.debug.trap.enable true
     *   # setprop sys.camera.debug.trap.words "fatal error|device error"
     *      //User can assign multiple string to property by delimeter "|"
     *   # setprop sys.camera.debug.trap.event "panic"
     *      //panic : kernel panic
     *      //assert : process kill
     *   # setprop sys.camera.debug.trap.count 5
     *      //after decreasing this number to 0, trap would be triggered
     *
     *  3. Change the assert action
     *   # setprop sys.camera.assert "errlog"
     *      //errlog : just print log by error level
     *      //assert : process kill(same as normal)
     *      //panic : kernel panic
     */
    static void logCommon(android_LogPriority prio,
            const char* tag,
            const char* prefix,
            int cameraId,
            const char* name,
            const char* func,
            int line,
            const char* fmt,
            ...);

    /**
     * Print perframe log.
     * Now that this class support perframe log about path, buf, meta or
     * size. Most of time, it's better that caller gives the frame to this
     * function, because this function print the frame's information by default.
     *  @prio   : android log level
     *  @tag    : LOG_TAG for android log print
     *  @prefix : prefix string literal(for cameraId, instance name)
     *  @cameraId : camera device id (ex. back : 0, front : 1)
     *  @name : instance name to call
     *  @func : function name to call
     *  @line : linenumber to call
     *  @fmt : string literal for print
     *  @type : type of perframe log (size, buf, path, meta)
     *  @frame : ExynosCameraFrame pointer
     *  @data : specific object for perframe log type
     *    - size : nullptr
     *    - buf : camera3_stream_buffer pointer
     *    - meta : camera2_shot buffer pointer
     *    - path : nullptr
     *
     * eg.
     *  CLOG_PERFRAME(META, m_cameraId, m_name, frame.get(), &shot_ext->shot,
     *                frame->getRequestKey(), "");
     *  CLOG_PERFRAME(BUF, m_cameraId, m_name, nullptr, (void *)(pStreamBuf),
     *                request->frame_number, "[INPUT]");
     *  CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame, nullptr, frame->getRequestKey(),
     *               "%s", str.c_str());
     *  CLOG_PERFRAME(PATH, m_cameraId, m_name, newFrame.get(), nullptr, newFrame->getRequestKey(), "");
     *
     *  <Property>
     *  1. Path
     *   # setprop sys.camera.log.perframe.enable true
     *   # setprop sys.camera.log.perframe.path.enable true
     *
     *  2. Size
     *   # setprop sys.camera.log.perframe.enable true
     *   # setprop sys.camera.log.perframe.size.enable true
     *
     *  3. Buf
     *   # setprop sys.camera.log.perframe.enable true
     *   # setprop sys.camera.log.perframe.buf.enable true
     *   # setprop sys.camera.log.perframe.buf.filter "MCSC_OUT_BUF|DEPTH_MAP_BUF"
     *
     *  4. Meta
     *   * Only the field that user set in "filter" property can be only printed.
     *   # setprop sys.camera.log.perframe.enable true
     *   # setprop sys.camera.log.perframe.meta.enable true
     *   # setprop sys.camera.log.perframe.meta.filter "ctl.aa.afMode|ctl.aa.aeTargetFpsRange|ctl.aa.vendor_isoMode"
     *     //print ctl.aa.afMode, ctl.aa.aeTargetFpsRange and ctl.aa.vendor_isoMode
     *   # setprop sys.camera.log.perframe.meta.values "1|15,30|100"
     *     //forcely value setting by fields on filter
     *     ctl.aa.afMode = 1
     *     ctl.aa.aeTargetFpsRange[0] = 15
     *     ctl.aa.aeTargetFpsRange[1] = 30
     *     ctl.aa.vendor_isoMode = 100
     *
     *  5. Filtering
     *   # setprop sys.camera.log.perframe.filter.cameraId "0|1"
     *     //print the log if cameraId is 0 or 1
     *   # setprop sys.camera.log.perframe.filter.factory "reproc|jpeg_reproc"
     *     //"reproc"      : FRAME_FACTORY_TYPE_REPROCESSING
     *     //"jpeg_reproc" : FRAME_FACTORY_TYPE_JPEG_REPROCESSING
     *     //"cap_prev"    : FRAME_FACTORY_TYPE_CAPTURE_PREVIEW
     *     //"prev_dual"   : FRAME_FACTORY_TYPE_PREVIEW_DUAL
     *     //"reproc_dual" : FRAME_FACTORY_TYPE_REPROCESSING_DUAL
     *     //"vision"      : FRAME_FACTORY_TYPE_VISION
     */
    static void logPerframe(android_LogPriority prio,
            const char* tag,
            const char* prefix,
            int cameraId,
            const char* name,
            const char* func,
            int line,
            const char* fmt,
            PerframeType type,
            void *frame,
            void *data,
            uint32_t requestKey,
            ...);

    /**
     * Print performance log.
     * print performance log like fps by summarizing of N frames.
     *  @prio   : android log level
     *  @tag    : LOG_TAG for android log print
     *  @prefix : prefix string literal(for cameraId, instance name)
     *  @cameraId : camera device id (ex. back : 0, front : 1)
     *  @factoryType : factory instance to call
     *  @pType : type of performance log
     *  @pos : position tag
     *  @posSubKey : sub key like pipeId
     *  @key : unique key like request key
     *  @data : some specific object like ExynosCameraFrame pointer
     *
     * eg.
     *  CLOG_PERFORMANCE(FPS, getCameraId(),
     *          getFactoryType(), DURATION,
     *          FRAME_COMPLETE, 0, getRequestKey(), this);
     *
     *  <Property>
     *  1. Fps
     *   # setprop sys.camera.log.performance.enable true
     *   # setprop sys.camera.log.performance.fps.enable true
     *   # setprop sys.camera.log.performance.frames 120
     *     // summary based on this N frames (in this ex, 120 frames)
     */
    static void logPerformance(android_LogPriority prio,
            const char* tag,
            const char* prefix,
            int cameraId,
            int factoryType,
            PerformanceType pType,
            TimeCalcType tType,
            int pos,
            __unused int posSubKey,  /* eg.pipeId */
            int key,        /* eg.requestKey */
            void *data      /* eg.ExynosCameraFrame */
            ...);

protected:
    static void m_panic(void);
    static void m_printLog(android_LogPriority prio, const char *tag, String8& logStr);
    static void m_logDebug(android_LogPriority &prio, const char *tag, String8& logStr);

    /**
     * Performance sub function
     */
    static void m_logPerformanceFps(android_LogPriority prio,
            const char* tag,
            const char* prefix,
            int cameraId,
            int factoryType,
            PerformanceType pType,
            TimeCalcType tType,
            int pos,
            int requestKey,
            void *frame);

    static void m_initMetaMapper(void);
private:
    ExynosCameraLogManager(); /* can't create an instance */

    static ExynosCameraProperty m_property;

    /**
     * For performance Lock
     */
    static Mutex m_perfLock[PERFORMANCE_TYPE_MAX];
    /**
     * Timestamp key value(64bit)
     * Refer to function "m_makeTimestampkey" to make key
     */
    static std::map<int64_t, nsecs_t> m_perfTimestamp[PERFORMANCE_TYPE_MAX][PERFORMANCE_POS_MAX];
    /**
     * Summary key value(32bit)
     * For Summary of statistics
     * Refer to function "m_makeSummaryKey" to make key
     */
    static std::map<int32_t, TimeInfo> m_perfSummary[PERFORMANCE_TYPE_MAX];

    /* Mata Mapper */
    static MetaMapper kLv1MetaMap;
};
#endif //USE_DEBUG_PROPERTY
#endif //EXYNOS_CAMERA_LOG_MANAGER_H
