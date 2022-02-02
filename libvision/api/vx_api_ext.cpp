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

#include "ExynosVisionReference.h"
#include "ExynosVisionGraph.h"
#include "ExynosVisionImage.h"
#include "ExynosVisionLut.h"
#include "ExynosVisionDistribution.h"
#include "ExynosVisionArray.h"

using namespace android;

VX_API_ENTRY void VX_API_CALL vxReferenceDisplayInfo(vx_reference ref)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return;
    }

    ExynosVisionReference *cRef = (ExynosVisionReference*)ref;
    cRef->displayInfo(0, vx_true_e);

    EXYNOS_VISION_API_OUT();
}

VX_API_ENTRY void VX_API_CALL vxReferenceDisplayPerf(vx_reference ref)
{
    EXYNOS_VISION_API_IN();
    if ((ExynosVisionContext::isValidContext((ExynosVisionContext*)ref) == vx_false_e) &&
        (ExynosVisionReference::isValidReference((ExynosVisionReference*)ref) == vx_false_e)) {
        VXLOGE("wrong pointer(%p)", ref);
        return;
    }

    ExynosVisionReference *cRef = (ExynosVisionReference*)ref;
    cRef->displayPerf(0, vx_true_e);

    EXYNOS_VISION_API_OUT();
}

VX_API_ENTRY vx_size VX_API_CALL vxSizeOfType(vx_enum type)
{
    EXYNOS_VISION_API_IN();
    vx_size size;

    size = ExynosVisionReference::sizeOfType(type);

    EXYNOS_VISION_API_OUT();
    return size;
}

VX_API_ENTRY vx_context VX_API_CALL vxCreateContextLite(void)
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

    EXYNOS_VISION_API_OUT();

    return (vx_context)cContext;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetImmediateModeTarget(vx_context context, vx_enum target_enum, const vx_char *target_string)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionContext::isValidContext((ExynosVisionContext*)context) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", context);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionContext *cContext = (ExynosVisionContext*)context;

    if (target_enum == VX_TARGET_STRING) {
        target_enum = ExynosVisionTarget::getTargetEnumByTargetString(target_string);
        if(target_enum == VX_TARGET_INVALID) {
            status = VX_FAILURE;
            goto EXIT;
        }
    }

    status = cContext->setImmediateModeTarget(target_enum);

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_node VX_API_CALL vxCreateNodeByStructure(vx_graph graph,
                                vx_enum kernel_enum,
                                vx_reference params[],
                                vx_uint32 num)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }
    for (vx_uint32 i=0; i<num; i++) {
        if (params[i] != NULL) {
            if (ExynosVisionDataReference::isValidDataReference((ExynosVisionReference*)params[i]) == vx_false_e) {
                VXLOGE("wrong pointer(%p)", params[i]);
                return NULL;
            }
        }
    }

    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    ExynosVisionNode* cNode = cGraph->createNodeByStructure(kernel_enum,(ExynosVisionDataReference**)params, num);
    cNode->incrementReference(VX_REF_EXTERNAL);

    EXYNOS_VISION_API_OUT();

    return (vx_node)cNode;
}

VX_API_ENTRY vx_status VX_API_CALL vxAddParameterToGraphByIndex(vx_graph graph, vx_node node, vx_uint32 index)
{
    EXYNOS_VISION_API_IN();

    vx_parameter param = vxGetParameterByIndex(node, index);
    vx_status status = vxAddParameterToGraph(graph, param);
    vxReleaseParameter(&param);

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetChildGraphOfNode(vx_node node, vx_graph graph)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    status = cNode->setChildGraphOfNode(cGraph);
    if (status != VX_SUCCESS) {
        VXLOGE("setting child graph fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_graph VX_API_CALL vxGetChildGraphOfNode(vx_node node)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return NULL;
    }

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    ExynosVisionGraph *cGraph;

    cGraph = cNode->getChildGraphOfNode();
    if (cGraph == NULL) {
        VXLOGE("%s, getting child graph fails", cNode->getName());
    }

    EXYNOS_VISION_API_OUT();

    return (vx_graph)cGraph;
}

VX_API_ENTRY vx_status VX_API_CALL vxReplaceNodeToGraph(vx_node node, vx_graph graph)
{
    EXYNOS_VISION_API_IN();

    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;

    ExynosVisionNode *cNode = (ExynosVisionNode*)node;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;

    if (cGraph->isGraphVerified() == vx_false_e) {
        VXLOGE("Only verified graph could replace a node");
        status = VX_ERROR_INVALID_GRAPH;
        goto EXIT;
    }

    status = cNode->getParentGraph()->replaceNodeToGraph(cNode, cGraph);
    if (status != VX_SUCCESS) {
        VXLOGE("setting child graph fails, err:%d", status);
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxSetNodeTarget(vx_node node, vx_enum target_enum, const vx_char *target_string)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)node, VX_TYPE_NODE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", node);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_SUCCESS;
    ExynosVisionNode *cNode = (ExynosVisionNode*)node;

    if (target_enum == VX_TARGET_STRING) {
        target_enum = ExynosVisionTarget::getTargetEnumByTargetString(target_string);
        if(target_enum == VX_TARGET_INVALID) {
            status = VX_FAILURE;
            goto EXIT;
        }
    }
    status = cNode->setNodeTarget(target_enum);

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_image VX_API_CALL vxCreateImageFromQueue(vx_graph graph, vx_uint32 width, vx_uint32 height, vx_df_image format)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)graph) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", graph);
        return NULL;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionGraph *cGraph = (ExynosVisionGraph*)graph;
    ExynosVisionImage *cImage = NULL;

    status = ExynosVisionImage::checkValidCreateImage(width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("parameter is not valid, err:%d", status);
        goto EXIT;
    }

    cImage = new ExynosVisionImageQueue(cGraph->getContext(), cGraph);
    status = cImage->getCreationStatus();
    if (status != VX_SUCCESS) {
        VXLOGE("image object creation fail(%d)", status);
        delete cImage;
        goto EXIT;
    }
    cImage->incrementReference(VX_REF_EXTERNAL);

    status = cImage->init(width, height, format);
    if (status != VX_SUCCESS) {
        VXLOGE("image object init fail(%d)", status);
        delete cImage;
        goto EXIT;
    }

    cImage->setImportType(VX_IMPORT_TYPE_HOST);

