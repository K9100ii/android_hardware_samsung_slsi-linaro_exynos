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
 * \brief The Mean and Standard Deviation Kernel
 * \author Erik Rainey <erik.rainey@gmail.com>
 */

#include <VX/vx.h>
#include <VX/vx_helper.h>
#include <vxcl_kernel_module.h>

static vx_status VX_CALLBACK vxMeanStdDevInputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 0)
    {
        vx_parameter param = vxGetParameterByIndex(node, index);
        vx_image image = 0;
        vx_df_image format = 0;

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &image, sizeof(image));
        if (image == 0)
        {
            status = VX_ERROR_INVALID_PARAMETERS;
        }
        else
        {
            status = VX_SUCCESS;
            vxQueryImage(image, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format != VX_DF_IMAGE_U8
#if defined(EXPERIMENTAL_USE_S16)
                && format != VX_DF_IMAGE_U16
#endif
                )
            {
                status = VX_ERROR_INVALID_PARAMETERS;
            }
            vxReleaseImage(&image);
        }
        vxReleaseParameter(&param);
    }
    return status;
}

static vx_status VX_CALLBACK vxMeanStdDevOutputValidator(vx_node node, vx_uint32 index, vx_meta_format ptr)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if (index == 1 || index == 2)
    {
        vx_enum scalar_type = VX_TYPE_FLOAT32;
        vxSetMetaFormatAttribute(ptr, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }

    return status;
}


/*! \brief Declares the parameter types for \ref vxuColorConvert.
 * \ingroup group_implementation
 */
static vx_param_description_t mean_stddev_kernel_params[] = {
    {VX_INPUT,  VX_TYPE_IMAGE,   VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_REQUIRED},
};


vx_cl_kernel_description_t mean_stddev_clkernel = {
	{
		VX_KERNEL_MEAN_STDDEV,
		"com.samsung.opencl.mean_stddev",
		NULL,
		mean_stddev_kernel_params, dimof(mean_stddev_kernel_params),
		vxMeanStdDevInputValidator,
		vxMeanStdDevOutputValidator,
        NULL,
        NULL,
	},
	"/system/usr/vxcl/vx_meanstddev.cl",
	"vx_meanstddev",
	INIT_PROGRAMS,
	INIT_KERNELS,
	INIT_NUMKERNELS,
	INIT_RETURNS,
	NULL,
};
