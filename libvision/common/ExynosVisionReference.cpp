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

#define LOG_TAG "ExynosVisionReference"
#include <cutils/log.h>

#include "ExynosVisionReference.h"
#include "ExynosVisionContext.h"
#include "ExynosVisionGraph.h"
#include "ExynosVisionSubgraph.h"
#include "ExynosVisionNode.h"
#include "ExynosVisionImage.h"
#include "ExynosVisionArray.h"
#include "ExynosVisionDistribution.h"
#include "ExynosVisionThreshold.h"
#include "ExynosVisionLut.h"
#include "ExynosVisionConvolution.h"
#include "ExynosVisionDelay.h"
#include "ExynosVisionRemap.h"
#include "ExynosVisionPyramid.h"
#include "ExynosVisionMatrix.h"

namespace android {

Mutex ExynosVisionReference::m_global_lock;
vx_uint32 ExynosVisionReference::m_ref_id_cnt = 0;

typedef struct _vx_type_size {
    vx_enum type;
    vx_size size;
} vx_type_size_t;

static vx_type_size_t type_sizes[] = {
    {VX_TYPE_INVALID,   0},
    // scalars
    {VX_TYPE_CHAR,      sizeof(vx_char)},
    {VX_TYPE_INT8,      sizeof(vx_int8)},
    {VX_TYPE_INT16,     sizeof(vx_int16)},
    {VX_TYPE_INT32,     sizeof(vx_int32)},
    {VX_TYPE_INT64,     sizeof(vx_int64)},
    {VX_TYPE_UINT8,     sizeof(vx_uint8)},
    {VX_TYPE_UINT16,    sizeof(vx_uint16)},
    {VX_TYPE_UINT32,    sizeof(vx_uint32)},
    {VX_TYPE_UINT64,    sizeof(vx_uint64)},
    {VX_TYPE_FLOAT32,   sizeof(vx_float32)},
    {VX_TYPE_FLOAT64,   sizeof(vx_float64)},
    {VX_TYPE_ENUM,      sizeof(vx_enum)},
    {VX_TYPE_BOOL,      sizeof(vx_bool)},
    {VX_TYPE_SIZE,      sizeof(vx_size)},
    {VX_TYPE_DF_IMAGE,    sizeof(vx_df_image)},
    // structures
    {VX_TYPE_RECTANGLE,     sizeof(vx_rectangle_t)},
    {VX_TYPE_COORDINATES2D, sizeof(vx_coordinates2d_t)},
    {VX_TYPE_COORDINATES3D, sizeof(vx_coordinates3d_t)},
    {VX_TYPE_KEYPOINT,      sizeof(vx_keypoint_t)},
    // pseudo objects
    {VX_TYPE_ERROR,     sizeof(ExynosVisionError)},
    {VX_TYPE_META_FORMAT,sizeof(ExynosVisionMeta)},
    // framework objects
    {VX_TYPE_REFERENCE, sizeof(ExynosVisionReference)},
    {VX_TYPE_CONTEXT,   sizeof(ExynosVisionContext)},
    {VX_TYPE_GRAPH,     sizeof(ExynosVisionGraph)},
    {VX_TYPE_NODE,      sizeof(ExynosVisionNode)},
    {VX_TYPE_TARGET,    sizeof(ExynosVisionTarget)},
    {VX_TYPE_PARAMETER, sizeof(ExynosVisionParameter)},
    {VX_TYPE_KERNEL,    sizeof(ExynosVisionKernel)},
    // data objects
    {VX_TYPE_ARRAY,     sizeof(ExynosVisionArray)},
    {VX_TYPE_CONVOLUTION, sizeof(ExynosVisionConvolution)},
    {VX_TYPE_DELAY,     sizeof(ExynosVisionDelay)},
    {VX_TYPE_DISTRIBUTION, sizeof(ExynosVisionDistribution)},
    {VX_TYPE_IMAGE,     sizeof(ExynosVisionImage)},
    {VX_TYPE_LUT,       sizeof(ExynosVisionLut)},
    {VX_TYPE_MATRIX,    sizeof(ExynosVisionMatrix)},
    {VX_TYPE_PYRAMID,   sizeof(ExynosVisionPyramid)},
    {VX_TYPE_REMAP,     sizeof(ExynosVisionRemap)},
    {VX_TYPE_SCALAR,    sizeof(ExynosVisionScalar)},
    {VX_TYPE_THRESHOLD, sizeof(ExynosVisionThreshold)},
    // others
};

static vx_bool getTypeName(vx_enum type, vx_char *name)
{
    vx_bool ret = vx_true_e;

    switch(type) {
    case VX_TYPE_ERROR:
        strcpy(name, "ERROR");
        break;
    case VX_TYPE_META_FORMAT:
        strcpy(name, "META");
        break;
    case VX_TYPE_REFERENCE:
        strcpy(name, "REFERENCE");
        break;
    case VX_TYPE_CONTEXT:
        strcpy(name, "CONTEXT");
        break;
    case VX_TYPE_GRAPH:
        strcpy(name, "GRAPH");
        break;
    case VX_TYPE_NODE:
        strcpy(name, "NODE");
        break;
    case VX_TYPE_PARAMETER:
        strcpy(name, "PARAMETER");
        break;
    case VX_TYPE_TARGET:
        strcpy(name, "TARGET");
        break;
    case VX_TYPE_KERNEL:
        strcpy(name, "KERNEL");
        break;
    case VX_TYPE_ARRAY:
        strcpy(name, "ARRAY");
        break;
    case VX_TYPE_CONVOLUTION:
        strcpy(name, "CONVOLUTION");
        break;
    case VX_TYPE_DELAY:
        strcpy(name, "DELAY");
        break;
    case VX_TYPE_DISTRIBUTION:
        strcpy(name, "DISTRIBUTION");
        break;
    case VX_TYPE_IMAGE:
        strcpy(name, "IMAGE");
        break;
    case VX_TYPE_LUT:
        strcpy(name, "LUT");
        break;
    case VX_TYPE_MATRIX:
        strcpy(name, "MATRIX");
        break;
    case VX_TYPE_PYRAMID:
        strcpy(name, "PYRAMID");
        break;
    case VX_TYPE_REMAP:
        strcpy(name, "REMAP");
        break;
    case VX_TYPE_SCALAR:
        strcpy(name, "SCALAR");
        break;
    case VX_TYPE_THRESHOLD:
        strcpy(name, "THRESHOLD");
        break;
    default:
        VXLOGE("no type, 0x%X", type);
        strcpy(name, "UNKNOWN");
        ret = vx_false_e;
        break;
    }

    return ret;
}

vx_bool
ExynosVisionReference::isValidReference(ExynosVisionReference *ref)
{
    vx_bool ret = vx_false_e;

    if (ref != NULL) {
        if ((ref->m_magic == VX_MAGIC) &&
            ((vxIsValidType(ref->getType())) && ref->getType() != VX_TYPE_CONTEXT) &&
            (ExynosVisionContext::isValidContext(ref->getContext()) == vx_true_e)) {
            ret = vx_true_e;
        }
    } else {
        VXLOGE("reference was NULL");
    }

    return ret;
}

vx_bool
ExynosVisionReference::isValidReference(ExynosVisionReference *ref, vx_enum type)
{
    vx_bool ret = vx_false_e;
    if (ref != NULL) {
        if ((ref->m_magic == VX_MAGIC) &&
            ((ref->getType() == type) && (ref->getType() != VX_TYPE_CONTEXT)) &&
            (ExynosVisionContext::isValidContext(ref->getContext()) == vx_true_e)) {
            ret = vx_true_e;
        } else {
            VXLOGE("ref(%p) is not a valid reference, expected type:%p", ref, type);
            ref->displayInfo(0, vx_true_e);
        }
    } else {
        VXLOGE("reference was NULL");
    }
    return ret;
}

vx_status
ExynosVisionReference::releaseReferenceInt(ExynosVisionReference **ref, vx_enum reftype, ExynosVisionReference *releasing_ref)
{
    vx_status status = VX_SUCCESS;

    if ((*ref)->decrementReference(reftype, releasing_ref) == 0) {
        status = (*ref)->getContext()->removeReference(*ref);
        if (status != VX_SUCCESS) {
            VXLOGE("removing %s in context fails, err:%d", (*ref)->getName(), status);
        }

        status  = (*ref)->destroy();
        if (status != VX_SUCCESS) {
            VXLOGE("destroying %s fails, err:%d", (*ref)->getName(), status);
        }
        delete *ref;
        *ref = NULL;
    }

    return status;
}

vx_size
ExynosVisionReference::sizeOfType(vx_enum type)
{
    vx_uint32 i = 0;
    vx_size size = 0ul;
    for (i = 0; i < dimof(type_sizes); i++) {
        if (type == type_sizes[i].type) {
            size = type_sizes[i].size;
            break;
        }
    }
    return size;
}

ExynosVisionReference::ExynosVisionReference(ExynosVisionContext* context, vx_type_e type, ExynosVisionReference* scope, vx_bool is_add_context)
{
    EXYNOS_VISION_REF_IN();
    vx_char type_name[VX_MAX_REFERENCE_NAME];

    m_global_lock.lock();
    m_id = ++m_ref_id_cnt;
    if (getTypeName(type, type_name) == vx_false_e)
        VXLOGE("unknown type(0x%X)", type);

    sprintf(m_name, "%s_%d", type_name, m_id);
    m_global_lock.unlock();

    m_context = context;
    m_scope = scope;
    m_magic = VX_MAGIC;
    m_type = type;
    m_internal_count = 0;
    m_external_count = 0;

    m_creation_status = VX_SUCCESS;

    if (is_add_context) {
        if (context->addReference(this) == vx_false_e) {
            VXLOGE("Failed to add reference to global table");
            context->addLogEntry(context, VX_ERROR_NO_RESOURCES, "Failed to add to resources table\n");
            m_creation_status = VX_ERROR_NO_RESOURCES;
        }
    }

    VXLOGD2("Create %p, %s", this, m_name);

    EXYNOS_VISION_REF_OUT();
}

ExynosVisionReference::~ExynosVisionReference(void)
{
    m_magic = VX_BAD_MAGIC;

    VXLOGD2("Delete %p, %s", this, m_name);
}

vx_status
ExynosVisionReference::queryReference(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_REF_ATTRIBUTE_COUNT:
        if (VX_CHECK_PARAM(ptr, size, vx_uint32, 0x3))
            *(vx_uint32 *)ptr = m_external_count;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_REF_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_enum, 0x3))
            *(vx_enum *)ptr = m_type;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }
    return status;
}

