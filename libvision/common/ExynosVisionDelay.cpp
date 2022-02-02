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

#define LOG_TAG "ExynosVisionDelay"
#include <cutils/log.h>

#include <VX/vx_helper.h>

#include "ExynosVisionDelay.h"

#include "ExynosVisionContext.h"
#include "ExynosVisionNode.h"

#include "ExynosVisionImage.h"
#include "ExynosVisionArray.h"
#include "ExynosVisionMatrix.h"
#include "ExynosVisionConvolution.h"
#include "ExynosVisionDistribution.h"
#include "ExynosVisionRemap.h"
#include "ExynosVisionLut.h"
#include "ExynosVisionPyramid.h"
#include "ExynosVisionThreshold.h"
#include "ExynosVisionScalar.h"

namespace android {

vx_status
ExynosVisionDelay::checkValidCreateDelay(ExynosVisionContext *context, ExynosVisionDataReference *reference)
{
    vx_enum invalid_types[] = {
        VX_TYPE_CONTEXT,
        VX_TYPE_GRAPH,
        VX_TYPE_NODE,
        VX_TYPE_KERNEL,
        VX_TYPE_TARGET,
        VX_TYPE_PARAMETER,
        VX_TYPE_REFERENCE,
        VX_TYPE_DELAY, /* no delays of delays */
    };

    for (vx_uint32 i = 0u; i < dimof(invalid_types); i++) {
        if (reference->getType() == invalid_types[i]) {
            VXLOGE("Attempted to create delay of invalid object type!", reference->getType());
            context->addLogEntry(context, VX_ERROR_INVALID_REFERENCE, "Attempted to create delay of invalid object type!\n");
            return VX_ERROR_INVALID_REFERENCE;
        }
    }

    return VX_SUCCESS;
}

ExynosVisionDelay::ExynosVisionDelay(ExynosVisionContext *context)
                                                       : ExynosVisionDataReference(context, VX_TYPE_DELAY, (ExynosVisionReference*)context, vx_true_e, vx_false_e)
{
    m_count = 0;
    m_base_index = 0;
    m_delay_type = VX_TYPE_INVALID;
}

ExynosVisionDelay::~ExynosVisionDelay()
{

}

vx_status
ExynosVisionDelay::init(ExynosVisionReference *exemplar, vx_size count)
{
    vx_status status = VX_SUCCESS;

    ExynosVisionContext *context = getContext();

    m_delay_type = exemplar->getType();
    m_count = count;

    m_set_vector.clear();
    m_set_vector.setCapacity(m_count);
    List<struct vx_delay_node_info> delay_node_list;
    m_set_vector.insertAt(delay_node_list, 0, m_count);

    m_ref_vector.clear();
    m_ref_vector.setCapacity(m_count);
    m_ref_vector.insertAt(NULL, 0, m_count);

    for (vx_uint32 phy_index = 0; phy_index < count; phy_index++)  {
        ExynosVisionDataReference *data_ref;

        switch (m_delay_type) {
        case VX_TYPE_IMAGE:
            {
                ExynosVisionImage *exemplar_image = (ExynosVisionImage*)exemplar;
                ExynosVisionImage *image = new ExynosVisionImage(context, this, vx_false_e);
                *image = *exemplar_image;

                data_ref = image;
            }
            break;
        case VX_TYPE_ARRAY:
            {
                ExynosVisionArray *exemplar_array = (ExynosVisionArray*)exemplar;
                ExynosVisionArray *array = new ExynosVisionArray(context, this, vx_false_e);
                *array = *exemplar_array;

                data_ref = array;
            }
            break;
        case VX_TYPE_MATRIX:
            {
                ExynosVisionMatrix *exemplar_matrix = (ExynosVisionMatrix*)exemplar;
                ExynosVisionMatrix *matrix = new ExynosVisionMatrix(context, this);
                *matrix = *exemplar_matrix;

                data_ref = matrix;
            }
            break;
        case VX_TYPE_CONVOLUTION:
            {
                ExynosVisionConvolution *exemplar_convolution = (ExynosVisionConvolution*)exemplar;
                ExynosVisionConvolution *convolution = new ExynosVisionConvolution(context, this);
                *convolution = *exemplar_convolution;

                vx_uint32 scale;
                status |= exemplar_convolution->queryConvolution(VX_CONVOLUTION_ATTRIBUTE_SCALE, &scale, sizeof(scale));
                status |= convolution->setConvolutionAttribute(VX_CONVOLUTION_ATTRIBUTE_SCALE, &scale, sizeof(scale));
                if (status != VX_SUCCESS)
                    VXLOGE("copying scale attribute fails, err:%d", status);

                data_ref = convolution;
            }
            break;
        case VX_TYPE_DISTRIBUTION:
            {
                ExynosVisionDistribution *exemplar_distribution = (ExynosVisionDistribution*)exemplar;
                ExynosVisionDistribution *distribution = new ExynosVisionDistribution(context, this);
                *distribution = *exemplar_distribution;

                data_ref = distribution;
            }
            break;
        case VX_TYPE_REMAP:
            {
                ExynosVisionRemap *exemplar_remap = (ExynosVisionRemap*)exemplar;
                ExynosVisionRemap *remap = new ExynosVisionRemap(context, this);
                *remap = *exemplar_remap;

                data_ref = remap;
            }
            break;
        case VX_TYPE_LUT:
            {
                ExynosVisionLut *exemplar_lut = (ExynosVisionLut*)exemplar;
                ExynosVisionLut *lut = new ExynosVisionLut(context, this);
                *lut = *exemplar_lut;

                data_ref = lut;
            }
            break;
        case VX_TYPE_PYRAMID:
            {
                ExynosVisionPyramid *exemplar_pyramid = (ExynosVisionPyramid*)exemplar;
                ExynosVisionPyramid *pyramid = new ExynosVisionPyramid(context, this, vx_false_e);
                *pyramid = *exemplar_pyramid;

                data_ref = pyramid;
            }
            break;
        case VX_TYPE_THRESHOLD:
            {
                ExynosVisionThreshold *exemplar_threshold = (ExynosVisionThreshold*)exemplar;
                ExynosVisionThreshold *threshold = new ExynosVisionThreshold(context, this);
                *threshold = *exemplar_threshold;

                data_ref = threshold;
            }
            break;
        case VX_TYPE_SCALAR:
            {
                ExynosVisionScalar *exemplar_scalar = (ExynosVisionScalar*)exemplar;
                ExynosVisionScalar *scalar = new ExynosVisionScalar(context, this);
                *scalar = *exemplar_scalar;

                data_ref = scalar;
            }
            break;
        default:
            VXLOGE("unsupoorted delay type: 0x%X", m_delay_type);
            status = VX_FAILURE;
            break;
        }

        m_ref_vector.replaceAt(data_ref, phy_index);
        data_ref->incrementReference(VX_REF_INTERNAL, this);

        /* delay object doesn't connect to node, so we should allocate delay element manually */
        data_ref->allocateMemory();

        /* set the object as a delay element */
        m_ref_vector[phy_index]->initReferenceForDelay(this, phy_index);
    }

    return status;
}

vx_status
ExynosVisionDelay::destroy(void)
{
    vx_status status = VX_SUCCESS;

    Vector<List<struct vx_delay_node_info>>::iterator node_list_vector_iter;
    for(node_list_vector_iter=m_set_vector.begin(); node_list_vector_iter!=m_set_vector.end(); node_list_vector_iter++) {
        List <struct vx_delay_node_info> *delay_node_list = &(*node_list_vector_iter);

        delay_node_list->clear();
    }

    Vector<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_ref_vector.begin(); ref_iter!=m_ref_vector.end(); ref_iter++) {
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&(*ref_iter), VX_REF_INTERNAL, this);
        if (status != VX_SUCCESS)
            VXLOGE("release %s fails at %s", (*ref_iter)->getName(), this->getName());
    }

    /* m_set_list */
    return status;
}

