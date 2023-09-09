/*
 **
 ** Copyright 2017, Samsung Electronics Co. LTD
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
#include <log/log.h>

#include "ExynosRect.h"
#include "ExynosCameraCommonInclude.h"
#include "ExynosCameraSizeControl.h"
#include "ExynosCameraFrame.h"
#include "ExynosCameraUtils.h"

namespace android {

/*
   Check whether the current minimum YUV size is smaller than 1/16th
   of srcRect(MCSC input). Due to the M2MScaler limitation, 1/16 is
   the maximum downscaling with color conversion.
   In this case(If this function returns true), 1/2 downscaling
   will be happend in MCSC output
*/
#ifdef CHECK_NEED_16_DOWN_SCALING
static bool checkNeed16Downscaling(
        const ExynosRect* srcRect,
        const ExynosCameraConfigurations *configurations,
        const ExynosCameraParameters *params
) {
    int minYuvW, minYuvH;

    if(srcRect == NULL || params == NULL) {
        return false;
    }

    if (configurations->getSize(CONFIGURATION_MIN_YUV_SIZE, (uint32_t *)&minYuvW, (uint32_t *)&minYuvH) != NO_ERROR) {
        return false;
    }

    if ((srcRect->w > minYuvW * M2M_SCALER_MAX_DOWNSCALE_RATIO)
        || (srcRect->h > minYuvH * M2M_SCALER_MAX_DOWNSCALE_RATIO) ) {
        CLOGI2("Minimum output YUV size is too small (Src:[%dx%d] Out:[%dx%d])."
            ,  srcRect->w, srcRect->h, minYuvW, minYuvH);

        if( (srcRect->w > minYuvW * M2M_SCALER_MAX_DOWNSCALE_RATIO * MCSC_DOWN_RATIO_SMALL_YUV)
            || (srcRect->h > minYuvH * M2M_SCALER_MAX_DOWNSCALE_RATIO * MCSC_DOWN_RATIO_SMALL_YUV) ) {
            /* Print warning if MCSC downscaling still not able to meet the requirements */
            CLOGW2("Minimum output YUV size is still too small (Src:[%dx%d] Out:[%dx%d])."
                  "1/2 MSCS Downscaling will not solve the problem"
                ,  srcRect->w, srcRect->h, minYuvW, minYuvH);
        }
        return true;
    } else {
        return false;
    }
}
#endif

