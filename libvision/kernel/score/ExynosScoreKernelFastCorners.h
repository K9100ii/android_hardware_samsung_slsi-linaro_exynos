/*
 * Copyright (C) 2016, Samsung Electronics Co. LTD
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

#ifndef EXYNOS_SCORE_KERNEL_FASTCORNERS_H
#define EXYNOS_SCORE_KERNEL_FASTCORNERS_H

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include <score.h>
#include "ExynosScoreKernel.h"
#include "score_kernel_util.h"

namespace android {

class ExynosScoreKernelFastCorners : public ExynosScoreKernel {
private:
    ScBuffer*   m_in_buf;
    ScBuffer*   m_out_buf;
    sc_s32      m_threshold;
    sc_enum_e   m_corner_policy;
    sc_enum_e   m_nms_policy;

    vx_image    m_out_image;

public:

private:

protected:

public:
    /* Contructor */
    ExynosScoreKernelFastCorners(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosScoreKernelFastCorners();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);

    vx_status updateScParamFromVx(vx_node node, const vx_reference *parameters);
    vx_status prepareScBuffers(const vx_reference *parameters);

    vx_status updateScBuffers(const vx_reference *parameters);
    vx_status runScvKernel(const vx_reference *parameters);

    vx_status destroy(void);

};

}; // namespace android
#endif
