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

#ifndef EXYNOS_VPU_TASK_DESC_OBJECT_H
#define EXYNOS_VPU_TASK_DESC_OBJECT_H

#include <utils/Vector.h>
#include <utils/List.h>

#include "vpul-ds.h"

#include "ExynosVpuDrvNode.h"

namespace android {

/* get pointer to the start of vertices list */
#define fst_vtx_ptr(_task) \
	(struct vpul_vertex *)((__u8 *)(_task) + (_task)->vertices_vec_ofs)

/* get pointer to vertex by it's index */
#define vtx_ptr(_task, _idx) \
	(struct vpul_vertex *)(fst_vtx_ptr(_task) + (_idx))

/*! \brief get pointer to the start of 3DNN Process Bases list */
#define fst_3dnn_process_base_ptr(_task) \
	(struct vpul_3dnn_process_base *)((__u8 *)(_task) + (_task)->process_bases_3dnn_vec_ofs)

#define tdnn_process_base_ptr(_task, _td_idx) \
	(struct vpul_3dnn_process_base *)(fst_3dnn_process_base_ptr(_task) + (_td_idx))

/* get pointer to the start of sub-chains list */
#define fst_sc_ptr(_task) \
	(struct vpul_subchain *)((__u8 *)(_task) + (_task)->sc_vec_ofs)

/* get pointer to subchain by it's index in all sub-chains vector */
#define sc_ptr(_task, _sc_idx) \
	(struct vpul_subchain *)(fst_sc_ptr(_task) + (_sc_idx))

/* get pointer to the sub-chains list of some vertex by it's pointer */
#define fst_vtx_sc_ptr(_task, _vtxptr) \
	(struct vpul_subchain *)(((__u8 *)(_task)) + ((_vtxptr)->sc_ofs))

/* get pointer to the start of pus list */
#define fst_pu_ptr(_task) \
	(struct vpul_pu *)((__u8 *)(_task) + (_task)->pus_vec_ofs)

/* get pointer to pu by it's index in all pus vector */
#define pu_ptr(_task, _pu_idx) \
	(struct vpul_pu *)(fst_pu_ptr(_task) + (_pu_idx))

/* get pointer to the pus list of some subchain by it's pointer */
#define fst_sc_pu_ptr(_task, _scptr) \
	(struct vpul_pu *)(((__u8 *)(_task)) + ((_scptr)->pus_ofs))

/*! \brief get pointer to the start of updatable PUs locations list */
#define fst_updateble_pu_location_ptr(_task) \
	(struct vpul_pu_location *)((__u8 *)(_task) \
	+ (_task)->invoke_params_vec_ofs)

/*! \brief get pointer to the updatable PU location by it's index  */
#define updateble_pu_location_ptr(_task, _idx) \
	((struct vpul_pu_location *)(fst_updateble_pu_location_ptr(_task)  + (_idx)))

#define SUBCHAIN_ID(task_id, subchain_index)    (task_id << 8 | subchain_index << 0)

#define PU_TARGET_ID(subchain_id, pu_instance)    (subchain_id << 16 | pu_instance << 0)
#define MEMMAP_TARGET_ID(task_id, memmap_index)    (task_id << 24 | memmap_index << 0)

class ExynosVpuTaskIf;

class ExynosVpuVertex;
class ExynosVpu3dnnProcessBase;
class ExynosVpuMemmap;
class ExynosVpuIoMemory;
class ExynosVpuIoExternalMem;
class ExynosVpuIoInternalRam;
class ExynosVpuSubchain;
class ExynosVpuPu;
class ExynosVpuUpdatablePu;

class ExynosVpuTask  {
private:
    ExynosVpuTaskIf *m_task_if;

    struct vpul_task m_task_info;

    Vector<ExynosVpuVertex*> m_vertex_vector;
    Vector<ExynosVpu3dnnProcessBase*> m_tdnn_proc_base_vector;
    Vector<ExynosVpuSubchain*> m_subchain_vector;
    Vector<ExynosVpuPu*> m_pu_vector;
    Vector<ExynosVpuUpdatablePu*> m_updatable_pu_vector;