vx_status
ExynosVisionReference::hint(vx_enum hint)
{
    vx_status status = VX_SUCCESS;

    switch (hint) {
    /*! \todo add hints to the sample implementation */
    case VX_HINT_SERIALIZE:
        if (m_type == VX_TYPE_GRAPH)
            ((ExynosVisionGraph*)this)->setSerialize();
        else
            status = VX_ERROR_INVALID_TYPE;
        break;
    case VX_HINT_STREAM:
        if (m_type == VX_TYPE_GRAPH)
            ((ExynosVisionGraph*)this)->setExecMode(GRAPH_EXEC_STREAM);
        else
            status = VX_ERROR_INVALID_TYPE;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_status
ExynosVisionReference::directive(vx_enum directive)
{
    vx_status status = VX_SUCCESS;

    switch (directive) {
    /*! \todo add directive to the sample implementation */
    case VX_DIRECTIVE_DISABLE_LOGGING:
    case VX_DIRECTIVE_ENABLE_LOGGING:
        status = getContext()->setDirective(directive);
        break;
    case VX_DIRECTIVE_IMAGE_CONTINUOUS:
        if (getType() == VX_TYPE_IMAGE) {
            status = ((ExynosVisionImage*)this)->setDirective(directive);
            if (status != VX_SUCCESS) {
                VXLOGE("setting directive fails at %s, err:%s", getName(), status);
            }
        } else {
            VXLOGE("continuous directive should apply to image object, not %s", getName());
            status = VX_ERROR_INVALID_REFERENCE;
        }
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

vx_uint32
ExynosVisionReference::incrementReference(vx_uint32 reftype, ExynosVisionReference *ref)
{
    vx_uint32 count = 0u;

    Mutex::Autolock lock(m_internal_lock);

    if (reftype & VX_REF_EXTERNAL) {
        m_external_count++;
    } if (reftype & VX_REF_INTERNAL) {
        m_internal_count++;
        m_internal_object_list.push_back(ref);
    }

    count = m_internal_count + m_external_count;

    return count;
}

vx_uint32
ExynosVisionReference::decrementReference(vx_uint32 reftype, ExynosVisionReference *referensing_obj)
{
    Mutex::Autolock lock(m_internal_lock);
    vx_uint32 result = 0xFFFFFFFF;

    if (reftype & VX_REF_INTERNAL) {
        if (m_internal_count == 0)
            VXLOGE("internal ref count is already zero, %s", getName());
        else
            m_internal_count--;

        List<ExynosVisionReference*>::iterator ref_iter;
        for (ref_iter=m_internal_object_list.begin(); ref_iter!=m_internal_object_list.end(); ref_iter++) {
            if (*ref_iter == referensing_obj)
                break;
        }

        if (ref_iter != m_internal_object_list.end()) {
            m_internal_object_list.erase(ref_iter);
        } else {
            if (referensing_obj != NULL) {
                VXLOGE("releasing object is not corrent, %s releases %s", referensing_obj->getName(), this->getName());
                for (ref_iter=m_internal_object_list.begin(); ref_iter!=m_internal_object_list.end(); ref_iter++) {
                    VXLOGD("referencing object %s", (*ref_iter)->getName());
                }
            } else {
                VXLOGE("releasing object is NULL, NULL releases %s", this->getName());
            }
        }
    }
    if (reftype & VX_REF_EXTERNAL) {
        if (m_external_count == 0) {
            VXLOGE("external ref count is already zero, %s", getName());
        } else {
            m_external_count--;
        }
    }

    result = m_internal_count + m_external_count;

    return result;
}

vx_status
ExynosVisionReference::destroy(void)
{
    VXLOGE("reference type(0x%X) is not implemented yet", getType());
    return VX_FAILURE;
}

void
ExynosVisionReference::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGD("%s[Referen][%d] ref:%s, refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(), getInternalCnt(), getExternalCnt());
}

void
ExynosVisionReference::displayPerf(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGD("%s[Referen][%d] %s doesn't report performance data", MAKE_TAB(tap, tab_num), detail_info, getName());
}

vx_bool
ExynosVisionDataReference::checkWriteDependency(ExynosVisionDataReference *data_ref1, ExynosVisionDataReference *data_ref2)
{
    if (data_ref1 == data_ref2) {
        VXLOGE("write dependency, same object, ref1(%d), ref2(%d)", data_ref1->getId(), data_ref2->getId());
        return vx_true_e;
    }

    /* write to layer then read pyramid */
    if (data_ref1->getType() == VX_TYPE_PYRAMID && data_ref2->getType() == VX_TYPE_IMAGE) {
        if (data_ref1 == data_ref2->getScope()) {
            VXLOGE("%s's scope is same with %s", data_ref2->getName(), data_ref1->getName());
            return vx_true_e;
        }
    }

    /* write to pyramid then read a layer */
    if (data_ref2->getType() == VX_TYPE_PYRAMID && data_ref1->getType() == VX_TYPE_IMAGE) {
        if (data_ref2 == data_ref1->getScope()) {
            VXLOGE("%s's ancestor is same with %s", data_ref1->getName(), data_ref2->getName());
            return vx_true_e;
        }
    }

    /* two images or ROIs */
    if (data_ref1->getType() == VX_TYPE_IMAGE && data_ref2->getType() == VX_TYPE_IMAGE) {
        if (ExynosVisionImage::checkImageDependenct((ExynosVisionImage*)data_ref1, (ExynosVisionImage*)data_ref2)) {
            VXLOGE("ref1(%d) and ref2 has same region of interest", data_ref1->getId(), data_ref2->getId());
            return vx_true_e;
        }
    }

    return vx_false_e;
}

vx_bool
ExynosVisionDataReference::isValidDataReference(ExynosVisionReference *ref)
{
    vx_bool ret = vx_false_e;
    if (ref != NULL) {
        if ((ref->m_magic == VX_MAGIC) &&
            ((vxIsValidType(ref->getType())) && ref->getType() != VX_TYPE_CONTEXT) &&
            (ExynosVisionContext::isValidContext(ref->getContext()) == vx_true_e)) {
            switch(ref->getType()) {
            case VX_TYPE_DELAY:
            case VX_TYPE_LUT:
            case VX_TYPE_DISTRIBUTION:
            case VX_TYPE_PYRAMID:
            case VX_TYPE_THRESHOLD:
            case VX_TYPE_MATRIX:
            case VX_TYPE_CONVOLUTION:
            case VX_TYPE_SCALAR:
            case VX_TYPE_ARRAY:
            case VX_TYPE_IMAGE:
            case VX_TYPE_REMAP:
                ret = vx_true_e;
                break;
            default:
                ret = vx_false_e;
                break;
            }
            ret = vx_true_e;
        } else {
            VXLOGE("ref(%p) is not a valid reference", ref);
        }
    } else {
        VXLOGE("reference was NULL");
    }
    return ret;
}

vx_status
ExynosVisionDataReference::jointAlliance(ExynosVisionDataReference *data_ref1, ExynosVisionDataReference *data_ref2)
{
    vx_status status = VX_SUCCESS;

    status |= data_ref1->registerAlliance(data_ref2);
    status |= data_ref2->registerAlliance(data_ref1);
    if (status != VX_SUCCESS) {
        VXLOGE("register alliance fails");
    }

    return status;
}

ExynosVisionDataReference::ExynosVisionDataReference(ExynosVisionContext* context, vx_type_e type, ExynosVisionReference* scope,
                                                                                                     vx_bool is_add_context, vx_bool is_virtual)
                                                                                                    : ExynosVisionReference(context, type, scope, is_add_context)
{
    m_delay = NULL;
    m_delay_slot_index = 0;

    m_is_virtual = is_virtual;

    m_res_type = RESOURCE_MNGR_NULL;

    m_is_allocated = vx_false_e;

    m_allocator_need_flag = vx_false_e;
    m_allocator = NULL;

    m_kernel_count = 0;
    m_access_count = 0;
}

ExynosVisionDataReference::~ExynosVisionDataReference(void)
{
    if (m_clone_object_map.size()) {
        VXLOGE("clone map of %s is not freed", getName());
    }

    map<void*, ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_clone_object_map.begin(); ref_iter!=m_clone_object_map.end(); ref_iter++) {
        if ((*ref_iter).second)
            delete (*ref_iter).second;
    }
}

vx_status
ExynosVisionDataReference::increaseKernelCount(void)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    m_kernel_count++;

    EXYNOS_VISION_REF_OUT();

    return VX_SUCCESS;
}

vx_status
ExynosVisionDataReference::decreaseKernelCount(void)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    if (m_kernel_count != 0) {
        m_kernel_count--;
    } else {
        VXLOGE("%s, kernel count is already zero", getName());
        displayInfo(0, vx_true_e);
        return VX_FAILURE;
    }

    EXYNOS_VISION_REF_OUT();

    return VX_SUCCESS;
}

vx_uint32
ExynosVisionDataReference::increaseAccessCount(void)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    m_access_count++;

    EXYNOS_VISION_REF_OUT();

    return m_access_count;
}