vx_status
ExynosVisionDelay::queryDelay(vx_enum attribute, void *ptr, vx_size size)
{
    vx_status status = VX_SUCCESS;

    switch (attribute) {
    case VX_DELAY_ATTRIBUTE_TYPE:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_enum *)ptr = m_delay_type;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    case VX_DELAY_ATTRIBUTE_SLOTS:
        if (VX_CHECK_PARAM(ptr, size, vx_size, 0x3))
            *(vx_size *)ptr = (vx_size)m_count;
        else
            status = VX_ERROR_INVALID_PARAMETERS;
        break;
    default:
        status = VX_ERROR_NOT_SUPPORTED;
        break;
    }

    return status;
}

ExynosVisionDataReference*
ExynosVisionDelay::getReferenceFromDelay(vx_int32 index)
{
    ExynosVisionDataReference *ref = NULL;

    if ((vx_uint32)abs(index) < m_count) {
        vx_int32 i = (m_base_index + abs(index)) % (vx_int32)m_count;
        VXLOGD2("base_index:%d, log index:%d, phy_index:%d", m_base_index, index, i);
        ref = m_ref_vector[i];
    } else {
        VXLOGE("invalid index:%d at %s, count:%d", index, getName(), m_count);
    }

    return ref;
}

vx_status
ExynosVisionDelay::ageDelay(void)
{
    EXYNOS_VISION_REF_IN();

    vx_status status = VX_SUCCESS;

    m_base_index = (m_base_index + 1) % (vx_uint32)m_count;

    VXLOGD2("delay has shifted by 1, base index is now %d, count: %d", m_base_index, m_count);

    Vector<List<struct vx_delay_node_info>> backup_set_vector;
    List<struct vx_delay_node_info>::iterator list_iter;
    List<struct vx_delay_node_info> *delay_node_list;
    struct vx_delay_node_info *delay_node;

    backup_set_vector = m_set_vector;

    vx_uint32 cur_phy_index, target_phy_index;

    Vector<List<struct vx_delay_node_info>>::iterator vector_iter;
    for (vector_iter=backup_set_vector.begin(), cur_phy_index=0; vector_iter!=backup_set_vector.end(); vector_iter++, cur_phy_index++) {
        delay_node_list = &(*vector_iter);
        target_phy_index = (cur_phy_index+1) % (vx_int32)m_count;
        for (list_iter=delay_node_list->begin(); list_iter!=delay_node_list->end(); list_iter++) {
            delay_node = &(*list_iter);

            VXLOGD2("%s, move from index[%d] to index[%d]", delay_node->node->getName(), cur_phy_index, target_phy_index);

            status = delay_node->node->nodeSetParameter(delay_node->index, m_ref_vector[target_phy_index]);
            if (status != VX_SUCCESS)
                VXLOGE("setting param fails, err:%d", status);
        }
    }

    EXYNOS_VISION_REF_OUT();

    return status;
}

