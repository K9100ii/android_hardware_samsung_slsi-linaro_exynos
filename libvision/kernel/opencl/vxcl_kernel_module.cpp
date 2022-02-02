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

#define LOG_TAG "VxclKernelInterface"

#include <cutils/log.h>
#include <stdlib.h>

#include <VX/vx.h>
#include <VX/vx_internal.h>
#include <VX/vx_helper.h>
#include <VX/vx_khr_opencl.h>
#include <CL/cl.h>

#include "vxcl_kernel_module.h"

static vx_cl_context_t vxclContext;
static cl_context clContext[VX_CL_MAX_PLATFORMS];

/*! \brief Prototype for assigning to kernel */
static vx_cl_kernel_description_t *cl_kernels[] = {
    &warp_affine_clkernel,
    &warp_perspective_clkernel,
    &remap_clkernel,
    &warp_perspective_inverse_rgb_clkernel,
};

static vx_cl_kernel_description_t * vxcl_findkernel(const vx_char *name)
{
    vx_cl_kernel_description_t *vxclk = NULL;
    vx_uint32 k;
    for (k = 0; k < dimof(cl_kernels); k++) {
        if (strcmp(name, cl_kernels[k]->description.name) == 0) {
            vxclk = cl_kernels[k];
            break;
        }
    }
    if(vxclk) {
        VXLOGD2("found kernel, %s, %s", name, vxclk->description.name);
    } else {
        VXLOGE("failed to find kernel, %s", name);
    }

    return vxclk;
}

