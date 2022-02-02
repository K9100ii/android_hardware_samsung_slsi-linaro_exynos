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

#define LOG_TAG "ExynosScoreKernelHistogram"
#include <cutils/log.h>

#include <score.h>
#include "ExynosScoreKernelHistogram.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelHistogram::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8 || format == VX_DF_IMAGE_U16 ) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    return status;

}

vx_status
ExynosScoreKernelHistogram::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_image src = 0;
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        vx_parameter dst_param = vxGetParameterByIndex(node, 1);
        vx_distribution dist;

        vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
        vxQueryParameter(dst_param, VX_PARAMETER_ATTRIBUTE_REF, &dist, sizeof(dist));
        if ((src) && (dist)) {
            vx_uint32 width = 0, height = 0;
            vx_df_image format;
            vx_size numBins = 0;
            vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_BINS, &numBins, sizeof(numBins));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(height));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
            vxQueryImage(src, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));

            /* fill in the meta data with the attributes so that the checker will pass */

            status = VX_SUCCESS;
            vxReleaseDistribution(&dist);
            vxReleaseImage(&src);
        }
        vxReleaseParameter(&dst_param);
        vxReleaseParameter(&src_param);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("output vliadator fails, %p", meta);
    }

    return status;
}

ExynosScoreKernelHistogram::ExynosScoreKernelHistogram(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelHistogram::~ExynosScoreKernelHistogram(void)
{
}

vx_status
ExynosScoreKernelHistogram::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image = (vx_image)parameters[0];

    m_in_buf = new ScBuffer;
    m_out_buf = new ScBuffer;

    // input
    status = getScImageParamInfo(input_image, m_in_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image info fails (status:%d)", status);
        goto EXIT;
    }

    // output: full distribution (range:256)
    m_full_dist = vxCreateDistribution(vxGetContext(parameters[0]), 256, 0, 256);
    status = getScParamInfo((vx_reference)m_full_dist, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output info fails (status:%d)", status);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernelHistogram::updateScBuffers(const vx_reference *parameters)
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

    // output full distribution buffer
    status = getScVxObjectHandle((vx_reference)m_full_dist, &fd, VX_READ_AND_WRITE);
    if (status != VX_SUCCESS) {
        VXLOGE("getting vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_out_buf->host_buf.fd = fd;
    m_out_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

EXIT:
    return status;
}

vx_status
ExynosScoreKernelHistogram::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;

    sc_status = scvHistogram(m_in_buf, m_out_buf);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvHistogramDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf->host_buf.fd);
    status != putScVxObjectHandle((vx_reference)m_full_dist, m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    vx_distribution out_dist = (vx_distribution)parameters[1];
    vx_uint32 range;
    vx_uint32 window;
    vx_size bins;
    vx_size size;
    status = vxQueryDistribution(out_dist, VX_DISTRIBUTION_ATTRIBUTE_RANGE, &range, sizeof(range));
    status |= vxQueryDistribution(out_dist, VX_DISTRIBUTION_ATTRIBUTE_WINDOW, &window, sizeof(window));
    status |= vxQueryDistribution(out_dist, VX_DISTRIBUTION_ATTRIBUTE_BINS, &bins, sizeof(bins));
    status |= vxQueryDistribution(out_dist, VX_DISTRIBUTION_ATTRIBUTE_SIZE, &size, sizeof(size));
    if (status != VX_SUCCESS) {
        VXLOGE("querying distribution attribute fails");
    }

    vx_uint32 i, j;
    void *ptr1 = NULL, *ptr2 = NULL;
    vx_uint32 *full_dist_buf;
    vx_uint32 *out_dist_buf;
    status = vxAccessDistribution(m_full_dist, &ptr1, VX_READ_ONLY);
    status |= vxAccessDistribution(out_dist, &ptr2, VX_WRITE_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing distribution fails");
    }
    full_dist_buf = (vx_uint32*)ptr1;
    out_dist_buf = (vx_uint32*)ptr2;
    memset(ptr2, 0x0, size);
    for (i = 0, j = 0; i < range; i ++) {
        if(i != 0 && i % window == 0) {
            j++;
        }
        out_dist_buf[j] += full_dist_buf[i];
    }

    status = vxCommitDistribution(out_dist, out_dist_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting distribution fails");
    }
    return status;
}

vx_status
ExynosScoreKernelHistogram::destroy(void)
{
    vx_status status = VX_SUCCESS;

    vxReleaseDistribution(&m_full_dist);
    if (m_in_buf != NULL)
        delete m_in_buf;
    if (m_out_buf != NULL)
        delete m_out_buf;
    return status;
}

}; /* namespace android */

