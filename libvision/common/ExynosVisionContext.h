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

#ifndef EXYNOS_VISION_CONTEXT_H
#define EXYNOS_VISION_CONTEXT_H

#include <VX/vx.h>
#include <VX/vx_internal.h>

#include "vx_osal.h"

#include "ExynosVisionReference.h"
#include "ExynosVisionKernel.h"
#include "ExynosVisionTarget.h"
#include "ExynosVisionError.h"

#include "ExynosVisionMemoryAllocator.h"
#include "ExynosVisionPerfMonitor.h"

namespace android {

using namespace std;

struct user_struct {
    /*! \brief Type constant */
    vx_enum type;
    /*! \brief Size in bytes */
    vx_size size;
} ;

typedef struct _kernel_target_st {
    vx_enum kernel_enum;
    vx_enum target_enum;
}kernel_target_t;

class ExynosVisionContext : public ExynosVisionReference {
private:
    List<ExynosVisionReference*> m_ref_list;
    List<ExynosVisionError*> m_error_ref_list;

    /*! \brief The array of kernel modules. */
    List<vx_module_t> m_module_list;
    /*! \brief The list of implemented targets */
    List<ExynosVisionTarget*> m_target_list;

    /*! \brief The log callback for errors */
    vx_log_callback_f   m_log_callback;
    /*! \brief The log semaphore */
    Mutex m_log_mutex;
    /*! \brief The log enable toggle. */
    vx_bool             m_log_enabled;
    /*! \brief If true the log callback is reentrant and doesn't need to be locked. */
    vx_bool             m_log_reentrant;

    /*! \brief The list of user defined structs. */
    List<struct user_struct> m_user_struct_list;

    /*! \brief The immediate mode border */
    vx_border_mode_t    m_imm_border;

    ExynosVisionPerfMonitor<ExynosVisionGraph*> *m_performance_monitor;

    ExynosVisionTarget* m_immediate_target;
public:

private:
    vx_uint32 getUniqueKernelsNum(void);
    vx_status fillUniqueKernelsTable(vx_kernel_info_t *table);

public:
    static vx_bool isValidContext(ExynosVisionContext *context);

    /* Constructor */
    ExynosVisionContext(void);

    /* Destructor */
    virtual ~ExynosVisionContext(void);

    vx_status init(void);
    virtual vx_status destroy(void);

    vx_bool createConstErrors();
    vx_bool addErrorReference(ExynosVisionError *error_ref);
    vx_bool addReference(ExynosVisionReference *ref);
    vx_bool addTarget(ExynosVisionTarget *target);
    ExynosVisionTarget* getTargetByEnum(vx_enum target_enum);

    vx_status removeReference(ExynosVisionReference *ref);

    vx_status loadKernels(const vx_char *name);
    ExynosVisionKernel *addKernel(const vx_char name[VX_MAX_KERNEL_NAME],
                                                         vx_enum enumeration,
                                                         vx_kernel_f func_ptr,
                                                         vx_uint32 numParams,
                                                         vx_kernel_input_validate_f input,
                                                         vx_kernel_output_validate_f output,
                                                         vx_kernel_initialize_f init,
                                                         vx_kernel_deinitialize_f deinit);
    ExynosVisionKernel* getKernelByEnum(vx_enum kernel_enum);
    ExynosVisionKernel* getKernelByName(const vx_char *name);
    ExynosVisionKernel* getKernelByTarget(vx_enum kernel_enum, vx_enum target_enum);

    vx_status setImmediateModeTarget(vx_enum target_enum);

    vx_status removeKernel(ExynosVisionKernel **kernel);

    vx_status queryContext(vx_enum attribute, void *ptr, vx_size size);
    vx_status setContextAttribute(vx_enum attribute, const void *ptr, vx_size size);
    vx_enum registerUserStruct(vx_size size);

    ExynosVisionReference *getErrorObject(vx_status status);

    void registerLogCallback(vx_log_callback_f callback, vx_bool reentrant);
    vx_status setDirective(vx_enum directive);
    void addLogEntry(ExynosVisionReference *ref, vx_status status, const char *message, ...);
    vx_size getUserStructsSize(vx_enum item_type);

    ExynosVisionPerfMonitor<ExynosVisionGraph*>* getPerfMonitor()
    {
        return m_performance_monitor;
    }

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android

#endif
