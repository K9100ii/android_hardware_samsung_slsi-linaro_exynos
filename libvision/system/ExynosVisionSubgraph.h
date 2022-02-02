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

#ifndef EXYNOS_VISION_SUBGRAPH_H
#define EXYNOS_VISION_SUBGRAPH_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>

#include <VX/vx.h>

#include "ExynosVisionQueue.h"
#include "ExynosVisionThread.h"
#include "ExynosVisionEvent.h"
#include "ExynosVisionState.h"

#include "ExynosVisionGraph.h"
#include "ExynosVisionNode.h"

namespace android {

enum thread_state {
    THREAD_STATE_NOT_START = 0,

    THREAD_STATE_WAIT_DONE = 1,
    THREAD_STATE_GET_INPUT = 2,
    THREAD_STATE_GET_OUTPUT = 3,
    THREAD_STATE_EXE_KERNEL = 4,
    THREAD_STATE_PUT_INPUT = 2,
    THREAD_STATE_PUT_OUTPUT = 3,
    THREAD_STATE_SEND_DONE = 6
};

enum sg_message_type {
    /* trigger is a signal that indicating subgraph to start process,
     subgraph that receiving trigger should not have an input shared reference */
    SG_MESSAGE_TRIGGER = 1,
    /* done events are came from input shared references */
    SG_MESSAGE_DONE_EVENT = 2
};

typedef struct _subgraph_message_t {
    enum sg_message_type type;
    vx_int32		frame_cnt;

    ExynosVisionDataReference   *done_reference;
    vx_uint32 node_index;
} subgraph_message_t;

typedef ExynosVisionQueue<subgraph_message_t> sg_msg_queue_t;

class ExynosVisionSubgraph {

typedef struct _ref_connect_info_t {
    ExynosVisionDataReference *ref;
    vx_uint32 node_index;
} ref_connect_info_t;

private:
    vx_uint32 m_id;
    vx_char m_sg_name[VX_MAX_SUBGRAPH_NAME];

    ExynosVisionGraph *m_graph;

    /* represented node, it will be generated from dynamic kernel */
    ExynosVisionNode* m_represent_node;
    /* list of all nodes including subgraph */
    List<ExynosVisionNode*> m_node_list;

    sg_msg_queue_t *m_message_queue;
    Mutex m_exec_mutex;

    sp<ExynosVisionThread<ExynosVisionSubgraph>>                  m_main_thread;
    ExynosVisionState   m_thread_state;

    map<vx_uint32, vx_uint32> m_ready_bitmask_map;

    vx_uint32 m_target_done_bitmask;

    List<ref_connect_info_t> m_input_data_ref_list;
    List<ref_connect_info_t> m_output_data_ref_list;

    /* instance data reference list for single frame */
    List<ref_connect_info_t> m_cur_input_data_ref_list;
    List<ref_connect_info_t> m_cur_output_data_ref_list;

    ExynosVisionDataReference **m_params;
    vx_uint32 m_param_num;

    ExynosVisionEvent *m_complete_event;

    vx_uint32 m_last_process_frame;

public:

private:
    bool mainThreadFunc(void);
    queue_exception_t popDoneEvent(vx_uint32 *ret_frame_cnt);

    vx_status getSrcRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode, vx_bool *ret_data_valid);
    vx_status getDstRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode);
    vx_status kernelProcess(vx_uint32 frame_cnt);
    vx_status putSrcRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode);
    vx_status putDstRef(vx_uint32 frame_cnt, graph_exec_mode_t exec_mode, vx_bool data_valid);
    vx_status sendDoneToPost(vx_uint32 frame_cnt);

    vx_status makeInputOutputPort(void);
    vx_status allocateDataRefMemory(void);

public:

    /* Constructor */
    ExynosVisionSubgraph(ExynosVisionGraph *graph);

    /* Destructor */
    virtual ~ExynosVisionSubgraph();

    vx_status init(ExynosVisionNode *node);
    vx_status destroy(void);

    vx_status fixSubgraph(void);

    vx_bool verifyPopedEvent(ExynosVisionDataReference *data_ref, vx_uint32 node_index);

    vx_bool isHeader(void)
    {
        return m_represent_node->isHeader();
    }
    vx_bool isFooter(void)
    {
        return m_represent_node->isFooter();
    }
    vx_uint32 getId()
    {
        return m_id;
    }
    vx_char* getSgName()
    {
        return m_sg_name;
    }

    /* push start signal to subgraph, all input data reference should be exclusive */
    vx_status pushTrigger(vx_uint32 frame_cnt);
    /* push doen event to subgraph, each input data reference send done event individually */
    vx_status pushDoneEvent(vx_uint32 frame_cnt, ExynosVisionDataReference *ref, vx_uint32 node_index);

    vx_status clearSubgraphComplete(void);
    vx_status waitSubgraphComplete(vx_uint64 wait_time);

    vx_status flushWaitEvent();
    vx_status exitThread();

    vx_status replaceDataRef(ExynosVisionDataReference *old_ref, ExynosVisionDataReference *new_ref, vx_uint32 node_index, enum vx_direction_e dir);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
    virtual void displayPerf(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
