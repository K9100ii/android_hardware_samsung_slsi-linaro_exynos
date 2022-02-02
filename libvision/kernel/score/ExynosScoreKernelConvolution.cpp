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

#define LOG_TAG "ExynosScoreKernelConvolution"
#include <cutils/log.h>

#include <score.h>
#include "memory/score_ion_memory_manager.h"
#include "ExynosScoreKernelConvolution.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelConvolution::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vx_uint32 width = 0, height = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if ((width > VX_INT_MAX_CONVOLUTION_DIM) &&
                (height > VX_INT_MAX_CONVOLUTION_DIM) &&
                ((format == VX_DF_IMAGE_U8) || (format == VX_DF_IMAGE_S16))) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_convolution conv = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &conv, sizeof(conv));
        if (conv) {
            vx_df_image dims[2] = {0,0};
            vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_COLUMNS, &dims[0], sizeof(dims[0]));
            vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_ROWS, &dims[1], sizeof(dims[1]));
            if ((dims[0] <= VX_INT_MAX_CONVOLUTION_DIM) &&
                (dims[1] <= VX_INT_MAX_CONVOLUTION_DIM)) {
                status = VX_SUCCESS;
            }
            vxReleaseConvolution(&conv);
        }
        vxReleaseParameter(&param);
    }

    return status;
}

vx_status
ExynosScoreKernelConvolution::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 2) {
        vx_parameter params[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, index),
        };

        if (params[0] && params[1]) {
            vx_image input = 0;
            vx_image output = 0;
            vxQueryParameter(params[0], VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            vxQueryParameter(params[1], VX_PARAMETER_ATTRIBUTE_REF, &output, sizeof(output));
            if (input && output) {
                vx_uint32 width = 0, height = 0;
                vx_df_image format = 0;
                vx_df_image output_format = 0;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
                vxQueryImage(output, VX_IMAGE_ATTRIBUTE_FORMAT, &output_format, sizeof(output_format));

                vx_df_image meta_format = output_format == VX_DF_IMAGE_U8 ? VX_DF_IMAGE_U8 : VX_DF_IMAGE_S16;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;

                vxReleaseImage(&input);
                vxReleaseImage(&output);
            }
            vxReleaseParameter(&params[0]);
            vxReleaseParameter(&params[1]);
        }
    }

    return status;
}

ExynosScoreKernelConvolution::ExynosScoreKernelConvolution(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelConvolution::~ExynosScoreKernelConvolution(void)
{
}

vx_status
ExynosScoreKernelConvolution::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image = (vx_image)parameters[0];
    vx_convolution conv = (vx_convolution)parameters[1];
    vx_image output_image = (vx_image)parameters[2];

    // input
    m_in_buf = new ScBuffer;
    status = getScImageParamInfo(input_image, m_in_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image info fails (status:%d)", status);
        goto EXIT;
    }

    // conv
    m_conv_buf = new ScBuffer;
    status = getScParamInfo((vx_reference)conv, m_conv_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input convolution info fails (status:%d)", status);
        goto EXIT;
    }

    // output
    m_out_buf = new ScBuffer;
    status = getScImageParamInfo(output_image, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output image info fails (status:%d)", status);
        goto EXIT;
    }

    // kernel_size
    vx_size rows;
    status = vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_ROWS, &rows, sizeof(rows));
    if (status != VX_SUCCESS) {
        VXLOGE("querying convolution fails");
        goto EXIT;
    }
    m_kernel_size = rows;

    // norm
     m_norm = 1;

EXIT:
    return status;
}

vx_status
ExynosScoreKernelConvolution::updateScBuffers(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_int32 fd;
    intptr_t temp_addr;
    sc_s16 *addr;

    // input image buffer
    status = getScVxObjectHandle(parameters[0], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_in_buf->host_buf.fd = fd;
    m_in_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

    // convolution coefficient buffer
    m_conv_buf->host_buf.fd = ScoreIonMemoryManager::Instance()->
                                  AllocScoreMemory(m_kernel_size * m_kernel_size * sizeof(sc_s16));
    m_conv_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;
    temp_addr = (intptr_t)ScoreIonMemoryManager::Instance()->GetVaddrFromFd(m_conv_buf->host_buf.fd);
    addr = (sc_s16 *)temp_addr;
    status = vxReadConvolutionCoefficients((vx_convolution)parameters[1], addr);

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
ExynosScoreKernelConvolution::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;

    sc_status = scvConvolution(
        m_in_buf, m_conv_buf, m_out_buf, m_kernel_size, m_norm);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvConvolutionDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf->host_buf.fd);
    status |= putScVxObjectHandle(parameters[2], m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    return status;
}

vx_status
ExynosScoreKernelConvolution::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_in_buf != NULL)
        delete m_in_buf;
    if (m_out_buf != NULL)
        delete m_out_buf;
    if (m_conv_buf != NULL) {
        ScoreIonMemoryManager::Instance()->FreeScoreMemory(m_conv_buf->host_buf.fd);
        delete m_conv_buf;
    }

    return status;
}

}; /* namespace android */

