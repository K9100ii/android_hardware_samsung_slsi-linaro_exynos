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

#ifndef EXYNOS_SCORE_KERNEL_H
#define EXYNOS_SCORE_KERNEL_H

#include <utils/List.h>
#include <utils/Vector.h>
#include <map>
#include <ExynosVisionCommonConfig.h>

#include <VX/vx.h>
#include <score.h>

//#define EXYNOS_SCORE_KERNEL_IF_TRACE
#ifdef EXYNOS_SCORE_KERNEL_IF_TRACE
#define EXYNOS_SCORE_KERNEL_IF_IN()   VXLOGD("IN...")
#define EXYNOS_SCORE_KERNEL_IF_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_SCORE_KERNEL_IF_IN()   ((void *)0)
#define EXYNOS_SCORE_KERNEL_IF_OUT()  ((void *)0)
#endif

//#define EXYNOS_SCORE_KERNEL_TRACE
#ifdef EXYNOS_SCORE_KERNEL_TRACE
#define EXYNOS_SCORE_KERNEL_IN()   VXLOGD("IN..")
#define EXYNOS_SCORE_KERNEL_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_SCORE_KERNEL_IN()   ((void *)0)
#define EXYNOS_SCORE_KERNEL_OUT()  ((void *)0)
#endif

namespace android {

using namespace std;
using namespace score;

typedef struct _sc_param_info_t {
    data_buf_type format;
    sc_u32 width;
    sc_u32 height;
    sc_u32 plane;
} sc_param_info_t;

typedef struct _sc_buffer_info_t {
    sc_param_info_t param;
    sc_s32 fd;
    sc_buffer buf;
} sc_buffer_info_t;

class ExynosScoreKernel  {

private:
    vx_char m_name[VX_MAX_KERNEL_NAME];

protected:
    vx_uint32 m_param_num;

public:


private:

protected:

public:
    /* Constructor */
    ExynosScoreKernel(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosScoreKernel();

    vx_char* getName(void)
    {
        return m_name;
    }

    vx_status initKernel(vx_node node, const vx_reference *parameters, vx_uint32 num);
    vx_status finalizeKernel(void);

    vx_status kernelFunction(const vx_reference *parameters);

    virtual vx_status updateScParamFromVx(vx_node node, const vx_reference *parameters) = 0;
    virtual vx_status updateScBuffers(const vx_reference *parameters) = 0;
    virtual vx_status runScvKernel(const vx_reference *parameters) = 0;

    virtual vx_status destroy(void) = 0;

};

}; // namespace android
#endif

