/*
 * Copyright (C) 2015, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_VISION_PERF_MONITOR_H
#define EXYNOS_VISION_PERF_MONITOR_H

#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Vector.h>
#include <map>

#include <VX/vx.h>

#include "ExynosVisionState.h"

namespace android {

using namespace std;

enum graph_time_pair {
    TIMEPAIR_PROCESS,

    GRAPH_TIMEPAIR_NUMBER
};

enum node_time_pair {
    TIMEPAIR_FRAMEWORK,
    TIMEPAIR_KERNEL,
    TIMEPAIR_FIRMWARE,

    NODE_TIMEPAIR_NUMBER
};

typedef struct _time_pair_t {
    vx_uint64 start;
    vx_uint64 end;
} time_pair_t;

#define TIMESTAMP_START(time_pair, index)  \
    if (time_pair) {    \
        time_pair[index].start = ExynosVisionDurationTimer::getTimeUs();    \
    }

#define TIMESTAMP_END(time_pair, index)  \
    if (time_pair) {    \
        time_pair[index].end = ExynosVisionDurationTimer::getTimeUs();    \
    }

#define TIMESTAMP_START_VALUE(time_pair, index, usec)  \
    if (time_pair) {    \
        time_pair[index].start = usec;    \
    }

#define TIMESTAMP_END_VALUE(time_pair, index, usec)  \
    if (time_pair) {    \
        time_pair[index].end = usec;    \
    }

class ExynosVisionStampElement {

enum graph_state {
    PERFELE_STATE_NO_LOCK = 1,
    PERFELE_STATE_LOCK = 2,
};

private:
    Mutex m_access_lock;

    vx_uint32 m_frame_number;
    ExynosVisionState m_state;
    time_pair_t *m_time_pair;

public:

private:

public:
    /* Constructor */
    ExynosVisionStampElement(uint32_t time_pair_num)
    {
        m_frame_number = 0;
        m_state.setState(PERFELE_STATE_NO_LOCK);
        m_time_pair = new time_pair_t[time_pair_num];
    }

    /* Destructor */
    virtual ~ExynosVisionStampElement()
    {
        delete m_time_pair;
    }

    time_pair_t* getStampStr(vx_uint32 frame_number)
    {
        Mutex::Autolock lock(m_access_lock);

        if (m_state.getState() != PERFELE_STATE_NO_LOCK) {
            ALOGE("[%s] stamp structure is already got", __FUNCTION__);
            return NULL;
        }

        m_frame_number = frame_number;
        m_state.setState(PERFELE_STATE_LOCK);
        return m_time_pair;
    }

    void putStampStr(vx_uint32 frame_number, time_pair_t *time_pair)
    {
        Mutex::Autolock lock(m_access_lock);

        if (m_state.getState() != PERFELE_STATE_LOCK)
            ALOGE("[%s] stamp structure is already put", __FUNCTION__);

        if ((frame_number != m_frame_number) || (time_pair != m_time_pair))
            ALOGE("[%s] returned stamp str is corrupted, expect:frame_%d, %p, received:frame_%d, %p", __FUNCTION__,
                m_frame_number, m_time_pair, frame_number, time_pair);

        m_state.setState(PERFELE_STATE_NO_LOCK);

        if (m_time_pair->start >= m_time_pair->end) {
            ALOGE("[%s] stamp value is invalid, start:%llu, end:%llu", __FUNCTION__, m_time_pair->start, m_time_pair->end);
            return;
        }
    }
};

template<typename T>
class ExynosVisionPerfMonitor {

#define MAX_TRACE_FRAME_NUM 100

typedef struct _perf_info_t {
    /* stamp vector for tracing several frames performance */
    Vector<ExynosVisionStampElement*> *stamp_vector;

    vx_perf_t *vx_perf_info;
} perf_info_t;

private:
    Mutex m_access_lock;

    map<T, perf_info_t*> m_perf_bunch_map;
    uint32_t m_timestamp_num;

public:

private:

public:
    /* Constructor */
    ExynosVisionPerfMonitor(void)
    {
        m_timestamp_num = 0;
    }

    /* Destructor */
    virtual ~ExynosVisionPerfMonitor()
    {
        typename map<T, perf_info_t*>::iterator  map_iter;
        Vector<ExynosVisionStampElement*>::iterator  frame_iter;

        for (map_iter=m_perf_bunch_map.begin(); map_iter!=m_perf_bunch_map.end(); map_iter++) {
            perf_info_t *object_perf = map_iter->second;
            if (object_perf) {
                for (frame_iter=object_perf->stamp_vector->begin(); frame_iter!=object_perf->stamp_vector->end(); frame_iter++) {
                    ExynosVisionStampElement *perf_elem = *frame_iter;
                    delete perf_elem;
                }
                delete object_perf->stamp_vector;
                delete object_perf->vx_perf_info;

                delete object_perf;
            }
        }

        m_perf_bunch_map.clear();
    }

