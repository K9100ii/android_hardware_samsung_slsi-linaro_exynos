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

#define LOG_TAG "ExynosScoreKernel"
#include <cutils/log.h>
#include <ctype.h>
#include <string.h>

#include "ExynosScoreKernel.h"
#include "score_kernel_util.h"

using namespace score;

namespace android {

ExynosScoreKernel::ExynosScoreKernel(vx_char *name, vx_uint32 param_num)
{
    strcpy(m_name, name);
    m_param_num = param_num;
}

/* Destructor */
ExynosScoreKernel::~ExynosScoreKernel(void)
{

}

vx_status
ExynosScoreKernel::initKernel(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    if (num != m_param_num) {
        VXLOGE("parameter number is not matching, expected:%d, received:%d", m_param_num, num);
        return VX_FAILURE;
    }

    status = updateScParamFromVx(node, parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("Updating score kernel parameters fails at initialize, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernel::kernelFunction(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    status = updateScBuffers(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("Updating score buffers fails, err:%d", status);
        goto EXIT;
    }

    status = runScvKernel(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("Processing score kernel fails, err:%d", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernel::finalizeKernel(void)
{
    vx_status status = VX_SUCCESS;
    status_t ret = NO_ERROR;

    status = destroy();
    if (status != VX_SUCCESS) {
        VXLOGE("destroy kernel fails at initialize, err:%d", status);
        goto EXIT;
    }

    score::ReleaseScoreInstance();

EXIT:
    return status;
}

}; /* namespace android */

