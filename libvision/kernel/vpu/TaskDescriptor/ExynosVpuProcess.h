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

#ifndef EXYNOS_VPU_PROCESS_H
#define EXYNOS_VPU_PROCESS_H

#include <utils/Vector.h>
#include <utils/List.h>

#include "ExynosVpuVertex.h"

namespace android {

enum {
    MAP_ROI_TYPE_FIXED,
    MAP_ROI_TYPE_DYNAMIC
};

class ExynosVpuIoMapRoi {

private:
    uint32_t m_type;

protected:
    ExynosVpuProcess *m_process;

    uint32_t m_maproi_index_in_process;

    ExynosVpuMemmap *m_memmap;

public:
    ExynosVpuIoMapRoi(uint32_t type)
    {
        m_type = type;
    }

    uint32_t getIndex(void)
    {
        return m_maproi_index_in_process;
    }
    ExynosVpuMemmap* getMemmap(void)
    {
        return m_memmap;
    }
    void setMemmap(ExynosVpuMemmap *memmap)
    {
        m_memmap = memmap;
    }
    uint32_t getType(void)
    {
        return m_type;
    }
};

class ExynosVpuIoDynamicMapRoi : public ExynosVpuIoMapRoi {
private:
    struct vpul_dynamic_map_roi m_dynamic_map_roi_info;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_dynamic_map_roi *dyn_roi);

    /* Constructor */
    ExynosVpuIoDynamicMapRoi(ExynosVpuProcess *process);
    ExynosVpuIoDynamicMapRoi(ExynosVpuProcess *process, const struct vpul_dynamic_map_roi *dynamic_map_info);

    struct vpul_dynamic_map_roi *getDynMapInfo(void)
    {
        return &m_dynamic_map_roi_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoFixedMapRoi : public ExynosVpuIoMapRoi {
private:
    struct vpul_fixed_map_roi m_fixed_map_roi_info;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_fixed_map_roi *fixed_roi);

    /* Constructor */
    ExynosVpuIoFixedMapRoi(ExynosVpuProcess *process, ExynosVpuMemmap *memmap);
    ExynosVpuIoFixedMapRoi(ExynosVpuProcess *process, const struct vpul_fixed_map_roi *fixed_map_info);

    struct vpul_fixed_map_roi *getFixedMapInfo(void)
    {
        return &m_fixed_map_roi_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoTypesDesc {
private:
    ExynosVpuProcess *m_process;

    struct vpul_iotypes_desc m_iotypes_info;
    uint32_t m_iotypes_index_in_process;

    ExynosVpuIoMapRoi *m_map_roi;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_iotypes_desc *iotype);

    /* Constructor */
    ExynosVpuIoTypesDesc(ExynosVpuProcess *process, ExynosVpuIoDynamicMapRoi *dyn_roi);
    ExynosVpuIoTypesDesc(ExynosVpuProcess *process, ExynosVpuIoFixedMapRoi *fixed_roi);
    ExynosVpuIoTypesDesc(ExynosVpuProcess *process, const struct vpul_iotypes_desc *iotypes_info);

    /* Destructor */
    virtual ~ExynosVpuIoTypesDesc()
    {

    }

