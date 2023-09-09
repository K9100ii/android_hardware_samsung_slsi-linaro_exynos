/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraUtilsModule"
#include <cutils/log.h>

#include "ExynosCameraUtilsModule.h"

namespace android {

void updateNodeGroupInfoMain(
        int pipeId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH)
{
    /* HACK: Support to YUV crop */
#if defined(YUV_CROP_ON_ZOOM)
    int fullW = (bayerCropSize.x * 2) + bayerCropSize.w;
    int fullH = (bayerCropSize.y * 2) + bayerCropSize.h;
    int calculatedBdsW = bdsSize.w;
    int calculatedBdsH = bdsSize.h;
#else
    int calculatedBdsW = (bayerCropSize.w < bdsSize.w) ? bayerCropSize.w : bdsSize.w;
    int calculatedBdsH = (bayerCropSize.h < bdsSize.h) ? bayerCropSize.h : bdsSize.h;
#endif

    if (pipeId == PIPE_3AA) {
        /* Leader : 3AA : BCrop */
        /* HACK: Support to YUV crop */
#if defined(YUV_CROP_ON_ZOOM)
        node_group_info->leader.input.cropRegion[0] = 0;
        node_group_info->leader.input.cropRegion[1] = 0;
        node_group_info->leader.input.cropRegion[2] = fullW;
        node_group_info->leader.input.cropRegion[3] = fullH;
#else
        node_group_info->leader.input.cropRegion[0] = bayerCropSize.x;
        node_group_info->leader.input.cropRegion[1] = bayerCropSize.y;
        node_group_info->leader.input.cropRegion[2] = bayerCropSize.w;
        node_group_info->leader.input.cropRegion[3] = bayerCropSize.h;
#endif
        node_group_info->leader.output.cropRegion[0] = node_group_info->leader.input.cropRegion[0];
        node_group_info->leader.output.cropRegion[1] = node_group_info->leader.input.cropRegion[1];
        node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
        node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];

        /* Capture 0 : 3AC -[X] - output cropX, cropY should be Zero */
        node_group_info->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_3AC_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_3AC_POS].output.cropRegion[3] = node_group_info->leader.output.cropRegion[3];

        /* Capture 1 : 3AP - [BDS] */
        node_group_info->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_3AP_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[2] = calculatedBdsW;
        node_group_info->capture[PERFRAME_BACK_3AP_POS].output.cropRegion[3] = calculatedBdsH;
    } else {
        /* Leader : ISP, TPU, MCSC */
        node_group_info->leader.input.cropRegion[0] = 0;
        node_group_info->leader.input.cropRegion[1] = 0;
        node_group_info->leader.input.cropRegion[2] = calculatedBdsW;
        node_group_info->leader.input.cropRegion[3] = calculatedBdsH;
        node_group_info->leader.output.cropRegion[0] = 0;
        node_group_info->leader.output.cropRegion[1] = 0;
        node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
        node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];
    }

    /* HACK: Support to YUV crop */
#if defined(YUV_CROP_ON_ZOOM)
    if (pipeId == PIPE_MCSC) {
        node_group_info->leader.input.cropRegion[0] = int(bayerCropSize.x * calculatedBdsW / fullW);
        node_group_info->leader.input.cropRegion[1] = int(bayerCropSize.y * calculatedBdsH / fullH);
        node_group_info->leader.input.cropRegion[2] = int(bayerCropSize.w * calculatedBdsW / fullW);
        node_group_info->leader.input.cropRegion[3] = int(bayerCropSize.h * calculatedBdsH / fullH);
        node_group_info->leader.output.cropRegion[0] = 0;
        node_group_info->leader.output.cropRegion[1] = 0;
        node_group_info->leader.output.cropRegion[2] = node_group_info->leader.input.cropRegion[2];
        node_group_info->leader.output.cropRegion[3] = node_group_info->leader.input.cropRegion[3];
    }
#endif

    if (pipeId == PIPE_3AA || pipeId == PIPE_ISP) {
        /* Capture : ISPC */
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[2] = calculatedBdsW;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[3] = calculatedBdsH;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_ISPC_POS].input.cropRegion[3];

        /* Capture : ISPP */
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_ISPC_POS].output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].output.cropRegion[2] = node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_ISPP_POS].output.cropRegion[3] = node_group_info->capture[PERFRAME_BACK_ISPP_POS].input.cropRegion[3];
    }

    if (pipeId != PIPE_3AA) {
        /* Capture 0 : MCSC Preview - [scaling] */
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].output.cropRegion[2] = previewW;
        node_group_info->capture[PERFRAME_BACK_MCSC0_POS].output.cropRegion[3] = previewH;

        /* Capture 1 : MCSC Preview callback - [scaling] */
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].output.cropRegion[2] = previewW;
        node_group_info->capture[PERFRAME_BACK_MCSC1_POS].output.cropRegion[3] = previewH;

        /* Capture 2 : MCSC Recording - [scaling] */
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].input.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].input.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].input.cropRegion[2] = node_group_info->leader.output.cropRegion[2];
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].input.cropRegion[3] = node_group_info->leader.output.cropRegion[3];
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].output.cropRegion[0] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].output.cropRegion[1] = 0;
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].output.cropRegion[2] = recordingW;
        node_group_info->capture[PERFRAME_BACK_MCSC2_POS].output.cropRegion[3] = recordingH;
    }
}


