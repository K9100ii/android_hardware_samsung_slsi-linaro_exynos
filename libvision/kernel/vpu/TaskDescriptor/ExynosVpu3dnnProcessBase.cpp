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

#define LOG_TAG "ExynosVpu3dnnProcessBase"
#include <cutils/log.h>

#include "ExynosVpu3dnnProcessBase.h"

namespace android {

void
ExynosVpuIo3dnnSize::display_structure_info(uint32_t tab_num, struct vpul_3dnn_size *tdnn_size)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s(tdnn_size) type:%d, op_idx:%d, inout_type:%d, src_idx:%d", MAKE_TAB(tap, tab_num),
                                                                    tdnn_size->type, tdnn_size->op_ind, tdnn_size->inout_3dnn_type, tdnn_size->src_idx);
}

void
ExynosVpuIo3dnnSize::displayObjectInfo(uint32_t tab_num)
{
    ExynosVpuIo3dnnSize::display_structure_info(tab_num, &m_tdnn_size_info);
}

ExynosVpuIo3dnnSize::ExynosVpuIo3dnnSize(ExynosVpu3dnnProcessBase *tdnn_proc_base, enum vpul_3d_sizes_op_type type, ExynosVpuIo3dnnSize *src_size)
{
    m_tdnn_proc_base = tdnn_proc_base;
    memset(&m_tdnn_size_info, 0x0, sizeof(m_tdnn_size_info));
    m_tdnn_size_info.type = type;

    m_size_index_in_3dnn_proc_base = tdnn_proc_base->addTdnnSize(this);
    m_src_size = src_size;
}

ExynosVpuIo3dnnSize::ExynosVpuIo3dnnSize(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *size_info)
{
    m_tdnn_proc_base = tdnn_proc_base;
    m_tdnn_size_info = *size_info;

    m_size_index_in_3dnn_proc_base = tdnn_proc_base->addTdnnSize(this);
    m_src_size = NULL;
}

ExynosVpuIo3dnnSizeInout::ExynosVpuIo3dnnSizeInout(ExynosVpu3dnnProcessBase *tdnn_proc_base, enum vpul_inout_3dnn_type inout_tdnn_type, ExynosVpuIo3dnnSize *src_size)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, VPUL_3DXY_SIZEOP_INOUT, src_size)
{
    m_tdnn_size_info.inout_3dnn_type = inout_tdnn_type;
}

ExynosVpuIo3dnnSizeInout::ExynosVpuIo3dnnSizeInout(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, tdnn_size_info)
{

}

ExynosVpuIo3dnnSizeZinToXy::ExynosVpuIo3dnnSizeZinToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, VPUL_3DXY_SIZEOP_ZIN_TO_XY, src_size)
{

}

ExynosVpuIo3dnnSizeZinToXy::ExynosVpuIo3dnnSizeZinToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, tdnn_size_info)
{

}

ExynosVpuIo3dnnSizeZoutToXy::ExynosVpuIo3dnnSizeZoutToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, VPUL_3DXY_SIZEOP_ZOUT_TO_XY, src_size)
{

}

ExynosVpuIo3dnnSizeZoutToXy::ExynosVpuIo3dnnSizeZoutToXy(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, tdnn_size_info)
{

}

ExynosVpuIo3dnnSizeCrop::ExynosVpuIo3dnnSizeCrop(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, VPUL_3DXY_SIZEOP_CROP, src_size)
{

}

ExynosVpuIo3dnnSizeCrop::ExynosVpuIo3dnnSizeCrop(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, tdnn_size_info)
{

}

ExynosVpuIo3dnnSizeScale::ExynosVpuIo3dnnSizeScale(ExynosVpu3dnnProcessBase *tdnn_proc_base, ExynosVpuIo3dnnSize *src_size)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, VPUL_3DXY_SIZEOP_SCALE, src_size)
{

}

ExynosVpuIo3dnnSizeScale::ExynosVpuIo3dnnSizeScale(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_size *tdnn_size_info)
                                                                            :ExynosVpuIo3dnnSize(tdnn_proc_base, tdnn_size_info)
{

}

ExynosVpu3dnnLayer::ExynosVpu3dnnLayer(ExynosVpu3dnnProcessBase *tdnn_proc_base)
{
    m_tdnn_proc_base = tdnn_proc_base;
    memset(&m_tdnn_layer_info, 0x0, sizeof(m_tdnn_layer_info));
    m_index_in_tdnr_proc_base = tdnn_proc_base->addTdnnLayer(this);
}

ExynosVpu3dnnLayer::ExynosVpu3dnnLayer(ExynosVpu3dnnProcessBase *tdnn_proc_base, const struct vpul_3dnn_layer *layer_info)
{
    m_tdnn_proc_base = tdnn_proc_base;
    m_tdnn_layer_info = *layer_info;
    m_index_in_tdnr_proc_base = tdnn_proc_base->addTdnnLayer(this);
}