vx_uint32
ExynosVisionDataReference::decreaseAccessCount(void)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    if (m_access_count != 0) {
        m_access_count--;
    } else {
        VXLOGE("%s, access count is already zero", getName());
        displayInfo(0, vx_true_e);
    }

    EXYNOS_VISION_REF_OUT();

    return m_access_count;
}

vx_bool
ExynosVisionDataReference::connectNode(ExynosVisionGraph *graph, ExynosVisionNode *node, vx_uint32 node_index, enum vx_direction_e data_ref_direction)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    VXLOGD2("%s, add %s of %s", this->getName(), node->getName(), graph->getName());
    node_connect_info_t connect_info;
    connect_info.node = node;
    connect_info.node_index = node_index;

    if (data_ref_direction == VX_INPUT) {
        m_output_node_list[graph].push_back(connect_info);
    } else {
        m_input_node_list[graph].push_back(connect_info);
    }

    EXYNOS_VISION_REF_OUT();

    return vx_true_e;
}

vx_bool
ExynosVisionDataReference::disconnectNode(ExynosVisionGraph *graph, ExynosVisionNode *node, vx_uint32 node_index, enum vx_direction_e data_ref_direction)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    vx_bool result = vx_false_e;

    List<node_connect_info_t> *node_list;
    List<node_connect_info_t>::iterator node_iter;

    VXLOGD2("%s disconnects %s of %s", this->getName(), node->getName(), graph->getName());
    if (data_ref_direction == VX_INPUT)
        node_list = &m_output_node_list[graph];
    else
        node_list = &m_input_node_list[graph];

    if (node_list->size() == 0) {
        result = vx_false_e;
        VXLOGE("%s does not have any node of %s", getName(), graph->getName());

        goto EXIT;
    }

    /* a node could connect multiple way to same data reference */
    for (node_iter = node_list->begin(); node_iter != node_list->end(); node_iter++) {
        if (((*node_iter).node == node) && ((*node_iter).node_index == node_index)){
            VXLOGD2("disconnect to %s", (*node_iter).node->getName());
            node_iter = node_list->erase(node_iter);
            result = vx_true_e;
            break;
        }
    }

    if (result != vx_true_e)
        VXLOGE("%s is not found at %s", node->getName(), this->getName());

    /* remove graph map if there is no node anymore */
    if (node_list->size() == 0) {
        VXLOGD2("remove map of %s", graph->getName());
        if (data_ref_direction == VX_INPUT)
            m_output_node_list.erase(graph);
        else
            m_input_node_list.erase(graph);
    }