void updateNodeGroupInfo(
        int pipeId,
        ExynosCameraFrameSP_dptr_t frame,
        camera2_node_group *node_group_info)
{
    status_t ret = NO_ERROR;
    ExynosCameraConfigurations *configurations = frame->getConfigurations();
    ExynosCameraParameters *params = frame->getParameters();
    enum FRAME_FACTORY_TYPE factoryType = frame->getFactoryType();
    uint32_t perframePosition = 0;
    bool isReprocessing = pipeId >= PIPE_FLITE_REPROCESSING ? true : false;
    int dsInputPortId = params->getDsInputPortId(isReprocessing);
    static const int kMcscSizeMax = 6;

    ExynosRect sensorSize;
#ifdef SUPPORT_DEPTH_MAP
    ExynosRect depthMapSize;
#endif // SUPPORT_DEPTH_MAP
#ifdef SUPPORT_ME
    ExynosRect meSize;
#endif
    ExynosRect bnsSize;
    ExynosRect bayerCropSize;
    ExynosRect bdsSize;
    ExynosRect ispSize;
    ExynosRect mcscInputSize;
    ExynosRect vraInputSize;
    ExynosRect ratioCropSize;
    ExynosRect dsInputSize;
    ExynosRect mcscSize[kMcscSizeMax];

    memset(mcscSize, 0x0, sizeof(mcscSize));

    if (isReprocessing == false) {
        params->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&sensorSize.w, (uint32_t *)&sensorSize.h);
#ifdef SUPPORT_DEPTH_MAP
        params->getDepthMapSize(&depthMapSize.w, &depthMapSize.h);
#endif // SUPPORT_DEPTH_MAP
#ifdef SUPPORT_ME
        params->getMeSize(&meSize.w, &meSize.h);
#endif
        params->getPreviewBayerCropSize(&bnsSize, &bayerCropSize);
        params->getPreviewBdsSize(&bdsSize);

        if (params->isUseIspInputCrop() == true)
            params->getPreviewYuvCropSize(&ispSize);
        else
            ispSize = bdsSize;

        if (params->isUseMcscInputCrop() == true)
            params->getPreviewYuvCropSize(&mcscInputSize);
        else
            mcscInputSize = ispSize;

        if (params->getCameraId() == CAMERA_ID_FRONT
            && configurations->getMode(CONFIGURATION_PIP_MODE) == true) {
            params->getYuvVendorSize(&mcscSize[2].w, &mcscSize[2].h, 0, ispSize);
            params->getYuvVendorSize(&mcscSize[3].w, &mcscSize[3].h, 1, ispSize);
        } else {
            for (int i = ExynosCameraParameters::YUV_0; i < ExynosCameraParameters::YUV_MAX; i++) {
                params->getYuvVendorSize(&mcscSize[i].w, &mcscSize[i].h, i, ispSize);

#ifdef SAMSUNG_DUAL_ZOOM_PREVIEW
                if (configurations->getScenario() == SCENARIO_DUAL_REAR_ZOOM
                    && params->isPreviewPortId(i) == true
                    && configurations->getDynamicMode(DYNAMIC_DUAL_FORCE_SWITCHING) == false) {
                    ExynosRect fusionSrcRect;
                    ExynosRect fusionDstRect;
                    int margin = params->getActiveZoomMargin();

                    if (margin == DUAL_SOLUTION_MARGIN_VALUE_30 ||
                        margin == DUAL_SOLUTION_MARGIN_VALUE_20) {
                        params->getFusionSize(mcscSize[i].w, mcscSize[i].h,
                                &fusionSrcRect, &fusionDstRect,
                                margin);
                        mcscSize[i].w = fusionSrcRect.w;
                        mcscSize[i].h = fusionSrcRect.h;
                    }
                }
#endif
            }
        }

        if (dsInputPortId > MCSC_PORT_4 || dsInputPortId < MCSC_PORT_0) {
            dsInputSize = mcscInputSize;
        } else {
            dsInputSize = mcscSize[dsInputPortId];
        }

        if (dsInputPortId != MCSC_PORT_NONE) {
            params->getHwVraInputSize(&mcscSize[5].w, &mcscSize[5].h, dsInputPortId);
        }
    } else {
        params->getSize(HW_INFO_HW_SENSOR_SIZE, (uint32_t *)&sensorSize.w, (uint32_t *)&sensorSize.h);

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

        for (int i = ExynosCameraParameters::YUV_STALL_0;
             i < ExynosCameraParameters::YUV_STALL_MAX; i++) {
            int yuvIndex = i % ExynosCameraParameters::YUV_MAX;

            params->getYuvVendorSize(&mcscSize[yuvIndex].w, &mcscSize[yuvIndex].h, i, ispSize);
        }

        configurations->getSize(CONFIGURATION_PICTURE_SIZE, (uint32_t *)&mcscSize[3].w, (uint32_t *)&mcscSize[3].h);
        configurations->getSize(CONFIGURATION_THUMBNAIL_SIZE, (uint32_t *)&mcscSize[4].w, (uint32_t *)&mcscSize[4].h);

        if (dsInputPortId > MCSC_PORT_4) {
            dsInputSize = mcscSize[dsInputPortId % MCSC_PORT_MAX];
        } else {
            dsInputSize = mcscInputSize;
        }

        if (dsInputPortId != MCSC_PORT_NONE) {
            params->getHwVraInputSize(&mcscSize[5].w, &mcscSize[5].h, dsInputPortId);
        }
    }

    /* Make sure that the size being scaled is not larger than the input size */
    if (checkAvailableDownScaleSize(&sensorSize, &bayerCropSize) == false) {
        CLOGW2("change Dst(%s) size to Src(%s) size",
                MAKE_STRING(bayerCropSize), MAKE_STRING(sensorSize));
    }
    if (checkAvailableDownScaleSize(&bayerCropSize, &bdsSize) == false) {
        CLOGW2("change Dst(%s) size to Src(%s) size",
                MAKE_STRING(bdsSize), MAKE_STRING(bayerCropSize));
    }
    if (isReprocessing == false) {
        if (checkAvailableDownScaleSize(&bdsSize, &ispSize) == false) {
            CLOGW2("change Dst(%s) size to Src(%s) size",
                    MAKE_STRING(ispSize), MAKE_STRING(bdsSize));
        }
    } else {
        if (params->getUsePureBayerReprocessing() == true) {
            if (checkAvailableDownScaleSize(&bdsSize, &ispSize) == false) {
                CLOGW2("change Dst(%s) size to Src(%s) size",
                        MAKE_STRING(ispSize), MAKE_STRING(bdsSize));
            }
        } else {
            /* for dirty bayer reprocessing */
            if (checkAvailableDownScaleSize(&bayerCropSize, &ispSize) == false) {
                CLOGW2("change Dst(%s) size to Src(%s) size",
                        MAKE_STRING(ispSize), MAKE_STRING(bayerCropSize));
            }
        }
    }
    if (checkAvailableDownScaleSize(&ispSize, &mcscInputSize) == false) {
        CLOGW2("change Dst(%s) size to Src(%s) size",
                MAKE_STRING(mcscInputSize), MAKE_STRING(ispSize));
    }

    /* Set Leader node perframe size */
    switch (pipeId) {
    case PIPE_FLITE:
    case PIPE_FLITE_REPROCESSING:
        /* Leader FLITE : sensor crop size */
        setLeaderSizeToNodeGroupInfo(node_group_info, sensorSize.x, sensorSize.y, sensorSize.w, sensorSize.h);
        break;
    case PIPE_3AA:
        /* Leader 3AX : [crop] : Bcrop */
        setLeaderSizeToNodeGroupInfo(node_group_info, bayerCropSize.x, bayerCropSize.y, bayerCropSize.w, bayerCropSize.h);
        break;
    case PIPE_3AA_REPROCESSING:
        /* Leader 3AX Reprocessing : sensor crop size */
        setLeaderSizeToNodeGroupInfo(node_group_info, sensorSize.x, sensorSize.y, sensorSize.w, sensorSize.h);
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
    case PIPE_VRA:
    case PIPE_VRA_REPROCESSING:
        {
            enum HW_CONNECTION_MODE e3aaVraMode = HW_CONNECTION_MODE_NONE;
            enum HW_CONNECTION_MODE eFlite3aaMode = HW_CONNECTION_MODE_NONE;
            if (!isReprocessing) {
                e3aaVraMode = params->getHwConnectionMode(PIPE_3AA, PIPE_VRA);
                eFlite3aaMode = params->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
            } else {
                e3aaVraMode = params->getHwConnectionMode(PIPE_3AA_REPROCESSING, PIPE_VRA_REPROCESSING);
                eFlite3aaMode = params->getHwConnectionMode(PIPE_FLITE_REPROCESSING, PIPE_3AA_REPROCESSING);
            }

            /* Leader VRA Size */
            if (e3aaVraMode != HW_CONNECTION_MODE_NONE) {
                if (eFlite3aaMode == HW_CONNECTION_MODE_OTF)
                    params->getHw3aaVraInputSize(bayerCropSize.w, bayerCropSize.h, &vraInputSize);
                else
                    params->getHw3aaVraInputSize(sensorSize.w, sensorSize.h, &vraInputSize);

                setLeaderSizeToNodeGroupInfo(node_group_info, 0, 0, vraInputSize.w, vraInputSize.h);
            } else {
                setLeaderSizeToNodeGroupInfo(node_group_info, 0, 0, mcscSize[5].w, mcscSize[5].h);
            }
            break;
        }
    default:
        CLOGE2("Invalid pipeId %d", pipeId);
        return;
    }

    CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame.get(), nullptr, frame->getRequestKey(),
            "[P%d] NODE_LEADER/ (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
            pipeId,
            node_group_info->leader.input.cropRegion[0],
            node_group_info->leader.input.cropRegion[1],
            node_group_info->leader.input.cropRegion[2],
            node_group_info->leader.input.cropRegion[3],
            node_group_info->leader.output.cropRegion[0],
            node_group_info->leader.output.cropRegion[1],
            node_group_info->leader.output.cropRegion[2],
            node_group_info->leader.output.cropRegion[3]);

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
            /* 3AC : [X] : 3AX input size without offset */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, bayerCropSize.w, bayerCropSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_31C_NUM:
            /* 3AC Reprocessing : [X] : 3AX Reprocessing input size */
