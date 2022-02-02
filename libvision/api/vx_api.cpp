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

#define LOG_TAG "ExynosEvfInterface"
#include <cutils/log.h>

#include <VX/vx.h>
#include <VX/vx_api.h>

#include "ExynosVisionContext.h"
#include "ExynosVisionGraph.h"
#include "ExynosVisionNode.h"
#include "ExynosVisionParameter.h"

#include "ExynosVisionImage.h"
#include "ExynosVisionArray.h"
#include "ExynosVisionMatrix.h"
#include "ExynosVisionDistribution.h"
#include "ExynosVisionLut.h"
#include "ExynosVisionConvolution.h"
#include "ExynosVisionThreshold.h"
#include "ExynosVisionScalar.h"
#include "ExynosVisionPyramid.h"
#include "ExynosVisionRemap.h"
#include "ExynosVisionDelay.h"

using namespace android;

VX_API_ENTRY vx_status VX_API_CALL vxGetStatus(vx_reference ref)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionReference *cRef = (ExynosVisionReference*)ref;

    status = cRef->getStatus();

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryReference(vx_reference ref, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionReference *cReference = (ExynosVisionReference*)ref;

    status = cReference->queryReference(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query reference fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxHint(vx_reference ref, vx_enum hint)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionReference *cReference = (ExynosVisionReference*)ref;

    status = cReference->hint(hint);
    if (status != VX_SUCCESS) {
        VXLOGE("hint reference fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxDirective(vx_reference ref, vx_enum directive)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionReference *cReference = (ExynosVisionReference*)ref;

    status = cReference->directive(directive);
    if (status != VX_SUCCESS) {
        VXLOGE("directive reference fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_context VX_API_CALL vxGetContext(vx_reference reference)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)reference) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", reference);
        return NULL;
    }

    ExynosVisionContext *cContext;

    ExynosVisionReference *cReference = (ExynosVisionReference*)reference;
    cContext = cReference->getContext();

    EXYNOS_VISION_API_OUT();

    return (vx_context)cContext;
}

VX_API_ENTRY vx_context VX_API_CALL vxCreateContext(void)
{
    EXYNOS_VISION_API_IN();

    vx_status status = VX_FAILURE;

    ExynosVisionContext *cContext;

    cContext = new ExynosVisionContext();
    status = cContext->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("context object creation fail(%d)", status);
        delete cContext;
        return NULL;
    }
    cContext->incrementReference(VX_REF_EXTERNAL);

    status = cContext->init();
    if (status != VX_SUCCESS) {
        VXLOGE("context init fail(%d)", status);
        delete cContext;
        cContext = NULL;
    }

    EXYNOS_VISION_API_OUT();

    return (vx_context)cContext;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryContext(vx_context context, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }
    vx_status status;

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;

    status = cContext->queryContext(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying context fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseContext(vx_context *context)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)*context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }
    vx_status status = VX_FAILURE;

    ExynosVisionContext *cContext = (ExynosVisionContext*)*context;
    status = cContext->destroy();
    if (status == VX_SUCCESS) {
        delete cContext;
        *context = NULL;
    } else {
        VXLOGE("destroying context fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetContextAttribute(vx_context context, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;

    status = cContext->setContextAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting context attribute fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_enum VX_API_CALL vxRegisterUserStruct(vx_context context, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_enum type = VX_TYPE_INVALID;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;

    type = cContext->registerUserStruct(size);

    EXYNOS_VISION_API_OUT();

    return type;
}

VX_API_ENTRY vx_status VX_API_CALL vxLoadKernels(vx_context context, const vx_char *module)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    status = cContext->loadKernels(module);
    if (status != VX_SUCCESS) {
        VXLOGE("loading kernels fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_kernel VX_API_CALL vxAddKernel(vx_context context,
                             const vx_char name[VX_MAX_KERNEL_NAME],
                             vx_enum enumeration,
                             vx_kernel_f func_ptr,
                             vx_uint32 numParams,
                             vx_kernel_input_validate_f input,
                             vx_kernel_output_validate_f output,
                             vx_kernel_initialize_f init,
                             vx_kernel_deinitialize_f deinit)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionKernel *kernel = NULL;

    kernel = cContext->addKernel(name, enumeration, func_ptr, numParams, input, output, init, deinit);
    if (kernel) {
        kernel->incrementReference(VX_REF_EXTERNAL);
    } else {
        VXLOGE("Adding kernel to context fails");
    }

    EXYNOS_VISION_API_OUT();

    return (vx_kernel)kernel;
}

VX_API_ENTRY vx_kernel VX_API_CALL vxGetKernelByEnum(vx_context context, vx_enum kernel)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionKernel *cKernel = NULL;

    cKernel = cContext->getKernelByEnum(kernel);
    if (cKernel) {
        cKernel->incrementReference(VX_REF_EXTERNAL);
    } else {
        VXLOGE("kernel is not found");
    }

    EXYNOS_VISION_API_OUT();

    return (vx_kernel)cKernel;
}

VX_API_ENTRY void VX_API_CALL vxRegisterLogCallback(vx_context context, vx_log_callback_f callback, vx_bool reentrant)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return;
    }

    ExynosVisionContext *cContext = (ExynosVisionContext *)context;
    cContext->registerLogCallback(callback, reentrant);

    EXYNOS_VISION_API_OUT();

    return;
}

VX_API_ENTRY vx_graph VX_API_CALL vxCreateGraph(vx_context context)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status;
    ExynosVisionContext *cContext = (ExynosVisionContext *)context;

    ExynosVisionGraph *cGraph = new ExynosVisionGraph(cContext);
    status = cGraph->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("graph object creation fail(%d)", status);
        delete cGraph;
        goto EXIT;
    }
    cGraph->incrementReference(VX_REF_EXTERNAL);

    status = cGraph->init();
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fail(%d)", status);
        delete cGraph;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_graph)cGraph;
    else
        return (vx_graph)cContext->getErrorObject(status);

}