EXIT:
    EXYNOS_VISION_REF_OUT();

    return result;
}

vx_status
ExynosVisionDataReference::registerAlliance(ExynosVisionDataReference *ref)
{
    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        if (*ref_iter == ref) {
            VXLOGE("%s is already alliance with %s", ref->getName(), this->getName());
            return VX_FAILURE;
        }
    }

    m_alliance_ref_list.push_back(ref);

    return VX_SUCCESS;
}

vx_uint32
ExynosVisionDataReference::getDirectInputNodeNum(ExynosVisionGraph *graph)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    vx_uint32 count;

    count = m_input_node_list[graph].size();

    EXYNOS_VISION_REF_OUT();

    return count;
}

vx_uint32
ExynosVisionDataReference::getDirectOutputNodeNum(ExynosVisionGraph *graph)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    vx_uint32 count;

    count = m_output_node_list[graph].size();

    EXYNOS_VISION_REF_OUT();

    return count;
}

vx_uint32
ExynosVisionDataReference::getIndirectInputNodeNum(ExynosVisionGraph *graph)
{
    vx_uint32 input_node_num = getDirectInputNodeNum(graph);

    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        input_node_num += (*ref_iter)->getDirectInputNodeNum(graph);
    }

    return input_node_num;
}

vx_uint32
ExynosVisionDataReference::getIndirectOutputNodeNum(ExynosVisionGraph *graph)
{
    vx_uint32 output_node_num = getDirectOutputNodeNum(graph);

    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        output_node_num += (*ref_iter)->getDirectOutputNodeNum(graph);
    }

    return output_node_num;
}

