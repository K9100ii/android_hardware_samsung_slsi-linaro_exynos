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

#ifndef EXYNOS_VISION_RESOURCE_MNGR_H
#define EXYNOS_VISION_RESOURCE_MNGR_H

#include <utils/threads.h>
#include <utils/Mutex.h>
#include <utils/List.h>
#include <utils/Vector.h>

#include <VX/vx.h>

#include "ExynosVisionMemoryAllocator.h"
#include "ExynosVisionPool.h"
#include "ExynosVisionState.h"

#define RES_DBG_MSG  0

namespace android {

enum RES_ELEMENT_STATE {
    RES_ELEMENT_NO_LOCK,
    RES_ELEMENT_WRITE_LOCK,
    RES_ELEMENT_READ_LOCK
};

template<typename T>
class ExynosVisionResElement {
private:
    T m_resource;

    ExynosVisionState m_state;

    vx_uint32 m_frame_id;
    /* this indicates the index within resource manager */
    vx_uint32 m_index;

    /* for management read reference */
    vx_int32 m_read_demand_number;

    vx_int32 m_read_ref_count;
    vx_int32 m_read_demand_count;

    /* this indicates the data is valid after writing */
    vx_bool m_valid;

public:

private:

public:
    /* Constructor */
    ExynosVisionResElement(void)
    {
        m_state.setState(RES_ELEMENT_NO_LOCK);
        m_frame_id = 0;
        m_index = 0;

        m_read_demand_number = 0;
        m_read_demand_count = 0;
        m_read_ref_count = 0;

        m_resource = NULL;

        m_valid = vx_true_e;
    }

    /* Destructor */
    virtual ~ExynosVisionResElement()
    {
        /* do nothing */
    }

    void clearReadInfo(void)
    {
        m_read_demand_number = 0;
        m_read_demand_count = 0;
        m_read_ref_count = 0;
    }

    void setResource(T resource)
    {
        m_resource = resource;
    }

    T getResource()
    {
        return m_resource;
    }
    void setId(vx_int32 frame_id)
    {
        m_frame_id = frame_id;
    };
    vx_uint32 getId()
    {
        return m_frame_id;
    };

    void setIndex(vx_int32 index)
    {
        m_index = index;
    };
    vx_uint32 getIndex()
    {
        return m_index;
    };
    vx_uint32 getReadRefCnt()
    {
        return m_read_ref_count;
    };
    vx_uint32 getReadDemandCnt()
    {
        return m_read_demand_count;
    };
    vx_uint32 getReadDemandNum()
    {
        return m_read_demand_number;
    };
    void setReadDemandNum(vx_uint32 demand_num)
    {
        m_read_demand_number = demand_num;
    }
    void incReadDemCnt(void)
    {
        m_read_demand_count++;
    }
    void incReadRefCnt(void)
    {
        m_read_ref_count++;
    }
    void decReadRefCnt(void)
    {
        m_read_ref_count--;
    }

    vx_bool isReadComplete(void)
    {
        if (m_read_ref_count == 0 && m_read_demand_count == m_read_demand_number)
            return vx_true_e;
        else
            return vx_false_e;
    }

    void setState(vx_enum new_state)
    {
        m_state.setState(new_state);
    }

    vx_enum getState(void)
    {
        return m_state.getState();
    }

    void setValid(vx_bool valid)
    {
        m_valid = valid;
    }

