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

#ifndef EXYNOS_VPU_PU_H
#define EXYNOS_VPU_PU_H

#include <utility>

#include "ExynosVpu3dnnProcessBase.h"
#include "ExynosVpuProcess.h"
#include "ExynosVpu3dnnProcess.h"
#include "ExynosVpuSubchain.h"

namespace android {

using namespace std;

#define VPU_MAX_PU_NAME     50

class ExynosVpuPuFactory {
public:
    ExynosVpuPu* createPu(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuPu* createPu(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);
};

class ExynosVpuPu {
private:
    ExynosVpuSubchainHw *m_subchain;

    /* for process */
    ExynosVpuIoSize *m_in_io_size;
    ExynosVpuIoSize *m_out_io_size;

    /* for 3dnn process */
    ExynosVpuIo3dnnSize *m_in_io_tdnn_size;
    ExynosVpuIo3dnnSize *m_out_io_tdnn_size;

protected:
    uint8_t m_name[VPU_MAX_PU_NAME];
    struct vpul_pu m_pu_info;
    uint32_t m_pu_index_in_task;
    uint32_t m_pu_index_in_subchain;

    Vector<pair<ExynosVpuPu*, uint32_t>> m_in_port_slots;
    Vector<pair<ExynosVpuPu*, uint32_t>> m_out_port_slots;

public:

private:
    void init(enum vpul_pu_instance pu_instance);

public:
    static void display_pu__prarm(uint32_t tab_num, struct vpul_pu *pu);
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_pu *pu);
    static bool connect(ExynosVpuPu *prev_pu, uint32_t prev_pu_out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port);