ExynosVisionNode*
ExynosVisionDataReference::getDirectOutputNode(ExynosVisionGraph *graph, vx_uint32 node_idx)
{
    EXYNOS_VISION_REF_IN();
    Mutex::Autolock lock(m_internal_lock);

    List<node_connect_info_t> *node_list = &m_output_node_list[graph];

    if (node_list->size() < (node_idx+1)) {
        VXLOGE("out of bound node index:%d", node_idx);
        return NULL;
    }

    List<node_connect_info_t>::iterator iter_pos = node_list->begin();
    for (vx_uint32 i = 0; i<node_idx; i++, iter_pos++);

    ExynosVisionNode *node = (*iter_pos).node;
    EXYNOS_VISION_REF_OUT();

    return node;
}

ExynosVisionNode*
ExynosVisionDataReference::getIndirectOutputNode(ExynosVisionGraph *graph, vx_uint32 node_idx)
{
    if (node_idx < getDirectOutputNodeNum(graph)) {
        return this->getDirectOutputNode(graph, node_idx);
    }

    vx_uint32 remained_idx = node_idx - getDirectOutputNodeNum(graph);

    List<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
        if (remained_idx < (*ref_iter)->getDirectOutputNodeNum(graph)) {
            return (*ref_iter)->getDirectOutputNode(graph, remained_idx);
        }
        remained_idx -= (*ref_iter)->getDirectOutputNodeNum(graph);
    }

    VXLOGE("can't find proper connection");

    return NULL;
}

