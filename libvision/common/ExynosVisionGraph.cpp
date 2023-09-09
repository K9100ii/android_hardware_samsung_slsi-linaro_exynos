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

#define LOG_TAG "ExynosVisionGraph"
#include <cutils/log.h>

#include "ExynosVisionAutoTimer.h"

#include "ExynosVisionGraph.h"
#include "ExynosVisionSubgraph.h"

#define GRAPHDBG    /* VXLOGD */

namespace android {

ExynosVisionGraph::ExynosVisionGraph(ExynosVisionContext *context)
                                                                    : ExynosVisionReference(context, VX_TYPE_GRAPH, (ExynosVisionReference*)context, vx_true_e)
{
    memset(&perf, 0x0, sizeof(perf));
    m_error_status = VX_SUCCESS;
    m_verified = vx_false_e;

    m_parent_node = NULL;
    m_graph_state.setState(GRAPH_STATE_NOT_VERIFIED);

    m_schedule_complete_event = NULL;

    m_exec_mode = GRAPH_EXEC_NORMAL;

    m_performance_monitor = NULL;
    m_is_replaced_flag = vx_false_e;

    m_error_queue = NULL;
    m_schedule_queue = NULL;

    m_verify_time = 0;
}

ExynosVisionGraph::~ExynosVisionGraph()
{

}

vx_status
ExynosVisionGraph::init(void)
{
    vx_status status = VX_SUCCESS;

    m_performance_monitor = new ExynosVisionPerfMonitor<ExynosVisionNode*>;
    if (m_performance_monitor == NULL)
        VXLOGE("performance monitor can't create");

    m_error_queue = new ExynosVisionQueue<graph_error_message_t>(0);
    m_error_thread = new ExynosVisionThread<ExynosVisionGraph>(this, &ExynosVisionGraph::errorThreadFunc, "graph_err", PRIORITY_DEFAULT);
    if (m_error_thread.get() != NULL) {
        m_error_thread->run();
    } else  {
        status = VX_FAILURE;
    }

    m_schedule_queue = new ExynosVisionQueue<graph_schedule_message_t>(0);
    m_schedule_thread = new ExynosVisionThread<ExynosVisionGraph>(this, &ExynosVisionGraph::scheduleThreadFunc, "graph_shd", PRIORITY_DEFAULT);
    if (m_schedule_thread.get() != NULL) {
        m_schedule_thread->run();
    } else  {
        status = VX_FAILURE;
    }

    if (getContext()->getPerfMonitor() != NULL) {
        getContext()->getPerfMonitor()->registerObjectForTrace(this, GRAPH_TIMEPAIR_NUMBER);
    } else {
        VXLOGE("performance monitor is not assigned");
    }

    m_schedule_complete_event = new ExynosVisionEvent(0);

    return status;
}

vx_status
ExynosVisionGraph::destroy(void)
{
    vx_status status = VX_SUCCESS;

    VXLOGD3("%s destroy", getName());

    if (m_parent_node)
        m_parent_node = NULL;

    List<ExynosVisionSubgraph*>::iterator sg_iter;
    for (sg_iter=m_sg_list.begin(); sg_iter!=m_sg_list.end(); sg_iter++) {
        status = (*sg_iter)->destroy();
        if (status != VX_SUCCESS)
            VXLOGE("destorying sg(%d) fails, err:%d", (*sg_iter)->getId(), status);
        delete *sg_iter;
    }
    m_sg_list.clear();

    m_param_vector.clear();

    List<ExynosVisionNode*>::iterator node_iter;
    for (node_iter=m_node_list.begin(); node_iter!=m_node_list.end(); node_iter++) {
         while((*node_iter)->getExternalCnt()) {
            status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&(*node_iter), VX_REF_EXTERNAL);
            if (status != VX_SUCCESS) {
                VXLOGE("release external reference of %s fails, err:%d", (*node_iter)->getName(), status);
            }
        }

        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&(*node_iter), VX_REF_INTERNAL, this);
        if (*node_iter != NULL) {
            VXLOGE("%s was not destoyed", (*node_iter)->getName());
            (*node_iter)->displayInfo(0, vx_true_e);
        }
    }
    m_node_list.clear();

    status_t ret;

    if (m_error_queue) {
        m_error_queue->wakeupPendingThreadAndQDisable();
        ret = m_error_thread->requestExitAndWait();
        if (ret != NO_ERROR) {
            VXLOGE("thread of subgraph cann't exit, ret:%d", ret);
            status = VX_FAILURE;
        }
        VXLOGD3("[TD]err-thread, finishing to exit thread");

        m_error_queue->release();

        delete m_error_queue;
        m_error_queue = NULL;
    }

    if (m_schedule_queue) {
        m_schedule_queue->wakeupPendingThreadAndQDisable();
        ret = m_schedule_thread->requestExitAndWait();
        if (ret != NO_ERROR) {
            VXLOGE("thread of subgraph cann't exit, ret:%d", ret);
            status = VX_FAILURE;
        }
        VXLOGD3("[TD]shd-thread, finishing to exit thread");

        m_schedule_queue->release();

        delete m_schedule_queue;
        m_schedule_queue = NULL;
    }

    if (m_performance_monitor) {
        delete m_performance_monitor;
        m_performance_monitor = NULL;
    }

    if (m_schedule_complete_event) {
        delete m_schedule_complete_event;
        m_schedule_complete_event = NULL;
    }

    if (getContext()->getPerfMonitor() != NULL) {
        getContext()->getPerfMonitor()->releaseObjectForTrace(this);
    } else {
        VXLOGE("performance monitor is not assigned");
    }

    return status;
}

