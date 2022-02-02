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

#ifndef EXYNOS_VPU_KERNEL_COLORCONV_H
#define EXYNOS_VPU_KERNEL_COLORCONV_H

#include <utility>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_api.h>
#include <VX/vx_helper.h>

#include "ExynosVpuKernel.h"
#include "ExynosVpuTaskIf.h"
#include "ExynosVpuVertex.h"
#include "ExynosVpuSubchain.h"
#include "ExynosVpuPu.h"

namespace android {

class ExynosVpuKernelColorConv : public ExynosVpuKernel {
private:
    vx_df_image_e m_src_format;
    vx_df_image_e m_dst_format;

    vx_uint32 m_src_io_num;
    vx_uint32 m_dst_io_num;

public:

private:
    vx_status setupBaseVxInfo(const vx_reference parameters[]);
    vx_status initTask(vx_node node, const vx_reference *parameters);
    vx_status updateTaskParamFromVX(vx_node node, const vx_reference *parameters);
    vx_status initVxIo(const vx_reference *parameters);

    vx_status initTaskFromBinary(void);
    vx_status initTaskFromApi(void);

    ExynosVpuTaskIf* initTask0FromBinary(void);
    ExynosVpuTaskIf* initTask0FromApi(void);

    vx_status setPreColorConvPu(ExynosVpuSubchainHw *subchain,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_in_pu_list,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list);
    vx_status setMainColorConvPu(ExynosVpuSubchainHw *subchain,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                    Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list);
    vx_status setPostColorConvPu(ExynosVpuSubchainHw *subchain,
                                                Vector<pair<ExynosVpuPu*, vx_uint32>> *in_pu_list,
                                                Vector<pair<ExynosVpuPu*, vx_uint32>> *ret_out_pu_list);

public:
    /* Contructor */
    ExynosVpuKernelColorConv(vx_char *name, vx_uint32 param_num);

    /* Destructor */
    virtual ~ExynosVpuKernelColorConv();

    static vx_status inputValidator(vx_node node, vx_uint32 index);
    static vx_status outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta);
};

}; // namespace android
#endif
