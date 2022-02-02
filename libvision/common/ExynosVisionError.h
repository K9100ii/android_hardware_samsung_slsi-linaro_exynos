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

#ifndef EXYNOS_VISION_ERROR_H
#define EXYNOS_VISION_ERROR_H

#include <VX/vx.h>

#include "ExynosVisionReference.h"

namespace android {

class ExynosVisionError : public ExynosVisionReference {
private:
    /*! \brief The specific error code contained in this object. */
    vx_status m_error_type;

public:

private:

public:
    /* Constructor */
    ExynosVisionError(ExynosVisionContext *context, vx_status kernel_enum);

    /* Destructor */
    virtual ~ExynosVisionError(void);

    virtual vx_status getStatus(void)
    {
        return m_error_type;
    }
    virtual vx_status destroy(void)
    {
        return VX_SUCCESS;
    }
};

}; // namespace android
#endif