VX_API_ENTRY vx_status VX_API_CALL vxVerifyGraph(vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    status = cGraph->verifyGraph();
    if (status != VX_SUCCESS) {
        VXLOGE("verifying graph fail id:%d(err:%d)", cGraph->getId(), status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
#include "ExynosVisionAutoTimer.h"
#endif

VX_API_ENTRY vx_status VX_API_CALL vxProcessGraph(vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
    uint64_t start_time, end_time;
    start_time = ExynosVisionDurationTimer::getTimeUs();
#endif

    status = cGraph->processGraph();
    if (status != VX_SUCCESS) {
        VXLOGE("processing graph fail, err:%d", status);
        cGraph->displayInfo(0, vx_true_e);
    }

#if (DISPLAY_PROCESS_GRAPH_TIME==1)
    end_time = ExynosVisionDurationTimer::getTimeUs();
    VXLOGI("[GP] %llu us", end_time - start_time);
#endif

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryGraph(vx_graph graph, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    status = cGraph->queryGraph(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAddParameterToGraph(vx_graph graph, vx_parameter parameter)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)parameter, VX_TYPE_PARAMETER) == vx_false_e) {
        /* Add null param to graph, if the parameter is invalid. */
        parameter = NULL;
    }

    vx_status status;

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionParameter *cParameter = (ExynosVisionParameter*)parameter;

    status = cGraph->addParameterToGraph(cParameter);
    if (status != VX_SUCCESS) {
        VXLOGE("adding parameter to graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetGraphParameterByIndex(vx_graph graph, vx_uint32 index, vx_reference ref)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }
    if (ExynosVisionDataReference::isValidDataReference((ExynosVisionReference*)ref) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionDataReference *cDataRef = (ExynosVisionDataReference*)ref;

    status = cGraph->setGraphParameterByIndex(index, cDataRef);
    if (status != VX_SUCCESS) {
        VXLOGE("setting parameter to graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_parameter VX_API_CALL vxGetGraphParameterByIndex(vx_graph graph, vx_uint32 index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionParameter *cParameter;

    cParameter = cGraph->getGraphParameterByIndex(index);
    if (cParameter == NULL) {
        VXLOGE("setting parameter to graph fails");
    }
    cParameter->incrementReference(VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return (vx_parameter)cParameter;
}

VX_API_ENTRY vx_bool VX_API_CALL vxIsGraphVerified(vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return vx_false_e;
    }

    vx_bool verified = vx_false_e;

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    verified = cGraph->isGraphVerified();

    EXYNOS_VISION_API_OUT();

    return verified;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseGraph(vx_graph *graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *graph);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)(*graph);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cGraph, VX_REF_EXTERNAL);
    *graph = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxScheduleGraph(vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return vx_false_e;
    }

    vx_status status;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    status = cGraph->scheduleGraph();
    if (status != VX_SUCCESS) {
        VXLOGE("scheduling graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxWaitGraph(vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return vx_false_e;
    }

    vx_status status;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    status = cGraph->waitGraph();
    if (status != VX_SUCCESS) {
        VXLOGE("waiting graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetGraphAttribute(vx_graph graph, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return vx_false_e;
    }

    vx_status status;

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    status = cGraph->setGraphAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting graph attribute fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxFinalizeKernel(vx_kernel kernel)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;

    status = cKernel->finalizeKernel();

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAddParameterToKernel(vx_kernel kernel,
                                        vx_uint32 index,
                                        vx_enum dir,
                                        vx_enum data_type,
                                        vx_enum state)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_ERROR_INVALID_PARAMETERS;
    ExynosVisionKernel *cKernel = (ExynosVisionKernel *)kernel;

    status = cKernel->addParameterToKernel(index, dir, data_type, state);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryKernel(vx_kernel kernel, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status;

    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;
    status = cKernel->queryKernel(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying kernel fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;

}

VX_API_ENTRY vx_kernel VX_API_CALL vxGetKernelByName(vx_context context, const vx_char *name)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }
    vx_status status = VX_SUCCESS;

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionKernel *cKernel = NULL;
    cKernel = cContext->getKernelByName(name);
    if (cKernel) {
        cKernel->incrementReference(VX_REF_EXTERNAL);
    } else {
        VXLOGE("getting kernel by name fails");
    }

    EXYNOS_VISION_API_OUT();

    return (vx_kernel)cKernel;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetKernelAttribute(vx_kernel kernel, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;
    status = cKernel->setKernelAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting kernel attr fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_parameter VX_API_CALL vxGetKernelParameterByIndex(vx_kernel kernel, vx_uint32 index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return NULL;
    }

    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;
    ExynosVisionParameter *cParameter;
    cParameter = cKernel->getKernelParameterByIndex(index);
    if (cParameter == NULL) {
        VXLOGE("getting kernel parameter fails");
    }
    cParameter->incrementReference(VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return (vx_parameter)cParameter;
}

VX_API_ENTRY vx_status VX_API_CALL vxRemoveKernel(vx_kernel kernel)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;
    if (cKernel->getFinalizeFlag() == vx_true_e) {
        VXLOGE("can't remove finalized kernel");
        return VX_ERROR_INVALID_REFERENCE;
    }

    ExynosVisionContext *cContext = cKernel->getContext();

    status = cContext->removeKernel(&cKernel);
    if (status != VX_SUCCESS) {
        VXLOGE("removing %s fails, err:%d", cKernel->getName(), status);
    }

    if (cKernel)
        status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cKernel, VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseKernel(vx_kernel *kernel)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *kernel);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)(*kernel);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cKernel, VX_REF_EXTERNAL);
    *kernel = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_node VX_API_CALL vxCreateGenericNode ( vx_graph graph, vx_kernel kernel)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)kernel, VX_TYPE_KERNEL) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", kernel);
        return NULL;
    }

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionKernel *cKernel = (ExynosVisionKernel*)kernel;
    ExynosVisionNode *cNode;

    cNode = cGraph->createGenericNode(cKernel);
    if (cNode->getStatus() != VX_SUCCESS) {
        VXLOGE("node is not created(%d)", cNode->getStatus());
    }
    cNode->incrementReference(VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return (vx_node)cNode;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryNode(vx_node node, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status;

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    status = cNode->queryNode(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying node fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetNodeAttribute(vx_node node, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    status = cNode->setNodeAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting node attr fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseNode(vx_node *node)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionNode *cNode = (ExynosVisionNode*)(*node);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cNode, VX_REF_EXTERNAL);
    if (status != VX_SUCCESS) {
        VXLOGE("releasing node fails, err:%d", status);
    }
    *node = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxRemoveNode(vx_node *node)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionNode *cNode = (ExynosVisionNode*)(*node);
    ExynosVisionGraph *cGraph = cNode->getParentGraph();

    status = cGraph->removeNode(cNode);
    if (status != VX_SUCCESS) {
        VXLOGE("removing %s from %s fails, err:%d", cNode->getName(), cGraph->getName(), status);
    }

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cNode, VX_REF_EXTERNAL);
    if (status != VX_SUCCESS) {
        VXLOGE("releasing %s fails, err:%d", cNode->getName(), status);
    }
    *node = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAssignNodeCallback(vx_node node, vx_nodecomplete_f callback)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    status = cNode->assignNodeCallback(callback);
    if (status != VX_SUCCESS) {
        VXLOGE("assigning node callback fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_nodecomplete_f VX_API_CALL vxRetrieveNodeCallback(vx_node node)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return NULL;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    vx_nodecomplete_f callback = NULL;

    callback = cNode->m_callback;

    EXYNOS_VISION_API_OUT();

    return callback;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetParameterByIndex ( vx_node node, vx_uint32 index, vx_reference value )
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;

    if (ExynosVisionDataReference::isValidDataReference((ExynosVisionReference*)value) == vx_false_e) {
        VXLOGE("wrong pointer(%p) at %s(%s)", value, cNode->getName(), cNode->getKernelName());
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionDataReference *cDataRef = (ExynosVisionDataReference*)value;

    status= cNode->setParameterByIndex(index, cDataRef);
    if (status != VX_SUCCESS) {
        VXLOGE("setting parameter by index fail(%d)", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_parameter VX_API_CALL vxGetParameterByIndex(vx_node node, vx_uint32 index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return NULL;
    }

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    ExynosVisionParameter *cParam = NULL;

    cParam = cNode->getParameterByIndex(index);
    if (cParam->getStatus() != VX_SUCCESS) {
        VXLOGE("parameter is not retrived(%d)", cParam->getStatus());
    }
    cParam->incrementReference(VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return (vx_parameter)cParam;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryParameter(vx_parameter param, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)param, VX_TYPE_PARAMETER) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", param);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionParameter *cParam = (ExynosVisionParameter*)param;
    status = cParam->queryParameter(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("querying parameter(%d) failed, err:%d", cParam->getId(), status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseParameter(vx_parameter *param)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*param, VX_TYPE_PARAMETER) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *param);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionParameter *cParam = (ExynosVisionParameter*)(*param);
    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cParam, VX_REF_EXTERNAL);
    *param = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetParameterByReference(vx_parameter parameter, vx_reference value)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)parameter, VX_TYPE_PARAMETER) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", parameter);
        return VX_ERROR_INVALID_REFERENCE;
    }
    if (ExynosVisionDataReference::isValidDataReference((ExynosVisionReference*)value) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", value);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionParameter *cParam = (ExynosVisionParameter*)parameter;
    ExynosVisionDataReference *cDataRef = (ExynosVisionDataReference*)value;
    ExynosVisionNode *cNode = cParam->getNode();
    vx_uint32 index = cParam->getIndex();

    if (cNode != NULL) {
        status= cNode->setParameterByIndex(index, cDataRef);
        if (status != VX_SUCCESS) {
            VXLOGE("setting parameter by index fail(%d)", status);
        }

    } else {
        VXLOGE("can't achive node object");
        status = VX_ERROR_INVALID_PARAMETERS;
    }


    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetMetaFormatAttribute(vx_meta_format meta, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)meta, VX_TYPE_META_FORMAT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", meta);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionMeta *cMeta = (ExynosVisionMeta*)meta;

    status = cMeta->setMetaFormatAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("set meta format fail, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessArrayRange(vx_array arr, vx_size start, vx_size end, vx_size *stride, void **ptr, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->accessArrayRange(start, end, stride, ptr, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing arr patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;

}

VX_API_ENTRY vx_status VX_API_CALL vxAddArrayItems(vx_array arr, vx_size count, const void *ptr, vx_size stride)
{
//    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->addArrayItems(count, ptr, stride);
    if (status != VX_SUCCESS)
        VXLOGE("adding item to %s fails, err:%d", cArray->getName(), status);

//    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitArrayRange(vx_array arr, vx_size start, vx_size end, const void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->commitArrayRange(start, end, ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing arr patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_array VX_API_CALL vxCreateArray(vx_context context, vx_enum item_type, vx_size capacity)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionArray *cArray = NULL;

    status = ExynosVisionArray::checkValidCreateArray(cContext, item_type, capacity);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cArray = new ExynosVisionArray(cContext, cContext, vx_false_e);
    status = cArray->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("array object creation fails, err:%d", status);
        delete cArray;
        goto EXIT;
    }
    cArray->incrementReference(VX_REF_EXTERNAL);

    status = cArray->init(item_type, capacity);
    if (status != VX_SUCCESS) {
        VXLOGE("array object init fails, err:%d", status);
        delete cArray;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_array)cArray;
    else
        return (vx_array)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_array VX_API_CALL vxCreateVirtualArray(vx_graph graph, vx_enum item_type, vx_size capacity)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionArray *cArray = NULL;

    cArray = new ExynosVisionArray(cGraph->getContext(), cGraph, vx_true_e);
    status = cArray->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("array object creation fails, err:%d", status);
        delete cArray;
        goto EXIT;
    }
    cArray->incrementReference(VX_REF_EXTERNAL);

    status = cArray->init(item_type, capacity);
    if (status != VX_SUCCESS) {
        VXLOGE("array object init fails, err:%d", status);
        delete cArray;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_array)cArray;
    else
        return (vx_array)cGraph->getContext()->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryArray(vx_array arr, vx_enum attribute, void *ptr, vx_size size)
{
//    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->queryArray(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query array object fails, err:%d", status);
    }

//    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseArray(vx_array *arr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionArray *cArray = (ExynosVisionArray*)(*arr);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cArray, VX_REF_EXTERNAL);
    *arr = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxTruncateArray(vx_array arr, vx_size new_num_items)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->truncateArray(new_num_items);
    if (status != VX_SUCCESS) {
        VXLOGE("truncating array object fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_convolution VX_API_CALL vxCreateConvolution(vx_context context, vx_size columns, vx_size rows)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionConvolution*cConvolution = NULL;

    status = ExynosVisionConvolution::checkValidCreateConvolution(columns, rows);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cConvolution = new ExynosVisionConvolution(cContext, cContext);
    status = cConvolution->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("convolution object creation fails, err:%d", status);
        delete cConvolution;
        goto EXIT;
    }
    cConvolution->incrementReference(VX_REF_EXTERNAL);

    status = cConvolution->init(columns, rows);
    if (status != VX_SUCCESS) {
        VXLOGE("convolution object init fails, err:%d)", status);
        delete cConvolution;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_convolution)cConvolution;
    else
        return (vx_convolution)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryConvolution(vx_convolution conv, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)conv, VX_TYPE_CONVOLUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", conv);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionConvolution *cConvolution = (ExynosVisionConvolution*)conv;

    status = cConvolution->queryConvolution(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query convolution object fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReadConvolutionCoefficients(vx_convolution conv, vx_int16 *array)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)conv, VX_TYPE_CONVOLUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", conv);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionConvolution *cConvolution = (ExynosVisionConvolution*)conv;
    status = cConvolution->readConvolutionCoefficients(array);
    if (status != VX_SUCCESS)
        VXLOGE("read convolution(%s) fails, err:%d", cConvolution->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseConvolution(vx_convolution *conv)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*conv, VX_TYPE_CONVOLUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *conv);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionConvolution *cCnvolution = (ExynosVisionConvolution*)(*conv);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cCnvolution, VX_REF_EXTERNAL);
    *conv = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetConvolutionAttribute(vx_convolution conv, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)conv, VX_TYPE_CONVOLUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", conv);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionConvolution *cConvolution = (ExynosVisionConvolution*)conv;

    status = cConvolution->setConvolutionAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting convolution attribute fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxWriteConvolutionCoefficients(vx_convolution conv, const vx_int16 *array)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)conv, VX_TYPE_CONVOLUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", conv);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionConvolution *cConvolution = (ExynosVisionConvolution*)conv;
    status = cConvolution->writeConvolutionCoefficients(array);
    if (status != VX_SUCCESS)
        VXLOGE("write convolution(%s) fails, err:%d", cConvolution->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessDistribution(vx_distribution distribution, void **ptr, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)distribution;
    status = cDistribution->accessDistribution(ptr, usage);
    if (status != VX_SUCCESS)
        VXLOGE("accessing distribution(%s) fails, err:%d", cDistribution->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitDistribution(vx_distribution distribution, const void * ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)distribution;
    status = cDistribution->commitDistribution(ptr);
    if (status != VX_SUCCESS)
        VXLOGE("commiting distribution(%s) fails, err:%d", cDistribution->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_distribution VX_API_CALL vxCreateDistribution(vx_context context, vx_size numBins, vx_int32 offset, vx_uint32 range)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionDistribution *cDistribution = NULL;

    status = ExynosVisionDistribution::checkValidCreateDistribution(numBins, range);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cDistribution = new ExynosVisionDistribution(cContext, cContext);
    status = cDistribution->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("distribution object creation fails, err:%d", status);
        delete cDistribution;
        goto EXIT;
    }
    cDistribution->incrementReference(VX_REF_EXTERNAL);

    status = cDistribution->init(numBins, offset, range);
    if (status != VX_SUCCESS) {
        VXLOGE("distribution object init fail(%d)", status);
        delete cDistribution;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_distribution)cDistribution;
    else
        return (vx_distribution)cContext->getErrorObject(status);
}


VX_API_ENTRY vx_status VX_API_CALL vxQueryDistribution(vx_distribution distribution, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)distribution;

    status = cDistribution->queryDistribution(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query distribution object init fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseDistribution(vx_distribution *distribution)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)(*distribution);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cDistribution, VX_REF_EXTERNAL);
    *distribution = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessImagePatch(vx_image image,
                                    const vx_rectangle_t *rect,
                                    vx_uint32 plane_index,
                                    vx_imagepatch_addressing_t *addr,
                                    void **ptr,
                                    vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->accessImagePatch(rect, plane_index, addr, ptr, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitImagePatch(vx_image image,
                                    vx_rectangle_t *rect,
                                    vx_uint32 plane_index,
                                    vx_imagepatch_addressing_t *addr,
                                    const void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->commitImagePatch(rect, plane_index, addr, ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_size VX_API_CALL vxComputeImagePatchSize(vx_image image,
                                       const vx_rectangle_t *rect,
                                       vx_uint32 plane_index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_size size;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    size = cImage->computeImagePatchSize(rect, plane_index);
    if (size == 0) {
        VXLOGE("computing image patch size is zero");
    }

    EXYNOS_VISION_API_OUT();

    return size;
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateImage(vx_context context, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionImage *cImage = NULL;

    status = ExynosVisionImage::checkValidCreateImage(width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cImage = new ExynosVisionImage(cContext, cContext, vx_false_e);
    status = cImage->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("image object creation fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }
    cImage->incrementReference(VX_REF_EXTERNAL);

    status = cImage->init(width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateImageFromHandle(vx_context context, vx_df_image color, vx_imagepatch_addressing_t addrs[], void *ptrs[], vx_enum import_type)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionImageHandle *cImage = NULL;

    if (vxIsValidImport(import_type) == vx_false_e)
        return (vx_image)cContext->getErrorObject(VX_ERROR_INVALID_PARAMETERS);

    status = ExynosVisionImage::checkValidCreateImage(addrs[0].dim_x, addrs[0].dim_y, color);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cImage = new ExynosVisionImageHandle(cContext, cContext);
    status = cImage->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("image object creation fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }
    cImage->incrementReference(VX_REF_EXTERNAL);

    status |= cImage->checkContinuous(color, ptrs);
    status |= cImage->init(addrs[0].dim_x, addrs[0].dim_y, color);
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }

    status = cImage->importMemory(addrs, ptrs, import_type);
    if (status != VX_SUCCESS) {
        VXLOGE("importing memory fails at %s, err:%d", cImage->getName(), status);
        delete cImage;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateImageFromROI(vx_image image, const vx_rectangle_t *rect)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionImageROI *cImage = NULL;
    ExynosVisionContext *cContext = (ExynosVisionContext*)vxGetContext((vx_reference)image);

    cImage = new ExynosVisionImageROI(cContext, cContext);
    status = cImage->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("image object creation fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }
    cImage->incrementReference(VX_REF_EXTERNAL);

    status = cImage->initFromROI((ExynosVisionImage*)image, rect);
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateUniformImage(vx_context context, vx_uint32 width, vx_uint32 height, vx_df_image format, const void *value)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionImage *cImage = (ExynosVisionImage*)vxCreateImage(context, width, height, format);

    status = cImage->fillUniformValue(value);
    if (status != VX_SUCCESS) {
        VXLOGE("filling value fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateVirtualImage(vx_graph graph, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionImage *cImage = NULL;

    cImage = new ExynosVisionImage(cGraph->getContext(), cGraph, vx_true_e);
    status = cImage->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("image object creation fails, err:%d", status);
        delete cImage;
        goto EXIT;
    }
    cImage->incrementReference(VX_REF_EXTERNAL);

    status = cImage->init(width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fail, err:%d", status);
        delete cImage;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cGraph->getContext()->getErrorObject(status);
}

VX_API_ENTRY void * VX_API_CALL vxFormatImagePatchAddress1d(void *ptr, vx_uint32 index, const vx_imagepatch_addressing_t *addr)
{
    vx_uint8 *new_ptr = NULL;
    if (ptr && index < addr->dim_x*addr->dim_y) {
        vx_uint32 x = index % addr->dim_x;
        vx_uint32 y = index / addr->dim_x;
        vx_uint32 offset = ((addr->stride_y * ((addr->scale_y * y)/VX_SCALE_UNITY)) +
             (addr->stride_x * ((addr->scale_x * x)/VX_SCALE_UNITY)));
        new_ptr = (vx_uint8 *)ptr;
        new_ptr = &new_ptr[offset];
    }
    return new_ptr;
}

VX_API_ENTRY void * VX_API_CALL vxFormatImagePatchAddress2d(void *ptr, vx_uint32 x, vx_uint32 y, const vx_imagepatch_addressing_t *addr)
{
    vx_uint8 *new_ptr = NULL;
    if (ptr && x < addr->dim_x && y < addr->dim_y) {
        vx_uint32 offset = ((addr->stride_y * ((addr->scale_y * y)/VX_SCALE_UNITY)) +
             (addr->stride_x * ((addr->scale_x * x)/VX_SCALE_UNITY)));
        new_ptr = (vx_uint8 *)ptr;
        new_ptr = &new_ptr[offset];
    }
    return new_ptr;
}

VX_API_ENTRY vx_status VX_API_CALL vxGetValidRegionImage(vx_image image, vx_rectangle_t *rect)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionImage *cImage = (ExynosVisionImage*)image;
    status = cImage->getValidRegionImage(rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region image fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryImage(vx_image image, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->queryImage(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query image object init fail, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseImage(vx_image *image)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionImage *cImage = (ExynosVisionImage*)(*image);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cImage, VX_REF_EXTERNAL);
    *image = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetImageAttribute(vx_image image, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionImage *cImage = (ExynosVisionImage*)image;
    status = cImage->setImageAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting image attr fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSwapImageHandle (vx_image image, void *const new_ptrs[], void *prev_ptrs[], vx_size num_planes)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionImage *cImage = (ExynosVisionImage*)image;
    status = cImage->swapImageHandle(new_ptrs, prev_ptrs, num_planes);
    if (status != VX_SUCCESS) {
        VXLOGE("swapping image handle fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessLUT(vx_lut lut, void **ptr, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionLut *cLut = (ExynosVisionLut*)lut;

    status = cLut->accessLut(ptr, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing lut patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitLUT(vx_lut lut, const void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionLut *cLut = (ExynosVisionLut*)lut;

    status = cLut->commitLut(ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing lut patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_lut VX_API_CALL vxCreateLUT(vx_context context, vx_enum data_type, vx_size count)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionLut *cLut = NULL;

    status = ExynosVisionLut::checkValidCreateLut(data_type, count);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cLut = new ExynosVisionLut(cContext, cContext);
    status = cLut->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("lut object creation fails, err:%d", status);
        delete cLut;
        goto EXIT;
    }
    cLut->incrementReference(VX_REF_EXTERNAL);

    status = cLut->init(data_type, count);
    if (status != VX_SUCCESS) {
        VXLOGE("lut object init fail(%d)", status);
        delete cLut;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_lut)cLut;
    else
        return (vx_lut)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryLUT(vx_lut lut, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionLut *cLut = (ExynosVisionLut*)lut;

    status = cLut->queryLut(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query lut object init fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseLUT(vx_lut *lut)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionLut *cLut = (ExynosVisionLut*)(*lut);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cLut, VX_REF_EXTERNAL);
    *lut = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_matrix VX_API_CALL vxCreateMatrix(vx_context context, vx_enum data_type, vx_size columns, vx_size rows)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionMatrix *cMatrix = NULL;

    status = ExynosVisionMatrix::checkValidCreateMatrix(data_type, columns, rows);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cMatrix = new ExynosVisionMatrix(cContext, cContext);
    status = cMatrix->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("matrix object creation fails, err:%d", status);
        delete cMatrix;
        goto EXIT;
    }
    cMatrix->incrementReference(VX_REF_EXTERNAL);

    status = cMatrix->init(data_type, columns, rows);
    if (status != VX_SUCCESS) {
        VXLOGE("matrix object init fail, err:%d", status);
        delete cMatrix;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_matrix)cMatrix;
    else
        return (vx_matrix)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryMatrix(vx_matrix mat, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)mat, VX_TYPE_MATRIX) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", mat);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionMatrix *cMatrix = (ExynosVisionMatrix*)mat;

    status = cMatrix->queryMatrix(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query matrix object init fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReadMatrix(vx_matrix mat, void *array)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)mat, VX_TYPE_MATRIX) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", mat);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionMatrix *cMatrix = (ExynosVisionMatrix*)mat;
    status = cMatrix->readMatrix(array);
    if (status != VX_SUCCESS)
        VXLOGE("read matrix(%s) fails, err:%d", cMatrix->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseMatrix(vx_matrix *mat)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*mat, VX_TYPE_MATRIX) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *mat);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionMatrix *cMatrix = (ExynosVisionMatrix*)(*mat);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cMatrix, VX_REF_EXTERNAL);
    *mat = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxWriteMatrix(vx_matrix mat, const void *array)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)mat, VX_TYPE_MATRIX) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", mat);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionMatrix *cMatrix = (ExynosVisionMatrix*)mat;
    status = cMatrix->writeMatrix(array);
    if (status != VX_SUCCESS)
        VXLOGE("write matrix(%s) fails, err:%d", cMatrix->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_pyramid VX_API_CALL vxCreatePyramid(vx_context context, vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionPyramid *cPyramid = NULL;

    status = ExynosVisionPyramid::checkValidCreatePyramid(levels, scale, width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cPyramid = new ExynosVisionPyramid(cContext, cContext, vx_false_e);
    status = cPyramid->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("pyramid object creation fails, err:%d", status);
        delete cPyramid;
        goto EXIT;
    }
    cPyramid->incrementReference(VX_REF_EXTERNAL);

    status = cPyramid->init(levels, scale, width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("pyramid object init fail, err:%d", status);
        delete cPyramid;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_pyramid)cPyramid;
    else
        return (vx_pyramid)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_pyramid VX_API_CALL vxCreateVirtualPyramid(vx_graph graph, vx_size levels, vx_float32 scale, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionPyramid *cPyramid = NULL;

    cPyramid = new ExynosVisionPyramid(cGraph->getContext(), cGraph, vx_true_e);
    status = cPyramid->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("pyramid object creation fail(%d)", status);
        delete cPyramid;
        goto EXIT;
    }
    cPyramid->incrementReference(VX_REF_EXTERNAL);

    status = cPyramid->init(levels, scale, width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("pyramid object init fail(%d)", status);
        delete cPyramid;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_pyramid)cPyramid;
    else
        return (vx_pyramid)cGraph->getContext()->getErrorObject(status);
}

VX_API_ENTRY vx_image VX_API_CALL vxGetPyramidLevel(vx_pyramid pyr, vx_uint32 index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)pyr, VX_TYPE_PYRAMID) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", pyr);
        return NULL;
    }

    ExynosVisionPyramid *cPyramid = (ExynosVisionPyramid*)pyr;
    ExynosVisionImage *cImage;

    cImage = cPyramid->getPyramidLevel(index);
    if (cImage == NULL)
        VXLOGE("getting image from pyramid fails");

    EXYNOS_VISION_API_OUT();

    return (vx_image)cImage;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryPyramid(vx_pyramid pyr, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)pyr, VX_TYPE_PYRAMID) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", pyr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionPyramid *cPyramid = (ExynosVisionPyramid*)pyr;

    status = cPyramid->queryPyramid(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query pyramid object fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleasePyramid(vx_pyramid *pyr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*pyr, VX_TYPE_PYRAMID) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *pyr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionPyramid *cPyramid = (ExynosVisionPyramid*)(*pyr);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cPyramid, VX_REF_EXTERNAL);
    *pyr = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_remap VX_API_CALL vxCreateRemap(vx_context context,
                              vx_uint32 src_width,
                              vx_uint32 src_height,
                              vx_uint32 dst_width,
                              vx_uint32 dst_height)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionRemap *cRemap = NULL;

    status = ExynosVisionRemap::checkValidCreateRemap(src_width, src_height, dst_width, dst_height);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cRemap = new ExynosVisionRemap(cContext, cContext);
    status = cRemap->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("remap object creation fail(%d)", status);
        delete cRemap;
        goto EXIT;
    }
    cRemap->incrementReference(VX_REF_EXTERNAL);

    status = cRemap->init(src_width, src_height, dst_width, dst_height);
    if (status != VX_SUCCESS) {
        VXLOGE("remap object init fail(%d)", status);
        delete cRemap;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_remap)cRemap;
    else
        return (vx_remap)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxGetRemapPoint(vx_remap table,
                                 vx_uint32 dst_x, vx_uint32 dst_y,
                                 vx_float32 *src_x, vx_float32 *src_y)
{
//    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)table, VX_TYPE_REMAP) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", table);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionRemap *cRemap = (ExynosVisionRemap*)table;

    status = cRemap->getRemapPoint(dst_x, dst_y, src_x, src_y);
    if (status != VX_SUCCESS) {
        VXLOGE("getting remap point fails, err:%d", status);
    }

//    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryRemap(vx_remap table, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)table, VX_TYPE_REMAP) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", table);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionRemap *cRemap = (ExynosVisionRemap*)table;

    status = cRemap->queryRemap(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query remap object fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseRemap(vx_remap *table)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*table, VX_TYPE_REMAP) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *table);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionRemap *cRemap = (ExynosVisionRemap*)(*table);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cRemap, VX_REF_EXTERNAL);
    *table = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetRemapPoint(vx_remap table,
                                 vx_uint32 dst_x, vx_uint32 dst_y,
                                 vx_float32 src_x, vx_float32 src_y)
{
//    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)table, VX_TYPE_REMAP) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", table);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionRemap *cRemap = (ExynosVisionRemap*)table;

    status = cRemap->setRemapPoint(dst_x, dst_y, src_x, src_y);
    if (status != VX_SUCCESS) {
        VXLOGE("setting remap point fails, err:%d", status);
    }

//    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_scalar VX_API_CALL vxCreateScalar(vx_context context, vx_enum data_type, const void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionScalar *cScalar = NULL;

    status = ExynosVisionScalar::isValidCreateScalar(cContext, data_type);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cScalar = new ExynosVisionScalar(cContext, cContext);
    status = cScalar->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("scalar object creation fails, err:%d", status);
        delete cScalar;
        goto EXIT;
    }
    cScalar->incrementReference(VX_REF_EXTERNAL);

    status = cScalar->init(data_type, ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("scalar object init fail(%d)", status);
        delete cScalar;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_scalar)cScalar;
    else
        return (vx_scalar)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryScalar(vx_scalar scalar, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)scalar, VX_TYPE_SCALAR) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", scalar);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionScalar *cScalar = (ExynosVisionScalar*)scalar;

    status = cScalar->queryScalar(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query scalar object init fail(%d)", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReadScalarValue(vx_scalar ref, void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref, VX_TYPE_SCALAR) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionScalar *cScalar = (ExynosVisionScalar*)ref;
    status = cScalar->readScalarValue(ptr);
    if (status != VX_SUCCESS)
        VXLOGE("read scalar(%s) fails, err:%d", cScalar->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseScalar(vx_scalar *scalar)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*scalar, VX_TYPE_SCALAR) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *scalar);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionScalar *cScalar = (ExynosVisionScalar*)(*scalar);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cScalar, VX_REF_EXTERNAL);
    *scalar = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxWriteScalarValue(vx_scalar ref, const void *ptr)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref, VX_TYPE_SCALAR) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", ref);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionScalar *cScalar = (ExynosVisionScalar*)ref;
    status = cScalar->writeScalarValue(ptr);
    if (status != VX_SUCCESS)
        VXLOGE("write scalar(%s) fails, err:%d", cScalar->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_threshold VX_API_CALL vxCreateThreshold(vx_context context, vx_enum thresh_type, vx_enum data_type)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionThreshold *cThreshold = NULL;

    status = ExynosVisionThreshold::checkValidCreateThreshold(thresh_type, data_type);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cThreshold = new ExynosVisionThreshold(cContext, cContext);
    status = cThreshold->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("threshold object creation fail(%d)", status);
        delete cThreshold;
        goto EXIT;
    }
    cThreshold->incrementReference(VX_REF_EXTERNAL);

    status = cThreshold->init(thresh_type);
    if (status != VX_SUCCESS) {
        VXLOGE("threshold object init fail(%d)", status);
        delete cThreshold;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_threshold)cThreshold;
    else
        return (vx_threshold)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryThreshold(vx_threshold thresh, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)thresh, VX_TYPE_THRESHOLD) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", thresh);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionThreshold *cThreshold = (ExynosVisionThreshold*)thresh;

    status = cThreshold->queryThreshold(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query threshold object init fail(%d)", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseThreshold(vx_threshold *thresh)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*thresh, VX_TYPE_THRESHOLD) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *thresh);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionThreshold *cThreshold = (ExynosVisionThreshold*)(*thresh);

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cThreshold, VX_REF_EXTERNAL);
    *thresh = NULL;

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetThresholdAttribute(vx_threshold thresh, vx_enum attribute, const void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)thresh, VX_TYPE_THRESHOLD) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", thresh);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionThreshold *cThreshold = (ExynosVisionThreshold*)thresh;

    status = cThreshold->setThresholdAttribute(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("setting threshold attribute fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAgeDelay(vx_delay delay)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)delay, VX_TYPE_DELAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", delay);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;

    ExynosVisionDelay *cDelay = (ExynosVisionDelay*)delay;
    status = cDelay->ageDelay();
    if (status != VX_SUCCESS)
        VXLOGE("aging delay fails, err:%d", status);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_delay VX_API_CALL vxCreateDelay(vx_context context,
                              vx_reference exemplar,
                              vx_size count)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionContext *cContext = (ExynosVisionContext*)context;
    ExynosVisionDataReference *cDataRef;
    ExynosVisionDelay *cDelay = NULL;

    if (ExynosVisionDataReference::isValidDataReference((ExynosVisionReference*)exemplar) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", exemplar);
        status = VX_ERROR_INVALID_REFERENCE;
        goto EXIT;
    }

    cDataRef = (ExynosVisionDataReference*)exemplar;
    status = ExynosVisionDelay::checkValidCreateDelay(cContext, cDataRef);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cDelay = new ExynosVisionDelay(cContext);
    status = cDelay->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("delay object creation fails, err:%d", status);
        delete cDelay;
        goto EXIT;
    }
    cDelay->incrementReference(VX_REF_EXTERNAL);

    status = cDelay->init(cDataRef, count);
    if (status != VX_SUCCESS) {
        VXLOGE("delay object init fail(%d)", status);
        delete cDelay;
        goto EXIT;
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_delay)cDelay;
    else
        return (vx_delay)cContext->getErrorObject(status);
}

VX_API_ENTRY vx_reference VX_API_CALL vxGetReferenceFromDelay(vx_delay delay, vx_int32 index)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)delay, VX_TYPE_DELAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", delay);
        return NULL;
    }

    ExynosVisionDelay *cDelay = (ExynosVisionDelay*)delay;
    ExynosVisionDataReference *cDataRef;
    cDataRef = cDelay->getReferenceFromDelay(index);
    if (cDataRef == NULL)
        VXLOGE("%s doesn't have reference at index(%d)", cDelay->getName(), index);

    EXYNOS_VISION_API_OUT();

    return (vx_reference)cDataRef;
}

VX_API_ENTRY vx_status VX_API_CALL vxQueryDelay(vx_delay delay, vx_enum attribute, void *ptr, vx_size size)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)delay, VX_TYPE_DELAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", delay);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionDelay *cDelay = (ExynosVisionDelay*)delay;

    status = cDelay->queryDelay(attribute, ptr, size);
    if (status != VX_SUCCESS) {
        VXLOGE("query delay object init fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxReleaseDelay(vx_delay *delay)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)*delay, VX_TYPE_DELAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", *delay);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionDelay *cDelay = (ExynosVisionDelay*)(*delay);
    *delay = NULL;

    status = ExynosVisionReference::releaseReferenceInt((ExynosVisionReference**)&cDelay, VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY void VX_API_CALL vxAddLogEntry(vx_reference ref, vx_status status, const char *message, ...)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return;
    }

    ExynosVisionReference *cRef = (ExynosVisionReference*)ref;
    ExynosVisionContext *cContext = cRef->getContext();

    va_list args;
    va_start(args, message);
    cContext->addLogEntry(cRef, status, message, args);
    va_end(args);

    EXYNOS_VISION_API_OUT();

    return;
}

