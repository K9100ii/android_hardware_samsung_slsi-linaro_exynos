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

#ifndef EXYNOS_OPENVX_NODE_H
#define EXYNOS_OPENVX_NODE_H

#include <VX/vx.h>

#include "ExynosVisionPerfMonitor.h"

#include "ExynosVisionReference.h"
#include "ExynosVisionContext.h"
#include "ExynosVisionKernel.h"
#include "ExynosVisionParameter.h"
#include "ExynosVisionMeta.h"
#include "ExynosVisionScalar.h"

namespace android {

class ExynosVisionNode : public ExynosVisionReference {
private:
    /*! \brief The pointer to the kernel structure */
    ExynosVisionKernel  *m_kernel;
    /*! \brief The list of references which are the values to pass to the kernels */
    Vector<ExynosVisionDataReference*> m_input_data_ref_vector;
    Vector<ExynosVisionDataReference*> m_output_data_ref_vector;
    /*! \brief Status code returned from the last execution of the kernel. */
    vx_status           m_node_status;
    /*! \brief A back reference to the parent graph. */
    ExynosVisionGraph   *m_parent_graph;
    /*! \brief The instance version of the attributes of the kernel */
    vx_kernel_attr_t    m_attributes;
    /*! \brief The child graph of the node. */
    ExynosVisionGraph   *m_child_graph;

    vx_uint32 m_cur_frame_cnt;
    /* request and release time stamp structure during run-time */
    time_pair_t *m_time_pair;

    vx_bool m_share_resource;

    vx_uint32 m_pre_node_num;
    vx_uint32 m_post_node_num;

    ExynosVisionSubgraph            *m_subgraph;

public:
    /*! \brief A callback to call when the node is complete */
    vx_nodecomplete_f   m_callback;

private:
    vx_bool clearDataRefByIndex(vx_uint32 index);
    vx_status setDataReferenceByIndex(vx_uint32 index, ExynosVisionDataReference *data_ref);
    vx_int32 getSubIndexFromIndex(vx_enum dir, vx_int32 index);
    vx_int32 getIndexFromSubIndex(vx_enum dir, vx_int32 sub_index);

public:
    /* Constructor */
    ExynosVisionNode(ExynosVisionContext *context, ExynosVisionGraph *graph);

    /* Destructor */
    virtual ~ExynosVisionNode();

    vx_status init(ExynosVisionGraph *graph, ExynosVisionKernel *kernel);
    virtual vx_status destroy(void);
    vx_status disconnectParentGraph(void);

    const vx_char* getKernelName(void) const;
    vx_status setParameterByIndex(vx_uint32 index, ExynosVisionDataReference *data_ref);
    vx_status queryNode(vx_enum attribute, void *ptr, vx_size size);
    vx_status setNodeAttribute(vx_enum attribute, const void *ptr, vx_size size);
    ExynosVisionParameter *getParameterByIndex(vx_uint32 index);
    vx_status nodeSetParameter(vx_uint32 index, ExynosVisionDataReference *data_ref);

    vx_status setNodeTarget(vx_enum target_enum);

    vx_status assignNodeCallback(vx_nodecomplete_f callback);

    ExynosVisionDataReference* getDataRefByIndex(vx_uint32 index);
    ExynosVisionDataReference* getInputDataRefByIndex(vx_uint32 in_index);
    ExynosVisionDataReference* getOutputDataRefByIndex(vx_uint32 out_index);

    vx_status initilalizeKernel(void);

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);

    /* Calculate previous and post node number */
    void calculateConnectedNode(void);
    vx_bool isHeader(void)
    {
        if (m_pre_node_num == 0)
            return vx_true_e;
        else
            return vx_false_e;
    }
    vx_bool isFooter(void)
    {
        if (m_post_node_num == 0)
            return vx_true_e;
        else
            return vx_false_e;
    }
    vx_uint32 getPreNodeNum(void)
    {
        return m_pre_node_num;
    }
    vx_uint32 getPostNodeNum(void)
    {
        return m_post_node_num;
    }
    vx_uint32 getInputDataRefNum(void)
    {
        return m_input_data_ref_vector.size();
    }
    vx_uint32 getOutputDataRefNum(void)
    {
        return m_output_data_ref_vector.size();
    }
    vx_uint32 getDataRefNum(void)
    {
        return m_input_data_ref_vector.size() + m_output_data_ref_vector.size();
    }
    enum vx_parameter_state_e getParamState(vx_enum dir, vx_uint32 sub_index)
    {
        vx_int32 index = getIndexFromSubIndex(dir, sub_index);
        if (index < 0)
            VXLOGE("can't find index");

        return m_kernel->getParamState(index);
    }

    void informKernelStart(vx_uint32 frame_cnt);
    void informKernelEnd(vx_uint32 frame_cnt, vx_status status);

    vx_uint32 getCurFrameCnt(void)
    {
        return m_cur_frame_cnt;
    }

    void setSubgraph(ExynosVisionSubgraph *subgraph)
    {
        m_subgraph = subgraph;
    }
    ExynosVisionSubgraph *getSubgraph(void)
    {
        return m_subgraph;
    }

    vx_status verifyNode(void);
    const ExynosVisionKernel* getKernelHandle(void) const
    {
        return m_kernel;
    }
    ExynosVisionGraph* getParentGraph(void)
    {
        return m_parent_graph;
    }

    vx_status setChildGraphOfNode(ExynosVisionGraph *graph);
    ExynosVisionGraph* getChildGraphOfNode()
    {
        return m_child_graph;
    }
};

}; // namespace android
#endif
