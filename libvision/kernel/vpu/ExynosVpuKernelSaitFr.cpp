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

#define LOG_TAG "ExynosVpuKernelSaitFr"
#include <cutils/log.h>

#include "ExynosVpuKernelSaitFr.h"

#include "ExynosVpu3dnnProcessBase.h"

#include "vpu_kernel_util.h"

#include "example_cnn_sait16_1st_part_task_ds.h"
#include "example_cnn_sait16_2nd_part_task_ds.h"

#include "cnn_desc_saitfr_ea.h"
#include "cnn_desc_saitfr_la.h"

namespace android {

using namespace std;

static vx_uint16 td_binary_saitfr_ea[] =
    TASK_DS_1ST;

static vx_uint16 td_binary_saitfr_la[] =
    TASK_DS_2ND;

#if (SAVE_INTERMEDIATE_BUFFER==1)
ExynosVpuKernelSaitFrEa *saitfr_ea = NULL;
ExynosVpuKernelSaitFrLa *saitfr_la = NULL;

#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static void
fileWriteToBuffer(char *file_name, char *addr,  int32_t size)
{
    /* file create */
    FILE* fp = NULL;

    fp = fopen(file_name, "w+t");
    if (fp == NULL) {
        mkdir("./cnn_debug/", 0777);
        fp = fopen(file_name, "w+t");
    }

    uint8_t *ptr = (uint8_t*)addr;
    uint32_t cnt = size;

    uint32_t i;
    for (i=0; i < cnt; ptr++, i++) {
        if (i !=0 && ((i%16) == 0)) {
            fprintf(fp, "\n");
        }
        fprintf(fp, "%02x ", *ptr);
    }

    fprintf(fp, "\n");

    fclose(fp);
}
#endif

ExynosVpuKernelCnn::ExynosVpuKernelCnn(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernel(name, param_num)
{

}

void
ExynosVpuKernelCnn::allocAndFillBuf(cnn_buffer_info_t *buf_info, vx_uint32 buf_size, io_buffer_info_t *io_buffer_info)
{
    memset(io_buffer_info, 0x0, sizeof(*io_buffer_info));
    io_buffer_info->size = buf_size;
    io_buffer_info->mapped = true;

    vx_status status = VX_SUCCESS;
    status = allocateBuffer(io_buffer_info);
    if (status != VX_SUCCESS) {
        VXLOGE("allcoating inter task buffer fails");
    }

    if (buf_info->default_buf_array && buf_info->default_buf_array->addr) {
        memcpy(io_buffer_info->addr, buf_info->default_buf_array->addr, buf_info->default_buf_array->size);
    }
}

vx_status
ExynosVpuKernelSaitFrEa::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("input validation fails, index:%d", index);
    }

    return status;
}

vx_status
ExynosVpuKernelSaitFrEa::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 1) {
        vx_df_image meta_format = VX_DF_IMAGE_U8;
        vx_uint32 width = 123008;
        vx_uint32 height = 1;
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
        status = VX_SUCCESS;
    } else if (index == 2) {
        vx_df_image meta_format = VX_DF_IMAGE_U8;
        vx_uint32 width = 240;
        vx_uint32 height = 1;
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
        status = VX_SUCCESS;
    }

    if (status != VX_SUCCESS) {
        VXLOGE("output validation fails, index:%d", index);
    }

    return status;
}

ExynosVpuKernelSaitFrEa::ExynosVpuKernelSaitFrEa(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelCnn(name, param_num)
{
    strcpy(m_task_name, "saitfr");

#if (SAVE_INTERMEDIATE_BUFFER==1)
    saitfr_ea = this;
#endif
}

ExynosVpuKernelSaitFrEa::~ExynosVpuKernelSaitFrEa(void)
{

}

vx_status
ExynosVpuKernelSaitFrEa::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVpuKernelSaitFrEa::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

    status = initTaskFromBinary();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from binary fails, %p, %p", node, parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSaitFrEa::initTaskFromBinary(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;

    task_if_0 = initTask0FromBinary();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelSaitFrEa::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapperSaitFrEa *task_wr = new ExynosVpuTaskIfWrapperSaitFrEa(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

#if 1
    /* JUN_TBD, update buffer size for max pooling */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary_saitfr_ea)->memmap_desc[0]);
    /* 61504 -> 246016 */
    memmap[10].image_sizes.width = 246016;
    /* 3600 -> 13456 */
    memmap[15].image_sizes.width = 13456;

    memmap[16].image_sizes.height = (memmap[16].image_sizes.width + 16384) >> 14;
    memmap[16].image_sizes.width = 16384;
    memmap[17].image_sizes.height = (memmap[17].image_sizes.width + 16384) >> 14;
    memmap[17].image_sizes.width = 16384;
    memmap[18].image_sizes.height = (memmap[18].image_sizes.width + 8192) >> 13;
    memmap[18].image_sizes.width = 8192;
    memmap[19].image_sizes.height = (memmap[19].image_sizes.width + 8192) >> 13;
    memmap[19].image_sizes.width = 8192;
