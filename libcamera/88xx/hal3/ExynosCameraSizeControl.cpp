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

/*
   Check whether the current minimum YUV size is smaller than 1/16th
   of srcRect(MCSC input). Due to the M2MScaler limitation, 1/16 is
   the maximum downscaling with color conversion.
   In this case(If this function returns true), 1/2 downscaling
   will be happend in MCSC output
*/
static bool checkNeed16Downscaling(const ExynosRect* srcRect, const ExynosCamera3Parameters *params) {
    int minYuvW, minYuvH;

    if(srcRect == NULL || params == NULL) {
        return false;
    }

    if(params->getMinYuvSize(&minYuvW, &minYuvH) != NO_ERROR) {
        return false;
    }

    if( (srcRect->w > minYuvW * M2M_SCALER_MAX_DOWNSCALE_RATIO)
        || (srcRect->h > minYuvH * M2M_SCALER_MAX_DOWNSCALE_RATIO) ) {
        ALOGI("INFO(%s[%d]):Minimum output YUV size is too small (Src:[%dx%d] Out:[%dx%d])."
            , __FUNCTION__, __LINE__, srcRect->w, srcRect->h, minYuvW, minYuvH);

        if( (srcRect->w > minYuvW * M2M_SCALER_MAX_DOWNSCALE_RATIO * MCSC_DOWN_RATIO_SMALL_YUV)
            || (srcRect->h > minYuvH * M2M_SCALER_MAX_DOWNSCALE_RATIO * MCSC_DOWN_RATIO_SMALL_YUV) ) {
            /* Print warning if MCSC downscaling still not able to meet the requirements */
            ALOGW("WARN(%s[%d]):Minimum output YUV size is still too small (Src:[%dx%d] Out:[%dx%d])."
                  "1/2 MSCS Downscaling will not solve the problem"
                , __FUNCTION__, __LINE__, srcRect->w, srcRect->h, minYuvW, minYuvH);
        }
        return true;
    } else {
        return false;
    }
}

