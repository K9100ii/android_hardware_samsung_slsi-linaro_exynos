/*
 **
 ** Copyright 2015, Samsung Electronics Co. LTD
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
#define LOG_TAG "ExynosCameraSizeControl"
#include <cutils/log.h>

#include "ExynosCameraSizeControl.h"

namespace android {

void updateNodeGroupInfo(
        int pipeId,
        ExynosCamera1Parameters *params,
        const size_control_info_t sizeControlInfo,
        camera2_node_group *node_group_info)
{
    status_t ret = NO_ERROR;
    uint32_t perframePosition = 0;
#ifdef USE_VRA_GROUP
    int dsInputPortId = params->getDsInputPortId();
#endif
    bool isReprocessing = pipeId >= PIPE_FLITE_REPROCESSING ? true : false;
#if defined(SAMSUNG_HIFI_LLS) || defined(SR_CAPTURE)
    bool isLlsSrEnabled = false;
#endif
    ExynosRect sensorSize;
#ifdef SUPPORT_DEPTH_MAP
    ExynosRect depthMapSize;
#endif // SUPPORT_DEPTH_MAP
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    ExynosRect hwBdsSize;
    ExynosRect bdsSize;
    ExynosRect ispSize;
    ExynosRect mcscInputSize;
    ExynosRect ratioCropSize;
    ExynosRect mcscSize[6];
#ifdef USE_VRA_GROUP
    ExynosRect vraInputSize;
    ExynosRect dsInputSize;
#endif
    sensorSize = sizeControlInfo.sensorSize;
#ifdef SUPPORT_DEPTH_MAP
    depthMapSize = sizeControlInfo.depthMapSize;
#endif // SUPPORT_DEPTH_MAP
    bnsSize = sizeControlInfo.bnsSize;
    bayerCropSize = sizeControlInfo.bayerCropSize;
    bdsSize = sizeControlInfo.bdsSize;
#ifdef USE_VRA_GROUP
    vraInputSize = sizeControlInfo.vraInputSize;
#endif
    if (bdsSize.w > bayerCropSize.w || bdsSize.h > bayerCropSize.h) {
        CLOGW2("Bcrop size(%dx%d) < BDS size(%dx%d). Fix it.",
                bayerCropSize.w, bayerCropSize.h, bdsSize.w, bdsSize.h);
        if (isReprocessing == true && params->getUsePureBayerReprocessing() == false) {
            bayerCropSize.x = 0;
            bayerCropSize.y = 0;
            bayerCropSize.w = bdsSize.w;
            bayerCropSize.h = bdsSize.h;
        } else {
            params->getPreviewBdsSize(&hwBdsSize, false);
            bdsSize.w = bayerCropSize.w;
            bdsSize.h = bayerCropSize.h;
            if (hwBdsSize.w < bdsSize.w || hwBdsSize.h < bdsSize.h) {
                CLOGW2("HWBDS size(%dx%d) < BDS size(%dx%d). Fix it.",
                        hwBdsSize.w, hwBdsSize.h, bdsSize.w, bdsSize.h);
                if (hwBdsSize.w < bdsSize.w) {
                    bdsSize.w = hwBdsSize.w;
                }

                if (hwBdsSize.h < bdsSize.h) {
                    bdsSize.h = hwBdsSize.h;
                }

            }
        }
    }

    if (isReprocessing == false) {
        if (params->isUseIspInputCrop() == true)
            ispSize = sizeControlInfo.yuvCropSize;
        else
            ispSize = sizeControlInfo.bdsSize;

        if (params->isUseMcscInputCrop() == true)
            mcscInputSize = sizeControlInfo.yuvCropSize;
        else
            mcscInputSize = ispSize;

        mcscSize[0] = sizeControlInfo.hwPreviewSize;

        if (params->getHighResolutionCallbackMode() == true) {
            mcscSize[1] = sizeControlInfo.hwPreviewSize;
        } else {
            mcscSize[1] = sizeControlInfo.previewSize;
        }

        if (params->getCameraId() == CAMERA_ID_FRONT && params->getDualMode() == true)
            mcscSize[2] = sizeControlInfo.hwPreviewSize;
        else
            mcscSize[2] = sizeControlInfo.hwVideoSize;

#ifdef SAMSUNG_DUAL_SOLUTION
        if (params->getDualCameraMode() == true) {
            int needMargin = params->getPreviewNeedMargin();
            if (needMargin == 0)
                mcscSize[0] = sizeControlInfo.fusionDstSize;
        }
#endif

#ifdef USE_VRA_GROUP
        dsInputSize = mcscSize[dsInputPortId];

        if (dsInputSize.w < vraInputSize.w || dsInputSize.h < vraInputSize.h) {
            vraInputSize.w = dsInputSize.w;
            vraInputSize.h = dsInputSize.h;
        }
        mcscSize[3] = vraInputSize;
#endif

    } else {
        if (params->isUseReprocessingIspInputCrop() == true)
            ispSize = sizeControlInfo.yuvCropSize;
        else
            ispSize = sizeControlInfo.bdsSize;

        if (params->isUseReprocessingMcscInputCrop() == true)
            mcscInputSize = sizeControlInfo.yuvCropSize;
        else
            mcscInputSize = ispSize;

        if (params->isSingleChain() == true) {
            mcscSize[1] = sizeControlInfo.pictureSize;
            mcscSize[2] = sizeControlInfo.thumbnailSize;

            if (params->getOutPutFormatNV21Enable())
                mcscSize[0] = sizeControlInfo.pictureSize;
            else
                mcscSize[0] = sizeControlInfo.previewSize;
        } else {
#ifdef EXYNOS7885
            mcscSize[1] = sizeControlInfo.pictureSize;
            mcscSize[2] = sizeControlInfo.thumbnailSize;
#else
            mcscSize[3] = sizeControlInfo.pictureSize;
            mcscSize[4] = sizeControlInfo.thumbnailSize;
#endif
            if (params->getOutPutFormatNV21Enable()
#ifdef USE_DUAL_CAMERA
                    || (params->getDualCameraMode() == true &&
                        params->getDualCameraReprocessingSyncType() == SYNC_TYPE_SYNC)
#endif
               ) {
#ifdef EXYNOS7885
                mcscSize[0] = sizeControlInfo.pictureSize;
#else
                mcscSize[1] = sizeControlInfo.pictureSize;
#endif
            } else {
#ifdef EXYNOS7885
                mcscSize[0] = sizeControlInfo.previewSize;
#else
                mcscSize[1] = sizeControlInfo.previewSize;
#endif
            }
        }
    }

    /* Set Leader node perframe size */
    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_FLITE_REPROCESSING:
        /* Leader FLITE : sensor crop size */
        setLeaderSizeToNodeGroupInfo(node_group_info, sensorSize.x, sensorSize.y, sensorSize.w, sensorSize.h);
        break;
    case PIPE_3AA:
    case PIPE_3AA_REPROCESSING:
        /* Leader 3AX : [crop] : Bcrop */
        setLeaderSizeToNodeGroupInfo(node_group_info, bayerCropSize.x, bayerCropSize.y, bayerCropSize.w, bayerCropSize.h);
        break;
    case PIPE_ISP:
    case PIPE_ISP_REPROCESSING:
        /* Leader ISPX : [X] : ISP input crop size */
        setLeaderSizeToNodeGroupInfo(node_group_info, ispSize.x, ispSize.y, ispSize.w, ispSize.h);
        break;
    case PIPE_TPU:
    case PIPE_TPU_REPROCESSING:
        /* Leader TPU : [crop] : 3AP output Size */
        setLeaderSizeToNodeGroupInfo(node_group_info, 0, 0, ispSize.w, ispSize.h);
        break;
    case PIPE_MCSC:
    case PIPE_MCSC_REPROCESSING:
        /* Leader MCSCS : [crop] : YUV Crop Size */
        setLeaderSizeToNodeGroupInfo(node_group_info, mcscInputSize.x, mcscInputSize.y, mcscInputSize.w, mcscInputSize.h);
        break;
    case PIPE_TPU1:
        setLeaderSizeToNodeGroupInfo(node_group_info, 0, 0, mcscSize[3].x, mcscSize[3].y);
        break;