    vx_bool getValid(void)
    {
        return m_valid;
    }

};

enum RESOURCE_MNGR_TYPE {
    /* resource manager is not assigned */
    RESOURCE_MNGR_NULL,
    /* single resource, support slot event, single object */
    RESOURCE_MNGR_SOLID,
    /* multi resource, support slot event, multi object */
    RESOURCE_MNGR_SLOT,
    /* multi resource, support queue event, single object */
    RESOURCE_MNGR_QUEUE,
};

struct resource_solid_param {

};

struct resource_slot_param {
    vx_uint32 slot_num;
};

struct resource_queue_param {
    vx_uint32 direction;
};

struct resource_param {
    union {
        struct resource_solid_param solid_param;
        struct resource_slot_param slot_param;
        struct resource_queue_param queue_param;
    } param;
};

enum RESOURCE_CLASS_TYPE {
    RESOURCE_CLASS_NONE,
    RESOURCE_CLASS_SLOT,
    RESOURCE_CLASS_INPUT_QUEUE,
    RESOURCE_CLASS_OUTPUT_QUEUE
};

template<typename T>
class ExynosVisionResManager {
private:

protected:
    Mutex               m_res_mutex;
    vx_enum m_res_class_type;

    List<ExynosVisionResElement<T>*> m_using_res_element_list;

private:

public:
    /* Constructor */
    ExynosVisionResManager(void)
    {
        m_res_class_type = RESOURCE_CLASS_NONE;
    }

    /* Destructor */
    virtual ~ExynosVisionResManager()
    {
        /* do noting */
    }

    virtual vx_status registerResource(T)
    {
        VXLOGE("not implemented at res-class-type %d", m_res_class_type);
        return VX_FAILURE;
    }

    virtual vx_status destroy(void)
    {
        VXLOGE("not implemented at res-class-type %d", m_res_class_type);
        return VX_FAILURE;
    }

    virtual T waitAndGetEmptyResAndWrLk(vx_uint32 frame_id)
    {
        VXLOGE("not supported, frame_id:%d", frame_id);
        return NULL;
    }

    virtual vx_status setFilledRes(vx_uint32 frame_id, vx_bool data_valid)
    {
        VXLOGE("not supported, frame_id:%d, data_valid:%d", frame_id, data_valid);
        return VX_FAILURE;
    }

    virtual vx_status setFilledResAndRdLk(vx_uint32 frame_id, vx_uint32 demand_number, vx_bool data_valid)
    {
        VXLOGE("not supported, frame_id:%d, demand_num:%d, data_valid:%d", frame_id, demand_number, data_valid);
        return VX_FAILURE;
    }

    virtual T getFilledRes(vx_uint32 frame_id, vx_bool *ret_data_valid)
    {
        VXLOGE("not supported, frame_id:%d, &ret_data_valid:%p", frame_id, ret_data_valid);
        return NULL;
    }

    virtual vx_status putFilledRes(vx_uint32 frame_id)
    {
        VXLOGE("not supported, frame_id:%d", frame_id);
        return VX_FAILURE;
    }

    /* output queue type */
    virtual vx_status pushResource(vx_uint32 index, T resource)
    {
        VXLOGE("not supported, index:%d, resource:%p", index, resource);
        return VX_FAILURE;
    }

    /* input queue type */
    virtual vx_status pushResource(vx_uint32 index, T resource, vx_uint32 frame_id, vx_uint32 demand_number)
    {
        VXLOGE("not supported, index:%d, resource:%p, frame id:%d, demand:%d", index, resource, frame_id, demand_number);
        return VX_FAILURE;
    }

    virtual vx_status popResource(vx_uint32 *ret_index, vx_bool *ret_data_valid, T *ret_resource)
    {
        VXLOGE("not supported, &index:%p, &data_valid:%p, &resource:%p", ret_index, ret_data_valid, ret_resource);
        return VX_FAILURE;
    }

    virtual void displayBuff(vx_uint32 tab_num, vx_bool detail_info)
    {
        VXLOGE("not supported, %d, %d", tab_num, detail_info);
    }
};

template<typename T>
class ExynosVisionResSlotType : public ExynosVisionResManager<T> {
private:
    vx_uint32 m_slot_num;

    /* m_buf_element_list has the all of the res element */
    List<ExynosVisionResElement<T>*> m_registered_res_element_list;

    ExynosVisionPool<ExynosVisionResElement<T>*> m_free_res_element_pool;

public:

private:

public:
    /* Constructor */
    ExynosVisionResSlotType(vx_uint32 slot_num = 1)
    {
        ExynosVisionResManager<T>::m_res_class_type = RESOURCE_CLASS_SLOT;
        m_slot_num = slot_num;
    }

