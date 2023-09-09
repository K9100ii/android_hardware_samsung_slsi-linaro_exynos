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
 * \brief The Min and Maximum Locations Kernel.
 * \author Erik Rainey <erik.rainey@gmail.com>
 */

#include <VX/vx.h>
#include <VX/vx_helper.h>
#include <vxcl_kernel_module.h>

static vx_status VX_CALLBACK vxMinMaxLocInputValidator(vx_node node, vx_uint32 index)
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
            if ((format == VX_DF_IMAGE_U8)
                || (format == VX_DF_IMAGE_S16)
#if defined(EXPERIMENTAL_USE_S16)
                || (format == VX_DF_IMAGE_U16)
                || (format == VX_DF_IMAGE_U32)
                || (format == VX_DF_IMAGE_S32)
#endif
                )
            {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }
    return status;
}

static vx_status VX_CALLBACK vxMinMaxLocOutputValidator(vx_node node, vx_uint32 index, vx_meta_format ptr)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    if ((index == 1) || (index == 2))
    {
        vx_parameter param = vxGetParameterByIndex(node, 0);
        if (param)
        {
            vx_image input = 0;
            vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
            if (input)
            {
                vx_df_image format;
                vx_enum type = VX_TYPE_INVALID;
                vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
                switch (format)
                {
                    case VX_DF_IMAGE_U8:
                        type = VX_TYPE_UINT8;
                        break;
                    case VX_DF_IMAGE_U16:
                        type = VX_TYPE_UINT16;
                        break;
                    case VX_DF_IMAGE_U32:
                        type = VX_TYPE_UINT32;
                        break;
                    case VX_DF_IMAGE_S16:
                        type = VX_TYPE_INT16;
                        break;
                    case VX_DF_IMAGE_S32:
                        type = VX_TYPE_INT32;
                        break;
                    default:
                        type = VX_TYPE_INVALID;
                        break;
                }
                if (type != VX_TYPE_INVALID)
                {
                    status = VX_SUCCESS;
                    vxSetMetaFormatAttribute(ptr, VX_SCALAR_ATTRIBUTE_TYPE, &type, sizeof(type));
                }
                else
                {
                    status = VX_ERROR_INVALID_TYPE;
                }
                vxReleaseImage(&input);
            }
            vxReleaseParameter(&param);
        }
    }
    if ((index == 3) || (index == 4))
    {
        /* nothing to check here */
        vx_enum array_item_type = VX_TYPE_COORDINATES2D;
        vx_size array_capacity = 1;
        vxSetMetaFormatAttribute(ptr, VX_ARRAY_ATTRIBUTE_ITEMTYPE, &array_item_type, sizeof(array_item_type));
        vxSetMetaFormatAttribute(ptr, VX_ARRAY_ATTRIBUTE_CAPACITY, &array_capacity, sizeof(array_capacity));

        status = VX_SUCCESS;
    }
    if ((index == 5) || (index == 6))
    {
        vx_enum scalar_type = VX_TYPE_UINT32;
        vxSetMetaFormatAttribute(ptr, VX_SCALAR_ATTRIBUTE_TYPE, &scalar_type, sizeof(scalar_type));

        status = VX_SUCCESS;
    }
    return status;
}

static vx_param_description_t minmaxloc_kernel_params[] = {
    {VX_INPUT, VX_TYPE_IMAGE, VX_PARAMETER_STATE_REQUIRED},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_ARRAY, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL},
    {VX_OUTPUT, VX_TYPE_SCALAR, VX_PARAMETER_STATE_OPTIONAL},
};


vx_cl_kernel_description_t minmaxloc_clkernel = {
	{
		VX_KERNEL_MINMAXLOC,
		"com.samsung.opencl.minmaxloc",
		NULL,
		minmaxloc_kernel_params, dimof(minmaxloc_kernel_params),
		vxMinMaxLocInputValidator,
		vxMinMaxLocOutputValidator,
        NULL,
        NULL,
	},
	"/system/usr/vxcl/vx_minmaxloc.cl",
	"vx_minmaxloc",
	INIT_PROGRAMS,
	INIT_KERNELS,
	INIT_NUMKERNELS,
	INIT_RETURNS,
	NULL,
};