#ifdef USE_3AA_CROP_AFTER_BDS
            if (isReprocessing == true)
                setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, sensorSize.w, sensorSize.h);
            else
#endif
                setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, bayerCropSize.w, bayerCropSize.h);

            perframePosition++;
            break;
        case FIMC_IS_VIDEO_30P_NUM:
        case FIMC_IS_VIDEO_31P_NUM:
            {
                ExynosRect inputSize;
                ExynosRect outputSize;
                enum HW_CONNECTION_MODE mode = HW_CONNECTION_MODE_NONE;

#ifdef USE_3AA_CROP_AFTER_BDS
                /* find 3AA input connection mode */
                if (isReprocessing == true) {
                    if (params->getUsePureBayerReprocessing() == true)
                        mode = HW_CONNECTION_MODE_M2M;
                    else
                        mode = HW_CONNECTION_MODE_NONE;
                } else {
                    mode = params->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
                }
#endif

                switch (mode) {
#ifdef USE_3AA_CROP_AFTER_BDS
                /* M2M Case */
                case HW_CONNECTION_MODE_M2M:
                    /* use Output BCrop */
                    inputSize.x = 0;
                    inputSize.y = 0;
                    inputSize.w = sensorSize.w;
                    inputSize.h = sensorSize.h;
                    outputSize.x = bayerCropSize.x;
                    outputSize.y = bayerCropSize.y;
                    outputSize.w = bayerCropSize.w;
                    outputSize.h = bayerCropSize.h;
                    break;
#endif
                /* Other Case */
                default:
                    inputSize.x = 0;
                    inputSize.y = 0;
                    inputSize.w = bayerCropSize.w;
                    inputSize.h = bayerCropSize.h;
                    outputSize.x = 0;
                    outputSize.y = 0;
                    outputSize.w = bdsSize.w;
                    outputSize.h = bdsSize.h;
                    break;
                }
                /* 3AP : [down-scale] : BDS */
                setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                        inputSize.x, inputSize.y, inputSize.w, inputSize.h,
                        outputSize.x, outputSize.y, outputSize.w, outputSize.h);
                perframePosition++;
            }
            break;
        case FIMC_IS_VIDEO_30G_NUM:
        case FIMC_IS_VIDEO_31G_NUM:
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, sensorSize.w, sensorSize.h);
            perframePosition++;
            break;
        case FIMC_IS_VIDEO_30F_NUM:
        case FIMC_IS_VIDEO_31F_NUM:
            {
                enum HW_CONNECTION_MODE eFlite3aaMode = HW_CONNECTION_MODE_NONE;
                ExynosRect dsInRect;
                if (!isReprocessing) {
                    eFlite3aaMode = params->getHwConnectionMode(PIPE_FLITE, PIPE_3AA);
                } else {
                    eFlite3aaMode = params->getHwConnectionMode(PIPE_FLITE_REPROCESSING, PIPE_3AA_REPROCESSING);
                }
                dsInRect.x = 0;
                dsInRect.y = 0;
                if (eFlite3aaMode == HW_CONNECTION_MODE_OTF) {
                    /* BCrop output to DS Input */
                    dsInRect.w = bayerCropSize.w;
                    dsInRect.h = bayerCropSize.h;
                    params->getHw3aaVraInputSize(bayerCropSize.w, bayerCropSize.h, &vraInputSize);
                } else {
                    dsInRect.w = sensorSize.w;
                    dsInRect.h = sensorSize.h;
                    params->getHw3aaVraInputSize(sensorSize.w, sensorSize.h, &vraInputSize);
                }
                ret = getCropRectAlign(
                        dsInRect.w, dsInRect.h, vraInputSize.w, vraInputSize.h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_3AA_DS_ALIGN_W, 1, 1.0f);
                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. 3AA_DS in_crop %dx%d, DS out_size %dx%d",
                             dsInRect.w, dsInRect.h, vraInputSize.w, vraInputSize.h);
                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = dsInRect.w;
                    ratioCropSize.h = dsInRect.h;
                }
                if (isReprocessing == false) {
                    params->getVendorRatioCropSizeForVRA(&ratioCropSize);
                }

                setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                        ratioCropSize.x, ratioCropSize.y,
                                                        ratioCropSize.w, ratioCropSize.h,
                                                        0, 0,
                                                        vraInputSize.w, vraInputSize.h);
                perframePosition++;
            }
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
#ifdef SUPPORT_ME
        case FIMC_IS_VIDEO_ME0C_NUM:
            /* ME0C : [X] : fixed size */
            setCaptureSizeToNodeGroupInfo(node_group_info, perframePosition, meSize.w, meSize.h);
            perframePosition++;
            break;