void updateNodeGroupInfoReprocessing(
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        __unused ExynosRect bdsSize,
        int pictureW, int pictureH,
        int thumbnailW, int thumbnailH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf)
{
    camera2_node_group *node_group_info_isp = NULL;
    int mcscInW = 0, mcscInH = 0;

    if (pureBayerReprocessing == true && flag3aaIspOtf == false)
        node_group_info_isp = node_group_info_2;
    else
        node_group_info_isp = node_group_info_1;

    if (pureBayerReprocessing == true) {
        /* Leader : 3AA */
        node_group_info_1->leader.input.cropRegion[0] = bayerCropSizePicture.x;
        node_group_info_1->leader.input.cropRegion[1] = bayerCropSizePicture.y;
        node_group_info_1->leader.input.cropRegion[2] = bayerCropSizePicture.w;
        node_group_info_1->leader.input.cropRegion[3] = bayerCropSizePicture.h;
        node_group_info_1->leader.output.cropRegion[0] = node_group_info_1->leader.input.cropRegion[0];
        node_group_info_1->leader.output.cropRegion[1] = node_group_info_1->leader.input.cropRegion[1];
        node_group_info_1->leader.output.cropRegion[2] = node_group_info_1->leader.input.cropRegion[2];
        node_group_info_1->leader.output.cropRegion[3] = node_group_info_1->leader.input.cropRegion[3];

        /* Capture 1 : 3AP - [BDS] */
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[0] = 0;
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[1] = 0;
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[2] = node_group_info_1->leader.output.cropRegion[2];
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[3] = node_group_info_1->leader.output.cropRegion[3];
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[0] = 0;
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[1] = 0;
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[2] = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[2];
        node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[3] = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].input.cropRegion[3];

        mcscInW = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[2];
        mcscInH = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[3];
    }

    if (pureBayerReprocessing == false || flag3aaIspOtf == false) {
        /* Leader : ISP */
        node_group_info_isp->leader.input.cropRegion[0] = 0;
        node_group_info_isp->leader.input.cropRegion[1] = 0;

        if (pureBayerReprocessing == false) {
            node_group_info_isp->leader.input.cropRegion[2] = bayerCropSizePreview.w;
            node_group_info_isp->leader.input.cropRegion[3] = bayerCropSizePreview.h;
        } else {
            node_group_info_isp->leader.input.cropRegion[2] = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[2];
            node_group_info_isp->leader.input.cropRegion[3] = node_group_info_1->capture[PERFRAME_REPROCESSING_3AP_POS].output.cropRegion[3];
        }

        node_group_info_isp->leader.output.cropRegion[0] = 0;
        node_group_info_isp->leader.output.cropRegion[1] = 0;
        node_group_info_isp->leader.output.cropRegion[2] = node_group_info_isp->leader.input.cropRegion[2];
        node_group_info_isp->leader.output.cropRegion[3] = node_group_info_isp->leader.input.cropRegion[3];

        mcscInW = node_group_info_isp->leader.output.cropRegion[2];
        mcscInH = node_group_info_isp->leader.output.cropRegion[3];
    }

    /* Capture 1 : MCSC3 */
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].input.cropRegion[0] = (mcscInW - bayerCropSizePicture.w) / 2;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].input.cropRegion[1] = (mcscInH - bayerCropSizePicture.h) / 2;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].input.cropRegion[2] = bayerCropSizePicture.w;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].input.cropRegion[3] = bayerCropSizePicture.h;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].output.cropRegion[0] = 0;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].output.cropRegion[1] = 0;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].output.cropRegion[2] = pictureW;
    node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC3_POS].output.cropRegion[3] = pictureH;

    /* Capture 1 : MCSC4 */
    if (thumbnailW > 0 && thumbnailH > 0) {
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].input.cropRegion[0] = (mcscInW - bayerCropSizePicture.w) / 2;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].input.cropRegion[1] = (mcscInH - bayerCropSizePicture.h) / 2;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].input.cropRegion[2] = bayerCropSizePicture.w;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].input.cropRegion[3] = bayerCropSizePicture.h;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].output.cropRegion[0] = 0;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].output.cropRegion[1] = 0;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].output.cropRegion[2] = thumbnailW;
        node_group_info_isp->capture[PERFRAME_REPROCESSING_MCSC4_POS].output.cropRegion[3] = thumbnailH; 
    }
}