vx_bool
ExynosVisionDelay::addAssociationToDelay(ExynosVisionDataReference *ref, ExynosVisionNode *node, vx_uint32 node_index)
{
    /* logical index means distance from base index */
    vx_int32 phy_index = ref->getDelaySlotIndex();

    struct vx_delay_node_info delay_node;
    delay_node.node = node;
    delay_node.index = node_index;

    List<struct vx_delay_node_info> *delay_node_list;
    delay_node_list = &m_set_vector.editItemAt(phy_index);
    delay_node_list->push_back(delay_node);

    return vx_true_e;
}

vx_bool
ExynosVisionDelay::removeAssociationToDelay(ExynosVisionDataReference *ref, ExynosVisionNode *node, vx_uint32 node_index)
{
    vx_bool ret = vx_false_e;
    vx_int32 phy_index = ref->getDelaySlotIndex();
    vx_status status;

    if (phy_index >= (vx_int32)m_count) {
        VXLOGE("index(%d) is out of bound(%d)", phy_index, m_count);
        return vx_false_e;
    }

    List<struct vx_delay_node_info> *delay_node_list;
    delay_node_list = &m_set_vector.editItemAt(phy_index);

    List<struct vx_delay_node_info>::iterator node_iter;
    for (node_iter=delay_node_list->begin(); node_iter!=delay_node_list->end(); node_iter++) {
        struct vx_delay_node_info *delay_node = &(*node_iter);
        if (delay_node->node == node && delay_node->index == node_index) {
            VXLOGD2("index[%d], remove %s, index:%d", phy_index, (*node_iter).node->getName(), (*node_iter).index);
            delay_node_list->erase(node_iter);
            ret = vx_true_e;
            break;
        }
    }

    if (ret != vx_true_e) {
        VXLOGE("cannot find matching delay parameter");
    }

    return ret;
}