struct vpul_3dnn_layer*
ExynosVpu3dnnLayer::getLayerInfo(void)
{
    return &m_tdnn_layer_info;
}

ExynosVpu3dnnProcessBase::ExynosVpu3dnnProcessBase(ExynosVpuTask *task)
{
    m_task = task;
    memset(&m_tdnn_proc_base_info, 0x0, sizeof(m_tdnn_proc_base_info));
    m_tdnn_proc_base_index_in_task = task->addTdnnProcBase(this);
}

ExynosVpu3dnnProcessBase::ExynosVpu3dnnProcessBase(ExynosVpuTask *task, const struct vpul_3dnn_process_base *tdnn_proc_base_info)
{
    m_task = task;
    m_tdnn_proc_base_info = *tdnn_proc_base_info;
    m_tdnn_proc_base_index_in_task = task->addTdnnProcBase(this);

    ExynosVpuIo3dnnSize *io_tdnn_size_object;
    const struct vpul_3dnn_size *tdnn_size;
    for (uint32_t i=0; i<m_tdnn_proc_base_info.io.n_sizes_op; i++) {
        tdnn_size = &m_tdnn_proc_base_info.io.sizes_3dnn[i];
        switch (tdnn_size->type) {
        case VPUL_3DXY_SIZEOP_INOUT:
            io_tdnn_size_object = new ExynosVpuIo3dnnSizeInout(this, tdnn_size);
            break;
        case VPUL_3DXY_SIZEOP_ZIN_TO_XY:
            io_tdnn_size_object = new ExynosVpuIo3dnnSizeZinToXy(this, tdnn_size);
            break;
        case VPUL_3DXY_SIZEOP_ZOUT_TO_XY:
            io_tdnn_size_object = new ExynosVpuIo3dnnSizeZoutToXy(this, tdnn_size);
            break;
        case VPUL_3DXY_SIZEOP_CROP:
            io_tdnn_size_object = new ExynosVpuIo3dnnSizeCrop(this, tdnn_size);
            break;
        case VPUL_3DXY_SIZEOP_SCALE:
            io_tdnn_size_object = new ExynosVpuIo3dnnSizeScale(this, tdnn_size);
            break;
        default:
            VXLOGE("un-supported type:0x%x", tdnn_size->type);
            break;
        }
    }

    ExynosVpu3dnnLayer *layer_object;
    const struct vpul_3dnn_layer *layer_info;
    for (uint32_t i=0; i<m_tdnn_proc_base_info.number_of_layers; i++) {
        layer_info = &m_tdnn_proc_base_info.layers[i];
        layer_object = new ExynosVpu3dnnLayer(this, layer_info);
    }
}

ExynosVpu3dnnProcessBase::~ExynosVpu3dnnProcessBase()
{
    uint32_t i;

    for (i=0; i<m_io_tdnn_size_list.size(); i++)
        delete m_io_tdnn_size_list[i];

    for (i=0; i<m_tdnn_layer_list.size(); i++)
        delete m_tdnn_layer_list[i];
}

uint32_t
ExynosVpu3dnnProcessBase::getIndex(void)
{
    return m_tdnn_proc_base_index_in_task;
}

uint32_t
ExynosVpu3dnnProcessBase::addTdnnSize(ExynosVpuIo3dnnSize *size)
{
    m_io_tdnn_size_list.push_back(size);

    return m_io_tdnn_size_list.size() - 1;
}

uint32_t
ExynosVpu3dnnProcessBase::addTdnnLayer(ExynosVpu3dnnLayer *tdnn_layer)
{
    m_tdnn_layer_list.push_back(tdnn_layer);

    return m_tdnn_layer_list.size() - 1;
}

ExynosVpuIo3dnnSize*
ExynosVpu3dnnProcessBase::getIoTdnnSize(uint32_t index)
{
    if (m_io_tdnn_size_list.size() <= index) {
        VXLOGE("index:%d is out of bound, %d", index, m_io_tdnn_size_list.size());
        return NULL;
    }

    return m_io_tdnn_size_list[index];
}

