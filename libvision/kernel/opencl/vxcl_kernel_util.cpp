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

#define LOG_TAG "VxclKernelUtil"
#include <stdio.h>
#include <stdlib.h>

#include "vxcl_kernel_util.h"

cl_int clPrintError(cl_int err, const char *label, const char *function, const char *file, int line)
{
    switch (err)
    {
        case CL_SUCCESS :
            return err;
        CASE_STRINGERIZE(CL_BUILD_PROGRAM_FAILURE, label, function, file, line);
        CASE_STRINGERIZE(CL_COMPILER_NOT_AVAILABLE, label, function, file, line);
        CASE_STRINGERIZE(CL_DEVICE_NOT_AVAILABLE, label, function, file, line);
        CASE_STRINGERIZE(CL_DEVICE_NOT_FOUND, label, function, file, line);
        CASE_STRINGERIZE(CL_IMAGE_FORMAT_MISMATCH, label, function, file, line);
        CASE_STRINGERIZE(CL_IMAGE_FORMAT_NOT_SUPPORTED, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_ARG_INDEX, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_ARG_SIZE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_ARG_VALUE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_BINARY, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_BUFFER_SIZE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_BUILD_OPTIONS, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_COMMAND_QUEUE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_CONTEXT, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_DEVICE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_DEVICE_TYPE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_EVENT, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_EVENT_WAIT_LIST, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_GL_OBJECT, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_GLOBAL_OFFSET, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_HOST_PTR, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_IMAGE_FORMAT_DESCRIPTOR, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_IMAGE_SIZE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_KERNEL_NAME, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_KERNEL, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_KERNEL_ARGS, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_KERNEL_DEFINITION, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_MEM_OBJECT, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_OPERATION, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_PLATFORM, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_PROGRAM, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_PROGRAM_EXECUTABLE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_QUEUE_PROPERTIES, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_SAMPLER, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_VALUE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_WORK_DIMENSION, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_WORK_GROUP_SIZE, label, function, file, line);
        CASE_STRINGERIZE(CL_INVALID_WORK_ITEM_SIZE, label, function, file, line);
        CASE_STRINGERIZE(CL_MAP_FAILURE, label, function, file, line);
        CASE_STRINGERIZE(CL_MEM_OBJECT_ALLOCATION_FAILURE, label, function, file, line);
        CASE_STRINGERIZE(CL_MEM_COPY_OVERLAP, label, function, file, line);
        CASE_STRINGERIZE(CL_OUT_OF_HOST_MEMORY, label, function, file, line);
        CASE_STRINGERIZE(CL_OUT_OF_RESOURCES, label, function, file, line);
        CASE_STRINGERIZE(CL_PROFILING_INFO_NOT_AVAILABLE, label, function, file, line);
        default:
            VXLOGE("%s: Unknown error %d at %s in %s:%d", label, err, function, file, line);
            break;
    }
    return err;
}


cl_int clBuildError(cl_int build_status, const char *label, const char *function, const char *file, int line)
{
    switch (build_status)
    {
        case CL_BUILD_SUCCESS:
            break;
        CASE_STRINGERIZE(CL_BUILD_NONE, label, function, file, line);
        CASE_STRINGERIZE(CL_BUILD_ERROR, label, function, file, line);
        CASE_STRINGERIZE(CL_BUILD_IN_PROGRESS, label, function, file, line);
        default:
            VXLOGE("%s: Unknown build error %d at %s in %s:%d", label, build_status, function, file, line);
            break;
    }
    return build_status;
}
char *clLoadSources(char *filename, size_t *programSize)
{
    char *programSource = NULL;

    FILE *pFile = NULL;
    VXLOGD2("Reading source file %s", filename);
    pFile = fopen((char *)filename, "rb");
    if (pFile != NULL && programSize) {
        // obtain file size:
        fseek(pFile, 0, SEEK_END);
        *programSize = ftell(pFile);
        rewind(pFile);

        int size = *programSize + 1;
        programSource = (char*)malloc(sizeof(char)*(size));
        if (programSource == NULL) {
            fclose(pFile);
            free(programSource);
            return NULL;
        }

        fread(programSource, sizeof(char), *programSize, pFile);
        programSource[*programSize] = '\0';
        fclose(pFile);
    } else {
        VXLOGE("Faild to read source file %s", filename);
    }

    return programSource;
}

uint64_t getTimeMs()
{
    struct timeval  m_Time;
    gettimeofday(&m_Time, NULL);

    return (m_Time.tv_sec) * 1000LL + (m_Time.tv_usec) / 1000LL;
}

uint64_t getTimeUs()
{
    struct timeval  m_Time;
    gettimeofday(&m_Time, NULL);

    return (m_Time.tv_sec) * 1000000LL + (m_Time.tv_usec);
}

