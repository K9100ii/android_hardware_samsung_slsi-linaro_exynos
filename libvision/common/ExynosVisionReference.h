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

#ifndef EXYNOS_VISION_REFERENCE_H
#define EXYNOS_VISION_REFERENCE_H

#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Vector.h>
#include <map>

#include "ExynosVisionCommonConfig.h"

#include <VX/vx.h>
#include <VX/vx_internal.h>

#include "ExynosVisionBufObject.h"
#include "ExynosVisionResManager.h"
#include "ExynosVisionMemoryAllocator.h"

namespace android {

using namespace std;

#define VX_REF_NONE (0)
#define VX_REF_INTERNAL  (0x1 << 1)
#define VX_REF_EXTERNAL  (0x1 << 2)
#define VX_REF_BOTH (VX_REF_INTERNAL |VX_REF_EXTERNAL)

typedef enum _vx_access_e {
    ACCESS_NONE = 0,
    ACCESS_MAP_MODE = 1,
    ACCESS_COPY_MODE = 2,
} vx_access_e;

class ExynosVisionContext;
class ExynosVisionGraph;
class ExynosVisionKernel;
class ExynosVisionNode;
class ExynosVisionSubgraph;
class ExynosVisionDelay;
class ExynosVisionMeta;
class ExynosVisionKernel;

class ExynosVisionReference {
private:
    static Mutex m_global_lock;
    static vx_uint32 m_ref_id_cnt;

    /*! \brief Set to an enum value in \ref vx_type_e. */
    vx_enum m_type;
    /*! \brief Pointer to the top level context.
     * If this reference is the context, this will be NULL.
     */
    ExynosVisionContext *m_context;
    /*! \brief The pointer to the object's scope parent. When virtual objects
     * are scoped within a graph, this will point to that parent graph. This is
     * left generic to allow future scoping variations. By default scope should
     * be the same as context.
     */
    ExynosVisionReference *m_scope;

    /*! \brief The count of the number of users with this reference. When
     * greater than 0, this can not be freed. When zero, the value can be
     * considered inaccessible.
     */
    vx_uint32 m_external_count;
    /*! \brief The count of the number of framework references. When
     * greater than 0, this can not be freed.
     */
    vx_uint32 m_internal_count;

    /* \brief construction error check */
    vx_status m_creation_status;

    vx_int32 m_id;
    vx_char m_name[VX_MAX_REFERENCE_NAME];

protected:
    /*! \brief The reference lock which is used to keep consistance of internal data. */
    Mutex m_internal_lock;
    /*! \brief The reference lock which is used to protect multiple access from external area(=application). */
    Mutex m_external_lock;

    /* object list that referencing current object */
    List<ExynosVisionReference*> m_internal_object_list;

public:
    /*! \brief Used to validate references, must be set to VX_MAGIC. */
    vx_uint32 m_magic;

private:

public:
    static vx_bool isValidReference(ExynosVisionReference *ref);
    static vx_bool isValidReference(ExynosVisionReference *ref, vx_enum type);
    static vx_status releaseReferenceInt(ExynosVisionReference **ref, vx_enum reftype, ExynosVisionReference *releasing_ref = NULL);
    static vx_size sizeOfType(vx_enum type);

    /* Constructor */
    ExynosVisionReference(ExynosVisionContext* context, vx_type_e type, ExynosVisionReference* scope, vx_bool is_add_context);

    /* Destructor */
    virtual ~ExynosVisionReference(void);

    virtual vx_status destroy(void);
    virtual vx_status getStatus(void)
    {
        return VX_SUCCESS;
    }

    vx_status queryReference(vx_enum attribute, void *ptr, vx_size size);
    vx_status hint(vx_enum hint);
    vx_status directive(vx_enum directive);

    vx_uint32 incrementReference(vx_uint32 reftype, ExynosVisionReference *ref = NULL);
    vx_uint32 decrementReference(vx_uint32 reftype, ExynosVisionReference *ref = NULL);