#endif

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    task_if->setCnnTask();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_saitfr_ea);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect memmap to io */
    for (vx_uint32 i=0; i<dimof(cnn_ea::sait_fr_ea_io); i++) {
        task_if->setIoMemmap(cnn_ea::sait_fr_ea_io[i].io_direction, cnn_ea::sait_fr_ea_io[i].io_index, (uint32_t)cnn_ea::sait_fr_ea_io[i].memmap_index);
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelSaitFrEa::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSaitFrEa::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    memset(&io_format, 0x0, sizeof(io_format));
    memset(&io_memory, 0x0, sizeof(io_memory));
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    struct vpul_memory_map_desc *memmap;
    struct vpul_task *task_info = task_wr_0->getTaskIf()->getTask()->getTaskInfo();
    memmap = &(((struct vpul_task*)task_info)->memmap_desc[0]);

    vx_uint32 memmap_index;
    vx_uint32 buf_size;

    for (vx_uint32 i=0; i<dimof(cnn_ea::sait_fr_ea_io); i++) {
        memmap_index = cnn_ea::sait_fr_ea_io[i].memmap_index;

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = memmap[memmap_index].image_sizes.width;
        io_format.height = memmap[memmap_index].image_sizes.height;
        io_format.pixel_byte = memmap[memmap_index].image_sizes.pixel_bytes;
        buf_size = io_format.width * io_format.height * io_format.pixel_byte;

        allocAndFillBuf(&cnn_ea::sait_fr_ea_io[i], buf_size, &io_buffer_info);
        status = task_wr_0->setIoCustomParam(cnn_ea::sait_fr_ea_io[i].io_direction, cnn_ea::sait_fr_ea_io[i].io_index, &io_format, &io_memory, &io_buffer_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

    for (vx_uint32 i=0; i<dimof(cnn_ea::sait_fr_ea_inter); i++) {
        memmap_index = cnn_ea::sait_fr_ea_inter[i].memmap_index;

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = memmap[memmap_index].image_sizes.width;
        io_format.height = memmap[memmap_index].image_sizes.height;
        io_format.pixel_byte = memmap[memmap_index].image_sizes.pixel_bytes;
        buf_size = io_format.width * io_format.height * io_format.pixel_byte;

        allocAndFillBuf(&cnn_ea::sait_fr_ea_inter[i], buf_size, &io_buffer_info);

        ExynosVpuMemmap *memmap = task_wr_0->getTaskIf()->getTask()->getMemmap(memmap_index);

        if (memmap->getMemory()->setBuffer(io_buffer_info.fd, io_buffer_info.size) != true) {
            VXLOGE("setting buffer fails");
            status = VX_FAILURE;
            goto EXIT;
        }
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperSaitFrEa::ExynosVpuTaskIfWrapperSaitFrEa(ExynosVpuKernelSaitFrEa *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperSaitFrEa::preProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy input data from vx object */
    vx_image input;
    input = (vx_image)parameters[0];

    vx_imagepatch_addressing_t addr;
    vx_uint8 *input_ptr;
    input_ptr = NULL;
    vx_rectangle_t rect;
    memset(&rect, 0x0, sizeof(rect));

    vx_uint32 width, height;
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    rect.end_x = width;
    rect.end_y = height;
    status = vxAccessImagePatch(input, &rect, 0, &addr, (void**)&input_ptr, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing input fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    vx_uint8 *vpu_input_ptr;
    io_buffer_info = &m_in_io_param_info[0].custom_param.io_buffer_info[0];
    vpu_input_ptr = (vx_uint8*)io_buffer_info->addr;

    memcpy(vpu_input_ptr, input_ptr, io_buffer_info->size);

    status = vxCommitImagePatch(input, &rect, 0, &addr, input_ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image fails");
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapperSaitFrEa::postProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

#if (SAVE_INTERMEDIATE_BUFFER==1)
    List<struct io_buffer_info_t> *inter_buf_list;
    List<struct io_buffer_info_t>::iterator buf_iter;
#endif

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy input data from vx object */
    vx_image output0, output1;
    output0 = (vx_image)parameters[1];
    output1 = (vx_image)parameters[2];

    vx_imagepatch_addressing_t addr0, addr1;
    vx_uint8 *output0_ptr, *output1_ptr;
    output0_ptr = NULL;
    output1_ptr = NULL;
    vx_rectangle_t rect0;
    memset(&rect0, 0x0, sizeof(rect0));
    vx_rectangle_t rect1;
    memset(&rect1, 0x0, sizeof(rect1));

    vx_uint32 width, height;

    status |= vxQueryImage(output0, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(output0, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }
    rect0.end_x = width;
    rect0.end_y = height;

    status |= vxQueryImage(output1, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(output1, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }
    rect1.end_x = width;
    rect1.end_y = height;

    status |= vxAccessImagePatch(output0, &rect0, 0, &addr0, (void**)&output0_ptr, VX_WRITE_ONLY);
    status |= vxAccessImagePatch(output1, &rect1, 0, &addr1, (void**)&output1_ptr, VX_WRITE_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing output fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    vx_uint16 *vpu_output_ptr;

    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    vpu_output_ptr = (vx_uint16*)io_buffer_info->addr;
    memcpy(output0_ptr, vpu_output_ptr, rect0.end_x * rect0.end_y);

    io_buffer_info = &m_out_io_param_info[1].custom_param.io_buffer_info[0];
    vpu_output_ptr = (vx_uint16*)io_buffer_info->addr;
    memcpy(output1_ptr, vpu_output_ptr, rect1.end_x * rect1.end_y);

    status |= vxCommitImagePatch(output0, &rect0, 0, &addr0, output0_ptr);
    status |= vxCommitImagePatch(output1, &rect1, 0, &addr1, output1_ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image fails");
        goto EXIT;
    }

#if (SAVE_INTERMEDIATE_BUFFER==1)
    //JUN_DBG, write intermediate buffer
    vx_char file_name_ext5[64];
    vx_char file_name_ext10[64];
    vx_char file_name_ext15[64];

    sprintf(file_name_ext5, "%s", "./cnn_debug/ea_external_5.bin");
    sprintf(file_name_ext10, "%s", "./cnn_debug/ea_external_10.bin");
    sprintf(file_name_ext15, "%s", "./cnn_debug/ea_external_15.bin");

    __u32 *external_mem;
    struct vpul_task *task_info;
    task_info = m_task_if->getTask()->getTaskInfo();
    external_mem = &(((struct vpul_task*)task_info)->external_mem_addr[0]);

    inter_buf_list = &saitfr_ea->m_inter_task_buffer_list;

    for (buf_iter=inter_buf_list->begin(); buf_iter!=inter_buf_list->end(); buf_iter++) {
        if ((*buf_iter).fd == external_mem[5]) {
            VXLOGE("write %s", file_name_ext5);
            fileWriteToBuffer(file_name_ext5, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[10]) {
            VXLOGE("write %s", file_name_ext10);
            fileWriteToBuffer(file_name_ext10, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[15]) {
            VXLOGE("write %s", file_name_ext15);
            fileWriteToBuffer(file_name_ext15, (*buf_iter).addr, (*buf_iter).size);
        }
  #if 1
        vx_char *buf_ptr;
        buf_ptr = (*buf_iter).addr;
        VXLOGE("[0x%x]%X %X %X %X %X %X %X %X", (*buf_iter).fd, buf_ptr[0], buf_ptr[1], buf_ptr[2], buf_ptr[3], buf_ptr[4], buf_ptr[5], buf_ptr[6], buf_ptr[7]);
  #endif
    }
#endif

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSaitFrLa::inputValidator(vx_node node, vx_uint32 index)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if (index == 0) {
        vx_image input = 0;
        vx_parameter param = vxGetParameterByIndex(node, index);

        vxQueryParameter(param, VX_PARAMETER_ATTRIBUTE_REF, &input, sizeof(input));
        if (input) {
            vx_df_image format = 0;
            vxQueryImage(input, VX_IMAGE_ATTRIBUTE_FORMAT, &format, sizeof(format));
            if (format == VX_DF_IMAGE_U8) {
                status = VX_SUCCESS;
            }
            vxReleaseImage(&input);
        }
        vxReleaseParameter(&param);
    }

    if (status != VX_SUCCESS) {
        VXLOGE("input validation fails, index:%d", index);
    }

    return status;
}

vx_status
ExynosVpuKernelSaitFrLa::outputValidator(vx_node node, vx_uint32 index, vx_meta_format meta)
{
    vx_status status = VX_ERROR_INVALID_PARAMETERS;

    if ((index == 1) || (index == 2) || (index == 3)) {
        vx_df_image meta_format = VX_DF_IMAGE_U8;
        vx_uint32 width = 640;
        vx_uint32 height = 1;
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_FORMAT, &meta_format, sizeof(meta_format));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
        vxSetMetaFormatAttribute(meta, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
        status = VX_SUCCESS;
    }

    if (status != VX_SUCCESS) {
        VXLOGE("output validation fails, index:%d", index);
    }

    return status;
}

ExynosVpuKernelSaitFrLa::ExynosVpuKernelSaitFrLa(vx_char *name, vx_uint32 param_num)
                                       : ExynosVpuKernelCnn(name, param_num)
{
    strcpy(m_task_name, "saitfr");

#if (SAVE_INTERMEDIATE_BUFFER==1)
    saitfr_la = this;
#endif
}

ExynosVpuKernelSaitFrLa::~ExynosVpuKernelSaitFrLa(void)
{

}

vx_status
ExynosVpuKernelSaitFrLa::setupBaseVxInfo(const vx_reference parameters[])
{
    vx_status status = VX_FAILURE;

    vx_image input = (vx_image)parameters[0];

    status = vxGetValidAncestorRegionImage(input, &m_valid_rect);
    if (status != VX_SUCCESS) {
        VXLOGE("getting valid region fails, err:%d", status);
    }

    return status;
}

vx_status
ExynosVpuKernelSaitFrLa::initTask(vx_node node, const vx_reference *parameters)
{
    vx_status status;

    status = initTaskFromBinary();
    if (status != VX_SUCCESS) {
        VXLOGE("init task from binary fails, %p, %p", node, parameters);
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSaitFrLa::initTaskFromBinary(void)
{
    vx_status status = VX_SUCCESS;
    ExynosVpuTaskIf *task_if_0;

    task_if_0 = initTask0FromBinary();
    if (task_if_0 == NULL) {
        VXLOGE("task0 isn't created");
        status = VX_FAILURE;
        goto EXIT;
    }

EXIT:
    return status;
}

ExynosVpuTaskIf*
ExynosVpuKernelSaitFrLa::initTask0FromBinary(void)
{
    vx_status status = VX_SUCCESS;
    int ret = NO_ERROR;

    ExynosVpuTaskIfWrapperSaitFrLa *task_wr = new ExynosVpuTaskIfWrapperSaitFrLa(this, 0);
    status = setTaskIfWrapper(0, task_wr);
    if (status != VX_SUCCESS) {
        VXLOGE("adding taskif wrapper fails");
        return NULL;
    }

#if 1
    /* JUN_TBD, update buffer size for max pooling */
    struct vpul_memory_map_desc *memmap;
    memmap = &(((struct vpul_task*)td_binary_saitfr_la)->memmap_desc[0]);
    memmap[10].image_sizes.width *= 4;
    memmap[20].image_sizes.width *= 4;
    memmap[30].image_sizes.width *= 4;

    for (vx_uint32 i=1; i<=2; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=3; i<=4; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=6; i<=7; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=8; i<=9; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=11; i<=12; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=13; i<=14; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=16; i<=17; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=18; i<=19; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=21; i<=22; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=23; i<=24; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=26; i<=27; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=28; i<=29; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=31; i<=32; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=33; i<=34; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=36; i<=37; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=38; i<=39; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=41; i<=42; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=43; i<=44; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=46; i<=47; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=48; i<=49; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=51; i<=52; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=53; i<=54; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }

    for (vx_uint32 i=56; i<=57; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 16384) >> 14;
        memmap[i].image_sizes.width = 16384;
    }
    for (vx_uint32 i=58; i<=59; i++) {
        memmap[i].image_sizes.height = (memmap[i].image_sizes.width + 8192) >> 13;
        memmap[i].image_sizes.width = 8192;
    }
#endif

    ExynosVpuTaskIf *task_if = task_wr->getTaskIf();
    task_if->setCnnTask();
    ret = task_if->importTaskStr((struct vpul_task*)td_binary_saitfr_la);
    if (ret != NO_ERROR) {
        VXLOGE("creating task descriptor fails, ret:%d", ret);
        status = VX_FAILURE;
    }

    /* connect memmap to io */
    for (vx_uint32 i=0; i<dimof(cnn_la::sait_fr_la_io); i++) {
        task_if->setIoMemmap(cnn_la::sait_fr_la_io[i].io_direction, cnn_la::sait_fr_la_io[i].io_index, (uint32_t)cnn_la::sait_fr_la_io[i].memmap_index);
    }

    if (status == VX_SUCCESS)
        return task_if;
    else
        return NULL;
}

vx_status
ExynosVpuKernelSaitFrLa::updateTaskParamFromVX(vx_node node, const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

     if (!node)
        VXLOGE("invalid node, %p, %p", node, parameters);

EXIT:
    return status;
}

vx_status
ExynosVpuKernelSaitFrLa::initVxIo(const vx_reference *parameters)
{
    vx_status status = VX_SUCCESS;

    ExynosVpuTaskIfWrapper *task_wr_0 = m_task_wr_list[0];

    struct io_format_t io_format;
    struct io_memory_t io_memory;
    io_buffer_info_t io_buffer_info;

    memset(&io_format, 0x0, sizeof(io_format));
    memset(&io_memory, 0x0, sizeof(io_memory));
    memset(&io_buffer_info, 0x0, sizeof(io_buffer_info));

    io_memory.type = VS4L_BUFFER_LIST;
    io_memory.memory = VS4L_MEMORY_DMABUF;
    io_memory.count = 1;

    struct vpul_memory_map_desc *memmap;
    struct vpul_task *task_info = task_wr_0->getTaskIf()->getTask()->getTaskInfo();
    memmap = &(((struct vpul_task*)task_info)->memmap_desc[0]);

    vx_uint32 memmap_index;
    vx_uint32 buf_size;

    for (vx_uint32 i=0; i<dimof(cnn_la::sait_fr_la_io); i++) {
        memmap_index = cnn_la::sait_fr_la_io[i].memmap_index;

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = memmap[memmap_index].image_sizes.width;
        io_format.height = memmap[memmap_index].image_sizes.height;
        io_format.pixel_byte = memmap[memmap_index].image_sizes.pixel_bytes;
        buf_size = io_format.width * io_format.height * io_format.pixel_byte;

        allocAndFillBuf(&cnn_la::sait_fr_la_io[i], buf_size, &io_buffer_info);
        status = task_wr_0->setIoCustomParam(cnn_la::sait_fr_la_io[i].io_direction, cnn_la::sait_fr_la_io[i].io_index, &io_format, &io_memory, &io_buffer_info);
        if (status != VX_SUCCESS) {
            VXLOGE("assigning param fails, %p", parameters);
            goto EXIT;
        }
    }

    for (vx_uint32 i=0; i<dimof(cnn_la::sait_fr_la_inter); i++) {
        memmap_index = cnn_la::sait_fr_la_inter[i].memmap_index;

        io_format.format = VS4L_DF_IMAGE_U8;
        io_format.width = memmap[memmap_index].image_sizes.width;
        io_format.height = memmap[memmap_index].image_sizes.height;
        io_format.pixel_byte = memmap[memmap_index].image_sizes.pixel_bytes;
        buf_size = io_format.width * io_format.height * io_format.pixel_byte;

        allocAndFillBuf(&cnn_la::sait_fr_la_inter[i], buf_size, &io_buffer_info);

        ExynosVpuMemmap *memmap = task_wr_0->getTaskIf()->getTask()->getMemmap(memmap_index);

        if (memmap->getMemory()->setBuffer(io_buffer_info.fd, io_buffer_info.size) != true) {
            VXLOGE("setting buffer fails");
            status = VX_FAILURE;
            goto EXIT;
        }
    }

EXIT:
    return status;
}

ExynosVpuTaskIfWrapperSaitFrLa::ExynosVpuTaskIfWrapperSaitFrLa(ExynosVpuKernelSaitFrLa *kernel, vx_uint32 task_index)
                                            :ExynosVpuTaskIfWrapper(kernel, task_index)
{

}

vx_status
ExynosVpuTaskIfWrapperSaitFrLa::preProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

    status = ExynosVpuTaskIfWrapper::preProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy input data from vx object */
    vx_image input;
    input = (vx_image)parameters[0];

    vx_imagepatch_addressing_t addr;
    vx_uint8 *input_ptr;
    input_ptr = NULL;
    vx_rectangle_t rect;
    memset(&rect, 0x0, sizeof(rect));

    vx_uint32 width, height;
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(input, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    rect.end_x = width;
    rect.end_y = height;
    status = vxAccessImagePatch(input, &rect, 0, &addr, (void**)&input_ptr, VX_READ_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing input fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    vx_uint8 *vpu_input_ptr;
    io_buffer_info = &m_in_io_param_info[0].custom_param.io_buffer_info[0];
    vpu_input_ptr = (vx_uint8*)io_buffer_info->addr;

    memcpy(vpu_input_ptr, input_ptr, io_buffer_info->size);

    status = vxCommitImagePatch(input, &rect, 0, &addr, input_ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image fails");
        goto EXIT;
    }

EXIT:
    return status;
}

vx_status
ExynosVpuTaskIfWrapperSaitFrLa::postProcessTask(const vx_reference parameters[])
{
    vx_status status = VX_SUCCESS;

#if (SAVE_INTERMEDIATE_BUFFER==1)
    List<struct io_buffer_info_t> *inter_buf_list;
    List<struct io_buffer_info_t>::iterator buf_iter;
#endif

    status = ExynosVpuTaskIfWrapper::postProcessTask(parameters);
    if (status != VX_SUCCESS) {
        VXLOGE("common post processing task fails");
        goto EXIT;
    }

    /* convert&copy input data from vx object */
    vx_image output0, output1, output2;
    output0 = (vx_image)parameters[1];
    output1 = (vx_image)parameters[2];
    output2 = (vx_image)parameters[3];

    vx_imagepatch_addressing_t addr;
    vx_uint8 *output0_ptr, *output1_ptr, *output2_ptr;
    output0_ptr = NULL;
    output1_ptr = NULL;
    output2_ptr = NULL;
    vx_rectangle_t rect;
    memset(&rect, 0x0, sizeof(rect));

    vx_uint32 width, height;
    status |= vxQueryImage(output0, VX_IMAGE_ATTRIBUTE_WIDTH, &width, sizeof(width));
    status |= vxQueryImage(output0, VX_IMAGE_ATTRIBUTE_HEIGHT, &height, sizeof(height));
    if (status != VX_SUCCESS) {
        VXLOGE("querying image fails");
        goto EXIT;
    }

    rect.end_x = width;
    rect.end_y = height;
    status |= vxAccessImagePatch(output0, &rect, 0, &addr, (void**)&output0_ptr, VX_WRITE_ONLY);
    status |= vxAccessImagePatch(output1, &rect, 0, &addr, (void**)&output1_ptr, VX_WRITE_ONLY);
    status |= vxAccessImagePatch(output2, &rect, 0, &addr, (void**)&output2_ptr, VX_WRITE_ONLY);
    if (status != VX_SUCCESS) {
        VXLOGE("accessing output fails");
        goto EXIT;
    }

    const io_buffer_info_t *io_buffer_info;
    vx_uint16 *vpu_output_ptr;

    io_buffer_info = &m_out_io_param_info[0].custom_param.io_buffer_info[0];
    vpu_output_ptr = (vx_uint16*)io_buffer_info->addr;
    memcpy(output0_ptr, vpu_output_ptr, io_buffer_info->size);

    io_buffer_info = &m_out_io_param_info[1].custom_param.io_buffer_info[0];
    vpu_output_ptr = (vx_uint16*)io_buffer_info->addr;
    memcpy(output1_ptr, vpu_output_ptr, io_buffer_info->size);

    io_buffer_info = &m_out_io_param_info[2].custom_param.io_buffer_info[0];
    vpu_output_ptr = (vx_uint16*)io_buffer_info->addr;
    memcpy(output2_ptr, vpu_output_ptr, io_buffer_info->size);

    status |= vxCommitImagePatch(output0, &rect, 0, &addr, output0_ptr);
    status |= vxCommitImagePatch(output1, &rect, 0, &addr, output1_ptr);
    status |= vxCommitImagePatch(output2, &rect, 0, &addr, output2_ptr);
    if (status != VX_SUCCESS) {
        VXLOGE("commiting image fails");
        goto EXIT;
    }

#if (SAVE_INTERMEDIATE_BUFFER==1)
    //JUN_DBG, write intermediate buffer
    vx_char file_name_ext5[64];
    vx_char file_name_ext10[64];
    vx_char file_name_ext15[64];
    vx_char file_name_ext20[64];
    vx_char file_name_ext25[64];
    vx_char file_name_ext30[64];
    vx_char file_name_ext35[64];
    vx_char file_name_ext45[64];
    vx_char file_name_ext55[64];

    sprintf(file_name_ext5, "%s", "./cnn_debug/la_external_5.bin");
    sprintf(file_name_ext10, "%s", "./cnn_debug/la_external_10.bin");
    sprintf(file_name_ext15, "%s", "./cnn_debug/la_external_15.bin");
    sprintf(file_name_ext20, "%s", "./cnn_debug/la_external_20.bin");
    sprintf(file_name_ext25, "%s", "./cnn_debug/la_external_25.bin");
    sprintf(file_name_ext30, "%s", "./cnn_debug/la_external_30.bin");
    sprintf(file_name_ext35, "%s", "./cnn_debug/la_external_35.bin");
    sprintf(file_name_ext45, "%s", "./cnn_debug/la_external_45.bin");
    sprintf(file_name_ext55, "%s", "./cnn_debug/la_external_55.bin");

    __u32 *external_mem;
    struct vpul_task *task_info;
    task_info = m_task_if->getTask()->getTaskInfo();
    external_mem = &(((struct vpul_task*)task_info)->external_mem_addr[0]);

    inter_buf_list = &saitfr_la->m_inter_task_buffer_list;

    for (buf_iter=inter_buf_list->begin(); buf_iter!=inter_buf_list->end(); buf_iter++) {
        if ((*buf_iter).fd == external_mem[5]) {
            VXLOGE("write %s", file_name_ext5);
            fileWriteToBuffer(file_name_ext5, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[10]) {
            VXLOGE("write %s", file_name_ext10);
            fileWriteToBuffer(file_name_ext10, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[15]) {
            VXLOGE("write %s", file_name_ext15);
            fileWriteToBuffer(file_name_ext15, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[20]) {
            VXLOGE("write %s", file_name_ext20);
            fileWriteToBuffer(file_name_ext20, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[25]) {
            VXLOGE("write %s", file_name_ext25);
            fileWriteToBuffer(file_name_ext25, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[30]) {
            VXLOGE("write %s", file_name_ext30);
            fileWriteToBuffer(file_name_ext30, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[35]) {
            VXLOGE("write %s", file_name_ext35);
            fileWriteToBuffer(file_name_ext35, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[45]) {
            VXLOGE("write %s", file_name_ext45);
            fileWriteToBuffer(file_name_ext45, (*buf_iter).addr, (*buf_iter).size);
        } else if ((*buf_iter).fd == external_mem[55]) {
            VXLOGE("write %s", file_name_ext55);
            fileWriteToBuffer(file_name_ext55, (*buf_iter).addr, (*buf_iter).size);
        }
    }
#endif

EXIT:
    return status;
}

}; /* namespace android */

