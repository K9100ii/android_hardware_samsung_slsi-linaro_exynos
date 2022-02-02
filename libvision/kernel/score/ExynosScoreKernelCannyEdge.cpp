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

#define LOG_TAG "ExynosScoreKernelCannyEdge"
#include <cutils/log.h>

#include <score.h>
#include "memory/score_ion_memory_manager.h"
#include "ExynosScoreKernelCannyEdge.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelCannyEdge::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8) {
                status = VX_SUCCESS;
            } else {
                VXLOGE("format is not matching, index: %d, format: 0x%x", index, format);
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_threshold hyst = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &hyst, sizeof(hyst));
        if (hyst) {
            vx_enum type = 0;
            vxQueryThreshold(hyst, VX_THRESHOLD_ATTRIBUTE_TYPE, &type, sizeof(type));
            if (type == VX_THRESHOLD_TYPE_RANGE) {
                status = VX_SUCCESS;
            }
            vxReleaseThreshold(&hyst);
        }
        vxReleaseParameter(&param);
    } else if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar value = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &value, sizeof(value));
            if (value) {
                vx_enum stype = 0;
                vxQueryScalar(value, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_INT32) {
                    vx_int32 gs = 0;
                    vxReadScalarValue(value, &gs);
                    if ((gs == 3) || (gs == 5) || (gs == 7)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&value);
            }
            vxReleaseParameter(&param);
        }
    } else if (index == 3) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar value = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &value, sizeof(value));
            if (value) {
                vx_enum norm = 0;
                vxReadScalarValue(value, &norm);
                if ((norm == VX_NORM_L1) ||
                    (norm == VX_NORM_L2)) {
                    status = VX_SUCCESS;
                }
                vxReleaseScalar(&value);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;
}

vx_status
ExynosScoreKernelCannyEdge::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 4) {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        if (src_param) {
            vx_image src = NULL;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            if (src) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = VX_DF_IMAGE_VIRT;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

                /* fill in the meta data with the attributes so that the checker will pass */
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&src);
            }
            vxReleaseParameter(&src_param);
        }
    }

    return status;
}

ExynosScoreKernelCannyEdge::ExynosScoreKernelCannyEdge(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelCannyEdge::~ExynosScoreKernelCannyEdge(void)
{
}

vx_status
ExynosScoreKernelCannyEdge::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image = (vx_image)parameters[0];
    vx_threshold threshold = (vx_threshold)parameters[1];
    vx_scalar grad_scalar = (vx_scalar)parameters[2];
    vx_scalar norm_scalar = (vx_scalar)parameters[3];
    vx_image output_image = (vx_image)parameters[4];

    // input
    m_in_buf = new ScBuffer;
    status = getScImageParamInfo(input_image, m_in_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image info fails (status:%d)", status);
        goto EXIT;
    }

    // output
    m_out_buf = new ScBuffer;
    status = getScImageParamInfo(output_image, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output image info fails (status:%d)", status);
        goto EXIT;
    }

    // norm
    vx_enum norm_type;
    status = vxReadScalarValue(norm_scalar, &norm_type);
    if (status != VX_SUCCESS) {
        VXLOGE("getting norm info fails (status:%d)", status);
        goto EXIT;
    }
    switch (norm_type) {
    case VX_NORM_L1:
        m_norm = SC_POLICY_NORM_L1;
        break;
    case VX_NORM_L2:
        m_norm = SC_POLICY_NORM_L2;
        break;
    default:
        VXLOGW("invalid norm type (0x%x)", norm_type);
        m_norm = SC_POLICY_NORM_L1;
    }

    // th_low, th_high
    status = vxQueryThreshold(threshold, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_LOWER, &m_th_low, sizeof(vx_int32));
    status != vxQueryThreshold(threshold, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_UPPER, &m_th_high, sizeof(vx_int32));
    if (status != VX_SUCCESS) {
        VXLOGE("getting threshold info fails (status:%d)", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernelCannyEdge::updateScBuffers(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_int32 fd;

    // input image buffer
    status = getScVxObjectHandle(parameters[0], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_in_buf->host_buf.fd = fd;
    m_in_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

    // output image buffer
    status = getScVxObjectHandle(parameters[4], &fd, VX_WRITE_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_out_buf->host_buf.fd = fd;
    m_out_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

EXIT:
    return status;
}

vx_status
ExynosScoreKernelCannyEdge::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;

    sc_status = scvCannyEdgeDetector(
        m_in_buf, m_out_buf, m_norm, m_th_low, m_th_high);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvCannyEdgeDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf->host_buf.fd);
    status |= putScVxObjectHandle(parameters[4], m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    return status;
}

vx_status
ExynosScoreKernelCannyEdge::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_in_buf != NULL)
        delete m_in_buf;
    if (m_out_buf != NULL)
        delete m_out_buf;

    return status;
}

}; /* namespace android */