vx_status display_cl_info(void)
{
    vx_status status = VX_SUCCESS;

    /* Index: pidx(platform), didx(device), kidx(kernel) */
    vx_uint32 pidx, didx, kidx;
    /* error check for open cl functions */
    cl_int err = 0;
    /* to log platform, device info */
    vx_char infoString[256] = { 0, };

    for (pidx = 0; pidx < vxclContext.num_platforms; pidx++) {
        size_t size = 0;

        /* 1. cl platform information*/
        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_PROFILE, 0, NULL, &size);
        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_PROFILE, 256, &infoString[0], &size);
        VXLOGD3("[OCL] CL_PLATFORM_PROFILE : %s  \n", infoString);

        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_VERSION, 0, NULL, &size);
        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_VERSION, 256, &infoString[0], &size);
        VXLOGD3("[OCL] CL_PLATFORM_VERSION : %s  \n", infoString);

        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_NAME, 0, NULL, &size);
        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_NAME, 256, &infoString[0], &size);
        VXLOGD3("[OCL] CL_PLATFORM_NAME : %s  \n", infoString);

        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_VENDOR, 0, NULL, &size);
        clGetPlatformInfo(vxclContext.platform[pidx], CL_PLATFORM_VENDOR, 256, &infoString[0], &size);
        VXLOGD3("[OCL] CL_PLATFORM_VENDOR : %s  \n", infoString);

        for (didx = 0; didx < vxclContext.num_devices[pidx]; didx++) {
            vx_char deviceName[VX_CL_MAX_DEVICE_NAME];
            size_t infotable[3];
            cl_uint infoValue;
            cl_ulong infouValue;
            cl_bool compiler = CL_FALSE;
            cl_bool available = CL_FALSE;
            cl_bool image_support = CL_FALSE;

            /* 2. cl device information */
            err = clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DRIVER_VERSION, sizeof(infoString), infoString, NULL);
            VXLOGD3("[OCL] CL_DRIVER_VERSION : %s \n", infoString);

            err = clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_VENDOR, sizeof(infoString), infoString, NULL);
            VXLOGD3("[OCL] CL_DEVICE_VENDOR : %s \n", infoString);

            err = clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_PROFILE, sizeof(infoString), infoString, NULL);
            VXLOGD3("[OCL] CL_DEVICE_PROFILE : %s \n", infoString);

            err = clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_VERSION, sizeof(infoString), infoString, NULL);
            VXLOGD3("[OCL] CL_DEVICE_VERSION : %s \n", infoString);

            cl_device_type type;
            err = clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_TYPE, sizeof(type), &type, NULL);
            CL_ERROR_MSG(err, "clGetDeviceInfo");
            if (type == CL_DEVICE_TYPE_CPU) {
                VXLOGD3("[OCL] CL_DEVICE_TYPE : CL_DEVICE_TYPE_CPU\n");
            } else if (type == CL_DEVICE_TYPE_GPU) {
                VXLOGD3("[OCL] CL_DEVICE_TYPE : CL_DEVICE_TYPE_GPU\n");
            } else {
                VXLOGD3("[OCL] CL_DEVICE_TYPE : CL_DEVICE_TYPE_ACCELERATOR\n");
            }

            cl_ulong timer_resol;
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_PROFILING_TIMER_RESOLUTION, sizeof(cl_ulong ), &timer_resol, NULL);
            VXLOGD3("[OCL] CL_DEVICE_PROFILING_TIMER_RESOLUTION : %d \n", timer_resol);

            size_t extensionSize;
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_EXTENSIONS, 0, NULL, &extensionSize);
            vx_char *extensions = (vx_char *)malloc(extensionSize);
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_EXTENSIONS, extensionSize, extensions, NULL);
            VXLOGD3("[OCL] CL_DEVICE_EXTENSIONS : %s \n", extensions);
            free(extensions);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_COMPUTE_UNITS : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_WORK_GROUP_SIZE, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_WORK_GROUP_SIZE : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_WORK_ITEM_SIZES, sizeof(infotable), &infotable, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_WORK_ITEM_SIZES : %u %u %u \n", infotable[0], infotable[1], infotable[2]);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_CLOCK_FREQUENCY, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_CLOCK_FREQUENCY : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_READ_IMAGE_ARGS, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_READ_IMAGE_ARGS : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_WRITE_IMAGE_ARGS, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_WRITE_IMAGE_ARGS : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_MAX_MEM_ALLOC_SIZE, sizeof(infouValue), &infouValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_MAX_MEM_ALLOC_SIZE : %u \n", infouValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_IMAGE2D_MAX_WIDTH, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_IMAGE2D_MAX_WIDTH : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_IMAGE2D_MAX_HEIGHT, sizeof(infoValue), &infoValue, NULL);
            VXLOGD3("[OCL] CL_DEVICE_IMAGE2D_MAX_HEIGHT : %u \n", infoValue);

            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_NAME, sizeof(deviceName), deviceName, NULL);
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_COMPILER_AVAILABLE, sizeof(cl_bool), &compiler, NULL);
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_AVAILABLE, sizeof(cl_bool), &available, NULL);
            clGetDeviceInfo(vxclContext.devices[pidx][didx], CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &image_support, NULL);
            VXLOGD3("[OCL] Device %s (compiler=%s) (available=%s) (images=%s)\n", deviceName, (compiler?"TRUE":"FALSE"), (available?"TRUE":"FALSE"), (image_support?"TRUE":"FALSE"));
        }

        /* 3. check for supported formats */
        cl_uint fmtIdx,num_entries = 0u;
        cl_image_format *formats = NULL;
        cl_mem_flags flags = CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR;
        cl_mem_object_type type = CL_MEM_OBJECT_IMAGE2D;

        err = clGetSupportedImageFormats(clContext[pidx], flags, type, 0, NULL, &num_entries);
        formats = (cl_image_format *)malloc(num_entries * sizeof(cl_image_format));
        err = clGetSupportedImageFormats(clContext[pidx], flags, type, num_entries, formats, NULL);
        if (err != CL_SUCCESS) {
            CL_ERROR_MSG(err, "clGetSupportedImageFormats");
            break;
        }
        for (fmtIdx = 0; fmtIdx < num_entries; fmtIdx++) {
            char order[256];
            char datat[256];
            switch(formats[fmtIdx].image_channel_order) {
                CASE_STRINGERIZE2(CL_R, order);
                CASE_STRINGERIZE2(CL_A, order);
                CASE_STRINGERIZE2(CL_RG, order);
                CASE_STRINGERIZE2(CL_RA, order);
                CASE_STRINGERIZE2(CL_RGB, order);
                CASE_STRINGERIZE2(CL_RGBA, order);
                CASE_STRINGERIZE2(CL_BGRA, order);
                CASE_STRINGERIZE2(CL_ARGB, order);
                CASE_STRINGERIZE2(CL_INTENSITY, order);
                CASE_STRINGERIZE2(CL_LUMINANCE, order);
                CASE_STRINGERIZE2(CL_Rx, order);
                CASE_STRINGERIZE2(CL_RGx, order);
                CASE_STRINGERIZE2(CL_RGBx, order);
#if defined(CL_VERSION_1_2) && defined(cl_khr_gl_depth_images)
                CASE_STRINGERIZE2(CL_DEPTH, order);
                CASE_STRINGERIZE2(CL_DEPTH_STENCIL, order);
#endif
                default:
                    VXLOGD3("%x", formats[fmtIdx].image_channel_order);
                    break;
            }
            switch(formats[fmtIdx].image_channel_data_type) {
                CASE_STRINGERIZE2(CL_SNORM_INT8, datat);
                CASE_STRINGERIZE2(CL_SNORM_INT16, datat);
                CASE_STRINGERIZE2(CL_UNORM_INT8, datat);
                CASE_STRINGERIZE2(CL_UNORM_INT16, datat);
                CASE_STRINGERIZE2(CL_UNORM_SHORT_565, datat);
                CASE_STRINGERIZE2(CL_UNORM_SHORT_555, datat);
                CASE_STRINGERIZE2(CL_UNORM_INT_101010, datat);
                CASE_STRINGERIZE2(CL_SIGNED_INT8, datat);
                CASE_STRINGERIZE2(CL_SIGNED_INT16, datat);
                CASE_STRINGERIZE2(CL_SIGNED_INT32, datat);
                CASE_STRINGERIZE2(CL_UNSIGNED_INT8, datat);
                CASE_STRINGERIZE2(CL_UNSIGNED_INT16, datat);
                CASE_STRINGERIZE2(CL_UNSIGNED_INT32, datat);
                CASE_STRINGERIZE2(CL_HALF_FLOAT, datat);
                CASE_STRINGERIZE2(CL_FLOAT, datat);
#if defined(CL_VERSION_2_0)
                CASE_STRINGERIZE2(CL_UNORM_INT24, datat);
#endif
                default:
                    VXLOGD3("%x", formats[fmtIdx].image_channel_data_type);
                    break;
            }
            VXLOGD2("%s (0x%04x) : %s (0x%04x) \n", \
                order, formats[fmtIdx].image_channel_order, \
                datat, formats[fmtIdx].image_channel_data_type);
        }
    }