#endif
        case FIMC_IS_VIDEO_M0P_NUM:     /* MCSC 0 / MCSC 1 / MCSC 2 */
        case FIMC_IS_VIDEO_M4P_NUM:     /* MCSC 4 : [crop/scale] : Picture Thumbnail /HWFC */
        {
            int portIndex = node_group_info->capture[i].vid + FIMC_IS_VIDEO_BAS_NUM - FIMC_IS_VIDEO_M0P_NUM;

            if (mcscSize[portIndex].w == 0 || mcscSize[portIndex].h == 0) {
                CLOGV2("MCSC width or height values is 0, (%dx%d)",
                        mcscSize[portIndex].w, mcscSize[portIndex].h);
                ratioCropSize = mcscSize[portIndex];
            } else {
                ret = getCropRectAlign(
                        mcscInputSize.w, mcscInputSize.h, mcscSize[portIndex].w, mcscSize[portIndex].h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_MCSC_ALIGN, 2, 1.0f);
                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC%d out_size %dx%d",
                             mcscInputSize.w, mcscInputSize.h, portIndex, mcscSize[portIndex].w, mcscSize[portIndex].h);

                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = mcscInputSize.w;
                    ratioCropSize.h = mcscInputSize.h;
                }

                params->getVendorRatioCropSize(&ratioCropSize,
                                               &mcscSize[portIndex],
                                               portIndex,
                                               isReprocessing);
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    0, 0,
                                                    mcscSize[portIndex].w, mcscSize[portIndex].h);
            perframePosition++;
        }
            break;
        case FIMC_IS_VIDEO_M1P_NUM:     /* [crop/scale] : Variable Function : Preview, Preview Callback, Recording, YUV Stall */
        {
            int portIndex = node_group_info->capture[i].vid + FIMC_IS_VIDEO_BAS_NUM - FIMC_IS_VIDEO_M0P_NUM;

            /* depending on using jpeg reprocessing factory */
            if (factoryType == FRAME_FACTORY_TYPE_JPEG_REPROCESSING &&
                            params->getNumOfMcscOutputPorts() <= 3)
                    portIndex = 3;

            if (mcscSize[portIndex].w == 0 || mcscSize[portIndex].h == 0) {
                CLOGV2("MCSC width or height values is 0, (%dx%d)",
                        mcscSize[portIndex].w, mcscSize[portIndex].h);
                ratioCropSize = mcscSize[portIndex];
            } else {
                ret = getCropRectAlign(
                        mcscInputSize.w, mcscInputSize.h, mcscSize[portIndex].w, mcscSize[portIndex].h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_MCSC_ALIGN, 2, 1.0f);
                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC%d out_size %dx%d",
                             mcscInputSize.w, mcscInputSize.h, portIndex, mcscSize[portIndex].w, mcscSize[portIndex].h);

                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = mcscInputSize.w;
                    ratioCropSize.h = mcscInputSize.h;
                }

                params->getVendorRatioCropSize(&ratioCropSize,
                                               &mcscSize[portIndex],
                                               portIndex,
                                               isReprocessing);
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    0, 0,
                                                    mcscSize[portIndex].w, mcscSize[portIndex].h);
            perframePosition++;
        }
            break;
        case FIMC_IS_VIDEO_M2P_NUM:
        {
            int portIndex = node_group_info->capture[i].vid + FIMC_IS_VIDEO_BAS_NUM - FIMC_IS_VIDEO_M0P_NUM;

            /* depending on using jpeg reprocessing factory */
            if (factoryType == FRAME_FACTORY_TYPE_JPEG_REPROCESSING &&
                            params->getNumOfMcscOutputPorts() <= 3)
                    portIndex = 4;

            if (mcscSize[portIndex].w == 0 || mcscSize[portIndex].h == 0) {
                CLOGV2("MCSC width or height values is 0, (%dx%d)",
                        mcscSize[portIndex].w, mcscSize[portIndex].h);
                ratioCropSize = mcscSize[portIndex];
            } else {
                ret = getCropRectAlign(
                        mcscInputSize.w, mcscInputSize.h, mcscSize[portIndex].w, mcscSize[portIndex].h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_MCSC_ALIGN, 2, 1.0f);
                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC%d out_size %dx%d",
                             mcscInputSize.w, mcscInputSize.h, portIndex, mcscSize[portIndex].w, mcscSize[portIndex].h);

                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = mcscInputSize.w;
                    ratioCropSize.h = mcscInputSize.h;
                }

                params->getVendorRatioCropSize(&ratioCropSize
                                               , &mcscSize[portIndex]
                                               , portIndex
                                               , isReprocessing
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
                                               , &sensorSize
                                               , &mcscInputSize
#endif
                                               );
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
#if defined(SAMSUNG_HIFI_VIDEO) && defined(HIFIVIDEO_ZOOM_SUPPORTED)
                                                    mcscSize[portIndex].x, mcscSize[portIndex].y,