    /* Destructor */
    virtual ~ExynosVisionResSlotType()
    {
        /* do nothing */
    }

    virtual vx_status registerResource(T resource)
    {
        if (m_registered_res_element_list.size() >= m_slot_num) {
            ALOGE("[%s] resource number is overflowed, slot_num:%d", m_slot_num);
            return VX_FAILURE;
        }

        ExynosVisionResManager<T>::m_res_mutex.lock();
        ExynosVisionResElement<T> *res_element = new ExynosVisionResElement<T>();
        res_element->setResource(resource);
        res_element->setIndex(m_registered_res_element_list.size());

        m_registered_res_element_list.push_back(res_element);
        ExynosVisionResManager<T>::m_res_mutex.unlock();

        m_free_res_element_pool.putResource(res_element);

        return VX_SUCCESS;
    }

    virtual vx_status destroy(void)
    {
        vx_status status = VX_SUCCESS;

        m_free_res_element_pool.release();
        m_free_res_element_pool.flush();

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        for (elem_iter=m_registered_res_element_list.begin(); elem_iter!=m_registered_res_element_list.end(); elem_iter++)
            delete (*elem_iter);

        m_slot_num = 0;

        return status;
    }

    virtual T waitAndGetEmptyResAndWrLk(vx_uint32 frame_id)
    {
        pool_exception_t pool_exception;
        ExynosVisionResElement<T>* res_element = NULL;

        pool_exception = m_free_res_element_pool.getResource(&res_element);

        if (pool_exception == POOL_EXCEPTION_NONE) {
            ExynosVisionResManager<T>::m_res_mutex.lock();
            res_element->setId(frame_id);
            res_element->setState(RES_ELEMENT_WRITE_LOCK);
            res_element->setValid(vx_true_e);
            ExynosVisionResManager<T>::m_using_res_element_list.push_back(res_element);
            ExynosVisionResManager<T>::m_res_mutex.unlock();

            return res_element->getResource();
        } else {
            VXLOGE("getting empty buffer");
            displayBuff(0, vx_true_e);

            return NULL;
        }
    }

    virtual vx_status setFilledResAndRdLk(vx_uint32 frame_id, vx_uint32 demand_number, vx_bool data_valid)
    {
        vx_status status = VX_FAILURE;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;

        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin();
                elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                elem_iter++) {
            ExynosVisionResElement<T> *res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->setState(RES_ELEMENT_READ_LOCK);
                res_element->setValid(data_valid);
                res_element->setReadDemandNum(demand_number);
                status = VX_SUCCESS;
                break;
            }
        }
        ExynosVisionResManager<T>::m_res_mutex.unlock();

        if (status != VX_SUCCESS)
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);

        return status;
    }


    virtual T getFilledRes(vx_uint32 frame_id, vx_bool *ret_data_valid)
    {
#if (RES_DBG_MSG==1)
        VXLOGD("frame:%d", frame_id);
        VXLOGD("enter");
        displayBuff(0, vx_true_e);
#endif

        vx_status status = VX_FAILURE;
        ExynosVisionResElement<T>* res_element = NULL;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin();
                elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                elem_iter++) {
            res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->incReadRefCnt();
                res_element->incReadDemCnt();
                *ret_data_valid = res_element->getValid();
                status = VX_SUCCESS;
                break;
            }
        }
        ExynosVisionResManager<T>::m_res_mutex.unlock();

#if (RES_DBG_MSG==1)
        VXLOGD("exit");
        displayBuff(0, vx_true_e);