void updateNodeGroupInfo(
        int pipeId,
        ExynosCamera3Parameters *params,
        camera2_node_group *node_group_info)
{
    status_t ret = NO_ERROR;
    uint32_t perframePosition = 0;
    bool isReprocessing = pipeId >= PIPE_FLITE_REPROCESSING ? true : false;
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;
    ExynosRect ispSize;
    ExynosRect mcscInputSize;
    ExynosRect ratioCropSize;
    ExynosRect mcscSize[MAX_YUV_STREAM_COUNT];

    if (isReprocessing == false) {
        params->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
        params->getPreviewBdsSize(&bdsSize);

        if (params->isUseIspInputCrop() == true)
            params->getPreviewYuvCropSize(&ispSize);
        else
            params->getPreviewBdsSize(&ispSize);

        if (params->isUseMcscInputCrop() == true)
            params->getPreviewYuvCropSize(&mcscInputSize);
        else
            mcscInputSize = ispSize;

        for (int i = 0; i < params->getYuvStreamMaxNum(); i++)
            params->getYuvSize(&mcscSize[i].w, &mcscSize[i].h, i);
    } else {
        if (params->getUsePureBayerReprocessing() == true) {
            params->getPictureBayerCropSize(&bnsSize, &bayerCropSize);
            params->getPictureBdsSize(&bdsSize);
        } else { /* If dirty bayer is used for reprocessing, reprocessing ISP input size should be set preview bayer crop size */
            params->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
            params->getPreviewBdsSize(&bdsSize);
        }

        if (params->isUseReprocessingIspInputCrop() == true)
            params->getPictureYuvCropSize(&ispSize);
        else if (params->getUsePureBayerReprocessing() == true)
            params->getPictureBdsSize(&ispSize);
        else /* for dirty bayer reprocessing */
            ispSize = bayerCropSize;

        if (params->isUseReprocessingMcscInputCrop() == true)
            params->getPictureYuvCropSize(&mcscInputSize);
        else
            mcscInputSize = ispSize;

        if (params->isSingleChain() == true) {
            /*
                                  Single      Dual
             mcscSize[0] -> MCSC    P0         P3
             mcscSize[1] -> MCSC    P1         P4
             mcscSize[2] -> MCSC    P2
            */

            /* HACK: For YUV output stream of reprocessing(For private reprocessing),
                     Downscale 1/2 on MCSC if One of more YUV output requires
                     More than 1/16 downscaling(which can not be supported
                     by M2M Scaler)
            */
            if(checkNeed16Downscaling(&mcscInputSize, params) == true) {
                mcscSize[0].w = ALIGN_DOWN(mcscInputSize.w / MCSC_DOWN_RATIO_SMALL_YUV, CAMERA_MCSC_ALIGN);
                mcscSize[0].h = ALIGN_DOWN(mcscInputSize.h / MCSC_DOWN_RATIO_SMALL_YUV, CAMERA_MCSC_ALIGN);
                ALOGI("INFO(%s[%d]): 1/2 Downscaling will be done at REPROCESSING_MCSC2" , __FUNCTION__, __LINE__);
            } else {
                mcscSize[0] = mcscInputSize;
            }

            params->getPictureSize(&mcscSize[1].w, &mcscSize[1].h);
            params->getThumbnailSize(&mcscSize[2].w, &mcscSize[2].h);
        } else {
            params->getPictureSize(&mcscSize[0].w, &mcscSize[0].h);
            params->getThumbnailSize(&mcscSize[1].w, &mcscSize[1].h);
        }
    }

    /* Set Leader node perframe size */
    switch (pipeId) {
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
    default:
        ALOGE("ERR(%s[%d]):Invalid pipeId %d", __FUNCTION__, __LINE__, pipeId);
        return;
    }

#ifdef DEBUG_PERFRAME_SIZE
    char *pipeName;
    switch (pipeId) {
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
    default:
        break;
    }

    ALOGD("DEBUG(%s[%d]):%s %s", __FUNCTION__, __LINE__, pipeName, isReprocessing ? "Reprocessing" : "Preview");
    ALOGD("DEBUG(%s[%d]):%s Leader (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
            __FUNCTION__, __LINE__, pipeName,
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
        case FIMC_IS_VIDEO_M0P_NUM:
        case FIMC_IS_VIDEO_M3P_NUM:
            /* MCSC 0 : [crop/scale] : Preview */
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[0].w, mcscSize[0].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):getCropRectAlign failed. MCSC in_crop %dx%d, MCSC0(3) out_size %dx%d",
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
        case FIMC_IS_VIDEO_M4P_NUM:
            /* MCSC 1 : [crop/scale] : Preview Callback */
            ret = getCropRectAlign(
                    mcscInputSize.w, mcscInputSize.h, mcscSize[1].w, mcscSize[1].h,
                    &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                    CAMERA_MCSC_ALIGN, 2, 0, 1.0);
            if (ret != NO_ERROR) {
                ALOGE("ERR(%s[%d]):getCropRectAlign failed. MCSC in_crop %dx%d, MCSC1(4) out_size %dx%d",
                        __FUNCTION__, __LINE__, mcscInputSize.w, mcscInputSize.h, mcscSize[1].w, mcscSize[1].h);

                ratioCropSize.x = 0;
                ratioCropSize.y = 0;
                ratioCropSize.w = mcscInputSize.w;
                ratioCropSize.h = mcscInputSize.h;
            }

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
                ALOGE("ERR(%s[%d]):getCropRectAlign failed. MCSC in_crop %dx%d, MCSC2 out_size %dx%d",
                        __FUNCTION__, __LINE__, mcscInputSize.w, mcscInputSize.h, mcscSize[2].w, mcscSize[2].h);

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
        default:
            break;
        }

#ifdef DEBUG_PERFRAME_SIZE
        ALOGD("DEBUG(%s[%d]):%s Capture(%d) (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
                __FUNCTION__, __LINE__, pipeName, node_group_info->capture[i].vid,
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
    ALOGW("INFO(%s[%d]):Use Legacy Utils Module API", __FUNCTION__, __LINE__);
#endif
    return use;
}

}; /* namespace android */