#else
                                                    0, 0,
#endif
                                                    mcscSize[portIndex].w, mcscSize[portIndex].h);
            perframePosition++;
        }
            break;
        case FIMC_IS_VIDEO_M3P_NUM:     /* MCSC 3 : [crop/scale] : Picture Main /HWFC*/
        {
            int portIndex = node_group_info->capture[i].vid + FIMC_IS_VIDEO_BAS_NUM - FIMC_IS_VIDEO_M0P_NUM;

            if (params->getNumOfMcscOutputPorts() <= 3)
                    portIndex = 5;

            if (mcscSize[portIndex].w == 0 || mcscSize[portIndex].h == 0) {
                CLOGV2("MCSC width or height values is 0, (%dx%d)",
                        mcscSize[portIndex].w, mcscSize[portIndex].h);
                ratioCropSize = mcscSize[portIndex];
            } else {
                if (params->getNumOfMcscOutputPorts() <= 3) {
                    ret = getCropRectAlign(
                            dsInputSize.w, dsInputSize.h, mcscSize[portIndex].w, mcscSize[portIndex].h,
                            &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                            CAMERA_MCSC_ALIGN, 2, 1.0f);
                } else {
                    ret = getCropRectAlign(
                            mcscInputSize.w, mcscInputSize.h, mcscSize[portIndex].w, mcscSize[portIndex].h,
                            &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                            CAMERA_MCSC_ALIGN, 2, 1.0f);
                }

                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. MCSC in_crop %dx%d, MCSC%d out_size %dx%d",
                             mcscInputSize.w, mcscInputSize.h, portIndex, mcscSize[portIndex].w, mcscSize[portIndex].h);

                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = mcscInputSize.w;
                    ratioCropSize.h = mcscInputSize.h;
                }

                params->getVendorRatioCropSize(&ratioCropSize,
                                               &mcscSize[portIndex],
                                               portIndex,
                                               isReprocessing);
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    0, 0,
                                                    mcscSize[portIndex].w, mcscSize[portIndex].h);
            perframePosition++;
        }
            break;
        case FIMC_IS_VIDEO_M5P_NUM:
            /* MCSC DS : [crop/scale] : VRA input */
            if (mcscSize[5].w == 0 || mcscSize[5].h == 0) {
                CLOGV2("MCSC width or height values is 0, (%dx%d)",
                        mcscSize[5].w, mcscSize[5].h);
                ratioCropSize = mcscSize[5];
            } else {
                ret = getCropRectAlign(
                        dsInputSize.w, dsInputSize.h, mcscSize[5].w, mcscSize[5].h,
                        &ratioCropSize.x, &ratioCropSize.y, &ratioCropSize.w, &ratioCropSize.h,
                        CAMERA_MCSC_ALIGN, 2, 1.0f);
                if (ret != NO_ERROR) {
                    CLOGE2("getCropRectAlign failed. MCSC_DS in_crop %dx%d, MCSC5 out_size %dx%d",
                             dsInputSize.w, dsInputSize.h, mcscSize[5].w, mcscSize[5].h);

                    ratioCropSize.x = 0;
                    ratioCropSize.y = 0;
                    ratioCropSize.w = dsInputSize.w;
                    ratioCropSize.h = dsInputSize.h;
                }
            }

            if (isReprocessing == false) {
                params->getVendorRatioCropSizeForVRA(&ratioCropSize);
            }

            setCaptureCropNScaleSizeToNodeGroupInfo(node_group_info, perframePosition,
                                                    ratioCropSize.x, ratioCropSize.y,
                                                    ratioCropSize.w, ratioCropSize.h,
                                                    0, 0,
                                                    mcscSize[5].w, mcscSize[5].h);
            perframePosition++;
            break;
        default:
            break;
        }

        if (perframePosition > 0) {
            CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame.get(), nullptr, frame->getRequestKey(),
                    "[P%d] NODE_CAPTURE(%d)/ (x:%d, y:%d) %d x %d -> (x:%d, y:%d) %d x %d",
                    pipeId, node_group_info->capture[i].vid,
                    node_group_info->capture[perframePosition - 1].input.cropRegion[0],
                    node_group_info->capture[perframePosition - 1].input.cropRegion[1],
                    node_group_info->capture[perframePosition - 1].input.cropRegion[2],
                    node_group_info->capture[perframePosition - 1].input.cropRegion[3],
                    node_group_info->capture[perframePosition - 1].output.cropRegion[0],
                    node_group_info->capture[perframePosition - 1].output.cropRegion[1],
                    node_group_info->capture[perframePosition - 1].output.cropRegion[2],
                    node_group_info->capture[perframePosition - 1].output.cropRegion[3]);
        }
    }

    CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame.get(), nullptr, frame->getRequestKey(),
            "[P%d] BAYER SIZE/ S[%dx%d] BN[%dx%d] BC[%d,%d,%dx%d] BD[%dx%d]",
            pipeId,
            sensorSize.w, sensorSize.h,
            bnsSize.w, bnsSize.h,
            bayerCropSize.x, bayerCropSize.y,
            bayerCropSize.w, bayerCropSize.h,
            bdsSize.w, bdsSize.h);

    CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame.get(), nullptr, frame->getRequestKey(),
            "[P%d] YUV SIZE/ IS[%dx%d] MC-I[%d,%d,%dx%d] RC[%d,%d,%dx%d] V[%dx%d->%dx%d]",
            pipeId,
            ispSize.w, ispSize.h,
            mcscInputSize.x, mcscInputSize.y,
            mcscInputSize.w, mcscInputSize.h,
            ratioCropSize.x, ratioCropSize.y,
            ratioCropSize.w, ratioCropSize.h,
            dsInputSize.w, dsInputSize.h,
            vraInputSize.w, vraInputSize.h);