#endif

        if (status != VX_SUCCESS) {
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);
            this->displayBuff(0, vx_true_e);

            return NULL;
        } else {
            return res_element->getResource();
        }
    }

    virtual vx_status putFilledRes(vx_uint32 frame_id)
    {
#if (RES_DBG_MSG==1)
        VXLOGD("frame:%d", frame_id);
        VXLOGD("enter");
        displayBuff(0, vx_true_e);
#endif

        vx_status status = VX_FAILURE;
        ExynosVisionResElement<T> *res_element;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin();
                elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                elem_iter++) {
            res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->decReadRefCnt();
                status = VX_SUCCESS;
                break;
            }
        }

        if (status != VX_SUCCESS) {
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);
        } else {
            if (res_element->isReadComplete()) {
                ExynosVisionResManager<T>::m_using_res_element_list.erase(elem_iter);
                res_element->clearReadInfo();
                res_element->setId(0);
                res_element->setState(RES_ELEMENT_NO_LOCK);
                m_free_res_element_pool.putResource(res_element);
            }
        }

        ExynosVisionResManager<T>::m_res_mutex.unlock();

#if (RES_DBG_MSG==1)
        VXLOGD("exit");
        displayBuff(0, vx_true_e);
#endif

        return status;
    }

    virtual void displayBuff(vx_uint32 tab_num, vx_bool detail_info)
    {
        vx_char tap[MAX_TAB_NUM];
        ExynosVisionResElement<T> *res_element = NULL;

        vx_uint32 i;
        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;

        VXLOGD("%s[SLOT]used buf num:%d, free buf num:%d", MAKE_TAB(tap, tab_num), ExynosVisionResManager<T>::m_using_res_element_list.size(), m_free_res_element_pool.getFreeNum());
        if (detail_info == vx_true_e) {
            for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin(), i=0;
                    elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                    elem_iter++, i++) {
                res_element = *elem_iter;
                VXLOGD("%sused buf, frame_id:%d, state:%d, RRefCnt:%d, RDemCnt:%d, RDemNum:%d", MAKE_TAB(tap, tab_num), res_element->getId(), res_element->getState(),
                                                                                        res_element->getReadRefCnt(), res_element->getReadDemandCnt(), res_element->getReadDemandNum());
            }
        }
    }
};

template<typename T>
class ExynosVisionResQueueType : public ExynosVisionResManager<T> {
#define MAX_QUEUE_RES_NUM     10

private:

protected:
    Vector<ExynosVisionResElement<T>*> m_res_element_vector;
    ExynosVisionPool<ExynosVisionResElement<T>*> m_done_res_element_pool;

public:

private:

public:
    /* Constructor */
    ExynosVisionResQueueType(void)
    {
        for (vx_uint32 i=0; i<MAX_QUEUE_RES_NUM; i++) {
            ExynosVisionResElement<T> *res_element = new ExynosVisionResElement<T>();
            res_element->setIndex(m_res_element_vector.size());
            m_res_element_vector.push_back(res_element);
        }
    }

    /* Destructor */
    virtual ~ExynosVisionResQueueType()
    {
        for (vx_uint32 i=0; i<MAX_QUEUE_RES_NUM; i++) {
            if (m_res_element_vector.editItemAt(i)->getResource() != NULL)
                VXLOGE("index:%d still hold resource", i);

            delete m_res_element_vector.editItemAt(i);
        }

        m_res_element_vector.clear();
    }

    virtual vx_status destroy(void)
    {
        return VX_SUCCESS;
    }

    virtual vx_status popResource(vx_uint32 *ret_index, vx_bool *ret_data_valid, T *ret_resource)
    {
        vx_status status = VX_SUCCESS;

        pool_exception_t pool_exception;
        ExynosVisionResElement<T>* res_element = NULL;

        pool_exception = m_done_res_element_pool.getResource(&res_element);
        if (pool_exception == POOL_EXCEPTION_NONE) {
            ExynosVisionResManager<T>::m_res_mutex.lock();
            if (res_element->getResource() == NULL)
                VXLOGE("element holds null resource");

            *ret_index = res_element->getIndex();
            *ret_data_valid = res_element->getValid();
            *ret_resource = res_element->getResource();
            res_element->setResource(NULL);
            res_element->setId(0);
            ExynosVisionResManager<T>::m_res_mutex.unlock();
        } else {
            VXLOGE("getting empty buffer");
            status = VX_FAILURE;
        }

        return status;
    }
};