EXIT:
    if (err == CL_SUCCESS) {
        status = VX_SUCCESS;
    } else {
        status = VX_FAILURE;
    }

    EXYNOS_CL_KERNEL_IF_OUT();

    return status;

}

static vx_status kernelfunction(vx_node node, const vx_reference *parameters, vx_uint32 num)
{
    EXYNOS_CL_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    vx_uint32 pidx, pln, argidx;
    vx_uint32 plidx = 0, didx = 0;
    cl_int err = 0;
    size_t off_dim[3] = {0, 0, 0};
    size_t work_dim[3] = { 0, 0, 0 };

    cl_event writeEvents[VX_INT_MAX_PARAMS];
    cl_event readEvents[VX_INT_MAX_PARAMS];
    cl_int we = 0, re = 0;

    cl_event reference_event[VX_INT_MAX_PARAMS];
    cl_event node_event = NULL;

    cl_ulong ev_start_time = 0;
    cl_ulong ev_end_time = 0;
    size_t return_bytes;
    double run_time0 = 0;
    double run_time1 = 0;
    double run_time2 = 0;
    double queue_time1 = 0;
    uint64_t test_ms = getTimeMs();

    vxcl_mem_t* memory[VX_INT_MAX_PARAMS];

    vx_char *kernelname;
    vxQueryNode(node, VX_NODE_ATTRIBUTE_KERNEL_NAME, &kernelname, sizeof(vx_size));
    vx_cl_kernel_description_t *vxclk = vxcl_findkernel(kernelname);

    /* for each input/bi data object, enqueue it and set the kernel parameters */
    for (argidx = 0, pidx = 0; pidx < num; pidx++) {
        vx_enum dir = vxclk->description.parameters[pidx].direction;
        vx_enum type = vxclk->description.parameters[pidx].data_type;
        vxGetClMemoryInfo(parameters[pidx], type, clContext[0], &memory[pidx]);

        if(memory[pidx]) {
            for (pln = 0; pln < memory[pidx]->nptrs; pln++) {
                if (memory[pidx]->cl_type == CL_MEM_OBJECT_BUFFER) {
                    if (type == VX_TYPE_IMAGE) {
                        vx_image image = (vx_image)parameters[pidx];
                        work_dim[0] = (size_t)memory[pidx]->dims[pln][VX_CL_DIM_X];
                        work_dim[1] = (size_t)memory[pidx]->dims[pln][VX_CL_DIM_Y];
                        // width, height, stride_x, stride_y
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), &memory[pidx]->dims[pln][VX_CL_DIM_X]);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), &memory[pidx]->dims[pln][VX_CL_DIM_Y]);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), &memory[pidx]->strides[pln][VX_CL_DIM_X]);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), &memory[pidx]->strides[pln][VX_CL_DIM_Y]);
                        VXLOGD2("Setting vx_image as Buffer with 4 parameters(w:%u, h:%u, strid_x:%u, strid_y:%u)\n", \
                           memory[pidx]->dims[pln][VX_CL_DIM_X], memory[pidx]->dims[pln][VX_CL_DIM_Y], memory[pidx]->strides[pln][VX_CL_DIM_X], memory[pidx]->strides[pln][VX_CL_DIM_Y]);
                    } else if (type == VX_TYPE_ARRAY) {
                        vx_array arr = (vx_array)parameters[pidx];
                        // sizeof item, active count, capacity
                        vx_uint32 arr_item_size;
                        vx_uint32 arr_num_items;
                        vx_uint32 arr_capacity;
                        vx_uint32 arr_stride;
                        vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_ITEMSIZE, &arr_item_size, sizeof(vx_uint32));
                        vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_NUMITEMS, &arr_num_items, sizeof(vx_uint32));
                        vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_CAPACITY, &arr_capacity,  sizeof(vx_uint32));
                        vxQueryArray(arr, VX_ARRAY_ATTRIBUTE_STRIDE,   &arr_stride,    sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&arr_item_size);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&arr_num_items);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&arr_capacity);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&arr_stride);
                        VXLOGD2("Setting vx_array as Buffer with 4 parameters(itemsize:%u, numitem:%u, capa:%u, strid:%u)\n", \
                            arr_item_size, arr_num_items, arr_capacity, arr_stride);
                    } else if (type == VX_TYPE_LUT) {
                        vx_lut lut = (vx_lut)parameters[pidx];
                        // sizeof item, active count, capacity
                        vx_uint32 lut_item_size;
                        vx_uint32 lut_num_items;
                        vx_uint32 lut_capacity;
                        vx_uint32 lut_stride;
                        vxQueryLUT(lut, VX_ARRAY_ATTRIBUTE_ITEMSIZE, &lut_item_size, sizeof(vx_uint32));
                        vxQueryLUT(lut, VX_ARRAY_ATTRIBUTE_NUMITEMS, &lut_num_items, sizeof(vx_uint32));
                        vxQueryLUT(lut, VX_ARRAY_ATTRIBUTE_CAPACITY, &lut_capacity,  sizeof(vx_uint32));
                        vxQueryLUT(lut, VX_ARRAY_ATTRIBUTE_STRIDE,   &lut_stride,    sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&lut_item_size);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&lut_num_items);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&lut_capacity);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&lut_stride);
                        VXLOGD2("Setting vx_lut as Buffer with 4 parameters(itemsize:%u, numitem:%u, capa:%u, strid:%u)\n", \
                            lut_item_size, lut_num_items, lut_capacity, lut_stride);
                    } else if (type == VX_TYPE_MATRIX) {
                        vx_matrix mat = (vx_matrix)parameters[pidx];
                        // columns, rows
                        vx_uint32 mat_columns;
                        vx_uint32 mat_rows;
                        vxQueryMatrix(mat, VX_MATRIX_ATTRIBUTE_COLUMNS, &mat_columns, sizeof(vx_uint32));
                        vxQueryMatrix(mat, VX_MATRIX_ATTRIBUTE_ROWS,    &mat_rows,    sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&mat_columns);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&mat_rows);
                        VXLOGD2("Setting vx_matrix as Buffer with 2 parameters(columns:%u, rows:%u)\n", mat_columns, mat_rows);
                    } else if (type == VX_TYPE_DISTRIBUTION) {
                        vx_distribution dist = (vx_distribution)parameters[pidx];
                        // num, range, offset, winsize
                        vx_uint32 dist_bins;
                        vx_uint32 dist_range;
                        vx_uint32 dist_offset_x;
                        vx_uint32 dist_window_x;
                        vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_BINS,   &dist_bins,     sizeof(vx_uint32));
                        vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_RANGE,  &dist_range,    sizeof(vx_uint32));
                        vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_OFFSET, &dist_offset_x, sizeof(vx_uint32));
                        vxQueryDistribution(dist, VX_DISTRIBUTION_ATTRIBUTE_WINDOW, &dist_window_x, sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&dist_bins);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&dist_range);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&dist_offset_x);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&dist_window_x);
                        VXLOGD2("Setting vx_distribution as Buffer with 4 parameters(bins:%u, range:%u, offset:%u, winsize:%u)\n", \
						    dist_bins, dist_range, dist_offset_x, dist_window_x);
                    } else if (type == VX_TYPE_CONVOLUTION) {
                        vx_convolution conv = (vx_convolution)parameters[pidx];
                        // columns, rows, scale
                        vx_uint32 conv_columns;
                        vx_uint32 conv_rows;
                        vx_uint32 conv_scale;
                        vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_COLUMNS, &conv_columns, sizeof(vx_uint32));
                        vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_ROWS,    &conv_rows,    sizeof(vx_uint32));
                        vxQueryConvolution(conv, VX_CONVOLUTION_ATTRIBUTE_SCALE,   &conv_scale,   sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&conv_columns);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&conv_rows);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&conv_scale);
                        VXLOGD2("Setting vx_matrix as Buffer with 2 parameters(columns:%u, rows:%u, scale: %u)\n", conv_columns, conv_rows, conv_scale);
                    } else if (type == VX_TYPE_REMAP) {
                        vx_remap remap = (vx_remap)parameters[pidx];
                        vx_uint32 remap_src_width;
                        vx_uint32 remap_src_height;
                        vx_uint32 remap_dst_width;
                        vx_uint32 remap_dst_height;
                        vxQueryRemap(remap, VX_REMAP_ATTRIBUTE_SOURCE_WIDTH,       &remap_src_width,  sizeof(vx_uint32));
                        vxQueryRemap(remap, VX_REMAP_ATTRIBUTE_SOURCE_HEIGHT,      &remap_src_height, sizeof(vx_uint32));
                        vxQueryRemap(remap, VX_REMAP_ATTRIBUTE_DESTINATION_WIDTH,  &remap_dst_width,  sizeof(vx_uint32));
                        vxQueryRemap(remap, VX_REMAP_ATTRIBUTE_DESTINATION_HEIGHT, &remap_dst_height, sizeof(vx_uint32));
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&remap_src_width);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&remap_src_height);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&remap_dst_width);
                        err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_uint32), (vx_uint32 *)&remap_dst_height);
                        VXLOGD2("Setting vx_matrix as Buffer with 2 parameters(src:%u x %u, dst:%u x %u)\n", \
						    remap_src_width, remap_src_height, remap_dst_width, remap_dst_height);
                        /* set the work dimensions */
                        work_dim[0] = remap_dst_width;
                        work_dim[1] = remap_dst_height;
                    }

                    err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(cl_mem), &memory[pidx]->hdls[pln]);
                    CL_ERROR_MSG(err, "clSetKernelArg_buffer_hdls");
                    if (dir == VX_INPUT || dir == VX_BIDIRECTIONAL) {
                        VXLOGD3("clEnqueWriteBuffer");
                        err = clEnqueueWriteBuffer(vxclContext.queues[plidx][didx],
                                                   memory[pidx]->hdls[pln],
                                                   CL_TRUE,
                                                   0,
                                                   memory[pidx]->sizes[pln],
                                                   memory[pidx]->ptrs[pln],
                                                   0,
                                                   NULL,
                                                   &reference_event[pidx]);
                         CL_ERROR_MSG(err, "clEnqueueWriteBuffer");
                    }
                } else if (memory[pidx]->cl_type == CL_MEM_OBJECT_IMAGE2D) {
                    vx_rectangle_t rect = {0, 0, 0, 0};
                    vx_image image = (vx_image)parameters[pidx];
                    vxGetValidRegionImage(image, &rect);
                    size_t origin[3] = {rect.start_x, rect.start_y, 0};
                    size_t region[3] = {rect.end_x-rect.start_x, rect.end_y-rect.start_y, 1};
                    /* set the work dimensions */
                    work_dim[0] = rect.end_x-rect.start_x;
                    work_dim[1] = rect.end_y-rect.start_y;
                    VXLOGD2("Setting vx_image as image2d_t wd={%u,%u} arg:%u\n",work_dim[0], work_dim[1], argidx);
                    err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(cl_mem), &memory[pidx]->hdls[pln]);
                    CL_ERROR_MSG(err, "clSetKernelArg_image_hdls");
                    if (err != CL_SUCCESS) {
                        VXLOGE("Error Calling Kernel %s, parameter %u\n", vxclk->kernelname, pidx);
                    }
                    if (dir == VX_INPUT || dir == VX_BIDIRECTIONAL) {
                        err = clFinish(vxclContext.queues[plidx][didx]);
                        VXLOGD3("clEnqueWriteImage");
                        err = clEnqueueWriteImage(vxclContext.queues[plidx][didx],
                                                  memory[pidx]->hdls[pln],
                                                  CL_TRUE,
                                                  origin, region,
                                                  memory[pidx]->strides[pln][VX_CL_DIM_Y],
                                                  0,
                                                  memory[pidx]->ptrs[pln],
                                                  0, NULL,
                                                  &reference_event[pidx]);
                        CL_ERROR_MSG(err, "clEnqueueWriteImage");
                        err = clFinish(vxclContext.queues[plidx][didx]);
                        err = clGetEventProfilingInfo(reference_event[pidx], CL_PROFILING_COMMAND_START, sizeof(cl_ulong),
                            &ev_start_time, &return_bytes);
                        err = clGetEventProfilingInfo(reference_event[pidx], CL_PROFILING_COMMAND_END, sizeof(cl_ulong),
                            &ev_end_time, &return_bytes);
                        run_time0 = (double)(ev_end_time - ev_start_time);

                    }
                }
            }
            if (dir == VX_INPUT || dir == VX_BIDIRECTIONAL) {
                memcpy(&writeEvents[we++], &reference_event[pidx], sizeof(cl_event));
            }
        } else {
            if (type == VX_TYPE_SCALAR) {
                vx_scalar sc = (vx_scalar)parameters[pidx];
                vx_size value; // largest platform atomic
                vx_size size = 0ul;
                vx_enum stype = VX_TYPE_INVALID;
                vxReadScalarValue(sc, &value);
                vxQueryScalar(sc, VX_SCALAR_ATTRIBUTE_TYPE, &stype, sizeof(stype));

                size = vxSizeOfType(stype);
                if (size == 0ul) {
                    status = VX_ERROR_INVALID_TYPE;
                    VXLOGE("type for scalar is invalid");
                    break;
                }
                err = clSetKernelArg(vxclk->kernels[plidx], argidx++, size, &value);
                VXLOGD2("Setting vx_scalar as Buffer with 1 parameter(stype: %u)\n", stype);
            } else if (type == VX_TYPE_THRESHOLD) {
                vx_threshold th = (vx_threshold)parameters[pidx];
                vx_enum ttype = 0;
                vxQueryThreshold(th, VX_THRESHOLD_ATTRIBUTE_TYPE, &ttype, sizeof(ttype));

                if (ttype == VX_THRESHOLD_TYPE_BINARY) {
                    vx_int32 th_value;
                    vxQueryThreshold(th, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_VALUE, &th_value, sizeof(vx_int32));
                    err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), (vx_int32 *)&th_value);
                    VXLOGD2("Setting vx_threshold(binary) as Buffer with 1 parameter(th_value:%u)\n", th_value);
                } else if (ttype == VX_THRESHOLD_TYPE_RANGE) {
                    vx_int32 th_lower;
                    vx_int32 th_upper;
                    vxQueryThreshold(th, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_LOWER, &th_lower, sizeof(vx_int32));
                    vxQueryThreshold(th, VX_THRESHOLD_ATTRIBUTE_THRESHOLD_UPPER, &th_upper, sizeof(vx_int32));
                    err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), (vx_int32 *)&th_lower);
                    err = clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), (vx_int32 *)&th_upper);
                    VXLOGD2("Setting vx_threshold(range) as Buffer with 2 parameters(th_lower:%u, th_upper: %u)\n", th_lower, th_upper);
                }
            }
        }
    }

    vx_border_mode_t border;
    vxQueryNode(node, VX_NODE_ATTRIBUTE_BORDER_MODE, &border, sizeof(vx_border_mode_t));
    VXLOGD3("Border: mode = 0x%x, value = %d", border.mode, border.constant_value);
    clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), (vx_int32 *)&border.mode);
    clSetKernelArg(vxclk->kernels[plidx], argidx++, sizeof(vx_int32), (vx_int32 *)&border.constant_value);

    err = clFinish(vxclContext.queues[plidx][didx]);
    uint64_t test_ms2 = getTimeMs();
    VXLOGD3("clEnqueueNDRangeKernel");
    err = clEnqueueNDRangeKernel(vxclContext.queues[plidx][didx],  // command_queue
                                 vxclk->kernels[plidx],            // kernel
                                 2,                                // work_dim
                                 off_dim,                          // global_work_offset
                                 work_dim,                         // global_work_size
                                 NULL,                             // local_work_size
                                 we,
                                 writeEvents,
                                 &node_event);
    CL_ERROR_MSG(err, "clEnqueueNDRangeKernel");
    err = clFinish(vxclContext.queues[plidx][didx]);
    uint64_t test_ms3 = getTimeMs();
    ev_start_time = 0;
    ev_end_time = 0;
    err = clGetEventProfilingInfo(node_event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong),
        &ev_start_time, &return_bytes);
    err = clGetEventProfilingInfo(node_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong),
        &ev_end_time, &return_bytes);
    run_time1 = (double)(ev_end_time - ev_start_time);

    /* enqueue a read on all output data */
    for (pidx = 0; pidx < num; pidx++) {
        vx_enum dir = vxclk->description.parameters[pidx].direction;
        if (dir == VX_OUTPUT || dir == VX_BIDIRECTIONAL) {
            vx_enum type = vxclk->description.parameters[pidx].data_type;
            if(memory[pidx]->allocated) {
                for (pln = 0; pln < memory[pidx]->nptrs; pln++) {
                    if (memory[pidx]->cl_type == CL_MEM_OBJECT_BUFFER) {
                        err = clEnqueueReadBuffer(vxclContext.queues[plidx][didx],
                                memory[pidx]->hdls[pln],
                                CL_TRUE, 0, memory[pidx]->sizes[pln],
                                memory[pidx]->ptrs[pln],
                                1,
                                &node_event,
                                &reference_event[pidx]);
                        CL_ERROR_MSG(err, "clEnqueueReadBuffer");
                    } else if (memory[pidx]->cl_type == CL_MEM_OBJECT_IMAGE2D) {
                        vx_rectangle_t rect = {0, 0, 0, 0};
                        vx_image image = (vx_image)parameters[pidx];
                        vxGetValidRegionImage(image, &rect);
                        size_t origin[3] = {rect.start_x, rect.start_y, 0};
                        size_t region[3] = {rect.end_x-rect.start_x, rect.end_y-rect.start_y, 1};
                        /* set the work dimensions */
                        work_dim[0] = rect.end_x-rect.start_x;
                        work_dim[1] = rect.end_y-rect.start_y;

                        err = clFinish(vxclContext.queues[plidx][didx]);
                        VXLOGD3("clEnqueueReadImage");
                        err = clEnqueueReadImage(vxclContext.queues[plidx][didx],
                                memory[pidx]->hdls[pln],
                                CL_TRUE,
                                origin, region,
                                memory[pidx]->strides[pln][VX_CL_DIM_Y],
                                0,
                                memory[pidx]->ptrs[pln],
                                1,
                                &node_event,
                                &reference_event[pidx]);
                        CL_ERROR_MSG(err, "clEnqueueReadImage");
                        err = clFinish(vxclContext.queues[plidx][didx]);
                        err = clGetEventProfilingInfo(reference_event[pidx], CL_PROFILING_COMMAND_START, sizeof(cl_ulong),
                            &ev_start_time, &return_bytes);
                        err = clGetEventProfilingInfo(reference_event[pidx], CL_PROFILING_COMMAND_END, sizeof(cl_ulong),
                            &ev_end_time, &return_bytes);
                        run_time2 = (double)(ev_end_time - ev_start_time);

                        VXLOGD3("Reading Image wd={%u,%u}.\n", work_dim[0], work_dim[1]);
                    }
                }
                memcpy(&readEvents[re++], &reference_event[pidx], sizeof(cl_event));
            }
        }
    }

    err = clFlush(vxclContext.queues[plidx][didx]);
    CL_ERROR_MSG(err, "Flush");
    VXLOGD2("Waiting for read events!\n");
    err = clWaitForEvents(re, readEvents);

    for (vx_int32 e = 0; e < we; e++) {
        err = clReleaseEvent(writeEvents[e]);
        CL_ERROR_MSG(err ,"clReleaseEvent_we");
    }
    for (vx_int32 e = 0; e < re; e++) {
        err = clReleaseEvent(readEvents[e]);
        CL_ERROR_MSG(err ,"clReleaseEvent_re");
    }
    err = clGetEventProfilingInfo(node_event, CL_PROFILING_COMMAND_QUEUED, sizeof(cl_ulong), &ev_start_time, &return_bytes);
    err = clGetEventProfilingInfo(node_event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &ev_end_time, &return_bytes);
    queue_time1 = (double)(ev_end_time - ev_start_time);
    run_time0 = run_time0*1.0e-6; //ns -> ms
    run_time1 = run_time1*1.0e-6; //ns -> ms
    run_time2 = run_time2*1.0e-6; //ns -> ms

    queue_time1 = queue_time1*1.0e-6; //ns -> ms

    VXLOGD1("Kernel Time : %8.4f %8.4f %8.4f (ms)\n",run_time0, run_time1, run_time2);
    VXLOGD1("NDR Queue   : %8.4f (ms)\n",queue_time1);
    VXLOGD1("[1]NDR gettimeofday() : %llu(ms)\n",test_ms3-test_ms2);

    if (err == CL_SUCCESS)
        status = VX_SUCCESS;

    VXLOGD1("%s exiting %s, %d\n", __FUNCTION__, vxclk->description.name, status);

    EXYNOS_CL_KERNEL_IF_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxModuleInitializer(vx_context context)
{
    EXYNOS_CL_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;

    /* Index: pidx(platform), didx(device), kidx(kernel) */
    vx_uint32 pidx, didx, kidx;
    /* error check for open cl functions */
    cl_int err = 0;
    /* to log platform, device info */
    vx_char infoString[256] = { 0, };

    if (vxGetStatus((vx_reference)context) != VX_SUCCESS) {
        status = VX_ERROR_INVALID_REFERENCE;
        goto EXIT;
    }

    vxclContext.num_platforms = VX_CL_MAX_PLATFORMS;
    err = clGetPlatformIDs(VX_CL_MAX_PLATFORMS, vxclContext.platform, NULL);
    if (err != CL_SUCCESS) {
        CL_ERROR_MSG(err, "clGetPlatformIDs");
        goto EXIT;
    }

    for (pidx = 0; pidx < vxclContext.num_platforms; pidx++) {
        size_t size = 0;

        err = clGetDeviceIDs(vxclContext.platform[pidx], CL_DEVICE_TYPE_ALL, 0, NULL, &vxclContext.num_devices[pidx]);
        err = clGetDeviceIDs(vxclContext.platform[pidx], CL_DEVICE_TYPE_ALL,
            vxclContext.num_devices[pidx] > VX_CL_MAX_DEVICES ? VX_CL_MAX_DEVICES : vxclContext.num_devices[pidx],
            vxclContext.devices[pidx], NULL);
        if (err != CL_SUCCESS) {
            CL_ERROR_MSG(err, "clGetDeviceIDs");
            break;
        }

        cl_context_properties props[] = {
			(cl_context_properties) CL_PRINTF_BUFFERSIZE_ARM, (cl_context_properties) 0x100000,
            (cl_context_properties) CL_CONTEXT_PLATFORM, (cl_context_properties) vxclContext.platform[pidx],
            (cl_context_properties) 0,
        };

        clContext[pidx] = clCreateContext(props,
                                           vxclContext.num_devices[pidx],
                                           vxclContext.devices[pidx],
                                           NULL,
                                           NULL,
                                           &err);
        if (err != CL_SUCCESS) {
            CL_ERROR_MSG(err, "Failed to create cl context");
            break;
        } else {
            VXLOGD3("Created cl context (platform: %d)", pidx);
        }

        /* create a queue for each device */
        for (didx = 0; didx < vxclContext.num_devices[pidx]; didx++) {
            vxclContext.queues[pidx][didx] = clCreateCommandQueue(clContext[pidx],
                                                      vxclContext.devices[pidx][didx],
                                                      CL_QUEUE_PROFILING_ENABLE,
                                                      &err);
            if (err == CL_SUCCESS) {
                VXLOGD3("CL Device (%d) comand queue created", didx);
            } else {
                CL_ERROR_MSG(err, "clCreateCommandQueue");
                break;
            }
        }
    }

EXIT:
    if (err == CL_SUCCESS) {
        status = VX_SUCCESS;
    } else {
        status = VX_FAILURE;
    }

    EXYNOS_CL_KERNEL_IF_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxModuleDeinitializer(void)
{
    EXYNOS_CL_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    vx_uint32 kidx, didx, pidx;
    cl_int err= 0;

    for (pidx = 0; pidx < vxclContext.num_platforms; pidx++) {
        for (kidx = 0; kidx < dimof(cl_kernels); kidx++) {
            err = clReleaseKernel(cl_kernels[kidx]->kernels[pidx]);
            CL_ERROR_MSG(err, "clReleaseKernel");
            err = clReleaseProgram(cl_kernels[kidx]->program[pidx]);
            CL_ERROR_MSG(err, "clReleaseProgram");
        }

        for (didx = 0; didx < vxclContext.num_devices[pidx]; didx++) {
            clReleaseCommandQueue(vxclContext.queues[pidx][didx]);
            CL_ERROR_MSG(err, "clReleaseCommandQueue");
        }
        clReleaseContext(clContext[pidx]);
        CL_ERROR_MSG(err, "clReleaseContext");
    }

    EXYNOS_CL_KERNEL_IF_OUT();
    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxPublishKernels(vx_context context)
{
    EXYNOS_CL_KERNEL_IF_IN();
    vx_status status = VX_SUCCESS;
    char cl_args[1024] = "-D VX_CL_KERNEL -D ARM_MALI -D CL_USE_IMAGES -I /system/usr/vxcl/include/";
    /* Index: pidx(platform) kidx(kernel) */
    vx_uint32 pidx, kidx;
    /* error check for open cl functions */
    cl_int err = 0;

    for (pidx = 0; pidx < vxclContext.num_platforms; pidx++) {
        /* for each kernel */
        for (kidx = 0; kidx < dimof(cl_kernels); kidx++) {
            char *sources = NULL;
            size_t programSze = 0;

            /* load the source file */
            VXLOGD2("Kernel[%u] File: %s\n", kidx, cl_kernels[kidx]->sourcepath);
            VXLOGD2("Kernel[%u] Name: %s\n", kidx, cl_kernels[kidx]->kernelname);
            VXLOGD2("Kernel[%u] ID: %s\n", kidx, cl_kernels[kidx]->description.name);
            sources = clLoadSources(cl_kernels[kidx]->sourcepath, &programSze);
            if (sources == NULL) {
                VXLOGE("Failed to load source (kernel: %s)", cl_kernels[kidx]->kernelname);
                continue;
            }
            /* create a program with this source */
            cl_kernels[kidx]->program[pidx] = clCreateProgramWithSource(clContext[pidx],
                                                                         1,
                                                                         (const char **)&sources,
                                                                         &programSze,
                                                                         &err);
            if (err != CL_SUCCESS) {
                CL_ERROR_MSG(err, "clCreateProgramWithSource");
            } else {
                err = clBuildProgram((cl_program)cl_kernels[kidx]->program[pidx],
                                    1,
                                    (const cl_device_id *)vxclContext.devices,
                                    (const char *)cl_args,
                                    NULL,
                                    NULL);
                if (err != CL_SUCCESS) {
                    CL_BUILD_MSG(err, cl_kernels[kidx]->kernelname);
                    if ((err == CL_BUILD_PROGRAM_FAILURE) || (err == CL_INVALID_BUILD_OPTIONS)) {
                        char log[10][1024];
                        size_t logSize = 0;
                        clGetProgramBuildInfo((cl_program)cl_kernels[kidx]->program[pidx],
                                                (cl_device_id)vxclContext.devices[pidx][0],
                                                CL_PROGRAM_BUILD_LOG,
                                                sizeof(log),
                                                log,
                                                &logSize);
                        VXLOGD2("%s", log);
                    }
                } else {
                    vx_kernel kernel;
                    cl_build_status bstatus = 0;
                    size_t bs = 0;
                    err = clGetProgramBuildInfo(cl_kernels[kidx]->program[pidx],
                                                vxclContext.devices[pidx][0],
                                                CL_PROGRAM_BUILD_STATUS,
                                                sizeof(cl_build_status),
                                                &bstatus,
                                                &bs);
                    VXLOGD2("Status = %d (%d)\n", bstatus, err);
                    /* get the cl_kernels from the program */
                    cl_kernels[kidx]->num_kernels[pidx] = 1;
                    err = clCreateKernelsInProgram(cl_kernels[kidx]->program[pidx],
                                                   1,
                                                   &cl_kernels[kidx]->kernels[pidx],
                                                   NULL);
                    CL_ERROR_MSG(err, "clCreateKernelsInProgram");
                    VXLOGD2("Found cl_kernel %s in %s (%d)\n", cl_kernels[kidx]->kernelname, cl_kernels[kidx]->sourcepath, err);

                    vx_kernel_f kfunc = cl_kernels[kidx]->description.function;
                    if (kfunc == NULL)
                        kfunc = (vx_kernel_f) kernelfunction;
                    kernel = vxAddKernel(context,
                                cl_kernels[kidx]->description.name,
                                cl_kernels[kidx]->description.enumeration,
                                kfunc,
                                cl_kernels[kidx]->description.numParams,
                                cl_kernels[kidx]->description.input_validate,
                                cl_kernels[kidx]->description.output_validate,
                                cl_kernels[kidx]->description.initialize,
                                cl_kernels[kidx]->description.deinitialize);
                    if (kernel) {
                        vx_uint32 paramIdx;
                        status = VX_SUCCESS; // temporary
                        for (paramIdx = 0; paramIdx < cl_kernels[kidx]->description.numParams; paramIdx++) {
                            status = vxAddParameterToKernel(kernel, paramIdx,
                                                            cl_kernels[kidx]->description.parameters[paramIdx].direction,
                                                            cl_kernels[kidx]->description.parameters[paramIdx].data_type,
                                                            cl_kernels[kidx]->description.parameters[paramIdx].state);
                            if (status != VX_SUCCESS) {
                                VXLOGE("Failed to add parameter %d to kernel %s! (%d)\n", pidx, cl_kernels[kidx]->kernelname, status);
                                break;
                            }
                        }
                        if (status == VX_SUCCESS) {
                            status = vxFinalizeKernel(kernel);
                            if (status != VX_SUCCESS) {
                                VXLOGE("Failed to finalize kernel[%u]=%s\n",kidx, cl_kernels[kidx]->kernelname);
                            } else {
                                VXLOGD2("Finalized kernel[%u]=%s", kidx, cl_kernels[kidx]->kernelname);
                            }
                        } else {
                            status = vxRemoveKernel(kernel);
                            if (status != VX_SUCCESS) {
                                VXLOGE("Failed to remove kernel[%u]=%s\n",kidx, cl_kernels[kidx]->kernelname);
                            }
                        }
                        /*! \todo should i continue with errors or fail and unwind? */
                    } else {
                        VXLOGE("Failed to add kernel %s\n", cl_kernels[kidx]->kernelname);
                    }
                }
            }
        }
    }

    EXYNOS_CL_KERNEL_IF_OUT();
    return status;
}