#ifdef USE_DEBUG_PROPERTY
    {
        String8 str;
        str.appendFormat("[P%d] YUV MCSC SIZE/ ", pipeId);
        for (int i = 0; i < kMcscSizeMax; i++)
            str.appendFormat("[%d][%dx%d] ", i, mcscSize[i].w, mcscSize[i].h);
        CLOG_PERFRAME(SIZE, frame->getCameraId(), "", frame.get(), nullptr, frame->getRequestKey(), "%s", str.c_str());
    }
#endif
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
        int inCropX, int inCropY,
        int inCropWidth, int inCropHeight,
        int outCropX, int outCropY,
        int outCropWidth, int outCropHeight)
{
    node_group_info->capture[perframePosition].input.cropRegion[0] = inCropX;
    node_group_info->capture[perframePosition].input.cropRegion[1] = inCropY;
    node_group_info->capture[perframePosition].input.cropRegion[2] = inCropWidth;
    node_group_info->capture[perframePosition].input.cropRegion[3] = inCropHeight;

    node_group_info->capture[perframePosition].output.cropRegion[0] = outCropX;
    node_group_info->capture[perframePosition].output.cropRegion[1] = outCropY;
    node_group_info->capture[perframePosition].output.cropRegion[2] = outCropWidth;
    node_group_info->capture[perframePosition].output.cropRegion[3] = outCropHeight;
}

bool checkAvailableDownScaleSize(ExynosRect *src, ExynosRect *dst)
{
        if (src->w < dst->w || src->h < dst->h) {
            CLOGW2("Src %dx%d is smaller than Dst %dx%d.",
                    src->w, src->h,
                    dst->w, dst->h);
            dst->w = src->w;
            dst->h = src->h;

            return false;
        }

        return true;
}

}; /* namespace android */