template<typename T>
class ExynosVisionResInputQueueType : public ExynosVisionResQueueType<T> {
private:

public:

private:

public:
    /* Constructor */
    ExynosVisionResInputQueueType(void)
    {
        ExynosVisionResManager<T>::m_res_class_type = RESOURCE_CLASS_INPUT_QUEUE;
    }

    virtual vx_status pushResource(vx_uint32 index, T resource, vx_uint32 frame_id, vx_uint32 demand_number) {
        ExynosVisionResElement<T> *res_element;

        ExynosVisionResManager<T>::m_res_mutex.lock();

        if (index < MAX_QUEUE_RES_NUM) {
            res_element = ExynosVisionResQueueType<T>::m_res_element_vector.editItemAt(index);
        } else {
            VXLOGE("index:%d is out of bound", index);

            ExynosVisionResManager<T>::m_res_mutex.unlock();
            return VX_FAILURE;
        }

        if (res_element->getResource() != NULL) {
            VXLOGE("index:%d is already assigned resource", index);

            ExynosVisionResManager<T>::m_res_mutex.unlock();
            return VX_FAILURE;
        }

        res_element->setResource(resource);
        res_element->setId(frame_id);
        res_element->setReadDemandNum(demand_number);
        res_element->setState(RES_ELEMENT_READ_LOCK);
        res_element->setValid(vx_true_e);

        ExynosVisionResManager<T>::m_using_res_element_list.push_back(res_element);

        ExynosVisionResManager<T>::m_res_mutex.unlock();
        return VX_SUCCESS;
    }

    virtual T getFilledRes(vx_uint32 frame_id, vx_bool *ret_data_valid)
    {
        vx_status status = VX_FAILURE;
        ExynosVisionResElement<T>* res_element = NULL;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin();
                elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                elem_iter++) {
            res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->incReadRefCnt();
                res_element->incReadDemCnt();
                *ret_data_valid = res_element->getValid();
                status = VX_SUCCESS;
                break;
            }
        }
        ExynosVisionResManager<T>::m_res_mutex.unlock();

        if (status != VX_SUCCESS) {
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);
            return NULL;
        } else {
            return res_element->getResource();
        }
    }

    virtual vx_status putFilledRes(vx_uint32 frame_id)
    {
        vx_status status = VX_FAILURE;
        ExynosVisionResElement<T> *res_element;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResManager<T>::m_using_res_element_list.begin();
                elem_iter!=ExynosVisionResManager<T>::m_using_res_element_list.end();
                elem_iter++) {
            res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->decReadRefCnt();
                status = VX_SUCCESS;
                break;
            }
        }

        if (status != VX_SUCCESS) {
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);
        } else {
            if (res_element->isReadComplete()) {
                ExynosVisionResManager<T>::m_using_res_element_list.erase(elem_iter);
                res_element->clearReadInfo();
                res_element->setState(RES_ELEMENT_NO_LOCK);
                ExynosVisionResQueueType<T>::m_done_res_element_pool.putResource(res_element);
            }
        }

        ExynosVisionResManager<T>::m_res_mutex.unlock();

        return status;
    }

    virtual void displayBuff(vx_uint32 tab_num, vx_bool detail_info)
    {
        vx_char tap[MAX_TAB_NUM];
        ExynosVisionResElement<T> *res_element = NULL;

        vx_uint32 i;
        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;

        VXLOGD("%s[QUEU][%d]rece buf num:%d, done buf num:%d", MAKE_TAB(tap, tab_num), detail_info, ExynosVisionResQueueType<T>::m_using_res_element_list.size(), ExynosVisionResQueueType<T>::m_done_res_element_pool.getFreeNum());
    }
};