    ExynosVpuProcess* getProcess(void)
    {
        return m_process;
    }
    uint32_t getIndex(void)
    {
        return m_iotypes_index_in_process;
    }
    struct vpul_iotypes_desc *getIoTypesInfo(void)
    {
        return &m_iotypes_info;
    }
    void setMapRoi(ExynosVpuIoMapRoi *map_roi)
    {
        m_map_roi = map_roi;
    }
    ExynosVpuIoMapRoi* getMapRoi(void)
    {
        return m_map_roi;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoSize {
private:

protected:
    ExynosVpuProcess *m_process;

    struct vpul_sizes m_size_info;
    uint32_t m_size_index_in_process;

    ExynosVpuIoSize *m_src_size;

    uint32_t m_origin_width;
    uint32_t m_origin_height;
    uint32_t m_origin_roi_width;
    uint32_t m_origin_roi_height;

public:

private:

protected:
    /* Constructor */
    ExynosVpuIoSize(ExynosVpuProcess *process, enum vpul_sizes_op_type type, ExynosVpuIoSize *src_size);
    ExynosVpuIoSize(ExynosVpuProcess *process, const struct vpul_sizes *size_info);

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_sizes *size);

    /* Destructor */
    virtual ~ExynosVpuIoSize()
    {

    }

    uint32_t getIndex(void)
    {
        return m_size_index_in_process;
    }
    enum vpul_sizes_op_type getType(void)
    {
        return m_size_info.type;
    }
    struct vpul_sizes* getSizeInfo(void)
    {
        return &m_size_info;
    }

    bool isAssignDimension(void);
    virtual status_t setOriginDimension(uint32_t width, uint32_t height, uint32_t roi_width, uint32_t roi_height);
    virtual status_t getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height) = 0;

    status_t checkConstraint(void);

    virtual status_t updateStructure(void);
    virtual status_t updateObject(void);
    virtual void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoSizeInout : public ExynosVpuIoSize {
public:
    /* Constructor */
    ExynosVpuIoSizeInout(ExynosVpuProcess *process, ExynosVpuIoSize *src_size = NULL);
    ExynosVpuIoSizeInout(ExynosVpuProcess *process, const struct vpul_sizes *size_info);

    status_t setOriginDimension(uint32_t width, uint32_t height, uint32_t roi_width, uint32_t roi_height);
    status_t getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height);

    status_t updateStructure(void);
    status_t updateObject(void);
};

class ExynosVpuIoSizeFix : public ExynosVpuIoSize {
public:
    /* Constructor */
    ExynosVpuIoSizeFix(ExynosVpuProcess *process);
    ExynosVpuIoSizeFix(ExynosVpuProcess *process, const struct vpul_sizes *size_info);

    status_t getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height);
};

class ExynosVpuIoSizeCrop : public ExynosVpuIoSize {
private:
    ExynosVpuIoSizeOpCropper *m_op_crop;

public:
    /* Constructor */
    ExynosVpuIoSizeCrop(ExynosVpuProcess *process, ExynosVpuIoSize *src_size, ExynosVpuIoSizeOpCropper *op_crop);
    ExynosVpuIoSizeCrop(ExynosVpuProcess *process, const struct vpul_sizes *size_info);

    ExynosVpuIoSizeOpCropper* getSizeOpCrop(void);
    status_t getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height);

    status_t updateStructure(void);
    status_t updateObject(void);
};

class ExynosVpuIoSizeScale : public ExynosVpuIoSize {
private:
    ExynosVpuIoSizeOpScale *m_op_scale;

public:
    /* Constructor */
    ExynosVpuIoSizeScale(ExynosVpuProcess *process, ExynosVpuIoSize *src_size, ExynosVpuIoSizeOpScale *op_scale);
    ExynosVpuIoSizeScale(ExynosVpuProcess *process, const struct vpul_sizes *size_info);

    ExynosVpuIoSizeOpScale* getSizeOpScale(void);
    status_t getDimension(uint32_t *ret_width, uint32_t *ret_height, uint32_t *ret_roi_width, uint32_t *ret_roi_height);

    status_t updateStructure(void);
    status_t updateObject(void);
};

class ExynosVpuIoSizeOp {
protected:
    ExynosVpuProcess *m_process;

    uint32_t m_sizeop_index_in_process;

public:
    uint32_t getIndex(void)
    {
        return m_sizeop_index_in_process;
    }
};