#ifdef USE_VRA_GROUP
    case PIPE_VRA:
        /* Leader VRA Size */
        setLeaderSizeToNodeGroupInfo(node_group_info, 0, 0, vraInputSize.w, vraInputSize.h);
        break;
#endif
    default:
        CLOGE2("Invalid pipeId %d", pipeId);
        return;
    }

#ifdef DEBUG_PERFRAME_SIZE
    char *pipeName;
    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_FLITE_REPROCESSING:
        pipeName = "FLITE";
        break;
    case PIPE_3AA:
    case PIPE_3AA_REPROCESSING:
        pipeName = "3AA";
        break;
    case PIPE_ISP:
    case PIPE_ISP_REPROCESSING:
        pipeName = "ISP";
        break;
    case PIPE_TPU:
    case PIPE_TPU_REPROCESSING:
        pipeName = "TPU";
        break;
    case PIPE_MCSC:
    case PIPE_MCSC_REPROCESSING:
        pipeName = "MCSC";
        break;
	case PIPE_VRA:
		pipeName = "VRA";
		break;
    default:
        break;
    }

    CLOGD2("%s %s", pipeName, isReprocessing ? "Reprocessing" : "Preview");
    CLOGD2("%s Leader (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
            pipeName,
            node_group_info->leader.input.cropRegion[0],
            node_group_info->leader.input.cropRegion[1],
            node_group_info->leader.input.cropRegion[2],
            node_group_info->leader.input.cropRegion[3],
            node_group_info->leader.output.cropRegion[0],
            node_group_info->leader.output.cropRegion[1],
            node_group_info->leader.output.cropRegion[2],
            node_group_info->leader.output.cropRegion[3]);