EXIT:
    EXYNOS_VISION_API_OUT();

    if (status == VX_SUCCESS)
        return (vx_image)cImage;
    else
        return (vx_image)cGraph->getContext()->getErrorObject(status);
}

VX_API_ENTRY vx_status VX_API_CALL vxPushImagePatch(vx_image image, vx_uint32 index, void *ptrs[], vx_uint32 num_buf)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->pushImagePatch(index, ptrs, num_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("pushing queue image fails, err:%d", status);
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxPushImageHandle(vx_image image, vx_uint32 index, vx_int32 fd[], vx_uint32 num_buf)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->pushImageHandle(index, fd, num_buf);
    if (status != VX_SUCCESS) {
        VXLOGE("pushing queue image fails, err:%d", status);
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxPopImage(vx_image image, vx_uint32 *ret_index, vx_bool *ret_data_valid)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->popImage(ret_index, ret_data_valid);
    if (status != VX_SUCCESS) {
        VXLOGE("popping queue image fails, err:%d", status);
    }

EXIT:
    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxGetValidAncestorRegionImage(vx_image image, vx_rectangle_t *rect)
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

    if (cImage->locateROI(rect) == NULL) {
        VXLOGE("calculating ROI fails");
        status = VX_FAILURE;
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessImageHandle(vx_image image,
                                    vx_uint32 plane_index,
                                    vx_int32 *fd,
                                    vx_rectangle_t *rect,
                                    vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    if (rect->start_x || rect->end_x || rect->start_y || rect->end_y) {
        VXLOGE("rect should be zero");
        return VX_ERROR_INVALID_PARAMETERS;
    }

    vx_status status;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->accessImageHandle(plane_index, fd, rect, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing image patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitImageHandle(vx_image image,
                                    vx_uint32 plane_index,
                                    const vx_int32 fd)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)image, VX_TYPE_IMAGE) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", image);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionImage *cImage = (ExynosVisionImage*)image;

    status = cImage->commitImageHandle(plane_index, fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image patch fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessLUTHandle(vx_lut lut, vx_int32 *fd, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionLut *cLut = (ExynosVisionLut*)lut;

    status = cLut->accessLutHandle(fd, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitLUTHandle(vx_lut lut, const vx_int32 fd)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)lut, VX_TYPE_LUT) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", lut);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionLut *cLut = (ExynosVisionLut*)lut;

    status = cLut->commitLutHandle(fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessDistributionHandle(vx_distribution distribution, vx_int32 *fd, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)distribution;

    status = cDistribution->accessDistributionHandle(fd, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitDistributionHandle(vx_distribution distribution, const vx_int32 fd)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)distribution, VX_TYPE_DISTRIBUTION) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", distribution);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionDistribution *cDistribution = (ExynosVisionDistribution*)distribution;

    status = cDistribution->commitDistributionHandle(fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAccessArrayHandle(vx_array arr, vx_int32 *fd, vx_enum usage)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->accessArrayHandle(fd, usage);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxCommitArrayHandle(vx_array arr, const vx_int32 fd)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->commitArrayHandle(fd);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting array ion fails, err:%d", status);
    }

    EXYNOS_VISION_API_OUT();

    return status;
}

VX_API_ENTRY vx_status VX_API_CALL vxAddEmptyArrayItems(vx_array arr, vx_size count)
{
    EXYNOS_VISION_API_IN();
    if (ExynosVisionReference::isValidReference((ExynosVisionReference*)arr, VX_TYPE_ARRAY) == vx_false_e) {
        VXLOGE("wrong pointer(%p)", arr);
        return VX_ERROR_INVALID_REFERENCE;
    }

    vx_status status = VX_FAILURE;
    ExynosVisionArray *cArray = (ExynosVisionArray*)arr;

    status = cArray->addEmptyArrayItems(count);
    if (status != VX_SUCCESS)
        VXLOGE("adding empty item to %s fails, err:%d", cArray->getName(), status);

    EXYNOS_VISION_API_OUT();

    return status;
}