class ExynosVpuIoSizeOpScale : public ExynosVpuIoSizeOp {
private:
    struct vpul_scales m_scales_info;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_scales *scale);

    /* Constructor */
    ExynosVpuIoSizeOpScale(ExynosVpuProcess *process);
    ExynosVpuIoSizeOpScale(ExynosVpuProcess *process, const struct vpul_scales *scales_info);

    struct vpul_scales *getScaleInfo(void)
    {
        return &m_scales_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuIoSizeOpCropper : public ExynosVpuIoSizeOp {
private:
    struct vpul_croppers m_cropper_info;

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_croppers *cropper);

    /* Constructor */
    ExynosVpuIoSizeOpCropper(ExynosVpuProcess *process);
    ExynosVpuIoSizeOpCropper(ExynosVpuProcess *process, const struct vpul_croppers *cropper_info);

    struct vpul_croppers* getCropperInfo(void)
    {
        return &m_cropper_info;
    }

    status_t updateStructure(void);
    status_t updateObject(void);

    status_t checkConstraint(void);
    void displayObjectInfo(uint32_t tab_num = 0);
};

class ExynosVpuProcess : public ExynosVpuVertex {
private:
    Vector<ExynosVpuIoDynamicMapRoi*> m_io_dynamic_roi_list;
    Vector<ExynosVpuIoFixedMapRoi*> m_io_fixed_roi_list;
    Vector<ExynosVpuIoTypesDesc*> m_io_iotypes_list;
    Vector<ExynosVpuIoSize*> m_io_size_list;
    Vector<ExynosVpuIoSizeOpScale*> m_io_sizeop_scale_list;
    Vector<ExynosVpuIoSizeOpCropper*> m_io_sizeop_cropper_list;

    Vector<ExynosVpuIoSizeOpCropper*> m_static_coff;

public:

private:

public:
    static void display_structure_info(uint32_t tab_num, struct vpul_process *process);

    /* Constructor */
    ExynosVpuProcess(ExynosVpuTask *task);
    ExynosVpuProcess(ExynosVpuTask *task, const struct vpul_vertex *vertex_info);

    /* Destructor */
    virtual ~ExynosVpuProcess();
    struct vpul_process* getProcessInfo(void)
    {
        return &m_vertex_info.proc;
    }

    uint32_t addDynamicMapRoi(ExynosVpuIoDynamicMapRoi *dynamic_map_roi);
    uint32_t addFixedMapRoi(ExynosVpuIoFixedMapRoi *fixed_map_roi);
    uint32_t addIotypes(ExynosVpuIoTypesDesc *iotypes);
    uint32_t addSize(ExynosVpuIoSize *size);
    uint32_t addSizeOpScale(ExynosVpuIoSizeOpScale *scale);
    uint32_t addSizeOpCropper(ExynosVpuIoSizeOpCropper *cropper);

    status_t addStaticCoffValue(int32_t coff[], uint32_t coff_num, uint32_t *index_ret);

    ExynosVpuIoDynamicMapRoi* getDynamicMapRoi(uint32_t index);
    ExynosVpuIoFixedMapRoi* getFixedMapRoi(uint32_t index);
    ExynosVpuIoTypesDesc* getIotypesDesc(uint32_t index);
    ExynosVpuIoSize* getIoSize(uint32_t index);
    ExynosVpuIoSizeOpScale* getIoSizeOpScale(uint32_t index);
    ExynosVpuIoSizeOpCropper* getIoSizeOpCropper(uint32_t index);

    int32_t calcPtsMapIndex(uint32_t iotype_index);

    status_t getPtsMapMemmapList(uint32_t pts_map_index, List<ExynosVpuMemmap*> *ret_memmap_list);
    struct vpul_roi_desc* getPtRoiDesc(uint32_t index);

    status_t updateVertexIo(uint32_t std_width, uint32_t std_height);

    virtual status_t updateStructure(void);
    virtual status_t updateObject(void);

    virtual void displayProcessInfo(uint32_t tab_num);
};

class ExynosVpuHostReport : public ExynosVpuVertex {
private:

public:

private:

public:

};

}; // namespace android
#endif