ExynosVisionNode*
ExynosVisionGraph::createGenericNode(ExynosVisionKernel *kernel)
{
    EXYNOS_VISION_SYSTEM_IN();

    /* VX_REF_INTERNAL for m_node_list of graph */
    ExynosVisionNode *node = new ExynosVisionNode(getContext(), this);
    if (node->getCreationStatus() != VX_SUCCESS) {
        VXLOGE("node object creation fail(%d)", node->getCreationStatus());
        delete node;
        return (ExynosVisionNode*)getContext()->getErrorObject(node->getCreationStatus());
    }

    node->init(this, kernel);

    m_internal_lock.lock();

    m_node_list.push_back(node);
    node->incrementReference(VX_REF_INTERNAL, this);

    /* force a re-verify */
    m_verified = vx_false_e;
    m_internal_lock.unlock();

    VXLOGD2("Created %s from %s", node->getName(), node->getKernelName());

    EXYNOS_VISION_SYSTEM_OUT();

    return node;
}

ExynosVisionNode*
ExynosVisionGraph::createNodeByStructure(vx_enum kernel_enum, ExynosVisionDataReference *params[], vx_uint32 num)
{
    vx_status status = VX_SUCCESS;
    ExynosVisionNode *node = NULL;
    ExynosVisionContext *context = getContext();
    ExynosVisionKernel *kernel = context->getKernelByEnum(kernel_enum);

    if (kernel) {
        VXLOGD3("kernel name:%s", kernel->getKernelName());
        node = createGenericNode(kernel);
        if (node->getStatus() == VX_SUCCESS) {
            vx_uint32 p = 0;
            for (p = 0; p < num; p++) {
                if (params[p] == NULL)
                    continue;

                status = node->setParameterByIndex(p, params[p]);
                if (status != VX_SUCCESS) {
                    VXLOGE("Kernel %d Parameter %u is invalid.\n", kernel_enum, p);
                    context->addLogEntry(this, status, "Kernel %d Parameter %u is invalid.\n", kernel_enum, p);

                    return (ExynosVisionNode*)context->getErrorObject(status);
                }
            }
        }
        else {
            VXLOGE("Failed to create node with kernel enum %d", kernel_enum);
            context->addLogEntry(this, VX_ERROR_INVALID_PARAMETERS, "Failed to create node with kernel enum %d\n", kernel_enum);
        }
    } else {
        VXLOGE("failed to retrieve kernel enum %d", kernel_enum);
        context->addLogEntry(this, VX_ERROR_NOT_SUPPORTED, "failed to retrieve kernel enum %d\n", kernel_enum);
        status = VX_ERROR_NOT_SUPPORTED;

        return (ExynosVisionNode*)context->getErrorObject(status);
    }

    return node;
}