    /* Constructor */
    ExynosVpuPu(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuPu(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    /* Destructor */
    virtual ~ExynosVpuPu()
    {

    }

    bool connectPostPu(uint32_t out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port);
    bool connectPrevPu(uint32_t in_port, ExynosVpuPu *prev_pu, uint32_t prev_pu_out_port);

    uint8_t* getName();
    enum vpul_logical_op_types getOpcode(void);
    enum vpul_pu_instance getInstance(void);
    ExynosVpuSubchain* getSubchain(void);
    void* getParameter(void);
    uint32_t getIndexInTask(void);
    uint32_t getIndexInSubchain(void);

    uint32_t getTargetId(void);

    void setBypass(bool bypass_flag);

    void setSize(ExynosVpuIoSize *in_size, ExynosVpuIoSize *out_size);
    void setSize(ExynosVpuIo3dnnSize *in_size, ExynosVpuIo3dnnSize *out_size);

    uint32_t getInSizeIndex(void);
    uint32_t getOutSizeIndex(void);

    ExynosVpuIoSize* getInIoSize(void);
    ExynosVpuIoSize* getOutIoSize(void);

    ExynosVpuIo3dnnSize* getInIoTdnnSize(void);
    ExynosVpuIo3dnnSize* getOutIoTdnnSize(void);

    void setPortNum(uint32_t in_port_num, uint32_t out_port_num);

    virtual status_t setIoTypesDesc(ExynosVpuIoTypesDesc *iotypes_desc);
    virtual ExynosVpuIoTypesDesc* getIoTypesDesc(void);
    virtual status_t setMemmap(ExynosVpuMemmap *memmap);
    virtual ExynosVpuMemmap* getMemmap(void);
    virtual status_t getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list);

    virtual status_t checkContraint();
    virtual status_t checkAndUpdatePuIoInfo(void);;

    /* update structure from obejct */
    virtual status_t updateStructure(void);

    /* update object from structure */
    virtual status_t updateObject(void);
    virtual status_t updateObjectOutputPort(uint32_t prev_pu_out_port, ExynosVpuPu *post_pu, uint32_t post_pu_in_port);
    virtual status_t updateIoInfo(List<struct io_format_t> *io_format_list);

    status_t insertPuToTaskDescriptor(void);
    status_t insertPuToTaskDescriptor(void *task_descriptor_base);
    status_t getOffsetAndValue(void *param_addr, uint32_t *ret_offset, uint32_t *ret_value);

    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuUpdatablePu {
private:
    ExynosVpuTask *m_task;
    struct vpul_pu_location m_updatable_pu_info;

    uint32_t m_updatable_pu_index_in_task;

    ExynosVpuPu *m_target_pu;

public:
    static void display_structure_info(uint32_t tab_num, void *base, struct vpul_pu_location *updatable_pu);

    /* Constructor */
    ExynosVpuUpdatablePu(ExynosVpuTask *task, ExynosVpuPu * target_pu);
    ExynosVpuUpdatablePu(ExynosVpuTask *task, const struct vpul_pu_location *updatable_pu_info);

    /* Destructor */
    virtual ~ExynosVpuUpdatablePu()
    {

    }

    status_t checkContraint();

    /* update structure from obejct */
    status_t updateStructure(void);
    /* update object from structure */
    status_t updateObject(void);

    status_t insertUpdatablePuToTaskDescriptor(void *task_descriptor_base);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuProcPuDma : public ExynosVpuPu {
protected:
    ExynosVpuIoTypesDesc *m_iotype_desc;

public:
    static status_t connectIntermediate(ExynosVpuPu *output_pu, ExynosVpuPu *input_pu);

    /* Constructor */
    ExynosVpuProcPuDma(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuProcPuDma(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    status_t setIoTypesDesc(ExynosVpuIoTypesDesc *iotypes_desc);
    ExynosVpuIoTypesDesc* getIoTypesDesc(void)
    {
        return m_iotype_desc;
    }

    ExynosVpuMemmap* getMemmap(void);
    status_t getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list);

    status_t updateStructure(void);
    status_t updateObject(void);
    status_t updateIoInfo(List<struct io_format_t> *io_format_list);
};

class ExynosVpuProcPuDmaIn : public ExynosVpuProcPuDma {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuProcPuDmaIn(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuProcPuDmaIn(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    status_t checkAndUpdatePuIoInfo(void);

    status_t checkContraint();
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuProcPuDmaOut : public ExynosVpuProcPuDma {
private:
    List<ExynosVpuProcPuDmaIn*> m_inter_post_dma_list;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuProcPuDmaOut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuProcPuDmaOut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    void addIntermediatePostDma(ExynosVpuProcPuDmaIn *dma_in);

    status_t checkAndUpdatePuIoInfo(void);

    status_t checkContraint();
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpu3dnnPuDma : public ExynosVpuPu {
protected:
//    ExynosVpuIoTypesDesc *m_iotype_desc;

public:
    static status_t connectIntermediate(ExynosVpuPu *output_pu, ExynosVpuPu *input_pu);

    /* Constructor */
    ExynosVpu3dnnPuDma(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpu3dnnPuDma(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    status_t getMemmap(List<ExynosVpuMemmap*> *ret_memmap_list);

    status_t updateStructure(void);
    status_t updateObject(void);
    status_t updateIoInfo(List<struct io_format_t> *io_format_list);
};

class ExynosVpu3dnnPuDmaIn : public ExynosVpu3dnnPuDma {
private:

public:

private:

public:
    /* Constructor */
    ExynosVpu3dnnPuDmaIn(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpu3dnnPuDmaIn(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    status_t checkAndUpdatePuIoInfo(void);
};

class ExynosVpu3dnnPuDmaOut : public ExynosVpu3dnnPuDma {
private:

public:

private:

public:
    /* Constructor */
    ExynosVpu3dnnPuDmaOut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpu3dnnPuDmaOut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    status_t checkAndUpdatePuIoInfo(void);
};

class ExynosVpuPuSalb : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuSalb(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuSalb(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuSalb()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuCalb : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuCalb(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuCalb(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuCalb()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuRois : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuRois(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuRois(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuRois()
    {

    }

    status_t updateIoInfo(List<struct io_format_t> *io_format_list);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuCrop : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuCrop(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuCrop(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuCrop()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuMde : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuMde(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuMde(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuMde()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuMapToList : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuMapToList(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuMapToList(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuMapToList()
    {

    }

    status_t checkAndUpdatePuIoInfo(void);
    status_t checkContraint(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuNms : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuNms(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuNms(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuNms()
    {

    }

    status_t checkAndUpdatePuIoInfo(void);
    status_t checkContraint();

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuSlf : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuSlf(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuSlf(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuSlf()
    {

    }

    status_t setStaticCoffValue(int32_t coff[], uint32_t vsize, uint32_t coff_num);
    status_t checkAndUpdatePuIoInfo(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuGlf : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuGlf(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuGlf(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuGlf()
    {

    }

    status_t setStaticCoffValue(int32_t coff[], uint32_t vsize, uint32_t coff_num);
    status_t checkAndUpdatePuIoInfo(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuCcm : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuCcm(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuCcm(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuCcm()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuLut : public ExynosVpuPu {
private:
    ExynosVpuMemmap *m_memmap;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuLut(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance);
    ExynosVpuPuLut(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info);

    /* Destructor */
    virtual ~ExynosVpuPuLut()
    {

    }

    status_t setMemmap(ExynosVpuMemmap *memmap);
    ExynosVpuMemmap* getMemmap(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuIntegral : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuIntegral(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuIntegral(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuIntegral()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuUpscaler : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuUpscaler(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuUpscaler(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuUpscaler()
    {

    }

    status_t checkAndUpdatePuIoInfo(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuDownscaler : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuDownscaler(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuDownscaler(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuDownscaler()
    {

    }

    status_t checkAndUpdatePuIoInfo(void);

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuJoiner : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuJoiner(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuJoiner(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuJoiner()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuSplitter : public ExynosVpuPu {

#define SPLIT_OUT_DISABLE   4

private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuSplitter(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuSplitter(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuSplitter()
    {

    }

    status_t checkContraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuDuplicator : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuDuplicator(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuDuplicator(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuDuplicator()
    {

    }

    virtual status_t checkContraint();

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuHistogram : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuHistogram(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuHistogram(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuHistogram()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuNlf : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuNlf(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuNlf(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuNlf()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuDepth : public ExynosVpuPu {
private:

public:

private:

public:
    /* Constructor */
    ExynosVpuPuDepth(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuDepth(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuDepth()
    {

    }
};

class ExynosVpuPuDisparity : public ExynosVpuPu {
private:

public:

private:

public:
    /* Constructor */
    ExynosVpuPuDisparity(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuDisparity(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuDisparity()
    {

    }
};

class ExynosVpuPuInPaint : public ExynosVpuPu {
private:

public:

private:

public:
    /* Constructor */
    ExynosVpuPuInPaint(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuInPaint(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuInPaint()
    {

    }
};

class ExynosVpuPuCnn : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuCnn(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuCnn(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuCnn()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuPuFifo : public ExynosVpuPu {
private:

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_pu *pu);

    /* Constructor */
    ExynosVpuPuFifo(ExynosVpuSubchainHw *subchain, enum vpul_pu_instance pu_instance)
                                            : ExynosVpuPu(subchain, pu_instance)
    {

    }
    ExynosVpuPuFifo(ExynosVpuSubchainHw *subchain, const struct vpul_pu *pu_info)
                                            : ExynosVpuPu(subchain, pu_info)
    {

    }

    /* Destructor */
    virtual ~ExynosVpuPuFifo()
    {

    }

    void displayObjectInfo(uint32_t tab_num = 0);
};

}; // namespace android
#endif
