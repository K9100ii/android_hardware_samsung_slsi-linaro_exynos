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

#define LOG_TAG "ExynosScoreKernelFastCorners"
#include <cutils/log.h>
#include <score.h>
#include "ExynosScoreKernelFastCorners.h"

namespace android {

using namespace std;
using namespace score;

vx_status
ExynosScoreKernelFastCorners::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_image input = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if ((status == VX_SUCCESS) && (input)) {
                vx_df_image format = 0;
                status = vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                if ((status == VX_SUCCESS) && (format == VX_DF_IMAGE_U8)) {
                    status = VX_SUCCESS;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    }
    if (index == 1) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar sens = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &sens, sizeof(sens));
            if ((status == VX_SUCCESS) && (sens)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(sens, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_FLOAT32) {
                    vx_float32 k = 0.0f;
                    status = vxReadScalarValue(sens, &k);
                    if ((status == VX_SUCCESS) && (k > 0) && (k < 256)) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&sens);
            }
            vxReleaseParameter(&param);
        }
    }
    if (index == 2) {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param) {
            vx_scalar s_nonmax = 0;
            status = vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &s_nonmax, sizeof(s_nonmax));
            if ((status == VX_SUCCESS) && (s_nonmax)) {
                vx_enum type = VX_TYPE_INVALID;
                vxQueryScalar(s_nonmax, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                if (type == VX_TYPE_BOOL) {
                    vx_bool nonmax = vx_false_e;
                    status = vxReadScalarValue(s_nonmax, &nonmax);
                    if ((status == VX_SUCCESS) && ((nonmax == vx_false_e) || (nonmax == vx_true_e))) {
                        status = VX_SUCCESS;
                    } else {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                } else {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&s_nonmax);
            }
            vxReleaseParameter(&param);
        }
    }

    return status;

}

vx_status
ExynosScoreKernelFastCorners::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    if (!node) {
        VXLOGE("node is null");
    }

    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 3) {
        vx_enum array_item_type = VX_TYPE_KEYPOINT;
        vx_size array_capacity = 0;
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(meta, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    } else if (index == 4) {
        vx_enum scalar_type = VX_TYPE_SIZE;
        vxSetMetaFormatAttribute(meta, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }

    return status;
}

ExynosScoreKernelFastCorners::ExynosScoreKernelFastCorners(vx_char *name, vx_uint32 param_num)
                                       : ExynosScoreKernel(name, param_num)
{
}

ExynosScoreKernelFastCorners::~ExynosScoreKernelFastCorners(void)
{
}

vx_status
ExynosScoreKernelFastCorners::updateScParamFromVx(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    if (!node) {
        VXLOGE("invalid node, %p, %p", node, parameters);
    }
    /* update score param from vx param */
    vx_image input_image = (vx_image)parameters[0];
    vx_scalar str_thresh = (vx_scalar)parameters[1];
    vx_scalar nonmax = (vx_scalar)parameters[2];
    vx_array corners = (vx_array)parameters[3];
    vx_scalar num_corners = (vx_scalar)parameters[4];

    m_in_buf = new ScBuffer;
    m_out_buf = new ScBuffer;

    // input image
    status = getScImageParamInfo(input_image, m_in_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting input image  info fails (status:%d)", status);
        goto EXIT;
    }

    // output image
    m_out_image = vxCreateImage(vxGetContext(parameters[0]), m_in_buf->buf.width, m_in_buf->buf.height, VX_DF_IMAGE_U16);
    status = getScImageParamInfo(m_out_image, m_out_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("getting output image  info fails (status:%d)", status);
        goto EXIT;
    }

    // threshold
    vx_float32 threshold;
    status = vxReadScalarValue(str_thresh, &threshold);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar fails (status:%d)", status);
        goto EXIT;
    }
    m_threshold = (sc_s32)threshold;

    // corner policy
    m_corner_policy = SC_POLICY_CORNERS_9;

    // nms policy
    vx_bool is_nms;
    status = vxReadScalarValue(nonmax, &is_nms);
    if (status != VX_SUCCESS) {
        VXLOGE("reading scalar, err:%d", status);
        goto EXIT;
    }
    if (is_nms == vx_true_e) {
        m_nms_policy = SC_POLICY_NMS_USE;
    } else {
        m_nms_policy = SC_POLICY_NMS_NO_USE;
    }

EXIT:
    return status;
}