    Vector<ExynosVpuMemmap*> m_io_memmap_list;
    Vector <ExynosVpuIoExternalMem*> m_io_external_mem_list;
    Vector<ExynosVpuIoInternalRam*> m_io_internal_ram_list;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_task *task);

    /* Constructor */
    ExynosVpuTask(ExynosVpuTaskIf *task_if);
    ExynosVpuTask(ExynosVpuTaskIf *task_if, const struct vpul_task *task_info);

    /* Destructor */
    virtual ~ExynosVpuTask();

    ExynosVpuTaskIf* getTaskIf(void);
    void setId(uint32_t task_id);
    uint32_t getId(void);
    void setTotalSize(uint32_t total_size);

    uint32_t addUpdatablePu(ExynosVpuUpdatablePu *updatable_up);
    uint32_t addPu(ExynosVpuPu *pu);
    uint32_t addSubchain(ExynosVpuSubchain *subchain);
    uint32_t addTdnnProcBase(ExynosVpu3dnnProcessBase *tdnn_proc_base);
    uint32_t addVertex(ExynosVpuVertex *vertex);
    uint32_t getVertexNumber(void);
    uint32_t getTotalTdnnProcBaseNumber(void);
    uint32_t getTotalSubchainNumber(void);
    uint32_t getTotalPuNumber(void);
    uint32_t getTotalUpdatablePuNumber(void);
    ExynosVpuVertex* getVertex(uint32_t index);
    ExynosVpu3dnnProcessBase* getTdnnProcBase(uint32_t index);
    ExynosVpuSubchain* getSubchain(uint32_t index);
    ExynosVpuPu* getPu(uint32_t phy_index);
    ExynosVpuPu* getPu(enum vpul_pu_instance pu_instance, uint32_t vertex_index, uint32_t subchain_index);
    ExynosVpuPu* getPu(uint32_t pu_index, uint32_t vertex_index, uint32_t subchain_index);

    uint32_t addMemmap(ExynosVpuMemmap *memmap);
    virtual uint32_t getMemmapNumber(void);
    virtual ExynosVpuMemmap* getMemmap(uint32_t index);
    uint32_t addExternalMem(ExynosVpuIoExternalMem *external_mem);

    virtual uint32_t getExternalMemNumber(void);
    virtual ExynosVpuIoExternalMem* getExternalMem(uint32_t index);

    uint32_t addInternalRam(ExynosVpuIoInternalRam *internal_ram);
    ExynosVpuIoInternalRam* getInternalRam(uint32_t index);

    status_t insertTaskToTaskDescriptor(void *task_descriptor_base);

    struct vpul_task* getTaskInfo(void);

    status_t spreadTaskIoInfo(void);

