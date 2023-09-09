/*
 * Copyright (c) 2011-2014 The Khronos Group Inc.
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

#ifndef _VXCL_KERNEL_UTIL_H_
#define _VXCL_KERNEL_UTIL_H_

#include <features.h>
#include <sys/types.h>
#include <dirent.h>
#include <fnmatch.h>

#include <VX/vx.h>
#include <VX/vx_helper.h>
#include <CL/cl.h>

#include <ExynosVisionCommonConfig.h>

//#define EXYNOS_CL_KERNEL_IF_TRACE
#ifdef EXYNOS_CL_KERNEL_IF_TRACE
#define EXYNOS_CL_KERNEL_IF_IN()   VXLOGD("IN...")
#define EXYNOS_CL_KERNEL_IF_OUT()  VXLOGD("OUT..")
#else
#define EXYNOS_CL_KERNEL_IF_IN()   ((void *)0)
#define EXYNOS_CL_KERNEL_IF_OUT()  ((void *)0)
#endif

/*! \brief The maximum number of platforms */
#define VX_CL_MAX_PLATFORMS (1)

/*! \brief The maximum number of CL devices in the system */
#define VX_CL_MAX_DEVICES     (2)
#define VX_CL_MAX_DEVICE_NAME (64)

/*! \brief The maximum number of characters on a line of OpenCL source code */
#define VX_CL_MAX_LINE_WIDTH (160)

/*! \brief The maximum path name */
#define VX_CL_MAX_PATH (256)

#ifndef VX_CL_ARGS
#define VX_CL_ARGS "-I."
#endif

/* Define a printf callback function. */
#define CL_PRINTF_CALLBACK_ARM    0x40B0
#define CL_PRINTF_BUFFERSIZE_ARM  0x40B1

#define INIT_PROGRAMS {0}
#define INIT_KERNELS  {0}
#define INIT_NUMKERNELS {0}
#define INIT_RETURNS  {{0,0}}

typedef struct _vx_type_size {
    vx_enum type;
    vx_size size;
} vx_type_size_t;

typedef struct _vx_cl_context_t {
    cl_uint               num_platforms;
    cl_uint               num_devices[VX_CL_MAX_PLATFORMS];
    cl_platform_id        platform[VX_CL_MAX_PLATFORMS];
    cl_device_id          devices[VX_CL_MAX_PLATFORMS][VX_CL_MAX_DEVICES];
    cl_context            context[VX_CL_MAX_PLATFORMS];
    cl_context_properties context_props;
    cl_command_queue      queues[VX_CL_MAX_PLATFORMS][VX_CL_MAX_DEVICES];
    struct _vx_cl_kernel_description_t **kernels;
    vx_uint32             num_kernels;
} vx_cl_context_t;

typedef struct _vx_cl_kernel_description_t {
    vx_kernel_description_t description;
    char             sourcepath[VX_CL_MAX_PATH];
    char             kernelname[VX_MAX_KERNEL_NAME];
    cl_program       program[VX_CL_MAX_PLATFORMS];
    cl_kernel        kernels[VX_CL_MAX_PLATFORMS];
    cl_uint          num_kernels[VX_CL_MAX_PLATFORMS];
    cl_int           returns[VX_CL_MAX_PLATFORMS][VX_CL_MAX_DEVICES];
    void            *reserved; /* for additional data */
} vx_cl_kernel_description_t;

#define EXPERIMENTAL_USE_FNMATCH

#define CL_MAX_LINESIZE (1024)

#define ALLOC(type,count)                               (type *)calloc(count, sizeof(type))
#define CL_ERROR_MSG(err, string)                       clPrintError(err, string, __FUNCTION__, __FILE__, __LINE__)
#define CL_BUILD_MSG(err, string)                       clBuildError(err, string, __FUNCTION__, __FILE__, __LINE__)

#define CASE_STRINGERIZE(err, label, function, file, line) \
    case err: \
        VXLOGE("%s: OpenCL error "#err" at %s in %s:%d\n", label, function, file, line); \
        break

#define CASE_STRINGERIZE2(value, string) case value: strcpy(string, #value); break

#ifdef __cplusplus
extern "C" {
#endif

char *clLoadSources(char *filename, size_t *programSize);
cl_int clBuildError(cl_int build_status, const char *label, const char *function, const char *file, int line);
cl_int clPrintError(cl_int err, const char *label, const char *function, const char *file, int line);

uint64_t getTimeMs(void);
uint64_t getTimeUs(void);

#ifdef __cplusplus
}
#endif

#endif