vx_status
ExynosVisionDataReference::allocateMemory(void)
{
    vx_enum res_type;
    struct resource_param res_param;
    vx_status status = VX_SUCCESS;

    if (m_is_allocated == vx_true_e) {
        VXLOGE("already allocated memory %s", getName());
        status = VX_FAILURE;
        goto EXIT;
    }

    if (m_is_virtual == vx_true_e) {
        ExynosVisionReference *scope = getScope();
        if (scope->getType() != VX_TYPE_GRAPH) {
            VXLOGE("%s, virtual data object should be belong to graph", getName());
            status = VX_FAILURE;
            goto EXIT;
        }

        if (((ExynosVisionGraph*)scope)->getExecMode() == GRAPH_EXEC_STREAM) {
            res_type = RESOURCE_MNGR_SLOT;
            res_param.param.slot_param.slot_num = DEFAULT_SLOT_NUM;
        } else {
            res_type = RESOURCE_MNGR_SOLID;
        }
    } else {
        res_type = RESOURCE_MNGR_SOLID;
    }

    status = allocateMemory(res_type, &res_param);
    if (status != VX_SUCCESS) {
        VXLOGE("allocating memory fails, err:%d", status);
    }

EXIT:

    return status;
}

vx_status
ExynosVisionDataReference::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    VXLOGE("%s couldn't allocate memory, res_type:%d, param:%p", getName(), res_type, param);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::triggerDoneEventIndirect(ExynosVisionGraph *graph, vx_uint32 frame_cnt)
{
    vx_status status = VX_SUCCESS;

    status = this->triggerDoneEventDirect(graph, frame_cnt);
    if (status != VX_SUCCESS) {
        VXLOGE("%s, trigger done event fails", this->getName());
    } else {
        List<ExynosVisionDataReference*>::iterator ref_iter;
        for (ref_iter=m_alliance_ref_list.begin(); ref_iter!=m_alliance_ref_list.end(); ref_iter++) {
            status = (*ref_iter)->triggerDoneEventDirect(graph, frame_cnt);
            if (status != VX_SUCCESS) {
                VXLOGE("%s, trigger done event fails", (*ref_iter)->getName());
                break;
            }
        }
    }

    return status;
}