void ExynosCameraNodeGroup::updateNodeGroupInfo(
        __unused int cameraId,
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        int thumbnailW, int thumbnailH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf)
{
    updateNodeGroupInfoReprocessing(
            node_group_info_1,
            node_group_info_2,
            bayerCropSizePreview,
            bayerCropSizePicture,
            bdsSize,
            pictureW, pictureH,
            thumbnailW, thumbnailH,
            pureBayerReprocessing,
            flag3aaIspOtf);

    // m_dump("3AA", cameraId, node_group_info_3aa);
    // m_dump("ISP", cameraId, node_group_info_isp);
}

void ExynosCameraNodeGroup::updateNodeGroupInfo(
        __unused int cameraId,
        camera2_node_group *node_group_info_1,
        camera2_node_group *node_group_info_2,
        ExynosRect bayerCropSizePreview,
        ExynosRect bayerCropSizePicture,
        ExynosRect bdsSize,
        int pictureW, int pictureH,
        bool pureBayerReprocessing,
        bool flag3aaIspOtf)
{
    updateNodeGroupInfoReprocessing(
            node_group_info_1,
            node_group_info_2,
            bayerCropSizePreview,
            bayerCropSizePicture,
            bdsSize,
            pictureW, pictureH,
            0, 0,
            pureBayerReprocessing,
            flag3aaIspOtf);

    // m_dump("3AA", cameraId, node_group_info_3aa);
    // m_dump("ISP", cameraId, node_group_info_isp);
}

void ExynosCameraNodeGroup3AA::updateNodeGroupInfo(
        __unused int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH)
{
    updateNodeGroupInfoMain(
            PIPE_3AA,
            node_group_info,
            bayerCropSize,
            bdsSize,
            previewW, previewH,
            recordingW, recordingH);

    // m_dump("3AA", cameraId, node_group_info);
}

void ExynosCameraNodeGroupISP::updateNodeGroupInfo(
        __unused int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH,
        __unused bool tpuEnabled)
{
    updateNodeGroupInfoMain(
            PIPE_ISP,
            node_group_info,
            bayerCropSize,
            bdsSize,
            previewW, previewH,
            recordingW, recordingH);

    // m_dump("ISP", cameraId, node_group_info);
}

void ExynosCameraNodeGroupDIS::updateNodeGroupInfo(
        __unused int cameraId,
        camera2_node_group *node_group_info,
        ExynosRect bayerCropSize,
        ExynosRect bdsSize,
        int previewW, int previewH,
        int recordingW, int recordingH,
        bool tpuEnabled)
{
    int pipeId = -1;

    if (tpuEnabled == true)
        pipeId = PIPE_TPU;
    else
        pipeId = PIPE_MCSC;

    updateNodeGroupInfoMain(
            pipeId,
            node_group_info,
            bayerCropSize,
            bdsSize,
            previewW, previewH,
            recordingW, recordingH);

    // m_dump("TPU", cameraId, node_group_info);
    // m_dump("MCSC", cameraId, node_group_info);
}

void ExynosCameraNodeGroup::m_dump(const char *name, int cameraId, camera2_node_group *node_group_info)
{

    ALOGD("[CAM_ID(%d)][%s]-DEBUG(%s[%d]):node_group_info->leader(in : %d, %d, %d, %d) -> (out : %d, %d, %d, %d)(request : %d, vid : %d)",
        cameraId,
        name,
        __FUNCTION__, __LINE__,
        node_group_info->leader.input.cropRegion[0],
        node_group_info->leader.input.cropRegion[1],
        node_group_info->leader.input.cropRegion[2],
        node_group_info->leader.input.cropRegion[3],
        node_group_info->leader.output.cropRegion[0],
        node_group_info->leader.output.cropRegion[1],
        node_group_info->leader.output.cropRegion[2],
        node_group_info->leader.output.cropRegion[3],
        node_group_info->leader.request,
        node_group_info->leader.vid);

    for (int i = 0; i < CAPTURE_NODE_MAX; i++) {
        ALOGD("[CAM_ID(%d)][%s]-DEBUG(%s[%d]):node_group_info->capture[%d](in : %d, %d, %d, %d) -> (out : %d, %d, %d, %d)(request : %d, vid : %d)",
        cameraId,
        name,
        __FUNCTION__, __LINE__,
        i,
        node_group_info->capture[i].input.cropRegion[0],
        node_group_info->capture[i].input.cropRegion[1],
        node_group_info->capture[i].input.cropRegion[2],
        node_group_info->capture[i].input.cropRegion[3],
        node_group_info->capture[i].output.cropRegion[0],
        node_group_info->capture[i].output.cropRegion[1],
        node_group_info->capture[i].output.cropRegion[2],
        node_group_info->capture[i].output.cropRegion[3],
        node_group_info->capture[i].request,
        node_group_info->capture[i].vid);
    }
}

}; /* namespace android */
