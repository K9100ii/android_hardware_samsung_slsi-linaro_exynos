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

#define LOG_TAG "ExynosVisionNode"
#include <cutils/log.h>

#include "ExynosVisionAutoTimer.h"

#include "ExynosVisionNode.h"

#include "ExynosVisionGraph.h"
#include "ExynosVisionSubgraph.h"
#include "ExynosVisionDelay.h"

namespace android {

ExynosVisionNode::ExynosVisionNode(ExynosVisionContext *context, ExynosVisionGraph *graph)
                                                                : ExynosVisionReference(context, VX_TYPE_NODE, (ExynosVisionReference*)graph, vx_true_e)
{
    m_kernel = NULL;

    m_node_status = VX_SUCCESS;
    m_callback = NULL;

    m_parent_graph = NULL;
    memset(&m_attributes, 0x0, sizeof(m_attributes));
    m_child_graph = NULL;

    m_cur_frame_cnt = 0;
    m_time_pair = NULL;

    /* JUN_TBD, this will be changed to vx_true_e after vpu is stable */
    m_share_resource = vx_false_e;

    m_pre_node_num = 0;
    m_post_node_num = 0;

    m_subgraph = NULL;
}

ExynosVisionNode::~ExynosVisionNode(void)
{

}

vx_status
ExynosVisionNode::init(ExynosVisionGraph *graph, ExynosVisionKernel *kernel)
{
    m_parent_graph = graph;
    m_kernel = kernel;

    kernel->incrementReference(VX_REF_INTERNAL, this);
    kernel->fiiledAttr(&m_attributes);

    m_input_data_ref_vector.clear();
    m_output_data_ref_vector.clear();

    for (vx_uint32 i=0; i<kernel->getNumParams(); i++) {
        if (kernel->getParamDirection(i) == VX_INPUT)
            m_input_data_ref_vector.push_back(NULL);
        else
            m_output_data_ref_vector.push_back(NULL);
    }

    return VX_SUCCESS;
}

vx_status
ExynosVisionNode::disconnectParentGraph(void)
{
    vx_status status = VX_SUCCESS;

    if (m_parent_graph == NULL) {
        VXLOGE("%s already doesn't belong to any graph", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    /* release data references which are connected. */
    for (vx_uint32 p = 0; p < getDataRefNum(); p++) {
        ExynosVisionDataReference *ref = getDataRefByIndex(p);
        if (ref) {
            if (ref->disconnectNode(m_parent_graph, this, p, m_kernel->getParamDirection(p)) != vx_true_e)
                VXLOGE("cannot remove %s at %s", getName(), ref->getName());

            /* remove the potential delay association */
            if (ref->isDelayElement()) {
                vx_bool res = ref->getDelay()->removeAssociationToDelay(ref, this, p);
                if (res == vx_false_e) {
                    VXLOGE("Internal error removing delay association");
                }

                ExynosVisionDelay *delay = ref->getDelay();
                status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&delay, VX_REF_INTERNAL, this);
                if (status != VX_SUCCESS) {
                    VXLOGE("releasing reference count fails at %s", ref->getDelay()->getName());
                }
            }

            ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&ref, VX_REF_INTERNAL, this);

            clearDataRefByIndex(p);
        }
    }

    m_parent_graph = NULL;

EXIT:
    return status;
}

vx_status
ExynosVisionNode::destroy(void)
{
    vx_status status = VX_SUCCESS;

    VXLOGD2("%s(%s) destroy", getName(), getKernelName());

    if (m_kernel) {
        const ExynosVisionDataReference *params[VX_INT_MAX_PARAMS];
        for (vx_uint32 i=0; i<getDataRefNum(); i++) {
            params[i] = getDataRefByIndex(i);
            if (params[i] != NULL) {
                VXLOGD2("params[%d] : %s", i, params[i]->getName());
            } else {
                VXLOGD2("params[%d] : NULL", i);
            }
        }
        m_kernel->deinitialize(this, params, getDataRefNum());
    }

    if (m_child_graph) {
        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_child_graph, VX_REF_INTERNAL, this);
        m_child_graph = NULL;
    }

    if (m_parent_graph) {
        disconnectParentGraph();
    }

    /* free the local memory */
    if (m_attributes.localDataPtr) {
        free(m_attributes.localDataPtr);
        m_attributes.localDataPtr = NULL;
    }