vx_status
ExynosScoreKernelFastCorners::updateScBuffers(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_int32 fd;

    vx_rectangle_t rect = {0, 0, m_out_buf->buf.width, m_out_buf->buf.height};
    vx_imagepatch_addressing_t addr;
    void *ptr = NULL;
    vx_uint16 *image_addr;

    // input image buffer
    status = getScVxObjectHandle(parameters[0], &fd, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("bring vx object handle fails (status:%d)", status);
        goto EXIT;
    }
    m_in_buf->host_buf.fd = fd;
    m_in_buf->host_buf.memory_type = VS4L_MEMORY_DMABUF;

    // output image buffer
    status = getScVxObjectHandle((vx_reference)m_out_image, &fd, VX_WRITE_ONLY);
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
ExynosScoreKernelFastCorners::runScvKernel(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;
    sc_status_t sc_status = STS_SUCCESS;
    sc_status = scvFastCorners(m_in_buf, m_out_buf, m_threshold, m_corner_policy, m_nms_policy);
    if(sc_status != STS_SUCCESS) {
        VXLOGE("Failed in processing scvFastCornersDetector. (status: %d)", sc_status);
        status = VX_FAILURE;
    }

    status = putScVxObjectHandle(parameters[0], m_in_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    status = putScVxObjectHandle((vx_reference)m_out_image, m_out_buf->host_buf.fd);
    if (status != VX_SUCCESS) {
        VXLOGE("putting handle of the vx object fails");
    }

    /* convert output from score image to vx format */
    vx_array vx_array_object = (vx_array)parameters[3];
    vx_scalar num_corners_scalar = (vx_scalar)parameters[4];;

    vx_size capacity;
    status = vxQueryArray(vx_array_object, VX_ARRAY_ATTRIBUTE_CAPACITY, &capacity, sizeof(capacity));
    if (status != NO_ERROR) {
        VXLOGE("querying array fails");
    }

    status = vxAddEmptyArrayItems(vx_array_object, capacity);
    if (status != NO_ERROR) {
        VXLOGE("adding empty array fails");
    }

    vx_keypoint_t *keypoint_array = NULL;;
    vx_size array_stride = 0;;
    status = vxAccessArrayRange(vx_array_object, 0, capacity, &array_stride, (void**)&keypoint_array, VX_WRITE_ONLY);
    if (status != NO_ERROR) {
        VXLOGE("accessing array fails");
    }
    if (array_stride != sizeof(vx_keypoint_t)) {
        VXLOGE("stride is not matching, %d, %d", array_stride, sizeof(vx_keypoint_t));
        status = VX_FAILURE;
    }

    vx_rectangle_t rect = {0, 0, m_out_buf->buf.width, m_out_buf->buf.height};
    vx_imagepatch_addressing_t addr;
    void *ptr = NULL;
    vx_uint16 *image_addr;
    status = vxAccessImagePatch(m_out_image, &rect, 0, &addr, &ptr, VX_READ_ONLY);
    image_addr = (vx_uint16*)ptr;

    vx_size found_point_num = 0;
    vx_uint32 x, y;
    vx_uint32 width = m_out_buf->buf.width;
    vx_uint32 height = m_out_buf->buf.height;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            if (image_addr[y*width+x] != 0) {
                keypoint_array[found_point_num].x = x;
                keypoint_array[found_point_num].y = y;
                keypoint_array[found_point_num].strength = image_addr[y*width+x];
                keypoint_array[found_point_num].scale = 0;
                keypoint_array[found_point_num].orientation = 0;
                keypoint_array[found_point_num].tracking_status = 1;
                keypoint_array[found_point_num].error = 0;
                found_point_num++;
            }
        }
    }

    status = vxCommitArrayRange(vx_array_object, 0, capacity, keypoint_array);
    if (status != NO_ERROR) {
        VXLOGE("commit array fails");
    }

    if (found_point_num != capacity)
        vxTruncateArray(vx_array_object, found_point_num);

    if (num_corners_scalar) {
        status = vxWriteScalarValue(num_corners_scalar, &found_point_num);
        if (status != VX_SUCCESS) {
            VXLOGE("writing scalar value fails");
        }
    }
    return status;
}

vx_status
ExynosScoreKernelFastCorners::destroy(void)
{
    vx_status status = VX_SUCCESS;

    if (m_in_buf != NULL)
        delete m_in_buf;
    if (m_out_buf != NULL)
        delete m_out_buf;
    if (m_out_image != NULL)
        vxReleaseImage(&m_out_image);

    return status;
}

}; /* namespace android */