status_t
ExynosVpu3dnnProcessBase::getLayerInoutMemmapList(uint32_t layer_inout_index, List<ExynosVpuMemmap*> *ret_memmap_list)
{
    ret_memmap_list->clear();

    if (layer_inout_index >= VPUL_MAX_3DNN_INOUT) {
        VXLOGE("layer_inout_index is out of bound, %d, %d", layer_inout_index, VPUL_MAX_3DNN_INOUT);
        return INVALID_OPERATION;
    }

    ExynosVpuMemmap *memmap;

    uint32_t layer_num = m_tdnn_layer_list.size();
    uint32_t inout_num = m_tdnn_proc_base_info.io.n_3dnn_inouts;

#if 0
    Vector<ExynosVpu3dnnLayer*>::iterator layer_iter;
    for (layer_iter=m_tdnn_layer_list.begin(); layer_iter!=m_tdnn_layer_list.end(); layer_iter++) {
        struct vpul_3dnn_layer *layer_info = (*layer_iter)->getLayerInfo();
#else
    uint32_t cur_layer;
     for (cur_layer=0; cur_layer<layer_num; cur_layer++) {
        struct vpul_3dnn_layer *layer_info = m_tdnn_layer_list[cur_layer]->getLayerInfo();
#endif
        uint32_t memmap_index = layer_info->inout_3dnn[layer_inout_index].mem_descr_index;

        // JUN_DBG
//        VXLOGE("test : memmap index : %d", memmap_index);

        uint8_t check_intermediate = false;

        if (layer_info->inout_3dnn[layer_inout_index].inout_3dnn_type == VPUL_IO_3DNN_INPUT) {
            for (uint32_t i=0; i<layer_num; i++) {
                struct vpul_3dnn_layer *other_layer_info = m_tdnn_layer_list[i]->getLayerInfo();
                for (uint32_t j=0; j<inout_num; j++) {
//                    VXLOGE("[layer_%d][inout_%d] type:%d, mem_descr_idnex:%d", i, j, other_layer_info->inout_3dnn[j].inout_3dnn_type, other_layer_info->inout_3dnn[j].mem_descr_index);
                    if (((other_layer_info->inout_3dnn[j].inout_3dnn_type == VPUL_IO_3DNN_OUTPUT) ||
                        (other_layer_info->inout_3dnn[j].inout_3dnn_type == VPUL_IO_LASTSLGR_OUTPUT)) &&
                        (other_layer_info->inout_3dnn[j].mem_descr_index == memmap_index) &&
                        (i != cur_layer)) {
                        /* this buffer is intermediate buffer */
                        check_intermediate = true;
                    }
                }
            }
        } else if ((layer_info->inout_3dnn[layer_inout_index].inout_3dnn_type == VPUL_IO_3DNN_OUTPUT) ||
            (layer_info->inout_3dnn[layer_inout_index].inout_3dnn_type == VPUL_IO_LASTSLGR_OUTPUT)) {
            for (uint32_t i=0; i<layer_num; i++) {
                struct vpul_3dnn_layer *other_layer_info = m_tdnn_layer_list[i]->getLayerInfo();
                for (uint32_t j=0; j<inout_num; j++) {
//                    VXLOGE("[layer_%d][inout_%d] type:%d, mem_descr_idnex:%d", i, j, other_layer_info->inout_3dnn[j].inout_3dnn_type, other_layer_info->inout_3dnn[j].mem_descr_index);
                    if ((other_layer_info->inout_3dnn[j].inout_3dnn_type == VPUL_IO_3DNN_INPUT) &&
                        (other_layer_info->inout_3dnn[j].mem_descr_index == memmap_index) &&
                        (i != cur_layer)) {
                        /* this buffer is intermediate buffer */
                        check_intermediate = true;
                    }
                }
            }
        }

        if (check_intermediate == true)
            continue;

        // JUN_DBG
//        VXLOGE("pick : memmap index : %d", memmap_index);

        memmap = m_task->getMemmap(memmap_index);
        if (memmap == NULL) {
            VXLOGE("getting memmap fails, index:%d", memmap_index);
            return INVALID_OPERATION;
        }
        ret_memmap_list->push_back(memmap);
    }

    return NO_ERROR;
}

void
ExynosVpu3dnnProcessBase::display_structure_info(uint32_t tab_num, void *base, struct vpul_3dnn_process_base *tdnn_proc_base)
{
    char tap[MAX_TAB_NUM];

    VXLOGI("%s[TDNN] number_of_layers:%d", MAKE_TAB(tap, tab_num),  tdnn_proc_base->number_of_layers);
}

status_t
ExynosVpu3dnnProcessBase::insertTdnnProcBaseToTaskDescriptor(void *task_descriptor_base)
{
    status_t ret = NO_ERROR;

    struct vpul_3dnn_process_base *td_tdnn_proc_base;
    td_tdnn_proc_base = tdnn_process_base_ptr((struct vpul_task*)task_descriptor_base, m_tdnn_proc_base_index_in_task);

    memcpy(td_tdnn_proc_base, &m_tdnn_proc_base_info, sizeof(struct vpul_3dnn_process_base));

EXIT:
    return ret;
}

}; /* namespace android */