    void registerObjectForTrace(T object, uint32_t timestamp_num)
    {
        Mutex::Autolock lock(m_access_lock);

        Vector<ExynosVisionStampElement*>*stamp_vector = new Vector<ExynosVisionStampElement*>;
        stamp_vector->setCapacity(MAX_TRACE_FRAME_NUM);

        ExynosVisionStampElement *perf_elem;
        for (vx_uint32 i=0; i<MAX_TRACE_FRAME_NUM; i++) {
            perf_elem = new ExynosVisionStampElement(timestamp_num);
            stamp_vector->push_back(perf_elem);
        }

        vx_perf_t *vx_perf_info = new vx_perf_t[timestamp_num];
        memset(vx_perf_info, 0x0, sizeof(vx_perf_t) * timestamp_num);

        perf_info_t *perf_info = new perf_info_t;
        perf_info->stamp_vector = stamp_vector;
        perf_info->vx_perf_info = vx_perf_info;

        m_perf_bunch_map[object] = perf_info;

        m_timestamp_num = timestamp_num;
    }

    void releaseObjectForTrace(T object)
    {
        Mutex::Autolock lock(m_access_lock);

        perf_info_t *object_perf = m_perf_bunch_map[object];
        if (object_perf) {
            Vector<ExynosVisionStampElement*>::iterator  frame_iter;
            for (frame_iter=object_perf->stamp_vector->begin(); frame_iter!=object_perf->stamp_vector->end(); frame_iter++) {
                ExynosVisionStampElement *perf_elem = *frame_iter;
                delete perf_elem;
            }
            delete object_perf->stamp_vector;
            delete object_perf->vx_perf_info;

            delete object_perf;
        } else {
            ALOGE("[%s] un-registered object", __FUNCTION__);
        }

        m_perf_bunch_map.erase(object);
    }

    time_pair_t* requestTimePairStr(T object, vx_uint32 frame_number)
    {
        m_access_lock.lock();
        perf_info_t *object_perf = m_perf_bunch_map[object];
        m_access_lock.unlock();

        if (object_perf == NULL) {
            ALOGE("[%s] perf str of unregistered object is accessed", __FUNCTION__);
            return NULL;
        }

        Vector<ExynosVisionStampElement*>*stamp_vector = object_perf->stamp_vector;
        ExynosVisionStampElement *perf_elem = stamp_vector->editItemAt(frame_number%MAX_TRACE_FRAME_NUM);;
        if (perf_elem == NULL) {
            ALOGE("[%s] perf str of unregistered object is accessed", __FUNCTION__);
            return NULL;
        }

        return perf_elem->getStampStr(frame_number);
    }

    void releaseTimePairStr(T object, vx_uint32 frame_number, time_pair_t *time_pair)
    {
        m_access_lock.lock();
        perf_info_t *object_perf = m_perf_bunch_map[object];
        m_access_lock.unlock();

        if (object_perf == NULL) {
            ALOGE("[%s] perf str of unregistered object is accessed", __FUNCTION__);
            return;
        }

        Vector<ExynosVisionStampElement*>*stamp_vector = object_perf->stamp_vector;
        ExynosVisionStampElement *perf_elem = stamp_vector->editItemAt(frame_number%MAX_TRACE_FRAME_NUM);;
        if (perf_elem == NULL) {
            ALOGE("[%s] perf str of unregistered object is accessed", __FUNCTION__);
            return;
        }

        updateVxPerfInfo(object_perf->vx_perf_info, time_pair);
        perf_elem->putStampStr(frame_number, time_pair);
    }

    void updateVxPerfInfo(vx_perf_t *vx_perf_info, time_pair_t *time_pair)
    {
        for (uint32_t i=0; i<m_timestamp_num; i++) {
            vx_perf_info[i].beg = time_pair[i].start;
            vx_perf_info[i].end = time_pair[i].end;

            vx_uint64 duration = time_pair[i].end -time_pair[i].start;
            if (vx_perf_info[i].num == 0) {
                vx_perf_info[i].min = duration;
                vx_perf_info[i].max = duration;
            }

            vx_perf_info[i].tmp = duration;
            vx_perf_info[i].sum += duration;
            vx_perf_info[i].num++;
            vx_perf_info[i].avg = vx_perf_info[i].sum / vx_perf_info[i].num;

            if (vx_perf_info[i].min > duration)
                vx_perf_info[i].min = duration;
            if (vx_perf_info[i].max < duration)
                vx_perf_info[i].max = duration;
        }
    }

    vx_perf_t* getVxPerfInfo(T object)
    {
        m_access_lock.lock();
        perf_info_t *object_perf = m_perf_bunch_map[object];
        m_access_lock.unlock();

        if (object_perf != NULL)
            return object_perf->vx_perf_info;
        else
            return NULL;
    }

    void displayPerfInfo(void)
    {
        typename map<T, perf_info_t*>::iterator  map_iter;

        for (map_iter=m_perf_bunch_map.begin(); map_iter!=m_perf_bunch_map.end(); map_iter++) {
            T object = map_iter->first;
            perf_info_t *object_perf = map_iter->second;
            vx_perf_t *vx_perf = object_perf->vx_perf_info;

            ALOGD("[%s, %s] beg:%llu, end:%llu, avg:%llu, min:%llu, max:%llu (num:%llu)", object->getName(), object->getKernelName(),
                vx_perf->beg/1000, vx_perf->end/1000, vx_perf->avg/1000, vx_perf->min/1000, vx_perf->max/1000, vx_perf->num/1000);
        }
    }
};

}; // namespace android
#endif
