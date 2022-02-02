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

#ifndef EXYNOS_VISION_GRAPH_H
#define EXYNOS_VISION_GRAPH_H

#include <utils/threads.h>

#include "ExynosVisionQueue.h"
#include "ExynosVisionState.h"
#include "ExynosVisionEvent.h"
#include "ExynosVisionThread.h"

#include "ExynosVisionPerfMonitor.h"

#include "ExynosVisionReference.h"
#include "ExynosVisionContext.h"
#include "ExynosVisionNode.h"

namespace android {

enum graph_state {
    GRAPH_STATE_NOT_VERIFIED = 1,
    GRAPH_STATE_VERIFIED = 2,
    GRAPH_STATE_PROCESSING = 3,
    GRAPH_STATE_STREAMING = 4,
};

typedef enum _graph_exec_mode_t {
    GRAPH_EXEC_NORMAL,
    GRAPH_EXEC_STREAM
} graph_exec_mode_t;

typedef struct _graph_error_message_t {
    ExynosVisionSubgraph *subgraph;
    vx_status		error;
} graph_error_message_t;

typedef struct _graph_schedule_message_t {

} graph_schedule_message_t;

struct graph_param_info {
    ExynosVisionNode *node;
    vx_uint32 index;
};

#define COMPLETE_WAIT_TIME ((vx_uint64)5 * 1000 * 1000 * 1000)     // 5 sec

class ExynosVisionGraph : public ExynosVisionReference {

private:
    /*! \brief The array of all nodes in this graph */
    List<ExynosVisionNode*> m_node_list;
    List<ExynosVisionNode*> m_sorted_node_list;

    /*! \brief The performance logging variable. */
    vx_perf_t      perf;
    /*! \brief The disposition of the execution of this graph, it will be used at vxQueryGraph(). */
    vx_status      m_error_status;
    /*! \brief This indicates that the graph has been verified. */
    vx_bool        m_verified;

    /*! \brief The list of graph parameters. */
    Vector<graph_param_info> m_param_vector;

    /*! \brief [hidden] If non-NULL, the parent graph, for scope handling. */
    ExynosVisionNode* m_parent_node;

    ExynosVisionState m_graph_state;

    List<ExynosVisionDataReference*> m_ref_header_list;
    List<ExynosVisionDataReference*> m_ref_footer_list;

    List<ExynosVisionSubgraph*> m_sg_list;
    List<ExynosVisionSubgraph*> m_sg_header_list;
    List<ExynosVisionSubgraph*> m_sg_footer_list;

    /* graph and queue object should be able to count frame number.
        - normal mode : an ancestor graph object counts frame number.
        - stream mode : an each queue object counts frame number separately.
        */
    map<ExynosVisionReference*, vx_uint32> m_frame_cnt_map;

    ExynosVisionQueue<graph_error_message_t> *m_error_queue;
    sp<ExynosVisionThread<ExynosVisionGraph>>                  m_error_thread;

    /* thread for vxScheduleGraph() and vxWaitGraph() */
    ExynosVisionQueue<graph_schedule_message_t> *m_schedule_queue;
    sp<ExynosVisionThread<ExynosVisionGraph>>                  m_schedule_thread;
    ExynosVisionEvent *m_schedule_complete_event;

    graph_exec_mode_t m_exec_mode;

    ExynosVisionPerfMonitor<ExynosVisionNode*> *m_performance_monitor;

    /* This indicates that whether child graph is merged or not */
    vx_bool m_is_replaced_flag;

    uint64_t m_verify_time;

public:

private:
    bool errorThreadFunc(void);
    queue_exception_t popErrorEvent(graph_error_message_t *error_message);

    vx_status pushScheduleEvent(void);
    bool scheduleThreadFunc(void);
    queue_exception_t popScheduleEvent(graph_schedule_message_t *schedule_message);

    vx_status topologicalSort(void);
    vx_status parameterValidation(void);
    /* 1. check data reference if multiple writer is exist, 2. find header/footer data reference */
    vx_status checkDataReference(void);
    vx_status initializeKernel(void);
    vx_status groupingSubgraph(void);
    vx_status fixAllSubgraph(void);
    vx_status checkExecutionModePropriety(void);

    vx_status stopProcessGraph(void);

public:
    /* Constructor */
    ExynosVisionGraph(ExynosVisionContext *context);

    /* Destructor */
    virtual ~ExynosVisionGraph();

    vx_status init(void);
    virtual vx_status destroy(void);

    vx_status verifyGraph();

    vx_status processGraph(void);
    vx_status scheduleGraph(void);
    vx_status waitGraph(void);

    vx_status queryGraph(vx_enum attribute, void *ptr, vx_size size);
    vx_status setGraphAttribute(vx_enum attribute, const void *ptr, vx_size size);

    ExynosVisionNode* createGenericNode(ExynosVisionKernel *kernel);
    ExynosVisionNode* createNodeByStructure(vx_enum kernel_enum, ExynosVisionDataReference *params[], vx_uint32 num);
    vx_status removeNode(ExynosVisionNode *node);

    vx_status addParameterToGraph(ExynosVisionParameter *param);
    vx_status setGraphParameterByIndex(vx_uint32 index, ExynosVisionDataReference *ref);
    ExynosVisionParameter* getGraphParameterByIndex(vx_uint32 index);

    vx_status replaceNodeToGraph(ExynosVisionNode *node, ExynosVisionGraph *child_graph);

    vx_status setParentNode(ExynosVisionNode* node);

    vx_bool isGraphVerified(void)
    {
        return m_verified;
    }
    vx_bool isWorking()
    {
        if (m_graph_state.getState() == GRAPH_STATE_PROCESSING) {
            return vx_true_e;
        } else {
            return vx_false_e;
        }
    }
    vx_uint32 getNumParams()
    {
        return m_param_vector.size();
    }
    const struct graph_param_info* getParameterHandle(vx_uint32 index)
    {
        return &m_param_vector[index];
    }

    ExynosVisionPerfMonitor<ExynosVisionNode*>* getPerfMonitor()
    {
        return m_performance_monitor;
    }

    vx_status stopGraph();
    vx_status pushErrorEvent(ExynosVisionSubgraph *subgraph, vx_status error);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
    virtual void displayPerf(vx_uint32 tab_num, vx_bool detail_info);

    vx_status setSerialize(void)
    {
        VXLOGE("graph serialization is not supported");

        return VX_ERROR_NOT_SUPPORTED;
    }
    void setExecMode(graph_exec_mode_t exec_mode)
    {
        m_exec_mode = exec_mode;
    }
    graph_exec_mode_t getExecMode(void)
    {
        return m_exec_mode;
    }

    vx_uint32 requestNewFrameCnt(ExynosVisionReference *ref);

    void invalidateGraph(void)
    {
        m_verified = vx_false_e;
    }
};

}; // namespace android
#endif
