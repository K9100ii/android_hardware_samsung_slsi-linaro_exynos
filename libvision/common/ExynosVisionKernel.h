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

#ifndef EXYNOS_OPENVX_KERNEL_H
#define EXYNOS_OPENVX_KERNEL_H

#include <VX/vx.h>
#include <VX/vx_helper.h>

#include "ExynosVisionReference.h"
#include "ExynosVisionMeta.h"
#include "ExynosVisionParameter.h"

enum vx_kernel_executor_e {
    KERNEL_CMODEL = 1 << 0,
    KERNEL_VPU = 1 << 1,
};

struct kernel_description {
    vx_kernel_e    kernel_enum;
    vx_kernel_executor_e   kernel_executor;
};

/*! \brief The internal representation of the attributes associated with a run-time parameter.
 * \ingroup group_int_kernel
 */
typedef struct _vx_signature_t {
    /*! \brief The array of directions */
    vx_enum        directions[VX_INT_MAX_PARAMS];
    /*! \brief The array of types */
    vx_enum        types[VX_INT_MAX_PARAMS];
    /*! \brief The array of states */
    vx_enum        states[VX_INT_MAX_PARAMS];
    /*! \brief The number of items in both \ref vx_signature_t::directions and \ref vx_signature_t::types. */
    vx_uint32      num_parameters;
} vx_signature_t;

namespace android {

class ExynosVisionKernel : public ExynosVisionReference {
private:
    /*! \brief */
    vx_char        m_kernel_name[VX_MAX_KERNEL_NAME];
    /*! \brief */
    vx_enum        m_enumeration;
    /*! \brief */
    vx_kernel_f    m_function;
    /*! \brief */
    vx_signature_t m_signature;
    /*! Indicates that the kernel is not yet enabled. */
    vx_bool        m_enabled;
    /*! \brief */
    vx_kernel_input_validate_f m_validate_input;
    /*! \brief */
    vx_kernel_output_validate_f m_validate_output;
    /*! \brief */
    vx_kernel_initialize_f m_initialize;
    /*! \brief */
    vx_kernel_deinitialize_f m_deinitialize;
    /*! \brief The collection of attributes of a kernel */
    vx_kernel_attr_t m_attributes;

public:

private:

public:
    /* Constructor */
    ExynosVisionKernel(ExynosVisionContext *context);

    /* Destructor */
    virtual ~ExynosVisionKernel();

    virtual vx_status destroy(void);

    vx_status assignFunc(const vx_char name[VX_MAX_KERNEL_NAME],
                         vx_enum enumeration,
                         vx_kernel_f func_ptr,
                         vx_uint32 numParams,
                         vx_kernel_input_validate_f input,
                         vx_kernel_output_validate_f output,
                         vx_kernel_initialize_f init,
                         vx_kernel_deinitialize_f deinit);

    vx_enum getEnumeration(void) const
    {
        return m_enumeration;
    }
    vx_status setKernelAttribute(vx_enum attribute, const void *ptr, vx_size size);
    vx_status queryKernel(vx_enum attribute, void *ptr, vx_size size);

    vx_status addParameterToKernel(vx_uint32 index, vx_enum dir, vx_enum data_type, vx_enum state);
    ExynosVisionParameter* getKernelParameterByIndex(vx_uint32 index);

    vx_status finalizeKernel(void);
    vx_status validateInput(ExynosVisionNode *node, vx_uint32 index);
    vx_status validateOutput(ExynosVisionNode *node, vx_uint32 index, ExynosVisionMeta *meta);
    vx_status initialize(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num);
    vx_status deinitialize(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num);
    vx_status kernelFunction(ExynosVisionNode *node, const ExynosVisionDataReference **parameters, vx_uint32 num) const;

    void fiiledAttr(vx_kernel_attr_t *attributes);

    const vx_char* getKernelName(void) const
    {
        return m_kernel_name;
    }
    vx_char* getKernelFuncName(void) const
    {
        vx_char *function_first_char = strrchr(m_kernel_name, '.') + 1;

        return function_first_char;
    }
    vx_uint32 getNumParams(void) const
    {
        return m_signature.num_parameters;
    }
    /* VX_INPUT, VX_OUTPUT and VX_BIDIRECTIONAL */
    enum vx_direction_e getParamDirection(vx_uint32 index) const
    {
        return (enum vx_direction_e)m_signature.directions[index];
    }
    enum vx_type_e getParamType(vx_uint32 index) const
    {
        return (enum vx_type_e)m_signature.types[index];

    }
    /* VX_PARAMETER_STATE_REQUIRED and VX_PARAMETER_STATE_OPTIONAL */
    enum vx_parameter_state_e getParamState(vx_uint32 index) const
    {
        return (enum vx_parameter_state_e)m_signature.states[index];
    }
    vx_bool getFinalizeFlag(void)
    {
        return m_enabled;
    }

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