    ExynosVisionContext *getContext(void)
    {
        return m_context;
    }
    vx_enum getType(void)
    {
        return m_type;
    }
    vx_status getCreationStatus(void)
    {
        return m_creation_status;
    }
    vx_uint32 getId(void)
    {
        return m_id;
    }
    const vx_char* getName(void) const
    {
        return m_name;
    }
    vx_uint32 getInternalCnt(void)
    {
        return m_internal_count;
    }
    vx_uint32 getExternalCnt(void)
    {
        return m_external_count;
    }
    vx_enum getScopeType(void)
    {
        return m_scope->getType();
    }
    ExynosVisionReference* getScope(void)
    {
        return m_scope;
    }

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
    virtual void displayPerf(vx_uint32 tab_num, vx_bool detail_info);
};

class ExynosVisionImage;
class ExynosVisionArray;
class ExynosVisionMatrix;

typedef ExynosVisionBufMemory default_resource_t;
typedef vx_uint32 empty_resource_t;
typedef Vector<ExynosVisionImage*> pyramid_resource_t;
typedef Vector<ExynosVisionBufMemory*> image_resource_t;
typedef ExynosVisionArray lut_resource_t;
typedef ExynosVisionMatrix convolution_resource_t;

class ExynosVisionDataReference : public ExynosVisionReference {

typedef struct _node_connect_info_t {
    ExynosVisionNode *node;
    vx_uint32 node_index;
} node_connect_info_t;

private:
    map<ExynosVisionGraph*, List<node_connect_info_t>> m_input_node_list;
    map<ExynosVisionGraph*, List<node_connect_info_t>> m_output_node_list;

    /* \brief This indicates if the object belongs to a delay */
    ExynosVisionDelay *m_delay;
    /* \brief This indicates the original delay slot index when the object belongs to a delay */
    vx_int32 m_delay_slot_index;

    vx_uint32 m_access_count;

protected:

    /*! \brief This indicates if the object is virtual or not */
    vx_bool m_is_virtual;

    /*! \brief This indicates that if the object is virtual whether it is accessible at the moment or not */
    vx_uint32 m_kernel_count;

    /* variable for resource management */
    vx_enum m_res_type;

    /*! \brief This indicates if the object's memory is allocated or not */
    vx_bool m_is_allocated;

    vx_bool m_allocator_need_flag;
    ExynosVisionAllocator *m_allocator;

    map<void*, ExynosVisionDataReference*> m_clone_object_map;

    List<ExynosVisionDataReference*> m_alliance_ref_list;
#ifdef USE_OPENCL_KERNEL
    /*! \brief memory structure for opencl kernel */
    vxcl_mem_t m_cl_memory;
#endif

public:

private:
    ExynosVisionDataReference *getVirtualInstance(void);

public:
    static vx_bool checkWriteDependency(ExynosVisionDataReference *data_ref1, ExynosVisionDataReference *data_ref2);
    static vx_bool isValidDataReference(ExynosVisionReference *ref);
    static vx_status jointAlliance(ExynosVisionDataReference *data_ref1, ExynosVisionDataReference *data_ref2);

    /* Constructor */
    ExynosVisionDataReference(ExynosVisionContext* context, vx_type_e type, ExynosVisionReference* scope,
                                                            vx_bool is_add_context, vx_bool is_virtual);

    /* Destructor */
    virtual ~ExynosVisionDataReference(void);

    /* allocator_flag : some object such as image object needs memory allocator for buffer */
    vx_status init(vx_bool allocator_flag);

    /* inc/dec count of kernel using data reference */
    vx_status increaseKernelCount(void);
    vx_status decreaseKernelCount(void);

    vx_uint32 increaseAccessCount(void);
    vx_uint32 decreaseAccessCount(void);

    /* data ref direction is a input or output refer from node's view */
    vx_bool connectNode(ExynosVisionGraph *graph, ExynosVisionNode *node, vx_uint32 node_index, enum vx_direction_e data_ref_direction);
    vx_bool disconnectNode(ExynosVisionGraph *graph, ExynosVisionNode *node, vx_uint32 node_index, enum vx_direction_e data_ref_direction);

