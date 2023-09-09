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
 * \brief The Filter Kernels.
 * \author Erik Rainey <erik.rainey@gmail.com>
 */

#include <VX/vx.h>
#include <VX/vx_helper.h>
#include "vxcl_kernel_module.h"

static vx_status VX_CALLBACK vxGradientInputValidator(vx_node node, vx_uint32 index)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 0)
	{
		vx_image input = 0;
		vx_parameter param = vxGetParameterByIndex(node, index);

		vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
		if (input)
		{
			vx_uint32 width = 0, height = 0;
			vx_df_image format = 0;
			vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
			vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
			vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
			if (width >= 3 && height >= 3 && format == VX_DF_IMAGE_U8)
				status = VX_SUCCESS;
			vxReleaseImage(&input);
		}
		vxReleaseParameter(&param);
	}
	return status;
}

static vx_status VX_CALLBACK vxGradientOutputValidator(vx_node node, vx_uint32 index, vx_meta_format ptr)
{
	vx_status status = VX_ERROR_INVALID_PARAMETERS;
	if (index == 1 || index == 2)
	{
		vx_parameter param = vxGetParameterByIndex(node, 0); /* we reference the input image */
		if (param)
		{
			vx_image input = 0;
			vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
			if (input)
			{
				vx_uint32 width = 0, height = 0;
				vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
				vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

				vx_df_image meta_format = VX_DF_IMAGE_S16;
				vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
				vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
				vxSetMetaFormatAttribute(ptr, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));

				status = VX_SUCCESS;
				vxReleaseImage(&input);
			}
			vxReleaseParameter(&param);
		}
	}
	return status;
}

static vx_param_description_t gradient_kernel_params[] = {
	{ VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED },
	{ VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED },
	{ VX_OUTPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED },
};


vx_cl_kernel_description_t sobel3x3_clkernel = {
	{
		VX_KERNEL_SOBEL_3x3,
		"com.samsung.opencl.sobel_3x3",
		NULL,
		gradient_kernel_params, dimof(gradient_kernel_params),
		vxGradientInputValidator,
		vxGradientOutputValidator,
        NULL,
        NULL,
	},
	"/system/usr/vxcl/vx_sobel3x3.cl",
	"vx_sobel3x3",
	INIT_PROGRAMS,
	INIT_KERNELS,
	INIT_NUMKERNELS,
	INIT_RETURNS,
	NULL,
};