vx_status
ExynosVisionDataReference::triggerDoneEventDirect(ExynosVisionGraph *graph, vx_uint32 frame_cnt)
{
    vx_status status = VX_SUCCESS;

    List<node_connect_info_t> *output_node_list = &m_output_node_list[graph];
    List<node_connect_info_t>::iterator node_iter;

    for (node_iter=output_node_list->begin(); node_iter!=output_node_list->end(); node_iter++) {
        ExynosVisionSubgraph *subgraph = (*node_iter).node->getSubgraph();
        if (subgraph == NULL) {
            VXLOGE("cannot find subgraph from %s", (*node_iter).node->getName());
            status = VX_FAILURE;
            break;
        } else {
            subgraph->pushDoneEvent(frame_cnt, this, (*node_iter).node_index);
        }
    }

    return status;
}

ExynosVisionDataReference*
ExynosVisionDataReference::getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    if (m_res_type == RESOURCE_MNGR_SOLID) {
        /* solid and exclusive input is always valid because it comes from application */
        *ret_data_valid = vx_true_e;
        return this;
    } else {
        VXLOGE("exclusive reference of %s can't be resource type:%d, frame_%d, &data_valid:%p", getName(), m_res_type, frame_cnt, ret_data_valid);
        *ret_data_valid = vx_false_e;
        return NULL;
    }
}