vx_status
ExynosVisionGraph::removeNode(ExynosVisionNode *node)
{
    if (m_graph_state.getState() == GRAPH_STATE_PROCESSING) {
        VXLOGE("%s, can't remove node during processing", getName());
        return VX_FAILURE;
    }

    vx_status status = VX_SUCCESS;

    m_verified = vx_false_e;
    m_graph_state.setState(GRAPH_STATE_NOT_VERIFIED);

    if (node->getSubgraph()) {
        List<ExynosVisionSubgraph*>::iterator sg_iter;
        for (sg_iter=m_sg_list.begin(); sg_iter!=m_sg_list.end(); sg_iter++) {
            if (*sg_iter == node->getSubgraph())
                break;
        }

        if (sg_iter != m_sg_list.end()) {
            status = (*sg_iter)->destroy();
            if (status != VX_SUCCESS)
                VXLOGE("destorying %s fails, err:%d", (*sg_iter)->getSgName(), status);
            delete *sg_iter;
            m_sg_list.erase(sg_iter);
        } else {
            VXLOGE("%s doesn't have %s", this->getName(), (*sg_iter)->getSgName());
        }
    }

    List<ExynosVisionNode*>::iterator node_iter;
    for (node_iter=m_node_list.begin(); node_iter!=m_node_list.end(); node_iter++) {
        if (*node_iter == node) {
            break;
        }
    }

    if (node_iter == m_node_list.end()) {
        VXLOGE("%s doesn't have %s", getName(), node->getName());
        status = VX_ERROR_INVALID_PARAMETERS;
    } else {
        status = (*node_iter)->disconnectParentGraph();

        m_node_list.erase(node_iter);
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&*node_iter, VX_REF_INTERNAL, this);
        if (status != VX_SUCCESS)
            VXLOGE("releasoing node fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVisionGraph::addParameterToGraph(ExynosVisionParameter *param)
{
    VXLOGD3("add param to graph:node:%s(%s), index:%d", param->getNodeHandle()->getName(), param->getKernelHandle()->getKernelName(), param->getIndex());

    struct graph_param_info graph_param;
    graph_param.node = param->getNode();
    graph_param.index = param->getIndex();

    m_param_vector.push_back(graph_param);

    return VX_SUCCESS;
}

vx_status
ExynosVisionGraph::setGraphParameterByIndex(vx_uint32 index, ExynosVisionDataReference *ref)
{
    if (m_param_vector.size() <= index) {
        VXLOGE("%s, out of bound, graph's param:%d, index:%d", getName(), m_param_vector.size(), index);
        return VX_FAILURE;
    }

    vx_status status;

    ExynosVisionNode *node = m_param_vector[index].node;
    vx_uint32 node_index = m_param_vector[index].index;
    status = node->setParameterByIndex(node_index, ref);
    if (status != VX_SUCCESS) {
        VXLOGE("setting node param fails, err:%d", status);
    }

    return status;
}

ExynosVisionParameter*
ExynosVisionGraph::getGraphParameterByIndex(vx_uint32 index)
{
    if (m_param_vector.size() <= index) {
        VXLOGE("%s, out of bound, graph's param:%d, index:%d", getName(), m_param_vector.size(), index);
        return NULL;
    }

    ExynosVisionNode *node = m_param_vector[index].node;
    vx_uint32 node_index = m_param_vector[index].index;
    ExynosVisionParameter *parameter;

    parameter = node->getParameterByIndex(node_index);
    if (parameter == NULL) {
        VXLOGE("getting node param fails, err:%d");
    }

    return parameter;
}

vx_status
ExynosVisionGraph::replaceNodeToGraph(ExynosVisionNode *node, ExynosVisionGraph *child_graph)
{
    vx_status status = VX_SUCCESS;

    ExynosVisionGraph *parent_graph = this;
    vx_uint32 param_num;
    List<ExynosVisionNode*>::iterator node_iter;
    List<ExynosVisionNode*> clone_node_list;
    map<ExynosVisionNode*, ExynosVisionNode*> replaced_node_map;

    vx_enum kernel_enum;
    ExynosVisionDataReference *params[VX_INT_MAX_PARAMS];

    vx_bool match = vx_true_e;
    param_num = node->getKernelHandle()->getNumParams();
    /* check to make sure the signature of the node matches the signature of the graph. */
    if (child_graph->getNumParams() > 0) {
        for (vx_uint32 p = 0; p < param_num; p++) {
            const struct graph_param_info *graph_param = child_graph->getParameterHandle(p);
            if ((graph_param->node) && (graph_param->node != node)) {
                ExynosVisionNode *graph_param_node = graph_param->node;
                vx_uint32 graph_param_index = graph_param->index;

                VXLOGD2("node:%s, p:%d  /  graph_param_node:%s, graph_param_index:%d", node->getName(), p, graph_param_node->getName(), graph_param_index);
                if (node->getDataRefByIndex(p) != graph_param_node->getDataRefByIndex(graph_param_index)) {
                    VXLOGE("graph parameter's node has invalid data object");
                    match = vx_false_e;
                    break;
                }
            } else  {
                VXLOGE("graph parameter's node is invalid object");
                match = vx_false_e;
                break;
            }
        }
    } else {
        VXLOGE("to replace node, the graph must have some parameters with replacing node");
    }
    if (match == vx_false_e) {
        goto EXIT;
    }

    for (node_iter=child_graph->m_node_list.begin(); node_iter!=child_graph->m_node_list.end(); node_iter++) {
        ExynosVisionNode *emigrant_node = *node_iter;
        kernel_enum = emigrant_node->getKernelHandle()->getEnumeration();
        param_num = emigrant_node->getKernelHandle()->getNumParams();
        for (vx_uint32 p=0; p<param_num; p++) {
            params[p] = emigrant_node->getDataRefByIndex(p);
        }

        ExynosVisionNode *immigrant_node = parent_graph->createNodeByStructure(kernel_enum, params, param_num);
        if (immigrant_node == NULL) {
            VXLOGE("creating node fails");
            status = VX_ERROR_INVALID_GRAPH;
            break;
        }

        /* emigrant_node will be removed later */
        replaced_node_map[emigrant_node] = immigrant_node;
    }

    if (status != VX_SUCCESS) {
        VXLOGE("Emigration of nodes fails, err:%d", status);
        goto EXIT;
    }

    /* update graph parameter if exists. (it means that the current graph is also the child graph.)
      if the parameters of replaced node is the one of graph parameter, it should be replaced to the parameter of replacing node in child-graph. */
    Vector<graph_param_info>::iterator graph_param_iter;
    for (graph_param_iter=m_param_vector.begin(); graph_param_iter!=m_param_vector.end(); graph_param_iter++) {
        VXLOGD3("%s graph param, node:%s, param:%d", getName(), graph_param_iter->node->getName(), graph_param_iter->index);
        if (graph_param_iter->node == node) {
            const struct graph_param_info *child_graph_param = child_graph->getParameterHandle(graph_param_iter->index);
            graph_param_iter->node = replaced_node_map[child_graph_param->node];
            graph_param_iter->index = child_graph_param->index;
            VXLOGD3("replace graph param: %s -> %s", graph_param_iter->node->getName(), child_graph_param->node->getName());
        }
    }

    status = parent_graph->removeNode(node);
    if  (status != VX_SUCCESS) {
        VXLOGE("removing %s from %s fails, err:%d", node->getName(), parent_graph->getName(), status);
        goto EXIT;
    }

    clone_node_list = child_graph->m_node_list;
    for (node_iter=clone_node_list.begin(); node_iter!=clone_node_list.end(); node_iter++) {
        ExynosVisionNode *emigrant_node = *node_iter;
        status = child_graph->removeNode(emigrant_node);
        if  (status != VX_SUCCESS) {
            VXLOGE("removing %s from %s fails, err:%d", emigrant_node->getName(), child_graph->getName(), status);
        }
    }

EXIT:
    if (status == VX_SUCCESS) {
        m_is_replaced_flag = vx_true_e;
    }

    return status;
}

vx_status
ExynosVisionGraph::verifyGraph(void)
{
    vx_status status = VX_FAILURE;
    uint64_t verify_start, verify_end;

    Mutex::Autolock lock(m_external_lock);

    if (m_verified == vx_true_e) {
        VXLOGD("already verified %s", getName());
        return VX_SUCCESS;
    }

    verify_start = ExynosVisionDurationTimer::getTimeUs();

    /* reset the frame count */
    m_frame_cnt_map.clear();

    VXLOGD3("Topological Sort phase - first");
    status = topologicalSort();
    if (status != VX_SUCCESS) {
        VXLOGE("topological sort fail in verify(%d)", status);
        goto Exit;
    }

    VXLOGD3("Parameter Validation phase");
    status = parameterValidation();
    if (status != VX_SUCCESS) {
        VXLOGE("parameter validation fail in verify(%d)", status);
        goto Exit;
    }

    VXLOGD3("Check data reference phase");
    status = checkDataReference();
    if (status != VX_SUCCESS) {
        VXLOGE("single writer check fail in verify(%d)", status);
        goto Exit;
    }

    VXLOGD3("Kernel Initialize phase");
    status = initializeKernel();
    if (status != VX_SUCCESS) {
        VXLOGE("kernel initialization fails in verify(%d)", status);
        goto Exit;
    }

    if (m_is_replaced_flag) {
        VXLOGD3("Topological Sort phase - second");
        status = topologicalSort();
        if (status != VX_SUCCESS) {
            VXLOGE("topological sort fail in verify(%d)", status);
            goto Exit;
        }
    }

    VXLOGD3("grouping subgraph phase\n");
    status = groupingSubgraph();
    if (status != VX_SUCCESS) {
        VXLOGE("grouping subgraph fails in verify(%d)", status);
        goto Exit;
    }

    VXLOGD3("fixing subgraph phase\n");
    status = fixAllSubgraph();
    if (status != VX_SUCCESS) {
        VXLOGE("fixing subgraph fails in verify(%d)", status);
        goto Exit;
    }

    VXLOGD3("checking graph execution mode phase\n");
    status = checkExecutionModePropriety();
    if (status != VX_SUCCESS) {
        VXLOGE("checking execution mode fails in verify(%d)", status);
        goto Exit;
    }

Exit:
    verify_end = ExynosVisionDurationTimer::getTimeUs();
    m_verify_time = verify_end - verify_start;

    if (status == VX_SUCCESS) {
        m_verified = vx_true_e;
        m_graph_state.setState(GRAPH_STATE_VERIFIED);
    } else {
        m_verified = vx_false_e;
    }

    return status;
}

vx_status
ExynosVisionGraph::topologicalSort(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    List<ExynosVisionNode*> prev_node_list;
    List<ExynosVisionNode*> cur_node_list;
    map<vx_uint32, vx_uint32> sorted_parent_node_num_map;
    const List<ExynosVisionNode*> *const_node_list;

    m_sorted_node_list.clear();

    /* Find all head nodes */
    for (List<ExynosVisionNode*>::iterator iter_node = m_node_list.begin(); iter_node != m_node_list.end(); iter_node++ ) {
        (*iter_node)->calculateConnectedNode();
        if ((*iter_node)->isHeader()) {
            GRAPHDBG("head node:(%s/%s)", (*iter_node)->getName(), (*iter_node)->getKernelName());
            prev_node_list.push_back(*iter_node);
        }
    }

    while(1) {
        /* Insert finding nodes to sorting queue */
        const_node_list = &prev_node_list;
        m_sorted_node_list.insert(m_sorted_node_list.end(), const_node_list->begin(), const_node_list->end());
        cur_node_list.clear();

        /* find 1st level children of previous nodes and check that all of its parents are inserted sorting queue already. */
        for (List<ExynosVisionNode*>::iterator parent_node = prev_node_list.begin(); parent_node != prev_node_list.end(); parent_node++) {
            for (vx_uint32 op = 0; op < (*parent_node)->getOutputDataRefNum(); op++) {
                ExynosVisionDataReference *data_ref = (*parent_node)->getOutputDataRefByIndex(op);
                if (data_ref == NULL) {
                    if ((*parent_node)->getParamState(VX_OUTPUT, op) == VX_PARAMETER_STATE_OPTIONAL) {
                        continue;
                    } else {
                        VXLOGE("some parameter is not assigned at %s", (*parent_node)->getName());
                        status = VX_ERROR_INVALID_REFERENCE;
                        m_sorted_node_list.clear();
                        goto EXIT;
                    }
                }

                for (vx_uint32 child_node_idx = 0; child_node_idx < data_ref->getIndirectOutputNodeNum(this); child_node_idx++) {
                    ExynosVisionNode *child_node = data_ref->getIndirectOutputNode(this, child_node_idx);
                    sorted_parent_node_num_map[child_node->getId()]++;
                    if (child_node->getPreNodeNum() == sorted_parent_node_num_map[child_node->getId()]) {
                        GRAPHDBG("cur node:(%s/%s)", child_node->getName(), child_node->getKernelName());
                        cur_node_list.push_back(child_node);
                    }
                }
            }
        }

        /* Set last finding node to previous group */
        prev_node_list.clear();
        const_node_list = &cur_node_list;
        prev_node_list.insert(prev_node_list.end(), const_node_list->begin(), const_node_list->end());

        for (List<ExynosVisionNode*>::iterator iter_pos=m_sorted_node_list.begin(); iter_pos!=m_sorted_node_list.end(); iter_pos++) {
            GRAPHDBG("sorted node:(%s/%s)", (*iter_pos)->getName(), (*iter_pos)->getKernelName());
        }

        if (!cur_node_list.size())
            break;
        if (m_sorted_node_list.size() > m_node_list.size())
            break;
    }

    GRAPHDBG("sorted node num:%d", m_sorted_node_list.size());

    if (m_node_list.size() == m_sorted_node_list.size()) {
        status = VX_SUCCESS;
    } else {
        VXLOGE("one node is not connected in graph");
        status = VX_ERROR_INVALID_GRAPH;
    }

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::parameterValidation(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;

    GRAPHDBG("sorted node num:%d", m_sorted_node_list.size());
    for (List<ExynosVisionNode*>::iterator iter_node = m_sorted_node_list.begin(); iter_node != m_sorted_node_list.end(); iter_node++ ) {
        status = (*iter_node)->verifyNode();
        if (status != VX_SUCCESS) {
            VXLOGE("node verifying fail at node(%d), %d", (*iter_node)->getId(), status);
            break;
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::checkDataReference(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;

    m_ref_header_list.clear();
    m_ref_footer_list.clear();

    GRAPHDBG("sorted node num:%d", m_sorted_node_list.size());
    for (List<ExynosVisionNode*>::iterator node_iter1 = m_sorted_node_list.begin(); node_iter1!=m_sorted_node_list.end(); node_iter1++) {
        /* find header reference */
        for (vx_uint32 input_ref_idx=0; input_ref_idx < (*node_iter1)->getInputDataRefNum(); input_ref_idx++) {
            ExynosVisionDataReference *in_ref = (*node_iter1)->getInputDataRefByIndex(input_ref_idx);

            if (in_ref == NULL)
                continue;

            GRAPHDBG("%s, input node num:%d, output node num:%d", in_ref->getName(),
                                                                in_ref->getDirectInputNodeNum(this), in_ref->getIndirectInputNodeNum(this),
                                                                in_ref->getDirectOutputNodeNum(this), in_ref->getIndirectOutputNodeNum(this));
            if (in_ref->getIndirectInputNodeNum(this) == 0)
                m_ref_header_list.push_back(in_ref);
        }

        for (vx_uint32 output_ref_idx1 = 0; output_ref_idx1 < (*node_iter1)->getOutputDataRefNum(); output_ref_idx1++) {
            ExynosVisionDataReference *out_ref = (*node_iter1)->getOutputDataRefByIndex(output_ref_idx1);

            /* find footer reference */
            if (out_ref == NULL)
                continue;

            GRAPHDBG("%s, input node num:%d/%d, output node num:%d/%d", out_ref->getName(),
                                                            out_ref->getDirectInputNodeNum(this), out_ref->getIndirectInputNodeNum(this),
                                                            out_ref->getDirectOutputNodeNum(this), out_ref->getIndirectOutputNodeNum(this));
            if (out_ref->getIndirectOutputNodeNum(this) == 0)
                m_ref_footer_list.push_back(out_ref);

            /* check multiple writer */
            if (out_ref->getIndirectInputNodeNum(this) >= 2) {
                VXLOGE("%s has multiple node writer", out_ref->getName(), out_ref->getType());
                status = VX_ERROR_MULTIPLE_WRITERS;
                continue;
            }
#if 0
            /* the checking of the in-direct input node number could cover below. */
            for (List<ExynosVisionNode*>::iterator node_iter2 = m_sorted_node_list.begin(); node_iter2!=m_sorted_node_list.end(); node_iter2++) {
                if ((*node_iter1) == (*node_iter2))
                    continue;
                for (vx_uint32 output_ref_idx2 = 0; output_ref_idx2 < (*node_iter2)->getOutputDataRefNum(); output_ref_idx2++) {
                    ExynosVisionDataReference *op2 = (*node_iter2)->getOutputDataRefByIndex(output_ref_idx2);
                    if ((out_ref != NULL) &&
                        (op2 != NULL) &&
                        ExynosVisionDataReference::checkWriteDependency(out_ref, op2) == vx_true_e) {
                        status = VX_ERROR_MULTIPLE_WRITERS;
                        VXLOGE("node(%d) and node(%d0) are trying to output to the same ref", (*node_iter1)->getId(), (*node_iter2)->getId());
                        getContext()->addLogEntry(this, status, "node(%d) and node(%d0) are trying to output to the same ref", (*node_iter1)->getId(), (*node_iter2)->getId());
                        break;
                    }
                }
            }
#endif
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::initializeKernel(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;

    vx_status kernel_init_status;
    for (List<ExynosVisionNode*>::iterator node_iter=m_sorted_node_list.begin(); node_iter!=m_sorted_node_list.end(); node_iter++) {
        ExynosVisionNode *node = (*node_iter);
        status = node->initilalizeKernel();
        if (status != VX_SUCCESS) {
            VXLOGE("node(%d,%s) is not initialized", node->getId(), node->getKernelName());
            break;
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::groupingSubgraph(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_FAILURE;

    for (List<ExynosVisionNode*>::iterator node_iter = m_sorted_node_list.begin(); node_iter != m_sorted_node_list.end(); node_iter++ ) {
        ExynosVisionNode *cur_node = *node_iter;
        ExynosVisionSubgraph *cur_subgraph = new ExynosVisionSubgraph(this);

        status = cur_subgraph->init(cur_node);
        cur_node->setSubgraph(cur_subgraph);

        if (status != VX_SUCCESS) {
            VXLOGE("subgraph can't init, err:%d", status);
            break;
        }

        m_sg_list.push_back(cur_subgraph);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::fixAllSubgraph(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status;

    for (List<ExynosVisionSubgraph*>::iterator subgraph_iter = m_sg_list.begin(); subgraph_iter != m_sg_list.end(); subgraph_iter++ ) {
        status = (*subgraph_iter)->fixSubgraph();
        if (status != VX_SUCCESS) {
            VXLOGE("subgraph is not fixed, err:%d", status);
            break;
        }

        if ((*subgraph_iter)->isHeader()) {
            m_sg_header_list.push_back((*subgraph_iter));
        }

        if ((*subgraph_iter)->isFooter()) {
            m_sg_footer_list.push_back((*subgraph_iter));
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::checkExecutionModePropriety(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;

    if (getExecMode() == GRAPH_EXEC_STREAM) {
        for (List<ExynosVisionNode*>::iterator iter_node = m_sorted_node_list.begin(); iter_node != m_sorted_node_list.end(); iter_node++ ) {
            for (vx_uint32 p = 0; p < (*iter_node)->getKernelHandle()->getNumParams(); p++) {
                ExynosVisionDataReference *data_ref =  (*iter_node)->getDataRefByIndex(p);
                if (data_ref == NULL)
                    continue;

                /* there are some restricts in stream mode.
                    1. can't contain delay object
                    2. can't contain alliance object.
                    3. can't contain object that doesn't have own resource. */
                if ((data_ref->isDelayElement() == vx_true_e) ||
                    (data_ref->getDirectInputNodeNum(this) != data_ref->getIndirectInputNodeNum(this)) ||
                    (data_ref->getDirectOutputNodeNum(this) != data_ref->getIndirectOutputNodeNum(this)) ||
                    (data_ref->getResourceType() == RESOURCE_MNGR_NULL)) {
                    VXLOGE("%s can't support stream mode", data_ref->getName());
                    status = VX_FAILURE;
                    goto EXIT;
                }
            }
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

EXIT:
    return status;
}

vx_status
ExynosVisionGraph::setParentNode(ExynosVisionNode* node)
{
    m_parent_node = node;

    return VX_SUCCESS;
}

vx_status
ExynosVisionGraph::processGraph(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_FAILURE;

    if (getExecMode() != GRAPH_EXEC_NORMAL) {
        VXLOGE("processing graph could be executed only on normal mode");
        return VX_FAILURE;
    }

    if (m_verified == vx_false_e) {
        status = verifyGraph();
        if (status != VX_SUCCESS) {
            VXLOGE("verifying graph fail, err:%d", status);
            return status;
        }
    }

    Mutex::Autolock lock(m_external_lock);

    if (m_graph_state.getState() != GRAPH_STATE_VERIFIED) {
        VXLOGE("%s is not verified", getName());
        return status;
    }

    m_error_status = VX_SUCCESS;
    m_graph_state.setState(GRAPH_STATE_PROCESSING);

    vx_uint32 frame_cnt;

    if (m_parent_node == NULL) {
        frame_cnt = requestNewFrameCnt(this);
    } else {
        /* child graph */
        frame_cnt = m_parent_node->getCurFrameCnt();
    }

    time_pair_t *time_pair = getContext()->getPerfMonitor()->requestTimePairStr(this, frame_cnt);
    TIMESTAMP_START(time_pair, TIMEPAIR_PROCESS);

    List<ExynosVisionSubgraph*>::iterator sg_iter;
    for (sg_iter=m_sg_footer_list.begin(); sg_iter!=m_sg_footer_list.end(); sg_iter++) {
        VXLOGD2("%s, clear complete flag", (*sg_iter)->getSgName());
        status = (*sg_iter)->clearSubgraphComplete();
        if (status != VX_SUCCESS) {
            VXLOGE("clearing subgraph complete flag fails, err:%d", status);
        }
    }

    for (sg_iter=m_sg_header_list.begin(); sg_iter!=m_sg_header_list.end(); sg_iter++) {
        VXLOGD2("trigger to %s to start", (*sg_iter)->getSgName());
        status = (*sg_iter)->pushTrigger(frame_cnt);
        if (status != VX_SUCCESS) {
            VXLOGE("pushing done event fails, err:%d", status);
        }
    }

    for (sg_iter=m_sg_footer_list.begin(); sg_iter!=m_sg_footer_list.end(); sg_iter++) {
        VXLOGD2("waiting complete signal from %s", (*sg_iter)->getSgName());
        status = (*sg_iter)->waitSubgraphComplete(m_sg_list.size() * COMPLETE_WAIT_TIME);
        if (status != VX_SUCCESS) {
            VXLOGE("waiting complete signal fails from %s, err:%d", (*sg_iter)->getSgName(), status);
            /* update error status if any error is not reproted. */
            if (m_error_status == VX_SUCCESS)
                m_error_status = status;
        } else {
            VXLOGD2("receive complete signal from %s", (*sg_iter)->getSgName());
        }
    }

    m_graph_state.setState(GRAPH_STATE_VERIFIED);

    TIMESTAMP_END(time_pair, TIMEPAIR_PROCESS);
    if (time_pair) {
        getContext()->getPerfMonitor()->releaseTimePairStr(this, frame_cnt, time_pair);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return m_error_status;
}

vx_status
ExynosVisionGraph::stopProcessGraph(void)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_FAILURE;
    List<ExynosVisionSubgraph*>::iterator sg_iter;

    for (sg_iter=m_sg_list.begin(); sg_iter!=m_sg_list.end(); sg_iter++) {
        VXLOGD("request stop to %s", (*sg_iter)->getSgName());
        status = (*sg_iter)->flushWaitEvent();
        if (status != VX_SUCCESS) {
            VXLOGE("clearing subgraph done fails, err:%d", status);
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionGraph::scheduleGraph(void)
{
    vx_status status;
    m_schedule_complete_event->clearEvent();
    status = pushScheduleEvent();
    if (status != VX_SUCCESS) {
        VXLOGE("scheudling graph fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVisionGraph::waitGraph(void)
{
    vx_status status = VX_SUCCESS;

    event_exception_t exception_state;

    VXLOGD2("Start waiting %s complete", getName());
    exception_state = m_schedule_complete_event->waitEvent();
    VXLOGD2("End waiting %s complete, exception:%d", getName(), exception_state);

    if (exception_state != EVENT_EXCEPTION_NONE) {
        status = VX_FAILURE;
    }

    return status;
}

vx_status
ExynosVisionGraph::queryGraph(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_GRAPH_ATTRIBUTE_PERFORMANCE:
        if (VX_CHECK_PARAM(ptr, size, vx_perf_t, 0x3) && (getContext()->getPerfMonitor())) {
            vx_perf_t *vx_perf = getContext()->getPerfMonitor()->getVxPerfInfo(this);
            if (vx_perf)
                memcpy(ptr, vx_perf,  size);
            else
                memset(ptr, 0x0, size);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_GRAPH_ATTRIBUTE_STATUS:
        if (VX_CHECK_PARAM(ptr, size, vx_status, 0x3))
            *(vx_status *)ptr = m_error_status;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_GRAPH_ATTRIBUTE_NUMNODES:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_node_list.size();
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_GRAPH_ATTRIBUTE_NUMPARAMETERS:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            *(vx_uint32 *)ptr = getNumParams();
            status = VX_ERROR_INVALID_PARAMETERS;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionGraph::setGraphAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_ERROR_NOT_SUPPORTED;

    VXLOGE("there are no settable attributes, attribute:0x%x, ptr:%p, size:%d", attribute, ptr, size);

    return status;
}

vx_uint32
ExynosVisionGraph::requestNewFrameCnt(ExynosVisionReference *ref)
{
    return ++m_frame_cnt_map[ref];
}

vx_status
ExynosVisionGraph::pushErrorEvent(ExynosVisionSubgraph *subgraph, vx_status error)
{
    VXLOGE("push error:%d from %s", error, subgraph->getSgName());

    graph_error_message_t error_message;
    error_message.subgraph = subgraph;
    error_message.error = error;

    m_error_queue->pushProcessQ(&error_message);

    return VX_SUCCESS;
}

queue_exception_t
ExynosVisionGraph::popErrorEvent(graph_error_message_t *error_message)
{
    vx_status status;
    queue_exception_t exception;

    if (!m_error_queue)
        VXLOGE("m_error_queue is NULL");

    exception = m_error_queue->waitAndPopProcessQ(error_message);
    if (exception == QUEUE_EXCEPTION_TIME_OUT)
        VXLOGE("queue fails, exception:%d", exception);

    return exception;
}

vx_status
ExynosVisionGraph::pushScheduleEvent(void)
{
    graph_schedule_message_t schedule_message;

    m_schedule_queue->pushProcessQ(&schedule_message);

    return VX_SUCCESS;
}

queue_exception_t
ExynosVisionGraph::popScheduleEvent(graph_schedule_message_t *schedule_message)
{
    vx_status status;
    queue_exception_t exception;

    if (!m_schedule_queue)
        VXLOGE("m_schedule_queue is NULL");

    exception = m_schedule_queue->waitAndPopProcessQ(schedule_message);
    if (exception == QUEUE_EXCEPTION_TIME_OUT)
        VXLOGE("queue fails, exception:%d", exception);

    return exception;
}

vx_status
ExynosVisionGraph::stopGraph()
{
    stopProcessGraph();

    return VX_SUCCESS;
}

bool
ExynosVisionGraph::errorThreadFunc(void)
{
    vx_status status;
    vx_uint32 frame_cnt;
    queue_exception_t exception;
    graph_error_message_t error_message;

    VXLOGD2("[TD]err-thread, waiting to err event");
    exception = popErrorEvent(&error_message);
    VXLOGD2("[TD]err-thread, poping err event, exception:%d", exception);
    if (exception == QUEUE_EXCEPTION_NONE) {
        if (error_message.error != VX_SUCCESS) {
            VXLOGE("pop error event, %s, error:%d", error_message.subgraph->getSgName(), error_message.error);
            m_error_status |= error_message.error;
            stopGraph();
        } else {
            VXLOGE("error report is corrupted");
        }
    }

    if (exception == QUEUE_EXCEPTION_DISABLE)
        return false;
    else
        return true;
}

bool
ExynosVisionGraph::scheduleThreadFunc(void)
{
    vx_status status;
    vx_uint32 frame_cnt;
    queue_exception_t exception;
    graph_schedule_message_t schedule_message;

    VXLOGD2("[TD]shd-thread, waiting to schedule event");
    exception = popScheduleEvent(&schedule_message);
    VXLOGD2("[TD]shd-thread, poping schedule event, exception:%d", exception);
    if (exception == QUEUE_EXCEPTION_NONE)
        processGraph();

    if (m_schedule_queue->getRemainedMsgNum() == 0)
        m_schedule_complete_event->setEvent();

    if (exception == QUEUE_EXCEPTION_DISABLE)
        return false;
    else
        return true;
}

void
ExynosVisionGraph::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s========================================================", MAKE_TAB(tap, tab_num));
    VXLOGI("%s[Graph  ][%d] %s, node_num:%d, refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(), m_node_list.size(),
                                                                        getInternalCnt(), getExternalCnt());

    if (m_parent_node)
        VXLOGI("%s          Parent node:%s(%s)", MAKE_TAB(tap, tab_num), m_parent_node->getName(), m_parent_node->getKernelName());

    VXLOGI("%s[-------] all subgraph: %d", MAKE_TAB(tap, tab_num), m_sg_list.size());
    if (m_sg_list.size() != 0) {
        List<ExynosVisionSubgraph*>::iterator sg_iter;
        for (sg_iter=m_sg_list.begin(); sg_iter!=m_sg_list.end(); sg_iter++)
            (*sg_iter)->displayInfo(tab_num+1, vx_true_e);
    } else {
        List<ExynosVisionNode*>::iterator node_iter;
        for (node_iter=m_node_list.begin(); node_iter!=m_node_list.end(); node_iter++)
            (*node_iter)->displayInfo(tab_num+1, vx_true_e);
    }

    VXLOGI("%s========================================================", MAKE_TAB(tap, tab_num));
}

void
ExynosVisionGraph::displayPerf(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s========================================================", MAKE_TAB(tap, tab_num));
    VXLOGI("%s[Graph  ] %s", MAKE_TAB(tap, tab_num), getName());

    vx_perf_t *vx_perf = getContext()->getPerfMonitor()->getVxPerfInfo(this);

    if (getContext()->getPerfMonitor() == NULL) {
        VXLOGE("unregistered graph, %s", getName());
        return;
    }

    VXLOGI("%s\tverify: %0.3lf ms", MAKE_TAB(tap, tab_num), (vx_float32)m_verify_time/1000.0f);
    VXLOGI("%s\tprocess [%llu]: %0.3lf ms", MAKE_TAB(tap, tab_num), vx_perf[TIMEPAIR_PROCESS].num, (vx_float32)vx_perf[TIMEPAIR_PROCESS].avg/1000.0f);

    if (detail_info == vx_true_e) {
        List<ExynosVisionSubgraph*>::iterator sg_iter;
        for (sg_iter=m_sg_list.begin(); sg_iter!=m_sg_list.end(); sg_iter++)
            (*sg_iter)->displayPerf(tab_num+1, vx_true_e);
    }

    VXLOGI("%s========================================================", MAKE_TAB(tap, tab_num));
}

}; /* namespace android */
