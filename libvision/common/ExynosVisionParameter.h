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

#ifndef EXYNOS_VISION_Parameter_H
#define EXYNOS_VISION_Parameter_H

#include "ExynosVisionReference.h"

namespace android {

class ExynosVisionParameter : public ExynosVisionReference {
private:
    /*! \brief Index at which this parameter is tracked in both the node references and kernel signatures */
    vx_uint32      m_index;
    /*! \brief Pointer to the node which this parameter is associated with */
    ExynosVisionNode *m_node;
    /*! \brief Pointer to the kernel which this parameter is associated with, if retreived from
     * \ref vxGetKernelParameterByIndex.
     */
    ExynosVisionKernel *m_kernel;

public:

private:

public:
    /* Constructor */
    ExynosVisionParameter(ExynosVisionContext *context, ExynosVisionReference *scope);

    /* Destructor */
    virtual ~ExynosVisionParameter(void);

    void init(vx_uint32 index, ExynosVisionNode *node, ExynosVisionKernel *kernel);
    virtual vx_status destroy(void);

    vx_status queryParameter(vx_enum attribute, void *ptr, vx_size size);
    const ExynosVisionKernel* getKernelHandle(void) const
    {
        return m_kernel;
    }
    const ExynosVisionNode* getNodeHandle(void) const
    {
        return m_node;
    }
    ExynosVisionNode* getNode(void)
    {
        return m_node;
    }
    vx_uint32 getIndex(void) const
    {
        return m_index;
    }

    virtual void displayInfo(vx_uint32 tab_num, vx_bool detail_info);
};

}; // namespace android
#endif