#endif

    /* Set capture node perframe size */
    for (int i = 0; i < CAPTURE_NODE_MAX; i++) {
        switch (node_group_info->capture[i].vid + FIMC_IS_VIDEO_BAS_NUM) {
        case FIMC_IS_VIDEO_SS0VC0_NUM:
        //case FIMC_IS_VIDEO_SS0VC1_NUM: // depth
        case FIMC_IS_VIDEO_SS0VC2_NUM:
        case FIMC_IS_VIDEO_SS0VC3_NUM:
        case FIMC_IS_VIDEO_SS1VC0_NUM:
        //case FIMC_IS_VIDEO_SS1VC1_NUM: // depth
        case FIMC_IS_VIDEO_SS1VC2_NUM:
        case FIMC_IS_VIDEO_SS1VC3_NUM:
        case FIMC_IS_VIDEO_SS2VC0_NUM:
        case FIMC_IS_VIDEO_SS2VC1_NUM:
        case FIMC_IS_VIDEO_SS2VC2_NUM:
        case FIMC_IS_VIDEO_SS2VC3_NUM:
        case FIMC_IS_VIDEO_SS3VC0_NUM:
        case FIMC_IS_VIDEO_SS3VC1_NUM:
        case FIMC_IS_VIDEO_SS3VC2_NUM:
        case FIMC_IS_VIDEO_SS3VC3_NUM:
#ifdef SAMSUNG_QUICK_SWITCH
        case FIMC_IS_VIDEO_SS4VC0_NUM:
        case FIMC_IS_VIDEO_SS4VC1_NUM:
        case FIMC_IS_VIDEO_SS4VC2_NUM:
        case FIMC_IS_VIDEO_SS4VC3_NUM:
        case FIMC_IS_VIDEO_SS5VC0_NUM:
        case FIMC_IS_VIDEO_SS5VC1_NUM:
        case FIMC_IS_VIDEO_SS5VC2_NUM:
        case FIMC_IS_VIDEO_SS5VC3_NUM:
#endif
            /* SENSOR : [X] : SENSOR output size for zsl bayer */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, sensorSize.w, sensorSize.h);
            perframePosition++;
            break;
#ifdef SUPPORT_DEPTH_MAP
        case FIMC_IS_VIDEO_SS0VC1_NUM:
        case FIMC_IS_VIDEO_SS1VC1_NUM:
            /* DEPTH_MAP : [X] : depth map size for */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, depthMapSize.w, depthMapSize.h);
            perframePosition++;
            break;
