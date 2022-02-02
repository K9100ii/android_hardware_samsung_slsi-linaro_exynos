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

#ifndef _OPENVX_API_EXT_H_
#define _OPENVX_API_EXT_H_

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * \file
 * \brief The API definition for OpenVX.
 */

/*==============================================================================
 REFERENCE
 =============================================================================*/
VX_API_ENTRY void VX_API_CALL vxReferenceDisplayInfo(vx_reference ref);
VX_API_ENTRY void VX_API_CALL vxReferenceDisplayPerf(vx_reference ref);
VX_API_ENTRY vx_size VX_API_CALL vxSizeOfType(vx_enum type);

/*==============================================================================
 CONTEXT
 =============================================================================*/
VX_API_ENTRY vx_context VX_API_CALL vxCreateContextLite(void);
VX_API_ENTRY vx_status VX_API_CALL vxSetImmediateModeTarget(vx_context context, vx_enum target_enum, const vx_char *target_string);

/*==============================================================================
 IMAGE
 =============================================================================*/
VX_API_ENTRY vx_image VX_API_CALL vxCreateImageFromQueue(vx_graph graph, vx_uint32 width, vx_uint32 height, vx_df_image format);
VX_API_ENTRY vx_status VX_API_CALL vxGetValidAncestorRegionImage(vx_image image, vx_rectangle_t *rect);
VX_API_ENTRY vx_status VX_API_CALL vxPushImagePatch(vx_image image, vx_uint32 index, void *ptrs[], vx_uint32 num_buf);
VX_API_ENTRY vx_status VX_API_CALL vxPushImageHandle(vx_image image, vx_uint32 index, vx_int32 fd[], vx_uint32 num_buf);
VX_API_ENTRY vx_status VX_API_CALL vxPopImage(vx_image image, vx_uint32 *ret_index, vx_bool *ret_data_valid);
VX_API_ENTRY vx_status VX_API_CALL vxAccessImageHandle(vx_image image,
                                    vx_uint32 plane_index,
                                    vx_int32 *fd,
                                    vx_rectangle_t *rect,
                                    vx_enum usage);
VX_API_ENTRY vx_status VX_API_CALL vxCommitImageHandle(vx_image image,
                                    vx_uint32 plane_index,
                                    const vx_int32 fd);

/*==============================================================================
 KERNEL
 =============================================================================*/

/*==============================================================================
 GRAPH
 =============================================================================*/

/*!
 * \brief A method to construct a node via arbitrary parameters and an enum.
 * \param [in] graph The handle to desired graph to add the node to.
 * \param [in] kernelenum The \ref vx_kernel_e enum value used to create a node.
 * \param [in] params The array of parameter information.
 * \param [in] num The number of elements in params.
 * \return vx_node
 * \retval 0 Indicates a failure.
 * \ingroup group_helper
 */
VX_API_ENTRY vx_node VX_API_CALL vxCreateNodeByStructure(vx_graph graph,
                                                                                                            vx_enum kernel_enum,
                                                                                                            vx_reference params[],
                                                                                                            vx_uint32 num);

/*! \brief Adds a parameter to a graph by indicating the source node, and the
 * index of the parameter on the node.
 * \param [in] g The graph handle.
 * \param [in] n The node handle.
 * \param [in] index The index of the parameter on the node.
 * \return Returns a \ref vx_status_e enumeration.
 * \ingroup group_helper
 */
VX_API_ENTRY vx_status VX_API_CALL vxAddParameterToGraphByIndex(vx_graph g, vx_node n, vx_uint32 index);

VX_API_ENTRY vx_status VX_API_CALL vxSetChildGraphOfNode(vx_node node, vx_graph graph);
VX_API_ENTRY vx_graph VX_API_CALL vxGetChildGraphOfNode(vx_node node);
VX_API_ENTRY vx_status VX_API_CALL vxReplaceNodeToGraph(vx_node node, vx_graph graph);


/*==============================================================================
 NODE
 =============================================================================*/
VX_API_ENTRY vx_status VX_API_CALL vxSetNodeTarget(vx_node node, vx_enum target_enum, const vx_char *target_string);

/*==============================================================================
 PARAMETER
 =============================================================================*/

/*==============================================================================
 SCALAR
 =============================================================================*/

/*==============================================================================
 DELAY
 =============================================================================*/

/*==============================================================================
 LOGGING
 =============================================================================*/

/*==============================================================================
 LUT
 =============================================================================*/
VX_API_ENTRY vx_status VX_API_CALL vxAccessLUTHandle(vx_lut lut, vx_int32 *fd, vx_enum usage);
VX_API_ENTRY vx_status VX_API_CALL vxCommitLUTHandle(vx_lut lut, const vx_int32 fd);

/*==============================================================================
 Distribution
 =============================================================================*/
VX_API_ENTRY vx_status VX_API_CALL vxAccessDistributionHandle(vx_distribution distribution, vx_int32 *fd, vx_enum usage);
VX_API_ENTRY vx_status VX_API_CALL vxCommitDistributionHandle(vx_distribution distribution, const vx_int32 fd);

/*==============================================================================
 THRESHOLD
 =============================================================================*/

/*==============================================================================
 MATRIX
 =============================================================================*/

/*==============================================================================
 CONVOLUTION
 =============================================================================*/

/*==============================================================================
 PYRAMID
 =============================================================================*/

/*==============================================================================
 REMAP
 =============================================================================*/

/*==============================================================================
 ARRAY
 =============================================================================*/
VX_API_ENTRY vx_status VX_API_CALL vxAccessArrayHandle(vx_array arr, vx_int32 *fd, vx_enum usage);
VX_API_ENTRY vx_status VX_API_CALL vxCommitArrayHandle(vx_array arr, const vx_int32 fd);
VX_API_ENTRY vx_status VX_API_CALL vxAddEmptyArrayItems(vx_array arr, vx_size count);

/*==============================================================================
 META FORMAT
 =============================================================================*/

#ifdef __cplusplus
}
#endif

#endif