template<typename T>
class ExynosVisionResOutputQueueType : public ExynosVisionResQueueType<T> {
private:
    ExynosVisionPool<ExynosVisionResElement<T>*> m_free_res_element_pool;

public:

private:

public:
    /* Constructor */
    ExynosVisionResOutputQueueType(void)
    {
        ExynosVisionResManager<T>::m_res_class_type = RESOURCE_CLASS_OUTPUT_QUEUE;
    }

    virtual vx_status pushResource(vx_uint32 index, T resource) {
        ExynosVisionResElement<T> *res_element;

        ExynosVisionResManager<T>::m_res_mutex.lock();
        if (index < MAX_QUEUE_RES_NUM) {
            res_element = ExynosVisionResQueueType<T>::m_res_element_vector.editItemAt(index);
        } else {
            VXLOGE("index:%d is out of bound", index);

            ExynosVisionResManager<T>::m_res_mutex.unlock();
            return VX_FAILURE;
        }

        if (res_element->getResource() != NULL) {
            VXLOGE("index:%d is already assigned resource", index);

            ExynosVisionResManager<T>::m_res_mutex.unlock();
            return VX_FAILURE;
        }

        res_element->setResource(resource);
        ExynosVisionResManager<T>::m_res_mutex.unlock();

        m_free_res_element_pool.putResource(res_element);

        return VX_SUCCESS;
    }

    virtual T waitAndGetEmptyResAndWrLk(vx_uint32 frame_id) {
        pool_exception_t pool_exception;
        ExynosVisionResElement<T>* res_element = NULL;

        pool_exception = m_free_res_element_pool.getResource(&res_element);

        if (pool_exception == POOL_EXCEPTION_NONE) {
            ExynosVisionResManager<T>::m_res_mutex.lock();
            res_element->setId(frame_id);
            res_element->setState(RES_ELEMENT_WRITE_LOCK);
            res_element->setValid(vx_true_e);
            ExynosVisionResQueueType<T>::m_using_res_element_list.push_back(res_element);
            ExynosVisionResManager<T>::m_res_mutex.unlock();

            return res_element->getResource();
        } else {
            VXLOGE("getting empty buffer");
            return NULL;
        }
    }

    virtual vx_status setFilledRes(vx_uint32 frame_id, vx_bool data_valid)
    {
        vx_status status = VX_FAILURE;
        ExynosVisionResElement<T> *res_element;

        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;
        ExynosVisionResManager<T>::m_res_mutex.lock();
        for (elem_iter=ExynosVisionResQueueType<T>::m_using_res_element_list.begin();
                elem_iter!= ExynosVisionResQueueType<T>::m_using_res_element_list.end();
                elem_iter++) {
            res_element = *elem_iter;
            if (res_element->getId() == frame_id) {
                res_element->setState(RES_ELEMENT_NO_LOCK);
                res_element->setValid(data_valid);
                status = VX_SUCCESS;
                break;
            }
        }

        if (status != VX_SUCCESS) {
            VXLOGE("cannot find perfer buffer, id:%d", frame_id);
        } else {
            ExynosVisionResQueueType<T>::m_using_res_element_list.erase(elem_iter);
            ExynosVisionResQueueType<T>::m_done_res_element_pool.putResource(res_element);
        }

        ExynosVisionResManager<T>::m_res_mutex.unlock();

        return status;
    }

    virtual void displayBuff(vx_uint32 tab_num, vx_bool detail_info)
    {
        vx_char tap[MAX_TAB_NUM];
        ExynosVisionResElement<T> *res_element = NULL;

        vx_uint32 i;
        typename List<ExynosVisionResElement<T>*>::iterator elem_iter;

        VXLOGD("%s[QUEU][%d]free buf num:%d, done buf num:%d", MAKE_TAB(tap, tab_num), detail_info,
                m_free_res_element_pool.getFreeNum(),
                ExynosVisionResQueueType<T>::m_done_res_element_pool.getFreeNum());
    }
};

}; // namespace android
#endif