    if (m_kernel) {
        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_kernel, VX_REF_INTERNAL, this);
        m_kernel = NULL;
    }

    return status;
}

const vx_char*
ExynosVisionNode::getKernelName(void) const
{
    EXYNOS_VISION_SYSTEM_IN();
    if (m_kernel) {
        return m_kernel->getKernelName();
    } else {
        VXLOGE("kernel is not assigned");
        return NULL;
    }

    EXYNOS_VISION_SYSTEM_OUT();
}

vx_status
ExynosVisionNode::setParameterByIndex(vx_uint32 index, ExynosVisionDataReference *data_ref)
{
    EXYNOS_VISION_SYSTEM_IN();

    vx_status status = VX_SUCCESS;
    vx_enum type = 0;
    vx_enum scalar_data_type = 0;

    /* is the index out of bounds? */
    if ((index >= m_kernel->getNumParams()) || (index >= VX_INT_MAX_PARAMS)) {
        VXLOGE("Invalid index %u", index);
        status = VX_ERROR_INVALID_VALUE;
        goto EXIT;
    }

    /* if it's an optional parameter, it's ok to be NULL */
    if ((data_ref == NULL) && (m_kernel->getParamState(index) == VX_PARAMETER_STATE_OPTIONAL)) {
        status = VX_SUCCESS;
        goto EXIT;
    }

    /* if it was a valid reference then get the type from it */
    data_ref->queryReference(VX_REF_ATTRIBUTE_TYPE, &type, sizeof(type));
    /* Check that signature type matches reference type*/
    if (m_kernel->getParamType(index) != type) {
        /* Check special case where signature is a specific scalar type.
           This can happen if the vxAddParameterToKernel() passes one of the scalar
           vx_type_e types instead of the more generic VX_TYPE_SCALAR since the spec
           doesn't specify that only VX_TYPE_SCALAR should be used for scalar types in
           this function. */
        if ((type == VX_TYPE_SCALAR) &&
           (((ExynosVisionScalar*)data_ref)->queryScalar(VX_SCALAR_ATTRIBUTE_TYPE, &scalar_data_type, sizeof(scalar_data_type)) == VX_SUCCESS)) {
            if (scalar_data_type != m_kernel->getParamType(index)) {
                VXLOGE("Invalid scalar type 0x%08x!", scalar_data_type);
                status = VX_ERROR_INVALID_TYPE;
                goto EXIT;
            }
        } else {
            VXLOGE("Invalid type 0x%08x!\n", type);
            status = VX_ERROR_INVALID_TYPE;
            goto EXIT;
        }
    }

    /* actual change of the node parameter */
    status = nodeSetParameter(index, data_ref);
    if (status != VX_SUCCESS) {
        VXLOGE("setting parameter to %s fails, err:%d", getName(), status);
        goto EXIT;
    }

    /* Note that we don't need to do anything special for parameters to child graphs. */

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    if (status == VX_SUCCESS) {
        VXLOGD2("assigned %s to %s(index:%u)", data_ref->getName(), getName(), index);
    } else {
        VXLOGE("Specified: parameter[%u] type:0x%X ref=%d\n", index, type, data_ref->getId());
        VXLOGE("Required: parameter[%u] dir:%d type:0x%X\n", index,
                                                                                            m_kernel->getParamDirection(index),
                                                                                            m_kernel->getParamType(index));
    }

    return status;
}