    vx_status registerAlliance(ExynosVisionDataReference *ref);

    vx_uint32 getDirectInputNodeNum(ExynosVisionGraph *graph);
    vx_uint32 getDirectOutputNodeNum(ExynosVisionGraph *graph);

    vx_uint32 getIndirectInputNodeNum(ExynosVisionGraph *graph);
    vx_uint32 getIndirectOutputNodeNum(ExynosVisionGraph *graph);

    ExynosVisionNode* getDirectOutputNode(ExynosVisionGraph *graph, vx_uint32 node_idx);
    ExynosVisionNode* getIndirectOutputNode(ExynosVisionGraph *graph, vx_uint32 node_idx);

    vx_bool isVirtual(void)
    {
        return m_is_virtual;
    }
    vx_bool isAllocated(void)
    {
        return m_is_allocated;
    }
    virtual vx_bool isQueue(void)
    {
        return vx_false_e;
    }
    vx_enum getResourceType(void)
    {
        return m_res_type;
    }
    vx_bool isDelayElement(void)
    {
        if (m_delay)
            return vx_true_e;
        else
            return vx_false_e;
    }
    ExynosVisionDelay* getDelay()
    {
        return m_delay;
    }
    void initReferenceForDelay(ExynosVisionDelay *delay, vx_int32 slot_index)
    {
        m_delay = delay;
        m_delay_slot_index = slot_index;
    }
    vx_int32 getDelaySlotIndex()
    {
        return m_delay_slot_index;
    }
    virtual vx_status verifyMeta(ExynosVisionMeta *meta)
    {
        if (!meta)
            VXLOGE("meta is null");

        return VX_SUCCESS;
    }

    virtual ExynosVisionDataReference *getInputExclusiveRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputExclusiveRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputExclusiveRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputExclusiveRef(vx_uint32 frame_cnt, vx_bool data_valid);

    template<typename T>
    ExynosVisionDataReference* getInputExclusiveRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, vx_bool *ret_data_valid, T **ret_current_resource)
    {
        ExynosVisionDataReference *clone_object = NULL;
        T *resource = NULL;

        if (m_res_type == RESOURCE_MNGR_QUEUE) {
            resource = resource_manager->getFilledRes(frame_cnt, ret_data_valid);
            if (resource == NULL)
                VXLOGE("resource manager has no resource");

            *ret_current_resource = resource;
            clone_object = this;
        } else {
            VXLOGE("exclusive reference<T> can't be resource type:%d", m_res_type);

            *ret_data_valid = vx_false_e;
            *ret_current_resource = NULL;
        }

        return clone_object;
    }

