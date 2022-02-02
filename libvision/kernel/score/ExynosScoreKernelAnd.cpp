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

#define LOG_TAG "ExynosScoreKernelAnd"
#include <cutils/log.h>

#include <score.h>
#include "ExynosScoreKernelAnd.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelAnd::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8)
                status = VX_SUCCESS;
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    } else if (index == 1) {
        vx_image images[2];
        vx_parameter param[2] = {
            vxGetParameterByIndex(node, 0),
            vxGetParameterByIndex(node, 1),
        };
        vxQueryParameter(param[0], VX_PARAMETER_ATTRIBUTE_REF, &images[0], sizeof(images[0]));
        vxQueryParameter(param[1], VX_PARAMETER_ATTRIBUTE_REF, &images[1], sizeof(images[1]));
        if (images[0] && images[1]) {
            vx_uint32 width[2], height[2];
            vx_df_image format[2];

            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_WIDTH, &width[0], sizeof(width[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_WIDTH, &width[1], sizeof(width[1]));
            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[0], sizeof(height[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_HEIGHT, &height[1], sizeof(height[1]));
            vxQueryImage(images[0], VX_IMAGE_ATTRIBUTE_FORMAT, &format[0], sizeof(format[0]));
            vxQueryImage(images[1], VX_IMAGE_ATTRIBUTE_FORMAT, &format[1], sizeof(format[1]));
            if (width[0] == width[1] && height[0] == height[1] && format[0] == format[1])
                status = VX_SUCCESS;
            vxReleaseImage(&images[1]);
            vxReleaseImage(&images[0]);
        }
        vxReleaseParameter(&param[0]);
        vxReleaseParameter(&param[1]);
    }

    return status;
}

vx_status
ExynosScoreKernelAnd::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 2) {
        vx_parameter param0 = vxGetParameterByIndex(node, 0);
        if (param0) {
            vx_image image0 = 0;
            vxQueryParameter(param0, VX_PARAMETER_ATTRIBUTE_REF, &image0, sizeof(image0));
            /*
             * When passing on the geometry to the output image, we only look at image 0, as
             * both input images are verified to match, at input validation.
             */
            if (image0) {
                vx_uint32 width = 0, height = 0;
                vxQueryImage(image0, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxQueryImage(image0, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                vx_df_image format = VX_DF_IMAGE_U8;
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
                vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

                status = VX_SUCCESS;
                vxReleaseImage(&image0);
            }
            vxReleaseParameter(&param0);
        }
    }

    return status;
}

ExynosScoreKernelAnd::ExynosScoreKernelAnd(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelAnd::~ExynosScoreKernelAnd(void)
{
}

vx_status
ExynosScoreKernelAnd::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image1 = (vx_image)parameters[0];
    vx_image input_image2 = (vx_image)parameters[1];
    vx_image output_image = (vx_image)parameters[2];


    m_in_buf1 = new ScBuffer;
    m_in_buf2 = new ScBuffer;
    m_out_buf = new ScBuffer;

    // input1
    status = getScImageParamInfo(input_image1, m_in_buf1);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image 1 info fails (status:%d)", status);
        goto EXIT;
    }

    // input2
    status = getScImageParamInfo(input_image2, m_in_buf2);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image 2 info fails (status:%d)", status);
        goto EXIT;
    }

    // output
    status = getScImageParamInfo(output_image, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output image info fails (status:%d)", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernelAnd::updateScBuffers(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_int32 fd;

    // input1 image buffer
    status = getScVxObjectHandle(parameters[0], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_in_buf1->host_buf.fd = fd;
    m_in_buf1->host_buf.memory_type = VS4L_MEMORY_DMABUF;

    // input2 image buffer
    status = getScVxObjectHandle(parameters[1], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_in_buf2->host_buf.fd = fd;
    m_in_buf2->host_buf.memory_type = VS4L_MEMORY_DMABUF;

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
ExynosScoreKernelAnd::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;

    sc_status = scvAnd(m_in_buf1, m_in_buf2, m_out_buf);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvAndDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf1->host_buf.fd);
    status |= putScVxObjectHandle(parameters[1], m_in_buf2->host_buf.fd);
    status |= putScVxObjectHandle(parameters[2], m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    return status;
}

vx_status
ExynosScoreKernelAnd::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_in_buf1 != NULL)
        delete m_in_buf1;
    if (m_in_buf2 != NULL)
        delete m_in_buf2;
    if (m_out_buf != NULL)
        delete m_out_buf;

    return status;
}

}; /* namespace android */