vx_status
ExynosVisionDelay::allocateMemory(vx_enum res_type, struct resource_param *param)
{
    if (res_type == RESOURCE_CLASS_NONE) {
        VXLOGE("unknown resource type:%d, %p", res_type, param);
    }

    return VX_SUCCESS;
}

vx_status
ExynosVisionDelay::allocateResource(empty_resource_t **ret_resource)
{
    VXLOGE("%s couldn't use alloc resource, ret_resource:%p", getName(), ret_resource);
    return VX_FAILURE;
}

vx_status
ExynosVisionDelay::freeResource(empty_resource_t *resource)
{
    VXLOGE("%s couldn't use free resource, resource:%p", getName(), resource);
    return VX_FAILURE;
}

ExynosVisionDataReference*
ExynosVisionDelay::getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid)
{
    VXLOGE("%s couldn't be shared reference, frame:%d, &data_valid:%p", getName(), frame_cnt, ret_data_valid);
    return NULL;
}

vx_status
ExynosVisionDelay::putInputShareRef(vx_uint32 frame_cnt)
{
    VXLOGE("%s couldn't be shared reference, frame:%d", getName(), frame_cnt);
    return VX_FAILURE;
}

ExynosVisionDataReference*
ExynosVisionDelay::getOutputShareRef(vx_uint32 frame_cnt)
{
    VXLOGE("%s couldn't be shared reference, frame:%d", getName(), frame_cnt);
    return NULL;
}

vx_status
ExynosVisionDelay::putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
{
    VXLOGE("%s couldn't be shared reference, frame:%d, demand:%d, data_valid:%d", getName(), frame_cnt, demand_num, data_valid);
    return VX_FAILURE;
}

void
ExynosVisionDelay::displayInfo(vx_uint32 tab_num, vx_bool detail_info)
{
    vx_char tap[MAX_TAB_NUM];

    VXLOGI("%s[Delay  ][%d] %s(%p), cnt:%d, base_index:%d, type:0x%x, refCnt:%d/%d", MAKE_TAB(tap, tab_num), detail_info, getName(), this,
                                                m_count, m_base_index, m_delay_type,
                                                getInternalCnt(), getExternalCnt());

    vx_uint32 i;

#if 0
    VXLOGE("%s          [Delay child-object list]", MAKE_TAB(tap, tab_num));
    Vector<ExynosVisionDataReference*>::iterator ref_iter;
    for (ref_iter=m_ref_vector.begin(), i=0; ref_iter!=m_ref_vector.end(); ref_iter++, i++) {
        VXLOGE("%s          Delay[%d]:%s", MAKE_TAB(tap, tab_num), i, (*ref_iter)->getName());
    }
#endif

    VXLOGI("%s          [Delay connection list]", MAKE_TAB(tap, tab_num));
    Vector<List<struct vx_delay_node_info>>::iterator index_iter;
    List<struct vx_delay_node_info> *node_list;
    List<struct vx_delay_node_info>::iterator node_iter;
    for (index_iter=m_set_vector.begin(), i=0; index_iter!=m_set_vector.end(); index_iter++, i++) {
        node_list = &m_set_vector.editItemAt(i);
        VXLOGI("%s          phy_index[%d] num:%d", MAKE_TAB(tap, tab_num), i, node_list->size());
        for (node_iter=node_list->begin(); node_iter!=node_list->end(); node_iter++) {
            VXLOGI("%s              %s(%p), node_index:%d", MAKE_TAB(tap, tab_num), (*node_iter).node->getName(), (*node_iter).node, (*node_iter).index);
        }
    }
}

}; /* namespace android */