    template<typename T>
    vx_status putInputExclusiveRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, T **ret_current_resource)
    {
        vx_status status = VX_FAILURE;

        if (m_res_type == RESOURCE_MNGR_QUEUE) {
            status = resource_manager->putFilledRes(frame_cnt);
            if (status != VX_SUCCESS)
                VXLOGE("resource manager has no frame number");

            *ret_current_resource = NULL;
        } else {
            VXLOGE("exclusive reference<T> can't be resource type:%d", m_res_type);

            *ret_current_resource = NULL;
        }

        return status;
    }

    template<typename T>
    ExynosVisionDataReference* getOutputExclusiveRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, T **ret_current_resource)
    {
        ExynosVisionDataReference *clone_object = NULL;
        T *resource = NULL;

        VXLOGD2("%s, frame_cnt:%d", getName(), frame_cnt);

        if (m_res_type == RESOURCE_MNGR_QUEUE) {
            resource = resource_manager->waitAndGetEmptyResAndWrLk(frame_cnt);
            if (resource == NULL)
                VXLOGE("resource manager has no resource");

            *ret_current_resource = resource;
            clone_object = this;
        } else {
            VXLOGE("exclusive reference<T> can't be resource type:%d", m_res_type);

            *ret_current_resource = NULL;
        }

        return clone_object;
    }

    template<typename T>
    vx_status putOutputExclusiveRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, T **ret_current_resource, vx_bool data_valid)
    {
        vx_status status = VX_FAILURE;
        VXLOGD2("%s, frame_cnt:%d", getName(), frame_cnt);

        if (m_res_type == RESOURCE_MNGR_QUEUE) {
            status = resource_manager->setFilledRes(frame_cnt, data_valid);
            if (status != VX_SUCCESS)
                VXLOGE("resource manager has no frame number");

            *ret_current_resource = NULL;
        } else {
            VXLOGE("exclusive reference<T> can't be resource type:%d", m_res_type);

            *ret_current_resource = NULL;
        }

        return status;
    }

    virtual ExynosVisionDataReference *getInputShareRef(vx_uint32 frame_cnt, vx_bool *ret_data_valid);
    virtual vx_status putInputShareRef(vx_uint32 frame_cnt);
    virtual ExynosVisionDataReference *getOutputShareRef(vx_uint32 frame_cnt);
    virtual vx_status putOutputShareRef(vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid);

    template<typename T>
    ExynosVisionDataReference* getInputShareRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, vx_bool *ret_data_valid)
    {
        ExynosVisionDataReference *clone_object = NULL;
        T *resource = NULL;

        VXLOGD2("%s, frame_cnt:%d", getName(), frame_cnt);

        if (m_res_type == RESOURCE_MNGR_SOLID) {
            resource = resource_manager->getFilledRes(frame_cnt, ret_data_valid);
            if (resource == NULL)
                VXLOGE("resource manager has no resource");

            clone_object = this;
        } else if (m_res_type == RESOURCE_MNGR_SLOT) {
            resource = resource_manager->getFilledRes(frame_cnt, ret_data_valid);
            if (resource == NULL)
                VXLOGE("resource manager has no resource");

            clone_object = m_clone_object_map[resource];
            if (clone_object == NULL) {
                VXLOGE("%s doesn't have clone object of %p", getName(), resource);
            }
        } else {
            VXLOGE("shared reference can't be resource type:%d", m_res_type);
        }

        return clone_object;
    }

    template<typename T>
    vx_status putInputShareRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt)
    {
        vx_status status = VX_FAILURE;

        if ((m_res_type == RESOURCE_MNGR_SOLID) || (m_res_type == RESOURCE_MNGR_SLOT)) {
            status = resource_manager->putFilledRes(frame_cnt);
            if (status != VX_SUCCESS)
                VXLOGE("resource manager has no frame number");
        } else {
            VXLOGE("shared reference can't be resource type:%d", m_res_type);
        }

        return status;
    }

    template<typename T>
    ExynosVisionDataReference* getOutputShareRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt)
    {
        ExynosVisionDataReference *clone_object = NULL;
        T *resource = NULL;

        VXLOGD2("%s, frame_cnt:%d", getName(), frame_cnt);

        if (m_res_type == RESOURCE_MNGR_SOLID) {
            resource = resource_manager->waitAndGetEmptyResAndWrLk(frame_cnt);
            if (resource == NULL)
                VXLOGE("%s, resource manager has no resource", getName());

            clone_object = this;
        } else if (m_res_type == RESOURCE_MNGR_SLOT) {
            resource = resource_manager->waitAndGetEmptyResAndWrLk(frame_cnt);
            if (resource == NULL)
                VXLOGE("resource manager has no resource");

            clone_object = m_clone_object_map[resource];
            if (clone_object == NULL) {
                VXLOGE("%s doesn't have clone object of %p", getName(), resource);

                map<void*, ExynosVisionDataReference*>::iterator ref_iter;
                for (ref_iter=m_clone_object_map.begin(); ref_iter!=m_clone_object_map.end(); ref_iter++) {
                    VXLOGE("indext:%p, clone_object:%p", (*ref_iter).first, (*ref_iter).second);
                }
            }
        } else {
            VXLOGE("shared reference can't be resource type:%d", m_res_type);
        }

        return clone_object;
    }

    template<typename T>
    vx_status putOutputShareRef_T(ExynosVisionResManager<T*> *resource_manager, vx_uint32 frame_cnt, vx_uint32 demand_num, vx_bool data_valid)
    {
        vx_status status = VX_FAILURE;
        VXLOGD2("%s, frame_cnt:%d, demand:%d", getName(), frame_cnt, demand_num);

        if ((m_res_type == RESOURCE_MNGR_SOLID) || (m_res_type == RESOURCE_MNGR_SLOT)) {
            status = resource_manager->setFilledResAndRdLk(frame_cnt, demand_num, data_valid);
            if (status != VX_SUCCESS)
                VXLOGE("resource manager has no frame number");
        } else {
            VXLOGE("shared reference can't be resource type:%d", m_res_type);
        }

        return status;
    }

    virtual vx_status allocateMemory(void);
    virtual vx_status allocateMemory(vx_enum res_type, struct resource_param *param);

    template<typename T>
    vx_status allocateMemory_T(vx_enum res_type, struct resource_param *res_param,
                                                    vx_bool memory_allocator_need,
                                                    ExynosVisionResManager<T*> **ret_resource_manager,
                                                    List<T*> *resource_list,
                                                    T **ret_current_resource)
    {
        vx_status status = VX_SUCCESS;

        if (m_is_allocated == vx_false_e) {
            VXLOGD2("%s, allocating memory", getName());
            ExynosVisionResManager<T*> *resource_manager = NULL;

            m_allocator_need_flag = memory_allocator_need;
            if (m_allocator_need_flag) {
                m_allocator = new ExynosVisionIonAllocator();
                status_t ret = m_allocator->init(true);
                if (ret != NO_ERROR) {
                    VXLOGE("ion allocator's init fail, ret:%d", ret);
                    status = VX_ERROR_NO_RESOURCES;
                }
            }

            T *resource = NULL;

            if (res_type == RESOURCE_MNGR_SOLID) {
                resource_manager = new ExynosVisionResSlotType<T*>();

                /* solid type has single resource */
                resource = NULL;
                status = allocateResource(&resource);
                if (status == VX_SUCCESS) {
                    resource_list->push_back(resource);
                    status = resource_manager->registerResource(resource);
                    if (status == VX_SUCCESS)
                        *ret_current_resource = resource;
                    else
                        VXLOGE("buffer allocation fails, err:%d", status);
                } else {
                    VXLOGE("buffer allocation fails at %s", getName());
                }
            } else if (res_type == RESOURCE_MNGR_SLOT) {
                resource_manager = new ExynosVisionResSlotType<T*>(res_param->param.slot_param.slot_num);

                for (vx_uint32 i=0; i<res_param->param.slot_param.slot_num; i++) {
                    status = allocateResource(&resource);
                    if (status == VX_SUCCESS) {
                        resource_list->push_back(resource);
                        status = resource_manager->registerResource(resource);
                        if (status != VX_SUCCESS) {
                            VXLOGE("buffer allocation fails, err:%d", status);
                            break;
                        }
                    } else {
                        VXLOGE("buffer allocation fails at %s", getName());
                    }

                    status = createCloneObject(resource);
                    if (status != VX_SUCCESS)
                        VXLOGE("creating clone object fails, err:%d", status);
                }
            } else if (res_type == RESOURCE_MNGR_QUEUE) {
                if (res_param->param.queue_param.direction == VX_INPUT) {
                    resource_manager = new ExynosVisionResInputQueueType<T*>();
                } else if (res_param->param.queue_param.direction == VX_OUTPUT) {
                    resource_manager = new ExynosVisionResOutputQueueType<T*>();
                } else {
                    status = VX_FAILURE;
                }
            } else {
                VXLOGE("unsupported resource manager type:%d", res_type);
            }

            if (status == VX_SUCCESS) {
                m_res_type = res_type;
                m_is_allocated = vx_true_e;

                *ret_resource_manager = resource_manager;
            } else {
                status = VX_ERROR_NOT_ALLOCATED;
                VXLOGE("failed to allocated resource");

                /* unroll */
                m_is_allocated = vx_false_e;

                resource_manager->destroy();
                delete resource_manager;
                *ret_resource_manager = NULL;
            }
        }

        return status;
    }

    template<typename T>
    vx_status freeMemory_T(ExynosVisionResManager<T*> **resource_manager, List<T*> *resource_list)
    {
        vx_status status = VX_SUCCESS;

        if (m_is_allocated == vx_true_e) {
            typename List<T*>::iterator res_iter;
            for (res_iter=resource_list->begin(); res_iter!=resource_list->end(); res_iter++) {
                if (m_res_type == RESOURCE_MNGR_SLOT) {
                    status = destroyCloneObject(*res_iter);
                    if (status != VX_SUCCESS)
                        VXLOGE("destroying clone object fails, err:%d", status);
                }
                freeResource(*res_iter);
            }
            resource_list->clear();

            (*resource_manager)->destroy();
            delete *resource_manager;
            *resource_manager = NULL;

            if (m_allocator) {
                if (m_allocator_need_flag == vx_false_e) {
                    VXLOGE("allocator is assigned to wrong object");
                }

                delete m_allocator;
            }

            m_is_allocated = vx_false_e;
        }
#ifdef USE_OPENCL_KERNEL
        /* free cl memory object only if it is allocated */
        cl_int err = 0;
        vx_uint32 pln;
        if (m_cl_memory.allocated == vx_true_e) {
            for (pln = 0; pln < m_cl_memory.nptrs; pln++) {
                err = clReleaseMemObject(m_cl_memory.hdls[pln]);
                if (err != CL_SUCCESS)
                    VXLOGE("clReleaseMemObject fails, cl_err:%d", err);
            }
        }
#endif
        return status;
    }

    /* Data reference class should know each child class's resource type, because template function couldn't be virtual function .
         We can't use like below.

            template<typename T>
            virtual vx_status allocateResource(T **ret_resource);

            template<typename T>
            virtual vx_status freeResource(T* resource);
    */
    virtual vx_status allocateResource(default_resource_t **ret_resource);
    virtual vx_status allocateResource(image_resource_t **ret_resource);
    virtual vx_status allocateResource(pyramid_resource_t **ret_resource);
    virtual vx_status allocateResource(lut_resource_t **ret_resource);
    virtual vx_status allocateResource(empty_resource_t **ret_resource);
    virtual vx_status allocateResource(convolution_resource_t **ret_resource);

    virtual vx_status freeResource(default_resource_t* resource);
    virtual vx_status freeResource(image_resource_t* resource);
    virtual vx_status freeResource(pyramid_resource_t* resource);
    virtual vx_status freeResource(lut_resource_t* resource);
    virtual vx_status freeResource(empty_resource_t* resource);
    virtual vx_status freeResource(convolution_resource_t* resource);

    virtual vx_status createCloneObject(default_resource_t* resource);
    virtual vx_status createCloneObject(image_resource_t* resource);
    virtual vx_status createCloneObject(pyramid_resource_t* resource);
    virtual vx_status createCloneObject(lut_resource_t* resource);
    virtual vx_status createCloneObject(empty_resource_t* resource);
    virtual vx_status createCloneObject(convolution_resource_t* resource);

    virtual vx_status destroyCloneObject(default_resource_t* resource);
    virtual vx_status destroyCloneObject(image_resource_t* resource);
    virtual vx_status destroyCloneObject(pyramid_resource_t* resource);
    virtual vx_status destroyCloneObject(lut_resource_t* resource);
    virtual vx_status destroyCloneObject(empty_resource_t* resource);
    virtual vx_status destroyCloneObject(convolution_resource_t* resource);

    vx_status triggerDoneEventDirect(ExynosVisionGraph *graph, vx_uint32 frame_cnt);
    vx_status triggerDoneEventIndirect(ExynosVisionGraph *graph, vx_uint32 frame_cnt);

    void displayConn(vx_uint32 tab_num);
    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
