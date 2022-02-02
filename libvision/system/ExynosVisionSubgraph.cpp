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

#define LOG_TAG "ExynosVisionSubgraph"
#include <cutils/log.h>

#include "ExynosVisionAutoTimer.h"

#include "ExynosVisionSubgraph.h"

#include "ExynosVisionGraph.h"
#include "ExynosVisionBufObject.h"

#define BIT_FLAG(i) ((1<<i))

#define ID_THREAD "[TD][%s][%s] "

/* #define EXYNOS_VISION_THREAD_TRACE */
#ifdef EXYNOS_VISION_THREAD_TRACE
#define VXLOGTD(fmt, ...) \
    ALOGD(Paste2(ID_THREAD, fmt), __FUNCTION__, m_sg_name,##__VA_ARGS__)
#else
#define VXLOGTD(fmt, ...)
#endif

namespace android {

ExynosVisionSubgraph::ExynosVisionSubgraph(ExynosVisionGraph *graph)
{
    m_id = 0;
    memset(&m_sg_name, 0x0, sizeof(m_sg_name));

    m_graph = graph;

    m_represent_node = NULL;

    m_params = NULL;
    m_message_queue = NULL;

    m_thread_state.setState(THREAD_STATE_NOT_START);

    m_target_done_bitmask = 0;
    m_param_num = 0;

    m_complete_event = NULL;

    m_last_process_frame = 0;
}

ExynosVisionSubgraph::~ExynosVisionSubgraph(void)
{

}

vx_status
ExynosVisionSubgraph::init(ExynosVisionNode *node)
{
    m_represent_node = node;
    m_id = node->getId();

    sprintf(m_sg_name, "SG_%d-%s", m_id, m_represent_node->getKernelHandle()->getKernelFuncName());

    m_node_list.push_back(node);

    m_complete_event = new ExynosVisionEvent(COMPLETE_WAIT_TIME);

    return VX_SUCCESS;
}

vx_status
ExynosVisionSubgraph::destroy(void)
{
    vx_status status = VX_SUCCESS;

    VXLOGD3("%s destroy", getSgName());

    VXLOGTD("trying stop thread, thread_status: %d", m_thread_state.getState());
    exitThread();

    if (m_message_queue) {
        m_message_queue->release();
        delete m_message_queue;
    }

    if (m_params)
        delete m_params;

    if (m_complete_event) {
        delete m_complete_event;
        m_complete_event = NULL;
    }

    return status;
}

vx_status
ExynosVisionSubgraph::makeInputOutputPort(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;

    m_target_done_bitmask = 0;

    m_input_data_ref_list.clear();
    m_output_data_ref_list.clear();

    for (vx_uint32 p = 0; p < m_represent_node->getDataRefNum(); p++) {
        ExynosVisionDataReference *data_ref = m_represent_node->getDataRefByIndex(p);
        if ((data_ref == NULL) && m_represent_node->getKernelHandle()->getParamState(p) == VX_PARAMETER_STATE_REQUIRED) {
            VXLOGE("%s(%s) does not have necessary parameter[%d]", m_represent_node->getName(), m_represent_node->getKernelName(), p);
        }

        ref_connect_info_t connect_info;
        connect_info.ref = data_ref;
        connect_info.node_index = p;

        if (m_represent_node->getKernelHandle()->getParamDirection(p) == VX_INPUT) {
            m_input_data_ref_list.push_back(connect_info);

            /* exclusive object doesn't need to receive doen event */
            if ((data_ref != NULL) &&
                ((data_ref->getIndirectInputNodeNum(m_graph) != 0) || (data_ref->isQueue()))) {
                m_target_done_bitmask |= BIT_FLAG(p);
            }
        } else {
            m_output_data_ref_list.push_back(connect_info);
        }
    }

    m_param_num = m_input_data_ref_list.size() + m_output_data_ref_list.size();
    m_params = new ExynosVisionDataReference*[m_param_num];

    EXYNOS_VISION_SYSTEM_OUT();

    return VX_SUCCESS;
}

vx_status
ExynosVisionSubgraph::replaceDataRef(ExynosVisionDataReference *old_ref, ExynosVisionDataReference *new_ref, vx_uint32 node_index, enum vx_direction_e dir)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_FAILURE;
    vx_bool result = vx_false_e;

    List<ExynosVisionNode*>::iterator node_iter;
    List<ref_connect_info_t>::iterator ref_iter;

    List<ref_connect_info_t> *data_ref_list;

    Mutex::Autolock lock(m_exec_mutex);

    vx_enum state = m_thread_state.getState();
    if (state != THREAD_STATE_WAIT_DONE) {
        VXLOGE("cannot change parameter during subgrapn is working, state:%d", state);
        status = VX_FAILURE;
        goto EXIT;
    }

    if (old_ref == NULL || new_ref == NULL) {
        VXLOGE("parameter contain null, old_ref:%p, new_ref:%p", old_ref, new_ref);
        status = VX_FAILURE;
        goto EXIT;
    }

    if (dir == VX_INPUT)
        data_ref_list = &m_input_data_ref_list;
    else
        data_ref_list = &m_output_data_ref_list;

    for (ref_iter=data_ref_list->begin(); ref_iter!=data_ref_list->end(); ref_iter++) {
        if (((*ref_iter).ref == old_ref) && ((*ref_iter).node_index == node_index)){
            ref_iter = data_ref_list->erase(ref_iter);

            ref_connect_info_t connect_info = { new_ref, node_index};
            data_ref_list->insert(ref_iter, connect_info);
            result = vx_true_e;
            break;
        }
    }

    if (result != vx_true_e) {
        VXLOGE("can't replace %s to %s at %s", old_ref->getName(), new_ref->getName(), this->getSgName());

        for (ref_iter=data_ref_list->begin(); ref_iter!=data_ref_list->end(); ref_iter++) {
            VXLOGD("ref:%s, node_index:%d", (*ref_iter).ref->getName(), (*ref_iter).node_index);
        }
        m_represent_node->displayInfo(0, vx_true_e);

        status = VX_FAILURE;
    } else {
        status = VX_SUCCESS;
    }

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionSubgraph::allocateDataRefMemory(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;

    /* allocation reference object memory */
    for (vx_uint32 p = 0; p < m_represent_node->getDataRefNum(); p++) {
        ExynosVisionDataReference *data_ref = m_represent_node->getDataRefByIndex(p);
        if (data_ref == NULL)
            continue;

        if (data_ref->isAllocated() == vx_false_e) {
            VXLOGD2("%s, allocating memory", data_ref->getName());
            status = data_ref->allocateMemory();
            if (status != VX_SUCCESS)
                VXLOGE("data_ref(%s) allocation memory fail, error:%d", data_ref->getName(), status);
        } else {
            VXLOGD2("%s, memory is already allocated", data_ref->getName());
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionSubgraph::fixSubgraph(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;

    status = makeInputOutputPort();
    if (status != VX_SUCCESS) {
        VXLOGE("making port fails, err:%d", status);
    }

    status = allocateDataRefMemory();
    if (status != VX_SUCCESS) {
        VXLOGE("allocating memory fails, err:%d", status);
    }

    if (m_graph->getPerfMonitor() != NULL) {
        m_graph->getPerfMonitor()->registerObjectForTrace(m_represent_node, NODE_TIMEPAIR_NUMBER);
    } else {
        VXLOGE("performance monitor is not assigned");
    }

    m_last_process_frame = 0;

    m_message_queue = new sg_msg_queue_t(0);
    m_main_thread = new ExynosVisionThread<ExynosVisionSubgraph>(this, &ExynosVisionSubgraph::mainThreadFunc, "subgraph", PRIORITY_DEFAULT);
    m_main_thread->run();

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_bool
ExynosVisionSubgraph::verifyPopedEvent(ExynosVisionDataReference *data_ref, vx_uint32 node_index)
{
    vx_uint32 i;
    vx_bool result = vx_false_e;

    List<ref_connect_info_t>::iterator ref_iter;
    for (ref_iter=m_input_data_ref_list.begin(), i=0; ref_iter!=m_input_data_ref_list.end(); ref_iter++, i++) {
        if (((*ref_iter).ref == data_ref) && ((*ref_iter).node_index == node_index)) {
            result = vx_true_e;
            break;
        }
    }

    if (result != vx_true_e) {
        VXLOGE("[%s] data reference is not found", getSgName());
        VXLOGD("ref:%s, node_index:%d", data_ref->getName(), node_index);
        m_represent_node->displayInfo(0, vx_true_e);

        for (ref_iter=m_input_data_ref_list.begin(), i=0; ref_iter!=m_input_data_ref_list.end(); ref_iter++, i++)
            VXLOGD("input_list, ref:%s, node_index:%d", (*ref_iter).ref->getName(), (*ref_iter).node_index);
    }

    return result;
}

vx_status
ExynosVisionSubgraph::pushDoneEvent(vx_uint32 frame_cnt, ExynosVisionDataReference *ref, vx_uint32 node_index)
{
    subgraph_message_t sg_msg;
    sg_msg.type = SG_MESSAGE_DONE_EVENT;
    sg_msg.frame_cnt = frame_cnt;
    sg_msg.done_reference = ref;
    sg_msg.node_index = node_index;

    VXLOGTD("push done event: %s, frame(%d)", ref->getName(), frame_cnt);

    m_message_queue->pushProcessQ(&sg_msg);

    return VX_SUCCESS;
}

vx_status
ExynosVisionSubgraph::pushTrigger(vx_uint32 frame_cnt)
{
    subgraph_message_t sg_msg;
    sg_msg.type = SG_MESSAGE_TRIGGER;
    sg_msg.frame_cnt = frame_cnt;
    sg_msg.done_reference = NULL;
    sg_msg.node_index = 0;

    VXLOGTD("push trigger:frame_%d", frame_cnt);

    m_message_queue->pushProcessQ(&sg_msg);

    return VX_SUCCESS;
}

queue_exception_t
ExynosVisionSubgraph::popDoneEvent(vx_uint32 *ret_frame_cnt)
{
    vx_uint32 ready_frame_cnt = 0;

    subgraph_message_t sg_msg;
    memset(&sg_msg, 0x0, sizeof(sg_msg));
    queue_exception_t exception;
    status_t ret = NO_ERROR;

    exception = m_message_queue->waitAndPopProcessQ(&sg_msg);

    if (exception == QUEUE_EXCEPTION_TIME_OUT) {
        VXLOGE("queue fails, ret:%d", exception);
        goto EXIT;
    }
    else if (exception != QUEUE_EXCEPTION_NONE) {
        VXLOGD3("queue returns some exception, exceptiont:%d", exception);
        goto EXIT;
    }

    if (sg_msg.type == SG_MESSAGE_TRIGGER) {
        if (m_target_done_bitmask != 0x0) {
            VXLOGE("Cannot receiving trigger when bitmask is %p, %s", m_target_done_bitmask, getSgName());
            ready_frame_cnt = 0;
            goto EXIT;
        }

        ready_frame_cnt = sg_msg.frame_cnt;
    } else {
        while(1) {
            if (verifyPopedEvent(sg_msg.done_reference, sg_msg.node_index) != vx_true_e) {
                VXLOGE("poped event doesn't match input reference information");
                break;
            }

            m_ready_bitmask_map[sg_msg.frame_cnt] |= BIT_FLAG(sg_msg.node_index);
            VXLOGTD("pop done: %s, frame(%d), ready_bitmask:%p, target_bitmask:%p", sg_msg.done_reference->getName(), sg_msg.frame_cnt,
                                                                                                                               m_ready_bitmask_map[sg_msg.frame_cnt], m_target_done_bitmask);

            if (m_ready_bitmask_map[sg_msg.frame_cnt] == m_target_done_bitmask) {
                ready_frame_cnt = sg_msg.frame_cnt;
                m_ready_bitmask_map.erase(ready_frame_cnt);
                break;
            }

            ret = m_message_queue->popProcessQ(&sg_msg);
            if (ret == TIMED_OUT)
                break;
        }
    }

EXIT:
    if (exception == QUEUE_EXCEPTION_NONE) {
        *ret_frame_cnt = ready_frame_cnt;
   }
    else {
        *ret_frame_cnt = 0;
    }

    return exception;
}

vx_status
ExynosVisionSubgraph::getSrcRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode, vx_bool *ret_data_valid)
{
    vx_status status = VX_SUCCESS;
    *ret_data_valid = vx_true_e;

    List<ref_connect_info_t>::iterator ref_iter;
    m_cur_input_data_ref_list.clear();

    /* prevent to get shared reference several times */
    map<ExynosVisionDataReference*, ExynosVisionDataReference*> ref_clone_map;

    for (ref_iter=m_input_data_ref_list.begin(); ref_iter!=m_input_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_represent;
        ExynosVisionDataReference* ref_clone;

        ref_represent = (*ref_iter).ref;
        if (ref_represent == NULL) {
            /* null parameter, it could be optional parameter */
            ref_connect_info_t connect_info = {NULL, (*ref_iter).node_index};
            m_cur_input_data_ref_list.push_back(connect_info);
            continue;
        }

        if ((exec_mode == GRAPH_EXEC_NORMAL) || (ref_represent->getDirectInputNodeNum(m_graph) == 0)) {
            VXLOGTD("trying to get exclusive from %s, frame_%d", ref_represent->getName(), frame_cnt);
            ref_clone = ref_represent->getInputExclusiveRef(frame_cnt, ret_data_valid);
            if (ref_clone == NULL) {
                VXLOGE("cann't get data reference from %s", ref_represent->getName());
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                ref_clone ->increaseKernelCount();
                ref_connect_info_t connect_info = {ref_clone, (*ref_iter).node_index};
                m_cur_input_data_ref_list.push_back(connect_info);
            }
        } else {
            VXLOGTD("trying to get share from %s, frame_%d", ref_represent->getName(), frame_cnt);
            if (ref_clone_map[ref_represent] == NULL) {
                ref_clone = ref_represent->getInputShareRef(frame_cnt, ret_data_valid);
                ref_clone_map[ref_represent] = ref_clone;
            } else {
                ref_clone = ref_clone_map[ref_represent];
            }

            if (ref_clone == NULL) {
                VXLOGE("cann't get data reference from %s", (*ref_iter).ref->getName());
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                ref_clone ->increaseKernelCount();
                ref_connect_info_t connect_info = {ref_clone, (*ref_iter).node_index};
                m_cur_input_data_ref_list.push_back(connect_info);
            }
        }
    }

    return status;
}

vx_status
ExynosVisionSubgraph::getDstRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode)
{
    vx_status status = VX_SUCCESS;
    List<ref_connect_info_t>::iterator ref_iter;
    m_cur_output_data_ref_list.clear();

    /* prevent to get shared reference several times */
    map<ExynosVisionDataReference*, ExynosVisionDataReference*> ref_clone_map;

    for (ref_iter=m_output_data_ref_list.begin(); ref_iter!=m_output_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_represent;
        ExynosVisionDataReference* ref_clone;

        ref_represent = (*ref_iter).ref;
        if (ref_represent == NULL) {
            /* null parameter, it could be optional parameter */
            ref_connect_info_t connect_info = {NULL, (*ref_iter).node_index};
            m_cur_output_data_ref_list.push_back(connect_info);
            continue;
        }

        if ((exec_mode == GRAPH_EXEC_NORMAL) || (ref_represent->getDirectOutputNodeNum(m_graph) == 0)) {
            VXLOGTD("trying to get exclusive from %s, frame_%d", ref_represent->getName(), frame_cnt);
            ref_clone = ref_represent->getOutputExclusiveRef(frame_cnt);
            if (ref_clone == NULL) {
                VXLOGE("cann't get data reference from %s", ref_represent->getName());
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                ref_clone ->increaseKernelCount();
                ref_connect_info_t connect_info = {ref_clone, (*ref_iter).node_index};
                m_cur_output_data_ref_list.push_back(connect_info);
            }
        } else {
            VXLOGTD("trying to get share from %s, frame_%d", ref_represent->getName(), frame_cnt);
            if (ref_clone_map[ref_represent] == NULL) {
                ref_clone = ref_represent->getOutputShareRef(frame_cnt);
                ref_clone_map[ref_represent] = ref_clone;
            } else {
                ref_clone = ref_clone_map[ref_represent];
            }

            if (ref_clone == NULL) {
                VXLOGE("cann't get data reference from %s", ref_represent->getName());
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                ref_clone ->increaseKernelCount();
                ref_connect_info_t connect_info = {ref_clone, (*ref_iter).node_index};
                m_cur_output_data_ref_list.push_back(connect_info);
            }
        }
    }

    return status;
}

vx_status
ExynosVisionSubgraph::kernelProcess(vx_uint32 frame_cnt)
{
    vx_status status = VX_FAILURE;

    if (frame_cnt == 0) {
        VXLOGW("frame count is zero");
    }

    List<ref_connect_info_t>::iterator ref_iter;
    for (ref_iter=m_cur_input_data_ref_list.begin(); ref_iter!=m_cur_input_data_ref_list.end(); ref_iter++) {
        m_params[(*ref_iter).node_index] = (*ref_iter).ref;
    }

    for (ref_iter=m_cur_output_data_ref_list.begin(); ref_iter!=m_cur_output_data_ref_list.end(); ref_iter++) {
        m_params[(*ref_iter).node_index] = (*ref_iter).ref;
    }

    const ExynosVisionKernel *kernel = m_represent_node->getKernelHandle();
    status = kernel->kernelFunction(m_represent_node, (const ExynosVisionDataReference **)m_params, m_param_num);

EXIT:

    return status;
}

vx_status
ExynosVisionSubgraph::putSrcRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode)
{
    vx_status status = VX_SUCCESS;

    /* prevent to put shared reference several times */
    map<ExynosVisionDataReference*, vx_bool> ref_put_map;

    List<ref_connect_info_t>::iterator ref_iter;
    List<ref_connect_info_t>::iterator ref_clone_iter;

    for (ref_iter=m_cur_input_data_ref_list.begin(); ref_iter!=m_cur_input_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_clone = (*ref_iter).ref;

        if (ref_clone != NULL)
            ref_clone ->decreaseKernelCount();
    }

    for (ref_iter=m_input_data_ref_list.begin(); ref_iter!=m_input_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_represent = (*ref_iter).ref;

        /* null parameter, it could be optional parameter */
        if (ref_represent == NULL)
            continue;

        if ((exec_mode == GRAPH_EXEC_NORMAL) || (ref_represent->getDirectInputNodeNum(m_graph) == 0)) {
            if (ref_represent->putInputExclusiveRef(frame_cnt) != VX_SUCCESS) {
                VXLOGE("put exclusive ref fails");
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                VXLOGTD("put exclusive to %s, frame_%d", ref_represent->getName(), frame_cnt);
            }
        } else {
            if (ref_put_map[ref_represent] != vx_true_e) {
                if (ref_represent->putInputShareRef(frame_cnt) != VX_SUCCESS) {
                    VXLOGE("put share ref fails");
                    status = VX_ERROR_INVALID_REFERENCE;
                } else {
                    VXLOGTD("put share to %s, frame_%d", ref_represent->getName(), frame_cnt);
                }
                ref_put_map[ref_represent] = vx_true_e;
            }
        }
    }

    m_cur_input_data_ref_list.clear();

    return status;
}

vx_status
ExynosVisionSubgraph::putDstRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode, vx_bool data_valid)
{
    vx_status status = VX_SUCCESS;

    /* prevent to put shared reference several times */
    map<ExynosVisionDataReference*, vx_bool> ref_put_map;

    List<ref_connect_info_t>::iterator ref_iter;

    for (ref_iter=m_cur_output_data_ref_list.begin(); ref_iter!=m_cur_output_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_clone = (*ref_iter).ref;

        if (ref_clone != NULL)
            ref_clone ->decreaseKernelCount();
    }

    for (ref_iter=m_output_data_ref_list.begin(); ref_iter!=m_output_data_ref_list.end(); ref_iter++) {
        ExynosVisionDataReference* ref_represent = (*ref_iter).ref;

        /* null parameter, it could be optional parameter */
        if (ref_represent == NULL)
            continue;

        if ((exec_mode == GRAPH_EXEC_NORMAL) || (ref_represent->getDirectOutputNodeNum(m_graph) == 0)) {
            if (ref_represent->putOutputExclusiveRef(frame_cnt, data_valid) != VX_SUCCESS) {
                VXLOGE("put exclusive ref fails");
                status = VX_ERROR_INVALID_REFERENCE;
            } else {
                VXLOGTD("put exclusive to %s, frame_%d", ref_represent->getName(), frame_cnt);
            }
        } else {
            if (ref_put_map[ref_represent] != vx_true_e) {
                if (ref_represent->putOutputShareRef(frame_cnt, ref_represent->getDirectOutputNodeNum(m_graph), data_valid) != VX_SUCCESS) {
                    VXLOGE("put share ref fails");
                    status = VX_ERROR_INVALID_REFERENCE;
                } else {
                    VXLOGTD("put share to %s, frame_%d", ref_represent->getName(), frame_cnt);
                }
                ref_put_map[ref_represent] = vx_true_e;
            }
        }
    }

    m_cur_output_data_ref_list.clear();

    return status;
}

vx_status
ExynosVisionSubgraph::sendDoneToPost(vx_uint32 frame_cnt)
{
    VXLOGD2("send done, frame_%d", frame_cnt);

    if (isFooter()) {
        m_complete_event->setEvent();
    } else {
        List<ref_connect_info_t>::iterator ref_iter;
        for (ref_iter=m_output_data_ref_list.begin(); ref_iter!=m_output_data_ref_list.end(); ref_iter++) {
            ExynosVisionDataReference *ref = (*ref_iter).ref;
            ref->triggerDoneEventIndirect(m_graph, frame_cnt);
        }
    }

    return VX_SUCCESS;
}

vx_status
ExynosVisionSubgraph::flushWaitEvent()
{
    m_complete_event->wakeupPendingThread();

    return VX_SUCCESS;
}

vx_status
ExynosVisionSubgraph::exitThread()
{
    status_t ret;
    vx_status status = VX_SUCCESS;

    m_main_thread->requestExit();
    m_message_queue->wakeupPendingThreadAndQDisable();
    ret = m_main_thread->requestExitAndWait();
    if (ret != NO_ERROR) {
        VXLOGE("thread of subgraph cann't exit, ret:%d", ret);
        status = VX_FAILURE;
    }
    VXLOGTD("finishing to exit thread");

    m_complete_event->wakeupPendingThread();

    return status;
}

vx_status
ExynosVisionSubgraph::clearSubgraphComplete(void)
{
    vx_status status = VX_SUCCESS;

    m_complete_event->clearEvent();

    return status;
}

vx_status
ExynosVisionSubgraph::waitSubgraphComplete(vx_uint64 wait_time)
{
    vx_status status = VX_FAILURE;
    event_exception_t exception_state;

    VXLOGD2("Start waiting %s complete", getSgName());
    exception_state = m_complete_event->waitEvent(wait_time);
    VXLOGD2("End waiting %s complete, exception:%d", getSgName(), exception_state);

    if (exception_state == EVENT_EXCEPTION_NONE)
        status = VX_SUCCESS;

    return status;
}

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
#include "ExynosVisionAutoTimer.h"
#endif

bool
ExynosVisionSubgraph::mainThreadFunc(void)
{
    queue_exception_t exception;
    vx_status status = VX_FAILURE;
    vx_uint32 frame_cnt;
    graph_exec_mode_t exec_mode = m_graph->getExecMode();

    m_thread_state.setState(THREAD_STATE_WAIT_DONE);
    VXLOGTD("waiting message");
    exception = popDoneEvent(&frame_cnt);
    VXLOGTD("receive message, exception:%d, frame_%d, exec:%d", exception, frame_cnt, exec_mode);

    if (exception == QUEUE_EXCEPTION_TIME_OUT) {
        VXLOGE("queuing fails, excepton:%d", exception);
        status = VX_FAILURE;
    } else {
        status = VX_SUCCESS;
    }

    m_exec_mutex.lock();

    if (frame_cnt && status == VX_SUCCESS) {
        VXLOGTD("%s, start frame_%d", getSgName(), frame_cnt);
        m_represent_node->informKernelStart(frame_cnt);

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
        uint64_t start_time, end_time;
        start_time = ExynosVisionDurationTimer::getTimeUs();
#endif

        /* just handover frame count and error to next subgraph if the execution of previous subgraph fails */
        m_thread_state.setState(THREAD_STATE_GET_INPUT);
        VXLOGTD("getSrcRef");
        vx_bool input_data_valid, output_data_valid;
        status = getSrcRef(frame_cnt, exec_mode, &input_data_valid);
        if (status != VX_SUCCESS) {
            VXLOGE("%s failed to getting src ref", m_sg_name);
            goto EXIT;
        }

        m_thread_state.setState(THREAD_STATE_GET_OUTPUT);
        VXLOGTD("getDstRef");
        status = getDstRef(frame_cnt, exec_mode);
        if (status != VX_SUCCESS) {
            VXLOGE("%s failed to getting dst ref", m_sg_name);
            goto EXIT;
        }

        if (input_data_valid == vx_true_e) {
            m_thread_state.setState(THREAD_STATE_EXE_KERNEL);
            VXLOGTD("kernelProcess start");
            VXLOGD2("%s process, frame_%d", m_sg_name, frame_cnt);
            status = kernelProcess(frame_cnt);
            if (status == VX_SUCCESS) {
                output_data_valid = vx_true_e;
            } else {
                output_data_valid = vx_false_e;
                VXLOGE("%s failed to process kernel function", m_sg_name);

                /* Marking only single-frame as bad instead of stopping graph in stream mode */
                if (exec_mode == GRAPH_EXEC_NORMAL) {
                    goto EXIT;
                }
            }
            VXLOGTD("kernelProcess end");

            /* node call back check */
            if (m_represent_node->m_callback) {
                vx_action action;
                action = m_represent_node->m_callback((vx_node)m_represent_node);
                if (action == VX_ACTION_ABANDON) {
                    VXLOGE("abandon graph due to callback from %s", m_represent_node->getName());
                    status = VX_ERROR_GRAPH_ABANDONED;
                    goto EXIT;
                }
            }
        } else {
            VXLOGE("receiving data is not valid");
            output_data_valid = vx_false_e;
        }

        m_thread_state.setState(THREAD_STATE_PUT_INPUT);
        VXLOGTD("putSrcRef");
        status = putSrcRef(frame_cnt, exec_mode);
        if (status != VX_SUCCESS) {
            VXLOGE("%s failed to putting src ref", m_sg_name);
            goto EXIT;
        }

        m_thread_state.setState(THREAD_STATE_PUT_OUTPUT);
        VXLOGTD("putDstRef");
        status = putDstRef(frame_cnt, exec_mode, output_data_valid);
        if (status != VX_SUCCESS) {
            VXLOGE("%s failed to putting dst ref", m_sg_name);
            goto EXIT;
        }

        m_thread_state.setState(THREAD_STATE_SEND_DONE);
        VXLOGTD("sendDoneToPost");
        status = sendDoneToPost(frame_cnt);
        if (status != VX_SUCCESS) {
            VXLOGE("%s failed to send done event", m_sg_name);
            goto EXIT;
        }

        m_thread_state.setState(THREAD_STATE_WAIT_DONE);

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
        end_time = ExynosVisionDurationTimer::getTimeUs();
        VXLOGI("[SG] %llu us", end_time - start_time);
#endif

        m_represent_node->informKernelEnd(frame_cnt, status);
    }

EXIT:
    m_exec_mutex.unlock();

    if (status == VX_SUCCESS) {
        m_last_process_frame = frame_cnt;
        if (m_main_thread->visionThreadExitPending()) {
            VXLOGTD("exit of main thread is requested");
        }
        return true;
    } else  {
        displayInfo(0, vx_true_e);
        m_graph->pushErrorEvent(this, status);
        return true;
    }
}

void
ExynosVisionSubgraph::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Subgrap] %s, state:%d, last frame:%d", MAKE_TAB(tap, tab_num), getSgName(), m_thread_state.getState(), m_last_process_frame);

    if (m_cur_input_data_ref_list.size()) {
        List<ref_connect_info_t>::iterator ref_iter;
        for (ref_iter=m_input_data_ref_list.begin(); ref_iter!=m_input_data_ref_list.end(); ref_iter++) {
            if ((*ref_iter).ref != NULL) {
                VXLOGI("%s          Holding input:%s", MAKE_TAB(tap, tab_num), (*ref_iter).ref->getName());
            }
        }
    }
    if (m_cur_output_data_ref_list.size()) {
        List<ref_connect_info_t>::iterator ref_iter;
        for (ref_iter=m_output_data_ref_list.begin(); ref_iter!=m_output_data_ref_list.end(); ref_iter++) {
            if ((*ref_iter).ref != NULL) {
                VXLOGI("%s          Holding output:%s", MAKE_TAB(tap, tab_num), (*ref_iter).ref->getName());
            }
        }
    }

    if (detail_info == vx_true_e) {
        for (List<ExynosVisionNode*>::iterator node_iter=m_node_list.begin(); node_iter!=m_node_list.end(); node_iter++)
            (*node_iter)->displayInfo(tab_num+1, vx_true_e);
    }
}

void
ExynosVisionSubgraph::displayPerf(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Subgrap][%d] represent node(%s, %s)", MAKE_TAB(tap, tab_num), detail_info,
        getSgName(), m_represent_node->getName(), m_represent_node->getKernelName());

    vx_perf_t *vx_perf = m_graph->getPerfMonitor()->getVxPerfInfo(m_represent_node);
    for (uint32_t i=0; i<NODE_TIMEPAIR_NUMBER; i++) {
        VXLOGI("%s ==Set_%d, number of exec: %llu==", MAKE_TAB(tap, tab_num+1), i, vx_perf[i].num);
        VXLOGI("%s average: %0.3lf ms", MAKE_TAB(tap, tab_num+1), (vx_float32)vx_perf[i].avg/1000.0f);
        VXLOGI("%s minimum: %0.3lf ms", MAKE_TAB(tap, tab_num+1), (vx_float32)vx_perf[i].min/1000.0f);
        VXLOGI("%s maximum: %0.3lf ms", MAKE_TAB(tap, tab_num+1), (vx_float32)vx_perf[i].max/1000.0f);
    }
}

}; /* namespace android */