vx_status
ExynosVisionDataReference::putInputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type == RESOURCE_MNGR_SOLID) {
        return VX_SUCCESS;
    } else {
        VXLOGE("exclusive reference of %s can't be resource type:%d, frame_%d", getName(), m_res_type, frame_cnt);
        return VX_FAILURE;
    }
}

ExynosVisionDataReference*
ExynosVisionDataReference::getOutputExclusiveRef(vx_uint32 frame_cnt)
{
    if (m_res_type == RESOURCE_MNGR_SOLID) {
        return this;
    } else {
        VXLOGE("exclusive reference of %s can't be resource type:%d, frame_%d", getName(), m_res_type, frame_cnt);
        return NULL;
    }
}

vx_status
ExynosVisionDataReference::putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid)
{
    if (m_res_type == RESOURCE_MNGR_SOLID) {
        return VX_SUCCESS;
    } else {
        VXLOGE("exclusive reference cf %s an't be resource type:%d, frame_%d, data_valid:%d", getName(), m_res_type, frame_cnt, data_valid);
        return VX_FAILURE;
    }
}

ExynosVisionDataReference*
ExynosVisionDataReference::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    VXLOGE("%s couldn't be shared reference, frame:%d, &data_valid:%p", getName(), frame_cnt, ret_data_valid);
    return NULL;
}

vx_status
ExynosVisionDataReference::putInputShareRef(vx_uint32 frame_cnt)
{
    VXLOGE("%s couldn't be shared reference, frame:%d", getName(), frame_cnt);
    return VX_FAILURE;
}

ExynosVisionDataReference*
ExynosVisionDataReference::getOutputShareRef(vx_uint32 frame_cnt)
{
    VXLOGE("%s couldn't be shared reference, frame:%d", getName(), frame_cnt);
    return NULL;
}

vx_status
ExynosVisionDataReference::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    VXLOGE("%s couldn't be shared reference, frame:%d, demand:%d, data_valid:%d", getName(), frame_cnt, demand_num, data_valid);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(default_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(image_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(pyramid_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(lut_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(empty_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::allocateResource(convolution_resource_t **ret_resource)
{
    VXLOGE("%s doesn't support this function, empty res:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(default_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(image_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(pyramid_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(lut_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(empty_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::freeResource(convolution_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(default_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(image_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(pyramid_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(lut_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(empty_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::createCloneObject(convolution_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(default_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(image_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(pyramid_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(lut_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(empty_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDataReference::destroyCloneObject(convolution_resource_t* resource)
{
    VXLOGE("%s doesn't support this function, res:%p", getName(), resource);
    return VX_FAILURE;
}

void
ExynosVisionDataReference::displayConn(vx_uint32 tab_num)
{
    vx_char tap[MAX_TAB_NUM];

    map<ExynosVisionGraph*, List<node_connect_info_t>>::iterator map_iter;
    for (map_iter=m_input_node_list.begin(); map_iter!=m_input_node_list.end(); map_iter++) {
        List<node_connect_info_t> *node_list = &(map_iter->second);
        List<node_connect_info_t>::iterator node_iter;
        for (node_iter=node_list->begin(); node_iter!=node_list->end(); node_iter++)
            VXLOGI("%s          input: %s at %s", MAKE_TAB(tap, tab_num), (*node_iter).node->getName(), map_iter->first->getName());
    }
    for (map_iter=m_output_node_list.begin(); map_iter!=m_output_node_list.end(); map_iter++) {
        List<node_connect_info_t> *node_list = &(map_iter->second);
        List<node_connect_info_t>::iterator node_iter;
        for (node_iter=node_list->begin(); node_iter!=node_list->end(); node_iter++)
            VXLOGI("%s          output: %s at %s", MAKE_TAB(tap, tab_num), (*node_iter).node->getName(), map_iter->first->getName());
    }
}

void
ExynosVisionDataReference::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[RefData][%d] ref:%s(%p), refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(), this, getInternalCnt(), getExternalCnt());

    LOG_FLUSH_TIME();

    if (detail_info)
        displayConn(tab_num);
}

}; /* namespace android */
