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

#define LOG_TAG "ExynosScoreKernelLut"
#include <cutils/log.h>

#include <score.h>
#include "ExynosScoreKernelLut.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelLut::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8 || format == VX_DF_IMAGE_S16) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);

    } else if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        vx_lut lut = 0;
        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &lut, sizeof(lut));
        if (lut) {
            vx_enum type = 0;
            vxQueryLUT(lut, VX_LUT_ATTRIBUTE_TYPE, &type, sizeof(type));
            if (type == VX_TYPE_UINT8 || type == VX_TYPE_INT16) {
                status = VX_SUCCESS;
            }
            vxReleaseLUT(&lut);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosScoreKernelLut::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 2) {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        if (src_param) {
            vx_image src = 0;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            if (src) {
                vx_uint32 width = 0, height = 0;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                /* output is equal type and size */
                vx_df_image format = VX_DF_IMAGE_U8;
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

ExynosScoreKernelLut::ExynosScoreKernelLut(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelLut::~ExynosScoreKernelLut(void)
{
}

vx_status
ExynosScoreKernelLut::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image = (vx_image)parameters[0];
    vx_lut lut = (vx_lut)parameters[1];
    vx_image output_image = (vx_image)parameters[2];

    // input
    m_in_buf = new ScBuffer;
    status = getScImageParamInfo(input_image, m_in_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image info fails (status:%d)", status);
        goto EXIT;
    }

    // lut
    m_lut_buf = new ScBuffer;
    status = getScParamInfo((vx_reference)lut, m_lut_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting lut info fails (status:%d)", status);
        goto EXIT;
    }

    // output
    m_out_buf = new ScBuffer;
    status = getScImageParamInfo(output_image, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output image info fails (status:%d)", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernelLut::updateScBuffers(const vx_reference *parameters)
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

    // lut buffer
    status = getScVxObjectHandle(parameters[1], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_lut_buf->host_buf.fd = fd;
    m_lut_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

    // output image buffer
    status = getScVxObjectHandle(parameters[2], &fd, VX_WRITE_ONLY);
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
ExynosScoreKernelLut::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;

    sc_status = scvTableLookup(
        m_in_buf, m_lut_buf, m_out_buf);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvLutDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf->host_buf.fd);
    status = putScVxObjectHandle(parameters[1], m_lut_buf->host_buf.fd);
    status |= putScVxObjectHandle(parameters[2], m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    return status;
}

vx_status
ExynosScoreKernelLut::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_in_buf != NULL)
        delete m_in_buf;
    if (m_lut_buf != NULL)
        delete m_lut_buf;
    if (m_out_buf != NULL)
        delete m_out_buf;

    return status;
}

}; /* namespace android */

