/*
 * Copyright (c) 2012-2014 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and/or associated documentation files (the
 * "Materials"), to deal in the Materials without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Materials, and to
 * permit persons to whom the Materials are furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * MATERIALS OR THE USE OR OTHER DEALINGS IN THE MATERIALS.
 */

/*!
 * \file
 * \brief The Remap Kernel
 * \author Erik Rainey <erik.rainey@gmail.com>
 */

#include <VX/vx.h>
#include <VX/vx_helper.h>
#include <vxcl_kernel_module.h>

static vx_status VX_CALLBACK vxRemapInputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0)
    {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input)
        {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8)
            {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }
    else if (index == 1)
    {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param)
        {
            vx_remap table;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &table, sizeof(table));
            if (table)
            {
                /* \todo what are we checking? */
                status = VX_SUCCESS;
                vxReleaseRemap(&table);
            }
            vxReleaseParameter(&param);
        }
    }
    else if (index == 2)
    {
        vx_parameter param = vxGetParameterByIndex(node, index);
        if (param)
        {
            vx_scalar scalar = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &scalar, sizeof(scalar));
            if (scalar)
            {
                vx_enum stype = 0;
                vxQueryScalar(scalar, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));
                if (stype == VX_TYPE_ENUM)
                {
                    vx_enum policy = 0;
                    vxReadScalarValue(scalar, &policy);
                    if ((policy == VX_INTERPOLATION_TYPE_NEAREST_NEIGHBOR) ||
                        (policy == VX_INTERPOLATION_TYPE_BILINEAR))
                    {
                        status = VX_SUCCESS;
                    }
                    else
                    {
                        status = VX_ERROR_INVALID_VALUE;
                    }
                }
                else
                {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseScalar(&scalar);
            }
            vxReleaseParameter(&param);
        }
    }
    return status;
}

static vx_status VX_CALLBACK vxRemapOutputValidator(vx_node node, vx_uint32 index, vx_meta_format ptr)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 3)
    {
        vx_parameter src_param = vxGetParameterByIndex(node, 0);
        vx_parameter tbl_param = vxGetParameterByIndex(node, 1);
        vx_parameter dst_param = vxGetParameterByIndex(node, index);
        if (src_param && dst_param && tbl_param)
        {
            vx_image src = 0;
            vx_image dst = 0;
            vx_remap tbl = 0;
            vxQueryParameter(src_param, VX_PARAMETER_ATTRIBUTE_REF, &src, sizeof(src));
            vxQueryParameter(dst_param, VX_PARAMETER_ATTRIBUTE_REF, &dst, sizeof(dst));
            vxQueryParameter(tbl_param, VX_PARAMETER_ATTRIBUTE_REF, &tbl, sizeof(tbl));
            if ((src) && (dst) && (tbl))
            {
                vx_uint32 w1 = 0, h1 = 0;
                vx_uint32 w2 = 0, h2 = 0;
                vx_uint32 w3 = 0, h3 = 0;

                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_WIDTH, &w1, sizeof(w1));
                vxQueryImage(src, VX_IMAGE_ATTRIBUTE_HEIGHT, &h1, sizeof(h1));
                vxQueryRemap(tbl, VX_REMAP_ATTRIBUTE_SOURCE_WIDTH, &w2, sizeof(w2));
                vxQueryRemap(tbl, VX_REMAP_ATTRIBUTE_SOURCE_HEIGHT, &h2, sizeof(h2));
                vxQueryRemap(tbl, VX_REMAP_ATTRIBUTE_DESTINATION_WIDTH, &w3, sizeof(w3));
                vxQueryRemap(tbl, VX_REMAP_ATTRIBUTE_DESTINATION_HEIGHT, &h3, sizeof(h3));

                if ((w1 == w2) && (h1 == h2))
                {
                    /* fill in the meta data with the attributes so that the checker will pass */
                    vx_df_image meta_format = VX_DF_IMAGE_U8;
                    vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
                    vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_WIDTH, &w3, sizeof(w3));
                    vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_HEIGHT, &h3, sizeof(h3));

                    status = VX_SUCCESS;
                }
                vxReleaseImage(&src);
                vxReleaseRemap(&tbl);
                vxReleaseImage(&dst);
            }
            vxReleaseParameter(&src_param);
            vxReleaseParameter(&tbl_param);
            vxReleaseParameter(&dst_param);
        }
    }
    return status;
}

static vx_param_description_t remap_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_REMAP, VX_PARAMETER_STATE_REQUIRED},
    {VX_INPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
};

vx_cl_kernel_description_t remap_clkernel = {
	{
		VX_KERNEL_REMAP,
		"com.samsung.opencl.remap",
		NULL,
		remap_kernel_params, dimof(remap_kernel_params),
		vxRemapInputValidator,
		vxRemapOutputValidator,
        NULL,
        NULL,
	},
	"/system/usr/vxcl/vx_remap.cl",
	"vx_remap",
	INIT_PROGRAMS,
	INIT_KERNELS,
	INIT_NUMKERNELS,
	INIT_RETURNS,
	NULL,
};