vx_status
ExynosVisionNode::queryNode(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_NODE_ATTRIBUTE_PERFORMANCE:
        if (VX_CHECK_PARAM(ptr, size, vx_perf_t, 0x3) && (m_parent_graph->getPerfMonitor())) {
            vx_perf_t *vx_perf = m_parent_graph->getPerfMonitor()->getVxPerfInfo(this);
            memcpy(ptr, vx_perf,  size);
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_NODE_ATTRIBUTE_STATUS:
        if (VX_CHECK_PARAM(ptr, size, vx_status, 0x3))
            *(vx_status *)ptr = m_node_status;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_LOCAL_DATA_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = m_attributes.localDataSize;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_LOCAL_DATA_PTR:
        if (VX_CHECK_PARAM(ptr, size, vx_ptr_t, 0x3))
            *(vx_ptr_t *)ptr = m_attributes.localDataPtr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_BORDER_MODE:
        if (VX_CHECK_PARAM(ptr, size, vx_border_mode_t, 0x3))
            memcpy((vx_border_mode_t *)ptr, &m_attributes.borders, sizeof(vx_border_mode_t));
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_FRAME_NUMBER:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_cur_frame_cnt;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_LOCAL_INFO_PTR:
        if (VX_CHECK_PARAM(ptr, size, vx_ptr_t, 0x3)) {
            if (m_time_pair) {
                *(vx_ptr_t *)ptr = m_time_pair;
            } else {
                status = VX_ERROR_NOT_SUPPORTED;
            }
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_NODE_ATTRIBUTE_KERNEL_NAME:
        *(const vx_char**)ptr = getKernelName();
        break;
    case VX_NODE_ATTRIBUTE_PRIORITY:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3)) {
            if (m_parent_graph->getExecMode() == GRAPH_EXEC_STREAM)
                *(vx_uint32*)ptr = 1;
            else
                *(vx_uint32*)ptr = 10;
        } else {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        break;
    case VX_NODE_ATTRIBUTE_SHARE_RESOURCE:
        if (VX_CHECK_PARAM(ptr, size, vx_bool, 0x3)) {
            *(vx_uint32*)ptr = m_share_resource;
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
ExynosVisionNode::setNodeAttribute(vx_enum attribute, const void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    if (m_parent_graph->isGraphVerified() == vx_true_e)
        return VX_ERROR_NOT_SUPPORTED;

    switch (attribute) {
    case VX_NODE_ATTRIBUTE_LOCAL_DATA_SIZE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            m_attributes.localDataSize = *(vx_size *)ptr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_LOCAL_DATA_PTR:
        if (VX_CHECK_PARAM(ptr, size, vx_ptr_t, 0x3))
            m_attributes.localDataPtr = *(vx_ptr_t *)ptr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_BORDER_MODE:
        if (VX_CHECK_PARAM(ptr, size, vx_border_mode_t, 0x3))
            memcpy(&m_attributes.borders, (vx_border_mode_t *)ptr, sizeof(vx_border_mode_t));
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_NODE_ATTRIBUTE_SHARE_RESOURCE:
        if (VX_CHECK_PARAM(ptr, size, vx_bool, 0x3))
            m_share_resource = *(vx_bool*)ptr;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

ExynosVisionParameter*
ExynosVisionNode::getParameterByIndex(vx_uint32 index)
{
    EXYNOS_VISION_SYSTEM_IN();
    ExynosVisionParameter *param = NULL;

    if (m_kernel == NULL) {
        /* this can probably never happen */
        getContext()->addLogEntry(this, VX_ERROR_INVALID_NODE, "Node was created without a kernel! Fatal Error!\n");
        param = (ExynosVisionParameter*)getContext()->getErrorObject(VX_ERROR_INVALID_NODE);
    } else {
        if (/*0 <= index &&*/ index < VX_INT_MAX_PARAMS && index < m_kernel->getNumParams()) {
            param = new ExynosVisionParameter(getContext(), this);
            param->init(index, this, m_kernel);
        }
        else
        {
            getContext()->addLogEntry(this, VX_ERROR_INVALID_PARAMETERS, "Index %u out of range for node %s (numparams = %u)!\n",
                    index, getKernelName(), m_kernel->getNumParams());
            VXLOGE("Index %u out of range for node %s (numparams = %u)",
                    index, getKernelName(), m_kernel->getNumParams());
            param = (ExynosVisionParameter*)getContext()->getErrorObject(VX_ERROR_INVALID_PARAMETERS);
        }
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return param;
}

vx_status
ExynosVisionNode::nodeSetParameter(vx_uint32 index, ExynosVisionDataReference *data_ref)
{
    vx_status status = VX_SUCCESS;

    EXYNOS_VISION_SYSTEM_IN();

    if (m_parent_graph->isWorking()) {
        VXLOGE("cannot change %s parameter during graph is working", getName());
        return VX_FAILURE;
    }

    ExynosVisionDataReference *old_data_ref = getDataRefByIndex(index);

    if (old_data_ref) {
        VXLOGD2("release already assigned %s", old_data_ref->getName());

        if (old_data_ref->isDelayElement()) {
            /* we already have a delay element here */
            vx_bool res = old_data_ref->getDelay()->removeAssociationToDelay(old_data_ref, this, index);
            if (res == vx_false_e) {
                VXLOGE("Internal error removing delay association");
                status = VX_ERROR_INVALID_REFERENCE;
                goto EXIT;
            }

            ExynosVisionDelay *delay = old_data_ref->getDelay();
            status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&delay, VX_REF_INTERNAL, this);
            if (status != VX_SUCCESS) {
                VXLOGE("releasing reference count fails at %s", old_data_ref->getDelay()->getName());
                goto EXIT;
            }
        }

        if (old_data_ref->disconnectNode(m_parent_graph, this, index, m_kernel->getParamDirection(index)) != vx_true_e) {
            VXLOGE("Removing %s failed at %s", getName(), old_data_ref->getName());
            status = VX_ERROR_INVALID_REFERENCE;
            goto EXIT;
        } else {
            VXLOGD2("Removing %s successed at %s", getName(), old_data_ref->getName());
        }

        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&old_data_ref, VX_REF_INTERNAL, this);
    }

    if (data_ref->isDelayElement()) {
        /* the new parameter is a delay element */
        vx_bool res = data_ref->getDelay()->addAssociationToDelay(data_ref, this, index);
        data_ref->getDelay()->incrementReference(VX_REF_INTERNAL, this);
        if (res == vx_false_e) {
            VXLOGE("Internal error adding delay association");
            status = VX_ERROR_INVALID_REFERENCE;
            goto EXIT;
        }
    }

    if (m_subgraph) {
        if (m_subgraph->replaceDataRef(old_data_ref, data_ref, index, m_kernel->getParamDirection(index)) != VX_SUCCESS)
            VXLOGE("%s cannot replace old reference", m_subgraph->getSgName());
    }

    setDataReferenceByIndex(index, data_ref);
    data_ref->connectNode(m_parent_graph, this, index, m_kernel->getParamDirection(index));

EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

 vx_status
ExynosVisionNode::setNodeTarget(vx_enum target_enum)
{
    vx_status status = VX_SUCCESS;

    EXYNOS_VISION_SYSTEM_IN();

    ExynosVisionKernel *kernel = NULL;
    vx_enum kernel_enum = m_kernel->getEnumeration();

    if (target_enum == VX_TARGET_ANY) {
        kernel = getContext()->getKernelByEnum(kernel_enum);
        if (strcmp(kernel->getKernelName(), m_kernel->getKernelName()) == 0) {
            VXLOGD3("No need to change target for node");
            goto EXIT;
        }
    } else {
        kernel = getContext()->getKernelByTarget(kernel_enum, target_enum);
        if (kernel == NULL) {
            VXLOGW("not available %s kernel at target(0x%X), target is not changed", m_kernel->getKernelFuncName(), target_enum);
            status = VX_ERROR_NOT_SUPPORTED;
            goto EXIT;
        }

        if (strcmp(kernel->getKernelName(), m_kernel->getKernelName()) == 0) {
            VXLOGD3("No need to change target for node");
            goto EXIT;
        }

        /* Deinitialize the original kernel */
        if(m_kernel) {
            const ExynosVisionDataReference *params[VX_INT_MAX_PARAMS];
            for (vx_uint32 i = 0; i < getDataRefNum(); i++) {
                params[i] = getDataRefByIndex(i);
                if (params[i] != NULL) {
                    VXLOGD3("params[%d] : %s", i, params[i]->getName());
                } else {
                    VXLOGD3("params[%d] : NULL", i);
                }
            }
            m_kernel->deinitialize(this, params, getDataRefNum());

            /* free the local memory */
            if (m_attributes.localDataPtr) {
                free(m_attributes.localDataPtr);
                m_attributes.localDataPtr = NULL;
            }

            ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_kernel, VX_REF_INTERNAL, this);
            m_kernel = NULL;
        }

        m_kernel = kernel;
        kernel->incrementReference(VX_REF_INTERNAL, this);
        kernel->fiiledAttr(&m_attributes);

        m_parent_graph->invalidateGraph();
    }
EXIT:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionNode::assignNodeCallback(vx_nodecomplete_f callback)
{
    vx_status status = VX_FAILURE;

    if ((callback) && (m_callback)) {
        VXLOGE("attempting to overriding existing callback %p on %s\n", m_callback, getName());
        status = VX_ERROR_NOT_SUPPORTED;
    } else {
        m_callback = callback;
        status = VX_SUCCESS;
    }

    return status;
}

vx_status
ExynosVisionNode::setDataReferenceByIndex(vx_uint32 index, ExynosVisionDataReference *data_ref)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;

    if (index >= m_kernel->getNumParams()) {
        VXLOGE("Invalid index %u", index);
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_int32 sub_index;
    if (m_kernel->getParamDirection(index) == VX_INPUT) {
        sub_index = getSubIndexFromIndex(VX_INPUT, index);
        if (sub_index >= 0)
            m_input_data_ref_vector.editItemAt(sub_index) = data_ref;
        else
            VXLOGE("can't find sub-index");
    } else {
        sub_index = getSubIndexFromIndex(VX_OUTPUT, index);
        if (sub_index >= 0)
            m_output_data_ref_vector.editItemAt(sub_index) = data_ref;
        else
            VXLOGE("can't find sub-index");
    }
    data_ref->incrementReference(VX_REF_INTERNAL, this);

    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_int32
ExynosVisionNode::getSubIndexFromIndex(vx_enum dir, vx_int32 index)
{
    vx_int32 sub_index = -1;

    for (vx_int32 i=0; i<(index+1); i++) {
        if (dir == VX_INPUT) {
            if (m_kernel->getParamDirection(i) == dir)
                sub_index++;
        } else {
            /* bi-directional has the output property. */
            if ((m_kernel->getParamDirection(i) == dir) || (m_kernel->getParamDirection(i) == VX_BIDIRECTIONAL))
                sub_index++;
        }
    }

    return sub_index;
}
vx_int32
ExynosVisionNode::getIndexFromSubIndex(vx_enum dir, vx_int32 sub_index)
{
    vx_int32 index = -1;
    vx_uint32 sub_param_cnt = 0;

    for (vx_uint32 i=0; i<m_kernel->getNumParams(); i++) {
        if (dir == VX_INPUT) {
            if (m_kernel->getParamDirection(i) == VX_INPUT)
                sub_param_cnt++;
        } else {
            /* bi-directional has the output property. */
            if ((m_kernel->getParamDirection(i) == VX_OUTPUT) || (m_kernel->getParamDirection(i) == VX_BIDIRECTIONAL))
                sub_param_cnt++;
        }

        if ((vx_int32)sub_param_cnt == (sub_index+1)) {
            index = i;
            break;
        }
    }

    if (index == -1)
        VXLOGE("can't find kernel index from sub-index");

    return index;
}


vx_bool
ExynosVisionNode::clearDataRefByIndex(vx_uint32 index)
{
    EXYNOS_VISION_SYSTEM_IN();

    if (index >= m_kernel->getNumParams()) {
        VXLOGE("Invalid index %u", index);
        return vx_false_e;
    }

    vx_int32 sub_index;
    if (m_kernel->getParamDirection(index) == VX_INPUT) {
        sub_index = getSubIndexFromIndex(VX_INPUT, index);
        if (sub_index >= 0)
            m_input_data_ref_vector.editItemAt(sub_index) = NULL;
        else
            VXLOGE("can't find sub-index");
    } else {
        sub_index = getSubIndexFromIndex(VX_OUTPUT, index);
        if (sub_index >= 0)
            m_output_data_ref_vector.editItemAt(sub_index) = NULL;
        else
            VXLOGE("can't find sub-index");
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return vx_true_e;
}

ExynosVisionDataReference*
ExynosVisionNode::getDataRefByIndex(vx_uint32 index)
{
    EXYNOS_VISION_SYSTEM_IN();
    ExynosVisionDataReference *data_ref = NULL;

    if (index >= m_kernel->getNumParams()) {
        VXLOGE("%s, invalid index %u, num_params: %d", getName(), index, m_kernel->getNumParams());
        return NULL;
    }

    vx_int32 sub_index;
    if (m_kernel->getParamDirection(index) == VX_INPUT) {
        sub_index = getSubIndexFromIndex(VX_INPUT, index);
        if (sub_index >= 0)
            data_ref = m_input_data_ref_vector.editItemAt(sub_index);
        else
            VXLOGE("can't find sub-index");
    } else {
        sub_index = getSubIndexFromIndex(VX_OUTPUT, index);
        if (sub_index >= 0)
            data_ref = m_output_data_ref_vector.editItemAt(sub_index);
        else
            VXLOGE("can't find sub-index");
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return data_ref;
}

ExynosVisionDataReference*
ExynosVisionNode::getInputDataRefByIndex(vx_uint32 in_index)
{
    EXYNOS_VISION_SYSTEM_IN();
    Mutex::Autolock lock(m_internal_lock);

    if (in_index > m_input_data_ref_vector.size()) {
        VXLOGE("index is out of bound, index:%d", in_index);
        return NULL;
    }

    ExynosVisionDataReference *data_ref= m_input_data_ref_vector[in_index];

    if ((data_ref == NULL) && (getParamState(VX_INPUT, in_index) == VX_PARAMETER_STATE_REQUIRED)) {
        VXLOGE("%s, data ref is empty, index:%d", getName(), in_index);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return data_ref;
}

ExynosVisionDataReference*
ExynosVisionNode::getOutputDataRefByIndex(vx_uint32 out_index)
{
    EXYNOS_VISION_SYSTEM_IN();
    Mutex::Autolock lock(m_internal_lock);

    if (out_index > m_output_data_ref_vector.size()) {
        VXLOGE("index is out of bound, index:%d", out_index);
        return NULL;
    }

    ExynosVisionDataReference *data_ref= m_output_data_ref_vector[out_index];

    if ((data_ref == NULL) && (getParamState(VX_OUTPUT, out_index) == VX_PARAMETER_STATE_REQUIRED)) {
        VXLOGE("%s, data ref is empty, index:%d", getName(), out_index);
    }

    EXYNOS_VISION_SYSTEM_OUT();

    return data_ref;
}

vx_status
ExynosVisionNode::initilalizeKernel(void)
{
    vx_status status;
    const ExynosVisionDataReference *params[VX_INT_MAX_PARAMS];

    for (vx_uint32 i=0; i<getDataRefNum(); i++)
        params[i] = getDataRefByIndex(i);

    status = m_kernel->initialize(this, params, getDataRefNum());
    if (status != VX_SUCCESS) {
        VXLOGE("input validation fail, err:%d", status);
    }

    /* once the kernel has been initialized, create any local data for it */
    if ((m_attributes.localDataSize > 0) &&
        (m_attributes.localDataPtr == NULL)) {
        m_attributes.localDataPtr = calloc(1, m_attributes.localDataSize);
        VXLOGD3("Local Data Allocated %d bytes for node into %p",
                m_attributes.localDataSize,
                m_attributes.localDataPtr);
    }

    return status;
}

void
ExynosVisionNode::calculateConnectedNode(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    Mutex::Autolock lock(m_internal_lock);

    m_pre_node_num = 0;
    m_post_node_num = 0;

    vx_uint32 i;
    for (i=0; i < m_input_data_ref_vector.size(); i++) {
        if (m_input_data_ref_vector[i])
            m_pre_node_num += m_input_data_ref_vector[i]->getIndirectInputNodeNum(m_parent_graph);
    }
    for (i=0; i < m_output_data_ref_vector.size(); i++) {
        if (m_output_data_ref_vector[i])
            m_post_node_num += m_output_data_ref_vector[i]->getIndirectOutputNodeNum(m_parent_graph);
    }

    EXYNOS_VISION_SYSTEM_OUT();
}

vx_status
ExynosVisionNode::verifyNode(void)
{
    EXYNOS_VISION_SYSTEM_IN();
    vx_status status = VX_SUCCESS;
    vx_uint32 p;

    m_cur_frame_cnt = 0;

    /* check to make sure that a node has all required parameters */
    for (p=0; p < m_kernel->getNumParams(); p++) {
        if (m_kernel->getParamState(p) == VX_PARAMETER_STATE_REQUIRED) {
            ExynosVisionDataReference *data_ref = getDataRefByIndex(p);
            if (data_ref == NULL) {
                getContext()->addLogEntry(this, VX_ERROR_NOT_SUFFICIENT, "Node %s: Some parameters were not supplied!\n", getKernelName());
                VXLOGE("Node(%d,%s) Parameter[%u] was required and not supplied", getId(), getKernelName(), p);
                status = VX_ERROR_NOT_SUFFICIENT;
                break;
            } else if (data_ref->getInternalCnt() == 0) {
                VXLOGE("Internal reference counts are wrong at %s", data_ref->getName());
                status = VX_FAILURE;
                break;
            }
        }
    }
    if (status != VX_SUCCESS)
        goto exit;

    /* Validate input */
    for (p = 0; p < m_kernel->getNumParams(); p++) {
        ExynosVisionDataReference *data_ref = getDataRefByIndex(p);
        if (((m_kernel->getParamDirection(p) == VX_BIDIRECTIONAL) ||
             (m_kernel->getParamDirection(p) == VX_INPUT)) && (data_ref != NULL)) {
            vx_status input_validation_status = m_kernel->validateInput(this, p);
            if (input_validation_status != VX_SUCCESS) {
                status = input_validation_status;
                getContext()->addLogEntry(this, status, "Node[%u] %s: parameter[%u] failed input/bi validation!\n", getId(), getKernelName(), p);
                VXLOGE("Failed on input validation of parameter %u of kernel %s in node #%d (status=%d)",
                         p, getKernelName(), getId(), status);
                break;
            }
        }
    }
    if (status != VX_SUCCESS)
        goto exit;

    /* Validate output */
    ExynosVisionMeta *meta;

    for (p = 0; p < m_kernel->getNumParams(); p++) {
        ExynosVisionDataReference *data_ref = getDataRefByIndex(p);
        if (data_ref == NULL)
            continue;

        if (m_kernel->getParamDirection(p) == VX_OUTPUT) {
            if ((data_ref->getScopeType() ==  VX_TYPE_GRAPH) && (data_ref->getScope() != (ExynosVisionReference*)m_parent_graph)) {
                /* major fault! */
                status = VX_ERROR_INVALID_SCOPE;
                getContext()->addLogEntry(this, status, "Virtual Reference is in the wrong scope, created from another graph!\n");
                VXLOGE("Virtual Reference is in the wrong scope, created from another graph, err:%d", status);
                break;
            }

            meta = new ExynosVisionMeta(getContext());

            /* the type of the parameter is known by the system, so let the system set it by default. */
            meta->setMetaType(m_kernel->getParamType(p));

            vx_status output_validation_status = m_kernel->validateOutput(this, p, meta);
            if (output_validation_status == VX_SUCCESS) {
                if (vxIsValidType(meta->getMetaType()) == vx_false_e) {
                    status = VX_ERROR_INVALID_TYPE;
                    getContext()->addLogEntry(this, status,
                        "Node: %s: parameter[%u] is not a valid type %d!\n",
                        getKernelName(), p, meta->getMetaType());
                    VXLOGE("Node: %s: parameter[%u] is not a valid type %d", getKernelName(), p, meta->getMetaType());
                    break;
                }

                vx_status verify_meta_status = data_ref->verifyMeta(meta);
                if (verify_meta_status != VX_SUCCESS) {
                    VXLOGE("verifying meta fail, index:%d of node:%s(%s), ref:%s, err:%d", p, getName(), getKernelName(), data_ref->getName(), verify_meta_status);
                    status = verify_meta_status;
                    break;
                }

            } else {
                status = output_validation_status;
                VXLOGE("Failed on output validation of parameter %u of kernel %s in node #%d (status=%d)",
                         p, getKernelName(), getId(), status);
                break;
            }

            if (meta) {
                meta->destroy();
                delete meta;
            }
        }
    }

    if (status != VX_SUCCESS)
        goto exit;

exit:
    EXYNOS_VISION_SYSTEM_OUT();

    return status;
}

vx_status
ExynosVisionNode::setChildGraphOfNode(ExynosVisionGraph *graph)
{
    vx_status status = VX_ERROR_INVALID_GRAPH;

    if (m_child_graph) {
        ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&m_child_graph, VX_REF_INTERNAL, this);
        m_child_graph = NULL;
    }

    if (graph == NULL) {
        /* it just release previous child graph */
        return VX_SUCCESS;
    }

    vx_uint32 param_num = getKernelHandle()->getNumParams();

    VXLOGD2("node param num:%d, graph param num:%d", param_num, graph->getNumParams());
    /* check to make sure the signature of the node matches the signature of the graph. */
    if (graph->getNumParams() > 0) {
        vx_bool match = vx_true_e;
        for (vx_uint32 p = 0; p < param_num; p++) {
            const struct graph_param_info *graph_param = graph->getParameterHandle(p);
            vx_uint32 child_index = graph_param->index;
            if (graph_param->node) {
                const ExynosVisionKernel *graph_kernel = graph_param->node->getKernelHandle();
                VXLOGD2("node param[%d]: kernel:%s, dir:0x%X, state:0x%X, type:0x%X", p, getKernelHandle()->getKernelName(),
                                                                                                                                    getKernelHandle()->getParamDirection(p),
                                                                                                                                    getKernelHandle()->getParamState(p),
                                                                                                                                    getKernelHandle()->getParamType(p));
                VXLOGD2("graph param[%d]: kernel:%s, dir:0x%X, state:0x%X, type:0x%X", p, graph_kernel->getKernelName(),
                                                                                                                                    graph_kernel->getParamDirection(p),
                                                                                                                                    graph_kernel->getParamState(p),
                                                                                                                                    graph_kernel->getParamType(p));
                if ((getKernelHandle()->getParamDirection(p) != graph_kernel->getParamDirection(child_index)) ||
                    (getKernelHandle()->getParamState(p) != graph_kernel->getParamState(child_index)) ||
                    (getKernelHandle()->getParamType(p) != graph_kernel->getParamType(child_index))) {
                    graph_param->node->displayInfo(0, vx_true_e);
                    VXLOGE("param_%d is not matching, child_index:%d", p, child_index);
                    VXLOGE("direction: 0x%x, 0x%x", getKernelHandle()->getParamDirection(p), graph_kernel->getParamDirection(child_index));
                    VXLOGE("state: 0x%x, 0x%x", getKernelHandle()->getParamState(p), graph_kernel->getParamState(child_index));
                    VXLOGE("type: 0x%x, 0x%x", getKernelHandle()->getParamType(p), graph_kernel->getParamType(child_index));
                    match = vx_false_e;
                }
            } else  {
                VXLOGE("the param of graph is invalid");
                match = vx_false_e;
            }
        }

        if (match == vx_true_e) {
            m_child_graph = graph;
            graph->incrementReference(VX_REF_INTERNAL, this);

            graph->setParentNode(this);
            VXLOGD3("graph %s set as child graph of %s", graph->getName(), this->getName());
            status = VX_SUCCESS;
        }
    } else {
        VXLOGE("graph must have some parameters");
    }

    return status;
}

void
ExynosVisionNode::informKernelStart(vx_uint32 frame_cnt)
{
    m_cur_frame_cnt = frame_cnt;

    time_pair_t *time_pair = m_parent_graph->getPerfMonitor()->requestTimePairStr(this, frame_cnt);
    if (time_pair == NULL) {
        VXLOGE("can't get stamp str, %s, frame_%d", getName(), frame_cnt);
        return ;
    }

    m_time_pair = time_pair;
    TIMESTAMP_START(m_time_pair, TIMEPAIR_FRAMEWORK);
}

void
ExynosVisionNode::informKernelEnd(vx_uint32 frame_cnt, vx_status status)
{
    m_node_status = status;

    if (m_time_pair == NULL) {
        VXLOGE("stamp was not achived, %s, frame_%d", getName(), frame_cnt);
        return ;
    }

    TIMESTAMP_END(m_time_pair, TIMEPAIR_FRAMEWORK);

    m_parent_graph->getPerfMonitor()->releaseTimePairStr(this, frame_cnt, m_time_pair);
    m_time_pair = NULL;
}

void
ExynosVisionNode::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    ExynosVisionSubgraph *subgraph = getSubgraph();
    VXLOGI("%s[Node   ] %s, kernel name:%s(ref in:%d, out:%d), refCnt:%d/%d", MAKE_TAB(tap, tab_num), getName(), m_kernel->getKernelName(),
                                                                                                                            m_input_data_ref_vector.size(), m_output_data_ref_vector.size(),
                                                                                                                            getInternalCnt(), getExternalCnt());

    List<ExynosVisionReference*>::iterator ref_iter;
    for (ref_iter=m_internal_object_list.begin(); ref_iter!=m_internal_object_list.end(); ref_iter++)
        VXLOGI("%s          referensing object:%s", MAKE_TAB(tap, tab_num), (*ref_iter)->getName());

    if (m_child_graph)
        VXLOGI("%s          m_child_graph:%s", MAKE_TAB(tap, tab_num), m_child_graph->getName());

    LOG_FLUSH_TIME();

    if (detail_info) {
        vx_uint32 i;
        ExynosVisionDataReference *ref;
        for (i=0; i<getDataRefNum(); i++) {
            ref = getDataRefByIndex(i);
            if (ref != NULL) {
                ref->displayInfo(tab_num+1, vx_false_e);
            } else {
                VXLOGI("%sref(index:%d, state:0x%X) is not assigned", MAKE_TAB(tap, tab_num+1), i, m_kernel->getParamState(i));
            }
        }

        if (m_child_graph)
            m_child_graph->displayInfo(tab_num+1, vx_false_e);
    }
}

}; /* namespace android */