#endif // SUPPORT_DEPTH_MAP
        case FIMC_IS_VIDEO_30C_NUM:
        case FIMC_IS_VIDEO_31C_NUM:
            /* 3AC : [X] : 3AX input size without offset */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, bayerCropSize.w, bayerCropSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_30P_NUM:
        case FIMC_IS_VIDEO_31P_NUM:
            /* 3AP : [down-scale] : BDS */
            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition, 0, 0,
                                                    bayerCropSize.w, bayerCropSize.h, bdsSize.w, bdsSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_I0C_NUM:
        case FIMC_IS_VIDEO_I1C_NUM:
            /* ISPC : [X] : 3AP output size */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, ispSize.w, ispSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_I0P_NUM:
        case FIMC_IS_VIDEO_I1P_NUM:
            /* ISPP : [X] : 3AP output size */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, ispSize.w, ispSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_M0P_NUM:
            /* MCSC 0 : [crop/scale] : Preview */
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[0].w, mcscSize[0].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):getCropRectAlign failed. MCSC in_crop %dx%d, MCSC0 out_size %dx%d",
                        __FUNCTION__, __LINE__, mcscInputSize.w, mcscInputSize.h, mcscSize[0].w, mcscSize[0].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    mcscSize[0].w, mcscSize[0].h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_M1P_NUM:
            /* MCSC 1 : [crop/scale] : Preview Callback */
#ifdef USE_MSC_CAPTURE
            if (params->getZoomRatio() == 1.0f)
#endif
            {
                ret = getCropRectAlign(
                        mcscInputSize.w, mcscInputSize.h, mcscSize[1].w, mcscSize[1].h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            }
#ifdef USE_MSC_CAPTURE
            else {
                ret = getCropRectAlign(
                        mcscInputSize.w, mcscInputSize.h, mcscInputSize.w, mcscInputSize.h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        16, 16, 0, 1.0);
                mcscSize[1].w = ratioCropSize.w;
                mcscSize[1].h = ratioCropSize.h;
            }
#endif
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
            params->getDualCropRect(&ratioCropSize);
#endif
            if (ret != NO_ERROR) {
                CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC1(4) out_size %dx%d",
                        mcscInputSize.w, mcscInputSize.h, mcscSize[1].w, mcscSize[1].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }

#ifdef SAMSUNG_HIFI_LLS
            if (params->getLDCaptureMode())
                isLlsSrEnabled = true;
#endif
#ifdef SR_CAPTURE
            if (params->getSROn())
                isLlsSrEnabled = true;
#endif
#if defined(SAMSUNG_HIFI_LLS) || defined(SR_CAPTURE)
            if (isLlsSrEnabled &&
                 (params->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_ALWAYS_ON ||
                  params->getReprocessingBayerMode() == REPROCESSING_BAYER_MODE_DIRTY_DYNAMIC)
            ) {
                mcscSize[1].w = ALIGN_UP(ratioCropSize.w, 16);
                mcscSize[1].h = ALIGN_UP(ratioCropSize.h, 16);
                CLOGD2("[LLS_MBR][SR] mcscSize[1].w: %d, mcscSize[1].h: %d", mcscSize[1].w, mcscSize[1].h);
            }
#endif

#if defined(USE_MCSC1_FOR_PREVIEWCALLBACK) && defined(SAMSUNG_HYPER_MOTION)
            if (params->getHyperMotionMode() == true) {
                params->getHyperMotionCropSize(mcscInputSize.w, mcscInputSize.h,
                                                mcscSize[0].w, mcscSize[0].h,
                                                &ratioCropSize.x, &ratioCropSize.y,
                                                &ratioCropSize.w, &ratioCropSize.h);
            }
#endif

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    mcscSize[1].w, mcscSize[1].h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_M2P_NUM:
            /* MCSC 2 : [crop/scale] : Recording */
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[2].w, mcscSize[2].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            if (ret != NO_ERROR) {
                CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC2 out_size %dx%d",
                        mcscInputSize.w, mcscInputSize.h, mcscSize[2].w, mcscSize[2].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    mcscSize[2].w, mcscSize[2].h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_M3P_NUM:
#ifdef USE_VRA_GROUP
            ret = getCropRectAlign(
                    dsInputSize.w, dsInputSize.h, mcscSize[3].w, mcscSize[3].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
            params->getDualCropRect(&ratioCropSize);
#endif
            if (ret != NO_ERROR) {
                CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC3 out_size %dx%d",
                        mcscInputSize.w, mcscInputSize.h, mcscSize[3].w, mcscSize[3].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = dsInputSize.w;
                ratioCropSize.h = dsInputSize.h;
            }
#else
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[3].w, mcscSize[3].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
#ifdef SAMSUNG_DUAL_CAPTURE_SOLUTION
            params->getDualCropRect(&ratioCropSize);
#endif
            if (ret != NO_ERROR) {
                CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC3 out_size %dx%d",
                        mcscInputSize.w, mcscInputSize.h, mcscSize[3].w, mcscSize[3].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }
#endif

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    mcscSize[3].w, mcscSize[3].h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_M4P_NUM:
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[4].w, mcscSize[4].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            if (ret != NO_ERROR) {
                CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC4 out_size %dx%d",
                        mcscInputSize.w, mcscInputSize.h, mcscSize[4].w, mcscSize[4].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    mcscSize[4].w, mcscSize[4].h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_D1C_NUM:
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, mcscSize[3].w, mcscSize[3].h);
            break;
        default:
            break;
        }

#ifdef DEBUG_PERFRAME_SIZE
        CLOGD2("%s Capture(%d) (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
                pipeName, node_group_info->capture[i].vid,
                node_group_info->capture[perframePosition - 1].input.cropRegion[0],
                node_group_info->capture[perframePosition - 1].input.cropRegion[1],
                node_group_info->capture[perframePosition - 1].input.cropRegion[2],
                node_group_info->capture[perframePosition - 1].input.cropRegion[3],
                node_group_info->capture[perframePosition - 1].output.cropRegion[0],
                node_group_info->capture[perframePosition - 1].output.cropRegion[1],
                node_group_info->capture[perframePosition - 1].output.cropRegion[2],
                node_group_info->capture[perframePosition - 1].output.cropRegion[3]);
#endif
    }
}

void setLeaderSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        int cropX, int cropY,
        int width, int height)
{
    node_group_info->leader.input.cropRegion[0] = cropX;
    node_group_info->leader.input.cropRegion[1] = cropY;
    node_group_info->leader.input.cropRegion[2] = width;
    node_group_info->leader.input.cropRegion[3] = height;

    node_group_info->leader.output.cropRegion[0] = 0;
    node_group_info->leader.output.cropRegion[1] = 0;
    node_group_info->leader.output.cropRegion[2] = width;
    node_group_info->leader.output.cropRegion[3] = height;
}

void setCaptureSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        uint32_t perframePosition,
        int width, int height)
{
    node_group_info->capture[perframePosition].input.cropRegion[0] = 0;
    node_group_info->capture[perframePosition].input.cropRegion[1] = 0;
    node_group_info->capture[perframePosition].input.cropRegion[2] = width;
    node_group_info->capture[perframePosition].input.cropRegion[3] = height;

    node_group_info->capture[perframePosition].output.cropRegion[0] = 0;
    node_group_info->capture[perframePosition].output.cropRegion[1] = 0;
    node_group_info->capture[perframePosition].output.cropRegion[2] = width;
    node_group_info->capture[perframePosition].output.cropRegion[3] = height;
}

void setCaptureCropNScaleSizeToNodeGroupInfo(
        camera2_node_group *node_group_info,
        uint32_t perframePosition,
        int cropX, int cropY,
        int cropWidth, int cropHeight,
        int targetWidth, int targetHeight)
{
    node_group_info->capture[perframePosition].input.cropRegion[0] = cropX;
    node_group_info->capture[perframePosition].input.cropRegion[1] = cropY;
    node_group_info->capture[perframePosition].input.cropRegion[2] = cropWidth;
    node_group_info->capture[perframePosition].input.cropRegion[3] = cropHeight;

    node_group_info->capture[perframePosition].output.cropRegion[0] = 0;
    node_group_info->capture[perframePosition].output.cropRegion[1] = 0;
    node_group_info->capture[perframePosition].output.cropRegion[2] = targetWidth;
    node_group_info->capture[perframePosition].output.cropRegion[3] = targetHeight;
}

bool useSizeControlApi(void)
{
    bool use = false;
#ifdef USE_SIZE_CONTROL_API
    use = USE_SIZE_CONTROL_API;
#else
    use = false;
    CLOGW2("Use Legacy Utils Module API");
#endif
    return use;
}

}; /* namespace android */