    status_t checkConstraint(void);
    status_t updateStructure(void);
    status_t updateObject(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuMemmap {
protected:
    ExynosVpuTask *m_task;

    struct vpul_memory_map_desc m_memmap_info;
    uint32_t m_memmap_index;

public:
    static bool compareSize(ExynosVpuMemmap *memmap0, ExynosVpuMemmap *memmap1);
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_memory_map_desc *memmap_desc);

    /* Constructor */
    ExynosVpuMemmap(ExynosVpuTask *task, enum vpul_memory_types type);
    ExynosVpuMemmap(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info);

    /* Destructor */
    virtual ~ExynosVpuMemmap()
    {

    }

    enum vpul_memory_types  getType(void)
    {
        return m_memmap_info.mtype;
    }
    uint32_t getIndex(void);
    struct vpul_memory_map_desc *getMemmapInfo(void);
    virtual ExynosVpuIoMemory* getMemory(void);

    virtual status_t updateStructure(void);
    virtual status_t updateObject(void);
    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuMemmapExternal : public ExynosVpuMemmap {
private:
    ExynosVpuIoMemory *m_memory;

public:
    static void display_structure_info(uint32_t tab_num, uint32_t *external_mem_desc);

    /* Constructor */
    ExynosVpuMemmapExternal(ExynosVpuTask *task, ExynosVpuIoMemory *memory);
    ExynosVpuMemmapExternal(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info);

    ExynosVpuIoMemory* getMemory(void);

    status_t updateStructure(void);
    status_t updateObject(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuMemmapPreloadPu : public ExynosVpuMemmap {
private:
    ExynosVpuPu *m_pu;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_memory_map_desc *memmap_desc);

    /* Constructor */
    ExynosVpuMemmapPreloadPu(ExynosVpuTask *task, ExynosVpuPu *pu);
    ExynosVpuMemmapPreloadPu(ExynosVpuTask *task, const struct vpul_memory_map_desc *memmap_info);

    status_t updateStructure(void);
    status_t updateObject(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoMemory {
protected:
    List<ExynosVpuMemmap*> m_memmap_list;
    List<ExynosVpuIoMemory*> m_alliance_memory_list;

public:
    virtual ~ExynosVpuIoMemory()
    {
    }
    virtual uint32_t getIndex(void) = 0;
    virtual status_t setIoBuffer(void)
    {
        VXLOGE("can't be io object");
        return INVALID_OPERATION;
    }

    void addMemmap(ExynosVpuMemmap *memmap);
    uint32_t getMemmapNumber(void)
    {
        return m_memmap_list.size();
    }
    virtual bool isIoBuffer(void)
    {
        return false;
    }
    virtual bool setBuffer(int32_t fd, uint32_t size)
    {
        if (fd==0) {
            VXLOGE("wrong buffer, fd:%d, size:%d", fd, size);
        }
        return false;
    }

    /* set the alliance memory object. alliance memory shares the file descriptor */
    virtual void addAlliancePostIoMemory(ExynosVpuIoMemory *post_memory)
    {
        if (post_memory != this)
            m_alliance_memory_list.push_back(post_memory);
    }
};

class ExynosVpuIoExternalMem : public ExynosVpuIoMemory {
private:
    ExynosVpuTask *m_task;

    uint32_t m_external_mem_info;
    uint32_t m_external_mem_index;

    bool m_allocated;
    uint32_t m_allocated_size;

    /* this flag is updated when the pu is assigned as io pu. (true : io buffer, false : intermediate buffer) */
    bool m_io_buffer_flag;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, void *base, uint32_t *external_mem_addr);

    /* Constructor */
    ExynosVpuIoExternalMem(ExynosVpuTask *task);
    ExynosVpuIoExternalMem(ExynosVpuTask *task, const uint32_t *external_mem_info);

    /* Destructor */
    virtual ~ExynosVpuIoExternalMem()
    {

    }

    bool isAllocated(void)
    {
        return m_allocated;
    }

    bool isIoBuffer(void)
    {
        return m_io_buffer_flag;
    }

    status_t setIoBuffer(void)
    {
        m_io_buffer_flag = true;

        /* clear trash fd information */
        m_external_mem_info = 0;

        return NO_ERROR;
    }

    uint32_t getIndex(void)
    {
        return m_external_mem_index;
    }

    uint32_t getRequiredBufferSize(void);

    bool setBuffer(int32_t fd, uint32_t size);

    uint32_t* getExternalMemInfo(void)
    {
        return &m_external_mem_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoInternalRam : public ExynosVpuIoMemory {
protected:
    ExynosVpuTask *m_task;

    struct vpul_internal_ram m_ram_info;
    uint32_t m_ram_index_in_process;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_internal_ram *ram);

    /* Constructor */
    ExynosVpuIoInternalRam(ExynosVpuTask *task);
    ExynosVpuIoInternalRam(ExynosVpuTask *task, const struct vpul_internal_ram *ram_info);

    uint32_t getIndex(void)
    {
        return m_ram_index_in_process;
    }
    struct vpul_internal_ram *getInternalRamInfo(void)
    {
        return &m_ram_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};


}; // namespace android
#endif
